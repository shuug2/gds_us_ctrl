# 원격 제어 활성화 게이트 — T-1~T-4 실행 계획

> **문서 요약**: 승인된 설계 spec(`docs/superpowers/specs/2026-08-15-remote-enable-gate-design.md`)의 §10 중 자산 없이 진행 가능한 T-1~T-4를 실행 가능한 수준으로 분해한다. T-1 = 순수 FSM `app_remote_en_fsm.{c,h}` 신설 + 15번째 host 스위트(TDD 12케이스, RED 순서 명시), T-2 = `app_modbus_core.h` 레지스터 계약(0x2A~0x2D 매크로·상태 코드·매직, 디코더 무변경) + cross-check 케이스 13, T-3 = `app_modbus.c` 글루(매 tick FSM step + `mirror_live()` 3종 미러 + 접근자 3종), T-4 = `app_modbus_apply_writes()` 선두 게이트(명령 소거 불변식 + STOP 상시 통과 + `return`으로 cfg 체인 skip). 실행 순서는 T-1→T-2→T-3→T-4 직렬(각 1커밋), `app_reg.c`·`app_modbus_core.c`·`app_config.*`·`app_modbus_tcp.c`는 무변경이며, T-5(LCD)가 소비할 접근자 시그니처를 지금 확정해 재작업을 차단한다. 함정 절에 CMake GLOB·0-warning 게이트·u32 랩 안전 시간 산술·Makefile 기존 중복 룰을 명시한다.

**작성일**: 2026-08-15 / **브랜치**: `feat/remote-enable-gate` (base = `refactor/ponytail-cleanup` `77beb7a`, spec 커밋 `2ebba2a`) / **정본 spec**: `docs/superpowers/specs/2026-08-15-remote-enable-gate-design.md` (이하 "spec")

---

## 0. 공통 전제

### 0.1 검증 명령 (전 Task 공용)

```sh
./fw.sh          # 크로스 빌드 (glob 스탬프 자동 재구성)
./fw.sh test     # host 스위트 (= make -C fw/test)
```

- `fw.sh`는 두 함정을 대신 처리한다: stale `STM32_TOOLCHAIN` 우회, `file(GLOB)` configure-타임 고정 대응(`build/.src-glob` 스탬프). **T-1이 새 `.c`를 추가하므로 raw `cmake --build` 증분 빌드 금지** — 반드시 `fw.sh` 경유.
- "0-warning"의 정의: 우리 코드(`src/`, `include/`, `test/`)에서 `-Wall -Wextra -Wundef -Wshadow` 경고 0. 크로스(`fw/CMakeLists.txt:19`)와 host(`fw/test/Makefile:7`)가 같은 플래그 기조다.
- host "pass"의 정의: 15개 스위트 전부 `all tests passed` 출력 + make exit 0.

### 0.2 준수할 기존 컨벤션 (T-1 스타일 계약)

최신 순수 FSM 스위트 2개(`fw/test/test_app_horn_fsm.c`, `fw/test/test_app_seek_reset_fsm.c`)에서 확인한 관례 — **그대로 따른다**:

| 항목 | 관례 | 근거 |
|---|---|---|
| 단언 매크로 | `CHECK_EQ(expr, expected)` — `unsigned long` 캐스트, `FAIL %s:%d %s = %lu, expected %lu` 출력, `failures++` | `test_app_horn_fsm.c:8-17` |
| 테스트 함수 | `static void test_<행위>(void)` + 함수 위 한국어 행위 주석(spec 참조 포함) | `test_app_horn_fsm.c:19-29` |
| `main()` 형태 | 케이스 순차 호출 → `if (failures) { printf("<스위트명>: %d FAIL\n"...); return 1; }` → `printf("<스위트명>: all tests passed\n"); return 0;` | `test_app_horn_fsm.c:135-148` |
| 순수 모듈 입력 | 전역/HAL 접근 금지, `<name>_in_t` 구조체 포인터 주입 + 반환 또는 `_out_t` 출력 | `app_horn_fsm.h:9-18`, `test_app_seek_reset_fsm.c:32-36` |
| FSM 네이밍 | `<name>_fsm_init(void)` / `<name>_fsm_step(...)`, 상태는 `.c` 파일 static | `app_horn_fsm.c:13-25` |
| bak 엣지 규율 | 엣지 검출용 bak는 모드/게이트 상태 무관 **매 step 동기**(stale 엣지 방지) | `app_horn_fsm.c:8-10, 32-40` |
| Makefile 등록 | `BIN_X := /tmp/gds_test_<이름>` → `test:` 의존+실행 라인 → 빌드 룰 → `clean` 목록 | `fw/test/Makefile:22, 24, 38, 40-41, 95` |
| 시간 산술 | **u32 랩 안전 elapsed 비교만**: `(uint32_t)(now - base) >= threshold` | `app_modbus.c:56`, `app_lcd_input.c:301`, `sys_tick.c:22` |

### 0.3 Task 간 의존과 병렬성

```
T-1 (순수 FSM + 스위트)  ──┐
T-2 (core.h 레지스터 계약) ─┴→ T-3 (글루: step+미러+접근자) → T-4 (apply_writes 게이트)
```

- T-1과 T-2는 파일이 겹치지 않아 원리상 병렬 가능하나, T-2의 완료 조건에 "상태 코드 cross-check를 T-1 스위트에 추가"가 있으므로 **직렬 T-1→T-2를 권장**한다.
- T-3은 T-1(FSM 심볼)과 T-2(레지스터 매크로)를 모두 소비 → 선행 2개 완료 후.
- T-4는 T-3의 `s_ren` 캐시를 소비 → T-3 후. **T-3/T-4를 한 커밋으로 합치지 말 것** — T-3까지는 거동 무변화(미러 3종 추가뿐), T-4가 최초의 거동 변화(원격 명령 차단)라 회귀 시 이등분 지점이 된다.

### 0.4 무변경 파일 (게이트 원칙)

| 파일 | 근거 |
|---|---|
| `fw/src/app_reg.c` | `app_reg_start_allowed()`(`:132-155`)·`ON_TIME_SAFETY_MS`(`:44`) 그대로. 게이트를 여기 두면 같은 guard를 쓰는 US_TOUCH/US_REMOTE/US_CYCLE 로컬 조작까지 차단(spec §5.3) |
| `fw/src/app_modbus_core.c` | `mb_core_decode`/`mb_write_reg` 무변경(spec §4). FC06 항상 에코(`:70-94`) — 거부는 read-back 미러 복원으로만 관측 |
| `fw/src/app_config.c`, `fw/include/app_config.h` | 비영속 요구(spec §5.4) |
| `fw/src/app_modbus_tcp.c` | apply 게이트가 공유 seam에 있으므로 **TCP 쪽 제2 게이트 금지** |
| `fw/include/dgus_lcd.h`, `app_lcd_input.c`, `app_lcd_disp.c` | T-5(자산 게이트) 몫 |

---

## T-1 — `app_remote_en_fsm` 순수 FSM + host 스위트 신설

### 목표

게이트 상태 전이(0~5)·창 만료·침묵 무장 규칙·E-STOP 엣지·잔여 초 산술을 HAL/전역 무의존 순수 FSM으로 구현하고 host 스위트(15번째)로 고정한다.

### 변경 파일

| 파일 | 신규/수정 |
|---|---|
| `fw/include/app_remote_en_fsm.h` | **신규** |
| `fw/src/app_remote_en_fsm.c` | **신규** (⚠ GLOB 함정 — §함정 1) |
| `fw/test/test_app_remote_en_fsm.c` | **신규** |
| `fw/test/Makefile` | 수정 (스위트 등록 4곳 + 헤더 주석 스위트 목록 `:2-4`) |

### 구현 지시

**헤더** — 상수는 `#ifndef` 가드(bench-short 빌드 `-D` 오버라이드 허용, VR-5):

```c
#ifndef REMOTE_EN_WINDOW_S
#define REMOTE_EN_WINDOW_S        600u   /* 10분 */
#endif
#ifndef REMOTE_EN_LINK_SILENCE_S
#define REMOTE_EN_LINK_SILENCE_S   10u   /* 10초 */
#endif

enum { REN_DISABLED=0, REN_ENABLED=1, REN_DIS_TIMEOUT=2,
       REN_DIS_LINK=3, REN_DIS_ESTOP=4, REN_DIS_LCD=5 };

typedef struct {
    uint32_t now_ms;        /* sys_tick ms (글루 주입) */
    uint8_t  lcd_enable;    /* 1-shot 이벤트 (롱프레스 릴리스) */
    uint8_t  lcd_disable;   /* 1-shot 이벤트 (짧은 탭) */
    uint32_t last_req_ms;   /* 마지막 유효 Modbus 요청 시각 (s_remote_ms 주입) */
    uint8_t  req_valid;     /* 요청 1회 이상 존재 (s_remote_seen 주입) */
    uint8_t  estop;         /* E-STOP 레벨 (app_estop_active() 주입) */
} remote_en_in_t;

typedef struct {
    uint8_t  state;   /* REN_* 0~5. 해제 사유는 다음 enable까지 래치 */
    uint16_t left_s;  /* 잔여 활성 초 (ceil). ENABLED ⇔ left_s >= 1 */
} remote_en_out_t;

void remote_en_fsm_init(void);
void remote_en_fsm_step(const remote_en_in_t *in, remote_en_out_t *out);
```

**구현** — 내부 static 4개(`s_state`, `s_enable_ms`, `s_silence_armed`, `s_estop_bak`). step 알고리즘(순서가 규약):

1. **E-STOP 엣지 검출** — `edge = (s_estop_bak == 0u) && (in->estop != 0u)`. bak는 상태 무관 매 step 동기(`app_horn_fsm.c:32-40` 규율).
2. **ENABLED이면 해제 조건 평가** — 우선순위 ① `lcd_disable` → `REN_DIS_LCD` ② E-STOP 상승 엣지 → `REN_DIS_ESTOP` ③ 창 만료 `(uint32_t)(now - s_enable_ms) >= REMOTE_EN_WINDOW_S*1000u` → `REN_DIS_TIMEOUT` ④ 침묵: 미무장이면 무장 검사 `req_valid && ((uint32_t)(last_req_ms - s_enable_ms) <= (uint32_t)(now - s_enable_ms))` → `s_silence_armed=1` (**enable 이후 요청만 무장** — 랩 안전: pre-enable 요청은 좌변이 언더플로로 거대값); 무장 && `(uint32_t)(now - last_req_ms) >= REMOTE_EN_LINK_SILENCE_S*1000u` → `REN_DIS_LINK`.
3. **enable 이벤트를 마지막에 평가** (상태 무관): `lcd_enable && (estop == 0u)` → `s_state=REN_ENABLED; s_enable_ms=now; s_silence_armed=0`. 마지막 평가라서 (a) 같은 tick 해제와 경합 시 재활성이 이기고 (b) ENABLED 중 재조작 = 창 갱신 + 침묵 재-미무장 + DIS_* 래치 해제가 한 경로로 통합된다(spec §5.2).
4. **출력** — `left_s = ENABLED ? ceil((WINDOW_MS - elapsed)/1000) : 0` (elapsed는 3에서 갱신된 `s_enable_ms` 기준 재계산).

⚠ spec §5.2 의사코드의 `now >= expiry`는 **절대 시각 비교로 구현하면 안 된다** — 반드시 elapsed 형태(§함정 3). 구현 방식 지정이지 결정 변경이 아니다.

### TDD 순서 (RED 먼저)

| # | 테스트 함수 | 내용 | 구현 전 실패 양상 |
|---|---|---|---|
| 1 | `test_boot_disabled` | init 후 idle step → state 0, left 0 | 컴파일 실패(모듈 부재) |
| 2 | `test_enable_enters_enabled` | `lcd_enable=1` → state 1, left 600 | 스텁이 state 0 유지 |
| 3 | `test_enable_refused_while_estop_level` | `estop=1`+`lcd_enable=1` → state 0 유지 | 무조건 enable 구현 검출 |
| 4 | `test_window_expiry_latched` | t0+599999 state 1 → t0+600000 state 2·left 0 → 이후 2 유지 | 만료 로직 부재 |
| 5 | `test_left_arithmetic_boundary_wrap` | ceil 경계 + **u32 랩**: enable @`0xFFFFFB00` → wrap 넘어 state 1, `+600000`에서 2 | **절대 시각 비교 구현을 강제 검출** |
| 6 | `test_silence_unarmed_stale_req` | pre-enable 요청 상태로 enable → 11초 경과에도 state 1 | 무장 규칙 없는 순진 구현 검출 |
| 7 | `test_silence_armed_link_loss` | 무장 후 +9999 state 1 → +10000 state 3 | 침묵 로직 부재 |
| 8 | `test_silence_brief_gap_survives` | 9초 간격 요청 반복 → 내내 state 1 | 기준 시각 오용 검출 |
| 9 | `test_activity_does_not_extend_window` | 1초 간격 요청 지속 → enable+600000에서 state 2 | 활동 시 창 갱신 오구현 검출 |
| 10 | `test_estop_edge_no_auto_revive` | 0→1 즉시 4; 레벨 유지 4; **레벨 해제해도 4 유지** | 레벨 추종 오구현 검출 |
| 11 | `test_lcd_disable` | ENABLED 중 `lcd_disable=1` → 5, left 0 | 분기 부재 |
| 12 | `test_reenable_refresh_rearm_latch` | state 3에서 enable → 1·left 600; **재활성 후 요청 없이 11초 → 1 유지** | `s_silence_armed` 리셋 누락 검출 |
| 13 | `test_state_codes_match_core` | **T-2에서 추가** | — |

시간 주입은 `in.now_ms` 명시 증가/점프(FSM은 tick 카운트가 아닌 ms 비교).

### 완료 조건

- [ ] Makefile 4곳 등록 (`Makefile:40-41` horn 룰과 동형)
- [ ] 12케이스 GREEN, 각 케이스가 구현 전 RED였음 확인
- [ ] FSM `.c`에 HAL/전역 include 없음 (자기 헤더 단독)
- [ ] 시간 비교 전부 elapsed 형태, 절대 시각 비교 0건
- [ ] host + 크로스 0-warning

---

## T-2 — `app_modbus_core.h` 레지스터 계약

### 목표

`0x2A~0x2C` 매크로 + `0x2D` 예약 + CAP 매직 + 상태 코드 6종을 wire 계약으로 정의(디코더 무변경).

### 구현 지시

`app_modbus_core.h:44`(`MB_REG_STATUS`) 뒤, STATUS bits 블록(`:46`) 앞에 삽입:

```c
#define MB_REG_REMOTE_CAP       0x2Au  /* R: capability probe — 미러가 매 tick 매직 복원 */
#define MB_REG_REMOTE_EN        0x2Bu  /* R: 게이트 상태 0~5 (사유는 다음 enable까지 래치) */
#define MB_REG_REMOTE_EN_LEFT   0x2Cu  /* R: 잔여 활성 초, 비활성 0 */
/* 0x2D reserved — 2단계 승인 승격 경로 (spec §2.2). 미러하지 않음. */
#define MB_REG_REMOTE_CAP_MAGIC 0x5201u   /* probe 매직. write-back 값 P는 0 금지 (spec §5.1) */
#define MB_REMOTE_EN_DISABLED     0u
#define MB_REMOTE_EN_ENABLED      1u
#define MB_REMOTE_EN_DIS_TIMEOUT  2u
#define MB_REMOTE_EN_DIS_LINK     3u
#define MB_REMOTE_EN_DIS_ESTOP    4u
#define MB_REMOTE_EN_DIS_LCD      5u
```

- `MB_REG_COUNT = 50u` **변경 금지**(0x2D=45 < 50). `mb_write_reg`(`app_modbus_core.c:70-94`) **무변경** — `addr < MB_REG_COUNT`면 저장·에코하는 기존 계약이 곧 probe의 write 반쪽이다.
- 상태 코드를 두 헤더에 중복 정의하는 이유: 순수 FSM 헤더는 modbus core에 결합되면 안 되고, core.h는 wire 계약 문서다. 중복은 케이스 13으로 고정.

**케이스 13** — `test_app_remote_en_fsm.c`에 `#include "app_modbus_core.h"` 추가 후 `REN_*` ↔ `MB_REMOTE_EN_*` 6쌍 `CHECK_EQ`. (TDD: 매크로 정의 전 컴파일 실패가 RED.)

### 완료 조건

- [ ] 매크로 3종 + 매직 + 상태 코드 6종 + 0x2D 예약 주석
- [ ] `MB_REG_COUNT`·디코더·`app_modbus_core.c` diff 0
- [ ] 케이스 13 GREEN / 크로스·host 0-warning

---

## T-3 — `app_modbus.c` 글루: tick step + 미러 3종 + 접근자 3종

### 목표

FSM을 매 tick 구동해 출력을 파일 static에 캐시하고, `mirror_live()`가 3종을 미러하며, LCD(T-5)용 접근자 3종을 노출한다. **이 Task까지 게이트 자체는 물리지 않는다**(거동 변화 = 0x2A~0x2C가 읽히기 시작하는 것뿐).

### 구현 지시

**① include** — `app_modbus.c:13` 뒤에 `#include "app_remote_en_fsm.h"`.

**② 파일 static** — `s_remote_seen`(`:41`) 뒤: `s_ren`(출력 캐시), `s_ren_lcd_enable`, `s_ren_lcd_disable`(1-shot 래치).

**③ step 헬퍼 + 접근자** — `app_modbus_remote_active()`(`:52-57`) 뒤:

```c
static void remote_en_step(void)   /* 게이트 FSM 1 tick */
{
    remote_en_in_t in;
    in.now_ms      = sys_tick_get_ms();
    in.lcd_enable  = s_ren_lcd_enable;
    in.lcd_disable = s_ren_lcd_disable;
    in.last_req_ms = s_remote_ms;      /* pre-enable 값일 수 있음 — FSM 무장 규칙이 처리 */
    in.req_valid   = s_remote_seen;
    in.estop       = app_estop_active();
    remote_en_fsm_step(&in, &s_ren);
    s_ren_lcd_enable = 0u; s_ren_lcd_disable = 0u;   /* 1-shot 소비 */
}

void     app_remote_en_set(bool on);      /* on ? enable 래치 : disable 래치 */
uint8_t  app_remote_en_state(void)  { return s_ren.state; }
uint16_t app_remote_en_left_s(void) { return s_ren.left_s; }
```

- 침묵 입력은 기존 `s_remote_ms`/`s_remote_seen`(`:40-41`, RTU 스탬프 `:357`·TCP `app_modbus_tcp.c:169`)을 그대로 주입 — **읽기 요청도 링크 생존 신호**(note_remote가 유효 디코드 전부에 찍힘). `MB_REMOTE_HOLD_MS`(`:39`)와 무관.
- E-STOP은 `app_estop_active()` 레벨 주입 — 엣지화는 FSM 내부 책임.

**④ init** — `app_modbus_init()`(`:320-325`)의 `apply_config()` 앞에 `remote_en_fsm_init()` + `s_ren`/래치 0 초기화.

**⑤ tick step 호출** — `app_modbus_tick()`(`:341`) **첫 문장**, `apply_config()`(`:343`) 앞. 근거: RTU 점유/TCP/미점유 어느 분기로 빠져도 시간이 흐르고 만료돼야 하며(spec §6), 같은 tick의 `mirror_live()`가 최신 상태를 쓴다.

**⑥ 미러 3종** — `mirror_live()` 말미, `holding[MB_REG_STATUS]`(`:120`) 뒤. CAP는 매직 무조건 복원(= probe, `MODEL_FREQ/TYPE :100-101` 동형), `0x2D`는 미러하지 않음. **조건 없이 함수 말미 고정**이면 3개 호출처(`:315`, `:364`, `:374`·`:380`) 전부 자동 커버.

**⑦ 헤더** — `app_modbus.h` 말미에 선언 3종.

### TDD/검증

글루는 host 커버 불가(HAL 결합, spec §11). 게이트 = ① 크로스 0-warning ② 15스위트 무회귀 ③ self-review(step 호출 위치·미러 무조건성·1-shot 소비).

### 완료 조건

- [ ] `remote_en_step()`이 `app_modbus_tick()` 첫 문장 (분기 밖)
- [ ] 미러 3종이 `mirror_live()` 말미 무조건 실행, `0x2D` 미러 없음
- [ ] 접근자 3종 시그니처가 T-5 예약과 일치
- [ ] `s_ren*` 외 신규 전역 없음, holding/FRAM에 상태 저장 없음
- [ ] 크로스 0-warning + 15스위트 무회귀 / `app_reg.c`·`app_modbus_core.c`·`app_config.*`·`app_modbus_tcp.c` diff 0

---

## T-4 — `app_modbus_apply_writes()` 선두 게이트

### 목표

게이트 비활성 시 명령 3종(RESET/SEEK/START)을 **디스패치 없이 소거**하고 STOP만 상시 통과, cfg 체인 전체 skip.

### 구현 지시

삽입 위치: 지역변수 선언(`:129-131`) **직후**, 첫 분기 `if (holding[MB_REG_RESET] == 1u)`(`:133`) **직전**:

```c
if (s_ren.state != (uint8_t)REN_ENABLED) {
    g_mb.holding[MB_REG_RESET] = 0u;
    g_mb.holding[MB_REG_SEEK]  = 0u;
    g_mb.holding[MB_REG_START] = 0u;
    if (g_mb.holding[MB_REG_STOP] == 1u) {
        app_reg_command(US_CMD_RUN_RELEASE, (uint8_t)US_COMM);
        g_mb.holding[MB_REG_STOP] = 0u;
    }
    return;
}
```

**배치·순서 근거 (임의 변경 금지):**

1. **else-if 체인 앞 + `return`** — 체인(`:133-254`)은 "1 메시지 = 1 변경"이며 명령 4종 뒤 cfg 분기 16종이 이어진다. cfg 쓰기 거부의 유일한 메커니즘이 이 `return`이다. 게이트를 체인 중간/개별 분기에 흩뿌리면 cfg 분기가 다시 열린다.
2. **STOP이 체인으로 fall-through 하면 안 되는 이유** — `return`이 cfg를 막는 바로 그 장치이므로, STOP을 기존 분기(`:155-157`)에 맡기려면 return을 포기해야 한다. 그러면 (a) cfg 거부가 사라지고 (b) 소거된 명령 뒤 cfg 분기가 같은 메시지에서 발화해 "게이트 닫힘 + cfg 반영" 모순. 따라서 STOP 소비를 게이트 블록 **안에 복제**(기존 분기와 동형).
3. **소거는 무조건, 값 불문** — `== 1u` 검사 없이 0 대입. 1 이외 값도 잔류물을 남기지 않는다.
4. **판정은 `s_ren.state` 직접 참조** — 같은 파일 static이라 접근자 불요. `!= REN_ENABLED`(0·2·3·4·5 전부 닫힘).
5. **양 전송로 단일 커버** — RTU(`:361`)·TCP(`app_modbus_tcp.c:172`)가 이 함수를 공유. TCP 쪽 추가 금지.
6. `app_modbus_note_remote()`는 디코드 시점(`:357`, tcp `:169`)에 이미 찍힘 — 게이트 블록에서 건드리지 않는다(읽기·에코 허용 = 링크 생존 신호 유지).

### TDD/검증

게이트 지점 자체는 host 불가 — 실검증은 T-7 VR-3(비활성 차단 + stale latch)·VR-4(활성 통과). 이 Task 게이트 = 크로스 0-warning + 15스위트 무회귀 + 위 6개 근거 diff 재확인.

### 완료 조건

- [ ] 게이트 블록이 `:133` 분기 앞 단일 위치, `return` 포함
- [ ] 명령 3종 무조건 소거 + STOP `== 1u` 소비가 기존 분기와 동형
- [ ] `app_reg.c`·`app_modbus_tcp.c` diff 0
- [ ] 크로스 0-warning + 15스위트 무회귀

---

## 커밋 분할 계획 (Task당 1커밋, 순서 고정)

| Task | 커밋 subject |
|---|---|
| T-1 | `feat(remote): app_remote_en_fsm 순수 FSM 신설 — host 스위트 15번째` |
| T-2 | `feat(remote): Modbus 레지스터 0x2A~0x2D 계약 — CAP 매직·상태 코드` |
| T-3 | `feat(remote): 게이트 글루 — 매 tick FSM step + 미러 3종 + 접근자 3종` |
| T-4 | `feat(remote): apply_writes 선두 게이트 — 명령 소거 불변식 + STOP 상시 통과` |

각 커밋 body에 spec §10 T-n 참조와 게이트 통과 사실(host green / 0-warning / 무회귀)을 기록. 커밋 전 검증 명령이 GREEN이어야 한다.

---

## T-5 인터페이스 예약 (T-3에서 확정, 재작업 금지 계약)

| 심볼 | 시그니처 | T-5 소비처 |
|---|---|---|
| `app_remote_en_set` | `void app_remote_en_set(bool on)` | `app_lcd_input.c` dispatch: 게이트 버튼 VP(제안 0x1086) 롱프레스 릴리스 → `set(true)`, 짧은 탭 → `set(false)`. 롱프레스는 기존 `long_press_released()`(`app_lcd_input.c:273-312`, `KEY_HOLD_MS :47`) 재사용(spec §8.2) |
| `app_remote_en_state` | `uint8_t app_remote_en_state(void)` — `MB_REMOTE_EN_*` 0~5 | `app_lcd_disp.c` 상태 아이콘(제안 0x1155) write-on-change |
| `app_remote_en_left_s` | `uint16_t app_remote_en_left_s(void)` — 비활성 0 | `app_lcd_disp.c` 잔여 초 VP(제안 0x1211) 1초 cadence (spec §9) |

의미 계약: `set(true)`는 E-STOP 레벨 활성 중 FSM이 거부하므로 **LCD 쪽에 별도 estop 검사를 넣지 않는다**(단일 책임). `state`의 해제 사유 래치(2~5)를 아이콘 구분에 쓸지는 T-5 몫 — 접근자가 이미 사유를 노출하므로 T-3 재작업은 발생하지 않는다. VP 정의(`dgus_lcd.h`)는 T-5에서, **트레이스 선행**(spec §8.3).

---

## 함정 (구현자 필독)

1. **CMake GLOB configure-타임 고정** — T-1의 `app_remote_en_fsm.c`가 신규다. `CMakeLists.txt:91`의 `file(GLOB)`은 configure 시점 고정이라 증분 빌드만 하면 undefined reference(2026-06-19 실전 피해). **항상 `./fw.sh` 경유**.
2. **0-warning 게이트** — `-Wall -Wextra -Wundef -Wshadow`. 특히 `-Wundef`: 상수 가드는 `#ifndef`(값 평가 `#if` 금지), `-Wshadow`: 파일 static과 지역변수 이름 충돌 금지, 미사용 파라미터는 `(void)` 캐스트.
3. **u32 랩 안전 시간 산술** — 이 코드베이스의 유일한 시간 비교 형태는 `(uint32_t)(now - base) >= threshold`(`app_modbus.c:56`, `app_lcd_input.c:301`, `sys_tick.c:22`). spec §5.2 의사코드 `now >= expiry`를 절대 시각으로 구현하면 49.7일 랩에서 깨진다 — **케이스 5를 건너뛰지 말 것**.
4. **`fw/test/Makefile`의 기존 중복 룰** — `BIN_OL`/`BIN_IN`/`BIN_OSC` 빌드 룰이 이중 정의돼 있다. 눈에 띄어도 **정리하지 말 것**(요청 범위 밖 — `~/dev/CLAUDE.md` "요청한 부분만"). 신규 스위트만 추가.
5. **명령 레지스터는 미러가 안 덮는다** — `mirror_live()`는 `0x19~0x1C`를 건드리지 않는다. T-4 소거를 빠뜨리면 stale START latch-through(spec §5.3, 리스크 §13.5). 미러 복원에 기대는 cfg 거부와 혼동 금지.
6. **상태 저장 위치** — holding[]은 `mb_core_init` 0-리셋(`:310`, `:373`)으로 소실, FRAM은 비영속 위반. 파일 static만. 리셋 직후 0-읽기 창은 같은 tick 미러가 닫는다 — 미러를 조건부로 만들지 말 것.
7. **probe 의미론을 "고치지" 말 것** — FC06이 0x2A에 써지고 성공 에코하는 것은 버그가 아니라 probe의 전제다(spec §5.1). 디코더에 쓰기 거부를 추가하면 계약 파괴.
8. **`MB_REMOTE_HOLD_MS`(1000)와 신규 침묵 상수는 별개** — 전자는 REMOTE 아이콘 1초 홀드, 후자는 10초 링크 침묵. 재사용·통합 금지.
9. **SWD halt 금지** — T-1~T-4는 보드 불필요. 벤치 검증은 T-7에서 mbpoll+LCD 육안으로만.
10. **테스트 이진은 `/tmp/gds_test_*`** — 기존 관례 그대로(신규 경로 발명 금지).
