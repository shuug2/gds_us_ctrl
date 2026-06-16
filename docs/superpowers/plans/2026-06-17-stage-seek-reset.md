# SEEK/RESET 효과 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** SEEK/RESET 명령의 효과(현재 `app_reg_command` no-op)를 순수 FSM + 글루로 구현 — RESET→500ms→SEEK 자동 체인→500ms→자동 해제, SEEK 단발, 양방향 RUN 직교, ICON 엣지 렌더.

**Architecture:** weld 슬라이스 패턴 동형 — HAL-free 순수 FSM 코어(`app_seek_reset_fsm`, host-test) + HAL 결합 글루(`app_seek_reset`, 10ms tick + hook/ICON). `app_reg`는 ① SEEK/RESET no-op을 글루 위임으로 교체 ② START guard에 `app_seek_reset_active()` read-only 조회 추가. 물리 OSC 신호 구동은 hook stub(mon 로그) — 실제 CTRL_OSC* 핀 구동은 B-SEAM 이연.

**Tech Stack:** C11, STM32 HAL (vendor in-tree), 자체 host-test (fw/test, `cc` + CHECK_EQ 매크로), CMake/Ninja (`file(GLOB)` 소스 수집).

**Spec:** `docs/superpowers/specs/2026-06-17-stage-seek-reset-design.md`

**Reference (samd20):** `ref/samd20/main.c:5388-5408`(자동 체인)·`118`(MAX_US_RESET_CNT). **Pattern (현 코드):** `fw/src/app_weld_fsm.c`(FSM)·`fw/src/app_weld.c`(글루)·`fw/test/test_app_weld_fsm.c`(host-test).

**Plan 작성 시 검증된 사실 (drift 없음):**
- START guard = `app_reg.c:96-97` (`main_state==0 && us_run_status==US_IDLE`)
- SEEK/RESET 현 no-op = `app_reg.c:139-146` (`case US_CMD_SEEK: case US_CMD_RESET: default: break;` — **위임 추가 시 `default:`를 분리**해야 alien cmd 흡수 유지)
- `ICON_RESET=0x1150`/`ICON_SEEK=0x1151` = `dgus_lcd.h:70-71`
- ICON write 헬퍼 `app_lcd_icon(uint16_t vp, bool on)` = `app_lcd_disp.c:250` 구현 존재 (spec §10 미해결 #4 해소 — 신설 불요)
- input layer(`app_lcd_input.c:99-138`)는 `ICON_RESET`/`ICON_SEEK`를 **write하지 않음** — 에러 상태에서만 `ICON_OL`/`ICON_OUTERR`(별개 에러 아이콘) 블랭킹 → FSM-구동 ICON write는 **clean add**, coordination 문제 없음
- `app_reg_measure()->us_run_status` = run_active 주입 소스 (weld의 `curr_energy` 주입과 동형)
- 슈퍼루프 배선 = `app.c:66 app_loop_iter`, `app_weld_tick()`은 line 83 (reg_tick 앞)
- init = `main.c:29 app_weld_init()`

---

## File Structure

| 파일 | 책임 | 생성/수정 |
|---|---|---|
| `fw/include/app_seek_reset_fsm.h` | 순수 FSM 인터페이스 (enum, in/out struct, 선언) | Create |
| `fw/src/app_seek_reset_fsm.c` | FSM 전이 로직 (SR_IDLE/RESET/SEEK, 500ms 체인/해제, RUN 직교) | Create |
| `fw/test/test_app_seek_reset_fsm.c` | host 단위 테스트 (6 시나리오) | Create |
| `fw/test/Makefile` | 신규 스위트 빌드/실행 배선 | Modify |
| `fw/include/app_seek_reset.h` | 글루 인터페이스 (request/tick/active/hook) | Create |
| `fw/src/app_seek_reset.c` | 글루 (10ms tick, FSM 구동, hook/ICON 라우팅, cmd 래치) | Create |
| `fw/src/app_reg.c` | SEEK/RESET 위임 + `default:` 분리 + START guard에 active 추가 | Modify |
| `fw/src/app.c` | `app_seek_reset_tick()` 슈퍼루프 배선 | Modify |
| `fw/src/main.c` | `app_seek_reset_init()` boot 배선 | Modify |

> **순환 의존 없음 (spec §10.5):** `app_seek_reset.c`가 `app_reg.h`(`app_reg_measure`) include + `app_reg.c`가 `app_seek_reset.h`(`app_seek_reset_active`/`_request`) include — 각 `.c`가 상대 헤더의 함수 선언만 가져옴(헤더 순환 아님). `app_seek_reset_active()`는 부작용 없는 read-only 상태 조회로 한정.

---

## Task 1: FSM 인터페이스 헤더 + 스캐폴드 + host-test 배선

순수 FSM의 인터페이스를 확정하고, 컴파일되는 빈 스텁 + init smoke 테스트로 빌드/테스트 파이프라인에 배선한다. 전이 로직은 Task 2에서 TDD.

**Files:**
- Create: `fw/include/app_seek_reset_fsm.h`
- Create: `fw/src/app_seek_reset_fsm.c`
- Create: `fw/test/test_app_seek_reset_fsm.c`
- Modify: `fw/test/Makefile`

- [ ] **Step 1: FSM 헤더 작성**

Create `fw/include/app_seek_reset_fsm.h`:

```c
/* fw/include/app_seek_reset_fsm.h — Stage SEEK/RESET: HAL-free pure FSM core.
 * samd20 공진 RESET/SEEK 명령 효과 (ref/samd20/main.c:5388-5408): TOUCH/COMM
 * 소스 RESET은 500ms 유지 후 SEEK으로 자동 체인 → 또 500ms 후 자동 해제. SEEK
 * 직접은 체인 없는 500ms 단발. 물리 OSC 신호 구동(reset_signal/seek_signal →
 * CTRL_OSC*)은 글루의 hook stub(B-SEAM 이연); 본 코어는 상태/타이밍/ICON 엣지만.
 * 글루(app_seek_reset.c)가 10ms마다 seek_reset_fsm_step() 호출. host-tested. */
#pragma once
#include <stdint.h>

enum { SR_IDLE = 0, SR_RESET = 1, SR_SEEK = 2 };
enum { SR_CMD_NONE = 0, SR_CMD_RESET = 1, SR_CMD_SEEK = 2 };

/* 500ms / 10ms tick = 50. samd20 100ms·us_reset_cnt>5의 명목 500ms를 10ms tick
 * 액면가로 재현 (±100ms quirk 미재현 — 명령 신호라 정밀도 비요구, spec §3.2). */
#define SR_TICKS  50u

/* step input — 글루가 매 tick 주입 (app_reg limit_on_time 주입 패턴; app_lcd로
 * 콜백 없음). */
typedef struct {
    uint8_t cmd;          /* SR_CMD_* — 이 tick의 명령 (없으면 NONE) */
    uint8_t run_active;   /* 1 = us_run_status != US_IDLE (RUN 중 → 명령 무시) */
} seek_reset_in_t;

/* step output — signal은 active 레벨, *_icon/*_icon_off는 전이 step 1-shot 엣지
 * (out은 매 step memset 0 → 엣지 자동). */
typedef struct {
    uint8_t state;          /* 현재 SR_* */
    uint8_t reset_signal;   /* RESET 명령선 레벨 (1=active) */
    uint8_t seek_signal;    /* SEEK 명령선 레벨 (1=active) */
    uint8_t reset_icon;     /* 1-shot: RESET ICON on 엣지 */
    uint8_t reset_icon_off; /* 1-shot: RESET ICON off 엣지 */
    uint8_t seek_icon;      /* 1-shot: SEEK ICON on 엣지 */
    uint8_t seek_icon_off;  /* 1-shot: SEEK ICON off 엣지 */
} seek_reset_out_t;

void seek_reset_fsm_init(void);                                    /* reset to IDLE (boot) */
void seek_reset_fsm_step(const seek_reset_in_t *in, seek_reset_out_t *out); /* one 10ms tick */
uint8_t seek_reset_fsm_state(void);                                /* current SR_* */
```

- [ ] **Step 2: FSM 스캐폴드 작성 (빈 step)**

Create `fw/src/app_seek_reset_fsm.c`:

```c
/* fw/src/app_seek_reset_fsm.c — SEEK/RESET FSM core. HAL-free. */
#include "app_seek_reset_fsm.h"
#include <string.h>

static uint8_t  s_state;
static uint16_t s_elapsed;   /* 진입 후 경과 (10ms/tick); SR_TICKS와 직접 비교 */

void seek_reset_fsm_init(void)
{
    s_state   = SR_IDLE;
    s_elapsed = 0u;
}

uint8_t seek_reset_fsm_state(void)
{
    return s_state;
}

void seek_reset_fsm_step(const seek_reset_in_t *in, seek_reset_out_t *out)
{
    memset(out, 0, sizeof(*out));
    (void)in;            /* Task 2에서 전이 로직 채움 */
    out->state = s_state;
}
```

- [ ] **Step 3: host-test 스캐폴드 (init smoke)**

Create `fw/test/test_app_seek_reset_fsm.c`:

```c
/* fw/test/test_app_seek_reset_fsm.c — host unit tests for the pure SEEK/RESET
 * FSM core. No HAL, no hardware. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "app_seek_reset_fsm.h"

static int failures = 0;

#define CHECK_EQ(expr, expected) do {                                       \
    unsigned long a_ = (unsigned long)(expr);                               \
    unsigned long e_ = (unsigned long)(expected);                           \
    if (a_ != e_) {                                                         \
        printf("FAIL %s:%d  %s = %lu, expected %lu\n",                      \
               __FILE__, __LINE__, #expr, a_, e_);                          \
        failures++;                                                         \
    }                                                                       \
} while (0)

static void test_init_idle(void)
{
    seek_reset_fsm_init();
    CHECK_EQ(seek_reset_fsm_state(), SR_IDLE);
}

int main(void)
{
    test_init_idle();
    if (failures) { printf("test_app_seek_reset_fsm: %d FAILED\n", failures); return 1; }
    printf("test_app_seek_reset_fsm: all passed\n");
    return 0;
}
```

- [ ] **Step 4: Makefile에 신규 스위트 배선**

Modify `fw/test/Makefile` — `BIN_SR` 변수 추가, `test` 타깃 deps/실행에 추가, 빌드 규칙 추가, `clean`에 추가. 최종 파일:

```makefile
# fw/test/Makefile — host unit tests for the pure (HAL-free) layers:
# regulation compute (app_reg_calc) + Modbus core (app_modbus_core).
# Runs on the host toolchain. Usage: make -C fw/test test
CC     ?= cc
CFLAGS := -std=c11 -Wall -Wextra -Wundef -Wshadow -O0 -g
INC    := -I../include
BIN_REG := /tmp/gds_test_app_reg_calc
BIN_MB  := /tmp/gds_test_app_modbus_core
BIN_TCP  := /tmp/gds_test_app_modbus_tcp_frame
BIN_WELD := /tmp/gds_test_app_weld_fsm
BIN_SR  := /tmp/gds_test_app_seek_reset_fsm

test: $(BIN_REG) $(BIN_MB) $(BIN_TCP) $(BIN_WELD) $(BIN_SR)
	$(BIN_REG)
	$(BIN_MB)
	$(BIN_TCP)
	$(BIN_WELD)
	$(BIN_SR)

$(BIN_REG): test_app_reg_calc.c ../src/app_reg_calc.c ../include/app_reg_calc.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_app_reg_calc.c ../src/app_reg_calc.c

$(BIN_MB): test_app_modbus_core.c ../src/app_modbus_core.c ../include/app_modbus_core.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_app_modbus_core.c ../src/app_modbus_core.c

$(BIN_TCP): test_app_modbus_tcp_frame.c ../src/app_modbus_tcp_frame.c ../src/app_modbus_core.c ../include/app_modbus_tcp_frame.h ../include/app_modbus_core.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_app_modbus_tcp_frame.c ../src/app_modbus_tcp_frame.c ../src/app_modbus_core.c

$(BIN_WELD): test_app_weld_fsm.c ../src/app_weld_fsm.c ../include/app_weld_fsm.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_app_weld_fsm.c ../src/app_weld_fsm.c

$(BIN_SR): test_app_seek_reset_fsm.c ../src/app_seek_reset_fsm.c ../include/app_seek_reset_fsm.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_app_seek_reset_fsm.c ../src/app_seek_reset_fsm.c

clean:
	rm -f $(BIN_REG) $(BIN_MB) $(BIN_TCP) $(BIN_WELD) $(BIN_SR)

.PHONY: test clean
```

- [ ] **Step 5: 빌드+테스트 통과 확인**

Run: `make -C fw/test clean test`
Expected: 5스위트 전부 `all passed` (신규 `test_app_seek_reset_fsm: all passed` 포함).

- [ ] **Step 6: Commit**

```bash
git add fw/include/app_seek_reset_fsm.h fw/src/app_seek_reset_fsm.c fw/test/test_app_seek_reset_fsm.c fw/test/Makefile
git commit -m "feat(seekreset): FSM 인터페이스 + 스캐폴드 + host-test 배선"
```

---

## Task 2: FSM 전이 로직 (자동 체인/단발/RUN 직교/busy) + host-test 6 시나리오

TDD — 6 시나리오 테스트를 먼저 작성(빈 step이라 FAIL) → 전이 로직 구현 → PASS.

**Files:**
- Modify: `fw/test/test_app_seek_reset_fsm.c`
- Modify: `fw/src/app_seek_reset_fsm.c`

- [ ] **Step 1: 6 시나리오 테스트 작성 (실패하는 테스트)**

Modify `fw/test/test_app_seek_reset_fsm.c` — `test_init_idle()` 아래, `main()` 위에 다음 테스트 함수들을 추가하고, `main()`에 호출을 추가한다.

테스트 함수 (추가):

```c
/* RESET 자동 체인: RESET → SR_RESET(reset_signal=1·reset_icon=1) → 50 tick 후
 * SR_SEEK(reset_signal=0·reset_icon_off=1·seek_signal=1·seek_icon=1) → 또 50 tick
 * 후 SR_IDLE(seek_signal=0·seek_icon_off=1). signal span 각 50 tick. spec §7-1. */
static void test_reset_auto_chain(void)
{
    seek_reset_fsm_init();
    seek_reset_in_t in = { .cmd = SR_CMD_RESET, .run_active = 0u };
    seek_reset_out_t out;

    /* 진입 tick: SR_RESET, reset on 엣지 */
    seek_reset_fsm_step(&in, &out);
    CHECK_EQ(out.state, SR_RESET);
    CHECK_EQ(out.reset_signal, 1);
    CHECK_EQ(out.reset_icon, 1);
    CHECK_EQ(out.seek_signal, 0);

    in.cmd = SR_CMD_NONE;
    int reset_sig_ticks = 1;      /* 진입 tick 1회 카운트 */
    int chain_at = -1;
    /* 체인 전이까지 step (진입 후 추가 step). */
    for (int i = 1; i < 60; i++) {
        seek_reset_fsm_step(&in, &out);
        if (out.state == SR_RESET) { reset_sig_ticks++; }
        if (out.reset_icon_off && out.seek_icon) {   /* 체인 전이 step */
            chain_at = i;
            CHECK_EQ(out.state, SR_SEEK);
            CHECK_EQ(out.reset_signal, 0);
            CHECK_EQ(out.seek_signal, 1);
            CHECK_EQ(out.seek_icon, 1);
            break;
        }
    }
    CHECK_EQ(reset_sig_ticks, 50);   /* reset_signal active span = 50 tick */
    CHECK_EQ(chain_at, 50);          /* 50번째 step(진입=0번째)에서 체인 */

    /* SEEK span: 체인 step에서 seek on. 50 tick 후 자동 해제. */
    int seek_sig_ticks = 1;          /* 체인 step의 seek_signal=1 카운트 */
    int release_at = -1;
    for (int i = 1; i < 60; i++) {
        seek_reset_fsm_step(&in, &out);
        if (out.state == SR_SEEK) { seek_sig_ticks++; }
        if (out.seek_icon_off) {     /* 해제 전이 step */
            release_at = i;
            CHECK_EQ(out.state, SR_IDLE);
            CHECK_EQ(out.seek_signal, 0);
            break;
        }
    }
    CHECK_EQ(seek_sig_ticks, 50);    /* seek_signal active span = 50 tick */
    CHECK_EQ(release_at, 50);
}

/* SEEK 단발(체인 없음): SEEK → SR_SEEK → 50 tick 후 SR_IDLE 직행. reset_signal
 * 내내 0. spec §7-2. */
static void test_seek_oneshot_no_chain(void)
{
    seek_reset_fsm_init();
    seek_reset_in_t in = { .cmd = SR_CMD_SEEK, .run_active = 0u };
    seek_reset_out_t out;

    seek_reset_fsm_step(&in, &out);
    CHECK_EQ(out.state, SR_SEEK);
    CHECK_EQ(out.seek_signal, 1);
    CHECK_EQ(out.seek_icon, 1);
    CHECK_EQ(out.reset_signal, 0);

    in.cmd = SR_CMD_NONE;
    int reset_seen = 0;
    int idle_at = -1;
    for (int i = 1; i < 60; i++) {
        seek_reset_fsm_step(&in, &out);
        if (out.reset_signal) { reset_seen = 1; }
        if (out.state == SR_IDLE) { idle_at = i; break; }
    }
    CHECK_EQ(reset_seen, 0);          /* RESET 신호 한 번도 안 뜸 (체인 없음) */
    CHECK_EQ(idle_at, 50);            /* 50번째 step에서 IDLE 직행 */
}

/* RUN 직교: run_active=1이면 RESET/SEEK 명령 무시 (SR_IDLE 유지, signal 0). spec §7-3. */
static void test_run_orthogonal(void)
{
    seek_reset_fsm_init();
    seek_reset_in_t in = { .cmd = SR_CMD_RESET, .run_active = 1u };
    seek_reset_out_t out;
    seek_reset_fsm_step(&in, &out);
    CHECK_EQ(out.state, SR_IDLE);
    CHECK_EQ(out.reset_signal, 0);
    CHECK_EQ(out.reset_icon, 0);

    in.cmd = SR_CMD_SEEK;
    seek_reset_fsm_step(&in, &out);
    CHECK_EQ(out.state, SR_IDLE);
    CHECK_EQ(out.seek_signal, 0);
}

/* busy 중 재명령 무시: SR_RESET 진행 중 SEEK/RESET 재주입 → 무시 (타이밍·전이 불변).
 * spec §7-4. */
static void test_busy_command_ignored(void)
{
    seek_reset_fsm_init();
    seek_reset_in_t in = { .cmd = SR_CMD_RESET, .run_active = 0u };
    seek_reset_out_t out;
    seek_reset_fsm_step(&in, &out);          /* 진입 SR_RESET */

    /* 진행 중 SEEK 재주입을 매 tick 시도 — 무시되어 RESET 타이밍 그대로 50에서 체인 */
    in.cmd = SR_CMD_SEEK;
    int chain_at = -1;
    for (int i = 1; i < 60; i++) {
        seek_reset_fsm_step(&in, &out);
        if (out.reset_icon_off && out.seek_icon) { chain_at = i; break; }
    }
    CHECK_EQ(chain_at, 50);                   /* 재명령 무시 → 정상 체인 타이밍 */
}

/* ICON 엣지 1-shot: 각 전이에서 icon on/off가 정확히 1 tick만 1. spec §7-5. */
static void test_icon_edges_single_shot(void)
{
    seek_reset_fsm_init();
    seek_reset_in_t in = { .cmd = SR_CMD_RESET, .run_active = 0u };
    seek_reset_out_t out;
    int reset_on = 0, reset_off = 0, seek_on = 0, seek_off = 0;

    seek_reset_fsm_step(&in, &out);           /* 진입 */
    if (out.reset_icon)     { reset_on++; }
    in.cmd = SR_CMD_NONE;
    for (int i = 1; i < 110; i++) {           /* 전체 체인(RESET 50 + SEEK 50)을 덮음 */
        seek_reset_fsm_step(&in, &out);
        if (out.reset_icon)     { reset_on++; }
        if (out.reset_icon_off) { reset_off++; }
        if (out.seek_icon)      { seek_on++; }
        if (out.seek_icon_off)  { seek_off++; }
    }
    CHECK_EQ(reset_on, 1);
    CHECK_EQ(reset_off, 1);
    CHECK_EQ(seek_on, 1);
    CHECK_EQ(seek_off, 1);
}

/* 타이밍 경계: 정확히 SR_TICKS(50) tick에 전이 (49 tick까진 유지). spec §7-6. */
static void test_timing_boundary(void)
{
    seek_reset_fsm_init();
    seek_reset_in_t in = { .cmd = SR_CMD_RESET, .run_active = 0u };
    seek_reset_out_t out;
    seek_reset_fsm_step(&in, &out);           /* 진입 (step 0) */
    in.cmd = SR_CMD_NONE;
    /* step 1..49: 아직 SR_RESET 유지 */
    for (int i = 1; i <= 49; i++) {
        seek_reset_fsm_step(&in, &out);
        CHECK_EQ(out.state, SR_RESET);
    }
    /* step 50: 체인 전이 → SR_SEEK */
    seek_reset_fsm_step(&in, &out);
    CHECK_EQ(out.state, SR_SEEK);
}
```

`main()`에 호출 추가 (`test_init_idle();` 다음):

```c
    test_init_idle();
    test_reset_auto_chain();
    test_seek_oneshot_no_chain();
    test_run_orthogonal();
    test_busy_command_ignored();
    test_icon_edges_single_shot();
    test_timing_boundary();
```

- [ ] **Step 2: 테스트 실패 확인**

Run: `make -C fw/test clean test`
Expected: `test_app_seek_reset_fsm`에서 다수 FAIL (빈 step이라 state가 IDLE에 머묾 — `out.state = SR_RESET, expected ...` 등). 나머지 4스위트는 PASS.

- [ ] **Step 3: FSM 전이 로직 구현**

Modify `fw/src/app_seek_reset_fsm.c` — `seek_reset_fsm_step`을 다음으로 교체 (spec §3.3):

```c
void seek_reset_fsm_step(const seek_reset_in_t *in, seek_reset_out_t *out)
{
    memset(out, 0, sizeof(*out));

    switch (s_state) {
    case SR_IDLE:
        if (in->run_active) {
            /* RUN 직교: RUN 중 SEEK/RESET 명령 무시 (spec §3.4). */
        } else if (in->cmd == SR_CMD_RESET) {
            s_state           = SR_RESET;
            s_elapsed         = 0u;
            out->reset_signal = 1u;
            out->reset_icon   = 1u;       /* on 엣지 */
        } else if (in->cmd == SR_CMD_SEEK) {
            s_state           = SR_SEEK;
            s_elapsed         = 0u;
            out->seek_signal  = 1u;
            out->seek_icon    = 1u;       /* on 엣지 */
        }
        break;

    case SR_RESET:                        /* cmd 무시 (busy) */
        if (s_elapsed < 0xFFFFu) {
            s_elapsed++;                  /* 포화 가드 */
        }
        if (s_elapsed >= SR_TICKS) {      /* 500ms 경과 → SEEK 자동 체인 (samd20 5395-5396) */
            out->reset_icon_off = 1u;     /* reset_signal=0 (memset), off 엣지 */
            out->seek_signal    = 1u;
            out->seek_icon      = 1u;     /* on 엣지 */
            s_state             = SR_SEEK;
            s_elapsed           = 0u;
        } else {
            out->reset_signal = 1u;       /* 레벨 유지 */
        }
        break;

    case SR_SEEK:                         /* cmd 무시 (busy) */
        if (s_elapsed < 0xFFFFu) {
            s_elapsed++;
        }
        if (s_elapsed >= SR_TICKS) {      /* 500ms 경과 → 자동 해제 (samd20 5403-5407) */
            out->seek_icon_off = 1u;      /* seek_signal=0 (memset), off 엣지 */
            s_state            = SR_IDLE;
            s_elapsed          = 0u;
        } else {
            out->seek_signal = 1u;        /* 레벨 유지 */
        }
        break;

    default:
        /* unreachable (s_state는 항상 SR_*); fail-safe (weld LOW-2 패턴). */
        s_state = SR_IDLE;
        break;
    }

    out->state = s_state;
}
```

`(void)in;` 줄은 제거(이제 `in` 사용).

- [ ] **Step 4: 테스트 통과 확인**

Run: `make -C fw/test clean test`
Expected: 5스위트 전부 `all passed` (`test_app_seek_reset_fsm: all passed` 포함).

- [ ] **Step 5: Commit**

```bash
git add fw/src/app_seek_reset_fsm.c fw/test/test_app_seek_reset_fsm.c
git commit -m "feat(seekreset): FSM 전이 로직 (자동 체인/단발/RUN직교/busy) + host-test 6"
```

---

## Task 3: 글루 + app_reg 위임/START guard + 슈퍼루프 배선

글루를 작성하고, `app_reg`의 no-op을 위임으로 교체 + START 직교 guard를 추가하고, 슈퍼루프/부팅에 배선한다. ARM 빌드 0-warning + 5스위트 host-test 통과가 게이트.

**Files:**
- Create: `fw/include/app_seek_reset.h`
- Create: `fw/src/app_seek_reset.c`
- Modify: `fw/src/app_reg.c:96-97` (START guard), `:139-146` (위임 + default 분리)
- Modify: `fw/src/app.c:83` 인근 (tick 배선)
- Modify: `fw/src/main.c:29` 인근 (init 배선)

- [ ] **Step 1: 글루 헤더 작성**

Create `fw/include/app_seek_reset.h`:

```c
/* fw/include/app_seek_reset.h — Stage SEEK/RESET 글루: 10ms마다 seek_reset_fsm_step
 * 구동, out 엣지를 물리 신호 hook stub(B-SEAM) + LCD ICON 렌더로 라우팅. app_reg
 * 의 SEEK/RESET no-op을 app_seek_reset_request() 위임으로 교체하고, START guard가
 * app_seek_reset_active()를 read-only 조회 (양방향 RUN 직교). */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "app_lcd.h"   /* us_cmd_t (US_CMD_SEEK/RESET) */

void app_seek_reset_init(void);                          /* boot: FSM reset + tick gate */
void app_seek_reset_tick(void);                          /* superloop: 10ms-gated step + effects */
void app_seek_reset_request(us_cmd_t cmd, uint8_t src);  /* app_reg_command 위임 (1-shot 래치) */
uint8_t app_seek_reset_active(void);                     /* 1 = FSM != SR_IDLE (START 직교 조회) */

/* 물리 OSC 신호 hook (stub) — reset/seek 명령선 레벨 엣지마다 1회.
 * which: SR_CMD_RESET=RESET선 / SR_CMD_SEEK=SEEK선. 본 스테이지: mon 로그만.
 * B-SEAM: CTRL_OSC* active-LOW 구동 (극성 미확인, spec §10-1). */
void app_seek_reset_hook_signal(uint8_t which, bool on);
```

- [ ] **Step 2: 글루 구현 작성**

Create `fw/src/app_seek_reset.c`:

```c
/* fw/src/app_seek_reset.c — SEEK/RESET 글루. 10ms-gated FSM advance; out 엣지를
 * 물리 신호 hook stub + LCD ICON 렌더로. 누산/주입 불필요 (weld energy와 달리
 * 순수 시간+명령 엣지 구동). spec §4. */
#include "app_seek_reset.h"
#include "app_seek_reset_fsm.h"
#include "app_reg.h"      /* app_reg_measure (us_run_status 조회) */
#include "dgus_lcd.h"     /* ICON_RESET, ICON_SEEK */
#include "sys_tick.h"
#include "mon.h"

/* app_lcd_icon(vp, on) 선언은 app_lcd.h (app_seek_reset.h가 include). */

#define SR_TICK_MS  10u   /* FSM tick cadence (weld 패턴) */

static uint32_t s_prev_ms;
static uint8_t  s_pending_cmd;   /* SR_CMD_* one-shot 래치 (다음 tick 소비) */

void app_seek_reset_init(void)
{
    seek_reset_fsm_init();
    s_prev_ms     = sys_tick_get_ms();
    s_pending_cmd = SR_CMD_NONE;
}

void app_seek_reset_request(us_cmd_t cmd, uint8_t src)
{
    /* src는 추적용 — 본 스테이지 미사용 (samd20 comm RESET이 src=US_TOUCH로
     * 마킹하고 에러 표시를 클리어하는 quirk는 에러 머신 슬라이스, app_reg.c:144). */
    (void)src;
    if (cmd == US_CMD_RESET) {
        s_pending_cmd = SR_CMD_RESET;
    } else if (cmd == US_CMD_SEEK) {
        s_pending_cmd = SR_CMD_SEEK;
    }
}

uint8_t app_seek_reset_active(void)
{
    return (uint8_t)(seek_reset_fsm_state() != SR_IDLE);
}

void app_seek_reset_hook_signal(uint8_t which, bool on)
{
    /* 물리 OSC 구동 stub (B-SEAM). 실제 CTRL_OSC* 핀 구동은 reset_signal/
     * seek_signal 레벨로 후속 바인딩. */
    mon_printf("[sr] %s signal %s\r\n",
               (which == SR_CMD_RESET) ? "RESET" : "SEEK", on ? "ON" : "OFF");
}

void app_seek_reset_tick(void)
{
    uint32_t now = sys_tick_get_ms();
    if ((uint32_t)(now - s_prev_ms) < SR_TICK_MS) {
        return;
    }
    s_prev_ms = now;

    seek_reset_in_t in = {
        .cmd        = s_pending_cmd,
        .run_active = (uint8_t)(app_reg_measure()->us_run_status != (uint8_t)US_IDLE),
    };
    /* one-shot 소비: 매 tick 클리어 (무시돼도 드롭, weld s_start_pending 패턴). */
    s_pending_cmd = SR_CMD_NONE;

    seek_reset_out_t out;
    seek_reset_fsm_step(&in, &out);

    /* signal 레벨 엣지(0↔1)는 icon on/off 엣지와 동일 step에 발생하므로 icon
     * 엣지 1-shot을 hook + LCD ICON 라우팅에 함께 사용. reset_signal/seek_signal
     * 레벨 필드는 host-test span 검증 + B-SEAM 실 핀 구동용으로 유지. */
    if (out.reset_icon)     { app_seek_reset_hook_signal(SR_CMD_RESET, true);  app_lcd_icon(ICON_RESET, true);  }
    if (out.reset_icon_off) { app_seek_reset_hook_signal(SR_CMD_RESET, false); app_lcd_icon(ICON_RESET, false); }
    if (out.seek_icon)      { app_seek_reset_hook_signal(SR_CMD_SEEK,  true);  app_lcd_icon(ICON_SEEK,  true);  }
    if (out.seek_icon_off)  { app_seek_reset_hook_signal(SR_CMD_SEEK,  false); app_lcd_icon(ICON_SEEK,  false); }
}
```

- [ ] **Step 3: app_reg.c — SEEK/RESET 위임 + default 분리**

Modify `fw/src/app_reg.c`. 먼저 파일 상단 include 블록에 추가 (기존 `#include` 들 근처):

```c
#include "app_seek_reset.h"   /* app_seek_reset_request, app_seek_reset_active */
```

그리고 `app_reg_command`의 `case US_CMD_SEEK: case US_CMD_RESET: default:` 병합 블록(현 139-146)을 분리:

```c
    case US_CMD_SEEK:
    case US_CMD_RESET:
        /* SEEK/RESET 효과를 app_seek_reset FSM에 위임 (이전 no-op 교체, spec §4).
         * RUN 중이면 FSM이 run_active 직교로 자체 무시. samd20 comm RESET src
         * quirk + 에러 표시 클리어는 입력 레이어(app_lcd_input.c)/에러 머신. */
        app_seek_reset_request(cmd, src);
        break;
    default:
        /* alien cmd 흡수 (no-op). */
        break;
```

- [ ] **Step 4: app_reg.c — START guard에 active 조회 추가 (swallow-safe)**

Modify `fw/src/app_reg.c`. ⚠ **단순 `&& app_seek_reset_active()==0`을 if 조건에 넣지 말 것** — 그러면 swallow-consume 블록(98-106)까지 스킵되어, RUN→ceiling-stop(swallow_start=1)→즉시 SEEK/RESET 사이에 도착한 V30 release-as-START가 삼켜지지 않고 잔존, SEEK/RESET 종료 후 다음 genuine press를 드롭함(advisor). 직교의 의미는 "새 RUN 시작 거부"이지 "release 페어링 동기화 파괴"가 아니므로, **swallow 처리 뒤 별도 break**로 분리한다.

현재(app_reg.c:96-107):

```c
        if ((g_reg.main_state == 0u) &&
            (g_reg.us_run_status == (uint8_t)US_IDLE)) {
            if ((src == (uint8_t)US_TOUCH) && (g_reg.swallow_start != 0u)) {
                /* V30 RUN button sends data=0 on BOTH edges: ... */
                g_reg.swallow_start = 0u;
                break;
            }
            g_reg.us_run_status = src;   /* US_TOUCH or US_COMM */
```

다음으로 교체 (기존 swallow 블록은 그대로 두고, 그 **뒤** + `g_reg.us_run_status = src;` **앞**에 active break 삽입):

```c
        if ((g_reg.main_state == 0u) &&
            (g_reg.us_run_status == (uint8_t)US_IDLE)) {
            if ((src == (uint8_t)US_TOUCH) && (g_reg.swallow_start != 0u)) {
                /* V30 RUN button sends data=0 on BOTH edges: ... (기존 주석 유지) */
                g_reg.swallow_start = 0u;
                break;
            }
            /* SEEK/RESET active 중 START 무시 (spec §3.4). 직교는 새 RUN 시작만
             * 막고, 위 swallow_start 페어링 동기화는 건드리지 않음 (advisor —
             * guard를 if 조건에 합치면 swallow consume도 스킵되는 비대칭 발생). */
            if (app_seek_reset_active() != 0u) {
                break;
            }
            g_reg.us_run_status = src;   /* US_TOUCH or US_COMM */
```

> ⚠ 기존 swallow 블록의 V30 주석 본문(99-103)은 손대지 않는다 — 위 발췌의 `...`는 기존 주석 생략 표기일 뿐, 실제로는 원본 주석을 그대로 유지.

- [ ] **Step 5: app.c — 슈퍼루프 tick 배선**

Modify `fw/src/app.c`. 파일 상단 include에 추가 (`#include "app_weld.h"` 근처):

```c
#include "app_seek_reset.h"
```

그리고 `app_loop_iter`의 `app_weld_tick();`(line 83) 바로 다음에 추가:

```c
    /* 2.6 SEEK/RESET FSM — 10 ms cadence. run_active(us_run_status)를 읽어 RUN
     * 직교; ICON/hook만 emit (app_reg에 명령 안 보냄)이라 reg_tick 앞/뒤 무관 —
     * weld 패턴 일관성 위해 weld_tick 다음에 배치 (1-iter stale run_active 무해). */
    app_seek_reset_tick();
```

- [ ] **Step 6: main.c — boot init 배선**

Modify `fw/src/main.c`. 상단 include에 추가 (`#include "app_weld.h"` 근처, line 6):

```c
#include "app_seek_reset.h"
```

그리고 `app_weld_init();`(line 29) 바로 다음에 추가:

```c
    app_seek_reset_init();   /* Stage SEEK/RESET: FSM reset (needs sys_tick up) */
```

- [ ] **Step 7: ARM 빌드 0-warning 확인 (GLOB 재구성)**

새 `.c` 파일이 `file(GLOB)`에 잡히도록 build를 재구성한 뒤 빌드한다.

Run:
```bash
cd fw && env -u STM32_TOOLCHAIN cmake -B build -G Ninja && env -u STM32_TOOLCHAIN cmake --build build 2>&1 | grep -iE "warning|error" | grep -v "vendor/\|wiznet\|WIZnet" || echo "0 warnings (our code)"
```
Expected: `0 warnings (our code)` (vendor wiznet 기존 경고는 무관). 빌드 성공(`.elf` 생성).

- [ ] **Step 8: host-test 5스위트 통과 확인**

Run: `make -C fw/test clean test`
Expected: 5스위트 전부 `all passed`.

- [ ] **Step 9: Commit**

```bash
git add fw/include/app_seek_reset.h fw/src/app_seek_reset.c fw/src/app_reg.c fw/src/app.c fw/src/main.c
git commit -m "feat(seekreset): 글루 + app_reg 위임/START 직교 guard + 슈퍼루프 배선"
```

---

## 검증 게이트 (spec §9)

- **호스트(주 게이트):** ARM 빌드 0-warning(우리 코드) + `make -C fw/test test` 5스위트 PASS (기존 4 + `app_seek_reset_fsm`).
- **HW(보드, 회귀 + ICON — 본 plan 범위 밖, 머지 전 별도 세션):**
  1. 기존 직접-초음파/weld 무회귀
  2. 패널 RESET 버튼 → ICON_RESET 점등 →(500ms)→ ICON_SEEK 점등 →(500ms)→ 소등 (자동 체인 시각 확인)
  3. 패널 SEEK 버튼 → ICON_SEEK 단발
  4. Modbus reg(SEEK/RESET 경로) 동일
  5. RUN 중 SEEK/RESET 무시 + SEEK/RESET 중 START 무시
- **물리 OSC 효과(주파수 탐색/리셋)는 B-SEAM/6b** (실 초음파 rig + 스코프). 본 스테이지 HW는 ICON·상태·직교만.
- **머지:** `--no-ff` 로컬, 태그 `hw-revA_fw-stage-seekreset` (host + HW ICON/직교 verified; 물리 OSC E2E는 B-SEAM). ⚠ **브랜치 stack**: seek-reset 머지는 weld slice3 머지(`hw-revA_fw-stage-weld3`) 후.

---

## cpp-review 포커스 (Task 3 통합 리뷰 브리프 — advisor)

본 plan의 FSM은 host-test로 6중 검증되지만, **`app_reg.c` START-guard diff가 host-test 커버리지 0인 유일한 behavioral change이며 HW-verified RUN-path 위에 있다**. weld LCD clamp 교훈("review + HW이 유일한 게이트")과 동형 — cpp-review 브리프는 신규 FSM이 아니라 **guard diff를 핵심 회귀 위험**으로 다룰 것:
1. **swallow_start 비대칭 (Step 4)** — guard가 swallow-consume(98-106)을 스킵하지 않고 START 시작만 막는지(별도 break 위치), RUN_RELEASE swallow(131-137)는 ungated 유지가 맞는지 확인.
2. **guard 회귀** — 정상 경로(SEEK/RESET inactive)에서 기존 START/ceiling/swallow 동작 byte-equivalent인지.
3. **순환 의존** — `app_seek_reset_active()`가 부작용 없는 read-only인지, `app_reg.c`↔`app_seek_reset.c` 상호 include가 헤더 순환이 아닌 함수 선언 참조인지.

## 구현 노트 & deferred (advisor 검토 반영)

1. **weld-START 상호작용 (슬라이스4 의식적 결정 필요)** — Step 4의 guard는 패널/Modbus START뿐 아니라 `app_weld.c`의 `app_reg_command(US_CMD_START, US_CYCLE)`(WELD 진입)도 게이팅한다. **본 슬라이스는 weld dormant(프로덕션 트리거 없음)라 inert** — acceptable. 그러나 **슬라이스4**(물리 SW_START)에서 SEEK/RESET active 중 물리 트리거가 들어오면 weld FSM은 기계적으로 전진(READY→CYL1→…)하면서 US START는 거부 → **초음파 없는 실린더 사이클**. 의도된 직교일 수도 있으나, 슬라이스4 진입 시 명시적으로 결정할 것(현재는 사고가 아닌 기록된 deferred). 후보: weld FSM 진입 자체를 `app_seek_reset_active()`로 게이트, 또는 SEEK/RESET을 weld 사이클 중 무시.
2. **B-SEAM 레벨 필드 핸드오프 경고** — `seek_reset_out_t.reset_signal`/`seek_signal` 레벨 필드는 본 스테이지 글루가 **읽지 않는다**(엣지 기반 icon/hook 라우팅 사용, signal 엣지 == icon 엣지 동치). B-SEAM이 실 CTRL_OSC* 핀을 구동할 때 **레벨-read와 엣지-기반 라우팅을 혼용하지 말 것** — 한 방식으로 통일(레벨 폴링 또는 엣지 이벤트). 혼용 시 중복/누락 구동 위험.

---

## Self-Review (작성자 체크)

**1. Spec coverage:**
- §1.2 순수 FSM 코어 → Task 1(인터페이스) + Task 2(로직) ✅
- §1.2 글루 → Task 3 ✅
- §1.2 양방향 RUN 직교 → Task 2(FSM run_active) + Task 3 Step 4(START guard) ✅
- §1.2 ICON 엣지 렌더 → Task 3 Step 2(`app_lcd_icon` 라우팅) ✅
- §1.2 물리 OSC hook stub → Task 3 Step 2(`app_seek_reset_hook_signal`) ✅
- §3.3 전이(체인/단발/busy) → Task 2 Step 3 ✅
- §7 host 테스트 6 → Task 2 Step 1 (체인/단발/RUN직교/busy/ICON엣지/타이밍경계) ✅
- §4 cmd 큐잉(1-shot 래치) → Task 3 Step 2(`s_pending_cmd`) ✅
- §10-4 ICON 헬퍼 → 기존 `app_lcd_icon` 사용 (신설 불요, plan 헤더에 기록) ✅

**2. Placeholder scan:** 모든 step에 실제 코드/명령 포함. TBD/TODO 없음. ✅

**3. Type consistency:**
- `seek_reset_in_t{cmd, run_active}` / `seek_reset_out_t{state, reset_signal, seek_signal, reset_icon, reset_icon_off, seek_icon, seek_icon_off}` — Task 1 헤더 정의와 Task 2 테스트/구현, Task 3 글루 사용처 전부 일치 ✅
- `seek_reset_fsm_init/step/state`, `app_seek_reset_init/tick/request/active/hook_signal` — 선언/정의/호출 시그니처 일치 ✅
- `SR_TICKS=50`, `SR_CMD_*`, `SR_IDLE/RESET/SEEK` — 일관 ✅
- `app_lcd_icon(uint16_t, bool)` — `app_lcd_disp.c:250` 실제 시그니처와 일치 ✅
- `app_reg_measure()->us_run_status` — `lcd_measure_t` 필드 존재 확인됨(app_lcd.h:122) ✅

*작성: 2026-06-17 SEEK/RESET writing-plans. weld 슬라이스 패턴 동형. 다음 = subagent-driven-development (Task 1부터).*
