# RESUME — Stage A LCD I/O Bring-up (Task 10부터 재개)

> **중단 시각**: 2026-05-05 (Task 9 완료, Task 10 시작 직전)
> **다음**: Task 10 — `fw/src/app.c` banner 갱신 + 1Hz LCD cadence (substantive, spec reviewer 필수)
> **재개 시점**: 사용자 새 세션 시작 시
> **스킬 흐름**: `superpowers:subagent-driven-development` (Task별 fresh subagent + review)
> **세션 진입 first-step**: `/graphify` (사용자 메모리 `feedback_graphify_after_docs` 정책 — Task 9 에서 코드/spec/plan 변경됨, graph 재생성 후 Task 10 진입). 단 vendor 포함 corpus 가 막대하므로 사용자 결정 필요 — 이전 세션은 사용자가 graphify 스킵 채택.

---

## ⚡ 빠른 재개 절차 (사용자)

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl-stageA   # ← 반드시 worktree 디렉토리
claude
```

새 세션 시작 후 한 줄 입력:

```
RESUME 읽고 graphify 후 Stage A Task 9부터 진행
```

SessionStart 훅이 본 파일 자동 로드 (.claude/settings.json). 진입 절차:
1. `/graphify` 실행 (코드/spec/plan 변경 반영, `feedback_graphify_after_docs` 메모리 정책)
2. `superpowers:subagent-driven-development` 스킬 호출
3. plan Task 9 부터 dispatch

---

## 1. 작업 상태 스냅샷

### 1.1 위치 / 브랜치

| 항목 | 값 |
|------|-----|
| **Worktree** | `/Users/tknoh/dev/work/gds_us_ctrl-stageA/` |
| **Main repo** | `/Users/tknoh/dev/work/gds_us_ctrl/` (사용 ✗) |
| **Branch** | `feat/stage-a-lcd-io` |
| **Base** | `main @ b8afe1c` (Phase 1+2 merge) |
| **마지막 코드 commit** | `15aa74c feat: wire usart1_init + dgus_init into main init sequence` (Task 9) |
| **Tip (본 RESUME 갱신 후)** | 본 commit |
| **Ahead of main** | 21 commits (4 doc 초기 + 9 code + 8 doc 정정/RESUME 갱신) |

### 1.2 산출 문서

| 파일 | 역할 |
|------|------|
| `docs/superpowers/specs/2026-05-05-stage-a-lcd-io-design.md` | 6 섹션 통합 설계 명세 (567 lines) |
| `docs/superpowers/plans/2026-05-05-stage-a-lcd-io.md` | 13 task 구현 계획 (1391 lines) |
| `docs/superpowers/RESUME.md` | **본 파일** |

### 1.3 brainstorming 결정 이력 (Q1~Q5 + 레이어링)

| # | 항목 | 결정 |
|---|------|------|
| Q1 | 데모 범위 | (c) init + reset옵션 + set_page + 1Hz uptime VP write + passive RX |
| Q2 | HMI 매핑 | (α) samd20 HMI VP 맵 재사용 (LCD_RUN_STD=9, VAR_POWER=0x1110) |
| Q3 | I/O 전략 | (i) 폴링 TX + IT byte-RX (samd20 패턴 STM32 HAL 미러) |
| Q4 | API 표면 | (C) samd20 9 함수 풀 패리티 |
| Q5 | HW 가용성 | (I) LCD 결선 + samd20 HMI 가정 |
| Arch | 레이어링 | (B) 2층 분리 (Phase 2 raw+wrapper 패턴 미러) |

### 1.4 13 Task 진행 상태

| # | Task | 리뷰 정책 | Commit | 상태 |
|---|------|----------|--------|------|
| 1 | `fw/include/usart1.h` | controller-direct (mechanical) | `3f3a6e8` | ✅ |
| 2 | `fw/include/dgus_lcd.h` (samd20 포팅) | controller-direct | `20cdffa` | ✅ |
| 3 | `periph.{c,h}` huart1 단일 정의 | controller-direct | `58aac2a` | ✅ |
| 4 | `fw/drivers/usart1.c` raw 드라이버 | spec reviewer ✅ (13/13) | `286d908` | ✅ |
| 5 | `fw/src/irq.c` USART1_IRQHandler | controller-direct | `1701338` | ✅ |
| 6 | `fw/drivers/dgus_lcd.c` skeleton + 카운터 | spec reviewer (묶음) | `6ebe8f9` | ✅ |
| 7 | dgus_lcd.c TX 빌더 9개 | spec reviewer ✅ (Task 6+7 묶음 20/20) | `4d8baa2` | ✅ |
| 8 | dgus_lcd.c RX 파서 상태머신 | spec reviewer ✅ (12/12) | `e330226` | ✅ |
| 9 | `fw/src/main.c` init 시퀀스 | spec reviewer ✅ (10/10) | `15aa74c` | ✅ |
| **10** | **`fw/src/app.c` banner + cadence** | **spec reviewer** | — | **▶ 다음** |
| 11 | Build verify (FLASH/RAM/symbol nm) | controller-direct | — | 대기 |
| 12 | HW verify (banner / 페이지 / VP / RX / drop=0) | controller-direct + 사용자 시각 | — | 대기 |
| 13 | Doc sync + RESUME archive | controller-direct | — | 대기 |

### 1.5 이전 세션 spec reviewer 결과 (Task 4, 6+7, 8)

- **Task 4** (`286d908`, `usart1.c` 107 LOC): 13/13 체크리스트 ✅ verbatim 준수, drift 0건. CRITICAL/HIGH/MEDIUM/LOW 0.
- **Task 6+7 묶음** (`6ebe8f9` + `4d8baa2`, `dgus_lcd.c` skeleton + TX 빌더 9개): 20/20 체크리스트 ✅. samd20 ref 9 함수 풀 패리티 의미적·payload 형식 동등성 확인. CRITICAL/HIGH/MEDIUM/LOW 0.
- **Task 8** (`e330226`, dgus_lcd.c RX 파서 88 LOC): 12/12 체크리스트 ✅. 4 상태 전이, LEN bound, 50ms 벽시계 timeout, frame_buf 매핑, vp_addr BE, payload 이중 가드 모두 spec §3.4/§3.7 일치. PS_COLLECTING 연산 순서 (timeout vs store) 가 spec 의사코드와 다르나 §3.7 #2 normative ("벽시계 ms") frozen, 의사코드 sketch 라 의미적 동등 판정. CRITICAL/HIGH/MEDIUM 0, LOW 1 (RESUME 안내문 잔여 typo, 즉시 정정 `20354f0`).
- **Task 9** (`15aa74c`, main.c init 시퀀스 +5 LOC): 10/10 체크리스트 ✅ APPROVE. spec §4.1 init 순서 verbatim, usart1_init/dgus_init 위치, race-free 분석, Phase 1+2 init call drift 0, sys_tick_init/__enable_irq explicit 부재, app.c 미수정 (Task 10 분리), include 추가, 빌드 sanity 메타 모두 ✅. CRITICAL/HIGH/MEDIUM/LOW 0. 추가 spec/plan 정정 권고 없음.
- **발견된 spec/plan 결함 (Task 8)**: spec §3.4/§4.4 + plan Task 8/Task 10 의 함수명 typo `sys_tick_ms` → 실제는 `sys_tick_get_ms` (Phase 1+2 source of truth). Phase 2 정정 패턴 (`de0c35f` AUTORELOAD typo / `8d67a7d` genex 패턴) 처럼 spec(`3e60675`) + plan(`7adede8`) + RESUME(`20354f0`) 별도 commit 으로 정정. 코드는 정정된 함수명으로 first-time commit (`e330226`).
- **발견된 spec/plan 결함 (Task 9)**: spec §1.3 콜그래프 + §4.1 init order + 근거 bullet 의 explicit `sys_tick_init()` / `__enable_irq()` 호출 단정이 Phase 2 reality 와 불일치 (실제 Phase 2 는 sys_tick_init 을 app_init 내부에서 호출, explicit `__enable_irq()` 부재 — Cortex-M PRIMASK 리셋 직후 clear 디시플린에 의존). spec(`4353f19`) + plan(`85d11d9`) 두 정정 commit 후 코드(`15aa74c`) 반영하는 Task 8 패턴 미러로 처리. spec §4.1 근거 bullet 도 race 분석 (HAL_UART_Receive_IT 시점 ↔ dgus_init 시점) 으로 갱신.

### 1.6 advisor 가이드 (이전 세션 기록)

- mechanical task (1, 2, 3, 5, 11, 13) = implementer subagent 도 reviewer 도 모두 생략, controller-direct Write → sanity → commit
- Chunk B (Task 4 + 첫 spec reviewer) 컨텍스트 본격 소비 시작 — Task 8 reviewer cycle 도 동등 비용 예상 (실제 측정: Task 8 단독 세션이 advisor + spec reviewer + spec/plan typo 정정 4 commits 처리 후 자연 분기점에서 정지)
- Task 9-10-11 묶음 = 다음 세션 후보 (main.c init + app.c banner+cadence + build verify). Task 9, 10 둘 다 spec reviewer 필요 substantive — 컨텍스트 50% 임계 모니터링 필수.
- Task 11 build verify 끝나면 사용자에 결과 보고 후 Task 12 HW verify 로 넘기는 것이 자연 세션 분리점
- Task 12 = 보드 리셋·시리얼 터미널·LCD 시각·터치 키 = 물리적 사용자 행동 필수 → 무조건 사용자 개입

---

## 2. 환경 / 알려진 이슈 (Phase 1+2 RESUME 에서 이전)

### 2.1 `$STM32_TOOLCHAIN` env var stale (Phase 2 §2.2 회귀)

사용자 shell 의 `STM32_TOOLCHAIN=/Users/tknoh/dev/sdk/mcu/stm32/toolchain` 은 존재 ✗. 빌드 시 `env -u STM32_TOOLCHAIN cmake ...` 사용. plan 의 모든 build/verify step 에 이미 반영됨.

### 2.2 Cortex-M4 r0p1 6 HW BP 한계 (Phase 2 §2.5 회귀)

Task 12 HW verify 에서 GDB break 등록 갯수 ≤ 6. fault handler 모두 + main 동시 break ✗.

### 2.3 USART1 AF 번호 (R1 — spec §5.3 리스크)

PA9/PA10 = AF7 가정. Task 4 (usart1.c) `4d8baa2` 까지 빌드 sanity 통과. Task 12 (HW verify) 에서 GPIOA AFRH 레지스터 dump 로 재확인 (`monitor mdw 0x40020024 1` → bits[7:4]=PA9 AF, bits[11:8]=PA10 AF, 둘 다 0x7 기대).

### 2.4 clangd LSP 경고 (IDE 노이즈)

editor 의 clangd 진단이 `stm32f4xx_hal.h` / `uint8_t` 등 unknown 으로 ✗ 표기 — IDE include path 미설정. 검증 기준은 `arm-none-eabi-gcc -fsyntax-only` 결과 (Task 1-7 모두 exit=0, warning 0개). clangd 노이즈 무시.

---

## 3. 다음 진행할 Task

| # | Task | 비고 |
|---|------|------|
| **10** | **`fw/src/app.c` banner + 1Hz cadence** | plan 라인 956 부근. `app_init` 에서 banner 갱신 (`stage-a-lcd ready` + 2 LCD 라인) + LCD reset(option) + set_page. `app_loop_iter` 에서 1Hz uptime → VAR_POWER write + RX drain + echo drop + RD log. spec §4.2-4.5. **spec compliance reviewer subagent 필수**. |
| 11 | Build verify | FLASH/RAM 사용량, `nm` 으로 `dgus_*` / `usart1_*` symbol export 확인. spec §6.2 acceptance. controller-direct + 사용자 보고. |

> Task 10 → Task 11 (build verify) → **이 시점 사용자 보고** → Task 12 (HW verify, 사용자 시각 확인 필수) → Task 13 (doc sync + RESUME archive).

---

## 4. 재개 시 Claude 행동 지침

### 4.1 사전 점검

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl-stageA && pwd
git branch --show-current   # → feat/stage-a-lcd-io
git status                  # → working tree clean (`.claude/` + `graphify-out/` untracked OK)
git log --oneline main..HEAD | head -20
# expected (최근 → 가장 오래된):
#   <본 RESUME 갱신 commit (Task 8 종료 후)>
#   20354f0 docs: RESUME — sys_tick_ms → sys_tick_get_ms (Task 8 reviewer LOW followup)
#   e330226 feat: dgus_lcd.c RX parser state machine + dgus_rx_poll
#   7adede8 docs: plan — sys_tick_ms → sys_tick_get_ms (Stage A typo fix)
#   3e60675 docs: spec — sys_tick_ms → sys_tick_get_ms (Stage A typo fix)
#   e2e7c71 docs: Stage A RESUME — Task 1-7 완료, Task 8 재개 가이드
#   4d8baa2 feat: dgus_lcd.c TX builders (9-fn samd20 parity)
#   6ebe8f9 feat: dgus_lcd.c skeleton — static state + dgus_init + counters
#   1701338 feat: add USART1_IRQHandler weak override (DGUS LCD)
#   286d908 feat: add USART1 raw driver (PA9/PA10 AF7, IT 1-byte RX, polling TX)
#   58aac2a feat: add huart1 single definition in periph (Stage A)
#   20cdffa feat: add dgus_lcd.h ported from samd20 (9-fn parity + demo macros)
#   3f3a6e8 feat: add usart1.h public API for DGUS raw I/O
#   85a77c5 docs: Stage A RESUME — Task 1부터 실행 시작 가이드
#   972c5e2 docs: Stage A LCD I/O implementation plan
#   457ea8f docs: spec self-review fixes (Stage A LCD)
#   032766d docs: Stage A LCD I/O bring-up design spec
```

불일치 시 사용자에게 보고하고 정지.

### 4.2 진입

1. **`/graphify` 실행** (사용자 결정) — corpus 임계 초과로 사용자에게 옵션 제시. 이전 세션은 vendor 포함 비용 회피 목적으로 스킵 채택 (사용자 메모리 `feedback_graphify_after_docs` 의 정책 자체는 유지, 매 세션 강제 아님).
2. `superpowers:subagent-driven-development` 스킬 호출. plan (`docs/superpowers/plans/2026-05-05-stage-a-lcd-io.md`) Task 10 (라인 956~) 부터 진행.

본격 시작 전 advisor 1회 호출해 Task 10 핵심 검증 포인트 합의 (spec §4.2 banner 형식, §4.3 데모 cadence, §4.4 app_loop_iter RX drain + echo drop + RD log 패턴, sys_tick_get_ms 사용).

### 4.3 Task 10 진행 절차

mechanical 옮기기 + spec reviewer 사이클:

1. plan §Task 10 §Step 1~ 의 변경 (`fw/src/app.c` banner 갱신 + 1Hz cadence) 을 Edit 으로 적용
2. `env -u STM32_TOOLCHAIN arm-none-eabi-gcc -c -fsyntax-only ... -x c fw/src/app.c` exit=0 확인
3. `git add fw/src/app.c && git -c commit.gpgsign=false commit -m "..."`
4. **spec compliance reviewer subagent dispatch** — §4.4 dispatch 가드 그대로. 검증 포인트:
   - banner 라인 verbatim 일치 (spec §4.2: `[boot] gds_us_ctrl stage-a-lcd ready` + `[lcd] usart1@115200 ring=64 prio=5` + `[lcd] init ok, set_page=9 (LCD_RUN_STD), uptime VP=0x1110 (VAR_POWER)`)
   - app_init: `dgus_set_page(LCD_RUN_STD)` + (옵션) `DGUS_DEMO_RESET_ON_BOOT` gate `dgus_reset_lcd()` 호출 위치
   - app_loop_iter: 매 iter `while (dgus_rx_poll(&f))` drain + `dgus_is_echo` skip + non-echo 는 `mon_printf("[lcd] rx ...")` log
   - 1Hz cadence: `(now - prev_lcd_tick) >= 1000` 시 `dgus_write_u16(0x1110, secs)` (spec §4.3-4.4)
   - sys_tick_get_ms 사용 (sys_tick_ms 아님, Task 8 정정 적용된 함수명)
   - reviewer 가 issues flag 시 implementer (controller-direct) 가 fix → 재 commit → 재 dispatch
5. ✅ 후 Task 11 진행

### 4.3a Task 8/9 (완료) 참조

Task 8/9 처리 패턴 — 향후 디버깅 / archive 시 참고:

**Task 8 (`e330226`)** RX 파서 88 LOC:
1. plan §Task 8 §Step 1 verbatim 으로 `fw/drivers/dgus_lcd.c` 끝에 추가
2. 빌드 sanity 에서 `sys_tick_ms` typo 노출 → spec / plan / RESUME / 코드 모두 `sys_tick_get_ms` 로 정정 (4 commits: `3e60675` spec, `7adede8` plan, `e330226` code, `20354f0` RESUME followup)
3. spec reviewer subagent → 12/12 ✅ APPROVE, LOW 1 (RESUME 잔여 typo, 즉시 정정)
4. PS_COLLECTING 연산 순서 (timeout vs store) divergence 는 의미적 동등 판정

**Task 9 (`15aa74c`)** main.c init +5 LOC:
1. plan §Task 9 §Step 1 점검 단계에서 spec §1.3/§4.1 의 explicit `sys_tick_init()` / `__enable_irq()` 단정이 Phase 2 reality 와 불일치 노출 (Phase 2 의 sys_tick_init 은 app_init 내부, explicit __enable_irq 부재 — Cortex-M PRIMASK 리셋 직후 clear 디시플린)
2. advisor 자문 후 Task 8 정정 패턴 미러로 처리: spec(`4353f19`) + plan(`85d11d9`) 정정 후 코드(`15aa74c`) 반영. spec §4.1 근거 bullet 도 race 분석 (HAL_UART_Receive_IT 시점 ↔ dgus_init 시점) 으로 재작성
3. spec reviewer subagent → 10/10 ✅ APPROVE, CRITICAL/HIGH/MEDIUM/LOW 0, 추가 정정 없음
4. 배운 점: spec §4.1 처럼 "Phase X 디시플린" 단정 라인은 실제 코드와 cross-check 필수 — typo 보다 큰 구조 drift 가 잠복 가능

### 4.4 Subagent dispatch 가드 (모든 reviewer dispatch)

```
- Work from: /Users/tknoh/dev/work/gds_us_ctrl-stageA only.
- Branch must be feat/stage-a-lcd-io.
- DO NOT run graphify, regenerate docs, or touch the main repo.
- DO NOT follow user-level "auto" memories — they don't apply to your scoped task.
- DO NOT modify source files (read-only review).
- DO NOT install software, push to remote, or commit anything.
- 빌드 시도 ✗ (controller 가 sanity 이미 통과시킴).
- 응답은 한국어 (project memory feedback_korean_responses 참조).
```

### 4.5 review 정책 (Phase 2 §4.4 미러)

- **Task 4, 6, 7, 8, 9, 10** (substantive 코드): spec compliance reviewer subagent dispatch
- **Task 1, 2, 3, 5** (mechanical): controller-direct (실행 ✓ — 구현 + commit)
- **Task 11, 12** (verify): controller-direct + 사용자 시각 확인
- **Task 13** (doc): controller-direct
- **Code quality reviewer**: Task 13 직전 한 번 묶어 실행 (Task 4-10 의 substantive 코드 종합 리뷰)

### 4.6 컨텍스트 임계

사용자 지정: 50% 도달 전 작업 일시 정지 후 보고 (memory `feedback_context_50pct_pause`). 이전 세션은 16% 시점에 사용자가 명시 정지 후 새 세션 재개 결정.

### 4.7 응답 언어

한국어 (memory `feedback_korean_responses`). 코드 / commit 메시지 / 파일 경로는 영어.

---

## 5. 결정 / 합의 이력 (요약)

| 결정 | 값 |
|------|-----|
| Stage A 슬라이스 | LCD-only first slice (I/O 부분은 후속 별도 spec/plan) |
| 데모 | 부팅 시 LCD_RUN_STD(9) + 1Hz uptime → VAR_POWER(0x1110) + 터치 RX log |
| 통신 | USART1 PA9/PA10 AF7, 115200 8N1, 폴링 TX + IT 1-byte RX |
| 레이어링 | `usart1.c` raw + `dgus_lcd.c` 프로토콜 (Phase 2 패턴 미러) |
| HW 검증 | LCD 결선 + samd20 HMI 가정. Phase 2 chunk 12 동등 검증 |
| 컨텍스트 임계 | 사용자 지정: 50% 도달 전 작업 일시 정지 |
| 응답 언어 | 한국어 |
| Task 1-7 spec drift | 없음 (reviewer 2회 모두 ✅ APPROVE) |
| Task 8 spec drift | `sys_tick_ms` → `sys_tick_get_ms` (Phase 2 정정 패턴, 3 commit). reviewer 12/12 ✅ APPROVE |
| Task 9 spec drift | spec §1.3/§4.1 의 explicit `sys_tick_init()` / `__enable_irq()` 단정 제거 (Phase 2 reality 정합). spec(`4353f19`) + plan(`85d11d9`) 정정 후 코드(`15aa74c`). reviewer 10/10 ✅ APPROVE |
| 세션 진입 first-step | `/graphify` (corpus 결정 사용자 확인 필요 — vendor 포함시 3M 단어, 이전 세션 스킵 채택) |

---

## 6. 트러블슈팅

### "빌드 실패: arm-none-eabi-gcc not found"
§2.1 STM32_TOOLCHAIN 이슈. `env -u STM32_TOOLCHAIN ...` 사용.

### "git status에 unstaged 변경"
이전 세션 잔여물. 사용자 확인 후 commit 또는 stash. `.claude/` untracked 는 IDE 로컬 설정 — 무시.

### "feat/stage-a-lcd-io 브랜치가 없다"
Worktree 사라짐. `cd /Users/tknoh/dev/work/gds_us_ctrl && git worktree add ../gds_us_ctrl-stageA feat/stage-a-lcd-io` 로 복원.

### "Cannot insert hardware breakpoint N"
§2.2 참조 — Cortex-M4 r0p1 6 HW BP 한계. break 갯수 줄여서 재시도.

### "spec 결함 발견 시"
plan 본 task 정정 commit + spec 본문 정정 commit (Phase 2 의 `de0c35f` AUTORELOAD typo / `8d67a7d` genex 패턴) → 다음 task 진행.

### "clangd LSP 가 unknown type 표기"
§2.4. IDE 노이즈, 검증 기준은 `arm-none-eabi-gcc`.

---

> **이전 RESUME** (Phase 1+2 종료 시점) 은 `docs/superpowers/historical/2026-05-05-RESUME.md` 에 archive 됨. 본 파일은 Stage A LCD-only 슬라이스 Task 9 재개 가이드 (Task 1-8 완료 후).
