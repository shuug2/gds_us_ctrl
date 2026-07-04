# Handoff: weld 슬라이스4 spec+plan 완료 — 다음 = 새 세션 subagent-driven 실행

**Generated**: 2026-07-04 (e 세션 마감)
**Branch**: `main` (docs tip; origin ahead — push 미실행). **작업 브랜치 = `feat/stage-weld-slice4-trigger`** (ch1' 스택 위, spec+plan 커밋 2개 보유)
**Status**: Ready — 플랜 승인·커밋 완료, 실행만 남음 (사용자 지시: 새 세션에서 subagent-driven)

> **요약**: weld 슬라이스4(TRIGGER+양손 트리거+안전 abort+진입 게이팅+D2 클램프
> M1~M4, D4/H1 선결)의 brainstorming→spec→plan을 완료. 브랜치
> `feat/stage-weld-slice4-trigger`(base=ch1' tip `9c7cd4a`)에 spec `434f7f5` +
> plan `e0bc491` 커밋. **다음 세션 = 그 브랜치 checkout 후
> `superpowers:subagent-driven-development`로 plan 10 Task 실행.**

## Goal

samd20 공압 프레스 사이클의 물리 계층 흡수 + 감사 D2/D4 종결. 정본 문서(모두
`feat/stage-weld-slice4-trigger` 브랜치에만 존재 — main엔 없음):
- spec: `docs/superpowers/specs/2026-07-04-stage-weld-cycle-slice4-trigger-design.md`
- plan: `docs/superpowers/plans/2026-07-04-stage-weld-cycle-slice4-trigger.md` (10 Task, TDD, zero-context 실행 가능)

## Completed (이 세션)

- [x] brainstorming: 사용자 결정 3건 — ① **사이클 진입 게이팅 채택**(START guard가 삼킬 상태면 사이클 자체 미시작 = 블라인드 사이클 차단, samd20 대비 의도된 deviation) ② **HORN 모드 이연** ③ 구조 = **A안**(순수 `app_weld_trigger_fsm` 신설 + `app_weld_fsm` 확장 + 글루 배선)
- [x] spec 작성+승인+커밋 `434f7f5` (+보강 커밋 `e0bc491`에 동승: SETUP 게이트 편입, swallow 정정)
- [x] plan 작성+커밋 `e0bc491` (10 Task; 실코드/실명령 포함, self-review 3건 수정 완료)
- [x] main 세션마감 docs (이 커밋)

## Not Yet Done

- [ ] **plan 10 Task 실행** (새 세션, subagent-driven) → CODE-COMPLETE까지. 머지/태그 없음 — **스택 전체 HW 게이트**(b'→d'→ch1'→slice4 순).
- [ ] D5 스택 HW 검증→단위별 머지/태그 (보드+rig 세션; `HANDOFF` 2026-07-04 d판 §참조 = `git show 9afa4c3:HANDOFF.md`)
- [ ] push (main + 스택 4브랜치)

## 다음 세션 Resume Instructions

1. `git checkout feat/stage-weld-slice4-trigger` (plan/spec이 여기 있음)
2. sanity: `env -u STM32_TOOLCHAIN cmake -S fw -B fw/build -G Ninja && env -u STM32_TOOLCHAIN cmake --build fw/build`(0-warning) + `make -C fw/test test`(12스위트 PASS)
3. **`superpowers:subagent-driven-development` 스킬 호출** → plan 문서의 Task 1부터 순서대로 (Task 1=H1 선결이 첫 코드 커밋이어야 함 — 감사 D4 결정)
4. Task별 게이트: host PASS + 빌드 0-warning + 2-stage 리뷰(spec 준수+cpp-reviewer), 최종 Task 10 = 통합 cpp-review + docs
5. ⚠ Task 5에서 신규 .c 추가 → Task 7 빌드 전 **reconfigure 필수**(GLOB 함정)

## Key Decisions (spec 정본에 상세)

| Decision | Rationale |
|----------|-----------|
| 진입 게이팅 = `app_reg_start_allowed()` 신설 (deviation) | samd20은 M_START 직결이라 guard 부재; 포팅 후 START 삼켜지면 SOL만 하강하는 블라인드 사이클 — 사용자 결정으로 차단. guard 4-break를 이 함수로 공용화(드리프트 차단) |
| swallow는 start_allowed 조건에서 제외 | `swallow_start`는 `src==US_TOUCH` 전용 소비(`app_reg.c:136`) — US_CYCLE START에 무관 (spec 작성 후 코드 실측으로 정정) |
| H1 래치 시점 = CYL1→WELD **전이 블록** | WELD 무장(temp_time 선택)과 exit가 같은 스냅샷 사용 — 1-tick 토글 창 제거 |
| TRIGGER CYL2 = 즉시 exit 충실 포팅 | legacy가 HOLD exit에서 `re_up_pressed=1` 강제(main.c:1593 `//-`) — SENSE_UP 실대기 아님. 실 rig 판정 후 1줄 제거로 전환 가능(주석 명기) |
| CYL 타임아웃 미구현 | legacy `CYL_TIMEOUT`은 대입만 되고 미검사 = 죽은 코드 — 충실 |
| M1 = `limit_energy==0` → 에너지-도달 체크 off | `limit_out_time=0`=OVTIME off 기존 패턴과 동일 의미론; 즉시 무증상 완료 차단, 런은 backstop/30s가 바운드 |
| SETUP 게이트 = 페이지 기반 `app_lcd_in_run_page()` | slice1 spec §5.4 이연분(글루 주석에서 재발견); `sys_status` 필드는 미배선이라 lcd_status==run_page 판정 |
| in_cycle set = 글루 `cycle_started()` 콜백 | 게이팅에 막힌 트리거가 양손 재장전 벌칙을 받지 않도록 set 시점을 실시작에 위임 |

## Warnings

- ⚠ **본 브랜치는 미머지 스택 위** (main→b'→d'→ch1'→slice4) — 스택 하부 수정 시 rebase 필요. slice-c/d 산출물(io/input/overload)은 **읽기 전용 소비만**.
- ⚠ plan Task 2의 `steps_to_state` step 수는 근사 — 테스트가 도달 상태를 CHECK로 먼저 검증하므로 어긋나면 즉시 드러남(실측 보정).
- ⚠ D5 거동 변화 2건(TOUCH 비-energy=30s만 / OVTIME>30s 캡)은 HW 회귀 시나리오에 이미 반영 필요 — spec §7.3 참조.
- ⚠ 보드 = SERIAL/addr=1/9600/EVEN, OUT_POWER=56, FRAM ether_ip=.199 (이 세션 보드 무접촉).
- ⚠ 검증 규칙: mbpoll/LCD 육안만, SWD halt 금지. 브랜치 전환 후 reconfigure.
