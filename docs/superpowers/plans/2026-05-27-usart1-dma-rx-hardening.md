# USART1 DGUS RX 견고화 (DMA circular) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** USART1 DGUS RX를 1바이트 인터럽트에서 DMA circular로 전환해 지속 패널 플러드에서도 wedge되지 않게 하고, 트리거였던 per-rx 블로킹 트레이스/무한 mon TX 타임아웃을 제거한다.

**Architecture:** DMA2 Stream2 Ch4를 USART1_RX에 circular·free-running으로 붙이고(`HAL_DMA_Start` + `CR3 DMAR`, UART/DMA RX 인터럽트 미사용), `usart1_rx_pop`을 NDTR 기반 인덱스로 재작성한다. 시그니처 불변이라 `dgus_lcd.c` 파서·`app.c` 드레인 루프는 무수정. per-rx 트레이스는 `#ifdef LCD_TRACE_RX` 게이트, mon TX는 50ms 타임아웃.

**Tech Stack:** STM32F410RBT, ST HAL/CMSIS(in-tree `fw/vendor/`), arm-none-eabi-gcc, CMake+Ninja, 베어메탈 C(슈퍼루프, RTOS 없음). Host 유닛테스트 하네스 없음 — 태스크 게이트 = 빌드 0-warning(`-Wall -Wextra -Wundef -Wshadow`), 행동 검증 = HW bench repro(Task 6).

**Spec:** `docs/superpowers/specs/2026-05-27-usart1-dma-rx-hardening-design.md`
**Finding:** `docs/superpowers/analysis/2026-05-27-lcd-hw-verify-rx-wedge.md`

**빌드 명령(모든 태스크 공통):**
```bash
cd /Users/tknoh/dev/work/gds_us_ctrl/fw && env -u STM32_TOOLCHAIN cmake --build build
```
기대: 컴파일 성공, **경고 0줄**, 링크 성공.

---

## File Structure

| 파일 | 책임 | 변경 |
|------|------|------|
| `fw/include/periph.h` | HAL 핸들 extern 선언 | `hdma_usart1_rx` extern 추가 |
| `fw/src/periph.c` | HAL 핸들 단일 정의 | `hdma_usart1_rx` 정의 추가 |
| `fw/drivers/usart1.c` | USART1 raw I/O (DGUS) | IT→DMA circular 재작성, pop NDTR, 카운터 0 |
| `fw/src/irq.c` | 벡터 핸들러 | `USART1_IRQHandler` 제거 |
| `fw/src/app_lcd_input.c` | VP 디스패치 | L619 트레이스 `#ifdef LCD_TRACE_RX` |
| `fw/drivers/mon_usart6.c` | mon TX | `HAL_MAX_DELAY`→50ms |

무수정: `fw/drivers/dgus_lcd.c`(파서), `fw/src/app.c`(드레인 루프).

---

## Task 0: 소비처 확인 (recon)

**Files:** 없음(읽기 전용 확인).

- [ ] **Step 1: 카운터 함수 소비처 grep**

Run:
```bash
cd /Users/tknoh/dev/work/gds_us_ctrl && grep -rn 'usart1_rx_drop_count\|usart1_rx_error_count' fw/src fw/drivers fw/include | grep -v 'usart1.c\|usart1.h'
```
기대: 호출처가 없거나(현재 미사용) 관측 print 뿐임을 확인. 호출처가 있으면 `return 0` 시그니처 호환(반환형 `uint16_t` 유지)이라 무영향임을 확인. **시그니처를 바꿔야 할 소비처가 있으면 중단하고 보고.**

- [ ] **Step 2: `usart1.h` 공개 API 확인**

Run: `sed -n '1,40p' fw/include/usart1.h`
기대: `usart1_init` / `usart1_send_blocking` / `usart1_rx_pop` / `usart1_rx_drop_count` / `usart1_rx_error_count` 선언 확인. 본 plan은 이 시그니처를 모두 유지.

- [ ] **Step 3: DMA 매핑 1차 확인 (vendor 헤더)**

Run: `grep -rn 'DMA2_Stream2\|DMA_CHANNEL_4' fw/vendor/Drivers/CMSIS/Device/ST/STM32F4xx/Include/*.h | head`
기대: `DMA2_Stream2` 정의 존재 확인. (request 매핑 USART1_RX=Stream2/Ch4는 RM0401 기준 — 헤더엔 없음. Task 6 HW에서 최종 입증. 만약 RX 무동작이면 spec F1대로 Stream5로 대안.)

커밋 없음(확인만).

---

## Task 1: `hdma_usart1_rx` 핸들 추가

**Files:**
- Modify: `fw/include/periph.h`
- Modify: `fw/src/periph.c`

- [ ] **Step 1: periph.h에 extern 추가**

`fw/include/periph.h`의 `extern UART_HandleTypeDef huart1;` 줄 아래에 추가:
```c
extern DMA_HandleTypeDef hdma_usart1_rx;   /* USART1 RX — DMA2 S2 Ch4 circular (RX hardening) */
```

- [ ] **Step 2: periph.c에 정의 추가**

`fw/src/periph.c`의 `UART_HandleTypeDef huart1;` 정의 줄 아래에 추가:
```c
DMA_HandleTypeDef hdma_usart1_rx;   /* USART1 RX — DMA2 S2 Ch4 circular (RX hardening) */
```

- [ ] **Step 3: 빌드 확인**

Run: 공통 빌드 명령.
기대: 0-warning(아직 `hdma_usart1_rx` 미사용이나 전역 정의는 unused 경고 없음). 성공.

- [ ] **Step 4: 커밋**

```bash
git add fw/include/periph.h fw/src/periph.c
git commit -m "feat(usart1): add hdma_usart1_rx handle (DMA RX hardening)"
```

---

## Task 2: `usart1.c` — DMA circular RX 재작성 (핵심)

**Files:**
- Modify (전체 교체): `fw/drivers/usart1.c`

- [ ] **Step 1: `usart1.c`를 아래 내용으로 교체**

```c
/* fw/drivers/usart1.c
 *
 * USART1 raw I/O — DGUS LCD (PA9/PA10, AF7, 115200 8N1).
 * RX = DMA2 Stream2 Ch4, circular, free-running (HAL UART/DMA RX 인터럽트 미사용).
 *   → 오버런 면역: DMA가 RXNE마다 DR을 소비, 정지(wedge) 불가. 극한 플러드 시
 *     최악 = 가장 오래된 미읽음 바이트 덮어씀 → 파서가 다음 5A A5에 재동기.
 * TX = 폴링(usart1_send_blocking, 10ms timeout) — 변경 없음.
 *
 * 호출자: fw/drivers/dgus_lcd.c 만 (단일 정의 디시플린).
 */
#include "stm32f4xx_hal.h"
#include "periph.h"
#include "clock.h"   /* Error_Handler */
#include "usart1.h"

#define RX_DMA_SIZE  256u                  /* 2의 거듭제곱 → mask 분기 없음 */

static uint8_t  s_rx_dma_buf[RX_DMA_SIZE]; /* DMA circular 목적지 (.bss SRAM, DMA2 접근 가능) */
static uint16_t s_rx_tail;                 /* loop reader 인덱스 (단독 소유) */

void usart1_init(void)
{
    /* 1. 클럭 */
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_DMA2_CLK_ENABLE();

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

    /* 4. DMA2 Stream2 Ch4 = USART1_RX (RM0401). circular P->M byte, free-running. */
    hdma_usart1_rx.Instance                 = DMA2_Stream2;
    hdma_usart1_rx.Init.Channel             = DMA_CHANNEL_4;
    hdma_usart1_rx.Init.Direction           = DMA_PERIPH_TO_MEMORY;
    hdma_usart1_rx.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_usart1_rx.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_usart1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart1_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    hdma_usart1_rx.Init.Mode                = DMA_CIRCULAR;
    hdma_usart1_rx.Init.Priority            = DMA_PRIORITY_HIGH;
    hdma_usart1_rx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_usart1_rx) != HAL_OK) {
        Error_Handler();
    }
    __HAL_LINKDMA(&huart1, hdmarx, hdma_usart1_rx);

    /* 5. reader 인덱스 초기화 */
    s_rx_tail = 0;

    /* 6. free-running circular RX 시작 — _Start (인터럽트 없음) + USART CR3 DMAR.
     *    UART/DMA RX 인터럽트·NVIC 일절 미사용 → ORE에 반응해 RX를 멈추는 HAL 경로 부재. */
    if (HAL_DMA_Start(&hdma_usart1_rx, (uint32_t)&huart1.Instance->DR,
                      (uint32_t)s_rx_dma_buf, RX_DMA_SIZE) != HAL_OK) {
        Error_Handler();
    }
    SET_BIT(USART1->CR3, USART_CR3_DMAR);
}

HAL_StatusTypeDef usart1_send_blocking(const uint8_t *buf, uint16_t len)
{
    /* 폴링 TX. 10 ms timeout — 케이블 빠짐/멈춤 시 HAL_TIMEOUT 반환, fault 진입 ✗. */
    return HAL_UART_Transmit(&huart1, (uint8_t *)buf, len, /*timeout_ms=*/10);
}

bool usart1_rx_pop(uint8_t *out_byte)
{
    /* DMA write 위치 = SIZE - NDTR. tail 단독 소유라 락 불필요. */
    uint16_t head = (uint16_t)(RX_DMA_SIZE - __HAL_DMA_GET_COUNTER(&hdma_usart1_rx));
    if (s_rx_tail == head) {
        return false;
    }
    *out_byte = s_rx_dma_buf[s_rx_tail];
    s_rx_tail = (uint16_t)((s_rx_tail + 1u) & (RX_DMA_SIZE - 1u));
    return true;
}

/* DMA circular RX 에는 ring-full / 재무장 실패 / IT-ORE 개념이 없음.
 * API/심볼 안정성 위해 유지하되 0 반환. RX 건강 신호는 dgus_rx_drop_count (파서 레벨). */
uint16_t usart1_rx_drop_count(void)  { return 0; }
uint16_t usart1_rx_error_count(void) { return 0; }
```

- [ ] **Step 2: 빌드 확인**

Run: 공통 빌드 명령.
기대: 0-warning. 특히 (a) 제거된 IT static(`s_rx_byte`/`s_rx_ring`/`s_rx_head`/`s_rx_drop_count`/`s_rx_error_count`) unused 경고 없음(전부 삭제됨), (b) `HAL_UART_RxCpltCallback`/`HAL_UART_ErrorCallback` 미정의로 인한 링크 에러 없음(vendor weak stub 사용), (c) `DMA2_Stream2`/`DMA_CHANNEL_4`/`USART_CR3_DMAR` 심볼 해결.

> 만약 `s_rx_dma_buf` unused 경고가 나면 init/pop이 참조하므로 발생 불가 — 그래도 나면 빌드 캐시 stale, `cmake --build build --target clean` 후 재빌드.

- [ ] **Step 3: 커밋**

```bash
git add fw/drivers/usart1.c
git commit -m "feat(usart1): DMA circular RX (DMA2 S2 Ch4) — overrun-immune, no IT rearm

Replaces 1-byte HAL_UART_Receive_IT (which could permanently wedge on
overrun/rearm-fail, RXNEIE=0) with free-running circular DMA. usart1_rx_pop
reads via NDTR; signature unchanged so dgus_lcd parser/app untouched.
Retires ring-full/rearm counters (return 0); dgus_rx_drop_count is the
health signal. Removes USART1 RX IT callbacks + s_rx_byte/ring/head."
```

---

## Task 3: `irq.c` — `USART1_IRQHandler` 제거

**Files:**
- Modify: `fw/src/irq.c:29-36`

- [ ] **Step 1: USART1_IRQHandler 블록 삭제**

`fw/src/irq.c`에서 아래 전체(주석 포함, 현재 29~36줄)를 삭제:
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
삭제 후 파일은 `Error_Handler` / TIM11 핸들러 / `HAL_TIM_PeriodElapsedCallback`까지만 남음. (DMA circular는 인터럽트 미사용 → DMA2_Stream2 핸들러 추가 불필요.)

- [ ] **Step 2: 빌드 확인**

Run: 공통 빌드 명령.
기대: 0-warning. `USART1_IRQHandler` 제거 시 vendor startup 의 weak alias(`Default_Handler`)로 폴백 — NVIC USART1_IRQn 비활성(Task 2에서 미설정)이라 절대 진입 안 함. 링크 성공.

- [ ] **Step 3: 커밋**

```bash
git add fw/src/irq.c
git commit -m "refactor(irq): drop USART1_IRQHandler — RX now DMA, no UART IT"
```

---

## Task 4: `app_lcd_input.c` — per-rx 트레이스 게이트

**Files:**
- Modify: `fw/src/app_lcd_input.c:619`

- [ ] **Step 1: L619 트레이스를 컴파일 플래그로 게이트**

`fw/src/app_lcd_input.c`에서:
```c
    mon_printf("[lcd] rx vp=0x%04X data=%u\r\n", (unsigned)vp, (unsigned)data16);
```
를 아래로 교체:
```c
#ifdef LCD_TRACE_RX
    mon_printf("[lcd] rx vp=0x%04X data=%u\r\n", (unsigned)vp, (unsigned)data16);
#endif
```
default(미정의)=off → 플러드 증폭 제거. 진단 시 `-DLCD_TRACE_RX`로 빌드(기존 `LCD_DEMO_MEASURE_RAMP` 관례와 일치).

- [ ] **Step 2: 빌드 확인**

Run: 공통 빌드 명령.
기대: 0-warning. `vp`/`data16`은 이후 `switch(vp)`/핸들러에서 사용되므로 게이트 off여도 unused 경고 없음. 성공.

- [ ] **Step 3: 커밋**

```bash
git add fw/src/app_lcd_input.c
git commit -m "fix(lcd): gate per-rx trace behind LCD_TRACE_RX (default off)

Per-rx blocking mon_printf saturated the superloop under VP-write flood
(energy slider), starving RX drain. Off by default; build -DLCD_TRACE_RX
for diagnostics."
```

---

## Task 5: `mon_usart6.c` — mon TX 타임아웃 한정

**Files:**
- Modify: `fw/drivers/mon_usart6.c:11-13`

- [ ] **Step 1: MON_TX_TIMEOUT_MS 정의 + 적용**

`fw/drivers/mon_usart6.c`의 `#include "mon.h"` 줄 아래에 추가:
```c
#define MON_TX_TIMEOUT_MS  50u   /* 진단 채널이 앱을 영구 블록하지 못하게 */
```
그리고 `mon_write`의:
```c
    HAL_UART_Transmit(&huart6, (uint8_t *)buf, len, HAL_MAX_DELAY);
```
를:
```c
    HAL_UART_Transmit(&huart6, (uint8_t *)buf, len, MON_TX_TIMEOUT_MS);
```

- [ ] **Step 2: 빌드 확인**

Run: 공통 빌드 명령.
기대: 0-warning. 성공.

- [ ] **Step 3: 커밋**

```bash
git add fw/drivers/mon_usart6.c
git commit -m "fix(mon): bound USART6 TX timeout to 50ms (was HAL_MAX_DELAY)

A diagnostic log channel must never be able to hang the app forever."
```

---

## Task 6: HW bench 검증 (행동 acceptance)

**Files:** 없음(코드 변경 시 해당 태스크로 복귀). USART6 mon @115200 = `/dev/cu.usbserial-BG02DMWU`. openocd = `fw/openocd/stm32f410.cfg`.

- [ ] **Step 1: 플래시**

Run: `cd /Users/tknoh/dev/work/gds_us_ctrl/fw && cmake --build build --target flash`
기대: `Programming Finished` / `Verified OK` / `Resetting Target`.

- [ ] **Step 2: 부팅 무회귀 확인 (mon)**

USART6 mon 단일-fd 캡처(백그라운드):
```bash
{ stty -f /dev/cu.usbserial-BG02DMWU 115200 cs8 -parenb -cstopb raw -echo; cat; } < /dev/cu.usbserial-BG02DMWU > /tmp/lcd-mon.log 2>/tmp/lcd-mon.err &
```
기대 로그: `[boot] … stage-b ready`, `[lcd] ready=1`, `[lcd] run_page_confirmed=1`, `[cfg] freq=0 type=0 work=0 energy=100000 …`. (Stage B 패리티 무회귀.)

- [ ] **Step 3: wedge repro 역검증 (핵심)**

사용자에게: SETUP 진입 → **energy 슬라이더를 공격적으로 반복 드래그**(이전 wedge 트리거). 드래그 중·후 openocd로 RX 상태 읽기:
```bash
cd /Users/tknoh/dev/work/gds_us_ctrl/fw && openocd -f openocd/stm32f410.cfg -c "init; halt; echo \"NDTR=[read_memory 0x40026444 32 1]\"; echo \"USART1_SR=[read_memory 0x40011000 32 1] CR3=[read_memory 0x40011014 32 1]\"; echo \"DGUS_DROP=[read_memory 0x2000033c 16 1]\"; resume; shutdown" 2>&1 | grep -E 'NDTR=|USART1_SR=|DGUS_DROP='
```
> DMA2_Stream2 NDTR 레지스터 = DMA2 base 0x40026400 + S2 base(0x10 + 0x18×2 = 0x40) + NDTR offset 0x04 = **0x40026444**. CR3 = USART1 0x40011000 + 0x14 = 0x40011014. 두 주소는 빌드 불변(레지스터). 단 `s_dgus_rx_drop_count`(0x2000033c)·`s_rx_tail`은 빌드마다 변하니 `arm-none-eabi-nm build/gds_us_ctrl.elf | grep -E 's_dgus_rx_drop_count|s_rx_tail'`로 재확인. NDTR 읽기가 번거로우면 `s_rx_tail` 전진만 봐도 RX 생존 입증.

기대:
- ① LCD 표시 계속 갱신(멈춤 없음) — 사용자 육안.
- ② NDTR 변동 + (nm으로 읽은) `s_rx_tail` 전진 = RX 흐름 생존.
- ③ `USART1_SR` ORE 떠도 무해(DMA 미정지), `CR3` DMAR(bit6)=1 유지.
- ④ `DGUS_DROP`(파서) ≈ 0 (이전엔 15524) — 체크리스트 ⑨ PASS.

- [ ] **Step 4: SAVE/CANCEL 동작 확인**

사용자에게: 드래그 직후 SETUP에서 값 편집 → **SAVE** → 전원사이클(또는 `reset run`) → 값 영속 / **CANCEL** → FRAM 값 복귀. mon에 `[lcd-hook]` 로그(필요 시 `-DLCD_TRACE_RX` 재빌드로 rx 트레이스).
기대: SAVE/CANCEL이 즉시 반응(wedge 해소 입증).

- [ ] **Step 5: T10 체크리스트 재개**

`docs/changelog.md` 2026-05-27 항목 ②~⑦, ⑩ 수행(⑧ N/A=전원공유). 트레이스 필요 항목은 `-DLCD_TRACE_RX` 빌드.

- [ ] **Step 6: 통과 시 문서/머지**

통과 → `docs/changelog.md`(RX 하드닝 + T10 done 항목 추가) / `docs/superpowers/RESUME.md` / `docs/NEXT_STEPS.md` "done" 갱신 → `gh pr create`(→main) → 태그 `hw-revA_fw-stage-lcd`. 실패 → spec §7 F1~F5(특히 F1 DMA 매핑 Stream5 대안)부터 점검.

---

## 검증 한계 (베어메탈)

Host 유닛테스트 하네스 없음. Task 1~5는 빌드 0-warning이 게이트, Task 6 HW repro가 행동 acceptance. 자동화 회귀 테스트는 향후 QEMU/HIL 도입 시 추가(현 범위 밖).
