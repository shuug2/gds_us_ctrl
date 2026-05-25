/* fw/src/sys_tick.c */
#include "stm32f4xx_hal.h"
#include "periph.h"
#include "clock.h"
#include "sys_tick.h"

static volatile uint32_t s_ms = 0;

void sys_tick_init(void) {
    if (HAL_TIM_Base_Start_IT(&htim11) != HAL_OK) Error_Handler();
}

uint32_t sys_tick_get_ms(void) {
    return s_ms;   /* 32-bit read는 Cortex-M4에서 atomic */
}

void sys_tick_delay_ms(uint32_t ms) {
    uint32_t t0 = sys_tick_get_ms();
    while ((uint32_t)(sys_tick_get_ms() - t0) < ms) {
        /* busy-wait on TIM11 1 ms tick; NOT HAL_Delay (HAL SysTick is not used here) */
    }
}

void sys_tick_handle_irq(void) {
    s_ms++;
}
