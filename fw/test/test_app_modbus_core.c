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
static uint8_t mk_req(uint8_t *f, uint8_t addr, uint8_t fc,
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

/* FC 03 — single + multi + response CRC validity + FC 04 echo. */
static void test_read_regs(void) {
    mb_core_t mb;
    uint8_t req[8], resp[MB_RESP_MAX];
    uint8_t fc = 0xEE;

    mb_core_init(&mb, 5);
    mb.holding[2] = 0x1234;
    mb.holding[3] = 0x00AB;

    /* single reg @0x0002 */
    mk_req(req, 5, 0x03, 0x0002, 0x0001);
    uint8_t n = mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc);
    CHECK_EQ(n, 7);             /* addr fc cnt hi lo crc2 */
    CHECK_EQ(fc, 0x03);
    CHECK_EQ(resp[0], 5);
    CHECK_EQ(resp[1], 0x03);
    CHECK_EQ(resp[2], 2);
    CHECK_EQ(resp[3], 0x12);
    CHECK_EQ(resp[4], 0x34);
    /* response carries a valid CRC over its own first n-2 bytes */
    uint16_t crc = mb_crc16(resp, (uint8_t)(n - 2u));
    CHECK_EQ(resp[5], (uint8_t)(crc >> 8));
    CHECK_EQ(resp[6], (uint8_t)crc);

    /* two regs @0x0002 */
    mk_req(req, 5, 0x03, 0x0002, 0x0002);
    n = mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc);
    CHECK_EQ(n, 9);
    CHECK_EQ(resp[2], 4);
    CHECK_EQ(resp[5], 0x00);
    CHECK_EQ(resp[6], 0xAB);

    /* full-map read: 50 regs from 0 -> 3 + 100 + 2 = 105 */
    mk_req(req, 5, 0x03, 0x0000, 0x0032);
    n = mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc);
    CHECK_EQ(n, 105);

    /* FC 04 mirrors FC 03 with its own echo */
    mk_req(req, 5, 0x04, 0x0002, 0x0001);
    n = mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc);
    CHECK_EQ(n, 7);
    CHECK_EQ(fc, 0x04);
    CHECK_EQ(resp[1], 0x04);
}

/* Port safety fix: out-of-range reads = silence (samd20 read past the table). */
static void test_read_regs_bounds(void) {
    mb_core_t mb;
    uint8_t req[8], resp[MB_RESP_MAX];
    uint8_t fc = 0xEE;
    mb_core_init(&mb, 5);

    mk_req(req, 5, 0x03, 0x0031, 0x0002);   /* 49 + 2 > 50 */
    CHECK_EQ(mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc), 0);
    CHECK_EQ(fc, 0);
    mk_req(req, 5, 0x03, 0x0000, 0x0000);   /* zero count */
    CHECK_EQ(mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc), 0);
    mk_req(req, 5, 0x03, 0x0000, 0x0033);   /* count 51 > 50 */
    CHECK_EQ(mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc), 0);

    /* fence-posts: last valid register reads fine; one past = silence */
    mk_req(req, 5, 0x03, 0x0031, 0x0001);   /* addr 49, num 1 -> ok */
    CHECK_EQ(mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc), 7);
    CHECK_EQ(fc, 0x03);
    mk_req(req, 5, 0x03, 0x0032, 0x0001);   /* addr 50, num 1 -> silence */
    CHECK_EQ(mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc), 0);
}

/* Silence paths: other addr, bad CRC, unsupported FC, runt frame (samd20:
 * no exception responses). TCP mode skips addr + CRC filtering. */
static void test_filters(void) {
    mb_core_t mb;
    uint8_t req[8], resp[MB_RESP_MAX];
    uint8_t fc = 0xEE;
    mb_core_init(&mb, 5);
    mb.holding[0] = 7;

    mk_req(req, 9, 0x03, 0x0000, 0x0001);   /* not our address */
    CHECK_EQ(mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc), 0);
    CHECK_EQ(fc, 0);

    mk_req(req, 5, 0x03, 0x0000, 0x0001);
    req[6] ^= 0xFFu;                        /* corrupt CRC */
    CHECK_EQ(mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc), 0);

    mk_req(req, 5, 0x10, 0x0000, 0x0001);   /* FC 16 unsupported */
    CHECK_EQ(mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc), 0);

    mk_req(req, 5, 0x03, 0x0000, 0x0001);
    CHECK_EQ(mb_core_decode(&mb, req, 3, MB_MODE_RTU, resp, &fc), 0);  /* runt */
    CHECK_EQ(mb_core_decode(&mb, req, 7, MB_MODE_RTU, resp, &fc), 0);  /* short */

    /* TCP mode: wrong addr AND garbage CRC bytes still processed (slice 2
     * strips MBAP; samd20 decode_comm(mode!=0) identical). len 6 = PDU only. */
    req[0] = 0xEEu; req[6] = 0; req[7] = 0;
    uint8_t n = mb_core_decode(&mb, req, 6, MB_MODE_TCP, resp, &fc);
    CHECK_EQ(n, 7);
    CHECK_EQ(fc, 0x03);
    CHECK_EQ(resp[4], 7);                   /* holding[0] low byte */
}

int main(void) {
    test_crc16();
    test_core_init();
    test_read_regs();
    test_read_regs_bounds();
    test_filters();
    if (failures) { printf("%d check(s) FAILED\n", failures); return 1; }
    printf("all checks PASSED\n");
    return 0;
}
