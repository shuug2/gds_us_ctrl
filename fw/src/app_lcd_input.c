/* fw/src/app_lcd_input.c — panel touch/key input dispatch (samd20 parse_lcd_comm port).
 *
 * Port source (read-only authoritative): ref/samd20/main.c parse_lcd_comm (3248-4135).
 * Spec: docs/superpowers/specs/2026-05-27-stage-lcd-full-behavior-port-design.md §3,§4,§7.
 *
 * Scope (Task 6): act only on DGUS_CMD_RD (0x83) touch/key reports. Config edits write
 * app_lcd_cfg() directly (samd20 immediate-to-live); transient/shadows write app_lcd_state().
 * DATA_SAVE / comm / ethernet 핸들러는 app_lcd_comm.c로 분할 (2026-07-19, 심=app_lcd_input_priv.h).
 *
 * Stage D owns us/measure state; this layer only RAISES the command via hooks — it does
 * NOT mutate us_run_status / sig_* / energy accumulators (those live behind the read-only
 * app_lcd_measure() provider and Stage D's FSM).
 */
#include "app_lcd.h"
#include "dgus_lcd.h"
#include "sys_tick.h"
#include "mon.h"
#include "cfg_clamp.h"
#include "app_input.h"     /* app_estop_active — 런 페이지 복귀 가드 */
#include "app_horn.h"      /* app_horn_mode_active — SETUP horn 체크박스 미러 */
#include "app_lcd_input_priv.h"   /* app_lcd_comm.c 분할 심 */

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

/* DATA_SAVE payload: 1 = SAVE, 0 = CANCEL (ref/samd20/main.c:3298,3511). */
#define SAVE_COMMIT  1u

/* comm/ether 전용 상수(COMM_*, ETHER_KEY_*)는 app_lcd_comm.c로 이동. */

/* Long-press hold threshold. samd20 KEY_HOLD_TH=200 ticks of the 10ms key timer
 * (ref/samd20/define.h:34) => 2000 ms wall-clock (clean STM32 equivalent). */
#define KEY_HOLD_MS  2000u

/*--------------------------------------------------------------
 * Multi-step branch helpers (keep the switch readable, <800 lines)
 *--------------------------------------------------------------*/

/* Resolve the RUN page id for the active sys_mode (samd20 model_type branch).
 * non-static: app_lcd_comm.c의 data_save_commit이 공유 (app_lcd_input_priv.h). */
uint8_t run_page_for_mode(uint8_t sys_mode)
{
    if (sys_mode == SYS_HAND)  return LCD_RUN_HAND;     /* 3 */
    if (sys_mode == SYS_MULTI) return LCD_RUN_MULTI;    /* 3 */
    return LCD_RUN_STD;                                 /* 9 (SYS_STD) */
}

/* slice4 SETUP 게이트: 현재 run 페이지인가 (setup/model 페이지면 0).
 * samd20 sys_status==SYS_RUN 등가 — 페이지 기반 판정 (sys_status 필드는 미배선). */
uint8_t app_lcd_in_run_page(void)
{
    const lcd_app_state_t *st = app_lcd_state();
    return (st->lcd_status == run_page_for_mode(st->sys_mode)) ? 1u : 0u;
}

/* 런 페이지 복귀 공용 가드 — 경고 페이지를 점유하는 조건이 남아 있으면 복귀 금지.
 * 점유 조건 = E-stop(라이브 접근자) + OVTIME 계열 error_status(reg 미러가 fault
 * 래치 동안 지속 공급 — app_lcd.c:193-200). OVLD는 아이콘-only(아래)라 비차단.
 * E-stop에 막혀 경고가 유지될 때는 호출측이 VP 텍스트를 "E-STOP"으로 재기록. */
static bool lcd_may_restore_run_page(void)
{
    return (app_estop_active() == 0u) &&
           (app_lcd_state()->error_status == 0u);
}

/* 과부하 에러 표시 — app_overload 글루가 assert/deassert 엣지에 호출.
 * legacy do_control(main.c:4218-4227): OVLD/OUTERR는 VP 텍스트+아이콘만 —
 * LCD_WARNING 페이지 전환은 OVTIME 전용(4229-4233). 런 화면 유지 + ICON_OL
 * (2026-07-05 벤치 실장비 거동으로 페이지 전환 편차 발견·정정). deassert 복귀는
 * 경고 페이지 표시 중일 때만 — 아이콘-only라 setup 등 임의 페이지 납치 금지. */
void app_lcd_set_overload(bool on)
{
    lcd_app_state_t *state = app_lcd_state();

    if (on) {
        state->error_status |= ERR_OVLD;
        dgus_write_text(VP_ERROR_MSG, "OVER LOAD");   /* 기록만 — 페이지 무전환 */
        dgus_write_u16(ICON_OL, 1);
    } else {
        state->error_status &= (uint8_t)~ERR_OVLD;
        dgus_write_u16(ICON_OL, 0);
        if (state->lcd_status == LCD_WARNING) {
            if (lcd_may_restore_run_page()) {
                state->lcd_status = run_page_for_mode(state->sys_mode);
                dgus_set_page(state->lcd_status);
            } else if (app_estop_active() != 0u) {
                dgus_write_text(VP_ERROR_MSG, "E-STOP");   /* 경고 유지 — 원인 갱신 */
            }
        }
    }
}

/* E-stop 표시 — app_input 글루가 enter/release 엣지에 호출. legacy는
 * sys_status=SYS_ESTOP 전이에서 VP_ERROR_MSG="E-STOP"+LCD_WARNING(do_control,
 * main.c:4209-4215), 해제 시 init_lcd_mode()로 런 페이지 복귀(4238-4241).
 * E-stop은 error_status 비트가 아님(레벨추종) — 비트 무조작. 복귀는 다른 에러
 * 활성 시 보류(overload off 경로와 동일 가드; legacy는 sys_status 단일값이라
 * 등가 상황 없음). */
void app_lcd_set_estop(bool on)
{
    lcd_app_state_t *state = app_lcd_state();

    if (on) {
        /* 동시 에러(OVLD 등) 활성이어도 텍스트는 E-STOP 우선 (최상위 안전 상태). */
        dgus_write_text(VP_ERROR_MSG, "E-STOP");
        state->lcd_status = LCD_WARNING;
        dgus_set_page(state->lcd_status);
        app_lcd_input_run_key_reanchor();   /* 홀드 중 페이지 이탈 → 토글 반전 방지 */
    } else if (lcd_may_restore_run_page()) {
        state->lcd_status = run_page_for_mode(state->sys_mode);
        dgus_set_page(state->lcd_status);
    }
}

/* fault 클리어 표면 — app_lcd_tick 미러가 measure.error_status nonzero→0
 * 엣지에 호출. legacy는 Modbus/물리 RESET 핸들러가 직접 런 페이지를 복귀
 * (samd20 main.c:4356-4370 / 4605-4617 set_lcd_page) — 이 포트에서는 클리어
 * 소스와 무관하게 이 한 지점이 커버한다(REMOTE/물리 B_RESET; KEY_ERROR_RESET
 * 터치 경로는 이미 복귀해 lcd_status != LCD_WARNING → no-op). 경고 페이지일
 * 때만 복귀(임의 페이지 납치 금지, overload off 경로와 동일 가드), E-stop
 * 활성이면 보류(레벨 해제 시 app_lcd_set_estop(false)가 복귀). */
void app_lcd_fault_cleared(void)
{
    lcd_app_state_t *state = app_lcd_state();

    if (state->lcd_status != LCD_WARNING) {
        return;
    }
    if (lcd_may_restore_run_page()) {
        state->lcd_status = run_page_for_mode(state->sys_mode);
        dgus_set_page(state->lcd_status);
    } else if (app_estop_active() != 0u) {
        dgus_write_text(VP_ERROR_MSG, "E-STOP");   /* 경고 유지 — 원인 갱신 */
    }
}

/* Resolve the setup-page-1 id for the active sys_mode (SETUP_PARAM / _MOOHAN). */
static uint8_t setup1_page_for_mode(uint8_t sys_mode)
{
    if (sys_mode == SYS_HAND)  return LCD_SETUP_HAND;   /* 7 */
    if (sys_mode == SYS_MULTI) return LCD_SETUP_MULTI;  /* 5 */
    return LCD_SETUP_STD1;                              /* 10 (SYS_STD) */
}

/* Physical RUN-key state reconstructed from the V30 data=0 edge stream (one
 * event per physical edge, alternating press/release — HW-traced 2026-06-08).
 * Zero-init assumes the key is up at boot (same assumption as the SETUP_MODEL
 * long-press pairing below). Reset on SYS_PIC_NOW re-init: a panel reset
 * means the release edge will never arrive (§4.4). */
static uint8_t s_run_key_down;

/* 런 페이지를 떠나는 페이지 전환(경고: show_error/E-stop)에서 호출 — 눌린 채
 * 페이지가 바뀌면 V30 런-키 컨트롤이 사라져 release data=0 이벤트가 영영 안
 * 오고, 토글이 "눌림"에 고착돼 press↔release가 반전됨(2026-07-18 에너지 모드
 * OVTIME 중 홀드 벤치 증상: 떼면 START). SYS_PIC_NOW 패널-리셋 처리와 동일
 * 패턴: RUN_RELEASE(IDLE이면 no-op + 자동정지가 무장한 swallow_start 정리 —
 * 안 하면 fault 후 첫 탭 1회 무시) + 토글 재앵커. */
void app_lcd_input_run_key_reanchor(void)
{
    app_lcd_hook_us_command(US_CMD_RUN_RELEASE);
    s_run_key_down = 0u;
}

/* KEY_MULTI (0x1080): 1=RESET / 2=SEEK / 3=RUN(press) / 4=RUN(release); the V30
 * DGUS asset additionally returns 0=RUN on both edges (toggle-mapped — §4.4).
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
            if (lcd_may_restore_run_page()) {
                state->lcd_status = run_page_for_mode(state->sys_mode);
                dgus_set_page(state->lcd_status);       /* samd20 uses set_lcd_page (no rebuild) */
            } else if (app_estop_active() != 0u) {
                dgus_write_text(VP_ERROR_MSG, "E-STOP");   /* 경고 유지 — 원인 갱신 */
            }
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
    } else if (data16 == 0) {                           /* RUN (V30 panel: key value 0 on both edges) */
        /* The V30 DGUS asset returns KEY_MULTI=0 on BOTH press and release for the
         * RUN button (RESET=1/SEEK=2 are correct; data=0 is unique to RUN, HW-traced
         * 2026-06-08). Each data=0 event IS one physical edge, so the s_run_key_down
         * toggle reconstructs the press/release pairing exactly. Mapping by the live
         * run state instead (pre-2026-07-08) inverted the pairing whenever app_reg
         * silently rejected the mapped START (boot warm-up ~4 s, seek/reset chain,
         * E-stop/overload/fault, back-to-back frames in one drain): the physical
         * release then re-mapped to START and began an un-held run that nothing
         * released (30 s safety cap only), and every further tap stop-then-
         * restarted it — RUN looked dead until power cycle. With the toggle a
         * rejected press simply pairs with a no-op RELEASE-while-IDLE (which also
         * clears any armed swallow_start). Only a lost or duplicated edge frame
         * can drift the toggle (the HW trace saw neither: one event per edge, no
         * auto-repeat); SYS_PIC_NOW re-init (panel reset) re-anchors it. The legacy
         * data=3/4 branches above stay for forward-compat if the asset is later
         * fixed to send them. See spec §4.4. */
        s_run_key_down ^= 1u;
        if (s_run_key_down != 0u) {
            app_lcd_hook_us_command(US_CMD_START);
            app_lcd_hook_set_pot(cfg->output_power);    /* DAC on run start (stub, F2) */
        } else {
            app_lcd_hook_us_command(US_CMD_RUN_RELEASE);
        }
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
        if (lcd_may_restore_run_page()) {
            state->lcd_status = run_page_for_mode(state->sys_mode);
            dgus_set_page(state->lcd_status);           /* samd20 uses set_lcd_page (no rebuild) */
        } else if (app_estop_active() != 0u) {
            dgus_write_text(VP_ERROR_MSG, "E-STOP");    /* 경고 유지 — 원인 갱신 */
        }
    }
}

/* SETUP_MODEL / SETUP_PARAM_MOOHAN long-press FSM.
 *
 * samd20 (main.c) assumed the panel reports data==0 on touch-down and data==2
 * on touch-up, timing the release to detect a >= KEY_HOLD_MS hold. HW-verify
 * (2026-05-27) found this panel's 0x1084 button instead emits data==0 on BOTH
 * down AND up (one event each, no auto-repeat) and NEVER data==2 — so the
 * verbatim port could never complete a long-press (the release event was
 * misread as a fresh press). See docs/superpowers/analysis/2026-05-27-lcd-
 * setup-model-longpress.md.
 *
 * Fix: pair consecutive same-VP data==0 events. The first arms the press
 * (records key_press_ms + key_press_vp); the second is the release and fires
 * if held >= KEY_HOLD_MS. data==2 is still honoured as an explicit release for
 * any button/panel that does send it (backward compatible). key_press_vp is
 * keyed by VP so the two long-press buttons (0x1084 / 0x1094) don't interfere.
 * Returns true only on a qualifying (long) release. */
static bool long_press_released(uint16_t vp, uint16_t data16)
{
    lcd_app_state_t *state = app_lcd_state();

    if (data16 == 0) {
        if (state->key_press_vp != vp) {                /* touch-down: arm this VP */
            state->key_press_vp = vp;
            state->key_press_ms = sys_tick_get_ms();
            return false;
        }
        /* touch-up (release reported as value 0): same VP already armed */
        state->key_press_vp = 0;
        return (uint32_t)(sys_tick_get_ms() - state->key_press_ms) >= KEY_HOLD_MS;
    }

    if (data16 == 2) {                                  /* explicit release (legacy / other panels) */
        bool fired = (state->key_press_vp == vp) &&
                     (uint32_t)(sys_tick_get_ms() - state->key_press_ms) >= KEY_HOLD_MS;
        state->key_press_vp = 0;
        return fired;
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

/* comm/ether shadow 편집 + DATA_SAVE commit/rollback → app_lcd_comm.c (분할). */
/* M4: 클램프 + 패널 에코 (클램프 발동 시에만 재전송 — LV_MO_TIME 관례). */
static uint16_t clamp_echo_max(uint16_t vp, uint16_t v, uint16_t max)
{
    uint16_t c = cfg_clamp_max(v, max);
    if (c != v) { dgus_write_u16(vp, c); }
    return c;
}
static uint16_t clamp_echo_power(uint16_t vp, uint16_t v)
{
    uint16_t c = cfg_clamp_power(v);
    if (c != v) { dgus_write_u16(vp, c); }
    return c;
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

    /* Payload must carry READ_LEN + DATA_H + DATA_L — a shorter (noise-truncated)
     * frame would read stale/uninitialised data[1..2]. Same guard as
     * dgus_read_word() in dgus_lcd.c. */
    if (f->data_len < 3u) {
        return;
    }

    /* DGUS 0x83 read-response payload after the VP address is:
     *   data[0] = READ_LEN (word count, 0x01 for a 1-word read)
     *   data[1] = DATA_H,  data[2] = DATA_L
     * So skip the READ_LEN byte — the value is data[1]:data[2] (big-endian).
     * (samd20 ref/dgus_lcd.h: DATA_H=5/DATA_L=6 skip the offset-4 READ_LEN;
     *  the prior data[0]:data[1] read mistook READ_LEN for the value MSB,
     *  making SAVE(=1) read as 256 → CANCEL, and all edits store garbage.) */
    uint16_t vp     = f->vp_addr;
    uint16_t data16 = (uint16_t)(((uint16_t)f->data[1] << 8) | f->data[2]);

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
        cfg->limit_delay_time1 = clamp_echo_max(LV_DM_DELAY, data16, 500u);
        break;
    case LV_DM_WELD:
        cfg->limit_delay_time2 = clamp_echo_max(LV_DM_WELD, data16, 500u);
        break;
    case LV_DM_HOLD:
        cfg->limit_delay_time3 = clamp_echo_max(LV_DM_HOLD, data16, 2000u);
        break;
    case LV_TM_WELD:
        cfg->limit_trigger_time2 = clamp_echo_max(LV_TM_WELD, data16, 500u);
        break;
    case LV_TM_HOLD:
        cfg->limit_trigger_time3 = clamp_echo_max(LV_TM_HOLD, data16, 2000u);
        break;

    /*--- multi-output limits -----------------------------------------------*/
    case LV_MO_OUT1:
        cfg->limit_mo_out1 = clamp_echo_power(LV_MO_OUT1, data16);
        break;
    case LV_MO_OUT2:
        cfg->limit_mo_out2 = clamp_echo_power(LV_MO_OUT2, data16);
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
         * + setup-page entry (spec §7 fidelity; samd20 main.c:3807-3813).
         * M4+LOW-1: [50,100]. */
        cfg->output_power = (uint8_t)clamp_echo_power(LV_OUT_POWER, data16);
        break;
    case LV_MAX_ON_TIME:
        cfg->limit_on_time = clamp_echo_max(LV_MAX_ON_TIME, data16, 2000u);
        break;
    case LV_ENERGY_EDIT:
        cfg->limit_energy = data16;
        break;
    case LV_LIMIT_OUT_T:
        cfg->limit_out_time = (uint8_t)clamp_echo_max(LV_LIMIT_OUT_T, data16, 10u);
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
        /* horn-down 체크박스 = 현재 SYS_HORN 모드 미러 + shadow 리셋 (legacy
         * main.c:3617-3622 verbatim — 저장 시 체크 안 건드리면 temp==0이라
         * 모드 이탈되는 legacy 거동 포함). */
        dgus_write_u16(DISP_HORNDOWN, (uint16_t)app_horn_mode_active());
        state->temp_horndown = 0u;
        break;
    case SETUP_PARAM_MOOHAN:                             /* long-press variant of SETUP_PARAM */
        if (long_press_released(vp, data16)) {
            state->lcd_status = setup1_page_for_mode(state->sys_mode);
            app_lcd_change_page(state->lcd_status);
            dgus_write_u16(DISP_HORNDOWN, (uint16_t)app_horn_mode_active());
            state->temp_horndown = 0u;                   /* legacy 3617-3622 미러 */
        }
        break;
    case SETUP_MODEL:                                    /* long-press → model setup */
        if (long_press_released(vp, data16)) {
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
            /* Panel self-reset mid-run: the held RUN press is lost and no
             * RUN_RELEASE will arrive, so stop the run (UI lost -> stop the
             * actuator). This also re-syncs ICON_RUN: us_run_status -> IDLE
             * makes the next disp_step see a real edge after init_mode clears
             * the icon (spec §4.3). Harmless when already idle. */
            app_lcd_hook_us_command(US_CMD_RUN_RELEASE);
            s_run_key_down = 0u;   /* panel reset: the release edge never arrives */
            app_lcd_var_init();
            app_lcd_send_model_str(cfg->model_freq, cfg->model_type);
            app_lcd_init_mode(cfg);
        }
        break;

    default:
        break;
    }
}
