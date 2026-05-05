/* fw/src/clock.c */
#include "stm32f4xx_hal.h"
#include "clock.h"

void clock_init(void) {
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    osc.OscillatorType       = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState             = RCC_HSI_ON;
    osc.HSICalibrationValue  = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState         = RCC_PLL_ON;
    osc.PLL.PLLSource        = RCC_PLLSOURCE_HSI;
    osc.PLL.PLLM             = 8;
    osc.PLL.PLLN             = 96;
    osc.PLL.PLLP             = RCC_PLLP_DIV2;
    osc.PLL.PLLQ             = 4;
    osc.PLL.PLLR             = 2;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) Error_Handler();

    clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                       | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_3) != HAL_OK) Error_Handler();
}
