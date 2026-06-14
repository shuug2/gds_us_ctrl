# Weld-Cycle Slice 1 (Core FSM, DELAY mode) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** samd20 공압 프레스 weld-cycle FSM(READY→CYL1→WELD→HOLD→CYL2→work_cnt++)의 DELAY 모드를 HAL-free 코어 + 글루로 STM32에 포팅한다.

**Architecture:** 순수 FSM 코어(`app_weld_fsm.c`, host-test)가 10ms마다 상태를 전진시키고 out-events(SOL_DN·weld_start/stop·cycle_done)를 낸다. 글루(`app_weld.c`)가 live config로 코어를 구동하고 out-events를 SOL_DN hook·`app_reg` US_CYCLE 명령·work_cnt 영속으로 변환한다. 프로덕션 트리거(물리 SW_START1/2)는 슬라이스4 — 슬라이스1은 host-test 검증, HW는 회귀확인.

**Tech Stack:** C11, arm-none-eabi-gcc(펌웨어) / host cc(테스트), CMake `file(GLOB src/*.c)`(신규 .c 자동 포함), 기존 `app_reg`/`app_lcd`/`app_config` API.

**Spec:** `docs/superpowers/specs/2026-06-14-stage-weld-cycle-slice1-design.md`

**Branch:** `feat/stage-weld-cycle-slice1` (이미 생성, spec 커밋됨)

**시간 단위:** 10ms/count. **충실도:** 혼합(거동 samd20 충실 + 진폭 언더플로 가드 1건 수정).

**전역 확인(매 커밋 전):** `make -C fw/test test` → 모든 스위트 PASS. 펌웨어 빌드 게이트는 Task 4·6.

---

### Task 1: FSM 코어 스캐폴드 + Makefile + init 테스트

**Files:**
- Create: `fw/include/app_weld_fsm.h`
- Create: `fw/src/app_weld_fsm.c`
- Create: `fw/test/test_app_weld_fsm.c`
- Modify: `fw/test/Makefile`

- [ ] **Step 1: 헤더 작성** — `fw/include/app_weld_fsm.h`

```c
/* fw/include/app_weld_fsm.h — Stage Weld-Cycle slice 1: HAL-free pure FSM core
 * for the samd20 pneumatic press weld cycle (READY→CYL1→WELD→HOLD→CYL2). DELAY
 * mode only this slice (time-based, 10ms/count); energy_ctrl/multi_ctrl/TRIGGER
 * = later slices. No HAL, no globals reached — host-tested (fw/test). The glue
 * (app_weld.c) calls weld_fsm_step() every 10ms and turns out-events into hooks
 * + app_reg US_CYCLE commands + work_cnt. */
#pragma once
#include <stdint.h>

/* run_status (samd20 main.c verbatim: RUN_READY..RUN_CYL2) */
enum {
    WELD_READY = 0,
    WELD_CYL1  = 1,
    WELD_WELD  = 2,
    WELD_HOLD  = 3,
    WELD_CYL2  = 4,
};

/* comp_time "no reduction" sentinel (samd20: comp_time==7 => full amplitude). */
#define WELD_COMP_FULL  7u

/* step input — caller injects live config each call (app_reg limit_on_time
 * injection pattern; no call back into app_lcd). run_mode is carried for the
 * slice-4 TRIGGER branch; slice 1 evaluates DELAY transitions only. */
typedef struct {
    uint8_t  start;              /* 1 = start trigger (consumed only in READY) */
    uint8_t  run_mode;           /* slice-4 (TRIGGER); slice 1 ignores */
    uint16_t limit_delay_time1;  /* CYL1 & CYL2 duration (x10 ms) */
    uint16_t limit_delay_time2;  /* WELD duration (x10 ms) */
    uint16_t limit_delay_time3;  /* HOLD duration (x10 ms) */
    uint8_t  output_power;       /* amplitude base, 50..100 (clamped upstream) */
} weld_in_t;

/* step output — edge flags (weld_start/weld_stop/cycle_done) are 1 for exactly
 * the one step they fire; sol_dn/run_status are levels. */
typedef struct {
    uint8_t  run_status;   /* current state (WELD_*) */
    uint8_t  sol_dn;       /* solenoid-down request level (1=ON, 0=OFF) */
    uint8_t  weld_start;   /* 1 = WELD entry edge: glue does US_CYCLE START + pot */
    uint8_t  weld_stop;    /* 1 = WELD exit edge: glue does US_CYCLE RUN_RELEASE */
    uint8_t  amplitude;    /* valid when weld_start: comp_time-corrected pot 0..127 */
    uint8_t  cycle_done;   /* 1 = CYL2 completion edge: glue does work_cnt++ */
} weld_out_t;

void weld_fsm_init(void);                                 /* reset to READY (boot) */
void weld_fsm_step(const weld_in_t *in, weld_out_t *out); /* one 10 ms tick */
uint8_t weld_fsm_status(void);                            /* current run_status */
```

- [ ] **Step 2: 스켈레톤 .c 작성** — `fw/src/app_weld_fsm.c`

```c
/* fw/src/app_weld_fsm.c — weld-cycle FSM core (slice 1, DELAY mode). HAL-free. */
#include "app_weld_fsm.h"
#include <string.h>

static uint8_t  s_run_status;
static uint8_t  s_f_status_start;   /* per-state "first entry done" flag */
static uint16_t s_temp_time;        /* active countdown (10ms/count) */
static uint16_t s_comp_time;        /* weld amplitude-comp counter (samd20) */
static uint8_t  s_sol_dn;           /* solenoid level held across steps */

void weld_fsm_init(void)
{
    s_run_status     = WELD_READY;
    s_f_status_start = 0u;
    s_temp_time      = 0u;
    s_comp_time      = WELD_COMP_FULL;
    s_sol_dn         = 0u;
}

uint8_t weld_fsm_status(void)
{
    return s_run_status;
}

void weld_fsm_step(const weld_in_t *in, weld_out_t *out)
{
    (void)in;
    memset(out, 0, sizeof(*out));
    out->run_status = s_run_status;
    out->sol_dn     = s_sol_dn;
}
```

- [ ] **Step 3: 테스트 파일 작성(init)** — `fw/test/test_app_weld_fsm.c`

```c
/* fw/test/test_app_weld_fsm.c — host unit tests for the pure weld-cycle FSM
 * core (slice 1, DELAY mode). No HAL, no hardware. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "app_weld_fsm.h"

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

static void test_init_ready(void)
{
    weld_fsm_init();
    CHECK_EQ(weld_fsm_status(), WELD_READY);
}

int main(void)
{
    test_init_ready();
    if (failures) { printf("test_app_weld_fsm: %d FAILED\n", failures); return 1; }
    printf("test_app_weld_fsm: all passed\n");
    return 0;
}
```

- [ ] **Step 4: Makefile에 weld 스위트 추가** — `fw/test/Makefile`

`BIN_TCP := ...` 줄 아래에 추가:
```make
BIN_WELD := /tmp/gds_test_app_weld_fsm
```
`test:` 타겟을 교체:
```make
test: $(BIN_REG) $(BIN_MB) $(BIN_TCP) $(BIN_WELD)
	$(BIN_REG)
	$(BIN_MB)
	$(BIN_TCP)
	$(BIN_WELD)
```
`$(BIN_TCP): ...` 규칙 아래에 추가:
```make
$(BIN_WELD): test_app_weld_fsm.c ../src/app_weld_fsm.c ../include/app_weld_fsm.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_app_weld_fsm.c ../src/app_weld_fsm.c
```
`clean:` 의 `rm -f` 줄에 `$(BIN_WELD)` 추가:
```make
	rm -f $(BIN_REG) $(BIN_MB) $(BIN_TCP) $(BIN_WELD)
```

- [ ] **Step 5: 테스트 실행 — PASS 확인**

Run: `make -C fw/test test`
Expected: `test_app_weld_fsm: all passed` + 다른 3 스위트도 PASS (0 warning).

- [ ] **Step 6: 커밋**

```bash
git add fw/include/app_weld_fsm.h fw/src/app_weld_fsm.c fw/test/test_app_weld_fsm.c fw/test/Makefile
git commit -m "feat(weld): FSM core scaffold + host test harness

- app_weld_fsm.{h,c} 인터페이스 + 스켈레톤(init/status), step은 스텁
- fw/test/test_app_weld_fsm.c + Makefile BIN_WELD 추가
- init -> READY 검증"
```

---

### Task 2: FSM 전체 step() 구현 + 완전 사이클·타이밍 테스트

**Files:**
- Modify: `fw/src/app_weld_fsm.c` (step 스텁 → 전체 구현)
- Modify: `fw/test/test_app_weld_fsm.c` (사이클/타이밍 테스트 추가)

- [ ] **Step 1: 실패 테스트 작성(완전 사이클 + 타이밍)** — `test_app_weld_fsm.c`의 `test_init_ready` 아래에 추가

```c
/* 한 사이클을 구동하며 상태별 step 수 + 이벤트를 집계 (DELAY 모드). */
typedef struct {
    int cyl1_steps, weld_steps, hold_steps, cyl2_steps;
    int weld_start_cnt, weld_stop_cnt, cycle_done_cnt;
    int sol_on_edges, sol_off_edges;
    uint8_t weld_amp;
    int prev_sol;
} trace_t;

static void run_cycle(const weld_in_t *base, trace_t *t, int max_steps)
{
    memset(t, 0, sizeof(*t));
    weld_in_t in = *base;
    in.start = 1u;                 /* trigger on the first step */
    for (int i = 0; i < max_steps; i++) {
        weld_out_t out;
        weld_fsm_step(&in, &out);
        in.start = 0u;             /* one-shot */
        switch (out.run_status) {
            case WELD_CYL1: t->cyl1_steps++; break;
            case WELD_WELD: t->weld_steps++; break;
            case WELD_HOLD: t->hold_steps++; break;
            case WELD_CYL2: t->cyl2_steps++; break;
            default: break;
        }
        if (out.weld_start) { t->weld_start_cnt++; t->weld_amp = out.amplitude; }
        if (out.weld_stop)  { t->weld_stop_cnt++; }
        if (out.cycle_done) { t->cycle_done_cnt++; }
        if (out.sol_dn && !t->prev_sol)  { t->sol_on_edges++; }
        if (!out.sol_dn && t->prev_sol)  { t->sol_off_edges++; }
        t->prev_sol = out.sol_dn;
        if (out.cycle_done) { break; }
    }
}

/* 완전 DELAY 사이클: 이벤트가 정확히 1회씩, SOL on→off, READY 복귀. */
static void test_full_cycle_delay(void)
{
    weld_fsm_init();
    weld_in_t in = { .start=0u, .run_mode=0u,
                     .limit_delay_time1=3u, .limit_delay_time2=10u,
                     .limit_delay_time3=2u, .output_power=100u };
    trace_t t;
    run_cycle(&in, &t, 200);
    CHECK_EQ(t.weld_start_cnt, 1);
    CHECK_EQ(t.weld_stop_cnt, 1);
    CHECK_EQ(t.cycle_done_cnt, 1);
    CHECK_EQ(t.sol_on_edges, 1);
    CHECK_EQ(t.sol_off_edges, 1);
    CHECK_EQ(weld_fsm_status(), WELD_READY);   /* 사이클 후 READY 복귀 */
}

/* 각 상태의 점유 step 수 = 해당 limit (out.run_status 기준). */
static void test_timing_durations(void)
{
    weld_fsm_init();
    weld_in_t in = { .start=0u, .run_mode=0u,
                     .limit_delay_time1=5u, .limit_delay_time2=8u,
                     .limit_delay_time3=4u, .output_power=100u };
    trace_t t;
    run_cycle(&in, &t, 200);
    CHECK_EQ(t.cyl1_steps, 5);   /* limit_delay_time1 */
    CHECK_EQ(t.weld_steps, 8);   /* limit_delay_time2 */
    CHECK_EQ(t.hold_steps, 4);   /* limit_delay_time3 */
    CHECK_EQ(t.cyl2_steps, 5);   /* limit_delay_time1 */
}
```
그리고 `main()`에 호출 추가(`test_init_ready();` 아래):
```c
    test_full_cycle_delay();
    test_timing_durations();
```

- [ ] **Step 2: 실패 확인**

Run: `make -C fw/test test`
Expected: FAIL — `weld_start_cnt = 0, expected 1` 등 (step이 아직 스텁).

- [ ] **Step 3: 전체 step() 구현** — `fw/src/app_weld_fsm.c`의 `weld_fsm_step`를 교체

```c
/* slice-1 진폭: base만 (comp_time 보정은 Task 3). output_power 50..100 계약. */
static uint8_t weld_amplitude(uint8_t output_power, uint16_t comp_time)
{
    (void)comp_time;
    return (uint8_t)(((uint16_t)(output_power - 50u) * 255u) / 100u);
}

void weld_fsm_step(const weld_in_t *in, weld_out_t *out)
{
    memset(out, 0, sizeof(*out));

    /* 10 ms elapsed: active timer counts down (samd20 timer temp_time--). */
    if (s_temp_time > 0u) {
        s_temp_time--;
    }

    switch (s_run_status) {
    case WELD_READY:
        if (in->start) {
            s_run_status     = WELD_CYL1;
            s_temp_time      = in->limit_delay_time1;
            s_f_status_start = 0u;       /* CYL1 first-entry drives SOL_DN ON */
        }
        break;

    case WELD_CYL1:
        if (s_f_status_start == 0u) {
            s_f_status_start = 1u;
            s_sol_dn         = 1u;       /* SOL_DN ON (cylinder descends) */
        } else if (s_temp_time == 0u) {
            s_f_status_start = 0u;
            s_run_status     = WELD_WELD;
            if (in->limit_delay_time2 > 6u) {           /* samd20 main.c:1504 */
                s_temp_time = in->limit_delay_time2;
                s_comp_time = WELD_COMP_FULL;
            } else {
                s_comp_time = in->limit_delay_time2;
                s_temp_time = WELD_COMP_FULL;
            }
        }
        break;

    case WELD_WELD:
        if (s_f_status_start == 0u) {
            s_f_status_start = 1u;
            out->amplitude   = weld_amplitude(in->output_power, s_comp_time);
            out->weld_start  = 1u;       /* glue: US_CYCLE START + pot write */
        } else if (s_temp_time == 0u) {
            out->weld_stop   = 1u;       /* glue: US_CYCLE RUN_RELEASE */
            s_f_status_start = 0u;
            s_run_status     = WELD_HOLD;
            s_temp_time      = in->limit_delay_time3;
        }
        break;

    case WELD_HOLD:
        if (s_f_status_start == 0u) {
            s_f_status_start = 1u;
        } else if (s_temp_time == 0u) {
            s_f_status_start = 0u;
            s_run_status     = WELD_CYL2;
            s_temp_time      = in->limit_delay_time1;
        }
        break;

    case WELD_CYL2:
        if (s_f_status_start == 0u) {
            s_f_status_start = 1u;
            s_sol_dn         = 0u;       /* SOL_DN OFF (cylinder rises) */
        } else if (s_temp_time == 0u) {
            s_f_status_start = 0u;
            s_run_status     = WELD_READY;
            out->cycle_done  = 1u;       /* glue: work_cnt++ */
        }
        break;

    default:
        s_run_status = WELD_READY;
        break;
    }

    out->run_status = s_run_status;
    out->sol_dn     = s_sol_dn;
}
```

- [ ] **Step 4: 테스트 PASS 확인**

Run: `make -C fw/test test`
Expected: `test_app_weld_fsm: all passed` (사이클/타이밍 포함).

- [ ] **Step 5: 커밋**

```bash
git add fw/src/app_weld_fsm.c fw/test/test_app_weld_fsm.c
git commit -m "feat(weld): FSM step() 전체 구현 (DELAY 5상태) + 사이클/타이밍 테스트

- READY→CYL1→WELD→HOLD→CYL2→READY, temp_time 10ms 카운트다운
- SOL_DN on(CYL1)/off(CYL2), weld_start/stop 엣지, cycle_done 엣지
- 진폭 base만(comp_time 보정은 다음 커밋); 상태별 점유=limit 검증"
```

---

### Task 3: comp_time 진폭보정 + 언더플로 가드 + 엣지/start 테스트

**Files:**
- Modify: `fw/src/app_weld_fsm.c` (`weld_amplitude`에 comp_time 보정 + 가드)
- Modify: `fw/test/test_app_weld_fsm.c` (진폭/quirk/엣지 테스트)

- [ ] **Step 1: 실패 테스트 작성** — `test_timing_durations` 아래에 추가

```c
/* 진폭 base: output_power 50/75/100 -> 0/63/127 (comp_time=7, ldt2>6). */
static void test_amplitude_values(void)
{
    uint8_t op[3]  = { 50u, 75u, 100u };
    uint8_t exp[3] = { 0u,  63u, 127u };
    for (int k = 0; k < 3; k++) {
        weld_fsm_init();
        weld_in_t in = { .start=0u, .run_mode=0u,
                         .limit_delay_time1=2u, .limit_delay_time2=10u,
                         .limit_delay_time3=2u, .output_power=op[k] };
        trace_t t;
        run_cycle(&in, &t, 200);
        CHECK_EQ(t.weld_amp, exp[k]);
    }
}

/* comp_time quirk: ldt2<=6 -> WELD가 7 step, 진폭 -= (7-ldt2)*10.
 * ldt2=3, op=100 -> base 127, comp_time=3, 127-(7-3)*10 = 87. */
static void test_comp_time_reduction(void)
{
    weld_fsm_init();
    weld_in_t in = { .start=0u, .run_mode=0u,
                     .limit_delay_time1=2u, .limit_delay_time2=3u,
                     .limit_delay_time3=2u, .output_power=100u };
    trace_t t;
    run_cycle(&in, &t, 200);
    CHECK_EQ(t.weld_steps, 7);    /* 짧은 용접 -> 최소 7 step */
    CHECK_EQ(t.weld_amp, 87u);    /* 127 - 40 */
}

/* 언더플로 가드: ldt2=0 -> comp_time=0, 감쇠 70; op=50 -> base 0 -> 0 (wrap 금지). */
static void test_amplitude_underflow_guard(void)
{
    weld_fsm_init();
    weld_in_t in = { .start=0u, .run_mode=0u,
                     .limit_delay_time1=2u, .limit_delay_time2=0u,
                     .limit_delay_time3=2u, .output_power=50u };
    trace_t t;
    run_cycle(&in, &t, 200);
    CHECK_EQ(t.weld_amp, 0u);     /* 0 - 70 -> 0 (언더플로 가드) */
}

/* READY 밖 start 무시: 사이클 도중 start=1 재주입해도 두 번째 weld_start 없음. */
static void test_start_ignored_outside_ready(void)
{
    weld_fsm_init();
    weld_in_t in = { .start=1u, .run_mode=0u,
                     .limit_delay_time1=3u, .limit_delay_time2=10u,
                     .limit_delay_time3=3u, .output_power=100u };
    int weld_starts = 0;
    for (int i = 0; i < 200; i++) {
        weld_out_t out;
        weld_fsm_step(&in, &out);
        in.start = 1u;            /* 매 step start 유지 — READY 밖에선 무시돼야 */
        if (out.weld_start) { weld_starts++; }
        if (out.cycle_done) { break; }
    }
    CHECK_EQ(weld_starts, 1);     /* 사이클당 정확히 1회 */
}

/* cycle_done은 1-step 엣지: READY 복귀 후 start=0 step들에서 재발화 없음. */
static void test_cycle_done_single_edge(void)
{
    weld_fsm_init();
    weld_in_t in = { .start=0u, .run_mode=0u,
                     .limit_delay_time1=2u, .limit_delay_time2=4u,
                     .limit_delay_time3=2u, .output_power=100u };
    trace_t t;
    run_cycle(&in, &t, 200);
    CHECK_EQ(t.cycle_done_cnt, 1);
    /* READY 복귀 후 추가 step (start=0) -> cycle_done 0 유지 */
    int extra = 0;
    in.start = 0u;
    for (int i = 0; i < 20; i++) {
        weld_out_t out;
        weld_fsm_step(&in, &out);
        if (out.cycle_done) { extra++; }
    }
    CHECK_EQ(extra, 0);
}
```
`main()`에 호출 추가:
```c
    test_amplitude_values();
    test_comp_time_reduction();
    test_amplitude_underflow_guard();
    test_start_ignored_outside_ready();
    test_cycle_done_single_edge();
```

- [ ] **Step 2: 실패 확인**

Run: `make -C fw/test test`
Expected: FAIL — `test_comp_time_reduction`의 `weld_amp = 127, expected 87` (보정 미구현), `test_amplitude_underflow_guard`도 FAIL.
(`test_amplitude_values`/`test_start_ignored`/`test_cycle_done_single_edge`는 Task 2 구현으로 이미 PASS.)

- [ ] **Step 3: weld_amplitude에 comp_time 보정 + 가드 구현** — `fw/src/app_weld_fsm.c`

`weld_amplitude` 함수를 교체:
```c
/* slice-1 진폭: base = ((op-50)*255)/100 (op 50..100 -> 0..127); comp_time<7
 * (짧은 용접)이면 (7-comp_time)*10 감쇠. samd20은 uint8_t로 언더플로(저전력+
 * 짧은용접 -> 큰 진폭) — 혼합 입장상 0 클램프로 수정 (spec §8). */
static uint8_t weld_amplitude(uint8_t output_power, uint16_t comp_time)
{
    uint16_t amp = (uint16_t)(((uint16_t)(output_power - 50u) * 255u) / 100u);
    if (comp_time < WELD_COMP_FULL) {
        uint16_t reduction = (uint16_t)((WELD_COMP_FULL - comp_time) * 10u);
        amp = (amp >= reduction) ? (uint16_t)(amp - reduction) : 0u;
    }
    return (uint8_t)amp;
}
```

- [ ] **Step 4: 테스트 PASS 확인**

Run: `make -C fw/test test`
Expected: `test_app_weld_fsm: all passed` (8개 테스트 함수 전부).

- [ ] **Step 5: 커밋**

```bash
git add fw/src/app_weld_fsm.c fw/test/test_app_weld_fsm.c
git commit -m "feat(weld): comp_time 진폭보정 + 언더플로 가드 + 엣지/start 테스트

- 짧은 용접(ldt2<=6) 진폭 감쇠 (7-comp_time)*10 (samd20 충실)
- uint8 언더플로 가드: amp<reduction -> 0 (명백한 버그 수정, spec §8)
- 진폭 0/63/127 경계, start READY-only 소비, cycle_done 단일엣지 검증"
```

---

### Task 4: app_reg US_CYCLE 통합 (enum + ceiling 주석) + 펌웨어 빌드

**Files:**
- Modify: `fw/include/app_lcd.h` (us_run_status enum)
- Modify: `fw/src/app_reg.c` (ceiling 블록 주석)

- [ ] **Step 1: us_run_status enum에 US_CYCLE 추가** — `fw/include/app_lcd.h`

기존:
```c
enum { US_IDLE = 0, US_REMOTE = 1, US_TOUCH = 2, US_COMM = 3 };
```
교체:
```c
enum { US_IDLE = 0, US_REMOTE = 1, US_TOUCH = 2, US_COMM = 3, US_CYCLE = 4 };
```
(주석에 한 줄 추가: `US_CYCLE = weld-cycle WELD 단계가 게이트를 구동하는 소스 — app_reg_command는 src를 그대로 us_run_status에 저장하므로 별도 분기 불필요.`)

- [ ] **Step 2: app_reg.c ceiling 블록에 US_CYCLE 제외 주석** — `fw/src/app_reg.c`

`uint8_t rs = g_reg.us_run_status;` 아래의 조건문 위에 주석 추가:
```c
        /* US_CYCLE(weld-cycle WELD)은 의도적으로 ceiling 대상 아님: WELD 길이는
         * weld-cycle FSM의 limit_delay_time2가 지배(app_weld). 아래 조건이
         * TOUCH/COMM만 포함하므로 US_CYCLE은 자연히 제외됨 (spec §5.2). */
        uint8_t rs = g_reg.us_run_status;
        if ((rs == (uint8_t)US_TOUCH) || (rs == (uint8_t)US_COMM)) {
```
(조건문 자체는 변경 없음 — 주석만 추가.)

- [ ] **Step 3: 펌웨어 재구성 + 빌드 (0-warning 확인)**

Run:
```bash
env -u STM32_TOOLCHAIN cmake -S fw -B fw/build -G Ninja
env -u STM32_TOOLCHAIN cmake --build fw/build
```
Expected: 빌드 성공, 0 warning. (app_weld_fsm.c는 GLOB로 이미 컴파일됨 — 아직 미호출이나 non-static이라 미사용 경고 없음.)

- [ ] **Step 4: host 테스트 회귀 확인**

Run: `make -C fw/test test`
Expected: 전 스위트 PASS.

- [ ] **Step 5: 커밋**

```bash
git add fw/include/app_lcd.h fw/src/app_reg.c
git commit -m "feat(weld): app_reg US_CYCLE 소스 추가 (enum) + ceiling 제외 주석

- us_run_status enum에 US_CYCLE=4 (WELD가 게이트 구동하는 소스)
- app_reg_command는 src를 그대로 저장 -> START/RELEASE 분기 불필요
- on-time ceiling은 TOUCH/COMM만 -> US_CYCLE 자연 제외(주석 명시, spec §5.2)"
```

---

### Task 5: 글루 모듈 app_weld.{h,c} + SOL_DN/set_amp hook 스텁

**Files:**
- Create: `fw/include/app_weld.h`
- Create: `fw/src/app_weld.c`

- [ ] **Step 1: 글루 헤더 작성** — `fw/include/app_weld.h`

```c
/* fw/include/app_weld.h — Stage Weld-Cycle slice 1 glue: drives weld_fsm_step
 * every 10 ms from live config, turns out-events into the SOL_DN hook +
 * app_reg US_CYCLE commands + work_cnt persistence. Production trigger
 * (physical SW_START1/2) is slice 4; slice 1 exposes app_weld_request_start()
 * as the future join point (no production caller this slice). */
#pragma once
#include <stdbool.h>
#include <stdint.h>

void app_weld_init(void);            /* boot: reset FSM + tick gate */
void app_weld_tick(void);            /* superloop: 10 ms-gated advance + effects */
void app_weld_request_start(void);   /* one-shot cycle trigger (slice 4 caller) */

/* SOL_DN solenoid hook (slice 1: mon log; slice 4: PB5 GPIO). */
void app_weld_hook_sol_dn(bool on);

/* Weld amplitude hook — takes the comp_time-corrected RAW DAC value (0..127)
 * and writes it straight to I2C_POT (samd20 main.c:1549). slice 1: mon log;
 * B-SEAM: real I2C_POT. Do NOT route through app_lcd_hook_set_pot — that one
 * takes output_power(50..100) and re-applies (x-50)*255/100 (double convert). */
void app_weld_hook_set_amp(uint8_t dac);
```

- [ ] **Step 2: 글루 .c 작성** — `fw/src/app_weld.c`

```c
/* fw/src/app_weld.c — weld-cycle glue (slice 1). 10 ms-gated FSM advance;
 * out-events -> SOL_DN hook + app_reg US_CYCLE + work_cnt FRAM/LCD. No SETUP
 * gate this slice (no running cycle on HW — slice 4, spec §5.4). */
#include "app_weld.h"
#include "app_weld_fsm.h"
#include "app_lcd.h"      /* app_lcd_cfg, app_lcd_set_work_cnt, US_CYCLE, us_cmd_t */
#include "app_reg.h"      /* app_reg_command, US_CMD_START/RUN_RELEASE */
#include "app_config.h"   /* app_config_t, app_config_save_all */
#include "sys_tick.h"
#include "mon.h"

#define WELD_TICK_MS  10u   /* samd20 temp_time-- cadence */

static uint32_t s_prev_ms;
static uint8_t  s_start_pending;   /* one-shot latch (slice 4 caller) */
static uint8_t  s_sol_last;        /* SOL_DN level edge tracking */

void app_weld_init(void)
{
    weld_fsm_init();
    s_prev_ms       = sys_tick_get_ms();
    s_start_pending = 0u;
    s_sol_last      = 0u;
}

void app_weld_request_start(void)
{
    s_start_pending = 1u;          /* consumed next tick; READY-only in core */
}

void app_weld_hook_sol_dn(bool on)
{
    mon_printf("[weld] SOL_DN %s\r\n", on ? "ON" : "OFF");
}

void app_weld_hook_set_amp(uint8_t dac)
{
    /* comp_time-corrected RAW DAC (0..127) straight to I2C_POT (samd20
     * main.c:1549). slice 1: log only. NOT app_lcd_hook_set_pot — that takes
     * output_power and re-converts (x-50)*255/100 = double-convert bug. */
    mon_printf("[weld] set_amp dac=%u\r\n", (unsigned)dac);
}

void app_weld_tick(void)
{
    uint32_t now = sys_tick_get_ms();
    if ((uint32_t)(now - s_prev_ms) < WELD_TICK_MS) {
        return;
    }
    s_prev_ms = now;

    app_config_t *cfg = app_lcd_cfg();
    weld_in_t in = {
        .start             = s_start_pending,
        .run_mode          = cfg->run_mode,
        .limit_delay_time1 = cfg->limit_delay_time1,
        .limit_delay_time2 = cfg->limit_delay_time2,
        .limit_delay_time3 = cfg->limit_delay_time3,
        .output_power      = cfg->output_power,
    };
    s_start_pending = 0u;          /* one-shot consumed */

    weld_out_t out;
    weld_fsm_step(&in, &out);

    if (out.sol_dn != s_sol_last) {
        s_sol_last = out.sol_dn;
        app_weld_hook_sol_dn(out.sol_dn != 0u);
    }
    if (out.weld_start) {
        app_weld_hook_set_amp(out.amplitude);   /* raw DAC, NOT set_pot (double-convert) */
        app_reg_command(US_CMD_START, (uint8_t)US_CYCLE);
    }
    if (out.weld_stop) {
        app_reg_command(US_CMD_RUN_RELEASE, (uint8_t)US_CYCLE);
    }
    if (out.cycle_done) {
        cfg->work_cnt++;
        app_config_save_all(cfg);
        app_lcd_set_work_cnt(cfg->work_cnt);
    }
}
```

- [ ] **Step 3: 펌웨어 재구성 + 빌드 (0-warning)**

Run:
```bash
env -u STM32_TOOLCHAIN cmake -S fw -B fw/build -G Ninja
env -u STM32_TOOLCHAIN cmake --build fw/build
```
Expected: 성공, 0 warning. (app_weld.c 컴파일·링크됨; app_weld_tick/init은 Task 6에서 호출 — non-static이라 경고 없음.)

- [ ] **Step 4: 커밋**

```bash
git add fw/include/app_weld.h fw/src/app_weld.c
git commit -m "feat(weld): 글루 app_weld.{h,c} + SOL_DN/set_amp hook 스텁

- 10ms 게이트 FSM 구동, out-events -> SOL_DN hook(로그) / app_reg US_CYCLE
  START·RELEASE / cycle_done -> work_cnt++ + app_config_save_all + LCD
- 진폭: 전용 app_weld_hook_set_amp(raw DAC 0..127) — 기존 set_pot은
  output_power 입력이라 재사용 시 이중 변환(spec §6)
- app_weld_request_start() one-shot 래치(슬라이스4 합류점, 프로덕션 호출자 없음)
- SETUP 게이트 없음(슬라이스4, spec §5.4)"
```

---

### Task 6: 슈퍼루프 배선 (init + tick) + 최종 빌드/테스트 게이트

**Files:**
- Modify: `fw/src/main.c` (app_weld_init)
- Modify: `fw/src/app.c` (app_weld_tick)

- [ ] **Step 1: main.c에 init 배선** — `fw/src/main.c`

`#include "app_reg.h"` 아래에 추가:
```c
#include "app_weld.h"
```
`app_reg_init();` 줄(현 27번) 바로 아래에 추가:
```c
    app_weld_init();   /* Stage Weld-Cycle: FSM reset (needs sys_tick up) */
```

- [ ] **Step 2: app.c app_loop_iter에 tick 배선** — `fw/src/app.c`

`#include "app_reg.h"` 아래에 추가:
```c
#include "app_weld.h"
```
`app_loop_iter`에서 `app_lcd_tick();` 블록과 `app_reg_tick(...)` 사이에 삽입(app_reg가 이번 iter의 게이트 상태를 보도록 app_reg 앞):
```c
    /* 2.5 Weld-cycle FSM — 10 ms cadence. WELD가 US_CYCLE로 게이트를 구동하므로
     * app_reg_tick 앞에 둬서 이번 iter publish에 반영. 슬라이스1은 프로덕션
     * 트리거 없음 -> READY 휴면(회귀 영향 없음). */
    app_weld_tick();
```

- [ ] **Step 3: 펌웨어 재구성 + 빌드 (0-warning)**

Run:
```bash
env -u STM32_TOOLCHAIN cmake -S fw -B fw/build -G Ninja
env -u STM32_TOOLCHAIN cmake --build fw/build
arm-none-eabi-size fw/build/gds_us_ctrl.elf
```
Expected: 성공, 0 warning. FLASH/RAM 사용량 출력(이전 대비 소폭 증가).

- [ ] **Step 4: host 테스트 전 스위트 PASS**

Run: `make -C fw/test test`
Expected: 4 스위트 전부 PASS (reg_calc, modbus_core, tcp_frame, weld_fsm).

- [ ] **Step 5: 커밋**

```bash
git add fw/src/main.c fw/src/app.c
git commit -m "feat(weld): 슈퍼루프 배선 — app_weld_init(boot) + app_weld_tick(loop)

- main.c: app_reg_init 뒤 app_weld_init
- app.c app_loop_iter: app_lcd_tick 뒤 / app_reg_tick 앞에 app_weld_tick
- 슬라이스1 트리거 없음 -> HW에서 READY 휴면, 기존 직접-초음파 무회귀"
```

- [ ] **Step 6: (HW, 선택) 보드 회귀 확인** — 보드 연결 시

플래시 후 mon으로:
- 부팅 정상, `[weld]` SOL_DN 로그 없음(READY 휴면 확인).
- 기존 직접-초음파 무회귀: 패널/Modbus START → ICON_RUN + on-time ceiling 정상(stage-d2b/c 거동 유지).

> 사이클 자체의 HW E2E(상태 시퀀스·work_cnt 증가·SOL_DN 구동)는 **슬라이스4**(물리 SW_START + SOL_DN/센서 실구동).

---

## Self-Review (작성자 체크)

**Spec coverage:**
- §2 모듈 분리(app_weld_fsm 코어 + app_weld 글루) → Task 1/2/3(코어), Task 5(글루). ✓
- §3 5상태 DELAY FSM + comp_time → Task 2(상태/타이밍), Task 3(comp_time). ✓
- §4 데이터 흐름(10ms 게이트, out-events) → Task 5 글루. ✓
- §5.1 US_CYCLE enum / §5.2 ceiling 제외 → Task 4. ✓
- §5.3 트리거: app_weld_request_start 정의(프로덕션 호출자 없음) → Task 5. ✓
- §5.4 SETUP 게이트 이연 → 글루에 미포함(주석). ✓
- §6 SOL_DN hook 스텁 / **전용 set_amp raw-DAC hook**(기존 set_pot은 output_power 입력→이중 변환이라 재사용 불가, spec §6) → Task 5. ✓
- §7 work_cnt++/FRAM/LCD → Task 5 cycle_done. ✓
- §8 언더플로 가드 → Task 3. ✓
- §9 host 테스트 6종 → Task 2(완전사이클·타이밍) + Task 3(진폭·comp_time·가드·start·엣지). ✓
- §10 빌드/검증 게이트 → Task 4/6. ✓

**Placeholder scan:** 없음 — 모든 step에 실제 코드/명령/기대출력.

**Type consistency:** `weld_in_t`/`weld_out_t` 필드명 Task1 헤더↔Task2/3 테스트↔Task5 글루 일치(start, run_mode, limit_delay_time1/2/3, output_power / run_status, sol_dn, weld_start, weld_stop, amplitude, cycle_done). `weld_fsm_init/step/status`, `app_weld_init/tick/request_start/hook_sol_dn`, `US_CYCLE`, `US_CMD_START`/`US_CMD_RUN_RELEASE`, `app_reg_command(us_cmd_t,uint8_t)`, `app_lcd_cfg`/`app_lcd_set_work_cnt`/`app_config_save_all` 전부 실제 시그니처와 일치(확인됨); 진폭은 신규 `app_weld_hook_set_amp(uint8_t)`(raw DAC) — 기존 `app_lcd_hook_set_pot`은 output_power 입력이라 미사용(이중 변환 회피).

**알려진 상호작용(슬라이스4 메모):** 부트 워밍업(~4s) 중 사이클 트리거 시 app_reg `main_state!=0` 가드로 WELD의 US_CYCLE START가 무시됨 → 슬라이스4 물리 트리거 도입 시 고려.

---

*작성: 2026-06-14. spec `2026-06-14-stage-weld-cycle-slice1-design.md`. 다음 = subagent-driven-development로 Task 1부터.*
