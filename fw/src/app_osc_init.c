/* fw/src/app_osc_init.c — OSC 보드 부팅 초기화 글루. 10ms-gated FSM advance;
 * PB12 피드백(io_read_usfb) 주입, RESET/SEEK 출력 레벨을 board_reset/board_seek
 * (active-LOW)로 라우팅. DONE 후 핀 구동 중단(향후 app_seek_reset과 PB10/PB2
 * 공유 시 충돌 회피). spec §구조. */
#include "app_osc_init.h"
#include "app_osc_init_fsm.h"
#include "io.h"        /* io_read_usfb (PB12) */
#include "board.h"     /* board_reset (PB10), board_seek (PB2) */
#include "sys_tick.h"

#define OSC_INIT_TICK_MS  10u   /* FSM tick cadence (seek_reset 패턴) */

static uint32_t s_prev_ms;

/* OSC init 글루 초기화 */
void app_osc_init_init(void)
{
    osc_init_fsm_init();
    s_prev_ms = sys_tick_get_ms();
}

/* 부팅 FSM 완주 블로킹 */
void app_osc_init_run_to_done(void)
{
    /* 부팅 블로킹: PB12 펄스(전원 후 ~600~1200ms)는 LCD 부팅(app_init) 블로킹 구간에
     * 묻히므로, app_init 전에 여기서 완주해야 이벤트 감지가 산다. 10ms gate라
     * busy-spin이 10ms마다 한 step 진행 (sys_tick 필요 — main이 먼저 init). */
    while (osc_init_fsm_state() != OSC_DONE) {
        app_osc_init_tick();
    }
}

/* OSC init 10ms tick */
void app_osc_init_tick(void)
{
    uint32_t now = sys_tick_get_ms();
    if ((uint32_t)(now - s_prev_ms) < OSC_INIT_TICK_MS) {
        return;
    }
    s_prev_ms = now;

    /* 완료 후엔 PB10/PB2를 더 구동하지 않음 (마지막 SEEK-off가 DONE 전이 step에서
     * 이미 idle로 보냄) — 향후 app_seek_reset 명령 구동과 핀 공유 충돌 방지. */
    if (osc_init_fsm_state() == OSC_DONE) {
        return;
    }

    osc_init_in_t in = { .pb12 = io_read_usfb() };   /* PB12 active-HIGH raw */
    osc_init_out_t out;
    osc_init_fsm_step(&in, &out);

    board_reset(out.reset_signal != 0u);
    board_seek(out.seek_signal != 0u);
}
