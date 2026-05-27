/* fw/src/app_lcd_str.c — LCD string formatters, verbatim port of samd20
 * time2str/energy2str/conv_addr2str/ip_to_string/lcd_data_pdd
 * (ref/samd20/main.c:2708-2939, 3236-3246). These strings land on the DGUS
 * panel byte-for-byte, so the digit/decimal placement is preserved exactly. */
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include "app_lcd.h"

uint8_t time2str(uint16_t src, uint8_t *dest)
{
    uint8_t  nibble, first_zero, pos;
    uint16_t temp_i;

    first_zero = pos = 0;

    nibble = (uint8_t)(src / 10000);
    temp_i = src % 10000;
    if (nibble != 0) {
        dest[pos++] = (uint8_t)(nibble + '0');
        first_zero = 1;
    }

    nibble = (uint8_t)(temp_i / 1000);
    temp_i = temp_i % 1000;
    if (nibble != 0) {
        dest[pos++] = (uint8_t)(nibble + '0');
        first_zero = 1;
    } else if (first_zero == 1) {
        dest[pos++] = '0';
    }

    nibble = (uint8_t)(temp_i / 100);
    temp_i = temp_i % 100;
    dest[pos++] = (uint8_t)(nibble + '0');

    dest[pos++] = '.';

    nibble = (uint8_t)(temp_i / 10);
    temp_i = temp_i % 10;
    dest[pos++] = (uint8_t)(nibble + '0');

    dest[pos++] = (uint8_t)(temp_i + '0');
    dest[pos++] = '\0';

    return pos;
}

uint8_t energy2str(uint32_t src, uint8_t *dest)
{
    uint8_t  nibble, first_zero, pos;
    uint16_t temp_i;

    first_zero = pos = 0;

    nibble = (uint8_t)(src / 100000);
    src    = src % 100000;
    if (nibble != 0) {
        dest[pos++] = (uint8_t)(nibble + '0');
        first_zero = 1;
    }

    nibble = (uint8_t)(src / 10000);
    temp_i = (uint16_t)(src % 10000);
    if (nibble != 0) {
        dest[pos++] = (uint8_t)(nibble + '0');
        first_zero = 1;
    }

    nibble = (uint8_t)(temp_i / 1000);
    temp_i = temp_i % 1000;
    if (nibble != 0) {
        dest[pos++] = (uint8_t)(nibble + '0');
        first_zero = 1;
    } else if (first_zero == 1) {
        dest[pos++] = '0';
    }

    nibble = (uint8_t)(temp_i / 100);
    temp_i = temp_i % 100;
    if (nibble != 0) {
        dest[pos++] = (uint8_t)(nibble + '0');
        first_zero = 1;
    } else if (first_zero == 1) {
        dest[pos++] = '0';
    }

    nibble = (uint8_t)(temp_i / 10);
    temp_i = temp_i % 10;
    if (nibble != 0) {
        dest[pos++] = (uint8_t)(nibble + '0');
        first_zero = 1;
    } else if (first_zero == 1) {
        dest[pos++] = '0';
    }

    dest[pos++] = (uint8_t)(temp_i + '0');
    dest[pos++] = '\0';

    return pos;
}

void conv_addr2str(uint8_t addr, uint8_t *buff)
{
    uint8_t temp;

    if (addr == 0) {
        buff[0] = 'N';
        buff[1] = 'O';
        buff[2] = 'N';
        buff[3] = 'E';
    } else {
        buff[0] = ' ';
        buff[1] = (uint8_t)('0' + addr / 100);
        temp    = addr % 100;
        buff[2] = (uint8_t)('0' + temp / 10);
        temp    = temp % 10;
        buff[3] = (uint8_t)('0' + temp);
    }
}

uint8_t ip_to_string(const uint8_t ip[4], char *str_buffer)
{
    uint8_t i;
    int     len;

    if (str_buffer == NULL) {
        return 0;
    }

    len = snprintf(str_buffer, 16, "%d.%d.%d.%d",
                   ip[0], ip[1], ip[2], ip[3]);
    if (len < 0) {
        len = 0;
    }
    for (i = (uint8_t)len; i < 16; i++) {
        str_buffer[i] = 0;
    }
    return 16;
}

void lcd_data_pdd(uint8_t *dest, const uint8_t *src, uint8_t len)
{
    uint8_t i, str_finish = 0;
    for (i = 0; i < len; i++) {
        if (src[i] == '\0') {
            str_finish = 1;
        }
        dest[i] = str_finish ? '\0' : src[i];
    }
}
