# Handoff: Weld-Cycle Slice 1 (Core FSM, DELAY mode) — 구현 완료(host-verified) · 미머지(보드 먼저)

**Generated**: 2026-06-14
**Branch**: `feat/stage-weld-cycle-slice1` (main에서 분기, 6개 docs 커밋, 코드 0줄)
**Status**: CODE-COMPLETE (host-verified) · 미머지 — 8 코드커밋, 최종 cpp-reviewer APPROVED, 호스트 4스위트 PASS, 빌드 0-warning(우리 코드). 사용자 "보드 먼저" → 다음 = 보드 직접-초음파 무회귀 확인 → 머지+태그.

> 이전 HANDOFF(Stage C slice-2)는 머지 완료로 supersede. 이력 = `docs/changelog.md` / `docs/superpowers/RESUME.md` / 메모리 `project_stage_c_modbus`.

## Goal

samd20 공압 프레스 **weld-cycle FSM**(`READY→CYL1→WELD→HOLD→CYL2→work_cnt++`)의 **DELAY 모드**를 STM32F410에 포팅한다(슬라이스1). HAL-free 순수 FSM 코어 + 글루 분리, WELD가 신규 소스 `US_CYCLE`로 기존 `app_reg` 초음파 게이트를 구동. energy/multi/TRIGGER/실GPIO/안전 = 슬라이스 2~4.

## Completed

- [x] 브레인스토밍 (제품 결정: 공압 프레스 O / DELAY·TRIGGER 둘 다 / 충실도=혼합)
- [x] Spec 작성·정정·커밋 → `docs/superpowers/specs/2026-06-14-stage-weld-cycle-slice1-design.md`
- [x] 구현 계획 (6 tasks, TDD, 전 step 실코드) 작성·커밋 → `docs/superpowers/plans/2026-06-14-stage-weld-cycle-slice1.md`
- [x] 통합 지점 사전 확인: CMake `file(GLOB src/*.c)`(신규 .c 자동), main.c/app.c 배선 위치, mon/sys_tick/app_lcd/app_config API 시그니처

## Done (구현 — 계획서 Task 1~6 전부 완료)

- [x] **Task 1**: `app_weld_fsm.{h,c}` 스캐폴드 + `test_app_weld_fsm.c` + Makefile `BIN_WELD` (`aef0885`)
- [x] **Task 2**: `weld_fsm_step()` 전체(5상태 DELAY) + 완전사이클·타이밍 테스트 (`fba8001`)
- [x] **Task 3**: comp_time 진폭보정 + **언더플로 가드** + 진폭/엣지/start 테스트 (`d1c680d`)
- [x] **Task 4**: `US_CYCLE=4` enum + `app_reg.c` ceiling 제외 주석(**comment-only**) + 펌웨어 빌드 (`70992f8`)
- [x] **Task 5**: 글루 `app_weld.{h,c}` + SOL_DN/**set_amp** hook (`0cda1e4` + 리뷰코멘트 `26913f9`)
- [x] **Task 6**: 슈퍼루프 배선(main.c init, app.c tick) + 최종 빌드/테스트 (`ba05147`)
- [x] 최종 cpp-reviewer **APPROVED** + LOW-2 fail-safe fix (`f482af9`)

## Not Yet Done (보드 대기)

- [ ] (HW) 보드 회귀확인 — 기존 직접-초음파(패널/Modbus START) 무회귀 + 사이클 READY 휴면(슬라이스1 트리거 없음)
- [ ] 머지(`--no-ff`) + 태그 `hw-revA_fw-stage-weld1` (⚠ **host-only 태그** — HW 회귀확인 시점 명시; 사이클 자체 E2E는 슬라이스4)
- [ ] (슬라이스4 must-fix) LOW-1: LCD `LV_OUT_POWER` [50,100] 클램프 — 아래 Edge Cases 참조

## Failed Approaches (반복 금지)

> **START 트리거 "통합" 설계 — 폐기됨.** 세션 중간에 "물리 SW_START1/2 · 패널 START · Modbus START를 **모두 동등한 사이클 트리거**로 통합"하는 안으로 spec을 한 번 고쳤다가(커밋 `98d0708`), 사용자 재확인으로 **되돌렸다**(커밋 `0203c90`). **올바른 설계 = samd20 충실 분리**:
> - **weld 사이클 = 물리 양수 시작스위치(SW_START1/2)로만** 열림 (`re_start1==0 && re_start2==0 && in_cycle==0`, ref/samd20/main.c:1404-1466). → 물리입력이라 **슬라이스4(HW-gated)**.
> - **패널 START(KEY_MULTI)·Modbus START = 직접 초음파(hand/comm)** 경로, 사이클 아님. 현 STM32 거동(`app_reg` US_TOUCH/US_COMM + on-time ceiling) **무수정**.
> - 초음파 ON 경로 2개 공존: (1)직접 (2)사이클. **패널/Modbus START를 사이클로 재라우팅하지 말 것** — 검증된 stage-d2b/c 거동을 깨뜨린다.
> 결과: **슬라이스1은 프로덕션 트리거가 없다** → FSM은 host 테스트로만 검증, HW는 회귀확인. 사이클 HW E2E는 슬라이스4.

## Key Decisions

| Decision | Rationale |
|----------|-----------|
| 신규 모듈 2개 분리(`app_weld_fsm` 코어 + `app_weld` 글루) | 프로젝트 관례: 순수 compute는 HAL-free host-test(app_reg_calc/modbus_core 패턴) |
| WELD가 `US_CYCLE` 소스로 `app_reg` 게이트 구동 | 진폭 hook·peak 래치·STATUS bit0 재사용; `app_reg_command`는 src를 그대로 저장 → 분기 불필요 |
| WELD 길이 = `limit_delay_time2`(weld 타이머), on-time ceiling 아님 | ceiling은 직접(TOUCH/COMM) 경로용; US_CYCLE은 ceiling 조건에서 자연 제외 |
| DELAY 먼저(슬라이스1), TRIGGER 후(슬라이스4) | DELAY=시간기반 host-test 가능; TRIGGER=위치센서 HW 필요 |
| 충실도 = 혼합 | 거동 samd20 충실 + 명백한 버그만 수정(진폭 언더플로 가드 1건) |
| 진폭 언더플로 가드 추가 | `(op-50)*255/100`(0..127) - `(7-comp_time)*10`이 저전력+짧은용접에서 uint8 언더플로→큰 진폭(안전 위험) → 0 클램프 |
| SETUP 게이트 슬라이스4 이연 | STM32에 sys_status 미유지 + 슬라이스1은 사이클이 HW에서 안 돔(무의미) |
| 시간 단위 10ms/count | samd20 `temp_time--` @ `tick_1ms>=10` (main.c:5313-5334) |

## Current State

**Working**: 펌웨어 빌드 green(코드 미추가 상태), host 테스트 3 스위트 PASS, 기존 직접-초음파(패널/Modbus START) 정상. main 대비 추가된 것은 docs 6커밋뿐.

**Broken**: 없음.

**Uncommitted Changes**: 없음(working tree clean; untracked `.understand-anything/`·`ref/atmega16/M16_reverse/`는 무관·무시).

## Files to Know

| File | Why It Matters |
|------|----------------|
| `docs/superpowers/plans/2026-06-14-stage-weld-cycle-slice1.md` | **구현 청사진** — Task별 실코드·명령·기대출력. 이것만 따라가면 됨 |
| `docs/superpowers/specs/2026-06-14-stage-weld-cycle-slice1-design.md` | 설계 근거(§5.3 트리거 분리, §8 가드, §9 테스트) |
| `ref/samd20/main.c:1450-1632` | weld-cycle FSM 원본(SYS_RUN 블록) — 포팅 소스 |
| `ref/samd20/main.c:1404-1466, 5313-5334` | start_key(물리 스위치) + temp_time 10ms 카운트다운 |
| `fw/src/app_reg.c` | 초음파 게이트(`app_reg_command`/ceiling) — US_CYCLE 통합 대상(Task 4) |
| `fw/include/app_lcd.h:67` | `us_run_status` enum — `US_CYCLE=4` 추가(Task 4) |
| `fw/src/app.c` (`app_loop_iter`) / `fw/src/main.c` | 슈퍼루프 tick / init 배선(Task 6) |
| `fw/test/Makefile`, `fw/test/test_app_reg_calc.c` | host 테스트 harness 패턴(CHECK_EQ) |

## Code Context

**FSM 코어 인터페이스** (Task 1에서 생성 — 계획서에 전문 있음):
```c
enum { WELD_READY=0, WELD_CYL1=1, WELD_WELD=2, WELD_HOLD=3, WELD_CYL2=4 };
#define WELD_COMP_FULL 7u
typedef struct { uint8_t start, run_mode; uint16_t limit_delay_time1, limit_delay_time2,
                 limit_delay_time3; uint8_t output_power; } weld_in_t;
typedef struct { uint8_t run_status, sol_dn, weld_start, weld_stop, amplitude, cycle_done; } weld_out_t;
void weld_fsm_init(void);
void weld_fsm_step(const weld_in_t *in, weld_out_t *out);  /* 10ms마다 1회 */
uint8_t weld_fsm_status(void);
```

**글루 → 기존 API 연결** (Task 5):
```c
/* out.weld_start */ app_weld_hook_set_amp(out.amplitude); app_reg_command(US_CMD_START, (uint8_t)US_CYCLE);  /* set_amp=raw DAC, NOT set_pot(이중변환) */
/* out.weld_stop  */ app_reg_command(US_CMD_RUN_RELEASE, (uint8_t)US_CYCLE);
/* out.cycle_done */ cfg->work_cnt++; app_config_save_all(cfg); app_lcd_set_work_cnt(cfg->work_cnt);
```
확인된 시그니처: `app_reg_command(us_cmd_t, uint8_t)`, `app_lcd_cfg()→app_config_t*`, `app_lcd_set_work_cnt(uint32_t)`, `app_config_save_all(const app_config_t*)`, `sys_tick_get_ms()`, `mon_printf(...)`(무가드). ⚠ 진폭은 신규 `app_weld_hook_set_amp(uint8_t dac)`(raw DAC 직접) — 기존 `app_lcd_hook_set_pot(uint8_t output_power)`은 인자를 output_power로 받아 내부에서 `(x-50)*255/100`을 적용하므로 보정된 DAC를 넘기면 이중 변환(op=100→127→196). spec §6 참조.

**비자명 로직**: `app_reg_command(START, US_CYCLE)`는 코드 변경 없이 동작 — START 케이스가 `us_run_status = src`로 저장하고 swallow_start 체크는 `src==US_TOUCH`만. ceiling은 `(rs==US_TOUCH)||(rs==US_COMM)`라 US_CYCLE 자연 제외. **enum에 US_CYCLE 추가만 필요**.

## Resume Instructions (보드 연결 세션 — 회귀확인 후 머지)

구현은 끝났다(8 커밋, cpp-reviewer APPROVED). 남은 건 HW 회귀확인 + 머지뿐.
1. `cd /Users/tknoh/dev/work/gds_us_ctrl && git checkout feat/stage-weld-cycle-slice1`
2. 사전 점검:
   ```bash
   env -u STM32_TOOLCHAIN cmake -S fw -B fw/build -G Ninja && env -u STM32_TOOLCHAIN cmake --build fw/build   # 0-warning(우리 코드)
   make -C fw/test test                                                                                       # 4 스위트 PASS
   ```
3. 플래시 → mon(`/dev/cu.usbserial-AB0MLYXA`)로 **회귀확인**(슬라이스1은 프로덕션 트리거 없음 → 사이클 안 돎이 정상, 속지 말 것):
   - 부팅 후 `[weld]` SOL_DN/set_amp 로그 **없음**(READY 휴면 확인).
   - 기존 직접-초음파 무회귀: 패널 RUN press → ICON_RUN + on-time ceiling 정상 / Modbus START도 동일(stage-d2b·c 거동 유지).
4. 통과 시 `superpowers:finishing-a-development-branch` → 머지(`--no-ff`) + 태그 `hw-revA_fw-stage-weld1`(⚠ host+HW-regression 검증 시점 명시) → changelog/RESUME/NEXT_STEPS/memory 갱신.
5. **사이클 동작 자체의 HW E2E(상태 시퀀스·work_cnt 증가·SOL_DN 구동)는 슬라이스4** — 물리 SW_START + 실 SOL_DN/센서 필요. 슬라이스1은 보드로 사이클 검증 불가(host-test가 전부).

## Edge Cases & Warnings

- **START 재통합 금지** (위 Failed Approaches). 패널/Modbus START는 무수정.
- **부트 워밍업 상호작용**: app_reg는 부트 후 ~4s `main_state!=0` 동안 START 무시. 슬라이스1은 트리거 없어 무관이나, 슬라이스4 물리 트리거 도입 시 워밍업 중 사이클→WELD의 US_CYCLE START가 무시될 수 있음(계획서 self-review 메모).
- **CMake GLOB**: 신규 .c 추가 후 반드시 `cmake -S fw -B fw/build` **재구성**(GLOB은 configure 시점 평가).
- **빌드 env**: `$STM32_TOOLCHAIN` stale → `env -u STM32_TOOLCHAIN` 필수.
- **슬라이스1 HW 한계**: 프로덕션 트리거 없음 → 보드에서 사이클은 절대 안 돎(READY 휴면). 사이클 동작 검증은 host 테스트가 전부. 속지 말 것 — "보드에서 사이클 안 보임"은 정상.
- **work_cnt 런타임**: 슬라이스1은 work_cnt 증가 **로직**만 확정(PC HMI 미해결 #7 설계 종결); 실제 증가는 슬라이스4 물리 트리거 후. PC HMI 연계 = 메모리 `project_pc_hmi_spinoff`.
- **⚠ 슬라이스4 MUST-FIX (cpp-review LOW-1) — 진폭 언더플로**: `weld_amplitude`의 `(uint16_t)(output_power - 50u)`는 `output_power<50`이면 unsigned 언더플로 → 거대 진폭(안전 위험). Modbus 경로는 `app_modbus.c`에서 `[50,100]` 클램프하나 **LCD 패널 경로 `app_lcd_input.c:752` `LV_OUT_POWER`는 클램프 없음**(기존 직접 set_pot `app_lcd.c:28`도 동일 pre-existing 노출). 슬라이스1은 **dormant**(프로덕션 트리거 없음 + hook 로그만) → 무수정 OK. **슬라이스4에서 물리 SW_START + 실 I2C_POT 연결 시 HIGH 격상** → 슬라이스4 진입 시 `app_lcd_input.c:752`에 Modbus와 동일한 `if (data16 < 50u) data16 = 50u;` 클램프 미러. (samd20도 여기서 클램프 안 함 = 충실; 그래서 슬라이스1은 추가 가드 없이 둠.)
- **cpp-review LOW-2 (반영됨)**: `weld_fsm_step` default fault-path(정상 도달 불가)에서 `s_sol_dn=0` fail-safe — 슬라이스4 실 SOL_DN GPIO 연결 시 fault 시 솔레노이드 잔류 방지.
- **글루 tick 게이트 슬라이스4 메모(cpp-review M1)**: `app_weld.c` 10ms 게이트가 `s_prev_ms = now`(누적 슬립)이라 weld 단계가 길어지면 dwell이 누적 지연 — 슬라이스1 무해(액추에이터 없음), 슬라이스4 실 공압 dwell엔 `s_prev_ms += WELD_TICK_MS`로 변경 권장(코드에 주석 있음).
