# Weld-Cycle Slice 3 (multi_ctrl 2단 진폭 스테핑) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** weld-cycle WELD 단계에서 진폭을 2단(`limit_mo_out1`→`limit_mo_out2`)으로 스테핑하고, 총 시간(`limit_mo_time2`) 경과 시 WELD를 정상 종료한다. `multi_ctrl=ON`이면 슬라이스1 DELAY-exit·슬라이스2 energy-exit보다 우선.

**Architecture:** 순수 FSM(`app_weld_fsm`) 내부에 stage 카운터(`s_multi_stage` 0/1)·경과 카운터(`s_multi_elapsed`, 10ms tick)를 추가(슬라이스2의 app_reg/주입 분리 불필요 — multi는 순수 시간 기반). 진폭 emit은 단일 설계: 기존 `weld_start` 엣지(1단) + 신규 `amp_change` 엣지(0→1 전환, 2단)가 **같은 `amplitude` 필드**를 내보내고 글루가 **같은 `app_weld_hook_set_amp`**로 라우팅. comp_time 미적용, 언더플로 가드(`limit_mo_out<50→0`). 타이밍은 액면가 10ms tick(samd20 100ms 절단 미재현). 범위 = weld-only.

**Tech Stack:** C (STM32F410 베어메탈, HAL-free FSM 코어), hand-rolled host-test(`fw/test`, `cc` + `CHECK_EQ`), CMake/Ninja 펌웨어 빌드.

**Spec:** `docs/superpowers/specs/2026-06-16-stage-weld-cycle-slice3-multi-design.md`

**빌드/테스트 명령(전 task 공통):**
- 호스트 테스트: `make -C fw/test test` (전 스위트; weld_fsm 스위트만 본 슬라이스에서 확장)
- 펌웨어 빌드(0-warning 확인): `env -u STM32_TOOLCHAIN cmake --build fw/build` (빌드 디렉토리 없으면 먼저 `env -u STM32_TOOLCHAIN cmake -B fw/build -G Ninja -S fw`)

---

## File Structure

| 파일 | 책임 | 본 슬라이스 변경 |
|---|---|---|
| `fw/include/app_weld_fsm.h` | 순수 FSM 인터페이스(`weld_in_t`/`weld_out_t`) | `weld_in_t` += multi 5필드, `weld_out_t` += `amp_change` |
| `fw/src/app_weld_fsm.c` | 순수 FSM 코어(WELD 분기) | 정적 stage/elapsed 상태, `weld_mo_amplitude()`, WELD 진입/step multi 분기 |
| `fw/test/test_app_weld_fsm.c` | host 단위 테스트 | `trace_t` 확장 + multi 시나리오 9 테스트 |
| `fw/src/app_weld.c` | 글루(cfg 주입 + out 라우팅) | multi 5필드 주입 + `amp_change`→`set_amp` 라우팅 |

> `app_config.h`/`app_config.c`(cfg `multi_ctrl`/`limit_mo_out1/2`/`limit_mo_time1/2`)·Modbus·LCD는 **이미 포팅됨** — 본 슬라이스 무수정. app_reg·신규 hook 없음.

---

## Task 1: FSM 인터페이스 확장 (헤더 + 정적 상태 + init)

순수 필드 추가만 — 동작 변경 없음(`multi_ctrl=0`이면 기존 경로). `weld_out_t`에 필드를 추가해도 `weld_fsm_step` 첫 줄 `memset(out,0,...)`이 매 step 0으로 초기화하므로 기존 테스트 무회귀.

**Files:**
- Modify: `fw/include/app_weld_fsm.h:36` (`weld_in_t` 끝, `limit_out_time` 뒤)
- Modify: `fw/include/app_weld_fsm.h:48` (`weld_out_t` 끝, `weld_fault` 뒤)
- Modify: `fw/src/app_weld_fsm.c:9` (정적 변수 선언 추가)
- Modify: `fw/src/app_weld_fsm.c:11-18` (`weld_fsm_init` 리셋 추가)

- [ ] **Step 1: `weld_in_t`에 multi 5필드 추가**

`fw/include/app_weld_fsm.h`의 `weld_in_t`에서 `uint16_t limit_out_time;` 줄 **뒤에** 추가:

```c
    uint8_t  multi_ctrl;         /* 1 = WELD 2단 진폭 스테핑 (energy/DELAY exit 차단) */
    uint8_t  limit_mo_out1;      /* 1단 진폭 base 50..100 (cfg; Modbus MULTI_O1) */
    uint8_t  limit_mo_out2;      /* 2단 진폭 base 50..100 (cfg; Modbus MULTI_O2) */
    uint16_t limit_mo_time1;     /* 1단 지속 = 0→1 전환 시점 (x10 ms) */
    uint16_t limit_mo_time2;     /* WELD 총 길이 = 종료 시점 (x10 ms, 정상 운용 >= time1) */
```

- [ ] **Step 2: `weld_out_t`에 `amp_change` 엣지 추가**

`weld_out_t`에서 `uint8_t weld_fault;` 줄 **뒤에** 추가:

```c
    uint8_t  amp_change;   /* 1 = stage 0→1 전환 엣지: 글루가 set_amp 재호출 (US_CYCLE 유지) */
```

- [ ] **Step 3: FSM 정적 상태 변수 추가**

`fw/src/app_weld_fsm.c`에서 `static uint8_t s_sol_dn;` 줄(현 line 9) **뒤에** 추가:

```c
static uint8_t  s_multi_stage;      /* 0 = out1 단계, 1 = out2 단계 (samd20 multi_ctrl_stage) */
static uint16_t s_multi_elapsed;    /* WELD 진입 후 경과 (10ms/count); time1/time2와 직접 비교 */
```

- [ ] **Step 4: `weld_fsm_init`에 리셋 추가**

`weld_fsm_init()`의 `s_sol_dn = 0u;` 줄 **뒤에** 추가:

```c
    s_multi_stage   = 0u;
    s_multi_elapsed = 0u;
```

- [ ] **Step 5: 회귀 확인 — 빌드 + 전체 host-test**

Run: `make -C fw/test test`
Expected: 모든 스위트 PASS (특히 `test_app_weld_fsm: all passed` — 필드 추가만이라 무회귀).

Run: `env -u STM32_TOOLCHAIN cmake --build fw/build`
Expected: 0-warning(우리 코드), 빌드 성공.

- [ ] **Step 6: Commit**

```bash
git add fw/include/app_weld_fsm.h fw/src/app_weld_fsm.c
git commit -m "feat(weld): slice3 FSM 인터페이스 확장 — multi_ctrl 5필드 + amp_change 엣지 + 정적 stage/elapsed"
```

---

## Task 2: FSM multi 분기 구현 (진폭 변환 + 진입 + step) + host-test

TDD: multi 시나리오 테스트를 먼저 추가(RED) → FSM 분기 구현(GREEN). `weld_mo_amplitude`는 static이라 사이클 실행으로 간접 검증(기존 `test_amplitude_values` 패턴).

**Files:**
- Modify: `fw/test/test_app_weld_fsm.c` (`trace_t` 확장 + `run_cycle` 집계 + 9 테스트 + `main()` 등록)
- Modify: `fw/src/app_weld_fsm.c` (`weld_mo_amplitude()` 함수 + `WELD_WELD` 진입/step 분기)

- [ ] **Step 1: `trace_t`에 amp_change 집계 필드 추가**

`fw/test/test_app_weld_fsm.c`의 `trace_t` 구조체(현 line 27-33)에서 `uint8_t weld_amp;` 줄 **뒤에** 추가:

```c
    int amp_change_cnt;        /* 0→1 전환 엣지 횟수 */
    uint8_t amp_at_change;     /* amp_change step의 amplitude (2단 진폭) */
```

- [ ] **Step 2: `run_cycle`에 amp_change 집계 추가**

`run_cycle()`의 `if (out.weld_start) { t->weld_start_cnt++; t->weld_amp = out.amplitude; }` 줄 **뒤에** 추가:

```c
        if (out.amp_change) { t->amp_change_cnt++; t->amp_at_change = out.amplitude; }
```

- [ ] **Step 3: multi 시나리오 9 테스트 작성**

`test_backstop_floor_zero` 함수 **뒤, `int main(void)` 앞에** 다음 9개 함수를 추가:

```c
/* multi 스테핑: out1=75 -> 1단 진폭 63 at weld_start, time1(=5) step에서 amp_change
 * +2단 진폭(out2=100 -> 127), time2(=12) step에서 weld_stop+HOLD, 정상 사이클 완주. */
static void test_multi_stepping(void)
{
    weld_fsm_init();
    weld_in_t in = { .start=0u, .run_mode=0u,
                     .limit_delay_time1=2u, .limit_delay_time2=100u,
                     .limit_delay_time3=2u, .output_power=100u,
                     .multi_ctrl=1u, .limit_mo_out1=75u, .limit_mo_out2=100u,
                     .limit_mo_time1=5u, .limit_mo_time2=12u };
    trace_t t;
    run_cycle(&in, &t, 300);
    CHECK_EQ(t.weld_start_cnt, 1);
    CHECK_EQ(t.weld_amp, 63u);          /* out1=75 -> (75-50)*255/100 = 63 (1단) */
    CHECK_EQ(t.amp_change_cnt, 1);      /* 0->1 전환 1회 */
    CHECK_EQ(t.amp_at_change, 127u);    /* out2=100 -> 127 (2단) */
    CHECK_EQ(t.weld_stop_cnt, 1);
    CHECK_EQ(t.cycle_done_cnt, 1);      /* graceful HOLD->CYL2->READY 완주 */
    CHECK_EQ(t.weld_steps, 12);         /* limit_mo_time2 (ldt2=100 무관) */
}

/* multi가 energy override: multi_ctrl=1 + energy_ctrl=1, curr_energy를 limit
 * 훨씬 초과 주입해도 energy-exit/backstop 미발동, time2에 종료. */
static void test_multi_overrides_energy(void)
{
    weld_fsm_init();
    weld_in_t in = { .start=1u, .run_mode=0u,
                     .limit_delay_time1=2u, .limit_delay_time2=100u,
                     .limit_delay_time3=2u, .output_power=100u,
                     .multi_ctrl=1u, .limit_mo_out1=60u, .limit_mo_out2=80u,
                     .limit_mo_time1=3u, .limit_mo_time2=8u,
                     .energy_ctrl=1u, .curr_energy=99999u, .limit_energy=1u,
                     .limit_out_time=10u };
    int weld_stop=0, weld_steps=0, fault=0;
    for (int i = 0; i < 300; i++) {
        weld_out_t out;
        weld_fsm_step(&in, &out);
        in.start = 0u;
        if (out.run_status == WELD_WELD) weld_steps++;
        if (out.weld_stop)  weld_stop++;
        if (out.weld_fault) fault++;
        if (out.cycle_done) break;
    }
    CHECK_EQ(weld_stop, 1);
    CHECK_EQ(fault, 0);          /* energy exit/backstop 미발동 (multi 우선) */
    CHECK_EQ(weld_steps, 8);     /* time2, energy 무관 */
}

/* multi가 DELAY override: multi_ctrl=1, ldt2=2(작음), s_temp_time 만료 무시, time2에 종료. */
static void test_multi_overrides_delay(void)
{
    weld_fsm_init();
    weld_in_t in = { .start=1u, .run_mode=0u,
                     .limit_delay_time1=2u, .limit_delay_time2=2u,
                     .limit_delay_time3=2u, .output_power=100u,
                     .multi_ctrl=1u, .limit_mo_out1=60u, .limit_mo_out2=80u,
                     .limit_mo_time1=4u, .limit_mo_time2=10u };
    int weld_steps=0, weld_stop=0;
    for (int i = 0; i < 300; i++) {
        weld_out_t out;
        weld_fsm_step(&in, &out);
        in.start = 0u;
        if (out.run_status == WELD_WELD) weld_steps++;
        if (out.weld_stop) weld_stop++;
        if (out.cycle_done) break;
    }
    CHECK_EQ(weld_steps, 10);    /* time2, NOT ldt2=2 */
    CHECK_EQ(weld_stop, 1);
}

/* 언더플로 가드: limit_mo_out1=25(<50) -> 1단 진폭 0(wrap 금지), out2=60 -> 25 정상 전환. */
static void test_multi_underflow_guard(void)
{
    weld_fsm_init();
    weld_in_t in = { .start=0u, .run_mode=0u,
                     .limit_delay_time1=2u, .limit_delay_time2=100u,
                     .limit_delay_time3=2u, .output_power=100u,
                     .multi_ctrl=1u, .limit_mo_out1=25u, .limit_mo_out2=60u,
                     .limit_mo_time1=3u, .limit_mo_time2=8u };
    trace_t t;
    run_cycle(&in, &t, 300);
    CHECK_EQ(t.weld_amp, 0u);          /* out1=25<50 -> 0 (가드) */
    CHECK_EQ(t.amp_at_change, 25u);    /* out2=60 -> (60-50)*255/100 = 25 */
}

/* stage 리셋: 1사이클 후 재실행 시 stage/elapsed 재시작 -> 2회차도 1단 진폭부터. */
static void test_multi_stage_reset(void)
{
    weld_fsm_init();
    weld_in_t base = { .start=0u, .run_mode=0u,
                       .limit_delay_time1=2u, .limit_delay_time2=100u,
                       .limit_delay_time3=2u, .output_power=100u,
                       .multi_ctrl=1u, .limit_mo_out1=75u, .limit_mo_out2=100u,
                       .limit_mo_time1=3u, .limit_mo_time2=7u };
    trace_t t1, t2;
    run_cycle(&base, &t1, 300);
    run_cycle(&base, &t2, 300);   /* READY 복귀 후 재실행 */
    CHECK_EQ(t2.weld_amp, 63u);          /* 2회차도 1단 진폭(out1=75->63)부터 */
    CHECK_EQ(t2.amp_change_cnt, 1);      /* 2회차도 전환 1회 */
    CHECK_EQ(t2.amp_at_change, 127u);
}

/* 경계 time1==time2: 전환과 종료가 같은 step (정의된 거동 동결: 둘 다 같은 step). */
static void test_multi_boundary_equal(void)
{
    weld_fsm_init();
    weld_in_t in = { .start=1u, .run_mode=0u,
                     .limit_delay_time1=2u, .limit_delay_time2=100u,
                     .limit_delay_time3=2u, .output_power=100u,
                     .multi_ctrl=1u, .limit_mo_out1=60u, .limit_mo_out2=100u,
                     .limit_mo_time1=5u, .limit_mo_time2=5u };
    int amp_change=0, weld_stop=0, both_same_step=0;
    for (int i = 0; i < 300; i++) {
        weld_out_t out;
        weld_fsm_step(&in, &out);
        in.start = 0u;
        if (out.amp_change) amp_change++;
        if (out.weld_stop)  weld_stop++;
        if (out.amp_change && out.weld_stop) both_same_step++;
        if (out.cycle_done) break;
    }
    CHECK_EQ(amp_change, 1);
    CHECK_EQ(weld_stop, 1);
    CHECK_EQ(both_same_step, 1);   /* 전환+종료 같은 step */
}

/* 경계 time1>time2: 종료가 먼저 -> 단일 진폭(out1)로 종료, amp_change 없음. */
static void test_multi_boundary_time1_gt_time2(void)
{
    weld_fsm_init();
    weld_in_t in = { .start=1u, .run_mode=0u,
                     .limit_delay_time1=2u, .limit_delay_time2=100u,
                     .limit_delay_time3=2u, .output_power=100u,
                     .multi_ctrl=1u, .limit_mo_out1=60u, .limit_mo_out2=100u,
                     .limit_mo_time1=20u, .limit_mo_time2=6u };
    int amp_change=0, weld_stop=0, weld_steps=0;
    for (int i = 0; i < 300; i++) {
        weld_out_t out;
        weld_fsm_step(&in, &out);
        in.start = 0u;
        if (out.run_status == WELD_WELD) weld_steps++;
        if (out.amp_change) amp_change++;
        if (out.weld_stop)  weld_stop++;
        if (out.cycle_done) break;
    }
    CHECK_EQ(amp_change, 0);     /* time1=20 미도달 -> 전환 없음 */
    CHECK_EQ(weld_stop, 1);
    CHECK_EQ(weld_steps, 6);     /* time2에 종료 */
}

/* 진폭 known-vector: out1/out2 = 50/100 -> 0/127 (역순/경계 포함 변환 검증). */
static void test_multi_amp_vectors(void)
{
    weld_fsm_init();
    weld_in_t in = { .start=0u, .run_mode=0u,
                     .limit_delay_time1=2u, .limit_delay_time2=100u,
                     .limit_delay_time3=2u, .output_power=100u,
                     .multi_ctrl=1u, .limit_mo_out1=50u, .limit_mo_out2=100u,
                     .limit_mo_time1=3u, .limit_mo_time2=8u };
    trace_t t;
    run_cycle(&in, &t, 300);
    CHECK_EQ(t.weld_amp, 0u);          /* out1=50 -> 0 */
    CHECK_EQ(t.amp_at_change, 127u);   /* out2=100 -> 127 */
}

/* multi_ctrl=0 회귀: 기존 슬라이스1 시간-exit 시퀀스 동일 (multi 분기 미진입). */
static void test_multi_off_regression(void)
{
    weld_fsm_init();
    weld_in_t in = { .start=0u, .run_mode=0u,
                     .limit_delay_time1=3u, .limit_delay_time2=10u,
                     .limit_delay_time3=2u, .output_power=100u,
                     .multi_ctrl=0u, .limit_mo_out1=75u, .limit_mo_out2=100u,
                     .limit_mo_time1=2u, .limit_mo_time2=4u };
    trace_t t;
    run_cycle(&in, &t, 300);
    CHECK_EQ(t.weld_steps, 10);        /* ldt2 시간-exit (multi 무시) */
    CHECK_EQ(t.amp_change_cnt, 0);     /* 전환 없음 */
    CHECK_EQ(t.weld_amp, 127u);        /* output_power=100 진폭 (limit_mo_out 무시) */
    CHECK_EQ(t.cycle_done_cnt, 1);
}
```

- [ ] **Step 4: `main()`에 9 테스트 등록**

`int main(void)`에서 `test_backstop_floor_zero();` 줄 **뒤에** 추가:

```c
    test_multi_stepping();
    test_multi_overrides_energy();
    test_multi_overrides_delay();
    test_multi_underflow_guard();
    test_multi_stage_reset();
    test_multi_boundary_equal();
    test_multi_boundary_time1_gt_time2();
    test_multi_amp_vectors();
    test_multi_off_regression();
```

- [ ] **Step 5: 테스트 RED 확인**

Run: `make -C fw/test test`
Expected: FAIL — multi 테스트가 실패(아직 FSM 분기 미구현; `multi_ctrl`이 무시되어 `weld_amp`가 output_power 기준, `amp_change_cnt=0`, `weld_steps`가 ldt2 기준 등). `test_multi_off_regression`만 통과할 수 있음.

- [ ] **Step 6: `weld_mo_amplitude` 순수 함수 추가**

`fw/src/app_weld_fsm.c`의 `weld_amplitude` 함수 **뒤(line 53 뒤, `weld_fsm_step` 앞)**에 추가:

```c
/* multi 경로 진폭: comp_time 미적용 (samd20 main.c:5242/1540 직접 변환). 팩토리
 * 기본 limit_mo_out1=25(<50)가 언더플로 트리거 -> 명시적 0 가드. weld_amplitude는
 * output_power [50,100] 클램프(upstream) 가정이라 가드 없으나, limit_mo_out은
 * config-validation 클램프가 슬라이스4 이연이라 여기서 방어. spec §3.5. */
static uint8_t weld_mo_amplitude(uint8_t mo_out)
{
    if (mo_out < 50u) {
        return 0u;
    }
    return (uint8_t)(((uint16_t)(mo_out - 50u) * 255u) / 100u);
}
```

- [ ] **Step 7: WELD 진입 multi 분기 (진폭 소스 분기)**

`fw/src/app_weld_fsm.c`의 `case WELD_WELD:` 첫 블록(현 line 96-99)을 교체.

기존:
```c
        if (s_f_status_start == 0u) {
            s_f_status_start = 1u;
            out->amplitude   = weld_amplitude(in->output_power, s_comp_time);
            out->weld_start  = 1u;       /* glue: US_CYCLE START + pot write */
        } else if (in->energy_ctrl) {
```

교체 후:
```c
        if (s_f_status_start == 0u) {
            s_f_status_start = 1u;
            if (in->multi_ctrl) {
                s_multi_stage   = 0u;
                s_multi_elapsed = 0u;
                out->amplitude  = weld_mo_amplitude(in->limit_mo_out1);  /* 1단 (comp 미적용) */
            } else {
                out->amplitude  = weld_amplitude(in->output_power, s_comp_time);
            }
            out->weld_start  = 1u;       /* glue: US_CYCLE START + pot write */
        } else if (in->multi_ctrl) {
            /* multi: 2단 진폭 스테핑 (samd20 5232-5258). 우선순위 최상 — energy/시간
             * exit 미발동(아래 else-if 분기 진입 안 함). spec §3.4. */
            if (s_multi_elapsed < 0xFFFFu) {
                s_multi_elapsed++;                   /* 포화 가드 (time2 매우 클 때 wrap 방지) */
            }
            if (s_multi_stage == 0u && s_multi_elapsed >= in->limit_mo_time1) {
                s_multi_stage   = 1u;
                out->amplitude  = weld_mo_amplitude(in->limit_mo_out2);  /* 2단 (samd20 5242) */
                out->amp_change = 1u;                /* glue: set_amp 재호출 */
            }
            if (s_multi_elapsed >= in->limit_mo_time2) {
                out->weld_stop   = 1u;               /* samd20 5250: WELD->HOLD */
                s_f_status_start = 0u;
                s_run_status     = WELD_HOLD;
                s_temp_time      = in->limit_delay_time3;
            }
        } else if (in->energy_ctrl) {
```

> 주의: 이 교체는 `else if (in->energy_ctrl) {` 줄을 **유지**한다(그 아래 슬라이스2 energy 블록·슬라이스1 `else if (s_temp_time == 0u)` 시간-exit 블록은 무수정). multi 분기가 energy/시간 분기 **앞**에 와서 우선순위(samd20 5234 if-else)를 재현하고, multi ON이면 두 기존 exit는 평가되지 않음.

- [ ] **Step 8: 테스트 GREEN 확인 (회귀 포함)**

Run: `make -C fw/test test`
Expected: 모든 스위트 PASS — `test_app_weld_fsm: all passed` (multi 9 테스트 + 기존 12 테스트 = 21 함수). 슬라이스1·2 회귀 없음.

- [ ] **Step 9: 펌웨어 빌드 0-warning 확인**

Run: `env -u STM32_TOOLCHAIN cmake --build fw/build`
Expected: 빌드 성공, 우리 코드 0-warning(vendor wiznet socket.h 기존 3경고는 무관).

- [ ] **Step 10: Commit**

```bash
git add fw/src/app_weld_fsm.c fw/test/test_app_weld_fsm.c
git commit -m "feat(weld): slice3 FSM multi 2단 진폭 스테핑 분기 + host-test 9 (스테핑/override/언더플로/경계/회귀)"
```

---

## Task 3: 글루 결선 (multi 필드 주입 + amp_change 라우팅)

FSM이 multi를 동작하려면 글루가 live cfg 5필드를 주입하고, `amp_change` 엣지를 `set_amp`로 라우팅해야 한다.

**Files:**
- Modify: `fw/src/app_weld.c:63-74` (`weld_in_t in` 초기화에 multi 5필드 추가)
- Modify: `fw/src/app_weld.c:89-92` (`amp_change` 라우팅 추가)

- [ ] **Step 1: `weld_in_t` 구성에 multi 5필드 주입**

`fw/src/app_weld.c`의 `weld_in_t in = { ... };` 블록에서 `.curr_energy = app_reg_measure()->curr_energy,` 줄 **뒤에** 추가:

```c
        .multi_ctrl        = cfg->multi_ctrl ? 1u : 0u,
        .limit_mo_out1     = (uint8_t)cfg->limit_mo_out1,
        .limit_mo_out2     = (uint8_t)cfg->limit_mo_out2,
        .limit_mo_time1    = cfg->limit_mo_time1,
        .limit_mo_time2    = cfg->limit_mo_time2,
```

> `cfg->limit_mo_out1/2`는 `uint16_t`지만 값 범위 50..100(+팩토리 25)이라 `(uint8_t)` 캐스트 안전. `output_power` 주입(uint8_t)과 시그니처 일관. `limit_mo_time1/2`는 `uint16_t` 그대로.

- [ ] **Step 2: `amp_change` 엣지 → `set_amp` 재호출 라우팅**

`fw/src/app_weld.c`의 out 라우팅에서 `weld_start` 블록(현 line 89-92) **뒤에** 추가:

```c
    if (out.amp_change) {
        app_weld_hook_set_amp(out.amplitude);   /* mid-WELD 2단 진폭 (US_CYCLE 유지, START 아님) */
    }
```

> `weld_start`는 `set_amp` + `app_reg_command(US_CMD_START)` 둘 다 하지만, `amp_change`는 이미 running 중이므로 `set_amp`만(진폭 갱신). START/STOP 명령 없음.

- [ ] **Step 3: 펌웨어 빌드 0-warning 확인**

Run: `env -u STM32_TOOLCHAIN cmake --build fw/build`
Expected: 빌드 성공, 우리 코드 0-warning. (글루는 host-test 비대상 — 통합 cpp-reviewer + HW mon으로 검증, spec §9.)

- [ ] **Step 4: 전체 host-test 회귀 확인**

Run: `make -C fw/test test`
Expected: 전 스위트 PASS (글루 변경은 FSM 테스트에 무영향, 회귀 가드).

- [ ] **Step 5: Commit**

```bash
git add fw/src/app_weld.c
git commit -m "feat(weld): slice3 글루 — multi 5필드 cfg 주입 + amp_change→set_amp 라우팅"
```

---

## 완료 후 (실행 핸들러 외부)

- **통합 cpp-reviewer**: FSM 분기(우선순위 게이팅 정확성) + 글루 결선 + 캐스트 안전성 리뷰.
- **HW 회귀확인(보드)**: spec §10 — ① 기존 직접-초음파 무회귀(START→ICON_RUN/~560ms ceiling) ② multi_ctrl=ON 직접 START 스테핑 **안 함**(단일 진폭, on-time ceiling 종료) = §6 deviation ③ work_cnt 0(weld dormant). 스테핑 자체 E2E는 슬라이스4(물리 SW_START + 실 초음파).
- **머지/태그**: `superpowers:finishing-a-development-branch` → `--no-ff` 머지 + 태그 `hw-revA_fw-stage-weld3`(⚠ host + HW-regression verified).

---

## Self-Review (spec 대조)

**Spec coverage:**
- §1.2 FSM 2단 스테핑(weld_in_t 5필드, s_multi_stage/elapsed, amp_change) → Task 1+2 ✓
- §1.2 WELD 진입 multi(stage=0/elapsed=0/conv(out1)/weld_start) → Task 2 Step 7 ✓
- §1.2 WELD step(stage 0→1 at time1, 종료 at time2→HOLD) → Task 2 Step 7 ✓
- §1.2/§3.4 우선순위 게이팅(multi 분기가 energy/시간 앞) → Task 2 Step 7(else-if 순서) + test_multi_overrides_energy/delay ✓
- §3.5 진폭 변환 + 언더플로 가드(out<50→0, comp_time 미적용) → Task 2 Step 6 + test_multi_underflow_guard/amp_vectors ✓
- §3.2 액면가 10ms tick(/10 절단 없음) → Task 2 Step 7(elapsed를 time1/time2와 직접 비교) + test_multi_stepping(weld_steps=12=time2) ✓
- §5 글루 5필드 주입 + amp_change→set_amp → Task 3 ✓
- §9 host-test(스테핑/양쪽진폭/time2종료/override×2/언더플로/stage리셋/경계×2/회귀) → Task 2 9 테스트 ✓
- §6 weld-only(직접 RUN 스테핑 이연) → 코드 미포함(글루는 US_CYCLE WELD만 multi 주입; 직접 RUN은 app_reg 경로 무수정) ✓

**Placeholder scan:** 없음 — 모든 step에 실제 코드/명령/기대값 포함.

**Type consistency:** `weld_mo_amplitude(uint8_t)` ↔ 글루 `(uint8_t)cfg->limit_mo_out1` 캐스트 일관. `limit_mo_time1/2` uint16_t(헤더) ↔ `cfg->limit_mo_time1/2` uint16_t 주입 일관. `amp_change`/`s_multi_stage`/`s_multi_elapsed` 명칭 전 task 동일. `weld_mo_amplitude` vs 기존 `weld_amplitude` 별도 함수(혼동 없음).

---

*작성: 2026-06-16 weld-cycle 슬라이스3 writing-plans. spec `2026-06-16-stage-weld-cycle-slice3-multi-design.md`. 다음 = 실행 방식 선택(subagent-driven 권장).*
