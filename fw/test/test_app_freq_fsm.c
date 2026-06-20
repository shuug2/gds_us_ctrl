/* fw/test/test_app_freq_fsm.c — host unit tests, 순수 freq 측정 FSM.
 * SAMD20 calc_freq 충실 포팅 (main.c:4141-4160) + 96MHz 도메인. */
#include <stdio.h>
#include <stdint.h>
#include "app_freq_fsm.h"

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

/* 96MHz / 20kHz = 4800 틱/주기. 10주기 Σ = 48000. CONST/48000 = 20000 Hz. */
#define PERIOD_20K  4800u
/* 96MHz / 15kHz = 6400 틱/주기. 10주기 Σ = 64000. CONST/64000 = 15000 Hz. */
#define PERIOD_15K  6400u

/* 첫 캡처는 prev 시드만(Δ 없음), 이후 N개 Δ를 만들려면 N+1 캡처 필요. */
static void feed_periods(uint32_t start, uint32_t period, unsigned n)
{
    uint32_t cap = start;
    freq_fsm_on_capture(cap);          /* prev 시드 (skip) */
    for (unsigned i = 0; i < n; i++) {
        cap += period;
        freq_fsm_on_capture(cap);      /* Δ = period */
    }
}

static void test_no_signal_returns_zero(void)
{
    freq_fsm_init();
    CHECK_EQ(freq_fsm_compute(0), 0);   /* 캡처 0건 → fresh batch 없음 → 0 */
}

static void test_incomplete_batch_zero(void)
{
    freq_fsm_init();
    feed_periods(1000u, PERIOD_20K, 9); /* 9 Δ만 (10 미만) → batch 미완성 */
    CHECK_EQ(freq_fsm_compute(0), 0);
}

static void test_20k_exact(void)
{
    freq_fsm_init();
    feed_periods(1000u, PERIOD_20K, 10); /* 10 Δ → batch 완성, Σ=48000 */
    CHECK_EQ(freq_fsm_compute(0), 20000);
}

static void test_15k_exact(void)
{
    freq_fsm_init();
    feed_periods(0u, PERIOD_15K, 10);    /* Σ=64000 → 15000 */
    CHECK_EQ(freq_fsm_compute(0), 15000);
}

static void test_cal_offset_signed(void)
{
    freq_fsm_init();
    feed_periods(1000u, PERIOD_20K, 10);
    CHECK_EQ(freq_fsm_compute(5), 20005);    /* +5 */
    freq_fsm_init();
    feed_periods(1000u, PERIOD_20K, 10);
    CHECK_EQ(freq_fsm_compute(-5), 19995);   /* -5 (uint16 wrap = 충실) */
}

static void test_second_compute_no_fresh_batch(void)
{
    freq_fsm_init();
    feed_periods(1000u, PERIOD_20K, 10);
    CHECK_EQ(freq_fsm_compute(0), 20000);    /* 1회: fresh */
    CHECK_EQ(freq_fsm_compute(0), 0);        /* 2회: 새 batch 없음 → 0 (무신호 거동) */
}

static void test_multi_batch_uses_latest(void)
{
    freq_fsm_init();
    feed_periods(1000u, PERIOD_20K, 10);     /* batch1: 20kHz */
    feed_periods(0u,    PERIOD_15K, 10);     /* batch2: 15kHz (compute 호출 없이 누적) */
    CHECK_EQ(freq_fsm_compute(0), 15000);    /* 최신 batch(15kHz) 사용 */
}

static void test_32bit_wraparound(void)
{
    freq_fsm_init();
    /* prev = 0xFFFFFFFF, 다음 cap = 4799 → unsigned Δ = 4799 - 0xFFFFFFFF = 4800 */
    freq_fsm_on_capture(0xFFFFFFFFu);        /* prev 시드 */
    uint32_t cap = 0xFFFFFFFFu;
    for (unsigned i = 0; i < 10; i++) {
        cap += PERIOD_20K;                   /* 32-bit wrap; 첫 스텝이 0xFFFFFFFF→4799 */
        freq_fsm_on_capture(cap);
    }
    CHECK_EQ(freq_fsm_compute(0), 20000);    /* wrap에도 Σ=48000 정확 */
}

static void test_div_guard_zero_sum(void)
{
    freq_fsm_init();
    /* 10 캡처가 모두 동일값 → Δ=0 × 10 → Σ=0. batch는 완성되나 0 나눗셈 가드. */
    for (unsigned i = 0; i < 11; i++) {
        freq_fsm_on_capture(7777u);          /* 첫 시드 + 10 Δ(=0) */
    }
    CHECK_EQ(freq_fsm_compute(0), 0);        /* div-by-zero 가드 → 0 */
}

int main(void)
{
    test_no_signal_returns_zero();
    test_incomplete_batch_zero();
    test_20k_exact();
    test_15k_exact();
    test_cal_offset_signed();
    test_second_compute_no_fresh_batch();
    test_multi_batch_uses_latest();
    test_32bit_wraparound();
    test_div_guard_zero_sum();
    if (failures) { printf("app_freq_fsm: %d FAIL\n", failures); return 1; }
    printf("app_freq_fsm: all tests passed\n");
    return 0;
}
