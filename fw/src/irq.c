/* fw/src/irq.c */
#include "stm32f4xx_hal.h"
#include "periph.h"
#include "sys_tick.h"

void NMI_Handler(void)        { while (1) {} }
void HardFault_Handler(void)  { while (1) {} }   /* TODO Stage A: register dump via mon_printf */
void MemManage_Handler(void)  { while (1) {} }
void BusFault_Handler(void)   { while (1) {} }
void UsageFault_Handler(void) { while (1) {} }
void SVC_Handler(void)        { /* unused */ }
void DebugMon_Handler(void)   { /* unused */ }
void PendSV_Handler(void)     { /* unused */ }

void SysTick_Handler(void) { HAL_IncTick(); }

void Error_Handler(void) { __disable_irq(); while (1) {} }

void TIM1_TRG_COM_TIM11_IRQHandler(void) {
    HAL_TIM_IRQHandler(&htim11);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM11) {
        sys_tick_handle_irq();
    }
}
