# Weld-Cycle Slice 2 (Energy-based WELD Exit) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** WELD 단계 종료를 시간(`limit_delay_time2`) 대신 에너지 도달(`energy_ctrl && curr_energy>=limit_energy`)로 전환하고, 미도달 시 `limit_out_time` 안에 abort하는 backstop을 추가한다.

**Architecture:** Option A — 에너지 누산(`acc_energy`/`curr_energy`)은 `app_reg`가 소유(samd20 ADC-ISR 동형, WELD 진입 리셋은 `US_CMD_START` 핸들러에 공짜로 포함), 순수 FSM(`app_weld_fsm`)은 `curr_energy`를 주입받아 비교만. 에너지 산술은 `app_reg_calc` 순수 헬퍼로 추출해 host-test. 누산은 모든 active run에서 보편적으로 동작(DISP_ENERGY/VAR_ENERGY 점등), 에너지-EXIT는 weld-cycle WELD에만.

**Tech Stack:** C11, STM32F410 HAL/CMSIS, 호스트 단위테스트(`fw/test`, plain `cc`), CMake/Ninja 펌웨어 빌드.

**Spec:** `docs/superpowers/specs/2026-06-14-stage-weld-cycle-slice2-energy-design.md`

---

## File Structure

| 파일 | 책임 | Task |
|---|---|---|
| `fw/include/app_reg_calc.h` | `reg_energy_from_acc` 선언 추가 | 1 |
| `fw/src/app_reg_calc.c` | `reg_energy_from_acc` 구현(`acc/REG_ENERGY_DIV`) | 1 |
| `fw/test/test_app_reg_calc.c` | 에너지 산술 host-test 추가 | 1 |
| `fw/include/app_weld_fsm.h` | `weld_in_t` +4필드 / `weld_out_t` +`weld_fault` | 2 |
| `fw/src/app_weld_fsm.c` | CYL1→WELD comp_time/temp_time 디커플 + WELD energy-exit/backstop 분기 | 2 |
| `fw/test/test_app_weld_fsm.c` | energy-exit / 시간-exit 스킵 / backstop abort host-test 추가 | 2 |
| `fw/src/app_reg.c` | `reg_state_t` +2필드, START 리셋, RUN_RELEASE/ceiling last_energy 래치, publish 누산 | 3 |
| `fw/include/app_weld.h` | `app_weld_hook_fault` 선언 | 4 |
| `fw/src/app_weld.c` | `weld_in_t` 4필드 + `curr_energy` 주입, `weld_fault`→fault hook | 4 |

> `fw/test/Makefile` **변경 불요**: `reg_energy_from_acc`는 이미 `BIN_REG` 의존인 `app_reg_calc.c`에 들어가고, `weld_fsm`는 새 외부 의존이 없다(curr_energy는 주입). 기존 weld 테스트는 designated-init로 새 필드를 0으로 채워 그대로 슬라이스1 회귀 테스트가 된다.

> **빌드/검증 주의(프로젝트 패턴)**: subagent는 코드만 작성(read-only review 별도), **빌드·테스트 sanity는 controller가 Task 사이에 실행**. Task 1·2는 host-test TDD(주 게이트), Task 3·4는 HAL 결합이라 host-test 비대상 → 펌웨어 빌드 0-warning + 통합 cpp-reviewer로 검증.

---

## Task 1: 에너지 산술 순수 헬퍼 (`reg_energy_from_acc`)

**Files:**
- Modify: `fw/include/app_reg_calc.h` (선언 추가, 파일 끝)
- Modify: `fw/src/app_reg_calc.c` (구현 추가, 파일 끝)
- Test: `fw/test/test_app_reg_calc.c`

- [ ] **Step 1: 실패하는 테스트 작성** — `fw/test/test_app_reg_calc.c`에 아래 두 함수를 `int main` 직전에 추가하고, `main()`의 `test_reg_on_time_200m();` 다음 줄에 두 호출을 추가한다.

```c
/* 누산기 -> 표시 에너지: acc / REG_ENERGY_DIV(=250). samd20 main.c:436은
 * acc/500 @1ms; STM32는 2ms publish에서 누산하므로 절반 divisor로 동일
 * energy-per-second 재현(spec §4.2). 절대 보정은 6b/HW. */
static void test_energy_from_acc(void) {
    CHECK_EQ(reg_energy_from_acc(0u), 0u);
    CHECK_EQ(reg_energy_from_acc(249u), 0u);    /* 250 미만 -> 0 */
    CHECK_EQ(reg_energy_from_acc(250u), 1u);    /* 정확히 1단위 */
    CHECK_EQ(reg_energy_from_acc(2500u), 10u);
    CHECK_EQ(reg_energy_from_acc(2749u), 10u);  /* 정수 나눗셈 floor */
}

/* 적분 구조: 일정 curr_power=250이면 step당 +1단위, limit=10에 step 10 도달. */
static void test_energy_integration_steps(void) {
    uint32_t acc = 0u;
    int step_reached = -1;
    for (int i = 1; i <= 20; i++) {
        acc += 250u;
        if (reg_energy_from_acc(acc) >= 10u) { step_reached = i; break; }
    }
    CHECK_EQ(step_reached, 10);
}
```

`main()` 추가(기존 `test_reg_on_time_200m();` 바로 아래):

```c
    test_energy_from_acc();
    test_energy_integration_steps();
```

- [ ] **Step 2: 테스트 실패 확인 (controller 실행)**

Run: `make -C fw/test test`
Expected: 컴파일 에러 — `reg_energy_from_acc` undeclared (아직 미구현).

- [ ] **Step 3: 헤더 선언 추가** — `fw/include/app_reg_calc.h` 파일 끝(`reg_on_time_200m` 선언 다음)에 추가:

```c
/* 누산 전력 -> 표시 에너지. samd20 main.c:436 (curr_energy = acc_energy/500)
 * 구조 포팅. STM32는 curr_power를 REG_TICK_MS(=2ms) publish gate에서 누산하므로
 * divisor를 500ms/2ms=250으로 두어 samd20의 1ms·/500 energy-per-second를 재현
 * (spec §4.2). ⚠ REG_ENERGY_DIV는 app_reg.c의 REG_TICK_MS=2ms 누산 cadence와
 * 결합 — REG_TICK_MS 변경 시 divisor도 재산정. 절대 보정은 6b/HW. */
uint32_t reg_energy_from_acc(uint32_t acc_energy);
```

- [ ] **Step 4: 구현 추가** — `fw/src/app_reg_calc.c` 파일 끝(`reg_on_time_200m` 구현 다음)에 추가:

```c
/* spec §4.2: 500ms 에너지-단위 윈도우 / REG_TICK_MS(2ms) = 250 샘플. */
#define REG_ENERGY_DIV  250u

uint32_t reg_energy_from_acc(uint32_t acc_energy)
{
    return acc_energy / REG_ENERGY_DIV;
}
```

- [ ] **Step 5: 테스트 통과 확인 (controller 실행)**

Run: `make -C fw/test test`
Expected: `all checks PASSED` (reg_calc 스위트), 나머지 스위트도 PASS.

- [ ] **Step 6: 커밋**

```bash
git add fw/include/app_reg_calc.h fw/src/app_reg_calc.c fw/test/test_app_reg_calc.c
git commit -m "feat(weld): reg_energy_from_acc 순수 헬퍼 + host-test (slice2 §4)"
```

---

## Task 2: FSM 에너지-exit + backstop + comp_time 디커플

**Files:**
- Modify: `fw/include/app_weld_fsm.h` (`weld_in_t`/`weld_out_t` 필드 추가)
- Modify: `fw/src/app_weld_fsm.c` (CYL1→WELD 전이 + WELD 분기)
- Test: `fw/test/test_app_weld_fsm.c`

- [ ] **Step 1: 인터페이스 확장** — `fw/include/app_weld_fsm.h`.

`weld_in_t`에서 `output_power` 줄 다음에 추가:

```c
    uint8_t  energy_ctrl;        /* 1 = WELD 종료를 에너지로 (시간-exit 스킵) */
    uint32_t curr_energy;        /* app_reg 누산 라이브 값 (글루가 주입) */
    uint32_t limit_energy;       /* 에너지 목표 (cfg; 외부 계약 Modbus reg 0x08) */
    uint16_t limit_out_time;     /* backstop 한계 (cfg; 초 단위, samd20 *10@100ms) */
```

`weld_out_t`에서 `cycle_done` 줄 다음에 추가:

```c
    uint8_t  weld_fault;   /* 1 = WELD backstop abort 엣지: 글루가 fault hook 호출 */
```

- [ ] **Step 2: 실패하는 테스트 작성** — `fw/test/test_app_weld_fsm.c`에 아래 세 함수를 `int main` 직전에 추가하고 `main()`에 호출 추가.

```c
/* energy-exit 정상: energy_ctrl=1, WELD step마다 curr_energy 증가 주입 ->
 * curr_energy>=limit_energy에서 정확히 1회 weld_stop+HOLD, ldt2(=100)와 무관. */
static void test_energy_exit_normal(void)
{
    weld_fsm_init();
    weld_in_t in = { .start=1u, .run_mode=0u,
                     .limit_delay_time1=2u, .limit_delay_time2=100u,
                     .limit_delay_time3=2u, .output_power=100u,
                     .energy_ctrl=1u, .curr_energy=0u, .limit_energy=50u,
                     .limit_out_time=10u };
    int weld_stop=0, weld_fault=0, cycle_done=0, weld_steps=0;
    for (int i = 0; i < 2000; i++) {
        weld_out_t out;
        weld_fsm_step(&in, &out);
        in.start = 0u;
        if (out.run_status == WELD_WELD) { weld_steps++; in.curr_energy += 10u; }
        if (out.weld_stop)  weld_stop++;
        if (out.weld_fault) weld_fault++;
        if (out.cycle_done) { cycle_done++; break; }
    }
    CHECK_EQ(weld_stop, 1);       /* 에너지 도달 -> 1회 정상 종료 */
    CHECK_EQ(weld_fault, 0);      /* fault 아님 */
    CHECK_EQ(cycle_done, 1);      /* HOLD->CYL2->READY 정상 완주 -> work_cnt++ */
    if (weld_steps >= 100) {      /* ldt2=100; 에너지-exit면 그보다 훨씬 짧아야 */
        printf("FAIL energy exit took %d weld steps (>= ldt2=100)\n", weld_steps);
        failures++;
    }
}

/* 시간-exit 스킵: energy_ctrl=1, ldt2=2(작음), 에너지 미도달 -> 50 step 후에도
 * WELD 유지(ldt2 시간-exit가 안 일어남), fault/stop 없음. */
static void test_time_exit_skipped_when_energy(void)
{
    weld_fsm_init();
    weld_in_t in = { .start=1u, .run_mode=0u,
                     .limit_delay_time1=2u, .limit_delay_time2=2u,
                     .limit_delay_time3=2u, .output_power=100u,
                     .energy_ctrl=1u, .curr_energy=0u, .limit_energy=1000u,
                     .limit_out_time=10u };   /* backstop=1000 tick, 50 step 내 미만료 */
    int weld_stop=0, weld_fault=0; uint8_t status=0xffu;
    for (int i = 0; i < 50; i++) {
        weld_out_t out;
        weld_fsm_step(&in, &out);
        in.start = 0u;
        if (out.weld_stop)  weld_stop++;
        if (out.weld_fault) weld_fault++;
        status = out.run_status;
    }
    CHECK_EQ(weld_stop, 0);          /* ldt2=2 시간-exit 안 일어남 (스킵) */
    CHECK_EQ(weld_fault, 0);
    CHECK_EQ(status, WELD_WELD);     /* 여전히 용접 중 */
}

/* backstop abort: energy_ctrl=1, 에너지 영원히 미도달, limit_out_time=1초
 * (=100 tick) -> abort. weld_fault+weld_stop 각 1회, cycle_done 0(work_cnt 미발생),
 * READY 복귀, sol_dn OFF. */
static void test_backstop_abort(void)
{
    weld_fsm_init();
    weld_in_t in = { .start=1u, .run_mode=0u,
                     .limit_delay_time1=2u, .limit_delay_time2=5u,
                     .limit_delay_time3=2u, .output_power=100u,
                     .energy_ctrl=1u, .curr_energy=0u, .limit_energy=1000u,
                     .limit_out_time=1u };
    int weld_stop=0, weld_fault=0, cycle_done=0;
    uint8_t final_status=0xffu, final_sol=1u;
    for (int i = 0; i < 300; i++) {
        weld_out_t out;
        weld_fsm_step(&in, &out);
        in.start = 0u;
        if (out.weld_stop)  weld_stop++;
        if (out.weld_fault) weld_fault++;
        if (out.cycle_done) cycle_done++;
        final_status = out.run_status;
        final_sol    = out.sol_dn;
    }
    CHECK_EQ(weld_fault, 1);          /* backstop abort 1회 */
    CHECK_EQ(weld_stop, 1);           /* abort도 US 정지 */
    CHECK_EQ(cycle_done, 0);          /* work_cnt++ 안 함 */
    CHECK_EQ(final_status, WELD_READY);
    CHECK_EQ(final_sol, 0);           /* 실린더 상승 */
}
```

`main()` 추가(기존 `test_cycle_done_single_edge();` 다음):

```c
    test_energy_exit_normal();
    test_time_exit_skipped_when_energy();
    test_backstop_abort();
```

- [ ] **Step 3: 테스트 실패 확인 (controller 실행)**

Run: `make -C fw/test test`
Expected: weld 스위트에서 FAIL — 현재 WELD는 시간-exit만이라 energy_ctrl=1 시나리오가 기대대로 동작 안 함(`test_time_exit_skipped`에서 weld_stop!=0 또는 `test_backstop_abort`에서 weld_fault==0). (필드는 Step 1에서 추가됐으므로 컴파일은 됨.)

- [ ] **Step 4: CYL1→WELD 전이 디커플** — `fw/src/app_weld_fsm.c`의 `case WELD_CYL1:` 블록에서 아래 부분을

```c
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
```

다음으로 교체:

```c
        } else if (s_temp_time == 0u) {
            s_f_status_start = 0u;
            s_run_status     = WELD_WELD;
            /* comp_time은 항상 ldt2에서 유도 (진폭 보정; energy 모드에서도 적용 —
             * samd20 main.c:1546-1547은 energy_ctrl 무관). slice2 §3.2. */
            s_comp_time = (in->limit_delay_time2 > 6u) ? WELD_COMP_FULL
                                                       : in->limit_delay_time2;
            /* temp_time: energy 모드 -> backstop 카운트다운(limit_out_time초 ×100
             * tick), 시간 모드 -> 기존 ldt2/7 (samd20 main.c:1504). */
            if (in->energy_ctrl) {
                s_temp_time = weld_backstop_ticks(in->limit_out_time);
            } else {
                s_temp_time = (in->limit_delay_time2 > 6u) ? in->limit_delay_time2
                                                           : WELD_COMP_FULL;
            }
        }
```

- [ ] **Step 5: backstop 헬퍼 추가** — `fw/src/app_weld_fsm.c`의 `weld_amplitude` 정적 함수 정의 **위**에 추가:

```c
/* limit_out_time(초) -> FSM tick(10ms/tick) 수. samd20 backstop은
 * us_on_time(100ms 단위) >= limit_out_time*10, 즉 limit_out_time초. 10ms tick에선
 * limit_out_time*100 (limit_out_time<=10 -> 최대 1000 tick, uint16 OK). spec §3.2. */
#define WELD_TICKS_PER_SEC  100u
static uint16_t weld_backstop_ticks(uint16_t limit_out_time_sec)
{
    return (uint16_t)(limit_out_time_sec * WELD_TICKS_PER_SEC);
}
```

- [ ] **Step 6: WELD 분기 교체** — `fw/src/app_weld_fsm.c`의 `case WELD_WELD:` 블록 전체를

```c
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
```

다음으로 교체:

```c
    case WELD_WELD:
        if (s_f_status_start == 0u) {
            s_f_status_start = 1u;
            out->amplitude   = weld_amplitude(in->output_power, s_comp_time);
            out->weld_start  = 1u;       /* glue: US_CYCLE START + pot write */
        } else if (in->energy_ctrl) {
            /* energy 모드: 에너지 도달 -> 정상 종료(samd20 5272); 미도달 +
             * backstop 만료 -> abort(samd20 5288, 에러 표시는 이연). spec §3.3. */
            if (in->curr_energy >= in->limit_energy) {
                out->weld_stop   = 1u;
                s_f_status_start = 0u;
                s_run_status     = WELD_HOLD;
                s_temp_time      = in->limit_delay_time3;
            } else if (s_temp_time == 0u) {
                out->weld_stop   = 1u;   /* abort도 US 정지 */
                out->weld_fault  = 1u;   /* glue: fault hook (mon; 후속 SYS_ERROR) */
                s_sol_dn         = 0u;   /* 실린더 즉시 상승 */
                s_f_status_start = 0u;
                s_run_status     = WELD_READY;   /* CYL2 미경유, work_cnt++ 없음 */
            }
        } else if (s_temp_time == 0u) {
            /* slice-1 시간-exit (energy_ctrl off) — 무회귀. */
            out->weld_stop   = 1u;
            s_f_status_start = 0u;
            s_run_status     = WELD_HOLD;
            s_temp_time      = in->limit_delay_time3;
        }
        break;
```

- [ ] **Step 7: 테스트 통과 확인 (controller 실행)**

Run: `make -C fw/test test`
Expected: 전 스위트 PASS — 신규 3개(energy_exit_normal / time_exit_skipped / backstop_abort) + 기존 weld 8개(슬라이스1 회귀, energy_ctrl=0) + reg_calc 포함 모두.

- [ ] **Step 8: 커밋**

```bash
git add fw/include/app_weld_fsm.h fw/src/app_weld_fsm.c fw/test/test_app_weld_fsm.c
git commit -m "feat(weld): FSM energy-exit + backstop abort + comp_time 디커플 (slice2 §3)"
```

---

## Task 3: app_reg 에너지 누산 결선 (HAL 결합 — build/review 검증)

**Files:**
- Modify: `fw/src/app_reg.c` (4지점)

> host-test 비대상. controller가 펌웨어 빌드 0-warning으로 sanity.

- [ ] **Step 1: `reg_state_t` 필드 추가** — `fw/src/app_reg.c`의 `reg_state_t` 구조에서 `uint8_t swallow_start;` 블록(주석 포함) 다음 줄에 추가:

```c
    uint32_t acc_energy;             /* 전력 적분기 (run-start 리셋; samd20 acc_energy) */
    uint32_t last_energy;            /* run-stop 시 curr_energy 래치 (samd20 last_energy) */
```

- [ ] **Step 2: `US_CMD_START` 리셋 추가** — `app_reg_command`의 `US_CMD_START` 분기에서 `g_measure.us_on_time_200m = 0u;` 줄 **다음**에 추가:

```c
            /* 에너지 적분 run-start 리셋 (samd20 main.c:1340/1366/1555 — 전부
             * run-start 엣지). curr_energy 직접 0으로 read-window 닫음. slice2 §2.2. */
            g_reg.acc_energy      = 0u;
            g_measure.curr_energy = 0u;
```

- [ ] **Step 3: `US_CMD_RUN_RELEASE` 래치 추가** — `app_reg_command`의 `US_CMD_RUN_RELEASE` 분기, source-matched 블록에서 `g_reg.last_amp = g_reg.max_amp;` 줄 다음에 추가:

```c
            g_reg.last_energy   = g_measure.curr_energy;   /* stopped-display 미러 (slice2) */
```

- [ ] **Step 4: on-time ceiling 래치 추가** — `app_reg_tick`의 ceiling stop 블록에서 `g_reg.last_amp = g_reg.max_amp;` 줄(주석 `/* same latch as release */` 옆) 다음에 추가:

```c
                g_reg.last_energy   = g_measure.curr_energy;   /* slice2 last_energy 래치 */
```

- [ ] **Step 5: `reg_publish_measure` 누산 + last 미러** — `reg_publish_measure`에서 `max_power` 갱신 블록

```c
    g_measure.curr_power = active ? g_reg.adc_scaled_value : 0u;
    if (active && (g_reg.adc_scaled_value > g_reg.max_power)) {
        g_reg.max_power = g_reg.adc_scaled_value;
    }
```

다음에 에너지 누산을 추가(바로 아래 줄에 삽입):

```c
    /* 에너지 적분: active면 curr_power를 acc에 누산(2ms publish cadence) ->
     * curr_energy = acc/250 (samd20 main.c:434-436 구조). idle엔 curr_power=0이라
     * 누산 정지. EXIT 판정은 weld FSM(US_CYCLE)만, 누산/표시는 모든 run 보편. slice2 §5. */
    if (active) {
        g_reg.acc_energy += g_measure.curr_power;
        g_measure.curr_energy = reg_energy_from_acc(g_reg.acc_energy);
    }
```

그리고 `g_measure.last_amp = g_reg.last_amp;` 줄 다음에 추가:

```c
    g_measure.last_energy = g_reg.last_energy;
```

- [ ] **Step 6: 헤더 포함 확인** — `fw/src/app_reg.c` 상단 include에 `#include "app_reg_calc.h"`가 이미 있는지 확인(없으면 추가). (기존에 `reg_scale`/`reg_on_time_200m`을 쓰므로 이미 포함돼 있어야 함.)

- [ ] **Step 7: 빌드 sanity (controller 실행)**

Run: `env -u STM32_TOOLCHAIN cmake --build fw/build`
Expected: 0-warning 빌드 성공.

- [ ] **Step 8: 커밋**

```bash
git add fw/src/app_reg.c
git commit -m "feat(weld): app_reg 에너지 누산 + last_energy 래치 (slice2 §5)"
```

---

## Task 4: app_weld 글루 — energy 필드 주입 + fault hook (HAL 결합)

**Files:**
- Modify: `fw/include/app_weld.h` (hook 선언)
- Modify: `fw/src/app_weld.c` (주입 + fault 처리)

- [ ] **Step 1: fault hook 선언** — `fw/include/app_weld.h`의 `app_weld_hook_set_amp` 선언 블록 다음에 추가:

```c
/* WELD backstop abort hook — energy_ctrl 모드에서 limit_out_time 안에 에너지
 * 미도달 시 1회. slice2: mon 로그만(저에너지=불량 용접 표시). 후속 에러 슬라이스가
 * SYS_ERROR | ERR_OVTIME + LCD 에러 표시로 배선. spec §7. */
void app_weld_hook_fault(void);
```

- [ ] **Step 2: fault hook 구현** — `fw/src/app_weld.c`의 `app_weld_hook_set_amp` 함수 정의 다음에 추가:

```c
void app_weld_hook_fault(void)
{
    mon_printf("[weld] fault: energy timeout (backstop abort)\r\n");
}
```

- [ ] **Step 3: `weld_in_t` 주입 확장** — `fw/src/app_weld.c`의 `app_weld_tick`에서 `weld_in_t in = { ... .output_power = cfg->output_power, };` 초기화에 4필드를 추가:

```c
    weld_in_t in = {
        .start             = s_start_pending,
        .run_mode          = cfg->run_mode,
        .limit_delay_time1 = cfg->limit_delay_time1,
        .limit_delay_time2 = cfg->limit_delay_time2,
        .limit_delay_time3 = cfg->limit_delay_time3,
        .output_power      = cfg->output_power,
        .energy_ctrl       = cfg->energy_ctrl ? 1u : 0u,
        .limit_energy      = cfg->limit_energy,
        .limit_out_time    = cfg->limit_out_time,
        .curr_energy       = app_reg_measure()->curr_energy,
    };
```

- [ ] **Step 4: `weld_fault` 처리** — `app_weld_tick`에서 `if (out.weld_stop) { app_reg_command(US_CMD_RUN_RELEASE, (uint8_t)US_CYCLE); }` 블록 다음에 추가:

```c
    if (out.weld_fault) {
        app_weld_hook_fault();
    }
```

- [ ] **Step 5: 포함 헤더 확인** — `app_weld.c`가 `app_reg_measure()`를 호출하므로 `#include "app_reg.h"`(또는 `app_lcd.h` 경유 lcd_measure_t)가 필요하다. 기존에 `app_reg.h`를 포함(이미 `app_reg_command` 사용)하고 있으나 `app_reg_measure` 선언이 `app_reg.h`에 있는지 확인. 없으면 `lcd_measure_t`/`app_reg_measure` 선언이 있는 헤더 포함 추가.

- [ ] **Step 6: 빌드 sanity (controller 실행)**

Run: `env -u STM32_TOOLCHAIN cmake --build fw/build`
Expected: 0-warning 빌드 성공.

- [ ] **Step 7: 커밋**

```bash
git add fw/include/app_weld.h fw/src/app_weld.c
git commit -m "feat(weld): 글루 energy 필드 주입 + fault hook (slice2 §7)"
```

---

## Task 5: 통합 검증 (host 전 스위트 + 펌웨어 0-warning + 사이즈)

**Files:** (없음 — 검증 전용)

- [ ] **Step 1: 호스트 전 스위트 (controller 실행)**

Run: `make -C fw/test test`
Expected: 4스위트 전부 PASS — `reg_calc`(에너지 헬퍼 포함) / `modbus_core` / `tcp_frame` / `weld_fsm`(슬라이스1 8 + 슬라이스2 3).

- [ ] **Step 2: 펌웨어 클린 빌드 0-warning (controller 실행)**

Run: `env -u STM32_TOOLCHAIN cmake -S fw -B fw/build -G Ninja && env -u STM32_TOOLCHAIN cmake --build fw/build`
Expected: 0-warning 성공.

- [ ] **Step 3: 사이즈 확인 (controller 실행)**

Run: `arm-none-eabi-size fw/build/gds_us_ctrl.elf`
Expected: FLASH/RAM 여유 정상(128KB/32KB 한계 내; slice1 대비 소폭 증가).

- [ ] **Step 4: 통합 cpp-reviewer (subagent-driven 2-stage 리뷰)**

전 변경(Task 1~4)에 대해 spec 준수 + cpp-reviewer. CRIT/HIGH 0 확인. 코멘트 반영 후 재확인.

> 이후 HW 회귀확인(보드)·머지·태그는 본 plan 범위 밖 — `finishing-a-development-branch`에서. HW 게이트: spec §10(직접-초음파 무회귀 + DISP_ENERGY/VAR_ENERGY 점등 + energy_ctrl=ON 직접 START가 ceiling으로 종료 = deviation 확인). 에너지-exit 사이클 E2E는 슬라이스4, 절대 divisor는 6b.

---

## Self-Review (작성자 점검)

**1. Spec coverage:**
- §2 Option A(누산 app_reg) → Task 3. ✅
- §2.2 run-start 리셋 공짜 → Task 3 Step 2. ✅
- §3.2 comp_time/temp_time 디커플 → Task 2 Step 4. ✅
- §3.3 WELD energy-exit + backstop abort → Task 2 Step 6. ✅
- §4 순수 헬퍼 + host-test → Task 1. ✅
- §5 app_reg 4지점(필드/START/RELEASE/ceiling/publish/last 미러) → Task 3 Step 1~5. ✅
- §6 deviation(weld-only) → 코드상 자연(energy-exit는 FSM/US_CYCLE만), 검증 = Task 5 노트 + HW. ✅
- §7 fault hook → Task 4. ✅
- §9 host 테스트(FSM 4 + 헬퍼 2) → Task 1·2. ✅ (슬라이스1 회귀는 기존 8 테스트가 energy_ctrl=0으로 커버.)
- §10 빌드/검증 게이트 → Task 5. ✅

**2. Placeholder scan:** 모든 step에 실제 코드/명령 포함. "TBD/TODO/적절히" 없음. ✅

**3. Type consistency:**
- `reg_energy_from_acc(uint32_t)->uint32_t` — Task 1 선언/구현/Task 3 사용 일치. ✅
- `weld_in_t` 필드명(`energy_ctrl`/`curr_energy`/`limit_energy`/`limit_out_time`) — Task 2 정의 ↔ Task 4 주입 일치. ✅
- `weld_out_t.weld_fault` — Task 2 정의 ↔ Task 2 emit ↔ Task 4 처리 일치. ✅
- `g_reg.acc_energy`/`g_reg.last_energy`(uint32) — Task 3 전 지점 일치. ✅
- `cfg->energy_ctrl`(bool)/`limit_energy`(u32)/`limit_out_time`(u16) — app_config.h와 일치(주입 시 energy_ctrl은 `?1u:0u`로 uint8 변환). ✅

---

*작성: 2026-06-14 weld-cycle 슬라이스2. spec = `docs/superpowers/specs/2026-06-14-stage-weld-cycle-slice2-energy-design.md`. 다음 = subagent-driven-development.*
