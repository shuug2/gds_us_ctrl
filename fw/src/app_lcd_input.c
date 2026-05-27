/* fw/src/app_lcd_input.c — panel touch/key input dispatch (samd20 parse_lcd_comm port).
 *
 * Port source (read-only authoritative): ref/samd20/main.c parse_lcd_comm (3248-4135).
 * Spec: docs/superpowers/specs/2026-05-27-stage-lcd-full-behavior-port-design.md §3,§4,§7.
 *
 * Scope (Task 6): act only on DGUS_CMD_RD (0x83) touch/key reports. Config edits write
 * app_lcd_cfg() directly (samd20 immediate-to-live); transient/shadows write app_lcd_state().
 * DATA_SAVE / comm / ethernet rows are DEFERRED to Task 7 (placeholder no-ops below).
 *
 * Stage D owns us/measure state; this layer only RAISES the command via hooks — it does
 * NOT mutate us_run_status / sig_* / energy accumulators (those live behind the read-only
 * app_lcd_measure() provider and Stage D's FSM).
 */
#include "app_lcd.h"
#include "dgus_lcd.h"
#include "sys_tick.h"
#include "mon.h"

/*--------------------------------------------------------------
 * samd20 define.h / main.c constants (file-local — verbatim values)
 *--------------------------------------------------------------*/
/* Error bitmask (ref/samd20/define.h:77-79) */
#define ERR_OVLD    1u
#define ERR_OVTIME  2u
#define ERR_OUTERR  4u

/* run_mode (ref/samd20/main.c:519-520) */
#define MODE_TRIGGER  1u
#define MODE_DELAY    0u

/* sys_mode = model_type (ref/samd20/main.c:505-507) */
#define SYS_HAND   0u
#define SYS_MULTI  1u
#define SYS_STD    2u

/* comm_mode values (ref/samd20/define.h:85-87) */
#define COMM_SERIAL      0u
#define COMM_ETH_STATIC  1u
#define COMM_ETH_DHCP    2u

/* DATA_SAVE payload: 1 = SAVE, 0 = CANCEL (ref/samd20/main.c:3298,3511). */
#define SAVE_COMMIT  1u

/* LV_ETHER_KEY field-select / edit key codes (ref/samd20/main.c:4081-4112).
 * 'I'/'M'/'G' select IP/NM/GW; digits arrive raw 0..9; 'D' dot, 'B' backspace,
 * 'E' enter (process_ip_char, ref/samd20/main.c:2850). */
#define ETHER_KEY_IP    0x49u   /* 'I' */
#define ETHER_KEY_NM    0x4Du   /* 'M' */
#define ETHER_KEY_GW    0x47u   /* 'G' */
#define ETHER_KEY_DOT   0x44u   /* 'D' */
#define ETHER_KEY_BKSP  0x42u   /* 'B' */
#define ETHER_KEY_ENTER 0x45u   /* 'E' */

/* IP/NM/GW text VP stride: COMM_IP_TXT + 0x10*field (ref/samd20/main.c:4124). */
#define COMM_TXT_STRIDE 0x10u

/* Comm-config string table widths (ref/samd20/main.c:160-161). */
#define COMM_SPEED_TXT_LEN   6u
#define COMM_PARITY_TXT_LEN  4u
#define COMM_ADDR_TXT_LEN    4u
#define COMM_IP_TXT_LEN      16u
#define COMM_SPEED_IDX_MAX   5u   /* comm_speed_txt[6][6] -> 0..5 */
#define COMM_PARITY_IDX_MAX  2u   /* comm_parity_txt[3][4] -> 0..2 */

/* Long-press hold threshold. samd20 KEY_HOLD_TH=200 ticks of the 10ms key timer
 * (ref/samd20/define.h:34) => 2000 ms wall-clock (clean STM32 equivalent). */
#define KEY_HOLD_MS  2000u

/*--------------------------------------------------------------
 * Multi-step branch helpers (keep the switch readable, <800 lines)
 *--------------------------------------------------------------*/

/* Resolve the RUN page id for the active sys_mode (samd20 model_type branch). */
static uint8_t run_page_for_mode(uint8_t sys_mode)
{
    if (sys_mode == SYS_HAND)  return LCD_RUN_HAND;     /* 3 */
    if (sys_mode == SYS_MULTI) return LCD_RUN_MULTI;    /* 3 */
    return LCD_RUN_STD;                                 /* 9 (SYS_STD) */
}

/* Resolve the setup-page-1 id for the active sys_mode (SETUP_PARAM / _MOOHAN). */
static uint8_t setup1_page_for_mode(uint8_t sys_mode)
{
    if (sys_mode == SYS_HAND)  return LCD_SETUP_HAND;   /* 7 */
    if (sys_mode == SYS_MULTI) return LCD_SETUP_MULTI;  /* 5 */
    return LCD_SETUP_STD1;                              /* 10 (SYS_STD) */
}

/* KEY_MULTI (0x1080): 1=RESET / 2=SEEK / 3=RUN(press) / 4=RUN(release).
 * Raise the ultrasonic command hook only (Stage D owns the us/sig/energy FSM).
 * RUN press also writes the DAC. RESET in an OVLD/OUTERR error clears those bits,
 * blanks the icons, and restores the run page (samd20 main.c:3633-3706). */
static void handle_key_multi(uint16_t data16)
{
    lcd_app_state_t *state = app_lcd_state();
    app_config_t    *cfg   = app_lcd_cfg();

    if (data16 == 1) {                                  /* RESET */
        app_lcd_hook_us_command(US_CMD_RESET);
        /* Stage D owns us/measure state; input only raises the command. */
        /* Task simplification of samd20's (sys_status==SYS_ERROR && ...) gate:
         * key clears the OVLD/OUTERR error bits + restores the run page. */
        if (state->error_status & (ERR_OVLD | ERR_OUTERR)) {
            state->error_status &= (uint8_t)~(ERR_OVLD | ERR_OUTERR);
            dgus_write_u16(ICON_OL, 0);
            dgus_write_u16(ICON_OUTERR, 0);
            state->lcd_status = run_page_for_mode(state->sys_mode);
            dgus_set_page(state->lcd_status);           /* samd20 uses set_lcd_page (no rebuild) */
        }
    } else if (data16 == 2) {                           /* SEEK */
        app_lcd_hook_us_command(US_CMD_SEEK);
        /* Stage D owns us/measure state; input only raises the command. */
    } else if (data16 == 3) {                           /* RUN (press) */
        app_lcd_hook_us_command(US_CMD_START);
        app_lcd_hook_set_pot(cfg->output_power);        /* DAC fires on RUN press */
        /* Stage D owns us/measure state; input only raises the command. */
    } else if (data16 == 4) {                           /* RUN (release) */
        app_lcd_hook_us_command(US_CMD_RUN_RELEASE);
        /* Stage D owns us/measure state; input only raises the command. */
    }
}

/* KEY_ERROR_RESET (0x1408): ==1 raises RESET; an OVTIME fault is cleared and the
 * run page restored (samd20 main.c:3707-3732). */
static void handle_key_error_reset(uint16_t data16)
{
    lcd_app_state_t *state = app_lcd_state();

    if (data16 != 1) {
        return;
    }
    app_lcd_hook_us_command(US_CMD_RESET);
    /* Stage D owns us/measure state; input only raises the command. */
    if (state->error_status & ERR_OVTIME) {
        state->error_status = 0;                        /* samd20 zeroes the whole word here */
        state->lcd_status = run_page_for_mode(state->sys_mode);
        dgus_set_page(state->lcd_status);               /* samd20 uses set_lcd_page (no rebuild) */
    }
}

/* SETUP_MODEL / SETUP_PARAM_MOOHAN long-press FSM.
 * press (data==0) records the press start; release (data==2) with >= KEY_HOLD_MS held
 * triggers the action. Returns true if the (long) release fired. */
static bool long_press_released(uint16_t data16)
{
    lcd_app_state_t *state = app_lcd_state();

    if (data16 == 0) {                                  /* press */
        state->key_press_ms = sys_tick_get_ms();
        return false;
    }
    if (data16 == 2) {                                  /* release */
        uint32_t held = (uint32_t)(sys_tick_get_ms() - state->key_press_ms);
        return held >= KEY_HOLD_MS;
    }
    return false;
}

/* SETUP_MODEL (0x1084) release action: enter the model-setup page and echo the
 * model/cal VPs (samd20 main.c:3734-3753). */
static void enter_model_setup(void)
{
    lcd_app_state_t *state = app_lcd_state();
    app_config_t    *cfg   = app_lcd_cfg();

    state->lcd_status = LCD_MODEL_SETUP;
    dgus_set_page(LCD_MODEL_SETUP);                     /* samd20 set_lcd_page then field echo */
    app_lcd_send_model_str(cfg->model_freq, cfg->model_type);
    dgus_write_u16(MODEL_FREQ, cfg->model_freq);
    dgus_write_u16(MODEL_TYPE, cfg->model_type);
    dgus_write_u16(VAR_CAL_VAL, (uint16_t)cfg->cal_val);
    dgus_write_u16(VAR_FREQ_CAL_VAL, (uint16_t)cfg->freq_cal_val);
}

/* STD_SETUP_PARAM (0x1020) page-nav cases 1..5 (samd20 main.c:3885-3963).
 * Updates state->lcd_status and switches page. NB case 1 uses set_page only
 * (no render rebuild); cases 2/3/4 use change_lcd_page. */
static void handle_std_setup_param(uint16_t data16)
{
    lcd_app_state_t *state = app_lcd_state();
    app_config_t    *cfg   = app_lcd_cfg();

    if (data16 == 1) {                                  /* GOTO SETUP PAGE 1 */
        if (state->lcd_status == LCD_SETUP_MH2 ||
            state->lcd_status == LCD_SETUP_MHC ||
            state->lcd_status == LCD_SETUP_MHE) {
            if (cfg->model_type == 0)        state->lcd_status = LCD_SETUP_HAND;
            else if (cfg->model_type == 1)   state->lcd_status = LCD_SETUP_MULTI;
        } else {
            state->lcd_status = LCD_SETUP_STD1;
        }
        dgus_set_page(state->lcd_status);               /* samd20 set_lcd_page only (no rebuild) */
    } else if (data16 == 2) {                           /* GOTO SETUP PAGE 2 */
        if (state->lcd_status == LCD_SETUP_STD1 ||
            state->lcd_status == LCD_SETUP_STD3 ||
            state->lcd_status == LCD_SETUP_STDC ||
            state->lcd_status == LCD_SETUP_STDE) {
            state->lcd_status = (cfg->run_mode == MODE_DELAY)
                                ? LCD_SETUP_STD2D : LCD_SETUP_STD2T;
        } else if (state->lcd_status == LCD_SETUP_MULTI ||
                   state->lcd_status == LCD_SETUP_HAND ||
                   state->lcd_status == LCD_SETUP_MHC ||
                   state->lcd_status == LCD_SETUP_MHE) {
            state->lcd_status = LCD_SETUP_MH2;
        }
        app_lcd_change_page(state->lcd_status);
    } else if (data16 == 3) {                           /* GOTO SETUP PAGE 3 */
        if (state->lcd_status == LCD_SETUP_STD1 ||
            state->lcd_status == LCD_SETUP_STD2D ||
            state->lcd_status == LCD_SETUP_STD2T ||
            state->lcd_status == LCD_SETUP_STDC ||
            state->lcd_status == LCD_SETUP_STDE) {
            state->lcd_status = LCD_SETUP_STD3;
        } else if (state->lcd_status == LCD_SETUP_MULTI ||
                   state->lcd_status == LCD_SETUP_HAND ||
                   state->lcd_status == LCD_SETUP_MH2) {
            /* COMM_SERIAL == 0 (ref/samd20/define.h:85) */
            state->lcd_status = (cfg->comm_mode == 0)
                                ? LCD_SETUP_MHC : LCD_SETUP_MHE;
        }
        app_lcd_change_page(state->lcd_status);
    } else if (data16 == 4) {                           /* GOTO SETUP PAGE 4 */
        /* COMM_SERIAL == 0 */
        state->lcd_status = (cfg->comm_mode == 0)
                            ? LCD_SETUP_STDC : LCD_SETUP_STDE;
        app_lcd_change_page(state->lcd_status);
    } else if (data16 == 5) {                           /* counter reset */
        if (cfg->work_cnt != 0) {
            state->temp_cnt_reset = 1;                  /* shadow: applied on DATA_SAVE */
        }
    }
}

/*--------------------------------------------------------------
 * Comm-config shadow edits (samd20 main.c:4026-4038)
 * Shadow-only: write temp_* + echo the fixed-width text field.
 *--------------------------------------------------------------*/

/* COMM_ADDR (0x1071): temp_address shadow + " NNN"/"NONE" echo. */
static void handle_comm_addr(uint16_t data16)
{
    lcd_app_state_t *state = app_lcd_state();
    uint8_t addr_buf[COMM_ADDR_TXT_LEN];

    state->temp_address = (uint8_t)data16;
    conv_addr2str(state->temp_address, addr_buf);
    dgus_write_bytes(COMM_ADDR_TXT, addr_buf, COMM_ADDR_TXT_LEN);
}

/* COMM_SPEED (0x1072): temp_speed_idx shadow + table-text echo (bounds-guarded). */
static void handle_comm_speed(uint16_t data16)
{
    lcd_app_state_t *state = app_lcd_state();
    uint8_t idx = (uint8_t)data16;

    if (idx > COMM_SPEED_IDX_MAX) {
        return;                                         /* out of comm_speed_txt range */
    }
    state->temp_speed_idx = idx;
    dgus_write_bytes(COMM_SPEED_TXT, comm_speed_txt[idx], COMM_SPEED_TXT_LEN);
}

/* COMM_PARITY (0x1073): temp_parity_idx shadow + table-text echo (bounds-guarded). */
static void handle_comm_parity(uint16_t data16)
{
    lcd_app_state_t *state = app_lcd_state();
    uint8_t idx = (uint8_t)data16;

    if (idx > COMM_PARITY_IDX_MAX) {
        return;                                         /* out of comm_parity_txt range */
    }
    state->temp_parity_idx = idx;
    dgus_write_bytes(COMM_PARITY_TXT, comm_parity_txt[idx], COMM_PARITY_TXT_LEN);
}

/* LV_COMM_MODE (0x140b): serial / ethernet-static / toggle-DHCP↔static
 * (samd20 main.c:4039-4079). Updates temp_comm_mode shadow + DISP_COMM_MODE /
 * DISP_EN_DHCP echoes + swaps between serial (_MHC/_STDC) and ethernet
 * (_MHE/_STDE) pages. */
static void handle_comm_mode(uint16_t data16)
{
    lcd_app_state_t *state = app_lcd_state();

    if (data16 == 0) {                                  /* serial */
        state->temp_comm_mode = COMM_SERIAL;
        dgus_write_u16(DISP_COMM_MODE, state->temp_comm_mode);
        if (state->lcd_status == LCD_SETUP_MHE) {
            state->lcd_status = LCD_SETUP_MHC;
        } else if (state->lcd_status == LCD_SETUP_STDE) {
            state->lcd_status = LCD_SETUP_STDC;
        }
        app_lcd_change_page(state->lcd_status);
    } else {                                            /* ethernet */
        if (data16 == 1) {                              /* enter ethernet (static) */
            dgus_write_u16(DISP_COMM_MODE, 1);          /* ethernet icon */
            state->temp_comm_mode = COMM_ETH_STATIC;
        } else {                                        /* toggle DHCP ↔ static */
            if (state->temp_comm_mode == COMM_ETH_STATIC) {
                state->temp_comm_mode = COMM_ETH_DHCP;
                dgus_write_u16(DISP_EN_DHCP, 1);
            } else if (state->temp_comm_mode == COMM_ETH_DHCP) {
                state->temp_comm_mode = COMM_ETH_STATIC;
                dgus_write_u16(DISP_EN_DHCP, 0);
            }
        }
        if (state->lcd_status == LCD_SETUP_MHC) {
            state->lcd_status = LCD_SETUP_MHE;
        } else if (state->lcd_status == LCD_SETUP_STDC) {
            state->lcd_status = LCD_SETUP_STDE;
        }
        app_lcd_change_page(state->lcd_status);
    }
}

/*--------------------------------------------------------------
 * Ethernet IP/NM/GW digit-entry FSM (samd20 process_ip_char,
 * ref/samd20/main.c:2850-2925; de-globalized onto app_lcd_state()).
 * Mojibake comments in the source ignored; logic ported verbatim.
 *--------------------------------------------------------------*/

/* key: raw digit 0..9, or 'D' dot, 'B' backspace, 'E' enter. */
static void process_ip_char(uint8_t key)
{
    lcd_app_state_t *state = app_lcd_state();

    if (key <= 9u) {                                    /* raw digit (samd20: c>=0 && c<=9) */
        state->ether_current_number = (uint16_t)(state->ether_current_number * 10u + key);
        if (state->ether_current_number > 255u) {
            state->ether_current_number = 255u;         /* clamp to valid octet */
        }
        if (state->ether_buffer_pos < 15u) {
            state->ether_input_buffer[state->ether_buffer_pos++] = (uint8_t)(key + '0');
            state->ether_input_buffer[state->ether_buffer_pos] = '\0';
        }
        state->ether_has_input = 1u;
    } else if (key == ETHER_KEY_DOT) {                  /* '.' → next octet */
        if (state->ether_has_input && state->ether_current_octet < 3u) {
            state->ether_temp_ip[state->ether_current_octet] =
                (uint8_t)state->ether_current_number;
            state->ether_current_octet++;
            state->ether_current_number = 0u;
            state->ether_has_input = 0u;
            if (state->ether_buffer_pos < 15u) {
                state->ether_input_buffer[state->ether_buffer_pos++] = '.';
                state->ether_input_buffer[state->ether_buffer_pos] = '\0';
            }
        }
    } else if (key == ETHER_KEY_BKSP) {                 /* backspace */
        if (state->ether_buffer_pos > 0u) {
            if (state->ether_input_buffer[state->ether_buffer_pos - 1u] == '.') {
                if (state->ether_current_octet > 0u) {
                    state->ether_current_octet--;
                    state->ether_current_number =
                        state->ether_temp_ip[state->ether_current_octet];
                    state->ether_has_input = 1u;
                }
            } else {
                state->ether_current_number /= 10u;
                state->ether_has_input = (state->ether_current_number > 0u) ? 1u : 0u;
            }
            state->ether_buffer_pos--;
            state->ether_input_buffer[state->ether_buffer_pos] = '\0';
        }
    } else if (key == ETHER_KEY_ENTER) {                /* enter → confirm */
        if (state->ether_has_input) {
            state->ether_temp_ip[state->ether_current_octet] =
                (uint8_t)state->ether_current_number;
        }
        state->ether_ip_input_complete = 1u;
    }
    /* any other key ignored (samd20) */
}

/* Seed the ether FSM for a freshly selected field from its shadow octets. */
static void ether_select_field(uint8_t field, const uint8_t shadow[4])
{
    lcd_app_state_t *state = app_lcd_state();
    uint8_t i;

    state->ether_what_input = field;
    state->ether_buffer_pos =
        ip_to_string(shadow, (char *)state->ether_input_buffer);
    for (i = 0; i < 4u; i++) {
        state->ether_temp_ip[i] = shadow[i];
    }
    state->ether_current_octet = 3u;
    /* samd20:4109 verbatim — seeds from the IP shadow's last octet UNCONDITIONALLY,
     * even when selecting NM/GW (likely a samd20 copy-paste bug; reproduced per
     * the verbatim-fidelity mandate). */
    state->ether_current_number = state->temp_ether_ip[3];
    state->ether_has_input = 1u;
}

/* LV_ETHER_KEY (0x140f): field select 'I'/'M'/'G' or digit/'D'/'B'/'E' edit
 * (samd20 main.c:4080-4135). On enter-complete commit ether_temp_ip → the
 * selected temp_ether_* shadow; echo the live edit buffer to COMM_*_TXT. */
static void handle_ether_key(uint16_t data16)
{
    lcd_app_state_t *state = app_lcd_state();
    uint8_t key = (uint8_t)data16;
    uint8_t disp_buf[COMM_IP_TXT_LEN];
    uint8_t i;

    if (key == ETHER_KEY_IP || key == ETHER_KEY_NM || key == ETHER_KEY_GW) {
        /* Field select: re-echo all three current shadows, then arm the chosen one. */
        ip_to_string(state->temp_ether_ip, (char *)state->ether_input_buffer);
        lcd_data_pdd(disp_buf, state->ether_input_buffer, COMM_IP_TXT_LEN);
        dgus_write_bytes(COMM_IP_TXT, disp_buf, COMM_IP_TXT_LEN);
        ip_to_string(state->temp_ether_nm, (char *)state->ether_input_buffer);
        lcd_data_pdd(disp_buf, state->ether_input_buffer, COMM_IP_TXT_LEN);
        dgus_write_bytes(COMM_NM_TXT, disp_buf, COMM_IP_TXT_LEN);
        ip_to_string(state->temp_ether_gw, (char *)state->ether_input_buffer);
        lcd_data_pdd(disp_buf, state->ether_input_buffer, COMM_IP_TXT_LEN);
        dgus_write_bytes(COMM_GW_TXT, disp_buf, COMM_IP_TXT_LEN);

        if (key == ETHER_KEY_IP) {
            ether_select_field(LCD_ETHER_INPUT_IP, state->temp_ether_ip);
        } else if (key == ETHER_KEY_NM) {
            ether_select_field(LCD_ETHER_INPUT_NM, state->temp_ether_nm);
        } else {
            ether_select_field(LCD_ETHER_INPUT_GW, state->temp_ether_gw);
        }
    } else {
        process_ip_char(key);
        if (state->ether_ip_input_complete) {
            uint8_t *dst = (state->ether_what_input == LCD_ETHER_INPUT_IP) ? state->temp_ether_ip
                         : (state->ether_what_input == LCD_ETHER_INPUT_NM) ? state->temp_ether_nm
                         :                                                    state->temp_ether_gw;
            for (i = 0; i < 4u; i++) {
                dst[i] = state->ether_temp_ip[i];
            }
            state->ether_ip_input_complete = 0u;
        }
        lcd_data_pdd(disp_buf, state->ether_input_buffer, COMM_IP_TXT_LEN);
        dgus_write_bytes((uint16_t)(COMM_IP_TXT + COMM_TXT_STRIDE * state->ether_what_input),
                         disp_buf, COMM_IP_TXT_LEN);
    }
}

/*--------------------------------------------------------------
 * DATA_SAVE (0x1050) — bulk commit / rollback (spec §8).
 *--------------------------------------------------------------*/

/* Commit the addr/speed/parity comm shadows → live cfg if any changed, and on
 * change fire the comm-reconfigure hook (samd20 close_modbus/init_modbus). */
static void commit_comm_serial_shadows(void)
{
    lcd_app_state_t *state = app_lcd_state();
    app_config_t    *cfg   = app_lcd_cfg();

    if (state->temp_address  != cfg->comm_address ||
        state->temp_speed_idx  != cfg->comm_speed_idx ||
        state->temp_parity_idx != cfg->comm_parity_idx) {
        cfg->comm_address    = state->temp_address;
        cfg->comm_speed_idx  = state->temp_speed_idx;
        cfg->comm_parity_idx = state->temp_parity_idx;
        app_lcd_hook_comm_reconfigure(cfg->comm_speed_idx,
                                      cfg->comm_parity_idx,
                                      cfg->comm_address);
    }
}

/* MULTI-only: commit comm_mode + ether shadows → live cfg, firing the ether
 * hook on ether change (samd20 main.c:3327-3403). HAND/STD do NOT do this. */
static void commit_comm_mode_and_ether(void)
{
    lcd_app_state_t *state = app_lcd_state();
    app_config_t    *cfg   = app_lcd_cfg();
    bool ether_changed = false;
    uint8_t i;

    if (state->temp_comm_mode != cfg->comm_mode) {
        cfg->comm_mode = state->temp_comm_mode;
    }
    for (i = 0; i < 4u; i++) {
        if (state->temp_ether_ip[i] != cfg->ether_ip[i] ||
            state->temp_ether_nm[i] != cfg->ether_nm[i] ||
            state->temp_ether_gw[i] != cfg->ether_gw[i]) {
            ether_changed = true;
            break;
        }
    }
    if (ether_changed) {
        for (i = 0; i < 4u; i++) {
            cfg->ether_ip[i] = state->temp_ether_ip[i];
            cfg->ether_nm[i] = state->temp_ether_nm[i];
            cfg->ether_gw[i] = state->temp_ether_gw[i];
        }
        app_lcd_hook_ether_apply(cfg->comm_mode, cfg->ether_ip, cfg->ether_nm, cfg->ether_gw);
    }
}

/* counter-reset shadow → work_cnt=0 + echo (samd20 main.c:3450-3454). */
static void commit_cnt_reset(void)
{
    lcd_app_state_t *state = app_lcd_state();
    app_config_t    *cfg   = app_lcd_cfg();

    if (state->temp_cnt_reset == 1u) {
        cfg->work_cnt = 0u;
        dgus_write_u32(LV_WORK_CNT, 0u);
        state->temp_cnt_reset = 0u;
    }
}

/* DATA_SAVE == 1: commit live cfg → FRAM by current page group (spec §8.2). */
static void data_save_commit(void)
{
    lcd_app_state_t *state = app_lcd_state();
    app_config_t    *cfg   = app_lcd_cfg();

    if (state->lcd_status == LCD_MODEL_SETUP) {
        /* MODEL_SETUP: model/cal already live; pick run page + sys_mode by model. */
        state->sys_mode   = cfg->model_type;
        state->lcd_status = run_page_for_mode(state->sys_mode);
    } else if (state->lcd_status == LCD_SETUP_MULTI ||
               (state->lcd_status == LCD_SETUP_MH2 && cfg->model_type == 1u) ||
               state->lcd_status == LCD_SETUP_MHC ||
               state->lcd_status == LCD_SETUP_MHE) {
        /* MULTI path: out power DAC + comm/ether commit + hooks (samd20 3327-3403).
         * NB MHC/MHE always resolve here (else-if order) — HAND's MHC/MHE is dead. */
        app_lcd_hook_set_pot(cfg->output_power);
        commit_comm_mode_and_ether();
        commit_comm_serial_shadows();
        state->lcd_status = LCD_RUN_MULTI;
    } else if (state->lcd_status == LCD_SETUP_HAND ||
               (state->lcd_status == LCD_SETUP_MH2 && cfg->model_type == 0u)) {
        /* HAND path: out power DAC + addr/speed/parity only (samd20 3406-3454).
         * NO comm_mode / ether commit (samd20 confines those to MULTI). */
        app_lcd_hook_set_pot(cfg->output_power);
        commit_comm_serial_shadows();
        state->lcd_status = LCD_RUN_MULTI;          /* F3: HAND→MULTI per samd20, verbatim */
    } else if (state->lcd_status == LCD_SETUP_STD1 ||
               state->lcd_status == LCD_SETUP_STD2D ||
               state->lcd_status == LCD_SETUP_STD2T ||
               state->lcd_status == LCD_SETUP_STD3 ||
               state->lcd_status == LCD_SETUP_STDC ||
               state->lcd_status == LCD_SETUP_STDE) {
        /* STD path: out power DAC + cnt_reset + horndown + addr/speed/parity only
         * (samd20 3455-3510). NO comm_mode / ether commit. */
        app_lcd_hook_set_pot(cfg->output_power);
        commit_cnt_reset();
        app_lcd_hook_horn(state->temp_horndown == 1u);   /* samd20 SOL_DN / SYS_HORN path */
        commit_comm_serial_shadows();
        state->lcd_status = LCD_RUN_STD;
    }

    /* FRAM has no write-cycle cost: commit the whole live map once (spec §8.3),
     * replacing samd20's scattered per-field save_*_fram calls. */
    app_config_save_all(cfg);

    app_lcd_change_page(state->lcd_status);
}

/* DATA_SAVE == 0: rollback. Full FRAM re-read reverts the process params that
 * were live-mutated on touch; then re-arm the comm-shadow sentinel so the next
 * comm-page entry reloads shadows from live (samd20 main.c:3511-3630). */
static void data_save_cancel(void)
{
    lcd_app_state_t *state = app_lcd_state();
    app_config_t    *cfg   = app_lcd_cfg();

    bool was_model_setup = (state->lcd_status == LCD_MODEL_SETUP);

    app_config_load(cfg);                           /* full FRAM rollback of process params */
    state->temp_comm_mode = 0xFFu;                  /* re-arm shadow-load sentinel */
    /* F4: full reload is a no-op for comm/ether (live never pre-save-mutated);
     * matches samd20 effective behavior. */

    if (was_model_setup) {
        /* samd20 MODEL_SETUP cancel: restore sys_mode + re-echo model string
         * (model_freq/type already reloaded by app_config_load). */
        state->sys_mode = cfg->model_type;
        app_lcd_send_model_str(cfg->model_freq, cfg->model_type);
    }

    /* Choose the return run page per current page group (samd20 sets lcd_status
     * inside each branch only; pages outside the known groups are left as-is —
     * matches samd20, which never mutates lcd_status for unrecognized pages).
     * MODEL_SETUP cancel → MULTI verbatim (CANCEL/SAVE asymmetry: cancel does
     * NOT pick by model_type). */
    if (was_model_setup ||
        state->lcd_status == LCD_SETUP_MULTI ||
        state->lcd_status == LCD_SETUP_HAND ||
        state->lcd_status == LCD_SETUP_MH2 ||
        state->lcd_status == LCD_SETUP_MHC ||
        state->lcd_status == LCD_SETUP_MHE) {
        state->lcd_status = LCD_RUN_MULTI;          /* F3: HAND/MODEL/MULTI cancel → MULTI */
    } else if (state->lcd_status == LCD_SETUP_STD1 ||
               state->lcd_status == LCD_SETUP_STD2D ||
               state->lcd_status == LCD_SETUP_STD2T ||
               state->lcd_status == LCD_SETUP_STD3 ||
               state->lcd_status == LCD_SETUP_STDC ||
               state->lcd_status == LCD_SETUP_STDE) {
        state->temp_horndown = 0u;                  /* samd20 clears temp_horndown only in STD cancel */
        state->lcd_status = LCD_RUN_STD;
    }

    app_lcd_change_page(state->lcd_status);
}

/*--------------------------------------------------------------
 * Public entry — VP → action dispatch
 *--------------------------------------------------------------*/

void app_lcd_input_dispatch(const dgus_frame_t *f)
{
    /* §3: act only on RD (0x83) touch/key reports; ignore WR (0x82) echoes. */
    if (f->cmd != DGUS_CMD_RD) {
        return;
    }

    /* F5 (confirmed against drivers/dgus_lcd.c parser_step):
     * frame_buf[3+i] -> data[i], so data[0] is the first payload byte (MSB)
     * after the VP address. data16 = first big-endian word. */
    uint16_t vp     = f->vp_addr;
    uint16_t data16 = (uint16_t)(((uint16_t)f->data[0] << 8) | f->data[1]);

#ifdef LCD_TRACE_RX
    mon_printf("[lcd] rx vp=0x%04X data=%u\r\n", (unsigned)vp, (unsigned)data16);
#endif

    lcd_app_state_t *state = app_lcd_state();
    app_config_t    *cfg   = app_lcd_cfg();

    switch (vp) {

    /*--- model / calibration -----------------------------------------------*/
    case MODEL_FREQ:
        cfg->model_freq = (uint8_t)data16;
        app_lcd_send_model_str(cfg->model_freq, cfg->model_type);
        break;
    case MODEL_TYPE:
        cfg->model_type = (uint8_t)data16;
        app_lcd_send_model_str(cfg->model_freq, cfg->model_type);
        break;
    case VAR_CAL_VAL:
        cfg->cal_val = (int16_t)data16;
        break;
    case VAR_FREQ_CAL_VAL:
        cfg->freq_cal_val = (int16_t)data16;
        break;

    /*--- delay/trigger limit times -----------------------------------------*/
    case LV_DM_DELAY:
        cfg->limit_delay_time1 = data16;
        break;
    case LV_DM_WELD:
        cfg->limit_delay_time2 = data16;
        break;
    case LV_DM_HOLD:
        cfg->limit_delay_time3 = data16;
        break;
    case LV_TM_WELD:
        cfg->limit_trigger_time2 = data16;
        break;
    case LV_TM_HOLD:
        cfg->limit_trigger_time3 = data16;
        break;

    /*--- multi-output limits -----------------------------------------------*/
    case LV_MO_OUT1:
        cfg->limit_mo_out1 = data16;
        break;
    case LV_MO_OUT2:
        cfg->limit_mo_out2 = data16;
        break;
    case LV_MO_TIME1:
        cfg->limit_mo_time1 = data16;
        if (cfg->limit_mo_time1 > cfg->limit_mo_time2) {
            cfg->limit_mo_time2 = cfg->limit_mo_time1;          /* samd20 main.c:4011-4015 */
            dgus_write_u16(LV_MO_TIME2, cfg->limit_mo_time2);   /* echo new clamped value */
        }
        break;
    case LV_MO_TIME2:
        cfg->limit_mo_time2 = data16;
        if (cfg->limit_mo_time1 > cfg->limit_mo_time2) {
            cfg->limit_mo_time2 = cfg->limit_mo_time1;          /* samd20 main.c:4020-4024 */
            dgus_write_u16(LV_MO_TIME2, cfg->limit_mo_time2);   /* echo new clamped value */
        }
        break;

    /*--- scalar limit edits ------------------------------------------------*/
    case LV_OUT_POWER:
        /* output_power ONLY — NO DAC here. set_pot fires on RUN press + DATA_SAVE
         * + setup-page entry (spec §7 fidelity; samd20 main.c:3807-3813). */
        cfg->output_power = (uint8_t)data16;
        break;
    case LV_MAX_ON_TIME:
        cfg->limit_on_time = data16;
        break;
    case LV_ENERGY_EDIT:
        cfg->limit_energy = data16;
        break;
    case LV_LIMIT_OUT_T:
        cfg->limit_out_time = (uint8_t)data16;
        break;
    case DISP_SAFTY:
        cfg->f_safty = (data16 == 1) ? 1u : 0u;
        break;

    /*--- toggles (echo NEW value after the flip) ---------------------------*/
    case ENERGY_EN:
        if (data16 == 1) {
            cfg->energy_ctrl = !cfg->energy_ctrl;
            dgus_write_u16(DISP_ENERGY_EN, cfg->energy_ctrl ? 1u : 0u);
        }
        break;
    case MULTI_EN:
        if (data16 == 1) {
            cfg->multi_ctrl = !cfg->multi_ctrl;
            dgus_write_u16(DISP_MULTI_EN, cfg->multi_ctrl ? 1u : 0u);
        }
        break;

    /*--- horn-down shadow --------------------------------------------------*/
    case DISP_HORNDOWN:
        state->temp_horndown = (data16 == 1) ? 1u : 0u;   /* applied on DATA_SAVE */
        break;

    /*--- page navigation ---------------------------------------------------*/
    case SETUP_PARAM:
        state->lcd_status = setup1_page_for_mode(state->sys_mode);
        app_lcd_change_page(state->lcd_status);
        break;
    case SETUP_PARAM_MOOHAN:                             /* long-press variant of SETUP_PARAM */
        if (long_press_released(data16)) {
            state->lcd_status = setup1_page_for_mode(state->sys_mode);
            app_lcd_change_page(state->lcd_status);
        }
        break;
    case SETUP_MODEL:                                    /* long-press → model setup */
        if (long_press_released(data16)) {
            enter_model_setup();
        }
        break;
    case STD_SETUP_PARAM:
        handle_std_setup_param(data16);
        break;
    case LV_RUN_MODE:
        if (data16 == 1) {                               /* delay mode */
            cfg->run_mode = MODE_DELAY;
            state->lcd_status = LCD_SETUP_STD2D;
            app_lcd_change_page(state->lcd_status);
        } else if (data16 == 2) {                        /* trigger mode */
            cfg->run_mode = MODE_TRIGGER;
            state->lcd_status = LCD_SETUP_STD2T;
            app_lcd_change_page(state->lcd_status);
        }
        break;

    /*--- ultrasonic commands ----------------------------------------------*/
    case KEY_MULTI:
        handle_key_multi(data16);
        break;
    case KEY_ERROR_RESET:
        handle_key_error_reset(data16);
        break;

    /*--- save / cancel + comm / ethernet (samd20 parse_lcd_comm tail) ------*/
    case DATA_SAVE:
        if (data16 == SAVE_COMMIT) {
            data_save_commit();
        } else {
            data_save_cancel();
        }
        break;
    case COMM_ADDR:
        handle_comm_addr(data16);
        break;
    case COMM_SPEED:
        handle_comm_speed(data16);
        break;
    case COMM_PARITY:
        handle_comm_parity(data16);
        break;
    case LV_COMM_MODE:
        handle_comm_mode(data16);
        break;
    case LV_ETHER_KEY:
        handle_ether_key(data16);
        break;

    /*--- panel boot / page-flip notification — guarded re-init (spec §10) ----
     * data16==0 means the panel reports it landed on a page (its own splash or a
     * mid-run reset). Re-seed the panel vars + model string + run page, but ONLY
     * when (a) the Stage B boot handshake has finished (boot_complete) and (b) at
     * least 200 ms passed since our own last set_page — otherwise the
     * change_page→set_page→SYS_PIC_NOW→re-init→set_page chain is a feedback loop.
     * app_lcd_init_mode ends in app_lcd_change_page, which refreshes
     * last_set_page_ms, so the 200 ms gate re-arms after each re-init. */
    case SYS_PIC_NOW:
        if (data16 == 0 && state->boot_complete &&
            (uint32_t)(sys_tick_get_ms() - state->last_set_page_ms) >= 200u) {
            app_lcd_var_init();
            app_lcd_send_model_str(cfg->model_freq, cfg->model_type);
            app_lcd_init_mode(cfg);
        }
        break;

    default:
        break;
    }
}
