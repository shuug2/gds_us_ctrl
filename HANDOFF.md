# Handoff: Stage D slice 2a — 상태머신 + soft-start 램프 (코드 완료, HW 검증 대기)

**Generated**: 2026-06-03 (updated)
**Branch**: `feat/stage-d-slice2-softstart` (main 미머지, tip `ae24ec4`)
**Status**: Code-complete (Task 1~3) — **Task 4 HW 검증만 남음(보드 필요)**

> 이 프로젝트의 1차 핸드오프는 `docs/superpowers/RESUME.md`(SessionStart hook 자동 로드) + `docs/NEXT_STEPS.md`. 본 `HANDOFF.md`는 보조. 응답은 **한국어**(코드/식별자 영어). 워크스페이스 규칙: **코드는 요청한 부분만 수정, 그 외 건드리지 않음.**

## Goal

Stage D slice 2a: M16 부팅 **soft-start 램프**(출력 ~4s 누진 상승 후 정상 레귤레이션으로 핸드오프) + `g_main_state` 상태머신을 STM32 슈퍼루프에 compute로 흡수. OSC 물리 출력은 slice 1과 동일 **deferred**.

## Completed (이번 세션)

- [x] **Task 1** `d98b025` — 순수 `reg_ramp_level`(10-rung thermometer fill) + 호스트 테스트. TDD RED→GREEN, `all checks PASSED`. spec PASS + cpp-reviewer APPROVED.
- [x] **Task 2** `ba67971` — `enum {US_IDLE=0,US_RUNNING=1}` → `app_lcd.h`, `app_lcd_disp.c` 로컬 define 제거. 빌드 0-warning. PASS.
- [x] **Task 3** `ae24ec4` — `app_reg.c` 상태머신(`main_state` init=1)+10ms 램프 cadence(`prev_ramp_ms`, ≥401→state0)+sel MUX(`state==1?reg_ramp_level:reg_scale`)+`us_run_status=US_RUNNING`+REG_TRACE(`st/rc/sel`). 빌드 0-warning. spec PASS + cpp-reviewer APPROVED(off-by-one/무회귀/시간델타 wraparound 안전 확인).
- 펌웨어 빌드 **0-warning** (FLASH 28.52%/RAM 10.57%), 호스트 테스트 `all checks PASSED`.

## Not Yet Done

- [ ] **Task 4 — HW 검증** (보드 필요, 전압주입 불필요). 플랜 `docs/superpowers/plans/2026-06-03-stage-d-slice2-softstart-ramp.md` Task 4.
- [ ] (Task 4 통과 후) 최종 cpp-reviewer 1회 → changelog/RESUME "done" → finishing-a-development-branch(머지/PR) + 태그 `hw-revA_fw-stage-d2`(또는 합의 태그명).
- [ ] (별개, HW 준비 후) **6b 신호 calibration**: `>>2` 정규화·2.56V↔3.3V 도메인·물리단위·B-OSC-MAP. 전압 가변 + 실 초음파 구동 필요.

## Failed Approaches (Don't Repeat These)

- **REG_TRACE 트레이스 빌드**: `cmake ... -DCMAKE_C_FLAGS="-DREG_TRACE"`는 캐시를 덮어써 툴체인 CPU 플래그를 날려 ARM-mode 빌드 실패. **CPU 플래그를 함께 전달**(아래 Resume §Task 4). (slice 1에서 발견·정정 `879f43e`.)

## Key Decisions

| Decision | Rationale |
|----------|-----------|
| sel MUX 통일 | `sel = state==1 ? reg_ramp_level(ctr) : reg_scale(ch0_avg)` → state==0 경로가 slice-1 verbatim = 무회귀 |
| 램프 레벨 LCD bar 미러(curr_power=sel) | 전압주입 불가 → 패널 육안 검증. **라벨된 테스트 deviation**(M16은 LCD 미구동) |
| `reg_ramp_level`=thermometer fill `{128..1024 포화}` | M16 g_019F 0x01→0xFF(recon :249-258). 정확한 패턴바이트→OSC는 deferred(B-SEAM) |
| us_run_status=US_RUNNING 상시 | 부팅 auto-run(state init=1), stop 명령은 slice 2b. taxonomy(REMOTE/TOUCH/COMM)도 2b |
| 코멘트 minor 제안 미반영 | reviewer APPROVED(비차단) + 워크스페이스 "요청한 부분만 수정" 규칙 |

## Current State

**Working**: `feat/stage-d-slice2-softstart`(tip `ae24ec4`) = main + slice 2a 코드 3커밋. 빌드 0-warning, 호스트 테스트 PASS. main(`5aea06f`)=slice 1, tag `hw-revA_fw-stage-d`.

**Broken**: 없음.

**Uncommitted**: 이 핸드오프/doc 갱신 커밋 외 없음. `ref/atmega16/M16_reverse/`만 untracked(의도적, 분석 ground-truth).

## Files to Know

| File | Why |
|------|-----|
| `docs/superpowers/plans/2026-06-03-stage-d-slice2-softstart-ramp.md` | 플랜 (Task 4 HW 절차 = 완전한 명령) |
| `docs/superpowers/specs/2026-06-02-stage-d-slice2-softstart-ramp-design.md` | 설계 근거 |
| `fw/src/app_reg.c` | 상태머신/cadence/MUX (Task 3) |
| `fw/src/app_reg_calc.c` `:reg_ramp_level` | 순수 램프 함수 (Task 1) |

## Resume Instructions — Task 4 HW 검증

1. 보드 연결 확인: `ls /dev/cu.usbserial-* /dev/cu.usbmodem*` (mon=`/dev/cu.usbserial-BG02DMWU`@115200, ST-LINK=usbmodem). BOOT0는 GND 결선(평범한 reset run).
2. **트레이스 빌드 (CPU 플래그 포함 — 필수)**:
   ```bash
   cd fw && env -u STM32_TOOLCHAIN cmake -B build-trace -G Ninja \
     -DCMAKE_C_FLAGS="-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard -DREG_TRACE"
   env -u STM32_TOOLCHAIN cmake --build build-trace
   ```
3. 플래시 + 모니터(단일 fd):
   ```bash
   openocd -f fw/openocd/stm32f410.cfg -c "program fw/build-trace/gds_us_ctrl.elf verify reset exit"
   DEV=$(ls /dev/cu.usbserial-* | head -1)
   { stty -f "$DEV" 115200 cs8 -parenb -cstopb raw -echo; cat; } < "$DEV" > /tmp/reg-mon.log &
   tr -d '\000' < /tmp/reg-mon.log | tr -s ' ' | grep '\['
   ```
4. **합격 기준**: 부팅 시 `[reg] st=1 rc=… sel=… band=…` → `sel` 128→1024 / `band` 18→0 ~4s 단조 + **LCD 출력 bar 상승 육안** + us_run_status running 아이콘 → ~4s 후 `st=0` 전환, 이후 idle이면 `sel=0/band=21`(slice-1 거동) + bar 복귀. 무회귀: 배너 정상 + LCD 네비 + (slice 1) PB2/PB10/PB14 idle-HIGH.
   - ARM-mode 빌드 에러 시: CPU 플래그 누락(Failed Approaches).
5. 통과 → `rm -rf fw/build-trace`(git-ignored, 커밋 안 함) → 최종 cpp-reviewer → finishing-a-development-branch.

## Warnings

- **`-DCMAKE_C_FLAGS` 단독 금지** — CPU 플래그 날아감(Failed Approaches). 모든 cmake에 `env -u STM32_TOOLCHAIN` 필수.
- `ref/atmega16/M16_reverse/` untracked는 정상(삭제/커밋 임의 결정 금지).
- 부팅 auto-run(state init=1)은 slice 2a 의도 동작(START 명령 re-arm은 slice 2b). 플랜 §10 flag.
- clangd/LSP 진단(`*.h not found`/`uint16_t unknown`)은 노이즈 — 게이트는 `make -C fw/test test` / cmake 빌드.
