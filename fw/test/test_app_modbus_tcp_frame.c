/* Host unit test for the pure Modbus-TCP framing unit. Manual asserts
 * (same style as test_app_modbus_core.c). */
#include <stdio.h>
#include <string.h>
#include "app_modbus_tcp_frame.h"
#include "app_modbus_core.h"

static int g_fail = 0;
#define CHECK(c) do { if(!(c)){ printf("FAIL %s:%d  %s\n",__FILE__,__LINE__,#c); g_fail++; } } while(0)

/* FC03 read 1 holding register at 0x0006 (OUT_POWER) = 80 (0x0050). */
static void test_fc03_read_one(void)
{
    mb_core_t mb;
    mb_core_init(&mb, 1u);
    mb.holding[MB_REG_OUT_POWER] = 80u;

    /* MBAP txn=0x0001 proto=0 length=6 ; PDU unit=1 fc=3 start=0x0006 cnt=0x0001 */
    uint8_t req[] = { 0x00,0x01, 0x00,0x00, 0x00,0x06,
                      0x01, 0x03, 0x00,0x06, 0x00,0x01 };
    uint8_t out[MB_TCP_RESP_MAX];
    uint16_t out_len = 0u;
    uint8_t  fc = 0xFFu;

    bool resp = mb_tcp_build_response(&mb, req, (uint16_t)sizeof req, out, &out_len, &fc);
    CHECK(resp == true);
    CHECK(fc == 0x03u);
    /* expected wire: txn echo 0001, proto 0000, len 0005, unit 01, fc 03,
       bytecount 02, data 00 50  — NO CRC. total 11 bytes. */
    CHECK(out_len == 11u);
    CHECK(out[0]==0x00 && out[1]==0x01);   /* txn echo */
    CHECK(out[2]==0x00 && out[3]==0x00);   /* proto */
    CHECK(out[4]==0x00 && out[5]==0x05);   /* length = 5 (big-endian) */
    CHECK(out[6]==0x01);                   /* unit echo */
    CHECK(out[7]==0x03);                   /* fc */
    CHECK(out[8]==0x02);                   /* byte count */
    CHECK(out[9]==0x00 && out[10]==0x50);  /* value 80 */
}

/* FC06 write OUT_POWER = 90 (0x005A): standard response echoes the request PDU. */
static void test_fc06_write(void)
{
    mb_core_t mb;
    mb_core_init(&mb, 1u);

    uint8_t req[] = { 0x12,0x34, 0x00,0x00, 0x00,0x06,
                      0x01, 0x06, 0x00,0x06, 0x00,0x5A };
    uint8_t out[MB_TCP_RESP_MAX];
    uint16_t out_len = 0u;
    uint8_t  fc = 0u;

    bool resp = mb_tcp_build_response(&mb, req, (uint16_t)sizeof req, out, &out_len, &fc);
    CHECK(resp == true);
    CHECK(fc == 0x06u);                    /* caller will run apply pass */
    CHECK(out_len == 12u);                 /* 6 MBAP + [unit fc addrHi addrLo valHi valLo] */
    CHECK(out[0]==0x12 && out[1]==0x34);   /* txn echo */
    CHECK(out[4]==0x00 && out[5]==0x06);   /* length = 6 */
    CHECK(out[6]==0x01 && out[7]==0x06);   /* unit, fc */
    CHECK(out[8]==0x00 && out[9]==0x06);   /* reg addr echo */
    CHECK(out[10]==0x00 && out[11]==0x5A); /* value echo */
    /* and the holding reg actually took the write (core wrote it): */
    CHECK(mb.holding[MB_REG_OUT_POWER] == 0x5Au);
}

static void test_rejects(void)
{
    mb_core_t mb;
    mb_core_init(&mb, 1u);
    uint8_t out[MB_TCP_RESP_MAX];
    uint16_t out_len = 0u; uint8_t fc = 0u;

    /* too short (<8) */
    uint8_t a[] = { 0,1,0,0,0,1,1 };
    CHECK(mb_tcp_build_response(&mb, a, sizeof a, out, &out_len, &fc) == false);

    /* protocol id != 0 */
    uint8_t b[] = { 0,1, 0,9, 0,6, 1,3,0,6,0,1 };
    CHECK(mb_tcp_build_response(&mb, b, sizeof b, out, &out_len, &fc) == false);

    /* declared length longer than received */
    uint8_t c[] = { 0,1, 0,0, 0,20, 1,3,0,6,0,1 };
    CHECK(mb_tcp_build_response(&mb, c, sizeof c, out, &out_len, &fc) == false);
}

/* Standard Modbus TCP echoes the REQUEST unit id, not the core device_addr.
 * device_addr=0x01 but request unit=0x11 → response unit must be 0x11. */
static void test_unit_echo(void)
{
    mb_core_t mb;
    mb_core_init(&mb, 0x01u);
    mb.holding[MB_REG_OUT_POWER] = 80u;

    /* unit byte (req[6]) = 0x11, differs from device_addr 0x01 */
    uint8_t req[] = { 0x00,0x01, 0x00,0x00, 0x00,0x06,
                      0x11, 0x03, 0x00,0x06, 0x00,0x01 };
    uint8_t out[MB_TCP_RESP_MAX];
    uint16_t out_len = 0u;
    uint8_t  fc = 0u;

    bool resp = mb_tcp_build_response(&mb, req, (uint16_t)sizeof req, out, &out_len, &fc);
    CHECK(resp == true);
    CHECK(out[6] == 0x11u);                /* echoes REQUEST unit, not device_addr */
}

int main(void)
{
    test_fc03_read_one();
    test_fc06_write();
    test_rejects();
    test_unit_echo();
    if (g_fail == 0) { printf("app_modbus_tcp_frame: all checks PASSED\n"); return 0; }
    printf("app_modbus_tcp_frame: %d FAILED\n", g_fail);
    return 1;
}
