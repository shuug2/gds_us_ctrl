/* fw/include/sys_tick.h */
#pragma once
#include <stdint.h>

void sys_tick_init(void);
uint32_t sys_tick_get_ms(void);
void sys_tick_delay_ms(uint32_t ms);
void sys_tick_handle_irq(void);
