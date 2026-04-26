# RESUME — Phase 1+2 Bootstrap Implementation (Chunk 2부터)

> **중단 시각**: 2026-04-26 (Chunk 1 완료 직후)
> **재개 예정**: 4시간 후 (사용자가 새 세션 시작)
> **스킬 흐름**: `superpowers:subagent-driven-development` (Chunk별 fresh subagent + 2-stage review)

---

## ⚡ 빠른 재개 절차 (사용자)

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl-phase12   # ← 반드시 worktree 디렉토리
claude
```

새 세션 시작 후 다음 한 줄 입력:

```
RESUME 읽고 Chunk 2부터 subagent-driven-development로 이어서 진행
```

SessionStart 훅이 본 파일을 자동 로드. Claude는 본 RESUME 컨텍스트만으로 Chunk 2 implementer subagent를 즉시 dispatch합니다.

---

## 1. 작업 상태 스냅샷

### 1.1 위치 / 브랜치

| 항목 | 값 |
|------|-----|
| **Worktree (작업 위치)** | `/Users/tknoh/dev/work/gds_us_ctrl-phase12/` |
| **Main repo** | `/Users/tknoh/dev/work/gds_us_ctrl/` (이 위치에서 cd 하지 않을 것) |
| **Branch** | `feat/phase1-2-bootstrap` |
| **Base** | `main @ 73027c8` (plan/spec commit) |

### 1.2 산출 문서

| 파일 | 역할 |
|------|------|
| `docs/superpowers/specs/2026-04-26-phase1-2-bootstrap-design.md` | 6 섹션 통합 설계 명세 (브레인스토밍 산출) |
| `docs/superpowers/plans/2026-04-26-phase1-2-bootstrap.md` | 30 task 구현 계획 (chunk별로 묶어 dispatch) |
| `docs/superpowers/RESUME.md` | **본 파일** — 재개 컨텍스트 |

### 1.3 완료된 Chunks

| # | Chunk | 상태 | Commits |
|---|-------|------|---------|
| 1 | Vendor seed import (plan tasks 1-2) | ✅ DONE | `c8eaac6`, `de37164`, `26d35d1` |

`git log --oneline main..HEAD` (worktree에서 실행 시 위 3개 commit + 새로 추가될 commit들 보임).

### 1.4 외부 의존 자산 상태

- **STM32CubeF4 SDK**: `/tmp/STM32CubeF4` (v1.27.1, 4시간 전 clone). 4시간 후 살아있을 가능성 높음. 만약 사라졌으면 다음으로 재clone:
  ```bash
  cd /tmp && git clone --depth 1 --branch v1.27.1 https://github.com/STMicroelectronics/STM32CubeF4.git
  ```
  Chunk 2 이후로는 SDK 의존성이 거의 없음 (vendor 카피본은 이미 worktree에 들어옴).

- **HW (ST-LINK + 보드 + USB-Serial)**: **미연결**. Chunk 7(Phase 1 HW verify)과 Chunk 12(Phase 2 HW verify)에서 일시정지 예정.

---

## 2. 다음 진행할 Chunks (트래킹 task #2~#13)

| # | Chunk | Plan tasks | 비고 |
|---|-------|------------|------|
| 2 | Build infrastructure | 3-5 | **다음 dispatch 대상**: 툴체인 cmake + .gitignore + hal_conf.h 편집 |
| 3 | Phase 1 source files | 6-8 | clock.h/c, irq.c, main.c (Phase 1 form) |
| 4 | Phase 1 CMakeLists | 9 | HAL lib + main/clock/irq + flash targets |
| 5 | Phase 1 build verify | 10 | **controller-direct (subagent ✗)** |
| 6 | OpenOCD config | 11 | stm32f410.cfg + debug.gdb |
| 7 | Phase 1 HW verify | 12 | **🛑 USER PAUSE — HW 연결 후 진행** |
| 8 | Phase 2 hal_conf + periph + drivers | 13-16 | UART/TIM enable, periph.h/c, drivers/usart.c, drivers/tim.c |
| 9 | Phase 2 modules | 17-20 | board, sys_tick, mon, app |
| 10 | Phase 2 main + irq + CMakeLists update | 21-23 | main Phase 2 form + TIM11 IRQ + CMakeLists 확장 |
| 11 | Phase 2 build verify | 24 | **controller-direct** |
| 12 | Phase 2 HW verify | 25 | **🛑 USER PAUSE** |
| 13 | Doc sync + graphify | 26-30 | CLAUDE.md, pinmap, changelog, RESUME archive, graphify 재생성 |

---

## 3. 재개 시 Claude가 따를 절차

1. **사전 점검** (한 줄씩 Bash 실행):
   ```bash
   cd /Users/tknoh/dev/work/gds_us_ctrl-phase12 && pwd
   git branch --show-current   # → feat/phase1-2-bootstrap 이어야 함
   git status                  # → working tree clean 이어야 함
   git log --oneline main..HEAD | head -5  # → c8eaac6, de37164, 26d35d1 보여야 함
   ls /tmp/STM32CubeF4 >/dev/null 2>&1 && echo "SDK present" || echo "SDK GONE — re-clone needed"
   ```
   불일치 발견 시 사용자에게 보고하고 정지.

2. **Task 트래킹 재구성**: TaskCreate로 task #2~#13(Chunk 2~13)을 새로 생성 (이전 세션의 task ID는 새 세션에서 사라짐). Chunk 2를 in_progress로 표시.

3. **Chunk 2 implementer subagent dispatch**: 본 RESUME §4의 prompt 사용.

4. **2-stage review** (spec compliance → code quality) 후 다음 Chunk로 진행.

5. Chunk 7 (Phase 1 HW verify)에 도달하면 사용자에게 **"ST-LINK + 보드 연결됐나요?"** 질문, 미연결 시 작업 일시 정지.

---

## 4. Chunk 2 implementer dispatch prompt (다음 세션이 즉시 사용)

```
You are implementing Chunk 2 of a STM32 firmware bootstrap plan: build infrastructure (toolchain CMake, .gitignore, HAL module selection). This is plan tasks 3-5.

## Working directory & branch (CRITICAL)

- Work from: `/Users/tknoh/dev/work/gds_us_ctrl-phase12/` (git worktree, NOT main repo)
- Branch: `feat/phase1-2-bootstrap`
- Verify before starting: `cd /Users/tknoh/dev/work/gds_us_ctrl-phase12 && git branch --show-current` should print `feat/phase1-2-bootstrap`. If not, STOP and report BLOCKED.

## Project context

Reference docs (read if needed):
- Plan: `docs/superpowers/plans/2026-04-26-phase1-2-bootstrap.md` — Tasks 3, 4, 5 are this chunk's scope
- Spec: `docs/superpowers/specs/2026-04-26-phase1-2-bootstrap-design.md` (§5.1 toolchain, §1.2 hal_conf modules)

Status: Chunk 1 (vendor seed) is done. `fw/vendor/` has the full HAL/CMSIS/startup/linker. `fw/vendor/Core/Inc/stm32f4xx_hal_conf.h` is currently the unedited ST template.

## What to produce

### Plan Task 3: arm-none-eabi-gcc.cmake (toolchain file)
Create `fw/arm-none-eabi-gcc.cmake` with content from plan Task 3 Step 1 (cross-compiler paths, Cortex-M4F flags). Verify `arm-none-eabi-gcc --version` works (install warning if not). Commit.

### Plan Task 4: .gitignore
Create `fw/.gitignore` with `build/`, `*.map`, `*.bin`, `*.hex`, `*.elf`, `*.su`. Commit.

### Plan Task 5: hal_conf.h Phase 1 module selection
Edit `fw/vendor/Core/Inc/stm32f4xx_hal_conf.h`:
- Enable: HAL_MODULE_ENABLED, HAL_RCC_MODULE_ENABLED, HAL_GPIO_MODULE_ENABLED, HAL_DMA_MODULE_ENABLED, HAL_CORTEX_MODULE_ENABLED, HAL_FLASH_MODULE_ENABLED, HAL_PWR_MODULE_ENABLED
- Disable (comment out) all other HAL_*_MODULE_ENABLED including ADC/CAN/CRC/DCMI/DAC/DFSDM/I2C/I2S/IWDG/LPTIM/RNG/RTC/SD/SMARTCARD/SPI/TIM/UART/USART/WWDG (Phase 2/Stage A에서 점진 활성화)
- Set TICK_INT_PRIORITY to 0 (HAL SysTick preempts application IRQs)
- Leave USE_FULL_ASSERT commented out
Commit with message describing module selection rationale.

## Self-review

- 3 commits exist on `feat/phase1-2-bootstrap` from this work (toolchain, gitignore, hal_conf)
- `arm-none-eabi-gcc.cmake` exists in `fw/`
- `.gitignore` exists in `fw/`
- `hal_conf.h` has only the 7 specified modules enabled
- TICK_INT_PRIORITY = 0
- working tree clean

Report DONE | DONE_WITH_CONCERNS | BLOCKED with commit SHAs and any deviations.
```

---

## 5. 환경 확인 사항 (선택 — 4시간 후 변경 가능성 있는 것)

| 항목 | 현재 값 | 4시간 후 변동 가능성 |
|------|---------|-------------------|
| `/tmp/STM32CubeF4` | exists | 시스템 재시작 시 사라짐 |
| `arm-none-eabi-gcc` PATH | (확인 안 됨) | 변동 없음 |
| ST-LINK + 보드 연결 | 미연결 | 사용자 판단 |
| Memory `/Users/tknoh/.claude/projects/-Users-tknoh-dev-work-gds-us-ctrl/memory/` | feedback_graphify_after_docs.md, project_gds_us_ctrl.md, MEMORY.md 존재 | 영구 |

---

## 6. 결정 / 합의 이력 (요약 — spec 미참조 상황 대비)

이 부분이 명확하지 않으면 spec을 직접 읽으세요. 다음은 핵심 5개:

| 결정 | 값 |
|------|-----|
| CubeMX UI | **미사용**. UI 호출 ✗. HAL/CMSIS는 vendor in-tree 카피로 동결 |
| 클럭 | **HSI×12 = 96 MHz**. CLAUDE.md의 100 MHz 표기는 정정 대상 (Chunk 13 doc sync) |
| Phase 2 데모 | **TIM11 1 kHz 틱 + USART6 115200 8N1 "[t=N ms] hello" + PB3 1 Hz heartbeat** |
| 디렉토리 | `fw/vendor/`(read-only) + `fw/include/` + `fw/src/` + `fw/drivers/` + `fw/openocd/` |
| 빌드/플래시 | CMake + arm-none-eabi-gcc + OpenOCD + ST-LINK |

---

## 7. 트러블슈팅 (재개 시 자주 마주칠 상황)

### "git status에 unstaged 변경이 보인다"
이전 세션의 미커밋 잔여물. 의도된 변경인지 사용자에게 확인 후 commit 또는 stash.

### "feat/phase1-2-bootstrap 브랜치가 없다"
Worktree가 사라졌을 가능성. 다음으로 재생성:
```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
git worktree add ../gds_us_ctrl-phase12 -b feat/phase1-2-bootstrap
```
이 경우 Chunk 1 commits(`c8eaac6` 등)는 **사라짐** — Chunk 1부터 다시 dispatch 필요.

### "/tmp/STM32CubeF4 사라짐"
재clone (RESUME §1.4 명령). Chunk 2 이후로는 사용 안 함.

---

> ✨ **자동화**: `.claude/settings.json` SessionStart 훅이 본 파일을 자동 로드.
> 훅이 동작 안 할 시 `/hooks` 명령으로 재로드 또는 본 파일 수동 열람.

> **이전 RESUME** (브레인스토밍 중단용)은 본 파일로 대체됨. 본 파일은 implementation 중단/재개용.
