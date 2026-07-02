/* fw/drivers/fram.c — FM24C16B FRAM driver over i2c1 (samd20 *_fram byte order verbatim). */
#include "fram.h"
#include "i2c1.h"

bool fram_read_byte(uint8_t addr, uint8_t *out)
{
    uint8_t b;
    if (i2c1_mem_read(FRAM_I2C_ADDR_7B, addr, &b, 1) != HAL_OK) {
        return false;
    }
    *out = b;
    return true;
}

bool fram_read_u16(uint8_t addr, uint16_t *out)
{
    uint8_t b[2];
    if (i2c1_mem_read(FRAM_I2C_ADDR_7B, addr, b, 2) != HAL_OK) {
        return false;
    }
    *out = (uint16_t)(((uint16_t)b[0] << 8) | (uint16_t)b[1]);
    return true;
}

bool fram_read_u32(uint8_t addr, uint32_t *out)
{
    uint8_t b[4];
    if (i2c1_mem_read(FRAM_I2C_ADDR_7B, addr, b, 4) != HAL_OK) {
        return false;
    }
    *out = ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16)
         | ((uint32_t)b[2] <<  8) |  (uint32_t)b[3];
    return true;
}

void fram_write_byte(uint8_t addr, uint8_t v)
{
    i2c1_mem_write(FRAM_I2C_ADDR_7B, addr, &v, 1);
}

void fram_write_u16(uint8_t addr, uint16_t v)
{
    uint8_t b[2];
    b[0] = (uint8_t)(v >> 8);
    b[1] = (uint8_t)v;
    i2c1_mem_write(FRAM_I2C_ADDR_7B, addr, b, 2);
}

void fram_write_u32(uint8_t addr, uint32_t v)
{
    uint8_t b[4];
    b[0] = (uint8_t)(v >> 24);
    b[1] = (uint8_t)(v >> 16);
    b[2] = (uint8_t)(v >> 8);
    b[3] = (uint8_t)v;
    i2c1_mem_write(FRAM_I2C_ADDR_7B, addr, b, 4);
}
