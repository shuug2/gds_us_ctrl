# RESUME — Stage A LCD I/O Bring-up (Task 8부터 재개)

> **중단 시각**: 2026-05-05 (Chunk B 종료, Task 1-7 완료, Task 8 RX 파서 시작 직전)
> **다음**: Task 8 — `fw/drivers/dgus_lcd.c` RX 파서 상태머신 추가 (가장 substantive, spec reviewer 필수)
> **재개 시점**: 사용자 새 세션 시작 시
> **스킬 흐름**: `superpowers:subagent-driven-development` (Task별 fresh subagent + review)

---

## ⚡ 빠른 재개 절차 (사용자)

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl-stageA   # ← 반드시 worktree 디렉토리
claude
```

새 세션 시작 후 한 줄 입력:

```
RESUME 읽고 Stage A Task 8부터 진행
```

SessionStart 훅이 본 파일 자동 로드 (.claude/settings.json). subagent-driven-development 스킬로 진입해서 plan task 8 부터 dispatch.

---

## 1. 작업 상태 스냅샷

### 1.1 위치 / 브랜치

| 항목 | 값 |
|------|-----|
| **Worktree** | `/Users/tknoh/dev/work/gds_us_ctrl-stageA/` |
| **Main repo** | `/Users/tknoh/dev/work/gds_us_ctrl/` (사용 ✗) |
| **Branch** | `feat/stage-a-lcd-io` |
| **Base** | `main @ b8afe1c` (Phase 1+2 merge) |
| **마지막 코드 commit** | `4d8baa2 feat: dgus_lcd.c TX builders (9-fn samd20 parity)` (Task 7) |
| **Tip (본 RESUME 갱신 후)** | 본 commit |
| **Ahead of main** | 12 commits (4 doc + 7 code + 본 RESUME 갱신) |

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
| **8** | **dgus_lcd.c RX 파서 상태머신** | **spec reviewer (가장 substantive)** | — | **▶ 다음** |
| 9 | `fw/src/main.c` init 시퀀스 | spec reviewer | — | 대기 |
| 10 | `fw/src/app.c` banner + cadence | spec reviewer | — | 대기 |
| 11 | Build verify (FLASH/RAM/symbol nm) | controller-direct | — | 대기 |
| 12 | HW verify (banner / 페이지 / VP / RX / drop=0) | controller-direct + 사용자 시각 | — | 대기 |
| 13 | Doc sync + RESUME archive | controller-direct | — | 대기 |

### 1.5 이전 세션 spec reviewer 결과 (Task 4, 6+7)

- **Task 4** (`286d908`, `usart1.c` 107 LOC): 13/13 체크리스트 ✅ verbatim 준수, drift 0건. CRITICAL/HIGH/MEDIUM/LOW 0.
- **Task 6+7 묶음** (`6ebe8f9` + `4d8baa2`, `dgus_lcd.c` skeleton + TX 빌더 9개): 20/20 체크리스트 ✅. samd20 ref 9 함수 풀 패리티 의미적·payload 형식 동등성 확인. CRITICAL/HIGH/MEDIUM/LOW 0.
- **발견된 spec/plan 결함**: 없음. 다음 task 그대로 진행.

### 1.6 advisor 가이드 (이전 세션 기록)

- mechanical task (1, 2, 3, 5, 11, 13) = implementer subagent 도 reviewer 도 모두 생략, controller-direct Write → sanity → commit
- Chunk B (Task 4 + 첫 spec reviewer) 컨텍스트 본격 소비 시작 — Task 8 reviewer cycle 도 동등 비용 예상
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
| **8** | `fw/drivers/dgus_lcd.c` RX 파서 상태머신 | plan 라인 769-861 verbatim. PS_IDLE → PS_GOT_5A → PS_GOT_HEADER → PS_COLLECTING 전이. LEN ∈ [4, 26] 검증 (samd20 결함 #1 회피) + 50ms 벽시계 timeout (결함 #2). `dgus_rx_poll(out)` ring drain → parser_step 반복. **spec compliance reviewer subagent 필수** — spec §3.4 RX 파서 전이 + §3.7 결함 회피 정밀 검증. |

> Task 8 완료 → Task 9 (main.c init 시퀀스) → Task 10 (app.c banner+1Hz cadence) → Task 11 (build verify) → 이 시점 사용자 보고 → Task 12 (HW verify, 사용자 시각 확인 필수) → Task 13 (doc sync + RESUME archive).

---

## 4. 재개 시 Claude 행동 지침

### 4.1 사전 점검

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl-stageA && pwd
git branch --show-current   # → feat/stage-a-lcd-io
git status                  # → working tree clean (`.claude/` untracked OK)
git log --oneline main..HEAD | head -15
# expected (최근 → 가장 오래된):
#   <RESUME 갱신 commit>
#   4d8baa2 feat: dgus_lcd.c TX builders (9-fn samd20 parity)
#   6ebe8f9 feat: dgus_lcd.c skeleton — static state + dgus_init + counters
#   1701338 feat: add USART1_IRQHandler weak override (DGUS LCD)
#   286d908 feat: add USART1 raw driver (PA9/PA10 AF7, IT 1-byte RX, polling TX)
#   58aac2a feat: add huart1 single definition in periph (Stage A)
#   20cdffa feat: add dgus_lcd.h ported from samd20 (9-fn parity + demo macros)
#   3f3a6e8 feat: add usart1.h public API for DGUS raw I/O
#   85a77c5 docs: Stage A RESUME — Task 1부터 실행 시작 가이드  (또는 갱신된 본 RESUME 의 직전 commit)
#   972c5e2 docs: Stage A LCD I/O implementation plan
#   457ea8f docs: spec self-review fixes (Stage A LCD)
#   032766d docs: Stage A LCD I/O bring-up design spec
```

불일치 시 사용자에게 보고하고 정지.

### 4.2 진입

`superpowers:subagent-driven-development` 스킬 호출. plan (`docs/superpowers/plans/2026-05-05-stage-a-lcd-io.md`) Task 8 (라인 764-882 근방) 부터 진행.

본격 시작 전 advisor 1회 호출해 Task 8 RX 파서 핵심 검증 포인트 합의 (spec §3.4 전이도, §3.7 결함 회피 디시플린).

### 4.3 Task 8 진행 절차

mechanical 옮기기 + spec reviewer 사이클:

1. plan §Task 8 §Step 1 의 코드 (`#define DGUS_LEN_MIN/MAX`, `parser_step`, `dgus_rx_poll`) 를 `fw/drivers/dgus_lcd.c` 끝에 추가 (Edit, TX 빌더 뒤에)
2. `env -u STM32_TOOLCHAIN arm-none-eabi-gcc -c -fsyntax-only ... -x c fw/drivers/dgus_lcd.c` exit=0 확인
3. `git add fw/drivers/dgus_lcd.c && git -c commit.gpgsign=false commit -m "feat: dgus_lcd.c RX parser state machine + dgus_rx_poll"`
4. **spec compliance reviewer subagent dispatch** — 검증 포인트:
   - 4 상태 전이 spec §3.4 verbatim 일치 (특히 PS_GOT_5A 에서 연속 0x5A 처리: state 유지)
   - PS_GOT_HEADER 에서 LEN ∈ [4, 26] 검증, 범위 외 → drop counter 증가 + IDLE 복귀 (samd20 결함 #1 회피)
   - PS_COLLECTING 에서 `sys_tick_get_ms()` 벽시계 50ms timeout (samd20 결함 #2 회피, byte-counted ✗)
   - frame_buf 매핑: [0]=cmd, [1]=addr_h, [2]=addr_l, [3..]=payload. `data_len = LEN - 3`
   - vp_addr big-endian 조립 `(frame_buf[1] << 8) | frame_buf[2]`
   - `out->data` copy 시 `i < data_len && i < DGUS_MAX_DATA` 이중 가드
   - `dgus_rx_poll` 가 `usart1_rx_pop` 호출로 ring 1 byte씩 소비, `parser_step` 가 true 반환 시 즉시 return true (한 호출 = 최대 1 프레임)
   - unreachable path (switch fall-through) 도 IDLE 복귀로 graceful
   - reviewer 가 issues flag 시 implementer (controller-direct) 가 fix → 재 commit → 재 dispatch
5. ✅ 후 Task 9 진행

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

> **이전 RESUME** (Phase 1+2 종료 시점) 은 `docs/superpowers/historical/2026-05-05-RESUME.md` 에 archive 됨. 본 파일은 Stage A LCD-only 슬라이스 Task 8 재개 가이드.
