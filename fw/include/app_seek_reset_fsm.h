/* fw/include/app_seek_reset_fsm.h — Stage SEEK/RESET: HAL-free pure FSM core.
 * samd20 공진 RESET/SEEK 명령 효과 (ref/samd20/main.c:5388-5408): TOUCH/COMM
 * 소스 RESET은 500ms 유지 후 SEEK으로 자동 체인 → 또 500ms 후 자동 해제. SEEK
 * 직접은 체인 없는 500ms 단발. 물리 OSC 신호 구동(reset_signal/seek_signal →
 * CTRL_OSC*)은 글루의 hook stub(B-SEAM 이연); 본 코어는 상태/타이밍/ICON 엣지만.
 * 글루(app_seek_reset.c)가 10ms마다 seek_reset_fsm_step() 호출. host-tested. */
#pragma once
#include <stdint.h>

enum { SR_IDLE = 0, SR_RESET = 1, SR_SEEK = 2 };
enum { SR_CMD_NONE = 0, SR_CMD_RESET = 1, SR_CMD_SEEK = 2 };

/* 500ms / 10ms tick = 50. samd20 100ms·us_reset_cnt>5의 명목 500ms를 10ms tick
 * 액면가로 재현 (±100ms quirk 미재현 — 명령 신호라 정밀도 비요구, spec §3.2). */
#define SR_TICKS  50u

/* step input — 글루가 매 tick 주입 (app_reg limit_on_time 주입 패턴; app_lcd로
 * 콜백 없음). */
typedef struct {
    uint8_t cmd;          /* SR_CMD_* — 이 tick의 명령 (없으면 NONE) */
    uint8_t run_active;   /* 1 = us_run_status != US_IDLE (RUN 중 → 명령 무시) */
} seek_reset_in_t;

/* step output — signal은 active 레벨, *_icon / *_icon_off는 전이 step 1-shot 엣지
 * (out은 매 step memset 0 → 엣지 자동). */
typedef struct {
    uint8_t state;          /* 현재 SR_* */
    uint8_t reset_signal;   /* RESET 명령선 레벨 (1=active) */
    uint8_t seek_signal;    /* SEEK 명령선 레벨 (1=active) */
    uint8_t reset_icon;     /* 1-shot: RESET ICON on 엣지 */
    uint8_t reset_icon_off; /* 1-shot: RESET ICON off 엣지 */
    uint8_t seek_icon;      /* 1-shot: SEEK ICON on 엣지 */
    uint8_t seek_icon_off;  /* 1-shot: SEEK ICON off 엣지 */
} seek_reset_out_t;

void seek_reset_fsm_init(void);                                    /* reset to IDLE (boot) */
void seek_reset_fsm_step(const seek_reset_in_t *in, seek_reset_out_t *out); /* one 10ms tick */
uint8_t seek_reset_fsm_state(void);                                /* current SR_* */
