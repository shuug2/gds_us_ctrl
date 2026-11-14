# UART 모니터 포트 신설 — USART2 PA2/PA3 + 소프트웨어 부트로더 점프 (H/W·F/W 수정점)

> **문서 요약**: 전용 UART 모니터 포트를 **USART2 (PA2 TX / PA3 RX)** 에 두고, 같은 포트로 **시스템 부트로더 UART 다운로드**까지 한다. 회로는 부저를 PA2 → **PB9** 로 옮기고 3 핀 헤더(GND·TX·RX)를 내는 것이 전부이며 **BOOT0 는 무변경**(점퍼 없음). 부트로더 진입은 앱이 모니터 RX 로 매직 문자열 `!!GDSUS_BOOT!!` 을 받으면 RTC 백업 레지스터에 표식을 남기고 리셋한 뒤, `main()` 첫 줄에서 `0x1FFF0000` 으로 점프하는 방식(IWDG 기동 전이라 안전). 펌웨어 수정 8곳 + 문서 3곳 + host 테스트 1개. 결정 2026-09-20(사용자). **2026-10-27: V31 회로 반영 확인 + 펌웨어 구현(미커밋). **2026-11-14: V31 실보드 벤치 V-1~V-10 PASS(§5.4), 부트로더 보율 = 115200 단일(사용자 결정)** — 실제 구현은 아래 §0 이 정본이고 §3.1 표와 다른 곳이 있다.**

---

## 0. 구현 반영 (2026-10-27) — 이 절이 §2.2·§3·§7 보다 우선

**회로 = `hw/schematics/USW_CTRL_V31.{asc,pdf,DSN}`** (넷 대조: PA2 U2.16 → MON_TX → J3.3 · PA3 U2.17 → MON_RX → J3.2 · PB9 U2.62 → BUZZER R60 · PC6/PC7 = MODBUS_TX/RX(U10 전용) · BOOT0 U2.60 → R64 → GND 무변경). V30 대비 spec 밖 변경: DB25 P2 `B_UVOUT_P/N` 핀 3/4 → 8/7, U20.6 → GND (펌웨어 무관).

**헤더 J3 (실물, §2.2 와 다름)**: 1 = `VBUS_IN` = **MCU VDD 3.3 V 레일**(이름과 달리 입력 아님) · 2 = RX(PA3) · 3 = TX(PA2) · 4 = GND. NRST 없음(O-2 = 미포함). 1 번 3.3 V 는 의도된 설계(O-5). ⚠ 어댑터의 전원 핀은 1 번에 꽂지 말 것 — 보드 3.3 V 와 충돌.

**V30 보드가 소량 출하됐으므로 두 리비전을 빌드 옵션으로 공존** — `HW_REV=v30|v31 ./fw.sh` (CMake `-DHW_REV`, 검증 `define.h #error`, 빌드 폴더 `fw/build-<hw>-<brand>-<model>/`). **V30 빌드는 변경 전과 바이트 동일**(4 업체 × CTRL/REMOTE 8 개 SHA 대조). 그래서 §3.1 의 "삭제·제거" 항목은 하지 않았다:

| §3.1 | 실제 구현 |
|---|---|
| F-1 부저 | `fw/drivers/io.c` — `#if HW_V31` PB9, 아니면 PA2 |
| F-2 mon | `mon_usart6.c` **유지**(공통 `mon_writeln/printf`), 송신 UART 만 `#if`(V31 = `huart2`). `mon_init()` 이 V31 에서 `usart2_init()` 호출. 신설 `fw/drivers/usart2.c` = USART2 115200 8N1(PA2/PA3 AF7, RX 풀업) + RXNE 인터럽트 + `USART2_IRQHandler`(irq.c 아님 — V31 코드를 한 파일에) |
| F-3 게이트 | `mon_set_enabled()` **유지** — V30 은 그대로, V31 에서는 무시(mon 이 RS-485 와 분리) |
| F-4 핸들 | `huart2` 정의는 `periph.c` 에 `#if HW_V31` (V30 RAM 배치 불변) |
| F-5 IRQ | usart2.c 안. SR→DR 읽기로 ORE/FE/NE 클리어, FE/NE 바이트는 매처에 안 넣음 |
| F-6 점프 | `fw/src/boot_jump.c`(V31 전용) API = `boot_jump_check()` / `boot_jump_arm()`(IRQ) / `boot_jump_poll()`(슈퍼루프). 매처는 순수 모듈 `fw/src/boot_magic.c` — **마지막 14 B 창 비교**(인덱스 방식은 `!!!GDSUS…` 같은 접두 겹침을 놓친다) |
| F-7 main | `main()` 첫 줄 `boot_jump_check()` · 루프 `app_loop_iter()` 뒤 `boot_jump_poll()` (둘 다 `#if HW_V31`) |
| F-8 날짜 | **미변경** — 버전 문자열은 리비전과 무관. 릴리스 시 날짜 갱신 |

§3.2 대비 추가(다운로드 툴 세션 검토 반영): 표식 읽기·지우기 전 `PWREN | RTCAPBEN` 클럭 + `PWR_CR_DBP`(리셋 직후 백업 도메인 쓰기 보호, F410 은 RTC APB 클럭이 따로 있다), 표식 없으면 셋 다 리셋 상태로 되돌림 · 점프 전 `SYSCFG->MEMRMP = 01`(시스템 메모리 리맵) + `SCB->VTOR = 0` · 점프 직전 mon 에 `[boot] magic -> ROM bootloader` 한 줄.

### 0.1 ROM 부트로더가 건드리는 핀 (AN2606 Rev 61 §31 Table 65 pp.135–137 · F410 DS DocID028094 Rev 3 Table 3/9/10 · V31 넷리스트)

F410 부트로더 V11.x = USART1(PA9/PA10)·USART2(PA2/PA3) 8E1 + I2C1(PB6/PB7)·I2C2(PB10/PB11)·I2C4(LQFP64 = PB14/PB15) slave 0x47 open-drain no-pull + SPI1(PA4~PA7)·SPI2(PB12/PB13/PC2/PC3) slave. 시스템 메모리 0x1FFF0000(29 KB), RAM 5 KB 사용. SYSCFG 리맵은 dual-bank 기종만 필요(§4.1 Note) — 우리 코드의 리맵·VTOR=0 은 BOOT0 부팅과 같은 맵을 만드는 무해한 보강.

| 보드 신호 | 부트로더 설정 | V31 외부 회로 | 판정 |
|---|---|---|---|
| PB2 SEEK / PB10 RESET / PB14 RUN (OSC_OUT0/1/4 → CN2) | PB2 미사용(리셋 기본 입력) · PB10 I2C2_SCL · PB14 I2C4_SDA — OD no pull = Hi-Z | 컨트롤러 측 풀 없음. 앱도 idle = OD Hi-Z + **OSC 보드 풀업**(board.c) | ✅ **부트로더 상태 = 앱 idle 상태**. 잔여: PB15(I2C4_SCL) 미배선 부유 — 잡음이 START+주소 0x47 을 만들면 PB14 ACK LOW 가능(극히 낮음) → V-2 스코프 확인, 다음 리비전 PB15 풀업 검토 |
| PB12 / PB13 (OSC_OUT2/3, 입력) | SPI2 NSS/SCK 입력 pull-down | R3/R4 10 K 풀다운 | ✅ 구동 없음 |
| PB3 과부하 릴레이 (B_OVLD → R38 → D7) | **미사용**(I2C4 PB3 은 F410Tx 전용) | 리셋 기본 JTDO — 정상 부팅 창과 동일 | ✅ 부트로더 고유 이슈 아님 |
| PB9 부저 / PC6·PC7 RS-485 | 미사용 | R61 풀다운 / — | ✅ |
| PB6/PB7 I2C1 (FRAM·pot) | I2C1 slave 0x47 | R1/R2 풀업, 버스에 master 없음 | ✅ FRAM 무영향 |
| PA4 ETH_SS → W5500 SCSn | SPI1_NSS 입력 **내부 pull-up** | 외부 풀업 없음 | ✅ W5500 비선택 유지(PA6 MISO 출력과 충돌 없음) — 내부 풀업 하나에 의존 |
| PA10 USART1 RX ← DGUS | 8E1 후보 — **0x7F 를 먼저 받은 USART 가 선택**(Fig.36) | DGUS 는 터치 없으면 무송신 | ⚠ 다운로드 중 LCD 터치 금지(§3.3) |

부트로더 USART: 0x7F 자동 보율(F4 는 115200 에서 오차 최대 −6 %, 실패 시 56000 권고 — AN2606 §4.5) · 기본 전압범위에서 플래시 **바이트 단위 쓰기**라 느릴 수 있음(V-7 에서 시간 실측).

**보관 파일명**: V31 용 = `gds_us_ctrl_<brand>_hw31_v<버전>.bin`, V30 = 지금 이름(`releases/README.md` §1). LCD 버전 문자열은 리비전과 무관.

**검증 현황**: host `test_boot_magic` PASS(18 스위트) · V-1 경고 0(vendor wiznet 기존 경고 제외), V31 FLASH +652 B(REMOTE gdsonic 67,844 B) · **V-2~V-10 V31 실보드 PASS(2026-11-14, §5.4)**.

---

## 1. 배경과 결정

| 항목 | 내용 |
|---|---|
| 현 상태 | mon 이 USART6 을 Modbus RTU 와 공유. RS-485 트랜시버(U10) DE 미제어로 캡처 불안정, Modbus 점유 중엔 `mon_set_enabled(false)` 로 침묵 |
| 목적 | ① 상시 모니터 포트 ② 현장에서 ST-LINK 없이 UART 로 펌웨어 다운로드 |
| 후보 검토 | F410RB 의 UART 는 USART1·USART2·USART6 **세 개뿐**(`stm32f410rx.h` 실측, UART4/5·LPUART 없음). USART1 = LCD(PA9/PA10, 대체핀 PB6/PB7 = I2C1 FRAM) · USART6 = RS-485(PC6/PC7, 대체핀 PA11/PA12 = 센서). **USART2 만 남음.** TX 대체핀 PD5 는 LQFP64 미노출, F4 는 TX/RX 스왑 없음 |
| 기각안 | LCD 를 USART2 로 옮기고 USART1 을 mon 으로(B안) — 회로 작업량 동일하나 LCD DMA RX 하드닝·DGUS 에코 경로 재검증 비용만 추가 |
| 부트로더 | AN2606 F410 부트로더(V11.x) = USART1 PA9/PA10 · USART2 PA2/PA3 지원. F4 는 옵션 바이트로 부트 소스 선택 불가(nBOOT0 없음) → BOOT0 점퍼 **또는** 소프트웨어 점프. **소프트웨어 점프 채택** |
| 트리거 | 모니터 RX 매직 문자열(사용자 결정). Modbus 레지스터 아님 → 계약 무변경, 원격기/HMI 통보 대상 아님 |

---

## 2. 회로 수정 (H/W)

### 2.1 핀 변경

| 신호 | 포트 | LQFP64 핀 | 현재 (V30 넷리스트) | 변경 | 핀번호 근거 |
|---|---|---|---|---|---|
| MON_TX (부트로더 TX) | **PA2** USART2_TX AF7 | **16** | `BUZZER` 넷 (R60.1) | R60 분리 → 헤더 TX | 넷리스트 `U2.16` 확인 |
| MON_RX (부트로더 RX) | **PA3** USART2_RX AF7 | 17 | 미배선 | 헤더 RX | ⚠ **DS 대조 필요** |
| BUZZER | **PB9** GPIO out | 62 | 미배선 | R60.1 → PB9 | ⚠ **DS 대조 필요** |
| BOOT0 | — | **60** | R64 풀다운 | **무변경** | 넷리스트 `U2.60` 확인 |

- 부저는 펌웨어상 단순 GPIO 토글(`fw/drivers/io.c:111`, TIM9 PWM 아님)이라 타이머 겸용 여부와 무관하게 아무 빈 GPIO 로 옮길 수 있다. PB9 는 ADC·기존 기능 겸용이 없는 순수 디지털 핀.
- ⚠ F410RB LQFP64 핀번호는 F401/F411 과 다르다(핀 54 = PB11, 사용자 DS 확인 2026-09-20). 넷리스트에 없는 PA3·PB9 번호는 **STM32F410RB 데이터시트 Table(LQFP64)** 로 확정할 것.

### 2.2 헤더

| 핀 | 신호 | 비고 |
|---|---|---|
| 1 | GND | |
| 2 | TX (PA2) | MCU → 호스트. 3.3 V TTL |
| 3 | RX (PA3) | 호스트 → MCU. 3.3 V TTL |

- 3.3 V 레벨 USB-UART 어댑터 직결. 5 V 전원 핀은 두지 않는다(J2 LCD 커넥터처럼 5 V 가 있으면 오배선 시 어댑터 손상).
- 선택: 4 번 핀에 NRST 를 내면 `stm32flash` 의 RTS 리셋으로 다운로드 후 자동 재기동 가능. 필수 아님(§6 호스트 절차 참조).
- BOOT0 는 헤더에 내지 않는다(소프트웨어 점프 채택). 앱이 죽은 보드의 복구는 기존대로 SWD.

### 2.3 확인 항목

| # | 항목 | 왜 |
|---|---|---|
| H-1 | PA3·PB9 의 LQFP64 핀번호 | §2.1 |
| H-2 | DGUS 패널 UART TX 출력 레벨 | 5 V 출력형이면 MCU RX 가 5 V 허용(FT)이어야 함. LCD 는 그대로 PA10(FT)이라 이번 변경과 직접 관계는 없으나 같은 김에 기록 |
| H-3 | R60 값·부저 구동단(Q?) 그대로 PB9 에 연결 가능한지 | PB9 구동 능력은 일반 GPIO 와 동일. 문제 없을 것 |

---

## 3. 펌웨어 수정 (F/W)

### 3.1 수정점 8곳

| # | 파일 | 변경 | 비고 |
|---|---|---|---|
| F-1 | `fw/drivers/io.c:31-32` | `BUZZER_PORT GPIOA → GPIOB`, `BUZZER_PIN GPIO_PIN_2 → GPIO_PIN_9` | 다른 줄 무변경. GPIOB 클럭은 이미 활성 |
| F-2 | `fw/drivers/mon_usart2.c` **신설** / `fw/drivers/mon_usart6.c` **삭제** | `usart2_init()`(PA2/PA3 AF7, `__HAL_RCC_USART2_CLK_ENABLE`, APB1 48 MHz, 115200 8N1, RXNE 인터럽트) + 기존 `mon_init/mon_write/mon_writeln/mon_printf` 이식(`huart6 → huart2`) + RX 매직 매처 | `MON_TX_TIMEOUT_MS 50` 유지. CMake `file(GLOB drivers/*.c)` 가 자동 포함 — `fw.sh` 가 소스 목록 변경을 감지해 재구성 |
| F-3 | `fw/include/mon.h`, `fw/src/app_modbus.c:687,698` | `mon_set_enabled()` 선언·호출 **제거**. `app.c:30`·`app_modbus.c:299` 주석 정정 | mon 이 USART6 을 떠나므로 RS-485 오염 방지 게이트 불필요 |
| F-4 | `fw/src/periph.c`, `fw/include/periph.h` | `UART_HandleTypeDef huart2` 추가 | HAL 핸들 단일 정의 규칙 |
| F-5 | `fw/src/irq.c` | `USART2_IRQHandler` 추가 — `USART2->SR` RXNE 직독, `DR` 읽기, ORE 클리어. HAL 미경유 | 매직 매처 호출만. 리셋은 여기서 하지 않음 |
| F-6 | `fw/src/boot_jump.c` + `fw/include/boot_jump.h` **신설** | `boot_jump_check()` · `boot_jump_request()` · 순수 매처 `boot_magic_feed()` | §3.2 |
| F-7 | `fw/src/main.c` | ① `main()` **첫 줄** `boot_jump_check()` (`HAL_Init()` 앞) ② `usart6_init()` 옆 `usart2_init()` ③ 슈퍼루프 `while(1)` 안 `if (boot_jump_pending()) boot_jump_request();` | IWDG 기동(`HAL_IWDG_Init`)보다 앞이 핵심 |
| F-8 | `fw/include/define.h` | 빌드 날짜 갱신 (`_260911` → 빌드일). 20 자 유지 | 관례 |

무변경으로 두는 것: `app_modbus.c:686` `usart6_init()` "restore mon" 호출 — ETH 모드 전환 시 USART6 을 115200 8N1 로 되돌리는 동작. 무해하고 요청 범위 밖. 주석만 "USART6 기본 상태 복원" 으로 정정.

### 3.2 부트로더 점프 설계 (`boot_jump.c`)

```
[런타임]  USART2 RX IRQ ─ boot_magic_feed(byte) ─┐ 14 바이트 전부 일치
                                                  └→ s_pending = true
[슈퍼루프] boot_jump_pending() → boot_jump_request():
             PWR 클럭 on, HAL_PWR_EnableBkUpAccess(), RTC->BKP0R = 0xB00710AD,
             NVIC_SystemReset()
[리셋 직후] main() 첫 줄 boot_jump_check():
             BKP0R == 0xB00710AD ? → BKP0R = 0, __disable_irq 불필요(아직 아무것도 안 켬),
             SysTick->CTRL = 0, __set_MSP(*(uint32_t*)0x1FFF0000),
             ((void(*)(void))*(uint32_t*)0x1FFF0004)()
           : 정상 부팅 계속
```

| 결정 | 이유 |
|---|---|
| 리셋 뒤 점프(실행 중 직접 점프 아님) | IWDG 가 켜진 뒤엔 해제 불가(RM0401) → 직접 점프하면 약 5 s 뒤 부트로더가 리셋됨. 시스템 리셋 후 IWDG 는 꺼진 채 시작하고 `HAL_IWDG_Init` 은 슈퍼루프 직전이라 `main()` 첫 줄은 안전 |
| 표식 = RTC 백업 레지스터 BKP0R | 시스템 리셋에 살아남는 유일한 저장소 중 벤더 코드 무편집으로 쓸 수 있는 것. RAM 은 startup 이 `.bss` 클리어·`.data` 재적재, 링커 스크립트(`fw/vendor/*.ld`)는 read-only 라 `.noinit` 추가 불가. RTC 클럭 기동 불요(APB 인터페이스만) |
| 매직 `!!GDSUS_BOOT!!` 14 B | 부팅 배너·mon 로그와 충돌 없음. 사람이 터미널에서 칠 수 있는 길이 |
| 매처 = 바이트 단위 인덱스, 불일치 시 `idx = (b == magic[0]) ? 1 : 0` | 부분 일치 뒤 재시작 처리. host 테스트 대상 |
| 리셋은 IRQ 밖(슈퍼루프)에서 | FRAM 쓰기 중 리셋 방지. 한 반복 지연은 최대 수십 ms |
| 점프 뒤 앱 없음 | 다운로드 후 `stm32flash -R` 또는 전원 재투입으로 재기동. 부트로더에서 아무 것도 안 하고 리셋해도 BKP0R 은 이미 0 이라 정상 부팅 |

### 3.3 거동 변화

- 부팅 배너와 mon 로그가 **USART2 로 상시** 출력. Modbus 점유와 무관.
- 모니터 헤더에 접근 가능한 누구나 매직으로 부트로더 진입 가능 — 디버그 포트로서 수용.
- 부트 모드 중 USART1(LCD)도 부트로더가 듣는다 → **다운로드 중 LCD 터치 금지**. DGUS 는 터치 없으면 자발 송신 없음.
- Modbus 레지스터 맵·LCD VP·FRAM 레이아웃 **무변경**.

---

## 4. 문서 수정 3곳

| 파일 | 변경 |
|---|---|
| `docs/pinmap.md` | USART2 절 신설(PA2/PA3, 부트로더 겸용) · 부저 PA2 → PB9 · "USART2(PA2=BUZZER 충돌)" 이중 역할 정책 문구 삭제 · 여유 핀 표에서 PA3·PB9 제외 · USART6 절의 mon 역할 삭제 · Peripherals Summary 갱신 |
| `docs/changelog.md` | `[Unreleased]` 항목 |
| `releases/README.md` | §5 "부팅 로그 확인" 을 USART2 기준으로 · **§4-D UART 부트로더 다운로드** 절 신설(§6 호스트 절차) |

세션 마감 시 `CLAUDE.md`·`HANDOFF.md` 의 "mon 은 RS-485 DE 미제어로 캡처 불가" 문구 정정.

---

## 5. 검증

### 5.1 host 테스트 (`fw/test`)

`test_boot_magic`: 완전 일치 → true / 부분 일치 뒤 다른 바이트 → 재시작 / `!!` 반복 뒤 정상 문자열 → true / 잡음 속 삽입 → true 1회.

### 5.2 보드 없이·회로 수정 전 가능 (현 보드, SWD)

| # | 항목 | 방법 | 기대 |
|---|---|---|---|
| V-1 | 빌드 경고 0, FLASH 증가량 | `MODEL=remote ./fw.sh` | 경고 0 |
| V-2 | 점프 로직 | 플래시 후 SWD 로 BKP0R 에 `0xB00710AD` 기록(PWR DBP 선설정) → NRST | LCD 부팅 안 됨 + USART1(LCD 선 대신 어댑터 PA9/PA10)로 `0x7F` → `0x79` ACK = 부트로더 진입 |
| V-3 | 정상 부팅 무회귀 | 전원 재투입 | LCD 버전 표시, Modbus FC03 정상, IWDG 리셋 없음(`s_boot_rst` 정적 read 0x0E/0x04) |
| V-4 | 부저 무회귀 | 회로 수정 전엔 PB9 미배선 → 부저 무음이 정상. 수정 후 부팅 beep 100 ms | — |

### 5.3 회로 수정 후

| # | 항목 | 기대 |
|---|---|---|
| V-5 | 부팅 배너 | 어댑터 115200 8N1 로 `[boot] …` 배너 + mon 로그 |
| V-6 | 매직 → 부트로더 | `printf '!!GDSUS_BOOT!!' > /dev/cu.usbserial-*` 후 mon 침묵, `stm32flash /dev/cu.usbserial-*` 가 `Version 0x31 … Device ID 0x0458` 표시 |
| V-7 | UART 다운로드 | `stm32flash -w releases/…/gds_us_ctrl_v*.bin -v -R` → 재기동 후 LCD 버전 = 파일 버전 |
| V-8 | 오탐 없음 | mon RX 에 임의 문자·부분 매직 입력 → 리셋 없음 |
| V-9 | 부저 | 부팅 beep + 점멸계 250 ms 정상 |
| V-10 | Modbus RTU 무회귀 | SERIAL 모드 FC03/FC06 정상(USART6 경로 무변경 확인) |

### 5.4 V31 실보드 결과 (2026-11-14)

환경: V31 PCB · ST-LINK V3(SWD) · FT232R 3.3 V → J3(2 RX · 3 TX · 4 GND, 1 번 미연결). 펌웨어 = 미커밋 작업 트리 `HW_REV=v31 MODEL=remote`(GDSONIC, `.bin` SHA256 `ea6b6e0e…`). SWD 는 비정지 메모리 read 만(V-2 부트로더 상태 PC 확인 1회 제외). 판정 스크립트 = pyserial(V-6/V-8), V-7 = stm32flash 0.7.

| # | 결과 | 근거 |
|---|---|---|
| V-1 | ✅ | 경고 0. REMOTE 67,844 B / CTRL 67,532 B |
| V-2 | ✅ | SWD 로 BKP0R=`0xB00710AD` → `reset run` → BKP0R=0(1회용 소거) · MEMRMP=1 · `uwTick` 정지 · PC=`0x1FFF1D6C`(시스템 메모리). 부트로더 ACK 는 V-6 에서 USART2 로 확인 |
| V-3 | ✅ | 재리셋 → MEMRMP=0 · `uwTick` 진행 · `s_boot_rst`=0x04(핀), 전원 재투입 0x0E(POR). LCD 버전·모델명 정상(육안) |
| V-4/V-9 | ✅ | PB9 부팅 beep(청각) |
| V-5 | ✅ | J3 115200 8N1 `[boot] gds_us_ctrl ready rst=0x04` + `[lcd]`/`[cfg]`/`[eth]` 로그 |
| V-6 | ✅ | 매직 → 진입 문구 `[boot] magic -> ROM bootloader (USART2 8E1)` **105 ms** → 8E1 전환 20 ms 후 첫 `0x7F` 에 `0x79`(매직부터 **144 ms**, 재시도 0). GET 버전 `0x31`, 명령 `00 01 02 11 21 31 44 63 73 82 92`(Extended Erase 0x44 · 0x43 없음). GET ID `0x0458`. Option 1/2 = 0x00 |
| V-7 | ✅ | `stm32flash -b 115200 -w build-v31-gdsonic-ctrl/gds_us_ctrl.bin -v -R` = 67,532 B 지우기+쓰기+대조 **40 s** → `-R` 후 앱 기동(`s_boot_rst`=0x14 = SW+핀), LCD 버전 `V3.0.0_260911` 로 바뀜(육안), SWD `verify_image` 전 바이트 일치 |
| V-8 | ✅ | `hello` · 매직 앞 11/13 B · `!!GDSUS_BOOX!!` · `0x7F` 단독 · 앞 `!` 빠진 매직 → 진입 문구 없음, `uwTick` 리셋 없이 진행 |
| V-10 | ✅ | FTDI 듀얼 RS-485(A 채널) · 38400 8E1 · addr 1. FC03 0x00~0x1F 정상, FC06 OUT_POWER 80→80 / 30→50 / 120→100(클램프) → 원복 50. ⚠ LCD 로 addr 0→1·보율 변경 직후엔 무응답(USART6 BRR·CR1·CR3 는 이미 38400 9 비트 EVEN DMA, 프레임 도달 확인) → **전원 재투입 후 응답**. `apply_config()` 는 재부팅 없이 재획득하는 설계라 원인 미규명 — J3 `[mb] acquire` 로그로 재현 확인 예정 |

**부트로더 보율 = 115200 단일 (사용자 결정 2026-11-14)** — 56000 은 접속(0x7F ACK · GET · GET ID)만 PASS. stm32flash 0.7 이 56000 미지원이라 쓰기 미시험이고, 다운로드 툴의 56000 재시도 경로는 쓰지 않는다. 미시험: O-6 PB14 스코프. 시험 후 보드 = V31 REMOTE(ST-LINK 재플래시). 결과는 다운로드 툴 세션에 통보.

---

## 6. 호스트 절차 (→ `releases/README.md` §4-E)

```bash
# 1. 부트로더 진입 (앱이 살아 있을 때)
printf '!!GDSUS_BOOT!!' > /dev/cu.usbserial-XXXX

# 2-A. stm32flash (macOS/Linux/Windows, 오픈소스)
stm32flash -w gds_us_ctrl_v3.1.0R_260911.bin -v -R /dev/cu.usbserial-XXXX

# 2-B. STM32CubeProgrammer CLI
STM32_Programmer_CLI -c port=/dev/cu.usbserial-XXXX br=115200 p=even \
  -w gds_us_ctrl_v3.1.0R_260911.bin 0x08000000 -v -g 0x08000000

# 2-C. Flash Loader Demonstrator (flasher-stm32, Windows) — 8 bit / Even / 1 stop.
#      F410(PID 0x458) 맵 파일 없으면 기존 F4 .STmap 복사 후 PID·128 KB 로 편집
```

- 설정(FRAM)은 플래시와 무관하게 보존.
- 앱이 죽어 매직을 못 받는 보드 = SWD(`releases/README.md` §4-A/B).

---

## 7. 열린 항목

| # | 항목 | 담당 |
|---|---|---|
| O-1 | ~~PA3·PB9 LQFP64 핀번호~~ — **V31 넷리스트로 확정**(U2.17 / U2.62) | 닫힘 |
| O-2 | ~~헤더 NRST~~ — **미포함**(V31 J3) | 닫힘 |
| O-3 | ~~매직 문자열~~ — **`!!GDSUS_BOOT!!` 확정**(사용자 2026-10-27) | 닫힘 |
| O-4 | ~~구현 착수~~ — 2026-10-27 구현(§0), 2026-11-14 V-1~V-10 PASS(§5.4). 남은 것 = LCD 통신 설정 즉시 반영 재현 | 펌웨어 세션 |
| O-5 | ~~J3.1 3.3 V~~ — **의도된 설계**(사용자 2026-10-27). 어댑터 전원 핀 연결 금지만 문서화 | 닫힘 |
| O-6 | 부트로더 핀 재설정 — **대조 완료(§0.1), 설계 변경 불요**. 남은 것 = V-2 때 스코프로 OSC_OUT4(PB14 RUN) HIGH 유지 확인 · 다음 리비전에 PB15 풀업 검토 | 벤치 |
