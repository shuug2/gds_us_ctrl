/* fw/include/app_reg.h — Stage D regulation core: superloop step that acquires
 * the 2-ch ADC (1 ms pace), averages (10/50), scales, and runs the lookup at
 * the ~2 ms regulation cadence. Run gate: one M16-faithful boot warm-up
 * (~4 s, commands ignored), then RUN = immediate level-follow with a TOUCH/COMM
 * on-time ceiling (limit_on_time x10 ms). Owns the live lcd_measure_t the LCD
 * display reads. USOUT(PB4) driven from run state this slice; OSC drive
 * deferred — B-SEAM. */
#pragma once
#include <stdbool.h>
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
    int16_t  freq_cal_val;    /* FREQ_IN 표시 보정 → freq_fsm_compute (slice-B) */
    uint8_t  model_type;      /* 0=hand — legacy ceiling 게이트 (slice-D) */
    int16_t  cal_val;         /* ch1 표시 전류 보정 (config, ch1 slice) */
} reg_run_limits_t;

/* Superloop regulation step; gates internally on sys_tick_get_ms() deltas:
 * 1 ms both-channel ADC accumulate/average, ~2 ms scale + lookup on the
 * latest ch0_avg + lcd_measure_t publish, ~10 ms boot warm-up advance.
 * The run termination (30s 안전 ceiling / 운영 ceiling / energy/OVTIME)은 매
 * call 평가 (2ms gate 아님). model_type(0=hand)은 운영 limit_on_time ceiling을
 * hand 모드로 게이트(samd20-faithful); 절대 30 s 안전 ceiling은 모드 무관.
 * `lim`은 호출자 스택 임시값 — app_reg_tick은 동기 호출이라 보관하지 않음
 * (caller's stack lifetime sufficient; cpp-review N1). */
void app_reg_tick(const reg_run_limits_t *lim);

/* Live measured values for the LCD display machine (single owner). */
const lcd_measure_t *app_reg_measure(void);

/* Route an ultrasonic command into the run FSM. src = command source
 * (US_TOUCH from the panel hook, US_COMM from Modbus — samd20 us_run_status
 * taxonomy). START arms only from US_IDLE; RUN_RELEASE stops only the run its
 * own source started (samd20: comm STOP ==US_COMM, touch release ==US_TOUCH).
 * SEEK/RESET = app_seek_reset로 위임(D1 이후). Superloop single-thread —
 * mutates FSM state in place. us_cmd_t comes from the included app_lcd.h. */
void app_reg_command(us_cmd_t cmd, uint8_t src);

/* START가 지금 수락될 상태인가 — guard와 동일 조건의 읽기 전용 쿼리 (상태 무변경).
 * slice4 weld 글루가 사이클 진입 게이팅에 사용 (블라인드 사이클 차단, spec §4.3).
 * swallow_start는 TOUCH 전용 소비라 조건에서 제외 (US_CYCLE에 무관). */
bool app_reg_start_allowed(void);

/* OVTIME fault 상승 — weld(US_CYCLE) 에너지 backstop abort가 호출 (직접런
 * OVTIME과 같은 error_status|=ERR_OVTIME으로 통합; legacy RUN_WELD OVTIME
 * main.c:5292). publish/RESET 클리어는 기존 인프라가 커버. */
void app_reg_raise_ovtime(void);

/* run-output(USOUT) 전이 hook: us_run_status idle↔active 변화 시 호출.
 * 기본 구현이 io_usout 구동 (app_reg.c). */
void app_reg_hook_us_output(bool on);
