# eth-reapply (M7/D6) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

> **요약**: LCD DATA_SAVE로 ether IP/NM/GW 또는 comm_mode가 바뀌면 재부팅 없이 W5500에
> 즉시 반영. Task 4개 = ① LCD 트리거 경로(커밋 조건 확장 + dirty flag/getter)
> ② sock0 reset API ③ eth_reapply 본체 + F2 + tick 배선 ④ docs.
> host 테스트 없음(HAL/vendor 글루) — 게이트 = build 0-warning + host 7스위트 무회귀 +
> Task별 cpp-review + HW E2E(spec §6, plan 범위 밖).

**Goal:** LCD 저장 → 가동 중 W5500 netinfo/DHCP 라이프사이클 즉시 재적용 (spec = `docs/superpowers/specs/2026-07-04-eth-reapply-m7-design.md`).

**Architecture:** LCD 커밋이 hook를 통해 dirty 플래그를 세우고, `app_eth_tick()`이 consume-and-clear getter로 소비해 phase별 재적용(`eth_reapply`). 재적용 시 TCP sock0 강제 close(F1). 의존 방향은 app_eth→app_lcd 유지(M1 discipline), app_lcd는 app_eth를 모름.

**Tech Stack:** STM32F410 bare-metal C (C11), WIZnet ioLibrary(vendored), CMake+Ninja, host tests = `fw/test/Makefile`(cc).

## Global Constraints

- 브랜치: `feat/eth-reapply-m7` (BASE = 현 main `0daf4e4`)
- our-code **0-warning** 유지 (vendor `socket.h` 경고 3건은 pre-existing 무관)
- host 7스위트 무회귀: `make -C fw/test` 전부 PASS
- `fw/vendor/` 수정 금지, `ref/` 읽기 전용
- 요청 범위 밖 코드 무변경 (워크스페이스 규칙)
- 빌드: `env -u STM32_TOOLCHAIN cmake -B build -G Ninja && env -u STM32_TOOLCHAIN cmake --build build` (in `fw/`; 신규 소스 파일 없음 → reconfigure는 안전빵)
- 커밋 메시지 conventional commits, 본문 한국어 가능

---

### Task 0: 브랜치 생성

- [ ] **Step 1: 브랜치**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
git checkout -b feat/eth-reapply-m7
```

---

### Task 1: LCD 트리거 경로 — 커밋 조건 확장(G4) + dirty flag/getter

**Files:**
- Modify: `fw/src/app_lcd_input.c` (commit_comm_mode_and_ether, ~505-548행)
- Modify: `fw/src/app_lcd.c:54-61` (app_lcd_hook_ether_apply)
- Modify: `fw/include/app_lcd.h:151` 부근 (getter 선언 추가)

**Interfaces:**
- Consumes: 기존 `app_lcd_hook_ether_apply(uint8_t mode, const uint8_t ip[4], const uint8_t nm[4], const uint8_t gw[4])` (시그니처 무변경)
- Produces: `bool app_lcd_ether_dirty_take(void)` — consume-and-clear. **Task 3이 이 정확한 이름을 호출한다.**

- [ ] **Step 1: `app_lcd_input.c` — mode-only 변경도 hook 발화**

`commit_comm_mode_and_ether()`에서 아래와 같이 변경 (0xFF 가드·shadow 커밋 로직 무변경).

기존:

```c
/* MULTI-only: commit comm_mode + ether shadows → live cfg, firing the ether
 * hook on ether change (samd20 main.c:3327-3403). HAND/STD do NOT do this. */
static void commit_comm_mode_and_ether(void)
{
    lcd_app_state_t *state = app_lcd_state();
    app_config_t    *cfg   = app_lcd_cfg();
    bool ether_changed = false;
    uint8_t i;
```

변경 (comment + `mode_changed` 도입):

```c
/* MULTI-only: commit comm_mode + ether shadows → live cfg, firing the ether
 * hook on ether OR comm_mode change (samd20 main.c:3327-3403 re-ran
 * close_tcps+network_init on save — M7 restores that liveness). HAND/STD do
 * NOT do this. */
static void commit_comm_mode_and_ether(void)
{
    lcd_app_state_t *state = app_lcd_state();
    app_config_t    *cfg   = app_lcd_cfg();
    bool ether_changed = false;
    bool mode_changed  = false;
    uint8_t i;
```

기존:

```c
    if (state->temp_comm_mode != cfg->comm_mode) {
        cfg->comm_mode = state->temp_comm_mode;
    }
```

변경:

```c
    if (state->temp_comm_mode != cfg->comm_mode) {
        cfg->comm_mode = state->temp_comm_mode;
        mode_changed   = true;
    }
```

기존 (hook 발화부):

```c
    if (ether_changed) {
        for (i = 0; i < 4u; i++) {
            cfg->ether_ip[i] = state->temp_ether_ip[i];
            cfg->ether_nm[i] = state->temp_ether_nm[i];
            cfg->ether_gw[i] = state->temp_ether_gw[i];
        }
        app_lcd_hook_ether_apply(cfg->comm_mode, cfg->ether_ip, cfg->ether_nm, cfg->ether_gw);
    }
}
```

변경 (복사는 ether_changed 조건 유지, hook은 OR 조건으로 분리):

```c
    if (ether_changed) {
        for (i = 0; i < 4u; i++) {
            cfg->ether_ip[i] = state->temp_ether_ip[i];
            cfg->ether_nm[i] = state->temp_ether_nm[i];
            cfg->ether_gw[i] = state->temp_ether_gw[i];
        }
    }
    if (ether_changed || mode_changed) {
        app_lcd_hook_ether_apply(cfg->comm_mode, cfg->ether_ip, cfg->ether_nm, cfg->ether_gw);
    }
}
```

- [ ] **Step 2: `app_lcd.c` — dirty flag + getter + hook에 set 추가**

파일 상단 기존 static 변수들 근처(없으면 hook 함수 바로 위)에 추가:

```c
/* M7: LCD DATA_SAVE가 ether/comm_mode를 커밋했음을 app_eth_tick에 알리는
 * 1-shot 플래그. hook에서 set, app_lcd_ether_dirty_take()가 consume-and-clear.
 * (직접 호출 대신 플래그 = app_lcd↔app_eth include 사이클 회피, M1 discipline —
 * comm_reconfigure hook이 passive인 것과 같은 패턴.) */
static bool s_ether_dirty = false;

bool app_lcd_ether_dirty_take(void)
{
    bool d = s_ether_dirty;
    s_ether_dirty = false;
    return d;
}
```

`app_lcd_hook_ether_apply()` 본문 첫 줄에 set 추가 (mon_printf 유지):

```c
void app_lcd_hook_ether_apply(uint8_t mode, const uint8_t ip[4], const uint8_t nm[4], const uint8_t gw[4])
{
    s_ether_dirty = true;   /* consumed by app_eth_tick -> eth_reapply (M7) */
    mon_printf("[lcd-hook] ether mode=%u ip=%u.%u.%u.%u nm=%u.%u.%u.%u gw=%u.%u.%u.%u\r\n",
               (unsigned)mode,
               (unsigned)ip[0], (unsigned)ip[1], (unsigned)ip[2], (unsigned)ip[3],
               (unsigned)nm[0], (unsigned)nm[1], (unsigned)nm[2], (unsigned)nm[3],
               (unsigned)gw[0], (unsigned)gw[1], (unsigned)gw[2], (unsigned)gw[3]);
}
```

- [ ] **Step 3: `app_lcd.h` — getter 선언**

`app_lcd_hook_horn(bool down);` 선언 (151행 부근) 바로 아래에 추가:

```c
/* M7: consume-and-clear — LCD ether/comm_mode 커밋 이후 첫 호출만 true.
 * 소비자 = app_eth_tick (가동 중 netinfo/DHCP 재적용 트리거). */
bool app_lcd_ether_dirty_take(void);
```

- [ ] **Step 4: 빌드 + host 무회귀**

```bash
cd fw && env -u STM32_TOOLCHAIN cmake -B build -G Ninja && env -u STM32_TOOLCHAIN cmake --build build
make -C test
```

기대: our-code 0-warning, 7스위트 전부 PASS(`all checks PASSED`/`all passed`).

- [ ] **Step 5: Commit**

```bash
git add fw/src/app_lcd_input.c fw/src/app_lcd.c fw/include/app_lcd.h
git commit -m "feat(lcd): ether/comm_mode 커밋 시 dirty 플래그 — mode-only 변경도 hook 발화 (M7 G4)"
```

---

### Task 2: sock0 reset API (F1)

**Files:**
- Modify: `fw/src/app_modbus_tcp.c` (함수 1개 추가)
- Modify: `fw/include/app_modbus_tcp.h` (선언 1줄)

**Interfaces:**
- Consumes: vendor `socket.h`의 `close()` (이미 include됨), `MB_TCP_SOCK`(파일 내 define)
- Produces: `void app_modbus_tcp_reset(void)` — **Task 3이 이 정확한 이름을 호출한다.**

- [ ] **Step 1: `app_modbus_tcp.c` — reset 함수 추가**

`control_tcp()` 정의 앞(= `s_rxbuf` 선언 뒤)에 추가:

```c
/* M7 (eth re-apply): force-close sock0 so the FSM re-opens LISTEN on the new
 * netinfo next poll. Without this a stale ESTABLISHED to a dead peer would
 * permanently block new connections (single-socket server; the peer's FIN/RST
 * may never arrive after the IP change and there is no keep-alive).
 * close() on an already-CLOSED socket is harmless. */
void app_modbus_tcp_reset(void)
{
    (void)close(MB_TCP_SOCK);
}
```

- [ ] **Step 2: `app_modbus_tcp.h` — 선언 추가**

`void app_modbus_tcp_poll(void);` 아래에 추가:

```c
/* M7: eth 재적용 시 sock0 강제 close — control_tcp가 다음 poll에서 재open+listen. */
void app_modbus_tcp_reset(void);
```

- [ ] **Step 3: 빌드 + host 무회귀**

```bash
cd fw && env -u STM32_TOOLCHAIN cmake --build build
make -C test
```

기대: 0-warning, 7스위트 PASS.

- [ ] **Step 4: Commit**

```bash
git add fw/src/app_modbus_tcp.c fw/include/app_modbus_tcp.h
git commit -m "feat(mb-tcp): app_modbus_tcp_reset — eth 재적용 시 sock0 강제 close (M7 F1)"
```

---

### Task 3: eth_reapply 본체 + F2 + tick 배선

**Files:**
- Modify: `fw/src/app_eth.c` (include 1개, `eth_apply_on_link` 1줄, `eth_reapply` 신규, `app_eth_tick` 진입부)

**Interfaces:**
- Consumes: `bool app_lcd_ether_dirty_take(void)` (Task 1, `app_lcd.h` — 이미 include됨) / `void app_modbus_tcp_reset(void)` (Task 2, `app_modbus_tcp.h` — **신규 include 필요**) / vendor `DHCP_stop(void)` (`dhcp.h` — 이미 include됨)
- Produces: 없음 (모듈 내부 static)

- [ ] **Step 1: include 추가**

`app_eth.c`의 `#include "socket.h"` 아래에:

```c
#include "app_modbus_tcp.h" /* app_modbus_tcp_reset — sock0 drop on re-apply (M7) */
```

- [ ] **Step 2: F2 — `eth_apply_on_link()` DHCP 분기에 available 명시**

기존:

```c
    if (cfg->comm_mode == COMM_ETH_DHCP) {
        /* Put our MAC on the chip (SHAR) BEFORE DHCP_init: the ioLibrary client
```

변경 (분기 첫 줄에 추가):

```c
    if (cfg->comm_mode == COMM_ETH_DHCP) {
        s_available = false;   /* re-apply may arrive here from STATIC_UP where
                                * it was true — boot path is already false (F2) */
        /* Put our MAC on the chip (SHAR) BEFORE DHCP_init: the ioLibrary client
```

- [ ] **Step 3: `eth_reapply()` 신규 — `eth_apply_on_link()` 정의 뒤, `app_eth_init()` 앞**

```c
/* M7: LCD DATA_SAVE committed new comm_mode/ether fields (dirty flag from
 * app_lcd) — re-apply the net lifecycle against the live cfg without a reboot.
 * samd20 re-ran close_tcps+network_init on save (main.c:3327-3403); this
 * restores that liveness. Phase transitions (spec §3.3):
 *   DOWN      no-op (chip absent — boot policy, no retry)
 *   LINKWAIT  no-op (link-up path reads the live cfg anyway)
 *   STATIC_UP drop sock0 + re-apply (static re-netinfo, or DHCP start)
 *   DHCP_RUN  still DHCP -> keep the lease; else stop DHCP + drop sock0 +
 *             re-apply static */
static void eth_reapply(void)
{
    const app_config_t *cfg = app_lcd_cfg();

    mon_printf("[eth] reapply mode=%u phase=%u\r\n",
               (unsigned)cfg->comm_mode, (unsigned)s_phase);

    switch (s_phase) {
    case ETH_STATIC_UP:
        app_modbus_tcp_reset();   /* F1: stale sock0 would block the new IP */
        eth_apply_on_link();      /* live cfg: static re-apply or DHCP start */
        break;
    case ETH_DHCP_RUN:
        if (cfg->comm_mode == COMM_ETH_DHCP) {
            break;                /* mode unchanged — keep the lease */
        }
        DHCP_stop();              /* closes SOCK_DHCP, client -> STOP state */
        app_modbus_tcp_reset();
        eth_apply_on_link();
        break;
    case ETH_DOWN:
    case ETH_LINKWAIT:
    default:
        break;
    }
}
```

- [ ] **Step 4: `app_eth_tick()` 진입부 배선**

기존:

```c
void app_eth_tick(void)
{
    uint32_t now = sys_tick_get_ms();
```

변경:

```c
void app_eth_tick(void)
{
    /* M7: consume the LCD commit flag in every phase (DOWN/LINKWAIT re-apply
     * is a no-op — those phases read the live cfg on their own path). */
    if (app_lcd_ether_dirty_take()) {
        eth_reapply();
    }

    uint32_t now = sys_tick_get_ms();
```

- [ ] **Step 5: 빌드 + host 무회귀**

```bash
cd fw && env -u STM32_TOOLCHAIN cmake --build build
make -C test
```

기대: 0-warning, 7스위트 PASS. FLASH 사용률이 43%대에서 소폭 증가 예상(정상).

- [ ] **Step 6: Commit**

```bash
git add fw/src/app_eth.c
git commit -m "feat(eth): eth_reapply — LCD 저장 시 가동 중 netinfo/DHCP 재적용 + DHCP 분기 available 명시 (M7 G1-G3, F2)"
```

---

### Task 4: docs (changelog + NEXT_STEPS)

**Files:**
- Modify: `docs/changelog.md` (2026-07-04 항목에 추가 또는 신규 항목)
- Modify: `docs/NEXT_STEPS.md` (§1.3 D6 행 갱신, §2.2 큐 갱신)

**Interfaces:** 없음 (docs)

- [ ] **Step 1: changelog**

`### 2026-07-04 —` 항목 마지막 불릿("다음 = D6...")을 다음으로 교체:

```markdown
- **D6 'eth-reapply(M7)' CODE-COMPLETE** (branch `feat/eth-reapply-m7`, 미머지·HW E2E 게이트): LCD DATA_SAVE의 ether/comm_mode 변경을 재부팅 없이 W5500에 즉시 반영(samd20 main.c:3327-3403 close_tcps+network_init 거동 복원). LCD 커밋 조건 확장(mode-only도 hook 발화, G4)+dirty flag/`app_lcd_ether_dirty_take`+`app_eth_tick`의 phase별 `eth_reapply`(STATIC_UP=재적용/DHCP_RUN=모드 유지 시 리스 보존, 이탈 시 `DHCP_stop`)+`app_modbus_tcp_reset`(sock0 강제 close — stale ESTABLISHED가 새 IP 접속 영구 차단 방지, F1)+DHCP 분기 `s_available=false` 명시(F2). host 테스트 없음(HAL/vendor 글루)=기존 7스위트 무회귀+0-warning. spec=`specs/2026-07-04-eth-reapply-m7-design.md`, plan=`plans/2026-07-04-eth-reapply-m7.md`.
- 다음 = M7 HW E2E(spec §6: IP 변경 즉시 반영/STATIC↔DHCP 왕복/ETH↔SERIAL/ceiling 무회귀) 후 머지 → D5(reconcile b→d→ch1).
```

- [ ] **Step 2: NEXT_STEPS §1.3 D6 행 갱신**

D6 행의 내용 뒤에 다음을 덧붙임:

```
🔨 **CODE-COMPLETE 2026-07-04**(branch `feat/eth-reapply-m7`, 0-warning+host 7 무회귀) — 남은 것=HW E2E(spec §6 5항목) 후 머지
```

§2.2 큐의 `**D6(M7)**` 표기를 `~~D6(M7)~~🔨(HW E2E 대기)`로, 다음 강조를 `**D5(reconcile b→d→ch1)**`로 이동.

- [ ] **Step 3: Commit**

```bash
git add docs/changelog.md docs/NEXT_STEPS.md
git commit -m "docs: D6 eth-reapply(M7) CODE-COMPLETE 반영"
```

---

## HW E2E 체크리스트 (다음 보드 세션 — plan 범위 밖, spec §6)

1. ETH_STATIC 부팅 → 가동 중 LCD IP 변경 저장 → **재부팅 없이** 새 IP ping+FC03, 옛 IP 무응답, mon `[eth] reapply`+`[eth] up ip=<new>`.
2. STATIC→DHCP 전환 저장 → `[eth] dhcp lease`+FC03+LCD 리스 IP.
3. DHCP→STATIC 전환 저장 (⚠ IP 필드 직접 입력 — 리스 미러 persist 함정).
4. ETH→SERIAL 저장 → RTU 응답+TCP 무응답 / SERIAL→ETH 재전환 → TCP 복귀.
5. 직접런 ceiling 무회귀 (START→STATUS 1×N→0).
6. PASS 시 main 머지(--no-ff) + tag `hw-revA_fw-stage-eth-reapply`.

## Self-Review 결과

- spec 커버리지: §3.1=Task 1 Step 1 / §3.2=Task 1 Step 2-3 / §3.3(F2 포함)=Task 3 / §3.4(F1)=Task 2 / §6 host 게이트=각 Task 검증 스텝 / §6 HW=범위 밖 명시. G1~G4 전부 매핑.
- placeholder 없음, 타입 일관성: `app_lcd_ether_dirty_take`·`app_modbus_tcp_reset` 이름 Task 간 일치 확인.
- 주의: `eth_reapply`는 `eth_apply_on_link` **정의 뒤**에 배치해야 함(전방선언 없음). tick 배선의 static 지역 cadence 변수(`s_link_ms`/`s_dhcp_ms`)는 재진입 시에도 안전(단조 시계 델타 비교).
