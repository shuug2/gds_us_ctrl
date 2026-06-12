/* fw/include/mon.h */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void mon_init(void);
void mon_write(const uint8_t *buf, size_t len);
void mon_writeln(const char *s);
int  mon_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* Output gate (spec §5): while Modbus owns USART6 the mon output is suppressed
 * so diagnostics never pollute the RS-485 bus. Suppression only — init state
 * is untouched and all mon_* calls stay safe no-ops. Default = enabled. */
void mon_set_enabled(bool on);
