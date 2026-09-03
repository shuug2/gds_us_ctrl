/* fw/src/app_modbus.c — samd20 Modbus slave integration port (spec §3~§5).
 * Mirror pass = update_holding_reg(0) field-for-field; write-apply pass =
 * update_holding_reg(1) one-change-per-message else-if chain with the samd20
 * clamps. FRAM persistence = whole-map app_config_save_all (repo pattern;
 * structurally fixes the samd20 DELAY3->ADDR_TRIGGER2 / TRIGGER2->ADDR_DELAY2
 * copy-paste bugs — spec §3.2). Occupancy switching = per-tick cfg compare
 * (samd20 main-loop gate (comm_mode==SERIAL)&&(addr!=0), main.c:5043, plus
 * the DATA_SAVE close/init pair 3387/3429/3501 — tick-polled instead of
 * hook-driven so a comm_mode-only change also releases the port and no
 * app_lcd<->app_modbus include cycle forms; plan Deviations 5). */
#include <string.h>
#include "app_modbus.h"
#include "app_modbus_core.h"
#include "app_eth.h"
#include "app_modbus_tcp.h"
#include "usart6_mb.h"
#include "app_lcd.h"      /* app_lcd_cfg/app_lcd_measure/hooks/us enum */
#include "app_reg.h"
#include "app_overload.h"   /* app_overload_active (STATUS OVLD 비트) */
#include "app_input.h"      /* app_estop_active (STATUS ESTOP 비트) */
#include "app_weld.h"       /* app_weld_sensor_active (STATUS SENSOR 비트, B-3) */
#include "app_horn.h"       /* app_horn_mode_active (STATUS HORN 비트, B-4) */
#include "app_config.h"
#include "dgus_lcd.h"     /* DISP_*_EN echo (samd20 send_lcd_data_var) */
#include "sys_tick.h"     /* REMOTE icon 1 s hold timestamp */
#include "mon.h"

#define MB_COMM_MODE_SERIAL  0u   /* cfg->comm_mode: 0=SERIAL 1=ETH_STATIC 2=ETH_DHCP */

extern void usart6_init(void);    /* drivers/usart.c — mon line config restore */

static mb_core_t g_mb;
static struct {
    uint8_t owned;
    uint8_t speed_idx;
    uint8_t parity_idx;
    uint8_t addr;
} g_applied;
static uint8_t g_tcp_active;   /* rising-edge baseline guard for ETH mode */

#define MB_REMOTE_HOLD_MS  1000u  /* samd20 modbus_comm_cnt>100 @ case-9 ~10ms */
static uint32_t s_remote_ms;    /* last decoded request (REMOTE icon hold base) */
static uint8_t  s_remote_seen;  /* 0 until the first request — boot/wrap guard */

/* REMOTE 시각 스탬프 */
void app_modbus_note_remote(void)
{
    /* samd20 modbus_status 등가: 유효 요청 디코드 시각 스탬프 (RTU/TCP 공용). */
    s_remote_ms   = sys_tick_get_ms();
    s_remote_seen = 1u;
}

/* REMOTE icon 게이트 */
bool app_modbus_remote_active(void)
{
    /* REMOTE icon 게이트: 마지막 요청 후 1 s 유지 (samd20 main.c:5189-5192). */
    return (s_remote_seen != 0u) &&
           ((uint32_t)(sys_tick_get_ms() - s_remote_ms) < MB_REMOTE_HOLD_MS);
}

/* mb 코어 ctx 반환 */
mb_core_t *app_modbus_core(void)
{
    return &g_mb;
}

/* live 값 mirror */
static void mirror_live(void)
{
    /* samd20 update_holding_reg(0): live values -> holding mirror. Runs every
     * owned tick (plan Deviations 6: fresher reads than samd20's post-message
     * refresh + immediately normalizes clamped writes). */
    const app_config_t  *cfg = app_lcd_cfg();
    const lcd_measure_t *m   = app_lcd_measure();
    uint8_t running = (m->us_run_status != (uint8_t)US_IDLE) ? 1u : 0u;

    g_mb.holding[MB_REG_WORK_CNTH]   = (uint16_t)(cfg->work_cnt >> 16);
    g_mb.holding[MB_REG_WORK_CNTL]   = (uint16_t)(cfg->work_cnt);
    g_mb.holding[MB_REG_DELAY1]      = cfg->limit_delay_time1;
    g_mb.holding[MB_REG_DELAY2]      = cfg->limit_delay_time2;
    g_mb.holding[MB_REG_DELAY3]      = cfg->limit_delay_time3;
    g_mb.holding[MB_REG_TRIGGER2]    = cfg->limit_trigger_time2;
    g_mb.holding[MB_REG_TRIGGER3]    = cfg->limit_trigger_time3;
    g_mb.holding[MB_REG_OUT_POWER]   = cfg->output_power;
    g_mb.holding[MB_REG_ON_TIME]     = cfg->limit_on_time;
    g_mb.holding[MB_REG_ENERGY]      = (uint16_t)cfg->limit_energy;
    g_mb.holding[MB_REG_MULTI_T1]    = cfg->limit_mo_time1;
    g_mb.holding[MB_REG_MULTI_T2]    = cfg->limit_mo_time2;
    g_mb.holding[MB_REG_MULTI_O1]    = cfg->limit_mo_out1;
    g_mb.holding[MB_REG_MULTI_O2]    = cfg->limit_mo_out2;
    g_mb.holding[MB_REG_TIMEOVER]    = cfg->limit_out_time;
    /* DISP_*: live shows the running peak, stopped shows the latched last
     * (samd20 main.c:4564-4567 us_on_status mirror). us_on_status = run OR
     * seek/reset active — SEEK/RESET 중에도 라이브 (legacy 4253/4280). STATUS
     * bit0(아래 running)는 run 전용 유지. */
    uint8_t disp_on = m->us_on_status;
    g_mb.holding[MB_REG_DISP_POWER]  = disp_on ? m->max_power : m->last_power;
    g_mb.holding[MB_REG_DISP_AMP]    = disp_on ? m->max_amp   : m->last_amp;
    g_mb.holding[MB_REG_DISP_FREQ]   = disp_on ? m->curr_freq : m->last_freq;
    g_mb.holding[MB_REG_DISP_ENERGY] = disp_on ? (uint16_t)m->curr_energy
                                               : (uint16_t)m->last_energy;
    g_mb.holding[MB_REG_MODEL_FREQ]  = cfg->model_freq;    /* read-only: mirror */
    g_mb.holding[MB_REG_MODEL_TYPE]  = cfg->model_type;    /* overwrites writes */
    g_mb.holding[MB_REG_RUN_MODE]    = cfg->run_mode;
    g_mb.holding[MB_REG_EN_ENERGY]   = cfg->energy_ctrl ? 1u : 0u;
    g_mb.holding[MB_REG_EN_MULTI]    = cfg->multi_ctrl  ? 1u : 0u;
    g_mb.holding[MB_REG_EN_SAFTY]    = cfg->f_safty;
    /* STATUS bit0 = run active (spec §3.1: us_run_status != US_IDLE).
     * OVTIME = app_reg가 publish한 energy 모드 직접런 과대시간 fault
     * (2026-06-28-ovtime spec). OVLD = app_overload_active() 라이브 반영
     * (슬라이스 C). ESTOP = app_estop_active() (슬라이스 D). OUTERR는 6b.
     * SENSOR/HORN = 원격 관측용 신규 비트 (요구사항 B-3/B-4). 비트 배치는
     * mb_status_bits()가 소유 — host 스위트가 겹침·극성까지 고정한다. */
    const mb_status_in_t sin = {
        .running = running,
        .estop   = app_estop_active(),
        .ovld    = app_overload_active(),
        .ovtime  = (m->error_status & ERR_OVTIME) ? 1u : 0u,
        .sensor  = app_weld_sensor_active(),
        .horn    = app_horn_mode_active(),
    };
    g_mb.holding[MB_REG_STATUS]      = mb_status_bits(&sin);
}

/* FC06 write 적용 */
void app_modbus_apply_writes(void)
{
    /* samd20 update_holding_reg(1): one else-if chain per message — commands
     * first (consume-and-clear), then the single config field that differs
     * (clamped, persisted). Chain order preserved verbatim. */
    app_config_t *cfg = app_lcd_cfg();
    uint16_t v;
    bool save = false;

    if (g_mb.holding[MB_REG_RESET] == 1u) {
        /* Routed for the consume-and-clear shape; effect = no-op this slice
         * (RESET->SEEK chain + error machine deferred, spec §3.3). */
        app_reg_command(US_CMD_RESET, (uint8_t)US_COMM);
        g_mb.holding[MB_REG_RESET] = 0u;
    } else if (g_mb.holding[MB_REG_SEEK] == 1u) {
        app_reg_command(US_CMD_SEEK, (uint8_t)US_COMM);   /* no-op, deferred */
        g_mb.holding[MB_REG_SEEK] = 0u;
    } else if (g_mb.holding[MB_REG_START] == 1u) {
        app_reg_command(US_CMD_START, (uint8_t)US_COMM);
        if (app_lcd_measure()->us_run_status == (uint8_t)US_COMM) {
            /* START accepted: samd20 comm START writes the amplitude pot in
             * the same breath (I2C_POT, main.c:4400) — stub hook logs until
             * B-SEAM/F2 resolves the pot identity.
             * NOTE: g_measure publishes on app_reg_tick's ~2 ms gate, so this
             * snapshot is one publish stale and the guard evaluates FALSE in
             * the same iter the run starts — harmless while set_pot is a log
             * stub, but B-SEAM must replace it with a live app_reg accessor
             * (final integration review 2026-06-12). */
            app_lcd_hook_set_pot(cfg->output_power);
        }
        g_mb.holding[MB_REG_START] = 0u;
    } else if (g_mb.holding[MB_REG_STOP] == 1u) {
        app_reg_command(US_CMD_RUN_RELEASE, (uint8_t)US_COMM);
        g_mb.holding[MB_REG_STOP] = 0u;
    } else if (g_mb.holding[MB_REG_DELAY1] != cfg->limit_delay_time1) {
        v = g_mb.holding[MB_REG_DELAY1];
        if (v > 500u) { v = 500u; }
        cfg->limit_delay_time1 = v;
        save = true;
    } else if (g_mb.holding[MB_REG_DELAY2] != cfg->limit_delay_time2) {
        v = g_mb.holding[MB_REG_DELAY2];
        if (v > 500u) { v = 500u; }
        cfg->limit_delay_time2 = v;
        save = true;
    } else if (g_mb.holding[MB_REG_DELAY3] != cfg->limit_delay_time3) {
        v = g_mb.holding[MB_REG_DELAY3];
        if (v > 2000u) { v = 2000u; }
        cfg->limit_delay_time3 = v;     /* samd20 saved this to ADDR_TRIGGER2 —
                                         * copy-paste bug, fixed by save_all */
        save = true;
    } else if (g_mb.holding[MB_REG_TRIGGER2] != cfg->limit_trigger_time2) {
        v = g_mb.holding[MB_REG_TRIGGER2];
        if (v > 500u) { v = 500u; }
        cfg->limit_trigger_time2 = v;   /* samd20 saved to ADDR_DELAY2 — ditto */
        save = true;
    } else if (g_mb.holding[MB_REG_TRIGGER3] != cfg->limit_trigger_time3) {
        v = g_mb.holding[MB_REG_TRIGGER3];
        if (v > 2000u) { v = 2000u; }
        cfg->limit_trigger_time3 = v;
        save = true;
    } else if (g_mb.holding[MB_REG_OUT_POWER] != cfg->output_power) {
        v = g_mb.holding[MB_REG_OUT_POWER];
        if (v > 100u) { v = 100u; }
        else if (v < 50u) { v = 50u; }
        cfg->output_power = (uint8_t)v;
        save = true;
    } else if (g_mb.holding[MB_REG_ON_TIME] != cfg->limit_on_time) {
        v = g_mb.holding[MB_REG_ON_TIME];
        if (v > 2000u) { v = 2000u; }
        cfg->limit_on_time = v;
        save = true;
    } else if (g_mb.holding[MB_REG_ENERGY] != (uint16_t)cfg->limit_energy) {
        cfg->limit_energy = (uint32_t)g_mb.holding[MB_REG_ENERGY];
        save = true;
    } else if (g_mb.holding[MB_REG_MULTI_T1] != cfg->limit_mo_time1) {
        v = g_mb.holding[MB_REG_MULTI_T1];
        if (v > 2000u) { v = 2000u; }
        cfg->limit_mo_time1 = v;
        save = true;
    } else if (g_mb.holding[MB_REG_MULTI_T2] != cfg->limit_mo_time2) {
        v = g_mb.holding[MB_REG_MULTI_T2];
        if (v > 2000u) { v = 2000u; }
        cfg->limit_mo_time2 = v;
        save = true;
    } else if (g_mb.holding[MB_REG_MULTI_O1] != cfg->limit_mo_out1) {
        v = g_mb.holding[MB_REG_MULTI_O1];
        if (v > 100u) { v = 100u; }
        else if (v < 50u) { v = 50u; }
        cfg->limit_mo_out1 = v;
        save = true;
    } else if (g_mb.holding[MB_REG_MULTI_O2] != cfg->limit_mo_out2) {
        v = g_mb.holding[MB_REG_MULTI_O2];
        if (v > 100u) { v = 100u; }
        else if (v < 50u) { v = 50u; }
        cfg->limit_mo_out2 = v;
        save = true;
    } else if (g_mb.holding[MB_REG_TIMEOVER] != cfg->limit_out_time) {
        v = g_mb.holding[MB_REG_TIMEOVER];
        if (v > 10u) { v = 10u; }
        cfg->limit_out_time = v;        /* samd20 wrote the clamp back into the
                                         * reg; our per-tick mirror does that */
        save = true;
    } else if (g_mb.holding[MB_REG_RUN_MODE] != cfg->run_mode) {
        cfg->run_mode = (uint8_t)g_mb.holding[MB_REG_RUN_MODE];   /* no clamp
                                         * (samd20 faithful — stored as-is) */
        save = true;
    } else if (((g_mb.holding[MB_REG_EN_ENERGY] == 0u) && cfg->energy_ctrl) ||
               ((g_mb.holding[MB_REG_EN_ENERGY] == 1u) && !cfg->energy_ctrl)) {
        /* samd20 acts only on exact 0/1 values (4512) — faithful. */
        cfg->energy_ctrl = (g_mb.holding[MB_REG_EN_ENERGY] == 1u);
        dgus_write_u16(DISP_ENERGY_EN, cfg->energy_ctrl ? 1u : 0u);
        save = true;
    } else if (((g_mb.holding[MB_REG_EN_MULTI] == 0u) && cfg->multi_ctrl) ||
               ((g_mb.holding[MB_REG_EN_MULTI] == 1u) && !cfg->multi_ctrl)) {
        cfg->multi_ctrl = (g_mb.holding[MB_REG_EN_MULTI] == 1u);
        dgus_write_u16(DISP_MULTI_EN, cfg->multi_ctrl ? 1u : 0u);
        save = true;
    } else if (g_mb.holding[MB_REG_EN_SAFTY] != cfg->f_safty) {
        /* no 0/1 normalization: samd20 stores as-is (4533) — the LCD input
         * path normalizes its own writes; comm writes stay faithful */
        cfg->f_safty = (uint8_t)g_mb.holding[MB_REG_EN_SAFTY];
        dgus_write_u16(DISP_SAFTY, cfg->f_safty);
        save = true;
    } else if ((g_mb.holding[MB_REG_WORK_CNTL] == 0u) &&
               ((uint16_t)cfg->work_cnt != 0u)) {
        /* CNTL=0 write = work counter reset (samd20 main.c:4539: cfg + FRAM +
         * LCD refresh). Low-word compare faithful to the samd20 condition. */
        cfg->work_cnt = 0u;
        app_lcd_set_work_cnt(0u);
        save = true;
    }

    if (save) {
        /* Whole-map FRAM commit — codebase pattern (data_save_commit).
         * ~2 ms at 400 kHz nominal; the 50 ms/call I2C timeout governs the
         * worst case (bus hang). Same budget as the LCD DATA_SAVE path. */
        app_config_save_all(cfg);
    }
    /* mirror_live() runs right after in app_modbus_tick(): the next read
     * returns the clamped/applied value, and a clamped write can't re-fire
     * this chain on the next message. */
}

/* 점유/라인설정 전이 */
static void apply_config(void)
{
    /* Occupancy + line-config edge detector. Cheap compares every tick;
     * transitions (close/open, mon gate) only on change. */
    const app_config_t *cfg = app_lcd_cfg();
    uint8_t want = ((cfg->comm_mode == MB_COMM_MODE_SERIAL) &&
                    (cfg->comm_address != 0u)) ? 1u : 0u;

    if ((want != 0u) && (g_applied.owned != 0u) &&
        (cfg->comm_speed_idx == g_applied.speed_idx) &&
        (cfg->comm_parity_idx == g_applied.parity_idx)) {
        if (cfg->comm_address != g_applied.addr) {
            /* address-only change: retarget the slave id, no line re-init */
            g_applied.addr   = cfg->comm_address;
            g_mb.device_addr = cfg->comm_address;
        }
        return;                                   /* steady state */
    }
    if ((want == 0u) && (g_applied.owned == 0u)) {
        return;                                   /* steady state */
    }

    if (g_applied.owned != 0u) {
        /* release (or reconfigure: close first, reacquire below) */
        usart6_mb_close();
        usart6_init();                            /* restore mon 115200 8N1 */
        mon_set_enabled(true);
        g_applied.owned = 0u;
        /* NOTE: a US_COMM run active at this point keeps running until the
         * on-time ceiling stops it (samd20-faithful link-loss behavior;
         * ceiling=0 disables that net — power cycle is then the only stop). */
        mon_printf("[mb] release usart6 (mode=%u addr=%u)\r\n",
                   (unsigned)cfg->comm_mode, (unsigned)cfg->comm_address);
    }
    if (want != 0u) {
        /* log BEFORE the gate closes so the transition is visible on mon */
        mon_printf("[mb] acquire usart6 speed=%u parity=%u addr=%u\r\n",
                   (unsigned)cfg->comm_speed_idx,
                   (unsigned)cfg->comm_parity_idx,
                   (unsigned)cfg->comm_address);
        mon_set_enabled(false);
        usart6_mb_open(cfg->comm_speed_idx, cfg->comm_parity_idx);
        mb_core_init(&g_mb, cfg->comm_address);   /* samd20 init_modbus zeroes tables */
        g_applied.owned      = 1u;
        g_applied.speed_idx  = cfg->comm_speed_idx;
        g_applied.parity_idx = cfg->comm_parity_idx;
        g_applied.addr       = cfg->comm_address;
        mirror_live();   /* baseline (samd20 boot update_holding_reg(0), main.c:5027) */
    }
}

/* modbus 글루 초기화 */
void app_modbus_init(void)
{
    memset(&g_applied, 0, sizeof(g_applied));
    mb_core_init(&g_mb, 0u);
    apply_config();
}

/* TCP 이탈 정리 */
static void tcp_leave(void)
{
    /* M8: TCP 서버를 떠나는 전이(RTU 점유 or ETH 불가)에서 sock0 + 수신 상태
     * 정리 — ESTABLISHED 방치 차단(감사 M8). 전이에서만 호출해 매-tick 중복
     * close(무해하나 SPI 낭비) 회피. g_tcp_active=1은 app_eth_available() 참
     * 경로에서만 세워지므로 칩-부재 시 도달 불가(vendor close 스핀 안전). */
    if (g_tcp_active != 0u) {
        app_modbus_tcp_reset();
        g_tcp_active = 0u;
    }
}

/* modbus 매 tick 처리 */
void app_modbus_tick(void)
{
    apply_config();
    const app_config_t *cfg = app_lcd_cfg();

    if (g_applied.owned != 0u) {
        /* RTU owns USART6 (comm_mode==SERIAL && addr!=0). Behavior-identical
         * to the hardware-verified slice-1 path. */
        tcp_leave();
        uint8_t frame[MB_FRAME_MAX];
        uint8_t len = usart6_mb_rx_frame(frame, sizeof frame);
        if (len != 0u) {
            uint8_t resp[MB_RESP_MAX];
            uint8_t fc = 0u;
            uint8_t n  = mb_core_decode(&g_mb, frame, len, MB_MODE_RTU, resp, &fc);
            if (n != 0u) {
                app_modbus_note_remote();   /* REMOTE icon (samd20 modbus_status) */
                usart6_mb_send(resp, n);
            }
            if (fc == 0x06u) {
                app_modbus_apply_writes(); /* samd20: update_holding_reg(1) on FC06 */
            }
        }
        mirror_live();
        return;
    }

    /* Not RTU. Run the TCP server when in an ETH mode and the W5500 is up.
     * This also closes a gap: the old early-return when !owned meant
     * mirror_live() never ran in ETH mode, so FC03 reads would go stale. */
    if ((cfg->comm_mode != MB_COMM_MODE_SERIAL) && app_eth_available()) {
        if (g_tcp_active == 0u) {
            mb_core_init(&g_mb, cfg->comm_address);  /* seed addr + zero tables */
            mirror_live();   /* baseline before first poll (matches apply_config
                              * RTU-acquisition): avoids a zeroed-holding[] read
                              * window on a SERIAL->ETH switch with a held socket */
            g_tcp_active = 1u;
        }
        app_modbus_tcp_poll();   /* decode(MB_MODE_TCP) + apply on FC06 + respond */
        mirror_live();           /* keep holding[] fresh for reads (closes the gap) */
    } else {
        tcp_leave();
    }
}
