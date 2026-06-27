/* fw/include/board.h */
#pragma once

#include <stdbool.h>

void board_init(void);

/* OSC4(PB14) 발진 게이트. active-LOW: on=LOW(초음파 출력 중), off=HIGH(idle off). */
void board_osc4(bool on);

/* OSC 보드 RESET/SEEK 신호. active-LOW: on=LOW(출력), off=HIGH(idle).
 * RESET=PB10(OSC_OUT1), SEEK=PB2(OSC_OUT0). app_osc_init(부팅)과 향후
 * app_seek_reset(명령)이 공유. */
void board_reset(bool on);
void board_seek(bool on);
