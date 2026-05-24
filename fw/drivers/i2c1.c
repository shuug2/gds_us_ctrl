/* fw/drivers/i2c1.c
 *
 * I2C1 raw I/O — FM24C16B FRAM (PB6=SCL / PB7=SDA, AF4, 400 kHz Fast mode).
 * External 10 kΩ pull-ups to VCC_5 → GPIO_NOPULL. PCLK1 = 48 MHz (APB1 = HCLK/2).
 * Caller: fw/drivers/fram.c only.
 */
#include "stm32f4xx_hal.h"
#include "periph.h"
#include "clock.h"   /* Error_Handler */
#include "i2c1.h"

static volatile uint16_t s_err_count;

void i2c1_init(void)
{
    /* 1. clocks */
    __HAL_RCC_I2C1_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* 2. GPIO PB6/PB7 AF4, open-drain, no internal pull (external 10k to 5V) */
    GPIO_InitTypeDef g = {0};
    g.Pin       = GPIO_PIN_6 | GPIO_PIN_7;
    g.Mode      = GPIO_MODE_AF_OD;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &g);

    /* 3. I2C config — 400 kHz Fast mode */
    hi2c1.Instance             = I2C1;
    hi2c1.Init.ClockSpeed      = 400000;
    hi2c1.Init.DutyCycle       = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1     = 0;
    hi2c1.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2     = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
        Error_Handler();
    }

    s_err_count = 0;
}

HAL_StatusTypeDef i2c1_mem_read(uint8_t dev7, uint8_t mem_addr, uint8_t *buf, uint16_t n)
{
    HAL_StatusTypeDef st = HAL_I2C_Mem_Read(&hi2c1, (uint16_t)(dev7 << 1), mem_addr,
                                            I2C_MEMADD_SIZE_8BIT, buf, n, I2C1_TIMEOUT_MS);
    if (st != HAL_OK) {
        s_err_count++;
    }
    return st;
}

HAL_StatusTypeDef i2c1_mem_write(uint8_t dev7, uint8_t mem_addr, const uint8_t *buf, uint16_t n)
{
    HAL_StatusTypeDef st = HAL_I2C_Mem_Write(&hi2c1, (uint16_t)(dev7 << 1), mem_addr,
                                             I2C_MEMADD_SIZE_8BIT, (uint8_t *)buf, n, I2C1_TIMEOUT_MS);
    if (st != HAL_OK) {
        s_err_count++;
    }
    return st;
}

uint16_t i2c1_err_count(void) { return s_err_count; }
