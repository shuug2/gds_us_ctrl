# RESUME — 다음 세션

> **상태 (2026-06-12 c, 마감 — Stage C slice 1 구현 완료, HW E2E 대기)**: **Modbus 코어+RTU 코드 완결.** plan `docs/superpowers/plans/2026-06-12-stage-c-modbus-slice1-core-rtu.md`(`5463370`) → subagent-driven 실행(Task 2~8 = 구현→spec리뷰→cpp-reviewer 2단 게이트, 코멘트 전건 반영) → **최종 통합 cpp-reviewer = APPROVED**(CRIT/HIGH 0, Minor 3 = NOTE 주석 반영 `ef6b371`). 브랜치 **`feat/stage-c-modbus-core-rtu`**(base = m1-주입 stacked tip, 16커밋, tip `ef6b371`). 산출물: `app_modbus_core.{c,h}`(순수 코어, 호스트 TDD; 원본 버그 3건 수정 = OOB/FC05 에코/FC01 풀바이트) + `usart6_mb.{c,h}`(DMA2 S1 Ch5 no-ISR + samd20 갭 프레이밍 + TX 후 에코 flush) + `app_modbus.{c,h}`(매 tick 미러 + FC06 클램프 체인 + save_all FRAM + US_COMM 명령 + 점유 전환 매 tick 평가) + `app_reg_command(cmd,src)`/max_amp/COMM ceiling + mon 게이트. 게이트 = main 클린 0-warning(FLASH 31.1%/RAM 12.3%)·trace 구성 0-warning·호스트 ×2 PASS. **다음 = ① 보드 재연결 시 slice 2b HW 재검증(HANDOFF §Resume — 변동 없음) → 머지 체인(2b → m1-주입 → 본 브랜치) ② 본 브랜치 HW E2E = plan §HW-gated(mbpoll 매트릭스 6항목; 트레이스 검증은 addr=NONE 미점유에서) ③ HW 없이 더 갈 거면 = Stage C slice 2(Modbus TCP/W5500) brainstorming부터.** B-SEAM 랜딩 시 주의: `app_modbus.c` pot-guard NOTE(스테일 g_measure → 라이브 accessor 필요).
> - **이전 상태 (2026-06-12 b)**: ↓ 스펙 승인(아래 블록).

> **상태 (2026-06-12 b, 마감 — Stage C slice 1 스펙 승인, plan부터 새 세션)**: **Modbus 작업 개시 — brainstorming 완료 → 스펙 작성·셀프리뷰·사용자 승인 완료(`ef359c5`).** 스펙 = `docs/superpowers/specs/2026-06-12-stage-c-modbus-slice1-core-rtu-design.md`. 핵심 결정(사용자): ① slice 1 = 공유 코어+RTU, TCP(W5500)=slice 2 ② USART6 mon↔Modbus = **런타임 점유 전환**(`comm_mode==SERIAL && addr!=0` ⇔ Modbus 소유, samd20 충실; 주소 NONE으로 mon 복구) ③ **전체 충실 포팅**(레지스터맵 0x00~0x1D + 설정 FRAM 커밋 + 명령 START/STOP/SEEK/RESET → run FSM **US_COMM** 소스) ④ 구조 = 접근 A(순수 코어 `app_modbus_core` 호스트테스트 + `usart6_mb` DMA+IDLE 전송층 + `app_modbus` 통합). 주의 디테일: DISP_POWER/AMP = max/last 미러(amp 추적 추가 필요), MODEL_* = read-only, COMM START 가드 `==US_IDLE`(의도적 편차), COMM ceiling = 원본충실, samd20 FRAM 주소 복붙버그 2건 수정 포팅, RS-485 DE = HW 자동(FW 제어 불필요, V30 U13/U16 추적). **다음 세션 = `superpowers:writing-plans`로 구현 계획 작성 → 구현(브랜치 `feat/stage-c-modbus-core-rtu`, base = 현 stacked tip) → 호스트테스트+빌드 게이트.** HW E2E(mbpoll 마스터)는 보드 후. 컨텍스트 50% 규칙으로 세션 분할(사용자 결정).
> - **이전 상태 (2026-06-12 a)**: ↓ HW 비의존 후속 2건(M1 주입 + us_on_time_200m) — 아래 블록.

> **상태 (2026-06-12, 마감 — HW 비의존 후속 2건 완료, stacked 브랜치)**: **보드 부재 중 deferred 2건 처리 = M1 파라미터 주입 리팩터링 + `us_on_time_200m` 공급(LCD 감사 갭 ②). 브랜치 = `refactor/stage-d-m1-cfg-param-injection`(base = slice 2b tip `322a779`, 코드 3커밋 `ce81d89`/`ec1b6d4`/`cb6ce44`), cpp-reviewer = APPROVED-WITH-COMMENTS(CRITICAL/HIGH 0, MEDIUM 3건 전부 반영, LOW 1건 pre-existing 미반영).** slice 2b 브랜치(`feat/stage-d-slice2b-run-gate`) + `fw/build-trace/` 재플래시용 바이너리 = **무변경 보존** → slice 2b HW 재검증 절차/합격기준은 `HANDOFF.md` §Resume 그대로 유효. 게이트 = main 0-warning(FLASH 28.72%/RAM 10.64%)·호스트 PASS·REG_TRACE 경로 syntax-check PASS. **다음 = ① 보드 재연결 → slice 2b HW 재검증(HANDOFF 기준, 기존 trace 바이너리) → slice 2b 머지 + 태그 `hw-revA_fw-stage-d2b` → ② 본 stacked 브랜치 rebase/머지(이때 LV_TIME 바 HW 확인: 런 중 200ms 단위 fill, 정지 후 마지막 값 latch).** 남은 HW 비의존 후보 = Modbus(Stage C, brainstorming부터 = 사용자 대화 필요). SEEK/RESET 체인·overload·weld-cycle·B-SEAM·6b cal = HW-gated 유지. 상세 = changelog 2026-06-12.
> - **이전 상태 (2026-06-10 b)**: ↓ slice 2b 통합 리뷰 APPROVED(아래 블록).

> **상태 (2026-06-10 b, 마감 — slice 2b 통합 리뷰 APPROVED)**: **Stage D slice 2b — `b78cbbf`+`27d45c3` 통합 cpp-reviewer 리뷰 완료 = APPROVED(CRITICAL/HIGH 0), 남은 것 = HW 재검증(보드 재연결 시)만.** 브랜치 `feat/stage-d-slice2b-run-gate` tip `c10b0aa`(리뷰 비차단 M2/L1 주석 추가 — `app_lcd_input.c` data=0 분기 same-burst stale `us_run_status` 의도 노트 + `app_reg.h` `app_lcd_cfg()` 라이브 읽기 노트; 코드 무변경). 리뷰 검증 범위 = data=0 매핑 코너 케이스 전수(`swallow_start` 누출/오흡수 없음, SYS_PIC_NOW 상호작용 OK), 워밍업 단발성, ceiling 산술(uint32 래핑-안전), ADC 누산, ISR 무race; 스펙 편차 4건 = 전부 2026-06-10 분석 문서 승인 확인. M1(app_lcd↔app_reg 순환 의존 → 후속 파라미터 주입 리팩터링)·L2(`reg_ramp_level` 잔류 = 의도적) = deferred. 빌드 main 28.67%/trace 29.00% 0-warning, 호스트 PASS. **다음 = 보드 재연결 → `fw/build-trace/` 재플래시 → HW 재검증(부팅 ~4s 워밍업 1회 → RUN press 즉시 `cmd=0 run=2`+`sel=scale(ch0_avg)`·ICON_RUN, 램프 없음, 500ms ceiling 자동정지(기본값), release 시 swallow 1회 정상) → 통과 시 finishing(머지) + 태그 `hw-revA_fw-stage-d2b`.** 절차 = `HANDOFF.md` §Resume.
> - **이전 상태 (2026-06-10 a)**: ↓ IPC 의미론 교차검증 + 코드 수정(리뷰 전 기록).

> **상태 (2026-06-10, 마감 — IPC 의미론 교차검증 + 충실도 정정)**: **Stage D — samd20 소스 × M16 디스어셈블리 교차 검증으로 런타임 의미론 확정, 비판적 검토 발견 4건을 사용자 결정대로 반영(코드 수정 완료, HW 재검증 대기).** 산출물 = `docs/superpowers/analysis/2026-06-10-samd20-m16-ipc-semantics-verified.md`(권위 소스) + changelog 2026-06-10. 브랜치 `feat/stage-d-slice2b-run-gate`(코드 `27d45c3` fix(stage-d) + 같은 날 docs(stage-d) 커밋 = 브랜치 tip, working tree clean; `ref/atmega16/M16_reverse/`만 종전대로 untracked). **수정 ①(원본충실 회귀)**: M16 램프 = **부팅 1회 워밍업**(0x0195 재진입 불가, 램프 중 OSC off + 명령 무시 @0x041E)이지 per-START 소프트스타트가 아님 → `app_reg.c` boot 1회 ~4s 워밍업(명령 무시·sel=0) 후 **START 즉시 `reg_scale(ch0_avg)`**, per-START 램프 폐기(`reg_ramp_level`은 calc에 레퍼런스 보존). **수정 ②(의도적 안전 추가)**: US_TOUCH 런에 `limit_on_time×10ms`(기본 500ms, 0=off) **on-time ceiling**(원본은 REMOTE/COMM+HAND 전용 — V30 data=0 quirk의 release 유실 대비) + ceiling 후 잔여 release의 START 역매핑은 `swallow_start` 1회 소비로 보정. **수정 ③**: ADC **1ms 페이스 양채널**(ch0_avg 40→10ms, ch1_avg 400→50ms; 원본 8.3/42ms 근사). **문서화 ④**: RESET→SEEK 500ms 자동 체인 + SEEK 500ms 자동해제 = SEEK/RESET 구현 시 필수(분석 문서 §5); **B-SEAM 가설 축소** = OSC 구동은 명령 3선 active-LOW 레벨 미러 유력(분석 §6, 벤치 측정은 "3선 미러 확인"으로 축소); M16 PA7/PC1/PC4 = IPC 아님. **LCD 포팅 전수 감사(후속) = 합격**(분석 문서 §6b, 코드 무변경): 34 VP 1:1 대응 + V30 적응/편차 문서화 확인; **미문서 갭 3건 deferred 기록** — ① RESET/SEEK/ERROR_RESET 아이콘 점등 엣지(체인 구현 시 포함) ② `us_on_time_200m` 미공급(LV_TIME 바 런 중 0, app_reg 후속 후보) ③ `work_cnt` 증가(용접 사이클 소속); **set_pot(I2C_POT, F2) = 진폭 제어 실체로 격상**(실출력 = B-SEAM 3선 미러 + I2C_POT). 빌드 0-warning(main+build-trace), 호스트 PASS. **⚠ HW 재검증 기대치 변경**: RUN = (부팅 ~4s 워밍업 1회 후) press 즉시 `cmd=0 run=2`+`sel=scale(ch0_avg)`·ICON_RUN, **램프 없음**, 500ms ceiling 자동 정지(기본값) → release 시 swallow 1회 정상. b78cbbf+금일 수정 cpp-reviewer = 다음 세션.
> - **이전 상태 (2026-06-08 c)**: ↓ HW 세션(부팅 IDLE/RESET/SEEK PASS, RUN quirk fix b78cbbf).

> **상태 (2026-06-08 c, 마감 — HW 세션)**: **Stage D slice 2b — HW 검증 일부 PASS, RUN은 V30 에셋 quirk로 막혀 펌웨어 fix `b78cbbf` 적용, RUN 재검증은 보드 재연결 후.** 브랜치 `feat/stage-d-slice2b-run-gate`(tip `b78cbbf`). REG_TRACE+LCD_TRACE_RX 빌드 플래시(ST-LINK) + SWD g_measure read + USART6 mon. **PASS: ① 부팅 IDLE**(`[reg] run=0 st=0 sel=0`, auto-ramp 없음, SWD us_run_status=0) **② 명령 라우팅**(RESET `cmd=2 run=0` + SEEK `cmd=1 run=0` = hook→app_reg_command HW 확인, no-op 정상). **RUN 막힘 원인규명(systematic-debugging)**: live `[lcd] rx` 트레이스 = **V30 RUN 버튼이 `KEY_MULTI(0x1080)` 키값 0을 press·release 양 edge 반환**(RESET=1/SEEK=2 정상; data=0 RUN 고유), `handle_key_multi`(samd20 포팅)는 data=3/4만 처리 → RUN 명령 미발화 = **LCD 스테이지 버그A와 동일한 V30 에셋 root**(`hw/lcd/dgus/13TouchFile.bin`, 편집 DWIN 프로젝트 repo 부재). **사용자 결정 = 펌웨어 적응(Option B)**: fix `b78cbbf`(`app_lcd_input.c` 한 파일, FSM/enum 무변경, spec §4.4) = `handle_key_multi` `data16==0`을 live `us_run_status`로 START(IDLE)/RUN_RELEASE(running) 매핑 → down/up data=0 쌍이 hold-to-run 재구성, self-syncing. 빌드 0-warning, 호스트 PASS. **⚠ 보드 USB가 fix 적용 후 분리됨 → RUN/ICON_RUN/램프/latch/re-arm 재검증 + fix `b78cbbf` cpp-reviewer = 다음 세션.** 절차 = `HANDOFF.md` §Resume(트레이스 빌드 `fw/build-trace/` 준비됨, 재플래시만). 통과 → finishing + 태그 `hw-revA_fw-stage-d2b`.
> - **이전 상태 (2026-06-08 b)**: ↓ slice 2b 코드 완료 + 통합 리뷰 APPROVED(HW 세션 전 기록).

> **상태 (2026-06-08 b, 마감)**: **Stage D slice 2b (RUN 명령 게이트) — 코드 완료 + 전체 cpp-reviewer APPROVED, Task 3 실보드 HW 검증만 남음(보드 미연결로 대기).** slice 2a는 main 머지 완료(`43fda87`, tag `hw-revA_fw-stage-d2`). slice 2b는 brainstorming→spec→plan→subagent-driven 구현. 브랜치 **`feat/stage-d-slice2b-run-gate`**(main 미머지). **터치 RUN start/stop 게이트만**; 부팅 auto-run→IDLE 전환, momentary hold-to-run, ICON_RUN, `us_run_status` taxonomy `{IDLE,REMOTE,TOUCH,COMM}`. 빌드 0-warning(FLASH 28.64%/RAM 10.60%), 호스트 `all checks PASSED`. 3 코드커밋: `decfc28`(run FSM+taxonomy+라우팅, START `==US_IDLE` 엣지 가드[advisor catch]) / `ed2093f`(ICON_RUN 엣지 렌더) / `4264bab`(SYS_PIC_NOW mid-run 리셋→RUN_RELEASE 정지[cpp-reviewer catch, spec §4.3]). Task별 spec+cpp-reviewer + 전체 통합 cpp-reviewer 모두 APPROVED. **다음 = Task 3 HW 검증(보드 연결 시) → finishing(머지/PR) + 태그 `hw-revA_fw-stage-d2b`.** 상세 절차 = `HANDOFF.md`(루트) §Resume / plan Task 3(합격기준 6항). 비차단 노트: ① IDLE `curr_amp` 발행→출력 바 idle 노이즈 플로어 HW 확인(plan item 6) ② FSM 호스트 미테스트(HW 게이트 의도). **SEEK/RESET 효과·overload·weld-cycle·Modbus·OSC 구동(B-SEAM)·6b = 여전히 DEFERRED.**
> - **이전 상태 (2026-06-08 a)**: ↓ slice 2a HW PASS + main 머지(아래 블록).

> **상태 (2026-06-08 a, 마감)**: **Stage D slice 2a (상태머신 + soft-start 램프) — Task 4 실보드 HW 검증 PASS, 통합 준비 완료.** STLINK V3 플래시 + USART6 mon trace로 부팅 램프 검증(reset 2회 재현): `st=1` sel 128→1024 단조 / band 18→0 / ~4s(rc 401×10ms) / rc=401 `st=0` 핸드오프 / st=0 = slice-1 verbatim 무회귀 / 배너 정상 + LCD power **숫자** 램프 추종 상승. **펌웨어 코드 변경 없음**(기검증 3커밋 그대로). **합격기준 2건 정정**(spec 디스플레이 계층 오독): 출력 바는 `curr_amp` amplitude 바(전압주입 없으면 idle 정지 = by-design), `ICON_RUN`은 samd20 run 명령 FSM(`main.c:4302`) 소속 = **slice 2b 몫** → 둘 다 예상 거동, 수정 불필요. 정정 반영 = spec §8.2 + plan Task 4 + changelog 2026-06-08. **main 머지 완료(`43fda87`, `--no-ff` 2-parent, tag `hw-revA_fw-stage-d2`)** — 머지 후 빌드 0-warning(FLASH 28.52%/RAM 10.57%) + 호스트 테스트 PASS, 최종 cpp-reviewer APPROVED, 피처 브랜치 삭제. origin push ✗(로컬 main이 origin 대비 144 ahead, local-authoritative). **다음 = slice 2b(명령 FSM = START re-arm·ICON_RUN 점등·SEEK/RESET·overload·blink) + OSC 물리 출력(B-SEAM) + 6b calibration = 여전히 DEFERRED.**
> - **이전 상태 (2026-06-03)**: ↓ slice 2a 코드 완료(Task 1~3), HW 검증 대기(아래 블록은 검증 전 기록).

> **상태 (2026-06-03, 마감)**: **Stage D slice 2a (상태머신 + soft-start 램프) — 코드 완료(Task 1~3), Task 4 HW 검증만 남음(보드 미연결로 대기).** slice 1은 main 머지 완료(`5aea06f`, tag `hw-revA_fw-stage-d`). slice 2a는 brainstorming→spec→plan→subagent-driven 구현. 브랜치 **`feat/stage-d-slice2-softstart`**(main 미머지, tip `ae24ec4`). 빌드 0-warning(FLASH 28.52%/RAM 10.57%), 호스트 테스트 `all checks PASSED`, 3 Task 모두 2-stage 리뷰(spec+cpp-reviewer) APPROVED. **다음 = Task 4 HW 검증(보드 연결 시) → 통과 시 최종리뷰 + finishing(머지/PR) + 태그.** 상세 절차 = `HANDOFF.md`(루트) + plan Task 4.
> - **slice 2a 구현 3커밋**: `d98b025`(순수 `reg_ramp_level` 10-rung thermometer fill + 호스트 테스트) / `ba67971`(`US_IDLE/US_RUNNING` enum→app_lcd.h) / `ae24ec4`(app_reg.c 상태머신 init=1 + 10ms 램프 cadence ≥401→state0 + sel MUX + us_run_status). 출력 OSC·정확한 패턴바이트·명령 FSM·overload·blink = **여전히 DEFERRED**.
> - **slice 2a 핵심**: `sel = state==1 ? reg_ramp_level(ctr) : reg_scale(ch0_avg)`(state==0=slice-1 verbatim 무회귀). 램프 레벨을 LCD bar(curr_power)에 미러 = 라벨된 테스트 deviation(전압주입 없이 패널 육안 검증). spec `…/specs/2026-06-02-stage-d-slice2-softstart-ramp-design.md`, plan `…/plans/2026-06-03-stage-d-slice2-softstart-ramp.md`.
> - **Task 4 HW 검증 (다음, 보드)**: 트레이스 빌드(⚠ CPU 플래그 포함) → 플래시 → 부팅 `[reg] st=1` sel 128→1024/band 18→0 ~4s + LCD bar 상승 + st→0 핸드오프 + 무회귀. 명령 = HANDOFF.md §Resume.
> - **이전 상태 (2026-06-02)**: ↓ slice 1 6a PASS(아래 블록은 "머지 전" 시점 기록 — 이후 머지됨).

> **상태 (2026-06-02, 마감)**: **Stage D slice 1 (레귤레이션 코어 compute) — HW 기능검증(6a) PASS, 통합 준비 완료.** 실보드 검증에서 Task 6을 **6a(기능/구조, 전압스윕 불필요)** + **6b(신호 calibration, HW 준비 후)** 로 분리(measure-first). **6a 전체 PASS**: compute liveness(×6 + floor→0 + band21 라이브), 무회귀(배너+LCD 네비), OSC 안전(PB2/PB10/PB14 idle-HIGH 스코프), LCD provider live(VAR_POWER=0). 펌웨어 빌드 0-warning, 호스트 단위테스트 `all checks PASSED`, cpp-reviewer는 코드 미변경분 APPROVED 유지. 브랜치 **`feat/stage-d-regulation-core`** (main 미머지). **다음 = finishing-a-development-branch 선택(머지/PR) + 태그 `hw-revA_fw-stage-d`. 6b calibration = 추적 후속(HW-gated).**
> - **6b calibration (DEFERRED, HW 준비 후)**: `>>2` 정규화 + 2.56V↔3.3V 도메인 실측 보정(DP2) / ch0_avg·scaled 물리단위(B-UNITS/B3) / ADC offset·gain / OSC 출력경로·비트매핑·극성(B-OSC-MAP/B-SEAM) + PB12/PB13 방향. 전압 가변 + 실 초음파 구동 필요.
> - **설명 문서**: `docs/superpowers/analysis/2026-06-02-m16-to-stm32-port-explained.md` (M16→STM32 포팅 compute+I/O 정리, 핸드오프용).
> - **이전 상태 (2026-06-01)**: ↓ 구현 완료, HW 검증 대기.

> **상태 (2026-06-01, 마감)**: **Stage D slice 1 (레귤레이션 코어) — 구현 완료, 실보드 HW 검증 대기.** spec → plan(`writing-plans`) → inline 구현(6 커밋). **compute 파이프라인만**(OSC 물리 구동 = B-SEAM, 벤치 측정까지 DEFERRED). 빌드 0-warning, 호스트 단위테스트 PASS, **cpp-reviewer APPROVED**(차단 0). 브랜치 **`feat/stage-d-regulation-core`** (main 미머지). **다음 = Task 6 HW 검증(REG_TRACE, 사용자+보드) → 통과 시 머지/PR + 태그.**
> - **plan**: `docs/superpowers/plans/2026-05-31-stage-d-slice1-regulation-core.md` (Task 1~5 = 구현 완료/커밋; Task 6 = HW 검증, 미실행).
> - **구현 산출물**: 신규 `app_reg_calc.{c,h}`(순수 `reg_scale`/`reg_output_level`+table, HAL-free, 호스트 테스트 `fw/test/`) + `adc1.{c,h}`(ADC1 폴링 PB0/PB1) + `app_reg.{c,h}`(2ms 틱, 10/50 평균, scale+lookup, `lcd_measure_t` 발행). 통합: `main.c`/`app.c`/`app_lcd.c`. `board.c` OSC 마스크 정정 + 3채널 idle-HIGH. `CMakeLists`+adc HAL·SYSTEM include, `hal_conf.h` ADC 모듈 enable. **FLASH 28.39%/RAM 10.55%**.
> - **spec 정제 3건**(plan §Deviations): 순수함수 분리(호스트테스트)/adc1 얇은 페리페럴 계층/`reg_output_level`=band index만(C-LADDER 패턴바이트 맵은 출력단=B-SEAM과 함께 DEFERRED=YAGNI, pure-add 후속).
> - **Task 6 HW 검증 (다음, 사용자+보드)**: 트레이스 빌드 = `env -u STM32_TOOLCHAIN cmake -B build-trace -G Ninja -DCMAKE_C_FLAGS="-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard -DREG_TRACE"`(⚠ CPU 플래그 함께 — `-DCMAKE_C_FLAGS`는 캐시를 덮어써 툴체인 `CMAKE_C_FLAGS_INIT`의 CPU 플래그가 날아가 ARM-mode 빌드 실패. 2026-06-02 정정) → 플래시 → mon `[reg] ch0/ch1/scaled/band`; PB0 주입 시 ch0 추종·×6/clip·band 단계 + LCD bar 이동 + PB2/PB10/PB14 idle-HIGH·OSC 무구동·PB15 NC 확인. 상세 = plan Task 6.
> - 미커밋: changelog/RESUME/NEXT_STEPS doc + 메모리(working tree). `ref/atmega16/M16_reverse/` 여전히 untracked(분석 ground-truth — 커밋 여부 사용자 결정).
> - **이전 상태 (2026-05-31 b)**: ↓ Stage D 분석 완료 + spec 초안 (구현 전).

> **상태 (2026-05-31 b, 마감)**: **Stage D slice 1 (레귤레이션 코어) — 분석 완료 + spec 초안·리뷰 대기.** disasm-adjudicated 분석(55-agent workflow, 3.7M tokens)으로 검증된 fact base 확정 → slice-1 spec 초안. **코드 변경 ✗**(분석+설계 단계). **다음 = 사용자 spec 리뷰 → `writing-plans` → 구현.**
> - **검증 분석**: `docs/superpowers/analysis/2026-05-31-m16-regulation-core-verified.md` (29 bucket-A fact / 7 bucket-B; raw disasm = ground-truth, v001·재구성본은 hypothesis). **v001 5건 정정**: 스케일링 **×6**(>>6 아님 — 자기일관: >>6은 1000 clip 미도달), **단발 SW re-arm**(free-run 아님), Timer1 **0xFFB1**, 램프 **10단**, compare **`<`**. 파이프라인: 2ch 단발 ADC(ch0÷10/ch1÷50) → clip/floor/×6 → 21엔트리 내림차순 lookup → 패턴. 메모리 `project_m16_regulation_verified`.
> - **spec 초안 (리뷰 대기)**: `docs/superpowers/specs/2026-05-31-stage-d-slice1-regulation-core-design.md`. compute 파이프라인(신규 `app_reg.c`+`adc1.c`)+`app_lcd_measure()` 라이브화+board.c 안전수정만; OSC 구동 **DEFERRED**. 결정: DP1 슈퍼루프 폴링 ADC / DP2 raw+측정보정(10-bit@2.56V vs 12-bit@3.3V 도메인, `>>2` 1차안) / DP3 spec→리뷰→plan→구현.
> - **B-SEAM (measure-first, 사용자 결정)**: OSC 물리 구동경로 + PB12/PB13(OSC2/3 ← PC4/PA7, 이미지상 입력) 방향 = 이미지 부재 → 벤치 스윕 후 출력단 확정. compute는 이것 없이 완결·테스트 가능. board.c는 확정 3채널(PB2/PB10/PB14)만 idle-HIGH.
> - 미커밋: 분석 doc + spec + changelog/RESUME (working tree). `ref/atmega16/M16_reverse/` 여전히 untracked(분석 ground-truth 소스 — 커밋 여부 사용자 결정).
> - **이전 상태 (2026-05-31 a, LCD 마감)**: ↓ 아래 블록.

> **상태 (2026-05-31 a, 마감)**: STD comm 표시결함 **해결 완료 — root = DGUS 에셋(page 27/23 위젯 auto-load 부재), firmware 아님**(교차테스트+사용자 에셋fix로 입증). 에셋fix `0eafe68` + firmware A/B/C/D `a0a631c` 적용·커밋. **§4 전체 HW 재검증 PASS**(트레이스+머지 PR 바이너리 양쪽: 진입 표시·토글 0x140B·DHCP 영속·Fix A sentinel·Fix B IP무손상). **cpp-reviewer APPROVED**(차단 0). 브랜치 **`feat/stage-lcd-full-behavior`** = **통합 준비 완료**(머지/PR + 태그 `hw-revA_fw-stage-lcd`). **다음 = STEP 1 Stage D slice 1.**
> - **Fix 처분(확정)**: A/B/D 유지(독립 유효), **C 유지**(에셋 root fix로 redundant지만 사용자 결정으로 defense-in-depth 보존 — cpp-reviewer가 idempotent·무플리커·`temp-1` 언더플로우가드 확인). 진단 트레이스 `LCD_TRACE_RX` **유지**(머지 컴파일아웃 검증), `fw/build-trace/` gitignore.
> - **§4 검증 로그 핵심**: 토글 `0x140B data=0/1/2 → page=23/27/27 tcm=0/1/2`(serial=STDC23, eth·dhcp=STDE27 = 패널 의도 2-레이아웃; 지난 세션 `page=9` 이탈 **재현 안 됨**). DHCP 저장→`boot cm=2 ip=192.168.1.128`. 머지 PR 바이너리 시각 sanity OK. 상세: `…/analysis/2026-05-31-std-comm-page27-display-port-faithful.md`, 핸드오프 `…/2026-05-31-HANDOFF-lcd-comm-display-resolved.md`.

> **이전 상태 (2026-05-27 d)**: LCD 포팅(T1~T9) + RX/data16 버그 2건 + T10 검증 + 버그A 수정 + 버그B WIP. tip `99d3502`.
> - **이전 세션 fix(검증완료)**: 버그1 USART1 RX wedge→DMA circular(82cdb4c), 버그2 data16 off-by-one(d6c681f). 상세 `…/analysis/2026-05-27-lcd-hw-verify-rx-wedge.md`.
> - **이번 세션 — T10 인터랙티브 HW 검증(사용자와)**: ① 부팅무회귀 ② SAVE→전원사이클→영속 ③ **CANCEL 복귀 PASS** ④ **전 페이지 네비 무락업/무루프 PASS** ⑥ **HAND저장→MULTI복귀 = 의도된 UX 확인 PASS** ⑦ **NM/GW 선택 시 IP 마지막옥텟 시드 = samd20 퀴크 확인 PASS** ⑧ N/A ⑨ 무wedge ⑩ 훅. (③④⑥⑦이 이번 세션 신규 PASS.)
> - **이번 세션 버그A (수정·HW검증·커밋 `c6c89ef`)**: SETUP_MODEL(0x1084) 롱프레스 진입 불가. 패널이 down/up 모두 data=0 전송(release=2 없음) → verbatim FSM 미발화. 수정 = `long_press_released(vp,data16)` 연속 두 data=0 페어링(2초 가드 유지). 2초홀드→진입/탭→미진입 PASS. finding `…/analysis/2026-05-27-lcd-setup-model-longpress.md`.
> - **이번 세션 버그B (WIP 커밋 `99d3502`, 표시 미수정)**: 사용자 요청 "STD도 MULTI처럼 ether/comm_mode 영속". **영속은 완료·검증**(STD `data_save_commit`에 `commit_comm_mode_and_ether` 추가 → `boot cm=2` DHCP 영속). 그러나 **comm_mode 표시 버그**(DHCP저장+전원사이클→STDE에 serial+dhcp 동시체크) + **0xFF 커밋 손상 위험** 미수정. 상세+수정안 A~D: `…/analysis/2026-05-27-std-ether-comm-mode-persist-display.md`.
> **다음 작업**: ✅ LCD 브랜치 마감 완료(핸드오프 §1~§5 처리, §4 HW 재검증 PASS, cpp-reviewer APPROVED). **남은 것 = main 통합(머지/PR) + 태그 `hw-revA_fw-stage-lcd` → STEP 1 Stage D slice 1.**

---

## 새 세션 First Step

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
git checkout feat/stage-c-modbus-core-rtu      # Modbus slice 1 (코드 완결, HW E2E 대기, 16커밋 tip 083f053)
git log --oneline 5463370..HEAD                # 5463370(plan) 위로 코어/전송층/FSM/통합 + 리뷰 fix
cd fw && env -u STM32_TOOLCHAIN cmake --build build   # 0-warning (FLASH 31.1%/RAM 12.3%)
make -C test test                              # 호스트 = app_reg_calc + app_modbus_core 둘 다 PASSED
# 다음 갈래 (세 택1, 상단 상태블록 참조):
#  ① 보드 있으면 → slice 2b HW 재검증(HANDOFF §Resume) → 머지 체인 2b→m1→본 브랜치 → Modbus HW E2E(plan §HW-gated, mbpoll)
#  ② HW 없이 계속 → Stage C slice 2 (Modbus TCP/W5500) brainstorming부터 (코어에 mode!=RTU 스킵 경로 이미 준비됨)
# plan: docs/superpowers/plans/2026-06-12-stage-c-modbus-slice1-core-rtu.md (§HW-gated = mbpoll 매트릭스)
# spec: docs/superpowers/specs/2026-06-12-stage-c-modbus-slice1-core-rtu-design.md
# (옛 Stage D slice 1 feat/stage-d-regulation-core = MERGED 5aea06f; 옛 pin-I/O = RETIRED)
```

> 진단 가시화가 필요하면 `-DLCD_TRACE_RX` 트레이스 빌드(별도 `build-trace/`, CMakeLists에 한 줄 추가→빌드→원복→`build-trace/...elf` 플래시): `[lcd] rx vp=.. data=..` + `[lcd] commit cm temp=.. cfg=..` + `[lcd] boot cm=.. ip=..` 출력. 시리얼 리셋 글리치로 부팅줄 NULL 플러드가 섞이니 `tr -d '\000' < log | tr -s ' ' | grep '\['`로 정리해 읽기.

USART6 mon(@115200) = `/dev/cu.usbserial-BG02DMWU`. 단일-fd 캡처:
`{ stty -f /dev/cu.usbserial-BG02DMWU 115200 cs8 -parenb -cstopb raw -echo; cat; } < /dev/cu.usbserial-BG02DMWU > /tmp/lcd-mon.log &`

### ▶ STEP 0 — ✅ 완료 (2026-05-31): 버그B comm_mode 표시 = DGUS 에셋 root, A/B/C/D + 에셋fix, §4 HW PASS, cpp-reviewer APPROVED. 아래는 진행 기록.

**T10 ③④⑥⑦ 이번 세션 PASS, ①②⑧⑨⑩ 기존 PASS, 버그A(롱프레스) 수정·PASS·커밋.** 남은 단 하나 = **버그B comm_mode 표시 + 0xFF 손상가드**.

**0-a) (권장) samd20 comm-섀도우 로드 방식 대조** — `temp_comm_mode` 0xFF sentinel이 포팅 발명인지 samd20 패턴인지 확인 후 수정 방향 확정.

**0-b) coherent 수정 A~D** (상세: `…/analysis/2026-05-27-std-ether-comm-mode-persist-display.md`):
- **B(우선, 손상방지)**: `commit_comm_mode_and_ether`에서 `temp_comm_mode==0xFF`면 comm_mode/ether 커밋 스킵. ← 이미 작업1이 STD에 commit 호출 추가했으므로 가드 없으면 손상 위험 실재.
- **A**: 부팅 시 `temp_comm_mode=0xFF` 초기화(`app_lcd_init_mode` 등) — 첫 comm 페이지 진입이 cfg에서 seed.
- **C**: A로 기존 `==0xff` seed 게이트 자동 정상화.
- **D**: render serial 분기(`render.c:198`)에서 `DISP_EN_DHCP=0` 명시 기록(stale dhcp 제거).
- 빌드 0-warning, **트레이스 빌드로 HW 재검증**: DHCP 저장→전원사이클→STDE에 ethernet+dhcp만(serial 미체크), serial 저장→전원사이클→serial만.

**0-c) ⑤ 재검증**: 이제 STD ether/comm_mode가 (의도적으로) 영속됨 = 원 samd20 "STD 미영속 퀴크"는 **사용자 결정으로 폐기**(MULTI와 동일하게 영속). spec §보존퀴크 목록의 ⑤ 항목은 "영속으로 변경" 반영 필요.

**0-d) 진단 스캐폴딩 결정**: `LCD_TRACE_RX` 게이트 `boot cm`/`commit cm` 유지(유용) or 제거.

통과 → changelog/RESUME/NEXT_STEPS "done" + **전체 브랜치 final 코드리뷰 1회**(cpp-reviewer) → `gh pr create`(→main) + 태그 `hw-revA_fw-stage-lcd`.

> 구조/계약: 포팅 spec `…/specs/2026-05-27-stage-lcd-full-behavior-port-design.md`(F5 가정 = 틀림, finding 2 참조), RX fix spec `…/specs/2026-05-27-usart1-dma-rx-hardening-design.md`. 신규 소스 = `fw/src/app_lcd_{str,render,input,disp}.c` + `app_lcd.c`. config 소유 = `app_lcd_cfg()`, transient = `app_lcd_state()`, 측정 = `app_lcd_measure()`(스텁). RX = `fw/drivers/usart1.c`(DMA circular).

---

### ▶ STEP 1 — Stage D slice 1: 레귤레이션 코어 (spec-first, 측정 게이트 해제됨)

> **진행 (2026-06-01)**: ✅ 분석 → ✅ spec → ✅ plan(`writing-plans`) → ✅ **구현 완료**(Task 1~5, 6 커밋, 빌드 0-warning, 호스트 테스트 PASS, cpp-reviewer APPROVED). **현재 = Task 6 실보드 HW 검증 대기**(REG_TRACE, 사용자+보드) → 통과 시 머지/PR + 태그. 상세 = 최상단 상태 블록 + plan 문서. 아래는 원 slice 정의(구현됨, 참조용).

LCD 머지 뒤 재개. **slice 1 spec 작성** (`docs/superpowers/specs/`) → 사용자 리뷰 → `writing-plans` → 구현. Stage D는 LCD의 `lcd_measure_t` provider(측정값) + 스텁 훅 실체(us_command/set_pot 등)를 채우는 형태로 자연 통합됨. (brainstorming 완료 — 프레이밍/슬라이스 확정.)

**슬라이스 1 = 레귤레이션 코어** (M16 §4 제어루프 심장부; 알고리즘은 펌웨어에서 포팅, 물리 의미는 HW-verify 주석):
1. **ADC 획득층**: 2ch free-run, ch0 = 10샘플 평균(PA0 = 출력세기 피드백), ch1 = 50샘플 평균(PA1). 근거 `196c`, ADCSRA=0xCF(/128) ch0/ch1 교대.
2. **~2ms cadence** (M16 Timer0 등가; superloop+SysTick/TIM 위): `adc_ch0_avg` → `lookup_table` 스케일 → scaled value(0~1000 clip) → `output_level_process` 비교 → 누진 드라이브 패턴 `0x01→0x03→0x07→…→0xFF`.
3. **출력**: 누진 레벨 → OSC 드라이브 출력 라인 (V30 OSC_OUT0..4 = 전부 출력; 펌웨어의 M16 mixed in/out 무시 — 함수 관점은 "레벨을 OSC보드로 출력").

**다음 슬라이스(2+)**: 명령 상태머신(내부 us_start/seek/reset) + overload 검출/플래그 + soft-start 램프(main_loop 임계 0x29..0x191) + blink phase.

**flag (코딩 차단 ✗, 주석만)**: lookup_table 물리 의미(전류한계/주파수추종? = B3), ADC ch1(PA1) 정체(B4), OSC 출력 활성극성(Q8).

**검증된 사실 (slice 1 spec 재사용 — 이번 세션 도출)**:
- 제어루프 개요 = `docs/superpowers/analysis/atmega16-io-behavior.md §4`.
- `lookup_table[24]` 값(0x03FF→0x0004 감소곡선) = `ref/atmega16/m16_conv_v001.c:127-132`.
- Timer0 OSC 미러 극성 = **active-LOW / idle-HIGH** (플래그 set → 핀 LOW; disasm `01:256-276`). **v001이 맞고, `M16_reverse/out/04_reconstruction.c:175-177`은 극성 반전 오류.** 출력 초기화는 idle=HIGH (부팅 시 OSC off; 현 `board.c`는 LOW=asserted = 잠재버그).
- 디컴파일 함수명(g_run_flag/"LED"/"blink") 무시, 레지스터 사실만.
- pinmap 부록 A IPC표의 PB13/PB12 = **SAMD20 핀**(폐지된 옛 신호), F410 핀 아님 — F410 충돌 없음.

그다음 **`docs/NEXT_STEPS.md`** 읽기 (§5.7 BOOT0 이슈).

---

## ✅ 보드 이슈 — BOOT0 해결됨 (2026-05-26)

**BOOT0(U2.60)를 GND로 묶음 → 해결.** 이제 플래시한 앱이 평범한 `reset run`으로 직접 부팅. HW 검증(2026-05-26): `reset halt` 후 **PC=0x080045c0 / MSP=0x20008000**(플래시 Reset_Handler), `reset run` → mon USART6에 `[boot] gds_us_ctrl stage-b ready` / `[cfg] …` / `[t=N ms] hello uptime=N`(1초 cadence 증가) 정상. **gdb force-jump 워크어라운드 불필요.**

- **이전 문제(해결됨)**: R64(BOOT0 풀다운, U2.60) 미실장 → BOOT0 floating high → 매 리셋 시 ST 부트로더(PC 0x1FFFxxxx)로 부팅, 앱 미실행.
- **다른 미개조 보드에서 재발 시 fallback** (memory `project_board_boot0_workaround`): `openocd … program … verify` 후 gdb `monitor reset halt` → `set $sp=*(unsigned*)0x08000000` → `set $pc=*(unsigned*)0x08000004` → `set *(unsigned*)0xE000ED08=0x08000000`(VTOR) → `monitor resume; detach`. 상태 읽기는 두 번째 세션 `monitor halt`(reset ✗) 후 `print`.
- **시리얼 (USART6 mon, /dev/cu.usbserial-*, 115200)**: 단일 fd — `{ stty 115200 cs8 -parenb -cstopb raw -echo; cat; } < /dev/cu.usbserial-XXXX > log`.

---

## Stage B 결과 요약 (머지 완료)

- 계층: `i2c1`(PB6/PB7 AF4 400kHz) → `fram`(FM24C16B @0x50, 1-byte, big-endian) → `app_config`(0xAA init-flag, β factory map) → `app_lcd`(`init_lcd_mode` 포팅, scope a).
- HW 검증: **FRAM I2C err=0**, **factory-write/load 2-boot determinism 동일값**, mon `[cfg]` 정상, **육안 "GDS-15H" hand 페이지 확인**.
- delay/weld/hold(`LV_DM_*`)는 SETUP 페이지 VP라 RUN_HAND 화면엔 미표시(정상, samd20 동일).
- Codex가 plan 실행 (7 task commits). deviation: `syscalls.c` 추가(+`-lnosys` 제거, 0-warning 게이트 위해) — 정당·문서화됨.
- 미적용(선택): `_sbrk` heap/stack 가드 (LOW, 현재 `%f` 미사용으로 dead).

상세: spec `docs/superpowers/specs/2026-05-25-stage-b-lcd-app-design.md`, plan `docs/superpowers/plans/2026-05-25-stage-b-lcd-app.md`.

---

## ATmega16 분석 결과 + 보드-진실 정정 (`analysis §0.1`이 본문 §0/§1/§3/§4/§7보다 우선)

**확정 (보드 직독 / 사용자):**
- 아키텍처: ATmega16 = 트리거+출력세기 모니터링(실시간 레귤레이션), SAMD20 = UI+전체제어, OSC = 별도 보드.
- `CTRL_OSC0..4` ← mega16 **PB1/PB0/PC4/PA7/PC7** (V30 PB2/PB10/PB12/PB13/PB14). ← OSC 트리거 핀 미해소 해소.
- **PA0(ADC ch0) = 출력 초음파 세기 피드백** = 레귤레이션 입력 (V30 PB0/ADC1_IN8 `SENS_OUT`).
- IPC는 OSC 핀과 별개: 명령선 M_START/SEEK/RESET(PA4/5/6) + M_OVLD(PC0).

**뒤집힌 본문 결론 (이제 무시):** "IPC=PB0/PB1/PA7/PC4/PC7"(§1), "PB0/PB1/PC7=상태표시"(§3), "누진패턴→dead display로만 감"(§4). 디컴파일 **함수명·동작주석은 무시**, 동작은 사용자가 재도출.

**측정 대기 (Stage D 차단 — `analysis §0.1` B1~B5):**
- B1 5 OSC핀 각 방향 (출력 vs 입력 — PC4·PA7는 코드상 입력이라 순수 5출력 아닐 수 있음)
- B2 레벨↔OSC 비트매핑 + 극성(Active H/L)
- B3 레귤레이션 전달함수 (PA0 피드백 → scaled → 레벨; 디컴파일 스케일 수식 placeholder)
- B4 ADC ch1(PA1, 50샘플) 정체
- B5 `g_main_state` 0/1 두 출력모드 (lookup 레귤레이션 vs 본문부재 `output_direct_process`)

## 다음 세션 진입

1. **사용자 측정 결과 + 행동 재분석 수령** → `analysis §0.1` B1~B5 채움.
2. **Stage D brainstorming 재개** — spec 작성. 결정 완료분: 슬라이스 1개 유지(피드백 루프, 모듈 경계로만 분할), 코드-포팅 먼저+머지前 실측(사용자 결정), 안전경계는 측정 후 재논의(원래 arm-gate/idle-safe 옵션 보류).
3. **Stage D 구조부(측정 무관, 선포팅 가능)**: ADC 획득층(2ch 평균 10/50) + TIM 2ms/10ms cadence(Timer0/1 등가, superloop+SysTick 위) + 상태머신 골격 + **출력드라이버 추상화**(5비트 레벨 → CTRL_OSC, 매핑/극성은 named const로 격리). 전달함수·핀방향은 B1~B5 대기.
4. **비포팅**: 7-seg/`display_*`(dead), comm IPC(single MCU).

> ⚠️ "ref/atmega16 verbatim 포팅" 전제 **폐기** — 디컴파일은 약한 참조. 레귤레이션 동작은 사용자 측정/재분석이 권위.

## 그 외 대기 슬라이스

- **Stage C — Modbus RTU on USART6** (속도/패리티 = FRAM `comm_speed_idx`/`comm_parity_idx`, Stage B 로드됨) — Stage D와 독립
- **Stage A I/O** — CON_OVLD / CON_START / CTRL_OSC0~4 GPIO (Stage D와 일부 중첩)
- **scope-b LCD 확장** — `change_lcd_page` + SETUP 페이지 (DGUS VP맵 `hw/lcd/dgus/README.md`)

코드 슬라이스 정석 흐름: worktree → brainstorming → spec → plan → subagent/Codex 실행 → HW 검증 → 머지.

---

## 환경 / 누적 이슈

- `$STM32_TOOLCHAIN` stale → `env -u STM32_TOOLCHAIN cmake ...` 필수
- 빌드 게이트: `cmake --build` 0-warning (`-Wall -Wextra -Wundef -Wshadow`). clangd LSP 노이즈 무시
- ST-LINK V3J15M7B5S1 (API v3), Cortex-M4 r0p1 (HW BP ≤6)
- I2C1 PB6/PB7 AF4, 외부 풀업 10k→VCC_5 (5V, FT 핀), FRAM 0x50
- graphify: **사용 중단(2026-06-10, 사용자 결정)** — 산출물/메모리 정책 삭제, 재생성 ✗
- 회로도/netlist: `hw/schematics/USW_CTRL_V30.{pdf,asc}`, DGUS: `hw/lcd/dgus/` (페이지↔매크로 맵 README)
- 응답 언어: 한국어 (코드/commit/식별자 영어)

---

## 참조

- 다음 단계: `docs/NEXT_STEPS.md` (§5.7 BOOT0)
- Stage B archive(kickoff): `docs/superpowers/historical/2026-05-25-RESUME-stageB-kickoff.md`
- Stage A archive: `docs/superpowers/historical/2026-05-06-RESUME.md`
- 변경 이력: `docs/changelog.md`
- samd20 ref (포팅 source, 수정 ✗): `ref/samd20/`
