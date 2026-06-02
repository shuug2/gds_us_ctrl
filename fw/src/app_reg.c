/* fw/src/app_reg.c — Stage D slice 1 regulation core (compute only).
 * Pipeline (verified analysis §3): alternate 2-ch polled ADC -> normalize 12->10b
 * -> ch0 mean-of-10 / ch1 mean-of-50 -> reg_scale(ch0_avg) -> reg_output_level.
 * Publishes lcd_measure_t; drives NO OSC GPIO (deferred, spec §9 / B-SEAM). */
#include <string.h>
#include "app_reg.h"
#include "app_reg_calc.h"
#include "adc1.h"
#include "sys_tick.h"
#ifdef REG_TRACE
#include "mon.h"
#endif

/* Cadence: Timer0 0xF0 -> 16 ct -> ~2.05 ms @8 MHz (cad-C1). Reproduced in ms. */
#define REG_TICK_MS      2u
#define CH0_SAMPLES      10u   /* ADC-2 */
#define CH1_SAMPLES      50u   /* ADC-3 */
#define ADC_NORM_SHIFT   2u    /* 12-bit -> 10-bit-equiv (spec §10 DP2 first-cut) */

#ifdef REG_TRACE
#define REG_TRACE_MS     500u  /* slow trace cadence so the mon log stays readable */
#endif

typedef struct {
    uint16_t ch0_avg, ch1_avg;        /* committed means (10-bit-equiv domain) */
    uint16_t adc_scaled_value;        /* reg_scale(ch0_avg) */
    uint8_t  band;                    /* reg_output_level() 0..21 (held; output deferred) */

    uint32_t ch0_acc; uint16_t ch0_cnt;
    uint32_t ch1_acc; uint16_t ch1_cnt;
    uint8_t  cur_ch;                  /* 0 = SENS_OUT, 1 = OSC (alternation) */

    uint32_t prev_ms;
#ifdef REG_TRACE
    uint32_t trace_ms;
#endif
} reg_state_t;

static reg_state_t   g_reg;
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
#endif
}
