/* fw/include/app_hold_wdt_fsm.h — 원격 hold-to-run 워치독 순수 FSM. HAL-free.
 *
 * 원격기 START 를 "누르고 있는 동안만 가동" 으로 만들려면 링크가 끊겼을 때 컨트롤러가
 * 스스로 서야 한다 — 원격기는 정지를 보낼 수 없다. 이 모듈은 hold 세션 하나의 시각만
 * 본다: START=2 로 무장, START=3 으로 갱신, T 안에 갱신이 없으면 트립(=글루가
 * RUN_RELEASE). 런 소스가 US_COMM 이 아니게 되면(누군가 세웠다) 조용히 끝난다.
 *
 * 요구사항: docs/superpowers/specs/2026-09-05-remote-hold-to-run-requirements.md R-1/R-3
 * 설계:     docs/superpowers/specs/2026-09-06-remote-hold-to-run-design.md §3·§4·§5
 *
 * 🔴 keep 에는 기동 권한이 없다 — 무장 중에만 시각을 갱신한다. 그렇지 않으면 E-STOP
 *    해제·30 s 상한·패널 정지 직후 도착한 늦은 유지 신호가 기계를 다시 돌린다.
 * 🔴 무장은 글루의 START=2 분기 한 곳에서만, 그것도 app_reg_start_allowed 일 때만.
 *    탭 START(=1)·LCD·물리 버튼·weld 는 이 컨텍스트에 접근 경로 자체가 없다. */
#pragma once
#include <stdint.h>

/* 유지 신호 타임아웃 (spec §5). 최악 적층 579 ms(P150 + 유실1 150 + 지터 100 +
 * 쓰기지연 131[정상 FRAM 포함] + FRAM 최악 추가 48) < 600. 500 이면 오정지.
 * 벤치 단축 빌드가 -D 로 덮어쓸 수 있게 #ifndef 가드 — host 테스트가 200~1000 을 고정. */
#ifndef HOLD_WDT_MS
#define HOLD_WDT_MS   600u
#endif

typedef struct {
    uint8_t  armed;          /* 1 = hold 세션 진행 중 */
    uint32_t last_keep_ms;   /* 마지막 arm/keep 시각 (sys_tick ms) */
} hold_wdt_t;

void    hold_wdt_init (hold_wdt_t *w);
/* START=2 수락 시. 중복 호출(응답 유실 재시도)은 기준 시각만 갱신. */
void    hold_wdt_arm  (hold_wdt_t *w, uint32_t now_ms);
/* START=3. armed 아니면 no-op. */
void    hold_wdt_keep (hold_wdt_t *w, uint32_t now_ms);
/* 매 tick. run_is_comm = (us_run_status == US_COMM, 지연 0 값).
 * 반환 1 = 트립(글루가 RUN_RELEASE 발행) — 트립 즉시 해제되어 두 번 트립하지 않는다.
 * run_is_comm == 0 이면 세션만 해제하고 0 을 돌려준다(누군가 이미 세웠다). */
uint8_t hold_wdt_step (hold_wdt_t *w, uint32_t now_ms, uint8_t run_is_comm);
uint8_t hold_wdt_armed(const hold_wdt_t *w);
