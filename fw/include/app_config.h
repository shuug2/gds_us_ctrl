/* fw/include/app_config.h — controller configuration loaded from FRAM (samd20 var_init port). */
#pragma once
#include <stdint.h>
#include <stdbool.h>

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

/* Read config from FRAM; if INIT_FLAG != 0xAA, write full factory defaults first. */
void app_config_load(app_config_t *cfg);

/* Write the full samd20 default map to FRAM and fill cfg with the same values. */
void app_config_factory_write(app_config_t *cfg);

/* Commit the full live config map to FRAM (DATA_SAVE). FRAM has no write-cycle
 * cost, so the whole map is written for consistency. EN_ENERGY/EN_MULTI and the
 * ether arrays reflect the live cfg fields (unlike factory defaults). */
void app_config_save_all(const app_config_t *cfg);
