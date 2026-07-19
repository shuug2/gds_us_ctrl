/* fw/src/app_weld_trigger_fsm.c — slice4 물리 트리거/센서 순수 FSM.
 * samd20 check_remote_input() weld 몫 (main.c:1187-1268, 1404-1407, 1484). */
#include "app_weld_trigger_fsm.h"
#include "app_weld_fsm.h"   /* WELD_READY / WELD_CYL1 */

static uint8_t s_in_cycle;   /* main.c:1472 set / 1219 clear */
static uint8_t s_dn_bak;     /* 엣지 검출용 이전 레벨 (main.c re_dn_bak) */
static uint8_t s_up_bak;

/* 트리거 FSM 초기화 */
void weld_trigger_fsm_init(void)
{
    s_in_cycle = 0u;
    s_dn_bak   = 1u;     /* idle = released(HIGH) */
    s_up_bak   = 1u;
}

/* in_cycle 무장 */
void weld_trigger_fsm_cycle_started(void)
{
    /* 게이팅 통과·사이클 실시작 시에만 in_cycle 무장 — 게이팅에 막힌 트리거가
     * 양손 재장전을 강요하지 않도록 set 시점을 글루에 위임 (spec §2.3). */
    s_in_cycle = 1u;
}

/* 트리거/센서 1스텝 */
void weld_trigger_fsm_step(const weld_trig_in_t *in, weld_trig_out_t *out)
{
    /* 양손 시작 (main.c:1404): 둘 다 press(0) && !in_cycle. 레벨 파생 —
     * in_cycle=0 동안 반복 발행, 소비는 글루(READY+게이팅). */
    out->start_pulse = ((in->key1 == 0u) && (in->key2 == 0u) &&
                        (s_in_cycle == 0u)) ? 1u : 0u;

    /* 재장전 (main.c:1219): 양손 release && READY 에서만 해제. */
    if ((in->key1 == 1u) && (in->key2 == 1u) &&
        (in->weld_state == (uint8_t)WELD_READY)) {
        s_in_cycle = 0u;
    }

    /* safety abort (main.c:1484): f_safty && CYL1 && 한 손이라도 release.
     * 조건 지속 동안 매 step 발행 — FSM abort가 CYL1을 벗어나며 자연 소멸. */
    out->safety_abort_pulse = ((in->f_safty != 0u) &&
                               (in->weld_state == (uint8_t)WELD_CYL1) &&
                               ((in->key1 == 1u) || (in->key2 == 1u))) ? 1u : 0u;

    /* 센서 press 엣지 (main.c:1222-1233 bak 패턴). */
    out->dn_edge = ((in->sens_dn == 0u) && (s_dn_bak == 1u)) ? 1u : 0u;
    out->up_edge = ((in->sens_up == 0u) && (s_up_bak == 1u)) ? 1u : 0u;
    s_dn_bak = in->sens_dn;
    s_up_bak = in->sens_up;
}
