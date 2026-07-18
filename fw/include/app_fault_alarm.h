/* fw/include/app_fault_alarm.h — 일반 fault(ERR_OVTIME 등) 부저 알람 글루. */
#pragma once

void app_fault_alarm_init(void);
void app_fault_alarm_tick(void);   /* 슈퍼루프 10ms gate */
