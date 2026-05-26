/* fw/src/app_lcd.c — samd20 init_lcd_mode port (scope a: RUN-page bring-up, no string formatting). */
#include "app_lcd.h"
#include "dgus_lcd.h"
#include "sys_tick.h"

uint8_t app_lcd_run_page(const app_config_t *cfg)
{
    if      (cfg->model_type == 0) return LCD_RUN_HAND;    /* 3 */
    else if (cfg->model_type == 1) return LCD_RUN_MULTI;   /* 3 */
    else                           return LCD_RUN_STD;     /* 9 */
}

void app_lcd_send_model_str(uint8_t freq, uint8_t type)
{
    /* GDSONIC build (samd20 main.c:2379-2426). Wire payload = 11 bytes incl. NUL at [10]. */
    uint8_t s[11];
    s[0] = 'G'; s[1] = 'D'; s[2] = 'S'; s[3] = '-';
    switch (freq) {
        case 0:  s[4] = '1'; s[5] = '5'; break;
        case 1:  s[4] = '2'; s[5] = '0'; break;
        case 2:  s[4] = '3'; s[5] = '0'; break;
        case 3:  s[4] = '3'; s[5] = '5'; break;
        case 4:  s[4] = '4'; s[5] = '0'; break;
        case 5:  s[4] = '5'; s[5] = '0'; break;
        default: s[4] = '1'; s[5] = '5'; break;
    }
    switch (type) {
        case 0:  s[6] = 'H'; break;
        case 1:  s[6] = 'M'; break;
        case 2:  s[6] = 'S'; break;
        default: s[6] = 'H'; break;
    }
    s[7] = ' '; s[8] = ' '; s[9] = ' '; s[10] = '\0';
    dgus_write_bytes(MODEL_NAME, s, 11);
}

void app_lcd_init_mode(const app_config_t *cfg)
{
    uint8_t run_page = app_lcd_run_page(cfg);

    app_lcd_send_model_str(cfg->model_freq, cfg->model_type);

    /* init_lcd_mode VP pre-fill (main.c:3216-3228) */
    dgus_write_u16(ICON_RESET, 0);
    dgus_write_u16(ICON_SEEK,  0);
    dgus_write_u16(ICON_RUN,   0);
    dgus_write_u16(LV_DM_DELAY, cfg->limit_delay_time1);
    dgus_write_u16(LV_DM_WELD,  cfg->limit_delay_time2);
    dgus_write_u16(LV_DM_HOLD,  cfg->limit_delay_time3);
    dgus_write_u16(LV_TM_WELD,  cfg->limit_trigger_time2);
    dgus_write_u16(LV_TM_HOLD,  cfg->limit_trigger_time3);
    dgus_write_u32(LV_WORK_CNT, cfg->work_cnt);
    dgus_write_u16(LV_ENERGY_EDIT, (uint16_t)(cfg->limit_energy / 10));
    dgus_write_u16(DISP_HORNDOWN, 0);

    /* change_lcd_page unconditional top (main.c:2947-2955) */
    dgus_write_u16(DISP_ENERGY_EN, cfg->energy_ctrl ? 1 : 0);
    dgus_write_u16(DISP_MULTI_EN,  cfg->multi_ctrl  ? 1 : 0);

    dgus_set_page(run_page);
}

bool app_lcd_ensure_run_page(const app_config_t *cfg)
{
    uint8_t  run_page = app_lcd_run_page(cfg);
    uint16_t now_pg;

    for (uint8_t i = 0; i < DGUS_PAGE_CONFIRM_RETRIES; i++) {
        if (dgus_read_word(SYS_PIC_NOW, &now_pg, DGUS_READ_REPLY_TIMEOUT_MS)
            && (uint8_t)now_pg == run_page) {
            return true;                                /* 패널이 run 페이지 확인 */
        }
        dgus_set_page(run_page);                        /* 미확인 → 재전송 */
        sys_tick_delay_ms(DGUS_PAGE_CONFIRM_SPACING_MS);
    }

    /* 마지막 재전송 후 최종 확인 */
    if (dgus_read_word(SYS_PIC_NOW, &now_pg, DGUS_READ_REPLY_TIMEOUT_MS)) {
        return (uint8_t)now_pg == run_page;
    }
    return false;
}
