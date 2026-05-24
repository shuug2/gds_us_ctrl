#ifndef  _ETHERNET_H_
#define  _ETHERNET_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "wizchip_conf.h"

#define PIN_SCK  18
#define PIN_MOSI 19
#define PIN_MISO 16
#define PIN_CS   17
#define PIN_RST  20

#define SPI_PORT spi0
#define READ_BIT 0x00
#define TCP_SOCKET 0
#define ETH_MAX_BUF 2048

extern unsigned char ethBuf0[ETH_MAX_BUF];
extern wiz_NetInfo gWIZNETINFO;

extern int ethernet_init(void);
extern void print_network_information(void);
extern void Net_Conf(void);

#ifdef __cplusplus
}
#endif
#endif
