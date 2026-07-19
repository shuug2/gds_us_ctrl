# Handoff: 2026-07-18~19 사용자 신규 8건(fix/기능) 전건 HW PASS + USOUT=PCB 확정, 다음 = HMI Task 8

**Generated**: 2026-07-19 (풀배선 벤치 인터랙티브 세션, Fable→Opus)
**Branch**: `main` tip `61524c1`(+이 docs 커밋) — **미푸시**: 코드 8커밋(`6af9882`..`61524c1`) + 직전 docs(`7258f7c`) + **태그 `hw-revA_fw-stage-mbtcp-hardening`**. 사람 터미널: `git push origin main && git push origin hw-revA_fw-stage-mbtcp-hardening`
**Status**: 사용자 신규 8건 전건 벤치 검증 완료. **★ 다음 세션 = HMI Task 8** (`~/dev/work/gds_us_hmi`, 별도 repo).

> **요약**: 사용자가 벤치에서 발견한 표시/부팅/알람/모드 이슈 8건을 fix/구현하고 전건 HW PASS. ⑴ 표시 데드밴드 20→14(최소 표시 0.15A=legacy 실효 게이트 복원) ⑵ 부팅 유령 SEEK 소멸(물리입력 bak zero-init) ⑶ 부팅 beep 신설+전원 직후로 이동 ⑷ fault 부저 알람 글루(OVTIME 등 무음이던 것) ⑸ 경고 페이지 전환 시 터치 RUN-키 토글 반전("떼면 시작") 해소 ⑹ SYS_HORN horn-down 모드 포팅(양손 키=솔 토글, 초음파/weld 배제) ⑺ STD 에너지 weld backstop→ERR_OVTIME(양손 weld OVTIME 알람 미발생) ⑻ USOUT(PB4) 미출력 = **코드 정상, PCB 원인 확정**(무수정). EMA α=1/2 유지·숫자 피크홀드·cal_val=16은 무변경 결정.

## Goal

사용자 벤치 발견 이슈 즉시 fix/구현 + 당일 HW 재검증 (벤치-수정 관례: main 직접 커밋 + cpp-review + HW PASS 동승).

## Completed (전건 cpp-review APPROVE + our-code 0-warning + host 14스위트 PASS + HW PASS)

- [x] **`6af9882` feat(reg)**: 표시 `REG_CURR_DEADBAND` 20→14 = 최소 표시 0.15A (legacy main.c:420 `>51 −37` 실효 게이트 복원; 구 20=0.21A 플로어). 바 게이트(>10) 무변경. host 벡터 갱신.
- [x] **`a46eaf3` fix(input)**: `input_fsm_init` bak 1→0(legacy BSS zero-init 충실) — 벤치 리그 EMSW NC 배선 평시 LOW인 PC11(multi=SEEK 역할)이 부팅 첫 tick에 내던 유령 SEEK 스윕 소멸. host `test_boot_active_inputs_no_ghost` 신설.
- [x] **`2ea5c2d`+`2cee1cc` feat(buzzer)**: 부팅 완료 1회 beep 100ms — legacy 부재(신규 기능), 처음엔 FSM 큐(슈퍼루프 시작 후)였다가 `sys_tick_init` 직후 블로킹 직접 구동으로 이동(전원 직후 발음, OSC PB12 윈도 전에 끝남).
- [x] **`6e30499` feat(alarm)**: `app_fault_alarm` 글루 신설 — `measure.error_status`(OVTIME 등) 활성 중 250ms/500ms 부저 점멸(legacy led_update SYS_ERROR 복원). 과부하/E-stop 개별 점멸과 disjoint(중복 없음).
- [x] **`789f347` fix(lcd)**: `app_lcd_input_run_key_reanchor()` — 홀드 중 OVTIME→경고 페이지 전환이 V30 RUN 컨트롤을 지워 release data=0 소실→토글 '눌림' 고착→press↔release 반전("떼면 START"+재시작마다 run_start_ms 리셋으로 제한시간 누적 불능). show_error 끝+set_estop(true)에서 재앵커(RUN_RELEASE+토글0, swallow 정리 포함).
- [x] **`519d908` feat(horn)**: SYS_HORN horn-down 포팅 — 순수 `app_horn_fsm`(host 8테스트) + 글루 `app_horn`. 양손 키 press=솔 토글, 초음파/weld는 게이트 2곳(`app_reg_start_allowed`+`app_weld_tick` 동결)이 배제. cpp 1차 BLOCK(공유 SOL 캐시 사각지대)→모드 전이 시 캐시 우회 무조건 OFF fix→APPROVE.
- [x] **`61524c1` fix(weld)**: STD 양손 weld(US_CYCLE) 에너지 backstop이 weld_fault만 내고 ERR_OVTIME 미세팅(app_weld_fsm.c:217 "후속 SYS_ERROR" 이연분)이던 것 — `app_reg_raise_ovtime()` 신설→`app_weld_hook_fault()`가 호출→직접런 OVTIME과 통합(부저+경고+STATUS+RESET 복구).
- [x] **USOUT(PB4) 미출력 = 코드 정상, PCB 원인** (무수정): 구동 조건(active 전이→io_usout)·극성(active-HIGH=legacy CTRL_ON=1)·핀 충돌 없음(SPI1=PA4/PC4/PC5, OSC=PB2/PB10/PB14)·핀 설정 전부 정상 확인 → 사용자 PCB 확정.
- [x] 메모리 갱신: [[project-lcd-amp-display-peak-hold]] / [[project-lcd-output-bar-realtime]] / [[project-bench-test-env]](PC11 평시 LOW) / [[project-ovtime-energy-run]](버그 3건) / [[project-sys-horn-port]] / [[feedback-confirm-before-code-change]].

## Not Yet Done

- [ ] **★ HMI SP1 Task 8 실보드 E2E** — `~/dev/work/gds_us_hmi` 세션 + 그쪽 HANDOFF.md. RS-485 어댑터 연결 + 보드를 LCD에서 SERIAL/addr=1/9600/EVEN 복원 필요(잔재 확인 필요). 이 repo `docs/superpowers/research/2026-07-05-rs485-first-write.md` §6 지참.
- [ ] (이월 3회째) **전류 표시 0.60A 실측** + **energy-exit 실전류** — 전류계 준비된 FW 벤치 세션. (EMA 체감은 2026-07-18 종결: α=1/2 유지 확정).
- [ ] **push** (사람 터미널) — 코드 8 + docs + 태그 `hw-revA_fw-stage-mbtcp-hardening`.
- [ ] 6b 잔여 / B-SEAM — ⏸ 사용자 보류 유지.

## Failed Approaches (Don't Repeat These)

- **EMA α로 "전류 표시 100ms 업데이트"를 해석** — 숫자 표시(VAR_AMP)는 EMA가 아니라 **피크홀드**(런 중 max_amp/정지 last_amp). α(app_reg.c:267)의 실시간 소비자는 바그래프+에너지 적분뿐. α=1/2 유지 결정(바 반응 우선). [[project-lcd-amp-display-peak-hold]].
- **SYS_HORN 첫 구현 = 모듈별 write-on-change 캐시** — weld와 horn이 각자 `s_sol_last`로 SOL 구동 시, horn 진입이 weld가 내려놓은 SOL을 영영 못 끔(cpp CRITICAL). 공유 액추에이터는 소유권 전이 시 **캐시 우회 무조건 강제 write**(app_input E-stop 패턴).
- **weld fault가 error_status를 안 세팅** — STD weld는 US_CYCLE이라 app_reg 직접런 OVTIME 분기(TOUCH/COMM/REMOTE)에서 구조적 제외. weld fault hook은 별도로 `app_reg_raise_ovtime()` 명시 배선 필요.

## Key Decisions

| Decision | Rationale |
|----------|-----------|
| 데드밴드 14(0.15A) 복원, 바 눈금 무변경 | legacy 실효 게이트 = 51−37 = 14. 눈금(ref_lv_*) 변경 금지 지시 유지 |
| EMA α=1/2 유지 (78a1e43 무변경) | 바그래프 반응(100ms) 우선. 숫자는 피크홀드라 α 무관 |
| cal_val=16 유지 | 사용자 의도 트림(전류계 대비 +0.16A) — 원복 안 함 |
| 부팅 beep = 블로킹 직접 구동(FSM 큐 아님) | 전원 직후 최속 발음. OSC PB12 윈도(~600ms) 전에 끝나 무영향 |
| SYS_HORN E-stop = 모드 유지+솔 OFF (legacy는 모드 소멸) | DGUS는 모드 소유가 LCD 체크박스 — 체크박스 상태 정합 유지 |
| weld OVTIME = 직접런과 같은 ERR_OVTIME | legacy RUN_WELD OVTIME도 동일(main.c:5292). fault 표면 통합 |

## Current State

**Working**: main `61524c1` — 보드에 플래시됨. 빌드 0-warning, host **14스위트** PASS(신규 horn_fsm 8케이스), working tree clean(이 docs 커밋 제외).

**보드**: `61524c1` 코드, **풀배선 리그**. ⚠ **세션 말미 전원 OFF 관측**(USOUT 조사 중 SWD 전압 0.003V) — 재개 시 **전원/ST-LINK 먼저 확인**. ⚠ **잔재 설정 불확정**: 세션 중 STD/HAND 모드·EN_ENERGY·EN_MULTI·horn·TIMEOVER 등을 다수 토글 — 재개 시 LCD/SWD로 model_type·comm_mode·EN_* 실측 필요(운영/HMI 투입 전 복원). ETH_STATIC .199 유지 추정(미확인). RS-485 어댑터 미접속.

**USOUT(PB4)**: 코드/핀/극성 정상 = **PCB 이슈**(무수정 종결).

**Uncommitted Changes**: 없음(docs 커밋 후).

## Files to Know

| File | Why It Matters |
|------|----------------|
| `fw/src/app_reg_calc.c` | `REG_CURR_DEADBAND 14`(표시 게이트) / `reg_current_from_adc`(GAIN 59/126, OFFSET 0) |
| `fw/src/app_fault_alarm.c` (신규) | 일반 fault(error_status) 부저 점멸 — OVTIME/향후 OUTERR |
| `fw/src/app_horn.c`+`app_horn_fsm.c` (신규) | SYS_HORN horn-down; 공유 SOL 캐시 우회 무조건 write 패턴 |
| `fw/src/app_reg.c:132-160` | `app_reg_start_allowed()`(horn/estop/overload/fault/seek 게이트) + `app_reg_raise_ovtime()`(weld fault 세터) |
| `fw/src/app_lcd_input.c` | `app_lcd_input_run_key_reanchor()`(토글 반전 방지) + SETUP horn 체크박스 미러 |
| `fw/src/app_weld.c:80-83, 207` | `app_weld_hook_fault()`→`app_reg_raise_ovtime()` (STD weld OVTIME 알람) |

## Code Context

**표시 게이트 (0.15A 최소)** `fw/src/app_reg_calc.c`:
```c
#define REG_CURR_DEADBAND 14   /* v = ch1*59/126 + cal_val; v<=14 -> 0 (legacy 51-37) */
```

**weld OVTIME 알람 배선** `fw/src/app_weld.c`:
```c
void app_weld_hook_fault(void) {      /* weld 에너지 backstop abort 엣지 */
    app_reg_raise_ovtime();            /* g_reg.error_status |= ERR_OVTIME */
    mon_printf("[weld] fault: energy timeout ... -> ERR_OVTIME\r\n");
}
```

**설계 불변식 3종** (이 세션 확립/재확인):
1. `dgus_set_page()` 호출은 반드시 `state->lcd_status` 스탬프와 쌍 (2026-07-11).
2. 런 페이지를 떠나는 모든 페이지 전환은 `app_lcd_input_run_key_reanchor()` 호출 (2026-07-18 신규).
3. 여러 모듈이 같은 액추에이터를 구동하면 소유권 전이 시 캐시 우회 무조건 강제 write (2026-07-18 신규).

## Resume Instructions

**HMI 세션 (최우선)**:
1. `cd ~/dev/work/gds_us_hmi` 새 세션 → 그쪽 `HANDOFF.md`(SP1 Task 8 = 실보드 E2E).
2. 준비물: RS-485 어댑터 연결 + 보드를 LCD에서 SERIAL/addr=1/9600/EVEN 복원(현 잔재 불확정) + 이 repo research doc §6.

**FW 전류 벤치 세션 (전류계 준비되면)**:
1. 플래시 불필요(보드=main 최신). 전원/ST-LINK 확인 후 유휴 표시 확인 → RUN → 전류계 0.6A ↔ 표시 0.60A.
2. EN_ENERGY=ON 실전류 energy-exit 확인.

## Warnings

1. **세션 말미 보드 전원 OFF** — 재개 시 전원/ST-LINK(target voltage) 먼저 확인. 잔재 설정 다수 토글됨(model_type/EN_*/comm_mode 불확정) → LCD/SWD 실측 후 복원.
2. **USOUT(PB4)=PCB 이슈** — 펌웨어 무관, 재조사 불필요(코드/극성/핀 정상 확인 완료).
3. **mbpoll**: 쓰기 값은 IP 뒤 / 부팅 직후·연속 TCP 간헐 실패 → 재시도 / 주소 1-based(STATUS=30/START=28/STOP=29/RESET=26). model_type=multi(1)이면 Modbus 직접런 자동정지 없음(30s 캡만) — 테스트 후 STOP.
4. **SWD 규칙**([[feedback-swd-halt-breaks-board-validation]]): 런타임 검증에 halt 금지. mbpoll+LCD 육안+비침습 openocd read_memory 루프만. SWD는 플래시/부팅직후 정적 1회.
