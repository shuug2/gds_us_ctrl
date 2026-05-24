/* fw/include/periph.h */
#pragma once
#include "stm32f4xx_hal.h"

extern UART_HandleTypeDef huart6;
extern UART_HandleTypeDef huart1;   /* USART1 — DGUS LCD (Stage A) */
extern TIM_HandleTypeDef  htim11;
extern I2C_HandleTypeDef hi2c1;   /* I2C1 — FM24C16B FRAM (Stage B) */
