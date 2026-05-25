# Stage B — LCD Application Data Setup (FRAM → init_lcd_mode) Design

> Status: DRAFT for user review · Date: 2026-05-25 · Branch: `feat/stage-b-lcd-app`
> Predecessor: Stage A LCD I/O (`hw-revA_fw-stage-a`) — DGUS 9-function wire layer in place.
> Port source (read-only): `ref/samd20/main.c`, `ref/samd20/define.h`, `ref/samd20/i2c_comm.c`.

---

## 1. Goal & Scope

Add the application **data pre-fill** layer above Stage A's DGUS wire layer, so the panel performs a real
boot transition `LOGO → RUN page` with its value/icon fields populated from persisted configuration —
porting samd20 `init_lcd_mode()` (main.c:3175). Configuration is loaded from the on-board **I2C FRAM**.

### 1.1 In scope (user decision: scope "(a) minimal RUN-page bring-up")

- I2C1 peripheral bring-up (PB6/PB7).
- FM24C16B FRAM device driver (read + write).
- Config load layer = port of samd20 `var_init()` boot section + `factory_init()` (init-flag-aware).
- `init_lcd_mode()` port: model string + numeric/icon VP pre-fill + page switch to the RUN page.

### 1.2 Out of scope (deferred)

- `change_lcd_page()` per-page **string formatting** (`DISP_STD_DATA1/2/3` via `time2str`/`energy2str`) — scope (b).
- All SETUP pages (`LCD_SETUP_*`), comm/ethernet pages — scope (c); pull in `I2C_POT`, W5500/Modbus.
- `ref_lv_*` current thresholds (control domain, not LCD).
- `I2C_POT` (U4) writes (control/output domain).
- Real-HW verification (board not available this session — see §9).

---

## 2. Hardware facts (cross-verified against `hw/schematics/usw_ctrl_v30`)

| Fact | Value | Source |
|---|---|---|
| MCU | STM32F410RBT (U2, LQFP64) | netlist pins SWDIO/PA13, SWCLK/PA14, USART1 PA9/PA10, USART6 PC6/PC7 |
| I2C bus | **I2C1**, PB6=SCL (U2.58), PB7=SDA (U2.59), **AF4**, APB1 | netlist `M_SCL`/`M_SDA`; `GPIO_AF4_I2C1` |
| FRAM part | **FM24C16B** (FRAM; schematic *symbol* says MR44V064B — BOM mismatch, user-confirmed actual = FM24C16B) | user 2026-05-25 |
| FRAM addressing | 7-bit slave **0x50**, **1-byte word address**, page 0 only (config at addr 0–56) | `I2C_FRAM=0x50` (i2c_comm.h); A0/A1/A2=GND |
| samd20 mode match | `fram_mode = 1 // fm24c16` (active path; MR44V064 2-byte path commented out) | main.c:887 |
| Write protect | PP/WP (pin7) = GND → writes always enabled | netlist U1.7=GND |
| Pull-ups | R1/R2 = 10 kΩ → **VCC_5 (5 V)** | schematic; firmware uses `GPIO_NOPULL` |
| 5 V tolerance | PB6/PB7 are FT (5 V-tolerant) on STM32F4 → 5 V I2C idle OK | datasheet |
| Bus sharing | U4 digital pot (I2C_POT @ 0x28) shares I2C1 — not touched this stage | netlist |

**FRAM (not EEPROM) ⇒ no write-cycle delay / no ACK-polling.** Writes complete at bus speed; factory-write of
~30 defaults is cheap. This simplifies the driver vs a 24Cxx EEPROM.

---

## 3. Architecture — layered mirror of Stage A (`usart1 → dgus_lcd → app`)

```
i2c1   (drivers/i2c1.c)      peripheral: HAL I2C1 @ 400 kHz, PB6/PB7 AF4, NOPULL
   │   thin blocking wrappers over HAL_I2C_Mem_Read/Write (MEMADD_8BIT)
   ▼
fram   (drivers/fram.c)      device: FM24C16B @ 0x50; read/write byte|u16|u32 (big-endian); FRAM_ADDR_* map
   │
   ▼
app_config (src/app_config.c)  policy: app_config_t + app_config_load() (init-flag 0xAA, factory-default-on-blank)
   │
   ▼
app_lcd (src/app_lcd.c)        view: app_lcd_init_mode(cfg) — model str + VP pre-fill + set_page(run)
   │   (reuses Stage A dgus_* wire functions; no new wire functions)
   ▼
app.c (modified)             boot sequence integration
```

Stage A's DGUS 9 functions are reused unchanged. The only `dgus_lcd.h` change is **adding VP macros**.

### 3.1 File plan

| File | New/Mod | Purpose |
|---|---|---|
| `fw/drivers/i2c1.c`, `fw/include/i2c1.h` | new | I2C1 init + blocking mem read/write wrappers |
| `fw/drivers/fram.c`, `fw/include/fram.h` | new | FM24C16B driver + `FRAM_ADDR_*` map (mirror samd20 `define.h`) |
| `fw/src/app_config.c`, `fw/include/app_config.h` | new | `app_config_t`, `app_config_load()`, `app_config_factory_write()` |
| `fw/src/app_lcd.c`, `fw/include/app_lcd.h` | new | `app_lcd_init_mode()` (= `init_lcd_mode` port) |
| `fw/include/dgus_lcd.h` | mod | add VP macros (§6) |
| `fw/src/app.c` | mod | boot sequence (§7) |
| `fw/src/periph.c`, `fw/include/periph.h` | mod | `I2C_HandleTypeDef hi2c1` single definition |
| `fw/src/sys_tick.c`, `fw/include/sys_tick.h` | mod | add `sys_tick_delay_ms()` busy-wait helper (boot settle) |
| `fw/CMakeLists.txt` | mod | add HAL `stm32f4xx_hal_i2c.c` (app sources auto-globbed) |
| `fw/vendor/Core/Inc/stm32f4xx_hal_conf.h` | mod | enable `HAL_I2C_MODULE_ENABLED` (project HAL config — same file that enables UART/TIM; required or I2C types/funcs don't compile) |

---

## 4. `i2c1` — peripheral layer

- `hi2c1`: `I2C1`, `ClockSpeed = 400000` (Fast mode), `DutyCycle = I2C_DUTYCYCLE_2`, 7-bit, no dual-addr,
  no general call, no stretch disable. GPIO: PB6/PB7 `AF4`, `OPEN_DRAIN`, `PULLUP = NOPULL`, `SPEED_HIGH`.
  Clocks: `__HAL_RCC_I2C1_CLK_ENABLE`, `__HAL_RCC_GPIOB_CLK_ENABLE`.
- I2C1 derives its CCR from **PCLK1 (APB1)** — implementer confirms PCLK1 from `src/clock.c` so HAL computes a
  valid 400 kHz CCR (no TBD: read the value, don't guess).
- API (blocking, FRAM word-address model):
  - `HAL_StatusTypeDef i2c1_mem_read(uint8_t dev7, uint8_t mem_addr, uint8_t *buf, uint16_t n);`
  - `HAL_StatusTypeDef i2c1_mem_write(uint8_t dev7, uint8_t mem_addr, const uint8_t *buf, uint16_t n);`
  - Internally `HAL_I2C_Mem_{Read,Write}(&hi2c1, dev7<<1, mem_addr, I2C_MEMADD_SIZE_8BIT, …, I2C1_TIMEOUT_MS)`.
- `I2C1_TIMEOUT_MS = 50` (named constant). On non-OK status, increment an observability counter
  (`i2c1_err_count`, mirrors Stage A `usart1_rx_error_count` pattern) — no silent failure.

---

## 5. `fram` — FM24C16B device driver

`FRAM_I2C_ADDR_7B = 0x50`. Addressing: 1 byte. All accesses target page 0.

> **Hard invariants** (FM24C16 page-0 model):
> - `FRAM_I2C_ADDR_7B` is the constant `0x50`, **never** OR'd with page/address bits.
> - Every read/write satisfies `mem_addr + n ≤ 256`. (A read/write crossing 256 silently wraps to
>   offset 0 of the page on FM24C16.) Max contiguous access this stage = the 4-byte u32 at
>   `FRAM_ADDR_ENERGY=22` (ends at 25) — within bound. Extending past 256 is a future `fram.c` change,
>   not in-scope creep.

Read/write helpers mirror samd20 byte order **exactly** (big-endian, MSB first — see main.c:578/660/685):

| Function | Bytes | Order (matches samd20) |
|---|---|---|
| `uint8_t  fram_read_byte(uint8_t a)` | 1 | — |
| `uint16_t fram_read_u16(uint8_t a)` | 2 | `(buf[0]<<8) \| buf[1]` |
| `uint32_t fram_read_u32(uint8_t a)` | 4 | `buf[0]` = MS byte … `buf[3]` = LS byte |
| `void fram_write_byte(uint8_t a, uint8_t v)` | 1 | — |
| `void fram_write_u16(uint8_t a, uint16_t v)` | 2 | `buf[0]=v>>8; buf[1]=v` |
| `void fram_write_u32(uint8_t a, uint32_t v)` | 4 | `buf[0]=v>>24 … buf[3]=v` |

> **Port-correctness note:** Cortex-M4 is little-endian; byte assembly MUST be explicit (shifts/masks),
> never `memcpy` of the raw word, to preserve samd20's on-wire big-endian layout.

### 5.1 `FRAM_ADDR_*` map (verbatim from samd20 `define.h:44-73`)

```
FRAM_ADDR_INIT_FLAG   0   (1B, magic 0xAA)
FRAM_ADDR_WORK_CNT    1   (4B)
FRAM_ADDR_DELAY1      5   (2B)   DELAY2 7   DELAY3 9
FRAM_ADDR_TRIGGER2   11   (2B)   TRIGGER3 13
FRAM_ADDR_SAFTY      15   (1B)   RUN_MODE 16
FRAM_ADDR_MODEL_FREQ 17   (1B)   MODEL_TYPE 18   OUT_POWER 19
FRAM_ADDR_ON_TIME    20   (2B)   ENERGY 22 (4B)
FRAM_ADDR_MULTI_T1   26 / T2 28 / O1 30 / O2 32   (2B each)
FRAM_ADDR_EN_ENERGY  34   EN_MULTI 35   TIMEOVER 36
FRAM_ADDR_COMM_ADDR  37   COMM_SPEED 38   COMM_PARITY 39
FRAM_ADDR_CAL_VAL    40 (2B)   FREQ_CAL_VAL 42 (2B)
FRAM_ADDR_COMM_MODE  44   ETHER_IP 45 (4B)   ETHER_NM 49 (4B)   ETHER_GW 53 (4B)
```

---

## 6. `app_config` — load / factory (decision: full samd20 map "(β)")

```c
typedef struct {
    uint8_t  model_freq, model_type;     /* LCD: model string + run-page select */
    uint8_t  f_safty, run_mode;
    uint16_t limit_delay_time1, limit_delay_time2, limit_delay_time3;
    uint16_t limit_trigger_time2, limit_trigger_time3;
    uint16_t limit_on_time, limit_out_time;
    uint16_t limit_mo_out1, limit_mo_out2, limit_mo_time1, limit_mo_time2;
    uint8_t  output_power;
    uint32_t work_cnt, limit_energy;
    bool     energy_ctrl, multi_ctrl;
    int16_t  cal_val, freq_cal_val;
    uint8_t  comm_address, comm_speed_idx, comm_parity_idx, comm_mode;
    uint8_t  ether_ip[4], ether_nm[4], ether_gw[4];
} app_config_t;
```

`void app_config_load(app_config_t *cfg)` — port of samd20 `var_init()` boot section (main.c:887-943):

1. If `fram_read_byte(FRAM_ADDR_INIT_FLAG) != 0xAA`:
   `fram_write_byte(FRAM_ADDR_INIT_FLAG, 0xAA)` → `app_config_factory_write(cfg)` (writes FRAM **and** fills `cfg`).
2. Else: read every field from FRAM into `cfg` (byte/u16/u32 per map widths).
   Signed fields load through a cast: `cfg->cal_val = (int16_t)fram_read_u16(FRAM_ADDR_CAL_VAL)`
   (and `freq_cal_val` likewise) — there is no `fram_read_i16`; samd20 reads u16 and casts (main.c:3292).

`void app_config_factory_write(app_config_t *cfg)` — port of samd20 `factory_init()` (main.c:753-828),
**(β) full map verbatim** (writes all addresses incl. comm/ether so the FRAM layout matches samd20 1:1 and
Stage C only reads). Defaults consumed by the LCD this stage:

| Field | Default | LCD use |
|---|---|---|
| `model_freq` | 0 (15K) | model string "GDS-15…" |
| `model_type` | 0 (hand) | **run page = LCD_RUN_HAND (3)** (decision §8.1) |
| `work_cnt` | 0 | `LV_WORK_CNT` |
| `limit_delay_time1/2/3` | 50 / 50 / 50 | `LV_DM_DELAY/WELD/HOLD` |
| `limit_trigger_time2/3` | 50 / 50 | `LV_TM_WELD/HOLD` |
| `limit_energy` | 100000 | `LV_ENERGY_EDIT` = energy/10 = 10000 |
| `energy_ctrl` | false | `DISP_ENERGY_EN` = 0 |
| `multi_ctrl` | false | `DISP_MULTI_EN` = 0 |

Remaining defaults (`output_power=50`, `limit_on_time=50`, `limit_mo_*`=25/50/25/50, `limit_out_time=10`,
`comm_address/speed/parity=0`, `cal_val=0`, `freq_cal_val=0`, `comm_mode=COMM_SERIAL`, ether=0) are written
verbatim from samd20 `factory_init` but not consumed by the LCD this stage.

> **Port-correctness fix:** samd20 leaves `multi_ctrl` (and `energy_ctrl` on factory path) to bss-zero implicitly.
> Our `app_config_factory_write` sets both explicitly = false. (NEXT_STEPS §4.4 drift pattern.)

### 6.1 New VP macros to add to `dgus_lcd.h` (verbatim from samd20 `dgus_lcd.h`)

Already present: `MODEL_NAME 0x1000`, `LV_WORK_CNT 0x1006`.
Add: `LV_ENERGY_EDIT 0x1202`, `LV_DM_DELAY 0x1203`, `LV_DM_WELD 0x1204`, `LV_DM_HOLD 0x1205`,
`LV_TM_WELD 0x1206`, `LV_TM_HOLD 0x1207`, `DISP_HORNDOWN 0x1209`, `DISP_ENERGY_EN 0x120a`,
`DISP_MULTI_EN 0x120b`, `ICON_RESET 0x1150`, `ICON_SEEK 0x1151`, `ICON_RUN 0x1152`.

---

## 7. `app_lcd` — `init_lcd_mode` port (scope a)

`void app_lcd_init_mode(const app_config_t *cfg)` — port of samd20 main.c:3175-3234 + the unconditional
top of `change_lcd_page` (main.c:2947-2955), **excluding** all per-page string branches:

1. `app_lcd_send_model_str(cfg->model_freq, cfg->model_type)` — `GDSONIC` build: 11 bytes
   `"GDS-" + freq2digits + typechar + "   "` to `MODEL_NAME` via `dgus_write_bytes(MODEL_NAME, buf, 11)`.
   freq: 0→"15" 1→"20" 2→"30" 3→"35" 4→"40" 5→"50"; type: 0→'H' 1→'M' 2→'S'.
   Wire payload bytes [0..3]="GDS-", [4..5]=freq, [6]=type, [7..9]=' ', **[10]=0x00 (literal NUL in the
   payload, sent on the wire — samd20 verbatim)**.
2. run page: type 0→`LCD_RUN_HAND`(3), 1→`LCD_RUN_MULTI`(3), 2→`LCD_RUN_STD`(9).
3. VP pre-fill (all `dgus_write_u16` unless noted):
   `ICON_RESET=0`, `ICON_SEEK=0`, `ICON_RUN=0`,
   `LV_DM_DELAY=delay1`, `LV_DM_WELD=delay2`, `LV_DM_HOLD=delay3`,
   `LV_TM_WELD=trigger2`, `LV_TM_HOLD=trigger3`,
   `LV_WORK_CNT=work_cnt` (**`dgus_write_u32`**), `LV_ENERGY_EDIT=limit_energy/10`,
   `DISP_HORNDOWN=0`, `DISP_ENERGY_EN=energy_ctrl?1:0`, `DISP_MULTI_EN=multi_ctrl?1:0`.
4. `dgus_set_page(run_page)`.

`app_lcd_send_model_str` is exposed (header) so Stage C / future model-setup can reuse it.

---

## 8. Boot sequence (`app.c`)

```c
app_init():
    sys_tick_init(); mon_init(); mon_writeln("[boot] gds_us_ctrl stage-b ready");
    /* usart1/dgus already init in Stage A path */
    i2c1_init();
    dgus_set_page(LCD_LOGO);          /* page 0 — logo */
    sys_tick_delay_ms(1000);          /* DGUS T5L boot settle. NEW helper in sys_tick (busy-wait on
                                         sys_tick_get_ms). NOT HAL_Delay — this project runs TIM11 tick,
                                         not HAL's SysTick, so HAL_Delay would hang. */
    app_config_load(&g_cfg);          /* FRAM read, or factory-write on blank */
    app_lcd_init_mode(&g_cfg);        /* VP pre-fill + set_page(run) */
    mon_printf("[cfg] flag=%s freq=%u type=%u work=%lu\r\n", …);  /* observability */
```

`g_cfg` is a single file-scope `app_config_t` in `app.c`.

### 8.1 Resolved micro-decisions (user)

- **Factory default run page** = samd20 verbatim → `model_type=0` (hand) → boots to `LCD_RUN_HAND` (page 3),
  model string "GDS-15H   ". (Not forced to std/page 9.)
- **1 Hz `VAR_POWER` demo** = retained as a liveness tick in `app_loop_iter` (writes uptime seconds to
  `VAR_POWER`), unchanged from Stage A; it is now cosmetic liveness, not the boot path.
  `VAR_POWER` (0x1110) is a RUN_STD VP; on the default RUN_HAND/MULTI boot page the write is harmless
  (DGUS ignores writes to VPs not on the active page). The `[cfg]` USART6 log line is the authoritative
  boot heartbeat regardless of page.

---

## 9. Verification (this session: code/build only — board unavailable)

- `arm-none-eabi-gcc -fsyntax-only` → exit 0, **0 warnings** (clangd LSP noise ignored).
- `env -u STM32_TOOLCHAIN cmake --build build` → success; report `arm-none-eabi-size` FLASH/RAM deltas.
- Logic correctness by **code review + static trace**: init-flag branch, byte order, VP payloads/addresses,
  run-page mapping, model-string bytes.
- Deferred to a follow-up HW Task (board on bench): I2C1 ACK from 0x50, write-then-read self-verify of
  `FRAM_ADDR_INIT_FLAG`/config round-trip, DGUS panel visual `LOGO→RUN_HAND`, USART6 `[cfg]` log.

---

## 10. Anticipated drift / risks (NEXT_STEPS §4.4 pattern)

1. PCLK1 value vs 400 kHz CCR — confirm from `clock.c`. Expected: SYSCLK 96 MHz, AHB/1 = 96, APB1/2 =
   **PCLK1 48 MHz** (F410 APB1 max 50 MHz) → HAL Fm CCR computes cleanly. Verify, don't assume.
2. `multi_ctrl`/`energy_ctrl` factory-path init (fix in §6).
3. Big-endian byte assembly on little-endian M4 (fix in §5).
4. `LCD_RUN_HAND == LCD_RUN_MULTI == 3` (same page id) — intentional per samd20; document, not a bug.
5. 5 V I2C idle on PB6/PB7 — HW design choice; firmware-side only `GPIO_NOPULL`.

---

## 11. References

- Port source: `ref/samd20/main.c` (`send_model_str` 2376, `change_lcd_page` 2942, `init_lcd_mode` 3175,
  `var_init` 832, `factory_init` 753, `*_fram` 578-729), `ref/samd20/define.h` (44-73), `ref/samd20/i2c_comm.h`.
- HW: `hw/schematics/usw_ctrl_v30.pdf` + `USW_CTRL_V30.asc`.
- Stage A wire layer: `fw/include/dgus_lcd.h`, `fw/drivers/dgus_lcd.c`.
- Conventions: `CLAUDE.md`, `docs/pinmap.md` (I2C1 PB6/PB7), `docs/NEXT_STEPS.md` §4.
