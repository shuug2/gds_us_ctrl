# 물리 IO 레이어 — 슬라이스 C (Overload 과부하 응답) 구현 플랜

> **✅ STATUS (2026-06-21): COMPLETE (미머지, HW 게이트).** 전 6 Task를 subagent-driven으로 구현 + Task별 2단계 리뷰(spec→cpp) + 최종 통합 cpp-reviewer **APPROVE (0 Crit/High)**. 빌드 0-warning(FLASH 43.13%/RAM 16.85%), host **7스위트 PASS**. 코드 커밋: Task1 `b0d6a52` / Task2 `c15cdca` / Task3 `d2e0eb9` / Task4 `d03a0d5` / Task5 `0670ebd`.
> **플랜 외 후속 fix(사용자 HW 확정 PB3=dry-contact 상태신호→펌웨어 정지 load-bearing):** force-stop 1-iter stale 레이스를 active-level 재시도 `44d64f9` + de-assert hardening `0bf084c`로 **완전 차단**(각각 cpp 재리뷰 APPROVE). 잔여 0.
> **다음 = 보드 세션 HW 검증(Task 6 Step 4 체크리스트) → A→B→C 의존순 머지.** 진입점=root `HANDOFF.md`.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** PB13(과부하 입력, active-HIGH)을 ×5 디바운스해 과부하 감지 시 초음파를 정지하고 ERR_OVLD/ICON_OL·릴레이(PB3)·부저 점멸로 알리며, 고장 해소 시 자동 RESET→SEEK 공진 재튜닝을 트리거한다.

**Architecture:** 순수 디바운스 FSM(`app_overload_fsm`, HAL-free, host-test)이 PB13 raw → active/assert-edge/deassert-edge를 산출. 글루(`app_overload`, 10ms tick)가 assert에 force-stop, active 동안 릴레이/부저/에러, deassert에 클리어+자동복구를 오케스트레이션. LCD 에러 표시(ERR_OVLD/ICON_OL/페이지)는 기존 인프라(`app_lcd_show_error`/RESET-키 클리어 패턴) 재사용. START 차단은 app_reg guard에 `app_overload_active()` 추가(seek_reset 직교 패턴), Modbus STATUS OVLD 비트 미러.

**Tech Stack:** C11, STM32 HAL (GPIO via slice A `drivers/io`), arm-none-eabi-gcc + CMake/Ninja, host 단위테스트.

> **⚠ 브랜치 베이스 = 슬라이스 A** (`feat/physical-io-slice-a`, tip `0e2408b`). 슬라이스 C는 A의 `io_read_overload`(PB13)/`io_ovld_relay`(PB3)/`app_buzzer_beep_ms`를 **소비**하므로 main이 아니라 **A 위에 스택**한다. A는 HW-gated 미머지 — C는 미검증 토대 위에 쌓임. **A·B·C 모두 `app_reg.c`/`main.c`에서 분기** → main 머지 시 충돌 예상(해소 가능). 클린 exit = 한 보드 세션에서 **A→B→C 의존순 HW 검증·머지**. (사용자 규율: HW PASS 후 머지)

> **충실도 결정 (1차 소스 + 사용자)**: 이 레이어는 **SAMD20** port (`ref/samd20/main.c`). SAMD20 overload 모델 = **레벨 추종 stop & wait**(`do_action` 1450·`do_action`/SYS_ERROR 1645): `re_ovld_issued`(레벨) → SYS_ERROR+ERR_OVLD+릴레이, **입력 de-assert 시 자동 클리어**(RESET 키 아님), 부저 `mode_blink` 점멸. **사용자 결정**: SAMD20 stop&wait 충실 + **고장 해소(de-assert) 시 자동 RESET→SEEK 공진 재튜닝 추가**(M16 흡수). ×5 디바운스 = M16 disasm(`firmware_disassembled.asm @0x10A6`: PA7 HIGH ×5 → assert, LOW → count reset).

---

## 활성 레벨·매핑 (spec §2 + M16 disasm)

| STM32 | Net | 방향 | 활성 | 슬라이스 A 헬퍼 |
|---|---|---|---|---|
| PB13 | overload 입력 | IN | HIGH (HIGH=fault) | `io_read_overload()` |
| PB3 | CON_OVLD 릴레이 | OUT | HIGH (assert) | `io_ovld_relay(bool)` |
| PA2 | BUZZER | OUT | HIGH | `app_buzzer_beep_ms(ms)` |

---

## 파일 구조

| 파일 | 책임 | 본 슬라이스 |
|---|---|---|
| `fw/include/app_overload_fsm.h` / `fw/src/app_overload_fsm.c` | 순수 ×5 디바운스 FSM (active + assert/deassert edge), HAL-free | Create |
| `fw/test/test_app_overload_fsm.c` | FSM host 단위테스트 | Create |
| `fw/test/Makefile` | overload FSM 테스트 타깃 추가 | Modify |
| `fw/include/app_overload.h` / `fw/src/app_overload.c` | 글루: 10ms tick, force-stop/릴레이/부저/에러/자동복구, `app_overload_active()` | Create |
| `fw/src/app_lcd_input.c` / `fw/include/app_lcd.h` | `app_lcd_set_overload(bool)` 접근자 (ERR_OVLD/ICON_OL/페이지) | Modify |
| `fw/src/app_reg.c` | START guard에 overload 차단 추가 | Modify |
| `fw/src/app_modbus.c` | STATUS 레지스터에 MB_STATUS_OVLD 비트 | Modify |
| `fw/src/main.c` | `app_overload_init()` 호출 | Modify |
| `fw/src/app.c` | 슈퍼루프에 `app_overload_tick()` (weld·seek_reset 사이) | Modify |

빌드 주의: 새 .c(`app_overload_fsm.c`, `app_overload.c`) 추가 후 **`cmake -B build -G Ninja` reconfigure 필수**(configure-time GLOB).

---

## 거동 모델 (확정)

- **assert edge** (inactive→active, ×5 후 1회): 현재 활성 run을 force-stop (`app_reg_command(US_CMD_RUN_RELEASE, 현재 us_run_status)` — source-matched지만 현재 소스를 넘기므로 항상 정지). 릴레이 ON, 에러 ON, 부저 시작.
- **active (레벨)**: ERR_OVLD/ICON_OL/LCD_WARNING, 릴레이 ON, 부저 점멸(250ms on/250ms off), **START 차단**(`app_overload_active()` guard), Modbus STATUS OVLD=1.
- **de-assert edge** (active→inactive, 입력 풀림): 에러 클리어(ICON_OL=0 + 런 페이지 복귀), 릴레이 OFF, **자동 `app_seek_reset_request(US_CMD_RESET, US_REMOTE)`** (RESET→SEEK 재튜닝; 체인의 `app_seek_reset_active()`가 재튜닝 동안 START 차단 → assert부터 복구완료까지 START 연속 차단).
- **stop 수명 ≠ 나머지**: stop은 edge 1회(영구 — de-assert가 자동 재시작 안 함; 재-START 필요). error/relay/buzzer/block은 레벨, clear는 de-assert. (advisor 강조)

> ⚠ **forward note**: force-stop은 **초음파 게이트**만 정지(US_TOUCH/US_COMM/US_CYCLE 게이트). weld **기계 사이클(CYL/SOL)** abort는 별개 — weld 물리트리거가 현재 dormant(호출자 없음)라 라이브 아님. 슬라이스 E/weld4가 물리트리거를 켤 때 **overload→weld FSM abort**를 배선해야 함(현재 범위 밖).

---

## Task 1: `app_overload_fsm` 순수 디바운스 FSM (TDD)

**Files:**
- Create: `fw/include/app_overload_fsm.h`, `fw/src/app_overload_fsm.c`
- Test: `fw/test/test_app_overload_fsm.c`
- Modify: `fw/test/Makefile`

- [x] **Step 1: 실패 테스트 작성 — `fw/test/test_app_overload_fsm.c`**

```c
/* fw/test/test_app_overload_fsm.c — host unit tests, 순수 과부하 디바운스 FSM.
 * M16 disasm 충실 (firmware_disassembled.asm @0x10A6): HIGH ×5 연속 → assert,
 * LOW → count 즉시 reset(noise reject). assert/deassert는 1회 edge. */
#include <stdio.h>
#include <stdint.h>
#include "app_overload_fsm.h"

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

static void test_idle_no_event(void)
{
    overload_fsm_init();
    CHECK_EQ(overload_fsm_step(0u), 0u);    /* 정상 입력 → 무이벤트 */
    CHECK_EQ(overload_fsm_step(0u), 0u);
}

static void test_assert_needs_5_consecutive(void)
{
    overload_fsm_init();
    CHECK_EQ(overload_fsm_step(1u), 0u);    /* 1 */
    CHECK_EQ(overload_fsm_step(1u), 0u);    /* 2 */
    CHECK_EQ(overload_fsm_step(1u), 0u);    /* 3 */
    CHECK_EQ(overload_fsm_step(1u), 0u);    /* 4 */
    CHECK_EQ(overload_fsm_step(1u), (unsigned)(OVLD_EV_ACTIVE | OVLD_EV_ASSERT)); /* 5 → assert */
    CHECK_EQ(overload_fsm_step(1u), OVLD_EV_ACTIVE);  /* 6: active 유지, assert edge 없음 */
}

static void test_deassert_on_low(void)
{
    overload_fsm_init();
    for (int i = 0; i < 5; i++) overload_fsm_step(1u);    /* assert */
    CHECK_EQ(overload_fsm_step(0u), OVLD_EV_DEASSERT);    /* LOW → deassert edge, active 0 */
    CHECK_EQ(overload_fsm_step(0u), 0u);                  /* 이후 무이벤트 */
}

static void test_noise_reject_resets_count(void)
{
    overload_fsm_init();
    overload_fsm_step(1u);                  /* 1 */
    overload_fsm_step(1u);                  /* 2 */
    overload_fsm_step(1u);                  /* 3 */
    CHECK_EQ(overload_fsm_step(0u), 0u);    /* LOW → count reset (active 아니었음) */
    CHECK_EQ(overload_fsm_step(1u), 0u);    /* 1 (재시작) */
    CHECK_EQ(overload_fsm_step(1u), 0u);    /* 2 */
    CHECK_EQ(overload_fsm_step(1u), 0u);    /* 3 */
    CHECK_EQ(overload_fsm_step(1u), 0u);    /* 4 */
    CHECK_EQ(overload_fsm_step(1u), (unsigned)(OVLD_EV_ACTIVE | OVLD_EV_ASSERT)); /* 5 → assert */
}

static void test_saturation_single_assert(void)
{
    overload_fsm_init();
    for (int i = 0; i < 4; i++) overload_fsm_step(1u);
    CHECK_EQ(overload_fsm_step(1u), (unsigned)(OVLD_EV_ACTIVE | OVLD_EV_ASSERT));
    for (int i = 0; i < 20; i++) {           /* 카운터 포화, assert edge 재발 없음 */
        CHECK_EQ(overload_fsm_step(1u), OVLD_EV_ACTIVE);
    }
}

static void test_glitch_low_deasserts(void)
{
    overload_fsm_init();
    for (int i = 0; i < 5; i++) overload_fsm_step(1u);    /* assert */
    CHECK_EQ(overload_fsm_step(0u), OVLD_EV_DEASSERT);    /* 단일 LOW → 즉시 deassert (M16 충실) */
    /* 재-assert는 다시 5 연속 필요 */
    CHECK_EQ(overload_fsm_step(1u), 0u);
}

int main(void)
{
    test_idle_no_event();
    test_assert_needs_5_consecutive();
    test_deassert_on_low();
    test_noise_reject_resets_count();
    test_saturation_single_assert();
    test_glitch_low_deasserts();
    if (failures) { printf("app_overload_fsm: %d FAIL\n", failures); return 1; }
    printf("app_overload_fsm: all tests passed\n");
    return 0;
}
```

- [x] **Step 2: `fw/test/Makefile`에 타깃 추가**

`BIN_BZ  := /tmp/gds_test_app_buzzer_fsm` 줄 다음에 추가:
```make
BIN_OL  := /tmp/gds_test_app_overload_fsm
```
`test:` 타깃 의존성 끝에 `$(BIN_OL)` 추가, 실행부 마지막 `	$(BIN_BZ)` 다음 줄에 `	$(BIN_OL)` 추가. `$(BIN_BZ):` 빌드 규칙 다음에:
```make
$(BIN_OL): test_app_overload_fsm.c ../src/app_overload_fsm.c ../include/app_overload_fsm.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_app_overload_fsm.c ../src/app_overload_fsm.c
```
`clean:`의 `rm -f`에 `$(BIN_OL)` 추가.

> NOTE: 이 브랜치(slice A 기반)의 host 스위트 = reg_calc/modbus_core/tcp_frame/weld_fsm/seek_reset_fsm/**buzzer_fsm**(slice A) — freq_fsm은 slice B 소속이라 **없음**. overload_fsm 추가로 **7스위트**.

- [x] **Step 3: 테스트 실패 확인**

Run: `make -C fw/test test`
Expected: FAIL — `app_overload_fsm.h` 없음 / `overload_fsm_*` 미정의 (컴파일 에러).

- [x] **Step 4: `fw/include/app_overload_fsm.h` 작성**

```c
/* fw/include/app_overload_fsm.h — 순수 과부하 디바운스 FSM (HAL-free, host-test).
 * M16 disasm 충실 (firmware_disassembled.asm @0x10A6): PB13 HIGH ×5 연속 →
 * assert, LOW → count 즉시 reset(noise reject). step()이 active 레벨 +
 * assert/deassert 1-shot edge를 비트마스크로 반환. */
#pragma once
#include <stdint.h>

#define OVLD_DEBOUNCE_N  5u     /* M16: PA7 HIGH ×5 연속 */
#define OVLD_EV_ACTIVE   0x01u  /* 현재 과부하 활성 (레벨) */
#define OVLD_EV_ASSERT   0x02u  /* inactive→active 전이 (1-shot) */
#define OVLD_EV_DEASSERT 0x04u  /* active→inactive 전이 (1-shot) */

void    overload_fsm_init(void);
uint8_t overload_fsm_step(uint8_t raw);  /* raw: 1=fault(PB13 HIGH). OVLD_EV_* 비트마스크 */
```

- [x] **Step 5: `fw/src/app_overload_fsm.c` 작성**

```c
/* fw/src/app_overload_fsm.c — 순수 과부하 디바운스 FSM. */
#include "app_overload_fsm.h"

static uint8_t s_count;    /* HIGH 연속 카운트 (cap N) */
static uint8_t s_active;   /* 디바운스된 활성 레벨 */

void overload_fsm_init(void)
{
    s_count  = 0u;
    s_active = 0u;
}

uint8_t overload_fsm_step(uint8_t raw)
{
    uint8_t prev = s_active;

    if (raw != 0u) {                       /* HIGH = fault */
        if (s_count < OVLD_DEBOUNCE_N) {
            s_count++;
        }
        if (s_count >= OVLD_DEBOUNCE_N) {
            s_active = 1u;
        }
    } else {                               /* LOW → 즉시 reset (noise reject, M16 충실) */
        s_count  = 0u;
        s_active = 0u;
    }

    uint8_t ev = s_active;                 /* bit0 = active 레벨 */
    if ((s_active != 0u) && (prev == 0u)) {
        ev |= OVLD_EV_ASSERT;
    }
    if ((s_active == 0u) && (prev != 0u)) {
        ev |= OVLD_EV_DEASSERT;
    }
    return ev;
}
```

- [x] **Step 6: 테스트 통과 확인**

Run: `make -C fw/test test`
Expected: `app_overload_fsm: all tests passed` + 기존 6스위트 전부 통과.

- [x] **Step 7: 커밋**

```bash
git add fw/include/app_overload_fsm.h fw/src/app_overload_fsm.c fw/test/test_app_overload_fsm.c fw/test/Makefile
git commit -m "feat(overload): 순수 ×5 디바운스 FSM + host-test (app_overload_fsm)

- M16 disasm 충실(@0x10A6): HIGH ×5 연속 → assert, LOW → count reset
- active 레벨 + assert/deassert 1-shot edge 비트마스크
- 슬라이스 C spec"
```

---

## Task 2: `app_lcd_set_overload` LCD 에러 접근자

**Files:**
- Modify: `fw/include/app_lcd.h`, `fw/src/app_lcd_input.c`

설명: 글루(app_overload)가 LCD를 직접 건드리지 않도록, ERR_OVLD/ICON_OL/페이지를 LCD 모듈이 소유하는 접근자로 캡슐화. 기존 RESET-키 클리어 시퀀스(`app_lcd_input.c` RESET 핸들러)와 `run_page_for_mode`(이 파일 static)를 재사용. **ERR_OVLD 비트만 `|=`/`&= ~`** (weld의 ERR_OVTIME 비트 클로버 금지 — advisor).

- [x] **Step 1: `fw/include/app_lcd.h`에 선언 추가**

`void app_lcd_show_error(uint8_t error_code);` 줄 근처(함수 선언부)에 추가:
```c
/* 과부하 에러 표시 set/clear (app_overload 글루가 호출). on=ERR_OVLD+ICON_OL+
 * LCD_WARNING, off=클리어+ICON_OL=0+런 페이지 복귀. RESET-키 클리어와 동일 패턴. */
void app_lcd_set_overload(bool on);
```
(`app_lcd.h`에 `#include <stdbool.h>`가 없으면 추가.)

- [x] **Step 2: `fw/src/app_lcd_input.c`에 정의 추가**

파일에 `#include <stdbool.h>`가 없으면 추가. `run_page_for_mode` 정의 아래(또는 RESET 핸들러 근처 적당한 곳)에 추가:
```c
/* 과부하 에러 표시 — app_overload 글루가 assert/deassert 엣지에 호출.
 * ERR_OVLD 비트만 조작(weld OVTIME 보존), ICON_OL + 경고/런 페이지는 RESET-키
 * 클리어 경로(이 파일)와 동일 시퀀스. */
void app_lcd_set_overload(bool on)
{
    lcd_app_state_t *state = app_lcd_state();

    if (on) {
        state->error_status |= ERR_OVLD;
        dgus_write_u16(ICON_OL, 1);
        app_lcd_show_error(state->error_status);   /* VP_ERROR_MSG + LCD_WARNING */
    } else {
        state->error_status &= (uint8_t)~ERR_OVLD;
        dgus_write_u16(ICON_OL, 0);
        if (state->error_status == 0u) {            /* 다른 에러 없으면 런 페이지 복귀 */
            state->lcd_status = run_page_for_mode(state->sys_mode);
            dgus_set_page(state->lcd_status);
        }
    }
}
```
(이 파일은 이미 `ERR_OVLD`(=1u), `run_page_for_mode`, `app_lcd_state`, `app_lcd_show_error`, `ICON_OL`, `dgus_write_u16`/`dgus_set_page`를 사용/포함 중 — 신규 include 불필요, stdbool 제외.)

- [x] **Step 3: 빌드 (증분; 신규 .c 아님)**

Run: `cd fw && env -u STM32_TOOLCHAIN cmake --build build 2>&1 | tail -10`
Expected: 0 warning(우리 코드), elf 생성. `run_page_for_mode`가 더 이상 "unused static" 경고 없이 참조됨(이전엔 RESET/E-stop 경로만 사용).

- [x] **Step 4: host-test 회귀**

Run: `make -C fw/test test`
Expected: 7스위트 전부 통과 (LCD 레이어는 host-test 비대상).

- [x] **Step 5: 커밋**

```bash
git add fw/include/app_lcd.h fw/src/app_lcd_input.c
git commit -m "feat(overload): app_lcd_set_overload 접근자 (ERR_OVLD/ICON_OL/페이지)

- on: ERR_OVLD|= + ICON_OL=1 + LCD_WARNING (show_error)
- off: ERR_OVLD&=~ + ICON_OL=0 + 런 페이지 복귀 (RESET-키 패턴 재사용)
- ERR_OVLD 비트만 조작(weld OVTIME 보존, advisor)"
```

---

## Task 3: `app_overload` 글루 + force-stop/릴레이/부저/자동복구 + 배선

**Files:**
- Create: `fw/include/app_overload.h`, `fw/src/app_overload.c`
- Modify: `fw/src/main.c`, `fw/src/app.c`

- [x] **Step 1: `fw/include/app_overload.h` 작성**

```c
/* fw/include/app_overload.h — 과부하 글루 (10ms tick).
 * PB13 디바운스(app_overload_fsm) → assert에 force-stop, active 동안 릴레이/
 * 부저/에러, deassert에 클리어 + 자동 RESET→SEEK 재튜닝. app_reg START guard와
 * Modbus STATUS가 app_overload_active()를 조회. */
#pragma once
#include <stdint.h>

void    app_overload_init(void);     /* boot: FSM reset + 릴레이 off */
void    app_overload_tick(void);     /* 슈퍼루프 10ms gate */
uint8_t app_overload_active(void);   /* 1 = 과부하 활성 (START 차단 / STATUS 비트) */
```

- [x] **Step 2: `fw/src/app_overload.c` 작성**

```c
/* fw/src/app_overload.c — 과부하 응답 글루.
 * 거동(SAMD20 stop&wait + M16 흡수 자동복구):
 *  assert edge → 현재 run force-stop(RUN_RELEASE 현재 소스) + 릴레이 ON + 에러 ON + 부저
 *  active     → 부저 점멸(250/250) + 릴레이/에러 유지 (START는 app_reg guard가 차단)
 *  deassert   → 릴레이 OFF + 에러 클리어(페이지 복귀) + 자동 app_seek_reset_request(RESET) */
#include "app_overload.h"
#include "app_overload_fsm.h"
#include "io.h"             /* io_read_overload, io_ovld_relay */
#include "app_buzzer.h"     /* app_buzzer_beep_ms */
#include "app_reg.h"        /* app_reg_measure, app_reg_command */
#include "app_seek_reset.h" /* app_seek_reset_request */
#include "app_lcd.h"        /* app_lcd_set_overload, US_*, us_cmd_t */
#include "sys_tick.h"

#define OVLD_TICK_MS   10u
#define OVLD_BEEP_MS   250u    /* 점멸 1회 on 길이 */
#define OVLD_BLINK_MS  500u    /* 250 on / 250 off 주기 = 재-arm 간격 */

static uint32_t s_prev_ms;
static uint32_t s_blink_ms;
static uint8_t  s_active;

void app_overload_init(void)
{
    overload_fsm_init();
    s_prev_ms  = sys_tick_get_ms();
    s_blink_ms = s_prev_ms;
    s_active   = 0u;
    io_ovld_relay(false);
}

uint8_t app_overload_active(void) { return s_active; }

void app_overload_tick(void)
{
    uint32_t now = sys_tick_get_ms();
    if ((uint32_t)(now - s_prev_ms) < OVLD_TICK_MS) {
        return;
    }
    s_prev_ms = now;

    uint8_t ev = overload_fsm_step(io_read_overload());
    s_active = (uint8_t)((ev & OVLD_EV_ACTIVE) != 0u);

    if ((ev & OVLD_EV_ASSERT) != 0u) {
        /* 현재 활성 run을 force-stop. us_run_status는 단일값이라 현재 소스를
         * RUN_RELEASE에 넘기면 source-matched 정지가 항상 발화 (TOUCH/COMM/CYCLE
         * 게이트). 신규 app_reg API 불필요 (advisor). ⚠ weld 기계 사이클 abort는
         * 별개 — weld 물리트리거 dormant라 현재 무관 (슬라이스 E/weld4). */
        uint8_t src = app_reg_measure()->us_run_status;
        if (src != (uint8_t)US_IDLE) {
            app_reg_command(US_CMD_RUN_RELEASE, src);
        }
        io_ovld_relay(true);
        app_lcd_set_overload(true);
        s_blink_ms = now;
        app_buzzer_beep_ms(OVLD_BEEP_MS);
    }

    if (s_active != 0u) {
        /* 부저 점멸: active 동안 OVLD_BLINK_MS마다 one-shot beep 재-arm
         * (SAMD20 mode_blink 점멸 재현). */
        if ((uint32_t)(now - s_blink_ms) >= OVLD_BLINK_MS) {
            s_blink_ms = now;
            app_buzzer_beep_ms(OVLD_BEEP_MS);
        }
    }

    if ((ev & OVLD_EV_DEASSERT) != 0u) {
        io_ovld_relay(false);
        app_lcd_set_overload(false);
        /* M16 흡수: 고장 해소 시 자동 공진 재튜닝(RESET→SEEK). 체인의
         * app_seek_reset_active()가 재튜닝 동안 START 차단 → assert부터 복구완료
         * 까지 START 연속 차단. src=US_REMOTE = 시스템 개시 복구(물리 source
         * 없음 — 의도적 선택). */
        app_seek_reset_request(US_CMD_RESET, (uint8_t)US_REMOTE);
    }
}
```

- [x] **Step 3: `fw/src/main.c`에 init 추가**

include 추가 (다른 app_* include 옆):
```c
#include "app_overload.h"
```
`app_buzzer_init();` 줄 다음(또는 app_seek_reset_init 다음, sys_tick·io 이후)에 추가:
```c
    app_overload_init();    /* 과부하 글루 (needs io_init + sys_tick up) */
```

- [x] **Step 4: `fw/src/app.c` 슈퍼루프에 tick 추가**

include 추가:
```c
#include "app_overload.h"
```
`app_weld_tick();`(2.5) 다음, `app_seek_reset_tick();`(2.6) **앞**에 추가:
```c
    /* 2.55 과부하 — 10 ms. assert면 force-stop(이번 iter reg publish 반영) +
     * deassert면 자동복구 요청(다음 줄 seek_reset_tick이 같은 iter에 처리). */
    app_overload_tick();
```

- [x] **Step 5: reconfigure + 빌드**

Run:
```bash
cd fw && env -u STM32_TOOLCHAIN cmake -B build -G Ninja && env -u STM32_TOOLCHAIN cmake --build build 2>&1 | tail -20
```
Expected: 새 `app_overload.c`/`app_overload_fsm.c` 링크, 0 warning(우리 코드), elf 생성, FLASH/RAM 출력.

- [x] **Step 6: host-test 회귀**

Run: `make -C fw/test test`
Expected: 7스위트 전부 통과.

- [x] **Step 7: 커밋**

```bash
git add fw/include/app_overload.h fw/src/app_overload.c fw/src/main.c fw/src/app.c
git commit -m "feat(overload): app_overload 글루 + force-stop/릴레이/부저/자동복구 배선

- assert: force-stop(현재 소스 RUN_RELEASE) + 릴레이 ON + 에러 + 부저
- active: 부저 점멸(250/250), 릴레이/에러 유지
- deassert: 릴레이 OFF + 에러 클리어 + 자동 seek_reset_request(RESET) (M16 흡수)
- app.c tick = weld와 seek_reset 사이 / main app_overload_init()"
```

---

## Task 4: app_reg START guard — 과부하 중 START 차단

**Files:**
- Modify: `fw/src/app_reg.c`

설명: SAMD20는 SYS_ERROR 상태가 START를 막음. STM32엔 그 게이트가 없어 force-stop 후 us_run_status=IDLE이라 즉시 재-START 가능 → 과부하 활성 중 차단 필요. 기존 `app_seek_reset_active()` 직교 guard와 동일 패턴(swallow consume 뒤, us_run_status 설정 앞, **별도 break** — advisor: if 조건 합치면 swallow 비대칭).

- [x] **Step 1: include 추가**

`fw/src/app_reg.c`의 include부(`#include "app_seek_reset.h"` 근처)에 추가:
```c
#include "app_overload.h"   /* app_overload_active (START 차단) */
```

- [x] **Step 2: START case에 overload guard 추가**

`app_reg_command`의 US_CMD_START case에서, 기존
```c
            if (app_seek_reset_active() != 0u) {
                break;
            }
```
**다음**에 추가:
```c
            /* 과부하 활성 중 START 차단 (SAMD20 SYS_ERROR가 START 막음).
             * seek_reset_active와 동일 직교 — 별도 break (swallow consume 뒤). */
            if (app_overload_active() != 0u) {
                break;
            }
```

- [x] **Step 3: 빌드**

Run: `cd fw && env -u STM32_TOOLCHAIN cmake --build build 2>&1 | tail -10`
Expected: 0 warning, elf 생성.

- [x] **Step 4: host-test 회귀**

Run: `make -C fw/test test`
Expected: 7스위트 통과 (app_reg.c는 host-test 비대상).

- [x] **Step 5: 커밋**

```bash
git add fw/src/app_reg.c
git commit -m "feat(overload): app_reg START guard에 과부하 차단 추가

- 과부하 활성 중 START 무시 (SAMD20 SYS_ERROR 게이트 재현)
- seek_reset_active와 동일 직교 패턴 (별도 break, swallow 대칭 보존)"
```

---

## Task 5: Modbus STATUS OVLD 비트

**Files:**
- Modify: `fw/src/app_modbus.c`

- [x] **Step 1: include 추가**

`fw/src/app_modbus.c`의 include부에 추가:
```c
#include "app_overload.h"   /* app_overload_active (STATUS OVLD 비트) */
```

- [x] **Step 2: STATUS 레지스터에 OVLD 비트 OR**

기존 라인:
```c
    g_mb.holding[MB_REG_STATUS]      = running ? MB_STATUS_US : 0u;
```
을 아래로 교체:
```c
    g_mb.holding[MB_REG_STATUS]      = (uint16_t)((running ? MB_STATUS_US : 0u)
                                       | (app_overload_active() ? MB_STATUS_OVLD : 0u));
```

- [x] **Step 3: 빌드**

Run: `cd fw && env -u STM32_TOOLCHAIN cmake --build build 2>&1 | tail -10`
Expected: 0 warning, elf 생성.

- [x] **Step 4: host-test 회귀**

Run: `make -C fw/test test`
Expected: 7스위트 통과 (app_modbus.c는 host-test 비대상; app_modbus_core 순수 코어 무영향).

- [x] **Step 5: 커밋**

```bash
git add fw/src/app_modbus.c
git commit -m "feat(overload): Modbus STATUS OVLD 비트 (MB_STATUS_OVLD=0x04)"
```

---

## Task 6: 통합 검증 + 코드 리뷰 + HW 체크리스트

**Files:** (변경 없음 — 검증 전용)

- [x] **Step 1: 클린 빌드**

Run:
```bash
cd fw && env -u STM32_TOOLCHAIN cmake -B build -G Ninja && env -u STM32_TOOLCHAIN cmake --build build 2>&1 | tail -25
```
Expected: 0 warning(우리 코드), FLASH/RAM 출력, elf 생성.

- [x] **Step 2: 전체 host-test**

Run: `make -C fw/test test`
Expected: 7스위트(reg_calc/modbus_core/tcp_frame/weld_fsm/seek_reset_fsm/buzzer_fsm/**overload_fsm**) 전부 통과.

- [x] **Step 3: cpp-reviewer 리뷰**

슬라이스 C 전체 diff(`git diff <슬라이스시작>..HEAD`)를 cpp-reviewer로 리뷰. 중점: force-stop의 현재-소스 RUN_RELEASE가 모든 라이브 run 정지하는지, edge(stop/복구) vs level(릴레이/에러/부저) 수명 정확성, START guard 직교(swallow 대칭), ERR_OVLD 비트만 조작(weld OVTIME 보존), app_overload_active() 동시성(10ms tick 단일 라이터). CRITICAL/HIGH 0 확인.

- [x] **Step 4: HW 검증 체크리스트 기록 (보드 세션용)**

⚠ slice A 의존(PB13 read / PB3 relay / 부저) — A·C 함께 검증. 3가지 "비슷한" 결과를 구분(slice B 교훈):
- **① assert (PB13 HIGH ×5)**: 신호인가/스위치로 PB13 HIGH → 초음파 즉시 정지(LCD/Modbus run→idle, STATUS bit0→0) + LCD "OVER LOAD"/ICON_OL + 릴레이(PB3) ON(멀티미터/LED) + 부저 점멸 + Modbus STATUS OVLD(0x04)=1. **②와 구분**: 정지/에러/릴레이/STATUS가 동시에.
- **② START 차단**: 과부하 활성 중 LCD/Modbus START → 무반응(run 안 뜸). seek_reset 재튜닝 중에도 차단.
- **③ de-assert (PB13 LOW)**: 에러 클리어(ICON_OL=0 + 런 페이지 복귀) + 릴레이 OFF + STATUS OVLD=0 + **자동 RESET→SEEK 체인 관찰**(ICON_RESET→ICON_SEEK, 재튜닝 동안 START 차단 후 해제). 정지된 run은 **자동 재시작 안 함**(재-START 필요).
- **④ 디바운스**: 짧은 PB13 글리치(<5 tick HIGH)는 무시(정지 안 함).
- **⑤ 극성 sanity**: PB13 active-HIGH·PB3 릴레이 active-HIGH 실측(spec §6 미해결 항목). 정상 시 PB13 LOW 유지 확인(fail-safe 가설).
- **⑥ 회귀**: 직접-초음파 ceiling(~560ms)·ICON_RUN·DISP_* 미러 무회귀; 과부하 없을 때 STATUS OVLD=0.

- [x] **Step 5: 완료 + 다음 안내**

리뷰 반영분 커밋 후 HW 게이트. 메모리 `[[project-physical-io-layer]]` 갱신(슬라이스 C CODE-COMPLETE). 다음 = 슬라이스 D(물리명령+E-stop) 또는 A/B/C 묶음 HW 세션. HANDOFF 갱신.

---

## Self-Review (작성자 점검 완료)

- **Spec coverage**: spec §4-C 전 항목 — PB13 ×5 디바운스(Task1) / US 정지(Task3 force-stop) / ERR_OVLD+ICON_OL(Task2) / 부저(Task3) / 릴레이 PB3(Task3) / Modbus STATUS OVLD(Task5) / 해제·클리어(Task2 deassert + Task3) 커버. **복구는 사용자 하이브리드 결정 반영**: SAMD20 stop&wait + de-assert 자동 RESET→SEEK(Task3) — spec §4-C의 "복구 재사용"을 충실도 정정(SAMD20=레벨추종 자동클리어, 복구는 de-assert 트리거)하여 구현. START 차단(Task4)은 SAMD20 SYS_ERROR 게이트 재현.
- **Placeholder scan**: 없음. 모든 코드 step 완전 코드. FSM 6케이스 구체값.
- **Type consistency**: `overload_fsm_init/step` + `OVLD_EV_*`(헤더↔구현↔테스트) 일치; `app_overload_init/tick/active`(헤더↔구현↔호출 app.c/app_reg/app_modbus) 일치; `app_lcd_set_overload(bool)`(app_lcd.h 선언↔app_lcd_input.c 정의↔app_overload.c 호출) 일치; force-stop은 `app_reg_measure()->us_run_status` + `app_reg_command(US_CMD_RUN_RELEASE, src)`(기존 API).
- **충실도/advisor 반영**: SAMD20 1차 소스(레벨추종·자동클리어·점멸부저) + 사용자 하이브리드(de-assert 자동복구) ✓; stop=edge(영구)/나머지=level/clear=deassert 수명 분리 ✓; force-stop=현재소스 RUN_RELEASE(신규 API 없음) + weld 기계abort forward note ✓; ERR_OVLD만 조작(weld 보존) ✓; 페이지 복귀(run_page_for_mode 재사용) ✓; START guard 직교(별도 break, swallow 대칭) ✓.
- **브랜치**: slice A 위 스택(io/buzzer 의존) + A·B·C app_reg/main.c 충돌 forward note.
