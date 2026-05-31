# Stage D — Slice 1: Ultrasonic Regulation Core (STM32F410 port) — DESIGN

> **Status (2026-05-31):** spec-first draft, **awaiting user review**. Output-binding decision = **measure-first** (user, 2026-05-31): the OSC physical drive (B-SEAM) waits on a bench sweep of the original board; this slice ports the **compute pipeline + display provider + safe board fixes** in parallel, leaving the GPIO output stage as a documented `DEFERRED` section.
> **Evidence base (authoritative):** `docs/superpowers/analysis/2026-05-31-m16-regulation-core-verified.md` (disasm-adjudicated, 55-agent workflow). All "verified fact (ID)" references below point at that doc's §3/§4. Memory: `project_m16_regulation_verified`.
> **Confirmed decisions:** DP1 = superloop-polled ADC (no ISR). DP2 = carry raw counts in the AVR domain + calibrate by measurement (§10). DP3 = spec → user review → `writing-plans` → implement (compute now, output stage after measurement).

---

## 0. Scope

**IN (this slice — compute pipeline, no physical output):**
1. ADC acquisition driver: 2-channel (PB0/ADC1_IN8 = ch0 sense feedback, PB1/ADC1_IN9 = ch1), superloop-polled, ch0 mean-of-10, ch1 mean-of-50.
2. Scaling transfer: `clip(≥1000→1000) / floor(<3→0) / else ×6` → `adc_scaled_value`.
3. Lookup thermometer: 21-entry descending table compare → progressive drive **pattern** (computed and held in regulation state; **not** written to GPIO this slice).
4. ~2 ms regulation cadence in `app_loop_iter()`.
5. Measure provider: replace the all-zero `app_lcd_measure()` stub with live regulation values (amp + scaled level), so the LCD bars show the loop is alive.
6. `board.c` safe fixes: correct the OSC pin mask; idle-HIGH the **3 confirmed** OSC outputs only.

**DEFERRED (§9 — pending bench measurement, B-SEAM/B-OSC-MAP):**
- The physical OSC drive: how the computed pattern/level reaches OSC0..4; active level & bit-order; the `set_pot` analog route.
- OSC2/OSC3 (STM32 PB12/PB13 ← mega16 PC4/PA7) — DDR-inputs in the image; direction unconfirmed → **not** initialized as outputs.

**OUT (later slices — confirmed but not here):**
- Soft-start ramp (`main_loop_process`, 10-rung, Timer1 ~10 ms) = **slice 2**.
- Command FSM (us_start/seek/reset), overload detect/flag, blink phase = slice 2+.
- 7-seg `display_*` path (board-dead), comm IPC (single-MCU) = never.

---

## 1. Why a faithful port needs the verified analysis (not the v001 decompile)

The disasm overturned **5** v001 claims a naive port would have shipped wrong (analysis §4.2). The two load-bearing for this slice:

| v001 said | disasm truth | impact on this slice |
|-----------|--------------|----------------------|
| scaling `>>6` | **`×6` multiply** (`1ac2 ldi r30,0x06` + `1ac4 call 0x2158`; zero shift opcodes in `0x1A5C-0x1AD0`) | §4 transfer uses `×6`. `>>6` is impossible: it never reaches the 1000 clip; `×6` does. |
| free-run ADC | **single-shot SW re-arm** (ADATE=0 in both 0xCD/0xCF) | §3 acquisition is poll/re-arm, matches DP1 superloop. |
| compare `table[i] > adc` | **`table[i] < adc`** (strictly-less, `cp/cpc`@0x202a + `brcs`) | §5 select direction. |

Everything in §3–§6 below is a `confirmed` verdict in the analysis doc. Function names (`output_level_process`, `lookup_table`) are v001-derived labels; the disasm proves addresses/data/flow, not names.

---

## 2. Module architecture

```
app_loop_iter()                         (fw/src/app.c — existing superloop)
 ├─ dgus_rx_poll drain                  (existing)
 ├─ regulation tick  ~2 ms  ───────────────► app_reg_tick()        (NEW: fw/src/app_reg.c)
 │                                              ├─ adc1_sample_step()    one conversion, accumulate   (NEW: fw/drivers/adc1.c)
 │                                              ├─ reg_scale(ch0_avg) → adc_scaled_value              (NEW: pure fn in app_reg.c)
 │                                              ├─ reg_output_level(adc_scaled_value) → pattern        (NEW: pure fn in app_reg.c)
 │                                              └─ (DEFERRED) reg_drive_osc(pattern)  — NOT this slice
 └─ app_lcd_tick()  4 ms display        (existing; reads app_lcd_measure())

app_lcd_measure()  (fw/src/app_lcd.c)  ── stub replaced: returns live reg values  ◄── app_reg state
board_init()       (fw/src/board.c)    ── OSC mask fix + 3-channel idle-HIGH
periph.c           ── add  ADC_HandleTypeDef hadc1;
main.c             ── add  adc1_init();  (init order §11)
```

**Ownership (per CLAUDE.md conventions):**
- `fw/drivers/adc1.c` owns ADC1 peripheral init **and** the PB0/PB1 analog GPIO config (driver owns its pins, as `usart.c` owns PC6/PC7).
- `ADC_HandleTypeDef hadc1` defined once in `fw/src/periph.c`, `extern` in `periph.h` (HAL-handle single-owner rule).
- `fw/src/app_reg.c` (+ `app_reg.h`) owns the regulation state (`ch0_avg`, `ch1_avg`, `adc_scaled_value`, `pattern`, sample accumulators) and the two pure functions. New module, parallel to `app_lcd.c`.

---

## 3. ADC acquisition contract (`adc1.c` + `app_reg.c`)

Verified facts: ADC-1…ADC-9, §3a.

| Item | AVR (verified) | STM32F410 port |
|------|----------------|-----------------|
| Channels | ch0=ADC0(PA0), ch1=ADC1(PA1), alternating idx 0/1 (ADC-5) | ADC1_IN8 (PB0), ADC1_IN9 (PB1) per pinmap; alternate per sample |
| Mode | single-shot, SW re-arm each conversion, ADATE=0 (ADC-7) | DP1: superloop-polled; one conversion per `adc1_sample_step()` call |
| ch0 average | sum 10 → ÷10 → `ch0_avg` (ADC-2) | identical: 10-sample running mean |
| ch1 average | sum 50 → ÷50 → `ch1_avg` (ADC-3) | identical: 50-sample running mean (ch1 acquired, **not** fed to regulation — B4) |
| Accumulator | true 32-bit (ADC-4) | `uint32_t` accumulators (10×1023 and 50×1023 fit in 16b, 32b is safe headroom) |
| Reference | REFS=11 → internal **2.56 V** full-scale (ADC-6, datasheet decode) | F410 has **no 2.56 V internal ref** → uses VREF+ (board 3.3 V). **Domain mismatch — see §10.** |
| Resolution | 10-bit (0..1023) | 12-bit (0..4095) → **normalize to 10-bit-equiv (`>>2`) before §4** (see §10) |

**Acquisition design (DP1, polled):** each `app_reg_tick()` (≈2 ms) calls `adc1_sample_step()` once; it reads the just-finished conversion, normalizes to the 10-bit domain, accumulates into the current channel's sum, advances the sample counter, and starts the next conversion on the **alternate** channel. When a channel's counter reaches its target (10 / 50) it commits the mean to `ch0_avg`/`ch1_avg` and resets. The latest committed `ch0_avg` is the regulation input.

> **Sample-cadence note (verify item):** the AVR accumulated in a free-running ISR while a 2 ms timer consumed the latest mean. Polling one conversion per 2 ms tick makes a ch0 mean span ~10×2 ms×2 (alternation) ≈ 40 ms, ch1 ≈ 200 ms — slower than the AVR. Acceptable for slice 1 (compute-only, display). If the bench shows the regulation needs the AVR's faster averaging, switch `adc1_sample_step()` to drain multiple conversions per tick or move to interrupt/DMA. Flagged, non-blocking.

---

## 4. Scaling transfer — `reg_scale()` (pure function)

Verified facts: SCALE-01…SCALE-07, §3b. Exact port of `adc_calc_scaled_output @0x1A5C`:

```c
/* input = ch0_avg in the 10-bit-equiv count domain (§10). */
uint16_t reg_scale(uint16_t in) {
    if (in >= 1000u) return 1000u;   /* SCALE-04 input-domain ceiling (cpi 0xE8/0x03 = 0x03E8) */
    if (in < 3u)     return 0u;       /* SCALE-05 input-domain floor (sbiw 0x03; value 3 falls through) */
    return (uint16_t)(in * 6u);       /* SCALE-06 ×6 (range [18,5994], NOT clamped to 1000) */
}
```

- The clip/floor test the **input**, not the output (analysis §4.1) — output of the normal branch is unbounded by 1000.
- The AVR gate `timer0_process_cnt ≥ 11` (SCALE-01/02, B-GATE): on STM32 the equivalent is "run every regulation tick after warm-up." Default = fire every tick (B-GATE suggested default; increment site is image-undecidable but harmless here). No separate counter needed this slice.
- `reg_scale` does **not** touch the lookup table (SCALE-07); the table is §5 only.

**Unit-testable** (TDD target §12): pure `uint16_t→uint16_t`, test the three branches + boundaries (2,3,999,1000,1023).

---

## 5. Lookup thermometer — `reg_output_level()` (pure function, OSC drive DEFERRED)

Verified facts: C1, C2, C-LADDER, §3c + §4.3 table.

```c
/* lookup_table[24], flash byte 0x58, strictly DECREASING idx 0..20 (§4.3). */
static const uint16_t lookup_table[24] = {
    0x03FF,0x03CC,0x0399,0x0366,0x0333,0x0300,0x02CD,0x029A,
    0x0267,0x0234,0x0201,0x01CE,0x01B9,0x0168,0x0135,0x0102,
    0x00CF,0x009C,0x0069,0x0036,0x000F,0x0004,0x0004,0x0054
};
/* Only the first 21 entries (idx 0..20) are used; idx 21..23 present but never compared (C1). */

uint8_t reg_output_level(uint16_t scaled) {
    for (uint8_t i = 0u; i < 21u; i++)
        if (lookup_table[i] < scaled)   /* C2: strictly-less, FIRST match */
            return i;                    /* band index 0..20 */
    return 21u;                          /* no match → output off (0x15B2 fall-through) */
}
```

The band index `i` maps to the progressive thermometer pattern per **C-LADDER** (i=0–4 full; i=5–12 drain digit; i=13–20 drain pattern; 21 → off). For **slice 1** the module computes the pattern bytes and **holds them in regulation state** (`reg.pattern`), so the eventual output stage and the measure provider can read them — but **no GPIO is driven** (B-SEAM deferred, §9).

> Implementation choice: port the full i→pattern map (it is a verified register-fact and the regulation's actual output), even though only `adc_scaled_value`/band-index feed the slice-1 display. This keeps the output stage a pure add later.

**Unit-testable** (§12): table values, compare direction (e.g. scaled just above/below a boundary picks the right `i`), the no-match path.

---

## 6. Cadence integration (`app.c`)

Verified facts: cad-C1, cad-C4, §3d. Timer0 reload 0xF0 → 16 ct → ~2.05 ms; ISR calls scale then lookup **unconditionally every tick**.

Add to `app_loop_iter()` the same delta-on-`sys_tick_get_ms()` pattern the existing 4 ms display tick uses (`app_lcd.c:170`):

```c
/* fw/src/app.c — app_loop_iter(), after dgus_rx drain, alongside app_lcd_tick() */
app_reg_tick();   /* ~2 ms regulation step (NEW) */
app_lcd_tick();   /* 4 ms display step (existing) */
```

`app_reg_tick()` (in `app_reg.c`) gates internally on a 2 ms `sys_tick_get_ms()` delta, then: `adc1_sample_step()` → `reg_scale(ch0_avg)` → `reg_output_level()`. STM32 at 96 MHz reproduces the ~2 ms period directly in ms (B-CADENCE: AVR F_CPU=8 MHz assumed; absolute period non-blocking).

---

## 7. Measure provider fill (`app_lcd.c`)

Replace the all-zero `app_lcd_measure()` stub (`app_lcd.c:16`) with live regulation values read from `app_reg.c`. Consumption is in `app_lcd_disp.c` (output bar uses `curr_amp` vs `ref_lv_*`; VAR_* via `disp_send_val`).

| `lcd_measure_t` field | Slice-1 source | Note |
|-----------------------|----------------|------|
| `curr_amp` | `ch0_avg` (10-bit-equiv) | drives the output bar; ref_lv_* thresholds interact (calibration §10) |
| `curr_power`,`max_power`,`last_power` | `adc_scaled_value` (all three = same value this slice) | opaque scaled level (B3); no cycle max/last tracking yet (slice 2 FSM) |
| `curr_freq`,`last_freq` | **0** | freq physical binding = B4; not bound this slice |
| `curr_energy`,`last_energy` | **0** | cycle accumulation = slice 2 |
| `us_on_time_200m` | **0** | run-timer = slice 2 |
| `us_run_status` | **0 (US_IDLE)** | command FSM = slice 2 |
| `sig_run/seek/reset_status` | **0** | the 3 mirror flags = output stage (§9) |

Result: amp + scaled-level are live (bars move with the sensed signal); cycle/freq/energy/status stay 0 (honest "regulating, no cycle yet"). The provider returns a pointer into `app_reg.c`'s state (single owner), mirroring `app_lcd_state()`/`app_lcd_cfg()`.

---

## 8. `board.c` fixes (safe subset only)

Verified facts: §6 board.c bugs (split per advisor — polarity proven only for the 3 mirrored channels).

`fw/src/board.c` currently inits the OSC pins LOW with a wrong mask. Fix in two safe parts; **do not idle the full mask HIGH** (that would silently make PB12/PB13 push-pull outputs — the exact OSC2/OSC3 direction B-OSC-MAP defers):

1. **Mask correction:** `CTRL_OSC_PINS` is `PIN_10|12|13|14|15` — **PB2 missing, PB15 spurious**. Per `pinmap.md:119-123` the channels are PB2|PB10|PB12|PB13|PB14.
2. **Confirmed-output init (now):** configure **PB2 / PB10 / PB14** (OSC0/OSC1/OSC4 ← mega16 PB1/PB0/PC7) as push-pull, idle **HIGH** (`GPIO_PIN_SET` = OSC off; active-LOW per C6). Map: mega16 PC7→PB14, PB1→PB2, PB0→PB10.
3. **Deferred (NOT initialized as outputs):** **PB12 / PB13** (OSC2/OSC3 ← mega16 PC4/PA7, DDR-inputs in the image, C7). Leave out of `board_init` output config until B-OSC-MAP is resolved by measurement. (Remove the heartbeat-era `CTRL_OSC0..4 LOW` write entirely.)

---

## 9. DEFERRED — output stage (pending bench measurement)

This is the **one slice-1 blocker** the user chose to resolve by measurement (B-SEAM). The compute pipeline (§3–§7) is complete and testable without it.

**What is known (in-image, §3c/§3e):** the pattern is computed and (in the AVR) driven to PORTD via a 3-phase multiplex through a non-inverting bit-map — but that 7-seg path is **board-dead on V26**. Separately the Timer0 ISR mirrors **3 binary flags** (`0x0189/018A/0196`) **active-LOW** to PC7/PB1/PB0 → STM32 PB14/PB2/PB10.

**What is NOT in the image (decide by measurement):**
1. Whether the real OSC drive is (a) the 8-level thermometer pattern, (b) the 3 binary mirror flags, or (c) the analog `set_pot` level — and onto which connector pins.
2. OSC2/OSC3 (PB12/PB13 ← PC4/PA7) direction: output drive vs OSC-board feedback input.
3. Active level + bit-order of the 5 OSC channels.

**Bench measurement procedure (user, on the original V26 board while regulating):**
- Scope / logic-analyzer the OSC connector (CN2 / U2.28/29/33/34/35) → capture the live drive: pattern vs flags vs analog, active level, per-channel bit mapping.
- Probe PC4/PA7 → confirm output-drive vs input-feedback (decides PB12/PB13 direction).
- Sweep ADC input vs OSC output → binds `ch0_avg`/`adc_scaled_value` to physical units (B3, §10).
- Live ADMUX/ADC read → PA1/ADC1 identity (B4).

After measurement → add `reg_drive_osc(pattern)` + finalize `board.c` PB12/PB13 + `app_lcd_hook_set_pot` wiring as a follow-up slice (or this slice's part 2).

---

## 10. ⚠ Domain / calibration hazard (DP2 — resolved by measurement)

The regulation math (`lookup_table` thresholds 0x000F..0x03FF, the 1000/3 clips, ×6) lives in the **AVR ADC count domain: 10-bit @ 2.56 V full-scale**. The STM32 differs on **both** axes:

| | AVR (regulation math assumes) | STM32F410 (V30 board) |
|---|---|---|
| Resolution | 10-bit (0..1023) | 12-bit (0..4095) |
| Reference | internal 2.56 V | VREF+ (board, ~3.3 V) |
| Analog front-end | V26 board | V30 board (may differ) |

**Slice-1 handling (DP2 = raw + calibrate later):**
- Normalize the 12-bit reading to the 10-bit-equiv domain by `>>2` **before** `reg_scale` (first-cut; keeps `lookup_table` thresholds meaningful).
- Port the math **verbatim** in that normalized domain.
- Because the reference (2.56 V vs 3.3 V) and the V26→V30 front-end differ, the same physical sense voltage maps to a **different count** than on the AVR → the regulation curve and the `ref_lv_*` display thresholds are **not** calibrated until the §9 ADC-vs-output sweep. **This is harmless in slice 1** (output is display-only, OSC drive deferred) and is exactly what the measure-first sweep corrects.
- Keep the normalization factor and any threshold rescale in **named constants** isolated in `app_reg.c` so calibration is a one-line change after measurement.

---

## 11. File / build changes

| File | Change |
|------|--------|
| `fw/drivers/adc1.c` (NEW) | `adc1_init()` (ADC1 + PB0/PB1 analog + clocks), `adc1_sample_step()` (one polled conversion, alternate channel, normalize `>>2`) |
| `fw/include/adc1.h` (NEW) | driver API |
| `fw/src/app_reg.c` (NEW) | regulation state owner + `app_reg_tick()` + `reg_scale()` + `reg_output_level()` + measure-getter |
| `fw/include/app_reg.h` (NEW) | `app_reg_tick()`, pure-fn protos, `app_reg_measure_*` accessors, `app_reg_state()` |
| `fw/src/periph.c` / `periph.h` | add `ADC_HandleTypeDef hadc1;` (+ extern) |
| `fw/src/app_lcd.c` | `app_lcd_measure()` body → read `app_reg` live values (§7) |
| `fw/src/app.c` | `app_loop_iter()` → add `app_reg_tick();` |
| `fw/src/board.c` | OSC mask fix + 3-channel idle-HIGH (§8) |
| `fw/src/main.c` | add `adc1_init();` — order: after `tim11_init()`, before `board_init()` |
| `fw/CMakeLists.txt` | add `stm32f4xx_hal_adc.c` to `HAL_SOURCES` (vendor list is explicit, not globbed). `adc1.c`/`app_reg.c` auto-picked by `file(GLOB src/*.c drivers/*.c)`. |

Build gate unchanged: `env -u STM32_TOOLCHAIN cmake --build build` → **0 warnings** (`-Wall -Wextra -Wundef -Wshadow`).

---

## 12. Test / verify plan

**Unit tests (TDD — host build of the pure functions, no HW):**
- `reg_scale`: branches + boundaries {0,2,3,4,999,1000,1023} → {0,0,18,24,5994,1000,1000}.
- `reg_output_level`: each band boundary picks the right `i`; no-match → 21; table values byte-match §4.3.

**On-target HW verify (BOOT0 resolved — plain `reset run`):**
- Build 0-warning; flash; USART6 mon shows `[reg]` trace (ch0_avg / scaled / band) on a slow cadence.
- Inject a known analog level on PB0 → confirm `ch0_avg` tracks it, `adc_scaled_value` follows the ×6/clip, band index steps through the table.
- LCD: output bar + VAR_POWER/VAR_AMP move with the injected signal (provider live).
- Confirm **no OSC GPIO activity** (output deferred) and OSC pins idle HIGH (the 3 confirmed).

**Trace build:** reuse the `-DLCD_TRACE_RX`-style gated `mon_printf` pattern for a `-DREG_TRACE` regulation trace.

---

## 13. Open questions for user review (bucket B — flagged, mostly non-blocking)

| ID | Question | Blocks slice 1? | Slice-1 default |
|----|----------|-----------------|------------------|
| B-SEAM | OSC physical drive path | **YES (output only)** | **deferred — measure first** (§9); compute pipeline proceeds |
| B-OSC-MAP | PB12/PB13 (OSC2/3) direction | partial | not initialized as outputs (§8) |
| B-UNITS / DP2 | 2.56 V/10-bit vs 3.3 V/12-bit domain | NO | `>>2` normalize + verbatim math; calibrate by measurement (§10) |
| B3 | `adc_scaled_value` physical meaning | NO | opaque scaled setpoint (§4/§7) |
| B4 | ch1 / PA1 identity | NO | acquire + average; not fed to regulation; freq fields 0 (§3/§7) |
| B-GATE | `timer0_process_cnt` increment site | NO | fire scale every tick after warm-up (§4) |
| B-CADENCE | original F_CPU | NO | assume 8 MHz → ~2 ms tick reproduced in ms (§6) |

---

## 14. References

- Verified analysis (authoritative): `docs/superpowers/analysis/2026-05-31-m16-regulation-core-verified.md`
- Raw disasm (ground truth): `ref/atmega16/M16_reverse/out/01_disassembly.asm`
- v001 decompile (weak ref): `ref/atmega16/m16_conv_v001.c`
- Pin map: `docs/pinmap.md` (ADC §, GPIO Output §, OSC table 119-125)
- Integration contract: `fw/include/app_lcd.h`, `fw/src/app_lcd.c`, `fw/src/app_lcd_disp.c`, `fw/src/app.c`, `fw/src/board.c`, `fw/src/periph.c`
- Prior board-truth analysis: `docs/superpowers/analysis/atmega16-io-behavior.md` §0.1
- Memory: `project_m16_regulation_verified`, `project_atmega16_absorption`
