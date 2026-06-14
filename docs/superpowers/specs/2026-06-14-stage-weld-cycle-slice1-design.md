# Stage Weld-Cycle — Slice 1: Core FSM (DELAY mode) Design

> **요약**: samd20의 공압 프레스 용접 사이클 FSM(`RUN_READY→CYL1→WELD→HOLD→CYL2→work_cnt++`)을 STM32F410으로 포팅하는 첫 슬라이스. **DELAY 모드 전용**(시간 기반, 위치센서 불필요), `energy_ctrl`/`multi_ctrl` off 경로만. 신규 모듈 2개로 분리: **`app_weld_fsm.c`(HAL-free 순수 FSM 코어, host-test)** + **`app_weld.c`(글루)**. WELD 단계는 신규 소스 **`US_CYCLE`**로 기존 `app_reg` 초음파 게이트를 구동(진폭 hook·peak 래치·STATUS bit0 재사용), WELD 길이는 weld 타이머(`limit_delay_time2`)가 지배(on-time ceiling 아님). 물리 IO(SOL_DN 솔레노이드·OSC 초음파 GPIO·위치센서)는 **전부 hook 뒤 / 로그 스텁**(실 GPIO 드라이브·센서·TRIGGER·안전 abort·energy·multi = 슬라이스 2~4 deferred). 사이클 완료 시 **`work_cnt++` + FRAM 영속 + LCD `LV_WORK_CNT` 갱신**. **트리거(samd20 충실, 2026-06-14 사용자 확정)**: weld 사이클은 **물리 양수 시작스위치(SW_START1/2)로만** 열린다 — **패널/Modbus START는 직접 초음파(hand/comm) 경로로 사이클 아님, 현 STM32 거동 무수정**. 물리 스위치는 물리입력 → 슬라이스4이므로, **슬라이스1은 사이클 FSM 로직만 구축하고 프로덕션 트리거는 미와이어**(FSM은 host 테스트로 검증, HW E2E는 슬라이스4). work_cnt++는 PC HMI "사이클 종료" 의미를 **설계로 확정**(런타임 증가는 슬라이스4 물리 트리거 후). 충실도 = 혼합(거동 samd20 충실 + 명백한 버그만 수정, quirk는 문서화). 시간 단위 = **10ms/count**.

---

## 1. 목표 & 범위

### 1.1 컨텍스트
- 제어 대상 = GD-SONIC 공압 프레스 초음파 용접기. samd20 `main.c`가 `sys_status==SYS_RUN`에서 5상태 FSM으로 실린더 하강→용접→유지→상승을 자동 구동(`ref/samd20/main.c:1450-1632`).
- 현재 STM32엔 **초음파 게이트만** 존재(`app_reg.c` `us_run_status` IDLE/TOUCH/COMM + on-time ceiling). 프레스 사이클(바깥 머신)은 미포팅 — 본 스테이지가 추가.
- 충실도 입장: **혼합** — 거동은 samd20 충실(comp_time 진폭보정 등 quirk 포함), 명백한 copy-paste 버그만 수정, quirk는 §8에 문서화·HW 리뷰 플래그.

### 1.2 In scope (슬라이스 1)
- **신규 모듈**: `app_weld_fsm.{c,h}`(HAL-free 순수 코어) + `app_weld.{c,h}`(글루).
- **DELAY 모드 FSM**: `RUN_READY→CYL1→WELD→HOLD→CYL2→RUN_READY`. 모든 전이는 `temp_time` 카운트다운(10ms/count) 기반.
- **`energy_ctrl==false && multi_ctrl==false` 경로만** (WELD 종료 = `limit_delay_time2` 시간 만료).
- **WELD↔초음파 게이트 통합**: 신규 소스 `US_CYCLE`로 `app_reg_command(START/RUN_RELEASE, US_CYCLE)`. `app_reg.c` 최소 수정(enum 추가 + ceiling에서 US_CYCLE 제외).
- **진폭 계산**: `temp_i = ((output_power-50)*255)/100`, `comp_time<7`이면 `temp_i -= (7-comp_time)*10` (samd20 충실, **0..127 DAC 도메인**). samd20은 이 보정된 `temp_i`를 **I2C_POT에 직접** 쓴다(`i2c_adc_write_byte(I2C_POT, ..., temp_i)`, main.c:1549). → **전용 weld raw-DAC hook `app_weld_hook_set_amp(uint8_t dac)` 신설**(슬라이스1 로그 스텁). ⚠ 기존 `app_lcd_hook_set_pot`은 인자를 **output_power(50..100)로 받아 내부에서 `(x-50)*255/100`을 적용**(app_lcd.c:28)하므로 재사용 불가 — 보정된 DAC를 넘기면 **이중 변환**(op=100→127→196). §6 참조.
- **사이클 완료 부수효과**: `work_cnt++` → FRAM 영속(`app_config_save_all` 패턴) → LCD `LV_WORK_CNT` 갱신.
- **물리 hook(로그만)**: `SOL_DN` on/off (신규 hook). 실 GPIO 미구동.
- **트리거 = 미와이어**(§5.3): 사이클 트리거는 물리 SW_START1/2(슬라이스4). 슬라이스1은 `app_weld_request_start()` 공개 API만 정의(슬라이스4 합류점). **패널/Modbus START는 직접 초음파 경로로 무수정** — 사이클 아님.
- **host 테스트**: FSM 코어의 상태전이·타이밍·comp_time·work_cnt를 `fw/test`에서 검증(코어 `start` 직접 주입).

### 1.3 Out of scope (deferred — 후속 슬라이스)
| 항목 | 슬라이스 |
|---|---|
| `energy_ctrl` 종료 + 에너지 적분(`curr_energy=acc/500`, DISP_ENERGY) | 2 (host-test) |
| `multi_ctrl` 다단 진폭 스테핑(`multi_ctrl_stage`, limit_mo_*) | 3 (host-test) |
| **TRIGGER 모드** + 위치센서(`re_dn_pressed`/`re_up_pressed` = SENSE_DN/UP) | 4 (HW-gated) |
| **물리 GPIO 드라이브**: SOL_DN(PB5) 실구동 + 극성, OSC 초음파(CTRL_OSC0-4 = B-SEAM) | 4 (HW-gated) |
| **안전 abort**: CYL1 중 `f_safty && (re_start1\|\|re_start2)` → SOL_DN OFF, READY 복귀 (SW_START1/2 물리입력) | 4 (HW-gated) |
| 물리 start 스위치(`SW_START1/2`) + `in_cycle` 재-arm 로직 (사이클 트리거) | 4 (HW-gated) |
| **SETUP 게이트**(사이클 진행 중 setup 진입 시 정지 — §5.4) | 4 (사이클 런타임 동반) |
| `overload`/`weld error`(ERR_OVLD/OUTERR) → SYS_ERROR 전이 | 별도(overload 스테이지) |

> 슬라이스1은 물리 트리거 없이 **FSM 코어가 `start` 주입 → DELAY 타이머 시퀀스 완주 → work_cnt++**까지를 **host-test로 검증**한다. HW에선 사이클이 휴면(READY) 상태로 idle, 기존 직접-초음파(패널/Modbus START)는 무회귀. 사이클 HW E2E는 슬라이스4(물리 SW_START + SOL_DN/센서)에서.

---

## 2. 아키텍처

### 2.1 모듈 분리 (프로젝트 관례: 작은 응집 파일 + 순수 compute는 HAL-free host-test)

```
app_weld_fsm.{c,h}   HAL-free 순수 FSM 코어 (app_reg_calc.c / app_modbus_core.c 패턴)
  - 상태: run_status, f_status_start, temp_time, comp_time
  - step(in) -> out events. sys_tick/HAL/config 전역 의존 없음.
  - fw/test/test_app_weld_fsm.c 에서 검증.

app_weld.{c,h}       얇은 글루 (app_reg.c / app_modbus.c 패턴)
  - 10ms 게이트로 코어 step 호출 (temp_time 감소 + FSM 전진)
  - out events -> 물리 hook(SOL_DN, 진폭 pot) + app_reg_command(US_CYCLE) + work_cnt/FRAM/LCD
  - live config(app_lcd_cfg) 읽기, START 트리거 진입점 제공.
```

### 2.2 순수 코어 인터페이스 (`app_weld_fsm.h`)

```c
/* run_status (samd20 verbatim) */
enum { WELD_READY=0, WELD_CYL1=1, WELD_WELD=2, WELD_HOLD=3, WELD_CYL2=4 };

/* step 입력: 호출자가 live config + 트리거를 주입 (app_reg의 limit_on_time 주입 패턴) */
typedef struct {
    uint8_t  start;              /* 1 = start_key edge (READY에서만 소비) */
    uint8_t  run_mode;           /* MODE_DELAY=0 (슬라이스1은 DELAY만 처리) */
    uint16_t limit_delay_time1;  /* CYL1/CYL2 시간 (10ms 단위) */
    uint16_t limit_delay_time2;  /* WELD 시간 */
    uint16_t limit_delay_time3;  /* HOLD 시간 */
    uint8_t  output_power;       /* 진폭 베이스 (50..100) */
} weld_in_t;

/* step 출력: 글루가 부수효과로 변환 (이벤트 플래그) */
typedef struct {
    uint8_t  run_status;         /* 갱신된 상태 (관측/트레이스용) */
    uint8_t  sol_dn;             /* 현재 솔레노이드 요청 레벨 (1=ON,0=OFF) */
    uint8_t  weld_start;         /* 1 = WELD 진입 엣지 (US_CYCLE START + pot write) */
    uint8_t  weld_stop;          /* 1 = WELD 종료 엣지 (US_CYCLE RUN_RELEASE) */
    uint8_t  amplitude;          /* weld_start 시 유효: comp_time 보정된 0..127 raw DAC(pot) 값 */
    uint8_t  cycle_done;         /* 1 = CYL2 완료 엣지 (work_cnt++) */
} weld_out_t;

void weld_fsm_init(void);
/* 10ms마다 1회 호출. 내부에서 temp_time 1 감소(>0일 때) 후 상태 평가. */
void weld_fsm_step(const weld_in_t *in, weld_out_t *out);
uint8_t weld_fsm_status(void);   /* 현재 run_status (글루/disp용) */
```

> `weld_fsm_step`이 temp_time 감소를 직접 소유(samd20은 timer가 `sys_status!=SETUP`일 때 10ms마다 `temp_time--`; STM32는 10ms 게이트로 step 진입 시 감소 — 동일 cadence, 단일 소유자). SETUP 게이팅은 글루가 step 호출 자체를 막아 재현(§5.4).

---

## 3. 상태 모델 (DELAY 모드, energy/multi off — samd20 1450-1632 충실)

시간 단위 = **10ms/count**. `f_status_start` = 상태별 "최초진입 처리 완료" 플래그(전이 시 0으로 리셋).

| 상태 | 최초진입 (f_status_start==0) | 이후 (전이 조건) |
|---|---|---|
| **READY(0)** | — | `start==1` → CYL1, `temp_time=limit_delay_time1`, (f_status_start는 0 유지 → CYL1 최초진입 발동), 사이클 시작 |
| **CYL1(1)** | f_status_start=1; **SOL_DN ON** | `temp_time==0` → f_status_start=0, **WELD**; weld 시간 세팅: `limit_delay_time2>6` ? {temp_time=ldt2, comp_time=7} : {comp_time=ldt2, temp_time=7} |
| **WELD(2)** | f_status_start=1; `temp_i=((output_power-50)*255)/100`; `comp_time<7`이면 `temp_i-=(7-comp_time)*10`; **weld_start 엣지(US_CYCLE START + amplitude=temp_i)**; max_amp/max_power 리셋은 게이트의 START가 처리(기존 app_reg) — energy accum 리셋은 슬라이스2 | `temp_time==0` → **weld_stop 엣지(US_CYCLE RUN_RELEASE)**, f_status_start=0, **HOLD**, `temp_time=limit_delay_time3` |
| **HOLD(3)** | f_status_start=1 | `temp_time==0` → f_status_start=0, **CYL2**, `temp_time=limit_delay_time1` |
| **CYL2(4)** | f_status_start=1; **SOL_DN OFF** | `temp_time==0` → f_status_start=0, **READY**, **cycle_done 엣지(work_cnt++)** |

**전이 다이어그램**:
```
READY --start--> CYL1 --ldt1--> WELD --ldt2--> HOLD --ldt3--> CYL2 --ldt1--> READY(work_cnt++)
        SOL_DN ON ^                US ON  US OFF              SOL_DN OFF
```

**comp_time quirk (samd20 충실, §8)**: 요청 weld 시간(`limit_delay_time2`)이 6 이하(≤60ms)면, 최소 7카운트(70ms) 동안 용접하되 진폭을 `(7-comp_time)*10`만큼 감쇠. 즉 매우 짧은 용접은 시간 대신 진폭으로 보정. **버그 아님 — 의도된 거동, 1:1 포팅.**

---

## 4. 데이터 흐름 & 타이밍

1. **superloop**가 `app_weld_tick()`를 매 루프 호출(`app_reg_tick`/`app_modbus_tick` 옆).
2. `app_weld_tick`이 **10ms 게이트**(`sys_tick_get_ms()` 델타) — 게이트 통과 시:
   a. live config에서 `weld_in_t` 채움(run_mode, limit_delay_time1/2/3, output_power) + 보류 중인 start 래치 반영(`app_weld_request_start()`가 세팅 — 슬라이스1엔 프로덕션 호출자 없음, 슬라이스4 물리 스위치가 합류; §5.3).
   b. `weld_fsm_step(&in, &out)` 호출(내부에서 temp_time 감소 + 상태 평가).
   c. `out` 이벤트 → 부수효과:
      - `out.sol_dn` 변화 → `app_weld_hook_sol_dn(on)` (슬라이스1 로그).
      - `out.weld_start` → `app_weld_hook_set_amp(out.amplitude)`(raw DAC 직접) + `app_reg_command(US_CMD_START, US_CYCLE)`.
      - `out.weld_stop` → `app_reg_command(US_CMD_RUN_RELEASE, US_CYCLE)`.
      - `out.cycle_done` → `cfg->work_cnt++`, FRAM save, `app_lcd_set_work_cnt(cfg->work_cnt)`.
3. (SETUP 게이트는 슬라이스4 이연 — §5.4. 슬라이스1 글루는 무조건 step 호출.)

> 10ms cadence는 samd20 timer(`tick_1ms>=10`)와 동일. app_reg는 2ms/1ms로 독립 — weld_fsm은 별도 10ms 게이트(서로 간섭 없음, 단일스레드 superloop).

---

## 5. 초음파 게이트 통합 (app_reg 최소 수정)

### 5.1 신규 소스 `US_CYCLE`
`app_lcd.h` enum 확장: `{ US_IDLE=0, US_REMOTE=1, US_TOUCH=2, US_COMM=3, US_CYCLE=4 }`. (REMOTE는 물리 스위치/IPC용 예약 유지 — weld-cycle은 별개 소스.)

### 5.2 `app_reg_command` / ceiling 수정
- `US_CMD_START` 가드는 그대로 `==US_IDLE`(중재 공짜): WELD 중 게이트 non-IDLE → 패널/Modbus START 무시.
- `US_CMD_RUN_RELEASE`는 source-matched 그대로: COMM/TOUCH STOP이 US_CYCLE 런을 못 죽임. weld_stop은 `US_CYCLE` 소스로 RELEASE.
- **on-time ceiling 블록에서 US_CYCLE 제외**: 현재 `(rs==US_TOUCH)||(rs==US_COMM)`에 US_CYCLE 미포함 → WELD 길이는 FSM의 `limit_delay_time2`가 지배(ceiling 무관). (자연히 제외됨 — 명시 주석 추가.)
- peak 래치(last_power/last_amp), STATUS bit0(MB_STATUS_US), curr/max publish는 **무수정 재사용** — US_CYCLE 런도 동일하게 표시·미러됨.

### 5.3 START 라우팅 (samd20 충실 — 2026-06-14 사용자 확정)
samd20 충실 구조 확정: **weld 사이클은 물리 양수 시작스위치(SW_START1/2)로만 열린다** (`re_start1==0 && re_start2==0 && in_cycle==0` → `start_key_pressed`, main.c:1404-1466). **패널 START(KEY_MULTI)·Modbus START 레지스터는 직접 초음파(hand/comm) 경로이며 사이클이 아니다** — 현재 STM32 거동(`app_reg` US_TOUCH/US_COMM + on-time ceiling)을 **그대로 유지·무수정**.

→ 초음파가 켜지는 경로 **두 가지가 공존**:
1. **직접(hand/comm)**: 패널/Modbus START → `app_reg_command(START, US_TOUCH|US_COMM)` → 즉시 초음파 + on-time ceiling. **기존, 본 스테이지 무수정.**
2. **사이클(press)**: 물리 SW_START1/2 → CYL1 → WELD(`app_reg_command(START, US_CYCLE)`) → HOLD → CYL2. **본 스테이지가 추가하는 머신.**

- **사이클 트리거(물리 SW_START1/2)는 물리입력 → 슬라이스4(HW-gated)**. 따라서 **슬라이스1은 사이클 FSM 로직만 구축하고 프로덕션 트리거를 와이어하지 않는다**. 사이클 FSM은 host 테스트로 검증(코어에 `start=1` 직접 주입). 사이클 HW E2E는 슬라이스4에서 물리 스위치(+ `in_cycle` 재-arm, 양손 AND, 안전 abort)와 함께.
- 글루는 `app_weld_request_start()` **공개 API**를 슬라이스1에 정의(슬라이스4 물리 스위치 폴/ISR이 호출할 합류점). one-shot 래치 시맨틱(§4.2: 보류 플래그 → 다음 step 1회 `in.start=1` → 클리어, READY 아니면 소실)은 슬라이스4를 위해 구현·유지하나 **슬라이스1엔 프로덕션 호출자 없음**(host 테스트는 코어를 직접 구동).
- 패널/Modbus START·V30 quirk·swallow_start = **무수정**(직접 경로 잔재, 본 스테이지 무관).

### 5.4 SETUP 게이트 — **슬라이스4로 이연**
samd20은 `sys_status!=SYS_SETUP`일 때만 `temp_time--`(사이클 진행). 그러나 STM32엔 `sys_status`가 실제 유지되지 않고(struct 필드만 존재, SYS_* define 미포팅) 신뢰할 SETUP 표시자가 없다. **슬라이스1은 프로덕션 트리거가 없어 HW에서 사이클이 절대 실행되지 않으므로**(READY 휴면) SETUP 게이트는 무의미 → **슬라이스4(사이클이 실제 도는 시점)로 이연**. 그때 `lcd_status`의 SETUP_* 페이지 범위 체크 또는 sys_status 포팅으로 구현. 슬라이스1 글루는 무조건 step 호출.

---

## 6. Hook 인터페이스 (물리 결합 — 슬라이스1 로그 스텁)

| hook | 호출 시점 | 슬라이스1 | 후속 |
|---|---|---|---|
| `app_weld_hook_sol_dn(bool on)` | CYL1 진입(ON) / CYL2 진입(OFF) | mon 로그만 | 4: PB5 실 GPIO + 극성 확정 |
| `app_weld_hook_set_amp(uint8_t dac)` | WELD 진입(comp_time 보정 진폭) | 신규 raw-DAC 로그 스텁 | B-SEAM: 실 I2C_POT |
| (초음파 OSC 구동) | US_CYCLE START가 게이트 set | 게이트 상태만(STATUS bit0) | 4/B-SEAM: CTRL_OSC0-4 실 GPIO |

> 신규 hook은 기존 `app_lcd_hook_*`와 동일 패턴(바디는 `mon_printf`만). 실 하드웨어 드라이브는 슬라이스4/B-SEAM.
>
> ⚠ **`app_weld_hook_set_amp(dac)` vs `app_lcd_hook_set_pot(output_power)` 입력 도메인 주의**: 둘 다 결국 같은 I2C_POT를 향하지만 입력 도메인이 다르다 — set_amp는 **이미 변환된 raw DAC(0..127)**, set_pot은 **output_power(50..100)를 받아 내부 변환**. B-SEAM에서 실 I2C_POT 연결 시 둘을 잘못 병합하면 이중/누락 변환 발생. weld는 comp_time 감쇠를 **DAC 도메인**에서 하므로(output_power로 역산 불가) raw 경로가 필수.

---

## 7. work_cnt 증가 + 영속

samd20: CYL2 완료에서 `work_cnt++; save_word_fram(ADDR_WORK_CNT); send_lcd_data_word(LV_WORK_CNT)`.

STM32 슬라이스1:
- `out.cycle_done` 엣지 → `cfg->work_cnt++` (app_lcd_cfg, u32).
- FRAM 영속: 프로젝트 패턴 `app_config_save_all()` (Modbus work_cnt 리셋이 쓰는 동일 경로).
- LCD: `app_lcd_set_work_cnt(cfg->work_cnt)` (기존 함수).
- **Modbus 미러 자동**: `app_modbus.c mirror_live`가 매 tick `cfg->work_cnt`를 WORK_CNTH/L에 미러 → PC HMI가 FC03로 읽음. 슬라이스1은 work_cnt 증가 **로직**을 확정(PC HMI "사이클 종료" 의미 = 설계 확정); **실제 런타임 증가는 슬라이스4 물리 트리거로 사이클이 완주해야** 발생.

---

## 8. 충실도 노트 & quirk (혼합 입장)

- **comp_time 진폭보정**(§3): 짧은 weld 시간의 의도된 거동. 1:1 포팅, 주석 문서화.
- **READY→CYL1 시 f_status_start 미설정**(samd20 `//f_status_start=1;` 주석처리): 의도. CYL1 최초진입에서 SOL_DN ON 발동하도록 0 유지 — 충실 재현.
- **수정할 명백한 버그 1건 — 진폭 언더플로 가드**: samd20 `temp_i = ((output_power-50)*255)/100; if(comp_time<7) temp_i -= (7-comp_time)*10;`에서 `temp_i`(uint8_t)는 저전력+짧은용접(예 output_power=50→amp 0, comp_time=0→감쇠 70)에서 **언더플로→큰 진폭**(안전상 위험). 혼합 입장상 **수정**: 코어에서 amp를 uint16_t로 계산 후 `amp = (amp >= reduction) ? amp-reduction : 0;` 클램프. samd20의 wrap 거동과 의도적 차이 — §11에 플래그.
- samd20 FRAM 주소 copy-paste 버그(ADDR_TRIGGER2/DELAY2)는 Stage C에서 이미 save_all로 해소 — 본 슬라이스 무관.
- **us_on_time 리셋**: samd20 WELD 진입 시 `us_on_time=0`. STM32는 app_reg가 START 엣지에서 `us_on_time_200m=0` 처리(기존) — US_CYCLE START도 동일 경로라 자동.

---

## 9. 테스트 (host, `fw/test/test_app_weld_fsm.c`)

순수 코어이므로 HAL 없이 검증(app_reg_calc/modbus_core 테스트 패턴):
1. **완전 사이클(DELAY)**: start=1 주입 → N×10ms step → 이벤트 시퀀스 검증: SOL_DN ON(CYL1) → weld_start(amplitude 정확) → weld_stop(ldt2 후) → SOL_DN OFF(CYL2) → cycle_done(ldt1 후). 각 상태 지속시간 = 해당 limit 시간.
2. **타이밍 정확도**: limit_delay_time1/2/3 각 값에 대해 상태 전이가 정확히 `time×10ms`(=time 스텝) 후 발생.
3. **comp_time quirk**: `limit_delay_time2<=6` → temp_time=7 스텝 동안 WELD + amplitude 감쇠(`(7-comp_time)*10`) 검증; `>6` → 감쇠 없음.
4. **진폭 계산**: `(output_power-50)*255/100`, output_power 50/75/100 → temp_i **0/63/127**(op 50..100 → pot 0..127). + 언더플로 가드: 저전력+짧은용접(예 op=50, comp_time=0 → 0-70) → amp=0 클램프(§8).
5. **start 무시**: WELD/HOLD/CYL2 중 start=1 재주입은 무시(READY에서만 소비).
6. **cycle_done은 1틱 엣지**: 다음 step에서 0으로 클리어(중복 work_cnt++ 방지).

> 글루(app_weld.c)는 HAL/hook 결합이라 host-test 비대상 — 통합 cpp-reviewer + HW mon 트레이스로 검증(프로젝트 패턴).

---

## 10. 빌드/검증 게이트

- **호스트(슬라이스1 주 게이트)**: `env -u STM32_TOOLCHAIN cmake --build fw/build` 0-warning + `make -C fw/test test` 신규 스위트 PASS. FSM 코어의 전 시퀀스·타이밍·comp_time·work_cnt 검증.
- **HW(보드, 슬라이스1)**: 프로덕션 사이클 트리거가 없으므로(§5.3) **회귀 없음 확인 위주** — 플래시 부팅 정상, `app_weld_tick`이 READY 휴면, **기존 직접-초음파(패널/Modbus START → ICON_RUN/ceiling)가 무회귀**. (선택) 임시 디버그 트리거(`#ifdef WELD_DEBUG_TRIG`로 Modbus 미사용 reg 1개를 `app_weld_request_start()`에 임시 연결)로 mon 트레이스 사이클 시퀀스 스모크 가능 — 슬라이스1 머지 전 제거.
- **사이클 HW E2E는 슬라이스4**(물리 SW_START + SOL_DN/센서 실구동).
- 머지: `--no-ff` 로컬, 태그 `hw-revA_fw-stage-weld1`.

---

## 11. 미해결/HW 확인 항목

1. ~~START 모드 게이팅~~ **해소(2026-06-14, 사용자 확정 — samd20 충실)**: weld 사이클은 **물리 SW_START1/2로만** 트리거. **패널/Modbus START는 직접 초음파(사이클 아님), 무수정**(§5.3). 두 초음파 경로(직접/사이클) 공존.
2. **SOL_DN 극성**(PB5): active-HIGH/LOW — 슬라이스4 실구동 시 측정.
3. **사이클 중 STOP/취소**: 패널/Modbus가 진행 중 사이클을 중단하는 경로가 필요한지(samd20엔 SYS_ERROR/ESTOP 경로뿐 — 정상 중단은 deferred). 슬라이스1은 사이클 완주 가정.
4. **work_cnt FRAM 마모**: 매 사이클 save_all → FRAM(마모 무한 가정이나 확인). 사람 페이스라 무해.

---

*작성: 2026-06-14 weld-cycle 스테이지 브레인스토밍. samd20 참조 `ref/samd20/main.c:1450-1632` + temp_time 10ms cadence(5313-5334). 충실도=혼합. 다음 = 본 spec 사용자 리뷰 → writing-plans.*
