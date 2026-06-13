/* fw/src/app_modbus_tcp_frame.c — see app_modbus_tcp_frame.h. */
#include "app_modbus_tcp_frame.h"

bool mb_tcp_build_response(mb_core_t *mb, const uint8_t *req, uint16_t req_len,
                           uint8_t out[MB_TCP_RESP_MAX], uint16_t *out_len,
                           uint8_t *fc_out)
{
    *fc_out  = 0u;
    *out_len = 0u;

    if (req_len < 8u) { return false; }                 /* MBAP(6)+unit+fc */

    uint16_t proto  = (uint16_t)(((uint16_t)req[2] << 8) | req[3]);
    uint16_t length = (uint16_t)(((uint16_t)req[4] << 8) | req[5]);  /* unit+PDU */
    if (proto != 0u)  { return false; }
    if (length < 2u)  { return false; }                 /* need unit + fc */
    if ((uint32_t)req_len < (uint32_t)MB_TCP_MBAP_LEN + (uint32_t)length) {
        return false;                                   /* truncated */
    }
    if (length > MB_FRAME_MAX) { return false; }        /* core frame bound */

    /* PDU-for-core = [unit, fc, data...] at req[6], len = MBAP length field.
     * MB_MODE_TCP skips the unit/addr filter + CRC check. */
    uint8_t resp[MB_RESP_MAX];
    uint8_t n = mb_core_decode(mb, &req[6], (uint8_t)length, MB_MODE_TCP,
                               resp, fc_out);
    if (n == 0u) { return false; }                      /* core silent */

    /* core resp = [unit, fc, data, CRC_hi, CRC_lo]; drop the trailing 2 CRC.
     * Guard the cross-unit coupling: a non-zero core return is >= 6 bytes;
     * this defends the n-2 subtraction if the core contract ever changes. */
    if (n < 4u) { return false; }
    uint16_t pdu_len = (uint16_t)(n - 2u);              /* unit + PDU bytes */

    out[0] = req[0];                       /* txn id hi (echo) */
    out[1] = req[1];                       /* txn id lo (echo) */
    out[2] = 0u;                           /* proto id hi */
    out[3] = 0u;                           /* proto id lo */
    out[4] = (uint8_t)(pdu_len >> 8);      /* length hi (big-endian) */
    out[5] = (uint8_t)(pdu_len & 0xFFu);   /* length lo */
    for (uint16_t i = 0u; i < pdu_len; i++) {
        out[6u + i] = resp[i];             /* [unit, fc, data] — no CRC */
    }
    out[6] = req[6];                       /* unit id = echo request unit (standard) */
    *out_len = (uint16_t)(6u + pdu_len);
    return true;
}
