# 물리 IO 슬라이스 D 구현 플랜 — 물리 명령 입력 + E-stop

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 패널 물리 버튼(B_START/PA15, B_RESET/PC10, PC11=EMSW|SEEK)을 `US_REMOTE` 통일 strict 소스로 `app_reg_command`에 디스패치하고, `model_type`으로 PC11 이중역할을 분기하며, E-stop을 **레벨-추종**(force-stop+SOL OFF+START 차단, 떼면 자동 클리어)으로 흡수한다.

**Architecture:** 순수 `app_input_fsm`(HAL-free, host-test) + 글루 `app_input`(10ms tick). 신규 io 함수 없음(슬라이스 A 헬퍼 소비). `app_reg` 편집 2곳(START guard에 `app_estop_active()` break, on-time ceiling에 `US_REMOTE`), Modbus STATUS에 `ESTOP`(0x02) 비트. force-stop은 `app_overload` 패턴(active 레벨 매-tick 재시도, source-matched).

**Tech Stack:** C11, arm-none-eabi-gcc(빌드 `env -u STM32_TOOLCHAIN cmake -S fw -B fw/build -G Ninja`), host-test cc(`make -C fw/test test`). 브랜치 `feat/physical-io-slice-d`(slice C 위 스택, 현재 체크아웃).

> **spec:** `docs/superpowers/specs/2026-06-27-physical-io-slice-d-design.md`
> **빌드 함정:** 신규 `.c` 추가 → `fw/CMakeLists.txt`의 `file(GLOB)`는 configure 시점만 평가 → **반드시 `cmake -S fw -B fw/build -G Ninja` reconfigure** 후 빌드.

---

## File Structure

| 파일 | 책임 | 신규/편집 |
|---|---|---|
| `fw/include/app_input_fsm.h` | 순수 FSM 인터페이스 (input_in_t/input_out_t/init/step) | 신규 |
| `fw/src/app_input_fsm.c` | edge-detect(START/RESET/SEEK) + E-stop 레벨추종 + model_type 분기 | 신규 |
| `fw/test/test_app_input_fsm.c` | host-test (8 시나리오) | 신규 |
| `fw/test/Makefile` | `app_input_fsm` 스위트 추가 | 편집 |
| `fw/include/app_input.h` | 글루 인터페이스 (init/tick/app_estop_active) | 신규 |
| `fw/src/app_input.c` | io_read→FSM→app_reg_command 디스패치 + E-stop force-stop/SOL | 신규 |
| `fw/src/main.c` | `app_input_init()` 배선 (app_overload_init 뒤) | 편집 |
| `fw/src/app.c` | `app_input_tick()` 배선 (app_overload_tick 뒤) | 편집 |
| `fw/src/app_reg.c` | START guard estop break + ceiling US_REMOTE | 편집 |
| `fw/src/app_modbus.c` | STATUS ESTOP 비트 | 편집 |

---

## Task 1: 순수 FSM `app_input_fsm` + host-test (TDD)

**Files:**
- Create: `fw/include/app_input_fsm.h`
- Create: `fw/src/app_input_fsm.c`
- Test: `fw/test/test_app_input_fsm.c`
- Modify: `fw/test/Makefile`

- [ ] **Step 1: 인터페이스 헤더 작성**

`fw/include/app_input_fsm.h`:
```c
/* fw/include/app_input_fsm.h — 순수 물리 입력 FSM (HAL-free, host-test).
 * B_START/B_RESET edge-detect + PC11 이중역할(model_type 분기): hand/multi=SEEK
 * active-LOW edge / std=EMSW active-HIGH 레벨추종. step()이 명령 엣지 이벤트 +
 * E-stop 레벨/진입엣지를 구조체로 반환. spec 2026-06-27-physical-io-slice-d §3. */
#pragma once
#include <stdint.h>

typedef struct {
    uint8_t start;        /* PA15 raw 0/1 (active-LOW: 0=눌림) */
    uint8_t reset;        /* PC10 raw 0/1 (active-LOW) */
    uint8_t estop_seek;   /* PC11 raw 0/1 (std=EMSW active-HIGH / hand·multi=SEEK active-LOW) */
    uint8_t model_type;   /* 0=hand 1=multi 2=std */
} input_in_t;

typedef struct {
    uint8_t start_press;    /* B_START 눌림 엣지 → US_CMD_START */
    uint8_t start_release;  /* B_START 뗌 엣지   → US_CMD_RUN_RELEASE */
    uint8_t reset_press;    /* B_RESET 눌림 엣지 → US_CMD_RESET */
    uint8_t seek_press;     /* B_SEEK 눌림 엣지(hand/multi) → US_CMD_SEEK */
    uint8_t estop_active;   /* E-stop 레벨 (std EMSW HIGH 동안 1) */
    uint8_t estop_enter;    /* E-stop 상승 엣지 1-shot → SOL OFF 트리거 */
} input_out_t;

void        input_fsm_init(void);
input_out_t input_fsm_step(const input_in_t *in);
```

- [ ] **Step 2: 실패하는 host-test 작성**

`fw/test/test_app_input_fsm.c`:
```c
/* fw/test/test_app_input_fsm.c — host unit tests, 순수 물리 입력 FSM.
 * 활성극성: START/RESET/SEEK active-LOW(0=눌림), EMSW active-HIGH(1=비상).
 * E-stop = 레벨추종(std 모드만); hand/multi에선 PC11=SEEK, EMSW 비활성. */
#include <stdio.h>
#include <stdint.h>
#include "app_input_fsm.h"

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

/* idle 입력(active-LOW 1, EMSW 0) — 부팅 무이벤트 */
static void test_idle_no_event(void)
{
    input_fsm_init();
    input_in_t in = { .start = 1u, .reset = 1u, .estop_seek = 1u, .model_type = 0u };
    input_out_t o = input_fsm_step(&in);
    CHECK_EQ(o.start_press, 0u);
    CHECK_EQ(o.reset_press, 0u);
    CHECK_EQ(o.seek_press, 0u);
    CHECK_EQ(o.estop_active, 0u);
}

/* B_START 모멘터리: 눌림→press, 유지→무이벤트, 뗌→release */
static void test_start_momentary(void)
{
    input_fsm_init();
    input_in_t in = { .start = 1u, .reset = 1u, .estop_seek = 1u, .model_type = 0u };
    (void)input_fsm_step(&in);                 /* idle */
    in.start = 0u;                             /* 눌림 */
    input_out_t o = input_fsm_step(&in);
    CHECK_EQ(o.start_press, 1u);
    CHECK_EQ(o.start_release, 0u);
    o = input_fsm_step(&in);                   /* 유지 */
    CHECK_EQ(o.start_press, 0u);
    in.start = 1u;                             /* 뗌 */
    o = input_fsm_step(&in);
    CHECK_EQ(o.start_release, 1u);
    CHECK_EQ(o.start_press, 0u);
}

/* B_RESET 눌림 엣지 */
static void test_reset_press_edge(void)
{
    input_fsm_init();
    input_in_t in = { .start = 1u, .reset = 1u, .estop_seek = 1u, .model_type = 2u };
    (void)input_fsm_step(&in);
    in.reset = 0u;
    input_out_t o = input_fsm_step(&in);
    CHECK_EQ(o.reset_press, 1u);
    o = input_fsm_step(&in);                   /* 유지 → 무이벤트 */
    CHECK_EQ(o.reset_press, 0u);
}

/* hand/multi: PC11 LOW → seek_press, EMSW 비활성 */
static void test_seek_in_hand_multi(void)
{
    input_fsm_init();
    input_in_t in = { .start = 1u, .reset = 1u, .estop_seek = 1u, .model_type = 1u };
    (void)input_fsm_step(&in);
    in.estop_seek = 0u;                        /* SEEK 눌림 (active-LOW) */
    input_out_t o = input_fsm_step(&in);
    CHECK_EQ(o.seek_press, 1u);
    CHECK_EQ(o.estop_active, 0u);              /* hand/multi에선 EMSW 비활성 */
    CHECK_EQ(o.estop_enter, 0u);
}

/* std: PC11 HIGH → estop_active + enter 1-shot, 유지→재진입 없음, LOW→자동 클리어 */
static void test_estop_level_follow_in_std(void)
{
    input_fsm_init();
    input_in_t in = { .start = 1u, .reset = 1u, .estop_seek = 0u, .model_type = 2u };
    (void)input_fsm_step(&in);                 /* EMSW idle(LOW) */
    in.estop_seek = 1u;                        /* EMSW 인가(HIGH) */
    input_out_t o = input_fsm_step(&in);
    CHECK_EQ(o.estop_active, 1u);
    CHECK_EQ(o.estop_enter, 1u);               /* 상승 엣지 1-shot */
    o = input_fsm_step(&in);                   /* HIGH 유지 */
    CHECK_EQ(o.estop_active, 1u);
    CHECK_EQ(o.estop_enter, 0u);               /* 재진입 엣지 없음 */
    in.estop_seek = 0u;                        /* EMSW 해제(LOW) */
    o = input_fsm_step(&in);
    CHECK_EQ(o.estop_active, 0u);              /* 자동 클리어 (RESET 불필요) */
}

/* std 모드에선 PC11을 SEEK로 보지 않음 (seek_press 없음) */
static void test_no_seek_in_std(void)
{
    input_fsm_init();
    input_in_t in = { .start = 1u, .reset = 1u, .estop_seek = 0u, .model_type = 2u };
    (void)input_fsm_step(&in);                 /* EMSW LOW */
    /* EMSW HIGH→LOW는 SEEK active-LOW와 같은 전이지만 std에선 seek_press 금지 */
    in.estop_seek = 1u; (void)input_fsm_step(&in);
    in.estop_seek = 0u;
    input_out_t o = input_fsm_step(&in);
    CHECK_EQ(o.seek_press, 0u);
}

/* hand/multi에선 EMSW HIGH라도 estop_active 0 (PC11=SEEK 전용) */
static void test_no_estop_in_hand_multi(void)
{
    input_fsm_init();
    input_in_t in = { .start = 1u, .reset = 1u, .estop_seek = 1u, .model_type = 0u };
    input_out_t o = input_fsm_step(&in);       /* estop_seek HIGH지만 hand 모드 */
    CHECK_EQ(o.estop_active, 0u);
    CHECK_EQ(o.estop_enter, 0u);
}

/* 동시 입력: START 눌림 + RESET 눌림 같은 step */
static void test_concurrent_start_reset(void)
{
    input_fsm_init();
    input_in_t in = { .start = 1u, .reset = 1u, .estop_seek = 1u, .model_type = 1u };
    (void)input_fsm_step(&in);
    in.start = 0u; in.reset = 0u;
    input_out_t o = input_fsm_step(&in);
    CHECK_EQ(o.start_press, 1u);
    CHECK_EQ(o.reset_press, 1u);
}

int main(void)
{
    test_idle_no_event();
    test_start_momentary();
    test_reset_press_edge();
    test_seek_in_hand_multi();
    test_estop_level_follow_in_std();
    test_no_seek_in_std();
    test_no_estop_in_hand_multi();
    test_concurrent_start_reset();
    if (failures) { printf("app_input_fsm: %d FAIL\n", failures); return 1; }
    printf("app_input_fsm: all tests passed\n");
    return 0;
}
```

- [ ] **Step 3: Makefile에 스위트 추가**

`fw/test/Makefile` 편집 — `BIN_OL` 다음에 추가:
```makefile
BIN_OL  := /tmp/gds_test_app_overload_fsm
BIN_IN  := /tmp/gds_test_app_input_fsm
```
`test:` 타깃에 `$(BIN_IN)` 추가 (의존 + 실행 줄):
```makefile
test: $(BIN_REG) $(BIN_MB) $(BIN_TCP) $(BIN_WELD) $(BIN_SR) $(BIN_BZ) $(BIN_OL) $(BIN_IN)
	$(BIN_REG)
	$(BIN_MB)
	$(BIN_TCP)
	$(BIN_WELD)
	$(BIN_SR)
	$(BIN_BZ)
	$(BIN_OL)
	$(BIN_IN)
```
`$(BIN_OL):` 룰 다음에 빌드 룰 추가:
```makefile
$(BIN_IN): test_app_input_fsm.c ../src/app_input_fsm.c ../include/app_input_fsm.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_app_input_fsm.c ../src/app_input_fsm.c
```
`clean:` 줄에 `$(BIN_IN)` 추가:
```makefile
clean:
	rm -f $(BIN_REG) $(BIN_MB) $(BIN_TCP) $(BIN_WELD) $(BIN_SR) $(BIN_BZ) $(BIN_OL) $(BIN_IN)
```

- [ ] **Step 4: 테스트 실패 확인**

Run: `make -C fw/test test`
Expected: FAIL — `app_input_fsm.c` 미존재로 링크 에러(undefined reference to `input_fsm_init`/`input_fsm_step`).

- [ ] **Step 5: FSM 구현**

`fw/src/app_input_fsm.c`:
```c
/* fw/src/app_input_fsm.c — 순수 물리 입력 FSM.
 * START/RESET/SEEK = active-LOW edge-detect(_bak), E-stop = 레벨추종(std EMSW).
 * PC11 이중역할: model_type<=1(hand/multi)=SEEK / ==2(std)=EMSW. 비활성 역할은
 * 레거시 main.c:1192 충실(hand/multi에서 EMSW=0 강제) + bak 동기로 모드전환 시
 * stale 엣지 회피. */
#include "app_input_fsm.h"

static uint8_t s_start_bak;   /* active-LOW idle = 1 */
static uint8_t s_reset_bak;   /* active-LOW idle = 1 */
static uint8_t s_seek_bak;    /* active-LOW idle = 1 */
static uint8_t s_emsw_bak;    /* active-HIGH idle = 0 */
static uint8_t s_estop_active;

void input_fsm_init(void)
{
    s_start_bak    = 1u;
    s_reset_bak    = 1u;
    s_seek_bak     = 1u;
    s_emsw_bak     = 0u;
    s_estop_active = 0u;
}

input_out_t input_fsm_step(const input_in_t *in)
{
    input_out_t out = { 0u, 0u, 0u, 0u, 0u, 0u };

    /* B_START (PA15 active-LOW): 모멘터리 hold-to-run */
    if (in->start != s_start_bak) {
        if (in->start == 0u) { out.start_press = 1u; }
        else                 { out.start_release = 1u; }
        s_start_bak = in->start;
    }

    /* B_RESET (PC10 active-LOW): 눌림 엣지 */
    if (in->reset != s_reset_bak) {
        if (in->reset == 0u) { out.reset_press = 1u; }
        s_reset_bak = in->reset;
    }

    /* PC11 이중역할 (model_type 매 step) */
    if (in->model_type <= 1u) {
        /* hand/multi → B_SEEK active-LOW */
        if (in->estop_seek != s_seek_bak) {
            if (in->estop_seek == 0u) { out.seek_press = 1u; }
            s_seek_bak = in->estop_seek;
        }
        /* EMSW 비활성 (레거시 main.c:1192 re_emsw=0); bak 0 동기로 std 복귀 시
         * 즉시 재진입 가능, estop 강제 해제. */
        s_emsw_bak     = 0u;
        s_estop_active = 0u;
    } else {
        /* std → EMSW active-HIGH 레벨추종 */
        if (in->estop_seek != s_emsw_bak) {
            if (in->estop_seek != 0u) { out.estop_enter = 1u; }  /* 상승 엣지 */
            s_emsw_bak = in->estop_seek;
        }
        s_estop_active = (uint8_t)(in->estop_seek != 0u);
        /* SEEK bak 동기 (std에선 SEEK 미발화, hand/multi 복귀 시 stale 엣지 회피). */
        s_seek_bak = in->estop_seek;
    }

    out.estop_active = s_estop_active;
    return out;
}
```

- [ ] **Step 6: 테스트 통과 확인**

Run: `make -C fw/test test`
Expected: PASS — 마지막 줄 `app_input_fsm: all tests passed` + 기존 7스위트도 PASS(총 8).

- [ ] **Step 7: 커밋**

```bash
git add fw/include/app_input_fsm.h fw/src/app_input_fsm.c fw/test/test_app_input_fsm.c fw/test/Makefile
git commit -m "feat(input): 순수 물리 입력 FSM + host-test (app_input_fsm)"
```

---

## Task 2: 글루 `app_input` + 배선

**Files:**
- Create: `fw/include/app_input.h`
- Create: `fw/src/app_input.c`
- Modify: `fw/src/main.c` (app_input_init — app_overload_init 뒤)
- Modify: `fw/src/app.c` (app_input_tick — app_overload_tick 뒤)

- [ ] **Step 1: 글루 헤더 작성**

`fw/include/app_input.h`:
```c
/* fw/include/app_input.h — 물리 명령 입력 + E-stop 글루 (10ms tick).
 * io_read_*(슬라이스 A) → app_input_fsm → app_reg_command(US_REMOTE) 디스패치.
 * E-stop(std EMSW)은 레벨추종 force-stop + io_sol_dn(off). app_reg START guard와
 * Modbus STATUS가 app_estop_active()를 조회. spec 2026-06-27-physical-io-slice-d. */
#pragma once
#include <stdint.h>

void    app_input_init(void);     /* boot: FSM reset (io_init + sys_tick 뒤) */
void    app_input_tick(void);     /* 슈퍼루프 10ms gate */
uint8_t app_estop_active(void);   /* 1 = E-stop 활성 (START 차단 / STATUS 비트) */
```

- [ ] **Step 2: 글루 구현**

`fw/src/app_input.c`:
```c
/* fw/src/app_input.c — 물리 명령 입력 + E-stop 글루.
 * B_START(모멘터리)/B_RESET/PC11(model_type 분기 SEEK|EMSW)을 매 10ms 스캔해
 * app_reg_command(US_REMOTE)로 디스패치. E-stop(std EMSW)은 레벨추종: 진입 엣지에
 * io_sol_dn(off) 1-shot, active 동안 force-stop 매-tick 재시도(app_overload 패턴),
 * 떼면 자동 클리어(RESET 불필요, 런 재시작 안 함 = 새 START 필요). */
#include "app_input.h"
#include "app_input_fsm.h"
#include "io.h"          /* io_read_start/reset/estop_seek, io_sol_dn */
#include "app_reg.h"     /* app_reg_command, app_reg_measure */
#include "app_lcd.h"     /* app_lcd_cfg, us_cmd_t, US_* */
#include "sys_tick.h"

#define INPUT_TICK_MS  10u

static uint32_t s_prev_ms;
static uint8_t  s_estop_active;

void app_input_init(void)
{
    input_fsm_init();
    s_prev_ms      = sys_tick_get_ms();
    s_estop_active = 0u;
}

uint8_t app_estop_active(void) { return s_estop_active; }

void app_input_tick(void)
{
    uint32_t now = sys_tick_get_ms();
    if ((uint32_t)(now - s_prev_ms) < INPUT_TICK_MS) {
        return;
    }
    s_prev_ms = now;

    input_in_t in;
    in.start      = io_read_start();
    in.reset      = io_read_reset();
    in.estop_seek = io_read_estop_seek();
    in.model_type = app_lcd_cfg()->model_type;

    input_out_t ev = input_fsm_step(&in);   /* 매 tick 실행: bak/엣지 항상 갱신 */
    s_estop_active = ev.estop_active;

    /* E-stop 진입 엣지: SOL OFF 1-shot (io_sol_dn idempotent). */
    if (ev.estop_enter != 0u) {
        io_sol_dn(false);
    }

    if (s_estop_active != 0u) {
        /* active 동안 force-stop 매-tick 재시도 (app_overload 패턴: us_run_status
         * 미러가 app_reg_tick 발행이라 1-iter lag — 다음 tick에 잡힘). source-matched
         * RUN_RELEASE; idempotent(IDLE→no-op). START는 app_reg guard(app_estop_active)
         * 가 차단. E-stop 활성 중엔 명령 버튼 디스패치 스킵(아래 return) — FSM step은
         * 이미 위에서 실행돼 bak이 갱신됐으므로 해제 시 stale 엣지 없음. */
        uint8_t src = app_reg_measure()->us_run_status;
        if (src != (uint8_t)US_IDLE) {
            app_reg_command(US_CMD_RUN_RELEASE, src);
        }
        return;
    }

    /* 명령 버튼 (US_REMOTE 통일 strict). START 가드(==US_IDLE + estop/overload/
     * seek_reset break)는 app_reg_command 내부. */
    if (ev.start_press != 0u)   { app_reg_command(US_CMD_START,       (uint8_t)US_REMOTE); }
    if (ev.start_release != 0u) { app_reg_command(US_CMD_RUN_RELEASE, (uint8_t)US_REMOTE); }
    if (ev.reset_press != 0u)   { app_reg_command(US_CMD_RESET,       (uint8_t)US_REMOTE); }
    if (ev.seek_press != 0u)    { app_reg_command(US_CMD_SEEK,        (uint8_t)US_REMOTE); }
}
```

- [ ] **Step 3: main.c init 배선**

`fw/src/main.c` — `app_overload_init();`(38행) 다음 줄에 추가:
```c
    app_overload_init();    /* 과부하 글루 (needs io_init + sys_tick up) */
    app_input_init();       /* 물리 명령 입력 + E-stop 글루 (needs io_init + sys_tick up) */
    app_modbus_init(); /* Stage C: USART6 occupancy decision (needs cfg loaded by app_init) */
```
(파일 상단에 `app_overload.h` extern/include가 있다면 `app_input` init은 `main.c`가 자체 extern 선언하는 패턴 확인 — main.c가 `extern void board_init(void);`처럼 직접 extern을 쓰면 `app_input_init`/`app_input_tick`도 동일하게; 헤더 include 패턴이면 `#include "app_input.h"` 추가. 기존 `app_overload_init` 선언 방식과 동일하게 맞출 것.)

- [ ] **Step 4: app.c tick 배선**

`fw/src/app.c` — include 블록에 추가(13행 `#include "app_overload.h"` 다음):
```c
#include "app_overload.h"
#include "app_input.h"
```
그리고 `app_overload_tick();`(90행, 2.55) 블록 다음에 추가:
```c
    /* 2.55 과부하 — 10 ms. ... */
    app_overload_tick();

    /* 2.57 물리 명령 입력 + E-stop — 10 ms. B_RESET/SEEK는 다음 줄
     * seek_reset_tick이 같은 iter 소비; B_START/force-stop은 app_reg_tick에 반영
     * (app_seek_reset_tick·app_reg_tick 앞 배치). */
    app_input_tick();

    /* 2.6 SEEK/RESET FSM — 10 ms cadence. ... */
    app_seek_reset_tick();
```

- [ ] **Step 5: reconfigure + 빌드 (신규 .c → GLOB reconfigure 필수)**

Run:
```bash
env -u STM32_TOOLCHAIN cmake -S fw -B fw/build -G Ninja
env -u STM32_TOOLCHAIN cmake --build fw/build 2>&1 | tail -5
```
Expected: 0-warning(우리 코드), elf 생성. (이 시점엔 `app_estop_active` 미사용이라 STATUS/guard 미연결 — Task 3/4에서 소비.)

- [ ] **Step 6: host 회귀**

Run: `make -C fw/test test`
Expected: 8스위트 PASS(`app_input_fsm: all tests passed` 포함).

- [ ] **Step 7: 커밋**

```bash
git add fw/include/app_input.h fw/src/app_input.c fw/src/main.c fw/src/app.c
git commit -m "feat(input): app_input 글루 + 배선 (US_REMOTE 디스패치 + E-stop force-stop)"
```

---

## Task 3: `app_reg` START guard estop break + on-time ceiling US_REMOTE

**Files:**
- Modify: `fw/src/app_reg.c` (include + START guard ~124 + ceiling ~282)

- [ ] **Step 1: include 추가**

`fw/src/app_reg.c` 상단 — `#include "app_overload.h"`(12행) 다음:
```c
#include "app_overload.h"   /* app_overload_active (START 차단) */
#include "app_input.h"      /* app_estop_active (START 차단, E-stop) */
```

- [ ] **Step 2: START guard에 estop break 추가**

`fw/src/app_reg.c` — `app_overload_active()` break(124-126행) 다음에 추가:
```c
            /* 과부하 활성 중 START 차단 (SAMD20 SYS_ERROR가 START 막음).
             * seek_reset_active와 동일 직교 — 별도 break (swallow consume 뒤). */
            if (app_overload_active() != 0u) {
                break;
            }
            /* E-stop 활성 중 START 차단 (SAMD20 SYS_ESTOP). overload와 동일 직교 —
             * 별도 break (swallow 대칭 보존). 레벨 기반(E-stop 떼면 자동 해제). */
            if (app_estop_active() != 0u) {
                break;
            }
            g_reg.us_run_status = src;   /* US_TOUCH or US_COMM */
```

- [ ] **Step 3: on-time ceiling에 US_REMOTE 추가**

`fw/src/app_reg.c` — ceiling 조건(282행) 편집:
```c
        uint8_t rs = g_reg.us_run_status;
        if ((rs == (uint8_t)US_TOUCH) || (rs == (uint8_t)US_COMM) ||
            (rs == (uint8_t)US_REMOTE)) {
```
주석(268-271행)도 정정 — "REMOTE ceiling lands with the REMOTE slice, NOT covered here" → "REMOTE도 슬라이스 D에서 포함됨" 취지로 갱신:
```c
    /* Run on-time ceiling (limit_on_time x10 ms, 0 = off, panel-editable).
     * TOUCH/COMM/REMOTE 모두 대상 (REMOTE = 물리 B_START, 슬라이스 D 추가).
     * 단 swallow_start(아래)는 TOUCH 전용: V30 RUN 버튼 data=0 quirk 대응이며,
     * 물리 B_START의 release 엣지는 신뢰성 있어 불필요. */
```
> ⚠ `swallow_start=1`은 `if (rs == US_TOUCH)`(293행 근처)로 TOUCH 전용 유지 — **수정 금지**(REMOTE/COMM는 swallow 없음).

- [ ] **Step 4: 빌드 + host 회귀**

Run:
```bash
env -u STM32_TOOLCHAIN cmake --build fw/build 2>&1 | tail -5
make -C fw/test test
```
Expected: 0-warning, elf 생성; host 8스위트 PASS(app_reg_calc 포함 무회귀).

- [ ] **Step 5: 커밋**

```bash
git add fw/src/app_reg.c
git commit -m "feat(reg): START guard에 E-stop break + on-time ceiling에 US_REMOTE 추가"
```

---

## Task 4: Modbus STATUS ESTOP 비트

**Files:**
- Modify: `fw/src/app_modbus.c` (include + STATUS OR ~88)

- [ ] **Step 1: include 추가**

`fw/src/app_modbus.c` — `#include "app_overload.h"`(19행) 다음:
```c
#include "app_overload.h"   /* app_overload_active (STATUS OVLD 비트) */
#include "app_input.h"      /* app_estop_active (STATUS ESTOP 비트) */
```

- [ ] **Step 2: STATUS에 ESTOP 비트 OR 추가**

`fw/src/app_modbus.c` — STATUS 조립(85-89행) 편집:
```c
    /* STATUS bit0 = run active (spec §3.1: us_run_status != US_IDLE);
     * OVLD = app_overload_active() (슬라이스 C); ESTOP = app_estop_active()
     * (슬라이스 D); OVTIME/OUTERR stay 0 until later slices. */
    g_mb.holding[MB_REG_STATUS]      = (uint16_t)((running ? MB_STATUS_US : 0u)
                                       | (app_overload_active() ? MB_STATUS_OVLD : 0u)
                                       | (app_estop_active()    ? MB_STATUS_ESTOP : 0u));
```
> `MB_STATUS_ESTOP=0x02`는 `app_modbus_core.h:49`에 이미 정의됨(HMI 계약) — 신규 #define 금지.

- [ ] **Step 3: 빌드 + host 회귀**

Run:
```bash
env -u STM32_TOOLCHAIN cmake --build fw/build 2>&1 | tail -5
make -C fw/test test
```
Expected: 0-warning, elf; host 8스위트 PASS.

- [ ] **Step 4: 커밋**

```bash
git add fw/src/app_modbus.c
git commit -m "feat(modbus): STATUS ESTOP 비트 (MB_STATUS_ESTOP=0x02, 슬라이스 D)"
```

---

## Task 5: 통합 검증 + 최종 리뷰

**Files:** (없음 — 검증만)

- [ ] **Step 1: 클린 reconfigure + 빌드 0-warning**

Run:
```bash
env -u STM32_TOOLCHAIN cmake -S fw -B fw/build -G Ninja
env -u STM32_TOOLCHAIN cmake --build fw/build --clean-first 2>&1 | grep -iE 'warning|error' | grep -v vendor
arm-none-eabi-size fw/build/gds_us_ctrl.elf | awk 'NR==2{f=($1+$2)/131072*100; r=($2+$3)/32768*100; printf "FLASH %.2f%%  RAM %.2f%%\n", f, r}'
```
Expected: 우리 코드 warning 0줄(vendor wiznet 경고는 무관); FLASH ~43.x%/RAM ~17.x% (slice C 43.13% 대비 소폭 증가).

- [ ] **Step 2: host 8스위트 PASS**

Run: `make -C fw/test test`
Expected: 8스위트 전부 PASS (`app_input_fsm: all tests passed` 포함).

- [ ] **Step 3: 최종 통합 cpp-reviewer**

`cpp-reviewer` 에이전트로 슬라이스 D diff(`git diff feat/physical-io-slice-c..HEAD`) 리뷰. 중점:
- E-stop 레벨추종 정합(진입 1-shot SOL / active force-stop 재시도 / 자동 클리어)
- START guard estop break의 swallow 대칭 보존(seek_reset/overload와 동일 패턴)
- ceiling US_REMOTE 추가 시 swallow TOUCH 전용 유지(회귀 없음)
- 순환 의존 없음(app_reg.c → app_input.h 함수선언만)
- model_type 분기 모드전환 stale 엣지 처리
Expected: 0 Crit/High (HW-게이팅 APPROVE). Minor는 커밋 또는 spec 기록.

- [ ] **Step 4: (리뷰 코멘트 반영 시) 커밋**

```bash
git add -A && git commit -m "refactor(input): cpp-reviewer 코멘트 반영 (슬라이스 D)"
```

---

## 검증/머지 (HW 게이트 — 본 플랜 범위 밖)

- **HW E2E는 실동작 rig 세션**(A/B/C/D 묶음, spec §9): 각 버튼→명령 효과(STATUS/ICON); E-stop(std)→즉시정지+SOL OFF(멀티미터)+START 차단+STATUS ESTOP(0x02)+해제 자동 ready; 극성 sanity; 직접-초음파 ceiling/ICON_RUN 무회귀 + US_REMOTE ceiling 확인.
- **머지**: HW PASS 후 A→B→C→D 의존순 `--no-ff` + 태그. 본 세션은 spec/코드 선행, **미머지 유지**.
- ⚠ 글루(`app_input`)는 host 커버리지 밖(io→FSM→app_reg 배선·force-stop·SOL) → cpp-review + HW가 실질 게이트.

---

## Self-Review (spec 대조)

- spec §1.1 In scope: B_START/RESET/SEEK 디스패치(Task 2) ✓ · E-stop 레벨추종(Task 1 FSM + Task 2 글루) ✓ · ceiling US_REMOTE(Task 3) ✓
- spec §4 명령 흐름: B_START 모멘터리(Task 1 test_start_momentary + Task 2 dispatch) ✓ · B_RESET→RESET ✓ · PC11 model_type 분기(Task 1 test_seek_in_hand_multi/test_estop_level_follow_in_std) ✓
- spec §5 E-stop: 진입 SOL 1-shot + active force-stop 재시도 + 자동 클리어(Task 2) ✓
- spec §6 START guard estop break(Task 3) ✓
- spec §7 ceiling US_REMOTE + swallow TOUCH 전용(Task 3) ✓
- spec §8 STATUS ESTOP(Task 4) ✓
- 타입 일관성: `input_in_t`/`input_out_t`/`input_fsm_init`/`input_fsm_step`/`app_estop_active`/`app_input_init`/`app_input_tick` — Task 1~4 전체 일치 ✓
- placeholder 없음 ✓
