# D5 reconcile 설계 — 미머지 3브랜치 스택 rebase (b→d→ch1)

> **요약**: 미머지 3단위(`feat/physical-io-slice-b` → `feat/physical-io-slice-d` → `feat/output-power-graph-ch1`)를
> 현 main(`18b87fe`) 위로 **순차 스택 in-place rebase**한다. `app_reg_tick`은 main의
> `reg_run_limits_t` struct 주입(ovtime 버전)을 기준으로 단계마다 필드를 추가해
> 4-way 시그니처(main/b/d/ch1)를 semantic 통합하고, `board.c`는 slice-d superset을
> 채택하며, stale handoff/resume docs 커밋 15개는 drop하여 docs 충돌을 소멸시킨다.
> 완료 기준 = 각 단계 tip에서 빌드 0-warning(our code) + host 스위트 PASS.
> **머지/태그/push는 범위 밖**(HW 검증 후 별도 세션, 기존 정책).

- **날짜**: 2026-07-04 (d 세션)
- **기준 main**: `18b87fe`
- **결정 근거**: 2026-07-02 전면 감사 D5 항목 (`docs/NEXT_STEPS.md` §1.3), HANDOFF.md Resume 지침 2

---

## 1. 배경과 목표

main은 감사 큐 D0~D6 종결로 크게 전진(weld3 → seekreset → i2c-pot → ovtime →
fram-robust → eth-reapply)했으나, 미머지 3브랜치는 전부 구 main(`36b0a069`,
2026-06-19경) 위에 있다. 3브랜치 모두 `app_reg.h/.c`·`app.c`를 건드려 서로 다른
`app_reg_tick` 시그니처를 갖고, slice-d는 `board.c`도 main과 갈라졌다(main이 OD
전기설정만 선행 승격). 이대로 방치하면 이후 어떤 순서로 머지해도 충돌이 반복·확대된다.

**목표**: 3단위를 현 main 위의 선형 스택으로 재구축하여, 이후 HW 검증 세션이
단위 경계별로 검증→머지→태그를 그대로 수행할 수 있게 한다.

**완료 기준**: 3브랜치 ref가 main 위 스택으로 이동 + 각 단계 tip에서
빌드 GREEN + host 스위트 PASS. 머지·태그·push 없음.

## 2. 전략 (승인: A안 순차 스택 rebase)

```
main (18b87fe)
  └─ feat/physical-io-slice-b'      (FREQ_IN 측정: +freq_cal_val)
       └─ feat/physical-io-slice-d' (물리 입력+E-stop, a⊂c⊂d superset: +model_type, board.c 채택)
            └─ feat/output-power-graph-ch1' (표시값 ch1 분리: +cal_val)
```

- **순서 b→d→ch1**: 감사 결정 그대로(독립·최고령 b 우선, ch1은 d 없이 HW 검증
  불가이므로 스택 최상단이 자연스러움).
- **in-place**: 브랜치 이름 유지(기존 docs/메모리 참조 연속성). rebase 전 원 tip을
  `backup/pre-d5-slice-b`, `backup/pre-d5-slice-d`, `backup/pre-d5-ch1` 브랜치로 보관.
- **기각 대안**: 독립 병렬 rebase(B — 시그니처 통합 3회 반복 + 이후 머지마다 충돌
  재발), 통합 merge 브랜치(C — 단위 경계 불명확, 단위별 태깅 정책과 불합치).

## 3. stale docs 커밋 drop (docs 충돌 소멸)

HANDOFF.md / `docs/superpowers/RESUME.md`를 다시 쓰는 세션-스냅샷 커밋은 rebase 시
제외한다. **15개 전부 docs-only임을 실측 확인**(코드 손실 없음). main의
HANDOFF/RESUME가 정본이므로 이 커밋들은 이력 가치만 있고 backup ref에 남는다.

| 브랜치 | drop (docs-only) | keep (코드 + spec/plan 문서) |
|---|---|---|
| slice-b | `a5d984f` | `44e1a36`(plan) `4cdcd79` `3f2f202` `2a22997` |
| slice-d | `0e2408b` `f3626d8` `2e44cf2` `f0de147` `248b370` `b207e01` `7483d77` `0c3497c` `56575c4` `df00e58` `9b341fc` `d70600c` | slice-a 6커밋(`0487551`..`104d3c8`) + slice-c 코드/주석 7커밋(`b0d6a52`..`0bf084c`, `20e0551` plan 포함) + slice-d 코드 5커밋(`c64c335`..`4f1c8ca`) + ceiling 이중화 `00835cb`+`81a44c0` + OSC boot-init `8006e9c` + spec/plan(`80985fc` `bece466` `d0d1f9d` `a040ad6` `b372da5` `b8bcaed`) |
| ch1 | `a7928e0` `27b6888` | `fefdc64`(spec) `be53d13`(plan) `116ed24` `1f43b8d` `f30bc51`(spec 노트) |

## 4. `app_reg_tick` 시그니처 semantic 통합 (핵심)

4-way 현황:

| 버전 | 시그니처 |
|---|---|
| main (기준, ovtime) | `void app_reg_tick(const reg_run_limits_t *lim)` |
| slice-b | `(uint16_t limit_on_time, int16_t freq_cal_val)` |
| slice-d | `(uint16_t limit_on_time, uint8_t model_type)` |
| ch1 | `(uint16_t limit_on_time, int16_t cal_val)` |

통합: **struct 주입 유지 + 단계별 필드 추가**. struct 이름 `reg_run_limits_t`는
유지(주석을 "run limits + per-call 주입 config"로 갱신 — 최소 변경). 세 필드는
서로 다른 `g_cfg` 필드에서 오므로 semantic 충돌 없음(`freq_cal_val`/`cal_val`은
merge-base 이전부터 `app_config.h`에 존재 — config/FRAM 레이아웃 변경 없음).

```c
typedef struct {
    uint16_t limit_on_time;   /* (main) x10 ms; 0 = ceiling off */
    uint8_t  energy_ctrl;     /* (main) 1 = energy 모드 */
    uint32_t limit_energy;    /* (main) 에너지-도달 정상정지 임계 */
    uint16_t limit_out_time;  /* (main) OVTIME 한계 초 */
    int16_t  freq_cal_val;    /* +b': FREQ_IN 보정 → freq_fsm_compute */
    uint8_t  model_type;      /* +d': 0=hand — legacy ceiling 게이트 */
    int16_t  cal_val;         /* +ch1': ch1 표시 전류 보정 */
} reg_run_limits_t;
```

`app.c` 호출부는 main의 지역 struct 채움 패턴 유지, 단계마다 필드 추가.

## 5. 단계별 충돌 해소 방침

### 5.1 b' (충돌 4파일: app_reg.h / app_reg.c / app.c / test/Makefile)

- `app_reg.h`: struct에 `freq_cal_val` 추가, tick 시그니처는 main 그대로.
- `app_reg.c`: `reg_publish_measure(now, freq_cal_val)` 인자 전달을 struct 경유로 각색
  (`lim->freq_cal_val`). `last_freq` 래치·`freq_fsm_compute` 호출은 b 그대로.
- `app.c`: main 호출부의 struct 초기화에 `.freq_cal_val = rc->freq_cal_val` 추가.
- `test/Makefile`: 스위트 union (+`test_app_freq_fsm`) — main 7 + 1 = 8.
- 신규 파일(freq_ic 드라이버, app_freq_fsm, irq/periph 배선)은 충돌 없음.

### 5.2 d' (충돌 6파일: 위 4 + app_modbus.c + board.c)

**ceiling semantic 통합** (main ovtime × d 이중화 — 이 rebase의 심장):

| 규칙 | 출처 | 통합 후 |
|---|---|---|
| 절대 30 s 안전 ceiling | d `00835cb` | **전모드·전소스 상시** (energy 여부 무관) |
| legacy `limit_on_time` ceiling | main(비-energy) × d(hand 전용) | **`model_type==0`(hand) && `energy_ctrl==0`** 에서만 |
| energy 종료(에너지-도달 + OVTIME) | main (ovtime) | main 그대로 — energy 모드에서 legacy ceiling 대체 |
| ceiling 소스 | main(TOUCH) × d(+US_REMOTE) | TOUCH + US_REMOTE (d `1f4f23c`) |

- `app_reg.c` START guard: main의 swallow-safe 구조(swallow consume 뒤,
  `us_run_status=src` 앞, **별도 break**) 유지 + d의 E-stop/과부하 차단 break를
  같은 패턴으로 3중 삽입 (spec §6, advisor 결정 존중).
- `app_modbus.c`: 상태비트 union — main `MB_STATUS_OVTIME`(bit3) + d
  `MB_STATUS_OVLD`(0x04) + `MB_STATUS_ESTOP`(0x02). 충돌 없는 비트 배치 확인.
- `board.c`: **slice-d 버전 채택** — main의 OD 전기설정은 d가 superset으로 포함
  (main 주석 "slice-d 머지 시 reconcile" 예정대로). PB3 heartbeat 제거(slice-c
  CON_OVLD 릴레이 본용도), `board_osc4/board_reset/board_seek` 추가,
  `board_heartbeat_toggle` 콜러 제거 확인.
- `app.c`: superloop 배선 order 보존 — weld → overload(2.55) → input(2.57) →
  seek_reset → reg → eth → modbus → buzzer. main의 i2c 관측 블록(§6)과 공존.
- `test/Makefile`: union (+input_fsm, osc_init_fsm, overload_fsm, buzzer_fsm) = 12.

### 5.3 ch1' (충돌 6파일: app_reg.h/.c / app.c / app_reg_calc.h/.c / test_app_reg_calc.c)

- `app_reg_calc.h/.c`: main의 `reg_energy_termination`(ovtime)과 ch1의
  `reg_current_from_adc` 등 순수함수 additive 병합 (양쪽 다 신규 추가라 교차 없음).
- `app_reg.c`: `curr_amp`/표시 전력·에너지 누산 소스 ch0→ch1 repoint (ch1 `1f43b8d`).
  ⚠ **교차영향(ch1 spec `f30bc51` 그대로)**: weld 에너지 종료 입력(`curr_energy`)도
  ch1 산출로 이동 — main의 energy 종료·OVTIME 판정이 ch1 기반이 됨. 의도된 동작.
- `app_reg.h`/`app.c`: struct에 `cal_val` 추가 + 호출부 필드 추가.
- `test_app_reg_calc.c`: 양쪽 테스트 케이스 union.

## 6. 검증·완료 기준

각 단계 tip에서 순서대로:

1. `env -u STM32_TOOLCHAIN cmake -S fw -B fw/build -G Ninja` — **reconfigure 필수**
   (GLOB 함정: 신규 소스가 링크에 안 잡힘, `project_phase12_env`).
2. `cmake --build fw/build` — our-code 0-warning (vendor wiznet socket.h 3건은
   pre-existing 무관).
3. `make -C fw/test` — host 스위트 PASS (b' 8개 → d' 12개 → ch1' 12개+확장).

리뷰 게이트: d'와 ch1'의 `app_reg.c` semantic 병합 결과는 각각 cpp-reviewer
서브에이전트 리뷰. 충돌 해소 자체는 컨트롤러 직접 수행(판단 집약적·상태 의존적 —
subagent 부적합).

## 7. 리스크와 완화

| 리스크 | 완화 |
|---|---|
| rebase 중 잘못된 해소로 코드 손실 | backup ref 3개 + 단계별 빌드/테스트 게이트 + 완료 후 `git diff backup..new` 코드 등가 확인 |
| ceiling 통합 오류(energy×hand 교차) | §5.2 표를 정본으로 구현, cpp-reviewer 게이트, host 테스트(기존 reg_calc/입력 FSM) 통과 |
| swallow-safe guard 구조 훼손 | main 구조를 기준으로 d 조건을 별도 break로만 삽입 (&&합산 금지 — advisor 결정) |
| drop 커밋에 코드 섞임 | 15개 전부 docs-only 실측 완료 (§3) |
| 브랜치 전환 stale 빌드 | 단계마다 reconfigure (§6-1) |

## 8. 범위 밖 (Out of scope)

- **머지 / 태그 / push** — HW 검증 후 별도 세션 (기존 정책).
- HW 검증 자체 (b=FREQ_IN rig, d=패널 rig+E-stop, ch1=실 초음파 부하).
- weld slice4 / B-SEAM / 6b / overload 실동작 — 기존 HW-gated 백로그 유지.
- `ref/signal/` untracked 디렉토리 — 이 작업과 무관, 손대지 않음.
