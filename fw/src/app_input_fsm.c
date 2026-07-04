/* fw/src/app_input_fsm.c — 순수 물리 입력 FSM.
 * START/RESET/SEEK = active-LOW edge-detect(_bak), E-stop = 레벨추종(std EMSW).
 * PC11 이중역할: model_type<=1(hand/multi)=SEEK / ==2(std)=EMSW. 비활성 역할은
 * 레거시 main.c:1192 충실(hand/multi에서 EMSW=0 강제) + bak 동기로 모드전환 시
 * stale 엣지 회피. */
#include "app_input_fsm.h"

static uint8_t s_start_bak;   /* active-LOW idle = 1 */
static uint8_t s_reset_bak;   /* active-LOW idle = 1 */
static uint8_t s_seek_bak;    /* active-LOW idle = 1 */
static uint8_t s_emsw_bak;    /* active-HIGH idle = 0 */
static uint8_t s_estop_active;

void input_fsm_init(void)
{
    s_start_bak    = 1u;
    s_reset_bak    = 1u;
    s_seek_bak     = 1u;
    s_emsw_bak     = 0u;
    s_estop_active = 0u;
}

input_out_t input_fsm_step(const input_in_t *in)
{
    input_out_t out = { 0u, 0u, 0u, 0u, 0u, 0u };

    /* B_START (PA15 active-LOW): 모멘터리 hold-to-run */
    if (in->start != s_start_bak) {
        if (in->start == 0u) { out.start_press = 1u; }
        else                 { out.start_release = 1u; }
        s_start_bak = in->start;
    }

    /* B_RESET (PC10 active-LOW): 눌림 엣지 */
    if (in->reset != s_reset_bak) {
        if (in->reset == 0u) { out.reset_press = 1u; }
        s_reset_bak = in->reset;
    }

    /* PC11 이중역할 (model_type 매 step) */
    if (in->model_type <= 1u) {
        /* hand/multi → B_SEEK active-LOW */
        if (in->estop_seek != s_seek_bak) {
            if (in->estop_seek == 0u) { out.seek_press = 1u; }
            s_seek_bak = in->estop_seek;
        }
        /* EMSW 비활성 (레거시 main.c:1192 re_emsw=0); bak 0 동기로 std 복귀 시
         * 즉시 재진입 가능, estop 강제 해제. */
        s_emsw_bak     = 0u;
        s_estop_active = 0u;
    } else {
        /* std → EMSW active-HIGH 레벨추종 */
        if (in->estop_seek != s_emsw_bak) {
            if (in->estop_seek != 0u) { out.estop_enter = 1u; }  /* 상승 엣지 */
            s_emsw_bak = in->estop_seek;
        }
        s_estop_active = (uint8_t)(in->estop_seek != 0u);
        /* SEEK bak 동기 (std에선 SEEK 미발화, hand/multi 복귀 시 stale 엣지 회피). */
        s_seek_bak = in->estop_seek;
    }

    out.estop_active = s_estop_active;
    return out;
}
