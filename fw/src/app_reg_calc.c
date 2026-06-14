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

uint16_t reg_ramp_level(uint16_t counter)
{
    /* M16 app_0x1226 rung thresholds (recon :249-258); per-rung level =
     * thermometer fill (g_019F popcount = rung+1) * 128, saturating at 1024
     * (full byte 0xFF) from rung 7. 2026-06-10: no longer used in the output
     * path — disasm proved the ramp is a one-shot BOOT animation on the
     * physically unconnected 7-seg (OSC flags stay 0 during ramp), not an
     * output soft-start. Kept as the verified table reference (host-tested). */
    static const uint16_t thr[10] = {41u,81u,121u,161u,201u,241u,281u,321u,361u,401u};
    static const uint16_t lvl[10] = {128u,256u,384u,512u,640u,768u,896u,1024u,1024u,1024u};
    for (uint8_t i = 0u; i < 10u; i++) {
        if (counter < thr[i]) {
            return lvl[i];
        }
    }
    return 1024u;   /* counter >= 401: full; caller transitions state to 0 */
}

uint8_t reg_on_time_200m(uint32_t run_elapsed_ms)
{
    uint32_t units = run_elapsed_ms / 200u;
    return (units > 200u) ? 200u : (uint8_t)units;
}

/* spec §4.2: 500ms 에너지-단위 윈도우 / REG_TICK_MS(2ms) = 250 샘플. */
#define REG_ENERGY_DIV  250u

uint32_t reg_energy_from_acc(uint32_t acc_energy)
{
    return acc_energy / REG_ENERGY_DIV;
}
