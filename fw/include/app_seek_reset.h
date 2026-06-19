/* fw/include/app_seek_reset.h — Stage SEEK/RESET 글루: 10ms마다 seek_reset_fsm_step
 * 구동, out 엣지를 물리 신호 hook stub(B-SEAM) + LCD ICON 렌더로 라우팅. app_reg
 * 의 SEEK/RESET no-op을 app_seek_reset_request() 위임으로 교체하고, START guard가
 * app_seek_reset_active()를 read-only 조회 (양방향 RUN 직교). */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "app_lcd.h"   /* us_cmd_t (US_CMD_SEEK/RESET) */

void app_seek_reset_init(void);                          /* boot: FSM reset + tick gate */
void app_seek_reset_tick(void);                          /* superloop: 10ms-gated step + effects */
void app_seek_reset_request(us_cmd_t cmd, uint8_t src);  /* app_reg_command 위임 (1-shot 래치) */
uint8_t app_seek_reset_active(void);                     /* 1 = FSM != SR_IDLE (START 직교 조회) */

/* 물리 OSC 신호 hook (stub) — reset/seek 명령선 레벨 엣지마다 1회.
 * which: SR_CMD_RESET=RESET선 / SR_CMD_SEEK=SEEK선. 본 스테이지: mon 로그만.
 * B-SEAM: CTRL_OSC* active-LOW 구동 (극성 미확인, spec §10-1). */
void app_seek_reset_hook_signal(uint8_t which, bool on);
