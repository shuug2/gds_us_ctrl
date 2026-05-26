/* fw/include/app_lcd.h — LCD application data setup (samd20 init_lcd_mode port, scope a). */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "app_config.h"

/* GDSONIC model string -> MODEL_NAME (11-byte payload incl. trailing NUL). */
void app_lcd_send_model_str(uint8_t freq, uint8_t type);

/* Resolve the RUN page id for the configured model_type. */
uint8_t app_lcd_run_page(const app_config_t *cfg);

/* Pre-fill RUN-page VPs from cfg, then switch to the run page. */
void app_lcd_init_mode(const app_config_t *cfg);

/* Read-back confirm the run page took (SYS_PIC_NOW); re-assert up to
 * DGUS_PAGE_CONFIRM_RETRIES times if the panel reverted to its boot page
 * after finishing its own splash. Returns true once confirmed. */
bool app_lcd_ensure_run_page(const app_config_t *cfg);
