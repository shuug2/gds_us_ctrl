# Changelog

## [Unreleased]

### 2026-05-25 (후속) — ATmega16 보드-진실 정정 + 소스오브트루스 전환 (코드 변경 ✗, 문서만)

- Stage D brainstorming 중 사용자 보드 직독으로 **OSC 드라이브 매핑 확정**: `CTRL_OSC0..4` ← mega16 **PB1/PB0/PC4/PA7/PC7** (V30 PB2/PB10/PB12/PB13/PB14). 분석 §7 #1(OSC 트리거 물리핀 미해소) **해소**. **PA0(ADC ch0) = 출력 초음파 세기 피드백**(레귤레이션 입력) 확정.
- **소스 오브 트루스 전환**: 1차 = 사용자 보드 측정 + 행동 재분석, 2차(약함) = `m16_conv_v001.c` 디컴파일(**함수명·동작주석 무시**). 코드 정밀추적 결과 누진 드라이브 레벨이 RAM 변수(`disp_*`) + 본문부재 `output_direct_process()`로만 라우팅됨을 확인 → 디컴파일이 출력단을 못 보여줌.
- **뒤집힌 본문 결론**(이제 무시): "IPC=PB0/PB1/PA7/PC4/PC7"(§1), "PB0/PB1/PC7=상태표시 IPC"(§3), "누진패턴→dead display로만 감"(§4). 실제 IPC는 명령선(PA4/5/6)+M_OVLD(PC0) 별개 그룹.
- **측정 대기 (Stage D 차단)**: 분석 **§0.1 B1~B5** — ① 5 OSC핀 방향(in/out; PC4·PA7 코드상 입력) ② 극성·레벨↔비트매핑 ③ 레귤레이션 전달함수 ④ ADC ch1(PA1) 정체 ⑤ `g_main_state` 0/1 두 출력모드. 사용자가 측정+행동재분석 제공 예정.
- **"ref verbatim 포팅" 전제 폐기**. Stage D 결정분: 1슬라이스(피드백 루프, 모듈경계로만 분할), 코드-포팅 먼저+머지前 실측, 안전경계 옵션은 측정 후 재논의.
- 갱신 문서: `analysis/atmega16-io-behavior.md`(§0.1 정정 절 + 배너 + §7 #1 CLOSED 표기), `pinmap.md`(CTRL_OSC 식별 확정/방향·극성 대기, Q8b/Q11), `NEXT_STEPS.md` §1.1, `RESUME.md`, memory `project_atmega16_absorption`.

### 2026-05-25 — ATmega16 FW 동작↔I/O 신호 분석 완료 (코드 변경 ✗)

- 산출물: **`docs/superpowers/analysis/atmega16-io-behavior.md`** (kickoff §6 deliverable). 소스 = `ref/atmega16/m16_conv_v001.c`(디컴파일, 비판적 독해) + `hw/schematics/usw_ctrl_v26_samd20.pdf`(OLD V26, page4=ATmega16 U6, pdftoppm 600/1200dpi 크롭 판독, netlist 없음) + `hw/schematics/USW_CTRL_V30.asc`(NEW netlist).
- **아키텍처 확정**(사용자): ATmega16 = 트리거 생성 + 전류 모니터링(실시간 레귤레이션), SAMD20 = UI+전체제어, OSC = 별도 보드. IPC = 순수 GPIO(`hw_init`이 USART/SPI/TWI 전부 비활성).
- **디컴파일러 noise 정정**: C 헤더의 핀-역할 주석 대부분 추측. `display_*`(PORTD+PB2/3/4)는 V26 미연결(×) = dead legacy 7-seg. `BUZ_M16`/`U1.xx` net 주석 오류 다수.
- **HIGH 확정**: PA4=`M_START`(START in, 3중확인), PC0=`M_OVLD`(overload out, 3중확인). 제어루프: ADC(PA0=`SENS_OUT`) → `lookup_table` 스케일 → `output_level_process` 누진 5비트(0x01→0x1F) ↔ V30 `OSC_OUT0..4` 1:1. ISR cadence: ADC~208µs/Timer0~2ms/Timer1~10ms.
- **미해소(최우선 HW-verify)**: ① OSC 트리거 물리 출력핀 ② ADC ch0/ch1 정체(PA1 미연결인데 ch1 처리) ③ PC1 방향 충돌. 분석 §7.
- 다음 = (선행)HW-verify 패스 → **Stage D Ultrasonic regulation 흡수**(분석 §8). `docs/NEXT_STEPS.md` §1.1 + `docs/superpowers/RESUME.md` 갱신, memory `project_atmega16_absorption` 갱신.

### 2026-05-25 — Stage A main 머지 + repo 자산 정리

- `feat/stage-a-lcd-io` (33 commits) → `main` **`--no-ff` 머지** (`4651453`), 태그 **`hw-revA_fw-stage-a`**. 머지 후 빌드 재검증 FLASH 18.94% / RAM 8.37% ✅ (브랜치 수치 일치).
- 머지 전 정리: 미커밋 `docs/requirements.md` 보강분(시스템 아키텍처 + F1~F6/FW1~FW3) 커밋(`e322644`); stale untracked `docs/NEXT_STEPS.md`·`docs/superpowers/RESUME.md` → `docs/superpowers/historical/` 아카이브(`cea0c3a`, 머지 시 untracked 충돌 회피 + Phase 1~7 로드맵 보존).
- repo 자산 track(`25fb41f`): `ref/`(레거시 SAMD20/ATmega16 포팅 source 8.8 MB), `fw/cube/gds_usctrl.ioc`(핀 config), `AGENTS.md`, `docs/fw_analysis.md`, `.claude/settings.json`·`.codex/hooks.json`(RESUME 자동로드 SessionStart 훅). `graphify-out/` 는 `.gitignore` 추가(재생성 산출물).
- worktree `gds_us_ctrl-stageA` + 브랜치 `feat/stage-a-lcd-io` 삭제 — 현재 `main` 단독, working tree clean. origin push ✗ (로컬만).
- 다음 슬라이스 **Stage B (LCD app 데이터 사전 셋업)** 사용자 확정 → `docs/NEXT_STEPS.md` §4 + `docs/superpowers/RESUME.md` 갱신.

### 2026-05-06 — Stage A LCD I/O Bring-up 완료 (DGUS LCD on USART1, Task 1–13)

- USART1 (PA9/PA10, AF7, 115200 8N1) raw 드라이버 신설 — `fw/drivers/usart1.c`. HAL `Receive_IT` 1-byte 무장 + 64-byte ring buffer + 폴링 TX (10 ms timeout). `HAL_UART_RxCpltCallback` + `HAL_UART_ErrorCallback` 두 vendor weak 함수 instance-USART1 분기 override.
- DGUS 프로토콜 레이어 신설 — `fw/drivers/dgus_lcd.c`. samd20 9 함수 풀 패리티 (`dgus_reset_lcd / set_page / write_u16 / write_u32 / write_bytes / write_u16_array / write_text / read_var`) + 4-state RX 파서 상태머신 (`PS_IDLE → PS_GOT_5A → PS_GOT_HEADER → PS_COLLECTING`) + WR-echo 헬퍼 (`dgus_is_echo`) + 관측성 카운터 (`dgus_rx_drop_count`, `dgus_tx_timeout_count`, `usart1_rx_drop_count`, `usart1_rx_error_count`).
- samd20 결함 5건 명시 회피 (spec §3.7) — LEN [4,26] 검증, 50 ms 벽시계 timeout (sys_tick_get_ms), HAL_UART_Init Error_Handler, 폴링 TX 자연 직렬화, RX ring drop-not-overwrite 카운터.
- 데모 동작: 부팅 시 banner 3줄 (`[boot] gds_us_ctrl stage-a-lcd ready` + `[lcd] usart1@115200 ring=64 prio=5` + `[lcd] init ok, set_page=9, uptime VP=0x1110`) → `dgus_set_page(LCD_RUN_STD)` → 1 Hz 로 `dgus_write_u16(VAR_POWER, secs)` + mon `[t=N ms] hello uptime=N` cadence + LCD echo passive RX 로깅.
- 빌드 결과: FLASH **24,824 B / 128 KB (18.94%)** ✅ (spec §6.2 acceptance < 30 KB), RAM **2,744 B / 32 KB (8.37%)** ✅ (acceptance < 4 KB). 신규 export 심볼 (`usart1_init`, `dgus_init`, `dgus_set_page`, `dgus_write_u16`, `dgus_rx_poll`, `USART1_IRQHandler`, `HAL_UART_RxCpltCallback`, `HAL_UART_ErrorCallback`) 모두 `.text` 확인. warning 0.
- HW 검증: ST-LINK V3 + STM32F410RBT 보드 flash → mon banner 3줄 + 1 Hz hello uptime 출력 ✅. USART1 wire-level 명령 valid 인식 ✅ (LCD 가 매 set_page/write_u16 명령에 DGUS WR-echo `5A A5 03 82 4F 4B` 응답, GPIOA AFRH=0x770 / BRR=0x341 / CR1=0x202C 정상). PC=`app_loop_iter` Thread mode, fault 미히트 ✅.
- **scope 정정 (2026-05-06)**: 초기 spec §6.3 6c "LCD 페이지 시각 전환" 이 samd20 ref 의 `init_lcd_mode` 흐름 (`send_model_str` + DISP_*/LV_*/ICON_* VP 11+ 사전 셋업) 의존을 누락. spec §0.3 OUT OF SCOPE 에 "LCD application 데이터 사전 셋업 — Stage B" 명시 추가, §6.3 6c/6d/6f acceptance 를 wire-level 입증 + mon-side cadence + drop counter 정밀 정의 로 완화. **Stage A 코드 자체 정상**, samd20 레퍼런스 분석 결과 Stage B 슬라이스로 분리.
- spec/plan 정정 패턴 (Task 8 sys_tick_get_ms / Task 9 sys_tick_init,__enable_irq / Task 10 mon_writeln 미러): 발견 → spec/plan 정정 commit → 코드 first-time commit. 모든 substantive Task (4, 6+7, 8, 9, 10) spec compliance reviewer ≥ 12/12 ✅ APPROVE, severity CRITICAL/HIGH/MEDIUM/LOW 0.
- Code quality reviewer (Task 4-10 묶음, code-reviewer agent): HIGH 1 (`HAL_UART_ErrorCallback` 미구현 → ORE 시 RX 영구 정지) + LOW 3 (헤더 가드 `#pragma once` 통일, irq.c mid-file include, `extern Error_Handler` → `#include "clock.h"`) 모두 fix 완료.
- `feat/stage-a-lcd-io` 브랜치 → main 머지 완료 (2026-05-25, `4651453`, tag `hw-revA_fw-stage-a`).
- 산출 spec: `docs/superpowers/specs/2026-05-05-stage-a-lcd-io-design.md`
- 산출 plan: `docs/superpowers/plans/2026-05-05-stage-a-lcd-io.md`
- RESUME 아카이브: `docs/superpowers/historical/2026-05-06-RESUME.md`

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
