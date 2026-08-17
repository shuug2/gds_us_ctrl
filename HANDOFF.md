# Handoff: 2026-07-18~19 사용자 신규 8건(fix/기능) 전건 HW PASS + USOUT=PCB 확정, 다음 = HMI Task 8

**Generated**: 2026-07-19 (풀배선 벤치 인터랙티브 세션, Fable→Opus)
**Branch**: `main` tip `61524c1` → 이후 docs 커밋 진행, 현재 main = `1364e5e`. **push 완료** — 2026-08-16 실측: 브랜치 3개·태그 21개 전부 origin 동기, 미푸시 없음.
**Status**: 사용자 신규 8건 전건 벤치 검증 완료. **★ 다음 세션 = HMI Task 8** (`~/dev/work/gds_us_hmi`, 별도 repo).

---

## ⟳ 2026-08-15 갱신 — 이 문서 작성 이후 발생한 것

이 아래 §Goal부터는 **2026-07-19 세션 기록 그대로**다. 그 뒤로 아래 3건이 추가됐고, 진입 시 반드시 함께 볼 것.

> **브랜치 스택 (2026-08-16 갱신)**: `main`(**ponytail 머지 완료**) ← `feat/remote-enable-gate`(**재베이스 완료**, T-5 = DGUS 자산 대기). 양쪽 origin 동기.

**① `refactor/ponytail-cleanup` — ✅ 벤치 전항목 PASS + main 머지 완료 (2026-08-16)**

벤치 실측(보드가 이미 이 브랜치 빌드로 플래시돼 있었음 — 덤프 대조 byte-identical, 재플래시 불요):

| 게이트 | 결과 |
|---|---|
| Modbus FC03 미러 | ✅ 30개 레지스터 전부 SWD `g_cfg`와 일치 |
| Modbus FC06 클램프 | ✅ OUT_POWER 120→100 / 30→50 / 56 복원 |
| 직접런 ceiling | ✅ 실측 자동정지 **[514, 578] ms** — 기대 560 ms(`limit_on_time` 56×10 ms) 부합, `1`×8→`0` 패턴이 과거 실측(537–617 ms)과 동일 |
| OVTIME 무회귀 | ✅ 런 중·정지 후 fault 비트 0 (STATUS=1 → 0, OVTIME `0x08` 미발생) |
| LCD SETUP comm/ether + DATA_SAVE | ✅ 사용자 육안 |
| IP 백스페이스 1자 / 모델명 문자열 | ✅ 사용자 육안 |

> 머지 결과 빌드가 벤치 검증된 보드 펌웨어와 **byte-identical**임을 확인 — 검증 결과가 main에 그대로 유효하다.
> 부수 확인: 명령 레지스터 `0x19~0x1C` 정지 후 전부 0(consume-and-clear 정상), `work_cnt` 0 유지(직접런은 미증가 = 설계대로).

<details><summary>머지 전 기록 (참고)</summary>

(main 대비 +12커밋, base `8eaac71`, tip `753778d`)
- 2026-07-19 리팩토링 4스테이지: 죽은 코드 삭제(`b02f5b1`, 바이너리 동일) / `app_lcd_input.c` 1038→622 분할 + 신규 `app_lcd_comm.c`(`e195564`) / `app_reg_tick` 118→57 헬퍼 추출(`30d001c`) / 전 269함수 한국어 ≤20자 주석 통일(바이너리 동일).
- 2026-07-25 4커밋: 모델 브랜드+버전을 `fw/include/define.h`로 분리 — 5종 컴파일-타임 선택(`7d9b7da`) / **MAKETECH** 신설 `SMT-{H|A|S}{freq}D`(`ebfd7d5`) / **ether IP 편집 커서 fix**(`e8e84fb`, `ip_to_string` 반환 16=필드폭이지 글자수 아님 → 헛 백스페이스) / `fw.sh` 빌드·플래시 스크립트(`753778d`).
- 게이트: 전 스테이지 0-warning + host 14스위트 PASS + cpp-review APPROVE. **머지 = 벤치 HW 재검증 3항목** ⓐ LCD SETUP comm/ether 편집 + DATA_SAVE 저장/복귀(분할 이동 경로) ⓑ 직접런 560ms ceiling + OVTIME 무회귀 ⓒ Modbus FC03/06 스모크. 07-25분 추가 육안 = 모델명 문자열 + IP 편집 백스페이스 1회 삭제. PASS 시 `git merge --no-ff`(태그 불요).
- ⚠ ~~보드는 이 브랜치 코드로 미플래시(여전히 `61524c1`)~~ — **이 기술은 오류였다**: 2026-08-16 플래시 덤프 대조로 보드가 이미 이 브랜치 빌드였음이 확인됐다(07-25 세션에서 플래시된 것). 재플래시 없이 검증 가능했다.

</details>

**② 원격 제어 활성화 게이트 — T-1~T-4 CODE-COMPLETE + VR-2/VR-3 실보드 PASS**(브랜치 `feat/remote-enable-gate`, base=main, **origin 푸시됨** tip `bc2067c`)

> **2026-08-17 실측 PASS**: 게이트 강제 빌드로 START/RESET/SEEK 전부 차단 확인(STATUS 무변화 + 레지스터 소거 + **`[sr]` OSC 구동 0건**), cfg 쓰기 거부(read-back 미러 복원), STOP 상시 통과, `0x2A=0x5201` 매직. 관측 로그:
> ```
> [mb] gate closed(state=0): blocked=0x1B stop_passed=0   ← START
> [mb] gate closed(state=0): blocked=0x00 stop_passed=1   ← STOP 통과
> ```
> 이 로그는 이번에 신설(`4db425d`) — 무음 거부는 "STATUS 무변화"라는 간접 증거만 남아 "막았다"와 "요청이 안 왔다"를 구분할 수 없었다.
> **남은 VR**: VR-10(probe 구/신 왕복)은 지금도 가능. VR-4~VR-9 + VR-3 stale-latch 후반부는 **T-5 이후**(게이트를 켤 수단 필요).
- 진입 정본 = **그 브랜치의 `HANDOFF.md`**. 설계 = `docs/superpowers/specs/2026-08-15-remote-enable-gate-design.md`, 실행 계획 = `plans/2026-08-15-remote-enable-gate-t1-t4.md`, 정책 결정 = `specs/2026-08-02-remote-enable-gate-decision.md`(`1364e5e`), 원격기 정본 = `~/dev/work/gds_us_remote`.
- **왜 필요한가**: 구 펌웨어에는 원격 제어 권한 게이트가 전혀 없다 — Modbus 도달 가능한 누구나 `START(0x1B)` 쓰기 가능, 물리 인터록도 없고 30s 절대 상한이 유일 backstop. `mb_write_reg`가 미사용 영역 write도 "성공" 에코하므로 원격기가 활성화 오판까지 할 수 있다(capability probe가 이를 막음).
- **완료(T-1~T-4)**: 순수 FSM `app_remote_en_fsm`(**host 15번째 스위트**) / 레지스터 `0x2A~0x2D` 계약 / `app_modbus.c` 글루(매 tick step·미러 3종·접근자 3종) / `apply_writes` 선두 게이트(명령 소거 불변식·STOP 상시 통과·cfg 체인 skip). `/code-review high` 3건 반영. FLASH 49.73%→49.77%.
- **남은 것**: **T-5(LCD 조작/표시) = DGUS 자산 대기** → T-6 리뷰 → T-7 벤치 VR-1~13 → T-8 머지(태그 `hw-revA_fw-stage-remote-gate`). **F-A(comm/eth `0x1E~0x29`)는 별도 스테이지로 분리**(비의존·덩치 큼).
- **사용자 작업 = DGUS 자산 3종**: 게이트 버튼 `0x1086`(**touch-down/up 양 이벤트 송신 필수** — 없으면 롱프레스 판정 불가), 상태 아이콘 `0x1155`, 잔여 초 `0x1211`. 반입 후 `LCD_TRACE_RX` 실물 트레이스 선행(추론 구현 금지 — `KEY_MULTI=0`·`0x120e` 전례).
- ⚠ **기본 빌드 플래시 금지** — 게이트를 켤 수단이 아직 없어 원격 명령이 RTU·TCP 양쪽에서 전부 막히고, **mbpoll 벤치 흐름과 `gds_us_hmi` 계약이 죽는다**(보드에서 복구 불가). 벤치는 `cmake -S fw -B build -G Ninja -DREMOTE_EN_GATE_BYPASS=ON`. 기본 OFF=게이트 유효이며 ON이면 CMake 경고 + 부팅 mon 경고가 나온다. **T-5 없이 main 머지 금지.**
- 확정 상수: 활성 창 **10분** / 링크 침묵 **10초**(VR-13 실측으로 확정), probe 매직 `0x5201` — **write-back 값은 비영값이어야 한다**(0이면 링크 전이 0-리셋과 구분 불가; 결정 기록의 예시 `0x0000`은 채택 안 함 → **원격기 spec 역반영 필요**).
- 원격기 파일럿(STOP·읽기·파라미터만)은 이것에 블로킹되지 않음. **원격 START의 유일한 선행.**

**③ push 상태** — 이 문서 원본의 "코드·docs 미푸시"는 stale이었고, 뒤이어 태그 7개도 푸시됐다. **2026-08-16 실측 = 브랜치 3개·태그 21개 전부 origin 동기, 미푸시 없음.** (8-15 갱신본이 한동안 "태그 7개 미푸시"로 남아 있었으나 그 시점 이후 푸시 완료 — 재시도 불요.)

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
- [x] ~~`refactor/ponytail-cleanup` HW 재검증 → 머지~~ — ✅ **완료 2026-08-16** (벤치 전항목 PASS, `--no-ff` 머지, 상단 §①).
- [x] ~~`feat/remote-enable-gate` 재베이스~~ — ✅ **완료 2026-08-16** (새 main 위, 코드 충돌 0·docs 1건만, 0-warning + host 15스위트 PASS, force-with-lease 푸시).
- [ ] **★ 원격 제어 활성화 게이트 T-5** — DGUS 자산 3종(사용자) → LCD 글루 → T-6 리뷰 → T-7 벤치 → T-8 머지. T-1~T-4는 브랜치에 완료 (상단 §②).
- [ ] (이월 3회째) **전류 표시 0.60A 실측** + **energy-exit 실전류** — 전류계 준비된 FW 벤치 세션. (EMA 체감은 2026-07-18 종결: α=1/2 유지 확정).
- [x] ~~태그 push~~ — ✅ 완료(2026-08-16 실측, 태그 21개 동기).
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
