/* fw/test/test_app_overload_fsm.c — host unit tests, 순수 과부하 디바운스 FSM.
 * M16 disasm 충실 (firmware_disassembled.asm @0x10A6): HIGH ×5 연속 → assert,
 * LOW → count 즉시 reset(noise reject). assert/deassert는 1회 edge. */
#include <stdio.h>
#include <stdint.h>
#include "app_overload_fsm.h"

static int failures = 0;
#define CHECK_EQ(expr, expected) do {                                       \
    unsigned long a_ = (unsigned long)(expr);                               \
    unsigned long e_ = (unsigned long)(expected);                           \
    if (a_ != e_) {                                                         \
        printf("FAIL %s:%d  %s = %lu, expected %lu\n",                      \
               __FILE__, __LINE__, #expr, a_, e_);                          \
        failures++;                                                         \
    }                                                                       \
} while (0)

static void test_idle_no_event(void)
{
    overload_fsm_init();
    CHECK_EQ(overload_fsm_step(0u), 0u);    /* 정상 입력 → 무이벤트 */
    CHECK_EQ(overload_fsm_step(0u), 0u);
}

static void test_assert_needs_5_consecutive(void)
{
    overload_fsm_init();
    CHECK_EQ(overload_fsm_step(1u), 0u);    /* 1 */
    CHECK_EQ(overload_fsm_step(1u), 0u);    /* 2 */
    CHECK_EQ(overload_fsm_step(1u), 0u);    /* 3 */
    CHECK_EQ(overload_fsm_step(1u), 0u);    /* 4 */
    CHECK_EQ(overload_fsm_step(1u), (unsigned)(OVLD_EV_ACTIVE | OVLD_EV_ASSERT)); /* 5 → assert */
    CHECK_EQ(overload_fsm_step(1u), OVLD_EV_ACTIVE);  /* 6: active 유지, assert edge 없음 */
}

static void test_deassert_on_low(void)
{
    overload_fsm_init();
    for (int i = 0; i < 5; i++) overload_fsm_step(1u);    /* assert */
    CHECK_EQ(overload_fsm_step(0u), OVLD_EV_DEASSERT);    /* LOW → deassert edge, active 0 */
    CHECK_EQ(overload_fsm_step(0u), 0u);                  /* 이후 무이벤트 */
}

static void test_noise_reject_resets_count(void)
{
    overload_fsm_init();
    overload_fsm_step(1u);                  /* 1 */
    overload_fsm_step(1u);                  /* 2 */
    overload_fsm_step(1u);                  /* 3 */
    CHECK_EQ(overload_fsm_step(0u), 0u);    /* LOW → count reset (active 아니었음) */
    CHECK_EQ(overload_fsm_step(1u), 0u);    /* 1 (재시작) */
    CHECK_EQ(overload_fsm_step(1u), 0u);    /* 2 */
    CHECK_EQ(overload_fsm_step(1u), 0u);    /* 3 */
    CHECK_EQ(overload_fsm_step(1u), 0u);    /* 4 */
    CHECK_EQ(overload_fsm_step(1u), (unsigned)(OVLD_EV_ACTIVE | OVLD_EV_ASSERT)); /* 5 → assert */
}

static void test_saturation_single_assert(void)
{
    overload_fsm_init();
    for (int i = 0; i < 4; i++) overload_fsm_step(1u);
    CHECK_EQ(overload_fsm_step(1u), (unsigned)(OVLD_EV_ACTIVE | OVLD_EV_ASSERT));
    for (int i = 0; i < 20; i++) {           /* 카운터 포화, assert edge 재발 없음 */
        CHECK_EQ(overload_fsm_step(1u), OVLD_EV_ACTIVE);
    }
}

static void test_glitch_low_deasserts(void)
{
    overload_fsm_init();
    for (int i = 0; i < 5; i++) overload_fsm_step(1u);    /* assert */
    CHECK_EQ(overload_fsm_step(0u), OVLD_EV_DEASSERT);    /* 단일 LOW → 즉시 deassert (M16 충실) */
    /* 재-assert는 다시 5 연속 필요 */
    CHECK_EQ(overload_fsm_step(1u), 0u);
}

int main(void)
{
    test_idle_no_event();
    test_assert_needs_5_consecutive();
    test_deassert_on_low();
    test_noise_reject_resets_count();
    test_saturation_single_assert();
    test_glitch_low_deasserts();
    if (failures) { printf("app_overload_fsm: %d FAIL\n", failures); return 1; }
    printf("app_overload_fsm: all tests passed\n");
    return 0;
}
