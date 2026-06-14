/* fw/include/app_weld_fsm.h — Stage Weld-Cycle slice 1: HAL-free pure FSM core
 * for the samd20 pneumatic press weld cycle (READY→CYL1→WELD→HOLD→CYL2). DELAY
 * mode only this slice (time-based, 10ms/count); energy_ctrl/multi_ctrl/TRIGGER
 * = later slices. No HAL, no globals reached — host-tested (fw/test). The glue
 * (app_weld.c) calls weld_fsm_step() every 10ms and turns out-events into hooks
 * + app_reg US_CYCLE commands + work_cnt. */
#pragma once
#include <stdint.h>

/* run_status (samd20 main.c verbatim: RUN_READY..RUN_CYL2) */
enum {
    WELD_READY = 0,
    WELD_CYL1  = 1,
    WELD_WELD  = 2,
    WELD_HOLD  = 3,
    WELD_CYL2  = 4,
};

/* comp_time "no reduction" sentinel (samd20: comp_time==7 => full amplitude). */
#define WELD_COMP_FULL  7u

/* step input — caller injects live config each call (app_reg limit_on_time
 * injection pattern; no call back into app_lcd). run_mode is carried for the
 * slice-4 TRIGGER branch; slice 1 evaluates DELAY transitions only. */
typedef struct {
    uint8_t  start;              /* 1 = start trigger (consumed only in READY) */
    uint8_t  run_mode;           /* slice-4 (TRIGGER); slice 1 ignores */
    uint16_t limit_delay_time1;  /* CYL1 & CYL2 duration (x10 ms) */
    uint16_t limit_delay_time2;  /* WELD duration (x10 ms) */
    uint16_t limit_delay_time3;  /* HOLD duration (x10 ms) */
    uint8_t  output_power;       /* amplitude base, 50..100 (clamped upstream) */
} weld_in_t;

/* step output — edge flags (weld_start/weld_stop/cycle_done) are 1 for exactly
 * the one step they fire; sol_dn/run_status are levels. */
typedef struct {
    uint8_t  run_status;   /* current state (WELD_*) */
    uint8_t  sol_dn;       /* solenoid-down request level (1=ON, 0=OFF) */
    uint8_t  weld_start;   /* 1 = WELD entry edge: glue does US_CYCLE START + pot */
    uint8_t  weld_stop;    /* 1 = WELD exit edge: glue does US_CYCLE RUN_RELEASE */
    uint8_t  amplitude;    /* valid when weld_start: comp_time-corrected DAC 0..127 */
    uint8_t  cycle_done;   /* 1 = CYL2 completion edge: glue does work_cnt++ */
} weld_out_t;

void weld_fsm_init(void);                                 /* reset to READY (boot) */
void weld_fsm_step(const weld_in_t *in, weld_out_t *out); /* one 10 ms tick */
uint8_t weld_fsm_status(void);                            /* current run_status */
