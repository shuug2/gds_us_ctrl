# NEXT_STEPS — 다음 세션 진입 가이드

> CLAUDE.md 에 명시된 first-load 문서. 새 세션 시작 시 본 파일을 가장 먼저 읽고 진행 상황 + 다음 작업을 확인.

---

## 1. 현재 상태 (2026-05-25 기준)

> **⚠ 활성 작업 (2026-06-01 갱신) — 아래 표보다 우선**: LCD 포팅 = ✅ main 머지 완료(`acb4be1`, tag `hw-revA_fw-stage-lcd`). **Stage D slice 1 (레귤레이션 코어 compute) = 구현 완료** — 브랜치 **`feat/stage-d-regulation-core`** (main 미머지). spec→plan→inline 구현(6 커밋), 빌드 0-warning, 호스트 단위테스트 PASS, **cpp-reviewer APPROVED**. **compute 파이프라인만**(2ch ADC→×6 scale→21엔트리 lookup→`lcd_measure_t` 발행); OSC 물리 구동 = **B-SEAM 벤치 측정까지 DEFERRED**. **(2026-06-02 갱신) HW 기능검증 6a = PASS** (compute liveness/무회귀/OSC idle-HIGH 스코프/LCD provider). Task 6은 6a(기능, 완료) + **6b(신호 calibration, HW 준비 후 = DEFERRED)** 로 분리. **(2026-06-03 갱신) slice 1 = main 머지 완료(`5aea06f`, tag `hw-revA_fw-stage-d`). Stage D slice 2a(상태머신 + soft-start 램프) = 코드 완료(Task 1~3, 브랜치 `feat/stage-d-slice2-softstart` tip `ae24ec4`, 빌드 0-warning, 호스트 테스트 PASS, 2-stage 리뷰 APPROVED). 남은 작업 = slice 2a Task 4 HW 검증(보드 연결 시) → 최종리뷰 + 머지/태그.** 정밀 상태 = `docs/superpowers/RESUME.md`(자동 로드, 2026-06-03 블록)·`docs/changelog.md 2026-06-03`·`HANDOFF.md`(루트)·plan `docs/superpowers/plans/2026-06-03-stage-d-slice2-softstart-ramp.md`. 아래 1.1 표는 Stage A/B 머지 시점(2026-05-25) 스냅샷.

### 1.1 슬라이스 현황

| Phase | 상태 | tip | 비고 |
|-------|------|-----|------|
| Phase 1+2 Bootstrap | ✅ main 머지 완료 (2026-05-05) | `b8afe1c` (merge) | 96 MHz HSI×12 + TIM11 1ms tick + USART6 mon + PB3 heartbeat |
| Stage A LCD I/O | ✅ **main 머지 완료 (2026-05-25)** | `4651453` (merge), tag `hw-revA_fw-stage-a` | DGUS LCD wire 통신 + 1Hz cadence 검증. 33 commits 병합 |
| Stage B LCD app 데이터 | ✅ **main 머지 완료 (2026-05-25)** | `540008d` (merge), tag `hw-revA_fw-stage-b` | FRAM(FM24C16B @0x50) config load + `init_lcd_mode` 포팅. HW 검증 통과(BOOT0 forced-jump). 7 commits. FLASH 22.30%/RAM 8.81% |
| ATmega16 FW↔I/O 분석 | ✅ 완료 + **보드-진실 정정 (2026-05-25 후속)** | — | 산출물 `docs/superpowers/analysis/atmega16-io-behavior.md` (**§0.1 정정 절 추가**). OSC 매핑 보드 직독 확정 + 소스오브트루스 전환(디컴파일→보드 측정) |
| **사용자 HW 재측정 (다음, 차단)** | ⏸ **측정 대기** | — | 분석 §0.1 B1(5 OSC핀 방향)·B2(극성/비트매핑)·B3(전달함수)·B4(ADC ch1)·B5(state 0/1). 사용자가 측정 후 행동 재분석 제공 예정 |
| **Stage D — Ultrasonic regulation 흡수** | ⬜ 측정 후 | — | 분석 §0.1/§8. 구조부(ADC층·TIM cadence·상태머신 골격·출력드라이버 추상화) 선포팅 가능, 전달함수·핀방향·매핑은 B1~B5 대기. "ref verbatim 포팅" 전제 폐기 |
| Stage C — Modbus RTU on USART6 | ⬜ 미시작 (Stage D와 독립) | — | 속도/패리티 = FRAM `comm_speed_idx`/`comm_parity_idx` |
| Stage A I/O (Stage D와 일부 중첩) | ⬜ 미시작 | — | CON_OVLD / CON_START / CTRL_OSC0~4 GPIO |

> **2026-05-25 머지/정리 세션 처리 내역**:
> - Stage A 머지 전: `docs/requirements.md` 보강분 커밋(`e322644`), stale NEXT_STEPS/RESUME → `historical/` archive(`cea0c3a`)
> - `--no-ff` 머지(`4651453`) + 태그 `hw-revA_fw-stage-a`, 빌드 검증 FLASH 18.94% / RAM 8.37% ✅
> - repo 정리(`25fb41f`): `ref/`·`fw/cube/*.ioc`·`AGENTS.md`·`docs/fw_analysis.md`·`.claude`/`.codex` track, `graphify-out/` gitignore
> - worktree `gds_us_ctrl-stageA` + 브랜치 `feat/stage-a-lcd-io` 삭제 — 현재 **`main` 단독, working tree clean**
> - origin push ✗ (로컬만)

### 1.2 Stage A 작업 요약 (머지 완료)

- 33 commits, 13 Task 완료. 모든 substantive Task spec compliance reviewer ≥ 12/12 ✅ APPROVE
- Build: FLASH 18.94% / RAM 8.37%. HW verify: wire-level + cadence + fault ✅
- visible 페이지 변경은 Stage B 의존 (samd20 application 데이터 사전 셋업)

상세: **`docs/superpowers/historical/2026-05-06-RESUME.md`** (489 라인 — 전체 상태 + reviewer 결과 + drift 정정 + HW verify 결과).

---

## 2. 다음 세션 진입 시 First Step

### 2.1 사전 점검

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl   # main repo (stageA worktree 는 삭제됨)
git status                             # working tree clean 기대
git log --oneline -5
git tag -l 'hw-revA*'                  # hw-revA_fw-stage-a 확인
```

### 2.2 결정 완료 — 다음 슬라이스 = **Stage B**

사용자 확정 (2026-05-25): **다음 세션은 Stage B 시작.** (머지 여부 / 슬라이스 선택은 이미 결정됨 — 추가 질문 불필요)

- Stage B = LCD application 데이터 사전 셋업 — samd20 `init_lcd_mode` 흐름 포팅, Stage A 의 visible 페이지 변경 완성
- 진입 절차는 **§4** 그대로 따름. 정석 흐름:
  1. 새 worktree 생성 (`feat/stage-b-lcd-app`)
  2. `superpowers:brainstorming` — §4.2 의 Q1~Q5 결정점 탐색
  3. spec 작성 + self-review → plan 작성
  4. `superpowers:subagent-driven-development` 로 Task 1 부터 진행

---

## 3. Stage A 머지 절차 — ✅ 완료 (2026-05-25, 기록용)

> 아래 절차로 머지 완료됨 (`4651453` + tag `hw-revA_fw-stage-a` + repo 정리 `25fb41f`). 다음 세션에서는 재실행 불필요 — 기록 보존용.

### 3.1 권장 흐름 — main repo 에서 직접 머지

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl   # main repo (worktree 가 아닌 본 디렉토리)
git checkout main
git pull origin main 2>/dev/null || true
git fetch ../gds_us_ctrl-stageA feat/stage-a-lcd-io
git merge --no-ff feat/stage-a-lcd-io -m "Merge Stage A LCD I/O Bring-up (33 commits)"
git tag hw-revA_fw-stage-a   # CLAUDE.md 태깅 규칙
```

### 3.2 머지 후 정리

```bash
# Worktree 제거 (선택)
cd /Users/tknoh/dev/work/gds_us_ctrl
git worktree remove ../gds_us_ctrl-stageA
git branch -d feat/stage-a-lcd-io

# 또는 worktree 유지 (Stage B 작업도 worktree 분리 진행할 경우)
```

### 3.3 머지 후 검증

```bash
# main 에서 빌드 + 검증
cd fw && env -u STM32_TOOLCHAIN cmake --build build
arm-none-eabi-size build/gds_us_ctrl.elf
# Expected: text 24336 / data 476 / bss 2276
```

---

## 4. Stage B 시작 절차 (다음 슬라이스 권장)

### 4.1 Stage B 개요

**목표**: Stage A 의 LCD wire 통신 + 1Hz cadence 위에 application 데이터 사전 셋업 layer 추가. visible 페이지 변경 완성.

samd20 ref `main.c:3175` `init_lcd_mode()` + `main.c:2942` `change_lcd_page()` 흐름 포팅:
- `send_model_str(model_freq, model_type)` — "GDS-15/20/30..." 모델 문자열 송신 (`main.c:2376`)
- 11+ VP 사전 채움: `DISP_ENERGY_EN`, `DISP_MULTI_EN`, `LV_DM_DELAY/WELD/HOLD`, `LV_TM_WELD/HOLD`, `LV_WORK_CNT`, `LV_ENERGY_EDIT`, `ICON_RESET/SEEK/RUN`, `DISP_HORNDOWN`
- boot 시퀀스: `dgus_set_page(0)` → **delay_ms(1000)** → 위 VP 들 사전 셋업 → `dgus_set_page(LCD_RUN_STD)`
- 페이지별 분기: `LCD_RUN_STD` / `LCD_RUN_HAND` / `LCD_RUN_MULTI` / `LCD_MODEL_SETUP` 등 별도 데이터 셋업

### 4.2 진입 절차 (subagent-driven-development 패턴)

1. **Worktree 생성** (Stage A 와 격리):
   ```bash
   cd /Users/tknoh/dev/work/gds_us_ctrl
   git worktree add ../gds_us_ctrl-stageB -b feat/stage-b-lcd-app
   cd ../gds_us_ctrl-stageB
   ```

2. **`superpowers:brainstorming` 스킬** — Stage B 의 결정점 (Q1-Q5):
   - Q1: 데모 범위 — 단일 페이지 (LCD_RUN_STD) full app 데이터만? 아니면 4-5 페이지 분기? 모델 SETUP 페이지 포함?
   - Q2: 데이터 source — hardcoded (`#define MODEL_FREQ 0`) vs RAM 변수 (Stage C 의 Modbus 로 갱신 가능) vs Flash 영구 저장
   - Q3: VP 매핑 — Stage A 의 dgus_lcd.h DGUS_DEMO_* 매크로 패턴 확장 (DGUS_APP_* 매크로 신설)
   - Q4: API 표면 — `app_lcd_init_mode(model_t)` 단일 진입점? 아니면 `app_lcd_send_model_str()` + `app_lcd_init_data()` + `app_lcd_change_page()` 분리?
   - Q5: 페이지별 데이터 — samd20 의 `change_lcd_page()` 의 페이지 분기 (LCD_RUN_STD vs LCD_RUN_HAND vs LCD_MODEL_SETUP) 모두 포팅 vs 일부만?

3. **spec 작성 + self-review** — `docs/superpowers/specs/<YYYY-MM-DD>-stage-b-lcd-app-design.md`

4. **plan 작성** — `docs/superpowers/plans/<YYYY-MM-DD>-stage-b-lcd-app.md` (Stage A 의 13 Task 패턴 미러)

5. **subagent-driven-development 시작** — Task 1 부터 dispatch

### 4.3 Stage B 의 Stage A 코드 의존성

이미 사용 가능 (Stage A 에서 완료):
- `usart1_init()`, `usart1_send_blocking()` — USART1 raw layer
- `dgus_init()`, `dgus_set_page()`, `dgus_write_u16()`, `dgus_write_u32()`, `dgus_write_bytes()`, `dgus_write_u16_array()`, `dgus_write_text()`, `dgus_read_var()`, `dgus_reset_lcd()` — DGUS 9 함수 풀 패리티
- `dgus_rx_poll()`, `dgus_is_echo()` — RX 파서
- `dgus_frame_t` — frame struct
- `DGUS_DEMO_BOOT_PAGE`, `DGUS_DEMO_UPTIME_VP` 등 매크로 (Stage B 가 확장하거나 별도 `DGUS_APP_*` 매크로 신설 가능)

신규 필요:
- 모델 정보 enum/struct (`model_freq`, `model_type`)
- 페이지별 application 데이터 표 (각 페이지가 어떤 VP 들을 요구하는지)
- 페이지 변경 wrapper (`app_lcd_change_page(page)`) — 페이지별 데이터 셋업 + set_page 묶음

### 4.4 Stage B 도중 발견 가능성 높은 결함

Stage A 에서 이미 발견된 패턴 (typo / Phase 2 reality 불일치 / scope 가정 오류) 과 유사한 drift 가 spec/plan 에 잠재할 수 있음. 발견 시 **Task 8/9/10/12 정정 패턴 미러** 사용:
1. spec 정정 commit
2. plan verbatim sync commit
3. 코드 first-time commit
4. RESUME 갱신 commit

---

## 5. 환경 / 알려진 이슈 (Phase 1+2 + Stage A 누적)

### 5.1 빌드 환경
- `$STM32_TOOLCHAIN` env var stale → `env -u STM32_TOOLCHAIN cmake ...` 필수
- 툴체인: `arm-none-eabi-gcc 15.2.1` (homebrew), `cmake`, `ninja`, `openocd 0.12.0`
- ST-LINK V3J15M7B5S1 (API v3) — Cortex-M4 r0p1 (6 HW BP 한계)

### 5.2 검증 도구
- 빌드 syntax check: `arm-none-eabi-gcc -fsyntax-only` exit=0 + warning 0 이 정답 (clangd LSP 노이즈 무시)
- HW 진단: openocd 직접 명령 (`init` + `halt` + `mdw/mdh/mdb` + `resume` + `exit`)

### 5.3 LCD HW 상태 (Stage A 검증)
- LCD 결선: PA9 (TX) / PA10 (RX) AF7 ✅
- LCD HMI: samd20 시절 동일 (사용자 보고). DGUS T5L echo "OK" 정상 응답 검증됨
- USART1 BRR=0x341 (115246 baud) ✅, CR1=0x202C ✅, CR3=0x1 (EIE) ✅

### 5.4 graphify
- vendor 포함 corpus 3M 단어 / 280 파일 — 임계 초과
- Stage A 두 세션 모두 사용자 graphify 스킵 채택
- 사용자 메모리 `feedback_graphify_after_docs` 정책은 유지하되 매 세션 강제 ✗
- 다음 세션 진입 시 동일 결정 필요

### 5.5 컨텍스트
- Opus 4.7 (1M context) 환경에서 Stage A 단일 세션 ~36% 사용
- 200k context 기준 50% 임계 정책은 다른 모델 세션에서만 의미
- `/context` 정기 점검 권장

### 5.6 핵심 질문 — 2026-05-25 회로도(`hw/schematics/USW_CTRL_V30`) + DGUS 자료로 해소
- **ATmega16 PA4/PC0/PC1/PC4**: PA4=초음파 출력개시 신호 입력, PC0=overload 출력, PC1/PC4=초음파 보드 신호 입력 (ATmega16 측 확정). 구체 동작은 `ref/atmega16` fw 분석 후 STM32 흡수 — 미래 슬라이스. (memory `project_atmega16_absorption`)
- **7-세그먼트**: V30 보드에 없음 — DGUS LCD 단독.
- **`I2C_POT`**: U4 외부 I2C 디지털 포텐셔미터 @0x28 (EEPROM과 I2C1 버스 공유).

### 5.7 ✅ 보드 BOOT0 이슈 — 해결됨 (2026-05-26)
- **BOOT0(U2.60)→GND 연결로 해결.** 이제 평범한 `reset run`으로 플래시 앱 직접 부팅. HW 검증(2026-05-26): `reset halt` → **PC=0x080045c0 / MSP=0x20008000**(플래시 Reset_Handler), `reset run` → mon `[boot] … stage-b ready` / `[cfg] …` / `[t=N ms] hello uptime=N`(1초 cadence) 정상. **force-jump 워크어라운드 불필요.**
- **이전 문제(해결됨)**: R64(BOOT0 풀다운, U2.60) 미실장 → BOOT0 floating high → 매 리셋 시 ST 부트로더 부팅(PC=0x1FFFxxxx), 앱 미실행.
- **다른 미개조 보드 재발 시 fallback**: `openocd reset halt` → gdb `set $sp/$pc`(플래시 벡터 `*0x08000000`/`*0x08000004`) + VTOR(`0xE000ED08`=0x08000000) → `monitor resume`. (memory `project_board_boot0_workaround`)
- **macOS 시리얼 캡처(USART6 mon 115200)**: 단일 fd 필수 — `{ stty 115200 cs8 -parenb -cstopb raw -echo; cat; } < /dev/cu.usbserial-XXXX`. (`stty -f` 후 `cat`은 baud 리셋되어 garbage)

---

## 6. 빠른 명령어 cheat sheet

### 6.1 Stage A 머지 verify
```bash
cd /Users/tknoh/dev/work/gds_us_ctrl-stageA
git log --oneline main..HEAD | wc -l    # Expected: 33
git log -1 --format="%H %s" HEAD        # Expected: 23da1cb docs: Stage A RESUME archive ...
```

### 6.2 Build verify
```bash
cd fw
env -u STM32_TOOLCHAIN cmake -B build -G Ninja
env -u STM32_TOOLCHAIN cmake --build build
arm-none-eabi-size build/gds_us_ctrl.elf
```

### 6.3 Flash + HW probe
```bash
openocd -f fw/openocd/stm32f410.cfg \
  -c "program fw/build/gds_us_ctrl.elf verify reset exit"

# 시리얼 모니터 (USART6 = mon, 115200 8N1)
ls /dev/cu.usbserial-* /dev/cu.usbmodem*
screen /dev/cu.usbserial-XXXXXXXX 115200    # 종료 Ctrl-A k y
```

### 6.4 GDB 직접 메모리 read (Stage A 의 카운터들)
```bash
arm-none-eabi-nm fw/build/gds_us_ctrl.elf | grep -E ' [bd] (s_ms|s_dgus_|s_rx_|prev_lcd_tick)'

openocd -f fw/openocd/stm32f410.cfg \
  -c "init" -c "halt" \
  -c "mdw 0x200004b0 1"     -c "echo {  s_ms (uptime ms)}" \
  -c "mdh 0x20000392 1"     -c "echo {  usart1_rx_drop_count}" \
  -c "mdh 0x20000390 1"     -c "echo {  usart1_rx_error_count}" \
  -c "mdh 0x20000340 1"     -c "echo {  dgus_rx_drop_count}" \
  -c "mdh 0x2000036e 1"     -c "echo {  dgus_tx_timeout_count}" \
  -c "mdb 0x20000398 64"    -c "echo {  s_rx_ring 64 bytes}" \
  -c "resume" -c "exit"
```

(주소는 build 마다 약간 다름 — `arm-none-eabi-nm` 으로 재확인 후 사용)

---

## 7. 응답 / 작업 정책

- **응답 언어**: 한국어 (코드 / commit 메시지 / 파일 경로 / 식별자는 영어). 사용자 메모리 `feedback_korean_responses`.
- **워크플로우**: `superpowers:subagent-driven-development` (Task 별 fresh subagent + 2-stage review). spec compliance reviewer 는 substantive Task 마다, code quality reviewer 는 묶음 (Task 11 build verify 직전, RESUME §4.5 갱신 정책).
- **drift 발견 시**: spec 정정 commit → plan verbatim sync → 코드 first-time commit (Task 8/9/10/12 패턴 미러).
- **subagent dispatch 가드**: worktree only 작업, 메인 repo touch ✗, graphify/doc regen 자동 실행 ✗, 코드 변경 ✗ (read-only review), 빌드 시도 ✗ (controller 가 sanity 통과시킴).

---

## 8. 참조 문서

- 본 슬라이스 상세 archive: `docs/superpowers/historical/2026-05-06-RESUME.md`
- 이전 슬라이스 archive: `docs/superpowers/historical/2026-05-05-RESUME.md` (Phase 1+2)
- 변경 이력: `docs/changelog.md`
- 핀 매핑: `docs/pinmap.md` (Stage A 활성화 표기 포함)
- 요구사항: `docs/requirements.md`
- 프로젝트 컨벤션: `CLAUDE.md` (root)
- samd20 ref 코드: `/Users/tknoh/dev/work/gds_us_ctrl/ref/samd20/` (수정 ✗, Stage B 포팅 source)
- atmega16 ref 코드: `/Users/tknoh/dev/work/gds_us_ctrl/ref/atmega16/` (수정 ✗)

---

> **본 문서 갱신 시점**: 2026-05-25 (Stage A 머지 + repo 정리 완료, Stage B 진입 대기)
> **다음 갱신 시점**: Stage B brainstorming/spec 시작 시 (RESUME.md 새로 작성)
