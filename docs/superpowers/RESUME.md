# RESUME — 다음 세션

> **상태 (2026-05-25)**: **ATmega16 FW 동작↔I/O 분석 완료** → `docs/superpowers/analysis/atmega16-io-behavior.md`. (Stage B는 `540008d`, tag `hw-revA_fw-stage-b`로 머지 완료.) `main` 단독.
> **다음 작업**: **(선행) HW-verify 패스** → 분석문서 §7의 LOW/M 행(OSC 트리거 핀·ADC 채널·PC1)을 실측 승급. 그다음 **Stage D — Ultrasonic regulation core 흡수** (분석문서 §8). 코드 슬라이스 정석 흐름.

---

## 새 세션 First Step

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
git status                 # clean 기대
git log --oneline -6
git tag -l 'hw-revA*'      # hw-revA_fw-stage-a, hw-revA_fw-stage-b
```
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

## ATmega16 분석 결과 요약 (완료 — `docs/superpowers/analysis/atmega16-io-behavior.md`)

- **아키텍처 확정**: ATmega16 = 트리거+전류모니터링(실시간 레귤레이션), SAMD20 = UI+전체제어, OSC = 별도 보드. IPC = 순수 GPIO.
- **디컴파일러 noise 정정**: C 헤더의 핀-역할 주석 대부분 추측. `display_*`(PORTD+PB2/3/4)는 **V26 미연결 = dead legacy**. `BUZ_M16`/`U1.xx` 등 net 주석 오류 다수.
- **HIGH 확정 핀**: PA4=`M_START`(START in), PC0=`M_OVLD`(overload out). PORTD/PB2-4 미연결.
- **핵심 단서**: `output_level_process` 누진 5비트(0x01→0x1F) ↔ V30 `OSC_OUT0..4` 1:1.
- **미해소(최우선 HW-verify)**: ① OSC 트리거 물리 출력핀 ② ADC ch0/ch1 정체 ③ PC1 방향 충돌. (분석문서 §7)

## 다음 세션 진입 (분석문서 §8 그대로)

1. **(선행) HW-verify 패스** — §7 #1/#2/#3 OLD 보드 또는 V30 보드로 실측. scope/openocd. (BOOT0 워크어라운드 필요)
2. **Stage D — Ultrasonic regulation core 흡수**:
   - in: STM32 ADC(`SENS_OUT`/`SENS_CURR`) → adc avg
   - proc: `lookup_table` 스케일 → `adc_scaled_value` → 누진 레벨 (`output_level_process` 포팅)
   - out: `OSC_OUT0..4`(U2.28/29/33/34/35), `CON_OVLD`, `BUZZER`
   - 명령 in: `B_START`/`B_RESET`(+SEEK) GPIO/EXTI
   - cadence: TIM 2ms/10ms로 Timer0/Timer1 ISR 등가 (superloop+SysTick 위)
   - 안전(overload) 직결 → **HW 실측 우선** 강제
3. **비포팅**: 7-seg/`display_*`(dead), comm IPC(single MCU).

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
