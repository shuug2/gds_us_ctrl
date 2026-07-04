# Stage Weld-Cycle 슬라이스 4 — TRIGGER + 물리 양손 트리거 + 안전 abort + D2 클램프 설계

> **요약**: samd20 공압 프레스 사이클의 물리 계층을 흡수한다 — 양손 시작(SW_START1/2)
> + `in_cycle` 재장전, TRIGGER 모드(SENSE_DN 엣지 exit), safety abort(f_safty, CYL1),
> 사이클 중 E-stop/overload/error abort, **사이클 진입 게이팅**(`app_reg_start_allowed()`
> 신설 — samd20 대비 의도된 deviation). 감사 결정 **D4/H1**(WELD 진입 모드 래치 +
> 전이 카운터 리셋)을 **선결 Task**로, **D2**(M1~M4 config-validation 클램프)를 일괄
> 포함한다. 글루 tick은 `+= WELD_TICK_MS`로 정밀화. 구조 = A안(순수 트리거 FSM
> `app_weld_trigger_fsm` 신설 + 기존 `app_weld_fsm` 확장 + 글루 배선).
> 브랜치 = `feat/stage-weld-slice4-trigger`(**ch1' 스택 위** — D5 정책 연장, 머지/태그
> = 스택 전체 HW 게이트 뒤).

- **일자**: 2026-07-04 (e)
- **브랜치**: `feat/stage-weld-slice4-trigger` (base = `feat/output-power-graph-ch1` tip `9c7cd4a`)
- **선행 정본**: 슬라이스 1~3 spec(`2026-06-14-stage-weld-cycle-slice1/2`, `2026-06-16-...-slice3`),
  감사 결정 = `docs/NEXT_STEPS.md` §1.3 (D2·D4), 발견 상세 = HANDOFF.md 2026-07-02 판(`5ca13b8`)
- **legacy 정본**: `ref/samd20/main.c` — 입력 스캔 `check_remote_input()` 1183-1268,
  start/estop 판정 1404-1425, 사이클 `do_action()` 1446-1632

---

## 1. 목표와 스코프

### 1.1 In

| # | 항목 | 근거 |
|---|------|------|
| 1 | **D4/H1 근본수정** (선결 Task): WELD 진입 시 `multi_ctrl/energy_ctrl` 내부 래치 + 모든 상태 전이에서 스테이지 카운터 리셋 | 감사 H1 (`app_weld_fsm.c:111-161` 런중 토글 stale) |
| 2 | 양손 트리거: KEY1/KEY2 동시 press → 사이클 시작, `in_cycle` 재장전 | main.c:1404, 1219 |
| 3 | TRIGGER 모드(`run_mode != 0`): CYL1 exit=SENSE_DN 엣지, WELD=trigger_time2, HOLD=trigger_time3, CYL2=즉시(§3.3) | main.c:1515, 1520, 1571, 1593 |
| 4 | safety abort: `f_safty && CYL1 && 한 손 release` → SOL OFF + READY | main.c:1484-1490 |
| 5 | 사이클 중 abort: E-stop/overload/`error_status`/safety → SOL OFF + READY (+WELD 중이면 US 정지) | main.c:1415(estop), 1452-1463, 1664-1665(error) |
| 6 | **사이클 진입 게이팅**: `app_reg_start_allowed()` 신설 — START가 거부될 상태면 사이클 자체 미시작 (**의도된 deviation**, §4.3) | 사용자 결정 2026-07-04 |
| 7 | **D2 클램프 M1~M4** (§6) | 감사 결정 D2 |
| 8 | 글루 tick `s_prev_ms += WELD_TICK_MS` + 재동기 가드 | 코드 주석 예고(`app_weld.c:59-63`), 감사 M1(글루) |

### 1.2 Out (이연)

- **HORN 모드**(SYS_HORN 수동 SOL 토글) — 사용자 결정으로 이연. `app_lcd_hook_horn`은 stub 유지.
- **CYL 타임아웃** — legacy `CYL_TIMEOUT`은 대입만 되고 TRIGGER 분기에서 미검사(**죽은 코드**, main.c:1476/1591). 충실 포팅 = 미구현.
- 에너지 절대값 E2E / divisor 검증 = 6b. OSC 물리 구동 = B-SEAM. SENSE_UP 실사용 여부 재검토 = 실 rig(§3.3).

### 1.3 재료 (이미 존재 — 신규 배선만)

- io 드라이버(slice-d): `io_read_key1`(PC12)/`io_read_key2`(PB11)/`io_read_sens_up`(PA12)/`io_read_sens_dn`(PA11) — 전부 active-LOW raw 레벨. `io_sol_dn`(PB5)은 슬라이스 1부터 hook 배선 완료.
- cfg: `run_mode`(0=delay/1=trigger), `limit_trigger_time2/3`, `f_safty` — FRAM/Modbus 배선 완료.
- ⚠ 본 브랜치는 **ch1' 스택 위** — slice-b/d 의 io/input/reg 산출물을 전제. main 단독 체리픽 불가.

---

## 2. 트리거 FSM — `app_weld_trigger_fsm` (신규, 순수)

`fw/src/app_weld_trigger_fsm.c` + `fw/include/app_weld_trigger_fsm.h`. HAL-free, host-test.
samd20 `check_remote_input()`의 weld 몫(start1/2, sens_up/dn, in_cycle)을 그대로 분리한다.

### 2.1 인터페이스

```c
typedef struct {
    uint8_t key1;        /* raw 레벨 (0=press, active-LOW) */
    uint8_t key2;
    uint8_t sens_up;     /* raw 레벨 (0=감지) */
    uint8_t sens_dn;
    uint8_t f_safty;     /* cfg 주입 */
    uint8_t weld_state;  /* WELD_* — READY/CYL1 판정용 (글루가 weld_fsm_status() 주입) */
} weld_trig_in_t;

typedef struct {
    uint8_t start_pulse;        /* 레벨 파생 시작 요청 (in_cycle==0 동안 반복 발행 가능, §2.3) */
    uint8_t safety_abort_pulse; /* 1-shot: f_safty CYL1 abort */
    uint8_t dn_edge;            /* 1-shot: SENSE_DN falling(press) 엣지 */
    uint8_t up_edge;            /* 1-shot: SENSE_UP falling 엣지 */
} weld_trig_out_t;

void weld_trigger_fsm_init(void);
void weld_trigger_fsm_step(const weld_trig_in_t *in, weld_trig_out_t *out);
```

### 2.2 거동 (legacy 대응)

| 출력 | 조건 | legacy |
|------|------|--------|
| `start_pulse` | `key1==0 && key2==0 && !in_cycle` (레벨 판정 — legacy `start_key_pressed`와 동일; 소비는 글루 게이팅이 담당) | 1404-1407 |
| `in_cycle` set | `start_pulse` 발행 시 (글루가 게이팅 통과·사이클 시작을 되알림 — §2.3) | 1472 |
| `in_cycle` clear | `key1==1 && key2==1 && weld_state==READY` | 1219-1220 |
| `safety_abort_pulse` | `f_safty && weld_state==CYL1 && (key1==1 \|\| key2==1)` | 1484 |
| `dn_edge`/`up_edge` | 해당 raw 1→0 전이 (bak 패턴) | 1222-1233 |

### 2.3 `in_cycle` 소유권

`in_cycle`은 트리거 FSM 내부 상태다. 단 **set 시점은 "게이팅을 통과해 사이클이 실제
시작된 때"**여야 한다(게이팅에 막힌 트리거가 `in_cycle`을 세우면 다음 시도 전에 양손
재장전을 강요 — legacy에 없는 벌칙). 따라서:

```c
void weld_trigger_fsm_cycle_started(void);   /* 글루: 사이클 실시작 시 호출 → in_cycle=1 */
```

`start_pulse`는 `in_cycle==0`인 동안 레벨성으로 반복 발행될 수 있다(legacy
`start_key_pressed`도 레벨) — 글루가 READY+게이팅으로 소비를 제어하므로 안전.

---

## 3. weld FSM 확장 — `app_weld_fsm`

### 3.1 H1 근본수정 (선결 Task — 본 슬라이스 첫 코드 커밋)

- WELD 진입 엣지(`weld_start`)에서 `in->multi_ctrl`/`in->energy_ctrl`을 **내부 래치**
  (`s_latched_multi`, `s_latched_energy`)로 복사. WELD 상태의 exit-경로 선택은 래치만
  참조 — 런중 LCD/Modbus 토글이 진행 중인 WELD의 종료 방식을 바꾸지 못한다.
- **모든 상태 전이**에서 `s_multi_stage/s_multi_elapsed/s_temp_time`(및 백스톱 카운터)
  리셋. READY 복귀(정상/abort 공통) 시 래치도 클리어.
- 파라미터 값(`limit_*`)은 기존대로 매 tick 주입 라이브 — 래치 대상은 **모드 2개만**
  (감사 H1의 stale은 모드 전환 조합에서만 발생; 값 변경은 legacy도 라이브).

### 3.2 TRIGGER 모드 분기 (`weld_in_t` 확장)

`weld_in_t`에 추가: `limit_trigger_time2`, `limit_trigger_time3`, `dn_edge`, `up_edge`, `abort`.

| 상태 | DELAY (`run_mode==0`, 기존) | TRIGGER (`run_mode!=0`, 신규) | legacy |
|------|------|------|--------|
| READY→CYL1 | `temp_time=delay_time1` | 시간 무장 없음 (+ **stale `dn_pressed` 클리어**) | 1473-1478 |
| CYL1 exit | `temp_time==0` | `dn_pressed` 소비 | 1498-1527 |
| WELD 무장 | comp swap(delay_time2) | comp swap(**trigger_time2**) — 로직 동일 | 1504-1510/1520-1526 |
| HOLD | `temp_time=delay_time3` | `temp_time=trigger_time3` | 1568-1571 |
| HOLD→CYL2 | `temp_time==0` 공통. CYL2 무장: `temp_time=delay_time1` | `temp_time==0` 공통 + **`up_pressed=1` 강제**(§3.3) | 1584-1593 |
| CYL2 exit | `temp_time`(=delay_time1) 소진 | `up_pressed` 소비 = **사실상 즉시** | 1606-1629 |

`dn_pressed`/`up_pressed`는 FSM **내부 래치**(legacy 전역 수명 재현): `dn_edge/up_edge`
입력이 set, 소비 지점에서 clear. `run_mode`는 사이클 시작 시점 값을 래치(모드 전환
중 사이클의 exit 규칙 혼합 방지 — H1과 같은 원리).

### 3.3 CYL2 즉시 exit (충실 포팅 판정)

legacy는 HOLD exit에서 `re_up_pressed = 1;`을 **무조건 강제**(1593, `//-` 주석) —
TRIGGER 모드 CYL2는 SENSE_UP 실입력과 무관하게 첫 평가 tick에 exit한다. 즉 legacy
실거동 = "CYL2는 SOL OFF 직후 즉시 완료(work_cnt++)". **그대로 포팅한다.**
SENSE_UP 스캔/엣지는 유지(legacy도 스캔) — 실 rig에서 상승 대기가 필요하다고 판정되면
후속(강제 set 1줄 제거로 전환 가능하도록 주석 명기).

### 3.4 abort 입력

`in.abort != 0` 이면 임의 상태에서:
1. `sol_dn=0` (SOL OFF)
2. WELD 상태였으면 `weld_stop` 엣지 발행 (글루가 US_CYCLE RUN_RELEASE — 슬라이스 C/D의
   force-stop과 이중 안전)
3. READY 복귀, `cycle_done` **미발행**(work_cnt 미증가), 래치/카운터 전부 클리어

legacy 대응: E-stop 1415(SOL OFF+M_START OFF, 해제 시 RUN_READY 1421), error 1452-1463
+1664-1665. abort 원인 구분은 FSM 밖(각 모듈이 이미 fault 표면 소유).

### 3.5 M1 — `limit_energy=0` 소비자 가드

`reg_energy_termination`(app_reg_calc)에서 **`limit_energy==0` → 에너지-도달 체크 skip**
(= `limit_out_time=0`=OVTIME off 기존 패턴과 동일 의미론). energy_ctrl ON + 목표 0의
"START 즉시 무증상 정상완료"가 사라지고, 런은 OVTIME/백스톱/30s 안전이 계속 바운드.
weld 쪽 energy exit(`app_weld_fsm`)에도 동일 가드.

---

## 4. 글루 — `app_weld.c`

### 4.1 tick 정밀화

```c
s_prev_ms += WELD_TICK_MS;                     /* 드리프트 무누적 (감사 M1) */
if ((uint32_t)(now - s_prev_ms) > 10u * WELD_TICK_MS) s_prev_ms = now;  /* 재동기 가드 */
```

### 4.2 배선 (매 10ms tick)

1. `weld_trig_in_t` 채움: `io_read_key1/key2/sens_up/sens_dn()` + `cfg->f_safty` + `weld_fsm_status()`
2. `weld_trigger_fsm_step()` 실행
3. `start_pulse && weld READY && app_reg_start_allowed()` → `s_start_pending=1` +
   `weld_trigger_fsm_cycle_started()` (기존 `app_weld_request_start()` 경로 재사용)
4. abort 합성: `E-stop 활성 || overload 활성 || error_status!=0 || safety_abort_pulse`
   → `in.abort=1`. (E-stop/overload 상태 접근자는 slice-c/d 글루가 이미 보유 — 없으면
   read-only 접근자 1개씩 추가)
5. `weld_in_t`에 `trigger_time2/3`, `dn_edge/up_edge`, `abort` 추가 주입.
   **M2**: `limit_mo_out1/2` cast 전 [50,100] 클램프(belt-and-braces).

### 4.3 `app_reg_start_allowed()` (신규, app_reg.c) — 의도된 deviation

```c
bool app_reg_start_allowed(void);   /* 읽기 전용 — 소비/상태 변경 없음 */
```

START guard 5-break와 **동일 조건**: warm-up 완료 && `us_run_status==US_IDLE` &&
seek/reset 비활성 && `error_status==0` && overload 비활성 && E-stop 비활성 &&
swallow 미대기. 구현은 guard 본문과 **공용 static helper**로 묶어 드리프트 차단.

**deviation 근거**: samd20은 M_START 직결이라 guard가 없었다. 포팅 후 guard가 US_CYCLE
START를 삼키면 "SOL만 하강하는 블라인드 사이클"이 발생 — 사용자 결정(2026-07-04)으로
**사이클 진입 자체를 게이팅**한다. CYL1 진행 중 조건 악화(overload 등)는 §3.4 abort가
커버(게이팅과 abort의 이중 방어 — WELD 진입 시점 START 거부 레이스 해소).

---

## 5. 사이클 시작 소스 정책

물리 양손 트리거 **전용**(legacy 충실 — `start_key_pressed`는 SW_START1/2만).
`app_weld_request_start()`는 글루 내부 경로로 유지하되 외부(LCD/Modbus) 호출자는 이번
슬라이스에도 만들지 않는다.

---

## 6. D2 클램프 — M3·M4

### 6.1 M4: LCD `app_lcd_input.c` LV_* 필드 클램프 (Modbus `app_modbus_apply_writes` 미러)

| LV (case 라인) | cfg 필드 | 클램프 |
|---|---|---|
| `LV_OUT_POWER` (784) | output_power | **[50,100]** (LOW-1 원조) |
| `LV_DM_DELAY` (745) | limit_delay_time1 | ≤500 |
| `LV_DM_WELD` (748) | limit_delay_time2 | ≤500 |
| `LV_DM_HOLD` (751) | limit_delay_time3 | ≤2000 |
| `LV_TM_WELD` (754) | limit_trigger_time2 | ≤500 |
| `LV_TM_HOLD` (757) | limit_trigger_time3 | ≤2000 |
| `LV_MAX_ON_TIME` (789) | limit_on_time | ≤2000 |
| `LV_MO_OUT1/2` (762/765) | limit_mo_out1/2 | [50,100] |
| `LV_LIMIT_OUT_T` (795) | limit_out_time | ≤10 |
| `LV_ENERGY_EDIT` (792) | limit_energy | 클램프 없음(Modbus 동일) — M1은 §3.5 소비자 가드 |

slice-3에서 규명된 LCD ON_TIME "clamp max=100" 관찰은 **DGUS 자산측 입력 제한**이지
펌웨어 클램프가 아님 — 펌웨어는 Modbus와 동일한 ≤2000을 방어선으로 갖는다.

### 6.2 M3: FRAM 로드 idx 클램프 (`app_config.c`)

`comm_speed_idx`/`parity_idx` 로드 직후 테이블 범위 밖이면 **factory 기본값으로 대체**
— OOB flash read 차단. 범위 = 기존 터치 경로 가드의 매크로 재사용:
`COMM_SPEED_IDX_MAX=5`(`comm_speed_txt[6][6]`), `COMM_PARITY_IDX_MAX=2`
(`comm_parity_txt[3][4]`) — `app_lcd_input.c:62-63`. 매크로는 공용 헤더로 승격해
`app_config.c`와 공유(중복 정의 금지).

---

## 7. 테스트

### 7.1 host (신규/확장)

- **`test_app_weld_trigger_fsm`** (신규): 양손 조합(단독/동시/역순), `in_cycle` 재장전
  (사이클 중 재press 무효·READY 복귀 후 양손 release 전 무효), `cycle_started` 콜백 게이팅
  (게이팅 실패 시 `in_cycle` 미장전), safety abort(f_safty=0/1 × CYL1/그 외 상태 × 한손/양손
  release), 센서 엣지 1-shot.
- **`test_app_weld_fsm`** (확장): TRIGGER 전이 전열(dn_pressed 소비/HOLD trigger_time3/
  CYL2 즉시 exit), stale dn_pressed 클리어(READY→CYL1), abort×각 상태(SOL OFF/weld_stop
  유무/work_cnt 미증가/재시작 정상), **H1**(WELD 중 multi/energy 토글이 exit 경로 불변
  + 전이 카운터 리셋 + READY 복귀 후 새 모드 정상 반영), run_mode 래치(사이클 중 모드
  전환 무영향), M1(limit_energy=0 → 에너지-도달 skip).
- 기존 스위트: LCD 클램프(M4 필드별 경계값), config 로드(M3 OOB→factory), app_reg_calc(M1).

### 7.2 빌드/리뷰 게이트

0-warning(our code) + 전 스위트 PASS + Task별 spec/cpp-reviewer 2-stage + 최종 통합
cpp-reviewer — 기존 절차(`docs/NEXT_STEPS.md` §3).

### 7.3 HW (패널 rig — 머지 게이트)

1. DELAY 사이클 E2E: 양손 press → SOL 하강 → (delay1) → US ON → (delay2) → US OFF →
   (delay3) → SOL 상승 → work_cnt++ / LCD 갱신
2. TRIGGER 사이클 E2E: 양손 → SOL 하강 → **SENSE_DN 물리 트리거** → US → HOLD →
   즉시 CYL2 완료(§3.3 거동 확인 — 실 rig에서 상승 대기 필요성 판정)
3. 재장전: 사이클 완료 후 양손 유지 → 재시작 없음, 양손 release 후 재press → 시작
4. safety abort: f_safty=1, CYL1 중 한 손 release → SOL 즉시 OFF, work_cnt 불변
5. E-stop abort: 사이클 중 EMSW → SOL OFF + US 정지, 해제 후 정상 재시작
6. 게이팅: RESET 체인 진행 중 양손 press → 사이클 미시작(SOL 무반응)
7. 회귀: 직접런 ceiling(mbpoll ~560ms)/LCD/Modbus 무회귀 — **D5 스택 거동 변화 2건 반영**
   (TOUCH 비-energy=30s만, OVTIME>30s=30s 캡)
8. 슬라이스 1~3 이연분: 사이클 타이밍 실측, multi 2단 스테핑 전환 tick, energy exit
   (에너지 절대값은 6b)

### 7.4 검증 규칙

mbpoll/LCD 육안만 — **SWD halt 금지**(`feedback-swd-halt-breaks-board-validation`).
브랜치 전환 후 **cmake reconfigure 필수**(GLOB 함정 — 신규 .c 2개).

---

## 8. 리스크·주의

- **스택 종속**: 본 브랜치는 b'/d'/ch1' 미머지 스택 위 — HW 검증·머지 순서는
  b'→d'→ch1'→slice4. 스택 하부 수정 발생 시 rebase 필요.
- E-stop/overload 접근자 신설 시 slice-c/d 파일에 **읽기 전용 추가만** — 기존 로직
  무수정(reconcile 산출물 보존).
- `weld_backstop_ticks` 폭(LOW, 감사): `limit_out_time` ≤10 클램프(M4/Modbus)로 실질
  차단되나, 백스톱 계산 자료형은 구현 시 재확인.
- 머지 노트: ch1 교차영향(에너지 적분=ch1) 명기는 ch1' 머지 커밋 조건 — slice4와 별개.
