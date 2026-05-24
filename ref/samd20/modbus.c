/******************************************************************************/
/* Files to Include                                                           */
/******************************************************************************/

#include <asf.h>
#include <string.h>
#include <stdlib.h>
//#include "modbus.h"
#include "process_tcp.h" 

#define USART_MODBUS_MODULE              SERCOM2							// FOR DEBUG
#define USART_MODBUS_SERCOM_MUX_SETTING  USART_RX_1_TX_0_XCK_1
#define USART_MODBUS_SERCOM_PINMUX_PAD0  PINMUX_PA12C_SERCOM2_PAD0
#define USART_MODBUS_SERCOM_PINMUX_PAD1  PINMUX_PA13C_SERCOM2_PAD1
#define USART_MODBUS_SERCOM_PINMUX_PAD2  PINMUX_UNUSED
#define USART_MODBUS_SERCOM_PINMUX_PAD3  PINMUX_UNUSED

/******************************************************************************/
/* Global variables                                                           */
/******************************************************************************/
extern volatile unsigned int  holdingReg[50];
extern volatile unsigned char coils[50];
extern volatile unsigned char response[125]; //Enough to return all holding-r's
extern volatile unsigned char received[125]; //Enough to write all holding-r's
extern volatile char modbusMessage,messageLength;
extern volatile unsigned char device_addr;

volatile char endOfMessage,newMessage = 1;
volatile char timerCount,messageLength,modbusMessage,z = 0, max_break_cnt;
volatile unsigned char response[125]; //Enough to return all holding-r's
volatile unsigned char received[125]; //Enough to write all holding-r's
volatile unsigned char tx_modbus_buf[125], tx_cnt, tx_len;

/******************************************************************************/
/* User Function Prototypes                                                   */
/******************************************************************************/
unsigned char check_crc(void);
void clear_response(void);
uint8_t read_reg(void);         /* I/O and Peripheral Initialization */
uint8_t read_input_reg(void);
uint8_t write_reg(void);
uint8_t read_coil(void);
uint8_t read_input_coil(void);
uint8_t write_coil(void);
void modbus_delay(void);              /* Writes to Timer0 for 1.04ms delay*/
uint8_t decode_comm(uint8_t);
unsigned int make_crc(unsigned char);


// ========================= TC ====================================
#define MODBUS_DELAY_MODULE           TC1
#define MODBUS_DELAY_CHANNEL        0
#define MODBUS_DELAY_PIN            PIN_PA08E_TC0_WO0
#define MODBUS_DELAY_MUX            MUX_PA08E_TC0_WO0
#define MODBUS_DELAY_PINMUX         PINMUX_PA08E_TC0_WO0

void configure_tc_FAN(void);
void configure_tc_callbacks(void);
void tc_callback_delay(struct tc_module *const module_inst);
struct tc_module tc_instance_MDELAY;

void tc_callback_delay(struct tc_module *const module_inst)
{
	timerCount++;
	if(timerCount >= max_break_cnt)
	{
		endOfMessage = 1;
		newMessage = 1;
		messageLength = z;
		modbusMessage = 1;
		for(z=(messageLength);z!=125;z++) //Clear rest of message
		{
			received[z] = 0;
		}
		z=0;
		timerCount = 0;
	}
}

void configure_tc_delay(void)
{
	struct tc_config config_tc;
	tc_get_config_defaults(&config_tc);

	config_tc.counter_size    = TC_COUNTER_SIZE_8BIT;
	config_tc.clock_source = GCLK_GENERATOR_3;
	config_tc.clock_prescaler =  TC_CLOCK_PRESCALER_DIV16;		// 16M / 16 = 1M clock
	config_tc.counter_8_bit.period = 250;						// every 250 uS
	config_tc.counter_8_bit.compare_capture_channel[0] = 100;

	tc_init(&tc_instance_MDELAY, MODBUS_DELAY_MODULE, &config_tc);
	//	tc_set_top_value(&tc_instance_FAN,0xff);
	//	tc_instance_MDELAY.hw->COUNT8.DBGCTRL.reg = 1;
	tc_enable(&tc_instance_MDELAY);
}
void configure_tc_callbacks(void)
{
	tc_register_callback(&tc_instance_MDELAY,	tc_callback_delay,	TC_CALLBACK_CC_CHANNEL0);
	tc_enable_callback(&tc_instance_MDELAY, TC_CALLBACK_CC_CHANNEL0);

}

// ========================= USART =================================================================================
struct usart_module usart_instance_modbus;
volatile uint8_t rx_buffer_mon[125];
uint16_t rx_buf_modbus;



void usart_read_callback_modbus(const struct usart_module *const usart_module)
{
	usart_read_job(&usart_instance_modbus, &rx_buf_modbus);
    if((!endOfMessage)&&(!newMessage))
    {
	    received[z++] = rx_buf_modbus;
	    timerCount = 0;
    }
    if(newMessage)
    {
	    //open_tmr1();
		tc_set_count_value(&tc_instance_MDELAY, 0);
	    received[z++] = rx_buf_modbus;
	    newMessage = 0;
	    endOfMessage = 0;
	    messageLength = 0;
	    modbusMessage = 0;
	    timerCount = 0;
    }
}

void usart_write_callback_modbus(const struct usart_module *const usart_module)
{
}

void configure_usart_modbus(uint8_t speed, uint8_t parity)
{
	struct usart_config config_usart;
	usart_get_config_defaults(&config_usart);

	config_usart.generator_source = GCLK_GENERATOR_3;
	switch(speed)
	{
		case 0:		config_usart.baudrate    = 2400; break;
		case 1:		config_usart.baudrate    = 4800; break;
		case 2:		config_usart.baudrate    = 9600; break;
		case 3:		config_usart.baudrate    = 19200; break;
		case 4:		config_usart.baudrate    = 38400; break;
		case 5:		config_usart.baudrate    = 115200; break;
		default:	config_usart.baudrate    = 19200; break;
	}
	if(parity == 0)
		config_usart.parity = USART_PARITY_EVEN;
	else if(parity == 1)
		config_usart.parity = USART_PARITY_ODD;
	else
		config_usart.parity = USART_PARITY_NONE;
	config_usart.mux_setting = USART_MODBUS_SERCOM_MUX_SETTING;
	config_usart.pinmux_pad0 = USART_MODBUS_SERCOM_PINMUX_PAD0;
	config_usart.pinmux_pad1 = USART_MODBUS_SERCOM_PINMUX_PAD1;
	config_usart.pinmux_pad2 = USART_MODBUS_SERCOM_PINMUX_PAD2;
	config_usart.pinmux_pad3 = USART_MODBUS_SERCOM_PINMUX_PAD3;

	while (usart_init(&usart_instance_modbus,USART_MODBUS_MODULE, &config_usart) != STATUS_OK) {
	}

	usart_enable(&usart_instance_modbus);
	usart_read_job(&usart_instance_modbus, &rx_buf_modbus);

}

void configure_usart_callbacks_modbus(void)
{
	usart_register_callback(&usart_instance_modbus,usart_write_callback_modbus, USART_CALLBACK_BUFFER_TRANSMITTED);
	usart_register_callback(&usart_instance_modbus,usart_read_callback_modbus, USART_CALLBACK_BUFFER_RECEIVED);
	usart_enable_callback(&usart_instance_modbus, USART_CALLBACK_BUFFER_TRANSMITTED);
	usart_enable_callback(&usart_instance_modbus, USART_CALLBACK_BUFFER_RECEIVED);
}

/******************************************************************************/
/* User Functions                                                             */
/******************************************************************************/

void init_modbus(uint8_t speed, uint8_t parity, uint8_t addr)
{
	uint8_t i;
	
	configure_tc_delay();
	configure_tc_callbacks();
	
	if(speed == 2)	// 9600 bps
		max_break_cnt = 14;
	else if(speed == 1)	// 4800 bps
		max_break_cnt = 28;
	else if(speed == 0)	// 2400 bps
		max_break_cnt = 56;
	else if(speed >= 3) // over 19200 bps
		max_break_cnt = 7;
	else	// 
		max_break_cnt = 7;
	 
	configure_usart_modbus(speed, parity);
	configure_usart_callbacks_modbus();
	
	device_addr = addr;	
	for(i = 0 ; i < 50 ; i++)
	{
		holdingReg[i] = 0;
		coils[i] = 0;
	}
}

void close_modbus(void)
{
	usart_reset(&usart_instance_modbus);
}

void clear_response(void)
{
    unsigned char i;
    for(i=0;i<125;i++) //response is 125 long
    {
        response[i] = 0;
    }
}

void send_data(unsigned char length)
{
    unsigned char i;
    
    tx_len = length;
    tx_cnt = 0 ;
    for(i=0 ; i < length ;i++){
        tx_modbus_buf[i] = response[i];
    }
	usart_write_buffer_job(&usart_instance_modbus, tx_modbus_buf, length);
}

uint8_t decode_comm(uint8_t mode)
{
	uint8_t ret_val, send_cnt;
	
	ret_val = 0; // not receive
	send_cnt = 0;
	
    if((received[0] == device_addr) || (mode != 0))
    {
        if((check_crc()) || (mode != 0))
        {
            if(received[1] == 0x01)
            {
                send_cnt = read_coil();
				ret_val = 0x01;	//ok
            }
            else if(received[1] == 0x02)
            {
                send_cnt = read_input_coil();
				ret_val = 0x02;
            }
            else if(received[1] == 0x03)
            {
                send_cnt = read_reg();
				ret_val = 0x03;
            }
            else if(received[1] == 0x04)
            {
                send_cnt = read_input_reg();
				ret_val = 0x04;
            }
            else if(received[1] == 0x05)
            {
                send_cnt = write_coil();
				ret_val = 0x05;
            }	  
            else if(received[1] == 0x06)
            {
                send_cnt = write_reg();
				ret_val = 0x06;
            }
            else
            {
                response[0] = 0; //error this does nothing though..
            }
			
			if(send_cnt != 0)
			{
				if(mode != 0)
					send_tcps(0, response, send_cnt);
				else
					send_data(send_cnt);
				clear_response();
			}
        }
    }
    modbusMessage = 0;
	
	return ret_val;
}

uint8_t read_reg(void)
{
    unsigned int rr_Address = 0;
    unsigned int rr_numRegs = 0;
    unsigned char j = 3;
    unsigned int crc = 0;
    unsigned int i = 0;

    //Combine address bytes
    rr_Address = received[2];
    rr_Address <<= 8;
    rr_Address |= received[3];

    //Combine number of regs bytes
    rr_numRegs = received[4];
    rr_numRegs <<= 8;
    rr_numRegs |= received[5];

    response[0] = device_addr;
    response[1] = 0x03;
    response[2] = (unsigned char)(rr_numRegs*2); //2 bytes per reg

    for(i=rr_Address;i<(rr_Address + rr_numRegs);i++)
    {
        if(holdingReg[i] > 255)
        {
            //Need to split it up into 2 bytes
            response[j] = (unsigned char)(holdingReg[i] >> 8);
            j++;
            response[j] = (unsigned char)(holdingReg[i]);
            j++;
        }
        else
        {
            response[j] = 0x00;
            j++;
            response[j] = (unsigned char)(holdingReg[i]);
            j++;
        }
    }
    crc = make_crc(j);
    response[j] = (unsigned char)(crc >> 8);
    j++;
    response[j] = (unsigned char)(crc);
    j++;

    //send_data(j);
    //j=0;

    //clear_response();
	return j;
}

uint8_t read_input_reg(void)
{
    unsigned int rr_Address = 0;
    unsigned int rr_numRegs = 0;
    unsigned char j = 3;
    unsigned int crc = 0;
    unsigned int i = 0;

    //Combine address bytes
    rr_Address = received[2];
    rr_Address <<= 8;
    rr_Address |= received[3];

    //Combine number of regs bytes
    rr_numRegs = received[4];
    rr_numRegs <<= 8;
    rr_numRegs |= received[5];

    response[0] = device_addr;
    response[1] = 0x04;
    response[2] = (unsigned char)(rr_numRegs*2); //2 bytes per reg

    for(i=rr_Address;i<(rr_Address + rr_numRegs);i++)
    {
        if(holdingReg[i] > 255)
        {
            //Need to split it up into 2 bytes
            response[j] = (unsigned char)(holdingReg[i] >> 8);
            j++;
            response[j] = (unsigned char)(holdingReg[i]);
            j++;
        }
        else
        {
            response[j] = 0x00;
            j++;
            response[j] = (unsigned char)(holdingReg[i]);
            j++;
        }
    }
    crc = make_crc(j);
    response[j] = (unsigned char)(crc >> 8);
    j++;
    response[j] = (unsigned char)(crc);
    j++;

    //send_data(j);
    //j=0;

    //clear_response();
	return j;
}

uint8_t write_reg(void)
{
    /******************************************************************************/
    /* Works out which reg to write and then responds                             */
    /******************************************************************************/
    unsigned char wr_AddressLow = 0;
    unsigned char wr_AddressHigh = 0;
    unsigned int wr_Address = 0;

    unsigned int wr_valToWrite = 0;
    unsigned char wr_valToWriteLow = 0;
    unsigned char wr_valToWriteHigh = 0;

    unsigned int crc = 0;
    unsigned int i = 0;

    //Combine address bytes
    wr_Address = received[2];
    wr_Address <<= 8;
    wr_Address |= received[3];

    wr_AddressLow = received[3]; //useful to store
    wr_AddressHigh = received[2];

    //Combine value to write regs
    wr_valToWrite = received[4];
    wr_valToWrite <<= 8;
    wr_valToWrite |= received[5];

    wr_valToWriteLow = received[5];
    wr_valToWriteHigh = received[4];

    holdingReg[wr_Address] = wr_valToWrite;

    response[0] = device_addr;
    response[1] = 0x06;
    response[3] = wr_AddressLow; //2 bytes per reg
    response[2] = wr_AddressHigh;

    //TO DO CHECK VALUE IS ACTUALLY WRITTEN//
    response[4] = wr_valToWriteHigh;
    response[5] = wr_valToWriteLow;

    crc = make_crc(6);

    response[6] = (unsigned char)(crc >> 8);
    response[7] = (unsigned char)(crc);

    //send_data(8);
    //clear_response();
	return 8;
}

uint8_t read_coil(void)
{
    /******************************************************************************/
    /* Reads a coil and then responds                                             */
    /******************************************************************************/
    unsigned int rc_Address = 0;
    unsigned int rc_numCoils = 0;
    unsigned int crc = 0;

    unsigned char howManyBytes = 0;
    unsigned char remainder = 0;
    unsigned char lsb = 0;
    unsigned char i,j,k,l = 0;

    //Combine address bytes
    rc_Address = received[2];
    rc_Address <<=8;
    rc_Address |= received[3];

    //Combine number of coils bytes
    rc_numCoils = received[4];
    rc_numCoils <<= 8;
    rc_numCoils |= received[5];

    response[0] = device_addr;
    response[1] = 0x01;

    howManyBytes = (unsigned char)(rc_numCoils/8);
    remainder = (unsigned char)(rc_numCoils % 8);

    if(remainder)
    {
        howManyBytes += 1;
    }
    response[2] = howManyBytes;

    l = (unsigned char)(rc_Address);
    k = 3; //start at response 3

    for(i=howManyBytes; i!=0; i--)
    {
        if(i>1)
        {
            for(j=0;j!=8;j++)
            {
                if(coils[l])
                {
                    lsb = 1;
                }
                else
                {
                    lsb = 0;
                }
                response[k] ^= (lsb << j);
                l++;
            }
            k++;
        }
        else
        {
            for(j=0;j!=remainder;j++)
            {
                if(coils[l])
                {
                    lsb = 1;
                }
                else
                {
                    lsb = 0;
                }
                response[k] ^= (lsb << j);
                l++;
            }
            k++;
        }
    }
    crc = make_crc(k);

    response[k] = (unsigned char)(crc >> 8);
    response[k+1] = (unsigned char)(crc);

//    send_data(k+2);
//    clear_response();
	return k+2;
}

uint8_t read_input_coil(void)
{
    /******************************************************************************/
    /* Reads a coil and then responds                                             */
    /******************************************************************************/
    unsigned int rc_Address = 0;
    unsigned int rc_numCoils = 0;
    unsigned int crc = 0;

    unsigned char howManyBytes = 0;
    unsigned char remainder = 0;
    unsigned char lsb = 0;
    unsigned char i,j,k,l = 0;

    //Combine address bytes
    rc_Address = received[2];
    rc_Address <<=8;
    rc_Address |= received[3];

    //Combine number of coils bytes
    rc_numCoils = received[4];
    rc_numCoils <<= 8;
    rc_numCoils |= received[5];

    response[0] = device_addr;
    response[1] = 0x02;

    howManyBytes = (unsigned char)(rc_numCoils/8);
    remainder = (unsigned char)(rc_numCoils % 8);

    if(remainder)
    {
        howManyBytes += 1;
    }
    response[2] = howManyBytes;

    l = (unsigned char)(rc_Address);
    k = 3; //start at response 3

    for(i=howManyBytes; i!=0; i--)
    {
        if(i>1)
        {
            for(j=0;j!=8;j++)
            {
                if(coils[l])
                {
                    lsb = 1;
                }
                else
                {
                    lsb = 0;
                }
                response[k] ^= (lsb << j);
                l++;
            }
            k++;
        }
        else
        {
            for(j=0;j!=remainder;j++)
            {
                if(coils[l])
                {
                    lsb = 1;
                }
                else
                {
                    lsb = 0;
                }
                response[k] ^= (lsb << j);
                l++;
            }
            k++;
        }
    }
    crc = make_crc(k);

    response[k] = (unsigned char)(crc >> 8);
    response[k+1] = (unsigned char)(crc);

    //send_data(k+2);
    //clear_response();
	return k+2;
}

uint8_t write_coil(void)
{
    /******************************************************************************/
    /* Writes to a coil and then responds                                         */
    /******************************************************************************/
    unsigned char wc_AddressLow = 0;
    unsigned char wc_AddressHigh = 0;
    unsigned int wc_Address = 0;

    unsigned int wc_valToWrite = 0;
    unsigned char wc_valToWriteLow = 0;
    unsigned char wc_valToWriteHigh = 0;
    int i = 0;
    unsigned int crc = 0;

    //Combine address bytes
    wc_Address = received[2];
    wc_Address <<= 8;
    wc_Address |= received[3];

    wc_AddressLow = received[3]; //useful to store
    wc_AddressHigh = received[2];

    //Combine value to write regs
    wc_valToWrite = received[4];
    wc_valToWrite <<= 8;
    wc_valToWrite |= received[5];

    wc_valToWriteLow = received[5];
    wc_valToWriteHigh = received[4];

    if(wc_valToWrite)
    {
        coils[wc_Address] = 0xFF;
    }
    else
    {
        coils[wc_Address] = 0x00;
    }

    response[0] = device_addr;
    response[1] = 0x02;
    response[3] = wc_AddressLow; //2 bytes per reg
    response[2] = wc_AddressHigh;

    //TO DO CHECK VALUE IS ACTUALLY WRITTEN//
    response[4] = wc_valToWriteHigh;
    response[5] = wc_valToWriteLow;

    crc = make_crc(6);

    response[6] = (unsigned char)(crc >> 8);
    response[7] = (unsigned char)(crc);

    //send_data(9);
    //clear_response();
	
	return 9;
}

unsigned int make_crc(unsigned char Length)
{
    unsigned int crc = 0xFFFF;
    unsigned int crcHigh = 0;
    unsigned int crcLow = 0;
    int i,j = 0;

    for(i=0;i<Length;i++)
    {
        crc ^= response[i];
        for(j=8; j!=0; j--)
        {
            if((crc & 0x0001) != 0)
            {
                crc >>= 1;
                crc ^= 0xA001;
            }
            else
            {
                crc >>= 1;
            }
        }
    }
    //bytes are wrong way round so doing a swap here..
    crcHigh = (crc & 0x00FF) <<8;
    crcLow = (crc & 0xFF00) >>8;
    crcHigh |= crcLow;
    crc = crcHigh;

    return crc;
}

unsigned char check_crc(void)
{
    unsigned int crc = 0xFFFF;
    unsigned int crcHigh = 0;
    unsigned int crcLow = 0;
    int i,j = 0;

    for(i=0;i<messageLength-2;i++)
    {
        crc ^= received[i];
        for(j=8; j!=0; j--)
        {
            if((crc & 0x0001) != 0)
            {
                crc >>= 1;
                crc ^= 0xA001;
            }
            else
            {
                crc >>= 1;
            }
        }
    }
    //bytes are wrong way round so doing a swap here..
    crcHigh = (crc & 0x00FF);
    crcLow = (crc & 0xFF00) >>8;
    if((crcHigh == received[i])&&(crcLow == received[i+1]))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}