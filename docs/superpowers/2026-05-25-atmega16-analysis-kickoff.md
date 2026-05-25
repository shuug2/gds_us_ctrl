# ATmega16 FW Behavior ↔ I/O Signal Analysis — Session Kickoff

> **This doc IS the spec.** It is an *analysis* task, not a feature build — do **not** run the
> brainstorm→spec→plan→implement cycle. The deliverable is the table in §6, filled in. Optionally one
> short scope pass (§7) before filling.
>
> **Goal**: precisely map what the ATmega16 firmware *does*, correlated to its I/O control signals, as the
> evidence base for later absorbing its real-time ultrasonic-control logic into the STM32F410 (a future
> code slice — NOT this session). See memory `project_atmega16_absorption`.

---

## 1. Critical-reading discipline (NON-NEGOTIABLE)

`ref/atmega16/m16_conv_v001.c` (1309 lines, single file) is a **binary→C reverse-engineering** (`_conv`).
**Treat every behavioral claim as a hypothesis, never as fact.** The decompiler invents variable/function
names, may fuse or split functions, miss tail calls, and leave dead stores.

**Per-function checklist** — run on every function you analyze; **2+ failures → flag the row "decompiler noise":**
- [ ] Register accesses explicit (`PORTB`, `PINC`) vs raw `*(volatile uint8_t*)0x38`?
- [ ] Function name matches what the code actually does, or is it decompiler invention?
- [ ] Magic constants (`0x10`, `0xAA`, bit masks) explained, or copy-paste artifacts?
- [ ] Control flow plausible — no unreachable branches / dead stores?
- [ ] Any I/O op **contradicts** the user's pin-role statement or the V30 netlist?

Findings feed §6 with an explicit **Confidence** and a concrete **HW-verify method** so real-hardware
testing can later confirm or correct them.

---

## 2. Confidence criteria (explicit — not gut feel)

| Level | Criteria |
|-------|----------|
| **HIGH** | DDR direction + register read/write pattern explicit in code **AND** matches a V30 netlist signal **AND** matches the user's pin-role statement (three-way confirmation) |
| **MEDIUM** | two of the three confirmations, or the third is inferred but consistent |
| **LOW** | single source; decompiler-named variable; behavior inferred from function name only; **or any conflict between sources** |

---

## 3. HW-verify method (fixed set — pick one per row, no free text)

- `scope <pin/net>` — capture edge timing / levels
- `openocd mdb/mdw <reg>` — read GPIO IDR/ODR / PORT/PIN register live
- `gdb watch <reg>` — break on access to a memory-mapped register
- `instrument` — add a `mon_printf` at the moment of interest in the STM32 port
- `manual stimulus` — assert a connector pin, observe MCU response

> ⚠️ Running the STM32 on the bench requires the **BOOT0 forced-jump workaround** (R64 unpopulated) —
> see RESUME / memory `project_board_boot0_workaround`. The ATmega16 itself can be probed on the *old*
> board if available.

---

## 4. Scope

**In scope** — I/O-control-relevant behavior:
- ADC sensing: `adc_process_channels`, `adc_calc_scaled_output`, `ISR(ADC_vect)` (current/power feedback)
- Output / ultrasonic drive control: `output_level_process`, `set_output_mode_{a,b,c}`, `set_output_full`,
  `set_output_reduced`, `compare_with_scaled`
- Main control flow: `main_loop_process`, `ISR(TIMER0_OVF_vect)`, `ISR(TIMER1_OVF_vect)`, `hw_init`
- All GPIO directions + the signal each pin carries

**Out of scope** (skip — don't burn turns analyzing):
- `display_multiplex` / `display_set_segments` — 7-seg display, **obsolete** (DGUS LCD replaces; netlist has no 7-seg).
- `comm_sequence_handler` / `comm_start_sequence` — inter-MCU GPIO signaling to SAMD20; V30 is single-MCU so
  this IPC path no longer exists. **Skip the protocol semantics**, but **do record the GPIO pin directions**
  (what was once IPC is now internal STM32 state).
- `btn_scan_and_debounce` / `btn_check_repeat` — note presence + which port; skip behavior unless V30 has an
  equivalent input.

(Confirm scope in §7 if unsure.)

---

## 5. Seed inventory (starting material)

**Function map** (`m16_conv_v001.c`):
`ISR(ADC_vect)`:276 · `btn_scan_and_debounce`:322 · `btn_check_repeat`:411 · `ISR(TIMER0_OVF_vect)`:433 ·
`ISR(TIMER1_OVF_vect)`:506 · `adc_process_channels`:612 · `adc_calc_scaled_output`:672 ·
`main_loop_process`:718 · `display_multiplex`:808 · `display_set_segments`:857 · `output_level_process`:881 ·
`compare_with_scaled`:1066 · `set_output_{mode_a:1038,mode_b:1045,mode_c:1052,full:1059,reduced:1072}` ·
`comm_sequence_handler`:1084 · `comm_start_sequence`:1135 · `hw_init`:1146 · `main`:1267

**I/O register usage** (ref counts): PORTB ×18, PORTD ×13, PINC ×12, PORTC ×10, PORTA/PINA ×3, DDR{A,B,C,D}.
→ PORTB/PORTD are output-heavy (likely OSC drive + old display); PINC/PINA are inputs.

**User-confirmed ATmega16 pin roles** (ATmega16 side, 2026-05-25):
- PA4 ← ultrasonic output-start signal **input**
- PC0 → overload **output**
- PC1, PC4 ← ultrasonic board signal **input**

**V30 STM32 (new board) candidate signals** to correlate against (`hw/schematics/USW_CTRL_V30.asc`, U2 pins):
- inputs/optos: `B_START`(U2.50), `B_OVLD`(U2.55), `B_RESET`(U2.51), `B_UVOUT`(U2.56), `B_ESTOP`(U2.52),
  `KEY_START1/2`(U2.53/54), `CHK_FREQ`(U2.14), `SENS_CURR`(U2.27), `SENS_OUT`(U2.26), `SENS_UP/DN`(U2.45/44)
- outputs: `OSC_OUT0..4`(U2.28/29/33/34/35→CN2), `SOL_DN`(U2.57), `CON_OVLD`(PB3 per pinmap)

⚠️ **PORTB/PORTD outputs have NO user-provided role** — the OLD↔NEW mapping for the OSC drive + other
outputs must be derived this session from code + V30 netlist, and is **partial without the OLD-board
schematic** (see §8).

---

## 6. DELIVERABLE — fill this table (one row per ATmega16 I/O pin)

Save as `docs/superpowers/analysis/atmega16-io-behavior.md` (create dir). Blank cell = TODO. Update
Confidence + Status as HW tests land (`proposed → HW-verified → ported`).

| ATmega16 pin | Dir | Old fn(s) touching it | Inferred behavior | Conf | V30 STM32 net | HW-verify method | Status |
|---|---|---|---|---|---|---|---|
| PA4 | IN | _(trace)_ | ultrasonic output-start signal in (user) | M | _(B_START? KEY_START?)_ | manual stimulus / openocd PINA | proposed |
| PC0 | OUT | _(trace)_ | overload out (user) | M | _(B_OVLD? CON_OVLD/PB3?)_ | scope / openocd ODR | proposed |
| PC1 | IN | _(trace)_ | ultrasonic board signal in (user) | M | _(CHK_FREQ? SENS_*?)_ | scope / manual stimulus | proposed |
| PC4 | IN | _(trace)_ | ultrasonic board signal in (user) | M | _(SENS_*?)_ | scope / manual stimulus | proposed |
| PORTB.0..7 | OUT? | `set_output_*`, `display_*` | OSC drive + (old) display — split needed | L | `OSC_OUT0..4`? | scope OSC_OUT | proposed |
| PORTD.x | ? | _(trace ISRs/main)_ | _(TODO)_ | L | _(TODO)_ | _(TODO)_ | proposed |
| PINA.x (ADC) | IN(A) | `adc_process_channels` | current/power sense | M | `SENS_CURR/SENS_OUT`? | scope / openocd ADC | proposed |
| _(enumerate remaining PORTC/PIN bits)_ | | | | | | | |

(First task: enumerate **every** DDR/PORT/PIN bit actually used in the code → one row each. The rows above are seeds, not the full set.)

Alongside the table, capture: the **control-loop semantics** (how `adc → compare_with_scaled → set_output_*`
forms the ultrasonic power-regulation loop) and the **timer/ISR cadence** (TIMER0/1 roles), each with
confidence + how to verify. These are the heart of what STM32 must absorb.

---

## 7. Session entry procedure

1. Read this doc (auto-loaded context will point here from RESUME).
2. *(optional, ≤1 short pass)* scope confirmation only — include `comm_*` GPIO directions? include button
   ports? Decide, don't redesign. No full brainstorming cycle.
3. Create `docs/superpowers/analysis/atmega16-io-behavior.md`, enumerate all used I/O bits → table rows.
4. Trace each function (with the §1 checklist), fill Inferred behavior + Conf + V30 net + HW-verify.
5. Write the control-loop + ISR semantics summary.
6. Commit the analysis doc. (No code changes this session — analysis only.)

---

## 8. Open ask for the user (decides analysis precision)

**Will you add the OLD-board (samd20+atmega16) schematic before this session?**
- **Yes** → closes the OLD ATmega16-pin ↔ net mapping decisively; most rows reach HIGH/MEDIUM.
- **No** → analysis still proceeds, but PORTB/PORTD output rows and several inputs stay LOW/MEDIUM until
  HW testing. (User earlier offered to add the old schematic — `usw_ctrl_v2x_samd20.*`.)

---

## 9. References

- Target: `ref/atmega16/m16_conv_v001.c` (read-only, decompiled — critical reading per §1)
- New board: `hw/schematics/USW_CTRL_V30.{asc,pdf}`
- Pin roles + absorption context: memory `project_atmega16_absorption`
- Bench/flash caveat: memory `project_board_boot0_workaround`, RESUME §보드 이슈
- samd20 side (how SAMD20 drove the LCD/comm): `ref/samd20/main.c`
