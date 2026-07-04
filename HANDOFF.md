# Handoff: D5 reconcile 완료 — 3브랜치 main 위 스택 재구축 (머지/태그 = HW 검증 게이트)

**Generated**: 2026-07-04 (d 세션 마감)
**Branch**: `main` (docs 커밋 후 tip; origin ahead — push 미실행)
**Status**: Ready — **2026-07-02 감사 결정 큐 D0~D6 전항목 종결.** 다음 = D5 스택 HW 검증→단위별 머지/태그, 또는 HW-gated 백로그(weld slice4/B-SEAM/6b/overload)

> **요약**: 미머지 3단위(`feat/physical-io-slice-b`→`feat/physical-io-slice-d`→`feat/output-power-graph-ch1`)를
> 현 main 위 **선형 스택으로 in-place rebase** 완료(cherry-pick 시퀀스). `app_reg_tick`
> 4-way 시그니처를 `reg_run_limits_t` 7필드로 통합, ceiling 이중화(d)×ovtime(main)
> semantic 병합, board.c=slice-d 최종판(byte-identical 검증). 단계별 0-warning +
> host 8/12/12 PASS + cpp-review 2회 게이트 통과. **머지/태그/push 없음** — HW 검증 후.

## Goal

STM32F410RBT 단일 MCU 통합 펌웨어. 이번 세션 몫 = D5 reconcile (코딩 세션, 보드 불필요):
미머지 3브랜치를 현 main에 재정렬해 이후 HW 세션이 단위별 검증→머지→태그를 그대로
수행할 수 있게 함.

## Completed

- [x] **spec** `docs/superpowers/specs/2026-07-04-d5-reconcile-design.md` (A안=순차 스택 rebase; §5.2 ceiling 통합 표가 정본 — 작성 중 실측으로 "TOUCH 제외" 정정)
- [x] **plan** `docs/superpowers/plans/2026-07-04-d5-reconcile.md` (7 Task, 인라인 실행)
- [x] **b'** = `feat/physical-io-slice-b` → main+4커밋 (FREQ_IN 측정; struct에 `freq_cal_val`; host 8스위트)
- [x] **d'** = `feat/physical-io-slice-d` → b'+28커밋 (a⊂c⊂d superset; `model_type`+ceiling 통합+guard 5-break+STATUS union+board.c d판; host 12스위트)
- [x] **ch1'** = `feat/output-power-graph-ch1` → d'+5커밋 (표시 ch1 분리; `cal_val`로 7필드 완성; host 12스위트)
- [x] 리뷰 게이트: d' `app_reg.c` 병합 cpp-review(semantic 정본 일치; IMPORTANT 1건=`board_osc4` hook은 원본 slice-d 코드로 판명—무수정) / ch1' cpp-review **APPROVE**(0 Crit/Imp)
- [x] 등가 검증: board.c/h + b/d 고유 신규 파일 = 원본 tip과 **byte-identical**; 스택 선형성(merge-base 체인) 확인
- [x] docs: changelog·NEXT_STEPS(§1.3 D5 ✅·§2.2)·RESUME(d 세션 블록)·메모리

## Not Yet Done

- [ ] **D5 스택 HW 검증 → 단위별 머지/태그** (다음 HW 세션): 순서 b'→d'→ch1'.
  - b' = FREQ_IN rig (TIM5_CH1 입력캡처 실신호)
  - d' = 패널 rig (물리 B_START/RESET/SEEK/E-stop + overload + OSC 부팅 초기화 + buzzer/SOL/USOUT)
  - ch1' = 실 초음파 부하 (표시 전류/전력 = ch1 실측; **간이 벤치에선 무신호=0**)
  - 간이 벤치에서 가능한 회귀: 직접런 ceiling(mbpoll COMM ~560ms, hand 모드) + STATUS 비트 + LCD 무회귀
  - **⚠ 머지 노트 필수 명기(ch1 spec 조건)**: 에너지 적분 입력 ch1 이동 = weld 에너지 종료/OVTIME 판정 입력도 ch1 (의도된 교차영향)
- [ ] 나머지 HW-gated 백로그: weld slice4(D2/D4 흡수) · B-SEAM OSC 물리 구동 · 6b calibration · overload 실동작. H4+IWDG 별도 슬라이스, M6/M8/M9=HMI 트리거 (`docs/NEXT_STEPS.md` §1.2/§1.3).
- [ ] push (사용자 SSH — main ahead origin 수커밋 + 재구축 브랜치 3개)

## ⚠ 의도된 거동 변화 (HW 회귀 시나리오 갱신 필수)

1. **TOUCH(LCD RUN) 비-energy 런은 `limit_on_time` ceiling 미적용** — 30s 안전 ceiling만.
   slice-d의 2026-06-27 승인 결정(samd20 main.c:5296 충실: 운영 ceiling=hand의 COMM/REMOTE만)
   채택. 기존 "LCD RUN + ON_TIME=100 → 1s 깜빡" 류 벤치 시나리오 무효. mbpoll COMM
   ceiling ~560ms는 hand(model_type=0)에서 그대로 유지.
2. **OVTIME(`limit_out_time`) > 30s 설정 시 30s 안전 ceiling이 먼저 발화** (d 결정 "unconditional").

## Failed Approaches (Don't Repeat These)

1. **test 파일 union을 기계적 regex(HEAD블록+incoming블록 연결)로 처리 → 함수 닫는 `}`가
   충돌 영역에 걸쳐 있어 중첩 함수 정의로 컴파일 실패** (`app_reg_calc.c`/`test_app_reg_calc.c`).
   해소: 병합 후 반드시 즉시 컴파일 확인, 경계 걸친 hunk는 수동으로. 나머지 union은 전부 정상.
2. cherry-pick 중 `git rebase -i`는 이 환경에서 미지원 — **cherry-pick 시퀀스가 정답**이었음.
   충돌 해소는 원 커밋에 폴드(`--continue`)해 이력 클린 유지.

## Key Decisions

| Decision | Rationale |
|----------|-----------|
| A안: 순차 스택 rebase (b→d→ch1, in-place + backup ref) | 시그니처 통합 1회, ch1은 d 없이 HW 검증 불가, 단위 경계=브랜치 ref로 기존 머지/태그 정책 호환 |
| stale handoff/resume docs 커밋 15개 drop | 전부 docs-only 실측 — docs 충돌 소멸, main의 HANDOFF/RESUME가 정본 |
| ceiling: slice-d의 TOUCH 제외 채택 | d가 나중의 의도적 결정(사용자 승인+HW 입증); V30 lost-release 리스크는 30s 안전이 커버 |
| energy 종료 소스에 REMOTE 추가 | d가 REMOTE 슬라이스 — COMM과 동급 직접런 취급 (main의 TOUCH/COMM에 확장) |
| helper = `reg_stop_run` 단일 (d의 `reg_run_stop_latch` 개명) | 본문 동일 — main 기준 명칭 유지, last_freq 래치 포함 |
| `board_osc4` in run hook = 유지 | 리뷰 IMPORTANT였으나 원본 slice-d 코드 그대로(8006e9c) — 물리 출력이 살아있는 것 자체가 d 머지가 HW-gated인 이유 |
| comment rot 4건 defer | 원본 slice-d에 동일하게 존재(pre-existing) — reconcile 순수성 유지, 머지 시 정리 후보 |

## Current State

**브랜치 토폴로지** (전부 로컬):

```
main (docs tip)
  └─ feat/physical-io-slice-b   (+4:  FREQ_IN)          ← backup/pre-d5-slice-b (구 tip)
       └─ feat/physical-io-slice-d  (+28: a+c+d)        ← backup/pre-d5-slice-d
            └─ feat/output-power-graph-ch1 (+5: ch1)    ← backup/pre-d5-ch1
```

- 최종 tip(ch1') 빌드: our-code 0-warning, FLASH 46.34% / RAM 17.29%, host 12스위트 PASS.
- `reg_run_limits_t` 최종 7필드: limit_on_time / energy_ctrl / limit_energy / limit_out_time / freq_cal_val / model_type / cal_val.
- **Board**: 이전 세션 그대로(M7 머지 코드, SERIAL/addr=1/9600/EVEN, OUT_POWER=56, FRAM ether_ip=.199) — 이번 세션 보드 무접촉.
- **Uncommitted**: 없음 (untracked `ref/signal/`만 — 무관, 불가촉).

## Files to Know

| File | Why It Matters |
|------|----------------|
| `docs/superpowers/specs/2026-07-04-d5-reconcile-design.md` | 통합 정본 — 특히 §5.2 ceiling 표 + 거동 변화 2건 |
| `fw/src/app_reg.c` (ch1' tip) | 통합의 심장: 30s 안전+energy/legacy 블록, guard 5-break, publish(ch1+freq+usout) |
| `fw/include/app_reg.h` | `reg_run_limits_t` 7필드 최종형 |
| `fw/src/app.c` | superloop 순서 + lim 7필드 주입부 |
| `fw/src/board.c` | slice-d 최종판 (OD 3채널 + board_osc4/reset/seek) |
| `backup/pre-d5-*` | 원 tip 3개 — 문제 시 즉시 복원/대조용 |

## Resume Instructions

1. sanity: `git log --oneline -3`(main) + `git branch --list 'backup/*' 'feat/*'` + ch1' tip에서 `env -u STM32_TOOLCHAIN cmake -S fw -B fw/build -G Ninja && cmake --build fw/build` + `make -C fw/test`(12스위트).
2. **HW 세션이면**: b'부터 플래시(브랜치 전환마다 reconfigure — GLOB 함정). 검증 규칙 = mbpoll/LCD만, SWD halt 금지. **⚠ 거동 변화 2건 반영해 시나리오 작성** (위 섹션). 단위 PASS마다 `--no-ff` 머지+`hw-revA_fw-stage-*` 태그, ch1 머지 커밋에 에너지 교차영향 명기.
3. 간이 벤치(패널 미연결)면: d'의 물리 입력/OSC는 검증 불가 — ceiling/STATUS/LCD 회귀만 하고 풀 rig 세션으로 이연 판단.
4. 코딩 세션이면: weld slice4(D2/D4 흡수) 착수가 자연스러움 — spec부터.

## Warnings

- ⚠ **재구축 3브랜치는 히스토리가 교체됨** — 구 tip 기준 로컬 작업물이 있다면 backup ref와 대조. origin에는 이 브랜치들 미push.
- ⚠ **거동 변화 2건** (위 섹션) — HW 회귀 시나리오 갱신 없이 옛 시나리오로 검증하면 오판.
- ⚠ d' 파일 헤더 comment rot 4건 defer 중("drives NO OSC GPIO" 등 — 실제론 board_osc4 구동): 머지 시 정리 후보.
- ⚠ `reg_power_from_amp` uint16 절단(비정상 대형 cal_val) = samd20 fidelity 특성 — 6b 재검토.
- ⚠ vendor wiznet `socket.h` 경고 3건 = pre-existing, our-code 0-warning 판정과 무관.
- ⚠ 보드 FRAM ether_ip=.199 / 시험 네트워크 IP 충돌 잦음(ARP MAC 00:08:dc:78:91:71로 판별) — ETH 시험 시.
