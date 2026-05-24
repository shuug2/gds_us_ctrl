/* fw/include/dgus_lcd.h
 *
 * DGUS LCD 프로토콜 레이어 — samd20 ref 포팅 (9 함수 풀 패리티).
 *
 * 프레임 포맷: 5A A5 LEN CMD ADDR_H ADDR_L [payload...]
 *   CMD: 0x82 = WR (echo 포함), 0x83 = RD (응답 / 터치 키)
 *   LEN = CMD(1) + ADDR(2) + payload byte 수. 유효 범위 [4, 26].
 *   Wire = big-endian (samd20 동일).
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

/*--------------------------------------------------------------
 * 프로토콜 상수 (samd20 ref/samd20/dgus_lcd.h verbatim)
 *--------------------------------------------------------------*/

/* Sync header */
#define DGUS_SYNC1   0x5A
#define DGUS_SYNC2   0xA5

/* Commands */
#define DGUS_CMD_WR  0x82
#define DGUS_CMD_RD  0x83

/* LCD page IDs (samd20 dgus_lcd.h 포팅) */
#define LCD_LOGO          0
#define LCD_MODEL_SETUP   1
#define LCD_RUN_MULTI     3
#define LCD_SETUP_MULTI   5
#define LCD_RUN_HAND      3
#define LCD_SETUP_HAND    7
#define LCD_RUN_STD       9
#define LCD_SETUP_STD1   10
#define LCD_SETUP_STD2D  12
#define LCD_SETUP_STD2T  13
#define LCD_SETUP_STD3   15
#define LCD_WARNING      17
#define LCD_SETUP_MH2    19
#define LCD_SETUP_MHC    21
#define LCD_SETUP_STDC   23
#define LCD_SETUP_MHE    25
#define LCD_SETUP_STDE   27

/* System VPs */
#define VP_LCD_RESET     0x0004
#define VP_LCD_SETPAGE   0x0084
#define SYS_PIC_NOW      0x0014

/* Application VPs (samd20 verbatim — Stage A 에선 VAR_POWER 만 데모에 사용) */
#define MODEL_NAME       0x1000
#define LV_WORK_CNT      0x1006
#define VAR_POWER        0x1110
#define VAR_AMP          0x1111
#define VAR_FREQ         0x1112
#define VAR_ENERGY       0x1113
#define LV_OUTPUT        0x1170

/* Stage B application VPs (samd20 dgus_lcd.h verbatim) */
#define LV_ENERGY_EDIT   0x1202
#define LV_DM_DELAY      0x1203
#define LV_DM_WELD       0x1204
#define LV_DM_HOLD       0x1205
#define LV_TM_WELD       0x1206
#define LV_TM_HOLD       0x1207
#define DISP_HORNDOWN    0x1209
#define DISP_ENERGY_EN   0x120a
#define DISP_MULTI_EN    0x120b
#define ICON_RESET       0x1150
#define ICON_SEEK        0x1151
#define ICON_RUN         0x1152

/* (samd20 의 다른 VP 매크로들은 후속 슬라이스에서 추가. Stage A 데모 범위 외) */

/*--------------------------------------------------------------
 * 프레임 / API
 *--------------------------------------------------------------*/

#define DGUS_MAX_DATA  23   /* payload 최대 = LEN_max(26) - cmd(1) - addr(2) */

typedef struct {
    uint8_t  cmd;                       /* 0x82 (WR/echo) or 0x83 (RD) */
    uint16_t vp_addr;
    uint8_t  data_len;                  /* payload 만의 길이 (= LEN - 3) */
    uint8_t  data[DGUS_MAX_DATA];
} dgus_frame_t;

/* 파서/카운터 상태 클리어. usart1_init 후 호출. */
void dgus_init(void);

/* TX builders — samd20 9 함수 풀 패리티 */
void dgus_reset_lcd(void);
void dgus_set_page(uint8_t page);
void dgus_write_u16(uint16_t vp, uint16_t value);
void dgus_write_u32(uint16_t vp, uint32_t value);
void dgus_write_bytes(uint16_t vp, const uint8_t *buf, uint8_t n);
void dgus_write_u16_array(uint16_t vp, const uint16_t *buf, uint8_t n);
void dgus_write_text(uint16_t vp, const char *txt);   /* 10 byte zero-pad */
void dgus_read_var(uint8_t var);                      /* 0x83 RD len=1 word */

/* RX */
bool dgus_rx_poll(dgus_frame_t *out);
bool dgus_is_echo(const dgus_frame_t *f);             /* cmd == 0x82 */

/* 관측성 (chunk 6f GDB read 용) */
uint16_t dgus_rx_drop_count(void);
uint16_t dgus_tx_timeout_count(void);

/*--------------------------------------------------------------
 * Stage A 데모 설정 (사용자가 보드에 맞춰 수정 가능)
 *--------------------------------------------------------------*/

#define DGUS_DEMO_BOOT_PAGE      LCD_RUN_STD    /* 9 — VAR_POWER 시각화되는 페이지 */
#define DGUS_DEMO_UPTIME_VP      VAR_POWER      /* 0x1110 — U16 (wrap @ 65535초) */
#define DGUS_DEMO_RESET_ON_BOOT  0              /* 0=skip / 1=부팅 시 LCD 풀-재시작 */
