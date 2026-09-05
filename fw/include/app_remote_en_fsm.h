/* fw/include/app_remote_en_fsm.h — 원격 제어 활성화 게이트 순수 FSM. HAL-free.
 *
 * 기계 쪽 물리 스위치(PC8/CON_REMOTE_EN)가 "허용"일 때만 Modbus(US_COMM) 경로의
 * START/SEEK/RESET + cfg 쓰기가 통과한다 (STOP·읽기는 게이트 무관 상시 허용).
 * 원격 START는 사람이 없을 수 있는 곳에서 기계를 돌리는 것이라, **기기 앞에
 * 사람이 있어야 한다**는 인터록이 필요하다. samd20 대응물 없음 — 신규 기능.
 *
 * 요구사항: docs/superpowers/specs/2026-08-30-remote-parity-requirements.md §요청 A
 * 설계 뿌리: docs/superpowers/specs/2026-08-15-remote-enable-gate-design.md
 *   ⚠ 그 spec의 "LCD 조작(T-5) + 창 만료(600s)"는 2026-08-30에 폐기됐다.
 *     스위치는 상태가 눈에 보여 "켜고 잊는다" 전제가 사라지고(A-3), 레벨에
 *     만료를 얹으면 조작자가 "스위치는 켜 있는데 왜 안 되지"를 겪는다.
 *
 * 🔴 해제 사유(링크 침묵·E-STOP)는 **래치**되고 스위치 OFF→ON 재무장으로만 풀린다.
 *    자동 복귀를 허용하면 E-STOP이 풀리는 순간 사람이 없어도 원격 기동이 되살아난다.
 *    스위치 OFF 자체가 래치를 지우므로 별도 엣지 검출 상태가 필요 없다. */
#pragma once
#include <stdint.h>

/* 링크 침묵 임계 (A-4). 벤치 단축 빌드가 -D로 덮어쓸 수 있게 #ifndef 가드. */
#ifndef REMOTE_EN_LINK_SILENCE_S
#define REMOTE_EN_LINK_SILENCE_S   10u
#endif

/* 상태 코드 = 레지스터 0x2B wire 값. app_modbus_core.h의 MB_REMOTE_EN_*와 값이
 * 일치해야 하며 host 테스트가 이를 고정한다.
 * ⚠ 2와 5는 폐기된 의미론(창 만료 / LCD 수동 해제)의 잔여 번호다. 원격기가 이미
 *   아는 값이라 **재번호하지 않는다** — 결번으로 두고 이 FSM은 생산하지 않는다. */
enum {
    REN_DISABLED    = 0,   /* 부팅 / 스위치 OFF */
    REN_ENABLED     = 1,
    /* 2 = 결번 (구 REN_DIS_TIMEOUT — 창 만료, A-3으로 폐기) */
    REN_DIS_LINK    = 3,   /* 링크 침묵 — 래치 */
    REN_DIS_ESTOP   = 4,   /* E-STOP — 레벨 추종 (래치 ✗, 2026-09-05) */
    /* 5 = 결번 (구 REN_DIS_LCD — LCD 수동 해제, T-5 폐기) */
};

typedef struct {
    uint32_t now_ms;        /* sys_tick ms */
    uint8_t  sw;            /* 물리 스위치 레벨: 1 = 허용 위치 (극성 변환은 글루 책임) */
    uint32_t last_req_ms;   /* 마지막 유효 Modbus 요청 시각 */
    uint8_t  req_valid;     /* 1 = 요청이 한 번이라도 있었음 */
    uint8_t  estop;         /* E-STOP 레벨 */
} remote_en_in_t;

typedef struct {
    uint8_t  state;   /* REN_* — 해제 사유는 스위치 재무장까지 래치 */
} remote_en_out_t;

void remote_en_fsm_init(void);
/* 한 step 진행. 시간 비교는 전부 u32 랩 안전 elapsed 형태. */
void remote_en_fsm_step(const remote_en_in_t *in, remote_en_out_t *out);
