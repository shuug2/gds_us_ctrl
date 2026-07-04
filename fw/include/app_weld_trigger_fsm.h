/* fw/include/app_weld_trigger_fsm.h — slice4 물리 트리거/센서 순수 FSM.
 * samd20 check_remote_input()의 weld 몫(SW_START1/2 양손 + in_cycle 재장전 +
 * f_safty CYL1 abort + SENSE_UP/DN 엣지) 분리. HAL-free, host-test.
 * 입력 = raw 레벨(active-LOW: 0=press/감지), 10ms tick마다 호출. */
#pragma once
#include <stdint.h>

typedef struct {
    uint8_t key1;        /* SW_START1 raw (PC12) */
    uint8_t key2;        /* SW_START2 raw (PB11) */
    uint8_t sens_up;     /* SENSE_UP raw (PA12) */
    uint8_t sens_dn;     /* SENSE_DN raw (PA11) */
    uint8_t f_safty;     /* cfg->f_safty */
    uint8_t weld_state;  /* weld_fsm_status() — READY/CYL1 판정용 */
} weld_trig_in_t;

typedef struct {
    uint8_t start_pulse;        /* 양손 press && !in_cycle — 레벨 파생 (게이팅/소비는 글루) */
    uint8_t safety_abort_pulse; /* f_safty && CYL1 && 한 손 release (조건 지속 동안 매 step) */
    uint8_t dn_edge;            /* SENSE_DN 1->0 엣지 1-shot */
    uint8_t up_edge;            /* SENSE_UP 1->0 엣지 1-shot */
} weld_trig_out_t;

void weld_trigger_fsm_init(void);
void weld_trigger_fsm_step(const weld_trig_in_t *in, weld_trig_out_t *out);
void weld_trigger_fsm_cycle_started(void);
