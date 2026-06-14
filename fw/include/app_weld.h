/* fw/include/app_weld.h — Stage Weld-Cycle slice 1 glue: drives weld_fsm_step
 * every 10 ms from live config, turns out-events into the SOL_DN hook +
 * app_reg US_CYCLE commands + work_cnt persistence. Production trigger
 * (physical SW_START1/2) is slice 4; slice 1 exposes app_weld_request_start()
 * as the future join point (no production caller this slice). */
#pragma once
#include <stdbool.h>
#include <stdint.h>

void app_weld_init(void);            /* boot: reset FSM + tick gate */
void app_weld_tick(void);            /* superloop: 10 ms-gated advance + effects */
void app_weld_request_start(void);   /* one-shot cycle trigger (slice 4 caller) */

/* SOL_DN solenoid hook (slice 1: mon log; slice 4: PB5 GPIO). */
void app_weld_hook_sol_dn(bool on);

/* Weld amplitude hook — takes the comp_time-corrected RAW DAC value (0..127)
 * and writes it straight to I2C_POT (samd20 main.c:1549). slice 1: log only;
 * B-SEAM: real I2C_POT. Do NOT route through app_lcd_hook_set_pot — that one
 * takes output_power and re-converts (x-50)*255/100 = double-convert bug. */
void app_weld_hook_set_amp(uint8_t dac);
