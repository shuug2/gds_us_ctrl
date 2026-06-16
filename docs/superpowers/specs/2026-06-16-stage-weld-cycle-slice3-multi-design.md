# Stage Weld-Cycle — Slice 3: Multi-Ctrl (2단 진폭 스테핑) Design

> **요약**: weld-cycle 슬라이스1(DELAY 시간-exit)·슬라이스2(energy-exit)에 이어, **WELD 단계 안에서 진폭을 2단으로 스테핑**하는 슬라이스. samd20 `multi_ctrl`(`main.c:131`)이 true면, WELD 진입 시 1단 진폭(`limit_mo_out1`)으로 시작 → 일정 시간(`limit_mo_time1`) 경과 후 2단 진폭(`limit_mo_out2`)으로 전환 → 총 시간(`limit_mo_time2`) 경과 시 WELD 종료(→HOLD). samd20 4개 지점 포팅(스테핑 로직 `main.c:5232-5267`, stage 0→1 전환 `5240-5242`, WELD→HOLD exit `5250-5258`, WELD 진입 리셋 `1539-1549`). **아키텍처 = FSM 내부 상태**(슬라이스2 Option A보다 단순 — app_reg/주입 불필요): multi는 순수 시간 기반이라 FSM이 이미 가진 타이머처럼 `s_multi_stage`(0/1)·`s_multi_elapsed`(10ms tick)를 **FSM 내부 런타임 상태**로 둔다. **진폭 emit = 단일 설계**: 기존 `weld_start` 엣지가 1단 진폭을 내보내므로, 0→1 전환 엣지(`amp_change`) 하나만 추가해 **같은 `set_amp` hook을 mid-WELD 재호출**. **우선순위 = multi > energy > 시간**(samd20 if-else `1562`); multi ON이면 슬라이스1 DELAY-exit·슬라이스2 energy-exit이 **둘 다 차단**됨(기존 브랜치 게이팅 누락 = 주 버그 핫스팟). **comp_time 미적용**(samd20 multi 경로 `5242`·`1540`은 `limit_mo_outN` 직접 변환, 단주기 보정 없음). **언더플로 가드 필수**(팩토리 기본 `limit_mo_out1=25` < 50이 바로 트리거 — 슬라이스1 가드 재사용). **타이밍 해상도 = 액면가 10ms tick**(사용자 확정, §3.2). **범위 = weld-cycle WELD 전용**(weld-only, 슬라이스2 선례); samd20이 US_COMM/US_REMOTE 직접런에도 스테핑하는 부분은 이연(§6 deviation). 충실도 = 혼합. 시간 단위 = WELD FSM **10ms/count**. HW 불요 — host-test가 게이트.

---

## 1. 목표 & 범위

### 1.1 컨텍스트
- 슬라이스1(DELAY 시간-exit)·슬라이스2(energy-exit)가 main 머지됨(`hw-revA_fw-stage-weld1`/`-weld2`). WELD 진폭은 진입 시 `output_power`(comp_time 보정) 1회 emit으로 **고정**.
- samd20 실제 거동: `multi_ctrl==true`면 WELD 동안 진폭을 **2단**으로 바꾼다(1단=초기 정렬/예열 진폭, 2단=본 용접 진폭, 또는 그 반대). 2단 스테핑 + 시간 기반 WELD 종료가 multi 모드의 핵심.
- STM32 현황: `multi_ctrl`/`limit_mo_out1/2`/`limit_mo_time1/2` **config는 이미 포팅됨**(`app_config.h:15,20`, factory `app_config.c:8-22`, FRAM save/load, Modbus `MB_REG_MULTI_T1/T2/O1/O2`, LCD `LV_MO_OUT1/OUT2/TIME1/TIME2`·`DISP_MULTI_EN`). 그러나 **스테핑 로직·`multi_ctrl_stage` 런타임 상태는 미포팅**(`app_reg.c:246` 주석 "deferred, spec §8") = 본 슬라이스.
- 충실도 입장: **혼합** — 거동은 samd20 충실, 명백한 버그(언더플로)만 가드, quirk·deviation은 §6/§8/§11 문서화.

### 1.2 In scope (슬라이스 3)
- **FSM 2단 스테핑 (`app_weld_fsm`)**: `weld_in_t`에 `multi_ctrl`/`limit_mo_out1/2`/`limit_mo_time1/2` 추가. FSM 내부 상태 `s_multi_stage`(0/1)·`s_multi_elapsed`(10ms tick) 추가. `weld_out_t`에 `amp_change`(0→1 전환 1-shot) 엣지 추가, `amplitude` 필드 재사용.
- **WELD 진입 (multi)**: `s_multi_stage=0`, `s_multi_elapsed=1`(0 아님 — §3.3 span-일관 rationale), `amplitude=conv(limit_mo_out1)`, 기존 `weld_start` 엣지로 emit(슬라이스1 경로 재사용).
- **WELD step (multi)**:
  - `s_multi_stage==0 && s_multi_elapsed >= limit_mo_time1` → `s_multi_stage=1`, `amplitude=conv(limit_mo_out2)`, `amp_change=1`.
  - `s_multi_elapsed >= limit_mo_time2` → `weld_stop` → HOLD, `s_temp_time=limit_delay_time3`(samd20 `5256`).
- **우선순위 / 기존 exit 게이팅**: `if multi_ctrl {…} else if energy_ctrl {…} else { DELAY 시간 }`. multi ON이면 슬라이스1·2 exit 미발동(samd20 `1562` 충실).
- **진폭 변환 + 언더플로 가드 (순수)**: `conv(out) = (out>=50) ? (out-50)*255/100 : 0`. **comp_time 미적용**(multi 경로). 슬라이스1 언더플로 가드 재사용.
- **glue (`app_weld`)**: `weld_in_t`에 cfg 5필드(`multi_ctrl`+4) 주입(energy 필드 옆). `amp_change` 엣지 → `app_weld_hook_set_amp(out.amplitude)` 재호출(기존 `weld_start` 경로와 동형).
- **host 테스트**: 스테핑 시퀀스·양쪽 진폭 emit·time2 종료·multi-overrides-energy·multi-overrides-DELAY·언더플로(out<50)·stage 리셋(WELD 재진입)·경계(`time1==time2`/`time1>time2`)·슬라이스1·2 회귀.

### 1.3 Out of scope (deferred — 후속 슬라이스)
| 항목 | 슬라이스 / 게이트 |
|---|---|
| **직접 RUN(US_COMM/US_REMOTE) 스테핑** (samd20 `5234`의 `us_run_status` 분기) | 별도(§6 deviation — weld-only) |
| `limit_mo_out1/2` [50,100] **config-validation 클램프** (언더플로 가드는 FSM이 하나, 클램프는 입력 계층) | 4 (슬라이스1 LOW-1·슬라이스2 §11-5와 일괄) |
| mid-cycle `multi_ctrl` / `limit_mo_*` 동적 토글 래치 | 4 (슬라이스2 §11-4와 동류) |
| 절대 진폭 물리 보정 (`limit_mo_outN` DAC → 실 진폭) | 6b / HW (실 초음파 rig + 스코프) |
| 물리 트리거(SW_START)·실 SOL_DN/센서·안전 abort | 4 (HW-gated) |

> 슬라이스3은 **2단 스테핑 로직 + 진폭 변환**을 host-test로 검증한다. HW에선 (슬라이스4 물리 트리거가 없으므로) 사이클 자체는 여전히 휴면 — 보드는 **기존 직접-초음파 무회귀만** 확인(§10). 스테핑 자체 HW E2E는 슬라이스4.

---

## 2. 아키텍처 (FSM 내부 상태 — app_reg/주입 불필요)

### 2.1 슬라이스2 Option A 대비 더 단순한 이유
슬라이스2의 "누산은 app_reg, FSM은 주입받아 비교만" 분리는 **에너지 누산이 ISR cadence ADC 샘플링을 요구**해 순수 FSM에 둘 수 없었기 때문이다. multi_ctrl은 **순수 시간 기반 스테핑** — FSM은 이미 DELAY/backstop 타이머(`s_temp_time`)를 위해 경과 시간을 추적한다. 따라서 stage 카운터(`s_multi_stage`)·경과 카운터(`s_multi_elapsed`)는 `s_f_status_start`/`s_temp_time`과 동급의 **FSM 내부 런타임 상태**일 뿐이다. **app_reg도, 주입도, 새 순수 헬퍼 파일도 필요 없다.**

```
app_weld_fsm.{c,h}   순수 FSM (host-test 대상)
  - weld_in_t  += multi_ctrl, limit_mo_out1, limit_mo_out2, limit_mo_time1, limit_mo_time2
  - weld_out_t += amp_change            (0→1 전환 1-shot; amplitude 필드는 재사용)
  - 내부 상태  += s_multi_stage(0/1), s_multi_elapsed(uint16, 10ms tick)
  - 진폭 변환 + 언더플로 가드 (multi 경로, comp_time 미적용)

app_weld.{c,h}       글루
  - weld_in_t 에 cfg 5필드 주입 (energy 필드 옆)
  - out.amp_change 엣지 → app_weld_hook_set_amp(out.amplitude)   (weld_start 경로와 동형)
```

### 2.2 진폭 emit = 단일 설계 (두 메커니즘 아님)
mid-WELD 진폭 변경에 필요한 것은 (a) 내부 stage 카운터 (b) 진폭-emit 엣지뿐이다. 기존 `weld_start` 엣지가 이미 1단(`s_multi_stage=0`) 진폭을 `amplitude` 필드로 내보내고 글루가 `set_amp`로 라우팅한다. 0→1 전환용 엣지(`amp_change`) 하나만 추가하고, 글루가 **같은 `amplitude` 필드를 같은 `set_amp` hook으로 재라우팅**하면 끝. 별도 출력 채널/별도 hook 불요.

---

## 3. FSM 상태 모델 변경 (WELD 분기, 내부 stage/elapsed 추가)

슬라이스1·2 대비 **WELD 상태만** 변경. READY/CYL1/HOLD/CYL2는 무수정.

### 3.1 인터페이스 추가 (`app_weld_fsm.h`)
```c
typedef struct {
    /* ... 슬라이스1/2 필드 그대로 ... */
    uint8_t  multi_ctrl;          /* 1 = WELD 동안 2단 진폭 스테핑 (energy/DELAY exit 차단) */
    uint16_t limit_mo_out1;       /* 1단 진폭 (cfg, output_power 단위 50..100; Modbus MB_REG_MULTI_O1) */
    uint16_t limit_mo_out2;       /* 2단 진폭 (cfg, output_power 단위 50..100; MB_REG_MULTI_O2) */
    uint16_t limit_mo_time1;      /* 1단 지속 (cfg, 10ms tick; 0→1 전환 시점) */
    uint16_t limit_mo_time2;      /* WELD 총 길이 (cfg, 10ms tick; WELD 종료 시점, ≥ time1) */
} weld_in_t;

typedef struct {
    /* ... 슬라이스1/2 필드 그대로 ... */
    uint8_t  amp_change;          /* 1 = stage 0→1 전환 엣지 (글루가 set_amp 재호출) */
} weld_out_t;
```
- 내부(FSM static): `static uint8_t s_multi_stage; static uint16_t s_multi_elapsed;`

### 3.2 타이밍 해상도 = 액면가 10ms tick (사용자 확정)
- samd20: `us_on_time(100ms 카운터) >= limit_mo_time1/10`(`main.c:5240`) — 100ms로 **정수 절단**. 팩토리 기본 `limit_mo_time1=25` → `25/10=2` → 실제 **200ms**에 전환(명목 250ms 아님).
- **본 슬라이스 = 액면가**: `s_multi_elapsed`(10ms/tick)를 `limit_mo_time1`/`limit_mo_time2`와 **직접** 비교(`/10` 절단 없음). `limit_mo_time1=25` → 250ms, `limit_mo_time2=50` → 500ms. **FSM이 `limit_delay_*`를 10ms tick으로 쓰는 방식과 일관**.
- ⚠ **divergence**: samd20 실동작(100ms 절단) 대비 최대 ~90ms 차이. 의도된 deviation — 본 §3.2 + §8에 명시. (절대 타이밍 일치가 필요하면 6b/실 rig에서 재검토.)

### 3.3 WELD 진입 (`f_status_start==0`)
- 슬라이스1·2 진입 시퀀스 유지하되 **진폭 소스만 multi 분기**:
  - `multi_ctrl`: `s_multi_stage=0; s_multi_elapsed=1; amplitude = conv(limit_mo_out1);` (comp_time 미적용)
  - `!multi_ctrl`: 기존 — `amplitude = conv_with_comp(output_power, comp_time)` (슬라이스1 경로 무수정)
- ⚠ **`s_multi_elapsed` 진입값 = `1`(0 아님 — 구현 시 확정, 본 spec 정정)**: CYL1→WELD 전이 step에서 `s_run_status`가 이미 WELD로 바뀌므로 weld_start emit은 그 **다음** step이다. 슬라이스1의 초음파-on span(weld_start→weld_stop)은 `limit_delay_time2−1` tick(전이 step에서 `s_temp_time=ldt2` 세팅 → 첫 WELD step 진입부에서 1회 감소). multi도 `elapsed=1`로 시작해야 span = `limit_mo_time2−1` tick으로 **슬라이스1 convention과 일치**(`elapsed=0`이면 1틱 김). host-test는 **총 duration**(`weld_steps==time2`)과 전환 **count/amplitude**를 핀하나, 정상(`time1<time2`) 케이스의 **전환 tick index**(out1 실제 duration=`time1−1`)는 핀하지 않음 — ±1틱(10ms) 정밀도는 슬라이스4 실 초음파 HW에서 확인.
- `weld_start` 엣지·US_CYCLE START·`s_temp_time`/comp_time 세팅은 기존 그대로. multi 모드에서도 `s_temp_time`은 진입 시 세팅되지만 **종료는 `s_multi_elapsed >= limit_mo_time2`가 담당**(§3.4) — `s_temp_time` 만료 경로는 multi 게이팅으로 차단.

### 3.4 WELD step (`f_status_start==1`) — 우선순위 체인 (★ 버그 핫스팟)
```
if (multi_ctrl):                                  # samd20 5234 (우선순위 최상)
    s_multi_elapsed++  (포화 가드)
    if (s_multi_stage == 0 and s_multi_elapsed >= limit_mo_time1):
        s_multi_stage = 1
        amplitude  = conv(limit_mo_out2)          # samd20 5242
        amp_change = 1                            # 1-shot 엣지
    if (s_multi_elapsed >= limit_mo_time2):       # samd20 5250 (WELD→HOLD)
        weld_stop = 1; -> HOLD; s_temp_time = limit_delay_time3
elif (energy_ctrl):                               # 슬라이스2 (무회귀)
    ... 기존 energy-exit / backstop ...
else:                                             # 슬라이스1 시간-exit (무회귀)
    if (s_temp_time == 0):
        weld_stop = 1; -> HOLD; s_temp_time = limit_delay_time3
```
- **게이팅이 핵심**: `multi_ctrl` 분기 안에서는 `s_temp_time` 만료(슬라이스1)·`curr_energy>=limit_energy`(슬라이스2)를 **검사하지 않는다**. samd20 `1562`의 `(multi!=true)&&(energy!=true)` 게이트 + `5234`의 if-else 우선순위를 재현. 기존 두 exit가 multi ON일 때도 발동하면 = 그럴듯한 버그(host-test 4·5가 가드).
- `s_multi_elapsed` 증가는 step 진입부에서 1회. 포화 가드(uint16 max에서 멈춤)로 wrap 방지(time2가 매우 크면).
- `amp_change`는 **0→1 전환 step에서만** 1(이후 step은 0). 글루가 set_amp 재호출.
- multi 모드 WELD 종료는 **graceful HOLD**(슬라이스1·2 정상 종료와 동일 → CYL2 → `cycle_done`/work_cnt++). abort 아님.

### 3.5 진폭 변환 + 언더플로 가드 (순수)
```c
/* multi 경로: comp_time 미적용 (samd20 5242/1540 직접 변환) */
static uint8_t conv_mo(uint16_t out) {
    return (out >= 50u) ? (uint8_t)(((out - 50u) * 255u) / 100u) : 0u;   /* 언더플로 가드 */
}
```
- ⚠ **팩토리 기본 `limit_mo_out1=25` < 50** → 가드 없으면 `(25-50)`가 음수 → uint 언더플로/wrap. **가드 필수.** 슬라이스1의 `weld_amplitude` 언더플로 가드와 동일 클래스(슬라이스1은 `output_power<50`, 여기선 `limit_mo_outN<50`).
- `[50,100]` config-validation 클램프(입력 계층)는 **슬라이스4 이연**(슬라이스1 LOW-1·슬라이스2 §11-5와 일괄). FSM 가드는 잘못된 config에서도 0(무진폭)으로 안전 degrade.

---

## 4. samd20 충실성 매핑

| samd20 (`ref/samd20/main.c`) | 동작 | STM32 슬라이스3 |
|---|---|---|
| `131` `bool multi_ctrl` / `128` `uint8_t multi_ctrl_stage` | 플래그 + 이진 stage | cfg `multi_ctrl`(포팅됨) + FSM `s_multi_stage` |
| `1539-1549` WELD 진입 `multi_ctrl_stage=0; temp_i=(out1-50)*255/100` | 진입 1단 진폭 | §3.3 진입 multi 분기 |
| `5234` `(...||RUN_WELD) && multi_ctrl` | 스테핑 활성 조건 | §3.4 `if(multi_ctrl)` (weld-only → RUN_WELD만) |
| `5240-5242` `us_on_time>=time1/10 → stage=1; out2` | 0→1 전환 + 2단 진폭 | §3.4 stage 0→1 (액면가 tick, §3.2) |
| `5250-5258` `us_on_time>=time2/10 → RUN_HOLD` | WELD 종료 | §3.4 `>= limit_mo_time2 → HOLD` |
| `5262` direct `us_off()` | 직접 RUN 종료 | **이연**(§6 deviation, weld-only) |
| `1562` `(multi!=true)&&(energy!=true)` 시간-exit 게이트 | 우선순위 게이팅 | §3.4 if-else 체인 |

---

## 5. glue / hook (`app_weld.c`)

| 위치 | 변경 |
|---|---|
| `weld_in_t` 구성(매 tick) | cfg 5필드 주입: `multi_ctrl`, `limit_mo_out1/2`, `limit_mo_time1/2` (energy 필드 옆) |
| out 라우팅 | `out.amp_change` 엣지 → `app_weld_hook_set_amp(out.amplitude)` (기존 `weld_start`→set_amp와 동형, 같은 hook) |

> 기존 hook(`set_amp`/`sol_dn`/`fault`)·`app_reg_command(US_CYCLE)`·10ms 게이트 무수정. **신규 hook 없음**(슬라이스2 fault hook 재사용 안 함 — multi는 abort 없음). app_reg 무수정(주입 불필요).

---

## 6. 범위 경계 / deviation (spec 명시)

- ✅ **In**: weld-cycle WELD 2단 진폭 스테핑 + 시간 기반 WELD 종료(multi 모드) + 언더플로 가드 + multi 우선순위 게이팅.
- ⛔ **이연**: 직접 RUN(US_COMM/US_REMOTE) 스테핑, `limit_mo_out` config 클램프, mid-cycle 토글 래치, 절대 진폭 보정, 물리 트리거.
- ⚠ **deviation 기록 (사용자 확정 — weld-only)**: samd20 `5234`는 `(US_COMM||US_REMOTE||RUN_WELD)&&multi_ctrl`로 직접런에도 스테핑. 본 슬라이스는 **weld-cycle WELD에만** 적용 → **multi_ctrl=ON 상태에서 Modbus/패널 직접 START(US_TOUCH/US_COMM)는 스테핑하지 않고 단일 진폭(`output_power`)으로 기존 on-time ceiling 종료**. 슬라이스2 energy §6 deviation과 동일 경계(트리거 분리 원칙). 직접런 스테핑이 필요해지면 별도 슬라이스에서 app_reg 런 경로에 추가.
- ⚠ **타이밍 deviation (§3.2)**: 액면가 10ms tick — samd20 100ms 절단 대비 ~90ms divergence.

---

## 7. (해당 없음 — 신규 hook 없음)

슬라이스3은 mid-WELD 진폭 재-emit을 **기존 `set_amp` hook 재호출**로 처리하므로 새 hook 인터페이스가 없다. (슬라이스2의 `app_weld_hook_fault`는 multi에 미사용 — multi 종료는 graceful HOLD.)

---

## 8. 충실도 노트 & quirk

- **2단 = 이진 stage**: samd20 `multi_ctrl_stage`는 0/1 두 값뿐(`main.c:128`). 3단 이상 없음 — FSM `s_multi_stage`도 0/1.
- **multi 우선순위 최상**: multi ON이면 energy_ctrl/시간-exit 무시(samd20 if-else `5234`). 같은 WELD에서 multi+energy 동시 ON은 multi가 이김(에너지 종료 안 함).
- **comp_time 미적용**: samd20 multi 경로(`5242`·`1540`)는 단주기 진폭 보정 없이 `limit_mo_outN` 직접 변환. 슬라이스1의 comp_time 감쇠는 일반 `output_power` 경로 전용.
- **타이밍 액면가(§3.2)**: samd20 100ms 절단(quirk)을 재현하지 않고 10ms tick 액면가 — FSM 내부 일관성 우선. ~90ms divergence는 의도.
- **언더플로 가드**: 팩토리 기본 `limit_mo_out1=25`가 바로 트리거. 가드로 0 degrade. 클램프(입력)는 슬라이스4.
- **time2 ≥ time1 가정**: LCD 입력이 `time1 ≤ time2` 클램프(`app_lcd_input.c`, samd20 `4011-4024`). `time1 > time2`면 0→1 전환 전에 종료 조건이 먼저 참 → stage 1 진폭이 안 나옴(단일 진폭). FSM은 방어적으로 동작(종료가 우선); host-test가 경계 커버. (정상 운용에선 클램프로 방지.)

---

## 9. 테스트 (host)

### 9.1 `test_app_weld_fsm.c` 추가 (multi 시나리오)
1. **스테핑 시퀀스**: `multi_ctrl=1`, out1/out2/time1/time2 설정 → WELD 진입 시 `amplitude=conv(out1)`+`weld_start`, `s_multi_elapsed==time1` step에서 정확히 1회 `amp_change`+`amplitude=conv(out2)`, `s_multi_elapsed==time2` step에서 `weld_stop`+HOLD.
2. **양쪽 진폭 값**: out1/out2 = 50/100 → 진폭 0/127; 75/50 → 63/0 등 known-vector(역순 포함).
3. **time2 종료 → 정상 사이클**: multi WELD 종료가 HOLD→CYL2→`cycle_done`(work_cnt++) 정상 완주(abort 아님).
4. **multi가 energy override**: `multi_ctrl=1 && energy_ctrl=1`, curr_energy를 limit 초과로 주입해도 **energy-exit 미발동**, time2에 종료.
5. **multi가 DELAY override**: `multi_ctrl=1`, `limit_delay_time2` 작게 줘도 `s_temp_time` 만료로 종료 **안 함**, time2에 종료.
6. **언더플로 가드**: `limit_mo_out1=25`(<50) → `amplitude=0`(wrap 없음), `limit_mo_out2=60` → 정상 전환.
7. **stage 리셋(WELD 재진입)**: 1사이클 완주 후 다음 사이클 WELD 진입 시 `s_multi_stage=0`/`s_multi_elapsed=0` 재시작(잔존 없음).
8. **경계 `time1==time2`**: 전환과 종료가 같은 step → 종료 우선(또는 전환+즉시 종료) 정의된 거동 1택 고정.
9. **경계 `time1 > time2`**: 종료 조건이 먼저 참 → 단일 진폭(out1)로 종료(방어적, §8).
10. **슬라이스1·2 회귀**: `multi_ctrl=0` → 기존 시간-exit/energy-exit 시퀀스 완전 동일(기존 12 테스트 그대로 PASS).

### 9.2 진폭 변환 헬퍼
11. **conv_mo known-vector**: out=50→0, 75→63, 100→127, 49→0(가드), 0→0. (슬라이스1 amp 테이블과 일관 검증.)

> 글루(app_weld.c) `amp_change`→set_amp 결선은 통합 cpp-reviewer + HW mon으로 검증(프로젝트 패턴).

---

## 10. 빌드/검증 게이트

- **호스트(슬라이스3 주 게이트)**: `env -u STM32_TOOLCHAIN cmake --build fw/build` 0-warning(우리 코드) + `make -C fw/test test` 4스위트(weld_fsm 확장 = 기존 12 + multi ~11) PASS.
- **HW(보드, 회귀확인)**: 프로덕션 사이클 트리거 없음(슬라이스4) → **무회귀 확인 위주**. ① 기존 직접-초음파(패널/Modbus START → ICON_RUN/~560ms ceiling) 무회귀. ② multi_ctrl=ON에서 직접 START가 스테핑 **안 함**(단일 진폭, on-time ceiling 종료) = §6 deviation 확인. ③ work_cnt 0 유지(weld FSM dormant 구조증명).
- **2단 스테핑 사이클 E2E는 슬라이스4**(물리 SW_START + 실 초음파로 진폭 변화를 스코프 관측) / 절대 진폭 보정은 6b.
- 머지: `--no-ff` 로컬, 태그 `hw-revA_fw-stage-weld3`(⚠ host + HW-regression verified — 스테핑 E2E는 슬라이스4/실 rig).

---

## 11. 미해결 / 후속 확인 항목

1. **직접 RUN 스테핑** — §6 deviation. multi_ctrl=ON 직접 START는 단일 진폭. 필요 시 별도 슬라이스(app_reg 런 경로).
2. **`limit_mo_out` config 클램프** — FSM 언더플로 가드는 0 degrade(안전). 입력 `[50,100]` 클램프는 슬라이스4(슬라이스1 LOW-1·슬라이스2 §11-5 일괄: `limit_energy≥1`/`limit_out_time≥1`/`output_power`/`limit_mo_out1/2` 클램프 통합).
3. **mid-cycle `multi_ctrl`/`limit_mo_*` 토글** — 글루가 매 tick 주입 → WELD 중 토글 시 거동. `s_multi_stage`/`s_multi_elapsed`는 WELD 진입 시 1회 래치되나 `limit_mo_time1/2`/`out1/2`는 매 tick 라이브 → mid-WELD 변경 시 즉시 반영(전환 시점/진폭 흔들림). 슬라이스2 §11-4와 동류 — 슬라이스4에서 WELD 진입 시 multi 파라미터 래치 또는 "mid-cycle 토글 금지" 보장(cpp-review 후보).
4. **타이밍 액면가 vs samd20 절단** (§3.2/§8) — ~90ms divergence. 실 용접 타이밍이 레거시와 일치해야 하면 6b/실 rig에서 재검토(절단 모드 옵션화 가능).
5. **`time1==time2` 경계** (§9-8) — 전환·종료 동시 step의 거동 1택 고정(구현 시 확정 + host-test 동결). 정상 운용은 `time1 < time2`(LCD 클램프).
6. **절대 진폭 물리 보정** — `limit_mo_outN` → DAC → 실 진폭은 6b 캘리브레이션/실 초음파 rig. host-test는 변환 산술 구조만.

---

*작성: 2026-06-16 weld-cycle 슬라이스3 브레인스토밍. samd20 참조 `ref/samd20/main.c:128/131`(stage/flag)·`1539-1549`(진입 리셋+1단)·`5232-5267`(스테핑)·`5240-5242`(0→1+2단)·`5250-5258`(WELD→HOLD)·`1562`(게이트). 아키텍처=FSM 내부 상태(advisor — app_reg/주입 불필요). 충실도=혼합. 타이밍=액면가 10ms tick(사용자 확정). 범위=weld-only. 다음 = 본 spec 사용자 리뷰 → writing-plans.*
