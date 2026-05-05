/* fw/src/app.c — Stage A: banner 갱신 + LCD demo cadence */
#include "stm32f4xx_hal.h"
#include "app.h"
#include "board.h"
#include "sys_tick.h"
#include "mon.h"
#include "dgus_lcd.h"

void app_init(void)
{
    sys_tick_init();
    mon_init();
    mon_writeln("[boot] gds_us_ctrl stage-a-lcd ready");
    mon_printf("[lcd] usart1@115200 ring=64 prio=5\r\n");

#if DGUS_DEMO_RESET_ON_BOOT
    dgus_reset_lcd();
#endif

    dgus_set_page(DGUS_DEMO_BOOT_PAGE);
    mon_printf("[lcd] init ok, set_page=%u, uptime VP=0x%04X\r\n",
               (unsigned)DGUS_DEMO_BOOT_PAGE, (unsigned)DGUS_DEMO_UPTIME_VP);
}

void app_loop_iter(void)
{
    uint32_t now = sys_tick_get_ms();

    /* 1. LCD RX drain — 매 iter 호출. ring 비어 있으면 dgus_rx_poll 즉시 false (저비용). */
    dgus_frame_t f;
    while (dgus_rx_poll(&f)) {
        if (dgus_is_echo(&f)) {
            continue;                                   /* WR-echo 드롭 */
        }
        mon_printf("[lcd] rx cmd=0x%02X vp=0x%04X len=%u data=",
                   (unsigned)f.cmd, (unsigned)f.vp_addr, (unsigned)f.data_len);
        for (uint8_t i = 0; i < f.data_len; i++) {
            mon_printf("%02X ", (unsigned)f.data[i]);
        }
        mon_printf("\r\n");
    }

    /* 2. 1 Hz cadence — Phase 2 hello + Stage A uptime VP write */
    static uint32_t prev_lcd_tick = 0;
    if ((uint32_t)(now - prev_lcd_tick) >= 1000) {
        prev_lcd_tick = now;
        uint16_t secs = (uint16_t)(now / 1000);
        dgus_write_u16(DGUS_DEMO_UPTIME_VP, secs);
        mon_printf("[t=%lu ms] hello uptime=%u\r\n",
                   (unsigned long)now, (unsigned)secs);
    }
}
