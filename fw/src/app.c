/* fw/src/app.c — Stage B: FRAM config load + LCD init_mode + liveness cadence */
#include "stm32f4xx_hal.h"
#include "app.h"
#include "board.h"
#include "sys_tick.h"
#include "mon.h"
#include "dgus_lcd.h"
#include "app_config.h"
#include "app_lcd.h"
#include "app_reg.h"
#include "app_modbus.h"
#include "app_eth.h"

void app_init(void)
{
    app_config_t *cfg = app_lcd_cfg();   /* config owned by the LCD subsystem */

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

    app_config_load(cfg);             /* FRAM read; factory-write on blank (0xAA flag) */
    app_lcd_init_mode(cfg);           /* model str + VP pre-fill + set_page(run) */

    /* Re-assert the run page until SYS_PIC_NOW confirms it — covers the panel
     * reverting to its boot page after finishing its own splash. */
    bool page_ok = app_lcd_ensure_run_page(cfg);
    mon_printf("[lcd] run_page_confirmed=%u\r\n", (unsigned)page_ok);

    mon_printf("[cfg] freq=%u type=%u work=%lu energy=%lu en_e=%u en_m=%u\r\n",
               (unsigned)cfg->model_freq, (unsigned)cfg->model_type,
               (unsigned long)cfg->work_cnt, (unsigned long)cfg->limit_energy,
               (unsigned)cfg->energy_ctrl, (unsigned)cfg->multi_ctrl);

#ifdef LCD_TRACE_RX
    mon_printf("[lcd] boot cm=%u ip=%u.%u.%u.%u\r\n", (unsigned)cfg->comm_mode,
               cfg->ether_ip[0], cfg->ether_ip[1], cfg->ether_ip[2], cfg->ether_ip[3]);
#endif

    /* Boot handshake done: now honor panel SYS_PIC_NOW re-init reports (spec §10).
     * Gating on this flag stops the §10 loop guard from firing during the
     * Stage B cold-boot dance above. */
    app_lcd_state()->boot_complete = true;
}

void app_loop_iter(void)
{
    /* 1. LCD RX drain — 매 iter 호출. ring 비어 있으면 dgus_rx_poll 즉시 false (저비용). */
    dgus_frame_t f;
    while (dgus_rx_poll(&f)) {
        if (dgus_is_echo(&f)) {
            continue;                                   /* WR-echo 드롭 */
        }
        app_lcd_input_dispatch(&f);                     /* panel touch/key → spec §7 handler */
    }

    /* 2. Display step machine — 4 ms cadence (spec §11), one VP-group per step. */
    app_lcd_tick();

    /* 3. Regulation core — ~2 ms cadence (spec §6), compute-only this slice.
     * limit_on_time injected from the live config (cpp-review M1: app_reg
     * must not reach back into app_lcd); per-iter read = live panel edits. */
    app_reg_tick(app_lcd_cfg()->limit_on_time);

    /* 4. Ethernet/DHCP — drive the W5500 DHCP client (no-op unless DHCP mode).
     * Before Modbus so a lease acquired this iter flips app_eth_available(). */
    app_eth_tick();

    /* 5. Modbus slave — occupancy re-eval + one RTU/TCP frame per iter (spec §2).
     * After app_reg_tick so the mirror sees this iter's freshest measure. */
    app_modbus_tick();
}
