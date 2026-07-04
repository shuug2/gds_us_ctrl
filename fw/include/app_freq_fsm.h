/* fw/include/app_freq_fsm.h — 순수 FREQ_IN 측정 FSM (HAL-free, host-test).
 * SAMD20 calc_freq 충실 포팅 (main.c:4141-4160). 캡처 ISR이 on_capture를,
 * main(reg_publish)이 compute를 호출 — 단일 생산자/단일 소비자. */
#pragma once
#include <stdint.h>

/* TIM5 = APB1 타이머 클럭 = 96 MHz (clock.c:28 APB1=HCLK/2 → ×2). */
#define FREQ_TIM_CLK_HZ   96000000UL
#define FREQ_AVG_SAMPLES  10u           /* SAMD20 freq_buf[10] 링 깊이 */
/* curr_freq = (f_tim × N) / Σ(N 주기).  ⚠ SAMD20의 81000000(=GCLK3≈8.1MHz×10)을
 * 복사하지 말 것 — 우리는 자기 96MHz 클럭으로 측정한다. */
#define FREQ_CONST        (FREQ_TIM_CLK_HZ * FREQ_AVG_SAMPLES)   /* 960000000 */

void     freq_fsm_init(void);                  /* 상태 리셋 (캡처 시작 전) */
void     freq_fsm_on_capture(uint32_t capture);/* ISR: 캡처 카운트 1건 투입 */
uint16_t freq_fsm_compute(int16_t cal_val);    /* main: fresh batch면 Hz, 아니면 0 */
