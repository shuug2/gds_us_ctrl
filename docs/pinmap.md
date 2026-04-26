# Pinmap — gds_us_ctrl (GD-SONIC Ultrasonic Controller)

## Target MCU
- **MCU:** STM32F410RBT
- **Core:** ARM Cortex-M4F @ 100 MHz
- **Flash:** 128 KB
- **RAM:** 32 KB
- **Package:** LQFP64
- **SDK:** `$STM32_SDK`
- **Toolchain:** `$STM32_TOOLCHAIN` (arm-none-eabi-gcc)

## Pin Assignment

| Pin | Port | Net Name | Direction | F/W 용도 |
|-----|------|----------|-----------|---------|
| 2   | PC13 |          |           |         |
| ... |      |          |           |         |

## Peripherals
- [ ] UART1 — 디버그 콘솔
- [ ] I2C1 —
- [ ] SPI1 —
- [ ] ADC1 —
- [ ] TIM1 — 초음파 PWM
