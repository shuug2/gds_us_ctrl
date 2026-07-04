/* fw/include/app_input_fsm.h — 순수 물리 입력 FSM (HAL-free, host-test).
 * B_START/B_RESET edge-detect + PC11 이중역할(model_type 분기): hand/multi=SEEK
 * active-LOW edge / std=EMSW active-HIGH 레벨추종. step()이 명령 엣지 이벤트 +
 * E-stop 레벨/진입엣지를 구조체로 반환. spec 2026-06-27-physical-io-slice-d §3. */
#pragma once
#include <stdint.h>

typedef struct {
    uint8_t start;        /* PA15 raw 0/1 (active-LOW: 0=눌림) */
    uint8_t reset;        /* PC10 raw 0/1 (active-LOW) */
    uint8_t estop_seek;   /* PC11 raw 0/1 (std=EMSW active-HIGH / hand·multi=SEEK active-LOW) */
    uint8_t model_type;   /* 0=hand 1=multi 2=std */
} input_in_t;

typedef struct {
    uint8_t start_press;    /* B_START 눌림 엣지 → US_CMD_START */
    uint8_t start_release;  /* B_START 뗌 엣지   → US_CMD_RUN_RELEASE */
    uint8_t reset_press;    /* B_RESET 눌림 엣지 → US_CMD_RESET */
    uint8_t seek_press;     /* B_SEEK 눌림 엣지(hand/multi) → US_CMD_SEEK */
    uint8_t estop_active;   /* E-stop 레벨 (std EMSW HIGH 동안 1) */
    uint8_t estop_enter;    /* E-stop 상승 엣지 1-shot → SOL OFF 트리거 */
} input_out_t;

void        input_fsm_init(void);
input_out_t input_fsm_step(const input_in_t *in);
