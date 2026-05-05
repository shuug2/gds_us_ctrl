# Stage A LCD I/O Bring-up Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** STM32F410RBT 보드에서 USART1 (PA9/PA10, 115200 8N1)로 DGUS LCD 와 통신해 부팅 시 페이지 전환 + 1 Hz uptime 카운터를 LCD `VAR_POWER` (0x1110) VP에 표시 + 사용자 터치 키 RX 프레임을 mon log 로 출력하는 데까지 도달.

**Architecture:** Phase 1+2 의 "raw periph driver + wrapper" 패턴을 미러링한 2층 분리. `fw/drivers/usart1.c` (USART1 raw I/O — init / GPIO AF / NVIC / IRQ / RX 링 / 폴링 TX) + `fw/drivers/dgus_lcd.c` (DGUS 프로토콜 — 9 함수 풀 패리티 + RX 파서 상태머신). 폴링 TX + IT 1-byte RX 전략 (samd20 패턴 STM32 HAL 미러). samd20 결함 5건(LEN 무경계, byte-counted 타임아웃, 무한 retry init, shared TX race, ring wrap-overwrite) 명시 회피.

**Tech Stack:** STM32 HAL F4 v1.27 (vendor in-tree), arm-none-eabi-gcc 13.x+, CMake 3.22+, Ninja, OpenOCD 0.12+, ST-LINK V3.

**Spec reference:** `docs/superpowers/specs/2026-05-05-stage-a-lcd-io-design.md`

**Base branch:** `feat/stage-a-lcd-io` (off `main` @ Phase 1+2 merge)

---

## File Structure

### Files to create (new)
| 파일 | 책임 |
|------|------|
| `fw/include/usart1.h` | USART1 raw 공개 API — `usart1_init`, `usart1_send_blocking`, `usart1_rx_pop`, `usart1_rx_drop_count` |
| `fw/include/dgus_lcd.h` | DGUS 공개 API + 프로토콜 상수 + `dgus_frame_t` + 데모 매크로 (samd20 dgus_lcd.h 포팅) |
| `fw/drivers/usart1.c` | USART1 raw I/O — init + GPIO AF7 (PA9/PA10) + NVIC priority 5 + `USART1_IRQHandler` 콜백 + RX 64-byte 링 + 폴링 TX |
| `fw/drivers/dgus_lcd.c` | DGUS 프로토콜 — 9 TX 빌더 + RX 파서 상태머신 + echo 헬퍼 + 관측성 카운터 + dgus_init |

### Files to modify (Phase 2 자산 — 추가만, 삭제 ✗)
| 파일 | 변경 |
|------|------|
| `fw/include/periph.h` | `extern UART_HandleTypeDef huart1;` 1줄 추가 |
| `fw/src/periph.c` | `UART_HandleTypeDef huart1;` 단일 정의 1줄 추가 |
| `fw/src/irq.c` | `USART1_IRQHandler` weak alias override 추가 (HAL_UART_IRQHandler 호출) |
| `fw/src/main.c` | `usart1_init()` + `dgus_init()` init 시퀀스에 추가 |
| `fw/src/app.c` | `app_init` 확장 (banner + `dgus_set_page`) + `app_loop_iter` 확장 (RX drain + 1Hz `dgus_write_u16`) |
| `docs/pinmap.md` | USART1 사용 항목 표시 (chunk 7) |
| `docs/changelog.md` | Stage A 완료 항목 추가 (chunk 7) |
| `docs/superpowers/historical/<chunk7-date>-RESUME.md` | RESUME 아카이브 (chunk 7) |

### Files NOT to touch
- `fw/vendor/**` — read-only (Phase 1+2 결정)
- `fw/CMakeLists.txt` — `drivers/*.c` GLOB이 신규 `usart1.c` / `dgus_lcd.c` 자동 흡수. 변경 ✗ 확인만
- `fw/vendor/Core/Inc/stm32f4xx_hal_conf.h` — `HAL_UART_MODULE_ENABLED` 이미 활성 (Phase 2 USART6에서 켜짐). 변경 ✗
- 기타 모든 Phase 1+2 파일

### File responsibility summary

| 파일 | 책임 |
|------|------|
| `fw/include/usart1.h` | usart1.c 의 좁은 공개 API. dgus_lcd.c 외 호출 ✗ |
| `fw/drivers/usart1.c` | USART1 페리페럴 자체 + RX 링. DGUS 프로토콜 인지 ✗ (byte stream만) |
| `fw/include/dgus_lcd.h` | DGUS 프로토콜 상수 (samd20 포팅) + 공개 API 시그니처 + 데모 설정 매크로 |
| `fw/drivers/dgus_lcd.c` | DGUS 프레임 빌더 + RX 파서. USART1 raw API 만 의존 |
| `fw/src/periph.c` | huart1 단일 정의 (단일 정의 디시플린) |
| `fw/src/irq.c` | USART1_IRQHandler weak override |
| `fw/src/main.c` | init 순서: HAL_Init → clock → usart6 → **usart1** → tim11 → board → **dgus** → app (Phase 2 form: sys_tick_init 은 app_init 내부) |
| `fw/src/app.c` | mon banner + dgus_set_page + 매 iter RX drain + 1Hz dgus_write_u16 cadence |

---

## Task 1: 헤더 — `fw/include/usart1.h`

**Files:**
- Create: `fw/include/usart1.h`

- [ ] **Step 1: 헤더 파일 작성**

```c
/* fw/include/usart1.h
 *
 * USART1 raw I/O — DGUS LCD 통신용.
 * Phase 2 mon_usart6 패턴의 raw 레이어 미러.
 *
 * 호출자: fw/drivers/dgus_lcd.c 만. 다른 모듈에서 직접 호출 ✗.
 */
#ifndef GDS_USART1_H_
#define GDS_USART1_H_

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"

/* USART1: PA9 (TX, AF7) + PA10 (RX, AF7), 115200 8N1, no flow control.
 * NVIC priority = 5 (TIM11 sys_tick 와 동일 — 둘 다 짧은 ISR, queueing).
 * RX: HAL_UART_Receive_IT 1-byte 무장, RxCpltCallback 에서 ring push + 재무장.
 */
void usart1_init(void);

/* 폴링 TX. 10 ms timeout — 16 B 프레임 @ 115200 ≈ 1.4 ms 의 7배 여유.
 * HAL_OK / HAL_TIMEOUT / HAL_ERROR / HAL_BUSY 반환.
 */
HAL_StatusTypeDef usart1_send_blocking(const uint8_t *buf, uint16_t len);

/* RX 링 버퍼에서 1 byte pop. 비어 있으면 false 반환, *out_byte 미변경.
 * 호출자: dgus_lcd.c 의 파서 only.
 */
bool usart1_rx_pop(uint8_t *out_byte);

/* RX 링 full 또는 HAL_UART_Receive_IT 재무장 실패로 폐기된 byte 누적.
 * Stage A 데모는 검증용 (chunk 6f 에서 GDB read).
 */
uint16_t usart1_rx_drop_count(void);

#endif /* GDS_USART1_H_ */
```

- [ ] **Step 2: 헤더 단독 컴파일 sanity 체크**

(전체 빌드는 chunk 5에서. 이 시점엔 컴파일 가능성만 확인)

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl-stageA
env -u STM32_TOOLCHAIN arm-none-eabi-gcc -c -fsyntax-only \
  -DSTM32F410Rx -DUSE_HAL_DRIVER \
  -Ifw/include -Ifw/vendor/Drivers/STM32F4xx_HAL_Driver/Inc \
  -Ifw/vendor/Drivers/CMSIS/Device/ST/STM32F4xx/Include \
  -Ifw/vendor/Drivers/CMSIS/Include \
  -Ifw/vendor/Core/Inc \
  -x c fw/include/usart1.h
```

Expected: warning 0개. (만약 STM32_TOOLCHAIN env 이슈 시 `env -u STM32_TOOLCHAIN` 으로 우회.)

- [ ] **Step 3: Commit**

```bash
git add fw/include/usart1.h
git -c commit.gpgsign=false commit -m "feat: add usart1.h public API for DGUS raw I/O"
```

---

## Task 2: 헤더 — `fw/include/dgus_lcd.h` (samd20 dgus_lcd.h 포팅)

**Files:**
- Create: `fw/include/dgus_lcd.h`

- [ ] **Step 1: 헤더 파일 작성** (samd20 `ref/samd20/dgus_lcd.h` 의 LCD 매크로 포팅 + STM32 컨벤션 API)

```c
/* fw/include/dgus_lcd.h
 *
 * DGUS LCD 프로토콜 레이어 — samd20 ref 포팅 (9 함수 풀 패리티).
 *
 * 프레임 포맷: 5A A5 LEN CMD ADDR_H ADDR_L [payload...]
 *   CMD: 0x82 = WR (echo 포함), 0x83 = RD (응답 / 터치 키)
 *   LEN = CMD(1) + ADDR(2) + payload byte 수. 유효 범위 [4, 26].
 *   Wire = big-endian (samd20 동일).
 */
#ifndef GDS_DGUS_LCD_H_
#define GDS_DGUS_LCD_H_

#include <stdint.h>
#include <stdbool.h>

/*--------------------------------------------------------------
 * 프로토콜 상수 (samd20 ref/samd20/dgus_lcd.h verbatim)
 *--------------------------------------------------------------*/

/* Sync header */
#define DGUS_SYNC1   0x5A
#define DGUS_SYNC2   0xA5

/* Commands */
#define DGUS_CMD_WR  0x82
#define DGUS_CMD_RD  0x83

/* LCD page IDs (samd20 dgus_lcd.h 포팅) */
#define LCD_LOGO          0
#define LCD_MODEL_SETUP   1
#define LCD_RUN_MULTI     3
#define LCD_SETUP_MULTI   5
#define LCD_RUN_HAND      3
#define LCD_SETUP_HAND    7
#define LCD_RUN_STD       9
#define LCD_SETUP_STD1   10
#define LCD_SETUP_STD2D  12
#define LCD_SETUP_STD2T  13
#define LCD_SETUP_STD3   15
#define LCD_WARNING      17
#define LCD_SETUP_MH2    19
#define LCD_SETUP_MHC    21
#define LCD_SETUP_STDC   23
#define LCD_SETUP_MHE    25
#define LCD_SETUP_STDE   27

/* System VPs */
#define VP_LCD_RESET     0x0004
#define VP_LCD_SETPAGE   0x0084
#define SYS_PIC_NOW      0x0014

/* Application VPs (samd20 verbatim — Stage A 에선 VAR_POWER 만 데모에 사용) */
#define MODEL_NAME       0x1000
#define LV_WORK_CNT      0x1006
#define VAR_POWER        0x1110
#define VAR_AMP          0x1111
#define VAR_FREQ         0x1112
#define VAR_ENERGY       0x1113
#define LV_OUTPUT        0x1170

/* (samd20 의 다른 VP 매크로들은 후속 슬라이스에서 추가. Stage A 데모 범위 외) */

/*--------------------------------------------------------------
 * 프레임 / API
 *--------------------------------------------------------------*/

#define DGUS_MAX_DATA  23   /* payload 최대 = LEN_max(26) - cmd(1) - addr(2) */

typedef struct {
    uint8_t  cmd;                       /* 0x82 (WR/echo) or 0x83 (RD) */
    uint16_t vp_addr;
    uint8_t  data_len;                  /* payload 만의 길이 (= LEN - 3) */
    uint8_t  data[DGUS_MAX_DATA];
} dgus_frame_t;

/* 파서/카운터 상태 클리어. usart1_init 후 호출. */
void dgus_init(void);

/* TX builders — samd20 9 함수 풀 패리티 */
void dgus_reset_lcd(void);
void dgus_set_page(uint8_t page);
void dgus_write_u16(uint16_t vp, uint16_t value);
void dgus_write_u32(uint16_t vp, uint32_t value);
void dgus_write_bytes(uint16_t vp, const uint8_t *buf, uint8_t n);
void dgus_write_u16_array(uint16_t vp, const uint16_t *buf, uint8_t n);
void dgus_write_text(uint16_t vp, const char *txt);   /* 10 byte zero-pad */
void dgus_read_var(uint8_t var);                      /* 0x83 RD len=1 word */

/* RX */
bool dgus_rx_poll(dgus_frame_t *out);
bool dgus_is_echo(const dgus_frame_t *f);             /* cmd == 0x82 */

/* 관측성 (chunk 6f GDB read 용) */
uint16_t dgus_rx_drop_count(void);
uint16_t dgus_tx_timeout_count(void);

/*--------------------------------------------------------------
 * Stage A 데모 설정 (사용자가 보드에 맞춰 수정 가능)
 *--------------------------------------------------------------*/

#define DGUS_DEMO_BOOT_PAGE      LCD_RUN_STD    /* 9 — VAR_POWER 시각화되는 페이지 */
#define DGUS_DEMO_UPTIME_VP      VAR_POWER      /* 0x1110 — U16 (wrap @ 65535초) */
#define DGUS_DEMO_RESET_ON_BOOT  0              /* 0=skip / 1=부팅 시 LCD 풀-재시작 */

#endif /* GDS_DGUS_LCD_H_ */
```

- [ ] **Step 2: 헤더 단독 컴파일 sanity 체크**

```bash
env -u STM32_TOOLCHAIN arm-none-eabi-gcc -c -fsyntax-only \
  -DSTM32F410Rx -DUSE_HAL_DRIVER \
  -Ifw/include -Ifw/vendor/Drivers/STM32F4xx_HAL_Driver/Inc \
  -Ifw/vendor/Drivers/CMSIS/Device/ST/STM32F4xx/Include \
  -Ifw/vendor/Drivers/CMSIS/Include \
  -Ifw/vendor/Core/Inc \
  -x c fw/include/dgus_lcd.h
```

Expected: warning 0개.

- [ ] **Step 3: Commit**

```bash
git add fw/include/dgus_lcd.h
git -c commit.gpgsign=false commit -m "feat: add dgus_lcd.h ported from samd20 (9-fn parity + demo macros)"
```

---

## Task 3: HAL 핸들 단일 정의 — `huart1` 추가

**Files:**
- Modify: `fw/include/periph.h`
- Modify: `fw/src/periph.c`

- [ ] **Step 1: `fw/include/periph.h` 에 extern 선언 추가**

```bash
# 현재 파일 확인
grep -n "extern" fw/include/periph.h
```

Expected output: 기존 `extern UART_HandleTypeDef huart6;` + `extern TIM_HandleTypeDef htim11;` 두 줄.

`fw/include/periph.h` 의 `extern TIM_HandleTypeDef htim11;` 직전에 한 줄 추가:
```c
extern UART_HandleTypeDef huart1;   /* USART1 — DGUS LCD (Stage A) */
```

- [ ] **Step 2: `fw/src/periph.c` 에 단일 정의 추가**

`fw/src/periph.c` 의 `TIM_HandleTypeDef htim11;` 직전에 한 줄 추가:
```c
UART_HandleTypeDef huart1;   /* USART1 — DGUS LCD (Stage A) */
```

- [ ] **Step 3: 컴파일 가능성 확인**

```bash
env -u STM32_TOOLCHAIN arm-none-eabi-gcc -c -fsyntax-only \
  -DSTM32F410Rx -DUSE_HAL_DRIVER \
  -Ifw/include -Ifw/vendor/Drivers/STM32F4xx_HAL_Driver/Inc \
  -Ifw/vendor/Drivers/CMSIS/Device/ST/STM32F4xx/Include \
  -Ifw/vendor/Drivers/CMSIS/Include \
  -Ifw/vendor/Core/Inc \
  -x c fw/src/periph.c
```

Expected: warning 0개.

- [ ] **Step 4: Commit**

```bash
git add fw/include/periph.h fw/src/periph.c
git -c commit.gpgsign=false commit -m "feat: add huart1 single definition in periph (Stage A)"
```

---

## Task 4: USART1 raw 드라이버 본체 — `fw/drivers/usart1.c`

**Files:**
- Create: `fw/drivers/usart1.c`

- [ ] **Step 1: 드라이버 파일 작성**

```c
/* fw/drivers/usart1.c
 *
 * USART1 raw I/O — DGUS LCD (PA9/PA10, AF7, 115200 8N1).
 * Phase 2 fw/drivers/usart.c (USART6) 패턴 미러 + RX IT 경로 추가.
 *
 * 호출자: fw/drivers/dgus_lcd.c 만 (단일 정의 디시플린).
 */
#include "usart1.h"
#include "periph.h"
#include "stm32f4xx_hal.h"

/* 64 = 2의 거듭제곱 → mask 분기 없음 */
#define RX_RING_SIZE  64

static volatile uint8_t  s_rx_byte;                     /* HAL_UART_Receive_IT 1바이트 도착지 */
static volatile uint8_t  s_rx_ring[RX_RING_SIZE];
static volatile uint8_t  s_rx_head;                     /* IRQ writer */
static volatile uint8_t  s_rx_tail;                     /* loop reader */
static volatile uint16_t s_rx_drop_count;               /* full / 재무장 실패 누적 */

extern void Error_Handler(void);                        /* fw/src/clock.c 정의 */

void usart1_init(void)
{
    /* 1. 클럭 */
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* 2. GPIO PA9/PA10 AF7 */
    GPIO_InitTypeDef g = {0};
    g.Pin       = GPIO_PIN_9 | GPIO_PIN_10;
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &g);

    /* 3. UART config */
    huart1.Instance          = USART1;
    huart1.Init.BaudRate     = 115200;
    huart1.Init.WordLength   = UART_WORDLENGTH_8B;
    huart1.Init.StopBits     = UART_STOPBITS_1;
    huart1.Init.Parity       = UART_PARITY_NONE;
    huart1.Init.Mode         = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK) {
        Error_Handler();
    }

    /* 4. NVIC — TIM11 과 동일한 priority 5 (양 ISR 짧음, queueing) */
    HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);

    /* 5. ring 클리어 */
    s_rx_head       = 0;
    s_rx_tail       = 0;
    s_rx_drop_count = 0;

    /* 6. 첫 RX 무장 — __enable_irq 후 byte 도착 시 콜백 발화 */
    if (HAL_UART_Receive_IT(&huart1, (uint8_t *)&s_rx_byte, 1) != HAL_OK) {
        Error_Handler();
    }
}

HAL_StatusTypeDef usart1_send_blocking(const uint8_t *buf, uint16_t len)
{
    /* 폴링 TX. 10 ms timeout — 최대 프레임 16 B × 87 µs/byte ≈ 1.4 ms 의 7배 여유.
     * LCD 케이블 빠짐/멈춤 시 HAL_TIMEOUT 반환 → caller 가 카운터로 처리, fault 진입 ✗.
     */
    return HAL_UART_Transmit(&huart1, (uint8_t *)buf, len, /*timeout_ms=*/10);
}

bool usart1_rx_pop(uint8_t *out_byte)
{
    if (s_rx_tail == s_rx_head) {
        return false;
    }
    *out_byte = s_rx_ring[s_rx_tail];
    s_rx_tail = (s_rx_tail + 1) & (RX_RING_SIZE - 1);
    return true;
}

uint16_t usart1_rx_drop_count(void)
{
    return s_rx_drop_count;
}

/* HAL 콜백 — periph 핸들이 USART1 인 경우만 동작.
 * mon_usart6 (USART6) 도 같은 콜백 시그니처를 공유하므로 instance 분기 필요.
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *h)
{
    if (h->Instance != USART1) {
        return;
    }
    uint8_t next = (s_rx_head + 1) & (RX_RING_SIZE - 1);
    if (next == s_rx_tail) {
        s_rx_drop_count++;                              /* ring full — byte 폐기 (samd20 결함 #5 회피) */
    } else {
        s_rx_ring[s_rx_head] = s_rx_byte;
        s_rx_head            = next;
    }
    if (HAL_UART_Receive_IT(&huart1, (uint8_t *)&s_rx_byte, 1) != HAL_OK) {
        s_rx_drop_count++;                              /* 재무장 실패 — R5 가시화 */
    }
}
```

- [ ] **Step 2: 컴파일 가능성 확인**

```bash
env -u STM32_TOOLCHAIN arm-none-eabi-gcc -c -fsyntax-only \
  -DSTM32F410Rx -DUSE_HAL_DRIVER \
  -Ifw/include -Ifw/vendor/Drivers/STM32F4xx_HAL_Driver/Inc \
  -Ifw/vendor/Drivers/CMSIS/Device/ST/STM32F4xx/Include \
  -Ifw/vendor/Drivers/CMSIS/Include \
  -Ifw/vendor/Core/Inc \
  -x c fw/drivers/usart1.c
```

Expected: warning 0개.

- [ ] **Step 3: Commit**

```bash
git add fw/drivers/usart1.c
git -c commit.gpgsign=false commit -m "feat: add USART1 raw driver (PA9/PA10 AF7, IT 1-byte RX, polling TX)"
```

---

## Task 5: USART1 IRQ 핸들러 — `fw/src/irq.c` 확장

**Files:**
- Modify: `fw/src/irq.c`

- [ ] **Step 1: 현재 파일 확인**

```bash
grep -n "TIM1_TRG_COM_TIM11_IRQHandler\|USART" fw/src/irq.c
```

Expected: TIM11 핸들러는 존재. USART1 은 아직 없음.

- [ ] **Step 2: USART1_IRQHandler weak override 추가**

`fw/src/irq.c` 끝에 (또는 TIM11 핸들러 뒤에) 추가:
```c
/* USART1 — DGUS LCD (Stage A).
 * vendor startup 의 weak alias 를 override.
 * HAL_UART_IRQHandler 가 RxCpltCallback (fw/drivers/usart1.c) 으로 분기.
 */
void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}
```

`fw/src/irq.c` 상단 include 에 `#include "periph.h"` 가 이미 있는지 확인. 없으면 추가.

- [ ] **Step 3: 컴파일 가능성 확인**

```bash
env -u STM32_TOOLCHAIN arm-none-eabi-gcc -c -fsyntax-only \
  -DSTM32F410Rx -DUSE_HAL_DRIVER \
  -Ifw/include -Ifw/vendor/Drivers/STM32F4xx_HAL_Driver/Inc \
  -Ifw/vendor/Drivers/CMSIS/Device/ST/STM32F4xx/Include \
  -Ifw/vendor/Drivers/CMSIS/Include \
  -Ifw/vendor/Core/Inc \
  -x c fw/src/irq.c
```

Expected: warning 0개.

- [ ] **Step 4: Commit**

```bash
git add fw/src/irq.c
git -c commit.gpgsign=false commit -m "feat: add USART1_IRQHandler weak override (DGUS LCD)"
```

---

## Task 6: DGUS 프로토콜 — 정적 상태 + `dgus_init` + 관측성 카운터

**Files:**
- Create: `fw/drivers/dgus_lcd.c` (skeleton — 후속 task 에서 함수 추가)

- [ ] **Step 1: skeleton 작성** (정적 상태 + dgus_init + accessor)

```c
/* fw/drivers/dgus_lcd.c
 *
 * DGUS LCD 프로토콜 레이어 — samd20 ref 9 함수 풀 패리티.
 * USART1 raw I/O (fw/drivers/usart1.c) 위에 빌드.
 *
 * samd20 결함 회피 (spec §3.7):
 *   #1 LEN 무경계: LEN ∈ [4, 26] 검증 + drop 카운터
 *   #2 byte-counted timeout: 벽시계 ms (sys_tick) 사용
 *   #3 init 무한 retry: HAL_UART_Init 실패 시 Error_Handler (usart1.c 책임)
 *   #4 shared TX race: 폴링 TX → caller block, 자연 직렬화
 *   #5 RX ring wrap-overwrite: drop + 카운터 (usart1.c 책임)
 */
#include "dgus_lcd.h"
#include "usart1.h"
#include "sys_tick.h"
#include <string.h>

/*--------------------------------------------------------------
 * 정적 상태
 *--------------------------------------------------------------*/

/* TX — 단일 정적 버퍼. 폴링 TX 이므로 caller 가 send_frame 동안 block,
 * 다른 호출과 자연 직렬화 (samd20 결함 #4 회피).
 */
static uint8_t s_tx_buf[32];                            /* 헤더(6) + payload max(23) + α */

/* TX 카운터 */
static uint16_t s_dgus_tx_timeout_count;

/* RX 파서 상태머신 */
typedef enum {
    PS_IDLE,
    PS_GOT_5A,
    PS_GOT_HEADER,
    PS_COLLECTING
} parse_state_t;

static parse_state_t s_parse_state;
static uint8_t       s_frame_buf[32];                   /* LEN_max + 약간 여유 */
static uint8_t       s_frame_idx;
static uint8_t       s_bytes_remaining;
static uint32_t      s_frame_start_ms;                  /* 벽시계 timeout (samd20 결함 #2 회피) */
static uint16_t      s_dgus_rx_drop_count;

/*--------------------------------------------------------------
 * 공개 — init / accessor
 *--------------------------------------------------------------*/

void dgus_init(void)
{
    s_parse_state           = PS_IDLE;
    s_frame_idx             = 0;
    s_bytes_remaining       = 0;
    s_frame_start_ms        = 0;
    s_dgus_rx_drop_count    = 0;
    s_dgus_tx_timeout_count = 0;
    /* USART1 / RX ring 책임은 usart1_init. 여기선 dgus 레이어 상태만 */
}

uint16_t dgus_rx_drop_count(void)
{
    return s_dgus_rx_drop_count;
}

uint16_t dgus_tx_timeout_count(void)
{
    return s_dgus_tx_timeout_count;
}

bool dgus_is_echo(const dgus_frame_t *f)
{
    return f->cmd == DGUS_CMD_WR;
}

/* (TX builders는 task 7, RX 파서는 task 8 에서 추가) */
```

- [ ] **Step 2: 컴파일 가능성 확인** (TX/RX 함수 미정의이므로 dgus_lcd.h 의 함수 선언과 mismatch — 이 단계에선 skeleton 만 컴파일)

```bash
env -u STM32_TOOLCHAIN arm-none-eabi-gcc -c -fsyntax-only \
  -DSTM32F410Rx -DUSE_HAL_DRIVER \
  -Ifw/include -Ifw/vendor/Drivers/STM32F4xx_HAL_Driver/Inc \
  -Ifw/vendor/Drivers/CMSIS/Device/ST/STM32F4xx/Include \
  -Ifw/vendor/Drivers/CMSIS/Include \
  -Ifw/vendor/Core/Inc \
  -x c fw/drivers/dgus_lcd.c
```

Expected: warning 0개. (link 시 unresolved symbol 은 task 7/8 에서 해결.)

- [ ] **Step 3: Commit**

```bash
git add fw/drivers/dgus_lcd.c
git -c commit.gpgsign=false commit -m "feat: dgus_lcd.c skeleton — static state + dgus_init + counters"
```

---

## Task 7: DGUS TX 빌더 — 9 함수 (samd20 풀 패리티)

**Files:**
- Modify: `fw/drivers/dgus_lcd.c`

- [ ] **Step 1: send_frame 헬퍼 + 9 공개 함수 추가**

`fw/drivers/dgus_lcd.c` 끝에 추가 (skeleton 다음):

```c
/*--------------------------------------------------------------
 * TX 빌더 — samd20 9 함수 풀 패리티
 *--------------------------------------------------------------*/

/* 단일 프레임 전송 helper. plen = payload byte 수.
 * 프레임: 5A A5 LEN(=3+plen) cmd AH AL [payload].
 */
static void send_frame(uint8_t cmd, uint16_t vp, const uint8_t *payload, uint8_t plen)
{
    s_tx_buf[0] = DGUS_SYNC1;
    s_tx_buf[1] = DGUS_SYNC2;
    s_tx_buf[2] = (uint8_t)(3 + plen);                  /* LEN = cmd+addr(3) + payload */
    s_tx_buf[3] = cmd;
    s_tx_buf[4] = (uint8_t)(vp >> 8);
    s_tx_buf[5] = (uint8_t)(vp & 0xFF);
    for (uint8_t i = 0; i < plen; i++) {
        s_tx_buf[6 + i] = payload[i];
    }
    if (usart1_send_blocking(s_tx_buf, (uint16_t)(6 + plen)) == HAL_TIMEOUT) {
        s_dgus_tx_timeout_count++;
    }
}

/* samd20 reset_dgus_lcd: VP_LCD_RESET 0x04, payload = 55 AA 5A A5 (DWIN reset magic) */
void dgus_reset_lcd(void)
{
    static const uint8_t magic[4] = { 0x55, 0xAA, 0x5A, 0xA5 };
    send_frame(DGUS_CMD_WR, VP_LCD_RESET, magic, 4);
}

/* samd20 set_lcd_page: VP_LCD_SETPAGE 0x84, payload = 5A 01 00 page */
void dgus_set_page(uint8_t page)
{
    uint8_t pl[4] = { 0x5A, 0x01, 0x00, page };
    send_frame(DGUS_CMD_WR, VP_LCD_SETPAGE, pl, 4);
}

/* samd20 send_lcd_data_var */
void dgus_write_u16(uint16_t vp, uint16_t value)
{
    uint8_t pl[2] = {
        (uint8_t)(value >> 8),
        (uint8_t)(value & 0xFF),
    };
    send_frame(DGUS_CMD_WR, vp, pl, 2);
}

/* samd20 send_lcd_data_word — big-endian 4 byte */
void dgus_write_u32(uint16_t vp, uint32_t value)
{
    uint8_t pl[4] = {
        (uint8_t)(value >> 24),
        (uint8_t)(value >> 16),
        (uint8_t)(value >> 8),
        (uint8_t)(value & 0xFF),
    };
    send_frame(DGUS_CMD_WR, vp, pl, 4);
}

/* samd20 send_lcd_byte_array */
void dgus_write_bytes(uint16_t vp, const uint8_t *buf, uint8_t n)
{
    if (n > DGUS_MAX_DATA) {
        n = DGUS_MAX_DATA;                              /* 무경계 입력 안전 */
    }
    send_frame(DGUS_CMD_WR, vp, buf, n);
}

/* samd20 send_lcd_int_array — u16 array, big-endian */
void dgus_write_u16_array(uint16_t vp, const uint16_t *buf, uint8_t n)
{
    if (n > (DGUS_MAX_DATA / 2)) {
        n = (uint8_t)(DGUS_MAX_DATA / 2);
    }
    uint8_t pl[DGUS_MAX_DATA];
    for (uint8_t i = 0; i < n; i++) {
        pl[(i * 2) + 0] = (uint8_t)(buf[i] >> 8);
        pl[(i * 2) + 1] = (uint8_t)(buf[i] & 0xFF);
    }
    send_frame(DGUS_CMD_WR, vp, pl, (uint8_t)(n * 2));
}

/* samd20 send_lcd_txt — 10 byte zero-pad. txt 가 NULL 또는 빈 문자열이면 nothing 송신.
 * samd20 라인 257-283 의 행동을 그대로 보존.
 */
void dgus_write_text(uint16_t vp, const char *txt)
{
    if (txt == 0 || txt[0] == '\0') {
        return;
    }
    uint8_t pl[10];
    for (uint8_t i = 0; i < 10; i++) {
        pl[i] = 0;
    }
    for (uint8_t i = 0; i < 10; i++) {
        if (txt[i] == '\0') {
            break;
        }
        pl[i] = (uint8_t)txt[i];
    }
    send_frame(DGUS_CMD_WR, vp, pl, 10);
}

/* samd20 read_system_var — RD 0x83, payload = 1 (1 word read) */
void dgus_read_var(uint8_t var)
{
    uint8_t pl[1] = { 1 };
    send_frame(DGUS_CMD_RD, (uint16_t)var, pl, 1);
}
```

- [ ] **Step 2: 컴파일 가능성 확인**

```bash
env -u STM32_TOOLCHAIN arm-none-eabi-gcc -c -fsyntax-only \
  -DSTM32F410Rx -DUSE_HAL_DRIVER \
  -Ifw/include -Ifw/vendor/Drivers/STM32F4xx_HAL_Driver/Inc \
  -Ifw/vendor/Drivers/CMSIS/Device/ST/STM32F4xx/Include \
  -Ifw/vendor/Drivers/CMSIS/Include \
  -Ifw/vendor/Core/Inc \
  -x c fw/drivers/dgus_lcd.c
```

Expected: warning 0개.

- [ ] **Step 3: Commit**

```bash
git add fw/drivers/dgus_lcd.c
git -c commit.gpgsign=false commit -m "feat: dgus_lcd.c TX builders (9-fn samd20 parity)"
```

---

## Task 8: DGUS RX 파서 상태머신 + `dgus_rx_poll`

**Files:**
- Modify: `fw/drivers/dgus_lcd.c`

- [ ] **Step 1: 파서 상태머신 + dgus_rx_poll 추가**

`fw/drivers/dgus_lcd.c` 끝에 추가 (TX 빌더 뒤):

```c
/*--------------------------------------------------------------
 * RX 파서 상태머신
 *
 * 프레임: 5A A5 LEN [LEN bytes = cmd + addr_h + addr_l + payload]
 * LEN ∈ [4, 26]. 그 외 → 폐기 + drop 카운터.
 * 프레임 mid-stream 50 ms timeout (벽시계 sys_tick_get_ms).
 *--------------------------------------------------------------*/

#define DGUS_LEN_MIN          4
#define DGUS_LEN_MAX          26
#define DGUS_FRAME_TIMEOUT_MS 50

/* 1바이트 처리. true 리턴 시 out 에 완성 프레임 적재. */
static bool parser_step(uint8_t b, dgus_frame_t *out)
{
    switch (s_parse_state) {

    case PS_IDLE:
        if (b == DGUS_SYNC1) {
            s_parse_state = PS_GOT_5A;
        }
        return false;

    case PS_GOT_5A:
        if (b == DGUS_SYNC2) {
            s_parse_state    = PS_GOT_HEADER;
            s_frame_start_ms = sys_tick_get_ms();
        } else if (b == DGUS_SYNC1) {
            /* 연속 0x5A: PS_GOT_5A 유지 */
        } else {
            s_parse_state = PS_IDLE;
        }
        return false;

    case PS_GOT_HEADER:
        /* 이 byte 가 LEN */
        if (b < DGUS_LEN_MIN || b > DGUS_LEN_MAX) {
            s_dgus_rx_drop_count++;                     /* samd20 결함 #1 회피 */
            s_parse_state = PS_IDLE;
            return false;
        }
        s_bytes_remaining = b;
        s_frame_idx       = 0;
        s_parse_state     = PS_COLLECTING;
        return false;

    case PS_COLLECTING:
        /* 벽시계 timeout — samd20 결함 #2 회피 */
        if ((uint32_t)(sys_tick_get_ms() - s_frame_start_ms) > DGUS_FRAME_TIMEOUT_MS) {
            s_dgus_rx_drop_count++;
            s_parse_state = PS_IDLE;
            return false;
        }
        s_frame_buf[s_frame_idx++] = b;
        s_bytes_remaining--;
        if (s_bytes_remaining == 0) {
            /* 프레임 완성 — frame_buf 매핑:
             *   [0] = cmd, [1] = addr_h, [2] = addr_l, [3..] = payload
             *   payload 길이 = LEN - 3 = s_frame_idx - 3
             */
            out->cmd      = s_frame_buf[0];
            out->vp_addr  = (uint16_t)((s_frame_buf[1] << 8) | s_frame_buf[2]);
            out->data_len = (uint8_t)(s_frame_idx - 3);
            for (uint8_t i = 0; i < out->data_len && i < DGUS_MAX_DATA; i++) {
                out->data[i] = s_frame_buf[3 + i];
            }
            s_parse_state = PS_IDLE;
            return true;
        }
        return false;
    }
    /* unreachable */
    s_parse_state = PS_IDLE;
    return false;
}

bool dgus_rx_poll(dgus_frame_t *out)
{
    uint8_t b;
    /* ring 을 한 프레임 완성될 때까지, 또는 비울 때까지 소비 */
    while (usart1_rx_pop(&b)) {
        if (parser_step(b, out)) {
            return true;
        }
    }
    return false;
}
```

- [ ] **Step 2: 컴파일 가능성 확인**

```bash
env -u STM32_TOOLCHAIN arm-none-eabi-gcc -c -fsyntax-only \
  -DSTM32F410Rx -DUSE_HAL_DRIVER \
  -Ifw/include -Ifw/vendor/Drivers/STM32F4xx_HAL_Driver/Inc \
  -Ifw/vendor/Drivers/CMSIS/Device/ST/STM32F4xx/Include \
  -Ifw/vendor/Drivers/CMSIS/Include \
  -Ifw/vendor/Core/Inc \
  -x c fw/drivers/dgus_lcd.c
```

Expected: warning 0개.

- [ ] **Step 3: Commit**

```bash
git add fw/drivers/dgus_lcd.c
git -c commit.gpgsign=false commit -m "feat: dgus_lcd.c RX parser state machine + dgus_rx_poll"
```

---

## Task 9: main.c init 시퀀스에 USART1 + DGUS 추가

**Files:**
- Modify: `fw/src/main.c`

- [ ] **Step 1: 현재 init 순서 확인**

```bash
grep -n "init\|enable_irq\|app_loop_iter" fw/src/main.c
```

Expected: Phase 2 순서 — `HAL_Init / clock_init / usart6_init / tim11_init / board_init / app_init / while(1) app_loop_iter` (Phase 2 form: sys_tick_init 은 app_init 내부에서 호출, explicit `__enable_irq()` 없음).

- [ ] **Step 2: include 추가 + init 호출 삽입**

`fw/src/main.c` 의 `#include` 섹션에 추가:
```c
#include "usart1.h"
#include "dgus_lcd.h"
```

`main()` 함수의 init 시퀀스 수정 — `usart6_init()` 호출 직후에 `usart1_init()`, `board_init()` 호출 직후에 `dgus_init()` 삽입:

```c
int main(void)
{
    HAL_Init();
    clock_init();

    usart6_init();      /* Phase 2 mon */
    usart1_init();      /* Stage A: DGUS LCD raw — GPIO AF7 + NVIC + 첫 RX 무장 */

    tim11_init();
    board_init();
    dgus_init();        /* Stage A: DGUS 프로토콜 레이어 상태 클리어 */

    app_init();         /* Phase 2 form: 내부에서 sys_tick_init + mon_init */

    while (1) {
        app_loop_iter();
    }
}
```

- [ ] **Step 3: 컴파일 가능성 확인**

```bash
env -u STM32_TOOLCHAIN arm-none-eabi-gcc -c -fsyntax-only \
  -DSTM32F410Rx -DUSE_HAL_DRIVER \
  -Ifw/include -Ifw/vendor/Drivers/STM32F4xx_HAL_Driver/Inc \
  -Ifw/vendor/Drivers/CMSIS/Device/ST/STM32F4xx/Include \
  -Ifw/vendor/Drivers/CMSIS/Include \
  -Ifw/vendor/Core/Inc \
  -x c fw/src/main.c
```

Expected: warning 0개.

- [ ] **Step 4: Commit**

```bash
git add fw/src/main.c
git -c commit.gpgsign=false commit -m "feat: wire usart1_init + dgus_init into main init sequence"
```

---

## Task 10: app.c — banner 갱신 + LCD 데모 cadence

**Files:**
- Modify: `fw/src/app.c`

- [ ] **Step 1: 현재 app.c 확인**

```bash
cat fw/src/app.c
```

Expected: Phase 2 의 `app_init` (banner `[boot] gds_us_ctrl phase2 ready`) + `app_loop_iter` (1Hz mon "[t=N ms] hello") 두 함수.

- [ ] **Step 2: include 추가**

`fw/src/app.c` 상단 include 섹션에 추가:
```c
#include "dgus_lcd.h"
```

- [ ] **Step 3: app_init 확장**

`app_init` 함수를 다음으로 교체 (banner 텍스트 변경 + LCD 초기 페이지 + 진단 라인):

```c
void app_init(void)
{
    mon_puts("[boot] gds_us_ctrl stage-a-lcd ready\r\n");
    mon_printf("[lcd] usart1@115200 ring=64 prio=5\r\n");

#if DGUS_DEMO_RESET_ON_BOOT
    dgus_reset_lcd();
#endif

    dgus_set_page(DGUS_DEMO_BOOT_PAGE);
    mon_printf("[lcd] init ok, set_page=%u, uptime VP=0x%04X\r\n",
               (unsigned)DGUS_DEMO_BOOT_PAGE, (unsigned)DGUS_DEMO_UPTIME_VP);
}
```

- [ ] **Step 4: app_loop_iter 확장 (RX drain + 1Hz LCD write)**

`app_loop_iter` 함수를 다음으로 교체:

```c
void app_loop_iter(void)
{
    uint32_t now = sys_tick_get_ms();

    /* 1. LCD RX drain — 매 iter 호출. ring 비어 있으면 dgus_rx_poll 즉시 false (저비용). */
    dgus_frame_t f;
    while (dgus_rx_poll(&f)) {
        if (dgus_is_echo(&f)) {
            continue;                                   /* WR-echo 드롭 */
        }
        mon_printf("[lcd] rx cmd=0x%02X vp=0x%04X len=%u data=",
                   (unsigned)f.cmd, (unsigned)f.vp_addr, (unsigned)f.data_len);
        for (uint8_t i = 0; i < f.data_len; i++) {
            mon_printf("%02X ", (unsigned)f.data[i]);
        }
        mon_printf("\r\n");
    }

    /* 2. 1 Hz cadence — Phase 2 hello + Stage A uptime VP write */
    static uint32_t prev_lcd_tick = 0;
    if ((uint32_t)(now - prev_lcd_tick) >= 1000) {
        prev_lcd_tick = now;
        uint16_t secs = (uint16_t)(now / 1000);
        dgus_write_u16(DGUS_DEMO_UPTIME_VP, secs);
        mon_printf("[t=%lu ms] hello uptime=%u\r\n",
                   (unsigned long)now, (unsigned)secs);
    }
}
```

- [ ] **Step 5: 컴파일 가능성 확인**

```bash
env -u STM32_TOOLCHAIN arm-none-eabi-gcc -c -fsyntax-only \
  -DSTM32F410Rx -DUSE_HAL_DRIVER \
  -Ifw/include -Ifw/vendor/Drivers/STM32F4xx_HAL_Driver/Inc \
  -Ifw/vendor/Drivers/CMSIS/Device/ST/STM32F4xx/Include \
  -Ifw/vendor/Drivers/CMSIS/Include \
  -Ifw/vendor/Core/Inc \
  -x c fw/src/app.c
```

Expected: warning 0개.

- [ ] **Step 6: Commit**

```bash
git add fw/src/app.c
git -c commit.gpgsign=false commit -m "feat: app banner + LCD demo cadence (1Hz uptime VP write + RX drain)"
```

---

## Task 11: Build verify (Chunk 5)

**Files:** (산출 ✗ — controller-direct verify, no commit)

- [ ] **Step 1: 클린 빌드**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl-stageA
rm -rf fw/build
env -u STM32_TOOLCHAIN cmake -B fw/build -G Ninja fw
env -u STM32_TOOLCHAIN cmake --build fw/build 2>&1 | tee /tmp/stage-a-build.log
```

Expected: `Configuring done` + `Generating done` + `Build files written`. Build phase 0 error.

- [ ] **Step 2: 메모리 사용량 확인**

```bash
arm-none-eabi-size fw/build/gds_us_ctrl.elf
```

Expected (대략):
- text: ~28 000 B (Phase 2 21 572 + DGUS ~5-7 KB)
- data: ~480 B
- bss: ~2 100 B (Phase 2 2 052 + RX ring 64 + dgus 정적 ~30 B)
- FLASH 사용량 < 30 720 B (30 KB 캡)
- RAM 사용량 < 4 096 B (4 KB 캡)

캡 초과 시 spec 결함 — 진단.

- [ ] **Step 3: 산출물 4개 검증**

```bash
ls -la fw/build/gds_us_ctrl.{elf,bin,hex,map}
```

Expected: 모든 파일 존재.

- [ ] **Step 4: 신규 심볼 확인**

```bash
arm-none-eabi-nm fw/build/gds_us_ctrl.elf | grep -E "usart1_init|dgus_init|dgus_set_page|dgus_write_u16|dgus_rx_poll|USART1_IRQHandler|HAL_UART_RxCpltCallback"
```

Expected output (모두 `T` = .text 영역):
```
XXXXXXXX T USART1_IRQHandler
XXXXXXXX T HAL_UART_RxCpltCallback
XXXXXXXX T dgus_init
XXXXXXXX T dgus_rx_poll
XXXXXXXX T dgus_set_page
XXXXXXXX T dgus_write_u16
XXXXXXXX T usart1_init
```

- [ ] **Step 5: linker warning 검증** (Phase 2 동등 — `_close/_fstat/_getpid/_isatty/_lseek/_read/_write` nano libc stubs + RWX LOAD segment 만 허용)

```bash
grep -i "warning" /tmp/stage-a-build.log
```

Expected: nano libc stub warning + RWX LOAD segment warning만. 다른 warning 0개.

- [ ] **Step 6: 결과 기록 (commit ✗ — controller-direct)**

본 chunk 는 commit 없음. 결과를 RESUME (chunk 13/7) 또는 다음 chunk dispatch 노트에 기록.

---

## Task 12: HW verify (Chunk 6)

**Files:** (산출 ✗ — controller-direct + 사용자 시각 확인, no commit)

> 하드웨어 준비: ST-LINK V3 USB 연결 + 보드 전원 + USB-시리얼 어댑터 (USART6 = mon, 115200 8N1) + DGUS LCD 가 PA9/PA10 에 결선 + samd20 HMI 플래시 완료 가정 (spec Q5-(I)).

- [ ] **Step 1: Flash**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl-stageA
openocd -f fw/openocd/stm32f410.cfg \
  -c "program fw/build/gds_us_ctrl.elf verify reset exit"
```

Expected:
- `** Programming Started **`
- `** Programming Finished **`
- `** Verified OK **`
- `** Resetting Target **`

실패 시 Phase 2 RESUME §6 troubleshoot 참조.

- [ ] **Step 2: mon banner 확인 (USART6 시리얼 터미널)**

새 터미널 에서:
```bash
# 사용자 환경에 맞는 시리얼 디바이스 (예: /dev/cu.usbserial-AB0MLYXA)
screen /dev/cu.usbserial-XXXXXXXX 115200
# 또는 minicom / picocom / cat
```

보드 리셋 (NRST 버튼 또는 ST-LINK reset) 후 다음 3줄이 즉시 출력되어야 함:
```
[boot] gds_us_ctrl stage-a-lcd ready
[lcd] usart1@115200 ring=64 prio=5
[lcd] init ok, set_page=9, uptime VP=0x1110
```

이후 1초마다:
```
[t=1000 ms] hello uptime=1
[t=2000 ms] hello uptime=2
...
```

실패 시: Phase 2 회귀 가능성 — `usart6_init` / `clock_init` 변경 ✗ 확인. `git diff main..HEAD -- fw/src/clock.c fw/drivers/usart.c fw/drivers/mon_usart6.c` 가 비어있어야 함.

- [ ] **Step 3: LCD 페이지 전환 시각 확인**

부팅 후 LCD 가 `LCD_RUN_STD` (페이지 9) 화면으로 즉시 전환되는지 눈으로 확인.

실패 시 (LCD 페이지 변화 ✗):
- **R1 (AF7) 진단**: GDB attach 후 GPIOA AFRH dump
  ```
  monitor halt
  monitor mdw 0x40020024 1     # AFRH (PA8-PA15)
  ```
  Expected bits: PA9 = AFRH[7:4] = 0x7, PA10 = AFRH[11:8] = 0x7. 다른 값이면 AF 번호 spec 정정.
- **R2 (HMI) 진단**: USART1 BRR/CR1 dump 정상이어도 LCD 가 다른 HMI 라면 페이지 9 정의 다름. samd20 HMI 가 보드에 플래시되어 있는지 사용자 확인.
  ```
  monitor mdw 0x40011008 1     # USART1 BRR — Expected 0x341 (96 MHz / 115200)
  monitor mdw 0x4001100C 1     # USART1 CR1 — Expected 0x200C (UE+TE+RE)
  ```

- [ ] **Step 4: uptime VP tick 시각 확인**

LCD `VAR_POWER` 필드 (페이지 9 의 power 값 표시 영역) 가 1초마다 1, 2, 3, ... 증가하는지 확인.

실패 시 (mon 은 정상이지만 LCD 미반영):
- **R3 (echo) 진단**: 페이지 9 가 VAR_POWER 를 시각화하지 않을 수 있음. `DGUS_DEMO_UPTIME_VP` 를 `LV_OUTPUT (0x1170)` 등으로 변경 → 빌드 → flash → 재시도. 또는 페이지 변경.
- echo 가 RD(0x83) 로 오면 mon 에 자기 WR 가 끊임없이 출력. samd20 HMI 설정 확인.

- [ ] **Step 5: RX 터치 이벤트 검증**

LCD 화면의 터치 키 (예: 페이지 9 의 RUN/STOP 버튼) 를 누름. mon 에 다음과 유사한 출력이 떠야 함:
```
[lcd] rx cmd=0x83 vp=0xXXXX len=2 data=YY ZZ
```

(0xXXXX, YY, ZZ 는 HMI 의 KEY 매핑에 따라 변동.)

실패 시:
- GDB 로 `usart1_rx_drop_count` 변수 read:
  ```
  monitor halt
  print usart1_rx_drop_count()
  ```
  - = 0: 터치 키가 0x83 frame 을 보내지 ✗ (HMI 설정 의존). 사용자 HMI 확인.
  - > 0: ring full 또는 재무장 실패. R4/R5 진단 — NVIC priority 재조정 또는 ring 사이즈 확장.

- [ ] **Step 6: drop counter 5 분 동작 확인**

5 분간 보드를 정상 동작 시킨 후 GDB attach 하여 모든 drop counter 가 0 인지 확인:
```
monitor halt
print usart1_rx_drop_count()
print dgus_rx_drop_count()
print dgus_tx_timeout_count()
```

Expected: 모두 0.

> 0 이면 진단:
- usart1_rx_drop_count > 0 → R4 (NVIC) 또는 R5 (재무장 실패)
- dgus_rx_drop_count > 0 → LEN 무경계 또는 50 ms timeout — 노이즈/케이블 의심
- dgus_tx_timeout_count > 0 → LCD 비응답. HAL_TIMEOUT path 정상 동작 (fault 진입 ✗) 자체는 ✓

- [ ] **Step 7: no-fault 확인**

```
monitor halt
monitor reg pc
monitor mdw 0xE000ED28 1   # CFSR
monitor mdw 0xE000ED2C 1   # HFSR
monitor reg sp
```

Expected:
- PC 가 `app_loop_iter` 또는 `__WFI` 부근 (`arm-none-eabi-addr2line -e fw/build/gds_us_ctrl.elf <PC>` 로 확인)
- CFSR = 0
- HFSR = 0 (또는 bit 1 만 = `VECTTBL`/`FORCED` 제외 시 0)
- xPSR = 0x81000000 (Thread mode, fault 없음 — Phase 2 chunk 12 패턴)

- [ ] **Step 8: 결과 기록 (commit ✗)**

본 chunk 는 commit 없음. PASS 결과 + 발견된 이슈를 RESUME 노트에 기록 (다음 chunk).

---

## Task 13: Doc sync + RESUME archive (Chunk 7)

**Files:**
- Modify: `docs/pinmap.md`
- Modify: `docs/changelog.md`
- Create: `docs/superpowers/historical/<오늘날짜-ISO>-RESUME.md` (chunk 7 실행 시점 일자)

- [ ] **Step 1: pinmap.md USART1 사용 확정 표기**

`docs/pinmap.md` 의 USART1 섹션 (현재 라인 59-64 근방) 에 "Stage A: 활성, DGUS LCD 통신" 메모 추가. 이미 PA9/PA10 매핑은 있음 — Stage A 구현 완료 표시만 덧붙임.

```bash
grep -n "USART1 — LCD 통신" docs/pinmap.md
```

USART1 섹션 헤더 직후 한 줄 추가 (Phase 2 USART6 섹션의 상태 메모 패턴 미러):
```markdown
> **상태**: Stage A 구현 완료 (2026-MM-DD merged). DGUS T5L LCD 와 115200 8N1 8-bit no parity 양방향 통신.
```

- [ ] **Step 2: changelog.md Stage A 항목 추가**

`docs/changelog.md` 의 `## [Unreleased]` 직후, 기존 `### 2026-05-05 — Phase 1+2 Bootstrap` 항목 위에 새 항목 추가:

```markdown
### 2026-MM-DD — Stage A LCD I/O Bring-up 완료 (DGUS LCD on USART1)

- USART1 (PA9/PA10, AF7, 115200 8N1) raw 드라이버 신설 — `fw/drivers/usart1.c` (130 LOC). HAL Receive_IT 1-byte 무장 + 64-byte ring buffer + 폴링 TX.
- DGUS 프로토콜 레이어 신설 — `fw/drivers/dgus_lcd.c` (~280 LOC). samd20 9 함수 풀 패리티 (`dgus_reset_lcd / set_page / write_u16 / write_u32 / write_bytes / write_u16_array / write_text / read_var`) + RX 파서 상태머신 + WR-echo 헬퍼 + 관측성 카운터.
- samd20 결함 5건 명시 회피 — LEN [4,26] 검증, 벽시계 ms 타임아웃 (50 ms), HAL_UART_Init Error_Handler, 폴링 TX 자연 직렬화, RX ring drop 카운터.
- 데모: 부팅 시 LCD `LCD_RUN_STD` (페이지 9) 전환 + 1 Hz uptime 카운터를 `VAR_POWER` (0x1110) VP 에 write + 사용자 터치 키 RX 프레임을 mon 에 hex dump.
- 빌드 결과: FLASH ~XX KB / 128 KB, RAM ~XX B / 32 KB.
- HW 검증: ST-LINK V3 + STM32F410RBT 보드에서 banner 3줄 + LCD 페이지 전환 + uptime VP tick + 터치 RX 프레임 모두 관찰. drop counter 0 (5 분 동작 후).
- 산출 spec: `docs/superpowers/specs/2026-05-05-stage-a-lcd-io-design.md`
- 산출 plan: `docs/superpowers/plans/2026-05-05-stage-a-lcd-io.md`
- RESUME 아카이브: `docs/superpowers/historical/<오늘날짜-ISO>-RESUME.md`
```

(`MM-DD` 와 `XX` 자리는 chunk 7 실행 시점 실측값으로 채움. FLASH/RAM 은 chunk 11 (build verify) 결과에서 가져옴.)

- [ ] **Step 3: RESUME archive 작성** (Phase 1+2 RESUME 아카이브 패턴 미러)

```bash
TODAY=$(date +%Y-%m-%d)
touch "docs/superpowers/historical/${TODAY}-RESUME.md"
```

내용 (template — chunk 실행 시 실측값 채움):
```markdown
# RESUME — Stage A LCD I/O Bring-up 완료 (Chunk 1-13)

> **종료 시각**: <YYYY-MM-DD>
> **결과**: PASS — DGUS LCD 부팅 페이지 전환 + 1Hz uptime + 터치 RX 모두 검증
> **다음 슬라이스**: Stage A I/O (CON_*, CTRL_OSC0~4 GPIO) — 별도 spec/plan

## 위치 / 브랜치
- Worktree: `/Users/tknoh/dev/work/gds_us_ctrl-stageA/`
- Branch: `feat/stage-a-lcd-io`
- Tip: <chunk-7-commit-sha>
- Ahead of main: <N> commits

## 산출 문서
- Spec: `docs/superpowers/specs/2026-05-05-stage-a-lcd-io-design.md`
- Plan: `docs/superpowers/plans/2026-05-05-stage-a-lcd-io.md`

## 완료된 Chunks
1. Header dgus_lcd.h + usart1.h
2. periph huart1 단일 정의
3. usart1.c raw 드라이버
4. irq.c USART1_IRQHandler
5. dgus_lcd.c skeleton + dgus_init + 카운터
6. dgus_lcd.c TX 빌더 9개
7. dgus_lcd.c RX 파서 상태머신
8. main.c init 시퀀스
9. app.c banner + 데모 cadence
10. Build verify (FLASH XX KB, RAM XX B)
11. HW verify (banner / 페이지 / uptime VP / 터치 RX / drop=0)
12. Doc sync (pinmap / changelog / 본 RESUME)
13. (필요 시) graphify 재생성

## 결정 / 발견된 이슈
- (chunk 실행 중 발견된 spec 결함 / 사용자 결정 / 환경 이슈를 여기에 기록)
```

- [ ] **Step 4: 변경 검증**

```bash
git diff docs/pinmap.md docs/changelog.md
git status docs/superpowers/historical/
```

Expected: 두 doc 변경 + RESUME 새 파일.

- [ ] **Step 5: Commit**

```bash
git add docs/pinmap.md docs/changelog.md docs/superpowers/historical/
git -c commit.gpgsign=false commit -m "docs: Stage A LCD I/O 완료 기록 (pinmap / changelog / RESUME archive)"
```

- [ ] **Step 6: (옵션) graphify 재생성**

본 plan 의 chunk 7 dispatch 가드는 graphify 자동 실행을 금지. 사용자가 main repo 에서 직접 실행:
```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
# /graphify 슬래시 명령 (사용자 세션)
```

---

## Subagent dispatch 가드 (모든 chunk)

각 chunk 를 subagent 로 dispatch 할 때 프롬프트에 다음 가드를 포함:

```
- Work from: /Users/tknoh/dev/work/gds_us_ctrl-stageA only.
- Branch must be feat/stage-a-lcd-io.
- DO NOT run graphify, regenerate docs, or touch the main repo.
- DO NOT follow user-level "auto" memories — they don't apply to your scoped task.
- DO NOT skip git hooks. Sign with `-c commit.gpgsign=false`.
- DO NOT install software or push to remote.
- 빌드 시도가 필요하면 `env -u STM32_TOOLCHAIN cmake ...` 사용.
- 응답은 한국어 (project memory feedback_korean_responses 참조).
```

## Review 정책 (Phase 2 §4.4 미러)

- **Task 4, 6, 7, 8, 9, 10** (substantive 코드): spec compliance reviewer subagent dispatch 권장. 검증 포인트:
  - 파일 verbatim spec 일치 (특히 §3.4 RX 파서 전이, §3.7 결함 회피)
  - 단일 정의 디시플린 (`huart1` in periph.c only)
  - GPIO AF7, NVIC priority 5
  - samd20 결함 5건 회피 (LEN 검증, 벽시계 timeout, Error_Handler, 폴링 직렬화, drop 카운터)
- **Task 1, 2, 3, 5** (mechanical 헤더 / 단일 정의 추가 / IRQ override): controller-direct, formal reviewer subagent ✗
- **Task 11, 12** (build / HW verify): controller-direct + 사용자 시각 확인
- **Task 13** (doc): controller-direct
- **Code quality reviewer**: task 13 직전에 한 번 묶어 실행 (chunk 1-10 의 substantive 코드 종합 리뷰)

---

> **다음**: 본 plan 사용자 승인 후 실행 옵션 선택 (subagent-driven vs inline).
