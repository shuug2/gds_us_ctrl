/* fw/src/app_lcd.c — LCD app: init_lcd_mode port + subsystem state owner + stub hooks. */
#include "app_lcd.h"
#include "dgus_lcd.h"
#include "sys_tick.h"
#include "mon.h"
#include "app_reg.h"

/*--- Stage LCD subsystem state owner + control/HW stub hooks ---
 * Single definition of the transient state; render/input/disp layers (Tasks 5-9)
 * reach it via app_lcd_state(). Hooks log only — Stage C/D add real bodies. */
static lcd_app_state_t g_lcd;
static app_config_t    g_cfg;   /* live config owner (loaded at boot, edited by input, saved to FRAM) */

lcd_app_state_t *app_lcd_state(void) { return &g_lcd; }
app_config_t    *app_lcd_cfg(void)   { return &g_cfg; }

const lcd_measure_t *app_lcd_measure(void)
{
    /* Stage D slice 1: live regulation values (amp + scaled level). Cycle/freq/
     * energy/status stay 0 until slice 2 (honest "regulating, no cycle yet"). */
    return app_reg_measure();
}

void app_lcd_hook_set_pot(uint8_t output_power)
{
    /* F1: DAC byte = (power-50)*255/100 → 50%→0, 100%→127. No I2C write
     * (U4 I2C_POT identity = open question F2). */
    uint8_t dac = (uint8_t)(((int)output_power - 50) * 255 / 100);
    mon_printf("[lcd-hook] set_pot power=%u dac=%u\r\n",
               (unsigned)output_power, (unsigned)dac);
}

void app_lcd_hook_us_command(us_cmd_t cmd)
{
    mon_printf("[lcd-hook] us_command=%u\r\n", (unsigned)cmd);
    app_reg_command(cmd, (uint8_t)US_TOUCH);   /* panel keys = touch source */
}

void app_lcd_hook_comm_reconfigure(uint8_t speed_idx, uint8_t parity_idx, uint8_t address)
{
    mon_printf("[lcd-hook] comm speed=%u parity=%u addr=%u\r\n",
               (unsigned)speed_idx, (unsigned)parity_idx, (unsigned)address);
}

void app_lcd_hook_ether_apply(uint8_t mode, const uint8_t ip[4], const uint8_t nm[4], const uint8_t gw[4])
{
    mon_printf("[lcd-hook] ether mode=%u ip=%u.%u.%u.%u nm=%u.%u.%u.%u gw=%u.%u.%u.%u\r\n",
               (unsigned)mode,
               (unsigned)ip[0], (unsigned)ip[1], (unsigned)ip[2], (unsigned)ip[3],
               (unsigned)nm[0], (unsigned)nm[1], (unsigned)nm[2], (unsigned)nm[3],
               (unsigned)gw[0], (unsigned)gw[1], (unsigned)gw[2], (unsigned)gw[3]);
}

void app_lcd_hook_horn(bool down)
{
    mon_printf("[lcd-hook] horn down=%u\r\n", (unsigned)down);
}

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
    uint8_t          run_page = app_lcd_run_page(cfg);
    lcd_app_state_t *state    = app_lcd_state();

    app_lcd_send_model_str(cfg->model_freq, cfg->model_type);

    /* samd20 init_lcd_mode: lcd_status + sys_mode from model_type (main.c:3181-3189) */
    state->lcd_status = run_page;
    state->sys_mode   = cfg->model_type;

    /* Arm the comm/ether shadow-load sentinel (fix A). The struct documents
     * temp_comm_mode==0xFF as "not loaded yet" (app_lcd.h:86), but zero-init
     * leaves it 0(=serial) at boot, so the first comm-page entry skips the
     * seed-from-cfg gate (render.c:143/178) and the display shows stale state.
     * samd20 relied on a setup-page entry to set 0xFF before any comm page;
     * setting it here at boot (and on SYS_PIC_NOW re-init) makes the lifecycle
     * coherent — consistent with the cancel-path re-arm (input.c:598). */
    state->temp_comm_mode = 0xFFu;

    /* output-bar thresholds from model_freq (main.c:3191-3211, verbatim) */
    if (cfg->model_freq == 0) {            /* 15 kHz */
        state->ref_lv_1 = 50;  state->ref_lv_2 = 100;
        state->ref_lv_10 = 1000; state->ref_lv_20 = 2000;
    } else if (cfg->model_freq == 1) {     /* 20 kHz */
        state->ref_lv_1 = 50;  state->ref_lv_2 = 100;
        state->ref_lv_10 = 600;  state->ref_lv_20 = 1200;
    } else {                               /* >=30 kHz */
        state->ref_lv_1 = 50;  state->ref_lv_2 = 100;
        state->ref_lv_10 = 400;  state->ref_lv_20 = 700;
    }

    /* init_lcd_mode VP pre-fill (main.c:3216-3228) — change_lcd_page does NOT
     * cover these, so they stay here (preserved verbatim). */
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

    /* samd20 init_lcd_mode tail: change_lcd_page(lcd_status) (main.c:3230).
     * For RUN_HAND/RUN_MULTI (page 3) this writes only DISP_ENERGY_EN/MULTI_EN
     * + set_page — byte-identical to the Stage-B inline trio it replaces. */
    app_lcd_change_page(run_page);
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

void app_lcd_tick(void)
{
    /* Advance the display step machine on a 4 ms cadence (spec §11) — samd20's
     * 10-step job_state on odd ticks of a 2 ms timer ⇒ ~4 ms/step. One VP-group
     * per call; the machine wraps 0..9 internally. */
    static uint32_t prev_ms = 0;
    uint32_t now = sys_tick_get_ms();

    if ((uint32_t)(now - prev_ms) >= 4) {
        prev_ms = now;
        app_lcd_disp_step();
    }
}
