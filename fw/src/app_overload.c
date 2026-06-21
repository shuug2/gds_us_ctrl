/* fw/src/app_overload.c — 과부하 응답 글루.
 * 거동(SAMD20 stop&wait + M16 흡수 자동복구):
 *  assert edge → 현재 run force-stop(RUN_RELEASE 현재 소스) + 릴레이 ON + 에러 ON + 부저
 *  active     → 부저 점멸(250/250) + 릴레이/에러 유지 (START는 app_reg guard가 차단)
 *  deassert   → 릴레이 OFF + 에러 클리어(페이지 복귀) + 자동 app_seek_reset_request(RESET) */
#include "app_overload.h"
#include "app_overload_fsm.h"
#include "io.h"             /* io_read_overload, io_ovld_relay */
#include "app_buzzer.h"     /* app_buzzer_beep_ms */
#include "app_reg.h"        /* app_reg_measure, app_reg_command */
#include "app_seek_reset.h" /* app_seek_reset_request */
#include "app_lcd.h"        /* app_lcd_set_overload, US_*, us_cmd_t */
#include "sys_tick.h"

#define OVLD_TICK_MS   10u
#define OVLD_BEEP_MS   250u    /* 점멸 1회 on 길이 */
#define OVLD_BLINK_MS  500u    /* 250 on / 250 off 주기 = 재-arm 간격 */

static uint32_t s_prev_ms;
static uint32_t s_blink_ms;
static uint8_t  s_active;

void app_overload_init(void)
{
    overload_fsm_init();
    s_prev_ms  = sys_tick_get_ms();
    s_blink_ms = s_prev_ms;
    s_active   = 0u;
    io_ovld_relay(false);
}

uint8_t app_overload_active(void) { return s_active; }

void app_overload_tick(void)
{
    uint32_t now = sys_tick_get_ms();
    if ((uint32_t)(now - s_prev_ms) < OVLD_TICK_MS) {
        return;
    }
    s_prev_ms = now;

    uint8_t ev = overload_fsm_step(io_read_overload());
    s_active = (uint8_t)((ev & OVLD_EV_ACTIVE) != 0u);

    if ((ev & OVLD_EV_ASSERT) != 0u) {
        /* 현재 활성 run을 force-stop. us_run_status는 단일값이라 현재 소스를
         * RUN_RELEASE에 넘기면 source-matched 정지가 발화한다. ⚠ 여기서 읽는
         * app_reg_measure()->us_run_status는 app_reg_tick(step3)에서 발행되는
         * 미러라 overload_tick(2.55)보다 1-iter lag — 과부하 ≥50ms(×5 디바운스)
         * steady-state에선 run이 안정 active라 항상 정지하지만, assert와 같은
         * 10ms iter에 START가 들어오면 stale IDLE을 읽어 즉시 정지를 놓치고
         * (edge-only라 재시도 없음) 그 run은 on-time ceiling(~560ms)까지 돈다.
         * 단 아래 io_ovld_relay(true)는 이 if 밖 무조건 발화라 릴레이 컷오프는
         * 이 레이스와 무관(릴레이가 실 초음파를 끊으면 escaped run은 cosmetic —
         * HW로 확정). 신규 app_reg API 불필요(advisor). ⚠ weld 기계 사이클
         * abort는 별개 — weld 물리트리거 dormant라 현재 무관 (슬라이스 E/weld4). */
        uint8_t src = app_reg_measure()->us_run_status;
        if (src != (uint8_t)US_IDLE) {
            app_reg_command(US_CMD_RUN_RELEASE, src);
        }
        io_ovld_relay(true);
        app_lcd_set_overload(true);
        s_blink_ms = now;
        app_buzzer_beep_ms(OVLD_BEEP_MS);
    }

    if (s_active != 0u) {
        /* 부저 점멸: active 동안 OVLD_BLINK_MS마다 one-shot beep 재-arm
         * (SAMD20 mode_blink 점멸 재현). */
        if ((uint32_t)(now - s_blink_ms) >= OVLD_BLINK_MS) {
            s_blink_ms = now;
            app_buzzer_beep_ms(OVLD_BEEP_MS);
        }
    }

    if ((ev & OVLD_EV_DEASSERT) != 0u) {
        io_ovld_relay(false);
        app_lcd_set_overload(false);
        /* M16 흡수: 고장 해소 시 자동 공진 재튜닝(RESET→SEEK). 체인의
         * app_seek_reset_active()가 재튜닝 동안 START 차단 → assert부터 복구완료
         * 까지 START 연속 차단. src=US_REMOTE = 시스템 개시 복구(물리 source
         * 없음 — 의도적 선택). */
        app_seek_reset_request(US_CMD_RESET, (uint8_t)US_REMOTE);
    }
}
