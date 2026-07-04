# Weld 슬라이스4 (TRIGGER+양손 트리거+abort+게이팅+D2 클램프) Implementation Plan

> **요약**: spec `2026-07-04-stage-weld-cycle-slice4-trigger-design.md`를 10 Task로 구현.
> 순서 = D4/H1 선결(Task 1) → weld FSM 확장(2~4) → 트리거 FSM 신설(5) →
> app_reg 쿼리(6) → 글루 배선+SETUP 게이트(7) → D2 클램프 M4/M3(8~9) → 최종 통합(10).
> 전 Task TDD(host 가능 범위), 빌드 0-warning, Task별 커밋.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** samd20 공압 프레스 사이클의 물리 계층(양손 트리거/TRIGGER 모드/안전 abort) + 감사 D2/D4 수정을 weld 스테이지에 흡수.

**Architecture:** A안 — 순수 `app_weld_trigger_fsm` 신설 + 기존 `app_weld_fsm` 확장(TRIGGER/abort/H1 래치) + 글루(`app_weld.c`) 배선. 사이클 진입은 `app_reg_start_allowed()`(신설)로 게이팅(의도된 deviation), SETUP 페이지에서는 step 동결.

**Tech Stack:** STM32F410RBT bare-metal C11 superloop, host 테스트 = `fw/test`(`cc` 단독 컴파일, CHECK_EQ 매크로).

## Global Constraints

- 빌드: `env -u STM32_TOOLCHAIN cmake -S fw -B fw/build -G Ninja && env -u STM32_TOOLCHAIN cmake --build fw/build` — **our-code 0-warning** (vendor wiznet socket.h 경고 3건은 pre-existing 무관). **신규 .c 추가 후 반드시 reconfigure**(GLOB 함정).
- host 테스트: `make -C fw/test test` — 기존 12스위트 + 신규 1 전부 PASS 유지.
- `ref/`·`fw/vendor/` = read-only. 요청 범위 밖 코드 무수정(사용자 규칙).
- 브랜치: `feat/stage-weld-slice4-trigger` (ch1' 스택 위 — main 단독 체리픽 불가).
- 커밋 형식: `<type>(weld|lcd|config|reg): <설명>` — 프로젝트 관례(한국어 설명).
- legacy 정본 = `ref/samd20/main.c` (라인 인용은 spec §1.1 표).
- slice-c/d 산출물(io/input/overload)은 **읽기 전용 소비만** — 기존 로직 무수정.

---

### Task 1: H1 — WELD 진입 모드 래치 + 전이 카운터 리셋 (D4 선결)

**Files:**
- Modify: `fw/src/app_weld_fsm.c`
- Test: `fw/test/test_app_weld_fsm.c`

**Interfaces:**
- Consumes: 기존 `weld_fsm_step(const weld_in_t*, weld_out_t*)` (시그니처 무변경)
- Produces: WELD 진행 중 `in->multi_ctrl`/`in->energy_ctrl` 토글이 exit 경로를 바꾸지 못하는 거동 (Task 2~4의 전제)

- [ ] **Step 1: 실패하는 테스트 작성** — `fw/test/test_app_weld_fsm.c`에 추가 (기존 `run_cycle`/`CHECK_EQ` 하네스 사용, main()에 호출 등록):

```c
/* H1: WELD 진행 중 multi_ctrl 토글(0->1)이 exit 경로를 바꾸지 못함 —
 * 시간-exit로 정상 완료해야 함 (감사 H1 근본수정). */
static void test_h1_multi_toggle_mid_weld_ignored(void)
{
    weld_fsm_init();
    weld_in_t in;
    memset(&in, 0, sizeof(in));
    in.limit_delay_time1 = 2u;   /* CYL1/CYL2 = 2 tick */
    in.limit_delay_time2 = 10u;  /* WELD = 10 tick (시간 모드) */
    in.limit_delay_time3 = 2u;
    in.output_power      = 100u;
    in.limit_mo_out1 = 60u; in.limit_mo_out2 = 80u;
    in.limit_mo_time1 = 1u;  in.limit_mo_time2 = 2u;  /* 토글이 반영되면 2 tick만에 스테핑 종료 */

    weld_out_t out;
    in.start = 1u;
    weld_fsm_step(&in, &out);            /* READY -> CYL1 */
    in.start = 0u;
    for (int i = 0; i < 3; i++) { weld_fsm_step(&in, &out); }  /* CYL1 소진 -> WELD 진입 */
    CHECK_EQ(weld_fsm_status(), WELD_WELD);
    CHECK_EQ(out.weld_start, 1u);

    in.multi_ctrl = 1u;                  /* 런중 토글 */
    int amp_changes = 0, stops = 0;
    for (int i = 0; i < 15 && weld_fsm_status() == WELD_WELD; i++) {
        weld_fsm_step(&in, &out);
        amp_changes += out.amp_change;
        stops       += out.weld_stop;
    }
    CHECK_EQ(amp_changes, 0);            /* 스테핑 미발동 = 래치 유효 */
    CHECK_EQ(stops, 1);                  /* 시간-exit 1회로 정상 HOLD 진입 */
    CHECK_EQ(weld_fsm_status(), WELD_HOLD);
}

/* H1: multi로 시작한 WELD는 런중 multi_ctrl=0 토글에도 스테핑 계속. */
static void test_h1_multi_off_toggle_mid_weld_ignored(void)
{
    weld_fsm_init();
    weld_in_t in;
    memset(&in, 0, sizeof(in));
    in.limit_delay_time1 = 2u; in.limit_delay_time2 = 10u; in.limit_delay_time3 = 2u;
    in.output_power = 100u;
    in.multi_ctrl = 1u;
    in.limit_mo_out1 = 60u; in.limit_mo_out2 = 80u;
    in.limit_mo_time1 = 3u; in.limit_mo_time2 = 6u;

    weld_out_t out;
    in.start = 1u;
    weld_fsm_step(&in, &out);
    in.start = 0u;
    for (int i = 0; i < 3; i++) { weld_fsm_step(&in, &out); }
    CHECK_EQ(weld_fsm_status(), WELD_WELD);

    in.multi_ctrl = 0u;                  /* 런중 off 토글 */
    int amp_changes = 0;
    for (int i = 0; i < 10 && weld_fsm_status() == WELD_WELD; i++) {
        weld_fsm_step(&in, &out);
        amp_changes += out.amp_change;
    }
    CHECK_EQ(amp_changes, 1);            /* 2단 전환 발동 = multi 래치 유지 */
    CHECK_EQ(weld_fsm_status(), WELD_HOLD);
}

/* H1: 이전 multi 사이클의 s_multi_stage/elapsed가 다음 사이클에 누출되지 않음. */
static void test_h1_counters_reset_between_cycles(void)
{
    weld_fsm_init();
    weld_in_t in;
    memset(&in, 0, sizeof(in));
    in.limit_delay_time1 = 2u; in.limit_delay_time2 = 10u; in.limit_delay_time3 = 2u;
    in.output_power = 100u;
    in.multi_ctrl = 1u;
    in.limit_mo_out1 = 60u; in.limit_mo_out2 = 80u;
    in.limit_mo_time1 = 3u; in.limit_mo_time2 = 6u;

    trace_t t;
    run_cycle(&in, &t, 200);             /* multi 사이클 1회 완주 */
    CHECK_EQ(t.amp_change_cnt, 1);

    run_cycle(&in, &t, 200);             /* 두 번째 사이클 — 카운터 fresh */
    CHECK_EQ(t.amp_change_cnt, 1);       /* stage 0부터 다시: 전환 정확히 1회 */
    CHECK_EQ(t.cycle_done_cnt, 1);
}
```

- [ ] **Step 2: 실패 확인** — `make -C fw/test test` → `test_h1_multi_toggle_mid_weld_ignored`에서 FAIL (amp_changes=1 관측: 토글이 라이브 반영되므로).

- [ ] **Step 3: 최소 구현** — `fw/src/app_weld_fsm.c`:

```c
/* 파일 상단 static 블록에 추가 (s_multi_elapsed 아래) */
static uint8_t  s_latched_multi;    /* H1: WELD 진입 시 multi_ctrl 스냅샷 (감사 D4) */
static uint8_t  s_latched_energy;   /* H1: WELD 진입 시 energy_ctrl 스냅샷 */
```

`weld_fsm_init()`에 `s_latched_multi = 0u; s_latched_energy = 0u;` 추가.

**래치 시점 = CYL1→WELD 전이 블록** (WELD 최초진입이 아님): CYL1 exit의 WELD 무장
(`in->energy_ctrl`로 temp_time 선택, 현 line 102)과 WELD 최초진입은 연속 2 step —
사이 1-tick 토글 창을 없애기 위해 무장과 exit가 **같은 스냅샷**을 쓰도록 전이 시점에
래치하고, temp_time 선택도 `s_latched_energy`로 통일한다:

```c
        } else if (s_temp_time == 0u) {          /* WELD_CYL1 전이 블록 */
            s_f_status_start = 0u;
            s_run_status     = WELD_WELD;
            s_latched_multi  = in->multi_ctrl;   /* H1 래치 (전이 시점 = 단일 스냅샷) */
            s_latched_energy = in->energy_ctrl;
            s_multi_stage    = 0u;               /* H1 카운터 리셋 */
            s_multi_elapsed  = 0u;
            s_comp_time = (in->limit_delay_time2 > 6u) ? WELD_COMP_FULL
                                                       : in->limit_delay_time2;
            if (s_latched_energy) {
                s_temp_time = weld_backstop_ticks(in->limit_out_time);
            } else {
                s_temp_time = (in->limit_delay_time2 > 6u) ? in->limit_delay_time2
                                                           : WELD_COMP_FULL;
            }
        }
```

WELD 최초진입 블록(f_status_start==0)과 이후 분기(`else if (in->multi_ctrl)` /
`else if (in->energy_ctrl)`)는 **`in->` 참조를 `s_latched_*`로 치환만** 한다
(최초진입의 `s_multi_stage=0; s_multi_elapsed=1;` 초기화는 기존 유지 — 전이 블록의
리셋과 중복이지만 진입 semantics 보존). READY 복귀 지점(정상 CYL2 완료 + energy
backstop abort + default fail-safe)에서 `s_latched_multi = s_latched_energy = 0u;` 클리어.

- [ ] **Step 4: 테스트 통과 확인** — `make -C fw/test test` → 전 스위트 PASS.

- [ ] **Step 5: 빌드 0-warning 확인** — `env -u STM32_TOOLCHAIN cmake --build fw/build` (reconfigure 불요 — 기존 파일만).

- [ ] **Step 6: 커밋**

```bash
git add fw/src/app_weld_fsm.c fw/test/test_app_weld_fsm.c
git commit -m "fix(weld): H1 근본수정 — WELD 모드 래치(CYL1 전이 시점)+전이 카운터 리셋 (감사 D4)"
```

---

### Task 2: weld FSM abort 입력

**Files:**
- Modify: `fw/include/app_weld_fsm.h`, `fw/src/app_weld_fsm.c`
- Test: `fw/test/test_app_weld_fsm.c`

**Interfaces:**
- Produces: `weld_in_t.abort`(uint8_t, 신규 필드) — 1이면 임의 상태에서 SOL OFF + (WELD면 weld_stop 엣지) + READY 복귀, cycle_done 미발행. Task 7 글루가 사용.

- [ ] **Step 1: 실패하는 테스트 작성**:

```c
/* abort: 각 상태에서 -> SOL OFF + READY + work_cnt 미발행; WELD에서만 weld_stop. */
static void test_abort_from_each_state(void)
{
    static const int steps_to_state[] = {1, 4, 6, 9};   /* CYL1/WELD/HOLD/CYL2 도달 step 수 */
    static const uint8_t expect_state[] = {WELD_CYL1, WELD_WELD, WELD_HOLD, WELD_CYL2};
    for (int s = 0; s < 4; s++) {
        weld_fsm_init();
        weld_in_t in;
        memset(&in, 0, sizeof(in));
        in.limit_delay_time1 = 2u; in.limit_delay_time2 = 2u; in.limit_delay_time3 = 2u;
        in.output_power = 100u;
        weld_out_t out;
        in.start = 1u;
        weld_fsm_step(&in, &out);
        in.start = 0u;
        for (int i = 1; i < steps_to_state[s]; i++) { weld_fsm_step(&in, &out); }
        CHECK_EQ(weld_fsm_status(), expect_state[s]);
        uint8_t was_weld = (weld_fsm_status() == WELD_WELD);

        in.abort = 1u;
        weld_fsm_step(&in, &out);
        CHECK_EQ(weld_fsm_status(), WELD_READY);
        CHECK_EQ(out.sol_dn, 0u);
        CHECK_EQ(out.cycle_done, 0u);
        CHECK_EQ(out.weld_stop, was_weld ? 1u : 0u);
        in.abort = 0u;

        /* abort 후 재시작 정상 */
        trace_t t;
        run_cycle(&in, &t, 200);
        CHECK_EQ(t.cycle_done_cnt, 1);
    }
}

/* abort: READY에서는 no-op. */
static void test_abort_in_ready_noop(void)
{
    weld_fsm_init();
    weld_in_t in; memset(&in, 0, sizeof(in));
    in.abort = 1u;
    weld_out_t out;
    weld_fsm_step(&in, &out);
    CHECK_EQ(weld_fsm_status(), WELD_READY);
    CHECK_EQ(out.weld_stop, 0u);
}
```

`steps_to_state`는 ldt1=ldt2=ldt3=2 기준: step1=READY→CYL1, step2=CYL1 진입(SOL ON),
step4=WELD 진입, step6=HOLD 진입, step9=CYL2 진입. **작성 후 CHECK_EQ(status)로 도달
상태를 먼저 검증하므로 step 수가 어긋나면 테스트가 즉시 알려줌** — 구현 시 실측으로 보정.

- [ ] **Step 2: 실패 확인** — 컴파일 에러(`weld_in_t`에 `abort` 없음). Expected: build FAIL.

- [ ] **Step 3: 최소 구현** — `app_weld_fsm.h`의 `weld_in_t` 끝에:

```c
    uint8_t  abort;              /* 1 = 즉시 abort: SOL OFF + READY (E-stop/overload/fault/safety, slice4) */
```

`app_weld_fsm.c`의 `weld_fsm_step` — `memset(out,...)` 직후, temp_time 감소 **앞**에:

```c
    /* abort (slice4 §3.4): 임의 상태 -> SOL OFF + READY. WELD 중이면 US 정지 엣지
     * (글루 US_CYCLE RUN_RELEASE — slice-c/d force-stop과 이중 안전). work_cnt 미발행.
     * legacy: E-stop main.c:1415 / SYS_ERROR 1664-1665. */
    if ((in->abort != 0u) && (s_run_status != WELD_READY)) {
        if (s_run_status == WELD_WELD) {
            out->weld_stop = 1u;
        }
        s_sol_dn         = 0u;
        s_run_status     = WELD_READY;
        s_f_status_start = 0u;
        s_temp_time      = 0u;
        s_multi_stage    = 0u;
        s_multi_elapsed  = 0u;
        s_latched_multi  = 0u;
        s_latched_energy = 0u;
        out->run_status  = s_run_status;
        out->sol_dn      = s_sol_dn;
        return;
    }
```

- [ ] **Step 4: 테스트 통과 확인** — `make -C fw/test test` → PASS.
- [ ] **Step 5: 빌드 확인** — 0-warning.
- [ ] **Step 6: 커밋**

```bash
git add fw/include/app_weld_fsm.h fw/src/app_weld_fsm.c fw/test/test_app_weld_fsm.c
git commit -m "feat(weld): FSM abort 입력 — 임의 상태 SOL OFF+READY 복귀, WELD면 US 정지 엣지 (spec §3.4)"
```

---

### Task 3: weld FSM TRIGGER 모드

**Files:**
- Modify: `fw/include/app_weld_fsm.h`, `fw/src/app_weld_fsm.c`
- Test: `fw/test/test_app_weld_fsm.c`

**Interfaces:**
- Produces: `weld_in_t`에 `limit_trigger_time2`(uint16_t)/`limit_trigger_time3`(uint16_t)/`dn_edge`(uint8_t)/`up_edge`(uint8_t) 신규 필드. Task 7 글루가 주입. `run_mode`(기존 필드)가 사이클 시작 시점에 래치되어 유효해짐.

- [ ] **Step 1: 실패하는 테스트 작성**:

```c
/* TRIGGER: 풀 사이클 — CYL1은 dn_edge까지 대기, HOLD=trigger_time3, CYL2 즉시. */
static void test_trigger_full_cycle(void)
{
    weld_fsm_init();
    weld_in_t in; memset(&in, 0, sizeof(in));
    in.run_mode          = 1u;    /* MODE_TRIGGER */
    in.limit_delay_time1 = 50u;   /* TRIGGER에선 미사용이어야 함 */
    in.limit_trigger_time2 = 3u;  /* WELD 3 tick (comp swap: <=6 -> comp=3, temp=7) */
    in.limit_trigger_time3 = 2u;  /* HOLD 2 tick */
    in.output_power = 100u;
    weld_out_t out;

    in.start = 1u;
    weld_fsm_step(&in, &out);                 /* READY -> CYL1 */
    in.start = 0u;
    CHECK_EQ(weld_fsm_status(), WELD_CYL1);

    for (int i = 0; i < 20; i++) { weld_fsm_step(&in, &out); }
    CHECK_EQ(weld_fsm_status(), WELD_CYL1);   /* dn 없음 -> 무기한 대기 (타임아웃 없음, 죽은 CYL_TIMEOUT 충실) */
    CHECK_EQ(out.sol_dn, 1u);

    in.dn_edge = 1u;
    weld_fsm_step(&in, &out);                 /* dn 소비 -> WELD */
    in.dn_edge = 0u;
    CHECK_EQ(weld_fsm_status(), WELD_WELD);

    int stops = 0, dones = 0, hold_steps = 0, cyl2_steps = 0;
    for (int i = 0; i < 30 && weld_fsm_status() != WELD_READY; i++) {
        weld_fsm_step(&in, &out);
        stops += out.weld_stop;
        dones += out.cycle_done;
        if (weld_fsm_status() == WELD_HOLD) hold_steps++;
        if (weld_fsm_status() == WELD_CYL2) cyl2_steps++;
    }
    CHECK_EQ(stops, 1);
    CHECK_EQ(dones, 1);                       /* work_cnt 경로 정상 */
    CHECK_EQ((cyl2_steps <= 2), 1);           /* CYL2 = up 강제 -> 사실상 즉시 (§3.3) */
}

/* TRIGGER: READY 중 들어온 dn_edge(stale)는 사이클 시작 시 클리어 (main.c:1478). */
static void test_trigger_stale_dn_cleared_at_start(void)
{
    weld_fsm_init();
    weld_in_t in; memset(&in, 0, sizeof(in));
    in.run_mode = 1u;
    in.limit_trigger_time2 = 3u; in.limit_trigger_time3 = 2u;
    in.output_power = 100u;
    weld_out_t out;

    in.dn_edge = 1u;
    weld_fsm_step(&in, &out);                 /* READY에서 stale 엣지 */
    in.dn_edge = 0u;
    in.start = 1u;
    weld_fsm_step(&in, &out);                 /* 시작 — stale 클리어돼야 함 */
    in.start = 0u;
    weld_fsm_step(&in, &out);                 /* CYL1 최초진입(SOL ON) */
    for (int i = 0; i < 5; i++) { weld_fsm_step(&in, &out); }
    CHECK_EQ(weld_fsm_status(), WELD_CYL1);   /* stale로 exit하지 않음 */
}

/* run_mode 래치: DELAY로 시작한 사이클은 런중 TRIGGER 전환에도 DELAY 규칙 유지. */
static void test_run_mode_latched_at_cycle_start(void)
{
    weld_fsm_init();
    weld_in_t in; memset(&in, 0, sizeof(in));
    in.run_mode = 0u;
    in.limit_delay_time1 = 2u; in.limit_delay_time2 = 2u; in.limit_delay_time3 = 2u;
    in.output_power = 100u;
    weld_out_t out;
    in.start = 1u;
    weld_fsm_step(&in, &out);
    in.start = 0u;
    in.run_mode = 1u;                         /* 런중 전환 */
    int done = 0;                             /* 이어서 그대로 완주 (DELAY 시간 규칙) */
    for (int i = 0; i < 50; i++) {
        weld_fsm_step(&in, &out);
        done += out.cycle_done;
    }
    CHECK_EQ(done, 1);                        /* dn_edge 없이 완주 = DELAY 래치 유지 */
}
```

- [ ] **Step 2: 실패 확인** — 컴파일 에러(`limit_trigger_time2` 등 필드 없음). Expected: FAIL.

- [ ] **Step 3: 최소 구현** — `app_weld_fsm.h`의 `weld_in_t`에 추가:

```c
    uint16_t limit_trigger_time2; /* TRIGGER WELD duration (x10 ms) — slice4 */
    uint16_t limit_trigger_time3; /* TRIGGER HOLD duration (x10 ms) — slice4 */
    uint8_t  dn_edge;            /* 1-shot: SENSE_DN press 엣지 (트리거 FSM 공급) */
    uint8_t  up_edge;            /* 1-shot: SENSE_UP press 엣지 */
```

`app_weld_fsm.c` static 추가 + init 리셋:

```c
static uint8_t  s_run_mode;         /* 사이클 시작 시 래치 (모드 혼합 방지, slice4) */
static uint8_t  s_dn_pressed;       /* legacy re_dn_pressed 재현 (내부 래치) */
static uint8_t  s_up_pressed;       /* legacy re_up_pressed 재현 */
```

`weld_fsm_step` 초입(abort 처리 뒤, temp_time 감소 앞)에서 엣지 → 래치:

```c
    if (in->dn_edge) { s_dn_pressed = 1u; }
    if (in->up_edge) { s_up_pressed = 1u; }
```

상태별 수정:

```c
    case WELD_READY:
        if (in->start) {
            s_run_status     = WELD_CYL1;
            s_run_mode       = in->run_mode;          /* 사이클 단위 래치 */
            s_temp_time      = in->limit_delay_time1; /* TRIGGER에선 미사용 (아래 분기) */
            s_f_status_start = 0u;
            s_dn_pressed     = 0u;                    /* stale 클리어 (main.c:1478) */
        }
        break;

    case WELD_CYL1:
        if (s_f_status_start == 0u) { ...기존 SOL ON... }
        else if (s_run_mode != 0u) {                  /* TRIGGER (main.c:1513-1528) */
            if (s_dn_pressed != 0u) {
                s_dn_pressed = 0u;
                /* WELD 전이+래치+무장 — DELAY 분기와 동일 블록, ldt2 -> ltt2 치환 */
                s_f_status_start = 0u;
                s_run_status     = WELD_WELD;
                s_latched_multi  = in->multi_ctrl;
                s_latched_energy = in->energy_ctrl;
                s_multi_stage    = 0u;
                s_multi_elapsed  = 0u;
                s_comp_time = (in->limit_trigger_time2 > 6u) ? WELD_COMP_FULL
                                                             : (uint16_t)in->limit_trigger_time2;
                if (s_latched_energy) {
                    s_temp_time = weld_backstop_ticks(in->limit_out_time);
                } else {
                    s_temp_time = (in->limit_trigger_time2 > 6u) ? in->limit_trigger_time2
                                                                 : WELD_COMP_FULL;
                }
            }
        } else if (s_temp_time == 0u) { ...기존 DELAY 전이 (Task 1 형태)... }
        break;
```

> WELD 전이 블록이 DELAY/TRIGGER 두 곳에 중복되므로 **static helper로 추출**한다:
> `static void enter_weld(const weld_in_t *in, uint16_t weld_time)` — 호출측이
> `weld_time = ldt2 or ltt2`를 넘김 (DRY; 리뷰어가 두 분기 동일성을 한눈에 검증).

```c
    case WELD_HOLD:
        if (s_f_status_start == 0u) {
            s_f_status_start = 1u;
        } else if (s_temp_time == 0u) {
            s_f_status_start = 0u;
            s_run_status     = WELD_CYL2;
            s_temp_time      = in->limit_delay_time1;  /* DELAY용; TRIGGER는 미사용 */
            if (s_run_mode != 0u) {
                s_up_pressed = 1u;   /* legacy 강제 set (main.c:1593 "//-") — CYL2 즉시
                                      * exit. SENSE_UP 실대기 필요 판정 시 이 줄 제거 (spec §3.3). */
            }
        }
        break;

    case WELD_CYL2:
        if (s_f_status_start == 0u) { ...기존 SOL OFF... }
        else if (s_run_mode != 0u) {                  /* TRIGGER (main.c:1618-1629) */
            if (s_up_pressed != 0u) {
                s_up_pressed     = 0u;
                s_f_status_start = 0u;
                s_run_status     = WELD_READY;
                out->cycle_done  = 1u;
            }
        } else if (s_temp_time == 0u) { ...기존 DELAY 완료... }
        break;
```

HOLD 무장(WELD exit 시 `s_temp_time = in->limit_delay_time3`)을 `s_run_mode != 0u ?
in->limit_trigger_time3 : in->limit_delay_time3`으로 치환 — WELD exit 3곳(multi/energy/
시간) 공통이므로 helper `hold_time(in)` 또는 전이 지점 공통화로 1곳만 수정.
abort/init에서 `s_run_mode = 0u; s_dn_pressed = 0u; s_up_pressed = 0u;` 리셋 추가.

- [ ] **Step 4: 테스트 통과 확인** — `make -C fw/test test` → PASS (기존 DELAY 스위트 무회귀 포함).
- [ ] **Step 5: 빌드 확인** — 0-warning.
- [ ] **Step 6: 커밋**

```bash
git add fw/include/app_weld_fsm.h fw/src/app_weld_fsm.c fw/test/test_app_weld_fsm.c
git commit -m "feat(weld): TRIGGER 모드 — dn/up 엣지 래치·trigger_time2/3·CYL2 즉시 exit·run_mode 사이클 래치 (spec §3.2/§3.3)"
```

---

### Task 4: M1 — limit_energy=0 소비자 가드 (reg_calc + weld FSM)

**Files:**
- Modify: `fw/src/app_reg_calc.c:63-76`, `fw/src/app_weld_fsm.c` (energy exit 분기)
- Test: `fw/test/test_app_reg_calc.c`, `fw/test/test_app_weld_fsm.c`

**Interfaces:**
- Consumes: `reg_energy_termination(uint8_t, uint32_t, uint32_t, uint32_t, uint16_t)` (시그니처 무변경)
- Produces: `limit_energy==0`이면 에너지-도달 판정 skip (OVTIME/backstop은 계속 유효)

- [ ] **Step 1: 실패하는 테스트 작성** — `test_app_reg_calc.c`에:

```c
/* M1(감사): energy_ctrl=1 + limit_energy=0 -> 즉시 무증상 "정상완료" 금지.
 * 0 = 에너지-도달 체크 off (limit_out_time=0=OVTIME off 패턴과 동일 의미론). */
static void test_energy_limit_zero_no_instant_stop(void)
{
    CHECK_EQ(reg_energy_termination(1u, 0u, 0u, 0u, 10u), REG_RUN_CONTINUE);
    CHECK_EQ(reg_energy_termination(1u, 99999u, 0u, 0u, 10u), REG_RUN_CONTINUE);
    /* OVTIME은 계속 유효 */
    CHECK_EQ(reg_energy_termination(1u, 0u, 0u, 10000u, 10u), REG_RUN_FAULT_OVTIME);
}
```

`test_app_weld_fsm.c`에 (energy weld, limit 0 → 즉시 stop 없음, backstop abort로 종료):

```c
static void test_weld_energy_limit_zero_no_instant_stop(void)
{
    weld_fsm_init();
    weld_in_t in; memset(&in, 0, sizeof(in));
    in.limit_delay_time1 = 2u; in.limit_delay_time2 = 10u; in.limit_delay_time3 = 2u;
    in.output_power = 100u;
    in.energy_ctrl = 1u; in.limit_energy = 0u; in.curr_energy = 0u;
    in.limit_out_time = 1u;                   /* backstop 1s = 100 tick */
    weld_out_t out;
    in.start = 1u;
    weld_fsm_step(&in, &out);
    in.start = 0u;
    for (int i = 0; i < 3; i++) { weld_fsm_step(&in, &out); }
    CHECK_EQ(weld_fsm_status(), WELD_WELD);
    weld_fsm_step(&in, &out);
    CHECK_EQ(out.weld_stop, 0u);              /* 즉시 정상완료 금지 */
    int faults = 0;
    for (int i = 0; i < 150 && weld_fsm_status() == WELD_WELD; i++) {
        weld_fsm_step(&in, &out);
        faults += out.weld_fault;
    }
    CHECK_EQ(faults, 1);                      /* backstop abort로 종료 */
    CHECK_EQ(weld_fsm_status(), WELD_READY);
}
```

- [ ] **Step 2: 실패 확인** — reg_calc 테스트: `REG_RUN_STOP_ENERGY` 반환으로 FAIL. weld 테스트: 즉시 weld_stop=1로 FAIL.

- [ ] **Step 3: 최소 구현** — `app_reg_calc.c:68`:

```c
    if ((limit_energy != 0u) && (curr_energy >= limit_energy)) { return REG_RUN_STOP_ENERGY; }
    /* limit_energy==0 = 에너지-도달 체크 off (감사 M1; limit_out_time=0=OVTIME off와
     * 동일 의미론 — 0 목표의 즉시 무증상 완료 차단, 런은 OVTIME/30s 안전이 바운드). */
```

`app_weld_fsm.c` energy exit 분기(현 line 142):

```c
            if ((in->limit_energy != 0u) && (in->curr_energy >= in->limit_energy)) {
```

- [ ] **Step 4: 테스트 통과 확인** — `make -C fw/test test` → PASS.
- [ ] **Step 5: 빌드 확인** — 0-warning.
- [ ] **Step 6: 커밋**

```bash
git add fw/src/app_reg_calc.c fw/src/app_weld_fsm.c fw/test/test_app_reg_calc.c fw/test/test_app_weld_fsm.c
git commit -m "fix(weld): M1 — limit_energy=0을 에너지-도달 체크 off로 (즉시 무증상 완료 차단, 감사 D2)"
```

---

### Task 5: 트리거 FSM 신설 (`app_weld_trigger_fsm`)

**Files:**
- Create: `fw/include/app_weld_trigger_fsm.h`, `fw/src/app_weld_trigger_fsm.c`, `fw/test/test_app_weld_trigger_fsm.c`
- Modify: `fw/test/Makefile` (스위트 등록 + 헤더 주석 갱신 + `BIN_OL/BIN_IN/BIN_OSC` 중복 정의 3줄 제거 — 감사 부수 "첫 접촉 커밋 동승")

**Interfaces:**
- Consumes: `WELD_READY`/`WELD_CYL1` enum (`app_weld_fsm.h`)
- Produces (Task 7 글루가 사용):

```c
void weld_trigger_fsm_init(void);
void weld_trigger_fsm_step(const weld_trig_in_t *in, weld_trig_out_t *out);
void weld_trigger_fsm_cycle_started(void);   /* 게이팅 통과·사이클 실시작 시 글루가 호출 -> in_cycle=1 */
```

- [ ] **Step 1: 헤더 작성** — `fw/include/app_weld_trigger_fsm.h`:

```c
/* fw/include/app_weld_trigger_fsm.h — slice4 물리 트리거/센서 순수 FSM.
 * samd20 check_remote_input()의 weld 몫(SW_START1/2 양손 + in_cycle 재장전 +
 * f_safty CYL1 abort + SENSE_UP/DN 엣지) 분리. HAL-free, host-test.
 * 입력 = raw 레벨(active-LOW: 0=press/감지), 10ms tick마다 호출. */
#pragma once
#include <stdint.h>

typedef struct {
    uint8_t key1;        /* SW_START1 raw (PC12) */
    uint8_t key2;        /* SW_START2 raw (PB11) */
    uint8_t sens_up;     /* SENSE_UP raw (PA12) */
    uint8_t sens_dn;     /* SENSE_DN raw (PA11) */
    uint8_t f_safty;     /* cfg->f_safty */
    uint8_t weld_state;  /* weld_fsm_status() — READY/CYL1 판정용 */
} weld_trig_in_t;

typedef struct {
    uint8_t start_pulse;        /* 양손 press && !in_cycle — 레벨 파생 (게이팅/소비는 글루) */
    uint8_t safety_abort_pulse; /* f_safty && CYL1 && 한 손 release (조건 지속 동안 매 step) */
    uint8_t dn_edge;            /* SENSE_DN 1->0 엣지 1-shot */
    uint8_t up_edge;            /* SENSE_UP 1->0 엣지 1-shot */
} weld_trig_out_t;

void weld_trigger_fsm_init(void);
void weld_trigger_fsm_step(const weld_trig_in_t *in, weld_trig_out_t *out);
void weld_trigger_fsm_cycle_started(void);
```

- [ ] **Step 2: 실패하는 테스트 작성** — `fw/test/test_app_weld_trigger_fsm.c` (기존 CHECK_EQ 하네스 카피):

```c
/* fw/test/test_app_weld_trigger_fsm.c — slice4 트리거 FSM host tests. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "app_weld_trigger_fsm.h"
#include "app_weld_fsm.h"   /* WELD_* enums */

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

static weld_trig_in_t mk_in(uint8_t k1, uint8_t k2, uint8_t state)
{
    weld_trig_in_t in;
    memset(&in, 0, sizeof(in));
    in.key1 = k1; in.key2 = k2;
    in.sens_up = 1u; in.sens_dn = 1u;      /* idle = released(HIGH) */
    in.weld_state = state;
    return in;
}

/* 양손: 둘 다 press일 때만 start_pulse; 단독 press는 아님 (main.c:1404). */
static void test_two_hand_start(void)
{
    weld_trigger_fsm_init();
    weld_trig_out_t out;
    weld_trig_in_t in = mk_in(1u, 1u, WELD_READY);
    weld_trigger_fsm_step(&in, &out);  CHECK_EQ(out.start_pulse, 0u);
    in = mk_in(0u, 1u, WELD_READY);
    weld_trigger_fsm_step(&in, &out);  CHECK_EQ(out.start_pulse, 0u);
    in = mk_in(1u, 0u, WELD_READY);
    weld_trigger_fsm_step(&in, &out);  CHECK_EQ(out.start_pulse, 0u);
    in = mk_in(0u, 0u, WELD_READY);
    weld_trigger_fsm_step(&in, &out);  CHECK_EQ(out.start_pulse, 1u);
}

/* in_cycle 재장전: cycle_started 후 양손 유지 -> pulse 없음; READY에서 양손
 * release해야 재장전 (main.c:1219, 1472). 게이팅 실패(= cycle_started 미호출)면
 * 재장전 벌칙 없음. */
static void test_in_cycle_rearm(void)
{
    weld_trigger_fsm_init();
    weld_trig_out_t out;
    weld_trig_in_t in = mk_in(0u, 0u, WELD_READY);
    weld_trigger_fsm_step(&in, &out);
    CHECK_EQ(out.start_pulse, 1u);
    weld_trigger_fsm_cycle_started();          /* 글루: 게이팅 통과 */

    in.weld_state = WELD_CYL1;                 /* 사이클 진행 중 양손 유지 */
    weld_trigger_fsm_step(&in, &out);
    CHECK_EQ(out.start_pulse, 0u);

    in.weld_state = WELD_READY;                /* 완료 복귀, 양손 아직 유지 */
    weld_trigger_fsm_step(&in, &out);
    CHECK_EQ(out.start_pulse, 0u);             /* release 전 재시작 금지 */

    in = mk_in(1u, 1u, WELD_READY);            /* 양손 release -> 재장전 */
    weld_trigger_fsm_step(&in, &out);
    in = mk_in(0u, 0u, WELD_READY);
    weld_trigger_fsm_step(&in, &out);
    CHECK_EQ(out.start_pulse, 1u);

    /* 게이팅 실패 케이스: pulse만 나가고 cycle_started 미호출 -> 다음 step도 pulse */
    weld_trigger_fsm_step(&in, &out);
    CHECK_EQ(out.start_pulse, 1u);
}

/* safety abort: f_safty && CYL1 && 한 손 release (main.c:1484). */
static void test_safety_abort(void)
{
    weld_trigger_fsm_init();
    weld_trig_out_t out;
    weld_trig_in_t in = mk_in(1u, 0u, WELD_CYL1);
    in.f_safty = 1u;
    weld_trigger_fsm_step(&in, &out);  CHECK_EQ(out.safety_abort_pulse, 1u);
    in.f_safty = 0u;                                   /* safety off -> 없음 */
    weld_trigger_fsm_step(&in, &out);  CHECK_EQ(out.safety_abort_pulse, 0u);
    in.f_safty = 1u; in.key1 = 0u;                     /* 양손 유지 -> 없음 */
    weld_trigger_fsm_step(&in, &out);  CHECK_EQ(out.safety_abort_pulse, 0u);
    in.key1 = 1u; in.weld_state = WELD_WELD;           /* CYL1 밖 -> 없음 (legacy 충실) */
    weld_trigger_fsm_step(&in, &out);  CHECK_EQ(out.safety_abort_pulse, 0u);
}

/* 센서 엣지: 1->0 전이에서만 1-shot (main.c:1222-1233). */
static void test_sensor_edges(void)
{
    weld_trigger_fsm_init();
    weld_trig_out_t out;
    weld_trig_in_t in = mk_in(1u, 1u, WELD_READY);
    weld_trigger_fsm_step(&in, &out);
    CHECK_EQ(out.dn_edge, 0u);
    in.sens_dn = 0u;
    weld_trigger_fsm_step(&in, &out);  CHECK_EQ(out.dn_edge, 1u);
    weld_trigger_fsm_step(&in, &out);  CHECK_EQ(out.dn_edge, 0u);   /* 유지 = 재발행 없음 */
    in.sens_dn = 1u;
    weld_trigger_fsm_step(&in, &out);  CHECK_EQ(out.dn_edge, 0u);   /* release 무이벤트 */
    in.sens_up = 0u;
    weld_trigger_fsm_step(&in, &out);  CHECK_EQ(out.up_edge, 1u);
}

int main(void)
{
    test_two_hand_start();
    test_in_cycle_rearm();
    test_safety_abort();
    test_sensor_edges();
    if (failures) { printf("%d FAILURES\n", failures); return 1; }
    printf("app_weld_trigger_fsm: all tests passed\n");
    return 0;
}
```

- [ ] **Step 3: Makefile 등록 + 실패 확인** — `fw/test/Makefile`:
  - 헤더 주석의 스위트 목록 갱신(buzzer/overload/input/osc_init 누락분 + weld_trigger 추가 — 감사 부수 stale 주석 정정 동승).
  - **중복 3줄 삭제**: 두 번째 `BIN_OL :=`/`BIN_IN :=`/`BIN_OSC :=` 정의(17-19행 뒤 반복분).
  - 추가:

```make
BIN_WTR := /tmp/gds_test_app_weld_trigger_fsm
```

  - `test:` 의존/실행 목록에 `$(BIN_WTR)` 추가.
  - 규칙:

```make
$(BIN_WTR): test_app_weld_trigger_fsm.c ../src/app_weld_trigger_fsm.c ../include/app_weld_trigger_fsm.h ../include/app_weld_fsm.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_app_weld_trigger_fsm.c ../src/app_weld_trigger_fsm.c
```

  Run: `make -C fw/test test` → Expected: FAIL (`app_weld_trigger_fsm.c` 부재 컴파일 에러).

- [ ] **Step 4: 최소 구현** — `fw/src/app_weld_trigger_fsm.c`:

```c
/* fw/src/app_weld_trigger_fsm.c — slice4 물리 트리거/센서 순수 FSM.
 * samd20 check_remote_input() weld 몫 (main.c:1187-1268, 1404-1407, 1484). */
#include "app_weld_trigger_fsm.h"
#include "app_weld_fsm.h"   /* WELD_READY / WELD_CYL1 */

static uint8_t s_in_cycle;   /* main.c:1472 set / 1219 clear */
static uint8_t s_dn_bak;     /* 엣지 검출용 이전 레벨 (main.c re_dn_bak) */
static uint8_t s_up_bak;

void weld_trigger_fsm_init(void)
{
    s_in_cycle = 0u;
    s_dn_bak   = 1u;     /* idle = released(HIGH) */
    s_up_bak   = 1u;
}

void weld_trigger_fsm_cycle_started(void)
{
    /* 게이팅 통과·사이클 실시작 시에만 in_cycle 무장 — 게이팅에 막힌 트리거가
     * 양손 재장전을 강요하지 않도록 set 시점을 글루에 위임 (spec §2.3). */
    s_in_cycle = 1u;
}

void weld_trigger_fsm_step(const weld_trig_in_t *in, weld_trig_out_t *out)
{
    /* 양손 시작 (main.c:1404): 둘 다 press(0) && !in_cycle. 레벨 파생 —
     * in_cycle=0 동안 반복 발행, 소비는 글루(READY+게이팅). */
    out->start_pulse = ((in->key1 == 0u) && (in->key2 == 0u) &&
                        (s_in_cycle == 0u)) ? 1u : 0u;

    /* 재장전 (main.c:1219): 양손 release && READY 에서만 해제. */
    if ((in->key1 == 1u) && (in->key2 == 1u) &&
        (in->weld_state == (uint8_t)WELD_READY)) {
        s_in_cycle = 0u;
    }

    /* safety abort (main.c:1484): f_safty && CYL1 && 한 손이라도 release.
     * 조건 지속 동안 매 step 발행 — FSM abort가 CYL1을 벗어나며 자연 소멸. */
    out->safety_abort_pulse = ((in->f_safty != 0u) &&
                               (in->weld_state == (uint8_t)WELD_CYL1) &&
                               ((in->key1 == 1u) || (in->key2 == 1u))) ? 1u : 0u;

    /* 센서 press 엣지 (main.c:1222-1233 bak 패턴). */
    out->dn_edge = ((in->sens_dn == 0u) && (s_dn_bak == 1u)) ? 1u : 0u;
    out->up_edge = ((in->sens_up == 0u) && (s_up_bak == 1u)) ? 1u : 0u;
    s_dn_bak = in->sens_dn;
    s_up_bak = in->sens_up;
}
```

- [ ] **Step 5: 테스트 통과 확인** — `make -C fw/test test` → 13스위트 전부 PASS.
- [ ] **Step 6: 커밋**

```bash
git add fw/include/app_weld_trigger_fsm.h fw/src/app_weld_trigger_fsm.c fw/test/test_app_weld_trigger_fsm.c fw/test/Makefile
git commit -m "feat(weld): 순수 트리거 FSM 신설 — 양손 start/in_cycle 재장전/safety abort/센서 엣지 + host 스위트 (spec §2)"
```

---

### Task 6: `app_reg_start_allowed()` + START guard 공용화

**Files:**
- Modify: `fw/include/app_reg.h`, `fw/src/app_reg.c` (START guard 영역, 현 line ~127-166)

**Interfaces:**
- Produces: `bool app_reg_start_allowed(void)` — 읽기 전용, START가 지금 수락될 상태인지. Task 7 글루가 사용.

- [ ] **Step 1: 구현** — `app_reg.h`에 선언 추가 (+ 같은 파일 42행 부근 stale 주석
  "SEEK/RESET = no-op this slice (deferred, spec §9)"를 "SEEK/RESET = app_seek_reset로
  위임(D1 이후)"으로 정정 — 감사 부수 동승):

```c
/* START가 지금 수락될 상태인가 — guard와 동일 조건의 읽기 전용 쿼리 (상태 무변경).
 * slice4 weld 글루가 사이클 진입 게이팅에 사용 (블라인드 사이클 차단, spec §4.3).
 * swallow_start는 TOUCH 전용 소비라 조건에서 제외 (US_CYCLE에 무관). */
bool app_reg_start_allowed(void);
```

`app_reg.c` — START case 위쪽에 함수 정의:

```c
bool app_reg_start_allowed(void)
{
    return (g_reg.main_state == 0u) &&                     /* boot warm-up 완료 */
           (g_reg.us_run_status == (uint8_t)US_IDLE) &&
           (app_seek_reset_active() == 0u) &&
           (g_reg.error_status == 0u) &&
           (app_overload_active() == 0u) &&
           (app_estop_active() == 0u);
}
```

START guard의 4개 개별 break(`app_seek_reset_active`/`error_status`/`app_overload_active`/
`app_estop_active`, 현 line 148-166)를 **1개 호출로 치환** (swallow consume 블록은 그
앞에 그대로 유지 — 비대칭 보존):

```c
            /* guard 4-조건 공용화 (slice4): app_reg_start_allowed()가 단일 진실
             * 원천. 외측 if의 main_state/US_IDLE 재검사는 중복-참(무해).
             * swallow consume은 위에 유지 (advisor 비대칭 — spec §4.3). */
            if (!app_reg_start_allowed()) {
                break;
            }
```

기존 4개 break의 사유 주석(seek_reset 직교/fault/overload/estop)은 **함수 정의부로 이동**
— 삭제가 아니라 재배치(리뷰어가 조건-사유 대응을 유지 확인).

- [ ] **Step 2: 빌드 확인** — `env -u STM32_TOOLCHAIN cmake --build fw/build` → 0-warning.
  host 테스트 없음(app_reg는 HAL 결합 — 기존 관례) → cpp-review + HW 회귀가 게이트.
- [ ] **Step 3: 커밋**

```bash
git add fw/include/app_reg.h fw/src/app_reg.c
git commit -m "feat(reg): app_reg_start_allowed() 쿼리 신설 + START guard 4-break 공용화 (slice4 게이팅, spec §4.3)"
```

---

### Task 7: 글루 배선 — tick 정밀화 + SETUP 게이트 + 트리거/abort/신필드 주입

**Files:**
- Modify: `fw/src/app_weld.c`, `fw/src/app_lcd_input.c` (+`fw/include/app_lcd.h` 선언 1줄)

**Interfaces:**
- Consumes: Task 2~6의 산출물 전부 + `io_read_key1/key2/sens_up/sens_dn`(io.h) + `app_estop_active`(app_input.h) + `app_overload_active`(app_overload.h) + `app_reg_measure()->error_status`
- Produces: `uint8_t app_lcd_in_run_page(void)` (app_lcd_input.c — run_page_for_mode 재사용)

- [ ] **Step 1: `app_lcd_in_run_page()` 추가** — `app_lcd_input.c` (run_page_for_mode 아래):

```c
/* slice4 SETUP 게이트: 현재 run 페이지인가 (setup/model 페이지면 0).
 * samd20 sys_status==SYS_RUN 등가 — 페이지 기반 판정 (sys_status 필드는 미배선). */
uint8_t app_lcd_in_run_page(void)
{
    const lcd_app_state_t *st = app_lcd_state();
    return (st->lcd_status == run_page_for_mode(st->sys_mode)) ? 1u : 0u;
}
```

`app_lcd.h`의 함수 선언 블록에:

```c
uint8_t app_lcd_in_run_page(void);   /* 1 = run 페이지 (slice4 weld SETUP 게이트) */
```

- [ ] **Step 2: 글루 재작성** — `fw/src/app_weld.c`의 `app_weld_tick()` 교체 + include 추가
  (`app_weld_trigger_fsm.h`, `app_input.h`, `app_overload.h`; `app_weld_init()`에
  `weld_trigger_fsm_init();` 추가). 파일 헤더 주석의 "No SETUP gate this slice" /
  "slice 4 caller" stale 서술 갱신:

```c
void app_weld_tick(void)
{
    uint32_t now = sys_tick_get_ms();
    if ((uint32_t)(now - s_prev_ms) < WELD_TICK_MS) {
        return;
    }
    /* 감사 M1(글루): += 로 드리프트 무누적 (실 공압 dwell 정밀도, samd20 timer ISR
     * 10ms 주기 재현). 장기 정지(>10 tick 밀림) 후엔 재동기 — catch-up 폭주 방지. */
    s_prev_ms += WELD_TICK_MS;
    if ((uint32_t)(now - s_prev_ms) > 10u * WELD_TICK_MS) {
        s_prev_ms = now;
    }

    /* SETUP 게이트 (slice1 spec §5.4 이연분): setup 페이지에선 step 스킵 =
     * 사이클 타이머 동결 + 시작 불가 (samd20 timer의 sys_status!=SETUP 게이트). */
    if (app_lcd_in_run_page() == 0u) {
        return;
    }

    app_config_t *cfg = app_lcd_cfg();

    /* 물리 트리거/센서 스캔 (slice4). */
    weld_trig_in_t tin = {
        .key1       = io_read_key1(),
        .key2       = io_read_key2(),
        .sens_up    = io_read_sens_up(),
        .sens_dn    = io_read_sens_dn(),
        .f_safty    = cfg->f_safty,
        .weld_state = weld_fsm_status(),
    };
    weld_trig_out_t tout;
    weld_trigger_fsm_step(&tin, &tout);

    /* 사이클 진입 게이팅 (spec §4.3, 의도된 deviation): US START가 거부될 상태면
     * 사이클 자체를 시작하지 않음 — SOL만 하강하는 블라인드 사이클 차단. */
    if ((tout.start_pulse != 0u) &&
        (weld_fsm_status() == (uint8_t)WELD_READY) &&
        app_reg_start_allowed()) {
        s_start_pending = 1u;
        weld_trigger_fsm_cycle_started();
    }

    /* abort 합성 (spec §3.4/§4.2): E-stop/overload/fault + f_safty CYL1 release.
     * US 정지는 slice-c/d force-stop과 이중 안전. */
    uint8_t abort_now =
        ((app_estop_active() != 0u) ||
         (app_overload_active() != 0u) ||
         (app_reg_measure()->error_status != 0u) ||
         (tout.safety_abort_pulse != 0u)) ? 1u : 0u;

    /* M2(감사 D2): mo_out cast 전 [50,100] 클램프 — 상류(LCD/Modbus) 클램프와
     * belt-and-braces (uint16->uint8 절단 silent 진폭0 차단). */
    uint16_t mo1 = cfg->limit_mo_out1, mo2 = cfg->limit_mo_out2;
    if (mo1 > 100u) { mo1 = 100u; } else if (mo1 < 50u) { mo1 = 50u; }
    if (mo2 > 100u) { mo2 = 100u; } else if (mo2 < 50u) { mo2 = 50u; }

    weld_in_t in = {
        .start               = s_start_pending,
        .run_mode            = cfg->run_mode,
        .limit_delay_time1   = cfg->limit_delay_time1,
        .limit_delay_time2   = cfg->limit_delay_time2,
        .limit_delay_time3   = cfg->limit_delay_time3,
        .limit_trigger_time2 = cfg->limit_trigger_time2,
        .limit_trigger_time3 = cfg->limit_trigger_time3,
        .output_power        = cfg->output_power,
        .energy_ctrl         = cfg->energy_ctrl ? 1u : 0u,
        .limit_energy        = cfg->limit_energy,
        .limit_out_time      = cfg->limit_out_time,
        .curr_energy         = app_reg_measure()->curr_energy,
        .multi_ctrl          = cfg->multi_ctrl ? 1u : 0u,
        .limit_mo_out1       = (uint8_t)mo1,
        .limit_mo_out2       = (uint8_t)mo2,
        .limit_mo_time1      = cfg->limit_mo_time1,
        .limit_mo_time2      = cfg->limit_mo_time2,
        .dn_edge             = tout.dn_edge,
        .up_edge             = tout.up_edge,
        .abort               = abort_now,
    };
    s_start_pending = 0u;

    ...이하 기존 out-이벤트 디스패치 무수정 (weld_fsm_step / sol edge / weld_start /
       amp_change / weld_stop / weld_fault / cycle_done)...
}
```

> ⚠ `weld_in_t`의 `limit_mo_out1/2` 타입이 uint8_t(기존)이므로 캐스트 유지 — 클램프가
> 절단을 무해화. 기존 out-이벤트 디스패치 블록(94-115행)은 그대로 둔다.

- [ ] **Step 3: 빌드 확인** — **reconfigure 필수**(Task 5에서 신규 .c 추가됨):
  `env -u STM32_TOOLCHAIN cmake -S fw -B fw/build -G Ninja && env -u STM32_TOOLCHAIN cmake --build fw/build` → 0-warning.
- [ ] **Step 4: host 무회귀 확인** — `make -C fw/test test` → PASS (글루 자체는 host 미대상 — 기존 관례, cpp-review + HW가 게이트).
- [ ] **Step 5: 커밋**

```bash
git add fw/src/app_weld.c fw/src/app_lcd_input.c fw/include/app_lcd.h
git commit -m "feat(weld): 글루 배선 — 물리 트리거 스캔+진입 게이팅+abort 합성+SETUP 게이트+tick 드리프트 무누적 (spec §4)"
```

---

### Task 8: M4 — LCD LV_* 필드 클램프 (`cfg_clamp.h` + 적용 + host test)

**Files:**
- Create: `fw/include/cfg_clamp.h`
- Modify: `fw/src/app_lcd_input.c:745-797` (LV_* case들)
- Test: `fw/test/test_app_config.c` (순수 헬퍼 검증 추가)

**Interfaces:**
- Produces: `cfg_clamp_max(uint16_t v, uint16_t max)` / `cfg_clamp_power(uint16_t v)` (static inline, 공용 헤더)

- [ ] **Step 1: 실패하는 테스트 작성** — `test_app_config.c`에 `#include "cfg_clamp.h"` +:

```c
/* M4(감사 D2): config-validation 클램프 헬퍼 — Modbus apply_writes 범위와 동일. */
static void test_cfg_clamp_helpers(void)
{
    CHECK_EQ(cfg_clamp_max(501u, 500u), 500u);
    CHECK_EQ(cfg_clamp_max(500u, 500u), 500u);
    CHECK_EQ(cfg_clamp_max(0u, 500u), 0u);
    CHECK_EQ(cfg_clamp_power(101u), 100u);
    CHECK_EQ(cfg_clamp_power(49u), 50u);    /* LOW-1: <50 진폭 언더플로 차단 */
    CHECK_EQ(cfg_clamp_power(0u), 50u);
    CHECK_EQ(cfg_clamp_power(75u), 75u);
}
```

- [ ] **Step 2: 실패 확인** — `make -C fw/test test` → 컴파일 에러(cfg_clamp.h 부재). Expected: FAIL.

- [ ] **Step 3: 헤더 구현** — `fw/include/cfg_clamp.h`:

```c
/* fw/include/cfg_clamp.h — config-validation 클램프 (감사 D2/M4). 순수 인라인,
 * LCD 터치 경로가 사용 (Modbus apply_writes의 인라인 클램프와 동일 범위 유지 —
 * 범위 변경 시 양쪽 함께). host-test: test_app_config. */
#pragma once
#include <stdint.h>

static inline uint16_t cfg_clamp_max(uint16_t v, uint16_t max)
{
    return (v > max) ? max : v;
}

static inline uint16_t cfg_clamp_power(uint16_t v)   /* [50,100] — 진폭 언더플로 가드 */
{
    if (v > 100u) { return 100u; }
    if (v < 50u)  { return 50u; }
    return v;
}
```

- [ ] **Step 4: LCD 적용** — `app_lcd_input.c`에 `#include "cfg_clamp.h"` + 클램프-에코
  로컬 헬퍼(기존 LV_MO_TIME 에코 관례 준수):

```c
/* M4: 클램프 + 패널 에코 (클램프 발동 시에만 재전송 — LV_MO_TIME 관례). */
static uint16_t clamp_echo_max(uint16_t vp, uint16_t v, uint16_t max)
{
    uint16_t c = cfg_clamp_max(v, max);
    if (c != v) { dgus_write_u16(vp, c); }
    return c;
}
static uint16_t clamp_echo_power(uint16_t vp, uint16_t v)
{
    uint16_t c = cfg_clamp_power(v);
    if (c != v) { dgus_write_u16(vp, c); }
    return c;
}
```

case들 치환 (spec §6.1 표의 범위):

```c
    case LV_DM_DELAY:
        cfg->limit_delay_time1 = clamp_echo_max(LV_DM_DELAY, data16, 500u);
        break;
    case LV_DM_WELD:
        cfg->limit_delay_time2 = clamp_echo_max(LV_DM_WELD, data16, 500u);
        break;
    case LV_DM_HOLD:
        cfg->limit_delay_time3 = clamp_echo_max(LV_DM_HOLD, data16, 2000u);
        break;
    case LV_TM_WELD:
        cfg->limit_trigger_time2 = clamp_echo_max(LV_TM_WELD, data16, 500u);
        break;
    case LV_TM_HOLD:
        cfg->limit_trigger_time3 = clamp_echo_max(LV_TM_HOLD, data16, 2000u);
        break;
    case LV_MO_OUT1:
        cfg->limit_mo_out1 = clamp_echo_power(LV_MO_OUT1, data16);
        break;
    case LV_MO_OUT2:
        cfg->limit_mo_out2 = clamp_echo_power(LV_MO_OUT2, data16);
        break;
    case LV_OUT_POWER:
        /* output_power ONLY — NO DAC here (기존 주석 유지). M4+LOW-1: [50,100]. */
        cfg->output_power = (uint8_t)clamp_echo_power(LV_OUT_POWER, data16);
        break;
    case LV_MAX_ON_TIME:
        cfg->limit_on_time = clamp_echo_max(LV_MAX_ON_TIME, data16, 2000u);
        break;
    case LV_LIMIT_OUT_T:
        cfg->limit_out_time = clamp_echo_max(LV_LIMIT_OUT_T, data16, 10u);
        break;
```

(`LV_ENERGY_EDIT`은 무클램프 유지 — Modbus 동일, M1은 Task 4 소비자 가드가 해소.)

- [ ] **Step 5: 테스트+빌드 확인** — `make -C fw/test test` PASS + 빌드 0-warning.
- [ ] **Step 6: 커밋**

```bash
git add fw/include/cfg_clamp.h fw/src/app_lcd_input.c fw/test/test_app_config.c
git commit -m "fix(lcd): M4 — LV_* 필드군 Modbus-동일 클램프+에코 (LOW-1 OUT_POWER 포함, 감사 D2)"
```

---

### Task 9: M3 — FRAM 로드 comm idx 클램프 + 매크로 승격

**Files:**
- Modify: `fw/include/app_config.h`, `fw/src/app_config.c:130-131`, `fw/src/app_lcd_input.c:62-63`
- Test: `fw/test/test_app_config.c`

**Interfaces:**
- Produces: `CFG_COMM_SPEED_IDX_MAX`(5)/`CFG_COMM_PARITY_IDX_MAX`(2) — `app_config.h` 공용 매크로

- [ ] **Step 1: 실패하는 테스트 작성** — `test_app_config.c` (mock-fram 스위트 관례 사용:
  기존 테스트가 mock fram에 값 심고 `app_config_load` 검증하는 패턴 그대로):

```c
/* M3(감사 D2): FRAM 로드 comm idx OOB -> factory(0) 클램프 (렌더 테이블 OOB read 차단). */
static void test_load_comm_idx_oob_clamped(void)
{
    app_config_t cfg;
    mock_fram_reset();
    mock_fram_poke(FRAM_ADDR_INIT_FLAG, FRAM_INIT_FLAG_MAGIC);   /* 초기화된 FRAM */
    mock_fram_poke(FRAM_ADDR_COMM_SPEED, 6u);     /* > CFG_COMM_SPEED_IDX_MAX(5) */
    mock_fram_poke(FRAM_ADDR_COMM_PARITY, 3u);    /* > CFG_COMM_PARITY_IDX_MAX(2) */
    (void)app_config_load(&cfg);
    CHECK_EQ(cfg.comm_speed_idx, 0u);             /* factory 기본 */
    CHECK_EQ(cfg.comm_parity_idx, 0u);
}
```

(mock API = `mock_fram.h`: `mock_fram_reset`/`mock_fram_poke` — 기존 스위트와 동일 관례.)

- [ ] **Step 2: 실패 확인** — 매크로 부재 컴파일 에러 또는 idx=6 통과로 FAIL.

- [ ] **Step 3: 구현** — `app_config.h`:

```c
/* comm 렌더 테이블 인덱스 상한 (감사 M3 클램프): app_lcd_str.c
 * comm_speed_txt[6][6] / comm_parity_txt[3][4] 크기와 동기 — 테이블 확장 시 함께. */
#define CFG_COMM_SPEED_IDX_MAX   5u
#define CFG_COMM_PARITY_IDX_MAX  2u
```

`app_config.c` 로드 2줄 교체:

```c
    if (!fram_read_byte(FRAM_ADDR_COMM_SPEED,  &cfg->comm_speed_idx))   { fail++; }
    else if (cfg->comm_speed_idx > CFG_COMM_SPEED_IDX_MAX)  { cfg->comm_speed_idx = 0u; }  /* M3: OOB -> factory */
    if (!fram_read_byte(FRAM_ADDR_COMM_PARITY, &cfg->comm_parity_idx))  { fail++; }
    else if (cfg->comm_parity_idx > CFG_COMM_PARITY_IDX_MAX) { cfg->comm_parity_idx = 0u; } /* M3: OOB -> factory */
```

`app_lcd_input.c:62-63` 로컬 define을 공용 매크로 별칭으로 교체 (사용처 diff 최소화):

```c
#define COMM_SPEED_IDX_MAX   CFG_COMM_SPEED_IDX_MAX   /* app_config.h 공용 (M3) */
#define COMM_PARITY_IDX_MAX  CFG_COMM_PARITY_IDX_MAX
```

- [ ] **Step 4: 테스트+빌드 확인** — `make -C fw/test test` PASS + 빌드 0-warning.
- [ ] **Step 5: 커밋**

```bash
git add fw/include/app_config.h fw/src/app_config.c fw/src/app_lcd_input.c fw/test/test_app_config.c
git commit -m "fix(config): M3 — FRAM 로드 comm speed/parity idx OOB를 factory로 클램프 + 상한 매크로 공용 승격 (감사 D2)"
```

---

### Task 10: 최종 통합 — 풀 게이트 + 통합 리뷰 + docs

**Files:**
- Modify: `docs/changelog.md`, `docs/NEXT_STEPS.md`(§1.2 slice4 항목 상태), `docs/superpowers/RESUME.md`, `HANDOFF.md`

- [ ] **Step 1: 풀 게이트** — 클린 reconfigure 빌드(0-warning, FLASH/RAM 기록) + `make -C fw/test test` 13스위트 PASS.
- [ ] **Step 2: 통합 cpp-review** — cpp-reviewer 에이전트로 슬라이스 전체 diff
  (`git diff feat/output-power-graph-ch1...HEAD -- fw/`) 리뷰. Critical/Important 0까지
  수정 반영 (기존 관례: Task별 리뷰는 subagent-driven 실행 시 2-stage로 이미 수행).
- [ ] **Step 3: spec 대조** — spec §1.1 In 9항목 각각 구현 커밋 존재 확인; deviation
  (게이팅/M1 0=off/CYL2 즉시)이 코드 주석에 legacy 라인 인용과 함께 남았는지 확인.
- [ ] **Step 4: docs 갱신** — changelog(슬라이스4 CODE-COMPLETE 항목), NEXT_STEPS §1.2
  (weld 슬라이스4 → CODE-COMPLETE 미머지·HW 게이트, D2/D4 종결 표기), RESUME/HANDOFF
  세션 블록. **머지/태그 없음** — HW 게이트(스택 b'→d'→ch1'→slice4 순).
- [ ] **Step 5: 커밋**

```bash
git add docs/ HANDOFF.md
git commit -m "docs: weld 슬라이스4 CODE-COMPLETE — 감사 D2/D4 종결, 머지/태그=스택 HW 게이트"
```

---

## HW 검증 (플랜 밖 — 다음 HW 세션, spec §7.3)

패널 rig에서 spec §7.3의 8항목. 검증 규칙 = mbpoll/LCD 육안만(SWD halt 금지),
브랜치 전환 시 reconfigure. 머지 순서 = b'→d'→ch1'→slice4, 단위별 `--no-ff`+태그.
