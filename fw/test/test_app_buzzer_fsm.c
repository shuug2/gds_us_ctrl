/* fw/test/test_app_buzzer_fsm.c — host unit tests, 순수 부저 timed-beep FSM. */
#include <stdio.h>
#include <stdint.h>
#include "app_buzzer_fsm.h"

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

static void test_idle_off(void)
{
    buzzer_fsm_init();
    CHECK_EQ(buzzer_fsm_step(), 0);   /* beep 없으면 항상 off */
    CHECK_EQ(buzzer_fsm_step(), 0);
}

static void test_beep_n_ticks(void)
{
    buzzer_fsm_init();
    buzzer_fsm_beep(3);
    CHECK_EQ(buzzer_fsm_step(), 1);   /* 3 tick on */
    CHECK_EQ(buzzer_fsm_step(), 1);
    CHECK_EQ(buzzer_fsm_step(), 1);
    CHECK_EQ(buzzer_fsm_step(), 0);   /* 이후 off */
    CHECK_EQ(buzzer_fsm_step(), 0);
}

static void test_rebeep_resets(void)
{
    buzzer_fsm_init();
    buzzer_fsm_beep(2);
    CHECK_EQ(buzzer_fsm_step(), 1);   /* 1 소비 (남은 1) */
    buzzer_fsm_beep(2);               /* 재트리거 → 2로 리셋 */
    CHECK_EQ(buzzer_fsm_step(), 1);
    CHECK_EQ(buzzer_fsm_step(), 1);
    CHECK_EQ(buzzer_fsm_step(), 0);
}

static void test_beep_zero(void)
{
    buzzer_fsm_init();
    buzzer_fsm_beep(0);
    CHECK_EQ(buzzer_fsm_step(), 0);   /* 0 tick = no-op */
}

static void test_beep_one_tick(void)
{
    buzzer_fsm_init();
    buzzer_fsm_beep(1);
    CHECK_EQ(buzzer_fsm_step(), 1);   /* 정확히 1 tick on (> vs >= 경계) */
    CHECK_EQ(buzzer_fsm_step(), 0);   /* 즉시 off */
}

int main(void)
{
    test_idle_off();
    test_beep_n_ticks();
    test_rebeep_resets();
    test_beep_zero();
    test_beep_one_tick();
    if (failures) { printf("app_buzzer_fsm: %d FAIL\n", failures); return 1; }
    printf("app_buzzer_fsm: all tests passed\n");
    return 0;
}
