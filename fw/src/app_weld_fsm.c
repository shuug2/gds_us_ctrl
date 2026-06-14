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

/* limit_out_time(초) -> FSM tick(10ms/tick) 수. samd20 backstop은
 * us_on_time(100ms 단위) >= limit_out_time*10, 즉 limit_out_time초. 10ms tick에선
 * limit_out_time*100 (limit_out_time<=10 -> 최대 1000 tick, uint16 OK). spec §3.2. */
#define WELD_TICKS_PER_SEC  100u
static uint16_t weld_backstop_ticks(uint16_t limit_out_time_sec)
{
    /* floor 0 -> 1s: a 0 backstop (corrupt FRAM / Modbus FC06 0 / LCD 0 — none
     * floor-clamp upstream) would abort every energy weld on the first WELD tick.
     * cpp-review M1. samd20 shares the hole but compares real time; the FSM's
     * discrete per-WELD-entry tick budget makes 0 instantaneous. Upper bound:
     * limit_out_time is cfg-clamped <=10 -> max 1000 tick (uint16 OK). */
    if (limit_out_time_sec == 0u) {
        limit_out_time_sec = 1u;
    }
    return (uint16_t)(limit_out_time_sec * WELD_TICKS_PER_SEC);
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
            /* comp_time은 항상 ldt2에서 유도 (진폭 보정; energy 모드에서도 적용 —
             * samd20 main.c:1546-1547은 energy_ctrl 무관). slice2 §3.2. */
            s_comp_time = (in->limit_delay_time2 > 6u) ? WELD_COMP_FULL
                                                       : in->limit_delay_time2;
            /* temp_time: energy 모드 -> backstop 카운트다운(limit_out_time초 ×100
             * tick), 시간 모드 -> 기존 ldt2/7 (samd20 main.c:1504). */
            if (in->energy_ctrl) {
                s_temp_time = weld_backstop_ticks(in->limit_out_time);
            } else {
                s_temp_time = (in->limit_delay_time2 > 6u) ? in->limit_delay_time2
                                                           : WELD_COMP_FULL;
            }
        }
        break;

    case WELD_WELD:
        if (s_f_status_start == 0u) {
            s_f_status_start = 1u;
            out->amplitude   = weld_amplitude(in->output_power, s_comp_time);
            out->weld_start  = 1u;       /* glue: US_CYCLE START + pot write */
        } else if (in->energy_ctrl) {
            /* energy 모드: 에너지 도달 -> 정상 종료(samd20 5272); 미도달 +
             * backstop 만료 -> abort(samd20 5288, 에러 표시는 이연). spec §3.3. */
            if (in->curr_energy >= in->limit_energy) {
                out->weld_stop   = 1u;
                s_f_status_start = 0u;
                s_run_status     = WELD_HOLD;
                s_temp_time      = in->limit_delay_time3;
            } else if (s_temp_time == 0u) {
                out->weld_stop   = 1u;   /* abort도 US 정지 */
                out->weld_fault  = 1u;   /* glue: fault hook (mon; 후속 SYS_ERROR) */
                s_sol_dn         = 0u;   /* 실린더 즉시 상승 */
                s_f_status_start = 0u;
                s_run_status     = WELD_READY;   /* CYL2 미경유, work_cnt++ 없음 */
            }
        } else if (s_temp_time == 0u) {
            /* slice-1 시간-exit (energy_ctrl off) — 무회귀. */
            out->weld_stop   = 1u;
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
