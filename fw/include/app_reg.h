/* fw/include/app_reg.h — Stage D regulation core: superloop step that acquires
 * the 2-ch ADC (1 ms pace), averages (10/50), scales, and runs the lookup at
 * the ~2 ms regulation cadence. Run gate: one M16-faithful boot warm-up
 * (~4 s, commands ignored), then RUN = immediate level-follow with a TOUCH/COMM
 * on-time ceiling (limit_on_time x10 ms). Owns the live lcd_measure_t the LCD
 * display reads. NO physical output yet (OSC drive deferred — B-SEAM). */
#pragma once
#include <stdint.h>
#include "app_lcd.h"   /* lcd_measure_t */

/* Init regulation state + start ADC1 + arm the one-shot boot warm-up.
 * Call once at boot (after sys_tick). */
void app_reg_init(void);

/* app_reg_tick per-call injected live config (cpp-review M1: app_reg must not
 * call back into app_lcd). All fields read from g_cfg each superloop iter so
 * panel/Modbus edits apply immediately (incl. mid-run). energy_ctrl gates the
 * energy-mode termination (에너지-도달 정상정지 + OVTIME)이 on-time ceiling을
 * 대체(legacy main.c:5270 분기); 비-energy면 기존 ceiling. */
typedef struct {
    uint16_t limit_on_time;   /* x10 ms; 0 = ceiling off (비-energy 경로) */
    uint8_t  energy_ctrl;     /* 1 = energy 모드 (on-time ceiling 대체) */
    uint32_t limit_energy;    /* 에너지-도달 정상정지 임계 (curr_energy 비교) */
    uint16_t limit_out_time;  /* OVTIME 한계 = 초 (0 = OVTIME off) */
} reg_run_limits_t;

/* Superloop regulation step; gates internally on sys_tick_get_ms() deltas:
 * 1 ms both-channel ADC accumulate/average, ~2 ms scale + lookup on the
 * latest ch0_avg + lcd_measure_t publish, ~10 ms boot warm-up advance.
 * The run termination (ceiling 또는 energy/OVTIME)은 매 call 평가 (2ms gate 아님).
 * `lim`은 호출자 스택 임시값 — app_reg_tick은 동기 호출이라 보관하지 않음
 * (caller's stack lifetime sufficient; cpp-review N1). */
void app_reg_tick(const reg_run_limits_t *lim);

/* Live measured values for the LCD display machine (single owner). */
const lcd_measure_t *app_reg_measure(void);

/* Route an ultrasonic command into the run FSM. src = command source
 * (US_TOUCH from the panel hook, US_COMM from Modbus — samd20 us_run_status
 * taxonomy). START arms only from US_IDLE; RUN_RELEASE stops only the run its
 * own source started (samd20: comm STOP ==US_COMM, touch release ==US_TOUCH).
 * SEEK/RESET = no-op this slice (deferred, spec §9). Superloop single-thread —
 * mutates FSM state in place. us_cmd_t comes from the included app_lcd.h. */
void app_reg_command(us_cmd_t cmd, uint8_t src);
