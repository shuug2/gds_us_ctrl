/* fw/include/boot_magic.h — mon RX 매직 문자열 매처 (순수, host 테스트 test_boot_magic.c).
 * V31 의 UART 부트로더 진입 트리거 — 호출처 = drivers/usart2.c 수신 IRQ. */
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define BOOT_MAGIC_STR  "!!GDSUS_BOOT!!"
#define BOOT_MAGIC_LEN  (sizeof BOOT_MAGIC_STR - 1u)

/* 마지막 BOOT_MAGIC_LEN 바이트 창. 접두가 겹치는 입력("!!!GDSUS…")도 놓치지 않는다. */
typedef struct { uint8_t win[BOOT_MAGIC_LEN]; } boot_magic_t;

void boot_magic_init(boot_magic_t *m);
/* 1 바이트 넣는다. 창이 매직과 같아지는 순간 true. */
bool boot_magic_feed(boot_magic_t *m, uint8_t b);
