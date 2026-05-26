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

    /* Cold-boot race fix: the DGUS panel boots slower than the MCU, so the old
     * blind 1 s delay + one-shot set_page lost the command and the panel stayed
     * on its power-on logo. Gate on the panel answering a SYS_PIC_NOW read
     * instead (replicates samd20's intended-but-commented check_lcd_comm
     * handshake, ref/samd20/main.c:4933-5022). */
    bool lcd_up = dgus_wait_ready(DGUS_BOOT_READY_TIMEOUT_MS);
    mon_printf("[lcd] ready=%u\r\n", (unsigned)lcd_up);

    /* Show the logo splash for a deliberate dwell, then switch to the run page
     * (samd20 UX: set_page(0) → 1 s → run). The readiness gate above guarantees
     * the panel is up, so this set_page(LOGO) lands and the logo is actually
     * visible for the dwell (on ST-LINK reset it also forces logo→run). */
    dgus_set_page(LCD_LOGO);          /* page 0 — logo */
    sys_tick_delay_ms(DGUS_LOGO_DWELL_MS);

    app_config_load(&g_cfg);          /* FRAM read; factory-write on blank (0xAA flag) */
    app_lcd_init_mode(&g_cfg);        /* model str + VP pre-fill + set_page(run) */

    /* Re-assert the run page until SYS_PIC_NOW confirms it — covers the panel
     * reverting to its boot page after finishing its own splash. */
    bool page_ok = app_lcd_ensure_run_page(&g_cfg);
    mon_printf("[lcd] run_page_confirmed=%u\r\n", (unsigned)page_ok);

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
