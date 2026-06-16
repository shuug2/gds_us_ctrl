# NEXT_STEPS — 다음 세션 진입 가이드

> CLAUDE.md 에 명시된 first-load 문서. 새 세션 시작 시 본 파일을 가장 먼저 읽고 진행 상황 + 다음 작업을 확인.
>
> **본 문서 최신화: 2026-06-17 b** — weld 슬라이스3 **CODE-COMPLETE(미머지)** + **SEEK/RESET 효과 spec 확정**(브랜치 `feat/stage-seek-reset`, 구현 미시작) 반영. 변경 이력 = `docs/changelog.md`(최신 위), 세션별 상태 로그 = `docs/superpowers/RESUME.md`(SessionStart 자동 로드).

---

## 1. 현재 상태 (2026-06-17)

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
| Weld-cycle | 공압 프레스 사이클 FSM 흡수 — slice 1 DELAY FSM + slice 2 energy_ctrl WELD exit (host + HW-regression verified; energy 누산/exit·사이클 E2E는 6b/슬라이스4) | `hw-revA_fw-stage-weld1` / `-weld2` |

> ⚠ `hw-revA_fw-stage-c2a`는 **없음** — pre-refactor slice 2a는 1s PHY-폴 버그 보유라 태그하지 않음. `-c2b`가 static+DHCP 전부 커버.

**slice-2 deferred HW (2026-06-13 j 전수 종결, 코드 무수정)**:
- ① **ICON_RUN over TCP = PASS** — START→US_COMM run→on-time ceiling 자동정지(실측 537–617ms = 560ms)→IDLE + ICON_RUN 육안
- ② **RTU FC06 회귀 = PASS** — FC03 미러(`55/56/567` = TCP 동일) + FC06 클램프(80/30→50/120→100/55), slice 2 무회귀
- ③ **RAM-only 재리스 = 동작 규명·수용** — DHCP 리스 IP가 LCD `comm_mode=ETH_STATIC` 저장 시 static IP로 FRAM에 굳음(가설 맞음, 무수정). 상세 = 메모리 `project_eth_dhcp_static_persist`

### 1.2 남은 작업 (전부 미착수)

**HW-gated deferred** — 전압 가변 / 실 초음파 rig 있어야 **검증** 가능 (단 설계·코드는 지금도 가능):
- **B-SEAM OSC 물리 구동** — 레귤레이션 compute의 마지막 블로커(OSC 출력 바인딩; 분석 §6 "명령 3선 active-LOW 레벨 미러" 가설, 벤치 측정으로 확정)
- **6b signal calibration** — `>>2` 정규화 + 2.56V↔3.3V 도메인 실측 보정, ch0/scaled 물리단위, ADC offset·gain, OSC 비트매핑·극성
- **SEEK/RESET 효과** — **brainstorming + spec 완료 2026-06-17 b**(브랜치 `feat/stage-seek-reset`, spec `1bd3ce8`; §2.2). 범위 = 상태머신 + 500ms 자동체인/해제 + ICON 렌더(**HW 불요**, host-test, 순수 FSM 분리 `app_seek_reset_fsm`); **물리 OSC 신호 구동만 B-SEAM/6b 이연**(M16 흡수→CTRL_OSC* 극성 미확인). 다음 = writing-plans. ⚠ 이 항목은 HW 불요로 재분류(아래 리스트는 검증 게이트가 HW인 것들).
- **overload 보호** — CON_OVLD 입력 + 보호 동작
- **weld-cycle 머신** — 슬라이스1(DELAY FSM) **main 머지 완료**(`hw-revA_fw-stage-weld1`, host + HW-regression verified; §1.1 표). 남은 슬라이스: **energy(2) = MERGED 2026-06-14 e**(`hw-revA_fw-stage-weld2`, host + HW-regression verified; energy 누산/exit 절대 E2E는 6b/실 rig 이연) / **multi(3) = CODE-COMPLETE 2026-06-17 미머지**(브랜치 `feat/stage-weld-cycle-slice3-multi`, 3커밋 `49ca2c7`→`52a24f2`→`f9c4ac9`, host+build verified, 최종 통합 cpp APPROVED; **HW 회귀확인=보드 게이트** → §2.2) / **TRIGGER+물리 SW_START+센서+실 SOL_DN GPIO+안전 abort(4)** 미착수. 슬라이스4는 HW-gated(물리 트리거+센서+실 SOL_DN). ⚠ **슬라이스4 must-fix(cpp-review LOW-1)**: `weld_amplitude`의 `output_power<50` 진폭 언더플로 — Modbus는 `app_modbus.c` [50,100] 클램프하나 **LCD `app_lcd_input.c:752` `LV_OUT_POWER`는 클램프 없음** → 물리 트리거+실 I2C_POT 연결 시 HIGH. 슬라이스4 진입 시 LCD 입력에 `if(data16<50u) data16=50u;` 미러(기존 직접 set_pot도 동일 pre-existing 노출). ⚠ M1(글루 tick `=now` 누적슬립): 슬라이스4 실 공압 dwell엔 `app_weld.c`를 `s_prev_ms += WELD_TICK_MS`로(코드 주석 있음).

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

- **weld 슬라이스2 (energy_ctrl) = MERGED 2026-06-14 e** (머지 `d32d014`, 태그 `hw-revA_fw-stage-weld2`, host + HW-regression verified). HW 회귀: 직접-초음파 ceiling 무회귀 + ICON_RUN 육안 / §6 deviation(energy_ctrl=ON 직접런 ceiling 종료) / work_cnt 0 / Modbus 무회귀. **#2 energy 누산 점등은 벤치 무신호(DISP_POWER=0)로 6b/실 rig 이연**(누산 절대 E2E + divisor REG_ENERGY_DIV=250 검증은 실 초음파 rig). ⚠ DISP_ENERGY=wire **0x05**(mbpoll `-r 6`), 0x16 아님.
- **weld 슬라이스3 (multi_ctrl) = CODE-COMPLETE 2026-06-17 미머지** (브랜치 `feat/stage-weld-cycle-slice3-multi`, 3커밋 `49ca2c7`→`52a24f2`→`f9c4ac9`, host+build verified, 최종 통합 cpp-reviewer APPROVED). WELD 2단 진폭 스테핑(진입 `limit_mo_out1` → `limit_mo_time1`서 `amp_change` 엣지+`limit_mo_out2` → `limit_mo_time2`서 WELD 종료). 아키텍처 = FSM 내부 상태(`s_multi_stage`/`s_multi_elapsed`, app_reg/주입 불요 — 슬라이스2보다 단순); 진폭 emit 단일 설계(`weld_start`+`amp_change` 같은 `set_amp` hook); 우선순위 multi>energy>시간(기존 exit 게이팅); comp_time 미적용; 언더플로 가드(`weld_mo_amplitude` `<50→0`); 타이밍 액면가 10ms tick(samd20 100ms 절단 미재현). 범위=weld-only(직접 START 스테핑 안 함=§6 deviation). spec/plan=`docs/superpowers/{specs,plans}/2026-06-16-stage-weld-cycle-slice3-multi*.md`.
- **★ 다음 세션 = 슬라이스3 보드 회귀확인 + 머지** (이 브랜치 체크아웃): ① 직접-초음파(START→~560ms ceiling+ICON_RUN) 무회귀 ② `multi_ctrl=ON` 직접 START가 스테핑 **안 함**(단일 진폭, ceiling 종료) = §6 deviation 확인 ③ work_cnt 0(weld dormant 구조증명) → `--no-ff` 머지 + 태그 `hw-revA_fw-stage-weld3`(host + HW-regression verified; 스테핑 자체 E2E는 슬라이스4). ⚠ 보드는 slice3 미플래시 — 먼저 `env -u STM32_TOOLCHAIN cmake --build fw/build` + 플래시. **slice3 보드 머지는 `feat/stage-weld-cycle-slice3-multi` 브랜치에서**(seek-reset 브랜치는 그 위 stack).
- **★ 또는 = SEEK/RESET writing-plans + 구현** (HW 불요, 이 세션에서 spec 완료). 브랜치 `feat/stage-seek-reset`(slice3 tip stack, spec `1bd3ce8`) 체크아웃 → spec 사용자 리뷰 → `superpowers:writing-plans` → subagent-driven. 신규 `app_seek_reset_fsm`(host-test) + 글루 + ICON 렌더 + 양방향 RUN 직교; 물리 OSC=hook stub(B-SEAM). spec=`docs/superpowers/specs/2026-06-17-stage-seek-reset-design.md`. ⚠ seek-reset 머지는 slice3 머지 후(stack 의존).
- **기타 신규(HW 불필요)** — SEEK·RESET 효과 / overload 보호. `brainstorming`→spec→plan→구현.
- **HW-gated** — weld 슬라이스4(TRIGGER+물리 SW_START+실 SOL_DN/센서+안전abort + LOW-1 LCD 클램프) / B-SEAM OSC 물리 구동 + 6b calibration(실 초음파 rig/스코프).
- 진입 절차 = **§3** (brainstorming → spec → writing-plans → subagent-driven → finishing).

### 2.3 보드 현 상태 (2026-06-14 e 마감 시점)

- **SERIAL / addr=1 / 9600 / EVEN** (벤치 기본; USART6=Modbus 점유 → mon 115200 비가용, mon 필요 시 LCD에서 addr=NONE), OUT_POWER=55, EN_ENERGY=0(복원), FRAM `ether_ip=.70` 잔여(무해). **weld-2 펌웨어**(태그 `hw-revA_fw-stage-weld2`) 플래시됨. ⚠ Modbus 검증 시 zsh는 unquoted 변수 word-split 안 함 → mbpoll 플래그는 **인라인**으로(또는 `${=VAR}`); DISP_ENERGY=wire **0x05**(`-r 6`).
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
- mon ↔ Modbus RTU = USART6 공유. SERIAL+addr!=0면 Modbus가 점유 → USART6=comm 속도(9600 9E)·mon 게이트오프. addr=NONE으로 풀리면 `usart6_init()`이 **115200 8N1로 정확히 복원**(SWD 검증됨). ETH 모드는 mon 동작.
- ⚠ **baud 의심 시 추측 말고 SWD로 USART6 레지스터 직독**(시리얼 캡처는 macOS 포트 재개방 플레이키 + 상태 thrash로 신뢰 낮음): `mdw 0x40011408`(BRR) → **833(0x341)=115200 / 10000(0x2710)=9600**(BRR=PCLK2 96MHz/baud). `mdw 0x4001140C`(CR1) → M(bit12)=0·PCE(bit10)=0 → mon 8N1 / M=1·PCE=1 → Modbus 9E. (2026-06-14 e: "mon baud 미복원 버그" 의심 → controlled SWD 실험으로 **버그 아님** 규명. live addr은 g_cfg `mdb 0x20000a86`로 확인.)

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

> **본 문서 갱신 시점**: 2026-06-17 b (weld 슬라이스3 CODE-COMPLETE 미머지 + SEEK/RESET spec 확정 반영)
> **다음 갱신 시점**: SEEK/RESET writing-plans/구현 시, 슬라이스3 보드 머지 시, 또는 신규 스테이지 시작 시
