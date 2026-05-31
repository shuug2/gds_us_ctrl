# Stage D Slice 1 — Ultrasonic Regulation Core Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port the ATmega16 regulation **compute** pipeline (2-ch polled ADC → ×6 scaling → 21-rung lookup) onto the STM32F410, feed live values to the LCD measure provider, and apply the two safe `board.c` OSC-pin fixes — **no physical OSC output driven** (deferred to bench measurement, B-SEAM).

**Architecture:** A HAL-free pure-compute module (`app_reg_calc`) holds the two verified transfer functions and the lookup table and is unit-tested on the host. A thin ADC driver (`adc1`) owns ADC1 + PB0/PB1 analog pins and returns raw 12-bit counts. A regulation state module (`app_reg`) runs on a ~2 ms superloop cadence: it alternates ADC channels, normalizes 12→10-bit, accumulates the 10/50-sample means, runs scale+lookup on `ch0_avg`, and publishes a live `lcd_measure_t`. `app_lcd_measure()` returns that struct; `board.c` is corrected for the OSC pin mask and idles the 3 confirmed channels HIGH.

**Tech Stack:** C11, STM32 HAL (vendor in-tree), CMake + Ninja (ARM cross), arm-none-eabi-gcc; host `cc` + a tiny Makefile for the pure-function unit tests.

---

## Spec & evidence base

- **Spec (authoritative):** `docs/superpowers/specs/2026-05-31-stage-d-slice1-regulation-core-design.md`
- **Verified facts (ground truth):** `docs/superpowers/analysis/2026-05-31-m16-regulation-core-verified.md` (§3/§4). Fact IDs (ADC-1…9, SCALE-01…07, C1/C2/C-LADDER/C6/C7, cad-C1/C4) are cited per task.
- **Pin map:** `docs/pinmap.md` — ADC §52-57 (PB0/ADC1_IN8=ADC_SENS_OUT, PB1/ADC1_IN9=ADC_OSC), OSC table §117-125.

## Deviations from spec (deliberate, surfaced for user review)

These refine the spec's file list / scope; **none change regulation behavior.** All are reversible pure-adds.

1. **Pure functions split into `app_reg_calc.{c,h}` (HAL-free).** The spec put `reg_scale`/`reg_output_level` inside `app_reg.c`. The repo has **no host test harness** (no `fw/test`, no ctest/gtest). To make spec §12 ("host build of the pure functions") actually runnable, the two pure functions + `lookup_table` live in a HAL-free `app_reg_calc.c` compiled by both the ARM build (globbed) and a host Makefile. `app_reg.c` keeps state/tick/acquisition.
2. **ADC driver is a thin peripheral layer.** Spec's `adc1_sample_step()` mixed peripheral read + normalize + accumulate. Per CLAUDE.md (`driver = peripheral; app = state`), `adc1.c` exposes `adc1_read(adc1_ch_t)` returning raw 12-bit; the `>>2` normalize + accumulation + channel alternation live in `app_reg.c` (domain logic). Net behavior identical.
3. **`reg_output_level()` returns the band index `i` (0..21) only.** The verified lookup *compare* (C2) is fully ported and tested. The band→thermometer-**pattern-byte** map (C-LADDER) is **DEFERRED** to the output slice alongside B-SEAM: in slice 1 it is neither displayed nor driven, so porting it now is dead, reality-untestable code (YAGNI). Slice 1 holds `band` in state for the future output stage to consume. (Spec §5 chose to port it now "to keep the output stage a pure add" — deferring keeps it an equally pure add and avoids untested code. If you prefer the spec's full-map port, say so and Task 1 grows by the C-LADDER ladder.)

## File structure (created / modified)

| File | Responsibility | New? |
|------|----------------|------|
| `fw/include/app_reg_calc.h` | pure transfer-fn prototypes + table extern decl | NEW |
| `fw/src/app_reg_calc.c` | `reg_scale()`, `reg_output_level()`, `lookup_table[]` — HAL-free | NEW |
| `fw/test/test_app_reg_calc.c` | host unit tests (assert harness) | NEW |
| `fw/test/Makefile` | host build/run of the unit tests | NEW |
| `fw/include/adc1.h` | ADC1 driver API (`adc1_init`, `adc1_read`) | NEW |
| `fw/drivers/adc1.c` | ADC1 peripheral init + PB0/PB1 analog + polled read | NEW |
| `fw/include/app_reg.h` | regulation state API: `app_reg_init/tick/measure` | NEW |
| `fw/src/app_reg.c` | state owner + ~2 ms tick + acquisition + scale/lookup wiring + `lcd_measure_t` publisher | NEW |
| `fw/src/periph.c` / `fw/include/periph.h` | add `ADC_HandleTypeDef hadc1;` (+ extern) | MODIFY |
| `fw/src/app_lcd.c` | `app_lcd_measure()` → return `app_reg_measure()` | MODIFY |
| `fw/src/app.c` | `app_loop_iter()` → add `app_reg_tick();` | MODIFY |
| `fw/src/main.c` | add `adc1_init();` after `tim11_init()` | MODIFY |
| `fw/src/board.c` | OSC mask fix + 3-channel idle-HIGH | MODIFY |
| `fw/CMakeLists.txt` | add `stm32f4xx_hal_adc.c` to `HAL_SOURCES` | MODIFY |

> ARM build globs `src/*.c drivers/*.c` (CMakeLists:70) → `app_reg_calc.c`/`app_reg.c`/`adc1.c` are auto-picked. `fw/test/` is **not** globbed (outside src/drivers) → host-only.

## Build / test commands (used throughout)

- **ARM build (0-warning gate):** `cd fw && env -u STM32_TOOLCHAIN cmake --build build`
  - If `build/` is stale after adding a HAL source, reconfigure: `env -u STM32_TOOLCHAIN cmake -B build -G Ninja`
- **Host unit test:** `make -C fw/test test`
- Build gate = **0 warnings** under `-Wall -Wextra -Wundef -Wshadow`.

---

## Task 1: Pure compute module (`app_reg_calc`) — TDD on host

**Files:**
- Create: `fw/include/app_reg_calc.h`
- Create: `fw/src/app_reg_calc.c`
- Test: `fw/test/test_app_reg_calc.c`, `fw/test/Makefile`

- [ ] **Step 1: Write the failing host test**

Create `fw/test/test_app_reg_calc.c`:

```c
/* fw/test/test_app_reg_calc.c — host unit tests for the pure regulation
 * compute layer (reg_scale, reg_output_level). No HAL, no hardware.
 * Verifies the disasm-adjudicated facts SCALE-04/05/06 and C1/C2 / §4.3 table. */
#include <stdio.h>
#include <stdint.h>
#include "app_reg_calc.h"

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

/* SCALE-04 (input ceiling), SCALE-05 (input floor), SCALE-06 (×6).
 * Faithful, do NOT normalize: the function is intentionally discontinuous
 * (999→5994 but 1000→1000) because the clip tests the INPUT and the ×6 path
 * is unbounded. Keep these vectors verbatim. */
static void test_reg_scale(void) {
    CHECK_EQ(reg_scale(0), 0);        /* < 3 → floor */
    CHECK_EQ(reg_scale(2), 0);        /* < 3 → floor */
    CHECK_EQ(reg_scale(3), 18);       /* 3 falls through → ×6 (no off-by-one) */
    CHECK_EQ(reg_scale(4), 24);       /* ×6 */
    CHECK_EQ(reg_scale(999), 5994);   /* ×6, NOT clamped to 1000 */
    CHECK_EQ(reg_scale(1000), 1000);  /* input ≥ 1000 → 1000 */
    CHECK_EQ(reg_scale(1023), 1000);  /* input ≥ 1000 → 1000 */
}

/* C2: first i in 0..20 with lookup_table[i] < scaled (strictly-less);
 * no match → 21. Table §4.3: table[0]=0x03FF=1023 … table[20]=0x000F=15. */
static void test_reg_output_level(void) {
    CHECK_EQ(reg_output_level(0), 21);     /* below all → no match */
    CHECK_EQ(reg_output_level(15), 21);    /* table[20]=15; 15<15 false → no match */
    CHECK_EQ(reg_output_level(16), 20);    /* table[20]=15 < 16 → i=20 */
    CHECK_EQ(reg_output_level(1023), 1);   /* table[0]=1023<1023 false; table[1]=972<1023 → i=1 */
    CHECK_EQ(reg_output_level(1024), 0);   /* table[0]=1023 < 1024 → i=0 */
    CHECK_EQ(reg_output_level(5994), 0);   /* huge → i=0 */
}

/* §4.3: table values byte-match, strictly decreasing for idx 0..20. */
static void test_table_values(void) {
    static const uint16_t expect[21] = {
        0x03FF,0x03CC,0x0399,0x0366,0x0333,0x0300,0x02CD,0x029A,
        0x0267,0x0234,0x0201,0x01CE,0x01B9,0x0168,0x0135,0x0102,
        0x00CF,0x009C,0x0069,0x0036,0x000F
    };
    for (int i = 0; i < 21; i++) {
        CHECK_EQ(reg_lookup_table[i], expect[i]);
        if (i > 0) {
            if (!(reg_lookup_table[i] < reg_lookup_table[i-1])) {
                printf("FAIL table not strictly decreasing at idx %d\n", i);
                failures++;
            }
        }
    }
}

int main(void) {
    test_reg_scale();
    test_reg_output_level();
    test_table_values();
    if (failures) { printf("%d check(s) FAILED\n", failures); return 1; }
    printf("all checks PASSED\n");
    return 0;
}
```

Create `fw/test/Makefile`:

```make
# fw/test/Makefile — host unit tests for the pure regulation compute layer.
# Runs on the host toolchain (no ARM, no HAL). Usage: make -C fw/test test
CC     ?= cc
CFLAGS := -std=c11 -Wall -Wextra -Wundef -Wshadow -O0 -g
INC    := -I../include
SRC    := ../src/app_reg_calc.c
BIN    := /tmp/gds_test_app_reg_calc

test: $(BIN)
	$(BIN)

$(BIN): test_app_reg_calc.c $(SRC) ../include/app_reg_calc.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_app_reg_calc.c $(SRC)

clean:
	rm -f $(BIN)

.PHONY: test clean
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `make -C fw/test test`
Expected: FAIL — compile error `fatal error: 'app_reg_calc.h' file not found` (header/impl not written yet).

- [ ] **Step 3: Write the header**

Create `fw/include/app_reg_calc.h`:

```c
/* fw/include/app_reg_calc.h — pure regulation compute (HAL-free, host-testable).
 * Exact port of the ATmega16 transfer functions; see
 * docs/superpowers/analysis/2026-05-31-m16-regulation-core-verified.md §3b/§3c/§4.3. */
#pragma once
#include <stdint.h>

/* lookup_table[24] (flash byte 0x58). Strictly DECREASING for idx 0..20;
 * only the first 21 entries are used by the selector (C1). idx 21..23 present
 * for fidelity but never compared. */
extern const uint16_t reg_lookup_table[24];

/* Scaling transfer — adc_calc_scaled_output @0x1A5C (SCALE-04/05/06).
 * INPUT-domain clip/floor, then ×6. Intentionally discontinuous:
 *   in >= 1000 → 1000;  in < 3 → 0;  else → in*6 (range [18,5994], unbounded).
 * `in` is ch0_avg already normalized to the 10-bit-equiv count domain. */
uint16_t reg_scale(uint16_t in);

/* Output-level selector — output_level_process @0x138C compare core (C2).
 * Returns the FIRST band index i in 0..20 where reg_lookup_table[i] < scaled
 * (strictly-less). No match → 21 (output off / fall-through @0x15B2). */
uint8_t reg_output_level(uint16_t scaled);
```

- [ ] **Step 4: Run the test — expect a different failure (link/undefined)**

Run: `make -C fw/test test`
Expected: FAIL — linker error `undefined reference to reg_scale` / `reg_output_level` / `reg_lookup_table` (header exists, impl missing).

- [ ] **Step 5: Write the implementation**

Create `fw/src/app_reg_calc.c`:

```c
/* fw/src/app_reg_calc.c — pure regulation compute (HAL-free, host-testable).
 * Faithful port of the ATmega16 transfer functions. Evidence:
 * docs/superpowers/analysis/2026-05-31-m16-regulation-core-verified.md §3b/§3c/§4.3. */
#include "app_reg_calc.h"

/* §4.3 lookup_table[24], flash byte 0x58, byte-for-byte. Strictly decreasing
 * idx 0..20; idx 21..23 (0x0004,0x0004,0x0054) present but never compared. */
const uint16_t reg_lookup_table[24] = {
    0x03FF, 0x03CC, 0x0399, 0x0366, 0x0333, 0x0300, 0x02CD, 0x029A,
    0x0267, 0x0234, 0x0201, 0x01CE, 0x01B9, 0x0168, 0x0135, 0x0102,
    0x00CF, 0x009C, 0x0069, 0x0036, 0x000F, 0x0004, 0x0004, 0x0054
};

uint16_t reg_scale(uint16_t in)
{
    if (in >= 1000u) return 1000u;       /* SCALE-04: input ceiling (cpi 0x03E8) */
    if (in < 3u)     return 0u;           /* SCALE-05: input floor (sbiw 0x03; 3 falls through) */
    return (uint16_t)(in * 6u);           /* SCALE-06: ×6 (not >>6), range [18,5994] */
}

uint8_t reg_output_level(uint16_t scaled)
{
    for (uint8_t i = 0u; i < 21u; i++) {
        if (reg_lookup_table[i] < scaled) {   /* C2: strictly-less, FIRST match */
            return i;
        }
    }
    return 21u;                                /* no match → output off (@0x15B2) */
}
```

- [ ] **Step 6: Run the test — expect PASS**

Run: `make -C fw/test test`
Expected: `all checks PASSED` (exit 0).

- [ ] **Step 7: Commit**

```bash
git add fw/include/app_reg_calc.h fw/src/app_reg_calc.c fw/test/test_app_reg_calc.c fw/test/Makefile
git commit -m "feat(stage-d): pure regulation compute (reg_scale/reg_output_level) + host tests"
```

---

## Task 2: ADC1 driver + HAL handle + CMake source

**Files:**
- Create: `fw/include/adc1.h`
- Create: `fw/drivers/adc1.c`
- Modify: `fw/src/periph.c`, `fw/include/periph.h`
- Modify: `fw/CMakeLists.txt:38-55` (HAL_SOURCES)

- [ ] **Step 1: Add the HAL ADC source to CMake + enable the module + SYSTEM vendor includes**

(a) Edit `fw/CMakeLists.txt` — inside the `set(HAL_SOURCES …)` block, after the I2C line (`stm32f4xx_hal_i2c.c`), add:

```cmake
    # Stage D — regulation ADC:
    ${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_adc.c
```

> If linking later reports an undefined `HAL_ADCEx_*` symbol, also add `stm32f4xx_hal_adc_ex.c`. Basic regular polled conversion needs only `..._hal_adc.c`.

(b) **Enable the ADC HAL module** (spec gap discovered in execution): edit `fw/vendor/Core/Inc/stm32f4xx_hal_conf.h` line 37 — uncomment to `#define HAL_ADC_MODULE_ENABLED`. This is the application-level HAL config in `Core/Inc` (NOT the read-only ST `Drivers/` tree); prior stages enabled `HAL_I2C_MODULE_ENABLED`/`HAL_TIM_MODULE_ENABLED` the same way. Without it the HAL umbrella never includes `stm32f4xx_hal_adc.h` and `HAL_ADC_Start/...` are undeclared.

(c) **Make vendor includes SYSTEM** (0-warning gate fix): change `target_include_directories(stm32_hal PUBLIC` → `target_include_directories(stm32_hal SYSTEM PUBLIC`. Enabling the ADC module pulls `stm32f4xx_ll_adc.h`, whose `LL_ADC_DMA_GetRegAddr` inline trips `-Wunused-parameter` in every app TU including the HAL umbrella. Marking vendor headers SYSTEM suppresses vendor-header warnings while keeping our own code under full `-Wall -Wextra`.

- [ ] **Step 2: Add the ADC handle to periph**

Edit `fw/include/periph.h` — add after the `hi2c1` extern (line 9):

```c
extern ADC_HandleTypeDef hadc1;     /* ADC1 — PB0/PB1 sense (Stage D regulation) */
```

Edit `fw/src/periph.c` — add after the `hi2c1` definition (line 8):

```c
ADC_HandleTypeDef  hadc1;   /* ADC1 — PB0/PB1 sense (Stage D regulation) */
```

- [ ] **Step 3: Write the driver header**

Create `fw/include/adc1.h`:

```c
/* fw/include/adc1.h — ADC1 driver: PB0/ADC1_IN8 (ADC_SENS_OUT) + PB1/ADC1_IN9
 * (ADC_OSC), polled single conversion. Driver owns its analog pins. */
#pragma once
#include <stdint.h>

/* Semantic channels (driver maps to HAL ADC_CHANNEL_* internally so callers
 * stay HAL-free). ch0 = regulation sense feedback; ch1 = oscillator monitor. */
typedef enum {
    ADC1_CH_SENS_OUT = 0,   /* PB0 / ADC1_IN8 — mega16 ch0 (regulation input) */
    ADC1_CH_OSC      = 1,   /* PB1 / ADC1_IN9 — mega16 ch1 (monitor, B4) */
} adc1_ch_t;

/* Init ADC1 + PB0/PB1 analog GPIO. Call once at boot (before board_init). */
void adc1_init(void);

/* Blocking single conversion on the given channel. Returns the raw 12-bit
 * count (0..4095). Caller normalizes to the regulation domain (>>2). */
uint16_t adc1_read(adc1_ch_t ch);
```

- [ ] **Step 4: Write the driver**

Create `fw/drivers/adc1.c`:

```c
/* fw/drivers/adc1.c — ADC1 peripheral init + polled read.
 * Pins: PB0 = ADC1_IN8 (ADC_SENS_OUT), PB1 = ADC1_IN9 (ADC_OSC). pinmap §52-57. */
#include "stm32f4xx_hal.h"
#include "periph.h"
#include "clock.h"   /* Error_Handler */
#include "adc1.h"

void adc1_init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitTypeDef g = {
        .Pin  = GPIO_PIN_0 | GPIO_PIN_1,
        .Mode = GPIO_MODE_ANALOG,
        .Pull = GPIO_NOPULL,
    };
    HAL_GPIO_Init(GPIOB, &g);

    __HAL_RCC_ADC1_CLK_ENABLE();
    hadc1.Instance                   = ADC1;
    hadc1.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode          = DISABLE;
    hadc1.Init.ContinuousConvMode    = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion       = 1;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) Error_Handler();
}

uint16_t adc1_read(adc1_ch_t ch)
{
    ADC_ChannelConfTypeDef s = {
        .Channel      = (ch == ADC1_CH_OSC) ? ADC_CHANNEL_9 : ADC_CHANNEL_8,
        .Rank         = 1,
        .SamplingTime = ADC_SAMPLETIME_84CYCLES,
    };
    if (HAL_ADC_ConfigChannel(&hadc1, &s) != HAL_OK) Error_Handler();

    HAL_ADC_Start(&hadc1);
    (void)HAL_ADC_PollForConversion(&hadc1, 2u);   /* µs-scale conversion; 2 ms guard */
    uint16_t v = (uint16_t)HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
    return v;
}
```

- [ ] **Step 5: Reconfigure + build (0-warning gate)**

Run:
```bash
cd fw && env -u STM32_TOOLCHAIN cmake -B build -G Ninja && env -u STM32_TOOLCHAIN cmake --build build
```
Expected: configure succeeds (new `app_reg_calc.c` + `adc1.c` globbed, `stm32f4xx_hal_adc.c` compiled), build completes with **0 warnings**. `app_reg_calc.c` links cleanly (unused on target so far is fine — it has external linkage). `adc1_init`/`adc1_read` are unused-but-defined (no warning: non-static external functions).

> If configure shows the source list didn't refresh, the `-B build` reconfigure handles it; a stale build without reconfigure would miss the new globs.

- [ ] **Step 6: Commit**

```bash
git add fw/include/adc1.h fw/drivers/adc1.c fw/src/periph.c fw/include/periph.h fw/CMakeLists.txt
git commit -m "feat(stage-d): ADC1 driver (PB0/PB1 polled) + hadc1 handle + HAL adc source"
```

---

## Task 3: Regulation state module (`app_reg`) — tick, acquisition, publish

**Files:**
- Create: `fw/include/app_reg.h`
- Create: `fw/src/app_reg.c`

This module owns the regulation state and the published `lcd_measure_t`. It does NOT drive any GPIO (output stage deferred, spec §9).

- [ ] **Step 1: Write the header**

Create `fw/include/app_reg.h`:

```c
/* fw/include/app_reg.h — Stage D slice 1 regulation core: ~2 ms superloop step
 * that acquires the 2-ch ADC, averages (10/50), scales, and runs the lookup.
 * Owns the live lcd_measure_t the LCD display reads. NO physical output this
 * slice (OSC drive deferred — spec §9, B-SEAM). */
#pragma once
#include <stdint.h>
#include "app_lcd.h"   /* lcd_measure_t */

/* Init regulation state + start ADC1. Call once at boot. */
void app_reg_init(void);

/* ~2 ms regulation step; gate internally on sys_tick_get_ms() delta.
 * One ADC conversion (alternating channel) per fire, accumulate/average,
 * then scale + lookup on the latest ch0_avg, then publish lcd_measure_t. */
void app_reg_tick(void);

/* Live measured values for the LCD display machine (single owner). */
const lcd_measure_t *app_reg_measure(void);
```

- [ ] **Step 2: Write the implementation**

Create `fw/src/app_reg.c`:

```c
/* fw/src/app_reg.c — Stage D slice 1 regulation core (compute only).
 * Pipeline (verified analysis §3): alternate 2-ch polled ADC → normalize 12→10b
 * → ch0 mean-of-10 / ch1 mean-of-50 → reg_scale(ch0_avg) → reg_output_level.
 * Publishes lcd_measure_t; drives NO OSC GPIO (deferred, spec §9 / B-SEAM). */
#include <string.h>
#include "app_reg.h"
#include "app_reg_calc.h"
#include "adc1.h"
#include "sys_tick.h"
#include "mon.h"

/* Cadence: Timer0 0xF0 → 16 ct → ~2.05 ms @8 MHz (cad-C1). Reproduced in ms. */
#define REG_TICK_MS      2u
#define CH0_SAMPLES      10u   /* ADC-2 */
#define CH1_SAMPLES      50u   /* ADC-3 */
#define ADC_NORM_SHIFT   2u    /* 12-bit → 10-bit-equiv (spec §10 DP2 first-cut) */

#ifdef REG_TRACE
#define REG_TRACE_MS     500u  /* slow trace cadence so the mon log stays readable */
#endif

typedef struct {
    uint16_t ch0_avg, ch1_avg;        /* committed means (10-bit-equiv domain) */
    uint16_t adc_scaled_value;        /* reg_scale(ch0_avg) */
    uint8_t  band;                    /* reg_output_level() result 0..21 (held; output deferred) */

    uint32_t ch0_acc; uint16_t ch0_cnt;
    uint32_t ch1_acc; uint16_t ch1_cnt;
    uint8_t  cur_ch;                  /* 0 = SENS_OUT, 1 = OSC (alternation) */

    uint32_t prev_ms;
#ifdef REG_TRACE
    uint32_t trace_ms;
#endif
} reg_state_t;

static reg_state_t  g_reg;
static lcd_measure_t g_measure;

void app_reg_init(void)
{
    memset(&g_reg, 0, sizeof(g_reg));
    memset(&g_measure, 0, sizeof(g_measure));
    adc1_init();
    g_reg.prev_ms = sys_tick_get_ms();
}

const lcd_measure_t *app_reg_measure(void)
{
    return &g_measure;
}

/* One ADC conversion on the current channel, normalize, accumulate, and commit
 * the mean when the per-channel sample count is reached. Advances the channel. */
static void reg_acquire_step(void)
{
    if (g_reg.cur_ch == 0u) {
        uint16_t s = (uint16_t)(adc1_read(ADC1_CH_SENS_OUT) >> ADC_NORM_SHIFT);
        g_reg.ch0_acc += s;
        if (++g_reg.ch0_cnt >= CH0_SAMPLES) {
            g_reg.ch0_avg = (uint16_t)(g_reg.ch0_acc / CH0_SAMPLES);
            g_reg.ch0_acc = 0u;
            g_reg.ch0_cnt = 0u;
        }
        g_reg.cur_ch = 1u;
    } else {
        uint16_t s = (uint16_t)(adc1_read(ADC1_CH_OSC) >> ADC_NORM_SHIFT);
        g_reg.ch1_acc += s;
        if (++g_reg.ch1_cnt >= CH1_SAMPLES) {
            g_reg.ch1_avg = (uint16_t)(g_reg.ch1_acc / CH1_SAMPLES);
            g_reg.ch1_acc = 0u;
            g_reg.ch1_cnt = 0u;
        }
        g_reg.cur_ch = 0u;
    }
}

static void reg_publish_measure(void)
{
    /* spec §7: amp + scaled level live; cycle/freq/energy/status stay 0. */
    g_measure.curr_amp   = g_reg.ch0_avg;
    g_measure.curr_power = g_reg.adc_scaled_value;
    g_measure.max_power  = g_reg.adc_scaled_value;
    g_measure.last_power = g_reg.adc_scaled_value;
    /* curr_freq/last_freq, curr_energy/last_energy, us_on_time_200m,
     * us_run_status, sig_*_status remain 0 (slice 2 / output stage). */
}

void app_reg_tick(void)
{
    uint32_t now = sys_tick_get_ms();
    if ((uint32_t)(now - g_reg.prev_ms) < REG_TICK_MS) {
        return;
    }
    g_reg.prev_ms = now;

    reg_acquire_step();

    /* Scale + lookup run unconditionally every tick (cad-C4), on the latest
     * committed ch0_avg. */
    g_reg.adc_scaled_value = reg_scale(g_reg.ch0_avg);
    g_reg.band             = reg_output_level(g_reg.adc_scaled_value);

    reg_publish_measure();

#ifdef REG_TRACE
    if ((uint32_t)(now - g_reg.trace_ms) >= REG_TRACE_MS) {
        g_reg.trace_ms = now;
        mon_printf("[reg] ch0=%u ch1=%u scaled=%u band=%u\r\n",
                   (unsigned)g_reg.ch0_avg, (unsigned)g_reg.ch1_avg,
                   (unsigned)g_reg.adc_scaled_value, (unsigned)g_reg.band);
    }
#else
    (void)mon_printf;  /* mon.h pulled only for the trace build */
#endif
}
```

> Note on the `(void)mon_printf;` line: `mon.h` is included for the trace build; in the non-trace build that include is otherwise unused. If the compiler warns about the unused include rather than the symbol, drop the `#include "mon.h"` and the `(void)` line under `#ifndef REG_TRACE`. Verify at Step 3 and adjust to keep 0-warning. (Simplest robust form: guard the whole `#include "mon.h"` with `#ifdef REG_TRACE` and delete the `(void)mon_printf;` else-branch.)

- [ ] **Step 3: Build (0-warning gate)**

Run: `cd fw && env -u STM32_TOOLCHAIN cmake --build build`
Expected: 0 warnings. `app_reg.c` compiles; `app_reg_init/tick/measure` defined but not yet called (no warning — external linkage). If any unused-symbol warning appears from the `mon.h` handling, apply the Step-2 note (guard the include) and rebuild to 0-warning.

- [ ] **Step 4: Commit**

```bash
git add fw/include/app_reg.h fw/src/app_reg.c
git commit -m "feat(stage-d): regulation state module — 2ms tick, 10/50 ADC averaging, scale+lookup"
```

---

## Task 4: Wire into the superloop + measure provider + boot init

**Files:**
- Modify: `fw/src/app.c` (`app_loop_iter`, line 62-75; `app_init` not changed except optional)
- Modify: `fw/src/main.c:20-21`
- Modify: `fw/src/app_lcd.c:16-22` (`app_lcd_measure`)

- [ ] **Step 1: Add the boot init**

Edit `fw/src/main.c` — add the extern prototype near the others (after line 11):

```c
extern void adc1_init(void);     /* drivers/adc1.c (called via app_reg_init) */
```

Actually `adc1_init()` is called by `app_reg_init()`, not directly from main. Instead, init regulation at boot. Edit `fw/src/main.c` init sequence — add `app_reg_init();` after `app_init();` is the natural spot, but `app_reg_init()` needs `sys_tick` running (started in `app_init`). So place it **after** `app_init();` (line 23):

```c
    app_init();        /* sys_tick start, mon banner */
    app_reg_init();    /* Stage D: ADC1 + regulation state (needs sys_tick up) */
```

Add the prototype for `app_reg_init` — include the header at the top of `main.c` (after line 7):

```c
#include "app_reg.h"
```

(Remove the unnecessary `extern void adc1_init(void);` idea above — `app_reg_init()` owns the ADC bring-up; do not call `adc1_init()` from main.)

- [ ] **Step 2: Wire the regulation tick into the superloop**

Edit `fw/src/app.c` — add the include (after line 9, `#include "app_lcd.h"`):

```c
#include "app_reg.h"
```

In `app_loop_iter()` (after the display tick, line 74 `app_lcd_tick();`), add:

```c
    /* 3. Regulation core — ~2 ms cadence (spec §6), compute-only this slice. */
    app_reg_tick();
```

- [ ] **Step 3: Point the measure provider at live values**

Edit `fw/src/app_lcd.c` — add the include (after line 5, `#include "mon.h"`):

```c
#include "app_reg.h"
```

Replace the stub body (`fw/src/app_lcd.c:16-22`):

```c
const lcd_measure_t *app_lcd_measure(void)
{
    /* Stage D slice 1: live regulation values (amp + scaled level). Cycle/freq/
     * energy/status stay 0 until slice 2 (honest "regulating, no cycle yet"). */
    return app_reg_measure();
}
```

- [ ] **Step 4: Build (0-warning gate)**

Run: `cd fw && env -u STM32_TOOLCHAIN cmake --build build`
Expected: 0 warnings; FLASH/RAM usage printed. The regulation pipeline is now live in the superloop and feeds the LCD.

- [ ] **Step 5: Commit**

```bash
git add fw/src/main.c fw/src/app.c fw/src/app_lcd.c
git commit -m "feat(stage-d): wire regulation tick into superloop + live LCD measure provider"
```

---

## Task 5: `board.c` safe fixes (OSC mask + 3-channel idle-HIGH)

**Files:**
- Modify: `fw/src/board.c:7,21-23`

Verified facts: §6 board.c bugs, C6 (active-LOW/idle-HIGH), C7 (PB12/PB13 are inputs in the image — do NOT init as outputs).

- [ ] **Step 1: Fix the mask and split the OSC init**

Edit `fw/src/board.c`. Replace the `CTRL_OSC_PINS` define (line 7):

```c
/* OSC channels per pinmap.md §117-125: PB2|PB10|PB12|PB13|PB14 (was wrong:
 * PB2 missing, PB15 spurious). Only the 3 image-confirmed OUTPUT channels are
 * driven here (C6/C7): PB2/OSC0←mega16 PB1, PB10/OSC1←PB0, PB14/OSC4←PC7,
 * all active-LOW so idle = HIGH. PB12/PB13 (OSC2/OSC3 ← PC4/PA7) are DDR-inputs
 * in the image (C7) — direction unconfirmed (B-OSC-MAP), left unconfigured. */
#define CTRL_OSC_OUT_PINS (GPIO_PIN_2 | GPIO_PIN_10 | GPIO_PIN_14)
```

Replace the OSC init block (lines 21-23, the `HAL_GPIO_WritePin(GPIOB, CTRL_OSC_PINS, GPIO_PIN_RESET)` + `out.Pin = CTRL_OSC_PINS` + `HAL_GPIO_Init`):

```c
    /* Idle the 3 confirmed OSC outputs HIGH (= OSC off; active-LOW per C6).
     * Set the level BEFORE switching to output to avoid a boot LOW glitch. */
    HAL_GPIO_WritePin(GPIOB, CTRL_OSC_OUT_PINS, GPIO_PIN_SET);
    out.Pin = CTRL_OSC_OUT_PINS;
    HAL_GPIO_Init(GPIOB, &out);
```

> The heartbeat-era `CTRL_OSC0..4 LOW` write is removed; PB12/PB13 are no longer configured as outputs. The `HB_PIN`/heartbeat block above stays unchanged.

- [ ] **Step 2: Build (0-warning gate)**

Run: `cd fw && env -u STM32_TOOLCHAIN cmake --build build`
Expected: 0 warnings.

- [ ] **Step 3: Commit**

```bash
git add fw/src/board.c
git commit -m "fix(stage-d): board.c OSC mask (PB2 add/PB15 drop) + idle-HIGH 3 confirmed channels"
```

---

## Task 6: On-target HW verify (REG_TRACE) — interactive, with board

This task is **manual / interactive** (needs the physical board + USART6 monitor). It is the embedded equivalent of an integration test (the project's established Stage B / LCD verification pattern).

- [ ] **Step 1: Build a trace image**

Add the trace define for a one-off build (do NOT commit this edit — mirror the `LCD_TRACE_RX` pattern):
```bash
cd fw
env -u STM32_TOOLCHAIN cmake -B build-trace -G Ninja -DCMAKE_C_FLAGS="-DREG_TRACE"
env -u STM32_TOOLCHAIN cmake --build build-trace
```
Expected: 0-warning build; `build-trace/gds_us_ctrl.elf`.

> `build-trace/` is already git-ignored. Reuse the existing pattern from RESUME (`-DLCD_TRACE_RX`).

- [ ] **Step 2: Flash and open the monitor**

```bash
openocd -f fw/openocd/stm32f410.cfg -c "program fw/build-trace/gds_us_ctrl.elf verify reset exit"
# monitor (single fd):
{ stty -f /dev/cu.usbserial-BG02DMWU 115200 cs8 -parenb -cstopb raw -echo; cat; } < /dev/cu.usbserial-BG02DMWU > /tmp/reg-mon.log &
```

- [ ] **Step 3: Observe — acceptance checks**

Read `/tmp/reg-mon.log` (clean reset glitches: `tr -d '\000' < /tmp/reg-mon.log | tr -s ' ' | grep '\['`):
- [ ] `[boot]` + `[cfg]` banners present (no regression).
- [ ] `[reg] ch0=… ch1=… scaled=… band=…` lines appear on the ~500 ms cadence.
- [ ] Inject / vary a known analog level on **PB0**: `ch0` tracks it; `scaled` follows the ×6/clip (e.g. ch0≈100 → scaled≈600; ch0≥1000 → scaled=1000); `band` steps through the table as `scaled` crosses thresholds.
- [ ] LCD output bar + VAR_POWER/VAR_AMP move with the injected PB0 signal (provider live).
- [ ] **No OSC GPIO activity** and PB2/PB10/PB14 idle **HIGH** (scope/meter); PB12/PB13 left as reset inputs (output deferred).

- [ ] **Step 4: Clean up the trace build**

```bash
rm -rf fw/build-trace   # (or leave; it is git-ignored)
```

No commit (trace build is uncommitted by design).

---

## Self-review (run after drafting; checklist)

1. **Spec coverage:**
   - §0 IN.1 ADC acquisition → Task 2 (driver) + Task 3 (10/50 averaging). ✓
   - §0 IN.2 scaling → Task 1 `reg_scale`. ✓
   - §0 IN.3 lookup → Task 1 `reg_output_level` (band index; pattern-byte map deferred per Deviation 3). ✓
   - §0 IN.4 ~2 ms cadence → Task 3 `app_reg_tick` + Task 4 wiring. ✓
   - §0 IN.5 measure provider → Task 3 publish + Task 4 `app_lcd_measure`. ✓
   - §0 IN.6 board.c fixes → Task 5. ✓
   - §10 domain `>>2` normalize → Task 3 `ADC_NORM_SHIFT`. ✓
   - §11 file/build changes → all covered; `stm32f4xx_hal_adc.c` Task 2. ✓
   - §12 unit tests → Task 1 (host); HW verify → Task 6. ✓
   - DEFERRED §9 output stage → explicitly NOT implemented (board.c leaves PB12/PB13 unconfigured; no `reg_drive_osc`). ✓
2. **Placeholders:** none — every code step shows complete code; commands have expected output.
3. **Type consistency:** `reg_scale(uint16_t)→uint16_t`, `reg_output_level(uint16_t)→uint8_t`, `reg_lookup_table[24]`, `adc1_read(adc1_ch_t)→uint16_t`, `app_reg_measure()→const lcd_measure_t*`, `app_reg_tick/init(void)` — used consistently across Tasks 1-4. `app_lcd_measure()` signature unchanged (matches `app_lcd.h:126`). ✓

## Execution notes

- Tasks 1→5 are build-gated and self-checking; Task 6 needs the board + user.
- Each task commits independently (frequent commits).
- After Task 5: full build 0-warning is the integration gate; consider a `cpp-reviewer` pass before merge.
- Output stage (B-SEAM) remains DEFERRED — a follow-up slice after bench measurement (spec §9).
