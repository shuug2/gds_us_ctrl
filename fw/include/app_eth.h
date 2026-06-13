/* fw/include/app_eth.h — W5500 bring-up over SPI1 (spec §4.3 / slice 2b DHCP).
 * Static IP from cfg(FRAM) or DHCP-acquired (comm_mode==ETH_DHCP); MAC =
 * hardcoded constant. Non-fatal: if the chip is absent or the link never comes
 * up, app_eth_init() returns false and the TCP path stays off — RTU / mon / LCD
 * are unaffected. */
#pragma once
#include <stdbool.h>

/* Bring up SPI1 + W5500. For static comm_mode, applies netinfo from cfg and
 * becomes available immediately. For DHCP comm_mode, starts the DHCP client and
 * stays unavailable until app_eth_tick() acquires a lease. Returns true if the
 * chip answered and the PHY linked within the timeout. Call once at boot, after
 * app_init() (needs cfg loaded). */
bool app_eth_init(void);

/* Drive the DHCP client (1s DHCP_time_handler + DHCP_run). No-op unless DHCP
 * mode started a lease. Call every superloop iteration (from app_loop_iter). */
void app_eth_tick(void);

/* True when ready to serve TCP: chip+link up AND IP ready (static: at init;
 * DHCP: after the lease is acquired). */
bool app_eth_available(void);
