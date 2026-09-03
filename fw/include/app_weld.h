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
void app_weld_abort_now(void);       /* E-stop 진입 엣지 즉시 abort — run-page 게이트
                                        무관 (app_input 호출; legacy main.c:1409-1425) */

/* SENSE_DN 라이브 상태 (1 = 감지 중). Modbus STATUS 의 SENSOR 비트가 소비한다
 * (요구사항 B-3). app_weld_tick 안의 LCD 표시 미러는 horn/SETUP 게이트 뒤라
 * 그 구간에서 동결되지만, 이 접근자는 게이트와 무관하게 즉시 읽는다 —
 * 원격 관측은 화면이 어느 페이지에 있든 같은 답을 줘야 한다. */
uint8_t app_weld_sensor_active(void);

/* SOL_DN solenoid hook (slice 1: mon log; slice 4: PB5 GPIO). */
void app_weld_hook_sol_dn(bool on);

/* Weld amplitude hook — takes the comp_time-corrected RAW DAC value (0..127)
 * and writes it straight to I2C_POT (samd20 main.c:1549). slice 1: log only;
 * B-SEAM: real I2C_POT. Do NOT route through app_lcd_hook_set_pot — that one
 * takes output_power and re-converts (x-50)*255/100 = double-convert bug. */
void app_weld_hook_set_amp(uint8_t dac);

/* WELD backstop abort hook — energy_ctrl 모드에서 limit_out_time 안에 에너지
 * 미도달 시 1회. slice2: mon 로그만(저에너지=불량 용접 표시). 후속 에러 슬라이스가
 * SYS_ERROR | ERR_OVTIME + LCD 에러 표시로 배선. spec §7. */
void app_weld_hook_fault(void);
