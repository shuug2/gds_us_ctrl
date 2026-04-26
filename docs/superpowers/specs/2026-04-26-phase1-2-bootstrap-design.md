# Phase 1 + Phase 2 Bootstrap Design — gds_us_ctrl

**작성일**: 2026-04-26
**대상 MCU**: STM32F410RBT (Cortex-M4F @ 96 MHz, 128 KB Flash, 32 KB RAM)
**범위**: CubeMX UI 의존 제거 후 보드 부팅 + 1 Hz heartbeat + USART6 monitor "hello" 출력까지
**브레인스토밍 세션**: 2026-04-26 재개 (RESUME.md 기반)

---

## 0. 컨텍스트 및 결정 요약

### 0.1 결정 이력 (브레인스토밍 채택안)

| # | 항목 | 결정 |
|---|------|------|
| Q1 | 브레인스토밍 범위 | Phase 1 + Phase 2 통합 |
| Q2 | CubeMX 통합 방식 | **CubeMX UI 미사용**. Hand-written CMake + 우리 init 코드 직접 작성 |
| Q3 | Phase 2 데모 타깃 | TIM11 1 ms 틱 + USART6 "hello" |
| Q4 | 디렉토리 레이아웃 | `fw/vendor/` (read-only HAL/CMSIS) + `fw/src/` + `fw/include/` + `fw/drivers/` + `fw/openocd/` |
| Q5 | HAL/CMSIS 처리 | HAL **유지**, vendor in-tree 카피 |
| Q6 | 플래시/디버그 도구 | OpenOCD + ST-LINK |
| Arch | 골격 구조 | Phased Modular: `board` + `sys_tick` + `mon`, 인터페이스/구현 분리 |
| Clock | MCU 클럭 | **96 MHz** (HSI 16 MHz × N=96 / M=8 / P=2) — `CLAUDE.md`의 100 MHz 표기는 정정 |
| USART6 | 단일 채널 이중 역할 | Phase 2~Stage A: Monitor / Stage B 이후: Modbus RTU |

### 0.2 USART6 이중 역할 (사용자 추가 제약)

F410RBT 핀 제약상 USART6 한 채널을 단계별로 다른 역할에 재사용. Monitor UART 모듈은 backend 교체 가능한 인터페이스로 분리 (`mon.h` API + `mon_usart6.c` backend).

### 0.3 작은 default 결정

| 항목 | Default |
|------|---------|
| TIM11 1 ms 틱 | APB2Tim 96 MHz, PSC=95, ARR=999 → 1 kHz IRQ |
| Monitor UART | USART6, 115200 8N1, HAL blocking TX |
| 빌드 산출물 | `.elf` + `.bin` + `.hex` |
| Heartbeat 임시 핀 | `CON_OVLD = PB3` 1 Hz 토글 (Stage A에서 본용도 복원) |
| `CTRL_OSC0~4` 부팅 시 | `board_init`에서 모두 LOW 강제 |
| HSI vs HSE | HSI 16 MHz default. HSE 결정은 Stage B(Modbus 정확도 요구) 진입 시 재검토 |

---

## 1. 아키텍처 + 디렉토리 레이아웃

### 1.1 핵심 원칙

1. **`fw/vendor/`는 read-only in-tree 카피.** ST의 HAL/CMSIS/startup/linker/hal_conf만 들어감. SDK 업그레이드는 폴더 통째로 교체.
2. **모든 어플리케이션 코드는 `fw/src/` + `fw/drivers/` + `fw/include/`.** 자동생성 없음. `main.c`도 우리가 작성.
3. **HAL 호출은 어디서든 OK.** LL/CMSIS는 성능 크리티컬한 곳만 escape hatch.
4. **각 드라이버가 자기 페리페럴의 GPIO 핀 init 소유.** `drivers/usart.c`가 PC6/PC7 AF8 설정까지 담당.
5. **`.ioc` 파일 폐기.** `docs/historical/gds_usctrl.ioc`로 격하 (수정 금지, 회로 이력 보존만).

### 1.2 디렉토리 트리

```
fw/
├── CMakeLists.txt                ← 빌드 진입점
├── arm-none-eabi-gcc.cmake       ← 툴체인 파일
│
├── vendor/                       [READ-ONLY — 절대 편집 ✗]
│   ├── Drivers/
│   │   ├── STM32F4xx_HAL_Driver/{Inc,Src}/
│   │   ├── CMSIS/Device/ST/STM32F4xx/
│   │   └── CMSIS/Include/
│   ├── Core/
│   │   ├── Inc/stm32f4xx_hal_conf.h    ← HAL 모듈 selection (1회 편집 후 고정)
│   │   └── Src/system_stm32f4xx.c
│   ├── startup_stm32f410rbtx.s
│   └── STM32F410RBTX_FLASH.ld
│
├── include/
│   ├── app.h
│   ├── board.h
│   ├── clock.h
│   ├── sys_tick.h
│   ├── mon.h
│   └── periph.h
│
├── src/
│   ├── main.c
│   ├── app.c
│   ├── board.c
│   ├── clock.c
│   ├── sys_tick.c
│   ├── irq.c
│   └── periph.c
│
├── drivers/
│   ├── usart.c
│   ├── tim.c
│   └── mon_usart6.c
│
└── openocd/
    ├── stm32f410.cfg
    └── debug.gdb
```

### 1.3 모듈 의존 관계

```
                         main.c
                            │
            ┌────┬──────┬───┴────┬──────┬──────┐
            ▼    ▼      ▼        ▼      ▼      ▼
         clock  usart  tim     adc*   board   app
           │     │      │       │      │       │
           └─ HAL/CMSIS (vendor/) ──────┘       │
                                                ▼
                                        sys_tick + mon
                                                │
                                        mon_usart6 → USART6 (HAL)

* adc는 Stage A 진입 시 추가 (Phase 2 미포함)
```

→ 단방향, 순환 없음.

### 1.4 명명 규칙

| 규칙 | 의미 |
|------|------|
| `fw/vendor/` 절대 편집 ✗ | SDK 업그레이드 = 폴더 통째 교체 |
| `MX_*_Init` 명명 ✗ | 우리 init은 `usart6_init`, `tim11_init` 등 lowercase + 페리페럴 명시 |
| 페리페럴 GPIO는 그 드라이버 안에 | `drivers/usart.c`가 PC6/PC7 AF 책임 |
| 보드 핀(LED, CTRL 라인)은 `board.c` | 페리페럴에 안 묶인 단순 GPIO out/in |
| HAL 핸들은 `src/periph.c`에서 단일 정의 | `extern` 선언은 `include/periph.h` |
| HAL `__weak` callback override는 `src/irq.c`에 집중 | 흩어지지 않게 |

---

## 2. Phase 1 — Bootstrap Bring-up

> **목표**: 펌웨어 플래시 → MCU 96 MHz로 부팅 → `main()`의 `while(1) __NOP()`에 도달.

### 2.1 `fw/cube/` → `fw/vendor/` 가지치기

| 현재 위치 | 처리 |
|-----------|------|
| `fw/cube/Drivers/STM32F4xx_HAL_Driver/` | → `fw/vendor/Drivers/STM32F4xx_HAL_Driver/` |
| `fw/cube/Drivers/CMSIS/` | → `fw/vendor/Drivers/CMSIS/` |
| `fw/cube/Core/Inc/stm32f4xx_hal_conf.h` | → `fw/vendor/Core/Inc/stm32f4xx_hal_conf.h` (1회 편집) |
| `fw/cube/Core/Src/system_stm32f4xx.c` | → `fw/vendor/Core/Src/system_stm32f4xx.c` |
| `fw/cube/startup_stm32f410rbtx.s` | → `fw/vendor/startup_stm32f410rbtx.s` |
| `fw/cube/STM32F410RBTX_FLASH.ld` | → `fw/vendor/STM32F410RBTX_FLASH.ld` |
| `fw/cube/Core/Src/main.c` | **삭제** |
| `fw/cube/Core/Src/{gpio,usart,tim,adc,i2c,spi,rtc}.c` | **삭제** |
| `fw/cube/Core/Src/stm32f4xx_it.c` | **삭제** |
| `fw/cube/Core/Inc/{main,gpio,usart,...}.h` | **삭제** |
| `fw/cube/gds_usctrl.ioc` | **이동: `docs/historical/gds_usctrl.ioc`로 격하** |
| `fw/cube/Makefile` (CubeMX 생성) | **삭제** (CMake로 대체) |

### 2.2 `stm32f4xx_hal_conf.h` 모듈 selection

| Phase | 활성화할 모듈 |
|-------|--------------|
| **Phase 1** | `HAL_MODULE_ENABLED`, `HAL_RCC_MODULE_ENABLED`, `HAL_GPIO_MODULE_ENABLED`, `HAL_CORTEX_MODULE_ENABLED`, `HAL_PWR_MODULE_ENABLED`, `HAL_FLASH_MODULE_ENABLED`, `HAL_DMA_MODULE_ENABLED` |
| **Phase 2 추가** | `HAL_UART_MODULE_ENABLED`, `HAL_TIM_MODULE_ENABLED` |
| **Stage A 추가** | `HAL_I2C_MODULE_ENABLED`, `HAL_ADC_MODULE_ENABLED`, `HAL_IWDG_MODULE_ENABLED` |
| **Stage C 추가** | `HAL_SPI_MODULE_ENABLED` |
| **결정 보류** | `HAL_RTC_MODULE_ENABLED` — 일단 disable, 사용처 확정 시 활성화 |

### 2.3 Phase 1 핵심 파일

#### `fw/src/main.c` (Phase 1 형태)

```c
#include "stm32f4xx_hal.h"
#include "clock.h"

int main(void) {
    HAL_Init();
    clock_init();

    /* Phase 1 verification: GDB로 PC 위치 확인 */
    while (1) {
        __NOP();
    }
}
```

#### `fw/src/clock.c`

```c
#include "stm32f4xx_hal.h"
#include "clock.h"

void clock_init(void) {
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /* HSI 16 MHz / M=8 × N=96 / P=2 = 96 MHz */
    osc.OscillatorType       = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState             = RCC_HSI_ON;
    osc.HSICalibrationValue  = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState         = RCC_PLL_ON;
    osc.PLL.PLLSource        = RCC_PLLSOURCE_HSI;
    osc.PLL.PLLM             = 8;
    osc.PLL.PLLN             = 96;
    osc.PLL.PLLP             = RCC_PLLP_DIV2;
    osc.PLL.PLLQ             = 4;
    osc.PLL.PLLR             = 2;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) Error_Handler();

    clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                       | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;      /* 48 MHz */
    clk.APB2CLKDivider = RCC_HCLK_DIV1;      /* 96 MHz */
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_3) != HAL_OK) Error_Handler();
}
```

#### `fw/src/irq.c` (Phase 1 형태)

```c
#include "stm32f4xx_hal.h"

void NMI_Handler(void)        { while (1) {} }
void HardFault_Handler(void)  { while (1) {} }   /* TODO Stage A: register dump */
void MemManage_Handler(void)  { while (1) {} }
void BusFault_Handler(void)   { while (1) {} }
void UsageFault_Handler(void) { while (1) {} }
void SVC_Handler(void)        { /* unused */ }
void DebugMon_Handler(void)   { /* unused */ }
void PendSV_Handler(void)     { /* unused */ }

/* HAL이 SysTick을 1 ms tick으로 사용 */
void SysTick_Handler(void) { HAL_IncTick(); }

void Error_Handler(void) { __disable_irq(); while (1) {} }
```

#### `fw/include/clock.h`

```c
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

void clock_init(void);
void Error_Handler(void);

#ifdef __cplusplus
}
#endif
```

### 2.4 Phase 1 검증

| # | 검증 | 방법 | 기대 결과 |
|---|------|------|-----------|
| 1 | 빌드 통과 | `cmake --build build` | `.elf/.bin/.hex` 생성, 경고 없음 |
| 2 | 플래시 | `cmake --build build --target flash` | "Verified OK" |
| 3 | 부팅 도달 | OpenOCD GDB attach 후 `mon halt`, `mon reg pc` | PC가 `main()`의 `while(1)` 안 |
| 4 | 클럭 동작 | GDB에서 `print SystemCoreClock` | 96000000 |
| 5 | 무 IRQ 진입 | 단계 실행 시 fault 핸들러 무진입 | NMI/HardFault 무진입 |

---

## 3. Phase 2 — 모듈 추가 (heartbeat + monitor hello)

> **목표**: Phase 1 위에 8개 파일 추가 → PB3 LED 1 Hz 토글 + USART6로 1초마다 `[t=NNNN ms] hello\r\n` 출력.

### 3.1 추가 파일 (Phase 2)

```
fw/
├── include/   {app, board, sys_tick, mon, periph}.h    (NEW × 5)
├── src/
│   ├── main.c           (UPDATE — init 호출 추가)
│   ├── irq.c            (UPDATE — TIM11 IRQ 추가)
│   ├── app.c            (NEW)
│   ├── board.c          (NEW)
│   ├── sys_tick.c       (NEW)
│   └── periph.c         (NEW)
└── drivers/
    ├── usart.c          (NEW)
    ├── tim.c            (NEW)
    └── mon_usart6.c     (NEW)
```

### 3.2 HAL 핸들 단일 정의

```c
/* fw/include/periph.h */
#pragma once
#include "stm32f4xx_hal.h"
extern UART_HandleTypeDef huart6;
extern TIM_HandleTypeDef  htim11;

/* fw/src/periph.c */
#include "periph.h"
UART_HandleTypeDef huart6;
TIM_HandleTypeDef  htim11;
```

### 3.3 USART6 driver

```c
/* fw/drivers/usart.c */
#include "stm32f4xx_hal.h"
#include "periph.h"
#include "clock.h"  /* Error_Handler */

void usart6_init(void) {
    __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIO_InitTypeDef g = {
        .Pin       = GPIO_PIN_6 | GPIO_PIN_7,
        .Mode      = GPIO_MODE_AF_PP,
        .Pull      = GPIO_NOPULL,
        .Speed     = GPIO_SPEED_FREQ_VERY_HIGH,
        .Alternate = GPIO_AF8_USART6,
    };
    HAL_GPIO_Init(GPIOC, &g);

    __HAL_RCC_USART6_CLK_ENABLE();
    huart6.Instance          = USART6;
    huart6.Init.BaudRate     = 115200;
    huart6.Init.WordLength   = UART_WORDLENGTH_8B;
    huart6.Init.StopBits     = UART_STOPBITS_1;
    huart6.Init.Parity       = UART_PARITY_NONE;
    huart6.Init.Mode         = UART_MODE_TX_RX;
    huart6.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart6.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart6) != HAL_OK) Error_Handler();
}
```

> Phase 2는 blocking TX만. NVIC USART6 IRQ 미활성. Stage B에서 RX IRQ 추가.

### 3.4 TIM11 driver

```c
/* fw/drivers/tim.c */
#include "stm32f4xx_hal.h"
#include "periph.h"
#include "clock.h"

void tim11_init(void) {
    __HAL_RCC_TIM11_CLK_ENABLE();
    htim11.Instance               = TIM11;
    htim11.Init.Prescaler         = 95;     /* 96 MHz / 96 = 1 MHz */
    htim11.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim11.Init.Period            = 999;    /* 1 MHz / 1000 = 1 kHz IRQ */
    htim11.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim11.Init.AutoReloadPreload = TIM_AUTORELOADPRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim11) != HAL_OK) Error_Handler();

    HAL_NVIC_SetPriority(TIM1_TRG_COM_TIM11_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(TIM1_TRG_COM_TIM11_IRQn);
}
```

### 3.5 mon backend

```c
/* fw/drivers/mon_usart6.c */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "stm32f4xx_hal.h"
#include "periph.h"
#include "mon.h"

void mon_init(void) { /* huart6는 usart6_init이 이미 초기화 */ }

void mon_write(const uint8_t *buf, size_t len) {
    HAL_UART_Transmit(&huart6, (uint8_t *)buf, len, HAL_MAX_DELAY);
}

void mon_writeln(const char *s) {
    mon_write((const uint8_t *)s, strlen(s));
    mon_write((const uint8_t *)"\r\n", 2);
}

int mon_printf(const char *fmt, ...) {
    char buf[128];
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n > 0) {
        size_t emit = (n < (int)sizeof buf) ? (size_t)n : sizeof buf - 1;
        mon_write((const uint8_t *)buf, emit);
    }
    return n;
}
```

```c
/* fw/include/mon.h */
#pragma once
#include <stddef.h>
#include <stdint.h>
void mon_init(void);
void mon_write(const uint8_t *buf, size_t len);
void mon_writeln(const char *s);
int  mon_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
```

### 3.6 Board module

```c
/* fw/src/board.c */
#include "stm32f4xx_hal.h"
#include "board.h"

#define HB_PORT   GPIOB
#define HB_PIN    GPIO_PIN_3       /* CON_OVLD heartbeat 임시 */
#define CTRL_OSC_PINS (GPIO_PIN_10 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15)

void board_init(void) {
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef out = {
        .Mode  = GPIO_MODE_OUTPUT_PP,
        .Pull  = GPIO_NOPULL,
        .Speed = GPIO_SPEED_FREQ_LOW,
    };
    HAL_GPIO_WritePin(HB_PORT, HB_PIN, GPIO_PIN_RESET);
    out.Pin = HB_PIN;
    HAL_GPIO_Init(HB_PORT, &out);

    HAL_GPIO_WritePin(GPIOB, CTRL_OSC_PINS, GPIO_PIN_RESET);
    out.Pin = CTRL_OSC_PINS;
    HAL_GPIO_Init(GPIOB, &out);
}

void board_heartbeat_toggle(void) {
    HAL_GPIO_TogglePin(HB_PORT, HB_PIN);
}
```

```c
/* fw/include/board.h */
#pragma once
void board_init(void);
void board_heartbeat_toggle(void);
```

### 3.7 sys_tick module

```c
/* fw/src/sys_tick.c */
#include "stm32f4xx_hal.h"
#include "periph.h"
#include "clock.h"
#include "sys_tick.h"

static volatile uint32_t s_ms = 0;

void sys_tick_init(void) {
    if (HAL_TIM_Base_Start_IT(&htim11) != HAL_OK) Error_Handler();
}

uint32_t sys_tick_get_ms(void) {
    return s_ms;   /* 32-bit read는 Cortex-M4에서 atomic */
}

void sys_tick_handle_irq(void) {
    s_ms++;
}
```

```c
/* fw/include/sys_tick.h */
#pragma once
#include <stdint.h>
void sys_tick_init(void);
uint32_t sys_tick_get_ms(void);
void sys_tick_handle_irq(void);
```

> HAL `SysTick_Handler`(1 kHz)는 `HAL_IncTick()`만 호출하는 HAL 내부 카운터. 우리 `sys_tick`(TIM11 1 kHz)은 어플리케이션 cadence 분리용. Phase 2에선 둘 다 1 kHz로 동등 동작하지만 Stage B/C에서 TIM11을 Modbus 타임아웃·ADC 트리거로 재조율.

### 3.8 app module

```c
/* fw/src/app.c */
#include "stm32f4xx_hal.h"
#include "app.h"
#include "board.h"
#include "sys_tick.h"
#include "mon.h"

static uint32_t s_last_beat_ms = 0;

void app_init(void) {
    sys_tick_init();
    mon_init();
    mon_writeln("[boot] gds_us_ctrl phase2 ready");
    s_last_beat_ms = sys_tick_get_ms();
}

void app_loop_iter(void) {
    uint32_t now = sys_tick_get_ms();
    if ((uint32_t)(now - s_last_beat_ms) >= 1000) {
        s_last_beat_ms = now;
        board_heartbeat_toggle();
        mon_printf("[t=%lu ms] hello\r\n", (unsigned long)now);
    }
}
```

```c
/* fw/include/app.h */
#pragma once
void app_init(void);
void app_loop_iter(void);
```

### 3.9 main.c (Phase 2 최종형)

```c
#include "stm32f4xx_hal.h"
#include "clock.h"
#include "app.h"

extern void usart6_init(void);   /* drivers/usart.c */
extern void tim11_init(void);    /* drivers/tim.c */
extern void board_init(void);    /* src/board.c */

int main(void) {
    HAL_Init();
    clock_init();      /* 96 MHz */
    /* TODO Stage A: iwdg_init(2000); */
    usart6_init();     /* PC6/PC7 + 115200 8N1 */
    tim11_init();      /* 1 kHz IRQ enabled, base not started yet */
    board_init();      /* GPIO out + CTRL_OSC0..4 LOW */
    app_init();        /* sys_tick start, mon banner */

    while (1) {
        app_loop_iter();
        /* TODO Stage A: HAL_IWDG_Refresh(&hiwdg); */
    }
}
```

### 3.10 irq.c (Phase 2 추가)

```c
/* (Phase 1의 fault 핸들러 + SysTick_Handler 그대로 유지) */
#include "periph.h"
#include "sys_tick.h"

void TIM1_TRG_COM_TIM11_IRQHandler(void) {
    HAL_TIM_IRQHandler(&htim11);   /* HAL이 PeriodElapsedCallback 분기 */
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM11) {
        sys_tick_handle_irq();
    }
}
```

---

## 4. 부팅 시퀀스 + Cadence

### 4.1 부팅 타임라인 (Phase 2 기준)

```
T+0       MCU reset → Reset_Handler → SystemInit() → main()
T+~ms     HAL_Init()  → SysTick @ 16 MHz / 16000 = 1 kHz
T+~ms     clock_init()  → PLL lock, 96 MHz, Flash latency 3, SysTick 재설정
T+~ms     usart6_init() → GPIOC AF8, USART6 enabled
T+~ms     tim11_init()  → TIM11 base init, NVIC enabled, counter 정지
T+~ms     board_init()  → GPIOB outputs LOW
T+~ms     app_init()    → HAL_TIM_Base_Start_IT → 1 kHz IRQ 시작
                       → mon_writeln("[boot] gds_us_ctrl phase2 ready")
T+0~999ms while(1) iter: skip (diff <1000)
T+1000ms  app_loop_iter: PB3 toggle, mon_printf "[t=1000 ms] hello"
T+2000ms  ... 반복
```

### 4.2 Steady-state cadence

```
TIM11 IRQ:     ▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲ ...   매 1 ms (sys_tick_handle_irq, ~1 µs)
SysTick IRQ:   △△△△△△△△△△△△△△△△△△△△△ ...   매 1 ms (HAL_IncTick, ~1 µs)
main loop:     ━━━━━━━━━━━━━━━━━━━━━ ...   수십 ns/iter, 매 1초만 ~1.6 ms TX burst
PB3:           ──┐    ┌──┐    ┌──┐         heartbeat 0.5 Hz 사각파
                  └──┘    └──┘
USART6 TX:        │      │      │           매 1초 ~1.6 ms blocking burst
```

### 4.3 IRQ 우선순위

| 핸들러 | 우선순위 | 빈도 | 길이 |
|--------|----------|------|------|
| HardFault, NMI | -1, -2 (고정) | (예외 시) | 무한루프 |
| SysTick | **`TICK_INT_PRIORITY`** (Phase 1-B 편집 시 0으로 설정) | 1 kHz | <1 µs |
| TIM11 (TIM1_TRG_COM_TIM11) | **5** | 1 kHz | ~2 µs |
| (Stage B 추가) USART6 | 6 | RX 시 | ~3 µs |
| (Stage C 추가) SPI1 (W5500) | 7 | TX/RX 완료 | ~5 µs |

→ NVIC priority group = 4 (Cortex-M4 default), 모든 4 bit가 preempt. `TICK_INT_PRIORITY`는 `vendor/Core/Inc/stm32f4xx_hal_conf.h`에서 정의. CubeMX 자동생성 default가 0(highest)인 경우와 0x0F(lowest)인 경우 모두 있으므로 Phase 1-B 편집 시 **0으로 명시 설정** (HAL tick이 어플리케이션 IRQ를 선점하도록). 기능적으로는 Phase 2에선 어느 쪽이든 차이 없음 (둘 다 <2 µs).

### 4.4 메모리 레이아웃 (Phase 2 추정, 빌드 후 갱신)

```
0x0800 0000  ┌─────────────────────────┐
             │ Vector table             │
             │ .text (vendor HAL ~15K, │
             │  우리 코드 ~5K, newlib  │
             │  ~8K)                    │
             │ .rodata, .data init     │
             │ ~30 KB / 128 KB         │
0x0800 ?     └─────────────────────────┘

0x2000 0000  ┌─────────────────────────┐
             │ .data + .bss ~2 KB      │
             │ heap (미사용)            │
             │ stack (SRAM end)        │
             │ ~28 KB free / 32 KB     │
0x2000 8000  └─────────────────────────┘
```

> 실제 값은 빌드 후 `arm-none-eabi-size build/gds_us_ctrl.elf` 결과로 갱신.

### 4.5 실패 시나리오

| 단계 | 실패 모드 | 증상 | 디버그 |
|------|-----------|------|--------|
| `clock_init` | PLL lock 실패 | 무한루프, LED ✗, UART ✗ | GDB attach, PC = `Error_Handler` |
| `usart6_init` | HAL_UART_Init 실패 | LED ✗, banner ✗ | 동일 |
| `tim11_init` | NVIC 설정 실패 | banner OK, LED ✗, hello ✗ | GDB `print s_ms` = 0 정지 |
| TX hang | `HAL_MAX_DELAY` blocking | banner 일부, LED ✗ | TX 라인 단락/부하 확인 |
| IRQ storm | flag 미클리어 | hello 폭주 | `__HAL_TIM_CLEAR_FLAG` 누락 점검 |

---

## 5. Build System (CMake + Toolchain + OpenOCD)

### 5.1 `fw/arm-none-eabi-gcc.cmake`

```cmake
set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

if(DEFINED ENV{STM32_TOOLCHAIN})
    set(TOOLCHAIN_PREFIX "$ENV{STM32_TOOLCHAIN}/arm-none-eabi-")
else()
    set(TOOLCHAIN_PREFIX "arm-none-eabi-")
endif()

set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_OBJCOPY      ${TOOLCHAIN_PREFIX}objcopy CACHE INTERNAL "")
set(CMAKE_OBJDUMP      ${TOOLCHAIN_PREFIX}objdump CACHE INTERNAL "")
set(CMAKE_SIZE         ${TOOLCHAIN_PREFIX}size    CACHE INTERNAL "")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CPU_FLAGS "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard")
set(CMAKE_C_FLAGS_INIT   "${CPU_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${CPU_FLAGS}")
set(CMAKE_ASM_FLAGS_INIT "${CPU_FLAGS} -x assembler-with-cpp")
```

### 5.2 `fw/CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.22)

if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/arm-none-eabi-gcc.cmake")
endif()

project(gds_us_ctrl C ASM)

set(CMAKE_C_STANDARD          11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS        OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

add_compile_options(
    -Wall -Wextra -Wundef -Wshadow
    -ffunction-sections -fdata-sections
    -fno-common
    -fstack-usage
    $<$<CONFIG:Debug>:-Og;-g3;-gdwarf-2>
    $<$<CONFIG:Release>:-O2>
    $<$<CONFIG:MinSizeRel>:-Os>
)

add_compile_definitions(
    STM32F410Rx
    USE_HAL_DRIVER
)

# vendor HAL static library
set(VENDOR ${CMAKE_CURRENT_SOURCE_DIR}/vendor)

set(HAL_SOURCES
    ${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c
    ${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c
    ${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c
    ${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c
    ${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c
    ${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr.c
    ${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr_ex.c
    ${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c
    ${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ex.c
    ${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c
    ${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma_ex.c
    # Phase 2:
    ${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c
    ${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim.c
    ${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim_ex.c
)

add_library(stm32_hal STATIC
    ${HAL_SOURCES}
    ${VENDOR}/Core/Src/system_stm32f4xx.c
)
target_include_directories(stm32_hal PUBLIC
    ${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Inc
    ${VENDOR}/Drivers/CMSIS/Device/ST/STM32F4xx/Include
    ${VENDOR}/Drivers/CMSIS/Include
    ${VENDOR}/Core/Inc
)
target_compile_options(stm32_hal PRIVATE -Wno-unused-parameter)

# 우리 어플리케이션
file(GLOB APP_SOURCES src/*.c drivers/*.c)
set(STARTUP ${VENDOR}/startup_stm32f410rbtx.s)
set(LDSCRIPT ${VENDOR}/STM32F410RBTX_FLASH.ld)

add_executable(${PROJECT_NAME}.elf
    ${APP_SOURCES}
    ${STARTUP}
)
target_include_directories(${PROJECT_NAME}.elf PRIVATE include)
target_link_libraries(${PROJECT_NAME}.elf PRIVATE stm32_hal)

target_link_options(${PROJECT_NAME}.elf PRIVATE
    -T${LDSCRIPT}
    -Wl,--gc-sections
    -Wl,-Map=${PROJECT_NAME}.map,--cref
    -Wl,--print-memory-usage
    --specs=nano.specs
    -u _printf_float
    -lc -lm -lnosys
)

add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.hex
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}.elf>
    COMMENT "Generating .bin / .hex / size"
)

# Flash / debug
set(OPENOCD_CFG ${CMAKE_CURRENT_SOURCE_DIR}/openocd/stm32f410.cfg)

add_custom_target(flash
    DEPENDS ${PROJECT_NAME}.elf
    COMMAND openocd -f ${OPENOCD_CFG}
        -c "program $<TARGET_FILE:${PROJECT_NAME}.elf> verify reset exit"
    USES_TERMINAL
)

add_custom_target(reset
    COMMAND openocd -f ${OPENOCD_CFG} -c "init; reset; exit"
    USES_TERMINAL
)

add_custom_target(server
    COMMAND openocd -f ${OPENOCD_CFG}
    USES_TERMINAL
)

add_custom_target(gdb
    DEPENDS ${PROJECT_NAME}.elf
    COMMAND ${TOOLCHAIN_PREFIX}gdb $<TARGET_FILE:${PROJECT_NAME}.elf>
        -x ${CMAKE_CURRENT_SOURCE_DIR}/openocd/debug.gdb
    USES_TERMINAL
)
```

### 5.3 `fw/openocd/stm32f410.cfg`

```tcl
source [find interface/stlink.cfg]
transport select hla_swd
source [find target/stm32f4x.cfg]
reset_config srst_only srst_nogate
```

### 5.4 `fw/openocd/debug.gdb`

```gdb
target extended-remote :3333
set print pretty on
set pagination off
monitor reset halt
load
break main
continue
```

### 5.5 사용 흐름

```bash
cd fw
cmake -B build -G Ninja
cmake --build build                                # → .elf/.bin/.hex + size 출력
cmake --build build --target flash                 # ST-LINK 통한 플래시
# GDB (별도 터미널 2개)
cmake --build build --target server                # 터미널 1
cmake --build build --target gdb                   # 터미널 2
```

### 5.6 `fw/.gitignore`

```
build/
*.map
*.bin
*.hex
*.elf
*.su
```

---

## 6. Error Handling + 검증 + 미래 자리 표시

### 6.1 3계층 에러 처리

| 계층 | 정책 |
|------|------|
| **(L1) HAL init 실패** | `Error_Handler()` → `__disable_irq()` + 무한루프. `clock_init`, `usart6_init`, `tim11_init` 모두 사용 |
| **(L2) Cortex 예외** | 일단 무한루프 (Stage A에서 register dump 추가) |
| **(L3) 런타임 비정상** | Phase 2엔 미감지 → Stage A IWDG 도입 시 자동 리셋 |

### 6.2 미래 자리 표시 (코드 주석으로만)

- `src/main.c`: `/* TODO Stage A: iwdg_init(2000); */`, `/* TODO Stage A: HAL_IWDG_Refresh(&hiwdg); */`
- `src/irq.c` HardFault: `/* TODO Stage A: register dump via mon_printf */`

### 6.3 IWDG 자리 (Stage A 활성화)

| 항목 | 결정 |
|------|------|
| 채택 | Stage A에서 활성화 |
| 위치 | `fw/drivers/iwdg.c`, `iwdg_init(uint32_t timeout_ms)` |
| 타임아웃 default | 2000 ms |
| 클럭 소스 | LSI 32 kHz |
| Refresh | `app_loop_iter` 끝에서 매 iter |

### 6.4 Phase 2 시리얼 출력 예시

```
[boot] gds_us_ctrl phase2 ready
[t=1000 ms] hello
[t=2000 ms] hello
[t=3000 ms] hello
...
```

LED (PB3): 0.5 Hz 사각파 (1 Hz toggle = 주기 2 s).

> Phase 2 LED는 임시. Stage A에서 `CON_OVLD` 본용도 복원 시 별도 디버그 LED 핀으로 이동 또는 LED 제거. 회로도/`pinmap.md` 재확인 필요.

### 6.5 Phase 1+2 통합 완료 체크리스트

**빌드/툴체인**
- [ ] `arm-none-eabi-gcc` 설치 + `$STM32_TOOLCHAIN` 또는 PATH 등록
- [ ] OpenOCD 설치 + ST-LINK 인식
- [ ] `cmake -B build -G Ninja` 무경고
- [ ] `cmake --build build` 무경고, `.elf/.bin/.hex` 생성, size 출력 확인

**Phase 1 부팅**
- [ ] `cmake --build build --target flash` 성공
- [ ] OpenOCD attach 후 PC = `main()`의 `while(1)` 영역
- [ ] `print SystemCoreClock` = 96000000

**Phase 2 동작**
- [ ] PB3 LED 0.5 Hz 사각파
- [ ] USART6 PC6/PC7 시리얼 115200 8N1 출력 6.4 예시와 일치
- [ ] 5분 연속 운영, HardFault 진입 ✗
- [ ] GDB attach 시 `s_ms` 단조증가 (시간당 약 3,600,000 ± 0.1%)
- [ ] linker map의 `.text` ≤ 40 KB, `.data + .bss` ≤ 4 KB

**문서 동기화**
- [ ] `CLAUDE.md`: CubeMX 미사용 + 96 MHz + `fw/vendor/` 구조로 갱신
- [ ] `docs/pinmap.md`: PB3 heartbeat 임시 표시
- [ ] `docs/changelog.md`: Phase 1+2 완료 기록
- [ ] `docs/historical/gds_usctrl.ioc`: 격하 + README 한 줄
- [ ] `docs/superpowers/RESUME.md`: archive 또는 삭제
- [ ] `graphify-out/`: 재생성

### 6.6 Stage A 인계 사항

| Stage A 진입점 | 의존하는 Phase 2 산출 |
|----------------|---------------------|
| `drivers/usart1.c` (DGUS LCD) | `usart6_init` 패턴 복제 |
| `drivers/i2c1.c` (EEPROM) | `hal_conf.h`의 I2C 모듈 enable |
| `drivers/adc.c` (US_PWR_FB / US_FREQ_FB) | `tim11_init` 패턴 + ADC trigger |
| `src/iwdg.c` | `main.c` TODO 주석 자리 |
| `src/fault.c` | `mon_printf` 사용해 register dump |
| heartbeat LED | 별도 핀 또는 제거 (CON_OVLD 본용도 복원) |
| `CTRL_OSC0..4` | `board_set_ctrl_osc(uint8_t mask)` 추가 |

→ Phase 2의 모듈 분할(`board`/`sys_tick`/`mon`/`app`)은 Stage A에서 그대로 재활용. 새 페리페럴은 `drivers/`만 추가.

---

## 부록 A. 의존성 / 외부 도구

| 항목 | 버전 권고 | 용도 |
|------|-----------|------|
| arm-none-eabi-gcc | 13.x 이상 | 컴파일 |
| CMake | 3.22 이상 | 빌드 시스템 |
| Ninja | 1.10 이상 | 빌드 백엔드 |
| OpenOCD | 0.12 이상 | 플래시·디버그 |
| ST-LINK | V2 또는 V3 | SWD 프로브 |

## 부록 B. 변경된 프로젝트 문서

다음 문서는 본 spec 채택 후 정정 필요:

| 파일 | 변경 |
|------|------|
| `CLAUDE.md` | "CubeMX + CMake" → "CMake (HAL/CMSIS in-tree, CubeMX UI 미사용)"; 100 MHz → 96 MHz; 디렉토리 트리 갱신; "CubeMX 재생성 후..." 문장 삭제 |
| `docs/pinmap.md` | PB3 heartbeat 임시 표시 |
| `docs/NEXT_STEPS.md` | Phase 1, 2 항목을 본 spec 참조로 단순화 |
| `docs/changelog.md` | Phase 1+2 설계 합의 기록 |
| `docs/superpowers/RESUME.md` | 역할 종료 — 본 spec으로 대체 |

