# GD-SONIC 초음파 융착기 제어 통신 프로토콜 (V3.0 / 2026-09-06)

> **문서 요약**: STM32F410 통합 컨트롤러(펌웨어 3.x)의 외부 통신 사양. **Modbus RTU(RS-485) 또는
> Modbus TCP(Ethernet) 택일**, 슬레이브 동작, FC 01/02/03/04/05/06 지원(FC16 미지원), **예외 응답
> 없음(오류 = 무응답)**. holding 레지스터 **51개(0x00~0x32)** 의 주소·R/W·단위·클램프를 정의하고,
> V2.0 이후 신설된 **명령 START 값 확장(탭/hold/유지)**, **STATUS 비트 5~8**, **통신 설정 staging+commit
> (0x1E~0x29)**, **원격 게이트(0x2A~0x2B, REMOTE 모델)**, **보정(0x2E~0x2F)**, **horn 모드(0x30)**,
> **capability 판별(0x31~0x32)** 의 동작 규칙과 예제 프레임을 담는다. 이전 문서
> `ref/통신프로토콜_GDSONIC_V20_250703.pdf`(V2.0, SAMD20 세대)를 기준으로 작성했고 차이는 부록 A 에 모았다.

---

## 0. 문서 정보

| 항목 | 내용 |
|---|---|
| 문서 버전 | **V3.0** (2026-09-06). 펌웨어 3.x 계열에 대응 |
| 대상 펌웨어 | STD `V3.0.0_260906` / REMOTE `V3.1.0R_260906` (태그 `hw-revA_fw-stage-hold-wdt` 이후) |
| 이전 문서 | `ref/통신프로토콜_GDSONIC_V20_250703.pdf` — V2.0/250705, SAMD20+ATmega16 세대 |
| 정본(코드) | 주소·비트·매직 = `fw/include/app_modbus_core.h`, 의미·클램프 = `fw/src/app_modbus.c`. **문서와 코드가 갈리면 코드가 이긴다** |
| 소비자 | 원격기 `gds_us_remote`, PC HMI `gds_us_hmi`, 고객 PLC/PC |
| 표기 | 레지스터 값은 16비트 부호 없는 정수(big-endian). 별도 표기(int16)가 있는 칸만 2의 보수 |

---

## 1. 통신 방식

컨트롤러는 **Modbus 슬레이브**다. RTU 와 TCP 는 **택일** — LCD `SETUP → COMM` 의 `comm_mode` 가 정한다
(0 = SERIAL / 1 = ETH_STATIC / 2 = ETH_DHCP). 원격에서는 이 값을 읽을 수만 있다(0x21).

### 1.1 Modbus RTU (RS-485)

| 항목 | 값 |
|---|---|
| 물리층 | RS-485 2선식(A/B), 반이중 |
| 슬레이브 주소 | **1 ~ 247**. `0` = 통신 비활성(RTU 포트를 열지 않음). 브로드캐스트(주소 0) 미지원 |
| 보드레이트 | 2400 / 4800 / 9600 / 19200 / 38400 / 115200 (index 0~5) |
| 패리티 | EVEN / ODD / NONE (index 0~2). 데이터 8비트, 스톱 1비트 |
| 공장 초기값 (FRAM 초기화 시) | 주소 **0(비활성)** / 2400 / EVEN / SERIAL → **LCD 에서 설정해야 통신이 열린다** |
| 출하·벤치 관례 | 주소 1 / **9600** / EVEN |
| 프레임 경계 (수신 유휴) | 2400 = 14 ms, 4800 = 7 ms, 9600 = 4 ms, 19200 이상 = 2 ms |
| 요청 최소 길이 | 8바이트(주소 + FC + 4 + CRC). 미달·주소 불일치·CRC 오류 = 무응답 |

> ⚠ V2.0 문서의 초기값(19200 / 8E1)과 다르다. 접속 전 LCD COMM 페이지 또는 0x1E~0x20 읽기로 실제 값을 확인할 것.
> ⚠ `parity index 0` 은 NONE 이 아니라 **EVEN** 이다.

### 1.2 Modbus TCP (Ethernet)

| 항목 | 값 |
|---|---|
| 칩 | W5500 (SPI) |
| 포트 | **502** |
| 동시 연결 | **1**. 연결 중 다른 클라이언트의 접속은 거부(RST) |
| Unit ID | 검사하지 않는다. 요청 값을 그대로 에코 |
| Protocol ID | **0** 이어야 한다. 0 이 아니면 수신 버퍼 전체 폐기(재동기) |
| 유휴 타임아웃 | **12 s** 동안 유효 요청이 없으면 컨트롤러가 연결을 끊고 재대기(listen) |
| Keep-alive | W5500 자동 KA 10 s(데이터 1회 이상 교환 후부터 동작) |
| IP 설정 | static 또는 DHCP(LCD 에서 선택). DHCP 리스 IP 는 LCD 와 0x22/0x23 에 미러된다 |
| 처리량 | poll 당 최대 4 프레임. **쓰기(FC06)는 poll 당 1건** — 연속 FC06 은 다음 poll 로 이월(수 ms) |

> ⚠ TCP 연결 성공(SYN/ACK)은 MCU 생존을 뜻하지 않는다 — W5500 이 자체 응답한다. **헬스체크는 FC03 트랜잭션 성공**으로 판정할 것.

### 1.3 공통 규칙

| 항목 | 내용 |
|---|---|
| 지원 Function Code | **01, 02, 03, 04, 05, 06**. FC16(다중 쓰기) 미지원 → 쓰기는 FC06 하나씩 |
| FC04 | FC03 과 **같은 holding 테이블**을 돌려준다(FC 만 에코) |
| FC01/02/05 | 코일 50개. **기능 매핑 없음**(읽고 쓸 수 있는 스크래치). 사용하지 말 것 |
| 예외 응답 | **보내지 않는다.** 잘못된 주소·CRC·미지원 FC·범위 밖(주소+수량 > 51, 수량 0) = **무응답**. 마스터는 타임아웃으로 처리 |
| 레지스터 수 | **51개**(0x00~0x32). FC03 은 한 프레임에 51칸까지(RTU 응답 107 B). **구 펌웨어(50칸)는 51칸 요청에 무응답** → §4.6 판별 후 폴 폭 결정 |
| 쓰기 확인 | **read-back 이 유일한 진실**. 클램프·거부·게이트 차단은 전부 무음이고 FC06 응답은 요청 에코다 |

---

## 2. 프레임 형식

### 2.1 Modbus RTU ADU

```
| Start ≥3.5 char | Address 1B | Function 1B | Data N×1B | CRC16 2B (Lo, Hi) | End ≥3.5 char |
```

CRC16: 다항식 0xA001(reflected 0x8005), 초기값 0xFFFF, **하위 바이트 먼저** 전송.

### 2.2 Modbus TCP ADU

```
| Transaction ID 2B | Protocol ID 2B (=0) | Length 2B | Unit ID 1B | Function 1B | Data N×1B |
|<------------------ MBAP Header ------------------>|<-------------- PDU -------------->|
```

Length = Unit ID 부터 Data 끝까지의 바이트 수. Transaction ID 는 응답에 그대로 에코.

### 2.3 FC03 (0x03) Read Holding Registers

| 필드 | 크기 | 값 |
|---|---|---|
| Function code | 1 B | 0x03 |
| Starting address | 2 B | 0x0000 ~ 0x0032 |
| Quantity | 2 B | 1 ~ 51 (start + quantity ≤ 51) |

응답: `0x03` / Byte count(= 2×N) / Register values N×2 B (big-endian).

### 2.4 FC06 (0x06) Write Single Register

| 필드 | 크기 | 값 |
|---|---|---|
| Function code | 1 B | 0x06 |
| Register address | 2 B | 0x0000 ~ 0x0032 |
| Register value | 2 B | 0x0000 ~ 0xFFFF |

응답: 요청 에코(주소·값 그대로). **클램프 전 값이 에코된다** — 실제 반영값은 다음 FC03 으로 확인.

---

## 3. 레지스터 맵

R = 읽기(미러가 매 tick 덮는다, 쓰기 무효) · R/W = 읽기/쓰기(FRAM 영속) · W(cmd) = 명령(쓰면 소비 후 0 으로 소거) ·
staged = 쓰면 staging 버퍼에만 저장, CFG_CTRL 커밋으로 일괄 반영(§4.5).

### 3.1 측정 표시 (0x00 ~ 0x05)

| 주소 | 이름 | R/W | 내용 | 단위 · 범위 |
|---|---|---|---|---|
| 0x00 | WORK_CNTH | R | 작업 카운트 상위 16비트 | u32 = (H << 16) \| L |
| 0x01 | WORK_CNTL | R / 특수 | 작업 카운트 하위 16비트. **0 쓰기 = 카운터 리셋**(FRAM 포함). 그 외 값 쓰기는 무시 | |
| 0x02 | DISP_POWER | R | 표시 출력 전력. 가동 중 = 그 런의 피크, 정지 = 마지막 런 래치 | W |
| 0x03 | DISP_AMP | R | 표시 전류. 피크/래치 규칙 동일 | 0.01 A (123 → 1.23 A) |
| 0x04 | DISP_FREQ | R | 출력 주파수. 가동 중 = 현재값, 정지 = 마지막 값 | **Hz** (35038 → 35.038 kHz) |
| 0x05 | DISP_ENERGY | R | 표시 에너지. 가동 중 = 누산 중, 정지 = 마지막 런 래치 | J, u16(65535 초과 시 하위 16비트) |

> "가동 중" = 초음파 런 또는 SEEK/RESET 진행 중. 정지 직후 값이 래치되므로 사이클 결과 캡처는 STATUS bit0 하강 후 읽으면 된다.

### 3.2 설정 (0x06 ~ 0x18)

| 주소 | 이름 | R/W | 내용 | 단위 · 쓰기 클램프 |
|---|---|---|---|---|
| 0x06 | OUT_POWER | R/W | 출력 진폭(A.V.) | % · **50 ~ 100** |
| 0x07 | ON_TIME | R/W | **H**(Hand 모델) 가동 시간 상한. 0 = 운영 상한 없음(30 s 안전 상한만 적용) | 10 ms · **0 ~ 2000** (0 ~ 20.00 s) |
| 0x08 | ENERGY | R/W | S/M/H 에너지 목표(에너지 제어 ON 시 종료 조건) | J · 0 ~ 65535 (cfg 는 u32, 65535 초과 저장값은 하위 16비트만 보임) |
| 0x09 | TIMEOVER | R/W | 에너지 제어 시간 초과 한계. 이 시간 안에 ENERGY 미도달 → OVTIME fault. ⚠ **0 = 즉시 OVTIME**(off 아님) | s · **0 ~ 10** |
| 0x0A | DELAY1 | R/W | S(Standard) DELAY 모드 D1 (하강 지연) | 10 ms · **0 ~ 500** |
| 0x0B | DELAY2 | R/W | S DELAY 모드 D2 (weld) | 10 ms · **0 ~ 500** |
| 0x0C | DELAY3 | R/W | S DELAY 모드 D3 (hold) | 10 ms · **0 ~ 2000** |
| 0x0D | TRIGGER2 | R/W | S TRIGGER 모드 T2 (weld) | 10 ms · **0 ~ 500** |
| 0x0E | TRIGGER3 | R/W | S TRIGGER 모드 T3 (hold) | 10 ms · **0 ~ 2000** |
| 0x0F | MULTI_T1 | R/W | Multi A.V. 1단 시간 T1 | 10 ms · **0 ~ 2000** |
| 0x10 | MULTI_T2 | R/W | Multi A.V. 2단 시간 T2 | 10 ms · **0 ~ 2000** |
| 0x11 | MULTI_O1 | R/W | Multi A.V. 1단 진폭 A.V.1 | % · **50 ~ 100** (쓰기 클램프. 공장 초기값 25 는 클램프 밖이라 읽기값이 범위 밖일 수 있다) |
| 0x12 | MULTI_O2 | R/W | Multi A.V. 2단 진폭 A.V.2 | % · **50 ~ 100** (동일) |
| 0x13 | RUN_MODE | R/W | Standard 동작 모드 | **0 = DELAY / 1 = TRIGGER** (클램프 없음, as-is 저장) |
| 0x14 | EN_ENERGY | R/W | 에너지 제어 | 0 = Disable / 1 = Enable (그 외 값 무시) |
| 0x15 | EN_MULTI | R/W | Multi 출력 모드 | 0 / 1 (그 외 값 무시) |
| 0x16 | EN_SAFTY | R/W | 안전 모드(양손 동시 눌림 판정) | 0 / 1 (**1 이외는 0 으로 정규화**) |
| 0x17 | MODEL_FREQ | **R/W** | 모델 주파수 | 0 = 15K / 1 = 20K / 2 = 30K / 3 = 35K / 4 = 40K / 5 = 50K (클램프 없음, 범위 밖은 15K 표기로 퇴화) |
| 0x18 | MODEL_TYPE | **R/W** | 모델 타입 | 0 = Hand / 1 = Multi / 2 = Standard (클램프 없음) |

> 🔴 **MODEL_TYPE(0x18) 쓰기는 외부 입력 PC11 의 역할을 바꾼다** — Hand/Multi 에서는 SEEK 버튼, Standard 에서는 E-STOP 입력이다.
> 0/1 → 2 로 바꾸는 순간 PC11 이 HIGH 면 즉시 E-STOP, 2 → 0/1 로 바꾸면 살아 있는 E-STOP 이 해제된다. 컨트롤러는 거부하지 않는다
> (LCD 경로와 동일). **가동 중·E-STOP 중에는 쓰지 말 것.**
> ⚠ LCD 에서 편집 중(SAVE 전)인 값도 미러에 실린다. 조작자가 CANCEL 을 누르면 값이 되돌아간다.

### 3.3 명령 (0x19 ~ 0x1C)

명령 레지스터는 미러 대상이 아니다. 쓰면 그 tick 에 소비되고 **0 으로 소거**된다(읽으면 보통 0).

| 주소 | 이름 | R/W | 내용 |
|---|---|---|---|
| 0x19 | RESET | W(cmd) | `1` = RESET. **RESET 600 ms → SEEK 600 ms 자동 체인**. 가동 중이면 무시 |
| 0x1A | SEEK | W(cmd) | `1` = SEEK 단발 600 ms. 가동 중이면 무시 |
| 0x1B | START | W(cmd) | **`1` = 탭 기동**(STOP·상한까지 가동) / **`2` = hold 기동**(hold 워치독 무장, §4.3) / **`3` = 유지 신호**(기동 권한 없음) / 그 외 무시. **값 불문 소거** |
| 0x1C | STOP | W(cmd) | `1` = 정지. **기동 주체와 무관**(패널에서 기동한 런도 세운다). 값 불문 소거 |

- 기동 거부(가동 중·SEEK/RESET 중·E-STOP·과부하·horn 모드·부팅 warm-up 약 4 s)는 **전부 무음**. STATUS 로 원인을 읽는다.
- START 후 STATUS bit0 반영까지 약 60 ms.
- START(0x1B)에 0 을 쓰는 것은 정지가 아니다. 정지는 반드시 STOP = 1.

### 3.4 상태 (0x1D)

| 주소 | 이름 | R/W | 내용 |
|---|---|---|---|
| 0x1D | STATUS | R | 상태 비트맵(아래) |

| 비트 | 값 | 이름 | 의미 |
|---|---|---|---|
| 0 | 0x0001 | US | 초음파 가동 중 |
| 1 | 0x0002 | ESTOP | 비상정지 활성 |
| 2 | 0x0004 | OVLD | 과부하 |
| 3 | 0x0008 | OVTIME | 에너지 제어 시간 초과 fault. RESET 으로 해제 |
| 4 | 0x0010 | OUTERR | 예약(항상 0) |
| 5 | 0x0020 | SENSOR | 하강 센서(SENSE_DN) 감지 중 — Standard TRIGGER RUN 의 "SENSOR ON" |
| 6 | 0x0040 | HORN | horn-down 모드. 서 있으면 모든 START 차단 + 진행 런 정지 |
| 7 | 0x0080 | SEEK | SEEK 진행 중(단발 SEEK · RESET 체인 후반) |
| 8 | 0x0100 | RESET | RESET 진행 중(체인 전반). bit7 과 동시에 서지 않는다 |
| 9~15 | — | — | 예약(0) |

> ⚠ 비트 5~8 은 V3.0 신설. 구 펌웨어에서는 영영 0 이므로 **0 을 "미지원"과 구분할 수 없다** — 0x31 매직(§4.6)으로 세대를 판별한 뒤 표시하고, 특히 bit7/8 의 0 을 "미진행" 근거로 삼는 로직은 만들지 말 것(1 만 정보다).

### 3.5 통신 설정 staging (0x1E ~ 0x29) — §4.5

| 주소 | 이름 | R/W | 내용 · 인코딩 | 커밋 검증 |
|---|---|---|---|---|
| 0x1E | COMM_ADDR | R/W staged | RTU 슬레이브 주소 | **1 ~ 247** 외 거부 |
| 0x1F | COMM_SPEED | R/W staged | 보드레이트 index 0~5 (2400/4800/9600/19200/38400/115200) | > 5 거부 |
| 0x20 | COMM_PARITY | R/W staged | 패리티 index 0~2 (EVEN/ODD/NONE) | > 2 거부 |
| 0x21 | COMM_MODE | **R** | 0 = SERIAL / 1 = ETH_STATIC / 2 = ETH_DHCP. **원격 쓰기 불가**(LCD 전용) | — |
| 0x22 | ETHER_IP_H | R/W staged | IP[0] << 8 \| IP[1] | 없음 |
| 0x23 | ETHER_IP_L | R/W staged | IP[2] << 8 \| IP[3] | 없음 |
| 0x24 / 0x25 | ETHER_NM_H / L | R/W staged | 넷마스크, 같은 인코딩 | 없음 |
| 0x26 / 0x27 | ETHER_GW_H / L | R/W staged | 게이트웨이, 같은 인코딩 | 없음 |
| 0x28 | CFG_CTRL | W(cmd) | `1` = COMMIT / `2` = DISCARD. 값 불문 소거 | — |
| 0x29 | CFG_STAT | R | staging 상태 코드(아래) | — |

| CFG_STAT | 이름 | 의미 |
|---|---|---|
| 0 | IDLE | staged 값 없음 |
| 1 | STAGED | 커밋 대기 중인 값 있음 |
| 2 | COMMIT_OK | 마지막 커밋 성공 |
| 3 | REJ_RANGE | 범위 밖 값이 있어 거부 |
| 4 | REJ_SAME_LINK | 지금 쓰고 있는 링크 자신의 설정을 커밋하려 해 거부 |
| 5 | REJ_RUNNING | 가동(런·SEEK/RESET) 중이라 거부 |
| 6 | REJ_TIMEOUT | 마지막 staged 쓰기 후 30 s 경과로 폐기 |

거부 사유는 다음 staged 쓰기까지 유지된다. 게이트(§4.4) 거부는 여기 반영되지 않는다(0x2B 로 판독).

### 3.6 원격 게이트 (0x2A ~ 0x2D) — REMOTE 모델 전용, §4.4

| 주소 | 이름 | R/W | 내용 |
|---|---|---|---|
| 0x2A | REMOTE_CAP | R | **REMOTE 모델: 매직 `0x5201` 상시 복원.** STD 모델: 미러 없음(쓴 값이 잔류) → 게이트 없음의 표식 |
| 0x2B | REMOTE_EN | R | 게이트 상태(아래). STD 모델은 0 |
| 0x2C | — | — | 결번(구 "잔여 초"). REMOTE 는 항상 0. 사용 금지 |
| 0x2D | — | — | 예약. 미러 없음. **쓰지 말 것** |

| REMOTE_EN | 이름 | 의미 |
|---|---|---|
| 0 | DISABLED | 원격 허용 스위치(PC8) OFF 또는 부팅 직후 |
| 1 | ENABLED | 원격 명령 허용 |
| 2 | — | 결번 |
| 3 | DIS_LINK | 통신 침묵 ≥ 10 s 로 해제. 요청이 다시 오면 자동 복귀 |
| 4 | DIS_ESTOP | E-STOP 활성 중. 해제되면 자동 복귀 |
| 5 | — | 결번 |

상태는 래치되지 않는다 — 사유가 사라지면 스스로 ENABLED 로 돌아간다.

### 3.7 보정 · horn · capability (0x2E ~ 0x32)

| 주소 | 이름 | R/W | 내용 | 단위 · 클램프 |
|---|---|---|---|---|
| 0x2E | CAL_VAL | R/W | 전류 보정값. ⚠ 표시 보정이 아니라 **제어 루프 입력** | **int16 2의 보수** · −1000 ~ 1000. LCD 는 ÷100 으로 표기(−16 → −0.16) |
| 0x2F | FREQ_CAL_VAL | R/W | 주파수 표시 보정값 | **int16 2의 보수** · −1000 ~ 1000 |
| 0x30 | HORN_CMD | R/W | `1` = horn-down 모드 ON. **비영속**(재부팅 시 0). 모드 전이 시 솔레노이드 무조건 OFF. ON 중 모든 START 차단, 진행 런 정지 | 0 / 1 (0 이외 = ON) |
| 0x31 | CFG_CAP | R | 매직 **`0xFA01`** = 확장 레지스터(0x1E 이후) 지원 펌웨어. 모델 무관 | |
| 0x32 | FEAT_CAP | R | 기능 비트맵. **bit0 (0x0001) = hold 워치독 지원**. 나머지 비트 0 = 미지원. 모델 무관 | |

---

## 4. 동작 규칙

### 4.1 쓰기 공통

1. **한 FC06 = 한 변경.** 우선순위: 명령(RESET → SEEK → START → STOP → CFG_CTRL) → 값이 달라진 설정 1개 → staged 쓰기.
2. 클램프·정규화·거부는 **무음**이고 FC06 응답은 요청 에코다. **쓰기 후 반드시 read-back** 으로 반영값을 확인한다.
3. 설정 쓰기는 즉시 FRAM 에 영속된다(전맵 저장, 약 2 ms, I2C 최악 50 ms).
4. 같은 값 재쓰기는 아무 일도 하지 않는다. 미러 대상(R) 레지스터에 쓴 값은 다음 tick 에 원값으로 덮인다.
5. 명령 레지스터는 값 불문 소거된다. 단 **구 펌웨어에서는 START 2/3 이 소거되지 않고 잔류**한다 — 읽혀도 "pending" 으로 해석하지 말 것.
6. RS-485: 어댑터 접속 직후 첫 FC06 이 간헐적으로 무응답일 수 있다(라인 글리치가 첫 프레임에 병합). read-back 후 재시도로 흡수한다.

### 4.2 기동 · 정지

| 항목 | 규칙 |
|---|---|
| 기동 | START = 1(탭) 또는 2(hold). 거부 조건 = 가동 중 · SEEK/RESET 진행 중 · E-STOP · 과부하 · horn 모드 · warm-up |
| 정지 경로 | STOP = 1 · Hand 모델 ON_TIME 상한 · 에너지 도달(EN_ENERGY) · OVTIME fault · **30 s 절대 안전 상한(모든 런)** · E-STOP · 과부하 · 패널 RUN 버튼 release · hold 워치독 트립 |
| STOP 대칭 | 원격 STOP 은 패널 기동 런도 세운다. 반대로 **패널 RUN 버튼 탭 1회가 원격 런을 세운다** |
| 링크 단절 | 탭 런은 링크가 끊겨도 상한까지 계속 가동한다. hold 런은 ≤ 0.6 s 에 선다(§4.3) |
| 직교성 | SEEK/RESET 진행 중 START 무시, 가동 중 SEEK/RESET 무시 |

### 4.3 hold-to-run (START = 2 / 3) — FEAT_CAP bit0

"누르고 있는 동안만 가동" 을 위한 방식. **TCP 에서 사용할 것**(RTU 9600 은 FC03 50칸 응답만 약 120 ms 라 주기 150 ms 를 못 맞춘다).

```
마스터                                컨트롤러
  FC06 START=2            →   기동 + hold 워치독 무장 (T = 600 ms)
  FC06 START=3 (150 ms 주기) →   워치독 시각 갱신 (기동 권한 없음)
  ... 유지 신호 중단 ...
                          ←   마지막 유지 신호 + 600 ms 에 STOP 과 동일 경로로 정지 (STATUS bit0 = 0)
```

- 권고 유지 주기 **P = 150 ms**(100 ms 허용). 유지 신호 3건 연속 유실 시 트립.
- 유지 신호(3)는 IDLE·트립 후·다른 경로 정지 후에 도착하면 **no-op**. 재기동은 START = 2 만.
- hold 런은 탭 런과 같은 정지 경로(30 s 상한·E-STOP·STOP·패널 release)를 모두 승계한다.
- 다른 마스터의 탭 런이 도는 중에 START = 2 는 무시된다(그 런은 워치독 대상이 아님).
- **구 펌웨어(FEAT_CAP 없음)에서 START 2/3 은 어떤 동작도 일으키지 않는다**(fail-safe). 지원 판별은 §4.6.

### 4.4 원격 게이트 (REMOTE 모델 전용)

| 항목 | 규칙 |
|---|---|
| 허용 조건 | 기계 측 **원격 허용 스위치(PC8) ON** + E-STOP 비활성 + 통신 침묵 < 10 s |
| 닫힘 시 | START · SEEK · RESET · CFG_CTRL · 설정 쓰기 전부 **무음 거부**(명령은 소거, 설정은 다음 tick 미러가 원값 복원) |
| 항상 통과 | **STOP 과 모든 읽기** |
| 상태 · 사유 | 0x2B REMOTE_EN |
| 침묵 후 첫 프레임 | 10 s 이상 침묵 뒤 첫 프레임은 거부된다 → 재접속 후 **FC03 1회로 프라임**한 뒤 명령을 보낼 것 |
| STD 모델 | 게이트 없음. 0x2A 에 매직이 없다 |

> ⚠ 출하 빌드(`V3.1.0R!`, 버전 문자열의 `!`)는 PC8 극성이 **반전**된 것이 최종 상태다(사용자 결정 2026-11-15) — 스위치 회로 미실장, PC8 은 상시 "허용". 위 표의 "원격 허용 스위치(PC8) ON" 조건은 항상 참이다. 단선 = 불허 fail-safe 는 없다.

### 4.5 통신 설정 변경 (staging + commit)

통신 설정은 그 값을 전달하는 링크 자체를 바꾸므로 즉시 반영하지 않는다.

```
마스터                                 컨트롤러
  FC06 0x1F COMM_SPEED = 2       →   staged 버퍼에만 저장 (cfg · FRAM · 링크 불변), CFG_STAT = 1
  FC03 0x1F                      ←   2   (staged 값이 read-back 된다)
  FC06 0x22 / 0x23 ...           →   여러 필드 자유 순서
  FC06 0x28 CFG_CTRL = 1         →   검증(범위 → 자기 링크 → 가동 중) → 통과: 전 필드 일괄 반영 + FRAM 1회
  FC03 0x29 CFG_STAT             ←   2 = COMMIT_OK / 3~5 = 거부 사유
```

| 규칙 | 내용 |
|---|---|
| 부분 커밋 없음 | 검증 하나라도 실패하면 아무것도 반영하지 않는다 |
| **자기 링크 커밋 거부** | RTU 로 접속해 serial 그룹(0x1E~0x20)을 커밋 ✗, TCP 로 접속해 ether 그룹(0x22~0x27)을 커밋 ✗ → REJ_SAME_LINK. 커밋 링크가 살아 있어야 read-back 으로 결과를 확인할 수 있기 때문 |
| 교차 커밋의 반영 시점 | ether 그룹(RTU 경유) = 즉시 재적용. serial 그룹(TCP 경유) = cfg·FRAM 에 저장되고 **다음에 SERIAL 로 전환할 때** 적용 |
| 가동 중 거부 | 런·SEEK/RESET 진행 중 커밋 ✗ (REJ_RUNNING) |
| 만료 | 마지막 staged 쓰기 후 **30 s** 무쓰기 → 폐기(REJ_TIMEOUT). 링크 전이(모드 전환·재접속)에도 폐기 |
| DISCARD | CFG_CTRL = 2 → staged 전부 폐기, IDLE |
| comm_mode | 원격 변경 불가. LCD 전용 |
| 게이트 | staged 쓰기는 게이트 무관하나, **게이트가 닫혀 있으면 staged 스캔 자체를 건너뛴다**(CFG_STAT 불변). 커밋은 게이트 대상 |

> ⚠ DHCP 리스 IP 는 0x22/0x23 과 같은 필드에 미러된다. 리스가 떠 있는 상태에서 LCD 로 `ETH_STATIC` 저장을 하면 그 리스 IP 가 static IP 로 굳는다(의도된 동작). static 설정 시 IP 를 직접 쓸 것.

### 4.6 펌웨어 세대 · 모델 · 기능 판별

쓰기 없이 FC03 읽기만으로 판별한다.

| 0x31 CFG_CAP | 0x2A REMOTE_CAP | 판정 |
|---|---|---|
| `0xFA01` | `0x5201` | V3 펌웨어 · **REMOTE** 모델(게이트 있음) |
| `0xFA01` | 그 외 | V3 펌웨어 · **STD** 모델(0x1E 이후 지원, 게이트 없음) |
| 그 외 | — | **구 펌웨어** — 0x00~0x1D 만 유효. 0x1E 이후를 그리면 안 된다 |

링크 전이 순간 0 리셋과 미러 복원 사이에 짧은 창이 있으므로 **연속 2폴 비-매직**을 구 펌웨어 판정 조건으로 쓸 것.

hold 워치독 판별(연결당 1회):

```
1. 평소 폴 = FC03 (0x00, 50칸)                  — 모든 펌웨어에서 응답
2. 0x31 != 0xFA01                              → 구 펌웨어, hold 미지원
3. 0x31 == 0xFA01 → FC03 (0x32, 1칸) 프로브
     응답 있음 → bit0 = 1 지원 / 0 확정적 미지원
     무응답     → 비트맵 이전 V3 펌웨어(3.1.0 포함), 미지원
4. 지원 확정 후 폴 폭을 51칸으로 넓혀도 된다
```

---

## 5. 예제 (슬레이브 주소 1)

RTU 프레임을 기준으로 적었다. TCP 는 CRC 를 빼고 앞에 MBAP 6바이트(Transaction ID · Protocol ID 0 · Length · Unit ID)를 붙인다.

### 5.1 Work count 읽기 (FC03, 0x00 × 2)

| Request | Response |
|---|---|
| `01 03 00 00 00 02 C4 0B` | `01 03 04 HH HH LL LL cc cc` (u32 = HH HH << 16 \| LL LL) |

TCP: `00 01 00 00 00 06 01 03 00 00 00 02` → `00 01 00 00 00 07 01 03 04 HH HH LL LL`

### 5.2 STATUS 읽기 (FC03, 0x1D × 1)

| Request | Response |
|---|---|
| `01 03 00 1D 00 01 14 0C` | `01 03 02 SS SS cc cc` (예: `00 01` = 가동 중) |

### 5.3 전 맵 읽기 (FC03, 0x00 × 51)

`01 03 00 00 00 33 05 DF` → 107 B 응답. **구 펌웨어(50칸)는 무응답** — §4.6 판별 전에는 50칸(`00 32`)으로 읽을 것.

### 5.4 초음파 기동 (탭) / 정지

| 동작 | Request (응답 = 에코) |
|---|---|
| START 탭 | `01 06 00 1B 00 01 38 0D` |
| STOP | `01 06 00 1C 00 01 89 CC` |

### 5.5 hold-to-run

| 동작 | Request (응답 = 에코) |
|---|---|
| hold 기동 | `01 06 00 1B 00 02 78 0C` |
| 유지 신호 (150 ms 마다) | `01 06 00 1B 00 03 B9 CC` |
| 손 떼기 | 유지 신호 중단 → 600 ms 후 자동 정지 (또는 STOP = 1) |

### 5.6 RTU 보드레이트 변경 (TCP 로 접속한 상태에서)

| 순서 | Request | 확인 |
|---|---|---|
| 1 | FC06 `0x1F` = `0002` (9600) | FC03 `0x1F` → 2, `0x29` → 1 (STAGED) |
| 2 | FC06 `0x28` = `0001` (COMMIT) — RTU `01 06 00 28 00 01 C8 02` 에 해당 | FC03 `0x29` → 2 (COMMIT_OK) |
| 3 | LCD 에서 `comm_mode` 를 SERIAL 로 전환 | 새 보드레이트로 RTU 개통 |

### 5.7 세대 판별 (FC03, 0x31 × 2)

`01 03 00 31 00 02 95 C4` → `01 03 04 FA 01 00 01 cc cc` = V3 · 0x32 FEAT_CAP bit0 = hold 지원.
(구 펌웨어는 51칸 범위를 넘지 않는 이 요청에 `0x31` 잔류값(대개 0)을 돌려준다.)

---

## 6. 외부 신호 (케이블)

V2.0 문서의 케이블 연결표를 현행 MCU 신호에 대응시킨 것이다. **핀 번호는 V2.0 문서 값**이며 현행 PCB(hw-revA)
커넥터 대응은 아직 확인되지 않았다 — 확인 후 갱신할 것.

| 신호 | 방향 | 전기 (V2.0 표기) | MCU 신호(현행) | V2.0 핀 (신호 / GND) |
|---|---|---|---|---|
| Start | 입력 | 접점 | PA15 `CON_START` | 19 / 23 |
| Reset | 입력 | 접점 | PC10 `CON_RESET` | 15 / 22 |
| Seek · E-STOP | 입력 | 접점 | PC11 (Hand/Multi = SEEK, Standard = E-STOP, MODEL_TYPE 에 따라 역할 전환) | 14 / 21 |
| Overload | 출력 | NPN 오픈 컬렉터 | PB3 `CON_OVLD` (릴레이 구동, active HIGH) | 12 / 11 |
| U.S. Output | 출력 | NPN 오픈 컬렉터 | PB4 `CON_USOUT` (초음파 출력 게이트, run/stop 연동) | 8 / 7 |
| RS485-COM | 통신 | RS-485 | USART6 PC6/PC7 → 트랜시버 A(TRX+) / B(TRX−) | 24 (TRX+) / 10 (TRX−) |
| 양손 시작 1 / 2 | 입력 | 접점 | PC12 `CON_KEY1` / PB11 `CON_KEY2` | (V2.0 표 없음) |
| 센서 상승 / 하강 | 입력 | 접점 | PA12 `CON_SENS_UP` / PA11 `CON_SENS_DN` | (V2.0 표 없음) |
| 솔레노이드 하강 | 출력 | — | PB5 `CON_SOL_DN` | (V2.0 표 없음) |
| 원격 허용 스위치 | 입력 | 접점(풀업, 닫힘 = 허용) | PC8 `CON_REMOTE_EN` — REMOTE 모델 전용 | (신설) |

핀 상세 = `docs/pinmap.md` (§GPIO Output/Input — 커넥터, 부록 D).

---

## 부록 A. V2.0 (250705) 대비 변경점

| 항목 | V2.0 | V3.0 |
|---|---|---|
| 레지스터 수 | 30 (0x00~0x1D) | **51** (0x00~0x32). 0x1E~0x32 신설 |
| 범위 밖 FC03 | (미정의, 오버런) | **무응답** |
| 0x17 MODEL_FREQ · 0x18 MODEL_TYPE | R | **R/W** |
| 0x1B START 값 | 1 = ON | **1 탭 / 2 hold / 3 유지 신호**, 값 불문 소거 |
| 0x1C STOP | 주체 일치 시만 정지(SAMD20) | **기동 주체 무관 정지** |
| 0x1D STATUS | bit0~3 | **bit5 SENSOR / bit6 HORN / bit7 SEEK / bit8 RESET 신설** |
| 0x16 EN_SAFTY | as-is 저장 | **0/1 정규화** |
| 0x01 WORK_CNTL = 0 리셋 | 하위 16비트만 비교(65536 배수에서 무시) | 32비트 비교 |
| 0x04 DISP_FREQ 단위 | "kHz × 10" 표기 | **Hz** (코드는 두 세대 모두 Hz 원시값 미러 — 표기 정정) |
| 통신 설정 원격 편집 | 없음(LCD 전용) | **0x1E~0x29 staging + commit** |
| 원격 게이트 · capability | 없음 | 0x2A/0x2B (REMOTE), 0x31/0x32 (모델 무관) |
| 보정 · horn | 없음 | 0x2E/0x2F int16, 0x30 |
| RTU 초기값 | 19200 / 8E1 | FRAM 초기값 = 주소 0(비활성) / 2400 / EVEN. 출하 관례 9600 / EVEN / 주소 1 |
| TCP Unit ID | 1 고정 | 무검사, 에코 |
| TCP 유휴 | — | **12 s 무요청 시 연결 해제** |
| 지원 FC | 03 / 06 만 기재 | 01 / 02 / 03 / 04 / 05 / 06 (코일·FC04 는 기능 없음). FC16 미지원 명시 |
| 예외 응답 | 미기재 | **없음(무응답)** 명시 |
| 예제 | 03 / 06 | + hold · 설정 커밋 · 세대 판별 |

---

## 부록 B. 참조

- 정본 코드: `fw/include/app_modbus_core.h` (주소·비트·매직·상한), `fw/src/app_modbus.c` (미러/적용/클램프), `fw/src/app_modbus_tcp.c` (포트·유휴·처리량), `fw/drivers/usart6_mb.c` (보드레이트·유휴 gap)
- 설계 문서: `docs/superpowers/specs/2026-06-12-stage-c-modbus-slice1-core-rtu-design.md` (RTU 코어) · `2026-06-13-stage-c-modbus-slice2a-tcp-design.md` (TCP) · `2026-08-16-comm-eth-register-extension-design.md` (0x1E~0x29) · `2026-08-15-remote-enable-gate-design.md` (게이트) · `2026-08-30-remote-parity-requirements.md` (0x2E~0x31, STATUS bit5/6) · `2026-09-06-remote-hold-to-run-design.md` (0x32, START 값)
- 벤치 결과: `docs/superpowers/plans/2026-09-05-bench-results.md`, `2026-09-06-hold-to-run-bench-results.md`
- 소비 측 계약: `~/dev/work/gdsonic/gds_us_remote/docs/reference/modbus-contract.md`
- Modbus 표준: *Modbus over serial line V1.02* (2006), *Modbus Application Protocol V1.1b3* (2012)
