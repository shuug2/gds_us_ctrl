/* fw/include/app_remote_en_fsm.h — 원격 제어 활성화 게이트 순수 FSM. HAL-free.
 *
 * 컨트롤러 LCD에서만 켤 수 있는 비영속 게이트. 켜져 있는 동안만 Modbus(US_COMM)
 * 경로의 START/SEEK/RESET + cfg 쓰기가 통과한다 (STOP·읽기는 게이트 무관 상시 허용).
 * 해제는 창 만료 / 링크 침묵 / E-STOP / LCD 수동 4종이며, 사유는 다음 활성화까지
 * 래치된다. samd20 대응물 없음 — 신규 기능.
 * spec: docs/superpowers/specs/2026-08-15-remote-enable-gate-design.md §5.2 */
#pragma once
#include <stdint.h>

/* DG-15a/DG-15b 초기값 (spec §7). 파일럿(VR-13) 후 조정 가능 — 벤치 단축 빌드가
 * -D로 덮어쓸 수 있게 #ifndef 가드 (VR-5의 창 만료 검증용). */
#ifndef REMOTE_EN_WINDOW_S
#define REMOTE_EN_WINDOW_S        600u   /* 활성 창 10분 */
#endif
#ifndef REMOTE_EN_LINK_SILENCE_S
#define REMOTE_EN_LINK_SILENCE_S   10u   /* 링크 침묵 임계 10초 */
#endif

/* 상태 코드 = 레지스터 0x2B wire 값 (spec §4). app_modbus_core.h의
 * MB_REMOTE_EN_*와 값이 일치해야 하며, host 테스트가 이를 고정한다. */
enum {
    REN_DISABLED    = 0,   /* 부팅/초기 */
    REN_ENABLED     = 1,
    REN_DIS_TIMEOUT = 2,   /* 창 만료 */
    REN_DIS_LINK    = 3,   /* 링크 침묵 */
    REN_DIS_ESTOP   = 4,   /* E-STOP 상승 엣지 */
    REN_DIS_LCD     = 5,   /* LCD 수동 해제 */
};

typedef struct {
    uint32_t now_ms;        /* sys_tick ms */
    uint8_t  lcd_enable;    /* 1-shot: 게이트 버튼 롱프레스 릴리스 */
    uint8_t  lcd_disable;   /* 1-shot: 게이트 버튼 짧은 탭 */
    uint32_t last_req_ms;   /* 마지막 유효 Modbus 요청 시각 */
    uint8_t  req_valid;     /* 1 = 요청이 한 번이라도 있었음 */
    uint8_t  estop;         /* E-STOP 레벨 (엣지화는 FSM 내부 책임) */
} remote_en_in_t;

typedef struct {
    uint8_t  state;   /* REN_* — 해제 사유는 다음 활성화까지 래치 */
    uint16_t left_s;  /* 잔여 활성 초(ceil). ENABLED인 동안 절대 0이 아니다 */
} remote_en_out_t;

void remote_en_fsm_init(void);
/* 한 step 진행. 시간 비교는 전부 u32 랩 안전 elapsed 형태. */
void remote_en_fsm_step(const remote_en_in_t *in, remote_en_out_t *out);
