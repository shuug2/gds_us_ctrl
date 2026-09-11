# Modbus FC06 쓰기 → LCD VP 에코 구현 계획

> **문서 요약**: spec `2026-09-11-modbus-write-lcd-echo-design.md` 를 5개 Task 로 나눈 실행 계획. Task 1 = `apply_writes` SETUP 숫자 13분기에 `dgus_write_u16/u32` 14줄(기존 3개 에코와 동형) → Task 2 = `render_run_std` 를 `app_lcd_run_std_refresh()` 로 승격 + `if (save)` 뒤 1호출 → Task 3 = HORN/CAL/MODEL 에코 5줄(spec §3.3 전부 포함 권고, 컨트롤러 판정 확인 후) → Task 4 = changelog/requirements → Task 5 = 빌드 날짜 + HW 벤치 체크리스트(사용자가 보드 앞에서 수행). 코드 커밋마다 STD/REMOTE 경고 0 · host 17 PASS · `size` 기록. **바이너리가 바뀌는 첫 커밋** — `.bin` 동일 게이트는 적용되지 않고 게이트는 HW 벤치다.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 원격기가 FC06 으로 쓴 SETUP 값이 컨트롤러 LCD 가 그 페이지를 표시 중일 때 **즉시** 화면에 나타나게 한다(STD RUN 페이지 텍스트 포함). 레지스터 계약은 무변경.

**Architecture:** `fw/src/app_modbus.c` `app_modbus_apply_writes()` 의 각 cfg 분기가 cfg 대입 직후 자기 VP 하나를 `dgus_write_u16` 로 쓴다(이미 있는 `DISP_ENERGY_EN/MULTI_EN/SAFTY` 3개와 동형). STD RUN 페이지의 조합 텍스트(`D/W(E)/H`)는 기존 렌더 `render_run_std` 를 공개 함수로 승격해 `if (save)` 뒤 한 번 부른다 — 텍스트 규칙을 두 번 쓰지 않는다. 페이지 게이트 없음(DGUS VP RAM 이 페이지 밖 쓰기를 보존하고, 경고→런 복귀가 `set_page` 만 하므로).

**Tech Stack:** STM32F410 / arm-none-eabi-gcc 15.2.1 / CMake+Ninja(`./fw.sh`, `MODEL=remote ./fw.sh`) / host 테스트 `./fw.sh test`(17 스위트) / 벤치 `docs/superpowers/tools/mb_tcp.py`·`mb_hold.py`

**Spec:** `docs/superpowers/specs/2026-09-11-modbus-write-lcd-echo-design.md` (구현 정본). 배경 조사 = `docs/superpowers/research/2026-09-11-setup-sync-investigation.md` §A·§C.

## Global Constraints

- 작업 트리 = `/Users/tknoh/dev/work/gds_us_ctrl/.claude/worktrees/feat-modbus-write-lcd-echo`, 브랜치 `feat/modbus-write-lcd-echo`(`refactor/byte-identical` tip `3f82c06` 위). 모든 명령은 이 디렉터리에서.
- 수정 파일은 **`fw/src/app_modbus.c` · `fw/src/app_lcd_render.c` · `fw/include/app_lcd.h` · `fw/include/define.h`(날짜) · `docs/changelog.md` · `docs/requirements.md`** 만. `fw/vendor/`·`ref/` 편집 금지. 다른 파일에 diff 가 생기면 리젝트.
- 에코는 **분기 안, cfg 대입 다음 줄, `save = true;` 앞**. `#include` 추가 없음(`dgus_lcd.h` :26 · `app_lcd.h` :13 · `app_horn.h` :18 이미 있음). 값 변환(÷10·×100) 넣지 않는다 — 어느 렌더에도 없다.
- `mirror_live()` ~ `apply_writes` 사이에 cfg 를 쓰는 코드를 넣지 않는다(`app_modbus.c:759-763` 불변식). 에코는 cfg 를 읽기만 한다.
- 빌드 게이트(코드 커밋마다): `./fw.sh` + `MODEL=remote ./fw.sh` 둘 다 our-code **경고 0**, `./fw.sh test` 종료코드 0, `arm-none-eabi-size` 출력을 커밋 메시지에 기록.
- 커밋 메시지 끝 트레일러 2줄:
  `Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>`
  `Claude-Session: https://claude.ai/code/session_01FLb4papJVFASVXee4FQmPA`
- 함정: `$STM32_TOOLCHAIN` stale → `fw.sh` 가 `env -u` 로 피함(수동 cmake 시 직접). 소스 파일 추가 없음이라 GLOB 재구성 불필요.
- HW 벤치는 **사용자가 보드 앞에서** 수행(Task 5). 벤치 PASS 전 원격기 통보·계약 문서 갱신 금지.

---

## 파일 구조

| 파일 | 책임 | Task |
|---|---|---|
| `fw/src/app_modbus.c` :453-520 (수정) | SETUP 숫자 13분기 에코 14줄 | 1 |
| `fw/src/app_lcd_render.c` :41-93, :171, :179 (수정) · `fw/include/app_lcd.h` :163 부근 (수정) | `render_run_std` → `app_lcd_run_std_refresh()` 승격 + 선언 | 2 |
| `fw/src/app_modbus.c` :621-626 (수정) | `if (save)` 뒤 `app_lcd_run_std_refresh()` 1줄 | 2 |
| `fw/src/app_modbus.c` :559 / :568 / :584 / :589 / :592 (수정) | HORN 1 · MODEL 2 · CAL 2 | 3 |
| `docs/changelog.md` `[Unreleased]` · `docs/requirements.md` FW3 (수정) | 기록 | 4 |
| `fw/include/define.h` :73/:75/:78 (수정) | 빌드 날짜 `_260906` → `_260911` (20자 유지) | 5 |
| (벤치 세션 신설) `docs/superpowers/plans/2026-09-11-modbus-write-lcd-echo-bench-results.md` | 벤치 결과 | 5 |

---

### Task 1: SETUP 숫자 13분기 LCD VP 에코

**Files:**
- Modify: `fw/src/app_modbus.c:453-520`

**Interfaces:** 신규 없음. `dgus_write_u16/u32`(`fw/include/dgus_lcd.h:160-161`) 와 VP 상수(`LV_DM_DELAY/LV_DM_WELD/LV_DM_HOLD/LV_TM_WELD/LV_TM_HOLD` :62-66, `LV_OUT_POWER/LV_MAX_ON_TIME/LV_ENERGY_VAL/LV_LIMIT_OUT_T` :106-113, `LV_ENERGY_EDIT` :61, `LV_MO_*` :115-118) 만 쓴다.

- [ ] **Step 1: 현 코드 확인 (before)**

Run: `sed -n 453,520p fw/src/app_modbus.c`
Expected: 13개 `else if (g_mb.holding[MB_REG_…] != cfg->…)` 분기, 각각 `cfg->… = v;` (또는 직접 대입) 뒤 바로 `save = true;`. 에코 줄 없음.

- [ ] **Step 2: 13분기에 에코 14줄 추가**

`fw/src/app_modbus.c:453-520` 을 아래로 교체(변경 = 각 분기의 `save = true;` 바로 앞 1줄, ENERGY 만 2줄. 주석·클램프·대입은 원문 그대로):

```c
    } else if (g_mb.holding[MB_REG_DELAY1] != cfg->limit_delay_time1) {
        v = g_mb.holding[MB_REG_DELAY1];
        if (v > 500u) { v = 500u; }
        cfg->limit_delay_time1 = v;
        dgus_write_u16(LV_DM_DELAY, cfg->limit_delay_time1);   /* LCD 에코 — SETUP_STD2D/RUN_STD (spec 2026-09-11 §3.1) */
        save = true;
    } else if (g_mb.holding[MB_REG_DELAY2] != cfg->limit_delay_time2) {
        v = g_mb.holding[MB_REG_DELAY2];
        if (v > 500u) { v = 500u; }
        cfg->limit_delay_time2 = v;
        dgus_write_u16(LV_DM_WELD, cfg->limit_delay_time2);
        save = true;
    } else if (g_mb.holding[MB_REG_DELAY3] != cfg->limit_delay_time3) {
        v = g_mb.holding[MB_REG_DELAY3];
        if (v > 2000u) { v = 2000u; }
        cfg->limit_delay_time3 = v;     /* samd20 saved this to ADDR_TRIGGER2 —
                                         * copy-paste bug, fixed by save_all */
        dgus_write_u16(LV_DM_HOLD, cfg->limit_delay_time3);
        save = true;
    } else if (g_mb.holding[MB_REG_TRIGGER2] != cfg->limit_trigger_time2) {
        v = g_mb.holding[MB_REG_TRIGGER2];
        if (v > 500u) { v = 500u; }
        cfg->limit_trigger_time2 = v;   /* samd20 saved to ADDR_DELAY2 — ditto */
        dgus_write_u16(LV_TM_WELD, cfg->limit_trigger_time2);
        save = true;
    } else if (g_mb.holding[MB_REG_TRIGGER3] != cfg->limit_trigger_time3) {
        v = g_mb.holding[MB_REG_TRIGGER3];
        if (v > 2000u) { v = 2000u; }
        cfg->limit_trigger_time3 = v;
        dgus_write_u16(LV_TM_HOLD, cfg->limit_trigger_time3);
        save = true;
    } else if (g_mb.holding[MB_REG_OUT_POWER] != cfg->output_power) {
        v = g_mb.holding[MB_REG_OUT_POWER];
        if (v > 100u) { v = 100u; }
        else if (v < 50u) { v = 50u; }
        cfg->output_power = (uint8_t)v;
        dgus_write_u16(LV_OUT_POWER, cfg->output_power);   /* 표시만 — pot 은 START/SAVE/페이지 진입 때 (spec §2.2) */
        save = true;
    } else if (g_mb.holding[MB_REG_ON_TIME] != cfg->limit_on_time) {
        v = g_mb.holding[MB_REG_ON_TIME];
        if (v > 2000u) { v = 2000u; }
        cfg->limit_on_time = v;
        dgus_write_u16(LV_MAX_ON_TIME, cfg->limit_on_time);
        save = true;
    } else if (g_mb.holding[MB_REG_ENERGY] != (uint16_t)cfg->limit_energy) {
        cfg->limit_energy = (uint32_t)g_mb.holding[MB_REG_ENERGY];
        dgus_write_u32(LV_ENERGY_VAL,  cfg->limit_energy);             /* render_setup_main :102-103 과 동형 */
        dgus_write_u16(LV_ENERGY_EDIT, (uint16_t)cfg->limit_energy);
        save = true;
    } else if (g_mb.holding[MB_REG_MULTI_T1] != cfg->limit_mo_time1) {
        v = g_mb.holding[MB_REG_MULTI_T1];
        if (v > 2000u) { v = 2000u; }
        cfg->limit_mo_time1 = v;
        dgus_write_u16(LV_MO_TIME1, cfg->limit_mo_time1);
        save = true;
    } else if (g_mb.holding[MB_REG_MULTI_T2] != cfg->limit_mo_time2) {
        v = g_mb.holding[MB_REG_MULTI_T2];
        if (v > 2000u) { v = 2000u; }
        cfg->limit_mo_time2 = v;
        dgus_write_u16(LV_MO_TIME2, cfg->limit_mo_time2);
        save = true;
    } else if (g_mb.holding[MB_REG_MULTI_O1] != cfg->limit_mo_out1) {
        v = g_mb.holding[MB_REG_MULTI_O1];
        if (v > 100u) { v = 100u; }
        else if (v < 50u) { v = 50u; }
        cfg->limit_mo_out1 = v;
        dgus_write_u16(LV_MO_OUT1, cfg->limit_mo_out1);
        save = true;
    } else if (g_mb.holding[MB_REG_MULTI_O2] != cfg->limit_mo_out2) {
        v = g_mb.holding[MB_REG_MULTI_O2];
        if (v > 100u) { v = 100u; }
        else if (v < 50u) { v = 50u; }
        cfg->limit_mo_out2 = v;
        dgus_write_u16(LV_MO_OUT2, cfg->limit_mo_out2);
        save = true;
    } else if (g_mb.holding[MB_REG_TIMEOVER] != cfg->limit_out_time) {
        v = g_mb.holding[MB_REG_TIMEOVER];
        if (v > 10u) { v = 10u; }
        cfg->limit_out_time = v;        /* samd20 wrote the clamp back into the
                                         * reg; our per-tick mirror does that */
        dgus_write_u16(LV_LIMIT_OUT_T, cfg->limit_out_time);
        save = true;
```

- [ ] **Step 3: diff 가 에코 줄만인지 확인**

Run: `git diff --stat && git diff fw/src/app_modbus.c | grep '^[+-]' | grep -v '^+++\|^---' | grep -vc 'dgus_write_u\(16\|32\)('`
Expected: `fw/src/app_modbus.c | 14 +` (1 file), 두 번째 명령 출력 `0`(에코 이외의 추가/삭제 줄 없음).

- [ ] **Step 4: STD 빌드 — 경고 0**

Run: `./fw.sh 2>&1 | tee /tmp/lcd-echo-std.log | tail -3; grep -c 'warning:' /tmp/lcd-echo-std.log`
Expected: ninja 완료 줄(`[N/N] Generating .bin / .hex / size` 포함), `grep -c` → `0`.

- [ ] **Step 5: REMOTE 빌드 — 경고 0**

Run: `MODEL=remote ./fw.sh 2>&1 | tee /tmp/lcd-echo-rem.log | tail -3; grep -c 'warning:' /tmp/lcd-echo-rem.log`
Expected: 동일, `0`.

- [ ] **Step 6: host 17 스위트**

Run: `./fw.sh test; echo "exit=$?"`
Expected: 17개 바이너리 출력 후 `exit=0`. (건드린 파일이 전부 HAL 글루라 결과 무변경.)

- [ ] **Step 7: FLASH 기록**

Run: `arm-none-eabi-size fw/build/gds_us_ctrl.elf fw/build-remote/gds_us_ctrl.elf`
Expected: `text` 가 기준(`3f82c06` 클린 빌드 STD 66,696 B / REMOTE 67,008 B `.bin` 크기 근사) 대비 **+120 ~ +180 B**. 수치를 커밋 본문에 적는다.

- [ ] **Step 8: 커밋**

```bash
git add fw/src/app_modbus.c
git commit -m "feat(modbus): FC06 SETUP 숫자 13분기 LCD VP 에코 — 기존 DISP_*_EN 에코와 동형 (spec 2026-09-11 §3.1)

- DELAY1/2/3 → LV_DM_DELAY/WELD/HOLD · TRIGGER2/3 → LV_TM_WELD/HOLD · OUT_POWER → LV_OUT_POWER
  · ON_TIME → LV_MAX_ON_TIME · ENERGY → LV_ENERGY_VAL(u32)+LV_ENERGY_EDIT(u16) · MULTI_T1/T2/O1/O2
  → LV_MO_TIME1/2·OUT1/2 · TIMEOVER → LV_LIMIT_OUT_T. 분기 안, cfg 대입 직후, 클램프 뒤 값.
- 의도적 legacy 이탈: samd20 update_holding_reg(1) 은 3곳만 에코(ref/samd20/main.c:4520/4530/4536).
  레지스터 의미·주소·클램프·저장 무변경. 원격 제약 추가 아님(LCD 표시를 cfg 에 맞춤).
- 추가 지연 u16 8 B ≈ 0.69 ms / ENERGY 18 B ≈ 1.56 ms (115200). host 17 PASS · STD/REMOTE 경고 0
  · size STD text=… / REMOTE text=… (+… B). HW 벤치 게이트 — plan Task 5.

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01FLb4papJVFASVXee4FQmPA"
```

---

### Task 2: STD RUN 페이지 텍스트 재기록 — `app_lcd_run_std_refresh()` 승격 + apply 뒤 1호출

**Files:**
- Modify: `fw/src/app_lcd_render.c:39-93` (함수 승격), `:171` (`buf` 삭제), `:178-179` (호출 교체)
- Modify: `fw/include/app_lcd.h:163` 다음 줄 (선언)
- Modify: `fw/src/app_modbus.c:621-626` (`if (save)` 블록)

**Interfaces:**
- Produces: `void app_lcd_run_std_refresh(void);` — STD RUN 페이지의 `LV_DM_DELAY/DISP_RUN_MODE/DISP_SAFTY/LV_LIMIT_OUT_T` + `DISP_STD_DATA1~3` 를 cfg 로 재기록. **`dgus_set_page` 없음.** `app_lcd_change_page` 와 `app_modbus_apply_writes` 가 공유.

- [ ] **Step 1: before 확인**

Run: `sed -n 39,48p fw/src/app_lcd_render.c; sed -n 167,181p fw/src/app_lcd_render.c`
Expected: `static inline __attribute__((always_inline)) void render_run_std(const app_config_t *cfg, uint8_t *buf)` :41, `uint8_t n;` :43, `uint8_t  buf[20];` :171, `render_run_std(cfg, buf);` :179.

- [ ] **Step 2: 함수 승격 (render.c)**

`fw/src/app_lcd_render.c:39-43` 을 교체(본체 :44-92 는 **그대로**, 닫는 중괄호 :93 유지):

```c
/* STD RUN 페이지 텍스트·수치 재기록 — LCD_RUN_STD: DELAY/TRIGGER 별 D/W(E)/H 3줄 텍스트
 * + 수치 4필드(LV_DM_DELAY/DISP_RUN_MODE/DISP_SAFTY/LV_LIMIT_OUT_T). set_page 는 하지 않는다.
 * 호출처 = app_lcd_change_page(페이지 진입) + app_modbus_apply_writes(FC06 cfg 저장 뒤,
 * spec 2026-09-11 §3.2). 페이지 게이트 없음 — 데이터/텍스트 VP 는 페이지 밖에서 써도 VP RAM 에
 * 남고, 경고→런 복귀 5곳(app_lcd_input.c:103/126/130/151/266)이 set_page 만 하므로 게이트가
 * 있으면 경고 중 도착한 원격 쓰기가 복귀 뒤 stale 로 남는다. */
void app_lcd_run_std_refresh(void)
{
    const app_config_t *cfg = app_lcd_cfg();
    uint8_t buf[20];            /* line-build scratch (samd20 global lcd_temp_buf) */
    uint8_t n;                  /* formatter return length (samd20 'temp') */
```

`fw/src/app_lcd_render.c:171` `    uint8_t  buf[20];           /* line-build scratch (samd20 global lcd_temp_buf) */` 줄을 **삭제**(미사용 → `-Wall` 경고).

`fw/src/app_lcd_render.c:179` `        render_run_std(cfg, buf);` → `        app_lcd_run_std_refresh();`

- [ ] **Step 3: 선언 (app_lcd.h)**

`fw/include/app_lcd.h:163` `void app_lcd_change_page(uint8_t page);               /* render + set_page (spec §6) */` 다음 줄에 추가:

```c
void app_lcd_run_std_refresh(void);                   /* STD RUN 텍스트·수치 재기록, set_page 없음 (Modbus 에코 공용) */
```

- [ ] **Step 4: apply 뒤 1호출 (app_modbus.c)**

`fw/src/app_modbus.c:621-626` 을 교체:

```c
    if (save) {
        /* Whole-map FRAM commit — codebase pattern (data_save_commit).
         * ~2 ms at 400 kHz nominal; the 50 ms/call I2C timeout governs the
         * worst case (bus hang). Same budget as the LCD DATA_SAVE path. */
        app_config_save_all(cfg);
        /* STD RUN 페이지(9) 텍스트 D/W(E)/H·RUN_MODE 배지는 cfg 여러 필드의 함수라 항목별 에코로
         * 못 맞춘다 — 페이지 렌더를 한 번 재사용한다(spec 2026-09-11 §3.2). ≤82 B ≈ 7.1 ms.
         * 페이지 무관(VP RAM). hold 워치독 적층 579 → 587.7 ms < 600 (spec §4.3). */
        app_lcd_run_std_refresh();
    }
```

- [ ] **Step 5: diff 범위 확인**

Run: `git diff --stat`
Expected: `fw/include/app_lcd.h | 1 +`, `fw/src/app_lcd_render.c | ≈12 ±`, `fw/src/app_modbus.c | 4 +`. 다른 파일 없음.

- [ ] **Step 6: 빌드 2종 + 경고 0 + host**

Run: `./fw.sh 2>&1 | grep -c 'warning:'; MODEL=remote ./fw.sh 2>&1 | grep -c 'warning:'; ./fw.sh test; echo "exit=$?"`
Expected: `0` · `0` · `exit=0`. (경고가 나면 원인은 render.c:171 `buf` 미삭제 — `unused variable 'buf'`.)

- [ ] **Step 7: FLASH 기록**

Run: `arm-none-eabi-size fw/build/gds_us_ctrl.elf fw/build-remote/gds_us_ctrl.elf`
Expected: Task 1 대비 **+20 ~ +60 B**(always_inline 해제로 본체는 한 벌, `bl` 2개 + 프롤로그). +300 B 이상이면 본체가 두 벌 — Step 2 에서 `static inline` 을 지웠는지 확인.

- [ ] **Step 8: 커밋**

```bash
git add fw/src/app_lcd_render.c fw/include/app_lcd.h fw/src/app_modbus.c
git commit -m "feat(lcd): STD RUN 텍스트 재기록 공개 함수 app_lcd_run_std_refresh + FC06 cfg 저장 뒤 1호출 (spec 2026-09-11 §3.2)

- render_run_std(static always_inline) → app_lcd_run_std_refresh(void): cfg=app_lcd_cfg(), buf 지역화,
  set_page 없음. change_page 는 호출만 교체, 미사용 buf 제거. 본체 무변경.
- apply_writes if(save) 뒤 1호출 — D/W(E)/H·DISP_RUN_MODE 는 run_mode×energy_ctrl×multi_ctrl 조합이라
  항목별 에코 불가. 페이지 게이트 없음(VP RAM 보존 + 경고→런 복귀가 set_page 만).
- 추가 지연 ≤82 B ≈ 7.1 ms/apply, ENERGY 와 합쳐 최악 100 B ≈ 8.7 ms → hold 적층 579→587.7 < 600.
- host 17 PASS · STD/REMOTE 경고 0 · size STD text=… / REMOTE text=… (+… B).

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01FLb4papJVFASVXee4FQmPA"
```

---

### Task 3: HORN / CAL / MODEL 에코 (컨트롤러 판정 후)

**전제:** spec §3.3 (a)(b)(c) 의 "포함" 권고를 컨트롤러 세션(사용자)이 확인했다. 한 항목만 제외되면 그 줄만 빼고 나머지는 진행(분기 독립). 전부 제외면 이 Task 는 건너뛴다.

**Files:**
- Modify: `fw/src/app_modbus.c:559` (HORN), `:568` (MODEL_FREQ), `:584` (MODEL_TYPE), `:589` (CAL), `:592` (FREQ_CAL) — Task 1·2 로 줄 번호가 **+18** 이동해 있다(`grep -n` 으로 다시 잡을 것).

- [ ] **Step 1: 자리 확인**

Run: `grep -n 'app_horn_set_mode(g_mb\|app_lcd_send_model_str(cfg->model_freq, cfg->model_type);\|cfg->cal_val = cfg_cal_from_wire\|cfg->freq_cal_val = cfg_cal_from_wire' fw/src/app_modbus.c`
Expected: 5줄(send_model_str 2회).

- [ ] **Step 2: 5줄 추가**

(a) `app_horn_set_mode(g_mb.holding[MB_REG_HORN_CMD] != 0u);` **다음 줄**:
```c
        dgus_write_u16(DISP_HORNDOWN, (uint16_t)app_horn_mode_active());   /* SETUP1 체크박스 — 진입 시드 app_lcd_input.c:458 와 동형. temp_horndown(shadow)은 안 건드린다 */
```

(c) MODEL_FREQ 분기의 `app_lcd_send_model_str(cfg->model_freq, cfg->model_type);` **다음 줄**:
```c
        dgus_write_u16(MODEL_FREQ, cfg->model_freq);   /* MODEL_SETUP 선택 VP — enter_model_setup :326 와 동형 */
```

(c) MODEL_TYPE 분기의 `app_lcd_send_model_str(...)` **다음 줄**:
```c
        dgus_write_u16(MODEL_TYPE, cfg->model_type);   /* :327 와 동형. PC11 의미 변경(위 주석)과 무관 */
```

(b) `cfg->cal_val = cfg_cal_from_wire(...)` **다음 줄**:
```c
        dgus_write_u16(VAR_CAL_VAL, (uint16_t)cfg->cal_val);   /* MODEL_SETUP — raw int16 캐스트, enter_model_setup :328 와 동형 (÷100 은 DGUS 자산 몫) */
```

(b) `cfg->freq_cal_val = cfg_cal_from_wire(...)` **다음 줄**:
```c
        dgus_write_u16(VAR_FREQ_CAL_VAL, (uint16_t)cfg->freq_cal_val);
```

- [ ] **Step 3: diff 확인**

Run: `git diff fw/src/app_modbus.c | grep '^+' | grep -v '^+++' | wc -l`
Expected: `5`(포함 항목 수만큼).

- [ ] **Step 4: 빌드 2종 + host + size**

Run: `./fw.sh 2>&1 | grep -c 'warning:'; MODEL=remote ./fw.sh 2>&1 | grep -c 'warning:'; ./fw.sh test; echo "exit=$?"; arm-none-eabi-size fw/build/gds_us_ctrl.elf fw/build-remote/gds_us_ctrl.elf`
Expected: `0` · `0` · `exit=0` · Task 2 대비 **+40 ~ +70 B**.

- [ ] **Step 5: 커밋**

```bash
git add fw/src/app_modbus.c
git commit -m "feat(modbus): HORN_CMD → DISP_HORNDOWN · CAL/FREQ_CAL → VAR_*CAL_VAL · MODEL_FREQ/TYPE → 선택 VP 에코 (spec 2026-09-11 §3.3)

- HORN: SETUP1 체크박스를 실제 모드로 — 진입 시드(app_lcd_input.c:458)와 동형. shadow temp_horndown 무변경,
  SAVE 시 shadow 적용 legacy 거동 그대로(2026-09-06 벤치 §4-5). 원격기 CLAUDE.md '컨트롤러 LCD horn 에코 부재' 해소.
- CAL: raw int16 캐스트(enter_model_setup :328-329 동형), 클램프 뒤 값. MODEL: 선택 VP(:326-327 동형),
  sys_mode·런페이지 미재파생·PC11 가드 없음은 기존 결정 그대로.
- host 17 PASS · STD/REMOTE 경고 0 · size STD text=… / REMOTE text=… (+… B).

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01FLb4papJVFASVXee4FQmPA"
```

---

### Task 4: 문서 — changelog `[Unreleased]` + requirements FW3

**Files:**
- Modify: `docs/changelog.md` (`## [Unreleased]` 바로 아래, 2026-09-09 항목 **위**에 삽입)
- Modify: `docs/requirements.md:98` (FW3 항목 7 다음에 8 추가)

- [ ] **Step 1: changelog 항목 삽입**

`docs/changelog.md` 의 `## [Unreleased]` 다음 빈 줄 뒤에 추가:

```markdown
### 2026-09-11 — feat(modbus/lcd): FC06 쓰기 → LCD VP 에코 — SETUP 숫자 13분기 + STD RUN 텍스트 + HORN/CAL/MODEL — HW 벤치 대기

- **무엇**: 원격기가 FC06 으로 SETUP 값을 쓰면 컨트롤러 LCD 가 그 페이지에 머무는 동안 옛 숫자를 보이던 것(조사 `research/2026-09-11-setup-sync-investigation.md` §A)을 고쳤다. `apply_writes` 의 cfg 분기마다 기존 3개 에코(`DISP_ENERGY_EN/MULTI_EN/SAFTY`)와 **동형**으로 `dgus_write_u16(VP, cfg->…)` 1줄: DELAY1/2/3 · TRIGGER2/3 · OUT_POWER · ON_TIME · ENERGY(u32+u16) · MULTI_T1/T2/O1/O2 · TIMEOVER(14 VP 쓰기). STD RUN 페이지(9)의 `D/W(E)/H`·RUN_MODE 배지는 `render_run_std` 를 공개 함수 `app_lcd_run_std_refresh()` 로 승격해 `if (save)` 뒤 1회 재기록(set_page 없음, 페이지 게이트 없음). HORN_CMD → `DISP_HORNDOWN`, CAL → `VAR_*CAL_VAL`, MODEL → 선택 VP 도 에코.
- **사용자 결정(방법 A)**: 페이지 재렌더(C-2)·주기 재쓰기(C-3) 기각. LCD 편집 중 충돌은 범위 밖. staged comm(`0x1E~0x29`) 은 COMM 페이지가 shadow 기반이라 **제외**.
- **의도적 legacy 이탈**: samd20 `update_holding_reg(1)` 은 3곳만 에코(`ref/samd20/main.c:4520/4530/4536`). "LCD 에 없는 규칙을 원격에만 발명 금지" 원칙과 무관 — 원격 제약이 아니라 LCD 표시를 cfg 에 맞추는 것.
- **계약 무변경**: 레지스터 주소·의미·클램프·저장·CAP 전부 그대로. 원격기 조치 없음(벤치 PASS 후 통보).
- **지연**: apply 1회 최악 100 B ≈ 8.7 ms(115200) → hold 워치독 적층 579 → **587.7 ms < 600**. UART wedge 시 프레임당 10 ms 캡은 `change_page` 와 같은 급.
- **FLASH** ≈ +… B(실측: STD text … / REMOTE text …). host 17 PASS · STD/REMOTE 경고 0. **HW 벤치 대기** — 항목 = spec §5.2 E-0~E-14·R-1~R-5, 결과는 `plans/2026-09-11-modbus-write-lcd-echo-bench-results.md`. 통과 시 태그 `hw-revA_fw-stage-lcd-echo`.
- 관찰(범위 밖, 무수정): `LV_ENERGY_EDIT` 부팅 시드가 `/10`(`app_lcd.c:240`)이고 페이지 렌더는 raw(`render.c:103`) — 기존 불일치. spec = `specs/2026-09-11-modbus-write-lcd-echo-design.md`, plan = `plans/2026-09-11-modbus-write-lcd-echo.md`.
```

(`…` 3곳은 Task 1~3 의 `size` 실측으로 채운다 — 플레이스홀더로 커밋하지 않는다.)

- [ ] **Step 2: requirements FW3 항목 8**

`docs/requirements.md:98` (항목 7 "원격 hold-to-run 워치독" 줄) 다음에 추가:

```markdown
8. **Modbus FC06 쓰기 → LCD VP 에코** (2026-09-11, 방법 A) — 원격이 쓴 SETUP 값·STD RUN 텍스트·HORN 체크박스·CAL/MODEL 선택이 LCD 표시 중 즉시 갱신. 레지스터 계약 무변경, staged comm 제외. 설계 `docs/superpowers/specs/2026-09-11-modbus-write-lcd-echo-design.md`. **HW 벤치 대기**(spec §5.2).
```

- [ ] **Step 3: 확인 + 커밋**

Run: `git diff --stat`
Expected: `docs/changelog.md`, `docs/requirements.md` 두 파일만.

```bash
git add docs/changelog.md docs/requirements.md
git commit -m "docs: FC06 → LCD VP 에코 코드-완료 기록 — changelog [Unreleased] + requirements FW3 항목 8

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01FLb4papJVFASVXee4FQmPA"
```

---

### Task 5: 빌드 날짜 + HW 벤치 체크리스트 (벤치는 사용자가 보드 앞에서)

**Files:**
- Modify: `fw/include/define.h:73, 75, 78` (`_260906` → `_260911`, 각 문자열 **20자 유지** — `app_lcd_render.c:37` `_Static_assert` 가 지킨다)
- Create (벤치 세션): `docs/superpowers/plans/2026-09-11-modbus-write-lcd-echo-bench-results.md`

- [ ] **Step 1: 날짜 갱신 + 커밋 (관례: 날짜 = 그 빌드)**

Run: `sed -i '' 's/_260906/_260911/g' fw/include/define.h && grep -n '_2609' fw/include/define.h && MODEL=remote ./fw.sh 2>&1 | grep -c 'warning:'`
Expected: 3줄 `V3.1.0R!_260911     ` / `V3.1.0R_260911      ` / `V3.0.0_260911       ` · `0`(길이가 바뀌면 `_Static_assert` 컴파일 에러).

```bash
git add fw/include/define.h
git commit -m "chore(version): 빌드 날짜 260911 — LCD 에코 벤치 빌드 (STD V3.0.0_260911 / REMOTE V3.1.0R!_260911)

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01FLb4papJVFASVXee4FQmPA"
```

- [ ] **Step 2: 플래시 (REMOTE 빌드 — 보드 현 모델)**

Run: `MODEL=remote ./fw.sh flash 2>&1 | grep -E 'Verified OK|Resetting Target'`
Expected: `** Verified OK **` + `** Resetting Target **`. 부팅 ≈4 s 뒤 LCD 버전 `V3.1.0R!_260911` 육안. ⚠ 부팅 warm-up 중 START 는 무음 거부(2026-09-06 벤치 §4-2) — E-14 는 부팅 10 s 후.

- [ ] **Step 3: 벤치 환경 규칙 (위반 시 항목 무효 — `plans/2026-09-05-bench-results.md` §4)**

- 🔴 `nc -z` **금지**(구 펌웨어 소켓 영구 고착). 🔴 TCP connect ≠ MCU 생존 → 생존은 FC03. 🔴 `mbpoll` 동작 불가 → `mb_tcp.py` 만.
- 침묵 >10 s 뒤 첫 프레임은 게이트가 버린다(`REMOTE_EN` `DIS_LINK`) → 세션 시작마다 `m.read(0, 51)` 1회 프라임. 소켓 1개 → 파이썬 세션 1개만.
- SWD halt 금지. **LCD SAVE 는 항목이 요구할 때만**(SETUP 저장 = horn down 재전송). 보드 현 상태: REMOTE `V3.1.0R!`, `ETH_STATIC 192.168.1.199`, 게이트 열림(극성 반전), cal 16/40(사용자 트림, 원복 금지 — E-13 뒤 반드시 16 으로).

- [ ] **Step 4: 세션 시작 — 스냅샷**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl/.claude/worktrees/feat-modbus-write-lcd-echo
python3 -i -c "
import sys, time; sys.path.insert(0, 'docs/superpowers/tools')
from mb_tcp import MB
m = MB().connect()
snap = m.read(0, 51); print('snap', snap)
open('/tmp/lcd-echo-snap.txt', 'w').write(repr(snap))
"
```
Expected: 51개 정수. `snap[0x31] == 0xFA01`, `snap[0x32] == 1`, `snap[0x2B] == 1`(게이트 열림). 이후 항목은 이 `python3 -i` 프롬프트에서 `m.write(...)` 로 실행.

- [ ] **Step 5: 항목 실행 (spec §5.2 표 — 아래는 조작 순서·기대 육안)**

| # | LCD 를 이 페이지에 두고 | 프롬프트 입력 | 기대(육안 / read-back) | 판정 |
|---|---|---|---|---|
| E-1 | SETUP_STD1(10): 런 페이지에서 SETUP 터치 | `m.write(0x06, 80)` | POWER 숫자 즉시 **80** / `80` | ⬜ |
| E-2 | STD1 | `m.write(0x07, 700)` | ON TIME **700** / `700` | ⬜ |
| E-3 | STD1 | `m.write(0x09, 5)` | LIMIT(OUT_T) **5** / `5` | ⬜ |
| E-4 | STD1 | `m.write(0x08, 1234)` | ENERGY 표시 **1234**(값·편집칸 둘) / `1234`. ⚠ 부팅 시드 `/10` 불일치(spec R4)는 기록만 | ⬜ |
| E-5 | STD2D(12): STD1 에서 페이지2(run_mode=DELAY) | `m.write(0x0A, 123); m.write(0x0B, 234); m.write(0x0C, 345)` | D/W/H 3칸 즉시 123/234/345 | ⬜ |
| E-5b | STD2D | `m.write(0x0A, 600)` | read-back **500**, LCD D 칸 **500** | ⬜ |
| E-6 | STD2T(13): E-9 뒤 또는 LCD RUN_MODE 터치로 TRIGGER | `m.write(0x0D, 222); m.write(0x0E, 333)` | W/H 222/333 | ⬜ |
| E-7 | STD3(15) | `m.write(0x0F, 100); m.write(0x10, 200); m.write(0x11, 60); m.write(0x12, 70)` | T1/T2/O1/O2 즉시 갱신 | ⬜ |
| E-8 | **RUN_STD(9)**: LCD CANCEL 로 복귀(SAVE 아님) | `m.write(0x0A, 150)` | `D : 1.50` 줄 + 상단 DELAY 숫자 150 즉시 | ⬜ |
| E-9 | RUN_STD | `m.write(0x13, 1)` → 관찰 → `m.write(0x13, 0)` | 1: `SENSOR OFF` / `W : <TRIGGER2>` / `H : <TRIGGER3>` + 배지 TRIGGER · 0: `D :`/`W : <DELAY2>`/`H : <DELAY3>` 복귀 | ⬜ |
| E-10 | RUN_STD | `m.write(0x14, 1)` → `m.write(0x14, 0)` | 2행 `E : <energy>` ↔ `W :` 전환 + ENERGY 아이콘 | ⬜ |
| E-11 | STD1 | `m.write(0x30, 1); hex(m.r1(0x1D) & 0x40)` → `m.write(0x30, 0)` | 체크박스 ☑, `0x40` → ☐. **SAVE 누르지 말고 CANCEL 로 나감** | ⬜ |
| E-12 | STD2D, D 칸 키패드 **열어 둔 채** | `m.write(0x0A, 111)` | 기록만(입력란 덮임? 확정 후 값?) — 범위 밖 | 📝 |
| E-13 | MODEL_SETUP(1): `SETUP_MODEL` 2 s 롱프레스 | `m.write(0x2E, 20); m.write(0x17, 2)` → (간이 벤치 = E-stop 미배선 확인) `m.write(0x18, 1); m.write(0x18, 2)` | CAL **20** · 주파수 선택 30 kHz + 모델명 `GDS-30…` · 타입 선택 이동 후 복귀. 끝나면 `m.write(0x2E, 16); m.write(0x17, 3)` 즉시 원복 | ⬜ |
| E-14(선택) | 부팅 10 s 후, RUN_STD | `m.write(0x14,1); m.write(0x09,1); m.write(0x1B,1)` → 1.5 s 후 `hex(m.r1(0x1D))` 에 `0x8` → 경고 페이지에서 `m.write(0x0A, 175)` → LCD 에러 RESET 키 | 런 복귀 뒤 `D : 1.75`(새 값) — 페이지 게이트 미채택 근거. 끝나면 `m.write(0x14,0); m.write(0x09, snap[9])` | ⬜ |
| R-1 | 각 E 뒤 | `[i for i,(a,b) in enumerate(zip(m.read(0,51), snap)) if a!=b]` | 이번에 쓴 칸 + 측정값(0x02~0x05)·STATUS(0x1D)만 | ⬜ |
| R-2 | — | 전원 재인가 → 새 `m = MB().connect(); m.r1(0x06)` | `80`(FRAM 영속) | ⬜ |
| R-3 | RUN_STD | 별 터미널(프롬프트 종료 후, 소켓 1개): `python3 docs/superpowers/tools/mb_hold.py 192.168.1.199 150 2` 15 s → Ctrl-C | keep 중 `US=1` 연속, 손 뗌 후 `+5xx ms US=0`(≈600) | ⬜ |
| R-4 | RUN_STD | `for _ in range(5): t=time.monotonic(); m.write(0x08, 1500); print(round((time.monotonic()-t)*1000,1))` | 5회 전부 **< 50 ms** | ⬜ |
| R-5 | STD2D | LCD 터치로 D 칸 = 321 확정 → `m.r1(0x0A)` | `321`(방향 ② 무변경) | ⬜ |
| X | — | `for a in [0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0x10,0x11,0x12,0x13,0x14,0x17,0x18,0x2E,0x2F,0x30]: m.write(a, snap[a])` → `m.read(0,51)[6:0x19] == snap[6:0x19]` | `True`. LCD 는 CANCEL 로 런 페이지. horn 0 | ⬜ |

- [ ] **Step 6: 결과 문서 + 마감**

벤치 세션이 `docs/superpowers/plans/2026-09-11-modbus-write-lcd-echo-bench-results.md` 를 신설(첫머리 요약 단락 · 항목별 PASS/FAIL 표 · E-12 관찰 · R-4 실측 ms · 보드 마감 상태). FAIL 항목이 위젯 무반응(spec R1)이면 **그 분기의 에코 1줄만 되돌리고** 나머지 유지(분기 독립).

전건 PASS 시(사용자 실행):
```bash
git checkout main && git merge --no-ff feat/modbus-write-lcd-echo -m "merge: feat/modbus-write-lcd-echo — FC06 → LCD VP 에코 (HW 벤치 PASS)"
git tag hw-revA_fw-stage-lcd-echo
```
그 뒤 원격기 세션에 spec §5.3 문구로 **통보**(계약 무변경 — 원격기 CLAUDE.md horn 에코 부재 항목 닫기 요청 포함). `CLAUDE.md`/`HANDOFF.md`/`NEXT_STEPS.md` 갱신은 세션 마감 절차에서.
