# RESUME — Phase 1+2 Bootstrap Implementation (Chunk 8부터)

> **중단 시각**: 2026-05-03 (Chunk 7 PASS 직후, Phase 1 HW verify 완료)
> **재개 시점**: 사용자 새 세션 시작 시
> **스킬 흐름**: `superpowers:subagent-driven-development` (Chunk별 fresh subagent + 정식 review는 substantive 코드 chunk에서만)

---

## ⚡ 빠른 재개 절차 (사용자)

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl-phase12   # ← 반드시 worktree 디렉토리
claude
```

새 세션 시작 후 한 줄 입력:

```
RESUME 읽고 Chunk 8부터 진행
```

SessionStart 훅이 본 파일 자동 로드. Chunk 8은 첫 substantive Phase 2 작업 (UART/TIM enable + periph + drivers).

---

## 1. 작업 상태 스냅샷

### 1.1 위치 / 브랜치

| 항목 | 값 |
|------|-----|
| **Worktree** | `/Users/tknoh/dev/work/gds_us_ctrl-phase12/` |
| **Main repo** | `/Users/tknoh/dev/work/gds_us_ctrl/` (사용 ✗) |
| **Branch** | `feat/phase1-2-bootstrap` |
| **Base** | `main @ 73027c8` |
| **Tip** | (Chunk 7 RESUME update commit — 본 commit) |
| **Ahead of main** | 15 commits |

### 1.2 산출 문서

| 파일 | 역할 |
|------|------|
| `docs/superpowers/specs/2026-04-26-phase1-2-bootstrap-design.md` | 6 섹션 통합 설계 명세 |
| `docs/superpowers/plans/2026-04-26-phase1-2-bootstrap.md` | 30 task 구현 계획 |
| `docs/superpowers/RESUME.md` | **본 파일** |

### 1.3 완료된 Chunks (1–7)

| # | Chunk | Commits |
|---|-------|---------|
| 1 | Vendor seed import (tasks 1-2) | `c8eaac6`, `de37164`, `26d35d1` |
| — | RESUME 초기 작성 | `e6e68bc` |
| 2 | Build infra (toolchain + gitignore + hal_conf) | `0e5c338`, `2a38e7f`, `64d8fb3` |
| 3 | Phase 1 sources (clock, irq, main) | `9ed6a88`, `b45891f`, `a3cee5b` |
| 4 | Phase 1 CMakeLists | `cfb1aa4` |
| — | **spec defect fix** (genex Debug flags) | `8d67a7d` |
| 5 | Phase 1 build verify ✅ (PASS) | (no commit; controller-direct) |
| 6 | OpenOCD config | `7f4fcf4` |
| — | RESUME Chunk 7 checkpoint | `befad7f` |
| 7 | **Phase 1 HW verify ✅ (PASS)** | (no commit; controller-direct) |

**Phase 1 빌드 결과**: FLASH 3860 B (2.94%), RAM 1584 B (4.83%), elf/bin/hex/map 4 산출물 정상.

**Phase 1 HW 검증 결과** (Chunk 7, 2026-05-03):
- ST-LINK V3J15M7B5S1 (API v3, VID:PID 0483:374F), Target Vcc 3.28 V
- Cortex-M4 r0p1, 6 HW BP / 4 watchpoints
- Reset_Handler 진입 OK (PC=0x080003b4, MSP=0x20008000=SRAM top 32KB)
- HAL_Init + clock_init 통과 (Error_Handler / HardFault 미히트)
- main `while(1) __NOP()` 도달 확인 (PC=0x80002ea = main+10)
- **SystemCoreClock = 96,000,000 Hz** (HSI×12 PLL lock 확인)

---

## 2. 발견된 이슈 (다음 세션이 알아야 함)

### 2.1 spec 결함 — generator expression 빌드 블로커 (해결됨)

`fw/CMakeLists.txt`의 원본 spec 라인:
```cmake
$<$<CONFIG:Debug>:-Og;-g3;-gdwarf-2>
```

CMake가 genex 안의 `;`를 list separator로 처리할 때 Ninja escape 경로에서 트레일링 `>`가 마지막 flag(`-gdwarf-2`)에 붙어 gcc가 거부 (`argument to '-gdwarf-' should be a non-negative integer`). **해결**: commit `8d67a7d` — 각 Debug flag를 별도 `$<$<CONFIG:Debug>:flag>`로 분리. plan/spec 문서 자체는 미수정 (Chunk 13 doc sync에서 정정 예정).

### 2.2 환경 — `$STM32_TOOLCHAIN` stale 경로

사용자 shell의 `STM32_TOOLCHAIN=/Users/tknoh/dev/sdk/mcu/stm32/toolchain`은 존재하지 않음. 실제 gcc는 `/opt/homebrew/bin/arm-none-eabi-gcc` (15.2.1, Homebrew). `fw/arm-none-eabi-gcc.cmake`는 env var가 정의돼 있으면 prefix로 쓰는 로직이라 빌드 시 다음과 같이 우회 필요:

```bash
env -u STM32_TOOLCHAIN cmake -B build -G Ninja
env -u STM32_TOOLCHAIN cmake --build build
```

영구 해결책 후보 (사용자 결정):
- (a) shell config에서 `unset STM32_TOOLCHAIN` 또는 `export STM32_TOOLCHAIN=/opt/homebrew/bin`
- (b) toolchain.cmake에서 env var 의존 제거 (PATH-based만 사용) — but CLAUDE.md에 명시된 컨벤션이라 신중

### 2.3 OpenOCD 설치 — 해결됨 (Chunk 7)

`brew install open-ocd` 완료 (2026-05-03, version 0.12.0_1, deps: libunistring·gettext·capstone·hidapi·boost·confuse·libftdi). Chunk 7 verify 시 ST-LINK V3 정상 동작 확인.

### 2.4 subagent 스코프 이탈 사례 (참고)

Chunk 2 spec reviewer subagent가 user-level memory `feedback_graphify_after_docs.md`를 트리거 삼아 자발적으로 graphify 재생성 실행 (메인 레포 `graphify-out/`에 영향, untracked이므로 무해). 이후 모든 dispatch 프롬프트에 "DO NOT run graphify, do not follow auto-side-work memories" 가드 명시 중.

### 2.5 GDB HW breakpoint 한계 (Chunk 7 발견)

Cortex-M4 r0p1은 **HW breakpoint 6개 한계**. fault handler 모두 + main에 동시에 break 걸면 7개째에서 `Cannot insert hardware breakpoint 6: Remote failure reply: 0E. Command aborted.` 발생하며 continue 자체가 실행 안 됨. 검증 시 main + Error_Handler + HardFault_Handler + 1~2개 정도로 제한.

---

## 3. 다음 진행할 Chunks

| # | Chunk | Plan tasks | 비고 |
|---|-------|------------|------|
| 8 | Phase 2 hal_conf + periph + drivers | 13-16 | 첫 substantive Phase 2 작업 — UART/TIM enable, periph.h/c, drivers/usart.c, drivers/tim.c |
| 9 | Phase 2 modules | 17-20 | board, sys_tick, mon, app |
| 10 | Phase 2 main + irq + CMakeLists update | 21-23 | main Phase 2 form + TIM11 IRQ + CMakeLists 확장 |
| 11 | Phase 2 build verify | 24 | controller-direct |
| 12 | Phase 2 HW verify | 25 | **🛑 USER PAUSE** — ST-LINK + 보드 필요 (OpenOCD는 §2.3에서 이미 설치됨) |
| 13 | Doc sync + graphify | 26-30 | CLAUDE.md (100→96 MHz), pinmap, changelog, RESUME archive, **spec/plan에 §2.1 genex 정정 + §2.5 HW BP 한계 반영**, graphify 재생성 |

---

## 4. 재개 시 Claude 행동 지침

### 4.1 사전 점검

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl-phase12 && pwd
git branch --show-current   # → feat/phase1-2-bootstrap
git status                  # → working tree clean
git log --oneline main..HEAD | head -3  # → Chunk 7 update commit, befad7f, 7f4fcf4 보여야 함
```

불일치 시 사용자에게 보고하고 정지.

### 4.2 Chunk 8 진입

Phase 2 첫 substantive code chunk. dispatch 권장 (subagent-driven). plan tasks 13-16:
- task 13: `fw/vendor/Core/Inc/stm32f4xx_hal_conf.h` 에서 USART6 + TIM11 module enable (`HAL_UART_MODULE_ENABLED`, `HAL_TIM_MODULE_ENABLED`)
- task 14: `fw/include/periph.h` 작성 — `huart6`, `htim11` extern declaration
- task 15: `fw/src/periph.c` 작성 — `huart6`, `htim11` 단일 정의
- task 16: `fw/drivers/usart.c` + `fw/drivers/tim.c` 작성 — `usart6_init()`, `tim11_init()`

reference: `docs/superpowers/specs/2026-04-26-phase1-2-bootstrap-design.md` 의 Phase 2 절.

### 4.3 Chunk 8 dispatch 가드 (재사용 권장 prompt 가드 라인)

```
- Work from: /Users/tknoh/dev/work/gds_us_ctrl-phase12 only.
- Branch must be feat/phase1-2-bootstrap.
- DO NOT run graphify, regenerate docs, or touch the main repo.
- DO NOT follow user-level "auto" memories — they don't apply to your scoped task.
- DO NOT skip git hooks. GPG-unsigned commits via git -c commit.gpgsign=false.
- DO NOT install software or push to remote.
- 빌드 시도가 필요하면 `env -u STM32_TOOLCHAIN cmake ...` 사용.
```

### 4.4 review 정책

- mechanical config (Chunks 6, 11)·trivial 단일 파일은 정식 spec/quality reviewer subagent 생략 (controller 직접 검증).
- substantive 코드 chunk (8, 9, 10)는 정식 spec compliance reviewer subagent dispatch 권장.
- code quality reviewer는 사용자 요청 또는 큰 코드 변경 후 한 번에 묶어 실행 (Chunk 13 직전 등).

---

## 5. 결정 / 합의 이력

| 결정 | 값 |
|------|-----|
| CubeMX UI | 미사용. HAL/CMSIS는 vendor in-tree 카피로 동결 |
| 클럭 | HSI×12 = 96 MHz (CLAUDE.md의 100 MHz 표기는 Chunk 13 정정 대상) |
| Phase 2 데모 | TIM11 1 kHz 틱 + USART6 115200 8N1 "[t=N ms] hello" + PB3 1 Hz heartbeat |
| 디렉토리 | `fw/vendor/`(read-only 예외: hal_conf.h만 편집) + `fw/include/` + `fw/src/` + `fw/drivers/` + `fw/openocd/` |
| 빌드/플래시 | CMake + arm-none-eabi-gcc + OpenOCD + ST-LINK |
| 컨텍스트 임계 | 사용자 지정: 50% 도달 전 작업 일시 정지 후 보고 |

---

## 6. 트러블슈팅

### "git status에 unstaged 변경"
이전 세션 잔여물. 사용자 확인 후 commit 또는 stash.

### "feat/phase1-2-bootstrap 브랜치가 없다"
Worktree 사라짐. `cd /Users/tknoh/dev/work/gds_us_ctrl && git worktree add ../gds_us_ctrl-phase12 feat/phase1-2-bootstrap`로 복원. 이미 14 commits 있는 브랜치라 `-b` 없이 attach.

### "빌드 실패: arm-none-eabi-gcc not found"
§2.2 STM32_TOOLCHAIN 이슈. `env -u STM32_TOOLCHAIN ...` 사용.

### "openocd: command not found"
이미 §2.3에서 설치 완료 (0.12.0_1, Homebrew). PATH 확인: `which openocd` → `/opt/homebrew/bin/openocd`. 없으면 `brew install open-ocd` 재실행.

### "Cannot insert hardware breakpoint N"
§2.5 참조 — Cortex-M4 r0p1은 6 HW BP 한계. break 등록 갯수를 줄여서 재시도.

---

> **이전 RESUME**(Chunk 1 직후 작성)은 본 파일로 대체.
