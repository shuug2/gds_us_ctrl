/* fw/drivers/i2c_pot.c — U4 I2C digital potentiometer (amplitude) actuation.
 *
 * Thin device layer over the raw I2C1 bus (drivers/i2c1.c, shared with FRAM).
 * samd20 wrote amplitude as i2c_adc_write_byte(I2C_POT, I2C_POT_WP, value);
 * this replicates that exact byte sequence: [S][0x28<<1|W][0x00][dac].
 * Pure %→DAC conversion lives in the HAL-free header (host-tested). See
 * spec docs/superpowers/specs/2026-06-28-i2c-pot-amplitude-actuation-design.md.
 */
#include "i2c_pot.h"
#include "i2c1.h"

void i2c_pot_set_dac(uint8_t dac)
{
    if (dac > 127u) { dac = 127u; }   /* defensive upper clamp (set_amp carries raw DAC) */
    /* best-effort: ignore return; failures counted in i2c1_err_count() (no
     * retry/block — must not stall the superloop). */
    (void)i2c1_mem_write(I2C_POT_ADDR, I2C_POT_REG, &dac, 1u);
}
