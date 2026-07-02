# fram-i2c-robustness (감사 D3 = H3+H2) Implementation Plan

> **문서 요약**: 감사 HIGH 2건 수정 계획 — H3(`fram_read_*` status 전파 + config 필드별 팩토리 폴백 + INIT_FLAG 읽기실패 시 factory-write 금지) + H2(I2C1 init 1회 SCL 9클럭 bus-unstick + `i2c1_err_count()` mon 표면 배선). Task 4개: ①H3 코어(TDD, mock-fram 신규 스위트) ②H2 unstick ③mon 표면 배선 ④docs. spec = `docs/superpowers/specs/2026-07-02-fram-i2c-robustness-design.md`.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** FRAM 읽기 실패가 조용히 0으로 로드되는 결함(H3)과 I2C1 버스 stuck 무복구(H2)를 수정하고, 관측을 mon 로그로 표면화한다.

**Architecture:** `fram_read_*` 3종을 `bool (addr, *out)` status-전파 시그니처로 변경(실패 시 `*out` 미기록). `app_config_load`는 팩토리 기본값을 **선적용** 후 읽기 성공 필드만 덮어쓰고 실패 수를 반환 — INIT_FLAG *읽기 실패*는 빈 FRAM과 구분해 **factory-write를 하지 않는다**(일시 버스 오류로 정상 FRAM 전체를 덮어쓰는 현행 데이터손실 경로 차단). `i2c1_init`은 HAL init 전에 SDA stuck 감지 시 SCL 9클럭+STOP 복구를 1회 시도. 경고/관측 표면은 mon 전용(사용자 확정).

**Tech Stack:** STM32 HAL(타깃) / C11 host 테스트(mock-fram link 치환, `fw/test/Makefile` 패턴).

## Global Constraints

- 타깃 빌드: `env -u STM32_TOOLCHAIN cmake -S fw -B fw/build -G Ninja && env -u STM32_TOOLCHAIN cmake --build fw/build` — **0-warning 필수** (우리 코드).
- host 테스트: `make -C fw/test test` — 기존 6스위트 무회귀 + 신규 `test_app_config`.
- `fw/vendor/` 및 `ref/` 수정 금지. CubeMX UI 미사용.
- 신규 `.c`는 `fw/test/`에만 생김(타깃 소스 신규 없음) → 타깃 CMake reconfigure 불요.
- 쓰기(`fram_write_*`) 시그니처는 **변경 금지**(best-effort 유지 — 사용자 확정).
- 경고 표면 = mon 전용. LCD_WARNING / Modbus status bit **추가 금지**(HMI 레지스터 계약 불변 — 사용자 확정).
- 브랜치: `feat/fram-i2c-robustness` (main에서 분기). 머지/태그는 HW 회귀 후(기존 정책).
- 커밋 메시지 = conventional commits, 한국어 본문 가능.

---

### Task 0: 브랜치 생성 + sanity

**Files:** 없음 (git만)

- [ ] **Step 1: sanity + 브랜치**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
git status --short           # ?? ref/signal/ 만 기대
make -C fw/test test         # 6스위트 PASS 기대
git checkout -b feat/fram-i2c-robustness
```

---

### Task 1: H3 — fram 읽기 status 전파 + app_config 폴백 로드 (TDD, mock-fram)

**Files:**
- Modify: `fw/include/fram.h:52-54` (read 3종 시그니처)
- Modify: `fw/drivers/fram.c:5-25` (read 3종 구현)
- Modify: `fw/include/app_config.h:27-30` (`factory_defaults` proto 추가 + `load` 반환형)
- Modify: `fw/src/app_config.c` (defaults 분리 + 로드 재작성)
- Create: `fw/test/mock_fram.h`, `fw/test/mock_fram.c`
- Create: `fw/test/test_app_config.c`
- Modify: `fw/test/Makefile` (신규 스위트 + 1-3행 stale 주석 정정)

**Interfaces:**
- Consumes: `i2c1_mem_read/write(dev7, mem_addr, buf, n) → HAL_StatusTypeDef` (기존, 변경 없음 — mock은 fram.c 전체를 치환하므로 host에서 미사용)
- Produces (Task 3이 사용):
  - `bool fram_read_byte(uint8_t addr, uint8_t *out);` / `bool fram_read_u16(uint8_t addr, uint16_t *out);` / `bool fram_read_u32(uint8_t addr, uint32_t *out);` — true=성공, 실패 시 `*out` 미기록
  - `void app_config_factory_defaults(app_config_t *cfg);` — RAM만 채움, FRAM 미접촉
  - `uint8_t app_config_load(app_config_t *cfg);` — 0=clean / 1..38=실패 read 수 / **0xFF=INIT_FLAG 읽기 실패(FRAM 미변경, 전 필드 기본값)**

- [ ] **Step 1: mock fram 작성**

`fw/test/mock_fram.h`:

```c
/* fw/test/mock_fram.h — host mock of the fram.h read/write API.
 * 256B backing store + per-address read-fail injection + write counter.
 * Links in place of fw/drivers/fram.c (which needs HAL/i2c1). */
#pragma once
#include <stdint.h>
#include "fram.h"

void     mock_fram_reset(void);                     /* store=0, fail=none, write_cnt=0 */
void     mock_fram_fail_read(uint8_t addr, uint8_t nbytes); /* [addr, addr+n) read 실패 주입 */
void     mock_fram_poke(uint8_t addr, uint8_t v);   /* backing store 직접 기록 */
uint8_t  mock_fram_peek(uint8_t addr);              /* backing store 직접 읽기 */
uint32_t mock_fram_write_count(void);               /* fram_write_* 호출 누계 */
void     mock_fram_clear_write_count(void);
```

`fw/test/mock_fram.c`:

```c
/* fw/test/mock_fram.c — see mock_fram.h. Byte order mirrors fw/drivers/fram.c
 * (big-endian u16/u32) so app_config_save_all→load round-trips. */
#include <string.h>
#include "mock_fram.h"

static uint8_t  s_mem[256];
static uint8_t  s_fail[256];      /* 1 = read of this address fails */
static uint32_t s_write_cnt;

void mock_fram_reset(void)
{
    memset(s_mem, 0, sizeof(s_mem));
    memset(s_fail, 0, sizeof(s_fail));
    s_write_cnt = 0u;
}

void mock_fram_fail_read(uint8_t addr, uint8_t nbytes)
{
    for (uint8_t i = 0u; i < nbytes; i++) { s_fail[(uint8_t)(addr + i)] = 1u; }
}

void     mock_fram_poke(uint8_t addr, uint8_t v) { s_mem[addr] = v; }
uint8_t  mock_fram_peek(uint8_t addr)            { return s_mem[addr]; }
uint32_t mock_fram_write_count(void)             { return s_write_cnt; }
void     mock_fram_clear_write_count(void)       { s_write_cnt = 0u; }

static int span_fails(uint8_t addr, uint8_t n)
{
    for (uint8_t i = 0u; i < n; i++) {
        if (s_fail[(uint8_t)(addr + i)]) { return 1; }
    }
    return 0;
}

bool fram_read_byte(uint8_t addr, uint8_t *out)
{
    if (span_fails(addr, 1u)) { return false; }
    *out = s_mem[addr];
    return true;
}

bool fram_read_u16(uint8_t addr, uint16_t *out)
{
    if (span_fails(addr, 2u)) { return false; }
    *out = (uint16_t)(((uint16_t)s_mem[addr] << 8) | s_mem[(uint8_t)(addr + 1u)]);
    return true;
}

bool fram_read_u32(uint8_t addr, uint32_t *out)
{
    if (span_fails(addr, 4u)) { return false; }
    *out = ((uint32_t)s_mem[addr] << 24) | ((uint32_t)s_mem[(uint8_t)(addr + 1u)] << 16)
         | ((uint32_t)s_mem[(uint8_t)(addr + 2u)] << 8) | (uint32_t)s_mem[(uint8_t)(addr + 3u)];
    return true;
}

void fram_write_byte(uint8_t addr, uint8_t v)
{
    s_mem[addr] = v;
    s_write_cnt++;
}

void fram_write_u16(uint8_t addr, uint16_t v)
{
    s_mem[addr] = (uint8_t)(v >> 8);
    s_mem[(uint8_t)(addr + 1u)] = (uint8_t)v;
    s_write_cnt++;
}

void fram_write_u32(uint8_t addr, uint32_t v)
{
    s_mem[addr] = (uint8_t)(v >> 24);
    s_mem[(uint8_t)(addr + 1u)] = (uint8_t)(v >> 16);
    s_mem[(uint8_t)(addr + 2u)] = (uint8_t)(v >> 8);
    s_mem[(uint8_t)(addr + 3u)] = (uint8_t)v;
    s_write_cnt++;
}
```

- [ ] **Step 2: 실패 테스트 작성 (spec §6.1 시나리오 6종)**

`fw/test/test_app_config.c`:

```c
/* fw/test/test_app_config.c — host tests for app_config_load fallback semantics
 * (감사 H3). app_config.c compiles HAL-free; fram is mock_fram.c (link 치환).
 * Scenarios = spec §6.1 (2026-07-02-fram-i2c-robustness-design.md). */
#include <stdio.h>
#include <stdint.h>
#include "app_config.h"
#include "fram.h"
#include "mock_fram.h"

static int failures = 0;

#define CHECK_EQ(expr, expected) do {                                       \
    unsigned long a_ = (unsigned long)(expr);                               \
    unsigned long e_ = (unsigned long)(expected);                           \
    if (a_ != e_) {                                                         \
        printf("FAIL %s:%d  %s = %lu, expected %lu\n",                      \
               __FILE__, __LINE__, #expr, a_, e_);                          \
        failures++;                                                         \
    }                                                                       \
} while (0)

/* 저장값이 기본값과 전부 구분되도록 만든 config를 mock FRAM에 기록. */
static void store_distinct_config(void)
{
    app_config_t c;
    app_config_factory_defaults(&c);
    c.output_power   = 77u;      /* default 50 */
    c.limit_on_time  = 123u;     /* default 50 */
    c.limit_energy   = 54321u;   /* default 100000 */
    c.work_cnt       = 999u;     /* default 0 */
    c.limit_mo_out1  = 61u;      /* default 25 */
    c.comm_address   = 7u;       /* default 0 */
    c.energy_ctrl    = true;     /* default false */
    c.ether_ip[0] = 192u; c.ether_ip[1] = 168u; c.ether_ip[2] = 1u; c.ether_ip[3] = 10u;

    mock_fram_reset();
    mock_fram_poke(FRAM_ADDR_INIT_FLAG, FRAM_INIT_FLAG_MAGIC);
    app_config_save_all(&c);
    mock_fram_clear_write_count();
}

/* #1 전 필드 성공: 반환 0, 저장값 그대로, write 0건 */
static void test_clean_load(void)
{
    store_distinct_config();
    app_config_t cfg;
    uint8_t fail = app_config_load(&cfg);
    CHECK_EQ(fail, 0);
    CHECK_EQ(cfg.output_power, 77);
    CHECK_EQ(cfg.limit_on_time, 123);
    CHECK_EQ(cfg.limit_energy, 54321);
    CHECK_EQ(cfg.work_cnt, 999);
    CHECK_EQ(cfg.limit_mo_out1, 61);
    CHECK_EQ(cfg.comm_address, 7);
    CHECK_EQ(cfg.energy_ctrl, 1);
    CHECK_EQ(cfg.ether_ip[3], 10);
    CHECK_EQ(mock_fram_write_count(), 0);
}

/* #2 magic ≠ 0xAA (read 성공) = 진짜 빈 FRAM: factory_write 발생, 반환 0, 기본값 */
static void test_blank_fram_factory(void)
{
    mock_fram_reset();                      /* INIT_FLAG = 0 ≠ 0xAA, 읽기는 성공 */
    app_config_t cfg;
    uint8_t fail = app_config_load(&cfg);
    CHECK_EQ(fail, 0);
    CHECK_EQ(cfg.output_power, 50);         /* factory default */
    CHECK_EQ(cfg.limit_energy, 100000);
    CHECK_EQ(mock_fram_peek(FRAM_ADDR_INIT_FLAG), FRAM_INIT_FLAG_MAGIC);
    CHECK_EQ(mock_fram_write_count() > 0u, 1);   /* save_all 발생 */
}

/* #3 INIT_FLAG read 실패: FRAM 미변경(write 0건) 입증 — 데이터손실 경로 차단 핵심 */
static void test_init_flag_read_fail_no_wipe(void)
{
    store_distinct_config();
    mock_fram_fail_read(FRAM_ADDR_INIT_FLAG, 1u);
    app_config_t cfg;
    uint8_t fail = app_config_load(&cfg);
    CHECK_EQ(fail, 0xFF);
    CHECK_EQ(mock_fram_write_count(), 0);   /* factory-write 금지 */
    CHECK_EQ(cfg.output_power, 50);         /* 전 필드 기본값 */
    CHECK_EQ(cfg.limit_on_time, 50);
    CHECK_EQ(mock_fram_peek(FRAM_ADDR_OUT_POWER), 77);  /* FRAM 원본 보존 */
}

/* #4 부분 실패: ON_TIME(u16)+OUT_POWER(byte)만 fail → 그 2필드만 기본값, 반환 2 */
static void test_partial_fail_field_fallback(void)
{
    store_distinct_config();
    mock_fram_fail_read(FRAM_ADDR_ON_TIME, 2u);
    mock_fram_fail_read(FRAM_ADDR_OUT_POWER, 1u);
    app_config_t cfg;
    uint8_t fail = app_config_load(&cfg);
    CHECK_EQ(fail, 2);
    CHECK_EQ(cfg.limit_on_time, 50);        /* fallback = factory default */
    CHECK_EQ(cfg.output_power, 50);         /* fallback */
    CHECK_EQ(cfg.limit_energy, 54321);      /* 나머지는 저장값 */
    CHECK_EQ(cfg.work_cnt, 999);
    CHECK_EQ(cfg.energy_ctrl, 1);
    CHECK_EQ(mock_fram_write_count(), 0);
}

/* #5 limit_energy > 100000 (read 성공): 클램프 + write-back 1건 (현행 무회귀) */
static void test_energy_clamp_writeback(void)
{
    store_distinct_config();
    /* ENERGY(u32 @22)에 200000 직접 기록 */
    mock_fram_poke(FRAM_ADDR_ENERGY,      (uint8_t)(200000u >> 24));
    mock_fram_poke((uint8_t)(FRAM_ADDR_ENERGY + 1u), (uint8_t)(200000u >> 16));
    mock_fram_poke((uint8_t)(FRAM_ADDR_ENERGY + 2u), (uint8_t)(200000u >> 8));
    mock_fram_poke((uint8_t)(FRAM_ADDR_ENERGY + 3u), (uint8_t)200000u);
    app_config_t cfg;
    uint8_t fail = app_config_load(&cfg);
    CHECK_EQ(fail, 0);
    CHECK_EQ(cfg.limit_energy, 100000);
    CHECK_EQ(mock_fram_write_count(), 1);   /* write-back 1건 */
}

/* #6 limit_energy read 실패: 기본값 100000, write-back 0건, 반환 1 */
static void test_energy_read_fail_no_writeback(void)
{
    store_distinct_config();
    mock_fram_fail_read(FRAM_ADDR_ENERGY, 4u);
    app_config_t cfg;
    uint8_t fail = app_config_load(&cfg);
    CHECK_EQ(fail, 1);
    CHECK_EQ(cfg.limit_energy, 100000);     /* default */
    CHECK_EQ(mock_fram_write_count(), 0);
}

int main(void)
{
    test_clean_load();
    test_blank_fram_factory();
    test_init_flag_read_fail_no_wipe();
    test_partial_fail_field_fallback();
    test_energy_clamp_writeback();
    test_energy_read_fail_no_writeback();
    if (failures) { printf("test_app_config: %d FAILED\n", failures); return 1; }
    printf("test_app_config: all passed\n");
    return 0;
}
```

- [ ] **Step 3: Makefile에 신규 스위트 추가 + stale 주석 정정 (동승 결정)**

`fw/test/Makefile` 1-3행을 다음으로 교체:

```make
# fw/test/Makefile — host unit tests for the HAL-free layers.
# Suites: reg_calc / modbus_core / tcp_frame / weld_fsm / seek_reset_fsm /
#         i2c_pot / app_config(mock-fram).  Usage: make -C fw/test test
```

변수·target 추가 (기존 패턴 그대로):

```make
BIN_CFG := /tmp/gds_test_app_config
```

`test:` 의존성·실행에 `$(BIN_CFG)` 추가 (기존 6개 뒤):

```make
test: $(BIN_REG) $(BIN_MB) $(BIN_TCP) $(BIN_WELD) $(BIN_SR) $(BIN_POT) $(BIN_CFG)
	$(BIN_REG)
	$(BIN_MB)
	$(BIN_TCP)
	$(BIN_WELD)
	$(BIN_SR)
	$(BIN_POT)
	$(BIN_CFG)
```

빌드 룰 (mock이 fram.c를 대체 — 실제 `../drivers/fram.c`는 링크 금지):

```make
# app_config: fram is mock_fram.c (fw/drivers/fram.c needs HAL — do NOT link it here)
$(BIN_CFG): test_app_config.c mock_fram.c mock_fram.h ../src/app_config.c ../include/app_config.h ../include/fram.h
	$(CC) $(CFLAGS) $(INC) -I. -o $@ test_app_config.c mock_fram.c ../src/app_config.c
```

`clean:`의 rm 목록에 `$(BIN_CFG)` 추가.

- [ ] **Step 4: RED 확인**

Run: `make -C fw/test test 2>&1 | tail -20`
Expected: **컴파일 실패** — `fram_read_byte`가 구 시그니처(`uint8_t fram_read_byte(uint8_t)`)라 mock/테스트와 충돌 + `app_config_factory_defaults` 미정의. (구현 전 = RED.)

- [ ] **Step 5: `fram.h` 읽기 시그니처 변경**

`fw/include/fram.h` — `#include <stdint.h>` 아래에 `#include <stdbool.h>` 추가, 52-54행 read 3종 proto를 다음으로 교체 (write 3종·주석·주소맵은 그대로):

```c
/* Reads return true on success; on failure *out is left untouched so a caller
 * that pre-loads defaults keeps them (감사 H3: silent-0 load 제거). */
bool fram_read_byte(uint8_t addr, uint8_t  *out);
bool fram_read_u16 (uint8_t addr, uint16_t *out);   /* big-endian */
bool fram_read_u32 (uint8_t addr, uint32_t *out);   /* big-endian */
```

- [ ] **Step 6: `fram.c` 읽기 구현 교체**

`fw/drivers/fram.c` 5-25행(read 3종)을 다음으로 교체 (write 3종 무변경):

```c
bool fram_read_byte(uint8_t addr, uint8_t *out)
{
    uint8_t b;
    if (i2c1_mem_read(FRAM_I2C_ADDR_7B, addr, &b, 1) != HAL_OK) {
        return false;
    }
    *out = b;
    return true;
}

bool fram_read_u16(uint8_t addr, uint16_t *out)
{
    uint8_t b[2];
    if (i2c1_mem_read(FRAM_I2C_ADDR_7B, addr, b, 2) != HAL_OK) {
        return false;
    }
    *out = (uint16_t)(((uint16_t)b[0] << 8) | (uint16_t)b[1]);
    return true;
}

bool fram_read_u32(uint8_t addr, uint32_t *out)
{
    uint8_t b[4];
    if (i2c1_mem_read(FRAM_I2C_ADDR_7B, addr, b, 4) != HAL_OK) {
        return false;
    }
    *out = ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16)
         | ((uint32_t)b[2] <<  8) |  (uint32_t)b[3];
    return true;
}
```

- [ ] **Step 7: `app_config.h` proto 갱신**

`fw/include/app_config.h` 27행 및 30행 부근을 다음으로 교체:

```c
/* Load config from FRAM. Factory defaults are pre-applied; each field is only
 * overwritten by a successful read (감사 H3 폴백).
 * Returns: 0 = clean / 1..38 = failed-read count (those fields run on
 * defaults) / 0xFF = INIT_FLAG itself unreadable → FRAM left UNTOUCHED
 * (no factory-write: a transient bus fault must not wipe a good FRAM),
 * all fields on defaults. */
uint8_t app_config_load(app_config_t *cfg);

/* Fill *cfg with factory defaults. RAM only — never touches FRAM. */
void app_config_factory_defaults(app_config_t *cfg);

/* factory defaults + persist full map (save_all). */
void app_config_factory_write(app_config_t *cfg);
```

- [ ] **Step 8: `app_config.c` — defaults 분리 + 폴백 로드 재작성**

`fw/src/app_config.c`에서 ① `app_config_factory_write`의 기본값 채움 본문(8-36행)을 `app_config_factory_defaults`로 이동하고 factory_write는 위임+save_all만, ② `app_config_load`를 재작성:

```c
void app_config_factory_defaults(app_config_t *cfg)
{
    /* defaults — samd20 factory_init (ref/samd20/main.c:753-828) */
    cfg->model_freq          = 0;
    cfg->model_type          = 0;     /* hand */
    cfg->f_safty             = 0;
    cfg->run_mode            = 0;     /* MODE_DELAY */
    cfg->limit_delay_time1   = 50;
    cfg->limit_delay_time2   = 50;
    cfg->limit_delay_time3   = 50;
    cfg->limit_trigger_time2 = 50;
    cfg->limit_trigger_time3 = 50;
    cfg->limit_on_time       = 50;
    cfg->limit_out_time      = 10;
    cfg->limit_mo_out1       = 25;
    cfg->limit_mo_out2       = 50;
    cfg->limit_mo_time1      = 25;
    cfg->limit_mo_time2      = 50;
    cfg->output_power        = 50;
    cfg->work_cnt            = 0;
    cfg->limit_energy        = 100000;
    cfg->energy_ctrl         = false;  /* port fix: samd20 leaves implicit (bss-zero) */
    cfg->multi_ctrl          = false;  /* port fix: samd20 leaves implicit (bss-zero) */
    cfg->cal_val             = 0;
    cfg->freq_cal_val        = 0;
    cfg->comm_address        = 0;
    cfg->comm_speed_idx      = 0;
    cfg->comm_parity_idx     = 0;
    cfg->comm_mode           = 0;      /* COMM_SERIAL */
    for (int i = 0; i < 4; i++) {
        cfg->ether_ip[i] = 0; cfg->ether_nm[i] = 0; cfg->ether_gw[i] = 0;
    }
}

void app_config_factory_write(app_config_t *cfg)
{
    app_config_factory_defaults(cfg);
    /* persist full map (defaults leave EN flags + ether zero, so save_all matches) */
    app_config_save_all(cfg);
}
```

`app_config_load` 전체 교체 (필드 순서는 기존 유지):

```c
uint8_t app_config_load(app_config_t *cfg)
{
    uint8_t fail = 0u;
    uint8_t flag = 0u;
    uint8_t b8;
    uint16_t u16;

    /* 기본값 선적용 — 이후 읽기 실패 필드는 자동으로 기본값 유지 (spec §3.2). */
    app_config_factory_defaults(cfg);

    if (!fram_read_byte(FRAM_ADDR_INIT_FLAG, &flag)) {
        /* 버스 이상: 빈 FRAM과 구분 불가 → factory-write 금지 (일시 오류가
         * 정상 FRAM을 덮어쓰는 데이터손실 차단). RAM은 전체 기본값으로 동작. */
        return 0xFFu;
    }
    if (flag != FRAM_INIT_FLAG_MAGIC) {
        fram_write_byte(FRAM_ADDR_INIT_FLAG, FRAM_INIT_FLAG_MAGIC);
        app_config_factory_write(cfg);
        return 0u;
    }

    if (!fram_read_u32 (FRAM_ADDR_WORK_CNT, &cfg->work_cnt))            { fail++; }
    if (!fram_read_u16 (FRAM_ADDR_DELAY1,   &cfg->limit_delay_time1))   { fail++; }
    if (!fram_read_u16 (FRAM_ADDR_DELAY2,   &cfg->limit_delay_time2))   { fail++; }
    if (!fram_read_u16 (FRAM_ADDR_DELAY3,   &cfg->limit_delay_time3))   { fail++; }
    if (!fram_read_u16 (FRAM_ADDR_TRIGGER2, &cfg->limit_trigger_time2)) { fail++; }
    if (!fram_read_u16 (FRAM_ADDR_TRIGGER3, &cfg->limit_trigger_time3)) { fail++; }
    if (!fram_read_byte(FRAM_ADDR_SAFTY,      &cfg->f_safty))           { fail++; }
    if (!fram_read_byte(FRAM_ADDR_RUN_MODE,   &cfg->run_mode))          { fail++; }
    if (!fram_read_byte(FRAM_ADDR_MODEL_FREQ, &cfg->model_freq))        { fail++; }
    if (!fram_read_byte(FRAM_ADDR_MODEL_TYPE, &cfg->model_type))        { fail++; }
    if (!fram_read_byte(FRAM_ADDR_OUT_POWER,  &cfg->output_power))      { fail++; }
    if (!fram_read_u16 (FRAM_ADDR_ON_TIME,    &cfg->limit_on_time))     { fail++; }
    if (fram_read_byte(FRAM_ADDR_EN_ENERGY, &b8)) { cfg->energy_ctrl = (b8 == 1); } else { fail++; }
    if (fram_read_u32(FRAM_ADDR_ENERGY, &cfg->limit_energy)) {
        if (cfg->limit_energy > 100000) {             /* samd20 clamp (main.c:923) */
            cfg->limit_energy = 100000;
            fram_write_u32(FRAM_ADDR_ENERGY, cfg->limit_energy);
        }
    } else { fail++; }                                /* 실패 시 default 100000 — write-back 불요 */
    if (fram_read_byte(FRAM_ADDR_EN_MULTI, &b8)) { cfg->multi_ctrl = (b8 == 1); } else { fail++; }
    if (!fram_read_u16 (FRAM_ADDR_MULTI_T1, &cfg->limit_mo_time1))      { fail++; }
    if (!fram_read_u16 (FRAM_ADDR_MULTI_T2, &cfg->limit_mo_time2))      { fail++; }
    if (!fram_read_u16 (FRAM_ADDR_MULTI_O1, &cfg->limit_mo_out1))       { fail++; }
    if (!fram_read_u16 (FRAM_ADDR_MULTI_O2, &cfg->limit_mo_out2))       { fail++; }
    if (fram_read_byte(FRAM_ADDR_TIMEOVER, &b8)) { cfg->limit_out_time = b8; } else { fail++; }
    if (!fram_read_byte(FRAM_ADDR_COMM_ADDR,   &cfg->comm_address))     { fail++; }
    if (!fram_read_byte(FRAM_ADDR_COMM_SPEED,  &cfg->comm_speed_idx))   { fail++; }
    if (!fram_read_byte(FRAM_ADDR_COMM_PARITY, &cfg->comm_parity_idx))  { fail++; }
    if (fram_read_u16(FRAM_ADDR_CAL_VAL,      &u16)) { cfg->cal_val      = (int16_t)u16; } else { fail++; }
    if (fram_read_u16(FRAM_ADDR_FREQ_CAL_VAL, &u16)) { cfg->freq_cal_val = (int16_t)u16; } else { fail++; }
    if (!fram_read_byte(FRAM_ADDR_COMM_MODE, &cfg->comm_mode))          { fail++; }
    for (int i = 0; i < 4; i++) {
        if (!fram_read_byte((uint8_t)(FRAM_ADDR_ETHER_IP + i), &cfg->ether_ip[i])) { fail++; }
        if (!fram_read_byte((uint8_t)(FRAM_ADDR_ETHER_NM + i), &cfg->ether_nm[i])) { fail++; }
        if (!fram_read_byte((uint8_t)(FRAM_ADDR_ETHER_GW + i), &cfg->ether_gw[i])) { fail++; }
    }
    return fail;
}
```

⚠ 주의: `limit_on_time`은 struct에서 `uint16_t`, `output_power`/`f_safty` 등은 `uint8_t` — 포인터 타입이 fram_read 변형과 정확히 일치해야 함(컴파일러가 검출). `limit_out_time`은 struct `uint16_t`이지만 저장 1B(`FRAM_ADDR_TIMEOVER`)이므로 `b8` 경유(기존 semantics 동일).

- [ ] **Step 9: GREEN 확인 (host)**

Run: `make -C fw/test test 2>&1 | tail -10`
Expected: 기존 6스위트 + `test_app_config: all passed` = 7스위트 PASS.

- [ ] **Step 10: 타깃 빌드 (시그니처 전파 완결 입증)**

Run: `env -u STM32_TOOLCHAIN cmake --build fw/build 2>&1 | grep -icE "warning|error" ; env -u STM32_TOOLCHAIN cmake --build fw/build 2>&1 | tail -3`
Expected: warning/error 0건, FLASH/RAM 사용률 출력. (호출자가 `app_config.c`뿐이므로 여기서 에러가 나면 전파 누락.)
⚠ `app.c:43`은 반환값을 아직 안 쓰므로(다음 Task 3에서 배선) void-call 그대로 컴파일됨 — C에서 반환 무시는 경고 아님. `app_lcd_input.c:626`(LCD 취소 롤백 경로)도 동일하게 반환 무시 유지 = 의도된 결정(런타임 경고 표면은 Task 3의 1s err 델타가 담당).

- [ ] **Step 11: Commit**

```bash
git add fw/include/fram.h fw/drivers/fram.c fw/include/app_config.h fw/src/app_config.c \
        fw/test/mock_fram.h fw/test/mock_fram.c fw/test/test_app_config.c fw/test/Makefile
git commit -m "feat(config): fram 읽기 status 전파 + 필드별 팩토리 폴백 + INIT_FLAG 실패 격리 (감사 H3/D3)

fram_read_* → bool(addr,*out), 실패 시 *out 미기록. app_config_load는
기본값 선적용 후 성공 필드만 덮어쓰고 실패 수 반환(0xFF=INIT_FLAG 읽기
실패 = factory-write 금지 → 일시 버스 오류의 FRAM 전체 덮어쓰기 차단).
host 신규 스위트 test_app_config(mock-fram) 6시나리오."
```

---

### Task 2: H2 — i2c1 init 1회 bus-unstick + unstick 이벤트 getter

**Files:**
- Modify: `fw/include/i2c1.h` (getter proto)
- Modify: `fw/drivers/i2c1.c` (preflight 함수 + init 배선)

**Interfaces:**
- Consumes: 없음 (HAL GPIO만)
- Produces (Task 3이 사용): `uint8_t i2c1_unstick_events(void);` — 0=버스 깨끗(복구 불요) / 1..9=복구에 쓴 SCL 클럭 수 / 0xFF=9클럭 후에도 SDA low(복구 실패)

- [ ] **Step 1: `i2c1.h`에 getter 추가**

`fw/include/i2c1.h`의 `i2c1_err_count` 아래에 추가:

```c
/* Boot bus-unstick result (감사 H2; init 시 1회만 시도 — 사용자 확정):
 * 0 = bus clean, 1..9 = SCL clocks used to recover, 0xFF = recovery failed. */
uint8_t i2c1_unstick_events(void);
```

- [ ] **Step 2: `i2c1.c`에 preflight 구현 + init 배선**

`fw/drivers/i2c1.c`의 `static volatile uint16_t s_err_count;` 아래에 추가:

```c
static uint8_t s_unstick_events;

/* ~10 µs @96 MHz busy-wait — i2c1_init()은 sys_tick 기동 전(main.c:24)이라
 * tick 지연 불가. I2C slave는 저속 클럭 무제한 허용이라 정밀도 비요구. */
static void unstick_delay(void)
{
    for (volatile uint32_t i = 0u; i < 240u; i++) { }
}

/* SDA(PB7) stuck-low 복구: SCL(PB6) GPIO-OD 9클럭 + STOP (감사 H2).
 * HAL_I2C_Init 전, GPIO clock enable 후에만 호출. 실패해도 진행 —
 * 이후 트랜잭션 실패는 s_err_count로 드러남. */
static void i2c1_bus_unstick(void)
{
    GPIO_InitTypeDef g = {0};

    /* SDA=input으로 버스 상태 관찰, SCL=OD output(idle high) */
    g.Pin   = GPIO_PIN_7;
    g.Mode  = GPIO_MODE_INPUT;
    g.Pull  = GPIO_NOPULL;              /* 외부 10k 풀업 (보드) */
    HAL_GPIO_Init(GPIOB, &g);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
    g.Pin   = GPIO_PIN_6;
    g.Mode  = GPIO_MODE_OUTPUT_OD;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &g);
    unstick_delay();

    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7) == GPIO_PIN_SET) {
        s_unstick_events = 0u;          /* 버스 깨끗 — 통상 경로 */
        return;
    }

    uint8_t clocks = 0u;
    while (clocks < 9u) {               /* 9클럭 = 8data+ACK 최악 케이스 */
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
        unstick_delay();
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
        unstick_delay();
        clocks++;
        if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7) == GPIO_PIN_SET) {
            break;
        }
    }

    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7) != GPIO_PIN_SET) {
        s_unstick_events = 0xFFu;       /* 복구 실패 — HAL init은 그대로 진행 */
        return;
    }

    /* STOP 조건: SCL high 상태에서 SDA low→high */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET);
    g.Pin  = GPIO_PIN_7;
    g.Mode = GPIO_MODE_OUTPUT_OD;
    HAL_GPIO_Init(GPIOB, &g);
    unstick_delay();
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);
    unstick_delay();
    s_unstick_events = clocks;
}
```

`i2c1_init()`의 clock enable(§1)과 GPIO AF 설정(§2) **사이**에 호출 삽입:

```c
    /* 1.5 bus-unstick preflight (감사 H2) — AF 설정 전 GPIO로 SDA stuck 복구.
     * 아래 §2 HAL_GPIO_Init(AF_OD)이 임시 GPIO 모드를 덮어쓴다. */
    i2c1_bus_unstick();
```

파일 끝에 getter 추가:

```c
uint8_t i2c1_unstick_events(void) { return s_unstick_events; }
```

- [ ] **Step 3: 타깃 빌드 0-warning**

Run: `env -u STM32_TOOLCHAIN cmake --build fw/build 2>&1 | grep -icE "warning|error" ; env -u STM32_TOOLCHAIN cmake --build fw/build 2>&1 | tail -3`
Expected: 0건 + 사이즈 출력. host 테스트는 이 Task 무관(그래도 `make -C fw/test test`로 7스위트 무회귀 확인).

- [ ] **Step 4: Commit**

```bash
git add fw/include/i2c1.h fw/drivers/i2c1.c
git commit -m "feat(i2c1): init 1회 SCL 9클럭 bus-unstick + i2c1_unstick_events 관측 (감사 H2/D3)

HAL init 전 GPIO 프리플라이트: SDA(PB7) stuck-low 감지 시 SCL(PB6) OD
9클럭 토글 + STOP 생성. 결과 latch(0=깨끗/1..9=클럭수/0xFF=실패)는
mon 부팅 로그가 소비(후속 커밋). 런타임 재시도 없음(사용자 확정)."
```

---

### Task 3: mon 표면 배선 — 부팅 [cfg] 확장 + 1s err 델타 로그

**Files:**
- Modify: `fw/src/app.c` (부팅 로그 + 루프 관측 블록 + include)

**Interfaces:**
- Consumes: `app_config_load → uint8_t` (Task 1) / `i2c1_unstick_events()`, `i2c1_err_count()` (Task 2 / 기존)
- Produces: 없음 (말단 — mon 로그만)

- [ ] **Step 1: include + 부팅 로그**

`fw/src/app.c` 상단 include 블록에 (없으면) 추가:

```c
#include "i2c1.h"    /* i2c1_err_count / i2c1_unstick_events — 부팅·1s 관측 로그 */
```

`app_init()`의 `app_config_load(cfg);` 줄(현 43행)을 반환 캡처로 교체:

```c
    uint8_t cfg_fail = app_config_load(cfg);  /* FRAM read; factory-write on blank (0xAA flag) */
```

기존 `[cfg] freq=... en_m=...` `mon_printf` 직후에 추가:

```c
    mon_printf("[cfg] fram_fail=%u unstick=%u i2c_err=%u\r\n",
               (unsigned)cfg_fail, (unsigned)i2c1_unstick_events(),
               (unsigned)i2c1_err_count());
    if (cfg_fail == 0xFFu) {
        mon_writeln("[cfg] WARN: FRAM unreadable - ALL defaults, FRAM untouched");
    } else if (cfg_fail != 0u) {
        mon_printf("[cfg] WARN: defaults active for %u field(s)\r\n", (unsigned)cfg_fail);
    }
```

- [ ] **Step 2: 루프 1s 델타 관측 블록**

`app_loop_iter()`의 마지막 스텝(`app_modbus_tick();` 호출 뒤)에 추가:

```c
    /* 6. I2C1 관측 — 1 s cadence, err_count 델타 시에만 mon 1줄 (감사 H2 표면;
     * mon 전용 = 사용자 확정. save_all/POT write 실패 런타임 관측용). */
    {
        static uint32_t s_i2c_chk_ms;
        static uint16_t s_i2c_err_last;
        uint32_t now_ms = sys_tick_get_ms();
        if ((uint32_t)(now_ms - s_i2c_chk_ms) >= 1000u) {
            s_i2c_chk_ms = now_ms;
            uint16_t e = i2c1_err_count();
            if (e != s_i2c_err_last) {
                mon_printf("[i2c] err=%u (+%u)\r\n",
                           (unsigned)e, (unsigned)(e - s_i2c_err_last));
                s_i2c_err_last = e;
            }
        }
    }
```

(⚠ `sys_tick_get_ms`/`mon_printf`는 app.c에서 기존 사용 중 — include 추가 불요. 델타 산술은 uint16 랩에도 안전: `e - s_i2c_err_last`는 uint16 모듈로.)

- [ ] **Step 3: 타깃 빌드 + host 회귀**

Run: `env -u STM32_TOOLCHAIN cmake --build fw/build 2>&1 | grep -icE "warning|error" ; make -C fw/test test 2>&1 | grep -cE "passed|PASSED"`
Expected: warning 0건 / 7스위트 PASS.

- [ ] **Step 4: Commit**

```bash
git add fw/src/app.c
git commit -m "feat(app): fram/i2c 관측 mon 표면 — 부팅 [cfg] fram_fail/unstick/i2c_err + 1s err 델타 (감사 D3)

경고 표면 = mon 전용(사용자 확정). cfg_fail>0이면 WARN 줄(0xFF=ALL
defaults/FRAM untouched 구분). 루프 1s cadence 델타 로그로
i2c1_err_count 죽은-API 해소."
```

---

### Task 4: 문서 갱신 (changelog + NEXT_STEPS + spec HW 체크리스트)

**Files:**
- Modify: `docs/changelog.md` (2026-07-02 항목에 D3 코드 완료 추가 또는 신규 항목)
- Modify: `docs/NEXT_STEPS.md` (§1.3 D3 행 → CODE-COMPLETE 표기, §2.2 큐 갱신)

**Interfaces:** 없음 (docs)

- [ ] **Step 1: changelog 갱신**

`docs/changelog.md`의 `### 2026-07-02 —` 항목 마지막 불릿(다음=D3...) 을 다음으로 교체:

```markdown
- **D3 'fram-i2c-robustness' CODE-COMPLETE** (branch `feat/fram-i2c-robustness`, 미머지·HW 회귀 게이트): H3=`fram_read_*` `bool(addr,*out)` 전파 + `app_config_load` 기본값 선적용-덮어쓰기(실패 수 반환, **INIT_FLAG 읽기실패=factory-write 금지** — FRAM 덮어쓰기 데이터손실 차단) / H2=init 1회 SCL 9클럭 unstick+STOP / 표면=mon 전용(부팅 `[cfg] fram_fail/unstick/i2c_err`+WARN, 루프 1s err 델타). host 신규 스위트 `test_app_config`(mock-fram) 6시나리오 = 총 7스위트. spec=`specs/2026-07-02-fram-i2c-robustness-design.md`, plan=`plans/2026-07-02-fram-i2c-robustness.md`.
- 다음 = D3 HW 회귀(정상 부팅 무회귀 + mon `fram_fail=0 unstick=0`) 후 머지 → D6(M7) → D5(reconcile b→d→ch1).
```

- [ ] **Step 2: NEXT_STEPS §1.3 D3 행 갱신**

`docs/NEXT_STEPS.md` §1.3 D3 행의 "다음=writing-plans→subagent-driven"을 다음으로 교체:

```
🔨 **CODE-COMPLETE 2026-07-02**(branch `feat/fram-i2c-robustness`, host 7스위트+0-warning) — 남은 것=HW 회귀(정상 부팅 mon `fram_fail=0 unstick=0` + 직접런 ceiling 무회귀) 후 머지
```

- [ ] **Step 3: Commit**

```bash
git add docs/changelog.md docs/NEXT_STEPS.md
git commit -m "docs: D3 fram-i2c-robustness CODE-COMPLETE 반영"
```

---

## HW 회귀 체크리스트 (다음 보드 세션 — 이 plan 범위 밖, spec §6.2)

1. 플래시 후 정상 부팅: LCD 값 육안(output_power 등 저장값 유지 = 폴백 미발동).
2. mon 가용 시(LCD에서 addr=NONE): `[cfg] fram_fail=0 unstick=0 i2c_err=0` 확인.
3. mbpoll 직접런 ceiling 무회귀(STATUS 1×N→0).
4. LCD 설정 저장(save_all) 후 재부팅 리로드 정상.
5. PASS 시 main 머지(--no-ff) + tag `hw-revA_fw-stage-fram-robust` (기존 태깅 규칙).

## Self-Review 결과

- spec 커버리지: §3.1(Task 1 Step 5-6) §3.2(Step 7-8) §3.3(Task 2) §3.4(Task 3) §6.1(Step 1-4,9) — 전부 매핑. §6.2 HW=범위 밖 명시.
- 반환값 상한: 실측 read 수 = 26 + ether 12 = **38** (spec §3.2의 "26회"는 ether 루프 12를 제외한 수치 — plan의 38이 정확, 0xFF 충돌 없음).
- 타입 일관성: `fram_read_u16`의 out 대상 중 struct `uint16_t` 필드는 직접 포인터, `int16_t`(cal_val 계열)·1B 저장 `uint16_t`(limit_out_time)·bool은 임시 변수 경유 — Step 8 코드에 반영.
- placeholder 없음 확인.
