/* fw/src/app_input.c — 물리 명령 입력 + E-stop 글루.
 * B_START(모멘터리)/B_RESET/PC11(model_type 분기 SEEK|EMSW)을 매 10ms 스캔해
 * app_reg_command(US_REMOTE)로 디스패치. E-stop(std EMSW)은 레벨추종: 진입 엣지에
 * io_sol_dn(off) 1-shot, active 동안 force-stop 매-tick 재시도(app_overload 패턴),
 * 떼면 자동 클리어(RESET 불필요, 런 재시작 안 함 = 새 START 필요). */
#include "app_input.h"
#include "app_input_fsm.h"
#include "io.h"          /* io_read_start/reset/estop_seek, io_sol_dn */
#include "app_reg.h"     /* app_reg_command, app_reg_measure */
#include "app_lcd.h"     /* app_lcd_cfg, us_cmd_t, US_*, app_lcd_set_estop */
#include "app_weld.h"    /* app_weld_abort_now (E-stop 진입 즉시 abort) */
#include "app_buzzer.h"  /* app_buzzer_beep_ms (E-stop 점멸 부저) */
#include "sys_tick.h"

#define INPUT_TICK_MS    10u
#define ESTOP_BEEP_MS   250u   /* 점멸 1회 on 길이 (app_overload와 동일 패턴) */
#define ESTOP_BLINK_MS  500u   /* 250 on / 250 off 주기 = 재-arm 간격 */

static uint32_t s_prev_ms;
static uint32_t s_blink_ms;
static uint8_t  s_estop_active;

void app_input_init(void)
{
    input_fsm_init();
    s_prev_ms      = sys_tick_get_ms();
    s_blink_ms     = s_prev_ms;
    s_estop_active = 0u;
}

uint8_t app_estop_active(void) { return s_estop_active; }

void app_input_tick(void)
{
    uint32_t now = sys_tick_get_ms();
    if ((uint32_t)(now - s_prev_ms) < INPUT_TICK_MS) {
        return;
    }
    s_prev_ms = now;

    input_in_t in;
    in.start      = io_read_start();
    in.reset      = io_read_reset();
    in.estop_seek = io_read_estop_seek();
    in.model_type = app_lcd_cfg()->model_type;

    input_out_t ev = input_fsm_step(&in);   /* 매 tick 실행: bak/엣지 항상 갱신 */
    uint8_t prev_estop = s_estop_active;
    s_estop_active = ev.estop_active;

    /* E-stop 진입 엣지: SOL OFF 1-shot (io_sol_dn idempotent) + weld 즉시
     * abort(페이지 무관 — LCD_WARNING 전환 후 weld tick 동결 레이스 차단) +
     * E-STOP 경고 페이지 (legacy EMSW 핸들러+do_control, main.c:1409-1425/
     * 4209-4215). */
    if (ev.estop_enter != 0u) {
        io_sol_dn(false);
        app_weld_abort_now();
        app_lcd_set_estop(true);
        s_blink_ms = now;
        app_buzzer_beep_ms(ESTOP_BEEP_MS);   /* 점멸 첫 beep (legacy SYS_ESTOP
                                                buzzer blink, main.c:2125-2136) */
    }
    /* 해제 엣지: 런 페이지 복귀 (legacy 4238-4241 init_lcd_mode 등가; 레벨추종
     * 자동 클리어 — RESET 불필요, 런 재시작 안 함은 기존 그대로). */
    if ((prev_estop != 0u) && (s_estop_active == 0u)) {
        app_lcd_set_estop(false);
    }

    if (s_estop_active != 0u) {
        /* active 동안 force-stop 매-tick 재시도 (app_overload 패턴: us_run_status
         * 미러가 app_reg_tick 발행이라 1-iter lag — 다음 tick에 잡힘). source-matched
         * RUN_RELEASE; idempotent(IDLE→no-op). START는 app_reg guard(app_estop_active)
         * 가 차단. E-stop 활성 중엔 명령 버튼 디스패치 스킵(아래 return) — FSM step은
         * 이미 위에서 실행돼 bak이 갱신됐으므로 해제 시 stale 엣지 없음. */
        uint8_t src = app_reg_measure()->us_run_status;
        if (src != (uint8_t)US_IDLE) {
            app_reg_command(US_CMD_RUN_RELEASE, src);
        }
        /* 부저 점멸: ESTOP_BLINK_MS마다 one-shot beep 재-arm (legacy SYS_ESTOP
         * mode_blink 등가, app_overload 패턴 재사용). 해제 시 자연 소멸. */
        if ((uint32_t)(now - s_blink_ms) >= ESTOP_BLINK_MS) {
            s_blink_ms = now;
            app_buzzer_beep_ms(ESTOP_BEEP_MS);
        }
        return;
    }

    /* 명령 버튼 (US_REMOTE 통일 strict). START 가드(==US_IDLE + estop/overload/
     * seek_reset break)는 app_reg_command 내부. */
    if (ev.start_press != 0u)   { app_reg_command(US_CMD_START,       (uint8_t)US_REMOTE); }
    if (ev.start_release != 0u) { app_reg_command(US_CMD_RUN_RELEASE, (uint8_t)US_REMOTE); }
    if (ev.reset_press != 0u)   { app_reg_command(US_CMD_RESET,       (uint8_t)US_REMOTE); }
    if (ev.seek_press != 0u)    { app_reg_command(US_CMD_SEEK,        (uint8_t)US_REMOTE); }
}
