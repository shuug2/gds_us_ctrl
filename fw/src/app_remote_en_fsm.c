/* fw/src/app_remote_en_fsm.c — 원격 활성화 게이트 순수 FSM (spec §5.2). */
#include "app_remote_en_fsm.h"

#define WINDOW_MS   (REMOTE_EN_WINDOW_S * 1000u)
#define SILENCE_MS  (REMOTE_EN_LINK_SILENCE_S * 1000u)

static uint8_t  s_state;
static uint32_t s_enable_ms;
static uint8_t  s_silence_armed;   /* 활성화 후 첫 유효 요청부터 1 */
static uint8_t  s_estop_bak;       /* 엣지 검출용 — 상태 무관 매 step 동기 */

/* 게이트 상태 초기화 */
void remote_en_fsm_init(void)
{
    s_state         = (uint8_t)REN_DISABLED;
    s_enable_ms     = 0u;
    s_silence_armed = 0u;
    s_estop_bak     = 0u;
}

/* 한 step 진행 */
void remote_en_fsm_step(const remote_en_in_t *in, remote_en_out_t *out)
{
    /* (1) E-STOP 상승 엣지. bak는 상태 무관 매 step 동기 — 비활성 구간에서 생긴
     * 엣지가 나중에 stale하게 발화하는 것을 막는다 (app_horn_fsm 규율). */
    uint8_t estop_edge = ((s_estop_bak == 0u) && (in->estop != 0u)) ? 1u : 0u;
    s_estop_bak = (in->estop != 0u) ? 1u : 0u;

    /* (2) 활성 중이면 해제 조건 평가. 우선순위는 같은 tick 다중 발화 시의
     * 결정성 확보용 (LCD 수동 > E-STOP > 창 만료 > 링크 침묵). */
    if (s_state == (uint8_t)REN_ENABLED) {
        uint32_t elapsed = (uint32_t)(in->now_ms - s_enable_ms);

        if (in->lcd_disable != 0u) {
            s_state = (uint8_t)REN_DIS_LCD;
        } else if (estop_edge != 0u) {
            s_state = (uint8_t)REN_DIS_ESTOP;
        } else if (elapsed >= WINDOW_MS) {
            /* 원격 활동은 창을 연장하지 않는다 (결정 기록 §3.1) — 연장 수단은
             * LCD 재활성화뿐이고, 그것은 아래 (3)이 처리한다. */
            s_state = (uint8_t)REN_DIS_TIMEOUT;
        } else {
            /* 침묵 타이머는 활성화 후 첫 유효 요청부터 무장한다. 활성화 이전
             * 스탬프로 무장하면 "LCD에서 켰는데 원격기가 아직 안 붙음 → 임계
             * 만에 DIS_LINK"가 된다 (spec §5.2 보완분).
             * 무장 판정은 랩 안전: 활성화 이전 요청이면 좌변이 언더플로로
             * 거대값이 되어 elapsed보다 커진다. */
            if (s_silence_armed == 0u) {
                if ((in->req_valid != 0u) &&
                    ((uint32_t)(in->last_req_ms - s_enable_ms) <= elapsed)) {
                    s_silence_armed = 1u;
                }
            }
            if ((s_silence_armed != 0u) &&
                ((uint32_t)(in->now_ms - in->last_req_ms) >= SILENCE_MS)) {
                s_state = (uint8_t)REN_DIS_LINK;
            }
        }
    }

    /* (3) enable 이벤트는 마지막에 평가한다. 그래야 ENABLED 중 재조작이
     * "창 갱신 + 침묵 재-미무장 + 해제 사유 래치 해제"로 한 경로에 통합되고,
     * 같은 tick에 해제와 경합해도 사용자의 최신 의사가 이긴다.
     * E-STOP 레벨 활성 중에는 거부 — 엣지가 이미 지나가 영영 안 오는 구멍을
     * 막는다 (spec §5.2). */
    if ((in->lcd_enable != 0u) && (in->estop == 0u)) {
        s_state         = (uint8_t)REN_ENABLED;
        s_enable_ms     = in->now_ms;
        s_silence_armed = 0u;
    }

    /* (4) 출력. left_s는 ceil이라 ENABLED인 동안 1 미만으로 떨어지지 않는다
     * (만료는 위 (2)가 상태로 표현하므로 0은 비활성 전용 값). */
    out->state = s_state;
    if (s_state == (uint8_t)REN_ENABLED) {
        uint32_t left_ms = WINDOW_MS - (uint32_t)(in->now_ms - s_enable_ms);
        out->left_s = (uint16_t)((left_ms + 999u) / 1000u);
    } else {
        out->left_s = 0u;
    }
}
