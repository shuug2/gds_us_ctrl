/* fw/include/app_lcd_input_priv.h — app_lcd_input.c ↔ app_lcd_comm.c 내부 공유 심(seam).
 * 외부 모듈 사용 금지: dispatch(입력)와 comm/ether/DATA_SAVE 핸들러(comm) 사이 전용. */
#pragma once
#include <stdint.h>

/* app_lcd_input.c 소유 */
uint8_t run_page_for_mode(uint8_t sys_mode);

/* app_lcd_comm.c 소유 — comm/ether shadow 편집 + DATA_SAVE commit/rollback */
void handle_comm_addr(uint16_t data16);
void handle_comm_speed(uint16_t data16);
void handle_comm_parity(uint16_t data16);
void handle_comm_mode(uint16_t data16);
void handle_ether_key(uint16_t data16);
void data_save_commit(void);
void data_save_cancel(void);
