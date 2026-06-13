/* fw/src/app_eth.c — see app_eth.h. ioLibrary callback wiring mirrors samd20
 * ethernet.c, retargeted to STM32 SPI1 (spi1.c) + W5500 chip. Static IP from
 * cfg, or DHCP-acquired (comm_mode==ETH_DHCP) via the ioLibrary DHCP client
 * driven from app_eth_tick(). */
#include "app_eth.h"
#include "spi1.h"
#include "app_lcd.h"        /* app_lcd_cfg() -> comm_mode, ether_ip/nm/gw */
#include "app_config.h"
#include "mon.h"
#include "sys_tick.h"       /* sys_tick_get_ms() — 1s DHCP cadence */
#include "stm32f4xx_hal.h"  /* HAL_Delay */
#include "wizchip_conf.h"
#include "socket.h"
#include "dhcp.h"

#define COMM_ETH_DHCP  2u   /* cfg->comm_mode: 0=SERIAL 1=ETH_STATIC 2=ETH_DHCP */
#define SOCK_DHCP      1u   /* UDP socket for DHCP (TCP server uses socket 0) */

/* Faithful to samd20 ethernet.c default MAC (per-unit MAC = future, spec §8). */
static const uint8_t kEthMac[6] = { 0x00, 0x08, 0xdc, 0x78, 0x91, 0x71 };

static bool    s_available   = false;
static bool    s_dhcp_active  = false;   /* DHCP_init done → drive in app_eth_tick */
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

bool app_eth_init(void)
{
    s_available   = false;
    s_dhcp_active  = false;
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
        return false;
    }

    /* PHY-link poll, non-fatal timeout (~1s = 100 x 10ms). */
    int8_t link = PHY_LINK_OFF;
    for (int i = 0; i < 100; i++) {
        if (ctlwizchip(CW_GET_PHYLINK, (void *)&link) == 0 && link == PHY_LINK_ON) {
            break;
        }
        HAL_Delay(10);
    }
    if (link != PHY_LINK_ON) {
        mon_printf("[eth] no PHY link — unavailable\r\n");
        return false;
    }

    const app_config_t *cfg = app_lcd_cfg();

    if (cfg->comm_mode == COMM_ETH_DHCP) {
        /* Put our MAC on the chip (SHAR) BEFORE DHCP_init: the ioLibrary client
         * reads SHAR for the DISCOVER/REQUEST CHADDR and, if it is all-zero
         * (post-reset), substitutes a temporary 00:08:dc:00:00:00 — the lease
         * would bind to the wrong MAC and renewals would carry a stale CHADDR.
         * samd20 set this via network-init's wizchip_setnetinfo. (IP/SN/GW are
         * left zero here; DHCP_init zeroes SIPR/GAR anyway, and the lease fills
         * them via dhcp_ip_assign.) */
        wiz_NetInfo dni = {0};
        for (int i = 0; i < 6; i++) { dni.mac[i] = kEthMac[i]; }
        dni.dhcp = NETINFO_DHCP;
        wizchip_setnetinfo(&dni);

        /* DHCP: start the client; the lease is acquired in app_eth_tick() and
         * applied by dhcp_ip_assign(). Stay unavailable until then. */
        DHCP_init(SOCK_DHCP, s_dhcp_buf);
        reg_dhcp_cbfunc(dhcp_ip_assign, dhcp_ip_assign, dhcp_ip_conflict);
        s_dhcp_active = true;
        mon_printf("[eth] dhcp init — acquiring lease...\r\n");
        return true;   /* chip+link up; s_available stays false until lease */
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

    mon_printf("[eth] up ip=%u.%u.%u.%u\r\n",
               (unsigned)ni.ip[0], (unsigned)ni.ip[1],
               (unsigned)ni.ip[2], (unsigned)ni.ip[3]);
    s_available = true;
    return true;
}

/* Drive the DHCP client (no-op unless comm_mode==ETH_DHCP started a lease).
 * Call every superloop iteration. DHCP_time_handler must tick at 1s during
 * acquisition AND lease (else DISCOVER never retransmits — spec §3.2). On
 * DHCP_FAILED we re-init to keep retrying (no static fallback — spec §3.3). */
void app_eth_tick(void)
{
    if (!s_dhcp_active) {
        return;
    }

    static uint32_t s_prev_ms = 0u;
    uint32_t now = sys_tick_get_ms();
    if ((uint32_t)(now - s_prev_ms) >= 1000u) {
        s_prev_ms = now;
        DHCP_time_handler();
    }

    switch (DHCP_run()) {       /* assign/conflict callbacks fire from here */
        case DHCP_FAILED:
            s_available = false;
            DHCP_init(SOCK_DHCP, s_dhcp_buf);   /* keep retrying — no fallback */
            break;
        default:
            break;              /* ASSIGN/CHANGED/LEASED handled in callbacks */
    }
}
