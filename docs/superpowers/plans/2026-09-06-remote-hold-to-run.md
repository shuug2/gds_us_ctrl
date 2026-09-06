# 원격 hold-to-run 워치독 구현 계획

> **문서 요약**: 스펙 `2026-09-06-remote-hold-to-run-design.md` 를 6개 Task 로 나눈 실행 계획.
> 계약 헤더(레지스터 51칸·START 값 3종·`0x32 FEAT_CAP`) → 순수 FSM `app_hold_wdt_fsm` +
> host 스위트 17번째 → `app_reg_run_src()` 접근자 4줄 → `app_modbus.c` 글루(미러 1줄·START
> `switch`·tick 첫머리 step·init·로그 제외) → 문서 → 벤치 도구. **`app_reg.c` 소스 열거
> 3곳·게이트 닫힘 분기·`2026-08-02` §3.1 은 무변경**이며, 리뷰어는 그 diff 가 나타나면
> 리젝트한다. 각 Task 는 host 테스트(또는 0-warning 빌드)로 닫히고 커밋 1개를 남긴다.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 원격기가 START 를 **누르고 있는 동안만** 초음파가 나가도록, 유지 신호가 600 ms 끊기면 컨트롤러가 스스로 그 런을 세우는 hold 워치독을 넣는다.

**Architecture:** 새 레지스터는 `0x32 FEAT_CAP` 비트맵 1칸뿐이고 hold 시작(2)·유지 신호(3)는 기존 `START(0x1B)` 의 값 확장으로 나른다. 워치독은 HAL-free 순수 FSM(`app_hold_wdt_fsm`) + `app_modbus.c` 글루이며, hold 런은 새 소스값 없이 `US_COMM` 그대로라 30초 상한·energy·on-time·E-STOP·STOP 이 코드 무변경으로 승계된다. 구 펌웨어 fail-safe 는 현행 `== 1u` 디스패치가 그대로 제공한다.

**Tech Stack:** STM32F410 / arm-none-eabi-gcc / CMake+Ninja(`./fw.sh`) / host 테스트 `cc -std=c11`(`./fw.sh test`) / Modbus TCP 벤치 `docs/superpowers/tools/mb_tcp.py`

**Spec:** `docs/superpowers/specs/2026-09-06-remote-hold-to-run-design.md` (구현 정본). 입력 명세 = `docs/superpowers/specs/2026-09-05-remote-hold-to-run-requirements.md`.

## Global Constraints

- `MB_REG_COUNT` = **51** (50→51). FC03 51칸 응답 = 107 B ≤ `MB_RESP_MAX` 125. 여유 = 57 − 51 = **6칸**.
- `HOLD_WDT_MS` = **600** (`#ifndef` 가드, 벤치 `-D` 로만 덮어씀). host 테스트가 `200 ≤ HOLD_WDT_MS ≤ 1000` 을 고정한다.
- START 값: `MB_START_TAP 1u` / `MB_START_HOLD 2u` / `MB_START_KEEP 3u`. **값 불문 소거.**
- `MB_REG_FEAT_CAP 0x32u` / `MB_FEAT_HOLD_WDT 0x0001u`. **모델 무관 무조건 미러** — `#if defined(MODEL_REMOTE)` 를 새로 늘리지 않는다(스펙 §0 A: `#if` 0개).
- **무변경 3곳** (diff 에 나타나면 리젝트): `fw/src/app_reg.c` 의 `us_run_status` 열거 3곳(`reg_check_safety_ceiling` / `reg_check_auto_terminate` 두 조건) · `fw/src/app_modbus.c` 게이트 닫힘 분기의 소거·STOP 통과 로직(#13 로그 조건 1줄 제외) · `docs/superpowers/specs/2026-08-02-remote-enable-gate-decision.md` §3.1.
- 순수 모듈은 항상 컴파일, `#if` 는 글루 한 곳에만(`fw/include/define.h` 주석 규칙).
- 빌드 게이트: STD `./fw.sh` + REMOTE `MODEL=remote ./fw.sh` 둘 다 our-code **0-warning**, `./fw.sh test` 전 스위트 PASS.
- 커밋 메시지 끝: `Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>` + `Claude-Session: https://claude.ai/code/session_019cfNvQoFEVoSBYXWr4uAFa`.
- `fw/vendor/`·`ref/` 편집 금지. `fw/CMakeLists.txt` 의 `file(GLOB src/*.c)` 는 configure 타임 고정 — 새 `.c` 추가 후 `./fw.sh` 가 스탬프로 재구성한다(수동 cmake 시 `-B` 재구성 필요).

---

## 파일 구조

| 파일 | 책임 | Task |
|---|---|---|
| `fw/include/app_modbus_core.h` (수정) | 레지스터 계약 — COUNT·START 값·FEAT_CAP·주석 | 1 |
| `fw/test/test_app_modbus_core.c` (수정) | 51칸 경계·FEAT_CAP/START 값 wire 계약 고정 | 1 |
| `fw/include/app_hold_wdt_fsm.h` (신규) | 순수 FSM API + `HOLD_WDT_MS` | 2 |
| `fw/src/app_hold_wdt_fsm.c` (신규, ~40줄) | `init/arm/keep/step/armed` — 컨텍스트 구조체 전달 | 2 |
| `fw/test/test_app_hold_wdt_fsm.c` (신규) + `fw/test/Makefile` | host 스위트 17번째, 11케이스 | 2 |
| `fw/include/app_reg.h` + `fw/src/app_reg.c` (수정) | `app_reg_run_src()` 지연-0 접근자 4줄 | 3 |
| `fw/src/app_modbus.c` (수정) | 글루 — include·static·init·미러 1줄·START `switch`·tick 첫머리 step+trip·로그 제외 | 4 |
| `docs/changelog.md` · `docs/requirements.md` · `CLAUDE.md` · `HANDOFF.md` (수정) | 기록 | 5 |
| `docs/superpowers/tools/mb_hold.py` (신규) · `fw/include/define.h` 날짜 | 벤치 도구 + 플래시 직전 버전 날짜 | 6 |

---

### Task 1: 레지스터 계약 — 51칸 · START 값 · FEAT_CAP

**Files:**
- Modify: `fw/include/app_modbus_core.h:9-17` (COUNT+주석), `:58` (START 뒤), `:120-121` (CFG_CAP 뒤), `:166-169` (SEEK 주석)
- Test: `fw/test/test_app_modbus_core.c:89-92` (full-map), `:103-126` (bounds), 신규 `test_feat_cap_contract`

**Interfaces:**
- Produces: `MB_REG_COUNT 51u`, `MB_START_TAP 1u`, `MB_START_HOLD 2u`, `MB_START_KEEP 3u`, `MB_REG_FEAT_CAP 0x32u`, `MB_FEAT_HOLD_WDT 0x0001u` — Task 2·4 가 그대로 쓴다.

- [ ] **Step 1: 기존 경계 테스트를 51칸 기준으로 고쳐 실패하게 만든다**

`fw/test/test_app_modbus_core.c` `test_read_regs_bounds` 를 아래로 교체(49+2 는 이제 51 이내라 유효해지므로 펜스를 한 칸 민다):

```c
static void test_read_regs_bounds(void) {
    mb_core_t mb;
    uint8_t req[8], resp[MB_RESP_MAX];
    uint8_t fc = 0xEE;
    mb_core_init(&mb, 5);

    mk_req(req, 5, 0x03, 0x0032, 0x0002);   /* 50 + 2 = 52 > 51 */
    CHECK_EQ(mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc), 0);
    CHECK_EQ(fc, 0);
    mk_req(req, 5, 0x03, 0x0000, 0x0000);   /* zero count */
    CHECK_EQ(mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc), 0);
    mk_req(req, 5, 0x03, 0x0000, 0x0034);   /* count 52 > 51 */
    CHECK_EQ(mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc), 0);

    /* fence-posts: last valid register (0x32 = FEAT_CAP) reads fine; one past = silence */
    mk_req(req, 5, 0x03, 0x0032, 0x0001);   /* addr 50, num 1 -> ok (2026-09-06: 51칸) */
    CHECK_EQ(mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc), 7);
    CHECK_EQ(fc, 0x03);
    mk_req(req, 5, 0x03, 0x0033, 0x0001);   /* addr 51, num 1 -> silence */
    CHECK_EQ(mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc), 0);
}
```

`test_read_regs` 의 full-map 블록(`:89-92`) 뒤에 추가:

```c
    /* 2026-09-06: 51 regs from 0 -> 3 + 102 + 2 = 107; 52 -> silence */
    mk_req(req, 5, 0x03, 0x0000, 0x0033);
    n = mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc);
    CHECK_EQ(n, 107);
    mk_req(req, 5, 0x03, 0x0000, 0x0034);
    n = mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc);
    CHECK_EQ(n, 0);
```

`main()` 앞에 신규 테스트 추가, `main()` 의 `test_work_cnt_reset_req();` 다음 줄에 `test_feat_cap_contract();` 호출 추가:

```c
/* hold-to-run wire 계약 (spec 2026-09-06 §2.1): FEAT_CAP 주소·비트, START 값 3종,
 * FC06 이 0x32 에는 저장되고(미러가 되돌리는 건 글루) 0x33 은 범위 밖. */
static void test_feat_cap_contract(void) {
    mb_core_t mb;
    uint8_t req[8], resp[MB_RESP_MAX];
    uint8_t fc = 0xEE;
    mb_core_init(&mb, 5);

    CHECK_EQ(MB_REG_FEAT_CAP, 0x32);
    CHECK_EQ(MB_REG_FEAT_CAP < MB_REG_COUNT, 1);
    CHECK_EQ(MB_FEAT_HOLD_WDT, 0x0001);
    CHECK_EQ(MB_START_TAP, 1);
    CHECK_EQ(MB_START_HOLD, 2);
    CHECK_EQ(MB_START_KEEP, 3);

    mk_req(req, 5, 0x06, MB_REG_FEAT_CAP, 0x1234);   /* in range: stored + echoed */
    CHECK_EQ(mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc), 8);
    CHECK_EQ(mb.holding[MB_REG_FEAT_CAP], 0x1234);
    mk_req(req, 5, 0x06, 0x0033, 0x0001);            /* out of range: silence */
    CHECK_EQ(mb_core_decode(&mb, req, 8, MB_MODE_RTU, resp, &fc), 0);
}
```

- [ ] **Step 2: 실패 확인**

Run: `make -C fw/test /tmp/gds_test_app_modbus_core && /tmp/gds_test_app_modbus_core`
Expected: 컴파일 에러 `MB_REG_FEAT_CAP undeclared` (또는 `MB_START_TAP`).

- [ ] **Step 3: 계약 헤더 수정**

`fw/include/app_modbus_core.h:9` 를 교체하고 `:10-17` 주석의 수치를 갱신:

```c
#define MB_REG_COUNT    51u    /* samd20 holdingReg[50] + 0x32 FEAT_CAP (2026-09-06) */
/* 🔴 확장 한계 — 다음에 레지스터를 늘리려는 사람이 먼저 읽을 것.
 * ① **프레임 상한 57 레지스터.** FC03 응답 = 3 + N*2 + CRC2 이고 MB_RESP_MAX=125
 *    이므로 N<=57 이다(현재 51칸 = 107 B). 여유는 **6칸**뿐이다.
```
(②·③ 문단은 그대로.)

`:58` `#define MB_REG_START 0x1Bu ...` 바로 다음에 추가:

```c
/* START(0x1B) 의 값 — hold-to-run (spec 2026-09-06 §2.1). 값 1 이외는 **구 펌웨어
 * 전부**(hw-revA_fw-stage-c1 ~ 3.1.0)에서 어느 분기에도 안 걸린다 = fail-safe 가
 * 추가 코드 없이 성립. 소거는 값 불문. 유지 신호는 기동 권한이 없다 — 무장 중에만
 * 시각을 갱신하고, IDLE·트립 후·타 경로 정지 후에 도착하면 no-op. */
#define MB_START_TAP        1u   /* 탭 START — 기존 의미 그대로 (STOP·상한까지 가동) */
#define MB_START_HOLD       2u   /* hold 시작 — START + hold 워치독 무장 */
#define MB_START_KEEP       3u   /* 유지 신호 — 무장 중에만 last_keep 갱신 */
```

`:121` `#define MB_REG_CFG_CAP_MAGIC 0xFA01u` 다음에 추가:

```c
/* 기능 비트맵 (spec 2026-09-06 §2). **모델 무관 무조건 미러** — 미래 기능은 비트만
 * 추가한다(기능당 매직 1칸씩 늘리지 않는다). 소비 측 판정(연결당 1회 프로브):
 *   0x31 != 0xFA01                 -> 구 펌웨어, 전부 미지원
 *   0x31 == 0xFA01, 0x32×1 무응답   -> 비트맵 이전 신 펌웨어(3.1.0 포함), 미지원
 *   0x31 == 0xFA01, 0x32 응답       -> 비트별 확정 (bit=0 은 **확정적 미지원**)
 * 구 펌웨어는 51칸 읽기에 무응답이므로 지원 확인 전에는 50칸으로 폴링할 것. */
#define MB_REG_FEAT_CAP         0x32u
#define MB_FEAT_HOLD_WDT        0x0001u   /* bit0: START=2/3 hold 워치독 (T=HOLD_WDT_MS) */
```

`:169` "상태 기반 hold 가 필요해지면 그때 capability 를 신설한다." 를 아래로 교체:

```
 * 소비자 쪽 시간 hold 로 남긴다. 상태 기반 hold(원격 hold-to-run)는 0x32 FEAT_CAP
 * 비트맵으로 판별자를 갖고 신설됐다(2026-09-06) — 단 **이 SEEK/RESET 비트에는 여전히
 * 판별자가 없다.**
```

- [ ] **Step 4: 테스트 통과 확인**

Run: `./fw.sh test 2>&1 | tail -3`
Expected: `all checks PASSED` 포함, FAIL 0.

- [ ] **Step 5: 펌웨어 빌드가 깨지지 않았는지 확인**

Run: `./fw.sh 2>&1 | grep -ciE "warning|error"; MODEL=remote ./fw.sh 2>&1 | grep -ciE "warning|error"`
Expected: `0` `0`. (holding[] 이 2 B 늘어 RAM +2 B — FLASH 는 무변화 또는 ±수 B.)

- [ ] **Step 6: 커밋**

```bash
git add fw/include/app_modbus_core.h fw/test/test_app_modbus_core.c
git commit -m "feat(modbus): hold-to-run 계약 — 레지스터 51칸, START 값 3종, 0x32 FEAT_CAP 비트맵

spec 2026-09-06 §2. 새 칸은 0x32 하나(여유 7→6). hold 시작/유지 신호는
START(0x1B) 값 2/3 으로 — 구 펌웨어 전부가 == 1u 만 디스패치해 fail-safe 가
공짜. FC03 51칸=107B / 52칸 무응답 / FC06 0x33 무응답을 host 로 고정.

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_019cfNvQoFEVoSBYXWr4uAFa"
```

---

### Task 2: 순수 FSM `app_hold_wdt_fsm` + host 스위트 17번째

**Files:**
- Create: `fw/include/app_hold_wdt_fsm.h`, `fw/src/app_hold_wdt_fsm.c`, `fw/test/test_app_hold_wdt_fsm.c`
- Modify: `fw/test/Makefile:2-5` (헤더 주석), `:25` (BIN 정의), `:27` (test 의존·실행), `:96-98` (규칙 + clean)

**Interfaces:**
- Consumes: Task 1 의 `MB_START_*`, `MB_REG_FEAT_CAP`, `MB_FEAT_HOLD_WDT`, `MB_REG_COUNT`(wire 테스트에서만).
- Produces (Task 4 가 호출):
  ```c
  typedef struct { uint8_t armed; uint32_t last_keep_ms; } hold_wdt_t;
  void    hold_wdt_init (hold_wdt_t *w);
  void    hold_wdt_arm  (hold_wdt_t *w, uint32_t now_ms);
  void    hold_wdt_keep (hold_wdt_t *w, uint32_t now_ms);
  uint8_t hold_wdt_step (hold_wdt_t *w, uint32_t now_ms, uint8_t run_is_comm); /* 1 = 트립 */
  uint8_t hold_wdt_armed(const hold_wdt_t *w);
  #define HOLD_WDT_MS 600u  /* #ifndef 가드 */
  ```

- [ ] **Step 1: 실패하는 테스트 작성**

`fw/test/test_app_hold_wdt_fsm.c`:

```c
/* fw/test/test_app_hold_wdt_fsm.c — host unit tests, 원격 hold-to-run 워치독 순수 FSM.
 *
 * 계약 = spec 2026-09-06 §3(의사코드)·§4(세션 경계)·§7(이 표). 핵심 불변식 셋:
 *   ① 무장 전엔 절대 트립하지 않는다 — HMI/mbpoll 의 탭 런을 건드리지 않는다.
 *   ② keep 은 무장 중에만 시각을 갱신한다 — 기동 권한이 없다(R-3 ②).
 *   ③ 런 소스가 US_COMM 이 아니게 되면 세션이 조용히 끝난다(트립 아님) — 타 경로
 *      정지(STOP·E-STOP·30 s·패널) 뒤 keep 이 와도 재기동이 없다.
 * legacy 대응물 없음(신규 기능). */
#include <stdio.h>
#include <stdint.h>
#include "app_hold_wdt_fsm.h"
#include "app_modbus_core.h"

static int failures = 0;
#define CHECK_EQ(expr, expected) do {                                       \
    unsigned long a_ = (unsigned long)(expr);                              \
    unsigned long e_ = (unsigned long)(expected);                          \
    if (a_ != e_) {                                                         \
        printf("FAIL %s:%d  %s = %lu, expected %lu\n",                      \
               __FILE__, __LINE__, #expr, a_, e_);                          \
        failures++;                                                         \
    }                                                                       \
} while (0)

/* 1. 무장 전엔 step 이 아무리 돌아도 트립 0 */
static void test_unarmed_never_trips(void)
{
    hold_wdt_t w;
    hold_wdt_init(&w);
    CHECK_EQ(hold_wdt_armed(&w), 0);
    for (uint32_t t = 0u; t < 120000u; t += 100u) {
        CHECK_EQ(hold_wdt_step(&w, t, 1u), 0);
    }
    CHECK_EQ(hold_wdt_armed(&w), 0);
}

/* 2. 무장 전 keep 은 no-op — 기동 권한 없음 */
static void test_keep_before_arm_is_noop(void)
{
    hold_wdt_t w;
    hold_wdt_init(&w);
    hold_wdt_keep(&w, 100u);
    hold_wdt_keep(&w, 200u);
    CHECK_EQ(hold_wdt_armed(&w), 0);
    CHECK_EQ(hold_wdt_step(&w, 5000u, 1u), 0);
}

/* 3. 정상 경로: 150 ms 마다 keep → 60 s 무트립, 계속 무장 */
static void test_periodic_keep_holds(void)
{
    hold_wdt_t w;
    hold_wdt_init(&w);
    hold_wdt_arm(&w, 1000u);
    for (uint32_t t = 1000u; t < 61000u; t += 150u) {
        hold_wdt_keep(&w, t);
        CHECK_EQ(hold_wdt_step(&w, t + 10u, 1u), 0);
    }
    CHECK_EQ(hold_wdt_armed(&w), 1);
}

/* 4. 경계값: keep 없이 599 ms → 0, 600 ms → 1, 그 뒤 해제 */
static void test_trip_boundary(void)
{
    hold_wdt_t w;
    hold_wdt_init(&w);
    hold_wdt_arm(&w, 10000u);
    CHECK_EQ(hold_wdt_step(&w, 10000u + HOLD_WDT_MS - 1u, 1u), 0);
    CHECK_EQ(hold_wdt_armed(&w), 1);
    CHECK_EQ(hold_wdt_step(&w, 10000u + HOLD_WDT_MS, 1u), 1);
    CHECK_EQ(hold_wdt_armed(&w), 0);
}

/* 5. 경계 반대편: 정확히 T-1 간격 keep 은 영구 무트립 */
static void test_keep_at_t_minus_1_never_trips(void)
{
    hold_wdt_t w;
    hold_wdt_init(&w);
    hold_wdt_arm(&w, 0u);
    uint32_t t = 0u;
    for (int i = 0; i < 200; i++) {
        t += HOLD_WDT_MS - 1u;
        CHECK_EQ(hold_wdt_step(&w, t, 1u), 0);
        hold_wdt_keep(&w, t);
    }
    CHECK_EQ(hold_wdt_armed(&w), 1);
}

/* 6. 타 경로 정지(run_is_comm=0) → 트립 0, 세션 종료; 이후 keep+step 으로 재무장 없음 */
static void test_foreign_stop_ends_session_silently(void)
{
    hold_wdt_t w;
    hold_wdt_init(&w);
    hold_wdt_arm(&w, 1000u);
    hold_wdt_keep(&w, 1100u);
    CHECK_EQ(hold_wdt_step(&w, 1200u, 0u), 0);   /* STOP/E-STOP/30s/패널이 세웠다 */
    CHECK_EQ(hold_wdt_armed(&w), 0);
    hold_wdt_keep(&w, 1300u);                    /* 늦은 keep */
    CHECK_EQ(hold_wdt_step(&w, 1400u, 1u), 0);   /* 다른 마스터의 새 탭 런이어도 무관 */
    CHECK_EQ(hold_wdt_armed(&w), 0);
    CHECK_EQ(hold_wdt_step(&w, 9000u, 1u), 0);   /* 절대 트립 안 함 */
}

/* 7. 트립 후 keep 계속 → 두 번째 트립 없음 */
static void test_no_double_trip(void)
{
    hold_wdt_t w;
    hold_wdt_init(&w);
    hold_wdt_arm(&w, 0u);
    CHECK_EQ(hold_wdt_step(&w, HOLD_WDT_MS, 1u), 1);
    for (uint32_t t = HOLD_WDT_MS; t < HOLD_WDT_MS * 10u; t += 150u) {
        hold_wdt_keep(&w, t);
        CHECK_EQ(hold_wdt_step(&w, t, 1u), 0);
    }
    CHECK_EQ(hold_wdt_armed(&w), 0);
}

/* 8. arm 중복 호출(START=2 응답 유실 재시도) → 세션 1개, 기준 시각 갱신 */
static void test_rearm_refreshes(void)
{
    hold_wdt_t w;
    hold_wdt_init(&w);
    hold_wdt_arm(&w, 0u);
    hold_wdt_arm(&w, 500u);                       /* 재시도 */
    CHECK_EQ(hold_wdt_step(&w, 500u + HOLD_WDT_MS - 1u, 1u), 0);
    CHECK_EQ(hold_wdt_step(&w, 500u + HOLD_WDT_MS, 1u), 1);
}

/* 9. u32 랩 안전 */
static void test_wrap_safe(void)
{
    hold_wdt_t w;
    hold_wdt_init(&w);
    uint32_t t0 = 0xFFFFFF00u;
    hold_wdt_arm(&w, t0);
    CHECK_EQ(hold_wdt_step(&w, t0 + HOLD_WDT_MS - 1u, 1u), 0);   /* 랩 넘어감 */
    CHECK_EQ(hold_wdt_step(&w, t0 + HOLD_WDT_MS, 1u), 1);
}

/* 10. T 범위 계약 (요구사항 §2: 200 ≤ T ≤ 1000, 벤치 -D 가 새는 것 방지) */
static void test_timeout_in_contract_range(void)
{
    CHECK_EQ(HOLD_WDT_MS >= 200u, 1);
    CHECK_EQ(HOLD_WDT_MS <= 1000u, 1);
}

/* 11. wire 계약 — START 값 3종 상호 상이·비0, FEAT_CAP 주소/비트 */
static void test_wire_values_match_contract(void)
{
    CHECK_EQ(MB_START_TAP, 1);
    CHECK_EQ(MB_START_HOLD, 2);
    CHECK_EQ(MB_START_KEEP, 3);
    CHECK_EQ(MB_START_TAP != MB_START_HOLD && MB_START_HOLD != MB_START_KEEP &&
             MB_START_TAP != MB_START_KEEP, 1);
    CHECK_EQ(MB_FEAT_HOLD_WDT, 1);
    CHECK_EQ(MB_REG_FEAT_CAP, 0x32);
    CHECK_EQ(MB_REG_FEAT_CAP < MB_REG_COUNT, 1);
}

int main(void) {
    test_unarmed_never_trips();
    test_keep_before_arm_is_noop();
    test_periodic_keep_holds();
    test_trip_boundary();
    test_keep_at_t_minus_1_never_trips();
    test_foreign_stop_ends_session_silently();
    test_no_double_trip();
    test_rearm_refreshes();
    test_wrap_safe();
    test_timeout_in_contract_range();
    test_wire_values_match_contract();
    if (failures) { printf("%d check(s) FAILED\n", failures); return 1; }
    printf("app_hold_wdt_fsm: all tests passed\n");
    return 0;
}
```

`fw/test/Makefile` — `:2-5` 헤더 주석을 `remote_en_fsm / hold_wdt_fsm.   (총 17 = grep -c '^BIN_')` 로, `BIN_REN` 줄 다음에 `BIN_HWD := /tmp/gds_test_app_hold_wdt_fsm`, `test:` 의존 목록 끝에 `$(BIN_HWD)` 추가 + 실행 줄 `	$(BIN_HWD)` 를 `$(BIN_REN)` 다음에, `$(BIN_STG)` 규칙 다음에:

```make
$(BIN_HWD): test_app_hold_wdt_fsm.c ../src/app_hold_wdt_fsm.c ../include/app_hold_wdt_fsm.h ../include/app_modbus_core.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_app_hold_wdt_fsm.c ../src/app_hold_wdt_fsm.c
```

`clean:` 의 `rm -f` 목록 끝에 `$(BIN_HWD)` 추가.

- [ ] **Step 2: 실패 확인**

Run: `make -C fw/test /tmp/gds_test_app_hold_wdt_fsm`
Expected: `app_hold_wdt_fsm.h: No such file or directory`.

- [ ] **Step 3: 헤더 + 구현 작성**

`fw/include/app_hold_wdt_fsm.h`:

```c
/* fw/include/app_hold_wdt_fsm.h — 원격 hold-to-run 워치독 순수 FSM. HAL-free.
 *
 * 원격기 START 를 "누르고 있는 동안만 가동" 으로 만들려면 링크가 끊겼을 때 컨트롤러가
 * 스스로 서야 한다 — 원격기는 정지를 보낼 수 없다. 이 모듈은 hold 세션 하나의 시각만
 * 본다: START=2 로 무장, START=3 으로 갱신, T 안에 갱신이 없으면 트립(=글루가
 * RUN_RELEASE). 런 소스가 US_COMM 이 아니게 되면(누군가 세웠다) 조용히 끝난다.
 *
 * 요구사항: docs/superpowers/specs/2026-09-05-remote-hold-to-run-requirements.md R-1/R-3
 * 설계:     docs/superpowers/specs/2026-09-06-remote-hold-to-run-design.md §3·§4·§5
 *
 * 🔴 keep 에는 기동 권한이 없다 — 무장 중에만 시각을 갱신한다. 그렇지 않으면 E-STOP
 *    해제·30 s 상한·패널 정지 직후 도착한 늦은 유지 신호가 기계를 다시 돌린다.
 * 🔴 무장은 글루의 START=2 분기 한 곳에서만, 그것도 app_reg_start_allowed 일 때만.
 *    탭 START(=1)·LCD·물리 버튼·weld 는 이 컨텍스트에 접근 경로 자체가 없다. */
#pragma once
#include <stdint.h>

/* 유지 신호 타임아웃 (spec §5). 최악 적층 579 ms(P150 + 유실1 150 + 지터 100 +
 * 쓰기지연 131[정상 FRAM 포함] + FRAM 최악 추가 48) < 600. 500 이면 오정지.
 * 벤치 단축 빌드가 -D 로 덮어쓸 수 있게 #ifndef 가드 — host 테스트가 200~1000 을 고정. */
#ifndef HOLD_WDT_MS
#define HOLD_WDT_MS   600u
#endif

typedef struct {
    uint8_t  armed;          /* 1 = hold 세션 진행 중 */
    uint32_t last_keep_ms;   /* 마지막 arm/keep 시각 (sys_tick ms) */
} hold_wdt_t;

void    hold_wdt_init (hold_wdt_t *w);
/* START=2 수락 시. 중복 호출(응답 유실 재시도)은 기준 시각만 갱신. */
void    hold_wdt_arm  (hold_wdt_t *w, uint32_t now_ms);
/* START=3. armed 아니면 no-op. */
void    hold_wdt_keep (hold_wdt_t *w, uint32_t now_ms);
/* 매 tick. run_is_comm = (us_run_status == US_COMM, 지연 0 값).
 * 반환 1 = 트립(글루가 RUN_RELEASE 발행) — 트립 즉시 해제되어 두 번 트립하지 않는다.
 * run_is_comm == 0 이면 세션만 해제하고 0 을 돌려준다(누군가 이미 세웠다). */
uint8_t hold_wdt_step (hold_wdt_t *w, uint32_t now_ms, uint8_t run_is_comm);
uint8_t hold_wdt_armed(const hold_wdt_t *w);
```

`fw/src/app_hold_wdt_fsm.c`:

```c
/* fw/src/app_hold_wdt_fsm.c — 원격 hold-to-run 워치독 순수 FSM (spec 2026-09-06 §3). */
#include "app_hold_wdt_fsm.h"

/* 세션 초기화 */
void hold_wdt_init(hold_wdt_t *w)
{
    w->armed        = 0u;
    w->last_keep_ms = 0u;
}

/* 무장 — START=2 수락 지점 전용 */
void hold_wdt_arm(hold_wdt_t *w, uint32_t now_ms)
{
    w->armed        = 1u;
    w->last_keep_ms = now_ms;
}

/* 유지 신호 — 기동 권한 없음 */
void hold_wdt_keep(hold_wdt_t *w, uint32_t now_ms)
{
    if (w->armed != 0u) {
        w->last_keep_ms = now_ms;
    }
}

/* 한 step. 시간 비교는 u32 랩 안전 elapsed 형태. */
uint8_t hold_wdt_step(hold_wdt_t *w, uint32_t now_ms, uint8_t run_is_comm)
{
    if (w->armed == 0u) {
        return 0u;
    }
    if (run_is_comm == 0u) {
        /* 누군가 세웠다(STOP·E-STOP·30 s·energy·패널) — 세션 종료, 트립 아님.
         * 이후 keep 은 no-op 이 되어 재기동 경로가 없다. */
        w->armed = 0u;
        return 0u;
    }
    if ((uint32_t)(now_ms - w->last_keep_ms) >= HOLD_WDT_MS) {
        w->armed = 0u;
        return 1u;
    }
    return 0u;
}

/* 무장 여부 */
uint8_t hold_wdt_armed(const hold_wdt_t *w)
{
    return w->armed;
}
```

- [ ] **Step 4: 테스트 통과 확인**

Run: `./fw.sh test 2>&1 | grep -E "hold_wdt|FAIL"`
Expected: `app_hold_wdt_fsm: all tests passed`, FAIL 없음. 전체 스위트 17개 실행.

- [ ] **Step 5: 펌웨어에 링크되는지 확인 (아직 호출자 없음 — 링커가 버려도 정상)**

Run: `./fw.sh 2>&1 | grep -ciE "warning|error"`
Expected: `0`. (`fw.sh` 가 소스 목록 변경을 감지해 재구성한다 — `[fw.sh] 소스 목록 변경 감지` 로그가 떠야 한다.)

- [ ] **Step 6: 커밋**

```bash
git add fw/include/app_hold_wdt_fsm.h fw/src/app_hold_wdt_fsm.c fw/test/test_app_hold_wdt_fsm.c fw/test/Makefile
git commit -m "feat(hold-wdt): 순수 FSM app_hold_wdt_fsm 신설 — host 스위트 17번째

spec 2026-09-06 §3. arm/keep/step 세 함수, 컨텍스트 구조체 전달. keep 은 무장
중에만 시각 갱신(기동 권한 없음), run_is_comm=0 이면 조용히 세션 종료(트립
아님). T=HOLD_WDT_MS 600 (#ifndef, host 가 200~1000 고정). 11케이스: 경계
599/600, u32 랩, 무권한 keep, 타 경로 정지 후 재무장 없음, 중복 arm.

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_019cfNvQoFEVoSBYXWr4uAFa"
```

---

### Task 3: `app_reg_run_src()` 지연-0 접근자

**Files:**
- Modify: `fw/include/app_reg.h:42-43` (`app_reg_measure` 선언 뒤), `fw/src/app_reg.c:275-278` (`app_reg_measure` 정의 뒤)

**Interfaces:**
- Produces: `uint8_t app_reg_run_src(void);` — `g_reg.us_run_status` 원본(`US_IDLE/REMOTE/TOUCH/COMM/CYCLE`, `app_lcd.h:67`). Task 4 의 `hold_wdt_step` 입력.

- [ ] **Step 1: 선언 추가**

`fw/include/app_reg.h` `const lcd_measure_t *app_reg_measure(void);` 다음에:

```c
/* 런 소스 즉시 읽기 — g_measure 게시(2 ms 게이트)를 거치지 않는 **지연 0** 값.
 * hold 워치독 세션 경계(spec 2026-09-06 §4 ②)가 "관측 사이 mutator ≤1" 불변식에
 * 의존하므로 stale 미러(app_lcd_measure()->us_run_status)로는 안 된다 — 상한 정지
 * 직후의 탭 START 가 세션을 상속받는 구멍이 생긴다. 읽기 전용, 거동 0. */
uint8_t app_reg_run_src(void);
```

- [ ] **Step 2: 정의 추가**

`fw/src/app_reg.c` `app_reg_measure` 정의 다음에:

```c
/* 런 소스 즉시 읽기 (hold 워치독 전용) */
uint8_t app_reg_run_src(void)
{
    return g_reg.us_run_status;
}
```

- [ ] **Step 3: 빌드 확인 (host 테스트 없음 — HAL 모듈, 4줄 읽기 전용)**

Run: `./fw.sh 2>&1 | grep -ciE "warning|error"; MODEL=remote ./fw.sh 2>&1 | grep -ciE "warning|error"`
Expected: `0` `0`. 호출자가 없어 링커가 버릴 수 있다 — 정상.

- [ ] **Step 4: `app_reg.c` 의 소스 열거 3곳이 diff 에 없는지 확인**

Run: `git diff -U0 fw/src/app_reg.c | grep -c "US_TOUCH) || (rs =="`
Expected: `0` (열거 라인 무변경).

- [ ] **Step 5: 커밋**

```bash
git add fw/include/app_reg.h fw/src/app_reg.c
git commit -m "feat(reg): app_reg_run_src() — us_run_status 지연-0 접근자 (hold 워치독용)

spec 2026-09-06 §3 #14. g_measure 게시는 2 ms 게이트라 stale 하고, 세션 경계
불변식(관측 사이 mutator ≤1)은 즉시값을 요구한다. 읽기 전용 4줄, 거동 0.
소스 열거 3곳 무변경.

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_019cfNvQoFEVoSBYXWr4uAFa"
```

---

### Task 4: `app_modbus.c` 글루 — 미러 · START switch · tick step · init · 로그 제외

**Files:**
- Modify: `fw/src/app_modbus.c` — `:23-27` include 블록, `:50-53` static 블록, `:254` 미러, `:323` 게이트 닫힘 로그 조건, `:363-373` START 분기, `:685-693` init, `:714` tick 첫머리

**Interfaces:**
- Consumes: Task 1 상수, Task 2 `hold_wdt_*`, Task 3 `app_reg_run_src()`, 기존 `app_reg_start_allowed()`(`app_reg.h:56`), `app_reg_command()`, `app_lcd_hook_set_pot()`, `sys_tick_get_ms()`, `mon_printf()`.
- Produces: FC03 `0x32 = 0x0001`(두 모델), START=2/3 거동, mon `[mb] hold wdt trip`.

- [ ] **Step 1: include + static**

`fw/src/app_modbus.c:26` `#include "io.h"` 다음 줄에:

```c
#include "app_hold_wdt_fsm.h"   /* 원격 hold-to-run 워치독 (spec 2026-09-06) */
```

`:53` `static cfg_stage_t s_stg;` 다음에:

```c
/* hold 워치독 세션 (비영속). 무장 지점은 apply_writes 의 START=2 분기 한 곳뿐이다. */
static hold_wdt_t s_hwd;
```

- [ ] **Step 2: init**

`app_modbus_init` 의 `cfg_stage_init(&s_stg);`(`:692`) 바로 앞에:

```c
    hold_wdt_init(&s_hwd);
```

- [ ] **Step 3: 미러 1줄 (모델 무관)**

`mirror_live` 의 `g_mb.holding[MB_REG_CFG_CAP] = MB_REG_CFG_CAP_MAGIC;`(`:254`) 다음에:

```c
    /* 기능 비트맵 — **모델 무관 무조건**(spec §0 A: STD 도 hold 워치독을 갖는다).
     * 0x31 매직과 조합해 "신 펌웨어인데 비트 0" 이 확정적 미지원으로 읽힌다. */
    g_mb.holding[MB_REG_FEAT_CAP]  = MB_FEAT_HOLD_WDT;
```

- [ ] **Step 4: START 분기를 값 switch 로**

`:363-373` 의 `} else if (g_mb.holding[MB_REG_START] == 1u) { ... }` 블록 전체를 아래로 교체(기존 set_pot 주석 8줄은 TAP 분기 안으로 그대로 옮긴다):

```c
    } else if (g_mb.holding[MB_REG_START] != 0u) {
        /* hold-to-run (spec 2026-09-06 §2.1·§4). 소거는 **값 불문** — CFG_CTRL 과 동형.
         * 값 1 = 탭 START(기존 그대로) / 2 = hold 시작 + 워치독 무장 / 3 = 유지 신호 /
         * 그 외 = 무시. 🔴 무장 지점은 이 START=2 분기 하나뿐이고 start_allowed 일
         * 때만이다 — "무장됐다 ⇒ 방금 시작된 런은 우리 hold 런" 이 세션 경계의 첫 겹. */
        uint16_t sv  = g_mb.holding[MB_REG_START];
        uint32_t now = sys_tick_get_ms();
        g_mb.holding[MB_REG_START] = 0u;
        if (sv == MB_START_TAP) {
            app_reg_command(US_CMD_START, (uint8_t)US_COMM);
            /* samd20 comm START 는 같은 자리에서 진폭 pot 을 쓴다(main.c:4400-4401).
             * LCD RUN-press 경로(app_lcd_input.c:217/242)와 동형 — 무조건 write.
             * 거부된 START 여도 출력이 없어 무해(멱등 1바이트).
             * 구 가드 `app_lcd_measure()->us_run_status == US_COMM` 는 g_measure 가
             * app_reg_tick(app_modbus_tick 앞)에서만 게시돼 START 를 수락한 그 iter 에
             * 항상 FALSE 였다 — 2026-06-12 리뷰 NOTE 가 "set_pot 이 log stub 이라 무해"
             * 로 남겼으나 2026-06-28 I2C_POT 실구동 이후 전제가 깨졌다(2026-09-04 fix). */
            app_lcd_hook_set_pot(cfg->output_power);
        } else if (sv == MB_START_HOLD) {
            if (app_reg_start_allowed()) {
                app_reg_command(US_CMD_START, (uint8_t)US_COMM);
                app_lcd_hook_set_pot(cfg->output_power);   /* 탭과 동형, 1회 */
                hold_wdt_arm(&s_hwd, now);
            } else if (hold_wdt_armed(&s_hwd) != 0u) {
                hold_wdt_keep(&s_hwd, now);   /* START=2 응답 유실 재시도 흡수 */
            }
            /* start_allowed 거짓 + 미무장 = 다른 마스터의 탭 런이 도는 중 — 무시.
             * 그 런은 워치독 대상이 아니다(§3.1 무변경의 근거). */
        } else if (sv == MB_START_KEEP) {
            hold_wdt_keep(&s_hwd, now);       /* armed 아니면 no-op = 기동 권한 없음 */
        }
    } else if (g_mb.holding[MB_REG_STOP] == 1u) {
```

- [ ] **Step 5: tick 첫머리 step + 트립 (분기 밖)**

`app_modbus_tick` 의 `remote_en_step();`(`:714`) 다음, `cfg_stage_tick(...)` 앞에:

```c
    /* hold 워치독 — 분기 밖 첫머리. RTU 점유/TCP/미점유 어디로 빠져도 시간이 흘러야
     * 한다: apply_config 가 링크를 해제해도 hold 런은 T 안에 서야 한다.
     * 🔴 불변식(spec §4 ②): 연속한 두 step 사이에 us_run_status 를 US_COMM 으로
     * 바꿀 수 있는 것은 같은 tick 의 apply_writes **1건**뿐이다(RTU = tick 당 1
     * 프레임, TCP = poll 당 FC06 1건 — app_modbus_tcp.c 의 break). tick 당 FC06
     * apply 를 2건으로 늘리면 "정지+재시작" 이 한 관측 구간에 들어가 다른 마스터의
     * 탭 런이 hold 세션을 상속받는다 — 그 변경은 이 워치독을 함께 고쳐야 한다. */
    {
        uint32_t now_hwd = sys_tick_get_ms();
        uint8_t  run_is_comm = (app_reg_run_src() == (uint8_t)US_COMM) ? 1u : 0u;
        if (hold_wdt_step(&s_hwd, now_hwd, run_is_comm) != 0u) {
            app_reg_command(US_CMD_RUN_RELEASE, (uint8_t)US_COMM);   /* STOP 과 동일 */
            mon_printf("[mb] hold wdt trip\r\n");
        }
    }
```

- [ ] **Step 6: 게이트 닫힘 로그에서 유지 신호 제외 (로직 무변경)**

`:323` `else if (g_mb.holding[MB_REG_START] != 0u) { blocked = MB_REG_START; }` 를:

```c
        else if ((g_mb.holding[MB_REG_START] != 0u) &&
                 (g_mb.holding[MB_REG_START] != MB_START_KEEP)) { blocked = MB_REG_START; }
        /* START=KEEP 은 로그에서 제외 — 닫힌 게이트에 초당 ~7건 오면 mon 을 덮는다.
         * 소거는 아래에서 값 불문 그대로(유지 신호가 굶어 ≤T 트립 = R-11 부수 효과). */
```

- [ ] **Step 7: 두 모델 0-warning 빌드 + host 전 스위트**

Run: `./fw.sh 2>&1 | grep -iE "warning|error|FLASH:"; MODEL=remote ./fw.sh 2>&1 | grep -iE "warning|error|FLASH:"; ./fw.sh test 2>&1 | grep -c "passed\|PASSED"`
Expected: warning/error 0, FLASH 두 줄(각 +수백 B 이내), 마지막 수 = `17`.

- [ ] **Step 8: 무변경 3곳 리뷰 체크**

Run: `git diff -U0 main -- fw/src/app_reg.c | grep -c "rs ==" ; git diff -U0 -- fw/src/app_modbus.c | grep -cE "^\-.*(holding\[MB_REG_(RESET|SEEK|START|STOP)\] = 0u|stop_passed)"`
Expected: `0` `0` — 열거 3곳·게이트 소거/STOP 통과 라인이 삭제·수정되지 않았다.

- [ ] **Step 9: 커밋**

```bash
git add fw/src/app_modbus.c
git commit -m "feat(modbus): hold-to-run 글루 — START=2 무장·START=3 유지·tick 워치독·0x32 미러

spec 2026-09-06 §3 #8-#13. START 분기를 값 불문 소거 + 값별 디스패치로(CFG_CTRL
동형): 1=탭(기존), 2=start_allowed 일 때만 START+pot+무장, 3=keep(무권한).
tick 첫머리(분기 밖)에서 step — 트립이면 STOP 과 동일한 RUN_RELEASE(US_COMM).
hold 런은 US_COMM 그대로라 30s/energy/on-time/E-STOP/패널 정지 전부 승계.
0x32 FEAT_CAP=0x0001 모델 무관 미러. 게이트 닫힘 분기는 로그 조건 1줄만
(KEEP 제외) — 소거·STOP 통과 로직 무변경. 불변식 주석: tick 당 FC06 apply
1건이 세션 경계의 전제.

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_019cfNvQoFEVoSBYXWr4uAFa"
```

---

### Task 5: 문서 — changelog · requirements · CLAUDE.md · HANDOFF

**Files:**
- Modify: `docs/changelog.md:3` (`## [Unreleased]` 아래), `docs/requirements.md:92` (FW3-6 항목 뒤 새 줄), `CLAUDE.md` "레지스터 여유 7칸" 문장, `HANDOFF.md` 열린 항목

- [ ] **Step 1: changelog Unreleased 항목**

`docs/changelog.md` `## [Unreleased]` 바로 아래에:

```markdown
### 2026-09-06 — feat: 원격 hold-to-run 워치독 (요구사항 2026-09-05 R-1/2/3/4/6) — HW 벤치 대기

- **무엇**: 원격기 START 를 "누르고 있는 동안만 가동" 으로 만들기 위해, 유지 신호가 **T=600 ms** 끊기면 컨트롤러가 스스로 그 런을 세운다. 단절 시 잔여 가동 30 s → **0.6 s**.
- **계약**(벤치 PASS 후 확정): `0x32 FEAT_CAP` 비트맵 R(bit0 = hold 워치독, **모델 무관 미러**) · `START(0x1B)` 값 확장 1=탭(기존)/2=hold 시작/3=유지 신호, 값 불문 소거 · `MB_REG_COUNT` 50→**51**(여유 6). 소비자 판정 = `0x31==0xFA01` → 연결당 1회 `0x32×1` 프로브 → bit0.
- **구 펌웨어 fail-safe 공짜**: `hw-revA_fw-stage-c1` ~ 3.1.0 전부 START `== 1u` 만 디스패치 — 값 2/3 은 어느 분기에도 안 걸린다(3지점 `git show` 확인).
- **hold 런 = `US_COMM` 동형** — 새 소스값 없음. `app_reg.c` 소스 열거 3곳 **무변경**으로 30초 상한·energy·on-time·E-STOP·STOP·패널 정지 전부 승계. 게이트 닫힘 분기 **무변경**(값 불문 소거가 유지 신호를 굶겨 ≤T 트립). `2026-08-02` §3.1 **무변경** — 주어가 "가동 중 해제" 라는 사건이고 hold 런은 그 규칙이 판단한 적 없는 범주.
- **구조**: 순수 FSM `app_hold_wdt_fsm`(~40줄, host 17번째 스위트 11케이스) + `app_modbus.c` 글루(START `switch` · tick 첫머리 step · 미러 1줄) + `app_reg_run_src()` 지연-0 접근자 4줄. `#if` 신설 0.
- **T 근거**: 최악 적층 P150 + 유실1 150 + 지터 100 + 쓰기지연 131(정상 FRAM ≈2 포함) + FRAM 최악 추가 48 = **579 < 600**. 500 이면 오정지. 원격기 산식에 FRAM 항이 없었다.
- **사용자 결정**: PC8 과 **독립**(순서 결정 불요) · STD 포함 · 패널 TOUCH 범위 밖 · R-5 이연. PC8 = 초기 설정 스위치(인터록 아님) → R-11 부수 효과, H-11 이연.
- 게이트: STD/REMOTE 0-warning · host 17스위트 PASS. **벤치 H-0~H-16 = spec §8** (RTU H-15 이연 — 9600 에서 FC03 50칸 ≈120 ms 라 P=150 불가, hold 는 TCP 전용).
```

- [ ] **Step 2: requirements.md — FW3 목록 아래 한 줄**

`docs/requirements.md:92` FW3-6 항목의 마지막 줄 다음에:

```markdown
7. **원격 hold-to-run 워치독** (2026-09-06, 원격기 요구 R-1~R-11) — START(0x1B) 값 2=hold 시작/3=유지, 유지 신호 600 ms 소실 → 자동 정지. 판별 `0x32 FEAT_CAP` bit0. 설계 `docs/superpowers/specs/2026-09-06-remote-hold-to-run-design.md`. **HW 벤치 대기**(spec §8 H-0~H-16, TCP 전용).
```

- [ ] **Step 3: CLAUDE.md 여유 칸수**

`CLAUDE.md` 의 `⚠ **레지스터 여유 7칸** — FC03 응답 상한 57칸(현재 50칸).` 을 `⚠ **레지스터 여유 6칸** — FC03 응답 상한 57칸(현재 51칸, `0x32 FEAT_CAP` 2026-09-06).` 로.

- [ ] **Step 4: HANDOFF 열린 항목 추가**

`HANDOFF.md` 의 "★ 다음 세션 진입 순서" 블록 뒤(또는 열린 항목 목록 선두)에:

```markdown
- ⬜ **hold-to-run 벤치** — 코드 완료(host 17 PASS). `docs/superpowers/specs/2026-09-06-remote-hold-to-run-design.md` §8 H-0~H-16. **H-0 은 새 빌드 플래시 전 현 보드(405f95e)로**. 도구 `docs/superpowers/tools/mb_hold.py`. PASS 후 `app_modbus_core.h` 값을 계약 확정으로 원격기·HMI 에 통보.
```

- [ ] **Step 5: 커밋**

```bash
git add docs/changelog.md docs/requirements.md CLAUDE.md HANDOFF.md
git commit -m "docs: hold-to-run 워치독 코드-완료 기록 — changelog/requirements/CLAUDE/HANDOFF

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_019cfNvQoFEVoSBYXWr4uAFa"
```

---

### Task 6: 벤치 도구 `mb_hold.py` + 플래시 직전 버전 날짜

**Files:**
- Create: `docs/superpowers/tools/mb_hold.py`
- Modify (플래시 직전에만): `fw/include/define.h` `VERSION_MSG` 3종의 날짜

**Interfaces:**
- Consumes: `docs/superpowers/tools/mb_tcp.py` 의 `MB(host)` 컨텍스트 매니저, `.write(addr, val)`, `.r1(addr)`.

- [ ] **Step 1: 유지 신호 송신기 작성**

`docs/superpowers/tools/mb_hold.py`:

```python
#!/usr/bin/env python3
"""mb_hold.py — hold-to-run 벤치용 유지 신호 송신기 (spec 2026-09-06 §8 H-2/H-3/H-9).

사용:  python3 mb_hold.py [host] [period_ms] [start_val]
       host      기본 192.168.1.199
       period_ms 유지 신호 주기, 기본 150 (H-9 는 550 / 650)
       start_val 첫 쓰기 값, 기본 2 (H-5 는 0 = START 안 보내고 keep 만)
동작:  START=start_val 1회 → period 마다 START=3 + STATUS(0x1D) bit0 출력.
       Ctrl-C = 손 뗌. 이후 2 s 동안 100 ms 마다 US 를 찍어 트립 시점을 잡는다.
⚠ mbpoll 은 이 환경에서 동작하지 않는다(bench-results §4). nc -z 금지.
"""
import sys, time
from mb_tcp import MB

host      = sys.argv[1] if len(sys.argv) > 1 else '192.168.1.199'
period    = (float(sys.argv[2]) if len(sys.argv) > 2 else 150.0) / 1000.0
start_val = int(sys.argv[3]) if len(sys.argv) > 3 else 2

with MB(host) as mb:
    t0 = time.monotonic()
    if start_val:
        mb.write(0x1B, start_val)
        print(f"{0.000:7.3f}s START={start_val}", flush=True)
    try:
        while True:
            mb.write(0x1B, 3)
            us = mb.r1(0x1D) & 1
            print(f"{time.monotonic()-t0:7.3f}s keep  US={us}", flush=True)
            time.sleep(period)
    except KeyboardInterrupt:
        print("--- keep 중단 (손 뗌) ---", flush=True)
    t_rel = time.monotonic()
    for _ in range(20):
        us = mb.r1(0x1D) & 1
        print(f"{time.monotonic()-t0:7.3f}s  +{(time.monotonic()-t_rel)*1000:4.0f}ms  US={us}", flush=True)
        if us == 0:
            break
        time.sleep(0.1)
```

- [ ] **Step 2: 문법·import 확인 (보드 없이)**

Run: `cd docs/superpowers/tools && python3 -c "import ast,sys; ast.parse(open('mb_hold.py').read()); import mb_tcp; print('ok')"`
Expected: `ok`.

- [ ] **Step 3: 커밋**

```bash
git add docs/superpowers/tools/mb_hold.py
git commit -m "tools: mb_hold.py — hold-to-run 벤치 유지 신호 송신기 (START=2 → START=3 주기 송신, Ctrl-C=손 뗌)

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_019cfNvQoFEVoSBYXWr4uAFa"
```

- [ ] **Step 4: (벤치 세션에서, 플래시 직전) 버전 날짜 갱신**

`fw/include/define.h` `VERSION_MSG` 3종의 `_260905` 를 플래시 날짜(`_YYMMDD`)로 바꾼다 — 번호는 동결(3.0.0 / 3.1.0), 날짜만 진행(CLAUDE.md 태깅 규칙). 20자 유지 확인:

Run: `python3 -c "import re;[print(len(m.group(1)),repr(m.group(1))) for l in open('fw/include/define.h') for m in [re.search(r'#define VERSION_MSG \"(.*)\"',l)] if m]"`
Expected: `20` 세 줄.

이후 커밋 `chore(version): 빌드 날짜 _YYMMDD (hold-to-run 벤치 빌드)`. **H-0 은 이 플래시보다 앞**이다 — 현 보드(`405f95e`)에 START=2·3 → `US=0` 을 먼저 찍는다.

---

## 벤치 진입 (이 계획의 마지막 게이트 — 별도 보드 세션)

spec §8 표를 그대로 따른다. 순서만 강제:
1. **H-0** 현 보드에서(플래시 전) — `python3 mb_hold.py 192.168.1.199 150 2` 5 s → `US=0` 유지, mon 에 `[reg] cmd` 없음.
2. Task 6 Step 4 날짜 갱신 → STD 플래시 → **H-1** → REMOTE 플래시 → H-1 재확인.
3. H-2 → H-3 → H-4 → H-5 → H-8 → H-9 → H-10 → H-7 → H-6 → H-13 → H-12 → H-14 → H-16.
4. PASS 시: `--no-ff` 아님(main 직접 커밋 스택) → 태그 `hw-revA_fw-stage-hold-wdt` → `app_modbus_core.h` 값을 **계약 확정**으로 `gds_us_remote` 에 통보, `gds_us_hmi` 에 통보(51칸·`0x32`·START 값).
5. **PC8 재시험(A-1/A-5/A-13)과 같은 보드 세션에 섞지 않는다**(스펙 §0 D).

---

## Self-Review (작성 후 점검 결과)

- **Spec 커버리지**: §2.1 레지스터 → T1·T4 / §2.3 판정 절차 → T1 주석 / §3 #1-#15 → T1(#4-7) · T2(#1-3) · T3(#14) · T4(#8-13) · T5(#16) · T6(#17) / §4 경계 3겹 → T4 Step 4·5(주석 포함) / §5 T=600 → T2 헤더 / §6 fail-safe → T1 주석 + 벤치 H-0 / §7 11케이스 → T2 그대로 / §8 → 벤치 진입 절 / §9-A·B·C → T5 changelog 결정 기록. 빠진 항목 없음.
- **Placeholder**: "TBD/TODO/적절히/handle edge cases" 0건. 모든 코드 스텝에 코드 블록 있음.
- **타입 일관성**: `hold_wdt_t{armed,last_keep_ms}` · `hold_wdt_init/arm/keep/step/armed` 시그니처가 T2 헤더·T2 테스트·T4 호출에서 동일. `app_reg_run_src(void)→uint8_t` T3 정의 = T4 사용. `MB_START_TAP/HOLD/KEEP`·`MB_REG_FEAT_CAP`·`MB_FEAT_HOLD_WDT` T1 정의 = T2 wire 테스트 = T4 사용. `sys_tick_get_ms()` 는 `app_modbus.c` 가 이미 include(`sys_tick.h:30`).
