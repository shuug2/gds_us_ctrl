/* fw/src/app_config.c — FRAM-backed config: factory defaults (full samd20 map) + load. */
#include "app_config.h"
#include "fram.h"

void app_config_factory_write(app_config_t *cfg)
{
    /* defaults — samd20 factory_init (ref/samd20/main.c:753-828) */
    cfg->model_freq          = 0;
    cfg->model_type          = 0;     /* hand */
    cfg->f_safty             = 0;
    cfg->run_mode            = 0;     /* MODE_DELAY */
    cfg->limit_delay_time1   = 50;
    cfg->limit_delay_time2   = 50;
    cfg->limit_delay_time3   = 50;
    cfg->limit_trigger_time2 = 50;
    cfg->limit_trigger_time3 = 50;
    cfg->limit_on_time       = 50;
    cfg->limit_out_time      = 10;
    cfg->limit_mo_out1       = 25;
    cfg->limit_mo_out2       = 50;
    cfg->limit_mo_time1      = 25;
    cfg->limit_mo_time2      = 50;
    cfg->output_power        = 50;
    cfg->work_cnt            = 0;
    cfg->limit_energy        = 100000;
    cfg->energy_ctrl         = false;  /* port fix: samd20 leaves implicit (bss-zero) */
    cfg->multi_ctrl          = false;  /* port fix: samd20 leaves implicit (bss-zero) */
    cfg->cal_val             = 0;
    cfg->freq_cal_val        = 0;
    cfg->comm_address        = 0;
    cfg->comm_speed_idx      = 0;
    cfg->comm_parity_idx     = 0;
    cfg->comm_mode           = 0;      /* COMM_SERIAL */
    for (int i = 0; i < 4; i++) {
        cfg->ether_ip[i] = 0; cfg->ether_nm[i] = 0; cfg->ether_gw[i] = 0;
    }

    /* persist full map (defaults leave EN flags + ether zero, so save_all matches) */
    app_config_save_all(cfg);
}

void app_config_save_all(const app_config_t *cfg)
{
    /* full samd20 map (decision beta). FRAM = no write-cycle cost → write all. */
    fram_write_u32 (FRAM_ADDR_WORK_CNT,     cfg->work_cnt);
    fram_write_u16 (FRAM_ADDR_DELAY1,       cfg->limit_delay_time1);
    fram_write_u16 (FRAM_ADDR_DELAY2,       cfg->limit_delay_time2);
    fram_write_u16 (FRAM_ADDR_DELAY3,       cfg->limit_delay_time3);
    fram_write_u16 (FRAM_ADDR_TRIGGER2,     cfg->limit_trigger_time2);
    fram_write_u16 (FRAM_ADDR_TRIGGER3,     cfg->limit_trigger_time3);
    fram_write_byte(FRAM_ADDR_SAFTY,        cfg->f_safty);
    fram_write_byte(FRAM_ADDR_RUN_MODE,     cfg->run_mode);
    fram_write_byte(FRAM_ADDR_MODEL_FREQ,   cfg->model_freq);
    fram_write_byte(FRAM_ADDR_MODEL_TYPE,   cfg->model_type);
    fram_write_byte(FRAM_ADDR_OUT_POWER,    cfg->output_power);
    fram_write_u16 (FRAM_ADDR_ON_TIME,      cfg->limit_on_time);
    fram_write_byte(FRAM_ADDR_EN_ENERGY,    cfg->energy_ctrl ? 1u : 0u);
    fram_write_byte(FRAM_ADDR_EN_MULTI,     cfg->multi_ctrl  ? 1u : 0u);
    fram_write_u32 (FRAM_ADDR_ENERGY,       cfg->limit_energy);
    fram_write_u16 (FRAM_ADDR_MULTI_T1,     cfg->limit_mo_time1);
    fram_write_u16 (FRAM_ADDR_MULTI_T2,     cfg->limit_mo_time2);
    fram_write_u16 (FRAM_ADDR_MULTI_O1,     cfg->limit_mo_out1);
    fram_write_u16 (FRAM_ADDR_MULTI_O2,     cfg->limit_mo_out2);
    fram_write_byte(FRAM_ADDR_TIMEOVER,     (uint8_t)cfg->limit_out_time);
    fram_write_byte(FRAM_ADDR_COMM_ADDR,    cfg->comm_address);
    fram_write_byte(FRAM_ADDR_COMM_SPEED,   cfg->comm_speed_idx);
    fram_write_byte(FRAM_ADDR_COMM_PARITY,  cfg->comm_parity_idx);
    fram_write_u16 (FRAM_ADDR_CAL_VAL,      (uint16_t)cfg->cal_val);
    fram_write_u16 (FRAM_ADDR_FREQ_CAL_VAL, (uint16_t)cfg->freq_cal_val);
    fram_write_byte(FRAM_ADDR_COMM_MODE,    cfg->comm_mode);
    /* ether arrays byte-by-byte to mirror app_config_load's byte-wise read */
    for (int i = 0; i < 4; i++) {
        fram_write_byte((uint8_t)(FRAM_ADDR_ETHER_IP + i), cfg->ether_ip[i]);
        fram_write_byte((uint8_t)(FRAM_ADDR_ETHER_NM + i), cfg->ether_nm[i]);
        fram_write_byte((uint8_t)(FRAM_ADDR_ETHER_GW + i), cfg->ether_gw[i]);
    }
}

void app_config_load(app_config_t *cfg)
{
    if (fram_read_byte(FRAM_ADDR_INIT_FLAG) != FRAM_INIT_FLAG_MAGIC) {
        fram_write_byte(FRAM_ADDR_INIT_FLAG, FRAM_INIT_FLAG_MAGIC);
        app_config_factory_write(cfg);
        return;
    }

    cfg->work_cnt            = fram_read_u32 (FRAM_ADDR_WORK_CNT);
    cfg->limit_delay_time1   = fram_read_u16 (FRAM_ADDR_DELAY1);
    cfg->limit_delay_time2   = fram_read_u16 (FRAM_ADDR_DELAY2);
    cfg->limit_delay_time3   = fram_read_u16 (FRAM_ADDR_DELAY3);
    cfg->limit_trigger_time2 = fram_read_u16 (FRAM_ADDR_TRIGGER2);
    cfg->limit_trigger_time3 = fram_read_u16 (FRAM_ADDR_TRIGGER3);
    cfg->f_safty             = fram_read_byte(FRAM_ADDR_SAFTY);
    cfg->run_mode            = fram_read_byte(FRAM_ADDR_RUN_MODE);
    cfg->model_freq          = fram_read_byte(FRAM_ADDR_MODEL_FREQ);
    cfg->model_type          = fram_read_byte(FRAM_ADDR_MODEL_TYPE);
    cfg->output_power        = fram_read_byte(FRAM_ADDR_OUT_POWER);
    cfg->limit_on_time       = fram_read_u16 (FRAM_ADDR_ON_TIME);
    cfg->energy_ctrl         = (fram_read_byte(FRAM_ADDR_EN_ENERGY) == 1);
    cfg->limit_energy        = fram_read_u32 (FRAM_ADDR_ENERGY);
    cfg->multi_ctrl          = (fram_read_byte(FRAM_ADDR_EN_MULTI) == 1);
    if (cfg->limit_energy > 100000) {                 /* samd20 clamp (main.c:923) */
        cfg->limit_energy = 100000;
        fram_write_u32(FRAM_ADDR_ENERGY, cfg->limit_energy);
    }
    cfg->limit_mo_time1      = fram_read_u16 (FRAM_ADDR_MULTI_T1);
    cfg->limit_mo_time2      = fram_read_u16 (FRAM_ADDR_MULTI_T2);
    cfg->limit_mo_out1       = fram_read_u16 (FRAM_ADDR_MULTI_O1);
    cfg->limit_mo_out2       = fram_read_u16 (FRAM_ADDR_MULTI_O2);
    cfg->limit_out_time      = fram_read_byte(FRAM_ADDR_TIMEOVER);
    cfg->comm_address        = fram_read_byte(FRAM_ADDR_COMM_ADDR);
    cfg->comm_speed_idx      = fram_read_byte(FRAM_ADDR_COMM_SPEED);
    cfg->comm_parity_idx     = fram_read_byte(FRAM_ADDR_COMM_PARITY);
    cfg->cal_val             = (int16_t)fram_read_u16(FRAM_ADDR_CAL_VAL);
    cfg->freq_cal_val        = (int16_t)fram_read_u16(FRAM_ADDR_FREQ_CAL_VAL);
    cfg->comm_mode           = fram_read_byte(FRAM_ADDR_COMM_MODE);
    for (int i = 0; i < 4; i++) {
        cfg->ether_ip[i] = fram_read_byte((uint8_t)(FRAM_ADDR_ETHER_IP + i));
        cfg->ether_nm[i] = fram_read_byte((uint8_t)(FRAM_ADDR_ETHER_NM + i));
        cfg->ether_gw[i] = fram_read_byte((uint8_t)(FRAM_ADDR_ETHER_GW + i));
    }
}
