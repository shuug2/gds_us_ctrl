/* fw/include/app_overload_fsm.h — 순수 과부하 디바운스 FSM (HAL-free, host-test).
 * M16 disasm 충실 (firmware_disassembled.asm @0x10A6): PB13 HIGH ×5 연속 →
 * assert, LOW → count 즉시 reset(noise reject). step()이 active 레벨 +
 * assert/deassert 1-shot edge를 비트마스크로 반환. */
#pragma once
#include <stdint.h>

#define OVLD_DEBOUNCE_N  5u     /* M16: PA7 HIGH ×5 연속 */
#define OVLD_EV_ACTIVE   0x01u  /* 현재 과부하 활성 (레벨) */
#define OVLD_EV_ASSERT   0x02u  /* inactive→active 전이 (1-shot) */
#define OVLD_EV_DEASSERT 0x04u  /* active→inactive 전이 (1-shot) */

void    overload_fsm_init(void);
uint8_t overload_fsm_step(uint8_t raw);  /* raw: 1=fault(PB13 HIGH). OVLD_EV_* 비트마스크 */
