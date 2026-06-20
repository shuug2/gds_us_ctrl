/* fw/src/board.c */
#include "stm32f4xx_hal.h"
#include "board.h"

/* OSC 출력 3채널 active-LOW (idle = HIGH = off). PB2/OSC0<-PB1, PB10/OSC1<-PB0,
 * PB14/OSC4<-PC7. PB12/PB13(OSC2/OSC3 후보)은 미확정으로 미설정. 실제 발진
 * 구동은 B-SEAM(stub) — 여기선 idle만. (heartbeat PB3는 dormant → io.c가 OVLD
 * 릴레이로 재용도, 2026-06-20 제거) */
#define CTRL_OSC_OUT_PINS (GPIO_PIN_2 | GPIO_PIN_10 | GPIO_PIN_14)

void board_init(void) {
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef out = {
        .Mode  = GPIO_MODE_OUTPUT_OD,   /* open-drain: PP는 OSC보드와 전기 충돌(GAP=0) — main 승격분 유지 */
        .Pull  = GPIO_NOPULL,
        .Speed = GPIO_SPEED_FREQ_LOW,
    };
    HAL_GPIO_WritePin(GPIOB, CTRL_OSC_OUT_PINS, GPIO_PIN_SET);   /* idle = hi-Z (외부 풀업 HIGH) */
    out.Pin = CTRL_OSC_OUT_PINS;
    HAL_GPIO_Init(GPIOB, &out);
}
