/*
 * dgus_lcd.c
 *
 * Created: 2021-11-25 오전 11:44:07
 *  Author: tknoh
 */ 
#include <asf.h>
#include <string.h>
#include <stdio.h>

#include "dgus_lcd.h"

struct usart_module usart_LCD_instance;

#define MAX_RX_BUFFER_LENGTH   120
#define MAX_TX_BUFFER_LENGTH   50

volatile uint8_t rx_buffer_LCD[MAX_RX_BUFFER_LENGTH];
volatile uint8_t tx_buffer_LCD[MAX_TX_BUFFER_LENGTH];
volatile uint8_t rx_temp_byte;
static volatile bool lcd_xmit_data_done;
uint8_t lcd_rxbuffer_h, lcd_rxbuffer_t;
uint8_t t_ch0, t_ch1, in_uart_comm, lcd_comm_length, lcd_comm_cnt, comm_tick, temp_buf[MAX_TX_BUFFER_LENGTH], send_buf[MAX_TX_BUFFER_LENGTH];
uint8_t lcd_buf[MAX_LCD_BUFFER_LENGTH]; 

void usart_read_callback_LCD(const struct usart_module *const usart_module)
{
	usart_read_job(&usart_LCD_instance, &rx_temp_byte);
	rx_buffer_LCD[lcd_rxbuffer_h] = rx_temp_byte;
	lcd_rxbuffer_h++;
	if(lcd_rxbuffer_h == MAX_RX_BUFFER_LENGTH) {lcd_rxbuffer_h = 0;}
}

void usart_write_callback_LCD(const struct usart_module *const usart_module)
{
	lcd_xmit_data_done = true;
	//	LED_Toggle(LED_IND);
}

void configure_usart_LCD(void)
{
	struct usart_config config_usart;
	usart_get_config_defaults(&config_usart);

	config_usart.generator_source = GCLK_GENERATOR_3;
	config_usart.baudrate    = 115200;
	config_usart.mux_setting = USART_LCD_SERCOM_MUX_SETTING;
	config_usart.pinmux_pad0 = USART_LCD_SERCOM_PINMUX_PAD0;
	config_usart.pinmux_pad1 = USART_LCD_SERCOM_PINMUX_PAD1;
	config_usart.pinmux_pad2 = USART_LCD_SERCOM_PINMUX_PAD2;
	config_usart.pinmux_pad3 = USART_LCD_SERCOM_PINMUX_PAD3;

	while (usart_init(&usart_LCD_instance, USART_LCD_MODULE, &config_usart) != STATUS_OK) {
	}

	usart_enable(&usart_LCD_instance);
	usart_read_job(&usart_LCD_instance, &rx_temp_byte);
}

void configure_usart_callbacks_LCD(void)
{
	usart_register_callback(&usart_LCD_instance,usart_write_callback_LCD, USART_CALLBACK_BUFFER_TRANSMITTED);
	usart_register_callback(&usart_LCD_instance,usart_read_callback_LCD, USART_CALLBACK_BUFFER_RECEIVED);

	usart_enable_callback(&usart_LCD_instance, USART_CALLBACK_BUFFER_TRANSMITTED);
	usart_enable_callback(&usart_LCD_instance, USART_CALLBACK_BUFFER_RECEIVED);
}

void init_lcd_comm(void)
{
	configure_usart_LCD();
	configure_usart_callbacks_LCD();
	lcd_rxbuffer_h = lcd_rxbuffer_t = 0;
	in_uart_comm = lcd_comm_length = lcd_comm_cnt = comm_tick = 0; 
	lcd_xmit_data_done = true;
}

static void chk_uart_empty(void)
{
	//	while (!(LPC_UART->LSR & LSR_TEMT));		// TEMP : 1 : EMPTY, 0 : CONTAINS VALID DATA
	while (usart_get_job_status(&usart_LCD_instance, USART_TRANSCEIVER_TX) == STATUS_BUSY);
	
}

bool check_tx_done(void)
{
	if (lcd_xmit_data_done)
	{
		chk_uart_empty();
		return true;
	}
	return false;
}

void reset_dgus_lcd(void)
{
	while(false == check_tx_done());
	lcd_xmit_data_done = false;

	tx_buffer_LCD[0] = SYNC1;
	tx_buffer_LCD[1] = SYNC2;
	tx_buffer_LCD[2] = 7;		// 3+ 18data
	tx_buffer_LCD[3] = LCD_WR;
	tx_buffer_LCD[4] = 0;
	tx_buffer_LCD[5] = VP_LCD_RESET;	// start_addr
	tx_buffer_LCD[6] = 0x55;
	tx_buffer_LCD[7] = 0xaa;
	tx_buffer_LCD[8] = 0x5a;
	tx_buffer_LCD[9] = 0xa5;

	usart_write_buffer_job(&usart_LCD_instance, tx_buffer_LCD, 10);
	/*

	send_buf[0] = SYNC1;
	send_buf[1] = SYNC2;
	send_buf[2] = 18;		// 3+ 18data
	send_buf[3] = LCD_WR;
	send_buf[4] = 0;
	send_buf[5] = VP_CH1_0; // start_addr
	conv.c_int = adc_result[0][0];
	send_buf[6] = conv.c_byte[3];
	send_buf[7] = conv.c_byte[2];
	send_buf[8] = conv.c_byte[1];
	send_buf[9] = conv.c_byte[0];
	conv.c_int = adc_result[0][1];
	send_buf[10] = conv.c_byte[3];
	send_buf[11] = conv.c_byte[2];
	send_buf[12] = conv.c_byte[1];
	send_buf[13] = conv.c_byte[0];
	conv.c_int = adc_result[0][2];
	send_buf[14] = conv.c_byte[3];
	send_buf[15] = conv.c_byte[2];
	send_buf[16] = conv.c_byte[1];
	send_buf[17] = conv.c_byte[0];
	conv.c_int = adc_result[0][3];
	send_buf[18] = conv.c_byte[3];
	send_buf[19] = conv.c_byte[2];
	send_buf[20] = conv.c_byte[1];
	send_buf[21] = conv.c_byte[0];
	for(i = 0 ; i < 21 ; i++) PutChar(send_buf[i]);
	*/
	
}

void set_lcd_page(uint8_t lcd_page)
{
	while(false == check_tx_done());
	lcd_xmit_data_done = false;

	tx_buffer_LCD[0] = SYNC1;
	tx_buffer_LCD[1] = SYNC2;
	tx_buffer_LCD[2] = 7;		// 3+ 18data
	tx_buffer_LCD[3] = LCD_WR;
	tx_buffer_LCD[4] = 0;
	tx_buffer_LCD[5] = VP_LCD_SETPAGE;	// start_addr
	tx_buffer_LCD[6] = 0x5a;
	tx_buffer_LCD[7] = 0x01;
	tx_buffer_LCD[8] = 0x00;
	tx_buffer_LCD[9] = lcd_page;

	usart_write_buffer_job(&usart_LCD_instance, tx_buffer_LCD, 10);
}

void read_system_var(uint8_t var)
{
	while(false == check_tx_done());
	lcd_xmit_data_done = false;

	tx_buffer_LCD[0] = SYNC1;
	tx_buffer_LCD[1] = SYNC2;
	tx_buffer_LCD[2] = 4;		// 3+ 18data
	tx_buffer_LCD[3] = LCD_RD;
	tx_buffer_LCD[4] = 0;
	tx_buffer_LCD[5] = var;	// start_addr
	tx_buffer_LCD[6] = 1;

	usart_write_buffer_job(&usart_LCD_instance, tx_buffer_LCD, 7);
}

void send_lcd_data_var(uint16_t vp_addr, uint16_t vp_data)
{
	while(false == check_tx_done());
	lcd_xmit_data_done = false;

	tx_buffer_LCD[0] = SYNC1;
	tx_buffer_LCD[1] = SYNC2;
	tx_buffer_LCD[2] = 5;		// 3+ 18data
	tx_buffer_LCD[3] = LCD_WR;
	tx_buffer_LCD[4] = MSB(vp_addr);
	tx_buffer_LCD[5] = LSB(vp_addr);	// start_addr
	tx_buffer_LCD[6] = MSB(vp_data);
	tx_buffer_LCD[7] = LSB(vp_data);

	usart_write_buffer_job(&usart_LCD_instance, tx_buffer_LCD, 8);
}

void send_lcd_data_word(uint16_t vp_addr, uint32_t vp_data)
{

	while(false == check_tx_done());
	lcd_xmit_data_done = false;

	tx_buffer_LCD[0] = SYNC1;
	tx_buffer_LCD[1] = SYNC2;
	tx_buffer_LCD[2] = 7;		// 3+ 18data
	tx_buffer_LCD[3] = LCD_WR;
	tx_buffer_LCD[4] = MSB(vp_addr);
	tx_buffer_LCD[5] = LSB(vp_addr);	// start_addr
	tx_buffer_LCD[6] = MSB0W(vp_data);
	tx_buffer_LCD[7] = MSB1W(vp_data);
	tx_buffer_LCD[8] = MSB2W(vp_data);
	tx_buffer_LCD[9] = MSB3W(vp_data);

	usart_write_buffer_job(&usart_LCD_instance, tx_buffer_LCD, 10);
}

void send_lcd_byte_array(uint16_t vp_addr, uint8_t length, uint8_t * byte_array)
{
	uint8_t i;
	
	while(false == check_tx_done());
	lcd_xmit_data_done = false;

	tx_buffer_LCD[0] = SYNC1;
	tx_buffer_LCD[1] = SYNC2;
	tx_buffer_LCD[2] = length+3;		// 3+ 18data
	tx_buffer_LCD[3] = LCD_WR;
	tx_buffer_LCD[4] = MSB(vp_addr);
	tx_buffer_LCD[5] = LSB(vp_addr);	// start_addr
	for(i = 0 ; i < length ; i++)
		tx_buffer_LCD[6+i] = byte_array[i];
	
	usart_write_buffer_job(&usart_LCD_instance, tx_buffer_LCD, 6+length);
}

void send_lcd_int_array(uint16_t vp_addr, uint8_t length, uint16_t * int_array)
{
	uint8_t i;
	
	while(false == check_tx_done());
	lcd_xmit_data_done = false;

	tx_buffer_LCD[0] = SYNC1;
	tx_buffer_LCD[1] = SYNC2;
	tx_buffer_LCD[2] = (length * 2)+3;		// 3+ 18data
	tx_buffer_LCD[3] = LCD_WR;
	tx_buffer_LCD[4] = MSB(vp_addr);
	tx_buffer_LCD[5] = LSB(vp_addr);	// start_addr
	for(i = 0 ; i < length ; i++)
	{
		tx_buffer_LCD[6+(i*2)] = MSB(int_array[i]);
		tx_buffer_LCD[7+(i*2)] = LSB(int_array[i]);
	}
	usart_write_buffer_job(&usart_LCD_instance, tx_buffer_LCD, 6+(length*2));
}

void send_lcd_txt(uint16_t vp_addr, uint8_t * ptxt)
{
	uint8_t i;
	
	while(false == check_tx_done());
	lcd_xmit_data_done = false;

	tx_buffer_LCD[0] = SYNC1;
	tx_buffer_LCD[1] = SYNC2;
	tx_buffer_LCD[3] = LCD_WR;
	tx_buffer_LCD[4] = MSB(vp_addr);
	tx_buffer_LCD[5] = LSB(vp_addr);	// start_addr
	for(i = 0 ; i < 10 ; i++)
		tx_buffer_LCD[6+i] = 0;
		
	for(i = 0 ; i < 10 ; i++)
	{
		tx_buffer_LCD[6+i] = ptxt[i];
		if(ptxt[i] == '\0') {
			if(i == 0) return;
			break;
		}
	}
	tx_buffer_LCD[2] = 13;		// 3+ 18data

	usart_write_buffer_job(&usart_LCD_instance, tx_buffer_LCD, 16);
}


unsigned char check_lcd_comm(uint8_t mode)
{
	unsigned char dummy, i;

	while(1) {
		
		if(lcd_rxbuffer_h != lcd_rxbuffer_t)
		{
			t_ch0 = rx_buffer_LCD[lcd_rxbuffer_t++];
			if(lcd_rxbuffer_t == MAX_RX_BUFFER_LENGTH) {lcd_rxbuffer_t = 0;}
			if(in_uart_comm == 0){
				if((t_ch0 == SYNC2)&&(t_ch1 == SYNC1)){
					in_uart_comm = 1;
					lcd_comm_length = 6;		
					lcd_comm_cnt = 0;	
					comm_tick = 0;		
				} 
				else 
				{
					t_ch1 = t_ch0;
				}
			}
			else
			{
				temp_buf[lcd_comm_cnt] = t_ch0;
				if(lcd_comm_cnt == LCD_COMM_N)
				{
					lcd_comm_length = t_ch0;
				} 
				else
				{
					if(lcd_comm_cnt == lcd_comm_length)
					{
						lcd_comm_cnt = 0;
						in_uart_comm = 0;
						
						if((mode == 1) && (temp_buf[LCD_COMM_MODE] == LCD_WR))
							continue;
						
						for(i = 0 ; i <= lcd_comm_length ; i++)
						{
							lcd_buf[i] = temp_buf[i];
						}
						return 1;
					}
				}
				lcd_comm_cnt++;
				if(++comm_tick > 20){	// time out
					lcd_comm_cnt = 0;
					in_uart_comm = 0;
				}
			}
		}
		else
			return 0;
	}
}

/*
void pharse_lcd_comm(void)
{
	unsigned int temp_com;
	unsigned char i;

	if(lcd_buf[LCD_COMM_MODE] == LCD_RD){
		if(wait_reply == YES){
			//			get_ref_data();
			wait_reply = NO;
			return;
		}
		
		if(lcd_buf[LCD_COMM_N] != 6) return;
		conv.c_byte[0] = lcd_buf[LCD_COMM_ADDR_L];
		conv.c_byte[1] = lcd_buf[LCD_COMM_ADDR_H];
		temp_com = conv.c_int;
		
		if(temp_com == VP_KEY_CONF){				// into configuration
			if(lcd_buf[LCD_COMM_DATA_L] == ON){	// On
				mode = MODE_CONFIG;
				send_limit_data();
			}
			} else if(temp_com == VP_KEY_QUIT){				// Humidifier control
			if(lcd_buf[LCD_COMM_DATA_L] == KEY_SAVE){	// On
				save_ref_val();
				} else {							// Off
			}
			mode = MODE_RUN;
			send_adc_data();
			} else if(temp_com == VP_KEY_IIN_MIN){				// Humidifier control
			if(lcd_buf[LCD_COMM_DATA_L] == KEY_DN){	// Down
				if(limit_min[LIMIT_IIN] > 10 ) limit_min[LIMIT_IIN] = limit_min[LIMIT_IIN] - 10 ;
				} else {							// Dn
				if(limit_min[LIMIT_IIN] < 29990 ) limit_min[LIMIT_IIN] = limit_min[LIMIT_IIN] + 10 ;
			}
			send_buf[0] = SYNC1;
			send_buf[1] = SYNC2;
			send_buf[2] = 5;		// 3+ 18data
			send_buf[3] = LCD_WR;
			send_buf[4] = (unsigned char)((VP_MIN_IIN) >> 8);
			send_buf[5] = (unsigned char)(VP_MIN_IIN); // start_addr
			conv.c_int = limit_min[LIMIT_IIN];
			send_buf[6] = conv.c_byte[1];
			send_buf[7] = conv.c_byte[0];
			wdt_reset();
			for(i = 0 ; i < 8 ; i++) PutChar(send_buf[i]);
			} else if(temp_com == VP_KEY_226_MIN){				// Humidifier control
			if(lcd_buf[LCD_COMM_DATA_L] == KEY_DN){	// Down
				if(limit_min[LIMIT_226] > 10 ) limit_min[LIMIT_226] = limit_min[LIMIT_226] - 10 ;
				} else {							// Dn
				if(limit_min[LIMIT_226] < 29990 ) limit_min[LIMIT_226] = limit_min[LIMIT_226] + 10 ;
			}
			send_buf[0] = SYNC1;
			send_buf[1] = SYNC2;
			send_buf[2] = 5;		// 3+ 18data
			send_buf[3] = LCD_WR;
			send_buf[4] = (unsigned char)((VP_MIN_226) >> 8);
			send_buf[5] = (unsigned char)(VP_MIN_226); // start_addr
			conv.c_int = limit_min[LIMIT_226];
			send_buf[6] = conv.c_byte[1];
			send_buf[7] = conv.c_byte[0];
			wdt_reset();
			for(i = 0 ; i < 8 ; i++) PutChar(send_buf[i]);
			}else if(temp_com == VP_KEY_8_MIN){				// Humidifier control
			if(lcd_buf[LCD_COMM_DATA_L] == KEY_DN){	// Down
				if(limit_min[LIMIT_8] > 10 ) limit_min[LIMIT_8] = limit_min[LIMIT_8] - 10 ;
				} else {							// Dn
				if(limit_min[LIMIT_8] < 29990 ) limit_min[LIMIT_8] = limit_min[LIMIT_8] + 10 ;
			}
			send_buf[0] = SYNC1;
			send_buf[1] = SYNC2;
			send_buf[2] = 5;		// 3+ 18data
			send_buf[3] = LCD_WR;
			send_buf[4] = (unsigned char)((VP_MIN_8) >> 8);
			send_buf[5] = (unsigned char)(VP_MIN_8); // start_addr
			conv.c_int = limit_min[LIMIT_8];
			send_buf[6] = conv.c_byte[1];
			send_buf[7] = conv.c_byte[0];
			wdt_reset();
			for(i = 0 ; i < 8 ; i++) PutChar(send_buf[i]);
			}else if(temp_com == VP_KEY_5_MIN){				// Humidifier control
			if(lcd_buf[LCD_COMM_DATA_L] == KEY_DN){	// Down
				if(limit_min[LIMIT_5] > 10 ) limit_min[LIMIT_5] = limit_min[LIMIT_5] - 10 ;
				} else {							// Dn
				if(limit_min[LIMIT_5] < 29990 ) limit_min[LIMIT_5] = limit_min[LIMIT_5] + 10 ;
			}
			send_buf[0] = SYNC1;
			send_buf[1] = SYNC2;
			send_buf[2] = 5;		// 3+ 18data
			send_buf[3] = LCD_WR;
			send_buf[4] = (unsigned char)((VP_MIN_5) >> 8);
			send_buf[5] = (unsigned char)(VP_MIN_5); // start_addr
			conv.c_int = limit_min[LIMIT_5];
			send_buf[6] = conv.c_byte[1];
			send_buf[7] = conv.c_byte[0];
			wdt_reset();
			for(i = 0 ; i < 8 ; i++) PutChar(send_buf[i]);
			} else if(temp_com == VP_KEY_IIN_MAX){				// Humidifier control
			if(lcd_buf[LCD_COMM_DATA_L] == KEY_DN){	// Down
				if(limit_max[LIMIT_IIN] > 10 ) limit_max[LIMIT_IIN] = limit_max[LIMIT_IIN] - 10 ;
				} else {							// Dn
				if(limit_max[LIMIT_IIN] < 29990 ) limit_max[LIMIT_IIN] = limit_max[LIMIT_IIN] + 10 ;
			}
			send_buf[0] = SYNC1;
			send_buf[1] = SYNC2;
			send_buf[2] = 5;		// 3+ 18data
			send_buf[3] = LCD_WR;
			send_buf[4] = (unsigned char)((VP_MAX_IIN) >> 8);
			send_buf[5] = (unsigned char)(VP_MAX_IIN); // start_addr
			conv.c_int = limit_max[LIMIT_IIN];
			send_buf[6] = conv.c_byte[1];
			send_buf[7] = conv.c_byte[0];
			wdt_reset();
			for(i = 0 ; i < 8 ; i++) PutChar(send_buf[i]);
			} else if(temp_com == VP_KEY_226_MAX){				// Humidifier control
			if(lcd_buf[LCD_COMM_DATA_L] == KEY_DN){	// Down
				if(limit_max[LIMIT_226] > 10 ) limit_max[LIMIT_226] = limit_max[LIMIT_226] - 10 ;
				} else {							// Dn
				if(limit_max[LIMIT_226] < 29990 ) limit_max[LIMIT_226] = limit_max[LIMIT_226] + 10 ;
			}
			send_buf[0] = SYNC1;
			send_buf[1] = SYNC2;
			send_buf[2] = 5;		// 3+ 18data
			send_buf[3] = LCD_WR;
			send_buf[4] = (unsigned char)((VP_MAX_226) >> 8);
			send_buf[5] = (unsigned char)(VP_MAX_226); // start_addr
			conv.c_int = limit_max[LIMIT_226];
			send_buf[6] = conv.c_byte[1];
			send_buf[7] = conv.c_byte[0];
			wdt_reset();
			for(i = 0 ; i < 8 ; i++) PutChar(send_buf[i]);
			}else if(temp_com == VP_KEY_8_MAX){				// Humidifier control
			if(lcd_buf[LCD_COMM_DATA_L] == KEY_DN){	// Down
				if(limit_max[LIMIT_8] > 10 ) limit_max[LIMIT_8] = limit_max[LIMIT_8] - 10 ;
				} else {							// Dn
				if(limit_max[LIMIT_8] < 29990 ) limit_max[LIMIT_8] = limit_max[LIMIT_8] + 10 ;
			}
			send_buf[0] = SYNC1;
			send_buf[1] = SYNC2;
			send_buf[2] = 5;		// 3+ 18data
			send_buf[3] = LCD_WR;
			send_buf[4] = (unsigned char)((VP_MAX_8) >> 8);
			send_buf[5] = (unsigned char)(VP_MAX_8); // start_addr
			conv.c_int = limit_max[LIMIT_8];
			send_buf[6] = conv.c_byte[1];
			send_buf[7] = conv.c_byte[0];
			wdt_reset();
			for(i = 0 ; i < 8 ; i++) PutChar(send_buf[i]);
			}else if(temp_com == VP_KEY_5_MAX){				// Humidifier control
			if(lcd_buf[LCD_COMM_DATA_L] == KEY_DN){	// Down
				if(limit_max[LIMIT_5] > 10 ) limit_max[LIMIT_5] = limit_max[LIMIT_5] - 10 ;
				} else {							// Dn
				if(limit_max[LIMIT_5] < 29990 ) limit_max[LIMIT_5] = limit_max[LIMIT_5] + 10 ;
			}
			send_buf[0] = SYNC1;
			send_buf[1] = SYNC2;
			send_buf[2] = 5;		// 3+ 18data
			send_buf[3] = LCD_WR;
			send_buf[4] = (unsigned char)((VP_MAX_5) >> 8);
			send_buf[5] = (unsigned char)(VP_MAX_5); // start_addr
			conv.c_int = limit_max[LIMIT_5];
			send_buf[6] = conv.c_byte[1];
			send_buf[7] = conv.c_byte[0];
			wdt_reset();
			for(i = 0 ; i < 8 ; i++) PutChar(send_buf[i]);
			}else if(temp_com == VP_KEY_DELAY){				// Humidifier control
			if(lcd_buf[LCD_COMM_DATA_L] == KEY_DN){	// Down
				if(delay > 20 ) delay = delay - 10 ;
				} else {							// Dn
				if(delay < 240 ) delay = delay + 10 ;
			}
			send_buf[0] = SYNC1;
			send_buf[1] = SYNC2;
			send_buf[2] = 5;		// 3+ 18data
			send_buf[3] = LCD_WR;
			send_buf[4] = (unsigned char)((VP_DELAY) >> 8);
			send_buf[5] = (unsigned char)(VP_DELAY); // start_addr
			send_buf[6] = 0;
			send_buf[7] = delay;
			wdt_reset();
			for(i = 0 ; i < 8 ; i++) PutChar(send_buf[i]);
		}
		
	}
}
*/
