/* fw/src/app_lcd_render.c — change_lcd_page() render port (samd20 main.c:2942-3173).
 *
 * Per-page VP pre-fill then page switch. Wire calls mapped per spec §2.1:
 *   send_lcd_data_var  -> dgus_write_u16
 *   send_lcd_data_word -> dgus_write_u32
 *   send_lcd_byte_array-> dgus_write_bytes
 *   send_lcd_int_array -> dgus_write_u16_array
 *   set_lcd_page       -> dgus_set_page
 *
 * State split (vs samd20 globals):
 *   config / limits  -> app_lcd_cfg()   (app_config_t)
 *   transient/shadow -> app_lcd_state() (lcd_app_state_t)
 *   lcd_temp_buf     -> local scratch buffer (de-globalized)
 *
 * The samd20 I2C_POT write on setup-page-1 entry is routed through
 * app_lcd_hook_set_pot() (no real I2C this stage — U4 identity is open Q F2).
 */
#include <stdint.h>
#include "app_lcd.h"
#include "app_config.h"
#include "dgus_lcd.h"
#include "sys_tick.h"

/* run_mode codes (samd20 main.c:519-520). Port app_config_t.run_mode uses the
 * same encoding (0=delay 1=trigger), so literals match verbatim. */
#define MODE_DELAY    0
#define MODE_TRIGGER  1

/* DISP_VERSION payload — 20 bytes, NO trailing NUL (sent raw via dgus_write_bytes).
 * samd20 had its own VERSION_MSG ("V2.9.1_250629   "); STM32 port string below. */
static const uint8_t VERSION_MSG[20] = {
    'G','D','S','-','U','S',' ','S','T','M','3','2',' ','v','0','.','1',' ',' ',' '
};

/* Render one panel page, then switch to it. Verbatim port of samd20
 * change_lcd_page (main.c:2942-3173). */
void app_lcd_change_page(uint8_t page)
{
    app_config_t    *cfg   = app_lcd_cfg();
    lcd_app_state_t *state = app_lcd_state();

    uint16_t i;
    uint8_t  n;                 /* formatter return length (samd20 'temp') */
    uint8_t  addr_str[4];       /* conv_addr2str field (samd20 'temp_str') */
    uint8_t  buf[20];           /* line-build scratch (samd20 global lcd_temp_buf) */
    char     ipbuf[16];         /* ip_to_string scratch */

    /* --- unconditional top (main.c:2947-2955) --- */
    dgus_write_u16(DISP_ENERGY_EN, cfg->energy_ctrl ? 1u : 0u);
    dgus_write_u16(DISP_MULTI_EN,  cfg->multi_ctrl  ? 1u : 0u);

    if (page == LCD_RUN_STD) {
        dgus_write_u16(LV_DM_DELAY,    cfg->limit_delay_time1);
        dgus_write_u16(DISP_RUN_MODE,  cfg->run_mode);
        dgus_write_u16(DISP_SAFTY,     cfg->f_safty);
        dgus_write_u16(LV_LIMIT_OUT_T, cfg->limit_out_time);

        if (cfg->run_mode == MODE_DELAY) {
            buf[0] = 'D'; buf[1] = ' '; buf[2] = ':'; buf[3] = ' ';
            n = time2str(cfg->limit_delay_time1, &buf[4]);
            dgus_write_bytes(DISP_STD_DATA1, buf, (uint8_t)(n + 4));

            if (cfg->energy_ctrl) {
                buf[0] = 'E'; buf[1] = ' '; buf[2] = ':'; buf[3] = ' ';
                n = energy2str(cfg->limit_energy, &buf[4]);
                dgus_write_bytes(DISP_STD_DATA2, buf, (uint8_t)(n + 4));
            } else {
                buf[0] = 'W'; buf[1] = ' '; buf[2] = ':'; buf[3] = ' ';
                if (cfg->multi_ctrl)
                    n = time2str((uint16_t)(cfg->limit_mo_time1 + cfg->limit_mo_time2), &buf[4]);
                else
                    n = time2str(cfg->limit_delay_time2, &buf[4]);
                dgus_write_bytes(DISP_STD_DATA2, buf, (uint8_t)(n + 4));
            }

            buf[0] = 'H'; buf[1] = ' '; buf[2] = ':'; buf[3] = ' ';
            n = time2str(cfg->limit_delay_time3, &buf[4]);
            dgus_write_bytes(DISP_STD_DATA3, buf, (uint8_t)(n + 4));
        } else if (cfg->run_mode == MODE_TRIGGER) {
            buf[0] = 'S'; buf[1] = 'E'; buf[2] = 'N'; buf[3] = 'S';
            buf[4] = 'O'; buf[5] = 'R'; buf[6] = ' '; buf[7] = 'O';
            buf[8] = 'F'; buf[9] = 'F'; buf[10] = '\0';
            dgus_write_bytes(DISP_STD_DATA1, buf, 11);

            if (cfg->energy_ctrl) {
                buf[0] = 'E'; buf[1] = ' '; buf[2] = ':'; buf[3] = ' ';
                n = energy2str(cfg->limit_energy, &buf[4]);
                dgus_write_bytes(DISP_STD_DATA2, buf, (uint8_t)(n + 4));
            } else {
                buf[0] = 'W'; buf[1] = ' '; buf[2] = ':'; buf[3] = ' ';
                if (cfg->multi_ctrl)
                    n = time2str((uint16_t)(cfg->limit_mo_time1 + cfg->limit_mo_time2), &buf[4]);
                else
                    n = time2str(cfg->limit_trigger_time2, &buf[4]);
                dgus_write_bytes(DISP_STD_DATA2, buf, (uint8_t)(n + 4));
            }

            buf[0] = 'H'; buf[1] = ' '; buf[2] = ':'; buf[3] = ' ';
            n = time2str(cfg->limit_trigger_time3, &buf[4]);
            dgus_write_bytes(DISP_STD_DATA3, buf, (uint8_t)(n + 4));
        }
    } else if (page == LCD_SETUP_HAND || page == LCD_SETUP_MULTI || page == LCD_SETUP_STD1) {
        dgus_write_bytes(DISP_VERSION, VERSION_MSG, 20);
        dgus_write_u16(LV_MAX_ON_TIME, cfg->limit_on_time);
        dgus_write_u16(LV_OUT_POWER,   cfg->output_power);
        app_lcd_hook_set_pot(cfg->output_power);   /* samd20 inline I2C_POT write -> hook (F2 open) */
        dgus_write_u32(LV_ENERGY_VAL,  cfg->limit_energy);
        dgus_write_u16(LV_ENERGY_EDIT, (uint16_t)cfg->limit_energy);
        dgus_write_u16(DISP_ENERGY_EN, cfg->energy_ctrl ? 1u : 0u);
        dgus_write_u16(LV_LIMIT_OUT_T, cfg->limit_out_time);
        state->temp_parity_idx = cfg->comm_parity_idx;
        state->temp_address    = cfg->comm_address;
        state->temp_speed_idx  = cfg->comm_speed_idx;
        state->temp_cnt_reset  = 0;
        state->temp_comm_mode  = 0xff;
    } else if (page == LCD_SETUP_STD2D) {
        dgus_write_u16(LV_DM_DELAY, cfg->limit_delay_time1);
        dgus_write_u16(LV_DM_WELD,  cfg->limit_delay_time2);
        dgus_write_u16(LV_DM_HOLD,  cfg->limit_delay_time3);
        state->temp_comm_mode = 0xff;
    } else if (page == LCD_SETUP_STD2T) {
        dgus_write_u16(LV_TM_WELD, cfg->limit_trigger_time2);
        dgus_write_u16(LV_TM_HOLD, cfg->limit_trigger_time3);
        state->temp_comm_mode = 0xff;
    } else if (page == LCD_SETUP_STD3 || page == LCD_SETUP_MH2) {
        dgus_write_u16(DISP_MULTI_EN, cfg->multi_ctrl ? 1u : 0u);
        dgus_write_u16(LV_MO_OUT1,  cfg->limit_mo_out1);
        dgus_write_u16(LV_MO_OUT2,  cfg->limit_mo_out2);
        dgus_write_u16(LV_MO_TIME1, cfg->limit_mo_time1);
        dgus_write_u16(LV_MO_TIME2, cfg->limit_mo_time2);
        state->temp_comm_mode = 0xff;
    } else if (page == LCD_SETUP_STDC || page == LCD_SETUP_MHC) {
        dgus_write_u16(COMM_ADDR,   cfg->comm_address);
        dgus_write_u16(COMM_SPEED,  cfg->comm_speed_idx);
        dgus_write_u16(COMM_PARITY, cfg->comm_parity_idx);
        state->temp_parity_idx = cfg->comm_parity_idx;
        state->temp_address    = cfg->comm_address;
        state->temp_speed_idx  = cfg->comm_speed_idx;
        conv_addr2str(cfg->comm_address, addr_str);
        dgus_write_bytes(COMM_ADDR_TXT, addr_str, 4);
        dgus_write_bytes(COMM_SPEED_TXT, comm_speed_txt[cfg->comm_speed_idx], 6);
        dgus_write_bytes(COMM_PARITY_TXT, comm_parity_txt[cfg->comm_parity_idx], 4);
        if (state->temp_comm_mode == 0xff) {
            state->temp_comm_mode = cfg->comm_mode;
            for (i = 0; i < 4; i++) {
                state->temp_ether_ip[i] = cfg->ether_ip[i];
                state->temp_ether_nm[i] = cfg->ether_nm[i];
                state->temp_ether_gw[i] = cfg->ether_gw[i];
            }
            n = ip_to_string(state->temp_ether_ip, ipbuf);
            dgus_write_bytes(COMM_IP_TXT, (const uint8_t *)ipbuf, n);
            n = ip_to_string(state->temp_ether_nm, ipbuf);
            dgus_write_bytes(COMM_NM_TXT, (const uint8_t *)ipbuf, n);
            n = ip_to_string(state->temp_ether_gw, ipbuf);
            dgus_write_bytes(COMM_GW_TXT, (const uint8_t *)ipbuf, n);
            state->ether_current_number   = 0;
            state->ether_current_octet    = 0;
            state->ether_has_input        = false;
            state->ether_ip_input_complete = false;
            state->ether_buffer_pos       = 0;
            state->ether_what_input       = LCD_ETHER_INPUT_NONE;
        }
        if (state->temp_comm_mode > 0)
            dgus_write_u16(DISP_COMM_MODE, 1);
        else
            dgus_write_u16(DISP_COMM_MODE, 0);
    } else if (page == LCD_SETUP_STDE || page == LCD_SETUP_MHE) {
        dgus_write_u16(COMM_ADDR,   cfg->comm_address);
        dgus_write_u16(COMM_SPEED,  cfg->comm_speed_idx);
        dgus_write_u16(COMM_PARITY, cfg->comm_parity_idx);
        state->temp_parity_idx = cfg->comm_parity_idx;
        state->temp_address    = cfg->comm_address;
        state->temp_speed_idx  = cfg->comm_speed_idx;
        conv_addr2str(cfg->comm_address, addr_str);
        dgus_write_bytes(COMM_ADDR_TXT, addr_str, 4);
        dgus_write_bytes(COMM_SPEED_TXT, comm_speed_txt[cfg->comm_speed_idx], 6);
        dgus_write_bytes(COMM_PARITY_TXT, comm_parity_txt[cfg->comm_parity_idx], 4);
        if (state->temp_comm_mode == 0xff) {
            state->temp_comm_mode = cfg->comm_mode;
            for (i = 0; i < 4; i++) {
                state->temp_ether_ip[i] = cfg->ether_ip[i];
                state->temp_ether_nm[i] = cfg->ether_nm[i];
                state->temp_ether_gw[i] = cfg->ether_gw[i];
            }
            n = ip_to_string(state->temp_ether_ip, ipbuf);
            dgus_write_bytes(COMM_IP_TXT, (const uint8_t *)ipbuf, n);
            n = ip_to_string(state->temp_ether_nm, ipbuf);
            dgus_write_bytes(COMM_NM_TXT, (const uint8_t *)ipbuf, n);
            n = ip_to_string(state->temp_ether_gw, ipbuf);
            dgus_write_bytes(COMM_GW_TXT, (const uint8_t *)ipbuf, n);
            state->ether_current_number   = 0;
            state->ether_current_octet    = 0;
            state->ether_has_input        = false;
            state->ether_ip_input_complete = false;
            state->ether_buffer_pos       = 0;
            state->ether_what_input       = LCD_ETHER_INPUT_NONE;
        }
        if (state->temp_comm_mode == 0)
            dgus_write_u16(DISP_COMM_MODE, 0);
        else {
            dgus_write_u16(DISP_COMM_MODE, 1);
            dgus_write_u16(DISP_EN_DHCP, (uint16_t)(state->temp_comm_mode - 1));
        }
    }

    dgus_set_page(page);
    state->last_set_page_ms = sys_tick_get_ms();   /* SYS_PIC_NOW loop guard (spec §10) */
}

/* Panel-var seed (samd20 lcd_var_init, main.c:2592-2603): initial key/model
 * widget state. Called at boot and on SYS_PIC_NOW==0 re-init. */
void app_lcd_var_init(void)
{
    uint16_t seed[2];

    seed[0] = 0x0003;
    seed[1] = 0x0004;
    dgus_write_u16_array(KEY_MULTI + 1, seed, 2);

    seed[0] = 0x0000;
    seed[1] = 0x0002;
    dgus_write_u16_array(SETUP_MODEL + 1, seed, 2);
}
