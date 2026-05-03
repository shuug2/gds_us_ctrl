/* fw/include/mon.h */
#pragma once
#include <stddef.h>
#include <stdint.h>

void mon_init(void);
void mon_write(const uint8_t *buf, size_t len);
void mon_writeln(const char *s);
int  mon_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
