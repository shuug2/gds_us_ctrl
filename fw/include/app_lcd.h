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

/*--------------------------------------------------------------
 * LCD string formatters (samd20 main.c port — values land on panel verbatim)
 *--------------------------------------------------------------*/

/* Tenths-of-seconds "X.XX" form. Returns bytes written INCLUDING trailing NUL
 * (samd20 callers pass the return as the send length). */
uint8_t time2str(uint16_t src, uint8_t *dest);

/* Compact decimal, leading zeros suppressed. Returns bytes written incl. NUL. */
uint8_t energy2str(uint32_t src, uint8_t *dest);

/* 4-byte field: addr 0 -> "NONE", else " NNN" (space + 3 digits). No NUL. */
void conv_addr2str(uint8_t addr, uint8_t *buff);

/* "a.b.c.d" zero-padded to 16 bytes. Returns 16. dest must be >= 16 bytes. */
uint8_t ip_to_string(const uint8_t ip[4], char *str_buffer);

/* Copy src into dest, NUL-filling from the first NUL onward (samd20 lcd_data_pdd,
 * de-globalized: explicit dest instead of the lcd_temp_buf global). */
void lcd_data_pdd(uint8_t *dest, const uint8_t *src, uint8_t len);
