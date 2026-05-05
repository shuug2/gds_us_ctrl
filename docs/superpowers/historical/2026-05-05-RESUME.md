# RESUME — Phase 1+2 Bootstrap Implementation (Chunk 13부터)

> **중단 시각**: 2026-05-05 (Chunk 12 PASS 직후, Phase 2 HW 검증 완료 — banner + 1Hz hello + PB3 heartbeat 모두 관찰됨)
> **다음**: Chunk 13 — Doc sync + graphify 재생성 (CLAUDE.md 100→96 MHz, pinmap, changelog, spec/plan defect 정정, RESUME archive)
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
RESUME 읽고 Chunk 12부터 진행
```

SessionStart 훅이 본 파일 자동 로드. Chunk 13은 doc sync + graphify 재생성 — CLAUDE.md (100→96 MHz), pinmap, changelog, RESUME archive, spec/plan defect 정정 (§2.0/§2.1/§2.5), graphify 재실행 (controller-direct, 보드 불필요).

---

## 1. 작업 상태 스냅샷

### 1.1 위치 / 브랜치

| 항목 | 값 |
|------|-----|
| **Worktree** | `/Users/tknoh/dev/work/gds_us_ctrl-phase12/` |
| **Main repo** | `/Users/tknoh/dev/work/gds_us_ctrl/` (사용 ✗) |
| **Branch** | `feat/phase1-2-bootstrap` |
| **Base** | `main @ 73027c8` |
| **Tip** | (Chunk 11 RESUME update commit — 본 commit) |
| **Ahead of main** | 31 commits |

### 1.2 산출 문서

| 파일 | 역할 |
|------|------|
| `docs/superpowers/specs/2026-04-26-phase1-2-bootstrap-design.md` | 6 섹션 통합 설계 명세 |
| `docs/superpowers/plans/2026-04-26-phase1-2-bootstrap.md` | 30 task 구현 계획 |
| `docs/superpowers/RESUME.md` | **본 파일** |

### 1.3 완료된 Chunks (1–12)

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
| — | RESUME Chunk 7 PASS | `c42e1fd` |
| 8 | Phase 2 periph + drivers (tasks 13-16) | `463a772`, `b85c81f`, `988ac88`, `9ed1da6` |
| — | RESUME Chunk 8 PASS | `fbebde7` |
| 9 | Phase 2 modules (tasks 17-20) | `7c1e6ca`, `d9149f5`, `6ba6094`, `e20cd0c` |
| — | RESUME Chunk 9 PASS | `159b094` |
| 10 | Phase 2 통합 (tasks 21-23) | `de03986`, `fdf04aa`, `a2a2d68` |
| — | RESUME Chunk 10 PASS | `190e890` |
| — | **spec defect fix** (TIM_AUTORELOAD_PRELOAD typo) | `de0c35f` |
| 11 | **Phase 2 build verify ✅ (PASS)** | (no commit; controller-direct) |
| — | RESUME Chunk 11 PASS | `59d6e66` |
| 12 | **Phase 2 HW verify ✅ (PASS)** | (no commit; controller-direct) |

**Phase 1 빌드 결과**: FLASH 3860 B (2.94%), RAM 1584 B (4.83%), elf/bin/hex/map 4 산출물 정상.

**Phase 2 빌드 결과** (Chunk 11, 2026-05-03): FLASH **22060 B (16.83%)** / 128 KB, RAM **2520 B (7.69%)** / 32 KB. text=21572 / data=476 / bss=2052. elf/bin/hex/map 4 산출물 정상. 핵심 심볼 (`main`, `app_init`, `app_loop_iter`, `tim11_init`, `usart6_init`, `board_init`, `sys_tick_init`, `sys_tick_handle_irq`, `TIM1_TRG_COM_TIM11_IRQHandler`, `HAL_TIM_PeriodElapsedCallback`) 모두 .text 위치 확인. Linker 경고: nano libc stubs (`_close/_fstat/_getpid/...`) — `--gc-sections`으로 strip 예상; RWX LOAD segment — 임베디드 ELF 표준.

**Phase 1 HW 검증 결과** (Chunk 7, 2026-05-03):
- ST-LINK V3J15M7B5S1 (API v3, VID:PID 0483:374F), Target Vcc 3.28 V
- Cortex-M4 r0p1, 6 HW BP / 4 watchpoints
- Reset_Handler 진입 OK (PC=0x080003b4, MSP=0x20008000=SRAM top 32KB)
- HAL_Init + clock_init 통과 (Error_Handler / HardFault 미히트)
- main `while(1) __NOP()` 도달 확인 (PC=0x80002ea = main+10)
- **SystemCoreClock = 96,000,000 Hz** (HSI×12 PLL lock 확인)

**Phase 2 HW 검증 결과** (Chunk 12, 2026-05-05):
- Flash: `openocd -f fw/openocd/stm32f410.cfg -c "program ... verify reset exit"` — Programming Finished + Verified OK + Resetting Target. Device ID 0x10006458, flash size 128 KiB.
- 칩 상태 (gdb attach + `monitor halt` 후 reg/mem 직접 dump): PC = `app_loop_iter+2` (0x0800385e), xPSR=0x81000000 (Thread mode, fault 없음), MSP=0x20007FF0. CFSR=0, HFSR=0 → fault 진입 흔적 없음.
- TIM11 1 kHz 틱: `s_ms` 1.2초 동안 69167 → 70388 (+1221) → **약 1018 Hz** (gdb halt overhead 포함, 정상 범위).
- PB3 heartbeat: GPIOB ODR(0x40020414) 250ms 간격 8회 샘플 = `0,0,8,8,8,8,0,0` → bit 3 토글 패턴 관찰. 사용자 직접 LED 점멸 확인 ✅.
- USART6: SR=0x000000C0 (TC=1, TXE=1 → 송신 완료), BRR=0x341 (96 MHz/833 = 115 246 baud, 목표 대비 ~0.04% 오차), CR1=0x200C (UE+TE+RE), GPIOC MODER bits[15:12]=AF, AFRL bits[31:24]=AF8 → PC6/PC7 USART6 mux 정확. 사용자 시리얼 터미널로 banner `[boot] gds_us_ctrl phase2 ready` + 1 Hz `[t=N ms] hello` 직접 관찰 확인 ✅.
- RCC: AHB1ENR=0x00000006 (GPIOB+GPIOC 클럭 ON), APB2ENR=0x00048020 (USART6 + SYSCFG + TIM11 클럭 ON).

---

## 2. 발견된 이슈 (다음 세션이 알아야 함)

### 2.0 spec/plan 결함 — `TIM_AUTORELOAD_PRELOAD_DISABLE` 매크로 오타 (해결됨, Chunk 11)

plan task 16 본문 + spec Phase 2 절에 `TIM_AUTORELOADPRELOAD_DISABLE` (no underscore between AUTORELOAD and PRELOAD) 표기. vendor HAL 실제 매크로는 `TIM_AUTORELOAD_PRELOAD_DISABLE` (밑줄 분리). gcc reject (`'TIM_AUTORELOADPRELOAD_DISABLE' undeclared`). **해결**: commit `de0c35f` — `fw/drivers/tim.c` 한 줄 수정. plan/spec 본문은 미수정 (Chunk 13 doc sync에서 정정 예정).

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
| 13 | Doc sync + graphify | 26-30 | CLAUDE.md (100→96 MHz), pinmap, changelog, RESUME archive, **spec/plan에 §2.0 AUTORELOAD_PRELOAD 정정 + §2.1 genex 정정 + §2.5 HW BP 한계 반영**, graphify 재생성. controller-direct, 보드 불필요. |

---

## 4. 재개 시 Claude 행동 지침

### 4.1 사전 점검

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl-phase12 && pwd
git branch --show-current   # → feat/phase1-2-bootstrap
git status                  # → working tree clean
git log --oneline main..HEAD | head -10  # → Chunk 11 RESUME update + de0c35f (AUTORELOAD fix) 190e890 a2a2d68 fdf04aa de03986 ...
```

불일치 시 사용자에게 보고하고 정지.

### 4.2 Chunk 13 진입 (controller-direct, 보드 불필요)

작업 항목 (plan tasks 26–30):
- `CLAUDE.md` `100 MHz` → `96 MHz` (Phase 1 HW verify에서 PLL=HSI×12=96 MHz 확정)
- `docs/pinmap.md` USART6 / TIM11 / PB3 heartbeat 등 Phase 2 신규 핀 사용 반영
- `docs/changelog.md` Phase 1+2 bootstrap 완료 항목 추가 (Chunks 1–12 요약)
- `docs/superpowers/specs/2026-04-26-phase1-2-bootstrap-design.md` defect 정정:
  - §2.0 `TIM_AUTORELOADPRELOAD_DISABLE` → `TIM_AUTORELOAD_PRELOAD_DISABLE`
  - §2.1 CMake genex Debug flags semicolon 처리 정정
  - §2.5 Cortex-M4 r0p1 6 HW BP 한계 메모 추가
- `docs/superpowers/plans/2026-04-26-phase1-2-bootstrap.md` 상응 정정
- `docs/superpowers/RESUME.md` archive (`docs/superpowers/historical/` 또는 main repo의 stale pointer만 유지)
- `graphify-out/` 재생성 (메인 레포에서 `/graphify` 실행, worktree 변경사항 반영)

### 4.3 Chunk 13 dispatch 가드

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
- substantive 코드 chunk (8 ✅, 9, 10)는 정식 spec compliance reviewer subagent dispatch 권장.
- code quality reviewer는 사용자 요청 또는 큰 코드 변경 후 한 번에 묶어 실행 (Chunk 13 직전 등).

**Chunk 8 review 결과** (2026-05-03): PASS. 4 commits, 5 files (hal_conf modify + periph.h/c + usart.c + tim.c), 모두 plan verbatim 일치. TIM11 prescaler 95 + period 999 → 1 kHz @ 96 MHz 수치 검증 완료. 단일 정의 디시플린(`huart6`/`htim11` in periph.c only) 유지.

**Chunk 9 review 결과** (2026-05-03): PASS. 4 commits, 8 새 파일 (board.h/c, sys_tick.h/c, mon.h + drivers/mon_usart6.c, app.h/c). 모두 plan verbatim. `CTRL_OSC_PINS` PIN_11 의도적 제외 확인, `s_ms` volatile + Cortex-M4 atomic read 디시플린, mon CRLF 라인 종결, app cadence wraparound-safe `(uint32_t)(now - prev) >= 1000`. TIM SR flag clear는 Chunk 10 IRQ wrapper 책임으로 deferred (correct).

**Chunk 10 review 결과** (2026-05-03): PASS-WITH-NOTES. 3 commits, 3 modified files (main.c overwrite, irq.c append, CMakeLists 3-edit). 초기화 순서 `HAL_Init → clock_init → usart6_init → tim11_init → board_init → app_init` 검증, IRQ symbol `TIM1_TRG_COM_TIM11_IRQHandler`이 vendor startup file weak alias와 일치 확인, `-u _printf_float` 링크 옵션 spec 준수 (defensive — `mon_printf` future 사용 대비). **Non-blocking note**: `fw/CMakeLists.txt:68` 주석 `# 어플리케이션 — Phase 1 sources`가 GLOB 변경 후 stale — Chunk 13 doc sync 시 정정 권장 (plan 범위 외).

**Chunk 11 결과** (2026-05-03): PASS. clean build (`rm -rf fw/build` → cmake configure → ninja build). 빌드 중 spec defect 1건 발견 (§2.0 AUTORELOAD_PRELOAD 매크로 오타) — controller-direct fix commit `de0c35f`로 봉합 후 재빌드 클린. 산출물 + 심볼 + 메모리 사용량 모두 §1.3 Phase 2 빌드 결과 참조.

**Chunk 12 결과** (2026-05-05): PASS. controller-direct, no commits (HW verify only). 5월 3일 빌드 산출물(elf 882684 B, 22060 B 코드) 그대로 flash. ST-LINK V3 (`/dev/cu.usbmodem14203`) + USB-시리얼 어댑터 (`/dev/cu.usbserial-AB0MLYXA`, FTDI). gdb register cache 함정: `info registers pc`는 stale, **`monitor reg pc` 또는 `monitor halt` 직후 출력만 신뢰**. 진단 흐름은 §1.3 Phase 2 HW 검증 결과 참조 — banner / 1Hz hello / PB3 heartbeat / no fault 4건 모두 확인.

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
