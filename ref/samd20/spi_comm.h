/*
 * spi_comm.h
 *
 * Created: 2017-12-09 오후 5:13:31
 *  Author: shuug
 */ 


#ifndef SPI_COMM_H_
#define SPI_COMM_H_

extern void configure_DAC_spi_master_callbacks(void);
extern void configure_DAC_spi_master(void);
extern void send_dac_data(void);

#endif /* SPI_AT45DB018_H_ */