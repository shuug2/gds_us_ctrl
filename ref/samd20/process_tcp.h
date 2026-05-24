#ifndef _PROCESS_TCP_H_
#define _PROCESS_TCP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Loopback test debug message printout enable */
#define	_TCPSRV_DEBUG_

/* DATA_BUF_SIZE define for Loopback example */
#ifndef DATA_BUF_SIZE
	#define DATA_BUF_SIZE			4096
#endif

/************************/
/* Select LOOPBACK_MODE */
/************************/
#define LOOPBACK_MAIN_NOBLOCK    0
#define LOOPBACK_MODE   LOOPBACK_MAIN_NOBLOCK

#define SERVER_CLOSED                  0x00
#define SERVER_INIT                    0x13
#define SERVER_LISTEN                  0x14
#define SERVER_CONNECT                 0x15
#define SERVER_SYNRECV                 0x16
#define SERVER_ESTABLISHED             0x17
#define SERVER_FIN_WAIT                0x18
#define SERVER_CLOSING                 0x1A
#define SERVER_CLOSE_WAIT              0x1C



/* TCP server Loopback test example */
int32_t control_tcps(uint8_t sn, uint8_t * status, uint16_t port);
int32_t recv_tcps(uint8_t sn, uint8_t* buf);
int32_t send_tcps(uint8_t sn, uint8_t* buf, uint16_t size);

#ifdef __cplusplus
}
#endif

#endif
