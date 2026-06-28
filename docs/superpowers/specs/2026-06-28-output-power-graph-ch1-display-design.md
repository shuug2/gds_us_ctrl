# 출력파워 그래프 표시 — ch1(소비전류) 소스 전환 설계

> **요약**: STM32의 LCD 출력파워 그래프(20-칸 바 `LV_OUTPUT` + 수치 `VAR_POWER`/에너지 `VAR_ENERGY`)는 표시 로직이 이미 완성·배선돼 있으나, 표시값(`curr_amp`/`curr_power`/`curr_energy`)이 레귤레이션과 같은 **ch0(IN8)** 에서 나온다. 이 설계는 SAMD20 충실 포팅을 위해 **표시 경로만 ch1(IN9=소비전류, SAMD20 PA02 등가)** 로 분리하고, SAMD20 `cal_real_val` ADC_CURR 변환식을 순수함수로 포팅한다. 레귤레이션(ch0)은 불변. 범위는 host-test까지(머지·보드검증·절대보정은 6b 이연).

- **작성일**: 2026-06-28
- **브랜치**: `feat/output-power-graph-ch1` (main `36b0a06`에서 분기)
- **선행 분석**: 본 세션 SAMD20 `ref/samd20/main.c` 추적 + `docs/pinmap.md` (2026-06-20 HW 확정)
- **관련 메모리**: `[[project-i2c-pot-amplitude]]`(언더플로 가드 방침), `[[project-ovtime-energy-run]]`(app_reg_tick 주입 통로)

---

## 1. 배경 / 동기

### 1.1 SAMD20 원본 (충실 포팅 대상)

SAMD20에서 출력파워 그래프의 데이터 원천은 **단일 ADC 채널 `ADC_CURR`(AIN0 = PA02 = 소비전류)** 한 곳이다. 가공은 `cal_real_val()` (`ref/samd20/main.c:400-436`):

```
temp     = average(8샘플, 최대·최소 1개씩 버린 trimmed-mean)
temp_val = temp * 4
temp_val = temp_val / 10 + cal_val          // cal_val = FRAM/LCD 보정값
curr_amp = (temp_val > 51) ? temp_val - 37 : 0   // 데드밴드 + 오프셋
curr_power = curr_amp * 22 / 10              // ×2.2 (전압상수 근사)
max_power 피크홀드 ; acc_energy += curr_power ; curr_energy = acc_energy/500
```

바 그래프(`send_outpower_data` step==0, `main.c:2616-2661`)는 `curr_amp`(순간값)와 `output_power`(설정 마커)를 입력으로 20-칸을 비선형 매핑하고, 수치(`send_val_data`, `main.c:4163-4178`)는 `max_power`/`max_amp`/`curr_freq`/`curr_energy`를 `VAR_POWER`(0x1110)~`VAR_ENERGY`(0x1113)로 보낸다.

`adc_change_ch`의 인라인 주석 `// GND`(PIN0)·`// I`(PIN11)는 **거꾸로 표기된 오류**다. 이름(`ADC_CURR`=전류)과 데이터 경로가 권위다. `ADC_OUT_LV`(PIN11)→`curr_lv`는 측정만 하고 표시·제어에 안 쓰이는 죽은 채널(유일한 그래프 사용처 `main.c:5135`가 주석처리됨).

### 1.2 STM32 현재 상태 (이미 된 것)

출력파워 그래프 **표시 로직은 완성·배선 완료**:
- `fw/src/app_lcd_disp.c` `disp_compute_output()` = SAMD20 20-칸 바 채우기 + 마커 verbatim 포팅
- 동 파일 `disp_send_val()` = `VAR_POWER/AMP/FREQ/ENERGY` 전송 포팅
- `app_lcd_disp_step()` = `app_lcd.c:192`에서 4ms cadence step machine으로 이미 호출

### 1.3 실제 갭 (이 설계가 메우는 것)

표시값이 전부 **ch0** 에서 온다:
- `fw/src/app_reg.c:221` `g_measure.curr_amp = g_reg.ch0_avg`
- `fw/src/app_reg.c:225,336` `curr_power = adc_scaled_value = reg_scale(g_reg.ch0_avg)` — **레귤레이션 출력레벨과 같은 값**
- 에너지도 그 `curr_power` 누산

`g_reg.ch1_avg`(IN9=소비전류)는 획득만 하고 표시에 안 쓰인다. pinmap(`docs/pinmap.md:34,80,224`, 2026-06-20 HW 확정)이 **IN9/PB1 = SAMD20 PA02 = 소비전류 = 전류/전력/에너지 source**, IN8/PB0 = 출력세기 피드백(vestigial)임을 확정했다. 즉 SAMD20 충실도는 표시 그래프를 **ch1** 로 모는 것이다.

---

## 2. 목표 / 비목표

### 2.1 목표
- 출력파워 그래프 표시값(`curr_amp`/`curr_power`/`curr_energy` + 피크홀드 `max_*`)을 **ch1(소비전류)** 에서 산출.
- SAMD20 `cal_real_val` ADC_CURR 변환식(×4/10+cal → 데드밴드/오프셋 → ×2.2)을 **구조 그대로** 순수함수로 포팅.
- 순수함수 host-test.

### 2.2 비목표 (불변/이연)
- **레귤레이션 경로 불변**: ch0 → `reg_scale` → `reg_output_level` → 소프트스타트.
- **표시 소비 측 불변**: `disp_compute_output`·`disp_send_val`·step machine·Modbus 미러는 무수정(값 출처만 바뀜).
- **주파수(`VAR_FREQ`) 불변**: 범위 밖.
- **6b 이연**: gain/deadband/offset/×2.2 **절대 보정**(실 HW), 필요 시 trimmed-mean 도입, 보드 E2E.
- **머지 안 함**: host-test까지.

---

## 3. 아키텍처 / 데이터 흐름

`app_reg.c` 안에 두 독립 경로:

```
레귤레이션(불변): ch0_avg ─> reg_scale ─> reg_output_level ─> 소프트스타트/출력레벨
표시 그래프(신설): ch1_avg ─> reg_current_from_adc(ch1_avg, cal_val) ─> curr_amp
                                  └> reg_power_from_amp(curr_amp) ─> curr_power
                                        └> (기존) max_amp/max_power 피크홀드
                                        └> (기존) acc_energy 누산 ─> curr_energy
```

소비 측(`app_lcd_disp.c` 바·`VAR_*`, Modbus 미러)은 이미 `g_measure.curr_amp/max_power/curr_energy`를 읽으므로 **`g_measure`에 들어가는 값의 출처만 ch0→ch1** 로 바뀐다.

### 3.1 수집(acquisition) — 결정: 기존 ch1 누산-평균 재사용 (마찰 최소)

`reg_acquire_step`의 ch1 경로(mean-of-50, 12→10bit 정규화 `ADC_NORM_SHIFT=2`, `CH1_SAMPLES=50`)는 **무수정**. SAMD20의 trimmed-mean(최대·최소 버림)은 **미적용**(6b에서 노이즈가 문제되면 도입). 사유: 사용자 결정 — 데이터 마찰이 적은 쪽.

---

## 4. 신설 순수함수 (`fw/src/app_reg_calc.c` + `fw/include/app_reg_calc.h`)

HAL-free, host-test 대상. 기존 `reg_scale`/`reg_energy_from_acc` 옆에 추가.

```c
/* 6b HW 보정 대상 상수 — SAMD20 cal_real_val ADC_CURR (main.c:416-433) */
#define REG_CURR_GAIN_NUM   4u    /* SAMD20 temp*4 */
#define REG_CURR_GAIN_DEN   10u   /* SAMD20 /10 */
#define REG_CURR_DEADBAND   51    /* (temp_val > 51) ? */
#define REG_CURR_OFFSET     37    /* temp_val - 37 */
#define REG_POWER_NUM       22u   /* ×2.2 */
#define REG_POWER_DEN       10u

/* ch1_avg(소비전류, 10bit-equiv) + cal_val(config) -> curr_amp.
 * SAMD20는 uint 연산이라 음수 cal_val에서 wrap 가능 → int32 중간연산 + 데드밴드로
 * 언더플로 가드(i2c_pot 방침). 절대 스케일은 6b. */
uint16_t reg_current_from_adc(uint16_t ch1_avg, int16_t cal_val) {
    int32_t v = (int32_t)ch1_avg * REG_CURR_GAIN_NUM / REG_CURR_GAIN_DEN + cal_val;
    if (v <= REG_CURR_DEADBAND) return 0u;
    return (uint16_t)(v - REG_CURR_OFFSET);
}

uint16_t reg_power_from_amp(uint16_t curr_amp) {
    return (uint16_t)(((uint32_t)curr_amp * REG_POWER_NUM) / REG_POWER_DEN);
}
```

> 데드밴드 경계 의미: SAMD20는 `> 51`이면 통과 후 `-37`. 위 가드는 `v <= 51 → 0`으로 동일 경계(51에서 0, 52에서 `52-37=15`). 언더플로 가드가 동시에 데드밴드 역할을 한다.

---

## 5. 글루 변경 (`fw/src/app_reg.c reg_publish_measure`)

```c
/* 기존
 *   g_measure.curr_amp   = g_reg.ch0_avg;
 *   g_measure.curr_power = active ? g_reg.adc_scaled_value : 0u;
 * 신규: 표시 전류/전력을 ch1에서 산출 (레귤레이션 ch0 불변) */
uint16_t disp_amp = reg_current_from_adc(g_reg.ch1_avg, /* cal_val: §5.1 */);
g_measure.curr_amp   = disp_amp;
uint16_t disp_pwr    = reg_power_from_amp(disp_amp);
g_measure.curr_power = active ? disp_pwr : 0u;
```

`max_amp/max_power` 피크홀드(`app_reg.c:222-228`)와 에너지 누산(`acc_energy += curr_power`, `curr_energy = reg_energy_from_acc(...)`, `app_reg.c:233-234`)은 **기존 코드가 `curr_amp/curr_power`를 읽으므로 그대로 따라온다** — 추가 변경 없음.

### 5.1 cal_val 주입 통로 (plan에서 확정)

`cal_val`은 `app_config_t`(`app_config.h:21`)에 있으나 `app_reg.c`는 현재 config 접근이 없다. `app_lcd_cfg()` 직접 호출은 `app_reg → app_lcd` **순환의존**(과거 리뷰 지적 클래스)을 만든다.

**권장**: 기존 주입 통로 재사용 — `app_reg_tick`에 이미 주입되는 `reg_run_limits_t`(OVTIME 작업, `[[project-ovtime-energy-run]]`)에 `cal_val` 필드를 더해 실어 보내거나, config-apply 시점에 `g_reg.cal_val`로 1회 복사하는 전용 setter. 순수함수는 `cal_val`을 인자로 받으므로 host-test는 통로와 무관. 최종 선택은 writing-plans에서.

---

## 6. 엣지 / 에러 처리

| 상황 | 거동 |
|------|------|
| `ch1_avg = 0` (idle) | `curr_amp=0` → `power=0` → 바 all-clear(+설정 마커만) = idle-safe |
| 음수 `cal_val` 언더플로 | §4 int32 + 데드밴드 가드로 0 (SAMD20 uint wrap 회피) |
| 범위 | `ch1_avg ≤ 1023` → ×4=4092 → int32 안전; `power ≈ 900` max → uint16 안전 |
| `active==false` (정지) | `curr_power=0` (기존 게이트 유지), 표시는 `last_*` 미러(기존) |

---

## 7. 테스트 (host-only)

`fw/test/test_app_reg_calc.c`에 케이스 추가. **절대값 아님 — 구조 검증**(절대 보정은 6b):

- `reg_current_from_adc`:
  - 데드밴드: `ch1_avg`가 임계 미만 → 0
  - 임계 위 단조 증가
  - 오프셋(-37) 적용 확인 (예: 데드밴드 직상 입력 → `v-37`)
  - `cal_val` 양/음 시프트
  - 음수 `cal_val` 언더플로 → 0 (wrap 아님)
- `reg_power_from_amp`:
  - amp×2.2 비율 (예: 100 → 220, 0 → 0, 큰 값 비율 유지)

기존 `reg_scale`/`reg_energy_from_acc` 회귀 케이스 유지.

---

## 8. 범위 경계 요약

- **불변**: 레귤레이션(ch0/`reg_scale`/`reg_output_level`), 주파수 소스, 바 표시 로직(`disp_compute_output`), `disp_send_val`, step machine, Modbus 미러, `reg_acquire_step`.
- **신설**: `reg_current_from_adc`/`reg_power_from_amp`(순수) + `reg_publish_measure` 글루 2줄 교체 + cal_val 주입 통로 + host-test.
- **6b 이연**: gain/deadband/offset/×2.2 절대 보정, trimmed-mean 여부, 보드 E2E, 머지.

---

## 9. 증거 레퍼런스

| 사실 | 출처 |
|------|------|
| SAMD20 ADC_CURR 변환식 | `ref/samd20/main.c:400-436` |
| SAMD20 바 채우기 + 마커 | `ref/samd20/main.c:2616-2661` |
| SAMD20 VAR_* 전송 | `ref/samd20/main.c:4163-4178` |
| IN9=소비전류 확정 | `docs/pinmap.md:34,80,224` (2026-06-20 HW) |
| STM32 표시 바 (완성) | `fw/src/app_lcd_disp.c:66-169,188-242` |
| 현재 ch0 소스 | `fw/src/app_reg.c:221,225,336` |
| ch1 획득 (재사용) | `fw/src/app_reg.c:195-212` (`CH1_SAMPLES=50`, `ADC_NORM_SHIFT=2`) |
| 순수 계층 / 테스트 | `fw/src/app_reg_calc.c`, `fw/test/test_app_reg_calc.c` |
| config cal_val | `fw/include/app_config.h:21` |
