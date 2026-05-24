/*
 * i2c_master_polled.h
 *
 * Created: 2018-11-26 오전 11:24:37
 *  Author: shuug
 */ 


#ifndef I2C_MASTER_POLLED_H_
#define I2C_MASTER_POLLED_H_

#define i2c_W_DATA_LENGTH (2+64)
#define i2c_R_DATA_LENGTH (64)

#define I2C_ADC_CH0		0x4a
#define I2C_ADC_CH1		0x4f

#define I2C_FRAM		0x50
#define I2C_POT			0x28
#define I2C_POT_WP		0x00
//#define I2C_POT_INIT	0x00

#define I2C_TIMEOUT 1000


extern void configure_i2c_master(void);
extern void i2c_adc_init(uint8_t addr, uint8_t init_data);
extern uint16_t i2c_adc_read(uint8_t ch, uint8_t reg_point);
extern void i2c_adc_write(uint8_t ch, uint8_t reg_point, uint16_t data);
extern void i2c_adc_write_byte(uint8_t ch, uint8_t reg_point, uint8_t data);
extern void i2c_adc_read_block(uint8_t device_addr, uint8_t wr_no, uint8_t rd_no);
extern void i2c_adc_write_block(uint8_t ch_addr, uint8_t wr_no);


#endif /* I2C_MASTER_POLLED_H_ */