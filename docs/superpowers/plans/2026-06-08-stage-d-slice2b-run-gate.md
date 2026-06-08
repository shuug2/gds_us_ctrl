# Stage D slice 2b — RUN 명령 게이트 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 터치 RUN을 momentary hold-to-run으로 게이트해 regulation core를 명령 구동하고, 부팅 auto-run을 IDLE로 전환한다 (SAMD20 명령 FSM의 run 절반 흡수, 내부 호출로 regulation core와 연결).

**Architecture:** `app_lcd_hook_us_command` 스텁을 신규 `app_reg_command(us_cmd_t)`로 라우팅 → `app_reg.c`의 run FSM이 `us_run_status`(소스 taxonomy)·soft-start re-arm·max/last latch를 소유하고 `lcd_measure_t` 발행. `app_lcd_disp.c`는 `us_run_status` 엣지로 ICON_RUN 렌더. OSC 물리구동·SEEK/RESET 효과·overload·weld-cycle = 범위 밖.

**Tech Stack:** STM32F410 베어메탈 C, arm-none-eabi-gcc, CMake/Ninja, 슈퍼루프 + SysTick(ms). 호스트 단위테스트(`fw/test/`)는 순수함수 전용 — 본 슬라이스는 순수함수 추가 없음(§Testing).

**Spec:** `docs/superpowers/specs/2026-06-08-stage-d-slice2b-run-gate-design.md`

---

## File Structure

| 파일 | 책임 | 본 슬라이스 변경 |
|------|------|------------------|
| `fw/include/app_lcd.h` | LCD 계약(enum/struct/hook decl) | `us_run_status` enum → 4-값 taxonomy |
| `fw/include/app_reg.h` | regulation core 공개 API | `app_reg_command` 선언 |
| `fw/src/app_reg.c` | regulation compute + run FSM + measure 소유 | 신규 FSM 필드·`app_reg_command`·부팅 IDLE·sel MUX(idle→0)·max/last·publish·trace |
| `fw/src/app_lcd.c` | LCD 상태 소유 + 훅 | `app_lcd_hook_us_command` → `app_reg_command` 라우팅 |
| `fw/src/app_lcd_disp.c` | 주기 디스플레이 step machine | ICON_RUN 엣지 렌더 |

**무변경**: `main.c`, `app.c`, `board.c`, `adc1.*`, `app_reg_calc.*`, `fw/test/*`.

**TDD 노트**: slice 1/2a는 순수함수(`reg_scale`/`reg_ramp_level`)만 호스트 테스트했다(경계값 다수 = 테스트 가치 큼). slice 2b의 로직은 (a) 2-가드 소스 arbitration + (b) `app_reg.c` static 상태·`g_measure`에 묶인 side effect로, 순수함수로 떼면 `us_cmd_t`(app_lcd.h) 의존이 호스트 빌드로 새어 결합 비용이 가치를 초과한다(YAGNI + spec §8.1 결정). 따라서 **신규 RED/GREEN 호스트 테스트 없음** — 게이트 = 빌드 0-warning + 기존 호스트 테스트 무회귀 + Task 3 실보드 HW 검증(풍부한 표면: mon trace + ICON_RUN + VAR_POWER). 이는 slice 1/2a가 실제 합격시킨 방식과 동일.

---

## Task 1: Run FSM + 소스 taxonomy + 명령 라우팅

활성 run일 때만 regulation이 구동되도록 게이트하고, 터치 START/RELEASE를 FSM 전환으로 연결한다. enum 확장과 `US_RUNNING` 제거는 결합돼 있어(값 충돌) 한 task에서 함께 처리 — 모든 편집 후 단일 빌드로 green.

**Files:**
- Modify: `fw/include/app_lcd.h:63-66` (us_run_status enum)
- Modify: `fw/include/app_reg.h` (app_reg_command 선언)
- Modify: `fw/src/app_reg.c` (reg_state_t 필드, init, command, tick MUX/gate, publish, trace)
- Modify: `fw/src/app_lcd.c:33-36` (hook 라우팅)

- [ ] **Step 1: `us_run_status` enum을 4-값 taxonomy로 확장**

`fw/include/app_lcd.h` 의 다음 블록(`:63-66`):
```c
/* us_run_status values. slice 2a fills US_RUNNING whenever the regulation loop
 * is active; the command-source taxonomy (REMOTE/TOUCH/COMM) is slice 2b.
 * disp gate = (us_run_status != US_IDLE). */
enum { US_IDLE = 0, US_RUNNING = 1 };
```
을 다음으로 교체:
```c
/* us_run_status command-source taxonomy (samd20 main.c:110-113). slice 2b run FSM
 * publishes US_TOUCH while a touch-started run is active, US_IDLE when stopped.
 * REMOTE (physical switch / IPC) and COMM (Modbus) are reserved for later slices.
 * disp gate = (us_run_status != US_IDLE). */
enum { US_IDLE = 0, US_REMOTE = 1, US_TOUCH = 2, US_COMM = 3 };
```

- [ ] **Step 2: `app_reg_command` 선언 추가**

`fw/include/app_reg.h` 의 `app_reg_measure` 선언 블록 바로 뒤(파일 끝)에 추가:
```c
/* Route a panel/comm ultrasonic command into the run FSM (slice 2b). Called from
 * app_lcd_hook_us_command. START/RUN_RELEASE gate the run; SEEK/RESET = no-op this
 * slice (deferred, spec §9). Superloop single-thread — mutates FSM state in place.
 * us_cmd_t is visible via the already-included app_lcd.h. */
void app_reg_command(us_cmd_t cmd);
```

- [ ] **Step 3: `reg_state_t` 에 run FSM 필드 추가**

`fw/src/app_reg.c` 의 `reg_state_t` 내 `main_state`/`ramp_counter`/`prev_ramp_ms` 블록:
```c
    uint8_t  main_state;              /* 1 = soft-start ramp, 0 = lookup regulation */
    uint16_t ramp_counter;           /* 0..401, advances ~every 10 ms while state==1 */
    uint32_t prev_ramp_ms;           /* 10 ms ramp cadence gate */
```
을 다음으로 교체(필드 3개 추가):
```c
    uint8_t  main_state;              /* 1 = soft-start ramp, 0 = lookup regulation */
    uint16_t ramp_counter;           /* 0..401, advances ~every 10 ms while running */
    uint32_t prev_ramp_ms;           /* 10 ms ramp cadence gate */

    uint8_t  us_run_status;          /* slice 2b: US_IDLE/REMOTE/TOUCH/COMM (FSM owns) */
    uint16_t max_power;              /* running peak of sel during the active run */
    uint16_t last_power;             /* peak latched on stop (us_off, samd20 4180) */
```

- [ ] **Step 4: `app_reg_init` 부팅 IDLE 전환 (auto-run 폐기)**

`fw/src/app_reg.c` 의 `app_reg_init`:
```c
void app_reg_init(void)
{
    memset(&g_reg, 0, sizeof(g_reg));
    memset(&g_measure, 0, sizeof(g_measure));
    adc1_init();
    g_reg.prev_ms      = sys_tick_get_ms();
    g_reg.prev_ramp_ms = g_reg.prev_ms;
    g_reg.main_state   = 1u;          /* boot into soft-start (M16 init=1, @0x1B8A) */
}
```
을 다음으로 교체(`main_state=1` 줄 제거 → memset이 IDLE/0 보장):
```c
void app_reg_init(void)
{
    memset(&g_reg, 0, sizeof(g_reg));
    memset(&g_measure, 0, sizeof(g_measure));
    adc1_init();
    g_reg.prev_ms      = sys_tick_get_ms();
    g_reg.prev_ramp_ms = g_reg.prev_ms;
    /* slice 2b: boot IDLE (us_run_status=0, main_state=0 via memset). The output
     * runs only on a RUN command (app_reg_command); slice-2a auto-run retired. */
}
```

- [ ] **Step 5: `app_reg_command` 구현**

`fw/src/app_reg.c` 의 `app_reg_init` 함수 바로 뒤에 추가:
```c
void app_reg_command(us_cmd_t cmd)
{
    switch (cmd) {
    case US_CMD_START:
        /* Touch RUN press: re-arm the soft-start ramp only on a real idle->run edge.
         * M16's ramp is edge-driven on M_START OFF->ON; a repeated/auto-repeat press
         * while already running must NOT restart it (else ramp_counter resets and
         * never reaches 401 -> no handoff to lookup -> output stuck at ramp start).
         * samd20 gated on != US_REMOTE because its ramp was already edge-driven
         * downstream; for a TOUCH-only slice == US_IDLE is strictly safer (can't
         * restart an active run). REMOTE/COMM source arbitration + re-press energy
         * reset = later slices. */
        if (g_reg.us_run_status == (uint8_t)US_IDLE) {
            g_reg.us_run_status = (uint8_t)US_TOUCH;
            g_reg.main_state    = 1u;
            g_reg.ramp_counter  = 0u;
            g_reg.max_power     = 0u;
        }
        break;
    case US_CMD_RUN_RELEASE:
        /* Touch RUN release: only a touch-started run stops here (samd20
         * main.c:3699 / us_off main.c:4180). Latch last_power = running peak. */
        if (g_reg.us_run_status == (uint8_t)US_TOUCH) {
            g_reg.last_power    = g_reg.max_power;
            g_reg.us_run_status = (uint8_t)US_IDLE;
        }
        break;
    case US_CMD_SEEK:
    case US_CMD_RESET:
    default:
        /* Regulation effect deferred (spec §9): the input layer already does the
         * RESET icon/page restore; SEEK/RESET drive is B-SEAM. */
        break;
    }
#ifdef REG_TRACE
    mon_printf("[reg] cmd=%u run=%u\r\n", (unsigned)cmd, (unsigned)g_reg.us_run_status);
#endif
}
```

- [ ] **Step 6: `reg_publish_measure` — max/last/us_run_status 충실 발행 (US_RUNNING 제거)**

`fw/src/app_reg.c` 의 `reg_publish_measure`:
```c
static void reg_publish_measure(void)
{
    /* spec §7: amp + scaled level live; cycle/freq/energy/status stay 0. */
    g_measure.curr_amp   = g_reg.ch0_avg;
    g_measure.curr_power = g_reg.adc_scaled_value;
    g_measure.max_power  = g_reg.adc_scaled_value;
    g_measure.last_power = g_reg.adc_scaled_value;
    /* slice 2a: system is active from boot (no stop command yet = slice 2b). */
    g_measure.us_run_status = (uint8_t)US_RUNNING;
}
```
을 다음으로 교체:
```c
static void reg_publish_measure(void)
{
    /* slice 2b run-gated: curr_power = live setpoint (0 when idle); max_power =
     * running peak during the run; last_power latched on stop (app_reg_command).
     * cycle/freq/energy stay 0 (weld-cycle deferred). */
    uint8_t active = (uint8_t)(g_reg.us_run_status != (uint8_t)US_IDLE);
    g_measure.curr_amp   = g_reg.ch0_avg;
    g_measure.curr_power = active ? g_reg.adc_scaled_value : 0u;
    if (active && (g_reg.adc_scaled_value > g_reg.max_power)) {
        g_reg.max_power = g_reg.adc_scaled_value;
    }
    g_measure.max_power     = g_reg.max_power;
    g_measure.last_power    = g_reg.last_power;
    g_measure.us_run_status = g_reg.us_run_status;
}
```

- [ ] **Step 7: `app_reg_tick` — ramp gate에 활성 조건, sel MUX에 idle→0**

`fw/src/app_reg.c` 의 ramp cadence 블록:
```c
    if ((uint32_t)(now - g_reg.prev_ramp_ms) >= REG_RAMP_MS) {
        g_reg.prev_ramp_ms = now;
        if (g_reg.main_state == 1u) {
            g_reg.ramp_counter++;
            if (g_reg.ramp_counter >= RAMP_DONE_COUNT) {
                g_reg.main_state = 0u;
            }
        }
    }
```
을 다음으로 교체(활성 run 조건 추가):
```c
    if ((uint32_t)(now - g_reg.prev_ramp_ms) >= REG_RAMP_MS) {
        g_reg.prev_ramp_ms = now;
        if ((g_reg.us_run_status != (uint8_t)US_IDLE) && (g_reg.main_state == 1u)) {
            g_reg.ramp_counter++;
            if (g_reg.ramp_counter >= RAMP_DONE_COUNT) {
                g_reg.main_state = 0u;
            }
        }
    }
```
그리고 sel MUX 블록:
```c
    /* Output setpoint MUX (spec §3.1): soft-start ramp while state==1, else the
     * slice-1 scale of the latest ch0_avg. state==0 path == slice 1 verbatim. */
    uint16_t sel = (g_reg.main_state == 1u)
                 ? reg_ramp_level(g_reg.ramp_counter)
                 : reg_scale(g_reg.ch0_avg);
    g_reg.adc_scaled_value = sel;
    g_reg.band             = reg_output_level(sel);
```
을 다음으로 교체(idle→0 추가):
```c
    /* Output setpoint MUX (spec §3.1): idle (no run command) forces 0; while
     * running, soft-start ramp during state==1, else the slice-1 scale of the
     * latest ch0_avg. The running state==0 path == slice 1 verbatim. */
    uint16_t sel;
    if (g_reg.us_run_status == (uint8_t)US_IDLE) {
        sel = 0u;
    } else if (g_reg.main_state == 1u) {
        sel = reg_ramp_level(g_reg.ramp_counter);
    } else {
        sel = reg_scale(g_reg.ch0_avg);
    }
    g_reg.adc_scaled_value = sel;
    g_reg.band             = reg_output_level(sel);
```

- [ ] **Step 8: REG_TRACE 주기 trace에 `run=` 추가**

`fw/src/app_reg.c` 의 REG_TRACE 주기 블록의 `mon_printf` 호출:
```c
        mon_printf("[reg] st=%u rc=%u ch0=%u ch1=%u sel=%u band=%u\r\n",
                   (unsigned)g_reg.main_state, (unsigned)g_reg.ramp_counter,
                   (unsigned)g_reg.ch0_avg, (unsigned)g_reg.ch1_avg,
                   (unsigned)g_reg.adc_scaled_value, (unsigned)g_reg.band);
```
을 다음으로 교체:
```c
        mon_printf("[reg] run=%u st=%u rc=%u ch0=%u ch1=%u sel=%u band=%u\r\n",
                   (unsigned)g_reg.us_run_status, (unsigned)g_reg.main_state,
                   (unsigned)g_reg.ramp_counter, (unsigned)g_reg.ch0_avg,
                   (unsigned)g_reg.ch1_avg, (unsigned)g_reg.adc_scaled_value,
                   (unsigned)g_reg.band);
```

- [ ] **Step 9: 훅을 `app_reg_command`로 라우팅**

`fw/src/app_lcd.c:33-36`:
```c
void app_lcd_hook_us_command(us_cmd_t cmd)
{
    mon_printf("[lcd-hook] us_command=%u\r\n", (unsigned)cmd);
}
```
을 다음으로 교체(mon trace 유지 + 라우팅 추가; `app_reg.h`는 이미 line 6 include):
```c
void app_lcd_hook_us_command(us_cmd_t cmd)
{
    mon_printf("[lcd-hook] us_command=%u\r\n", (unsigned)cmd);
    app_reg_command(cmd);   /* slice 2b: route into the regulation run FSM */
}
```

- [ ] **Step 10: 빌드 0-warning 검증**

Run:
```bash
cd /Users/tknoh/dev/work/gds_us_ctrl/fw
env -u STM32_TOOLCHAIN cmake -B build -G Ninja >/dev/null
env -u STM32_TOOLCHAIN cmake --build build
```
Expected: 경고/에러 0, `gds_us_ctrl.elf` 링크 성공. (게이트: `-Wall -Wextra -Wundef -Wshadow` 무경고.)

- [ ] **Step 11: 호스트 단위테스트 무회귀**

Run:
```bash
make -C /Users/tknoh/dev/work/gds_us_ctrl/fw/test test
```
Expected: `all checks PASSED` (기존 `reg_scale`/`reg_output_level`/`reg_ramp_level` 테스트 — 무변경 확인).

- [ ] **Step 12: 커밋**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
git add fw/include/app_lcd.h fw/include/app_reg.h fw/src/app_reg.c fw/src/app_lcd.c
git commit -m "feat(stage-d): RUN command gate + us_run_status taxonomy (slice 2b)

Touch RUN press/release gates the regulation core via app_reg_command
(routed from app_lcd_hook_us_command). Boot now IDLE (auto-run retired);
START re-arms the soft-start ramp + resets peak, RUN_RELEASE latches
last_power=max_power (us_off equiv). us_run_status holds the source
taxonomy {IDLE,REMOTE,TOUCH,COMM}; US_RUNNING removed. Running lookup
path == slice-1 verbatim. SEEK/RESET = no-op (deferred)."
```

---

## Task 2: ICON_RUN 엣지 렌더

`us_run_status` running-ness 변화 시에만 ICON_RUN을 1회 write(매 4ms step 스팸 방지). app_reg는 DGUS 미접근 — disp가 measure를 읽어 렌더(계층 분리, 주석 `app_lcd_disp.c:39-41` 예고).

**Files:**
- Modify: `fw/src/app_lcd_disp.c:188-194` (app_lcd_disp_step 진입부)

- [ ] **Step 1: step machine 진입부에 ICON_RUN 엣지 검출 추가**

`fw/src/app_lcd_disp.c` 의 `app_lcd_disp_step` 진입부:
```c
void app_lcd_disp_step(void)
{
    static uint8_t s = 0u;       /* not named `step` to avoid -Wshadow vs samd20 param */

    const lcd_measure_t   *m  = app_lcd_measure();
    const lcd_app_state_t *st = app_lcd_state();
    const app_config_t    *cfg = app_lcd_cfg();

    switch (s) {
```
을 다음으로 교체(엣지 트래커 + ICON_RUN write 추가):
```c
void app_lcd_disp_step(void)
{
    static uint8_t s = 0u;       /* not named `step` to avoid -Wshadow vs samd20 param */
    static bool prev_run_on = false;   /* slice 2b: ICON_RUN edge tracker */

    const lcd_measure_t   *m  = app_lcd_measure();
    const lcd_app_state_t *st = app_lcd_state();
    const app_config_t    *cfg = app_lcd_cfg();

    /* slice 2b: drive ICON_RUN on us_run_status running-ness edges (write once on
     * change, not every 4 ms step). samd20 sets ICON_RUN in the sig_run_status edge
     * handler (main.c:4302); here disp renders the FSM state app_reg publishes. */
    bool run_on = (m->us_run_status != US_IDLE);
    if (run_on != prev_run_on) {
        dgus_write_u16(ICON_RUN, run_on ? 1u : 0u);
        prev_run_on = run_on;
    }

    switch (s) {
```

- [ ] **Step 2: 빌드 0-warning 검증**

Run:
```bash
cd /Users/tknoh/dev/work/gds_us_ctrl/fw
env -u STM32_TOOLCHAIN cmake --build build
```
Expected: 경고/에러 0. (`ICON_RUN`/`dgus_write_u16` = `dgus_lcd.h`, `bool` = `<stdbool.h>` — 모두 이미 include.)

- [ ] **Step 3: 호스트 단위테스트 무회귀**

Run:
```bash
make -C /Users/tknoh/dev/work/gds_us_ctrl/fw/test test
```
Expected: `all checks PASSED`.

- [ ] **Step 4: 커밋**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
git add fw/src/app_lcd_disp.c
git commit -m "feat(stage-d): render ICON_RUN on us_run_status edges (slice 2b)

disp_step edge-detects (us_run_status != US_IDLE) and writes ICON_RUN
once per transition (no 4 ms spam). app_reg stays DGUS-free; disp renders
the published run state (port of samd20 sig_run_status icon, main.c:4302)."
```

### Task 2 code-review fix — SYS_PIC_NOW mid-run reset stops the run (spec §4.3)

cpp-reviewer catch (검증됨): `SYS_PIC_NOW`=0(패널 자체 리셋)이 run 중 `app_lcd_init_mode`로 `ICON_RUN=0`을 클리어하나 disp `prev_run_on`은 미동기 → 아이콘 "정지" 고착. 더 위험한 동반 결함: momentary hold-to-run에서 패널 mid-hold 리셋 시 RUN_RELEASE 미도착 → `us_run_status` US_TOUCH 영구 고착(무한구동). **Option A(안전 포스처)**: 재init 시 RUN_RELEASE 발행 → FSM IDLE → 진짜 엣지로 아이콘 재동기 + 무한구동 해소. **Task 2 disp 코드(`ed2093f`) 무변경** — 수정은 `app_lcd_input.c` SYS_PIC_NOW 케이스 한 줄.

**Files:** Modify `fw/src/app_lcd_input.c` (SYS_PIC_NOW 케이스, `case SYS_PIC_NOW:` 블록)

- [ ] **Step 5: SYS_PIC_NOW 재init 블록에 RUN_RELEASE 추가**

`fw/src/app_lcd_input.c` 의 SYS_PIC_NOW 케이스:
```c
    case SYS_PIC_NOW:
        if (data16 == 0 && state->boot_complete &&
            (uint32_t)(sys_tick_get_ms() - state->last_set_page_ms) >= 200u) {
            app_lcd_var_init();
            app_lcd_send_model_str(cfg->model_freq, cfg->model_type);
            app_lcd_init_mode(cfg);
        }
        break;
```
을 다음으로 교체(재init 전 run 정지 — 안전 포스처, spec §4.3):
```c
    case SYS_PIC_NOW:
        if (data16 == 0 && state->boot_complete &&
            (uint32_t)(sys_tick_get_ms() - state->last_set_page_ms) >= 200u) {
            /* Panel self-reset mid-run: the held RUN press is lost and no
             * RUN_RELEASE will arrive, so stop the run (UI lost -> stop the
             * actuator). This also re-syncs ICON_RUN: us_run_status -> IDLE
             * makes the next disp_step see a real edge after init_mode clears
             * the icon (spec §4.3). Harmless when already idle. */
            app_lcd_hook_us_command(US_CMD_RUN_RELEASE);
            app_lcd_var_init();
            app_lcd_send_model_str(cfg->model_freq, cfg->model_type);
            app_lcd_init_mode(cfg);
        }
        break;
```

- [ ] **Step 6: 빌드 0-warning + 호스트 무회귀**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl/fw && env -u STM32_TOOLCHAIN cmake --build build
make -C /Users/tknoh/dev/work/gds_us_ctrl/fw/test test
```
Expected: 빌드 경고 0; `all checks PASSED`.

- [ ] **Step 7: 커밋**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
git add fw/src/app_lcd_input.c
git commit -m "fix(stage-d): stop run on SYS_PIC_NOW mid-run panel reset (slice 2b)

Panel self-reset clears ICON_RUN via init_mode but leaves us_run_status
stuck at US_TOUCH (held-RUN release lost) -> infinite drive + icon reads
stopped while live. Issue US_CMD_RUN_RELEASE in the SYS_PIC_NOW re-init
(UI lost -> stop actuator); the FSM->IDLE edge re-syncs ICON_RUN. disp
edge code unchanged. spec §4.3."
```

---

## Task 3: 실보드 HW 검증

전압 주입 불필요 — 명령 게이트는 mon trace + ICON_RUN + VAR_POWER 숫자로 검증. (slice 2a Task 4 절차 미러; CPU 플래그 누락 주의.)

**전제**: 보드 연결(`ls /dev/cu.usbserial-* /dev/cu.usbmodem*`, mon=`/dev/cu.usbserial-BG02DMWU`@115200, ST-LINK=usbmodem). BOOT0=GND(평범한 reset run).

- [ ] **Step 1: 트레이스 빌드 (CPU 플래그 포함 — 필수)**

Run:
```bash
cd /Users/tknoh/dev/work/gds_us_ctrl/fw
env -u STM32_TOOLCHAIN cmake -B build-trace -G Ninja \
  -DCMAKE_C_FLAGS="-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard -DREG_TRACE"
env -u STM32_TOOLCHAIN cmake --build build-trace
```
Expected: 0-warning. ⚠ CPU 플래그를 함께 안 주면 캐시 덮어써 ARM-mode 빌드 실패(slice 1 발견).

- [ ] **Step 2: 플래시 + 모니터 (단일 fd)**

Run:
```bash
openocd -f /Users/tknoh/dev/work/gds_us_ctrl/fw/openocd/stm32f410.cfg \
  -c "program /Users/tknoh/dev/work/gds_us_ctrl/fw/build-trace/gds_us_ctrl.elf verify reset exit"
DEV=$(ls /dev/cu.usbserial-* | head -1)
{ stty -f "$DEV" 115200 cs8 -parenb -cstopb raw -echo; cat; } < "$DEV" > /tmp/reg-mon.log &
```
읽기: `tr -d '\000' < /tmp/reg-mon.log | tr -s ' ' | grep '\['`

- [ ] **Step 3: 합격 기준 검증**

| # | 동작 | 기대 |
|---|------|------|
| 1 | **부팅** | mon `[reg] run=0 st=0 … sel=0`; 출력 미구동; LCD 배너/네비 정상; ICON_RUN off. **slice-2a 부팅 auto-ramp 없음**(의도). |
| 2 | **RUN press** (패널 RUN 누름·유지) | mon `[lcd-hook] us_command=0` + `[reg] cmd=0 run=2`; 이후 `run=2 st=1` `rc` 0→401 ~4s 단조 + `sel` 128→1024; **ICON_RUN 점등**; VAR_POWER 숫자 peak 추종 상승 → ~4s 후 `st=0`(sel=reg_scale(ch0_avg)). |
| 3 | **RUN release** (RUN 뗌) | mon `us_command=3` + `[reg] cmd=3 run=0`; **ICON_RUN 소등**; `sel=0`; VAR_POWER = last_power(latch peak, 고정). |
| 4 | **re-arm** (다시 press) | `max_power` 0 리셋 → VAR_POWER 0에서 재상승 + `st=1 rc` 0부터 램프 재발화(가시적). |
| 5 | **무회귀** | 활성 `st=0` lookup = slice-1 거동; OSC PB2/PB10/PB14 idle-HIGH(slice 1 6a); LCD 배너/네비/SETUP 정상. |

- ARM-mode 빌드 에러 시: CPU 플래그 누락(Step 1).
- 출력 바(`LV_OUTPUT`)는 amplitude(`curr_amp`) 구동이라 전압주입 없으면 정지 = by-design(slice 2a §8.2). 램프 가시면은 **VAR_POWER 숫자**.

- [ ] **Step 4: 정리 + 결과 기록**

```bash
rm -rf /Users/tknoh/dev/work/gds_us_ctrl/fw/build-trace   # git-ignored, 커밋 안 함
```
검증 결과를 본 plan Task 3 + spec §8.2 + `docs/changelog.md`에 기록(PASS/정정). 통과 → 최종 cpp-reviewer → finishing-a-development-branch(머지/PR) + 태그 `hw-revA_fw-stage-d2b`(또는 합의 태그명).

---

## Out of Scope (후속 / HW-gated)

SEEK/RESET regulation 효과(B-SEAM), overload/ICON_OL/error_status FSM(6b 임계), weld-cycle(RUN_CYL/WELD/HOLD, Stage A I/O), Modbus US_COMM(Stage C), blink(7-seg 부재), OSC 물리구동(B-SEAM), 6b calibration.

---

## Self-Review (작성자 체크)

- **Spec 커버리지**: §3 라우팅=T1S9 / §4 FSM(init·START·RELEASE·SEEK-RESET no-op)=T1S4-5 / §3.1·§4 MUX·gate=T1S7 / §4.1 max/last=T1S5-6 / §5 app_reg_command=T1S2,5 / §6 enum=T1S1, ICON_RUN=T2, VAR_POWER=T1S6(measure)+disp 무변경 / §8.2 HW=T3. **갭 없음**.
- **순수함수 추가 없음** → §8.1 결정대로 신규 호스트 테스트 없음(File Structure TDD 노트에 근거 명시).
- **타입 일관성**: `app_reg_command(us_cmd_t)` 선언(T1S2)·정의(T1S5)·호출(T1S9) 일치. enum 값 `US_IDLE/US_REMOTE/US_TOUCH`(T1S1) 사용처(T1S5-7, T2) 일치. `US_RUNNING` 잔여 참조 2곳(app_lcd.h def, app_reg.c:96) 모두 T1에서 제거.
- **placeholder 없음**: 모든 코드 step 완전한 old→new 블록.
