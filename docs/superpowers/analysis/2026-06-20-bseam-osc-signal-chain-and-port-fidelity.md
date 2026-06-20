# B-SEAM OSC — 신호 체인 · 명령선 · 포팅 충실도 (2026-06-20 분석 세션)

> **요약**: HW 비의존 분석 세션(코드 0줄, HW 측정 0건). B-SEAM(OSC 출력단)이 풀렸을 때 MCU가 무엇을 하는지, legacy M16 동작이 STM32로 제대로 포팅됐는지를 1차 소스(`ref/atmega16/m16_conv_v001.c` + disasm 분석 + STM32 코드)로 교차 검토했다. **핵심 결론 4건**: ① **출력단은 2조각 모두 미구동 stub** — OSC 채널 구동(**B-SEAM**) + 진폭 pot 실구동(**F2**, U4 칩 정체); 둘 다 `mon_printf` 로그만. ② 옛 IPC 명령 3선 **PA4=START / PA5=SEEK / PA6=RESET**(+PA7/PC4=IPC 아님)는 STM32에서 내부 `us_cmd`로 소멸, 처리부는 이식 완료. ③ M16은 "선(wire)"이 아니라 **프로세서** — "미러"는 명령핀→출력핀 통과가 아니라 *내부 상태 플래그 → OSC 핀* 매핑(finding ④: OSC 3선 = 명령 3선 active-LOW 레벨 미러, 가설). ④ **M16 처리코어 포팅 충실도 = 높음** — STM32가 disasm을 권위로 삼아 v001이 틀린 3곳(`×6` vs `>>6`, lookup `<`, warmup 부팅1회 vs per-START)을 모두 비껴감. 미검증은 전부 코드 버그가 아니라 절대값/물리출력(6b·B-SEAM·F2 게이트). **다음 세션 = 사용자 오프라인 검토 반영 → HW 있으면 측정+설계, 없으면 B-SEAM/F2 spec 선행.**
>
> **산출물**: 검토용 Artifact(claude.ai) — 신호 체인 다이어그램 + 측정 절차 7단계 + 포팅 충실도(OSC 핀/처리코어) + 명령선/프로세서 정리. URL은 §7. (scratchpad HTML 원본은 세션 한정 소멸; 본 .md가 repo 영속본.)

---

## 1. 신호 체인 (MCU 중심)

옛 2-MCU(SAMD20 → 명령 4선 → ATmega16 → 발진보드)를 STM32 하나가 양쪽 흡수. **MCU는 초음파 주파수를 직접 만들지 않는다** — 발진은 외부 오실레이터 보드가 하고, MCU는 (1) 채널 ON/OFF, (2) 진폭, (3) 피드백, (4) SEEK/RESET 명령만 담당.

| 역할 | 핀 / 경로 | 상태 |
|------|-----------|------|
| 채널 ON/OFF | `CTRL_OSC0..4` = PB2/PB10/PB12/PB13/PB14 (active-LOW) | **B-SEAM** (미구동 stub) |
| 진폭(파워) | I2C_POT `@0x28` (I2C1, FRAM과 공유) | **F2** (미구동 stub) |
| 피드백 읽기 | ADC ch0(전류/전력) · ch1=`ADC_OSC`/PB1(발진 모니터) | 읽기 동작 / 절대보정=6b |
| SEEK / RESET | 공진 탐색 / 초기화 신호 (옛 M_SEEK·M_RESET 자리) | hook stub |

**⚠ 출력단 전체가 미구동**: `app_reg.c`는 "drives NO OSC GPIO"(compute-only). `app_lcd_hook_set_pot`(app_lcd.c:24)·`app_weld_hook_set_amp`(app_weld.c:36)는 **`mon_printf` 로그만**. I2C1 버스는 FRAM(FM24C16B) 전용으로만 실구동. 즉 두뇌·상태머신·계기판·명령 라우팅은 다 돌지만, **실출력 = ① B-SEAM(OSC 채널) + ② F2(진폭 pot) 둘 다 미이식.**

## 2. 명령 입력선 (PA4/5/6) + "M16 = 프로세서"

옛 SAMD20→M16 IPC 명령선(2026-06-10 IPC semantics 분석, samd20+disasm 교차):

| IPC 명령 | SAMD20 out | M16 in | 의미론 |
|---|---|---|---|
| M_START | PB13 | **PA4** | active-LOW 레벨, hold-to-run |
| M_SEEK | PB11 | **PA5** | active-LOW, 500ms 자동 해제 |
| M_RESET | PB12 | **PA6** | active-LOW, 500ms 후 자동 해제 + SEEK 자동 체인 |

(4번째 선 M_OVLD=PC0, overload 스테이지 이연. PA7/PC4는 samd20 대응핀 없음 = **IPC 아님**.)

**"미러"는 passthrough가 아님** — M16은 명령을 받아 *자기 로직 수행*(상태머신·warmup·500ms 체인·레귤레이션) 후 내부 플래그를 OSC 핀에 미러. "미러" = `내부 상태 플래그(g_0189/018A/0196) → 출력핀`, NOT `명령핀 → 출력핀`. (예: START 플래그는 warmup 중 안 켜짐; RESET은 500ms 처리 후 SEEK 체인 — 순수 통과면 없을 거동.)

```
[옛]  SAMD20 ─명령(PA4/5/6)→ M16 ┌ 처리: 상태머신·warmup·500ms RESET→SEEK·레귤레이션
                                 └ 내부 플래그 ─(Timer0 ISR active-LOW)→ PB0/PB1/PC7(OSC) → 발진보드
       (진폭은 SAMD20이 I2C_POT 직접 구동)
[신]  STM32 ─내부명령(US_CMD_*)→ [처리 = 이미 포팅] → 내부 run/seek/reset 상태
                                 └ [B-SEAM: 내부상태 → active-LOW 출력] → PB10/PB2/PB14 → 발진보드
       (진폭 I2C_POT = F2 stub)
```

**3↔3 대응**: 명령 3개(START/SEEK/RESET) ↔ 출력 3채널(PB0/PB1/PC7). finding ④ = OSC 3선이 명령 3선의 active-LOW 레벨 미러일 공산(가설, 측정 확정). STM32 처리부는 이식 완료(START→`app_reg` RUN 게이트 슬2b HW✓ / SEEK·RESET→`app_seek_reset` 500ms 체인 host+HW✓); 남은 건 출력 와이어업(B-SEAM + F2).

**⚠ 참조 C 함정**: `m16_conv_v001.c`는 PINA.4만(디스플레이 타이밍 문맥), PA5/PA6 read 0건. 명령 디스패처(`app_0x06d2`/`app_0x0c70`)는 disasm에만 → 명령 의미론 권위 = 참조 C 아님, **disasm**.

## 3. 포팅 충실도 — OSC 핀 (방향/극성)

매핑(M16→STM32, pinmap.md·board.c 일치): PB1→PB2(OSC0), PB0→PB10(OSC1), PC4→PB12(OSC2), PA7→PB13(OSC3), PC7→PB14(OSC4).

| M16 | →STM32 | M16 방향/idle | M16 런타임 | STM32 |
|---|---|---|---|---|
| PB1 | PB2 OSC0 | 출력 idle HIGH | active-LOW `g_blink_active` (v001:462) | init ✓ / 런타임 ✗ |
| PB0 | PB10 OSC1 | 출력 idle HIGH | active-LOW **`g_run_flag`** (v001:469) | init ✓ / 런타임 ✗ |
| PC7 | PB14 OSC4 | 출력 idle HIGH | active-LOW `g_blink_state` (v001:455) | init ✓ / 런타임 ✗ |
| PC4 | PB12 OSC2 | 입력+풀업 | 코드상 안 읽힘 | 미설정(풀업 미복제) |
| PA7 | PB13 OSC3 | 입력+풀업 | 코드상 안 읽힘 | 미설정(풀업 미복제) |

판정: **init은 충실(idle HIGH=active-LOW off, 과거 부팅-OSC-ON 버그 수정됨), 런타임 구동은 미이식(B-SEAM)**. 입력 2채널(PC4/PA7)은 M16에서도 안 읽힘 → 포팅할 동작 자체 없음, 방향은 default로 일치(풀업만 미복제). board.c: `CTRL_OSC_OUT_PINS = PB2|PB10|PB14` idle HIGH; PB12/PB13 미설정(B-OSC-MAP 측정 게이트).

## 4. 포팅 충실도 — M16 처리코어 (scaling/lookup/ADC/warmup)

3-way 교차(참조 C v001 + disasm 권위 + STM32). **v001은 부분·부정확 재구성 → 단독 신뢰 금지.**

| 처리기능 | 참조 C (v001) | disasm (권위) | STM32 | 판정 |
|---|---|---|---|---|
| scaling ×6 | `temp >> 6` (L695) ❌ | `input × 6` mul 0x2158 (SCALE-06) | `in*6u` (reg_scale:18) | ✅ disasm |
| clip/floor | `≥1000`,`<3` 입력 | 입력 도메인, value 3 통과 (SCALE-04/05) | `≥1000`,`<3`, 3 통과 | ✅ 경계까지 |
| lookup band | `<`/`>` 내부 불일치 | 첫 i where table[i]<scaled, strictly-less (C2) | strictly-less first, else 21 | ✅ disasm 해소 |
| lookup 값 | table[24] (L127) | flash 0x58, idx0-20 ↓, 첫 21 copy (C1) | reg_lookup_table[24] byte-for-byte | ✅ |
| ADC 평균 | 10/50 (acc 16bit 오해) | 10/50, 32bit acc (ADC-2/3) | CH0=10/CH1=50, 32bit | ✅ 샘플수 (cadence ms-grid 근사) |
| warmup | g_main_state 1→0 @401 | 부팅 1회 ~4s, @0x137C one-way, 명령무시 @0x041E (finding ①) | main_state 1→401→0, START guard | ✅ (slice2a per-START 램프 폐기) |
| OSC 극성 | active-LOW (454-471) ✅ | active-LOW/idle-HIGH cbi (C6) | board.c idle HIGH; 런타임 미구동 | ⚠ init ✅ / 런타임=B-SEAM |
| RESET→SEEK 체인 | (samd20 원본) | main.c:5395-5407 (finding ③) | SR_TICKS=50=500ms 액면가 | ✅ 로직 / ±100ms quirk 의도적 미재현 |

**★ v001 틀렸고 STM32가 disasm 따른 3곳**: (1) ×6 vs `>>6` (disasm에 시프트 0건, `0x1AC2 ldi r30,0x06`+`0x2158` 곱셈) (2) lookup `<` (C2 `0x202A` cp→brcs) (3) warmup 부팅1회 (finding ① `@0x041E`/`@0x137C`; `reg_ramp_level`은 검증 테이블 레퍼런스로만 보존, 출력 경로 제거).

## 5. 측정 절차 (HW 세션용, 스코프 + SWD)

**안전**: ① 부팅 시 OSC off 극성 확인(과거 버그) ② **PB12/PB13 출력 구동 금지**(M16 입력핀 → contention) ③ 최소 진폭+짧은 on-time부터 ④ **스코프 프로빙 자체** — 파워 구동단 고전압·비접지 가능 → GND 클립 전 전압/절연 확인(차동/절연 프로브).

1. 보드 모드/관측 채널 확보 (SERIAL=mon 비가용 → addr=NONE 또는 ETH, 아니면 scope+SWD)
2. 부팅 직후 CTRL_OSC0..4 5핀 idle 레벨 (PB2/PB10/PB14=HIGH 기대; PB12/PB13=?)
3. RUN 트리거 → 어느 핀 토글? active 레벨/지속시간(ceiling 560ms?)
4. output_power 스윕(50→100) → I2C_POT(@0x28) 트랜잭션 + OSC 패턴 변화? (불변=enable+pot 단순 / 변함=패턴 매핑) **+ F2: U4 칩 정체·I2C write 확인**
5. PB12/PB13 외부 구동 여부 → 피드백 입력 vs 출력
6. SEEK/RESET 물리 핀 + 거동; 스윕이 ADC_OSC(PB1)에 보이면 보드-side, 안 보이면 MCU 생성 필요. ⚠ 실 트랜스듀서/혼 필요(공진=음향 부하 의존)
7. ADC 도메인 실측(PB0/PB1 전압 범위) → 6b 보정 입력

**최단 경로**: 동작 원본(M16) 보드가 있으면 OSC 커넥터를 같은 절차로 스코프 → gold-standard 매핑/극성(net-to-channel은 AVR 기계어에 없음).

## 6. HW로 확정할 미해소 (코드 충실도와 별개)

- **B-SEAM**: 계산된 레벨/명령 상태가 어느 OSC 핀으로, 어떤 극성/패턴으로 가는가 + PB12/PB13 방향. 출력=binary 명령미러(finding ④)인지 레벨패턴인지.
- **F2**: U4 I2C_POT 칩 정체 → 진폭 pot 실구동 구현. (실출력 = B-SEAM + F2)
- **SEEK/RESET 스윕 주체**: 보드-side(MCU 1신호) vs MCU 생성 — 코드 규모 좌우.
- **6b**: ×6 등 절대 보정(M16 2.56V ↔ STM32 3.3V/12bit), energy divisor `REG_ENERGY_DIV=250`.

## 7. 다음 세션 진입

1. **사용자 오프라인 검토 반영** — 본 분석 + Artifact를 사용자가 오프라인 검토 후 피드백. 그 피드백을 먼저 반영한 뒤 B-SEAM/F2 spec 작성.
2. **HW 있으면** → §5 측정 절차 실행 → 결과로 §6 미해소 확정 → B-SEAM(+F2) 코딩 스테이지(spec→plan→subagent→HW 검증→머지+태그).
3. **HW 없으면** → B-SEAM/F2/overload spec 선행 가능(설계만; 절대 검증은 HW).
4. ⚠ B-SEAM은 "검증"이 아니라 **measure → 설계 → 코드 → 검증** (현재 OSC 구동 코드 0줄).

**검토용 Artifact (claude.ai)**: https://claude.ai/code/artifact/ce2570b9-4e50-4ad8-bfab-8c3d2edec2b1

## 근거 (1차 소스)

- `ref/atmega16/m16_conv_v001.c` — init 1151-1168, OSC 미러 453-473, scaling 672-695, PINA.4 580
- `docs/superpowers/analysis/2026-05-31-m16-regulation-core-verified.md` — SCALE-04~07, ADC-2/3, C1/C2, C6/C7, B-SEAM/B-OSC-MAP §5
- `docs/superpowers/analysis/2026-06-10-samd20-m16-ipc-semantics-verified.md` — PA4/5/6 IPC, finding ①(warmup)·③(500ms 체인)·④(OSC=명령 미러 가설), §6b(set_pot stub/F2)
- `docs/pinmap.md` §117-131, 부록 A IPC
- `fw/src/{app_reg.c, app_reg_calc.c, board.c, app_lcd.c, app_weld.c, app_seek_reset.c, app_seek_reset_fsm.c, app_modbus.c, app_lcd_input.c, app_lcd_render.c}`
