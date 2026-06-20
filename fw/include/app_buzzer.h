/* fw/include/app_buzzer.h — 부저 글루 (10ms tick → io_buzzer). */
#pragma once
#include <stdint.h>

void app_buzzer_init(void);
void app_buzzer_beep_ms(uint16_t ms);  /* ms 길이 비프 요청 (비블로킹) */
void app_buzzer_tick(void);            /* 슈퍼루프 10ms gate */
