/* fw/drivers/tim.c */
#include "stm32f4xx_hal.h"
#include "periph.h"
#include "clock.h"

void tim11_init(void) {
    __HAL_RCC_TIM11_CLK_ENABLE();
    htim11.Instance               = TIM11;
    htim11.Init.Prescaler         = 95;
    htim11.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim11.Init.Period            = 999;
    htim11.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim11.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim11) != HAL_OK) Error_Handler();

    HAL_NVIC_SetPriority(TIM1_TRG_COM_TIM11_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(TIM1_TRG_COM_TIM11_IRQn);
}
