/* fw/src/app_reg.c — Stage D regulation core (compute + run gate).
 * Pipeline (verified analysis §3): 2-ch polled ADC -> normalize 12->10b
 * -> ch0 mean-of-10 / ch1 mean-of-50 -> reg_scale(ch0_avg) -> reg_output_level.
 * Run semantics (M16-faithful, 2026-06-10 IPC analysis): one boot warm-up
 * (~4 s, commands ignored, output 0), then RUN = immediate level-follow gate
 * with NO per-START soft-start. Publishes lcd_measure_t; drives NO OSC GPIO
 * (deferred, spec §9 / B-SEAM). */
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
#define REG_ACQ_MS       1u    /* ADC pace: M16 ISR self-re-armed ~417 us/ch (ch0_avg
                                * ~8.3 ms). Both channels per 1 ms -> ch0_avg 10 ms,
                                * ch1_avg 50 ms — closest the ms-grid superloop gets
                                * (the old 1-ch-per-2ms alternation was 40/400 ms).
                                * Budget: 84+12 cyc @ PCLK2/4 ~= 4 us/conversion, so
                                * 2 blocking reads ~= 8 us per 1 ms — negligible. */
#define CH0_SAMPLES      10u   /* ADC-2 */
#define CH1_SAMPLES      50u   /* ADC-3 */
#define ADC_NORM_SHIFT   2u    /* 12-bit -> 10-bit-equiv (spec §10 DP2 first-cut) */
#define REG_RAMP_MS      10u   /* M16 Timer1 0xFFB1 ~10.1 ms warm-up cadence (cad-C8) */
#define RAMP_DONE_COUNT  401u  /* counter >= 401 (0x191) -> state 0 (verified §2.1) */
#define ON_TIME_UNIT_MS  10u   /* limit_on_time unit: x10 ms (samd20 main.c:769) */

#ifdef REG_TRACE
#define REG_TRACE_MS     500u  /* slow trace cadence so the mon log stays readable */
#endif

typedef struct {
    uint16_t ch0_avg, ch1_avg;        /* committed means (10-bit-equiv domain) */
    uint16_t adc_scaled_value;        /* reg_scale(ch0_avg) */
    uint8_t  band;                    /* reg_output_level() 0..21 (held; output deferred) */

    uint32_t ch0_acc; uint16_t ch0_cnt;
    uint32_t ch1_acc; uint16_t ch1_cnt;

    uint8_t  main_state;              /* 1 = boot warm-up, 0 = lookup regulation */
    uint16_t ramp_counter;           /* boot warm-up 0..401, ~every 10 ms, one-shot */
    uint32_t prev_ramp_ms;           /* 10 ms warm-up cadence gate */

    uint8_t  us_run_status;          /* slice 2b: US_IDLE/REMOTE/TOUCH/COMM (FSM owns) */
    uint16_t max_power;              /* running peak of sel during the active run */
    uint16_t last_power;             /* peak latched on stop (us_off, samd20 4180) */
    uint16_t max_amp;                /* running peak of curr_amp during the run */
    uint16_t last_amp;               /* amp latched on stop (samd20 last_amp) */
    uint32_t run_start_ms;           /* TOUCH/COMM run start (on-time ceiling base) */
    uint8_t  swallow_start;          /* 1 = next START is the orphaned release of a
                                      * timeout-stopped run (V30 data=0 quirk) */

    uint32_t prev_acq_ms;
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
    g_reg.prev_acq_ms  = g_reg.prev_ms;
    g_reg.prev_ramp_ms = g_reg.prev_ms;
    /* Boot = IDLE (us_run_status=0 via memset) + one-shot warm-up. M16-faithful:
     * main sets g_main_state=1 exactly once (@0x1B8A); during warm-up the OSC
     * flags stay at their boot zeros and commands are ignored (Timer1 ISR skips
     * app_0x06d2 while state!=0). The per-START soft-start of slice 2a is
     * retired — the M16 never re-enters the ramp (one-way @0x137C). */
    g_reg.main_state = 1u;
}

void app_reg_command(us_cmd_t cmd, uint8_t src)
{
    switch (cmd) {
    case US_CMD_START:
        /* M16-faithful: commands are ignored during the boot warm-up (Timer1
         * ISR skips the PINA dispatcher app_0x06d2 while g_main_state!=0,
         * disasm @0x041E); after warm-up RUN is an immediate level-follow
         * gate (no per-START ramp). == US_IDLE strict guard for BOTH sources
         * (intentional deviation from samd20's comm !=US_REMOTE takeover,
         * approved spec §4 — REMOTE arbitration is a later slice). */
        if ((g_reg.main_state == 0u) &&
            (g_reg.us_run_status == (uint8_t)US_IDLE)) {
            if ((src == (uint8_t)US_TOUCH) && (g_reg.swallow_start != 0u)) {
                /* V30 RUN button sends data=0 on BOTH edges: after an on-time
                 * ceiling stop, the still-held button's release arrives mapped
                 * as START (input layer sees IDLE). Consume it once instead of
                 * restarting the run. Touch-only: a COMM START is a register
                 * write with no release back-mapping (spec §4). */
                g_reg.swallow_start = 0u;
                break;
            }
            g_reg.us_run_status = src;   /* US_TOUCH or US_COMM */
            g_reg.max_power     = 0u;
            g_reg.max_amp       = 0u;    /* samd20 comm START zeroes max_amp too */
            g_reg.run_start_ms  = sys_tick_get_ms();
            /* samd20 zeroes us_on_time_200m at the run-start edge (main.c:4306);
             * the live compute would reach 0 on the first active publish anyway,
             * but zeroing here closes the <=2ms window where a disp read could
             * pair the old time value with the new run status. */
            g_measure.us_on_time_200m = 0u;
        }
        break;
    case US_CMD_RUN_RELEASE:
        /* Source-matched stop only (samd20 main.c:3699/4180 touch, 4405 comm):
         * a COMM STOP cannot kill a touch run and vice versa. Latch the
         * power/amp peaks for the stopped-display mirror. */
        if (g_reg.us_run_status == src) {
            g_reg.last_power    = g_reg.max_power;
            g_reg.last_amp      = g_reg.max_amp;
            g_reg.us_run_status = (uint8_t)US_IDLE;
        } else if ((src == (uint8_t)US_TOUCH) && (g_reg.swallow_start != 0u)) {
            /* Any touch RUN_RELEASE arriving while IDLE after a ceiling stop
             * resyncs the press/release pairing so the next genuine press is
             * not eaten: legacy data=4 release, or SYS_PIC_NOW re-init (panel
             * reset means the physical release will never arrive). */
            g_reg.swallow_start = 0u;
        }
        break;
    case US_CMD_SEEK:
    case US_CMD_RESET:
    default:
        /* Regulation effect deferred (spec §9): the input layer already does
         * the RESET icon/page restore; SEEK/RESET drive is B-SEAM. NB samd20
         * comm RESET marks src US_TOUCH (main.c:4353 quirk) and clears the
         * error display — both inert until the error machine slice. */
        break;
    }
#ifdef REG_TRACE
    mon_printf("[reg] cmd=%u src=%u run=%u\r\n", (unsigned)cmd, (unsigned)src,
               (unsigned)g_reg.us_run_status);
#endif
}

const lcd_measure_t *app_reg_measure(void)
{
    return &g_measure;
}

/* One ADC conversion on BOTH channels, normalize, accumulate, and commit the
 * mean when the per-channel sample count is reached. Paced at REG_ACQ_MS. */
static void reg_acquire_step(void)
{
    uint16_t s0 = (uint16_t)(adc1_read(ADC1_CH_SENS_OUT) >> ADC_NORM_SHIFT);
    g_reg.ch0_acc += s0;
    if (++g_reg.ch0_cnt >= CH0_SAMPLES) {
        g_reg.ch0_avg = (uint16_t)(g_reg.ch0_acc / CH0_SAMPLES);
        g_reg.ch0_acc = 0u;
        g_reg.ch0_cnt = 0u;
    }

    uint16_t s1 = (uint16_t)(adc1_read(ADC1_CH_OSC) >> ADC_NORM_SHIFT);
    g_reg.ch1_acc += s1;
    if (++g_reg.ch1_cnt >= CH1_SAMPLES) {
        g_reg.ch1_avg = (uint16_t)(g_reg.ch1_acc / CH1_SAMPLES);
        g_reg.ch1_acc = 0u;
        g_reg.ch1_cnt = 0u;
    }
}

static void reg_publish_measure(uint32_t now)
{
    /* slice 2b run-gated: curr_power = live setpoint (0 when idle); max_power =
     * running peak during the run; last_power latched on stop (app_reg_command).
     * cycle/freq/energy stay 0 (weld-cycle deferred). */
    uint8_t active = (uint8_t)(g_reg.us_run_status != (uint8_t)US_IDLE);
    g_measure.curr_amp   = g_reg.ch0_avg;
    if (active && (g_reg.ch0_avg > g_reg.max_amp)) {
        g_reg.max_amp = g_reg.ch0_avg;   /* amp peak — same pattern as max_power */
    }
    g_measure.curr_power = active ? g_reg.adc_scaled_value : 0u;
    if (active && (g_reg.adc_scaled_value > g_reg.max_power)) {
        g_reg.max_power = g_reg.adc_scaled_value;
    }
    if (active) {
        /* LV_TIME bar: live on-time in 200 ms units from the run-start stamp
         * (samd20 main.c:5223 cadence counter equivalent). TOUCH and COMM runs
         * both stamp run_start_ms at their START edge (app_reg_command); a
         * future REMOTE slice must stamp its own start. When idle the field
         * keeps the last run's final value — samd20 shows the latched
         * last_time when stopped, and disp feeds this one field on both paths
         * (app_lcd_disp.c:183 note). */
        g_measure.us_on_time_200m =
            reg_on_time_200m((uint32_t)(now - g_reg.run_start_ms));
    }
    g_measure.max_power     = g_reg.max_power;
    g_measure.last_power    = g_reg.last_power;
    g_measure.max_amp  = g_reg.max_amp;
    g_measure.last_amp = g_reg.last_amp;
    g_measure.us_run_status = g_reg.us_run_status;
}

void app_reg_tick(uint16_t limit_on_time)
{
    uint32_t now = sys_tick_get_ms();

    /* ~10 ms boot warm-up cadence (M16 Timer1 0xFFB1 equiv, cad-C8). M16-faithful:
     * runs exactly once, from boot, unconditionally (ramp counter zeroed only at
     * app_0x1226 entry); one-way handoff to lookup regulation at RAMP_DONE_COUNT
     * (@0x137C). During warm-up the output stays 0 — on the M16 only the
     * physically unconnected 7-seg pattern vars animate (g_019F/A0/A1). */
    if ((uint32_t)(now - g_reg.prev_ramp_ms) >= REG_RAMP_MS) {
        g_reg.prev_ramp_ms = now;
        if (g_reg.main_state == 1u) {
            g_reg.ramp_counter++;
            if (g_reg.ramp_counter >= RAMP_DONE_COUNT) {
                g_reg.main_state = 0u;
            }
        }
    }

    /* Run on-time ceiling (limit_on_time x10 ms, 0 = off, panel-editable).
     * COMM runs: samd20-faithful (main.c:5296 applies it to COMM/REMOTE in
     * SYS_HAND; 2026-06-10 analysis doc authority — REMOTE ceiling lands with
     * the REMOTE slice, NOT covered here). TOUCH runs: intentional
     * STM32 safety addition — the V30 RUN button's data=0 quirk can lose the
     * release edge (infinite run); the M16 itself also force-cleared the run
     * flag on an internal countdown (g_018F, Timer1 ISR @0x0572).
     * samd20's multi_ctrl/energy_ctrl run branches (main.c:5234..) belong to
     * the weld-cycle machine — deferred, spec §8. */
    {
        /* US_CYCLE(weld-cycle WELD)은 의도적으로 ceiling 대상 아님: WELD 길이는
         * weld-cycle FSM의 limit_delay_time2가 지배(app_weld). 아래 조건이
         * TOUCH/COMM만 포함하므로 US_CYCLE은 자연히 제외됨 (spec §5.2). */
        uint8_t rs = g_reg.us_run_status;
        if ((rs == (uint8_t)US_TOUCH) || (rs == (uint8_t)US_COMM)) {
            /* limit_on_time is injected by the caller from the live config
             * each call (M1: no call back into app_lcd), so a panel edit of
             * LV_MAX_ON_TIME still takes effect immediately, even mid-run. */
            if ((limit_on_time != 0u) &&
                ((uint32_t)(now - g_reg.run_start_ms) >=
                 (uint32_t)limit_on_time * ON_TIME_UNIT_MS)) {
                g_reg.last_power    = g_reg.max_power;   /* same latch as release */
                g_reg.last_amp      = g_reg.max_amp;
                g_reg.us_run_status = (uint8_t)US_IDLE;
                if (rs == (uint8_t)US_TOUCH) {
                    /* held button's release still pending — touch-only quirk */
                    g_reg.swallow_start = 1u;
                }
#ifdef REG_TRACE
                mon_printf("[reg] on-time ceiling (%u x10ms) -> stop\r\n",
                           (unsigned)limit_on_time);
#endif
            }
        }
    }

    if ((uint32_t)(now - g_reg.prev_acq_ms) >= REG_ACQ_MS) {
        g_reg.prev_acq_ms = now;
        reg_acquire_step();
    }

    if ((uint32_t)(now - g_reg.prev_ms) < REG_TICK_MS) {
        return;
    }
    g_reg.prev_ms = now;

    /* Output setpoint MUX (M16-faithful): boot warm-up and idle force 0; while
     * running the slice-1 scale of the latest ch0_avg applies immediately
     * (no per-START ramp — see app_reg_command). */
    uint16_t sel;
    if ((g_reg.main_state == 1u) ||
        (g_reg.us_run_status == (uint8_t)US_IDLE)) {
        sel = 0u;
    } else {
        sel = reg_scale(g_reg.ch0_avg);
    }
    g_reg.adc_scaled_value = sel;
    g_reg.band             = reg_output_level(sel);

    /* Publish runs only on the ~2 ms gate above: a ceiling/release stop that
     * fires earlier in this same call reaches g_measure up to ~2 ms later —
     * bounded and invisible (disp renders each VP-group at a ~40 ms cadence). */
    reg_publish_measure(now);

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
