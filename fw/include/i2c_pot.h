/* fw/include/i2c_pot.h — U4 I2C digital potentiometer (amplitude / I2C_POT @0x28).
 *
 * Shares I2C1 (PB6/PB7) with the FRAM; all access is superloop-only (no ISR).
 * Two amplitude paths converge here (spec 2026-06-28-i2c-pot-amplitude-actuation):
 *   - app_lcd_hook_set_pot(output_power %)  -> pot_dac_from_power() -> i2c_pot_set_dac()
 *   - app_weld_hook_set_amp(raw DAC)         ->                         i2c_pot_set_dac()
 * The %→DAC conversion is a pure HAL-free inline (host-tested); the actual I2C
 * write (i2c_pot_set_dac) is HAL/HW-gated.
 *
 * This header is intentionally HAL-free (only <stdint.h>) so the host test can
 * include it directly.
 */
#pragma once
#include <stdint.h>

/* samd20 i2c_comm.h: I2C_POT device + I2C_POT_WP wiper command byte. */
#define I2C_POT_ADDR  0x28u   /* 7-bit device address */
#define I2C_POT_REG   0x00u   /* wiper write command byte */

/* Pure output_power(%) → wiper DAC. samd20 main.c: (power-50)*255/100, faithful
 * over [50,100], plus underflow guard (<=50 → 0) and upper saturation (>=100 → 127).
 * set_amp does NOT route through this (it carries an already-computed raw DAC). */
static inline uint8_t pot_dac_from_power(uint8_t power)
{
    if (power <= 50u)  { return 0u; }
    if (power >= 100u) { return 127u; }
    return (uint8_t)(((uint16_t)(power - 50u) * 255u) / 100u);
}

/* Write a raw wiper value (0..127) to U4. Clamps >127, best-effort (no retry/
 * block; failures observable via i2c1_err_count()). HAL/HW-gated. */
void i2c_pot_set_dac(uint8_t dac);
