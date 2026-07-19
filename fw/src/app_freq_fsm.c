/* fw/src/app_freq_fsm.c — 순수 FREQ_IN 측정 FSM. SAMD20 calc_freq 충실.
 * ISR(on_capture)이 연속 rising-edge 캡처 간 Δ(=주기)를 10개 누산해 batch를
 * 래치하고 seq++; main(compute)은 직전 호출 이후 새 batch가 생겼으면 평균
 * 주파수를, 아니면 0(무신호)을 돌려준다. seq 비교라 clear-flag 경합 없음. */
#include "app_freq_fsm.h"

/* ISR가 쓰는 상태 (단일 생산자). 32-bit 읽기는 Cortex-M4에서 atomic. */
static volatile uint32_t s_prev_cap;     /* 직전 캡처값 */
static volatile uint8_t  s_have_prev;    /* 첫 캡처 시드 여부 */
static volatile uint32_t s_acc;          /* 진행 중 batch의 Δ 합 */
static volatile uint8_t  s_cnt;          /* 진행 중 batch의 Δ 개수 */
static volatile uint32_t s_latched_sum;  /* 마지막 완성 batch의 Σ */
static volatile uint32_t s_batch_seq;    /* 완성 batch마다 ++ */
/* main이 쓰는 상태 (단일 소비자) */
static uint32_t s_last_seen_seq;

/* FREQ FSM 초기화 */
void freq_fsm_init(void)
{
    s_prev_cap     = 0u;
    s_have_prev    = 0u;
    s_acc          = 0u;
    s_cnt          = 0u;
    s_latched_sum  = 0u;
    s_batch_seq    = 0u;
    s_last_seen_seq = 0u;
}

/* 캡처 Δ 누산·batch 래치 */
void freq_fsm_on_capture(uint32_t capture)
{
    if (!s_have_prev) {              /* 첫 캡처는 기준점만 (Δ 없음) */
        s_prev_cap  = capture;
        s_have_prev = 1u;
        return;
    }
    uint32_t delta = capture - s_prev_cap;   /* unsigned: 32-bit wrap 흡수 */
    s_prev_cap = capture;
    s_acc += delta;
    if (++s_cnt >= (uint8_t)FREQ_AVG_SAMPLES) {
        s_latched_sum = s_acc;       /* batch 완성 → 래치 */
        s_acc = 0u;
        s_cnt = 0u;
        s_have_prev = 0u;            /* 다음 캡처는 새 기준점(시드)으로 처리 */
        s_batch_seq++;               /* 소비자에게 fresh 신호 */
    }
}

/* batch 평균 주파수 계산 */
uint16_t freq_fsm_compute(int16_t cal_val)
{
    if (s_batch_seq == s_last_seen_seq) {
        return 0u;                   /* 직전 호출 이후 새 batch 없음 = 무신호 */
    }
    /* benign race (single ISR writer / this lone reader): if on_capture completes
     * a batch between these reads, we may observe seq=N but sum of N+1 (or skip one
     * batch). Worst case = one batch's value reported a tick early/late — always a
     * valid recent average, never torn (32-bit aligned reads atomic on Cortex-M4).
     * Harmless for a frequency display; no critical section needed. (Task 1 review MINOR-2) */
    s_last_seen_seq = s_batch_seq;
    uint32_t sum = s_latched_sum;
    if (sum == 0u) {
        return 0u;                   /* div-by-zero 가드 (SAMD20에 없던 방어) */
    }
    uint32_t base = FREQ_CONST / sum;            /* 평균 주파수 (Hz) */
    return (uint16_t)((uint16_t)base + (uint16_t)cal_val);  /* SAMD20 uint16 += int16 */
}
