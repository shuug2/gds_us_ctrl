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
#include "app_input.h"      /* app_estop_active (STATUS ESTOP 비트 + 게이트 해제) */
#include "app_weld.h"       /* app_weld_sensor_active (STATUS SENSOR 비트, B-3) */
#include "app_horn.h"       /* app_horn_mode_active (STATUS HORN 비트, B-4) */
#include "app_remote_en_fsm.h"   /* 원격 활성화 게이트 (요구사항 A) */
#include "app_cfg_stage.h"  /* comm/eth staging + commit (F-A) */
#include "io.h"             /* io_read_remote_en (PC8 물리 인터록) */
#include "define.h"         /* MODEL_REMOTE — 게이트는 REMOTE 모델 전용 */
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

/* 원격 활성화 게이트 상태 (비영속 — holding[]은 링크 전이의 mb_core_init이 0으로
 * 지우고 FRAM 저장은 요구사항 위반이라, 파일 static만 가능).
 * s_ren = 마지막 step 출력 캐시(미러 + apply 게이트가 소비).
 * 조작 입력은 PC8 물리 스위치 레벨뿐이라 1-shot 래치가 필요 없다. */
static remote_en_out_t s_ren;

/* comm/eth staging 버퍼 (F-A). 비영속 — 링크 전이에서 무조건 폐기한다. */
static cfg_stage_t s_stg;

/* staged 인덱스 → 레지스터 주소. 미러와 스캔이 같은 표를 쓴다. */
static const uint8_t k_stg_reg[CFG_STG_COUNT] = {
    MB_REG_COMM_ADDR,  MB_REG_COMM_SPEED, MB_REG_COMM_PARITY,
    MB_REG_ETHER_IP_H, MB_REG_ETHER_IP_L,
    MB_REG_ETHER_NM_H, MB_REG_ETHER_NM_L,
    MB_REG_ETHER_GW_H, MB_REG_ETHER_GW_L,
};

/* 비-dirty staged 레지스터가 보여야 할 cfg 라이브 값. IP/NM/GW 는 2옥텟/레지스터
 * (1옥텟이면 12칸을 먹고 FC06 쓰기 횟수도 2배 — RS-485 첫 write 간헐 무효 노출이
 * 그만큼 늘어난다). WORK_CNTH/L 상하위 분할 선례와 같은 형태다. */
static uint16_t stg_mirror_val(const app_config_t *cfg, uint8_t idx)
{
    switch (idx) {
    case CFG_STG_ADDR:   return cfg->comm_address;
    case CFG_STG_SPEED:  return cfg->comm_speed_idx;
    case CFG_STG_PARITY: return cfg->comm_parity_idx;
    case CFG_STG_IP_H:   return (uint16_t)((cfg->ether_ip[0] << 8) | cfg->ether_ip[1]);
    case CFG_STG_IP_L:   return (uint16_t)((cfg->ether_ip[2] << 8) | cfg->ether_ip[3]);
    case CFG_STG_NM_H:   return (uint16_t)((cfg->ether_nm[0] << 8) | cfg->ether_nm[1]);
    case CFG_STG_NM_L:   return (uint16_t)((cfg->ether_nm[2] << 8) | cfg->ether_nm[3]);
    case CFG_STG_GW_H:   return (uint16_t)((cfg->ether_gw[0] << 8) | cfg->ether_gw[1]);
    default:             return (uint16_t)((cfg->ether_gw[2] << 8) | cfg->ether_gw[3]);
    }
}

/* 커밋 통과분을 cfg 에 일괄 반영. d = 커밋이 dirty 를 지우기 전에 떠 둔 스냅샷.
 * 범위 검증이 이미 통과했으므로 u16→u8 절단은 손실이 없다 (ether 는 2옥텟 패킹
 * 자체가 무손실). */
static void stg_apply_to_cfg(app_config_t *cfg, uint16_t d)
{
    if ((d & (1u << CFG_STG_ADDR))   != 0u) { cfg->comm_address    = (uint8_t)s_stg.val[CFG_STG_ADDR]; }
    if ((d & (1u << CFG_STG_SPEED))  != 0u) { cfg->comm_speed_idx  = (uint8_t)s_stg.val[CFG_STG_SPEED]; }
    if ((d & (1u << CFG_STG_PARITY)) != 0u) { cfg->comm_parity_idx = (uint8_t)s_stg.val[CFG_STG_PARITY]; }
    if ((d & (1u << CFG_STG_IP_H)) != 0u) { cfg->ether_ip[0] = (uint8_t)(s_stg.val[CFG_STG_IP_H] >> 8); cfg->ether_ip[1] = (uint8_t)s_stg.val[CFG_STG_IP_H]; }
    if ((d & (1u << CFG_STG_IP_L)) != 0u) { cfg->ether_ip[2] = (uint8_t)(s_stg.val[CFG_STG_IP_L] >> 8); cfg->ether_ip[3] = (uint8_t)s_stg.val[CFG_STG_IP_L]; }
    if ((d & (1u << CFG_STG_NM_H)) != 0u) { cfg->ether_nm[0] = (uint8_t)(s_stg.val[CFG_STG_NM_H] >> 8); cfg->ether_nm[1] = (uint8_t)s_stg.val[CFG_STG_NM_H]; }
    if ((d & (1u << CFG_STG_NM_L)) != 0u) { cfg->ether_nm[2] = (uint8_t)(s_stg.val[CFG_STG_NM_L] >> 8); cfg->ether_nm[3] = (uint8_t)s_stg.val[CFG_STG_NM_L]; }
    if ((d & (1u << CFG_STG_GW_H)) != 0u) { cfg->ether_gw[0] = (uint8_t)(s_stg.val[CFG_STG_GW_H] >> 8); cfg->ether_gw[1] = (uint8_t)s_stg.val[CFG_STG_GW_H]; }
    if ((d & (1u << CFG_STG_GW_L)) != 0u) { cfg->ether_gw[2] = (uint8_t)(s_stg.val[CFG_STG_GW_L] >> 8); cfg->ether_gw[3] = (uint8_t)s_stg.val[CFG_STG_GW_L]; }
}

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

/* 게이트 FSM 1 tick.
 *
 * MODEL_STD 에는 인터록 스위치가 없다 — 게이트를 넣으면 스위치 미장착 유닛에서
 * 유선 Modbus HMI 의 설정 쓰기가 죽는다. 그래서 STD 는 게이트 자체를 두지 않고
 * 상시 개방으로 고정한다. 이 #if 하나가 apply_writes 쪽 분기를 대신하므로
 * 아래 게이트 검사는 두 모델 공통 코드로 남는다 (분기 확산 방지). */
static void remote_en_step(void)
{
#if defined(MODEL_REMOTE)
    remote_en_in_t in;

    in.now_ms      = sys_tick_get_ms();
    /* PC8 active-LOW → 논리 "허용". 극성은 여기 한 곳에서만 뒤집는다. */
    in.sw          = (io_read_remote_en() == 0u) ? 1u : 0u;
    /* 침묵 입력 = REMOTE 아이콘과 같은 스탬프. note_remote가 유효 디코드 전부에
     * 찍히므로 읽기 요청도 링크 생존 신호다. 진입 이전 값일 수 있으나 FSM의
     * 무장 규칙이 걸러낸다. MB_REMOTE_HOLD_MS와는 무관. */
    in.last_req_ms = s_remote_ms;
    in.req_valid   = s_remote_seen;
    in.estop       = app_estop_active();

    remote_en_fsm_step(&in, &s_ren);
#else
    s_ren.state = (uint8_t)REN_ENABLED;   /* STD: 인터록 없음 = 상시 통과 */
#endif
}

/* 게이트 상태 조회 */
uint8_t app_remote_en_state(void)
{
    return s_ren.state;
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
    /* B-5: 이제 R/W 다. 미러는 그대로 두는 것이 맞다 — 다른 cfg 필드와 동형으로
     * "쓰기는 apply 체인이 받고, 미러가 결과를 되비춘다". 미러를 없앨 필요가
     * 없었다: 예전에 쓰기가 안 먹은 진짜 원인은 미러가 아니라 **apply 체인에
     * 분기가 없어서** 값이 무시된 것이었다. */
    g_mb.holding[MB_REG_MODEL_FREQ]  = cfg->model_freq;
    g_mb.holding[MB_REG_MODEL_TYPE]  = cfg->model_type;
    g_mb.holding[MB_REG_RUN_MODE]    = cfg->run_mode;
    g_mb.holding[MB_REG_EN_ENERGY]   = cfg->energy_ctrl ? 1u : 0u;
    g_mb.holding[MB_REG_EN_MULTI]    = cfg->multi_ctrl  ? 1u : 0u;
    g_mb.holding[MB_REG_EN_SAFTY]    = cfg->f_safty;
    /* B-2 calibration — int16 를 2의 보수 그대로 싣는다 (C-1). */
    g_mb.holding[MB_REG_CAL_VAL]      = (uint16_t)cfg->cal_val;
    g_mb.holding[MB_REG_FREQ_CAL_VAL] = (uint16_t)cfg->freq_cal_val;
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

    /* F-A comm/eth 미러. COMM_MODE·CFG_STAT 는 무조건, staged 9종은 **비-dirty 일
     * 때만** cfg 라이브 값으로 덮는다 — dirty 인 동안 미러가 덮으면 "쓰기 후
     * read-back" 계약이 staging 에서 깨져 마스터가 자기가 쓴 값을 확인할 수 없다. */
    g_mb.holding[MB_REG_COMM_MODE] = cfg->comm_mode;
    g_mb.holding[MB_REG_CFG_STAT]  = s_stg.stat;
    /* F-A capability — **모델 무관 무조건**. 게이트 CAP(0x2A)와 달리 STD 에서도
     * 실어야 한다: F-A 는 두 모델 모두에 있으므로, 안 실으면 소비 측이 F-A 를
     * 지원하는 STD 유닛을 구 펌웨어로 오판한다. */
    g_mb.holding[MB_REG_CFG_CAP]   = MB_REG_CFG_CAP_MAGIC;
    /* B-4 조작 미러 — 실제 모드 상태를 되비춘다(0/1 정규화는 접근자가 보장). */
    g_mb.holding[MB_REG_HORN_CMD]  = app_horn_mode_active();
    for (uint8_t i = 0u; i < (uint8_t)CFG_STG_COUNT; i++) {
        if (cfg_stage_dirty(&s_stg, i) == 0u) {
            g_mb.holding[k_stg_reg[i]] = stg_mirror_val(cfg, i);
        }
    }

    /* 원격 게이트 미러. CAP는 매직 무조건 복원 = capability probe의 신-펌웨어
     * 판별점("read-only는 미러가 덮음" — MODEL_FREQ/TYPE 위와 동형). 0x2D는
     * 예약이라 미러하지 않는다. 조건 없이 함수 말미에 두어야 세 호출처
     * (apply_config RTU 획득 / tick RTU / tick TCP)가 전부 커버되고, 링크 전이의
     * mb_core_init 0-리셋도 같은 tick에 즉시 복원된다.
     *
     * ⚠ STD 는 미러하지 않는다 — 인터록이 없는데 CAP 매직을 실으면 원격기가
     * "이 컨트롤러는 게이트를 지원한다"고 오판한다(A-7의 판별이 정확히 이것).
     * 미러가 없으면 0x2A 는 원격기가 쓴 probe 값 P 가 그대로 남아 구-펌웨어와
     * 같은 판정을 받는다 = 의도한 동작. */
#if defined(MODEL_REMOTE)
    g_mb.holding[MB_REG_REMOTE_CAP]     = MB_REG_REMOTE_CAP_MAGIC;
    g_mb.holding[MB_REG_REMOTE_EN]      = s_ren.state;
    /* 0x2C 는 결번(구 잔여-초). 레벨 스위치는 만료가 없다 — 원격기가 옛 의미로
     * 읽지 않도록 0 으로 고정한다. */
    g_mb.holding[MB_REG_REMOTE_EN_LEFT] = 0u;
#endif
}

/* FC06 write 적용 */
void app_modbus_apply_writes(mb_link_t link)
{
    /* samd20 update_holding_reg(1): one else-if chain per message — commands
     * first (consume-and-clear), then the single config field that differs
     * (clamped, persisted). Chain order preserved verbatim. */
    app_config_t *cfg = app_lcd_cfg();
    uint16_t v;
    bool save = false;

    /* 원격 활성화 게이트 (spec §5.3). 닫혀 있으면 명령 3종은 디스패치 없이
     * 소거하고, STOP만 통과시킨 뒤 return으로 cfg 체인 전체를 건너뛴다.
     *
     * ⚠ 소거는 생략 불가 — 명령 레지스터 0x19~0x1C는 미러 대상이 아니라서
     * (mirror_live 위쪽 전수) 무시만 하면 1이 홀딩에 잔류하고, 게이트가 열린
     * 뒤 아무 FC06이나 도착하는 순간 아래 체인이 그 stale START를 디스패치한다.
     * 값 불문 무조건 0 — 1 이외 값도 잔류물을 남기지 않는다.
     *
     * STOP을 아래 기존 분기에 맡기지 않고 여기 복제하는 이유: cfg 쓰기 거부의
     * 유일한 장치가 이 return이라, STOP을 fall-through 시키려면 return을
     * 포기해야 하고 그러면 "게이트 닫힘 + cfg 반영"이라는 모순이 생긴다.
     *
     * cfg 거부에 별도 조치가 없는 것은 의도 — 체인을 건너뛰면 다음 tick의
     * mirror_live()가 holding을 cfg 값으로 되돌리므로 원격기 read-back이
     * 불일치를 본다 (예외 응답 없음 = samd20 계약 동형).
     *
     * RTU/TCP가 이 함수를 공유하므로 여기 1곳이 양 전송로 전부다.
     *
     * ⚠ REMOTE_EN_GATE_BYPASS는 T-5(LCD 활성화 조작)가 없는 동안의 한시적
     * 벤치 탈출구다. 게이트를 켤 수단이 아직 없어 기본 빌드는 모든 원격 명령을
     * 막고, 그러면 이 repo의 HW 검증이 의존하는 mbpoll 흐름이 죽는다.
     * T-5 머지 시 이 #ifdef와 CMake 옵션을 함께 제거할 것. */
#ifndef REMOTE_EN_GATE_BYPASS
    if (s_ren.state != (uint8_t)REN_ENABLED) {
        /* 벤치 관측용(VR-3): 무엇이 막혔는지 mon에 남긴다. 소거 전에 잡아야 한다.
         * 무음 거부는 "STATUS 무변화"라는 간접 증거만 남겨서, 게이트가 막은 것인지
         * 애초에 요청이 안 온 것인지 컨트롤러 쪽에서 구분할 수 없다.
         * ⚠ mon은 RTU 점유 시 꺼지므로(app_modbus.c apply_config의
         * mon_set_enabled) 이 줄은 ETH 모드에서만 보인다 — VR-3은 TCP로 칠 것. */
        uint8_t blocked = 0u;
        if      (g_mb.holding[MB_REG_RESET] != 0u) { blocked = MB_REG_RESET; }
        else if (g_mb.holding[MB_REG_SEEK]  != 0u) { blocked = MB_REG_SEEK;  }
        else if (g_mb.holding[MB_REG_START] != 0u) { blocked = MB_REG_START; }

        g_mb.holding[MB_REG_RESET] = 0u;
        g_mb.holding[MB_REG_SEEK]  = 0u;
        g_mb.holding[MB_REG_START] = 0u;
        uint8_t stop_passed = (g_mb.holding[MB_REG_STOP] == 1u) ? 1u : 0u;
        if (stop_passed != 0u) {
            app_reg_command(US_CMD_RUN_RELEASE, (uint8_t)US_COMM);
        }
        /* 명령이 걸린 경우에만 찍는다 — cfg 전용 쓰기까지 찍으면 원격기의 주기
         * 파라미터 쓰기(수 초 간격)가 로그를 덮어버린다. cfg 거부는 read-back
         * 미러 복원으로 이미 관측 가능하다(위 주석). */
        if ((blocked != 0u) || (stop_passed != 0u)) {
            mon_printf("[mb] gate closed(state=%u): blocked=0x%02X stop_passed=%u\r\n",
                       (unsigned)s_ren.state, (unsigned)blocked, (unsigned)stop_passed);
        }
        /* STOP도 값 불문 소거 — 디스패치는 ==1일 때만이지만, 소거를 그 안에 두면
         * STOP=2 같은 비-1 write가 영영 잔류해(미러 대상 아님, 아래 체인도 ==1만
         * 매치) FC03 읽기가 유령 pending STOP을 계속 보고한다. */
        g_mb.holding[MB_REG_STOP] = 0u;
        /* F-A: 커밋은 실계 변경이라 게이트 대상이다. 값 불문 소거하되 CFG_STAT 는
         * 건드리지 않는다 — 게이트 거부와 커밋 검증 거부는 다른 층이고, 사유는
         * REMOTE_EN(0x2B)을 읽어 안다. staged 쓰기 자체는 실계 무영향이라
         * 게이트 대상이 아니지만, 이 return 이 스캔 분기도 함께 건너뛴다:
         * 게이트가 닫힌 동안의 staged 편집은 열린 뒤 다시 쓰면 된다. */
        g_mb.holding[MB_REG_CFG_CTRL] = 0u;
        return;
    }
#endif

    if (g_mb.holding[MB_REG_RESET] == 1u) {
        /* app_reg_command 가 app_seek_reset 에 위임: RESET→SEEK 자동 체인 +
         * 물리 OSC 구동 + fault 클리어 (tag hw-revA_fw-stage-seekreset, 물리
         * 구동 `29803ae`). */
        app_reg_command(US_CMD_RESET, (uint8_t)US_COMM);
        g_mb.holding[MB_REG_RESET] = 0u;
    } else if (g_mb.holding[MB_REG_SEEK] == 1u) {
        app_reg_command(US_CMD_SEEK, (uint8_t)US_COMM);   /* SEEK 단발 → app_seek_reset 위임 */
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
    } else if (g_mb.holding[MB_REG_CFG_CTRL] != 0u) {
        /* F-A 커밋/폐기. 🔴 소거는 무조건, 디스패치는 조건부 — 소거를 ==1/==2
         * 안에 두면 CFG_CTRL=7 같은 값이 영구 잔류하고(0x28 은 미러 대상 ✗)
         * 이후 모든 FC03 읽기가 유령 pending 커밋을 보고한다. */
        uint16_t ctrl = g_mb.holding[MB_REG_CFG_CTRL];
        g_mb.holding[MB_REG_CFG_CTRL] = 0u;

        if (ctrl == 1u) {
            /* 커밋이 dirty 를 지우기 전에 스냅샷 — 무엇을 반영할지가 여기에 있다. */
            uint16_t d = s_stg.dirty;
            /* 가동 중 판정 = us_on_status (run OR seek/reset). 가동 중 통신 링크
             * 재초기화를 막는다. */
            if (cfg_stage_commit(&s_stg, link,
                                 app_lcd_measure()->us_on_status) != 0u) {
                stg_apply_to_cfg(cfg, d);
                if ((d & CFG_STG_ETHER_MASK) != 0u) {
                    /* LCD SAVE 와 같은 훅을 재사용 — app_eth_tick 이 dirty 를
                     * consume 해 재적용한다. RTU 는 응답을 blocking 으로 먼저
                     * 보내고 나서 apply 를 부르므로(send → apply 순서, 아래 tick)
                     * 지연이 불필요하다. DG-12 로 ether 커밋은 RTU 로만 도착하니
                     * TCP 응답 유실 시나리오 자체가 없다 — spec §7 의 500ms 지연
                     * 상수는 근거가 사라져 도입하지 않는다(T-1 재확인 결과). */
                    app_lcd_hook_ether_apply(cfg->comm_mode, cfg->ether_ip,
                                             cfg->ether_nm, cfg->ether_gw);
                }
                /* serial 그룹에는 즉시 재초기화가 **일어나지 않는다** — 그리고
                 * 그것이 맞다. DG-12 가 serial 커밋을 RTU 에서 거부하므로 커밋은
                 * TCP 로만 오고, TCP 분기는 comm_mode != SERIAL 일 때만 도는데,
                 * 그때 apply_config()는 (want==0 && !owned) 로 조기 반환한다.
                 * 즉 재설정할 살아있는 RTU 링크가 애초에 없다. 새 speed/parity 는
                 * cfg·FRAM 에 들어가고 **다음에 SERIAL 로 전환할 때** 적용된다.
                 * (구 주석은 "다음 tick 이 재초기화한다"고 단언했는데 도달 불가
                 * 경로였다 — 2026-09-04 리뷰 지적, spec §5.6 U-1 행도 같은 오해.) */
                save = true;
            }
        } else if (ctrl == 2u) {
            cfg_stage_discard(&s_stg);
        }
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
        /* C-2 (2026-08-30 요구사항): 0/1 정규화.
         *
         * ⚠ samd20 이탈이다 — 원본 comm 경로는 as-is 저장이었고(main.c:4533)
         * 이 포트도 그걸 의식적으로 충실 복제하고 있었다. 사용자 승인 후 변경.
         * 이탈을 받아들인 이유: **LCD 경로는 이미 정규화한다**
         * (app_lcd_input.c:518 `(data16 == 1) ? 1 : 0`). 즉 두 편집 경로가
         * 갈려 있었고, 이 변경은 "원격에만 새 규칙을 발명"하는 것이 아니라
         * 유일하게 어긋나 있던 Modbus 를 LCD 에 맞추는 쪽이다.
         *
         * 기능 동작은 불변 — 소비자(weld trigger FSM)는 != 0 판정이다.
         * 달라지는 것은 read-back 값·FRAM 저장값·DISP_SAFTY 로 보내는 값. */
        uint8_t sf = (g_mb.holding[MB_REG_EN_SAFTY] == 1u) ? 1u : 0u;
        if (sf != cfg->f_safty) {
            cfg->f_safty = sf;
            dgus_write_u16(DISP_SAFTY, cfg->f_safty);
            save = true;
        }
        /* 정규화 결과가 같으면(예: 이미 1인데 5 를 씀) 아무것도 하지 않는다.
         * 다음 미러가 holding 을 1 로 되돌리므로 read-back 은 정규화를 보고,
         * 불필요한 전체맵 FRAM 쓰기를 피한다. */
    } else if (g_mb.holding[MB_REG_HORN_CMD] != app_horn_mode_active()) {
        /* B-4 조작. cfg 가 아니라 비영속 RAM 상태라 save 하지 않는다 —
         * "재부팅 시 소실"은 설계이지 누락이 아니다(원격기에도 그렇게 알렸다).
         * 모드를 켜는 것만으로는 아무것도 움직이지 않는다: 솔레노이드를 실제로
         * 토글하는 것은 기계 앞 조작자의 양손 START 다. 그래서 요구사항이 이것을
         * "일반 설정과 같은 급"으로 분류했다.
         * ⚠ 전이 시 솔레노이드는 무조건 OFF 된다(app_horn_set_mode 안, legacy
         * 3459/3468). horn 모드가 켜지면 모든 소스의 START 가 차단되며, 그
         * 사실은 STATUS 의 HORN 비트로 원격에서 읽힌다. */
        app_horn_set_mode(g_mb.holding[MB_REG_HORN_CMD] != 0u);
    } else if (g_mb.holding[MB_REG_MODEL_FREQ] != cfg->model_freq) {
        /* B-5 모델 주파수. LCD 편집 경로(app_lcd_input.c:447-450)와 **정확히
         * 동형**: cfg 설정 + 모델명 문자열 갱신이 전부다. sys_mode·런페이지·
         * 출력바 임계(ref_lv_*)는 여기서 재파생하지 않는다 — LCD 도 그렇고,
         * 다음 app_lcd_init_mode()(부팅 / SYS_PIC_NOW)에서 갱신된다.
         * 범위 클램프 없음: LCD 에 없는 규칙을 원격에만 발명하지 않는다(사용자
         * 결정). 범위 밖 값은 안전하게 퇴화한다 — send_model_str 은 switch+
         * default(배열 인덱싱 ✗), run_page/ref_lv_* 도 else 분기를 갖는다. */
        cfg->model_freq = (uint8_t)g_mb.holding[MB_REG_MODEL_FREQ];
        app_lcd_send_model_str(cfg->model_freq, cfg->model_type);
        save = true;
    } else if (g_mb.holding[MB_REG_MODEL_TYPE] != cfg->model_type) {
        /* B-5 모델 타입. 위와 동형이나 🔴 **부작용이 하나 더 있다**:
         * PC11 의 의미가 model_type 으로 뒤바뀐다(app_input_fsm.c:46-60).
         *   <=1 (hand/multi) -> PC11 = B_SEEK (active-LOW)
         *   ==2 (std)        -> PC11 = EMSW  (active-HIGH 레벨추종)
         * 따라서 이 쓰기 하나가
         *   0/1 -> 2 : PC11 이 HIGH 면 즉시 E-stop 진입 + SOL 강제 OFF
         *   2 -> 0/1 : E-stop 활성 중이면 s_estop_active 가 0 으로 클리어
         * 를 일으킨다. **가드를 두지 않는 것은 사용자 결정**이다(2026-09-04):
         * LCD 편집 경로에도 같은 가드가 없어(app_lcd_input.c:452 무조건 대입)
         * 기계 앞의 조작자는 이미 같은 일을 할 수 있고, 원격에만 새 규칙을
         * 만들지 않는다는 이 저장소 원칙과 일관된다.
         * ⚠ 남는 차이: 원격 조작자는 기계 앞에 없을 수 있다. 거부가 필요해지면
         * app_estop_active() || us_on_status 로 막는 것이 그 자리다. */
        cfg->model_type = (uint8_t)g_mb.holding[MB_REG_MODEL_TYPE];
        app_lcd_send_model_str(cfg->model_freq, cfg->model_type);
        save = true;
    } else if (g_mb.holding[MB_REG_CAL_VAL] != (uint16_t)cfg->cal_val) {
        /* 클램프된 쓰기는 다음 미러가 되돌리므로 이 체인을 재발화시키지 않는다
         * (기존 클램프 분기들과 같은 형태). 원격기는 read-back 으로 클램프를 본다. */
        cfg->cal_val = cfg_cal_from_wire(g_mb.holding[MB_REG_CAL_VAL]);
        save = true;
    } else if (g_mb.holding[MB_REG_FREQ_CAL_VAL] != (uint16_t)cfg->freq_cal_val) {
        cfg->freq_cal_val = cfg_cal_from_wire(g_mb.holding[MB_REG_FREQ_CAL_VAL]);
        save = true;
    } else if ((g_mb.holding[MB_REG_WORK_CNTL] == 0u) &&
               (cfg->work_cnt != 0u)) {
        /* CNTL=0 write = work counter reset (samd20 main.c:4539: cfg + FRAM +
         * LCD refresh).
         *
         * ⚠ samd20 이탈(사용자 승인 2026-09-04): 원본은 하위 워드만 비교했고 이
         * 포트도 그것을 충실 복제했는데, `work_cnt` 는 uint32_t 라 **65536 의
         * 배수일 때 `(uint16_t)work_cnt == 0` 이 되어 리셋이 조용히 무시된다.**
         * 조작자가 WORK_CNTL=0 을 써도 아무 일도 안 일어나고 에러도 안 난다.
         * 용접기 수명 동안 사이클 65536 회는 충분히 도달하므로 이론적이지 않다.
         * LCD 경로는 원래부터 32비트 전체를 비교한다(app_lcd_input.c:385) —
         * 여기서도 그렇게 맞춘다. 거동 차이는 **65536 의 배수라는 한 점에서만**
         * 생기고(다른 모든 값에서 두 비교는 일치한다), 그 변화는
         * "조용히 실패 → 정상 동작" 방향이다. */
        cfg->work_cnt = 0u;
        app_lcd_set_work_cnt(0u);
        save = true;
    } else {
        /* staged 스캔 — 예약 영역 쓰기를 staging 으로 흡수한다.
         *
         * 🔴 전수 비교(holding != 기대값)로 하면 **stale 미러를 staged 편집으로
         * 오인한다**: mirror_live()는 tick 말미에 돌므로, LCD 나 DHCP 가
         * cfg->ether_* 를 바꾼 직후 한 iteration 동안 holding 은 옛값이다. 그때
         * 아무 FC06 이나 도착하면 스캔이 **옛값**을 staged 로 잡고, 이후 커밋이
         * 그것을 cfg·FRAM 에 되써서 조작자의 LCD 변경을 조용히 되돌린다.
         * 그래서 "마스터가 이번에 실제로 쓴 주소"만 본다 — 코어가 기록해 준다.
         * mb_core_decode/mb_write_reg 의 거동은 그대로다(관측값 1개 추가). */
        for (uint8_t i = 0u; i < (uint8_t)CFG_STG_COUNT; i++) {
            if ((uint16_t)k_stg_reg[i] == g_mb.last_write_addr) {
                cfg_stage_write(&s_stg, i, g_mb.holding[k_stg_reg[i]],
                                sys_tick_get_ms());
                break;
            }
        }
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
        cfg_stage_discard(&s_stg);  /* 링크 전이 = staging 무조건 폐기 */
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
#ifdef REMOTE_EN_GATE_BYPASS
    /* 우회 빌드임을 부팅 로그에 남긴다 — 플래그가 출하 빌드에 실수로 남는 것이
     * 이 탈출구의 유일한 새 위험이므로, 조용히 지나가게 두지 않는다. */
    mon_printf("[mb] *** REMOTE ENABLE GATE BYPASSED — 벤치 전용 빌드 ***\r\n");
#endif
    /* 게이트는 apply_config()의 첫 mirror_live()보다 먼저 초기화 — 부팅 첫 미러가
     * 쓰레기 대신 DISABLED/0을 싣도록. */
#if defined(MODEL_REMOTE)
    remote_en_fsm_init();
    s_ren.state = (uint8_t)REN_DISABLED;
#else
    /* STD 는 FSM 을 아예 호출하지 않는다 — init 까지 빼야 링커가 순수 모듈을
     * 통째로 버린다(호스트 테스트는 모델과 무관하게 계속 돈다). */
    s_ren.state = (uint8_t)REN_ENABLED;
#endif
    cfg_stage_init(&s_stg);
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
    /* 게이트는 분기 밖 첫 문장 — RTU 점유/TCP/미점유 어디로 빠지든 시간이 흐르고
     * 만료돼야 한다 (spec §6). 이후 같은 tick의 mirror_live()가 최신 상태를 싣는다. */
    remote_en_step();
    /* staging 타임아웃 — 분기 밖. 어느 경로로 빠지든 만료돼야 한다. */
    cfg_stage_tick(&s_stg, sys_tick_get_ms());
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
                app_modbus_apply_writes(MB_LINK_RTU); /* samd20: update_holding_reg(1) on FC06 */
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
            cfg_stage_discard(&s_stg);  /* 링크 전이 = staging 무조건 폐기 */
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
