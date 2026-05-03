/* fw/src/board.c */
#include "stm32f4xx_hal.h"
#include "board.h"

#define HB_PORT   GPIOB
#define HB_PIN    GPIO_PIN_3       /* CON_OVLD heartbeat 임시 — Stage A에서 본용도 복원 */
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
