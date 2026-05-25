/* fw/include/i2c1.h — I2C1 raw layer (FM24C16B FRAM, PB6/PB7 AF4, 400 kHz). */
#pragma once
#include "stm32f4xx_hal.h"
#include <stdint.h>

#define I2C1_TIMEOUT_MS  50u

void i2c1_init(void);

/* Blocking FRAM-style access: [S][dev_w][mem_addr][...]. 1-byte memory address. */
HAL_StatusTypeDef i2c1_mem_read (uint8_t dev7, uint8_t mem_addr, uint8_t *buf, uint16_t n);
HAL_StatusTypeDef i2c1_mem_write(uint8_t dev7, uint8_t mem_addr, const uint8_t *buf, uint16_t n);

uint16_t i2c1_err_count(void);   /* non-OK transfer count (observability) */
