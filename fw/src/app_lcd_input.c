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

    mon_printf("[lcd] rx vp=0x%04X data=%u\r\n", (unsigned)vp, (unsigned)data16);

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

    /*--- DEFERRED to Task 7 (save/cancel + comm/ethernet) ------------------*/
    case DATA_SAVE:
        /* Task 7: SAVE (==1) / CANCEL (==0) — bulk commit / rollback (spec §8). */
        break;
    case COMM_ADDR:
        /* Task 7: temp_address shadow + COMM_ADDR_TXT echo. */
        break;
    case COMM_SPEED:
        /* Task 7: temp_speed_idx shadow + COMM_SPEED_TXT echo. */
        break;
    case COMM_PARITY:
        /* Task 7: temp_parity_idx shadow + COMM_PARITY_TXT echo. */
        break;
    case LV_COMM_MODE:
        /* Task 7: temp_comm_mode serial/static/DHCP + DISP_COMM_MODE/EN_DHCP + change_page. */
        break;
    case LV_ETHER_KEY:
        /* Task 7: ether field select + process_ip_char FSM → temp_ether_*. */
        break;

    default:
        break;
    }
}
