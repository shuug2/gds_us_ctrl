/**
 * \file
 *
 * \brief User board definition template
 *
 */

 /* This file is intended to contain definitions and configuration details for
 * features and devices that are available on the board, e.g., frequency and
 * startup time for an external crystal, external memory devices, LED and USART
 * pins.
 */
/*
 * Support and FAQ: visit <a href="http://www.atmel.com/design-support/">Atmel Support</a>
 */

#ifndef USER_BOARD_H
#define USER_BOARD_H

#include <conf_board.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \ingroup group_common_boards
 * \defgroup user_board_group User board
 *
 * @{
 */

void system_board_init(void);

/** Name string macro */
#define BOARD_NAME                "USW_CTRL_BOARD"

#define BOARD_FREQ_SLCK_XTAL      0 /* Not Mounted */
#define BOARD_FREQ_SLCK_BYPASS    0 /* Not Mounted */
#define BOARD_FREQ_MAINCK_XTAL    (16000000U)
#define BOARD_FREQ_MAINCK_BYPASS  (16000000U)
#define BOARD_MCK                 48000000UL
#define BOARD_OSC_STARTUP_US      15625
/** @} */

/** \name LED0 definitions
 *  @{ */
	 
// Output	 
//#define BUZZER				PIN_PA28
#define BUZZER				PIN_PB30

//#define LED_RUN				PIN_PB03
//#define LED_ALARM			PIN_PA02
//#define LED_DELAY			PIN_PA03
//#define LED_TRIGGER			PIN_PB04

#define FND_COM0			PIN_PA22
#define FND_COM1			PIN_PA23
#define FND_COM2			PIN_PA24
#define FND_COM3			PIN_PA25
#define FND_COM4			PIN_PB22
#define FND_COM5			PIN_PB23
#define FND_COM6			PIN_PA27
#define FND_COM7			PIN_PA28
#define FND_D0				PIN_PA16
#define FND_D1				PIN_PA17
#define FND_D2				PIN_PA18
#define FND_D3				PIN_PA19
#define FND_D4				PIN_PB16
#define FND_D5				PIN_PB17
#define FND_D6				PIN_PA20
#define FND_D7				PIN_PA21

#define B_OVLD				PIN_PA08
#define B_USOUT				PIN_PA09
#define M_OVLD				PIN_PB10
#define M_SEEK				PIN_PB11
#define M_RESET				PIN_PB12
#define M_START				PIN_PB13

#define SOL_UP				PIN_PB31
#define SOL_DN				PIN_PB14



//Input
#define SW_CNT_RESET		PIN_PB05
#define SW_UP				PIN_PA04
#define SW_DOWN				PIN_PA05
#define SW_MODE				PIN_PA06
#define SW_SET				PIN_PA07
//#define CONF_START			PIN_PB30
//#define CONF_CYL_MODE		PIN_PB31

#define SW_START1			PIN_PB06
#define SW_START2			PIN_PB07
#define SW_EMSW				PIN_PB00

#define B_SEEK				PIN_PB00
#define B_RESET				PIN_PB01
#define B_START				PIN_PB02
#define M_OVLD				PIN_PB10

#define FREQ_IN				PIN_PB15

#define SENSE_UP			PIN_PA10
#define SENSE_DN			PIN_PA11

#define	BUZZER_ON			1
#define	BUZZER_OFF			0
#define	LED_ON				0
#define	LED_OFF				1
#define CTRL_ON				1
#define CTRL_INT_OFF			1
#define CTRL_INT_ON				0
#define CTRL_OFF			0
#define SOL_ON				0
#define SOL_OFF				1
#define FND_ON				1
#define FND_OFF				0

/** @} */

//#define USART_MON_MODULE              SERCOM2							// FOR DEBUG
//#define USART_MON_SERCOM_MUX_SETTING  USART_RX_1_TX_0_XCK_1
//#define USART_MON_SERCOM_PINMUX_PAD0  PINMUX_PA12C_SERCOM2_PAD0
//#define USART_MON_SERCOM_PINMUX_PAD1  PINMUX_PA13C_SERCOM2_PAD1
//#define USART_MON_SERCOM_PINMUX_PAD2  PINMUX_UNUSED
//#define USART_MON_SERCOM_PINMUX_PAD3  PINMUX_UNUSED

#define FC_MODULE           TC0
#define BZ_PWM_0_CHANNEL        0
#define BZ_PWM_0_PIN            PIN_PB30F_TC0_WO0
#define BZ_PWM_0_MUX            MUX_PB30F_TC0_WO0
#define BZ_PWM_0_PINMUX         PINMUX_PB30F_TC0_WO0

#define LED_HIGH(led_gpio)     port_pin_set_output_level(led_gpio,true)
#define LED_LOW(led_gpio)      port_pin_set_output_level(led_gpio,false)
#define LED_Toggle(led_gpio)  port_pin_toggle_output_level(led_gpio)


#ifdef __cplusplus
}
#endif

#endif // USER_BOARD_H
