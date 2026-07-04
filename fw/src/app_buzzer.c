/* fw/src/app_buzzer.c — 부저 글루: 10ms tick에 FSM 진행, 레벨 엣지에만 io 구동. */
#include "app_buzzer.h"
#include "app_buzzer_fsm.h"
#include "io.h"
#include "sys_tick.h"

#define BUZZER_TICK_MS  10u

static uint32_t s_prev_ms;
static uint8_t  s_last;

void app_buzzer_init(void)
{
    buzzer_fsm_init();
    s_prev_ms = sys_tick_get_ms();
    s_last    = 0u;
    io_buzzer(false);
}

void app_buzzer_beep_ms(uint16_t ms)
{
    /* ms=0 → no-op; 0<ms<10 은 최소 1 tick으로 클램프 (10ms 미만 묵음 방지, 리뷰 M-2). */
    uint16_t ticks = (uint16_t)(ms / BUZZER_TICK_MS);
    if ((ms != 0u) && (ticks == 0u)) {
        ticks = 1u;
    }
    buzzer_fsm_beep(ticks);
}

void app_buzzer_tick(void)
{
    uint32_t now = sys_tick_get_ms();
    if ((uint32_t)(now - s_prev_ms) < BUZZER_TICK_MS) {
        return;
    }
    s_prev_ms = now;

    uint8_t on = buzzer_fsm_step();
    if (on != s_last) {
        s_last = on;
        io_buzzer(on != 0u);
    }
}
