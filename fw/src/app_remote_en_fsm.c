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
    /* (1) 스위치 OFF = 불허. 인터록의 본래 역할(스위치가 켜져야 원격 가능)은
     * 여기 그대로다. */
    if (in->sw == 0u) {
        s_state         = (uint8_t)REN_DISABLED;
        s_silence_armed = 0u;
        out->state      = s_state;
        return;
    }

    /* (2) E-STOP 은 **레벨로만** 본다 — 래치하지 않는다 (2026-09-05 결정).
     * 래치는 스위치를 껐다 켜야만 풀렸는데, PC8 인터록이 미실장이라 그 동작을
     * 할 수단이 없어 E-STOP 한 번이면 재부팅 전까지 원격 제어가 죽었다
     * (원격기 벤치 실측: `remote_en 1 -> 4` 이후 8분간 전이 없음).
     *
     * 래치를 빼도 E-STOP 차단 자체는 남는다 — 이 게이트는 세 층 중 하나일 뿐이다:
     *   · app_reg.c START 가드 = app_estop_active() 레벨, 소스 무관(US_COMM 포함)
     *   · app_input.c = E-STOP 활성 중 매 tick RUN_RELEASE 강제 + SOL OFF
     *   · 원격기 gds_safety = STATUS bit1 로 물리 명령 전체 차단
     * 잃는 것은 "E-STOP 해제 후 사람이 스위치로 한 번 더 재확인" 의식뿐이고,
     * E-STOP 스위치 자체가 기계 앞 사람의 의도적 조작이라 사용자가 수용했다. */
    if (in->estop != 0u) {
        s_state    = (uint8_t)REN_DIS_ESTOP;
        out->state = s_state;
        return;
    }

    /* (3) 링크 생존 판정. 요청 스탬프는 유효 디코드 전부에 찍히므로 읽기도
     * 생존 신호다. */
    uint8_t link_alive = ((in->req_valid != 0u) &&
                          ((uint32_t)(in->now_ms - in->last_req_ms) < SILENCE_MS))
                       ? 1u : 0u;

    /* (4) 🔴 DIS_LINK 는 **래치하지 않는다** — 링크가 살아나면 스스로 복귀한다.
     * 래치했더니 자기교착이 생겼다: 이 보드의 Modbus TCP 소켓은 1개이고 피어가
     * 사라진 뒤 stale ESTABLISHED 가 자가치유되는 데 실측 ~20초가 걸린다(침묵
     * 임계 10초보다 길다). 즉 원격기가 재접속할 때마다 게이트가 잠기고, 사람이
     * 기계까지 걸어가 키를 껐다 켜야 했다 — 매번.
     * 자동 복귀가 안전한 이유: 스위치가 여전히 ON 이므로 "사람이 기기 앞에
     * 있다"는 인터록의 전제는 깨지지 않는다. 침묵 해제의 목적은 통신이 죽은 채
     * 허용이 남는 것을 막는 것이지, 사람을 다시 부르는 것이 아니다. */
    if (s_state != (uint8_t)REN_ENABLED) {
        if ((s_state == (uint8_t)REN_DIS_LINK) && (link_alive == 0u)) {
            out->state = s_state;      /* 아직 침묵 — 닫힌 채 유지 */
            return;
        }
        s_state    = (uint8_t)REN_ENABLED;
        s_enter_ms = in->now_ms;
        /* 살아있는 링크로 복귀한 것이면 이미 무장 상태다. 스위치를 방금 켠
         * 경우(트래픽 없음)는 미무장 — 아래 (5) 규칙이 그것을 처리한다. */
        s_silence_armed = link_alive;
    }

    /* (5) 링크 침묵 감시. 무장은 진입 이후 도착한 첫 유효 요청부터 — 진입 이전
     * 스탬프로 무장하면 "스위치는 켰는데 원격기가 아직 안 붙음"이 임계 만에
     * DIS_LINK 가 된다. 무장 판정은 랩 안전: 진입 이전 요청이면 좌변이
     * 언더플로로 거대값이 되어 elapsed 보다 커진다. */
    if (s_silence_armed == 0u) {
        uint32_t elapsed = (uint32_t)(in->now_ms - s_enter_ms);
        if ((in->req_valid != 0u) &&
            ((uint32_t)(in->last_req_ms - s_enter_ms) <= elapsed)) {
            s_silence_armed = 1u;
        }
    }
    if ((s_silence_armed != 0u) && (link_alive == 0u)) {
        s_state = (uint8_t)REN_DIS_LINK;
    }

    out->state = s_state;
}
