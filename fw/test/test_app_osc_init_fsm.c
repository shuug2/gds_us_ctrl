/* fw/test/test_app_osc_init_fsm.c — host unit tests for the pure OSC boot-init
 * FSM core. No HAL, no hardware. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "app_osc_init_fsm.h"

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

/* 부팅 초기 상태 = WAIT_H. */
static void test_init_wait_h(void)
{
    osc_init_fsm_init();
    CHECK_EQ(osc_init_fsm_state(), OSC_WAIT_H);
}

/* 정상 경로: WAIT_H(연속 H DEBOUNCE 샘플)→WAIT_L(pb12 L 감지)→GAP(15)→
 * RESET(20)→SEEK(10)→DONE. reset/seek span + 순서(겹치지 않음) 검증. */
static void test_normal_sequence(void)
{
    osc_init_fsm_init();
    osc_init_in_t  in  = { .pb12 = 0u };
    osc_init_out_t out;

    /* pb12=0: WAIT_H 유지 */
    osc_init_fsm_step(&in, &out);
    CHECK_EQ(out.state, OSC_WAIT_H);

    /* pb12=1 연속 DEBOUNCE 샘플 → WAIT_L */
    in.pb12 = 1u;
    int wait_l_at = -1;
    for (int i = 1; i <= 5; i++) {
        osc_init_fsm_step(&in, &out);
        if (out.state == OSC_WAIT_L) { wait_l_at = i; break; }
    }
    CHECK_EQ(wait_l_at, OSC_H_DEBOUNCE);   /* 연속 H DEBOUNCE(2) 샘플에 전이 */

    /* pb12=1 유지: WAIT_L 유지 */
    osc_init_fsm_step(&in, &out);
    CHECK_EQ(out.state, OSC_WAIT_L);

    /* pb12=0 → GAP (출력 종료 감지) */
    in.pb12 = 0u;
    osc_init_fsm_step(&in, &out);
    CHECK_EQ(out.state, OSC_GAP);
    CHECK_EQ(out.reset_signal, 0);

    /* GAP → RESET: 진입 후 OSC_GAP_TICKS step */
    int reset_enter = -1;
    for (int i = 1; i < 40; i++) {
        osc_init_fsm_step(&in, &out);
        if (out.state == OSC_RESET) { reset_enter = i; break; }
    }
    CHECK_EQ(reset_enter, OSC_GAP_TICKS);   /* 15 */
    CHECK_EQ(out.reset_signal, 1);          /* RESET 진입 step에 reset active */
    CHECK_EQ(out.seek_signal, 0);

    /* RESET span = OSC_RESET_TICKS, 종료 시 SEEK 진입. RESET 동안 seek=0. */
    int reset_ticks = 1;                    /* 진입 step 카운트 */
    int seek_enter  = -1;
    int seek_during_reset = 0;
    for (int i = 1; i < 40; i++) {
        osc_init_fsm_step(&in, &out);
        if (out.state == OSC_RESET && out.seek_signal) { seek_during_reset = 1; }
        if (out.reset_signal && out.state == OSC_RESET) { reset_ticks++; }
        if (out.state == OSC_SEEK) { seek_enter = i; break; }
    }
    CHECK_EQ(reset_ticks, OSC_RESET_TICKS);  /* 20 */
    CHECK_EQ(seek_enter, OSC_RESET_TICKS);   /* RESET 진입 후 20 step에 SEEK */
    CHECK_EQ(seek_during_reset, 0);          /* RESET 중 seek 안 뜸 (순서) */
    CHECK_EQ(out.seek_signal, 1);            /* SEEK 진입 step에 seek active */
    CHECK_EQ(out.reset_signal, 0);           /* reset off */

    /* SEEK span = OSC_SEEK_TICKS, 종료 시 DONE. SEEK 동안 reset=0. */
    int seek_ticks = 1;
    int done_enter = -1;
    int reset_during_seek = 0;
    for (int i = 1; i < 30; i++) {
        osc_init_fsm_step(&in, &out);
        if (out.state == OSC_SEEK && out.reset_signal) { reset_during_seek = 1; }
        if (out.seek_signal && out.state == OSC_SEEK) { seek_ticks++; }
        if (out.state == OSC_DONE) { done_enter = i; break; }
    }
    CHECK_EQ(seek_ticks, OSC_SEEK_TICKS);    /* 10 */
    CHECK_EQ(done_enter, OSC_SEEK_TICKS);    /* SEEK 진입 후 10 step에 DONE */
    CHECK_EQ(reset_during_seek, 0);          /* SEEK 중 reset 안 뜸 */
}

/* 디바운스: 단일 H 스파이크(1 tick)는 무시 — WAIT_L로 전이 안 함. */
static void test_h_debounce_spike(void)
{
    osc_init_fsm_init();
    osc_init_in_t  in  = { .pb12 = 0u };
    osc_init_out_t out;

    osc_init_fsm_step(&in, &out);
    CHECK_EQ(out.state, OSC_WAIT_H);

    /* H 1 tick(스파이크) → 아직 WAIT_H (debounce 미달, DEBOUNCE>=2) */
    in.pb12 = 1u;
    osc_init_fsm_step(&in, &out);
    CHECK_EQ(out.state, OSC_WAIT_H);

    /* 바로 L → debounce 리셋, WAIT_H 유지 */
    in.pb12 = 0u;
    osc_init_fsm_step(&in, &out);
    CHECK_EQ(out.state, OSC_WAIT_H);

    /* 스파이크 반복해도 WAIT_L 안 감 */
    in.pb12 = 1u; osc_init_fsm_step(&in, &out);
    in.pb12 = 0u; osc_init_fsm_step(&in, &out);
    CHECK_EQ(out.state, OSC_WAIT_H);
}

/* 폴백: PB12가 계속 0(H 안 옴) → WAIT_H 타임아웃 후 진행. */
static void test_fallback_no_h(void)
{
    osc_init_fsm_init();
    osc_init_in_t  in  = { .pb12 = 0u };
    osc_init_out_t out;
    int left_wait_h = -1;
    for (int i = 1; i < 300; i++) {
        osc_init_fsm_step(&in, &out);
        if (out.state != OSC_WAIT_H) { left_wait_h = i; break; }
    }
    CHECK_EQ(left_wait_h, OSC_WAIT_H_TIMEOUT);   /* 90 tick 폴백 */
}

/* 폴백: PB12가 H(디바운스 통과) 후 안 떨어짐(계속 1) → WAIT_L 타임아웃 후 진행. */
static void test_fallback_no_l(void)
{
    osc_init_fsm_init();
    osc_init_in_t  in  = { .pb12 = 1u };
    osc_init_out_t out;

    /* 연속 H → WAIT_L 진입 (debounce) */
    int wait_l_at = -1;
    for (int i = 1; i <= 5; i++) {
        osc_init_fsm_step(&in, &out);
        if (out.state == OSC_WAIT_L) { wait_l_at = i; break; }
    }
    CHECK_EQ(wait_l_at, OSC_H_DEBOUNCE);

    /* pb12=1 유지 → WAIT_L 타임아웃 */
    int left_wait_l = -1;
    for (int i = 1; i < 300; i++) {
        osc_init_fsm_step(&in, &out);
        if (out.state != OSC_WAIT_L) { left_wait_l = i; break; }
    }
    CHECK_EQ(left_wait_l, OSC_WAIT_L_TIMEOUT);   /* 90 tick 폴백 */
    CHECK_EQ(out.state, OSC_GAP);
}

/* DONE은 terminal: 도달 후 계속 step해도 DONE 유지, 모든 signal 0. */
static void test_done_terminal(void)
{
    osc_init_fsm_init();
    osc_init_in_t  in  = { .pb12 = 1u };
    osc_init_out_t out;

    /* 빠른 경로로 DONE까지: H 디바운스 → L → GAP/RESET/SEEK 소진 */
    for (int i = 0; i < (int)OSC_H_DEBOUNCE; i++) {
        osc_init_fsm_step(&in, &out);            /* 연속 H → WAIT_L */
    }
    in.pb12 = 0u;
    for (int i = 0; i < 100; i++) {
        osc_init_fsm_step(&in, &out);
        if (out.state == OSC_DONE) { break; }
    }
    CHECK_EQ(out.state, OSC_DONE);

    /* DONE 유지 확인 */
    for (int i = 0; i < 20; i++) {
        osc_init_fsm_step(&in, &out);
        CHECK_EQ(out.state, OSC_DONE);
        CHECK_EQ(out.reset_signal, 0);
        CHECK_EQ(out.seek_signal, 0);
    }
}

int main(void)
{
    test_init_wait_h();
    test_normal_sequence();
    test_h_debounce_spike();
    test_fallback_no_h();
    test_fallback_no_l();
    test_done_terminal();
    if (failures) { printf("test_app_osc_init_fsm: %d FAILED\n", failures); return 1; }
    printf("test_app_osc_init_fsm: all passed\n");
    return 0;
}
