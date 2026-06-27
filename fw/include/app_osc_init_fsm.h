/* fw/include/app_osc_init_fsm.h — OSC 보드 부팅 초기화: HAL-free pure FSM core.
 * 전원 투입 후 OSC 보드 자가 초기화(자체 초음파 ~600ms 출력)를 PB12 피드백으로
 * 감지(H→L), 종료 150ms 후 RESET(PB10) 200ms → SEEK(PB2) 100ms 펄스를 출력해
 * OSC 보드 초기화를 완료. 이벤트 기반 + 타임아웃 폴백(보드 부재/고장 시 무한대기
 * 방지). 글루(app_osc_init.c)가 10ms마다 osc_init_fsm_step() 호출. host-tested.
 * spec docs/superpowers/specs/2026-06-27-osc-boot-init-design.md. */
#pragma once
#include <stdint.h>

enum {
    OSC_WAIT_H = 0,   /* PB12 H(초음파 출력 시작) 대기 */
    OSC_WAIT_L = 1,   /* PB12 L(초음파 출력 종료) 대기 */
    OSC_GAP    = 2,   /* 종료 후 150ms 갭 */
    OSC_RESET  = 3,   /* RESET 펄스 200ms */
    OSC_SEEK   = 4,   /* SEEK 펄스 100ms */
    OSC_DONE   = 5    /* 초기화 완료 (terminal) */
};

/* 10ms tick 기준 카운트. 정상 부팅은 실제 PB12 엣지가 구동(타임아웃은 보드
 * 부재/고장 폴백 전용) — 부팅 직렬화이므로 폴백을 짧게 잡아 스톨 최소화. */
#define OSC_WAIT_H_TIMEOUT   90u  /* 900ms 폴백 (H 미감지 시 진행) */
#define OSC_WAIT_L_TIMEOUT   90u  /* 900ms 폴백 (L 미감지 시 진행) */
#define OSC_H_DEBOUNCE        2u  /* WAIT_H→WAIT_L: 연속 H 샘플 수 (스파이크 무시) */
#define OSC_GAP_TICKS        15u  /* 150ms */
#define OSC_RESET_TICKS       4u  /* 40ms */
#define OSC_SEEK_TICKS        2u  /* 20ms */

/* step input — 글루가 매 tick 주입 (app_seek_reset 주입 패턴). */
typedef struct {
    uint8_t pb12;   /* PB12 피드백 raw 레벨 (1=H=초음파 출력 중) */
} osc_init_in_t;

/* step output — signal은 active 레벨(글루가 active-LOW 폴라리티 적용). */
typedef struct {
    uint8_t state;          /* 현재 OSC_* */
    uint8_t reset_signal;   /* RESET 명령선 레벨 (1=active) → PB10 */
    uint8_t seek_signal;    /* SEEK 명령선 레벨 (1=active) → PB2 */
} osc_init_out_t;

void    osc_init_fsm_init(void);                                        /* reset to WAIT_H (boot) */
void    osc_init_fsm_step(const osc_init_in_t *in, osc_init_out_t *out); /* one 10ms tick */
uint8_t osc_init_fsm_state(void);                                       /* current OSC_* */
