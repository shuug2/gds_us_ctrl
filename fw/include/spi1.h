/* fw/include/spi1.h — SPI1 master + W5500 CS/RST GPIO (spec §4.2).
 * PA5/6/7 = SCK/MISO/MOSI (AF5), PA4 = software-GPIO CS (NOT HW NSS — W5500
 * needs CS held LOW across a multi-byte frame), PC5 = NRST out, PC4 = INT in
 * (configured, polled-mode so unused this slice). These functions back the
 * ioLibrary callbacks registered in app_eth.c. */
#pragma once
#include <stdint.h>

void    spi1_init(void);                 /* clocks, GPIO AF, SPI1 CR (Mode 0, MSB) */
uint8_t spi1_xfer_byte(uint8_t tx);      /* blocking 1-byte transceive */
void    spi1_burst_read(uint8_t *buf, uint16_t len);
void    spi1_burst_write(uint8_t *buf, uint16_t len);
void    spi1_cs_low(void);               /* assert CS (PA4 low) */
void    spi1_cs_high(void);              /* deassert CS (PA4 high) */
void    spi1_eth_reset(void);            /* PC5 NRST pulse (W5500 reset timing) */
