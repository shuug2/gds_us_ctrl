/* fw/src/app_weld_fsm.c — weld-cycle FSM core (slice 1, DELAY mode). HAL-free. */
#include "app_weld_fsm.h"
#include <string.h>

static uint8_t  s_run_status;
static uint8_t  s_f_status_start;   /* per-state "first entry done" flag */
static uint16_t s_temp_time;        /* active countdown (10ms/count) */
static uint16_t s_comp_time;        /* weld amplitude-comp counter (samd20) */
static uint8_t  s_sol_dn;           /* solenoid level held across steps */
static uint8_t  s_multi_stage;      /* 0 = out1 단계, 1 = out2 단계 (samd20 multi_ctrl_stage) */
static uint16_t s_multi_elapsed;    /* WELD 진입 후 경과 (10ms/count); time1/time2와 직접 비교 */
static uint8_t  s_latched_multi;    /* H1: WELD 진입 시 multi_ctrl 스냅샷 (감사 D4) */
static uint8_t  s_latched_energy;   /* H1: WELD 진입 시 energy_ctrl 스냅샷 */
static uint8_t  s_run_mode;         /* 사이클 시작 시 래치 (모드 혼합 방지, slice4) */
static uint8_t  s_dn_pressed;       /* legacy re_dn_pressed 재현 (내부 래치) */
static uint8_t  s_up_pressed;       /* legacy re_up_pressed 재현 */

void weld_fsm_init(void)
{
    s_run_status     = WELD_READY;
    s_f_status_start = 0u;
    s_temp_time      = 0u;
    s_comp_time      = WELD_COMP_FULL;
    s_sol_dn         = 0u;
    s_multi_stage    = 0u;
    s_multi_elapsed  = 0u;
    s_latched_multi  = 0u;
    s_latched_energy = 0u;
    s_run_mode       = 0u;
    s_dn_pressed     = 0u;
    s_up_pressed     = 0u;
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

/* multi 경로 진폭: comp_time 미적용 (samd20 main.c:5242/1540 직접 변환). 팩토리
 * 기본 limit_mo_out1=25(<50)가 언더플로 트리거 -> 명시적 0 가드. weld_amplitude는
 * output_power [50,100] 클램프(upstream) 가정이라 가드 없으나, limit_mo_out은
 * config-validation 클램프가 슬라이스4 이연이라 여기서 방어. spec §3.5. */
static uint8_t weld_mo_amplitude(uint8_t mo_out)
{
    if (mo_out < 50u) {
        return 0u;
    }
    return (uint8_t)(((uint16_t)(mo_out - 50u) * 255u) / 100u);
}

/* CYL1 -> WELD 전이 (DELAY/TRIGGER 공용, DRY — slice4 §3.2). 호출측이
 * weld_time = ldt2(DELAY) 또는 trigger_time2(TRIGGER)를 넘긴다. H1: 무장(temp_time
 * 선택)과 WELD 최초진입(진폭 분기)이 같은 값을 보도록 전이 시점에 단일 스냅샷으로
 * multi_ctrl/energy_ctrl을 래치 — 사이 1-tick 토글 창 제거 (감사 D4). */
static void enter_weld(const weld_in_t *in, uint16_t weld_time)
{
    s_f_status_start = 0u;
    s_run_status     = WELD_WELD;
    s_latched_multi  = in->multi_ctrl;   /* H1 래치 (전이 시점 = 단일 스냅샷) */
    s_latched_energy = in->energy_ctrl;
    s_multi_stage    = 0u;               /* H1 카운터 리셋 */
    s_multi_elapsed  = 0u;
    /* comp_time은 항상 weld_time에서 유도 (진폭 보정; energy 모드에서도 적용 —
     * samd20 main.c:1546-1547은 energy_ctrl 무관). slice2 §3.2. */
    s_comp_time = (weld_time > 6u) ? WELD_COMP_FULL : weld_time;
    /* temp_time: energy 모드 -> backstop 카운트다운(limit_out_time초 ×100 tick),
     * 시간 모드 -> weld_time/comp swap (samd20 main.c:1504). s_latched_energy로
     * 통일 — 무장과 exit가 동일 스냅샷을 사용 (H1). */
    if (s_latched_energy) {
        s_temp_time = weld_backstop_ticks(in->limit_out_time);
    } else {
        s_temp_time = (weld_time > 6u) ? weld_time : WELD_COMP_FULL;
    }
}

/* WELD exit 시 HOLD 무장 시간 (DELAY/TRIGGER 공용, DRY — slice4 §3.2). WELD exit는
 * 3갈래(multi/energy/시간)이므로 helper로 단일화. */
static uint16_t hold_time(const weld_in_t *in)
{
    return (s_run_mode != 0u) ? in->limit_trigger_time3 : in->limit_delay_time3;
}

void weld_fsm_step(const weld_in_t *in, weld_out_t *out)
{
    memset(out, 0, sizeof(*out));

    /* abort (slice4 §3.4): 임의 상태 -> SOL OFF + READY. WELD 중이면 US 정지 엣지
     * (글루 US_CYCLE RUN_RELEASE — slice-c/d force-stop과 이중 안전). work_cnt 미발행.
     * legacy: E-stop main.c:1415 / SYS_ERROR 1664-1665. */
    if ((in->abort != 0u) && (s_run_status != WELD_READY)) {
        if (s_run_status == WELD_WELD) {
            out->weld_stop = 1u;
        }
        s_sol_dn         = 0u;
        s_run_status     = WELD_READY;
        s_f_status_start = 0u;
        s_temp_time      = 0u;
        s_multi_stage    = 0u;
        s_multi_elapsed  = 0u;
        s_latched_multi  = 0u;
        s_latched_energy = 0u;
        s_run_mode       = 0u;
        s_dn_pressed     = 0u;
        s_up_pressed     = 0u;
        out->run_status  = s_run_status;
        out->sol_dn      = s_sol_dn;
        return;
    }

    /* slice4: SENSE_DN/UP 1-shot 엣지 -> 내부 래치(legacy re_dn_pressed/re_up_pressed
     * 재현). 소비는 각 상태 분기(CYL1/CYL2)에서. */
    if (in->dn_edge) { s_dn_pressed = 1u; }
    if (in->up_edge) { s_up_pressed = 1u; }

    /* 10 ms elapsed: active timer counts down (samd20 timer temp_time--). */
    if (s_temp_time > 0u) {
        s_temp_time--;
    }

    switch (s_run_status) {
    case WELD_READY:
        if (in->start) {
            s_run_status     = WELD_CYL1;
            s_run_mode       = in->run_mode;          /* 사이클 단위 래치 (slice4) */
            s_temp_time      = in->limit_delay_time1; /* TRIGGER에선 미사용 (아래 분기) */
            s_f_status_start = 0u;       /* CYL1 first-entry drives SOL_DN ON */
            s_dn_pressed     = 0u;       /* stale 클리어 (main.c:1478) */
        }
        break;

    case WELD_CYL1:
        if (s_f_status_start == 0u) {
            s_f_status_start = 1u;
            s_sol_dn         = 1u;       /* SOL_DN ON (cylinder descends) */
        } else if (s_run_mode != 0u) {   /* TRIGGER (main.c:1513-1528) */
            if (s_dn_pressed != 0u) {
                s_dn_pressed = 0u;
                enter_weld(in, in->limit_trigger_time2);
            }
            /* dn 없음 -> 무기한 대기 (죽은 legacy CYL_TIMEOUT 충실히 미구현, spec §3.2). */
        } else if (s_temp_time == 0u) {          /* WELD_CYL1 전이 블록 (DELAY) */
            enter_weld(in, in->limit_delay_time2);
        }
        break;

    case WELD_WELD:
        if (s_f_status_start == 0u) {
            s_f_status_start = 1u;
            if (s_latched_multi) {       /* H1: 전이 시점 스냅샷 (in->multi_ctrl 아님 —
                                             무장-직후 1-tick 토글 창 제거) */
                s_multi_stage   = 0u;
                s_multi_elapsed = 1u;  /* weld_start step은 elapsed=1로 시작 (전환 step 포함, slice-1 s_temp_time 정합) */
                out->amplitude  = weld_mo_amplitude(in->limit_mo_out1);  /* 1단 (comp 미적용) */
            } else {
                out->amplitude  = weld_amplitude(in->output_power, s_comp_time);
            }
            out->weld_start  = 1u;       /* glue: US_CYCLE START + pot write */
        } else if (s_latched_multi) {
            /* multi: 2단 진폭 스테핑 (samd20 5232-5258). 우선순위 최상 — energy/시간
             * exit 미발동(아래 else-if 분기 진입 안 함). spec §3.4. H1: 전이 시점
             * 스냅샷(s_latched_multi) 참조 — 런중 in->multi_ctrl 토글 무시. */
            if (s_multi_elapsed < 0xFFFFu) {
                s_multi_elapsed++;                   /* 포화 가드 (time2 매우 클 때 wrap 방지) */
            }
            if (s_multi_stage == 0u && s_multi_elapsed >= in->limit_mo_time1) {
                s_multi_stage   = 1u;
                out->amplitude  = weld_mo_amplitude(in->limit_mo_out2);  /* 2단 (samd20 5242) */
                out->amp_change = 1u;                /* glue: set_amp 재호출 */
            }
            if (s_multi_elapsed >= in->limit_mo_time2) {
                out->weld_stop   = 1u;               /* samd20 5250: WELD->HOLD */
                s_f_status_start = 0u;
                s_run_status     = WELD_HOLD;
                s_temp_time      = hold_time(in);
            }
        } else if (s_latched_energy) {
            /* energy 모드: 에너지 도달 -> 정상 종료(samd20 5272); 미도달 +
             * backstop 만료 -> abort(samd20 5288, 에러 표시는 이연). spec §3.3. H1:
             * 전이 시점 스냅샷(s_latched_energy) 참조 — 런중 in->energy_ctrl 토글 무시. */
            if ((in->limit_energy != 0u) && (in->curr_energy >= in->limit_energy)) {
                out->weld_stop   = 1u;
                s_f_status_start = 0u;
                s_run_status     = WELD_HOLD;
                s_temp_time      = hold_time(in);
            } else if (s_temp_time == 0u) {
                out->weld_stop   = 1u;   /* abort도 US 정지 */
                out->weld_fault  = 1u;   /* glue: fault hook → app_reg_raise_ovtime
                                          * (ERR_OVTIME=legacy SYS_ERROR, 2026-07-18) */
                s_sol_dn         = 0u;   /* 실린더 즉시 상승 */
                s_f_status_start = 0u;
                s_run_status     = WELD_READY;   /* CYL2 미경유, work_cnt++ 없음 */
                s_latched_multi  = 0u;            /* H1: READY 복귀 지점 클리어 */
                s_latched_energy = 0u;
            }
        } else if (s_temp_time == 0u) {
            /* slice-1 시간-exit (energy_ctrl off) — 무회귀. */
            out->weld_stop   = 1u;
            s_f_status_start = 0u;
            s_run_status     = WELD_HOLD;
            s_temp_time      = hold_time(in);
        }
        break;

    case WELD_HOLD:
        if (s_f_status_start == 0u) {
            s_f_status_start = 1u;
        } else if (s_temp_time == 0u) {
            s_f_status_start = 0u;
            s_run_status     = WELD_CYL2;
            s_temp_time      = in->limit_delay_time1;  /* DELAY용; TRIGGER는 미사용 */
            if (s_run_mode != 0u) {
                s_up_pressed = 1u;   /* legacy 강제 set (main.c:1593 "//-") — CYL2 즉시
                                      * exit. SENSE_UP 실대기 필요 판정 시 이 줄 제거 (spec §3.3). */
            }
        }
        break;

    case WELD_CYL2:
        if (s_f_status_start == 0u) {
            s_f_status_start = 1u;
            s_sol_dn         = 0u;       /* SOL_DN OFF (cylinder rises) */
        } else if (s_run_mode != 0u) {   /* TRIGGER (main.c:1618-1629) */
            if (s_up_pressed != 0u) {
                s_up_pressed     = 0u;
                s_f_status_start = 0u;
                s_run_status     = WELD_READY;
                out->cycle_done  = 1u;       /* glue: work_cnt++ */
                s_latched_multi  = 0u;       /* H1: READY 복귀 지점 클리어 */
                s_latched_energy = 0u;
            }
        } else if (s_temp_time == 0u) {
            s_f_status_start = 0u;
            s_run_status     = WELD_READY;
            out->cycle_done  = 1u;       /* glue: work_cnt++ */
            s_latched_multi  = 0u;       /* H1: READY 복귀 지점 클리어 */
            s_latched_energy = 0u;
        }
        break;

    default:
        /* unreachable in normal operation (s_run_status is only ever a WELD_*
         * value); fail-safe on fault — drop the solenoid (cpp-review LOW-2). */
        s_run_status     = WELD_READY;
        s_sol_dn         = 0u;
        s_latched_multi  = 0u;          /* H1: READY 복귀 지점 클리어 */
        s_latched_energy = 0u;
        break;
    }

    out->run_status = s_run_status;
    out->sol_dn     = s_sol_dn;
}
