/* fw/test/mock_fram.h — host mock of the fram.h read/write API.
 * 256B backing store + per-address read-fail injection + write counter.
 * Links in place of fw/drivers/fram.c (which needs HAL/i2c1). */
#pragma once
#include <stdint.h>
#include "fram.h"

void     mock_fram_reset(void);                     /* store=0, fail=none, write_cnt=0 */
void     mock_fram_fail_read(uint8_t addr, uint8_t nbytes); /* [addr, addr+n) read 실패 주입 */
void     mock_fram_poke(uint8_t addr, uint8_t v);   /* backing store 직접 기록 */
uint8_t  mock_fram_peek(uint8_t addr);              /* backing store 직접 읽기 */
uint32_t mock_fram_write_count(void);               /* fram_write_* 호출 누계 */
void     mock_fram_clear_write_count(void);
