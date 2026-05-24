# Stage B — LCD Application Data Setup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** On boot, load persisted configuration from the on-board FM24C16B FRAM and drive the DGUS panel through a real `LOGO → RUN` transition with model string, icons, and numeric fields pre-filled (port of samd20 `init_lcd_mode`).

**Architecture:** Layered mirror of Stage A (`usart1 → dgus_lcd → app`): `i2c1` (HAL peripheral) → `fram` (FM24C16B device driver) → `app_config` (init-flag-aware load + factory defaults) → `app_lcd` (`init_lcd_mode` port). Stage A's DGUS 9 wire functions are reused unchanged.

**Tech Stack:** C11, STM32F4 HAL, arm-none-eabi-gcc, CMake+Ninja. FRAM over I2C1 (PB6/PB7 AF4, 400 kHz, slave 0x50, 1-byte addressing).

**Spec:** `docs/superpowers/specs/2026-05-25-stage-b-lcd-app-design.md`.

---

## Verification model (read first — embedded, no host test framework)

This target has **no on-target unit-test harness**, and the hw revA board is **not available this session**
(user decision). Stage A's established gate is reused as the per-task RED→GREEN equivalent:

- **GREEN gate (per task):** `env -u STM32_TOOLCHAIN cmake -B build -G Ninja && env -u STM32_TOOLCHAIN cmake --build build`
  → **build succeeds with 0 warnings** (`-Wall -Wextra -Wundef -Wshadow` are on). clangd LSP noise is ignored.
  CMake globs `src/*.c drivers/*.c`, so new files require the `-B build` **re-configure** shown above.
- **Logic correctness:** spec-compliance review per task (the subagent-driven-development reviewer), plus the
  static traces noted in each task. Byte order, VP addresses/payloads, and the init-flag branch are review targets.
- **Deferred to a follow-up HW task** (board on bench): I2C ACK from 0x50, write-then-read FRAM round-trip,
  visual `LOGO→RUN_HAND`, USART6 `[cfg]` log. NOT in this plan.

There are no `pytest`/`gtest` steps because none exist in this repo; inventing them would be a fake gate.

**Commit policy:** attribution disabled (user `~/.claude/settings.json`) — no Co-Authored-By trailer.

---

## File Structure

| File | Action | Responsibility |
|---|---|---|
| `fw/include/dgus_lcd.h` | modify | add Stage B VP address macros |
| `fw/include/sys_tick.h`, `fw/src/sys_tick.c` | modify | add `sys_tick_delay_ms()` busy-wait |
| `fw/include/periph.h`, `fw/src/periph.c` | modify | declare/define `I2C_HandleTypeDef hi2c1` |
| `fw/include/i2c1.h`, `fw/drivers/i2c1.c` | create | I2C1 init + blocking mem read/write wrappers |
| `fw/include/fram.h`, `fw/drivers/fram.c` | create | FM24C16B address map + read/write byte/u16/u32 |
| `fw/include/app_config.h`, `fw/src/app_config.c` | create | `app_config_t` + `app_config_load` + `app_config_factory_write` |
| `fw/include/app_lcd.h`, `fw/src/app_lcd.c` | create | `app_lcd_send_model_str` + `app_lcd_init_mode` |
| `fw/src/main.c` | modify | call `i2c1_init()` |
| `fw/src/app.c` | modify | boot sequence (logo → delay → load → init_mode) |
| `fw/CMakeLists.txt` | modify | add HAL `stm32f4xx_hal_i2c.c` to `HAL_SOURCES` |
| `fw/vendor/Core/Inc/stm32f4xx_hal_conf.h` | modify | enable `HAL_I2C_MODULE_ENABLED` (line 57) |

Each `*.c` in `src/`/`drivers/` is auto-globbed; only the HAL I2C source needs a manual CMake entry.

> **Note on the HAL conf file:** `stm32f4xx_hal_conf.h` is the **project-owned HAL configuration**
> (the file that already enables UART/TIM/GPIO/RCC for Phase 2/Stage A), not read-only ST library code.
> Enabling the I2C module there follows the exact precedent that enabled UART/TIM. Without it,
> `I2C_HandleTypeDef` and `HAL_I2C_*` do not exist and the build fails. It lives under `vendor/` only by
> this project's directory layout.

---

## Task 1: Stage B VP address macros

**Files:**
- Modify: `fw/include/dgus_lcd.h` (Application VPs block, after `LV_OUTPUT` near line 58)

- [ ] **Step 1: Add the VP macros** (values verbatim from `ref/samd20/dgus_lcd.h`)

Insert after the existing `#define LV_OUTPUT        0x1170` line:

```c
/* Stage B application VPs (samd20 dgus_lcd.h verbatim) */
#define LV_ENERGY_EDIT   0x1202
#define LV_DM_DELAY      0x1203
#define LV_DM_WELD       0x1204
#define LV_DM_HOLD       0x1205
#define LV_TM_WELD       0x1206
#define LV_TM_HOLD       0x1207
#define DISP_HORNDOWN    0x1209
#define DISP_ENERGY_EN   0x120a
#define DISP_MULTI_EN    0x120b
#define ICON_RESET       0x1150
#define ICON_SEEK        0x1151
#define ICON_RUN         0x1152
```

- [ ] **Step 2: Verify build is green**

Run: `cd fw && env -u STM32_TOOLCHAIN cmake -B build -G Ninja && env -u STM32_TOOLCHAIN cmake --build build`
Expected: build succeeds, 0 warnings (macros unused so far — no warning for unused macros).

- [ ] **Step 3: Commit**

```bash
git add fw/include/dgus_lcd.h
git commit -m "feat(lcd): add Stage B application VP address macros"
```

---

## Task 2: `sys_tick_delay_ms()` busy-wait helper

**Files:**
- Modify: `fw/include/sys_tick.h`, `fw/src/sys_tick.c`

- [ ] **Step 1: Declare the helper**

In `fw/include/sys_tick.h`, add after `uint32_t sys_tick_get_ms(void);`:

```c
void sys_tick_delay_ms(uint32_t ms);
```

- [ ] **Step 2: Implement the helper**

In `fw/src/sys_tick.c`, add after `sys_tick_get_ms()`:

```c
void sys_tick_delay_ms(uint32_t ms) {
    uint32_t t0 = sys_tick_get_ms();
    while ((uint32_t)(sys_tick_get_ms() - t0) < ms) {
        /* busy-wait on TIM11 1 ms tick; NOT HAL_Delay (HAL SysTick is not used here) */
    }
}
```

- [ ] **Step 3: Verify build is green**

Run: `cd fw && env -u STM32_TOOLCHAIN cmake -B build -G Ninja && env -u STM32_TOOLCHAIN cmake --build build`
Expected: build succeeds, 0 warnings.

Static trace: `sys_tick_get_ms()` returns `s_ms` incremented by the TIM11 IRQ (`sys_tick_handle_irq`),
so the loop advances only after `sys_tick_init()` (called first in `app_init`). Wrap-safe via unsigned subtraction.

- [ ] **Step 4: Commit**

```bash
git add fw/include/sys_tick.h fw/src/sys_tick.c
git commit -m "feat(sys_tick): add sys_tick_delay_ms busy-wait helper"
```

---

## Task 3: I2C1 peripheral driver

**Files:**
- Modify: `fw/include/periph.h`, `fw/src/periph.c`, `fw/CMakeLists.txt`, `fw/vendor/Core/Inc/stm32f4xx_hal_conf.h`
- Create: `fw/include/i2c1.h`, `fw/drivers/i2c1.c`

> **Steps 1–4 are one logical change — do NOT build between them.** `periph.h` (Step 1) references
> `I2C_HandleTypeDef`, which only exists once the I2C module is enabled in Step 4. The first valid build
> is Step 5. A single commit (Step 6) covers all five sub-changes.

- [ ] **Step 1: Declare/define the HAL handle**

In `fw/include/periph.h`, add after the `htim11` extern:

```c
extern I2C_HandleTypeDef hi2c1;   /* I2C1 — FM24C16B FRAM (Stage B) */
```

In `fw/src/periph.c`, add after the `htim11` definition:

```c
I2C_HandleTypeDef  hi2c1;   /* I2C1 — FM24C16B FRAM (Stage B) */
```

- [ ] **Step 2: Create the header `fw/include/i2c1.h`**

```c
/* fw/include/i2c1.h — I2C1 raw layer (FM24C16B FRAM, PB6/PB7 AF4, 400 kHz). */
#pragma once
#include "stm32f4xx_hal.h"
#include <stdint.h>

#define I2C1_TIMEOUT_MS  50u

void i2c1_init(void);

/* Blocking FRAM-style access: [S][dev_w][mem_addr][...]. 1-byte memory address. */
HAL_StatusTypeDef i2c1_mem_read (uint8_t dev7, uint8_t mem_addr, uint8_t *buf, uint16_t n);
HAL_StatusTypeDef i2c1_mem_write(uint8_t dev7, uint8_t mem_addr, const uint8_t *buf, uint16_t n);

uint16_t i2c1_err_count(void);   /* non-OK transfer count (observability) */
```

- [ ] **Step 3: Create the driver `fw/drivers/i2c1.c`**

```c
/* fw/drivers/i2c1.c
 *
 * I2C1 raw I/O — FM24C16B FRAM (PB6=SCL / PB7=SDA, AF4, 400 kHz Fast mode).
 * External 10 kΩ pull-ups to VCC_5 → GPIO_NOPULL. PCLK1 = 48 MHz (APB1 = HCLK/2).
 * Caller: fw/drivers/fram.c only.
 */
#include "stm32f4xx_hal.h"
#include "periph.h"
#include "clock.h"   /* Error_Handler */
#include "i2c1.h"

static volatile uint16_t s_err_count;

void i2c1_init(void)
{
    /* 1. clocks */
    __HAL_RCC_I2C1_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* 2. GPIO PB6/PB7 AF4, open-drain, no internal pull (external 10k to 5V) */
    GPIO_InitTypeDef g = {0};
    g.Pin       = GPIO_PIN_6 | GPIO_PIN_7;
    g.Mode      = GPIO_MODE_AF_OD;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &g);

    /* 3. I2C config — 400 kHz Fast mode */
    hi2c1.Instance             = I2C1;
    hi2c1.Init.ClockSpeed      = 400000;
    hi2c1.Init.DutyCycle       = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1     = 0;
    hi2c1.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2     = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
        Error_Handler();
    }

    s_err_count = 0;
}

HAL_StatusTypeDef i2c1_mem_read(uint8_t dev7, uint8_t mem_addr, uint8_t *buf, uint16_t n)
{
    HAL_StatusTypeDef st = HAL_I2C_Mem_Read(&hi2c1, (uint16_t)(dev7 << 1), mem_addr,
                                            I2C_MEMADD_SIZE_8BIT, buf, n, I2C1_TIMEOUT_MS);
    if (st != HAL_OK) {
        s_err_count++;
    }
    return st;
}

HAL_StatusTypeDef i2c1_mem_write(uint8_t dev7, uint8_t mem_addr, const uint8_t *buf, uint16_t n)
{
    HAL_StatusTypeDef st = HAL_I2C_Mem_Write(&hi2c1, (uint16_t)(dev7 << 1), mem_addr,
                                             I2C_MEMADD_SIZE_8BIT, (uint8_t *)buf, n, I2C1_TIMEOUT_MS);
    if (st != HAL_OK) {
        s_err_count++;
    }
    return st;
}

uint16_t i2c1_err_count(void) { return s_err_count; }
```

- [ ] **Step 4: Enable the HAL I2C module + add the HAL source to CMake**

In `fw/vendor/Core/Inc/stm32f4xx_hal_conf.h` (line 57), uncomment the I2C module:

```c
/* before */   /* #define HAL_I2C_MODULE_ENABLED */
/* after  */   #define HAL_I2C_MODULE_ENABLED
```

In `fw/CMakeLists.txt`, inside the `set(HAL_SOURCES …)` list, add after the `…_hal_tim_ex.c` line:

```cmake
    ${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_i2c.c
```

(Both are required: the conf define makes `I2C_HandleTypeDef`/`HAL_I2C_*` visible; the CMake entry links
the implementation. This is the same mechanism that enabled UART/TIM — see the File Structure note.)

- [ ] **Step 5: Verify build is green** (re-configure picks up new files + CMake change)

Run: `cd fw && env -u STM32_TOOLCHAIN cmake -B build -G Ninja && env -u STM32_TOOLCHAIN cmake --build build`
Expected: build succeeds, 0 warnings. `i2c1_*` are extern (header-declared) so no unused-function warning before they have callers.

- [ ] **Step 6: Commit**

```bash
git add fw/include/periph.h fw/src/periph.c fw/include/i2c1.h fw/drivers/i2c1.c fw/CMakeLists.txt
git commit -m "feat(i2c1): bring up I2C1 @400kHz (PB6/PB7 AF4) for FRAM"
```

---

## Task 4: FM24C16B FRAM driver

**Files:**
- Create: `fw/include/fram.h`, `fw/drivers/fram.c`

- [ ] **Step 1: Create the header `fw/include/fram.h`**

```c
/* fw/include/fram.h
 *
 * FM24C16B FRAM (I2C1 slave 0x50, 1-byte word address, page 0).
 * Byte map verbatim from samd20 ref/samd20/define.h:44-73.
 *
 * Hard invariants (FM24C16 page-0 model):
 *   - FRAM_I2C_ADDR_7B is constant 0x50, never OR'd with page bits.
 *   - every access satisfies (mem_addr + n) <= 256 (a crossing wraps to offset 0 of the page).
 *   u16/u32 are big-endian (MSB first) to match samd20 on-wire layout.
 */
#pragma once
#include <stdint.h>

#define FRAM_I2C_ADDR_7B       0x50u
#define FRAM_INIT_FLAG_MAGIC   0xAAu

#define FRAM_ADDR_INIT_FLAG    0    /* 1B */
#define FRAM_ADDR_WORK_CNT     1    /* 4B */
#define FRAM_ADDR_DELAY1       5    /* 2B */
#define FRAM_ADDR_DELAY2       7    /* 2B */
#define FRAM_ADDR_DELAY3       9    /* 2B */
#define FRAM_ADDR_TRIGGER2     11   /* 2B */
#define FRAM_ADDR_TRIGGER3     13   /* 2B */
#define FRAM_ADDR_SAFTY        15   /* 1B */
#define FRAM_ADDR_RUN_MODE     16   /* 1B */
#define FRAM_ADDR_MODEL_FREQ   17   /* 1B */
#define FRAM_ADDR_MODEL_TYPE   18   /* 1B */
#define FRAM_ADDR_OUT_POWER    19   /* 1B */
#define FRAM_ADDR_ON_TIME      20   /* 2B */
#define FRAM_ADDR_ENERGY       22   /* 4B */
#define FRAM_ADDR_MULTI_T1     26   /* 2B */
#define FRAM_ADDR_MULTI_T2     28   /* 2B */
#define FRAM_ADDR_MULTI_O1     30   /* 2B */
#define FRAM_ADDR_MULTI_O2     32   /* 2B */
#define FRAM_ADDR_EN_ENERGY    34   /* 1B */
#define FRAM_ADDR_EN_MULTI     35   /* 1B */
#define FRAM_ADDR_TIMEOVER     36   /* 1B */
#define FRAM_ADDR_COMM_ADDR    37   /* 1B */
#define FRAM_ADDR_COMM_SPEED   38   /* 1B */
#define FRAM_ADDR_COMM_PARITY  39   /* 1B */
#define FRAM_ADDR_CAL_VAL      40   /* 2B */
#define FRAM_ADDR_FREQ_CAL_VAL 42   /* 2B */
#define FRAM_ADDR_COMM_MODE    44   /* 1B */
#define FRAM_ADDR_ETHER_IP     45   /* 4B */
#define FRAM_ADDR_ETHER_NM     49   /* 4B */
#define FRAM_ADDR_ETHER_GW     53   /* 4B */

uint8_t  fram_read_byte(uint8_t addr);
uint16_t fram_read_u16 (uint8_t addr);   /* big-endian */
uint32_t fram_read_u32 (uint8_t addr);   /* big-endian */
void     fram_write_byte(uint8_t addr, uint8_t  v);
void     fram_write_u16 (uint8_t addr, uint16_t v);   /* big-endian */
void     fram_write_u32 (uint8_t addr, uint32_t v);   /* big-endian */
```

- [ ] **Step 2: Create the driver `fw/drivers/fram.c`**

```c
/* fw/drivers/fram.c — FM24C16B FRAM driver over i2c1 (samd20 *_fram byte order verbatim). */
#include "fram.h"
#include "i2c1.h"

uint8_t fram_read_byte(uint8_t addr)
{
    uint8_t b = 0;
    i2c1_mem_read(FRAM_I2C_ADDR_7B, addr, &b, 1);
    return b;
}

uint16_t fram_read_u16(uint8_t addr)
{
    uint8_t b[2] = {0};
    i2c1_mem_read(FRAM_I2C_ADDR_7B, addr, b, 2);
    return (uint16_t)(((uint16_t)b[0] << 8) | (uint16_t)b[1]);
}

uint32_t fram_read_u32(uint8_t addr)
{
    uint8_t b[4] = {0};
    i2c1_mem_read(FRAM_I2C_ADDR_7B, addr, b, 4);
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16)
         | ((uint32_t)b[2] <<  8) |  (uint32_t)b[3];
}

void fram_write_byte(uint8_t addr, uint8_t v)
{
    i2c1_mem_write(FRAM_I2C_ADDR_7B, addr, &v, 1);
}

void fram_write_u16(uint8_t addr, uint16_t v)
{
    uint8_t b[2];
    b[0] = (uint8_t)(v >> 8);
    b[1] = (uint8_t)v;
    i2c1_mem_write(FRAM_I2C_ADDR_7B, addr, b, 2);
}

void fram_write_u32(uint8_t addr, uint32_t v)
{
    uint8_t b[4];
    b[0] = (uint8_t)(v >> 24);
    b[1] = (uint8_t)(v >> 16);
    b[2] = (uint8_t)(v >> 8);
    b[3] = (uint8_t)v;
    i2c1_mem_write(FRAM_I2C_ADDR_7B, addr, b, 4);
}
```

- [ ] **Step 3: Verify build is green**

Run: `cd fw && env -u STM32_TOOLCHAIN cmake -B build -G Ninja && env -u STM32_TOOLCHAIN cmake --build build`
Expected: build succeeds, 0 warnings.

Static trace: byte assembly uses explicit shifts (not `memcpy`), so the little-endian M4 still emits MSB-first
on the wire, matching samd20 `read_param_fram`/`read_word_fram` (main.c:594, 677). Largest access = u32 at
`FRAM_ADDR_ENERGY=22` → bytes 22–25 ≤ 256 (invariant holds).

- [ ] **Step 4: Commit**

```bash
git add fw/include/fram.h fw/drivers/fram.c
git commit -m "feat(fram): add FM24C16B read/write (byte/u16/u32, big-endian)"
```

---

## Task 5: `app_config` — load + factory defaults

**Files:**
- Create: `fw/include/app_config.h`, `fw/src/app_config.c`

- [ ] **Step 1: Create the header `fw/include/app_config.h`**

```c
/* fw/include/app_config.h — controller configuration loaded from FRAM (samd20 var_init port). */
#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t  model_freq;        /* 0..5  (15/20/30/35/40/50 K) */
    uint8_t  model_type;        /* 0=hand 1=multi 2=std */
    uint8_t  f_safty;
    uint8_t  run_mode;          /* 0=delay 1=trigger */
    uint16_t limit_delay_time1, limit_delay_time2, limit_delay_time3;
    uint16_t limit_trigger_time2, limit_trigger_time3;
    uint16_t limit_on_time;
    uint16_t limit_out_time;    /* stored 1 byte (ADDR_TIMEOVER) */
    uint16_t limit_mo_out1, limit_mo_out2, limit_mo_time1, limit_mo_time2;
    uint8_t  output_power;
    uint32_t work_cnt;
    uint32_t limit_energy;
    bool     energy_ctrl;
    bool     multi_ctrl;
    int16_t  cal_val, freq_cal_val;
    uint8_t  comm_address, comm_speed_idx, comm_parity_idx, comm_mode;
    uint8_t  ether_ip[4], ether_nm[4], ether_gw[4];
} app_config_t;

/* Read config from FRAM; if INIT_FLAG != 0xAA, write full factory defaults first. */
void app_config_load(app_config_t *cfg);

/* Write the full samd20 default map to FRAM and fill cfg with the same values. */
void app_config_factory_write(app_config_t *cfg);
```

- [ ] **Step 2: Create `fw/src/app_config.c`** (port of samd20 `factory_init` 753-828 + `var_init` 887-943)

```c
/* fw/src/app_config.c — FRAM-backed config: factory defaults (full samd20 map) + load. */
#include "app_config.h"
#include "fram.h"

void app_config_factory_write(app_config_t *cfg)
{
    /* defaults — samd20 factory_init (ref/samd20/main.c:753-828) */
    cfg->model_freq          = 0;
    cfg->model_type          = 0;     /* hand */
    cfg->f_safty             = 0;
    cfg->run_mode            = 0;     /* MODE_DELAY */
    cfg->limit_delay_time1   = 50;
    cfg->limit_delay_time2   = 50;
    cfg->limit_delay_time3   = 50;
    cfg->limit_trigger_time2 = 50;
    cfg->limit_trigger_time3 = 50;
    cfg->limit_on_time       = 50;
    cfg->limit_out_time      = 10;
    cfg->limit_mo_out1       = 25;
    cfg->limit_mo_out2       = 50;
    cfg->limit_mo_time1      = 25;
    cfg->limit_mo_time2      = 50;
    cfg->output_power        = 50;
    cfg->work_cnt            = 0;
    cfg->limit_energy        = 100000;
    cfg->energy_ctrl         = false;  /* port fix: samd20 leaves implicit (bss-zero) */
    cfg->multi_ctrl          = false;  /* port fix: samd20 leaves implicit (bss-zero) */
    cfg->cal_val             = 0;
    cfg->freq_cal_val        = 0;
    cfg->comm_address        = 0;
    cfg->comm_speed_idx      = 0;
    cfg->comm_parity_idx     = 0;
    cfg->comm_mode           = 0;      /* COMM_SERIAL */
    for (int i = 0; i < 4; i++) {
        cfg->ether_ip[i] = 0; cfg->ether_nm[i] = 0; cfg->ether_gw[i] = 0;
    }

    /* persist — full samd20 map (decision beta) */
    fram_write_u32 (FRAM_ADDR_WORK_CNT,     cfg->work_cnt);
    fram_write_u16 (FRAM_ADDR_DELAY1,       cfg->limit_delay_time1);
    fram_write_u16 (FRAM_ADDR_DELAY2,       cfg->limit_delay_time2);
    fram_write_u16 (FRAM_ADDR_DELAY3,       cfg->limit_delay_time3);
    fram_write_u16 (FRAM_ADDR_TRIGGER2,     cfg->limit_trigger_time2);
    fram_write_u16 (FRAM_ADDR_TRIGGER3,     cfg->limit_trigger_time3);
    fram_write_byte(FRAM_ADDR_SAFTY,        cfg->f_safty);
    fram_write_byte(FRAM_ADDR_RUN_MODE,     cfg->run_mode);
    fram_write_byte(FRAM_ADDR_MODEL_FREQ,   cfg->model_freq);
    fram_write_byte(FRAM_ADDR_MODEL_TYPE,   cfg->model_type);
    fram_write_byte(FRAM_ADDR_OUT_POWER,    cfg->output_power);
    fram_write_u16 (FRAM_ADDR_ON_TIME,      cfg->limit_on_time);
    fram_write_byte(FRAM_ADDR_EN_ENERGY,    0);
    fram_write_byte(FRAM_ADDR_EN_MULTI,     0);
    fram_write_u32 (FRAM_ADDR_ENERGY,       cfg->limit_energy);
    fram_write_u16 (FRAM_ADDR_MULTI_T1,     cfg->limit_mo_time1);
    fram_write_u16 (FRAM_ADDR_MULTI_T2,     cfg->limit_mo_time2);
    fram_write_u16 (FRAM_ADDR_MULTI_O1,     cfg->limit_mo_out1);
    fram_write_u16 (FRAM_ADDR_MULTI_O2,     cfg->limit_mo_out2);
    fram_write_byte(FRAM_ADDR_TIMEOVER,     (uint8_t)cfg->limit_out_time);
    fram_write_byte(FRAM_ADDR_COMM_ADDR,    cfg->comm_address);
    fram_write_byte(FRAM_ADDR_COMM_SPEED,   cfg->comm_speed_idx);
    fram_write_byte(FRAM_ADDR_COMM_PARITY,  cfg->comm_parity_idx);
    fram_write_u16 (FRAM_ADDR_CAL_VAL,      (uint16_t)cfg->cal_val);
    fram_write_u16 (FRAM_ADDR_FREQ_CAL_VAL, (uint16_t)cfg->freq_cal_val);
    fram_write_byte(FRAM_ADDR_COMM_MODE,    cfg->comm_mode);
    fram_write_u32 (FRAM_ADDR_ETHER_IP,     0);   /* serial default → zero IP/NM/GW */
    fram_write_u32 (FRAM_ADDR_ETHER_NM,     0);
    fram_write_u32 (FRAM_ADDR_ETHER_GW,     0);
}

void app_config_load(app_config_t *cfg)
{
    if (fram_read_byte(FRAM_ADDR_INIT_FLAG) != FRAM_INIT_FLAG_MAGIC) {
        fram_write_byte(FRAM_ADDR_INIT_FLAG, FRAM_INIT_FLAG_MAGIC);
        app_config_factory_write(cfg);
        return;
    }

    cfg->work_cnt            = fram_read_u32 (FRAM_ADDR_WORK_CNT);
    cfg->limit_delay_time1   = fram_read_u16 (FRAM_ADDR_DELAY1);
    cfg->limit_delay_time2   = fram_read_u16 (FRAM_ADDR_DELAY2);
    cfg->limit_delay_time3   = fram_read_u16 (FRAM_ADDR_DELAY3);
    cfg->limit_trigger_time2 = fram_read_u16 (FRAM_ADDR_TRIGGER2);
    cfg->limit_trigger_time3 = fram_read_u16 (FRAM_ADDR_TRIGGER3);
    cfg->f_safty             = fram_read_byte(FRAM_ADDR_SAFTY);
    cfg->run_mode            = fram_read_byte(FRAM_ADDR_RUN_MODE);
    cfg->model_freq          = fram_read_byte(FRAM_ADDR_MODEL_FREQ);
    cfg->model_type          = fram_read_byte(FRAM_ADDR_MODEL_TYPE);
    cfg->output_power        = fram_read_byte(FRAM_ADDR_OUT_POWER);
    cfg->limit_on_time       = fram_read_u16 (FRAM_ADDR_ON_TIME);
    cfg->energy_ctrl         = (fram_read_byte(FRAM_ADDR_EN_ENERGY) == 1);
    cfg->limit_energy        = fram_read_u32 (FRAM_ADDR_ENERGY);
    cfg->multi_ctrl          = (fram_read_byte(FRAM_ADDR_EN_MULTI) == 1);
    if (cfg->limit_energy > 100000) {                 /* samd20 clamp (main.c:923) */
        cfg->limit_energy = 100000;
        fram_write_u32(FRAM_ADDR_ENERGY, cfg->limit_energy);
    }
    cfg->limit_mo_time1      = fram_read_u16 (FRAM_ADDR_MULTI_T1);
    cfg->limit_mo_time2      = fram_read_u16 (FRAM_ADDR_MULTI_T2);
    cfg->limit_mo_out1       = fram_read_u16 (FRAM_ADDR_MULTI_O1);
    cfg->limit_mo_out2       = fram_read_u16 (FRAM_ADDR_MULTI_O2);
    cfg->limit_out_time      = fram_read_byte(FRAM_ADDR_TIMEOVER);
    cfg->comm_address        = fram_read_byte(FRAM_ADDR_COMM_ADDR);
    cfg->comm_speed_idx      = fram_read_byte(FRAM_ADDR_COMM_SPEED);
    cfg->comm_parity_idx     = fram_read_byte(FRAM_ADDR_COMM_PARITY);
    cfg->cal_val             = (int16_t)fram_read_u16(FRAM_ADDR_CAL_VAL);
    cfg->freq_cal_val        = (int16_t)fram_read_u16(FRAM_ADDR_FREQ_CAL_VAL);
    cfg->comm_mode           = fram_read_byte(FRAM_ADDR_COMM_MODE);
    for (int i = 0; i < 4; i++) {
        cfg->ether_ip[i] = fram_read_byte((uint8_t)(FRAM_ADDR_ETHER_IP + i));
        cfg->ether_nm[i] = fram_read_byte((uint8_t)(FRAM_ADDR_ETHER_NM + i));
        cfg->ether_gw[i] = fram_read_byte((uint8_t)(FRAM_ADDR_ETHER_GW + i));
    }
}
```

- [ ] **Step 3: Verify build is green**

Run: `cd fw && env -u STM32_TOOLCHAIN cmake -B build -G Ninja && env -u STM32_TOOLCHAIN cmake --build build`
Expected: build succeeds, 0 warnings.

Static trace vs samd20 `var_init`: read path order/widths match main.c:897-942; the `>100000` energy clamp +
write-back is preserved; `cal_val`/`freq_cal_val` loaded via `(int16_t)` cast (no `fram_read_i16` exists).

- [ ] **Step 4: Commit**

```bash
git add fw/include/app_config.h fw/src/app_config.c
git commit -m "feat(config): load FRAM config with factory-default-on-blank (full samd20 map)"
```

---

## Task 6: `app_lcd` — `init_lcd_mode` port (scope a)

**Files:**
- Create: `fw/include/app_lcd.h`, `fw/src/app_lcd.c`

- [ ] **Step 1: Create the header `fw/include/app_lcd.h`**

```c
/* fw/include/app_lcd.h — LCD application data setup (samd20 init_lcd_mode port, scope a). */
#pragma once
#include "app_config.h"

/* GDSONIC model string -> MODEL_NAME (11-byte payload incl. trailing NUL). */
void app_lcd_send_model_str(uint8_t freq, uint8_t type);

/* Pre-fill RUN-page VPs from cfg, then switch to the run page. */
void app_lcd_init_mode(const app_config_t *cfg);
```

- [ ] **Step 2: Create `fw/src/app_lcd.c`** (port of samd20 main.c:2376-2426 + 3216-3230 + 2947-2955)

```c
/* fw/src/app_lcd.c — samd20 init_lcd_mode port (scope a: RUN-page bring-up, no string formatting). */
#include "app_lcd.h"
#include "dgus_lcd.h"

void app_lcd_send_model_str(uint8_t freq, uint8_t type)
{
    /* GDSONIC build (samd20 main.c:2379-2426). Wire payload = 11 bytes incl. NUL at [10]. */
    uint8_t s[11];
    s[0] = 'G'; s[1] = 'D'; s[2] = 'S'; s[3] = '-';
    switch (freq) {
        case 0:  s[4] = '1'; s[5] = '5'; break;
        case 1:  s[4] = '2'; s[5] = '0'; break;
        case 2:  s[4] = '3'; s[5] = '0'; break;
        case 3:  s[4] = '3'; s[5] = '5'; break;
        case 4:  s[4] = '4'; s[5] = '0'; break;
        case 5:  s[4] = '5'; s[5] = '0'; break;
        default: s[4] = '1'; s[5] = '5'; break;
    }
    switch (type) {
        case 0:  s[6] = 'H'; break;
        case 1:  s[6] = 'M'; break;
        case 2:  s[6] = 'S'; break;
        default: s[6] = 'H'; break;
    }
    s[7] = ' '; s[8] = ' '; s[9] = ' '; s[10] = '\0';
    dgus_write_bytes(MODEL_NAME, s, 11);
}

void app_lcd_init_mode(const app_config_t *cfg)
{
    uint8_t run_page;

    app_lcd_send_model_str(cfg->model_freq, cfg->model_type);

    if      (cfg->model_type == 0) run_page = LCD_RUN_HAND;    /* 3 */
    else if (cfg->model_type == 1) run_page = LCD_RUN_MULTI;   /* 3 */
    else                           run_page = LCD_RUN_STD;     /* 9 */

    /* init_lcd_mode VP pre-fill (main.c:3216-3228) */
    dgus_write_u16(ICON_RESET, 0);
    dgus_write_u16(ICON_SEEK,  0);
    dgus_write_u16(ICON_RUN,   0);
    dgus_write_u16(LV_DM_DELAY, cfg->limit_delay_time1);
    dgus_write_u16(LV_DM_WELD,  cfg->limit_delay_time2);
    dgus_write_u16(LV_DM_HOLD,  cfg->limit_delay_time3);
    dgus_write_u16(LV_TM_WELD,  cfg->limit_trigger_time2);
    dgus_write_u16(LV_TM_HOLD,  cfg->limit_trigger_time3);
    dgus_write_u32(LV_WORK_CNT, cfg->work_cnt);
    dgus_write_u16(LV_ENERGY_EDIT, (uint16_t)(cfg->limit_energy / 10));
    dgus_write_u16(DISP_HORNDOWN, 0);

    /* change_lcd_page unconditional top (main.c:2947-2955) */
    dgus_write_u16(DISP_ENERGY_EN, cfg->energy_ctrl ? 1 : 0);
    dgus_write_u16(DISP_MULTI_EN,  cfg->multi_ctrl  ? 1 : 0);

    dgus_set_page(run_page);
}
```

- [ ] **Step 3: Verify build is green**

Run: `cd fw && env -u STM32_TOOLCHAIN cmake -B build -G Ninja && env -u STM32_TOOLCHAIN cmake --build build`
Expected: build succeeds, 0 warnings.

Static trace: all called functions exist in Stage A `dgus_lcd.h` (`dgus_write_bytes/u16/u32`, `dgus_set_page`);
all VP macros exist (Task 1 + Stage A `MODEL_NAME`/`LV_WORK_CNT`). `limit_energy/10` = 10000 at default (fits u16).

- [ ] **Step 4: Commit**

```bash
git add fw/include/app_lcd.h fw/src/app_lcd.c
git commit -m "feat(lcd): port init_lcd_mode — model str + RUN-page VP pre-fill"
```

---

## Task 7: Boot sequence integration

**Files:**
- Modify: `fw/src/main.c`, `fw/src/app.c`

- [ ] **Step 1: Call `i2c1_init()` in `main.c`**

In `fw/src/main.c`, add the include with the other driver includes:

```c
#include "i2c1.h"
```

and add the init call immediately after `usart1_init();`:

```c
    i2c1_init();       /* Stage B: I2C1 @400kHz (PB6/PB7) for FRAM */
```

- [ ] **Step 2: Replace the `app_init()` body in `app.c`**

In `fw/src/app.c`, replace the includes block and `app_init()` (keep `app_loop_iter()` unchanged):

Update the includes at the top to:

```c
/* fw/src/app.c — Stage B: FRAM config load + LCD init_mode + liveness cadence */
#include "stm32f4xx_hal.h"
#include "app.h"
#include "board.h"
#include "sys_tick.h"
#include "mon.h"
#include "dgus_lcd.h"
#include "app_config.h"
#include "app_lcd.h"

static app_config_t g_cfg;
```

Replace the whole `app_init(void)` function with:

```c
void app_init(void)
{
    sys_tick_init();
    mon_init();
    mon_writeln("[boot] gds_us_ctrl stage-b ready");

#if DGUS_DEMO_RESET_ON_BOOT
    dgus_reset_lcd();
#endif

    dgus_set_page(LCD_LOGO);          /* page 0 — logo */
    sys_tick_delay_ms(1000);          /* DGUS T5L boot settle (TIM11 tick, not HAL_Delay) */

    app_config_load(&g_cfg);          /* FRAM read; factory-write on blank (0xAA flag) */
    app_lcd_init_mode(&g_cfg);        /* model str + VP pre-fill + set_page(run) */

    mon_printf("[cfg] freq=%u type=%u work=%lu energy=%lu en_e=%u en_m=%u\r\n",
               (unsigned)g_cfg.model_freq, (unsigned)g_cfg.model_type,
               (unsigned long)g_cfg.work_cnt, (unsigned long)g_cfg.limit_energy,
               (unsigned)g_cfg.energy_ctrl, (unsigned)g_cfg.multi_ctrl);
}
```

`app_loop_iter()` is unchanged: it keeps draining DGUS RX and the 1 Hz `VAR_POWER` liveness write
(`DGUS_DEMO_UPTIME_VP`) from Stage A — now cosmetic, not the boot path.

- [ ] **Step 3: Verify build is green**

Run: `cd fw && env -u STM32_TOOLCHAIN cmake -B build -G Ninja && env -u STM32_TOOLCHAIN cmake --build build`
Expected: build succeeds, 0 warnings.

Static trace: boot order = `sys_tick_init` (starts `s_ms`) → … → `dgus_set_page(LCD_LOGO)` →
`sys_tick_delay_ms(1000)` (valid now that tick runs) → `app_config_load` (i2c1 already init in `main`) →
`app_lcd_init_mode`. `LCD_LOGO` exists in `dgus_lcd.h`.

- [ ] **Step 4: Commit**

```bash
git add fw/src/main.c fw/src/app.c
git commit -m "feat(app): Stage B boot — logo, FRAM config load, LCD init_mode"
```

---

## Task 8: Full build verification + size report

**Files:** none (verification only)

- [ ] **Step 1: Clean configure + build**

Run:
```bash
cd fw && rm -rf build && env -u STM32_TOOLCHAIN cmake -B build -G Ninja && env -u STM32_TOOLCHAIN cmake --build build 2>&1 | tee /tmp/stageb_build.log
```
Expected: configures, builds `gds_us_ctrl.elf`, **0 warnings**, prints memory usage.

- [ ] **Step 2: Report size**

Run: `arm-none-eabi-size fw/build/gds_us_ctrl.elf`
Expected: sanity-check growth over Stage A baseline (text 24336 / data 476 / bss 2276). I2C HAL + Stage B
code adds roughly a few KB FLASH; RAM grows by `sizeof(app_config_t)` + `hi2c1` handle. No hard target — record actuals.

- [ ] **Step 3: Final spec-compliance pass (review, not code)**

Confirm against the spec: scope (a) only (no SETUP pages, no `DISP_STD_DATA*` string formatting, no I2C_POT write);
factory map (β) full; default boot page = `LCD_RUN_HAND` (model_type 0); big-endian FRAM access; `GPIO_NOPULL`.

- [ ] **Step 4: Commit (if any review-driven fixes were made; otherwise skip)**

```bash
git add -A && git commit -m "chore(stage-b): build verify + size report"
```

---

## Deferred to a follow-up HW task (board on bench — NOT this plan)

- I2C1 ACK from slave 0x50; `i2c1_err_count()` stays 0 after `app_config_load`.
- FRAM round-trip: read `FRAM_ADDR_INIT_FLAG` (expect 0xAA after first boot), verify config bytes via GDB `mdb`.
- Visual: panel shows logo, then RUN_HAND with "GDS-15H" + delay/trigger fields = 50, work_cnt = 0.
- USART6 `[cfg]` log line matches loaded values.
- Confirm PCLK1 = 48 MHz yields a clean 400 kHz SCL on scope.
