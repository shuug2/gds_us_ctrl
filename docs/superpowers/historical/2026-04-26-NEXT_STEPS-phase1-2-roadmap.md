# NEXT_STEPS — 다음 세션 진행 가이드

> 이 파일은 세션 간 컨텍스트 인계용. 새 세션 시작 시 먼저 이 파일을 읽고 이어서 진행.

> ⏸️ **현재 일시 중단 (2026-04-26)**: Phase 1+2 설계 brainstorming이 섹션 1/6에서 중단됨.
> 재개 시 우선 **`docs/superpowers/RESUME.md`** 읽기 → 결정/대기/남은 섹션 모두 거기 정리.

---

## 현재 진행 상황 (as of 2026-04-26)

### ✅ 완료
1. **프로젝트 구조 생성** — `fw/`, `hw/`, `docs/`, `ref/` 디렉토리
2. **CLAUDE.md 작성** — 프로젝트 개요, 통합 목표, 디렉토리 구조
3. **CubeMX `.ioc` 분석** — `fw/cube/gds_usctrl.ioc`에서 핀 할당 추출
4. **`docs/pinmap.md` 작성** — STM32F410RBT 전체 핀 맵 정리
5. **`ref/samd20/` + `ref/atmega16/` 코드 분석**
   - graphify로 그래프 생성: `graphify-out/graph.html`, `GRAPH_REPORT.md`
   - 340 노드 / 851 엣지 / 39 커뮤니티
6. **SAMD20 `main()` 슈퍼루프 구조 추적** — 10-state job_state 머신, 1ms 틱 기반
7. **ATmega16 단독 분석** — 핀 매핑, 21단계 룩업 테이블, 인터럽트 구조
8. **`docs/requirements.md` 업데이트** — 시스템 아키텍처, F1~F6, FW1~FW3

### ✅ 해소 (2026-04-26 후속)

| # | 항목 | 답변 |
|---|------|------|
| Q1 | ATmega16 PA4 ← SAMD20 어느 출력 | **`M_START = PIN_PB13` (active LOW)** — 초음파 출력 개시 |
| Q2 (PC0) | ATmega16 PC0 → SAMD20 어느 입력 | **`M_OVLD = PIN_PB10`** — 과부하 알람 |
| (신규) | ATmega16 PA5 ← SAMD20 어느 출력 | **`M_SEEK = PIN_PB11` (active LOW)** — 공진 주파수 탐색 |
| (신규) | ATmega16 PA6 ← SAMD20 어느 출력 | **`M_RESET = PIN_PB12` (active LOW)** — 초음파 초기화 |
| Q9 | TIM11 용도 | **1 ms 시스템 틱 (확정 권고)** |

> 출처: `ref/samd20/ASF/common2/boards/user_board/user_board.h:75-103` + 사용자 회로 동작 확인.
> 통합 시 4개 GPIO는 **내부 함수/플래그**로 대체 (us_start/us_seek/us_reset/g_overload). PB10–PB13은 F410에서 CTRL_OSC1/CON_KEY2/CTRL_OSC2/CTRL_OSC3로 재할당됨.

### ✅ 추가 해소 (2026-04-26 후속 #2)

| # | 항목 | 결정 |
|---|------|------|
| Q2-잔 | ATmega16 PC1 출력 의미 | **무시** (신규 흡수 시 미사용) |
| Q3 | ATmega16 PC4 입력 정체 | **외부 입력 신호** (SAMD20 신호 아님) |
| Q5 | `disp_led_pattern` 의미 | **단순 표시 출력** (외부 비트 출력 아님) |
| Q6 | SAMD20 `I2C_POT` 정체 | **외부 디지털 포텐셔미터 칩** (I2C 마스터 그대로 사용) |
| Q7 | PA0 Net Name | **`US_PWM_OUT` `.ioc`에 추가 완료** |
| Q8 | CTRL_OSC0~4 정체 | **외부 제어 신호 출력** 확정. 활성 극성은 추후 확정(유보) |

### ✅ 추가 해소 (2026-04-26 후속 #3)

| # | 항목 | 결정 |
|---|------|------|
| Q4 | 7-segment 디스플레이 처리 | **삭제** — DGUS LCD가 모든 표시 흡수. ATmega16 `disp_led_pattern` / 7-seg PORTB·PORTD 출력 폐기 |

### ⚠️ 잔여 (구현 시점 재확인)

| # | 항목 | 시점 |
|---|------|------|
| Q8b | CTRL_OSC0~4 활성 극성 (Active H/L) | Stage D `do_control()` 구현 시 회로도 + 실측 |
| Q10 | ADC1 IN9 (PB1) `.ioc`에 채널 추가 | Phase 1 CubeMX 보정 + Stage D ADC 흡수 시 확인 |

---

## 다음 단계 (순서대로)

> **개발 순서 결정 (2026-04-26 사용자 지정):**
> **(A) LCD + IO 구현 → (B) Modbus 통신 → (C) Ethernet 통신 → (D) mega16 기능 흡수**
> 각 스테이지는 모두 SAMD20 참고 코드(`ref/samd20/`) 우선, ATmega16 흡수는 마지막 단계(D)에서 일괄.

### Phase 1: CubeMX 보정
- [ ] `gds_usctrl.ioc`에 `ADC1 IN9` 채널 추가 (PB1)
- [ ] `TIM11` 용도 확정 — 1ms 시스템 틱으로 사용 권장
- [x] `PA0` (TIM5_CH1) Net Name = `US_PWM_OUT` (2026-04-26 추가 완료)
- [ ] CubeMX 코드 생성 → `fw/src`, `fw/include`로 정리

### Phase 2: 빌드 시스템 + 슈퍼루프 골격
- [ ] `fw/CMakeLists.txt` 작성 (CMake + Ninja)
- [ ] `arm-none-eabi-gcc` 툴체인 설정 (`$STM32_TOOLCHAIN`)
- [ ] HAL/CMSIS 경로 (`$STM32_SDK`)
- [ ] 빌드 검증: `cmake -B build -G Ninja && cmake --build build`
- [ ] OpenOCD/J-Link 플래시 명령 정리
- [ ] **시스템 틱**: TIM11 1ms IRQ → `tick_1ms_state` 토글
- [ ] **슈퍼루프 골격**: 빈 10단계 `job_state` 스위치 (각 케이스 placeholder)

> 본격적인 모듈 흡수는 Phase 3부터. Phase 2까지가 "보드가 살아있다"의 토대.

---

### Phase 3 (Stage A): LCD + IO 구현 — `ref/samd20/` 참조
> 사용자 가시(可視) 영역을 가장 먼저. 이후 단계 디버깅이 LCD/시리얼 콘솔로 가능해짐.

1. **DGUS LCD UART 드라이버** (`USART1` PA9/PA10, IRQ 기반 RX 누적)
   - 원본: `ref/samd20/dgus_lcd.c` 송수신 헬퍼 + `ref/samd20/main.c:3248` `parse_lcd_comm()` (888 lines)
   - 산출: `fw/drivers/dgus_lcd/` (`init_lcd_comm`, `send_lcd_data_var`, `send_lcd_data_word`, `send_lcd_byte_array`, `send_lcd_int_array`, `send_lcd_txt`, `change_lcd_page`, `read_system_var`, `reset_dgus_lcd`)
   - 산출: `fw/src/dgus_lcd_parser.c` (`parse_lcd_comm` — VP 주소 → 핸들러 디스패치)
   - 검증: 전원 on → 시작 페이지 표시. 키 입력 1개 수신 → echo back.
2. **GPIO 입력 (커넥터 입력)** — 폴링 + 디바운스
   - `CON_START` (PA15), `CON_RESET` (PC10), `CON_ESTOP` (PC11), `CON_KEY1` (PC12), `CON_KEY2` (PB11)
   - `CON_SENS_DN` (PA11), `CON_SENS_UP` (PA12)
   - 원본 디바운스 알고리즘: ATmega16 `m16_conv_v001.c:322-415` (참고만, ATmega16 코드 자체는 Stage D에서 흡수)
   - SAMD20측 키 처리: `ref/samd20/main.c::sys_tick_func()` (10ms 주기 키 스캔)
   - 산출: `fw/src/buttons.c`
3. **GPIO 출력 (커넥터 출력 + 외부 발진회로 제어)**
   - `CON_OVLD` (PB3), `CON_UVOUT` (PB4), `CON_SOL_DN` (PB5)
   - `CTRL_OSC0~4` (PB2/PB10/PB12/PB13/PB14) — 활성 극성 미확정(Q8b), 일단 active HIGH로 가정
   - 산출: `fw/src/io_ctrl.c`
4. **Buzzer PWM** — TIM9_CH1 (PA2)
   - 원본: SAMD20 `BZ_PWM_0_*` 매크로 (`user_board.h:132-136`)
   - 산출: `fw/src/buzzer.c`
5. **모니터 UART** — `USART6` (PC6/PC7)
   - 원본: `ref/samd20/main.c::configure_usart_*_callback_mon` + `command_out`, `my_itoa32`
   - 검증: printf-like 디버그 출력
   - 산출: `fw/src/mon_uart.c`

**Stage A 종료 조건**: LCD에 메시지 표시 + 키 입력 검출 + 외부 출력 토글 + 모니터 UART로 디버그 메시지 송출 모두 성공.

---

### Phase 4 (Stage B): Modbus 통신 — `ref/samd20/modbus.c` 참조
> Modbus는 LCD가 살아있어야 디버깅이 쉬움 (Stage A 의존).

1. **Modbus RTU 시리얼 드라이버**
   - 원본: `ref/samd20/modbus.c` (`init_modbus`, `decode_comm`, `read_coil`, `read_input_coil`, `read_reg`, `read_input_reg`, `write_coil`, `write_reg`, `clear_response`, `make_crc`, `check_crc`)
   - USART(미정 — USART2/3 후보 — Phase 1에서 추가 필요) + 1.04 ms inter-frame 타이머 (TIM6 또는 TIM7 후보)
   - **comm_mode = COMM_SERIAL (0)** 분기만 우선 구현
   - 함수 코드 0x01–0x06 모두 지원
   - 산출: `fw/src/modbus.c`, `fw/include/modbus.h`
2. **Holding 레지스터 매핑**
   - 원본: `ref/samd20/main.c::update_holding_reg()` 주변
   - 신규에서도 동일 레지스터 주소 유지 (외부 마스터 호환)
3. **검증**
   - Modbus Poll 등 PC 도구로 RTU 통신 — read holding (0x03), write single (0x06), write multiple

**Stage B 종료 조건**: PC Modbus 마스터에서 holding 레지스터 read/write 정상.

---

### Phase 5 (Stage C): Ethernet 통신 — `ref/samd20/W5500/*` + `dhcp.c` + `process_tcp.c` 참조
> W5500은 SPI 콜백만 F410 HAL로 갈아끼우면 ioLibrary 거의 그대로 동작.

1. **W5500 SPI 드라이버** (`SPI1` PA4–PA7 + PC4 INT + PC5 NRST)
   - 원본: `ref/samd20/W5500/`, `wizchip_conf.c`, `socket.c`, `w5500.c`
   - 콜백 교체: `_DAT_IO_BASE_` SPI 콜백 → STM32 HAL `HAL_SPI_TransmitReceive()`
   - 리셋 시퀀스: PC5 NRST 토글 + PC4 INT 입력
   - 산출: `fw/drivers/w5500/`
2. **Ethernet 초기화** — `ref/samd20/ethernet.c::ethernet_init()`, `Net_Conf()`, `print_network_information()`
3. **DHCP 클라이언트** — `ref/samd20/dhcp.c`
   - `default_ip_assign()`, `default_ip_conflict()`, `default_ip_update()` 콜백 등록
   - `DHCP_init()`, `DHCP_run()`
   - 폴백: 정적 IP 모드 (`comm_mode == COMM_ETH_STATIC`)
4. **Modbus TCP 서버** — `ref/samd20/process_tcp.c`
   - 단일 SOCK_TCPS=0
   - `decode_comm(mode=1)` 분기 활성화 → `send_tcps()`로 응답
5. **PHY 상태 모니터** — `check_PHYstatus()` 슈퍼루프에서 호출

**Stage C 종료 조건**: DHCP 동작 + 정적 IP 동작 + Modbus TCP 마스터에서 read/write 정상. `comm_mode` 플래그로 RTU↔TCP 전환 동작.

---

### Phase 6 (Stage D): mega16 기능 흡수 — `ref/atmega16/m16_conv_v001.c` 참조
> 가장 마지막. 여기까지 오면 LCD/IO/통신은 모두 동작 중이라 ATmega16 알고리즘 검증이 쉬움.

1. **ADC 평균화** (`m16_conv_v001.c:612-663` `adc_process_channels()`)
   - PB0 (IN8) 센서: 10 샘플 평균 → `adc_ch0_avg`
   - PB1 (IN9) 발진기: 50 샘플 평균 → `adc_ch1_avg`
   - HAL ADC + DMA 또는 폴링 (Phase 1에서 IN9 추가 필요)
   - 산출: `fw/src/adc.c`
2. **21단계 룩업 출력 결정** (`m16_conv_v001.c:881-981` `output_level_process()` + `lookup_table[24]`)
   - 룩업 테이블 그대로 복사
   - 산출: `fw/src/output_level.c`
3. **초음파 출력 제어 통합**
   - 이전 GPIO 시그널 4개를 **내부 함수로 변환**:
     - `us_start_request()` ← 이전 `M_START` 펄스
     - `us_freq_seek_start()` ← 이전 `M_SEEK` 펄스
     - `us_reset_init()` ← 이전 `M_RESET` 펄스
     - `g_overload_flag` ← 이전 `M_OVLD` 입력 (출력단 검출 로직이 직접 set)
   - `CTRL_OSC0~4` ON/OFF + `TIM5_CH1` PWM duty 매핑 (출력 레벨 → 5채널 + PWM)
   - 원본 통합점: `ref/samd20/main.c::do_control()` (`main.c:1446-`)
   - 산출: `fw/src/us_ctrl.c`
4. **`job_state` 10단계 본격 구현**
   - Stage A에서 만든 placeholder를 실제 로직으로 채움
   - 원본: `ref/samd20/main.c:5108-5202`
   - `do_action()`, `do_control()`, `calc_freq()`, `send_outpower_data()`, `send_outtime_data()`, `send_val_data()`
5. **I2C EEPROM 설정 저장** (`I2C1` PB6/PB7)
   - 원본: `ref/samd20/main.c::read_param_fram()` 및 EEPROM 인터페이스
   - 부팅 시 로드, 변경 시 저장
   - `SW_CNT_RESET` 200회 감지 → 공장 초기화
   - 산출: `fw/src/config_storage.c`
6. **I2C 디지털 POT** (외부 칩, Q6 확정)
   - 원본: `ref/samd20/i2c_comm.c::i2c_adc_*`
   - 출력 레벨 → POT 값 매핑 그대로 유지
   - 산출: `fw/drivers/i2c_pot/`

**Stage D 종료 조건**: 21단계 출력 레벨 정상 변화 + multi_ctrl 단계 전이 + 과부하 검출 + EEPROM 영속화.

---

### Phase 7: 보조 기능 + 통합 테스트
- [ ] **워치독 (IWDG)** 활성화
- [ ] **단위 테스트** — 룩업 테이블, ADC 평균화, Modbus 디코드, CRC
- [ ] **HW-in-loop 테스트** — 실제 보드 + DGUS LCD + 외부 발진회로
- [ ] **Modbus RTU/TCP 통신 검증** (Modbus Poll 등)
- [ ] **DHCP / 정적 IP 모드 검증**
- [ ] **첫 릴리즈 태그**: `hw-revA_fw-1.0.0`

---

## 핵심 코드 참조 위치 (포팅 시)

| 기능 | 원본 위치 | F410 포팅 대상 |
|------|---------|--------------|
| 슈퍼루프 + job_state | `ref/samd20/main.c:4859-5310` | `fw/src/main.c` |
| LCD 명령 파싱 | `ref/samd20/dgus_lcd.c` (`parse_lcd_comm`) | `fw/src/dgus_lcd.c` |
| Modbus RTU/TCP | `ref/samd20/modbus.c` | `fw/src/modbus.c` |
| TCP 서버 | `ref/samd20/process_tcp.c` | `fw/src/process_tcp.c` |
| W5500 SPI | `ref/samd20/W5500/*` | `fw/drivers/w5500/` |
| DHCP | `ref/samd20/dhcp.c` | `fw/src/dhcp.c` |
| ADC 평균화 | `ref/atmega16/m16_conv_v001.c:612-663` | `fw/src/adc.c` |
| 21단계 룩업 출력 | `ref/atmega16/m16_conv_v001.c:881-981` | `fw/src/output_level.c` |
| 버튼 디바운스 | `ref/atmega16/m16_conv_v001.c:322-415` | `fw/src/buttons.c` |

---

## 분석 산출물

- `graphify-out/graph.html` — 인터랙티브 코드 그래프 (브라우저로 열기)
- `graphify-out/GRAPH_REPORT.md` — 감사 리포트 (커뮤니티, God Nodes, Surprises)
- `graphify-out/graph.json` — 그래프 데이터

**다음 세션에서 "/graphify query <질문>" 또는 "/graphify path A B" 사용 가능 (재구축 불필요).**

---

## 결정 사항 (확정)

| 항목 | 결정 | 근거 |
|------|------|------|
| 개발 순서 | **A: LCD+IO → B: Modbus → C: Ethernet → D: mega16 흡수** | 사용자 지정 (2026-04-26). UI/디버깅 인프라 우선, ATmega16 알고리즘 흡수는 마지막 |
| RTOS 도입 | ❌ 슈퍼루프 유지 | SAMD20 코드가 단순 슈퍼루프로 잘 동작. F410은 96MHz로 SAMD20(48MHz) 2배 → 마진 충분 |
| Ethernet 칩 | W5500 (SPI1) 유지 | 기존 코드 재사용 가능 |
| LCD | DGUS LCD (USART1) 유지 | 기존 `dgus_lcd.c` + `parse_lcd_comm()` 재사용 |
| Modbus 동시 모드 | ❌ Serial OR TCP 택일 (`comm_mode` 분기) | 기존 동작 유지 |
| 1ms 틱 소스 | TIM11 | RTC LSI보다 정확. SysTick은 HAL 점유 |
| job_state 단계 | 10단계 유지 | F410이 빨라 80us 제약 여유 |
| I2C 디지털 POT | 외부 칩 (Q6 확정) | I2C 마스터 그대로 사용 |
| 이전 4 GPIO 시그널 | 내부 함수/플래그로 대체 | 단일 MCU에서는 불필요. PB10–PB13은 다른 신호로 재할당 |
