# Stage Weld-Cycle — Slice 2: Energy-based WELD Exit Design

> **요약**: weld-cycle 슬라이스1(DELAY 시간-exit)에 이어, **WELD 종료를 시간(`limit_delay_time2`) 대신 에너지 도달(`energy_ctrl==true && curr_energy>=limit_energy`)로** 전환하는 슬라이스. samd20 4개 지점(누산 `main.c:434-436`, WELD 진입 리셋 `1555`, 시간-exit 게이트 `1560`, 에너지-exit + overtime `5270-5294`)을 포팅. **아키텍처 = Option A**: 에너지 누산(`acc_energy`/`curr_energy`)은 **`app_reg`에 둔다**(samd20 ADC-ISR과 동형); FSM은 순수 유지하고 `curr_energy`를 **주입받아 비교만**. **WELD 진입 리셋은 공짜** — WELD 진입이 이미 호출하는 `app_reg_command(US_CMD_START, US_CYCLE)` 핸들러(run-start 엣지)에 `acc_energy=0` 한 줄 추가. **load-bearing 결정 = 누산 rate**: `limit_energy`가 이미 외부 계약(Modbus reg 0x08·LCD)이라 누산 rate가 다르면 같은 설정값이 다른 용접 에너지가 됨 → 에너지 산술을 **`app_reg_calc.{c,h}` 순수 헬퍼로 추출**해 `test_app_reg_calc.c`에 끌어들임(절대 타이밍은 6b 캘리브레이션/HW-gated, 적분 **구조**는 host-test). **overtime backstop은 필수(블로커)**: energy_ctrl이면 시간-exit가 통째로 스킵되므로 에너지 미도달 시 WELD 무한 지속 → `limit_out_time`(이미 cfg 포팅, 기본 10초) 안에 미도달 시 **abort**(US 정지 + 실린더 상승 + READY 복귀 + **work_cnt++ 안 함** + `weld_fault` 1-step 엣지). 에러 *표시*(SYS_ERROR)는 후속 에러 슬라이스로 이연(fault는 플래그/mon만). **범위 = weld-cycle WELD 전용**(2026-06-14 사용자 확정); samd20 `5270`이 US_COMM/US_REMOTE 직접런에도 적용하는 부분은 이연. 충실도 = 혼합. 시간 단위 = WELD FSM **10ms/count**.

---

## 1. 목표 & 범위

### 1.1 컨텍스트
- 슬라이스1이 DELAY 모드 시간 기반 사이클(`READY→CYL1→WELD→HOLD→CYL2→work_cnt++`)을 main에 머지(`hw-revA_fw-stage-weld1`). WELD 종료는 `limit_delay_time2` 시간 만료 뿐.
- samd20 실제 거동: `energy_ctrl==true`면 **WELD를 시간이 아니라 에너지 도달로 종료**한다. 에너지 = 출력 전력의 시간 적분. 저에너지(under-weld) 방지 + 일정 용접 품질 보장이 목적.
- STM32 현황: `limit_energy`/`energy_ctrl`/`limit_out_time` **config는 이미 포팅됨**(`app_config`, LCD `app_lcd_input.c:758/770`·`app_lcd_render.c:66-112`, Modbus reg `ENERGY 0x08`/`EN_ENERGY 0x14`/`TIMEOVER`/`DISP_ENERGY 0x16`). 그러나 **누산(curr_energy 항상 0)·에너지-exit·시간-exit 게이트는 미포팅** = 본 슬라이스.
- 충실도 입장: **혼합** — 거동은 samd20 충실, 명백한 버그만 수정, quirk·deviation은 §8/§11 문서화.

### 1.2 In scope (슬라이스 2)
- **에너지 누산 (`app_reg`)**: active run 동안 `acc_energy += curr_power`(매 publish tick); `curr_energy = reg_energy_from_acc(acc_energy)`. WELD 진입(US_CMD_START US_CYCLE) 시 리셋. samd20 `434-436`·`1555`.
- **순수 에너지 헬퍼 (`app_reg_calc`)**: `reg_energy_from_acc()` 추출 + `test_app_reg_calc.c` host-test(적분 구조 + limit 도달 step 수). `reg_scale`/`reg_on_time_200m` 선례.
- **FSM 에너지-exit + backstop (`app_weld_fsm`)**: `weld_in_t`에 `energy_ctrl`/`curr_energy`/`limit_energy`/`limit_out_time` 추가, `weld_out_t`에 `weld_fault` 추가. WELD 상태에 분기:
  - `energy_ctrl`: `curr_energy>=limit_energy` → 정상 종료(weld_stop→HOLD). / backstop 만료 → abort(weld_stop+weld_fault→READY).
  - `!energy_ctrl`: 기존 슬라이스1 시간-exit 그대로(무회귀).
- **시간-exit 게이트**: `energy_ctrl`이면 `limit_delay_time2` 시간-exit 스킵(samd20 `1560` 충실). backstop(`limit_out_time`)이 유일한 시간 한계.
- **glue (`app_weld`)**: `weld_in_t`에 cfg 3필드 + `app_reg_measure()->curr_energy` 주입. `weld_fault` → 신규 `app_weld_hook_fault()`(mon 로그만).
- **last_energy 래치 (`app_reg`)**: run-stop(US_CMD_RUN_RELEASE / on-time ceiling)에서 기존 last_power/last_amp 옆에 `last_energy=curr_energy` 추가(stopped-display 미러 정확 — advisor 마이너).
- **host 테스트**: FSM energy-exit·게이트 스킵·backstop abort·슬라이스1 회귀 + 헬퍼 known-vector.

### 1.3 Out of scope (deferred — 후속 슬라이스)
| 항목 | 슬라이스 / 게이트 |
|---|---|
| 에러 표시 머신(`SYS_ERROR`/`ERR_OVTIME` LCD·상태 전이) — fault는 플래그/mon stub만 | 별도(에러/overload 스테이지) |
| **COMM/TOUCH 직접런 energy-gating** (samd20 `5270`의 `US_COMM\|US_REMOTE` 분기) | 별도(범위 결정 — §6 deviation) |
| `multi_ctrl` 다단 진폭 스테핑 | 3 (host-test) |
| **절대 에너지 보정**(curr_power=adc_scaled_value 물리단위, divisor 실측 미세조정) | 6b / HW (실 초음파 rig + 스코프) |
| 물리 트리거(SW_START)·실 SOL_DN/센서·안전 abort | 4 (HW-gated) |

> 슬라이스2는 **에너지-exit 로직 + 누산 산술**을 host-test로 검증한다. HW에선 (슬라이스4 물리 트리거가 없으므로) 사이클 자체는 여전히 휴면 — 단, 누산은 *모든* active run에서 동작하므로 직접-초음파 런의 `DISP_ENERGY`/`VAR_ENERGY`가 이제 채워지는 것은 HW 회귀확인 대상(§10).

---

## 2. 아키텍처 (Option A — 누산은 app_reg, FSM은 읽기 전용)

### 2.1 책임 분리

```
app_reg_calc.{c,h}   순수 에너지 산술 (host-test 대상)
  - reg_energy_from_acc(acc) -> curr_energy = acc / REG_ENERGY_DIV
  - reg_scale/reg_on_time_200m 와 같은 파일/패턴

app_reg.{c,h}        에너지 누산 소유 (HAL 결합, host-test 비대상)
  - g_reg.acc_energy (uint32), g_reg.last_energy (uint32)
  - US_CMD_START: acc_energy=0, g_measure.curr_energy=0 (리셋, run-start 엣지)
  - reg_publish_measure: active면 acc_energy += curr_power; curr_energy = reg_energy_from_acc(acc)
  - run-stop(RUN_RELEASE / ceiling): last_energy = curr_energy 래치

app_weld_fsm.{c,h}   순수 FSM (host-test 대상) — curr_energy 주입받아 비교만
  - weld_in_t += energy_ctrl, curr_energy, limit_energy, limit_out_time
  - weld_out_t += weld_fault
  - WELD 진입/step 분기 (§3)

app_weld.{c,h}       글루 — cfg 3필드 + app_reg_measure()->curr_energy 주입, fault hook
```

### 2.2 누산 위치 = run-start 리셋이 공짜인 이유
WELD 진입은 이미 `app_weld.c`에서 `app_reg_command(US_CMD_START, US_CYCLE)`를 호출한다(슬라이스1). 그 핸들러는 run-start 엣지에서 `max_power=0; max_amp=0; run_start_ms=now`를 이미 수행 — 여기에 `acc_energy=0; curr_energy=0`를 추가하면 samd20의 run-start 리셋(`1340`/`1366` COMM/TOUCH start, `1555` WELD 진입)이 **전부 같은 엣지**라 한 곳에서 충실 재현된다. FSM이 별도 "리셋 이벤트"를 emit할 필요 없음.

### 2.3 부수효과 (의도) — 누산은 모든 active run에서
누산이 `reg_publish_measure`의 `active` 분기에 들어가므로, curr_energy는 weld뿐 아니라 **모든 active run(US_TOUCH/US_COMM/US_CYCLE)**에서 차오른다. → LCD `VAR_ENERGY`·Modbus `DISP_ENERGY`(reg 0x16)가 이제 실제 표시됨(현재 항상 0). **samd20 충실**(curr_energy를 모든 런에서 표시·미러). 단 **에너지-EXIT는 weld-cycle WELD에만** 적용(§6 범위). 누산=보편 / exit=weld-only.

---

## 3. FSM 상태 모델 변경 (WELD 분기, `s_temp_time` 재사용)

슬라이스1 대비 **WELD 상태만** 변경. READY/CYL1/HOLD/CYL2는 무수정.

### 3.1 인터페이스 추가 (`app_weld_fsm.h`)
```c
typedef struct {
    /* ... 슬라이스1 필드 그대로 ... */
    uint8_t  energy_ctrl;        /* 1 = WELD 종료를 에너지로 (시간-exit 스킵) */
    uint32_t curr_energy;        /* app_reg 누산 라이브 값 (글루 주입) */
    uint32_t limit_energy;       /* 에너지 목표 (cfg, 외부 계약 reg 0x08) */
    uint16_t limit_out_time;     /* backstop 한계 (cfg, 초 단위; samd20 *10@100ms) */
} weld_in_t;

typedef struct {
    /* ... 슬라이스1 필드 그대로 ... */
    uint8_t  weld_fault;         /* 1 = WELD backstop abort 엣지 (글루 fault hook) */
} weld_out_t;
```

### 3.2 CYL1→WELD 전이 (CYL1 상태에서 `s_temp_time==0`) — comp_time / temp_time 디커플
슬라이스1은 이 전이에서 `comp_time`(진폭 보정)과 `s_temp_time`(WELD 시간)을 **커플링**해 설정한다(`ldt2>6 ? {temp=ldt2,comp=7} : {comp=ldt2,temp=7}`). 슬라이스2는 둘을 **분리**:
- **`comp_time`은 항상 `limit_delay_time2`에서 유도**(진폭 보정 — samd20은 energy 모드에서도 comp_time 진폭 보정을 적용; `main.c:1546-1547`은 energy_ctrl 무관):
  `s_comp_time = (ldt2 > 6) ? WELD_COMP_FULL : ldt2;`
- **`s_temp_time`만 모드별 분기**:
  `s_temp_time = energy_ctrl ? limit_out_time_to_ticks(limit_out_time) : ((ldt2 > 6) ? ldt2 : WELD_COMP_FULL);`

→ `energy_ctrl==0`이면 슬라이스1과 **완전 동일**(comp_time/temp_time 동일 값). `energy_ctrl==1`이면 comp_time은 진폭용으로 유지하되 s_temp_time은 backstop 카운트다운으로 전용.

> **단위 변환**: samd20 backstop = `us_on_time >= limit_out_time*10`, `us_on_time`은 100ms 단위(`main.c:5380`) → 한계 = `limit_out_time` 초. FSM은 10ms/tick → `limit_out_time_to_ticks = limit_out_time * 100`(limit_out_time max 10 → 1000 tick, uint16 OK). 순수 헬퍼/매크로로 두고 주석에 samd20 cadence 인용.

> WELD 최초진입(`f_status_start==0`)의 amplitude/weld_start/US_CYCLE START emit은 슬라이스1 그대로(comp_time이 위에서 이미 세팅됨).

### 3.3 WELD step (`f_status_start==1`)
```
if (energy_ctrl):
    if (curr_energy >= limit_energy):           # 정상 종료 (samd20 5272)
        weld_stop = 1; -> HOLD; s_temp_time = limit_delay_time3
    elif (s_temp_time == 0):                      # backstop abort (samd20 5288)
        weld_stop = 1; weld_fault = 1;
        s_sol_dn = 0;                             # 실린더 상승
        -> READY                                  # work_cnt++ 없음 (cycle_done emit 안 함)
else:                                             # 슬라이스1 시간-exit (무회귀)
    if (s_temp_time == 0):
        weld_stop = 1; -> HOLD; s_temp_time = limit_delay_time3
```
- `s_temp_time` 감소는 슬라이스1과 동일하게 step 진입부에서 처리(energy 모드에선 backstop 카운트다운으로 의미만 바뀜).
- **abort는 weld_stop도 emit**(US_CYCLE RUN_RELEASE로 초음파 정지) + weld_fault(fault hook). 둘 다 같은 step에서 1.
- abort는 **CYL2를 거치지 않고 즉시 READY + SOL_DN OFF**(사용자 확정 — "실린더 상승 + 사이클 중단"). 제어된 상승(CYL2 ldt1)을 원하면 §11-2 재검토.

### 3.4 samd20 게이트(`1560`) 충실성
samd20 `if((multi_ctrl!=true)&&(energy_ctrl!=true))`로 시간-exit를 감싼 구조를, 위 `if(energy_ctrl)/else` 분기로 재현. `multi_ctrl`은 슬라이스3 — 본 슬라이스는 multi_ctrl=false 가정(분기에 미포함, 기존 슬라이스1 경로).

---

## 4. 에너지 산술 (순수 헬퍼 + 누산 cadence)

### 4.1 헬퍼 (`app_reg_calc.{c,h}`)
```c
/* 누산기 -> 표시 에너지. samd20 main.c:436 (curr_energy = acc_energy/500)
 * 구조 재현. divisor는 누산 cadence 보정 포함(§4.2). 절대 보정은 6b/HW. */
uint32_t reg_energy_from_acc(uint32_t acc_energy);
```
- host-test(`test_app_reg_calc.c`): known acc → curr_energy 산술; 일정 curr_power 주입 시 N step 후 curr_energy가 limit 교차하는 step 수가 기대와 일치.

### 4.2 cadence / divisor (load-bearing — §11-1 HW 확인)
- samd20: ADC complete-callback에서 `acc_energy += curr_power` ~1ms, `curr_energy = acc/500`. → 일정 전력에서 `curr_energy`는 `curr_power/500` per ms 증가.
- STM32: `curr_power(=adc_scaled_value)`는 publish gate(`REG_TICK_MS=2ms`)에만 fresh. → **publish tick에서 누산**, divisor를 2ms cadence로 보정(`REG_ENERGY_DIV=250`, samd20 1ms·/500의 절반 샘플 → 절반 divisor)해 **energy-per-second를 samd20과 일치**.
- ⚠ **절대 타이밍은 HW-gated**: `adc_scaled_value` 자체가 6b 미보정 + superloop publish는 "≥2ms"(하드타이머 아님). host-test는 적분 **구조**만, 절대 용접 시간 일치는 실 초음파 rig/스코프에서 divisor 미세조정(6b 동반). spec/코드에 정직히 명시.

---

## 5. app_reg 변경 (최소)

| 위치 | 변경 |
|---|---|
| `reg_state_t` | `uint32_t acc_energy; uint32_t last_energy;` 추가 |
| `app_reg_command` `US_CMD_START` | `g_reg.acc_energy=0; g_measure.curr_energy=0;` 추가(run-start 리셋, §2.2) |
| `app_reg_command` `US_CMD_RUN_RELEASE` (source-matched) | `g_reg.last_energy = g_measure.curr_energy;` 래치 추가(기존 last_power/amp 옆) |
| on-time ceiling stop (app_reg_tick) | 동일 `last_energy = curr_energy;` 래치 추가 |
| `reg_publish_measure` | `if(active){ g_reg.acc_energy += g_measure.curr_power; g_measure.curr_energy = reg_energy_from_acc(g_reg.acc_energy);}` (curr_power 계산 직후) |
| `reg_publish_measure` (stopped) | `g_measure.last_energy = g_reg.last_energy;` 미러(기존 last_power/amp와 동형) |

> `lcd_measure_t`에 `curr_energy`/`last_energy`는 **이미 존재**(`app_lcd.h:120`) — 필드 추가 불요, 값만 채움. Modbus `DISP_ENERGY` 미러(`app_modbus.c:76`)·LCD disp(`app_lcd_disp.c:167`)도 이미 curr/last_energy를 읽으므로 **자동 점등**.

---

## 6. 범위 경계 / deviation (spec 명시)

- ✅ **In**: weld-cycle WELD energy-exit + 누산(보편) + backstop abort + fault 플래그 stub + last_energy 래치.
- ⛔ **이연**: 에러 표시/SYS_ERROR 머신, COMM/TOUCH 직접런 energy-gating, multi_ctrl, 절대 에너지 보정, 물리 트리거.
- ⚠ **deviation 기록 (사용자 확정 — weld-only)**: samd20 `5270`은 `(US_COMM||US_REMOTE||RUN_WELD)&&energy_ctrl`에서 모두 에너지 정지. 본 슬라이스는 **weld-cycle WELD에만** 적용 → **energy_ctrl=ON 상태에서 Modbus/패널 직접 START(US_TOUCH/US_COMM)는 에너지로 자동정지하지 않고 기존 on-time ceiling으로 종료**. 레거시 제품과 동작 차이 — 의도된 슬라이스 경계. (직접런 energy-gating이 필요해지면 별도 슬라이스에서 app_reg 런 경로에 추가.)

---

## 7. Hook 인터페이스 (슬라이스2 추가)

| hook | 호출 시점 | 슬라이스2 | 후속 |
|---|---|---|---|
| `app_weld_hook_fault(void)` | WELD backstop abort 엣지 | mon 로그(`[weld] fault: energy timeout`) | 에러 슬라이스: SYS_ERROR\|ERR_OVTIME + LCD 에러 표시 |

> 기존 hook(sol_dn/set_amp)·`app_reg_command(US_CYCLE)` 무수정. fault hook은 1-step 엣지(`out.weld_fault`)에서만 호출.

---

## 8. 충실도 노트 & quirk

- **시간-exit 스킵(`energy_ctrl`)**: samd20 `1560` 충실 — energy 모드에선 `limit_delay_time2`가 WELD 길이에 무관(backstop만 시간 한계).
- **backstop = abort(에러), not graceful HOLD**: samd20 `5288`은 `us_off + SYS_ERROR | ERR_OVTIME`(사이클 정상 완주 아님). 본 슬라이스는 에러머신 미포팅이라 **abort→READY + fault 플래그 stub**로 의도(저에너지=불량 용접을 양품 work_cnt로 안 셈). 에러 표시는 이연.
- **누산 보편 / exit weld-only**: §2.3 — 충실(표시) + 슬라이스 경계(exit).
- **divisor cadence 보정**: §4.2 — samd20 1ms·/500을 2ms publish·/250로 재현. 절대값은 6b/HW.
- **curr_power source**: `g_measure.curr_power = active ? adc_scaled_value : 0`(슬라이스2b 무수정). 누산은 이 값을 적분 — idle엔 0이라 누산 정지(자연).

---

## 9. 테스트 (host)

### 9.1 `test_app_weld_fsm.c` 추가 (energy 시나리오)
1. **energy-exit 정상**: `energy_ctrl=1`, curr_energy를 step마다 증가 주입 → `curr_energy>=limit_energy`인 step에서 정확히 1회 `weld_stop`+HOLD 전이, 그 전엔 시간(ldt2) 무관하게 WELD 유지.
2. **시간-exit 스킵**: `energy_ctrl=1`, `limit_delay_time2` 작게(예 2) 줘도 curr_energy 미도달이면 WELD가 ldt2 후 종료 **안 함**(슬라이스1 회귀와 대비).
3. **backstop abort**: `energy_ctrl=1`, curr_energy=0 유지 → `limit_out_time*100` tick 후 `weld_fault`+`weld_stop` 1회, READY 복귀, `cycle_done`(work_cnt++) **미발생**, sol_dn OFF.
4. **슬라이스1 회귀**: `energy_ctrl=0` → 기존 시간-exit 시퀀스 완전 동일(기존 테스트 그대로 PASS).

### 9.2 `test_app_reg_calc.c` 추가 (에너지 산술)
5. **reg_energy_from_acc known-vector**: 알려진 acc 입력 → 기대 curr_energy(divisor 적용).
6. **적분 step 수**: 일정 curr_power 주입 시 curr_energy가 limit_energy 교차하는 step 수가 `limit_energy*DIV/curr_power` 근사와 일치(구조 검증, 절대 아님).

> 글루(app_weld.c)·app_reg 누산 결선은 통합 cpp-reviewer + HW mon으로 검증(프로젝트 패턴).

---

## 10. 빌드/검증 게이트

- **호스트(슬라이스2 주 게이트)**: `env -u STM32_TOOLCHAIN cmake --build fw/build` 0-warning + `make -C fw/test test` 5스위트(기존 4 + reg_calc 확장/weld 확장) PASS.
- **HW(보드, 회귀확인)**: 프로덕션 사이클 트리거 없음(슬라이스4) → **무회귀 확인 위주**. ① 기존 직접-초음파(패널/Modbus START → ICON_RUN/ceiling) 무회귀. ② **누산 점등 확인**: energy_ctrl=ON에서 직접 START 시 `DISP_ENERGY`(Modbus reg 0x16)/LCD `VAR_ENERGY`가 0→증가→stop 시 last_energy 래치(§2.3 부수효과). ③ energy_ctrl=ON에서 직접 START가 에너지로 자동정지 **안 함**(on-time ceiling으로 종료) = §6 deviation 확인.
- **에너지-exit 사이클 E2E는 슬라이스4**(물리 SW_START + 실 초음파로 curr_power가 실제 차오를 때) / 절대 divisor는 6b.
- 머지: `--no-ff` 로컬, 태그 `hw-revA_fw-stage-weld2`.

---

## 11. 미해결 / HW 확인 항목

1. **누산 divisor(`REG_ENERGY_DIV=250`) 절대 보정** — samd20 1ms·/500 가정의 2ms·/250 보정. 실제 samd20 ADC sample period + STM32 publish 실측 cadence + curr_power 물리단위(6b)로 미세조정. host-test는 구조만, 절대 용접 에너지 일치는 실 초음파 rig/스코프(6b 동반).
2. **abort 경로 = 즉시 READY vs CYL2 경유** — 사용자 확정 "즉시 상승+중단"(즉시 SOL OFF + READY). 기계적으로 제어된 상승(CYL2 ldt1 경유)이 필요하면 재검토. samd20 에러 abort는 사이클 경로 안 거침(에러상태).
3. **fault 후 재-arm** — abort 후 READY에서 다음 start가 정상 수용되는지(슬라이스1 start 시맨틱 그대로 — fault는 상태 잔존 없음, READY 복귀로 충분). 슬라이스4 물리 트리거에서 재확인.
4. **energy_ctrl 동적 변경 mid-WELD** — WELD 중 패널/Modbus가 energy_ctrl 토글 시 거동(글루가 매 tick 주입 → 즉시 반영). samd20도 라이브 변수라 동형. 경계 케이스이나 host-test로 토글 시점 검증 가능(선택).

---

*작성: 2026-06-14 weld-cycle 슬라이스2 브레인스토밍. samd20 참조 `ref/samd20/main.c:434-436`(누산)·`1555`(리셋)·`1560`(게이트)·`5270-5294`(exit+overtime). 아키텍처=Option A(advisor). 충실도=혼합. 다음 = 본 spec 사용자 리뷰 → writing-plans.*
