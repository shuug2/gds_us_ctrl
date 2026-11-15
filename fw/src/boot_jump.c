/* fw/src/boot_jump.c — V31 UART 부트로더 진입. 설명 = boot_jump.h */
#include "define.h"
#if defined(HW_V31)
#include <stdbool.h>
#include <stdint.h>
#include "stm32f4xx_hal.h"
#include "boot_jump.h"
#include "mon.h"

#define BOOT_JUMP_MARK  0xB00710ADu   /* RTC->BKP0R 표식 — 시스템 리셋에 살아남는다 */
#define SYSMEM_BASE     0x1FFF0000u   /* F410 시스템 메모리 (ROM 부트로더 벡터 테이블) */

static volatile bool s_armed;

/* 백업 도메인 쓰기 해제 — 리셋 직후엔 쓰기 보호 상태. F410 은 RTC APB 클럭도 따로 켠다 */
static void bkp_open(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_RTCAPBEN;
    (void)RCC->APB1ENR;               /* 클럭 반영 대기 */
    PWR->CR |= PWR_CR_DBP;
}

/* 리셋 직후: 표식이 있으면 지우고 ROM 부트로더로 점프 (안 돌아옴) */
void boot_jump_check(void)
{
    bkp_open();
    if (RTC->BKP0R != BOOT_JUMP_MARK) {
        PWR->CR &= ~PWR_CR_DBP;       /* 정상 부팅 — 리셋 상태로 되돌린다 */
        RCC->APB1ENR &= ~(RCC_APB1ENR_PWREN | RCC_APB1ENR_RTCAPBEN);
        return;
    }
    RTC->BKP0R = 0u;                  /* 1 회용 — 부트로더에서 리셋하면 정상 부팅 */

    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    (void)RCC->APB2ENR;
    SYSCFG->MEMRMP = SYSCFG_MEMRMP_MEM_MODE_0;   /* 0x0 = 시스템 메모리 (BOOT0 부팅과 같은 맵) */
    SCB->VTOR = 0u;
    SysTick->CTRL = 0u;               /* HAL_Init 전이라 꺼져 있지만 명시 */

    uint32_t sp = *(volatile uint32_t *)SYSMEM_BASE;
    uint32_t pc = *(volatile uint32_t *)(SYSMEM_BASE + 4u);
    __set_MSP(sp);
    ((void (*)(void))pc)();
    while (1) {}
}

/* IRQ: 매직 수신 표시 */
void boot_jump_arm(void) { s_armed = true; }

/* 슈퍼루프: 표식 기록 후 시스템 리셋 */
void boot_jump_poll(void)
{
    if (!s_armed) {
        return;
    }
    mon_writeln("[boot] magic -> ROM bootloader (USART2 8E1)");
    bkp_open();
    RTC->BKP0R = BOOT_JUMP_MARK;
    NVIC_SystemReset();
}
#endif /* HW_V31 */
