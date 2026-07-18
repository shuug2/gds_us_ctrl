/* fw/src/app_horn_fsm.c — SYS_HORN(horn-down) 순수 FSM. HAL-free.
 *
 * legacy 충실(samd20): 양손 START 아무거나 press 엣지 = 솔 토글
 * (main.c:1433-1440, re_start1||re_start2 단일 분기 = 동시 press도 1토글),
 * 모드 진입/이탈은 항상 솔 OFF(3457-3471) + 진입 시점 pending press 무시
 * (re_*_pressed=0). E-stop 처리는 의도적 이탈: legacy는 SYS_ESTOP 전이로
 * 모드 자체가 소멸하지만 DGUS 포팅은 모드 소유가 LCD 체크박스라 모드 유지 +
 * 솔 강제 OFF + 토글 차단(해제 후에도 자발 재하강 금지 — 새 press 필요).
 * bak zero-init = "눌림 가정" (legacy BSS zero-init 등가 — 2026-07-18 부팅
 * 유령 SEEK 교훈: 평시-활성 입력이 부팅/진입 엣지를 만들지 않게). */
#include "app_horn_fsm.h"

static uint8_t s_mode_prev;
static uint8_t s_key1_bak;    /* active-LOW idle = 1 (init은 의도적 0) */
static uint8_t s_key2_bak;    /* active-LOW idle = 1 (init은 의도적 0) */
static uint8_t s_horn;        /* 솔레노이드 상태 (1=하강) */

void horn_fsm_init(void)
{
    s_mode_prev = 0u;
    s_key1_bak  = 0u;
    s_key2_bak  = 0u;
    s_horn      = 0u;
}

uint8_t horn_fsm_step(const horn_in_t *in)
{
    uint8_t press = 0u;

    /* 엣지 검출 — bak는 모드/estop 무관 매 step 동기 (stale 엣지 방지) */
    if (in->key1 != s_key1_bak) {
        if (in->key1 == 0u) { press = 1u; }
        s_key1_bak = in->key1;
    }
    if (in->key2 != s_key2_bak) {
        if (in->key2 == 0u) { press = 1u; }
        s_key2_bak = in->key2;
    }

    if (in->mode == 0u) {
        s_horn = 0u;                        /* 이탈/비활성 = 솔 OFF (3466-3471) */
    } else if (in->estop != 0u) {
        s_horn = 0u;                        /* E-stop = 강제 OFF + 토글 차단 */
    } else if (s_mode_prev == 0u) {
        s_horn = 0u;                        /* 진입 step = OFF부터 + press 무시 */
    } else if (press != 0u) {
        s_horn ^= 1u;                       /* press 엣지 = 토글 (1433-1440) */
    }
    s_mode_prev = in->mode;
    return s_horn;
}
