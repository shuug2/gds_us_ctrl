# Stage LCD — Full LCD Behavior Port (samd20 → STM32F410) Design

> Status: DRAFT for user review · Date: 2026-05-27 · Branch: `feat/stage-d-osc-pin-io` (LCD work precedes Stage D regulation per user 2026-05-27)
> Predecessor: Stage B (`init_lcd_mode` port, RUN-page bring-up, FRAM config). Wire layer + FRAM layer complete & HW-verified.
> Port source (read-only, **authoritative — user: "samd20은 동작이 정확하게 되는 코드"**): `ref/samd20/main.c`, `ref/samd20/dgus_lcd.h`, `ref/samd20/define.h`.
> Behavioral map this spec is built from: code-explorer analysis 2026-05-27 (parse_lcd_comm + display refresh + change_lcd_page).

---

## 1. Goal & Scope

Port the **entire LCD operational behavior** of the samd20 firmware to STM32 on top of the Stage B wire/FRAM
layers: panel touch/key **input** (page navigation, setup editing, save/cancel to FRAM), per-page **rendering**
(`change_lcd_page`), and the periodic **live-value display** (output bar, time bar, power/amp/freq/energy,
status icons). Where behavior reaches into not-yet-ported control/hardware (ultrasonic commands, output-power
DAC, Modbus/Ethernet reconfigure, physical buttons), it is routed through **named stub hooks** that become the
Stage C/D integration points.

### 1.1 In scope

- **Full VP macro set** — extend `fw/include/dgus_lcd.h` with every VP/key macro samd20 uses (currently only the scope-a subset is present).
- **Input dispatch** — port `parse_lcd_comm()`: the panel→MCU touch/key handler (§7 table). Replaces the debug RX print in `app_loop_iter`.
- **Page rendering** — port `change_lcd_page()`: per-page VP pre-fill for RUN_STD, all SETUP pages, comm/ethernet pages (§6 table).
- **Periodic display refresh** — port the samd20 main-loop `job_state` display machine (LV_OUTPUT bar, LV_TIME bar, VAR_POWER/AMP/FREQ/ENERGY) + event-driven ICON/work_cnt writes. **User decision 2026-05-27: include with stubbed measurement sources.**
- **Setup-page edit/save discipline** — shadow-copy pattern + DATA_SAVE commit-to-FRAM / DATA_CANCEL rollback (§8). Verbatim behavior.
- **String formatters** — `time2str`, `energy2str`, `conv_addr2str`, `ip_to_string`, `lcd_data_pdd`.
- **Stub hook layer** (§5) — DAC, ultrasonic command, comm/ethernet reconfigure, horn/solenoid. Empty bodies + `mon_printf` trace.
- **`lcd_var_init`** — port the boot/cold-boot panel-var seed (subagent map: `dgus_write_u16_array(KEY_MULTI+1, …, 2)` + `dgus_write_u16_array(SETUP_MODEL+1, …, 2)` — initial key/model widget state); called at boot and on `SYS_PIC_NOW==0`.

**Fidelity stance:** this port reproduces samd20 behavior **including its documented quirks** (F3 HAND→MULTI page, F4 cancel does not restore comm_mode/ether). STM32-discovered improvements (§10 cold-boot handshake) are **preserved, not reverted**. The implementer must not silently "fix" F3/F4 — they are intentional fidelity, flagged for HW review only.

### 1.2 Out of scope (deferred, behind hooks)

- Real ultrasonic regulation / ADC measurement (**Stage D** — fills the measurement provider §4.3 and `app_lcd_hook_us_command`).
- Real `I2C_POT` DAC write (U4 @ 0x28) — chip identity is an open question (NEXT_STEPS); hook stubbed.
- Modbus RTU reconfigure (**Stage C**) and W5500/Ethernet apply — config **renders & persists** but is **not applied** to live comm this stage.
- Physical buttons (`sw_mode`/`sw_set` etc.) — GPIO, control domain. The LCD touch equivalents are ported; physical keys deferred.
- 7-segment FND (`disp_update`/`fnd_out`) — dead in this product; **not ported** (confirmed: samd20 `disp_update()` drives only the FND, not the DGUS panel).

---

## 2. Architecture — extend the Stage A/B layering

```
usart1   (drivers/usart1.c)         USART1 PA9/PA10 — DGUS @115200, IRQ ring rx  [done]
   │
dgus_lcd (drivers/dgus_lcd.c + .h)  wire layer: write_u16/u32/bytes/u16_array/text,
   │                                set_page, read_var, rx_poll, is_echo, read_word  [done]
   │                                +++ add full VP macro set (§9)
   │
app_lcd  (src/app_lcd*.c)           BEHAVIOR layer — this stage:
   │   ├─ app_lcd.c        init_mode (Stage B, kept) + state owner + top-level tick/dispatch
   │   ├─ app_lcd_render.c change_lcd_page() port (§6)
   │   ├─ app_lcd_input.c  parse_lcd_comm() port — VP→action dispatch (§7) + DATA_SAVE/CANCEL (§8)
   │   ├─ app_lcd_disp.c   periodic display step-machine (§ LV_OUTPUT/LV_TIME/VAR_*) + ICON helpers
   │   └─ app_lcd_str.c    time2str / energy2str / conv_addr2str / ip_to_string / lcd_data_pdd
   │
app_config (src/app_config.c)       live config + FRAM persistence  [done]
   │                                +++ add field-save helpers used by DATA_SAVE (§8.3)
fram     (drivers/fram.c + .h)       fram_write_byte/u16/u32, FRAM_ADDR_* map  [done]
```

Rationale: the wire layer (`send_lcd_*` → `dgus_write_*`) and FRAM byte-map are already complete and HW-verified;
this stage adds only the application logic above them. Split into focused files (<800 lines each per coding-style;
`parse_lcd_comm` is the largest samd20 function and must not become one monolith).

### 2.1 samd20 → STM32 call mapping (wire layer, already present)

| samd20 | STM32 |
|---|---|
| `send_lcd_data_var(vp, u16)` | `dgus_write_u16(vp, u16)` |
| `send_lcd_data_word(vp, u32)` | `dgus_write_u32(vp, u32)` |
| `send_lcd_byte_array(vp, n, buf)` | `dgus_write_bytes(vp, buf, n)` |
| `send_lcd_int_array(vp, n, u16buf)` | `dgus_write_u16_array(vp, u16buf, n)` |
| `send_lcd_txt(vp, s)` | `dgus_write_text(vp, s)` |
| `set_lcd_page(p)` | `dgus_set_page(p)` |
| `read_system_var(v)` | `dgus_read_var(v)` |
| `check_lcd_comm()` framing | `dgus_rx_poll(&frame)` (done in `app_loop_iter`) |
| `save_byte/word/param_fram(addr,…)` | `fram_write_byte/u32/u16(FRAM_ADDR_*, …)` |
| `read_byte/word/param_fram(addr)` | `fram_read_byte/u32/u16(FRAM_ADDR_*)` |

---

## 3. Input path: frame → dispatch

samd20 `parse_lcd_comm` acts **only on `LCD_RD` (0x83)** frames (panel touch/auto-upload reports) and ignores
`LCD_WR` (0x82) echoes. Our `app_loop_iter` already drains `dgus_rx_poll` and drops echoes via `dgus_is_echo`
(cmd==0x82). Replace the current debug print with:

```c
dgus_frame_t f;
while (dgus_rx_poll(&f)) {
    if (dgus_is_echo(&f)) continue;     /* 0x82 WR echo */
    app_lcd_input_dispatch(&f);         /* 0x83 touch/key report */
}
```

Frame decode (mirror samd20 `lcd_data_addr` / `lcd_data`):
- `vp = f.vp_addr`
- `data16 = (f.data[0] << 8) | f.data[1]` (first word, big-endian)
- **Implementer must confirm** `dgus_frame_t.data[]` holds the payload after the VP address (data[0]=MSB). Verify against `drivers/dgus_lcd.c` rx parser before writing dispatch. FLAG if layout differs.

---

## 4. State model

### 4.1 Live config (exists) — `app_config_t g_cfg`
LCD edits write **directly to the live `g_cfg` field** (samd20 "immediate-to-live, deferred-FRAM" pattern §8).
`g_cfg` already contains every config/limit var the LCD touches (model_freq/type, run_mode, f_safty, all
limit_*, output_power, work_cnt, limit_energy, energy_ctrl, multi_ctrl, cal_val, freq_cal_val, comm_*, ether_*).

### 4.2 Transient runtime state (new) — `lcd_app_state_t`
Owned by the LCD subsystem, initialized in `app_lcd_init_mode`:

| Field | Purpose | samd20 origin |
|---|---|---|
| `lcd_status` | current page ID | global `lcd_status` |
| `sys_mode` | hand/multi/std (= model_type) | `sys_mode` |
| `sys_status` | RUN/SETUP/HORN/ERROR | `sys_status` (control-fed via hook; default SYS_RUN) |
| `error_status` | bitmask ERR_OVLD/OVTIME/OUTERR | control-fed; LCD clears on error-reset key |
| `horn_status` | solenoid request | control-fed |
| `key_tick` | long-press timer (SETUP_MODEL, SETUP_PARAM_MOOHAN; threshold 200) | `key_tick` |
| `ref_lv_1/2/10/20` | output-bar thresholds, set from `model_freq` | `init_lcd_mode` |
| comm shadows: `temp_address`,`temp_speed_idx`,`temp_parity_idx`,`temp_comm_mode`(0xff sentinel),`temp_ether_ip/nm/gw[4]`,`temp_cnt_reset`,`temp_horndown` | setup-page edit staging | identical names |
| ether-input FSM: `ether_what_input`,`ether_current_number`,`ether_current_octet`,`ether_has_input`,`ether_ip_input_complete`,`ether_input_buffer[16]`,`ether_temp_ip[4]`,`ether_buffer_pos` | IP/NM/GW text entry | identical names |

`ref_lv_*` from model_freq (init_lcd_mode, verbatim): 15 kHz→{50,100,1000,2000}; 20 kHz→{50,100,600,1200}; ≥30 kHz→{50,100,400,700}.

### 4.3 Measurement provider (stubbed) — `lcd_measure_t`
The display refresh reads measured values from a provider the control layer (Stage D) fills. Until then a stub
returns zeros (optionally a slow test ramp behind a compile flag for visual bring-up).

| Field | Display target | Stage D source |
|---|---|---|
| `curr_amp` | LV_OUTPUT bar fill | ADC ch0 scaled |
| `curr_power` / `max_power` | VAR_POWER (0x1110) | curr_amp·22/10 |
| `curr_freq` / `last_freq` | VAR_FREQ (0x1112) = freq/100 | freq counter |
| `curr_energy` / `last_energy` | VAR_ENERGY (0x1113) | energy accumulator |
| `us_on_time_200m` | LV_TIME bar fill (0..200) | run timer |
| `us_run_status`, `sig_*` | ICON_RUN/SEEK/RESET/OL/OUTERR | command FSM |

Interface: `const lcd_measure_t *app_lcd_measure(void)` — provided by a weak/stub source now, real source in Stage D.

---

## 5. Stub hook interface (control/hardware coupling)

Empty bodies + `mon_printf` trace this stage; real bodies in Stage C/D. Declared in `app_lcd.h`, stub-defined in
`app_lcd.c` (or a dedicated `app_lcd_hooks_stub.c`).

| Hook | Triggered by | Stage |
|---|---|---|
| `app_lcd_hook_set_pot(uint8_t output_power)` | DATA_SAVE / RUN press / setup-page entry / boot — DAC write | D (chip = open Q) |
| `app_lcd_hook_us_command(us_cmd_t {START,SEEK,RESET,RUN_RELEASE})` | KEY_MULTI 1/2/3/4, KEY_ERROR_RESET | D |
| `app_lcd_hook_comm_reconfigure(speed_idx, parity_idx, address)` | DATA_SAVE when comm changed (MULTI path) | C |
| `app_lcd_hook_ether_apply(mode, ip[4], nm[4], gw[4])` | DATA_SAVE when ethernet changed | later (W5500) |
| `app_lcd_hook_horn(bool down)` | DATA_SAVE STD path, `temp_horndown` | D |

**DAC formula (exact, from samd20):** `(uint8_t)((output_power - 50) * 255 / 100)`. NB output_power=50→byte 0,
output_power=100→byte **127** (not 255). FLAG for HW verify whether power=50 is the intended zero point. Hook
logs the byte; no I2C write until U4 identity confirmed.

---

## 6. Page rendering — `change_lcd_page(page)` port

Always-first (both run & setup): send `DISP_ENERGY_EN ← energy_ctrl`, `DISP_MULTI_EN ← multi_ctrl`. Then per page:

| Page | VP writes (source) | Notes |
|---|---|---|
| `LCD_RUN_STD` (9) | LV_DM_DELAY←limit_delay_time1; DISP_RUN_MODE←run_mode; DISP_SAFTY←f_safty; LV_LIMIT_OUT_T←limit_out_time; DISP_STD_DATA1/2/3 ← formatted "D:/W:/H:" or "SENSOR OFF" strings (run_mode + energy_ctrl + multi_ctrl branching) | uses `time2str`/`energy2str` |
| `LCD_SETUP_HAND`/`MULTI`/`STD1` (7/5/10) | DISP_VERSION←VERSION_MSG(20B); LV_MAX_ON_TIME←limit_on_time; LV_OUT_POWER←output_power; **hook_set_pot(output_power)**; LV_ENERGY_VAL(u32)←limit_energy; LV_ENERGY_EDIT←limit_energy; DISP_ENERGY_EN←energy_ctrl; LV_LIMIT_OUT_T←limit_out_time; **load comm shadows**, temp_cnt_reset=0, temp_comm_mode=0xff | page-1 setup entry |
| `LCD_SETUP_STD2D` (12) | LV_DM_DELAY/WELD/HOLD ← limit_delay_time1/2/3; temp_comm_mode=0xff | |
| `LCD_SETUP_STD2T` (13) | LV_TM_WELD/HOLD ← limit_trigger_time2/3; temp_comm_mode=0xff | |
| `LCD_SETUP_STD3`/`MH2` (15/19) | DISP_MULTI_EN←multi_ctrl; LV_MO_OUT1/OUT2/TIME1/TIME2 ← limit_mo_*; temp_comm_mode=0xff | |
| `LCD_SETUP_STDC`/`MHC` (23/21) | COMM_ADDR/SPEED/PARITY ← comm_*; load comm shadows; ADDR/SPEED/PARITY text; if temp_comm_mode==0xff load ether shadows + IP/NM/GW text + reset ether FSM; DISP_COMM_MODE per temp_comm_mode | serial config |
| `LCD_SETUP_STDE`/`MHE` (27/25) | as STDC plus DISP_EN_DHCP when temp_comm_mode>0 | ethernet config |

Tail: `dgus_set_page(page)`. Comm-shadow load is gated by the `temp_comm_mode == 0xff` sentinel (load-on-first-visit;
prevents reload when navigating between comm sub-pages).

`VERSION_MSG` — define a 20-byte STM32 firmware version string (new; samd20 had its own).

---

## 7. Input dispatch — `parse_lcd_comm()` port (VP → action)

Condensed from the behavioral map. `data16` = incoming value. Edits write **live `g_cfg`** unless noted "shadow".

| Incoming VP | Condition | Action | Persist? |
|---|---|---|---|
| `SYS_PIC_NOW` 0x0014 | ==0 (panel boot/page-flip) | re-init: lcd_var_init + send_model_str + init_lcd_mode — **guarded** (see §10 loop guard) | — (coordinate w/ Stage-B `ensure_run_page`, §10) |
| `MODEL_FREQ` 0x1060 | any | model_freq=data; send_model_str | on DATA_SAVE |
| `MODEL_TYPE` 0x1070 | any | model_type=data; send_model_str | on DATA_SAVE |
| `VAR_CAL_VAL` 0x1410 | int16 | cal_val=data | on DATA_SAVE |
| `VAR_FREQ_CAL_VAL` 0x1412 | int16 | freq_cal_val=data | on DATA_SAVE |
| `KEY_MULTI` 0x1080 | 1/2/3/4 | hook_us_command(RESET/SEEK/RUN/RUN_RELEASE); set sig/us status; RUN press → hook_set_pot + zero energy accum; if in ERROR clear ICON_OL/OUTERR & restore page | — |
| `KEY_ERROR_RESET` 0x1408 | ==1 | hook_us_command(RESET); if error_status&ERR_OVTIME clear & restore page via model_type | — |
| `SETUP_MODEL` 0x1084 | 0=press / 2=release | press: key_tick=0; release & key_tick>200: page=LCD_MODEL_SETUP, send model/cal VPs | — |
| `SETUP_PARAM` 0x1100 | any | page = setup-1 by sys_mode (HAND/MULTI/STD1); change_lcd_page | — |
| `SETUP_PARAM_MOOHAN` 0x1094 | 0/2 | long-press variant of SETUP_PARAM | — |
| `STD_SETUP_PARAM` 0x1020 | 1..5 | page nav: 1→setup1 (set_page only); 2→STD2D/STD2T(by run_mode)/MH2; 3→STD3/MHC/MHE(by comm_mode); 4→STDC/STDE; 5→ if work_cnt!=0 temp_cnt_reset=1 | shadow (5) |
| `LV_RUN_MODE` 0x1210 | 1=DELAY / 2=TRIGGER | run_mode=MODE_*, page=STD2D/STD2T, change_lcd_page | on DATA_SAVE |
| `LV_DM_DELAY/WELD/HOLD` 0x1203/4/5 | u16 | limit_delay_time1/2/3 = data | on DATA_SAVE |
| `LV_TM_WELD/HOLD` 0x1206/7 | u16 | limit_trigger_time2/3 = data | on DATA_SAVE |
| `LV_MO_OUT1/OUT2` 0x1400/1 | u16 | limit_mo_out1/2 = data | on DATA_SAVE |
| `LV_MO_TIME1` 0x1402 | u16 | limit_mo_time1=data; if >time2 → time2=time1, echo LV_MO_TIME2 | on DATA_SAVE |
| `LV_MO_TIME2` 0x1403 | u16 | limit_mo_time2=data; if time1>time2 clamp time2=time1, echo | on DATA_SAVE |
| `LV_OUT_POWER` 0x1200 | u8 (50–100) | output_power=data (DAC written only on SAVE/RUN) | on DATA_SAVE |
| `LV_MAX_ON_TIME` 0x1201 | u16 | limit_on_time=data (HAND) | on DATA_SAVE |
| `LV_ENERGY_EDIT` 0x1202 | u16 | limit_energy=data | on DATA_SAVE |
| `LV_LIMIT_OUT_T` 0x120f | u8 | limit_out_time=data | on DATA_SAVE |
| `ENERGY_EN` 0x1085 | ==1 | toggle energy_ctrl; echo DISP_ENERGY_EN | on DATA_SAVE (live toggled now) |
| `MULTI_EN` 0x140a | ==1 | toggle multi_ctrl; echo DISP_MULTI_EN | on DATA_SAVE (live toggled now) |
| `DISP_SAFTY` 0x120d | 0/1 | f_safty = data | on DATA_SAVE |
| `DISP_HORNDOWN` 0x1209 | 0/1 | temp_horndown = data | shadow |
| `COMM_ADDR` 0x1071 | 0–255 | temp_address=data; echo COMM_ADDR_TXT (conv_addr2str) | shadow |
| `COMM_SPEED` 0x1072 | 0–5 | temp_speed_idx=data; echo COMM_SPEED_TXT | shadow |
| `COMM_PARITY` 0x1073 | 0–2 | temp_parity_idx=data; echo COMM_PARITY_TXT | shadow |
| `LV_COMM_MODE` 0x140b | 0/1/2 | temp_comm_mode serial/static/toggle-DHCP; echo DISP_COMM_MODE/EN_DHCP; change_lcd_page | shadow |
| `LV_ETHER_KEY` 0x140f | 'I'/'M'/'G' / 0-9/'D'/'B'/'E' | select field / process_ip_char FSM → temp_ether_*; echo COMM_*_TXT | shadow |
| `DATA_SAVE` 0x1050 | ==1 SAVE / ==0 CANCEL | §8 bulk commit / rollback | FRAM |

`comm_speed_txt[6][6]` = {" 2400 "," 4800 "," 9600 ","19200 ","38400 ","115200"}; `comm_parity_txt[3][4]` = {"EVEN","ODD ","NONE"} — port verbatim.

---

## 8. Setup edit / save discipline (FRAM contract — preserve verbatim)

### 8.1 Two patterns
- **Process params** (delay/weld/hold/trigger/mo/out_power/energy/run_mode/safty…): VP touch writes **live `g_cfg`** immediately, no FRAM. Cancel = full FRAM re-read into `g_cfg`.
- **Comm/ethernet**: VP touch writes **shadow** (`temp_*`). Load shadows on page entry under `temp_comm_mode==0xff` sentinel. Save commits shadows→live→FRAM (+ reconfigure hooks). Cancel re-reads FRAM (samd20 cancel restores only addr/speed/parity, **not** comm_mode/ether — replicate this latent quirk, FLAG it).

### 8.2 DATA_SAVE==1 by current page (`lcd_status`)
- **MODEL_SETUP**: FRAM model_freq/type, cal_val, freq_cal_val; update lcd_status & sys_mode by model_type.
- **MULTI path**: out_power(+DAC hook), energy(u32), timeover, en_energy, en_multi, mo_time1/2, mo_out1/2; if comm changed → live+FRAM+`hook_comm_reconfigure`; if ether changed → live+FRAM+`hook_ether_apply`; lcd_status=LCD_RUN_MULTI.
- **HAND path**: as MULTI + on_time; does NOT save comm/ether; lcd_status=LCD_RUN_MULTI (FLAG: HAND returns to MULTI page — keep).
- **STD path**: MULTI core + f_safty, run_mode, delay1/2/3, trigger2/3; temp_cnt_reset→work_cnt=0+FRAM+echo LV_WORK_CNT; temp_horndown→`hook_horn`; lcd_status=LCD_RUN_STD.

### 8.3 app_config save helpers (new)
Add to `app_config.c`: small field-save helpers (or a `app_config_save_group(...)`) writing the exact `FRAM_ADDR_*`
of each field, so the input layer does not hardcode addresses. Mirrors `app_config_factory_write` field set.

---

## 9. VP macros to add (`fw/include/dgus_lcd.h`)

Port the full set from `ref/samd20/dgus_lcd.h` (page IDs already present): `LCD_LOGO`, `LCD_MODEL_SETUP`,
`LCD_WARNING`(17); VPs `STD_SETUP_PARAM`,`DATA_SAVE`,`MODEL_FREQ`,`MODEL_TYPE`,`COMM_ADDR/SPEED/PARITY`,
`COMM_*_TXT`,`COMM_IP/NM/GW_TXT`,`KEY_MULTI`,`SETUP_MODEL`,`ENERGY_EN`,`SETUP_PARAM`,`SETUP_PARAM_MOOHAN`,
`VAR_AMP/FREQ/ENERGY`,`ICON_OL/OUTERR`,`LV_OUTPUT2`,`LV_TIME/TIME2`,`LV_OUT_POWER`,`LV_ENERGY_VAL`,
`LV_MAX_ON_TIME`,`DISP_MULTI`,`DISP_RUN_MODE`,`DISP_SAFTY`,`DISP_REMOTE`,`LV_LIMIT_OUT_T`,`LV_RUN_MODE`,
`LV_MO_OUT1/2`,`LV_MO_TIME1/2`,`M_DATA_SAVE`,`VP_ERROR_MSG`,`ICON_ERROR_RESET`,`KEY_ERROR_RESET`,`MULTI_EN`,
`DISP_STD_DATA1/2/3`,`DISP_VERSION`,`LV_COMM_MODE`,`LV_ETHER_KEY`,`DISP_COMM_MODE`,`DISP_EN_DHCP`,
`VAR_CAL_VAL`,`VAR_FREQ_CAL_VAL`; keys `KEY_UP=1`,`KEY_DN=0`,`KEY_CANCEL=0`,`KEY_SAVE=1`. Keep existing
16-bit form (`0x0014` not `0x14`) — driver takes `uint16_t vp`.

---

## 10. Stage B fixes — DO NOT regress

- Cold-boot handshake `dgus_wait_ready` + `app_lcd_ensure_run_page` in `app_init` (commits `d5860a3`) — keep. These are STM32-discovered fixes **not present in samd20** (samd20's check was commented out). The ported `SYS_PIC_NOW==0` re-init handler (§7) is complementary (handles mid-run panel reset); ensure the two cooperate (re-init must re-assert the run page, not fight the ensure loop).
- BOOT0→GND boot path (`9bbc505`) — unrelated, unaffected.
- `app_lcd_init_mode` already ports the `init_lcd_mode` VP pre-fill; refactor to call the new `change_lcd_page` (samd20 `init_lcd_mode` ends by calling `change_lcd_page(lcd_status)`), removing the Stage-B inline duplication.

**SYS_PIC_NOW re-init loop guard (mandatory):** every `dgus_set_page` records `state.last_set_page_ms = sys_tick_get_ms()`. The `SYS_PIC_NOW==0` handler (§7) **suppresses re-init if `now − state.last_set_page_ms < 200 ms`** (our own page-set provoked the report) — and only honors it after `app_init` has completed. Without this guard `change_lcd_page → set_lcd_page → panel SYS_PIC_NOW → re-init → set_lcd_page …` is a feedback loop. Cheap to add; must be in the first cut.

---

## 11. Cadence / display step machine

samd20 refreshes the panel via a 10-step `job_state` machine advanced on **odd ticks of a 2 ms timer ⇒ ~4 ms per
step, ~40 ms full cycle** (chunked 5-element writes to avoid saturating the LCD UART). STM32 port: a step counter
advanced on a **4 ms cadence** (matches samd20) in `app_loop_iter` (using `sys_tick_get_ms`), one VP-group per step:

| Step | Write |
|---|---|
| 0–4 | LV_OUTPUT[0..19] in 4×5 chunks (bar from curr_amp vs ref_lv_*; set-point marker at output_power·20/100) |
| 5–6,8,9 | LV_TIME[0..19] in 4×5 chunks (fill i< us_on_time_200m) |
| 7 | VAR_POWER/AMP/FREQ (3-word array) + VAR_ENERGY; DISP_REMOTE on modbus_status change (deferred—skip until Stage C) |

ICONs (RESET/SEEK/RUN/OL/OUTERR) and LV_WORK_CNT are **event-driven** (written when the owning state changes),
not in the step machine. With the stub measurement provider returning 0, bars render empty and VAR_* show 0 —
exactly the "alive but idle" UI the user approved.

**Error page (`LCD_WARNING`=17) entry** is also event-driven: `error_status` 0→non-zero → `dgus_set_page(LCD_WARNING)`
+ `VP_ERROR_MSG` text; cleared by `KEY_ERROR_RESET` (§7). `error_status` is control-fed (stub provider §4.3), so
this path is **unreachable until Stage D** — but the code path is implemented now (the clear side already is via
KEY_ERROR_RESET). Wiring it now keeps the error UX whole when Stage D feeds real faults.

---

## 12. Open flags (code does not block — annotate in source)

- **F1 DAC zero-point**: output_power=50→DAC byte 0; confirm with HW whether 50% is intended zero (§5).
- **F2 I2C_POT identity** (U4 @0x28) — NEXT_STEPS open question; hook stubbed.
- **F3 HAND save → MULTI page** (§8.2) — keep verbatim, verify intent on HW.
- **F4 CANCEL comm-mode/ether** not restored from FRAM (§8.1) — replicate latent quirk.
- **F5 `dgus_frame_t.data[]` layout** — confirm payload offset before dispatch (§3).
- **F6 DGUS touch report `cmd`** — confirm panel sends 0x83 for touch auto-upload (samd20 assumes this); verify on HW with the existing RX trace.

---

## 13. Build & verify plan

1. **Build gate**: `env -u STM32_TOOLCHAIN cmake --build build` — 0 warning (`-Wall -Wextra -Wundef -Wshadow`).
2. **Static**: dispatch table covers every VP in §7; no hardcoded FRAM addresses outside `app_config`/`fram.h`.
3. **HW bring-up** (board available):
   - Boot → RUN page renders (Stage B parity).
   - Touch SETUP entry → setup page shows current limits; edit a value; SAVE → power-cycle → value persists (FRAM determinism, like Stage B factory test).
   - CANCEL → value reverts to FRAM.
   - Page navigation across all setup/comm/ethernet pages — no lockup.
   - Comm/ethernet edits render & persist (no live apply this stage — verify via `[lcd]`/`[cfg]` mon trace + reboot read-back).
   - Hooks emit `mon_printf` on RUN/SEEK/RESET/SAVE — confirms wiring without driving hardware.
   - (Optional) compile-flag measurement ramp → LV_OUTPUT/LV_TIME bars sweep, VAR_* count.
4. **No-regression**: cold-boot logo→run still correct (`run_page_confirmed=1`).

---

## 14. Deliverables

- `fw/include/dgus_lcd.h` — full VP macro set (§9).
- `fw/src/app_lcd.c` — state owner, top-level dispatch/tick, stub hooks, refactor init_mode→change_lcd_page.
- `fw/src/app_lcd_render.c` (+ proto in `app_lcd.h`) — change_lcd_page (§6).
- `fw/src/app_lcd_input.c` — parse dispatch + DATA_SAVE/CANCEL (§7,§8).
- `fw/src/app_lcd_disp.c` — display step machine + ICON/work_cnt (§11).
- `fw/src/app_lcd_str.c` — formatters.
- `fw/src/app_config.c` — field-save helpers (§8.3).
- `fw/src/app.c` — `app_loop_iter`: replace RX debug print with `app_lcd_input_dispatch`; add display step tick.
- `fw/CMakeLists.txt` — new sources.
- Docs: changelog + NEXT_STEPS update on completion.

**Branch hygiene (decide at review):** this spec's banner branch `feat/stage-d-osc-pin-io` is the retired Stage D
pin-IO branch and already carries the unrelated BOOT0/LCD fix commits — the same control/LCD conflation that
muddied history before. Recommend cutting `feat/stage-lcd-full-behavior` off `main` (cherry-pick or merge the
BOOT0 `9bbc505` + LCD `d5860a3` fixes first) so the eventual PR is clean. User decision required before plan.

> After review → `writing-plans` → implementation (subagent/Codex) → HW verify → merge.
