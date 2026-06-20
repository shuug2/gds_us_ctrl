/* fw/drivers/freq_ic.h — FREQ_IN(PA0) 입력캡처 드라이버.
 * TIM5_CH1 32-bit rising-edge 캡처를 IT 모드로 구동. 캡처마다 irq.c의
 * HAL_TIM_IC_CaptureCallback이 app_freq_fsm으로 전달. spec 슬라이스 B. */
#pragma once

void freq_ic_init(void);   /* freq_fsm 리셋 + TIM5_CH1 IC start (IT) */
