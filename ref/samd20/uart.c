/*
 * uart.c
 *
 * Created: 2016-11-09 오후 5:49:41
 *  Author: ajun_pc
 */ 
#include <asf.h>
#include <string.h>
#include <stdlib.h>
#include "define.h"
#include "uart.h"

// ========================= ETC ====================================
void my_itoa32(int32_t num, uint8_t *pStr);


// ========================= USART ====================================
struct usart_module usart_instance_mon, usart_instance_lcd;

#define MAX_RX_BUFFER_LENGTH   1

volatile uint8_t rx_buffer_lcd[MAX_RX_BUFFER_LENGTH];
volatile uint8_t rx_buffer_mon[MAX_RX_BUFFER_LENGTH];

uint16_t rx_data_lcd;
uint16_t rx_data_mon;


uint8_t my_str[16];

uint8_t command_txbuffer[MAX_CMD_LENGTH];


/*******************************************************************************
* Local functions
*******************************************************************************/

void my_itoa32(int32_t num, uint8_t *pStr)
{
	int16_t radix = 10;
	int32_t deg = 1;
	int16_t cnt = 0;
	int16_t i;

	if(pStr == NULL) return ;

	if(num < 0){
		*pStr = '-';
		num *= -1;
		pStr++;
	}

	while(1){
		if( (num / deg) > 0)
		cnt++;
		else
		break;

		deg *= radix;
	}
	deg /= radix;

	if(num != 0) {
		for(i = 0; i < cnt; i++, pStr++){
			*pStr    = (num / deg) + '0';
			num        -= (num / deg) * deg;
			deg        /= radix;
		}
	}
	else {
		*pStr = '0';
		pStr++;
	}
	*pStr = '\0';
}

// ========================= USART =================================================================================
void usart_read_callback_mon(const struct usart_module *const usart_module)
{
	usart_read_job(&usart_instance_mon, &rx_data_mon);
}

void usart_write_callback_mon(const struct usart_module *const usart_module)
{
}

void configure_usart_mon(void)
{
	struct usart_config config_usart;
	usart_get_config_defaults(&config_usart);

	config_usart.generator_source = GCLK_GENERATOR_3;
	config_usart.baudrate    = 115200;
	config_usart.mux_setting = USART_MON_SERCOM_MUX_SETTING;
	config_usart.pinmux_pad0 = USART_MON_SERCOM_PINMUX_PAD0;
	config_usart.pinmux_pad1 = USART_MON_SERCOM_PINMUX_PAD1;
	config_usart.pinmux_pad2 = USART_MON_SERCOM_PINMUX_PAD2;
	config_usart.pinmux_pad3 = USART_MON_SERCOM_PINMUX_PAD3;

	while (usart_init(&usart_instance_mon,USART_MON_MODULE, &config_usart) != STATUS_OK) {
	}

	usart_enable(&usart_instance_mon);
	usart_read_job(&usart_instance_mon, &rx_data_mon);

}

void configure_usart_callbacks_mon(void)
{
	usart_register_callback(&usart_instance_mon,usart_write_callback_mon, USART_CALLBACK_BUFFER_TRANSMITTED);
	usart_register_callback(&usart_instance_mon,usart_read_callback_mon, USART_CALLBACK_BUFFER_RECEIVED);
	usart_enable_callback(&usart_instance_mon, USART_CALLBACK_BUFFER_TRANSMITTED);
	usart_enable_callback(&usart_instance_mon, USART_CALLBACK_BUFFER_RECEIVED);
}
// ===============================================================================================================

void command_out(int32_t var1, int32_t var2)
{
	command_txbuffer[0] = 0;
	strcat((char *)command_txbuffer,(char *)"iset ");
	my_itoa32(var1,my_str);		strcat((char *)command_txbuffer,(char *)my_str);	strcat((char *)command_txbuffer,(char *)" ");
	my_itoa32(var2,my_str);		strcat((char *)command_txbuffer,(char *)my_str);	strcat((char *)command_txbuffer,(char *)CRLF);
	usart_write_buffer_job(&usart_instance_mon, command_txbuffer, strlen((char *)command_txbuffer));
}


/* EOF */
