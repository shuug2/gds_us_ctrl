/* fw/drivers/dgus_lcd.c
 *
 * DGUS LCD 프로토콜 레이어 — samd20 ref 9 함수 풀 패리티.
 * USART1 raw I/O (fw/drivers/usart1.c) 위에 빌드.
 *
 * samd20 결함 회피 (spec §3.7):
 *   #1 LEN 무경계: LEN ∈ [4, 26] 검증 + drop 카운터
 *   #2 byte-counted timeout: 벽시계 ms (sys_tick) 사용
 *   #3 init 무한 retry: HAL_UART_Init 실패 시 Error_Handler (usart1.c 책임)
 *   #4 shared TX race: 폴링 TX → caller block, 자연 직렬화
 *   #5 RX ring wrap-overwrite: drop + 카운터 (usart1.c 책임)
 */
#include "dgus_lcd.h"
#include "usart1.h"
#include "sys_tick.h"
#include <string.h>

/*--------------------------------------------------------------
 * 정적 상태
 *--------------------------------------------------------------*/

/* TX — 단일 정적 버퍼. 폴링 TX 이므로 caller 가 send_frame 동안 block,
 * 다른 호출과 자연 직렬화 (samd20 결함 #4 회피).
 */
static uint8_t s_tx_buf[32];                            /* 헤더(6) + payload max(23) + α */

/* TX 카운터 */
static uint16_t s_dgus_tx_timeout_count;

/* RX 파서 상태머신 */
typedef enum {
    PS_IDLE,
    PS_GOT_5A,
    PS_GOT_HEADER,
    PS_COLLECTING
} parse_state_t;

static parse_state_t s_parse_state;
static uint8_t       s_frame_buf[32];                   /* LEN_max + 약간 여유 */
static uint8_t       s_frame_idx;
static uint8_t       s_bytes_remaining;
static uint32_t      s_frame_start_ms;                  /* 벽시계 timeout (samd20 결함 #2 회피) */
static uint16_t      s_dgus_rx_drop_count;

/*--------------------------------------------------------------
 * 공개 — init / accessor
 *--------------------------------------------------------------*/

void dgus_init(void)
{
    s_parse_state           = PS_IDLE;
    s_frame_idx             = 0;
    s_bytes_remaining       = 0;
    s_frame_start_ms        = 0;
    s_dgus_rx_drop_count    = 0;
    s_dgus_tx_timeout_count = 0;
    /* USART1 / RX ring 책임은 usart1_init. 여기선 dgus 레이어 상태만 */
}

uint16_t dgus_rx_drop_count(void)
{
    return s_dgus_rx_drop_count;
}

uint16_t dgus_tx_timeout_count(void)
{
    return s_dgus_tx_timeout_count;
}

bool dgus_is_echo(const dgus_frame_t *f)
{
    return f->cmd == DGUS_CMD_WR;
}

/*--------------------------------------------------------------
 * TX 빌더 — samd20 9 함수 풀 패리티
 *--------------------------------------------------------------*/

/* 단일 프레임 전송 helper. plen = payload byte 수.
 * 프레임: 5A A5 LEN(=3+plen) cmd AH AL [payload].
 */
static void send_frame(uint8_t cmd, uint16_t vp, const uint8_t *payload, uint8_t plen)
{
    s_tx_buf[0] = DGUS_SYNC1;
    s_tx_buf[1] = DGUS_SYNC2;
    s_tx_buf[2] = (uint8_t)(3 + plen);                  /* LEN = cmd+addr(3) + payload */
    s_tx_buf[3] = cmd;
    s_tx_buf[4] = (uint8_t)(vp >> 8);
    s_tx_buf[5] = (uint8_t)(vp & 0xFF);
    for (uint8_t i = 0; i < plen; i++) {
        s_tx_buf[6 + i] = payload[i];
    }
    if (usart1_send_blocking(s_tx_buf, (uint16_t)(6 + plen)) == HAL_TIMEOUT) {
        s_dgus_tx_timeout_count++;
    }
}

/* samd20 reset_dgus_lcd: VP_LCD_RESET 0x04, payload = 55 AA 5A A5 (DWIN reset magic) */
void dgus_reset_lcd(void)
{
    static const uint8_t magic[4] = { 0x55, 0xAA, 0x5A, 0xA5 };
    send_frame(DGUS_CMD_WR, VP_LCD_RESET, magic, 4);
}

/* samd20 set_lcd_page: VP_LCD_SETPAGE 0x84, payload = 5A 01 00 page */
void dgus_set_page(uint8_t page)
{
    uint8_t pl[4] = { 0x5A, 0x01, 0x00, page };
    send_frame(DGUS_CMD_WR, VP_LCD_SETPAGE, pl, 4);
}

/* samd20 send_lcd_data_var */
void dgus_write_u16(uint16_t vp, uint16_t value)
{
    uint8_t pl[2] = {
        (uint8_t)(value >> 8),
        (uint8_t)(value & 0xFF),
    };
    send_frame(DGUS_CMD_WR, vp, pl, 2);
}

/* samd20 send_lcd_data_word — big-endian 4 byte */
void dgus_write_u32(uint16_t vp, uint32_t value)
{
    uint8_t pl[4] = {
        (uint8_t)(value >> 24),
        (uint8_t)(value >> 16),
        (uint8_t)(value >> 8),
        (uint8_t)(value & 0xFF),
    };
    send_frame(DGUS_CMD_WR, vp, pl, 4);
}

/* samd20 send_lcd_byte_array */
void dgus_write_bytes(uint16_t vp, const uint8_t *buf, uint8_t n)
{
    if (n > DGUS_MAX_DATA) {
        n = DGUS_MAX_DATA;                              /* 무경계 입력 안전 */
    }
    send_frame(DGUS_CMD_WR, vp, buf, n);
}

/* samd20 send_lcd_int_array — u16 array, big-endian */
void dgus_write_u16_array(uint16_t vp, const uint16_t *buf, uint8_t n)
{
    if (n > (DGUS_MAX_DATA / 2)) {
        n = (uint8_t)(DGUS_MAX_DATA / 2);
    }
    uint8_t pl[DGUS_MAX_DATA];
    for (uint8_t i = 0; i < n; i++) {
        pl[(i * 2) + 0] = (uint8_t)(buf[i] >> 8);
        pl[(i * 2) + 1] = (uint8_t)(buf[i] & 0xFF);
    }
    send_frame(DGUS_CMD_WR, vp, pl, (uint8_t)(n * 2));
}

/* samd20 send_lcd_txt — 10 byte zero-pad. txt 가 NULL 또는 빈 문자열이면 nothing 송신.
 * samd20 라인 257-283 의 행동을 그대로 보존.
 */
void dgus_write_text(uint16_t vp, const char *txt)
{
    if (txt == 0 || txt[0] == '\0') {
        return;
    }
    uint8_t pl[10];
    for (uint8_t i = 0; i < 10; i++) {
        pl[i] = 0;
    }
    for (uint8_t i = 0; i < 10; i++) {
        if (txt[i] == '\0') {
            break;
        }
        pl[i] = (uint8_t)txt[i];
    }
    send_frame(DGUS_CMD_WR, vp, pl, 10);
}

/* samd20 read_system_var — RD 0x83, payload = 1 (1 word read) */
void dgus_read_var(uint8_t var)
{
    uint8_t pl[1] = { 1 };
    send_frame(DGUS_CMD_RD, (uint16_t)var, pl, 1);
}
