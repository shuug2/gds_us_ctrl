/* fw/src/board.c */
#include "stm32f4xx_hal.h"
#include "board.h"

/* OSC 인터페이스 출력 3채널 = open-drain (open collector), active-LOW:
 * on=드레인 LOW, off=hi-Z (idle HIGH는 외부 풀업 의존).
 * PB2=SEEK(OSC_OUT0), PB10=RESET(OSC_OUT1), PB14=초음파 게이트(OSC4).
 * PB12·PB13은 io.c가 입력으로 설정(PB12=초음파 출력 피드백, PB13=OVLD sense). */
#define CTRL_OSC_OUT_PINS (GPIO_PIN_2 | GPIO_PIN_10 | GPIO_PIN_14)

void board_init(void) {
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef out = {
        .Mode  = GPIO_MODE_OUTPUT_OD,   /* open-drain (active-LOW; idle HIGH=외부 풀업) */
        .Pull  = GPIO_NOPULL,
        .Speed = GPIO_SPEED_FREQ_LOW,
    };
    HAL_GPIO_WritePin(GPIOB, CTRL_OSC_OUT_PINS, GPIO_PIN_SET);   /* idle = hi-Z (외부 풀업 HIGH) */
    out.Pin = CTRL_OSC_OUT_PINS;
    HAL_GPIO_Init(GPIOB, &out);
}

void board_osc4(bool on)
{
    /* PB14 초음파 게이트 active-LOW open-drain: 출력 중 = LOW(드레인 on), idle = hi-Z. */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

void board_reset(bool on)
{
    /* PB10 OSC_OUT1 active-LOW open-drain: RESET 중 = LOW, idle = hi-Z. */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

void board_seek(bool on)
{
    /* PB2 OSC_OUT0 active-LOW open-drain: SEEK 중 = LOW, idle = hi-Z. */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
}
