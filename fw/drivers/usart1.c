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
