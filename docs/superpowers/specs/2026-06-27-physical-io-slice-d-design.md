# 물리 IO 슬라이스 D 설계 — 물리 명령 입력 + E-stop (2026-06-27)

> **요약**: 부록 D 물리 IO 5-슬라이스 중 **D = 물리 명령 입력 + E-stop**. 패널 물리 버튼(B_START/PA15, B_RESET/PC10, PC11=EMSW 또는 SEEK)을 `US_REMOTE` 통일 strict 소스로 `app_reg_command`에 디스패치하고, `model_type`으로 PC11 이중역할(std=EMSW / hand·multi=SEEK)을 분기한다. **E-stop은 레벨-추종**(레거시 `main.c:1409-1425` 충실, 사용자 확정) — EMSW 누름 동안만 force-stop+SOL OFF+START 차단, 떼면 자동 READY 복귀(RESET 불필요, 런 자동 재시작 안 함). 신규 io 함수 0개(슬라이스 A 헬퍼 소비). 아키텍처 = 순수 `app_input_fsm`(host-test) + 글루 `app_input`(10ms). `app_reg` 편집 2곳(START guard에 `app_estop_active()` break 추가, on-time ceiling에 `US_REMOTE` 추가). 단일 spec, 슬라이스 단위 빌드+host-test, HW 검증은 A/B/C와 묶어 실동작 rig 세션.

---

## 1. 목표 / 범위

### 1.1 In scope
- 물리 명령 버튼 → 명령 디스패치 (전부 `US_REMOTE` 통일 strict):
  - **B_START(PA15, active-LOW)** → `app_reg_command(US_CMD_START/RUN_RELEASE, US_REMOTE)` (모멘터리 hold-to-run)
  - **B_RESET(PC10, active-LOW)** → `app_reg_command(US_CMD_RESET, US_REMOTE)` (seek-reset 자동 체인)
  - **PC11** → `cfg->model_type` 분기: hand/multi → **B_SEEK**(active-LOW) → `US_CMD_SEEK`; std → **EMSW**(active-HIGH) → E-stop
- **E-stop (레벨-추종)**: force-stop(전 소스) + `io_sol_dn(off)` + START 차단 + Modbus STATUS `ESTOP`(0x02); EMSW release 시 자동 클리어
- **on-time ceiling에 `US_REMOTE` 추가** (`app_reg.c:270` 주석이 예고한 "REMOTE slice" 작업)

### 1.2 Out of scope (이연)
- HORN 모드(`sys_status==SYS_HORN`) / 풀 sys_status 상태기 UX
- RESET 시 model_type 페이지 전환 (legacy `main.c` reset 핸들러의 페이지 전환 부분)
- "SENSOR ON/OFF" LCD 텍스트 (`main.c:1234-1265`, MODE_TRIGGER 의존)
- 양손 SW_START1(PC12)+SW_START2(PB11) weld 트리거 + 센서 SENSE_UP/DN(PA12/PA11) → **슬라이스 E**
- OSC 발진 3채널 / 진폭 pot 실구동 → B-SEAM / 6b

### 1.3 출처
- legacy 거동: `ref/samd20/main.c` — 입력 read 1187-1203 / B_START 1356-1390 / B_RESET 1279+ / B_SEEK 1331-1353 / PC11 이중역할 1189-1198 / **E-stop 1409-1425** (⚠ 1270-1277 블록은 주석처리됨 = 비활성, 실 로직은 1409)
- 기존 STM32: `app_reg.c` START guard 119/124/127 + on-time ceiling 268-296 / `app_modbus_core.h:49` `MB_STATUS_ESTOP=0x02` / `io.h:11-13,22` (슬라이스 A 헬퍼)
- 핀/극성: `docs/pinmap.md` 부록 D + 움브렐라 spec `2026-06-20-physical-io-layer-design.md` §2

---

## 2. 활성 레벨 (움브렐라 §2 부분집합 — 슬라이스 D 핀)

| STM32 | Net | 방향 | 활성 | legacy 근거 | io 헬퍼 (슬라이스 A) |
|---|---|---|---|---|---|
| PA15 | CON_START | IN | LOW(눌림=0) | `B_START` re_start==0 | `io_read_start()` |
| PC10 | CON_RESET | IN | LOW | `B_RESET` re_reset==0 | `io_read_reset()` |
| PC11 | CON_ESTOP / SEEK | IN | **이중역할** | std=EMSW active-HIGH(re_emsw==1→estop) / hand·multi=`B_SEEK` active-LOW | `io_read_estop_seek()` |
| PB5 | CON_SOL_DN | OUT | LOW | E-stop 시 `SOL_DN=SOL_OFF` (`main.c:1415`) | `io_sol_dn(bool)` |

> **PC11 이중역할** (`main.c:1189-1198`): `sys_mode==SYS_MULTI||SYS_HAND`이면 `re_emsw=0`(EMSW 비활성) + `re_seek` 읽음; 그 외 `re_emsw=port_pin_get_input_level(SW_EMSW)`. STM32는 `cfg->model_type`(0=hand/1=multi → SEEK / 2=std → EMSW)로 **매 tick** 재현.

io 함수 신규 추가 **없음** — 슬라이스 A가 전 입력 read + `io_sol_dn`을 이미 제공(`io.h`). 입력 read는 raw 물리 레벨(0/1), 극성 해석은 `app_input` 호출측.

---

## 3. 아키텍처 / 모듈

```
src/app_input_fsm.{c,h}  ← 순수(HAL-free, host-test): edge-detect(START/RESET/SEEK,
                            _bak 패턴) + E-stop 레벨추종. model_type 입력으로 PC11 분기.
                            출력 = 명령 이벤트(START/RELEASE/RESET/SEEK) + estop_active 플래그.
src/app_input.{c,h}      ← 글루(10ms tick): io_read_* 읽어 FSM 구동 → app_reg_command(US_REMOTE)
                            디스패치 + E-stop force-stop/io_sol_dn(off) + app_estop_active() 노출.
app_reg.c (편집 2곳)     ← (a) START guard에 app_estop_active() break 추가(level)
                            (b) on-time ceiling 조건에 US_REMOTE 추가
main.c / app.c (배선)    ← app_input_init(io_init 뒤) / app_input_tick(10ms, app_overload_tick 인근).
```

**원칙**: HW 접근은 슬라이스 A `drivers/io`로 격리(슬라이스 D는 신규 HW 없음). 순수 로직(edge/debounce/E-stop 레벨추종/model_type 분기)은 `app_input_fsm`로 분리해 host-test. 접근법 1(단일 모듈) 선택 근거 = PC11이 단일 물리핀으로 model_type에 따라 SEEK/EMSW로 갈리므로, 그 read+분기를 한 모듈에 두는 응집도가 E-stop을 별도 모듈로 떼는 것보다 높음(E-stop은 디바운스 없는 즉시 레벨이라 로직이 작음).

### 3.1 공개 API (`app_input.h`)
```c
void    app_input_init(void);     /* boot: FSM reset (io_init 뒤) */
void    app_input_tick(void);     /* 슈퍼루프 10ms gate */
uint8_t app_estop_active(void);   /* 1=E-stop 활성 (START 차단 / STATUS 비트) */
```

### 3.2 순수 FSM (`app_input_fsm`)
- 입력 구조체: raw `start/reset/estop_seek` 레벨(0/1) + `model_type`.
- 내부 상태: `start_bak/reset_bak/seek_bak/emsw_bak`(edge 검출), `estop_active`.
- 출력 구조체: 명령 이벤트 비트(start_press/start_release/reset_press/seek_press) + `estop_active`(레벨) + `estop_enter`(진입 엣지 — `io_sol_dn(off)` 1-shot 트리거용; **force-stop은 `estop_active` 레벨이 매 tick 구동**).
- model_type 분기: hand/multi → estop_seek 핀을 SEEK(active-LOW)로 해석(EMSW 무시, `estop_active=0`); std → EMSW(active-HIGH)로 해석(SEEK 무시).

---

## 4. 명령 흐름 (US_REMOTE 통일 strict)

물리 버튼 → `app_reg_command(cmd, US_REMOTE)`. 기존 `app_reg_command`의 START 가드 `==US_IDLE`가 REMOTE에도 적용되고(움브렐라 §3.1), RUN_RELEASE는 기존 source-matched 로직이 US_REMOTE 처리. `US_REMOTE`는 이미 enum에 존재(슬라이스 2b).

| 버튼 | 엣지 | 디스패치 | 비고 |
|---|---|---|---|
| B_START(PA15) | 누름(LOW) | `app_reg_command(US_CMD_START, US_REMOTE)` | 모멘터리 hold-to-run, `main.c:1356-1362` |
| B_START(PA15) | 뗌(HIGH) | `app_reg_command(US_CMD_RUN_RELEASE, US_REMOTE)` | `main.c:1383-1387` (us_run_status==REMOTE→IDLE) |
| B_RESET(PC10) | 누름(LOW) | `app_reg_command(US_CMD_RESET, US_REMOTE)` | seek-reset 자동 체인 위임 |
| PC11 SEEK (hand/multi) | 누름(LOW) | `app_reg_command(US_CMD_SEEK, US_REMOTE)` | `main.c:1331-1353`; SEEK 단발 |

> B_START의 레거시 부가효과(`main.c:1366-1376` energy/multi 리셋 + I2C_POT 진폭 write)는 기존 `app_reg_command(START)` 핸들러(슬라이스 d2b/weld)가 이미 담당(진폭 pot 실구동은 B-SEAM/6b stub). 슬라이스 D는 명령 디스패치만.

---

## 5. E-stop 거동 (레벨-추종 — 사용자 확정 2026-06-27)

레거시 `main.c:1409-1425`가 권위 (1270-1277은 주석처리=비활성):
```c
if (re_emsw != re_emsw_bak) {
    if (re_emsw) {                 /* EMSW HIGH → 진입 */
        sys_status = SYS_ESTOP;
        SOL_DN = SOL_OFF;          /* 실린더 내림 */
        M_START = CTRL_INT_OFF;    /* 초음파 정지 */
    } else {                       /* EMSW LOW → 자동 복귀 */
        sys_status = SYS_RUN; run_status = RUN_READY;   /* RESET 없이 클리어 */
    }
}
```

STM32 재현 (std 모드 한정 — hand/multi는 PC11=SEEK):
- **진입 엣지** (EMSW LOW→HIGH): `estop_active=1` + `io_sol_dn(off)`(`estop_enter` 1-shot — overload 릴레이 선례와 동일; `io_sol_dn`은 idempotent).
- **active 동안 (레벨, 진입 tick 포함)**: force-stop을 **매 tick 재시도**(전 소스 source-matched `RUN_RELEASE`; `app_overload` force-stop 패턴 — 1-iter stale 레이스 일관 처리, idempotent: `us_run_status==src`일 때만 정지) + START 차단(§6 guard) + Modbus STATUS `ESTOP`(0x02).
- **해제 엣지** (EMSW HIGH→LOW): `estop_active=0` → 자동 ready 복귀. **RESET 불필요, 런 자동 재시작 안 함**(새 START 필요). 레거시 `run_status=RUN_READY` 충실.
- **hand/multi 모드**: PC11=SEEK이므로 EMSW 경로 비활성, `estop_active=0` 강제(레거시 `re_emsw=0` 충실).

> **force-stop 범위**: 초음파 게이트(US_REMOTE/TOUCH/COMM/CYCLE)만 정지 + `io_sol_dn(off)`. weld 기계 사이클(CYL) abort 배선은 슬라이스 E(weld 물리트리거 도입 시); 현재 weld dormant라 무관 — forward note.

---

## 6. START guard 합성 (`app_reg.c`)

`US_CMD_START` 케이스에서 swallow_start consume **뒤**, 기존 `seek_reset`/`overload` break와 **나란히** estop break 추가(전부 레벨 기반 별도 break — swallow 대칭 보존, advisor 선례):
```c
/* swallow_start consume → */
if (app_seek_reset_active() != 0u) break;
if (app_overload_active()  != 0u) break;
if (app_estop_active()     != 0u) break;   /* ← 슬라이스 D 추가 */
g_reg.us_run_status = src;                  /* run-start */
```
순환 의존 주의: `app_reg.c`가 `app_input.h`(`app_estop_active`)를 include — 기존 `app_overload.h`/`app_seek_reset.h` include 패턴과 동일.

---

## 7. on-time ceiling — 이중 구조 재설계 (2026-06-27 보드세션, 사용자 확정)

> **개정**: 초기 설계는 단일 ceiling에 `US_REMOTE`만 추가(`US_TOUCH||US_COMM||US_REMOTE`, 모드 무관)였으나, 보드세션에서 legacy 대조 결과 **legacy `main.c:5296`은 limit_on_time을 `(US_COMM||US_REMOTE) && sys_mode==SYS_HAND`에만 적용**(hand 모드 전용, TOUCH 제외)함이 확인됨. 사용자 결정 = **두 개의 독립 ceiling으로 분리**.

**(1) 절대 on-time 안전 ceiling — `ON_TIME_SAFETY_MS=30000`(30초)**
- 적용: **모든 초음파 런(US_TOUCH/US_COMM/US_REMOTE/US_CYCLE), 모든 모드, 무조건**.
- `limit_on_time`과 **완전 독립**(limit_on_time==0이어도 발화), 패널 편집 불가.
- 목적: 트랜스듀서 폭주 백스톱(V30 RUN data=0 release 엣지 분실, 멈춘 원격명령 등). `run_start_ms`는 모든 START 엣지(US_CYCLE 포함, app_weld 경유)에서 스탬프되므로 30초 기준 유효.
- 정지 = `reg_run_stop_latch(rs)`(last_* 래치 + IDLE; TOUCH면 swallow_start).

**(2) 운용 on-time ceiling — `limit_on_time`(×10ms, 0=off, 패널 편집)**
- 적용: **`(US_COMM||US_REMOTE) && model_type==HAND(0)` 에만** (legacy `main.c:5296` 충실). **US_TOUCH 제외**(legacy 미적용; V30 quirk는 위 30초 안전망이 커버), **multi/std 제외**(weld/energy/multi 한계가 지배).
- `limit_on_time`/`model_type` 모두 `app_reg_tick(limit_on_time, model_type)`로 주입(M1 순환의존 회피).

> **보드 검증(2026-06-27)**: 임시 `ON_TIME_SAFETY_MS=3000`(3초)로 빌드→플래시→multi 모드 Modbus START → SWD 샘플링 = `US_COMM` 런이 **~2.7s(129샘플) 지속 후 정지**(560ms 아님) → ① multi 모드 limit_on_time 미적용 ② 30초(임시 3초) 안전 발화 동시 입증. 이후 30000으로 굳혀 재플래시.

**cpp-review(opus) MERGE-READY 0 Crit/High, Minor 3(문서만)**:
- **≤2ms 물리 비활성 지연**: ceiling은 `us_run_status=IDLE`만 세트하고 실제 트랜스듀서 비활성은 다음 `reg_publish_measure`(~2ms 게이트)의 `app_reg_hook_us_output(false)` + setpoint MUX=0에서 발생 → 안전정지~물리off ≤2ms(손상 임계 훨씬 미만).
- **게이트 기준**: 운용 ceiling은 persisted `cfg->model_type`로 게이트(legacy는 runtime `sys_mode`). 본 코드베이스선 `sys_mode(=model_type)`로 일치(LCD 편집이 `cfg->model_type` 직접 기록·FRAM 영속) → 무해. RAM-only 모드전환 미영속 시에만 괴리(30초 안전은 모드 무관이라 무영향).
- **TOUCH-in-hand**: 운용 ceiling 제외(legacy 미적용 충실)이나 30초 안전은 추가 → **legacy보다 엄격(안전)**, 회귀 아님.
- ⚠ tick/ceiling는 host 커버리지 밖 → rig 잔여검증: hand COMM/REMOTE가 `limit_on_time×10ms`에 cut / US_CYCLE 30초 도달 / TOUCH-in-hand 30초 안전.

---

## 8. 핵심 결정 (확정)

| 항목 | 결정 |
|---|---|
| E-stop 해제 | **레벨-추종** — EMSW 누름 동안만 active, 떼면 자동 READY(RESET 불필요, 런 재시작 안 함). 레거시 `main.c:1409-1425` 충실 (사용자 2026-06-27). 메모리의 "latch" 가정은 1차 소스로 폐기. |
| 모듈 구조 | **접근법 1 단일 `app_input`** — PC11 dual-role 응집도 (사용자 확정) |
| REMOTE 중재 | 통일 strict (`==US_IDLE` 시작, source-matched 정지) — 움브렐라 §5 |
| B_START | 모멘터리 hold-to-run (`main.c:1356-1390`) |
| PC11 분기 | `cfg->model_type` 매 tick (0/1=SEEK, 2=EMSW) |
| force-stop | overload와 동일 active 레벨 재시도(idempotent, source-matched) |
| ceiling | **이중 구조(§7 개정)**: (1) 절대 30초 안전(전 모드·전 소스·limit_on_time 독립) + (2) 운용 limit_on_time(COMM/REMOTE·hand 전용, legacy 충실). swallow는 TOUCH 전용 유지 |
| STATUS ESTOP | `MB_STATUS_ESTOP=0x02` (이미 정의됨) OR 반영 |

---

## 9. 검증 전략

- **슬라이스 단위 (host 게이트)**: 빌드 0-warning + host-test `app_input_fsm` 스위트 + cpp-reviewer.
  - host-test 시나리오: B_START 누름→START 이벤트 / 뗌→RELEASE / B_RESET→RESET / PC11 SEEK(hand·multi)→SEEK 이벤트 + EMSW 무시 / PC11 EMSW(std)→estop_active+enter 엣지 + SEEK 무시 / E-stop active 중 START 이벤트 게이팅 / 해제 시 estop_active=0 / model_type 전환 분기.
- **HW 묶음 (실동작 rig 세션, A/B/C/D 함께 — 보드 게이트)**:
  - 각 버튼 → 명령 효과(Modbus STATUS / LCD ICON): B_START→런(STATUS bit0)+ICON_RUN, 뗌→정지; B_RESET→ICON_RESET→ICON_SEEK 체인; PC11(hand/multi)→SEEK ICON.
  - E-stop(std): EMSW 인가→즉시 정지+`io_sol_dn` OFF(멀티미터)+START 차단+STATUS ESTOP(0x02); 해제→자동 ready(RESET 없이), 런 미재시작.
  - 극성 sanity: PC11 std=EMSW active-HIGH / hand·multi=SEEK active-LOW; PA15/PC10 active-LOW.
  - 회귀: 직접-초음파(LCD/Modbus) ceiling·ICON_RUN 무회귀; **US_REMOTE도 ceiling 대상**이 됨 확인.

> ⚠ 글루(`app_input`)의 io→FSM→app_reg 배선·force-stop·SOL은 host 커버리지 밖 → cpp-review + HW가 실질 게이트(슬라이스 C/seek-reset 선례).

---

## 10. 이연 (slice D 범위 밖)

- HORN/full sys_status UX, RESET 페이지 전환, "SENSOR ON/OFF" 텍스트
- 양손 weld 트리거 + 센서(슬라이스 E) — E-stop의 weld FSM abort 배선도 거기서
- OSC 발진 / 진폭 pot 실구동 (B-SEAM/6b)
- E-stop force-stop forward note: weld 기계 사이클(CYL/SOL) abort는 슬라이스 E

---

## 11. 브랜치 / 머지 순서

- 슬라이스 D는 io(슬라이스 A) + START guard 합성(overload=슬라이스 C)에 의존 → **`feat/physical-io-slice-c` 위에 스택**(C가 A 위 스택이므로 D 브랜치 = A+C+D). START guard 합성이 머지 충돌 없이 정리됨.
- 머지 순서 = A→B→C→D (D는 C 뒤). **HW 게이트 상속**(실동작 rig 세션). 지금은 사용자 요청대로 spec/코드 선행, HW 검증 이연.
- 슬라이스마다 빌드 0-warning + host-test + cpp-reviewer 통과 후 미머지 유지.
