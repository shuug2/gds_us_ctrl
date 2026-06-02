/* fw/include/adc1.h — ADC1 driver: PB0/ADC1_IN8 (ADC_SENS_OUT) + PB1/ADC1_IN9
 * (ADC_OSC), polled single conversion. Driver owns its analog pins. */
#pragma once
#include <stdint.h>

/* Semantic channels (driver maps to HAL ADC_CHANNEL_* internally so callers
 * stay HAL-free). ch0 = regulation sense feedback; ch1 = oscillator monitor. */
typedef enum {
    ADC1_CH_SENS_OUT = 0,   /* PB0 / ADC1_IN8 — mega16 ch0 (regulation input) */
    ADC1_CH_OSC      = 1,   /* PB1 / ADC1_IN9 — mega16 ch1 (monitor, B4) */
} adc1_ch_t;

/* Init ADC1 + PB0/PB1 analog GPIO. Call once at boot (via app_reg_init). */
void adc1_init(void);

/* Blocking single conversion on the given channel. Returns the raw 12-bit
 * count (0..4095). Caller normalizes to the regulation domain (>>2). */
uint16_t adc1_read(adc1_ch_t ch);
