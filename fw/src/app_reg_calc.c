/* fw/src/app_reg_calc.c — pure regulation compute (HAL-free, host-testable).
 * Faithful port of the ATmega16 transfer functions. Evidence:
 * docs/superpowers/analysis/2026-05-31-m16-regulation-core-verified.md §3b/§3c/§4.3. */
#include "app_reg_calc.h"

/* §4.3 lookup_table[24], flash byte 0x58, byte-for-byte. Strictly decreasing
 * idx 0..20; idx 21..23 (0x0004,0x0004,0x0054) present but never compared. */
const uint16_t reg_lookup_table[24] = {
    0x03FF, 0x03CC, 0x0399, 0x0366, 0x0333, 0x0300, 0x02CD, 0x029A,
    0x0267, 0x0234, 0x0201, 0x01CE, 0x01B9, 0x0168, 0x0135, 0x0102,
    0x00CF, 0x009C, 0x0069, 0x0036, 0x000F, 0x0004, 0x0004, 0x0054
};

uint16_t reg_scale(uint16_t in)
{
    if (in >= 1000u) return 1000u;       /* SCALE-04: input ceiling (cpi 0x03E8) */
    if (in < 3u)     return 0u;           /* SCALE-05: input floor (sbiw 0x03; 3 falls through) */
    return (uint16_t)(in * 6u);           /* SCALE-06: ×6 (not >>6), range [18,5994] */
}

uint8_t reg_output_level(uint16_t scaled)
{
    for (uint8_t i = 0u; i < 21u; i++) {
        if (reg_lookup_table[i] < scaled) {   /* C2: strictly-less, FIRST match */
            return i;
        }
    }
    return 21u;                                /* no match -> output off (@0x15B2) */
}

uint16_t reg_ramp_level(uint16_t counter)
{
    /* M16 app_0x1226 rung thresholds (recon :249-258); per-rung level =
     * thermometer fill (g_019F popcount = rung+1) * 128, saturating at 1024
     * (full byte 0xFF) from rung 7. 2026-06-10: no longer used in the output
     * path — disasm proved the ramp is a one-shot BOOT animation on the
     * physically unconnected 7-seg (OSC flags stay 0 during ramp), not an
     * output soft-start. Kept as the verified table reference (host-tested). */
    static const uint16_t thr[10] = {41u,81u,121u,161u,201u,241u,281u,321u,361u,401u};
    static const uint16_t lvl[10] = {128u,256u,384u,512u,640u,768u,896u,1024u,1024u,1024u};
    for (uint8_t i = 0u; i < 10u; i++) {
        if (counter < thr[i]) {
            return lvl[i];
        }
    }
    return 1024u;   /* counter >= 401: full; caller transitions state to 0 */
}

uint8_t reg_on_time_200m(uint32_t run_elapsed_ms)
{
    uint32_t units = run_elapsed_ms / 200u;
    return (units > 200u) ? 200u : (uint8_t)units;
}

/* spec §4.2: 500ms 에너지-단위 윈도우 / REG_TICK_MS(2ms) = 250 샘플. */
#define REG_ENERGY_DIV  250u

uint32_t reg_energy_from_acc(uint32_t acc_energy)
{
    return acc_energy / REG_ENERGY_DIV;
}

reg_energy_outcome_t reg_energy_termination(uint8_t energy_ctrl, uint32_t curr_energy,
                                            uint32_t limit_energy, uint32_t elapsed_ms,
                                            uint16_t limit_out_time)
{
    if (!energy_ctrl)                { return REG_RUN_CONTINUE; }     /* 비-energy → 호출측 on-time ceiling */
    if ((limit_energy != 0u) && (curr_energy >= limit_energy)) { return REG_RUN_STOP_ENERGY; }
    /* limit_energy==0 = 에너지-도달 체크 off (감사 M1; limit_out_time=0=OVTIME off와
     * 동일 의미론 — 0 목표의 즉시 무증상 완료 차단, 런은 OVTIME/30s 안전이 바운드). */
    /* legacy(main.c:5288) us_on_time >= limit_out_time*10 — 0=off 가드 없음(있으면
     * energy 모드가 ceiling을 대체하므로 limit_out_time=0이 never-stop이 됨, advisor).
     * 0이면 elapsed>=0 항상 참 → 즉시 OVTIME(degenerate; config-validation 클램프는 slice4). */
    if (elapsed_ms >= (uint32_t)limit_out_time * OVTIME_SEC_MS) {
        return REG_RUN_FAULT_OVTIME;                                 /* fault (main.c:5288) */
    }
    return REG_RUN_CONTINUE;
}

/* ── 출력파워 그래프 표시 전류/전력 (ch1=소비전류) ─────────────────
 * SAMD20 cal_real_val ADC_CURR (ref/samd20/main.c:416-433) 구조 포팅.
 * 2026-07-05 c 사용자 결정: legacy −37 오프셋 제거 = 순수 비례 표시
 * (OFFSET 0). GAIN은 legacy 관례대로 보드 실측 맞춤값(legacy 원본 4/10도
 * 그 보드 실측 — 주석 이력 /260·/5 참조): RUN 정착 상태 전류계 600mA ↔
 * ch1(legacy 도메인) ≈126 → 표시 59+cal(보드 1)=60 = 0.60A(10mA 단위) 앵커.
 * (같은 날 초기 fit 7/5·−37의 ch1≈65 앵커는 EMA τ400ms 미정착 오측 —
 * RUN 정착 실측 표시 1.4A/실제 0.6A로 판명, 유휴 ch1=29는 양일 동일.)
 * DEADBAND 14 = legacy 실효 게이트(51−37 도메인); 표시 플로어 0.15A, 그
 * 이하는 0 (2026-07-18 사용자 결정 — 구 20은 유휴 v=13+마진 기준 0.21A).
 * 미세 트림 = cal_val(LCD CAL 필드). */
#define REG_CURR_GAIN_NUM   59u   /* rig-fit ×0.468 (samd20 원본 4/10) */
#define REG_CURR_GAIN_DEN   126u
#define REG_CURR_DEADBAND   14    /* legacy 실효 게이트(main.c:420 51−37 도메인) —
                                   * 최소 표시 0.15A (2026-07-18; 구 20=0.21A 플로어).
                                   * ⚠ cal_val≈0 운용 시 유휴 v≈13-14가 경계에 걸림 */
#define REG_CURR_OFFSET     0     /* legacy 'temp_val − 37' 제거 (사용자 결정) */
#define REG_POWER_NUM       22u   /* samd20 ×2.2 */
#define REG_POWER_DEN       10u

uint16_t reg_current_from_adc(uint16_t ch1_avg, int16_t cal_val)
{
    int32_t v = (int32_t)ch1_avg * (int32_t)REG_CURR_GAIN_NUM
              / (int32_t)REG_CURR_GAIN_DEN + (int32_t)cal_val;
    if (v <= (int32_t)REG_CURR_DEADBAND) {
        return 0u;                /* 데드밴드 + 음수 cal_val 언더플로 가드 */
    }
    return (uint16_t)(v - (int32_t)REG_CURR_OFFSET);
}

uint16_t reg_power_from_amp(uint16_t curr_amp)
{
    return (uint16_t)(((uint32_t)curr_amp * REG_POWER_NUM) / REG_POWER_DEN);
}
