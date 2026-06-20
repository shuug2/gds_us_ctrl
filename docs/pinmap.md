# Pinmap — gds_us_ctrl (GD-SONIC Ultrasonic Controller)

## Target MCU

| 항목 | 내용 |
|------|------|
| MCU | STM32F410RBTx |
| Core | ARM Cortex-M4F |
| SYSCLK | **96 MHz** (HSI 16 MHz × 12 PLL ÷ 2 — Phase 1 결정) |
| APB1 | 48 MHz |
| APB2 | 96 MHz |
| Flash | 128 KB |
| RAM | 32 KB |
| Package | LQFP64 |
| Toolchain | arm-none-eabi-gcc |

> **Source of truth (clock)**: `fw/src/clock.c` — Phase 1 HW verify로 SystemCoreClock 96 MHz 확인 완료. HSE crystal 미사용 (HSI 내부 RC + PLL).

---

## ⚠ 2026-06-20 정정 (M16 disasm + 사용자 HW 확정 — 본문보다 우선)

아래 표 일부가 분석 전 추정이었고, M16 펌웨어 disasm(`ref/atmega16/m16_disasm/firmware_disassembled.asm`) + 사용자 HW 확정으로 정정됨. **충돌 시 본 절 우선.** 상세 = `docs/superpowers/analysis/2026-06-20-bseam-osc-signal-chain-and-port-fidelity.md`.

| 핀 (mega16 원본) | 기존(추정) | 정정(확정) |
|---|---|---|
| PB2 (PB1) | CTRL_OSC0 발진채널 | **SEEK** 출력 (active-LOW binary 미러) |
| PB10 (PB0) | CTRL_OSC1 발진채널 | **RESET** 출력 (active-LOW) |
| PB14 (PC7) | CTRL_OSC4 발진채널 | **RUN** 출력 (active-LOW) |
| PB12 (PC4) | CTRL_OSC2 출력 | mega16 **입력** — OSC 출력 아닐 수 있음(미확정), 미설정 |
| PB13 (PA7) | CTRL_OSC3 출력 | **과부하 sense 입력 (active HIGH)** — OSC 아님 |
| PB3 | CON_OVLD (heartbeat 임시) | CON_OVLD = 외부 **릴레이 제어 출력 (active HIGH)**; heartbeat → **PB8** 이전 |
| PB0 / ADC1_IN8 | 센서 출력 아날로그 | mega16 PA0 = 출력세기 피드백 (레거시 vestigial) |
| PB1 / ADC1_IN9 | ADC_OSC 발진기 모니터 | **SAMD20 PA02 = 소비전류** (전류/전력/에너지 source) |

- **OSC 구동 = binary run/seek/reset active-LOW 미러** (8단 thermometer 패턴 아님 = V26 dead 7-seg). 진폭은 별도 I2C_POT(open Q F2 = U4 칩 정체).
- **overload**: 입력 PB13(active HIGH) → 내부 처리(seek-reset FSM 복구 재사용, 초음파 정지/ERR/LCD) → 출력 PB3 릴레이. 옛 M_OVLD IPC(M16 PC0→SAMD20 PB10) 소멸(PB10=CTRL_OSC1 재용도).
- **heartbeat**: PB3 → PB8(빈 GPIO; 대안 PB9/PB15/PC0-3). ⚠ "mega16 PA7"(→PB13) ≠ "STM32 자신 PA7"(=ETH_MOSI).

---

## Phase 1+2 Bootstrap에서 실제 사용된 핀 (2026-05-05)

> 보드 부팅 + heartbeat + monitor "hello" 데모 검증용. Stage A 진입 시 일부 핀이 본용도로 재할당됨.

| 핀 | Net Name | Direction | Phase 1+2 용도 | 본용도 (Stage A 이후) |
|----|----------|-----------|----------------|-----------------------|
| PB3 | CON_OVLD | Out (Phase 2 임시) | **heartbeat 1 Hz toggle** (50% duty) | CON_OVLD 본용도 복원 = 외부 **릴레이 출력** (heartbeat는 **PB8**로 이전 — 2026-06-20) |
| PC6 | USART6_TX | AF8 (USART6) | Monitor UART TX (115200 8N1) — banner / `[t=N ms] hello` | Stage B 이후 Modbus RTU TX (USART6 섹션 이중 역할 참조) |
| PC7 | USART6_RX | AF8 (USART6) | Monitor UART RX (Phase 2 미사용) | Stage B 이후 Modbus RTU RX |
| — | TIM11 | (no pin) | **1 kHz IRQ source for `sys_tick`** (PSC=95, ARR=999 @ APB2Tim 96 MHz, NVIC priority 5) | 동일 (1 ms 시스템 틱) |
| PA13 | SWDIO | AF (SWD) | ST-LINK debug | 변경 없음 |
| PA14 | SWCLK | AF (SWD) | ST-LINK debug | 변경 없음 |

---

## Pin Assignment

### Power & Clock

| Pin | Net Name | Signal | 설명 |
|-----|----------|--------|------|
| PH0 | OSC_IN | RCC_OSC_IN | 16 MHz 외부 크리스탈 |
| PH1 | OSC_OUT | RCC_OSC_OUT | 16 MHz 외부 크리스탈 |

### Debug (SWD)

| Pin | Net Name | Signal | 설명 |
|-----|----------|--------|------|
| PA13 | — | SYS_JTMS-SWDIO | SWD 데이터 |
| PA14 | — | SYS_JTCK-SWCLK | SWD 클럭 |

### ADC — 아날로그 입력

| Pin | Net Name | Signal | 설명 |
|-----|----------|--------|------|
| PB0 | ADC_SENS_OUT | ADC1_IN8 | mega16 PA0 = 출력세기 피드백 (reg_scale 입력; **레거시 vestigial** — 2026-06-20 정정) |
| PB1 | ADC_CURR | ADC1_IN9 | **SAMD20 PA02 = 소비전류** (전류/전력/에너지 source) — "ADC_OSC" 오라벨 정정 (2026-06-20) |

### USART1 — LCD 통신

> **상태**: Stage A 구현 완료 (2026-05-06). DGUS T5L LCD 와 115200 8N1 양방향 통신. AF7 + IT 1-byte RX (`HAL_UART_Receive_IT` + 64-byte ring) + 폴링 TX (10 ms timeout). 상세 설계: `docs/superpowers/specs/2026-05-05-stage-a-lcd-io-design.md`.

| Pin | Net Name | Signal | AF | 설명 |
|-----|----------|--------|----|------|
| PA9 | LCD_TX | USART1_TX | AF7 | DGUS LCD 시리얼 TX (Stage A 활성) |
| PA10 | LCD_RX | USART1_RX | AF7 | DGUS LCD 시리얼 RX (Stage A 활성, IT byte 모드) |

### USART6 — 모니터링 + Modbus RTU (이중 역할, 단계별 전환)

| Pin | Net Name | Signal | 설명 |
|-----|----------|--------|------|
| PC6 | MON_TX (Phase 2) / MB_TX (Stage B 이후) | USART6_TX | Phase 2: 디버그 콘솔 TX. Stage B부터 Modbus RTU TX |
| PC7 | MON_RX (Phase 2) / MB_RX (Stage B 이후) | USART6_RX | Phase 2: 디버그 콘솔 RX. Stage B부터 Modbus RTU RX |

> **이중 역할 정책 (사용자 결정 2026-04-26)**:
> F410RBT는 USART1(LCD 점유) + USART2(PA2=BUZZER 충돌) 외에 USART6 한 채널만 가용 → Modbus와 Monitor가 같은 포트 공유.
> - **Phase 2 ~ Stage A 종료까지**: USART6 = Monitor (115200 8N1, 디버그 출력)
> - **Stage B (Modbus 구현 시작)**: USART6 = Modbus RTU (속도/패리티는 EEPROM `comm_speed_idx`/`comm_parity_idx`로 가변)
> - **Stage B 이후 Monitor 처리**: 미정 (런타임 스위치 / 컴파일 플래그 / SWO·ITM 이전 중 선택 — Stage B 진입 시 결정)
> - 설계 함의: Monitor 모듈을 **인터페이스/구현 분리** (`mon_uart_write()` 추상화) — Stage B에서 backend만 교체

### SPI1 — Ethernet

| Pin | Net Name | Signal | 설명 |
|-----|----------|--------|------|
| PA4 | ETH_SS | SPI1_NSS | Ethernet SPI CS (HW NSS) |
| PA5 | ETH_SCK | SPI1_SCK | Ethernet SPI CLK |
| PA6 | ETH_MISO | SPI1_MISO | Ethernet SPI MISO |
| PA7 | ETH_MOSI | SPI1_MOSI | Ethernet SPI MOSI |
| PC4 | ETH_INT | GPIO_Input | Ethernet 인터럽트 |
| PC5 | ETH_NRST | GPIO_Output | Ethernet 리셋 |

### I2C1 — EEPROM

| Pin | Net Name | Signal | 설명 |
|-----|----------|--------|------|
| PB6 | EEP_SCL | I2C1_SCL | EEPROM I2C CLK |
| PB7 | EEP_SDA | I2C1_SDA | EEPROM I2C DATA |

### TIM9_CH1 — 부저

| Pin | Net Name | Signal | 설명 |
|-----|----------|--------|------|
| PA2 | BUZZER | TIM9_CH1 (PWM) | 부저 PWM 출력 |

### TIM5_CH1 — 초음파 PWM 출력

| Pin | Net Name | Signal | 설명 |
|-----|----------|--------|------|
| PA0 | US_PWM_OUT | TIM5_CH1 (PWM) | 초음파 PWM 출력 (WKUP 겸용) |

### GPIO Output — 초음파 발진회로 제어 (외부 제어 신호 출력)

> 외부 발진회로로 가는 제어 신호. 5채널 모두 ATmega16의 특정 핀이 구동하던 신호를 F410이 흡수.
> **mega16 출신핀 identity = 보드 직독 확정 (2026-05-25, H-C)**: PB1/PB0/PC4/PA7/PC7 (아래 표). 단 **각 핀 방향(출력 vs 입력)·활성 극성·레벨↔비트 매핑은 사용자 재측정 대기** — PC4·PA7는 mega16 FW상 입력핀(DDR)이라 "순수 5출력"이 아닐 수 있음(OSC보드→M16 피드백 입력 혼재 가능). 상세: `docs/superpowers/analysis/atmega16-io-behavior.md` §0.1 (B1/B2). Stage D 코딩은 이 측정 대기로 보류.

> **2026-06-20 정정**: 아래 5핀 중 **출력은 3개(PB2/PB10/PB14)만** — disasm로 active-LOW binary 명령 미러(RUN/SEEK/RESET) 확정. PB12/PB13은 mega16 **입력**(OSC 출력 아님): PB13=과부하 입력, PB12=미확정. 8단 thermometer 패턴→OSC 가설 기각(=dead 7-seg).

| Pin (F410) | Net Name | ATmega16 원본 핀 | 설명 (2026-06-20 정정) |
|-----|----------|------------------|------|
| PB2  | CTRL_OSC0 | **mega16 PB1** | **SEEK** 신호 출력 (active-LOW binary 미러) |
| PB10 | CTRL_OSC1 | **mega16 PB0** | **RESET** 신호 출력 (active-LOW) |
| PB14 | CTRL_OSC4 | **mega16 PC7** | **RUN** 신호 출력 (active-LOW) |
| PB12 | (OSC2?) | **mega16 PC4** | mega16 **입력**(`sbis 0x13,4` read) — 정체 미확정, 출력 구동 금지(미설정) |
| PB13 | **과부하 입력** | **mega16 PA7** | **과부하 sense 입력 (active HIGH)** — OSC3 아님(오라벨 정정). 디바운스→내부 overload 처리 |

> 출처: 사용자 회로 동작 확인 (2026-04-26).
>
> ATmega16 초기화 값 (`m16_conv_v001.c`):
> - **mega16 PB0, PB1**: `DDRB=0xFF`로 출력. 분석 코드(`m16_conv_v001.c:462-471`)는 라벨 "LED ON/OFF"로 토글 — 실제 회로 기능은 OSC1/OSC0 채널 구동.
> - **mega16 PC7**: `DDRC=0xC3` bit7=1로 출력, `PORTC=0xBC` bit7=1로 초기 HIGH. 분석 코드(`m16_conv_v001.c:455-457`)는 라벨 "LED OFF" — 실제 OSC4 채널 구동.
> - **mega16 PA7**: `DDRA=0x00` 전체 입력, `PORTA=0xFC` bit7=1로 풀업 활성. 분석 코드에서 직접 토글 흔적 없음 — 실제 OSC3 채널 구동(F410 통합).
> - **mega16 PC4**: `DDRC=0xC3` bit4=0 입력, `PORTC=0xBC` bit4=1로 풀업 활성 — 실제 OSC2 채널 구동(F410 통합).
>
> 분석 코드의 핀 라벨("LED", "보조" 등)은 토글 동작 추정일 뿐, 보드 회로 기능은 사용자 정보 우선.

### GPIO Output — 커넥터 출력

| Pin | Net Name | 설명 |
|-----|----------|------|
| PB3 | CON_OVLD | 과부하 출력 = 외부 **릴레이 제어 신호 (active HIGH)**. ⚠ 현재 Phase2 heartbeat 임시 점유 → heartbeat **PB8** 이전 후 복원 (2026-06-20) |
| PB4 | CON_UVOUT | UV 출력 |
| PB5 | CON_SOL_DN | 솔레노이드 하강 출력 |
| PB8 | (heartbeat) | **신규 heartbeat 핀** (PB3에서 이전, 빈 GPIO; 2026-06-20 결정) |

### GPIO Input — 커넥터 입력

| Pin | Net Name | 설명 |
|-----|----------|------|
| PA11 | CON_SENS_DN | 센서 하강 감지 |
| PA12 | CON_SENS_UP | 센서 상승 감지 |
| PA15 | CON_START | 시작 신호 |
| PB11 | CON_KEY2 | 키 입력 2 |
| PC10 | CON_RESET | 리셋 입력 |
| PC11 | CON_ESTOP | 비상 정지 |
| PC12 | CON_KEY1 | 키 입력 1 |

---

## Peripherals Summary

| 주변장치 | 핀 | 용도 |
|---------|-----|------|
| USART1 | PA9/PA10 | LCD 시리얼 통신 |
| USART6 | PC6/PC7 | 모니터링/디버그 |
| SPI1 | PA4–PA7, PC4, PC5 | Ethernet (W5500) |
| I2C1 | PB6/PB7 | EEPROM |
| ADC1 | PB0(출력세기 피드백, vestigial)/PB1(**소비전류**, 2026-06-20 정정) | 아날로그 입력 2채널 |
| TIM9_CH1 | PA2 | 부저 PWM |
| TIM5_CH1 | PA0 | 초음파 PWM 출력 |
| TIM11 | — | 내부 타이머 (용도 TBD) |
| RTC | — | 실시간 클럭 (LSI 32 kHz) |
| SWD | PA13/PA14 | 디버그/플래시 |
| GPIO Out | PB2/PB10/PB14(OSC SEEK/RESET/RUN active-LOW), PB3(CON_OVLD 릴레이)/PB4/PB5, PB8(heartbeat), PC5 | 발진제어/커넥터 (2026-06-20 정정) |
| GPIO In | PA11/PA12/PA15, PB11, **PB13(과부하)**, PC4, PC10–PC12 | 센서/키/이벤트/과부하 입력. PB12=미확정 |

---

## Clock Tree

```
HSE 16 MHz
  └→ PLL (×12, ÷2) → SYSCLK 96 MHz
       ├→ AHB  96 MHz
       ├→ APB1 48 MHz  (TIM ×2 = 96 MHz)
       └→ APB2 96 MHz  (TIM ×1 = 96 MHz)

RTC ← LSI 32 kHz
USB 48 MHz ← PLLQ
```

---

## 미사용 / 확인 필요

| 항목 | 비고 |
|------|------|
| TIM11 | **확정 — 1 kHz IRQ 시스템 틱**. Phase 2에서 활성 (PSC=95, ARR=999, NVIC priority 5). HW 검증 완료 (Chunk 12) |
| PA0 (TIM5_CH1) | Net Name **`US_PWM_OUT` 추가 완료** (`.ioc` 반영) |
| CTRL_OSC active level | **확정 (2026-06-20)**: 출력 3채널 PB2/PB10/PB14 = **active-LOW** binary 미러(SEEK/RESET/RUN). PB12/PB13=입력(출력 아님). |
| ADC1 IN9 (PB1) | **= SAMD20 소비전류**(2026-06-20 정정, 옛 "발진 모니터" 아님). 채널 활성 + 6b 보정 필요 |

---

## 부록 A — 이전 MCU 간 GPIO 시그널 (참고용, 신규 보드에서는 폐지)

이전 시스템에서 **SAMD20 ↔ ATmega16** 사이는 4개 GPIO만으로 통신했다. STM32F410 통합 시에는 모두 **내부 함수 호출/상태 변수**로 대체되며, 아래 이전 핀들은 신규 보드에서 다른 신호로 재할당된다.

| 신호명 | 방향 | SAMD20 핀 | ATmega16 핀 | 활성 레벨 | 의미 |
|--------|------|-----------|-------------|----------|------|
| `M_START` | SAMD20 → ATmega16 | PB13 (out) | PA4 (in) | LOW | 초음파 출력 개시 |
| `M_SEEK`  | SAMD20 → ATmega16 | PB11 (out) | PA5 (in) | LOW | 공진 주파수 탐색 |
| `M_RESET` | SAMD20 → ATmega16 | PB12 (out) | PA6 (in) | LOW | 초음파 초기화 |
| `M_OVLD`  | ATmega16 → SAMD20 | PB10 (in)  | PC0 (out) | LOW(추정) | 과부하 알람. **2026-06-20: IPC 소멸** — STM32가 과부하 입력(PB13←PA7, active HIGH)을 직접 처리, 외부 릴레이는 PB3/CON_OVLD로 출력 |

> 출처: `ref/samd20/ASF/common2/boards/user_board/user_board.h:75-103` + 사용자 회로 동작 확인.
> SAMD20측 호출 위치: `M_START` `main.c:1416,1552,1564,1665,1686,4185,4303,5252,5276`, `M_RESET` `main.c:4256,4262`, `M_OVLD` `main.c:1201`.

신규 F410 보드 매핑 (위 핀들의 새 용도):

| F410 핀 | 신규 용도 | 비고 |
|---------|----------|------|
| PB10 | `CTRL_OSC1` (out) | 이전 M_OVLD 자리 |
| PB11 | `CON_KEY2` (in)  | 이전 M_SEEK 자리 |
| PB12 | `CTRL_OSC2` (out) | 이전 M_RESET 자리 |
| PB13 | **과부하 입력** (in) | 이전 M_START 자리. ⚠ "CTRL_OSC3 out"은 오기 — 실제 mega16 PA7=과부하 sense 입력 (2026-06-20) |

---

## 부록 B — 결정 사항 (2026-04-26 후속)

| # | 항목 | 결정 |
|---|------|------|
| Q2-잔 | ATmega16 PC1 출력 의미 | **무시** (신규 보드 흡수 시 미사용) |
| Q3 | ATmega16 PC4 입력 정체 | **외부 입력 신호** (SAMD20 신호 아님) |
| Q4 | 7-segment 디스플레이 처리 | **삭제** — DGUS LCD가 모든 표시 흡수 |
| Q5 | `disp_led_pattern` (0x00–0xFF) 의미 | **단순 표시 출력** (외부 비트 출력 아님 → Q4 삭제로 같이 폐기) |
| Q6 | SAMD20 `I2C_POT` 정체 | **외부 디지털 포텐셔미터 칩** (I2C 마스터 그대로 사용) |
| Q7 | PA0 (TIM5_CH1) Net Name | **`US_PWM_OUT` 추가 완료** |
| Q8 | CTRL_OSC0~4 정체 | **외부 제어 신호 출력** |

## 부록 C — 구현 시점 재확인 (회로도/실측 필요)

| # | 항목 | 재확인 시점 |
|---|------|----------|
| Q8b | CTRL_OSC0~4 **방향·극성·매핑** | **대부분 해소 (2026-06-20, disasm+HW)**: PB2/PB10/PB14=출력 active-LOW(SEEK/RESET/RUN binary 미러), PB13=과부하 입력, **PB12(PC4)만 미확정**. 극성 sanity만 보드 실측 |
| Q10 | ADC1 IN9 (PB1) `.ioc`에 채널 추가 | Phase 1 CubeMX 보정 + Stage D ADC 흡수 시 |
| Q11 | ADC1 IN8 (PB0) = mega16 PA0 = **출력세기 피드백** | **확정 (2026-05-25)**. ⚠ **2026-06-20**: 레거시 양쪽 vestigial(M16 dead 7seg/SAMD20 curr_lv 주석) → 통합 역할 미결정. **IN9(PB1)=SAMD20 소비전류 확정**(B4 해소, 옛 "ch1 발진모니터" 아님) |
