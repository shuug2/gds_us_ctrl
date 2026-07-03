# D5 Reconcile Implementation Plan — 3브랜치 스택 rebase (b→d→ch1)

> **요약**: 미머지 3브랜치를 main 위 선형 스택으로 재구축하는 git-수술 플랜.
> Task 1=백업+b'(FREQ_IN), Task 2=a+c 구간, Task 3=d 구간(ceiling semantic 병합),
> Task 4=d' cpp-review, Task 5=ch1'(표시 ch1 분리), Task 6=ch1' cpp-review+최종 스택
> 검증, Task 7=docs 갱신. 각 Task 끝 = reconfigure→빌드 0-warning→host PASS 게이트.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans
> (인라인 실행 — 충돌 해소는 판단 집약적·상태 의존적, spec §6이 컨트롤러 직접
> 수행으로 확정. subagent-driven 부적합). Steps use checkbox (`- [ ]`) syntax.

**Goal:** `feat/physical-io-slice-b` → `feat/physical-io-slice-d` → `feat/output-power-graph-ch1`을 현 main 위 스택으로 재구축, 각 tip 빌드+host PASS.

**Architecture:** cherry-pick 시퀀스로 keep-커밋만 순차 적용(= 수동 rebase; `git rebase -i` 미지원 환경). 충돌은 spec `docs/superpowers/specs/2026-07-04-d5-reconcile-design.md` §4~§5의 해소 규칙대로 커밋 단위로 해소하고 `git cherry-pick --continue`로 원 커밋 메시지를 보존한다.

**Tech Stack:** git cherry-pick, arm-none-eabi-gcc(CMake+Ninja), host gcc(fw/test).

## Global Constraints

- **머지/태그/push 금지** — HW 검증 후 별도 세션 (spec §8).
- 빌드는 our-code **0-warning** (vendor wiznet socket.h 3건 pre-existing 무관).
- 브랜치 전환/신규 소스 후 **reconfigure 필수**: `env -u STM32_TOOLCHAIN cmake -S fw -B fw/build -G Ninja` (GLOB 함정).
- host 테스트: `make -C fw/test` — 스위트 수 게이트: b'=8, a+c=10, d'=12, ch1'=12.
- stale docs 커밋 15개는 cherry-pick 대상에서 제외 (spec §3 표 — 전부 docs-only 실측).
- 충돌 해소 시 main의 error_status/ovtime/seek-reset/swallow-safe 로직과 d의 io_usout/overload/estop 로직 **양쪽 모두 보존** (union). `&&` 합산 금지 — START guard는 별도 break만 (advisor 결정).
- `ref/` untracked 디렉토리 불가촉.
- cherry-pick 충돌 해소는 원 커밋에 폴드 (`--continue`), 별도 fix 커밋 금지.

---

### Task 1: 백업 + b' 재구축 (FREQ_IN 측정)

**Files:**
- Modify (충돌 해소): `fw/include/app_reg.h`, `fw/src/app_reg.c`, `fw/src/app.c`, `fw/test/Makefile`
- Auto-apply (신규/무충돌): `fw/drivers/freq_ic.c`, `fw/include/freq_ic.h`, `fw/include/app_freq_fsm.h`, `fw/src/app_freq_fsm.c`, `fw/src/irq.c`, `fw/src/main.c`, `fw/src/periph.c`, `fw/include/periph.h`, `fw/test/test_app_freq_fsm.c`, `docs/superpowers/plans/2026-06-20-physical-io-slice-b.md`

**Interfaces:**
- Consumes: main tip의 `reg_run_limits_t`(4필드) + `void app_reg_tick(const reg_run_limits_t *lim)`.
- Produces: `reg_run_limits_t`에 `int16_t freq_cal_val` 추가. `reg_publish_measure(uint32_t now, int16_t freq_cal_val)`. Task 2~5는 이 5필드 struct 위에 쌓는다.

- [ ] **Step 1: 백업 ref 3개 생성 + 시작점 확인**

```bash
git checkout main && git status --short   # clean 확인 (ref/signal/ untracked만 허용)
git branch backup/pre-d5-slice-b feat/physical-io-slice-b
git branch backup/pre-d5-slice-d feat/physical-io-slice-d
git branch backup/pre-d5-ch1     feat/output-power-graph-ch1
git branch --list 'backup/pre-d5-*'   # 3개 확인
```

- [ ] **Step 2: 브랜치 ref를 main으로 이동 후 keep 4커밋 cherry-pick**

```bash
git checkout -B feat/physical-io-slice-b main
git cherry-pick 44e1a36 4cdcd79 3f2f202 2a22997
```

예상: `44e1a36`(plan doc) 클린, `4cdcd79`(FSM+test)에서 `fw/test/Makefile` 충돌
가능, `3f2f202`(드라이버+ISR) 클린 예상, `2a22997`(app_reg 배선)에서
`app_reg.h`/`app_reg.c`/`app.c` 충돌 확정.

- [ ] **Step 3: Makefile 충돌 해소 — 스위트 union**

main의 7 스위트(REG/MB/TCP/WELD/SR/POT/CFG) 전부 유지 + `BIN_FQ` 추가
(변수 선언, 빌드 룰, run 타깃 모두 slice-b 쪽 패턴 그대로):

```make
BIN_FQ  := /tmp/gds_test_app_freq_fsm
$(BIN_FQ): test_app_freq_fsm.c ../src/app_freq_fsm.c ../include/app_freq_fsm.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_app_freq_fsm.c ../src/app_freq_fsm.c
```

run 섹션에 `$(BIN_FQ)` 실행 라인 추가 (기존 스위트들과 동일 형식).

- [ ] **Step 4: `2a22997` 충돌 해소 — struct 경유 각색**

`fw/include/app_reg.h` — struct에 필드 추가, tick 시그니처는 main 그대로:

```c
typedef struct {
    uint16_t limit_on_time;   /* x10 ms; 0 = ceiling off (비-energy 경로) */
    uint8_t  energy_ctrl;     /* 1 = energy 모드 (on-time ceiling 대체) */
    uint32_t limit_energy;    /* 에너지-도달 정상정지 임계 (curr_energy 비교) */
    uint16_t limit_out_time;  /* OVTIME 한계 = 초 (0 = OVTIME off) */
    int16_t  freq_cal_val;    /* FREQ_IN 표시 보정 → freq_fsm_compute (slice-B) */
} reg_run_limits_t;
```

`fw/src/app_reg.c` — b의 변경을 main 코드 위에 이식:
1. `#include "app_freq_fsm.h"` 추가 (include 블록 끝).
2. `reg_state_t`에 `uint16_t last_freq;` 추가 (`last_energy` 아래, b 주석 그대로).
3. `last_freq` 래치를 `last_energy` 래치가 있는 **두 곳 모두**에 추가 —
   `reg_stop_run()` 헬퍼(자동 정지)와 `US_CMD_RUN_RELEASE` 분기(수동 정지):
   ```c
   g_reg.last_freq = g_measure.curr_freq;   /* freq 래치 — last_energy 패턴 (samd20 us_off) */
   ```
   (b 원본은 RUN_RELEASE에만 있었으나 main의 자동정지 경로가 헬퍼로 분리됐으므로
   양쪽 래치가 b의 의도(us_off 시 래치)와 동등 — samd20 us_off 충실.)
4. `reg_publish_measure` 시그니처 확장 + freq publish (함수 끝부분,
   `us_run_status` publish 직전):
   ```c
   static void reg_publish_measure(uint32_t now, int16_t freq_cal_val)
   ...
       g_measure.curr_freq = freq_fsm_compute(freq_cal_val);
       g_measure.last_freq = g_reg.last_freq;
   ```
   (b의 "run 게이팅 없음 — 무신호면 FSM이 0 반환" 주석 블록 유지.)
5. `app_reg_tick` 내 호출부: `reg_publish_measure(now, lim->freq_cal_val);`

`fw/src/app.c` — main의 lim struct 초기화에 필드 1줄 추가:

```c
            .limit_out_time = rc->limit_out_time,
            .freq_cal_val   = rc->freq_cal_val,
```

해소 후: `git add -A && git cherry-pick --continue` (원 메시지 유지).

- [ ] **Step 5: 게이트 — reconfigure + 빌드 + host 8스위트**

```bash
env -u STM32_TOOLCHAIN cmake -S fw -B fw/build -G Ninja && cmake --build fw/build
make -C fw/test
```

기대: our-code 0-warning, 8스위트 전부 PASS (기존 7 + `test_app_freq_fsm`).
실패 시 해소 지점 수정 후 `git commit --amend` (tip 커밋에 폴드).

- [ ] **Step 6: b' 확정 확인**

```bash
git log --oneline main..feat/physical-io-slice-b   # 4커밋 (plan, fsm, driver, wiring)
git diff backup/pre-d5-slice-b feat/physical-io-slice-b -- fw/drivers/freq_ic.c fw/src/app_freq_fsm.c
# 기대: 신규 파일들은 원본과 동일 (변경 無)
```

---

### Task 2: d' 재구축 1/2 — slice-a + slice-c 구간

**Files:**
- Modify (충돌 해소): `fw/src/board.c`, `fw/include/board.h`, `fw/src/app.c`, `fw/src/app_reg.c`, `fw/src/app_modbus.c`, `fw/test/Makefile`
- Auto-apply (신규): `fw/drivers/io.c`, `fw/include/io.h`, `fw/src/app_buzzer*.c/h`, `fw/src/app_overload*.c/h` 등 slice-a/c 신규 파일 + host tests + spec/plan docs

**Interfaces:**
- Consumes: Task 1의 5필드 `reg_run_limits_t`.
- Produces: `io_*` 드라이버, `app_buzzer_tick()`, `app_overload_tick()`/`app_overload_active()`, `app_lcd_set_overload()`, `MB_STATUS_OVLD=0x04`, app_reg START guard의 overload break. board.c/h에서 heartbeat 제거.

- [ ] **Step 1: slice-d ref를 b' 위로 이동 후 a+c keep 커밋 cherry-pick**

```bash
git checkout -B feat/physical-io-slice-d feat/physical-io-slice-b
git cherry-pick 0487551 3e89252 bca437a 5a81ae5 80128e3 104d3c8 \
                80985fc b0d6a52 c15cdca d2e0eb9 d03a0d5 0670ebd \
                a040ad6 b372da5 44d64f9 b8bcaed 0bf084c 20e0551
```

(drop: `0e2408b f3626d8 2e44cf2 f0de147 248b370 b207e01 7483d77` — docs-only.)

- [ ] **Step 2: `0487551` 충돌 해소 — board.c/h heartbeat 제거**

main의 board.c는 OD 승격판(heartbeat 잔존). d의 0487551은 heartbeat 제거 +
io 드라이버 도입. 해소 = **d 방향**: `board_heartbeat_toggle()` 정의(`board.c`)와
선언(`board.h`) 삭제, `HB_PORT/HB_PIN` define 삭제. main의 OSC OD 블록
(`CTRL_OSC_OUT_PINS` + `GPIO_MODE_OUTPUT_OD` + idle SET)은 **유지** —
커밋 시점 board.c가 "OD 3채널만 초기화, heartbeat 없음" 상태면 정답.
main에 heartbeat 콜러는 이미 없음(사전 grep 확인) — 삭제로 끝.

- [ ] **Step 3: 나머지 a+c 충돌 해소 — 규칙**

- `app.c` (bca437a buzzer, d2e0eb9 overload 글루): main의 superloop 구조/주석/
  i2c 관측 블록(§6) 유지, d의 tick 호출을 원 주석과 함께 문서화된 위치에 삽입 —
  `app_weld_tick()` 다음에 2.55 overload, 2.6 seek_reset 앞. buzzer는 modbus 뒤.
- `app_reg.c` (80128e3 usout hook): d의 `us_out_on` 필드 + `io_usout()` 전이
  hook을 main 코드에 이식 (publish의 run-output 전이 감지 위치 그대로).
- `app_reg.c` (d03a0d5 overload guard): START guard에 별도 break 삽입 —
  main의 `error_status` break **다음**:
  ```c
            /* 과부하 활성 중 START 차단 (SAMD20 SYS_ERROR가 START 막음).
             * seek_reset_active와 동일 직교 — 별도 break (swallow consume 뒤). */
            if (app_overload_active() != 0u) {
                break;
            }
  ```
- `app_modbus.c` (0670ebd): `MB_STATUS_OVLD`(0x04) 비트 추가 — main의
  `MB_STATUS_OVTIME`(bit3=0x08) 라인과 공존 (비트 충돌 없음).
- `fw/test/Makefile` (3e89252, b0d6a52): union — Task 1의 8스위트 + `BIN_BZ` + `BIN_OL`.

- [ ] **Step 4: 게이트 — reconfigure + 빌드 + host 10스위트**

```bash
env -u STM32_TOOLCHAIN cmake -S fw -B fw/build -G Ninja && cmake --build fw/build
make -C fw/test   # 10스위트 PASS (8 + buzzer_fsm + overload_fsm)
grep -rn 'board_heartbeat' fw/src fw/include   # 기대: 0건
```

---

### Task 3: d' 재구축 2/2 — slice-d 구간 + ceiling semantic 병합

**Files:**
- Modify (충돌 해소): `fw/include/app_reg.h`, `fw/src/app_reg.c`, `fw/src/app.c`, `fw/src/app_modbus.c`, `fw/src/board.c`, `fw/src/main.c`, `fw/test/Makefile`
- Auto-apply (신규): `fw/src/app_input*.c/h`, `fw/src/app_osc_init*.c/h` 등 + host tests + spec/plan docs

**Interfaces:**
- Consumes: Task 2 산출 전부.
- Produces: `reg_run_limits_t`에 `uint8_t model_type` 추가(6필드), 통합 termination 블록(30 s 안전 + energy/legacy ceiling), `app_input_tick()`, `app_estop_active()`, `MB_STATUS_ESTOP=0x02`, `board_osc4/board_reset/board_seek`, OSC 부팅 초기화. **spec §5.2 정정 표가 이 Task의 정본.**

- [ ] **Step 1: slice-d keep 커밋 cherry-pick**

```bash
git cherry-pick bece466 d0d1f9d c64c335 dcdea60 1f4f23c 37b981d 4f1c8ca \
                00835cb 81a44c0 8006e9c
```

(drop: `0c3497c 56575c4 df00e58 9b341fc d70600c` — docs-only.)

- [ ] **Step 2: `1f4f23c` 충돌 해소 — START guard E-stop break + REMOTE**

`app_reg.c` START guard 최종 순서 (전부 **별도 break**, `&&` 합산 금지):

```c
            /* 1. swallow consume (TOUCH, V30 data=0 페어링) — main 그대로 */
            /* 2. app_seek_reset_active() break — main 그대로 */
            /* 3. g_reg.error_status break (OVTIME 등) — main 그대로 */
            /* 4. app_overload_active() break — Task 2에서 삽입됨 */
            /* 5. E-stop 활성 중 START 차단 (SAMD20 SYS_ESTOP). overload와 동일 직교 —
             *    별도 break (swallow 대칭 보존). 레벨 기반(E-stop 떼면 자동 해제). */
            if (app_estop_active() != 0u) {
                break;
            }
```

이 커밋의 "ceiling에 US_REMOTE 추가"는 다음 Step 3의 통합 블록으로 흡수
(중간 상태는 빌드만 되면 됨 — 1f4f23c 시점 해소는 main의 ceiling 블록
소스 조건에 `|| (rs == (uint8_t)US_REMOTE)`만 추가해 두면 충분).

- [ ] **Step 3: `00835cb` 충돌 해소 — 통합 termination 블록 (핵심)**

`app_reg.h`: struct에 `uint8_t model_type; /* 0=hand — legacy ceiling 게이트 (slice-D) */`
추가. `app_reg.c` 상단에 d의 define 2개 추가:

```c
#define ON_TIME_SAFETY_MS  30000u   /* 30 s */
#define MODEL_TYPE_HAND    0u       /* model_type/sys_mode: 0=hand (limit_on_time gate) */
```

`app_reg_tick`의 기존 ceiling/energy 블록 전체를 아래 2블록으로 교체
(spec §5.2 정정 표 = 정본; d의 30 s 블록 주석 verbatim 유지, helper는
main 명칭 `reg_stop_run`으로 통일 — d의 `reg_run_stop_latch` 호출을 전부 개명):

```c
    /* (1) Absolute on-time SAFETY ceiling — ON_TIME_SAFETY_MS (30 s). Fires for
     * ANY active ultrasonic run (TOUCH/COMM/REMOTE/CYCLE) in ANY mode,
     * independent of limit_on_time (fires even when limit_on_time==0), NOT
     * panel-editable. Transducer runaway backstop. run_start_ms is stamped at
     * every START edge (incl. US_CYCLE via app_weld), so the 30 s base is valid
     * for all sources. User decision 2026-06-27: unconditional, all incl. weld. */
    {
        uint8_t rs = g_reg.us_run_status;
        if ((rs == (uint8_t)US_TOUCH) || (rs == (uint8_t)US_COMM) ||
            (rs == (uint8_t)US_REMOTE) || (rs == (uint8_t)US_CYCLE)) {
            if ((uint32_t)(now - g_reg.run_start_ms) >= ON_TIME_SAFETY_MS) {
                reg_stop_run(rs);
#ifdef REG_TRACE
                mon_printf("[reg] 30s safety ceiling -> stop\r\n");
#endif
            }
        }
    }

    /* (2) 런 자동 종료 — energy 모드면 에너지-도달 정상정지 + OVTIME이 운영
     * ceiling을 대체 (ovtime, legacy main.c:5270 분기; REMOTE는 slice-D가 소스
     * 추가). 비-energy면 legacy limit_on_time ceiling — slice-D 이중화 결정
     * (2026-06-27): HAND 모드의 COMM/REMOTE만, NOT TOUCH (V30 lost-release
     * 리스크는 위 30 s 안전 ceiling이 커버). US_CYCLE은 양쪽 모두 자연 제외
     * (weld 한계가 담당). limit_*은 매 call cfg 주입(M1) — 패널 편집 즉시 반영. */
    {
        uint8_t rs = g_reg.us_run_status;
        if ((rs == (uint8_t)US_TOUCH) || (rs == (uint8_t)US_COMM) ||
            (rs == (uint8_t)US_REMOTE)) {
            uint32_t elapsed = (uint32_t)(now - g_reg.run_start_ms);
            if (lim->energy_ctrl != 0u) {
                reg_energy_outcome_t oc = reg_energy_termination(
                    lim->energy_ctrl, g_measure.curr_energy, lim->limit_energy,
                    elapsed, lim->limit_out_time);
                if (oc != REG_RUN_CONTINUE) {
                    reg_stop_run(rs);
                    if (oc == REG_RUN_FAULT_OVTIME) {
                        g_reg.error_status |= ERR_OVTIME;   /* samd20 main.c:5292 */
                    }
#ifdef REG_TRACE
                    mon_printf("[reg] energy stop oc=%u (e=%lu/%lu t=%lums)\r\n",
                               (unsigned)oc, (unsigned long)g_measure.curr_energy,
                               (unsigned long)lim->limit_energy, (unsigned long)elapsed);
#endif
                }
            } else if ((lim->model_type == MODEL_TYPE_HAND) &&
                       ((rs == (uint8_t)US_COMM) || (rs == (uint8_t)US_REMOTE)) &&
                       (lim->limit_on_time != 0u) &&
                       (elapsed >= (uint32_t)lim->limit_on_time * ON_TIME_UNIT_MS)) {
                reg_stop_run(rs);   /* COMM/REMOTE: no swallow (legacy) */
#ifdef REG_TRACE
                mon_printf("[reg] on-time ceiling (%u x10ms) -> stop\r\n",
                           (unsigned)lim->limit_on_time);
#endif
            }
        }
    }
```

`app.c` lim 초기화에 `.model_type = rc->model_type,` 추가.

- [ ] **Step 4: 나머지 d 충돌 해소 — 규칙**

- `dcdea60` (app_input 글루/배선): `app.c`에 2.57 위치(overload 다음,
  seek_reset 앞) 삽입, d 주석 그대로.
- `37b981d` (`app_modbus.c`): `MB_STATUS_ESTOP`(0x02) 추가 — OVLD(0x04)/
  OVTIME(0x08)과 공존.
- `8006e9c` (`board.c`/`main.c`): board.c는 **d 최종판 wholesale 채택**
  (main의 OD는 부분집합 — d판 = OD + `board_osc4/board_reset/board_seek` +
  heartbeat 없음). main.c의 OSC 부팅 초기화(블로킹, app_init 전) 삽입은
  d 그대로; main.c의 main 쪽 변경(있다면)과 위치 병합.
- `c64c335`/`4f1c8ca` (`fw/test/Makefile`): union — 10 + `BIN_IN` + `BIN_OSC` = 12.

- [ ] **Step 5: 게이트 — 빌드 + host 12스위트 + 등가 검증**

```bash
env -u STM32_TOOLCHAIN cmake -S fw -B fw/build -G Ninja && cmake --build fw/build
make -C fw/test   # 12스위트 PASS
git diff backup/pre-d5-slice-d feat/physical-io-slice-d -- fw/src/board.c fw/include/board.h
# 기대: 빈 출력 (board.c/h 최종 = 구 d tip과 byte-identical)
git grep -n 'io_usout' feat/physical-io-slice-d -- fw/src/app_reg.c   # 비어있으면 안 됨
git grep -n 'reg_run_stop_latch' feat/physical-io-slice-d -- fw/       # 기대: 0건 (개명 완료)
```

---

### Task 4: d' app_reg.c cpp-reviewer 게이트

**Files:** 읽기 전용 리뷰 — `fw/src/app_reg.c`, `fw/include/app_reg.h`, `fw/src/app.c`

- [ ] **Step 1: cpp-reviewer 서브에이전트 디스패치**

리뷰 대상 = `git diff main feat/physical-io-slice-d -- fw/src/app_reg.c fw/include/app_reg.h fw/src/app.c`.
리뷰 관점 지정: ① 통합 termination 블록이 spec §5.2 정정 표와 일치하는가
② START guard 5-break가 swallow-safe 구조를 훼손하지 않는가 ③ struct 수명/
주입 규약(N1: 호출자 스택 임시값) 유지 ④ REG_TRACE 경로 컴파일 안전.
서브에이전트에 메모리 기반 부수 작업 금지 명시.

- [ ] **Step 2: Critical/Important 지적 반영**

수정은 해당 원인 커밋에 폴드 불가(이미 시퀀스 완료)하므로 tip에
`fix(reg): d5 reconcile 리뷰 반영 — <내용>` 단일 커밋. Minor는 defer 기록.
반영 후 Task 3 Step 5 게이트 재실행.

---

### Task 5: ch1' 재구축 (표시값 ch1 분리)

**Files:**
- Modify (충돌 해소): `fw/include/app_reg.h`, `fw/src/app_reg.c`, `fw/src/app.c`, `fw/include/app_reg_calc.h`, `fw/src/app_reg_calc.c`, `fw/test/test_app_reg_calc.c`
- Auto-apply: `docs/superpowers/specs/2026-06-28-output-power-graph-ch1-*.md` 등 spec/plan docs

**Interfaces:**
- Consumes: Task 3의 6필드 struct + `reg_publish_measure(now, freq_cal_val)`.
- Produces: `reg_run_limits_t`에 `int16_t cal_val` 추가(7필드 최종), `reg_current_from_adc(uint16_t ch1_avg, int16_t cal_val)`, `reg_power_from_amp(uint16_t curr_amp)`, publish의 ch1 repoint.

- [ ] **Step 1: ch1 ref를 d' 위로 이동 후 keep 5커밋 cherry-pick**

```bash
git checkout -B feat/output-power-graph-ch1 feat/physical-io-slice-d
git cherry-pick fefdc64 be53d13 116ed24 1f43b8d f30bc51
```

(drop: `a7928e0 27b6888` — docs-only.)

- [ ] **Step 2: `116ed24` 충돌 해소 — app_reg_calc additive 병합**

`app_reg_calc.h`: main의 `reg_energy_from_acc`/`reg_energy_termination` 선언 뒤에
ch1의 `reg_current_from_adc`/`reg_power_from_amp` 선언 추가 (양쪽 주석 verbatim).
`app_reg_calc.c`: 동일하게 함수 2개 append. `test_app_reg_calc.c`: main의
energy_termination 케이스들 + ch1의 current_from_adc/power_from_amp 케이스 union.

- [ ] **Step 3: `1f43b8d` 충돌 해소 — publish ch1 repoint**

`app_reg.h`: struct에 `int16_t cal_val; /* ch1 표시 전류 보정 (config, ch1 slice) */`
추가 — **7필드 최종형** (spec §4 코드블록과 일치 확인).
`app_reg.c`:
1. `reg_state_t`에 `int16_t cal_val;` 추가 + tick 진입부에
   `g_reg.cal_val = lim->cal_val;` (ch1 주석 그대로).
2. `reg_publish_measure`의 amp/power 소스 교체 — b'의 freq 라인, main의 에너지
   적분, d'의 us_out_on hook은 **그대로 두고** 아래만 교체:
   ```c
       uint8_t active = (uint8_t)(g_reg.us_run_status != (uint8_t)US_IDLE);
       /* 표시 전류/전력은 ch1(소비전류)에서 — 레귤레이션(ch0/reg_scale)과 분리.
        * SAMD20 cal_real_val 포팅 (spec §3). 피크홀드 비교 소스도 ch1 산출값. */
       uint16_t disp_amp = reg_current_from_adc(g_reg.ch1_avg, g_reg.cal_val);
       g_measure.curr_amp = disp_amp;
       if (active && (disp_amp > g_reg.max_amp)) {
           g_reg.max_amp = disp_amp;
       }
       uint16_t disp_pwr = reg_power_from_amp(disp_amp);
       g_measure.curr_power = active ? disp_pwr : 0u;
       if (active && (disp_pwr > g_reg.max_power)) {
           g_reg.max_power = disp_pwr;
       }
   ```
   에너지 적분은 main 구조 그대로 curr_power 누산 → **이제 ch1 기반**
   (교차영향 f30bc51 의도된 동작 — weld/OVTIME 에너지 판정 입력도 ch1로 이동).
`app.c`: lim 초기화에 `.cal_val = rc->cal_val,` 추가 — 7필드 완성.

- [ ] **Step 4: 게이트 — 빌드 + host 12스위트(reg_calc 확장)**

```bash
env -u STM32_TOOLCHAIN cmake -S fw -B fw/build -G Ninja && cmake --build fw/build
make -C fw/test   # 12스위트 PASS, test_app_reg_calc에 ch1 케이스 포함 확인
/tmp/gds_test_app_reg_calc   # 단독 재실행으로 union 케이스 수 육안 확인
```

---

### Task 6: ch1' cpp-review + 최종 스택 검증

**Files:** 읽기 전용 검증.

- [ ] **Step 1: cpp-reviewer 서브에이전트 — ch1' 병합분**

대상 = `git diff feat/physical-io-slice-d feat/output-power-graph-ch1 -- fw/`.
관점: ① publish 병합이 b'freq/d'usout/main 에너지 적분을 보존했는가
② ch1 repoint의 에너지 교차영향이 주석으로 정직한가 ③ 7필드 struct 정합.
Critical/Important는 Task 4 Step 2와 동일 방식으로 tip fix 커밋 + 게이트 재실행.

- [ ] **Step 2: 스택 전체 구조 검증**

```bash
git log --oneline --graph main feat/physical-io-slice-b feat/physical-io-slice-d feat/output-power-graph-ch1 | head -50
# 기대: main → b'(4) → d'(28±리뷰fix) → ch1'(5±리뷰fix) 선형 스택
git merge-base feat/physical-io-slice-b main            # = main tip
git merge-base feat/physical-io-slice-d feat/physical-io-slice-b   # = b' tip
git merge-base feat/output-power-graph-ch1 feat/physical-io-slice-d  # = d' tip
```

- [ ] **Step 3: 코드 등가/보존 총점검**

```bash
# 각 단위의 "자기 파일"이 원본과 등가인지 (충돌 없던 파일은 byte-identical 기대)
git diff backup/pre-d5-slice-b feat/output-power-graph-ch1 -- fw/src/app_freq_fsm.c fw/drivers/freq_ic.c
git diff backup/pre-d5-slice-d feat/output-power-graph-ch1 -- fw/src/board.c fw/src/app_input_fsm.c fw/src/app_osc_init_fsm.c
git diff backup/pre-d5-ch1     feat/output-power-graph-ch1 -- fw/src/app_reg_calc.c
# app_reg_calc.c만 main의 energy 함수가 더해져 diff 有(추가분만) — 삭제 hunk가 있으면 실패
```

최종 게이트: ch1' tip에서 빌드 0-warning + host 12스위트 PASS 재확인 +
FLASH/RAM 사용률 기록.

---

### Task 7: docs 갱신 + 세션 기록

**Files:**
- Modify: `docs/NEXT_STEPS.md`(§1.3 D5 행), `docs/changelog.md`, `docs/superpowers/RESUME.md`, `HANDOFF.md`, `CLAUDE.md`(진행 블록)
- 메모리: `project_audit_2026_07.md`(D5 완료), `project_physical_io_layer.md`/`project_osc_boot_init.md`/`project_output_power_graph_ch1.md`(rebase 반영)

- [ ] **Step 1: main으로 전환 후 docs 갱신**

D5 상태 = "reconcile 완료(코딩 세션 몫), 3브랜치 main 위 스택, 머지/태그는 HW
검증 후"로 기록. ⚠ 거동 변화 2건(spec §5.2 — TOUCH ceiling 소멸, OVTIME>30 s
캡)을 HW 검증 체크리스트에 명시. backup ref 3개 존재 기록.

- [ ] **Step 2: 커밋**

```bash
git add docs/ HANDOFF.md CLAUDE.md
git commit -m "docs: D5 reconcile 완료 — 3브랜치 main 위 스택 재구축(b'→d'→ch1'), 머지/태그=HW 게이트"
```

(push는 사용자 확인 후 — Global Constraints.)
