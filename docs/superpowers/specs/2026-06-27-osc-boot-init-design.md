# OSC 보드 부팅 초기화 시퀀스 — 설계

> **요약**: 전원 투입 시 OSC 보드 자가 초기화(자체 초음파 600ms 출력)를 STM32가 PB12 피드백으로 감지하고, 종료 후 RESET(PB10)·SEEK(PB2) 펄스를 보내 OSC 보드 초기화를 완료하는 부팅 시퀀스. 별도 모듈 `app_osc_init`(순수 FSM + 글루)로 구현하며, RESET/SEEK 물리 핀 구동은 board 레벨 setter로 두어 향후 `app_seek_reset` 명령 FSM과 공유한다. 기존 ~4초 ADC warm-up과 병렬로 부팅 중 1회 실행(타임아웃이 4초 안에 들어 명령 게이트는 기존 warm-up 그대로 유지).

## 목표

OSC 보드는 전원 투입 후 자체적으로 약 600ms 후부터 초음파를 600ms 출력하며 자가 초기화한다. 그 출력은 PB12(OSC→STM32, active-HIGH)로 STM32가 감지한다. 자가 초기화(PB12 H→L)가 끝나면 STM32가 RESET·SEEK 명령을 보내 OSC 보드 초기화를 마무리해야 정상 동작이 가능하다. 이 시퀀스를 펌웨어 부팅 초기화에 반영한다.

## 핀 매핑 (확정)

| 핀 | 방향 | 역할 | 폴라리티 |
|----|------|------|----------|
| **PB12** | 입력 (OSC→STM32) | 초음파 출력 피드백 (`io_read_usfb`) | active-HIGH (출력 중 H) |
| **PB10** (OSC_OUT1) | 출력 (STM32→OSC) | RESET 신호 (`board_reset`) | **active-LOW** (idle HIGH) |
| **PB2** (OSC_OUT0) | 출력 (STM32→OSC) | SEEK 신호 (`board_seek`) | **active-LOW** (idle HIGH) |
| PB14 | — | 이 시퀀스 **미관여** (정상 운전 초음파 출력 게이트로 유지) | active-LOW |

PB2/PB10은 board.c가 `CTRL_OSC_OUT_PINS`로 이미 idle-HIGH 출력 설정 중. 부팅 초기화 후에도 idle(HIGH)로 복귀.

## 시퀀스 (이벤트 기반 + 타임아웃 폴백)

| 상태 | 전이 조건 | 출력 |
|------|-----------|------|
| `OSC_WAIT_H`  | PB12=H 감지 (폴백 1000ms) | — |
| `OSC_WAIT_L`  | PB12=L 감지 (폴백 1500ms) | — |
| `OSC_GAP`     | 150ms 경과 | — |
| `OSC_RESET`   | 200ms 경과 | RESET active (PB10 LOW) |
| `OSC_SEEK`    | 100ms 경과 | SEEK active (PB2 LOW) |
| `OSC_DONE`    | — | 완료 (모든 출력 idle) |

- **이벤트 기반**: 정상 시 PB12 H→L 실제 전이에 동기. 폴백 타임아웃은 OSC 보드 부재/고장 시 무한대기 방지용.
- **타임아웃 보장**: 최악 1000+1500+150+200+100 = 2950ms < ADC warm-up 4000ms → 부팅 명령 무시 게이트(app_reg warm-up)가 항상 OSC init을 덮음.
- **순서 고정**: RESET → SEEK. 펄스 후 각 핀 idle(HIGH) 복귀.
- **부팅 1회**: DONE 후 재실행 없음.

## warm-up 통합

부팅 시 OSC init과 기존 ~4초 ADC warm-up을 **병렬** 진행(둘 다 유지). OSC init은 최악 2.95초 내 완료(타임아웃 보장)이므로 START 등 명령 무시 게이트는 **기존 `app_reg` warm-up(~4초)을 그대로 사용** — OSC init 완료 플래그를 app_reg에 연동할 필요 없음.

## 구조 (Approach A — 별도 모듈)

기존 "순수 FSM + 글루" 패턴(`app_seek_reset`, `app_weld`)을 따른다.

- **`app_osc_init_fsm.{c,h}`** (순수, host-test): 입력 = PB12 레벨 + 경과 ms; 출력 = 상태 + RESET/SEEK 레벨. HAL 비의존.
- **`app_osc_init.{c,h}`** (글루): 10ms tick(`SR_TICK_MS` 패턴), `io_read_usfb()` 주입, FSM step, RESET/SEEK 레벨 → `board_reset`/`board_seek` 호출. 부팅 시 1회 init.
- **`board.c`/`board.h`**: `board_reset(bool on)`(PB10), `board_seek(bool on)`(PB2) setter 신설 (active-LOW). app_seek_reset stub(`app_seek_reset_hook_signal`)이 향후 *같은 setter*에 바인딩 가능(중복 방지).
- **배선**: `main.c` boot에서 `app_osc_init_init()`, 슈퍼루프에서 `app_osc_init_tick()` 호출(app.c).

**대안**: B(app_reg에 직접 통합 → 코어 비대), C(app_seek_reset 확장 → 부팅/명령 타이밍 혼재). A 채택 — 관심사 분리·독립 테스트·setter 공유.

## 테스트

- **host-test** `test_app_osc_init_fsm`: 정상 경로(H→L→GAP→RESET→SEEK→DONE 타이밍), 폴백(H 미감지 1000ms→진행, L 미감지 1500ms→진행), 출력 레벨 span 검증, 1회성(DONE 유지).
- **HW**: 실배선 rig — PB12 OSC 피드백 감지, PB10/PB2 펄스 폭(200/100ms) + 순서 + 폴라리티(active-LOW), 부팅 후 정상 운전 진입.

## 범위 밖 (이연)

- `app_seek_reset` stub의 실 핀 구동화(명령 RESET/SEEK가 PB10/PB2 구동) — board setter는 준비하되 바인딩은 별도 작업.
- PB14 초음파 출력 게이트 거동 변경 없음.
