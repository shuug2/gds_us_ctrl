# ATmega16 Regulation Core — VERIFIED ANALYSIS (Stage D slice 1 evidence base)

> Status: evidence base only. This document does **NOT** write the STM32F410 spec and does **NOT** make the image-undecidable design decisions — those are enumerated for the user in §5.
> Source of truth: `ref/atmega16/M16_reverse/out/01_disassembly.asm` (3417 lines, `m16_controller_prg.hex` linear disasm), cross-checked against `03_behavior_spec.md`, `_func_facts.json`, `05_sram_map.md`. Board-truth references: `docs/pinmap.md`, `docs/superpowers/analysis/atmega16-io-behavior.md`.
> Filing rule applied mechanically: bucket placement is governed by each finding's **verdict**, not its `kind` tag. `confirmed` slice-1 → §3 (bucket A). `undecidable_from_image` → §5 (bucket B), even where tagged `register_fact`. `refuted` → §4 + listed as an overturned correction.

---

## 1. One-line conclusion

The ATmega16 regulation core (slice 1) is fully recovered from the flash image as a deterministic SRAM-to-SRAM pipeline — **2-channel single-shot ADC (ch0 mean-of-10 / ch1 mean-of-50 @ 2.56 V internal ref) → `0x1A5C` clip/floor/×6 scaling → `0x138C` 21-rung monotone-lookup thermometer pattern**, all driven unconditionally every ~2.05 ms Timer0 tick — but **where that computed drive level physically reaches the 5 OSC channels is the one seam absent from the image** and must be decided by the user before wiring those outputs.

---

## 2. Slice-1 vs Slice-2 boundary (explicit)

**SLICE 1 — regulation core (this document's bucket-A facts):**
1. ADC acquisition + averaging primitive: ISR `0x00EA` 2-channel alternating single-shot, results to `0x0160`/`0x0162`, 32-bit accumulate, ch0 mean-of-10 → `ch0_avg @0x01B9`, ch1 mean-of-50 → `ch1_avg @0x018D`.
2. Scaling transfer `adc_calc_scaled_output @0x1A5C`: gate (cnt≥11) → input copy → input-domain clip(≥1000→1000)/floor(<3→0) → normal-range `input×6` → `adc_scaled_value @0x01B5`.
3. Output level `output_level_process @0x138C`: copy 21 of 24 lookup words from flash `0x58`, select first `table[i] < adc_scaled_value`, emit the thermometer pattern to `0x019F`/`0x01A0`/`0x01A1`.
4. Cadence: Timer0 OVF ISR `@0x280` calls `0x1A5C` then `0x138C` (then `0x1692`) **unconditionally every tick** (TCNT0=0xF0 → 16 counts → ~2.05 ms @8 MHz), before the `g_main_state` branch — so the regulation compute runs in **both** states.

**SLICE 2 — NOT this document (confirmed but out of scope, noted here for the boundary only):**
- **Soft-start ramp** `app_0x1226` (cadence-C8): a 10-rung threshold ladder on a 16-bit tick counter, paced by the Timer1 OVF ISR (`@0x3FE`, TCNT1=0xFFB1 → 79 counts → ~10.1 ms), which increments the counter `@0x0173` only while `g_main_state != 0`. Writes the SAME pattern vars `0x019F/0x01A0/0x01A1`. Duration ≈ 401 × ~10 ms ≈ 4 s.
- **State handoff** `g_main_state @0x0195` (cadence-C6): init=1 `@0x1B8A`; cleared to 0 `@0x137C` when ramp counter ≥ 0x191 (401). One-way transition. THE SLICE LINE is the handoff on the shared pattern vars: ramp drives them while state==1; lookup regulation drives them while state==0.
- **Secondary per-state handlers** (cadence-C7): Timer0 ISR `@0x324` branch — state≠0 → `0x15BC` (reads PINC, writes `0x0179/017B/017C/017D`); state==0 → `0x1066` (reads PINC, writes PORTC bit1) + eeprom-test `0x1BF8` → conditional `0xC36`. Neither writes the regulation/output-pattern vars. Command/IPC + overload + blink concerns.
- **Display render path** `display_multiplex @0x1692` / `0x2064` / PORTD — see §3e; in-image active but physically dead on V26 (§5 seam).

Function names (`adc_calc_scaled_output`, `output_level_process`, `lookup_table`, `output_direct_process`) are v001/seed-derived; the raw disasm proves addresses/data/control-flow, not symbolic names.

---

## 3. VERIFIED REGISTER FACTS (bucket A — the spec implements these)

Confidence column: **HIGH** = byte-exact disasm, exhaustive/unique grep where claimed. Where a fact carries an interpretation layer (datasheet decode, schematic label, v001 name), the image-proven part is separated from the interpretation explicitly.

### 3a. ADC acquisition (channels, 10/50 averaging, ADMUX/ADCSRA)

| ID | Statement | Disasm evidence (byte addr) | Conf | Verdict |
|----|-----------|------------------------------|------|---------|
| ADC-1 | ISR_ADC reads ADCL(IO 0x04)/ADCH(IO 0x05) → result array `0x0160 + 2*idx`; idx0→`0x0160/61`, idx1→`0x0162/63`. | ISR body `0xEE` `lds r30,0x021E`(idx); `0xF2/F4` X=0x0160; `0xF8/FA` Z=idx*2; `0xFC/FE` X+=idx*2; `0x100/102` in ADCL/ADCH; `0x104/106` st X+. Wrap `0x110 subi r26,0xFF`/`0x116 cpi 0x02`/reset → idx∈{0,1}. | HIGH | confirmed |
| ADC-2 | ch0 (idx==0): += into 32-bit accumulator `0x01A9-0x01AC`, 16-bit counter `0x01BB`; at counter≥10 (`sbiw r26,0x0a`) divide by 10 → `ch0_avg @0x01B9/0x01BA`, then zero acc+counter. | Fn `@0x196C`: `196C and r10/breq`→ch0 `1974 lds 0x0160`; accumulate via `20BA/20D0/20E2`→32-bit add `0x2144`; store `1988-1994`; counter++ `0x1BEC`; `19A8 sbiw r26,0x0a`; divide `0x2164`; store `19C4/19C8 sts 0x01B9/0x01BA`; zero `19CC-19E2`. | HIGH | confirmed |
| ADC-3 | ch1 (idx!=0): separate 32-bit accumulator `0x01AD-0x01B0`, counter `0x01BD:01BE`; at ≥50 (`sbiw r26,0x32`) divide by 50 → `ch1_avg @0x018D/018E`, then zero. Counts ch0=10/ch1=50; targets `0x01B9`/`0x018D`. | CH1 block `0x19E8-0x1A5A`: sample `19E8 lds 0x0162`; acc `19FC-1A08 sts 0x01AD..0x01B0`; `1A1C sbiw r26,0x32`(50); divide `0x2164`; `1A38/1A3C sts 0x018D/018E`. | HIGH | confirmed |
| ADC-4 | Both averages use a true 32-bit restoring division (32 shift-subtract-restore iterations) `@0x2164`; divisor 10 or 50, dividend the 32-bit accumulator. Accumulators 4-byte; sums never overflow (50×1023 fits in 16 bits). | `0x2164`: `ldi r19,0x20`(32 iters) + 4-byte sub/sbc chain `2184-218A` + restore `218E-2194` (= `__udivmodsi4`). Divisor sites: `19B8 ldi 0x0A`, `1A2C ldi 0x32`. | HIGH | confirmed |
| ADC-5 | Channel select: ISR `ADMUX=(idx-0xC0)` then `ADMUX|=0xC0`; MUX[4:0]=idx → idx0=ADC0(PA0), idx1=ADC1(PA1). Array index == MUX value, alternating 0/1. | `012A lds idx`; `012E subi r30,0xC0`; `0130 out 0x07`; `0132 in`; `0134 ori 0xC0`; `0136 out 0x07` → final ADMUX=idx\|0xC0. | HIGH | confirmed |
| ADC-6 | Reference: **first** conversion (hw_init) is acquired under REFS=01; **every ISR re-arm** forces REFS=11 before ADSC, so all steady-state averaged samples use REFS=11. | hw_init `1B44 ldi 0x40`/`1B46 out 0x07` (REFS=01) + `1B48 ldi 0xCD`/`1B4A out 0x06` starts conv0. ISR final ADMUX `0x0136`=0xC0\|idx; ADSC set only after (`013A out 0x06`=0xCF). **Interpretation layer:** REFS=11→internal 2.56 V, REFS=01→AVCC are ATmega16-datasheet decodes of the bit patterns, not bytes in the image. Full-scale = 2.56V/1023 per LSB in steady state. | HIGH (bits) / decode (voltages) | confirmed |
| ADC-7 | Single-shot with software re-arm, NOT auto-trigger/free-running. ADCSRA=0xCD (init) and 0xCF (ISR) both have ADATE(bit5)=0; ISR sets ADSC(bit6)=1 each conversion. ADIE/ADEN=1 throughout. | Only two writes to IO 0x06 in image (`out 0x06` → lines 112, 2346). 0xCD/0xCF both bit5=0. No `sts 0x0026`, no `sbi/cbi 0x06` anywhere (exhaustive). | HIGH | confirmed |
| ADC-8 | ADC clock prescaler: hw_init 0xCD → ADPS=101=/32 (first conversion only); ISR 0xCF → ADPS=111=/128 (all steady-state). | Same two writes. 0xCD bits2:0=101; 0xCF bits2:0=111. (Per-conversion clock counts 25/13 are datasheet constants; absolute µs is undecidable — see §5 cadence-C9.) | HIGH | confirmed |
| ADC-9 | SFIOR written 0x00 once in hw_init (the seed's "0x30" is the SFIOR I/O **address**, not a value). With ADATE=0 the ADTS field is don't-care. | `1B40 ldi 0x00`/`1B42 out 0x30,r30`. Only write to SFIOR (no `sts 0x0050`). | HIGH | confirmed |

**ch0_avg is the slice-1 regulation input** consumed downstream by `0x1A5C`. ch1_avg (50-sample) is genuinely computed in-image (branch B exists/executes) but its physical identity is open (§5 B4).

### 3b. Scaling transfer — `adc_calc_scaled_output @0x1A5C` exact ops

| ID | Statement | Disasm evidence | Conf | Verdict |
|----|-----------|-----------------|------|---------|
| SCALE-01 | Gate: runs only when `timer0_process_cnt` (16-bit `@0x01BF:0x01C0`) ≥ 11; else immediate return. | `1A5C/1A60 lds 0x01BF/0x01C0`; `1A64 sbiw r26,0x0b`; `1A66 brcc →0x1A6C`; `1A68 jmp 0x1AD0`(=`ret`@1AD0). | HIGH | confirmed |
| SCALE-02 | Past the gate, cnt is clamped to exactly 11 (`ldi 0x0B/0x00; sts 0x01BF/0x01C0`). Never reset to 0; this fn is the only writer of `0x01BF/01C0`. | `1A6C ldi 0x0B`/`1A6E ldi 0x00`/`1A70 sts 0x01BF`/`1A74 sts 0x01C0`. | HIGH | confirmed |
| SCALE-03 | `adc_ch0_result (0x01C1:0x01C2) := ch0_avg (0x01B9:0x01BA)`, 16-bit copy before scaling; all compares read this copy via getter `0x213A`. | `1A78/1A7C lds 0x01B9/01BA`; `1A80/1A84 sts 0x01C1/01C2`. Getter `213A lds 0x01C1/01C2; ret`. Unique writer/reader (grep). | HIGH | confirmed |
| SCALE-04 | **INPUT-domain max clip**: if input ≥ 0x03E8 (1000) → `adc_scaled_value (0x01B5:01B6) := 0x03E8` and return. Test on input, not output. | `1A88 call 0x213A`; `1A8C cpi r26,0xE8`/`1A8E ldi r30,0x03`/`1A90 cpc r27,r30`; `1A92 brcc →0x1A98`; `1A98/1A9A ldi 0xE8/0x03`; `1A9C/1AA0 sts 0x01B5/01B6`. | HIGH | confirmed |
| SCALE-05 | **INPUT-domain floor**: else if input < 3 → `0x01B5:01B6 := 0` and return. | `1AA6 call 0x213A`; `1AAA sbiw r26,0x03`; `1AAC brcs →0x1AB2`; `1AB2 ldi 0x00`/`1AB4/1AB8 sts 0x01B5/01B6`. Floors 0/1/2; value 3 falls through (no off-by-one). **Note:** the name "adc_ch0_result" is the C hypothesis; the compared value is provably the return of `call 0x213A` (same source as the ceiling check). | HIGH | confirmed |
| SCALE-06 | **Normal range (3 ≤ input < 1000)**: `adc_scaled_value := input × 6` (16-bit). A MULTIPLY by immediate 6, NOT a shift. Output range here [18, 5994], unbounded by 1000. | `1AC2 ldi r30,0x06`; `1AC4 call 0x2158`; helper `2158 mov r22,r30/mul r22,r26/movw r30,r0/mul r22,r27/add r31,r0` = `(r26:r27)*6 mod 2^16`; `1AC8/1ACC sts 0x01B5/01B6`. | HIGH | confirmed |
| SCALE-07 | `0x1A5C` does **not** touch the flash lookup_table. Calls only `0x213A` (SRAM getter) and `0x2158` (mul). The `lpm` flash read `@0x21F0` is invoked by `0x138C` (`call @0x13A4`), a different function. | Body `0x1A5C-0x1AD0`: only calls = `0x213A`(×3), `0x2158`(×1); no lpm/spm; all branch targets stay inside the body. AVR's only flash-read opcode is `lpm` → absence across the closed call-tree is exhaustive. | HIGH | confirmed |

Net transfer (portable as a pure function `f(ch0_avg) → adc_scaled_value`):
```
in = ch0_avg                  // 16-bit, raw ADC counts
if (in >= 1000) out = 1000
else if (in < 3) out = 0
else             out = in * 6 // 16-bit, range [18, 5994]
```

### 3c. Output level — `output_level_process @0x138C` lookup-compare + setter semantics

| ID | Statement | Disasm evidence | Conf | Verdict |
|----|-----------|-----------------|------|---------|
| C1 | A flash PROGMEM table of 24 16-bit LE words at flash byte `0x58` (values in §4); strictly DECREASING for idx 0..20. The single routine `@0x21F0` (called once, `@0x13A4`) copies ONLY the first 21 entries (`r24=0x2A`=42 bytes) into a stack Y-buffer via lpm/st loop. | Table bytes `0x58-0x88` (asm L30-33, byte-decoded). Copy: `13A0 ldi r30,0x58`/`13A2 ldi r31,0x00`(Z); `139A ldi r24,0x2A`; `1398 sbiw r28,0x2a`(alloc); loop `0x21F0` add/lpm/st/dec/brne. **Names "lookup_table"/"output_level_process" are v001-derived, not in raw disasm.** | HIGH (data) | confirmed |
| C2 | Selector finds the FIRST i (0..20) where `table[i] < adc_scaled_value` (strictly-less-than, NOT '>'). | Helper `0x202A`: `lds r26,0x01B5`/`lds r27,0x01B6`; `cp r30,r26`/`cpc r31,r27`(= table[i]−adc). Caller `13AC call 0x202A`; `13B0 brcs →handler`. AVR C set ⇔ table[i] < adc → brcs taken. (Resolves v001 internal inconsistency, §4.) | HIGH | confirmed |
| C3 | `0x138C-0x15BA` writes ONLY three SRAM display vars: `0x019F` (pattern), `0x01A0` (digit0), `0x01A1` (digit1/mode). Does NOT write `0x0189/018A/0196` (the OSC-mirrored flags). | Direct inline: `0x019F`×7 (`144E..14F6`), `0x01A0`@`141A`, `0x01A1`@`1420`; plus setters `0x2024/2038/200A/2016`. `grep "sts 0x(0189\|018A\|0196)"` in range = ZERO; all 26 OSC-flag writes are elsewhere. (`0x21F0` writes 42 bytes but to the local stack frame, not a global.) | HIGH | confirmed |
| C4 | set_output family: `0x2022`→`0x019F=0xFF` (leaves r30=0xFF); `0x2038`(v)→`0x019F=v`, loads r30=0, falls into `0x200A`; `0x200A`(v)→`0x01A0=v,0x01A1=0x00`; `0x2016`(v)→`0x01A0=v,0x01A1=0x03`; `0x1FFE`(v)→`0x01A0=v,0x01A1=0x30`. At i=0-3 the disasm calls `0x2022` then `0x2016` with NO ldi between → r30 still 0xFF → `0x01A0=0xFF,0x01A1=0x03`. | Setter bodies L2742-2764 (byte-decoded). i=0 path `13B6 call 0x2022`/`13BA call 0x2016` (no intervening ldi). | HIGH | confirmed |
| C-LADDER (critic gap) | **Full 21-rung i→pattern thermometer** (two-stage): i=0-4 → `0x019F=0xFF` full + `0x01A0=0xFF`, `0x01A1`=0x03(i0-3)/0x01(i4); i=5-12 → hold `0x019F=0xFF` while `0x01A0` drains 0xFF→0x7F→0x3F→0x1F→0x0F→0x07→0x03→0x01 (`0x01A1=0x00`); i=13-20 → drain `0x019F` itself 0xFF→…→0x01 (also zeroes `0x01A0/01A1`); no-match fall-through `@0x15B2` → `0x019F=0x00` (output off). The 21 compare-branches map 1:1 onto the 21 copied words. | Fn body L1643-1841: all 21 `brcs` branches + `0x15B2` fall-through; setters `0x200A/2016/2022/2038` L2742-2764. **GATED on §5 seam:** matters only if the pattern bytes (not just the band index i) are the real output. Non-blocking. | HIGH (in-image) | confirmed |
| C6 | Timer0 ISR `@0x280` mirrors THREE binary SRAM flags to mapped pins **ACTIVE-LOW / idle-HIGH**: `0x0189`→PORTC bit7 (cbi 0x15,7 when flag≠0; sbi when ==0), `0x018A`→PORTB bit1, `0x0196`→PORTB bit0. Flag set → pin LOW. | `02E2 lds 0x0189`/`02E8 brne →0x2EE`/`02EE cbi 0x15,7`/`02F2 sbi 0x15,7`; `02F4..` PB1; `0306..` PB0. (PB5 `@0x2B0/0x2D8` is a separate counter-threshold strobe off g_01B7, NOT a regulation flag.) **Channel labels OSC0/1/4 are schematic-level.** | HIGH | confirmed |
| C7 | DDR/PORT init `@0x1AD2`: **3 binary OUTPUTS (PB0/PB1/PC7), 2 INPUTS (PC4/PA7)**, plus a separate display pattern — NOT "5-bit pattern → 5 OSC lines". DDRA=0x00 (PA7 in, PORTA=0xFC pull-up); DDRC=0xC3 (PC4 in bit4=0, PORTC=0xBC pull-up; PC7 out bit7=1 idle HIGH); DDRB=0xFF, PORTB=0xDF (PB0/PB1 idle HIGH). | `1AD2-1AE8` byte-decoded (sole DDR writer). PC4/PA7 read as inputs `1072 sbis 0x13,4`, `1688/168A PINA`; never reconfigured to output. | HIGH | confirmed |

### 3d. Cadence (slice-1 only)

| ID | Statement | Disasm evidence | Conf | Verdict |
|----|-----------|-----------------|------|---------|
| cad-C1 | Timer0 OVF ISR (vector `0x0024`→`0x280`) reloads TCNT0=0xF0 unconditionally every entry. 0x100−0xF0 = 16 counts to next overflow. | `0024 jmp 0x280`; `0284 ldi r30,0xF0`/`0286 out 0x32,r30` (before any branch). | HIGH | confirmed |
| cad-C2 | Both timers Normal mode, /1024 prescaler: TCCR0=0x05 `@0x1AF6`; TCCR1A=0x00 `@0x1B00`, TCCR1B=0x05 `@0x1B04`; TIMSK=0x05 (TOIE0+TOIE1) `@0x1B20`. Licenses "reload→period". | Each register written exactly once (grep). Bit decode: CS=101=/1024, WGM=0=Normal. | HIGH | confirmed |
| cad-C3 | hw_init sets a one-shot FIRST Timer0 reload TCNT0=0x9B `@0x1AFA` (101 counts), distinct from steady 0xF0. First overflow delayed ~101 ticks. | `1AF8 ldi 0x9B`/`1AFA out 0x32`. `grep out 0x32` = exactly 2 hits (steady 0xF0 + this). TIMSK armed after seed. | HIGH | confirmed |
| cad-C4 | Timer0 ISR calls `0x1A5C` (@0x318) then `0x138C` (@0x31C) then `0x1692` (@0x320) **UNCONDITIONALLY** every tick, BEFORE the `g_main_state` branch `@0x324`; both arms merge at the `0x348` epilogue/reti. Regulation compute runs in both states. | Calls at L277-279. Enumerated every branch from `0x280`→`0x318`: max target `0x316`, no branch reaches `0x31C+`, no ret/reti before `0x34C`. **Function names + "regulation" semantics are interpretive;** `_func_facts.json` shows `0x1A5C`/`0x138C` touch only SRAM globals, no I/O register, so "ADC scaling" semantics are not register-established. | HIGH (structure) | confirmed |

Absolute timing: Timer0 16 ct → ~2.05 ms, Timer1 79 ct → ~10.1 ms **assuming F_CPU=8 MHz** — but the µs figure is image-undecidable (§5 cadence-C9). The /1024 ratio and the 16/79 reload counts ARE in the image.

### 3e. Output routing AS SEEN IN THE IMAGE

| ID | Statement | Disasm evidence | Conf | Verdict |
|----|-----------|-----------------|------|---------|
| C5 | Every READER of `0x019F` in the image is the display render helper `@0x17AC` (called from `display_multiplex @0x1692`), which feeds PORTD (`out 0x12`) + digit-selects PORTB bits2/3/4 (sbi/cbi `0x18,2/3/4`). NO instruction routes `0x019F` to any OSC-mapped pin (PORTC bit7 / PORTB bit0/bit1). | Exhaustive grep on `0x019F`: 18 writers, 2 readers, both inside `0x17AC` (`17AC lds 0x019F`…`17BA lds 0x019F`/`17BE call 0x2064`→`206C out 0x12`). OSC pins driven by separate immediate/sbi-cbi code that never reads `0x019F`. | HIGH | confirmed |
| C8-routing (was claimed "undecidable", REFUTED) | The firmware DOES decode the routing: `0x019F` is read back (`0x17AC/17BA`), passed through an explicit non-inverting per-bit identity map `0x16F6` (all 8 bits, active-high, bit0 forced high by `ori 0x01`@0x17B0), stored to `0x01D3`, written to PORTD (`out 0x12 @0x206C`). `@0x1692` sets DDRD=0xFF and multiplexes PORTD across 3 phases (`0x019F`,`0x01A0`,`0x01A1`) round-robin, using PORTB[2:4] as per-channel **strobes** (not "3 unrelated flags"). Pattern is **8 thermometer levels** (0x01,0x03,…,0xFF), not a 5-bit/5-OSC count. | `0x16F6` L1953-2034 (8× andi/ori bit copy); `17A0 sts 0x01D3`; `2064/2068/206C`; `1692/169E-16A2 sbi 0x18,2/3/4`; `16B0/16C0/16D0` phase calls. | HIGH | refuted (the "absent/dead-var" framing) — routing IS in image; see §4 |

**Reconciliation of C5 and C8-routing (REQUIRED — they collide at face value):** Both are true at different layers. *In-image (bucket A):* the firmware actively drives PORTD with the pattern via a 3-phase multiplex; the routing, bit-order (identity), and active-level (active-high) are all decidable. *Board-truth (bucket B):* per `atmega16-io-behavior.md §0.1`, that PORTD/7-seg path is physically UNCONNECTED (dead legacy) on the V26 board. Therefore the firmware-decoded PORTD routing is real but does **not** reach the OSC channels on the board — **the actual OSC drive path is the seam the user must resolve (§5).** The only residue still undecidable is whether those specific PORTD pins / PORTB[2:4] strobes wire to the 5 OSC channels (vs a bargraph display).

---

## 4. RESOLVED DISCREPANCIES (v001 vs reconstruction; disasm adjudication)

### 4.1 Confirmed adjudications (image settles a prior ambiguity)

- **ADC reference (REFS bits)** (adc-acq-disc0): ISR `0x0134 ori r30,0xC0` forces REFS=11 every steady conversion; hw_init `0x40` sets REFS=01 for conv0 only. Settles v001's "11 → 2.56V or AVCC" ambiguity (cannot be AVCC since REFS=11 ≠ AVCC pattern 01) and confirms recon's guess that REFS bits are involved. *Voltage labels are datasheet decodes, not image bytes.*
- **Conversion mode** (adc-acq-disc1): ADATE=0 in both writes (0xCD, 0xCF), never set anywhere → **single-shot software re-arm, NOT free-running**. v001's bit decode correct; `atmega16-io-behavior.md L140` "free-run (~208µs/conv)" wording is imprecise/refuted.
- **Accumulator width** (adc-acq-disc2): genuine 32-bit accumulators (`0x01A9-AC` ch0, `0x01AD-B0` ch1), 32-bit add `0x2144`, 32-bit `__udivmodsi4` `0x2164`. v001's two-16-bit-halves (zeroes both) interpretation matches; recon under-decoded (mislabeled `0x018D` as unknown 16-bit counter when it is the ch1 average result).
- **Clip/floor domain** (scaling-disc1 / output-path adjudication): both ≥1000 clip and <3 floor are tested on the INPUT, before ×6. Output is NOT bounded to 1000 (normal-range reaches ~5994). Any port must clip/floor on the input, then multiply.
- **Timer0 OSC-mirror polarity** (output-path-disc0 / cadence-disc0): `cbi 0x15,7 @0x2EE` (LOW) on flag≠0, `sbi @0x2F2` (HIGH) on flag==0 → **ACTIVE-LOW / idle-HIGH** confirmed. **v001:454-471 CORRECT; `04_reconstruction.c:175-177` is polarity-INVERTED** (writes HIGH when flag set).
- **Compare direction** (output-path-disc1): `cp/cpc @0x202a` + caller `brcs` = carry set ⇔ `table[i] < adc_scaled_value`. v001's loop comment `table[i] > adc` (L894) is the error; v001's own helper `compare_with_scaled` (L1066-1068, `<`) is correct.
- **Timer1 reload value** (cadence-disc1): `out 0x2d,0xFF` + `out 0x2c,0xB1` → **0xFFB1** (79 counts → ~10 ms @8 MHz). The "**0xB1FF**" in the task seed and `03_behavior_spec` §1 L78/87/107 is **byte-transposed** (low:high swapped); 0xB1FF would imply ~2.56 s, contradicting the 79-count intent. Both C hypotheses render 0xFFB1 correctly.
- **Timer0 state-branch polarity** (cadence-disc0): state≠0→`0x15BC`, state==0→`0x1066`→`0x1BF8`→(breq)`0xC36`. Both hypotheses correct; recon more complete; no polarity conflict here (recon's known polarity error is the `0x2EE/0x2F2` mirror, above).

### 4.2 Refuted prior claims (overturned by disasm) — see `refuted_corrections`

1. **`>>6` shift → actually `×6` MULTIPLY** (scaling-disc0). v001 L695 `adc_scaled_value = (uint16_t)(temp >> 6)` is wrong: NO shift instruction anywhere in `0x1A5C-0x1AD0`; `0x1AC2 ldi r30,0x06` + `0x2158` widening multiply. Shift-count question moot.
2. **"two real call sites for `0x1A5C`" → single call** (scaling-disc2). `0x1A5C` has exactly ONE reference: `call 0x1A5C @0x318` (sole grep hit), inside the Timer0 ISR body (`0x280`→reti `0x34C`). v001 ("@0x318") and recon ("ISR @0x0280") describe the same call at two abstraction levels — not competing callers. Entry path IS decidable.
3. **`set_output_mode_c(0xFC)` → 0xFF register leftover** (output-path-disc2). No `ldi r30,0xFC` on the i=0-3 path; `0x2016` stores the incoming r30 = 0xFF leftover from the preceding `set_output_full (0x2022)` → `0x01A0=0xFF, 0x01A1=0x03`. (Slice-2 display detail; lands on dead display.)
4. **"pattern→OSC routing absent / `0x019F` is a dead var" → routing IS in the image** (output-path C8). See §3e C8-routing: `0x019F`→`0x17AC`→`0x16F6`(identity bit-map)→`0x01D3`→PORTD `out 0x12`, 3-phase strobed by PORTB[2:4]. Only the OSC *physical binding* of those pins remains undecidable.
5. **6-rung ramp ladder → actually 10 rungs** (cadence-C8, slice-2). Thresholds v < {41,81,121,161,201,241,281,321,361,401} = {0x29,0x51,0x79,0xA1,0xC9,0xF1,0x119,0x141,0x169,0x191}. The enumerated {0xC9,0xF1,0x119,0x141,0x169,0x191} is only the upper 6; the four lower onset rungs (0x29,0x51,0x79,0xA1 — the soft-start onset) were omitted.

### 4.3 lookup_table[24] values (flash byte 0x58, confirmed byte-for-byte)

```
idx:  0      1      2      3      4      5      6      7      8      9     10     11
val: 0x03FF 0x03CC 0x0399 0x0366 0x0333 0x0300 0x02CD 0x029A 0x0267 0x0234 0x0201 0x01CE
idx: 12     13     14     15     16     17     18     19     20    | 21     22     23
val: 0x01B9 0x0168 0x0135 0x0102 0x00CF 0x009C 0x0069 0x0036 0x000F| 0x0004 0x0004 0x0054
```
Strictly DECREASING for idx 0..20. Only the first **21** words (idx 0..20, 42 bytes) are copied into the working buffer; idx 21-23 (0x0004,0x0004,0x0054) are present in flash but NOT copied/used by `0x138C`.

---

## 5. OPEN DESIGN DECISIONS for user review (bucket B — image-undecidable)

The spec will FLAG these in comments and NOT block on them. None promoted to bucket-A facts. Each: what IS known, what is NOT in the image, suggested safe default, blocks-slice-1.

### B-SEAM — Where the computed drive level physically reaches the 5 OSC pins (THE central seam)
- **Known (in-image):** the firmware computes the thermometer pattern and drives it to PORTD via a 3-phase multiplex (§3e C8-routing); the Timer0 ISR separately mirrors only 3 binary flags (`0x0189/018A/0196`) ACTIVE-LOW to PORTC bit7 / PORTB bit1 / PORTB bit0 (§3c C6); PC4/PA7 are inputs (§3c C7).
- **NOT in image:** whether the PORTD/7-seg path (board-dead on V26) or the 3 mirrored binary flags or some other route is the intended OSC drive on the integrated board; the 5-bit-pattern → 5-OSC-line 1:1 mapping asserted by prior analysis is a hypothesis it concedes is "outside visible code".
- **Suggested safe default:** treat slice-1 regulation OUTPUT as the **3 confirmed binary OSC channels** (PB1/PB0/PC7 → STM32 PB2/PB10/PB14) driven active-LOW/idle-HIGH from boolean flags; route the analog/level output power via the existing `app_lcd_hook_set_pot(output_power)` seam rather than auto-mapping the thermometer pattern onto GPIOs. Do NOT auto-map the 8-level pattern onto the 5 OSC pins.
- **Blocks slice 1:** YES (the output binding) — surface to user before wiring OSC2/OSC3.

### B-OSC-MAP — 5-bit→5-OSC pin bit-order + active-level + channel identity (C9, ADC-10 part, C8 residue)
- **Known:** pin directions/polarities/init values (§3c C7); active-LOW/idle-HIGH for the 3 driven channels (§3c C6); PORTD identity bit-map is active-high non-inverting (§3e).
- **NOT in image:** the net-to-channel-to-STM32 identity (CTRL_OSC0..4 ↔ mega16 PB1/PB0/PC4/PA7/PC7 ↔ STM32 PB2/PB10/PB12/PB13/PB14) — this is board-truth from `pinmap.md:117-125` (user-confirmed 2026-04-26/05-25), NOT derivable from AVR machine code. (The C9 `register_fact` tag for the identity mapping is an over-claim; identity is image-undecidable.)
- **Suggested default:** wire only the 3 confirmed output channels (PB2/PB10/PB14) idle-HIGH first; keep PB12/PB13 (OSC2/OSC3 ← mega16 PC4/PA7, which are INPUTS in the image) configurable/unwired until direction confirmed — a naive "5 GPIO push-pull outputs" model may not match the original loop topology (possible OSC-board feedback inputs, open Q B1/B2).
- **Blocks slice 1:** partially — the 2 input-side channels block; the 3 confirmed outputs do not.

### B3 — Transfer-function physical meaning of `adc_scaled_value` (SCALE-08)
- **Known:** the arithmetic — clamp to [0,1000] else `input×6`, written to `0x01B5:01B6`, consumed only by the flag-returning comparator `@0x202A`. Decidable.
- **NOT in image:** the engineering units / physical quantity (current-limit setpoint vs frequency-tracking vs duty target). No net or unit encoded in the binary.
- **Suggested default:** carry `adc_scaled_value` as an opaque scaled regulation setpoint; preserve exact arithmetic; bind its physical destination in the STM32 board layer separately.
- **Blocks slice 1:** NO (port the math SRAM-to-SRAM faithfully).

### B4 — ch1 / PA1 (ADC1) physical identity (ADC-10 part)
- **Known:** ch1 is genuinely acquired and 50-sample-averaged (`ch1_avg @0x018D`); branch B exists and executes.
- **NOT in image:** what physical signal PA1/ADC1 carries. Board report: the V26 pin is unconnected; STM32 pinmap tentatively maps PB1/ADC1_IN9 = ADC_OSC (oscillator monitor).
- **Suggested default:** implement the ch1 acquisition + 50-sample averaging path exactly (it is a register fact), route PB1/ADC1_IN9 as ADC_OSC, but treat `ch1_avg` as NOT driving the slice-1 regulation core. Confirm PA1 identity by live ADMUX/ADC read on the original board.
- **Blocks slice 1:** NO.

### B-UNITS — Engineering units / full-scale of ch0_avg (regulation input)
- **Known:** raw 10-bit counts averaged over 10 samples against REFS=11 (2.56 V datasheet decode); 0..1023.
- **NOT in image:** physical mapping (current? output power?). Board-truth (proposed PA0=SENS_OUT current/output sense feedback).
- **Suggested default:** carry `ch0_avg` as raw ADC counts (0..1023 @ 2.56V) into the lookup unchanged; preserve the numeric domain the table expects. (Same domain as B3.)
- **Blocks slice 1:** NO.

### B-CADENCE — Original F_CPU / absolute tick period (cadence-C9)
- **Known:** /1024 prescaler ratio + reload counts (16, 79) are in the image.
- **NOT in image:** the µs value; requires F_CPU (fuse/oscillator config), present only as the author build flag `-DF_CPU=8000000UL` in `m16_conv_v001.c:111`, not a register write.
- **Suggested default:** assume 8 MHz → 128µs/tick → Timer0 ~2.05 ms / Timer1 ~10.1 ms; the STM32 (96 MHz) reproduces these periods in ms directly. Ship the ~2.05 ms regulation tick + ~10 ms ramp tick as the contract; exact original F_CPU does not gate the port. Confirm 8 MHz on bench if absolute cadence matters.
- **Blocks slice 1:** NO.

### B-GATE — `timer0_process_cnt` increment/reset lifecycle (SCALE open question)
- **Known:** `0x1A5C` only clamps cnt to 11 and never resets to 0; it is the only direct `sts` writer found.
- **NOT in image:** where cnt is incremented (no in-image direct `sts` increment found). If an ISR increments it, after warmup cnt stays ≥11 and `0x1A5C` effectively fires every superloop pass.
- **Suggested default:** assume the transfer fires every regulation cycle once cnt first reaches 11; the increment/reset lifecycle is owned by the gate/scheduling dimension.
- **Blocks slice 1:** NO.

---

## 6. STM32 INTEGRATION MAP (which facts fill which contract point)

Contract surfaces verified in `fw/include/app_lcd.h`, `fw/src/app.c`, `fw/src/board.c`.

| STM32 contract point | Filled by (bucket-A fact) | Notes |
|----------------------|---------------------------|-------|
| `app_lcd_measure()` → `lcd_measure_t.curr_amp` | `ch0_avg @0x01B9` (ADC-2) — the regulation input | Provide as raw counts (B-UNITS); display layer already reads `curr_amp` (`app_lcd_disp.c:156`). |
| `lcd_measure_t.curr_power` / scaled level | `adc_scaled_value` (SCALE-04/05/06) | Opaque scaled setpoint (B3); display reads `curr_power/max_power/last_power`. |
| `lcd_measure_t` ADC_OSC / freq monitor | `ch1_avg @0x018D` (ADC-3) | Route to PB1/ADC1_IN9 per pinmap; do NOT feed regulation (B4). `curr_freq` displayed /100. |
| `lcd_measure_t.sig_run/seek/reset_status` | the 3 binary mirror flags `0x0189/018A/0196` (C6) | Active-LOW/idle-HIGH at the GPIO layer. |
| OSC output driver | C6 (active-LOW/idle-HIGH mirror) + C7 (3 out / 2 in) + B-SEAM | Drive PB2/PB10/PB14 with `GPIO_PIN_SET` = idle/off; the 8-level pattern→OSC binding is the seam (§5). |
| `app_lcd_hook_set_pot(output_power)` | suggested route for the analog/level power output (B-SEAM default) | Existing seam (`app_lcd_render.c:109`, `app_lcd_input.c:115`); preferred over OSC GPIOs for level output. |
| `app_lcd_hook_us_command(us_cmd_t)` | command path — slice-2 FSM (`0x15BC`/`0x1066`) | Not slice-1; START/RUN/RESET/SEEK already plumbed in input layer. |
| ~2 ms cadence slot in `app_loop_iter()` (`fw/src/app.c:62`) | cad-C1/C4 (Timer0 0xF0 → 16 ct → ~2.05 ms, unconditional `0x1A5C`+`0x138C` every tick) | `app_loop_iter` currently runs only LCD RX drain + 4 ms display tick; add the ~2 ms regulation step calling the scaling+lookup pipeline. |

### Two board.c bugs to fix (confirmed)

`fw/src/board.c` (read this session):
1. **CTRL_OSC_PINS wrong set** (line 7): `#define CTRL_OSC_PINS (GPIO_PIN_10 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15)` — **PB2 MISSING, PB15 SPURIOUS**. Per `pinmap.md:119-123` the OSC channels are **PB2 | PB10 | PB12 | PB13 | PB14**. (Pure F410 source-vs-doc fact, outside the AVR image.)
2. **OSC init polarity** (line 21): `HAL_GPIO_WritePin(GPIOB, CTRL_OSC_PINS, GPIO_PIN_RESET)` drives the OSC pins LOW at boot. The disasm confirms active-LOW/idle-HIGH, so **LOW at init = OSC ON at boot** — wrong; they should idle **HIGH (`GPIO_PIN_SET`) = OSC off**.

   **⚠ The fix is NOT "idle the whole mask HIGH" — it couples to §5 B-OSC-MAP and must split** (polarity is proven ONLY for the 3 mirrored flags `0x0189/018A/0196`):
   - **Now (safe, image-proven):** idle **PB2 / PB10 / PB14** HIGH = OSC0/OSC1/OSC4 ← mega16 PB1/PB0/PC7, the 3 channels Timer0-mirrored active-LOW (C6). Map: mega16 PC7→PB14, PB1→PB2, PB0→PB10.
   - **Defer (open — do NOT init as push-pull HIGH):** **PB12 / PB13** = OSC2/OSC3 ← mega16 PC4/PA7, which are **DDR-inputs in the image** (C7). Their direction AND idle polarity are unconfirmed (§5 B-OSC-MAP). Idling them HIGH as outputs would silently commit the exact direction the user must decide. Leave PB12/PB13 out of the output-init until B-OSC-MAP is resolved.
   - Bug #1's mask correction (add PB2, drop PB15) is orthogonal source-vs-doc hygiene; but the *output-init write* should cover only the 3 confirmed channels, not the full 5-pin mask.

---

## 7. Confidence ledger + what bench HW measurement would upgrade

**HIGH confidence (byte-exact image, ready to port):**
- ADC channel select, 10/50 averaging math, 32-bit accumulate+divide, ADMUX/ADCSRA bits, single-shot re-arm, /32→/128 prescaler.
- `0x1A5C` scaling transfer (gate, copy, input clip/floor, ×6) — exact arithmetic.
- `0x138C` lookup-compare (strictly-less-than), 21-rung thermometer ladder, set_output_* semantics, lookup_table[24] values.
- Cadence: Timer0 0xF0/16ct, first-reload 0x9B, unconditional ISR call order, both-timers /1024 Normal.
- Polarity: active-LOW/idle-HIGH for the 3 driven channels; pin directions (3 out / 2 in).
- The two board.c bugs.

**Interpretation layer (image-proven bytes + datasheet/schematic/name decode — keep visible):**
- REFS bit patterns proven; "2.56V / AVCC" are datasheet decodes (ADC-6).
- Table data/addresses/control-flow proven; names "lookup_table"/"output_level_process"/"adc_ch0_result" are v001-derived (C1, SCALE-05).
- Pin directions/polarities proven; "OSC0/1/4" numbering is schematic-level (C7/C9).
- ISR call structure proven; "regulation"/"ADC scaling" semantics are interpretive (cad-C4).

**Image-undecidable (bucket B, §5) — what bench HW would upgrade:**
- **B-SEAM / B-OSC-MAP:** scope/logic-analyzer sweep of the OSC connector vs the computed level — confirms whether the 8-level pattern, the 3 binary flags, or `set_pot` is the real drive, and the OSC2/OSC3 (PC4/PA7) direction. *Highest-value measurement; unblocks the output side of slice 1.*
- **B3 / B-UNITS:** ADC-input vs OSC-output sweep on the original board — binds `ch0_avg`/`adc_scaled_value` to physical units (current / power / duty).
- **B4:** live ADMUX/ADC read on the original board — confirms PA1/ADC1 identity (ADC_OSC monitor vs unconnected).
- **B-CADENCE:** read fuses or measure tick period — confirms F_CPU=8 MHz so absolute cadence is exact.
- **B-GATE:** trace `timer0_process_cnt` increment site — confirms the gate fires every superloop pass after warmup.
