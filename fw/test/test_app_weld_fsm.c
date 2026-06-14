/* fw/test/test_app_weld_fsm.c — host unit tests for the pure weld-cycle FSM
 * core (slice 1, DELAY mode). No HAL, no hardware. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "app_weld_fsm.h"

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

static void test_init_ready(void)
{
    weld_fsm_init();
    CHECK_EQ(weld_fsm_status(), WELD_READY);
}

/* 한 사이클을 구동하며 상태별 step 수 + 이벤트를 집계 (DELAY 모드). */
typedef struct {
    int cyl1_steps, weld_steps, hold_steps, cyl2_steps;
    int weld_start_cnt, weld_stop_cnt, cycle_done_cnt;
    int sol_on_edges, sol_off_edges;
    uint8_t weld_amp;
    int prev_sol;
} trace_t;

static void run_cycle(const weld_in_t *base, trace_t *t, int max_steps)
{
    memset(t, 0, sizeof(*t));
    weld_in_t in = *base;
    in.start = 1u;                 /* trigger on the first step */
    for (int i = 0; i < max_steps; i++) {
        weld_out_t out;
        weld_fsm_step(&in, &out);
        in.start = 0u;             /* one-shot */
        switch (out.run_status) {
            case WELD_CYL1: t->cyl1_steps++; break;
            case WELD_WELD: t->weld_steps++; break;
            case WELD_HOLD: t->hold_steps++; break;
            case WELD_CYL2: t->cyl2_steps++; break;
            default: break;
        }
        if (out.weld_start) { t->weld_start_cnt++; t->weld_amp = out.amplitude; }
        if (out.weld_stop)  { t->weld_stop_cnt++; }
        if (out.cycle_done) { t->cycle_done_cnt++; }
        if (out.sol_dn && !t->prev_sol)  { t->sol_on_edges++; }
        if (!out.sol_dn && t->prev_sol)  { t->sol_off_edges++; }
        t->prev_sol = out.sol_dn;
        if (out.cycle_done) { break; }
    }
}

/* 완전 DELAY 사이클: 이벤트가 정확히 1회씩, SOL on→off, READY 복귀. */
static void test_full_cycle_delay(void)
{
    weld_fsm_init();
    weld_in_t in = { .start=0u, .run_mode=0u,
                     .limit_delay_time1=3u, .limit_delay_time2=10u,
                     .limit_delay_time3=2u, .output_power=100u };
    trace_t t;
    run_cycle(&in, &t, 200);
    CHECK_EQ(t.weld_start_cnt, 1);
    CHECK_EQ(t.weld_stop_cnt, 1);
    CHECK_EQ(t.cycle_done_cnt, 1);
    CHECK_EQ(t.sol_on_edges, 1);
    CHECK_EQ(t.sol_off_edges, 1);
    CHECK_EQ(weld_fsm_status(), WELD_READY);   /* 사이클 후 READY 복귀 */
}

/* 각 상태의 점유 step 수 = 해당 limit (out.run_status 기준). */
static void test_timing_durations(void)
{
    weld_fsm_init();
    weld_in_t in = { .start=0u, .run_mode=0u,
                     .limit_delay_time1=5u, .limit_delay_time2=8u,
                     .limit_delay_time3=4u, .output_power=100u };
    trace_t t;
    run_cycle(&in, &t, 200);
    CHECK_EQ(t.cyl1_steps, 5);   /* limit_delay_time1 */
    CHECK_EQ(t.weld_steps, 8);   /* limit_delay_time2 */
    CHECK_EQ(t.hold_steps, 4);   /* limit_delay_time3 */
    CHECK_EQ(t.cyl2_steps, 5);   /* limit_delay_time1 */
}

/* 진폭 base: output_power 50/75/100 -> 0/63/127 (comp_time=7, ldt2>6). */
static void test_amplitude_values(void)
{
    uint8_t op[3]  = { 50u, 75u, 100u };
    uint8_t exp[3] = { 0u,  63u, 127u };
    for (int k = 0; k < 3; k++) {
        weld_fsm_init();
        weld_in_t in = { .start=0u, .run_mode=0u,
                         .limit_delay_time1=2u, .limit_delay_time2=10u,
                         .limit_delay_time3=2u, .output_power=op[k] };
        trace_t t;
        run_cycle(&in, &t, 200);
        CHECK_EQ(t.weld_amp, exp[k]);
    }
}

/* comp_time quirk: ldt2<=6 -> WELD가 7 step, 진폭 -= (7-ldt2)*10.
 * ldt2=3, op=100 -> base 127, comp_time=3, 127-(7-3)*10 = 87. */
static void test_comp_time_reduction(void)
{
    weld_fsm_init();
    weld_in_t in = { .start=0u, .run_mode=0u,
                     .limit_delay_time1=2u, .limit_delay_time2=3u,
                     .limit_delay_time3=2u, .output_power=100u };
    trace_t t;
    run_cycle(&in, &t, 200);
    CHECK_EQ(t.weld_steps, 7);    /* 짧은 용접 -> 최소 7 step */
    CHECK_EQ(t.weld_amp, 87u);    /* 127 - 40 */
}

/* 언더플로 가드: ldt2=0 -> comp_time=0, 감쇠 70; op=50 -> base 0 -> 0 (wrap 금지). */
static void test_amplitude_underflow_guard(void)
{
    weld_fsm_init();
    weld_in_t in = { .start=0u, .run_mode=0u,
                     .limit_delay_time1=2u, .limit_delay_time2=0u,
                     .limit_delay_time3=2u, .output_power=50u };
    trace_t t;
    run_cycle(&in, &t, 200);
    CHECK_EQ(t.weld_amp, 0u);     /* 0 - 70 -> 0 (언더플로 가드) */
}

/* READY 밖 start 무시: 사이클 도중 start=1 재주입해도 두 번째 weld_start 없음. */
static void test_start_ignored_outside_ready(void)
{
    weld_fsm_init();
    weld_in_t in = { .start=1u, .run_mode=0u,
                     .limit_delay_time1=3u, .limit_delay_time2=10u,
                     .limit_delay_time3=3u, .output_power=100u };
    int weld_starts = 0;
    for (int i = 0; i < 200; i++) {
        weld_out_t out;
        weld_fsm_step(&in, &out);
        in.start = 1u;            /* 매 step start 유지 — READY 밖에선 무시돼야 */
        if (out.weld_start) { weld_starts++; }
        if (out.cycle_done) { break; }
    }
    CHECK_EQ(weld_starts, 1);     /* 사이클당 정확히 1회 */
}

/* cycle_done은 1-step 엣지: READY 복귀 후 start=0 step들에서 재발화 없음. */
static void test_cycle_done_single_edge(void)
{
    weld_fsm_init();
    weld_in_t in = { .start=0u, .run_mode=0u,
                     .limit_delay_time1=2u, .limit_delay_time2=4u,
                     .limit_delay_time3=2u, .output_power=100u };
    trace_t t;
    run_cycle(&in, &t, 200);
    CHECK_EQ(t.cycle_done_cnt, 1);
    /* READY 복귀 후 추가 step (start=0) -> cycle_done 0 유지 */
    int extra = 0;
    in.start = 0u;
    for (int i = 0; i < 20; i++) {
        weld_out_t out;
        weld_fsm_step(&in, &out);
        if (out.cycle_done) { extra++; }
    }
    CHECK_EQ(extra, 0);
}

int main(void)
{
    test_init_ready();
    test_full_cycle_delay();
    test_timing_durations();
    test_amplitude_values();
    test_comp_time_reduction();
    test_amplitude_underflow_guard();
    test_start_ignored_outside_ready();
    test_cycle_done_single_edge();
    if (failures) { printf("test_app_weld_fsm: %d FAILED\n", failures); return 1; }
    printf("test_app_weld_fsm: all passed\n");
    return 0;
}
