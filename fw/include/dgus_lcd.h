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

/* Full LCD behavior VPs (samd20 dgus_lcd.h verbatim — Stage LCD full port) */
/* setup / save / model */
#define STD_SETUP_PARAM     0x1020
#define DATA_SAVE           0x1050
#define M_DATA_SAVE         0x1250
#define MODEL_FREQ          0x1060
#define MODEL_TYPE          0x1070
#define KEY_MULTI           0x1080
#define SETUP_MODEL         0x1084
#define ENERGY_EN           0x1085
#define SETUP_PARAM_MOOHAN  0x1094
#define SETUP_PARAM         0x1100
/* comm config (values + on-screen text) */
#define COMM_ADDR           0x1071
#define COMM_SPEED          0x1072
#define COMM_PARITY         0x1073
#define COMM_ADDR_TXT       0x1460
#define COMM_SPEED_TXT      0x1464
#define COMM_PARITY_TXT     0x146a
#define COMM_IP_TXT         0x1470
#define COMM_NM_TXT         0x1480
#define COMM_GW_TXT         0x1490
/* status icons (extend ICON_RESET/SEEK/RUN block) */
#define ICON_OL             0x1153
#define ICON_OUTERR         0x1154
#define ICON_ERROR_RESET    0x1407
#define KEY_ERROR_RESET     0x1408
/* live display bars / values */
#define LV_OUTPUT2          0x117a
#define LV_TIME             0x1184
#define LV_TIME2            0x118e
/* setup-page edit fields */
#define LV_OUT_POWER        0x1200
#define LV_MAX_ON_TIME      0x1201
#define LV_ENERGY_VAL       0x1212
#define DISP_MULTI          0x1208
#define DISP_RUN_MODE       0x120c
#define DISP_SAFTY          0x120d
#define DISP_REMOTE         0x120e
#define LV_LIMIT_OUT_T      0x120f
#define LV_RUN_MODE         0x1210
#define LV_MO_OUT1          0x1400
#define LV_MO_OUT2          0x1401
#define LV_MO_TIME1         0x1402
#define LV_MO_TIME2         0x1403
/* std-page formatted text + version + error msg */
#define DISP_STD_DATA1      0x1300
#define DISP_STD_DATA2      0x1310
#define DISP_STD_DATA3      0x1320
#define DISP_VERSION        0x1330
#define VP_ERROR_MSG        0x1350
/* ethernet / comm-mode */
#define MULTI_EN            0x140a
#define LV_COMM_MODE        0x140b
#define DISP_COMM_MODE      0x140c
#define DISP_EN_DHCP        0x140d
#define LV_ETHER_KEY        0x140f
/* calibration */
#define VAR_CAL_VAL         0x1410
#define VAR_FREQ_CAL_VAL    0x1412

/* Key/touch return codes (samd20 dgus_lcd.h verbatim) */
#define KEY_UP      1
#define KEY_DN      0
#define KEY_CANCEL  0
#define KEY_SAVE    1

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

/* Boot-time blocking RD helpers — DGUS cold-boot handshake (panel boots
 * slower than the MCU, so a one-shot set_page was lost and the panel stayed
 * on its power-on logo). NOT for the main loop. */
bool dgus_read_word(uint16_t addr, uint16_t *out_val, uint32_t timeout_ms);
bool dgus_wait_ready(uint32_t timeout_ms);            /* poll SYS_PIC_NOW until panel UART answers */

/* 관측성 (chunk 6f GDB read 용) */
uint16_t dgus_rx_drop_count(void);
uint16_t dgus_tx_timeout_count(void);

/*--------------------------------------------------------------
 * Stage A 데모 설정 (사용자가 보드에 맞춰 수정 가능)
 *--------------------------------------------------------------*/

#define DGUS_DEMO_BOOT_PAGE      LCD_RUN_STD    /* 9 — VAR_POWER 시각화되는 페이지 */
#define DGUS_DEMO_UPTIME_VP      VAR_POWER      /* 0x1110 — U16 (wrap @ 65535초) */
#define DGUS_DEMO_RESET_ON_BOOT  0              /* 0=skip / 1=부팅 시 LCD 풀-재시작 */

/*--------------------------------------------------------------
 * LCD 콜드부팅 핸드셰이크 튜닝 (set_page 레이스 픽스)
 *--------------------------------------------------------------*/
#define DGUS_BOOT_READY_TIMEOUT_MS   4000u   /* 패널 UART 기동 최대 대기 */
#define DGUS_READ_REPLY_TIMEOUT_MS    120u   /* RD 1회 응답 대기창 */
#define DGUS_LOGO_DWELL_MS           1000u   /* 패널 기동 후 로고 splash 노출 시간 (samd20 동일) */
#define DGUS_PAGE_CONFIRM_RETRIES       8u   /* run 페이지 read-back 재전송 횟수 */
#define DGUS_PAGE_CONFIRM_SPACING_MS  150u   /* 재전송 간격 */
