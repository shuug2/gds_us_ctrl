# Handoff: weld 슬라이스4 CODE-COMPLETE (감사 D2/D4 종결) — 다음 = 스택 HW 검증 → 순서 머지

**Generated**: 2026-07-04 (f 세션 마감)
**Branch**: `feat/stage-weld-slice4-trigger` (tip `1cb76bd`, ch1' 스택 위, **미머지·push 미실행**)
**Status**: CODE-COMPLETE — 최종 opus whole-branch cpp-review 승인(0 Critical/0 Important 잔존), Ready-to-merge YES. **머지/태그 = 스택 b'→d'→ch1'→slice4 전체 HW 게이트 뒤.**

> **요약**: weld 슬라이스4(TRIGGER 모드+양손 트리거+안전 abort+사이클 진입 게이팅+감사 D2 클램프 M1~M4+D4/H1 래치)를
> subagent-driven으로 10 Task 전부 실행. Task별 2-stage 리뷰 전건 Approved.
> 최종 리뷰가 **I-1**(output_power FRAM 로드 미클램프 — 진폭 언더플로 경로) 1건을 추가 발견 → fix `1cb76bd` + 재리뷰 Resolved.
> 게이트 GREEN: 클린 reconfigure 빌드 our-code 0-warning FLASH 47.33%/RAM 17.31% + host **13스위트**(기존 12+신규 app_weld_trigger_fsm) PASS.

## Goal

samd20 공압 프레스 사이클의 물리 계층(양손 트리거/TRIGGER 모드/안전 abort) + 감사 D2/D4 수정을 weld 스테이지에 흡수. spec = `docs/superpowers/specs/2026-07-04-stage-weld-cycle-slice4-trigger-design.md`, plan = `docs/superpowers/plans/2026-07-04-stage-weld-cycle-slice4-trigger.md` (10 Task).

## Completed (코드 12커밋, BASE `e0bc491`)

- [x] `4541ef4` **H1/D4**: WELD 모드 래치(CYL1→WELD 전이 시점 단일 스냅샷 `s_latched_multi/energy`) + 전이 카운터 리셋 — 런중 EN_MULTI/EN_ENERGY 토글 무력화. host 3테스트.
- [x] `a11d2b8` **abort 입력**: `weld_in_t.abort` — 임의 상태 SOL OFF+READY, WELD면 weld_stop 엣지, cycle_done 미발행. 상태별 host 테스트(step 카운트 {1,4,10,12} 실측 보정 — comp_time 7-tick quirk).
- [x] `57788aa`+`e31c69e` **TRIGGER 모드**: dn/up 엣지 래치, CYL1=SENSE_DN 무기한 대기(죽은 CYL_TIMEOUT 충실), WELD=trigger_time2/HOLD=trigger_time3, CYL2=legacy 강제 set(main.c:1593) 즉시 exit, run_mode 사이클 래치. `enter_weld()`/`hold_time()` helper로 DELAY/TRIGGER DRY — DELAY byte-equivalence 리뷰어 trace 입증.
- [x] `51fb9e2` **M1**: `limit_energy==0` = 에너지-도달 체크 off (reg_calc+weld FSM 두 소비자, OVTIME/backstop 유효 유지).
- [x] `608de22` **트리거 FSM 신설**(`app_weld_trigger_fsm`, 순수): 양손 start(레벨 파생)/in_cycle 재장전(`cycle_started()` 글루 위임=게이팅 실패 시 재장전 벌칙 없음)/safety abort(f_safty∧CYL1∧한손 release)/센서 press 엣지. legacy main.c:1219/1404-1407/1472/1484 대응 리뷰어 독립 대조. host 신규 스위트+Makefile 중복 3줄 정리.
- [x] `d035802` **진입 게이팅**: `app_reg_start_allowed()`(6-conjunct 읽기 전용) 신설 + START guard 4-break 공용화(equivalence 정적 입증, swallow 비대칭 보존). 직접런↔weld 상호배제 성립.
- [x] `0aa91ad`+`9e3198d` **글루 배선**: 물리 트리거 스캔→진입 게이팅→abort 합성(estop/overload/reg error/safety)→신필드 주입 + tick `+=` 드리프트 무누적(>10tick 재동기) + **SETUP 게이트**(`app_lcd_in_run_page()` 신설) + M2 mo_out [50,100]. `9e3198d` = SETUP 동결 거동 주석 정정(아래 인간 결정 ①).
- [x] `9792c5b` **M4**: `cfg_clamp.h` 신설 + LCD LV_* 10케이스 Modbus-동일 클램프+변경시 에코(전수 대조 drift 0) — 구 LV_OUT_POWER/LV_LIMIT_OUT_T `(uint8_t)` 무클램프 절단 버그(LOW-1) 폐쇄.
- [x] `17a07e3` **M3**: FRAM 로드 comm speed/parity idx OOB→factory(0), `CFG_COMM_*_IDX_MAX` 공용 매크로.
- [x] `1cb76bd` **I-1(최종 리뷰 발견→fix)**: output_power **FRAM 로드 경로만** 미클램프 — corrupt FRAM 0이면 `weld_amplitude`의 `op-50u` 언더플로로 임의 진폭(77). 로드 성공 시 `cfg_clamp_power` [50,100](RAM-only=M3 패턴)+host 테스트(0→50, 255→100). 재리뷰 Resolved.
- [x] docs: changelog(슬라이스4 항목)/NEXT_STEPS(§1.2 slice4·§1.3 D2/D4 ✅)/RESUME(f 블록)/HANDOFF(본 문서)/SDD ledger.

## 인간 결정 (이 세션, 2건)

| 결정 | 내용 |
|------|------|
| ① SETUP 게이트 ↔ abort 순서 | plan 순서(SETUP 게이트가 abort보다 앞 = setup 페이지 체류 중 weld FSM 완전 동결) **유지+문서화**. 리뷰어 실사로 구현자 rationale 오류 적발: **overload는 SOL을 해제하지 않음**(`io_sol_dn(false)` 호출자 = weld 훅+`app_input.c:46` E-stop뿐; app_overload는 US만 RUN_RELEASE). 잔존 = setup 체류∧overload∧mid-cycle 교집합에서 SOL 유지→run 복귀 시 레벨-abort 해제. legacy samd20도 SETUP에서 timer 전체 동결 = 충실. 주석 `9e3198d`. |
| ② I-1 fix | output_power FRAM 로드 클램프 추가 승인 (리뷰어 제안 형태 그대로). |

## Not Yet Done

- [ ] **스택 HW 검증 세션 (다음 작업)**: 패널 rig에서 spec §7.3 **8항목** + 추가 노트 2건:
  1. SETUP 체류 중 overload → SOL 유지, run 페이지 복귀 → 해제 (인간 결정 ① 거동 확인)
  2. LCD LV_* 클램프 에코 스팟체크 (예: DELAY에 600 입력 → 패널 500 스냅)
  - 검증 규칙 = **mbpoll/LCD 육안만 (SWD halt 금지)**, 브랜치 전환 시 **reconfigure 필수**(GLOB 함정).
- [ ] **머지+태그**: HW PASS 후 **b'→d'→ch1'→slice4** 순서, 단위별 `--no-ff`+태그 (backup=`backup/pre-d5-*`).
  - ⚠ **docs 충돌 예상**: 이 브랜치의 docs 갱신(changelog/NEXT_STEPS/RESUME/HANDOFF)은 D5-이전 base 위 — main의 d/e 세션 docs 커밋(`9afa4c3`/`5a677f0` 등)과 충돌 시 양쪽 이력 보존으로 해소(코드 충돌은 없음 — fw/는 스택 선형).
- [ ] Defer 백로그(차기 정리 커밋 후보): m-2/이월#8 `app_weld_request_start` dormant API+stale 주석 정리, 이월#6 test Makefile BIN_OL/IN/OSC 룰블록 중복, m-1 backstop fault 경로 보조 상태 리셋(또는 spec §3.1 문구 정정). 전체 목록 = `.superpowers/sdd/progress.md`.
- [ ] 기존 HW-gated: B-SEAM OSC 물리 구동 / 6b signal calibration(에너지 절대 E2E 포함) / overload 보호 슬라이스 / HMI-트리거(M6/M8/M9, H4+IWDG).

## Failed Approaches / 절차 노트

- 코드 차원 실패 없음 (전 Task 1회 리뷰 통과; Task 7만 Important 1건 → 인간 결정으로 해소).
- **구현자 rationale 오류 1건**(Task 7): "overload도 독립 SOL OFF" 주장 — opus 리뷰어 grep 실사로 반증. 안전 관련 주장은 리뷰어가 콜사이트 실사할 것.
- 최종 리뷰어 API 오류 2회 중단 → SendMessage 재개로 완주 (재개 시 검증 상태 유지됨).
- Task 4/7 리포트 파일이 이전 세션 stale 리포트와 이름 충돌 — 디렉토리가 gitignore라 무해, 덮어씀.

## Key Decisions (설계, spec 시점 + 실행 중 확정)

| Decision | Rationale |
|----------|-----------|
| 사이클 진입 게이팅 (의도된 deviation) | START가 거부될 상태면 사이클 자체 미시작 — SOL만 하강하는 블라인드 사이클 차단 (spec §4.3, 사용자 결정 2026-07-04 e) |
| A안 = 순수 트리거 FSM 분리 | check_remote_input()의 weld 몫을 host-testable 순수 모듈로 (13번째 스위트) |
| H1 래치 = CYL1→WELD 전이 시점 | 무장과 exit가 같은 스냅샷 — 1-tick 토글 창 제거 |
| CYL2 즉시 exit | legacy main.c:1593 강제 set 충실 (SENSE_UP 실대기 필요 판정 시 1줄 제거, spec §3.3) |
| limit_energy=0 = off | limit_out_time=0=OVTIME off와 동일 의미론 (M1) |
| in_cycle set = 글루 위임 | 게이팅에 막힌 트리거가 양손 재장전을 강요하지 않도록 (spec §2.3) |
| I-1 클램프 = RAM-only | M3 comm idx와 동일 패턴 (corrupt FRAM 바이트 write-back 안 함) |

## 상태 스냅샷

- 브랜치 스택: `main`(5a677f0) ← `feat/physical-io-slice-b'` ← `slice-d'` ← `feat/output-power-graph-ch1`(9c7cd4a) ← **`feat/stage-weld-slice4-trigger`(1cb76bd = spec 434f7f5 + plan e0bc491 + 코드 12 + docs)**
- 게이트: our-code 0-warning FLASH 47.33%/RAM 17.31%, host 13스위트 PASS, 최종 opus 리뷰 0 Crit/0 Imp.
- push 미실행. 보드 = 이전 세션 상태(SERIAL/addr=1/9600/EVEN, FRAM ether_ip=.199).
