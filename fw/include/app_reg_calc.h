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
