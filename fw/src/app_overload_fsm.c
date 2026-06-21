/* fw/src/app_overload_fsm.c — 순수 과부하 디바운스 FSM. */
#include "app_overload_fsm.h"

static uint8_t s_count;    /* HIGH 연속 카운트 (cap N) */
static uint8_t s_active;   /* 디바운스된 활성 레벨 */

void overload_fsm_init(void)
{
    s_count  = 0u;
    s_active = 0u;
}

uint8_t overload_fsm_step(uint8_t raw)
{
    uint8_t prev = s_active;

    if (raw != 0u) {                       /* HIGH = fault */
        if (s_count < OVLD_DEBOUNCE_N) {
            s_count++;
        }
        if (s_count >= OVLD_DEBOUNCE_N) {
            s_active = 1u;
        }
    } else {                               /* LOW → 즉시 reset (noise reject, M16 충실) */
        s_count  = 0u;
        s_active = 0u;
    }

    uint8_t ev = s_active;                 /* bit0 = active 레벨 */
    if ((s_active != 0u) && (prev == 0u)) {
        ev |= OVLD_EV_ASSERT;
    }
    if ((s_active == 0u) && (prev != 0u)) {
        ev |= OVLD_EV_DEASSERT;
    }
    return ev;
}
