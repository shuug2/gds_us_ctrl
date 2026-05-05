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
static volatile uint16_t s_rx_error_count;              /* ORE/FE/NE/PE 누적 (ErrorCallback 진입 횟수) */

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
    s_rx_head        = 0;
    s_rx_tail        = 0;
    s_rx_drop_count  = 0;
    s_rx_error_count = 0;

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

uint16_t usart1_rx_error_count(void)
{
    return s_rx_error_count;
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

/* HAL UART error 경로 — vendor stm32f4xx_hal_uart.c 의 HAL_UART_IRQHandler 가
 * SR 의 ORE/FE/NE/PE 비트 발견 시 UART_EndRxTransfer (RXNEIE clear) 후 본 콜백 호출.
 * 이 경로에서는 HAL_UART_RxCpltCallback 발화하지 않으므로 여기서 명시적 재무장 필수.
 * 미구현 시 ORE 한 번 → RX 영구 정지 (samd20 결함 미커버 영역, code reviewer HIGH).
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *h)
{
    if (h->Instance != USART1) {
        return;
    }
    s_rx_error_count++;
    /* ORE/FE/NE/PE 플래그 클리어 — vendor 매크로는 SR 읽기 + DR 읽기 시퀀스 */
    __HAL_UART_CLEAR_OREFLAG(h);
    __HAL_UART_CLEAR_FEFLAG(h);
    __HAL_UART_CLEAR_NEFLAG(h);
    __HAL_UART_CLEAR_PEFLAG(h);
    /* RX 재무장 — 실패 시 drop_count 로 가시화 (R5 의 일반화) */
    if (HAL_UART_Receive_IT(&huart1, (uint8_t *)&s_rx_byte, 1) != HAL_OK) {
        s_rx_drop_count++;
    }
}
