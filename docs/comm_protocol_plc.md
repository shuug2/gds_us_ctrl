# 초음파 융착기 제어 프로토콜 (V3.0 / 260907)

> **문서 요약**: GD-SONIC 초음파 융착기 컨트롤러를 PLC·PC 에서 Modbus 로 제어하기 위한 사양.
> 통신 설정(RTU/TCP), 프레임 형식, 레지스터 주소표, 상태 비트, 제어 절차, 통신 예제, 케이블 연결을 담는다.
> 컨트롤러는 Modbus **슬레이브**, PLC 가 **마스터**다.

---

## 1. 통신 방식

Modbus RTU(RS-485) 와 Modbus TCP(Ethernet) 중 **하나**를 컨트롤러 LCD `SETUP → COMM` 에서 선택한다.

| 항목 | Modbus RTU | Modbus TCP |
|---|---|---|
| 물리층 | RS-485 2선식 (A / B) | Ethernet |
| 주소 | 슬레이브 주소 1 ~ 247 (LCD 설정, 0 = 통신 OFF) | IP 고정 또는 DHCP (LCD 설정), 포트 **502** |
| 통신 조건 | 2400 ~ 115200 bps, 패리티 EVEN / ODD / NONE, 데이터 8, 스톱 1 (LCD 설정) | Unit ID 는 검사하지 않음 (요청값 그대로 응답) |
| 출하 기본값 | **9600 bps / EVEN / 주소 1** | — |
| 연결 | 멀티드롭 가능 | **동시 1 연결.** 12 초간 요청이 없으면 컨트롤러가 연결을 끊고 다시 대기 |

공통 사항

- 지원 Function Code: **03 (Read Holding Registers)**, **06 (Write Single Register)**. FC 04 는 03 과 같은 값을 돌려준다. FC 16 (다중 쓰기) 미지원 → 쓰기는 한 레지스터씩.
- **오류 시 예외 응답을 보내지 않고 무응답**한다 (잘못된 주소·CRC·범위 밖·미지원 FC). PLC 는 타임아웃으로 처리하고 재시도한다.
- 한 번에 읽을 수 있는 레지스터는 최대 **51 개** (0x00 ~ 0x32). 시작주소 + 개수가 51 을 넘으면 무응답.
- 쓰기 응답은 요청을 그대로 되돌린다. 범위 밖 값은 조용히 보정(클램프)되므로 **쓴 뒤 다시 읽어 반영값을 확인**한다.

---

## 2. 프레임 형식

Modbus RTU

```
| Start ≥3.5 char | Address 1 B | Function 1 B | Data N B | CRC 2 B (Lo, Hi) | End ≥3.5 char |
```

Modbus TCP

```
| Transaction ID 2 B | Protocol ID 2 B | Length 2 B | Unit ID 1 B | Function 1 B | Data N B |
|<---------------------- MBAP Header ---------------------->|<------------ PDU ------------>|
```

1. Address / Unit ID — RTU: 1 ~ 247 (LCD 설정값). TCP: 임의 (1 권장), 응답에 그대로 에코.
2. Transaction ID — TCP: 마스터가 부여, 응답에 그대로 에코.
3. Protocol ID — TCP: 0 고정.
4. Length — TCP: Unit ID 부터 Data 끝까지의 바이트 수.
5. CRC — RTU: CRC-16 (다항식 0xA001, 초기값 0xFFFF), 하위 바이트 먼저.

### 03 (0x03) Read Holding Registers

| Request | 크기 | 값 |
|---|---|---|
| Function code | 1 B | 0x03 |
| Starting address | 2 B | 0x0000 ~ 0x0032 |
| Quantity of registers | 2 B | 1 ~ 51 |

| Response | 크기 | 값 |
|---|---|---|
| Function code | 1 B | 0x03 |
| Byte count | 1 B | 2 × N |
| Register values | N × 2 B | 상위 바이트 먼저 |

### 06 (0x06) Write Single Register

| Request / Response (동일) | 크기 | 값 |
|---|---|---|
| Function code | 1 B | 0x06 |
| Register address | 2 B | 0x0000 ~ 0x0032 |
| Register value | 2 B | 0x0000 ~ 0xFFFF |

---

## 3. 레지스터 주소표

R = 읽기 전용 (쓰기 무효) · R/W = 읽기/쓰기 (쓰면 즉시 저장, 전원 OFF 후에도 유지) · W = 명령 (쓰면 실행 후 0 으로 자동 소거).
시간 항목의 단위 10 ms 는 "값 ÷ 100 = 초" 다 (예: 123 → 1.23 s).

### 3.1 측정값 · 설정값 · 명령 · 상태 (0x00 ~ 0x1D)

| 주소 | R/W | 설명 | 범위 · 단위 |
|---|---|---|---|
| 0x00 | R | Work count 상위 16 bit | 32 bit = 0x00 × 65536 + 0x01 |
| 0x01 | R/W | Work count 하위 16 bit. **0 을 쓰면 카운터 리셋** | |
| 0x02 | R | 출력 Power | W |
| 0x03 | R | 출력 Amp. | 0.01 A (123 → 1.23 A) |
| 0x04 | R | 출력 Freq. | Hz (35038 → 35.038 kHz) |
| 0x05 | R | 출력 Energy | J |
| 0x06 | R/W | S/M/H : A.V. (출력 진폭) | % · 50 ~ 100 |
| 0x07 | R/W | H : Run time (Hand 모델 가동 시간 상한, 0 = 상한 없음) | 10 ms · 0 ~ 2000 |
| 0x08 | R/W | S/M/H : Energy (에너지 제어 목표) | J · 0 ~ 65535 |
| 0x09 | R/W | S/M/H : Limit time (에너지 미도달 시 Over time 판정 시간) | s · 0 ~ 10 |
| 0x0A | R/W | S (D) : Delay (D1) | 10 ms · 0 ~ 500 |
| 0x0B | R/W | S (D) : Weld (D2) | 10 ms · 0 ~ 500 |
| 0x0C | R/W | S (D) : Hold (D3) | 10 ms · 0 ~ 2000 |
| 0x0D | R/W | S (T) : Weld (T2) | 10 ms · 0 ~ 500 |
| 0x0E | R/W | S (T) : Hold (T3) | 10 ms · 0 ~ 2000 |
| 0x0F | R/W | S/M/H : Multi A.V. – T1 | 10 ms · 0 ~ 2000 |
| 0x10 | R/W | S/M/H : Multi A.V. – T2 | 10 ms · 0 ~ 2000 |
| 0x11 | R/W | S/M/H : Multi A.V. – A.V.1 | % · 50 ~ 100 |
| 0x12 | R/W | S/M/H : Multi A.V. – A.V.2 | % · 50 ~ 100 |
| 0x13 | R/W | Run mode (Standard) | 0 = Delay / 1 = Trigger |
| 0x14 | R/W | Enable Energy control | 0 = Disable / 1 = Enable |
| 0x15 | R/W | Enable Multi-out mode | 0 = Disable / 1 = Enable |
| 0x16 | R/W | Enable Safe mode | 0 = Disable / 1 = Enable |
| 0x17 | R/W | Model Frequency | 0 = 15K / 1 = 20K / 2 = 30K / 3 = 35K / 4 = 40K / 5 = 50K |
| 0x18 | R/W | Model Type (⚠ 가동 중·비상정지 중 변경 금지) | 0 = Hand / 1 = Multi / 2 = Standard |
| 0x19 | W | Reset (RESET 0.6 s → SEEK 0.6 s 자동 진행) | 1 = 실행 |
| 0x1A | W | Seek (0.6 s) | 1 = 실행 |
| 0x1B | W | RUN | **1 = 기동** / 2 = hold 기동 / 3 = hold 유지 (§4.2) |
| 0x1C | W | STOP | 1 = 정지 |
| 0x1D | R | STATUS (아래 비트표) | |

S = Standard, M = Multi, H = Hand 모델. 0x02 ~ 0x05 는 가동 중에는 그 런의 값, 정지 후에는 마지막 런의 값을 유지한다.

**STATUS (0x1D) 비트**

| 값 | 의미 |
|---|---|
| 0x0001 | Ultra sonic ON (가동 중) |
| 0x0002 | E-STOP (비상정지) |
| 0x0004 | Overload (과부하) |
| 0x0008 | Over time (에너지 미도달 시간 초과 — Reset 으로 해제) |
| 0x0020 | Sensor ON (하강 센서 감지 중) |
| 0x0040 | Horn-down 모드 (이 상태에서는 기동 불가) |
| 0x0080 | Seek 진행 중 |
| 0x0100 | Reset 진행 중 |

### 3.2 확장 영역 (0x1E ~ 0x32)

일반 제어에는 필요하지 않다. 상태 확인용 몇 개만 알면 된다.

| 주소 | R/W | 설명 | 값 |
|---|---|---|---|
| 0x1E ~ 0x20 | R/W | RTU 슬레이브 주소 / 속도 index / 패리티 index (원격 변경은 §5) | 1 ~ 247 / 0 ~ 5 (2400·4800·9600·19200·38400·115200) / 0 ~ 2 (EVEN·ODD·NONE) |
| 0x21 | R | 통신 모드 | 0 = RTU / 1 = TCP 고정 IP / 2 = TCP DHCP |
| 0x22 ~ 0x27 | R/W | IP · Netmask · Gateway (각 2 레지스터, 옥텟 2 개씩 상위 먼저) | 예: 192.168.1.10 → 0x22 = 0xC0A8, 0x23 = 0x010A |
| 0x28 | W | 통신 설정 적용 명령 | 1 = 적용 / 2 = 취소 |
| 0x29 | R | 통신 설정 적용 결과 | 0 대기 없음 / 1 적용 대기 / 2 성공 / 3 범위 오류 / 4 현재 사용 중인 링크 설정은 변경 불가 / 5 가동 중 / 6 30 초 초과로 취소 |
| 0x2A | R | 원격 제어 모델 식별 | 0x5201 = 원격 제어 모델 (REMOTE) |
| 0x2B | R | **원격 허용 상태** (REMOTE 모델) | 0 허용 스위치 OFF / **1 허용** / 3 통신 끊김 (10 s 이상 무통신) / 4 비상정지 중 |
| 0x2C ~ 0x2D | — | 예약 (사용 금지) | |
| 0x2E | R/W | 전류 보정값 (제조사 설정, 변경 비권장) | int16 · −1000 ~ 1000 |
| 0x2F | R/W | 주파수 보정값 (제조사 설정, 변경 비권장) | int16 · −1000 ~ 1000 |
| 0x30 | R/W | Horn-down 모드 | 0 = 해제 / 1 = 설정 (전원 OFF 시 해제) |
| 0x31 | R | 펌웨어 확인 | 0xFA01 = 이 문서(V3.0)의 주소표 전체 유효. 다른 값 = 구형 펌웨어, 0x00 ~ 0x1D 만 유효 |
| 0x32 | R | 기능 확인 | bit0 = 1 → hold 기동(§4.2) 지원 |

---

## 4. 제어 절차

### 4.1 기동 · 정지

1. **STATUS(0x1D) 를 읽어** 0x0001(가동 중) · 0x0002 · 0x0004 · 0x0040 · 0x0080 · 0x0100 이 모두 0 인지 확인한다.
2. 0x1B 에 **1** 을 쓴다. 약 60 ms 후 STATUS 0x0001 이 1 이 된다.
3. 정지는 0x1C 에 **1** 을 쓴다. 다음 조건에서는 컨트롤러가 스스로 정지한다: Hand 모델 Run time(0x07) 도달, 에너지 목표(0x08) 도달, Over time, **30 초 안전 상한**, 비상정지, 과부하.
4. 기동 명령이 거부되면 응답은 정상이지만 STATUS 가 변하지 않는다. 원인은 STATUS 비트로 확인한다 (비상정지 · 과부하 · Horn-down · Seek/Reset 진행 중 · 이미 가동 중 · 전원 투입 후 약 4 초 예열).

### 4.2 hold 기동 (선택 · TCP 권장)

버튼을 누르고 있는 동안만 가동하려면 0x1B 에 **2** 를 써서 기동한 뒤, **150 ms 주기**로 0x1B 에 **3** 을 계속 쓴다.
유지 신호가 **0.6 초** 끊기면 컨트롤러가 자동 정지한다. 3 은 기동 명령이 아니므로 정지 후 다시 기동하려면 2 를 쓴다.
0x32 bit0 이 0 인 컨트롤러(구형)에서는 2 · 3 을 써도 아무 동작을 하지 않는다. RTU 9600 bps 에서는 주기를 맞추기 어려우므로 TCP 에서 사용한다.

### 4.3 Reset · Seek

0x19 또는 0x1A 에 1 을 쓴다. 가동 중에는 무시된다. 진행 상태는 STATUS 0x0100(Reset) → 0x0080(Seek) 으로 확인한다. 진행 중에는 기동 명령이 무시된다.

### 4.4 파라미터 변경

- FC06 으로 한 레지스터씩 쓴다. 범위 밖 값은 자동 보정되므로 **쓴 뒤 읽어서 확인**한다.
- 쓰는 즉시 컨트롤러에 저장된다 (전원 OFF 후에도 유지).
- 작업 카운터 리셋 = 0x01 에 0 쓰기.
- LCD 에서 조작자가 편집 중인 값도 그대로 읽힌다.

### 4.5 원격 허용 (REMOTE 모델)

기계 측 **원격 허용 스위치**가 ON 이어야 기동·Reset·Seek·파라미터 쓰기가 받아들여진다. 스위치 OFF · 비상정지 · 10 초 이상 무통신이면 이 명령들은 무시된다 (응답은 정상). **STOP 과 읽기는 항상 가능**하다.
상태는 0x2B 로 확인한다. 10 초 이상 통신이 끊긴 뒤 보내는 첫 명령은 무시될 수 있으므로 **STATUS 를 1 초 이하 주기로 계속 읽어** 통신을 유지하고, 재접속 후에는 읽기 한 번 뒤에 명령을 보낸다.

### 4.6 권장 폴링

STATUS 와 측정값은 0x00 부터 30 개(0x00 ~ 0x1D)를 FC03 한 번으로 **100 ms ~ 1 s 주기**로 읽는다. 원격 허용 상태까지 보려면 44 개(0x00 ~ 0x2B).

---

## 5. 통신 설정 원격 변경 (선택)

지금 쓰고 있는 링크의 설정은 바꿀 수 없다 — RTU 설정(0x1E ~ 0x20)은 TCP 로 접속해서, IP 설정(0x22 ~ 0x27)은 RTU 로 접속해서 바꾼다. 통신 모드(0x21) 전환은 LCD 에서만 가능하다.

1. 바꿀 레지스터에 값을 쓴다 (여러 개 가능, 아직 적용되지 않음. 0x29 = 1).
2. 30 초 안에 0x28 에 1 을 쓴다.
3. 0x29 를 읽어 2(성공)인지 확인한다. 3 ~ 5 는 거부 사유. 가동 중에는 거부된다.
4. RTU 설정은 다음에 LCD 에서 통신 모드를 RTU 로 전환할 때 적용된다. IP 설정은 즉시 적용된다.

---

## 6. 통신 예제 (슬레이브 주소 1)

### 6.1 Work count 읽기

Modbus RTU

| Request | Hex | | Response | Hex | |
|---|---|---|---|---|---|
| Slave addr. | 01 | | Slave addr. | 01 | |
| Function | 03 | | Function | 03 | |
| Start addr. Hi | 00 | Reg. 0x00 | Byte count | 04 | |
| Start addr. Lo | 00 | | Reg. value Hi | xx | Count high |
| No. of Reg. Hi | 00 | | Reg. value Lo | xx | |
| No. of Reg. Lo | 02 | 2 개 | Reg. value Hi | xx | Count low |
| CRC Lo | C4 | | Reg. value Lo | xx | |
| CRC Hi | 0B | | CRC Lo / Hi | xx xx | |

Modbus TCP

| Request | Hex | | Response | Hex | |
|---|---|---|---|---|---|
| Transaction ID | 00 01 | | Transaction ID | 00 01 | |
| Protocol ID | 00 00 | | Protocol ID | 00 00 | |
| Length | 00 06 | | Length | 00 07 | |
| Unit ID | 01 | | Unit ID | 01 | |
| Function | 03 | | Function | 03 | |
| Start addr. | 00 00 | Reg. 0x00 | Byte count | 04 | |
| No. of Reg. | 00 02 | 2 개 | Reg. values | xx xx xx xx | Count high, low |

### 6.2 초음파 기동 (RUN)

| | RTU | TCP |
|---|---|---|
| Request | `01 06 00 1B 00 01 38 0D` | `00 02 00 00 00 06 01 06 00 1B 00 01` |
| Response | `01 06 00 1B 00 01 38 0D` (에코) | `00 02 00 00 00 06 01 06 00 1B 00 01` (에코) |

### 6.3 정지 (STOP) · STATUS 읽기

| 동작 | RTU Request | RTU Response |
|---|---|---|
| STOP | `01 06 00 1C 00 01 89 CC` | 에코 |
| STATUS 읽기 | `01 03 00 1D 00 01 14 0C` | `01 03 02 SS SS cc cc` (SS SS = 상태 비트) |

### 6.4 hold 기동

| 순서 | RTU Request |
|---|---|
| 기동 | `01 06 00 1B 00 02 78 0C` |
| 유지 (150 ms 마다) | `01 06 00 1B 00 03 B9 CC` |
| 정지 | 유지 신호 중단 (0.6 s 후 자동 정지) 또는 `01 06 00 1C 00 01 89 CC` |

---

## 7. 케이블 연결

| 신호명 | 구분 | 색깔 | Pin No | |
|---|---|---|---|---|
| Start | 입력 | 청색 | 19 | |
| | | 백색 | 23 | GND |
| Reset | 입력 | 녹색 | 15 | |
| | | 주황 | 22 | GND |
| Seek | 입력 | 노랑 | 14 | |
| | | 보라 | 21 | GND |
| Overload | NPN 오픈컬렉터 출력 | 흑색 | 12 | |
| | | 적색 | 11 | GND |
| U.S. Output | NPN 오픈컬렉터 출력 | 회색 | 8 | |
| | | 갈색 | 7 | GND |
| RS485-COM | | 핑크 | 24 | TRX+ (A) |
| | | 하늘색 | 10 | TRX− (B) |

※ Standard 모델에서는 Seek 입력이 비상정지(E-STOP) 입력으로 동작한다.

※ 핀 번호·색상은 V2.0 문서 기준 — 현행 PCB 커넥터로 확정 후 갱신.

참고 규격: *Modbus over serial line V1.02* (2006), *Modbus Application Protocol V1.1b3* (2012)
