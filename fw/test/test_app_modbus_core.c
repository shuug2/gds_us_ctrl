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

/* FC 06 — store + echo (echo bytes == request bytes for a valid write). */
static void test_write_reg(void) {
    mb_core_t mb;
    uint8_t req[8], resp[MB_RESP_MAX];
    uint8_t fc = 0xEE;
    mb_core_init(&mb, 5);

    mk_req(req, 5, 0x06, MB_REG_OUT_POWER, 80);
    uint8_t n = mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc);
    CHECK_EQ(n, 8);
    CHECK_EQ(fc, 0x06);
    CHECK_EQ(mb.holding[MB_REG_OUT_POWER], 80);
    CHECK_EQ(memcmp(resp, req, 8), 0);      /* FC06 echo == request */

    /* port safety fix: out-of-range write = silence + no state change
     * (samd20 wrote holdingReg[addr] UNBOUNDED = arbitrary memory write). */
    mk_req(req, 5, 0x06, 50, 1);
    CHECK_EQ(mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc), 0);
    CHECK_EQ(fc, 0);

    /* TCP mode: write path also skips addr/CRC filtering (PDU only) */
    mk_req(req, 0xEE, 0x06, MB_REG_ON_TIME, 1500);
    req[6] = 0; req[7] = 0;                 /* garbage CRC, ignored in TCP */
    CHECK_EQ(mb_core_decode(&mb, req, 6, MB_MODE_TCP, resp, &fc), 8);
    CHECK_EQ(fc, 0x06);
    CHECK_EQ(mb.holding[MB_REG_ON_TIME], 1500);
}

/* FC 05 — coil set/clear; port fix: proper 0x05 echo, 8 bytes (samd20 answered
 * fc=0x02 and 9 bytes — copy-paste bug). */
static void test_write_coil(void) {
    mb_core_t mb;
    uint8_t req[8], resp[MB_RESP_MAX];
    uint8_t fc = 0xEE;
    mb_core_init(&mb, 5);

    mk_req(req, 5, 0x05, 3, 0xFF00);
    uint8_t n = mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc);
    CHECK_EQ(n, 8);
    CHECK_EQ(fc, 0x05);
    CHECK_EQ(mb.coils[3], 0xFF);
    CHECK_EQ(resp[1], 0x05);
    CHECK_EQ(memcmp(resp, req, 8), 0);      /* with the fixed echo, == request */

    mk_req(req, 5, 0x05, 3, 0x0000);
    n = mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc);
    CHECK_EQ(n, 8);
    CHECK_EQ(mb.coils[3], 0x00);

    mk_req(req, 5, 0x05, 50, 0xFF00);       /* out of range */
    CHECK_EQ(mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc), 0);
}

/* FC 01/02 — bit packing: partial byte, multi-byte, and the full-byte case
 * (count%8==0) that samd20 left empty (port fix). */
static void test_read_coils(void) {
    mb_core_t mb;
    uint8_t req[8], resp[MB_RESP_MAX];
    uint8_t fc = 0xEE;
    mb_core_init(&mb, 5);
    mb.coils[0] = 1; mb.coils[2] = 1; mb.coils[9] = 1; mb.coils[15] = 1;

    /* 10 coils from 0: 2 bytes (8 + rem 2). byte0 = 0b00000101, byte1 = 0b10. */
    mk_req(req, 5, 0x01, 0, 10);
    uint8_t n = mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc);
    CHECK_EQ(n, 7);                         /* addr fc cnt b0 b1 crc2 */
    CHECK_EQ(fc, 0x01);
    CHECK_EQ(resp[2], 2);
    CHECK_EQ(resp[3], 0x05);
    CHECK_EQ(resp[4], 0x02);

    /* 16 coils from 0 (count%8==0): samd20 bug left byte1 = 0. Fixed: bit15. */
    mk_req(req, 5, 0x01, 0, 16);
    n = mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc);
    CHECK_EQ(n, 7);
    CHECK_EQ(resp[2], 2);
    CHECK_EQ(resp[3], 0x05);
    CHECK_EQ(resp[4], 0x82);                /* coil9 -> bit1, coil15 -> bit7 */

    /* FC 02 echoes its own code */
    mk_req(req, 5, 0x02, 0, 8);
    n = mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc);
    CHECK_EQ(n, 6);                         /* addr fc cnt b0 crc2 */
    CHECK_EQ(fc, 0x02);
    CHECK_EQ(resp[1], 0x02);
    CHECK_EQ(resp[3], 0x05);

    mk_req(req, 5, 0x01, 45, 8);            /* 45 + 8 > 50: silence */
    CHECK_EQ(mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc), 0);
    mk_req(req, 5, 0x01, 0, 0);             /* zero count: silence */
    CHECK_EQ(mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc), 0);

    /* fence-posts: last valid coil reads fine; one past = silence */
    mk_req(req, 5, 0x01, 49, 1);
    CHECK_EQ(mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc), 6);
    mk_req(req, 5, 0x01, 50, 1);
    CHECK_EQ(mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc), 0);
}

/* STATUS(0x1D) 비트 합성 — B-3 센서 / B-4 horn 관측 추가.
 * 원격기·gds_us_hmi 가 읽는 계약이라 순수 합성기를 host 로 고정한다.
 * 요구사항: docs/superpowers/specs/2026-08-30-remote-parity-requirements.md B-3/B-4 */
static void test_status_bits(void) {
    mb_status_in_t in;

    /* 아무것도 아니면 0 */
    memset(&in, 0, sizeof in);
    CHECK_EQ(mb_status_bits(&in), 0x0000);

    /* 비트 하나씩 — 각 입력이 자기 비트만 세운다 */
    memset(&in, 0, sizeof in); in.running = 1;
    CHECK_EQ(mb_status_bits(&in), MB_STATUS_US);
    memset(&in, 0, sizeof in); in.estop = 1;
    CHECK_EQ(mb_status_bits(&in), MB_STATUS_ESTOP);
    memset(&in, 0, sizeof in); in.ovld = 1;
    CHECK_EQ(mb_status_bits(&in), MB_STATUS_OVLD);
    memset(&in, 0, sizeof in); in.ovtime = 1;
    CHECK_EQ(mb_status_bits(&in), MB_STATUS_OVTIME);
    memset(&in, 0, sizeof in); in.sensor = 1;
    CHECK_EQ(mb_status_bits(&in), MB_STATUS_SENSOR);
    memset(&in, 0, sizeof in); in.horn = 1;
    CHECK_EQ(mb_status_bits(&in), MB_STATUS_HORN);
    memset(&in, 0, sizeof in); in.seek = 1;
    CHECK_EQ(mb_status_bits(&in), MB_STATUS_SEEK);
    memset(&in, 0, sizeof in); in.reset = 1;
    CHECK_EQ(mb_status_bits(&in), MB_STATUS_RESET);

    /* 신규 2비트가 기존 5비트와 겹치지 않는다 (주소 여유 계산의 전제) */
    CHECK_EQ(MB_STATUS_SENSOR & (MB_STATUS_US | MB_STATUS_ESTOP | MB_STATUS_OVLD
                                 | MB_STATUS_OVTIME | MB_STATUS_OUTERR), 0);
    CHECK_EQ(MB_STATUS_HORN   & (MB_STATUS_US | MB_STATUS_ESTOP | MB_STATUS_OVLD
                                 | MB_STATUS_OVTIME | MB_STATUS_OUTERR), 0);
    CHECK_EQ(MB_STATUS_SENSOR & MB_STATUS_HORN, 0);

    /* SEEK/RESET 진행 비트도 기존 전부와 직교 (2026-09-05) */
    CHECK_EQ(MB_STATUS_SEEK  & (MB_STATUS_US | MB_STATUS_ESTOP | MB_STATUS_OVLD
                                | MB_STATUS_OVTIME | MB_STATUS_OUTERR
                                | MB_STATUS_SENSOR | MB_STATUS_HORN), 0);
    CHECK_EQ(MB_STATUS_RESET & (MB_STATUS_US | MB_STATUS_ESTOP | MB_STATUS_OVLD
                                | MB_STATUS_OVTIME | MB_STATUS_OUTERR
                                | MB_STATUS_SENSOR | MB_STATUS_HORN), 0);
    CHECK_EQ(MB_STATUS_SEEK & MB_STATUS_RESET, 0);
    /* uint16 안에 들어간다 — holding[] 이 uint16 이라 bit8 이 상한이 아님을 고정 */
    CHECK_EQ(MB_STATUS_RESET & 0xFFFFu, MB_STATUS_RESET);

    /* 조합 — horn 모드에서 원격 START 가 막힌 채 센서가 눌린 상태.
     * 원격기가 "왜 안 먹는지" 를 읽어내야 하는 바로 그 조합이다. */
    memset(&in, 0, sizeof in);
    in.running = 1; in.estop = 1; in.sensor = 1; in.horn = 1;
    CHECK_EQ(mb_status_bits(&in),
             MB_STATUS_US | MB_STATUS_ESTOP | MB_STATUS_SENSOR | MB_STATUS_HORN);

    /* 0/1 이 아닌 truthy 입력도 정확히 1비트로 정규화된다 (C-2 와 같은 사고 방지) */
    memset(&in, 0, sizeof in); in.horn = 5; in.sensor = 200;
    CHECK_EQ(mb_status_bits(&in), MB_STATUS_SENSOR | MB_STATUS_HORN);
    memset(&in, 0, sizeof in); in.seek = 9; in.reset = 3;
    CHECK_EQ(mb_status_bits(&in), MB_STATUS_SEEK | MB_STATUS_RESET);

    /* 체인 중 과부하 자동 RESET 조합 — 원격기가 "왜 지금 명령이 안 먹나" 를
     * 읽어내야 하는 자리다(과부하 해제 직후 1.2s 자동 RESET→SEEK). */
    memset(&in, 0, sizeof in); in.ovld = 1; in.reset = 1;
    CHECK_EQ(mb_status_bits(&in), MB_STATUS_OVLD | MB_STATUS_RESET);
}

/* work counter 리셋 술어 — 65536 배수에서의 가짜 리셋 회귀를 고정한다.
 * 이 결함은 벤치로 재현할 수 없다(work_cnt=65536 도달에 65536 사이클 필요,
 * Modbus 로 설정 불가, SWD 쓰기는 규율상 금지) → host 가 유일한 게이트다. */
static void test_work_cnt_reset_req(void) {
    mb_core_t mb;

    /* 마스터가 CNTL 에 0 을 썼지만 카운터가 이미 0 = no-op (레거시·LCD 와 동일) */
    mb_core_init(&mb, 1);
    mb.last_write_addr = MB_REG_WORK_CNTL; mb.holding[MB_REG_WORK_CNTL] = 0u;
    CHECK_EQ(mb_work_cnt_reset_req(&mb, 0u), 0);

    /* 통상 리셋 */
    CHECK_EQ(mb_work_cnt_reset_req(&mb, 5u), 1);

    /* 🔴 회귀 고정: work_cnt = 65536 이면 하위-워드 미러가 holding 에 0 을 싣는다.
     * 이때 마스터가 건드린 칸은 COMM_SPEED(staged 쓰기)다 — 리셋이 아니다.
     * 가드가 없으면 여기서 생산 카운트가 날아가고 staged 쓰기까지 탈락한다. */
    mb_core_init(&mb, 1);
    mb.last_write_addr = MB_REG_COMM_SPEED; mb.holding[MB_REG_WORK_CNTL] = 0u;
    CHECK_EQ(mb_work_cnt_reset_req(&mb, 65536u), 0);

    /* 같은 65536 에서 마스터가 진짜로 CNTL=0 을 쓰면 리셋된다
     * (2026-09-04 승인된 samd20 이탈 — 하위 워드만 봤다면 무시됐을 자리) */
    mb.last_write_addr = MB_REG_WORK_CNTL;
    CHECK_EQ(mb_work_cnt_reset_req(&mb, 65536u), 1);

    /* CNTL 에 0 이 아닌 값을 쓰면 리셋 아님 (레거시 거동) */
    mb.holding[MB_REG_WORK_CNTL] = 7u;
    CHECK_EQ(mb_work_cnt_reset_req(&mb, 5u), 0);
}

int main(void) {
    test_crc16();
    test_core_init();
    test_read_regs();
    test_read_regs_bounds();
    test_filters();
    test_write_reg();
    test_write_coil();
    test_read_coils();
    test_status_bits();
    test_work_cnt_reset_req();
    if (failures) { printf("%d check(s) FAILED\n", failures); return 1; }
    printf("all checks PASSED\n");
    return 0;
}
