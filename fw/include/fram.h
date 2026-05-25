/* fw/include/fram.h
 *
 * FM24C16B FRAM (I2C1 slave 0x50, 1-byte word address, page 0).
 * Byte map verbatim from samd20 ref/samd20/define.h:44-73.
 *
 * Hard invariants (FM24C16 page-0 model):
 *   - FRAM_I2C_ADDR_7B is constant 0x50, never OR'd with page bits.
 *   - every access satisfies (mem_addr + n) <= 256 (a crossing wraps to offset 0 of the page).
 *   u16/u32 are big-endian (MSB first) to match samd20 on-wire layout.
 */
#pragma once
#include <stdint.h>

#define FRAM_I2C_ADDR_7B       0x50u
#define FRAM_INIT_FLAG_MAGIC   0xAAu

#define FRAM_ADDR_INIT_FLAG    0    /* 1B */
#define FRAM_ADDR_WORK_CNT     1    /* 4B */
#define FRAM_ADDR_DELAY1       5    /* 2B */
#define FRAM_ADDR_DELAY2       7    /* 2B */
#define FRAM_ADDR_DELAY3       9    /* 2B */
#define FRAM_ADDR_TRIGGER2     11   /* 2B */
#define FRAM_ADDR_TRIGGER3     13   /* 2B */
#define FRAM_ADDR_SAFTY        15   /* 1B */
#define FRAM_ADDR_RUN_MODE     16   /* 1B */
#define FRAM_ADDR_MODEL_FREQ   17   /* 1B */
#define FRAM_ADDR_MODEL_TYPE   18   /* 1B */
#define FRAM_ADDR_OUT_POWER    19   /* 1B */
#define FRAM_ADDR_ON_TIME      20   /* 2B */
#define FRAM_ADDR_ENERGY       22   /* 4B */
#define FRAM_ADDR_MULTI_T1     26   /* 2B */
#define FRAM_ADDR_MULTI_T2     28   /* 2B */
#define FRAM_ADDR_MULTI_O1     30   /* 2B */
#define FRAM_ADDR_MULTI_O2     32   /* 2B */
#define FRAM_ADDR_EN_ENERGY    34   /* 1B */
#define FRAM_ADDR_EN_MULTI     35   /* 1B */
#define FRAM_ADDR_TIMEOVER     36   /* 1B */
#define FRAM_ADDR_COMM_ADDR    37   /* 1B */
#define FRAM_ADDR_COMM_SPEED   38   /* 1B */
#define FRAM_ADDR_COMM_PARITY  39   /* 1B */
#define FRAM_ADDR_CAL_VAL      40   /* 2B */
#define FRAM_ADDR_FREQ_CAL_VAL 42   /* 2B */
#define FRAM_ADDR_COMM_MODE    44   /* 1B */
#define FRAM_ADDR_ETHER_IP     45   /* 4B */
#define FRAM_ADDR_ETHER_NM     49   /* 4B */
#define FRAM_ADDR_ETHER_GW     53   /* 4B */

uint8_t  fram_read_byte(uint8_t addr);
uint16_t fram_read_u16 (uint8_t addr);   /* big-endian */
uint32_t fram_read_u32 (uint8_t addr);   /* big-endian */
void     fram_write_byte(uint8_t addr, uint8_t  v);
void     fram_write_u16 (uint8_t addr, uint16_t v);   /* big-endian */
void     fram_write_u32 (uint8_t addr, uint32_t v);   /* big-endian */
