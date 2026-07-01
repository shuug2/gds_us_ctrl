/* fw/test/test_app_seek_reset_fsm.c — host unit tests for the pure SEEK/RESET
 * FSM core. No HAL, no hardware. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "app_seek_reset_fsm.h"

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

static void test_init_idle(void)
{
    seek_reset_fsm_init();
    CHECK_EQ(seek_reset_fsm_state(), SR_IDLE);
}

/* RESET 자동 체인: RESET → SR_RESET(reset_signal=1·reset_icon=1) → 60 tick 후
 * SR_SEEK(reset_signal=0·reset_icon_off=1·seek_signal=1·seek_icon=1) → 또 60 tick
 * 후 SR_IDLE(seek_signal=0·seek_icon_off=1). signal span 각 60 tick. spec §7-1. */
static void test_reset_auto_chain(void)
{
    seek_reset_fsm_init();
    seek_reset_in_t in = { .cmd = SR_CMD_RESET, .run_active = 0u };
    seek_reset_out_t out;

    /* 진입 tick: SR_RESET, reset on 엣지 */
    seek_reset_fsm_step(&in, &out);
    CHECK_EQ(out.state, SR_RESET);
    CHECK_EQ(out.reset_signal, 1);
    CHECK_EQ(out.reset_icon, 1);
    CHECK_EQ(out.seek_signal, 0);

    in.cmd = SR_CMD_NONE;
    int reset_sig_ticks = 1;      /* 진입 tick 1회 카운트 */
    int chain_at = -1;
    /* 체인 전이까지 step (진입 후 추가 step). */
    for (int i = 1; i < 70; i++) {
        seek_reset_fsm_step(&in, &out);
        if (out.state == SR_RESET) { reset_sig_ticks++; }
        if (out.reset_icon_off && out.seek_icon) {   /* 체인 전이 step */
            chain_at = i;
            CHECK_EQ(out.state, SR_SEEK);
            CHECK_EQ(out.reset_signal, 0);
            CHECK_EQ(out.seek_signal, 1);
            CHECK_EQ(out.seek_icon, 1);
            break;
        }
    }
    CHECK_EQ(reset_sig_ticks, 60);   /* reset_signal active span = 60 tick */
    CHECK_EQ(chain_at, 60);          /* 60번째 step(진입=0번째)에서 체인 */

    /* SEEK span: 체인 step에서 seek on. 60 tick 후 자동 해제. */
    int seek_sig_ticks = 1;          /* 체인 step의 seek_signal=1 카운트 */
    int release_at = -1;
    for (int i = 1; i < 70; i++) {
        seek_reset_fsm_step(&in, &out);
        if (out.state == SR_SEEK) { seek_sig_ticks++; }
        if (out.seek_icon_off) {     /* 해제 전이 step */
            release_at = i;
            CHECK_EQ(out.state, SR_IDLE);
            CHECK_EQ(out.seek_signal, 0);
            break;
        }
    }
    CHECK_EQ(seek_sig_ticks, 60);    /* seek_signal active span = 60 tick */
    CHECK_EQ(release_at, 60);
}

/* SEEK 단발(체인 없음): SEEK → SR_SEEK → 60 tick 후 SR_IDLE 직행. reset_signal
 * 내내 0. spec §7-2. */
static void test_seek_oneshot_no_chain(void)
{
    seek_reset_fsm_init();
    seek_reset_in_t in = { .cmd = SR_CMD_SEEK, .run_active = 0u };
    seek_reset_out_t out;

    seek_reset_fsm_step(&in, &out);
    CHECK_EQ(out.state, SR_SEEK);
    CHECK_EQ(out.seek_signal, 1);
    CHECK_EQ(out.seek_icon, 1);
    CHECK_EQ(out.reset_signal, 0);

    in.cmd = SR_CMD_NONE;
    int reset_seen = 0;
    int idle_at = -1;
    for (int i = 1; i < 70; i++) {
        seek_reset_fsm_step(&in, &out);
        if (out.reset_signal) { reset_seen = 1; }
        if (out.state == SR_IDLE) { idle_at = i; break; }
    }
    CHECK_EQ(reset_seen, 0);          /* RESET 신호 한 번도 안 뜸 (체인 없음) */
    CHECK_EQ(idle_at, 60);            /* 60번째 step에서 IDLE 직행 */
}

/* RUN 직교: run_active=1이면 RESET/SEEK 명령 무시 (SR_IDLE 유지, signal 0). spec §7-3. */
static void test_run_orthogonal(void)
{
    seek_reset_fsm_init();
    seek_reset_in_t in = { .cmd = SR_CMD_RESET, .run_active = 1u };
    seek_reset_out_t out;
    seek_reset_fsm_step(&in, &out);
    CHECK_EQ(out.state, SR_IDLE);
    CHECK_EQ(out.reset_signal, 0);
    CHECK_EQ(out.reset_icon, 0);

    in.cmd = SR_CMD_SEEK;
    seek_reset_fsm_step(&in, &out);
    CHECK_EQ(out.state, SR_IDLE);
    CHECK_EQ(out.seek_signal, 0);
}

/* busy 중 재명령 무시: SR_RESET 진행 중 SEEK/RESET 재주입 → 무시 (타이밍·전이 불변).
 * spec §7-4. */
static void test_busy_command_ignored(void)
{
    seek_reset_fsm_init();
    seek_reset_in_t in = { .cmd = SR_CMD_RESET, .run_active = 0u };
    seek_reset_out_t out;
    seek_reset_fsm_step(&in, &out);          /* 진입 SR_RESET */

    /* 진행 중 SEEK 재주입을 매 tick 시도 — 무시되어 RESET 타이밍 그대로 60에서 체인 */
    in.cmd = SR_CMD_SEEK;
    int chain_at = -1;
    for (int i = 1; i < 70; i++) {
        seek_reset_fsm_step(&in, &out);
        if (out.reset_icon_off && out.seek_icon) { chain_at = i; break; }
    }
    CHECK_EQ(chain_at, 60);                   /* 재명령 무시 → 정상 체인 타이밍 */
}

/* ICON 엣지 1-shot: 각 전이에서 icon on/off가 정확히 1 tick만 1. spec §7-5. */
static void test_icon_edges_single_shot(void)
{
    seek_reset_fsm_init();
    seek_reset_in_t in = { .cmd = SR_CMD_RESET, .run_active = 0u };
    seek_reset_out_t out;
    int reset_on = 0, reset_off = 0, seek_on = 0, seek_off = 0;

    seek_reset_fsm_step(&in, &out);           /* 진입 */
    if (out.reset_icon)     { reset_on++; }
    in.cmd = SR_CMD_NONE;
    for (int i = 1; i < 130; i++) {           /* 전체 체인(RESET 60 + SEEK 60)을 덮음 */
        seek_reset_fsm_step(&in, &out);
        if (out.reset_icon)     { reset_on++; }
        if (out.reset_icon_off) { reset_off++; }
        if (out.seek_icon)      { seek_on++; }
        if (out.seek_icon_off)  { seek_off++; }
    }
    CHECK_EQ(reset_on, 1);
    CHECK_EQ(reset_off, 1);
    CHECK_EQ(seek_on, 1);
    CHECK_EQ(seek_off, 1);
}

/* 타이밍 경계: 정확히 SR_TICKS(60) tick에 전이 (59 tick까진 유지). spec §7-6. */
static void test_timing_boundary(void)
{
    seek_reset_fsm_init();
    seek_reset_in_t in = { .cmd = SR_CMD_RESET, .run_active = 0u };
    seek_reset_out_t out;
    seek_reset_fsm_step(&in, &out);           /* 진입 (step 0) */
    in.cmd = SR_CMD_NONE;
    /* step 1..59: 아직 SR_RESET 유지 */
    for (int i = 1; i <= 59; i++) {
        seek_reset_fsm_step(&in, &out);
        CHECK_EQ(out.state, SR_RESET);
    }
    /* step 60: 체인 전이 → SR_SEEK */
    seek_reset_fsm_step(&in, &out);
    CHECK_EQ(out.state, SR_SEEK);
}

int main(void)
{
    test_init_idle();
    test_reset_auto_chain();
    test_seek_oneshot_no_chain();
    test_run_orthogonal();
    test_busy_command_ignored();
    test_icon_edges_single_shot();
    test_timing_boundary();
    if (failures) { printf("test_app_seek_reset_fsm: %d FAILED\n", failures); return 1; }
    printf("test_app_seek_reset_fsm: all passed\n");
    return 0;
}
