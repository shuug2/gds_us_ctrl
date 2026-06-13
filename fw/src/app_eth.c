/* fw/src/app_eth.c — see app_eth.h. ioLibrary callback wiring mirrors samd20
 * ethernet.c, retargeted to STM32 SPI1 (spi1.c) + W5500 chip. */
#include "app_eth.h"
#include "spi1.h"
#include "app_lcd.h"        /* app_lcd_cfg() -> ether_ip/nm/gw */
#include "app_config.h"
#include "mon.h"
#include "stm32f4xx_hal.h"  /* HAL_Delay */
#include "wizchip_conf.h"
#include "socket.h"

/* Faithful to samd20 ethernet.c default MAC (per-unit MAC = future, spec §8). */
static const uint8_t kEthMac[6] = { 0x00, 0x08, 0xdc, 0x78, 0x91, 0x71 };

static bool s_available = false;

bool app_eth_available(void) { return s_available; }

/* ioLibrary CS callbacks (it wants void(*)(void)). */
static void cs_sel(void)   { spi1_cs_low();  }
static void cs_desel(void) { spi1_cs_high(); }

bool app_eth_init(void)
{
    s_available = false;
    spi1_init();

    /* Register the ioLibrary SPI / CS callbacks.
     * CRITICAL: the byte callbacks (spi1_rb/spi1_wb) must NOT touch CS — the
     * ioLibrary holds CS low across the whole frame via cs_sel/cs_desel. */
    reg_wizchip_cs_cbfunc(cs_sel, cs_desel);
    reg_wizchip_spi_cbfunc(spi1_rb, spi1_wb);
    reg_wizchip_spiburst_cbfunc(spi1_burst_read, spi1_burst_write);

    spi1_eth_reset();

    /* socket buffer sizes: 2KB each, socket 0 only used. wizchip_init returns
     * -1 if the requested sizes don't fit. */
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

    /* static netinfo from cfg(FRAM). */
    const app_config_t *cfg = app_lcd_cfg();
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
