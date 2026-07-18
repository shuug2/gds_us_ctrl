/* fw/include/app_horn.h — SYS_HORN(horn-down) 글루. LCD STD SETUP이 모드 소유. */
#pragma once
#include <stdbool.h>
#include <stdint.h>

void    app_horn_init(void);
void    app_horn_set_mode(bool on);     /* app_lcd_hook_horn(DATA_SAVE)에서 호출 */
uint8_t app_horn_mode_active(void);     /* 게이트 소비: app_reg START / app_weld */
void    app_horn_tick(void);            /* 슈퍼루프 10ms gate */
