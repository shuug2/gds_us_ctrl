/* fw/include/app_lcd.h — LCD application data setup (samd20 init_lcd_mode port, scope a). */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "app_config.h"
#include "dgus_lcd.h"   /* dgus_frame_t for app_lcd_input_dispatch */

/* GDSONIC model string -> MODEL_NAME (11-byte payload incl. trailing NUL). */
void app_lcd_send_model_str(uint8_t freq, uint8_t type);

/* Resolve the RUN page id for the configured model_type. */
uint8_t app_lcd_run_page(const app_config_t *cfg);

/* Pre-fill RUN-page VPs from cfg, then switch to the run page. */
void app_lcd_init_mode(const app_config_t *cfg);

/* Read-back confirm the run page took (SYS_PIC_NOW); re-assert up to
 * DGUS_PAGE_CONFIRM_RETRIES times if the panel reverted to its boot page
 * after finishing its own splash. Returns true once confirmed. */
bool app_lcd_ensure_run_page(const app_config_t *cfg);

/*--------------------------------------------------------------
 * LCD string formatters (samd20 main.c port — values land on panel verbatim)
 *--------------------------------------------------------------*/

/* Tenths-of-seconds "X.XX" form. Returns bytes written INCLUDING trailing NUL
 * (samd20 callers pass the return as the send length). */
uint8_t time2str(uint16_t src, uint8_t *dest);

/* Compact decimal, leading zeros suppressed. Returns bytes written incl. NUL. */
uint8_t energy2str(uint32_t src, uint8_t *dest);

/* 4-byte field: addr 0 -> "NONE", else " NNN" (space + 3 digits). No NUL. */
void conv_addr2str(uint8_t addr, uint8_t *buff);

/* "a.b.c.d" zero-padded to 16 bytes. Returns 16. dest must be >= 16 bytes. */
uint8_t ip_to_string(const uint8_t ip[4], char *str_buffer);

/* Copy src into dest, NUL-filling from the first NUL onward (samd20 lcd_data_pdd,
 * de-globalized: explicit dest instead of the lcd_temp_buf global). */
void lcd_data_pdd(uint8_t *dest, const uint8_t *src, uint8_t len);

/* Fixed-width comm-config display strings (samd20 main.c:160-161 verbatim).
 * NOT NUL-terminated: sent as exactly 6 / 4 bytes via dgus_write_bytes.
 * Shared by render (COMM_*_TXT echo on page entry) and input (COMM_SPEED/PARITY). */
extern const uint8_t comm_speed_txt[6][6];   /* " 2400 " … "115200" */
extern const uint8_t comm_parity_txt[3][4];  /* "EVEN" "ODD " "NONE" */

/*==============================================================
 * LCD subsystem state, measurement provider, control/HW hooks
 *  (Stage LCD full port — spec §4, §5)
 *==============================================================*/

/* Ultrasonic command edges raised by panel keys (KEY_MULTI / KEY_ERROR_RESET).
 * Stub-logged now; Stage D drives the real command FSM / output pins. */
typedef enum {
    US_CMD_START,
    US_CMD_SEEK,
    US_CMD_RESET,
    US_CMD_RUN_RELEASE,
} us_cmd_t;

/* ether-input field selector (LV_ETHER_KEY 'I'/'M'/'G'); NONE = idle. */
#define LCD_ETHER_INPUT_NONE  0xFFu
#define LCD_ETHER_INPUT_IP    0u
#define LCD_ETHER_INPUT_NM    1u
#define LCD_ETHER_INPUT_GW    2u

/* Transient runtime state owned by the LCD subsystem (spec §4.2).
 * Config/limit values live in app_config_t (g_cfg); this holds only what is
 * NOT persisted: current page, mode/status, setup-edit shadows, ether FSM. */
typedef struct {
    uint8_t  lcd_status;        /* current page ID */
    uint8_t  sys_mode;          /* 0=hand 1=multi 2=std (= model_type) */
    uint8_t  sys_status;        /* SYS_RUN/SETUP/HORN/ERROR (control-fed) */
    uint8_t  error_status;      /* ERR_* bitmask (control-fed; cleared by key) */
    uint8_t  horn_status;       /* solenoid request (control-fed) */
    uint8_t  key_tick;          /* long-press timer (SETUP_MODEL / _MOOHAN) — legacy, unused */
    uint32_t key_press_ms;      /* long-press start ms (SETUP_MODEL / _MOOHAN); samd20 KEY_HOLD_TH 200×10ms = 2000ms */
    uint16_t key_press_vp;      /* VP currently held down for long-press pairing (0 = none); HW: panel emits data=0 on both down+up */

    uint16_t ref_lv_1, ref_lv_2, ref_lv_10, ref_lv_20;  /* output-bar thresholds (from model_freq) */

    /* setup-page comm/ether edit shadows (committed to FRAM on DATA_SAVE) */
    uint8_t  temp_address, temp_speed_idx, temp_parity_idx;
    uint8_t  temp_comm_mode;    /* 0xFF sentinel = "not loaded yet" */
    uint8_t  temp_ether_ip[4], temp_ether_nm[4], temp_ether_gw[4];
    uint8_t  temp_cnt_reset;    /* shadow: reset work_cnt on save */
    uint8_t  temp_horndown;     /* shadow: horn-down request on save */

    /* ether IP/NM/GW text-entry FSM (LV_ETHER_KEY) */
    uint8_t  ether_what_input;          /* LCD_ETHER_INPUT_* */
    uint16_t ether_current_number;      /* octet accumulator; wider than octet so the >255 clamp is live */
    uint8_t  ether_current_octet;       /* 0..3 */
    uint8_t  ether_has_input;
    uint8_t  ether_ip_input_complete;
    uint8_t  ether_buffer_pos;
    uint8_t  ether_input_buffer[16];    /* ASCII edit buffer shown on panel */
    uint8_t  ether_temp_ip[4];          /* staging during one field's edit */

    uint32_t last_set_page_ms;  /* SYS_PIC_NOW loop guard (spec §10) */
    bool     boot_complete;     /* honor SYS_PIC_NOW re-init only after app_init */
} lcd_app_state_t;

/* Live measured values the display machine renders (spec §4.3).
 * Stubbed (all-zero) until Stage D regulation fills this provider. */
typedef struct {
    uint16_t curr_amp;
    uint16_t curr_power, max_power, last_power;
    uint16_t curr_freq, last_freq;          /* Hz; displayed /100 */
    uint32_t curr_energy, last_energy;
    uint8_t  us_on_time_200m;               /* 0..200 → LV_TIME bar fill */
    uint8_t  us_run_status;                 /* US_IDLE/REMOTE/TOUCH/COMM */
    uint8_t  sig_run_status, sig_seek_status, sig_reset_status;
} lcd_measure_t;

/* Single owner of the transient state (kept in app_lcd.c). */
lcd_app_state_t *app_lcd_state(void);

/* Single owner of the live config (kept in app_lcd.c). The input layer mutates
 * it; the render layer reads it; DATA_SAVE commits it to FRAM. app.c loads it
 * at boot via this accessor. */
app_config_t *app_lcd_cfg(void);

/* Measurement provider — stub returns all-zero until Stage D. */
const lcd_measure_t *app_lcd_measure(void);

/* Control / hardware stub hooks (spec §5) — Stage C/D integration points.
 * Bodies log via mon_printf only; no hardware driven this stage. */
void app_lcd_hook_set_pot(uint8_t output_power);
void app_lcd_hook_us_command(us_cmd_t cmd);
void app_lcd_hook_comm_reconfigure(uint8_t speed_idx, uint8_t parity_idx, uint8_t address);
void app_lcd_hook_ether_apply(uint8_t mode, const uint8_t ip[4], const uint8_t nm[4], const uint8_t gw[4]);
void app_lcd_hook_horn(bool down);

/* Subsystem entry points (defined in app_lcd_render/input/disp — Tasks 5-9). */
void app_lcd_change_page(uint8_t page);               /* render + set_page (spec §6) */
void app_lcd_input_dispatch(const dgus_frame_t *f);   /* panel touch/key handler (spec §7) */
void app_lcd_tick(void);                              /* periodic display step (spec §11) */
void app_lcd_var_init(void);                          /* panel-var seed (boot / SYS_PIC_NOW) */

/* Display layer (app_lcd_disp.c — spec §11). step machine advances one VP-group per
 * call (0..9 wrap); the icon/work_cnt/error helpers are event-driven (input + Stage D). */
void app_lcd_disp_step(void);                         /* one VP-group per call (LV_OUTPUT/LV_TIME/VAR_*) */
void app_lcd_icon(uint16_t icon_vp, bool on);         /* ICON_RESET/SEEK/RUN/OL/OUTERR set/clear */
void app_lcd_set_work_cnt(uint32_t cnt);              /* LV_WORK_CNT (u32) */
void app_lcd_show_error(uint8_t error_code);          /* error_status -> VP_ERROR_MSG + LCD_WARNING */
