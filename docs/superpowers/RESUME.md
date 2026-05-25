# RESUME — 다음 세션

> **상태 (2026-05-25)**: ATmega16 FW↔I/O 분석 완료 + **보드-진실 정정** (`analysis §0.1`). OSC 드라이브 매핑 **보드 직독 확정**: `CTRL_OSC0..4` ← mega16 PB1/PB0/PC4/PA7/PC7 (V30 PB2/PB10/PB12/PB13/PB14). **소스 오브 트루스 전환** — 1차=사용자 보드 측정+행동 재분석, 2차(약함)=디컴파일(함수명 무시). (Stage B는 `540008d`, tag `hw-revA_fw-stage-b`로 머지 완료.) `main` 단독, 문서 갱신 완료.
> **다음 작업**: **사용자 HW 재측정 대기** — `analysis §0.1` B1~B5 (① 5 OSC핀 방향 in/out ② 극성·레벨↔비트 매핑 ③ 레귤레이션 전달함수 ④ ADC ch1(PA1) 정체 ⑤ `g_main_state` 0/1 두 출력모드). 사용자가 측정 + 행동 재분석 제공 예정. **수령 후 Stage D brainstorming 재개**. Stage D 코딩은 측정 전까지 보류.

---

## 새 세션 First Step

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
git status                 # clean 기대 (tip = 9cac85c docs: ATmega16 board-truth correction)
git log --oneline -6
git tag -l 'hw-revA*'      # hw-revA_fw-stage-a, hw-revA_fw-stage-b
```

### ⛔ STEP 0 — 측정 게이트 (Stage D 코딩 차단 중)

이 세션은 **사용자 HW 재측정 대기** 상태로 멈춤. 새 세션 첫 행동:

1. **사용자가 `analysis §0.1` B1~B5 측정/행동 재분석을 제공했는가?**
   - **제공됨** → 값을 `analysis/atmega16-io-behavior.md` §0.1 B1~B5 표에 채우고 → **Stage D brainstorming 재개** (spec 작성). 결정 완료분: 1슬라이스(피드백 루프, 모듈경계로만 분할) · 코드포팅 먼저+머지前 실측 · 안전경계 옵션 측정 후 재논의.
   - **아직 안 됨** → 측정 항목(B1~B5) 다시 안내하고 대기. 코딩 착수 금지.
2. 측정과 무관하게 착수 가능한 **Stage D 구조부**(원하면): ADC 획득층(2ch 평균 10/50) + TIM 2ms/10ms cadence(superloop+SysTick 위) + 상태머신 골격 + 출력드라이버 추상화(매핑/극성 named const 격리). 단 전달함수·핀방향은 B1~B5 대기.

그다음 **`docs/NEXT_STEPS.md`** 읽기 (§1.1 슬라이스 현황, §5.7 BOOT0 이슈).

---

## ⚠️ 먼저 알아야 할 보드 이슈 (HW 작업 시)

**R64 (BOOT0 풀다운, U2.60) 미실장** → BOOT0 floating high → 매 리셋 시 ST 시스템 부트로더로 부팅, **플래시한 앱이 안 돎**. 증상: `reset halt` PC=0x1FFFxxxx, RAM garbage, 시리얼 무출력.

- **영구 수정**: R64 실장 또는 BOOT0→GND.
- **검증 워크어라운드** (memory `project_board_boot0_workaround`):
  ```
  openocd ... program build/gds_us_ctrl.elf verify
  gdb: monitor reset halt; set $sp=*(unsigned*)0x08000000; set $pc=*(unsigned*)0x08000004;
       set *(unsigned*)0xE000ED08=0x08000000; monitor resume; detach
  ```
  강제 점프해도 클럭 96MHz 정상 (RCC 확인). g_cfg/카운터는 두 번째 gdb 세션에서 `monitor halt`(reset ✗) 후 `print`.
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
