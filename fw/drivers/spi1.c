/* fw/drivers/spi1.c — see spi1.h. HAL SPI1 @ ~12 MHz (APB2 96 MHz / 8),
 * Mode 0 (CPOL=0/CPHA=0), 8-bit MSB-first. CS = PA4 plain GPIO. */
#include "spi1.h"
#include "stm32f4xx_hal.h"
#include "clock.h"  /* Error_Handler */

static SPI_HandleTypeDef s_spi1;

#define ETH_CS_PORT    GPIOA
#define ETH_CS_PIN     GPIO_PIN_4
#define ETH_RST_PORT   GPIOC
#define ETH_RST_PIN    GPIO_PIN_5
#define ETH_INT_PORT   GPIOC
#define ETH_INT_PIN    GPIO_PIN_4

void spi1_cs_low(void)  { HAL_GPIO_WritePin(ETH_CS_PORT, ETH_CS_PIN, GPIO_PIN_RESET); }
void spi1_cs_high(void) { HAL_GPIO_WritePin(ETH_CS_PORT, ETH_CS_PIN, GPIO_PIN_SET);   }

void spi1_eth_reset(void)
{
    HAL_GPIO_WritePin(ETH_RST_PORT, ETH_RST_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(ETH_RST_PORT, ETH_RST_PIN, GPIO_PIN_RESET);
    HAL_Delay(2);                 /* W5500 RSTn low >= 500us */
    HAL_GPIO_WritePin(ETH_RST_PORT, ETH_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(60);                /* PLL lock >= 50ms before access */
}

uint8_t spi1_xfer_byte(uint8_t tx)
{
    uint8_t rx = 0u;
    (void)HAL_SPI_TransmitReceive(&s_spi1, &tx, &rx, 1u, 10u);
    return rx;
}

void spi1_burst_read(uint8_t *buf, uint16_t len)
{
    /* W5500 burst read: clock out 0xFF, capture MISO. */
    for (uint16_t i = 0u; i < len; i++) { buf[i] = spi1_xfer_byte(0xFFu); }
}

void spi1_burst_write(uint8_t *buf, uint16_t len)
{
    (void)HAL_SPI_Transmit(&s_spi1, buf, len, 100u);
}

void spi1_init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_SPI1_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};

    /* PA5/6/7 = SPI1 SCK/MISO/MOSI, AF5 */
    g.Pin       = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &g);

    /* PA4 = software CS, idle HIGH */
    HAL_GPIO_WritePin(ETH_CS_PORT, ETH_CS_PIN, GPIO_PIN_SET);
    g.Pin       = ETH_CS_PIN;
    g.Mode      = GPIO_MODE_OUTPUT_PP;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = 0u;
    HAL_GPIO_Init(ETH_CS_PORT, &g);

    /* PC5 = NRST out, idle HIGH (deasserted) */
    HAL_GPIO_WritePin(ETH_RST_PORT, ETH_RST_PIN, GPIO_PIN_SET);
    g.Pin  = ETH_RST_PIN;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(ETH_RST_PORT, &g);

    /* PC4 = INT input (polled this slice) */
    g.Pin  = ETH_INT_PIN;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(ETH_INT_PORT, &g);

    s_spi1.Instance               = SPI1;
    s_spi1.Init.Mode              = SPI_MODE_MASTER;
    s_spi1.Init.Direction         = SPI_DIRECTION_2LINES;
    s_spi1.Init.DataSize          = SPI_DATASIZE_8BIT;
    s_spi1.Init.CLKPolarity       = SPI_POLARITY_LOW;     /* Mode 0 */
    s_spi1.Init.CLKPhase          = SPI_PHASE_1EDGE;      /* Mode 0 */
    s_spi1.Init.NSS               = SPI_NSS_SOFT;
    s_spi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;  /* 96/8 = 12 MHz */
    s_spi1.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    s_spi1.Init.TIMode            = SPI_TIMODE_DISABLE;
    s_spi1.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    s_spi1.Init.CRCPolynomial     = 7u;
    if (HAL_SPI_Init(&s_spi1) != HAL_OK) { Error_Handler(); }
}
