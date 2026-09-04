/* fw/src/app_remote_en_fsm.c — 원격 활성화 게이트 순수 FSM (레벨 스위치). */
#include "app_remote_en_fsm.h"

#define SILENCE_MS  (REMOTE_EN_LINK_SILENCE_S * 1000u)

static uint8_t  s_state;
static uint32_t s_enter_ms;      /* ENABLED 진입 시각 — 침묵 무장 기준선 */
static uint8_t  s_silence_armed; /* 진입 후 첫 유효 요청부터 1 */

/* 게이트 상태 초기화 */
void remote_en_fsm_init(void)
{
    s_state         = (uint8_t)REN_DISABLED;
    s_enter_ms      = 0u;
    s_silence_armed = 0u;
}

/* 한 step 진행 */
void remote_en_fsm_step(const remote_en_in_t *in, remote_en_out_t *out)
{
    /* (1) 스위치 OFF = 불허 + 래치 해제. 사람이 스위치를 내린 것이 곧 "재무장
     * 준비"라, 이 한 줄이 해제 사유 래치의 유일한 청소 경로다 — 덕분에 스위치
     * 엣지를 따로 기억할 상태가 필요 없다. */
    if (in->sw == 0u) {
        s_state         = (uint8_t)REN_DISABLED;
        s_silence_armed = 0u;
        out->state      = s_state;
        return;
    }

    /* (2) 스위치 ON. 래치된 해제 사유가 있으면 그대로 유지 — 위 (1)을 거쳐야만
     * 풀린다. E-STOP이 풀리거나 통신이 돌아왔다고 스스로 열리지 않는다. */
    if ((s_state == (uint8_t)REN_DIS_LINK) || (s_state == (uint8_t)REN_DIS_ESTOP)) {
        out->state = s_state;
        return;
    }

    /* (3) E-STOP은 레벨로 본다 (엣지 ✗). 엣지로 잡으면 "E-STOP이 이미 눌린 채
     * 스위치를 켜는" 순서에서 엣지가 영영 안 와 게이트가 열려버린다. */
    if (in->estop != 0u) {
        s_state    = (uint8_t)REN_DIS_ESTOP;
        out->state = s_state;
        return;
    }

    /* (4) 진입. 만료가 없으므로 여기서 재무장 기준선만 잡는다 (A-3). */
    if (s_state != (uint8_t)REN_ENABLED) {
        s_state         = (uint8_t)REN_ENABLED;
        s_enter_ms      = in->now_ms;
        s_silence_armed = 0u;
    }

    /* (5) 링크 침묵 (A-4). 무장은 진입 이후 도착한 첫 유효 요청부터 — 진입 이전
     * 스탬프로 무장하면 "스위치는 켰는데 원격기가 아직 안 붙음"이 임계 만에
     * DIS_LINK가 된다. 무장 판정은 랩 안전: 진입 이전 요청이면 좌변이 언더플로로
     * 거대값이 되어 elapsed보다 커진다. */
    if (s_silence_armed == 0u) {
        uint32_t elapsed = (uint32_t)(in->now_ms - s_enter_ms);
        if ((in->req_valid != 0u) &&
            ((uint32_t)(in->last_req_ms - s_enter_ms) <= elapsed)) {
            s_silence_armed = 1u;
        }
    }
    if ((s_silence_armed != 0u) &&
        ((uint32_t)(in->now_ms - in->last_req_ms) >= SILENCE_MS)) {
        s_state = (uint8_t)REN_DIS_LINK;
    }

    out->state = s_state;
}
