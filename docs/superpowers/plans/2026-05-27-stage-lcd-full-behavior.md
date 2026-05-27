# Stage LCD — Full LCD Behavior Port Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port the entire samd20 LCD operational behavior to STM32 on top of the Stage B wire/FRAM layers — panel touch/key **input** (page nav, setup editing, save/cancel to FRAM), per-page **rendering** (`change_lcd_page`), and the periodic **live-value display** (output/time bars, power/amp/freq/energy, status icons). Hardware/control coupling (ultrasonic command, output DAC, Modbus/Ethernet reconfigure, horn) is routed through **named stub hooks** = Stage C/D integration points. Measurement sources are **stubbed** (bars/values read a provider that returns 0 until Stage D).

**Architecture:** Behavior layer above the existing `usart1 → dgus_lcd (wire) → app` stack and `fram → app_config` persistence. New files split by responsibility (<800 lines each): `app_lcd_str` (formatters), `app_lcd_render` (change_lcd_page), `app_lcd_input` (parse dispatch + save/cancel), `app_lcd_disp` (display step-machine + icons). `app_lcd.c` owns subsystem state + top-level dispatch/tick + stub hooks. Wire functions (`dgus_write_*`, `dgus_set_page`, `dgus_rx_poll`) and FRAM primitives (`fram_write_*`) are reused unchanged.

**Tech Stack:** C11, STM32F4 HAL, arm-none-eabi-gcc, CMake+Ninja. DGUS over USART1 (PA9/PA10, 115200). FRAM over I2C1 (slave 0x50).

**Spec:** `docs/superpowers/specs/2026-05-27-stage-lcd-full-behavior-port-design.md` — the §6 page-render table, §7 VP→action table, §8 save/cancel discipline, §9 VP macros, §11 display machine are the authoritative detail this plan references rather than re-pasting.

**Port source (read-only, authoritative):** `ref/samd20/main.c` (`parse_lcd_comm`, `change_lcd_page`, `lcd_var_init`, display job-state, formatters), `ref/samd20/dgus_lcd.h`, `ref/samd20/define.h`.

---

## Verification model (read first — embedded gate, board now on bench)

- **GREEN gate (per task):** `cd fw && env -u STM32_TOOLCHAIN cmake -B build -G Ninja && env -u STM32_TOOLCHAIN cmake --build build` → **build succeeds, 0 warnings** (`-Wall -Wextra -Wundef -Wshadow`). CMake globs `src/*.c drivers/*.c`, so new files require the `-B build` re-configure.
- **Logic correctness:** spec-compliance review per task (subagent-driven-development reviewer) + the static trace named in each task. VP addresses/payloads, byte order, clamp limits, and the FRAM save sets are review targets — cross-checked against the spec tables and samd20 source.
- **HW verification:** the hw-revA board is **available** (BOOT0/LCD HW-verified 2026-05-26). Real bench verification is **Task 10** (spec §13), not deferred.

**Commit policy:** attribution disabled (user `~/.claude/settings.json`) — no Co-Authored-By trailer. Conventional commits.

**Quirk fidelity (do NOT "fix"):** F3 HAND-save→MULTI page, F4 cancel skips comm_mode/ether restore — reproduce verbatim, annotate with `/* samd20 quirk Fn — verbatim, HW-verify only */`. Preserve Stage B cold-boot fixes (`dgus_wait_ready`/`app_lcd_ensure_run_page`) — no regression.

---

## File Structure

| File | Action | Responsibility |
|---|---|---|
| `fw/include/dgus_lcd.h` | modify | add full VP/key macro set (spec §9) |
| `fw/include/app_lcd.h` | modify | `lcd_app_state_t`, `lcd_measure_t`, `us_cmd_t`, hook + dispatch + render + disp + formatter prototypes |
| `fw/src/app_lcd.c` | modify | subsystem state owner, stub hooks, `app_lcd_tick`, refactor `init_mode`→`change_lcd_page` |
| `fw/src/app_lcd_str.c` | create | `time2str`/`energy2str`/`conv_addr2str`/`ip_to_string`/`lcd_data_pdd` |
| `fw/src/app_lcd_render.c` | create | `app_lcd_change_page()` port (spec §6) |
| `fw/src/app_lcd_input.c` | create | `app_lcd_input_dispatch()` (spec §7) + DATA_SAVE/CANCEL (spec §8) |
| `fw/src/app_lcd_disp.c` | create | display step-machine + bar fills + ICON/work_cnt/error-page helpers (spec §11) |
| `fw/include/app_config.h`, `fw/src/app_config.c` | modify | `app_config_save_all()` (commit live cfg → FRAM) |
| `fw/src/app.c` | modify | `app_loop_iter`: dispatch RX to input layer + drive `app_lcd_tick`; keep Stage B boot |
| `fw/include/mon.h` (if needed) | — | reuse existing `mon_printf` for hook traces |

All `src/*.c` auto-globbed; no manual CMake entry (no new HAL module this stage).

---

## Task 1: Full VP / key macro set

**Files:** modify `fw/include/dgus_lcd.h`

- [ ] **Step 1:** Add every VP/key macro samd20 uses that is not already present (spec §9), values **verbatim from `ref/samd20/dgus_lcd.h`**, 16-bit form (`0x0014`, not `0x14`). Group under a `/* Full LCD behavior VPs (samd20 verbatim) */` block after the existing Stage B VPs. Include: page IDs `LCD_LOGO`/`LCD_MODEL_SETUP`/`LCD_WARNING`; VPs `STD_SETUP_PARAM`,`DATA_SAVE`,`MODEL_FREQ`,`MODEL_TYPE`,`COMM_ADDR/SPEED/PARITY`,`COMM_ADDR_TXT`/`COMM_SPEED_TXT`/`COMM_PARITY_TXT`,`COMM_IP_TXT`/`COMM_NM_TXT`/`COMM_GW_TXT`,`KEY_MULTI`,`SETUP_MODEL`,`ENERGY_EN`,`SETUP_PARAM`,`SETUP_PARAM_MOOHAN`,`STD_SETUP_PARAM`,`VAR_AMP`/`VAR_FREQ`/`VAR_ENERGY`,`ICON_OL`/`ICON_OUTERR`,`LV_OUTPUT2`,`LV_TIME`/`LV_TIME2`,`LV_OUT_POWER`,`LV_ENERGY_VAL`,`LV_MAX_ON_TIME`,`DISP_MULTI`,`DISP_RUN_MODE`,`DISP_SAFTY`,`DISP_REMOTE`,`LV_LIMIT_OUT_T`,`LV_RUN_MODE`,`LV_MO_OUT1/2`,`LV_MO_TIME1/2`,`M_DATA_SAVE`,`VP_ERROR_MSG`,`ICON_ERROR_RESET`,`KEY_ERROR_RESET`,`MULTI_EN`,`DISP_STD_DATA1/2/3`,`DISP_VERSION`,`LV_COMM_MODE`,`LV_ETHER_KEY`,`DISP_COMM_MODE`,`DISP_EN_DHCP`,`VAR_CAL_VAL`,`VAR_FREQ_CAL_VAL`; key codes `KEY_UP=1`,`KEY_DN=0`,`KEY_CANCEL=0`,`KEY_SAVE=1`. (Page-ID macros `LCD_RUN_*`/`LCD_SETUP_*` already present — verify, don't duplicate.)
- [ ] **Step 2: build green** (unused macros emit no warning).
- [ ] **Step 3: commit** — `feat(lcd): add full LCD behavior VP/key macro set`

Static trace: every macro value matches `ref/samd20/dgus_lcd.h`; no collision with existing Stage B defines.

---

## Task 2: `app_config_save_all()` — commit live cfg → FRAM

**Files:** modify `fw/include/app_config.h`, `fw/src/app_config.c`

- [ ] **Step 1:** Declare `void app_config_save_all(const app_config_t *cfg);`
- [ ] **Step 2:** Implement = the exact `fram_write_*(FRAM_ADDR_*, cfg->…)` block from `app_config_factory_write` (the persist half, lines 39–67), **plus** `EN_ENERGY ← cfg->energy_ctrl`, `EN_MULTI ← cfg->multi_ctrl`, and the ether arrays `ETHER_IP/NM/GW ← cfg->ether_ip/nm/gw` (4 bytes each, big-endian via `fram_write_byte` loop or `fram_write_u32`). Refactor `factory_write` to call `app_config_save_all(cfg)` after setting defaults (DRY) — keep behavior identical.
- [ ] **Step 3: build green.**
- [ ] **Step 4: commit** — `feat(config): add app_config_save_all (commit live cfg to FRAM)`

Rationale: FRAM has no write-cycle cost, so committing the whole map on DATA_SAVE is simpler and strictly more consistent than samd20's per-page EEPROM-frugal saves (spec §8.3). The per-page sets in spec §8.2 are the *minimum*; save_all is a faithful superset (other fields hold unchanged live values).

Static trace: field set + addresses + widths identical to `factory_write`; `EN_ENERGY`/`EN_MULTI` now reflect live bools (factory wrote 0).

---

## Task 3: String formatters (`app_lcd_str`)

**Files:** create `fw/src/app_lcd_str.c`; add prototypes to `app_lcd.h`

- [ ] **Step 1:** Port from samd20 (read exact behavior from `ref/samd20/main.c`):
  - `uint8_t time2str(uint16_t src, uint8_t *dest)` — tenths-of-seconds "X.XX" form (main.c:2708); returns length.
  - `uint8_t energy2str(uint32_t src, uint8_t *dest)` — compact decimal, leading-zeros suppressed.
  - `uint8_t conv_addr2str(uint8_t addr, uint8_t *dest)` — 0→"NONE", 1–255→" NNN" (space + 3 digits).
  - `uint8_t ip_to_string(const uint8_t ip[4], uint8_t *dest)` — "%d.%d.%d.%d" padded to 16 bytes.
  - `void lcd_data_pdd(uint8_t len, uint8_t *data)` — the IP-edit display helper used by `LV_ETHER_KEY`.
- [ ] **Step 2: build green** (functions extern via header → no unused warning).
- [ ] **Step 3: commit** — `feat(lcd): port LCD string formatters (time/energy/addr/ip)`

Static trace: confirm digit/decimal placement against samd20 byte-for-byte (these strings land on the panel verbatim). No `sprintf` float; integer-only. FLAG any ambiguity in `energy2str` rounding (spec note).

---

## Task 4: LCD subsystem scaffolding — state, measurement provider, stub hooks

**Files:** modify `fw/include/app_lcd.h`, `fw/src/app_lcd.c`

- [ ] **Step 1:** In `app_lcd.h` define:
  - `typedef enum { US_CMD_START, US_CMD_SEEK, US_CMD_RESET, US_CMD_RUN_RELEASE } us_cmd_t;`
  - `lcd_app_state_t` — fields from spec §4.2 (`lcd_status`, `sys_mode`, `sys_status`, `error_status`, `horn_status`, `key_tick`, `ref_lv_1/2/10/20`, comm shadows `temp_*`, ether-input FSM fields, `last_set_page_ms`).
  - `lcd_measure_t` — fields from spec §4.3 (`curr_amp`,`curr_power`,`max_power`,`curr_freq`,`last_freq`,`curr_energy`,`last_energy`,`us_on_time_200m`,`us_run_status`,`sig_run/seek/reset_status`).
  - Hook prototypes (spec §5): `app_lcd_hook_set_pot`, `app_lcd_hook_us_command`, `app_lcd_hook_comm_reconfigure`, `app_lcd_hook_ether_apply`, `app_lcd_hook_horn`.
  - Provider prototype: `const lcd_measure_t *app_lcd_measure(void);`
  - Subsystem entry prototypes: `app_lcd_change_page`, `app_lcd_input_dispatch`, `app_lcd_tick`, `app_lcd_var_init`.
- [ ] **Step 2:** In `app_lcd.c`: define `static lcd_app_state_t g_lcd;` exposed to sibling files via an accessor `lcd_app_state_t *app_lcd_state(void)` (keeps state ownership in one TU). Define stub hook bodies = `mon_printf("[lcd-hook] …\r\n", …)` only (no hardware). Define a **stub measurement provider** returning a `static const lcd_measure_t` of all-zeros (optionally a `#if LCD_DEMO_MEASURE_RAMP` slow ramp for visual bring-up). Set `ref_lv_*` from `model_freq` in `app_lcd_init_mode` (spec §4.2 values).
  - **DAC formula in `app_lcd_hook_set_pot`:** log `(uint8_t)((output_power - 50) * 255 / 100)` (spec §5, F1) — no I2C write (U4 identity = F2 open).
- [ ] **Step 3: build green.**
- [ ] **Step 4: commit** — `feat(lcd): add LCD subsystem state, measurement stub, control/HW stub hooks`

Static trace: hooks/provider are referenced by Tasks 5–8; declared-in-header so no unused warning. `app_lcd_state()` is the single state owner (no globals scattered across TUs — coding-style).

---

## Task 5: `change_lcd_page()` render port (`app_lcd_render`)

**Files:** create `fw/src/app_lcd_render.c`; refactor `app_lcd.c` `init_mode`

- [ ] **Step 1:** Implement `void app_lcd_change_page(uint8_t page)` per **spec §6 table** — always-first `DISP_ENERGY_EN`/`DISP_MULTI_EN`, then per-page VP pre-fill (RUN_STD strings via Task 2 formatters; SETUP/comm/ethernet pages; comm-shadow load under `temp_comm_mode==0xff` sentinel), tail `dgus_set_page(page)`. Read config from `g_cfg` (pass `const app_config_t *` or use accessor), transient from `app_lcd_state()`. Setup-page-1 entry calls `app_lcd_hook_set_pot(output_power)`. Define `VERSION_MSG` (20-byte STM32 fw version string) for `DISP_VERSION`.
- [ ] **Step 2:** Record `state->last_set_page_ms = sys_tick_get_ms()` inside the page-set path (feeds the §7 loop guard). Implement `app_lcd_var_init()` (spec §1.1: the `KEY_MULTI+1`/`SETUP_MODEL+1` seed writes). Refactor `app_lcd_init_mode` to set `ref_lv_*` + pre-fill + end by calling `app_lcd_change_page(run_page)` (samd20 `init_lcd_mode` tail), removing the Stage-B inline duplication — **keep the exact RUN-page result** (no visual regression).
- [ ] **Step 3: build green.**
- [ ] **Step 4: commit** — `feat(lcd): port change_lcd_page page rendering`

Static trace: every VP written exists (Task 1); every formatter exists (Task 2); RUN_HAND/RUN_MULTI(3)/RUN_STD(9) branch matches Stage B; F3 (HAND→MULTI) not relevant here (that's save-side). Confirm `app_lcd_ensure_run_page` (Stage B) still drives the same run page after refactor.

---

## Task 6: Input dispatch — core (`app_lcd_input`, part A)

**Files:** create `fw/src/app_lcd_input.c`

- [ ] **Step 1: frame decode.** `void app_lcd_input_dispatch(const dgus_frame_t *f)` — act only on `f->cmd == DGUS_CMD_RD` (0x83); `vp = f->vp_addr`; `data16 = (f->data[0]<<8)|f->data[1]`. **Confirm `dgus_frame_t.data[]` payload offset** against `drivers/dgus_lcd.c` rx parser before relying on it (spec F5); if layout differs, adjust decode. Add a one-line `mon_printf` trace of dispatched vp/data (keep observability).
- [ ] **Step 2: simple-edit + nav + command branches** (spec §7 table, all rows EXCEPT `DATA_SAVE` and the comm/ether shadow rows — those are Task 7):
  - Live-cfg edits: MODEL_FREQ/TYPE (+`send_model_str`), VAR_CAL_VAL, VAR_FREQ_CAL_VAL, LV_DM_*, LV_TM_*, LV_MO_OUT1/2, LV_MO_TIME1/2 (with the cross-clamp + echo), LV_OUT_POWER, LV_MAX_ON_TIME, LV_ENERGY_EDIT, LV_LIMIT_OUT_T, DISP_SAFTY, ENERGY_EN/MULTI_EN (toggle live + echo).
  - **LV_OUT_POWER updates live `output_power` ONLY — NO `app_lcd_hook_set_pot` here.** The DAC write fires only on KEY_MULTI==3 RUN press (this task, commands), DATA_SAVE (Task 7), and setup-page-1 entry (Task 5). Do not add a DAC call on the edit (samd20 §7 fidelity).
  - Page nav: SETUP_MODEL (long-press via key_tick>200), SETUP_PARAM, SETUP_PARAM_MOOHAN, STD_SETUP_PARAM (1..5; case 5 → temp_cnt_reset shadow), LV_RUN_MODE (→change_page).
  - Commands → hooks: KEY_MULTI (1/2/3/4 → `app_lcd_hook_us_command` + sig/us status on state; RUN press → `app_lcd_hook_set_pot` + zero energy accdoms in state), KEY_ERROR_RESET (→ hook + error clear/page restore).
  - DISP_HORNDOWN → `temp_horndown` shadow.
- [ ] **Step 3: build green.**
- [ ] **Step 4: commit** — `feat(lcd): port parse_lcd_comm core dispatch (edits, nav, commands)`

Static trace: every VP/condition/clamp matches spec §7 (which mirrors samd20). `LV_MO_TIME1>TIME2` clamp + echo present. Page-nav targets match model_type/run_mode/comm_mode branching.

---

## Task 7: Input dispatch — save/cancel + comm/ethernet (`app_lcd_input`, part B)

**Files:** modify `fw/src/app_lcd_input.c`

- [ ] **Step 1: comm/ether shadow edits** (spec §7 rows): COMM_ADDR/SPEED/PARITY (→`temp_*` + echo `*_TXT` via formatters + `comm_speed_txt`/`comm_parity_txt` tables — port both string tables verbatim), LV_COMM_MODE (0/1/2 serial/static/toggle-DHCP + echo + change_page), LV_ETHER_KEY (field select 'I'/'M'/'G' + `process_ip_char` digit/'D'/'B'/'E' FSM → `temp_ether_*`, echo via `ip_to_string`/`lcd_data_pdd`). Implement `process_ip_char` as a static helper.
- [ ] **Step 2: `DATA_SAVE` (0x1050)** — `==1` SAVE / `==0` CANCEL (spec §8):
  - SAVE: by `state->lcd_status` page group (spec §8.2): MODEL_SETUP / MULTI / HAND / STD. Commit comm/ether shadows → live cfg; if comm changed → `app_lcd_hook_comm_reconfigure`; if ether changed → `app_lcd_hook_ether_apply`; if `temp_cnt_reset` → `cfg.work_cnt=0` + echo `LV_WORK_CNT`; if `temp_horndown` → `app_lcd_hook_horn`; MULTI/HAND path → `app_lcd_hook_set_pot`. Then **`app_config_save_all(&g_cfg)`** (single FRAM commit, Task 2). Set `lcd_status` to the run page per group (**F3: HAND→MULTI verbatim**). `change_page(run page)`.
  - CANCEL: `app_config_load(&g_cfg)` (full FRAM re-read = process-param rollback — process params ARE live-mutated on touch, so they must reload). Then reset comm shadow sentinel `state->temp_comm_mode = 0xff` so the next setup-page entry reloads shadows from live. **F4 note:** `app_config_load` also reads comm_mode/ether, but in the LCD flow those live fields are **never mutated before SAVE** (only `temp_*` shadows are), so reloading them writes back identical values — **no observable difference** from samd20's "skip comm_mode/ether on cancel" quirk. This is the faithful outcome; the implementer must **not** add a special skip-variant (it would be dead complexity). Annotate `/* F4: full reload is a no-op for comm/ether since live is never pre-save-mutated */`. `change_page(lcd_status)`.
- [ ] **Step 3: build green.**
- [ ] **Step 4: commit** — `feat(lcd): port DATA_SAVE/CANCEL + comm/ethernet setup editing`

Static trace: save groups + FRAM persistence match spec §8.2; shadow sentinel (`temp_comm_mode==0xff`) load gate intact; F3/F4 annotated. `app_config_save_all` covers every saved field.

---

## Task 8: Display step-machine + icons + error page (`app_lcd_disp`)

**Files:** create `fw/src/app_lcd_disp.c`

- [ ] **Step 1: step machine** `void app_lcd_disp_step(void)` — a static step counter 0..9 advancing one group per call (spec §11). Read measured values from `app_lcd_measure()`:
  - steps 0–4: `LV_OUTPUT[0..19]` bar in 4×5 `dgus_write_u16_array` chunks. Port the exact bar-fill (spec §11): curr_amp vs `ref_lv_1/10/20` thresholds + set-point marker at `output_power*20/100`.
  - steps 5–6,8,9: `LV_TIME[0..19]` bar in 4×5 chunks (`i < us_on_time_200m`).
  - step 7: `VAR_POWER/AMP/FREQ` (3-word array; FREQ = freq/100) + `VAR_ENERGY`. (Skip `DISP_REMOTE`/modbus_status — Stage C.)
- [ ] **Step 2: event-driven helpers** (called by input/control, not the step machine): `app_lcd_icon(vp, on)` for ICON_RESET/SEEK/RUN/OL/OUTERR; `app_lcd_set_work_cnt(u32)`; `app_lcd_show_error(code)` — `error_status` 0→nonzero → `dgus_set_page(LCD_WARNING)` + `VP_ERROR_MSG` text (unreachable until Stage D feeds faults, but path implemented; clear side already in Task 6 KEY_ERROR_RESET).
- [ ] **Step 3: build green.**
- [ ] **Step 4: commit** — `feat(lcd): port periodic display step-machine + icons + error page`

Static trace: bar-fill matches samd20 `send_outpower_data`/`send_outtime_data`; with zero measurement provider, bars render empty + VAR_*=0 (the approved "alive but idle" UI). Chunked 5-element writes preserved (UART anti-saturation).

---

## Task 9: `app.c` integration — dispatch wiring, display tick, loop guard

**Files:** modify `fw/src/app.c`, `fw/src/app_lcd.c`

- [ ] **Step 1:** In `app_loop_iter`, replace the debug RX print with `app_lcd_input_dispatch(&f)` (keep echo drop). Keep the Stage B boot path and `ensure_run_page` untouched.
- [ ] **Step 2:** Add `app_lcd_tick()` (in `app_lcd.c`) called every iter; internally it advances `app_lcd_disp_step()` on a **4 ms cadence** (spec §11) using `sys_tick_get_ms()`. **MUST retire the Stage B 1 Hz liveness write:** it writes `DGUS_DEMO_UPTIME_VP == VAR_POWER` (0x1110), the **same VP** the display step 7 now drives — they would fight on the same address. Remove it (do not keep behind `#if` on the same VP). If a heartbeat is still wanted for bring-up, move it to an unused VP behind `LCD_DEMO_*`.
- [ ] **Step 3: SYS_PIC_NOW loop guard** (spec §10, mandatory): in the input layer's `SYS_PIC_NOW==0` branch, suppress re-init (`app_lcd_var_init`+`send_model_str`+`app_lcd_init_mode`) if `sys_tick_get_ms() - state->last_set_page_ms < 200`, and only after a boot-complete flag is set. Set that flag at the end of `app_init`.
- [ ] **Step 4: build green.**
- [ ] **Step 5: commit** — `feat(app): wire LCD input dispatch + display tick + SYS_PIC_NOW loop guard`

Static trace: dispatch replaces print only (RX drain + echo drop unchanged); `app_lcd_tick` cadence = 4 ms; loop guard kills the `change_page → SYS_PIC_NOW → re-init` feedback loop. No Stage B regression (cold-boot handshake intact).

---

## Task 10: HW bring-up verification (board on bench) + size

**Files:** none (verification); `docs/changelog.md` + `docs/superpowers/RESUME.md`/`NEXT_STEPS.md` update at end.

- [ ] **Step 0: confirm board on bench** — verify the hw-revA board + ST-LINK are connected before starting bench steps. If not present, **mark Task 10 deferred and ship a code-only PR** (Tasks 1–9 are build-gated and merge-able without the board); record the deferral in the changelog.
- [ ] **Step 1: clean build + size** — `rm -rf build && env -u STM32_TOOLCHAIN cmake -B build -G Ninja && env -u STM32_TOOLCHAIN cmake --build build`; `arm-none-eabi-size build/gds_us_ctrl.elf` (record growth over Stage B 29104/476/2420).
- [ ] **Step 2: flash + bench checklist** (spec §13.3, USART6 mon @115200):
  - Boot → RUN page renders (Stage B parity), `run_page_confirmed=1` (no regression).
  - Touch SETUP entry → setup page shows current limits; edit one value; SAVE → power-cycle → value persists (FRAM determinism).
  - CANCEL → value reverts to FRAM.
  - Navigate all setup/comm/ethernet pages — no lockup, no SYS_PIC_NOW loop (watch mon trace).
  - Comm/ethernet edits render + persist (no live apply this stage — verify via reboot read-back + `[lcd-hook]` trace).
  - Hooks emit `[lcd-hook]` on RUN/SEEK/RESET/SAVE — confirms wiring without driving hardware.
  - (Optional) `LCD_DEMO_MEASURE_RAMP` build → LV_OUTPUT/LV_TIME bars sweep, VAR_* count.
- [ ] **Step 3:** Record results in `docs/changelog.md`; update RESUME/NEXT_STEPS (LCD port done; next = Stage D regulation feeds the measurement provider + real hooks). Commit — `docs(lcd): record Stage LCD HW verification + size`.
- [ ] **Step 4:** (separate, on user go-ahead) open PR `feat/stage-lcd-full-behavior → main`; tag if green (`hw-revA_fw-stage-lcd`).

---

## Deferred (NOT this plan)

- Real measurement source (Stage D regulation fills `lcd_measure_t`).
- Real hook bodies: I2C_POT DAC (F2 chip identity), ultrasonic command pins, Modbus reconfigure (Stage C), W5500/Ethernet apply.
- Physical buttons (`sw_mode`/`sw_set`) — control GPIO.
- `DISP_REMOTE`/modbus_status display (Stage C).
- 7-seg FND (`disp_update`/`fnd_out`) — dead, never ported.

## Open flags carried from spec (annotate in source, do not block)
F1 DAC zero-point · F2 I2C_POT identity · F3 HAND→MULTI save · F4 cancel comm/ether · F5 `dgus_frame_t.data[]` offset (confirm in Task 6) · F6 DGUS touch report cmd=0x83 (confirm in Task 10).
