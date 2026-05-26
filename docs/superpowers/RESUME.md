# RESUME — 다음 세션

> **상태 (2026-05-25)**: Stage D **프레이밍 확정 + slice 1 선택**. ATmega16 흡수 = **제어 함수 흡수이지 1:1 IO 이전이 아님** (사용자 2026-05-25 명시, 2회 강조). 단일 STM32가 SAMD20+M16 역할 모두 보유 → **명령 IPC는 내부화로 소멸**: M_START(PA4)/M_SEEK(PA5)/M_RESET(PA6) = 내부 `us_start/seek/reset()` 호출, M_OVLD(PC0) = 내부 플래그(옵션: CON_OVLD/PB3 외부 출력). **외부 물리 I/O로 남는 것** = ADC 센스(PA0=출력세기 피드백, PA1=ch1), **OSC 드라이브 출력**(누진 레벨 → OSC_OUT0..4 / CN2), buzzer/solenoid. ⇒ PB1/PB0/PC4/PA7/PC7 in/out 논쟁은 무의미해짐(M16 내부 핀 quirk). **측정 게이트(B1~B5) 해제** — 사용자 지시: 동작은 펌웨어 분석에서 도출, 물리 의미(B3)는 flag만 하고 코딩 차단 안 함.
> **다음 작업**: **Stage D slice 1 = 레귤레이션 코어** (사용자 선택). spec-first. 브랜치 `feat/stage-d-osc-pin-io` (이전 핀-I/O 스펙 `2026-05-25-stage-d-osc-pin-io-design.md`은 **RETIRED** — 프레이밍 오류, 배너 추가됨. 검증된 사실은 보존). 슬라이스1 내용 ↓ STEP 0.

---

## 새 세션 First Step

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
git checkout feat/stage-d-osc-pin-io   # slice 작업 브랜치 (main에 미머지)
git log --oneline -3                    # tip = 825ab7c (RETIRED 핀-IO 스펙 + 배너)
git tag -l 'hw-revA*'                   # hw-revA_fw-stage-a, hw-revA_fw-stage-b
```

### ▶ STEP 0 — Stage D slice 1: 레귤레이션 코어 (spec-first, 측정 게이트 해제됨)

새 세션 첫 행동: 위 브랜치 체크아웃 → **slice 1 spec 작성** (`docs/superpowers/specs/`) → 사용자 리뷰 → `writing-plans` → 구현. (brainstorming은 이미 완료 — 프레이밍/슬라이스 확정.)

**슬라이스 1 = 레귤레이션 코어** (M16 §4 제어루프 심장부; 알고리즘은 펌웨어에서 포팅, 물리 의미는 HW-verify 주석):
1. **ADC 획득층**: 2ch free-run, ch0 = 10샘플 평균(PA0 = 출력세기 피드백), ch1 = 50샘플 평균(PA1). 근거 `196c`, ADCSRA=0xCF(/128) ch0/ch1 교대.
2. **~2ms cadence** (M16 Timer0 등가; superloop+SysTick/TIM 위): `adc_ch0_avg` → `lookup_table` 스케일 → scaled value(0~1000 clip) → `output_level_process` 비교 → 누진 드라이브 패턴 `0x01→0x03→0x07→…→0xFF`.
3. **출력**: 누진 레벨 → OSC 드라이브 출력 라인 (V30 OSC_OUT0..4 = 전부 출력; 펌웨어의 M16 mixed in/out 무시 — 함수 관점은 "레벨을 OSC보드로 출력").

**다음 슬라이스(2+)**: 명령 상태머신(내부 us_start/seek/reset) + overload 검출/플래그 + soft-start 램프(main_loop 임계 0x29..0x191) + blink phase.

**flag (코딩 차단 ✗, 주석만)**: lookup_table 물리 의미(전류한계/주파수추종? = B3), ADC ch1(PA1) 정체(B4), OSC 출력 활성극성(Q8).

**검증된 사실 (slice 1 spec 재사용 — 이번 세션 도출)**:
- 제어루프 개요 = `docs/superpowers/analysis/atmega16-io-behavior.md §4`.
- `lookup_table[24]` 값(0x03FF→0x0004 감소곡선) = `ref/atmega16/m16_conv_v001.c:127-132`.
- Timer0 OSC 미러 극성 = **active-LOW / idle-HIGH** (플래그 set → 핀 LOW; disasm `01:256-276`). **v001이 맞고, `M16_reverse/out/04_reconstruction.c:175-177`은 극성 반전 오류.** 출력 초기화는 idle=HIGH (부팅 시 OSC off; 현 `board.c`는 LOW=asserted = 잠재버그).
- 디컴파일 함수명(g_run_flag/"LED"/"blink") 무시, 레지스터 사실만.
- pinmap 부록 A IPC표의 PB13/PB12 = **SAMD20 핀**(폐지된 옛 신호), F410 핀 아님 — F410 충돌 없음.

그다음 **`docs/NEXT_STEPS.md`** 읽기 (§5.7 BOOT0 이슈).

---

## ✅ 보드 이슈 — BOOT0 해결됨 (2026-05-26)

**BOOT0(U2.60)를 GND로 묶음 → 해결.** 이제 플래시한 앱이 평범한 `reset run`으로 직접 부팅. HW 검증(2026-05-26): `reset halt` 후 **PC=0x080045c0 / MSP=0x20008000**(플래시 Reset_Handler), `reset run` → mon USART6에 `[boot] gds_us_ctrl stage-b ready` / `[cfg] …` / `[t=N ms] hello uptime=N`(1초 cadence 증가) 정상. **gdb force-jump 워크어라운드 불필요.**

- **이전 문제(해결됨)**: R64(BOOT0 풀다운, U2.60) 미실장 → BOOT0 floating high → 매 리셋 시 ST 부트로더(PC 0x1FFFxxxx)로 부팅, 앱 미실행.
- **다른 미개조 보드에서 재발 시 fallback** (memory `project_board_boot0_workaround`): `openocd … program … verify` 후 gdb `monitor reset halt` → `set $sp=*(unsigned*)0x08000000` → `set $pc=*(unsigned*)0x08000004` → `set *(unsigned*)0xE000ED08=0x08000000`(VTOR) → `monitor resume; detach`. 상태 읽기는 두 번째 세션 `monitor halt`(reset ✗) 후 `print`.
- **시리얼 (USART6 mon, /dev/cu.usbserial-*, 115200)**: 단일 fd — `{ stty 115200 cs8 -parenb -cstopb raw -echo; cat; } < /dev/cu.usbserial-XXXX > log`.

---

## Stage B 결과 요약 (머지 완료)

- 계층: `i2c1`(PB6/PB7 AF4 400kHz) → `fram`(FM24C16B @0x50, 1-byte, big-endian) → `app_config`(0xAA init-flag, β factory map) → `app_lcd`(`init_lcd_mode` 포팅, scope a).
- HW 검증: **FRAM I2C err=0**, **factory-write/load 2-boot determinism 동일값**, mon `[cfg]` 정상, **육안 "GDS-15H" hand 페이지 확인**.
- delay/weld/hold(`LV_DM_*`)는 SETUP 페이지 VP라 RUN_HAND 화면엔 미표시(정상, samd20 동일).
- Codex가 plan 실행 (7 task commits). deviation: `syscalls.c` 추가(+`-lnosys` 제거, 0-warning 게이트 위해) — 정당·문서화됨.
- 미적용(선택): `_sbrk` heap/stack 가드 (LOW, 현재 `%f` 미사용으로 dead).

상세: spec `docs/superpowers/specs/2026-05-25-stage-b-lcd-app-design.md`, plan `docs/superpowers/plans/2026-05-25-stage-b-lcd-app.md`.

---

## ATmega16 분석 결과 + 보드-진실 정정 (`analysis §0.1`이 본문 §0/§1/§3/§4/§7보다 우선)

**확정 (보드 직독 / 사용자):**
- 아키텍처: ATmega16 = 트리거+출력세기 모니터링(실시간 레귤레이션), SAMD20 = UI+전체제어, OSC = 별도 보드.
- `CTRL_OSC0..4` ← mega16 **PB1/PB0/PC4/PA7/PC7** (V30 PB2/PB10/PB12/PB13/PB14). ← OSC 트리거 핀 미해소 해소.
- **PA0(ADC ch0) = 출력 초음파 세기 피드백** = 레귤레이션 입력 (V30 PB0/ADC1_IN8 `SENS_OUT`).
- IPC는 OSC 핀과 별개: 명령선 M_START/SEEK/RESET(PA4/5/6) + M_OVLD(PC0).

**뒤집힌 본문 결론 (이제 무시):** "IPC=PB0/PB1/PA7/PC4/PC7"(§1), "PB0/PB1/PC7=상태표시"(§3), "누진패턴→dead display로만 감"(§4). 디컴파일 **함수명·동작주석은 무시**, 동작은 사용자가 재도출.

**측정 대기 (Stage D 차단 — `analysis §0.1` B1~B5):**
- B1 5 OSC핀 각 방향 (출력 vs 입력 — PC4·PA7는 코드상 입력이라 순수 5출력 아닐 수 있음)
- B2 레벨↔OSC 비트매핑 + 극성(Active H/L)
- B3 레귤레이션 전달함수 (PA0 피드백 → scaled → 레벨; 디컴파일 스케일 수식 placeholder)
- B4 ADC ch1(PA1, 50샘플) 정체
- B5 `g_main_state` 0/1 두 출력모드 (lookup 레귤레이션 vs 본문부재 `output_direct_process`)

## 다음 세션 진입

1. **사용자 측정 결과 + 행동 재분석 수령** → `analysis §0.1` B1~B5 채움.
2. **Stage D brainstorming 재개** — spec 작성. 결정 완료분: 슬라이스 1개 유지(피드백 루프, 모듈 경계로만 분할), 코드-포팅 먼저+머지前 실측(사용자 결정), 안전경계는 측정 후 재논의(원래 arm-gate/idle-safe 옵션 보류).
3. **Stage D 구조부(측정 무관, 선포팅 가능)**: ADC 획득층(2ch 평균 10/50) + TIM 2ms/10ms cadence(Timer0/1 등가, superloop+SysTick 위) + 상태머신 골격 + **출력드라이버 추상화**(5비트 레벨 → CTRL_OSC, 매핑/극성은 named const로 격리). 전달함수·핀방향은 B1~B5 대기.
4. **비포팅**: 7-seg/`display_*`(dead), comm IPC(single MCU).

> ⚠️ "ref/atmega16 verbatim 포팅" 전제 **폐기** — 디컴파일은 약한 참조. 레귤레이션 동작은 사용자 측정/재분석이 권위.

## 그 외 대기 슬라이스

- **Stage C — Modbus RTU on USART6** (속도/패리티 = FRAM `comm_speed_idx`/`comm_parity_idx`, Stage B 로드됨) — Stage D와 독립
- **Stage A I/O** — CON_OVLD / CON_START / CTRL_OSC0~4 GPIO (Stage D와 일부 중첩)
- **scope-b LCD 확장** — `change_lcd_page` + SETUP 페이지 (DGUS VP맵 `hw/lcd/dgus/README.md`)

코드 슬라이스 정석 흐름: worktree → brainstorming → spec → plan → subagent/Codex 실행 → HW 검증 → 머지.

---

## 환경 / 누적 이슈

- `$STM32_TOOLCHAIN` stale → `env -u STM32_TOOLCHAIN cmake ...` 필수
- 빌드 게이트: `cmake --build` 0-warning (`-Wall -Wextra -Wundef -Wshadow`). clangd LSP 노이즈 무시
- ST-LINK V3J15M7B5S1 (API v3), Cortex-M4 r0p1 (HW BP ≤6)
- I2C1 PB6/PB7 AF4, 외부 풀업 10k→VCC_5 (5V, FT 핀), FRAM 0x50
- graphify: vendor 포함 3M 단어 임계 초과 — 매 세션 강제 ✗ (memory 정책 유지)
- 회로도/netlist: `hw/schematics/USW_CTRL_V30.{pdf,asc}`, DGUS: `hw/lcd/dgus/` (페이지↔매크로 맵 README)
- 응답 언어: 한국어 (코드/commit/식별자 영어)

---

## 참조

- 다음 단계: `docs/NEXT_STEPS.md` (§5.7 BOOT0)
- Stage B archive(kickoff): `docs/superpowers/historical/2026-05-25-RESUME-stageB-kickoff.md`
- Stage A archive: `docs/superpowers/historical/2026-05-06-RESUME.md`
- 변경 이력: `docs/changelog.md`
- samd20 ref (포팅 source, 수정 ✗): `ref/samd20/`
