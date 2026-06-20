/* fw/drivers/freq_ic.c — FREQ_IN 입력캡처. PA0 = TIM5_CH1 (AF2, GPIO_AF2_TIM5).
 * TIM5는 F410 유일 32-bit 타이머(IS_TIM_32B_COUNTER_INSTANCE==TIM5) → prescaler 0,
 * ARR=0xFFFFFFFF free-run, 연속 rising 캡처 간 Δ = 입력 주기(96MHz 틱). */
#include "stm32f4xx_hal.h"
#include "periph.h"
#include "clock.h"    /* Error_Handler */
#include "freq_ic.h"
#include "app_freq_fsm.h"

void freq_ic_init(void)
{
    freq_fsm_init();                 /* IRQ 켜기 전에 소비자 상태 리셋 */

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_TIM5_CLK_ENABLE();

    GPIO_InitTypeDef g = {
        .Pin       = GPIO_PIN_0,
        .Mode      = GPIO_MODE_AF_PP,
        .Pull      = GPIO_NOPULL,
        .Speed     = GPIO_SPEED_FREQ_HIGH,
        .Alternate = GPIO_AF2_TIM5,   /* F410: PA0 TIM5_CH1 = AF2 */
    };
    HAL_GPIO_Init(GPIOA, &g);

    htim5.Instance               = TIM5;
    htim5.Init.Prescaler         = 0u;            /* 96 MHz 풀 해상도 */
    htim5.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim5.Init.Period            = 0xFFFFFFFFUL;  /* 32-bit free-run */
    htim5.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim5.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_IC_Init(&htim5) != HAL_OK) { Error_Handler(); }

    TIM_IC_InitTypeDef ic = {
        .ICPolarity  = TIM_ICPOLARITY_RISING,
        .ICSelection = TIM_ICSELECTION_DIRECTTI,
        .ICPrescaler = TIM_ICPSC_DIV1,            /* 매 rising 캡처 */
        .ICFilter    = 0u,                        /* 필터 없음 (SAMD20 충실); 지터 시 HW에서 상향 */
    };
    if (HAL_TIM_IC_ConfigChannel(&htim5, &ic, TIM_CHANNEL_1) != HAL_OK) { Error_Handler(); }

    /* 캡처 ISR 우선순위: sys_tick(TIM11=5) 및 comm보다 낮게(=숫자 큼). 캡처 유실은
     * 10샘플 평균이 흡수하므로 비치명적. ~50kHz×수십cyc ≈ 1.5% CPU. */
    HAL_NVIC_SetPriority(TIM5_IRQn, 10, 0);
    HAL_NVIC_EnableIRQ(TIM5_IRQn);

    if (HAL_TIM_IC_Start_IT(&htim5, TIM_CHANNEL_1) != HAL_OK) { Error_Handler(); }
}
