# Stage C slice 1 — Modbus 코어 + RTU(RS-485) 설계

> **요약**: samd20의 Modbus 슬레이브(`ref/samd20/modbus.c` + `update_holding_reg`)를
> STM32F410 펌웨어에 흡수하는 첫 슬라이스. RTU/TCP가 공유하는 **순수 Modbus 코어**
> (CRC16·FC 01~06 디코드·레지스터 테이블, 호스트 테스트)와 **USART6 RTU 전송층**
> (DMA circular + IDLE-line 프레임 경계), **앱 통합층**(레지스터↔앱 동기화, US_COMM
> 명령, mon↔Modbus USART6 런타임 점유 전환)을 만든다. Modbus TCP(W5500)는 같은
> 코어를 재사용하는 slice 2. 코드+호스트 테스트는 HW 없이 완결; 실통신 E2E는
> 보드 연결 후(HW-gated). 브랜치 base = stacked 체인 tip(run FSM 필요).
>
> 승인: 사용자 brainstorming 2026-06-12 (슬라이스/점유 전환/전체 충실 포팅/접근 A).

## 1. 배경 / 목표

- 기존 SAMD20은 Modbus 슬레이브를 Serial(RS-485) **또는** TCP(W5500) 택일 모드로 제공
  (`comm_mode`: 0=SERIAL / 1=ETH_STATIC / 2=ETH_DHCP — FRAM 영속, 이미 LCD 스테이지에서
  편집·영속 포팅 완료).
- Stage C slice 1 = **코어 + RTU**. 목표: HW 없이 코드·테스트 완결, 보드 연결 후 E2E만.
- 포팅 방침 = 의미론은 samd20 충실(레지스터맵·클램프·consume-and-clear·FRAM 저장),
  구조는 코드베이스 관용(순수 코어 분리 + 얇은 드라이버 + 앱 통합층) — LCD/app_reg와 동일.

## 2. 모듈 구조 (접근 A, 승인됨)

| 모듈 | 책임 | HAL 의존 |
|---|---|---|
| `fw/src/app_modbus_core.{c,h}` | 순수 코어: CRC16, RTU/TCP 공용 PDU 디코드(FC 01/02/03/04/05/06), `holdingReg[50]`/`coils[50]` 테이블, 응답 PDU 생성 | 없음 (호스트 테스트) |
| 신규 `fw/drivers/usart6_mb.{c,h}` (mon_usart6와 페리페럴 공유 — 점유 전환 규칙이 배타 보장) | RTU 전송층: DMA circular RX + **IDLE-line 검출** = 프레임 경계, blocking TX. 속도 2400/4800/9600/19200/38400/115200 + 패리티 EVEN/ODD/NONE (cfg 인덱스 그대로) | USART6 |
| `fw/src/app_modbus.{c,h}` | 통합: `update_holding_reg` 등가(읽기 미러/쓰기 반영), 명령→`app_reg_command`(US_COMM), FRAM 커밋, USART6 점유 전환, `app_lcd_hook_comm_reconfigure` 실체화 | 간접 |

데이터 흐름:

```
USART6 RX(DMA+IDLE) ─frame→ core: check_crc → addr 필터 → FC 디스패치 → 응답 PDU ─→ TX
                                     │ 쓰기(FC 05/06)
                                     ▼
                       app_modbus: 명령 consume-and-clear → app_reg_command(US_COMM)
                                   설정 클램프 → app_lcd_cfg() + FRAM 커밋
                       app_modbus_tick(): 라이브 값 → holdingReg[] 미러 갱신
```

슈퍼루프: `app_loop_iter()`에 `app_modbus_tick()` 추가 (LCD RX 드레인 패턴과 동일).
RTU 프레임 경계는 samd20의 byte-ISR + 1.04ms 소프트 타이머 대신 STM32 USART
IDLE-line 인터럽트 사용 — 같은 의미론(유휴 갭 = 프레임 끝), `usart1.c` DMA 선례 재사용.
**구조 편차로 문서화**(메커니즘만 다름).

## 3. 레지스터맵 (samd20 `modbus.h` H_REG 0x00~0x1D 충실)

### 3.1 라이브 읽기 (FC 03/04, `app_modbus_tick()` 미러)

| Reg | 이름 | 소스 | 비고 |
|---|---|---|---|
| 0x00/0x01 | WORK_CNT H/L | `app_lcd_cfg()->work_cnt` (u32 분할) | **CNTL에 0 쓰기 = 카운터 리셋**(samd20:4539, cfg+FRAM+LCD 갱신) |
| 0x02 | DISP_POWER | 러닝 중 `max_power`, 정지 시 `last_power` (samd20:4564 미러) | `lcd_measure_t`에 이미 존재 |
| 0x03 | DISP_AMP | 러닝 중 `max_amp`, 정지 시 `last_amp` (samd20:4565) | **app_reg에 max_amp/last_amp 추적 추가**(power latch와 동일 패턴) + `lcd_measure_t` 필드 확장 |
| 0x04 | DISP_FREQ | 0 | freq 미구현 — 정직 응답, deferred |
| 0x05 | DISP_ENERGY | 0 | energy 미구현 — 정직 응답, deferred |
| 0x17/0x18 | MODEL_FREQ/TYPE | cfg 미러 | **read-only** — samd20 `update_holding_reg`에 쓰기 반영 분기 없음(FC 06은 받아들이나 미러가 덮어씀) = 충실 보존 |
| 0x1D | STATUS | bit0 `STATUS_US` = (us_run_status != US_IDLE) | ESTOP/OVLD/OVTIME/OUTERR(bit1~4) = 0, overload/weld deferred |

### 3.2 설정 R/W (FC 06 쓰기 시 클램프 → cfg 반영 + FRAM 커밋)

| Reg | 이름 | 클램프 (samd20 그대로) |
|---|---|---|
| 0x06 | OUT_POWER | 50~100 |
| 0x07 | ON_TIME | ≤2000 |
| 0x08 | ENERGY | u16 → limit_energy |
| 0x09 | TIMEOVER | ≤10 |
| 0x0A~0x0C | DELAY1/2/3 | ≤500/≤500/≤2000 |
| 0x0D~0x0E | TRIGGER2/3 | ≤500/≤2000 |
| 0x0F~0x12 | MULTI_T1/T2/O1/O2 | ≤2000/≤2000/50~100/50~100 |
| 0x13 | RUN_MODE | 클램프 없음 (samd20 충실 — 저장 그대로) |
| 0x14~0x16 | EN_ENERGY/EN_MULTI/EN_SAFTY | 0/1 |

- 저장은 우리 `app_config` 커밋 경로 사용. **원본 FRAM 주소 복붙 버그 2건 수정**
  (samd20: DELAY3→`ADDR_TRIGGER2`, TRIGGER2→`ADDR_DELAY2`에 저장 — 의도적 편차, 수정 포팅).
- EN_* 쓰기 시 samd20은 LCD 표시도 갱신(`DISP_ENERGY_EN` 등) — 동등하게 기존 LCD
  렌더 경로로 반영.

### 3.3 명령 (FC 06, consume-and-clear)

| Reg | 이름 | 동작 |
|---|---|---|
| 0x19 | RESET | `app_reg_command(US_CMD_RESET)` 라우팅. 효과는 기존대로 no-op(RESET→SEEK 체인 deferred). samd20의 에러표시 클리어 분기는 error 기계 자체가 deferred라 inert — 주석 문서화 |
| 0x1A | SEEK | `app_reg_command(US_CMD_SEEK)` 라우팅 (no-op, deferred) |
| 0x1B | START | run FSM COMM 시작 (§4) |
| 0x1C | STOP | run FSM COMM 정지 (§4) |

쓰기 후 레지스터 1회 소비 → 0 클리어 (samd20 `update_holding_reg(1)` 충실).

### 3.4 코일

`coils[50]`는 코어에 존재(FC 01/05 동작)하나 samd20과 동일하게 **앱 매핑 없음** —
충실 보존, 후속 슬라이스에서 필요 시 매핑.

### 3.5 프로토콜 에러 처리 (samd20 충실)

- CRC 불일치 / 타 주소 = **무응답**.
- 미지원 FC = **무응답** (samd20은 Modbus 예외 응답을 만들지 않음 — 충실 보존, deferred).
- TCP 모드(slice 2): `decode(mode != SERIAL)` 시 addr 필터 + CRC 검사 스킵 (samd20
  `decode_comm` 동일).

## 4. run FSM — US_COMM 소스 추가

- `app_reg_command(us_cmd_t cmd)` → `app_reg_command(us_cmd_t cmd, uint8_t src)`로 확장
  (src ∈ {US_TOUCH, US_COMM}; 기존 호출처 `app_lcd_hook_us_command`는 US_TOUCH 전달).
  START 가드:
  `us_run_status == US_IDLE`이면 호출 소스(US_TOUCH/US_COMM) 기록. STOP/RELEASE는
  **자기 소스 런만 정지** (samd20: comm STOP `==US_COMM`만, touch release `==US_TOUCH`만).
- **의도적 편차(승인)**: samd20 comm START 가드는 `!=US_REMOTE`(터치 런 탈취 허용).
  우리는 slice 2b advisor 결정과 일관되게 **`==US_IDLE` 엄격 가드**. 동시 소스 중재는
  REMOTE 슬라이스에서 재론.
- **on-time ceiling**: `limit_on_time×10ms`(0=off)을 US_COMM 런에도 적용 — samd20이
  COMM/REMOTE+SYS_HAND에 적용(main.c:5296 분기; 2026-06-10 분석 문서 권위)하던
  **원본 충실** 쪽. (TOUCH 적용이 의도적 안전 추가였음 — 기존 주석 유지.)
  COMM 정지 시 `swallow_start`는 **불필요**(V30 data=0 quirk는 터치 입력 전용) — START
  소비는 Modbus 레지스터라 release 역매핑이 없음.
- samd20의 multi_ctrl(2단계 출력)·energy_ctrl 분기(main.c:5234~)는 weld-cycle 기계 —
  **deferred**, 주석으로 위치 기록.
- 워밍업(main_state==1) 중 명령 무시는 기존 FSM 가드 그대로 적용(M16 충실 유지).
- samd20 comm START는 `I2C_POT` 쓰기(진폭) 동반 — 기존 `app_lcd_hook_set_pot` 스텁
  호출로 동등 배선(스텁 로깅, I2C_POT 실구동 = B-SEAM/F2 deferred).

## 5. USART6 점유 전환 (mon ↔ Modbus, 승인됨)

- **점유 규칙**: `comm_mode == COMM_SERIAL && comm_address != 0` ⇔ Modbus가 USART6 소유
  (samd20 메인루프 가드 `(comm_mode==COMM_SERIAL)&&(comm_address!=0)` 충실).
- 부팅: `app_modbus_init()`이 cfg 로드 후 결정. Modbus 소유면 mon 비활성(이후
  `mon_*` 호출 = no-op, 버스 오염 방지) + USART6를 Modbus 설정(속도/패리티)으로 재init.
- 재설정: 패널 DATA_SAVE → `app_lcd_hook_comm_reconfigure(speed, parity, addr)` 실체화
  → close/reinit (samd20 main.c:3387/3429/3501 `close_modbus`+`init_modbus` 등가).
  주소=NONE(0) 또는 comm_mode=ETH 저장 → Modbus 해제 + mon 복구.
- 트레이스 빌드(REG_TRACE 등)는 Modbus 미점유 상태에서만 의미 — HANDOFF/검증 절차에 명시.
- mon 모듈에 enable/disable 게이트 추가(출력 억제만, init 상태 유지).

## 6. 테스트 / 게이트

- **호스트 테스트** (`fw/test/`, 코어 전수): CRC16 벡터(표준 + samd20 루프 동등성),
  FC 03/04 읽기(단일/다중/범위 밖), FC 06 쓰기(클램프 경계·명령 consume-and-clear),
  FC 01/02/05 코일, bad-CRC·타 주소 무응답, 미지원 FC 무응답, TCP 모드 스킵 분기.
- **빌드 게이트**: main + 트레이스 구성 0-warning, 게이트된 코드(`-DREG_TRACE` 등)
  syntax-check 포함.
- **HW-gated (보드 연결 후)**: Mac USB-serial = RTU 마스터(`mbpoll` 등)로 ① 폴링(FC 03
  라이브 값) ② 설정 쓰기→FRAM 영속(전원사이클) ③ START/STOP 원격 런(ceiling 동작)
  ④ 점유 전환 라이브(주소 NONE↔설정) ⑤ 속도/패리티 샘플 매트릭스. 절차는 plan에서 상세화.

## 7. 브랜치 / 머지 순서

- 브랜치 `feat/stage-c-modbus-core-rtu`, **base = stacked 체인 tip**
  (`refactor/stage-d-m1-cfg-param-injection` — run FSM(slice 2b) + M1 주입 필요).
- 머지 순서: slice 2b HW 재검증·머지 → m1-refactor 머지 → 본 슬라이스(HW E2E 후) 머지.

## 8. Deferred (이번 슬라이스 범위 밖)

- **slice 2**: Modbus TCP — W5500 벤더 스택(w5500/socket/wizchip_conf) + SPI1 드라이버
  + ethernet/DHCP/process_tcp + MBAP. 코어는 본 슬라이스에서 mode 분기까지 준비됨.
- STATUS 비트 ESTOP/OVLD/OVTIME/OUTERR 라이브화(overload/weld 슬라이스).
- DISP_FREQ/ENERGY 라이브화, multi_ctrl/energy_ctrl 런 분기(weld-cycle).
- REMOTE 소스 중재(물리 스위치), Modbus 예외 응답 생성.
- 코일 앱 매핑.
