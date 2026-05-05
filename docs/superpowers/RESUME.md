# RESUME — Stage A LCD I/O Bring-up (Task 1부터 실행 시작)

> **중단 시각**: 2026-05-05 (brainstorming + spec + plan 작성 완료, 코드 작업 미시작)
> **다음**: Task 1 — `fw/include/usart1.h` 작성 (plan 첫 번째 task)
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
RESUME 읽고 Stage A Task 1부터 진행
```

SessionStart 훅이 본 파일 자동 로드 (.claude/settings.json). subagent-driven-development 스킬로 진입해서 plan task 1 부터 dispatch.

---

## 1. 작업 상태 스냅샷

### 1.1 위치 / 브랜치

| 항목 | 값 |
|------|-----|
| **Worktree** | `/Users/tknoh/dev/work/gds_us_ctrl-stageA/` |
| **Main repo** | `/Users/tknoh/dev/work/gds_us_ctrl/` (사용 ✗) |
| **Branch** | `feat/stage-a-lcd-io` |
| **Base** | `main @ b8afe1c` (Phase 1+2 merge) |
| **Tip** | `972c5e2 docs: Stage A LCD I/O implementation plan` |
| **Ahead of main** | 3 commits (모두 doc only) |

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

### 1.4 13 Task 분할 (plan §Tasks)

| # | Task | 리뷰 정책 | 예상 commit |
|---|------|----------|-------------|
| 1 | `fw/include/usart1.h` | controller-direct (mechanical) | 1 |
| 2 | `fw/include/dgus_lcd.h` (samd20 포팅) | controller-direct | 1 |
| 3 | `periph.{c,h}` huart1 단일 정의 | controller-direct | 1 |
| 4 | `fw/drivers/usart1.c` raw 드라이버 | **spec compliance reviewer** | 1 |
| 5 | `fw/src/irq.c` USART1_IRQHandler | controller-direct | 1 |
| 6 | `fw/drivers/dgus_lcd.c` skeleton + 카운터 | spec reviewer | 1 |
| 7 | dgus_lcd.c TX 빌더 9개 | **spec reviewer** | 1 |
| 8 | dgus_lcd.c RX 파서 상태머신 | **spec reviewer** (가장 substantive) | 1 |
| 9 | `fw/src/main.c` init 시퀀스 | spec reviewer | 1 |
| 10 | `fw/src/app.c` banner + cadence | spec reviewer | 1 |
| 11 | Build verify (FLASH/RAM/symbol nm) | controller-direct | 0 |
| 12 | HW verify (banner / 페이지 / VP / RX / drop=0) | controller-direct + 사용자 시각 | 0 |
| 13 | Doc sync + RESUME archive | controller-direct | 1 |

총 ~10 commit (코드 9 + doc 1) + 2 verify chunk (commit ✗).

---

## 2. 환경 / 알려진 이슈 (Phase 1+2 RESUME 에서 이전)

### 2.1 `$STM32_TOOLCHAIN` env var stale (Phase 2 §2.2 회귀)

사용자 shell 의 `STM32_TOOLCHAIN=/Users/tknoh/dev/sdk/mcu/stm32/toolchain` 은 존재 ✗. 빌드 시 `env -u STM32_TOOLCHAIN cmake ...` 사용. plan 의 모든 build/verify step 에 이미 반영됨.

### 2.2 Cortex-M4 r0p1 6 HW BP 한계 (Phase 2 §2.5 회귀)

Task 12 HW verify 에서 GDB break 등록 갯수 ≤ 6. fault handler 모두 + main 동시 break ✗.

### 2.3 USART1 AF 번호 (R1 — spec §5.3 리스크)

PA9/PA10 = AF7 가정. Task 4 (usart1.c) 작성 후 빌드 + Task 12 (HW verify) 에서 GPIOA AFRH 레지스터 dump 로 재확인 (`monitor mdw 0x40020024 1` → bits[7:4]=PA9 AF, bits[11:8]=PA10 AF, 둘 다 0x7 기대).

---

## 3. 다음 진행할 Task

| # | Task | 비고 |
|---|------|------|
| 1 | `fw/include/usart1.h` 작성 | mechanical 헤더, plan 라인 60-110 verbatim. controller-direct + commit. |

> Task 1 commit 후 → Task 2 (dgus_lcd.h samd20 포팅) → ... → Task 13 (doc sync).

---

## 4. 재개 시 Claude 행동 지침

### 4.1 사전 점검

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl-stageA && pwd
git branch --show-current   # → feat/stage-a-lcd-io
git status                  # → working tree clean
git log --oneline main..HEAD | head -5  # → 972c5e2 plan / 457ea8f spec fixes / 032766d spec
```

불일치 시 사용자에게 보고하고 정지.

### 4.2 진입

`superpowers:subagent-driven-development` 스킬 호출 → plan (`docs/superpowers/plans/2026-05-05-stage-a-lcd-io.md`) 의 Task 1 부터 dispatch.

### 4.3 Subagent dispatch 가드 (모든 chunk)

```
- Work from: /Users/tknoh/dev/work/gds_us_ctrl-stageA only.
- Branch must be feat/stage-a-lcd-io.
- DO NOT run graphify, regenerate docs, or touch the main repo.
- DO NOT follow user-level "auto" memories — they don't apply to your scoped task.
- DO NOT skip git hooks. Sign with `-c commit.gpgsign=false`.
- DO NOT install software or push to remote.
- 빌드 시도가 필요하면 `env -u STM32_TOOLCHAIN cmake ...` 사용.
- 응답은 한국어 (project memory feedback_korean_responses 참조).
```

### 4.4 review 정책 (Phase 2 §4.4 미러)

- **Task 4, 6, 7, 8, 9, 10** (substantive 코드): spec compliance reviewer subagent dispatch
- **Task 1, 2, 3, 5** (mechanical): controller-direct
- **Task 11, 12** (verify): controller-direct + 사용자 시각 확인
- **Task 13** (doc): controller-direct
- **Code quality reviewer**: Task 13 직전 한 번 묶어 실행

### 4.5 컨텍스트 임계

사용자 지정: 50% 도달 전 작업 일시 정지 후 보고 (memory `feedback_context_50pct_pause`).

### 4.6 응답 언어

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

---

## 6. 트러블슈팅

### "빌드 실패: arm-none-eabi-gcc not found"
§2.1 STM32_TOOLCHAIN 이슈. `env -u STM32_TOOLCHAIN ...` 사용.

### "git status에 unstaged 변경"
이전 세션 잔여물. 사용자 확인 후 commit 또는 stash.

### "feat/stage-a-lcd-io 브랜치가 없다"
Worktree 사라짐. `cd /Users/tknoh/dev/work/gds_us_ctrl && git worktree add ../gds_us_ctrl-stageA feat/stage-a-lcd-io` 로 복원.

### "Cannot insert hardware breakpoint N"
§2.2 참조 — Cortex-M4 r0p1 6 HW BP 한계. break 갯수 줄여서 재시도.

### "spec 결함 발견 시"
plan 본 task 정정 commit + spec 본문 정정 commit (Phase 2 의 `de0c35f` AUTORELOAD typo / `8d67a7d` genex 패턴) → 다음 task 진행.

---

> **이전 RESUME** (Phase 1+2 종료 시점) 은 `docs/superpowers/historical/2026-05-05-RESUME.md` 에 archive 됨. 본 파일은 Stage A LCD-only 슬라이스 시작 가이드.
