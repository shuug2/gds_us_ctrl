/*
 * spi_comm.c
 *
 * Created: 2017-12-09 오후 5:12:57
 *  Author: shuug
 */ 
#include <asf.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <spi.h>
#include "spi_comm.h"

#define BUF_LENGTH 48

extern uint16_t i2c_dimm_buf[16];

static uint8_t dac_wr_buffer[BUF_LENGTH], dac_rd_buffer[BUF_LENGTH];

struct spi_module dac_spi_master_instance;

volatile bool transrev_complete_dac_spi_master = true;

void configure_DAC_spi_master(void)
{
	struct spi_config config_spi_master;
//	struct spi_slave_inst_config slave_dev_config;
	/* Configure and initialize software device instance of peripheral slave */
//	spi_slave_inst_get_config_defaults(&slave_dev_config);
//	slave_dev_config.ss_pin = DAC_SLAVE_SELECT_PIN;
//	spi_attach_slave(&slave, &slave_dev_config);
	/* Configure, initialize and enable SERCOM SPI module */
	spi_get_config_defaults(&config_spi_master);
//	config_spi_master.receiver_enable  = false;
	config_spi_master.mode_specific.master.baudrate = 1500000;
	config_spi_master.transfer_mode    = SPI_TRANSFER_MODE_0;
	config_spi_master.mux_setting = DAC_SPI_SERCOM_MUX_SETTING;
	/* Configure pad 0 for data in */
	config_spi_master.pinmux_pad0 = DAC_SPI_SERCOM_PINMUX_PAD0;
	/* Configure pad 1 as unused */
	config_spi_master.pinmux_pad1 = DAC_SPI_SERCOM_PINMUX_PAD1;
	/* Configure pad 2 for data out */
	config_spi_master.pinmux_pad2 = PINMUX_UNUSED;
	/* Configure pad 3 for SCK */
	config_spi_master.pinmux_pad3 = PINMUX_UNUSED;
	spi_init(&dac_spi_master_instance, DAC_SPI_MODULE, &config_spi_master);
	spi_enable(&dac_spi_master_instance);
}

static void callback_dac_spi_master( struct spi_module *const module)
{
	port_pin_set_output_level(DAC_SLAVE_SELECT_PIN, HIGH);
	transrev_complete_dac_spi_master = true;
}

void configure_DAC_spi_master_callbacks(void)
{
	spi_register_callback(&dac_spi_master_instance, callback_dac_spi_master, SPI_CALLBACK_BUFFER_TRANSCEIVED);
	spi_enable_callback(&dac_spi_master_instance, SPI_CALLBACK_BUFFER_TRANSCEIVED);
}

void send_dac_data(void)
{
	uint32_t crc_val;
	 
	dac_wr_buffer[0] = MSB(i2c_dimm_buf[0]);
	dac_wr_buffer[1] = LSB(i2c_dimm_buf[0]);
	dac_wr_buffer[2] = MSB(i2c_dimm_buf[1]);
	dac_wr_buffer[3] = LSB(i2c_dimm_buf[1]);
	dac_wr_buffer[4] = MSB(i2c_dimm_buf[2]);
	dac_wr_buffer[5] = LSB(i2c_dimm_buf[2]);
	dac_wr_buffer[6] = MSB(i2c_dimm_buf[3]);
	dac_wr_buffer[7] = LSB(i2c_dimm_buf[3]);
	dac_wr_buffer[8] = MSB(i2c_dimm_buf[4]);
	dac_wr_buffer[9] = LSB(i2c_dimm_buf[4]);
	dac_wr_buffer[10] = MSB(i2c_dimm_buf[5]);
	dac_wr_buffer[11] = LSB(i2c_dimm_buf[5]);
	dac_wr_buffer[12] = MSB(i2c_dimm_buf[6]);
	dac_wr_buffer[13] = LSB(i2c_dimm_buf[6]);
	dac_wr_buffer[14] = MSB(i2c_dimm_buf[7]);
	dac_wr_buffer[15] = LSB(i2c_dimm_buf[7]);
	dac_wr_buffer[16] = MSB(i2c_dimm_buf[8]);
	dac_wr_buffer[17] = LSB(i2c_dimm_buf[8]);
	dac_wr_buffer[18] = MSB(i2c_dimm_buf[9]);
	dac_wr_buffer[19] = LSB(i2c_dimm_buf[9]);
	dac_wr_buffer[20] = MSB(i2c_dimm_buf[10]);
	dac_wr_buffer[21] = LSB(i2c_dimm_buf[10]);
	dac_wr_buffer[22] = MSB(i2c_dimm_buf[11]);
	dac_wr_buffer[23] = LSB(i2c_dimm_buf[11]);
	dac_wr_buffer[24] = MSB(i2c_dimm_buf[12]);
	dac_wr_buffer[25] = LSB(i2c_dimm_buf[12]);
	dac_wr_buffer[26] = MSB(i2c_dimm_buf[13]);
	dac_wr_buffer[27] = LSB(i2c_dimm_buf[13]);
	dac_wr_buffer[28] = MSB(i2c_dimm_buf[14]);
	dac_wr_buffer[29] = LSB(i2c_dimm_buf[14]);
	dac_wr_buffer[30] = MSB(i2c_dimm_buf[15]);
	dac_wr_buffer[31] = LSB(i2c_dimm_buf[15]);
	
	crc_val = 0;
	crc32_calculate(dac_wr_buffer, 8, &crc_val);
	
	dac_wr_buffer[32] = MSB0W(crc_val);
	dac_wr_buffer[33] = MSB1W(crc_val);
	dac_wr_buffer[34] = MSB2W(crc_val);
	dac_wr_buffer[35] = MSB3W(crc_val);

//	while (transrev_complete_dac_spi_master == false) {
		
//	}
	transrev_complete_dac_spi_master = false;
	port_pin_set_output_level(DAC_SLAVE_SELECT_PIN, LOW);
	port_pin_set_output_level(DAC_SLAVE_SELECT_PIN, LOW);
	port_pin_set_output_level(DAC_SLAVE_SELECT_PIN, LOW);
	port_pin_set_output_level(DAC_SLAVE_SELECT_PIN, LOW);
	port_pin_set_output_level(DAC_SLAVE_SELECT_PIN, LOW);
//	spi_transceive_buffer_job(&dac_spi_master_instance, dac_wr_buffer, dac_rd_buffer, 36);
	spi_write_buffer_wait(&dac_spi_master_instance, (uint8_t *)dac_wr_buffer, 36);
	port_pin_set_output_level(DAC_SLAVE_SELECT_PIN, HIGH);

}
