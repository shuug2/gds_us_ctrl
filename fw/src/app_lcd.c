/* fw/src/app_lcd.c — LCD app: init_lcd_mode port + subsystem state owner + stub hooks. */
#include "define.h"    /* 모델 브랜드 선택 (GDSONIC/DIAMT/POWERTECH/MOOHAN) */
#include "app_lcd.h"
#include "dgus_lcd.h"
#include "sys_tick.h"
#include "mon.h"
#include "app_reg.h"
#include "app_horn.h"
#include "i2c_pot.h"   /* pot_dac_from_power + i2c_pot_set_dac (U4 진폭) */

/*--- Stage LCD subsystem state owner + control/HW stub hooks ---
 * Single definition of the transient state; render/input/disp layers (Tasks 5-9)
 * reach it via app_lcd_state(). Hooks log only — Stage C/D add real bodies. */
static lcd_app_state_t g_lcd;
static app_config_t    g_cfg;   /* live config owner (loaded at boot, edited by input, saved to FRAM) */

/* LCD 상태 싱글턴 접근 */
lcd_app_state_t *app_lcd_state(void) { return &g_lcd; }
/* 라이브 cfg 접근 */
app_config_t    *app_lcd_cfg(void)   { return &g_cfg; }

/* 측정값 제공 */
const lcd_measure_t *app_lcd_measure(void)
{
    /* app_reg 가 게시하는 라이브 측정값 — 진폭·주파수·에너지·상태 전부
     * (slice 2 이후 전 필드 라이브). */
    return app_reg_measure();
}

/* 진폭 POT hook */
void app_lcd_hook_set_pot(uint8_t output_power)
{
    /* output_power(%) → wiper DAC (언더플로 가드 + 상한 포화, i2c_pot.h) →
     * U4 I2C_POT @0x28. F2: 진폭=I2C_POT 기능적 역할만 확정(사용자) —
     * 칩 정체(DS1803?)·wiper 스케일(0–127 vs 0–255)은 HW/6b 이연. */
    uint8_t dac = pot_dac_from_power(output_power);
    i2c_pot_set_dac(dac);
    mon_printf("[lcd-hook] set_pot power=%u dac=%u\r\n",
               (unsigned)output_power, (unsigned)dac);
}

/* US 명령 hook */
void app_lcd_hook_us_command(us_cmd_t cmd)
{
    mon_printf("[lcd-hook] us_command=%u\r\n", (unsigned)cmd);
    app_reg_command(cmd, (uint8_t)US_TOUCH);   /* panel keys = touch source */
}

/* 통신 재설정 hook */
void app_lcd_hook_comm_reconfigure(uint8_t speed_idx, uint8_t parity_idx, uint8_t address)
{
    /* Intentionally log-only: app_modbus_tick() re-evaluates the occupancy
     * rule + line params against live cfg every superloop iter (samd20
     * main-loop gate equivalent), so the close/reinit happens within one iter
     * of DATA_SAVE — including the comm_mode-only change this hook never sees.
     * Keeping the body passive avoids an app_lcd <-> app_modbus include cycle
     * (M1 discipline). NB: suppressed while Modbus owns USART6 (mon gate). */
    mon_printf("[lcd-hook] comm speed=%u parity=%u addr=%u\r\n",
               (unsigned)speed_idx, (unsigned)parity_idx, (unsigned)address);
}

/* M7: LCD DATA_SAVE가 ether/comm_mode를 커밋했음을 app_eth_tick에 알리는
 * 1-shot 플래그. hook에서 set, app_lcd_ether_dirty_take()가 consume-and-clear.
 * (직접 호출 대신 플래그 = app_lcd↔app_eth include 사이클 회피, M1 discipline —
 * comm_reconfigure hook이 passive인 것과 같은 패턴.) */
static bool s_ether_dirty = false;

/* ether dirty 소비 */
bool app_lcd_ether_dirty_take(void)
{
    bool d = s_ether_dirty;
    s_ether_dirty = false;
    return d;
}

/* ether 설정 hook */
void app_lcd_hook_ether_apply(uint8_t mode, const uint8_t ip[4], const uint8_t nm[4], const uint8_t gw[4])
{
    s_ether_dirty = true;   /* consumed by app_eth_tick -> eth_reapply (M7) */
    mon_printf("[lcd-hook] ether mode=%u ip=%u.%u.%u.%u nm=%u.%u.%u.%u gw=%u.%u.%u.%u\r\n",
               (unsigned)mode,
               (unsigned)ip[0], (unsigned)ip[1], (unsigned)ip[2], (unsigned)ip[3],
               (unsigned)nm[0], (unsigned)nm[1], (unsigned)nm[2], (unsigned)nm[3],
               (unsigned)gw[0], (unsigned)gw[1], (unsigned)gw[2], (unsigned)gw[3]);
}

/* horn 모드 hook */
void app_lcd_hook_horn(bool down)
{
    mon_printf("[lcd-hook] horn down=%u\r\n", (unsigned)down);
    app_horn_set_mode(down);   /* SYS_HORN 모드 진입/이탈 (구 스텁 → 실배선 2026-07-18) */
}

/* model별 런 페이지 */
uint8_t app_lcd_run_page(const app_config_t *cfg)
{
    if      (cfg->model_type == 0) return LCD_RUN_HAND;    /* 3 */
    else if (cfg->model_type == 1) return LCD_RUN_MULTI;   /* 3 */
    else                           return LCD_RUN_STD;     /* 9 */
}

/* 모델명 문자열 전송 */
void app_lcd_send_model_str(uint8_t freq, uint8_t type)
{
    /* 브랜드는 define.h가 고른다 (samd20 main.c:2379-2586, 브랜드당 #ifdef 1블록).
     * Wire payload = 11 bytes incl. NUL at [10]. samd20은 freq/type이 범위를
     * 벗어나면 버퍼를 미초기화로 남겼다 — 포트는 default로 첫 케이스를 쓴다. */
    uint8_t s[11];
#if defined(GDSONIC) || defined(DIAMT)
    /* samd20 main.c:2379-2426 (GDS) / 2428-2475 (DIS) — 접두 3바이트만 다르다. */
#ifdef GDSONIC
    s[0] = 'G'; s[1] = 'D'; s[2] = 'S';
#else
    s[0] = 'D'; s[1] = 'I'; s[2] = 'S';
#endif
    s[3] = '-';
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
#elif defined(POWERTECH)
    /* samd20 main.c:2477-2530. 30K/35K 모두 " 735" (legacy 그대로). */
    s[0] = 'P'; s[1] = 'T'; s[2] = 'W'; s[3] = '-';
    switch (freq) {
        case 0:  s[4] = '2'; s[5] = '5'; s[6] = '1'; s[7] = '5'; break;   /* 15K */
        case 1:  s[4] = '2'; s[5] = '0'; s[6] = '2'; s[7] = '0'; break;   /* 20K */
        case 2:  s[4] = ' '; s[5] = '7'; s[6] = '3'; s[7] = '5'; break;   /* 30K */
        case 3:  s[4] = ' '; s[5] = '7'; s[6] = '3'; s[7] = '5'; break;   /* 35K */
        case 4:  s[4] = ' '; s[5] = '7'; s[6] = '4'; s[7] = '0'; break;   /* 40K */
        case 5:  s[4] = ' '; s[5] = '7'; s[6] = '5'; s[7] = '0'; break;   /* 50K */
        default: s[4] = '2'; s[5] = '5'; s[6] = '1'; s[7] = '5'; break;
    }
    switch (type) {
        case 0:  s[8] = 'D'; s[9] = 'H'; break;
        case 1:  s[8] = 'M'; s[9] = 'D'; break;
        case 2:  s[8] = 'S'; s[9] = 'D'; break;
        default: s[8] = 'D'; s[9] = 'H'; break;
    }
    s[10] = '\0';
#elif defined(MAKETECH)
    /* samd20에 대응 블록 없는 신규 브랜드. "SMT-" + type문자 + freq 2자리 + 'D'.
     *   hand "SMT-H15D" / multi "SMT-A15D" / std "SMT-S15D"
     * 패널 선택지는 15K/20K/35K. 나머지 freq 코드는 GDSONIC과 같이 실제 주파수를
     * 그대로 찍는다 (MOOHAN처럼 30K를 35로 접지 않음 — 접을 근거가 legacy에 없음). */
    s[0] = 'S'; s[1] = 'M'; s[2] = 'T'; s[3] = '-';
    switch (type) {
        case 0:  s[4] = 'H'; break;                                       /* hand */
        case 1:  s[4] = 'A'; break;                                       /* multi */
        case 2:  s[4] = 'S'; break;                                       /* standard */
        default: s[4] = 'H'; break;
    }
    switch (freq) {
        case 0:  s[5] = '1'; s[6] = '5'; break;
        case 1:  s[5] = '2'; s[6] = '0'; break;
        case 2:  s[5] = '3'; s[6] = '0'; break;
        case 3:  s[5] = '3'; s[6] = '5'; break;
        case 4:  s[5] = '4'; s[6] = '0'; break;
        case 5:  s[5] = '5'; s[6] = '0'; break;
        default: s[5] = '1'; s[6] = '5'; break;
    }
    s[7] = 'D'; s[8] = ' '; s[9] = ' '; s[10] = '\0';
#else   /* MOOHAN */
    /* samd20 main.c:2532-2585. 30K/35K 모두 "1535" (legacy 그대로). */
    s[0] = 'M'; s[1] = 'H'; s[2] = '-';
    switch (freq) {
        case 0:  s[3] = '1'; s[4] = '5'; s[5] = '1'; s[6] = '5'; break;   /* 15K */
        case 1:  s[3] = '1'; s[4] = '5'; s[5] = '2'; s[6] = '0'; break;   /* 20K */
        case 2:  s[3] = '1'; s[4] = '5'; s[5] = '3'; s[6] = '5'; break;   /* 30K */
        case 3:  s[3] = '1'; s[4] = '5'; s[5] = '3'; s[6] = '5'; break;   /* 35K */
        case 4:  s[3] = '1'; s[4] = '5'; s[5] = '4'; s[6] = '0'; break;   /* 40K */
        case 5:  s[3] = '1'; s[4] = '5'; s[5] = '5'; s[6] = '0'; break;   /* 50K */
        default: s[3] = '1'; s[4] = '5'; s[5] = '1'; s[6] = '5'; break;
    }
    switch (type) {
        case 0:  s[7] = 'D'; s[8] = 'H'; break;
        case 1:  s[7] = 'D'; s[8] = 'M'; break;
        case 2:  s[7] = 'D'; s[8] = 'S'; break;
        default: s[7] = 'D'; s[8] = 'H'; break;
    }
    s[9] = ' '; s[10] = '\0';
#endif
    dgus_write_bytes(MODEL_NAME, s, 11);
}

/* LCD 모드 초기화 */
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

/* 런 페이지 확인/재전송 */
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

/* LCD 주기 tick */
void app_lcd_tick(void)
{
    /* fault 표면: app_reg가 publish한 measure.error_status의 0→nonzero 엣지에
     * 경고 페이지 1회 진입(app_lcd_show_error→LCD_WARNING), nonzero→0 엣지에
     * 런 페이지 복귀(app_lcd_fault_cleared — REMOTE/물리 RESET 클리어 커버;
     * legacy는 각 RESET 핸들러가 직접 복귀, samd20 main.c:4356-4370/4605-4617).
     * state.error_status는 입력 레이어(KEY_ERROR_RESET)가 읽으므로 매 호출 미러.
     * 매 iter 호출(4ms 게이트는 disp_step 한정). */
    static uint8_t prev_err = 0u;
    uint8_t err = app_lcd_measure()->error_status;
    /* 터치 RESET 직후 ~1 iter 동안 stale publish가 state를 재기록할 수 있음 —
     * 모든 복귀 경로가 lcd_status==LCD_WARNING을 먼저 게이트하므로 무해. */
    app_lcd_state()->error_status = err;
    if ((err != 0u) && (prev_err == 0u)) {
        app_lcd_show_error(err);
    } else if ((err == 0u) && (prev_err != 0u)) {
        app_lcd_fault_cleared();
    }
    prev_err = err;

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
