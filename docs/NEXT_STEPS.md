# NEXT_STEPS — 다음 세션 진입 가이드

> CLAUDE.md 에 명시된 first-load 문서. 새 세션 시작 시 본 파일을 가장 먼저 읽고 진행 상황 + 다음 작업을 확인.
>
> **본 문서 최신화: 2026-06-14 c** — weld-cycle 슬라이스1(DELAY FSM) **HW 회귀확인 PASS + main 머지 + 태그** 반영. 변경 이력 = `docs/changelog.md`(최신 위), 세션별 상태 로그 = `docs/superpowers/RESUME.md`(SessionStart 자동 로드).

---

## 1. 현재 상태 (2026-06-14 c)

**통합 핵심 기능 대부분 흡수 완료.** STM32F410RBT 단일 MCU로 기존 SAMD20 + ATmega16 기능을 통합 중. LCD·레귤레이션·Modbus(RTU+TCP)까지 main에 있고, 남은 것은 대부분 **실 초음파/가변전압이 있어야 검증 가능한 출력·효과 계층**.

### 1.1 스테이지 현황 (전부 main 머지 완료)

| Stage | 내용 | tag |
|-------|------|-----|
| Phase 1+2 | 부트스트랩 — 96 MHz HSI×12 + TIM11 1ms tick + USART6 mon + PB3 heartbeat | `b8afe1c` (merge) |
| Stage A | DGUS LCD I/O — wire 통신 + 1Hz cadence | `hw-revA_fw-stage-a` |
| Stage B | LCD application 데이터 사전 셋업 (`init_lcd_mode` 포팅) | `hw-revA_fw-stage-b` |
| LCD full port | LCD 전체 거동 포팅 (comm 표시 등, DGUS 에셋 root) | `hw-revA_fw-stage-lcd` |
| Stage D | ATmega16 흡수 — 레귤레이션 compute · 상태머신 · soft-start · RUN 게이트 · m1(param 주입) | `hw-revA_fw-stage-d` / `-d2` / `-d2b` |
| Stage C | Modbus 흡수 — slice 1 RTU(USART6) + slice 2 TCP(W5500 static+DHCP) | `hw-revA_fw-stage-c1` / `-c2b` |
| Weld-cycle | 공압 프레스 사이클 FSM 흡수 — slice 1 DELAY FSM (host + HW-regression verified; 사이클 E2E는 슬라이스4) | `hw-revA_fw-stage-weld1` |

> ⚠ `hw-revA_fw-stage-c2a`는 **없음** — pre-refactor slice 2a는 1s PHY-폴 버그 보유라 태그하지 않음. `-c2b`가 static+DHCP 전부 커버.

**slice-2 deferred HW (2026-06-13 j 전수 종결, 코드 무수정)**:
- ① **ICON_RUN over TCP = PASS** — START→US_COMM run→on-time ceiling 자동정지(실측 537–617ms = 560ms)→IDLE + ICON_RUN 육안
- ② **RTU FC06 회귀 = PASS** — FC03 미러(`55/56/567` = TCP 동일) + FC06 클램프(80/30→50/120→100/55), slice 2 무회귀
- ③ **RAM-only 재리스 = 동작 규명·수용** — DHCP 리스 IP가 LCD `comm_mode=ETH_STATIC` 저장 시 static IP로 FRAM에 굳음(가설 맞음, 무수정). 상세 = 메모리 `project_eth_dhcp_static_persist`

### 1.2 남은 작업 (전부 미착수)

**HW-gated deferred** — 전압 가변 / 실 초음파 rig 있어야 **검증** 가능 (단 설계·코드는 지금도 가능):
- **B-SEAM OSC 물리 구동** — 레귤레이션 compute의 마지막 블로커(OSC 출력 바인딩; 분석 §6 "명령 3선 active-LOW 레벨 미러" 가설, 벤치 측정으로 확정)
- **6b signal calibration** — `>>2` 정규화 + 2.56V↔3.3V 도메인 실측 보정, ch0/scaled 물리단위, ADC offset·gain, OSC 비트매핑·극성
- **SEEK/RESET 효과** — RESET→SEEK 500ms 자동 체인 + SEEK 자동해제(분석 §5)
- **overload 보호** — CON_OVLD 입력 + 보호 동작
- **weld-cycle 머신** — 슬라이스1(DELAY FSM) **main 머지 완료**(`hw-revA_fw-stage-weld1`, host + HW-regression verified; §1.1 표). 남은 슬라이스: **energy(2) = CODE-COMPLETE(2026-06-14 d, host+build verified, 브랜치 `feat/stage-weld-cycle-slice2-energy` 미머지=보드 회귀확인 대기 — §2.2)** / multi(3) 미착수 / **TRIGGER+물리 SW_START+센서+실 SOL_DN GPIO+안전 abort(4)** 미착수. 슬라이스3은 HW 불요(설계·코드·host-test 가능), 슬라이스4는 HW-gated(물리 트리거+센서+실 SOL_DN). ⚠ **슬라이스4 must-fix(cpp-review LOW-1)**: `weld_amplitude`의 `output_power<50` 진폭 언더플로 — Modbus는 `app_modbus.c` [50,100] 클램프하나 **LCD `app_lcd_input.c:752` `LV_OUT_POWER`는 클램프 없음** → 물리 트리거+실 I2C_POT 연결 시 HIGH. 슬라이스4 진입 시 LCD 입력에 `if(data16<50u) data16=50u;` 미러(기존 직접 set_pot도 동일 pre-existing 노출). ⚠ M1(글루 tick `=now` 누적슬립): 슬라이스4 실 공압 dwell엔 `app_weld.c`를 `s_prev_ms += WELD_TICK_MS`로(코드 주석 있음).

**설계상 이연(slice 2)**: DHCP 핫플러그(링크 드롭 후 재획득 — 현재 LINKWAIT→UP 단방향), SERIAL boot-skip.

---

## 2. 다음 세션 진입 시 First Step

### 2.1 사전 점검

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl   # main repo
git status                             # working tree clean 기대 (untracked .understand-anything/, ref/atmega16/M16_reverse/ = 무관)
git log --oneline -5
git tag -l 'hw-revA*'                  # 위 §1.1 태그들 확인

# 빌드 + 호스트 테스트 sanity
env -u STM32_TOOLCHAIN cmake -S fw -B fw/build -G Ninja
env -u STM32_TOOLCHAIN cmake --build fw/build      # 0-warning 기대
make -C fw/test test                                # 4 스위트 PASS 기대 (reg_calc/modbus_core/tcp_frame/weld_fsm)
```

### 2.2 다음 작업 후보 (사용자 선택)

- **★ 다음 세션 = weld 슬라이스2 (energy_ctrl) HW 회귀확인 + 머지** (보드 게이트). 슬라이스2는 **CODE-COMPLETE**(2026-06-14 d, host+build verified, 미머지) — 브랜치 `feat/stage-weld-cycle-slice2-energy` 체크아웃 상태 유지.
  - **무엇**: WELD 종료를 시간(`limit_delay_time2`) 대신 **에너지 도달**(`energy_ctrl && curr_energy>=limit_energy`)로. 아키텍처 Option A(누산=app_reg, 순수 FSM은 curr_energy 주입받아 비교만) + backstop abort(limit_out_time 미도달→`weld_fault`+READY). 산출물 `app_reg_calc.reg_energy_from_acc` / `app_weld_fsm` WELD 분기 / `app_reg` 누산 / `app_weld` 글루. host 4스위트 PASS(weld_fsm 12), 빌드 0-warning(우리 코드).
  - **보드 세션 체크**: ① 직접-초음파(START→~560ms ceiling + ICON_RUN) 무회귀 ② **DISP_ENERGY(Modbus 0x16)/VAR_ENERGY 누산 점등**(energy_ctrl=ON 직접런 0→증가→stop 시 last_energy) ③ energy_ctrl=ON 직접런이 **ceiling 종료**(에너지 조기종료 없음 = spec §6 deviation 확인) → `--no-ff` 머지 + 태그 `hw-revA_fw-stage-weld2`. (사이클 자체 E2E는 슬라이스4 물리 트리거.)
  - 상세 = RESUME.md d블록, spec/plan = `docs/superpowers/{specs,plans}/2026-06-14-stage-weld-cycle-slice2-energy*.md`.
- **weld 슬라이스3 (multi_ctrl)** — 다단 진폭 스테핑(`multi_ctrl_stage`, limit_mo_*); 슬라이스2 후.
- **기타 신규(HW 불필요)** — SEEK·RESET 효과 / overload 보호. `brainstorming`→spec→plan→구현.
- **HW-gated** — weld 슬라이스4(TRIGGER+물리 SW_START+실 SOL_DN/센서+안전abort + LOW-1 LCD 클램프) / B-SEAM OSC 물리 구동 + 6b calibration(실 초음파 rig/스코프).
- 진입 절차 = **§3** (brainstorming → spec → writing-plans → subagent-driven → finishing).

### 2.3 보드 현 상태 (2026-06-14 c 마감 시점)

- **SERIAL / addr=1 / 9600 / EVEN** (벤치 기본; USART6=Modbus 점유 → mon 115200 비가용, mon 필요 시 LCD에서 addr=NONE), OUT_POWER=55, FRAM `ether_ip=.70` 잔여(무해). weld-1 펌웨어(태그 `hw-revA_fw-stage-weld1`) 플래시됨.
- ETH 재검증 시: LCD에서 `comm_mode=ETH_STATIC`(static `.70`) 또는 `ETH_DHCP` 전환 + SAVE + **물리 전원사이클**. ETH E2E 재현 절차 = 루트 `HANDOFF.md`(⚠ 시리얼 캡처 함정 절 필독).

---

## 3. 신규 스테이지 진입 절차 (subagent-driven-development 패턴)

1. **Worktree 생성** (선택, 격리 작업 시):
   ```bash
   cd /Users/tknoh/dev/work/gds_us_ctrl
   git worktree add ../gds_us_ctrl-<stage> -b feat/<stage-name>
   ```
   (단일 슬라이스/소규모는 main에서 직접 브랜치도 가능 — 최근 stage-c/d 슬라이스는 feature 브랜치 직접 사용)

2. **`superpowers:brainstorming`** — 결정점(범위/구조/데이터 source/API 표면/이연 범위) 탐색 후 사용자 확정.

3. **spec 작성 + self-review** — `docs/superpowers/specs/<YYYY-MM-DD>-<stage>-design.md`.

4. **`superpowers:writing-plans`** — `docs/superpowers/plans/<YYYY-MM-DD>-<stage>.md` (Task 분해, HW-gated Task 분리).

5. **`superpowers:subagent-driven-development`** — Task별 fresh subagent + 2-stage 리뷰(spec compliance + cpp-reviewer). 호스트 게이트(빌드 0-warning + 테스트) 통과 후, HW E2E는 보드 게이트로 분리.

6. **finishing-a-development-branch** — HW 검증 통과 시 머지(`--no-ff`, local-authoritative main, origin push ✗) + 태그 `hw-revA_fw-stage-<x>`.

> drift 발견 시: spec 정정 commit → plan verbatim sync → 코드 first-time commit.
> subagent dispatch 가드: worktree/브랜치 only, 메인 무관 touch ✗, doc regen 자동 ✗, 코드 변경 ✗(read-only review), 빌드 시도 ✗(controller가 sanity).

---

## 4. 환경 / 알려진 이슈

### 4.1 빌드 환경
- `$STM32_TOOLCHAIN` env var stale → **`env -u STM32_TOOLCHAIN cmake ...` 필수**.
- 툴체인: `arm-none-eabi-gcc 15.2.1`(homebrew), `cmake`, `ninja`, `openocd 0.12.0`.
- ST-LINK V3 (`/dev/cu.usbmodem*`) — Cortex-M4 r0p1 (6 HW BP 한계).
- 빌드 syntax check: `arm-none-eabi-gcc -fsyntax-only` exit=0 + warning 0 (clangd LSP 노이즈 무시).

### 4.2 펌웨어 구조 핵심
- HAL 핸들 단일 정의 = `src/periph.c`, extern = `include/periph.h`.
- 페리페럴 GPIO는 그 드라이버가 직접 책임(예 `drivers/usart.c`가 PC6/PC7 AF).
- `fw/vendor/` = ST HAL/CMSIS + WIZnet ioLibrary(핀 `220ca7a6`, `_WIZCHIP_=W5500`, 경고격리 lib) — read-only, 편집 ✗.
- MCU 클럭 96 MHz, source of truth = `fw/src/clock.c`.

### 4.3 시리얼 캡처 (USART6 mon, 115200) — ⚠ 함정
- **리다이렉트 형식 필수**: `{ stty 115200 cs8 -parenb -cstopb raw -echo; exec cat; } < /dev/cu.usbserial-AB0MLYXA > /tmp/mon.log &` (`cat /dev/...` 인자형식은 포트를 9600으로 리셋 → garbage).
- 비-UTF8 글리치: `LC_ALL=C tr -d '\000'`로 바이트 처리. 종료: `pkill -x cat`(`-f 'cat'`은 과매칭).
- 깨끗한 부팅 mon = **물리 전원사이클**(openocd reset은 boot 버스트 안 나옴).
- mon ↔ Modbus RTU = USART6 공유. SERIAL+addr!=0면 Modbus가 점유 → mon 게이트오프. ETH 모드는 mon 동작.

### 4.4 보드 BOOT0 — 해결됨 (2026-05-26)
- BOOT0(U2.60)→GND 연결로 평범한 `reset run` 플래시 부팅. force-jump 워크어라운드 불필요. (메모리 `project_board_boot0_workaround`)

### 4.5 회로 핵심 (V30 회로도 + DGUS 자료로 해소)
- ATmega16 PA4=초음파 출력개시 입력, PC0=overload 출력, PC1/PC4=초음파 보드 신호 입력. 7-세그먼트 없음(DGUS 단독). `I2C_POT`=U4 외부 I2C 디지털 포텐셔미터 @0x28(EEPROM과 I2C1 공유, 진폭 제어 실체).

### 4.6 graphify
- **사용 중단 (2026-06-10)** — 재생성 ✗.

---

## 5. 빠른 명령어 cheat sheet

### 5.1 빌드 + 호스트 테스트
```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
env -u STM32_TOOLCHAIN cmake -S fw -B fw/build -G Ninja && env -u STM32_TOOLCHAIN cmake --build fw/build
arm-none-eabi-size fw/build/gds_us_ctrl.elf
make -C fw/test test
```

### 5.2 플래시 + 시리얼 mon
```bash
openocd -f fw/openocd/stm32f410.cfg -c "program fw/build/gds_us_ctrl.elf verify reset exit"
# mon (USART6, 115200) — §4.3 리다이렉트 형식
{ stty 115200 cs8 -parenb -cstopb raw -echo; exec cat; } < /dev/cu.usbserial-AB0MLYXA > /tmp/mon.log &
LC_ALL=C tr -d '\000' < /tmp/mon.log | LC_ALL=C tr -s ' ' | grep -aE '\[boot|\[eth|\[mb|\[cfg'
```

### 5.3 Modbus 검증 (mbpoll)
```bash
# RTU (SERIAL/addr=1/9600/EVEN) — RS-485 마스터, AB0MLYXA, pkill -x cat 먼저
mbpoll -m rtu -a 1 -b 9600 -P even -t 4 -r 7 -c 1 -1 /dev/cu.usbserial-AB0MLYXA      # FC03 read OUT_POWER
mbpoll -m rtu -a 1 -b 9600 -P even -t 4 -r 7 /dev/cu.usbserial-AB0MLYXA 80           # FC06 write (clamp 50..100)
# TCP (comm_mode=ETH_STATIC/DHCP) — 별도 소켓, cat 무관
mbpoll -m tcp -a 1 -t 4 -r 1 -c 12 -1 <board-ip>                                     # FC03 mirror
mbpoll -m tcp -a 1 -t 4 -r 28 <board-ip> 1                                           # START(reg 28); STATUS=reg 30, STOP=reg 29
```
> 레지스터(wire→`-r`=wire+1): OUT_POWER 0x06→7 / RESET 0x19→26 / SEEK 0x1A→27 / START 0x1B→28 / STOP 0x1C→29 / STATUS 0x1D→30. mb_baud[]={2400,4800,9600,19200,38400,115200}, parity 0=EVEN/1=ODD/2=NONE. comm_mode 0=SERIAL/1=ETH_STATIC/2=ETH_DHCP.

### 5.4 RAM cfg 직독 (openocd) — comm/ether 설정 확인
```bash
arm-none-eabi-nm fw/build/gds_us_ctrl.elf | grep ' g_cfg'   # 주소 재확인 (build마다 변동)
# g_cfg+0x2A = [comm_address, speed_idx, parity_idx, comm_mode, ether_ip[4]]
openocd -f fw/openocd/stm32f410.cfg -c "init" -c "halt" -c "mdb 0x20000a86 8" -c "resume" -c "exit"
```

---

## 6. 응답 / 작업 정책

- **응답 언어**: 한국어 (코드 / commit / 파일 경로 / 식별자는 영어). 메모리 `feedback_korean_responses`.
- **코드 수정 범위**: 요청한 부분만 (워크스페이스 규칙). `ref/`·`fw/vendor/` 편집 ✗.
- **워크플로**: `superpowers:subagent-driven-development`(Task별 fresh subagent + 2-stage 리뷰). HW-gated Task는 분리.
- **머지**: `--no-ff` 로컬, origin push ✗(local-authoritative main). 태그 = `hw-revA_fw-stage-<x>` 규칙.
- **컨텍스트**: 50% 임계 일시정지 정책(메모리 `feedback_context_50pct_pause`). `/context` 정기 점검.

---

## 7. 참조 문서

- 변경 이력: `docs/changelog.md` (최신 위)
- 세션별 상태 로그(자동 로드): `docs/superpowers/RESUME.md`
- slice-2 핸드오프: 루트 `HANDOFF.md`
- 핀 매핑: `docs/pinmap.md`
- 요구사항: `docs/requirements.md`
- ATmega16 분석: `docs/superpowers/analysis/` (regulation-core-verified, samd20-m16-ipc-semantics-verified, atmega16-io-behavior 등)
- 프로젝트 컨벤션: `CLAUDE.md` (root)
- ref 코드(수정 ✗): `ref/samd20/`, `ref/atmega16/`
- 과거 RESUME archive: `docs/superpowers/historical/`

---

> **본 문서 갱신 시점**: 2026-06-13 j (Stage C/D 완료 + slice-2 deferred HW 종결 반영)
> **다음 갱신 시점**: 신규 스테이지 brainstorming/spec 시작 시
