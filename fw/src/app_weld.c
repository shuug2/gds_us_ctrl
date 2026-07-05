/* fw/src/app_weld.c — weld-cycle glue (slice 1~4). 10 ms-gated FSM advance;
 * out-events -> SOL_DN hook + app_reg US_CYCLE + work_cnt FRAM/LCD. slice 4
 * adds the SETUP-page gate (run 페이지에서만 진행) + 물리 양손 트리거 스캔/
 * 사이클 진입 게이팅 + abort 합성 (spec §4, §5.4). */
#include "app_weld.h"
#include "app_weld_fsm.h"
#include "app_weld_trigger_fsm.h"  /* weld_trig_in_t/out_t, weld_trigger_fsm_* (slice4) */
#include "app_lcd.h"      /* app_lcd_cfg, app_lcd_set_work_cnt, app_lcd_in_run_page, US_CYCLE, us_cmd_t */
#include "app_reg.h"      /* app_reg_command, app_reg_measure, app_reg_start_allowed */
#include "app_config.h"   /* app_config_t, app_config_save_all */
#include "app_input.h"    /* app_estop_active (slice4 abort 합성) */
#include "app_overload.h" /* app_overload_active (slice4 abort 합성) */
#include "i2c_pot.h"      /* i2c_pot_set_dac (U4 진폭, raw DAC) */
#include "sys_tick.h"
#include "mon.h"
#include "io.h"

#define WELD_TICK_MS  10u   /* samd20 temp_time-- cadence */

static uint32_t s_prev_ms;
static uint8_t  s_start_pending;   /* one-shot latch (slice4: 트리거 FSM start_pulse가 채움) */
static uint8_t  s_sol_last;        /* SOL_DN level edge tracking */
static uint8_t  s_sens_dn_bak;     /* SENSOR ON/OFF 표시용 레벨 bak (legacy re_dn_bak) */

void app_weld_init(void)
{
    weld_fsm_init();
    weld_trigger_fsm_init();
    s_prev_ms       = sys_tick_get_ms();
    s_start_pending = 0u;
    s_sol_last      = 0u;
    s_sens_dn_bak   = io_read_sens_dn();   /* io_init 선행(main.c) — 현재 레벨 기준 */
}

void app_weld_request_start(void)
{
    s_start_pending = 1u;          /* consumed next tick; READY-only in core */
}

/* E-stop 진입 엣지 전용 즉시 abort (app_input 글루가 호출) — run-page 게이트와
 * 무관. legacy는 EMSW 핸들러가 입력 스캔에서 직접 SOL/M_START를 끊고 해제 시
 * RUN_READY를 강제(main.c:1409-1425)한 구조의 등가. E-stop 진입이 LCD_WARNING
 * 페이지 전환(app_lcd_set_estop)을 동반하면 아래 tick이 SETUP 게이트로 동결돼
 * abort 미처리→해제 후 유령 재개가 되므로, 여기서 페이지 무관으로 정리한다.
 * abort 경로는 in.abort+현재 상태만 읽음(app_weld_fsm.c:119-137) — zeroed in 안전. */
void app_weld_abort_now(void)
{
    weld_in_t  in = {0};
    weld_out_t out;

    in.abort = 1u;
    weld_fsm_step(&in, &out);

    if (out.sol_dn != s_sol_last) {
        s_sol_last = out.sol_dn;
        app_weld_hook_sol_dn(out.sol_dn != 0u);
    }
    if (out.weld_stop) {
        app_reg_command(US_CMD_RUN_RELEASE, (uint8_t)US_CYCLE);
    }
    /* cycle_done/weld_start/amp_change는 abort 경로에서 미발행. */
}

void app_weld_hook_sol_dn(bool on)
{
    io_sol_dn(on);                 /* PB5 active-LOW (SOL_ON=LOW) */
    mon_printf("[weld] SOL_DN %s\r\n", on ? "ON" : "OFF");
}

void app_weld_hook_set_amp(uint8_t dac)
{
    /* comp_time-corrected RAW DAC (0..127) straight to I2C_POT (samd20
     * main.c:1549). NOT app_lcd_hook_set_pot — that takes output_power and
     * re-converts (x-50)*255/100 = double-convert bug. */
    i2c_pot_set_dac(dac);
    mon_printf("[weld] set_amp dac=%u\r\n", (unsigned)dac);
}

void app_weld_hook_fault(void)
{
    mon_printf("[weld] fault: energy timeout (backstop abort)\r\n");
}

void app_weld_tick(void)
{
    uint32_t now = sys_tick_get_ms();
    if ((uint32_t)(now - s_prev_ms) < WELD_TICK_MS) {
        return;
    }
    /* 감사 M1(글루): += 로 드리프트 무누적 (실 공압 dwell 정밀도, samd20 timer ISR
     * 10ms 주기 재현). 장기 정지(>10 tick 밀림) 후엔 재동기 — catch-up 폭주 방지. */
    s_prev_ms += WELD_TICK_MS;
    if ((uint32_t)(now - s_prev_ms) > 10u * WELD_TICK_MS) {
        s_prev_ms = now;
    }

    /* SETUP 게이트 (slice1 spec §5.4 이연분): setup 페이지에선 step 스킵 =
     * 사이클 타이머 동결 + 시작 불가 (samd20 timer의 sys_status!=SETUP 게이트).
     * 의도된 legacy-충실 거동: 동결 중엔 abort도 미처리 — E-stop은 app_input이
     * 독립 SOL OFF(app_input.c:46)로 커버하지만, overload/OVTIME은 US만 독립
     * 정지(app_overload는 SOL 미해제) → mid-cycle SOL은 run 페이지 복귀 시
     * 레벨-기반 abort가 해제 (리뷰 반영, 결정=plan 순서 유지+문서화). */
    if (app_lcd_in_run_page() == 0u) {
        return;
    }

    app_config_t *cfg = app_lcd_cfg();

    /* 물리 트리거/센서 스캔 (slice4). */
    weld_trig_in_t tin = {
        .key1       = io_read_key1(),
        .key2       = io_read_key2(),
        .sens_up    = io_read_sens_up(),
        .sens_dn    = io_read_sens_dn(),
        .f_safty    = cfg->f_safty,
        .weld_state = weld_fsm_status(),
    };
    weld_trig_out_t tout;
    weld_trigger_fsm_step(&tin, &tout);

    /* TRIGGER 모드 SENSOR ON/OFF 표시 미러 — SENSE_DN 레벨 변화 양쪽 엣지, 라이브
     * run_mode (samd20 check_remote_input main.c:1230-1265; bak 갱신은 모드 무관).
     * 페이지 진입 초기값은 app_lcd_change_page("SENSOR OFF")가 담당. deviation:
     * legacy 스캔은 페이지 무관이지만 이 코드는 SETUP 게이트(위) 뒤라 setup 중
     * 동결 — run 페이지 복귀 시 change_page 초기값이 재기준선. */
    if (tin.sens_dn != s_sens_dn_bak) {
        if (cfg->run_mode != 0u) {
            app_lcd_weld_sensor_text(tin.sens_dn == 0u);   /* active-LOW: 0=감지=ON */
        }
        s_sens_dn_bak = tin.sens_dn;
    }

    /* 사이클 진입 게이팅 (spec §4.3, 의도된 deviation): US START가 거부될 상태면
     * 사이클 자체를 시작하지 않음 — SOL만 하강하는 블라인드 사이클 차단. */
    if ((tout.start_pulse != 0u) &&
        (weld_fsm_status() == (uint8_t)WELD_READY) &&
        app_reg_start_allowed()) {
        s_start_pending = 1u;
        weld_trigger_fsm_cycle_started();
    }

    /* abort 합성 (spec §3.4/§4.2): E-stop/overload/fault + f_safty CYL1 release.
     * US 정지는 slice-c/d force-stop과 이중 안전. */
    uint8_t abort_now =
        ((app_estop_active() != 0u) ||
         (app_overload_active() != 0u) ||
         (app_reg_measure()->error_status != 0u) ||
         (tout.safety_abort_pulse != 0u)) ? 1u : 0u;

    /* M2(감사 D2): mo_out cast 전 [50,100] 클램프 — 상류(LCD/Modbus) 클램프와
     * belt-and-braces (uint16->uint8 절단 silent 진폭0 차단). */
    uint16_t mo1 = cfg->limit_mo_out1, mo2 = cfg->limit_mo_out2;
    if (mo1 > 100u) { mo1 = 100u; } else if (mo1 < 50u) { mo1 = 50u; }
    if (mo2 > 100u) { mo2 = 100u; } else if (mo2 < 50u) { mo2 = 50u; }

    weld_in_t in = {
        .start               = s_start_pending,
        .run_mode            = cfg->run_mode,
        .limit_delay_time1   = cfg->limit_delay_time1,
        .limit_delay_time2   = cfg->limit_delay_time2,
        .limit_delay_time3   = cfg->limit_delay_time3,
        .limit_trigger_time2 = cfg->limit_trigger_time2,
        .limit_trigger_time3 = cfg->limit_trigger_time3,
        .output_power        = cfg->output_power,
        .energy_ctrl         = cfg->energy_ctrl ? 1u : 0u,
        .limit_energy        = cfg->limit_energy,
        .limit_out_time      = cfg->limit_out_time,
        .curr_energy         = app_reg_measure()->curr_energy,
        .multi_ctrl          = cfg->multi_ctrl ? 1u : 0u,
        .limit_mo_out1       = (uint8_t)mo1,
        .limit_mo_out2       = (uint8_t)mo2,
        .limit_mo_time1      = cfg->limit_mo_time1,
        .limit_mo_time2      = cfg->limit_mo_time2,
        .dn_edge             = tout.dn_edge,
        .up_edge             = tout.up_edge,
        .abort               = abort_now,
    };
    s_start_pending = 0u;

    weld_out_t out;
    weld_fsm_step(&in, &out);

    if (out.sol_dn != s_sol_last) {
        s_sol_last = out.sol_dn;
        app_weld_hook_sol_dn(out.sol_dn != 0u);
    }
    if (out.weld_start) {
        app_weld_hook_set_amp(out.amplitude);   /* raw DAC, NOT set_pot (double-convert) */
        app_reg_command(US_CMD_START, (uint8_t)US_CYCLE);
    }
    if (out.amp_change) {
        app_weld_hook_set_amp(out.amplitude);   /* mid-WELD 2단 진폭 (US_CYCLE 유지, START 아님) */
    }
    if (out.weld_stop) {
        app_reg_command(US_CMD_RUN_RELEASE, (uint8_t)US_CYCLE);
    }
    if (out.weld_fault) {
        app_weld_hook_fault();
    }
    if (out.cycle_done) {
        cfg->work_cnt++;
        app_config_save_all(cfg);
        app_lcd_set_work_cnt(cfg->work_cnt);
    }
}
