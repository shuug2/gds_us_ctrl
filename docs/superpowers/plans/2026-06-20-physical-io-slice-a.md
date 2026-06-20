# 물리 IO 레이어 — 슬라이스 A (GPIO 인프라 + 출력 미러) 구현 플랜

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 커넥터/패널 전 핀의 GPIO 인프라(`drivers/io`)를 깔고, 기존 상태를 그대로 미러하는 출력(USOUT=run연동, SOL_DN=weld hook)과 비블로킹 부저를 실제 GPIO로 구동한다 (제어 로직 무변경).

**Architecture:** HW 접근은 `drivers/io.{c,h}` 한 곳으로 격리(전 핀 config + raw read/write 헬퍼 — 후속 슬라이스가 공유). 출력은 기존 hook(`app_weld_hook_sol_dn`)·신규 hook(`app_reg_hook_us_output`)을 io 헬퍼로 배선. 부저는 순수 timed-beep FSM(`app_buzzer_fsm`, host-test) + 글루(`app_buzzer`, 10ms tick). heartbeat(PB3 dormant)는 제거해 PB3을 후속 OVLD 릴레이용으로 해방.

**Tech Stack:** C11, STM32 HAL (GPIO), arm-none-eabi-gcc + CMake/Ninja, host 단위테스트(cc + fw/test/Makefile).

> **슬라이스 범위**: spec `docs/superpowers/specs/2026-06-20-physical-io-layer-design.md`의 슬라이스 A. B(FREQ_IN)·C(overload)·D(물리명령+E-stop)·E(weld 물리트리거)는 본 슬라이스 HW 검증 후 각자 플랜으로 진행. **드라이버 헬퍼는 전 핀을 본 슬라이스에서 선구현**(후속 슬라이스는 read/write 헬퍼만 소비).

---

## 파일 구조

| 파일 | 책임 | 본 슬라이스 |
|---|---|---|
| `fw/drivers/io.h` | 커넥터 IO 인터페이스 (init + read/write 헬퍼) | Create |
| `fw/drivers/io.c` | 전 커넥터 핀 GPIO config + 폴라리티 적용 read/write | Create |
| `fw/src/app_buzzer_fsm.h` / `.c` | 순수 timed-beep FSM (HAL-free, host-test) | Create |
| `fw/src/app_buzzer.h` / `.c` | 부저 글루 (10ms tick → io_buzzer) | Create |
| `fw/test/test_app_buzzer_fsm.c` | 부저 FSM host 단위테스트 | Create |
| `fw/test/Makefile` | 부저 테스트 타깃 추가 | Modify |
| `fw/src/board.c` / `fw/include/board.h` | heartbeat(PB3) 제거 (dormant) | Modify |
| `fw/src/main.c` | `io_init()` + `app_buzzer_init()` 호출 | Modify |
| `fw/src/app.c` | 슈퍼루프에 `app_buzzer_tick()` | Modify |
| `fw/src/app_weld.c` | `app_weld_hook_sol_dn` → `io_sol_dn` 배선 | Modify |
| `fw/src/app_reg.c` / `fw/include/app_reg.h` | run-output 전이 hook → `io_usout` | Modify |

빌드 주의: `fw/CMakeLists.txt:90 file(GLOB src/*.c drivers/*.c)`는 configure 시점 평가 → **새 .c 추가 후 `cmake -B build -G Ninja` reconfigure 필수** (증분 빌드만 하면 미링크).

---

## Task 1: `drivers/io` GPIO 인프라 + heartbeat 제거

**Files:**
- Create: `fw/drivers/io.h`, `fw/drivers/io.c`
- Modify: `fw/src/board.c`, `fw/include/board.h`, `fw/src/main.c`

- [ ] **Step 1: `fw/drivers/io.h` 작성**

```c
/* fw/drivers/io.h — 커넥터/패널 물리 IO (OSC 발진단 제외).
 * 입력 read = raw 물리 레벨(0/1); 폴라리티 해석은 호출측. 출력 write = 논리
 * on/off(폴라리티는 io.c가 적용). spec 2026-06-20-physical-io-layer-design §2. */
#pragma once
#include <stdbool.h>
#include <stdint.h>

void io_init(void);                 /* 전 커넥터 핀 config (idle = inactive) */

/* 입력 (raw 물리 레벨 0/1; HAL_GPIO_ReadPin 그대로) */
uint8_t io_read_start(void);        /* PA15  active-LOW */
uint8_t io_read_reset(void);        /* PC10  active-LOW */
uint8_t io_read_estop_seek(void);   /* PC11  EMSW active-HIGH / SEEK active-LOW (model_type 분기는 호출측) */
uint8_t io_read_key1(void);         /* PC12  active-LOW */
uint8_t io_read_key2(void);         /* PB11  active-LOW */
uint8_t io_read_sens_up(void);      /* PA12  active-LOW */
uint8_t io_read_sens_dn(void);      /* PA11  active-LOW */
uint8_t io_read_overload(void);     /* PB13  active-HIGH */

/* 출력 (논리 on/off; 폴라리티 io.c 내부 적용) */
void io_usout(bool on);             /* PB4  active-HIGH */
void io_sol_dn(bool on);            /* PB5  active-LOW  */
void io_ovld_relay(bool on);        /* PB3  active-HIGH */
void io_buzzer(bool on);            /* PA2  active-HIGH */
```

- [ ] **Step 2: `fw/drivers/io.c` 작성**

```c
/* fw/drivers/io.c — 커넥터/패널 GPIO. 활성레벨 spec §2:
 * 버튼/센서 active-LOW(pull-up), overload active-HIGH(pull-down),
 * USOUT/OVLD/BUZZER active-HIGH(idle LOW), SOL_DN active-LOW(idle HIGH=off).
 * PA0(FREQ_IN)은 슬라이스 B에서 TIM5 AF로 설정 — 여기서 건드리지 않음. */
#include "stm32f4xx_hal.h"
#include "io.h"

/* ---- 입력 핀 ---- */
#define START_PORT      GPIOA
#define START_PIN       GPIO_PIN_15
#define SENS_UP_PORT    GPIOA
#define SENS_UP_PIN     GPIO_PIN_12
#define SENS_DN_PORT    GPIOA
#define SENS_DN_PIN     GPIO_PIN_11
#define KEY2_PORT       GPIOB
#define KEY2_PIN        GPIO_PIN_11
#define OVLD_IN_PORT    GPIOB
#define OVLD_IN_PIN     GPIO_PIN_13
#define RESET_PORT      GPIOC
#define RESET_PIN       GPIO_PIN_10
#define ESTOP_PORT      GPIOC
#define ESTOP_PIN       GPIO_PIN_11
#define KEY1_PORT       GPIOC
#define KEY1_PIN        GPIO_PIN_12

/* ---- 출력 핀 ---- */
#define BUZZER_PORT     GPIOA
#define BUZZER_PIN      GPIO_PIN_2
#define OVLD_RLY_PORT   GPIOB
#define OVLD_RLY_PIN    GPIO_PIN_3
#define USOUT_PORT      GPIOB
#define USOUT_PIN       GPIO_PIN_4
#define SOL_DN_PORT     GPIOB
#define SOL_DN_PIN      GPIO_PIN_5

void io_init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitTypeDef in_pu = {
        .Mode = GPIO_MODE_INPUT, .Pull = GPIO_PULLUP, .Speed = GPIO_SPEED_FREQ_LOW,
    };
    GPIO_InitTypeDef in_pd = {
        .Mode = GPIO_MODE_INPUT, .Pull = GPIO_PULLDOWN, .Speed = GPIO_SPEED_FREQ_LOW,
    };
    GPIO_InitTypeDef out = {
        .Mode = GPIO_MODE_OUTPUT_PP, .Pull = GPIO_NOPULL, .Speed = GPIO_SPEED_FREQ_LOW,
    };

    /* 입력 active-LOW (pull-up) */
    in_pu.Pin = START_PIN;   HAL_GPIO_Init(START_PORT,   &in_pu);
    in_pu.Pin = SENS_UP_PIN; HAL_GPIO_Init(SENS_UP_PORT, &in_pu);
    in_pu.Pin = SENS_DN_PIN; HAL_GPIO_Init(SENS_DN_PORT, &in_pu);
    in_pu.Pin = KEY2_PIN;    HAL_GPIO_Init(KEY2_PORT,    &in_pu);
    in_pu.Pin = RESET_PIN;   HAL_GPIO_Init(RESET_PORT,   &in_pu);
    in_pu.Pin = ESTOP_PIN;   HAL_GPIO_Init(ESTOP_PORT,   &in_pu);  /* 폴라리티는 호출측 model_type 분기 */
    in_pu.Pin = KEY1_PIN;    HAL_GPIO_Init(KEY1_PORT,    &in_pu);

    /* 입력 active-HIGH overload (pull-down → idle LOW) */
    in_pd.Pin = OVLD_IN_PIN; HAL_GPIO_Init(OVLD_IN_PORT, &in_pd);

    /* 출력 idle = inactive (레벨 먼저 세팅 후 출력 전환 — boot glitch 회피) */
    HAL_GPIO_WritePin(BUZZER_PORT,   BUZZER_PIN,   GPIO_PIN_RESET);  /* active-H off */
    HAL_GPIO_WritePin(OVLD_RLY_PORT, OVLD_RLY_PIN, GPIO_PIN_RESET);  /* active-H off */
    HAL_GPIO_WritePin(USOUT_PORT,    USOUT_PIN,    GPIO_PIN_RESET);  /* active-H off */
    HAL_GPIO_WritePin(SOL_DN_PORT,   SOL_DN_PIN,   GPIO_PIN_SET);    /* active-L off(=SOL_OFF) */
    out.Pin = BUZZER_PIN;   HAL_GPIO_Init(BUZZER_PORT,   &out);
    out.Pin = OVLD_RLY_PIN; HAL_GPIO_Init(OVLD_RLY_PORT, &out);
    out.Pin = USOUT_PIN;    HAL_GPIO_Init(USOUT_PORT,    &out);
    out.Pin = SOL_DN_PIN;   HAL_GPIO_Init(SOL_DN_PORT,   &out);
}

uint8_t io_read_start(void)      { return (uint8_t)HAL_GPIO_ReadPin(START_PORT,   START_PIN);   }
uint8_t io_read_reset(void)      { return (uint8_t)HAL_GPIO_ReadPin(RESET_PORT,   RESET_PIN);   }
uint8_t io_read_estop_seek(void) { return (uint8_t)HAL_GPIO_ReadPin(ESTOP_PORT,   ESTOP_PIN);   }
uint8_t io_read_key1(void)       { return (uint8_t)HAL_GPIO_ReadPin(KEY1_PORT,    KEY1_PIN);    }
uint8_t io_read_key2(void)       { return (uint8_t)HAL_GPIO_ReadPin(KEY2_PORT,    KEY2_PIN);    }
uint8_t io_read_sens_up(void)    { return (uint8_t)HAL_GPIO_ReadPin(SENS_UP_PORT, SENS_UP_PIN); }
uint8_t io_read_sens_dn(void)    { return (uint8_t)HAL_GPIO_ReadPin(SENS_DN_PORT, SENS_DN_PIN); }
uint8_t io_read_overload(void)   { return (uint8_t)HAL_GPIO_ReadPin(OVLD_IN_PORT, OVLD_IN_PIN); }

void io_usout(bool on)      { HAL_GPIO_WritePin(USOUT_PORT,    USOUT_PIN,    on ? GPIO_PIN_SET : GPIO_PIN_RESET); }
void io_ovld_relay(bool on) { HAL_GPIO_WritePin(OVLD_RLY_PORT, OVLD_RLY_PIN, on ? GPIO_PIN_SET : GPIO_PIN_RESET); }
void io_buzzer(bool on)     { HAL_GPIO_WritePin(BUZZER_PORT,   BUZZER_PIN,   on ? GPIO_PIN_SET : GPIO_PIN_RESET); }
void io_sol_dn(bool on)     { HAL_GPIO_WritePin(SOL_DN_PORT,   SOL_DN_PIN,   on ? GPIO_PIN_RESET : GPIO_PIN_SET); } /* active-LOW */
```

- [ ] **Step 3: `board.c`에서 heartbeat(PB3) 제거**

`fw/src/board.c`를 아래로 교체 (OSC idle 전용만 남김; PB3 해방):

```c
/* fw/src/board.c */
#include "stm32f4xx_hal.h"
#include "board.h"

/* OSC 출력 3채널 active-LOW (idle = HIGH = off). PB2/OSC0<-PB1, PB10/OSC1<-PB0,
 * PB14/OSC4<-PC7. PB12/PB13(OSC2/OSC3 후보)은 미확정으로 미설정. 실제 발진
 * 구동은 B-SEAM(stub) — 여기선 idle만. (heartbeat PB3는 dormant → io.c가 OVLD
 * 릴레이로 재용도, 2026-06-20 제거) */
#define CTRL_OSC_OUT_PINS (GPIO_PIN_2 | GPIO_PIN_10 | GPIO_PIN_14)

void board_init(void) {
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef out = {
        .Mode  = GPIO_MODE_OUTPUT_PP,
        .Pull  = GPIO_NOPULL,
        .Speed = GPIO_SPEED_FREQ_LOW,
    };
    HAL_GPIO_WritePin(GPIOB, CTRL_OSC_OUT_PINS, GPIO_PIN_SET);
    out.Pin = CTRL_OSC_OUT_PINS;
    HAL_GPIO_Init(GPIOB, &out);
}
```

- [ ] **Step 4: `board.h`에서 heartbeat 선언 제거**

`fw/include/board.h`를 아래로 교체:

```c
/* fw/include/board.h */
#pragma once

void board_init(void);
```

- [ ] **Step 5: `main.c`에 `io_init()` 추가**

`fw/src/main.c`에 include 추가 (다른 include 옆):
```c
#include "io.h"
```
`board_init();` 줄(라인 26) 바로 다음에 추가:
```c
    io_init();         /* 커넥터/패널 GPIO (입력 pull-up / 출력 idle off) */
```

- [ ] **Step 6: reconfigure + 빌드**

Run:
```bash
cd fw && env -u STM32_TOOLCHAIN cmake -B build -G Ninja && env -u STM32_TOOLCHAIN cmake --build build 2>&1 | tail -20
```
Expected: 새 `drivers/io.c` 링크, **0 warning(우리 코드)**, `gds_us_ctrl.elf` 생성. `board_heartbeat_toggle` 미정의 참조 에러 없음(호출처 0 확인됨).

- [ ] **Step 7: 커밋**

```bash
git add fw/drivers/io.h fw/drivers/io.c fw/src/board.c fw/include/board.h fw/src/main.c
git commit -m "feat(io): 커넥터/패널 GPIO 인프라(drivers/io) + heartbeat(PB3) 제거

- 전 커넥터 핀 config: 버튼/센서 active-LOW(pull-up), overload active-HIGH(pull-down), USOUT/OVLD/BUZZER active-HIGH, SOL_DN active-LOW
- raw read + 폴라리티 적용 write 헬퍼(후속 슬라이스 공유)
- PA0(FREQ_IN)은 슬라이스B로 미룸; heartbeat dormant 제거해 PB3 해방
- main.c io_init() 배선; 슬라이스 A spec"
```

---

## Task 2: `app_buzzer_fsm` 순수 timed-beep FSM (TDD)

**Files:**
- Create: `fw/src/app_buzzer_fsm.h`, `fw/src/app_buzzer_fsm.c`
- Test: `fw/test/test_app_buzzer_fsm.c`
- Modify: `fw/test/Makefile`

- [ ] **Step 1: 실패 테스트 작성 — `fw/test/test_app_buzzer_fsm.c`**

```c
/* fw/test/test_app_buzzer_fsm.c — host unit tests, 순수 부저 timed-beep FSM. */
#include <stdio.h>
#include <stdint.h>
#include "app_buzzer_fsm.h"

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

static void test_idle_off(void)
{
    buzzer_fsm_init();
    CHECK_EQ(buzzer_fsm_step(), 0);   /* beep 없으면 항상 off */
    CHECK_EQ(buzzer_fsm_step(), 0);
}

static void test_beep_n_ticks(void)
{
    buzzer_fsm_init();
    buzzer_fsm_beep(3);
    CHECK_EQ(buzzer_fsm_step(), 1);   /* 3 tick on */
    CHECK_EQ(buzzer_fsm_step(), 1);
    CHECK_EQ(buzzer_fsm_step(), 1);
    CHECK_EQ(buzzer_fsm_step(), 0);   /* 이후 off */
    CHECK_EQ(buzzer_fsm_step(), 0);
}

static void test_rebeep_resets(void)
{
    buzzer_fsm_init();
    buzzer_fsm_beep(2);
    CHECK_EQ(buzzer_fsm_step(), 1);   /* 1 소비 (남은 1) */
    buzzer_fsm_beep(2);               /* 재트리거 → 2로 리셋 */
    CHECK_EQ(buzzer_fsm_step(), 1);
    CHECK_EQ(buzzer_fsm_step(), 1);
    CHECK_EQ(buzzer_fsm_step(), 0);
}

static void test_beep_zero(void)
{
    buzzer_fsm_init();
    buzzer_fsm_beep(0);
    CHECK_EQ(buzzer_fsm_step(), 0);   /* 0 tick = no-op */
}

int main(void)
{
    test_idle_off();
    test_beep_n_ticks();
    test_rebeep_resets();
    test_beep_zero();
    if (failures) { printf("app_buzzer_fsm: %d FAIL\n", failures); return 1; }
    printf("app_buzzer_fsm: all tests passed\n");
    return 0;
}
```

- [ ] **Step 2: `fw/test/Makefile`에 타깃 추가**

`BIN_SR := ...` 줄 다음에:
```make
BIN_BZ  := /tmp/gds_test_app_buzzer_fsm
```
`test:` 타깃의 의존성·실행에 `$(BIN_BZ)` 추가 (마지막 줄 `$(BIN_SR)` 다음 줄에 `	$(BIN_BZ)`), 그리고 빌드 규칙:
```make
$(BIN_BZ): test_app_buzzer_fsm.c ../src/app_buzzer_fsm.c ../include/app_buzzer_fsm.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_app_buzzer_fsm.c ../src/app_buzzer_fsm.c
```
`clean:`의 `rm -f`에 `$(BIN_BZ)` 추가. (`INC := -I../include`이므로 헤더는 `include/`에 두지 않고 `src/`에 둘 경우 `-I../src` 필요 → 헤더는 **`fw/include/`에 배치**.)

> 정정: 헤더 위치 일관성을 위해 `app_buzzer_fsm.h`는 `fw/include/`에 둔다(아래 Step 3). Makefile 의존 경로도 `../include/app_buzzer_fsm.h`.

- [ ] **Step 3: 테스트 실패 확인**

Run: `make -C fw/test test`
Expected: FAIL — `app_buzzer_fsm.h` 없음 / `buzzer_fsm_*` 미정의 (컴파일 에러).

- [ ] **Step 4: `fw/include/app_buzzer_fsm.h` 작성**

```c
/* fw/include/app_buzzer_fsm.h — 순수 timed-beep FSM (HAL-free, host-test). */
#pragma once
#include <stdint.h>

void    buzzer_fsm_init(void);           /* off */
void    buzzer_fsm_beep(uint16_t ticks); /* ticks 동안 on (재호출 시 리셋) */
uint8_t buzzer_fsm_step(void);           /* 1 tick 진행: 남으면 1(on)+감소, 아니면 0 */
```

- [ ] **Step 5: `fw/src/app_buzzer_fsm.c` 작성**

```c
/* fw/src/app_buzzer_fsm.c — 순수 timed-beep FSM. */
#include "app_buzzer_fsm.h"

static uint16_t s_remaining;

void buzzer_fsm_init(void)            { s_remaining = 0u; }
void buzzer_fsm_beep(uint16_t ticks)  { s_remaining = ticks; }

uint8_t buzzer_fsm_step(void)
{
    if (s_remaining > 0u) {
        s_remaining--;
        return 1u;
    }
    return 0u;
}
```

- [ ] **Step 6: 테스트 통과 확인**

Run: `make -C fw/test test`
Expected: `app_buzzer_fsm: all tests passed` + 기존 5스위트 전부 통과.

- [ ] **Step 7: 커밋**

```bash
git add fw/include/app_buzzer_fsm.h fw/src/app_buzzer_fsm.c fw/test/test_app_buzzer_fsm.c fw/test/Makefile
git commit -m "feat(buzzer): 순수 timed-beep FSM + host-test (app_buzzer_fsm)"
```

---

## Task 3: `app_buzzer` 글루 + 슈퍼루프 배선

**Files:**
- Create: `fw/include/app_buzzer.h`, `fw/src/app_buzzer.c`
- Modify: `fw/src/main.c`, `fw/src/app.c`

- [ ] **Step 1: `fw/include/app_buzzer.h` 작성**

```c
/* fw/include/app_buzzer.h — 부저 글루 (10ms tick → io_buzzer). */
#pragma once
#include <stdint.h>

void app_buzzer_init(void);
void app_buzzer_beep_ms(uint16_t ms);  /* ms 길이 비프 요청 (비블로킹) */
void app_buzzer_tick(void);            /* 슈퍼루프 10ms gate */
```

- [ ] **Step 2: `fw/src/app_buzzer.c` 작성**

```c
/* fw/src/app_buzzer.c — 부저 글루: 10ms tick에 FSM 진행, 레벨 엣지에만 io 구동. */
#include "app_buzzer.h"
#include "app_buzzer_fsm.h"
#include "io.h"
#include "sys_tick.h"

#define BUZZER_TICK_MS  10u

static uint32_t s_prev_ms;
static uint8_t  s_last;

void app_buzzer_init(void)
{
    buzzer_fsm_init();
    s_prev_ms = sys_tick_get_ms();
    s_last    = 0u;
    io_buzzer(false);
}

void app_buzzer_beep_ms(uint16_t ms)
{
    buzzer_fsm_beep((uint16_t)(ms / BUZZER_TICK_MS));
}

void app_buzzer_tick(void)
{
    uint32_t now = sys_tick_get_ms();
    if ((uint32_t)(now - s_prev_ms) < BUZZER_TICK_MS) {
        return;
    }
    s_prev_ms = now;

    uint8_t on = buzzer_fsm_step();
    if (on != s_last) {
        s_last = on;
        io_buzzer(on != 0u);
    }
}
```

- [ ] **Step 3: `main.c`에 init 추가**

include 추가:
```c
#include "app_buzzer.h"
```
`app_seek_reset_init();` 줄 다음에:
```c
    app_buzzer_init();      /* 부저 글루 (needs sys_tick up) */
```

- [ ] **Step 4: `app.c` 슈퍼루프에 tick 추가**

include 추가:
```c
#include "app_buzzer.h"
```
`app_loop_iter()` 안 `app_modbus_tick();` **다음**(루프 끝)에:
```c
    /* 6. 부저 — 10ms gate. 비블로킹 timed-beep 진행 (트리거는 overload/입력 슬라이스). */
    app_buzzer_tick();
```

- [ ] **Step 5: reconfigure + 빌드**

Run:
```bash
cd fw && env -u STM32_TOOLCHAIN cmake -B build -G Ninja && env -u STM32_TOOLCHAIN cmake --build build 2>&1 | tail -20
```
Expected: `app_buzzer.c`/`app_buzzer_fsm.c` 링크, 0 warning(우리 코드), elf 생성.

- [ ] **Step 6: 커밋**

```bash
git add fw/include/app_buzzer.h fw/src/app_buzzer.c fw/src/main.c fw/src/app.c
git commit -m "feat(buzzer): app_buzzer 글루 + 슈퍼루프 10ms tick 배선"
```

---

## Task 4: SOL_DN 실구동 배선 (weld hook → io)

**Files:**
- Modify: `fw/src/app_weld.c`

- [ ] **Step 1: `app_weld_hook_sol_dn`를 io_sol_dn으로 배선**

`fw/src/app_weld.c` include에 추가:
```c
#include "io.h"
```
기존 함수(라인 31-34)를 교체:
```c
void app_weld_hook_sol_dn(bool on)
{
    io_sol_dn(on);                 /* PB5 active-LOW (SOL_ON=LOW) */
    mon_printf("[weld] SOL_DN %s\r\n", on ? "ON" : "OFF");
}
```

- [ ] **Step 2: 빌드 (구조 무변경 → reconfigure 불필요, 증분 OK)**

Run: `cd fw && env -u STM32_TOOLCHAIN cmake --build build 2>&1 | tail -10`
Expected: 0 warning, elf 생성.

- [ ] **Step 3: 기존 weld host-test 회귀 확인**

Run: `make -C fw/test test`
Expected: 전 스위트 통과 (weld FSM은 hook과 무관한 순수 코어라 무영향).

- [ ] **Step 4: 커밋**

```bash
git add fw/src/app_weld.c
git commit -m "feat(io): SOL_DN(PB5) 실구동 배선 (app_weld_hook_sol_dn → io_sol_dn, active-LOW)"
```

---

## Task 5: USOUT run-output hook (app_reg → io)

**Files:**
- Modify: `fw/src/app_reg.c`, `fw/include/app_reg.h`

- [ ] **Step 1: `app_reg.h`에 hook 선언 추가**

`fw/include/app_reg.h`의 함수 선언부에 추가:
```c
/* run-output(USOUT) 전이 hook: us_run_status idle↔active 변화 시 호출.
 * 기본 구현이 io_usout 구동 (app_reg.c). */
void app_reg_hook_us_output(bool on);
```
(파일에 `#include <stdbool.h>`가 없으면 추가.)

- [ ] **Step 2: `app_reg.c`에 hook 정의 + 전이 감지**

`fw/src/app_reg.c` include에 추가:
```c
#include "io.h"
```
`reg_state_t`에 필드 추가 (`uint8_t us_run_status;` 줄 근처):
```c
    uint8_t  us_out_on;              /* USOUT 마지막 구동 레벨 (전이 감지) */
```
hook 정의 추가 (`app_reg_init` 위 등 파일 상단 함수부):
```c
void app_reg_hook_us_output(bool on)
{
    io_usout(on);                    /* PB4 active-HIGH = 초음파 출력 enable */
}
```
`reg_publish_measure()` 안, `g_measure.us_run_status = g_reg.us_run_status;` **다음**에 전이 감지 추가:
```c
    /* USOUT: run 활성(idle 아님)에 출력 enable. 전이에만 hook 구동. */
    uint8_t out_on = (uint8_t)(g_reg.us_run_status != (uint8_t)US_IDLE);
    if (out_on != g_reg.us_out_on) {
        g_reg.us_out_on = out_on;
        app_reg_hook_us_output(out_on != 0u);
    }
```

- [ ] **Step 3: 빌드**

Run: `cd fw && env -u STM32_TOOLCHAIN cmake --build build 2>&1 | tail -10`
Expected: 0 warning, elf 생성.

- [ ] **Step 4: 기존 host-test 회귀 확인**

Run: `make -C fw/test test`
Expected: 전 스위트 통과 (app_reg.c는 host-test 비대상, app_reg_calc 무영향).

- [ ] **Step 5: 커밋**

```bash
git add fw/src/app_reg.c fw/include/app_reg.h
git commit -m "feat(io): USOUT(PB4) run 연동 구동 (app_reg run-output 전이 hook → io_usout)"
```

---

## Task 6: 통합 검증 + 코드 리뷰

**Files:** (변경 없음 — 검증 전용)

- [ ] **Step 1: 클린 빌드**

Run:
```bash
cd fw && env -u STM32_TOOLCHAIN cmake -B build -G Ninja && env -u STM32_TOOLCHAIN cmake --build build 2>&1 | tail -25
```
Expected: 0 warning(우리 코드), FLASH/RAM 사용량 출력, elf 생성.

- [ ] **Step 2: 전체 host-test**

Run: `make -C fw/test test`
Expected: 6스위트(reg_calc/modbus_core/tcp_frame/weld_fsm/seek_reset_fsm/buzzer_fsm) 전부 통과.

- [ ] **Step 3: cpp-reviewer 리뷰**

슬라이스 A 전체 diff(`git diff <슬라이스시작>..HEAD`)를 cpp-reviewer 에이전트로 리뷰. CRITICAL/HIGH 0 확인, 지적은 인라인 반영 후 재빌드/재테스트.

- [ ] **Step 4: HW 검증 항목 기록 (보드 세션용)**

다음을 `docs/superpowers/` 체크리스트나 커밋 메시지에 기록 (실측은 보드 세션):
- USOUT(PB4): LCD/Modbus START → HIGH, STOP/ceiling → LOW (run 연동)
- SOL_DN(PB5): weld 사이클 중 active-LOW 토글 (단, weld 트리거는 슬라이스E — 현재는 직접 사이클 없음; Modbus/직접 경로엔 SOL 무영향)
- 부저(PA2): `app_buzzer_beep_ms` 트리거는 슬라이스 C/D — 본 슬라이스는 드라이버만 (HW 무동작 정상)
- 회귀: 직접-초음파 ceiling/ICON_RUN 무회귀

- [ ] **Step 5: 슬라이스 A 완료 커밋 (있으면) + 다음 안내**

리뷰 반영분 커밋 후, 슬라이스 B(FREQ_IN) 플랜 작성으로 진행.

---

## Self-Review (작성자 점검 완료)

- **Spec coverage**: 슬라이스 A 범위(GPIO infra + USOUT + SOL_DN + 부저 드라이버) 전부 Task로 커버. 전 핀 헬퍼 선구현(후속 슬라이스 소비)도 Task 1에 포함. read 헬퍼(start/reset/estop_seek/key/sens/overload)는 슬라이스 C/D/E 소비용으로 정의만.
- **Placeholder scan**: 없음. 모든 코드 step에 완전한 코드 포함.
- **Type consistency**: `io_*` 시그니처(io.h ↔ io.c ↔ 소비처) 일치; `buzzer_fsm_*`(헤더↔구현↔테스트) 일치; `app_reg_hook_us_output`(선언↔정의↔호출) 일치.
- **주의**: heartbeat 제거는 호출처 0(grep 확인) 전제 — Task 1 빌드에서 `board_heartbeat_toggle` 미참조 확인.
