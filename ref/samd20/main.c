

/**
 *
 * main.c
 *
 * 
 **/

#include <asf.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "define.h"
#include "uart.h"
#include "i2c_comm.h"
#include "dgus_lcd.h" 
#include "modbus.h"

#include "loopback.h"
#include "ethernet.h"
#include "dhcp.h"
#include "process_tcp.h"

#define GDSONIC
//#define DIAMT
//#define POWERTECH
//#define MOOHAN
// ---------------------------------
void usart_read_callback_lcd(const struct usart_module *const usart_module);
void usart_write_callback_lcd(const struct usart_module *const usart_module);
void configure_usart_lcd(void);
void configure_usart_callbacks_lcd(void);
// ---------------------------------
void usart_read_callback_mon(const struct usart_module *const usart_module);
void usart_write_callback_mon(const struct usart_module *const usart_module);
void configure_usart_mon(void);

void configure_usart_callbacks_mon(void);
// ---------------------------------
#define SEG_ALARM	0x01
#define SEG_DELAY	0x02
#define SEG_TRRIGER	0x04
#define SEG_RUN		0x08
#define	SEG_d	11
#define SEG_h	12
#define SEG_e	13
#define SEG_E	14
#define SEG_o	17
#define SEG_t	18
#define SEG_n	19
#define SEG_f	20
#define SEG_s	21
#define SEG_a	22
#define SEG_u	23
#define SEG_p	24
#define SEG_r	25
#define SEG_DASH	16 
#define SEG_SPACE 10
uint8_t digit[26] = {0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x67,0x00,0x5e,0x74,0x7b,0x79,0x08, 0x40, 0x5c, 0x78, 0x54, 0x71, 0x6d, 0x77, 0x3e, 0x73, 0x50};	
						// 10(null), 11(d), 12(h), 13(e), 14(E), 15(_), 16(-), 17(o), 18(t), 19(n), 20(f), 21(s), 22(a), 23(u), 24(p), 25(r)
uint8_t aChar[7] = {10, 10, 10, 10, 10, 10, 10};
	
//uint8_t aChar[3] = {0,1,2};

uint8_t fnd_dig, fnd_status_data;
uint8_t point_ch;

uint8_t mode_blink;
uint8_t sw_mode_f;

#define DIGIT_LEN	8

// ========================= RTC ====================================
void rtc_overflow_callback(void);
void configure_rtc_count(void);
void configure_rtc_callbacks(void);

struct rtc_module rtc_instance;

void sys_tick_func(void);

// system variables


volatile uint8_t tick_1ms, base_cnt, tick_1ms_state, tick_10ms_state, bak_1ms_state, bak_10ms_state, sys_tick, key_tick, sys_tick_10m, event_cnt;
volatile uint8_t disp_cnt, max_disp_cnt, in_cycle;

uint8_t tick_1ms_bak;
uint8_t tick_10ms;
uint8_t tick_100ms;
uint8_t blink_cnt;
uint8_t tick_1s;
uint8_t tick_10s;
uint8_t tick_error;
uint8_t disp_timer;
uint8_t ctrl_cnt, max_ctrl_cnt, run_led_toggle_cnt, run_led_toggle_time, startup_time;

uint32_t unique_serial[4];

uint8_t error_str[32];

//---------
#define RESET_IDLE	0
#define RESET_ISSUE	1
#define SEEK_ISSUE	2

#define US_IDLE	0
#define US_REMOTE	1
#define US_TOUCH	2
#define US_COMM		3

#define ON	1
#define OFF	0

#define MAX_US_RESET_CNT 5		// x 100mS

#define USOUT_TH	25
#define USOUT_ERR_CNT_MAX	8

uint8_t model_freq, model_type, output_power, current_power, us_on_time_200m, max_us_reset_cnt;
uint16_t level_buf[20], out_time_buf[20];
uint8_t us_reset_mode, us_reset_cnt, us_run_status, us_reset_status, us_seek_status, us_on_status;
uint8_t sig_run_status, sig_reset_status, sig_seek_status, sig_ovld_status;
uint8_t bak_run_status, bak_reset_status, bak_seek_status, bak_ovld_status;
uint8_t multi_ctrl_stage;
int16_t cal_val, temp_cal_val, freq_cal_val, temp_freq_cal_val;

bool update_amp, update_freq, energy_ctrl, multi_ctrl;
uint8_t freq_cnt;
uint16_t freq_buf[10], curr_freq, curr_amp, curr_lv, curr_power, max_amp, max_power, limit_on_time;
uint16_t last_freq, last_amp, last_power, last_lv, us_on_time, usout_err_cnt;
uint16_t ref_lv_1, ref_lv_2, ref_lv_10, ref_lv_20;
uint32_t limit_energy, curr_energy, acc_energy, last_energy, last_time;

uint8_t temp_horndown, temp_cnt_reset;
uint8_t comm_speed_idx, comm_parity_idx, comm_address;
uint8_t temp_speed_idx, temp_parity_idx, temp_address;
uint8_t modbus_status, bak_modbus_status, modbus_comm_cnt;

uint8_t comm_mode, ether_ip[4], ether_nm[4], ether_gw[4];
uint8_t temp_comm_mode, temp_ether_ip[4], temp_ether_nm[4], temp_ether_gw[4];

uint8_t ether_what_input = 0;
uint8_t ether_current_number = 0;
uint8_t ether_current_octet = 0;
uint8_t ether_has_input = false;
uint8_t ether_ip_input_complete = false;
uint8_t ether_input_buffer[16], ether_temp_ip[4];
uint8_t ether_buffer_pos = 0;

uint8_t lcd_temp_buf[30];

uint8_t estop_msg[] = {"E-STOP\0"};
uint8_t ovld_msg[] = {"OVER LOAD\0"};
uint8_t outerr_msg[] = {"OUTPUT ERROR\0"};
uint8_t ovtime_msg[] = {"OVER TIME\0"};
uint8_t comm_speed_txt[6][6] = {{" 2400 "}, {" 4800 "}, {" 9600 "}, {"19200 "}, {"38400 "}, {"115200"}};
uint8_t comm_parity_txt[3][4] ={{"EVEN"}, {"ODD "}, {"NONE"}};
uint8_t fram_mode;


// ========================= TC ====================================
void configure_tc_FC(void);
void tc_callback_FC(struct tc_module *const module_inst);

struct tc_module tc_instance_FC;
struct events_resource extint_event;

int16_t bz_ctrl, bz_duty, bz_flag;
void tc_callback_FC(struct tc_module *const module_inst)
{
	freq_buf[freq_cnt] = tc_get_capture_value(&tc_instance_FC, TC_COMPARE_CAPTURE_CHANNEL_0);
	freq_cnt++;
	if(freq_cnt >= 10)
	{
		freq_cnt = 0;
		update_freq = true;
	}
}

void configure_tc_FC(void)
{
	struct tc_config config_tc;
	tc_get_config_defaults(&config_tc);

	config_tc.clock_source               = GCLK_GENERATOR_3;
	config_tc.counter_size    = TC_COUNTER_SIZE_16BIT;
	config_tc.clock_prescaler = TC_CLOCK_PRESCALER_DIV1;
	config_tc.enable_capture_on_channel[0] = true;

	tc_init(&tc_instance_FC, FC_MODULE, &config_tc);
	
	struct tc_events tc_event = {
		.generate_event_on_compare_channel[0] =	true,
		.generate_event_on_compare_channel[1] =	true,
		.on_event_perform_action = true,
		//.invert_event_input = true,
		.event_action = TC_EVENT_ACTION_PWP
	};
	
	tc_enable_events(&tc_instance_FC, &tc_event);
	events_attach_user(&extint_event, EVSYS_ID_USER_TC0_EVU);
	// Callback function for capture */
	tc_register_callback(&tc_instance_FC, tc_callback_FC, TC_CALLBACK_CC_CHANNEL0);
	tc_enable_callback(&tc_instance_FC, TC_CALLBACK_CC_CHANNEL0);	

	tc_enable(&tc_instance_FC);
}

/* Configure external interrupt controller */
void extint_configuration(void)
{
	struct extint_chan_conf extint_chan_config;
	
	extint_chan_get_config_defaults(&extint_chan_config);
	//extint_chan_config.gpio_pin = PIN_PA15A_EIC_EXTINT15;
	//extint_chan_config.gpio_pin_mux = MUX_PA15A_EIC_EXTINT15;
	extint_chan_config.gpio_pin = PIN_PB15A_EIC_EXTINT15;
	extint_chan_config.gpio_pin_mux = MUX_PB15A_EIC_EXTINT15;
	extint_chan_config.detection_criteria = EXTINT_DETECT_HIGH;
	extint_chan_set_config(15, &extint_chan_config);
	/* Configure external interrupt module to be event generator */
	struct extint_events extint_event_conf;
	extint_event_conf.generate_event_on_detect[15] = true;
	extint_enable_events(&extint_event_conf);
}

/* Configure event system: */
void evsys_configuration(void)
{
	struct events_config config;
	events_get_config_defaults(&config);
	config.generator = EVSYS_ID_GEN_EIC_EXTINT_15;
	config.edge_detect = EVENTS_EDGE_DETECT_BOTH;
	config.path = EVENTS_PATH_ASYNCHRONOUS;
	config.clock_source = GCLK_GENERATOR_3;
	events_allocate(&extint_event, &config);
}

void buzzer_on(void)
{
	port_pin_set_output_level(BUZZER, BUZZER_ON);

}

void buzzer_off(void)
{
	port_pin_set_output_level(BUZZER, BUZZER_OFF);
}

// ========================= ADC ====================================
void configure_adc(void);
void configure_adc_callbacks(void);
void adc_complete_callback(const struct adc_module *const module);
void adc_change_ch(void);
void start_adc(struct adc_module *const module_inst,int16_t *buffer,uint16_t samples);
enum status_code my_adc_read_buffer_job(struct adc_module *const module_inst,int16_t *buffer,uint16_t samples);
//void adc_port_gpio(void);
//void adc_port_adc(void);

#define ADC_CURR		0
#define ADC_OUT_LV		1

#define ADC_CHANNEL	2
#define ADC_SAMPLE	8

int16_t adc_result_buffer[ADC_CHANNEL][ADC_SAMPLE];
int16_t adc_gnd;
int16_t adc_iled[2];
int16_t adc_vled[2];
int16_t adc_cathode[2];

uint8_t adc_current_ch;
uint8_t adc_gain[2];
int16_t adc_offset[2][5];

struct adc_module adc_instance;

volatile bool adc_read_done = false;

void adc_complete_callback(const struct adc_module *const module)
{
//			LED0_HIGH;
	adc_read_done = true;
}

void configure_adc(void)
{
	struct adc_config config_adc;
	adc_get_config_defaults(&config_adc);

//	config_adc.clock_prescaler = ADC_CLOCK_PRESCALER_DIV128;
	config_adc.clock_source    = GCLK_GENERATOR_3;
	config_adc.clock_prescaler = ADC_CLOCK_PRESCALER_DIV64;
	config_adc.reference       = ADC_REFERENCE_INTVCC1;	// 3.3/1.48 = 2.23V
	config_adc.positive_input  = ADC_POSITIVE_INPUT_PIN0;
	config_adc.negative_input                = ADC_NEGATIVE_INPUT_GND ;
//	config_adc.negative_input                = ADC_NEGATIVE_INPUT_PIN0;
	config_adc.resolution      = ADC_RESOLUTION_CUSTOM;	
//	config_adc.accumulate_samples = ADC_ACCUMULATE_SAMPLES_8;
//	config_adc.accumulate_samples = ADC_ACCUMULATE_SAMPLES_2;
	config_adc.accumulate_samples = ADC_ACCUMULATE_SAMPLES_4;
	config_adc.divide_result	= ADC_DIVIDE_RESULT_DISABLE;
//	config_adc.differential_mode	= true;
	config_adc.differential_mode	= false;
//	config_adc.correction.offset_correction = 5;
	config_adc.correction.offset_correction = 0;
	config_adc.correction.gain_correction = 0x800;
//	config_adc.resolution      = ADC_RESOLUTION_14BIT;		
//	config_adc.resolution      = ADC_RESOLUTION_15BIT;		
//	config_adc.resolution      = ADC_RESOLUTION_16BIT;		
	config_adc.correction.correction_enable  = false;
//	config_adc.pin_scan.offset_start_scan    = 0;
//	config_adc.pin_scan.inputs_to_scan       = 3;

	adc_init(&adc_instance, ADC, &config_adc);

	adc_enable(&adc_instance);
}

void configure_adc_callbacks(void)
{
	adc_register_callback(&adc_instance, adc_complete_callback, ADC_CALLBACK_READ_BUFFER);
	adc_enable_callback(&adc_instance, ADC_CALLBACK_READ_BUFFER);
}

void start_adc(struct adc_module *const module_inst,int16_t *buffer,uint16_t samples)
{
	module_inst->job_status = STATUS_BUSY;
	module_inst->remaining_conversions = samples;
	module_inst->job_buffer = (uint16_t *)buffer;

	adc_enable_interrupt(module_inst, ADC_INTERRUPT_RESULT_READY);

	if(module_inst->software_trigger == true) {
		adc_start_conversion(module_inst);
	}
	
}

void adc_change_ch(void)
{
	switch(adc_current_ch) {
		case ADC_CURR : 
				adc_set_positive_input(&adc_instance,ADC_POSITIVE_INPUT_PIN0); 
				break;	// GND
		case ADC_OUT_LV : 
				adc_set_positive_input(&adc_instance,ADC_POSITIVE_INPUT_PIN11); 
				break;// I
	}
}

enum status_code my_adc_read_buffer_job(struct adc_module *const module_inst,int16_t *buffer,uint16_t samples)
{
	Assert(module_inst);
	Assert(samples);
	Assert(buffer);

	if(module_inst->remaining_conversions != 0 ||
			module_inst->job_status == STATUS_BUSY){
		return STATUS_BUSY;
	}

//	adc_current_ch++;
//	if(adc_current_ch == ADC_CHANNEL) { adc_current_ch = 0; }
//	adc_change_ch();
	
	return STATUS_OK;
}

uint16_t average(int16_t* array, uint16_t size) {
	uint32_t sum = 0;
	uint32_t result;
	uint16_t new_size;
	uint16_t max_val = 0;//0x0;
	uint16_t min_val = 0xffff;//0xffff;

	for (uint8_t i = 0; i < size; i++) {
		sum += array[i];
	}

	if(size > 2) {
		new_size = size - 2;
		for(uint16_t i=0;i<size;i++) {
			if(max_val < array[i]) max_val = array[i];
			if(min_val > array[i]) min_val = array[i];
		}
		sum -= max_val;
		sum -= min_val;
	}
	else new_size = size;

	result = sum / new_size;
	return ((uint16_t)result);
}

void cal_real_val(void)
{
	uint16_t temp, temp_val;
	temp = average(adc_result_buffer[adc_current_ch],ADC_SAMPLE);

//	switch(adc_current_ch) {
//		case ADC_CH0_I : temp = moving_avg(CH0, temp); break;
//		case ADC_CH1_I : temp = moving_avg(CH1, temp); break;
//	}


	switch(adc_current_ch) {
		case ADC_CURR : 	
//				temp_val = (temp > 45) ? temp - 45 : 0;
//				curr_amp = 	(temp_val) / 260; // calib. amp
//				curr_amp = 	temp_val / 5; // calib. amp
				temp_val = temp * 4 ;
#ifdef MOOHAN
				temp_val =  (temp_val / 10 ) + cal_val;
//				curr_amp = (temp_val > 40) ? (temp_val - 37) : 0;// B VALUE + OFFSET 15mA
				curr_amp = (temp_val > 51) ? (temp_val - 37) : 0;// B VALUE + OFFSET 15mA_
#else
				temp_val =  (temp_val / 10 ) + cal_val;
//				curr_amp = (temp_val > 40) ? (temp_val - 37) : 0;// B VALUE + OFFSET 15mA
				curr_amp = (temp_val > 51) ? (temp_val - 37) : 0;// B VALUE + OFFSET 15mA_
#endif
//				curr_amp = temp;
//				curr_amp = 	temp; // calib. amp
				if(curr_amp > max_amp) 
					max_amp = curr_amp;

				curr_power = (curr_amp * 22) / 10;
				if(curr_power > max_power) 
					max_power = curr_power;
				acc_energy += curr_power;		// energy
//				curr_energy = acc_energy / 25;	// every 200mS
				curr_energy = acc_energy / 500;	// every 1mS
				
				break;
		case ADC_OUT_LV :	curr_lv = temp;		
							break;
	}
}

// ========================= ETC ====================================
void var_init(void);
void configure_eeprom(void);
void write_eeprom(uint8_t page, uint16_t addr_l, uint16_t data);
uint16_t read_eeprom(uint8_t page, uint16_t addr_l);

void display_FND(void);
void check_sw_input(void);



uint8_t error_num, first_1sec;

uint8_t fnd_display_dimm;
uint8_t fnd_display_color;
uint8_t fnd_display_watt;
uint8_t fnd_display_error;

extern uint32_t		app_millisec_cnt;

extern uint16_t dmx_data_cnt, dmx_data_no;


extern uint8_t i2c_wr_buffer[i2c_W_DATA_LENGTH];
extern uint8_t i2c_rd_buffer[i2c_R_DATA_LENGTH];

uint8_t disp_buf[17], temp_lcd_buf[17];
uint8_t boot_status;
uint8_t data_edit, job_state, mon_updated;

uint16_t temp_edit_data, temp_edit_data_bak;
uint16_t user_data_no, confirm_no;

uint8_t sync_ok, sync_flag;

#define TOT_SW	5

uint8_t sw_up,   sw_up_bak,   sw_up_pressed;
uint8_t sw_down, sw_down_bak, sw_down_pressed;
uint8_t mode_key_hold, cnt_reset_key_hold, up_key_hold, dn_key_hold, set_key_hold;
uint8_t sw_mode, sw_mode_bak, sw_mode_pressed;
uint8_t sw_set, sw_set_bak, sw_set_pressed, set_key_f;
uint8_t sw_cnt_reset, sw_cnt_reset_bak, sw_cnt_reset_pressed;
uint8_t start_key_pressed, f_status_start;

uint8_t re_start1, re_start2, re_emsw, re_seek, re_reset, re_start, re_ovld, re_up, re_dn;
uint8_t re_start1_bak, re_start2_bak, re_emsw_bak, re_seek_bak, re_reset_bak, re_start_bak, re_ovld_bak, re_up_bak, re_dn_bak;
uint8_t re_start1_pressed, re_start2_pressed, re_emsw_issued, re_seek_pressed, re_reset_pressed, re_start_pressed, re_ovld_issued, re_outerr_issued, re_up_pressed, re_dn_pressed;

uint8_t sys_status, bak_sys_status, run_mode, sys_mode, lcd_status, run_status, error_status;
uint8_t setup_mode, setup_status;
uint8_t horn_status;

uint16_t limit_delay_time1, limit_delay_time2, limit_delay_time3, limit_trigger_time2, limit_trigger_time3, step_run_time, temp_time, comp_time;
uint16_t limit_mo_out1, limit_mo_out2, limit_mo_time1, limit_mo_time2, limit_out_time;
uint8_t f_safty;

uint32_t work_cnt;
unsigned int run_cnt;

// system mode
#define SYS_HAND	0
#define SYS_MULTI	1
#define SYS_STD		2
#define SYS_FND		3

//------- standard mode
// system mode
#define SYS_RUN		0
#define SYS_SETUP	1
#define SYS_HORN	2
#define SYS_ESTOP	3
#define SYS_ERROR	4

// run_mode 
#define MODE_TRIGGER	1
#define MODE_DELAY		0

// run_status 
#define RUN_READY	0
#define RUN_CYL1	1
#define RUN_WELD	2
#define RUN_HOLD	3
#define RUN_CYL2	4

// setup status
#define SETUP_DELAY	 0
#define SETUP_WELD	 1
#define SETUP_HOLD	 2
#define SETUP_SAFTY	 3



// ========================= RTC ===================================================================================
void rtc_overflow_callback(void)
{
	tick_1ms_state++;
	tick_1ms++;
	base_cnt++;
//	if(tick_1ms & 0x01) 	tick_1ms_state++;
	sys_tick++;
	if(sys_tick >= 10){
		event_cnt++;
		sys_tick = 0;
		sys_tick_10m++;
		tick_10ms_state++;
	}
//	app_millisec_cnt++;
//	LED_Toggle(TP_PIN);

}

void configure_rtc_count(void)
{
	struct rtc_count_config config_rtc_count;
	rtc_count_get_config_defaults(&config_rtc_count);

	config_rtc_count.prescaler           = RTC_COUNT_PRESCALER_DIV_1;
	config_rtc_count.mode                = RTC_COUNT_MODE_16BIT;
	#ifdef FEATURE_RTC_CONTINUOUSLY_UPDATED
	config_rtc_count.continuously_update = true;
	#endif
	rtc_count_init(&rtc_instance, RTC, &config_rtc_count);

	rtc_count_enable(&rtc_instance);
}

void configure_rtc_callbacks(void)
{
	rtc_count_register_callback(&rtc_instance, rtc_overflow_callback, RTC_COUNT_CALLBACK_OVERFLOW);
	rtc_count_enable_callback(&rtc_instance, RTC_COUNT_CALLBACK_OVERFLOW);
}

//-----------------------------------------------------------
uint16_t read_param_fram(uint16_t addr, uint8_t mode)
{
	uint16_t temp;

//	if(mode == 0)	//MR44V64
//	{
//		i2c_wr_buffer[0] = MSB(addr);
//		i2c_wr_buffer[1] = LSB(addr);
//		i2c_adc_read_block(I2C_FRAM, 2, 2);
//	} 
//	else 
//	{	// FM24C16
		i2c_wr_buffer[0] = LSB(addr);
		i2c_adc_read_block(I2C_FRAM, 1, 2);
//	}
	
	MSB(temp) = i2c_rd_buffer[0];
	LSB(temp) = i2c_rd_buffer[1];
	
	return (uint16_t)temp;
}

void save_param_fram(uint16_t addr, uint16_t data, uint8_t mode)
{
//	if(mode == 0)	//MR44V64
//	{
//		i2c_wr_buffer[0] = MSB(addr);
//		i2c_wr_buffer[1] = LSB(addr);
//		i2c_wr_buffer[2] = MSB(data);
//		i2c_wr_buffer[3] = LSB(data);
//		i2c_adc_write_block(I2C_FRAM, 2);
//	} 
//	else 
//	{	// FM24C16
		i2c_wr_buffer[0] = LSB(addr);
		i2c_wr_buffer[1] = MSB(data);
		i2c_wr_buffer[2] = LSB(data);
		i2c_adc_write_block(I2C_FRAM, 1);
//	}
	
}

uint8_t read_byte_fram(uint16_t addr, uint8_t mode)
{
	uint8_t temp;
	
//	if(mode == 0)	//MR44V64
//	{
//		i2c_wr_buffer[0] = MSB(addr);
//		i2c_wr_buffer[1] = LSB(addr);
//		i2c_adc_read_block(I2C_FRAM, 2, 1);
//	} 
//	else 
//	{	// FM24C16
		i2c_wr_buffer[0] = LSB(addr);
		i2c_adc_read_block(I2C_FRAM, 1, 1);
//	}
	
	
	temp = i2c_rd_buffer[0];

	return (uint8_t)temp;
}

void save_byte_fram(uint16_t addr, uint8_t data, uint8_t mode)
{
//	if(mode == 0)	//MR44V64
//	{
//		i2c_wr_buffer[0] = MSB(addr);
//		i2c_wr_buffer[1] = LSB(addr);
//		i2c_wr_buffer[2] = data;
//		i2c_adc_write_block(I2C_FRAM, 1);
//	} 
//	else 
//	{	// FM24C16
		i2c_wr_buffer[0] = LSB(addr);
		i2c_wr_buffer[1] = data;
		i2c_adc_write_block(I2C_FRAM, 0);
//	}
	
}

uint32_t read_word_fram(uint16_t addr, uint8_t mode)
{
	uint32_t temp;
	
//	if(mode == 0)	//MR44V64
//	{
//		i2c_wr_buffer[0] = MSB(addr);
//		i2c_wr_buffer[1] = LSB(addr);
//		i2c_adc_read_block(I2C_FRAM, 2, 4);
//	} 
//	else 
//	{	// FM24C16
		i2c_wr_buffer[0] = LSB(addr);
		i2c_adc_read_block(I2C_FRAM, 1, 4);
//	}
	
	
	MSB0W(temp) = i2c_rd_buffer[0];
	MSB1W(temp) = i2c_rd_buffer[1];
	MSB2W(temp) = i2c_rd_buffer[2];
	MSB3W(temp) = i2c_rd_buffer[3];
	
	return (uint32_t)temp;
}

void save_word_fram(uint16_t addr, uint32_t data, uint8_t mode)
{
//	if(mode == 0)	//MR44V64
//	{
//		i2c_wr_buffer[0] = MSB(addr);
//		i2c_wr_buffer[1] = LSB(addr);
//		i2c_wr_buffer[2] = MSB0W(data);
//		i2c_wr_buffer[3] = MSB1W(data);
//		i2c_wr_buffer[4] = MSB2W(data);
//		i2c_wr_buffer[5] = MSB3W(data);
//		i2c_adc_write_block(I2C_FRAM, 4);
//	} 
//	else 
//	{	// FM24C16
		i2c_wr_buffer[0] = LSB(addr);
		i2c_wr_buffer[1] = MSB0W(data);
		i2c_wr_buffer[2] = MSB1W(data);
		i2c_wr_buffer[3] = MSB2W(data);
		i2c_wr_buffer[4] = MSB3W(data);
		i2c_adc_write_block(I2C_FRAM, 3);
//	}
	
}

uint32_t read_4byte_fram(uint16_t addr, uint8_t *data)
{
	i2c_wr_buffer[0] = LSB(addr);
	i2c_adc_read_block(I2C_FRAM, 1, 4);
	
	
	data[0] = i2c_rd_buffer[0];
	data[1] = i2c_rd_buffer[1];
	data[2] = i2c_rd_buffer[2];
	data[3] = i2c_rd_buffer[3];
}

void save_4byte_fram(uint16_t addr, uint8_t *data)
{
		i2c_wr_buffer[0] = LSB(addr);
		i2c_wr_buffer[1] = data[0];
		i2c_wr_buffer[2] = data[1];
		i2c_wr_buffer[3] = data[2];
		i2c_wr_buffer[4] = data[3];
		i2c_adc_write_block(I2C_FRAM, 3);
}
/*
// ================== Event system
#define CONF_EVENT_GENERATOR EVSYS_ID_GEN_RTC_OVF
#define CONF_EVENT_USER EVSYS_ID_USER_ADC_START

static void configure_event_channel(struct events_resource *resource)
{
	struct events_config config;
	events_get_config_defaults(&config);
	config.generator = CONF_EVENT_GENERATOR;
	config.edge_detect = EVENTS_EDGE_DETECT_RISING;
	config.path = EVENTS_PATH_SYNCHRONOUS;
	config.clock_source = GCLK_GENERATOR_0;
	events_allocate(resource, &config);
}
static void configure_event_user(struct events_resource *resource)
{
	events_attach_user(resource, CONF_EVENT_USER);
}
*/

//============= I2C ==================================================================================
// =================================================================================================================
void factory_init(void)
{
	uint8_t i;
	
	work_cnt = 0;
	limit_delay_time1 = 50;	// 0.5 sec
	limit_delay_time2 = 50;	// 0.5 sec
	limit_delay_time3 = 50;	// 0.5 sec
	limit_trigger_time2 = 50;	// 0.5 sec
	limit_trigger_time3 = 50;	// 0.5 sec
	f_safty = 0;				// safty_func off
	run_mode = MODE_DELAY;
	sys_mode = SYS_FND;
	
	model_freq = 0;
	model_type = 0;
	limit_on_time = 50;		// x 10mS
	output_power = 50;		// 50%
	limit_mo_out1 = 25;
	limit_mo_out2 = 50;
	limit_mo_time1 = 25;
	limit_mo_time2 = 50;
	limit_out_time = 10;
	cal_val = 0;
	freq_cal_val = 0;
	
	limit_energy = 100000;
	energy_ctrl = false;

	comm_mode = COMM_SERIAL;

	save_word_fram(ADDR_WORK_CNT, work_cnt, fram_mode);
	save_param_fram(ADDR_DELAY1, limit_delay_time1, fram_mode);
	save_param_fram(ADDR_DELAY2, limit_delay_time2, fram_mode);
	save_param_fram(ADDR_DELAY3, limit_delay_time3, fram_mode);
	save_param_fram(ADDR_TRIGGER2, limit_trigger_time2, fram_mode);
	save_param_fram(ADDR_TRIGGER3, limit_trigger_time3, fram_mode);
	save_byte_fram(ADDR_SAFTY, f_safty, fram_mode);
	save_byte_fram(ADDR_RUN_MODE, run_mode, fram_mode);
	
	save_byte_fram(ADDR_MODEL_FREQ, model_freq, fram_mode);
	save_byte_fram(ADDR_MODEL_TYPE, model_type, fram_mode);
	save_byte_fram(ADDR_OUT_POWER, output_power, fram_mode);
	save_param_fram(ADDR_ON_TIME, limit_on_time, fram_mode);
	
	save_byte_fram(ADDR_EN_ENERGY, 0, fram_mode);	// flase
	save_byte_fram(ADDR_EN_MULTI, 0, fram_mode);	// flase
	save_word_fram(ADDR_ENERGY, limit_energy, fram_mode);
	save_param_fram(ADDR_MULTI_T1, limit_mo_time1, fram_mode);
	save_param_fram(ADDR_MULTI_T2, limit_mo_time2, fram_mode);
	save_param_fram(ADDR_MULTI_O1, limit_mo_out1, fram_mode);
	save_param_fram(ADDR_MULTI_O2, limit_mo_out2, fram_mode);
	save_byte_fram(ADDR_TIMEOVER, limit_out_time, fram_mode);
	save_byte_fram(ADDR_COMM_ADDR, comm_address, fram_mode);
	save_byte_fram(ADDR_COMM_SPEED, comm_speed_idx, fram_mode);
	save_byte_fram(ADDR_COMM_PARITY, comm_parity_idx, fram_mode);
	save_param_fram(ADDR_CAL_VAL, cal_val, fram_mode);
	save_param_fram(ADDR_FREQ_CAL_VAL, freq_cal_val, fram_mode);
	
	save_byte_fram(ADDR_COMM_MODE, comm_mode, fram_mode);	// flase
	if(comm_mode == COMM_ETH_DHCP)
	{
		for(i = 0 ; i < 4 ; i++)
		{
			ether_ip[i] = 0;
			ether_nm[i] = 0;
			ether_gw[i] = 0;
		}
	}
	else
	{
		save_4byte_fram(ADDR_ETHER_IP, ether_ip);
		save_4byte_fram(ADDR_ETHER_NM, ether_nm);
		save_4byte_fram(ADDR_ETHER_GW, ether_gw);
	}
}



void var_init(void)
{
	uint8_t temp, temp2;
	
	

	uint32_t *ptr1 = (volatile uint32_t *)0x0080A00C;

	unique_serial[0] = *ptr1;

	uint32_t *ptr = (volatile uint32_t *)0x0080A040;

	unique_serial[1] = *ptr;

	ptr++;

	unique_serial[2] = *ptr;

	ptr++;

	unique_serial[3] = *ptr;
	
	first_1sec = 1;

	disp_timer = 0;
	boot_status = 0;

	job_state = JOB_STATE_COMMAND;
	
	sw_up = sw_down = sw_mode = sw_set = sw_cnt_reset = 1;
	sw_up_bak = sw_down_bak = sw_mode_bak = sw_set_bak = sw_cnt_reset = 1;
	sw_mode_pressed = sw_up_pressed = sw_down_pressed = sw_set_pressed  = sw_cnt_reset = 0; 
	mode_key_hold = up_key_hold = dn_key_hold = 0;
	in_cycle = 0;
	
	fnd_display_dimm = 20;
	fnd_display_color = 20;
	fnd_display_watt = 0;
	fnd_status_data = 0;

	ctrl_cnt = 0;
	startup_time = 0;
	
	
	sys_status = SYS_RUN;
	sys_mode = SYS_FND;
	run_status = RUN_READY;
	
	us_run_status = us_reset_status = us_seek_status = US_IDLE;
	comm_address = 0;
	comm_speed_idx = 0;
	comm_parity_idx = 0;
	modbus_status = bak_modbus_status = 0;
	
	run_cnt = 0;
	fram_mode = 1;		//fm24c16
	temp = read_byte_fram(ADDR_INIT_FLAG, fram_mode);
	if(temp != 0xaa)
	{
		save_byte_fram(ADDR_INIT_FLAG, 0xaa, fram_mode);
		factory_init();

	}
	else
	{
		work_cnt = read_word_fram(ADDR_WORK_CNT, fram_mode);
		limit_delay_time1 = read_param_fram(ADDR_DELAY1, fram_mode);
		limit_delay_time2 = read_param_fram(ADDR_DELAY2, fram_mode);
		limit_delay_time3 = read_param_fram(ADDR_DELAY3, fram_mode);
		limit_trigger_time2 = read_param_fram(ADDR_TRIGGER2, fram_mode);
		limit_trigger_time3 = read_param_fram(ADDR_TRIGGER3, fram_mode);
		f_safty = read_byte_fram(ADDR_SAFTY, fram_mode);
		run_mode = read_byte_fram(ADDR_RUN_MODE, fram_mode);
		
		model_freq = read_byte_fram(ADDR_MODEL_FREQ, fram_mode);
		model_type = read_byte_fram(ADDR_MODEL_TYPE, fram_mode);
		output_power = read_byte_fram(ADDR_OUT_POWER, fram_mode);
		temp = ((output_power-50) * 255) / 100;
		i2c_adc_write_byte(I2C_POT, I2C_POT_WP, (uint8_t)temp);
		limit_on_time = read_param_fram(ADDR_ON_TIME, fram_mode);
		temp = read_byte_fram(ADDR_EN_ENERGY, fram_mode);
		if(temp == 1)
			energy_ctrl = true;
		else 
			energy_ctrl = false;
		limit_energy = read_word_fram(ADDR_ENERGY, fram_mode);
		temp = read_byte_fram(ADDR_EN_MULTI, fram_mode);
		if(temp == 1)
			multi_ctrl = true;
		else 
			multi_ctrl = false;
		if(limit_energy > 100000)
		{
			limit_energy = 100000;
			save_word_fram(ADDR_ENERGY, limit_energy, fram_mode);
		}
		limit_mo_time1 = read_param_fram(ADDR_MULTI_T1, fram_mode);
		limit_mo_time2 = read_param_fram(ADDR_MULTI_T2, fram_mode);
		limit_mo_out1 = read_param_fram(ADDR_MULTI_O1, fram_mode);
		limit_mo_out2 = read_param_fram(ADDR_MULTI_O2, fram_mode);
		limit_out_time = read_byte_fram(ADDR_TIMEOVER, fram_mode);
		comm_address = read_byte_fram(ADDR_COMM_ADDR, fram_mode);
		comm_speed_idx = read_byte_fram(ADDR_COMM_SPEED, fram_mode);
		comm_parity_idx = read_byte_fram(ADDR_COMM_PARITY, fram_mode);
		last_amp = last_energy = last_freq = last_lv = 0;
		cal_val = read_param_fram(ADDR_CAL_VAL, fram_mode);
		freq_cal_val = read_param_fram(ADDR_FREQ_CAL_VAL, fram_mode);
		comm_mode = read_byte_fram(ADDR_COMM_MODE, fram_mode);
		read_4byte_fram(ADDR_ETHER_IP, ether_ip);
		read_4byte_fram(ADDR_ETHER_NM, ether_nm);
		read_4byte_fram(ADDR_ETHER_GW, ether_gw);
	}
	
	step_run_time = 0;
	sig_run_status = bak_run_status = 0;
	sig_reset_status = bak_reset_status = 0;
	sig_seek_status = bak_seek_status = 0;
	sig_ovld_status = bak_ovld_status = 0;
	
	max_amp = 0;
	
	update_amp = update_freq = false;
}

void Decimal2String(uint16_t dec, uint8_t * str)
{
	if(dec > 999)
		str[0] = (uint8_t)(dec / 1000) + 0x30;
	else 
		str[0] = ' ';
	dec %= 1000;

	str[1] = (uint8_t)(dec / 100) + 0x30;
	dec %= 100;

	str[2] = (uint8_t)(dec / 10) + 0x30;
	dec %= 10;

	str[3] = (uint8_t)dec + 0x30;
}

void Decimal2String2(uint16_t dec, uint8_t * str)
{
	str[0] = (uint8_t)(dec / 100) + 0x30;
	dec %= 100;

	str[1] = (uint8_t)(dec / 10) + 0x30;
	dec %= 10;

	str[2] = (uint8_t)dec + 0x30;
}

void Decimal2String3(uint8_t dec, uint8_t * str)
{
	str[0] = (uint8_t)(dec / 10) + 0x30;
	dec %= 10;

	str[1] = (uint8_t)dec + 0x30;
}


// ===============================================================================================================

void check_sw_input(void)
{
	sw_up = port_pin_get_input_level(SW_UP);
	sw_down = port_pin_get_input_level(SW_DOWN);
	sw_mode = port_pin_get_input_level(SW_MODE);
	sw_set = port_pin_get_input_level(SW_SET);
	sw_cnt_reset = port_pin_get_input_level(SW_CNT_RESET);
	
	if(sw_up != sw_up_bak) {
		if(sw_up == 0)
		{ 
//			LED_LOW(GPIO_LED_UP);
			sw_up_pressed = 1;
			key_tick = 0;
			disp_cnt = 0;
		} 
		else 
		{
//			LED_HIGH(GPIO_LED_UP);
			up_key_hold = 0;
		}
		sw_up_bak = sw_up;
	} else {
		if(sw_up == 0) {
			if(up_key_hold == 0)
			{
				if(key_tick > KEY_HOLD_TH)
				{
					up_key_hold = 1;
					key_tick = 0;
				}
			} 
			else 
			{
				if(key_tick > REPEATE_KEY_TH)
				{
					sw_up_pressed = 1;
					key_tick = 0;
				}
			}
		}
	}
	if(sw_down != sw_down_bak) 
	{
		if(sw_down == 0) 
		{ 
//			LED_LOW(GPIO_LED_DOWN);
			sw_down_pressed = 1;
			key_tick = 0;
			disp_cnt = 0;
		} 
		else 
		{
//			LED_HIGH(GPIO_LED_DOWN);
			dn_key_hold = 0;
		}
		sw_down_bak = sw_down;
	} 
	else 
	{
		if(sw_down == 0) 
		{
			if(dn_key_hold == 0)
			{
				if(key_tick > KEY_HOLD_TH)
				{
					dn_key_hold = 1;
					key_tick = 0;
				}
			} 
			else 
			{
				if(key_tick > REPEATE_KEY_TH)
				{
					sw_down_pressed = 1;
					key_tick = 0;
				}
			}
		}
	}
	
	if(sw_mode != sw_mode_bak) // change key status
	{
		if(sw_mode == 0) 
		{ 
//			LED_LOW(GPIO_LED_MODE);
			//sw_mode_pressed = 1;
			key_tick = 0;
			disp_cnt = 0;
		} 
		else 
		{
			if((key_tick != 0) && (key_tick < 50))
				sw_mode_pressed = 1;
			else
				sw_mode_pressed = 0;
//			LED_HIGH(GPIO_LED_MODE);
			mode_key_hold = 0;
		}
		sw_mode_bak = sw_mode;
	} 
	else 
	{
		if(sw_mode == 0) // key still pressed
		{
			if(mode_key_hold == 0)
			{
				if(key_tick > MODE_KEY_HOLD_TH)
				{
					mode_key_hold = 1;
					sw_mode_pressed =0;
//					key_tick = 0;
				}
			} 
		}
	}

	if(sw_set != sw_set_bak) 
	{
//		if(set_key_hold != 1)
//		{
			if(sw_set == 0) { 
		//		sw_set_pressed = 1;
				key_tick = 0;
				disp_cnt = 0;
			}
			else {
				if((key_tick != 0) && (key_tick < 50))
					sw_set_pressed = 1;
				else
					sw_set_pressed = 0;
			
			}
//		}
		sw_set_bak = sw_set;
	}
	else 
	{
		if(sw_set == 0) // key still pressed
		{
			if(set_key_hold == 0)
			{
				if(key_tick > MODE_KEY_HOLD_TH)
				{
					set_key_hold = 1;
					sw_set_pressed =0;
//					key_tick = 0;
				}
			} 
		}
	}

	
	if(sw_cnt_reset != sw_cnt_reset_bak) // change key status
	{
		if(sw_cnt_reset == 0)
		{
			//			LED_LOW(GPIO_LED_MODE);
			sw_cnt_reset_pressed = 1;
			key_tick = 0;
			disp_cnt = 0;
		}
		else
		{
			//			LED_HIGH(GPIO_LED_MODE);
			cnt_reset_key_hold = 0;
		}
		sw_cnt_reset_bak = sw_cnt_reset;
	}
	else
	{
		if(sw_cnt_reset == 0) // key still pressed
		{
			if(cnt_reset_key_hold == 0)
			{
				if(key_tick > MODE_KEY_HOLD_TH)
				{
					cnt_reset_key_hold = 1;
					sw_cnt_reset_pressed = 0;
					key_tick = 0;
				}
			}
		}
	}

	
}

void check_remote_input(void)
{
	uint16_t temp_i;
	
	re_start1 = port_pin_get_input_level(SW_START1);
	re_start2 = port_pin_get_input_level(SW_START2);
	if((sys_mode == SYS_MULTI) || (sys_mode == SYS_HAND))
	{
//		re_emsw = !port_pin_get_input_level(SW_EMSW);
		re_emsw = 0;	// normal
		re_seek = port_pin_get_input_level(B_SEEK);
	}
	else
	{
		re_emsw = port_pin_get_input_level(SW_EMSW);
	}
	re_reset = port_pin_get_input_level(B_RESET);
	re_start = port_pin_get_input_level(B_START);
	re_ovld = !(port_pin_get_input_level(M_OVLD));
	re_up = port_pin_get_input_level(SENSE_UP);
	re_dn = port_pin_get_input_level(SENSE_DN);
	
	if(re_start1 != re_start1_bak) {
		if(re_start1 == 0)
		{ 
			re_start1_pressed = 1;
		} 
		re_start1_bak = re_start1;
	}
	if(re_start2 != re_start2_bak) {
		if(re_start2 == 0)
		{
			re_start2_pressed = 1;
		}
		re_start2_bak = re_start2;
	}
	if((re_start1 == 1) && (re_start2 == 1) && (run_status == RUN_READY))
		in_cycle = 0;

	if(re_up != re_up_bak) {
		if(re_up == 0)
		{
			re_up_pressed = 1;
		}
		re_up_bak = re_up;
	}
	
	if(re_dn != re_dn_bak) {
		if(re_dn == 0)
		{
			re_dn_pressed = 1;
			if(run_mode == MODE_TRIGGER)
			{
				lcd_temp_buf[0] = 'S';
				lcd_temp_buf[1] = 'E';
				lcd_temp_buf[2] = 'N';
				lcd_temp_buf[3] = 'S';
				lcd_temp_buf[4] = 'O';
				lcd_temp_buf[5] = 'R';
				lcd_temp_buf[6] = ' ';
				lcd_temp_buf[7] = 'O';
				lcd_temp_buf[8] = 'N';
				lcd_temp_buf[9] = '\0';
				send_lcd_byte_array(DISP_STD_DATA1, 10, lcd_temp_buf);
			}
		} 
		else
		{
			if(run_mode == MODE_TRIGGER)
			{
				lcd_temp_buf[0] = 'S';
				lcd_temp_buf[1] = 'E';
				lcd_temp_buf[2] = 'N';
				lcd_temp_buf[3] = 'S';
				lcd_temp_buf[4] = 'O';
				lcd_temp_buf[5] = 'R';
				lcd_temp_buf[6] = ' ';
				lcd_temp_buf[7] = 'O';
				lcd_temp_buf[8] = 'F';
				lcd_temp_buf[9] = 'F';
				lcd_temp_buf[10] = '\0';
				send_lcd_byte_array(DISP_STD_DATA1, 11, lcd_temp_buf);
			}
		} 
		re_dn_bak = re_dn;
	}
/*
	if(re_emsw != re_emsw_bak) {		// normal close
		if(re_emsw == 1)
		{
			re_emsw_issued = 1;
		}
		
		re_emsw_bak = re_emsw;
	}
*/	
	if(re_reset != re_reset_bak) {
		if(re_reset == 0)
		{		// on
			if( us_reset_status != US_TOUCH)
			{
				us_reset_status = US_REMOTE;
				re_reset_pressed = 1;
				sig_reset_status = ON;
				max_us_reset_cnt = 0xff;
				us_reset_cnt = 0;
				if(sig_seek_status == ON)
				{
					sig_seek_status = OFF;
					us_seek_status = US_IDLE;
				}
				if((sys_status == SYS_ERROR) && (((error_status & ERR_OVLD) == ERR_OVLD) || ((error_status & ERR_OUTERR) == ERR_OUTERR)))
				{
					send_lcd_data_var(ICON_OL, (uint16_t)0);
					send_lcd_data_var(ICON_OUTERR, (uint16_t)0);
					re_outerr_issued = 0;
					if(model_type == 0)		//hand
					lcd_status = LCD_RUN_HAND;
					else if(model_type == 1)		//multi
					lcd_status = LCD_RUN_MULTI;
					else if(model_type == 2)		//standrd
					lcd_status = LCD_RUN_STD;
							
					set_lcd_page(lcd_status);
				}
			}
		}
		else
		{		// off
			if(us_reset_status == US_REMOTE)
			{
				sig_reset_status = OFF;
				//us_reset_status = US_IDLE;
				//us_reset_cnt = 0;
				//max_us_reset_cnt = MAX_US_RESET_CNT;
				
				sig_reset_status = OFF;
				sig_seek_status = ON;
				us_reset_cnt = 0;
				max_us_reset_cnt = MAX_US_RESET_CNT;
				us_reset_status = US_IDLE;
				us_seek_status = US_TOUCH;
				
			}
		}
		re_reset_bak = re_reset;
	}

	if(re_seek != re_seek_bak) {
		if(re_seek == 0)
		{		// on
			if( us_seek_status != US_TOUCH)
			{
				us_seek_status = US_REMOTE;
				re_seek_pressed = 1;
				sig_seek_status = ON;
				max_us_reset_cnt = 0xff;
				curr_energy = last_energy = max_amp = max_power = acc_energy = 0;
			}
		}
		else
		{		// off
			if(us_seek_status == US_REMOTE)
			{
				us_seek_status = US_IDLE;
				sig_seek_status = OFF;
				us_reset_cnt = 0;
				max_us_reset_cnt = MAX_US_RESET_CNT;
			}
		}
		re_seek_bak = re_seek;
	}

	if(re_start != re_start_bak) {
		if(re_start == 0)
		{		// on
			re_start_pressed = 1;
			if( us_run_status != US_TOUCH)
			{
				us_run_status = US_REMOTE;
				sig_run_status = ON;
				if(bak_run_status == OFF)
				{
					curr_energy = last_energy = max_amp = max_power = acc_energy = 0;
					if(multi_ctrl == true)
					{
						multi_ctrl_stage = 0;
						temp_i = ((limit_mo_out1-50) * 255) / 100;
					}
					else
					{
						temp_i = (((output_power-50) * 255) / 100 );
					}
					i2c_adc_write_byte(I2C_POT, I2C_POT_WP, (uint8_t)temp_i);

				}
			}
		}
		else
		{		// off
			if(us_run_status == US_REMOTE)
			{
				us_run_status = US_IDLE;
				sig_run_status = OFF;
			}
		}
		re_start_bak = re_start;
	}
		
//	if(re_ovld != re_ovld_bak) {
		if(re_ovld == 0)
		{
			re_ovld_issued = 1;
		}
		else
		{
			re_ovld_issued = 0;
		}
//		re_ovld_bak = re_ovld;
//	}
	
	if((re_start1 == 0)&&(re_start2 == 0)&&(in_cycle == 0))
		start_key_pressed = 1;
	else
		start_key_pressed = 0;
		
	if(re_emsw != re_emsw_bak)
	{
		if(re_emsw)
		{
			re_emsw_issued = 0;
			sys_status = SYS_ESTOP;
			port_pin_set_output_level(SOL_DN, SOL_OFF);
			port_pin_set_output_level(M_START, CTRL_INT_OFF);
		}	
		else
		{
			sys_status = SYS_RUN;
			run_status = RUN_READY;
			f_status_start = 0;
		}
		re_emsw_bak = re_emsw;
	}
	
	if(sys_status == SYS_HORN)
	{
		if(sw_mode_pressed) {
			sw_mode_pressed = 0;
			sys_status = SYS_RUN;
		}
		if(re_start1_pressed || re_start2_pressed)
		{
			re_start1_pressed = re_start2_pressed = 0;
			if(horn_status == 0)
				horn_status = 1;
			else
				horn_status = 0;
				
		}
	}
}


void do_action(void)
{
	uint16_t temp_i;
	
	if(sys_status == SYS_RUN)
	{
		if(re_ovld_issued)
		{
			sys_status = SYS_ERROR;
			error_status |= ERR_OVLD;
			f_status_start = 0;
		}
		if(re_outerr_issued)
		{
			sys_status = SYS_ERROR;
			error_status |= ERR_OUTERR;
			f_status_start = 0;
		}
		if(run_status == RUN_READY)
		{
			if(start_key_pressed)
			{
				if(f_status_start == 0)		// first time
				{
					//f_status_start = 1;
					run_status = RUN_CYL1;
					in_cycle = 1;
					if(run_mode == MODE_DELAY)
						temp_time = limit_delay_time1;
					else
						temp_time = CYL_TIMEOUT;
					
					re_dn_pressed = 0;
				}
			}
		}
		else if(run_status == RUN_CYL1)
		{
			if((f_safty == 1) && ((re_start1 == 1)||(re_start2 == 1)))
			{
				port_pin_set_output_level(SOL_DN, SOL_OFF);
				run_status = RUN_READY;
				f_status_start = 0;
				return;
			}
			if(f_status_start == 0)
			{
				f_status_start = 1;
				port_pin_set_output_level(SOL_DN, SOL_ON);
			}
			else
			{
				if(run_mode == MODE_DELAY)
				{
					if(temp_time == 0)
					{
						f_status_start = 0;
						run_status = RUN_WELD;
						if(limit_delay_time2 > 6){
						temp_time = limit_delay_time2;
							comp_time = 7;
						} else {
							comp_time = limit_delay_time2;
							temp_time = 7;
						}
					}
				}
				else
				{
					if(re_dn_pressed == 1)
					{
						re_dn_pressed = 0;
						f_status_start = 0;
						run_status = RUN_WELD;
						if(limit_trigger_time2 > 6){
						temp_time = limit_trigger_time2;
							comp_time = 7;
						}else{
							comp_time = limit_trigger_time2;
							temp_time = 7;
						}
					}
				}
			}
		}
		else if(run_status == RUN_WELD)
		{
			if(f_status_start == 0)
			{
				f_status_start = 1;
				
				if(multi_ctrl == true)
				{
					multi_ctrl_stage = 0;
					temp_i = (((limit_mo_out1-50) * 255) / 100 );
				}
				else
				{
					//if()
					temp_i = (((output_power-50) * 255) / 100 );
					if(comp_time < 7)
						temp_i = (temp_i - ((7 - comp_time)*10));
				}
				i2c_adc_write_byte(I2C_POT, I2C_POT_WP, (uint8_t)temp_i);

				
				port_pin_set_output_level(M_START, CTRL_INT_ON);
				sig_run_status = ON;
				us_on_time = 0;
				curr_energy = last_energy = max_amp = max_power = acc_energy = 0;

			}
			else
			{
				if((multi_ctrl != true) && (energy_ctrl != true))
				{
					if(temp_time == 0)
					{
						port_pin_set_output_level(M_START, CTRL_INT_OFF);
						sig_run_status = OFF;
						f_status_start = 0;
						run_status = RUN_HOLD;
						if(run_mode == MODE_DELAY)
							temp_time = limit_delay_time3;
						else
							temp_time = limit_trigger_time3;
					}
				}
			}
		}
		else if(run_status == RUN_HOLD)
		{
			if(f_status_start == 0)
			{
				f_status_start = 1;
			}
			else
			{
				if(temp_time == 0)
				{
					f_status_start = 0;
					run_status = RUN_CYL2;
					if(run_mode == MODE_DELAY)
						temp_time = limit_delay_time1;
					else
						temp_time = CYL_TIMEOUT;

					re_up_pressed = 1;	//-
				}
			}
		}
		else if(run_status == RUN_CYL2)
		{
			if(f_status_start == 0)
			{
				f_status_start = 1;
				port_pin_set_output_level(SOL_DN, SOL_OFF);
			}
			else
			{
				if(run_mode == MODE_DELAY)
				{
					if(temp_time == 0)
					{
						f_status_start = 0;
						run_status = RUN_READY;
						work_cnt++;
						save_word_fram(ADDR_WORK_CNT, work_cnt, fram_mode);
						send_lcd_data_word(LV_WORK_CNT, (uint32_t)work_cnt);

					}
				}
				else
				{
					if(re_up_pressed == 1)
					{
						re_up_pressed = 0;
						f_status_start = 0;
						run_status = RUN_READY;
						work_cnt++;
						save_word_fram(ADDR_WORK_CNT, work_cnt, fram_mode);
						send_lcd_data_word(LV_WORK_CNT, (uint32_t)work_cnt);
					}
				}
			}
			
		}
	}
	else if(sys_status == SYS_HORN)
	{
		if(horn_status == 1)
		{
			port_pin_set_output_level(SOL_DN, SOL_ON);
		}
		else
		{
			port_pin_set_output_level(SOL_DN, SOL_OFF);
		}
	}
	else if(sys_status == SYS_ERROR)
	{
		if(sw_cnt_reset_pressed)
		{
			sw_cnt_reset_pressed = 0;
			us_reset_status = US_TOUCH;
			sig_reset_status = ON;
			max_us_reset_cnt = MAX_US_RESET_CNT;
		}
		
//		if(temp_time == 0)
//			port_pin_set_output_level(M_RESET, CTRL_INT_OFF);
		
		
		if(re_ovld_issued)
		{
			sys_status = SYS_ERROR;
			error_status |= ERR_OVLD;
			port_pin_set_output_level(B_OVLD, CTRL_ON);
			port_pin_set_output_level(SOL_DN, SOL_OFF);
			port_pin_set_output_level(M_START, CTRL_INT_OFF);
		}
		else
		{
			error_status &= ~(ERR_OVLD);
			port_pin_set_output_level(B_OVLD, CTRL_OFF);
			if(error_status == 0)
			{
				sys_status = SYS_RUN;
				error_status = ERR_NOERROR;
				run_status = RUN_READY;
				f_status_start = 0;
			}
		}
		
		if(re_outerr_issued)
		{
			sys_status = SYS_ERROR;
			error_status |= ERR_OUTERR;
			//port_pin_set_output_level(B_OVLD, CTRL_ON);
			port_pin_set_output_level(SOL_DN, SOL_OFF);
			port_pin_set_output_level(M_START, CTRL_INT_OFF);
		}
		else
		{
			error_status &= ~(ERR_OUTERR);
			//port_pin_set_output_level(B_OVLD, CTRL_OFF);
			if(error_status == 0)
			{
				sys_status = SYS_RUN;
				error_status = ERR_NOERROR;
				run_status = RUN_READY;
				f_status_start = 0;
			}
		}
	}
}
//---------------------------------------------------------------------

//------------------------------ watch dog----------------------------
void configure_wdt_off(void)
{
    /* Create a new configuration structure for the Watchdog settings and fill
     * with the default module settings. */
    struct wdt_conf config_wdt;
    wdt_get_config_defaults(&config_wdt);

    /* Set the Watchdog configuration settings */
    config_wdt.enable      = false;
#if !((SAML21) || (SAMC21) || (SAML22) || (SAMR30))
    config_wdt.clock_source   = GCLK_GENERATOR_1;
#endif
    config_wdt.timeout_period = WDT_PERIOD_16384CLK;

    /* Initialize and enable the Watchdog with the user settings */
    wdt_set_config(&config_wdt);
}

void configure_wdt_on(void)
{
    /* Create a new configuration structure for the Watchdog settings and fill
     * with the default module settings. */
    struct wdt_conf config_wdt;
    wdt_get_config_defaults(&config_wdt);

    /* Set the Watchdog configuration settings */
    config_wdt.enable      = true;
#if !((SAML21) || (SAMC21) || (SAML22) || (SAMR30))
    config_wdt.clock_source   = GCLK_GENERATOR_1;
#endif
    config_wdt.timeout_period = WDT_PERIOD_128CLK;

    /* Initialize and enable the Watchdog with the user settings */
    wdt_set_config(&config_wdt);
}

void my_utoa(uint16_t src_val, uint8_t *dest_ptr)
{
	uint8_t nibble;
	uint16_t temp_i;
	
	nibble = (uint8_t)((src_val >> 12) & 0x000f);
	dest_ptr[0] = (nibble > 9) ? (nibble-10 + 'A') : (nibble+'0');
	nibble = (uint8_t)((src_val >> 8) & 0x000f);
	dest_ptr[1] = (nibble > 9) ? (nibble-10 + 'A') : (nibble+'0');
	nibble = (uint8_t)((src_val >> 4) & 0x000f);
	dest_ptr[2] = (nibble > 9) ? (nibble-10 + 'A') : (nibble+'0');
	nibble = (uint8_t)(src_val & 0x000f);
	dest_ptr[3] = (nibble > 9) ? (nibble-10 + 'A') : (nibble+'0');
}

void display_FND(void)
{
	unsigned char num;
	port_pin_set_output_level(FND_D0, FND_OFF);
	port_pin_set_output_level(FND_D1, FND_OFF);
	port_pin_set_output_level(FND_D2, FND_OFF);
	port_pin_set_output_level(FND_D3, FND_OFF);
	port_pin_set_output_level(FND_D4, FND_OFF);
	port_pin_set_output_level(FND_D5, FND_OFF);
	port_pin_set_output_level(FND_D6, FND_OFF);
	port_pin_set_output_level(FND_D7, FND_OFF);
	port_pin_set_output_level(FND_COM0, FND_OFF);
	port_pin_set_output_level(FND_COM1, FND_OFF);
	port_pin_set_output_level(FND_COM2, FND_OFF);
	port_pin_set_output_level(FND_COM3, FND_OFF);
	port_pin_set_output_level(FND_COM4, FND_OFF);
	port_pin_set_output_level(FND_COM5, FND_OFF);
	port_pin_set_output_level(FND_COM6, FND_OFF);
	port_pin_set_output_level(FND_COM7, FND_OFF);

	if(fnd_dig == (DIGIT_LEN-1))
		num = fnd_status_data;
	else
		num = digit[aChar[fnd_dig]]; //if dot include...then -1
		
	
	if(fnd_dig == point_ch) num += 0x80;		// dot

	port_pin_set_output_level(FND_D0, num & 0x01);
	port_pin_set_output_level(FND_D1, num & 0x02);
	port_pin_set_output_level(FND_D2, num & 0x04);
	port_pin_set_output_level(FND_D3, num & 0x08);
	port_pin_set_output_level(FND_D4, num & 0x10);
	port_pin_set_output_level(FND_D5, num & 0x20);
	port_pin_set_output_level(FND_D6, num & 0x40);
	port_pin_set_output_level(FND_D7, num & 0x80);

	switch(fnd_dig) {
		case 0 : 
			port_pin_set_output_level(FND_COM0, FND_ON);
			break;
		case 1 :
			port_pin_set_output_level(FND_COM1, FND_ON);
			break;
		case 2 :
			port_pin_set_output_level(FND_COM2, FND_ON);
			break;
		case 3 :
			port_pin_set_output_level(FND_COM3, FND_ON);
			break;
		case 4 :
			port_pin_set_output_level(FND_COM4, FND_ON);
			break;
		case 5 :
			port_pin_set_output_level(FND_COM5, FND_ON);
			break;
		case 6 :
			port_pin_set_output_level(FND_COM6, FND_ON);
		break;
		case 7 :
			port_pin_set_output_level(FND_COM7, FND_ON);
		break;
	}
	fnd_dig++;
	if(fnd_dig == DIGIT_LEN)	fnd_dig=0;
}

void fnd_out(uint16_t s_mode)		// 520usec
{
	uint32_t temp;
	uint16_t temp_i;
	if(s_mode == SYS_RUN)
	{
		if(run_status == RUN_READY)
		{
			aChar[0] = (uint8_t)(work_cnt / 1000000);
			temp = work_cnt % 1000000;
			aChar[1] = (uint8_t)(temp / 100000);
			temp = temp % 100000;
			aChar[2] = (uint8_t)(temp /10000);
			temp = temp % 10000;
			aChar[3] = (uint8_t)(temp / 1000);
			temp = temp % 1000;
			aChar[4] = (uint8_t)(temp / 100);
			temp = temp % 100;
			aChar[5] = (uint8_t)(temp / 10);
			aChar[6] = (uint8_t)(temp % 10);
			point_ch = 7;
		}
		else
		{
			if(run_mode == MODE_DELAY)
			{
				aChar[0] = SEG_d;
				if(run_status == RUN_CYL1)
				{
					aChar[1] = 1;
				}
				else if(run_status == RUN_WELD)
				{
					aChar[1] = 2;
				}
				else if(run_status == RUN_HOLD)
				{
					aChar[1] = 3;
				}
				else if(run_status == RUN_CYL2)
				{
					aChar[1] = 1;
				}
				aChar[6] = SEG_SPACE;
				aChar[3] = (uint8_t)(temp_time / 1000);
				temp_i = temp_time % 1000;
				aChar[4] = (uint8_t)(temp_i / 100);
				temp_i = temp_i % 100;
				aChar[5] = (uint8_t)(temp_i / 10);
				aChar[6] = (uint8_t)(0);
				point_ch = 4;
			}
			else
			{
				if(run_status == RUN_CYL1)
				{
					aChar[0] = SEG_d;
					aChar[1] = SEG_n;
				}
				else if(run_status == RUN_WELD)
				{
					aChar[0] = SEG_t;
					aChar[1] = 2;
				}
				else if(run_status == RUN_HOLD)
				{
					aChar[0] = SEG_t;
					aChar[1] = 3;
				}
				else if(run_status == RUN_CYL2)
				{
					aChar[0] = SEG_u;
					aChar[1] = SEG_p;
				}
				if((run_status == RUN_CYL1) ||(run_status == RUN_CYL2))
				{
					aChar[2] = aChar[3] = aChar[4] = aChar[5] = aChar[6] = SEG_SPACE;
					point_ch = 7;
				}
				else
				{
					aChar[6] = SEG_SPACE;
					aChar[3] = (uint8_t)(temp_time / 1000);
					temp_i = temp_time % 1000;
					aChar[4] = (uint8_t)(temp_i / 100);
					temp_i = temp_i % 100;
					aChar[5] = (uint8_t)(temp_i / 10);
					aChar[6] = (uint8_t)(0);
					point_ch = 4;
				}
			}
		}
	}
	else if(s_mode == SYS_SETUP)
	{
		if(setup_mode == MODE_DELAY)
			aChar[0] = SEG_d;
		else
			aChar[0] = SEG_t;
				
		if(setup_status == SETUP_DELAY)
			aChar[1] = 1;
		else if(setup_status == SETUP_WELD)
			aChar[1] = 2;
		else if(setup_status == SETUP_HOLD)
			aChar[1] = 3;

		if(setup_status != SETUP_SAFTY)
		{
			aChar[3] = (uint8_t)(temp_time / 1000);
			temp_i = temp_time % 1000;
			aChar[4] = (uint8_t)(temp_i / 100);
			temp_i = temp_i % 100;
			aChar[5] = (uint8_t)(temp_i / 10);
			aChar[6] = (uint8_t)(temp_i % 10);
			point_ch = 4;
		}
		else
		{
			aChar[0] = SEG_s;
			aChar[1] = SEG_a;
			aChar[2] = SEG_SPACE;
			aChar[3] = SEG_SPACE;
			aChar[4] = SEG_o;
			if(temp_time)
			{
				aChar[5] = SEG_n;
				aChar[6] = SEG_SPACE;
			}
			else
			{
				aChar[5] = SEG_f;
				aChar[6] = SEG_f;
			}
			point_ch = 7;
		}
	}
	else if(s_mode == SYS_HORN)
	{
		aChar[0] = SEG_h;
		aChar[1] = SEG_o;
		aChar[2] = SEG_r;
		aChar[3] = SEG_n;
		aChar[4] = SEG_SPACE;
		if(horn_status)
		{
			aChar[5] = SEG_d;
			aChar[6] = SEG_n;
		}
		else
		{
			aChar[5] = SEG_u;
			aChar[6] = SEG_p;
		}
		point_ch = 7;
	}
	else if(s_mode == SYS_ERROR)
	{
		if(error_status == 1)	// OVLD
		{
			if(mode_blink == 0)
			{
				aChar[0] = 0;
				aChar[1] = SEG_DASH;
				aChar[2] = SEG_s;
				aChar[3] = SEG_t;
				aChar[4] = 0;
				aChar[5] = SEG_p;
			}
			else
			{
				aChar[0] = SEG_SPACE;
				aChar[1] = SEG_SPACE;
				aChar[2] = SEG_SPACE;
				aChar[3] = SEG_SPACE;
				aChar[4] = SEG_SPACE;
				aChar[5] = SEG_SPACE;
				aChar[6] = SEG_SPACE;
			}
			point_ch = 7;
		}
		
	}
	else if(sys_status == SYS_ESTOP)
	{
		if(mode_blink == 0)
		{
			aChar[0] = SEG_E;
			aChar[1] = SEG_DASH;
			aChar[2] = SEG_s;
			aChar[3] = SEG_t;
			aChar[4] = 0;
			aChar[5] = SEG_p;
		}
		else 
		{
			aChar[0] = SEG_SPACE;
			aChar[1] = SEG_SPACE;
			aChar[2] = SEG_SPACE;
			aChar[3] = SEG_SPACE;
			aChar[4] = SEG_SPACE;
			aChar[5] = SEG_SPACE;
			aChar[6] = SEG_SPACE;
		}
		point_ch = 7;
	}
}

void fnd_out_error(uint8_t code)
{
	point_ch = 0;
	aChar[0] = 11;	// 'E'
	aChar[1] = 12;	// '-'
	aChar[2] = code%10;
}

void disp_update(uint8_t mode)
{
	if(mode == SYS_SETUP)
	{
//			run_mode = RUN_NORMAL;
		disp_cnt = 0;
		max_disp_cnt = MAX_RUN_DISP_TIME;
		fnd_out(sys_status);
	}
	else
	{
		disp_cnt = 0;
		fnd_out(sys_status);
	}
}

void led_update(void)
{
//-	port_pin_set_output_level(LED_ALARM, LED_OFF);
	fnd_status_data &= ~(SEG_ALARM);
	buzzer_off();

	if(sys_status == SYS_RUN)
	{
		if(run_mode == MODE_TRIGGER)
		{
//-			port_pin_set_output_level(LED_TRIGGER, LED_ON);
//-			port_pin_set_output_level(LED_DELAY, LED_OFF);
			fnd_status_data |= (SEG_TRRIGER);
			fnd_status_data &= ~(SEG_DELAY);
		}
		else
		{
//-			port_pin_set_output_level(LED_TRIGGER, LED_OFF);
//-			port_pin_set_output_level(LED_DELAY, LED_ON);
			fnd_status_data &= ~(SEG_TRRIGER);
			fnd_status_data |= (SEG_DELAY);
		}

		if((run_status == RUN_WELD) || (run_status == RUN_HOLD))
		{
//-			port_pin_set_output_level(LED_RUN, LED_ON);
			fnd_status_data |= (SEG_RUN);
		}
		else
		{
//-			port_pin_set_output_level(LED_RUN, LED_OFF);
			fnd_status_data &= ~(SEG_RUN);
		}
	}
	else if(sys_status == SYS_SETUP)
	{
		if(setup_mode == MODE_TRIGGER)
		{
			if(mode_blink == 0)
//-				port_pin_set_output_level(LED_TRIGGER, LED_ON);
				fnd_status_data |= SEG_TRRIGER;
			else 
//-				port_pin_set_output_level(LED_TRIGGER, LED_OFF);
				fnd_status_data &= ~(SEG_TRRIGER);
//-			port_pin_set_output_level(LED_DELAY, LED_OFF);
			fnd_status_data &= ~(SEG_DELAY);
		}
		else
		{
//-			port_pin_set_output_level(LED_TRIGGER, LED_OFF);
			fnd_status_data &= ~(SEG_TRRIGER);
			
			if(mode_blink == 0)
//-				port_pin_set_output_level(LED_DELAY, LED_ON);
				fnd_status_data |= SEG_DELAY;
			else
//-				port_pin_set_output_level(LED_DELAY, LED_OFF);
				fnd_status_data &= ~(SEG_DELAY);
		}
	}
	else if(sys_status == SYS_ESTOP)
	{
//-		port_pin_set_output_level(LED_RUN, LED_OFF);
//-		port_pin_set_output_level(LED_ALARM, LED_OFF);
//-		port_pin_set_output_level(LED_TRIGGER, LED_OFF);
//-		port_pin_set_output_level(LED_DELAY, LED_OFF);
		fnd_status_data &= ~(SEG_RUN);
		fnd_status_data &= ~(SEG_ALARM);
		fnd_status_data &= ~(SEG_TRRIGER);
		fnd_status_data &= ~(SEG_DELAY);
		if(mode_blink == 0)
		{
//-			port_pin_set_output_level(LED_ALARM, LED_ON);
			fnd_status_data |= (SEG_ALARM);
			buzzer_on();
		}
		else
		{
//-			port_pin_set_output_level(LED_ALARM, LED_OFF);
			fnd_status_data &= ~(SEG_ALARM);
			buzzer_off();
		}

	}
	else if(sys_status == SYS_ERROR)
	{
//-		port_pin_set_output_level(LED_RUN, LED_OFF);
//-		port_pin_set_output_level(LED_ALARM, LED_ON);
//-		port_pin_set_output_level(LED_TRIGGER, LED_OFF);
//-		port_pin_set_output_level(LED_DELAY, LED_OFF);
		fnd_status_data &= ~(SEG_RUN);
		fnd_status_data |= (SEG_ALARM);
		fnd_status_data &= ~(SEG_TRRIGER);
		fnd_status_data &= ~(SEG_DELAY);
		if(mode_blink == 0)
		{
//-			port_pin_set_output_level(LED_ALARM, LED_ON);
			fnd_status_data |= (SEG_ALARM);
			buzzer_on();
		}
		else
		{
//-			port_pin_set_output_level(LED_ALARM, LED_OFF);
			fnd_status_data &= ~(SEG_ALARM);
			buzzer_off();
		}

	}
	
}


void check_input(void)
{
	if(key_tick < 255)	key_tick++;
		
	check_sw_input();
	
	if(sys_status == SYS_RUN)
	{
		if(set_key_hold == 1)
		{
			set_key_hold = 0;
			sys_status = SYS_SETUP;
			if(run_mode == MODE_DELAY)
			{
				setup_mode = MODE_DELAY;
				setup_status = SETUP_DELAY;
				temp_time = limit_delay_time1;
			}
			else if(run_mode == MODE_TRIGGER)
			{
				setup_mode = MODE_TRIGGER;
				setup_status = SETUP_WELD;
				temp_time = limit_trigger_time2;
			}
			
			sw_set_pressed = 0;
		}
		if(sw_set_pressed)
		{
			sw_set_pressed = 0;
		}
		if(sw_mode_pressed) {
			sw_mode_pressed = 0;
			if(run_mode == MODE_TRIGGER)
				run_mode = MODE_DELAY;
			else
				run_mode = MODE_TRIGGER;
				
			save_byte_fram(ADDR_RUN_MODE, run_mode, fram_mode);

		}

		if(cnt_reset_key_hold == 1)
		{
			cnt_reset_key_hold = 0;
			sw_cnt_reset_pressed = 0;
			if(work_cnt != 0)
			{
				work_cnt = 0;
				save_word_fram(ADDR_WORK_CNT, work_cnt, fram_mode);
			}
		}
		
		if(mode_key_hold == 1)
		{
			mode_key_hold = 0;
			sys_status = SYS_HORN;
			horn_status = 0;	// solenoid off
			re_start1_pressed = re_start2_pressed = 0;
			sw_mode_pressed = 0;
		}
		
	}
	else if(sys_status == SYS_SETUP)
	{
		if(sw_mode_pressed) {
			sw_mode_pressed = 0;
			if(setup_mode == MODE_TRIGGER)
			{
				setup_mode = MODE_DELAY;
				setup_status = SETUP_DELAY;
				temp_time = limit_delay_time1;	
			}
			else
			{
				setup_mode = MODE_TRIGGER;
				setup_status = SETUP_WELD;
				temp_time = limit_trigger_time2;	
			}
		}
		
		if(sw_up_pressed)
		{
			sw_up_pressed = 0;
			if(setup_status != SETUP_SAFTY)
			{
				if(temp_time < 9998) temp_time++;
			}
			else
			{
				if(temp_time == 0)
					temp_time = 1;
				else
					temp_time = 0;		
			}
		}
		if(sw_down_pressed)
		{
			sw_down_pressed = 0;
			if(setup_status != SETUP_SAFTY)
			{
				if(temp_time > 0) temp_time--;
			}
			else
			{
				if(temp_time == 0)
					temp_time = 1;
				else
					temp_time = 0;		
			}
		}	
		
		if(sw_set_pressed)
		{
			sw_set_pressed = 0;
			if(setup_status == SETUP_DELAY)
			{
				if(setup_mode == MODE_DELAY)
				{
					if(limit_delay_time1 != temp_time)
					{
						limit_delay_time1 = temp_time;
						save_param_fram(ADDR_DELAY1, limit_delay_time1, fram_mode);
					}
					if(limit_delay_time2 > 6){
					temp_time = limit_delay_time2;
						comp_time = 7;
					} else {
						comp_time = limit_delay_time2;
						temp_time = 7;
					}
				}
				setup_status = SETUP_WELD;
			}
			else if(setup_status == SETUP_WELD)
			{
				if(setup_mode == MODE_DELAY)
				{
					if(limit_delay_time2 != temp_time)
					{
						limit_delay_time2 = temp_time;
						save_param_fram(ADDR_DELAY2, limit_delay_time2, fram_mode);
					}
					temp_time = limit_delay_time3;
				}
				else
				{
					if(limit_trigger_time2 != temp_time)
					{
						limit_trigger_time2 = temp_time;
						save_param_fram(ADDR_TRIGGER2, limit_trigger_time2, fram_mode);
					}
					temp_time = limit_trigger_time3;
				}
				setup_status = SETUP_HOLD;
			}
			else if(setup_status == SETUP_HOLD)
			{
				if(setup_mode == MODE_DELAY)
				{
					if(limit_delay_time3 != temp_time)
					{
						limit_delay_time3 = temp_time;
						save_param_fram(ADDR_DELAY3, limit_delay_time3, fram_mode);
					}
				}
				else
				{
					if(limit_trigger_time3 != temp_time)
					{
						limit_trigger_time3 = temp_time;
						save_param_fram(ADDR_TRIGGER3, limit_trigger_time3, fram_mode);
					}
				}
				temp_time = (uint16_t)f_safty;
				setup_status = SETUP_SAFTY;
			}
			else if(setup_status == SETUP_SAFTY)
			{
				if(f_safty != LSB(temp_time))
				{
					f_safty = LSB(temp_time);
					save_byte_fram(ADDR_SAFTY, f_safty, fram_mode);
				}
				sys_status = SYS_RUN;
				set_key_hold = 0;
			}
		}		
	}
	else if(sys_status == SYS_HORN)
	{
		if(sw_mode_pressed) {
			sw_mode_pressed = 0;
			sys_status = SYS_RUN;
			horn_status = 0;
			port_pin_set_output_level(SOL_DN, SOL_OFF);
		}
		if(sw_up_pressed)
		{
			sw_up_pressed = 0;
				
		}
		if(sw_down_pressed)
		{
			sw_down_pressed = 0;
		}			
	}
		
}
void send_model_str(uint8_t freq, uint8_t type)
{
	uint8_t temp_str[11], i;
#ifdef GDSONIC	
	temp_str[0] = 'G';
	temp_str[1] = 'D';
	temp_str[2] = 'S';
	temp_str[3] = '-';
	if(freq == 0)
	{
		temp_str[4] = '1'; temp_str[5] = '5';
	}
	else if(freq == 1)
	{
		temp_str[4] = '2'; temp_str[5] = '0';
	}
	else if(freq == 2)
	{
		temp_str[4] = '3'; temp_str[5] = '0';
	}
	else if(freq == 3)
	{
		temp_str[4] = '3'; temp_str[5] = '5';
	}
	else if(freq == 4)
	{
		temp_str[4] = '4'; temp_str[5] = '0';
	}
	else if(freq == 5)
	{
		temp_str[4] = '5'; temp_str[5] = '0';
	}

	if(type == 0)
	{
		temp_str[6] = 'H'; 
	}
	else if(type == 1)
	{
		temp_str[6] = 'M'; 
	}
	else if(type == 2)
	{
		temp_str[6] = 'S'; 
	}
	temp_str[7] = ' '; 
	temp_str[8] = ' '; 
	temp_str[9] = ' '; 
	temp_str[10] = '\0';
	
	send_lcd_byte_array(MODEL_NAME, 11, (uint8_t *)temp_str);
#endif
#ifdef DIAMT
	temp_str[0] = 'D';
	temp_str[1] = 'I';
	temp_str[2] = 'S';
	temp_str[3] = '-';
	if(freq == 0)
	{
		temp_str[4] = '1'; temp_str[5] = '5';
	}
	else if(freq == 1)
	{
		temp_str[4] = '2'; temp_str[5] = '0';
	}
	else if(freq == 2)
	{
		temp_str[4] = '3'; temp_str[5] = '0';
	}
	else if(freq == 3)
	{
		temp_str[4] = '3'; temp_str[5] = '5';
	}
	else if(freq == 4)
	{
		temp_str[4] = '4'; temp_str[5] = '0';
	}
	else if(freq == 5)
	{
		temp_str[4] = '5'; temp_str[5] = '0';
	}

	if(type == 0)
	{
		temp_str[6] = 'H';
	}
	else if(type == 1)
	{
		temp_str[6] = 'M';
	}
	else if(type == 2)
	{
		temp_str[6] = 'S';
	}
	temp_str[7] = ' ';
	temp_str[8] = ' ';
	temp_str[9] = ' ';
	temp_str[10] = '\0';

	send_lcd_byte_array(MODEL_NAME, 11, (uint8_t *)temp_str);
#endif
#ifdef POWERTECH
	temp_str[0] = 'P';
	temp_str[1] = 'T';
	temp_str[2] = 'W';
	temp_str[3] = '-';
	if(freq == 0)
	{ // 15K
		temp_str[4] = '2'; temp_str[5] = '5';
		temp_str[6] = '1'; temp_str[7] = '5';
	}
	else if(freq == 1)
	{	//20K
		temp_str[4] = '2'; temp_str[5] = '0';
		temp_str[6] = '2'; temp_str[7] = '0';
	}
	else if(freq == 2)
	{	// 30K
		temp_str[4] = ' '; temp_str[5] = '7';
		temp_str[6] = '3'; temp_str[7] = '5';
	}
	else if(freq == 3)
	{	// 35K
		temp_str[4] = ' '; temp_str[5] = '7';
		temp_str[6] = '3'; temp_str[7] = '5';
	}
	else if(freq == 4)
	{	// 40K
		temp_str[4] = ' '; temp_str[5] = '7';
		temp_str[6] = '4'; temp_str[7] = '0';
	}
	else if(freq == 5)
	{	// 50K
		temp_str[4] = ' '; temp_str[5] = '7';
		temp_str[6] = '5'; temp_str[7] = '0';
	}

	if(type == 0)
	{
		temp_str[8] = 'D';
		temp_str[9] = 'H';
	}
	else if(type == 1)
	{
		temp_str[8] = 'M';
		temp_str[9] = 'D';
	}
	else if(type == 2)
	{
		temp_str[8] = 'S';
		temp_str[9] = 'D';
	}
	temp_str[10] = '\0';

	send_lcd_byte_array(MODEL_NAME, 11, (uint8_t *)temp_str);
#endif
#ifdef MOOHAN
	temp_str[0] = 'M';
	temp_str[1] = 'H';
	temp_str[2] = '-';
	if(freq == 0)
	{ // 15K
		temp_str[3] = '1'; temp_str[4] = '5';
		temp_str[5] = '1'; temp_str[6] = '5';
	}
	else if(freq == 1)
	{	//20K
		temp_str[3] = '1'; temp_str[4] = '5';
		temp_str[5] = '2'; temp_str[6] = '0';
	}
	else if(freq == 2)
	{	// 30K
		temp_str[3] = '1'; temp_str[4] = '5';
		temp_str[5] = '3'; temp_str[6] = '5';
	}
	else if(freq == 3)
	{	// 35K
		temp_str[3] = '1'; temp_str[4] = '5';
		temp_str[5] = '3'; temp_str[6] = '5';
	}
	else if(freq == 4)
	{	// 40K
		temp_str[3] = '1'; temp_str[4] = '5';
		temp_str[5] = '4'; temp_str[6] = '0';
	}
	else if(freq == 5)
	{	// 50K
		temp_str[3] = '1'; temp_str[4] = '5';
		temp_str[5] = '5'; temp_str[6] = '0';
	}

	if(type == 0)
	{
		temp_str[7] = 'D';
		temp_str[8] = 'H';
	}
	else if(type == 1)
	{
		temp_str[7] = 'D';
		temp_str[8] = 'M';
	}
	else if(type == 2)
	{
		temp_str[7] = 'D';
		temp_str[8] = 'S';
	}
	temp_str[9] = ' ';
	temp_str[10] = '\0';

	send_lcd_byte_array(MODEL_NAME, 11, (uint8_t *)temp_str);
#endif

}



void lcd_var_init(void)
{
	uint16_t int_data[2];
	
	int_data[0] = 0x0003;
	int_data[1] = 0x0004;
	send_lcd_int_array(KEY_MULTI+1, 2, (uint16_t *)int_data);
	int_data[0] = 0x0000;
	int_data[1] = 0x0002;
	send_lcd_int_array(SETUP_MODEL+1, 2, (uint16_t *)int_data);

}

void send_outpower_data(uint8_t step, uint16_t current_data, uint8_t ref_data)
{
	uint8_t i, temp_byte;
	uint16_t temp_i;
/*	
	temp_i = current_data * 20;
	temp_byte = (uint8_t)(temp_i / 100);
	
	for(i = 0 ; i < 20 ; i++)
	level_buf[i] = (i < temp_byte) ? 1 : 0;
*/
	if(step == 0)
	{
		if(current_data > 10)
		{
			level_buf[0] = 1;
			if(current_data > ref_lv_1)
			{
				level_buf[1] = 1;
				if(current_data > ref_lv_10)
				{
					if(current_data >= ref_lv_20)
					{
						for(i = 1 ; i < 20 ; i++)
						level_buf[i] = 1;
					}
					else
					{
						for(i = 2 ; i < 10 ; i++)
						level_buf[i] = 1;
						temp_byte = ((current_data - ref_lv_10) * 10 ) / (ref_lv_20 - ref_lv_10);
						for(i = 0 ; i < 10 ; i++)
						level_buf[10+i] = (i < temp_byte) ? 1 : 0;
					}
				}
				else
				{
					temp_byte = ((current_data - ref_lv_1) * 8 ) / (ref_lv_10 - ref_lv_1);
					for(i = 0 ; i < 18 ; i++)
					level_buf[2+i] = (i < temp_byte) ? 1 : 0;
				}
			}
			else
			{
				for(i = 1 ; i < 20 ; i++)
				level_buf[i] = 0;
			}
		}
		else
		{
			for(i = 0 ; i < 20 ; i++)
			level_buf[i] = 0;
		}
		temp_i = ref_data * 20;
		temp_byte = (uint8_t)(temp_i / 100);
		if(temp_byte >= 20) temp_byte = 19;
		level_buf[temp_byte] = 1;
	}
	else if (step == 1)
	{
		send_lcd_int_array(LV_OUTPUT, 5, (uint16_t *)level_buf);
	}
	else if (step == 2)
	{
		send_lcd_int_array(LV_OUTPUT+5, 5, (uint16_t *)&level_buf[5]);		
	}
	else if (step == 3)
	{
		send_lcd_int_array(LV_OUTPUT+10, 5, (uint16_t *)&level_buf[10]);
	}
	else if (step == 4)
	{
		send_lcd_int_array(LV_OUTPUT+15, 5, (uint16_t *)&level_buf[15]);
	}
}

void send_outtime_data(uint8_t step, uint8_t current_time)
{
	uint8_t i;
	
	if(step == 5)
	{
		for(i = 0 ; i < 20 ; i++)
			out_time_buf[i] = (i < current_time) ? 1 : 0;
	}
	else if(step == 6)
	{
		send_lcd_int_array(LV_TIME, 5, (uint16_t *)out_time_buf);
	}
	else if(step == 7)
	{
		send_lcd_int_array(LV_TIME+5, 5, (uint16_t *)&out_time_buf[5]);
	}
	else if(step == 8)
	{
		send_lcd_int_array(LV_TIME+10, 5, (uint16_t *)&out_time_buf[10]);
	}
	else if(step == 9)
	{
		send_lcd_int_array(LV_TIME+15, 5, (uint16_t *)&out_time_buf[15]);
	}
}

uint8_t time2str(uint16_t src, unsigned char *dest_str)
{
	uint8_t nibble, first_zero, pos, temp;
	uint16_t temp_i;
	
	first_zero = pos = 0;
	
	nibble = src / 10000 ;
	temp_i = src % 10000;
	if(nibble != 0)
	{
		dest_str[pos++] = nibble + '0';
		first_zero = 1;
	}

	nibble = temp_i / 1000 ;
	temp_i = temp_i % 1000;
	if(nibble != 0)
	{
		dest_str[pos++] = nibble + '0';
		first_zero = 1;
	}
	else
	{
		if(first_zero == 1)
		{
			dest_str[pos++] = '0';
		}
	}
	
	nibble = temp_i / 100 ;
	temp_i = temp_i % 100;
	dest_str[pos++] = nibble + '0';
	
	dest_str[pos++] = '.';
	
	nibble = temp_i / 10 ;
	temp_i = temp_i % 10;
	dest_str[pos++] = nibble + '0';
	
	dest_str[pos++] = (uint8_t)(temp_i + '0');
	dest_str[pos++] = (uint8_t)'\0';

	return pos;
}

uint8_t energy2str(uint32_t src, unsigned char *dest_str)
{
	uint8_t nibble, first_zero, pos, temp;
	uint16_t temp_i;
	
	first_zero = pos = 0;
	
	nibble = src / 100000 ;
	src = src % 100000;
	if(nibble != 0)
	{
		dest_str[pos++] = nibble + '0';
		first_zero = 1;
	}

	nibble = src / 10000 ;
	temp_i = src % 10000;
	if(nibble != 0)
	{
		dest_str[pos++] = nibble + '0';
		first_zero = 1;
	}

	nibble = temp_i / 1000 ;
	temp_i = temp_i % 1000;
	if(nibble != 0)
	{
		dest_str[pos++] = nibble + '0';
		first_zero = 1;
	}
	else
	{
		if(first_zero == 1)
		{
			dest_str[pos++] = '0';
		}
	}
	
	nibble = temp_i / 100 ;
	temp_i = temp_i % 100;
	if(nibble != 0)
	{
		dest_str[pos++] = nibble + '0';
		first_zero = 1;
	}
	else
	{
		if(first_zero == 1)
		{
			dest_str[pos++] = '0';
		}
	}
	
	nibble = temp_i / 10 ;
	temp_i = temp_i % 10;
	if(nibble != 0)
	{
		dest_str[pos++] = nibble + '0';
		first_zero = 1;
	}
	else
	{
		if(first_zero == 1)
		{
			dest_str[pos++] = '0';
		}
	}
	
	dest_str[pos++] = (uint8_t)(temp_i + '0');
	dest_str[pos++] = (uint8_t)'\0';
	
	return pos;
}

void conv_addr2str(uint8_t addr, uint8_t *buff)
{
	uint8_t temp;
	
	if(addr == 0)
	{
		buff[0] = 'N';
		buff[1] = 'O';
		buff[2] = 'N';
		buff[3] = 'E';
	}
	else
	{
		buff[0] = ' ';
		buff[1] = '0' + addr/100;
		temp = addr % 100;
		buff[2] = '0' + temp / 10;
		temp = temp % 10;
		buff[3] = '0' + temp;
	}
}

void process_ip_char(char c) {
    // ���� ó��
    if (c >= 0 && c <= 9) {
        // ���� ���ݿ� ���� �߰�
        ether_current_number = ether_current_number * 10 + (c);
        
        // ��ȿ�� IP �ּ� ���� üũ (0-255)
        if (ether_current_number > 255) {
            ether_current_number = 255;  // 255�� ����
        }
        
        // �Է� ���ۿ� ���� �߰�
        if (ether_buffer_pos < 15) {
            ether_input_buffer[ether_buffer_pos++] = c + '0';
            ether_input_buffer[ether_buffer_pos] = '\0';
        }
        
        ether_has_input = 1;
    }
    // ��(.) ó�� - ���� �������� �̵�
    else if (c == 'D') {
        // ���� ���ݿ� ���ڰ� �ְ�, ���� 4���� ������ ��� ó������ �ʾҴٸ�
        if (ether_has_input && ether_current_octet < 3) {
            // ���� ������ �����ϰ� ���� �������� �̵�
            ether_temp_ip[ether_current_octet] = ether_current_number;
            ether_current_octet++;
            ether_current_number = 0;
            ether_has_input = 0;
            
            // �Է� ���ۿ� �� �߰�
            if (ether_buffer_pos < 15) {
                ether_input_buffer[ether_buffer_pos++] = '.';
                ether_input_buffer[ether_buffer_pos] = '\0';
            }
        }
    }
    // �齺���̽� ó��
    else if ( c == 'B' ) {  // ASCII �齺���̽� �Ǵ� DEL
        if (ether_buffer_pos > 0) {
            // ������ ���ڰ� ���̾����� Ȯ��
            if ((ether_buffer_pos > 0) && (ether_input_buffer[ether_buffer_pos-1] == '.')) {
                // ���� �������� ���ư���
                if (ether_current_octet > 0) {
                    ether_current_octet--;
                    ether_current_number = ether_temp_ip[ether_current_octet];
                    ether_has_input = 1;
                }
            } else {
                // ���� ������ ������ �ڸ� ���� ����
                ether_current_number /= 10;
                ether_has_input = (ether_current_number > 0) ? 1 : 0;
            }
            // �Է� ���ۿ��� ������ ���� ����
            ether_buffer_pos--;
            ether_input_buffer[ether_buffer_pos] = '\0';
        }
    }
    // ����Ű ó�� - IP �ּ� Ȯ��
    else if (c == 'E') {
        // ������ ���ݿ� �Է��� ������ ����
        if (ether_has_input) {
            ether_temp_ip[ether_current_octet] = ether_current_number;
        }
        
        // IP �ּ� �Է��� �Ϸ�Ǿ����� ǥ��
        ether_ip_input_complete = true;
        
        // ���� �ʱ�ȭ
        //ether_current_number = 0;
        //ether_current_octet = 0;
        //ether_has_input = 0;
        //ether_buffer_pos = 0;
        //ether_input_buffer[0] = '\0';
    }
    // �� ���� ���ڴ� ����
}

uint8_t ip_to_string(uint8_t ip_bytes[4], char *str_buffer) {
	uint8_t i;
	
    if (str_buffer == NULL) {
        return 0;  // ���۰� NULL�� ��� ����
    }
    
    int len = sprintf(str_buffer, "%d.%d.%d.%d", 
                      ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3]);
    for(i = len ; i < 16 ; i++)
		str_buffer[i] = 0;
    return 16;  // ������ ���ڿ��� ���� ��ȯ
}


void change_lcd_page(uint8_t lcd_page)
{
	uint16_t temp_i;
	uint8_t temp, temp_str[4];
	
	if(energy_ctrl == true)
		send_lcd_data_var(DISP_ENERGY_EN, (uint16_t)1);
	else
		send_lcd_data_var(DISP_ENERGY_EN, (uint16_t)0);

	if(multi_ctrl == true)
		send_lcd_data_var(DISP_MULTI_EN, (uint16_t)1);
	else
		send_lcd_data_var(DISP_MULTI_EN, (uint16_t)0);
	
	
	if(lcd_page == LCD_RUN_STD)
	{
		send_lcd_data_var(LV_DM_DELAY, (uint16_t)limit_delay_time1);
		send_lcd_data_var(DISP_RUN_MODE, (uint16_t)run_mode);
		send_lcd_data_var(DISP_SAFTY, (uint16_t)f_safty);
		send_lcd_data_var(LV_LIMIT_OUT_T, (uint16_t)limit_out_time);
		if(run_mode == MODE_DELAY)
		{
			lcd_temp_buf[0] = 'D';
			lcd_temp_buf[1] = ' ';
			lcd_temp_buf[2] = ':';
			lcd_temp_buf[3] = ' ';
			temp = time2str(limit_delay_time1, (uint8_t *)&lcd_temp_buf[4]);
			send_lcd_byte_array(DISP_STD_DATA1, temp+4, lcd_temp_buf);
			if(energy_ctrl == true)
			{
				lcd_temp_buf[0] = 'E';
				lcd_temp_buf[1] = ' ';
				lcd_temp_buf[2] = ':';
				lcd_temp_buf[3] = ' ';
				temp = energy2str(limit_energy, (uint8_t *)&lcd_temp_buf[4]);
				send_lcd_byte_array(DISP_STD_DATA2, temp+4, lcd_temp_buf);
			}
			else
			{
				lcd_temp_buf[0] = 'W';
				lcd_temp_buf[1] = ' ';
				lcd_temp_buf[2] = ':';
				lcd_temp_buf[3] = ' ';
				if(multi_ctrl == true)
					temp = time2str(limit_mo_time1+limit_mo_time2, (uint8_t *)&lcd_temp_buf[4]);
				else
					temp = time2str(limit_delay_time2, (uint8_t *)&lcd_temp_buf[4]);
				send_lcd_byte_array(DISP_STD_DATA2, temp+4, lcd_temp_buf);
			}
			lcd_temp_buf[0] = 'H';
			lcd_temp_buf[1] = ' ';
			lcd_temp_buf[2] = ':';
			lcd_temp_buf[3] = ' ';
			temp = time2str(limit_delay_time3, (uint8_t *)&lcd_temp_buf[4]);
			send_lcd_byte_array(DISP_STD_DATA3, temp+4, lcd_temp_buf);
		}
		else if(run_mode == MODE_TRIGGER)
		{
			lcd_temp_buf[0] = 'S';
			lcd_temp_buf[1] = 'E';
			lcd_temp_buf[2] = 'N';
			lcd_temp_buf[3] = 'S';
			lcd_temp_buf[4] = 'O';
			lcd_temp_buf[5] = 'R';
			lcd_temp_buf[6] = ' ';
			lcd_temp_buf[7] = 'O';
			lcd_temp_buf[8] = 'F';
			lcd_temp_buf[9] = 'F';
			lcd_temp_buf[10] = '\0';
			send_lcd_byte_array(DISP_STD_DATA1, 11, lcd_temp_buf);
			if(energy_ctrl == true)
			{
				lcd_temp_buf[0] = 'E';
				lcd_temp_buf[1] = ' ';
				lcd_temp_buf[2] = ':';
				lcd_temp_buf[3] = ' ';
				temp = energy2str(limit_energy, (uint8_t *)&lcd_temp_buf[4]);
				send_lcd_byte_array(DISP_STD_DATA2, temp+4, lcd_temp_buf);
			}
			else
			{
				lcd_temp_buf[0] = 'W';
				lcd_temp_buf[1] = ' ';
				lcd_temp_buf[2] = ':';
				lcd_temp_buf[3] = ' ';
				if(multi_ctrl == true)
					temp = time2str(limit_mo_time1+limit_mo_time2, (uint8_t *)&lcd_temp_buf[4]);
				else
					temp = time2str(limit_trigger_time2, (uint8_t *)&lcd_temp_buf[4]);
				send_lcd_byte_array(DISP_STD_DATA2, temp+4, lcd_temp_buf);
			}
			lcd_temp_buf[0] = 'H';
			lcd_temp_buf[1] = ' ';
			lcd_temp_buf[2] = ':';
			lcd_temp_buf[3] = ' ';
			temp = time2str(limit_trigger_time3, (uint8_t *)&lcd_temp_buf[4]);
			send_lcd_byte_array(DISP_STD_DATA3, temp+4, lcd_temp_buf);
			
		}
	}
	else if((lcd_page == LCD_SETUP_HAND)||(lcd_page == LCD_SETUP_MULTI)||(lcd_page == LCD_SETUP_STD1))
	{
		send_lcd_byte_array(DISP_VERSION, 20, VERSION_MSG);
		send_lcd_data_var(LV_MAX_ON_TIME, (uint16_t)(limit_on_time));
		send_lcd_data_var(LV_OUT_POWER, (uint16_t)(output_power));
		temp_i = (((output_power-50) * 255) / 100 );
		i2c_adc_write_byte(I2C_POT, I2C_POT_WP, (uint8_t)temp_i);
		send_lcd_data_word(LV_ENERGY_VAL, (uint32_t)limit_energy);
		send_lcd_data_var(LV_ENERGY_EDIT, (uint16_t)(limit_energy));
		if(energy_ctrl == true)
			send_lcd_data_var(DISP_ENERGY_EN, (uint16_t)1);
		else
			send_lcd_data_var(DISP_ENERGY_EN, (uint16_t)0);

		send_lcd_data_var(LV_LIMIT_OUT_T, (uint16_t)limit_out_time);
		temp_parity_idx = comm_parity_idx;
		temp_address = comm_address;
		temp_speed_idx = comm_speed_idx;
		temp_cnt_reset = 0;
		temp_comm_mode = 0xff;
	}
	else if(lcd_page == LCD_SETUP_STD2D)
	{
		send_lcd_data_var(LV_DM_DELAY, (uint16_t)limit_delay_time1);
		send_lcd_data_var(LV_DM_WELD, (uint16_t)limit_delay_time2);
		send_lcd_data_var(LV_DM_HOLD, (uint16_t)limit_delay_time3);
		temp_comm_mode = 0xff;
	}
	else if(lcd_page == LCD_SETUP_STD2T)
	{
		send_lcd_data_var(LV_TM_WELD, (uint16_t)limit_trigger_time2);
		send_lcd_data_var(LV_TM_HOLD, (uint16_t)limit_trigger_time3);
		temp_comm_mode = 0xff;
	}
	else if((lcd_page == LCD_SETUP_STD3)||(lcd_page == LCD_SETUP_MH2))
	{
		if(multi_ctrl == true)
			send_lcd_data_var(DISP_MULTI_EN, (uint16_t)1);
		else
 			send_lcd_data_var(DISP_MULTI_EN, (uint16_t)0);
		send_lcd_data_var(LV_MO_OUT1, (uint16_t)limit_mo_out1);
		send_lcd_data_var(LV_MO_OUT2, (uint16_t)limit_mo_out2);
		send_lcd_data_var(LV_MO_TIME1, (uint16_t)limit_mo_time1);
		send_lcd_data_var(LV_MO_TIME2, (uint16_t)limit_mo_time2);
		temp_comm_mode = 0xff;
	}
	else if((lcd_page == LCD_SETUP_STDC)||(lcd_page == LCD_SETUP_MHC))
	{
		send_lcd_data_var(COMM_ADDR, (uint16_t)comm_address);
		send_lcd_data_var(COMM_SPEED, (uint16_t)comm_speed_idx);
		send_lcd_data_var(COMM_PARITY, (uint16_t)comm_parity_idx);
		temp_parity_idx = comm_parity_idx;
		temp_address = comm_address;
		temp_speed_idx = comm_speed_idx;
		conv_addr2str(comm_address, (uint8_t *)temp_str);
		send_lcd_byte_array(COMM_ADDR_TXT, 4, (uint8_t *)temp_str);
		send_lcd_byte_array(COMM_SPEED_TXT, 6, (uint8_t *)comm_speed_txt[comm_speed_idx]);
		send_lcd_byte_array(COMM_PARITY_TXT, 4, (uint8_t *)comm_parity_txt[comm_parity_idx]);
		if(temp_comm_mode == 0xff)
		{
			temp_comm_mode = comm_mode;
			for(temp_i = 0 ; temp_i < 4 ; temp_i++)
			{
				temp_ether_ip[temp_i] = ether_ip[temp_i];
				temp_ether_nm[temp_i] = ether_nm[temp_i];
				temp_ether_gw[temp_i] = ether_gw[temp_i];
			}
			temp = ip_to_string(temp_ether_ip, lcd_temp_buf);
			send_lcd_byte_array(COMM_IP_TXT, temp, (uint8_t *)lcd_temp_buf);
			temp = ip_to_string(temp_ether_nm, lcd_temp_buf);
			send_lcd_byte_array(COMM_NM_TXT, temp, (uint8_t *)lcd_temp_buf);
			temp = ip_to_string(temp_ether_gw, lcd_temp_buf);
			send_lcd_byte_array(COMM_GW_TXT, temp, (uint8_t *)lcd_temp_buf);
			ether_current_number = 0;
			ether_current_octet = 0;
			ether_has_input = false;
			ether_ip_input_complete = false;
			ether_buffer_pos = 0;
			ether_what_input = ETHER_INPUT_NONE;
		}
		if(temp_comm_mode > 0)
			send_lcd_data_var(DISP_COMM_MODE, 1);
		else
			send_lcd_data_var(DISP_COMM_MODE, 0);			
	}
	else if((lcd_page == LCD_SETUP_STDE)||(lcd_page == LCD_SETUP_MHE))
	{
		send_lcd_data_var(COMM_ADDR, (uint16_t)comm_address);
		send_lcd_data_var(COMM_SPEED, (uint16_t)comm_speed_idx);
		send_lcd_data_var(COMM_PARITY, (uint16_t)comm_parity_idx);
		temp_parity_idx = comm_parity_idx;
		temp_address = comm_address;
		temp_speed_idx = comm_speed_idx;
		conv_addr2str(comm_address, (uint8_t *)temp_str);
		send_lcd_byte_array(COMM_ADDR_TXT, 4, (uint8_t *)temp_str);
		send_lcd_byte_array(COMM_SPEED_TXT, 6, (uint8_t *)comm_speed_txt[comm_speed_idx]);
		send_lcd_byte_array(COMM_PARITY_TXT, 4, (uint8_t *)comm_parity_txt[comm_parity_idx]);
		if(temp_comm_mode == 0xff)
		{
			temp_comm_mode = comm_mode;
			for(temp_i = 0 ; temp_i < 4 ; temp_i++)
			{
				temp_ether_ip[temp_i] = ether_ip[temp_i];
				temp_ether_nm[temp_i] = ether_nm[temp_i];
				temp_ether_gw[temp_i] = ether_gw[temp_i];
			}
			temp = ip_to_string(temp_ether_ip, lcd_temp_buf);
			send_lcd_byte_array(COMM_IP_TXT, temp, (uint8_t *)lcd_temp_buf);
			temp = ip_to_string(temp_ether_nm, lcd_temp_buf);
			send_lcd_byte_array(COMM_NM_TXT, temp, (uint8_t *)lcd_temp_buf);
			temp = ip_to_string(temp_ether_gw, lcd_temp_buf);
			send_lcd_byte_array(COMM_GW_TXT, temp, (uint8_t *)lcd_temp_buf);
			ether_current_number = 0;
			ether_current_octet = 0;
			ether_has_input = false;
			ether_ip_input_complete = false;
			ether_buffer_pos = 0;
			ether_what_input = ETHER_INPUT_NONE;
		}
		if(temp_comm_mode == 0)
			send_lcd_data_var(DISP_COMM_MODE, 0);
		else
		{
			send_lcd_data_var(DISP_COMM_MODE, 1);
			send_lcd_data_var(DISP_EN_DHCP, (uint16_t)(temp_comm_mode - 1));
		}
	}
	
	set_lcd_page(lcd_page);
}

void init_lcd_mode(void)
{
	uint8_t temp_array[4];
	send_model_str(model_freq, model_type);


	if(model_type == 0)		//hand
		lcd_status = LCD_RUN_HAND;
	else if(model_type == 1)		//multi
	
		lcd_status = LCD_RUN_MULTI;
	else		//standard
		lcd_status = LCD_RUN_STD;
		
	sys_mode = model_type;
	
	if(model_freq == 0)	// 15K
	{
		ref_lv_1 = 50;		// 0.5A
		ref_lv_2 = 100;		// 1A
		ref_lv_10 = 1000;	// 10A
		ref_lv_20 = 2000;	// 20A
	}
	else if(model_freq == 1)	// 20K
	{
		ref_lv_1 = 50;		// 0.5A
		ref_lv_2 = 100;		// 1A
		ref_lv_10 = 600;	// 6A
		ref_lv_20 = 1200;	// 12A
	}
	else				// above 30k
	{
		ref_lv_1 = 50;		// 0.5A
		ref_lv_2 = 100;		// 1A
		ref_lv_10 = 400;	// 4A
		ref_lv_20 = 700;	// 7A
	}

//	if(lcd_status == LCD_RUN_MULTI)
//	{
//		set_lcd_page(LCD_RUN_MULTI);
		send_lcd_data_var(ICON_RESET, (uint16_t)0);
		send_lcd_data_var(ICON_SEEK, (uint16_t)0);
		send_lcd_data_var(ICON_RUN, (uint16_t)0);
		send_lcd_data_var(LV_DM_DELAY, (uint16_t)limit_delay_time1);
		send_lcd_data_var(LV_DM_WELD, (uint16_t)limit_delay_time2);
		send_lcd_data_var(LV_DM_HOLD, (uint16_t)limit_delay_time3);
		send_lcd_data_var(LV_TM_WELD, (uint16_t)limit_trigger_time2);
		send_lcd_data_var(LV_TM_HOLD, (uint16_t)limit_trigger_time3);
		send_lcd_data_word(LV_WORK_CNT, (uint32_t)work_cnt);
		send_lcd_data_var(LV_ENERGY_EDIT, (uint16_t)(limit_energy /10));
//		send_lcd_data_var(VV_OUTPOWER, (uint16_t)(100 - output_power));
//		send_lcd_data_word(VV_LIMITENERGY, (uint32_t)(10000 - limit_energy));
		send_lcd_data_var(DISP_HORNDOWN, (uint16_t)0);

		change_lcd_page(lcd_status);
		
//		send_lcd_data_var(ICON_OL, (uint16_t)0);
//	}
}

void lcd_data_pdd(uint8_t src_length, uint8_t *src_data)
{
	uint8_t i, str_finish;
	for(i = 0, str_finish = 0 ; i < src_length ; i++)
	{
		if(src_data[i] == '\0')
			str_finish = 1;
		//lcd_temp_buf[i] = (str_finish) ? ' ' : src_data[i];
		lcd_temp_buf[i] = (str_finish) ? '\0' : src_data[i];
	}
}

void parse_lcd_comm(void)
{
	uint16_t lcd_data_addr, lcd_data, lcd_word, temp_i;
	int16_t temp_int;
	uint8_t temp_str[16], i;
//	if(lcd_buf[LCD_COMM_MODE] == LCD_WR)
//	{
//		if(check_lcd_comm() == 0)
//			return;
//	}	

	
	if(lcd_buf[LCD_COMM_MODE] == LCD_RD)
	{
		MSB(lcd_data_addr) = lcd_buf[LCD_COMM_ADDR_H];
		LSB(lcd_data_addr) = lcd_buf[LCD_COMM_ADDR_L];
		MSB(lcd_data) = lcd_buf[LCD_COMM_DATA_H];
		LSB(lcd_data) = lcd_buf[LCD_COMM_DATA_L];
		MSB0W(lcd_word) = lcd_buf[LCD_COMM_DATA_H];
		MSB1W(lcd_word) = lcd_buf[LCD_COMM_DATA_L];
		MSB2W(lcd_word) = lcd_buf[LCD_COMM_DATA_H + 2];  
		MSB3W(lcd_word) = lcd_buf[LCD_COMM_DATA_L + 2];
		
 		switch(lcd_data_addr)
		{
			case SYS_PIC_NOW:
				if(lcd_data == 0)
				{
					lcd_var_init();
					send_model_str(model_freq, model_type);
					init_lcd_mode();
				}
				break;
			case MODEL_FREQ:
				model_freq = (uint8_t)lcd_data;
//				send_lcd_data_var(MODEL_FREQ, (uint16_t)model_freq);
				send_model_str(model_freq, model_type);
				break;
			case MODEL_TYPE:
				model_type = (uint8_t)lcd_data;
//				send_lcd_data_var(MODEL_TYPE, (uint16_t)model_type);
				send_model_str(model_freq, model_type);
				break;				
			case VAR_CAL_VAL:
				cal_val = (int16_t)lcd_data;
				break;
			case VAR_FREQ_CAL_VAL:
				freq_cal_val = (int16_t)lcd_data;
				break;
			case DATA_SAVE:
				if(lcd_data == 1)	// save
				{
					if(lcd_status == LCD_MODEL_SETUP)
					{
						save_byte_fram(ADDR_MODEL_FREQ, model_freq, fram_mode);
						save_byte_fram(ADDR_MODEL_TYPE, model_type, fram_mode);
						save_param_fram(ADDR_CAL_VAL, cal_val, fram_mode);
						save_param_fram(ADDR_FREQ_CAL_VAL, freq_cal_val, fram_mode);
						
//						lcd_status = LCD_RUN_MULTI;
#ifndef MOOHAN
						if(model_type == 0)		//hand
							lcd_status = LCD_RUN_HAND;
						else if(model_type == 1)		//multi
							lcd_status = LCD_RUN_MULTI;
						else if(model_type == 2)		//standrd
							lcd_status = LCD_RUN_STD;
						sys_mode = model_type;
#endif
						
					}
					else if((lcd_status == LCD_SETUP_MULTI)||((lcd_status == LCD_SETUP_MH2)&&(model_type == 1))||(lcd_status == LCD_SETUP_MHC)||(lcd_status == LCD_SETUP_MHE))
					{
						save_byte_fram(ADDR_OUT_POWER, output_power, fram_mode);
						temp_i = ((output_power-50) * 255) / 100;
						i2c_adc_write_byte(I2C_POT, I2C_POT_WP, (uint8_t)temp_i);
						save_word_fram(ADDR_ENERGY, limit_energy, fram_mode);
						
						save_byte_fram(ADDR_TIMEOVER, limit_out_time, fram_mode);
						if(energy_ctrl == true)
							save_byte_fram(ADDR_EN_ENERGY, (uint16_t)1, fram_mode);
						else
							save_byte_fram(ADDR_EN_ENERGY, (uint16_t)0, fram_mode);

						if(multi_ctrl == true)
							save_byte_fram(ADDR_EN_MULTI, (uint16_t)1, fram_mode);
						else
							save_byte_fram(ADDR_EN_MULTI, (uint16_t)0, fram_mode);
						save_param_fram(ADDR_MULTI_T1, limit_mo_time1, fram_mode);
						save_param_fram(ADDR_MULTI_T2, limit_mo_time2, fram_mode);
						save_param_fram(ADDR_MULTI_O1, limit_mo_out1, fram_mode);
						save_param_fram(ADDR_MULTI_O2, limit_mo_out2, fram_mode);						
						if(temp_comm_mode != comm_mode)
						{
							comm_mode = temp_comm_mode;
							save_byte_fram(ADDR_COMM_MODE, comm_mode, fram_mode);
						}
						
						for(temp_i = 0 ; temp_i < 4 ; temp_i++)
							if(temp_ether_ip[temp_i] != ether_ip[temp_i]) break;
						if(temp_i < 4){
							for(temp_i = 0 ; temp_i < 4 ; temp_i++)
							{
								ether_ip[temp_i] = temp_ether_ip[temp_i];
								gWIZNETINFO.ip[i] = ether_ip[i];
							}
							save_4byte_fram(ADDR_ETHER_IP, ether_ip);
						}
						for(temp_i = 0 ; temp_i < 4 ; temp_i++)
							if(temp_ether_nm[temp_i] != ether_nm[temp_i]) break;
						if(temp_i < 4){
							for(temp_i = 0 ; temp_i < 4 ; temp_i++)
							{
								ether_nm[temp_i] = temp_ether_nm[temp_i];
								gWIZNETINFO.sn[i] = ether_nm[i];
							}
							save_4byte_fram(ADDR_ETHER_NM, ether_nm);
						}
						for(temp_i = 0 ; temp_i < 4 ; temp_i++)
							if(temp_ether_gw[temp_i] != ether_gw[temp_i]) break;
						if(temp_i < 4){
							for(temp_i = 0 ; temp_i < 4 ; temp_i++)
							{
								ether_gw[temp_i] = temp_ether_gw[temp_i];
								gWIZNETINFO.gw[i] = ether_gw[i];
							}
							save_4byte_fram(ADDR_ETHER_GW, ether_gw);
						}
						
						if((temp_address != comm_address)||(temp_speed_idx != comm_speed_idx)||(temp_parity_idx != comm_parity_idx))
						{
							comm_address = temp_address;
							comm_parity_idx = temp_parity_idx;
							comm_speed_idx = temp_speed_idx;
							
							save_byte_fram(ADDR_COMM_ADDR, comm_address, fram_mode);
							save_byte_fram(ADDR_COMM_SPEED, comm_speed_idx, fram_mode);
							save_byte_fram(ADDR_COMM_PARITY, comm_parity_idx, fram_mode);
							
							close_modbus();
							if(comm_address != 0)
								init_modbus(comm_speed_idx, comm_parity_idx, comm_address);
						}
#ifndef MOOHAN
						lcd_status = LCD_RUN_MULTI;
#endif
					}
					else if((lcd_status == LCD_SETUP_HAND)||((lcd_status == LCD_SETUP_MH2)&&(model_type == 0))||(lcd_status == LCD_SETUP_MHC)||(lcd_status == LCD_SETUP_MHE))
					{
						save_byte_fram(ADDR_OUT_POWER, output_power, fram_mode);
						temp_i = ((output_power-50) * 255) / 100;
						i2c_adc_write_byte(I2C_POT, I2C_POT_WP, (uint8_t)temp_i);
						save_word_fram(ADDR_ENERGY, limit_energy, fram_mode);
						if(energy_ctrl == true)
							save_byte_fram(ADDR_EN_ENERGY, (uint16_t)1, fram_mode);
						else
							save_byte_fram(ADDR_EN_ENERGY, (uint16_t)0, fram_mode);
							
						save_param_fram(ADDR_ON_TIME, (uint16_t)(limit_on_time), fram_mode);
						
						save_byte_fram(ADDR_TIMEOVER, limit_out_time, fram_mode);

						if(multi_ctrl == true)
							save_byte_fram(ADDR_EN_MULTI, (uint16_t)1, fram_mode);
						else
							save_byte_fram(ADDR_EN_MULTI, (uint16_t)0, fram_mode);
						save_param_fram(ADDR_MULTI_T1, limit_mo_time1, fram_mode);
						save_param_fram(ADDR_MULTI_T2, limit_mo_time2, fram_mode);
						save_param_fram(ADDR_MULTI_O1, limit_mo_out1, fram_mode);
						save_param_fram(ADDR_MULTI_O2, limit_mo_out2, fram_mode);
						
						if((temp_address != comm_address)||(temp_speed_idx != comm_speed_idx)||(temp_parity_idx != comm_parity_idx))
						{
							comm_address = temp_address;
							comm_parity_idx = temp_parity_idx;
							comm_speed_idx = temp_speed_idx;
							
							save_byte_fram(ADDR_COMM_ADDR, comm_address, fram_mode);
							save_byte_fram(ADDR_COMM_SPEED, comm_speed_idx, fram_mode);
							save_byte_fram(ADDR_COMM_PARITY, comm_parity_idx, fram_mode);
							
							close_modbus();
							if(comm_address != 0)
								init_modbus(comm_speed_idx, comm_parity_idx, comm_address);
						}
#ifndef MOOHAN
						lcd_status = LCD_RUN_MULTI;
#endif
					}
					else if((lcd_status == LCD_SETUP_STD1)||(lcd_status == LCD_SETUP_STD2D)||(lcd_status == LCD_SETUP_STD2T)||(lcd_status == LCD_SETUP_STD3)||(lcd_status == LCD_SETUP_STDC)||(lcd_status == LCD_SETUP_STDE))
					{
						save_byte_fram(ADDR_OUT_POWER, output_power, fram_mode);
						temp_i = ((output_power-50) * 255) / 100;
						i2c_adc_write_byte(I2C_POT, I2C_POT_WP, (uint8_t)temp_i);
						save_word_fram(ADDR_ENERGY, limit_energy, fram_mode);
						if(energy_ctrl == true)
							save_byte_fram(ADDR_EN_ENERGY, (uint8_t)1, fram_mode);
						else
							save_byte_fram(ADDR_EN_ENERGY, (uint8_t)0, fram_mode);

						save_byte_fram(ADDR_TIMEOVER, limit_out_time, fram_mode);
							
						if(temp_cnt_reset == 1)
						{
							work_cnt = 0;
							save_word_fram(ADDR_WORK_CNT, work_cnt, fram_mode);
							send_lcd_data_word(LV_WORK_CNT, (uint32_t)work_cnt);
						}
						
						if(temp_horndown == 1)
						{
							sys_status = SYS_HORN;
							horn_status = 0;
							port_pin_set_output_level(SOL_DN, SOL_OFF);
							re_start1_pressed = re_start2_pressed = 0;
						}
						else
						{
							if(sys_status == SYS_HORN)
							{
								horn_status = 0;
								port_pin_set_output_level(SOL_DN, SOL_OFF);
							}
							sys_status = SYS_RUN;							
						}

						save_byte_fram(ADDR_SAFTY, f_safty, fram_mode);
						
						save_byte_fram(ADDR_RUN_MODE, run_mode, fram_mode);
						save_param_fram(ADDR_DELAY1, limit_delay_time1, fram_mode);
						save_param_fram(ADDR_DELAY2, limit_delay_time2, fram_mode);
						save_param_fram(ADDR_DELAY3, limit_delay_time3, fram_mode);
						save_param_fram(ADDR_TRIGGER2, limit_trigger_time2, fram_mode);
						save_param_fram(ADDR_TRIGGER3, limit_trigger_time3, fram_mode);
						if(multi_ctrl == true)
							save_byte_fram(ADDR_EN_MULTI, (uint16_t)1, fram_mode);
						else
							save_byte_fram(ADDR_EN_MULTI, (uint16_t)0, fram_mode);
						save_param_fram(ADDR_MULTI_T1, limit_mo_time1, fram_mode);
						save_param_fram(ADDR_MULTI_T2, limit_mo_time2, fram_mode);
						save_param_fram(ADDR_MULTI_O1, limit_mo_out1, fram_mode);
						save_param_fram(ADDR_MULTI_O2, limit_mo_out2, fram_mode);
						
						if((temp_address != comm_address)||(temp_speed_idx != comm_speed_idx)||(temp_parity_idx != comm_parity_idx))
						{
							comm_address = temp_address;
							comm_parity_idx = temp_parity_idx;
							comm_speed_idx = temp_speed_idx;
							
							save_byte_fram(ADDR_COMM_ADDR, comm_address, fram_mode);
							save_byte_fram(ADDR_COMM_SPEED, comm_speed_idx, fram_mode);
							save_byte_fram(ADDR_COMM_PARITY, comm_parity_idx, fram_mode);
							
							close_modbus();
							if(comm_address != 0)
								init_modbus(comm_speed_idx, comm_parity_idx, comm_address);
						}
#ifndef MOOHAN
						lcd_status = LCD_RUN_STD;
#endif
					}
				}
				else
				{					// cancel
					if(lcd_status == LCD_MODEL_SETUP)
					{
						model_freq = read_byte_fram(ADDR_MODEL_FREQ, fram_mode);
						model_type = read_byte_fram(ADDR_MODEL_TYPE, fram_mode);
						cal_val = read_param_fram(ADDR_CAL_VAL, fram_mode);
						freq_cal_val = read_param_fram(ADDR_FREQ_CAL_VAL, fram_mode);
						sys_mode = model_type;
						send_model_str(model_freq, model_type);
						lcd_status = LCD_RUN_MULTI;
					}
					else if((lcd_status == LCD_SETUP_MULTI)||(((lcd_status == LCD_SETUP_MH2)||(lcd_status == LCD_SETUP_MHC)||(lcd_status == LCD_SETUP_MHE))&&(model_type == 1)))
					{
						output_power = read_byte_fram(ADDR_OUT_POWER, fram_mode);
//						temp_i = (output_power / 2) + 50;
						send_lcd_data_var(LV_OUT_POWER, output_power);
						limit_energy = read_word_fram(ADDR_ENERGY, fram_mode);
						send_lcd_data_word(LV_ENERGY_VAL, (uint32_t)limit_energy);
						send_lcd_data_var(LV_ENERGY_EDIT, (uint16_t)(limit_energy /10));
						lcd_data = read_byte_fram(ADDR_EN_ENERGY, fram_mode);
						if(lcd_data == 1)
							energy_ctrl = true;
						else
							energy_ctrl = false;
						send_lcd_data_var(DISP_ENERGY_EN, lcd_data);

						lcd_data = read_byte_fram(ADDR_EN_MULTI, fram_mode);
						if(lcd_data == 1)
							multi_ctrl = true;
						else
							multi_ctrl = false;
						limit_mo_time1 = read_param_fram(ADDR_MULTI_T1, fram_mode);
						limit_mo_time2 = read_param_fram(ADDR_MULTI_T2, fram_mode);
						limit_mo_out1 = read_param_fram(ADDR_MULTI_O1, fram_mode);
						limit_mo_out2 = read_param_fram(ADDR_MULTI_O2, fram_mode);
						comm_address = read_byte_fram(ADDR_COMM_ADDR, fram_mode);
						comm_speed_idx = read_byte_fram(ADDR_COMM_SPEED, fram_mode);
						comm_parity_idx = read_byte_fram(ADDR_COMM_PARITY, fram_mode);

						lcd_status = LCD_RUN_MULTI;
					}
					else if((lcd_status == LCD_SETUP_HAND)||(((lcd_status == LCD_SETUP_MH2)||(lcd_status == LCD_SETUP_MHC)||(lcd_status == LCD_SETUP_MHE))&&(model_type == 0)))
					{
						output_power = read_byte_fram(ADDR_OUT_POWER, fram_mode);
//						temp_i = (output_power / 2) + 50;
						send_lcd_data_var(LV_OUT_POWER, output_power);
						limit_energy = read_word_fram(ADDR_ENERGY, fram_mode);
						limit_on_time = read_param_fram(ADDR_ON_TIME, fram_mode);
						
						send_lcd_data_var(LV_MAX_ON_TIME, (uint16_t)(limit_on_time));
						send_lcd_data_word(LV_ENERGY_VAL, (uint32_t)limit_energy);
						send_lcd_data_var(LV_ENERGY_EDIT, (uint16_t)(limit_energy /10));
						lcd_data = read_byte_fram(ADDR_EN_ENERGY, fram_mode);
						if(lcd_data == 1)
							energy_ctrl = true;
						else
							energy_ctrl = false;
						send_lcd_data_var(DISP_ENERGY_EN, lcd_data);

						lcd_data = read_byte_fram(ADDR_EN_MULTI, fram_mode);
						if(lcd_data == 1)
							multi_ctrl = true;
						else
							multi_ctrl = false;
						limit_mo_time1 = read_param_fram(ADDR_MULTI_T1, fram_mode);
						limit_mo_time2 = read_param_fram(ADDR_MULTI_T2, fram_mode);
						limit_mo_out1 = read_param_fram(ADDR_MULTI_O1, fram_mode);
						limit_mo_out2 = read_param_fram(ADDR_MULTI_O2, fram_mode);
						comm_address = read_byte_fram(ADDR_COMM_ADDR, fram_mode);
						comm_speed_idx = read_byte_fram(ADDR_COMM_SPEED, fram_mode);
						comm_parity_idx = read_byte_fram(ADDR_COMM_PARITY, fram_mode);

						lcd_status = LCD_RUN_MULTI;
					}
					else if((lcd_status == LCD_SETUP_STD1)||(lcd_status == LCD_SETUP_STD2D)||(lcd_status == LCD_SETUP_STD2T)||(lcd_status == LCD_SETUP_STD3)||(lcd_status == LCD_SETUP_STDC)||(lcd_status == LCD_SETUP_STDE))
					{
						output_power = read_byte_fram(ADDR_OUT_POWER, fram_mode);
//						temp_i = (output_power / 2) + 50;
						send_lcd_data_var(LV_OUT_POWER, output_power);
						limit_energy = read_word_fram(ADDR_ENERGY, fram_mode);
						send_lcd_data_word(LV_ENERGY_VAL, (uint32_t)limit_energy);
						send_lcd_data_var(LV_ENERGY_EDIT, (uint16_t)(limit_energy /10));
						lcd_data = read_byte_fram(ADDR_EN_ENERGY, fram_mode);
						if(lcd_data == 1)
							energy_ctrl = true;
						else
							energy_ctrl = false;

						lcd_data = read_byte_fram(ADDR_EN_MULTI, fram_mode);
						if(lcd_data == 1)
							multi_ctrl = true;
						else
							multi_ctrl = false;
						f_safty = read_byte_fram(ADDR_SAFTY, fram_mode);
						
						run_mode = read_byte_fram(ADDR_RUN_MODE, fram_mode);
						limit_delay_time1 = read_param_fram(ADDR_DELAY1, fram_mode);
						limit_delay_time2 = read_param_fram(ADDR_DELAY2, fram_mode);
						limit_delay_time3 = read_param_fram(ADDR_DELAY3, fram_mode);
						limit_trigger_time2 = read_param_fram(ADDR_TRIGGER2, fram_mode);
						limit_trigger_time3 = read_param_fram(ADDR_TRIGGER3, fram_mode);
						limit_mo_time1 = read_param_fram(ADDR_MULTI_T1, fram_mode);
						limit_mo_time2 = read_param_fram(ADDR_MULTI_T2, fram_mode);
						limit_mo_out1 = read_param_fram(ADDR_MULTI_O1, fram_mode);
						limit_mo_out2 = read_param_fram(ADDR_MULTI_O2, fram_mode);
						
						if(sys_status == SYS_HORN)
							send_lcd_data_var(DISP_HORNDOWN, (uint16_t)1);
						else
							send_lcd_data_var(DISP_HORNDOWN, (uint16_t)0);
						
						temp_horndown = 0;

						comm_address = read_byte_fram(ADDR_COMM_ADDR, fram_mode);
						comm_speed_idx = read_byte_fram(ADDR_COMM_SPEED, fram_mode);
						comm_parity_idx = read_byte_fram(ADDR_COMM_PARITY, fram_mode);

						lcd_status = LCD_RUN_STD;
					}
				}
				change_lcd_page(lcd_status);
				break;
			case KEY_MULTI:
				if(lcd_data == 1)		//RESET
				{
					if( us_reset_status != US_REMOTE)
					{
						us_reset_status = US_TOUCH;
						sig_reset_status = ON;
						max_us_reset_cnt = MAX_US_RESET_CNT;
						if(sig_seek_status == ON)
						{
							sig_seek_status = OFF;
							us_seek_status = US_IDLE;
						}

						if((sys_status == SYS_ERROR) && (((error_status & ERR_OVLD) == ERR_OVLD) || ((error_status & ERR_OUTERR) == ERR_OUTERR)))
						{
							send_lcd_data_var(ICON_OL, (uint16_t)0);
							send_lcd_data_var(ICON_OUTERR, (uint16_t)0);
							re_outerr_issued = 0;
							if(model_type == 0)		//hand
								lcd_status = LCD_RUN_HAND;
							else if(model_type == 1)		//multi
								lcd_status = LCD_RUN_MULTI;
							else if(model_type == 2)		//standrd
								lcd_status = LCD_RUN_STD;
						
							set_lcd_page(lcd_status);
						}
					}
				}
				else if(lcd_data == 2)		//SEEK
				{
					if( us_seek_status != US_REMOTE)
					{
						us_seek_status = US_TOUCH;
						sig_seek_status = ON;
						max_us_reset_cnt = MAX_US_RESET_CNT;
						
						if(((us_reset_status == US_TOUCH)||(us_reset_status == US_COMM)) && (us_reset_mode == RESET_ISSUE))
						{
							sig_reset_status = OFF;
							us_reset_cnt = 0;
							us_reset_status = US_IDLE;
							us_seek_status = US_TOUCH;
						}
					}
				}
				else if(lcd_data == 3)		//RUN(press)
				{
					if( us_run_status != US_REMOTE)
					{
						us_run_status = US_TOUCH;
						sig_run_status = ON;
						us_on_time = 0;
						curr_energy = last_energy = max_amp = max_power = acc_energy = 0;
						temp_i = (((output_power-50) * 255) / 100 );
						i2c_adc_write_byte(I2C_POT, I2C_POT_WP, (uint8_t)temp_i);
//						if(multi_ctrl == true)
//						{
//							multi_ctrl_stage = 0;
//							temp_i = (((limit_mo_out1-50) * 255) / 100 );
//							i2c_adc_write_byte(I2C_POT, I2C_POT_WP, (uint8_t)temp_i);
//						}
					}
				}
				else if(lcd_data == 4)		//RUN(release)
				{
					if(us_run_status == US_TOUCH)
					{
						us_run_status = US_IDLE;
						sig_run_status = OFF;
					}
				}
				break;
			case KEY_ERROR_RESET:
				if(lcd_data == 1)		//RESET
				{
					if( us_reset_status != US_REMOTE)
					{
						us_reset_status = US_TOUCH;
						sig_reset_status = ON;
						max_us_reset_cnt = MAX_US_RESET_CNT;
					}
					
					if(	error_status & ERR_OVTIME)
					{
						error_status = 0;
						
						if(model_type == 0)		//hand
							lcd_status = LCD_RUN_HAND;
						else if(model_type == 1)		//multi
							lcd_status = LCD_RUN_MULTI;
						else if(model_type == 2)		//standrd
							lcd_status = LCD_RUN_STD;
						
						set_lcd_page(lcd_status);
					}

				}
				break;		
						
			case SETUP_MODEL:
				if(lcd_data == 0)	// press
				{
					key_tick = 0;
				}
				else if(lcd_data == 2) //release
				{
					if(key_tick > KEY_HOLD_TH)
					{
						key_tick = 255;
						set_lcd_page(LCD_MODEL_SETUP);
						lcd_status = LCD_MODEL_SETUP;
						send_model_str(model_freq, model_type);
						send_lcd_data_var(MODEL_FREQ, (uint16_t)model_freq);
						send_lcd_data_var(MODEL_TYPE, (uint16_t)model_type);
						send_lcd_data_var(VAR_CAL_VAL, (uint16_t)cal_val);
						send_lcd_data_var(VAR_FREQ_CAL_VAL, (uint16_t)freq_cal_val);
					}
				}
				break;
			case SETUP_PARAM:
				if (sys_mode == SYS_HAND)
				{
//					model_type = (uint8_t)lcd_data;
					lcd_status = LCD_SETUP_HAND;
				}
				else if (sys_mode == SYS_MULTI)
				{
//					model_type = (uint8_t)lcd_data;
					lcd_status = LCD_SETUP_MULTI;
				}
				else if (sys_mode == SYS_STD)
				{
//					model_type = (uint8_t)lcd_data;
					lcd_status = LCD_SETUP_STD1;
				}
				change_lcd_page(lcd_status);
				break;
			case SETUP_PARAM_MOOHAN:
				if(lcd_data == 0)	// press
				{
					key_tick = 0;
				}
				else if(lcd_data == 2) //release
				{
					if(key_tick > KEY_HOLD_TH)
					{
						key_tick = 255;
						if (sys_mode == SYS_HAND)
						{
							//					model_type = (uint8_t)lcd_data;
							lcd_status = LCD_SETUP_HAND;
						}
						else if (sys_mode == SYS_MULTI)
						{
							//					model_type = (uint8_t)lcd_data;
							lcd_status = LCD_SETUP_MULTI;
						}
						else if (sys_mode == SYS_STD)
						{
							//					model_type = (uint8_t)lcd_data;
							lcd_status = LCD_SETUP_STD1;
						}
						change_lcd_page(lcd_status);
					}
				}
				break;
			case LV_LIMIT_OUT_T:
				limit_out_time = (uint8_t)(lcd_data);
				break;
			case LV_MAX_ON_TIME:
				limit_on_time = (uint16_t)(lcd_data);
				break;
			case LV_OUT_POWER:
//				output_power = (uint8_t)((lcd_data - 50) * 2);
				output_power = (uint8_t)(lcd_data);
//				temp_i = (((output_power * 128) / 100) - 1);
//				i2c_adc_write_byte(I2C_POT, I2C_POT_WP, (uint8_t)temp_i);
//				send_lcd_data_var(LV_OUT_POWER, (uint16_t)output_power);
				break;
/*			case VV_OUTPOWER:
				output_power = (uint8_t)(100 - lcd_data);
				temp_i = (((output_power * 128) / 100) -1);
				i2c_adc_write_byte(I2C_POT, I2C_POT_WP, (uint8_t)temp_i);
				send_lcd_data_var(LV_OUT_POWER, (uint16_t)output_power);
				break; */
			case LV_ENERGY_EDIT:
				limit_energy = (uint16_t)(lcd_data);
//				send_lcd_data_var(LV_ENERGY_VAL, (uint16_t)limit_energy);
//				send_lcd_data_var(LV_ENERGY_EDIT, (uint16_t)limit_energy);
				break;
/*			case VV_LIMITENERGY:
				limit_energy = (uint32_t)((10000 - lcd_data) * 10);
				send_lcd_data_word(LV_ENERGY_VAL, (uint32_t)limit_energy);
				send_lcd_data_var(LV_ENERGY_EDIT, (uint16_t)(10000 - lcd_data));
				break;*/
			case ENERGY_EN:
				if(lcd_data == 1)
				{
					if(energy_ctrl == false)
					{
						energy_ctrl = true;
						send_lcd_data_var(DISP_ENERGY_EN, (uint16_t)1);
					}
					else
					{
						energy_ctrl = false;
						send_lcd_data_var(DISP_ENERGY_EN, (uint16_t)0);
					}
				}
			
				break;
			case DISP_HORNDOWN:
				if(lcd_data == 1)
				{
					temp_horndown = 1;
//					sys_status = SYS_HORN;
//					horn_status = 0;
				}
				else
				{
					temp_horndown = 0;
//					sys_status = SYS_RUN;
				}
				break;
			case DISP_SAFTY:
				if(lcd_data == 1)
				{
					f_safty = 1;
				}
				else
				{
					f_safty = 0;
				}
				break;
			case MULTI_EN:
				if(lcd_data == 1)
				{
					if(multi_ctrl == false)
					{
						multi_ctrl = true;
						send_lcd_data_var(DISP_MULTI_EN, (uint16_t)1);
					}
					else
					{
						multi_ctrl = false;
						send_lcd_data_var(DISP_MULTI_EN, (uint16_t)0);
					}
				}
				
				break;
			case STD_SETUP_PARAM:
				if(lcd_data == 1)	// GOTO SETUP PAGE 1
				{
					if((lcd_status == LCD_SETUP_MH2)||(lcd_status == LCD_SETUP_MHC)||(lcd_status == LCD_SETUP_MHE))
					{
						if(model_type == 0)		//hand
							lcd_status = LCD_SETUP_HAND;
						else if(model_type == 1)		//multi
							lcd_status = LCD_SETUP_MULTI;
						
					}
					else
					{
						lcd_status = LCD_SETUP_STD1;
					}
					set_lcd_page(lcd_status);
				}
				else if(lcd_data == 2)	// GOTO SETUP PAGE 2
				{
					if((lcd_status == LCD_SETUP_STD1)||(lcd_status == LCD_SETUP_STD3)||(lcd_status == LCD_SETUP_STDC)||(lcd_status == LCD_SETUP_STDE))
					{
						if(run_mode == MODE_DELAY)
						{
							lcd_status = LCD_SETUP_STD2D;
						}
						else
						{
							lcd_status = LCD_SETUP_STD2T;
						}
					}
					else if((lcd_status == LCD_SETUP_MULTI)||(lcd_status == LCD_SETUP_HAND)||(lcd_status == LCD_SETUP_MHC)||(lcd_status == LCD_SETUP_MHE))
					{
						lcd_status = LCD_SETUP_MH2;
					}
					change_lcd_page(lcd_status);
				}
				else if(lcd_data == 3)	// GOTO SETUP PAGE 3
				{
					if((lcd_status == LCD_SETUP_STD1)||(lcd_status == LCD_SETUP_STD2D)||(lcd_status == LCD_SETUP_STD2T)||(lcd_status == LCD_SETUP_STDC)||(lcd_status == LCD_SETUP_STDE))
					{
						lcd_status = LCD_SETUP_STD3;
					}
					else if((lcd_status == LCD_SETUP_MULTI)||(lcd_status == LCD_SETUP_HAND)||(lcd_status == LCD_SETUP_MH2))
					{
						if(comm_mode == COMM_SERIAL)
							lcd_status = LCD_SETUP_MHC;
						else
							lcd_status = LCD_SETUP_MHE;
					}
					change_lcd_page(lcd_status);
				}
				else if(lcd_data == 4)	// GOTO SETUP PAGE 4
				{
					if(comm_mode == COMM_SERIAL)
						lcd_status = LCD_SETUP_STDC;
					else
						lcd_status = LCD_SETUP_STDE;
					change_lcd_page(lcd_status);
				}
				else if(lcd_data == 5)	// counter reset
				{
					if(work_cnt != 0)
					{
						temp_cnt_reset = 1;
					}
				}
/*				else if(lcd_data == 6)	// change safty mode
				{
					if(f_safty == 1)
					{
						f_safty = 0;
					}
					else
					{
						f_safty = 1;
					}
				}
*/				
				break;
			case LV_RUN_MODE:
				if(lcd_data == 1)	// change to delay mode
				{
					run_mode = MODE_DELAY;
					lcd_status = LCD_SETUP_STD2D;
					change_lcd_page(lcd_status);
				}
				else if(lcd_data == 2)	// change to trigger mode
				{
					run_mode = MODE_TRIGGER;
					lcd_status = LCD_SETUP_STD2T;
					change_lcd_page(lcd_status);
				}
				break;
			case LV_DM_DELAY:
				limit_delay_time1 = (uint16_t)lcd_data;
				break;
			case LV_DM_WELD:
				limit_delay_time2 = (uint16_t)lcd_data;
				break;
			case LV_DM_HOLD:
				limit_delay_time3 = (uint16_t)lcd_data;
				break;
			case LV_TM_WELD:
				limit_trigger_time2 = (uint16_t)lcd_data;
				break;
			case LV_TM_HOLD:
				limit_trigger_time3 = (uint16_t)lcd_data;
				break;
			case LV_MO_OUT1:
				limit_mo_out1 = (uint16_t)lcd_data;
//				if(limit_mo_out1 > limit_mo_out2)
//				{
//					limit_mo_out2 = limit_mo_out1;
//					send_lcd_data_var(LV_MO_OUT2, (uint16_t)limit_mo_out2);
//				}
				break;
			case LV_MO_OUT2:
				limit_mo_out2 = (uint16_t)lcd_data;
//				if(limit_mo_out1 > limit_mo_out2)
//				{
//					limit_mo_out2 = limit_mo_out1;
//					send_lcd_data_var(LV_MO_OUT2, (uint16_t)limit_mo_out2);
//				}
				break;
			case LV_MO_TIME1:
				limit_mo_time1 = (uint16_t)lcd_data;
				if(limit_mo_time1 > limit_mo_time2)
				{
					limit_mo_time2 = limit_mo_time1;
					send_lcd_data_var(LV_MO_TIME2, (uint16_t)limit_mo_time2);
				}
				
				break;
			case LV_MO_TIME2:
				limit_mo_time2 = (uint16_t)lcd_data;
				if(limit_mo_time1 > limit_mo_time2)
				{
					limit_mo_time2 = limit_mo_time1;
					send_lcd_data_var(LV_MO_TIME2, (uint16_t)limit_mo_time2);
				}
				break;
			case COMM_ADDR:
				temp_address = (uint8_t)lcd_data;
				conv_addr2str(temp_address, (uint8_t *)temp_str);
				send_lcd_byte_array(COMM_ADDR_TXT, 4, (uint8_t *)temp_str);
				break;
			case COMM_SPEED:
				temp_speed_idx = (uint8_t)lcd_data;
				send_lcd_byte_array(COMM_SPEED_TXT, 6, (uint8_t *)comm_speed_txt[temp_speed_idx]);
				break;
			case COMM_PARITY:
				temp_parity_idx = (uint8_t)lcd_data;
				send_lcd_byte_array(COMM_PARITY_TXT, 4, (uint8_t *)comm_parity_txt[temp_parity_idx]);
				break;
			case LV_COMM_MODE:
				if(lcd_data == 0)		// serial
				{
					temp_comm_mode = (uint8_t)lcd_data;
					send_lcd_data_var(DISP_COMM_MODE, temp_comm_mode);
					if (lcd_status == LCD_SETUP_MHE)
						lcd_status =  LCD_SETUP_MHC;
					else if(lcd_status == LCD_SETUP_STDE)
						lcd_status = LCD_SETUP_STDC;
					change_lcd_page(lcd_status);
				}
				else // ether
				{
					if(lcd_data == 1)
					{
						// dhcp
						send_lcd_data_var(DISP_COMM_MODE, 1);	// ethernet icon
						temp_comm_mode = COMM_ETH_STATIC;
					}
					else
					{	
						// static
						if(temp_comm_mode == COMM_ETH_STATIC)
						{
							temp_comm_mode = COMM_ETH_DHCP;
							send_lcd_data_var(DISP_EN_DHCP, 1);
						} 
						else if(temp_comm_mode == COMM_ETH_DHCP)
						{
							temp_comm_mode = COMM_ETH_STATIC;
							send_lcd_data_var(DISP_EN_DHCP, 0);
						}
						
					}
					if (lcd_status == LCD_SETUP_MHC)
						lcd_status =  LCD_SETUP_MHE;
					else if(lcd_status == LCD_SETUP_STDC)
						lcd_status = LCD_SETUP_STDE;
					change_lcd_page(lcd_status);
				}
				break;
			case LV_ETHER_KEY:
				if((lcd_data == 'I') || (lcd_data == 'M') || (lcd_data == 'G')){
					ip_to_string(temp_ether_ip, ether_input_buffer);
					lcd_data_pdd(16, ether_input_buffer);
					send_lcd_byte_array(COMM_IP_TXT, 16, (uint8_t *)lcd_temp_buf);
					ip_to_string(temp_ether_nm, ether_input_buffer);
					lcd_data_pdd(16, ether_input_buffer);
					send_lcd_byte_array(COMM_NM_TXT, 16, (uint8_t *)lcd_temp_buf);
					ip_to_string(temp_ether_gw, ether_input_buffer);
					lcd_data_pdd(16, ether_input_buffer);
					send_lcd_byte_array(COMM_GW_TXT, 16, (uint8_t *)lcd_temp_buf);

					if(lcd_data == 'I'){
						ether_what_input = ETHER_INPUT_IP;
						ether_buffer_pos = ip_to_string(temp_ether_ip, ether_input_buffer);	
						for(temp_i = 0; temp_i < 4 ; temp_i++)
							ether_temp_ip[temp_i] = temp_ether_ip[temp_i];				
					} else if(lcd_data == 'M') {
						ether_what_input = ETHER_INPUT_NM;
						ether_buffer_pos = ip_to_string(temp_ether_nm, ether_input_buffer);					
						for(temp_i = 0; temp_i < 4 ; temp_i++)
							ether_temp_ip[temp_i] = temp_ether_nm[temp_i];				
					} else {
						ether_what_input = ETHER_INPUT_GW;
						ether_buffer_pos = ip_to_string(temp_ether_gw, ether_input_buffer);					
						for(temp_i = 0; temp_i < 4 ; temp_i++)
						ether_temp_ip[temp_i] = temp_ether_gw[temp_i];				
					}
					ether_current_octet= 3;
					ether_current_number = temp_ether_ip[3];
					ether_has_input = true;
				} else {
					process_ip_char(lcd_data);
					if(ether_ip_input_complete == true)
					{
						if(ether_what_input == ETHER_INPUT_IP){
							for(temp_i = 0; temp_i < 4 ; temp_i++)
								temp_ether_ip[temp_i] = ether_temp_ip[temp_i] ;				
						} else if(ether_what_input == ETHER_INPUT_NM) {
							for(temp_i = 0; temp_i < 4 ; temp_i++)
								temp_ether_nm[temp_i] = ether_temp_ip[temp_i];				
						} else {
							for(temp_i = 0; temp_i < 4 ; temp_i++)
								temp_ether_gw[temp_i] = ether_temp_ip[temp_i];				
						}
						ether_ip_input_complete = false;
					}
					lcd_data_pdd(16, ether_input_buffer);
					send_lcd_byte_array(COMM_IP_TXT + (0x10*ether_what_input), 16, (uint8_t *)lcd_temp_buf);
				}
				break;
			default:
				break;
		}
	}	
}

void sig_process(void)
{
	
}
void calc_freq(void)
{
	uint8_t i;
	
	uint32_t temp_long;
	
	if(update_freq)
	{
		for(i = 0, temp_long = 0 ; i < 10 ; i++)
			temp_long += freq_buf[i];
		
//		curr_freq = 64000000UL / temp_long;
		curr_freq = 81000000UL / temp_long;
		curr_freq += freq_cal_val;
	}
	else
	{
		curr_freq = 0;
	}
	update_freq = false;
}

void send_val_data(void)
{
	uint16_t temp_buf[4], temp_val, temp_i;
	
	temp_buf[1] = (us_on_status == ON) ? max_amp : last_amp;		// current
//	temp_buf[0]  = curr_power;		// power
	temp_buf[0]  = (us_on_status == ON) ?max_power : last_power;		// power
	temp_i = (us_on_status == ON) ? curr_freq : last_freq;
	temp_buf[2] = temp_i / 100;
	send_lcd_int_array(VAR_POWER, 3, (uint16_t *)temp_buf);
	if(us_on_status == ON)
		send_lcd_data_var(VAR_ENERGY, (uint16_t)curr_energy);
	else
		send_lcd_data_var(VAR_ENERGY, (uint16_t)last_energy);

}

void us_off(void)
{
	last_time = us_on_time_200m;
	us_on_time_200m = 0;
	send_lcd_data_var(ICON_RUN, 0);
	port_pin_set_output_level(M_START, CTRL_INT_OFF);
	port_pin_set_output_level(B_USOUT, CTRL_OFF);
	if(run_status != RUN_HOLD)
		port_pin_set_output_level(SOL_DN, SOL_OFF);
//	last_amp = curr_amp;
//	last_power = curr_power;
	last_amp = max_amp;
	last_power = max_power;
	last_energy = curr_energy;
	curr_energy = 0;
	acc_energy = 0;
	last_freq = curr_freq;
	last_lv = curr_lv;
	us_on_time = 0;
	us_run_status = US_IDLE;	
	bak_run_status = sig_run_status = OFF;
	re_start_bak = re_start = OFF;	
	us_on_status = OFF;
}

void do_control(void)
{
	if(bak_sys_status != sys_status)
	{
		if((sys_status == SYS_ESTOP) || (sys_status == SYS_ERROR))
		{
			if(sys_status == SYS_ESTOP)
			{
				send_lcd_txt(VP_ERROR_MSG, estop_msg );
				lcd_status = LCD_WARNING;
				set_lcd_page(lcd_status);
			}
			else if(sys_status == SYS_ERROR)
			{
				if((error_status & ERR_OVLD) == ERR_OVLD)
				{
					send_lcd_txt(VP_ERROR_MSG, ovld_msg );
					send_lcd_data_var(ICON_OL, (uint16_t)1);
				}
				if((error_status & ERR_OUTERR) == ERR_OUTERR)
				{
					send_lcd_txt(VP_ERROR_MSG, outerr_msg );
					send_lcd_data_var(ICON_OUTERR, (uint16_t)1);
				}
				if((error_status & ERR_OVTIME) == ERR_OVTIME)
				{
					send_lcd_txt(VP_ERROR_MSG, ovtime_msg );
					lcd_status = LCD_WARNING;
					set_lcd_page(lcd_status);
				}
			}
			us_off();
		}
		else if(sys_status == SYS_RUN)
		{
			init_lcd_mode();
		}
	}
	bak_sys_status = sys_status;
	
	if(bak_reset_status != sig_reset_status)		//RESET
	{
		if(sig_reset_status == ON)
		{			// on
			send_lcd_data_var(ICON_RESET, 1);
			send_lcd_data_var(ICON_ERROR_RESET, 1);
			us_reset_mode = RESET_ISSUE;
			us_reset_cnt = 0;
			us_on_status = ON;
			curr_energy = 0;
			curr_energy = last_energy = max_amp = max_power = acc_energy = 0;
			port_pin_set_output_level(M_RESET, CTRL_INT_ON);
		}
		else
		{			// off
			send_lcd_data_var(ICON_RESET, 0);
			send_lcd_data_var(ICON_ERROR_RESET, 0);
			port_pin_set_output_level(M_RESET, CTRL_INT_OFF);
			us_on_status = OFF;
			last_amp = curr_amp;
			last_power = curr_power;
			last_energy = curr_energy;
			curr_energy = 0;
			acc_energy = 0;
		}
	}
	bak_reset_status = sig_reset_status;
	
	if(bak_seek_status != sig_seek_status)		//SEEK
	{
		if(sig_seek_status == ON)
		{			// on
			send_lcd_data_var(ICON_SEEK, 1);
			us_reset_mode = SEEK_ISSUE;
			us_reset_cnt = 0;
			us_on_status = ON;
			curr_energy = last_energy = max_amp = max_power = acc_energy = 0;
			port_pin_set_output_level(M_SEEK, CTRL_INT_ON);
		}
		else
		{			// off
			send_lcd_data_var(ICON_SEEK, 0);
			port_pin_set_output_level(M_SEEK, CTRL_INT_OFF);
			us_on_status = OFF;
			last_amp = curr_amp;
			last_power = curr_power;
			last_energy = curr_energy;
			curr_energy = 0;
			acc_energy = 0;
		}
	}
	bak_seek_status = sig_seek_status;
	
	if(bak_run_status != sig_run_status)		//run
	{
		if(sig_run_status == ON)
		{			// on
			send_lcd_data_var(ICON_RUN, 1);
			port_pin_set_output_level(M_START, CTRL_INT_ON);
			port_pin_set_output_level(B_USOUT, CTRL_ON);
			curr_energy = last_energy = max_amp = max_power = acc_energy = 0;
			us_on_time_200m = 0;
			us_on_time = 0;
			us_on_status = ON;
			usout_err_cnt = 0;
		}
		else
		{			// off
			us_off();
		}		
	}
	else
	{
		if(sig_run_status == ON)
		{
			if(curr_amp > USOUT_TH)
			{
				//port_pin_set_output_level(B_USOUT, CTRL_ON);
				usout_err_cnt = 0;
			}
			else
			{
				//port_pin_set_output_level(B_USOUT, CTRL_OFF);
				if(model_type == 1)	// multi
				{
					usout_err_cnt++;
					if(usout_err_cnt > USOUT_ERR_CNT_MAX)
					{
						//re_outerr_issued = 1;
					}
				}
				
			}
		}
	}
	bak_run_status = sig_run_status;
}

void update_holding_reg(uint8_t mode)
{
	uint16_t i, temp_i;
	
	if(mode)
	{
		if(holdingReg[H_REG_RESET] == 1)		//RESET
		{
			if( us_reset_status != US_REMOTE)
			{
				us_reset_status = US_TOUCH;
				sig_reset_status = ON;
				max_us_reset_cnt = MAX_US_RESET_CNT;
				if((sys_status == SYS_ERROR) && (((error_status & ERR_OVLD) == ERR_OVLD) || ((error_status & ERR_OUTERR) == ERR_OUTERR)|| ((error_status & ERR_OVTIME) == ERR_OVTIME)))
				{
					send_lcd_data_var(ICON_OL, (uint16_t)0);
					send_lcd_data_var(ICON_OUTERR, (uint16_t)0);
					re_outerr_issued = 0; 
					if(model_type == 0)		//hand
						lcd_status = LCD_RUN_HAND;
					else if(model_type == 1)		//multi
						lcd_status = LCD_RUN_MULTI;
					else if(model_type == 2)		//standrd
						lcd_status = LCD_RUN_STD;
							
					set_lcd_page(lcd_status);
					error_status = 0;
				}
			}
			holdingReg[H_REG_RESET] = 0;
		}
		else if(holdingReg[H_REG_SEEK] == 1)		//SEEK
		{
			if( us_seek_status != US_REMOTE)
			{
				us_seek_status = US_COMM;
				sig_seek_status = ON;
				max_us_reset_cnt = MAX_US_RESET_CNT;

				if(((us_reset_status == US_TOUCH)||(us_reset_status == US_COMM)) && (us_reset_mode == RESET_ISSUE))
				{
					sig_reset_status = OFF;
					us_reset_cnt = 0;
					us_reset_status = US_IDLE;
					us_seek_status = US_TOUCH;
				}
			}
			holdingReg[H_REG_SEEK] = 0;
		}
		else if(holdingReg[H_REG_START] == 1)		//RUN(press)
		{
			if( us_run_status != US_REMOTE)
			{
				us_run_status = US_COMM;
				sig_run_status = ON;
				us_on_time = 0;
				curr_energy = last_energy = max_amp = max_power = acc_energy = 0;
				temp_i = (((output_power-50) * 255) / 100 );
				i2c_adc_write_byte(I2C_POT, I2C_POT_WP, (uint8_t)temp_i);
			}
			holdingReg[H_REG_START] = 0;
		}
		else if(holdingReg[H_REG_STOP] == 1)		//RUN(release)
		{
			if( us_run_status == US_COMM)
			{
				us_run_status = US_IDLE;
				sig_run_status = OFF;
			}
			holdingReg[H_REG_STOP] = 0;
		}
		else if(holdingReg[H_REG_DELAY1] != limit_delay_time1)
		{
			temp_i = holdingReg[H_REG_DELAY1];
			if(temp_i > 500) temp_i = 500;
			limit_delay_time1 = temp_i;
			save_param_fram(ADDR_DELAY1, limit_delay_time1, fram_mode);
		}
		else if(holdingReg[H_REG_DELAY2] != limit_delay_time2)
		{
			temp_i = holdingReg[H_REG_DELAY2];
			if(temp_i > 500) temp_i = 500;
			limit_delay_time2 = temp_i;
			save_param_fram(ADDR_DELAY2, limit_delay_time2, fram_mode);
		}
		else if(holdingReg[H_REG_DELAY3] != limit_delay_time3)
		{
			temp_i = holdingReg[H_REG_DELAY3];
			if(temp_i > 2000) temp_i = 2000;
			limit_delay_time3 = temp_i;
			save_param_fram(ADDR_TRIGGER2, limit_delay_time3, fram_mode);
		}
		else if(holdingReg[H_REG_TRIGGER2] != limit_trigger_time2)
		{
			temp_i = holdingReg[H_REG_TRIGGER2];
			if(temp_i > 500) temp_i = 500;
			limit_trigger_time2 = temp_i;
			save_param_fram(ADDR_DELAY2, limit_trigger_time2, fram_mode);
		}
		else if(holdingReg[H_REG_TRIGGER3] != limit_trigger_time3)
		{
			temp_i = holdingReg[H_REG_TRIGGER3];
			if(temp_i > 2000) temp_i = 2000;
			limit_trigger_time3 = temp_i;
			save_param_fram(ADDR_TRIGGER3, limit_trigger_time3, fram_mode);
		}
		else if(holdingReg[H_REG_OUT_POWER] != output_power)
		{
			temp_i = holdingReg[H_REG_OUT_POWER];
			if(temp_i > 100) temp_i = 100;
			else if(temp_i < 50) temp_i = 50;
			output_power = (uint8_t)temp_i;
			save_byte_fram(ADDR_OUT_POWER, output_power, fram_mode);
		}
		else if(holdingReg[H_REG_ON_TIME] != limit_on_time)
		{
			temp_i = holdingReg[H_REG_ON_TIME];
			if(temp_i > 2000) temp_i = 2000;
			limit_on_time = temp_i;
			save_param_fram(ADDR_ON_TIME, limit_on_time, fram_mode);
		}
		else if(holdingReg[H_REG_ENERGY] != (uint16_t)limit_energy)
		{
			temp_i = holdingReg[H_REG_ENERGY];
			limit_energy = (uint32_t)temp_i;
			save_word_fram(ADDR_ENERGY, limit_energy, fram_mode);
		}
		else if(holdingReg[H_REG_MULTI_T1] != limit_mo_time1)
		{
			temp_i = holdingReg[H_REG_MULTI_T1];
			if(temp_i > 2000) temp_i = 2000;
			limit_mo_time1 = temp_i;
			save_param_fram(ADDR_MULTI_T1, limit_mo_time1, fram_mode);
		}
		else if(holdingReg[H_REG_MULTI_T2] != limit_mo_time2)
		{
			temp_i = holdingReg[H_REG_MULTI_T2];
			if(temp_i > 2000) temp_i = 2000;
			limit_mo_time2 = temp_i;
			save_param_fram(ADDR_MULTI_T2, limit_mo_time2, fram_mode);
		}
		else if(holdingReg[H_REG_MULTI_O1] != limit_mo_out1)
		{
			temp_i = holdingReg[H_REG_MULTI_O1];
			if(temp_i > 100) temp_i = 100;
			else if(temp_i < 50) temp_i = 50;
			limit_mo_out1 = (uint8_t)temp_i;
			save_param_fram(ADDR_MULTI_O1, limit_mo_out1, fram_mode);
		}
		else if(holdingReg[H_REG_MULTI_O2] != limit_mo_out2)
		{
			temp_i = holdingReg[H_REG_MULTI_O2];
			if(temp_i > 100) temp_i = 100;
			else if(temp_i < 50) temp_i = 50;
			limit_mo_out2 = (uint8_t)temp_i;
			save_param_fram(ADDR_MULTI_O2, limit_mo_out2, fram_mode);
		}
		else if(holdingReg[H_REG_TIMEOVER] != limit_out_time)
		{
			limit_out_time = (uint8_t)holdingReg[H_REG_TIMEOVER];
			if(limit_out_time > 10) limit_out_time = 10;
			holdingReg[H_REG_TIMEOVER] = (uint16_t)limit_out_time;
			save_byte_fram(ADDR_TIMEOVER, limit_out_time, fram_mode);
		}
		else if(holdingReg[H_REG_RUN_MODE] != run_mode)
		{
			run_mode = (uint8_t)holdingReg[H_REG_RUN_MODE];
			save_byte_fram(ADDR_RUN_MODE, run_mode, fram_mode);
		}
		else if(((holdingReg[H_REG_EN_ENERGY] == 0) && (energy_ctrl == true)) || ((holdingReg[H_REG_EN_ENERGY] == 1) && (energy_ctrl == false)))
		{
			temp_i = holdingReg[H_REG_EN_ENERGY];
			if(temp_i == 1)
				energy_ctrl = true;
			else
				energy_ctrl = false;
			save_byte_fram(ADDR_EN_ENERGY, (uint8_t)temp_i, fram_mode);
			send_lcd_data_var(DISP_ENERGY_EN, (uint16_t)temp_i);
		}
		else if(((holdingReg[H_REG_EN_MULTI] == 0) && (multi_ctrl == true)) || ((holdingReg[H_REG_EN_MULTI] == 1) && (multi_ctrl == false)))
		{
			temp_i = holdingReg[H_REG_EN_MULTI];
			if(temp_i == 1)
				multi_ctrl = true;
			else
				multi_ctrl = false;
			save_byte_fram(ADDR_EN_MULTI, (uint8_t)temp_i, fram_mode);
			send_lcd_data_var(DISP_MULTI_EN, (uint16_t)temp_i);
		}
		else if(holdingReg[H_REG_EN_SAFTY] != f_safty)
		{
			f_safty = (uint8_t)holdingReg[H_REG_EN_SAFTY];
			save_byte_fram(ADDR_SAFTY, f_safty, fram_mode);
			send_lcd_data_var(DISP_SAFTY, (uint16_t)f_safty);
		}
		else if((holdingReg[H_REG_WORK_CNTL] == 0) && (holdingReg[H_REG_WORK_CNTL] != (uint16_t)work_cnt))		//work_count
		{
			holdingReg[H_REG_WORK_CNTH] = 0;
			holdingReg[H_REG_WORK_CNTL] = 0;
			work_cnt = 0;
			save_word_fram(ADDR_WORK_CNT, work_cnt, fram_mode);
		}
	}
	else
	{
		holdingReg[H_REG_WORK_CNTH] = (uint16_t)(work_cnt >> 16);
		holdingReg[H_REG_WORK_CNTL] = (uint16_t)(work_cnt);
		holdingReg[H_REG_DELAY1] = (uint16_t)limit_delay_time1;
		holdingReg[H_REG_DELAY2] = (uint16_t)limit_delay_time2;
		holdingReg[H_REG_DELAY3] = (uint16_t)limit_delay_time3;
		holdingReg[H_REG_TRIGGER2] = (uint16_t)limit_trigger_time2;
		holdingReg[H_REG_TRIGGER3] = (uint16_t)limit_trigger_time3;
		holdingReg[H_REG_OUT_POWER] = (uint16_t)output_power;
		holdingReg[H_REG_ON_TIME] = (uint16_t)limit_on_time;
		holdingReg[H_REG_ENERGY] = (uint16_t)limit_energy;
		holdingReg[H_REG_MULTI_T1] = (uint16_t)limit_mo_time1;
		holdingReg[H_REG_MULTI_T2] = (uint16_t)limit_mo_time2;
		holdingReg[H_REG_MULTI_O1] = (uint16_t)limit_mo_out1;
		holdingReg[H_REG_MULTI_O2] = (uint16_t)limit_mo_out2;
		holdingReg[H_REG_TIMEOVER] = (uint16_t)limit_out_time;
		holdingReg[H_REG_DISP_POWER] = (us_on_status == ON) ? (uint16_t)max_power :(uint16_t)last_power;
		holdingReg[H_REG_DISP_AMP] = (us_on_status == ON) ? (uint16_t)max_amp :(uint16_t)last_amp;
		holdingReg[H_REG_DISP_FREQ] = (us_on_status == ON) ? (uint16_t)curr_freq : (uint16_t)last_freq;
		holdingReg[H_REG_DISP_ENERGY] = (us_on_status == ON) ? (uint16_t)curr_energy :(uint16_t)last_energy;
		holdingReg[H_REG_MODEL_FREQ] = (uint16_t)model_freq;
		holdingReg[H_REG_MODEL_TYPE] = (uint16_t)model_type;
		holdingReg[H_REG_RUN_MODE] = (uint16_t)run_mode;
		holdingReg[H_REG_EN_ENERGY] = (energy_ctrl == true) ? 1 : 0;
		holdingReg[H_REG_EN_MULTI] = (multi_ctrl == true) ? 1 : 0;
		holdingReg[H_REG_EN_SAFTY] = (uint16_t)f_safty;
	}
	
	i = 0;
	if(sig_run_status == ON)
		i += STATUS_US;
	if(sys_status == SYS_ESTOP)
		i += STATUS_ESTOP;
	else if(sys_status == SYS_ERROR)
	{
		if((error_status & ERR_OVLD) == ERR_OVLD)
			i += STATUS_OVLD;
		if((error_status & ERR_OVTIME) == ERR_OVTIME)
			i += STATUS_OVTIME;
		if((error_status & ERR_OUTERR) == ERR_OUTERR)
			i += STATUS_OUTERR;
	}
	holdingReg[H_REG_STATUS] = i;

}

void check_modbus(void)
{
	uint16_t temp_i;
	
	if(holdingReg[H_REG_RESET] == 1)		//RESET
	{
		if( us_reset_status != US_REMOTE)
		{
			us_reset_status = US_TOUCH;
			sig_reset_status = ON;
			max_us_reset_cnt = MAX_US_RESET_CNT;
			re_outerr_issued = 0; 
			if((sys_status == SYS_ERROR) && (((error_status & ERR_OVLD) == ERR_OVLD) || ((error_status & ERR_OUTERR) == ERR_OUTERR)|| ((error_status & ERR_OVTIME) == ERR_OVTIME)))
			{
				send_lcd_data_var(ICON_OL, (uint16_t)0);
				send_lcd_data_var(ICON_OUTERR, (uint16_t)0);
				if(model_type == 0)		//hand
					lcd_status = LCD_RUN_HAND;
				else if(model_type == 1)		//multi
					lcd_status = LCD_RUN_MULTI;
				else if(model_type == 2)		//standrd
					lcd_status = LCD_RUN_STD;
							
				set_lcd_page(lcd_status);
				error_status = 0;
			}
		}
		holdingReg[H_REG_RESET] = 0;
	}
	else if(holdingReg[H_REG_SEEK] == 1)		//SEEK
	{
		if( us_seek_status != US_REMOTE)
		{
			us_seek_status = US_COMM;
			sig_seek_status = ON;
			max_us_reset_cnt = MAX_US_RESET_CNT;
		}
		holdingReg[H_REG_SEEK] = 0;
	}
	else if(holdingReg[H_REG_START] == 1)		//RUN(press)
	{
		if( us_run_status != US_REMOTE)
		{
			us_run_status = US_COMM;
			sig_run_status = ON;
			us_on_time = 0;
			curr_energy = last_energy = max_amp = max_power = acc_energy = 0;
			temp_i = (((output_power-50) * 255) / 100 );
			i2c_adc_write_byte(I2C_POT, I2C_POT_WP, (uint8_t)temp_i);
		}
		holdingReg[H_REG_START] = 0;
	}
	else if(holdingReg[H_REG_STOP] == 1)		//RUN(release)
	{
		if( us_run_status == US_COMM)
		{
			us_run_status = US_IDLE;
			sig_run_status = OFF;
		}
		holdingReg[H_REG_STOP] = 0;
	}
	

}
//================================================================================

int16_t comm_status;
bool run_user_applications = false;

// comm status error
#define ETH_NO_ERROR    0
#define ETH_PHY_ERROR   -3
#define ETH_IP_ERROR    -2
#define ETH_DHCP_ERROR  -1

//////////////////////////////////////////////////
// Socket & Port number definition for Examples //
//////////////////////////////////////////////////

#define SOCK_DHCP       6

#define SOCK_TCPS       0
#define PORT_TCPS		502

////////////////////////////////////////////////
// Shared Buffer Definition for Loopback test //
////////////////////////////////////////////////
#define DATA_BUF_SIZE   2048
uint8_t gDATABUF[DATA_BUF_SIZE];

uint8_t server_status;
int16_t f_tcp_data_cnt;
uint8_t g_tcp_data_buf[DATA_BUF_SIZE];

/////////////////////
// PHYStatus check //
/////////////////////
#define SEC_PHYSTATUS_CHECK 		1		// sec
bool PHYStatus_check_enable = false;
bool PHYStatus_check_flag = true;

////////////////
// DHCP client//
////////////////
#define MY_MAX_DHCP_RETRY			2
uint8_t my_dhcp_retry = 0;

/*****************************************************************************
 * Private functions
 ****************************************************************************/
/*******************************************************
 * @ brief Call back for ip assing & ip update from DHCP
 *******************************************************/
void my_ip_assign(void)
{
   getIPfromDHCP(gWIZNETINFO.ip);
   getGWfromDHCP(gWIZNETINFO.gw);
   getSNfromDHCP(gWIZNETINFO.sn);
   getDNSfromDHCP(gWIZNETINFO.dns);
   gWIZNETINFO.dhcp = NETINFO_DHCP;
   /* Network initialization */
   Net_Conf();      // apply from DHCP
}

/************************************
 * @ brief Call back for ip Conflict
 ************************************/
void my_ip_conflict(void)
{
#ifdef _MAIN_DEBUG_
	printf("CONFLICT IP from DHCP\r\n");
#endif
   //halt or reset or any...
   while(1); // this example is halt.
}

static void PHYStatus_Check(void)
{
	uint8_t tmp;
    
    printf("PHY check\r\n");
	do
	{
		ctlwizchip(CW_GET_PHYLINK, (void*) &tmp);
	}while(tmp == PHY_LINK_OFF);
}

void check_PHYstatus(void)
{
	uint8_t tmp, i;

    if(PHYStatus_check_flag)
    {
	    PHYStatus_check_flag = false;
	    if(comm_status == ETH_PHY_ERROR)
	    {
		    printf("PHY check\r\n");
		    ctlwizchip(CW_GET_PHYLINK, (void*) &tmp);
		    if(tmp != PHY_LINK_OFF) {
			    if(gWIZNETINFO.dhcp == NETINFO_DHCP)
			    {
				    DHCP_init(SOCK_DHCP, g_tcp_data_buf);
				    // if you want different action instead default ip assign, update, conflict.
				    // if cbfunc == 0, act as default.
				    reg_dhcp_cbfunc(my_ip_assign, my_ip_assign, my_ip_conflict);
			    }

			    comm_status = ETH_IP_ERROR;
		    }
	    }
	    else
	    {
		    if(gWIZNETINFO.dhcp == NETINFO_STATIC)
		    {
			    ctlnetwork(CN_SET_NETINFO, &gWIZNETINFO);
			    print_network_information();
			    run_user_applications = true; 	// flag for running user's code
			    comm_status = ETH_NO_ERROR;
			    //jig_status |= JIG_ETHERNET;
		    }
	    }

	    /* DHCP */
	    /* DHCP IP allocation and check the DHCP lease time (for IP renewal) */

	    if((gWIZNETINFO.dhcp == NETINFO_DHCP)&&(comm_status == ETH_IP_ERROR))
	    {
		    switch(DHCP_run())
		    {
			    case DHCP_IP_ASSIGN:
			    case DHCP_IP_CHANGED:
					/* If this block empty, act with default_ip_assign & default_ip_update */
					//
					// This example calls my_ip_assign in the two case.
					//
					// Add to ...
					//
			    break;
			    case DHCP_IP_LEASED:
					//
					// TODO: insert user's code here
					for(i = 0 ; i < 4 ; i++)
					{
						ether_ip[i] = gWIZNETINFO.ip[i];
						ether_nm[i] = gWIZNETINFO.sn[i];
						ether_gw[i] = gWIZNETINFO.gw[i];
					}
					run_user_applications = true;
					comm_status =ETH_NO_ERROR;
					//
			    break;
			    case DHCP_FAILED:
					/* ===== Example pseudo code =====  */
					// The below code can be replaced your code or omitted.
					// if omitted, retry to process DHCP
					my_dhcp_retry++;
					if(my_dhcp_retry > MY_MAX_DHCP_RETRY)
					{
						gWIZNETINFO.dhcp = NETINFO_STATIC;
						DHCP_stop();      // if restart, recall DHCP_init()
						my_dhcp_retry = 0;
					}
			    break;
			    default:
			    break;
		    }
	    }
    }

}
void init_eth_ip(void)
{
	gWIZNETINFO.ip[0] = ether_ip[0];
	gWIZNETINFO.ip[1] = ether_ip[1];
	gWIZNETINFO.ip[2] = ether_ip[2];
	gWIZNETINFO.ip[3] = ether_ip[3];

	gWIZNETINFO.sn[0] = ether_nm[0];
	gWIZNETINFO.sn[1] = ether_nm[1];
	gWIZNETINFO.sn[2] = ether_nm[2];
	gWIZNETINFO.sn[3] = ether_nm[3];

	gWIZNETINFO.gw[0] = ether_gw[0];
	gWIZNETINFO.gw[1] = ether_gw[1];
	gWIZNETINFO.gw[2] = ether_gw[2];
	gWIZNETINFO.gw[3] = ether_gw[3];
}

uint8_t parse_comm(void)
{
	uint8_t ret_val;
	
	ret_val = 0; // not receive
	
	if(received[0] == device_addr)
	{
		ret_val = 1;
	}
	modbusMessage = 0;
	
	return ret_val;
}


// ===============================================================================

int main (void)
{
	uint8_t i, j;
	uint16_t temp;
	uint8_t * array_ptr;
	
	int32_t error, ret;
	
//	struct events_resource example_event;

	fram_mode = 0;
	
	system_init();
	
	configure_adc();
	configure_adc_callbacks();

	
	evsys_configuration();
	configure_tc_FC();
	extint_configuration();
	system_interrupt_enable(TC0_IRQn);
//	system_interrupt_enable_global();
	
	configure_rtc_count();
	configure_rtc_callbacks();
	rtc_count_set_period(&rtc_instance, 1000);
	init_lcd_comm();
	
	configure_i2c_master();

	delay_init();
/*	
	configure_usart_DRV1();
	configure_usart_callbacks_DRV1();
	
	configure_wdt_off();
	
	run_status = MODE_RUN;
	configure_DAC_spi_master();

*/
	buzzer_off();
	bz_duty = BZ_OFF;
	bz_ctrl = 1;
//	configure_tc_BZ();
//	configure_tc_callbacks();

	system_interrupt_enable_global();
	
	var_init();
	for(i = 0, j = 0 ; i < 100 ; i++)
	{
		temp = port_pin_get_input_level(SW_CNT_RESET);
		if(temp == 0)
		{
			i = 0;
			j++;
		} 
//		else 
//		{
//			break;
//		}
		if(j >= 200)
		{
			factory_init();
			break;
		}
		delay_ms(10);
	}

	set_lcd_page(0);

	delay_ms(1000);
	//	if(check_lcd_comm(0))
	//	{
	//		init_lcd_mode();
	//		break;
			
	//	}
	
	init_lcd_mode();
	delay_ms(100);
	system_interrupt_disable_global();
	
	
	if(comm_mode == COMM_SERIAL){
		init_modbus(comm_speed_idx, comm_parity_idx, comm_address);
	} else {
		if(comm_mode == COMM_ETH_DHCP)
			gWIZNETINFO.dhcp = NETINFO_DHCP;
		else
			gWIZNETINFO.dhcp = NETINFO_STATIC;
		comm_status = ETH_PHY_ERROR;
		
		init_eth_ip();
		
		if(ethernet_init() == 0)
		{
			//print_network_information();
			comm_status = ETH_IP_ERROR;
			/* DHCP client Initialization */
			if(gWIZNETINFO.dhcp == NETINFO_DHCP)
			{
				DHCP_init(SOCK_DHCP, gDATABUF);
				// if you want different action instead default ip assign, update, conflict.
				// if cbfunc == 0, act as default.
				reg_dhcp_cbfunc(my_ip_assign, my_ip_assign, my_ip_conflict);

				run_user_applications = false; 	// flag for running user's code
				comm_status = -1;
			}
			//else
			//{
				// Static
			//	run_user_applications = true; 	// flag for running user's code
			//	comm_status = ETH_NO_ERROR;
			//}
		}
		
	}
	system_interrupt_enable_global();
/*
	if(sys_mode != SYS_FND)
	{
		i = j = 0;
		while(1){
			if(++i > 300) break;	// wait 2 sec
			if(check_lcd_comm(0))
			{
				i = 0;
				if((lcd_buf[LCD_COMM_MODE] == LCD_RD)&&(lcd_buf[LCD_COMM_ADDR_H] == 0x50)&&(lcd_buf[LCD_COMM_ADDR_L] == 0x00))
				{
					if((lcd_buf[LCD_COMM_DATA_L] == 1))
					{
						j = 1;
					}
					else if((lcd_buf[LCD_COMM_DATA_L] == 2) && ( j == 1))
					{
						j = 2;
					}
					else if((lcd_buf[LCD_COMM_DATA_L] == 3) && ( j == 2))
					{
						j = 3;
					}
					else if((lcd_buf[LCD_COMM_DATA_L] == 4) && ( j == 3))
					{
						set_lcd_page(LCD_MODEL_SETUP);
						lcd_status = LCD_MODEL_SETUP;
						send_model_str(model_freq, model_type);
						send_lcd_data_var(MODEL_FREQ, (uint16_t)model_freq);
						send_lcd_data_var(MODEL_TYPE, (uint16_t)model_type);
						break;
					}
				}
			}
			delay_ms(10);
		}
		if(lcd_status == LCD_LOGO)
		{
			init_lcd_mode();
		}
	}
*/	
	disp_timer = 0;
	boot_status = 0;
	lcd_var_init();
//	load_val();
	update_holding_reg(0);
	send_model_str(model_freq, model_type);
	read_system_var(SYS_PIC_NOW);
	while(1) {

		if(port_pin_get_input_level(SW_SET) == 0) {
		}
		else
		{
			if(set_key_f == 1)
			{
				sw_set_pressed = 0;
				set_key_f = 0;
			}
		}
		
		if((comm_mode == COMM_SERIAL)&&(comm_address != 0))
		{
			if((modbusMessage != 0)&&(messageLength != 0))
			{
				modbus_status = decode_comm(COMM_SERIAL);
				if(modbus_status == 0x06)
				{
					update_holding_reg(1);
				}
				else
				{
					update_holding_reg(0);
				}
//				check_modbus();
				modbus_comm_cnt = 0;
			}
		}
		
		if(tick_1ms_state != bak_1ms_state) {		// every 1mS
			bak_1ms_state = tick_1ms_state;

			//display_FND();
			if(run_user_applications)
			{
				if(tick_1ms_state & 0x01)
				{
                    if((ret = control_tcps(SOCK_TCPS, &server_status, PORT_TCPS)) == 0)
                    {
	                    sprintf(error_str, "SOCKET ERROR");
                    }
                    else
                    {
	                    if(server_status == SERVER_CONNECT)
	                    {
		                    sprintf(error_str, "connected");
	                    }
                    }
					
					if((server_status == SERVER_CONNECT)||(server_status == SERVER_ESTABLISHED))
					{
						f_tcp_data_cnt = recv_tcps(SOCK_TCPS, g_tcp_data_buf);
					}
					if(f_tcp_data_cnt > 0)
					{
						for(i = 0 ; (i < 12) ; i++)
						{
							received[i] = g_tcp_data_buf[6+i];
						}
						modbus_status = decode_comm(COMM_ETH_STATIC);
						if(modbus_status == 0x06)
						{
							update_holding_reg(1);
						}
						else
						{
							update_holding_reg(0);
						}
					}
				}
			}
			else
			{
				if(comm_mode != COMM_SERIAL)
					check_PHYstatus();
			}
			if(tick_1ms_state & 0x01)
			{
				switch(job_state) {
					case 0 :		// process comm data
						check_remote_input();
						send_outpower_data(job_state, curr_amp, output_power);
						job_state++;
						break;
					case 1 :		// 80us
						do_action();
						send_outpower_data(job_state, curr_amp, output_power);
						job_state++;
						break;
					case 2 :		// 80us
						if(check_lcd_comm(1))
						{
							parse_lcd_comm();
						}
						send_outpower_data(job_state, curr_amp, output_power);
						job_state++;
						break;
					case 3 :		// 80us
						do_control();
						send_outpower_data(job_state, curr_amp, output_power);
						job_state++;
						break;
					case 4 :		// 80us
//						send_outpower_data(curr_lv/40, output_power);
						send_outpower_data(job_state, curr_amp, output_power);
						job_state++;
						break;
					case 5 :		// 80us
						if(sig_run_status == ON)
							send_outtime_data(job_state, us_on_time_200m);					
						else
							send_outtime_data(job_state, last_time);
					/*
						if(my_adc_read_buffer_job(&adc_instance, adc_result_buffer[adc_current_ch], ADC_SAMPLE) == STATUS_OK) {
							cal_real_val();
							adc_current_ch++;
							if(adc_current_ch == ADC_CHANNEL) { 
								adc_current_ch = 0; 
							}
							adc_change_ch();
							start_adc(&adc_instance, adc_result_buffer[adc_current_ch], ADC_SAMPLE);
						}
					*/
						job_state++;
						break;
					case 6 :		// 80us
						calc_freq();
						if(sig_run_status == ON)
							send_outtime_data(job_state, us_on_time_200m);					
						else
							send_outtime_data(job_state, last_time);
						job_state++;
						break;
					case 7 :		// 80us
						send_val_data();
						if(sig_run_status == ON)
							send_outtime_data(job_state, us_on_time_200m);					
						else
							send_outtime_data(job_state, last_time);
						job_state++;
						break;
					case 8 :		// 80us
						if(sig_run_status == ON)
							send_outtime_data(job_state, us_on_time_200m);					
						else
							send_outtime_data(job_state, last_time);
						job_state++;
						break;
					case 9 :		// 80us
					default:
						if(sig_run_status == ON)
							send_outtime_data(job_state, us_on_time_200m);					
						else
							send_outtime_data(job_state, last_time);
						job_state = 0;
						if(comm_address != 0)
						{
							if(++modbus_comm_cnt > 100)
							{
								modbus_comm_cnt = 100;
								modbus_status = 0;
							}
							if(modbus_status != bak_modbus_status)
								if(modbus_status != 0)
									send_lcd_data_var(DISP_REMOTE, (uint16_t)1);
								else
									send_lcd_data_var(DISP_REMOTE, (uint16_t)0);
							bak_modbus_status = modbus_status;
						}
						break;
				}
				
			}				
			sys_tick_func();
			
			if(my_adc_read_buffer_job(&adc_instance, adc_result_buffer[adc_current_ch], ADC_SAMPLE) == STATUS_OK) {
				cal_real_val();
				adc_current_ch++;
				if(adc_current_ch == ADC_CHANNEL) {
					adc_current_ch = 0;
				}
				adc_change_ch();
				start_adc(&adc_instance, adc_result_buffer[adc_current_ch], ADC_SAMPLE);
			}

			if(first_1sec) continue;
			if(sig_run_status == ON)
			{
				if(base_cnt >= 200)
				{		// every 200ms
					base_cnt = 0;
					if(us_on_time_200m < 200) us_on_time_200m++;
				}
			
				//if((base_cnt  % 100) == 0)	
				//{
				//	if(us_on_time < 200) us_on_time++;	// every 100mS max 20s
				//}
			}
			
			if(sig_run_status == ON)
			{			// on
				if(((us_run_status == US_COMM)||(us_run_status == US_REMOTE)||(run_status == RUN_WELD))&&(multi_ctrl == true))
				{
					if(multi_ctrl_stage == 0)
					{
						if(us_on_time >= (limit_mo_time1 / 10))
						{
							multi_ctrl_stage = 1;
							temp = ((limit_mo_out2-50) * 255) / 100 ;
							i2c_adc_write_byte(I2C_POT, I2C_POT_WP, (uint8_t)temp);
						}
						
					} 
					else 
					{
						if(us_on_time >= (limit_mo_time2 / 10))
						{
							if(run_status == RUN_WELD)
							{
								port_pin_set_output_level(M_START, CTRL_INT_OFF);
								sig_run_status = OFF;
								us_on_status = OFF;
								f_status_start = 0;
								run_status = RUN_HOLD;
								if((run_mode == MODE_DELAY)||(run_mode == MODE_TRIGGER))
									temp_time = limit_delay_time3;
							}
							else
							{
								us_off();
							}							
						}
					}
					
				}
				else
				{
					if(((us_run_status == US_COMM)||(us_run_status == US_REMOTE)||(run_status == RUN_WELD))&&(energy_ctrl == true))
					{
						if(curr_energy >= limit_energy)
						{
							if(run_status == RUN_WELD)
							{
								port_pin_set_output_level(M_START, CTRL_INT_OFF);
								sig_run_status = OFF;
								f_status_start = 0;
								run_status = RUN_HOLD;
								if(run_mode == MODE_DELAY)
									temp_time = limit_delay_time3;
								else
									temp_time = limit_trigger_time3;
							}
							else
								us_off();
						}
						else if(us_on_time >= (limit_out_time * 10))
						{
							us_off();
							sys_status = SYS_ERROR;
							error_status |= ERR_OVTIME;
							f_status_start = 0;
						}
					}
					else if(((us_run_status == US_COMM)||(us_run_status == US_REMOTE))&&(sys_mode == SYS_HAND))
					{
						if(us_on_time >= (limit_on_time / 10))
						{
							us_off();
						}
					}
				}
			}			
		}
	}
}

void sys_tick_func(void)
{
	static uint16_t phystatus_check_cnt = 0;

	if(tick_1ms >= 10) {
    	tick_1ms = 0;
        tick_10ms++;
//		LED_Toggle(LED_IND);

        if(PHYStatus_check_enable)
        {
	        if (phystatus_check_cnt++ > (100 * SEC_PHYSTATUS_CHECK))
	        {
		        PHYStatus_check_flag = true;
		        phystatus_check_cnt = 0;
	        }
        }
        else
        {
	        phystatus_check_cnt = 0;
        }

		check_input();

		if(sys_status != SYS_SETUP)
			if(temp_time > 0) temp_time--;

		if(boot_status == 1)
		{
			
		}

		if(++run_led_toggle_cnt >= run_led_toggle_time)
		{
			run_led_toggle_cnt = 0;
		}
		
// =================================== sw input
	}
	if( tick_10ms > 9 ) {
    	tick_10ms = 0;
        tick_100ms++;
		
		blink_cnt++;
		
		if(comm_mode != COMM_SERIAL){
            if(PHYStatus_check_enable)
            {
	            if (phystatus_check_cnt++ > (100 * SEC_PHYSTATUS_CHECK))
	            {
		            PHYStatus_check_flag = true;
		            phystatus_check_cnt = 0;
	            }
            }
            else
            {
	            phystatus_check_cnt = 0;
            }
		}
// ============ 100msec routine =============
		if(blink_cnt > 2) {	// when mode switch pressed 7-segment blink
			blink_cnt = 0;
			if(mode_blink == 0) mode_blink = 1;
			else mode_blink = 0;
		}

		disp_update(run_mode);
		led_update();
		
		if((sig_run_status == ON) )
		{
			if(us_on_time < 200) us_on_time++;	// every 100mS max 20s
		}
		
		
		if(first_1sec != 1) {
//			command_out(target_iled[CH_W],target_iled[CH_C]);
		}
		
		if((us_reset_mode == RESET_ISSUE) || (us_reset_mode == SEEK_ISSUE))
		{
			us_reset_cnt++;
			if(us_reset_cnt > max_us_reset_cnt)
			{
				if(((us_reset_status == US_TOUCH)||(us_reset_status == US_COMM)) && (us_reset_mode == RESET_ISSUE))
				{
					sig_reset_status = OFF;
					sig_seek_status = ON;
					us_reset_cnt = 0;
					max_us_reset_cnt = MAX_US_RESET_CNT;
					us_reset_status = US_IDLE;
					us_seek_status = US_TOUCH;
				}

				if(((us_seek_status == US_TOUCH)||(us_reset_status == US_COMM)) && (us_reset_mode == SEEK_ISSUE))
				{
					us_seek_status = US_IDLE;
					sig_seek_status = OFF;
				}
			}
		}

// ==========================================
	}
	if( tick_100ms > 9 ) {
    	tick_100ms = 0;
        tick_1s++;

		if(first_1sec){
			 first_1sec = 0;
		}
// ============ 1sec routine ================
		tick_error++;
		if(tick_error > TICK_ERROR_SEC) {
			tick_error = 0;
//			if(error_num != 0) fnd_display_error = 10;
		}

		if(comm_mode != COMM_SERIAL){
			if(comm_status == ETH_NO_ERROR)
				DHCP_time_handler();
			else
			{
				PHYStatus_check_flag = true;
			}
		}
// ==========================================
	}
	if ( tick_1s > 9 ) {
		tick_1s = 0;
		tick_10s++;
// =========== 10sec routine ================
// ==========================================
	}
	/* Check if 10's reach top value and reset to zero if it does. */
	if ( tick_10s > 5 ) {
		tick_10s = 0;
// =========== 1min routine =================
// ==========================================
	}
}
