# 물리 IO 레이어 — 슬라이스 B (FREQ_IN 주파수 측정) 구현 플랜

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** PA0(FREQ_IN, TIM5_CH1 입력캡처)으로 출력 초음파 주파수를 측정해 `curr_freq`를 채우고, 이미 존재하는 LCD `VAR_FREQ` 표시·Modbus `MB_REG_DISP_FREQ` 미러에 실값을 공급한다 (표시/미러 배선은 이미 있고 현재 0으로 stub).

**Architecture:** 3계층 분리 — (1) HW: `drivers/freq_ic`가 TIM5_CH1 입력캡처(PA0, AF2, 32-bit free-run, rising edge)를 init·start하고 캡처 ISR을 받는다. (2) 순수 로직(HAL-free, host-test): `app_freq_fsm`이 캡처값 → 주기 Δ 누산(10샘플 링) → `curr_freq = CONST/Σ + freq_cal_val` 변환. ISR이 `freq_fsm_on_capture()`를 호출하고 main이 `freq_fsm_compute()`를 호출하는 단일-생산자/단일-소비자 구조. (3) 배선: `app_reg.reg_publish_measure`가 매 publish(2ms)에 `freq_fsm_compute()`로 `g_measure.curr_freq`를 채우고, 정지 시 `last_freq`를 래치(`last_energy`와 동일 패턴).

**Tech Stack:** C11, STM32 HAL (TIM input capture + GPIO AF), arm-none-eabi-gcc + CMake/Ninja, host 단위테스트(cc + fw/test/Makefile).

> **슬라이스 범위**: spec `docs/superpowers/specs/2026-06-20-physical-io-layer-design.md`의 **슬라이스 B**. 슬라이스 A(GPIO infra + 출력 미러)는 별도 브랜치 `feat/physical-io-slice-a`(HW 게이트, 미머지). 본 슬라이스 B는 A와 **독립적인 추가**(공유 파일 충돌 없음 — A의 `drivers/io.c`는 PA0를 의도적으로 건드리지 않음, io.c 주석 "PA0(FREQ_IN)은 슬라이스 B에서 TIM5 AF로 설정"). C(overload)·D(물리명령+E-stop)·E(weld 물리트리거)는 후속.

> **충돌 해소 기록 (2026-06-20, 사용자 확정)**: B-SEAM 분석 §8의 "PA0/SENS_OUT vestigial"은 **레거시 MCU의 PA0 핀**(M16 PA0=출력세기 7seg / SAMD20 PA0 네트=출력레벨 ADC `curr_lv`)을 가리키며, 이는 STM32 **PB0**(ADC1_IN8)에 흡수된 신호다. **STM32 PA0**은 전혀 다른 핀 = **FREQ_IN**(SAMD20 **PB15/FREQ_IN** TC0 입력캡처 흡수, `ref/samd20/main.c:175`). 같은 "PA0" 라벨이 서로 다른 핀을 가리킨 표기 충돌이었고, **충돌 없음**. SAMD20에서 FREQ는 SAMD20 자신이 TC0 입력캡처로 측정한 실재·활성 기능(`curr_freq`/`freq_buf[10]`/표시). pinmap 부록 D 라인 291 확정.

---

## 파일 구조

| 파일 | 책임 | 본 슬라이스 |
|---|---|---|
| `fw/include/app_freq_fsm.h` | 순수 freq FSM 인터페이스 + 변환 상수 | Create |
| `fw/src/app_freq_fsm.c` | 캡처 Δ 누산(10샘플 링) + `CONST/Σ + cal` 변환 (HAL-free) | Create |
| `fw/test/test_app_freq_fsm.c` | freq FSM host 단위테스트 | Create |
| `fw/test/Makefile` | freq FSM 테스트 타깃 추가 | Modify |
| `fw/drivers/freq_ic.h` | FREQ_IN 입력캡처 드라이버 인터페이스 | Create |
| `fw/drivers/freq_ic.c` | TIM5_CH1 IC init(PA0 AF2, 32-bit) + start | Create |
| `fw/include/periph.h` | `htim5` 핸들 extern | Modify |
| `fw/src/periph.c` | `htim5` 핸들 정의 | Modify |
| `fw/src/irq.c` | `TIM5_IRQHandler` + `HAL_TIM_IC_CaptureCallback` → fsm | Modify |
| `fw/src/main.c` | `freq_ic_init()` 호출 | Modify |
| `fw/src/app_reg.c` | `reg_publish_measure`에서 `curr_freq` 채움 + `last_freq` 래치 | Modify |
| `fw/include/app_reg.h` | `app_reg_tick` 시그니처에 `freq_cal_val` 추가 | Modify |
| `fw/src/app.c` | `app_reg_tick`에 `freq_cal_val` 주입 | Modify |

빌드 주의: `fw/CMakeLists.txt:90 file(GLOB src/*.c drivers/*.c)`는 configure 시점 평가 → **새 .c 추가(`app_freq_fsm.c`, `freq_ic.c`) 후 `cmake -B build -G Ninja` reconfigure 필수** (증분 빌드만 하면 미링크 = undefined ref). 이 함정은 2026-06-19 seek-reset 세션에서 실제로 물렸음 ([[project_phase12_env]]).

---

## 핵심 포팅 결정 (legacy SAMD20 `calc_freq` 충실 + 보드 도메인 보정)

SAMD20 원본 (`ref/samd20/main.c:4141-4160`, `tc_callback_FC:173-181`):
```c
/* ISR: 입력 펄스 주기를 TC가 캡처 (PWP 모드 CC0 = 풀 주기), 10개 링버퍼 */
freq_buf[freq_cnt] = tc_get_capture_value(...);   // GCLK3(≈8.1MHz) 틱 단위 주기
if (++freq_cnt >= 10) { freq_cnt = 0; update_freq = true; }
/* main(주기): 10개 합으로 평균 주파수 */
for(i=0; i<10; i++) temp_long += freq_buf[i];
curr_freq = 81000000UL / temp_long;   // = GCLK3 × 10 / Σ(10 주기)
curr_freq += freq_cal_val;
/* 무신호(fresh batch 없음) → curr_freq = 0 */
```

| 항목 | SAMD20 | STM32 포팅 | 근거 |
|---|---|---|---|
| 캡처 주기 | TC PWP 모드 CC0 = rising-to-rising 풀 주기, GCLK3 클럭 | TIM5_CH1 rising-edge 캡처, 연속 캡처 간 Δ = 풀 주기, **96 MHz** 클럭 | PWP CC0 ≡ rising 간 Δ |
| 카운터 폭 | 16-bit (≈1.5kHz 미만 wrap) | **32-bit** (TIM5 = F410 유일 32-bit, `IS_TIM_32B_COUNTER_INSTANCE==TIM5`); 44.7s wrap, unsigned Δ가 단일 wrap 흡수 | F410 stm32f410rx.h |
| 평균 | `freq_buf[10]` 합 | 10샘플 Δ 합 | 동일 |
| 변환 상수 | `81000000` (= GCLK3≈8.1MHz × 10) | **`960000000` = 96MHz × 10**. ⚠ **81e6를 복사하지 말 것** — 그건 SAMD20의 외부 클럭이다. 우리 보드는 **자기 96MHz 클럭**으로 측정하므로 `f_tim × 10`이 정확. | `curr_freq = f_tim×10 / Σ(10주기)` |
| cal 보정 | `curr_freq += freq_cal_val` (uint16 += int16) | `(uint16_t)((uint16_t)base + (uint16_t)cal)` 동일 wrap | 충실 |
| 무신호 | fresh batch 없으면 `curr_freq=0` | batch 시퀀스 미증가 → `compute()` returns 0 | 충실 |

- **TIM5 클럭 = 96 MHz 확인**: `fw/src/clock.c:28` `APB1CLKDivider = RCC_HCLK_DIV2` → APB1 = 48MHz, **APB1 prescaler≠1이므로 타이머 클럭 = APB1×2 = 96MHz** (TIM5는 APB1 버스). `FREQ_TIM_CLK_HZ = 96000000`.
- **절대 스케일은 HW-rig 게이트**: `CONST/Σ`의 절대 정확도는 실 초음파 신호로만 확정. `freq_cal_val`(LCD/FRAM 설정)이 오프셋 트림 제공. 본 슬라이스 = 측정 경로 구축; 절대값 sanity는 Task 4 HW(신호주입).

---

## Task 1: `app_freq_fsm` 순수 freq FSM (TDD)

**Files:**
- Create: `fw/include/app_freq_fsm.h`, `fw/src/app_freq_fsm.c`
- Test: `fw/test/test_app_freq_fsm.c`
- Modify: `fw/test/Makefile`

- [ ] **Step 1: 실패 테스트 작성 — `fw/test/test_app_freq_fsm.c`**

```c
/* fw/test/test_app_freq_fsm.c — host unit tests, 순수 freq 측정 FSM.
 * SAMD20 calc_freq 충실 포팅 (main.c:4141-4160) + 96MHz 도메인. */
#include <stdio.h>
#include <stdint.h>
#include "app_freq_fsm.h"

static int failures = 0;
#define CHECK_EQ(expr, expected) do {                                       \
    unsigned long a_ = (unsigned long)(expr);                               \
    unsigned long e_ = (unsigned long)(expected);                           \
    if (a_ != e_) {                                                         \
        printf("FAIL %s:%d  %s = %lu, expected %lu\n",                      \
               __FILE__, __LINE__, #expr, a_, e_);                          \
        failures++;                                                         \
    }                                                                       \
} while (0)

/* 96MHz / 20kHz = 4800 틱/주기. 10주기 Σ = 48000. CONST/48000 = 20000 Hz. */
#define PERIOD_20K  4800u
/* 96MHz / 15kHz = 6400 틱/주기. 10주기 Σ = 64000. CONST/64000 = 15000 Hz. */
#define PERIOD_15K  6400u

/* 첫 캡처는 prev 시드만(Δ 없음), 이후 N개 Δ를 만들려면 N+1 캡처 필요. */
static void feed_periods(uint32_t start, uint32_t period, unsigned n)
{
    uint32_t cap = start;
    freq_fsm_on_capture(cap);          /* prev 시드 (skip) */
    for (unsigned i = 0; i < n; i++) {
        cap += period;
        freq_fsm_on_capture(cap);      /* Δ = period */
    }
}

static void test_no_signal_returns_zero(void)
{
    freq_fsm_init();
    CHECK_EQ(freq_fsm_compute(0), 0);   /* 캡처 0건 → fresh batch 없음 → 0 */
}

static void test_incomplete_batch_zero(void)
{
    freq_fsm_init();
    feed_periods(1000u, PERIOD_20K, 9); /* 9 Δ만 (10 미만) → batch 미완성 */
    CHECK_EQ(freq_fsm_compute(0), 0);
}

static void test_20k_exact(void)
{
    freq_fsm_init();
    feed_periods(1000u, PERIOD_20K, 10); /* 10 Δ → batch 완성, Σ=48000 */
    CHECK_EQ(freq_fsm_compute(0), 20000);
}

static void test_15k_exact(void)
{
    freq_fsm_init();
    feed_periods(0u, PERIOD_15K, 10);    /* Σ=64000 → 15000 */
    CHECK_EQ(freq_fsm_compute(0), 15000);
}

static void test_cal_offset_signed(void)
{
    freq_fsm_init();
    feed_periods(1000u, PERIOD_20K, 10);
    CHECK_EQ(freq_fsm_compute(5), 20005);    /* +5 */
    freq_fsm_init();
    feed_periods(1000u, PERIOD_20K, 10);
    CHECK_EQ(freq_fsm_compute(-5), 19995);   /* -5 (uint16 wrap = 충실) */
}

static void test_second_compute_no_fresh_batch(void)
{
    freq_fsm_init();
    feed_periods(1000u, PERIOD_20K, 10);
    CHECK_EQ(freq_fsm_compute(0), 20000);    /* 1회: fresh */
    CHECK_EQ(freq_fsm_compute(0), 0);        /* 2회: 새 batch 없음 → 0 (무신호 거동) */
}

static void test_multi_batch_uses_latest(void)
{
    freq_fsm_init();
    feed_periods(1000u, PERIOD_20K, 10);     /* batch1: 20kHz */
    feed_periods(0u,    PERIOD_15K, 10);     /* batch2: 15kHz (compute 호출 없이 누적) */
    CHECK_EQ(freq_fsm_compute(0), 15000);    /* 최신 batch(15kHz) 사용 */
}

static void test_32bit_wraparound(void)
{
    freq_fsm_init();
    /* prev = 0xFFFFFFFF, 다음 cap = 4799 → unsigned Δ = 4799 - 0xFFFFFFFF = 4800 */
    freq_fsm_on_capture(0xFFFFFFFFu);        /* prev 시드 */
    uint32_t cap = 0xFFFFFFFFu;
    for (unsigned i = 0; i < 10; i++) {
        cap += PERIOD_20K;                   /* 32-bit wrap; 첫 스텝이 0xFFFFFFFF→4799 */
        freq_fsm_on_capture(cap);
    }
    CHECK_EQ(freq_fsm_compute(0), 20000);    /* wrap에도 Σ=48000 정확 */
}

static void test_div_guard_zero_sum(void)
{
    freq_fsm_init();
    /* 10 캡처가 모두 동일값 → Δ=0 × 10 → Σ=0. batch는 완성되나 0 나눗셈 가드. */
    for (unsigned i = 0; i < 11; i++) {
        freq_fsm_on_capture(7777u);          /* 첫 시드 + 10 Δ(=0) */
    }
    CHECK_EQ(freq_fsm_compute(0), 0);        /* div-by-zero 가드 → 0 */
}

int main(void)
{
    test_no_signal_returns_zero();
    test_incomplete_batch_zero();
    test_20k_exact();
    test_15k_exact();
    test_cal_offset_signed();
    test_second_compute_no_fresh_batch();
    test_multi_batch_uses_latest();
    test_32bit_wraparound();
    test_div_guard_zero_sum();
    if (failures) { printf("app_freq_fsm: %d FAIL\n", failures); return 1; }
    printf("app_freq_fsm: all tests passed\n");
    return 0;
}
```

- [ ] **Step 2: `fw/test/Makefile`에 타깃 추가**

`BIN_BZ  := /tmp/gds_test_app_buzzer_fsm` 줄(12) 다음에 추가:
```make
BIN_FQ  := /tmp/gds_test_app_freq_fsm
```
`test:` 타깃(14) 의존성 끝 `$(BIN_BZ)`를 `$(BIN_BZ) $(BIN_FQ)`로, 실행부 마지막 `	$(BIN_BZ)` 줄(20) 다음에 `	$(BIN_FQ)` 추가. 빌드 규칙은 `$(BIN_BZ): ...` 규칙(37-38) 다음에:
```make
$(BIN_FQ): test_app_freq_fsm.c ../src/app_freq_fsm.c ../include/app_freq_fsm.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_app_freq_fsm.c ../src/app_freq_fsm.c
```
`clean:`의 `rm -f ... $(BIN_BZ)`를 `... $(BIN_BZ) $(BIN_FQ)`로.

- [ ] **Step 3: 테스트 실패 확인**

Run: `make -C fw/test test`
Expected: FAIL — `app_freq_fsm.h` 없음 / `freq_fsm_*` 미정의 (컴파일 에러).

- [ ] **Step 4: `fw/include/app_freq_fsm.h` 작성**

```c
/* fw/include/app_freq_fsm.h — 순수 FREQ_IN 측정 FSM (HAL-free, host-test).
 * SAMD20 calc_freq 충실 포팅 (main.c:4141-4160). 캡처 ISR이 on_capture를,
 * main(reg_publish)이 compute를 호출 — 단일 생산자/단일 소비자. */
#pragma once
#include <stdint.h>

/* TIM5 = APB1 타이머 클럭 = 96 MHz (clock.c:28 APB1=HCLK/2 → ×2). */
#define FREQ_TIM_CLK_HZ   96000000UL
#define FREQ_AVG_SAMPLES  10u           /* SAMD20 freq_buf[10] 링 깊이 */
/* curr_freq = (f_tim × N) / Σ(N 주기).  ⚠ SAMD20의 81000000(=GCLK3≈8.1MHz×10)을
 * 복사하지 말 것 — 우리는 자기 96MHz 클럭으로 측정한다. */
#define FREQ_CONST        (FREQ_TIM_CLK_HZ * FREQ_AVG_SAMPLES)   /* 960000000 */

void     freq_fsm_init(void);                  /* 상태 리셋 (캡처 시작 전) */
void     freq_fsm_on_capture(uint32_t capture);/* ISR: 캡처 카운트 1건 투입 */
uint16_t freq_fsm_compute(int16_t cal_val);    /* main: fresh batch면 Hz, 아니면 0 */
```

- [ ] **Step 5: `fw/src/app_freq_fsm.c` 작성**

```c
/* fw/src/app_freq_fsm.c — 순수 FREQ_IN 측정 FSM. SAMD20 calc_freq 충실.
 * ISR(on_capture)이 연속 rising-edge 캡처 간 Δ(=주기)를 10개 누산해 batch를
 * 래치하고 seq++; main(compute)은 직전 호출 이후 새 batch가 생겼으면 평균
 * 주파수를, 아니면 0(무신호)을 돌려준다. seq 비교라 clear-flag 경합 없음. */
#include "app_freq_fsm.h"

/* ISR가 쓰는 상태 (단일 생산자). 32-bit 읽기는 Cortex-M4에서 atomic. */
static volatile uint32_t s_prev_cap;     /* 직전 캡처값 */
static volatile uint8_t  s_have_prev;    /* 첫 캡처 시드 여부 */
static volatile uint32_t s_acc;          /* 진행 중 batch의 Δ 합 */
static volatile uint8_t  s_cnt;          /* 진행 중 batch의 Δ 개수 */
static volatile uint32_t s_latched_sum;  /* 마지막 완성 batch의 Σ */
static volatile uint32_t s_batch_seq;    /* 완성 batch마다 ++ */
/* main이 쓰는 상태 (단일 소비자) */
static uint32_t s_last_seen_seq;

void freq_fsm_init(void)
{
    s_prev_cap     = 0u;
    s_have_prev    = 0u;
    s_acc          = 0u;
    s_cnt          = 0u;
    s_latched_sum  = 0u;
    s_batch_seq    = 0u;
    s_last_seen_seq = 0u;
}

void freq_fsm_on_capture(uint32_t capture)
{
    if (!s_have_prev) {              /* 첫 캡처는 기준점만 (Δ 없음) */
        s_prev_cap  = capture;
        s_have_prev = 1u;
        return;
    }
    uint32_t delta = capture - s_prev_cap;   /* unsigned: 32-bit wrap 흡수 */
    s_prev_cap = capture;
    s_acc += delta;
    if (++s_cnt >= (uint8_t)FREQ_AVG_SAMPLES) {
        s_latched_sum = s_acc;       /* batch 완성 → 래치 */
        s_acc = 0u;
        s_cnt = 0u;
        s_batch_seq++;               /* 소비자에게 fresh 신호 */
    }
}

uint16_t freq_fsm_compute(int16_t cal_val)
{
    if (s_batch_seq == s_last_seen_seq) {
        return 0u;                   /* 직전 호출 이후 새 batch 없음 = 무신호 */
    }
    s_last_seen_seq = s_batch_seq;
    uint32_t sum = s_latched_sum;
    if (sum == 0u) {
        return 0u;                   /* div-by-zero 가드 (SAMD20에 없던 방어) */
    }
    uint32_t base = FREQ_CONST / sum;            /* 평균 주파수 (Hz) */
    return (uint16_t)((uint16_t)base + (uint16_t)cal_val);  /* SAMD20 uint16 += int16 */
}
```

- [ ] **Step 6: 테스트 통과 확인**

Run: `make -C fw/test test`
Expected: `app_freq_fsm: all tests passed` + 기존 6스위트(reg_calc/modbus_core/tcp_frame/weld_fsm/seek_reset_fsm/buzzer_fsm) 전부 통과.

- [ ] **Step 7: 커밋**

```bash
git add fw/include/app_freq_fsm.h fw/src/app_freq_fsm.c fw/test/test_app_freq_fsm.c fw/test/Makefile
git commit -m "feat(freq): 순수 FREQ_IN 측정 FSM + host-test (app_freq_fsm)

- SAMD20 calc_freq 충실 포팅 (main.c:4141-4160): 10샘플 Δ 합 평균
- CONST=960000000 (96MHz×10, 우리 보드 클럭에서 유도 — SAMD20 81e6 복사 아님)
- 32-bit unsigned Δ (wrap 흡수), batch-seq 비교(무신호→0), div 가드
- 슬라이스 B spec"
```

---

## Task 2: `drivers/freq_ic` — TIM5_CH1 입력캡처 HW + ISR 배선

**Files:**
- Create: `fw/drivers/freq_ic.h`, `fw/drivers/freq_ic.c`
- Modify: `fw/include/periph.h`, `fw/src/periph.c`, `fw/src/irq.c`, `fw/src/main.c`

- [ ] **Step 1: `fw/include/periph.h`에 `htim5` extern 추가**

`extern TIM_HandleTypeDef  htim11;` 줄(9) 다음에 추가:
```c
extern TIM_HandleTypeDef  htim5;    /* FREQ_IN 입력캡처 (TIM5_CH1, PA0) */
```

- [ ] **Step 2: `fw/src/periph.c`에 `htim5` 정의 추가**

`TIM_HandleTypeDef  htim11;` 줄(8) 다음에 추가:
```c
TIM_HandleTypeDef  htim5;
```

- [ ] **Step 3: `fw/drivers/freq_ic.h` 작성**

```c
/* fw/drivers/freq_ic.h — FREQ_IN(PA0) 입력캡처 드라이버.
 * TIM5_CH1 32-bit rising-edge 캡처를 IT 모드로 구동. 캡처마다 irq.c의
 * HAL_TIM_IC_CaptureCallback이 app_freq_fsm으로 전달. spec 슬라이스 B. */
#pragma once

void freq_ic_init(void);   /* freq_fsm 리셋 + TIM5_CH1 IC start (IT) */
```

- [ ] **Step 4: `fw/drivers/freq_ic.c` 작성**

```c
/* fw/drivers/freq_ic.c — FREQ_IN 입력캡처. PA0 = TIM5_CH1 (AF2, GPIO_AF2_TIM5).
 * TIM5는 F410 유일 32-bit 타이머(IS_TIM_32B_COUNTER_INSTANCE==TIM5) → prescaler 0,
 * ARR=0xFFFFFFFF free-run, 연속 rising 캡처 간 Δ = 입력 주기(96MHz 틱). */
#include "stm32f4xx_hal.h"
#include "periph.h"
#include "freq_ic.h"
#include "app_freq_fsm.h"

void freq_ic_init(void)
{
    freq_fsm_init();                 /* IRQ 켜기 전에 소비자 상태 리셋 */

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_TIM5_CLK_ENABLE();

    GPIO_InitTypeDef g = {
        .Pin       = GPIO_PIN_0,
        .Mode      = GPIO_MODE_AF_PP,
        .Pull      = GPIO_NOPULL,
        .Speed     = GPIO_SPEED_FREQ_HIGH,
        .Alternate = GPIO_AF2_TIM5,   /* F410: PA0 TIM5_CH1 = AF2 */
    };
    HAL_GPIO_Init(GPIOA, &g);

    htim5.Instance               = TIM5;
    htim5.Init.Prescaler         = 0u;            /* 96 MHz 풀 해상도 */
    htim5.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim5.Init.Period            = 0xFFFFFFFFUL;  /* 32-bit free-run */
    htim5.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim5.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_IC_Init(&htim5) != HAL_OK) { Error_Handler(); }

    TIM_IC_InitTypeDef ic = {
        .ICPolarity  = TIM_ICPOLARITY_RISING,
        .ICSelection = TIM_ICSELECTION_DIRECTTI,
        .ICPrescaler = TIM_ICPSC_DIV1,            /* 매 rising 캡처 */
        .ICFilter    = 0u,                        /* 필터 없음 (SAMD20 충실); 지터 시 HW에서 상향 */
    };
    if (HAL_TIM_IC_ConfigChannel(&htim5, &ic, TIM_CHANNEL_1) != HAL_OK) { Error_Handler(); }

    /* 캡처 ISR 우선순위: sys_tick(TIM11=5) 및 comm보다 낮게(=숫자 큼). 캡처 유실은
     * 10샘플 평균이 흡수하므로 비치명적. ~50kHz×수십cyc ≈ 1.5% CPU. */
    HAL_NVIC_SetPriority(TIM5_IRQn, 10, 0);
    HAL_NVIC_EnableIRQ(TIM5_IRQn);

    if (HAL_TIM_IC_Start_IT(&htim5, TIM_CHANNEL_1) != HAL_OK) { Error_Handler(); }
}
```

- [ ] **Step 5: `fw/src/irq.c`에 TIM5 핸들러 + 캡처 콜백 추가**

include부에 추가 (`#include "sys_tick.h"` 다음):
```c
#include "app_freq_fsm.h"
```
`TIM1_TRG_COM_TIM11_IRQHandler` 함수 다음에 추가:
```c
void TIM5_IRQHandler(void) {
    HAL_TIM_IRQHandler(&htim5);
}
```
`HAL_TIM_PeriodElapsedCallback` 함수 다음에 추가:
```c
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM5) {
        freq_fsm_on_capture(HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1));
    }
}
```

- [ ] **Step 6: `fw/src/main.c`에 `freq_ic_init()` 추가**

include 추가 (`#include "io.h"` 옆):
```c
#include "freq_ic.h"
```
`io_init();` 줄(30) 바로 다음에 추가:
```c
    freq_ic_init();    /* FREQ_IN(PA0/TIM5_CH1) 입력캡처 — HW only, sys_tick 불요 */
```

- [ ] **Step 7: reconfigure + 빌드**

Run:
```bash
cd fw && env -u STM32_TOOLCHAIN cmake -B build -G Ninja && env -u STM32_TOOLCHAIN cmake --build build 2>&1 | tail -20
```
Expected: 새 `drivers/freq_ic.c` + `src/app_freq_fsm.c` 링크, **0 warning(우리 코드)**, `gds_us_ctrl.elf` 생성, FLASH/RAM 사용량 출력.

- [ ] **Step 8: 커밋**

```bash
git add fw/drivers/freq_ic.h fw/drivers/freq_ic.c fw/include/periph.h fw/src/periph.c fw/src/irq.c fw/src/main.c
git commit -m "feat(freq): TIM5_CH1 입력캡처 드라이버(freq_ic) + ISR 배선

- PA0 = TIM5_CH1 AF2 (GPIO_AF2_TIM5), 32-bit free-run rising 캡처 IT
- htim5 핸들(periph) + TIM5_IRQHandler + HAL_TIM_IC_CaptureCallback → freq_fsm
- NVIC prio 10 (sys_tick/comm 아래; 캡처 유실은 평균이 흡수)
- main freq_ic_init() 배선"
```

---

## Task 3: `curr_freq`/`last_freq`를 measure에 배선 (app_reg)

**Files:**
- Modify: `fw/src/app_reg.c`, `fw/include/app_reg.h`, `fw/src/app.c`

설명: 표시(`app_lcd_disp.c:163` `on?curr_freq:last_freq`)·Modbus(`app_modbus.c:75` `running?curr_freq:last_freq`) 배선은 **이미 존재**. 본 Task는 `g_measure.curr_freq`를 freq FSM 결과로 채우고, 정지 시 `last_freq`를 래치(`last_energy`와 **동일 패턴 — max 아님**, max_freq 없음). `freq_cal_val`은 기존 `limit_on_time` 선례대로 `app_reg_tick`에 **주입**.

- [ ] **Step 1: `app_reg.c`에 freq FSM include 추가**

`#include "io.h"` 줄(13) 다음에 추가:
```c
#include "app_freq_fsm.h"
```

- [ ] **Step 2: `reg_state_t`에 `last_freq` 필드 추가**

`uint32_t last_energy;` 줄(61) 다음에 추가:
```c
    uint16_t last_freq;              /* run-stop 시 curr_freq 래치 (samd20 us_off last_freq=curr_freq) */
```

- [ ] **Step 3: `reg_publish_measure` 시그니처에 `freq_cal_val` 추가 + curr/last 배선**

함수 정의(198) `static void reg_publish_measure(uint32_t now)`를:
```c
static void reg_publish_measure(uint32_t now, int16_t freq_cal_val)
```
로 변경. 함수 안 `g_measure.last_energy = g_reg.last_energy;` 줄(234) 다음에 추가:
```c
    /* FREQ_IN: 매 publish에 측정 (SAMD20 calc_freq처럼 run 게이팅 없음 — 무신호면
     * FSM이 0 반환). 표시/Modbus는 on?curr:last로 자체 게이팅(기존 배선). */
    g_measure.curr_freq = freq_fsm_compute(freq_cal_val);
    g_measure.last_freq = g_reg.last_freq;
```

- [ ] **Step 4: 두 정지 사이트에서 `last_freq` 래치 (last_energy 패턴)**

`app_reg.c`에 `g_reg.last_energy   = g_measure.curr_energy;` 가 **2곳**(라인 ~143 RUN_RELEASE 경로, 라인 ~285 mid-run source-mismatch 정지) 있음. **각 사이트의 그 줄 다음에** 추가:
```c
            g_reg.last_freq     = g_measure.curr_freq;   /* freq 래치 — last_energy 패턴(max 없음) */
```
(들여쓰기는 각 사이트의 기존 `g_reg.last_energy` 줄과 정확히 동일하게 맞출 것.)

- [ ] **Step 5: `reg_publish_measure` 호출부에 `freq_cal_val` 전달**

`app_reg_tick` 안 `reg_publish_measure(now);` 줄(325)을:
```c
    reg_publish_measure(now, freq_cal_val);
```
로 변경.

- [ ] **Step 6: `app_reg_tick` 시그니처에 `freq_cal_val` 추가 (정의 + 선언)**

`app_reg.c` 함수 정의(243) `void app_reg_tick(uint16_t limit_on_time)`를:
```c
void app_reg_tick(uint16_t limit_on_time, int16_t freq_cal_val)
```
로 변경. `fw/include/app_reg.h`의 선언 `void app_reg_tick(uint16_t limit_on_time);`를:
```c
void app_reg_tick(uint16_t limit_on_time, int16_t freq_cal_val);
```
로 변경.

- [ ] **Step 7: `app.c` 호출부에 `freq_cal_val` 주입**

`fw/src/app.c`의 `app_reg_tick(app_lcd_cfg()->limit_on_time);` 줄(95)을:
```c
    app_reg_tick(app_lcd_cfg()->limit_on_time, app_lcd_cfg()->freq_cal_val);
```
로 변경.

- [ ] **Step 8: 빌드 (구조 무변경 — 증분 OK, 단 안전하게 reconfigure)**

Run:
```bash
cd fw && env -u STM32_TOOLCHAIN cmake --build build 2>&1 | tail -15
```
Expected: 0 warning(우리 코드), elf 생성. `app_reg_tick`/`reg_publish_measure` 시그니처 변경이 app.c·app_reg.c에 일관 반영(미스매치 경고 없음).

- [ ] **Step 9: 기존 host-test 회귀 확인**

Run: `make -C fw/test test`
Expected: 7스위트 전부 통과 (app_reg.c는 host-test 비대상; app_reg_calc 순수 코어는 무영향).

- [ ] **Step 10: 커밋**

```bash
git add fw/src/app_reg.c fw/include/app_reg.h fw/src/app.c
git commit -m "feat(freq): curr_freq/last_freq를 measure에 배선 (app_reg)

- reg_publish_measure: g_measure.curr_freq = freq_fsm_compute(freq_cal_val)
  (run 게이팅 없음 — SAMD20 calc_freq 충실, 무신호면 FSM이 0)
- last_freq 래치 2곳 = last_energy 패턴(max 아님; max_freq 없음)
- freq_cal_val은 app_reg_tick에 주입(기존 limit_on_time 선례)
- 표시(VAR_FREQ)·Modbus(MB_REG_DISP_FREQ) 미러는 기존 배선 재사용"
```

---

## Task 4: 통합 검증 + 코드 리뷰 + HW 체크리스트

**Files:** (변경 없음 — 검증 전용)

- [ ] **Step 1: 클린 빌드**

Run:
```bash
cd fw && env -u STM32_TOOLCHAIN cmake -B build -G Ninja && env -u STM32_TOOLCHAIN cmake --build build 2>&1 | tail -25
```
Expected: 0 warning(우리 코드), FLASH/RAM 사용량 출력, elf 생성.

- [ ] **Step 2: 전체 host-test**

Run: `make -C fw/test test`
Expected: 7스위트(reg_calc/modbus_core/tcp_frame/weld_fsm/seek_reset_fsm/buzzer_fsm/**freq_fsm**) 전부 통과.

- [ ] **Step 3: PA0→TIM5_CH1 = AF2 데이터시트 교차확인**

`fw/vendor/.../stm32f4xx_hal_gpio_ex.h`에 `GPIO_AF2_TIM5 = 0x02` 확인 완료. **F410 데이터시트 Table(Alternate function mapping)에서 PA0의 AF2 = TIM5_CH1임을 한 번 더 확인** (일반 F4가 아닌 **F410 전용** — TIM2_CH1은 AF1, TIM5_CH1은 AF2). 잘못된 AF는 "캡처 ISR 안 뜸 = freq 0"으로 무신호와 구별 불가하므로 코드 머지 전 확정.

- [ ] **Step 4: cpp-reviewer 리뷰**

슬라이스 B 전체 diff(`git diff <슬라이스시작>..HEAD`)를 cpp-reviewer 에이전트로 리뷰. 중점: ISR↔main 공유 상태의 `volatile`/atomic 가정(32-bit 읽기), batch-seq 경합 분석, NVIC 우선순위 타당성, `app_reg_tick` 시그니처 변경의 호출부 일관성. CRITICAL/HIGH 0 확인, 지적은 인라인 반영 후 재빌드/재테스트.

- [ ] **Step 5: HW 검증 체크리스트 기록 (보드 세션용)**

⚠ **검증 모호성 — 반드시 신호주입으로 측정경로부터 증명**: "freq가 0"은 **세 가지 독립 원인**이 같은 증상을 낸다 — (1) FREQ_IN 신호 없음(초음파 출력단 = B-SEAM stub이라 RUN해도 신호 없을 수 있음), (2) PA0 AF 오설정(캡처 ISR 안 뜸), (3) 단순 idle. 따라서:
  - **① 신호주입 테스트 (B-SEAM 비의존, 필수 1순위)**: 신호발생기로 PA0에 **알려진 주파수**(예 20.0 kHz, 3.3V 구형파/펄스) 인가 → LCD `VAR_FREQ` 표시값과 Modbus `MB_REG_DISP_FREQ`(0x04)가 인가값과 일치(±오차)하는지 확인. 이게 **AF+캡처+CONST+평균 경로 전체를 B-SEAM과 무관하게 검증**. 15/20/40 kHz 스윕으로 스케일 선형성 확인.
  - **② `freq_cal_val` 트림**: LCD에서 `freq_cal_val` 조정 → 표시값이 오프셋만큼 이동하는지 (cal 배선 확인).
  - **③ 무신호 거동**: 신호 제거 → 표시가 0(run 중) 또는 last_freq(정지)로 떨어지는지.
  - **④ 절대 스케일 sanity**: 실 초음파 rig가 있으면 RUN 중 측정값이 모델 주파수(15~50kHz) 부근인지 — 단 **RUN 중 0은 코드 버그가 아니라 B-SEAM(출력단 미구동)일 수 있음**. ①이 통과했다면 측정경로는 정상.
  - **⑤ 회귀**: 직접-초음파(LCD/Modbus START) ceiling(~560ms)·ICON_RUN 무회귀; freq 추가가 publish 경로/시그니처를 건드렸으므로 STATUS/DISP_AMP/DISP_POWER 미러 무회귀 확인.

- [ ] **Step 6: 슬라이스 B 완료 + 다음 안내**

리뷰 반영분 커밋 후, 본 슬라이스는 **HW 게이트**(프로젝트 규율: 측정경로 HW PASS=신호주입 후 머지+태그). 다음 = 슬라이스 C(Overload PB13) 플랜 또는 슬라이스 A/B 묶음 HW 세션. 메모리 [[project-physical-io-layer]] 갱신(슬라이스 B CODE-COMPLETE).

---

## Self-Review (작성자 점검 완료)

- **Spec coverage**: spec §4 슬라이스 B의 모든 항목 커버 — TIM5_CH1 입력캡처 init(Task 2), 주기 Δ→`freq=CONST/Σ` 10샘플 평균(Task 1, SAMD20 `freq_buf[10]`/`81000000/Σ` 충실), `curr_freq`→LCD `VAR_FREQ`+Modbus `MB_REG_DISP_FREQ` 미러(Task 3, 기존 배선 재사용), 무신호 0 처리(Task 1 batch-seq), 평균/변환 순수함수 host-test(Task 1), 부분 HW-gated 표시 스케일(Task 4 신호주입).
- **Placeholder scan**: 없음. 모든 코드 step에 완전한 코드. 테스트 9케이스 모두 구체값.
- **Type consistency**: `freq_fsm_init/on_capture/compute`(헤더↔구현↔테스트↔ISR↔app_reg) 시그니처 일치; `htim5`(periph.h extern↔periph.c def↔freq_ic.c↔irq.c) 일치; `app_reg_tick(uint16_t, int16_t)`(app_reg.h 선언↔app_reg.c 정의↔app.c 호출) 일치; `reg_publish_measure(uint32_t, int16_t)`(정의↔호출) 일치; `FREQ_CONST/FREQ_TIM_CLK_HZ/FREQ_AVG_SAMPLES`(헤더 정의↔fsm.c·test 사용) 일치.
- **advisor 반영**: ① CONST=960e6 유도(81e6 복사 금지) 근거 명시 ✓ ② last_freq=last_energy 패턴(max 아님) ✓ ③ 검증 모호성→신호주입 1순위 + "RUN 중 0은 B-SEAM일 수 있음" 명시 ✓ ④ PA0 AF2 F410 데이터시트 교차확인 step ✓ ⑤ GLOB reconfigure ✓ ⑥ NVIC 우선순위 의도적 설정(10) ✓ ⑦ host-test에 first-capture-skip/32bit-wrap/multi-batch-latest 추가 ✓
- **주의**: `app_reg_tick` 시그니처 변경은 호출부 app.c 단 1곳 — Task 3 Step 7에서 동시 반영(미반영 시 빌드 경고로 즉시 검출).
