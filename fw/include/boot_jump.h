/* fw/include/boot_jump.h — V31 UART 부트로더 진입 (ROM 시스템 부트로더, USART2 PA2/PA3).
 * 흐름: mon RX 매직 → boot_jump_arm()(IRQ) → 슈퍼루프 boot_jump_poll() 이 BKP0R 표식 + 시스템 리셋
 *       → 리셋 직후 main() 첫 줄 boot_jump_check() 가 표식을 지우고 시스템 메모리로 점프.
 * 리셋을 거치는 이유 = IWDG 는 켜진 뒤 끌 수 없다 — 리셋 직후엔 아직 안 켜져 있다.
 * spec docs/superpowers/specs/2026-09-20-uart-mon-port-design.md §3.2 */
#pragma once

void boot_jump_check(void);   /* main() 첫 줄 (HAL_Init 앞). 표식 없으면 그냥 돌아온다 */
void boot_jump_arm(void);     /* IRQ 에서 호출 — 다음 boot_jump_poll() 에서 리셋 */
void boot_jump_poll(void);    /* 슈퍼루프 iter 경계에서 호출 (FRAM 쓰기 중 리셋 방지) */
