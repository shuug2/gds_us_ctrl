/* fw/include/app_config.h — controller configuration loaded from FRAM (samd20 var_init port). */
#pragma once
#include <stdint.h>
#include <stdbool.h>

/* comm 렌더 테이블 인덱스 상한 (감사 M3 클램프): app_lcd_str.c
 * comm_speed_txt[6][6] / comm_parity_txt[3][4] 크기와 동기 — 테이블 확장 시 함께. */
#define CFG_COMM_SPEED_IDX_MAX   5u
#define CFG_COMM_PARITY_IDX_MAX  2u
/* 원격 커밋 검증용 슬레이브 주소 범위 (F-A §5.3). LCD 는 0(NONE)을 허용하지만
 * 원격 커밋에서 0 은 RTU 자체를 죽여 복구 불능이 되므로 막는다 — "LCD 에 없는
 * 규칙 발명"이 아니라 원격 경로에서만 회복 불가인 값이라서다. */
#define CFG_COMM_ADDR_MIN        1u
#define CFG_COMM_ADDR_MAX      247u

typedef struct {
    uint8_t  model_freq;        /* 0..5  (15/20/30/35/40/50 K) */
    uint8_t  model_type;        /* 0=hand 1=multi 2=std */
    uint8_t  f_safty;
    uint8_t  run_mode;          /* 0=delay 1=trigger */
    uint16_t limit_delay_time1, limit_delay_time2, limit_delay_time3;
    uint16_t limit_trigger_time2, limit_trigger_time3;
    uint16_t limit_on_time;
    uint16_t limit_out_time;    /* stored 1 byte (ADDR_TIMEOVER) */
    uint16_t limit_mo_out1, limit_mo_out2, limit_mo_time1, limit_mo_time2;
    uint8_t  output_power;
    uint32_t work_cnt;
    uint32_t limit_energy;
    bool     energy_ctrl;
    bool     multi_ctrl;
    int16_t  cal_val, freq_cal_val;
    uint8_t  comm_address, comm_speed_idx, comm_parity_idx, comm_mode;
    uint8_t  ether_ip[4], ether_nm[4], ether_gw[4];
} app_config_t;

/* Load config from FRAM. Factory defaults are pre-applied; each field is only
 * overwritten by a successful read (감사 H3 폴백).
 * Returns: 0 = clean / 1..38 = failed-read count (those fields run on
 * defaults) / 0xFF = INIT_FLAG itself unreadable → FRAM left UNTOUCHED
 * (no factory-write: a transient bus fault must not wipe a good FRAM),
 * all fields on defaults. */
uint8_t app_config_load(app_config_t *cfg);

/* Fill *cfg with factory defaults. RAM only — never touches FRAM. */
void app_config_factory_defaults(app_config_t *cfg);

/* factory defaults + persist full map (save_all). */
void app_config_factory_write(app_config_t *cfg);

/* Commit the full live config map to FRAM (DATA_SAVE). FRAM has no write-cycle
 * cost, so the whole map is written for consistency. EN_ENERGY/EN_MULTI and the
 * ether arrays reflect the live cfg fields (unlike factory defaults). */
void app_config_save_all(const app_config_t *cfg);
