/*
 * dgus_lcd.h
 *
 * Created: 2021-11-25 오전 11:44:32
 *  Author: tknoh
 */ 

#ifndef DGUS_LCD_H_
#define DGUS_LCD_H_

#define USART_LCD_MODULE              SERCOM1
#define USART_LCD_SERCOM_MUX_SETTING  USART_RX_1_TX_0_XCK_1
#define USART_LCD_SERCOM_PINMUX_PAD0  PINMUX_PA00D_SERCOM1_PAD0
#define USART_LCD_SERCOM_PINMUX_PAD1  PINMUX_PA01D_SERCOM1_PAD1
#define USART_LCD_SERCOM_PINMUX_PAD2  PINMUX_UNUSED
#define USART_LCD_SERCOM_PINMUX_PAD3  PINMUX_UNUSED

#define MAX_LCD_BUFFER_LENGTH   50

// lcd display mode
#define LCD_LOGO		0
#define LCD_MODEL_SETUP 1
#define LCD_RUN_MULTI	3
#define LCD_SETUP_MULTI	5
#define LCD_RUN_HAND	3
#define LCD_SETUP_HAND	7
#define LCD_SETUP_MH2	19
#define LCD_SETUP_MHC	21
#define LCD_RUN_STD		9
#define LCD_SETUP_STD1	10
#define LCD_SETUP_STD2D	12
#define LCD_SETUP_STD2T	13
#define LCD_SETUP_STD3	15
#define LCD_SETUP_STDC	23
#define LCD_WARNING		17

#define LCD_SETUP_MHE	25
#define LCD_SETUP_STDE	27

//--- comm
#define SYNC1		0x5a
#define SYNC2		0xa5

#define LCD_COMM_N	0
#define LCD_COMM_MODE	1
#define LCD_COMM_ADDR_H	2
#define LCD_COMM_ADDR_L	3
#define LCD_COMM_DATA_N	4
#define LCD_COMM_DATA_H	5
#define LCD_COMM_DATA_L	6

//---- LCD VP address
#define VP_LCD_RESET	0x04
#define VP_LCD_SETPAGE	0x84

#define SYS_PIC_NOW 0x0014

#define MODEL_NAME	0x1000
#define LV_WORK_CNT	0x1006
#define STD_SETUP_PARAM	0x1020
#define DATA_SAVE	0x1050
#define MODEL_FREQ	0x1060
#define MODEL_TYPE	0x1070
#define COMM_ADDR	0x1071
#define COMM_SPEED	0x1072
#define COMM_PARITY	0x1073

#define COMM_ADDR_TXT	0x1460
#define COMM_SPEED_TXT	0x1464
#define COMM_PARITY_TXT	0x146a

#define COMM_IP_TXT	0x1470
#define COMM_NM_TXT	0x1480
#define COMM_GW_TXT 0x1490

#define KEY_MULTI	0x1080
#define SETUP_MODEL	0x1084
#define ENERGY_EN	0x1085
#define SETUP_PARAM	0x1100
#define SETUP_PARAM_MOOHAN	0x1094
#define VAR_POWER	0x1110
#define VAR_AMP		0x1111
#define VAR_FREQ	0x1112
#define VAR_ENERGY	0x1113
#define ICON_RESET	0x1150
#define ICON_SEEK	0x1151
#define ICON_RUN	0x1152
#define ICON_OL		0x1153
#define ICON_OUTERR		0x1154
#define LV_OUTPUT	0x1170
#define LV_OUTPUT2	0x117a
#define LV_TIME		0x1184
#define LV_TIME2	0x118e
#define LV_OUT_POWER	0x1200
#define LV_ENERGY_VAL	0x1212
#define LV_MAX_ON_TIME	0x1201
#define LV_ENERGY_EDIT	0x1202
#define LV_DM_DELAY		0x1203
#define LV_DM_WELD		0x1204
#define LV_DM_HOLD		0x1205
#define LV_TM_WELD		0x1206
#define LV_TM_HOLD		0x1207
#define DISP_MULTI		0x1208
#define DISP_HORNDOWN	0x1209
#define DISP_ENERGY_EN	0x120a
#define DISP_MULTI_EN	0x120b
#define DISP_RUN_MODE	0x120c
#define DISP_SAFTY		0x120d
#define DISP_REMOTE		0x120e
#define LV_LIMIT_OUT_T	0x120f
#define LV_RUN_MODE		0x1210
#define LV_MO_OUT1		0x1400
#define LV_MO_OUT2		0x1401
#define LV_MO_TIME1		0x1402
#define LV_MO_TIME2		0x1403
#define M_DATA_SAVE	0x1250
#define VP_ERROR_MSG	0x1350
#define ICON_ERROR_RESET	0x1407
#define KEY_ERROR_RESET	0x1408
#define MULTI_EN	0x140a
#define DISP_STD_DATA1	0x1300
#define DISP_STD_DATA2	0x1310
#define DISP_STD_DATA3	0x1320
#define DISP_VERSION	0x1330

#define LV_COMM_MODE	0x140b
#define LV_ETHER_KEY	0x140f
#define DISP_COMM_MODE	0x140c
#define DISP_EN_DHCP	0x140d

#define VAR_CAL_VAL	0x1410
#define VAR_FREQ_CAL_VAL	0x1412

#define VV_OUTPOWER		0x1450
#define VV_LIMITENERGY	0x1452

#define KEY_UP		1
#define KEY_DN		0

#define KEY_CANCEL	0
#define KEY_SAVE	1

//---- LCD SP address
#define SP_CH1_3	0x5010
#define SP_CH1_0	0x5020
#define SP_CH1_1	0x5030
#define SP_CH2_0	0x5040

#define SP_OFFSET_COLOR	3

#define RED_H	0xf8
#define RED_L	0x00
#define GREEN_H		0x07
#define GREEN_L		0xe0


#define LCD_RD	0x83
#define LCD_WR	0x82

#define RD_OFFSET_H	5
#define RD_OFFSET_L	6

void init_lcd_comm(void);
void reset_dgus_lcd(void);
unsigned char check_lcd_comm(uint8_t mode);
void set_lcd_page(uint8_t lcd_page);
void send_lcd_data_var(uint16_t vp_addr, uint16_t vp_data);
void send_lcd_byte_array(uint16_t vp_addr, uint8_t length, uint8_t * byte_array);
void send_lcd_int_array(uint16_t vp_addr, uint8_t length, uint16_t * int_array);
void send_lcd_txt(uint16_t vp_addr, uint8_t * ptxt);
void send_lcd_data_word(uint16_t vp_addr, uint32_t vp_data);
void read_system_var(uint8_t var);


extern uint8_t lcd_buf[MAX_LCD_BUFFER_LENGTH];

#endif /* DGUS_LCD_H_ */