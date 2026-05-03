/* fw/src/app.c */
#include "stm32f4xx_hal.h"
#include "app.h"
#include "board.h"
#include "sys_tick.h"
#include "mon.h"

static uint32_t s_last_beat_ms = 0;

void app_init(void) {
    sys_tick_init();
    mon_init();
    mon_writeln("[boot] gds_us_ctrl phase2 ready");
    s_last_beat_ms = sys_tick_get_ms();
}

void app_loop_iter(void) {
    uint32_t now = sys_tick_get_ms();
    if ((uint32_t)(now - s_last_beat_ms) >= 1000) {
        s_last_beat_ms = now;
        board_heartbeat_toggle();
        mon_printf("[t=%lu ms] hello\r\n", (unsigned long)now);
    }
}
