/* fw/test/mock_fram.c — see mock_fram.h. Byte order mirrors fw/drivers/fram.c
 * (big-endian u16/u32) so app_config_save_all→load round-trips. */
#include <string.h>
#include "mock_fram.h"

static uint8_t  s_mem[256];
static uint8_t  s_fail[256];      /* 1 = read of this address fails */
static uint32_t s_write_cnt;

void mock_fram_reset(void)
{
    memset(s_mem, 0, sizeof(s_mem));
    memset(s_fail, 0, sizeof(s_fail));
    s_write_cnt = 0u;
}

void mock_fram_fail_read(uint8_t addr, uint8_t nbytes)
{
    for (uint8_t i = 0u; i < nbytes; i++) { s_fail[(uint8_t)(addr + i)] = 1u; }
}

void     mock_fram_poke(uint8_t addr, uint8_t v) { s_mem[addr] = v; }
uint8_t  mock_fram_peek(uint8_t addr)            { return s_mem[addr]; }
uint32_t mock_fram_write_count(void)             { return s_write_cnt; }
void     mock_fram_clear_write_count(void)       { s_write_cnt = 0u; }

static int span_fails(uint8_t addr, uint8_t n)
{
    for (uint8_t i = 0u; i < n; i++) {
        if (s_fail[(uint8_t)(addr + i)]) { return 1; }
    }
    return 0;
}

bool fram_read_byte(uint8_t addr, uint8_t *out)
{
    if (span_fails(addr, 1u)) { return false; }
    *out = s_mem[addr];
    return true;
}

bool fram_read_u16(uint8_t addr, uint16_t *out)
{
    if (span_fails(addr, 2u)) { return false; }
    *out = (uint16_t)(((uint16_t)s_mem[addr] << 8) | s_mem[(uint8_t)(addr + 1u)]);
    return true;
}

bool fram_read_u32(uint8_t addr, uint32_t *out)
{
    if (span_fails(addr, 4u)) { return false; }
    *out = ((uint32_t)s_mem[addr] << 24) | ((uint32_t)s_mem[(uint8_t)(addr + 1u)] << 16)
         | ((uint32_t)s_mem[(uint8_t)(addr + 2u)] << 8) | (uint32_t)s_mem[(uint8_t)(addr + 3u)];
    return true;
}

void fram_write_byte(uint8_t addr, uint8_t v)
{
    s_mem[addr] = v;
    s_write_cnt++;
}

void fram_write_u16(uint8_t addr, uint16_t v)
{
    s_mem[addr] = (uint8_t)(v >> 8);
    s_mem[(uint8_t)(addr + 1u)] = (uint8_t)v;
    s_write_cnt++;
}

void fram_write_u32(uint8_t addr, uint32_t v)
{
    s_mem[addr] = (uint8_t)(v >> 24);
    s_mem[(uint8_t)(addr + 1u)] = (uint8_t)(v >> 16);
    s_mem[(uint8_t)(addr + 2u)] = (uint8_t)(v >> 8);
    s_mem[(uint8_t)(addr + 3u)] = (uint8_t)v;
    s_write_cnt++;
}
