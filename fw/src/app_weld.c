/* fw/src/app_weld.c — weld-cycle glue (slice 1). 10 ms-gated FSM advance;
 * out-events -> SOL_DN hook + app_reg US_CYCLE + work_cnt FRAM/LCD. No SETUP
 * gate this slice (no running cycle on HW — slice 4, spec §5.4). */
#include "app_weld.h"
#include "app_weld_fsm.h"
#include "app_lcd.h"      /* app_lcd_cfg, app_lcd_set_work_cnt, US_CYCLE, us_cmd_t */
#include "app_reg.h"      /* app_reg_command, US_CMD_START/RUN_RELEASE */
#include "app_config.h"   /* app_config_t, app_config_save_all */
#include "sys_tick.h"
#include "mon.h"

#define WELD_TICK_MS  10u   /* samd20 temp_time-- cadence */

static uint32_t s_prev_ms;
static uint8_t  s_start_pending;   /* one-shot latch (slice 4 caller) */
static uint8_t  s_sol_last;        /* SOL_DN level edge tracking */

void app_weld_init(void)
{
    weld_fsm_init();
    s_prev_ms       = sys_tick_get_ms();
    s_start_pending = 0u;
    s_sol_last      = 0u;
}

void app_weld_request_start(void)
{
    s_start_pending = 1u;          /* consumed next tick; READY-only in core */
}

void app_weld_hook_sol_dn(bool on)
{
    mon_printf("[weld] SOL_DN %s\r\n", on ? "ON" : "OFF");
}

void app_weld_hook_set_amp(uint8_t dac)
{
    /* comp_time-corrected RAW DAC (0..127) straight to I2C_POT (samd20
     * main.c:1549). slice 1: log only. NOT app_lcd_hook_set_pot — that takes
     * output_power and re-converts (x-50)*255/100 = double-convert bug. */
    mon_printf("[weld] set_amp dac=%u\r\n", (unsigned)dac);
}

void app_weld_tick(void)
{
    uint32_t now = sys_tick_get_ms();
    if ((uint32_t)(now - s_prev_ms) < WELD_TICK_MS) {
        return;
    }
    s_prev_ms = now;

    app_config_t *cfg = app_lcd_cfg();
    weld_in_t in = {
        .start             = s_start_pending,
        .run_mode          = cfg->run_mode,
        .limit_delay_time1 = cfg->limit_delay_time1,
        .limit_delay_time2 = cfg->limit_delay_time2,
        .limit_delay_time3 = cfg->limit_delay_time3,
        .output_power      = cfg->output_power,
    };
    s_start_pending = 0u;          /* one-shot consumed */

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
    if (out.weld_stop) {
        app_reg_command(US_CMD_RUN_RELEASE, (uint8_t)US_CYCLE);
    }
    if (out.cycle_done) {
        cfg->work_cnt++;
        app_config_save_all(cfg);
        app_lcd_set_work_cnt(cfg->work_cnt);
    }
}
