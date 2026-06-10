/* fw/include/app_reg.h — Stage D regulation core: superloop step that acquires
 * the 2-ch ADC (1 ms pace), averages (10/50), scales, and runs the lookup at
 * the ~2 ms regulation cadence. Run gate: one M16-faithful boot warm-up
 * (~4 s, commands ignored), then RUN = immediate level-follow with a TOUCH
 * on-time ceiling (limit_on_time x10 ms). Owns the live lcd_measure_t the LCD
 * display reads. NO physical output yet (OSC drive deferred — B-SEAM). */
#pragma once
#include <stdint.h>
#include "app_lcd.h"   /* lcd_measure_t */

/* Init regulation state + start ADC1 + arm the one-shot boot warm-up.
 * Call once at boot (after sys_tick). */
void app_reg_init(void);

/* Superloop regulation step; gates internally on sys_tick_get_ms() deltas:
 * 1 ms both-channel ADC accumulate/average, ~2 ms scale + lookup on the
 * latest ch0_avg + lcd_measure_t publish, ~10 ms boot warm-up advance. */
void app_reg_tick(void);

/* Live measured values for the LCD display machine (single owner). */
const lcd_measure_t *app_reg_measure(void);

/* Route a panel/comm ultrasonic command into the run FSM (slice 2b). Called from
 * app_lcd_hook_us_command. START/RUN_RELEASE gate the run; SEEK/RESET = no-op this
 * slice (deferred, spec §9). Superloop single-thread — mutates FSM state in place.
 * us_cmd_t is visible via the already-included app_lcd.h. */
void app_reg_command(us_cmd_t cmd);
