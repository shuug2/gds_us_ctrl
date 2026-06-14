/* fw/src/app_weld_fsm.c — weld-cycle FSM core (slice 1, DELAY mode). HAL-free. */
#include "app_weld_fsm.h"
#include <string.h>

static uint8_t  s_run_status;
static uint8_t  s_f_status_start;   /* per-state "first entry done" flag */
static uint16_t s_temp_time;        /* active countdown (10ms/count) */
static uint16_t s_comp_time;        /* weld amplitude-comp counter (samd20) */
static uint8_t  s_sol_dn;           /* solenoid level held across steps */

void weld_fsm_init(void)
{
    s_run_status     = WELD_READY;
    s_f_status_start = 0u;
    s_temp_time      = 0u;
    s_comp_time      = WELD_COMP_FULL;
    s_sol_dn         = 0u;
}

uint8_t weld_fsm_status(void)
{
    return s_run_status;
}

/* slice-1 진폭: base = ((op-50)*255)/100 (op 50..100 -> 0..127); comp_time<7
 * (짧은 용접)이면 (7-comp_time)*10 감쇠. samd20은 uint8_t로 언더플로(저전력+
 * 짧은용접 -> 큰 진폭) — 혼합 입장상 0 클램프로 수정 (spec §8). */
static uint8_t weld_amplitude(uint8_t output_power, uint16_t comp_time)
{
    uint16_t amp = (uint16_t)(((uint16_t)(output_power - 50u) * 255u) / 100u);
    if (comp_time < WELD_COMP_FULL) {
        uint16_t reduction = (uint16_t)((WELD_COMP_FULL - comp_time) * 10u);
        amp = (amp >= reduction) ? (uint16_t)(amp - reduction) : 0u;
    }
    return (uint8_t)amp;
}

void weld_fsm_step(const weld_in_t *in, weld_out_t *out)
{
    memset(out, 0, sizeof(*out));

    /* 10 ms elapsed: active timer counts down (samd20 timer temp_time--). */
    if (s_temp_time > 0u) {
        s_temp_time--;
    }

    switch (s_run_status) {
    case WELD_READY:
        if (in->start) {
            s_run_status     = WELD_CYL1;
            s_temp_time      = in->limit_delay_time1;
            s_f_status_start = 0u;       /* CYL1 first-entry drives SOL_DN ON */
        }
        break;

    case WELD_CYL1:
        if (s_f_status_start == 0u) {
            s_f_status_start = 1u;
            s_sol_dn         = 1u;       /* SOL_DN ON (cylinder descends) */
        } else if (s_temp_time == 0u) {
            s_f_status_start = 0u;
            s_run_status     = WELD_WELD;
            if (in->limit_delay_time2 > 6u) {           /* samd20 main.c:1504 */
                s_temp_time = in->limit_delay_time2;
                s_comp_time = WELD_COMP_FULL;
            } else {
                s_comp_time = in->limit_delay_time2;
                s_temp_time = WELD_COMP_FULL;
            }
        }
        break;

    case WELD_WELD:
        if (s_f_status_start == 0u) {
            s_f_status_start = 1u;
            out->amplitude   = weld_amplitude(in->output_power, s_comp_time);
            out->weld_start  = 1u;       /* glue: US_CYCLE START + pot write */
        } else if (s_temp_time == 0u) {
            out->weld_stop   = 1u;       /* glue: US_CYCLE RUN_RELEASE */
            s_f_status_start = 0u;
            s_run_status     = WELD_HOLD;
            s_temp_time      = in->limit_delay_time3;
        }
        break;

    case WELD_HOLD:
        if (s_f_status_start == 0u) {
            s_f_status_start = 1u;
        } else if (s_temp_time == 0u) {
            s_f_status_start = 0u;
            s_run_status     = WELD_CYL2;
            s_temp_time      = in->limit_delay_time1;
        }
        break;

    case WELD_CYL2:
        if (s_f_status_start == 0u) {
            s_f_status_start = 1u;
            s_sol_dn         = 0u;       /* SOL_DN OFF (cylinder rises) */
        } else if (s_temp_time == 0u) {
            s_f_status_start = 0u;
            s_run_status     = WELD_READY;
            out->cycle_done  = 1u;       /* glue: work_cnt++ */
        }
        break;

    default:
        /* unreachable in normal operation (s_run_status is only ever a WELD_*
         * value); fail-safe on fault — drop the solenoid (cpp-review LOW-2). */
        s_run_status = WELD_READY;
        s_sol_dn     = 0u;
        break;
    }

    out->run_status = s_run_status;
    out->sol_dn     = s_sol_dn;
}
