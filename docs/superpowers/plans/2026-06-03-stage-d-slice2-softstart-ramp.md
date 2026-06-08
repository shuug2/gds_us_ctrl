# Stage D slice 2a — 상태머신 + soft-start 램프 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** M16의 부팅 soft-start(출력을 ~4s 누진 상승 후 정상 레귤레이션으로 핸드오프)를 STM32 슈퍼루프에 compute로 흡수한다. OSC 물리 출력은 deferred(slice 1과 동일).

**Architecture:** slice-1 구조 미러(Approach A). 순수 `reg_ramp_level`(10-rung, 호스트 테스트)을 `app_reg_calc`에 추가하고, `app_reg.c`에 `g_main_state`/`ramp_counter` 상태 + ~10ms cadence + `sel` MUX(state==1이면 램프, 아니면 slice-1 scale)를 얹는다. `sel`이 deferred 출력 band + LCD bar(curr_power) + us_run_status를 공통으로 구동.

**Tech Stack:** C11, arm-none-eabi-gcc(STM32F410 HAL), CMake/Ninja, 호스트 gcc 단위테스트(`fw/test`).

**Spec:** `docs/superpowers/specs/2026-06-02-stage-d-slice2-softstart-ramp-design.md`
**Build env:** `$STM32_TOOLCHAIN` stale → 모든 cmake에 `env -u STM32_TOOLCHAIN` 필수.

---

## File Structure

| 파일 | 책임 | 변경 |
|------|------|------|
| `fw/include/app_reg_calc.h` | 순수 계산 계약 | `reg_ramp_level` 선언 추가 |
| `fw/src/app_reg_calc.c` | 순수 계산 (HAL-free) | `reg_ramp_level` 구현 추가 |
| `fw/test/test_app_reg_calc.c` | 호스트 단위테스트 | `test_reg_ramp_level` 추가 |
| `fw/include/app_lcd.h` | LCD 계약 | `us_run_status` 값 enum `{US_IDLE,US_RUNNING}` 추가 |
| `fw/src/app_lcd_disp.c` | LCD 렌더 | 로컬 `#define US_IDLE 0u` 제거(헤더 값 사용) |
| `fw/src/app_reg.c` | 레귤레이션 상태/cadence/발행 | 상태머신+램프 cadence+sel MUX+us_run_status+REG_TRACE |

`main.c`/`app.c`/`board.c`/`adc1.*`/`fw/test/Makefile` 무변경.

---

## Task 1: 순수 `reg_ramp_level` (호스트 TDD)

**Files:**
- Test: `fw/test/test_app_reg_calc.c`
- Modify: `fw/include/app_reg_calc.h`, `fw/src/app_reg_calc.c`

- [ ] **Step 1: 실패하는 테스트 작성**

`fw/test/test_app_reg_calc.c`에서 `test_table_values` 함수 정의 뒤(63행 `int main` 앞)에 추가:

```c
/* slice 2a soft-start ramp: 10-rung threshold ladder (M16 app_0x1226 recon
 * :249-258). thresholds {41,81,...,401}; level = thermometer fill * 128,
 * saturating at full (1024) from rung 7 (counter 281). Monotone non-decreasing. */
static void test_reg_ramp_level(void) {
    CHECK_EQ(reg_ramp_level(0),   128);    /* rung 0 (counter < 41) */
    CHECK_EQ(reg_ramp_level(40),  128);    /* rung 0 upper edge */
    CHECK_EQ(reg_ramp_level(41),  256);    /* rung 1 onset */
    CHECK_EQ(reg_ramp_level(80),  256);    /* rung 1 upper edge */
    CHECK_EQ(reg_ramp_level(240), 768);    /* rung 5 (201..240) */
    CHECK_EQ(reg_ramp_level(280), 896);    /* rung 6 upper edge (241..280) */
    CHECK_EQ(reg_ramp_level(281), 1024);   /* rung 7: first full (g_019F=0xFF) */
    CHECK_EQ(reg_ramp_level(400), 1024);   /* rung 9 (361..400) */
    CHECK_EQ(reg_ramp_level(401), 1024);   /* >= 401: full (caller hands off) */
    CHECK_EQ(reg_ramp_level(5000),1024);   /* clamp above */
    /* monotone non-decreasing across the whole counter range */
    for (uint16_t c = 1; c <= 420; c++) {
        if (reg_ramp_level(c) < reg_ramp_level((uint16_t)(c - 1))) {
            printf("FAIL reg_ramp_level not monotone at c=%u\n", (unsigned)c);
            failures++;
        }
    }
}
```

그리고 `main()`의 `test_table_values();` 다음 줄에 호출 추가:

```c
    test_table_values();
    test_reg_ramp_level();
```

- [ ] **Step 2: 테스트 실행 — 실패 확인**

Run: `make -C fw/test test`
Expected: 컴파일/링크 FAIL — `reg_ramp_level` 미선언/undefined reference.

- [ ] **Step 3: 선언 + 구현 추가**

`fw/include/app_reg_calc.h`에서 `reg_output_level` 선언 뒤에 추가:

```c
/* Soft-start ramp (slice 2a): counter (0..401, ~10 ms/step) -> scaled-domain
 * output setpoint. 10-rung thermometer fill of M16 app_0x1226 (recon :249-258),
 * scaled as fill_bits*128, saturating at full (1024) from rung 7. The exact
 * pattern bytes (g_019F 2nd stage / PORTC bit) -> OSC mapping is deferred
 * (B-SEAM), same as the lookup C-LADDER. Monotone non-decreasing. */
uint16_t reg_ramp_level(uint16_t counter);
```

`fw/src/app_reg_calc.c` 끝(파일 마지막 함수 뒤)에 추가:

```c
uint16_t reg_ramp_level(uint16_t counter)
{
    /* M16 app_0x1226 rung thresholds (recon :249-258); per-rung level =
     * thermometer fill (g_019F popcount = rung+1) * 128, saturating at 1024
     * (full byte 0xFF) from rung 7. Exact pattern -> OSC deferred (B-SEAM). */
    static const uint16_t thr[10] = {41u,81u,121u,161u,201u,241u,281u,321u,361u,401u};
    static const uint16_t lvl[10] = {128u,256u,384u,512u,640u,768u,896u,1024u,1024u,1024u};
    for (uint8_t i = 0u; i < 10u; i++) {
        if (counter < thr[i]) {
            return lvl[i];
        }
    }
    return 1024u;   /* counter >= 401: full; caller transitions state to 0 */
}
```

- [ ] **Step 4: 테스트 실행 — 통과 확인**

Run: `make -C fw/test test`
Expected: `all checks PASSED`

- [ ] **Step 5: 커밋**

```bash
git add fw/test/test_app_reg_calc.c fw/include/app_reg_calc.h fw/src/app_reg_calc.c
git commit -m "feat(stage-d): soft-start ramp pure compute (reg_ramp_level) + host tests"
```

---

## Task 2: LCD `us_run_status` 값 enum 통합

**Files:**
- Modify: `fw/include/app_lcd.h`, `fw/src/app_lcd_disp.c`

- [ ] **Step 1: 헤더에 enum 추가**

`fw/include/app_lcd.h`에서 `us_cmd_t` enum 정의(`} us_cmd_t;`, 61행) 바로 뒤에 추가:

```c
/* us_run_status values. slice 2a fills US_RUNNING whenever the regulation loop
 * is active; the command-source taxonomy (REMOTE/TOUCH/COMM) is slice 2b.
 * disp gate = (us_run_status != US_IDLE). */
enum { US_IDLE = 0, US_RUNNING = 1 };
```

- [ ] **Step 2: disp의 로컬 정의 제거**

`fw/src/app_lcd_disp.c` 43행의 로컬 정의를 삭제 (헤더 값 사용):

```c
#define US_IDLE  0u
```

삭제 후 `app_lcd.h`가 이 파일에 이미 포함돼 있어 `US_IDLE`(=0)는 헤더에서 해소된다. 158행 `(m->us_run_status != US_IDLE)`은 그대로.

- [ ] **Step 3: 펌웨어 빌드 — 0-warning 확인**

Run: `env -u STM32_TOOLCHAIN cmake --build fw/build`
Expected: 0 warning / 0 error로 링크 완료 (US_IDLE 중복정의·미해소 없음).

- [ ] **Step 4: 커밋**

```bash
git add fw/include/app_lcd.h fw/src/app_lcd_disp.c
git commit -m "refactor(stage-d): hoist US_IDLE/US_RUNNING to app_lcd.h contract"
```

---

## Task 3: `app_reg.c` 상태머신 + 램프 cadence + sel MUX + 발행

**Files:**
- Modify: `fw/src/app_reg.c`

- [ ] **Step 1: 상수 + 상태 필드 추가**

`fw/src/app_reg.c`에서 `#define ADC_NORM_SHIFT 2u` 줄 뒤에 추가:

```c
#define REG_RAMP_MS      10u   /* M16 Timer1 0xFFB1 ~10.1 ms ramp cadence (cad-C8) */
#define RAMP_DONE_COUNT  401u  /* counter >= 401 (0x191) -> state 0 (verified §2.1) */
```

`reg_state_t` 구조체에서 `uint32_t prev_ms;` 줄 바로 앞에 3개 필드 추가:

```c
    uint8_t  main_state;              /* 1 = soft-start ramp, 0 = lookup regulation */
    uint16_t ramp_counter;           /* 0..401, advances ~every 10 ms while state==1 */
    uint32_t prev_ramp_ms;           /* 10 ms ramp cadence gate */
```

- [ ] **Step 2: `app_reg_init`에서 상태 초기화**

`app_reg_init`의 `g_reg.prev_ms = sys_tick_get_ms();` 줄을 다음 두 줄로 교체:

```c
    g_reg.prev_ms      = sys_tick_get_ms();
    g_reg.prev_ramp_ms = g_reg.prev_ms;
    g_reg.main_state   = 1u;          /* boot into soft-start (M16 init=1, @0x1B8A) */
```

(`ramp_counter`는 `memset`으로 0.)

- [ ] **Step 3: `reg_publish_measure`에 us_run_status 추가**

`reg_publish_measure`에서 `g_measure.last_power = g_reg.adc_scaled_value;` 줄 뒤에 추가:

```c
    /* slice 2a: system is active from boot (no stop command yet = slice 2b). */
    g_measure.us_run_status = (uint8_t)US_RUNNING;
```

(`US_RUNNING`은 `app_reg.h`가 include하는 `app_lcd.h`에서 해소됨.)

- [ ] **Step 4: `app_reg_tick` — 램프 cadence + sel MUX**

`app_reg_tick` 본문을 아래로 교체 (REG_TRACE 블록 포함):

```c
void app_reg_tick(void)
{
    uint32_t now = sys_tick_get_ms();

    /* ~10 ms soft-start ramp cadence (M16 Timer1 0xFFB1 equiv, cad-C8). The
     * counter advances only while state==1; one-way handoff to the lookup
     * regulation at RAMP_DONE_COUNT (M16 @0x137C). */
    if ((uint32_t)(now - g_reg.prev_ramp_ms) >= REG_RAMP_MS) {
        g_reg.prev_ramp_ms = now;
        if (g_reg.main_state == 1u) {
            g_reg.ramp_counter++;
            if (g_reg.ramp_counter >= RAMP_DONE_COUNT) {
                g_reg.main_state = 0u;
            }
        }
    }

    if ((uint32_t)(now - g_reg.prev_ms) < REG_TICK_MS) {
        return;
    }
    g_reg.prev_ms = now;

    reg_acquire_step();

    /* Output setpoint MUX (spec §3.1): soft-start ramp while state==1, else the
     * slice-1 scale of the latest ch0_avg. state==0 path == slice 1 verbatim. */
    uint16_t sel = (g_reg.main_state == 1u)
                 ? reg_ramp_level(g_reg.ramp_counter)
                 : reg_scale(g_reg.ch0_avg);
    g_reg.adc_scaled_value = sel;
    g_reg.band             = reg_output_level(sel);

    reg_publish_measure();

#ifdef REG_TRACE
    if ((uint32_t)(now - g_reg.trace_ms) >= REG_TRACE_MS) {
        g_reg.trace_ms = now;
        mon_printf("[reg] st=%u rc=%u ch0=%u ch1=%u sel=%u band=%u\r\n",
                   (unsigned)g_reg.main_state, (unsigned)g_reg.ramp_counter,
                   (unsigned)g_reg.ch0_avg, (unsigned)g_reg.ch1_avg,
                   (unsigned)g_reg.adc_scaled_value, (unsigned)g_reg.band);
    }
#endif
}
```

- [ ] **Step 5: 펌웨어 빌드 — 0-warning 확인**

Run: `env -u STM32_TOOLCHAIN cmake --build fw/build`
Expected: 0 warning / 0 error. `arm-none-eabi-size fw/build/gds_us_ctrl.elf`로 FLASH/RAM 약간 증가 확인(이상 없음).

- [ ] **Step 6: 커밋**

```bash
git add fw/src/app_reg.c
git commit -m "feat(stage-d): soft-start state machine + ramp cadence + sel MUX (slice 2a)"
```

---

## Task 4: On-target HW verify (REG_TRACE + LCD) — 수동/인터랙티브

slice 1 Task 6과 동일한 패턴(임베디드 통합 테스트). **전압 주입 불필요** — 부팅 soft-start가 자체적으로 출력 setpoint를 누진하므로 trace + LCD bar로 검증.

- [ ] **Step 1: 트레이스 이미지 빌드**

> ⚠️ `-DCMAKE_C_FLAGS`는 캐시를 덮어쓰므로 CPU 플래그를 함께 전달해야 함(슬라이스 1에서 정정된 명령).

```bash
cd fw
env -u STM32_TOOLCHAIN cmake -B build-trace -G Ninja \
  -DCMAKE_C_FLAGS="-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard -DREG_TRACE"
env -u STM32_TOOLCHAIN cmake --build build-trace
```
Expected: 0-warning, `build-trace/gds_us_ctrl.elf`.

- [ ] **Step 2: 플래시 + 모니터 (단일 fd)**

```bash
openocd -f fw/openocd/stm32f410.cfg -c "program fw/build-trace/gds_us_ctrl.elf verify reset exit"
DEV=$(ls /dev/cu.usbserial-* 2>/dev/null | head -1)
{ stty -f "$DEV" 115200 cs8 -parenb -cstopb raw -echo; cat; } < "$DEV" > /tmp/reg-mon.log &
```

- [ ] **Step 3: 관측 — 합격 기준**

`tr -d '\000' < /tmp/reg-mon.log | tr -s ' ' | grep '\['` 로 정리해 읽기:

- [ ] 부팅 직후 `[reg] st=1 rc=… sel=… band=…` — `st=1`(soft-start), `rc`(ramp_counter)가 증가, `sel`이 128→1024로, `band`이 ~18→0으로 **~4s에 걸쳐 단조 상승/하강**.
- [ ] ~4s 후 `st=0`으로 전환, 이후 `sel = reg_scale(ch0_avg)`(slice-1 lookup 거동: idle이면 ch0=0→sel=0→band=21).
- [ ] **LCD 출력 bar**가 부팅 시 상승했다가 핸드오프 후 실측값(idle 0)으로 복귀 — 육안.
- [ ] **us_run_status running 아이콘** 패널 표시(부팅 후 계속).
- [ ] 무회귀: `[boot]/[lcd]/[cfg]` 배너 정상 + LCD 네비 락업 없음 + (slice 1) PB2/PB10/PB14 idle-HIGH 유지.

- [ ] **Step 4: 트레이스 빌드 정리**

```bash
rm -rf fw/build-trace   # git-ignored; 커밋 안 함
```

커밋 없음(트레이스 빌드는 의도적으로 비커밋).

> **검증 결과 (2026-06-08) — PASS (compute), 합격기준 2건 정정.** STLINK V3 플래시 → USART6 mon. **부팅 램프 trace (2회 재현)**:
> ```
> st=1 rc=0   sel=128  band=18      st=1 rc=200 sel=640  band=8
> st=1 rc=50  sel=256  band=16      st=1 rc=250 sel=896  band=3
> st=1 rc=100 sel=384  band=13      st=1 rc=300 sel=1024 band=0   (포화)
> st=1 rc=150 sel=512  band=11      st=1 rc=400 sel=1024 band=0
> st=0 rc=401 sel=18 band=20  ← 핸드오프, 이후 idle = sel=0/band=21 (slice-1 verbatim)
> ```
> - ✅ sel 128→1024 단조 / band 18→0 단조 / ~4s (rc 401×10ms) / rc=401 핸드오프 / st=0 slice-1 무회귀 / 배너·`[boot]/[lcd]/[cfg]` 정상.
> - ✅ LCD **power 숫자(VAR_POWER)** 램프 추종 상승 — 전압주입 없이 램프 패널 도달 입증.
> - ⚠ **"LCD 출력 bar 상승" / "running 아이콘"** 두 육안 항목은 spec 디스플레이 계층 오독에 따른 잘못된 예측 — 정정: 출력 바 = `curr_amp` amplitude 바(idle 정지 by-design), `ICON_RUN` = 명령 FSM(2b) 소속. **펌웨어 수정 불필요.** 상세 = spec §8.2 검증결과 노트.

---

## Self-Review (작성 후 점검 결과)

1. **Spec coverage:** §2 상태머신→Task 3 Step 2/4; §2.2 cadence→Task 3 Step 1/4; §2.3 per-rung→Task 1; §3.1 sel MUX→Task 3 Step 4; §4 reg_ramp_level→Task 1; §5 cadence 10ms→Task 3 Step 1/4; §6 LCD(curr_power=sel/us_run_status)→Task 3 Step 3/4 + Task 2; §7 파일→전 Task; §8.1 호스트테스트→Task 1; §8.2 HW→Task 4. 누락 없음.
2. **Placeholder scan:** 모든 코드 스텝에 실제 코드. TBD/TODO 없음.
3. **Type consistency:** `reg_ramp_level(uint16_t)->uint16_t` (Task 1 선언/구현/테스트/Task 3 호출 일치); `US_RUNNING`/`US_IDLE` (Task 2 정의 / Task 3 사용 / disp 사용 일치); `main_state`(uint8_t)/`ramp_counter`(uint16_t)/`prev_ramp_ms`(uint32_t) Task 3 내부 일관; `g_measure.us_run_status`(uint8_t, app_lcd.h:113) 일치.

## Execution notes
- Task 1만 host-TDD(순수함수). Task 2/3는 build-gated(통합), Task 4는 보드+사용자.
- 각 Task 독립 커밋(frequent commits).
- 통과 후: changelog/RESUME/NEXT_STEPS "done" + cpp-reviewer 1회 → finishing-a-development-branch(머지/PR) + 태그.
- 출력단(B-SEAM)·명령 FSM·overload·blink·6b calibration은 여전히 DEFERRED.
