/* fw/src/app_seek_reset.c — SEEK/RESET 글루. 10ms-gated FSM advance; out 엣지를
 * 물리 신호 hook stub + LCD ICON 렌더로. 누산/주입 불필요 (weld energy와 달리
 * 순수 시간+명령 엣지 구동). spec §4. */
#include "app_seek_reset.h"
#include "app_seek_reset_fsm.h"
#include "app_reg.h"      /* app_reg_measure (us_run_status 조회) */
#include "dgus_lcd.h"     /* ICON_RESET, ICON_SEEK */
#include "sys_tick.h"
#include "mon.h"

/* app_lcd_icon(vp, on) 선언은 app_lcd.h (app_seek_reset.h가 include). */

#define SR_TICK_MS  10u   /* FSM tick cadence (weld 패턴) */

static uint32_t s_prev_ms;
static uint8_t  s_pending_cmd;   /* SR_CMD_* one-shot 래치 (다음 tick 소비) */

void app_seek_reset_init(void)
{
    seek_reset_fsm_init();
    s_prev_ms     = sys_tick_get_ms();
    s_pending_cmd = SR_CMD_NONE;
}

void app_seek_reset_request(us_cmd_t cmd, uint8_t src)
{
    /* src는 추적용 — 본 스테이지 미사용 (samd20 comm RESET이 src=US_TOUCH로
     * 마킹하고 에러 표시를 클리어하는 quirk는 에러 머신 슬라이스, app_reg.c:144). */
    (void)src;
    if (cmd == US_CMD_RESET) {
        s_pending_cmd = SR_CMD_RESET;
    } else if (cmd == US_CMD_SEEK) {
        s_pending_cmd = SR_CMD_SEEK;
    }
}

uint8_t app_seek_reset_active(void)
{
    return (uint8_t)(seek_reset_fsm_state() != SR_IDLE);
}

void app_seek_reset_hook_signal(uint8_t which, bool on)
{
    /* 물리 OSC 구동 stub (B-SEAM). 실제 CTRL_OSC* 핀 구동은 reset_signal/
     * seek_signal 레벨로 후속 바인딩. */
    mon_printf("[sr] %s signal %s\r\n",
               (which == SR_CMD_RESET) ? "RESET" : "SEEK", on ? "ON" : "OFF");
}

void app_seek_reset_tick(void)
{
    uint32_t now = sys_tick_get_ms();
    if ((uint32_t)(now - s_prev_ms) < SR_TICK_MS) {
        return;
    }
    s_prev_ms = now;

    seek_reset_in_t in = {
        .cmd        = s_pending_cmd,
        /* g_measure는 직전 iter의 app_reg_tick 게시값 (이 tick은 reg_tick 앞) —
         * 1-iter stale run_active. 이 스테이지 hook stub 한정 무해 (cpp-review
         * Minor 1); B-SEAM 실 OSC 핀 구동 시 RUN-중-신호 윈도우 재검토. */
        .run_active = (uint8_t)(app_reg_measure()->us_run_status != (uint8_t)US_IDLE),
    };
    /* one-shot 소비: 매 tick 클리어 (무시돼도 드롭, weld s_start_pending 패턴). */
    s_pending_cmd = SR_CMD_NONE;

    seek_reset_out_t out;
    seek_reset_fsm_step(&in, &out);

    /* signal 레벨 엣지(0↔1)는 icon on/off 엣지와 동일 step에 발생하므로 icon
     * 엣지 1-shot을 hook + LCD ICON 라우팅에 함께 사용. reset_signal/seek_signal
     * 레벨 필드는 host-test span 검증 + B-SEAM 실 핀 구동용으로 유지. */
    if (out.reset_icon)     { app_seek_reset_hook_signal(SR_CMD_RESET, true);  app_lcd_icon(ICON_RESET, true);  }
    if (out.reset_icon_off) { app_seek_reset_hook_signal(SR_CMD_RESET, false); app_lcd_icon(ICON_RESET, false); }
    if (out.seek_icon)      { app_seek_reset_hook_signal(SR_CMD_SEEK,  true);  app_lcd_icon(ICON_SEEK,  true);  }
    if (out.seek_icon_off)  { app_seek_reset_hook_signal(SR_CMD_SEEK,  false); app_lcd_icon(ICON_SEEK,  false); }
}
