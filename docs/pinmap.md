# Pinmap — gds_us_ctrl (GD-SONIC Ultrasonic Controller)

## Target MCU
- **MCU:** STM32F410RBT
- **Core:** ARM Cortex-M4F @ 96 MHz (HSI ×12, source of truth = `fw/src/clock.c`)
- **Flash:** 128 KB
- **RAM:** 32 KB
- **Package:** LQFP64
- **SDK:** `$STM32_SDK`
- **Toolchain:** `$STM32_TOOLCHAIN` (arm-none-eabi-gcc)

## Phase 1+2 Bootstrap에서 사용된 핀

> Phase 1+2 단계는 "보드가 살아있다" 검증용. Stage A 이후 본용도로 핀이 재할당됨.

| 핀 | Net Name | Direction | Phase 2 용도 | 본용도 (Stage A 이후) |
|----|----------|-----------|--------------|-----------------------|
| PB3 | CON_OVLD | Out (Phase 2 임시) | **heartbeat 1 Hz toggle** (50% duty) | CON_OVLD 본용도 복원 (외부 출력) |
| PC6 | USART6_TX | AF8 (USART6) | Monitor UART TX (115200 8N1) — banner / `[t=N ms] hello` | Stage B 이후 Modbus RTU TX (이중 역할) |
| PC7 | USART6_RX | AF8 (USART6) | Monitor UART RX (Phase 2 미사용) | Stage B 이후 Modbus RTU RX |
| — | TIM11 | (no pin) | 1 kHz IRQ source for `sys_tick` (PSC=95, ARR=999 @ APB2Tim 96 MHz) | 동일 (1 ms 시스템 틱) |
| PA13 | SWDIO | AF (SWD) | ST-LINK debug | 변경 없음 |
| PA14 | SWCLK | AF (SWD) | ST-LINK debug | 변경 없음 |

## Pin Assignment (전체 — Stage A부터 채움)

| Pin | Port | Net Name | Direction | F/W 용도 |
|-----|------|----------|-----------|---------|
| 2   | PC13 |          |           |         |
| ... |      |          |           |         |

> Stage A (LCD + IO) 단계부터 본격 채움. 본 표는 plan task 27 시점에서 stub.

## Peripherals

### Phase 1+2에서 활성
- [x] **TIM11** — sys_tick 1 kHz IRQ (TIM1_TRG_COM_TIM11_IRQHandler, NVIC priority 5)
- [x] **USART6** (PC6/PC7, AF8) — Monitor UART, blocking TX, 115200 8N1
- [x] **GPIOB PB3** — heartbeat output (Phase 2 임시)
- [x] **HSI ×12 PLL** — 96 MHz SystemCoreClock

### Stage A 이후 활성 예정
- [ ] USART1 — DGUS LCD (PA9/PA10)
- [ ] USART6 (Stage B 재할당) — Modbus RTU
- [ ] I2C1 — EEPROM + 디지털 POT (PB6/PB7)
- [ ] SPI1 — W5500 Ethernet (PA4–PA7)
- [ ] ADC1 — IN8/IN9 (PB0/PB1) 센서 / 발진기 모니터
- [ ] TIM5 — 초음파 PWM (PA0)
- [ ] TIM9 — Buzzer PWM (PA2)
- [ ] CTRL_OSC0~4 — 5채널 외부 발진 ON/OFF
