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
