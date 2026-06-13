/* fw/src/app_modbus_tcp.c — see app_modbus_tcp.h.
 *
 * ASSUMPTION (spec §3, advisor #4): the request/response cadence of a Modbus
 * master means each recv() yields exactly ONE complete MBAP frame. We do not
 * accumulate partial frames or split coalesced ones — a short read is rejected
 * by mb_tcp_build_response() (length-field check) and the master retries. This
 * is sufficient for mbpoll-class masters; streaming reassembly is future work. */
#include "app_modbus_tcp.h"
#include "app_modbus.h"            /* app_modbus_core(), app_modbus_apply_writes() */
#include "app_modbus_tcp_frame.h"
#include "socket.h"
#include "wizchip_conf.h"

#define MB_TCP_SOCK   0u
#define MB_TCP_PORT   502u

static uint8_t s_rxbuf[MB_FRAME_MAX + MB_TCP_MBAP_LEN];

/* Port of samd20 process_tcp.c control_tcps: walk the socket FSM one step. */
static void control_tcp(void)
{
    switch (getSn_SR(MB_TCP_SOCK)) {
        case SOCK_ESTABLISHED:
            if (getSn_IR(MB_TCP_SOCK) & Sn_IR_CON) {
                setSn_IR(MB_TCP_SOCK, Sn_IR_CON);
            }
            break;
        case SOCK_CLOSE_WAIT:
            (void)disconnect(MB_TCP_SOCK);
            break;
        case SOCK_INIT:
            (void)listen(MB_TCP_SOCK);
            break;
        case SOCK_CLOSED:
            (void)socket(MB_TCP_SOCK, Sn_MR_TCP, MB_TCP_PORT, 0x00u);
            break;
        default:
            break;
    }
}

void app_modbus_tcp_poll(void)
{
    control_tcp();

    if (getSn_SR(MB_TCP_SOCK) != SOCK_ESTABLISHED) {
        return;
    }
    uint16_t avail = getSn_RX_RSR(MB_TCP_SOCK);
    if (avail == 0u) {
        return;
    }
    if (avail > (uint16_t)sizeof s_rxbuf) {
        avail = (uint16_t)sizeof s_rxbuf;
    }
    int32_t got = recv(MB_TCP_SOCK, s_rxbuf, avail);
    if (got <= 0) {
        return;
    }

    uint8_t  out[MB_TCP_RESP_MAX];
    uint16_t out_len = 0u;
    uint8_t  fc = 0u;
    if (mb_tcp_build_response(app_modbus_core(), s_rxbuf, (uint16_t)got,
                              out, &out_len, &fc)) {
        (void)send(MB_TCP_SOCK, out, out_len);   /* respond first (matches RTU) */
        if (fc == 0x06u) {
            /* then the shared write-apply (clamp + FRAM + cmd), same order as
             * the RTU path in app_modbus_tick: send response, then apply. The
             * FC06 echo was already captured at decode time, so wire bytes are
             * unaffected by apply ordering. */
            app_modbus_apply_writes();
        }
    }
}
