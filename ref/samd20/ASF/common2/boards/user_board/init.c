/**
 * \file
 *
 * \brief User board initialization template
 *
 */
/*
 * Support and FAQ: visit <a href="http://www.atmel.com/design-support/">Atmel Support</a>
 */

#include <asf.h>
#include <board.h>
#include <conf_board.h>

#if defined(__GNUC__)
void board_init(void) WEAK __attribute__((alias("system_board_init")));
#elif defined(__ICCARM__)
void board_init(void);
#  pragma weak board_init=system_board_init
#endif

void system_board_init(void)
{
	struct port_config pin_conf;
	port_get_config_defaults(&pin_conf);

	/* Configure LEDs as outputs, turn them off */
	pin_conf.direction  = PORT_PIN_DIR_OUTPUT;
	port_pin_set_config(BUZZER, &pin_conf);
//	port_pin_set_config(LED_RUN, &pin_conf);
//	port_pin_set_config(LED_ALARM, &pin_conf);
//	port_pin_set_config(LED_DELAY, &pin_conf);
//	port_pin_set_config(LED_TRIGGER, &pin_conf);
	port_pin_set_config(FND_COM0, &pin_conf);
	port_pin_set_config(FND_COM1, &pin_conf);
	port_pin_set_config(FND_COM2, &pin_conf);
	port_pin_set_config(FND_COM3, &pin_conf);
	port_pin_set_config(FND_COM4, &pin_conf);
	port_pin_set_config(FND_COM5, &pin_conf);
	port_pin_set_config(FND_COM6, &pin_conf);
	port_pin_set_config(FND_COM7, &pin_conf);
	port_pin_set_config(FND_D0, &pin_conf);
	port_pin_set_config(FND_D1, &pin_conf);
	port_pin_set_config(FND_D2, &pin_conf);
	port_pin_set_config(FND_D3, &pin_conf);
	port_pin_set_config(FND_D4, &pin_conf);
	port_pin_set_config(FND_D5, &pin_conf);
	port_pin_set_config(FND_D6, &pin_conf);
	port_pin_set_config(FND_D7, &pin_conf);
	port_pin_set_config(B_OVLD, &pin_conf);
	port_pin_set_config(B_USOUT, &pin_conf);
	port_pin_set_config(M_SEEK, &pin_conf);
	port_pin_set_config(M_RESET, &pin_conf);
	port_pin_set_config(M_START, &pin_conf);
	port_pin_set_config(SOL_UP, &pin_conf);
	port_pin_set_config(SOL_DN, &pin_conf);
	port_pin_set_output_level(BUZZER, BUZZER_OFF);		// RX mode
//	port_pin_set_output_level(LED_RUN, LED_OFF);
//	port_pin_set_output_level(LED_ALARM, LED_OFF);
//	port_pin_set_output_level(LED_DELAY, LED_OFF);
//	port_pin_set_output_level(LED_TRIGGER, LED_OFF);
	port_pin_set_output_level(FND_COM0, FND_OFF);		//FND OFF
	port_pin_set_output_level(FND_COM1, FND_OFF);		//FND OFF
	port_pin_set_output_level(FND_COM2, FND_OFF);		//FND OFF
	port_pin_set_output_level(FND_COM3, FND_OFF);		//FND OFF
	port_pin_set_output_level(FND_COM4, FND_OFF);		//FND OFF
	port_pin_set_output_level(FND_COM5, FND_OFF);		//FND OFF
	port_pin_set_output_level(FND_COM6, FND_OFF);		//FND OFF
	port_pin_set_output_level(FND_D0, FND_OFF);
	port_pin_set_output_level(FND_D1, FND_OFF);
	port_pin_set_output_level(FND_D2, FND_OFF);
	port_pin_set_output_level(FND_D3, FND_OFF);
	port_pin_set_output_level(FND_D4, FND_OFF);
	port_pin_set_output_level(FND_D5, FND_OFF);
	port_pin_set_output_level(FND_D6, FND_OFF);
	port_pin_set_output_level(FND_D7, FND_OFF);
	port_pin_set_output_level(B_OVLD, CTRL_OFF);
	port_pin_set_output_level(B_USOUT, CTRL_OFF);
	port_pin_set_output_level(M_SEEK, CTRL_INT_OFF);
	port_pin_set_output_level(M_RESET, CTRL_INT_OFF);
	port_pin_set_output_level(M_START, CTRL_INT_OFF);
	port_pin_set_output_level(SOL_UP, SOL_OFF);
	port_pin_set_output_level(SOL_DN, SOL_OFF);

	pin_conf.direction  = PORT_PIN_DIR_INPUT;
	pin_conf.input_pull = PORT_PIN_PULL_UP;
	port_pin_set_config(SW_CNT_RESET, &pin_conf);
	port_pin_set_config(SW_UP, &pin_conf);
	port_pin_set_config(SW_DOWN, &pin_conf);
	port_pin_set_config(SW_MODE, &pin_conf);
	port_pin_set_config(SW_SET, &pin_conf);
	port_pin_set_config(FREQ_IN, &pin_conf);
	//port_pin_set_config(CONF_START, &pin_conf);
	//port_pin_set_config(CONF_CYL_MODE, &pin_conf);
	//port_pin_set_config(PIN_PB08, &pin_conf);		// pull-up i2c port
	//port_pin_set_config(PIN_PB09, &pin_conf);	// pull-up i2c port

	pin_conf.direction  = PORT_PIN_DIR_INPUT;
	pin_conf.input_pull = PORT_PIN_PULL_NONE;
	port_pin_set_config(SW_START1, &pin_conf);
	port_pin_set_config(SW_START2, &pin_conf);
	port_pin_set_config(SW_EMSW, &pin_conf);
	port_pin_set_config(B_RESET, &pin_conf);
	port_pin_set_config(B_START, &pin_conf);
	port_pin_set_config(M_OVLD, &pin_conf);
	port_pin_set_config(SENSE_UP, &pin_conf);
	port_pin_set_config(SENSE_DN, &pin_conf);
}