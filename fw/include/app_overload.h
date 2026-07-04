/* fw/include/app_overload.h — 과부하 글루 (10ms tick).
 * PB13 디바운스(app_overload_fsm) → assert에 force-stop, active 동안 릴레이/
 * 부저/에러, deassert에 클리어 + 자동 RESET→SEEK 재튜닝. app_reg START guard와
 * Modbus STATUS가 app_overload_active()를 조회. */
#pragma once
#include <stdint.h>

void    app_overload_init(void);     /* boot: FSM reset + 릴레이 off */
void    app_overload_tick(void);     /* 슈퍼루프 10ms gate */
uint8_t app_overload_active(void);   /* 1 = 과부하 활성 (START 차단 / STATUS 비트) */
