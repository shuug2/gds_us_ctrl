/* fw/src/app_modbus_core.c — pure Modbus slave core (samd20 modbus.c port).
 * Stateless over an mb_core_t context (de-globalized: samd20 used file-scope
 * volatile arrays). HAL-free by design — host tests in fw/test/.
 * Port fixes vs samd20 (plan §Deviations): request bounds checks (samd20 did
 * OOB reads and an UNBOUNDED holdingReg write), FC05 echo (samd20 answered
 * fc=0x02/9 bytes — copy-paste bug), FC01/02 full-byte reads (samd20 left the
 * last byte empty when count%8==0). Everything else byte-faithful. */
#include <string.h>
#include "app_modbus_core.h"

void mb_core_init(mb_core_t *mb, uint8_t device_addr)
{
    memset(mb, 0, sizeof(*mb));
    mb->device_addr = device_addr;
}

uint16_t mb_crc16(const uint8_t *buf, uint8_t len)
{
    uint16_t crc = 0xFFFFu;
    for (uint8_t i = 0; i < len; i++) {
        crc = (uint16_t)(crc ^ buf[i]);
        for (uint8_t j = 8; j != 0; j--) {
            if ((crc & 0x0001u) != 0u) {
                crc = (uint16_t)((crc >> 1u) ^ 0xA001u);
            } else {
                crc = (uint16_t)(crc >> 1u);
            }
        }
    }
    /* samd20 make_crc swap: wire order is lo-byte first, so the first wire
     * byte rides in the HIGH byte of the return. */
    return (uint16_t)(((crc & 0x00FFu) << 8) | ((crc & 0xFF00u) >> 8));
}

/* FC 03/04 — samd20 read_reg/read_input_reg folded into one (the two bodies
 * are identical except the echoed FC; samd20's >255 split produces the same
 * hi/lo bytes unconditionally — byte-identical simplification). */
static uint8_t mb_read_regs(const mb_core_t *mb, const uint8_t *frame,
                            uint8_t fc, uint8_t resp[MB_RESP_MAX])
{
    /* contract: caller (mb_core_decode) guarantees frame[0..5] are valid */
    uint16_t addr = (uint16_t)(((uint16_t)frame[2] << 8) | frame[3]);
    uint16_t num  = (uint16_t)(((uint16_t)frame[4] << 8) | frame[5]);
    uint8_t  j    = 3;

    /* Port safety fix (NOT samd20): out-of-range request = silence. samd20
     * walked holdingReg past [50] and overflowed response past [125]. */
    if ((num == 0u) || (num > MB_REG_COUNT) ||
        ((uint32_t)addr + num > MB_REG_COUNT)) {
        return 0;
    }

    resp[0] = mb->device_addr;
    resp[1] = fc;
    resp[2] = (uint8_t)((uint16_t)(num * 2u));
    for (uint16_t i = addr; i < (uint16_t)(addr + num); i++) {
        resp[j++] = (uint8_t)(mb->holding[i] >> 8);
        resp[j++] = (uint8_t)(mb->holding[i]);
    }
    uint16_t crc = mb_crc16(resp, j);
    resp[j++] = (uint8_t)(crc >> 8);
    resp[j++] = (uint8_t)(crc);
    return j;
}

/* FC 06 — samd20 write_reg: store raw + echo the request. The app layer
 * interprets the value (clamp/persist/command) in its apply pass. */
static uint8_t mb_write_reg(mb_core_t *mb, const uint8_t *frame,
                            uint8_t resp[MB_RESP_MAX])
{
    /* contract: caller (mb_core_decode) guarantees frame[0..5] are valid */
    uint16_t addr = (uint16_t)(((uint16_t)frame[2] << 8) | frame[3]);
    uint16_t val  = (uint16_t)(((uint16_t)frame[4] << 8) | frame[5]);

    /* Port safety fix (NOT samd20): samd20 wrote holdingReg[addr] unbounded —
     * an out-of-range register write was an arbitrary memory write. */
    if (addr >= MB_REG_COUNT) {
        return 0;
    }
    mb->holding[addr] = val;

    resp[0] = mb->device_addr;
    resp[1] = 0x06u;
    resp[2] = frame[2];
    resp[3] = frame[3];
    resp[4] = frame[4];
    resp[5] = frame[5];
    uint16_t crc = mb_crc16(resp, 6u);
    resp[6] = (uint8_t)(crc >> 8);
    resp[7] = (uint8_t)(crc);
    return 8u;
}

/* FC 01/02 — samd20 read_coil/read_input_coil folded (identical bodies, own FC
 * echo). Port fix: samd20 wrote NO bits into the last byte when count%8==0
 * (its remainder loop ran zero times) — full bytes now always fill. */
static uint8_t mb_read_coils(const mb_core_t *mb, const uint8_t *frame,
                             uint8_t fc, uint8_t resp[MB_RESP_MAX])
{
    /* contract: caller (mb_core_decode) guarantees frame[0..5] are valid */
    uint16_t addr = (uint16_t)(((uint16_t)frame[2] << 8) | frame[3]);
    uint16_t num  = (uint16_t)(((uint16_t)frame[4] << 8) | frame[5]);

    /* Port safety fix: out-of-range = silence (samd20 read past coils[50]). */
    if ((num == 0u) || ((uint32_t)addr + num > MB_COIL_COUNT)) {
        return 0;
    }

    uint8_t bytes = (uint8_t)(num / 8u);
    uint8_t rem   = (uint8_t)(num % 8u);
    if (rem != 0u) {
        bytes++;
    }

    resp[0] = mb->device_addr;
    resp[1] = fc;
    resp[2] = bytes;

    uint8_t k = 3;
    uint8_t l = (uint8_t)addr;
    for (uint8_t i = bytes; i != 0u; i--) {
        uint8_t nbits = ((i == 1u) && (rem != 0u)) ? rem : 8u;
        for (uint8_t j = 0; j != nbits; j++) {
            resp[k] ^= (uint8_t)((mb->coils[l] ? 1u : 0u) << j);
            l++;
        }
        k++;
    }
    uint16_t crc = mb_crc16(resp, k);
    resp[k]      = (uint8_t)(crc >> 8);
    resp[k + 1u] = (uint8_t)(crc);
    return (uint8_t)(k + 2u);
}

/* FC 05 — samd20 write_coil semantics (any nonzero -> 0xFF). Port fix: echo
 * fc 0x05 / 8 bytes (samd20 answered 0x02 / 9 bytes — copy-paste bug). */
static uint8_t mb_write_coil(mb_core_t *mb, const uint8_t *frame,
                             uint8_t resp[MB_RESP_MAX])
{
    /* contract: caller (mb_core_decode) guarantees frame[0..5] are valid */
    uint16_t addr = (uint16_t)(((uint16_t)frame[2] << 8) | frame[3]);
    uint16_t val  = (uint16_t)(((uint16_t)frame[4] << 8) | frame[5]);

    if (addr >= MB_COIL_COUNT) {
        return 0;
    }
    mb->coils[addr] = (val != 0u) ? 0xFFu : 0x00u;

    resp[0] = mb->device_addr;
    resp[1] = 0x05u;
    resp[2] = frame[2];
    resp[3] = frame[3];
    resp[4] = frame[4];
    resp[5] = frame[5];
    uint16_t crc = mb_crc16(resp, 6u);
    resp[6] = (uint8_t)(crc >> 8);
    resp[7] = (uint8_t)(crc);
    return 8u;
}

uint8_t mb_core_decode(mb_core_t *mb, const uint8_t *frame, uint8_t len,
                       uint8_t mode, uint8_t resp[MB_RESP_MAX], uint8_t *fc_out)
{
    uint8_t n = 0;
    *fc_out = 0;

    /* samd20 decode_comm: RTU filters slave address + CRC; TCP skips both. */
    if (mode == MB_MODE_RTU) {
        if (len < 8u) {                 /* full RTU request floor: addr fc 4B crc2 */
            return 0;
        }
        if (frame[0] != mb->device_addr) {
            return 0;
        }
        uint16_t crc = mb_crc16(frame, (uint8_t)(len - 2u));
        if ((frame[len - 2u] != (uint8_t)(crc >> 8)) ||
            (frame[len - 1u] != (uint8_t)(crc))) {
            return 0;
        }
    } else {
        if (len < 6u) {                 /* PDU floor: addr fc 4B (MBAP stripped) */
            return 0;
        }
    }

    /* read_coil builds bits with ^= over a zeroed buffer (samd20
     * clear_response invariant) — zero up front. */
    memset(resp, 0, MB_RESP_MAX);

    switch (frame[1]) {
    case 0x01u:
    case 0x02u:
        n = mb_read_coils(mb, frame, frame[1], resp);
        break;
    case 0x03u:
    case 0x04u:
        n = mb_read_regs(mb, frame, frame[1], resp);
        break;
    case 0x05u:
        n = mb_write_coil(mb, frame, resp);
        break;
    case 0x06u:
        n = mb_write_reg(mb, frame, resp);
        break;
    default:
        return 0;   /* unsupported FC: silence (samd20 builds no exception) */
    }
    if (n != 0u) {
        *fc_out = frame[1];
    }
    return n;
}
