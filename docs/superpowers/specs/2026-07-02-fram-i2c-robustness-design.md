# fram-i2c-robustness (감사 D3 = H3+H2) — 설계

> **요약**: 2026-07-02 전면 감사 HIGH 2건을 한 슬라이스로 수정한다. **H3** — `fram_read_*`가 I2C 실패를 무시하고 0을 반환해 config 필드가 조용히 0으로 로드되는 문제(`limit_on_time=0`/`limit_out_time=0`→즉시 OVTIME/`output_power=0`)를 **읽기 status 전파 + 실패 필드별 팩토리 기본값 폴백 + mon 경고**로 수정. 특히 현재 코드는 INIT_FLAG *읽기 실패*를 "빈 FRAM"으로 오인해 **정상 FRAM을 팩토리로 덮어쓰는 데이터 손실 경로**가 있어 이를 차단한다. **H2** — I2C1 버스 stuck(SDA low 고착) 복구 전무 문제를 **init 시 1회 SCL 9클럭 bus-unstick**으로 수정하고, 소비처 0인 `i2c1_err_count()`를 **mon 로그로 표면 배선**한다. 경고 표면 = **mon 전용**(사용자 확정 2026-07-02; LCD/Modbus 표면 없음), unstick = **init 1회만**(런타임 재시도 없음). 폴백/로드 로직은 mock-fram link 치환으로 **host-test 신규 스위트**, unstick·실버스는 HW 회귀만.

## 1. 배경 / 감사 근거 (HANDOFF.md 2026-07-02)

- **H3** (`fram.c:5-25` + `app_config.c:79-122`): `fram_read_*`는 `i2c1_mem_read` 실패 시 지역 버퍼 `{0}`을 그대로 반환 → `app_config_load`가 실패 필드를 **조용히 0으로 로드**. 전체 고장(INIT_FLAG도 실패→0≠0xAA)은 factory reset 경로로 "안전"해 보이지만 그 경로 자체가 **일시적 버스 오류에서 정상 FRAM 전체를 팩토리로 덮어씀**(factory_write→save_all). **부분 실패가 최위험**: `limit_on_time=0`(ceiling 즉시), `limit_out_time=0`(energy 모드 즉시 OVTIME — ovtime 스테이지에서 0=즉시 fault로 확정된 semantics), `output_power=0`(진폭 언더플로 가드로 0 진폭).
- **H2** (`i2c1.c:47-65`): slave가 SDA를 물고 있는 stuck 상태 복구 전무. FRAM+I2C_POT 버스 공유 → 동반 마비, `app_config_save_all`(write 29회)은 실패당 `I2C1_TIMEOUT_MS`(50ms) 블로킹 = 최대 ~1.5s 슈퍼루프 정지. `i2c1_err_count()`는 존재하나 **소비처 0**(죽은 observability).

## 2. 목표 / 범위 (사용자 확정 2026-07-02)

| 항목 | 결정 |
|---|---|
| 실패 필드 처리 | **필드별 팩토리 기본값 폴백** (전체 로드 포기 아님) |
| 경고 표면 | **mon 로그만** (LCD_WARNING ✗ / Modbus status bit ✗ — HMI 계약 불변) |
| bus-unstick 시점 | **`i2c1_init` 1회만** (런타임 임계치 재시도 ✗ — superloop 중 버스 재초기화 개입 리스크 회피) |
| 쓰기(`fram_write_*`) | **변경 없음** (best-effort 유지, i2c-pot 슬라이스와 동일 패턴; 관측은 err_count) |
| H4(ADC Error_Handler+IWDG) | **범위 밖** — 별도 슬라이스 (감사 결정 그대로) |

## 3. 아키텍처

### 3.1 `fram.{h,c}` — 읽기 시그니처 변경 (status 전파)

```c
bool fram_read_byte(uint8_t addr, uint8_t  *out);   /* true = HAL_OK */
bool fram_read_u16 (uint8_t addr, uint16_t *out);   /* big-endian 유지 */
bool fram_read_u32 (uint8_t addr, uint32_t *out);
```

- 실패 시 `*out` 미기록(호출자가 선적용한 기본값 보존) — "기본값 선적용 → 성공만 덮어쓰기" 패턴(§3.2)과 맞물림.
- 헤더 `#include <stdbool.h>` 추가. `fram.h`는 HAL-free 유지(host mock 가능 조건).
- **호출자 전수 = `app_config.c`뿐** (2026-07-02 grep 실측: read 계열 외부 호출자 없음. write 계열은 app_config/app_weld/app_modbus/app_lcd_input이 `app_config_save_all` 경유 — 무변경).

### 3.2 `app_config.c` — defaults 분리 + 폴백 로드

**분리**: 현 `app_config_factory_write` = "기본값 채움 + save_all"이 한 몸 → 분리:

```c
void app_config_factory_defaults(app_config_t *cfg);   /* RAM만 채움 (신규, 기존 본문 이동) */
/* app_config_factory_write = factory_defaults + save_all (기존 시그니처 유지) */
uint8_t app_config_load(app_config_t *cfg);            /* void → uint8_t: 실패 필드 수 */
```

**로드 알고리즘** (기존 필드 순서·클램프 semantics 유지):

1. `app_config_factory_defaults(cfg)` **선적용** — 이후 실패 필드는 자동으로 기본값 유지(기본값 테이블 중복 없음).
2. `INIT_FLAG` 읽기:
   - **읽기 실패** → `return 0xFFu` (전 필드 기본값 상태, **factory-write 하지 않음** — 일시 버스 오류로 정상 FRAM을 덮어쓰는 현행 데이터 손실 경로 차단. FRAM 내용 보존, RAM은 기본값으로 동작).
   - 읽기 성공 & `!= 0xAA` → 현행 유지(진짜 빈 FRAM): flag 기록 + `app_config_factory_write(cfg)` + `return 0`.
   - 읽기 성공 & `== 0xAA` → 3.으로.
3. 필드별: `if (fram_read_u16(FRAM_ADDR_X, &cfg->x)) {} else { fail++; }` — 실패 필드는 1.의 기본값 그대로. bool 필드(`energy_ctrl`/`multi_ctrl`)와 캐스팅 필드(`cal_val` 등)는 임시 변수 경유.
4. `limit_energy > 100000` 클램프(samd20 `main.c:923`)는 **읽기 성공 시에만** 평가·write-back (실패 시 기본값 100000이라 클램프 불요, 죽은 버스에 write 회피).
5. `return fail;` (0 = clean).

**반환 semantics**: `0`=전 필드 정상, `1..N`=부분 실패(해당 수만큼 기본값 동작), `0xFF`=INIT_FLAG 자체 실패(전체 기본값, FRAM 미변경). 필드 수(26회 read)는 0xFF와 충돌 불가.

### 3.3 `i2c1.c` — init 1회 bus-unstick (H2)

`i2c1_init()`에서 **AF 설정 전** GPIO 프리플라이트:

1. PB7(SDA)=input, PB6(SCL)=GPIO open-drain output(high)으로 임시 설정.
2. SDA가 **low**면: SCL을 최대 **9클럭** 토글(각 반주기 ~5µs busy-loop — `sys_tick`은 이 시점 미가동이므로 NOP 루프, 96MHz 보정), 매 클럭 후 SDA 확인, high 복귀 시 중단.
3. SDA high 복귀 시 **STOP 조건 생성**(SDA를 OD output low → SCL high 상태에서 SDA release).
4. 결과 latch: `static uint8_t s_unstick_events;` (0=불필요/깨끗, 1..9=클럭 수, 0xFF=9클럭 후에도 SDA low = 복구 실패 — 이후 HAL init은 그대로 진행, 실패는 err_count로 드러남).
5. PB6/PB7을 기존대로 AF4 OD로 재설정 → 기존 HAL init 시퀀스 그대로.

```c
uint8_t i2c1_unstick_events(void);   /* 신규 getter — app.c boot 로그가 소비 */
```

- `i2c1_init()`은 `main.c:24`에서 `mon_init()` **이전**에 호출됨 → unstick 결과는 즉시 출력 불가, latch 후 app 부팅 로그(§3.4)에서 출력.

### 3.4 mon 표면 배선 (경고 = mon 전용)

- **부팅 1회** (`app.c` 기존 `[cfg]` 로그 직후):
  - `[cfg] fram_fail=%u unstick=%u i2c_err=%u` — load 반환값 + `i2c1_unstick_events()` + `i2c1_err_count()`.
  - `fail > 0`이면 추가 경고 줄: `[cfg] WARN: defaults active for %u field(s)` (0xFF면 `ALL fields, FRAM untouched`).
- **런타임**: app 슈퍼루프의 기존 1s cadence 지점에서 `i2c1_err_count()` 델타 감지 시 `[i2c] err=%u (+%u)` 1줄 (save_all/POT write 실패 관측용; 정확한 훅 지점은 plan에서 고정).
- ⚠ 벤치에서 Modbus RTU가 USART6 점유 시 mon 비가용([[feedback-swd-halt-breaks-board-validation]]) — mon-only 표면의 알려진 한계로 **사용자 수용**(관찰 필요 시 LCD에서 addr=NONE).

## 4. 에러 처리 semantics 표

| 상황 | 현행 | 변경 후 |
|---|---|---|
| 필드 read 실패 | 조용히 0 로드 | 해당 필드 팩토리 기본값 + fail 카운트 + mon 경고 |
| INIT_FLAG read 실패 | **factory_write로 FRAM 전체 덮어씀** | FRAM 미변경, RAM 전체 기본값, `0xFF` 반환 + mon 경고 |
| INIT_FLAG ≠ 0xAA (read 성공) | factory_write | 동일 (진짜 빈 FRAM — 유일하게 안전한 factory 경로) |
| write 실패 | err_count만 | 동일 (변경 없음) + 런타임 mon 델타 로그로 관측 가능 |
| SDA stuck @boot | HAL init 후 전 트랜잭션 timeout | init 전 9클럭 복구 시도, 결과 mon 로그 |

## 5. 동시성

- I2C1 접근은 슈퍼루프 단독(ISR 0 — i2c-pot 슬라이스에서 검증 완료) → 락 불요. unstick은 HAL init 전 단일 실행 컨텍스트.

## 6. 테스트

### 6.1 host-test 신규 스위트 `test_app_config` (핵심 게이트)

`app_config.c`는 `app_config.h`+`fram.h`만 include(HAL-free) → **mock fram.c link 치환**으로 host 컴파일. mock: 64B 배열 + 주소별 실패 주입 세트(`mock_fram_fail(addr)`).

| # | 시나리오 | 기대 |
|---|---|---|
| 1 | 전 필드 성공 (magic OK) | 반환 0, 저장값 그대로 로드 (현행 무회귀) |
| 2 | magic ≠ 0xAA (read 성공) | factory_write 발생(mock write 로그), 반환 0, 기본값 |
| 3 | INIT_FLAG read 실패 | **mock write 0건**(FRAM 미변경 입증), 반환 0xFF, 전 필드 기본값 |
| 4 | 부분 실패 (예: ON_TIME·OUT_POWER만 fail) | 해당 2필드만 기본값(50/50), 나머지 저장값, 반환 2 |
| 5 | limit_energy > 100000 (read 성공) | 클램프 + write-back 1건 (현행 무회귀) |
| 6 | limit_energy read 실패 | 기본값 100000, **write-back 0건** |

- 기존 스위트 6개 무회귀 + 0-warning 빌드(타깃 컴파일에서 시그니처 전파 완결 입증 — 호출자가 app_config.c뿐이라 컴파일 에러=전파 누락 검출).

### 6.2 HW 회귀 (board-gated, 이연 — 정상 경로만)

- 정상 부팅 무회귀: LCD 값 육안(output_power 등 저장값 유지) + mon(가용 시) `fram_fail=0 unstick=0`.
- 직접런 ceiling 무회귀(mbpoll STATUS 1×N→0) — app_config_load 경로 변경이 런타임에 무영향 입증.
- fault 주입(버스 단선/SDA 고착)은 벤치 리스크 대비 이득 낮아 **범위 밖** (로직은 host 스위트가 게이트).

## 7. 범위 밖 (이연)

- **H4** ADC `Error_Handler` 영구 lock + IWDG — 별도 슬라이스(감사 D3 결정).
- 쓰기 status 전파 / save_all 실패 시 롤백 — best-effort 유지.
- LCD_WARNING·Modbus status bit 경고 표면 — mon-only 사용자 확정. HMI 착수 시 재검토 여지.
- M3(comm_speed_idx/parity_idx 무클램프 OOB)·M5(FRAM 레이아웃 버전) — D2/slice4 및 후속.
- 런타임 unstick(임계치 재시도) — init 1회 확정.

## 8. 구현 단위 요약 (plan 입력)

| 파일 | 변경 | host-test |
|---|---|---|
| `fw/include/fram.h` | read 3종 `bool(addr, *out)` 시그니처 + stdbool | ✓ (mock 대상 계약) |
| `fw/drivers/fram.c` | read 3종 status 전파(실패 시 *out 미기록) | ✗ (타깃; mock이 대행) |
| `fw/include/app_config.h` | `factory_defaults` proto 추가, `load` 반환 uint8_t | ✓ |
| `fw/src/app_config.c` | defaults 분리 + 선적용-덮어쓰기 로드 + INIT_FLAG 실패 격리 + 반환 | ✓ (mock fram link) |
| `fw/drivers/i2c1.c` + `fw/include/i2c1.h` | init 프리플라이트 unstick(9클럭+STOP) + `i2c1_unstick_events()` | ✗ (HW 회귀만) |
| `fw/src/app.c` | 부팅 `[cfg]` 로그 확장 + WARN 줄 + 1s err 델타 로그 | ✗ |
| `fw/test/test_app_config.c` + mock fram (신규) | §6.1 시나리오 6종 | ✓ |
| `fw/test/Makefile` | 신규 스위트 추가 + **stale 주석 정정 동승**(1-3행 "pure layers 2종" 서술이 현 6스위트와 불일치 — 감사 부수 결정) | — |

⚠ 신규 `.c` 없음(테스트 제외) → 타깃 CMake reconfigure 불요(기존 GLOB 파일만 수정). 단 브랜치 전환 시 기존 규칙([[project-phase12-env]]) 유지.
