/* fw/test/test_app_config.c — host tests for app_config_load fallback semantics
 * (감사 H3). app_config.c compiles HAL-free; fram is mock_fram.c (link 치환).
 * Scenarios = spec §6.1 (2026-07-02-fram-i2c-robustness-design.md). */
#include <stdio.h>
#include <stdint.h>
#include "app_config.h"
#include "fram.h"
#include "mock_fram.h"
#include "cfg_clamp.h"

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

/* 저장값이 기본값과 전부 구분되도록 만든 config를 mock FRAM에 기록. */
static void store_distinct_config(void)
{
    app_config_t c;
    app_config_factory_defaults(&c);
    c.output_power   = 77u;      /* default 50 */
    c.limit_on_time  = 123u;     /* default 50 */
    c.limit_energy   = 54321u;   /* default 100000 */
    c.work_cnt       = 999u;     /* default 0 */
    c.limit_mo_out1  = 61u;      /* default 25 */
    c.comm_address   = 7u;       /* default 0 */
    c.energy_ctrl    = true;     /* default false */
    c.ether_ip[0] = 192u; c.ether_ip[1] = 168u; c.ether_ip[2] = 1u; c.ether_ip[3] = 10u;

    mock_fram_reset();
    mock_fram_poke(FRAM_ADDR_INIT_FLAG, FRAM_INIT_FLAG_MAGIC);
    app_config_save_all(&c);
    mock_fram_clear_write_count();
}

/* #1 전 필드 성공: 반환 0, 저장값 그대로, write 0건 */
static void test_clean_load(void)
{
    store_distinct_config();
    app_config_t cfg;
    uint8_t fail = app_config_load(&cfg);
    CHECK_EQ(fail, 0);
    CHECK_EQ(cfg.output_power, 77);
    CHECK_EQ(cfg.limit_on_time, 123);
    CHECK_EQ(cfg.limit_energy, 54321);
    CHECK_EQ(cfg.work_cnt, 999);
    CHECK_EQ(cfg.limit_mo_out1, 61);
    CHECK_EQ(cfg.comm_address, 7);
    CHECK_EQ(cfg.energy_ctrl, 1);
    CHECK_EQ(cfg.ether_ip[3], 10);
    CHECK_EQ(mock_fram_write_count(), 0);
}

/* #2 magic ≠ 0xAA (read 성공) = 진짜 빈 FRAM: factory_write 발생, 반환 0, 기본값 */
static void test_blank_fram_factory(void)
{
    mock_fram_reset();                      /* INIT_FLAG = 0 ≠ 0xAA, 읽기는 성공 */
    app_config_t cfg;
    uint8_t fail = app_config_load(&cfg);
    CHECK_EQ(fail, 0);
    CHECK_EQ(cfg.output_power, 50);         /* factory default */
    CHECK_EQ(cfg.limit_energy, 100000);
    CHECK_EQ(mock_fram_peek(FRAM_ADDR_INIT_FLAG), FRAM_INIT_FLAG_MAGIC);
    CHECK_EQ(mock_fram_write_count() > 0u, 1);   /* save_all 발생 */
}

/* #3 INIT_FLAG read 실패: FRAM 미변경(write 0건) 입증 — 데이터손실 경로 차단 핵심 */
static void test_init_flag_read_fail_no_wipe(void)
{
    store_distinct_config();
    mock_fram_fail_read(FRAM_ADDR_INIT_FLAG, 1u);
    app_config_t cfg;
    uint8_t fail = app_config_load(&cfg);
    CHECK_EQ(fail, 0xFF);
    CHECK_EQ(mock_fram_write_count(), 0);   /* factory-write 금지 */
    CHECK_EQ(cfg.output_power, 50);         /* 전 필드 기본값 */
    CHECK_EQ(cfg.limit_on_time, 50);
    CHECK_EQ(mock_fram_peek(FRAM_ADDR_OUT_POWER), 77);  /* FRAM 원본 보존 */
}

/* #4 부분 실패: ON_TIME(u16)+OUT_POWER(byte)만 fail → 그 2필드만 기본값, 반환 2 */
static void test_partial_fail_field_fallback(void)
{
    store_distinct_config();
    mock_fram_fail_read(FRAM_ADDR_ON_TIME, 2u);
    mock_fram_fail_read(FRAM_ADDR_OUT_POWER, 1u);
    app_config_t cfg;
    uint8_t fail = app_config_load(&cfg);
    CHECK_EQ(fail, 2);
    CHECK_EQ(cfg.limit_on_time, 50);        /* fallback = factory default */
    CHECK_EQ(cfg.output_power, 50);         /* fallback */
    CHECK_EQ(cfg.limit_energy, 54321);      /* 나머지는 저장값 */
    CHECK_EQ(cfg.work_cnt, 999);
    CHECK_EQ(cfg.energy_ctrl, 1);
    CHECK_EQ(mock_fram_write_count(), 0);
}

/* #5 limit_energy > 100000 (read 성공): 클램프 + write-back 1건 (현행 무회귀) */
static void test_energy_clamp_writeback(void)
{
    store_distinct_config();
    /* ENERGY(u32 @22)에 200000 직접 기록 */
    mock_fram_poke(FRAM_ADDR_ENERGY,      (uint8_t)(200000u >> 24));
    mock_fram_poke((uint8_t)(FRAM_ADDR_ENERGY + 1u), (uint8_t)(200000u >> 16));
    mock_fram_poke((uint8_t)(FRAM_ADDR_ENERGY + 2u), (uint8_t)(200000u >> 8));
    mock_fram_poke((uint8_t)(FRAM_ADDR_ENERGY + 3u), (uint8_t)200000u);
    app_config_t cfg;
    uint8_t fail = app_config_load(&cfg);
    CHECK_EQ(fail, 0);
    CHECK_EQ(cfg.limit_energy, 100000);
    CHECK_EQ(mock_fram_write_count(), 1);   /* write-back 1건 */
}

/* #6 limit_energy read 실패: 기본값 100000, write-back 0건, 반환 1 */
static void test_energy_read_fail_no_writeback(void)
{
    store_distinct_config();
    mock_fram_fail_read(FRAM_ADDR_ENERGY, 4u);
    app_config_t cfg;
    uint8_t fail = app_config_load(&cfg);
    CHECK_EQ(fail, 1);
    CHECK_EQ(cfg.limit_energy, 100000);     /* default */
    CHECK_EQ(mock_fram_write_count(), 0);
}

/* M4(감사 D2): config-validation 클램프 헬퍼 — Modbus apply_writes 범위와 동일. */
static void test_cfg_clamp_helpers(void)
{
    CHECK_EQ(cfg_clamp_max(501u, 500u), 500u);
    CHECK_EQ(cfg_clamp_max(500u, 500u), 500u);
    CHECK_EQ(cfg_clamp_max(0u, 500u), 0u);
    CHECK_EQ(cfg_clamp_power(101u), 100u);
    CHECK_EQ(cfg_clamp_power(49u), 50u);    /* LOW-1: <50 진폭 언더플로 차단 */
    CHECK_EQ(cfg_clamp_power(0u), 50u);
    CHECK_EQ(cfg_clamp_power(75u), 75u);
}

int main(void)
{
    test_clean_load();
    test_blank_fram_factory();
    test_init_flag_read_fail_no_wipe();
    test_partial_fail_field_fallback();
    test_energy_clamp_writeback();
    test_energy_read_fail_no_writeback();
    test_cfg_clamp_helpers();
    if (failures) { printf("test_app_config: %d FAILED\n", failures); return 1; }
    printf("test_app_config: all passed\n");
    return 0;
}
