/* fw/include/app_buzzer_fsm.h — 순수 timed-beep FSM (HAL-free, host-test). */
#pragma once
#include <stdint.h>

void    buzzer_fsm_init(void);           /* off */
void    buzzer_fsm_beep(uint16_t ticks); /* ticks 동안 on (재호출 시 리셋) */
uint8_t buzzer_fsm_step(void);           /* 1 tick 진행: 남으면 감소 후 1(on) 반환, 아니면 0 */
