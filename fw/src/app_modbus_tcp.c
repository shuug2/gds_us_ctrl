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
/* W5500 auto keep-alive 주기 (5s 단위 → 2=10s). 케이블 분리/피어 소멸로
 * ESTABLISHED가 고착되면 단일-소켓 서버가 새 SYN에 RST(영구 락아웃) —
 * app_eth는 STATIC_UP에서 링크 재폴링을 안 하므로(선재 설계) 칩 KA가 유일한
 * 자가-치유: 유휴 10s 후 프로브 → 무응답 → Sn_IR TIMEOUT → SOCK_CLOSED →
 * FSM 재리슨 (HW E2E M9 케이블-복귀 락아웃으로 발견, 2026-07-05).
 * ⚠ W5500 KA는 데이터 1회 이상 교환 후부터 동작 — 연결만 하고 무송신인
 * 피어는 미커버(Modbus 마스터는 즉시 요청하므로 실질 무해). */
#define MB_TCP_KEEPALIVE_5S      2u

/* 앱 계층 유휴 타임아웃 — 소켓이 1개뿐이라 죽은 피어가 물고 있으면 아무도 못 붙는다.
 * 칩 KA 만으로는 두 가지가 부족했다:
 *   ① KA 자가치유가 실측 ~20초로 느리다.
 *   ② 🔴 **KA 는 데이터를 1회 이상 주고받은 뒤에만 동작한다**(위 주석). 연결만
 *      하고 아무것도 안 보낸 피어가 사라지면 ESTABLISHED 가 **영구 고착**되어
 *      새 SYN 이 RST 를 받는다 = 사람이 전원을 내리기 전까지 복구 불가.
 * 그래서 "마지막 유효 요청 이후 N 초"를 앱이 직접 재고 disconnect 한다.
 * 값: 게이트 침묵 임계(10s)보다 크고 KA 경로(~20s)보다 작게 잡아 이쪽이 지배하게
 * 한다. 실 Modbus 마스터는 100ms~1s 주기로 폴링하므로 정상 트래픽과 겹치지 않는다. */
#ifndef MB_TCP_IDLE_MAX_MS
#define MB_TCP_IDLE_MAX_MS   12000u
#endif

static uint8_t  s_acc[MB_TCP_ACC_LEN];      /* 수신 누적 (partial 이월) */
static uint16_t s_acc_len;
static uint8_t  s_txacc[MB_TCP_TXACC_LEN];  /* poll당 응답 코얼레스 (스택 스파이크 회피) */
static uint32_t s_cw_since_ms;              /* CLOSE_WAIT 진입 스탬프 */
static uint32_t s_last_rx_ms;               /* 마지막 유효 요청(또는 연결 수립) 스탬프 */
static uint8_t  s_cw_active;

/* sock0 강제 close */
void app_modbus_tcp_reset(void)
{
    /* M7 (eth re-apply) + M8 (comm_mode 이탈): sock0 강제 close + 수신/종료
     * 상태 리셋. close()는 CLOSED 소켓에 무해. 호출측 계약: W5500이 살아있던
     * 경로에서만 호출됨(app_eth_available 게이트) — 칩 부재 시 vendor close()의
     * 레지스터 폴링 스핀 위험은 도달 불가. */
    (void)close(MB_TCP_SOCK);
    s_acc_len   = 0u;
    s_cw_active = 0u;
}

/* 소켓 FSM 1스텝 */
static void control_tcp(void)
{
    /* Port of samd20 process_tcp.c control_tcps: walk the socket FSM one step.
     * M9: 소켓을 SF_IO_NONBLOCK으로 열어 disconnect()가 DISCON 발행 후 즉시
     * SOCK_BUSY 반환(socket.c:491-513) — CLOSE_WAIT에서 슈퍼루프 무정지. DISCON
     * 후 LAST_ACK로 이탈하므로 재발행 없음; 500ms 내 미완료면 close() 벨트. */
    switch (getSn_SR(MB_TCP_SOCK)) {
        case SOCK_ESTABLISHED:
            s_cw_active = 0u;
            if (getSn_IR(MB_TCP_SOCK) & Sn_IR_CON) {
                setSn_IR(MB_TCP_SOCK, Sn_IR_CON);
                s_acc_len    = 0u;   /* 새 연결: 이전 피어의 stale partial 폐기 */
                s_last_rx_ms = sys_tick_get_ms();   /* 유휴 시계 기준선 */
            }
            /* 유휴 타임아웃 → 끊고 재리슨. FSM 이 CLOSE_WAIT/CLOSED 를 거쳐
             * 스스로 다시 listen 하므로 여기서는 disconnect 만 하면 된다.
             * 사람 개입 없이 다음 피어가 붙을 수 있게 하는 것이 목적이다. */
            if ((uint32_t)(sys_tick_get_ms() - s_last_rx_ms) > MB_TCP_IDLE_MAX_MS) {
                mon_printf("[mbtcp] idle %us -> disconnect (재리슨)\r\n",
                           (unsigned)(MB_TCP_IDLE_MAX_MS / 1000u));
                (void)disconnect(MB_TCP_SOCK);
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
            if (socket(MB_TCP_SOCK, Sn_MR_TCP, MB_TCP_PORT, SF_IO_NONBLOCK)
                == (int8_t)MB_TCP_SOCK) {
                setSn_KPALVTR(MB_TCP_SOCK, MB_TCP_KEEPALIVE_5S);
            }
            break;
        default:
            break;
    }
}

/* TCP 서버 poll */
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
        /* ⚠ recv만 블로킹 모드로 일시 토글: 이 vendored socket.c의 비-IPv6
         * recv 경로(:687-692)는 논블로킹 체크가 recvsize 체크보다 앞이라
         * NONBLOCK이면 데이터가 있어도 무조건 SOCK_BUSY 반환(업스트림과
         * 순서 뒤집힘 — HW E2E 전면 무응답으로 발견, 2026-07-05). RSR>0
         * 가드 후 호출이라 블로킹 recv도 즉시 반환(RSR은 RECV 커맨드 전엔
         * 감소 불가; 피어 RST/close 시 벤더가 에러 반환 → 스톨 불가).
         * vendor read-only → ctlsocket 공개 API 우회. send/disconnect는
         * NONBLOCK 유지 (M9). */
        uint8_t iomode = SOCK_IO_BLOCK;
        (void)ctlsocket(MB_TCP_SOCK, CS_SET_IOMODE, &iomode);
        int32_t got = recv(MB_TCP_SOCK, &s_acc[s_acc_len], avail);
        iomode = SOCK_IO_NONBLOCK;
        (void)ctlsocket(MB_TCP_SOCK, CS_SET_IOMODE, &iomode);
        if (got > 0) {
            s_acc_len = (uint16_t)(s_acc_len + (uint16_t)got);
        }
    }
    if (s_acc_len == 0u) {
        return;
    }

    /* 워커: 완전 프레임을 순차 응답(TX 누적), poll당 상한으로 슈퍼루프 시간
     * 바운드. FC06을 만나면 그 프레임까지 처리(응답 append + apply_writes)
     * 하고 워크를 즉시 종료한다 — apply_writes는 holding[] vs cfg 1-change
     * -per-call else-if 체인이라, 클램프된 write는 holding에 raw 잔여를
     * 남기고 그 잔여는 poll 뒤 mirror_live()만 재동기함. 같은 poll에서
     * 두 번째 FC06까지 처리하면 잔여를 먼저 재발견해 반환 → 뒤 write가
     * 조용히 유실(whole-branch review HIGH). 잔여 프레임은 누적 버퍼로
     * 다음 poll에 이월되고, 그 사이 mirror_live()가 holding을 cfg와
     * 재동기하므로 RTU와 동일한 "poll당 최대 1 write-apply" 불변식이 됨. */
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
        bool     wrote = false;
        if (mb_tcp_build_response(app_modbus_core(), &s_acc[off], frame_len,
                                  &s_txacc[tx_len], &out_len, &fc)) {
            app_modbus_note_remote();   /* REMOTE icon (samd20 modbus_status) */
            s_last_rx_ms = sys_tick_get_ms();   /* 유휴 시계 리셋 */
            tx_len = (uint16_t)(tx_len + out_len);
            if (fc == 0x06u) {
                app_modbus_apply_writes(MB_LINK_TCP);
                wrote = true;
            }
        }
        off = (uint16_t)(off + frame_len);
        frames++;
        if (wrote) {
            /* FC06 뒤는 이번 poll에서 처리하지 않음: apply_writes는
             * one-change-per-call 체인 + 클램프 잔여(holding=raw vs
             * cfg=클램프) 재동기가 poll 뒤 mirror_live()뿐이라, 같은
             * poll에서 두 번째 FC06을 apply하면 잔여 재발견으로 굶겨
             * write가 조용히 유실됨(whole-branch review HIGH). 잔여
             * 프레임은 누적 버퍼로 다음 poll 이월 — mirror가 사이에
             * 돌아 RTU와 동일한 1-write-per-mirror 사이클. read-
             * after-write stale도 함께 차단. */
            break;
        }
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
