# Requirements — GD-SONIC Ultrasonic Controller

## Overview
ATSAMD20 + ATmega16 두 MCU로 분리되어 있던 초음파 컨트롤러를 **STM32F410RBT 단일 MCU**로 통합.

---

## 시스템 아키텍처 (이전 → 신규)

### 이전 (Legacy)
```
[SAMD20 메인 컨트롤러] ─── GPIO 신호 ──── [ATmega16 서브 컨트롤러]
       │                                          │
       ├─ DGUS LCD (UART)                        ├─ ADC 2채널 (자체 처리)
       ├─ Modbus RTU/TCP                         ├─ 7-seg 3-digit 디스플레이
       ├─ W5500 Ethernet (SPI)                   ├─ 버튼 2개 디바운스
       ├─ I2C 디지털 POT (출력 레벨)              ├─ 21단계 룩업 테이블 출력 결정
       ├─ 5채널 초음파 발진 ON/OFF                ├─ 하트비트 LED
       └─ TIM5 초음파 PWM                        └─ EEPROM 설정 저장
```

**MCU 간 통신: 순수 GPIO 시그널링 (UART/SPI/TWI 모두 비활성)**

### 신규 (STM32F410RBT)
모든 기능 통합. 5x 초음파 발진회로 ON/OFF + 1x 초음파 PWM + W5500 Ethernet + DGUS LCD + I2C EEPROM.

---

## Functional Requirements

### F1. 통신 (택일 동작)
- **F1.1** Modbus RTU (시리얼) 또는 Modbus TCP (이더넷) — `comm_mode` 플래그로 선택
- **F1.2** DHCP 클라이언트 또는 정적 IP 지원
- **F1.3** TCP 서버 단일 소켓 (W5500 SOCK_TCPS), Modbus PDU 디코드

### F2. 초음파 출력 제어
- **F2.1** 5채널 초음파 발진회로 ON/OFF 제어 (CTRL_OSC0~4)
- **F2.2** 1채널 초음파 PWM 출력 (TIM5_CH1 / PA0)
- **F2.3** 출력 레벨 21단계 (룩업 테이블 기반 — ATmega16 알고리즘 흡수)
- **F2.4** Multi-stage 제어 (시간 경과에 따른 출력 레벨 변경)

### F3. ADC 처리
- **F3.1** PB0 (ADC_SENS_OUT, IN8): 센서 출력 — 10 샘플 평균 (빠른 응답)
- **F3.2** PB1 (ADC_OSC, IN9): 발진기 모니터 — 50 샘플 평균 (느린 응답)
- **F3.3** ADC 결과 → 룩업 테이블 스케일링 → 출력 레벨 결정

### F4. UI
- **F4.1** DGUS LCD (USART1 / PA9·PA10): 사용자 명령/상태 표시 — `parse_lcd_comm()` 핵심
  - **이전 ATmega16 7-segment 3-digit 디스플레이는 폐기 → DGUS LCD가 모든 표시 흡수** (2026-04-26 결정)
- **F4.2** Buzzer PWM (TIM9_CH1 / PA2): 알람음
- **F4.3** 키 입력: CON_KEY1/2, CON_START, CON_RESET, CON_ESTOP
- **F4.4** 모니터 UART (USART6 / PC6·PC7): 디버그/모니터링

### F5. 데이터 저장
- **F5.1** I2C EEPROM (PB6·PB7): 설정값 (comm_mode, IP, freq, type, limits 등)
- **F5.2** 부팅 시 SW_CNT_RESET 200회 감지 시 공장 초기화

### F6. 스케줄링
- **F6.1** 슈퍼루프 + 1ms 틱 (TIM11 권장)
- **F6.2** 10단계 `job_state` 머신 (각 ~80us 이내, 한 사이클 ~20ms)
- **F6.3** 200ms 단위 시간 누적 (ON 시간 측정)

---

## Hardware Requirements
- **MCU**: STM32F410RBT (Cortex-M4F @ 96MHz, 128KB Flash, 32KB RAM, LQFP64)
- **Ethernet**: W5500 (SPI1)
- **Display**: DGUS LCD (USART1)
- **EEPROM**: I2C 연결 (PB6/PB7)
- **클럭**: HSE 16MHz 외부 크리스탈 → PLL 96MHz

---

## Firmware Requirements

### FW1. 빌드
- arm-none-eabi-gcc, CubeMX 6.16.1 + CMake + Ninja
- HAL/CMSIS 기반 (FreeRTOS 미사용 — 슈퍼루프로 충분)

### FW2. 코드 구조
- `fw/cube/`: CubeMX 생성 코드 (USER CODE 섹션 보존)
- `fw/src/`: 애플리케이션 모듈
- `fw/include/`: 공용 헤더
- `fw/drivers/`: W5500, DGUS LCD, EEPROM 드라이버

### FW3. 통합 우선순위 (2026-04-26 사용자 지정 순서)
1. **베이스 펌웨어 골격** — CubeMX 생성, TIM11 1ms 틱, 빈 10단계 `job_state` 슈퍼루프
2. **Stage A — LCD + IO**: DGUS LCD + `parse_lcd_comm()`, 키/스위치 입력, GPIO 출력(외부 발진 제어), Buzzer, 모니터 UART
3. **Stage B — Modbus 통신**: `init_modbus`, `decode_comm` (RTU 우선, holding 레지스터)
4. **Stage C — Ethernet 통신**: W5500 SPI 드라이버, DHCP/정적 IP, Modbus TCP
5. **Stage D — mega16 기능 흡수**: ADC 평균화, 21단계 룩업, 출력 레벨 + CTRL_OSC + TIM5 PWM, `do_control()`, I2C EEPROM, I2C 디지털 POT
6. 워치독, 단위 테스트, 통합 테스트, 첫 릴리즈 태그
