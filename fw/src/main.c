/* fw/src/main.c — Phase 1 form */
#include "stm32f4xx_hal.h"
#include "clock.h"

int main(void) {
    HAL_Init();
    clock_init();

    /* Phase 1 verification: GDB로 PC 위치 확인. Phase 2 modules는 Task 17에서 추가 */
    while (1) {
        __NOP();
    }
}
