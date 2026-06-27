/* fw/include/app_osc_init.h — OSC 보드 부팅 초기화 글루: 10ms마다 osc_init_fsm
 * 구동, PB12(io_read_usfb) 주입, RESET/SEEK 출력 레벨을 board_reset/board_seek로
 * 라우팅. 부팅 1회 시퀀스(DONE 후 핀 구동 중단 → 향후 app_seek_reset과 공유).
 * spec docs/superpowers/specs/2026-06-27-osc-boot-init-design.md. */
#pragma once

void app_osc_init_init(void);          /* boot: FSM reset + tick gate */
void app_osc_init_run_to_done(void);   /* boot: 블로킹으로 시퀀스 완료 (app_init/LCD 전) */
void app_osc_init_tick(void);          /* 10ms-gated 한 step (run_to_done가 호출) */
