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
#define REG_RAMP_MS      10u   /* M16 Timer1 0xFFB1 ~10.1 ms ramp cadence (cad-C8) */
#define RAMP_DONE_COUNT  401u  /* counter >= 401 (0x191) -> state 0 (verified §2.1) */

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

    uint8_t  main_state;              /* 1 = soft-start ramp, 0 = lookup regulation */
    uint16_t ramp_counter;           /* 0..401, advances ~every 10 ms while running */
    uint32_t prev_ramp_ms;           /* 10 ms ramp cadence gate */

    uint8_t  us_run_status;          /* slice 2b: US_IDLE/REMOTE/TOUCH/COMM (FSM owns) */
    uint16_t max_power;              /* running peak of sel during the active run */
    uint16_t last_power;             /* peak latched on stop (us_off, samd20 4180) */

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
    g_reg.prev_ms      = sys_tick_get_ms();
    g_reg.prev_ramp_ms = g_reg.prev_ms;
    /* slice 2b: boot IDLE (us_run_status=0, main_state=0 via memset). The output
     * runs only on a RUN command (app_reg_command); slice-2a auto-run retired. */
}

void app_reg_command(us_cmd_t cmd)
{
    switch (cmd) {
    case US_CMD_START:
        /* Touch RUN press: re-arm the soft-start ramp only on a real idle->run edge.
         * M16's ramp is edge-driven on M_START OFF->ON; a repeated/auto-repeat press
         * while already running must NOT restart it (else ramp_counter resets and
         * never reaches 401 -> no handoff to lookup -> output stuck at ramp start).
         * samd20 gated on != US_REMOTE because its ramp was already edge-driven
         * downstream; for a TOUCH-only slice == US_IDLE is strictly safer (can't
         * restart an active run). REMOTE/COMM source arbitration + re-press energy
         * reset = later slices. */
        if (g_reg.us_run_status == (uint8_t)US_IDLE) {
            g_reg.us_run_status = (uint8_t)US_TOUCH;
            g_reg.main_state    = 1u;
            g_reg.ramp_counter  = 0u;
            g_reg.max_power     = 0u;
        }
        break;
    case US_CMD_RUN_RELEASE:
        /* Touch RUN release: only a touch-started run stops here (samd20
         * main.c:3699 / us_off main.c:4180). Latch last_power = running peak. */
        if (g_reg.us_run_status == (uint8_t)US_TOUCH) {
            g_reg.last_power    = g_reg.max_power;
            g_reg.us_run_status = (uint8_t)US_IDLE;
        }
        break;
    case US_CMD_SEEK:
    case US_CMD_RESET:
    default:
        /* Regulation effect deferred (spec §9): the input layer already does the
         * RESET icon/page restore; SEEK/RESET drive is B-SEAM. */
        break;
    }
#ifdef REG_TRACE
    mon_printf("[reg] cmd=%u run=%u\r\n", (unsigned)cmd, (unsigned)g_reg.us_run_status);
#endif
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
    /* slice 2b run-gated: curr_power = live setpoint (0 when idle); max_power =
     * running peak during the run; last_power latched on stop (app_reg_command).
     * cycle/freq/energy stay 0 (weld-cycle deferred). */
    uint8_t active = (uint8_t)(g_reg.us_run_status != (uint8_t)US_IDLE);
    g_measure.curr_amp   = g_reg.ch0_avg;
    g_measure.curr_power = active ? g_reg.adc_scaled_value : 0u;
    if (active && (g_reg.adc_scaled_value > g_reg.max_power)) {
        g_reg.max_power = g_reg.adc_scaled_value;
    }
    g_measure.max_power     = g_reg.max_power;
    g_measure.last_power    = g_reg.last_power;
    g_measure.us_run_status = g_reg.us_run_status;
}

void app_reg_tick(void)
{
    uint32_t now = sys_tick_get_ms();

    /* ~10 ms soft-start ramp cadence (M16 Timer1 0xFFB1 equiv, cad-C8). The
     * counter advances only while state==1; one-way handoff to the lookup
     * regulation at RAMP_DONE_COUNT (M16 @0x137C). */
    if ((uint32_t)(now - g_reg.prev_ramp_ms) >= REG_RAMP_MS) {
        g_reg.prev_ramp_ms = now;
        if ((g_reg.us_run_status != (uint8_t)US_IDLE) && (g_reg.main_state == 1u)) {
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

    /* Output setpoint MUX (spec §3.1): idle (no run command) forces 0; while
     * running, soft-start ramp during state==1, else the slice-1 scale of the
     * latest ch0_avg. The running state==0 path == slice 1 verbatim. */
    uint16_t sel;
    if (g_reg.us_run_status == (uint8_t)US_IDLE) {
        sel = 0u;
    } else if (g_reg.main_state == 1u) {
        sel = reg_ramp_level(g_reg.ramp_counter);
    } else {
        sel = reg_scale(g_reg.ch0_avg);
    }
    g_reg.adc_scaled_value = sel;
    g_reg.band             = reg_output_level(sel);

    reg_publish_measure();

#ifdef REG_TRACE
    if ((uint32_t)(now - g_reg.trace_ms) >= REG_TRACE_MS) {
        g_reg.trace_ms = now;
        mon_printf("[reg] run=%u st=%u rc=%u ch0=%u ch1=%u sel=%u band=%u\r\n",
                   (unsigned)g_reg.us_run_status, (unsigned)g_reg.main_state,
                   (unsigned)g_reg.ramp_counter, (unsigned)g_reg.ch0_avg,
                   (unsigned)g_reg.ch1_avg, (unsigned)g_reg.adc_scaled_value,
                   (unsigned)g_reg.band);
    }
#endif
}
