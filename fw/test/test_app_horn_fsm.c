/* fw/test/test_app_horn_fsm.c — host unit tests, SYS_HORN(horn-down) 순수 FSM.
 * legacy: 양손 START 아무거나 press → 솔 토글(main.c:1433-1440), 모드 진입은
 * 항상 솔 OFF부터(3457-3463) + 진입 시점 pending press 무시(re_*_pressed=0). */
#include <stdio.h>
#include <stdint.h>
#include "app_horn_fsm.h"

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

/* 모드 OFF: 키를 아무리 눌러도 솔 0 */
static void test_mode_off_inert(void)
{
    horn_fsm_init();
    horn_in_t in = { .mode = 0u, .key1 = 1u, .key2 = 1u, .estop = 0u };
    CHECK_EQ(horn_fsm_step(&in), 0u);
    in.key1 = 0u;
    CHECK_EQ(horn_fsm_step(&in), 0u);
    in.key1 = 1u; in.key2 = 0u;
    CHECK_EQ(horn_fsm_step(&in), 0u);
}

/* 모드 ON: key1 press 엣지마다 토글 (하강→상승→하강), release는 무효과 */
static void test_key1_toggle(void)
{
    horn_fsm_init();
    horn_in_t in = { .mode = 1u, .key1 = 1u, .key2 = 1u, .estop = 0u };
    CHECK_EQ(horn_fsm_step(&in), 0u);          /* 진입 = 솔 OFF */
    in.key1 = 0u;                              /* press 엣지 */
    CHECK_EQ(horn_fsm_step(&in), 1u);          /* 하강 */
    CHECK_EQ(horn_fsm_step(&in), 1u);          /* 유지 중 무변화 */
    in.key1 = 1u;                              /* release */
    CHECK_EQ(horn_fsm_step(&in), 1u);          /* release는 토글 안 함 */
    in.key1 = 0u;                              /* 재-press */
    CHECK_EQ(horn_fsm_step(&in), 0u);          /* 상승 */
}

/* key2도 동일 토글 */
static void test_key2_toggle(void)
{
    horn_fsm_init();
    horn_in_t in = { .mode = 1u, .key1 = 1u, .key2 = 1u, .estop = 0u };
    (void)horn_fsm_step(&in);
    in.key2 = 0u;
    CHECK_EQ(horn_fsm_step(&in), 1u);
    in.key2 = 1u;
    (void)horn_fsm_step(&in);
    in.key2 = 0u;
    CHECK_EQ(horn_fsm_step(&in), 0u);
}

/* 동시 press = 1회 토글 (legacy re_start1||re_start2 단일 분기) */
static void test_both_keys_single_toggle(void)
{
    horn_fsm_init();
    horn_in_t in = { .mode = 1u, .key1 = 1u, .key2 = 1u, .estop = 0u };
    (void)horn_fsm_step(&in);
    in.key1 = 0u; in.key2 = 0u;                /* 같은 step 양쪽 press */
    CHECK_EQ(horn_fsm_step(&in), 1u);          /* 두 번 토글(=0) 아님 */
}

/* 모드 진입 시점에 이미 눌린 키 = 유령 토글 금지 (bak zero-init +
 * 진입-step 토글 무시; legacy 진입 시 re_*_pressed=0 등가) */
static void test_entry_with_held_key_no_ghost(void)
{
    horn_fsm_init();
    horn_in_t in = { .mode = 0u, .key1 = 0u, .key2 = 1u, .estop = 0u };
    (void)horn_fsm_step(&in);                  /* 모드 밖에서 이미 눌림 */
    in.mode = 1u;
    CHECK_EQ(horn_fsm_step(&in), 0u);          /* 진입 step: 토글 없음 */
    CHECK_EQ(horn_fsm_step(&in), 0u);          /* 유지 눌림: 엣지 아님 */
    in.key1 = 1u;
    (void)horn_fsm_step(&in);                  /* 실제 release */
    in.key1 = 0u;
    CHECK_EQ(horn_fsm_step(&in), 1u);          /* 이후 실제 press = 정상 토글 */
}

/* 진입 step에 press 엣지가 함께 오면 무시 (legacy pending 클리어) */
static void test_entry_step_press_swallowed(void)
{
    horn_fsm_init();
    horn_in_t in = { .mode = 0u, .key1 = 1u, .key2 = 1u, .estop = 0u };
    (void)horn_fsm_step(&in);
    in.mode = 1u; in.key1 = 0u;                /* 진입 + press 같은 step */
    CHECK_EQ(horn_fsm_step(&in), 0u);          /* 토글 무시 */
    in.key1 = 1u;
    (void)horn_fsm_step(&in);
    in.key1 = 0u;
    CHECK_EQ(horn_fsm_step(&in), 1u);          /* 다음 실제 press부터 유효 */
}

/* E-stop: 하강 중 진입 → 즉시 솔 OFF + 상태 0, 활성 중 토글 차단,
 * 해제 후 솔 OFF 유지(자발 재하강 금지) → 새 press로만 재토글 */
static void test_estop_forces_off(void)
{
    horn_fsm_init();
    horn_in_t in = { .mode = 1u, .key1 = 1u, .key2 = 1u, .estop = 0u };
    (void)horn_fsm_step(&in);
    in.key1 = 0u;
    CHECK_EQ(horn_fsm_step(&in), 1u);          /* 하강 */
    in.estop = 1u;
    CHECK_EQ(horn_fsm_step(&in), 0u);          /* 즉시 OFF */
    in.key1 = 1u; (void)horn_fsm_step(&in);
    in.key1 = 0u;                              /* estop 중 press */
    CHECK_EQ(horn_fsm_step(&in), 0u);          /* 토글 차단 */
    in.estop = 0u;
    CHECK_EQ(horn_fsm_step(&in), 0u);          /* 해제돼도 OFF 유지 */
    in.key1 = 1u; (void)horn_fsm_step(&in);
    in.key1 = 0u;
    CHECK_EQ(horn_fsm_step(&in), 1u);          /* 새 press = 정상 토글 */
}

/* 모드 이탈: 하강 중이라도 솔 OFF, 재진입도 OFF부터 (legacy 3466-3471) */
static void test_mode_exit_forces_off(void)
{
    horn_fsm_init();
    horn_in_t in = { .mode = 1u, .key1 = 1u, .key2 = 1u, .estop = 0u };
    (void)horn_fsm_step(&in);
    in.key1 = 0u;
    CHECK_EQ(horn_fsm_step(&in), 1u);
    in.mode = 0u;
    CHECK_EQ(horn_fsm_step(&in), 0u);          /* 이탈 = 즉시 OFF */
    in.mode = 1u; in.key1 = 1u;
    CHECK_EQ(horn_fsm_step(&in), 0u);          /* 재진입 = OFF부터 */
}

int main(void)
{
    test_mode_off_inert();
    test_key1_toggle();
    test_key2_toggle();
    test_both_keys_single_toggle();
    test_entry_with_held_key_no_ghost();
    test_entry_step_press_swallowed();
    test_estop_forces_off();
    test_mode_exit_forces_off();
    if (failures) { printf("app_horn_fsm: %d FAIL\n", failures); return 1; }
    printf("app_horn_fsm: all tests passed\n");
    return 0;
}
