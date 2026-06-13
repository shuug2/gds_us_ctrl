/* fw/include/app_modbus_tcp.h — Modbus-TCP server transport over W5500
 * socket 0, port 502 (spec §3.2). Drives the socket state machine and, on a
 * received frame, runs the pure framing (app_modbus_tcp_frame) against the
 * shared core (app_modbus_core) and the shared write-apply (app_modbus). Call
 * each tick while in ETH mode with the W5500 available. */
#pragma once

void app_modbus_tcp_poll(void);
