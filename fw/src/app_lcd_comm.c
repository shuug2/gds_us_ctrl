/* fw/src/app_lcd_comm.c — comm/ethernet 설정 편집 + DATA_SAVE commit/rollback.
 * app_lcd_input.c에서 분할 (2026-07-19, 순수 코드 이동 — samd20 parse_lcd_comm tail).
 * Port source: ref/samd20/main.c 2850-2925 (process_ip_char), 3327-3630 (save/cancel),
 * 4026-4135 (comm/ether rows). Spec: §7, §8. */
#include "app_lcd.h"
#include "dgus_lcd.h"
#include "mon.h"
#include "app_lcd_input_priv.h"

/*--------------------------------------------------------------
 * samd20 define.h / main.c constants (file-local — verbatim values)
 *--------------------------------------------------------------*/
/* comm_mode values (ref/samd20/define.h:85-87) */
#define COMM_SERIAL      0u
#define COMM_ETH_STATIC  1u
#define COMM_ETH_DHCP    2u

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
#define COMM_SPEED_IDX_MAX   CFG_COMM_SPEED_IDX_MAX   /* app_config.h 공용 (M3) */
#define COMM_PARITY_IDX_MAX  CFG_COMM_PARITY_IDX_MAX

/*--------------------------------------------------------------
 * Comm-config shadow edits (samd20 main.c:4026-4038)
 * Shadow-only: write temp_* + echo the fixed-width text field.
 *--------------------------------------------------------------*/

/* COMM_ADDR 편집 에코 */
void handle_comm_addr(uint16_t data16)
{
    lcd_app_state_t *state = app_lcd_state();
    uint8_t addr_buf[COMM_ADDR_TXT_LEN];

    state->temp_address = (uint8_t)data16;
    conv_addr2str(state->temp_address, addr_buf);
    dgus_write_bytes(COMM_ADDR_TXT, addr_buf, COMM_ADDR_TXT_LEN);
}

/* COMM_SPEED 편집 에코 */
void handle_comm_speed(uint16_t data16)
{
    lcd_app_state_t *state = app_lcd_state();
    uint8_t idx = (uint8_t)data16;

    if (idx > COMM_SPEED_IDX_MAX) {
        return;                                         /* out of comm_speed_txt range */
    }
    state->temp_speed_idx = idx;
    dgus_write_bytes(COMM_SPEED_TXT, comm_speed_txt[idx], COMM_SPEED_TXT_LEN);
}

/* COMM_PARITY 편집 에코 */
void handle_comm_parity(uint16_t data16)
{
    lcd_app_state_t *state = app_lcd_state();
    uint8_t idx = (uint8_t)data16;

    if (idx > COMM_PARITY_IDX_MAX) {
        return;                                         /* out of comm_parity_txt range */
    }
    state->temp_parity_idx = idx;
    dgus_write_bytes(COMM_PARITY_TXT, comm_parity_txt[idx], COMM_PARITY_TXT_LEN);
}

/* comm 모드 선택 처리 */
void handle_comm_mode(uint16_t data16)
{
    /* LV_COMM_MODE (0x140b): serial / ethernet-static / toggle-DHCP↔static
     * (samd20 main.c:4039-4079). Updates temp_comm_mode shadow + DISP_COMM_MODE /
     * DISP_EN_DHCP echoes + swaps between serial (_MHC/_STDC) and ethernet
     * (_MHE/_STDE) pages. */
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

/* IP 문자 입력 FSM */
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

/* ether 필드 선택 시드 */
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

/* ether 키 입력 처리 */
void handle_ether_key(uint16_t data16)
{
    /* LV_ETHER_KEY (0x140f): field select 'I'/'M'/'G' or digit/'D'/'B'/'E' edit
     * (samd20 main.c:4080-4135). On enter-complete commit ether_temp_ip → the
     * selected temp_ether_* shadow; echo the live edit buffer to COMM_*_TXT. */
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

/* serial 설정 커밋 */
static void commit_comm_serial_shadows(void)
{
    /* Commit the addr/speed/parity comm shadows → live cfg if any changed, and on
     * change fire the comm-reconfigure hook (samd20 close_modbus/init_modbus). */
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

/* comm/ether 커밋 */
static void commit_comm_mode_and_ether(void)
{
    /* Commit comm_mode + ether shadows → live cfg, firing the ether hook on
     * ether OR comm_mode change (samd20 main.c:3327-3403 re-ran
     * close_tcps+network_init on save — M7 restores that liveness). HAND does
     * NOT do this; STD reaches here via the STD-persist deviation (fix-B 0xFF
     * guard below covers its unseeded-shadow case). */
    lcd_app_state_t *state = app_lcd_state();
    app_config_t    *cfg   = app_lcd_cfg();
    bool ether_changed = false;
    bool mode_changed  = false;
    uint8_t i;

#ifdef LCD_TRACE_RX
    mon_printf("[lcd] commit cm temp=%u cfg=%u\r\n",
               (unsigned)state->temp_comm_mode, (unsigned)cfg->comm_mode);
#endif

    /* Guard (fix B): 0xFF = comm/ether shadows were never seeded from cfg this
     * setup session (no comm-page visit). The shadows hold the sentinel/zero
     * (boot) state, so committing would write comm_mode=0xFF + 0.0.0.0 ether
     * over live cfg and persist garbage to FRAM. Skip = cfg unchanged.
     * samd20 never hit this because STD save did not commit comm; the
     * STD-persist deviation (data_save_commit STD branch) opened a live path:
     * SAVE from a non-comm STD page (STD1/2/3, which set 0xFF on entry). */
    if (state->temp_comm_mode == 0xFFu) {
        return;
    }

    if (state->temp_comm_mode != cfg->comm_mode) {
        cfg->comm_mode = state->temp_comm_mode;
        mode_changed   = true;
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
    }
    if (ether_changed || mode_changed) {
        app_lcd_hook_ether_apply(cfg->comm_mode, cfg->ether_ip, cfg->ether_nm, cfg->ether_gw);
    }
}

/* 카운터 리셋 커밋 */
static void commit_cnt_reset(void)
{
    /* counter-reset shadow → work_cnt=0 + echo (samd20 main.c:3450-3454). */
    lcd_app_state_t *state = app_lcd_state();
    app_config_t    *cfg   = app_lcd_cfg();

    if (state->temp_cnt_reset == 1u) {
        cfg->work_cnt = 0u;
        dgus_write_u32(LV_WORK_CNT, 0u);
        state->temp_cnt_reset = 0u;
    }
}

/* DATA_SAVE 커밋 */
void data_save_commit(void)
{
    /* DATA_SAVE == 1: commit live cfg → FRAM by current page group (spec §8.2). */
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
        /* STD path: out power DAC + cnt_reset + horndown + addr/speed/parity.
         * samd20 (3455-3510) confined comm_mode/ether commit to MULTI, so STD
         * saves dropped ether/comm_mode (quirk). Intentional deviation
         * (2026-05-27, user): commit comm_mode/ether here too so STD persists
         * them like MULTI. */
        app_lcd_hook_set_pot(cfg->output_power);
        commit_cnt_reset();
        app_lcd_hook_horn(state->temp_horndown == 1u);   /* samd20 SOL_DN / SYS_HORN path */
        commit_comm_mode_and_ether();                    /* deviation: STD now persists ether/comm_mode */
        commit_comm_serial_shadows();
        state->lcd_status = LCD_RUN_STD;
    }

    /* FRAM has no write-cycle cost: commit the whole live map once (spec §8.3),
     * replacing samd20's scattered per-field save_*_fram calls. */
    app_config_save_all(cfg);

    app_lcd_change_page(state->lcd_status);
}

/* DATA_SAVE 롤백 */
void data_save_cancel(void)
{
    /* DATA_SAVE == 0: rollback. Full FRAM re-read reverts the process params that
     * were live-mutated on touch; then re-arm the comm-shadow sentinel so the next
     * comm-page entry reloads shadows from live (samd20 main.c:3511-3630). */
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
