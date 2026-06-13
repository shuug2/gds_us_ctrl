# Handoff: Stage C slice 2 — Modbus TCP/W5500 (static + DHCP) — HW E2E PASS + main 머지 완료

**Generated**: 2026-06-13 i (보드 세션 마감)
**Branch**: `main` (머지 `fa93dcd`, `--no-ff` 2-parent; 태그 `hw-revA_fw-stage-c2b`; feature 브랜치 2개 삭제됨)
**Status**: ✅ **DONE — Modbus TCP(static+DHCP) over W5500 실보드 HW E2E 통과, main 머지·태그 완료.** Stage C(Modbus 흡수) = slice 1(RTU) + slice 2(TCP static+DHCP) 전부 main에 있음.

> **요약**: slice 2a(static IP)+2b(DHCP) 스택 브랜치를 구현→호스트게이트→실보드 HW E2E까지 완주. HW에서 실 버그 2건 발견·수정: ① DHCP 분기가 `DHCP_init` 전 칩 MAC(SHAR) 미설정(advisor catch, `b7ce6b0`) ② `app_eth_init`의 1s PHY-link 부팅 폴이 W5500 오토네고(~1.5s)보다 짧아 링크를 놓침 → **비블로킹 FSM 리팩터**(`635e9ec`). HW 전수 PASS(static `up`+ping+FC03+FC06클램프 / DHCP 리스+ping+FC03+LCD리스IP / **재시도 복구**=부팅 후 케이블 꽂으면 무재부팅 복구). 머지는 **Option B(2a/2b 합쳐 2b 통째)** — 리팩터로 2b app_eth가 2a와 byte-identical이 아니고 2a 원본은 confirmed 버그 보유 → 2b가 2a 전부 포함·검증하므로 통째 머지, **pre-refactor 2a는 태그 안 함**.
>
> 1차 핸드오프 = `docs/superpowers/RESUME.md`(SessionStart 자동 로드). 본 문서 = HW E2E 절차 + **시리얼 캡처 함정**(이번 세션에서 크게 데인 부분) + 측정 교훈. 응답 **한국어**(코드/식별자 영어). 워크스페이스 규칙: **코드는 요청한 부분만 수정**. origin push ✗(local-authoritative main).

## Goal

samd20 Modbus 슬레이브를 STM32F410에 흡수. slice 1 = 공유 코어 + RTU(USART6). **slice 2 = TCP(W5500 over SPI1): 2a static IP, 2b DHCP 획득.** 표준 Modbus TCP(MBAP 에코 + CRC 절단), 코어 무수정 재사용. 실보드 mbpoll TCP 마스터로 검증.

## Completed (이번 세션 = slice 2 전체)

- [x] **구현 (subagent-driven)**: 벤더 WIZnet ioLibrary(Ethernet + DHCP, 핀 `220ca7a6`) + `spi1` + 순수 MBAP 프레이밍 + `app_eth`(W5500 브링업/DHCP) + `app_modbus_tcp`(소켓 FSM) + 통합(`app_modbus_tick` RTU/ETH 분기)
- [x] **HW E2E 전수 PASS** (ST-LINK V3, W5500 실장 + DHCP 서버 망):
  - [x] **비블로킹 부팅** `[eth] chip up — waiting for PHY link...` 후 슈퍼루프 진행
  - [x] **재시도 복구(핵심)**: 케이블 빼고 부팅→`LINKWAIT` 유지(`up` 없음, 보드 정상)→케이블 꽂기→**무재부팅** `[eth] up`
  - [x] **static**: `[eth] up 192.168.1.128`, ping 0%, FC03 미러(`[7]=55/[8]=56/[9]=567`), FC06 클램프(80→80/30→**50**/120→**100**/55복원)
  - [x] **DHCP**: `[eth] dhcp lease ip=192.168.1.70`, ping 0%, FC03 미러, **LCD 리스IP 표시(RAM-only)** = MAC-before-DHCP 수정으로 올바른 MAC 바인딩 입증
- [x] **버그 2건 발견·수정**: MAC-before-DHCP(`b7ce6b0`, advisor) + 비블로킹 PHY 리팩터(`635e9ec`, 리뷰fix `62c1cc1`)
- [x] **머지** `fa93dcd`(Option B, `--no-ff`) + 태그 `hw-revA_fw-stage-c2b`, 머지 후 클린 빌드 0-warning(text 52728B ≈40.65%)·호스트 3스위트 PASS, feature 브랜치 삭제
- [x] tip(`62c1cc1`) 재플래시 + HW 재확인(기록 정확성)

## Not Yet Done

- [ ] **slice 2 deferred(사용자 "핵심으로 충분"으로 보류, 보드 있을 때)**: ICON_RUN 육안(START→560ms ceiling→STOP over TCP) / RTU FC06 회귀 spot-check(SERIAL 복귀 + RS-485 마스터) / RAM-only 재리스 증명(static 복귀→.128 표시, .70 아님)
- [ ] **slice 2 deferred 범위(설계상 이연)**: DHCP 핫플러그(링크 드롭 후 재획득; 현재 LINKWAIT→UP 단방향) / SERIAL boot-skip
- [ ] **HW-gated deferred(전압 가변/실 초음파 필요)**: SEEK/RESET 효과·overload·weld-cycle(work_cnt 증가)·B-SEAM OSC 물리 구동·6b signal calibration

## Failed Approaches (Don't Repeat These) — 이번 세션 시리얼 캡처에서 크게 데임

- **`cat /dev/...`(인자 형식)은 포트를 기본 termios(9600)로 리셋** → 115200 출력이 garbage(`95 9b 55 1b...`)로 보임. 앞서 `stty -f device 115200`을 해도 cat이 자기 fd를 새로 열며 무시함. → **리다이렉트 형식 필수**: `{ stty 115200 cs8 -parenb -cstopb raw -echo; exec cat; } < /dev/cu.usbserial-AB0MLYXA > /tmp/eth-mon.log &` (stty가 cat이 쓸 바로 그 fd=stdin을 설정).
- **openocd `reset`/`reset run`은 깨끗한 부팅 mon을 안 만듦**(글리치 8~9바이트만). 신뢰 가능한 재부팅 = **물리 전원사이클** 또는 **`program ... verify reset exit`(플래시)**. 코드 무변경 재부팅이 필요하면 물리 전원사이클로.
- **`tr`가 "Illegal byte sequence"**: 시리얼 리셋 글리치의 비-UTF8 바이트를 UTF-8 로케일 tr이 거부. → **`LC_ALL=C tr -d '\000'`** 로 바이트 단위 처리.
- **빈 캡처(0바이트)**: ETH 모드 mon은 부팅 직후 **1회성 버스트**(이후 이벤트성). 캡처를 부팅 전에 띄워두고, 부팅 버스트를 놓치지 말 것. cat 죽이고 재시작하면 그 사이 부팅을 놓침.
- **`pkill -f 'cat'`은 너무 광범위**(macOS "loCATion"d 등 매칭). → **`pkill -x cat`**(정확 매칭) 사용.
- **1s 부팅 PHY 폴은 W5500엔 너무 짧음**: NRST 후 PHY 오토네고 ~1.5s(진단 빌드로 t=1490ms 실측). 부팅 블로킹 폴 대신 슈퍼루프 tick 재시도로 옮긴 이유.

## Key Decisions

| Decision | Rationale |
|----------|-----------|
| 비블로킹 FSM 리팩터 (vs 타임아웃 연장) | 진단 빌드로 링크 ~1.49s 실측 → 단순 3s 타임아웃은 SERIAL 부팅도 블로킹+1회성. tick 재시도가 비블로킹+부팅후 케이블연결 복구(HW로 입증). advisor 설계검토. |
| MAC-before-DHCP_init | 벤더 `DHCP_init`이 SHAR 0이면 temp `00:08:dc:00:00:00` 대체(dhcp.c:1030) → 리스가 엉뚱한 MAC 바인딩. `DHCP_init` 직전 `wizchip_setnetinfo(MAC)`로 SHAR 선설정(samd20 충실). 리뷰 3건 다 놓침(spec 미포함+호스트 미커버) → advisor가 잡음. |
| 머지 = Option B (2a/2b 합쳐 2b 통째) | 리팩터로 2b app_eth가 2a와 byte-identical 아님; 2a 원본은 confirmed PHY 버그+HW 미검증. 2b가 2a 전부 포함·검증(2a static ⊂ 2b 검증). 2a 재검증=busywork. |
| **pre-refactor 2a에 c2a 태그 안 함** | known-broken 코드에 HW-PASS 태그를 붙이면 안 됨. 단일 태그 `c2b`가 static+DHCP 전부 커버. |
| 브링업 무조건(SERIAL 포함) | advisor: SERIAL→런타임 ETH_STATIC 무재부팅 동작 보존(slice 2a 동작). mon은 SERIAL서 게이트오프라 무해. |
| 머지 = `--no-ff` 로컬, origin push 생략 | 프로젝트 패턴(local-authoritative main) |

## Current State

**Working**: main HEAD `fa93dcd`(머지), 태그 `hw-revA_fw-stage-c2b`. 클린 빌드 0-warning(text 52728B, FLASH 40.65%, RAM 16.66%), 호스트 3스위트(`app_reg_calc`+`app_modbus_core`+`app_modbus_tcp_frame`) PASS.

**Broken**: 없음.

**Uncommitted**: 없음(작업트리 클린). untracked = `.understand-anything/`, `ref/atmega16/M16_reverse/`(세션 시작부터, 분석 ground-truth — 무관).

**보드 물리 상태**: **ETH_DHCP 모드(리스 .70 보유)**, tip `62c1cc1` 빌드 플래시됨. 벤치 기본(SERIAL/addr=1)으로 돌리려면 LCD에서 comm_mode=SERIAL 저장+전원사이클. (RTU 회귀 deferred 검증 시 이 상태 필요.)

## Files to Know (slice 2에서 신규/핵심)

| File | Why It Matters |
|------|----------------|
| `fw/src/app_eth.c` / `fw/include/app_eth.h` | **비블로킹 W5500 브링업 FSM**(`ETH_DOWN`/`LINKWAIT`/`STATIC_UP`/`DHCP_RUN`). init=칩 init만, `app_eth_tick`이 100ms 링크 폴→`eth_apply_on_link`(static netinfo / MAC선설정+DHCP). DHCP 콜백(assign=RAM-only cfg 미러, conflict=non-fatal). MAC-before-DHCP_init. |
| `fw/src/app_modbus_tcp.c` / `.h` | TCP 소켓 FSM(port 502, sock0), recv→frame→공유 apply→send |
| `fw/src/app_modbus_tcp_frame.c` / `.h` | 순수 MBAP 프레이밍(strip+검증, CRC 2B 절단, unit 에코, 빅엔디안 length). 호스트 TDD `fw/test/test_app_modbus_tcp_frame.c` |
| `fw/src/spi1.c` / `.h` | SPI1 마스터 Mode0 12MHz, PA5/6/7(AF5)/PA4(소프트CS)/PC5(NRST)/PC4(INT 폴링). byte 콜백 CS 미토글(ioLibrary가 프레임 전체 CS 감쌈) |
| `fw/vendor/wiznet/` | WIZnet ioLibrary 벤더(핀 `220ca7a6`, `_WIZCHIP_=W5500`, 경고격리 `-w` lib). Ethernet/{wizchip_conf,socket,W5500/w5500} + Internet/DHCP/dhcp |
| `fw/src/app_modbus.c` | `app_modbus_tick` RTU/ETH 분기. ETH = `comm_mode!=SERIAL && app_eth_available()` 게이트 + mirror. **app_eth 무수정 재사용**(게이트가 available()만 봄) |
| `docs/superpowers/{specs,plans}/2026-06-13-stage-c-modbus-slice2{a,b}-*.md` | slice 2a/2b spec+plan |

## Code Context

**W5500 핀**(pinmap §SPI1): PA4 소프트CS / PA5 SCK / PA6 MISO / PA7 MOSI(AF5) / PC5 NRST / PC4 INT(폴링). MAC = `00:08:dc:78:91:71`(하드코딩, samd20 충실).

**app_eth 브링업 흐름**: boot `app_eth_init`(spi1+콜백등록+NRST+`wizchip_init`, 빠름, 즉시 리턴, `s_phase=LINKWAIT`) → 슈퍼루프 `app_eth_tick`(`LINKWAIT`: 100ms `CW_GET_PHYLINK` 폴 → 업 시 `eth_apply_on_link`: `comm_mode==ETH_DHCP`면 MAC선설정+`DHCP_init`+`DHCP_RUN`, else static netinfo+`STATIC_UP`+available). `DHCP_RUN`: 1s `DHCP_time_handler`+`DHCP_run`, `DHCP_FAILED`→re-init keep-retry. **링크 드롭 재획득=핫플러그 deferred(단방향)**.

**comm_mode**: 0=SERIAL 1=ETH_STATIC 2=ETH_DHCP (모듈별 로컬 `#define`, 공유 enum 없음 — 코드베이스 관행). 게이트: RTU=`SERIAL&&addr!=0`, TCP=`comm_mode!=SERIAL && app_eth_available()`.

**mbpoll TCP**: `mbpoll -m tcp -a 1 -t 4 -r <ref> -c <n> -1 <ip>`(읽기), `mbpoll -m tcp -a 1 -t 4 -r 7 <ip> <val>`(쓰기). `-r`=레퍼런스(wire addr+1; OUT_POWER 0x06→`-r 7`). 레지스터맵·클램프(50..100)는 slice 1과 동일(HANDOFF 이력/spec).

## Resume Instructions

**시작**:
1. `cd /Users/tknoh/dev/work/gds_us_ctrl && git checkout main`(이미 main, tip `fa93dcd`)
2. sanity: `env -u STM32_TOOLCHAIN cmake -S fw -B fw/build -G Ninja && env -u STM32_TOOLCHAIN cmake --build fw/build`(0-warning) + `make -C fw/test test`(3 PASSED)

**다음 작업 후보**:
- **slice 2 deferred HW**(보드+망): ICON_RUN 육안 / RTU FC06 회귀(SERIAL+RS-485) / RAM-only 재리스. ETH E2E 재현 절차 ↓.
- **HW-gated deferred**: SEEK/RESET·overload·weld-cycle·B-SEAM·6b cal(전압 가변/실 초음파 필요).
- **신규 스테이지**(HW 불필요): `superpowers:brainstorming`.

**ETH E2E 재현**(보드+W5500+DHCP 망):
1. 플래시: `openocd -f fw/openocd/stm32f410.cfg -c "program fw/build/gds_us_ctrl.elf verify reset exit"`
2. mon 캡처(**리다이렉트 형식 + 115200**): `{ stty 115200 cs8 -parenb -cstopb raw -echo; exec cat; } < /dev/cu.usbserial-AB0MLYXA > /tmp/eth-mon.log &`
3. 읽기: `LC_ALL=C tr -d '\000' < /tmp/eth-mon.log | LC_ALL=C tr -s ' ' | grep -aE '\[boot|\[eth|\[mb|\[cfg'`
4. LCD에서 comm_mode=ETH_STATIC(또는 DHCP)+SAVE → **물리 전원사이클**(openocd reset은 mon 안 나옴) → `[eth] up ip=…`(static) / `[eth] dhcp lease ip=…`(DHCP)
5. `ping <ip>` → `mbpoll -m tcp -a 1 -t 4 -r 1 -c 30 -1 <ip>`(FC03) / `-r 7 <ip> 80`(FC06 쓰기, 인라인)
6. mbpoll 전 `pkill -x cat` 불필요(TCP는 별도 소켓; cat은 USART6 mon용). 단 RTU 회귀 땐 USART6 공유라 `pkill -x cat` 후 RS-485 mbpoll.

## Warnings

- **시리얼 캡처 함정 = Failed Approaches 절 필독**(cat 리다이렉트 형식/`LC_ALL=C`/물리 전원사이클/`pkill -x cat`). 이번 세션에서 0바이트·garbage로 여러 번 헛돔.
- **mon ↔ Modbus RTU는 USART6 공유**(AB0MLYXA). ETH 모드에선 Modbus가 USART6 미점유 → mon 정상. SERIAL+addr!=0이면 Modbus 점유 → mon 침묵(`[eth]`도 게이트오프). **요청 동작 확인됨**: ETH 모드는 addr 값 무관 mon 동작(점유 규칙 `SERIAL&&addr!=0`만 게이트).
- **DHCP/HAL/PHY 글루는 호스트 테스트 없음** → 통합 cpp-reviewer + advisor + **HW E2E**가 유일한 게이트. 이번에 HW가 PHY 타임아웃 버그를 드러냈고, 리뷰가 놓친 MAC 버그를 advisor가 잡음. 부팅 PHY 대기는 블로킹 init이 아니라 슈퍼루프 tick에 두는 게 맞음(교훈).
- **머지 시 pre-refactor 2a 코드는 broken**(1s 폴) — `c2b`만 유효, `c2a` 태그 없음.
- **B-SEAM 랜딩 시**: `app_modbus.c` apply_writes의 pot-guard NOTE(스테일 `g_measure`).
- **이전 HANDOFF(slice 1)는 폐기**: 이 문서가 최신. Stage C = slice 1(RTU) + slice 2(TCP static+DHCP) 전부 머지 완료.
