# Changelog

## [Unreleased]

### 2026-05-05 — Phase 1+2 Bootstrap 완료 (Chunks 1–13)

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
- `feat/phase1-2-bootstrap` 브랜치(33 commits) → main 머지 (merge commit on 2026-05-05).
- 산출 spec: `docs/superpowers/specs/2026-04-26-phase1-2-bootstrap-design.md`
- 산출 plan: `docs/superpowers/plans/2026-04-26-phase1-2-bootstrap.md`
- RESUME 아카이브: `docs/superpowers/historical/2026-05-05-RESUME.md`

### 2026-04-26 — 분석 세션
- 프로젝트 구조 생성 (`fw/`, `hw/`, `docs/`, `ref/`)
- `CLAUDE.md` 작성 — 통합 목표 (SAMD20 + ATmega16 → STM32F410RBT)
- `ref/samd20/`, `ref/atmega16/` 이전 코드 보관
- CubeMX `.ioc` 분석 → `docs/pinmap.md` 작성
  - W5500, DGUS LCD, I2C EEPROM, 5채널 OSC, TIM5 PWM 등 핀 매핑 확정
- graphify로 코드 분석: 340 노드 / 851 엣지 / 39 커뮤니티
- SAMD20 `main()` 슈퍼루프 추적 — 10-state job_state 머신 구조 파악
- ATmega16 `m16_conv_v001.c` 분석 — 독립 서브 컨트롤러, GPIO 시그널링
- `docs/requirements.md` 작성 — 시스템 아키텍처 (이전 → 신규), F1~F6
- `docs/NEXT_STEPS.md` 작성 — 회로도 미해결 질문 + 포팅 단계별 계획

### 2026-04-26 — FW 분석 마무리 + 회로 답변 통합
- **이전 MCU 간 GPIO 시그널 매핑 확정** (출처: `user_board.h:75-103` + 사용자 회로 동작 확인)
  - `M_START = PB13` (out, active LOW) → ATmega16 PA4 (초음파 출력 개시)
  - `M_SEEK  = PB11` (out, active LOW) → ATmega16 PA5 (공진 주파수 탐색)
  - `M_RESET = PB12` (out, active LOW) → ATmega16 PA6 (초음파 초기화)
  - `M_OVLD  = PB10` (in)               ← ATmega16 PC0 (과부하 알람)
- 통합 시 위 4개 GPIO는 **내부 함수/플래그**로 대체 (us_start/us_seek/us_reset/g_overload)
- 회로 미해결 항목 후속 답변 통합:
  - Q2-잔(PC1) = 무시, Q3(PC4) = 외부 입력, Q5(disp_led_pattern) = 단순 표시 출력
  - Q6(I2C_POT) = 외부 디지털 포텐셔미터 칩 → `i2c_adc_*` 그대로 포팅
  - Q7 = `.ioc`에 PA0 Net Name **`US_PWM_OUT`** 추가 완료
  - Q8 = CTRL_OSC0~4 = 외부 제어 신호 출력 (활성 극성은 추후 회로도 확정)
  - Q9 = TIM11 = 1 ms 시스템 틱
- `docs/fw_analysis.md` 신규 작성 — 포팅 참조용 종합 분석 (job_state 10단계, parse_lcd_comm, decode_comm, W5500/DHCP/TCP, 글로벌 상태, 인터럽트 인벤토리, F410 변환 규칙)
- `docs/pinmap.md` 갱신 — PA0 Net Name, CTRL_OSC0~4 외부 출력 표기, 부록 A(이전 MCU GPIO), 부록 B(결정), 부록 C(잔여)
- `docs/NEXT_STEPS.md` 갱신 — 회로 답변 반영, **개발 순서 4스테이지 재구성** (A: LCD+IO → B: Modbus → C: Ethernet → D: mega16 흡수)
- `docs/requirements.md` FW3 우선순위 = 4스테이지 순서로 재정렬

### 2026-04-26 — 잔여 회로 항목 결정
- **Q4 (7-segment) = 삭제**: DGUS LCD가 모든 표시 흡수. ATmega16 `disp_led_pattern` / 7-seg PORTB·PORTD 출력 폐기
- **Q8b (CTRL_OSC0~4 활성 극성) = 미정**: Stage D `do_control()` 구현 시 회로도 + 실측으로 확정
- **Q10 (ADC1 IN9 채널 추가) = 미정**: Phase 1 CubeMX 보정 + Stage D ADC 흡수 시 확인
- 위 결정 사항을 `docs/pinmap.md` (부록 B/C), `docs/fw_analysis.md` §13, `docs/NEXT_STEPS.md` 잔여 표에 반영

### 모든 회로 미해결 항목 종결 — 다음은 Phase 1 (CubeMX 보정) → Phase 2 (빌드 시스템 + 슈퍼루프 골격) → Stage A (LCD + IO)

### 2026-04-26 — Phase 1+2 brainstorming 진행 + 일시 중단
- `superpowers:brainstorming` 스킬로 Phase 1+2 설계 진행
- 결정 완료: 범위(Phase 1+2) / 통합방식(Hand-written CMake) / 데모(TIM11+UART hello) / 레이아웃(현 구조) / HAL(in-tree) / 플래시(OpenOCD+ST-LINK) / 아키텍처(Phased Modular)
- **새 제약 기록**: USART6 이중 역할 (Phase 2~Stage A: Monitor / Stage B 이후: Modbus RTU). `pinmap.md` USART6 섹션 + Monitor 모듈 backend 교체 가능 인터페이스 설계 반영
- 설계 6 섹션 중 **섹션 1/6 (디렉토리+모듈 경계) 사용자 승인 대기 중 일시 중단**
- 재개 가이드: `docs/superpowers/RESUME.md` 작성 — 결정 사항 / 대기 질문 / 남은 5 섹션 / 작업 재개 절차 모두 포함
- `.claude/settings.json` 신규 — **SessionStart 훅 추가**: 다음 세션 시작 시 RESUME.md를 자동으로 시스템 메시지(사용자 가시) + additionalContext(Claude 컨텍스트)로 주입. 사용자는 한 번에 컨텍스트 복원 가능
