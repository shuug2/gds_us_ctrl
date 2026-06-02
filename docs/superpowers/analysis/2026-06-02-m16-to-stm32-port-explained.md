# ATmega16 → STM32F410 포팅 설명 (Stage D slice 1)

> **성격**: 설명/요약 문서 (teaching/handoff). M16 레귤레이션 코어가 STM32F410으로 어떻게
> 포팅됐는지를 compute(알고리즘)와 I/O(핀) 두 축으로 정리.
> **권위 소스가 아님** — 사실의 출처는 아래 참조 문서. 본 문서는 그것을 엮어 설명할 뿐.
> **작성**: 2026-06-02. **대상**: 브랜치 `feat/stage-d-regulation-core` 구현분.

## 참조 (사실의 출처)

- 검증된 M16 동작: `docs/superpowers/analysis/2026-05-31-m16-regulation-core-verified.md` (disasm byte-exact fact base)
- M16 I/O 동작 + 보드-진실 정정: `docs/superpowers/analysis/atmega16-io-behavior.md` §0.1
- 핀 매핑 권위 소스: `docs/pinmap.md` (§52–57 ADC, §112–131 OSC, 부록 A IPC)
- 구현 spec/plan: `docs/superpowers/specs/2026-05-31-stage-d-slice1-regulation-core-design.md`,
  `docs/superpowers/plans/2026-05-31-stage-d-slice1-regulation-core.md`
- 구현 소스: `fw/src/app_reg.c`, `fw/src/app_reg_calc.c`, `fw/drivers/adc1.c`,
  `fw/src/board.c`, 통합 `fw/src/main.c`/`app.c`/`app_lcd.c`

---

## 0. 큰 그림 — 무엇이 포팅됐나

핵심 전제: 이건 **"M16 칩의 핀 배치를 1:1로 옮기는 것"이 아니라 "M16이 수행하던 제어 기능
(레귤레이션 루프)을 STM32에 흡수"** 하는 작업이다. 그리고 M16 전체가 아니라
**레귤레이션 코어의 compute 파이프라인 한 조각(slice 1)만** 포팅됐다.

```
[M16 레귤레이션 코어]                    [STM32 slice 1]
2ch ADC 교대 획득  ──────────────────▶  adc1.c (폴링) + app_reg.c
10/50 샘플 평균    ──────────────────▶  app_reg.c reg_acquire_step()
~2.05ms Timer0 틱  ──────────────────▶  app_reg.c app_reg_tick() (2ms 게이트)
×6 clip/floor 스케일 ─────────────────▶  app_reg_calc.c reg_scale()       ← 순수함수
21-rung lookup     ──────────────────▶  app_reg_calc.c reg_output_level()  ← 순수함수
8-level 패턴 → OSC 물리 구동  ─ ✗ ─────▶  DEFERRED (B-SEAM, 벤치 측정 후)
```

HW 검증에서 관측된 `ch0=4 → scaled=24`(정확히 ×6)가 이 파이프라인이 라이브로 도는 증거다.

---

# Part 1 — Compute (알고리즘) 포팅

## 1.1 아키텍처 패러다임 변환

| 측면 | ATmega16 (원본) | STM32F410 (포팅) |
|---|---|---|
| 코어 | AVR 8-bit @ 8 MHz | Cortex-M4F @ 96 MHz |
| 구동 방식 | **인터럽트 구동** (Timer0 OVF ISR + ADC complete ISR) | **슈퍼루프 협조적 폴링** (RTOS·인터럽트 없음) |
| ADC | ISR가 single-shot 재무장, 결과를 SRAM 배열로 | `app_reg_tick()`이 매 틱 `adc1_read()` 직접 폴링 |
| 타이밍 | Timer0 OVF가 자동으로 ISR 깨움 | `sys_tick_get_ms()` 델타로 2ms 게이트 |
| 나눗셈 | 소프트웨어 32-bit `__udivmodsi4` | 하드웨어 정수 나눗셈 |

두 ISR(타이머·ADC)로 돌던 비동기 루프를, **단일 `app_reg_tick()` 함수**가 슈퍼루프(`app.c:78`)에서
2ms마다 한 스텝씩 협조적으로 굴리는 형태로 평탄화했다.

## 1.2 단계별 매핑

### (1) ADC 획득 — `adc1.c` + `app_reg.c reg_acquire_step()`

- **M16** (분석 §3a): ADC complete ISR가 2채널 교대 single-shot. ch0=PA0, ch1=PA1.
  ADCSRA=0xCF(/128), REFS=11(**2.56V** 내부 레퍼런스), **10-bit**.
- **STM32**: `adc1_read(ch)`가 채널 설정 → `HAL_ADC_Start` → `PollForConversion` → `GetValue` (폴링).
  PB0=ADC1_IN8(SENS_OUT), PB1=ADC1_IN9(OSC). **12-bit @ 3.3V**.
- **도메인 변환** (`app_reg.c:18`): `ADC_NORM_SHIFT = 2` → `adc1_read() >> 2`로 12-bit를 10-bit 등가로
  정규화. lookup table이 기대하는 0..1023 도메인을 맞추기 위함.
  > ⚠️ M16은 10-bit@**2.56V**, STM32는 12-bit@**3.3V**라 전압 레퍼런스가 달라 절대 물리 매핑은
  > 다르다. 이건 `spec §10 DP2 first-cut`으로 표시된 1차 근사치 — 벤치 측정 후 보정 대상.
- **교대 로직**: `g_reg.cur_ch`가 0↔1 토글하며 매 틱 한 채널만 변환 (ISR 교대를 폴링으로 재현).

### (2) 평균 — `reg_acquire_step()` (app_reg.c:57–78)

- **M16** (§3a ADC-2/3): 32-bit accumulator + counter, ch0는 10샘플÷10, ch1은 50샘플÷50,
  true 32-bit restoring division.
- **STM32**: `ch0_acc`(uint32)에 누적, `ch0_cnt >= 10`이면 `/10` → `ch0_avg`. ch1은 `/50` → `ch1_avg`.
  동일 산술, 하드웨어 나눗셈.

### (3) cadence — `app_reg_tick()` (app_reg.c:91)

- **M16** (§3d): Timer0 TCNT0=0xF0 reload → 16카운트 → **~2.05ms** @8MHz÷1024. ISR가 매 틱
  scaling+lookup을 **무조건** 호출.
- **STM32**: `REG_TICK_MS = 2`. `app_reg_tick()`은 슈퍼루프에서 매 패스 불리지만,
  `now - prev_ms < 2ms`면 즉시 리턴 → 2ms마다 한 스텝. (96MHz STM32가 ms 단위로 직접 재현 —
  `B-CADENCE` 결정대로.)

### (4) 스케일 — `reg_scale()` (app_reg_calc.c:14)

M16 `adc_calc_scaled_output @0x1A5C`를 **한 줄씩 그대로** 옮긴 것:
```c
if (in >= 1000u) return 1000u;   /* SCALE-04: 입력 ceiling */
if (in < 3u)     return 0u;       /* SCALE-05: 입력 floor */
return (uint16_t)(in * 6u);       /* SCALE-06: ×6 (>>6 아님!) */
```
> 분석의 핵심 정정 포인트. 디컴파일 초안(v001)은 `>> 6`(나누기)로 잘못 읽었는데, raw disasm에는
> shift 명령이 없고 `ldi 0x06` + 위젯 곱셈(`0x2158`)이라 **×6 곱하기**가 맞다. clip/floor도 출력이
> 아니라 **입력**에 건다 (그래서 정상범위 출력은 1000을 넘어 최대 5994까지 갈 수 있음).

### (5) lookup — `reg_output_level()` (app_reg_calc.c:21)

- **M16** (§3c): flash의 21-word 내림차순 테이블에서 **첫 `table[i] < scaled`인 i**를 찾아
  (strictly-less; v001의 `>` 주석은 오류) 8-level thermometer 패턴으로.
- **STM32**: `reg_lookup_table[24]`를 **byte-for-byte** 동일하게 옮기고, 동일한 첫-매치 루프로
  **band index(0..21)만** 반환.
  ```c
  for (i=0; i<21; i++) if (reg_lookup_table[i] < scaled) return i;
  return 21;   /* no match → output off */
  ```
- **의도적 축소** (spec 정제 3건 중 하나): M16의 8-level 패턴 바이트 맵(C-LADDER)은 **포팅 안 함**.
  band index 계산까지만 하고, 그 index를 실제 패턴 바이트→OSC로 내보내는 출력단은 DEFERRED.
  (YAGNI — 출력 경로 자체가 미확정이라 패턴 인코딩을 미리 만들 이유 없음.)

### (6) 출력 (OSC 구동) — DEFERRED

Part 2 §2.1(2) 참조. compute는 band index를 계산해 보관(`g_reg.band`)만 하고 물리 구동은 안 한다.

### (7) 발행 + 통합 — `lcd_measure_t` provider

- `reg_publish_measure()` (app_reg.c:80): `ch0_avg → curr_amp`, `scaled → curr_power/max_power/last_power`.
  나머지(freq/energy/status)는 slice 2/출력단 몫이라 0 유지.
- `app_reg_measure()`가 `const lcd_measure_t*` 제공 → **`app_lcd.c:21`이 LCD provider로 연결**.
  (이전엔 0만 반환하던 스텁 — Stage D가 그 스텁을 실체로 채움.)
- `main.c:25` `app_reg_init()`, `app.c:78` `app_reg_tick()`로 부팅·루프 배선.

## 1.3 포팅 철학

1. **사실 검증 등급 분리**: disasm에서 byte-exact로 증명된 산술(×6, lookup, 평균)만 포팅.
   디컴파일의 함수명·동작주석(`g_run_flag`/"LED"/"blink")은 **무시**. 물리 의미(전달함수 단위,
   OSC 핀 매핑)는 "측정 게이트"로 주석만 달고 코딩은 차단 안 함.
2. **순수함수 분리**: `reg_scale`/`reg_output_level`은 HAL 의존 없는 순수함수(`app_reg_calc.c`)로
   떼어내 **호스트 단위테스트**(`fw/test/`). 페리페럴(`adc1.c`)·상태(`app_reg.c`)와 계산을 격리.
3. **측정 우선(measure-first)**: 출력단은 사용자가 벤치에서 OSC 커넥터를 스코프로 스윕해 실제
   구동 경로를 확인한 뒤 후속 slice에서 pure-add. compute는 그것 없이 완결·검증 가능하게 설계.

---

# Part 2 — I/O (핀) 이관

## 2.0 이관은 두 종류로 갈린다

M16의 핀들은 성격에 따라 완전히 다른 운명을 맞는다:

| 유형 | 무엇 | 이관 방식 |
|---|---|---|
| **A. 물리 신호 재배치** | 외부 회로(센서·OSC보드)로 실제 나가는 아날로그/디지털 | M16 핀 → **STM32 핀** (위치만 이동) |
| **B. IPC 신호 소멸** | 옛 SAMD20↔M16 사이 명령용 GPIO | **핀 자체가 사라짐** → 내부 함수 호출/상태 변수 |

B가 핵심 통찰이다. "MCU 간 통신 = 순수 GPIO 시그널링"이 두 MCU를 하나로 합치면 **불필요해진다.**
두 칩 사이를 잇던 전선이 함수 호출이 되는 것.

## 2.1 유형 A — 물리 신호 재배치 (핀 → 핀)

### (1) ADC 아날로그 입력 (pinmap §52–57, §245)

| M16 원본 | 역할 | STM32 핀 | 상태 |
|---|---|---|---|
| **PA0** (ADC0) | 출력 초음파 세기 피드백 = **레귤레이션 입력** | **PB0 / ADC1_IN8** (SENS_OUT) | ✅ 구현 (adc1.c) |
| **PA1** (ADC1) | ch1 (50샘플 평균, 정체 미확정 B4) | **PB1 / ADC1_IN9** (ADC_OSC) | ✅ 구현 |

> M16은 ADC를 PORTA에 물렸지만 통합 보드에선 PORTB의 ADC1 채널로 재배선. 둘 다
> `GPIO_MODE_ANALOG`로 설정 (adc1.c:11–16).

### (2) OSC 발진회로 제어 5채널 (pinmap §117–131)

출신 핀은 보드 직독으로 확정됐지만, **방향·극성·비트매핑은 미측정**이다:

| STM32 | Net | M16 원본 핀 | M16상 DDR | slice 1 상태 |
|---|---|---|---|---|
| **PB2** | CTRL_OSC0 | mega16 **PB1** | **출력** (DDRB=0xFF) | ✅ idle-HIGH 구동 |
| **PB10** | CTRL_OSC1 | mega16 **PB0** | **출력** | ✅ idle-HIGH 구동 |
| **PB14** | CTRL_OSC4 | mega16 **PC7** | **출력** (DDRC bit7=1) | ✅ idle-HIGH 구동 |
| **PB12** | CTRL_OSC2 | mega16 **PC4** | **입력** (DDRC bit4=0, 풀업) | ⛔ **미설정** (방향 미확정) |
| **PB13** | CTRL_OSC3 | mega16 **PA7** | **입력** (DDRA=0x00, 풀업) | ⛔ **미설정** |

**함정** (분석 §3c C7, B-OSC-MAP): M16에서 PC4·PA7는 **입력 핀**이다. 즉 "5채널 OSC = 5개 push-pull
출력"이라는 단순 가정이 틀릴 수 있다 — 이 둘은 OSC보드→M16 **피드백 입력**일 가능성이 있다.
그래서 board.c는:
- 출력이 확정된 **3채널(PB2/PB10/PB14)만** idle-HIGH로 초기화 (active-LOW이므로 HIGH=OSC off)
- 입력이었던 **2채널(PB12/PB13)은 손대지 않음** — HIGH로 밀면 사용자가 측정으로 결정해야 할
  방향을 임의로 커밋하는 셈이라 의도적으로 미설정

```c
// board.c — 3채널만, 글리치 방지 위해 레벨 먼저 set 후 출력 전환
#define CTRL_OSC_OUT_PINS (GPIO_PIN_2 | GPIO_PIN_10 | GPIO_PIN_14)
HAL_GPIO_WritePin(GPIOB, CTRL_OSC_OUT_PINS, GPIO_PIN_SET);  // idle = HIGH = OSC off
```

> **극성 정정**: M16 Timer0 ISR이 3 flag을 **active-LOW/idle-HIGH**(flag set→핀 LOW)로 미러하는
> 걸 disasm으로 확정(§4.1). 옛 board.c는 `GPIO_PIN_RESET`(LOW)으로 초기화 = **부팅 시 OSC ON**
> 버그였는데, 이번에 idle-HIGH로 바로잡음. (HW 검증의 "PB2/PB10/PB14 idle-HIGH"가 이걸 보는 항목.)
>
> **마스크 정정**: 옛 board.c 마스크는 PB2 누락 + PB15 군더더기였음 (분석 §6 board.c bug #1).
> 정정 후 출력-init 쓰기는 확정 3채널만 덮는다.

### (3) Overload 출력

| M16 원본 | STM32 | 상태 |
|---|---|---|
| **PC0** (overload 출력) | **PB3 / CON_OVLD** | 현재 Phase 2 heartbeat 임시 점유, 본용도(과부하 출력) 복원 예정 — slice 2 |

## 2.2 유형 B — IPC 신호 소멸 (핀 → 내부 호출)

옛 시스템에서 **SAMD20 ↔ M16**는 단 4개 GPIO로 통신했다 (pinmap 부록 A). 통합 후 이 핀들은
**신규 보드에 존재하지 않는다** — 전부 내부 함수/상태로 대체:

| 옛 신호 | 방향 | SAMD20 핀 | M16 핀 | STM32 통합 후 |
|---|---|---|---|---|
| `M_START` | SAMD20→M16 | PB13(out) | **PA4**(in) | 내부 호출 `app_lcd_hook_us_command(START)` |
| `M_SEEK` | SAMD20→M16 | PB11(out) | **PA5**(in) | 내부 호출 (SEEK) |
| `M_RESET` | SAMD20→M16 | PB12(out) | **PA6**(in) | 내부 호출 (RESET) |
| `M_OVLD` | M16→SAMD20 | PB10(in) | **PC0**(out) | 내부 상태 변수 (overload flag) |

> ⚠️ 헷갈리기 쉬운 점: 부록 A의 **SAMD20 핀 번호(PB13/PB11/PB12/PB10)는 옛 SAMD20 칩의 핀**이지
> STM32F410 핀이 아니다. 신규 보드에선 그 물리 위치가 **다른 신호로 재할당**됐다 (pinmap §218–223).
> 분석 문서가 "pinmap 부록 A의 PB13/PB12 = SAMD20 핀, F410 핀 아님 — F410 충돌 없음"이라고 못박은
> 이유다.

이 명령 경로는 slice 1이 아니라 **slice 2(명령 상태머신)** 몫이고, 현재는
`app_lcd_hook_us_command()` 등 훅 스텁만 배선돼 있다.

## 2.3 비포팅 — 사라진 I/O

| M16 I/O | 운명 |
|---|---|
| **PORTD** (7-seg multiplex, 8핀) | **비포팅**. V30 보드에 7-세그먼트 없음 = 물리적 dead. STM32는 DGUS LCD 단독 |
| PORTB[2:4] (7-seg 자리선택 strobe) | 비포팅 (위와 동반) |

---

# Part 3 — slice 1 구현 현황 + 미해소

## 3.1 I/O 구현 현황 요약

```
✅ 구현됨:
   PB0/PB1        ADC analog 모드                       (adc1.c)
   PB2/PB10/PB14  OSC 3채널 idle-HIGH 안전 초기화        (board.c)

⛔ DEFERRED (벤치 측정 = B-OSC-MAP / B-SEAM):
   PB12/PB13      OSC 2채널 — M16상 입력, 방향·극성 미확정 → 미설정
   OSC 실제 구동 로직 (레벨→핀 바인딩, 비트매핑, 활성극성)

🔜 slice 2:
   PB3/CON_OVLD   overload 출력 본용도 복원
   us_command     명령 경로 (옛 M_START/SEEK/RESET의 내부 호출 실체)
```

## 3.2 미포팅/후속 슬라이스 (compute 측)

| 항목 | 상태 |
|---|---|
| OSC 물리 구동 (레벨→핀 바인딩) | DEFERRED (B-SEAM, 벤치 측정 후) |
| 명령 상태머신 (us_start/seek/reset 내부 FSM) | slice 2 |
| Overload 검출/플래그 | slice 2 |
| Soft-start 램프 (10-rung, Timer1 ~10ms) | slice 2 |
| 7-seg/`display_*` PORTD multiplex | 비포팅 (dead) |
| comm IPC (옛 SAMD20↔M16 통신) | 비포팅 (단일 MCU) |

## 3.3 핵심 미해소 — B-OSC-MAP (왜 일부만 했나)

이미지(M16 펌웨어 바이너리)로는 풀 수 없는 것:
- 5채널 각 핀의 **방향** (출력 vs OSC보드 피드백 입력)
- **활성 극성** (3채널만 active-LOW 증명, 나머지 2채널 미상)
- **레벨↔OSC 비트 매핑** (계산된 레벨이 어느 핀 조합으로 나가는지)

AVR 기계어에는 "net-to-channel" 정보가 없다. 사용자가 보드에서 OSC 커넥터를 스코프로 스윕해
실제 구동 경로를 확인해야 출력 5채널 전체를 확정할 수 있고, 그게 slice 1의 출력단을 DEFERRED로
둔 이유다. (HW 검증의 "OSC 무구동 / PB12·PB13 입력 유지" 확인이 이 안전 경계를 검증.)

---

## 한 줄 요약

**물리 신호(ADC 2 + OSC 5)는 핀 재배치, 옛 MCU간 IPC(명령 4선)는 내부 호출로 소멸, 7-seg는 폐기.**
**Compute는 "ADC 피드백 → ×6 스케일 → 테이블 룩업 → 레벨" 계산 심장부를 byte-exact로 흡수**하고
그 결과를 LCD 측정값으로 발행하는 데까지가 slice 1. OSC 5채널 중 방향이 확정된 3개만 안전하게
초기화하고, 실제 구동(B-SEAM)은 측정 게이트로 남긴 상태.
