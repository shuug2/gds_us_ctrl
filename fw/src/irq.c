/* fw/src/irq.c */
#include "stm32f4xx_hal.h"
#include "periph.h"
#include "sys_tick.h"
#include "app_freq_fsm.h"

/* NMI 폴트 트랩 */
void NMI_Handler(void)        { while (1) {} }
/* HardFault 트랩 */
void HardFault_Handler(void)  { while (1) {} }   /* TODO Stage A: register dump via mon_printf */
/* MemManage 트랩 */
void MemManage_Handler(void)  { while (1) {} }
/* BusFault 트랩 */
void BusFault_Handler(void)   { while (1) {} }
/* UsageFault 트랩 */
void UsageFault_Handler(void) { while (1) {} }
/* SVC 미사용 핸들러 */
void SVC_Handler(void)        { /* unused */ }
/* DebugMon 미사용 핸들러 */
void DebugMon_Handler(void)   { /* unused */ }
/* PendSV 미사용 핸들러 */
void PendSV_Handler(void)     { /* unused */ }

/* HAL 1ms tick 증가 */
void SysTick_Handler(void) { HAL_IncTick(); }

/* 전역 에러 정지 */
void Error_Handler(void) { __disable_irq(); while (1) {} }

/* TIM11 IRQ 처리 */
void TIM1_TRG_COM_TIM11_IRQHandler(void) {
    HAL_TIM_IRQHandler(&htim11);
}

/* TIM5 IRQ 처리 */
void TIM5_IRQHandler(void) {
    HAL_TIM_IRQHandler(&htim5);
}

/* TIM 주기만료 콜백 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM11) {
        sys_tick_handle_irq();
    }
}

/* TIM 입력캡처 콜백 */
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM5) {
        freq_fsm_on_capture(HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1));
    }
}
