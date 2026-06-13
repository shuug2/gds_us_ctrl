# Vendored WIZnet ioLibrary_Driver (W5500 subset)

Upstream: https://github.com/Wiznet/ioLibrary_Driver
Pinned commit: 220ca7a6d92677830f1dab1383c47e1eb620cad9
Fetched: 2026-06-13

Vendored subset (Modbus-TCP server over W5500 — Stage C slice 2a/2b):
- Ethernet/wizchip_conf.{c,h}
- Ethernet/socket.{c,h}
- Ethernet/W5500/w5500.{c,h}
- Internet/DHCP/dhcp.{c,h}   (slice 2b — DHCP client)

Local edit (config only): `wizchip_conf.h` line 79 sets `#define _WIZCHIP_ W5500`
(was the upstream default `W6300`). This is the ONLY hand-edit. With W5500
selected, `wiz_NetInfo` resolves to the classic
`{ uint8_t mac[6]; ip[4]; sn[4]; gw[4]; dns[4]; dhcp_mode dhcp; }` and
`reg_wizchip_spi_cbfunc` is the 2-arg byte form (the 4-arg form is W6100-only).

NOT vendored: loopback, Application/, other PHY chips.
Treated as read-only vendor code (like fw/vendor/Drivers ST HAL) — warnings
isolated in CMake (separate `wiznet` STATIC library, SYSTEM includes, built with
`-w`). Do not hand-edit beyond the chip-select.
