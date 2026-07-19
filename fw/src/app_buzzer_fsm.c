/* fw/src/app_buzzer_fsm.c — 순수 timed-beep FSM. */
#include "app_buzzer_fsm.h"

static uint16_t s_remaining;

/* 부저 FSM 초기화 */
void buzzer_fsm_init(void)            { s_remaining = 0u; }
/* beep tick 수 설정 */
void buzzer_fsm_beep(uint16_t ticks)  { s_remaining = ticks; }

/* 부저 FSM 1틱 진행 */
uint8_t buzzer_fsm_step(void)
{
    if (s_remaining > 0u) {
        s_remaining--;
        return 1u;
    }
    return 0u;
}
