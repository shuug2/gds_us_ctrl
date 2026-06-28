# Handoff: i2c-pot + ovtime HW 검증·머지 완료 — 남은 백로그 = 6b/rig 게이트

**Generated**: 2026-06-28 (d, HW 검증 세션)
**Branches**: `feat/i2c-pot-amplitude` + `feat/ovtime-energy-run` → **둘 다 main 머지 + 태그 완료, 브랜치 삭제**
**Status**: ✅ 2026-06-28 d HW 세션 — 둘 다 mbpoll 벤치 검증 PASS → `--no-ff` 머지. main ahead origin (push=사람 SSH 미완).

> ## ⚡ 2026-06-28 d — HW 검증 세션 결과 (간이 벤치, [[feedback-swd-halt-breaks-board-validation]] 규칙 준수: 런타임=mbpoll/LCD만, SWD=정적 1회만)
>
> - **i2c-pot** (tag `hw-revA_fw-stage-i2c-pot`, main `189810b`): U4 0x28 **ACK PASS** — 부팅+Modbus START set_pot write 후 `s_err_count`@0x200003e0=0(단발 정적 SWD read=no-halt 예외; NACK였다면 50ms timeout+err≥1), 직접런 STATUS 1×8→0 무회귀. START가 set_pot 확실 호출(`app_modbus.c:117`)+err0 유지로 write-미발생 회의 제거. **진폭 절대추종/칩스케일(0–127/255)/극성 = 6b/스코프 이연**.
> - **ovtime** (tag `hw-revA_fw-stage-ovtime`, main `4c31f8a`): energy_ctrl=OFF 직접런 무회귀(STATUS 1×8→560ms ceiling) + **OVTIME fault 벤치 PASS**(EN_ENERGY=ON+TIMEOVER=1s+START→curr_energy=0[무신호]→~1s후 STATUS=**8**=MB_STATUS_OVTIME) + RESET 복구(bit3→0) + fault 가드 해제 재START 정상. **에너지-도달 정상정지 실 curr_energy + LCD_WARNING 육안 = 6b/실rig 이연**.
> - **정리**: 머지 완료된 `stage-d-slice2b`/`m1` 브랜치 삭제. output-power-graph의 orphaned stash(2026-06-28c 세션맵) drop(내용 MEMORY.md 백업).
> - **OSC OD 전기설정 main 분리 승격** (board.c OSC 3채널 `OUTPUT_PP`→`OUTPUT_OD`만, slice-d `8006e9c`와 byte-identical): 반복 "회귀" 마찰 종료 — OD가 미머지 slice-d에만 갇혀 main 계열 브랜치 전환마다 PP로 돌아가던 것을 main에 영구 적용. heartbeat(PB3) PP 유지(dead). regression-only PASS(START→STATUS 1×8→560ms ceiling 무회귀, 빌드 0-warning). **⚠ divergence: main이 OD를 slice-d보다 앞서 보유** → slice-d board.c=superset(board_osc4/boot-init FSM/PB12 입력/heartbeat 제거), slice-d 머지 시 reconcile. **초음파 RUN 물리구동(board_osc4)은 main에 없음=충돌 회피(전기 정합)만, 초음파 출력 자체는 full slice-d/6b 이연**(app_reg는 여전히 drives NO OSC GPIO). [[project-osc-boot-init]]
> - **⚠ app_reg_tick = 이제 ovtime 버전(`reg_run_limits_t`)이 main 기준** → 미머지 output-power-graph(cal_val 주입)/physical-io-slice-d(ceiling 이중화)는 후속 머지 시 이 구조에 흡수(advisor: 먼저 머지된 ovtime 기준 rebase).
> - **남은 미머지 4 (전부 HW-gated)**: `output-power-graph-ch1`(ch1 repoint + OD + 6b 통합 스테이지 게이트, 단독 불가 판명) / `physical-io-slice-a~d`(패널·물리입력 실배선 rig 필요).
> - **★ 다음 세션 작업 (사용자 우선순위 2026-06-28 d 마감 시점)**: **`feat/physical-io-slice-d` 전체 통합 검토**. ⚠ **중요 발견: OSC 명령 제어(seek/reset/RUN → OSC 핀 PB2/PB10/PB14 물리 구동)는 slice-d 포함 어느 브랜치에도 미구현**. slice-d의 `board_reset(PB10)`/`board_seek(PB2)`/`board_osc4(PB14)` setter는 **boot-init(app_osc_init) 부팅 시퀀스 전용**이고, `app_seek_reset_hook_signal`은 slice-d에서도 **stub**(`app_seek_reset.c:44` "물리 OSC 구동 stub (B-SEAM)", mon 로그만). 즉 **slice-d 통합 ≠ OSC 명령 제어 완성**. ⓐ slice-d 통합 게이트 = 패널 실배선 rig(io.c START/SENS/ESTOP/SOL_DN 등) + PB12 피드백 입력(OSC 보드) + **board.c reconcile**(main OD가 slice-d보다 앞섬, slice-d board.c=superset). ⓑ OSC 명령 제어 실구동 = **B-SEAM 별도 작업**(app_seek_reset hook → board_reset/seek 바인딩 + app_reg RUN → board_osc4 + 스코프/실초음파 극성·주파수 검증). [[project-seek-reset]]/[[project-osc-boot-init]].
> - **보드 현재**: ovtime 펌웨어 적재, SERIAL/addr=1/9600/EVEN, EN_ENERGY=0·TIMEOVER=8 복원, OUT_POWER=55, ether_ip .70 잔여.

> 진입점 = 이 파일. (이하 2026-06-28 b mode-b 설계+host 세션 기록 — 두 슬라이스 모두 위에서 머지됨)

> 진입점 = 이 파일. 이번 세션은 **HW 없이 설계+host 구현(mode-b)** — 초음파 종료조건 SAMD20 분석에서 출발해 2개 슬라이스를 brainstorming→spec→TDD(host)→glue→cpp-review까지. 실 HW actuation/표면 검증만 보드 세션으로 분리. (이 브랜치들과 별개로 `feat/physical-io-slice-d`의 OSC+slice D 스택은 직전 세션 산출물 — 이번 세션 무관, 여전히 실배선 rig 대기.)

## Goal

SAMD20 legacy 분석으로 도출한 두 미포팅 기능을 6b/HW 없이 가능한 데까지 포팅: ① 진폭(I2C_POT) 실구동 ② energy 모드 직접런 종료(에너지-도달 정상정지 + 과대시간 OVTIME fault). + #7 출력이상(ERR_OUTERR)을 6b 하위항목으로 문서화.

## Completed

### 슬라이스 1 — I2C_POT 진폭 actuation (`feat/i2c-pot-amplitude`: `77844f7` spec + `2d653f4` feat)
- [x] 두 진폭 hook(`app_lcd_hook_set_pot` %, `app_weld_hook_set_amp` raw DAC)이 둘 다 로그 stub이던 것을 공용 `drivers/i2c_pot.c`(`i2c_pot_set_dac`→`i2c1_mem_write(0x28,0x00)` best-effort)로 통합.
- [x] 순수 `pot_dac_from_power`(언더플로 가드 `<=50→0`, 상한 127; 50~100 samd20 비트-동일) host-test 신규 스위트 `test_i2c_pot`.
- [x] 부팅 초기 진폭 1회(app.c config 로드 직후, samd20 main.c:910).
- [x] 빌드 0-warning FLASH 41.73%, host 6스위트 PASS, cpp-review **APPROVE**.

### 슬라이스 2 — OVTIME energy-run 종료 (`feat/ovtime-energy-run`: `f9b878c` spec+#7 + `ffd675c` feat)
- [x] 순수 `reg_energy_termination`(`app_reg_calc`): energy 도달→STOP_ENERGY(우선) / 미달+`elapsed≥limit_out_time*1000ms`→FAULT_OVTIME / 비-energy·`limit_out_time=0`→CONTINUE. host-test 8케이스(reg_calc 편입).
- [x] `app_reg_tick(reg_run_limits_t*)`: energy 모드면 on-time ceiling **대체**(legacy 충실). `reg_stop_run` helper. OVTIME→`error_status|=ERR_OVTIME`. START fault 가드(swallow consume 뒤 별도 break). RESET이 클리어. `lcd_measure_t.error_status` publish.
- [x] fault 표면(기존 미공급 인프라 첫 공급): `app_lcd_tick` 엣지→`app_lcd_show_error`(LCD_WARNING) + `app_modbus` STATUS|=`MB_STATUS_OVTIME`. ERR_* `app_lcd.h` 공유.
- [x] 빌드 0-warning FLASH 42.01%/RAM 16.82%, host 5스위트 PASS, cpp-review **APPROVE-WITH-COMMENTS**(0 Crit/High).

### #7 출력이상 (문서만)
- [x] `docs/NEXT_STEPS.md` §1.2 6b **하위항목으로 명시** — legacy 트리거 `re_outerr_issued=1` 주석처리(main.c:4333)=비활성, `curr_amp`(ADC) 의존→6b 종속.

## Not Yet Done

- [ ] **두 슬라이스 머지** — 각각 HW 검증 후 `--no-ff` 머지 + 태그. 독립(서로·OSC 스택과 무관, 전부 off main).
- [ ] **HW 검증(슬라이스1)**: U4 0x28 ACK 확인(미ACK 시 매 set 호출 50ms 블로킹) → 스코프로 wiper가 output_power/weld out1→out2 추종.
- [ ] **HW 검증(슬라이스2)**: energy 모드 직접런 OVTIME→LCD_WARNING+`MB_STATUS_OVTIME`(mbpoll)+RESET 복구. 에너지-도달 정상정지는 실 `curr_energy`(=6b) 후.
- [ ] **OSC/slice-D 스택**(`feat/physical-io-slice-d`, 이번 세션 무관) — 실배선 rig 대기. 메모리 [[project-osc-boot-init]]/[[project-physical-io-layer]].

## Failed Approaches (Don't Repeat These)

- **#4 OVTIME을 OVTIME만 떼어 포팅 X** — legacy(5270-5294)에서 OVTIME은 energy 종료 **쌍의 절반**(에너지-도달 정상정지가 나머지). OVTIME만 넣으면 에너지-도달 정상정지가 없어 벤치선 늘 OVTIME으로 끝나 의미가 약함 → 쌍 전체 포팅.
- **#7 ERR_OUTERR 단독 포팅 X** — legacy에서 트리거(`re_outerr_issued=1`)가 **주석처리되어 비활성**이고, 검출이 실 `curr_amp`(ADC 측정 진폭)에 의존 → 6b calibration 없이는 무의미. 6b 하위항목으로만 문서화.

## Key Decisions

| Decision | Rationale |
|----------|-----------|
| 두 슬라이스 각각 main에서 분기(OSC 스택 위에 안 쌓음) | I2C_POT·OVTIME은 app_lcd/app_weld/app_reg(전부 main에 존재)만 의존 — OSC 미머지 스택 오염 회피 |
| I2C_POT 통합(set_amp만 X) | legacy 12 write 중 9곳이 set_pot(%) = 정상 진폭 전부. set_amp만 연결 시 일반 진폭 죽음 |
| OVTIME = energy 종료 쌍 전체 | energy 모드가 on-time ceiling **대체**(legacy 5270 분기) |
| fault source=app_reg, `lcd_measure_t.error_status` publish | 기존 미공급 인프라(LCD warning/MB_STATUS_OVTIME/키 복구) 첫 공급자 |
| START fault 가드 = swallow consume **뒤** 별도 break | advisor: if 조건 합산 시 swallow 비대칭 버그(seek_reset 가드와 동일 패턴) |

## Current State

**Working**: 두 브랜치 각각 빌드 0-warning(우리 코드; vendor WIZnet 경고 격리), host 전 스위트 PASS. 트리 clean(`?? ref/signal/`=이전 OSC Saleae 캡처, 무관·미추적).

**Broken**: 없음.

**현재 체크아웃**: `feat/ovtime-energy-run`. ⚠ **브랜치 전환 시 `cmake -S fw -B fw/build` reconfigure 필수**(GLOB은 configure-time — i2c-pot 브랜치엔 `drivers/i2c_pot.c` 있고 ovtime/main엔 없음 → 캐시 stale 시 빌드 실패. 이번 세션 실제로 겪음).

## Files to Know

| File | Why It Matters |
|------|----------------|
| `fw/src/app_reg_calc.c` / `.h` | 순수 `reg_energy_termination`(host-test) + `OVTIME_SEC_MS`. reg_* 순수 계열 |
| `fw/src/app_reg.c` | energy 분기(`app_reg_tick`)·`reg_stop_run`·START fault 가드·RESET 클리어·`error_status` publish |
| `fw/include/app_reg.h` | `reg_run_limits_t`(주입 구조)·시그니처 |
| `fw/src/app_lcd.c` | `app_lcd_tick` error_status 엣지→`app_lcd_show_error`. `set_pot`(i2c-pot 브랜치) |
| `fw/include/app_lcd.h` | ERR_* 공유 define + `lcd_measure_t.error_status` |
| `fw/drivers/i2c_pot.c` / `.h` | (i2c-pot 브랜치) `i2c_pot_set_dac`·순수 `pot_dac_from_power` |
| `docs/superpowers/specs/2026-06-28-{i2c-pot-amplitude,ovtime-energy-run}-design.md` | 두 슬라이스 spec(legacy 근거·결정·HW-gated 경계) |

## Code Context

**OVTIME 순수 결정** (`app_reg_calc.h`):
```c
typedef enum { REG_RUN_CONTINUE=0, REG_RUN_STOP_ENERGY=1, REG_RUN_FAULT_OVTIME=2 } reg_energy_outcome_t;
#define OVTIME_SEC_MS 1000u   /* limit_out_time = 초 (legacy us_on_time[100ms]>=limit_out_time*10) */
reg_energy_outcome_t reg_energy_termination(uint8_t energy_ctrl, uint32_t curr_energy,
    uint32_t limit_energy, uint32_t elapsed_ms, uint16_t limit_out_time);
/* !energy→CONTINUE; curr>=limit→STOP_ENERGY(우선); elapsed>=lot*1000→FAULT_OVTIME
   (lot=0이면 elapsed>=0 항상 참=즉시 OVTIME — 0=off 아님, never-stop 방지, advisor) */
```
**진폭 변환** (`i2c_pot.h`): `pot_dac_from_power(p)` = p<=50?0 : p>=100?127 : (p-50)*255/100.

## Resume Instructions

1. 빌드/host sanity (브랜치 선택 후 **reconfigure**):
   `cd fw && env -u STM32_TOOLCHAIN cmake -S . -B build -G Ninja && env -u STM32_TOOLCHAIN cmake --build build` (0-warning 기대)
   `make -C fw/test test` — i2c-pot 브랜치=6스위트, ovtime 브랜치=5스위트 PASS.
2. **머지 결정**: 두 슬라이스는 독립·HW 게이트. HW 검증 통과분부터 `--no-ff` 머지 + 태그(`hw-revA_fw-...`).
3. HW 세션 = 위 "Not Yet Done" HW 항목.

## Setup Required

- 빌드: `env -u STM32_TOOLCHAIN` 래핑 필수. 신규 `.c` 추가/브랜치 전환 시 `cmake -S fw -B build` reconfigure.
- 보드: ST-LINK SWD. SERIAL이면 USART6=Modbus 점유 → mon 비가용(검증=mbpoll/스코프).

## Edge Cases & Error Handling

- **벤치 무신호(curr_energy=0)**: energy 모드 직접런은 에너지-도달 불가 → `limit_out_time*1000ms`(또는 0이면 즉시) OVTIME으로 종료(정상; 실 에너지=6b). ⚠ `limit_out_time=0`은 "off"가 아니라 **즉시 OVTIME**(legacy 충실; 0=off로 두면 energy 모드가 ceiling 대체라 never-stop — advisor가 잡은 버그, ffd675c 이후 수정).
- **OVTIME fault 중 START**: app_reg가 `error_status!=0`이면 새 START 거부 → RESET으로 클리어해야 재시작(samd20 SYS_ERROR 충실).
- **U4 미ACK(슬라이스1)**: 모든 set_pot/set_amp가 `I2C1_TIMEOUT_MS`(50ms) 블로킹 + `i2c1_err_count`만 증가. HW 첫 검증=err_count 0 확인.

## Warnings

- ⚠ **두 슬라이스 미머지·각각 HW 게이트** — 단독 머지는 HW 검증 후.
- ⚠ **OVTIME ↔ slice-D ceiling 이중화 머지 충돌**: 둘 다 `app_reg_tick` 종료 블록 개조. 머지 시 energy 분기 + 절대30s + legacy hand ceiling 통합 필요.
- ⚠ **cpp-review 비차단 코멘트(반영 보류, OVTIME)**: M1 ERR_* 중복정의 drift 위험(app_lcd_input.c 로컬 vs app_lcd.h 공유, "요청한 부분만" 규칙으로 미수정) / L1 error_status publish 2ms 게이트 stale window(무해) / L2 OVTIME_SEC_MS↔ON_TIME_UNIT_MS 단위 주석 분산. N1(동기 lifetime 주석)은 반영함.
- ⚠ **clangd LSP 위양성**(cmake include 미연동) — "file not found/unknown type" 무시. 진실 = `cmake --build`(0-warning) + `make -C fw/test test`.
- ⚠ git 해시는 2026-06-20 filter-repo 재작성됨 — 안정 레퍼런스는 태그.
