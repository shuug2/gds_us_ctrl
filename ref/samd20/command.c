/////////////////////////////////////////////////
// Base for a command line handler
// To be used with a terminal program of some kind
//
// Echoes all characters to the serial port
// Handles backspace for editing
// TAB will run the last command
// UP ARROW will load the last command into the command buffer and echo it to the serial port
// Can handle up to 5 parametere in HEX or DEC
#include <asf.h>
#include <ctype.h>
#include <string.h>
#include "define.h"

void Cmd_WR (void);
void Cmd_SET(void);
void Cmd_HELP(void);
void Cmd_WATCH(void);
void Cmd_ON(void);
void Cmd_OFF(void);
void Cmd_GAIN(void);
void Cmd_VOUT (void);
void Cmd_ISET (void);
void Cmd_MODIFY(void);

void set_current(uint32_t ch0_current, uint32_t ch1_current);
uint8_t adc_gain2real(uint8_t gain_val);
uint8_t adc_real2gain(uint8_t gain_val);

uint16_t		GetParm (uint8_t parm, uint8_t base);
uint32_t		GetParm_32 (uint8_t parm, uint8_t base);
void	SplitCommand(void);
void	ProcessCommand (void);
uint16_t htoi (const uint8_t *ptr);
void	command_handling_loop(uint8_t c);
void my_itoa(int16_t num, uint8_t *pStr);
void my_itoa32(int32_t num, uint8_t *pStr);

extern struct usart_module usart_DEBUG_instance;
extern uint8_t	display_adc_f;
extern uint8_t debug_txbuffer[MAX_DEBUG_LENGTH];
extern uint8_t my_str[16];
extern uint8_t adc_gain[2];
extern int16_t pwm_vout[2];

typedef struct command_ss {
	uint8_t	cmd[10];
	void	(*func)	(void);
}command_s;

#define MAX_PARMS 5
uint8_t *parms[MAX_PARMS];
command_s commands[] = {
//	{"rd",		Cmd_RD},
//	{"rdw",		Cmd_RDW},
	{"rw",		Cmd_WR},
//	{"l",		Cmd_L},
//	{"p",		Cmd_P},
//	{"regs",	Cmd_REGS},
//	{"io",		Cmd_IO},
//	{"f",		Cmd_F},
	{"iset",	Cmd_ISET},
	{"gain",	Cmd_GAIN},
	{"wat",		Cmd_WATCH},
//	{"rst",		Cmd_RST},
	{"vout",	Cmd_VOUT},
	{"set",		Cmd_SET},
	{"on",		Cmd_ON},
	{"off",		Cmd_OFF},
	{"mod",		Cmd_MODIFY},
//	{"pc",		Cmd_PC},
//	{"info",	Cmd_INFO},
	{"/",		Cmd_HELP}
};
const uint16_t N_COMMANDS = sizeof (commands)/ sizeof (command_s);

#define NULLCHAR	0x00
#define BACKSPACE	0x08
#define TAB			0x09
#define LF			0x0a
#define CR			0x0d
#define SPACE		0x20

const uint8_t BS_S[4] = {BACKSPACE,SPACE,BACKSPACE,NULLCHAR};
#define PROMPT	"QM> "
#define	HEX		16
#define	DEC		10

static 	uint16_t	n_parms; // number of parameters entered after the command
//static	uint16_t	last_address = 0;
static	uint16_t	cmd_buffer_index = 0;
#define CMD_BUFFER_LEN	30
static	uint8_t		cmd_buffer[CMD_BUFFER_LEN];
static	uint8_t		last_cmd[CMD_BUFFER_LEN];

////////////////////////////////////////////////////
// example command, reads the first parm (address) and the second parm (value)
// and calls a function to do something with the values
void	Cmd_WR (void) {
	/////////////////////////////////////////////////////////
	// write a value to target RAM
//	uint16_t addr = GetParm(0, HEX);
//	uint8_t val = GetParm(1, HEX);
//	do_something_with (addr, val);
	usart_write_buffer_job(&usart_DEBUG_instance,	(uint8_t *)"Cmd_WR\r\n", 8);
}
void	Cmd_GAIN (void)
{
	/////////////////////////////////////////////////////////
	// write a value to target RAM
	uint8_t val = GetParm(0, DEC);
	debug_txbuffer[0] = 0;

	if( (val==1) || (val==2) || (val==4) || (val==8) || (val==16)) {
		adc_gain[CH0] = adc_real2gain(val);
		adc_gain[CH1] = adc_real2gain(val);
		my_itoa(val,my_str);		strcat((char *)debug_txbuffer,(char *)my_str);	strcat((char *)debug_txbuffer,(char *)" GAIN\r\n\0");
		} else {
		my_itoa(val,my_str);		strcat((char *)debug_txbuffer,(char *)my_str);	strcat((char *)debug_txbuffer,(char *)" is invalid value\r\n\0");
	}
	//	uint8_t val = GetParm(1, HEX);
	//	do_something_with (addr, val);
	//usart_write_buffer_job(&usart_DEBUG_instance,	(uint8_t *)"Cmd_GAIN\r\n", 8);
	usart_write_buffer_job(&usart_DEBUG_instance, debug_txbuffer, strlen((char *)debug_txbuffer));
}
void	Cmd_ISET (void)
{
	/////////////////////////////////////////////////////////
	// write a value to target RAM
	uint32_t val_ch0;
	uint32_t val_ch1;

//	while(usart_get_job_status(&usart_DEBUG_instance,USART_TRANSCEIVER_TX) == STATUS_BUSY) {}
//	debug_txbuffer[0] = 0;
//	strcat((char *)debug_txbuffer,(char *)" S");
//	usart_write_buffer_job(&usart_DEBUG_instance, debug_txbuffer, strlen((char *)debug_txbuffer));

	val_ch0 = GetParm_32(0, DEC);
	val_ch1 = GetParm_32(1, DEC);


	set_current(val_ch0, val_ch1);
	
	
/*	debug_txbuffer[0] = 0;
	strcat((char *)debug_txbuffer,(char *)" CH0 : ");
	my_itoa32(val_ch0,my_str);		strcat((char *)debug_txbuffer,(char *)my_str);
	strcat((char *)debug_txbuffer,(char *)", CH1 : ");
	my_itoa32(val_ch1,my_str);		strcat((char *)debug_txbuffer,(char *)my_str);
	strcat((char *)debug_txbuffer,(char *)" \r\n\0");
	//	uint8_t val = GetParm(1, HEX);
	//	do_something_with (addr, val);
	//usart_write_buffer_job(&usart_DEBUG_instance,	(uint8_t *)"Cmd_GAIN\r\n", 8);
	usart_write_buffer_job(&usart_DEBUG_instance, debug_txbuffer, strlen((char *)debug_txbuffer));
*/
}

void	Cmd_VOUT (void) 
{
	/////////////////////////////////////////////////////////
	// write a value to target RAM
	uint8_t val = GetParm(0, HEX);
	debug_txbuffer[0] = 0;

	pwm_vout[0] = val;
	pwm_vout[1] = val;
	my_itoa(val,my_str);		strcat((char *)debug_txbuffer,(char *)my_str);	strcat((char *)debug_txbuffer,(char *)" GAIN\r\n\0");
//	uint8_t val = GetParm(1, HEX);
	//	do_something_with (addr, val);
	//usart_write_buffer_job(&usart_DEBUG_instance,	(uint8_t *)"Cmd_GAIN\r\n", 8);
	usart_write_buffer_job(&usart_DEBUG_instance, debug_txbuffer, strlen((char *)debug_txbuffer));
}

void Cmd_SET(void) {
	usart_write_buffer_job(&usart_DEBUG_instance,	(uint8_t *)"Cmd_SET\r\n", 9);
}
void Cmd_HELP(void) {
	usart_write_buffer_job(&usart_DEBUG_instance,	(uint8_t *)"Cmd_HELP\r\n", 10);
}
uint16_t htoi (const uint8_t *ptr) {
	uint16_t value = 0;
	uint8_t ch = *ptr;
	
    while (ch == ' ' || ch == '\t')
        ch = *(++ptr);

    for (;;) {

        if (ch >= '0' && ch <= '9')
            value = (value << 4) + (ch - '0');
        else if (ch >= 'A' && ch <= 'F')
            value = (value << 4) + (ch - 'A' + 10);
        else if (ch >= 'a' && ch <= 'f')
            value = (value << 4) + (ch - 'a' + 10);
        else
            return value;
        ch = *(++ptr);
    }
}
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
void my_itoa(int16_t num, uint8_t *pStr)
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

uint16_t		GetParm (uint8_t parm, uint8_t base) {
	switch (base) {
		case HEX:
		return ( htoi(parms[parm]));

		case DEC:
		return ( atoi((char *)parms[parm]));
	}
	return 0;
}
uint32_t		GetParm_32 (uint8_t parm, uint8_t base) {
		
	return ( atol((char *)parms[parm]));
}

void	SplitCommand(void) {
	/////////////////////////////////////////////////////
	// split the command line by replacing all ' ' with 
	// '\0' and saving pointers to all the parm strings
	uint16_t i, p;

	/////////////////////////////////////////////////////
	// clear the pointer array
	n_parms = 0;
	for (i = 0; i < MAX_PARMS; i++)
		parms[i] = NULL;

	/////////////////////////////////////////////////////
	// scan the command line, dork any spaces with null chars
	// and save the location of the first char after the null
	for (i = 0, p = 0; cmd_buffer[i] != NULLCHAR; i++) {
		if (cmd_buffer[i] == SPACE) {
			cmd_buffer[i] = NULLCHAR;
			parms[p++] = &cmd_buffer[i] + 1;
			n_parms++;
		}
	}
}
void	ProcessCommand (void) {
	uint16_t cmd;
//LED0_HIGH;
	
	//Serial.println("");
//	usart_write_buffer_job(&usart_DEBUG_instance,	(uint8_t *)CRLF, 2);
	
	/////////////////////////////////////////////////////
	// trap just a CRLF
//	if (cmd_buffer[0] == NULLCHAR) {
//		usart_write_buffer_job(&usart_DEBUG_instance,	(uint8_t *)PROMPT, 4);
//		return;
//	}
	
	/////////////////////////////////////////////////////
	// save this command for later use with TAB or UP arrow
	memcpy(last_cmd, cmd_buffer, sizeof(last_cmd));
	
	/////////////////////////////////////////////////////
	// Chop the command line into substrings by
	// replacing ' ' with '\0' 
	// Also adds pointers to the substrings 
	SplitCommand(); 
	
	/////////////////////////////////////////////////////
	// Scan the command table looking for a match
	for (cmd = 0; cmd < N_COMMANDS; cmd++) {
		if (strcmp ((char *)commands[cmd].cmd, (char *)cmd_buffer) == 0) {
			commands[cmd].func();  // command found, run its function
			goto done;
		}
	}

	/////////////////////////////////////////////////////
	// if we get here no valid command was found
	//Serial << "wtf?" << endl;
	usart_write_buffer_job(&usart_DEBUG_instance,	(uint8_t *)"????\r\n", 6);
//LED0_HIGH;
	done:
	cmd_buffer_index = 0;
	cmd_buffer[0] = NULLCHAR;
	//Serial << "QM> ";
//	while(usart_get_job_status(&usart_DEBUG_instance,USART_TRANSCEIVER_TX) == STATUS_BUSY) {}
//	usart_write_buffer_job(&usart_DEBUG_instance,	(uint8_t *)PROMPT, 4);
//LED0_LOW;

}

void	command_handling_loop(uint8_t c) {
	
//	if (Serial.peek() > 0) {
//		c = Serial.read();
/*		if (ESC == c) {
			while (Serial.available() < 2) {};
			c = Serial.read();
			c = Serial.read();	
			switch (c) {
				case 'A':  // up arrow
					// copy the last command into the command buffer
					// then echo it to the terminal and set the 
					// the buffer's index pointer to the end 
					memcpy(cmd_buffer, last_cmd, sizeof(last_cmd));
					cmd_buffer_index = strlen (cmd_buffer);
					Serial << cmd_buffer;
					break;
			}
		} else {
*/
//LED0_HIGH;
			c = tolower(c);
			switch (c) {
				
				case TAB:  // TAB runs the last command
					memcpy(cmd_buffer, last_cmd, sizeof(cmd_buffer));
					ProcessCommand ();
					break;

				case BACKSPACE:
					if (cmd_buffer_index > 0) {
						cmd_buffer[--cmd_buffer_index] = NULLCHAR;
						//Serial << _BYTE(BACKSPACE) << SPACE << _BYTE(BACKSPACE);
						usart_write_buffer_job(&usart_DEBUG_instance,	(uint8_t *)BS_S, 3);
					}
					break;

				case LF:
					ProcessCommand ();
					//Serial.flush();		// remove any following CR
					break;
				
				case CR:
//					ProcessCommand ();
					//Serial.flush();		// remove any following LF
					break;

				default:  // just put the char in the buffer
					cmd_buffer[cmd_buffer_index] = c;
					cmd_buffer_index ++;
					if(cmd_buffer_index > CMD_BUFFER_LEN-1) {
						cmd_buffer_index = 0;
					}
					cmd_buffer[cmd_buffer_index] = NULLCHAR;
//					usart_write_job(&usart_DEBUG_instance,	&c);
			}
		usart_write_job(&usart_DEBUG_instance,	&c);
			
//		}
//	}
//LED0_LOW;

}