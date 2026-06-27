/* fw/include/app_input.h — 물리 명령 입력 + E-stop 글루 (10ms tick).
 * io_read_*(슬라이스 A) → app_input_fsm → app_reg_command(US_REMOTE) 디스패치.
 * E-stop(std EMSW)은 레벨추종 force-stop + io_sol_dn(off). app_reg START guard와
 * Modbus STATUS가 app_estop_active()를 조회. spec 2026-06-27-physical-io-slice-d. */
#pragma once
#include <stdint.h>

void    app_input_init(void);     /* boot: FSM reset (io_init + sys_tick 뒤) */
void    app_input_tick(void);     /* 슈퍼루프 10ms gate */
uint8_t app_estop_active(void);   /* 1 = E-stop 활성 (START 차단 / STATUS 비트) */
