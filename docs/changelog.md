# Changelog

## [Unreleased]

### 2026-06-17 c — SEEK/RESET 효과 CODE-COMPLETE (host+build verified, 미머지)

SEEK/RESET 명령 효과(기존 `app_reg_command` no-op)를 순수 FSM + 글루로 구현. spec 사용자 리뷰(코드 참조 drift 0) → `superpowers:writing-plans`(plan 3 Task TDD) → `subagent-driven-development`(Task별 fresh subagent + 2-stage 리뷰[spec 대조 + cpp-reviewer] + 최종 통합 리뷰). 브랜치 `feat/stage-seek-reset` **미머지**(HW 회귀 + ICON 육안 = 보드 게이트; slice3 머지 후 stack). 빌드 **0-warning(우리 코드, FLASH 42.1%/RAM 16.8%)**, 호스트 **5스위트 PASS**(기존 4 + `app_seek_reset_fsm` 6 시나리오).

- **거동(samd20 충실)**: RESET→500ms→SEEK 자동 체인→500ms→자동 해제→IDLE(`ref/samd20/main.c:5388-5408`); SEEK 직접은 단발(체인 없음); 타이밍 10ms tick `SR_TICKS=50`=500ms 액면가(samd20 100ms quirk 미재현, weld3 교훈; 진입 elapsed=0→`>=50` 전이→signal span 50tick).
- **양방향 RUN 직교**: RUN 중 SEEK/RESET 무시(FSM `run_active`) + SEEK/RESET active 중 START 무시(`app_reg_command` START guard에 `app_seek_reset_active()` read-only 조회). 두 가드 상호보강 **구조적 불변성** — `us_run_status`는 START case에서만 non-IDLE이 되는데 그게 guard로 막혀 run_active mid-sequence 전이 경로 부재.
- **분석 §5 ICON 엣지 갭 해결**: reset/seek_icon on/off 1-shot 엣지 → `ICON_RESET`(0x1150)/`ICON_SEEK`(0x1151) VP write(`app_lcd_icon` 기존 헬퍼). 물리 OSC 신호 구동은 **hook stub(mon 로그) + B-SEAM 이연**.
- **산출물**: `app_seek_reset_fsm.{h,c}`(HAL-free FSM, 포화 가드) / `app_seek_reset.{h,c}`(글루: 10ms tick, cmd 1-shot 래치, run_active 주입, icon 엣지→hook+`app_lcd_icon` 라우팅) / `app_reg.c`(no-op→`app_seek_reset_request` 위임 + default 분리 + START guard **swallow-safe** 직교) / `app.c`·`main.c` 배선 / host-test 6(체인/SEEK단발/RUN직교/busy/ICON엣지/타이밍경계).
- **커밋 3**: `0dddba3`(인터페이스+스캐폴드+host-test 배선)→`6410c5f`(FSM 전이 로직+host-test 6)→`8186325`(글루+위임/swallow-safe guard+배선).
- **⚠ swallow-safe guard**(advisor 핵심 회귀 위험 — HW-verified RUN-path): `if (app_seek_reset_active()!=0) break;`를 swallow-consume **뒤**+`us_run_status=src` **앞** 별도 블록으로 — if 조건에 `&&` 합산 시 swallow consume까지 스킵되는 비대칭 버그. 정상경로(active=0) byte-equivalent, `app_reg_tick` 무변경.
- **리뷰**: Task별 spec✅ + cpp-reviewer APPROVED-WITH-COMMENTS(0 Crit/High), **최종 통합 cpp APPROVED-WITH-COMMENTS(0 Crit/High/Med — 코드 레벨 머지 blocker 없음)**. cpp Minor 반영: 포화가드 근거 주석, 1-iter stale run_active 주석, warm-up 미게이팅 주석. main.c re-indent 원복(CLAUDE.md "요청 부분만").
- **⚠ 글루 host 커버리지 0**: FSM 코어만 host-test; 글루(10ms 게이트·icon→hook→LCD·run_active 조회·START 역직교)는 cpp-review + HW 위임(spec §7) → **spec §9 HW 4항목이 실질 게이트**.
- **다음 = 보드 세션 HW 4항목**: ① 직접-초음파 무회귀 ② RESET→ICON_RESET→(500ms)→ICON_SEEK→(500ms)→소등 자동 체인 시각 ③ SEEK 단발 ④ RUN↔SEEK/RESET 양방향 무시 → `--no-ff` 머지 + 태그 `hw-revA_fw-stage-seekreset`(slice3 머지 후). 물리 OSC E2E는 B-SEAM/6b.
- **이연**: 물리 OSC 구동(B-SEAM/6b)·물리버튼 레벨팔로우(슬라이스4)·과부하 복구(overload, 이 FSM 재사용)·에러 표시(에러 머신)·weld-START 상호작용(슬라이스4 물리 SW_START 시 의식적 결정)·1-iter stale(B-SEAM)·warm-up 미게이팅(samd20 충실).

### 2026-06-17 b — SEEK/RESET 효과 brainstorming + spec 확정 (구현 미시작)

SEEK/RESET 명령의 효과(현재 `app_reg_command` no-op) 스테이지 설계. `superpowers:brainstorming` → spec 커밋. 코드 0줄. 브랜치 `feat/stage-seek-reset`(slice3 tip `33b7ae9` 위 stack — slice3 미머지라 docs 연속성 위해; main 분기 시 slice3 docs 누락). spec `1bd3ce8`.

- **범위(사용자 확정)** = 상태머신 + 500ms 타이밍 체인 + 자동해제 + ICON 렌더(**HW 불요**, host-test). **물리 OSC 신호 구동은 hook stub + B-SEAM/6b 이연** — samd20은 M_RESET/M_SEEK active-LOW를 외부 M16으로 보냈으나 STM32가 M16 흡수 → 물리 출력(CTRL_OSC* 미러) 극성 미확인.
- **아키텍처** = 순수 FSM 분리(신규 `app_seek_reset_fsm` HAL-free host-test + 글루 `app_seek_reset` 10ms tick+hook, weld 패턴).
- **거동**: RESET→500ms→SEEK 자동 체인→500ms→자동 해제→IDLE(samd20 `main.c:5388-5408`); SEEK 직접은 단발(체인 없음); 타이밍 10ms tick 50=500ms 액면가(samd20 100ms quirk 미재현, weld3 교훈); 양방향 RUN 직교(RUN 중 SEEK/RESET 무시 + SEEK/RESET active 중 START 무시); 분석 §5 ICON 엣지 렌더 갭 동시 해결.
- **이연(spec §10)**: 물리 OSC 구동(B-SEAM/6b)·물리버튼 레벨-팔로우(슬라이스4)·과부하 복구 시퀀스(overload, 이 FSM 재사용)·에러 표시(에러 머신).
- **다음** = spec 사용자 리뷰 → writing-plans → subagent-driven(새 세션, 이 브랜치). spec=`docs/superpowers/specs/2026-06-17-stage-seek-reset-design.md`.

### 2026-06-17 — weld-cycle 슬라이스3 (multi_ctrl 2단 진폭 스테핑) CODE-COMPLETE (host+build verified, 미머지)

samd20 `multi_ctrl` 2단 진폭 스테핑 포팅. WELD 단계에서 진입 시 `limit_mo_out1` → `limit_mo_time1` 경과 후 `limit_mo_out2`로 전환 → `limit_mo_time2`에서 WELD 정상 종료(→HOLD). `superpowers:brainstorming → spec → writing-plans → subagent-driven`(plan 3 Task, Task별 fresh subagent + 2-stage 리뷰[controller spec 대조 + cpp-reviewer]). 브랜치 `feat/stage-weld-cycle-slice3-multi` **미머지**(HW 회귀확인=보드 게이트, spec §10). 빌드 **0-warning(우리 코드, FLASH 41.64%)**, 호스트 **4스위트 PASS**(weld_fsm 21함수 = 기존 12 + multi 9).

- **아키텍처 = FSM 내부 상태**(advisor — 슬라이스2 Option A보다 단순): multi는 순수 시간 기반이라 `s_multi_stage`(0/1)·`s_multi_elapsed`(10ms tick)를 FSM 내부 런타임 상태로 둠(`app_reg`/주입 불필요). **진폭 emit 단일 설계**: 기존 `weld_start` 엣지(1단) + 신규 `amp_change` 엣지(2단)가 같은 `amplitude` 필드를 내보내고 글루가 같은 `app_weld_hook_set_amp` 재호출(START/STOP 없음, US_CYCLE 유지).
- **우선순위 multi>energy>시간**(samd20 if-else `main.c:1562`): multi ON이면 슬라이스1 DELAY-exit·슬라이스2 energy-exit 둘 다 게이팅(else-if 단락). **comp_time 미적용**(samd20 multi 경로 `5242`/`1540` 직접 변환). **언더플로 가드**(`weld_mo_amplitude`: `<50→0`, 팩토리 기본 `limit_mo_out1=25`가 바로 트리거; `weld_amplitude`와 별개 함수). **타이밍 액면가 10ms tick**(사용자 확정; `s_multi_elapsed`를 time1/time2와 직접 비교, samd20 100ms 절단 미재현 → ~90ms divergence).
- **산출물**: `app_weld_fsm`(`weld_in_t` multi 5필드 + `weld_out_t` `amp_change` + WELD multi 분기 + `weld_mo_amplitude`) / `app_weld` 글루(cfg 5필드 주입 + `amp_change`→`set_amp` 라우팅) / host-test 9(스테핑·override energy·override DELAY·언더플로·stage리셋·경계 `time1==time2`·`time1>time2`·진폭 vector·multi-off 회귀).
- **커밋 3**: `49ca2c7`(인터페이스 5필드+amp_change+정적 stage/elapsed)→`52a24f2`(FSM multi 분기+host-test 9)→`f9c4ac9`(글루 5필드 주입+amp_change 라우팅+amplitude 주석 M1; Co-Authored trailer amend 제거).
- **리뷰**: Task2 cpp-reviewer APPROVED-WITH-COMMENTS(0 Crit/High), **최종 통합 cpp-reviewer APPROVED(머지 가능)**. Minor 2 = `limit_mo_out`(uint8) 캐스트 truncation(Modbus 256-305 시; config-validation 클램프 슬라이스4 이연)·`time1==time2` 경계(test_multi_boundary_equal로 동결) — 둘 다 spec §8/§11 기록.
- ⚠ **`s_multi_elapsed` 진입값 = `1u`**(implementer가 plan `0u`에서 정정 → spec §3.3·plan 정정 완료): 진짜 근거 = **초음파-on span**(weld_start→weld_stop)이 슬라이스1(`ldt2−1` tick)과 일관 — multi `elapsed=1`이면 span=`time2−1`, `elapsed=0`이면 1틱 김(weld_steps 카운트 일치는 부산물). host-test는 총 duration+전환 count/amplitude만 핀; **정상(time1<time2) 전환 tick index(out1 duration=`time1−1`)는 미검증** → ±1틱(10ms) 정밀도는 슬라이스4 실 초음파 HW 확인.
- **범위=weld-only**(슬라이스2 선례): `multi_ctrl=ON` 직접 START(TOUCH/COMM)는 스테핑 안 함=단일 진폭 on-time ceiling 종료(spec §6 deviation). 이연(spec §11): 직접 RUN 스테핑·`limit_mo_out` config 클램프(슬라이스4)·mid-cycle 토글 래치·절대 진폭 보정(6b).
- **다음 = 보드 세션**(이 브랜치): ① 직접-초음파 무회귀(START→~560ms ceiling+ICON_RUN) ② `multi_ctrl=ON` 직접 START 스테핑 안 함(§6 deviation) ③ work_cnt 0(dormant) → `--no-ff` 머지 + 태그 `hw-revA_fw-stage-weld3`(host + HW-regression verified; 스테핑 E2E는 슬라이스4).

### 2026-06-14 e — weld-cycle 슬라이스2 (energy_ctrl) HW 회귀확인 PASS + main 머지 + 태그

보드 연결 세션. 슬라이스2(energy 기반 WELD exit) 플래시 후 기존 경로 무회귀를 mbpoll RTU(SERIAL/addr=1/9600/EVEN)로 확인 → main 머지 + 태그.

- **HW 회귀 PASS**: ① **직접-초음파 ceiling 무회귀**(START reg0x1B→STATUS bit0 545–609ms 밴드=ON_TIME 56×10ms, slice-1 동일) + LCD **ICON_RUN 켜짐→꺼짐 육안**(user, 3회) ② **§6 deviation 확인**: energy_ctrl=ON 직접런이 에너지 조기종료 없이 전 ceiling 완주(에너지-exit은 weld FSM 전용, app_reg 런 경로에 없음) ③ **work_cnt 0 유지**(weld FSM dormant=프로덕션 트리거 없음 구조증명; 직접 COMM런 미증가) ④ **Modbus FC03 config 미러 전건 일치**(OUT_POWER 55/ON_TIME 56/ENERGY 567/TIMEOVER 8/work_cnt 0/STATUS 0) + FC06(EN_ENERGY write+restore) 무회귀.
- **#2 누산 점등 = 벤치 한계(6b 이연, 사용자 수용)**: **DISP_POWER=0**(실 초음파 무신호)→curr_power 0→curr_energy 0, 누산 관측 불가. 누산 로직은 host-test 검증; 절대 에너지는 6b/실 초음파 rig(spec §4.2/§11).
- **머지** `d32d014`(`--no-ff`) + 태그 `hw-revA_fw-stage-weld2`(⚠ **host + HW-regression verified** — energy 누산/exit E2E는 6b/실 rig 필요). feature 브랜치 삭제. 머지 후 0-warning(우리 코드, text 53776B/FLASH 41.46%)·host 4스위트 PASS.
- 보드 종료 = SERIAL/addr=1/9600/EVEN, OUT_POWER 55, EN_ENERGY 0(복원), ether_ip .70(무변경).
- ⚠ **레지스터 정정**: DISP_ENERGY = wire **0x05**(mbpoll `-r 6`), **0x16 아님**(0x16=EN_SAFTY). slice2 d블록/RESUME/NEXT_STEPS의 "0x16" 오기 정정.
- **다음 = 슬라이스3(multi_ctrl, HW 불요)** 또는 HW-gated(B-SEAM OSC·6b cal·SEEK/RESET·overload·슬라이스4).

### 2026-06-14 d — weld-cycle 슬라이스2 (energy_ctrl) CODE-COMPLETE (host+build verified, 미머지)

samd20 energy 기반 WELD 종료(`energy_ctrl && curr_energy>=limit_energy`) 포팅. `superpowers:brainstorming → spec → writing-plans → subagent-driven`(plan 5 Task, Task별 fresh subagent + 2-stage 리뷰[spec 준수 + cpp-reviewer]). 브랜치 `feat/stage-weld-cycle-slice2-energy` **미머지**(HW 회귀확인=보드 게이트, spec §10). 빌드 **0-warning(우리 코드)**(FLASH 41.46%/text 53776B; vendor wiznet socket.h 3경고는 stage C부터 기존, slice2 무관), 호스트 **4스위트 PASS**(weld_fsm 12함수, reg_calc 에너지 헬퍼 포함).

- **아키텍처 Option A**(advisor): 누산 acc_energy/curr_energy는 `app_reg`(samd20 ADC-ISR 동형), WELD 진입 리셋은 `US_CMD_START` 핸들러 1줄(run-start 엣지=공짜). 순수 FSM은 curr_energy 주입받아 비교만.
- **산출물**: `app_reg_calc.reg_energy_from_acc(acc/250)` 순수 헬퍼(host-test; 2ms publish가 samd20 1ms·/500 재현, 절대보정 6b/HW) / `app_weld_fsm` WELD energy-exit + backstop abort(limit_out_time×100 tick, floor 1s → US정지+실린더상승+READY+work_cnt 미증가+`weld_fault` 엣지) + comp_time/temp_time 디커플 / `app_reg` 누산+last_energy 래치(START 리셋·RUN_RELEASE·ceiling) / `app_weld` 글루 4필드+curr_energy 주입 + fault hook(mon).
- **커밋 6**: `bc43ed5`(헬퍼+test)→`260ed00`(test 헤더 LOW-2)→`6a48931`(FSM)→`a387624`(M1 backstop floor+회귀test)→`8cdd651`(app_reg 누산+LOW-1 cross-ref)→`7825ab5`(글루+fault hook).
- **리뷰**: Task별 spec✅ + cpp-reviewer APPROVED-WITH-COMMENTS(0 Crit/High), 최종 통합 APPROVED/READY-pending-HW. 수정: M1(limit_out_time=0 즉시-fault → floor 1s). 이연(spec §11): limit_energy=0 즉시-exit(samd20 충실, config-validation 슬라이스)·mid-cycle energy_ctrl 토글(슬라이스4 래치)·acc 24분 wrap/uint16 truncation(display-only, samd20 충실).
- **범위=weld-only**(사용자 확정): energy_ctrl=ON 직접 START(TOUCH/COMM)는 에너지 자동정지 안 함=on-time ceiling 종료(samd20 5270 일부 deviation, spec §6).
- **다음 = 보드 세션**(이 브랜치): 직접-초음파 무회귀 + DISP_ENERGY(Modbus 0x16)/VAR_ENERGY 누산 점등 + energy_ctrl=ON 직접런 ceiling 종료(deviation) 확인 → `--no-ff` 머지 + 태그 `hw-revA_fw-stage-weld2`.

### 2026-06-14 c — weld-cycle 슬라이스1 HW 회귀확인 PASS + main 머지 + 태그

보드 연결 세션(ST-LINK V3 플래시, USART6=Modbus 9600 8E1 addr 1 = 보드 벤치 기본). 슬라이스1 dormant `app_weld_tick` 삽입의 **기존 경로 무회귀**를 실보드로 확인 후 `finishing-a-development-branch`로 main 머지 + 태그.

- **HW 회귀 PASS**: ① **FC03 미러 = SWD 직독 g_cfg 전건 일치**(OUT_POWER 55 / ON_TIME 56 / ENERGY 567 / work_cnt 0 / DELAY 53·52·51 / TRIGGER 50·50 / MULTI 25·50·25·50 / MODEL 1·1 / STATUS 0) → 레지스터 publish 경로가 weld tick 삽입 후 무회귀 ② **START(reg 0x1B)→STATUS bit0=1→~560ms ceiling 자동정지**(STATUS 폴 0.078~0.558s=1, 0.638s=0; limit_on_time=56×10ms, t0=write 직후 오프셋 감안 537–617ms 밴드 내) + LCD **ICON_RUN 점등→소등 육안**(user, 3회 재현) ③ **work_cnt 0 유지** = weld 사이클 미발화(dormant **구조증명** = `app_weld_request_start` 프로덕션 호출자 없음) + US_COMM 직접런은 work_cnt 미증가 ④ 명령 레지스터 consume-and-clear, clean idle 복귀.
- **mon 비가용**: 보드 SERIAL/addr=1이라 USART6=Modbus 점유 → 115200 mon 글리치만. dormancy는 코드증명이라 mon `[weld]` 부재 확인 불요(advisor); Modbus 매트릭스가 공유 run FSM 무회귀를 더 강하게 입증.
- **머지** `718678b`(`--no-ff`) + 태그 `hw-revA_fw-stage-weld1`(⚠ **host + HW-regression verified** — 사이클 자체 E2E 아님, 슬라이스4). feature 브랜치 `feat/stage-weld-cycle-slice1` 삭제. 머지 후 0-warning(우리 코드, text 53500B/FLASH 41.25%)·호스트 4스위트 PASS.
- 보드 종료 상태 = SERIAL/addr=1/9600/EVEN, OUT_POWER=55, ether_ip=.70 (as-found, 무변경).

### 2026-06-14 b — weld-cycle 슬라이스1 (DELAY FSM) 구현 완료 (host-verified, 미머지)

samd20 공압 프레스 weld-cycle FSM(`READY→CYL1→WELD→HOLD→CYL2→work_cnt++`) **DELAY 모드** 포팅. `superpowers:subagent-driven-development`로 plan 6 Task 전부 구현(Task별 fresh subagent + 리뷰 + host-test 게이트). 브랜치 `feat/stage-weld-cycle-slice1` **미머지**(사용자 "보드 먼저" — 다음 보드 세션에서 기존 직접-초음파 무회귀 HW 확인 후 머지+태그). 빌드 0-warning(우리 코드, FLASH 41.25%/RAM 16.70%), 호스트 **4스위트** PASS, 최종 cpp-reviewer APPROVED.

- **신규 모듈**: `app_weld_fsm.{h,c}`(HAL-free 순수 FSM 코어, host-test 8함수 = init/완전사이클/타이밍/진폭 0·63·127/comp_time 감쇠 127−40=87/언더플로 가드 0/start READY-only/cycle_done 단일엣지) + `app_weld.{h,c}`(글루: `sys_tick_get_ms` 10ms 게이트 → live cfg 주입 → `weld_fsm_step` → SOL_DN hook·`app_weld_hook_set_amp`·`app_reg_command(US_CYCLE START/RUN_RELEASE)`·`work_cnt++`+FRAM+LCD).
- **통합**: `app_lcd.h` `us_run_status` enum에 `US_CYCLE=4`(WELD가 게이트 구동 소스; `app_reg_command`가 src 그대로 저장→분기 불필요) + `app_reg.c` on-time ceiling 블록 **comment-only**(US_CYCLE은 TOUCH/COMM 조건에서 자연 제외, 로직 무변경 — HW 검증 코드 보호) + `main.c`(`app_weld_init` boot)·`app.c`(`app_weld_tick` app_lcd 뒤/app_reg 앞 배선 → 같은 iter publish 반영).
- **설계 버그 수정(구현 전, advisor+1차증거)**: plan 글루가 `app_lcd_hook_set_pot(out.amplitude)`를 호출했는데 이는 **이중 변환** — 기존 hook은 output_power(50..100)를 받아 내부에서 `(x-50)*255/100` 적용(`app_lcd.c:28`)하나, 코어 `amplitude`는 이미 보정된 DAC(0..127) = samd20 `temp_i`를 I2C_POT에 직접 쓰는 값(`ref/samd20/main.c:1549`). op=100→127→196 오류. → **전용 `app_weld_hook_set_amp(uint8_t dac)` raw-DAC hook 신설**(spec §6 정정, docs `e550414`). comp_time 감쇠는 DAC 도메인이라 output_power로 역산 불가 → raw 경로 필수.
- **충실도 = 혼합**: 거동 samd20 충실 + 진폭 언더플로 가드 1건 수정(저전력+짧은용접 시 uint8 언더플로→큰 진폭 안전위험 → uint16 계산 후 `amp>=reduction? amp-reduction : 0` 클램프).
- **cpp-reviewer LOW 2건**: ① **LOW-2 fix**(`f482af9`) — `weld_fsm_step` default fault-path(정상 도달 불가)에서 `s_sol_dn=0` fail-safe(공압 프레스 솔레노이드 잔류 방지; SOL_DN 실 GPIO는 슬라이스4). ② **LOW-1 deferred = 슬라이스4 must-fix** — `weld_amplitude`의 `output_power<50` 언더플로: Modbus는 `app_modbus.c`에서 [50,100] 클램프하나 **LCD `app_lcd_input.c:752` `LV_OUT_POWER`는 클램프 없음**. 슬라이스1 dormant(프로덕션 트리거 없음, hook 로그만) → 무수정. **슬라이스4에서 물리 트리거+실 I2C_POT 연결 시 HIGH** → LCD 입력에 Modbus와 동일한 `if(data16<50u) data16=50u;` 클램프 미러 필요(기존 직접-초음파 set_pot도 동일 노출 = pre-existing).
- **트리거 분리(samd20 충실)**: weld 사이클 = 물리 SW_START1/2 전용(슬라이스4); 패널/Modbus START = 직접 초음파 무수정. 슬라이스1은 프로덕션 트리거 미와이어 → FSM은 host-test로만 검증, HW에서는 READY 휴면(직접-초음파 무회귀).
- 커밋 8: `e550414`(docs 설계수정)·`aef0885`·`fba8001`·`d1c680d`·`70992f8`·`0cda1e4`·`26913f9`·`ba05147`·`f482af9`. **다음**: 보드 연결 → 직접-초음파 무회귀 확인 → 머지(`--no-ff`)+태그 `hw-revA_fw-stage-weld1`(⚠ host-only 태그 = HW 회귀확인 시점 명시).

### 2026-06-13 j — Stage C slice 2 deferred HW 검증 (보드 ETH_DHCP .70 연결) — 코드 무수정

보드 연결 세션(ST-LINK V3, W5500 실장 + DHCP 망, 보드 부팅 시 ETH_DHCP 리스 `.70`). slice 2 deferred HW 항목 중 ① ICON_RUN, ③ RAM-only 재리스 검증. **펌웨어 무수정**(분석·확인만).

- **① ICON_RUN over TCP = PASS**: `mbpoll -m tcp` START(reg 28 wire 0x1B)=1 → `US_COMM` run 진입(STATUS reg 30 bit0=1) → **on-time ceiling 자동정지 정량 측정 537–617ms = 목표 560ms**(`limit_on_time=ON_TIME=56`×10ms) → IDLE 복귀. ceiling은 `app_reg.c:234`에서 `US_COMM`에도 적용(`#ifdef REG_TRACE`라 프로덕션엔 mon 트레이스 없음 — STATUS 폴로 측정). START 6회 연속 발사로 **ICON_RUN 6/6 육안 확인**(user). START는 `US_IDLE`에서만 arm(`app_reg.c` guard)이라 1.5s 간격 안전.
- **③ RAM-only 재리스 = 동작 규명(코드 확정), 현 상태 유지(user 수용)**: LCD `comm_mode=ETH_STATIC` SAVE + 전원사이클 후 IP가 `.128`(이전 static)이 아닌 **`.70`(직전 DHCP 리스)으로 굳음**. 사용자 가설("저장 시 IP 필드에 남아있던 .70이 같이 저장")이 **코드로 확인됨**. 체인: ⓐ DHCP 리스 `.70`이 `cfg->ether_ip[]`에 미러(`app_eth.c:64-70`, eth 코드 자체는 FRAM 미기록=주석대로 RAM-only) → ⓑ comm/ether 페이지 진입 시 섀도우 시드 `temp_ether_ip=cfg->ether_ip=.70` + IP 필드 렌더(`app_lcd_render.c:147-153`)=**필드에 .70 표시된 이유** → ⓒ comm_mode→STATIC만 변경(IP 미편집) → ⓓ DATA_SAVE → `app_config_save_all(cfg)`이 `cfg->ether_ip(.70)`을 FRAM 기록(`app_config.c:611,73`) → ⓔ 재부팅 시 FRAM에서 `.70` 로드(`app_config.c:118`) → ⓕ static 경로가 `cfg->ether_ip=.70` 사용 → `[eth] up ip=192.168.1.70`(static 배너, `app_eth.c:116-130`). **핵심**: eth 모듈 "RAM-only(no FRAM)"는 문자 그대로 사실이나, 리스가 static과 **동일 필드**에 미러되므로 LCD 전체-설정 저장 경로를 통해 FRAM에 새어듦(WYSIWYG; "DHCP 받은 IP를 static 고정" 워크플로로는 합리적). 공장 기본 `ether_ip=0.0.0.0`(`app_config.c:35`)이라 이전 `.128`은 하드코딩 기본이 아닌 과거 입력값 → `.70`으로 대체됨. **사용자 결정: 무수정·현 상태 유지.** 메모리 `project_eth_dhcp_static_persist` 기록.
- **② RTU FC06 회귀 = PASS**(보드 LCD `comm_mode=SERIAL`+`addr=1`+`9600/EVEN` SAVE+전원사이클, RS-485 마스터 `mbpoll -m rtu -b 9600 -P even`). config는 openocd로 `g_cfg`(0x20000a5c)+0x2A 직독 확정(`addr=01 speed=02 parity=00 mode=00`). 매트릭스: FC03 regs 7-9 = `55/56/567`(=TCP 미러 동일) / FC06 80→80(범위내)·30→**50**(클램프 LOW)·120→**100**(클램프 HIGH)·55→55(복원). **slice 2(TCP/W5500)가 RTU 경로 무회귀** 입증. 첫 read 1회 transient 타임아웃→재시도 정상(포트 워밍업).
- 보드 현 상태: **SERIAL/addr=1/9600/EVEN**(핸드오프 벤치 기본과 일치), OUT_POWER=55, FRAM `ether_ip=.70` 잔여(무해). 빌드/git 무변경(0-warning, 호스트 3스위트 PASS, 작업트리 클린).

### 2026-06-13 i — Stage C slice 2 (Modbus TCP static+DHCP) main 머지 + 태그

Option B(2a/2b 합쳐 2b 통째)로 main 머지. `finishing-a-development-branch` 절차(게이트 검증→환경 감지→머지→태그→브랜치 삭제).

- **머지** `fa93dcd`(`--no-ff` 2-parent `8ec57ec`+`e3e10d7`) — 선형 후손(main..HEAD=26, HEAD..main=0)이라 깨끗. 2b가 2a(`faf89d5`) 전부 포함 → static+DHCP 모두 합류.
- **태그** `hw-revA_fw-stage-c2b`(merge commit). **⚠ pre-refactor 2a에 `c2a` 태그 안 함** — 2a 원본 app_eth는 confirmed 1s-폴 PHY 버그 보유(`c2b`가 static+DHCP 전부 커버).
- feature 브랜치 2개 삭제(`git branch -d` = main에 완전 포함): `feat/stage-c-modbus-tcp-dhcp`, `feat/stage-c-modbus-tcp-static`.
- 머지 후 클린 재빌드 0-warning(text 52728B/FLASH 40.65%/RAM 16.66%)·호스트 3스위트 PASS. tip `62c1cc1` 빌드 보드 재플래시 HW 재확인(dhcp lease .70).
- 핸드오프 `HANDOFF.md`(루트) slice 2용 재작성(시리얼 캡처 함정 + 측정 교훈 + ETH E2E 재현 절차). **Stage C(Modbus 흡수) = slice 1(RTU) + slice 2(TCP static+DHCP) 전부 main 완료.**
- **다음**: ① slice 2 deferred HW(ICON_RUN 육안/RTU FC06 회귀/RAM-only 재리스) ② HW-gated deferred(SEEK/RESET·overload·weld-cycle·B-SEAM·6b) ③ 신규 스테이지.

### 2026-06-13 h — Stage C slice 2b HW E2E PASS (static+DHCP+retry) + 비블로킹 PHY 리팩터

보드 연결 세션(ST-LINK V3, W5500 실장 + DHCP 서버 망, mon `/dev/cu.usbserial-AB0MLYXA`@115200). slice 2b(+2a static) 실보드 HW E2E. **실 버그 1건 발견·수정**(slice 2a app_eth PHY 폴링 타임아웃) → 비블로킹 FSM 리팩터. 코드 2커밋(`635e9ec`+`62c1cc1`), 브랜치 tip `62c1cc1`(미머지). 빌드 0-warning·FLASH 40.65%(53288B/128KB)·RAM 16.66%·호스트 3스위트 PASS.

- **버그 발견 (HW)**: `comm_mode=ETH_STATIC`로 부팅 시 `[eth] no PHY link — unavailable` — **링크 LED는 켜져 있는데** 펌웨어가 못 잡음. 진단 빌드(타임아웃 5s+폴마다 로그)로 측정 → `rc=0` 내내 정상, **링크가 reset 후 t=1490ms에 업**. 원인 = `app_eth_init`의 PHY 폴링이 **1s(100×10ms) 1회**뿐이라 ~0.5s 일찍 포기(W5500 PHY는 NRST 후 ~1.5s 오토네고). static·DHCP 둘 다 영향(폴이 comm_mode 분기 앞). **이 폴링은 slice 2a 코드의 버그.**
- **수정 `635e9ec` (비블로킹 FSM 리팩터, advisor 설계검토)**: `app_eth`를 상태머신(`ETH_DOWN`/`ETH_LINKWAIT`/`ETH_STATIC_UP`/`ETH_DHCP_RUN`)으로. `app_eth_init`=빠른 칩 init만(블로킹 폴 제거, 즉시 리턴). `app_eth_tick`=`LINKWAIT`에서 100ms마다 `CW_GET_PHYLINK` 폴 → 링크 업 시 `eth_apply_on_link`(static netinfo 또는 MAC선설정+DHCP 시작). 브링업 **무조건**(SERIAL 포함, advisor: SERIAL→런타임 ETH_STATIC 무재부팅 동작 보존; mon은 SERIAL서 게이트오프라 무해). 링크 드롭 후 재획득=핫플러그(deferred). MAC-before-DHCP 유지. `stm32f4xx_hal.h`/`HAL_Delay` 제거.
- **HW E2E 전수 PASS**:
  - **비블로킹 부팅**: `[eth] chip up — waiting for PHY link...` 후 슈퍼루프 진행, 링크 업 시 `[eth] up`.
  - **재시도 복구(핵심)**: 케이블 빼고 부팅→`LINKWAIT` 유지(`up` 없음, 보드 정상)→케이블 꽂기→**무재부팅** `[eth] up ip=192.168.1.128`. tick 링크 재시도 복구 입증(단순 타임아웃 연장으론 불가능한 검증).
  - **static**: `[eth] up 192.168.1.128`, ping 0% loss, FC03 미러(`[7]=55 OUT_POWER/[8]=56 limit_on_time/[9]=567 energy`), FC06 클램프(80→80/30→**50**/120→**100**/55 복원) — 공유 apply-path over TCP.
  - **DHCP(2b 본 목표)**: `[eth] dhcp init`→`[eth] dhcp lease ip=192.168.1.70`, ping 0%, FC03 미러 동일, **LCD에 리스 IP 표시(RAM-only 미러)**. 리스+ARP 성공 = **MAC-before-DHCP 수정으로 올바른 MAC(00:08:dc:78:91:71) 바인딩** 입증.
- **리뷰 `62c1cc1`**: 리팩터 통합 cpp-reviewer = APPROVED-WITH-COMMENTS(Crit/High 0; FSM 상호배타·wrap-safe 타이머·MAC순서·s_available 일관성 정적 확인). 지적 3건 전부 반영 = ① `app_eth.h` 배너 정정(링크 미업 시 false 리턴→LINKWAIT 유지) ② `<stdint.h>` 직접 include(HAL 헤더 제거로 transitive 의존 제거) ③ DHCP_FAILED re-init 시 `s_dhcp_ms` 리셋(케이던스 명확화).
- **deferred(사용자 결정 "핵심으로 충분")**: ICON_RUN 육안(START→560ms ceiling→STOP over TCP), RTU FC06 회귀 spot-check(RS-485), RAM-only 재리스 증명(static 복귀→.128 표시).
- **재플래시 확인**: tip `62c1cc1` 빌드(text 52728)를 보드에 재플래시 + HW 재확인(`[eth] dhcp lease ip=192.168.1.70` + ping OK) = "tip 바이너리가 보드에서 검증됨" 보장(리팩터 검증은 `635e9ec`에서, 리뷰 fix 델타는 inert지만 기록 정확성 위해 재확인).
- **⚠ 머지 영향(다음 세션 결정 필요, 사용자 콜)**: 비블로킹 리팩터로 2b의 `app_eth`가 2a와 **더 이상 byte-identical 아님**(static 경로가 FSM 안으로 이동). slice 2a 원본 app_eth(블로킹 1s 폴)는 **confirmed HW 버그 + HW 미검증**(static은 2b 리팩터 빌드에서만 검증됨). **권장(기본) = 2a/2b split 합쳐 2b 통째 머지**(2b가 2a 전부 포함+검증, 2a static은 strict subset). **⚠ pre-refactor 2a에 `hw-revA_fw-stage-c2a` 태그 금지(known-broken).** Option A(2a가 리팩터 cherry-pick→재검증)는 가능하나 busywork.

### 2026-06-13 g — Stage C slice 2b (Modbus TCP/W5500 DHCP): 구현 완료(host-complete), HW E2E 대기

HW 비의존 세션. plan(`166fe78`, 5 task)을 subagent-driven으로 실행 — Task 1~4 완료(구현+host 게이트+per-task 리뷰+통합 리뷰+advisor 검증), Task 5(HW E2E)만 보드 게이트로 미실행. 코드 6커밋. 브랜치 `feat/stage-c-modbus-tcp-dhcp`(base = 2a tip `faf89d5`, **stack** — 2a W5500 스택 의존, 미머지). 클린 빌드 0-warning(우리 코드)·**FLASH 40.62%(53240B/128KB)·RAM 16.65%**·호스트 3스위트(`app_reg_calc`+`app_modbus_core`+`app_modbus_tcp_frame`) 전수 PASS(2b 새 호스트 테스트 ✗ — DHCP=HAL/소켓/콜백 글루). 통합 cpp-reviewer = APPROVED-WITH-COMMENTS(Crit/High 0).

- **T1 `3a62233`**: ioLibrary DHCP `Internet/DHCP/dhcp.{c,h}` 벤더(**2a와 같은 핀 커밋 `220ca7a6`**, master tip ✗) + `wiznet` lib에 `dhcp.c`·`Internet/DHCP` include 추가(경고격리 `-w`·SYSTEM 유지). dhcp includes = `socket.h`(이미 벤더)+`stdio.h`만(추가 벤더 불필요). **controller가 `dhcp.h` grep해 API 확정→T2 프롬프트 주입**(load-bearing): `reg_dhcp_cbfunc`=3-arg(assign,update,conflict), `DHCP_init(uint8_t s,uint8_t*buf)`, `DHCP_run→u8`, `DHCP_FAILED=0`, `get{IP,GW,SN,DNS}fromDHCP(u8*)`, `NETINFO_DHCP=2`(wizchip_conf W5500 클래식 struct). dhcp.c는 우리 코드서 미참조→`--gc-sections` 드롭(격리 빌드 green이 목표).
- **T2 `5b84f71`(+`a403326` 정렬 minor)**: `app_eth` DHCP 라이프사이클(`app_eth.{c,h}`만 수정) — DHCP 분기(`DHCP_init`+`reg_dhcp_cbfunc`, `s_dhcp_active=true`, `return true`지만 **`s_available=false` 유지=논블로킹 boot**), `dhcp_ip_assign`(get*fromDHCP→`wizchip_setnetinfo`+**RAM-only cfg 미러**(`app_lcd_cfg()` write, FRAM 커밋 ✗)+`s_available=true`), `dhcp_ip_conflict`(**non-fatal**, samd20 `while(1)` 폐기), `app_eth_tick`(1s `sys_tick_get_ms` 게이트 `DHCP_time_handler` **항상**+`DHCP_run`; `DHCP_FAILED`→re-`DHCP_init` keep-retry, **static 폴백 ✗**). **static 경로 verbatim 보존**(`const cfg` 선언 branch 위로 hoist만). sock1(TCP=sock0), `s_dhcp_buf[1024] _Alignas(uint32_t)`(리뷰 minor — RIP_MSG* 캐스트). **spec+quality 2단 리뷰 APPROVED**(advisor 5개 우려 전부 정적 분석 benign 해소).
- **T3 `08b4004`**: `app.c app_loop_iter`에 `app_eth_tick()` 배선 — `app_modbus_tick()` **직전**(이번 iter 리스 획득이 게이트(`comm_mode!=SERIAL && app_eth_available()`) 읽기 전 available 플립). `app_eth_init()`은 boot(`main.c:29`, 2a) 이미 배선 → 라이프사이클 end-to-end 연결. ELF text 48120→52628(+4.5KB=app_eth_tick+DHCP lib 도달).
- **T4 통합 cpp-reviewer = APPROVED-WITH-COMMENTS**: 정적 확인 2건 = ① **소켓 분리**(`MB_TCP_SOCK=0`/`SOCK_DHCP=1`, 충돌 없음) ② **콜백 생존**(벤더 `dhcp.c:1028-1063` 확인 — `DHCP_init` 재호출이 콜백 포인터 미손상 → keep-retry 안전). Minor 3 = ① 콜드스타트 1회 즉시 `DHCP_time_handler`(무해, 무변경) ② `s_prev_ms=now` stretch-safe(의도, 무변경) ③ `main.c:30` 주석 `comm_mode==ETH`→`ETH_STATIC/ETH_DHCP` 정정(`86a3923`로 반영, 2b가 stale 만든 주석).
- **완료 후 advisor 검증 → 실버그 1건 수정 `b7ce6b0`**(spec 7요건 미포함·호스트 미커버라 spec/quality/통합 리뷰 3건 모두 누락): DHCP 분기가 `DHCP_init` **전에 칩 MAC(SHAR) 미설정**. 벤더 `DHCP_init`(dhcp.c:1030)이 `getSHAR(DHCP_CHADDR)` 읽고 all-zero(post-reset)면 temp `00:08:dc:00:00:00` 대체("set SHAR before call" 주석) → DISCOVER/REQUEST가 `kEthMac`(…78:91:71) 아닌 temp MAC으로 나가고, 리스 후 MAC이 바뀌어 갱신 CHADDR stale → MAC-aware 망에서 간헐 실패 + samd20 불충실. samd20 = network-init `wizchip_setnetinfo`→`setSHAR`로 선설정(ref/samd20/wizchip_conf.c:855, dhcp.c:897 동일 getSHAR 패턴). **수정 = DHCP 분기에서 `DHCP_init` 직전 `wizchip_setnetinfo(&dni)`(MAC만, IP/SN/GW=0; DHCP_init이 SIPR/GAR 어차피 0클리어)로 `kEthMac` SHAR 선설정.** re-init/lease idempotent. 0-warning·host PASS, text 52628→52680.
- **다음 = Task 5 HW E2E**(보드+W5500+DHCP 서버 망 게이트, 코드 변경 기대 ✗): `comm_mode=ETH_DHCP(2)` 설정→전원사이클→mon `[eth] dhcp init — acquiring lease...`→`[eth] dhcp lease ip=a.b.c.d` 확인(DHCP 서버 없으면 acquiring 유지+`available()`=false=**keep-retry non-fatal 입증**, RTU/mon/LCD 무영향)→`mbpoll -m tcp -a 1 ...` 매트릭스(FC03 미러/FC06 클램프+FRAM/START-560ms ceiling-STOP+ICON_RUN/**LCD 리스 IP 표시=RAM-only**(전원사이클→재리스, FRAM ether=0)/RTU FC06 회귀 spot-check). 통과 시 `finishing-a-development-branch` 머지(`--no-ff`)+태그 `hw-revA_fw-stage-c2b`(**2a 머지 후, 2a→2b 순서**).

### 2026-06-13 f — Stage C slice 2b (Modbus TCP/W5500 DHCP): brainstorming → spec → plan

HW 비의존 세션 연속(slice 2a 구현 완료 후). slice 2b(DHCP) 착수 — 설계 단계만(코드 변경 ✗). 브랜치 `feat/stage-c-modbus-tcp-dhcp`(base = slice 2a tip `faf89d5`, stack — 2a의 W5500 스택 의존, 미머지).

- **brainstorming**(질문 4개): ① 범위 = **DHCP 획득만**(핫플러그 재init + SERIAL boot-skip 이연) ② 획득 = **논블로킹 슈퍼루프**(samd20 충실; boot 비블록, 리스 전 `app_eth_available()`=false로 TCP 대기) ③ 실패 = **미획득 유지+계속 재시도**(static 폴백 폐기 — DHCP면 ether=0.0.0.0; conflict는 non-fatal 라이브러리 decline/재요청, samd20 `while(1)` halt 폐기) ④ 획득 IP = **RAM만**(in-RAM cfg 미러로 LCD 표시), FRAM 미영속(samd20 충실).
- **컨텍스트 탐색**(Explore): samd20 `main.c`/`W5500/dhcp.c` DHCP 흐름(`DHCP_init`/`reg_dhcp_cbfunc`/`DHCP_run`/1s `DHCP_time_handler`/getIP·GW·SN·DNSfromDHCP), ioLibrary `Internet/DHCP/dhcp.{c,h}`만 추가 벤더(deps=이미 벤더된 socket.h), 2a가 comm_mode==2를 NETINFO_STATIC로 잘못 처리, LCD COMM_ETH_DHCP=2 선택/토글·DISP_EN_DHCP 존재, FRAM comm_mode@44/ether@45~56 영속(DHCP면 0), `sys_tick_get_ms()`(1kHz) 위 1s 게이트, `app_loop_iter`=app.c:64.
- **spec** `5444aee` (`docs/superpowers/specs/2026-06-13-stage-c-modbus-slice2b-dhcp-design.md`): 2a 위 DHCP 라이프사이클만 추가(`app_eth` 수정 + 신규 `app_eth_tick()`), 코어/프레이밍/transport/spi1 무변경. `app_eth_available()` 의미 "칩+링크+IP준비"로 확장 → modbus tick 게이트 무수정 재사용. §3.2 정정(획득 중에도 1s 틱), §3.3 conflict halt·static 폴백 폐기.
- **plan** `166fe78` (`docs/superpowers/plans/2026-06-13-stage-c-modbus-slice2b-dhcp.md`, 5 task): T1 벤더 `Internet/DHCP/dhcp.{c,h}`(같은 핀 커밋 `220ca7a6`, 경고격리 lib) → T2 app_eth DHCP 라이프사이클(static 경로 verbatim 보존 + DHCP 분기/콜백/`app_eth_tick`) → T3 `app.c` 슈퍼루프 배선(`app_modbus_tick` 직전) → T4 빌드/host 게이트+cpp-reviewer → T5 HW E2E(보드+W5500+DHCP 서버 망 게이트). 새 호스트 테스트 ✗.
- **다음 = plan Task 1부터 구현**(subagent-driven = option 1, slice2a 패턴). 호스트 게이트 후 HW E2E → 머지+태그 `hw-revA_fw-stage-c2b`(2a 머지 후).

### 2026-06-13 e — Stage C slice 2a (Modbus TCP/W5500 static): 구현 완료(host-complete), HW E2E 대기

HW 비의존 세션. plan(`79526fb`, 9 task)을 subagent-driven으로 실행(Task 1~8 = 구현, 각 Task 구현→리뷰 게이트, Task 9 = 보드 게이트로 미실행). 코드 7커밋. 브랜치 `feat/stage-c-modbus-tcp-static`(base = `8ec57ec`, slice 1 머지 후 docs 커밋). 클린 빌드 0-warning(우리 코드), **FLASH 36.43%(47748B/128KB)·RAM 13.23%**, 호스트 3개 스위트(`app_reg_calc`+`app_modbus_core`+신규 `app_modbus_tcp_frame`) 전수 PASS.

- **T1 `478b50e`**: WIZnet ioLibrary upstream 벤더(핀 커밋 `220ca7a6`, 6파일 `Ethernet/{wizchip_conf,socket}.{c,h}`+`W5500/w5500.{c,h}`). 경고격리 별도 STATIC lib(`-w`, SYSTEM include). **현 master 기본 `_WIZCHIP_=W6300`→`W5500`로 1줄 수정**(딱 이것만) → `wiz_NetInfo`가 클래식 `{mac,ip,sn,gw,dns,dhcp}`+`reg_wizchip_spi_cbfunc` 2-arg 형. controller가 벤더 헤더에서 전 API 이름/시그니처 확정해 후속 Task 프롬프트에 주입.
- **T2 `c512bf3`**: `spi1.{c,h}` — SPI1 마스터 Mode0 12MHz, PA5/6/7(AF5)/PA4(소프트 CS, idle-HIGH)/PC5(NRST)/PC4(INT 폴링). HAL SPI 모듈 enable + 소스 추가. 리뷰 Minor 반영: `HAL_SPI_Init` 반환 체크+`Error_Handler`(형제 드라이버 컨벤션).
- **T3 `3b01d0b`**: 순수 MBAP 프레이밍 `app_modbus_tcp_frame.{c,h}` — TDD(RED→GREEN). MBAP strip+검증, `mb_core_decode(MB_MODE_TCP)`, trailing CRC 2B 절단, unit 에코, 빅엔디안 length. 호스트 테스트(FC03=11B/FC06=12B/reject 3종). 리뷰 Minor 반영: `n<4` 방어 가드 + unit≠device_addr **unit-echo 테스트** 추가.
- **T4 `90e72e4`**: `apply_writes`(static)→`app_modbus_apply_writes`(public) + `app_modbus_core()` accessor. **순수 linkage 변경, 로직 무변경**(clamp/FRAM/mirror 본문 무수정 — controller diff 검증). RTU(tag `hw-revA_fw-stage-c1`)와 TCP가 동일 `g_mb`+동일 apply 공유.
- **T5 `d310527`**: `app_eth.{c,h}` — W5500 non-fatal 브링업(콜백 등록→reset→`wizchip_init`→PHY 폴링 ~1s 타임아웃→cfg static netinfo). **byte 콜백 `spi1_rb/wb`는 CS 미토글**(ioLibrary가 프레임 전체를 CS 콜백으로 감쌈). MAC 하드코딩. `%u` `(unsigned)` 캐스트. cpp-reviewer APPROVED.
- **T6 `dcc0c6a`**: `app_modbus_tcp.{c,h}` — 소켓 FSM(port 502, sock0) + recv→frame→공유 apply→send, 1-frame-per-recv 가정. 리뷰 Minor 반영: send-first 재정렬(RTU와 동일 순서) + 주석 정정.
- **T7 `3eb516f`**: 통합 — `main.c` boot `app_eth_init()` 1회; `app_modbus_tick()` RTU/ETH 분기. **RTU 분기 기능 동일(무회귀, controller diff 검증)**, ETH 분기가 `comm_mode!=SERIAL && app_eth_available()` 게이트 + `g_tcp_active` 래치 1회 재시드 + **mirror gap 수정**(ETH에서도 `mirror_live()` 호출). 리뷰 Minor 반영: rising-edge `mb_core_init` 직후 baseline `mirror_live()`(zeroed-holding 읽기 창 제거, apply_config 획득 패턴과 대칭).
- **T8 통합 cpp-reviewer = APPROVED-WITH-COMMENTS**(Critical/High/Medium 0). Minor 3건 = 전부 비차단/2a 의도: ① recv/send 반환 무시(RTU+samd20 동일, self-healing) ② app_eth_init boot-only/재init 없음(=slice 2b) ③ unit-echo 오버라이트 가독성(테스트로 가드됨). 조치 불필요.
- **다음 = Task 9 HW E2E**(보드 게이트): `comm_mode=ETH_STATIC`+static IP 설정→`mbpoll -m tcp -a 1 ...`로 slice-1 매트릭스 미러(링크/FC03 미러/FC06 클램프+FRAM/START-ceiling-STOP+ICON_RUN/점유 serial↔eth 전환/RTU FC06 회귀 spot-check). 통과 시 머지+태그 `hw-revA_fw-stage-c2a`.

### 2026-06-13 d — Stage C slice 2a (Modbus TCP/W5500 static): brainstorming → spec → plan

HW 비의존 세션. slice 1 머지 후 slice 2(Modbus TCP) 착수 — 설계 단계만(코드 변경 ✗). 브랜치 `feat/stage-c-modbus-tcp-static`(base = main `e351cad`).

- **brainstorming**(질문 4개): ① 범위 = **slice 2a = static IP만**(W5500 벤더스택+SPI1+TCP 서버), DHCP=slice 2b ② 프레이밍 = **표준 Modbus TCP**(MBAP 에코+CRC 제거) — samd20 비표준 raw 응답 quirk 폐기; 코어 무수정 ③ 드라이버 = **공식 WIZnet ioLibrary upstream** 벤더 ④ 활성화 = boot 1회 non-fatal init + `comm_mode==ETH`일 때만 TCP(RTU 점유 거울=택일).
- **컨텍스트 탐색 확정**: 코어 `MB_MODE_TCP` 스킵경로 준비됨(addr+CRC 생략, 응답엔 항상 CRC 부착) → TCP 글루에서 trailing CRC 2B 절단+MBAP 래핑+unit 에코. W5500 핀 pinmap §SPI1: PA4(소프트 CS)/PA5/6/7(AF5)/PC5(NRST)/PC4(INT). `cfg`에 `comm_mode`+`ether_ip/nm/gw[4]` 영속 확인. samd20 포팅 소스 = WIZnet ioLibrary + `process_tcp.c`/`ethernet.c`.
- **spec** `65c7d25` (`docs/superpowers/specs/2026-06-13-stage-c-modbus-slice2a-tcp-design.md`): 모듈(벤더 wiznet / spi1 / app_modbus_tcp_frame[순수] / app_eth / app_modbus_tcp / app_modbus 통합), 표준 프레이밍 데이터흐름, 점유/게이팅, 테스트, §8 미해소 6건.
- **advisor 검토**(설계 승인, 프레이밍 산술 검증 FC03 len=5/FC06 len=6) → plan에 5건 반영: ⓐ apply-path 추출은 HW 검증된 민감 코드(동작보존) ⓑ 벤더 ioLibrary=Task1(버전핀+API확인, 현 master 기본 `_WIZCHIP_=W6300`→W5500 설정 필수) ⓒ 순수 프레이밍 함수 명시 분리(host test) ⓓ 1-frame-per-recv 가정 문서화 ⓔ MBAP length 빅엔디안.
- **plan** `79526fb` (`docs/superpowers/plans/2026-06-13-stage-c-modbus-slice2a-tcp.md`, 9 task): T1 벤더 ioLibrary(경고격리 CMake lib, downstream 게이트) → T2 SPI1 드라이버 → T3 순수 MBAP 프레이밍(TDD host test) → T4 apply-path 노출(동작보존) → T5 W5500 non-fatal 브링업 → T6 소켓 FSM 전송층(port 502) → T7 슈퍼루프 통합(ETH 게이팅 + **mirror gap 수정**: 기존 tick이 ETH면 조기 return해 mirror_live 미호출) → T8 빌드/host 게이트+cpp-reviewer → T9 HW E2E(mbpoll -m tcp, 보드 게이트).
- **다음 = plan Task 1부터 구현**. 호스트 게이트 후 HW E2E → 머지+태그 `hw-revA_fw-stage-c2a`.

### 2026-06-13 c — Stage C slice 1: Modbus 코어+RTU 실보드 HW E2E (mbpoll RS-485) PASS

같은 보드 세션 연속. `feat/stage-c-modbus-core-rtu`(코드 완결, 최종 cpp-reviewer APPROVED) 빌드를 ST-LINK V3로 플래시(클린 빌드 0-warning) 후, Mac↔V30 RS-485(`/dev/cu.usbserial-AB0MLYXA`) + `mbpoll`로 plan §HW-gated 매트릭스 6항목 전수 검증. 코드 변경 없음(순수 HW 검증).

- **전제 확인**: 부팅 시 미점유(mon 소유) 상태 — `[boot]/[cfg] freq=1 type=1 work=0 energy=567 en_e=0 en_m=0`. 패널 comm `SERIAL+addr=1+19200+NONE` 저장 → `[lcd-hook] comm speed=3 parity=2 addr=1` → `[mb] acquire usart6 speed=3 parity=2 addr=1` 후 mon 침묵(점유 전환). 매핑 검증: `mb_baud[]`={2400,4800,9600,19200,38400,115200} → speed idx3=19200; parity 0=EVEN/1=ODD/2=NONE.
- **① 폴링(FC03)**: `mbpoll -t4 -r1 -c30` → 0x00~0x1D 전체 덤프. OUT_POWER(0x06)=55·ON_TIME(0x07)=56·ENERGY(0x08)=567·MODEL_FREQ/TYPE(0x17/18)=1/1·STATUS(0x1D)=0 — 부팅 `[cfg]`/`[lcd-hook] set_pot power=55` 값과 전건 일치(미러 충실).
- **② 쓰기+클램프+영속(FC06)**: OUT_POWER 80 쓰기→80, 클램프 30→**50**(min)·120→**100**(max, app_modbus.c:143), openocd `reset run` 후 재폴링 80 유지 = **FRAM 커밋 입증**(RAM-only면 옛값 복귀) + 재획득 확인.
- **③ 원격 런**: START(0x1B=1)→`STATUS bit0(US)=1`, **on-time ceiling 560ms**(`limit_on_time`=ON_TIME=56 ×10ms, app_reg.c:223-249 COMM 적용) 자동정지→STATUS=0, 재발화 없음, STOP(0x1C) 정상, ×3 재현. **ICON_RUN 점등/소등 시각 확인(사용자)**. DISP_POWER/AMP=0 = 벤치 전압주입 없음(B-SEAM deferred) → idle floor, by-design. (초기 8-연속폴 전부 STATUS=1은 측정 아티팩트 = mbpoll 오버헤드<ceiling이라 전부 창 안 — 정확 sleep로 1→0 전이 확인.)
- **④ 점유 전환**: addr=NONE 저장→`[mb] release (mode=0 addr=0)`+mon 복구+mbpoll 타임아웃; addr=1→재획득; comm_mode=ETH 저장→`[mb] release (mode=1 addr=1)` = **comm_mode-only 해제 경로 실증**(addr≠0이어도 SERIAL 아니면 해제, §Deviations 5).
- **⑤ 속도/패리티 매트릭스**: 9600/EVEN→`acquire speed=2 parity=0` mbpoll 8E1 폴링 OK; 115200/NONE→**reconfigure 경로**(`release`+`acquire speed=5 parity=2`, app_modbus.c:247 닫고 재오픈) mbpoll 8N1 OK. 전 재설정 거쳐 레지스터 값 보존(OUT_POWER=80 유지).
- **⑥ work_cnt 리셋**: FC06 0 쓰기(WORK_CNTL 0x01) 수락+읽기 0. **non-zero→0 리셋 시연은 벤치 불가** — work_cnt는 deferred weld-cycle 머신(energy/multi 런 브랜치)만 증가시켜 벤치 상시 0; 디코드/리셋 분기는 호스트 테스트(`test_app_modbus_core`)가 커버.
- 테스트 후 OUT_POWER 원래값 55로 복원. **다음 = modbus 브랜치 main 머지(merge-base 9083aa4, `diff 9083aa4..main`=∅ → 충돌 없는 깨끗한 머지) + 태그.**

### 2026-06-13 — Stage D slice 2b run-gate: 실보드 HW 재검증 PASS → main 머지 + 태그

보드 연결 세션. slice 2b(터치 RUN 게이트) HW 재검증을 ST-LINK V3 + 보존 트레이스 바이너리(`fw/build-trace/`, Jun 10 빌드 = slice2b 코드 최신 이후 — 재빌드 불필요 확인) 재플래시 + USART6 mon(`/dev/cu.usbserial-AB0MLYXA`) 캡처로 수행 → **6항목 + ICON_RUN 시각 전수 PASS**.

- ① 부팅 워밍업 1회: `st=1 rc 0→401`(~4s, 50/trace) `sel=0/run=0` 단방향 → `st=0`(재진입 없음).
- ② RUN press: `[lcd] rx vp=0x1080 data=0 → us_command=0 → [reg] cmd=0 run=2` + `set_pot power=55` + **`sel=0` 고정**(ch0=0 idle floor, **램프 상승 없음** = 2026-06-10 램프 폐기 의미론 확인).
- ③ ceiling 자동정지: `on-time ceiling (56 x10ms) -> stop`(이 보드 FRAM `limit_on_time=56`=560ms) → 이어진 held-button release `cmd=0 **run=0**` = **swallow_start 1회 소비, 재시동 없음**(V30 data=0 quirk 보정 동작 실증).
- ④ ceiling 전 release: `us_command=3 → cmd=3 run=0`. ⑤ swallow 후 재press 정상 `run=2`. ⑥ RESET `data=1→cmd=2 run=0`/SEEK `data=2→cmd=1 run=0` no-op + ICON_RUN 점등/소등 시각 확인(사용자).
- **main 머지**(`e9b593d`, `--no-ff` 2-parent) + 태그 `hw-revA_fw-stage-d2b`. 머지 후 main 빌드 0-warning(text 37088B ≈28.7%)·호스트 PASS. origin push ✗(local-authoritative 유지).

### 2026-06-13 b — M1 파라미터 주입 + us_on_time_200m: LV_TIME 바 HW 확인 PASS → main 머지

같은 보드 세션 연속. m1-주입 브랜치(`refactor/stage-d-m1-cfg-param-injection`)의 LV_TIME 바 동작을 일회용 트레이스로 검증 후 main 머지(`83e4e9c`, `--no-ff`).

- **검증**: 일회용 트레이스(REG_TRACE 출력에 `t200=us_on_time_200m` 추가 + 간격 100ms — 검증 후 폐기, 리뷰 코드 무변경 머지) → 런 중 `t200` 0→1→2 **200ms 케이던스** + ceiling(56×10ms=560ms) 정지 후 `run=0 t200=2` **latch** + 재press `t200=0` **리셋** 수치 확인. LV_TIME 바 fill/latch/reset 시각 확인(사용자). reg_on_time_200m 순수함수는 기존 호스트 테스트로 검증됨.
- **머지 범위 주의**: m1 브랜치 tip은 `5463370`(modbus plan)이었음 — modbus 설계문서(spec `ef359c5`/session-close `f56f17d`/plan `5463370`)가 m1 브랜치 위에 얹혀 있었기 때문(설계 단계를 m1 위에서 진행 후 modbus 코드 브랜치를 분기). 첫 브랜치-tip 머지가 modbus 문서까지 끌고 와서 **되돌리고(`reset --hard e9b593d`) m1 작업 마지막 커밋 `9083aa4`로 재머지**. modbus 설계문서는 modbus 브랜치 머지 때 코드와 함께 합류 예정.
- 머지 후 main 0-warning·호스트 PASS. **다음** = Modbus HW E2E(mbpoll RS-485) → modbus 머지. 머지 체인 slice2b✅→m1✅→modbus.

### 2026-06-12 c — Stage C slice 1: Modbus 코어 + RTU 구현 (코드 완결, HW E2E 대기)

plan(`docs/superpowers/plans/2026-06-12-stage-c-modbus-slice1-core-rtu.md`, 셀프리뷰 포함 `5463370`) → subagent-driven 구현(Task별 구현→spec리뷰→cpp-reviewer 2단 게이트, 리뷰 코멘트 전건 반영). 브랜치 **`feat/stage-c-modbus-core-rtu`**(base = stacked tip `refactor/stage-d-m1-cfg-param-injection`).

- **`ea7258f`/`8226091`/`ce0a6e8` 순수 코어 `app_modbus_core.{c,h}`** (+리뷰 fix `6688fd1`/`ed94472`/`32ef033`): samd20 modbus.c 충실 포팅 — CRC16(swap 컨벤션 문서화), FC 01/02/03/04/05/06, holdingReg/coils[50], RTU 주소+CRC 필터/TCP 스킵(slice 2 대비), 미지원 FC·범위 밖 = 무응답. HAL-free, 호스트 TDD 전수(`fw/test/test_app_modbus_core.c`, CRC 표준벡터+fence-post+TCP 분기). **포팅 수정 3건**(승인 편차): 요청 범위검사(원본 = OOB read + **무경계 임의 write**), FC05 에코(원본 0x02/9B 복붙버그), FC01/02 count%8==0 마지막 바이트 미충전.
- **`107d04f` mon 게이트 + `76ed82a` 전송층 `usart6_mb.{c,h}`** (+리뷰 fix `de1e077` double-open 가드): DMA2 S1 Ch5 circular free-running RX(no-ISR, usart1 선례) + **samd20 max_break_cnt 갭 프레이밍**(250µs표→ms 그리드: 14/7/4/2ms; 스펙의 IDLE-line 인터럽트 대신 — 의미론 동일, plan §Deviations 1) + blocking TX(길이 비례 timeout) + **TX 후 RX flush**(auto-DE 에코가 자기 FC06 응답을 요청으로 재해석하는 루프 차단, §Deviations 7). 패리티 = 9-bit word length 처리.
- **`a60f2d1` run FSM 소스 확장** (+주석 정정 `918d524`): `app_reg_command(cmd, src)`(US_TOUCH/US_COMM), 소스일치 정지(samd20 충실), `swallow_start`는 TOUCH 전용, on-time ceiling을 COMM 런에도(원본충실; REMOTE ceiling은 REMOTE 슬라이스), max_amp/last_amp 추적(DISP_AMP 미러용, `lcd_measure_t` 확장).
- **`349bd91` 통합층 `app_modbus.{c,h}`** (+주석 `1564131`): 매 tick 라이브 미러(samd20 대비 신선도↑·클램프 정규화, §Deviations 6), FC06 1-change else-if 체인(samd20 클램프 verbatim + EN_* LCD 에코 + work_cnt 리셋) + **`app_config_save_all` 전체맵 FRAM 커밋**(원본 DELAY3→ADDR_TRIGGER2/TRIGGER2→ADDR_DELAY2 복붙버그 구조적 해소), 명령 4종 consume-and-clear→US_COMM 라우팅(START 수락 시 set_pot 스텁), **USART6 점유 전환 = 매 tick cfg 평가**(samd20 메인루프 게이트 등가; comm hook은 로그 전용 유지 = app_lcd↔app_modbus 순환 차단, §Deviations 5). 슈퍼루프 step 4 + 부팅 `app_modbus_init()` 배선.
- 게이트: main 클린 재빌드 **0-warning**(FLASH 40780B 31.1%/RAM 12.3%), 트레이스 구성(REG_TRACE+LCD_TRACE_RX) 0-warning, 호스트 테스트 ×2 PASS. **HW E2E(mbpoll 매트릭스 6항목) = 보드 연결 후**(plan §HW-gated; 트레이스 검증은 Modbus 미점유 상태에서 — addr=NONE).

### 2026-06-12 b — Stage C slice 1 (Modbus 코어+RTU) 스펙 승인 — plan은 새 세션

Modbus 작업 개시(사용자 요청 "RTU/TCP 코드 선행"). brainstorming(슬라이스/점유 전환/기능 범위/접근 3택) → 스펙 작성·셀프리뷰·**사용자 승인**(`ef359c5`, `docs/superpowers/specs/2026-06-12-stage-c-modbus-slice1-core-rtu-design.md`). slice 1 = 순수 코어(`app_modbus_core`, 호스트테스트) + USART6 RTU(DMA+IDLE, `usart6_mb`) + 통합(`app_modbus`: 레지스터 미러/FRAM 커밋/US_COMM 명령/mon 점유 전환); TCP(W5500) = slice 2. 셀프리뷰 정정 3건 = DISP_POWER/AMP max/last 미러(amp 추적 추가), MODEL_* read-only, 드라이버/FSM 시그니처 확정. V30 회로 추적 = RS-485 방향 자동(U13 트랜시버 + U16, FW DE 제어 불필요). **다음 = 새 세션에서 writing-plans → 구현**(컨텍스트 50% 규칙 분할).

### 2026-06-12 — HW 비의존 후속 2건: M1 파라미터 주입 리팩터링 + us_on_time_200m 공급 (stacked 브랜치)

보드 부재 중 진행 가능한 deferred 항목 2건을 **stacked 브랜치 `refactor/stage-d-m1-cfg-param-injection`**(base = slice 2b tip `322a779`)에서 처리. slice 2b 브랜치와 `fw/build-trace/`(재플래시용 바이너리)는 무변경 보존 — HW 재검증 절차(HANDOFF §Resume)는 그대로 유효.

- **`ce81d89` M1 리팩터링(리뷰 deferred 해소)**: `app_reg_tick(void)`→`app_reg_tick(uint16_t limit_on_time)`. app_reg가 `app_lcd_cfg()`를 역호출하던 순환 고리 제거 — 슈퍼루프(`app.c`)가 매 iter 라이브 config에서 주입하므로 패널 편집 즉시반영 의미론 보존. 동작 보존 리팩터링.
- **`ec1b6d4` us_on_time_200m 공급(LCD 감사 갭 ② 해소)**: 순수함수 `reg_on_time_200m(elapsed_ms)`(200ms 단위, cap 200=40s; samd20 main.c:5223 등가) 신설 + 호스트 TDD. `app_reg`가 러닝 중 `run_start_ms` 기준 라이브 발행, idle 시 마지막 값 latch 유지(samd20 `last_time` 표시 등가 — disp 단일필드 노트 `app_lcd_disp.c:183` 정합). LV_TIME 바 런 중 0 고착 해소. 갭 ①(아이콘 엣지 = RESET→SEEK 체인 소속)·③(work_cnt = weld-cycle) = 여전히 deferred.
- **`cb6ce44` cpp-reviewer 반영**: 리뷰 = **APPROVED-WITH-COMMENTS**(CRITICAL/HIGH 0, M3건/L1건). M3 = START 엣지 `us_on_time_200m=0` 명시(samd20:4306 literal + ≤2ms stale-pairing 윈도우 제거), M2 = 잔존 `lim` 로컬 제거(REG_TRACE print의 `lim` 참조 = 잠재 트레이스-빌드 break였음 → `-DREG_TRACE` syntax-check로 검출·수정), M1 = publish ~2ms 게이트 전파 주석. L1(`ON_TIME_UNIT_MS` suffix) = pre-existing, 미반영.
- 게이트: main 빌드 0-warning(FLASH 28.72%/RAM 10.64%), 호스트 테스트 PASS, REG_TRACE 경로 syntax-check PASS. **HW 확인 항목(후속)**: LV_TIME 바가 런 중 200ms 단위로 차오르고 정지 후 마지막 값 유지 — slice 2b HW 재검증과 별개로 본 브랜치 머지 시점에 확인.

### 2026-06-10 — slice 2b 통합 cpp-reviewer 리뷰 APPROVED (b78cbbf + 27d45c3)

V30 RUN quirk fix `b78cbbf` + 금일 의미론 수정 `27d45c3` 통합 cpp-reviewer 리뷰 = **APPROVED**(CRITICAL/HIGH 0건). 코너 케이스 전수 검증: data=0 START/RELEASE 매핑(더블 프레스·release 유실·ceiling 후 orphaned release·`swallow_start` 누출/오흡수·SYS_PIC_NOW 상호작용), 워밍업 단발성(`main_state=1` 쓰기 = init 1곳, 오버플로 안전), ceiling 산술(×10ms 단위·0=off·uint32 래핑-안전), ADC 누산 오버플로 무, ISR 비접근(슈퍼루프 단일스레드 무race). 스펙 편차 4건 = 전부 2026-06-10 분석 문서가 승인(스펙 부분 대체 관계 확인).

- **비차단 4건 처분**: M2(same-burst stale `us_run_status` 의도 주석)·L1(`app_reg_tick`의 `app_lcd_cfg()` 라이브 읽기 헤더 주석) = 주석 추가 커밋 `c10b0aa`(코드 무변경, main 28.67%/trace 29.00% 0-warning, 호스트 PASS). M1(app_lcd↔app_reg 순환 의존 → 후속 슬라이스에서 파라미터 주입 리팩터링 권고)·L2(`reg_ramp_level` 레퍼런스 API 잔류 = 의도적 보존) = deferred.
- **남은 것** = HW 재검증(보드 재연결 시: 부팅 ~4s 워밍업 1회 → RUN press 즉시 `cmd=0 run=2`+`sel=scale(ch0_avg)`·ICON_RUN·램프 없음·500ms ceiling 자동정지·release swallow 1회) → finishing(머지) + 태그 `hw-revA_fw-stage-d2b`.

### 2026-06-10 — IPC 의미론 교차검증 → 램프 원본충실 회귀 + on-time ceiling + ADC 페이스 복원

samd20 소스 × M16 디스어셈블리 교차 검증으로 두-MCU IPC 런타임 의미론 확정(산출물 `docs/superpowers/analysis/2026-06-10-samd20-m16-ipc-semantics-verified.md`). 비판적 검토에서 발견 4건 → 사용자 결정(①은 원본충실 회귀, 나머지는 권고대로) 반영. `app_reg.c`/`app_reg.h`/`app_reg_calc.c`(주석만) 수정, 빌드 0-warning(main+trace), 호스트 PASS.

- **램프 의미론 정정(원본충실 회귀)**: M16 램프는 per-START 소프트스타트가 아니라 **부팅 1회 워밍업**(0x0195 nonzero 쓰기는 @0x1B8A 뿐, 재진입 불가; 램프 중 OSC 플래그 0 + 명령 디스패처 스킵 @0x041E; 진폭은 samd20 I2C_POT 소관). → boot 1회 ~4s 워밍업(명령 무시·sel=0) 후 **START 즉시 구동**. slice 2a per-START 램프 폐기, `reg_ramp_level()`은 출력 경로에서 제거(검증 레퍼런스로 보존, 호스트 테스트 유지). 기존 "M16 ramp is edge-driven on M_START" 주석은 디스어셈블리와 모순으로 폐기.
- **TOUCH run on-time ceiling(의도적 안전 추가, 원본 비충실 명기)**: 원본 `limit_on_time`은 REMOTE/COMM+SYS_HAND 전용(`main.c:5296`, 터치 런 무제한). V30 RUN 버튼 data=0 quirk(release 유실→무한구동) 대비로 US_TOUCH 런에 `limit_on_time×10ms`(기본 500ms, 패널 편집, 0=off) ceiling 적용. ceiling 정지 후 잔여 release가 START로 매핑되는 역전은 `swallow_start` 1회 소비로 보정(data=4 release는 RUN_RELEASE-while-IDLE에서 해소).
- **ADC 획득 페이스 복원**: 2ms/1채널 교대(ch0_avg 40ms·ch1_avg 400ms = 원본 8.3/42ms 대비 5~10×低) → **1ms 양채널**(ch0_avg 10ms·ch1_avg 50ms).
- **deferred 명문화**: RESET→SEEK 500ms 자동 체인(+SEEK 500ms 자동해제, `main.c:5388-5408`) = SEEK/RESET 구현 시 필수 의미론. **B-SEAM 가설 축소**: OSC 구동 = 명령 3선 active-LOW 레벨 미러 유력(룩업/램프는 미연결 7-seg 표시 전용이었을 공산) → 벤치 측정을 "3선 미러 확인"으로 축소. M16 PA7/PC1/PC4는 samd20 대응 없음(IPC 아님). M_OVLD 극성은 이중부정 혼동 위험 — 포팅 시 실측 필수.
- **HW 재검증 절차 변경**: RUN 검증 기대치가 "램프 0→401"에서 **"부팅 워밍업 1회(~4s) 후 RUN 즉시 sel=scale(ch0_avg)·release 정지·500ms ceiling"**으로 바뀜.
- **LCD 포팅 전수 감사(후속) = 합격**(분석 문서 §6b): samd20 parse_lcd_comm 34 VP·표시 TX·init/SYS_PIC_NOW 복구 전부 1:1 대응 + V30 적응 4건/의도적 편차 문서화 확인, 코드 무변경. **미문서 갭 3건 deferred 기록**: ① ICON_RESET/SEEK/ERROR_RESET 점등 엣지(RESET→SEEK 체인 구현 시 포함) ② `us_on_time_200m` 미공급(LV_TIME 바 런 중 0 — app_reg 저비용 후속 후보) ③ `LV_WORK_CNT` 증가(용접 사이클 deferred 소속). set_pot(I2C_POT)은 진폭 제어 실체로 격상 — 실출력 = B-SEAM 3선 미러 + I2C_POT(F2) 두 축.

### 2026-06-08 — Stage D slice 2b HW 검증 일부 PASS + V30 RUN 버튼 에셋 quirk fix (RUN 재검증 대기)

실보드 HW 검증 세션(ST-LINK 플래시 + SWD g_measure read + USART6 mon, REG_TRACE+LCD_TRACE_RX 빌드). **부팅 IDLE·RESET/SEEK 라우팅 PASS, RUN은 V30 에셋 quirk로 막혀 펌웨어 fix 적용, RUN 재검증은 보드 재연결 후(세션 중 USB 분리).**

- **✅ 부팅 IDLE**: `[reg] run=0 st=0 rc=0 sel=0 band=21`, auto-ramp 없음(SWD `us_run_status=0` 불변) = slice 2a auto-run 폐기 확인.
- **✅ 명령 라우팅**: RESET → `[lcd-hook] us_command=2` `[reg] cmd=2 run=0`; SEEK → `us_command=1` `cmd=1 run=0`. hook→`app_reg_command` HW 확인, RESET/SEEK no-op(spec §9) 정상.
- **원인규명(systematic-debugging)**: RUN press 무반응. `[lcd-hook]` 미발화 트레이스 → upstream 격리 → `LCD_TRACE_RX` 빌드로 수신 프레임 관측 = **V30 RUN 버튼이 `KEY_MULTI(0x1080)` 키값 0을 press·release 양 edge 반환**(RESET=1/SEEK=2는 정상, data=0은 RUN 고유). `handle_key_multi`(samd20 포팅)는 data=3/4만 처리 → RUN 명령 미발화. **LCD 스테이지 버그A(SETUP_MODEL data=0)와 동일 = V30 DGUS 에셋 root, slice 2b 코드 무관.** 에셋 점검: `hw/lcd/dgus/13TouchFile.bin`은 컴파일 출력, 편집 DWIN 프로젝트 repo 부재.
- **fix `b78cbbf`** (사용자 결정 = Option B 펌웨어 적응, `app_lcd_input.c` 한 파일, FSM/enum/app_reg 무변경, spec §4.4): `handle_key_multi` `data16==0` 분기 추가 → `app_lcd_measure()->us_run_status`로 START(IDLE)/RUN_RELEASE(running) 매핑. down/up data=0 쌍이 hold-to-run 재구성, self-syncing(edge 누락→다음 press가 정지로 복구). set_pot은 START에서만. 빌드 0-warning(FLASH 28.96% trace), 호스트 PASS.
- **다음**: 보드 재연결 → 재플래시(`fw/build-trace/` 준비됨) → RUN press→`cmd=0 run=2`+램프+ICON_RUN→release→정지+latch→re-arm 재검증 + fix `b78cbbf` cpp-reviewer → finishing + 태그 `hw-revA_fw-stage-d2b`. (A 에셋 정상화 = DWIN 툴 가용 시 후속.)

### 2026-06-08 — Stage D slice 2b (RUN 명령 게이트) 코드 완료 — HW 검증 대기

slice 2a 머지 후 slice 2b 착수. brainstorming→spec→plan(`writing-plans`)→subagent-driven 구현(Task별 fresh subagent + 2-stage 리뷰). 브랜치 **`feat/stage-d-slice2b-run-gate`**(main 미머지). **터치 RUN start/stop 게이트만**(SEEK/RESET regulation 효과·overload·weld-cycle·Modbus·OSC 구동·blink = DEFERRED). 빌드 0-warning(FLASH 28.64%/RAM 10.60%), 호스트 테스트 `all checks PASSED`(순수함수 무회귀; 신규 순수함수 없음), 전체 cpp-reviewer **APPROVED**(HW 검증 대기). **남은 것 = Task 3 실보드 HW 검증 → 최종 머지/태그.**

- **사실 출처(소스 직독)**: 터치 RUN = **momentary hold-to-run, model_type 무관**(samd20 `main.c:3676` press→`us_run_status=US_TOUCH`/`sig_run_status=ON`, `3699` release→IDLE/OFF). Modbus=latched(`H_REG_START/STOP`=US_COMM, Stage C). `us_off`(`4180`)가 **`last_=max_` latch**.
- **`decfc28` run FSM + taxonomy + 라우팅**: `app_reg_command(us_cmd_t)` 신설(`app_lcd_hook_us_command`에서 라우팅). **부팅 IDLE**(slice 2a auto-run 폐기). START는 **`== US_IDLE` 엣지만** re-arm(advisor catch: `!= US_REMOTE`면 패널 auto-repeat가 램프 재시작→401 미도달; M16은 `M_START` 엣지구동). RUN_RELEASE는 `== US_TOUCH`만 정지(last_power=max_power latch). enum `{US_IDLE,US_RUNNING}`→`{US_IDLE=0,US_REMOTE=1,US_TOUCH=2,US_COMM=3}`(US_RUNNING 제거). sel MUX에 IDLE→0; ramp 증가 게이트에 활성 조건. 활성 lookup 경로 = slice-1 verbatim.
- **`ed2093f` ICON_RUN 엣지 렌더**: `app_lcd_disp_step`이 `us_run_status` running-ness 엣지(`prev_run_on`)에 ICON_RUN 1회 write(4ms 스팸 방지). app_reg DGUS-free 유지(계층 분리).
- **`4264bab` SYS_PIC_NOW mid-run 리셋 → 정지**(cpp-reviewer catch, spec §4.3): 패널 자체 리셋(`SYS_PIC_NOW`=0, page-0 splash, 진짜 리셋만)이 `init_mode`로 ICON_RUN을 클리어하나 held-RUN release 미도착 → `us_run_status` US_TOUCH 고착(무한구동)+아이콘 거짓표시. 재init 시 `US_CMD_RUN_RELEASE` 발행(UI 상실→액추에이터 정지, Option A) → FSM IDLE이 무한구동+아이콘 동시 해소. disp 코드 무변경.
- **리뷰**: Task별 spec+cpp-reviewer APPROVED + 전체 통합 cpp-reviewer APPROVED(엔드투엔드 superloop 순서 무race·slice-1 무회귀·enum 마이그레이션·max/last 라이프사이클·부팅 clean·SYS_PIC_NOW 순서). 비차단 노트 2건: ① IDLE에서 `curr_amp` 무조건 발행→출력 바 = idle ADC 노이즈 플로어 HW 확인(plan Task 3 item 6, 범위 밖=B-SEAM) ② FSM 전환 호스트 미테스트(HW 게이트로 의도, spec §8.1).
- **다음**: Task 3 HW(보드 연결 시) — 부팅 IDLE / RUN hold→`rc` 0→401→`st=0` + ICON_RUN 점등 + VAR_POWER 추종 / release→소등+latch / re-arm / 무회귀. 절차 = `HANDOFF.md` §Resume / plan Task 3. SEEK/RESET·overload·OSC·6b = 여전히 DEFERRED.

### 2026-06-08 — Stage D slice 2a HW 검증(Task 4) PASS + 바/아이콘 합격기준 정정

브랜치 **`feat/stage-d-slice2-softstart`** 실보드 HW 검증(STLINK V3 + USART6 mon, REG_TRACE 빌드, 전압 주입 불필요). **compute(상태머신 + soft-start 램프) = PASS.** 펌웨어 코드 변경 없음(기검증 3커밋 그대로).

- **부팅 램프 trace (reset 2회 재현)**: `st=1` `rc` 0→401, `sel` 128→256→384→512→640→896→**1024**(rc=300 포화), `band` 18→16→13→11→8→3→**0** — ~4s(rc 401 × 10ms cadence) 단조. `rc≥401`에서 **`st=0` 핸드오프** → 이후 idle `sel=18/band=20`·바닥 `sel=0/band=21` = **slice-1 verbatim 무회귀**. 배너 `[boot] stage-b ready`/`[lcd] ready=1`/`[cfg]` 정상.
- **램프 패널 도달 입증**: LCD **power 숫자(VAR_POWER)** 가 램프 추종 상승(전압주입 없이 setpoint가 디스플레이까지 전파됨 확인).
- **합격기준 2건 정정 (spec 디스플레이 계층 오독 → 펌웨어 수정 불필요)**:
  - **출력 바 정지 = by-design**: 출력 바(`LV_OUTPUT`)는 `disp_compute_output(curr_amp,…)`로 구동되는 **amplitude 바**(samd20 충실 포팅)이지 `curr_power`가 아님. 램프 미러는 power **숫자**(`disp_send_val`: `VAR_POWER ← max_power ← sel`). 전압주입 없으면 `curr_amp` idle(≈3 ≤ 10) → 바 비움이 정상. (바를 `sel`로 구동 = 충실 위젯 거짓 구동 → 기각.)
  - **running 아이콘 미점등 = 2b 몫**: `ICON_RUN`(0x1152)은 samd20 run **명령 FSM**(`ref/samd20/main.c:4302` = `sig_run_status` 엣지 + `M_START` + accumulator 리셋) 소속 → spec §9 명령 FSM = **slice 2b**. 2a는 `app_lcd.c:125` init 클리어만. `us_run_status != US_IDLE` 게이트(`app_lcd_disp.c:156`)는 숫자 소스 선택일 뿐 아이콘 미구동. us_run_status→ICON_RUN 직접 브리지는 충실 포팅 이탈이라 미적용.
- **정정 반영**: spec §8.2 + plan Task 4에 검증결과 노트 추가. compute 검증의 권위 소스 = 시리얼 trace + power 숫자.
- **통합 완료**: 최종 cpp-reviewer **APPROVED**(401 핸드오프 off-by-one 없음·state==0 slice-1 verbatim 무회귀·시간델타 wraparound·counter 경계 안전) → **main `--no-ff` 머지(`43fda87`, 2-parent) + tag `hw-revA_fw-stage-d2`**. 머지 후 빌드 0-warning(FLASH 28.52%/RAM 10.57%) + 호스트 테스트 `all checks PASSED`, 피처 브랜치 삭제. origin push ✗(로컬 main 144 ahead = local-authoritative).
- **다음**: slice 2b — 명령 FSM(START re-arm·ICON_RUN 점등·SEEK/RESET) + overload + blink; OSC 물리 출력(B-SEAM) + 6b calibration = HW-gated DEFERRED.

### 2026-06-03 — Stage D slice 2a (상태머신 + soft-start 램프) 코드 완료 — HW 검증 대기

slice 1 머지 후 slice 2a 착수. brainstorming→spec→plan(`writing-plans`)→subagent-driven 구현(Task별 fresh subagent + 2-stage 리뷰). 브랜치 **`feat/stage-d-slice2-softstart`**(main 미머지, tip `ae24ec4`). **compute만**(출력 OSC·명령 FSM·overload·blink = DEFERRED, slice 1 measure-first 유지). 빌드 0-warning(FLASH 28.39→28.52%/RAM 10.55→10.57%), 호스트 테스트 `all checks PASSED`, 3 Task 모두 spec+cpp-reviewer APPROVED. **남은 것 = Task 4 실보드 HW 검증(REG_TRACE+LCD bar) → 최종리뷰 + 머지/태그.**

- **`d98b025` 순수 `reg_ramp_level`**(호스트 TDD): 10-rung thermometer fill(M16 `app_0x1226` recon :249-258 = `g_019F` 0x01→0xFF), scaled 도메인 `{128,256,…,1024 포화 rung7+}`. 정확한 패턴바이트→OSC는 deferred(B-SEAM, C-LADDER와 동일). 경계/단조 호스트 테스트.
- **`ba67971`**: `enum {US_IDLE=0,US_RUNNING=1}`를 `app_lcd.h` contract로 끌어올림(기존 `app_lcd_disp.c` 로컬 `#define US_IDLE` 제거). disp 게이트 `!= US_IDLE` 무변경.
- **`ae24ec4` app_reg.c 상태머신**: `main_state` 부팅 init=1(M16 `@0x1B8A`), 10ms 램프 cadence(별도 `prev_ramp_ms` 게이트, Timer1 0xFFB1 등가) `ramp_counter` state==1에서만 증가, ≥401(`@0x137C`)→state0 단방향. sel MUX: `state==1?reg_ramp_level(ctr):reg_scale(ch0_avg)`(state==0=slice-1 verbatim 무회귀). `sel`→band(deferred)+`curr_power`(LCD bar, state==1 동안 램프 미러 = 라벨된 테스트 deviation)+`us_run_status=US_RUNNING`. REG_TRACE에 `st/rc/sel` 추가.
- **리뷰**: cpp-reviewer가 시간델타 wraparound 안전·401 핸드오프 off-by-one clean·state==0 무회귀·정수 안전 확인. 비차단 minor 코멘트 제안은 "요청한 부분만 수정" 워크스페이스 규칙에 따라 미반영.
- **다음**: Task 4 HW(보드 연결 시) — 부팅 `st=1` sel 128→1024/band 18→0 ~4s + LCD bar 상승 + st→0 핸드오프 + 무회귀. 절차 = `HANDOFF.md`(루트) §Resume / plan Task 4.

### 2026-06-02 — Stage D slice 1 HW 기능검증(6a) PASS + Task 6 분리(6b calibration HW-gated)

실보드 HW 검증 진행. 전압 가변 소스 부재로 Task 6을 **6a(기능/구조, 지금 가능)** + **6b(신호 calibration, HW 준비 후)** 로 분리(사용자 결정 = measure-first). 6a 전체 PASS → slice 1(compute, 출력 deferred)은 설계대로 통합 준비 완료. compute 수식은 호스트 단위테스트가 전 범위 커버.

- **트레이스 빌드 명령 정정** (`879f43e`): plan Task 6 / RESUME의 `-DCMAKE_C_FLAGS="-DREG_TRACE"`가 캐시를 덮어써 툴체인 `CMAKE_C_FLAGS_INIT`의 CPU 플래그(-mcpu/-mthumb/-mfpu/-mfloat-abi)를 날려 ARM-mode 빌드 실패 → CPU 플래그를 함께 전달하도록 정정. 트레이스 ELF 0-warning, FLASH 28.45%/RAM 10.57%.
- **6a 기능/구조 검증 PASS** (REG_TRACE 빌드, 사용자+보드):
  - 6a-1 compute liveness: `[reg] ch0/ch1/scaled/band` ~500ms cadence. floating(신호 미연결) 시 ch0=3~5→scaled=18/24/30(**×6 라이브 확인**), 실 SENS_OUT 연결 후 ch0=0→**scaled=0(floor SCALE-05)→band=21(no-match off)** = scale 2분기+lookup off 경로 라이브 확인.
  - 6a-2 무회귀: `[boot]/[lcd] ready/[cfg]` 배너 정상 + LCD 네비게이션 락업 없음.
  - 6a-3 OSC 안전: **PB2/PB10/PB14 idle-HIGH 스코프 확인**(active-LOW=off, C6), OSC 무구동, PB12/PB13 미구동.
  - 6a-4 LCD provider live: idle scaled=0 → **VAR_POWER=0 표시 정상**(provider 라이브, garbage/stuck 아님).
- **6b 신호 calibration (DEFERRED, HW 준비 후)**: ① `>>2` 12→10bit 정규화 + 2.56V↔3.3V 레퍼런스 도메인 보정(DP2 first-cut→실측) ② ch0_avg/adc_scaled_value 물리단위 바인딩(B-UNITS/B3) ③ ADC offset/gain ④ OSC 출력 경로·비트매핑·극성(B-OSC-MAP/B-SEAM) + PB12/PB13 방향 확정. 전압 가변 + 실 초음파 구동 필요.
- **설명 문서 신규** (`docs/superpowers/analysis/2026-06-02-m16-to-stm32-port-explained.md`): M16→STM32 포팅을 compute(알고리즘) + I/O(핀) 두 축으로 정리한 핸드오프/teaching 문서(권위 소스 아님, 참조 명시).

### 2026-06-01 — Stage D slice 1 구현 완료 (레귤레이션 코어 compute, 출력단 DEFERRED) — HW 검증 대기

브랜치 **`feat/stage-d-regulation-core`** (main 미머지). spec(`…/specs/2026-05-31-stage-d-slice1-regulation-core-design.md`) → plan(`…/plans/2026-05-31-stage-d-slice1-regulation-core.md`) → inline 구현. **compute 파이프라인만**(OSC 물리 구동 = B-SEAM, 벤치 측정까지 DEFERRED). 빌드 0-warning, 호스트 단위테스트 PASS, **cpp-reviewer APPROVED**(차단 0). **남은 것 = 실보드 HW 검증(REG_TRACE) → 머지/태그.**

- **신규 `app_reg_calc.{c,h}` (HAL-free, 호스트 테스트)**: `reg_scale`(SCALE-04/05/06: `in≥1000→1000` / `in<3→0` / else `in×6`, 의도적 불연속) + `reg_output_level`(C2: 21엔트리 strictly-less lookup, no-match→21) + `reg_lookup_table[24]`(§4.3 byte-exact). 순수함수 분리 = spec §12 호스트 TDD를 실제 실행 가능하게(레포에 테스트 하네스 부재 → `fw/test/Makefile` + assert 하네스 신설). TDD RED→GREEN.
- **신규 `adc1.{c,h}` 드라이버**: ADC1 init(12-bit, SW 트리거, 폴링) + `adc1_read(ch)` raw 12-bit; semantic 채널 enum→ADC_CHANNEL_8/9(PB0/PB1). cpp-reviewer L1/L2: `HAL_ADC_Start`/`PollForConversion` 실패 시 `Error_Handler`(타임아웃 스테일 샘플 누산 방지, 드라이버 기존 체크와 일관).
- **신규 `app_reg.{c,h}` 상태 모듈**: `app_reg_tick()` 2ms `sys_tick` 델타 게이트 → ch0/ch1 교대 1변환/틱, `>>2` 정규화, mean-of-10/50 누산·커밋 → 최신 `ch0_avg`에 scale+lookup → `lcd_measure_t` 발행(curr_amp=ch0_avg, curr/max/last_power=adc_scaled_value; freq/energy/status=0). OSC GPIO 미구동.
- **통합**: `main.c` `app_reg_init()`(app_init 뒤) / `app.c` `app_reg_tick()`(슈퍼루프 step 3) / `app_lcd.c` `app_lcd_measure()`→`app_reg_measure()`(스텁 라이브화).
- **board.c 안전수정**: `CTRL_OSC_OUT_PINS = PB2|PB10|PB14`(옛 마스크 PB10|PB12|PB13|PB14|PB15 정정 — PB2 누락·PB15 spurious) + 확정 3채널 idle-HIGH(active-LOW=off, C6); PB12/PB13(OSC2/3←PC4/PA7, 이미지상 입력 C7)은 미설정(B-OSC-MAP 측정 대기).
- **빌드**: `CMakeLists` + `stm32f4xx_hal_adc.c`, vendor include `SYSTEM`(LL ADC 헤더 `-Wunused-parameter`가 앱 0-warning 게이트 깨던 것 차단); `hal_conf.h` `HAL_ADC_MODULE_ENABLED`(앱레벨 config, I2C/TIM 선례 동일). **FLASH 26.86→28.39% / RAM 10.16→10.55%**.
- **의도적 spec 정제 3건**: ①순수함수 `app_reg_calc`로 분리(호스트 테스트). ②`adc1`은 얇은 페리페럴 계층(정규화/누산은 app_reg). ③`reg_output_level`은 band index만 반환 — band→thermometer 패턴바이트 맵(C-LADDER)은 출력단(B-SEAM)과 함께 DEFERRED(slice 1에서 미표시·미구동 = YAGNI, pure-add로 후속 가능).
- **다음**: Task 6 HW 검증(`-DREG_TRACE` 빌드 → PB0 주입 → ch0/scaled/band 추종 + LCD bar 확인 + PB2/10/14 idle-HIGH + PB15 NC 확인) → 머지/PR + 태그.

### 2026-05-31 (후속) — Stage D slice 1 분석 완료 + spec 초안 (레귤레이션 코어, 코드 변경 ✗)

ATmega16 레귤레이션 코어를 STM32F410으로 흡수하는 Stage D slice 1 착수. disasm-adjudicated 다중소스 분석(55-agent workflow, 3.7M tokens) → spec 초안. **코드 변경 없음**(분석+설계 단계, spec 리뷰 대기).

- **검증된 분석** (`docs/superpowers/analysis/2026-05-31-m16-regulation-core-verified.md`): raw disasm(`M16_reverse/out/01_disassembly.asm`)을 ground-truth로 v001 디컴파일·재구성본을 교차검증(두 C파일은 hypothesis, "N소스 합의" 추론 금지). 29 register-fact(bucket A) + 7 open decision(bucket B). **v001 5건 정정**: ①스케일링 `>>6`→**×6 곱셈**(`1ac2 ldi r30,0x06`+`call 0x2158`, 시프트 명령 0개; 자기일관: `>>6`은 1000 clip 도달 불가) ②free-run→**단발 SW re-arm**(ADATE=0) ③Timer1 0xB1FF→**0xFFB1** ④램프 6단→**10단** ⑤compare `>`→**`<`**(strictly-less). 검증 파이프라인(SRAM→SRAM, ~2.05ms Timer0 틱): 2ch 단발 ADC(ch0÷10@0x01B9/ch1÷50@0x018D) → clip(≥1000)/floor(<3)/×6 → 21엔트리 내림차순 lookup thermometer → 패턴. advisor 최종검증: bucket-B 미승격 확인(C9 OSC identity는 register_fact 태그였으나 bucket B로 강등). 메모리 `project_m16_regulation_verified`.
- **핵심 미확정 B-SEAM**: 레귤레이션 패턴이 OSC 5핀에 닿는 물리 경로는 이미지에 없음(디컴파일은 board-dead PORTD로 라우팅; Timer0 ISR은 3 바이너리 플래그만 active-LOW로 PC7/PB1/PB0=STM32 PB14/PB2/PB10 미러). PC4/PA7(OSC2/3=STM32 PB12/PB13)는 이미지상 입력. **사용자 결정 = measure-first**(벤치 스윕으로 실제 구동경로·방향 확정 후 출력단 구현).
- **spec 초안** (`docs/superpowers/specs/2026-05-31-stage-d-slice1-regulation-core-design.md`, **리뷰 대기**): compute 파이프라인(신규 `app_reg.c`+`adc1.c`) + `app_lcd_measure()` 라이브화 + board.c 안전수정(마스크 정정+확정 3채널 idle-HIGH, PB12/PB13 제외)만; OSC GPIO 구동은 DEFERRED(§9). 결정 DP1=슈퍼루프 폴링 ADC / DP2=raw 카운트+측정보정(AVR 10-bit@2.56V vs F410 12-bit@3.3V 도메인 해저드, `>>2` 정규화 1차안) / DP3=spec→리뷰→plan→구현. 순수함수 `reg_scale`/`reg_output_level`은 TDD 단위테스트 대상.
- **다음**: 사용자 spec 리뷰 → `writing-plans` → 구현(compute 먼저, 출력단은 측정 결과 수령 후).

### 2026-05-31 — STD comm 표시결함 해결(DGUS 에셋 root) + LCD 브랜치 HW 검증 완료 (통합/태그 대기)

comm_mode 표시결함의 **root는 firmware 아니라 DGUS 패널 에셋**(page 27/23 위젯이 page-show 시 `DISP_COMM_MODE`(0x140c) auto-load 안 함; page 25는 함)임을 **교차테스트**(STD comm 진입을 page 25로 repoint→정상)로 확정 → 사용자가 에셋만 수정해 정상화 = 진단 입증. firmware는 coherent 수정 A/B/C/D 적용. 상세: `docs/superpowers/analysis/2026-05-31-std-comm-page27-display-port-faithful.md`, 핸드오프 `docs/superpowers/2026-05-31-HANDOFF-lcd-comm-display-resolved.md`.

- **Fix A/B/C/D (커밋 `a0a631c`)**: A=부팅 `temp_comm_mode=0xFF` sentinel(`app_lcd.c init_mode`, 헤더 의도 정합) / B=`commit_comm_mode_and_ether` 0xFF early-return 손상가드(`app_lcd_input.c`, comm 미방문 STD-page SAVE 시 comm_mode=255+0.0.0.0 손상 차단) / C=`dgus_set_page` **후** comm 아이콘 재기록(`app_lcd_render.c`, page-27 auto-load 부재 방어 — 에셋 root fix로 redundant지만 **사용자 결정으로 defense-in-depth 유지**) / D=serial 분기 `DISP_EN_DHCP=0`(stale dhcp echo 제거). 머지빌드 0-warning, **FLASH 26.86%/RAM 10.16%**.
- **에셋 fix (커밋 `0eafe68`)**: `hw/lcd/dgus/14ShowFile.bin` page 27/23 comm-mode 아이콘 표시설정 (root fix, 레포 반영).
- **§4 전체 HW 재검증 PASS** (트레이스 빌드 + 머지 PR 바이너리 양쪽 플래시): ① STD comm 진입 표시 정상(`page=27 tcm=1`, ethernet O) ② **토글 0x140B serial→ethernet→dhcp 아이콘 정상 갱신·사라짐 없음**(이번 세션 핵심 실패분 해소; `data=0/1/2 → page=23/27/27 tcm=0/1/2`, serial=STDC23/eth·dhcp=STDE27 2-레이아웃은 패널 의도, 지난 세션 `page=9` 이탈 재현 안 됨) ③ DHCP 저장→전원사이클→`boot cm=2 ip=192.168.1.128` 영속+IP 정상 ④ Fix A 부팅 `tcm=255` sentinel ⑤ Fix B IP 0.0.0.0 손상 없음.
- **최종 리뷰**: cpp-reviewer **APPROVED** (A/B/C/D + `#ifdef LCD_TRACE_RX` 게이팅, 차단 이슈 0; B 오스킵 없음·C idempotent/무플리커/언더플로우가드 확인).
- **진단 스캐폴딩**: `LCD_TRACE_RX` 게이트 트레이스 **유지**(머지 컴파일아웃 검증), `fw/build-trace/` gitignore.
- **다음**: main 통합(머지/PR) + 태그 `hw-revA_fw-stage-lcd` → Stage D slice 1.

### 2026-05-27 (후속 3, WIP 체크포인트) — STD 이더넷/comm_mode 영속 + comm_mode 표시 버그

T10 항목 ⑤ 중 사용자 요청 "STD도 MULTI처럼 ether/comm_mode 영속" 작업. 상세: `docs/superpowers/analysis/2026-05-27-std-ether-comm-mode-persist-display.md`. **세션 종료 체크포인트 — 표시 버그 미수정, 다음 세션 이어감.**

- **작업1 (완료·HW검증, 미커밋→이 커밋에 포함)**: `data_save_commit()` STD 분기에 `commit_comm_mode_and_ether()` 추가 → STD에서도 comm_mode/ether 영속. 검증: `commit cm temp=1 cfg=0`→커밋, 전원사이클 후 `boot cm=1`, DHCP는 `boot cm=2`로 FRAM 영속 확인. = 사용자가 본 "ethernet enable 저장 안됨"은 영속 실패가 아니라 표시 버그.
- **작업2 (root cause 확정, 미수정)**: DHCP 저장+전원사이클 후 STDE에 **serial+dhcp 동시 체크**. 원인 = `temp_comm_mode` 섀도우 생명주기 결함: (1) 부팅 zero-init→`temp_comm_mode=0`(0xFF sentinel 아님), (2) seed가 `==0xff` 게이트라 부팅 후 첫 진입 스킵, (3) serial 분기가 `DISP_EN_DHCP` 미클리어(stale), (4) **추가 위험**: `commit_comm_mode_and_ether`가 0xFF 미가드 → comm 페이지 미방문 저장 시 comm_mode=255 손상(latent, MULTI에도 잠재, 작업1이 STD로 확장).
- **다음 세션 = coherent 수정 A~D** (spec/plan + samd20 comm-섀도우 대조 후): A 부팅 `temp_comm_mode=0xFF` 초기화 / B commit 0xFF 가드(손상방지) / C seed 자동정상화 / D serial 분기 `DISP_EN_DHCP=0`.
- **진단 스캐폴딩(LCD_TRACE_RX 게이트, 머지 컴파일아웃)**: `app.c` `boot cm=.. ip=..`, `app_lcd_input.c` commit `commit cm temp=.. cfg=..`. 유지/제거 결정 보류.

### 2026-05-27 (후속 2) — SETUP_MODEL 롱프레스 진입 불가 발견·수정 (코드 변경 O, **HW 검증 PASS**)

T10 항목 ⑥ 검증 중 **SETUP에서 모델 셋업 페이지 진입 키가 안 먹힘** 발견. systematic-debugging Phase 1~4. 상세 finding: `docs/superpowers/analysis/2026-05-27-lcd-setup-model-longpress.md`.

- **Root cause**: DGUS 모델 키(VP 0x1084)가 **터치-다운/업 양쪽 모두 value 0**을 올리고 **release=2를 영영 안 보냄**(LCD_TRACE_RX 진단 빌드 3회 캡처: 4초 홀드=2이벤트 둘 다 0, auto-repeat 없음). 포팅된 `long_press_released()`는 samd20 verbatim으로 `data==2`를 릴리스로 가정 → 업(0)을 "또 다른 press"로 오인 → **롱프레스 절대 미완성**. 포팅 회귀 아님(이 패널 설정이면 samd20도 동일 실패 구조) = 패널 DGUS 터치 설정 ↔ 펌웨어 프로토콜 가정 불일치.
- **수정 (P2, 펌웨어측 — 사용자 결정)**: `long_press_released(vp, data16)` 재설계 — 같은 VP의 **연속 두 data=0을 press→release로 페어링**, 간격 ≥ KEY_HOLD_MS(2000ms)면 발화(2초 가드 유지). `data==2` 하위호환 유지. `key_press_vp`로 키잉(0x1084/0x1094 무간섭). `app_lcd.h`에 `key_press_vp` 필드 추가. 빌드 0-warning, FLASH 26.80%/RAM 10.16%.
- **HW 검증 PASS**: ① 모델 키 2초 홀드 → 모델 셋업 진입 ✓, ② 짧은 탭 → 미진입(가드 유지) ✓.
- **보류 (P1, 패널측)**: DGUS `.bin`을 0x1084 업에서 value 2 올리도록 재설정(펌웨어 무수정·verbatim 유지) — DGUS 에디터+패널 재플래시 필요라 미채택.
- **flag(차단 아님)**: 토글 desync 위험(0x1084 다운 드롭 시 spurious 진입; DMA RX로 완화). 0x1094 동일거동 여부 미확인(같은 로직 공유).

### 2026-05-27 (후속) — HW 검증 중 버그 2건 발견·수정 (코드 변경 O, **HW 검증 PASS**)

T10 HW bench 검증 착수 → energy 슬라이더 드래그가 DGUS RX를 영구 wedge시킴을 발견. 추적 결과 **연결된 버그 2건**을 수정. 둘 다 LCD 포팅 로직이 아닌 하부 결함이며, RX wedge가 다른 하나를 가리고 있었음. 상세 finding: `docs/superpowers/analysis/2026-05-27-lcd-hw-verify-rx-wedge.md`. spec/plan: `docs/superpowers/{specs,plans}/2026-05-27-usart1-dma-rx-hardening*`.

- **버그 1 — USART1 DGUS RX 영구 wedge (Phase2/StageA 드라이버 결함, LCD 브랜치가 노출)**: 1바이트 `HAL_UART_Receive_IT`가 오버런/재무장 실패 시 RXNEIE=0으로 영구 정지(SR ORE=1, RXNE 갇힘). 트리거 = per-rx 블로킹 `mon_printf`(무한 타임아웃)가 VP 플러드 중 루프 포화. **수정**: USART1 RX를 **DMA2 Stream2 Ch4 circular free-running**으로 전환(오버런 면역, `usart1_rx_pop` NDTR 기반, 시그니처 불변 → 파서/app 무수정) + per-rx 트레이스 `#ifdef LCD_TRACE_RX` 게이트(default off) + mon TX 타임아웃 50ms. 커밋 a2c37b4→82cdb4c(+리뷰 20b3380). cpp-reviewer APPROVE.
- **버그 2 — LCD 입력 값 추출 off-by-one (LCD 브랜치 T6, RX fix 후 노출)**: DGUS 0x83 응답의 VP주소 뒤 READ_LEN(워드수) 바이트를 값 MSB로 오독(`data16=(data[0]<<8)|data[1]`). SAVE(값1)→256으로 읽혀 CANCEL 분기 실행(영속 안 됨), 전 numeric 입력이 garbage. **수정**: `data16=(data[1]<<8)|data[2]`(READ_LEN 건너뜀, samd20 dgus_lcd.h DATA_H=5/L=6 일치). 1줄. 커밋 d6c681f.
- **HW 검증 PASS**: ① `run_page_confirmed=1`(무회귀). ② SETUP energy 편집→SAVE→**전원사이클→영속**(`[cfg] energy=567`). 공격적 슬라이더 드래그에도 **RX 무wedge**(CR3 DMAR=1, SR ORE=0, NDTR/tail 전진). `[lcd-hook] set_pot power=55 dac=12`(output_power 정확 추출). ⑩ 훅 배선 OK. ⑧ N/A(전원 공유). ⑨ 재해석(패널이 idle에 LEN=3 `5A A5 03 82 4F 4B` 프레임 연속 스트리밍 → 파서가 설계상 거부 → dgus_rx_drop_count 항상 비0이나 무wedge가 진짜 신호).
- **잔여 T10(미검증)**: ③ CANCEL 복귀, ④ 전 페이지 네비 무락업, ⑤ STD ether 미영속 퀴크, ⑥ F3 HAND→MULTI, ⑦ NM/GW IP옥텟 시드 퀴크. 이들 통과 후 main PR + 태그 `hw-revA_fw-stage-lcd`.
- **향후 조사 후보(코딩 차단 아님)**: 패널이 스트리밍하는 VP 0x4F4B `5A A5 03 82 4F 4B`(LEN=3, WR)의 정체/이유.

### 2026-05-27 — LCD 전체 동작 포팅 (samd20→STM32) 구현 완료 (코드 변경 O, **HW 검증 대기**)

- **브랜치**: `feat/stage-lcd-full-behavior` (main에서 분기, BOOT0 `9bbc505`+LCD `d5860a3` cherry-pick 선반영). 사용자 지시: "samd20은 동작이 정확하게 되는 코드" → 충실 포팅.
- **범위**: parse_lcd_comm(터치/키 입력+설정편집+DATA_SAVE/CANCEL) + change_lcd_page(페이지 렌더) + 주기 표시 스텝머신(LV_OUTPUT/LV_TIME 바, VAR_*, ICON) + 문자열 포맷터. 와이어/FRAM 계층은 Stage B 재사용. 측정값은 **스텁 provider**(전부 0 → 바 빈/값 0), HW/제어 결합은 **named 스텁 훅**(set_pot/us_command/comm_reconfigure/ether_apply/horn = Stage C/D 통합점).
- **구현(spec→plan→10태스크, 각 빌드-게이트+커밋)**: T1 VP 매크로 / T2 `app_config_save_all` / T3 포맷터(time/energy/addr/ip/pdd) / T4 서브시스템 state+측정스텁+훅 / T5 `change_lcd_page` / T6 입력 디스패치 코어 / T7 DATA_SAVE/CANCEL+comm/ether / T8 표시 스텝머신+아이콘+에러페이지 / T9 app.c 배선+SYS_PIC_NOW 루프가드. T1-T4 직접, T5-T9 subagent-driven(태스크별 리뷰+samd20 원본 대조).
- 신규 소스: `src/app_lcd_str.c`(167) `app_lcd_render.c`(223) `app_lcd_input.c`(802) `app_lcd_disp.c`(290), `app_lcd.c`(165) 확장. **빌드 0-warning, FLASH 27.9%(35684B)/RAM 9.3%**. config 소유권을 LCD 서브시스템으로 이동(`app_lcd_cfg()`).
- **보존된 samd20 퀴크(verbatim, 임의수정 ✗)**: F3 HAND 저장→MULTI 페이지 복귀. F4 cancel = full FRAM reload(live comm/ether는 저장前 미변경이라 no-op). STD 경로 저장은 comm_mode/ether **미영속**(addr/speed/parity만). `ether_select_field`가 항상 `temp_ether_ip[3]`로 시드(samd20 copy-paste 버그). KEY_MULTI RESET 에러클리어가 samd20 `sys_status==SYS_ERROR` 추가게이트 생략(에러경로 미도달, Stage D 재조정).
- **Stage B 콜드부팅 핸드셰이크 무회귀** (`dgus_wait_ready`/`ensure_run_page` 그대로; init_mode는 change_page 호출로 리팩터, 기본설정 RUN 페이지 렌더 byte-identical 검증). 회귀 여부는 첫 부팅서 실증.
- **잔여 cleanup(옵션)**: `app_lcd_input.c` 802줄(800 가이드라인 2줄 초과) — 분할 시 ether FSM을 `app_lcd_input_ether.c`로 절단 권장. 그린 빌드 리스크로 지금은 보류.
- **다음 = HW bench 검증 (T10, 사용자와)**. 통과 시 RESUME/NEXT_STEPS "done" 갱신 + PR + 태그 `hw-revA_fw-stage-lcd`. spec `docs/superpowers/specs/2026-05-27-stage-lcd-full-behavior-port-design.md`, plan `docs/superpowers/plans/2026-05-27-stage-lcd-full-behavior.md`.

#### HW bench 검증 체크리스트 (USART6 mon @115200; spec §13)
- [ ] 부팅 → RUN 페이지 렌더 (Stage B 패리티), `run_page_confirmed=1` (Stage B 경로는 inspection상 불변, 첫 부팅서 실증).
- [ ] SETUP 진입 → 현재 리밋 표시 → 값 편집 → SAVE → 전원 사이클 → 값 영속 (FRAM determinism).
- [ ] CANCEL → 값이 FRAM 값으로 복귀.
- [ ] 전 setup/comm/ethernet 페이지 네비 — 락업/SYS_PIC_NOW 루프 없음 (mon trace 관찰).
- [ ] **STD 모델(type=2)에서 STDE 이더넷 편집 SAVE/reload → comm_mode/ether 미영속 = samd20 동작과 일치 확인** (의도된 동작).
- [ ] **HAND 저장 후 MULTI 페이지로 복귀(F3)** — 편집한 페이지와 다른 페이지 표시, 의도된 UX인지 사용자 확인.
- [ ] **NM/GW 필드 선택 시 IP 마지막 옥텟이 초기값으로 표시**(`temp_ether_ip[3]` 시드, verbatim) — samd20과 일치 확인.
- [ ] **패널만 전원 사이클**(MCU 유지) → SYS_PIC_NOW 재init이 루프 없이 복구되는지 (200ms 가드가 정상 케이스를 안 막는지).
- [ ] 터치-헤비 네비 중 `dgus_rx_drop_count()` 관찰 (RX 링 오버플로 없음).
- [ ] 훅이 RUN/SEEK/RESET/SAVE에 `[lcd-hook]` 로그 — 하드웨어 미구동 배선 확인.
- [ ] (옵션) `LCD_DEMO_MEASURE_RAMP` 빌드 → LV_OUTPUT/LV_TIME 바 스윕, VAR_* 증가.

### 2026-05-26 — BOOT0 해결 + LCD 콜드부팅 핸드셰이크 픽스 (코드 변경 O, HW 검증)

- **BOOT0 이슈 해결**: BOOT0(U2.60)→GND 연결. 콜드부팅 시 플래시 앱 직접 실행. HW 검증: `reset halt` PC=0x080045c0/MSP=0x20008000, mon 정상. force-jump 워크어라운드 폐기 (fallback으로 문서 보존). `docs/NEXT_STEPS.md §5.7`, `docs/superpowers/RESUME.md`, memory `project_board_boot0_workaround` 갱신.
- **LCD 콜드부팅 레이스 픽스**: 증상 = 콜드 전원 사이클 시 로고 화면만 잔류(동작화면 미전환). 근본원인 = DGUS 패널이 MCU보다 늦게 부팅 → 단발 `set_page(run)`(전원-on 후 ~1050ms)이 패널 준비 전 도착·유실 → 패널이 power-on 기본 페이지(로고)에 잔류. ST-LINK reset 경로는 패널이 이미 부팅돼 있어 미재현(비대칭).
  - 수정 = blind 1s delay 대체 **readiness 게이트**(`dgus_wait_ready`: SYS_PIC_NOW(0x0014) read에 패널이 응답할 때까지 폴링, 최대 4s) + **read-back 확인/재전송**(`app_lcd_ensure_run_page`: SYS_PIC_NOW가 run 페이지 확정할 때까지 최대 8회 재전송). samd20의 주석처리된 `check_lcd_comm` 핸드셰이크(`ref/samd20/main.c:4933-5022`) 의도 복원.
  - UX 보존: readiness 게이트 후 **로고 1초 dwell → run 페이지**(samd20 `set_page(0)→delay 1000→run` 동일).
  - 신규 드라이버 API: `dgus_read_word`/`dgus_wait_ready`(`drivers/dgus_lcd.c`), `app_lcd_run_page`/`app_lcd_ensure_run_page`(`src/app_lcd.c`). 튜닝 상수 `DGUS_BOOT_READY_TIMEOUT_MS`/`DGUS_LOGO_DWELL_MS`/`DGUS_PAGE_CONFIRM_*`(`include/dgus_lcd.h`).
  - HW 검증: 콜드 전원 사이클 → 로고 ~1초 → 동작화면 전환 확인. mon `[lcd] ready=1`/`run_page_confirmed=1`. 빌드 0-warning, FLASH 22.57%.

### 2026-05-25 (후속) — ATmega16 보드-진실 정정 + 소스오브트루스 전환 (코드 변경 ✗, 문서만)

- Stage D brainstorming 중 사용자 보드 직독으로 **OSC 드라이브 매핑 확정**: `CTRL_OSC0..4` ← mega16 **PB1/PB0/PC4/PA7/PC7** (V30 PB2/PB10/PB12/PB13/PB14). 분석 §7 #1(OSC 트리거 물리핀 미해소) **해소**. **PA0(ADC ch0) = 출력 초음파 세기 피드백**(레귤레이션 입력) 확정.
- **소스 오브 트루스 전환**: 1차 = 사용자 보드 측정 + 행동 재분석, 2차(약함) = `m16_conv_v001.c` 디컴파일(**함수명·동작주석 무시**). 코드 정밀추적 결과 누진 드라이브 레벨이 RAM 변수(`disp_*`) + 본문부재 `output_direct_process()`로만 라우팅됨을 확인 → 디컴파일이 출력단을 못 보여줌.
- **뒤집힌 본문 결론**(이제 무시): "IPC=PB0/PB1/PA7/PC4/PC7"(§1), "PB0/PB1/PC7=상태표시 IPC"(§3), "누진패턴→dead display로만 감"(§4). 실제 IPC는 명령선(PA4/5/6)+M_OVLD(PC0) 별개 그룹.
- **측정 대기 (Stage D 차단)**: 분석 **§0.1 B1~B5** — ① 5 OSC핀 방향(in/out; PC4·PA7 코드상 입력) ② 극성·레벨↔비트매핑 ③ 레귤레이션 전달함수 ④ ADC ch1(PA1) 정체 ⑤ `g_main_state` 0/1 두 출력모드. 사용자가 측정+행동재분석 제공 예정.
- **"ref verbatim 포팅" 전제 폐기**. Stage D 결정분: 1슬라이스(피드백 루프, 모듈경계로만 분할), 코드-포팅 먼저+머지前 실측, 안전경계 옵션은 측정 후 재논의.
- 갱신 문서: `analysis/atmega16-io-behavior.md`(§0.1 정정 절 + 배너 + §7 #1 CLOSED 표기), `pinmap.md`(CTRL_OSC 식별 확정/방향·극성 대기, Q8b/Q11), `NEXT_STEPS.md` §1.1, `RESUME.md`, memory `project_atmega16_absorption`.

### 2026-05-25 — ATmega16 FW 동작↔I/O 신호 분석 완료 (코드 변경 ✗)

- 산출물: **`docs/superpowers/analysis/atmega16-io-behavior.md`** (kickoff §6 deliverable). 소스 = `ref/atmega16/m16_conv_v001.c`(디컴파일, 비판적 독해) + `hw/schematics/usw_ctrl_v26_samd20.pdf`(OLD V26, page4=ATmega16 U6, pdftoppm 600/1200dpi 크롭 판독, netlist 없음) + `hw/schematics/USW_CTRL_V30.asc`(NEW netlist).
- **아키텍처 확정**(사용자): ATmega16 = 트리거 생성 + 전류 모니터링(실시간 레귤레이션), SAMD20 = UI+전체제어, OSC = 별도 보드. IPC = 순수 GPIO(`hw_init`이 USART/SPI/TWI 전부 비활성).
- **디컴파일러 noise 정정**: C 헤더의 핀-역할 주석 대부분 추측. `display_*`(PORTD+PB2/3/4)는 V26 미연결(×) = dead legacy 7-seg. `BUZ_M16`/`U1.xx` net 주석 오류 다수.
- **HIGH 확정**: PA4=`M_START`(START in, 3중확인), PC0=`M_OVLD`(overload out, 3중확인). 제어루프: ADC(PA0=`SENS_OUT`) → `lookup_table` 스케일 → `output_level_process` 누진 5비트(0x01→0x1F) ↔ V30 `OSC_OUT0..4` 1:1. ISR cadence: ADC~208µs/Timer0~2ms/Timer1~10ms.
- **미해소(최우선 HW-verify)**: ① OSC 트리거 물리 출력핀 ② ADC ch0/ch1 정체(PA1 미연결인데 ch1 처리) ③ PC1 방향 충돌. 분석 §7.
- 다음 = (선행)HW-verify 패스 → **Stage D Ultrasonic regulation 흡수**(분석 §8). `docs/NEXT_STEPS.md` §1.1 + `docs/superpowers/RESUME.md` 갱신, memory `project_atmega16_absorption` 갱신.

### 2026-05-25 — Stage A main 머지 + repo 자산 정리

- `feat/stage-a-lcd-io` (33 commits) → `main` **`--no-ff` 머지** (`4651453`), 태그 **`hw-revA_fw-stage-a`**. 머지 후 빌드 재검증 FLASH 18.94% / RAM 8.37% ✅ (브랜치 수치 일치).
- 머지 전 정리: 미커밋 `docs/requirements.md` 보강분(시스템 아키텍처 + F1~F6/FW1~FW3) 커밋(`e322644`); stale untracked `docs/NEXT_STEPS.md`·`docs/superpowers/RESUME.md` → `docs/superpowers/historical/` 아카이브(`cea0c3a`, 머지 시 untracked 충돌 회피 + Phase 1~7 로드맵 보존).
- repo 자산 track(`25fb41f`): `ref/`(레거시 SAMD20/ATmega16 포팅 source 8.8 MB), `fw/cube/gds_usctrl.ioc`(핀 config), `AGENTS.md`, `docs/fw_analysis.md`, `.claude/settings.json`·`.codex/hooks.json`(RESUME 자동로드 SessionStart 훅). `graphify-out/` 는 `.gitignore` 추가(재생성 산출물).
- worktree `gds_us_ctrl-stageA` + 브랜치 `feat/stage-a-lcd-io` 삭제 — 현재 `main` 단독, working tree clean. origin push ✗ (로컬만).
- 다음 슬라이스 **Stage B (LCD app 데이터 사전 셋업)** 사용자 확정 → `docs/NEXT_STEPS.md` §4 + `docs/superpowers/RESUME.md` 갱신.

### 2026-05-06 — Stage A LCD I/O Bring-up 완료 (DGUS LCD on USART1, Task 1–13)

- USART1 (PA9/PA10, AF7, 115200 8N1) raw 드라이버 신설 — `fw/drivers/usart1.c`. HAL `Receive_IT` 1-byte 무장 + 64-byte ring buffer + 폴링 TX (10 ms timeout). `HAL_UART_RxCpltCallback` + `HAL_UART_ErrorCallback` 두 vendor weak 함수 instance-USART1 분기 override.
- DGUS 프로토콜 레이어 신설 — `fw/drivers/dgus_lcd.c`. samd20 9 함수 풀 패리티 (`dgus_reset_lcd / set_page / write_u16 / write_u32 / write_bytes / write_u16_array / write_text / read_var`) + 4-state RX 파서 상태머신 (`PS_IDLE → PS_GOT_5A → PS_GOT_HEADER → PS_COLLECTING`) + WR-echo 헬퍼 (`dgus_is_echo`) + 관측성 카운터 (`dgus_rx_drop_count`, `dgus_tx_timeout_count`, `usart1_rx_drop_count`, `usart1_rx_error_count`).
- samd20 결함 5건 명시 회피 (spec §3.7) — LEN [4,26] 검증, 50 ms 벽시계 timeout (sys_tick_get_ms), HAL_UART_Init Error_Handler, 폴링 TX 자연 직렬화, RX ring drop-not-overwrite 카운터.
- 데모 동작: 부팅 시 banner 3줄 (`[boot] gds_us_ctrl stage-a-lcd ready` + `[lcd] usart1@115200 ring=64 prio=5` + `[lcd] init ok, set_page=9, uptime VP=0x1110`) → `dgus_set_page(LCD_RUN_STD)` → 1 Hz 로 `dgus_write_u16(VAR_POWER, secs)` + mon `[t=N ms] hello uptime=N` cadence + LCD echo passive RX 로깅.
- 빌드 결과: FLASH **24,824 B / 128 KB (18.94%)** ✅ (spec §6.2 acceptance < 30 KB), RAM **2,744 B / 32 KB (8.37%)** ✅ (acceptance < 4 KB). 신규 export 심볼 (`usart1_init`, `dgus_init`, `dgus_set_page`, `dgus_write_u16`, `dgus_rx_poll`, `USART1_IRQHandler`, `HAL_UART_RxCpltCallback`, `HAL_UART_ErrorCallback`) 모두 `.text` 확인. warning 0.
- HW 검증: ST-LINK V3 + STM32F410RBT 보드 flash → mon banner 3줄 + 1 Hz hello uptime 출력 ✅. USART1 wire-level 명령 valid 인식 ✅ (LCD 가 매 set_page/write_u16 명령에 DGUS WR-echo `5A A5 03 82 4F 4B` 응답, GPIOA AFRH=0x770 / BRR=0x341 / CR1=0x202C 정상). PC=`app_loop_iter` Thread mode, fault 미히트 ✅.
- **scope 정정 (2026-05-06)**: 초기 spec §6.3 6c "LCD 페이지 시각 전환" 이 samd20 ref 의 `init_lcd_mode` 흐름 (`send_model_str` + DISP_*/LV_*/ICON_* VP 11+ 사전 셋업) 의존을 누락. spec §0.3 OUT OF SCOPE 에 "LCD application 데이터 사전 셋업 — Stage B" 명시 추가, §6.3 6c/6d/6f acceptance 를 wire-level 입증 + mon-side cadence + drop counter 정밀 정의 로 완화. **Stage A 코드 자체 정상**, samd20 레퍼런스 분석 결과 Stage B 슬라이스로 분리.
- spec/plan 정정 패턴 (Task 8 sys_tick_get_ms / Task 9 sys_tick_init,__enable_irq / Task 10 mon_writeln 미러): 발견 → spec/plan 정정 commit → 코드 first-time commit. 모든 substantive Task (4, 6+7, 8, 9, 10) spec compliance reviewer ≥ 12/12 ✅ APPROVE, severity CRITICAL/HIGH/MEDIUM/LOW 0.
- Code quality reviewer (Task 4-10 묶음, code-reviewer agent): HIGH 1 (`HAL_UART_ErrorCallback` 미구현 → ORE 시 RX 영구 정지) + LOW 3 (헤더 가드 `#pragma once` 통일, irq.c mid-file include, `extern Error_Handler` → `#include "clock.h"`) 모두 fix 완료.
- `feat/stage-a-lcd-io` 브랜치 → main 머지 완료 (2026-05-25, `4651453`, tag `hw-revA_fw-stage-a`).
- 산출 spec: `docs/superpowers/specs/2026-05-05-stage-a-lcd-io-design.md`
- 산출 plan: `docs/superpowers/plans/2026-05-05-stage-a-lcd-io.md`
- RESUME 아카이브: `docs/superpowers/historical/2026-05-06-RESUME.md`

### 2026-05-05 — Phase 1+2 Bootstrap 완료 (Chunks 1–13)

- CubeMX UI 의존 제거 결정. HAL/CMSIS는 `fw/vendor/` in-tree read-only 카피로 동결.
- 디렉토리 재편: `fw/cube/` → `fw/vendor/` + `fw/{src,include,drivers,openocd}/` 신설.
- 빌드 시스템: CMake + arm-none-eabi-gcc 툴체인 + Ninja, OpenOCD/ST-LINK V3 flash/debug.
- **Phase 1**: HSI ×12 = **96 MHz** SystemCoreClock 부팅. 빈 main while 루프 도달 GDB 검증 완료 (PC=0x80002ea, MSP=0x20008000).
- **Phase 2**: TIM11 1 kHz 틱 (PSC=95, ARR=999, NVIC priority 5), USART6 115200 8N1 monitor (PC6/PC7 AF8, blocking TX), PB3 1 Hz heartbeat (50% duty).
- 빌드 결과: FLASH 22 060 B / 128 KB (16.83%), RAM 2 520 B / 32 KB (7.69%).
- HW 검증: ST-LINK V3 + STM32F410RBT 보드에서 banner `[boot] gds_us_ctrl phase2 ready` + 1 Hz `[t=N ms] hello` + PB3 LED 점멸 모두 관찰. fault 미히트.
- 발견 + 해소된 결함:
  - `TIM_AUTORELOADPRELOAD_DISABLE` (no underscore) 매크로 오타 → `TIM_AUTORELOAD_PRELOAD_DISABLE` (commit `de0c35f`).
  - CMake genex `$<$<CONFIG:Debug>:-Og;-g3;-gdwarf-2>` semicolon 처리 → 각 flag를 별도 genex로 분리 (commit `8d67a7d`).
  - Cortex-M4 r0p1 6 HW BP 한계 — fault handler 모두 + main 동시 break 시 7개째 거부. 디버그 시 break 갯수 제한.
- `STM32_TOOLCHAIN` env var stale 경로 이슈 — 빌드 시 `env -u STM32_TOOLCHAIN cmake ...` 우회.
- `feat/phase1-2-bootstrap` 브랜치(33 commits) → main 머지 (merge commit on 2026-05-05).
- 산출 spec: `docs/superpowers/specs/2026-04-26-phase1-2-bootstrap-design.md`
- 산출 plan: `docs/superpowers/plans/2026-04-26-phase1-2-bootstrap.md`
- RESUME 아카이브: `docs/superpowers/historical/2026-05-05-RESUME.md`

### 2026-04-26 — 분석 세션
- 프로젝트 구조 생성 (`fw/`, `hw/`, `docs/`, `ref/`)
- `CLAUDE.md` 작성 — 통합 목표 (SAMD20 + ATmega16 → STM32F410RBT)
- `ref/samd20/`, `ref/atmega16/` 이전 코드 보관
- CubeMX `.ioc` 분석 → `docs/pinmap.md` 작성
  - W5500, DGUS LCD, I2C EEPROM, 5채널 OSC, TIM5 PWM 등 핀 매핑 확정
- graphify로 코드 분석: 340 노드 / 851 엣지 / 39 커뮤니티
- SAMD20 `main()` 슈퍼루프 추적 — 10-state job_state 머신 구조 파악
- ATmega16 `m16_conv_v001.c` 분석 — 독립 서브 컨트롤러, GPIO 시그널링
- `docs/requirements.md` 작성 — 시스템 아키텍처 (이전 → 신규), F1~F6
- `docs/NEXT_STEPS.md` 작성 — 회로도 미해결 질문 + 포팅 단계별 계획

### 2026-04-26 — FW 분석 마무리 + 회로 답변 통합
- **이전 MCU 간 GPIO 시그널 매핑 확정** (출처: `user_board.h:75-103` + 사용자 회로 동작 확인)
  - `M_START = PB13` (out, active LOW) → ATmega16 PA4 (초음파 출력 개시)
  - `M_SEEK  = PB11` (out, active LOW) → ATmega16 PA5 (공진 주파수 탐색)
  - `M_RESET = PB12` (out, active LOW) → ATmega16 PA6 (초음파 초기화)
  - `M_OVLD  = PB10` (in)               ← ATmega16 PC0 (과부하 알람)
- 통합 시 위 4개 GPIO는 **내부 함수/플래그**로 대체 (us_start/us_seek/us_reset/g_overload)
- 회로 미해결 항목 후속 답변 통합:
  - Q2-잔(PC1) = 무시, Q3(PC4) = 외부 입력, Q5(disp_led_pattern) = 단순 표시 출력
  - Q6(I2C_POT) = 외부 디지털 포텐셔미터 칩 → `i2c_adc_*` 그대로 포팅
  - Q7 = `.ioc`에 PA0 Net Name **`US_PWM_OUT`** 추가 완료
  - Q8 = CTRL_OSC0~4 = 외부 제어 신호 출력 (활성 극성은 추후 회로도 확정)
  - Q9 = TIM11 = 1 ms 시스템 틱
- `docs/fw_analysis.md` 신규 작성 — 포팅 참조용 종합 분석 (job_state 10단계, parse_lcd_comm, decode_comm, W5500/DHCP/TCP, 글로벌 상태, 인터럽트 인벤토리, F410 변환 규칙)
- `docs/pinmap.md` 갱신 — PA0 Net Name, CTRL_OSC0~4 외부 출력 표기, 부록 A(이전 MCU GPIO), 부록 B(결정), 부록 C(잔여)
- `docs/NEXT_STEPS.md` 갱신 — 회로 답변 반영, **개발 순서 4스테이지 재구성** (A: LCD+IO → B: Modbus → C: Ethernet → D: mega16 흡수)
- `docs/requirements.md` FW3 우선순위 = 4스테이지 순서로 재정렬

### 2026-04-26 — 잔여 회로 항목 결정
- **Q4 (7-segment) = 삭제**: DGUS LCD가 모든 표시 흡수. ATmega16 `disp_led_pattern` / 7-seg PORTB·PORTD 출력 폐기
- **Q8b (CTRL_OSC0~4 활성 극성) = 미정**: Stage D `do_control()` 구현 시 회로도 + 실측으로 확정
- **Q10 (ADC1 IN9 채널 추가) = 미정**: Phase 1 CubeMX 보정 + Stage D ADC 흡수 시 확인
- 위 결정 사항을 `docs/pinmap.md` (부록 B/C), `docs/fw_analysis.md` §13, `docs/NEXT_STEPS.md` 잔여 표에 반영

### 모든 회로 미해결 항목 종결 — 다음은 Phase 1 (CubeMX 보정) → Phase 2 (빌드 시스템 + 슈퍼루프 골격) → Stage A (LCD + IO)

### 2026-04-26 — Phase 1+2 brainstorming 진행 + 일시 중단
- `superpowers:brainstorming` 스킬로 Phase 1+2 설계 진행
- 결정 완료: 범위(Phase 1+2) / 통합방식(Hand-written CMake) / 데모(TIM11+UART hello) / 레이아웃(현 구조) / HAL(in-tree) / 플래시(OpenOCD+ST-LINK) / 아키텍처(Phased Modular)
- **새 제약 기록**: USART6 이중 역할 (Phase 2~Stage A: Monitor / Stage B 이후: Modbus RTU). `pinmap.md` USART6 섹션 + Monitor 모듈 backend 교체 가능 인터페이스 설계 반영
- 설계 6 섹션 중 **섹션 1/6 (디렉토리+모듈 경계) 사용자 승인 대기 중 일시 중단**
- 재개 가이드: `docs/superpowers/RESUME.md` 작성 — 결정 사항 / 대기 질문 / 남은 5 섹션 / 작업 재개 절차 모두 포함
- `.claude/settings.json` 신규 — **SessionStart 훅 추가**: 다음 세션 시작 시 RESUME.md를 자동으로 시스템 메시지(사용자 가시) + additionalContext(Claude 컨텍스트)로 주입. 사용자는 한 번에 컨텍스트 복원 가능
