/* fw/include/app_eth.h — W5500 bring-up over SPI1 (spec §4.3). Static IP from
 * cfg(FRAM); MAC = hardcoded constant. Non-fatal: if the chip is absent or the
 * link never comes up, app_eth_init() returns false and the TCP path stays off
 * — RTU / mon / LCD are unaffected. */
#pragma once
#include <stdbool.h>

/* Bring up SPI1 + W5500, apply static netinfo from cfg. Returns true if the
 * chip answered and the PHY linked within the timeout. Call once at boot,
 * after app_init() (needs cfg loaded). */
bool app_eth_init(void);

/* True if app_eth_init() succeeded (W5500 present + linked). */
bool app_eth_available(void);
