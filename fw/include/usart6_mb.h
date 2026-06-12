/* fw/include/usart6_mb.h — USART6 Modbus-RTU transport (PC6/PC7 AF8; RS-485
 * DE = HW-auto per V30 U13/U16, no FW control). Shares the USART6 peripheral
 * with mon_usart6: the app_modbus occupancy rule (comm_mode==SERIAL &&
 * addr!=0) guarantees exactly one owner at a time.
 * RX = DMA2 Stream1 Ch5 circular free-running (usart1.c precedent — no IRQ,
 * overrun-immune). Frame boundary = samd20 max_break_cnt soft gap (250 us
 * ticks), reproduced as a per-baud ms threshold polled from the superloop
 * (plan Deviations 1). TX = blocking (spec §2).
 * Callers: fw/src/app_modbus.c only. */
#pragma once
#include <stdint.h>

/* Re-init USART6 at the Modbus line config (cfg indices, samd20 verbatim:
 * speed 0..5 = 2400/4800/9600/19200/38400/115200, default 19200; parity
 * 0=EVEN 1=ODD 2=NONE) and start the circular RX DMA. */
void usart6_mb_open(uint8_t speed_idx, uint8_t parity_idx);

/* Stop RX DMA + deinit the UART. The caller (app_modbus) re-runs usart6_init()
 * to restore the mon line config (115200 8N1). */
void usart6_mb_close(void);

/* Poll for one complete gap-delimited RTU frame. Copies it into buf and
 * returns its length, or 0 when no complete frame is pending. Frames longer
 * than maxlen are drained and dropped (line garbage; master retries). */
uint8_t usart6_mb_rx_frame(uint8_t *buf, uint8_t maxlen);

/* Blocking TX. Response <= 105 B; worst stall ~482 ms @2400 (len-scaled
 * timeout) — bounded and spec-approved. Flushes the RX ring afterwards so an
 * auto-DE transceiver echo of our own response is never re-parsed. */
void usart6_mb_send(const uint8_t *buf, uint8_t len);
