# Handoff: Stage D slice 2b — RUN 명령 게이트 (코드 완료, RUN HW 재검증 대기)

**Generated**: 2026-06-08 (HW session)
**Branch**: `feat/stage-d-slice2b-run-gate` (main 미머지, tip `b78cbbf`)
**Status**: ✅ **코드 완료 + 통합 cpp-reviewer APPROVED.** HW 검증 일부 PASS(부팅 IDLE / RESET·SEEK 라우팅). **RUN은 V30 에셋 quirk로 막혀 펌웨어 fix `b78cbbf` 적용 — RUN/ICON_RUN/램프 재검증은 보드 재연결 후.** fix `b78cbbf`는 cpp-reviewer 미수행.

> 1차 핸드오프 = `docs/superpowers/RESUME.md`(SessionStart 자동 로드) + `docs/NEXT_STEPS.md`. 본 `HANDOFF.md`는 보조(HW 절차 상세). 응답 **한국어**(코드/식별자 영어). 워크스페이스: **코드는 요청한 부분만 수정**. graphify 미사용(2026-06-10 사용 중단, 산출물 삭제).

## Goal

SAMD20 명령 FSM의 **RUN 게이트**를 흡수해 slice 1/2a regulation core(부팅 auto-run)를 **터치 RUN 명령 구동**으로 전환. 터치 RUN = momentary hold-to-run. 부팅 = IDLE. OSC 물리출력·SEEK/RESET 효과·overload·weld-cycle = deferred.

## Completed (코드 — 6 커밋)

- [x] **Task 1** `decfc28` — run FSM + `app_reg_command` 라우팅 + 부팅 IDLE + START `==US_IDLE` 엣지 가드(advisor) + enum taxonomy(US_RUNNING 제거) + sel MUX idle→0. spec+cpp-reviewer APPROVED.
- [x] **Task 2** `ed2093f` — ICON_RUN 엣지 렌더(`app_lcd_disp_step`, app_reg DGUS-free). spec APPROVED.
- [x] **Task 2 fix** `4264bab` — SYS_PIC_NOW mid-run 패널 리셋 → `US_CMD_RUN_RELEASE`(무한구동+아이콘 고착 해소, spec §4.3). cpp-reviewer APPROVED.
- [x] **전체 통합 cpp-reviewer** — APPROVED. 빌드 0-warning, 호스트 `all checks PASSED`.
- [x] **RUN fix** `b78cbbf` — V30 RUN 버튼 `KEY_MULTI data=0` 매핑(spec §4.4, `app_lcd_input.c` 한 파일). **⚠ cpp-reviewer 미수행** — 다음 세션서 리뷰.

## HW 검증 세션 결과 (2026-06-08, 부분)

REG_TRACE+LCD_TRACE_RX 빌드 플래시(ST-LINK V3) + SWD g_measure read + USART6 mon trace.
- ✅ **부팅 IDLE**: `[reg] run=0 st=0 rc=0 sel=0 band=21`, auto-ramp 없음(SWD `us_run_status=0` 1초후 불변). = slice 2a auto-run 폐기 확인.
- ✅ **명령 라우팅**: RESET → `[lcd-hook] us_command=2` `[reg] cmd=2 run=0`; SEEK → `us_command=1` `cmd=1 run=0`. **hook→app_reg_command HW 확인**, RESET/SEEK no-op(spec §9) 정상.
- ⏸ **RUN 막힘 → 원인규명 → fix**: live `[lcd] rx` 트레이스로 **V30 RUN 버튼이 `KEY_MULTI(0x1080)` 키값 0을 press·release 양쪽에 반환**(RESET=1/SEEK=2는 정상; data=0은 RUN 고유) 발견. `handle_key_multi`(samd20 포팅)는 data=3/4만 처리 → RUN 명령 미발화(LCD 스테이지 버그A와 동일 = **V30 에셋 root**). 에셋 점검: `hw/lcd/dgus/13TouchFile.bin`(컴파일 출력, 편집 DWIN 프로젝트 repo 부재). **사용자 결정 = 펌웨어 적응(Option B)** → fix `b78cbbf`.
- ⏸ **RUN/ICON_RUN/램프/latch/re-arm = 미검증**(fix 적용 후 보드 USB 분리됨).

## Not Yet Done (다음 세션)

- [ ] **보드 재연결 → fix `b78cbbf` HW 재검증**(RUN press→START→램프+ICON_RUN→release→정지+latch→re-arm). 절차 = ▼ Resume.
- [ ] fix `b78cbbf` **cpp-reviewer**(spec compliance + quality) — 다른 커밋은 리뷰 완료.
- [ ] 통과 → finishing-a-development-branch(머지/PR) + 태그 `hw-revA_fw-stage-d2b`.
- [ ] (DEFERRED) SEEK/RESET regulation 효과·overload·weld-cycle·Modbus·OSC 구동(B-SEAM)·6b. (후속) 에셋 정상화(A): DWIN 툴서 RUN 키 press=3/release=4 — 가용 시.

## Key Decisions

| Decision | Rationale |
|----------|-----------|
| 부팅 IDLE (auto-run 폐기) | 원 SAMD20이 M_START를 명령 게이트. slice 2a auto-run = 테스트 단순화. |
| START 가드 `== US_IDLE` | M16 램프 = M_START 엣지구동. `!= US_REMOTE`면 패널 auto-repeat가 램프 재시작(advisor catch). |
| SYS_PIC_NOW mid-run → RUN_RELEASE (A) | 패널 자체 리셋 시 무한구동 방지(UI 상실→정지). cpp-reviewer catch §4.3. |
| **V30 RUN = KEY_MULTI data=0 → state 매핑 (B)** | V30 에셋이 RUN 키값 0을 양 edge 반환(HW-traced). `app_lcd_measure()->us_run_status`로 START(IDLE)/RELEASE(running) → down/up 쌍이 hold-to-run 재구성, self-syncing. data=0이 RUN 고유라 모호성 0. FSM/enum 무변경. spec §4.4. |
| 펌웨어 적응 > 에셋 수정 | 편집 DWIN 프로젝트 repo 부재 + B가 자족·견고·즉시 검증. A는 후속 옵션. |

## Current State

**Working**: `feat/stage-d-slice2b-run-gate` tip `b78cbbf` = main(`396ec18`) + slice 2b 9 커밋(doc 4 + code 3 + fix 2). 빌드 0-warning(FLASH 28.96% trace빌드 / 28.64% 일반), 호스트 PASS. main = slice 2a(`43fda87`, tag `hw-revA_fw-stage-d2`).

**Broken**: 없음. **트레이스 빌드 `fw/build-trace/` 준비됨**(RUN fix + REG_TRACE + LCD_TRACE_RX, git-ignored).

**Uncommitted**: 없음(이 핸드오프/RESUME/changelog 갱신 제외). `ref/atmega16/M16_reverse/`만 untracked.

## Files to Know

| File | 변경 |
|------|------|
| `docs/superpowers/specs/2026-06-08-stage-d-slice2b-run-gate-design.md` | §4.2 START·§4.3 SYS_PIC_NOW·**§4.4 V30 RUN data=0** |
| `docs/superpowers/plans/2026-06-08-stage-d-slice2b-run-gate.md` | Task 3 HW 절차(합격 6항) |
| `fw/src/app_reg.c` | run FSM·MUX·max/last·publish |
| `fw/src/app_lcd.c` | 훅→app_reg_command |
| `fw/src/app_lcd_disp.c` | ICON_RUN 엣지 |
| `fw/src/app_lcd_input.c` | SYS_PIC_NOW→RELEASE(`4264bab`) + **RUN data=0 매핑(`b78cbbf`)** |

## Resume Instructions — RUN HW 재검증

1. **보드 연결**: ST-LINK(usbmodem) + USART6 mon(usbserial-BG02DMWU @115200). 확인 `ls /dev/cu.usbserial-* /dev/cu.usbmodem*`. BOOT0=GND.
2. **재플래시**(트레이스 빌드 이미 fix 포함 — 재빌드 불필요, 단 의심되면 `cd fw && env -u STM32_TOOLCHAIN cmake --build build-trace`):
   ```bash
   openocd -f fw/openocd/stm32f410.cfg -c "program fw/build-trace/gds_us_ctrl.elf verify reset exit"
   ```
3. **캡처**(단일 fd):
   ```bash
   DEV=$(ls /dev/cu.usbserial-* | head -1)
   { stty -f "$DEV" 115200 cs8 -parenb -cstopb raw -echo; cat; } < "$DEV" > /tmp/reg-mon.log &
   tr -d '\000' < /tmp/reg-mon.log | tr -s ' ' | grep -E '\[(reg|lcd-hook|lcd] rx|boot)'
   ```
4. **RUN 합격기준** (패널 조작):
   - RUN **누름**: `[lcd] rx vp=0x1080 data=0` → `[lcd-hook] us_command=0`(START) → `[reg] cmd=0 run=2` → `run=2 st=1 rc` 0→401 ~4s + `sel` 128→1024 + **ICON_RUN 점등** + VAR_POWER 추종 → ~4s 후 `st=0`.
   - RUN **뗌**: `0x1080 data=0` → (running이므로) `us_command=3`(RUN_RELEASE) → `cmd=3 run=0` + **ICON_RUN 소등** + `sel=0` + VAR_POWER=last_power latch.
   - **재press**: `max_power` 리셋 → VAR_POWER 0에서 재상승 + 램프 재발화.
   - 무회귀: 활성 `st=0` lookup=slice-1; OSC idle-HIGH; LCD 네비. IDLE 출력바(LV_OUTPUT=curr_amp) 빈값 확인(plan item 6).
5. 통과 → `rm -rf fw/build-trace` → **fix `b78cbbf` cpp-reviewer** → finishing(머지/태그).
   - **만약 RUN이 여전히 안 되면**: `[lcd] rx`로 실제 vp/data 재확인(에셋이 또 다른 인코딩일 가능성). systematic-debugging 재진입.

## Warnings

- `-DCMAKE_C_FLAGS` 단독 금지(CPU 플래그 날아감). 모든 cmake `env -u STM32_TOOLCHAIN`.
- 보드 USB가 세션 중 분리될 수 있음(시간 경과) — 재검증 시 장치 재확인 필수.
- clangd/LSP 진단은 노이즈 — 게이트 = cmake 빌드 / `make -C fw/test test`.
- `ref/atmega16/M16_reverse/` untracked 정상.
- (pre-existing) `app_lcd_disp.c:24` "No callers yet" 주석 stale — 후속 정리 후보.
