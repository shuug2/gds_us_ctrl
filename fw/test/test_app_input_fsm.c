/* fw/test/test_app_input_fsm.c — host unit tests, 순수 물리 입력 FSM.
 * 활성극성: START/RESET/SEEK active-LOW(0=눌림), EMSW active-HIGH(1=비상).
 * E-stop = 레벨추종(std 모드만); hand/multi에선 PC11=SEEK, EMSW 비활성. */
#include <stdio.h>
#include <stdint.h>
#include "app_input_fsm.h"

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

/* idle 입력(active-LOW 1, EMSW 0) — 부팅 무이벤트 */
static void test_idle_no_event(void)
{
    input_fsm_init();
    input_in_t in = { .start = 1u, .reset = 1u, .estop_seek = 1u, .model_type = 0u };
    input_out_t o = input_fsm_step(&in);
    CHECK_EQ(o.start_press, 0u);
    CHECK_EQ(o.reset_press, 0u);
    CHECK_EQ(o.seek_press, 0u);
    CHECK_EQ(o.estop_active, 0u);
}

/* B_START 모멘터리: 눌림→press, 유지→무이벤트, 뗌→release */
static void test_start_momentary(void)
{
    input_fsm_init();
    input_in_t in = { .start = 1u, .reset = 1u, .estop_seek = 1u, .model_type = 0u };
    (void)input_fsm_step(&in);                 /* idle */
    in.start = 0u;                             /* 눌림 */
    input_out_t o = input_fsm_step(&in);
    CHECK_EQ(o.start_press, 1u);
    CHECK_EQ(o.start_release, 0u);
    o = input_fsm_step(&in);                   /* 유지 */
    CHECK_EQ(o.start_press, 0u);
    in.start = 1u;                             /* 뗌 */
    o = input_fsm_step(&in);
    CHECK_EQ(o.start_release, 1u);
    CHECK_EQ(o.start_press, 0u);
}

/* B_RESET 눌림 엣지 */
static void test_reset_press_edge(void)
{
    input_fsm_init();
    input_in_t in = { .start = 1u, .reset = 1u, .estop_seek = 1u, .model_type = 2u };
    (void)input_fsm_step(&in);
    in.reset = 0u;
    input_out_t o = input_fsm_step(&in);
    CHECK_EQ(o.reset_press, 1u);
    o = input_fsm_step(&in);                   /* 유지 → 무이벤트 */
    CHECK_EQ(o.reset_press, 0u);
}

/* hand/multi: PC11 LOW → seek_press, EMSW 비활성 */
static void test_seek_in_hand_multi(void)
{
    input_fsm_init();
    input_in_t in = { .start = 1u, .reset = 1u, .estop_seek = 1u, .model_type = 1u };
    (void)input_fsm_step(&in);
    in.estop_seek = 0u;                        /* SEEK 눌림 (active-LOW) */
    input_out_t o = input_fsm_step(&in);
    CHECK_EQ(o.seek_press, 1u);
    CHECK_EQ(o.estop_active, 0u);              /* hand/multi에선 EMSW 비활성 */
    CHECK_EQ(o.estop_enter, 0u);
}

/* std: PC11 HIGH → estop_active + enter 1-shot, 유지→재진입 없음, LOW→자동 클리어 */
static void test_estop_level_follow_in_std(void)
{
    input_fsm_init();
    input_in_t in = { .start = 1u, .reset = 1u, .estop_seek = 0u, .model_type = 2u };
    (void)input_fsm_step(&in);                 /* EMSW idle(LOW) */
    in.estop_seek = 1u;                        /* EMSW 인가(HIGH) */
    input_out_t o = input_fsm_step(&in);
    CHECK_EQ(o.estop_active, 1u);
    CHECK_EQ(o.estop_enter, 1u);               /* 상승 엣지 1-shot */
    o = input_fsm_step(&in);                   /* HIGH 유지 */
    CHECK_EQ(o.estop_active, 1u);
    CHECK_EQ(o.estop_enter, 0u);               /* 재진입 엣지 없음 */
    in.estop_seek = 0u;                        /* EMSW 해제(LOW) */
    o = input_fsm_step(&in);
    CHECK_EQ(o.estop_active, 0u);              /* 자동 클리어 (RESET 불필요) */
}

/* std 모드에선 PC11을 SEEK로 보지 않음 (seek_press 없음) */
static void test_no_seek_in_std(void)
{
    input_fsm_init();
    input_in_t in = { .start = 1u, .reset = 1u, .estop_seek = 0u, .model_type = 2u };
    (void)input_fsm_step(&in);                 /* EMSW LOW */
    /* EMSW HIGH→LOW는 SEEK active-LOW와 같은 전이지만 std에선 seek_press 금지 */
    in.estop_seek = 1u; (void)input_fsm_step(&in);
    in.estop_seek = 0u;
    input_out_t o = input_fsm_step(&in);
    CHECK_EQ(o.seek_press, 0u);
}

/* hand/multi에선 EMSW HIGH라도 estop_active 0 (PC11=SEEK 전용) */
static void test_no_estop_in_hand_multi(void)
{
    input_fsm_init();
    input_in_t in = { .start = 1u, .reset = 1u, .estop_seek = 1u, .model_type = 0u };
    input_out_t o = input_fsm_step(&in);       /* estop_seek HIGH지만 hand 모드 */
    CHECK_EQ(o.estop_active, 0u);
    CHECK_EQ(o.estop_enter, 0u);
}

/* 동시 입력: START 눌림 + RESET 눌림 같은 step */
static void test_concurrent_start_reset(void)
{
    input_fsm_init();
    input_in_t in = { .start = 1u, .reset = 1u, .estop_seek = 1u, .model_type = 1u };
    (void)input_fsm_step(&in);
    in.start = 0u; in.reset = 0u;
    input_out_t o = input_fsm_step(&in);
    CHECK_EQ(o.start_press, 1u);
    CHECK_EQ(o.reset_press, 1u);
}

int main(void)
{
    test_idle_no_event();
    test_start_momentary();
    test_reset_press_edge();
    test_seek_in_hand_multi();
    test_estop_level_follow_in_std();
    test_no_seek_in_std();
    test_no_estop_in_hand_multi();
    test_concurrent_start_reset();
    if (failures) { printf("app_input_fsm: %d FAIL\n", failures); return 1; }
    printf("app_input_fsm: all tests passed\n");
    return 0;
}
