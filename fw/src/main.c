/* fw/src/main.c — Phase 2 form */
#include "stm32f4xx_hal.h"
#include "clock.h"
#include "app.h"

extern void usart6_init(void);   /* drivers/usart.c */
extern void tim11_init(void);    /* drivers/tim.c */
extern void board_init(void);    /* src/board.c */

int main(void) {
    HAL_Init();
    clock_init();      /* 96 MHz */
    /* TODO Stage A: iwdg_init(2000); */
    usart6_init();     /* PC6/PC7 + 115200 8N1 */
    tim11_init();      /* 1 kHz IRQ enabled, base not started yet */
    board_init();      /* GPIO out + CTRL_OSC0..4 LOW */
    app_init();        /* sys_tick start, mon banner */

    while (1) {
        app_loop_iter();
        /* TODO Stage A: HAL_IWDG_Refresh(&hiwdg); */
    }
}
