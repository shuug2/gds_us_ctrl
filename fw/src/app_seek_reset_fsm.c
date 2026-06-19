/* fw/src/app_seek_reset_fsm.c — SEEK/RESET FSM core. HAL-free. */
#include "app_seek_reset_fsm.h"
#include <string.h>

static uint8_t  s_state;
static uint16_t s_elapsed;   /* 진입 후 경과 (10ms/tick); SR_TICKS와 직접 비교 */

void seek_reset_fsm_init(void)
{
    s_state   = SR_IDLE;
    s_elapsed = 0u;
}

uint8_t seek_reset_fsm_state(void)
{
    return s_state;
}

void seek_reset_fsm_step(const seek_reset_in_t *in, seek_reset_out_t *out)
{
    memset(out, 0, sizeof(*out));

    switch (s_state) {
    case SR_IDLE:
        if (in->run_active) {
            /* RUN 직교: RUN 중 SEEK/RESET 명령 무시 (spec §3.4). */
        } else if (in->cmd == SR_CMD_RESET) {
            s_state           = SR_RESET;
            s_elapsed         = 0u;
            out->reset_signal = 1u;
            out->reset_icon   = 1u;       /* on 엣지 */
        } else if (in->cmd == SR_CMD_SEEK) {
            s_state           = SR_SEEK;
            s_elapsed         = 0u;
            out->seek_signal  = 1u;
            out->seek_icon    = 1u;       /* on 엣지 */
        }
        break;

    case SR_RESET:                        /* cmd 무시 (busy) */
        /* 포화 가드: SR_TICKS=50 고정이라 s_elapsed는 최대 50에서 전이 — 현재는
         * 미발동이나 config-driven 비교로 확장될 때 대비해 weld 패턴 유지
         * (cpp-review Minor 1). SR_SEEK도 동일. */
        if (s_elapsed < 0xFFFFu) {
            s_elapsed++;
        }
        if (s_elapsed >= SR_TICKS) {      /* 500ms 경과 → SEEK 자동 체인 (samd20 5395-5396) */
            out->reset_icon_off = 1u;     /* reset_signal=0 (memset), off 엣지 */
            out->seek_signal    = 1u;
            out->seek_icon      = 1u;     /* on 엣지 */
            s_state             = SR_SEEK;
            s_elapsed           = 0u;
        } else {
            out->reset_signal = 1u;       /* 레벨 유지 */
        }
        break;

    case SR_SEEK:                         /* cmd 무시 (busy) */
        if (s_elapsed < 0xFFFFu) {
            s_elapsed++;
        }
        if (s_elapsed >= SR_TICKS) {      /* 500ms 경과 → 자동 해제 (samd20 5403-5407) */
            out->seek_icon_off = 1u;      /* seek_signal=0 (memset), off 엣지 */
            s_state            = SR_IDLE;
            s_elapsed          = 0u;
        } else {
            out->seek_signal = 1u;        /* 레벨 유지 */
        }
        break;

    default:
        /* unreachable (s_state는 항상 SR_*); fail-safe (weld LOW-2 패턴). */
        s_state = SR_IDLE;
        break;
    }

    out->state = s_state;
}
