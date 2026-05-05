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

/* (TX builders는 task 7, RX 파서는 task 8 에서 추가) */
