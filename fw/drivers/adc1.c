/* fw/drivers/adc1.c — ADC1 peripheral init + polled read.
 * Pins: PB0 = ADC1_IN8 (ADC_SENS_OUT), PB1 = ADC1_IN9 (ADC_OSC). pinmap §52-57. */
#include "stm32f4xx_hal.h"
#include "periph.h"
#include "clock.h"   /* Error_Handler */
#include "adc1.h"

void adc1_init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitTypeDef g = {
        .Pin  = GPIO_PIN_0 | GPIO_PIN_1,
        .Mode = GPIO_MODE_ANALOG,
        .Pull = GPIO_NOPULL,
    };
    HAL_GPIO_Init(GPIOB, &g);

    __HAL_RCC_ADC1_CLK_ENABLE();
    hadc1.Instance                   = ADC1;
    hadc1.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode          = DISABLE;
    hadc1.Init.ContinuousConvMode    = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion       = 1;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) Error_Handler();
}

uint16_t adc1_read(adc1_ch_t ch)
{
    ADC_ChannelConfTypeDef s = {
        .Channel      = (ch == ADC1_CH_OSC) ? ADC_CHANNEL_9 : ADC_CHANNEL_8,
        .Rank         = 1,
        .SamplingTime = ADC_SAMPLETIME_84CYCLES,
    };
    if (HAL_ADC_ConfigChannel(&hadc1, &s) != HAL_OK) Error_Handler();

    /* Check Start + Poll: on timeout HAL_ADC_GetValue would return a stale DR
     * value that would silently enter the averaging accumulator. Conversion is
     * ~4 us vs the 2 ms guard (~500x margin), so a timeout means a real HW/clock
     * fault — halt like the Init/ConfigChannel checks above. */
    if (HAL_ADC_Start(&hadc1) != HAL_OK) Error_Handler();
    if (HAL_ADC_PollForConversion(&hadc1, 2u) != HAL_OK) Error_Handler();
    uint16_t v = (uint16_t)HAL_ADC_GetValue(&hadc1);
    (void)HAL_ADC_Stop(&hadc1);   /* best-effort cleanup */
    return v;
}
