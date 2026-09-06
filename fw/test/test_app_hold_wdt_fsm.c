/* fw/test/test_app_hold_wdt_fsm.c — host unit tests, 원격 hold-to-run 워치독 순수 FSM.
 *
 * 계약 = spec 2026-09-06 §3(의사코드)·§4(세션 경계)·§7(이 표). 핵심 불변식 셋:
 *   ① 무장 전엔 절대 트립하지 않는다 — HMI/mbpoll 의 탭 런을 건드리지 않는다.
 *   ② keep 은 무장 중에만 시각을 갱신한다 — 기동 권한이 없다(R-3 ②).
 *   ③ 런 소스가 US_COMM 이 아니게 되면 세션이 조용히 끝난다(트립 아님) — 타 경로
 *      정지(STOP·E-STOP·30 s·패널) 뒤 keep 이 와도 재기동이 없다.
 * legacy 대응물 없음(신규 기능). */
#include <stdio.h>
#include <stdint.h>
#include "app_hold_wdt_fsm.h"
#include "app_modbus_core.h"

static int failures = 0;
#define CHECK_EQ(expr, expected) do {                                       \
    unsigned long a_ = (unsigned long)(expr);                              \
    unsigned long e_ = (unsigned long)(expected);                          \
    if (a_ != e_) {                                                         \
        printf("FAIL %s:%d  %s = %lu, expected %lu\n",                      \
               __FILE__, __LINE__, #expr, a_, e_);                          \
        failures++;                                                         \
    }                                                                       \
} while (0)

/* 1. 무장 전엔 step 이 아무리 돌아도 트립 0 */
static void test_unarmed_never_trips(void)
{
    hold_wdt_t w;
    hold_wdt_init(&w);
    CHECK_EQ(hold_wdt_armed(&w), 0);
    for (uint32_t t = 0u; t < 120000u; t += 100u) {
        CHECK_EQ(hold_wdt_step(&w, t, 1u), 0);
    }
    CHECK_EQ(hold_wdt_armed(&w), 0);
}

/* 2. 무장 전 keep 은 no-op — 기동 권한 없음 */
static void test_keep_before_arm_is_noop(void)
{
    hold_wdt_t w;
    hold_wdt_init(&w);
    hold_wdt_keep(&w, 100u);
    hold_wdt_keep(&w, 200u);
    CHECK_EQ(hold_wdt_armed(&w), 0);
    CHECK_EQ(hold_wdt_step(&w, 5000u, 1u), 0);
}

/* 3. 정상 경로: 150 ms 마다 keep → 60 s 무트립, 계속 무장 */
static void test_periodic_keep_holds(void)
{
    hold_wdt_t w;
    hold_wdt_init(&w);
    hold_wdt_arm(&w, 1000u);
    for (uint32_t t = 1000u; t < 61000u; t += 150u) {
        hold_wdt_keep(&w, t);
        CHECK_EQ(hold_wdt_step(&w, t + 10u, 1u), 0);
    }
    CHECK_EQ(hold_wdt_armed(&w), 1);
}

/* 4. 경계값: keep 없이 599 ms → 0, 600 ms → 1, 그 뒤 해제 */
static void test_trip_boundary(void)
{
    hold_wdt_t w;
    hold_wdt_init(&w);
    hold_wdt_arm(&w, 10000u);
    CHECK_EQ(hold_wdt_step(&w, 10000u + HOLD_WDT_MS - 1u, 1u), 0);
    CHECK_EQ(hold_wdt_armed(&w), 1);
    CHECK_EQ(hold_wdt_step(&w, 10000u + HOLD_WDT_MS, 1u), 1);
    CHECK_EQ(hold_wdt_armed(&w), 0);
}

/* 5. 경계 반대편: 정확히 T-1 간격 keep 은 영구 무트립 */
static void test_keep_at_t_minus_1_never_trips(void)
{
    hold_wdt_t w;
    hold_wdt_init(&w);
    hold_wdt_arm(&w, 0u);
    uint32_t t = 0u;
    for (int i = 0; i < 200; i++) {
        t += HOLD_WDT_MS - 1u;
        CHECK_EQ(hold_wdt_step(&w, t, 1u), 0);
        hold_wdt_keep(&w, t);
    }
    CHECK_EQ(hold_wdt_armed(&w), 1);
}

/* 6. 타 경로 정지(run_is_comm=0) → 트립 0, 세션 종료; 이후 keep+step 으로 재무장 없음 */
static void test_foreign_stop_ends_session_silently(void)
{
    hold_wdt_t w;
    hold_wdt_init(&w);
    hold_wdt_arm(&w, 1000u);
    hold_wdt_keep(&w, 1100u);
    CHECK_EQ(hold_wdt_step(&w, 1200u, 0u), 0);   /* STOP/E-STOP/30s/패널이 세웠다 */
    CHECK_EQ(hold_wdt_armed(&w), 0);
    hold_wdt_keep(&w, 1300u);                    /* 늦은 keep */
    CHECK_EQ(hold_wdt_step(&w, 1400u, 1u), 0);   /* 다른 마스터의 새 탭 런이어도 무관 */
    CHECK_EQ(hold_wdt_armed(&w), 0);
    CHECK_EQ(hold_wdt_step(&w, 9000u, 1u), 0);   /* 절대 트립 안 함 */
    /* 정당한 새 press(arm)는 정상 동작 — 세션 종료가 arm 경로를 막지 않는다 */
    hold_wdt_arm(&w, 20000u);
    CHECK_EQ(hold_wdt_step(&w, 20000u + HOLD_WDT_MS - 1u, 1u), 0);
    CHECK_EQ(hold_wdt_step(&w, 20000u + HOLD_WDT_MS, 1u), 1);
}

/* 7. 트립 후 keep 계속 → 두 번째 트립 없음 */
static void test_no_double_trip(void)
{
    hold_wdt_t w;
    hold_wdt_init(&w);
    hold_wdt_arm(&w, 0u);
    CHECK_EQ(hold_wdt_step(&w, HOLD_WDT_MS, 1u), 1);
    for (uint32_t t = HOLD_WDT_MS; t < HOLD_WDT_MS * 10u; t += 150u) {
        hold_wdt_keep(&w, t);
        CHECK_EQ(hold_wdt_step(&w, t, 1u), 0);
    }
    CHECK_EQ(hold_wdt_armed(&w), 0);
}

/* 8. arm 중복 호출(START=2 응답 유실 재시도) → 세션 1개, 기준 시각 갱신 */
static void test_rearm_refreshes(void)
{
    hold_wdt_t w;
    hold_wdt_init(&w);
    hold_wdt_arm(&w, 0u);
    hold_wdt_arm(&w, 500u);                       /* 재시도 */
    CHECK_EQ(hold_wdt_step(&w, 500u + HOLD_WDT_MS - 1u, 1u), 0);
    CHECK_EQ(hold_wdt_step(&w, 500u + HOLD_WDT_MS, 1u), 1);
}

/* 9. u32 랩 안전 */
static void test_wrap_safe(void)
{
    hold_wdt_t w;
    hold_wdt_init(&w);
    uint32_t t0 = 0xFFFFFF00u;
    hold_wdt_arm(&w, t0);
    CHECK_EQ(hold_wdt_step(&w, t0 + HOLD_WDT_MS - 1u, 1u), 0);   /* 랩 넘어감 */
    CHECK_EQ(hold_wdt_step(&w, t0 + HOLD_WDT_MS, 1u), 1);
}

/* 10. T 범위 계약 (요구사항 §2: 200 ≤ T ≤ 1000, 벤치 -D 가 새는 것 방지) */
static void test_timeout_in_contract_range(void)
{
    CHECK_EQ(HOLD_WDT_MS >= 200u, 1);
    CHECK_EQ(HOLD_WDT_MS <= 1000u, 1);
}

/* 11. wire 계약 — START 값 3종 상호 상이·비0, FEAT_CAP 주소/비트 */
static void test_wire_values_match_contract(void)
{
    CHECK_EQ(MB_START_TAP, 1);
    CHECK_EQ(MB_START_HOLD, 2);
    CHECK_EQ(MB_START_KEEP, 3);
    CHECK_EQ(MB_START_TAP != MB_START_HOLD && MB_START_HOLD != MB_START_KEEP &&
             MB_START_TAP != MB_START_KEEP, 1);
    CHECK_EQ(MB_FEAT_HOLD_WDT, 1);
    CHECK_EQ(MB_REG_FEAT_CAP, 0x32);
    CHECK_EQ(MB_REG_FEAT_CAP < MB_REG_COUNT, 1);
}

int main(void) {
    test_unarmed_never_trips();
    test_keep_before_arm_is_noop();
    test_periodic_keep_holds();
    test_trip_boundary();
    test_keep_at_t_minus_1_never_trips();
    test_foreign_stop_ends_session_silently();
    test_no_double_trip();
    test_rearm_refreshes();
    test_wrap_safe();
    test_timeout_in_contract_range();
    test_wire_values_match_contract();
    if (failures) { printf("%d check(s) FAILED\n", failures); return 1; }
    printf("app_hold_wdt_fsm: all tests passed\n");
    return 0;
}
