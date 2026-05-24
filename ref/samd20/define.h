#ifndef DEFINE_H
#define DEFINE_H

#define CRLF	"\r\n"
#define MAX_CMD_LENGTH		256

#define TICK_STATE_MS 1			// state machine change every 2mS
#define JOB_STATE_COMMAND	0
#define JOB_STATE_MENU		1
#define JOB_STATE_SENSE		2
#define JOB_STATE_CALC		3
#define JOB_STATE_IDLE		4

#define RDM_RESPONSE_MSG "GD SONIC"
//#define VERSION_MSG		"V2.1.0_211021"
//#define VERSION_MSG		"V2.4.0_220501   "	//max 16
//#define VERSION_MSG		"V2.4.2_220516   "	//max 16 , add modbus function
//#define VERSION_MSG		"V2.4.4_220527   "	//max 16 , change modbus reg. address
//#define VERSION_MSG		"V2.4.5_220805   "	//max 16 , add current calib function
//#define VERSION_MSG		"V2.4.8_221008   "	//max 16 , add current calib function
//#define VERSION_MSG		"V2.5.0_221031   "	//max 16 , model string 8 -> 10
//#define VERSION_MSG		"V2.5.2_221111   "	//max 16 , model string 8 -> 10 / 
//#define VERSION_MSG		"V2.5.4_230419   "	//max 16 , Fix Lcd stuck error
//#define VERSION_MSG		"V2.6.0_23707   "	//max 16 , Fix Lcd stuck error
//#define VERSION_MSG		"V2.6.2_230712   "	//max 16 , Fix Lcd stuck error
//#define VERSION_MSG		"V2.6.4_230823   "	//max 16 , WELD-ON signal 
//#define VERSION_MSG		"V2.6.6_231203   "	//max 16 , fix Handgun-mode timeout error
//#define VERSION_MSG		"V2.6.8_240425   "	//max 16 , add Moohan Eng
//#define VERSION_MSG		"V2.7.0_241126   "	//max 16 , fix Moohan Eng lcd
//#define VERSION_MSG		"V2.9.0_250509   "	//max 16 , add ethernet comm
#define VERSION_MSG		"V2.9.1_250629   "	//max 16 , fix ethernet comm, fix Error message clear

#define	MAX_RUN_DISP_TIME	30		// 0.1X
#define KEY_HOLD_TH			200		// 0.01x 
#define MODE_KEY_HOLD_TH	200		// 0.01x
#define REPEATE_KEY_TH		2		// 0.01 x
#define TICK_ERROR_SEC		30		// 0.1x

#define CYL_TIMEOUT			10000	// 100.00 sec
#define RESET_TIMEOUT		50		// x10 mS
#define SEEK_TIMEOUT		50		// x10 mS

// fram addr
#define ADDR_INIT_FLAG	0		// 1 byte
#define ADDR_WORK_CNT	1		// 4 bytes
#define ADDR_DELAY1		5		// 2 bytes
#define	ADDR_DELAY2		7		// 2 bytes
#define	ADDR_DELAY3		9		// 2 bytes
#define	ADDR_TRIGGER2	11		// 2 bytes
#define	ADDR_TRIGGER3	13		// 2 bytes
#define ADDR_SAFTY		15		// 1 byte
#define ADDR_RUN_MODE	16		// 1 byte
#define ADDR_MODEL_FREQ	17		// 1 byte
#define ADDR_MODEL_TYPE	18		// 1 byte
#define ADDR_OUT_POWER	19		// 1 byte
#define ADDR_ON_TIME	20		// 2 byte
#define ADDR_ENERGY		22		// 4 byte
#define ADDR_MULTI_T1	26		// 2 byte
#define ADDR_MULTI_T2	28		// 2 byte
#define ADDR_MULTI_O1	30		// 2 byte
#define ADDR_MULTI_O2	32		// 2 byte
#define ADDR_EN_ENERGY	34		// 1 byte
#define ADDR_EN_MULTI	35		// 1 byte
#define ADDR_TIMEOVER	36		// 1 byte
#define ADDR_COMM_ADDR	37		// 1 byte
#define ADDR_COMM_SPEED	38		// 1 byte
#define ADDR_COMM_PARITY	39		// 1 byte
#define	ADDR_CAL_VAL	40		// 2 bytes
#define	ADDR_FREQ_CAL_VAL	42		// 2 bytes
#define ADDR_COMM_MODE	44		// 1		// 0: serial , 1: ETHERNET
#define ADDR_ETHER_IP	45		// 4 byte
#define ADDR_ETHER_NM	49		// 4 byte
#define ADDR_ETHER_GW	53		// 4 byte


#define ERR_NOERROR		0
#define ERR_OVLD		1
#define ERR_OVTIME		2
#define ERR_OUTERR		4

#define BZ_PER			92	
#define BZ_ON			46	 
#define BZ_OFF			BZ_PER+1	 

#define COMM_SERIAL		0
#define COMM_ETH_STATIC		1
#define COMM_ETH_DHCP		2
#define ETH_DHCP		0
#define ETH_STATIC		1

#define ETHER_INPUT_NONE	0xff
#define ETHER_INPUT_IP	0
#define ETHER_INPUT_NM	1
#define ETHER_INPUT_GW	2

#endif // DEFINE_H


