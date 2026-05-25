/* fw/src/app.c — Stage B: FRAM config load + LCD init_mode + liveness cadence */
#include "stm32f4xx_hal.h"
#include "app.h"
#include "board.h"
#include "sys_tick.h"
#include "mon.h"
#include "dgus_lcd.h"
#include "app_config.h"
#include "app_lcd.h"

static app_config_t g_cfg;

void app_init(void)
{
    sys_tick_init();
    mon_init();
    mon_writeln("[boot] gds_us_ctrl stage-b ready");

#if DGUS_DEMO_RESET_ON_BOOT
    dgus_reset_lcd();
#endif

    dgus_set_page(LCD_LOGO);          /* page 0 — logo */
    sys_tick_delay_ms(1000);          /* DGUS T5L boot settle (TIM11 tick, not HAL_Delay) */

    app_config_load(&g_cfg);          /* FRAM read; factory-write on blank (0xAA flag) */
    app_lcd_init_mode(&g_cfg);        /* model str + VP pre-fill + set_page(run) */

    mon_printf("[cfg] freq=%u type=%u work=%lu energy=%lu en_e=%u en_m=%u\r\n",
               (unsigned)g_cfg.model_freq, (unsigned)g_cfg.model_type,
               (unsigned long)g_cfg.work_cnt, (unsigned long)g_cfg.limit_energy,
               (unsigned)g_cfg.energy_ctrl, (unsigned)g_cfg.multi_ctrl);
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
