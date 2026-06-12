/* fw/drivers/usart6_mb.c — Modbus-RTU transport on USART6.
 * Mechanism notes:
 *  - RX is the usart1.c free-running circular DMA pattern (no UART/DMA IRQ,
 *    no NVIC): DMA consumes DR on every RXNE, so the receiver cannot wedge.
 *  - Frame boundary: the DMA write position holding still for >= the per-baud
 *    break time while bytes are pending = end of frame. This is samd20's
 *    max_break_cnt x 250 us timer ported to the 1 ms sys_tick grid (ceiled),
 *    chosen over the USART IDLE flag for USB-master mid-frame-gap tolerance
 *    and to keep the zero-ISR discipline (plan Deviations 1).
 *  - Undetectable corner: if exactly 256 bytes (a full wrap) land between two
 *    polls the head looks unmoved — the merged frame then fails CRC and the
 *    master retries. Real frames are <= ~10 B; not reachable in practice. */
#include <stdbool.h>
#include "stm32f4xx_hal.h"
#include "periph.h"
#include "clock.h"     /* Error_Handler */
#include "sys_tick.h"
#include "usart6_mb.h"

#define MB_RX_DMA_SIZE  256u   /* power of two — mask, no modulo (usart1.c) */

static const uint32_t mb_baud[6]   = { 2400u, 4800u, 9600u, 19200u, 38400u, 115200u };
/* samd20 init_modbus max_break_cnt x 250 us, ceiled to 1 ms:
 * 56->14ms, 28->7ms, 14->3.5->4ms, >=19200: 7->1.75->2ms. */
static const uint8_t  mb_gap_ms[6] = { 14u, 7u, 4u, 2u, 2u, 2u };

static volatile uint8_t s_rx_dma_buf[MB_RX_DMA_SIZE];
static uint16_t s_rx_tail;       /* superloop reader index (sole owner) */
static uint16_t s_prev_head;     /* DMA position at the previous poll */
static uint32_t s_last_rx_ms;    /* last time the DMA position moved */
static uint32_t s_baud   = 19200u;
static uint8_t  s_gap_ms = 2u;
static bool     s_open;

static uint16_t rx_head(void)
{
    /* DMA write position = SIZE - NDTR; mask the NDTR reload edge (reads 0 ->
     * head 256) back to 0 since head is used for indexing here. */
    uint16_t head = (uint16_t)(MB_RX_DMA_SIZE -
                               __HAL_DMA_GET_COUNTER(&hdma_usart6_rx));
    return (uint16_t)(head & (MB_RX_DMA_SIZE - 1u));
}

void usart6_mb_open(uint8_t speed_idx, uint8_t parity_idx)
{
    /* Double-open guard: the app_modbus state machine always closes before
     * reopening; a second open here would silently kill the live DMA stream
     * mid-transfer and reset the ring state. Make it a no-op instead. */
    if (s_open) {
        return;
    }

    __HAL_RCC_DMA2_CLK_ENABLE();

    /* Re-init USART6 at the Modbus line config. GPIO PC6/PC7 AF8 was set by
     * usart6_init() at boot and is never unconfigured. */
    HAL_UART_DeInit(&huart6);
    huart6.Instance      = USART6;
    huart6.Init.BaudRate = (speed_idx < 6u) ? mb_baud[speed_idx]
                                            : 19200u;   /* samd20 default branch */
    s_baud   = huart6.Init.BaudRate;
    s_gap_ms = (speed_idx < 6u) ? mb_gap_ms[speed_idx] : 2u;
    /* STM32 parity rides in the 9th bit: EVEN/ODD need WORDLENGTH_9B for
     * 8 data bits + parity. 9B+NONE (the HAL's u16-buffer mode) never occurs. */
    if (parity_idx == 0u) {
        huart6.Init.Parity     = UART_PARITY_EVEN;
        huart6.Init.WordLength = UART_WORDLENGTH_9B;
    } else if (parity_idx == 1u) {
        huart6.Init.Parity     = UART_PARITY_ODD;
        huart6.Init.WordLength = UART_WORDLENGTH_9B;
    } else {
        huart6.Init.Parity     = UART_PARITY_NONE;
        huart6.Init.WordLength = UART_WORDLENGTH_8B;
    }
    huart6.Init.StopBits     = UART_STOPBITS_1;
    huart6.Init.Mode         = UART_MODE_TX_RX;
    huart6.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart6.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart6) != HAL_OK) {
        Error_Handler();
    }

    /* DMA2 Stream1 Ch5 = USART6_RX (RM0401 DMA2 request map; the Stream2 Ch5
     * alternate is unusable — Stream2 belongs to USART1 RX). Same circular
     * free-running config as usart1.c. */
    hdma_usart6_rx.Instance                 = DMA2_Stream1;
    hdma_usart6_rx.Init.Channel             = DMA_CHANNEL_5;
    hdma_usart6_rx.Init.Direction           = DMA_PERIPH_TO_MEMORY;
    hdma_usart6_rx.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_usart6_rx.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_usart6_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart6_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    hdma_usart6_rx.Init.Mode                = DMA_CIRCULAR;
    hdma_usart6_rx.Init.Priority            = DMA_PRIORITY_HIGH;
    hdma_usart6_rx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_usart6_rx) != HAL_OK) {
        Error_Handler();
    }
    __HAL_LINKDMA(&huart6, hdmarx, hdma_usart6_rx);

    s_rx_tail    = 0;
    s_prev_head  = 0;
    s_last_rx_ms = sys_tick_get_ms();

    if (HAL_DMA_Start(&hdma_usart6_rx, (uint32_t)&huart6.Instance->DR,
                      (uint32_t)s_rx_dma_buf, MB_RX_DMA_SIZE) != HAL_OK) {
        Error_Handler();
    }
    SET_BIT(USART6->CR3, USART_CR3_DMAR);
    s_open = true;
}

void usart6_mb_close(void)
{
    if (!s_open) {
        return;
    }
    CLEAR_BIT(USART6->CR3, USART_CR3_DMAR);
    (void)HAL_DMA_Abort(&hdma_usart6_rx);
    (void)HAL_DMA_DeInit(&hdma_usart6_rx);
    (void)HAL_UART_DeInit(&huart6);
    s_open = false;
    /* caller re-runs usart6_init() to restore the mon line config */
}

uint8_t usart6_mb_rx_frame(uint8_t *buf, uint8_t maxlen)
{
    if (!s_open) {
        return 0;
    }
    uint32_t now  = sys_tick_get_ms();
    uint16_t head = rx_head();

    if (head != s_prev_head) {       /* bytes still arriving — restart the gap */
        s_prev_head  = head;
        s_last_rx_ms = now;
        return 0;
    }
    if (head == s_rx_tail) {         /* nothing pending */
        return 0;
    }
    if ((uint32_t)(now - s_last_rx_ms) < s_gap_ms) {
        return 0;                    /* break gap not yet elapsed */
    }

    /* Gap elapsed: tail..head = one frame (samd20 endOfMessage equivalent). */
    uint16_t n = 0;
    while (s_rx_tail != head) {
        uint8_t b = s_rx_dma_buf[s_rx_tail];
        s_rx_tail = (uint16_t)((s_rx_tail + 1u) & (MB_RX_DMA_SIZE - 1u));
        if (n < maxlen) {
            buf[n] = b;
        }
        n++;                         /* oversize: keep draining, then drop */
    }
    if (n > maxlen) {
        return 0;                    /* oversize frame dropped (line garbage) */
    }
    return (uint8_t)n;
}

void usart6_mb_send(const uint8_t *buf, uint8_t len)
{
    if (!s_open) {
        return;
    }
    /* Blocking TX (spec). Timeout scaled to the frame time: 11 bits/char
     * (start + 8 + parity + stop) + 50 ms margin. Worst case 105 B @2400
     * ~= 482 ms — bounded superloop stall, spec-approved. */
    uint32_t timeout = ((uint32_t)len * 11000u) / s_baud + 50u;
    /* HAL pTxData is non-const; TX path is read-only on the buffer. */
    (void)HAL_UART_Transmit(&huart6, (uint8_t *)buf, len, timeout);

    /* Echo guard (plan Deviations 7): if the auto-DE transceiver loops our
     * own TX back into RX, discard everything received during the send so the
     * response (FC06 echo == request bytes!) is never re-parsed as a request. */
    s_rx_tail    = rx_head();
    s_prev_head  = s_rx_tail;
    s_last_rx_ms = sys_tick_get_ms();
}
