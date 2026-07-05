/* fw/src/app_modbus_tcp.c — see app_modbus_tcp.h.
 *
 * M6/M9 hardening (spec 2026-07-05): 수신은 누적 버퍼 + mb_tcp_frame_peek
 * 워커로 coalesced/파이프라인/경계-partial 프레임을 처리하고(구 "recv당 1
 * 완전 프레임" 가정 폐기), 소켓은 SF_IO_NONBLOCK — send/disconnect가
 * SOCK_BUSY 즉시 반환하므로 슈퍼루프 스톨(구 blocking 최대 ~1.6s) 제거.
 * 레거시 samd20 process_tcp.c도 blocking이었음 — 의도적 강화 편차(spec §2.2).
 * 응답은 poll당 코얼레스드 단일 send: 벤더 논블로킹 send는 직전 send의
 * SENDOK(피어 ACK) 미도래 시 SOCK_BUSY라 프레임별 연속 send는 2번째부터
 * 드롭됨(socket.c:531-550). 실패 응답은 드롭 — 마스터 재시도가 Modbus 표준
 * 회복 경로(재전송 큐 없음, spec §3). */
#include <string.h>
#include "app_modbus_tcp.h"
#include "app_modbus.h"            /* app_modbus_core(), app_modbus_apply_writes() */
#include "app_modbus_tcp_frame.h"
#include "socket.h"
#include "wizchip_conf.h"
#include "sys_tick.h"
#include "mon.h"

#define MB_TCP_SOCK   0u
#define MB_TCP_PORT   502u

/* 최대 wire 프레임 = MBAP(6)+length(<=125) = 131B. 누적 2프레임 = 경계
 * partial 이월까지 커버 (spec §2.2). */
#define MB_TCP_ACC_LEN           (2u * (MB_TCP_MBAP_LEN + MB_FRAME_MAX))
#define MB_TCP_FRAMES_PER_POLL   4u
#define MB_TCP_TXACC_LEN         (MB_TCP_FRAMES_PER_POLL * MB_TCP_RESP_MAX)
#define MB_TCP_CLOSEWAIT_MAX_MS  500u   /* graceful DISCON 벨트 (spec §3) */

static uint8_t  s_acc[MB_TCP_ACC_LEN];      /* 수신 누적 (partial 이월) */
static uint16_t s_acc_len;
static uint8_t  s_txacc[MB_TCP_TXACC_LEN];  /* poll당 응답 코얼레스 (스택 스파이크 회피) */
static uint32_t s_cw_since_ms;              /* CLOSE_WAIT 진입 스탬프 */
static uint8_t  s_cw_active;

/* M7 (eth re-apply) + M8 (comm_mode 이탈): sock0 강제 close + 수신/종료
 * 상태 리셋. close()는 CLOSED 소켓에 무해. 호출측 계약: W5500이 살아있던
 * 경로에서만 호출됨(app_eth_available 게이트) — 칩 부재 시 vendor close()의
 * 레지스터 폴링 스핀 위험은 도달 불가. */
void app_modbus_tcp_reset(void)
{
    (void)close(MB_TCP_SOCK);
    s_acc_len   = 0u;
    s_cw_active = 0u;
}

/* Port of samd20 process_tcp.c control_tcps: walk the socket FSM one step.
 * M9: 소켓을 SF_IO_NONBLOCK으로 열어 disconnect()가 DISCON 발행 후 즉시
 * SOCK_BUSY 반환(socket.c:491-513) — CLOSE_WAIT에서 슈퍼루프 무정지. DISCON
 * 후 LAST_ACK로 이탈하므로 재발행 없음; 500ms 내 미완료면 close() 벨트. */
static void control_tcp(void)
{
    switch (getSn_SR(MB_TCP_SOCK)) {
        case SOCK_ESTABLISHED:
            s_cw_active = 0u;
            if (getSn_IR(MB_TCP_SOCK) & Sn_IR_CON) {
                setSn_IR(MB_TCP_SOCK, Sn_IR_CON);
                s_acc_len = 0u;   /* 새 연결: 이전 피어의 stale partial 폐기 */
            }
            break;
        case SOCK_CLOSE_WAIT:
            if (s_cw_active == 0u) {
                s_cw_active   = 1u;
                s_cw_since_ms = sys_tick_get_ms();
                (void)disconnect(MB_TCP_SOCK);
            } else if ((uint32_t)(sys_tick_get_ms() - s_cw_since_ms)
                       > MB_TCP_CLOSEWAIT_MAX_MS) {
                (void)close(MB_TCP_SOCK);   /* graceful 포기 (abrupt, spec §3) */
                s_cw_active = 0u;
            }
            break;
        case SOCK_INIT:
            s_cw_active = 0u;
            (void)listen(MB_TCP_SOCK);
            break;
        case SOCK_CLOSED:
            s_cw_active = 0u;
            (void)socket(MB_TCP_SOCK, Sn_MR_TCP, MB_TCP_PORT, SF_IO_NONBLOCK);
            break;
        default:
            break;
    }
}

void app_modbus_tcp_poll(void)
{
    control_tcp();

    if (getSn_SR(MB_TCP_SOCK) != SOCK_ESTABLISHED) {
        return;
    }

    /* 수신 누적: 남은 공간만큼만. 최대 프레임(131) < ACC(262)라 완전 프레임
     * 없이 버퍼가 차는 경우는 DESYNC-급 garbage뿐 → 아래서 폐기됨. */
    uint16_t space = (uint16_t)(MB_TCP_ACC_LEN - s_acc_len);
    uint16_t avail = getSn_RX_RSR(MB_TCP_SOCK);
    if ((avail != 0u) && (space != 0u)) {
        if (avail > space) {
            avail = space;
        }
        int32_t got = recv(MB_TCP_SOCK, &s_acc[s_acc_len], avail);
        if (got > 0) {
            s_acc_len = (uint16_t)(s_acc_len + (uint16_t)got);
        }
    }
    if (s_acc_len == 0u) {
        return;
    }

    /* 워커: 완전 프레임을 순차 응답(TX 누적), poll당 상한으로 슈퍼루프 시간
     * 바운드. FC06 apply는 프레임별 순차 — 뒤 프레임의 decode가 앞 write의
     * 반영을 보는 기존 단일-프레임 순서와 동일 (respond-then-apply 계약은
     * "에코가 decode 시점 캡처"라 send 타이밍과 무관, 구 :78-82 주석). */
    uint16_t tx_len = 0u;
    uint16_t off = 0u;
    uint8_t  frames = 0u;
    while (frames < MB_TCP_FRAMES_PER_POLL) {
        uint16_t frame_len = 0u;
        mb_tcp_fr_t r = mb_tcp_frame_peek(&s_acc[off],
                                          (uint16_t)(s_acc_len - off),
                                          &frame_len);
        if (r == MB_TCP_FR_DESYNC) {
            mon_printf("[mbtcp] desync, drop %u\r\n",
                       (unsigned)(s_acc_len - off));
            s_acc_len = 0u;
            off = 0u;
            break;
        }
        if (r != MB_TCP_FR_OK) {
            break;                          /* NEED_MORE — 잔여는 이월 */
        }
        uint16_t out_len = 0u;
        uint8_t  fc = 0u;
        if (mb_tcp_build_response(app_modbus_core(), &s_acc[off], frame_len,
                                  &s_txacc[tx_len], &out_len, &fc)) {
            tx_len = (uint16_t)(tx_len + out_len);
            if (fc == 0x06u) {
                app_modbus_apply_writes();
            }
        }
        off = (uint16_t)(off + frame_len);
        frames++;
    }
    if (off != 0u) {
        s_acc_len = (uint16_t)(s_acc_len - off);
        if (s_acc_len != 0u) {
            memmove(s_acc, &s_acc[off], s_acc_len);   /* partial 선두 이동 */
        }
    }

    if (tx_len != 0u) {
        int32_t sent = send(MB_TCP_SOCK, s_txacc, tx_len);
        if (sent != (int32_t)tx_len) {
            /* SOCK_BUSY(직전 SENDOK 미도래)/SOCKERR_TIMEOUT(벤더가 close —
             * FSM이 재오픈)/기타 — 드롭, 마스터 재시도 (spec §3). */
            mon_printf("[mbtcp] send drop r=%ld len=%u\r\n",
                       (long)sent, (unsigned)tx_len);
        }
    }
}
