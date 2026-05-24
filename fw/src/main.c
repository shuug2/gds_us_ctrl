/* fw/src/main.c — Stage A form (Phase 2 + USART1/DGUS LCD) */
#include "stm32f4xx_hal.h"
#include "clock.h"
#include "app.h"
#include "usart1.h"
#include "i2c1.h"
#include "dgus_lcd.h"

extern void usart6_init(void);   /* drivers/usart.c */
extern void tim11_init(void);    /* drivers/tim.c */
extern void board_init(void);    /* src/board.c */

int main(void) {
    HAL_Init();
    clock_init();      /* 96 MHz */
    /* TODO Stage A: iwdg_init(2000); */
    usart6_init();     /* PC6/PC7 + 115200 8N1 */
    usart1_init();     /* Stage A: PA9/PA10 AF7 + NVIC + 첫 RX 무장 */
    i2c1_init();       /* Stage B: I2C1 @400kHz (PB6/PB7) for FRAM */
    tim11_init();      /* 1 kHz IRQ enabled, base not started yet */
    board_init();      /* GPIO out + CTRL_OSC0..4 LOW */
    dgus_init();       /* Stage A: DGUS 프로토콜 레이어 상태 클리어 */
    app_init();        /* sys_tick start, mon banner */

    while (1) {
        app_loop_iter();
        /* TODO Stage A: HAL_IWDG_Refresh(&hiwdg); */
    }
}
