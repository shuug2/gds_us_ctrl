# Changelog

## [Unreleased]

### 2026-05-05 — Phase 1+2 Bootstrap 완료

- CubeMX UI 의존 제거 결정. HAL/CMSIS는 `fw/vendor/` in-tree read-only 카피로 동결.
- 디렉토리 재편: `fw/cube/` → `fw/vendor/` + `fw/{src,include,drivers,openocd}/` 신설.
- 빌드 시스템: CMake + arm-none-eabi-gcc 툴체인 + Ninja, OpenOCD/ST-LINK V3 flash/debug.
- **Phase 1**: HSI ×12 = **96 MHz** SystemCoreClock 부팅. 빈 main while 루프 도달 GDB 검증 완료 (PC=0x80002ea, MSP=0x20008000).
- **Phase 2**: TIM11 1 kHz 틱 (PSC=95, ARR=999, NVIC priority 5), USART6 115200 8N1 monitor (PC6/PC7 AF8, blocking TX), PB3 1 Hz heartbeat (50% duty).
- 빌드 결과: FLASH 22 060 B / 128 KB (16.83%), RAM 2 520 B / 32 KB (7.69%).
- HW 검증: ST-LINK V3 + STM32F410RBT 보드에서 banner `[boot] gds_us_ctrl phase2 ready` + 1 Hz `[t=N ms] hello` + PB3 LED 점멸 모두 관찰. fault 미히트.
- 발견 + 해소된 결함:
  - `TIM_AUTORELOADPRELOAD_DISABLE` (no underscore) 매크로 오타 → `TIM_AUTORELOAD_PRELOAD_DISABLE` (commit `de0c35f`).
  - CMake genex `$<$<CONFIG:Debug>:-Og;-g3;-gdwarf-2>` semicolon 처리 → 각 flag를 별도 genex로 분리 (commit `8d67a7d`).
  - Cortex-M4 r0p1 6 HW BP 한계 — fault handler 모두 + main 동시 break 시 7개째 거부. 디버그 시 break 갯수 제한.
- `STM32_TOOLCHAIN` env var stale 경로 이슈 — 빌드 시 `env -u STM32_TOOLCHAIN cmake ...` 우회.
- 산출 spec: `docs/superpowers/specs/2026-04-26-phase1-2-bootstrap-design.md`
- 산출 plan: `docs/superpowers/plans/2026-04-26-phase1-2-bootstrap.md`
- RESUME 아카이브: `docs/superpowers/historical/2026-05-05-RESUME.md`

### 2026-04-26 — 분석 세션
- 프로젝트 생성
