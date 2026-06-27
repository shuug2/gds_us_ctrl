/* fw/src/app_osc_init_fsm.c — OSC boot-init FSM core. HAL-free. */
#include "app_osc_init_fsm.h"
#include <string.h>

static uint8_t  s_state;
static uint16_t s_elapsed;     /* 상태 진입 후 경과 (10ms/tick) */
static uint8_t  s_h_debounce;  /* WAIT_H: 연속 H 샘플 카운트 */

void osc_init_fsm_init(void)
{
    s_state      = OSC_WAIT_H;
    s_elapsed    = 0u;
    s_h_debounce = 0u;
}

uint8_t osc_init_fsm_state(void)
{
    return s_state;
}

void osc_init_fsm_step(const osc_init_in_t *in, osc_init_out_t *out)
{
    memset(out, 0, sizeof(*out));

    switch (s_state) {
    case OSC_WAIT_H:                       /* PB12 H(초음파 출력 시작) 대기 + 폴백 */
        if (in->pb12) {
            if (s_h_debounce < 0xFFu) { s_h_debounce++; }
            if (s_h_debounce >= OSC_H_DEBOUNCE) {   /* 연속 H 샘플 → 스파이크 배제 */
                s_state      = OSC_WAIT_L;
                s_elapsed    = 0u;
                s_h_debounce = 0u;
            }
        } else {
            s_h_debounce = 0u;             /* H 끊김 → 디바운스 리셋 */
            if (s_elapsed < 0xFFFFu) { s_elapsed++; }
            if (s_elapsed >= OSC_WAIT_H_TIMEOUT) {  /* 보드 부재/고장 폴백 */
                s_state   = OSC_WAIT_L;
                s_elapsed = 0u;
            }
        }
        break;

    case OSC_WAIT_L:                       /* PB12 L(출력 종료) 대기 + 폴백 */
        if (!in->pb12) {
            s_state   = OSC_GAP;
            s_elapsed = 0u;
        } else {
            if (s_elapsed < 0xFFFFu) { s_elapsed++; }
            if (s_elapsed >= OSC_WAIT_L_TIMEOUT) {  /* 출력 안 떨어짐 폴백 */
                s_state   = OSC_GAP;
                s_elapsed = 0u;
            }
        }
        break;

    case OSC_GAP:                          /* 종료 후 150ms 갭 */
        if (s_elapsed < 0xFFFFu) { s_elapsed++; }
        if (s_elapsed >= OSC_GAP_TICKS) {
            s_state           = OSC_RESET;
            s_elapsed         = 0u;
            out->reset_signal = 1u;        /* RESET 펄스 시작 엣지 */
        }
        break;

    case OSC_RESET:                        /* RESET 펄스 200ms */
        if (s_elapsed < 0xFFFFu) { s_elapsed++; }
        if (s_elapsed >= OSC_RESET_TICKS) {
            s_state          = OSC_SEEK;
            s_elapsed        = 0u;
            out->seek_signal = 1u;         /* SEEK 펄스 시작 엣지 (reset off) */
        } else {
            out->reset_signal = 1u;        /* 레벨 유지 */
        }
        break;

    case OSC_SEEK:                         /* SEEK 펄스 100ms */
        if (s_elapsed < 0xFFFFu) { s_elapsed++; }
        if (s_elapsed >= OSC_SEEK_TICKS) {
            s_state   = OSC_DONE;
            s_elapsed = 0u;                /* 완료 (seek off) */
        } else {
            out->seek_signal = 1u;         /* 레벨 유지 */
        }
        break;

    case OSC_DONE:                         /* terminal: 재실행 없음, 출력 idle */
        break;

    default:
        /* unreachable; fail-safe → 완료 처리 (재초기화 펄스 폭주 방지). */
        s_state = OSC_DONE;
        break;
    }

    out->state = s_state;
}
