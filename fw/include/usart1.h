/* fw/include/usart1.h
 *
 * USART1 raw I/O — DGUS LCD 통신용.
 * Phase 2 mon_usart6 패턴의 raw 레이어 미러.
 *
 * 호출자: fw/drivers/dgus_lcd.c 만. 다른 모듈에서 직접 호출 ✗.
 */
#ifndef GDS_USART1_H_
#define GDS_USART1_H_

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"

/* USART1: PA9 (TX, AF7) + PA10 (RX, AF7), 115200 8N1, no flow control.
 * NVIC priority = 5 (TIM11 sys_tick 와 동일 — 둘 다 짧은 ISR, queueing).
 * RX: HAL_UART_Receive_IT 1-byte 무장, RxCpltCallback 에서 ring push + 재무장.
 */
void usart1_init(void);

/* 폴링 TX. 10 ms timeout — 16 B 프레임 @ 115200 ≈ 1.4 ms 의 7배 여유.
 * HAL_OK / HAL_TIMEOUT / HAL_ERROR / HAL_BUSY 반환.
 */
HAL_StatusTypeDef usart1_send_blocking(const uint8_t *buf, uint16_t len);

/* RX 링 버퍼에서 1 byte pop. 비어 있으면 false 반환, *out_byte 미변경.
 * 호출자: dgus_lcd.c 의 파서 only.
 */
bool usart1_rx_pop(uint8_t *out_byte);

/* RX 링 full 또는 HAL_UART_Receive_IT 재무장 실패로 폐기된 byte 누적.
 * Stage A 데모는 검증용 (chunk 6f 에서 GDB read).
 */
uint16_t usart1_rx_drop_count(void);

/* USART ORE/FE/NE/PE 발생 누적 — HAL_UART_ErrorCallback 진입 횟수.
 * ORE 등 발생 시 콜백에서 플래그 클리어 + RX 재무장 처리. 정상 통신에서 0 기대.
 */
uint16_t usart1_rx_error_count(void);

#endif /* GDS_USART1_H_ */
