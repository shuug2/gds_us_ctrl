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
#include "app_weld.h"
#include "app_seek_reset.h"
#include "app_overload.h"
#include "app_input.h"
#include "app_horn.h"
#include "app_modbus.h"
#include "app_eth.h"
#include "i2c1.h"    /* i2c1_err_count / i2c1_unstick_events — 부팅·1s 관측 로그 */
#include "app_buzzer.h"
#include "app_fault_alarm.h"

static volatile uint8_t s_boot_rst;   /* RCC->CSR[31:24] @boot — SWD 정적 read 진단용 (IWDG=0x20
                                      * 비트). volatile 필수: 이 값은 코드가 안 읽으므로 -O2 면
                                      * 심볼째 사라진다(형제 i2c1.c s_err_count 동일 이유). */

/* FRAM 로드+LCD 부팅 */
void app_init(void)
{
    app_config_t *cfg = app_lcd_cfg();   /* config owned by the LCD subsystem */

    /* sys_tick_init()은 main()으로 hoist됨 — OSC 부팅 초기화(app_init 전 실행)가
     * sys_tick_get_ms()를 필요로 하기 때문. */
    mon_init();
    /* 리셋 원인 — CSR 플래그는 POR 또는 RMVF 로만 지워지므로 읽은 뒤 즉시 클리어
     * (안 지우면 이전 부팅의 IWDG 플래그가 다음 부팅에 남는다). 기대값:
     * 전원 0x0E(BOR|PIN|POR) / NRST 0x04 / IWDG 0x24(IWDG|PIN).
     * 이 시점은 app_modbus_init() 이전이라 mon_set_enabled(false)가 아직 안 걸려
     * comm_mode 무관하게 USART6 로 나간다 — 배너를 뒤로 옮기지 말 것.
     * spec docs/superpowers/specs/2026-09-04-iwdg-watchdog-design.md §2.7 */
    s_boot_rst = (uint8_t)(RCC->CSR >> 24);
    __HAL_RCC_CLEAR_RESET_FLAGS();
    mon_printf("[boot] gds_us_ctrl ready rst=0x%02X%s\r\n", (unsigned)s_boot_rst,
               (s_boot_rst & (uint8_t)(RCC_CSR_IWDGRSTF >> 24)) ? " IWDG" : "");

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

    uint8_t cfg_fail = app_config_load(cfg);  /* FRAM read; factory-write on blank (0xAA flag) */
    app_lcd_hook_set_pot(cfg->output_power);  /* 부팅 초기 진폭 1회 (samd20 main.c:910) */
    app_lcd_init_mode(cfg);           /* model str + VP pre-fill + set_page(run) */

    /* Re-assert the run page until SYS_PIC_NOW confirms it — covers the panel
     * reverting to its boot page after finishing its own splash. */
    bool page_ok = app_lcd_ensure_run_page(cfg);
    mon_printf("[lcd] run_page_confirmed=%u\r\n", (unsigned)page_ok);

    mon_printf("[cfg] freq=%u type=%u work=%lu energy=%lu en_e=%u en_m=%u\r\n",
               (unsigned)cfg->model_freq, (unsigned)cfg->model_type,
               (unsigned long)cfg->work_cnt, (unsigned long)cfg->limit_energy,
               (unsigned)cfg->energy_ctrl, (unsigned)cfg->multi_ctrl);

    mon_printf("[cfg] fram_fail=%u unstick=%u i2c_err=%u\r\n",
               (unsigned)cfg_fail, (unsigned)i2c1_unstick_events(),
               (unsigned)i2c1_err_count());
    if (cfg_fail == 0xFFu) {
        mon_writeln("[cfg] WARN: FRAM unreadable - ALL defaults, FRAM untouched");
    } else if (cfg_fail != 0u) {
        mon_printf("[cfg] WARN: defaults active for %u field(s)\r\n", (unsigned)cfg_fail);
    }

#ifdef LCD_TRACE_RX
    mon_printf("[lcd] boot cm=%u ip=%u.%u.%u.%u\r\n", (unsigned)cfg->comm_mode,
               cfg->ether_ip[0], cfg->ether_ip[1], cfg->ether_ip[2], cfg->ether_ip[3]);
#endif

    /* Boot handshake done: now honor panel SYS_PIC_NOW re-init reports (spec §10).
     * Gating on this flag stops the §10 loop guard from firing during the
     * Stage B cold-boot dance above. */
    app_lcd_state()->boot_complete = true;
}

/* 슈퍼루프 1회 반복 */
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

    /* 2.5 Weld-cycle FSM — 10 ms cadence. WELD가 US_CYCLE로 게이트를 구동하므로
     * app_reg_tick 앞에 둬서 이번 iter publish에 반영. 슬라이스1은 프로덕션
     * 트리거 없음 -> READY 휴면(회귀 영향 없음). */
    app_weld_tick();

    /* 2.55 과부하 — 10 ms. assert면 force-stop(이번 iter reg publish 반영) +
     * deassert면 자동복구 요청(다음 줄 seek_reset_tick이 같은 iter에 처리). */
    app_overload_tick();

    /* 2.57 물리 명령 입력 + E-stop — 10 ms. B_RESET/SEEK는 다음 줄
     * seek_reset_tick이 같은 iter 소비; B_START/force-stop은 app_reg_tick에 반영
     * (app_seek_reset_tick·app_reg_tick 앞 배치). */
    app_input_tick();

    /* 2.58 SYS_HORN horn-down — 10 ms. STD SETUP이 모드 소유(LCD dispatch가
     * set_mode), 양손 키 press 엣지 = 솔 토글. 초음파/weld 배제는 게이트
     * (app_reg_start_allowed / app_weld_tick)가 담당. app_input_tick 뒤 =
     * estop 신선. */
    app_horn_tick();

    /* 2.6 SEEK/RESET FSM — 10 ms cadence. run_active(us_run_status)를 읽어 RUN
     * 직교; ICON/hook만 emit (app_reg에 명령 안 보냄)이라 reg_tick 앞/뒤 무관 —
     * weld 패턴 일관성 위해 weld_tick 다음에 배치 (1-iter stale run_active 무해). */
    app_seek_reset_tick();

    /* 3. Regulation core — ~2 ms cadence (spec §6), compute-only this slice.
     * 런 한계(on-time ceiling / energy-도달 / OVTIME)를 라이브 config에서 주입
     * (cpp-review M1: app_reg는 app_lcd로 콜백 금지); per-iter read = 라이브 편집. */
    {
        const app_config_t *rc = app_lcd_cfg();
        reg_run_limits_t lim = {
            .limit_on_time  = rc->limit_on_time,
            .energy_ctrl    = rc->energy_ctrl ? 1u : 0u,
            .limit_energy   = rc->limit_energy,
            .limit_out_time = rc->limit_out_time,
            .freq_cal_val   = rc->freq_cal_val,
            .model_type     = rc->model_type,
            .cal_val        = rc->cal_val,
        };
        app_reg_tick(&lim);
    }

    /* 4. Ethernet/DHCP — drive the W5500 DHCP client (no-op unless DHCP mode).
     * Before Modbus so a lease acquired this iter flips app_eth_available(). */
    app_eth_tick();

    /* 5. Modbus slave — occupancy re-eval + one RTU/TCP frame per iter (spec §2).
     * After app_reg_tick so the mirror sees this iter's freshest measure. */
    app_modbus_tick();

    /* 6. I2C1 관측 — 1 s cadence, err_count 델타 시에만 mon 1줄 (감사 H2 표면;
     * mon 전용 = 사용자 확정. save_all/POT write 실패 런타임 관측용). */
    {
        static uint32_t s_i2c_chk_ms;
        static uint16_t s_i2c_err_last;
        uint32_t now_ms = sys_tick_get_ms();
        if ((uint32_t)(now_ms - s_i2c_chk_ms) >= 1000u) {
            s_i2c_chk_ms = now_ms;
            uint16_t e = i2c1_err_count();
            if (e != s_i2c_err_last) {
                mon_printf("[i2c] err=%u (+%u)\r\n",
                           (unsigned)e, (unsigned)(e - s_i2c_err_last));
                s_i2c_err_last = e;
            }
        }
    }

    /* 6.5 일반 fault(OVTIME 등) 부저 점멸 — measure.error_status 감시.
     * app_reg_tick(위) publish 후·app_buzzer_tick(아래) 전 = 같은 iter 반영. */
    app_fault_alarm_tick();

    /* 7. 부저 — 10ms gate. 비블로킹 timed-beep 진행 (트리거는 overload/입력/fault 슬라이스). */
    app_buzzer_tick();
}
