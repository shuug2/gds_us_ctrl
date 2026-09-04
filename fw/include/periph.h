/* fw/include/periph.h */
#pragma once
#include "stm32f4xx_hal.h"

extern UART_HandleTypeDef huart6;
extern UART_HandleTypeDef huart1;   /* USART1 — DGUS LCD (Stage A) */
extern DMA_HandleTypeDef hdma_usart1_rx;   /* USART1 RX — DMA2 S2 Ch4 circular (RX hardening) */
extern DMA_HandleTypeDef hdma_usart6_rx;   /* USART6 RX — DMA2 S1 Ch5 circular (Modbus RTU) */
extern TIM_HandleTypeDef  htim11;
extern TIM_HandleTypeDef  htim5;    /* FREQ_IN 입력캡처 (TIM5_CH1, PA0) */
extern I2C_HandleTypeDef hi2c1;   /* I2C1 — FM24C16B FRAM (Stage B) */
extern ADC_HandleTypeDef hadc1;   /* ADC1 — PB0/PB1 sense (Stage D regulation) */
extern IWDG_HandleTypeDef hiwdg;  /* IWDG — 슈퍼루프 워치독 (main.c 기동/kick) */
