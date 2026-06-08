# Handoff: Stage D slice 2b — RUN 명령 게이트 (코드 완료, HW 검증 대기)

**Generated**: 2026-06-08
**Branch**: `feat/stage-d-slice2b-run-gate` (main 미머지)
**Status**: ✅ **코드 완료 + 전체 cpp-reviewer APPROVED. 남은 것 = Task 3 실보드 HW 검증(보드 연결 시) → 최종 머지/태그.**

> 이 프로젝트의 1차 핸드오프 = `docs/superpowers/RESUME.md`(SessionStart hook 자동 로드) + `docs/NEXT_STEPS.md`. 본 `HANDOFF.md`는 보조(HW 검증 절차 상세). 응답 **한국어**(코드/식별자 영어). 워크스페이스 규칙: **코드는 요청한 부분만 수정**. graphify는 per-session 스킵(corpus 과대 — RESUME §환경).

## Goal

Stage D slice 2b: SAMD20 명령 FSM의 **RUN 게이트 절반**을 흡수해 slice 1/2a regulation core(부팅 auto-run)를 **터치 RUN 명령 구동**으로 전환(내부 호출로 연결, 원 inter-MCU `M_START` 대체). 터치 RUN = momentary hold-to-run. 부팅 = IDLE. OSC 물리 출력·SEEK/RESET 효과·overload·weld-cycle = slice 1/2a 동일 **deferred**.

## Completed (이번 세션, 3 코드 커밋)

- [x] **Task 1** `decfc28` — run FSM + `us_run_status` taxonomy + 명령 라우팅. `app_reg_command(us_cmd_t)` 신설, 부팅 IDLE, START `== US_IDLE` 엣지 re-arm, RUN_RELEASE `last_=max_` latch, enum `{IDLE,REMOTE,TOUCH,COMM}`(US_RUNNING 제거), sel MUX idle→0, ramp 활성 게이트. spec+cpp-reviewer APPROVED.
- [x] **Task 2** `ed2093f` — ICON_RUN 엣지 렌더(`app_lcd_disp_step` `prev_run_on`). spec APPROVED.
- [x] **Task 2 fix** `4264bab` — SYS_PIC_NOW mid-run 패널 리셋 시 `US_CMD_RUN_RELEASE` 발행(무한구동+아이콘 고착 해소, spec §4.3). cpp-reviewer 재리뷰 APPROVED.
- [x] **전체 통합 cpp-reviewer** — APPROVED(merge-ready, HW 대기). 빌드 0-warning(FLASH 28.64%/RAM 10.60%), 호스트 테스트 `all checks PASSED`.

## Not Yet Done

- [ ] **Task 3 — 실보드 HW 검증** (보드 연결 시, 사용자+보드). 절차 = ▼ Resume.
- [ ] 통과 → finishing-a-development-branch(머지/PR) + 태그 `hw-revA_fw-stage-d2b`(또는 합의 태그명).
- [ ] (별개, DEFERRED) SEEK/RESET regulation 효과·overload·weld-cycle·Modbus·OSC 구동(B-SEAM)·6b calibration.

## Key Decisions

| Decision | Rationale |
|----------|-----------|
| 부팅 IDLE (auto-run 폐기) | 원 SAMD20이 `M_START`를 명령 게이트. slice 2a auto-run은 명시적 테스트 단순화(spec §10 예고). |
| START 가드 `== US_IDLE` (samd20 `!= US_REMOTE`에서 분기) | M16 램프는 `M_START` 엣지구동. 통합 코드는 램프 직접 소유 → `!= US_REMOTE`면 패널 auto-repeat가 매 폴 ramp_counter=0 재시작→401 미도달. `== US_IDLE`이 TOUCH-only에서 안전+충실(advisor catch). |
| SYS_PIC_NOW mid-run 리셋 → RUN_RELEASE (Option A) | 패널 자체 리셋 시 held-RUN release 미도착 → 무한구동. UI 상실→액추에이터 정지가 안전 포스처(cpp-reviewer catch). SYS_PIC_NOW=0은 진짜 리셋만(page-0 splash). |
| faithful max/last latch (mirror collapse 폐기) | `us_off`(samd20 4180) 충실. `curr_power=sel`는 라벨된 setpoint 표면 유지(램프 가시, 6b에서 실측 전력 대체). |
| 신규 순수함수 없음 → 호스트 FSM 테스트 없음 | FSM이 `app_reg.c` static 상태에 묶임 + `us_cmd_t` 결합 비용 > 가치(YAGNI). 게이트 = 빌드+호스트 무회귀+HW(slice 1/2a와 동일). |

## Current State

**Working**: `feat/stage-d-slice2b-run-gate` = main(`396ec18`) + slice 2b 6 커밋(doc 3 + code 3). 빌드 0-warning, 호스트 PASS. main = slice 2a(`43fda87`, tag `hw-revA_fw-stage-d2`).

**Broken**: 없음.

**Uncommitted**: 이 핸드오프/doc 갱신 외 없음. `ref/atmega16/M16_reverse/`만 untracked(의도적).

## Files to Know

| File | 변경 |
|------|------|
| `docs/superpowers/specs/2026-06-08-stage-d-slice2b-run-gate-design.md` | 설계(§4.2 START 가드·§4.3 SYS_PIC_NOW) |
| `docs/superpowers/plans/2026-06-08-stage-d-slice2b-run-gate.md` | 플랜(Task 3 = HW 절차 + 합격기준 6항) |
| `fw/src/app_reg.c` | run FSM(`app_reg_command`)·MUX·max/last·publish |
| `fw/src/app_lcd.c` | 훅 → `app_reg_command` 라우팅 |
| `fw/src/app_lcd_disp.c` | ICON_RUN 엣지 렌더 |
| `fw/src/app_lcd_input.c` | SYS_PIC_NOW → RUN_RELEASE |

## Resume Instructions — Task 3 HW 검증

1. 보드 연결 확인: `ls /dev/cu.usbserial-* /dev/cu.usbmodem*` (mon=`/dev/cu.usbserial-BG02DMWU`@115200, ST-LINK=usbmodem). BOOT0=GND(평범한 reset run).
2. **트레이스 빌드 (CPU 플래그 포함 — 필수)**:
   ```bash
   cd fw && env -u STM32_TOOLCHAIN cmake -B build-trace -G Ninja \
     -DCMAKE_C_FLAGS="-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard -DREG_TRACE"
   env -u STM32_TOOLCHAIN cmake --build build-trace
   ```
   ⚠ CPU 플래그를 함께 안 주면 캐시 덮어써 ARM-mode 빌드 실패(slice 1 발견).
3. 플래시 + 모니터(단일 fd):
   ```bash
   openocd -f fw/openocd/stm32f410.cfg -c "program fw/build-trace/gds_us_ctrl.elf verify reset exit"
   DEV=$(ls /dev/cu.usbserial-* | head -1)
   { stty -f "$DEV" 115200 cs8 -parenb -cstopb raw -echo; cat; } < "$DEV" > /tmp/reg-mon.log &
   tr -d '\000' < /tmp/reg-mon.log | tr -s ' ' | grep '\['
   ```
4. **합격 기준** (plan Task 3 Step 3 표 = 권위, 6항):
   1. **부팅**: `[reg] run=0 st=0 … sel=0`, 출력 미구동, ICON_RUN off, 배너/네비 정상. **auto-ramp 없음**(의도).
   2. **RUN press(유지)**: `[lcd-hook] us_command=0` + `[reg] cmd=0 run=2` → `run=2 st=1` `rc` 0→401 ~4s + `sel` 128→1024 + **ICON_RUN 점등** + VAR_POWER 추종 → ~4s 후 `st=0`. (유지가 401 도달 = auto-repeat 재시작 버그 없음 검증)
   3. **RUN release**: `us_command=3` + `cmd=3 run=0` + **ICON_RUN 소등** + `sel=0` + VAR_POWER=last_power(latch).
   4. **re-arm**: 다시 press → `max_power` 0 → VAR_POWER 0에서 재상승 + 램프 재발화.
   5. **무회귀**: 활성 `st=0` lookup=slice-1; OSC PB2/PB10/PB14 idle-HIGH; LCD 네비/SETUP.
   6. **IDLE 출력 바**: IDLE에서 `LV_OUTPUT` 비움/안정(curr_amp idle 노이즈 플로어 확인, ≈0이면 정상 — final-review 노트).
5. 통과 → `rm -rf fw/build-trace`(git-ignored) → finishing-a-development-branch(머지/PR) + 태그.

## Warnings

- **`-DCMAKE_C_FLAGS` 단독 금지** — CPU 플래그 날아감. 모든 cmake에 `env -u STM32_TOOLCHAIN` 필수.
- `ref/atmega16/M16_reverse/` untracked 정상(삭제/커밋 임의 결정 금지).
- clangd/LSP 진단(`*.h not found`/`uint16_t unknown`)은 노이즈 — 게이트 = cmake 빌드 0-warning / `make -C fw/test test`.
- (pre-existing, 본 슬라이스 무관) `app_lcd_disp.c:24` "No callers yet — Task 9 wires…" 주석 stale(호출부 존재) — 후속 정리 후보.
