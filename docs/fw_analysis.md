# FW Analysis — 참고용 (Legacy SAMD20 + ATmega16) 종합 분석

> **목적**: STM32F410RBT 단일 MCU로 통합 포팅하기 위한 레거시 펌웨어 구조 정리.
> **대상**: `ref/samd20/main.c` (≈145 KB), `ref/atmega16/m16_conv_v001.c` (≈53 KB), 그리고 그 보조 파일.
> `ref/`는 **읽기 전용**. 본 문서는 그 위에 만들어진 포팅 청사진이다.

---

## 1. 코드 토대

| 영역 | 파일 | 비고 |
|------|------|------|
| 메인 슈퍼루프 + LCD 명령 파서 | `ref/samd20/main.c` | god node `main()`(46 edges), `parse_lcd_comm()`(25 edges) |
| LCD 송수신 헬퍼 | `ref/samd20/dgus_lcd.c` | `pharse_lcd_comm()`은 구버전, 실제 사용본은 `main.c:3248` |
| Modbus RTU/TCP 디코더 | `ref/samd20/modbus.c` | `decode_comm()`(13 edges), 함수코드 0x01–0x06 |
| TCP 서버 / 콜백 | `ref/samd20/process_tcp.c`, `loopback.c` | W5500 SOCK_TCPS 단일 소켓 |
| W5500 드라이버 | `ref/samd20/W5500/`, `wizchip_conf.c`, `socket.c`, `dhcp.c` | ioLibrary, 거의 그대로 포팅 가능 |
| I2C ADC/POT | `ref/samd20/i2c_comm.c` | `i2c_adc_*` (외부 디지털 POT 추정 — Q6 미해결) |
| 사브 MCU(독립) | `ref/atmega16/m16_conv_v001.c` | ADC 평균화 + 21단계 룩업 + 7-seg + GPIO 응답 |
| 핀/보드 매크로 | `ref/samd20/ASF/common2/boards/user_board/user_board.h` | M_START / M_SEEK / M_RESET / M_OVLD 핀 정의 |

**graphify 결과**: 340 노드 / 851 엣지 / 11 커뮤니티. 산출물은 `graphify-out/graph.html`(인터랙티브), `GRAPH_REPORT.md`(요약), `graph.json`(재질의 가능).

---

## 2. 시스템 아키텍처 (이전 → 신규 비교)

```
[이전]
   ┌──────────── SAMD20 (메인) ────────────┐    GPIO 시그널    ┌── ATmega16 (서브) ──┐
   │ DGUS LCD (UART)                       │                    │ ADC 2채널 평균       │
   │ Modbus RTU/TCP                        │                    │ 21단계 룩업 → 출력  │
   │ W5500 Ethernet (SPI)                  │  M_START  ────────►│ PA4: 출력 개시       │
   │ I2C 디지털 POT (출력 레벨)             │  M_SEEK   ────────►│ PA5: 공진 탐색       │
   │ 5채널 OSC ON/OFF + TIM5 PWM (us_pwm)   │  M_RESET  ────────►│ PA6: 초기화          │
   │ I2C EEPROM(설정)                      │  M_OVLD   ◄────────│ PC0: 과부하 알람     │
   │ 슈퍼루프 + 10단계 job_state            │                    │ 7-seg 3-digit       │
   └────────────────────────────────────────┘                    └─────────────────────┘

[신규 — STM32F410RBT 단일 MCU]
   M_START / M_SEEK / M_RESET / M_OVLD = 물리 GPIO가 아닌 **내부 함수/상태 변수**.
   ATmega16의 ADC 평균화·룩업·7-seg(또는 LCD 흡수) 로직 = job_state 머신 내부에 흡수.
```

핵심 통합 결정:
- **MCU 간 4개 GPIO 신호 → 함수 호출/내부 플래그로 대체** (us_start(), us_seek(), us_reset(), ovld_flag).
- **7-세그먼트 디스플레이의 운명** = 미해결(Q4) — DGUS LCD가 모두 흡수하면 폐기, 보조 표시면 GPIO 출력 유지.
- **`I2C_POT`의 정체** = 미해결(Q6). 내부 흡수 가능한 ATmega16 인터페이스라면 코드 단순화 가능, 외부 칩이라면 I2C 마스터 그대로 사용.

---

## 3. SAMD20 슈퍼루프와 10단계 job_state 머신

### 3.1 1 ms 틱 소스 (이전: RTC 오버플로 콜백)

`main.c:538-554` `rtc_overflow_callback()`이 1 ms마다 호출되어 다음 카운터를 갱신한다.

```c
tick_1ms_state++;   // 매 1ms 토글로 사용 (& 0x01)
tick_1ms++;
base_cnt++;         // 200ms 누적용
sys_tick++;         // 10ms 단위 → tick_10ms_state, event_cnt
```

F410 포팅: `TIM11` 1 ms 인터럽트 또는 SysTick 콜백이 동일 역할 수행. 권장 = TIM11 (HAL이 SysTick 점유).

### 3.2 슈퍼루프 골격 (`main.c:5061-5310`)

```c
while (1) {
    if (tick_1ms_state != bak_1ms_state) {        // 1 ms 단위
        bak_1ms_state = tick_1ms_state;

        // (1) 통신 처리: comm_address != 0 일 때 Modbus RTU/TCP 디코드
        // (2) tick_1ms_state & 0x01 일 때만 job_state 한 칸 진행
        if (tick_1ms_state & 0x01) {
            switch (job_state) { /* 10 cases — 각 ~80us */ }
        }
        sys_tick_func();                          // 키 디바운스, 깜빡임
        if (my_adc_read_buffer_job(...) == OK) cal_real_val(); start_adc();
        // 200 ms 누적 (us_on_time_200m)
        // multi_ctrl / energy_ctrl / 시간초과 종료 판정
    }
}
```

핵심 관찰:
- `job_state`는 **2 ms마다 한 칸 진행** (`& 0x01` 게이트). 한 사이클 = 20 ms.
- 한 케이스가 ~80 µs 안에 끝나도록 설계 (SAMD20 @ 48 MHz). F410 @ 96 MHz면 여유 충분.
- ADC 읽기는 **job_state와 무관하게 매 1ms 폴링**.
- 200 ms 슬라이스(`base_cnt >= 200`)에서 ON-time 누적. F410도 동일 구조 유지 권장.

### 3.3 10단계 케이스 표 (`main.c:5110-5202`)

| state | 함수 | 역할 |
|-------|------|------|
| 0 | `check_remote_input()` + `send_outpower_data(0,…)` | Modbus 원격 명령 수신 처리 |
| 1 | `do_action()` + `send_outpower_data(1,…)` | LCD/스위치 입력→상태 전이 디스패치 |
| 2 | `if(check_lcd_comm(1)) parse_lcd_comm()` | LCD 패킷 1개 파싱 |
| 3 | `do_control()` + `send_outpower_data(3,…)` | 출력 제어 (M_START/M_RESET/M_SEEK 토글, 5채널 OSC 활성화) |
| 4 | `send_outpower_data(4,…)` | LCD에 출력 파워 전송 |
| 5 | `send_outtime_data(5, …)` | LCD에 ON-time 또는 last_time 전송 |
| 6 | `calc_freq()` + `send_outtime_data(6,…)` | 주파수 계산 |
| 7 | `send_val_data()` + `send_outtime_data(7,…)` | 전류/전압/전력 실시간 LCD 갱신 |
| 8 | `send_outtime_data(8,…)` | (실효 동일) 시간 전송 |
| 9 / default | `send_outtime_data(9,…)` + `job_state=0` + Modbus heartbeat | 한 사이클 종료, modbus_status 갱신 |

> `do_control()`(`main.c:1446-`)이 **us 출력 제어의 핵심 엔진** — 모든 GPIO 토글이 여기서 일어남. 포팅 시 가장 큰 함수가 됨.

---

## 4. ATmega16 서브 MCU 내부 동작

> 이전 시스템에서 ATmega16은 PA4/PA5/PA6 GPIO 입력만으로 SAMD20에서 명령을 받고, ADC 평균화·룩업·디스플레이를 독립 수행했다.

### 4.1 핀 사용 (analyzed `m16_conv_v001.c:1151-1164` + 사용자 확인)

> 초기화 값: `DDRA=0x00` / `PORTA=0xFC` (PA0/1 ADC 외 풀업), `DDRB=0xFF` / `DDRC=0xC3` / `PORTC=0xBC`, `DDRD=0xFF`.
> 분석 코드의 라벨("LED", "보조" 등)은 토글 동작에서의 추정. **실제 보드 신호 기능은 사용자 정보 우선** — 일부 라벨은 OSC 채널 구동으로 재해석됨.

| ATmega16 핀 | 초기 방향 | 분석 코드 라벨 | 실제 기능 (사용자 확인) |
|---|---|---|---|
| **PA0, PA1** | 입력 (analog) | ADC | ADC 채널 0/1 (센서, 발진기) |
| PA2~PA3 | 입력 풀업 | 보조 | 미사용 추정 |
| **PA4** | 입력 풀업 | M_START 입력 | **초음파 출력 개시** (M_START from SAMD20, active LOW) |
| **PA5** | 입력 풀업 | M_SEEK 입력 | **공진 주파수 탐색** (M_SEEK from SAMD20, active LOW) |
| **PA6** | 입력 풀업 | M_RESET 입력 | **초음파 초기화** (M_RESET from SAMD20, active LOW) |
| **PA7** | 입력 풀업 | (예비) | **OSC3 채널 구동** → F410 `CTRL_OSC3 = PB13` 흡수 |
| **PB0** | 출력 | "LED ON/OFF" (`m16_conv_v001.c:469-471`) | **OSC1 채널 구동** → F410 `CTRL_OSC1 = PB10` 흡수 |
| **PB1** | 출력 | "LED ON/OFF" (`m16_conv_v001.c:462-464`) | **OSC0 채널 구동** → F410 `CTRL_OSC0 = PB2` 흡수 |
| PB2~PB4 | 출력 | 7-seg digit select (`m16_conv_v001.c:817-840`) | 7-segment 폐기(Q4) → 미사용 |
| PB5~PB7 | 출력 | 7-seg/LED 보조 | 7-segment 폐기(Q4) → 미사용 |
| **PC0** | 출력 | M_OVLD 출력 (`m16_conv_v001.c:1285`) | **과부하 알람** (M_OVLD to SAMD20, 평상시 0) |
| PC1 | 출력 | (보조) | **무시** (Q2-잔, 신규 흡수 시 미사용) |
| PC2, PC3 | 입력 풀업 | 버튼 1, 2 (`PINC.2/3` 디바운스) | 키 입력 (F410 CON_KEY1/2로 대체) |
| **PC4** | 입력 풀업 | 외부 입력 | **OSC2 채널 구동** → F410 `CTRL_OSC2 = PB12` 흡수 |
| PC5 | 입력 풀업 | (예비) | 미사용 추정 |
| PC6 | 출력 | LED 패턴 보조(`m16_conv_v001.c:731`) | 7-segment 보조 → Q4 폐기로 미사용 |
| **PC7** | 출력 (초기 HIGH) | "LED ON/OFF" (`m16_conv_v001.c:455-457`) | **OSC4 채널 구동** → F410 `CTRL_OSC4 = PB14` 흡수 |
| PORTD (PD0~PD7) | 출력 | 7-seg 세그먼트 (`m16_conv_v001.c:870-871`) | 7-segment 폐기(Q4) → 미사용 |

**5채널 OSC 통합 매핑 (요약)**:

| F410 핀 | F410 Net | ← ATmega16 핀 | mega16 초기 방향 |
|---|---|---|---|
| PB2  | CTRL_OSC0 | PB1 | 출력 |
| PB10 | CTRL_OSC1 | PB0 | 출력 |
| PB12 | CTRL_OSC2 | PC4 | 입력 풀업 |
| PB13 | CTRL_OSC3 | PA7 | 입력 풀업 |
| PB14 | CTRL_OSC4 | PC7 | 출력 (초기 HIGH) |

### 4.2 ADC 평균화 (`m16_conv_v001.c:612-663`)

```c
void adc_process_channels(void) {
    if (adc_ch_idx == 0) {                       // 센서
        adc_ch0_sum_raw += raw;
        if (++adc_ch0_sample_cnt < 10) return;
        adc_ch0_avg = adc_ch0_sum_raw / 10;       // 10-sample
        adc_ch0_sum_raw = 0; adc_ch0_sample_cnt = 0;
    } else {                                      // 발진기
        adc_ch1_sum_raw += raw;
        if (++adc_ch1_sample_cnt < 50) return;
        adc_ch1_avg = adc_ch1_sum_raw / 50;       // 50-sample
        adc_ch1_sum_raw = 0; adc_ch1_sample_cnt = 0;
    }
}
```

→ F410: `fw/src/adc.c::adc_process_channels()` 그대로 흡수. PB0=센서(IN8), PB1=발진기(IN9).

### 4.3 21단계 룩업 출력 (`m16_conv_v001.c:881-981`)

`output_level_process()` + `lookup_table[24]`이 핵심. ADC 평균값(scaled) → 21단계 출력 레벨 결정.
F410에서 **이 테이블은 그대로 복사**, 호출은 `do_control()`의 `job_state==3`에서 발생.

### 4.4 버튼 디바운스 (`m16_conv_v001.c:322-415`)

타이머0 인터럽트(1 ms)에서 PINC.2 / PINC.3 상태를 읽고 `btn_scan_and_debounce()` + `btn_check_repeat()`로 처리. F410 포팅 대상은 CON_KEY1(PC12), CON_KEY2(PB11), CON_START(PA15), CON_RESET(PC10), CON_ESTOP(PC11).

---

## 5. **이전 MCU 간 GPIO 시그널 매핑 (확정)**

이번 세션에서 사용자가 회로 동작을 명확히 제공.

| 신호 | SAMD20 핀 | ATmega16 핀 | 활성 레벨 | 의미 |
|------|-----------|-------------|----------|------|
| `M_START` | **PB13 (out)** | PA4 (in) | LOW (`CTRL_INT_ON=0`) | 초음파 출력 개시 펄스 |
| `M_SEEK` | **PB11 (out)** | PA5 (in) | LOW | 공진 주파수 탐색 명령 |
| `M_RESET` | **PB12 (out)** | PA6 (in) | LOW | 초음파 초기화 |
| `M_OVLD` | **PB10 (in)** | PC0 (out) | (확인 필요, 추정 LOW) | 과부하 알람 보고 |

> 출처: `user_board.h:75-103` + 사용자 확인.
> SAMD20측 호출 위치: `main.c:1416,1552,1564,1665,1686,4185,5252,5276` (M_START), `main.c:4256,4262` (M_RESET), `main.c:1201` (M_OVLD).

### 5.1 F410 통합 시 변환 규칙

| 이전 (SAMD20→ATmega16 GPIO 토글) | 신규 (F410 단일) |
|---|---|
| `port_pin_set_output_level(M_START, CTRL_INT_ON)` | `us_start_request()` 함수 호출 → 내부 상태 머신 |
| `port_pin_set_output_level(M_RESET, CTRL_INT_ON)` | `us_reset_init()` 함수 호출 |
| (구) M_SEEK 펄스 | `us_freq_seek_start()` 함수 호출 |
| `re_ovld = !port_pin_get_input_level(M_OVLD)` | `g_overload_flag` 내부 플래그 (출력 단의 검출 로직이 직접 set) |

→ **물리 핀 PB10/PB11/PB12/PB13은 F410에서 다른 용도로 재할당됨**:
- PB10 = `CTRL_OSC1`
- PB11 = `CON_KEY2`
- PB12 = `CTRL_OSC2`
- PB13 = `CTRL_OSC3`

이는 충돌이 아니라 **물리 신호의 폐지** (내부 함수 호출로 대체) 결과.

---

## 6. DGUS LCD 통신 (`parse_lcd_comm()` — `main.c:3248`)

888 줄. LCD에서 보낸 8-byte 패킷(LCD_RD/LCD_WR + VP 주소 + 데이터)을 파싱.

### 패킷 구조

```
[LCD_COMM_MODE][LCD_COMM_N][LCD_COMM_ADDR_H][LCD_COMM_ADDR_L][LCD_COMM_DATA_H][LCD_COMM_DATA_L][...]
```

- `LCD_COMM_MODE` = `LCD_RD` 또는 `LCD_WR`.
- 16-bit VP 주소(`temp_com`)에 따라 분기 (`VP_KEY_CONF`, `VP_KEY_QUIT`, `VP_KEY_IIN_MIN/MAX`, `VP_KEY_226_*`, `VP_KEY_8_*`, `VP_KEY_5_*`, `VP_KEY_DELAY/WELD/HOLD`, `VP_FREQ`, `VP_MOD_*`, `VP_REM_*`, `VP_AMP_*` 등).

### 송신 헬퍼

`dgus_lcd.c`에서:
- `send_lcd_data_var(addr, val)` — 단일 16-bit 변수 (god node, 13 edges)
- `send_lcd_data_word(addr, val32)` — 32-bit 변수
- `send_lcd_byte_array(addr, len, *)` / `send_lcd_int_array(...)`
- `send_lcd_txt(addr, *str)`
- `change_lcd_page(page)` (god node, 13 edges)

→ F410 포팅: `fw/drivers/dgus_lcd/`에 송수신 헬퍼, `fw/src/dgus_lcd_parser.c`에 `parse_lcd_comm()` 분리. 가장 큰 함수이므로 VP 주소 → 핸들러 함수 포인터 테이블로 리팩터링 권장 (KISS — 일단 그대로 포팅 후 사이클 측정 결과에 따라).

---

## 7. Modbus RTU/TCP (`modbus.c::decode_comm`)

### 함수 코드 매핑 (`modbus.c:238-297`)

```
0x01 → read_coil()
0x02 → read_input_coil()
0x03 → read_reg()           ← holding 레지스터 읽기 (가장 빈번)
0x04 → read_input_reg()
0x05 → write_coil()
0x06 → write_reg()
```

### 동작

- `init_modbus(speed, parity, addr)` → USART(SAMD20 SERCOM) + 1.04 ms 타이머 콜백 설정.
- `usart_read_callback_modbus()`이 한 바이트씩 buffer에 누적.
- 슈퍼루프에서 `decode_comm(mode)` 호출 — `mode==0` (RTU, CRC 검사) / `mode!=0` (TCP, CRC 생략, 응답을 `send_tcps()`로).
- `update_holding_reg(0)` 또는 `update_holding_reg(1)`로 레지스터 동기화.
- `comm_mode == COMM_SERIAL` (0) → RTU만, `COMM_ETH_*` → TCP만 — **동시 미지원**.

→ F410 포팅: `fw/src/modbus.c`. USART2/3(미지정) + TIM6/7(미지정) 후보. holding 레지스터 정의는 그대로 유지.

---

## 8. W5500 / DHCP / TCP 서버

| 모듈 | 파일 | 포팅 비고 |
|------|------|----------|
| W5500 ioLibrary | `W5500/`, `wizchip_conf.c`, `socket.c`, `w5500.c` | WIZnet 공식 라이브러리. SPI bus 콜백만 F410 SPI1 HAL로 교체 |
| DHCP 클라이언트 | `dhcp.c` (god node `DHCP_run`) | 변경 거의 없음. `default_ip_assign()`/`default_ip_conflict()`/`default_ip_update()` 콜백 등록만 |
| TCP 서버 | `process_tcp.c` (`control_tcps`, `recv_tcps`, `send_tcps`) | SOCK_TCPS=0 단일 소켓, Modbus TCP 처리만 |
| 루프백 (디버그) | `loopback.c` | 통합 시 제거 검토 |

PHY 상태 점검: `check_PHYstatus()` — Modbus가 idle일 때만 호출 (`main.c:5106`). 슈퍼루프 cadence 유지.

---

## 9. 글로벌 상태 변수 인벤토리 (포팅 시 그대로 유지)

핵심 변수(`main.c:88-161`)를 카테고리별로 분류:

```c
// 시간/틱
volatile uint8_t tick_1ms, base_cnt, tick_1ms_state, tick_10ms_state,
                 sys_tick, key_tick, sys_tick_10m, event_cnt;

// 운전 상태
uint8_t sig_run_status, sig_reset_status, sig_seek_status, sig_ovld_status;
uint8_t us_run_status, us_reset_status, us_seek_status, us_on_status;
uint8_t bak_*;                 // 변경 검출용 백업

// 측정값
uint16_t curr_freq, curr_amp, curr_lv, curr_power, max_amp, max_power;
uint16_t last_freq, last_amp, last_power, last_lv, us_on_time, us_on_time_200m;
uint32_t curr_energy, acc_energy, last_energy, last_time, limit_energy;

// 모델/리미트 (EEPROM)
uint8_t model_freq, model_type, output_power, current_power;
uint16_t limit_min[N], limit_max[N];      // dgus 키별 (LIMIT_IIN, LIMIT_226, LIMIT_8, LIMIT_5)

// Modbus / Ethernet
uint8_t comm_address, comm_speed_idx, comm_parity_idx, comm_mode;
uint8_t modbus_status, modbus_comm_cnt;
uint8_t ether_ip[4], ether_nm[4], ether_gw[4];
```

→ F410: 상태를 모듈별로 분리 (`run_state.c/h`, `meas.c/h`, `config.c/h`, `comm.c/h`)하되, 한 번에 옮기지 말고 흡수 단계마다 점진적으로 이동.

---

## 10. 인터럽트 / 콜백 인벤토리 (이전 SAMD20)

| 트리거 | 콜백 | F410 등가 |
|--------|------|----------|
| RTC overflow (1 ms) | `rtc_overflow_callback` | TIM11 IRQ |
| ADC 완료 | `adc_complete_callback` | ADC1 EOC IRQ |
| USART_LCD RX | `usart_read_callback_LCD` | USART1 IRQ |
| USART_LCD TX 완료 | `usart_write_callback_LCD` | 동일 |
| USART_modbus RX/TX | `usart_*_callback_modbus` | USART2/3 (TBD) |
| USART_mon RX/TX | `usart_*_callback_mon` | USART6 |
| Modbus 1.04 ms 타이머 | `tc_callback_delay` | TIM6 또는 TIM7 |

→ HAL의 `HAL_*_IRQHandler` + `HAL_*Callback` 매크로로 1:1 매칭.

---

## 11. 공급 업체 코드 (벤더) 처리

- **WIZnet ioLibrary** (`W5500/`, `wizchip_conf.c`, `socket.c`, `dhcp.c`) — 라이선스 상 그대로 사용. `_DAT_IO_BASE_` SPI 콜백만 F410 SPI1 HAL 호출로 교체.
- **Atmel ASF** (`ASF/`) — F410으로 가면서 **전부 삭제**. STM32 HAL/LL/CMSIS로 대체.
- **사용자 보드 헤더** (`user_board.h`) — 핀 정의는 STM32CubeMX `.ioc` + 자동생성 `main.h`로 옮김. M_START/M_SEEK/M_RESET 매크로는 폐기 (내부 함수로 변환).

---

## 12. F410 포팅 시 주요 변경점 요약

| 항목 | 이전 | 신규 |
|------|------|------|
| 1 ms 틱 | RTC overflow | **TIM11 IRQ** (또는 SysTick) |
| ADC 트리거 | `my_adc_read_buffer_job()` 폴링 | HAL DMA + EOC 콜백 (그대로 폴링도 가능) |
| MCU 간 4신호 | GPIO PB10–PB13 | **내부 함수/플래그** (us_start/us_seek/us_reset/g_overload) |
| 7-seg 디스플레이 | ATmega16 PORTB/PORTD | **DGUS LCD 흡수**(권장) 또는 GPIO 출력 유지 — 회로도 확정 필요 |
| I2C 디지털 POT | `I2C_POT` 외부 칩 추정 | TBD — 회로도 확정 필요 |
| 슈퍼루프 cadence | 1 ms 게이트 + 2 ms job_state | 동일 유지 (마진 충분) |
| EEPROM | I2C FRAM/EEPROM (SAMD20) | I2C1 (PB6/PB7) — 동일 디바이스 재사용 |
| Modbus 동시 모드 | RTU XOR TCP | **유지 (단일 모드)** |
| 빌드 | Atmel Studio + ASF | **CMake + Ninja + arm-none-eabi-gcc + STM32 HAL** |

---

## 13. 결정 사항 / 잔여 미해결

### 13.1 결정 사항 (2026-04-26 확정)

| # | 결정 |
|---|------|
| Q1/PA4 | `M_START = PB13` (active LOW) — 초음파 출력 개시 |
| Q2/PC0 | `M_OVLD = PB10` — 과부하 알람 |
| (PA5)  | `M_SEEK = PB11` (active LOW) — 공진 주파수 탐색 |
| (PA6)  | `M_RESET = PB12` (active LOW) — 초음파 초기화 |
| Q2-잔/PC1 | **무시** — 신규 보드 흡수 시 미사용 |
| Q3/PC4 | **외부 입력 신호** (SAMD20 신호 아님). F410 통합 시 외부 신호로 직접 GPIO 할당 필요 시 추후 검토 |
| Q4 | 7-segment 디스플레이 = **삭제** (DGUS LCD가 모든 표시 흡수). ATmega16 `disp_led_pattern` / 7-seg PORTB·PORTD 출력 폐기 |
| Q5 | `disp_led_pattern` = **단순 표시 출력** → Q4 삭제 결정으로 같이 폐기 |
| Q6 | `I2C_POT` = **외부 디지털 포텐셔미터 칩**. 신규 보드도 I2C 마스터로 동일하게 제어 (`i2c_adc_*` 함수 그대로 포팅) |
| Q7 | PA0 = **FREQ_IN (출력 초음파 주파수 측정, TIM5_CH1 입력 캡처)** — SAMD20 PB15 흡수. "US_PWM_OUT(PWM 출력)" 오기 정정 (2026-06-20, `.ioc` 라벨 포함) |
| Q8 | `CTRL_OSC0~4` = **외부 발진회로 제어 신호 출력** 확정 |
| Q9 | TIM11 = **1 ms 시스템 틱** |

### 13.2 구현 시점 재확인 (회로도/실측 필요)

| # | 항목 | 재확인 시점 |
|---|------|----------|
| Q8b | CTRL_OSC0~4 활성 극성 (Active H/L) | Stage D `do_control()` 구현 시 회로도 + 실측 |
| Q10 | `.ioc`의 ADC1 IN9 (PB1) 채널 활성화 | Phase 1 CubeMX 보정 + Stage D ADC 흡수 시 |

---

## 14. 다음 단계 진입점

본 문서를 토대로 `docs/NEXT_STEPS.md`의 Phase 2 (CMake 빌드) 부터 진행 가능.
포팅 시 함수별 원본 위치는 `docs/NEXT_STEPS.md` "핵심 코드 참조 위치" 표 사용.

> **운영 원칙**: 한 번에 한 모듈만 흡수 → 빌드 → 동작 확인 → 다음 모듈. 그래픽 도구 `graphify-out/graph.html`로 이전 호출관계를 매번 추적할 것.
