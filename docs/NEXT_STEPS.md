# NEXT_STEPS — 다음 세션 진입 가이드

> CLAUDE.md 에 명시된 first-load 문서. 새 세션 시작 시 본 파일을 가장 먼저 읽고 진행 상황 + 다음 작업을 확인.
>
> **본 문서 최신화: 2026-06-20** — weld 슬라이스3 + SEEK/RESET 효과 **둘 다 HW 검증 + main 머지 + origin 푸시 완료**. **코딩 가능한 스테이지 전부 완료, 남은 작업은 전부 HW-gated.** 변경 이력 = `docs/changelog.md`(최신 위), 세션별 상태 로그 = `docs/superpowers/RESUME.md`(SessionStart 자동 로드).
>
> **⚠⚠ git 히스토리 재작성됨 (2026-06-20)**: `git filter-repo`로 전체 282커밋 author 이메일을 `nogari@gmail.com`으로 재작성(shuug2 GitHub 연결용) → **모든 커밋 해시 변경**. 본 문서·RESUME·changelog·메모리에 적힌 옛 해시(`f6a7eee`/`49ca2c7`/`d32d014` 등)는 **더 이상 존재하지 않음**(`git show` 시 "Not a valid object name"). **안정 레퍼런스 = 태그**; 현재 해시는 `git log --oneline`로 확인. origin = `git@github.com:shuug2/gds_us_ctrl.git`(SSH, filter-repo가 origin 제거 후 재추가). main `1fa5938`(이후 HANDOFF 커밋으로 진행) = origin 동기. 이메일 인증(shuug2 Settings→Emails)은 사용자 미완료(소급 적용).

---

## 1. 현재 상태 (2026-06-20)

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
| Weld-cycle | 공압 프레스 사이클 FSM 흡수 — slice 1 DELAY + slice 2 energy_ctrl exit + slice 3 multi_ctrl 2단 진폭 (host + HW-regression verified; 사이클/스테핑 자체 E2E·에너지 절대값은 6b/슬라이스4) | `hw-revA_fw-stage-weld1` / `-weld2` / `-weld3` |
| SEEK/RESET | samd20 공진 RESET/SEEK 명령 효과 — 순수 FSM + 글루, 자동 체인(RESET→500ms→SEEK→500ms→해제) + ICON + 양방향 RUN 직교 (host + HW verified; 물리 OSC 효과는 hook stub → B-SEAM/6b) | `hw-revA_fw-stage-seekreset` |

> ⚠ `hw-revA_fw-stage-c2a`는 **없음** — pre-refactor slice 2a는 1s PHY-폴 버그 보유라 태그하지 않음. `-c2b`가 static+DHCP 전부 커버.

**slice-2 deferred HW (2026-06-13 j 전수 종결, 코드 무수정)**:
- ① **ICON_RUN over TCP = PASS** — START→US_COMM run→on-time ceiling 자동정지(실측 537–617ms = 560ms)→IDLE + ICON_RUN 육안
- ② **RTU FC06 회귀 = PASS** — FC03 미러(`55/56/567` = TCP 동일) + FC06 클램프(80/30→50/120→100/55), slice 2 무회귀
- ③ **RAM-only 재리스 = 동작 규명·수용** — DHCP 리스 IP가 LCD `comm_mode=ETH_STATIC` 저장 시 static IP로 FRAM에 굳음(가설 맞음, 무수정). 상세 = 메모리 `project_eth_dhcp_static_persist`

### 1.2 남은 작업 (전부 HW-gated, spec 미작성 → brainstorming부터)

코딩/host-test 가능한 계층은 전부 완료. 남은 것은 **실 초음파 rig·가변전압·스코프·물리 입력**이 있어야 검증 가능. slice4 외엔 아직 spec 없음 → 각각 `brainstorming → spec → writing-plans → subagent-driven`(§3).

- **weld 슬라이스4** — TRIGGER 모드 + **물리 SW_START1/2** + 위치센서 + 실 SOL_DN(PB5) GPIO + 안전 abort + **config-validation 클램프**. 슬라이스1~3의 사이클/스테핑 자체 E2E도 여기서. samd20 물리 start = `ref/samd20/main.c:1404-1466`. ⚠ **must-fix(slice1~3 누적 LOW-1)**: `weld_amplitude`의 `output_power<50` 진폭 언더플로 — Modbus는 `app_modbus.c` [50,100] 클램프하나 **LCD `app_lcd_input.c:752` `LV_OUT_POWER`는 클램프 없음** → 물리 트리거+실 I2C_POT 연결 시 HIGH. 진입 시 LCD 입력에 `if(data16<50u) data16=50u;` 미러. + `limit_mo_out`/`limit_energy`/`limit_out_time` config-validation 클램프. ⚠ M1(글루 tick `=now` 누적슬립): 실 공압 dwell엔 `app_weld.c`를 `s_prev_ms += WELD_TICK_MS`로(코드 주석 있음). ⚠ **weld-START 상호작용**: seek-reset START guard가 US_CYCLE START도 게이팅(현재 weld dormant라 inert) → 물리 트리거 도입 시 의식적 결정.
- **B-SEAM OSC 물리 구동** — 레귤레이션 compute의 마지막 블로커(OSC 출력 바인딩; 분석 §6 "명령 3선 active-LOW 레벨 미러" 가설, 스코프 측정으로 확정). seek/reset/weld의 실제 `CTRL_OSC*` 주파수 거동은 전부 hook stub(mon 로그)만 — 여기서 검증.
- **6b signal calibration** — `>>2` 정규화 + 2.56V↔3.3V 도메인 실측 보정, ch0/scaled 물리단위, ADC offset·gain, OSC 비트매핑·극성. weld energy 누산 절대 E2E + divisor(`REG_ENERGY_DIV=250`)도 여기(벤치 DISP_POWER=0 무신호).
- **overload 보호** — CON_OVLD(PC0) 입력 + 보호 동작. OVLD→RESET→자동 SEEK 복구 = **seek-reset FSM 재사용** 설계됨.

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
make -C fw/test test                                # 5 스위트 PASS 기대 (reg_calc/modbus_core/tcp_frame/weld_fsm/seek_reset_fsm)
```

### 2.2 다음 작업 후보 (전부 HW-gated — §1.2)

**코딩 가능한 스테이지 전부 완료.** 남은 후보는 전부 실 HW가 검증 게이트. **HW 없으면 설계(brainstorming+spec)만 선행 가능**, 절대 검증은 HW 세션으로.

- **weld 슬라이스4** — TRIGGER + 물리 SW_START + 센서 + 실 SOL_DN GPIO + 안전 abort + config-validation 클램프(LOW-1 LCD `app_lcd_input.c:752` 포함). 사이클/스테핑 자체 E2E도 여기. (samd20 `ref/samd20/main.c:1404-1466`)
- **B-SEAM OSC 물리 구동** — seek/reset/weld의 실제 `CTRL_OSC*` 주파수 출력(현재 전부 hook stub). 레귤레이션 compute의 마지막 블로커. 실 초음파 rig + 스코프.
- **6b signal calibration** — 진폭/주파수/에너지 절대 보정 + weld energy 누산 절대 E2E + divisor(`REG_ENERGY_DIV=250`). 실 rig.
- **overload 보호** — CON_OVLD(PC0) → RESET → 자동 SEEK 복구(seek-reset FSM 재사용).
- 진입 절차 = **§3** (brainstorming → spec → writing-plans → subagent-driven → finishing).
- ⚠ 머지/푸시 정책 변경: 이제 origin(SSH) 사용 — 머지 후 `git push origin main` + 태그 푸시(§6).

### 2.3 보드 현 상태 (2026-06-20 마감 시점)

- **SERIAL / addr=1 / 9600 / EVEN** (벤치 기본; USART6=Modbus 점유 → mon 115200 비가용, mon 필요 시 LCD에서 addr=NONE), OUT_POWER=55, EN_MULTI=0·ON_TIME=56(테스트 후 복원), FRAM `ether_ip=.70` 잔여(무해). **seek-reset/main 펌웨어**(태그 `hw-revA_fw-stage-seekreset` = 현재 main) 플래시됨. ⚠ ON_TIME(직접런 ceiling)은 **clamp max=100**(1000ms). ⚠ Modbus 검증 시 mbpoll 플래그는 인라인(zsh word-split 안 함); 함수명은 alias 피해 `mbread`/`mbspin`/`mbwrite`; DISP_ENERGY=wire **0x05**(`-r 6`); 첫 트랜잭션 `Invalid CRC`면 더미 read 1회 후 재시도.
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

6. **finishing-a-development-branch** — HW 검증 통과 시 머지(`--no-ff`) + 태그 `hw-revA_fw-stage-<x>` → **origin(SSH) 푸시**(`git push origin main && git push origin --tags`). ⚠ 에이전트 샌드박스 환경은 SSH 인증 불가 → push는 사람 터미널에서.

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
- **머지**: `--no-ff` + 태그 `hw-revA_fw-stage-<x>` → **origin(SSH) 푸시**(`git@github.com:shuug2/gds_us_ctrl.git`). 과거 "local-authoritative, push ✗" 정책은 2026-06-20부로 origin 푸시 사용으로 변경. ⚠ 샌드박스에선 SSH 인증 불가 → push는 사람 터미널. ⚠ 옛 해시는 filter-repo 재작성으로 무효 — 태그로 참조.
- **컨텍스트**: 50% 임계 일시정지 정책(메모리 `feedback_context_50pct_pause`). `/context` 정기 점검.

---

## 7. 참조 문서

- 변경 이력: `docs/changelog.md` (최신 위)
- 세션별 상태 로그(자동 로드): `docs/superpowers/RESUME.md`
- 다음 세션 핸드오프: 루트 `HANDOFF.md` (남은 작업 HW-gated + filter-repo 재작성 컨텍스트)
- 핀 매핑: `docs/pinmap.md`
- 요구사항: `docs/requirements.md`
- ATmega16 분석: `docs/superpowers/analysis/` (regulation-core-verified, samd20-m16-ipc-semantics-verified, atmega16-io-behavior 등)
- 프로젝트 컨벤션: `CLAUDE.md` (root)
- ref 코드(수정 ✗): `ref/samd20/`, `ref/atmega16/`
- 과거 RESUME archive: `docs/superpowers/historical/`

---

> **본 문서 갱신 시점**: 2026-06-20 (weld 슬라이스3 + SEEK/RESET HW 검증·머지·origin 푸시 완료 + filter-repo 히스토리 재작성 반영; 코딩 스테이지 전부 완료, 남은 작업 HW-gated)
> **다음 갱신 시점**: HW-gated 스테이지(slice4/B-SEAM/6b/overload) 착수·완료 시, 또는 신규 스테이지 시작 시
