/* fw/src/app_lcd_disp.c — periodic display step-machine + event-driven icons/error.
 *
 * Port of the samd20 main-loop job_state display block (ref/samd20/main.c:5108-5202)
 * + send_outpower_data (2605-2679) + send_outtime_data (2681-2706) + send_val_data
 * (4163-4178). Reproduces the samd20 bar-fill math VERBATIM; the chunked 5-element
 * dgus_write_u16_array writes are preserved (samd20 split them across job_state steps
 * to avoid saturating the LCD UART — kept one VP-group per call here).
 *
 * State split (vs samd20 globals):
 *   curr_amp / freq / power / energy / on-time / run-status -> app_lcd_measure()
 *   ref_lv_* output-bar thresholds                          -> app_lcd_state()
 *   output_power set-point                                  -> app_lcd_cfg()
 *   level_buf[20] / out_time_buf[20]                         -> file-static buffers
 *
 * Measurement is the all-zero stub provider this stage (Stage D fills it). With
 * curr_amp == 0 every bar slot clears, VAR_* == 0 — the approved "alive but idle"
 * UI. (Note the output-bar set-point MARKER still fires even at curr_amp==0: see
 * the comment at the marker line — that is samd20 fidelity, not a bug.)
 *
 * Wire-call mapping (spec §2.1): send_lcd_int_array -> dgus_write_u16_array;
 *   send_lcd_data_var -> dgus_write_u16; send_lcd_data_word -> dgus_write_u32;
 *   send_lcd_txt -> dgus_write_text; set_lcd_page -> dgus_set_page.
 *
 * No callers yet — Task 9 wires app_lcd_disp_step() into app_lcd_tick() at a 4 ms
 * cadence; the icon/work_cnt/error helpers are event-driven (input + Stage D).
 */
#include <stdint.h>
#include <stdbool.h>
#include "app_lcd.h"
#include "app_config.h"
#include "dgus_lcd.h"

/* US-on gate. The display picks curr/max while the ultrasonic output is on, else
 * the last-cycle hold values.
 *
 * samd20 gates send_val_data on a dedicated boolean `us_on_status == ON` (ON==1).
 * lcd_measure_t has no separate us_on_status — it collapses onto `us_run_status`,
 * documented as an FSM state (US_IDLE/REMOTE/TOUCH/COMM). The faithful gate is
 * therefore "not idle" (!= US_IDLE == 0), NOT "== 1": any active run state counts
 * as on. With the all-zero stub both readings are identical (idle/last path, all
 * zeros). Stage D, when it fills us_run_status with real FSM states, gets the
 * intended curr/max display for every non-idle state. */
/* Bar buffers — file-static, computed once on the compute step then sent in chunks
 * (samd20 globals level_buf[20] / out_time_buf[20]). uint16_t so the 5-element
 * dgus_write_u16_array carries one word per LCD bar slot. */
static uint16_t level_buf[20];
static uint16_t time_buf[20];

/*--------------------------------------------------------------
 * Output-power bar fill (port of send_outpower_data step==0, main.c:2616-2661).
 *
 * Thresholds (state->ref_lv_1/10/20) seed from model_freq in app_lcd_init_mode.
 * Set-point marker lands at slot (output_power * 20 / 100), clamped to 19.
 *
 *   curr_amp <= 10            -> all 20 slots 0
 *   curr_amp >  10            -> slot[0]=1
 *     curr_amp <= ref_lv_1    -> slots[1..19]=0
 *     curr_amp >  ref_lv_1
 *       curr_amp <= ref_lv_10 -> slots[2..2+t-1]=1 where t=(amp-ref_lv_1)*8/(ref_lv_10-ref_lv_1)
 *       curr_amp >  ref_lv_10
 *         curr_amp >= ref_lv_20 -> slots[1..19]=1 (full)
 *         else                  -> slots[2..9]=1 and slots[10..10+t-1]=1
 *                                  where t=(amp-ref_lv_10)*10/(ref_lv_20-ref_lv_10)
 * Then unconditionally: marker slot = 1.
 *--------------------------------------------------------------*/
static void disp_compute_output(uint16_t curr_amp, uint8_t out_power,
                                const lcd_app_state_t *st)
{
    uint8_t  i;
    uint8_t  marker;
    uint16_t span;          /* threshold gap, guarded != 0 before dividing */

    if (curr_amp > 10u) {
        level_buf[0] = 1u;
        if (curr_amp > st->ref_lv_1) {
            level_buf[1] = 1u;
            if (curr_amp > st->ref_lv_10) {
                if (curr_amp >= st->ref_lv_20) {
                    for (i = 1u; i < 20u; i++) {
                        level_buf[i] = 1u;
                    }
                } else {
                    uint8_t fill;
                    for (i = 2u; i < 10u; i++) {
                        level_buf[i] = 1u;
                    }
                    /* t = (amp-ref_lv_10)*10 / (ref_lv_20-ref_lv_10) */
                    span = (uint16_t)(st->ref_lv_20 - st->ref_lv_10);
                    fill = (span != 0u)
                        ? (uint8_t)(((uint32_t)(curr_amp - st->ref_lv_10) * 10u) / span)
                        : 0u;
                    for (i = 0u; i < 10u; i++) {
                        level_buf[10u + i] = (i < fill) ? 1u : 0u;
                    }
                }
            } else {
                uint8_t fill;
                /* t = (amp-ref_lv_1)*8 / (ref_lv_10-ref_lv_1) */
                span = (uint16_t)(st->ref_lv_10 - st->ref_lv_1);
                fill = (span != 0u)
                    ? (uint8_t)(((uint32_t)(curr_amp - st->ref_lv_1) * 8u) / span)
                    : 0u;
                for (i = 0u; i < 18u; i++) {
                    level_buf[2u + i] = (i < fill) ? 1u : 0u;
                }
            }
        } else {
            for (i = 1u; i < 20u; i++) {
                level_buf[i] = 0u;
            }
        }
    } else {
        for (i = 0u; i < 20u; i++) {
            level_buf[i] = 0u;
        }
    }

    /* Set-point marker: slot (out_power*20/100), clamped to 19 (main.c:2658-2661).
     * samd20 fidelity: this fires unconditionally, so even at curr_amp==0 (all-clear
     * branch above) a single marker dot is set. Intentional — do NOT "fix". */
    marker = (uint8_t)(((uint16_t)out_power * 20u) / 100u);
    if (marker >= 20u) {
        marker = 19u;
    }
    level_buf[marker] = 1u;
}

/*--------------------------------------------------------------
 * Output-time bar fill (port of send_outtime_data step==5, main.c:2685-2689).
 * Slot i set when i < current_time (0..200 run-timer ticks → 20-slot bar).
 *--------------------------------------------------------------*/
static void disp_compute_time(uint8_t current_time)
{
    uint8_t i;
    for (i = 0u; i < 20u; i++) {
        time_buf[i] = (i < current_time) ? 1u : 0u;
    }
}

/*--------------------------------------------------------------
 * VAR_POWER / VAR_AMP / VAR_FREQ / VAR_ENERGY (port of send_val_data, main.c:4163-4178).
 *
 * samd20 builds a 3-word array sent at VAR_POWER (0x1110), laying VAR_AMP (0x1111)
 * and VAR_FREQ (0x1112) into the same contiguous VP block, then a separate word at
 * VAR_ENERGY (0x1113):
 *   temp_buf[0] = power   = us_on ? max_power : last_power
 *   temp_buf[1] = current = us_on ? max_amp   : last_amp   (see note)
 *   temp_buf[2] = freq/100= (us_on ? curr_freq : last_freq) / 100
 *   VAR_ENERGY  = us_on ? curr_energy : last_energy
 *
 * NOTE: lcd_measure_t has no max_amp/last_amp split — only curr_amp. Both branches
 * use curr_amp for the VAR_AMP slot (stub → 0 either way; Stage D may add the split).
 *--------------------------------------------------------------*/
static void disp_send_val(const lcd_measure_t *m)
{
    bool     on = (m->us_run_status != US_IDLE);
    uint16_t vals[3];
    uint16_t freq;
    uint32_t energy;

    vals[0] = on ? m->max_power : m->last_power;        /* VAR_POWER */
    vals[1] = m->curr_amp;                              /* VAR_AMP — no max/last split */
    freq    = on ? m->curr_freq : m->last_freq;
    vals[2] = (uint16_t)(freq / 100u);                  /* VAR_FREQ = freq/100 */
    dgus_write_u16_array(VAR_POWER, vals, 3);

    energy = on ? m->curr_energy : m->last_energy;
    dgus_write_u16(VAR_ENERGY, (uint16_t)energy);       /* samd20 casts u32 → u16 */
}

/*--------------------------------------------------------------
 * Step machine — one VP-group per call, 0..9 then wrap (spec §11).
 * Faithful port of the samd20 job_state display dispatch (main.c:5110-5202):
 *
 *   step 0   : compute LV_OUTPUT bar (send_outpower_data step==0)
 *   step 1-4 : LV_OUTPUT[0..19] in 4×5 chunks
 *   step 5   : compute LV_TIME bar (send_outtime_data step==5)
 *   step 6   : LV_TIME[0..4]
 *   step 7   : send_val_data (VAR_*) AND LV_TIME[5..9]   <- both, like samd20 case 7
 *   step 8   : LV_TIME[10..14]
 *   step 9   : LV_TIME[15..19], wrap to 0
 *
 * samd20 gates the time bar on sig_run_status (curr us_on_time_200m vs last_time);
 * lcd_measure_t carries no last_time, so we feed us_on_time_200m on both paths (per
 * the task spec; stub → 0 → empty bar). DISP_REMOTE/modbus_status (samd20 case 9)
 * is skipped — Stage C.
 *--------------------------------------------------------------*/
void app_lcd_disp_step(void)
{
    static uint8_t s = 0u;       /* not named `step` to avoid -Wshadow vs samd20 param */

    const lcd_measure_t   *m  = app_lcd_measure();
    const lcd_app_state_t *st = app_lcd_state();
    const app_config_t    *cfg = app_lcd_cfg();

    switch (s) {
        case 0:     /* compute output bar */
            disp_compute_output(m->curr_amp, cfg->output_power, st);
            break;
        case 1:
            dgus_write_u16_array(LV_OUTPUT,      &level_buf[0],  5);
            break;
        case 2:
            dgus_write_u16_array(LV_OUTPUT + 5,  &level_buf[5],  5);
            break;
        case 3:
            dgus_write_u16_array(LV_OUTPUT + 10, &level_buf[10], 5);
            break;
        case 4:
            dgus_write_u16_array(LV_OUTPUT + 15, &level_buf[15], 5);
            break;
        case 5:     /* compute time bar */
            disp_compute_time(m->us_on_time_200m);
            break;
        case 6:
            dgus_write_u16_array(LV_TIME,        &time_buf[0],  5);
            break;
        case 7:     /* samd20 case 7 does BOTH val_data and the time[5..9] chunk */
            disp_send_val(m);
            dgus_write_u16_array(LV_TIME + 5,    &time_buf[5],  5);
            break;
        case 8:
            dgus_write_u16_array(LV_TIME + 10,   &time_buf[10], 5);
            break;
        case 9:
        default:
            dgus_write_u16_array(LV_TIME + 15,   &time_buf[15], 5);
            s = 0u;
            return;             /* wrapped — skip the post-switch increment */
    }
    s++;
}

/*==============================================================
 * Event-driven helpers (NOT in the step machine — called by input/Stage D).
 *==============================================================*/

/* Status icon set/clear (ICON_RESET/SEEK/RUN/OL/OUTERR). samd20 writes a u16 1/0
 * (send_lcd_data_var(ICON_*, 1|0)). */
void app_lcd_icon(uint16_t icon_vp, bool on)
{
    dgus_write_u16(icon_vp, on ? 1u : 0u);
}

/* Work-count display (LV_WORK_CNT, 32-bit). samd20 send_lcd_data_word. */
void app_lcd_set_work_cnt(uint32_t cnt)
{
    dgus_write_u32(LV_WORK_CNT, cnt);
}

/* Error page entry (port of the samd20 do_control fault block, main.c:4209-4234).
 * error_code is the error_status bitmask (define.h: ERR_OVLD=1, ERR_OVTIME=2,
 * ERR_OUTERR=4). Picks the message by bit priority, writes VP_ERROR_MSG, and
 * switches to LCD_WARNING. dgus_write_text is the 10-byte-zero-pad equivalent of
 * samd20 send_lcd_txt (so "OUTPUT ERROR" truncates to "OUTPUT ERR" exactly as on
 * samd20 — verbatim wire behavior). The OL/OUTERR icons are written via the
 * event-driven path / Stage D; here we only render the page + message.
 *
 * error_status is control-fed (stub provider §4.3 returns 0), so this path is
 * UNREACHABLE until Stage D feeds real faults — implemented now so the error UX is
 * whole the moment regulation supplies faults. The clear side lives in the input
 * layer (KEY_ERROR_RESET). */
#define ERR_BIT_OVLD    1u
#define ERR_BIT_OVTIME  2u
#define ERR_BIT_OUTERR  4u

void app_lcd_show_error(uint8_t error_code)
{
    const char *msg;

    if (error_code == 0u) {
        return;                 /* no fault — nothing to render */
    }

    /* samd20 message strings (main.c:156-159), same priority order. */
    if (error_code & ERR_BIT_OVLD) {
        msg = "OVER LOAD";
    } else if (error_code & ERR_BIT_OUTERR) {
        msg = "OUTPUT ERROR";   /* truncated to 10 bytes on the wire — samd20 fidelity */
    } else if (error_code & ERR_BIT_OVTIME) {
        msg = "OVER TIME";
    } else {
        msg = "E-STOP";         /* fallback for any other set bit */
    }

    dgus_write_text(VP_ERROR_MSG, msg);
    dgus_set_page(LCD_WARNING);
}
