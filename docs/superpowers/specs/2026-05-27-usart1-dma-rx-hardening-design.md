# Spec — USART1 DGUS RX 견고화 (DMA circular) + 트레이스/타임아웃

> 날짜: 2026-05-27 · 브랜치: `feat/stage-lcd-full-behavior` · 상태: 설계 승인됨, 구현 대기
> 동기 finding: [`docs/superpowers/analysis/2026-05-27-lcd-hw-verify-rx-wedge.md`](../analysis/2026-05-27-lcd-hw-verify-rx-wedge.md)

## 1. 문제 (Problem)

T10 HW 검증 중, SETUP 페이지에서 **energy 슬라이더 드래그**(DGUS `LV_ENERGY_EDIT` 0x1202가 연속 VP 플러드)가 USART1 DGUS RX를 **영구 wedge**시킴. 결과: SAVE/CANCEL 포함 모든 터치 미수신(화면은 패널 자체 페이지 점프만 동작).

증거(openocd, live, 1초 간격 2회 동일): `USART1 CR1=0x200C`(RXNEIE=0), `SR=0xF8`(ORE=1, RXNE=1), head=tail, ERRCNT=0, dgus drop=15524, usart1 drop=28.

근본 원인 2계층:
1. **트리거**: per-rx 디버그 `mon_printf`(`app_lcd_input.c:619`)가 무한-타임아웃 블로킹 TX(`mon_usart6.c:12`, `HAL_MAX_DELAY`)를 매 RD마다 호출 → 플러드 중 슈퍼루프 포화 → 64B RX 링 드레이닝 굶음 → 오버플로.
2. **wedge(아키텍처)**: 1바이트 `HAL_UART_Receive_IT` RX가 오버런/재무장 실패 시 RXNEIE=0으로 영구 정지. 루프를 ~5.5ms(64B@115200) 막는 어떤 이벤트든 RX를 죽일 수 있음.

RX 드라이버는 Phase2/StageA 산출물(286d908, d8d413c). LCD 브랜치가 처음으로 대량 패널→MCU RD 경로를 추가해 기존 버그를 노출. **LCD 포팅 로직 자체는 정상.**

## 2. 목표 / 비목표

**목표**
- DGUS RX가 지속적 패널 플러드(슬라이더/연타)에서 **절대 wedge되지 않음**. 정지 대신 우아한 열화(극한 시 오래된 미읽음 바이트 손실 허용).
- `usart1_rx_pop` / `dgus_rx_poll` 인터페이스 불변 → **`dgus_lcd.c` 파서·`app.c` 무수정**.
- 진단 계측이 앱을 영구 블록하지 못함.
- 빌드 0-warning(`-Wall -Wextra -Wundef -Wshadow`). merge-ready.

**비목표**
- 측정/제어(Stage D) 결합, LCD 렌더/입력 로직 변경.
- 무손실 보장(극한 플러드 시 손실 허용 — wedge 회피가 우선).
- USART1 TX 경로 변경(폴링 `usart1_send_blocking` 10ms 유지).
- USART6(mon) RX(미사용) 변경.

## 3. 접근 결정

선택 = **A. DMA circular RX**. (대안 B 폴링 = 부하 시 손실 잔존·RX 지연 루프 종속, C self-heal IT = 구조적 취약성 미해결 밴드에이드 → 기각.)

근거: DMA가 RXNE마다 DR을 수 사이클 내 비움 → ORE 사실상 미발생, 발생해도 DMA 미정지. per-byte ISR/재무장/RXNEIE 개념 자체 소멸 → wedge 버그 클래스 제거. 인터페이스 동일. DMA2 현재 전부 미사용.

## 4. 설계

### 4.1 DMA circular RX — `fw/drivers/usart1.c` 재아키텍처

**핸들/버퍼** (핸들은 CLAUDE.md 관례대로 `periph.c` 단일 정의, `periph.h` extern):
- `periph.c`: `DMA_HandleTypeDef hdma_usart1_rx;`
- `usart1.c` (static): `static uint8_t s_rx_dma_buf[RX_DMA_SIZE];`  `#define RX_DMA_SIZE 256` (2의 거듭제곱 → mask). `static uint16_t s_rx_tail;`
- 배치: 일반 `.bss`(F410 단일 32KB SRAM, DMA2 접근 가능, 데이터캐시 없음 → 코히런시 무관).

**`usart1_init` 변경**:
1. 기존 USART1 클럭/GPIO(PA9/PA10 AF7)/`HAL_UART_Init` 유지.
2. **제거**: `USART1_IRQn` NVIC 설정(`usart1.c:52-53`), 첫 `HAL_UART_Receive_IT`(`:62-64`).
3. **추가**:
   - `__HAL_RCC_DMA2_CLK_ENABLE()`.
   - `hdma_usart1_rx`: `Instance=DMA2_Stream2`, `Init.Channel=DMA_CHANNEL_4`, `Direction=DMA_PERIPH_TO_MEMORY`, `PeriphInc=DISABLE`, `MemInc=ENABLE`, `PeriphDataAlignment=BYTE`, `MemDataAlignment=BYTE`, `Mode=DMA_CIRCULAR`, `Priority=HIGH`, `FIFOMode=DISABLE`(direct). → `HAL_DMA_Init`. 실패 시 `Error_Handler`.
   - `__HAL_LINKDMA(&huart1, hdmarx, hdma_usart1_rx)`.
   - **free-running 시작 (HAL UART RX 상태머신 우회)**: `HAL_DMA_Start(&hdma_usart1_rx, (uint32_t)&huart1.Instance->DR, (uint32_t)s_rx_dma_buf, RX_DMA_SIZE)` (인터럽트 없는 `_Start` = DMA IRQ 불필요) → `SET_BIT(USART1->CR3, USART_CR3_DMAR)`.
   - `s_rx_tail=0`. 카운터 0.
   - **UART RX 인터럽트/NVIC 일절 미사용** → ORE에 반응해 RX를 멈추는 HAL 경로 부재.

> ⚠ 구현 시 **RM0401 §DMA2 request mapping**으로 `USART1_RX = DMA2_Stream2, Channel 4` 재확인(대안 Stream5). vendor CMSIS는 request 매핑을 인코딩하지 않음(사용자 책임).

**`usart1_rx_pop(uint8_t *out)`** — 시그니처 불변:
```c
uint16_t head = (uint16_t)(RX_DMA_SIZE - __HAL_DMA_GET_COUNTER(&hdma_usart1_rx));
if (s_rx_tail == head) return false;
*out = s_rx_dma_buf[s_rx_tail];
s_rx_tail = (uint16_t)((s_rx_tail + 1) & (RX_DMA_SIZE - 1));
return true;
```
- NDTR는 `SIZE`→`0`→reload `SIZE` 순환. `head = SIZE - NDTR` ∈ [0, SIZE-1] = 다음 DMA write 위치. `head==tail` = 미읽음 없음.

**제거 심볼**: `s_rx_byte`, `HAL_UART_RxCpltCallback`(USART1부), `HAL_UART_ErrorCallback`(USART1부), `usart1.c`의 IT 관련 static. **`irq.c:33` `USART1_IRQHandler` 제거**(또는 빈 본문 + 주석). **유지**: `usart1_send_blocking`(폴링 TX 10ms).

> RxCplt/Error 콜백은 weak HAL 오버라이드. mon(USART6)은 TX 전용이라 RX 콜백 미사용 → 제거 안전.

### 4.2 트리거 제거 — 2곳

- **`app_lcd_input.c:619`**: per-rx `mon_printf("[lcd] rx vp=...")`를 `#ifdef LCD_TRACE_RX … #endif`로 게이트(default 미정의=off). 기존 `LCD_DEMO_MEASURE_RAMP` 빌드 플래그 관례와 일치. 진단 시 `-DLCD_TRACE_RX`로 빌드. **유지**: 부팅/라이프사이클(`[lcd] ready`, `run_page_confirmed`, `[cfg]`) + `[lcd-hook]` 로그(저빈도).
- **`mon_usart6.c:12`**: `HAL_UART_Transmit(&huart6, …, HAL_MAX_DELAY)` → `…, MON_TX_TIMEOUT_MS)`. `#define MON_TX_TIMEOUT_MS 50`. 진단 채널이 앱을 영구 블록 못 함. 극한 시 라인 truncate 허용(진단 채널 수용 가능).

### 4.3 관측성

- `dgus_rx_drop_count()`(파서 레벨, `dgus_lcd.c`) **유지** = 유일한 RX 건강 신호(체크리스트 ⑨). DMA RX에서 저수준 손실이 생겨도(오래된 바이트 덮어씀) 파서가 desync→재동기하며 여기 잡힘.
- `usart1_rx_drop_count()` / `usart1_rx_error_count()`: DMA 모델에서 "링-풀"·"재무장 실패"·IT-ORE 개념 없음 → **둘 다 0 고정** + 주석(API/심볼 안정성 위해 유지). ORE 비트는 pop에서 **건드리지 않음**(클리어가 DR 읽기를 요구 → DMA의 DR 소비와 경합하므로 회피; DMA 모델에서 ORE는 양성·무해).
- 호출자 확인: 현재 `usart1_rx_*_count`/`dgus_rx_drop_count` 소비처를 grep해 시그니처 호환 확인(구현 task 0).

### 4.4 DMA2 스트림 예약 (향후)

USART1_RX = **DMA2 Stream2 Ch4**. 향후 W5500(SPI1) DMA 사용 시: **SPI1_RX → DMA2 Stream0 Ch3, SPI1_TX → DMA2 Stream3 Ch3** (Stream2/Stream5 회피 → 충돌 0). 이 예약을 `docs/pinmap.md` 또는 본 spec으로 추적.

## 5. 영향 파일

| 파일 | 변경 |
|------|------|
| `fw/src/periph.c` / `fw/include/periph.h` | `hdma_usart1_rx` 정의/extern |
| `fw/drivers/usart1.c` | IT→DMA circular 재아키텍처, pop NDTR 기반, 카운터 재정의 |
| `fw/src/irq.c` | `USART1_IRQHandler` 제거 |
| `fw/src/app_lcd_input.c` | L619 트레이스 `#ifdef LCD_TRACE_RX` 게이트 |
| `fw/drivers/mon_usart6.c` | mon TX 타임아웃 `HAL_MAX_DELAY`→50ms |

**무수정**: `dgus_lcd.c`(파서), `app.c`(드레인 루프), 그 외 LCD 소스.

## 6. 검증 (HW repro = 테스트)

베어메탈 — host 유닛테스트 하네스 없음. openocd RAM/레지스터 읽기로 입증.

1. 빌드 0-warning(`env -u STM32_TOOLCHAIN cmake --build build`).
2. `flash` → 카운터 리셋(reset).
3. **wedge repro 역검증**: SETUP → energy 슬라이더 공격적 반복 드래그(이전 wedge 트리거).
   - ① 표시 계속 갱신(멈춤 없음).
   - ② openocd: `__HAL_DMA_GET_COUNTER`(NDTR) 변동 + `s_rx_tail` 전진 + `USART1 CR1` RXNEIE 무관(0이어도 정상), `SR` ORE≈0.
   - ③ `dgus_rx_drop_count()` ≈ 0 (체크리스트 ⑨ PASS).
   - ④ **SAVE/CANCEL 동작**(드래그 직후 즉시).
4. **T10 체크리스트 재개**: `docs/changelog.md` 2026-05-27 항목 ②~⑦, ⑩ (⑧ N/A=전원공유). 트레이스 필요 항목은 `-DLCD_TRACE_RX` 빌드로.
5. 통과 → changelog/RESUME/NEXT_STEPS 갱신 + main PR + 태그 `hw-revA_fw-stage-lcd`.

## 7. 리스크 / 플래그

- **F1 DMA 매핑**: Stream2/Ch4가 RM0401과 일치하는지 구현 시 1차 확인(틀리면 RX 무동작). 대안 Stream5.
- **F2 NDTR race**: pop은 단일 reader(superloop), DMA는 writer. `head` 1회 읽기로 스냅샷, tail 단독 소유 → 락 불필요. 단 `s_rx_tail` 16비트 정렬 접근.
- **F3 버퍼 배치**: `.bss` SRAM이 DMA2 접근 가능 영역인지(F410 전 SRAM 가능). 링커 배치 확인.
- **F4 트레이스 게이트 OFF 시 검증 가시성**: rx 트레이스 없이 검증해야 하는 항목은 `[lcd-hook]` 로그 + openocd 카운터로 대체, 필요 시 `-DLCD_TRACE_RX` 재빌드.
- **F5 ORE/DR 경합 (해소됨)**: pop에서 ORE를 클리어하지 않기로 결정(§4.3) → CPU의 DR 읽기와 DMA의 DR 소비 경합 없음. DMA 모델에서 ORE는 무해(양성).
