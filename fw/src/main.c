/* fw/src/main.c — Stage A form (Phase 2 + USART1/DGUS LCD) */
#include "stm32f4xx_hal.h"
#include "clock.h"
#include "app.h"
#include "app_reg.h"
#include "app_weld.h"
#include "app_seek_reset.h"
#include "app_modbus.h"
#include "app_eth.h"
#include "app_buzzer.h"
#include "app_fault_alarm.h"
#include "app_overload.h"
#include "app_input.h"
#include "app_osc_init.h"
#include "sys_tick.h"
#include "usart1.h"
#include "i2c1.h"
#include "freq_ic.h"
#include "dgus_lcd.h"

#include "io.h"

extern void usart6_init(void);   /* drivers/usart.c */
extern void tim11_init(void);    /* drivers/tim.c */
extern void board_init(void);    /* src/board.c */

#define BOOT_BEEP_MS  100u   /* 부팅 완료 1회 beep 길이 (점멸계 250ms와 구분) */

int main(void) {
    HAL_Init();
    clock_init();      /* 96 MHz */
    /* TODO Stage A: iwdg_init(2000); */
    usart6_init();     /* PC6/PC7 + 115200 8N1 */
    usart1_init();     /* Stage A: PA9/PA10 AF7 + NVIC + 첫 RX 무장 */
    i2c1_init();       /* Stage B: I2C1 @400kHz (PB6/PB7) for FRAM */
    tim11_init();      /* 1 kHz IRQ enabled, base not started yet */
    board_init();      /* GPIO out + 3 confirmed OSC channels idle-HIGH (off) */
    freq_ic_init();    /* FREQ_IN(PA0/TIM5_CH1) 입력캡처 — HW only, sys_tick 불요 */
    io_init();         /* 커넥터/패널 GPIO (입력 pull-up / 출력 idle off) */
    sys_tick_init();        /* hoist from app_init: OSC 부팅 시퀀스가 sys_tick 필요 */
    io_buzzer(true);        /* 부팅 beep — 블로킹 딜레이(OSC 핸드셰이크/LCD 부트) 전
                             * 최속 발음(사용자 2026-07-18). 부저 FSM tick이 아직 없어
                             * 직접 구동+블로킹 100ms. PB12 H 윈도(전원 ~600ms 후)보다
                             * 앞서 끝나 OSC 시퀀스 무영향. */
    sys_tick_delay_ms(BOOT_BEEP_MS);
    io_buzzer(false);
    app_osc_init_init();    /* OSC FSM reset */
    app_osc_init_run_to_done(); /* 블로킹 OSC 보드 초기화: PB12 H→L 감지 후 RESET/SEEK
                                 * 펄스. app_init(LCD 부팅, 블로킹) 전에 완주해야
                                 * PB12 펄스(전원~600~1200ms)를 놓치지 않음. */
    dgus_init();       /* Stage A: DGUS 프로토콜 레이어 상태 클리어 */
    app_init();        /* sys_tick start, mon banner */
    app_reg_init();    /* Stage D: ADC1 + regulation state (needs sys_tick up) */
    app_weld_init();   /* Stage Weld-Cycle: FSM reset (needs sys_tick up) */
    app_seek_reset_init();  /* Stage SEEK/RESET: FSM reset (needs sys_tick up) */
    app_buzzer_init();      /* 부저 글루 (needs sys_tick up) — 부팅 beep은 위
                             * sys_tick_init 직후 블로킹 구동으로 이동(2026-07-18) */
    app_fault_alarm_init(); /* 일반 fault(OVTIME 등) 부저 점멸 (needs sys_tick up) */
    app_overload_init();    /* 과부하 글루 (needs io_init + sys_tick up) */
    app_input_init();       /* 물리 명령 입력 + E-stop 글루 (needs io_init + sys_tick up) */
    app_modbus_init(); /* Stage C: USART6 occupancy decision (needs cfg loaded by app_init) */
    app_eth_init();    /* Stage C slice 2a/2b: W5500 bring-up (non-fatal). TCP
                        * server runs from app_modbus_tick() when comm_mode is
                        * ETH_STATIC or ETH_DHCP; slice 2b drives the DHCP client
                        * from app_eth_tick() in the superloop. */

    while (1) {
        app_loop_iter();
        /* TODO Stage A: HAL_IWDG_Refresh(&hiwdg); */
    }
}
