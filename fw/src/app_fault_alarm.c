/* fw/src/app_fault_alarm.c — 일반 fault 부저 알람 글루.
 *
 * legacy는 led_update()(samd20 main.c:5376, 100ms 루틴 무조건 호출)가
 * sys_status==SYS_ERROR면 부저를 점멸 — 에러 종류 불문 단일 알람. 포팅은
 * E-stop(app_input)/과부하(app_overload)만 개별 점멸이 있어 일반 fault
 * (measure.error_status: 현재 ERR_OVTIME, 향후 ERR_OUTERR)가 무음이었음
 * (2026-07-18 사용자 보고). 과부하/E-stop은 error_status가 아닌 자체 상태로
 * 점멸하므로 여기와 중복 구동 없음(ERR_OVLD는 LCD state 쪽 비트 —
 * measure.error_status 세터는 app_reg의 OVTIME뿐).
 *
 * 패턴 = app_overload 점멸과 동일(250ms beep을 500ms마다 재-arm, legacy
 * mode_blink 등가). RESET이 error_status를 클리어하면 자동 소음. */
#include "app_fault_alarm.h"
#include "app_buzzer.h"
#include "app_reg.h"
#include "sys_tick.h"

#define FAULT_TICK_MS   10u
#define FAULT_BEEP_MS   250u   /* 점멸 1회 on 길이 (E-stop/OVLD와 동일) */
#define FAULT_BLINK_MS  500u   /* 250 on / 250 off 주기 = 재-arm 간격 */

static uint32_t s_prev_ms;
static uint32_t s_blink_ms;
static uint8_t  s_prev_err;

/* fault 알람 초기화 */
void app_fault_alarm_init(void)
{
    s_prev_ms  = sys_tick_get_ms();
    s_blink_ms = s_prev_ms;
    s_prev_err = 0u;
}

/* fault 부저 점멸 tick */
void app_fault_alarm_tick(void)
{
    uint32_t now = sys_tick_get_ms();
    if ((uint32_t)(now - s_prev_ms) < FAULT_TICK_MS) {
        return;
    }
    s_prev_ms = now;

    uint8_t err = app_reg_measure()->error_status;
    if ((err != 0u) && (s_prev_err == 0u)) {
        /* fault 진입 1-shot: 즉시 beep + 점멸 타이머 리셋 */
        s_blink_ms = now;
        app_buzzer_beep_ms(FAULT_BEEP_MS);
    } else if (err != 0u) {
        if ((uint32_t)(now - s_blink_ms) >= FAULT_BLINK_MS) {
            s_blink_ms = now;
            app_buzzer_beep_ms(FAULT_BEEP_MS);
        }
    }
    s_prev_err = err;
}
