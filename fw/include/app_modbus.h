/* fw/include/app_modbus.h — Modbus slave integration layer: owns what the
 * mb_core registers MEAN (samd20 update_holding_reg port — live mirror +
 * clamped config writes + FRAM commit), routes commands into the run FSM as
 * US_COMM, and arbitrates USART6 between mon and Modbus-RTU per the occupancy
 * rule (comm_mode==SERIAL && comm_address!=0, spec §5). */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "app_modbus_core.h"

/* Boot: evaluate the occupancy rule from the loaded config and acquire USART6
 * if Modbus owns it. Call after app_init() — needs app_config_load done. */
void app_modbus_init(void);

/* Superloop step: re-evaluate occupancy/line config (covers panel DATA_SAVE
 * changes — samd20 main-loop gate equivalent), drain one RTU frame, respond,
 * run the write-apply pass on FC06, refresh the live register mirror. */
void app_modbus_tick(void);

/* Shared Modbus core instance (register tables). RTU (this file) and TCP
 * (app_modbus_tcp.c) decode into the SAME core so register semantics are
 * single-sourced. */
mb_core_t *app_modbus_core(void);

/* The samd20 update_holding_reg(1) write-apply pass: clamp the one changed
 * config field, persist to FRAM, dispatch commands. Called by whichever
 * transport processed an FC06 (RTU inline; TCP via this accessor). Operates
 * on the shared core from app_modbus_core(). */
void app_modbus_apply_writes(void);

/* REMOTE-icon activity (samd20 modbus_status/modbus_comm_cnt, main.c:5187-
 * 5199): note_remote() is stamped by whichever transport decodes a valid
 * request (RTU/TCP); remote_active() then holds true for 1 s after the last
 * request (legacy timeout = 100 case-9 wraps ~= 1 s) and falls back to false.
 * Consumed by app_lcd_disp_step to drive DISP_REMOTE. */
void app_modbus_note_remote(void);
bool app_modbus_remote_active(void);
