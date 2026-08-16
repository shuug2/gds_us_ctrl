/* fw/drivers/i2c1.c
 *
 * I2C1 raw I/O — FM24C16B FRAM (PB6=SCL / PB7=SDA, AF4, 400 kHz Fast mode).
 * External 10 kΩ pull-ups to VCC_5 → GPIO_NOPULL. PCLK1 = 48 MHz (APB1 = HCLK/2).
 * Callers: fw/drivers/fram.c (FRAM) + fw/drivers/i2c_pot.c (U4 I2C_POT @0x28).
 * All access is superloop-only (no ISR) → no bus contention.
 */
#include "stm32f4xx_hal.h"
#include "periph.h"
#include "clock.h"   /* Error_Handler */
#include "i2c1.h"

static volatile uint16_t s_err_count;

static uint8_t s_unstick_events;

/* busy-wait 지연 */
static void unstick_delay(void)
{
    /* ~10 µs @96 MHz busy-wait — i2c1_init()은 sys_tick 기동 전(main.c:24)이라
     * tick 지연 불가. I2C slave는 저속 클럭 무제한 허용이라 정밀도 비요구. */
    for (volatile uint32_t i = 0u; i < 240u; i++) { }
}

/* I2C 버스 stuck 복구 */
static void i2c1_bus_unstick(void)
{
    /* SDA(PB7) stuck-low 복구: SCL(PB6) GPIO-OD 9클럭 + STOP (감사 H2).
     * HAL_I2C_Init 전, GPIO clock enable 후에만 호출. 실패해도 진행 —
     * 이후 트랜잭션 실패는 s_err_count로 드러남. */
    GPIO_InitTypeDef g = {0};

    /* SDA=input으로 버스 상태 관찰, SCL=OD output(idle high) */
    g.Pin   = GPIO_PIN_7;
    g.Mode  = GPIO_MODE_INPUT;
    g.Pull  = GPIO_NOPULL;              /* 외부 10k 풀업 (보드) */
    HAL_GPIO_Init(GPIOB, &g);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
    g.Pin   = GPIO_PIN_6;
    g.Mode  = GPIO_MODE_OUTPUT_OD;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &g);
    unstick_delay();

    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7) == GPIO_PIN_SET) {
        s_unstick_events = 0u;          /* 버스 깨끗 — 통상 경로 */
        return;
    }

    uint8_t clocks = 0u;
    while (clocks < 9u) {               /* 9클럭 = 8data+ACK 최악 케이스 */
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
        unstick_delay();
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
        unstick_delay();
        clocks++;
        if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7) == GPIO_PIN_SET) {
            break;
        }
    }

    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7) != GPIO_PIN_SET) {
        s_unstick_events = 0xFFu;       /* 복구 실패 — HAL init은 그대로 진행 */
        return;
    }

    /* STOP 조건: SCL high 상태에서 SDA low→high */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET);
    g.Pin  = GPIO_PIN_7;
    g.Mode = GPIO_MODE_OUTPUT_OD;
    HAL_GPIO_Init(GPIOB, &g);
    unstick_delay();
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);
    unstick_delay();
    s_unstick_events = clocks;
}

/* I2C1 초기화 */
void i2c1_init(void)
{
    /* 1. clocks */
    __HAL_RCC_I2C1_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* 1.5 bus-unstick preflight (감사 H2) — AF 설정 전 GPIO로 SDA stuck 복구.
     * 아래 §2 HAL_GPIO_Init(AF_OD)이 임시 GPIO 모드를 덮어쓴다. */
    i2c1_bus_unstick();

    /* 2. GPIO PB6/PB7 AF4, open-drain, no internal pull (external 10k to 5V) */
    GPIO_InitTypeDef g = {0};
    g.Pin       = GPIO_PIN_6 | GPIO_PIN_7;
    g.Mode      = GPIO_MODE_AF_OD;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &g);

    /* 3. I2C config — 400 kHz Fast mode */
    hi2c1.Instance             = I2C1;
    hi2c1.Init.ClockSpeed      = 400000;
    hi2c1.Init.DutyCycle       = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1     = 0;
    hi2c1.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2     = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
        Error_Handler();
    }

    s_err_count = 0;
}

/* I2C 메모리 읽기 */
HAL_StatusTypeDef i2c1_mem_read(uint8_t dev7, uint8_t mem_addr, uint8_t *buf, uint16_t n)
{
    HAL_StatusTypeDef st = HAL_I2C_Mem_Read(&hi2c1, (uint16_t)(dev7 << 1), mem_addr,
                                            I2C_MEMADD_SIZE_8BIT, buf, n, I2C1_TIMEOUT_MS);
    if (st != HAL_OK) {
        s_err_count++;
    }
    return st;
}

/* I2C 메모리 쓰기 */
HAL_StatusTypeDef i2c1_mem_write(uint8_t dev7, uint8_t mem_addr, const uint8_t *buf, uint16_t n)
{
    HAL_StatusTypeDef st = HAL_I2C_Mem_Write(&hi2c1, (uint16_t)(dev7 << 1), mem_addr,
                                             I2C_MEMADD_SIZE_8BIT, (uint8_t *)buf, n, I2C1_TIMEOUT_MS);
    if (st != HAL_OK) {
        s_err_count++;
    }
    return st;
}

/* 에러 카운트 조회 */
uint16_t i2c1_err_count(void) { return s_err_count; }

/* unstick 이벤트 조회 */
uint8_t i2c1_unstick_events(void) { return s_unstick_events; }
