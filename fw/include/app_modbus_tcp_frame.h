/* fw/include/app_modbus_tcp_frame.h — pure standard-Modbus-TCP framing
 * (spec §3). MBAP strip on input, MBAP wrap on output, trailing CRC dropped,
 * request unit id echoed. Wraps the untouched mb_core (MB_MODE_TCP). No socket
 * or HAL — host-tested. samd20's raw-response quirk is intentionally dropped
 * (spec §1.1/§3.3). */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "app_modbus_core.h"

#define MB_TCP_MBAP_LEN  6u                 /* txn(2)+proto(2)+length(2) before unit */
#define MB_TCP_RESP_MAX  (MB_RESP_MAX + 4u) /* 6 MBAP + (core resp - 2 CRC) */

/* Build a standard Modbus-TCP response from ONE received TCP frame.
 * req/req_len = raw recv() bytes (MBAP + PDU). On a frame that must be
 * answered, fills out[0..*out_len-1] and returns true. Returns false (send
 * nothing) when: < 8 bytes, protocol id != 0, MBAP length field < 2, the
 * declared length exceeds what was received or the core frame bound, or the
 * core stays silent (malformed / unsupported FC — samd20-faithful).
 * *fc_out = function code processed (0 if none) so the caller runs the
 * write-apply pass when *fc_out == 0x06. */
bool mb_tcp_build_response(mb_core_t *mb, const uint8_t *req, uint16_t req_len,
                           uint8_t out[MB_TCP_RESP_MAX], uint16_t *out_len,
                           uint8_t *fc_out);

/* M6 프레임 워커(spec §2.1): 누적 수신 버퍼 선두에서 완전 MBAP 프레임 1개를
 * 판정. *frame_len = 6(MBAP) + length필드(unit+PDU) wire 길이. DESYNC(헤더
 * 불량)면 호출측이 누적 버퍼를 통째로 폐기 — 재동기는 마스터 재시도에 위임.
 * length 수용 경계는 mb_tcp_build_response(:20)와 동일(MB_FRAME_MAX). */
typedef enum {
    MB_TCP_FR_NEED_MORE = 0,   /* 더 수신 필요 (frame_len 미기록) */
    MB_TCP_FR_OK,              /* *frame_len 바이트의 완전 프레임 */
    MB_TCP_FR_DESYNC           /* proto!=0 / length<2 / length>MB_FRAME_MAX */
} mb_tcp_fr_t;

mb_tcp_fr_t mb_tcp_frame_peek(const uint8_t *buf, uint16_t len,
                              uint16_t *frame_len);
