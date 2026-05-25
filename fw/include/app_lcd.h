/* fw/include/app_lcd.h — LCD application data setup (samd20 init_lcd_mode port, scope a). */
#pragma once
#include "app_config.h"

/* GDSONIC model string -> MODEL_NAME (11-byte payload incl. trailing NUL). */
void app_lcd_send_model_str(uint8_t freq, uint8_t type);

/* Pre-fill RUN-page VPs from cfg, then switch to the run page. */
void app_lcd_init_mode(const app_config_t *cfg);
