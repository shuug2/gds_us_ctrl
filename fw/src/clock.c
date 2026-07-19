/* fw/src/clock.c */
#include "stm32f4xx_hal.h"
#include "clock.h"

/* 0 = HSE(16MHz X-tal) PLL 정상. 1 = HSE 기동 실패 → HSI 폴백.
 * SWD 정적 read 진단용 — 폴백이면 주파수 표시/타이밍이 HSI 공차(±1%대)로
 * 틀어지므로 크리스탈 회로 점검 필요. */
static uint8_t s_clk_hsi_fallback;

/* 96MHz 클럭 설정 */
void clock_init(void) {
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /* HSE(보드 16MHz X-tal, HSE_VALUE는 CMake 주입) 우선 — 크리스탈 정확도로
     * FREQ_IN 측정/타이밍 편차 원천 제거 (2026-07-05 벤치: HSI 실측 +1.39%가
     * 주파수 underread -479Hz·전 타이밍 -1.3%의 단일 원인, 스코프 34.98kHz 대조).
     * PLL 체인은 기존 HSI 구성과 동일(16MHz /M8 ×N96 /P2 = 96MHz) — 소스만 교체. */
    osc.OscillatorType       = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState             = RCC_HSE_ON;
    osc.PLL.PLLState         = RCC_PLL_ON;
    osc.PLL.PLLSource        = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM             = 8;
    osc.PLL.PLLN             = 96;
    osc.PLL.PLLP             = RCC_PLLP_DIV2;
    osc.PLL.PLLQ             = 4;
    osc.PLL.PLLR             = 2;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        /* HSE 기동 실패(크리스탈 미실장/불량 대비) — HSI 폴백으로 부팅 유지
         * (Phase 1 원구성과 동일). 필드에서 부팅 불능이 되지 않게 하는 안전망. */
        s_clk_hsi_fallback = 1u;

        RCC_OscInitTypeDef hsi = {0};
        hsi.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
        hsi.HSIState            = RCC_HSI_ON;
        hsi.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
        hsi.PLL.PLLState        = RCC_PLL_ON;
        hsi.PLL.PLLSource       = RCC_PLLSOURCE_HSI;
        hsi.PLL.PLLM            = 8;
        hsi.PLL.PLLN            = 96;
        hsi.PLL.PLLP            = RCC_PLLP_DIV2;
        hsi.PLL.PLLQ            = 4;
        hsi.PLL.PLLR            = 2;
        if (HAL_RCC_OscConfig(&hsi) != HAL_OK) Error_Handler();
    }

    clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                       | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_3) != HAL_OK) Error_Handler();
}
