/*
 * i2c_master_polled.c
 *
 * Created: 2018-11-26 오전 11:24:07
 *  Author: shuug
 */ 

#include <asf.h>
#include <stdlib.h>
#include "i2c_comm.h"

uint16_t i2c_timeout_cnt;

uint8_t i2c_wr_buffer[i2c_W_DATA_LENGTH];
uint8_t i2c_rd_buffer[i2c_R_DATA_LENGTH];

struct i2c_master_module i2c_master_instance;
struct i2c_master_packet i2c_wr_packet;
struct i2c_master_packet i2c_rd_packet;


void configure_i2c_master(void)
{
	struct i2c_master_config config_i2c_master;
	i2c_master_get_config_defaults(&config_i2c_master);

	config_i2c_master.generator_source = GCLK_GENERATOR_3;
	config_i2c_master.baud_rate      = I2C_MASTER_BAUD_RATE_400KHZ;
	config_i2c_master.buffer_timeout = 1000;
	config_i2c_master.pinmux_pad0    = I2C_SDA_PINMUX;
	config_i2c_master.pinmux_pad1    = I2C_SCL_PINMUX;
	while(i2c_master_init(&i2c_master_instance, I2C_MASTER_MODULE, &config_i2c_master)  != STATUS_OK);

	i2c_master_enable(&i2c_master_instance);
}

uint16_t i2c_adc_read(uint8_t ch_addr, uint8_t reg_point)
{
	int temp;
	i2c_wr_packet.address     = ch_addr;
	i2c_wr_packet.data_length = 1;
	i2c_wr_packet.data        = i2c_wr_buffer;
	i2c_wr_buffer[0] = reg_point;

	i2c_rd_packet.address     = ch_addr;
	i2c_rd_packet.data_length = 2;
	i2c_rd_packet.data        = i2c_rd_buffer;

	while(i2c_master_write_packet_wait(&i2c_master_instance, &i2c_wr_packet) != STATUS_OK) { if(i2c_timeout_cnt++ == I2C_TIMEOUT) { break; } }
	while(i2c_master_read_packet_wait(&i2c_master_instance, &i2c_rd_packet) != STATUS_OK) {	if(i2c_timeout_cnt++ == I2C_TIMEOUT) { break; } }
	MSB(temp) = i2c_rd_buffer[0];
	LSB(temp) = i2c_rd_buffer[1];
	return (uint16_t)temp;
//	return(i2c_rd_buffer[0]*256 + i2c_rd_buffer[1]);
}

void i2c_adc_read_block(uint8_t device_addr, uint8_t wr_no, uint8_t rd_no)
{
	int temp;
	i2c_wr_packet.address     = device_addr;
	i2c_wr_packet.data_length = wr_no;
	i2c_wr_packet.data        = i2c_wr_buffer;

	i2c_rd_packet.address     = device_addr;
	i2c_rd_packet.data_length = rd_no;
	i2c_rd_packet.data        = i2c_rd_buffer;

	while(i2c_master_write_packet_wait(&i2c_master_instance, &i2c_wr_packet) != STATUS_OK) { if(i2c_timeout_cnt++ == I2C_TIMEOUT) { break; } }
	while(i2c_master_read_packet_wait(&i2c_master_instance, &i2c_rd_packet) != STATUS_OK) {	if(i2c_timeout_cnt++ == I2C_TIMEOUT) { break; } }
}

void i2c_adc_write(uint8_t ch_addr, uint8_t reg_point, uint16_t data)
{
	i2c_wr_packet.address     = ch_addr;
	i2c_wr_packet.data_length = 3;
	i2c_wr_packet.data        = i2c_wr_buffer;
	i2c_wr_buffer[0] = reg_point;
	i2c_wr_buffer[1] = data / 256;
	i2c_wr_buffer[2] = data % 256;
	
	while(i2c_master_write_packet_wait(&i2c_master_instance, &i2c_wr_packet) != STATUS_OK) { if(i2c_timeout_cnt++ == I2C_TIMEOUT) { break; } }

}

void i2c_adc_write_byte(uint8_t ch_addr, uint8_t reg_point, uint8_t data)
{
	i2c_wr_packet.address     = ch_addr;
	i2c_wr_packet.data_length = 2;
	i2c_wr_packet.data        = i2c_wr_buffer;
	i2c_wr_buffer[0] = reg_point;
	i2c_wr_buffer[1] = data;
	
	while(i2c_master_write_packet_wait(&i2c_master_instance, &i2c_wr_packet) != STATUS_OK) { if(i2c_timeout_cnt++ == I2C_TIMEOUT) { break; } }

}

void i2c_adc_write_block(uint8_t ch_addr, uint8_t wr_no)
{
	i2c_wr_packet.address     = ch_addr;
	i2c_wr_packet.data_length = (wr_no + 2);
	i2c_wr_packet.data        = i2c_wr_buffer;
	while(i2c_master_write_packet_wait(&i2c_master_instance, &i2c_wr_packet) != STATUS_OK) { if(i2c_timeout_cnt++ == I2C_TIMEOUT) { break; } }
}

