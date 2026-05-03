/* fw/src/irq.c */
#include "stm32f4xx_hal.h"

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
