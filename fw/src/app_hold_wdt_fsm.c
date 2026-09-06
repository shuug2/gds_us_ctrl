/* fw/src/app_hold_wdt_fsm.c — 원격 hold-to-run 워치독 순수 FSM (spec 2026-09-06 §3). */
#include "app_hold_wdt_fsm.h"

/* 세션 초기화 */
void hold_wdt_init(hold_wdt_t *w)
{
    w->armed        = 0u;
    w->last_keep_ms = 0u;
}

/* 무장 — START=2 수락 지점 전용 */
void hold_wdt_arm(hold_wdt_t *w, uint32_t now_ms)
{
    w->armed        = 1u;
    w->last_keep_ms = now_ms;
}

/* 유지 신호 — 기동 권한 없음 */
void hold_wdt_keep(hold_wdt_t *w, uint32_t now_ms)
{
    if (w->armed != 0u) {
        w->last_keep_ms = now_ms;
    }
}

/* 한 step. 시간 비교는 u32 랩 안전 elapsed 형태. */
uint8_t hold_wdt_step(hold_wdt_t *w, uint32_t now_ms, uint8_t run_is_comm)
{
    if (w->armed == 0u) {
        return 0u;
    }
    if (run_is_comm == 0u) {
        /* 누군가 세웠다(STOP·E-STOP·30 s·energy·패널) — 세션 종료, 트립 아님.
         * 이후 keep 은 no-op 이 되어 재기동 경로가 없다. */
        w->armed = 0u;
        return 0u;
    }
    if ((uint32_t)(now_ms - w->last_keep_ms) >= HOLD_WDT_MS) {
        w->armed = 0u;
        return 1u;
    }
    return 0u;
}

/* 무장 여부 */
uint8_t hold_wdt_armed(const hold_wdt_t *w)
{
    return w->armed;
}
