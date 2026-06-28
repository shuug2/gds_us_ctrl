# 출력파워 그래프 ch1(소비전류) 소스 전환 — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** STM32 LCD 출력파워 그래프(바 + VAR_POWER 수치/에너지)의 표시값을 레귤레이션과 분리해 ch1(소비전류)에서 산출한다.

**Architecture:** SAMD20 `cal_real_val` ADC_CURR 변환식(×4/10+cal → 데드밴드/오프셋 → ×2.2)을 `app_reg_calc.c`의 순수함수 2개로 포팅하고, `app_reg.c reg_publish_measure`의 표시 전류/전력 산출을 ch0→ch1로 repoint한다(피크홀드 비교 소스 포함). 레귤레이션(ch0)·표시 소비측·acquisition은 불변.

**Tech Stack:** C11, arm-none-eabi-gcc (펌웨어), host `cc` (순수 단위테스트), CMake+Ninja, Make(테스트).

## Global Constraints

- 레귤레이션 경로(ch0 → `reg_scale` → `reg_output_level`)·`reg_acquire_step`·표시 소비측(`app_lcd_disp.c`/Modbus 미러)·주파수 소스는 **수정 금지**.
- 보정 상수(gain/deadband/offset/×2.2)는 **named #define**, 주석에 "6b HW 보정 대상" 명기. 절대값 검증은 하지 않음(host-test는 구조만).
- 음수 `cal_val` 언더플로는 **int32 중간연산 + 데드밴드 가드**로 0 (SAMD20 uint wrap 회피).
- 펌웨어 빌드 시 `$STM32_TOOLCHAIN` stale 회피: `env -u STM32_TOOLCHAIN` 접두 사용.
- 머지·보드검증 안 함. host-test + 펌웨어 0-warning 빌드까지.
- spec: `docs/superpowers/specs/2026-06-28-output-power-graph-ch1-display-design.md`.

---

## File Structure

| 파일 | 역할 | 변경 |
|------|------|------|
| `fw/include/app_reg_calc.h` | 순수 계층 선언 | `reg_current_from_adc`/`reg_power_from_amp` 선언 추가 |
| `fw/src/app_reg_calc.c` | 순수 계층 구현 | 보정 상수 #define + 두 함수 구현 추가 |
| `fw/test/test_app_reg_calc.c` | 순수 host-test | 두 함수 테스트 추가 + main() 등록 |
| `fw/include/app_reg.h` | reg 공개 IF | `app_reg_tick` 시그니처에 `int16_t cal_val` 추가 |
| `fw/src/app_reg.c` | reg 글루 | `g_reg.cal_val` 필드 + tick 주입 + `reg_publish_measure` ch1 repoint |
| `fw/src/app.c` | 슈퍼루프 배선 | `app_reg_tick` 호출에 `cal_val` 전달 |

`fw/test/Makefile`은 `BIN_REG`를 `test_app_reg_calc.c + app_reg_calc.c`로 이미 빌드하므로 **변경 불필요**.

---

## Task 1: 순수 변환함수 `reg_current_from_adc` / `reg_power_from_amp`

**Files:**
- Modify: `fw/include/app_reg_calc.h` (선언 추가, 파일 끝 `reg_energy_from_acc` 선언 뒤)
- Modify: `fw/src/app_reg_calc.c` (구현 추가, 파일 끝 `reg_energy_from_acc` 정의 뒤)
- Test: `fw/test/test_app_reg_calc.c`

**Interfaces:**
- Consumes: 없음(순수, stdint만).
- Produces:
  - `uint16_t reg_current_from_adc(uint16_t ch1_avg, int16_t cal_val)` — ch1 평균(10bit-equiv) + cal_val → 표시 전류 `curr_amp`.
  - `uint16_t reg_power_from_amp(uint16_t curr_amp)` — `curr_amp` → 표시 전력(×2.2).

- [ ] **Step 1: 실패하는 테스트 작성**

`fw/test/test_app_reg_calc.c`의 `int main(void)` 정의 **바로 위**에 두 함수를 추가:

```c
/* 출력파워 그래프 표시 전류/전력 (ch1=소비전류). 절대 보정은 6b — 여기선
 * 데드밴드/오프셋/단조성/언더플로 가드/×2.2 비율 등 구조만 검증.
 * 상수: GAIN 4/10, DEADBAND 51, OFFSET 37, POWER 22/10. */
static void test_reg_current_from_adc(void) {
    /* idle / 데드밴드 이하 -> 0 (v = ch1*4/10 + cal) */
    CHECK_EQ(reg_current_from_adc(0,   0), 0);    /* v=0           */
    CHECK_EQ(reg_current_from_adc(100, 0), 0);    /* v=40  <=51    */
    CHECK_EQ(reg_current_from_adc(128, 0), 0);    /* v=51  경계 ->0 */
    /* 데드밴드 직상 -> v-37 */
    CHECK_EQ(reg_current_from_adc(130, 0), 15);   /* v=52  -> 15   */
    CHECK_EQ(reg_current_from_adc(200, 0), 43);   /* v=80  -> 43   */
    CHECK_EQ(reg_current_from_adc(250, 0), 63);   /* v=100 -> 63   */
    /* 단조 증가: 15 < 43 < 63 (위 벡터로 확인됨) */
    /* cal_val 양수: 데드밴드 입력을 활성대역으로 끌어올림 */
    CHECK_EQ(reg_current_from_adc(100, 20), 23);  /* v=40+20=60 -> 23 */
    /* cal_val 음수: 활성 입력을 데드밴드 아래로 */
    CHECK_EQ(reg_current_from_adc(130, -10), 0);  /* v=52-10=42 <=51  */
    /* 음수 cal 언더플로 가드: uint wrap 없이 0 */
    CHECK_EQ(reg_current_from_adc(0, -100), 0);   /* v=-100 -> 0      */
}

static void test_reg_power_from_amp(void) {
    CHECK_EQ(reg_power_from_amp(0),   0);
    CHECK_EQ(reg_power_from_amp(10),  22);    /* 10*22/10  */
    CHECK_EQ(reg_power_from_amp(50),  110);
    CHECK_EQ(reg_power_from_amp(100), 220);
    CHECK_EQ(reg_power_from_amp(900), 1980);
}
```

그리고 `int main(void)` 안에서 `test_energy_integration_steps();` 호출 **바로 뒤**에 등록:

```c
    test_energy_integration_steps();
    test_reg_current_from_adc();
    test_reg_power_from_amp();
```

- [ ] **Step 2: 테스트 실패 확인**

Run: `make -C fw/test test`
Expected: 컴파일 에러 — `reg_current_from_adc`/`reg_power_from_amp` 미정의 (`implicit declaration` / link `undefined reference`).

- [ ] **Step 3: 선언 추가**

`fw/include/app_reg_calc.h` 파일 끝(`reg_energy_from_acc(...)` 선언 다음 줄)에 추가:

```c
/* 출력파워 그래프 표시 전류 — SAMD20 cal_real_val ADC_CURR 포팅
 * (ref/samd20/main.c:416-433). ch1_avg(소비전류, 10bit-equiv) + cal_val(config)
 * -> curr_amp. (temp_val>51)?temp_val-37:0 데드밴드/오프셋; int32 중간연산으로
 * 음수 cal_val 언더플로 가드. 절대 스케일 상수는 6b/HW 보정 대상. */
uint16_t reg_current_from_adc(uint16_t ch1_avg, int16_t cal_val);

/* curr_amp -> 표시 전력 (samd20 curr_power = curr_amp*22/10, main.c:432). */
uint16_t reg_power_from_amp(uint16_t curr_amp);
```

- [ ] **Step 4: 구현 추가**

`fw/src/app_reg_calc.c` 파일 끝(`reg_energy_from_acc` 정의 다음)에 추가:

```c
/* ── 출력파워 그래프 표시 전류/전력 (ch1=소비전류) ─────────────────
 * SAMD20 cal_real_val ADC_CURR (ref/samd20/main.c:416-433) 구조 포팅.
 * 상수는 6b HW 보정 대상 — 절대 스케일은 실측 후 확정. */
#define REG_CURR_GAIN_NUM   4u    /* samd20 temp*4 */
#define REG_CURR_GAIN_DEN   10u   /* samd20 /10 */
#define REG_CURR_DEADBAND   51    /* samd20 (temp_val > 51) ? */
#define REG_CURR_OFFSET     37    /* samd20 temp_val - 37 */
#define REG_POWER_NUM       22u   /* samd20 ×2.2 */
#define REG_POWER_DEN       10u

uint16_t reg_current_from_adc(uint16_t ch1_avg, int16_t cal_val)
{
    int32_t v = (int32_t)ch1_avg * (int32_t)REG_CURR_GAIN_NUM
              / (int32_t)REG_CURR_GAIN_DEN + (int32_t)cal_val;
    if (v <= (int32_t)REG_CURR_DEADBAND) {
        return 0u;                /* 데드밴드 + 음수 cal_val 언더플로 가드 */
    }
    return (uint16_t)(v - (int32_t)REG_CURR_OFFSET);
}

uint16_t reg_power_from_amp(uint16_t curr_amp)
{
    return (uint16_t)(((uint32_t)curr_amp * REG_POWER_NUM) / REG_POWER_DEN);
}
```

- [ ] **Step 5: 테스트 통과 확인**

Run: `make -C fw/test test`
Expected: 전 스위트 PASS — `BIN_REG` 출력 `all checks PASSED` 포함, 종료코드 0.

- [ ] **Step 6: 커밋**

```bash
git add fw/include/app_reg_calc.h fw/src/app_reg_calc.c fw/test/test_app_reg_calc.c
git commit -m "feat(reg): ch1 표시 전류/전력 순수 변환함수 + host-test

reg_current_from_adc(ch1_avg,cal_val): SAMD20 cal_real_val ADC_CURR
(×4/10+cal→데드밴드51/오프셋37) 포팅, int32+가드로 음수 cal 언더플로 0.
reg_power_from_amp: ×2.2. 상수는 6b 보정 대상 named #define."
```

---

## Task 2: 글루 — cal_val 주입 + `reg_publish_measure` ch1 repoint

**Files:**
- Modify: `fw/src/app_reg.c` (`reg_state_t`에 `cal_val` 필드 / `app_reg_tick` 주입 / `reg_publish_measure` repoint)
- Modify: `fw/include/app_reg.h:23` (`app_reg_tick` 시그니처)
- Modify: `fw/src/app.c:94` (호출부에 `cal_val` 전달)

**Interfaces:**
- Consumes: Task 1의 `reg_current_from_adc` / `reg_power_from_amp` (`app_reg.c`는 이미 `#include "app_reg_calc.h"`).
- Produces: `void app_reg_tick(uint16_t limit_on_time, int16_t cal_val)` — 표시 전류/전력을 `g_reg.ch1_avg`+`cal_val`에서 산출해 `g_measure`에 게시. 외부 동작 계약(레지스터/표시 VP)은 불변, 값 출처만 ch1.

> 이 Task는 순수 host-test가 없다(글루/HAL 결합). 게이트 = ① 펌웨어 0-warning 빌드 ② 기존 host 스위트 무회귀(순수층 미변경). 실거동(절대 스케일) 검증은 6b/HW 이연.
> spec §5 보정: 피크홀드(`max_amp`/`max_power`) 비교 소스도 ch1 산출값으로 함께 이동 — 아래 Step 3에 포함.

- [ ] **Step 1: `app_reg_tick` 시그니처 변경 (헤더)**

`fw/include/app_reg.h`의 줄:

```c
void app_reg_tick(uint16_t limit_on_time);
```

를 다음으로 교체:

```c
void app_reg_tick(uint16_t limit_on_time, int16_t cal_val);  /* cal_val: 표시 전류 보정(config) */
```

- [ ] **Step 2: `reg_state_t`에 `cal_val` 필드 추가**

`fw/src/app_reg.c`의 `reg_state_t` 안, `uint16_t adc_scaled_value;` 줄 **다음 줄**에 추가:

```c
    int16_t  cal_val;                 /* config 보정값 (app_reg_tick 주입) — 표시 전류 */
```

- [ ] **Step 3: `reg_publish_measure` repoint (ch1 + 피크홀드)**

`fw/src/app_reg.c` `reg_publish_measure`에서 기존 블록:

```c
    g_measure.curr_amp   = g_reg.ch0_avg;
    if (active && (g_reg.ch0_avg > g_reg.max_amp)) {
        g_reg.max_amp = g_reg.ch0_avg;   /* amp peak — same pattern as max_power */
    }
    g_measure.curr_power = active ? g_reg.adc_scaled_value : 0u;
    if (active && (g_reg.adc_scaled_value > g_reg.max_power)) {
        g_reg.max_power = g_reg.adc_scaled_value;
    }
```

를 다음으로 교체:

```c
    /* 표시 전류/전력은 ch1(소비전류)에서 — 레귤레이션(ch0/reg_scale)과 분리.
     * SAMD20 cal_real_val 포팅 (spec §3). 피크홀드 비교 소스도 ch1 산출값. */
    uint16_t disp_amp = reg_current_from_adc(g_reg.ch1_avg, g_reg.cal_val);
    g_measure.curr_amp = disp_amp;
    if (active && (disp_amp > g_reg.max_amp)) {
        g_reg.max_amp = disp_amp;
    }
    uint16_t disp_pwr = reg_power_from_amp(disp_amp);
    g_measure.curr_power = active ? disp_pwr : 0u;
    if (active && (disp_pwr > g_reg.max_power)) {
        g_reg.max_power = disp_pwr;
    }
```

- [ ] **Step 4: `app_reg_tick` 정의 — 시그니처 + cal_val 저장**

`fw/src/app_reg.c`의 정의 줄:

```c
void app_reg_tick(uint16_t limit_on_time)
{
    uint32_t now = sys_tick_get_ms();
```

를 다음으로 교체(시그니처 + 첫 줄 직후 저장):

```c
void app_reg_tick(uint16_t limit_on_time, int16_t cal_val)
{
    uint32_t now = sys_tick_get_ms();
    g_reg.cal_val = cal_val;        /* 표시 전류 보정값 주입 (reg_publish_measure 사용) */
```

- [ ] **Step 5: 호출부 배선 (`app.c`)**

`fw/src/app.c`의 줄:

```c
    app_reg_tick(app_lcd_cfg()->limit_on_time);
```

를 다음으로 교체:

```c
    app_reg_tick(app_lcd_cfg()->limit_on_time, app_lcd_cfg()->cal_val);
```

- [ ] **Step 6: 펌웨어 빌드 (0-warning 확인)**

Run:
```bash
env -u STM32_TOOLCHAIN cmake -S fw -B fw/build -G Ninja >/dev/null && \
env -u STM32_TOOLCHAIN cmake --build fw/build 2>&1 | grep -iE "\bwarning\b|\berror\b" || echo "BUILD CLEAN"
```
Expected: `BUILD CLEAN` (우리 파일 경고/에러 0). ELF 산출.

- [ ] **Step 7: host-test 무회귀 확인**

Run: `make -C fw/test test`
Expected: 전 스위트 PASS (`all checks PASSED`), 종료코드 0.

- [ ] **Step 8: 커밋**

```bash
git add fw/include/app_reg.h fw/src/app_reg.c fw/src/app.c
git commit -m "feat(reg): 출력파워 그래프 표시값을 ch1(소비전류)로 분리

reg_publish_measure가 표시 curr_amp/curr_power(+피크홀드)를 ch1_avg+cal_val의
reg_current_from_adc/reg_power_from_amp에서 산출(레귤레이션 ch0 불변).
app_reg_tick(limit_on_time,cal_val)로 cal_val 주입, app.c 배선.
에너지 누산은 curr_power 추종으로 자동. 머지/절대보정 6b 이연."
```

---

## Self-Review

**1. Spec coverage (spec 각 절 → task 매핑):**
- §3 데이터 흐름(ch1 분리) → Task 2 Step 3.
- §3.1 acquisition 재사용(불변) → Global Constraints + 어느 task도 `reg_acquire_step` 미수정.
- §4 순수함수 2개 + 상수 → Task 1 Step 3–4.
- §5 글루 + 피크홀드 보정 → Task 2 Step 3.
- §5.1 cal_val 주입(순환의존 회피) → Task 2 Step 1/4/5 (app_reg는 config/app_lcd 미포함, app.c가 배선 — 순환 없음).
- §6 엣지(idle 0/언더플로/범위) → Task 1 테스트 벡터(0, -100) + int32.
- §7 host-test → Task 1 Step 1.
- §8 범위 경계(불변 목록) → Global Constraints.
- 갭 없음.

**2. Placeholder scan:** "TBD/TODO/적절히 처리" 없음. 모든 코드 step에 실제 코드·명령·기대출력 포함. ✅

**3. Type consistency:** `reg_current_from_adc(uint16_t,int16_t)→uint16_t`, `reg_power_from_amp(uint16_t)→uint16_t` — 선언(T1 Step3)·구현(T1 Step4)·테스트(T1 Step1)·호출(T2 Step3) 전부 동일. `app_reg_tick(uint16_t,int16_t)` — 헤더(T2 S1)·정의(T2 S4)·호출(T2 S5) 일치. `g_reg.cal_val` int16_t — 선언(T2 S2)·저장(T2 S4)·사용(T2 S3) 일치. ✅

---

## Execution Handoff

(작성 후 실행 방식 선택은 본문 외에서 안내)
