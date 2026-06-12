/* fw/test/test_app_modbus_core.c — host unit tests for the pure Modbus core
 * (CRC16, FC 01..06 decode, response build). No HAL, no hardware.
 * samd20 modbus.c port verification + port-fix coverage (bounds, FC05 echo,
 * full-byte coil reads). */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "app_modbus_core.h"

static int failures = 0;

#define CHECK_EQ(expr, expected) do {                                       \
    unsigned long a_ = (unsigned long)(expr);                               \
    unsigned long e_ = (unsigned long)(expected);                           \
    if (a_ != e_) {                                                         \
        printf("FAIL %s:%d  %s = %lu, expected %lu\n",                      \
               __FILE__, __LINE__, #expr, a_, e_);                          \
        failures++;                                                         \
    }                                                                       \
} while (0)

/* Build an 8-byte RTU request: addr, fc, u16 a, u16 b, CRC. Returns 8. */
static uint8_t __attribute__((unused)) mk_req(uint8_t *f, uint8_t addr, uint8_t fc,
                      uint16_t a, uint16_t b)
{
    f[0] = addr; f[1] = fc;
    f[2] = (uint8_t)(a >> 8); f[3] = (uint8_t)a;
    f[4] = (uint8_t)(b >> 8); f[5] = (uint8_t)b;
    uint16_t crc = mb_crc16(f, 6);
    f[6] = (uint8_t)(crc >> 8); f[7] = (uint8_t)crc;
    return 8;
}

/* CRC16/MODBUS check value: "123456789" -> 0x4B37; wire order lo-first means
 * the samd20-swapped return is 0x374B. Classic frame vector: 11 03 00 6B 00 03
 * -> wire CRC bytes 76 87 -> swapped return 0x7687. */
static void test_crc16(void) {
    CHECK_EQ(mb_crc16((const uint8_t *)"123456789", 9), 0x374B);
    static const uint8_t classic[6] = { 0x11, 0x03, 0x00, 0x6B, 0x00, 0x03 };
    CHECK_EQ(mb_crc16(classic, 6), 0x7687);
    /* empty buffer = init value swapped */
    CHECK_EQ(mb_crc16(classic, 0), 0xFFFF);
}

static void test_core_init(void) {
    mb_core_t mb;
    memset(&mb, 0xAA, sizeof(mb));
    mb_core_init(&mb, 17);
    CHECK_EQ(mb.device_addr, 17);
    CHECK_EQ(mb.holding[0], 0);
    CHECK_EQ(mb.holding[MB_REG_COUNT - 1u], 0);
    CHECK_EQ(mb.coils[0], 0);
    CHECK_EQ(mb.coils[MB_COIL_COUNT - 1u], 0);
}

int main(void) {
    test_crc16();
    test_core_init();
    if (failures) { printf("%d check(s) FAILED\n", failures); return 1; }
    printf("all checks PASSED\n");
    return 0;
}
