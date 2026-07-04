/* fw/include/io.h — 커넥터/패널 물리 IO (OSC 발진단 제외).
 * 입력 read = raw 물리 레벨(0/1); 폴라리티 해석은 호출측. 출력 write = 논리
 * on/off(폴라리티는 io.c가 적용). spec 2026-06-20-physical-io-layer-design §2. */
#pragma once
#include <stdbool.h>
#include <stdint.h>

void io_init(void);                 /* 전 커넥터 핀 config (idle = inactive) */

/* 입력 (raw 물리 레벨 0/1; HAL_GPIO_ReadPin 그대로) */
uint8_t io_read_start(void);        /* PA15  active-LOW */
uint8_t io_read_reset(void);        /* PC10  active-LOW */
uint8_t io_read_estop_seek(void);   /* PC11  EMSW active-HIGH / SEEK active-LOW (model_type 분기는 호출측) */
uint8_t io_read_key1(void);         /* PC12  active-LOW */
uint8_t io_read_key2(void);         /* PB11  active-LOW */
uint8_t io_read_sens_up(void);      /* PA12  active-LOW */
uint8_t io_read_sens_dn(void);      /* PA11  active-LOW */
uint8_t io_read_overload(void);     /* PB13  active-HIGH */
uint8_t io_read_usfb(void);         /* PB12  active-HIGH (초음파 출력 피드백: 출력 중 H) */

/* 출력 (논리 on/off; 폴라리티 io.c 내부 적용) */
void io_usout(bool on);             /* PB4  active-HIGH */
void io_sol_dn(bool on);            /* PB5  active-LOW  */
void io_ovld_relay(bool on);        /* PB3  active-HIGH */
void io_buzzer(bool on);            /* PA2  active-HIGH */
