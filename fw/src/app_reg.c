/* fw/src/app_reg.c — Stage D regulation core (compute + run gate).
 * Pipeline (verified analysis §3): 2-ch polled ADC -> normalize 12->10b
 * -> ch0 mean-of-10 / ch1 mean-of-50 -> reg_scale(ch0_avg) -> reg_output_level.
 * Run semantics (M16-faithful, 2026-06-10 IPC analysis): one boot warm-up
 * (~4 s, commands ignored, output 0), then RUN = immediate level-follow gate
 * with NO per-START soft-start. Publishes lcd_measure_t; drives NO OSC GPIO
 * (deferred, spec §9 / B-SEAM). */
#include <string.h>
#include "app_reg.h"
#include "app_reg_calc.h"
#include "app_seek_reset.h"   /* app_seek_reset_request, app_seek_reset_active */
#include "app_overload.h"   /* app_overload_active (START 차단) */
#include "app_input.h"      /* app_estop_active (START 차단, E-stop) */
#include "app_horn.h"       /* app_horn_mode_active (START 차단, SYS_HORN) */
#include "adc1.h"
#include "io.h"
#include "board.h"          /* board_osc4 (PB14 OSC4 발진 게이트) */
#include "sys_tick.h"
#include "app_freq_fsm.h"
#ifdef REG_TRACE
#include "mon.h"
#endif

/* Cadence: Timer0 0xF0 -> 16 ct -> ~2.05 ms @8 MHz (cad-C1). Reproduced in ms. */
#define REG_TICK_MS      2u    /* ⚠ app_reg_calc.c REG_ENERGY_DIV(=250)가 이 2ms 누산
                                * cadence에 결합 — 변경 시 divisor 재산정 (cpp-review LOW-1) */
#define REG_ACQ_MS       1u    /* ADC pace: M16 ISR self-re-armed ~417 us/ch (ch0_avg
                                * ~8.3 ms). Both channels per 1 ms -> ch0_avg 10 ms,
                                * ch1_avg 50 ms — closest the ms-grid superloop gets
                                * (the old 1-ch-per-2ms alternation was 40/400 ms).
                                * Budget: 84+12 cyc @ PCLK2/4 ~= 4 us/conversion, so
                                * 2 blocking reads ~= 8 us per 1 ms — negligible. */
#define CH0_SAMPLES      10u   /* ADC-2 */
#define CH1_SAMPLES      50u   /* ADC-3 */
#define ADC_NORM_SHIFT   2u    /* 12-bit -> 10-bit-equiv (spec §10 DP2 first-cut) */
#define REG_RAMP_MS      10u   /* M16 Timer1 0xFFB1 ~10.1 ms warm-up cadence (cad-C8) */
#define RAMP_DONE_COUNT  401u  /* counter >= 401 (0x191) -> state 0 (verified §2.1) */
#define ON_TIME_UNIT_MS  10u   /* limit_on_time unit: x10 ms (samd20 main.c:769) */
/* Absolute on-time SAFETY ceiling. Fires for ANY active ultrasonic run
 * (TOUCH/COMM/REMOTE/CYCLE) in ANY mode, independent of limit_on_time (works
 * even when limit_on_time==0), NOT panel-editable. Transducer runaway backstop
 * (lost release edge, stuck remote command). User decision 2026-06-27:
 * unconditional, all sources incl. weld. */
#define ON_TIME_SAFETY_MS  30000u   /* 30 s */
#define MODEL_TYPE_HAND    0u       /* model_type/sys_mode: 0=hand (limit_on_time gate) */

#ifdef REG_TRACE
#define REG_TRACE_MS     500u  /* slow trace cadence so the mon log stays readable */
#endif

typedef struct {
    uint16_t ch0_avg, ch1_avg;        /* committed means (ch0=10-bit-equiv,
                                         ch1=legacy 2.23V/4×누산 도메인 — acquire 노트) */
    uint16_t adc_scaled_value;        /* reg_scale(ch0_avg) */
    int16_t  cal_val;                 /* config 보정값 (app_reg_tick 주입) — 표시 전류 */
    uint8_t  band;                    /* reg_output_level() 0..21 (held; output deferred) */

    uint32_t ch0_acc; uint16_t ch0_cnt;
    uint32_t ch1_acc; uint16_t ch1_cnt;

    uint8_t  main_state;              /* 1 = boot warm-up, 0 = lookup regulation */
    uint16_t ramp_counter;           /* boot warm-up 0..401, ~every 10 ms, one-shot */
    uint32_t prev_ramp_ms;           /* 10 ms warm-up cadence gate */

    uint8_t  us_run_status;          /* slice 2b: US_IDLE/REMOTE/TOUCH/COMM (FSM owns) */
    uint8_t  us_out_on;              /* USOUT 마지막 구동 레벨 (전이 감지) */
    uint16_t max_power;              /* running peak of sel during the active run */
    uint16_t last_power;             /* peak latched on stop (us_off, samd20 4180) */
    uint16_t max_amp;                /* running peak of curr_amp during the run */
    uint16_t last_amp;               /* amp latched on stop (samd20 last_amp) */
    uint32_t run_start_ms;           /* TOUCH/COMM run start (on-time ceiling base) */
    uint8_t  swallow_start;          /* 1 = next START is the orphaned release of a
                                      * timeout-stopped run (V30 data=0 quirk) */
    uint32_t acc_energy;             /* 전력 적분기 (run-start 리셋; samd20 acc_energy) */
    uint32_t last_energy;            /* run-stop 시 curr_energy 래치 (samd20 last_energy) */
    uint16_t last_freq;              /* run-stop 시 curr_freq 래치 (samd20 us_off last_freq=curr_freq) */
    uint8_t  sr_disp_active;         /* seek/reset 표시 라이브 엣지 추적 (samd20
                                      * bak_reset/seek_status 등가) */
    uint8_t  error_status;           /* ERR_* (OVTIME 등); RESET이 클리어. publish됨 */

    uint32_t prev_acq_ms;
    uint32_t prev_ms;
#ifdef REG_TRACE
    uint32_t trace_ms;
#endif
} reg_state_t;

static reg_state_t   g_reg;
static lcd_measure_t g_measure;

/* USOUT/OSC4 출력 구동 */
void app_reg_hook_us_output(bool on)
{
    io_usout(on);                    /* PB4 active-HIGH = 초음파 출력 enable */
    board_osc4(on);                  /* PB14 OSC4 active-LOW = 초음파 출력 중 LOW */
}

/* 런 자동 정지 공통 처리 */
static void reg_stop_run(uint8_t rs)
{
    /* 자동 정지 공통 (on-time ceiling / energy-reached / OVTIME): 피크 래치 + IDLE.
     * TOUCH 런은 V30 데이터=0 release 페어링 위해 swallow_start 무장 (수동
     * RUN_RELEASE는 별도 — swallow 없음). */
    g_reg.last_power    = g_reg.max_power;
    g_reg.last_amp      = g_reg.max_amp;
    g_reg.last_energy   = g_measure.curr_energy;
    g_reg.last_freq     = g_measure.curr_freq;   /* freq 래치 — last_energy 패턴(max 없음) */
    g_reg.us_run_status = (uint8_t)US_IDLE;
    if (rs == (uint8_t)US_TOUCH) {
        g_reg.swallow_start = 1u;
    }
}

/* 레귤레이션 상태 초기화 */
void app_reg_init(void)
{
    memset(&g_reg, 0, sizeof(g_reg));
    memset(&g_measure, 0, sizeof(g_measure));
    adc1_init();
    g_reg.prev_ms      = sys_tick_get_ms();
    g_reg.prev_acq_ms  = g_reg.prev_ms;
    g_reg.prev_ramp_ms = g_reg.prev_ms;
    /* Boot = IDLE (us_run_status=0 via memset) + one-shot warm-up. M16-faithful:
     * main sets g_main_state=1 exactly once (@0x1B8A); during warm-up the OSC
     * flags stay at their boot zeros and commands are ignored (Timer1 ISR skips
     * app_0x06d2 while state!=0). The per-START soft-start of slice 2a is
     * retired — the M16 never re-enters the ramp (one-way @0x137C). */
    g_reg.main_state = 1u;
}

/* START 수락 가능 여부 쿼리 */
bool app_reg_start_allowed(void)
{
    /* START가 지금 수락될 상태인가 — app_reg_command의 START guard와 동일 조건의
     * 읽기 전용 쿼리 (상태 무변경). slice4 weld 글루가 사이클 진입 게이팅에 사용
     * (블라인드 사이클 차단, spec §4.3). swallow_start는 TOUCH 전용 소비라 조건에서
     * 제외 (US_CYCLE에 무관). */
    return (g_reg.main_state == 0u) &&                     /* boot warm-up 완료 */
           (g_reg.us_run_status == (uint8_t)US_IDLE) &&
           /* SEEK/RESET active 중 START 무시 (spec §3.4). 직교는 새 RUN 시작만
            * 막고, swallow_start 페어링 동기화는 건드리지 않음 (advisor —
            * guard를 if 조건에 합치면 swallow consume도 스킵되는 비대칭 발생). */
           (app_seek_reset_active() == 0u) &&
           /* fault(OVTIME 등) 중 새 START 막음 — RESET으로 클리어해야 재시작
            * (samd20 SYS_ERROR가 START 무시). */
           (g_reg.error_status == 0u) &&
           /* 과부하 활성 중 START 차단 (SAMD20 SYS_ERROR가 START 막음). */
           (app_overload_active() == 0u) &&
           /* E-stop 활성 중 START 차단 (SAMD20 SYS_ESTOP). 레벨 기반
            * (E-stop 떼면 자동 해제). */
           (app_estop_active() == 0u) &&
           /* SYS_HORN(horn-down) 중 모든 소스 START 차단 — legacy는
            * sys_status!=SYS_RUN이라 런 머시너리 자체가 배제 (main.c:1427). */
           (app_horn_mode_active() == 0u);
}

/* OVTIME fault 세터 */
void app_reg_raise_ovtime(void)
{
    /* weld(US_CYCLE) 에너지 backstop abort용 fault 세터 — 직접런 OVTIME과 같은
     * 비트. 다음 publish(reg_publish_measure)가 measure.error_status로 노출 →
     * app_fault_alarm(부저)/app_lcd_show_error(경고화면)/Modbus STATUS. RESET
     * (US_CMD_RESET)이 클리어. legacy RUN_WELD OVTIME main.c:5292. */
    g_reg.error_status |= ERR_OVTIME;
}

/* US 명령 디스패치 */
void app_reg_command(us_cmd_t cmd, uint8_t src)
{
    switch (cmd) {
    case US_CMD_START:
        /* M16-faithful: commands are ignored during the boot warm-up (Timer1
         * ISR skips the PINA dispatcher app_0x06d2 while g_main_state!=0,
         * disasm @0x041E); after warm-up RUN is an immediate level-follow
         * gate (no per-START ramp). == US_IDLE strict guard for BOTH sources
         * (intentional deviation from samd20's comm !=US_REMOTE takeover,
         * approved spec §4 — REMOTE arbitration is a later slice). */
        if ((g_reg.main_state == 0u) &&
            (g_reg.us_run_status == (uint8_t)US_IDLE)) {
            if ((src == (uint8_t)US_TOUCH) && (g_reg.swallow_start != 0u)) {
                /* V30 RUN button sends data=0 on BOTH edges: after an on-time
                 * ceiling stop, the still-held button's release arrives mapped
                 * as START (input layer sees IDLE). Consume it once instead of
                 * restarting the run. Touch-only: a COMM START is a register
                 * write with no release back-mapping (spec §4). */
                g_reg.swallow_start = 0u;
                break;
            }
            /* guard 4-조건 공용화 (slice4): app_reg_start_allowed()가 단일 진실
             * 원천. 외측 if의 main_state/US_IDLE 재검사는 중복-참(무해).
             * swallow consume은 위에 유지 (advisor 비대칭 — spec §4.3). */
            if (!app_reg_start_allowed()) {
                break;
            }
            g_reg.us_run_status = src;   /* US_TOUCH or US_COMM */
            g_reg.max_power     = 0u;
            g_reg.max_amp       = 0u;    /* samd20 comm START zeroes max_amp too */
            g_reg.run_start_ms  = sys_tick_get_ms();
            /* samd20 zeroes us_on_time_200m at the run-start edge (main.c:4306);
             * the live compute would reach 0 on the first active publish anyway,
             * but zeroing here closes the <=2ms window where a disp read could
             * pair the old time value with the new run status. */
            g_measure.us_on_time_200m = 0u;
            /* 에너지 적분 run-start 리셋 (samd20 main.c:1340/1366/1555 — 전부
             * run-start 엣지). curr_energy 직접 0으로 read-window 닫음. slice2 §2.2. */
            g_reg.acc_energy      = 0u;
            g_measure.curr_energy = 0u;
        }
        break;
    case US_CMD_RUN_RELEASE:
        /* ⚠️ **의도적 samd20 이탈 (2026-08-17, 사용자 결정 B).**
         * 원본은 source-matched stop 이었다 — "a COMM STOP cannot kill a touch
         * run and vice versa" (samd20 main.c:3699/4180 touch, 4405 comm).
         * 그 결과 **원격(Modbus=COMM) STOP 이 패널(TOUCH) 운전을 못 세웠고**,
         * FC06 에코·레지스터 소거까지 정상이라 원격기는 성공한 전송과 구분할
         * 수 없었다(gds_us_remote 실측 2026-08-16,
         * `docs/superpowers/specs/2026-08-16-source-matched-stop.md`).
         * 정지는 방향이 안전 측이므로 **주체와 무관하게** 운전을 내린다.
         *
         * 이 대칭의 대가 두 가지 — 벤치에서 확인할 것:
         *  ① 패널 RUN 버튼 release 가 COMM 운전을 정지시킨다
         *  ② RUN 페이지 이탈(`app_lcd_input_run_key_reanchor`: 에러/E-stop
         *     전환)도 RUN_RELEASE 라 COMM 운전을 정지시킨다
         * 둘 다 "정지시킨다"이므로 안전 측이지만 조작자에겐 새 동작이다.
         *
         * 부수 효과(의도됨): 아래 `else if` 가 이제 **us_run_status == IDLE**
         * 일 때만 도달한다 — 그 분기 주석이 원래 말하던 "arriving while IDLE"
         * 조건과 정확히 일치한다. 구판에서는 COMM 운전 중 TOUCH release 가
         * 이 분기로 새어 swallow_start 를 지웠다. */
        if (g_reg.us_run_status != (uint8_t)US_IDLE) {
            g_reg.last_power    = g_reg.max_power;
            g_reg.last_amp      = g_reg.max_amp;
            g_reg.last_energy   = g_measure.curr_energy;   /* stopped-display 미러 (slice2) */
            g_reg.last_freq     = g_measure.curr_freq;   /* freq 래치 — last_energy 패턴(max 없음) */
            g_reg.us_run_status = (uint8_t)US_IDLE;
        } else if ((src == (uint8_t)US_TOUCH) && (g_reg.swallow_start != 0u)) {
            /* Any touch RUN_RELEASE arriving while IDLE after a ceiling stop
             * resyncs the press/release pairing so the next genuine press is
             * not eaten: legacy data=4 release, or SYS_PIC_NOW re-init (panel
             * reset means the physical release will never arrive). */
            g_reg.swallow_start = 0u;
        }
        break;
    case US_CMD_SEEK:
    case US_CMD_RESET:
        /* SEEK/RESET 효과를 app_seek_reset FSM에 위임 (이전 no-op 교체, spec §4).
         * RUN 중이면 FSM이 run_active 직교로 자체 무시. samd20 comm RESET src
         * quirk + 에러 표시 클리어는 입력 레이어(app_lcd_input.c)/에러 머신.
         * warm-up(main_state) 게이팅 불요 — samd20 충실, spec §3.4 (SEEK/RESET은
         * START과 별도 경로; FSM 자체 타임아웃으로 해제, cpp-review Minor 3). */
        if (cmd == US_CMD_RESET) {
            g_reg.error_status = 0u;   /* samd20 RESET이 error_status 클리어 (main.c:3719) */
        }
        app_seek_reset_request(cmd, src);
        break;
    default:
        /* alien cmd 흡수 (no-op). */
        break;
    }
#ifdef REG_TRACE
    mon_printf("[reg] cmd=%u src=%u run=%u\r\n", (unsigned)cmd, (unsigned)src,
               (unsigned)g_reg.us_run_status);
#endif
}

/* 측정값 포인터 반환 */
const lcd_measure_t *app_reg_measure(void)
{
    return &g_measure;
}

static uint32_t s_ch1_filt_x16;   /* ch1 표시 EMA 상태 (×16 고정소수 — acquire 노트) */

/* ADC 2채널 샘플 누산 */
static void reg_acquire_step(void)
{
    uint16_t s0 = (uint16_t)(adc1_read(ADC1_CH_SENS_OUT) >> ADC_NORM_SHIFT);
    g_reg.ch0_acc += s0;
    if (++g_reg.ch0_cnt >= CH0_SAMPLES) {
        g_reg.ch0_avg = (uint16_t)(g_reg.ch0_acc / CH0_SAMPLES);
        g_reg.ch0_acc = 0u;
        g_reg.ch0_cnt = 0u;
    }

    /* ch1(소비전류): raw 12-bit 누산 → legacy ADC 도메인 평균으로 커밋.
     * SAMD20 ADC = 2.23V ref(VCC/1.48) + 4샘플 누산(divide 없음) → 카운트/V가
     * 우리 12-bit/3.3V의 ×5.92 — ×6 근사(오차 +1.4%, cal/gain 보정에 흡수)로
     * 정합해 cal_real_val 상수(4/10·51·37)를 verbatim 유지 (2026-07-05 벤치:
     * 600mA→~15mV 초소신호 — 구 >>2 10-bit 경로는 1카운트≈128mA로 해상도 불능,
     * 12-bit 50평균×6은 sub-count 유지). 절대 스케일 확정 = 6b/실측 gain. */
    uint16_t s1 = adc1_read(ADC1_CH_OSC);
    g_reg.ch1_acc += s1;
    if (++g_reg.ch1_cnt >= CH1_SAMPLES) {
        uint32_t avg = (g_reg.ch1_acc * 6u) / CH1_SAMPLES;
        /* 표시 안정화 EMA: α=1/2 per 50ms 커밋 → τ≈100ms (사용자 요청
         * 2026-07-08 "업데이트 주기 100ms" — 2026-07-05의 α=1/8·τ≈400ms가
         * 너무 느림). 스파이크 억제는 그만큼 약해짐(50샘플 평균이 1차 방어).
         * ×16 고정소수(잔여분 보존), 유휴↔런 전이 손실 없음(연속 필터).
         * ⚠ 에너지 적분은 이 필터 통과 후 curr_power를 누산(6b 디커플링
         * 이연) — α 증가는 적분 지연도 같이 줄임(legacy 무필터에 근접). */
        int32_t d = (int32_t)(avg << 4) - (int32_t)s_ch1_filt_x16;
        s_ch1_filt_x16 = (uint32_t)((int32_t)s_ch1_filt_x16 + d / 2);
        g_reg.ch1_avg = (uint16_t)(s_ch1_filt_x16 >> 4);
        g_reg.ch1_acc = 0u;
        g_reg.ch1_cnt = 0u;
    }
}

/* 측정값 publish 갱신 */
static void reg_publish_measure(uint32_t now, int16_t freq_cal_val)
{
    /* slice 2b run-gated: curr_power = live setpoint (0 when idle); max_power =
     * running peak during the run; last_power latched on stop (app_reg_command).
     * cycle/freq/energy stay 0 (weld-cycle deferred). */
    uint8_t active = (uint8_t)(g_reg.us_run_status != (uint8_t)US_IDLE);
    /* SEEK/RESET 중 측정값 라이브 표시 (samd20 us_on_status 복원): on-엣지에
     * 피크/에너지 제로화(main.c:4253-4256/4280-4282), off-엣지에 last_* 스냅샷
     * 래치(main.c:4263-4268/4288-4293). last_freq는 legacy 충실로 래치하지
     * 않음(seek off에 freq 래치 없음 — 종료 후 이전 런 주파수로 복귀).
     * RESET→SEEK 자동 체인은 하나의 active 윈도우(체인 경계 latch/re-zero
     * 생략 — legacy와 미세 편차, 피크가 체인 전체에 걸쳐 연속). */
    uint8_t sr = app_seek_reset_active();
    if (sr != g_reg.sr_disp_active) {
        if (sr != 0u) {
            g_reg.max_amp     = 0u;
            g_reg.max_power   = 0u;
            g_reg.last_energy = 0u;
            g_reg.acc_energy      = 0u;
            g_measure.curr_energy = 0u;
        } else {
            g_reg.last_amp    = g_measure.curr_amp;
            g_reg.last_power  = g_measure.curr_power;
            g_reg.last_energy = g_measure.curr_energy;
            g_reg.acc_energy      = 0u;
            g_measure.curr_energy = 0u;
        }
        g_reg.sr_disp_active = sr;
    }
    uint8_t live = (uint8_t)(active || (sr != 0u));
    /* 표시 전류/전력은 ch1(소비전류)에서 — 레귤레이션(ch0/reg_scale)과 분리.
     * SAMD20 cal_real_val 포팅 (spec §3). 피크홀드 비교 소스도 ch1 산출값.
     * 피크 추적/curr_power는 live 게이트 — seek/reset 중에도 갱신 (samd20은
     * ADC 경로 무게이트 추적 + 엣지 제로화, main.c:428-433). */
    uint16_t disp_amp = reg_current_from_adc(g_reg.ch1_avg, g_reg.cal_val);
    g_measure.curr_amp = disp_amp;
    if (live && (disp_amp > g_reg.max_amp)) {
        g_reg.max_amp = disp_amp;
    }
    uint16_t disp_pwr = reg_power_from_amp(disp_amp);
    g_measure.curr_power = live ? disp_pwr : 0u;
    if (live && (disp_pwr > g_reg.max_power)) {
        g_reg.max_power = disp_pwr;
    }
    /* 에너지 적분: active면 curr_power를 acc에 누산(2ms publish cadence) ->
     * curr_energy = acc/250 (samd20 main.c:434-436 구조). idle엔 curr_power=0이라
     * 누산 정지. EXIT 판정은 weld FSM(US_CYCLE)만, 누산/표시는 모든 run 보편. slice2 §5. */
    if (active) {
        g_reg.acc_energy += g_measure.curr_power;
        g_measure.curr_energy = reg_energy_from_acc(g_reg.acc_energy);
    }
    if (active) {
        /* LV_TIME bar: live on-time in 200 ms units from the run-start stamp
         * (samd20 main.c:5223 cadence counter equivalent). TOUCH and COMM runs
         * both stamp run_start_ms at their START edge (app_reg_command); a
         * future REMOTE slice must stamp its own start. When idle the field
         * keeps the last run's final value — samd20 shows the latched
         * last_time when stopped, and disp feeds this one field on both paths
         * (app_lcd_disp.c:183 note). */
        g_measure.us_on_time_200m =
            reg_on_time_200m((uint32_t)(now - g_reg.run_start_ms));
    }
    g_measure.max_power     = g_reg.max_power;
    g_measure.last_power    = g_reg.last_power;
    g_measure.max_amp  = g_reg.max_amp;
    g_measure.last_amp = g_reg.last_amp;
    g_measure.last_energy = g_reg.last_energy;
    /* FREQ_IN: 매 publish에 측정 (SAMD20 calc_freq처럼 run 게이팅 없음 — 무신호면
     * FSM이 0 반환). 표시(VAR_FREQ)/Modbus(MB_REG_DISP_FREQ)는 on?curr:last로
     * 자체 게이팅(기존 배선). slice-B Task 3. */
    g_measure.curr_freq = freq_fsm_compute(freq_cal_val);
    g_measure.last_freq = g_reg.last_freq;
    g_measure.us_run_status = g_reg.us_run_status;
    g_measure.us_on_status  = live;   /* 표시 라이브 게이트 (LCD VAR_ / Modbus DISP_) */
    g_measure.error_status  = g_reg.error_status;   /* OVTIME 등 fault → LCD/Modbus */
    /* USOUT: run 활성(idle 아님)에 출력 enable. 전이에만 hook 구동 (active 재사용). */
    if (active != g_reg.us_out_on) {
        g_reg.us_out_on = active;
        app_reg_hook_us_output(active != 0u);
    }
}

/* 30s 안전 ceiling 검사 */
static void reg_check_safety_ceiling(uint32_t now)
{
    /* (1) Absolute on-time SAFETY ceiling — ON_TIME_SAFETY_MS (30 s). Fires for
     * ANY active ultrasonic run (TOUCH/COMM/REMOTE/CYCLE) in ANY mode,
     * independent of limit_on_time (fires even when limit_on_time==0), NOT
     * panel-editable. Transducer runaway backstop. run_start_ms is stamped at
     * every START edge (incl. US_CYCLE via app_weld), so the 30 s base is valid
     * for all sources. User decision 2026-06-27: unconditional, all incl. weld. */
    uint8_t rs = g_reg.us_run_status;
    if ((rs == (uint8_t)US_TOUCH) || (rs == (uint8_t)US_COMM) ||
        (rs == (uint8_t)US_REMOTE) || (rs == (uint8_t)US_CYCLE)) {
        if ((uint32_t)(now - g_reg.run_start_ms) >= ON_TIME_SAFETY_MS) {
            reg_stop_run(rs);
#ifdef REG_TRACE
            mon_printf("[reg] 30s safety ceiling -> stop\r\n");
#endif
        }
    }
}

/* 런 자동 종료 판정 */
static void reg_check_auto_terminate(uint32_t now, const reg_run_limits_t *lim)
{
    /* (2) 런 자동 종료 — energy 모드면 에너지-도달 정상정지 + OVTIME이 운영
     * ceiling을 대체 (ovtime, legacy main.c:5270 분기; REMOTE는 slice-D가 소스
     * 추가). 비-energy면 legacy limit_on_time ceiling — slice-D 이중화 결정
     * (2026-06-27, samd20 main.c:5296-faithful): HAND 모드의 COMM/REMOTE만,
     * NOT TOUCH (V30 lost-release 리스크는 위 30 s 안전 ceiling이 커버).
     * US_CYCLE은 양쪽 모두 자연 제외 — WELD 길이는 weld-cycle FSM의
     * limit_delay_time2가 지배(app_weld). limit_*은 매 call cfg 주입(M1) —
     * 패널 편집 즉시 반영(mid-run 포함). */
    uint8_t rs = g_reg.us_run_status;
    if ((rs == (uint8_t)US_TOUCH) || (rs == (uint8_t)US_COMM) ||
        (rs == (uint8_t)US_REMOTE)) {
        uint32_t elapsed = (uint32_t)(now - g_reg.run_start_ms);
        if (lim->energy_ctrl != 0u) {
            reg_energy_outcome_t oc = reg_energy_termination(
                lim->energy_ctrl, g_measure.curr_energy, lim->limit_energy,
                elapsed, lim->limit_out_time);
            if (oc != REG_RUN_CONTINUE) {
                reg_stop_run(rs);
                if (oc == REG_RUN_FAULT_OVTIME) {
                    g_reg.error_status |= ERR_OVTIME;   /* samd20 main.c:5292 */
                }
#ifdef REG_TRACE
                mon_printf("[reg] energy stop oc=%u (e=%lu/%lu t=%lums)\r\n",
                           (unsigned)oc, (unsigned long)g_measure.curr_energy,
                           (unsigned long)lim->limit_energy, (unsigned long)elapsed);
#endif
            }
        } else if ((lim->model_type == MODEL_TYPE_HAND) &&
                   ((rs == (uint8_t)US_COMM) || (rs == (uint8_t)US_REMOTE)) &&
                   (lim->limit_on_time != 0u) &&
                   (elapsed >= (uint32_t)lim->limit_on_time * ON_TIME_UNIT_MS)) {
            reg_stop_run(rs);   /* COMM/REMOTE: no swallow (legacy) */
#ifdef REG_TRACE
            mon_printf("[reg] on-time ceiling (%u x10ms) -> stop\r\n",
                       (unsigned)lim->limit_on_time);
#endif
        }
    }
}

/* 레귤레이션 주기 tick */
void app_reg_tick(const reg_run_limits_t *lim)
{
    uint32_t now = sys_tick_get_ms();
    g_reg.cal_val = lim->cal_val;   /* 표시 전류 보정값 주입 (reg_publish_measure 사용) */

    /* ~10 ms boot warm-up cadence (M16 Timer1 0xFFB1 equiv, cad-C8). M16-faithful:
     * runs exactly once, from boot, unconditionally (ramp counter zeroed only at
     * app_0x1226 entry); one-way handoff to lookup regulation at RAMP_DONE_COUNT
     * (@0x137C). During warm-up the output stays 0 — on the M16 only the
     * physically unconnected 7-seg pattern vars animate (g_019F/A0/A1). */
    if ((uint32_t)(now - g_reg.prev_ramp_ms) >= REG_RAMP_MS) {
        g_reg.prev_ramp_ms = now;
        if (g_reg.main_state == 1u) {
            g_reg.ramp_counter++;
            if (g_reg.ramp_counter >= RAMP_DONE_COUNT) {
                g_reg.main_state = 0u;
            }
        }
    }

    reg_check_safety_ceiling(now);
    reg_check_auto_terminate(now, lim);

    if ((uint32_t)(now - g_reg.prev_acq_ms) >= REG_ACQ_MS) {
        g_reg.prev_acq_ms = now;
        reg_acquire_step();
    }

    if ((uint32_t)(now - g_reg.prev_ms) < REG_TICK_MS) {
        return;
    }
    g_reg.prev_ms = now;

    /* Output setpoint MUX (M16-faithful): boot warm-up and idle force 0; while
     * running the slice-1 scale of the latest ch0_avg applies immediately
     * (no per-START ramp — see app_reg_command). */
    uint16_t sel;
    if ((g_reg.main_state == 1u) ||
        (g_reg.us_run_status == (uint8_t)US_IDLE)) {
        sel = 0u;
    } else {
        sel = reg_scale(g_reg.ch0_avg);
    }
    g_reg.adc_scaled_value = sel;
    g_reg.band             = reg_output_level(sel);

    /* Publish runs only on the ~2 ms gate above: a ceiling/release stop that
     * fires earlier in this same call reaches g_measure up to ~2 ms later —
     * bounded and invisible (disp renders each VP-group at a ~40 ms cadence). */
    reg_publish_measure(now, lim->freq_cal_val);

#ifdef REG_TRACE
    if ((uint32_t)(now - g_reg.trace_ms) >= REG_TRACE_MS) {
        g_reg.trace_ms = now;
        mon_printf("[reg] run=%u st=%u rc=%u ch0=%u ch1=%u sel=%u band=%u\r\n",
                   (unsigned)g_reg.us_run_status, (unsigned)g_reg.main_state,
                   (unsigned)g_reg.ramp_counter, (unsigned)g_reg.ch0_avg,
                   (unsigned)g_reg.ch1_avg, (unsigned)g_reg.adc_scaled_value,
                   (unsigned)g_reg.band);
    }
#endif
}
