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
    (void)in;            /* Task 2에서 전이 로직 채움 */
    out->state = s_state;
}
