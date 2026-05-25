# ATmega16 FW Behavior ↔ I/O Signal Analysis

> **분석 산출물** (kickoff `2026-05-25-atmega16-analysis-kickoff.md` §6의 deliverable).
> 코드 변경 없음 — 순수 분석. 모든 행동주장은 **가설**이며 confidence + HW-verify 방법을 명시.
> 작성: 2026-05-25. 소스: `ref/atmega16/m16_conv_v001.c`(디컴파일, 비판적 독해), `hw/schematics/usw_ctrl_v26_samd20.pdf`(OLD 보드, netlist 없음/육안), `hw/schematics/USW_CTRL_V30.asc`(NEW 보드 netlist).

---

## 0. 한 줄 결론

ATmega16은 **초음파 전력 레귤레이션 루프**(전류/출력 센스 → lookup 스케일 → 누진 드라이브 레벨)와 **overload 검출**을 담당한 실시간 코프로세서였고, SAMD20과는 **순수 GPIO IPC**(START/SEEK/RESET 명령 수신, OVLD/상태 보고)로만 연결됐다. **디컴파일러가 "7-세그 display"라고 명명한 출력 서브시스템(PORTD + PB2/3/4)은 V26 보드에서 물리적으로 미연결(dead legacy)**이며, 그 누진 비트 패턴(0x01→0x1F, 5비트)이 V30의 `OSC_OUT0..4`(5라인)와 1:1 대응한다 — **흡수 대상은 이 레귤레이션 로직**이다.

---

## 1. 확정된 아키텍처 (사용자 확인 2026-05-25 + 회로도 교차검증)

| 블록 | 역할 | 근거 |
|------|------|------|
| **OSC 보드** | 발진회로 (별도 보드) | 사용자 확인 |
| **ATmega16** | 트리거 신호 생성 + 전류 모니터링 (실시간 레귤레이션) | 사용자 + 코드(ADC/lookup/output_level) |
| **SAMD20** | UI(DGUS LCD) + 전체 동작 제어 | 사용자 + 회로도(LCD_TX/RX, ETH, MON) |
| **IPC** | SAMD20 ↔ ATmega16 = 순수 GPIO (UART/SPI/TWI 미사용) | 코드(hw_init이 USART/SPI/TWI 전부 비활성) + 회로도(`M_*`, `U1.xx` nets) |

V30 통합: STM32F410 하나가 **SAMD20 역할(LCD/comm/제어) + ATmega16 역할(센스/레귤레이션/트리거)** 모두 흡수.

---

## 2. 디컴파일 신뢰도 판정 (kickoff §1 체크리스트 결과)

`m16_conv_v001.c`의 **헤더 핀-역할 주석은 상당 부분 디컴파일러/작성자의 추측**이며 V26 회로도와 충돌한다. 레지스터 접근(`PORTx`/`PINx`/`DDRx` 명시)은 신뢰 가능하나, **"이 핀이 무엇에 연결되는가"의 주석은 회로도로 정정**한다.

**Decompiler-noise / 정정 로그:**

| C 주석 주장 | 코드 레지스터 동작 (신뢰O) | V26 회로도 실제 net | 판정 |
|---|---|---|---|
| PB0/PB1 = "LED 출력" | `g_run_flag→PB0`, `g_blink_active→PB1` | `U1.40`/`U1.41` (SAMD20) | net 주석 ✗ → IPC 상태신호 |
| PB2/3/4 = "디스플레이 digit 선택" | `display_multiplex`가 PB2/3/4 토글 | **× 미연결** | display 전체 dead |
| PORTD = "7-세그 세그먼트" | `display_set_segments`가 PORTD 기록 (DDRD=0xFF) | PD0/1/2/3/7 **× 미연결** | dead legacy 출력 |
| PB5 = "하트비트 LED" | Timer0 ISR가 PB5 토글 | `MOSI_M16` (AVR ISP) | LOW — ISP 핀 공유 or noise |
| PA1 = "ADC ch1 센서2" | ADC ch1 50샘플 평균 처리 | **× 미연결** | CONFLICT — §6 PA1 행 참조 |
| PC0/PC1 = "보조 제어" | DDRC로 출력 설정 | PC0=`M_OVLD`, PC1=`×` | PC0=overload OK / PC1 충돌 |
| PC6 = "ADC 레벨 표시 출력" | `main_loop_process`가 PC6 set/clr | `BUZ_M16` (buzzer) | net 주석 ✗ → 버저 |

**신뢰 가능한 코드 사실** (레지스터 명시·일관): DDR 방향 설정(`hw_init`), 타이머 reload 값, ADC 채널 토글, lookup_table 내용, output_level/main_loop의 누진 패턴 산출 로직, ISR 호출 그래프.

---

## 3. DELIVERABLE — 핀별 I/O 거동 테이블

방향(Dir)은 `hw_init`의 DDR 설정에서 **확정**(신뢰 HIGH). net은 V26 PDF 육안 판독. Conf 기준 = kickoff §2 (3중확인=HIGH).

### PORTA — `DDRA=0x00` (전체 입력), `PORTA=0xFC` (PA2~7 풀업)

| 핀 | Dir | Old fn | 추정 거동 | Conf | V30 net | HW-verify | Status |
|---|---|---|---|---|---|---|---|
| PA0/ADC0 (37) | IN(A) | `ISR(ADC)`→`adc_process_channels` ch0 | **출력/전류 센스 피드백** (10샘플 평균 → lookup 스케일 → 레귤레이션 입력). net=`SENS_OUT`+`U1.37`(SAMD20 공유) | **M** (코드 ADC + V26 SENS_OUT + V30 SENS_OUT 일치, 단 ch0/ch1 매핑 불확실) | `openocd` ADC reg / `scope SENS_OUT` | proposed |
| PA1/ADC1 (36) | IN(A) | `ISR(ADC)`→`adc_process_channels` ch1 (50샘플) | C는 ch1을 무겁게 처리(display-period 산출)하나 **회로도 핀 미연결(×)** | **L** (3중 충돌: 코드 사용 ↔ 핀 미연결. 디컴파일러 채널 혼동 or floating read 의심) | `openocd` ADMUX/ADC live read | proposed |
| PA2/ADC2 (35) | IN | (config read) | DIP 스위치 입력 `DIP_2` (설정) | M | EEPROM/FRAM (V30은 DIP 폐지) | `openocd PINA` | proposed |
| PA3/ADC3 (34) | IN | (config read) | DIP 스위치 입력 `DIP_3` | M | EEPROM/FRAM | `openocd PINA` | proposed |
| PA4/ADC4 (33) | IN | `ISR(TIMER1)` `PINA&(1<<4)` (L580) | **START 명령 입력** (초음파 출력 개시). net=`M_START` (R14 140Ω 직렬) | **H** (3중: 코드 PINA.4 읽기 + V26 M_START + 사용자 "start in" + V30 B_START) | `manual stimulus` B_START / `gdb watch` IDR | proposed |
| PA5/ADC5 (32) | IN | (코드 명시 read 미발견) | **SEEK 명령 입력**. net=`M_SEEK` (R16 140Ω) | M (V26 net + 역할추론; V30 직접대응 불명) | `manual stimulus` / `openocd PINA` | proposed |
| PA6/ADC6 (31) | IN | (코드 명시 read 미발견) | **RESET 명령 입력**. net=`M_RESET` (R15 140Ω) | M (V26 net + V30 B_RESET) | `manual stimulus` B_RESET | proposed |
| PA7/ADC7 (30) | IN | (IPC) | SAMD20 IPC 입력. net=`U1.30` | L (net만) | `openocd PINA` | proposed |

### PORTB — `DDRB=0xFF` (전체 출력), `PORTB=0xDF` init (PB5=0)

| 핀 | Dir | Old fn | 추정 거동 | Conf | V30 net | HW-verify | Status |
|---|---|---|---|---|---|---|---|
| PB0/T0 (40) | OUT | `ISR(TIMER0)` `g_run_flag→PB0` (L468) | **IPC: run 상태 → SAMD20**. net=`U1.40` (디컴파일러 "LED" ✗) | M (코드 write + V26 U1.40) | internal (single MCU) | `scope U1.40` / `openocd ODR` | proposed |
| PB1/T1 (41) | OUT | `ISR(TIMER0)` `g_blink_active→PB1` (L461) | **IPC: blink/active 상태 → SAMD20**. net=`U1.41` | M | internal | `scope` / `openocd ODR` | proposed |
| PB2/AIN0 (42) | OUT | `display_multiplex` digit0 sel | **DEAD** — 미연결(×). legacy 7-seg | H (미연결 확정) | — | (없음) | n/a |
| PB3/AIN1 (43) | OUT | `display_multiplex` digit1 sel | **DEAD** — 미연결(×) | H | — | (없음) | n/a |
| PB4/SS (44) | OUT | `display_multiplex` digit2 sel | **DEAD** — 미연결(×) | H | — | (없음) | n/a |
| PB5/MOSI (1) | OUT | `ISR(TIMER0)` 하트비트 토글 (L442/447) | net=`MOSI_M16` (AVR ISP). 하트비트 LED와 ISP MOSI 공유 추정 | L (코드 toggle ↔ ISP 핀 충돌) | — (ISP) | `scope MOSI_M16` | proposed |
| PB6/MISO (2) | OUT | (코드 미접근) | net=`MISO_M16` (AVR ISP) | M (ISP 헤더) | — | (없음) | proposed |
| PB7/SCK (3) | OUT | (코드 미접근) | net=`SCK_M16` (AVR ISP) | M (ISP 헤더) | — | (없음) | proposed |

> `hw_init`이 SPI 비활성(`SPCR=0`) → PB5/6/7은 SPI 페리페럴이 아니라 ISP 프로그래밍 헤더 + PB5는 GPIO 재사용.

### PORTC — `DDRC=0xC3` (PC0,1,6,7 출력 / PC2~5 입력), `PORTC=0xBC` (입력핀 풀업)

| 핀 | Dir | Old fn | 추정 거동 | Conf | V30 net | HW-verify | Status |
|---|---|---|---|---|---|---|---|
| PC0/SCL (19) | OUT | `hw_init` init0 + (set on fault) | **overload 출력 → SAMD20**. net=`M_OVLD` | **H** (3중: 코드 출력 + V26 M_OVLD + 사용자 "overload out" + V30 B_OVLD) | `B_OVLD`(U2.55) in / `CON_OVLD`(PB3) out | `scope M_OVLD` / `openocd ODR` | proposed |
| PC1/SDA (20) | OUT | `hw_init` `PORTC&=~(1<<1)` (output, init0) | **충돌**: 코드=출력, 회로도=미연결(×), 사용자="board signal **입력**" | **L** (3중 충돌 — 최우선 HW-verify) | (불명) | `openocd DDRC/PINC` + `scope` | proposed |
| PC2/TCK (21) | IN | `btn_scan_and_debounce` `PINC&(1<<2)` (L356) | 버튼1 입력 (out-of-scope: 존재만 기록). 회로도 라벨 불명 | M (코드 read) | — (V30은 opto KEY_START) | `manual stimulus` | proposed |
| PC3/TMS (22) | IN | `btn_scan_and_debounce` `PINC&(1<<3)` (L352) | 버튼2 입력 (out-of-scope) | M | — | `manual stimulus` | proposed |
| PC4/TDO (23) | IN | (IPC, DDRC bit4=0) | SAMD20 IPC 입력. net=`U1.23`. 사용자="board signal in"과 tension | L-M | internal | `openocd PINC` | proposed |
| PC5/TDI (24) | IN | (config read) | DIP 스위치 입력 `DIP_1` | M | EEPROM/FRAM | `openocd PINC` | proposed |
| PC6/TOSC1 (25) | OUT | `main_loop_process` PC6 set/clr (L731/742) | **버저 출력**. net=`BUZ_M16` (디컴파일러 "ADC레벨표시" ✗) | M-H (코드 + V26 BUZ_M16 + V30 BUZZER) | `BUZZER`(U2.16) | `scope BUZ_M16` | proposed |
| PC7/TOSC2 (26) | OUT | `ISR(TIMER0)` `g_blink_state→PC7` (L454) | **IPC: blink 상태 → SAMD20**. net=`U1.26` | M | internal | `scope U1.26` | proposed |

### PORTD — `DDRD=0xFF` (전체 출력), `PORTD=0xFF` init

| 핀 | Dir | Old fn | 추정 거동 | Conf | V30 net | HW-verify | Status |
|---|---|---|---|---|---|---|---|
| PD0~PD3, PD7 (9/10/11/12/16) | OUT | `display_set_segments` (PORTD = 7-seg seg) | **DEAD** — 전부 미연결(×). legacy 7-seg 출력 | H (PD0/1/2/3/7 미연결 확정) | — | (없음) | n/a |
| PD4~PD6 (13/14/15) | OUT | (display) | 미판독 — PD 패턴상 미연결(×) 추정 | M (추론) | — | `openocd` 확인 | proposed |

> **핵심**: PORTD는 7-세그 디스플레이용으로 보이나 V26에서 전부 미연결 → `display_*` 코드는 dead 핀을 구동. 7-세그는 OLD-OLD 보드 잔재로 추정.

---

## 4. 제어 루프 의미 (흡수의 핵심 — STM32가 가져갈 로직)

```
            ┌─────────────── ISR(ADC) free-run (~208µs/conv, /128) ───────────────┐
SENS_OUT ──▶│ PA0=ch0 (10샘플 평균) → adc_ch0_avg                                  │
(PA1=ch1?)  │ PA1=ch1 (50샘플 평균) → adc_ch1_avg                                  │
            └──────────────────────────────┬──────────────────────────────────────┘
                                            ▼
       ┌──── ISR(TIMER0_OVF) ~2.05ms (TCNT0 reload 0xF0) ────┐
       │ adc_calc_scaled_output():                           │
       │   adc_ch0_avg ──(lookup_table 21-entry 스케일)──▶ adc_scaled_value (0..1000 clip) │
       │ output_level_process():                             │
       │   lookup_table[i] vs adc_scaled_value 비교          │
       │   → 누진 드라이브 패턴 0x01→0x03→0x07→0x0F→0x1F→…→0xFF │
       │     (set_output_mode_{a,b,c}/full/reduced)          │
       └─────────────────────────────────┬───────────────────┘
                                          ▼
                       드라이브 레벨 (디컴파일러는 display 변수로 라우팅;
                       실제 물리 출력 핀은 §6 PC1/§7 미해소 — V30에선 OSC_OUT0..4)
```

- **레귤레이션 본질**: 센스된 전류/출력값(adc_ch0_avg)을 21-엔트리 lookup_table로 스케일링 → `adc_scaled_value`. `output_level_process`가 이를 테이블과 비교해 **누진 비트 패턴**(켜지는 드라이브 라인 수)을 결정. 센스값이 높을수록 패턴이 바뀌어 출력을 조절 (전력 레귤레이션 + 소프트스타트).
- **누진 5비트(0x01,0x03,0x07,0x0F,0x1F) ↔ V30 `OSC_OUT0..4`(5라인)** 의 1:1 대응이 가장 강한 단서.
- **`main_loop_process`** (메인 while): `timer1_tick_cnt`를 0부터 램프하며 임계값(0x29,0x51,0x79,…0x191)마다 패턴 상향 → **소프트스타트 램프**. ≥0x191(401)에서 `g_main_state=0`으로 종료.
- **overload**: 과전류/이상 시 PC0=`M_OVLD` 어서트 → SAMD20 보고.
- **명령**: PA4=START / PA5=SEEK / PA6=RESET (SAMD20→M16, 140Ω 직렬 = 노이즈/전류제한).

`lookup_table[24]` (0x03FF→0x0004 감소 곡선): ADC raw(10비트, 0x3FF=만재)를 역방향 스케일 — 센스가 낮을(부하 큰?)수록 다른 출력 레벨 매핑. **정확한 물리 의미(전류 한계? 주파수 추종?)는 HW 동작 관찰 필요.**

---

## 5. 타이머 / ISR cadence

| ISR | 주기 | 역할 | Conf |
|---|---|---|---|
| `ISR(ADC_vect)` | ~208µs/변환 (ADCSRA=0xCF, /128, 13cyc, single-shot 재무장) → ch0/ch1 교대 ⇒ 채널당 ~417µs | ADC 샘플 적재 + 평균(ch0 10·ch1 50) | H (레지스터 명시) |
| `ISR(TIMER0_OVF)` | **~2.05ms** (ISR가 TCNT0=0xF0 reload, 16cnt×128µs; 단 hw_init 첫 reload는 0x9B=~12.9ms) | 하트비트(PB5, ~1–2s), IPC 상태(PB0/PB1/PC7), `adc_calc_scaled_output`, `output_level_process`, `display_multiplex`(dead), comm_seq | H |
| `ISR(TIMER1_OVF)` | **~10.1ms** (TCNT1=0xFFB1, 79cnt×128µs) | mode1: `timer1_tick_cnt`++ (램프); mode0: `btn_scan`+전원/모드; blink phase; `adc_ch1_avg≥0x321` 리셋; **PA4(M_START) 모니터** → display-period 카운트다운 | H |

`F_CPU=8MHz`, 모든 타이머 prescaler `/1024` (128µs/tick). Timer2/USART/SPI/TWI/AnaComp 전부 비활성(`hw_init`).

---

## 6. V26(OLD) → V30(NEW STM32) 신호 매핑 — 흡수 작업표

| 기능 | V26 ATmega16 핀/net | V30 STM32 net (U2 / pinmap) | 흡수 메모 |
|---|---|---|---|
| 출력/전류 센스 in | PA0 `SENS_OUT` (ADC ch0) | `SENS_OUT` U2.26 + `SENS_CURR` U2.27 | STM32 ADC로 흡수. ch0/ch1 정체 HW 확인 |
| START 명령 in | PA4 `M_START` | `B_START` U2.50 (opto) | STM32 GPIO in (또는 EXTI) |
| RESET 명령 in | PA6 `M_RESET` | `B_RESET` U2.51 | STM32 GPIO in |
| SEEK 명령 in | PA5 `M_SEEK` | `CHK_FREQ` U2.14? (직접대응 불명) | HW 확인 |
| overload out | PC0 `M_OVLD` | `B_OVLD` U2.55 (in) / `CON_OVLD` PB3 (out) | 검출→출력 STM32 내부화 |
| 버저 out | PC6 `BUZ_M16` | `BUZZER` U2.16 | STM32 GPIO/PWM out |
| **OSC 드라이브 (로직)** | `output_level` 누진패턴 (디컴파일러→PORTD dead) | **`OSC_OUT0..4` U2.28/29/33/34/35 → CN2** | **레귤레이션 로직 흡수 + 5비트→OSC_OUT0..4 출력** |
| 솔레노이드 | (SAMD20측 `SOL_DN`) | `SOL_DN` U2.57 | SAMD20 역할이었음 |
| 설정 | `DIP_1/2/3` (PC5/PA2/PA3) | EEPROM/FRAM (DIP 폐지) | Stage B FRAM config로 대체 |
| IPC | PB0/PB1/PA7/PC4/PC7 `U1.xx` | (single MCU — 내부 상태변수) | IPC 자체가 소멸 |

V30 추가 센스: `SENS_UP`(U2.45)/`SENS_DN`(U2.44) — V26에선 SAMD20측이 읽음(page2 확인). STM32가 통합.

---

## 7. 미해소 / HW-verify 우선순위 (다음 단계 입력)

| # | 질문 | 왜 중요 | 검증 방법 |
|---|---|---|---|
| **1** | **OSC 트리거 물리 출력 핀** — V26 ATmega16의 어느 핀이 OSC 보드를 구동? (디컴파일러는 dead PORTD로 라우팅, 회로도 PDF엔 OSC_OUT 라벨 부재) | 흡수의 출력단 = OSC_OUT0..4 매핑 확정에 필요 | OLD 보드 동작 중 ATmega16 전 출력핀 `scope`, 전류레벨과 상관. 또는 OLD 보드 OSC 커넥터 추적 (netlist 없어 육안) |
| **2** | **ADC ch0/ch1 정체** — PA0=SENS_OUT(연결), PA1=미연결(×)인데 코드는 ch1을 50샘플 처리 | 레귤레이션 입력이 무엇인지 (전류? 출력전압?) | `openocd` ADMUX/ADC live read + `scope SENS_OUT/SENS_CURR` |
| 3 | **PC1 충돌** — 코드=출력 / 회로도=미연결 / 사용자=입력 | 핀 역할 모순 해소 | `openocd DDRC`(0xC3 재확인) + `scope PC1` |
| 4 | **SEEK 의미** — `M_SEEK`가 V30 어디로? 주파수 추종(seek)인가? | 기능 누락 방지 | OLD 동작 관찰 + V30 `CHK_FREQ` 대조 |
| 5 | **lookup_table 물리 의미** — 21엔트리 곡선이 전류 한계인지 주파수 매핑인지 | 레귤레이션 정확 포팅 | OLD 보드에서 센스 입력 sweep → 출력 레벨 기록 |
| 6 | comm_sequence_handler / comm_start_sequence IPC 프로토콜 | (out-of-scope) V30은 single-MCU라 소멸 | 불필요 (방향만 기록 완료) |

> ⚠️ HW 검증 시 STM32(V30)는 **BOOT0 forced-jump 워크어라운드** 필요 (R64 미실장). ATmega16은 OLD 보드에서 직접 probe 가능(있을 경우). memory `project_board_boot0_workaround`.

---

## 8. 다음 세션 실행 계획 (흡수 슬라이스 후보)

이 분석이 **ATmega16 흡수 슬라이스**의 evidence base. 코드 슬라이스 정석 흐름(worktree → brainstorm → spec → plan → subagent → HW검증 → 머지).

**권장 진입 순서:**

1. **(선행) HW-verify 패스** — §7의 #1(OSC 트리거 핀), #2(ADC 채널), #3(PC1)을 OLD 보드 또는 V30 보드로 실측해 LOW/M 행을 HIGH로 승급. 이게 없으면 흡수 출력단이 가설 위에 세워짐.
2. **Stage D (가칭) — Ultrasonic regulation core**:
   - 입력: STM32 ADC(`SENS_OUT`/`SENS_CURR`) → `adc_ch0_avg` 등가
   - 처리: `lookup_table` 스케일 → `adc_scaled_value` → 누진 레벨 (`output_level_process` 포팅)
   - 출력: `OSC_OUT0..4` GPIO (U2.28/29/33/34/35), `CON_OVLD` overload, `BUZZER`
   - cadence: STM32 TIM 2ms/10ms tick으로 Timer0/Timer1 ISR 등가 재현 (현 superloop + SysTick 위에)
   - 명령 입력: `B_START`/`B_RESET`(+SEEK) GPIO/EXTI
3. **순서 의존성**: Stage C(Modbus)와 독립. 단 레귤레이션은 안전(overload) 직결이라 **HW 실측 우선** 강제.

**비차단(차후):** 7-세그/display_* 포팅 ✗ (dead). comm IPC 포팅 ✗ (single MCU).

---

## 9. 참조

- 디컴파일 소스: `ref/atmega16/m16_conv_v001.c` (read-only, 비판적 독해 §2)
- OLD 회로도: `hw/schematics/usw_ctrl_v26_samd20.pdf` (page4 = ATmega16 U6; netlist 없음/육안)
- NEW netlist: `hw/schematics/USW_CTRL_V30.asc` (U2 = STM32F410)
- 핀 역할 + 흡수 컨텍스트: memory `project_atmega16_absorption`
- 벤치/플래시 주의: memory `project_board_boot0_workaround`, RESUME §보드 이슈
- kickoff(=spec): `docs/superpowers/2026-05-25-atmega16-analysis-kickoff.md`
</content>
</invoke>
