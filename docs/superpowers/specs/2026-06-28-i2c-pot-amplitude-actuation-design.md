# I2C_POT 진폭 actuation 통합 — 설계

> **요약**: 진폭을 결정하는 두 hook(`app_lcd_hook_set_pot`=output_power %, `app_weld_hook_set_amp`=raw DAC)이 현재 둘 다 로그 stub이다. 이를 **공용 I2C_POT 구동 함수 1개**(`drivers/i2c_pot.c`)로 통합해 U4 디지털 포텐셔미터(@0x28)에 실제 wiper 값을 쓴다. `set_pot`은 % → DAC 순수 변환(언더플로 가드 포함) 후, `set_amp`은 raw DAC를 그대로 구동 함수에 넘긴다(double-convert 회피, legacy 충실). 부팅 시 config(output_power) 로드 직후 초기 진폭 1회 적용(samd20 `main.c:910` 재현). 변환 로직은 순수 함수로 **host-test 신규 스위트**, 실제 I2C write는 **HW-gated**. 기존 I2C1 드라이버(`i2c1_mem_write`, FRAM이 사용 중)와 버스 공유 — 슈퍼루프 단독 호출이라 경합 없음.

## 1. 배경 / legacy 근거

samd20 `main.c`는 진폭을 항상 단일 디바이스 `i2c_adc_write_byte(I2C_POT=0x28, I2C_POT_WP=0x00, value)`로 쓴다. write 사이트는 12곳이며 값 계산 도메인에 따라 F410의 두 hook으로 갈린다:

| samd20 line | 맥락 | 값 | → F410 hook |
|---|---|---|---|
| 910 | 부팅 init | `(output_power-50)*255/100` | set_pot |
| 1376 | START/REMOTE run start | multi→out1 / else output_power | (multi)set_amp / else set_pot |
| 1549 | WELD run start | multi→out1 / else output_power **+comp_time 보정** | set_amp |
| 3050 | LCD 셋업페이지 진입(프리뷰) | output_power | set_pot |
| 3323 | LCD 저장(MULTI 계열) | output_power | set_pot |
| 3399 | LCD 저장(HAND 계열) | output_power | set_pot |
| 3441 | LCD 저장(STD 계열) | output_power | set_pot |
| 3689 | LCD RUN(touch) press (multi 경로 주석처리) | output_power | set_pot |
| 3817 | (주석 = dead) | — | — |
| 4401 | Modbus START | output_power | set_pot |
| 4641 | Modbus START(2nd) | output_power | set_pot |
| 5242 | 다단 스테핑 mid-run | out2 | set_amp |

→ **set_pot(output_power %) = 9곳**(부팅·LCD 프리뷰/저장/RUN press·Modbus START×2) = 정상 동작 진폭 actuation 전부. **set_amp(raw DAC) = 3곳**(다단 out1/out2·WELD comp_time)으로 값이 DAC 도메인에서 계산되는 multi/weld 한정. 둘 다 같은 0x28을 때린다.

**진폭과 모드·다단 연동 (samd20 `main.c` 트레이스)**: 진폭 source는 런타임 플래그 `multi_ctrl`(EN_MULTI, `918-922`)·`energy_ctrl`(EN_ENERGY)가 가른다. RUN 시작(`1367-1376`/`1537-1549`)에서 다단 ON이면 out1, 아니면 output_power; mid-run(`5234-5242`) 다단이면 time1 경과 시 out2로 전환. `model_type`(hand=0/multi=1/standard=2, `1299-1303`)은 LCD 페이지·`sys_mode` 선택용. 종료 우선순위 multi>energy>시간. (이 결정 로직은 weld 슬라이스1~3에서 FSM으로 이미 포팅됨 — 본 슬라이스는 그 결과 DAC를 **실제 칩에 쓰는 마지막 고리**.)

## 2. 목표 / 범위 (확정)

- **범위 = 통합** (사용자 확정): set_pot·set_amp 둘 다 공용 I2C_POT 구동 함수로 연결. set_amp만 연결 시 12곳 중 9곳(정상 진폭)이 죽으므로 통합.
- **부팅 초기 진폭 포함** (사용자 확정): config 로드 직후 1회.
- **접근법 A** (사용자 확정): 전용 드라이버 `drivers/i2c_pot.{c,h}` 신설. (대안 B=각 hook 인라인/상수 중복, C=board.c 혼입 — 기각.)

## 3. 아키텍처 / 컴포넌트

레이어링(기존 정합): **i2c1.c(raw 버스) → i2c_pot.c(디바이스) → app hooks(정책)**.

```
[app_lcd set_pot(power%)] ─┐                       pure (host-test)
                           ├─ pot_dac_from_power(power) ─┐
[boot: set_pot(cfg)]    ───┘                             │ dac
                                                          ▼
[app_weld set_amp(dac, comp보정됨)] ──── dac ────────► i2c_pot_set_dac(dac)   ← HW-gated
                                                          │ clamp(>127→127)
                                                          ▼
                                            i2c1_mem_write(0x28, 0x00, &dac, 1)
                                                          ▼
                                                    U4 I2C_POT @0x28
```

### 3.1 신규 `drivers/i2c_pot.{c,h}`

**`i2c_pot.h`** — HAL-free (`<stdint.h>`만 include) → host에서 그대로 include 가능:
- 상수: `#define I2C_POT_ADDR 0x28u` / `#define I2C_POT_REG 0x00u` (samd20 `I2C_POT`/`I2C_POT_WP` 재현)
- `void i2c_pot_set_dac(uint8_t dac);` (HW 의존, 본문은 .c)
- `static inline uint8_t pot_dac_from_power(uint8_t power);` (순수, §4 공식 — 헤더 인라인이라 host-test가 헤더만 include하면 검증 가능)

**`i2c_pot.c`** — HAL 의존:
- `i2c_pot_set_dac(uint8_t dac)`: `dac > 127 → 127` 상한 클램프 후 `i2c1_mem_write(I2C_POT_ADDR, I2C_POT_REG, &dac, 1)`. **best-effort**(반환 무시; 관측은 `i2c1_err_count()`).
- 별도 init 없음 — 버스는 `i2c1_init()`이 부팅 초기에 이미 올림.

### 3.2 기존 hook 수정 (stub 본문 교체 — 시그니처 불변)

- `app_lcd.c::app_lcd_hook_set_pot(uint8_t output_power)`
  → `i2c_pot_set_dac(pot_dac_from_power(output_power));` + 기존 `mon_printf` 로그 유지.
- `app_weld.c::app_weld_hook_set_amp(uint8_t dac)`
  → `i2c_pot_set_dac(dac);` + 기존 `mon_printf` 로그 유지. (comp_time 보정은 **호출자(weld FSM)가 이미 적용한 값** — 본 슬라이스는 변환 안 함.)

### 3.3 부팅 초기 진폭

- config(`output_power`) 로드 완료 **직후 1회** `app_lcd_hook_set_pot(cfg->output_power)` 호출 (samd20 `main.c:910` 재현). 
- 정확한 호출 지점은 plan에서 config 로드 위치(현재 `app_init` 단계에서 `app_config_load`)에 맞춰 고정. 초음파 출력은 별도 게이트(USOUT/`board_osc4`)라 부팅 시 wiper만 세팅되는 것은 무해(legacy 동일).

## 4. 변환 공식 (순수, 확정 — 언더플로 가드 둠)

```c
static inline uint8_t pot_dac_from_power(uint8_t power) {
    if (power <= 50u)  return 0u;     /* 언더플로 가드 (samd20는 무가드; 49↓ 거동만 차이) */
    if (power >= 100u) return 127u;   /* 상한 = (100-50)*255/100 = 127 */
    return (uint8_t)(((uint16_t)(power - 50u) * 255u) / 100u);
}
```

- 50~100 구간은 samd20 `(power-50)*255/100`과 **비트-동일**. 경계만 포화 추가.
- `power<=50 → 0` 가드로 **디바이스 진폭 언더플로 제거**(LOW-1 must-fix가 노출하던 음수 → uint8 거대값 회피).
- `set_amp`은 이 변환을 **거치지 않음**(raw DAC) → double-convert 회피(legacy 충실).

## 5. 에러 처리

- **best-effort**: I2C NACK/타임아웃 시 블로킹·재시도 없음(슈퍼루프 stall 회피). `i2c1_mem_write`가 실패 시 `s_err_count` 증가 → `i2c1_err_count()`로 관측. samd20도 반환 무시. 기능 폴백 없음(진폭은 다음 트리거에서 갱신).

## 6. 동시성

- I2C_POT·FRAM 모두 **슈퍼루프 단독 호출**(ISR 접근 0 — 검증: FRAM 호출자는 `app_config.c` load/save뿐, 전부 슈퍼루프). I2C1 버스 직렬화, 락 불요.
- 부팅: config 로드(FRAM read) → 그 후 진폭 write 순차.

## 7. 테스트

### 7.1 host-test (모드 b, 즉시 가능) — 신규 스위트 `test_i2c_pot` (확정)
- 대상: `pot_dac_from_power` (헤더 인라인 include).
- 경계: `50→0`, `100→127`, `49(<50)→0`(언더플로 가드), `120(>100)→127`(상한), 중간 `75→63`.
- `fw/test`에 신규 스위트로 추가(기존 reg_calc 편입 아님 — 사용자 확정). 빌드 0-warning + 기존 스위트 무회귀.

### 7.2 HW E2E (board-gated, 이연)
- **선결: U4가 0x28에서 ACK하는지 먼저 확인.** 미실장/오주소면 모든 `set_pot`/`set_amp` 호출이 `I2C1_TIMEOUT_MS`(50ms) 동안 슈퍼루프를 블로킹하고 `i2c1_err_count()`만 증가한다(증상=주기적 50ms stall). 부팅 1회 write + 첫 RUN에서 `i2c1_err_count()` 0인지 확인.
- 스코프/멀티미터: ① LCD/Modbus로 output_power 변경 → I2C_POT wiper 전압 변화 ② weld out1→time1→out2 진폭 전환 ③ 부팅 초기값 적용. 실제 초음파 진폭이 설정 추종하는지.

## 8. 범위 밖 (이연)

- **comp_time 보정** — weld FSM 책임(set_amp에 보정된 DAC 유입). 본 슬라이스 무관.
- **LOW-1 LCD 입력 클램프** (`app_lcd_input.c:752` output_power<50) — §4 변환 가드로 **디바이스 진폭 언더플로는 제거**되나, 저장/표시값 자체 클램프는 weld 슬라이스4 must-fix로 유지(별개 이슈).
- **칩 정체(DS1803 등)·wiper 물리 스케일(0–127 vs 0–255)·극성** — HW/6b calibration 확정. (F2 = "진폭은 I2C_POT로 제어"라는 **기능적 역할만** 사용자 확정으로 해소; 칩 정체·스케일은 여전히 HW/6b 이연 — "종결" 아님.)

## 9. HW-gated 표시

- host-test로 검증되는 것 = **변환 로직(§4)뿐**. 실제 I2C write·진폭 추종(§7.2)은 보드 게이트. 모드 b(HW 없이 설계+host-test)에 정합 — actuation E2E는 실배선 보드 세션으로 분리.

## 10. 구현 단위 요약 (plan 입력)

| 파일 | 변경 | host-test |
|---|---|---|
| `fw/include/i2c_pot.h` (신규) | 상수·`i2c_pot_set_dac` proto·`pot_dac_from_power` 인라인 | ✓ (인라인) |
| `fw/drivers/i2c_pot.c` (신규) | `i2c_pot_set_dac` (clamp + i2c1_mem_write, best-effort) | ✗ (HAL/HW) |
| `fw/src/app_lcd.c` | `set_pot` 본문 = 변환→set_dac (+로그) | ✗ |
| `fw/src/app_weld.c` | `set_amp` 본문 = set_dac (+로그) | ✗ |
| 부팅 경로 (`app_init` 직후) | config 로드 후 `set_pot(cfg->output_power)` 1회 | ✗ |
| `fw/test/test_i2c_pot.*` (신규) | `pot_dac_from_power` 경계 | ✓ |
| `fw/CMakeLists.txt` | 신규 `.c` 추가 시 reconfigure (`cmake -S fw -B build`) | — |
