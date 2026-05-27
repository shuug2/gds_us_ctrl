/* fw/drivers/mon_usart6.c */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "stm32f4xx_hal.h"
#include "periph.h"
#include "mon.h"
#define MON_TX_TIMEOUT_MS  50u   /* 진단 채널이 앱을 영구 블록하지 못하게 */

void mon_init(void) { /* huart6는 usart6_init이 이미 초기화 */ }

void mon_write(const uint8_t *buf, size_t len) {
    HAL_UART_Transmit(&huart6, (uint8_t *)buf, len, MON_TX_TIMEOUT_MS);
}

void mon_writeln(const char *s) {
    mon_write((const uint8_t *)s, strlen(s));
    mon_write((const uint8_t *)"\r\n", 2);
}

int mon_printf(const char *fmt, ...) {
    char buf[128];
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n > 0) {
        size_t emit = (n < (int)sizeof buf) ? (size_t)n : sizeof buf - 1;
        mon_write((const uint8_t *)buf, emit);
    }
    return n;
}
