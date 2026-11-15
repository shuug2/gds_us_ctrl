/* fw/drivers/usart2.c — V31 mon UART (헤더 J3: PA2 TX / PA3 RX) + 수신 매직 감시.
 * TX 는 mon_usart6.c 의 mon_write 가 huart2 로 보낸다. RX 는 HAL 미경유 IRQ 직독. */
#include "define.h"
#if defined(HW_V31)
#include "stm32f4xx_hal.h"
#include "periph.h"
#include "clock.h"        /* Error_Handler */
#include "boot_magic.h"
#include "boot_jump.h"

#define USART2_IRQ_PRIO  10u   /* freq_ic 와 같은 최하위 — 사람 입력 속도라 지연 무관 */

static boot_magic_t s_magic;

/* USART2 115200 8N1 + RXNE 인터럽트 */
void usart2_init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef g = {
        .Pin       = GPIO_PIN_2 | GPIO_PIN_3,
        .Mode      = GPIO_MODE_AF_PP,
        .Pull      = GPIO_PULLUP,          /* 헤더 미연결 시 RX 부유 → 잡음 바이트 방지 */
        .Speed     = GPIO_SPEED_FREQ_VERY_HIGH,
        .Alternate = GPIO_AF7_USART2,
    };
    HAL_GPIO_Init(GPIOA, &g);

    __HAL_RCC_USART2_CLK_ENABLE();
    huart2.Instance          = USART2;
    huart2.Init.BaudRate     = 115200;
    huart2.Init.WordLength   = UART_WORDLENGTH_8B;
    huart2.Init.StopBits     = UART_STOPBITS_1;
    huart2.Init.Parity       = UART_PARITY_NONE;
    huart2.Init.Mode         = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart2) != HAL_OK) Error_Handler();

    boot_magic_init(&s_magic);
    __HAL_UART_ENABLE_IT(&huart2, UART_IT_RXNE);
    HAL_NVIC_SetPriority(USART2_IRQn, USART2_IRQ_PRIO, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
}

/* 수신 1 바이트 → 매직 매처. SR→DR 읽기 순서가 ORE/FE/NE 도 지운다. 리셋은 여기서 안 한다 */
void USART2_IRQHandler(void)
{
    uint32_t sr = USART2->SR;
    if ((sr & (USART_SR_RXNE | USART_SR_ORE | USART_SR_FE | USART_SR_NE)) == 0u) {
        return;
    }
    uint8_t b = (uint8_t)USART2->DR;
    if ((sr & (USART_SR_FE | USART_SR_NE)) == 0u && boot_magic_feed(&s_magic, b)) {
        boot_jump_arm();
    }
}
#endif /* HW_V31 */
