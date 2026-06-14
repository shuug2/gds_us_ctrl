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
 *   in >= 1000 -> 1000;  in < 3 -> 0;  else -> in*6 (range [18,5994], unbounded).
 * `in` is ch0_avg already normalized to the 10-bit-equiv count domain. */
uint16_t reg_scale(uint16_t in);

/* Output-level selector — output_level_process @0x138C compare core (C2).
 * Returns the FIRST band index i in 0..20 where reg_lookup_table[i] < scaled
 * (strictly-less). No match -> 21 (output off / fall-through @0x15B2). */
uint8_t reg_output_level(uint16_t scaled);

/* Soft-start ramp (slice 2a): counter (0..401, ~10 ms/step) -> scaled-domain
 * output setpoint. 10-rung thermometer fill of M16 app_0x1226 (recon :249-258),
 * scaled as fill_bits*128, saturating at full (1024) from rung 7. The exact
 * pattern bytes (g_019F 2nd stage / PORTC bit) -> OSC mapping is deferred
 * (B-SEAM), same as the lookup C-LADDER. Monotone non-decreasing. */
uint16_t reg_ramp_level(uint16_t counter);

/* LV_TIME bar source: elapsed run ms -> 200 ms units, capped at 200 (= 40 s).
 * samd20 increments us_on_time_200m every 200 ms while running with a <200
 * guard (main.c:5223); computing live from the run-start stamp is equivalent
 * and needs no per-200ms cadence state. */
uint8_t reg_on_time_200m(uint32_t run_elapsed_ms);

/* 누산 전력 -> 표시 에너지. samd20 main.c:436 (curr_energy = acc_energy/500)
 * 구조 포팅. STM32는 curr_power를 REG_TICK_MS(=2ms) publish gate에서 누산하므로
 * divisor를 500ms/2ms=250으로 두어 samd20의 1ms·/500 energy-per-second를 재현
 * (spec §4.2). ⚠ REG_ENERGY_DIV는 app_reg.c의 REG_TICK_MS=2ms 누산 cadence와
 * 결합 — REG_TICK_MS 변경 시 divisor도 재산정. 절대 보정은 6b/HW. */
uint32_t reg_energy_from_acc(uint32_t acc_energy);
