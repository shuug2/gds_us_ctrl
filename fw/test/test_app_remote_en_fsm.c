/* fw/test/test_app_remote_en_fsm.c — host unit tests, 원격 활성화 게이트 순수 FSM.
 * spec: docs/superpowers/specs/2026-08-15-remote-enable-gate-design.md §5.2/§10.
 * legacy 대응물 없음(신규 기능) — 충실도 기준은 spec 자체. */
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

#define WINDOW_MS   (REMOTE_EN_WINDOW_S * 1000u)
#define SILENCE_MS  (REMOTE_EN_LINK_SILENCE_S * 1000u)

/* 부팅 = DISABLED, 잔여 0 */
static void test_boot_disabled(void)
{
    remote_en_fsm_init();
    remote_en_in_t  in  = { .now_ms = 0u };
    remote_en_out_t out;
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state,  REN_DISABLED);
    CHECK_EQ(out.left_s, 0u);
}

/* LCD enable → ENABLED, 잔여 = 창 길이 */
static void test_enable_enters_enabled(void)
{
    remote_en_fsm_init();
    remote_en_in_t  in  = { .now_ms = 1000u, .lcd_enable = 1u };
    remote_en_out_t out;
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state,  REN_ENABLED);
    CHECK_EQ(out.left_s, REMOTE_EN_WINDOW_S);
}

/* E-STOP 레벨 활성 중 enable 요청은 거부 (spec §5.2 — 엣지 부재 구멍 차단) */
static void test_enable_refused_while_estop_level(void)
{
    remote_en_fsm_init();
    remote_en_in_t  in  = { .now_ms = 1000u, .lcd_enable = 1u, .estop = 1u };
    remote_en_out_t out;
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state, REN_DISABLED);

    /* E-STOP 해제 후에는 정상 활성 */
    in.now_ms = 2000u; in.estop = 0u; in.lcd_enable = 1u;
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state, REN_ENABLED);
}

/* 창 만료 → DIS_TIMEOUT, 사유는 다음 enable까지 래치 */
static void test_window_expiry_latched(void)
{
    remote_en_fsm_init();
    remote_en_in_t  in  = { .now_ms = 1000u, .lcd_enable = 1u };
    remote_en_out_t out;
    remote_en_fsm_step(&in, &out);

    in.lcd_enable = 0u;
    in.now_ms = 1000u + WINDOW_MS - 1u;
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state, REN_ENABLED);

    in.now_ms = 1000u + WINDOW_MS;
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state,  REN_DIS_TIMEOUT);
    CHECK_EQ(out.left_s, 0u);

    in.now_ms = 1000u + WINDOW_MS + 60000u;   /* 계속 step 해도 사유 유지 */
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state, REN_DIS_TIMEOUT);
}

/* 잔여 초 = ceil, ENABLED 동안 절대 0 아님 + u32 랩 경계에서 정상 동작.
 * 절대 시각 비교(now >= expiry) 구현이면 랩 구간에서 깨진다 (계획 §함정 3). */
static void test_left_arithmetic_boundary_wrap(void)
{
    remote_en_fsm_init();
    remote_en_in_t  in  = { .now_ms = 0u, .lcd_enable = 1u };
    remote_en_out_t out;
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.left_s, REMOTE_EN_WINDOW_S);

    in.lcd_enable = 0u;
    in.now_ms = 1u;                              /* ceil: 599999ms 남음 → 600 */
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.left_s, REMOTE_EN_WINDOW_S);

    in.now_ms = 1000u;                           /* 정확히 1초 경과 → 599 */
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.left_s, REMOTE_EN_WINDOW_S - 1u);

    in.now_ms = WINDOW_MS - 1u;                  /* 1ms 남음 → ceil 1 (0 금지) */
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state,  REN_ENABLED);
    CHECK_EQ(out.left_s, 1u);

    /* u32 랩: enable 시각이 랩 직전 */
    remote_en_fsm_init();
    in.now_ms = 0xFFFFFB00u; in.lcd_enable = 1u;
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state, REN_ENABLED);

    in.lcd_enable = 0u;
    in.now_ms = (uint32_t)(0xFFFFFB00u + WINDOW_MS - 1u);   /* 랩 넘김 */
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state,  REN_ENABLED);
    CHECK_EQ(out.left_s, 1u);

    in.now_ms = (uint32_t)(0xFFFFFB00u + WINDOW_MS);
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state, REN_DIS_TIMEOUT);
}

/* 침묵 미무장: activation 이전 요청 스탬프로는 침묵이 발화하지 않는다.
 * (spec §5.2 보완분 — "LCD에서 켰는데 원격기 미접속 → 10초 만에 DIS_LINK" 금지) */
static void test_silence_unarmed_stale_req(void)
{
    remote_en_fsm_init();
    remote_en_in_t  in  = { .now_ms = 200000u, .lcd_enable = 1u,
                            .last_req_ms = 100u, .req_valid = 1u };
    remote_en_out_t out;
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state, REN_ENABLED);

    in.lcd_enable = 0u;
    in.now_ms = 200000u + SILENCE_MS + 1000u;   /* 임계 초과했지만 미무장 */
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state, REN_ENABLED);
}

/* 무장 후 링크 침묵 → DIS_LINK */
static void test_silence_armed_link_loss(void)
{
    remote_en_fsm_init();
    remote_en_in_t  in  = { .now_ms = 1000u, .lcd_enable = 1u };
    remote_en_out_t out;
    remote_en_fsm_step(&in, &out);

    in.lcd_enable = 0u;
    in.now_ms = 2000u; in.last_req_ms = 2000u; in.req_valid = 1u;  /* 첫 요청 = 무장 */
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state, REN_ENABLED);

    in.now_ms = 2000u + SILENCE_MS - 1u;
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state, REN_ENABLED);

    in.now_ms = 2000u + SILENCE_MS;
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state,  REN_DIS_LINK);
    CHECK_EQ(out.left_s, 0u);
}

/* 임계 미만 순단은 해제하지 않는다 (오탐 방지) */
static void test_silence_brief_gap_survives(void)
{
    remote_en_fsm_init();
    remote_en_in_t  in  = { .now_ms = 1000u, .lcd_enable = 1u };
    remote_en_out_t out;
    remote_en_fsm_step(&in, &out);

    in.lcd_enable = 0u; in.req_valid = 1u;
    uint32_t t = 2000u;
    for (int i = 0; i < 5; i++) {
        in.now_ms = t; in.last_req_ms = t;              /* 요청 도착 */
        remote_en_fsm_step(&in, &out);
        CHECK_EQ(out.state, REN_ENABLED);
        in.now_ms = t + SILENCE_MS - 1000u;             /* 9초 무음 — 임계 미만 */
        remote_en_fsm_step(&in, &out);
        CHECK_EQ(out.state, REN_ENABLED);
        t += SILENCE_MS - 1000u;
    }
}

/* 원격 활동은 창을 연장하지 않는다 (결정 기록 §3.1) */
static void test_activity_does_not_extend_window(void)
{
    remote_en_fsm_init();
    remote_en_in_t  in  = { .now_ms = 1000u, .lcd_enable = 1u };
    remote_en_out_t out;
    remote_en_fsm_step(&in, &out);

    in.lcd_enable = 0u; in.req_valid = 1u;
    for (uint32_t t = 2000u; t < 1000u + WINDOW_MS; t += 1000u) {
        in.now_ms = t; in.last_req_ms = t;              /* 1초마다 계속 요청 */
        remote_en_fsm_step(&in, &out);
        CHECK_EQ(out.state, REN_ENABLED);
    }
    in.now_ms = 1000u + WINDOW_MS; in.last_req_ms = 1000u + WINDOW_MS;
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state, REN_DIS_TIMEOUT);               /* 활동 무관하게 만료 */
}

/* E-STOP 상승 엣지로 해제, 레벨 해제로 자동 부활하지 않는다 (spec §5.2) */
static void test_estop_edge_no_auto_revive(void)
{
    remote_en_fsm_init();
    remote_en_in_t  in  = { .now_ms = 1000u, .lcd_enable = 1u };
    remote_en_out_t out;
    remote_en_fsm_step(&in, &out);

    in.lcd_enable = 0u;
    in.now_ms = 2000u; in.estop = 1u;                   /* 상승 엣지 */
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state,  REN_DIS_ESTOP);
    CHECK_EQ(out.left_s, 0u);

    in.now_ms = 3000u;                                  /* 레벨 유지 */
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state, REN_DIS_ESTOP);

    in.now_ms = 4000u; in.estop = 0u;                   /* 레벨 해제 = 부활 금지 */
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state, REN_DIS_ESTOP);
}

/* LCD 수동 해제 → DIS_LCD */
static void test_lcd_disable(void)
{
    remote_en_fsm_init();
    remote_en_in_t  in  = { .now_ms = 1000u, .lcd_enable = 1u };
    remote_en_out_t out;
    remote_en_fsm_step(&in, &out);

    in.lcd_enable = 0u;
    in.now_ms = 2000u; in.lcd_disable = 1u;
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state,  REN_DIS_LCD);
    CHECK_EQ(out.left_s, 0u);
}

/* 재활성 = 창 갱신 + 사유 래치 해제 + 침묵 재-미무장 */
static void test_reenable_refresh_rearm_latch(void)
{
    remote_en_fsm_init();
    remote_en_in_t  in  = { .now_ms = 1000u, .lcd_enable = 1u };
    remote_en_out_t out;
    remote_en_fsm_step(&in, &out);

    /* 무장 → 침묵 해제까지 진행 */
    in.lcd_enable = 0u; in.req_valid = 1u;
    in.now_ms = 2000u; in.last_req_ms = 2000u;
    remote_en_fsm_step(&in, &out);
    in.now_ms = 2000u + SILENCE_MS;
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state, REN_DIS_LINK);

    /* 재활성: 창 갱신 + 사유 해제 */
    uint32_t t_re = 100000u;
    in.now_ms = t_re; in.lcd_enable = 1u;
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state,  REN_ENABLED);
    CHECK_EQ(out.left_s, REMOTE_EN_WINDOW_S);

    /* 재활성 후 요청이 한 번도 없으면 침묵은 다시 미무장 (stale last_req 무시) */
    in.lcd_enable = 0u;
    in.now_ms = t_re + SILENCE_MS + 1000u;
    remote_en_fsm_step(&in, &out);
    CHECK_EQ(out.state, REN_ENABLED);
}

/* wire 계약: FSM enum과 core.h 매크로 값 일치 (두 헤더 중복 정의 고정).
 * 순수 FSM 헤더는 modbus core에 결합되면 안 되고 core.h는 wire 계약 문서라
 * 중복이 의도적이다 — 어긋나면 0x2B가 거짓말을 하게 되므로 여기서 못박는다. */
static void test_state_codes_match_core(void)
{
    CHECK_EQ(REN_DISABLED,    MB_REMOTE_EN_DISABLED);
    CHECK_EQ(REN_ENABLED,     MB_REMOTE_EN_ENABLED);
    CHECK_EQ(REN_DIS_TIMEOUT, MB_REMOTE_EN_DIS_TIMEOUT);
    CHECK_EQ(REN_DIS_LINK,    MB_REMOTE_EN_DIS_LINK);
    CHECK_EQ(REN_DIS_ESTOP,   MB_REMOTE_EN_DIS_ESTOP);
    CHECK_EQ(REN_DIS_LCD,     MB_REMOTE_EN_DIS_LCD);
    /* probe 매직은 0이 아니어야 한다 — 링크 전이 0-리셋과 구분 불가해진다 */
    CHECK_EQ(MB_REG_REMOTE_CAP_MAGIC != 0u, 1u);
}

int main(void)
{
    test_state_codes_match_core();
    test_boot_disabled();
    test_enable_enters_enabled();
    test_enable_refused_while_estop_level();
    test_window_expiry_latched();
    test_left_arithmetic_boundary_wrap();
    test_silence_unarmed_stale_req();
    test_silence_armed_link_loss();
    test_silence_brief_gap_survives();
    test_activity_does_not_extend_window();
    test_estop_edge_no_auto_revive();
    test_lcd_disable();
    test_reenable_refresh_rearm_latch();
    if (failures) { printf("app_remote_en_fsm: %d FAIL\n", failures); return 1; }
    printf("app_remote_en_fsm: all tests passed\n");
    return 0;
}
