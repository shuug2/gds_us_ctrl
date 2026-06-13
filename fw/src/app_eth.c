/* fw/src/app_eth.c — see app_eth.h. ioLibrary callback wiring mirrors samd20
 * ethernet.c, retargeted to STM32 SPI1 (spi1.c) + W5500 chip. Static IP from
 * cfg, or DHCP-acquired (comm_mode==ETH_DHCP) via the ioLibrary DHCP client.
 *
 * Bring-up is NON-BLOCKING: app_eth_init() does only the fast chip init at boot
 * (no PHY-link wait — the W5500 PHY auto-negotiates ~1.5 s after reset, far too
 * long to block the superloop). app_eth_tick() then polls the PHY link from the
 * superloop and applies the net config (static netinfo, or DHCP start) on the
 * link-up transition — so a cable plugged in after boot still works and a
 * SERIAL-mode boot is never delayed. Link-DROP recovery (hot-plug) is out of
 * scope (spec §1.1, deferred). */
#include "app_eth.h"
#include "spi1.h"
#include "app_lcd.h"        /* app_lcd_cfg() -> comm_mode, ether_ip/nm/gw */
#include "app_config.h"
#include "mon.h"
#include "sys_tick.h"       /* sys_tick_get_ms() — link-poll + DHCP cadence */
#include "wizchip_conf.h"
#include "socket.h"
#include "dhcp.h"

#define COMM_ETH_DHCP  2u     /* cfg->comm_mode: 0=SERIAL 1=ETH_STATIC 2=ETH_DHCP */
#define SOCK_DHCP      1u     /* UDP socket for DHCP (TCP server uses socket 0) */
#define LINK_POLL_MS   100u   /* PHY-link poll cadence while waiting for link */
#define DHCP_TICK_MS   1000u  /* DHCP_time_handler cadence (1 s) */

/* Non-blocking bring-up phases. */
typedef enum {
    ETH_DOWN = 0,    /* chip init failed (absent) — terminal, no retry */
    ETH_LINKWAIT,    /* chip up; polling for PHY link to apply net config */
    ETH_STATIC_UP,   /* static netinfo applied; serving (available) */
    ETH_DHCP_RUN     /* DHCP client started; driving DHCP_run each tick */
} eth_phase_t;

/* Faithful to samd20 ethernet.c default MAC (per-unit MAC = future, spec §8). */
static const uint8_t kEthMac[6] = { 0x00, 0x08, 0xdc, 0x78, 0x91, 0x71 };

static eth_phase_t s_phase     = ETH_DOWN;
static bool        s_available = false;
/* _Alignas: ioLibrary casts this to (RIP_MSG*) which has uint32_t fields. */
static _Alignas(uint32_t) uint8_t s_dhcp_buf[1024];  /* DHCP RX buffer (min 548) */

bool app_eth_available(void) { return s_available; }

/* ioLibrary CS callbacks (it wants void(*)(void)). */
static void cs_sel(void)   { spi1_cs_low();  }
static void cs_desel(void) { spi1_cs_high(); }

/* DHCP IP-assign + IP-update callback (same handler for both). Pull the leased
 * address from the client, apply it to the W5500, mirror it into the live cfg
 * for LCD display (RAM only — never committed to FRAM), and mark available. */
static void dhcp_ip_assign(void)
{
    wiz_NetInfo ni = {0};
    for (int i = 0; i < 6; i++) { ni.mac[i] = kEthMac[i]; }
    getIPfromDHCP(ni.ip);
    getGWfromDHCP(ni.gw);
    getSNfromDHCP(ni.sn);
    getDNSfromDHCP(ni.dns);
    ni.dhcp = NETINFO_DHCP;
    wizchip_setnetinfo(&ni);

    /* mirror into the live config so the LCD shows the leased IP (RAM only). */
    app_config_t *cfg = app_lcd_cfg();
    for (int i = 0; i < 4; i++) {
        cfg->ether_ip[i] = ni.ip[i];
        cfg->ether_nm[i] = ni.sn[i];
        cfg->ether_gw[i] = ni.gw[i];
    }

    s_available = true;
    mon_printf("[eth] dhcp lease ip=%u.%u.%u.%u\r\n",
               (unsigned)ni.ip[0], (unsigned)ni.ip[1],
               (unsigned)ni.ip[2], (unsigned)ni.ip[3]);
}

/* DHCP IP-conflict callback. NON-FATAL (samd20's while(1) halt is dropped):
 * drop availability and let the library DECLINE + re-request. */
static void dhcp_ip_conflict(void)
{
    s_available = false;
    mon_printf("[eth] dhcp ip conflict — re-requesting\r\n");
}

/* Apply the net config on the PHY link-up transition (from app_eth_tick).
 * DHCP mode starts the client (stays unavailable until the lease arrives); any
 * other mode applies the static netinfo from cfg and becomes available.
 * Intentionally unconditional w.r.t. comm_mode (SERIAL included): in SERIAL the
 * TCP gate (comm_mode!=SERIAL) keeps the server closed, but a later runtime
 * switch to ETH_STATIC then serves without a reboot — preserving slice-2a
 * behavior. */
static void eth_apply_on_link(void)
{
    const app_config_t *cfg = app_lcd_cfg();

    if (cfg->comm_mode == COMM_ETH_DHCP) {
        /* Put our MAC on the chip (SHAR) BEFORE DHCP_init: the ioLibrary client
         * reads SHAR for the DISCOVER/REQUEST CHADDR and, if it is all-zero
         * (post-reset), substitutes a temporary 00:08:dc:00:00:00 — the lease
         * would bind to the wrong MAC and renewals would carry a stale CHADDR.
         * samd20 set this via network-init's wizchip_setnetinfo. (IP/SN/GW left
         * zero; DHCP_init zeroes SIPR/GAR anyway, the lease fills them.) */
        wiz_NetInfo dni = {0};
        for (int i = 0; i < 6; i++) { dni.mac[i] = kEthMac[i]; }
        dni.dhcp = NETINFO_DHCP;
        wizchip_setnetinfo(&dni);

        DHCP_init(SOCK_DHCP, s_dhcp_buf);
        reg_dhcp_cbfunc(dhcp_ip_assign, dhcp_ip_assign, dhcp_ip_conflict);
        s_phase = ETH_DHCP_RUN;
        mon_printf("[eth] dhcp init — acquiring lease...\r\n");
        return;   /* s_available stays false until the lease arrives */
    }

    /* static netinfo from cfg(FRAM). */
    wiz_NetInfo ni = {0};
    for (int i = 0; i < 6; i++) { ni.mac[i] = kEthMac[i]; }
    for (int i = 0; i < 4; i++) {
        ni.ip[i]  = cfg->ether_ip[i];
        ni.sn[i]  = cfg->ether_nm[i];
        ni.gw[i]  = cfg->ether_gw[i];
        ni.dns[i] = 0u;
    }
    ni.dhcp = NETINFO_STATIC;
    wizchip_setnetinfo(&ni);

    s_available = true;
    s_phase     = ETH_STATIC_UP;
    mon_printf("[eth] up ip=%u.%u.%u.%u\r\n",
               (unsigned)ni.ip[0], (unsigned)ni.ip[1],
               (unsigned)ni.ip[2], (unsigned)ni.ip[3]);
}

bool app_eth_init(void)
{
    s_available = false;
    s_phase     = ETH_DOWN;
    spi1_init();

    /* Register the ioLibrary SPI / CS callbacks.
     * CRITICAL: the byte callbacks (spi1_rb/spi1_wb) must NOT touch CS — the
     * ioLibrary holds CS low across the whole frame via cs_sel/cs_desel. */
    reg_wizchip_cs_cbfunc(cs_sel, cs_desel);
    reg_wizchip_spi_cbfunc(spi1_rb, spi1_wb);
    reg_wizchip_spiburst_cbfunc(spi1_burst_read, spi1_burst_write);

    spi1_eth_reset();

    /* socket buffer sizes: 2KB each. wizchip_init returns -1 if they don't fit. */
    uint8_t txsize[8] = { 2, 2, 2, 2, 2, 2, 2, 2 };
    uint8_t rxsize[8] = { 2, 2, 2, 2, 2, 2, 2, 2 };
    if (wizchip_init(txsize, rxsize) != 0) {
        mon_printf("[eth] wizchip_init failed — unavailable\r\n");
        return false;   /* phase stays ETH_DOWN: chip absent, no retry */
    }

    /* Non-blocking: do NOT wait here for the PHY link (the W5500 auto-negotiates
     * ~1.5 s after reset). app_eth_tick() polls the link and applies the net
     * config when it comes up. */
    s_phase = ETH_LINKWAIT;
    mon_printf("[eth] chip up — waiting for PHY link...\r\n");
    return true;
}

/* Superloop driver. While LINKWAIT, polls the PHY link every LINK_POLL_MS and
 * applies the net config on link-up. While DHCP_RUN, drives the DHCP client
 * (1 s DHCP_time_handler + DHCP_run; DHCP_FAILED -> re-init, keep retrying, no
 * static fallback — spec §3.3). No-op once STATIC_UP or DOWN. */
void app_eth_tick(void)
{
    uint32_t now = sys_tick_get_ms();

    switch (s_phase) {
    case ETH_LINKWAIT: {
        static uint32_t s_link_ms = 0u;
        if ((uint32_t)(now - s_link_ms) >= LINK_POLL_MS) {
            s_link_ms = now;
            int8_t link = PHY_LINK_OFF;
            if (ctlwizchip(CW_GET_PHYLINK, (void *)&link) == 0 &&
                link == PHY_LINK_ON) {
                eth_apply_on_link();
            }
        }
        break;
    }
    case ETH_DHCP_RUN: {
        static uint32_t s_dhcp_ms = 0u;
        if ((uint32_t)(now - s_dhcp_ms) >= DHCP_TICK_MS) {
            s_dhcp_ms = now;
            DHCP_time_handler();
        }
        switch (DHCP_run()) {   /* assign/conflict callbacks fire from here */
        case DHCP_FAILED:
            s_available = false;
            DHCP_init(SOCK_DHCP, s_dhcp_buf);   /* keep retrying — no fallback */
            break;
        default:
            break;              /* ASSIGN/CHANGED/LEASED handled in callbacks */
        }
        break;
    }
    case ETH_STATIC_UP:
    case ETH_DOWN:
    default:
        break;
    }
}
