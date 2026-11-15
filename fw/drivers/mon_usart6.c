/* fw/drivers/mon_usart6.c — mon 출력. V30 = USART6(RS-485 와 공유, Modbus 점유 중 게이트),
 * V31 = USART2(헤더 J3, 상시 출력 — usart2.c). 이름은 V30 시절 그대로. */
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "stm32f4xx_hal.h"
#include "periph.h"
#include "mon.h"
#include "define.h"
#define MON_TX_TIMEOUT_MS  50u   /* 진단 채널이 앱을 영구 블록하지 못하게 */

/* mon 초기화(no-op) */
#if defined(HW_V31)
extern void usart2_init(void);   /* drivers/usart2.c */
void mon_init(void) { usart2_init(); }
#else
void mon_init(void) { /* huart6는 usart6_init이 이미 초기화 */ }
#endif

static bool s_mon_enabled = true;

/* mon 출력 on/off */
void mon_set_enabled(bool on) { s_mon_enabled = on; }

/* mon 바이트 송신 */
void mon_write(const uint8_t *buf, size_t len) {
#if defined(HW_V31)
    (void)s_mon_enabled;  /* V31 mon 은 RS-485 와 분리 — 게이트 불요 */
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, len, MON_TX_TIMEOUT_MS);
#else
    if (!s_mon_enabled) {
        return;   /* Modbus owns USART6 — keep the RS-485 bus clean */
    }
    HAL_UART_Transmit(&huart6, (uint8_t *)buf, len, MON_TX_TIMEOUT_MS);
#endif
}

/* mon 줄 단위 송신 */
void mon_writeln(const char *s) {
    mon_write((const uint8_t *)s, strlen(s));
    mon_write((const uint8_t *)"\r\n", 2);
}

/* mon printf 송신 */
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
