/* fw/drivers/io.c — 커넥터/패널 GPIO. 활성레벨 spec §2:
 * 버튼/센서 active-LOW(pull-up), overload active-HIGH(pull-down),
 * USOUT/OVLD/BUZZER active-HIGH(idle LOW), SOL_DN active-LOW(idle HIGH=off).
 * PA0(FREQ_IN)은 슬라이스 B에서 TIM5 AF로 설정 — 여기서 건드리지 않음. */
#include "stm32f4xx_hal.h"
#include "io.h"

/* ---- 입력 핀 ---- */
#define START_PORT      GPIOA
#define START_PIN       GPIO_PIN_15
#define SENS_UP_PORT    GPIOA
#define SENS_UP_PIN     GPIO_PIN_12
#define SENS_DN_PORT    GPIOA
#define SENS_DN_PIN     GPIO_PIN_11
#define KEY2_PORT       GPIOB
#define KEY2_PIN        GPIO_PIN_11
#define OVLD_IN_PORT    GPIOB
#define OVLD_IN_PIN     GPIO_PIN_13
#define USFB_PORT       GPIOB
#define USFB_PIN        GPIO_PIN_12     /* 초음파 출력 피드백: 출력 중 L->H (active-HIGH) */
#define PANEL_RESET_PORT      GPIOC
#define PANEL_RESET_PIN       GPIO_PIN_10   /* 패널 RESET 버튼 입력 (board_reset PB10 출력과 구분) */
#define ESTOP_PORT      GPIOC
#define ESTOP_PIN       GPIO_PIN_11
#define KEY1_PORT       GPIOC
#define KEY1_PIN        GPIO_PIN_12

/* ---- 출력 핀 ---- */
#define BUZZER_PORT     GPIOA
#define BUZZER_PIN      GPIO_PIN_2
#define OVLD_RLY_PORT   GPIOB
#define OVLD_RLY_PIN    GPIO_PIN_3
#define USOUT_PORT      GPIOB
#define USOUT_PIN       GPIO_PIN_4
#define SOL_DN_PORT     GPIOB
#define SOL_DN_PIN      GPIO_PIN_5

/* 패널 GPIO 초기화 */
void io_init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitTypeDef in_pu = {
        .Mode = GPIO_MODE_INPUT, .Pull = GPIO_PULLUP, .Speed = GPIO_SPEED_FREQ_LOW,
    };
    GPIO_InitTypeDef in_pd = {
        .Mode = GPIO_MODE_INPUT, .Pull = GPIO_PULLDOWN, .Speed = GPIO_SPEED_FREQ_LOW,
    };
    GPIO_InitTypeDef out = {
        .Mode = GPIO_MODE_OUTPUT_PP, .Pull = GPIO_NOPULL, .Speed = GPIO_SPEED_FREQ_LOW,
    };

    /* 입력 active-LOW (pull-up) */
    in_pu.Pin = START_PIN;   HAL_GPIO_Init(START_PORT,   &in_pu);
    in_pu.Pin = SENS_UP_PIN; HAL_GPIO_Init(SENS_UP_PORT, &in_pu);
    in_pu.Pin = SENS_DN_PIN; HAL_GPIO_Init(SENS_DN_PORT, &in_pu);
    in_pu.Pin = KEY2_PIN;    HAL_GPIO_Init(KEY2_PORT,    &in_pu);
    in_pu.Pin = PANEL_RESET_PIN;   HAL_GPIO_Init(PANEL_RESET_PORT,   &in_pu);
    in_pu.Pin = ESTOP_PIN;   HAL_GPIO_Init(ESTOP_PORT,   &in_pu);  /* 폴라리티는 호출측 model_type 분기 */
    in_pu.Pin = KEY1_PIN;    HAL_GPIO_Init(KEY1_PORT,    &in_pu);

    /* 입력 active-HIGH (pull-down → idle LOW): overload sense + 초음파 출력 피드백 */
    in_pd.Pin = OVLD_IN_PIN; HAL_GPIO_Init(OVLD_IN_PORT, &in_pd);
    in_pd.Pin = USFB_PIN;    HAL_GPIO_Init(USFB_PORT,    &in_pd);

    /* 출력 idle = inactive (레벨 먼저 세팅 후 출력 전환 — boot glitch 회피) */
    HAL_GPIO_WritePin(BUZZER_PORT,   BUZZER_PIN,   GPIO_PIN_RESET);  /* active-H off */
    HAL_GPIO_WritePin(OVLD_RLY_PORT, OVLD_RLY_PIN, GPIO_PIN_RESET);  /* active-H off */
    HAL_GPIO_WritePin(USOUT_PORT,    USOUT_PIN,    GPIO_PIN_RESET);  /* active-H off */
    HAL_GPIO_WritePin(SOL_DN_PORT,   SOL_DN_PIN,   GPIO_PIN_SET);    /* active-L off(=SOL_OFF) */
    out.Pin = BUZZER_PIN;   HAL_GPIO_Init(BUZZER_PORT,   &out);
    out.Pin = OVLD_RLY_PIN; HAL_GPIO_Init(OVLD_RLY_PORT, &out);
    out.Pin = USOUT_PIN;    HAL_GPIO_Init(USOUT_PORT,    &out);
    out.Pin = SOL_DN_PIN;   HAL_GPIO_Init(SOL_DN_PORT,   &out);
}

/* START 입력 읽기 */
uint8_t io_read_start(void)      { return (uint8_t)HAL_GPIO_ReadPin(START_PORT,   START_PIN);   }
/* RESET 입력 읽기 */
uint8_t io_read_reset(void)      { return (uint8_t)HAL_GPIO_ReadPin(PANEL_RESET_PORT,   PANEL_RESET_PIN);   }
/* ESTOP/SEEK 읽기 */
uint8_t io_read_estop_seek(void) { return (uint8_t)HAL_GPIO_ReadPin(ESTOP_PORT,   ESTOP_PIN);   }
/* KEY1 입력 읽기 */
uint8_t io_read_key1(void)       { return (uint8_t)HAL_GPIO_ReadPin(KEY1_PORT,    KEY1_PIN);    }
/* KEY2 입력 읽기 */
uint8_t io_read_key2(void)       { return (uint8_t)HAL_GPIO_ReadPin(KEY2_PORT,    KEY2_PIN);    }
/* SENS_UP 읽기 */
uint8_t io_read_sens_up(void)    { return (uint8_t)HAL_GPIO_ReadPin(SENS_UP_PORT, SENS_UP_PIN); }
/* SENS_DN 읽기 */
uint8_t io_read_sens_dn(void)    { return (uint8_t)HAL_GPIO_ReadPin(SENS_DN_PORT, SENS_DN_PIN); }
/* overload 입력 읽기 */
uint8_t io_read_overload(void)   { return (uint8_t)HAL_GPIO_ReadPin(OVLD_IN_PORT, OVLD_IN_PIN); }
/* US 피드백 읽기 */
uint8_t io_read_usfb(void)       { return (uint8_t)HAL_GPIO_ReadPin(USFB_PORT,    USFB_PIN);    }

/* USOUT 출력 설정 */
void io_usout(bool on)      { HAL_GPIO_WritePin(USOUT_PORT,    USOUT_PIN,    on ? GPIO_PIN_SET : GPIO_PIN_RESET); }
/* OVLD 릴레이 설정 */
void io_ovld_relay(bool on) { HAL_GPIO_WritePin(OVLD_RLY_PORT, OVLD_RLY_PIN, on ? GPIO_PIN_SET : GPIO_PIN_RESET); }
/* 부저 출력 설정 */
void io_buzzer(bool on)     { HAL_GPIO_WritePin(BUZZER_PORT,   BUZZER_PIN,   on ? GPIO_PIN_SET : GPIO_PIN_RESET); }
/* SOL_DN 출력 설정 */
void io_sol_dn(bool on)     { HAL_GPIO_WritePin(SOL_DN_PORT,   SOL_DN_PIN,   on ? GPIO_PIN_RESET : GPIO_PIN_SET); } /* active-LOW */
