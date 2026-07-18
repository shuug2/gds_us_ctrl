/* fw/include/app_horn_fsm.h — SYS_HORN(horn-down) 순수 FSM. HAL-free.
 *
 * legacy samd20: SYS_HORN 상태에서 양손 START(re_start1||re_start2) press가
 * 솔레노이드(horn_status)를 토글, 초음파/weld 사이클은 완전 배제
 * (main.c:1427-1441 토글 / 1634-1644 SOL 구동 / 3457-3471 진입·이탈). */
#pragma once
#include <stdint.h>

typedef struct {
    uint8_t mode;    /* 1 = horn-down 모드 (LCD STD SETUP 소유) */
    uint8_t key1;    /* SW_START1 raw 레벨 (active-LOW, 0=눌림) */
    uint8_t key2;    /* SW_START2 raw 레벨 (active-LOW) */
    uint8_t estop;   /* 1 = E-stop 활성 (솔 강제 OFF + 토글 차단) */
} horn_in_t;

void    horn_fsm_init(void);
/* 한 step(10ms) 진행, 반환 = 솔레노이드 레벨 (1=하강/ON). */
uint8_t horn_fsm_step(const horn_in_t *in);
