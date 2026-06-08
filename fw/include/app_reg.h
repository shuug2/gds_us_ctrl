/* fw/include/app_reg.h — Stage D slice 1 regulation core: ~2 ms superloop step
 * that acquires the 2-ch ADC, averages (10/50), scales, and runs the lookup.
 * Owns the live lcd_measure_t the LCD display reads. NO physical output this
 * slice (OSC drive deferred — spec §9, B-SEAM). */
#pragma once
#include <stdint.h>
#include "app_lcd.h"   /* lcd_measure_t */

/* Init regulation state + start ADC1. Call once at boot (after sys_tick). */
void app_reg_init(void);

/* ~2 ms regulation step; gate internally on sys_tick_get_ms() delta.
 * One ADC conversion (alternating channel) per fire, accumulate/average,
 * then scale + lookup on the latest ch0_avg, then publish lcd_measure_t. */
void app_reg_tick(void);

/* Live measured values for the LCD display machine (single owner). */
const lcd_measure_t *app_reg_measure(void);

/* Route a panel/comm ultrasonic command into the run FSM (slice 2b). Called from
 * app_lcd_hook_us_command. START/RUN_RELEASE gate the run; SEEK/RESET = no-op this
 * slice (deferred, spec §9). Superloop single-thread — mutates FSM state in place.
 * us_cmd_t is visible via the already-included app_lcd.h. */
void app_reg_command(us_cmd_t cmd);
