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
    return 21u;                                /* no match -> output off (@0x15B2) */
}
