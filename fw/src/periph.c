/* fw/src/periph.c */
#include "periph.h"

UART_HandleTypeDef huart6;
UART_HandleTypeDef huart1;   /* USART1 — DGUS LCD (Stage A) */
DMA_HandleTypeDef hdma_usart1_rx;   /* USART1 RX — DMA2 S2 Ch4 circular (RX hardening) */
DMA_HandleTypeDef hdma_usart6_rx;   /* USART6 RX — DMA2 S1 Ch5 circular (Modbus RTU) */
TIM_HandleTypeDef  htim11;
I2C_HandleTypeDef  hi2c1;   /* I2C1 — FM24C16B FRAM (Stage B) */
ADC_HandleTypeDef  hadc1;   /* ADC1 — PB0/PB1 sense (Stage D regulation) */
