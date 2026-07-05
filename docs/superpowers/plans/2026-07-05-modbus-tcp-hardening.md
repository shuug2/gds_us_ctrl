# modbus-tcp-hardening (M6/M8/M9 + RS-485 조사) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

> **요약**: spec `docs/superpowers/specs/2026-07-05-modbus-tcp-hardening-design.md`
> 구현 plan. T1 순수 프레임 워커 `mb_tcp_frame_peek` TDD → T2 `app_modbus_tcp.c`
> 누적 수신+워커 루프+논블로킹 소켓+CLOSE_WAIT 벨트 + `app_modbus.c` tcp_active
> 하강 엣지 정리 → T3 RS-485 첫-write 코드 조사 보고 → T4 통합 cpp-review.
> HW E2E(§Task 4 체크리스트)와 머지+태그는 벤치 세션 게이트.

**Goal:** Modbus TCP 서버의 coalesced 프레임 폐기(M6)/전환 시 소켓 방치(M8)/blocking 스톨(M9)을 해소하고 RS-485 첫-write 간헐 무효의 코드 원인 후보를 보고한다.

**Architecture:** 순수 프레임 판정 함수(host-test)를 신설하고, 글루는 누적 버퍼 + poll당 코얼레스드 단일 send + `SF_IO_NONBLOCK` 소켓으로 재구성. `app_modbus.c`는 TCP를 떠나는 전이에서만 기존 `app_modbus_tcp_reset()`을 호출.

**Tech Stack:** C11 bare-metal(STM32F410, 슈퍼루프), WIZnet ioLibrary(vendored, 수정 ✗), host 테스트 = `fw/test/Makefile`(cc, manual assert).

## Global Constraints

- `fw/vendor/` 수정 금지 (read-only in-tree).
- 우리 코드 0-warning: `-Wall -Wextra -Wundef -Wshadow` (ARM + host 공통).
- `mb_tcp_build_response`/`app_modbus_core.c` 무변경 (spec §4).
- 커밋 메시지 conventional (`feat:`/`fix:`/`docs:`), 브랜치 `feat/modbus-tcp-hardening` (main에서 분기).
- 빌드: `cd fw && env -u STM32_TOOLCHAIN cmake --build build` (신규 .c 없음 → 재구성 불요; T1은 기존 파일 수정만).
- host 테스트: `cd fw/test && make` → 전 스위트 `all checks PASSED`/`all passed`.
- 벤더 계약(사전 검증 완료 — 재확인 불요): `SF_IO_NONBLOCK=0x01`(socket.h:143, 비-IPv6 경로), TCP+flag 검증 통과(socket.c:290-295), `sock_io_mode` bit0 세팅(socket.c:342), 논블로킹 `disconnect()`=DISCON 발행 후 `SOCK_BUSY` 즉시 반환(socket.c:491-513), 논블로킹 `send()`=직전 SENDOK 미도래 시 `SOCK_BUSY`(0)/TIMEOUT 시 내부 close+`SOCKERR_TIMEOUT`/여유 부족 시 `SOCK_BUSY`/성공 시 len 반환·무대기(socket.c:514-604의 `#if 1` 첫 번째 정의가 활성).

---

## 사전: 브랜치 생성

- [ ] `git -C /Users/tknoh/dev/work/gds_us_ctrl checkout -b feat/modbus-tcp-hardening`

---

### Task 1: `mb_tcp_frame_peek` 순수 프레임 워커 (M6 코어, TDD)

**Files:**
- Modify: `fw/include/app_modbus_tcp_frame.h` (선언 추가)
- Modify: `fw/src/app_modbus_tcp_frame.c` (구현 추가)
- Test: `fw/test/test_app_modbus_tcp_frame.c` (케이스 추가)

**Interfaces:**
- Consumes: `MB_TCP_MBAP_LEN`(6u, 기존), `MB_FRAME_MAX`(125u, `app_modbus_core.h:11`)
- Produces: `mb_tcp_fr_t mb_tcp_frame_peek(const uint8_t *buf, uint16_t len, uint16_t *frame_len)` — T2가 워커 루프에서 호출. enum `MB_TCP_FR_NEED_MORE=0 / MB_TCP_FR_OK / MB_TCP_FR_DESYNC`.

- [ ] **Step 1: 실패하는 테스트 작성** — `fw/test/test_app_modbus_tcp_frame.c`의 `main()` 위에 아래 함수를 추가하고, `main()` 안 기존 호출들 다음에 `test_frame_peek();` 호출을 추가:

```c
/* M6 프레임 워커: 누적 버퍼 선두의 완전 프레임 판정 (spec §2.1). */
static void test_frame_peek(void)
{
    uint16_t fl = 0u;
    /* 정상 FC03 요청 12B (기존 test_fc03_read_one과 동일 형태) */
    uint8_t one[] = { 0x00,0x01, 0x00,0x00, 0x00,0x06,
                      0x01, 0x03, 0x00,0x06, 0x00,0x01 };

    /* MBAP 헤더(6B) 미만 -> NEED_MORE */
    CHECK(mb_tcp_frame_peek(one, 5u, &fl) == MB_TCP_FR_NEED_MORE);
    /* 헤더만(6B, length=6 선언) -> 본문 미도착 NEED_MORE */
    CHECK(mb_tcp_frame_peek(one, 6u, &fl) == MB_TCP_FR_NEED_MORE);
    /* 부분(10B) -> NEED_MORE */
    CHECK(mb_tcp_frame_peek(one, 10u, &fl) == MB_TCP_FR_NEED_MORE);
    /* 완전 12B -> OK, frame_len=12 */
    fl = 0u;
    CHECK(mb_tcp_frame_peek(one, 12u, &fl) == MB_TCP_FR_OK);
    CHECK(fl == 12u);
    /* coalesced 2프레임(24B): 선두 판정은 첫 프레임 12만 */
    uint8_t two[24];
    memcpy(two, one, 12u); memcpy(&two[12], one, 12u); two[13] = 0x02u; /* txn 차별화 */
    fl = 0u;
    CHECK(mb_tcp_frame_peek(two, 24u, &fl) == MB_TCP_FR_OK);
    CHECK(fl == 12u);
    /* proto != 0 -> DESYNC */
    uint8_t bad_proto[12]; memcpy(bad_proto, one, 12u); bad_proto[2] = 0x01u;
    CHECK(mb_tcp_frame_peek(bad_proto, 12u, &fl) == MB_TCP_FR_DESYNC);
    /* length < 2 -> DESYNC */
    uint8_t bad_short[12]; memcpy(bad_short, one, 12u);
    bad_short[4] = 0x00u; bad_short[5] = 0x01u;
    CHECK(mb_tcp_frame_peek(bad_short, 12u, &fl) == MB_TCP_FR_DESYNC);
    /* length > MB_FRAME_MAX(125) -> DESYNC (build_response :20 경계와 동일) */
    uint8_t bad_big[12]; memcpy(bad_big, one, 12u);
    bad_big[4] = 0x00u; bad_big[5] = (uint8_t)(MB_FRAME_MAX + 1u);
    CHECK(mb_tcp_frame_peek(bad_big, 12u, &fl) == MB_TCP_FR_DESYNC);
    /* length 경계 정확값(125) + 미도착 -> NEED_MORE (DESYNC 아님) */
    uint8_t max_hdr[6] = { 0x00,0x02, 0x00,0x00, 0x00, (uint8_t)MB_FRAME_MAX };
    CHECK(mb_tcp_frame_peek(max_hdr, 6u, &fl) == MB_TCP_FR_NEED_MORE);
}
```

- [ ] **Step 2: RED 확인** — Run: `cd fw/test && make 2>&1 | grep -A2 tcp_frame`
  Expected: 컴파일 에러 `mb_tcp_frame_peek`/`MB_TCP_FR_NEED_MORE` undeclared (선언 전이므로).

- [ ] **Step 3: 선언 추가** — `fw/include/app_modbus_tcp_frame.h` 파일 끝(`mb_tcp_build_response` 선언 뒤)에 추가:

```c
/* M6 프레임 워커(spec §2.1): 누적 수신 버퍼 선두에서 완전 MBAP 프레임 1개를
 * 판정. *frame_len = 6(MBAP) + length필드(unit+PDU) wire 길이. DESYNC(헤더
 * 불량)면 호출측이 누적 버퍼를 통째로 폐기 — 재동기는 마스터 재시도에 위임.
 * length 수용 경계는 mb_tcp_build_response(:20)와 동일(MB_FRAME_MAX). */
typedef enum {
    MB_TCP_FR_NEED_MORE = 0,   /* 더 수신 필요 (frame_len 미기록) */
    MB_TCP_FR_OK,              /* *frame_len 바이트의 완전 프레임 */
    MB_TCP_FR_DESYNC           /* proto!=0 / length<2 / length>MB_FRAME_MAX */
} mb_tcp_fr_t;

mb_tcp_fr_t mb_tcp_frame_peek(const uint8_t *buf, uint16_t len,
                              uint16_t *frame_len);
```

- [ ] **Step 4: 구현 추가** — `fw/src/app_modbus_tcp_frame.c` 파일 끝에 추가:

```c
mb_tcp_fr_t mb_tcp_frame_peek(const uint8_t *buf, uint16_t len,
                              uint16_t *frame_len)
{
    if (len < MB_TCP_MBAP_LEN) {
        return MB_TCP_FR_NEED_MORE;
    }
    uint16_t proto  = (uint16_t)(((uint16_t)buf[2] << 8) | buf[3]);
    uint16_t length = (uint16_t)(((uint16_t)buf[4] << 8) | buf[5]);
    if ((proto != 0u) || (length < 2u) || (length > MB_FRAME_MAX)) {
        return MB_TCP_FR_DESYNC;
    }
    uint16_t total = (uint16_t)(MB_TCP_MBAP_LEN + length);
    if (len < total) {
        return MB_TCP_FR_NEED_MORE;
    }
    *frame_len = total;
    return MB_TCP_FR_OK;
}
```

- [ ] **Step 5: GREEN 확인** — Run: `cd fw/test && make`
  Expected: `app_modbus_tcp_frame: all checks PASSED` + 전 스위트 PASS.

- [ ] **Step 6: ARM 빌드 0-warning 확인** — Run: `cd fw && env -u STM32_TOOLCHAIN cmake --build build 2>&1 | grep -v wiznet`
  Expected: 우리 파일 경고 0, 링크 성공.

- [ ] **Step 7: Commit**

```bash
git add fw/include/app_modbus_tcp_frame.h fw/src/app_modbus_tcp_frame.c fw/test/test_app_modbus_tcp_frame.c
git commit -m "feat(mbtcp): M6 순수 프레임 워커 mb_tcp_frame_peek + host 테스트 (spec §2.1)"
```

---

### Task 2: 글루 재구성 — 누적/워커/논블로킹/벨트 (M6+M9) + 하강 엣지 정리 (M8)

**Files:**
- Modify: `fw/src/app_modbus_tcp.c` (poll 재작성)
- Modify: `fw/src/app_modbus.c:311-348` (tick의 tcp_active 하강 엣지)

**Interfaces:**
- Consumes: `mb_tcp_frame_peek`(T1), `app_modbus_tcp_reset()`(기존 M7 산물, 본 Task에서 확장), `sys_tick_get_ms()`(`include/sys_tick.h`), `mon_printf`(`include/mon.h`)
- Produces: 외부 API 무변경 (`app_modbus_tcp_poll`/`app_modbus_tcp_reset` 시그니처 유지 — `app_modbus.c`/`app_eth.c` 호출처 재컴파일만)

- [ ] **Step 1: `app_modbus_tcp.c` 전체를 아래로 교체** (파일 헤더 주석의 "recv당 1프레임 가정"이 본 Task로 폐기되므로 주석도 함께 갱신):

```c
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
```

- [ ] **Step 2: `app_modbus.c` 하강 엣지 정리 (M8)** — `app_modbus_tick()`(`:306`) 위에 헬퍼 추가, tick의 두 곳 수정.

`g_tcp_active` 정의 근처(파일 상단 static 변수부)에 아래 헬퍼를 `app_modbus_tick` 바로 위에 추가:

```c
/* M8: TCP 서버를 떠나는 전이(RTU 점유 or ETH 불가)에서 sock0 + 수신 상태
 * 정리 — ESTABLISHED 방치 차단(감사 M8). 전이에서만 호출해 매-tick 중복
 * close(무해하나 SPI 낭비) 회피. g_tcp_active=1은 app_eth_available() 참
 * 경로에서만 세워지므로 칩-부재 시 도달 불가(vendor close 스핀 안전). */
static void tcp_leave(void)
{
    if (g_tcp_active != 0u) {
        app_modbus_tcp_reset();
        g_tcp_active = 0u;
    }
}
```

RTU 분기(`:314`)의 `g_tcp_active = 0u;`를 `tcp_leave();`로 교체:

```c
    if (g_applied.owned != 0u) {
        /* RTU owns USART6 (comm_mode==SERIAL && addr!=0). Behavior-identical
         * to the hardware-verified slice-1 path. */
        tcp_leave();
```

else 분기(`:345-347`)의 `g_tcp_active = 0u;`도 교체:

```c
    } else {
        tcp_leave();
    }
```

- [ ] **Step 3: ARM 빌드 0-warning** — Run: `cd fw && env -u STM32_TOOLCHAIN cmake --build build 2>&1 | grep -v wiznet`
  Expected: 우리 파일 경고 0, FLASH/RAM 사용률 출력(RAM +~650B 예상 = s_acc 262+s_txacc 516−구 s_rxbuf 131).

- [ ] **Step 4: host 회귀** — Run: `cd fw/test && make`
  Expected: 전 스위트 PASS (글루 무 host-test — 회귀 확인만).

- [ ] **Step 5: Commit**

```bash
git add fw/src/app_modbus_tcp.c fw/src/app_modbus.c
git commit -m "feat(mbtcp): M6 누적+워커+코얼레스드 send / M9 SF_IO_NONBLOCK+CLOSE_WAIT 벨트 / M8 tcp_active 하강 엣지 소켓 정리"
```

---

### Task 3: RS-485 첫-write 간헐 무효 — 코드 조사 보고 (read-only)

**Files:**
- Create: `docs/superpowers/research/2026-07-05-rs485-first-write.md`
- 코드 수정 없음 (원인 확정적+저위험일 때만 별도 fix 커밋 — 컨트롤러 승인 필요)

**Interfaces:**
- Consumes(분석 대상): `fw/drivers/usart6_mb.c`(전체 180줄 — DMA circular RX `rx_head/s_rx_tail/s_prev_head/s_last_rx_ms` 프레이밍, `mb_gap_ms` 테이블, `usart6_mb_open:44`의 초기 인덱스/타임스탬프 세팅, `usart6_mb_send:162`의 half-duplex 처리), `fw/src/app_modbus.c:311-330`(RTU 분기 decode→send→apply 순서), `fw/src/app_modbus_core.c`의 FC06 경로, `apply_config():250-297`의 open 직후 상태
- Produces: 보고서 — 후속 벤치(HMI Task 8) 세션의 재현 절차 입력

- [ ] **Step 1: 정적 분석** — 아래 가설 각각에 대해 해당 코드를 읽고 성립/기각/추가정보-필요를 판정 (파일:라인 인용 필수):
  1. **open 직후 stale DMA 인덱스**: `usart6_mb_open`이 `s_rx_tail`/`s_prev_head`를 현재 DMA 위치가 아닌 0으로 세팅해, open 전 라인 노이즈가 첫 프레임 앞에 붙어 CRC 실패 → 무응답.
  2. **첫 프레임 gap 판정 실패**: `s_last_rx_ms` 초기값과 `mb_gap_ms` 비교 로직이 open 직후 첫 수신에서 프레임 완성 판정을 못 하거나 조기 절단.
  3. **어댑터 접속/DE 턴어라운드 글리치 바이트**: USB-RS485 어댑터 첫 전송 시 라인 글리치 1바이트가 프레임 선두 오염 → CRC 실패 → Modbus 정상 침묵(재시도로 회복 — FW 결함 아님으로 분류될 수 있음).
  4. **decode→apply 체인의 첫-write 특이 조건**: `mb_core_decode`가 응답을 만들되 `apply_writes`가 첫 write에서 cfg 반영을 놓치는 경로(예: mirror_live 선행 여부, 클램프 에코) — "응답은 왔는데 값 무효" 시나리오 (사용자 관찰이 "무응답"인지 "응답 후 무효"인지 벤치 기록 불충분 → 보고서에 양 시나리오 매핑).
- [ ] **Step 2: 보고서 작성** — `docs/superpowers/research/2026-07-05-rs485-first-write.md` (문서 첫머리 요약 규칙 준수): 가설별 판정 + 근거(파일:라인) + 최유력 원인 순위 + **벤치 재현 절차**(RS-485 어댑터 연결 후: 전원 사이클 → 첫 FC06 1회 → 성공/실패 기록 ×10회, 실패 시 mon/SWD 관찰 포인트) + 수정 후보(있다면)와 위험도.
- [ ] **Step 3: Commit**

```bash
git add docs/superpowers/research/2026-07-05-rs485-first-write.md
git commit -m "docs(research): RS-485 첫-write 간헐 무효 코드 조사 — 가설 판정 + 벤치 재현 절차 (spec §2.4)"
```

---

### Task 4: 통합 cpp-review → HW E2E → 머지+태그

**Files:** (리뷰/검증 — 코드 수정은 리뷰 지적 반영 시만)

- [ ] **Step 1: 통합 cpp-reviewer** — 브랜치 전체 diff(`git diff main...HEAD`) 대상. CRITICAL/HIGH = 즉시 수정 후 재리뷰, MEDIUM 이하 = 컨트롤러 판단.
- [ ] **Step 2: HW E2E (이더넷 벤치 — 머지 게이트, spec §5.2)**:
  1. **M6**: 워크스테이션에서 파이프라인 테스트 (보드 IP는 LCD 확인, 예 192.168.1.199):

```python
# scratchpad/mb_pipeline.py — 한 TCP 세그먼트에 FC03 요청 2개
# 응답: FC03 1-reg 정상응답 = 11B ×2 = 22B. 두 주소 모두 맵(0x00~0x1D) 내.
import socket, sys
ip = sys.argv[1]
req1 = bytes([0,1, 0,0, 0,6, 1,3, 0,6, 0,1])   # txn1 FC03 addr6 (OUT_POWER)
req2 = bytes([0,2, 0,0, 0,6, 1,3, 0,7, 0,1])   # txn2 FC03 addr7 (ON_TIME)
s = socket.create_connection((ip, 502), timeout=3)
s.sendall(req1 + req2)                          # coalesced 단일 세그먼트
buf = b''
while len(buf) < 22:
    chunk = s.recv(64)
    if not chunk:
        break
    buf += chunk
print(buf.hex(), len(buf))
assert len(buf) == 22 and buf[0:2] == b'\x00\x01' and buf[11:13] == b'\x00\x02', "2nd response missing"
print("PIPELINE OK")
```

     Expected: `PIPELINE OK` (구 펌웨어면 2번째 응답 미수신 timeout). mbpoll 단일 요청 회귀 병행: `mbpoll -m tcp -a 1 -r 7 -c 3 <ip>` 정상.
  2. **M8**: ESTABLISHED 유지 중 LCD comm_mode→SERIAL 저장 → SWD `Sn_SR(0)` read(`openocd read_memory` — W5500은 SPI 레지스터라 SWD 불가 → 대신 **워크스테이션 소켓이 close/RST 수신**하는지 python 스크립트로 관찰: `recv()`가 b'' 반환) → ETH 재전환 → 재연결+FC03 정상.
  3. **M9**: FC03 1초 폴링 중 이더넷 케이블 분리 → 보드 LCD 터치 반응성 유지(육안) + TCL 샘플러(비침습)로 슈퍼루프 진행 확인(예: sys_tick ms 카운터 연속 증가, >100ms 공백 없음) → 케이블 복귀 → 재연결 정상.
  4. **회귀**: mbpoll -m tcp FC03 미러/FC06 클램프+에코(120→100), 직접-초음파 START→ceiling STATUS `1×n→0`, comm_mode 왕복(ETH↔SERIAL) 무크래시.
- [ ] **Step 3: 머지+태그** (HW 전항목 PASS 후):

```bash
git checkout main
git merge --no-ff feat/modbus-tcp-hardening -m "merge: modbus-tcp-hardening (M6/M8/M9) — HW E2E PASS"
git tag hw-revA_fw-stage-mbtcp-hardening
git branch -d feat/modbus-tcp-hardening
```

- [ ] **Step 4: 문서** — `docs/changelog.md` 항목 추가 + HANDOFF/NEXT_STEPS 갱신(세션 마감 절차), push는 사람 터미널.
