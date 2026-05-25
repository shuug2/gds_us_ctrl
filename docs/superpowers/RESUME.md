# RESUME — 다음 세션

> **상태 (2026-05-25)**: **Stage B LCD app 데이터 main 머지 완료** (`540008d`, tag `hw-revA_fw-stage-b`). FRAM(FM24C16B) config load + `init_lcd_mode` 포팅. HW 검증 통과. `main` 단독, working tree clean.
> **다음 작업**: 사용자 확정 대기 — 후보는 **Stage C (Modbus RTU on USART6)**, Stage A I/O(GPIO), scope-b LCD 확장, 또는 ATmega16 흡수.

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

## 다음 슬라이스 후보 (사용자 결정 필요)

1. **Stage C — Modbus RTU on USART6** (속도/패리티는 EEPROM `comm_speed_idx`/`comm_parity_idx` 가변, Stage B에서 이미 FRAM에 로드됨)
2. **Stage A I/O** — CON_OVLD / CON_START / CTRL_OSC0~4 GPIO (구 회로도 + ATmega16 핀 역할 참조)
3. **scope-b LCD 확장** — `change_lcd_page` per-page 문자열 포맷(`DISP_STD_DATA`, time2str/energy2str), SETUP 페이지 (DGUS `14ShowFile.bin` VP맵 참조: `hw/lcd/dgus/README.md`)
4. **ATmega16 흡수** — 초음파 실시간 제어 로직 (`ref/atmega16` 분석 → STM32 내장)

정석 흐름 동일: worktree → brainstorming → spec → plan → subagent/Codex 실행 → HW 검증 → 머지.

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
