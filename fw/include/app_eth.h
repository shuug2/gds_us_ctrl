/* fw/include/app_eth.h — W5500 bring-up over SPI1 (spec §4.3 / slice 2b DHCP).
 * Static IP from cfg(FRAM) or DHCP-acquired (comm_mode==ETH_DHCP); MAC =
 * hardcoded constant. Non-fatal: if the chip is absent or the link never comes
 * up, app_eth_init() returns false and the TCP path stays off — RTU / mon / LCD
 * are unaffected. */
#pragma once
#include <stdbool.h>

/* Bring up SPI1 + W5500 (fast chip init only — NON-BLOCKING: does not wait for
 * the PHY link, which the W5500 establishes ~1.5 s after reset). Returns true if
 * the chip answered; false if absent. The PHY link is then awaited and the net
 * config (static netinfo, or DHCP start) is applied by app_eth_tick() on the
 * link-up transition. Call once at boot, after app_init() (needs cfg loaded). */
bool app_eth_init(void);

/* Drive the non-blocking bring-up + DHCP client from the superloop: polls the
 * PHY link until up (then applies static netinfo or starts DHCP), and for DHCP
 * runs DHCP_time_handler (1 s) + DHCP_run. Call every superloop iteration (from
 * app_loop_iter). No-op once a static IP is up or the chip is absent. */
void app_eth_tick(void);

/* True when ready to serve TCP: chip + PHY link up AND IP ready (static: when
 * the link comes up; DHCP: after the lease is acquired). False until then. */
bool app_eth_available(void);
