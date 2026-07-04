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
    int amp_change_cnt;        /* 0→1 전환 엣지 횟수 */
    uint8_t amp_at_change;     /* amp_change step의 amplitude (2단 진폭) */
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
        if (out.amp_change) { t->amp_change_cnt++; t->amp_at_change = out.amplitude; }
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

/* energy-exit 정상: energy_ctrl=1, WELD step마다 curr_energy 증가 주입 ->
 * curr_energy>=limit_energy에서 정확히 1회 weld_stop+HOLD, ldt2(=100)와 무관. */
static void test_energy_exit_normal(void)
{
    weld_fsm_init();
    weld_in_t in = { .start=1u, .run_mode=0u,
                     .limit_delay_time1=2u, .limit_delay_time2=100u,
                     .limit_delay_time3=2u, .output_power=100u,
                     .energy_ctrl=1u, .curr_energy=0u, .limit_energy=50u,
                     .limit_out_time=10u };
    int weld_stop=0, weld_fault=0, cycle_done=0, weld_steps=0;
    for (int i = 0; i < 2000; i++) {
        weld_out_t out;
        weld_fsm_step(&in, &out);
        in.start = 0u;
        /* 에너지는 step 후 주입 -> FSM은 1 step 지연된 값을 봄 (hence weld_steps<100 proxy). */
        if (out.run_status == WELD_WELD) { weld_steps++; in.curr_energy += 10u; }
        if (out.weld_stop)  weld_stop++;
        if (out.weld_fault) weld_fault++;
        if (out.cycle_done) { cycle_done++; break; }
    }
    CHECK_EQ(weld_stop, 1);       /* 에너지 도달 -> 1회 정상 종료 */
    CHECK_EQ(weld_fault, 0);      /* fault 아님 */
    CHECK_EQ(cycle_done, 1);      /* HOLD->CYL2->READY 정상 완주 -> work_cnt++ */
    if (weld_steps >= 100) {      /* ldt2=100; 에너지-exit면 그보다 훨씬 짧아야 */
        printf("FAIL energy exit took %d weld steps (>= ldt2=100)\n", weld_steps);
        failures++;
    }
}

/* 시간-exit 스킵: energy_ctrl=1, ldt2=2(작음), 에너지 미도달 -> 50 step 후에도
 * WELD 유지(ldt2 시간-exit가 안 일어남), fault/stop 없음. */
static void test_time_exit_skipped_when_energy(void)
{
    weld_fsm_init();
    weld_in_t in = { .start=1u, .run_mode=0u,
                     .limit_delay_time1=2u, .limit_delay_time2=2u,
                     .limit_delay_time3=2u, .output_power=100u,
                     .energy_ctrl=1u, .curr_energy=0u, .limit_energy=1000u,
                     .limit_out_time=10u };   /* backstop=1000 tick, 50 step 내 미만료 */
    int weld_stop=0, weld_fault=0; uint8_t status=0xffu;
    for (int i = 0; i < 50; i++) {
        weld_out_t out;
        weld_fsm_step(&in, &out);
        in.start = 0u;
        if (out.weld_stop)  weld_stop++;
        if (out.weld_fault) weld_fault++;
        status = out.run_status;
    }
    CHECK_EQ(weld_stop, 0);          /* ldt2=2 시간-exit 안 일어남 (스킵) */
    CHECK_EQ(weld_fault, 0);
    CHECK_EQ(status, WELD_WELD);     /* 여전히 용접 중 */
}

/* backstop abort: energy_ctrl=1, 에너지 영원히 미도달, limit_out_time=1초
 * (=100 tick) -> abort. weld_fault+weld_stop 각 1회, cycle_done 0(work_cnt 미발생),
 * READY 복귀, sol_dn OFF. */
static void test_backstop_abort(void)
{
    weld_fsm_init();
    weld_in_t in = { .start=1u, .run_mode=0u,
                     .limit_delay_time1=2u, .limit_delay_time2=5u,
                     .limit_delay_time3=2u, .output_power=100u,
                     .energy_ctrl=1u, .curr_energy=0u, .limit_energy=1000u,
                     .limit_out_time=1u };
    int weld_stop=0, weld_fault=0, cycle_done=0;
    uint8_t final_status=0xffu, final_sol=1u;
    for (int i = 0; i < 300; i++) {
        weld_out_t out;
        weld_fsm_step(&in, &out);
        in.start = 0u;
        if (out.weld_stop)  weld_stop++;
        if (out.weld_fault) weld_fault++;
        if (out.cycle_done) cycle_done++;
        final_status = out.run_status;
        final_sol    = out.sol_dn;
    }
    CHECK_EQ(weld_fault, 1);          /* backstop abort 1회 */
    CHECK_EQ(weld_stop, 1);           /* abort도 US 정지 */
    CHECK_EQ(cycle_done, 0);          /* work_cnt++ 안 함 */
    CHECK_EQ(final_status, WELD_READY);
    CHECK_EQ(final_sol, 0);           /* 실린더 상승 */
}

/* M1 회귀: limit_out_time=0이 floor되어 1s(=100 tick) backstop이 됨 — 0틱
 * 즉시-abort가 아님을 검증. floor 없으면 weld step ~1에서 abort. */
static void test_backstop_floor_zero(void)
{
    weld_fsm_init();
    weld_in_t in = { .start=1u, .run_mode=0u,
                     .limit_delay_time1=2u, .limit_delay_time2=5u,
                     .limit_delay_time3=2u, .output_power=100u,
                     .energy_ctrl=1u, .curr_energy=0u, .limit_energy=1000u,
                     .limit_out_time=0u };
    int weld_fault=0, weld_steps=0, weld_steps_at_fault=-1;
    for (int i = 0; i < 300; i++) {
        weld_out_t out;
        weld_fsm_step(&in, &out);
        in.start = 0u;
        if (out.run_status == WELD_WELD) weld_steps++;
        if (out.weld_fault) { weld_fault++; if (weld_steps_at_fault < 0) weld_steps_at_fault = weld_steps; }
    }
    CHECK_EQ(weld_fault, 1);              /* backstop 결국 동작 */
    if (weld_steps_at_fault < 50) {       /* floor 미적용이면 ~1 step에 abort */
        printf("FAIL backstop floor: aborted at weld step %d (<50; floor not applied)\n",
               weld_steps_at_fault);
        failures++;
    }
}

/* M1(감사): energy_ctrl=1 + limit_energy=0 -> 즉시 무증상 "정상완료" 금지.
 * limit_out_time=1(backstop=100 tick)로 abort 종료해야 함. */
static void test_weld_energy_limit_zero_no_instant_stop(void)
{
    weld_fsm_init();
    weld_in_t in; memset(&in, 0, sizeof(in));
    in.limit_delay_time1 = 2u; in.limit_delay_time2 = 10u; in.limit_delay_time3 = 2u;
    in.output_power = 100u;
    in.energy_ctrl = 1u; in.limit_energy = 0u; in.curr_energy = 0u;
    in.limit_out_time = 1u;                   /* backstop 1s = 100 tick */
    weld_out_t out;
    in.start = 1u;
    weld_fsm_step(&in, &out);
    in.start = 0u;
    for (int i = 0; i < 3; i++) { weld_fsm_step(&in, &out); }
    CHECK_EQ(weld_fsm_status(), WELD_WELD);
    weld_fsm_step(&in, &out);
    CHECK_EQ(out.weld_stop, 0u);              /* 즉시 정상완료 금지 */
    int faults = 0;
    for (int i = 0; i < 150 && weld_fsm_status() == WELD_WELD; i++) {
        weld_fsm_step(&in, &out);
        faults += out.weld_fault;
    }
    CHECK_EQ(faults, 1);                      /* backstop abort로 종료 */
    CHECK_EQ(weld_fsm_status(), WELD_READY);
}

/* multi 스테핑: out1=75 -> 1단 진폭 63 at weld_start, time1(=5) step에서 amp_change
 * +2단 진폭(out2=100 -> 127), time2(=12) step에서 weld_stop+HOLD, 정상 사이클 완주. */
static void test_multi_stepping(void)
{
    weld_fsm_init();
    weld_in_t in = { .start=0u, .run_mode=0u,
                     .limit_delay_time1=2u, .limit_delay_time2=100u,
                     .limit_delay_time3=2u, .output_power=100u,
                     .multi_ctrl=1u, .limit_mo_out1=75u, .limit_mo_out2=100u,
                     .limit_mo_time1=5u, .limit_mo_time2=12u };
    trace_t t;
    run_cycle(&in, &t, 300);
    CHECK_EQ(t.weld_start_cnt, 1);
    CHECK_EQ(t.weld_amp, 63u);          /* out1=75 -> (75-50)*255/100 = 63 (1단) */
    CHECK_EQ(t.amp_change_cnt, 1);      /* 0->1 전환 1회 */
    CHECK_EQ(t.amp_at_change, 127u);    /* out2=100 -> 127 (2단) */
    CHECK_EQ(t.weld_stop_cnt, 1);
    CHECK_EQ(t.cycle_done_cnt, 1);      /* graceful HOLD->CYL2->READY 완주 */
    CHECK_EQ(t.weld_steps, 12);         /* limit_mo_time2 (ldt2=100 무관) */
}

/* multi가 energy override: multi_ctrl=1 + energy_ctrl=1, curr_energy를 limit
 * 훨씬 초과 주입해도 energy-exit/backstop 미발동, time2에 종료. */
static void test_multi_overrides_energy(void)
{
    weld_fsm_init();
    weld_in_t in = { .start=1u, .run_mode=0u,
                     .limit_delay_time1=2u, .limit_delay_time2=100u,
                     .limit_delay_time3=2u, .output_power=100u,
                     .multi_ctrl=1u, .limit_mo_out1=60u, .limit_mo_out2=80u,
                     .limit_mo_time1=3u, .limit_mo_time2=8u,
                     .energy_ctrl=1u, .curr_energy=99999u, .limit_energy=1u,
                     .limit_out_time=10u };
    int weld_stop=0, weld_steps=0, fault=0;
    for (int i = 0; i < 300; i++) {
        weld_out_t out;
        weld_fsm_step(&in, &out);
        in.start = 0u;
        if (out.run_status == WELD_WELD) weld_steps++;
        if (out.weld_stop)  weld_stop++;
        if (out.weld_fault) fault++;
        if (out.cycle_done) break;
    }
    CHECK_EQ(weld_stop, 1);
    CHECK_EQ(fault, 0);          /* energy exit/backstop 미발동 (multi 우선) */
    CHECK_EQ(weld_steps, 8);     /* time2, energy 무관 */
}

/* multi가 DELAY override: multi_ctrl=1, ldt2=2(작음), s_temp_time 만료 무시, time2에 종료. */
static void test_multi_overrides_delay(void)
{
    weld_fsm_init();
    weld_in_t in = { .start=1u, .run_mode=0u,
                     .limit_delay_time1=2u, .limit_delay_time2=2u,
                     .limit_delay_time3=2u, .output_power=100u,
                     .multi_ctrl=1u, .limit_mo_out1=60u, .limit_mo_out2=80u,
                     .limit_mo_time1=4u, .limit_mo_time2=10u };
    int weld_steps=0, weld_stop=0;
    for (int i = 0; i < 300; i++) {
        weld_out_t out;
        weld_fsm_step(&in, &out);
        in.start = 0u;
        if (out.run_status == WELD_WELD) weld_steps++;
        if (out.weld_stop) weld_stop++;
        if (out.cycle_done) break;
    }
    CHECK_EQ(weld_steps, 10);    /* time2, NOT ldt2=2 */
    CHECK_EQ(weld_stop, 1);
}

/* 언더플로 가드: limit_mo_out1=25(<50) -> 1단 진폭 0(wrap 금지), out2=60 -> 25 정상 전환. */
static void test_multi_underflow_guard(void)
{
    weld_fsm_init();
    weld_in_t in = { .start=0u, .run_mode=0u,
                     .limit_delay_time1=2u, .limit_delay_time2=100u,
                     .limit_delay_time3=2u, .output_power=100u,
                     .multi_ctrl=1u, .limit_mo_out1=25u, .limit_mo_out2=60u,
                     .limit_mo_time1=3u, .limit_mo_time2=8u };
    trace_t t;
    run_cycle(&in, &t, 300);
    CHECK_EQ(t.weld_amp, 0u);          /* out1=25<50 -> 0 (가드) */
    CHECK_EQ(t.amp_at_change, 25u);    /* out2=60 -> (60-50)*255/100 = 25 */
}

/* stage 리셋: 1사이클 후 재실행 시 stage/elapsed 재시작 -> 2회차도 1단 진폭부터. */
static void test_multi_stage_reset(void)
{
    weld_fsm_init();
    weld_in_t base = { .start=0u, .run_mode=0u,
                       .limit_delay_time1=2u, .limit_delay_time2=100u,
                       .limit_delay_time3=2u, .output_power=100u,
                       .multi_ctrl=1u, .limit_mo_out1=75u, .limit_mo_out2=100u,
                       .limit_mo_time1=3u, .limit_mo_time2=7u };
    trace_t t1, t2;
    run_cycle(&base, &t1, 300);
    run_cycle(&base, &t2, 300);   /* READY 복귀 후 재실행 */
    CHECK_EQ(t2.weld_amp, 63u);          /* 2회차도 1단 진폭(out1=75->63)부터 */
    CHECK_EQ(t2.amp_change_cnt, 1);      /* 2회차도 전환 1회 */
    CHECK_EQ(t2.amp_at_change, 127u);
}

/* 경계 time1==time2: 전환과 종료가 같은 step (정의된 거동 동결: 둘 다 같은 step). */
static void test_multi_boundary_equal(void)
{
    weld_fsm_init();
    weld_in_t in = { .start=1u, .run_mode=0u,
                     .limit_delay_time1=2u, .limit_delay_time2=100u,
                     .limit_delay_time3=2u, .output_power=100u,
                     .multi_ctrl=1u, .limit_mo_out1=60u, .limit_mo_out2=100u,
                     .limit_mo_time1=5u, .limit_mo_time2=5u };
    int amp_change=0, weld_stop=0, both_same_step=0;
    for (int i = 0; i < 300; i++) {
        weld_out_t out;
        weld_fsm_step(&in, &out);
        in.start = 0u;
        if (out.amp_change) amp_change++;
        if (out.weld_stop)  weld_stop++;
        if (out.amp_change && out.weld_stop) both_same_step++;
        if (out.cycle_done) break;
    }
    CHECK_EQ(amp_change, 1);
    CHECK_EQ(weld_stop, 1);
    CHECK_EQ(both_same_step, 1);   /* 전환+종료 같은 step */
}

/* 경계 time1>time2: 종료가 먼저 -> 단일 진폭(out1)로 종료, amp_change 없음. */
static void test_multi_boundary_time1_gt_time2(void)
{
    weld_fsm_init();
    weld_in_t in = { .start=1u, .run_mode=0u,
                     .limit_delay_time1=2u, .limit_delay_time2=100u,
                     .limit_delay_time3=2u, .output_power=100u,
                     .multi_ctrl=1u, .limit_mo_out1=60u, .limit_mo_out2=100u,
                     .limit_mo_time1=20u, .limit_mo_time2=6u };
    int amp_change=0, weld_stop=0, weld_steps=0;
    for (int i = 0; i < 300; i++) {
        weld_out_t out;
        weld_fsm_step(&in, &out);
        in.start = 0u;
        if (out.run_status == WELD_WELD) weld_steps++;
        if (out.amp_change) amp_change++;
        if (out.weld_stop)  weld_stop++;
        if (out.cycle_done) break;
    }
    CHECK_EQ(amp_change, 0);     /* time1=20 미도달 -> 전환 없음 */
    CHECK_EQ(weld_stop, 1);
    CHECK_EQ(weld_steps, 6);     /* time2에 종료 */
}

/* 진폭 known-vector: out1/out2 = 50/100 -> 0/127 (역순/경계 포함 변환 검증). */
static void test_multi_amp_vectors(void)
{
    weld_fsm_init();
    weld_in_t in = { .start=0u, .run_mode=0u,
                     .limit_delay_time1=2u, .limit_delay_time2=100u,
                     .limit_delay_time3=2u, .output_power=100u,
                     .multi_ctrl=1u, .limit_mo_out1=50u, .limit_mo_out2=100u,
                     .limit_mo_time1=3u, .limit_mo_time2=8u };
    trace_t t;
    run_cycle(&in, &t, 300);
    CHECK_EQ(t.weld_amp, 0u);          /* out1=50 -> 0 */
    CHECK_EQ(t.amp_at_change, 127u);   /* out2=100 -> 127 */
}

/* multi_ctrl=0 회귀: 기존 슬라이스1 시간-exit 시퀀스 동일 (multi 분기 미진입). */
static void test_multi_off_regression(void)
{
    weld_fsm_init();
    weld_in_t in = { .start=0u, .run_mode=0u,
                     .limit_delay_time1=3u, .limit_delay_time2=10u,
                     .limit_delay_time3=2u, .output_power=100u,
                     .multi_ctrl=0u, .limit_mo_out1=75u, .limit_mo_out2=100u,
                     .limit_mo_time1=2u, .limit_mo_time2=4u };
    trace_t t;
    run_cycle(&in, &t, 300);
    CHECK_EQ(t.weld_steps, 10);        /* ldt2 시간-exit (multi 무시) */
    CHECK_EQ(t.amp_change_cnt, 0);     /* 전환 없음 */
    CHECK_EQ(t.weld_amp, 127u);        /* output_power=100 진폭 (limit_mo_out 무시) */
    CHECK_EQ(t.cycle_done_cnt, 1);
}

/* H1: WELD 진행 중 multi_ctrl 토글(0->1)이 exit 경로를 바꾸지 못함 —
 * 시간-exit로 정상 완료해야 함 (감사 H1 근본수정). */
static void test_h1_multi_toggle_mid_weld_ignored(void)
{
    weld_fsm_init();
    weld_in_t in;
    memset(&in, 0, sizeof(in));
    in.limit_delay_time1 = 2u;   /* CYL1/CYL2 = 2 tick */
    in.limit_delay_time2 = 10u;  /* WELD = 10 tick (시간 모드) */
    in.limit_delay_time3 = 2u;
    in.output_power      = 100u;
    in.limit_mo_out1 = 60u; in.limit_mo_out2 = 80u;
    in.limit_mo_time1 = 1u;  in.limit_mo_time2 = 2u;  /* 토글이 반영되면 2 tick만에 스테핑 종료 */

    weld_out_t out;
    in.start = 1u;
    weld_fsm_step(&in, &out);            /* READY -> CYL1 */
    in.start = 0u;
    for (int i = 0; i < 3; i++) { weld_fsm_step(&in, &out); }  /* CYL1 소진 -> WELD 진입 */
    CHECK_EQ(weld_fsm_status(), WELD_WELD);
    CHECK_EQ(out.weld_start, 1u);

    in.multi_ctrl = 1u;                  /* 런중 토글 */
    int amp_changes = 0, stops = 0;
    for (int i = 0; i < 15 && weld_fsm_status() == WELD_WELD; i++) {
        weld_fsm_step(&in, &out);
        amp_changes += out.amp_change;
        stops       += out.weld_stop;
    }
    CHECK_EQ(amp_changes, 0);            /* 스테핑 미발동 = 래치 유효 */
    CHECK_EQ(stops, 1);                  /* 시간-exit 1회로 정상 HOLD 진입 */
    CHECK_EQ(weld_fsm_status(), WELD_HOLD);
}

/* H1: multi로 시작한 WELD는 런중 multi_ctrl=0 토글에도 스테핑 계속. */
static void test_h1_multi_off_toggle_mid_weld_ignored(void)
{
    weld_fsm_init();
    weld_in_t in;
    memset(&in, 0, sizeof(in));
    in.limit_delay_time1 = 2u; in.limit_delay_time2 = 10u; in.limit_delay_time3 = 2u;
    in.output_power = 100u;
    in.multi_ctrl = 1u;
    in.limit_mo_out1 = 60u; in.limit_mo_out2 = 80u;
    in.limit_mo_time1 = 3u; in.limit_mo_time2 = 6u;

    weld_out_t out;
    in.start = 1u;
    weld_fsm_step(&in, &out);
    in.start = 0u;
    for (int i = 0; i < 3; i++) { weld_fsm_step(&in, &out); }
    CHECK_EQ(weld_fsm_status(), WELD_WELD);

    in.multi_ctrl = 0u;                  /* 런중 off 토글 */
    int amp_changes = 0;
    for (int i = 0; i < 10 && weld_fsm_status() == WELD_WELD; i++) {
        weld_fsm_step(&in, &out);
        amp_changes += out.amp_change;
    }
    CHECK_EQ(amp_changes, 1);            /* 2단 전환 발동 = multi 래치 유지 */
    CHECK_EQ(weld_fsm_status(), WELD_HOLD);
}

/* H1: 이전 multi 사이클의 s_multi_stage/elapsed가 다음 사이클에 누출되지 않음. */
static void test_h1_counters_reset_between_cycles(void)
{
    weld_fsm_init();
    weld_in_t in;
    memset(&in, 0, sizeof(in));
    in.limit_delay_time1 = 2u; in.limit_delay_time2 = 10u; in.limit_delay_time3 = 2u;
    in.output_power = 100u;
    in.multi_ctrl = 1u;
    in.limit_mo_out1 = 60u; in.limit_mo_out2 = 80u;
    in.limit_mo_time1 = 3u; in.limit_mo_time2 = 6u;

    trace_t t;
    run_cycle(&in, &t, 200);             /* multi 사이클 1회 완주 */
    CHECK_EQ(t.amp_change_cnt, 1);

    run_cycle(&in, &t, 200);             /* 두 번째 사이클 — 카운터 fresh */
    CHECK_EQ(t.amp_change_cnt, 1);       /* stage 0부터 다시: 전환 정확히 1회 */
    CHECK_EQ(t.cycle_done_cnt, 1);
}

/* abort: 각 상태에서 -> SOL OFF + READY + work_cnt 미발행; WELD에서만 weld_stop. */
static void test_abort_from_each_state(void)
{
    /* CYL1/WELD/HOLD/CYL2 도달 step 수 — 실측 보정(brief 원안 {1,4,6,9}은 comp_time
     * quirk 미반영: ldt2<=6이면 WELD가 최소 7 step 점유(WELD_COMP_FULL) 하므로
     * HOLD는 step10, CYL2는 step12에 진입 (trace 실측, task-2-report.md 참조). */
    static const int steps_to_state[] = {1, 4, 10, 12};
    static const uint8_t expect_state[] = {WELD_CYL1, WELD_WELD, WELD_HOLD, WELD_CYL2};
    for (int s = 0; s < 4; s++) {
        weld_fsm_init();
        weld_in_t in;
        memset(&in, 0, sizeof(in));
        in.limit_delay_time1 = 2u; in.limit_delay_time2 = 2u; in.limit_delay_time3 = 2u;
        in.output_power = 100u;
        weld_out_t out;
        in.start = 1u;
        weld_fsm_step(&in, &out);
        in.start = 0u;
        for (int i = 1; i < steps_to_state[s]; i++) { weld_fsm_step(&in, &out); }
        CHECK_EQ(weld_fsm_status(), expect_state[s]);
        uint8_t was_weld = (weld_fsm_status() == WELD_WELD);

        in.abort = 1u;
        weld_fsm_step(&in, &out);
        CHECK_EQ(weld_fsm_status(), WELD_READY);
        CHECK_EQ(out.sol_dn, 0u);
        CHECK_EQ(out.cycle_done, 0u);
        CHECK_EQ(out.weld_stop, was_weld ? 1u : 0u);
        in.abort = 0u;

        /* abort 후 재시작 정상 */
        trace_t t;
        run_cycle(&in, &t, 200);
        CHECK_EQ(t.cycle_done_cnt, 1);
    }
}

/* abort: READY에서는 no-op. */
static void test_abort_in_ready_noop(void)
{
    weld_fsm_init();
    weld_in_t in; memset(&in, 0, sizeof(in));
    in.abort = 1u;
    weld_out_t out;
    weld_fsm_step(&in, &out);
    CHECK_EQ(weld_fsm_status(), WELD_READY);
    CHECK_EQ(out.weld_stop, 0u);
}

/* TRIGGER: 풀 사이클 — CYL1은 dn_edge까지 대기, HOLD=trigger_time3, CYL2 즉시. */
static void test_trigger_full_cycle(void)
{
    weld_fsm_init();
    weld_in_t in; memset(&in, 0, sizeof(in));
    in.run_mode          = 1u;    /* MODE_TRIGGER */
    in.limit_delay_time1 = 50u;   /* TRIGGER에선 미사용이어야 함 */
    in.limit_trigger_time2 = 3u;  /* WELD 3 tick (comp swap: <=6 -> comp=3, temp=7) */
    in.limit_trigger_time3 = 2u;  /* HOLD 2 tick */
    in.output_power = 100u;
    weld_out_t out;

    in.start = 1u;
    weld_fsm_step(&in, &out);                 /* READY -> CYL1 */
    in.start = 0u;
    CHECK_EQ(weld_fsm_status(), WELD_CYL1);

    for (int i = 0; i < 20; i++) { weld_fsm_step(&in, &out); }
    CHECK_EQ(weld_fsm_status(), WELD_CYL1);   /* dn 없음 -> 무기한 대기 (타임아웃 없음, 죽은 CYL_TIMEOUT 충실) */
    CHECK_EQ(out.sol_dn, 1u);

    in.dn_edge = 1u;
    weld_fsm_step(&in, &out);                 /* dn 소비 -> WELD */
    in.dn_edge = 0u;
    CHECK_EQ(weld_fsm_status(), WELD_WELD);

    int stops = 0, dones = 0, hold_steps = 0, cyl2_steps = 0;
    for (int i = 0; i < 30 && weld_fsm_status() != WELD_READY; i++) {
        weld_fsm_step(&in, &out);
        stops += out.weld_stop;
        dones += out.cycle_done;
        if (weld_fsm_status() == WELD_HOLD) hold_steps++;
        if (weld_fsm_status() == WELD_CYL2) cyl2_steps++;
    }
    CHECK_EQ(stops, 1);
    CHECK_EQ(dones, 1);                       /* work_cnt 경로 정상 */
    CHECK_EQ((cyl2_steps <= 2), 1);           /* CYL2 = up 강제 -> 사실상 즉시 (§3.3) */
    CHECK_EQ((hold_steps >= 1), 1);           /* HOLD를 실제로 거침 (trigger_time3) */
}

/* TRIGGER: READY 중 들어온 dn_edge(stale)는 사이클 시작 시 클리어 (main.c:1478). */
static void test_trigger_stale_dn_cleared_at_start(void)
{
    weld_fsm_init();
    weld_in_t in; memset(&in, 0, sizeof(in));
    in.run_mode = 1u;
    in.limit_trigger_time2 = 3u; in.limit_trigger_time3 = 2u;
    in.output_power = 100u;
    weld_out_t out;

    in.dn_edge = 1u;
    weld_fsm_step(&in, &out);                 /* READY에서 stale 엣지 */
    in.dn_edge = 0u;
    in.start = 1u;
    weld_fsm_step(&in, &out);                 /* 시작 — stale 클리어돼야 함 */
    in.start = 0u;
    weld_fsm_step(&in, &out);                 /* CYL1 최초진입(SOL ON) */
    for (int i = 0; i < 5; i++) { weld_fsm_step(&in, &out); }
    CHECK_EQ(weld_fsm_status(), WELD_CYL1);   /* stale로 exit하지 않음 */
}

/* run_mode 래치: DELAY로 시작한 사이클은 런중 TRIGGER 전환에도 DELAY 규칙 유지. */
static void test_run_mode_latched_at_cycle_start(void)
{
    weld_fsm_init();
    weld_in_t in; memset(&in, 0, sizeof(in));
    in.run_mode = 0u;
    in.limit_delay_time1 = 2u; in.limit_delay_time2 = 2u; in.limit_delay_time3 = 2u;
    in.output_power = 100u;
    weld_out_t out;
    in.start = 1u;
    weld_fsm_step(&in, &out);
    in.start = 0u;
    in.run_mode = 1u;                         /* 런중 전환 */
    int done = 0;                             /* 이어서 그대로 완주 (DELAY 시간 규칙) */
    for (int i = 0; i < 50; i++) {
        weld_fsm_step(&in, &out);
        done += out.cycle_done;
    }
    CHECK_EQ(done, 1);                        /* dn_edge 없이 완주 = DELAY 래치 유지 */
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
    test_energy_exit_normal();
    test_time_exit_skipped_when_energy();
    test_backstop_abort();
    test_backstop_floor_zero();
    test_weld_energy_limit_zero_no_instant_stop();
    test_multi_stepping();
    test_multi_overrides_energy();
    test_multi_overrides_delay();
    test_multi_underflow_guard();
    test_multi_stage_reset();
    test_multi_boundary_equal();
    test_multi_boundary_time1_gt_time2();
    test_multi_amp_vectors();
    test_multi_off_regression();
    test_h1_multi_toggle_mid_weld_ignored();
    test_h1_multi_off_toggle_mid_weld_ignored();
    test_h1_counters_reset_between_cycles();
    test_abort_from_each_state();
    test_abort_in_ready_noop();
    test_trigger_full_cycle();
    test_trigger_stale_dn_cleared_at_start();
    test_run_mode_latched_at_cycle_start();
    if (failures) { printf("test_app_weld_fsm: %d FAILED\n", failures); return 1; }
    printf("test_app_weld_fsm: all passed\n");
    return 0;
}
