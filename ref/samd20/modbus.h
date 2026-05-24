
#define H_REG_WORK_CNTH	0x00
#define H_REG_WORK_CNTL	0x01		
#define H_REG_DISP_POWER	0x02
#define H_REG_DISP_AMP		0x03
#define H_REG_DISP_FREQ		0x04
#define H_REG_DISP_ENERGY	0x05
#define H_REG_OUT_POWER	0x06
#define H_REG_ON_TIME	0x07
#define H_REG_ENERGY	0x08
#define H_REG_TIMEOVER	0x09
#define H_REG_DELAY1	0x0a
#define	H_REG_DELAY2	0x0b
#define	H_REG_DELAY3	0x0c
#define	H_REG_TRIGGER2	0x0d
#define	H_REG_TRIGGER3	0x0e
#define H_REG_MULTI_T1	0x0f
#define H_REG_MULTI_T2	0x10
#define H_REG_MULTI_O1	0x11
#define H_REG_MULTI_O2	0x12
#define H_REG_RUN_MODE	0x13
#define H_REG_EN_ENERGY	0x14
#define H_REG_EN_MULTI	0x15
#define H_REG_EN_SAFTY		0x16		
#define H_REG_MODEL_FREQ	0x17
#define H_REG_MODEL_TYPE	0x18
#define H_REG_RESET		0x19
#define H_REG_SEEK		0x1a
#define H_REG_START		0x1b
#define H_REG_STOP		0x1c
#define H_REG_STATUS	0x1d

#define STATUS_US	0x01
#define STATUS_ESTOP	0x02
#define STATUS_OVLD		0x04
#define STATUS_OVTIME		0x08
#define STATUS_OUTERR		0x10

/******************************************************************************/
/* User Level #define Macros                                                  */
/******************************************************************************/
volatile unsigned int  holdingReg[50];
volatile unsigned char coils[50];
volatile unsigned char response[125]; //Enough to return all holding-r's
volatile unsigned char received[125]; //Enough to write all holding-r's
volatile char modbusMessage,messageLength;

volatile unsigned char tx_bodbus_buf[125], tx_cnt, tx_len, device_addr;

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
uint8_t decode_comm(uint8_t mode);
unsigned int make_crc(unsigned char);

void init_modbus(uint8_t speed, uint8_t parity, uint8_t addr);
void close_modbus(void);

