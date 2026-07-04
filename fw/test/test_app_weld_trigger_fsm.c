/* fw/test/test_app_weld_trigger_fsm.c — slice4 트리거 FSM host tests. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "app_weld_trigger_fsm.h"
#include "app_weld_fsm.h"   /* WELD_* enums */

static int failures = 0;
#define CHECK_EQ(expr, expected) do {                                       \
    unsigned long a_ = (unsigned long)(expr);                               \
    unsigned long e_ = (unsigned long)(expected);                           \
    if (a_ != e_) {                                                         \
        printf("FAIL %s:%d  %s = %lu, expected %lu\n",                      \
               __FILE__, __LINE__, #expr, a_, e_);                          \
        failures++;                                                         \
    }                                                                       \
} while (0)

static weld_trig_in_t mk_in(uint8_t k1, uint8_t k2, uint8_t state)
{
    weld_trig_in_t in;
    memset(&in, 0, sizeof(in));
    in.key1 = k1; in.key2 = k2;
    in.sens_up = 1u; in.sens_dn = 1u;      /* idle = released(HIGH) */
    in.weld_state = state;
    return in;
}

/* 양손: 둘 다 press일 때만 start_pulse; 단독 press는 아님 (main.c:1404). */
static void test_two_hand_start(void)
{
    weld_trigger_fsm_init();
    weld_trig_out_t out;
    weld_trig_in_t in = mk_in(1u, 1u, WELD_READY);
    weld_trigger_fsm_step(&in, &out);  CHECK_EQ(out.start_pulse, 0u);
    in = mk_in(0u, 1u, WELD_READY);
    weld_trigger_fsm_step(&in, &out);  CHECK_EQ(out.start_pulse, 0u);
    in = mk_in(1u, 0u, WELD_READY);
    weld_trigger_fsm_step(&in, &out);  CHECK_EQ(out.start_pulse, 0u);
    in = mk_in(0u, 0u, WELD_READY);
    weld_trigger_fsm_step(&in, &out);  CHECK_EQ(out.start_pulse, 1u);
}

/* in_cycle 재장전: cycle_started 후 양손 유지 -> pulse 없음; READY에서 양손
 * release해야 재장전 (main.c:1219, 1472). 게이팅 실패(= cycle_started 미호출)면
 * 재장전 벌칙 없음. */
static void test_in_cycle_rearm(void)
{
    weld_trigger_fsm_init();
    weld_trig_out_t out;
    weld_trig_in_t in = mk_in(0u, 0u, WELD_READY);
    weld_trigger_fsm_step(&in, &out);
    CHECK_EQ(out.start_pulse, 1u);
    weld_trigger_fsm_cycle_started();          /* 글루: 게이팅 통과 */

    in.weld_state = WELD_CYL1;                 /* 사이클 진행 중 양손 유지 */
    weld_trigger_fsm_step(&in, &out);
    CHECK_EQ(out.start_pulse, 0u);

    in.weld_state = WELD_READY;                /* 완료 복귀, 양손 아직 유지 */
    weld_trigger_fsm_step(&in, &out);
    CHECK_EQ(out.start_pulse, 0u);             /* release 전 재시작 금지 */

    in = mk_in(1u, 1u, WELD_READY);            /* 양손 release -> 재장전 */
    weld_trigger_fsm_step(&in, &out);
    in = mk_in(0u, 0u, WELD_READY);
    weld_trigger_fsm_step(&in, &out);
    CHECK_EQ(out.start_pulse, 1u);

    /* 게이팅 실패 케이스: pulse만 나가고 cycle_started 미호출 -> 다음 step도 pulse */
    weld_trigger_fsm_step(&in, &out);
    CHECK_EQ(out.start_pulse, 1u);
}

/* safety abort: f_safty && CYL1 && 한 손 release (main.c:1484). */
static void test_safety_abort(void)
{
    weld_trigger_fsm_init();
    weld_trig_out_t out;
    weld_trig_in_t in = mk_in(1u, 0u, WELD_CYL1);
    in.f_safty = 1u;
    weld_trigger_fsm_step(&in, &out);  CHECK_EQ(out.safety_abort_pulse, 1u);
    in.f_safty = 0u;                                   /* safety off -> 없음 */
    weld_trigger_fsm_step(&in, &out);  CHECK_EQ(out.safety_abort_pulse, 0u);
    in.f_safty = 1u; in.key1 = 0u;                     /* 양손 유지 -> 없음 */
    weld_trigger_fsm_step(&in, &out);  CHECK_EQ(out.safety_abort_pulse, 0u);
    in.key1 = 1u; in.weld_state = WELD_WELD;           /* CYL1 밖 -> 없음 (legacy 충실) */
    weld_trigger_fsm_step(&in, &out);  CHECK_EQ(out.safety_abort_pulse, 0u);
}

/* 센서 엣지: 1->0 전이에서만 1-shot (main.c:1222-1233). */
static void test_sensor_edges(void)
{
    weld_trigger_fsm_init();
    weld_trig_out_t out;
    weld_trig_in_t in = mk_in(1u, 1u, WELD_READY);
    weld_trigger_fsm_step(&in, &out);
    CHECK_EQ(out.dn_edge, 0u);
    in.sens_dn = 0u;
    weld_trigger_fsm_step(&in, &out);  CHECK_EQ(out.dn_edge, 1u);
    weld_trigger_fsm_step(&in, &out);  CHECK_EQ(out.dn_edge, 0u);   /* 유지 = 재발행 없음 */
    in.sens_dn = 1u;
    weld_trigger_fsm_step(&in, &out);  CHECK_EQ(out.dn_edge, 0u);   /* release 무이벤트 */
    in.sens_up = 0u;
    weld_trigger_fsm_step(&in, &out);  CHECK_EQ(out.up_edge, 1u);
}

int main(void)
{
    test_two_hand_start();
    test_in_cycle_rearm();
    test_safety_abort();
    test_sensor_edges();
    if (failures) { printf("%d FAILURES\n", failures); return 1; }
    printf("app_weld_trigger_fsm: all tests passed\n");
    return 0;
}
