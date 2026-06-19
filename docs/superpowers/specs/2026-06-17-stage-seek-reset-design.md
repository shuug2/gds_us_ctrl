# Stage SEEK/RESET 효과 — Design

> **요약**: SEEK/RESET 명령의 **효과**(현재 `app_reg_command` no-op)를 구현하는 스테이지. samd20 거동: RESET 발행 → 500ms 후 **SEEK으로 자동 체인** → 또 500ms 후 **자동 해제** → IDLE (TOUCH/COMM 소스). SEEK 직접 발행은 체인 없이 500ms 단발. **범위 = 상태머신 + 500ms 타이밍 체인 + 자동해제 + ICON 렌더** (HW 불요, host-test). **물리 OSC 신호 구동(reset_signal/seek_signal → CTRL_OSC* 핀)은 hook stub(로그) + B-SEAM/6b 이연** — samd20은 M_RESET(PB12)/M_SEEK(PB11) active-LOW를 별도 M16으로 보냈으나, STM32는 M16을 흡수했고 그 물리 출력(OSC 3선 미러)의 극성이 미확인(B-SEAM 영역)이기 때문. **아키텍처 = 순수 FSM 분리**(신규 `app_seek_reset_fsm`, HAL-free, host-test) + 글루 `app_seek_reset`(10ms tick, hook 라우팅) — weld 슬라이스 패턴 동형. **RUN과 직교**(samd20 충실): RUN 중 SEEK/RESET 무시 + SEEK/RESET 중 RUN(START) 무시. **타이밍 = 10ms FSM tick, 500ms=50 tick 액면가**(weld FSM 정합); samd20 100ms 카운터 quirk(`us_reset_cnt>5`의 ±100ms)는 미재현 — 명령 신호라 정밀도 비요구. **분석 §5 ICON 엣지 렌더 갭**(samd20은 `sig_reset/seek_status` 엣지에서 ICON 점등, STM32는 부팅 클리어만) 동시 해결. 충실도 = 혼합.

---

## 1. 목표 & 범위

### 1.1 컨텍스트
- samd20 SEEK/RESET은 초음파 혼(horn) 공진 주파수 **리셋(RESET)** + **탐색(SEEK)** 명령. 패널/Modbus/물리버튼으로 트리거, M16으로 active-LOW 명령선(M_RESET PB12 / M_SEEK PB11) 전송.
- **자동 체인**(`ref/samd20/main.c:5388-5408`): TOUCH/COMM 소스 RESET은 500ms 유지 후 `sig_reset_status=OFF` + `sig_seek_status=ON` 자동 발행 → SEEK 500ms 후 자동 해제. 타이머 = 100ms tick `us_reset_cnt`, `MAX_US_RESET_CNT=5`(main.c:118), `us_reset_mode`(RESET_IDLE/RESET_ISSUE/SEEK_ISSUE).
- STM32 현황: `US_CMD_SEEK=1`/`US_CMD_RESET=2`(`app_lcd.h:56-61`) **라우팅 완성**(LCD `app_lcd_input.c:100/112/150`, Modbus `app_modbus.c:101/104` consume-clear) 그러나 `app_reg_command`(`app_reg.c:139-146`) **완전 no-op**. `ICON_RESET=0x1150`/`ICON_SEEK=0x1151`(`dgus_lcd.h:70-71`) 정의됐으나 엣지 렌더 미포팅. `sig_seek_status`/`sig_reset_status`(`app_lcd.h:123`) 미사용.
- 충실도 입장: **혼합** — 거동은 samd20 충실, 타이밍 quirk·물리층 deviation은 §4/§6/§8/§10 문서화.

### 1.2 In scope
- **순수 FSM 코어 (`app_seek_reset_fsm`)**: `SR_IDLE`/`SR_RESET`/`SR_SEEK` 상태. RESET→SEEK 자동 체인, SEEK 단발, 500ms 타이밍(50×10ms tick), RUN 직교(`run_active` 입력). 출력 = `reset_signal`/`seek_signal` 레벨 + ICON on/off 엣지. host-test.
- **글루 (`app_seek_reset`)**: `app_seek_reset_request(cmd, src)` + `app_seek_reset_tick()`(10ms 게이트). `app_reg_command`의 SEEK/RESET no-op을 위임으로 교체. hook 라우팅.
- **양방향 RUN 직교**: ① RUN 중(`us_run_status != US_IDLE`) SEEK/RESET 명령 무시 ② SEEK/RESET active 중 START 무시(`app_reg_command` START guard에 `app_seek_reset_active()` 추가).
- **ICON 엣지 렌더**(분석 §5 갭): `reset_icon`/`seek_icon` 엣지 → LCD `ICON_RESET`/`ICON_SEEK` 점등/소등.
- **물리 OSC hook stub**: `reset_signal`/`seek_signal` 엣지 → `app_seek_reset_hook_signal()`(mon 로그) — 실제 CTRL_OSC* 구동은 B-SEAM 이연.
- **host 테스트**: 체인 시퀀스·SEEK 단발·RUN 직교·ICON 엣지·재진입(busy 중 재명령).

### 1.3 Out of scope (deferred)
| 항목 | 이연처 / 게이트 |
|---|---|
| **물리 OSC 신호 구동** (reset_signal/seek_signal → CTRL_OSC* 핀, 극성 active-LOW 확정) | B-SEAM / 6b (실 초음파 rig + 스코프) |
| **물리버튼 B_RESET/B_SEEK** 레벨-팔로우 경로(samd20 `max_us_reset_cnt=0xff`, 버튼 누르는 동안 유지, 체인 없음) | 슬라이스4 / HW (물리 입력 GPIO) |
| **과부하 복구 시퀀스**(OVLD→START 해제→RESET→자동 SEEK→idle) | overload 스테이지 (이 FSM 재사용) |
| **에러 표시**(ICON_ERROR_RESET, SYS_ERROR) | 에러 머신 스테이지 |

> SEEK/RESET 효과는 **상태머신·타이밍·LCD 피드백**을 host-test로 검증한다. 물리 효과(OSC 구동)는 hook stub이라 HW에선 명령 라우팅+ICON+상태 전이만 관측 — 무회귀 + ICON 육안이 HW 확인 대상(§9). 실제 주파수 탐색/리셋 물리 동작은 B-SEAM/6b rig.

---

## 2. 아키텍처 (순수 FSM 분리 — weld 패턴)

```
app_seek_reset_fsm.{c,h}   순수 FSM (host-test 대상)
  - 상태 SR_IDLE/SR_RESET/SR_SEEK + 내부 s_elapsed(10ms tick)
  - seek_reset_in_t  { cmd(NONE/RESET/SEEK), run_active }
  - seek_reset_out_t { reset_signal, seek_signal (레벨); reset_icon, seek_icon (on/off 엣지) }
  - 자동 체인/해제 타이밍, RUN 직교

app_seek_reset.{c,h}       글루 (HAL 결합)
  - app_seek_reset_request(us_cmd_t cmd, uint8_t src)  ← app_reg_command 위임
  - app_seek_reset_tick()        ← 슈퍼루프 10ms 게이트, FSM step
  - app_seek_reset_active()      ← START 직교용 (SR_IDLE 아니면 true)
  - hook: app_seek_reset_hook_signal(reset/seek, on)  ← OSC stub (B-SEAM)
          + LCD ICON 렌더 호출 (reset_icon/seek_icon 엣지)
```

> 누산·주입 불필요(weld 슬라이스2 energy와 달리). SEEK/RESET은 순수 시간 기반 + 명령 엣지 구동이라 FSM 내부 상태로 자족. `app_reg`는 START 직교용 `app_seek_reset_active()` 조회 + SEEK/RESET 위임 진입점만 결합.

---

## 3. FSM 상태 모델 (`app_seek_reset_fsm`)

### 3.1 인터페이스 (`app_seek_reset_fsm.h`)
```c
enum { SR_IDLE = 0, SR_RESET = 1, SR_SEEK = 2 };
enum { SR_CMD_NONE = 0, SR_CMD_RESET = 1, SR_CMD_SEEK = 2 };

typedef struct {
    uint8_t cmd;          /* SR_CMD_* — 이 tick의 명령 (없으면 NONE) */
    uint8_t run_active;   /* 1 = us_run_status != IDLE (RUN 중 → 명령 무시) */
} seek_reset_in_t;

typedef struct {
    uint8_t state;        /* 현재 SR_* */
    uint8_t reset_signal; /* RESET 명령선 레벨 (1=active) */
    uint8_t seek_signal;  /* SEEK 명령선 레벨 (1=active) */
    uint8_t reset_icon;   /* 1-shot: RESET ICON on 엣지 */
    uint8_t reset_icon_off;/* 1-shot: RESET ICON off 엣지 */
    uint8_t seek_icon;    /* 1-shot: SEEK ICON on 엣지 */
    uint8_t seek_icon_off;/* 1-shot: SEEK ICON off 엣지 */
} seek_reset_out_t;

void seek_reset_fsm_init(void);
void seek_reset_fsm_step(const seek_reset_in_t *in, seek_reset_out_t *out);  /* one 10ms tick */
uint8_t seek_reset_fsm_state(void);
```
- 내부(static): `static uint8_t s_state; static uint16_t s_elapsed;`
- ICON 엣지는 on/off를 별도 1-shot 필드로(글루가 LCD VP에 0x01/0x00 write). `out`은 매 step memset 0 → 엣지 자동.

### 3.2 타이밍 (액면가 10ms tick)
- `#define SR_TICKS  50u` (500ms / 10ms). samd20 `us_reset_cnt>5`(100ms tick)의 명목 500ms를 **10ms tick 액면가**로 재현.
- samd20 100ms 카운터 quirk(`>5`가 cnt==6=600ms일 수 있는 ±100ms)는 **미재현** — SEEK/RESET은 명령 신호라 정밀도 비요구(spec §8).
- **진입 tick에 `s_elapsed=0`**, 이후 tick마다 `s_elapsed++`, `s_elapsed >= SR_TICKS`에서 전이. signal active span = 50 tick = 500ms. (weld 슬라이스3의 CYL→WELD 전이 step 보정 이슈 없음 — SEEK/RESET은 IDLE에서 직접 진입.)

### 3.3 전이
```
SR_IDLE:
    if (in->run_active) → 무시 (RUN 직교)
    elif (cmd == RESET) → SR_RESET; reset_signal=1; reset_icon=1; s_elapsed=0
    elif (cmd == SEEK)  → SR_SEEK;  seek_signal=1;  seek_icon=1;  s_elapsed=0
    (cmd NONE → 유지)

SR_RESET:                                     # cmd 무시 (busy)
    s_elapsed++  (포화 가드)
    if (s_elapsed >= SR_TICKS):               # 500ms 경과 → SEEK 자동 체인
        reset_signal=0; reset_icon_off=1
        seek_signal=1;  seek_icon=1
        s_state=SR_SEEK; s_elapsed=0           # samd20 5395-5396
    else: reset_signal=1 (레벨 유지)

SR_SEEK:                                      # cmd 무시 (busy)
    s_elapsed++  (포화 가드)
    if (s_elapsed >= SR_TICKS):               # 500ms 경과 → 자동 해제
        seek_signal=0; seek_icon_off=1
        s_state=SR_IDLE; s_elapsed=0           # samd20 5403-5407
    else: seek_signal=1 (레벨 유지)
```
- **SEEK 직접**(IDLE→SR_SEEK)은 **체인 없음** — SR_SEEK 만료 시 IDLE로(samd20 충실: RESET만 SEEK으로 체인, SEEK_ISSUE는 단발).
- **busy 중 재명령 무시**: SR_RESET/SR_SEEK에서 cmd는 평가 안 함(이미 진행 중). host-test로 동결. (samd20: 진행 중 sig_*_status 재ON은 do_control 엣지 없음 → 무해.)
- **레벨 vs 엣지**: `reset_signal`/`seek_signal`은 active 동안 매 step 1로 emit(레벨; 글루가 stub에 전달). `*_icon`/`*_icon_off`는 전이 step 1-shot(글루가 LCD 렌더).

### 3.4 RUN 직교 (양방향)
- **RUN → SEEK/RESET 무시**: FSM `SR_IDLE`에서 `run_active`면 cmd 무시(§3.3). 글루가 `us_run_status != US_IDLE`를 `run_active`로 주입.
- **SEEK/RESET → START 무시**: `app_reg_command` START guard(현재 `main_state==0 && us_run_status==US_IDLE`)에 `&& !app_seek_reset_active()` 추가. SR_RESET/SR_SEEK 중 START 명령 거부.

---

## 4. 글루 (`app_seek_reset`) & app_reg 결합

| 위치 | 변경 |
|---|---|
| `app_seek_reset_request(cmd, src)` | `app_reg_command`의 `case US_CMD_SEEK/US_CMD_RESET`에서 호출(no-op 교체). `run_active`= `g_reg.us_run_status != US_IDLE` 판정해 FSM에 1회 cmd 큐잉(다음 tick 소비). |
| `app_seek_reset_tick()` | 슈퍼루프 10ms 게이트. `seek_reset_in_t{ cmd=대기명령, run_active }` → `seek_reset_fsm_step` → out 라우팅. `app_reg_tick` 인근 배선(weld_tick 패턴). |
| out 라우팅 | `reset_signal`/`seek_signal` 엣지(레벨 변화) → `app_seek_reset_hook_signal()`(mon stub) / `reset_icon`·`seek_icon`(+off) → LCD `ICON_RESET`/`ICON_SEEK` VP write(0x01/0x00). |
| `app_seek_reset_active()` | `seek_reset_fsm_state() != SR_IDLE`. `app_reg_command` START guard가 조회. |

> cmd 큐잉: `app_seek_reset_request`는 명령을 1-shot 래치(`s_pending_cmd`), `app_seek_reset_tick`이 다음 step에서 소비·클리어(weld `s_start_pending` 패턴 — 무시되면 드롭). run_active는 **tick 시점 평가**: `app_seek_reset_tick`이 `app_reg_measure()->us_run_status != US_IDLE`를 조회(weld의 `curr_energy` 주입과 동형 방향, 라이브·samd20 do_control 동형).

---

## 5. Hook 인터페이스

| hook | 호출 시점 | 본 스테이지 | 후속 |
|---|---|---|---|
| `app_seek_reset_hook_signal(uint8_t which, bool on)` | reset_signal/seek_signal 레벨 엣지 | mon 로그(`[sr] RESET/SEEK signal ON/OFF`) | B-SEAM: CTRL_OSC* active-LOW 구동 |
| (LCD ICON) | reset_icon/seek_icon (+off) 엣지 | `app_lcd` ICON_RESET/SEEK VP write | (완성 — 분석 §5 갭 해결) |

> ICON 렌더는 stub 아님(완성). 물리 신호만 stub(B-SEAM). `app_lcd`에 ICON write 헬퍼가 없으면 추가(ICON_RUN 렌더 패턴 참조).

---

## 6. 범위 경계 / deviation

- ✅ **In**: SEEK/RESET 상태머신 + 500ms 자동 체인/해제 + 양방향 RUN 직교 + ICON 엣지 렌더 + 물리 신호 hook stub.
- ⛔ **이연**: 물리 OSC 구동(B-SEAM/6b), 물리버튼 레벨-팔로우(슬라이스4), 과부하 복구 시퀀스(overload), 에러 표시(에러 머신).
- ⚠ **deviation (물리층)**: samd20은 M_RESET/M_SEEK를 외부 M16으로 보냈으나 STM32는 M16 흡수 → 물리 출력 대상이 CTRL_OSC*(극성 미확인). 본 스테이지는 신호를 **hook stub**으로만 emit, 실제 핀 구동은 B-SEAM. HW에선 ICON·상태 전이만 관측 가능.
- ⚠ **타이밍 deviation (§3.2)**: 액면가 10ms tick — samd20 100ms 카운터 ±100ms quirk 미재현.

---

## 7. 테스트 (host)

`test_app_seek_reset_fsm.c` (신규 host 스위트):
1. **RESET 자동 체인**: cmd=RESET → SR_RESET, reset_signal=1·reset_icon=1; 50 tick 후 SR_SEEK 전이(reset_signal=0·reset_icon_off=1·seek_signal=1·seek_icon=1); 또 50 tick 후 SR_IDLE(seek_signal=0·seek_icon_off=1). reset_signal span=50 tick, seek_signal span=50 tick.
2. **SEEK 단발(체인 없음)**: cmd=SEEK → SR_SEEK; 50 tick 후 SR_IDLE 직행(체인 없음, reset_signal 내내 0).
3. **RUN 직교**: `run_active=1`이면 cmd=RESET/SEEK 무시(SR_IDLE 유지, signal 0).
4. **busy 중 재명령 무시**: SR_RESET 중 cmd=SEEK/RESET 재주입 → 무시(타이밍·전이 불변).
5. **ICON 엣지 1-shot**: 각 전이에서 icon on/off가 정확히 1 tick만 1.
6. **타이밍 경계**: 정확히 SR_TICKS(50) tick에 전이(49 tick까진 유지).

> 글루(app_seek_reset)·app_reg START 직교 결선·LCD ICON write는 통합 cpp-reviewer + HW 육안(ICON)으로 검증.

---

## 8. 충실도 노트 & quirk

- **RESET만 체인, SEEK 단발**: samd20 `us_reset_mode` RESET_ISSUE만 SEEK으로 체인(main.c:5395), SEEK_ISSUE는 단발 해제(5403). 충실.
- **타이밍 액면가**: samd20 100ms·`us_reset_cnt>5` quirk 미재현(§3.2). 명령 신호라 ±100ms 무의미.
- **물리층 흡수**: M_RESET/M_SEEK(외부 M16 명령선) → STM32는 신호 hook stub. M16 흡수의 자연 귀결(분석 §5 "OSC 미러 포함 구현 시 ①~④ 재현").
- **busy 무시 = samd20 무해 동작 재현**: samd20은 진행 중 재명령이 do_control 엣지 없어 무해; FSM은 명시적 busy 무시로 동결.
- **양방향 직교**: samd20은 RUN 중 RESET/SEEK 필터(main.c:3636/3665). 역방향(SEEK/RESET 중 START)은 SEEK/RESET이 짧아 samd20에서 자연 회피되나, STM32는 명시적 guard(`app_seek_reset_active`)로 보장.

---

## 9. 빌드/검증 게이트

- **호스트(주 게이트)**: `env -u STM32_TOOLCHAIN cmake --build fw/build` 0-warning + `make -C fw/test test` 5스위트(기존 4 + 신규 `app_seek_reset_fsm`) PASS.
- **HW(보드, 회귀 + ICON)**: ① 기존 직접-초음파/weld 무회귀 ② **ICON_RESET/ICON_SEEK 육안**: 패널 RESET 버튼 → ICON_RESET 점등 →(500ms)→ ICON_SEEK 점등 →(500ms)→ 소등(자동 체인 시각 확인); SEEK 버튼 → ICON_SEEK 단발 ③ Modbus 0x19/0x1A로 동일 ④ RUN 중 SEEK/RESET 무시 + SEEK/RESET 중 START 무시.
- **물리 OSC 효과(주파수 탐색/리셋)는 B-SEAM/6b**(실 초음파 rig + 스코프). 본 스테이지 HW는 ICON·상태·직교만.
- 머지: `--no-ff` 로컬, 태그 `hw-revA_fw-stage-seekreset`(host + HW ICON/직교 verified; 물리 OSC E2E는 B-SEAM).

---

## 10. 미해결 / 후속 확인 항목

1. **물리 OSC 신호 바인딩** — reset_signal/seek_signal → CTRL_OSC* 핀 매핑·극성(active-LOW?)·OSC 비트매핑은 B-SEAM/6b(분석 §6). hook stub 시그니처(`which` 인자)가 B-SEAM에서 확장될 수 있음.
2. **물리버튼 레벨-팔로우** — B_RESET/B_SEEK 물리 입력(samd20 `max_us_reset_cnt=0xff`, 버튼 동안 유지·체인 없음)은 슬라이스4 물리 트리거와 함께. FSM에 "hold" 모드 추가 후보.
3. **과부하 복구 재사용** — overload 스테이지가 OVLD→RESET→자동 SEEK 시퀀스에 본 FSM 재사용(이미 자동 체인이 그 경로). overload 진입 시 `app_seek_reset_request(RESET)` 호출 설계.
4. **ICON write 헬퍼** — `app_lcd`에 ICON VP write 함수가 ICON_RUN 외 일반화 필요할 수 있음(구현 시 확인; 없으면 ICON_RUN 패턴으로 신설).
5. **START 직교 guard 위치 / 결합** — 양방향 함수 조회: `app_reg_command` START guard가 `app_seek_reset_active()` 조회 + `app_seek_reset_tick`이 `app_reg_measure()->us_run_status` 조회. **헤더 순환은 아님**(각 `.c`가 상대 헤더의 함수 선언만 include — 빌드 OK). weld 슬라이스2b M1(circular-dep refactor) 교훈: `app_seek_reset_active()`를 **read-only 상태 조회(부작용 없음)**로 한정해 결합 최소화. cpp-review 확인 항목.

---

*작성: 2026-06-17 SEEK/RESET 효과 브레인스토밍. samd20 참조 `ref/samd20/main.c:3634-3679`(트리거)·`4256/4262/4282/4287`(do_control GPIO)·`5388-5408`(자동 체인)·`118`(MAX_US_RESET_CNT). 분석 `docs/superpowers/analysis/2026-06-10-samd20-m16-ipc-semantics-verified.md:49-54/58/69`. 아키텍처=순수 FSM 분리(weld 패턴). 범위=상태머신+타이밍+ICON(HW 불요), 물리 OSC=B-SEAM 이연. 다음 = 본 spec 사용자 리뷰 → writing-plans(새 세션).*
