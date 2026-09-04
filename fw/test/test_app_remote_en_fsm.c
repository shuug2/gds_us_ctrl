/* fw/test/test_app_remote_en_fsm.c — host unit tests, 원격 활성화 게이트 순수 FSM.
 *
 * 계약 = 요구사항 2026-08-30 §요청 A (물리 인터록). 2026-08-15 spec의 "LCD 조작 +
 * 창 만료" 의미론은 폐기됐다 — 스위치는 상태가 눈에 보이므로 "켜고 잊는다" 전제가
 * 사라지고(A-3), 레벨에 만료를 얹으면 조작자가 "스위치는 켜 있는데 왜 안 되지"를
 * 겪는다. legacy 대응물 없음(신규 기능).
 *
 * 핵심 불변식 — 해제 사유(링크 침묵·E-STOP)는 **래치**되고, 스위치를 껐다 켜는
 * 재무장으로만 풀린다. 자동 복귀를 허용하면 E-STOP이 풀리는 순간 사람이 없어도
 * 원격 기동이 되살아난다. */
#include <stdio.h>
#include <stdint.h>
#include "app_remote_en_fsm.h"
#include "app_modbus_core.h"

static int failures = 0;
#define CHECK_EQ(expr, expected) do {                                       \
    unsigned long a_ = (unsigned long)(expr);                              \
    unsigned long e_ = (unsigned long)(expected);                          \
    if (a_ != e_) {                                                         \
        printf("FAIL %s:%d  %s = %lu, expected %lu\n",                      \
               __FILE__, __LINE__, #expr, a_, e_);                          \
        failures++;                                                         \
    }                                                                       \
} while (0)

#define SILENCE_MS  (REMOTE_EN_LINK_SILENCE_S * 1000u)

/* 링크가 살아 있는 입력 한 세트 (요청 스탬프 = 현재). */
static void mk(remote_en_in_t *in, uint32_t now, uint8_t sw, uint8_t estop)
{
    in->now_ms      = now;
    in->sw          = sw;
    in->last_req_ms = now;
    in->req_valid   = 1u;
    in->estop       = estop;
}

/* 부팅 = DISABLED */
static void test_boot_disabled(void)
{
    remote_en_in_t in; remote_en_out_t out;
    remote_en_fsm_init();
    mk(&in, 1000u, 0u, 0u);
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state, REN_DISABLED);
}

/* 스위치 ON = 허용. 별도 조작 없이 레벨만으로 열린다 (A-3). */
static void test_switch_on_enables(void)
{
    remote_en_in_t in; remote_en_out_t out;
    remote_en_fsm_init();
    mk(&in, 1000u, 1u, 0u);
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state, REN_ENABLED);
}

/* 스위치 OFF = 즉시 불허 */
static void test_switch_off_disables(void)
{
    remote_en_in_t in; remote_en_out_t out;
    remote_en_fsm_init();
    mk(&in, 1000u, 1u, 0u);
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state, REN_ENABLED);

    mk(&in, 2000u, 0u, 0u);
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state, REN_DISABLED);
}

/* 만료 없음 (A-3) — 링크만 살아 있으면 몇 시간이 지나도 열려 있다.
 * 옛 창(600s)이 남아 있으면 이 테스트가 잡는다. */
static void test_no_window_expiry(void)
{
    remote_en_in_t in; remote_en_out_t out;
    remote_en_fsm_init();
    mk(&in, 1000u, 1u, 0u);
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state, REN_ENABLED);

    /* 2시간을 1분 간격으로 흘려보낸다 (요청은 계속 도착) */
    for (uint32_t t = 1000u; t <= 1000u + (7200u * 1000u); t += 60000u) {
        mk(&in, t, 1u, 0u);
        remote_en_fsm_step(&in, &out);
    }
    CHECK_EQ(out.state, REN_ENABLED);
}

/* E-STOP 레벨 활성 중에는 스위치가 켜져 있어도 열리지 않는다 (A-5) */
static void test_estop_level_blocks_enable(void)
{
    remote_en_in_t in; remote_en_out_t out;
    remote_en_fsm_init();
    mk(&in, 1000u, 1u, 1u);
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state, REN_DIS_ESTOP);
}

/* E-STOP 해제 사유는 래치된다 — E-STOP이 풀려도 스위치가 계속 ON이면 닫힌 채다.
 * 자동 복귀를 허용하면 사람 없이 원격 기동이 되살아난다. */
static void test_estop_latches_after_clear(void)
{
    remote_en_in_t in; remote_en_out_t out;
    remote_en_fsm_init();
    mk(&in, 1000u, 1u, 0u);
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state, REN_ENABLED);

    mk(&in, 2000u, 1u, 1u);            /* E-STOP 눌림 */
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state, REN_DIS_ESTOP);

    mk(&in, 3000u, 1u, 0u);            /* E-STOP 풀림, 스위치는 그대로 ON */
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state, REN_DIS_ESTOP);
}

/* 재무장은 스위치를 껐다 켜는 것뿐 — 그것이 "사람이 기기 앞에 있다"의 갱신이다 */
static void test_switch_cycle_rearms(void)
{
    remote_en_in_t in; remote_en_out_t out;
    remote_en_fsm_init();
    mk(&in, 1000u, 1u, 1u);
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state, REN_DIS_ESTOP);

    mk(&in, 2000u, 0u, 0u);            /* 스위치 OFF = 래치 해제 */
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state, REN_DISABLED);

    mk(&in, 3000u, 1u, 0u);            /* 다시 ON */
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state, REN_ENABLED);
}

/* 침묵 타이머는 활성화 이전 스탬프로 무장하지 않는다 — 그러지 않으면
 * "스위치는 켰는데 원격기가 아직 안 붙음"이 임계 만에 DIS_LINK가 된다. */
static void test_silence_unarmed_stale_req(void)
{
    remote_en_in_t in; remote_en_out_t out;
    remote_en_fsm_init();

    in.now_ms = 100000u; in.sw = 1u; in.estop = 0u;
    in.last_req_ms = 1000u;   /* 활성화보다 한참 전 */
    in.req_valid   = 1u;
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state, REN_ENABLED);

    in.now_ms = 100000u + SILENCE_MS + 5000u;
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state, REN_ENABLED);   /* 무장 전이므로 침묵으로 안 죽는다 */
}

/* 무장 후 링크가 끊기면 해제 (A-4) */
static void test_silence_armed_link_loss(void)
{
    remote_en_in_t in; remote_en_out_t out;
    remote_en_fsm_init();
    mk(&in, 10000u, 1u, 0u);
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state, REN_ENABLED);

    mk(&in, 11000u, 1u, 0u);            /* 요청 도착 → 무장 */
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state, REN_ENABLED);

    in.now_ms = 11000u + SILENCE_MS;    /* last_req_ms는 11000 고정 */
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state, REN_DIS_LINK);
}

/* 임계 미만의 짧은 갭은 살아남는다 */
static void test_silence_brief_gap_survives(void)
{
    remote_en_in_t in; remote_en_out_t out;
    remote_en_fsm_init();
    mk(&in, 10000u, 1u, 0u);
    remote_en_fsm_step(&in, &out);
    mk(&in, 11000u, 1u, 0u);
    remote_en_fsm_step(&in, &out);

    in.now_ms = 11000u + SILENCE_MS - 1u;
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state, REN_ENABLED);
}

/* 🔴 링크 해제는 **래치하지 않는다** — 통신이 돌아오면 스스로 복귀한다.
 * 래치했더니 자기교착이었다: TCP 소켓 1개 + stale ESTABLISHED 자가치유 실측
 * ~20초 > 침묵 임계 10초 → 원격기가 재접속할 때마다 사람이 키를 껐다 켜야 했다.
 * 스위치가 ON 인 한 "사람이 앞에 있다"는 전제는 깨지지 않으므로 안전하다. */
static void test_link_loss_auto_recovers(void)
{
    remote_en_in_t in; remote_en_out_t out;
    remote_en_fsm_init();
    mk(&in, 10000u, 1u, 0u);
    remote_en_fsm_step(&in, &out);
    mk(&in, 11000u, 1u, 0u);
    remote_en_fsm_step(&in, &out);
    in.now_ms = 11000u + SILENCE_MS;
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state, REN_DIS_LINK);

    /* 아직 침묵이면 닫힌 채 유지 */
    in.now_ms = 11000u + SILENCE_MS + 3000u;
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state, REN_DIS_LINK);

    /* 요청 재개 → 스위치를 만지지 않아도 복귀 */
    mk(&in, 11000u + SILENCE_MS + 5000u, 1u, 0u);
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state, REN_ENABLED);

    /* 복귀 후 다시 침묵하면 다시 닫힌다 (무장이 유지돼야 성립) */
    in.now_ms = 11000u + SILENCE_MS + 5000u + SILENCE_MS;
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state, REN_DIS_LINK);
}

/* E-STOP 래치는 링크와 달리 유지된다 — 트래픽이 계속 와도 안 열린다.
 * 이 대비가 무너지면 안전 이벤트가 통신 재개만으로 지워진다. */
static void test_estop_latch_survives_traffic(void)
{
    remote_en_in_t in; remote_en_out_t out;
    remote_en_fsm_init();
    mk(&in, 1000u, 1u, 0u);
    remote_en_fsm_step(&in, &out);
    mk(&in, 2000u, 1u, 1u);            /* E-STOP */
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state, REN_DIS_ESTOP);

    for (uint32_t t = 3000u; t <= 30000u; t += 1000u) {
        mk(&in, t, 1u, 0u);            /* 트래픽 정상, E-STOP 해제됨 */
        remote_en_fsm_step(&in, &out);
    }
    CHECK_EQ(out.state, REN_DIS_ESTOP);   /* 그래도 닫힌 채 */
}

/* u32 랩 근처에서도 침묵 판정이 정상 (elapsed 형태 비교) */
static void test_wrap_safe_silence(void)
{
    remote_en_in_t in; remote_en_out_t out;
    remote_en_fsm_init();
    uint32_t base = 0xFFFFF000u;

    mk(&in, base, 1u, 0u);
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state, REN_ENABLED);

    mk(&in, base + 1000u, 1u, 0u);       /* 무장 */
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state, REN_ENABLED);

    in.now_ms = base + 1000u + SILENCE_MS;   /* 랩을 넘어간다 */
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state, REN_DIS_LINK);
}

/* wire 값 계약 — 원격기가 이미 알고 있는 값이라 재번호 금지 */
static void test_wire_values_match_contract(void)
{
    CHECK_EQ((unsigned)REN_DISABLED,  MB_REMOTE_EN_DISABLED);
    CHECK_EQ((unsigned)REN_ENABLED,   MB_REMOTE_EN_ENABLED);
    CHECK_EQ((unsigned)REN_DIS_LINK,  MB_REMOTE_EN_DIS_LINK);
    CHECK_EQ((unsigned)REN_DIS_ESTOP, MB_REMOTE_EN_DIS_ESTOP);
}

int main(void) {
    test_boot_disabled();
    test_switch_on_enables();
    test_switch_off_disables();
    test_no_window_expiry();
    test_estop_level_blocks_enable();
    test_estop_latches_after_clear();
    test_switch_cycle_rearms();
    test_silence_unarmed_stale_req();
    test_silence_armed_link_loss();
    test_silence_brief_gap_survives();
    test_link_loss_auto_recovers();
    test_estop_latch_survives_traffic();
    test_wrap_safe_silence();
    test_wire_values_match_contract();
    if (failures) { printf("%d check(s) FAILED\n", failures); return 1; }
    printf("app_remote_en_fsm: all tests passed\n");
    return 0;
}
