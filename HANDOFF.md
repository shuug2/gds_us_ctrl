# Handoff: B-SEAM OSC 출력단 + overload 분석/설계 (코드 무변경)

**Generated**: 2026-06-20
**Branch**: `main` — origin 동기 (`git status` = `## main...origin/main`), 푸시 완료. 현재 해시는 `git log --oneline`; 안정 레퍼런스=태그.
**Status**: 🟢 깨끗한 체크포인트 — 진행 중 코드 작업 없음. 이번 세션은 **HW 비의존 분석/설계**(펌웨어 0줄, docs+ref만). 다음 = 확정 사실 위에서 코딩 또는 측정.

> 이전 HANDOFF(2026-06-20 "B-SEAM 분석 대기")는 본 세션 결과로 supersede. 이력 = `docs/changelog.md` / `docs/superpowers/RESUME.md`(자동 로드, 2026-06-20 블록 ①~⑨).

## Goal

SAMD20 + ATmega16 → STM32F410RBT 단일 MCU 통합. 코드/host-test 가능 계층은 이미 완료(LCD·레귤레이션·Modbus·weld·seek-reset). 본 세션은 **남은 출력/효과 계층(B-SEAM/overload/6b)을 코딩 가능한 수준까지 규명** — M16 disasm + SAMD20 소스 + 사용자 HW 진실로 미지 다수 해소.

## Completed (이번 세션, 전부 docs/ref — origin 푸시 완료)

- [x] **B-SEAM 신호 체인 정리**: 출력단 = ① OSC 채널(B-SEAM) + ② 진폭 pot(F2=U4 칩 정체), **둘 다 현재 hook 로그 stub**(`mon_printf`만, 실 핀/I2C 미구동; `app_reg.c`="drives NO OSC GPIO").
- [x] **B4 해소**: PB1(ADC1_IN9) = **SAMD20 PA02 소비전류**(M16 PA1은 미연결). 전류/전력(`curr_amp×2.2`)/에너지(`acc/500`) 전부 PB1 파생. STM32는 현재 PB1 read-and-discard + 이 3값을 ch0(reg_scale)에서 뽑음 = **source 틀림**.
- [x] **OSC 3채널 명령 매핑 확정(사용자 HW + disasm)**: **RUN→PC7→PB14(OSC4) / SEEK→PB1→PB2(OSC0) / RESET→PB0→PB10(OSC1)**, 전부 **active-LOW binary 미러**(thermometer 패턴 아님). v001 변수명 오라벨(`g_run_flag`=실제 RESET).
- [x] **OVLD(PC0) 로직 디코드**: PA7(PINA.7) HIGH ×5 디바운스 → PC0 어서트 + OVLD→SEEK 복구 훅. **PA7=과부하 입력 → 보류했던 OSC3(PB13) 해소**.
- [x] **PA0/SENS_OUT = 양 레거시 MCU 모두 vestigial**(M16 dead 7seg / SAMD20 curr_lv 소비처 주석).
- [x] **overload 스테이지 설계 확정(사용자 결정)**: STM32 직접 처리(M_OVLD IPC 소멸), 입력 PB13 active HIGH, 출력 PB3 CON_OVLD=외부 릴레이(active HIGH), heartbeat PB3→PB8, 복구=seek-reset FSM 재사용.
- [x] **M16 disasm 자료 repo 추가**: `ref/atmega16/m16_controller_prg.hex` + `m16_disasm/firmware_disassembled.asm`(2953줄) — 권위 소스.
- [x] **pinmap 정정**(`docs/pinmap.md` 상단 "2026-06-20 정정" 배너 + 인라인 표).
- [x] 분석 문서/메모리/RESUME/검토 Artifact 동기화.

## Not Yet Done (전부 HW-gated 또는 코딩 대기)

- [ ] **B-SEAM 3채널 코딩** — 내부 run/seek/reset 상태 → PB14/PB2/PB10 active-LOW 구동. (로직 확정, 코딩 가능; 극성 sanity만 HW.)
- [ ] **overload 코딩** — PB13 입력 디바운스 → 내부 응답(정지/ERR/LCD "OVER LOAD"/복구) → PB3 릴레이. heartbeat PB3→PB8 이전 필요. (로직 확정, 코딩 가능.)
- [ ] **6b** — 전류/전력/에너지를 PB1(소비전류)에서 SAMD20 공식으로 재구성 + 절대 보정(cal_val·×4·/10·×2.2·thr51/off37·/500↔/250). ch0 currentprice 오용 정리. (실 rig 게이트.)
- [ ] **F2** — U4 I2C_POT 진폭 실구동(칩 정체 확인 + I2C1 write).
- [ ] **OSC2(PC4/PB12) 정체** — mega16 입력(`sbis 0x13,4`), 용도 미디코드 (사용자 보류).
- [ ] **ch0(PA0/출력세기) 통합 역할 결정** — curr_lv 표시 부활 / vestigial 유지 / 기타.
- [ ] **measurements** — OSC/OVLD 극성 sanity, F2, 6b 절대값 (스코프/실 초음파 rig).

## Failed Approaches (반복 금지)

- **v001(`m16_conv_v001.c`) 단독 신뢰 금지** — 부분·부정확 재구성. 누락/오기 누적: ×6를 `>>6`로, lookup `<`를 `>`로, warmup, ADMUX REFS=11을 AVCC로, OSC 변수명 오라벨, **PA5/PA6 read 0건, PC4 read 누락, overload 로직 전체 누락**. → **권위 = `ref/atmega16/m16_disasm/firmware_disassembled.asm`**, v001은 참고용.
- **"진폭(I2C_POT)은 동작중" 가정 = 틀림** — `app_lcd_hook_set_pot`/`app_weld_hook_set_amp`는 `mon_printf` stub. I2C1은 FRAM 전용. 진폭도 미구동(F2).
- **"8단 thermometer 패턴 → OSC 1:1" 가설 = 기각** — 그 패턴은 V26 dead 7-seg 전용. OSC = binary run/seek/reset 미러(사용자 HW 매핑이 입증).
- **"5채널 OSC = 5 push-pull 출력" = 틀림** — PB13=과부하 입력, PB12=입력. 출력은 PB2/PB10/PB14 3개뿐. PB12/PB13 출력 구동 금지(contention).
- **핀 라벨 혼동 주의** — "STM32 PB1"(소비전류 ADC 입력) ≠ "M16 PB1"(SEEK 출력→STM32 PB2). "mega16 PA7"(→PB13 과부하) ≠ "STM32 자신 PA7"(=ETH_MOSI).

## Key Decisions

| Decision | Rationale |
|----------|-----------|
| overload = STM32 직접 처리, M_OVLD IPC 소멸 | PA4/5/6 명령처럼 MCU간 신호는 내부화. PB13(←PA7) 입력이 STM32에 연결됨 |
| overload active HIGH (입력 fault HIGH / 출력 릴레이 assert HIGH) | 사용자 확정. M16 disasm(PA7 HIGH=fault)과 일관 |
| CON_OVLD(PB3) = 외부 릴레이 제어 출력 유지 | 사용자: 외주 릴레이제어신호로 사용 |
| heartbeat PB3 → PB8 이전 | PB3을 CON_OVLD 본용도로 복원 (빈 GPIO 중 PB8 선정) |
| 전력/에너지 source = PB1(소비전류), not ch0 | SAMD20 검증: curr_power=curr_amp×2.2. ch0(PA0)는 레거시 vestigial |
| OSC 구동 = binary run/seek/reset active-LOW | disasm C6 + 사용자 HW 매핑; thermometer 패턴은 dead 7seg |

## Current State

**Working**: main 빌드 0-warning, FLASH 42.11%/RAM 16.77%, host **5스위트 PASS**(reg_calc/modbus_core/tcp_frame/weld_fsm/seek_reset_fsm) — 세션 시작 시 검증, 이후 펌웨어 코드 무변경이라 유효. `fw/build/gds_us_ctrl.elf` 존재.

**Broken**: 없음.

**Uncommitted**: 없음 (origin 동기). 이번 세션 6커밋 전부 docs/ref, 푸시 완료: `31ae52b`→`c5fd258`→`1d2107b`→`a886edf`→`d086a6c`→`90ef91b`.

## Files to Know

| File | Why It Matters |
|------|----------------|
| `docs/superpowers/analysis/2026-06-20-bseam-osc-signal-chain-and-port-fidelity.md` | **이번 세션 권위 분석** — §1 신호체인 · §2 명령선 · §3 OSC핀 충실도 · §4 처리코어 · §4b ADC/B4 · §4c OVLD/PA7/PA0/overload설계 · §5 측정 · 부록A 처리코어 |
| `ref/atmega16/m16_disasm/firmware_disassembled.asm` | **M16 권위 disasm**(2953줄). OVLD fn `@0x10A6`, OSC 미러 `@0x2EE`, scaling `@0x1A5C` |
| `docs/pinmap.md` | 핀 스펙 — 상단 "2026-06-20 정정" 배너 우선 |
| `fw/src/app_reg.c` | 레귤레이션+RUN게이트. `reg_acquire_step`(ch0/ch1), `reg_publish_measure`(curr_power=ch0 오용), "drives NO OSC GPIO" |
| `fw/src/app_reg_calc.c` | reg_scale(×6)/lookup/`reg_energy_from_acc(acc/250)` |
| `fw/src/app_seek_reset_fsm.c` `app_seek_reset.c` | overload 복구가 재사용할 FSM/글루 |
| `fw/src/board.c` | OSC idle-HIGH init(PB2/PB10/PB14), heartbeat=PB3(→PB8 이전 대상) |
| `ref/samd20/main.c` | curr_amp/power/energy(412–436), curr_lv 주석(5135), overload 응답(1452/1659) |
| 검토 Artifact | https://claude.ai/code/artifact/ce2570b9-4e50-4ad8-bfab-8c3d2edec2b1 (사용자 오프라인 검토용) |

## Code Context

**확정 I/O 매핑 (STM32):**
```
입력 ADC:  PB0(IN8)=출력세기 피드백(M16 PA0, vestigial)  ·  PB1(IN9)=소비전류(SAMD20 PA02)
OSC 출력:  PB14=RUN  PB2=SEEK  PB10=RESET   (전부 active-LOW binary 미러)
           PB12=OSC2?(입력 미확정)  PB13=과부하 입력(active HIGH)
출력:      PB3=CON_OVLD 릴레이(active HIGH)  ·  heartbeat→PB8
진폭:      I2C_POT @0x28 (F2, 미구동)
명령:      옛 IPC PA4=START/PA5=SEEK/PA6=RESET → STM32 내부 us_cmd
```

**M16 OVLD 로직 (firmware_disassembled.asm @0x10A6, per-tick 0x06EC 호출):**
```
PA7(PINA.7) HIGH → 카운터 0x0199++ ; ≥5 → sbi 0x15,0 (PC0=HIGH) + 래치 0x019C/0x01A2
PA7 LOW → 카운터 리셋 (noise reject)
복구: 0x08CC→call 0x1D28 → SEEK 플래그 0x018A=1
```

**현재 STM32 measure 게시 (app_reg.c reg_publish_measure) — ⚠ source 오용:**
```c
g_measure.curr_amp   = g_reg.ch0_avg;            // ⚠ ch0=PA0(vestigial); 실 전류는 PB1
g_measure.curr_power = reg_scale(g_reg.ch0_avg);  // ⚠ 전력=PB1 소비전류×2.2 여야 함 (6b)
g_reg.acc_energy += curr_power; curr_energy = acc/250;  // 구조 OK, 입력 틀림
```

## Resume Instructions

1. 자동 로드 `docs/superpowers/RESUME.md`(2026-06-20 블록 ①~⑨) + 권위 분석 `docs/superpowers/analysis/2026-06-20-bseam-osc-…md` 정독.
2. sanity:
   ```bash
   env -u STM32_TOOLCHAIN cmake -S fw -B fw/build -G Ninja && env -u STM32_TOOLCHAIN cmake --build fw/build   # 0-warning 기대
   make -C fw/test test    # 5 스위트 PASS 기대
   git status              # ## main...origin/main 기대
   ```
3. **사용자 오프라인 검토(분석+Artifact) 피드백을 먼저 반영** → 그 후 spec.
4. 방향 선택:
   - **HW 없음(설계/코딩)** → `superpowers:brainstorming→spec→writing-plans→subagent-driven`로: ① B-SEAM 3채널(내부 run/seek/reset→PB14/PB2/PB10 active-LOW) ② overload(PB13 디바운스→내부 응답+seek-reset FSM 복구→PB3 릴레이; heartbeat PB8 이전) ③ 6b(전력/에너지 PB1 재구성). host-test로 검증, 절대값/극성은 HW로 분리.
   - **HW 있음(측정)** → 분석 §5 측정 절차(OSC/OVLD 극성 sanity, F2, 6b 절대값). ⚠ 스코프 프로빙 고전압/비접지 절연 확인.
5. 머지 시 `--no-ff` + 태그 `hw-revA_fw-stage-<x>` → `git push origin main && git push origin --tags` (이 환경 푸시 가능 확인됨; ls-remote로 검증).

## Setup Required

- 빌드: `$STM32_TOOLCHAIN` stale → **`env -u STM32_TOOLCHAIN` 필수**. 새 소스 추가 후 **`cmake -B build` reconfigure 필수**(GLOB는 configure 시점만 평가).
- 보드(HW 세션): SERIAL/addr=1/9600/EVEN(USART6=Modbus 점유, mon 비가용). RS-485=`/dev/cu.usbserial-AB0MLYXA`, ST-LINK=`/dev/cu.usbmodem*`. mon 필요 시 LCD addr=NONE.

## Edge Cases & Error Handling

- OVLD 입력(PB13) 극성: active HIGH 확정이나 PA7 풀업 idle HIGH 텐션 → 정상시 외부 LOW 유지·fault/단선 HIGH(fail-safe) 가설, **보드 실측**. STM32 GPIO pull 설정(pull-down vs none) 결정 필요.
- B-SEAM/overload 코딩은 host-test 가능하나 **물리 출력/극성/실 fault는 HW** — host로 로직만, HW로 절대 검증 분리(weld/seek-reset 선례).

## Warnings

- **옛 해시 신뢰 금지** — 2026-06-20 `git filter-repo`로 전체 SHA 재작성됨. 태그/메시지로 참조.
- **v001 단독 신뢰 금지** (Failed Approaches 참조). 권위 = disasm.
- **핀 라벨 중복** — STM32 PB1≠M16 PB1, STM32 PA7≠mega16 PA7 (Failed Approaches).
- **push = SSH** (`git@github.com:shuug2/gds_us_ctrl.git`). 이 세션에서 푸시 동작 확인됨(ls-remote 검증). GitHub 이메일 인증(nogari@gmail.com → shuug2 Settings)은 사용자 미완료(소급).
- **물리 OSC/진폭/overload 효과 전부 미검증**(코드 0줄 또는 stub) — B-SEAM/F2/6b까지 실거동 미확인.
