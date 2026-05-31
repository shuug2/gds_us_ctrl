/* fw/include/periph.h */
#pragma once
#include "stm32f4xx_hal.h"

extern UART_HandleTypeDef huart6;
extern UART_HandleTypeDef huart1;   /* USART1 — DGUS LCD (Stage A) */
extern DMA_HandleTypeDef hdma_usart1_rx;   /* USART1 RX — DMA2 S2 Ch4 circular (RX hardening) */
extern TIM_HandleTypeDef  htim11;
extern I2C_HandleTypeDef hi2c1;   /* I2C1 — FM24C16B FRAM (Stage B) */
extern ADC_HandleTypeDef hadc1;   /* ADC1 — PB0/PB1 sense (Stage D regulation) */
