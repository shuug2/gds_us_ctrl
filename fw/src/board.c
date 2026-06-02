/* fw/src/board.c */
#include "stm32f4xx_hal.h"
#include "board.h"

#define HB_PORT   GPIOB
#define HB_PIN    GPIO_PIN_3       /* CON_OVLD heartbeat 임시 — Stage A에서 본용도 복원 */
/* OSC channels per pinmap.md §117-125: PB2|PB10|PB12|PB13|PB14 (the old mask
 * was wrong: PB2 missing, PB15 spurious). Only the 3 image-confirmed OUTPUT
 * channels are driven here (analysis C6/C7): PB2/OSC0<-mega16 PB1,
 * PB10/OSC1<-PB0, PB14/OSC4<-PC7 — all active-LOW, so idle = HIGH. PB12/PB13
 * (OSC2/OSC3 <- mega16 PC4/PA7) are DDR-inputs in the image (C7); direction
 * unconfirmed (B-OSC-MAP), so they are left unconfigured until bench measure. */
#define CTRL_OSC_OUT_PINS (GPIO_PIN_2 | GPIO_PIN_10 | GPIO_PIN_14)

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

    /* Idle the 3 confirmed OSC outputs HIGH (= OSC off; active-LOW per C6).
     * Set the level BEFORE switching to output to avoid a boot LOW glitch. */
    HAL_GPIO_WritePin(GPIOB, CTRL_OSC_OUT_PINS, GPIO_PIN_SET);
    out.Pin = CTRL_OSC_OUT_PINS;
    HAL_GPIO_Init(GPIOB, &out);
}

void board_heartbeat_toggle(void) {
    HAL_GPIO_TogglePin(HB_PORT, HB_PIN);
}
