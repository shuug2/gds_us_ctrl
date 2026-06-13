# Handoff: gds_us_ctrl — Stage C/D 완료, slice-2 deferred HW 종결 + 문서정비 (2026-06-13 j)

**Generated**: 2026-06-13 j (보드 세션 마감)
**Branch**: `main` (tip `7beb5b4`)
**Status**: ✅ **세션 클린 마감 — 진행 중 코드 작업 없음.** Stage C(Modbus RTU+TCP) + Stage D(ATmega16 흡수) 전부 main 머지·태그 완료. 이번 세션은 slice-2 deferred HW 3항목 종결(코드 무수정) + 문서정비.

> 응답 **한국어**(코드/식별자 영어). 워크스페이스 규칙: **코드는 요청한 부분만 수정**. 머지 = `--no-ff` 로컬, origin push ✗(local-authoritative main). first-load = `docs/NEXT_STEPS.md`, 세션 로그(자동) = `docs/superpowers/RESUME.md`, 이력 = `docs/changelog.md`.

## Goal

STM32F410RBT 단일 MCU로 SAMD20 + ATmega16 기능 통합. 핵심 기능(LCD·레귤레이션·Modbus)은 흡수 완료. 남은 것은 실 초음파/가변전압이 필요한 출력·효과 계층.

## Completed (이번 세션 = j)

- [x] **slice-2 deferred HW ① ICON_RUN over TCP = PASS**: `mbpoll -m tcp` START(reg 28)=1 → US_COMM run(STATUS reg 30 bit0=1) → on-time ceiling 자동정지 **실측 537–617ms = 560ms**(limit_on_time=56×10ms) → IDLE. START 6연발로 ICON_RUN 6/6 육안 확인(user).
- [x] **slice-2 deferred HW ② RTU FC06 회귀 = PASS**: 보드 SERIAL/addr=1/9600/EVEN 전환(openocd `g_cfg` 직독 확정), RS-485 `mbpoll -m rtu`. FC03 regs 7-9 = `55/56/567`(=TCP 미러 동일), FC06 클램프 80→80/30→50/120→100/55. **slice 2(TCP/W5500)가 RTU 경로 무회귀** 입증.
- [x] **slice-2 deferred HW ③ RAM-only 재리스 = 동작 규명·수용**: DHCP 리스가 LCD `comm_mode=ETH_STATIC` 저장 시 static IP로 굳는 동작을 코드로 확인(사용자 가설 맞음). **무수정**, 메모리 `project_eth_dhcp_static_persist`.
- [x] **문서정비**: `docs/changelog.md`(j 엔트리) + `docs/NEXT_STEPS.md`(Stage C/D 완료 반영 재작성, stale 해소) + `docs/superpowers/RESUME.md`(j 블록) + `HANDOFF.md`(본 문서) + `CLAUDE.md`(해소된 핵심 질문 갱신 + ETH 저장 주의 추가).

## Not Yet Done (= 다음 작업 후보)

- [ ] **신규 스테이지 (HW 불필요, 설계/코드 가능)**: weld-cycle 머신(work_cnt 증가, energy/multi run 분기 samd20 main.c:5234+) / SEEK·RESET 효과(RESET→SEEK 500ms 자동 체인) / overload 보호(CON_OVLD). `superpowers:brainstorming`부터.
- [ ] **HW-gated (실 초음파 rig/스코프 필요)**: B-SEAM OSC 물리 구동(레귤레이션 compute의 마지막 블로커) / 6b signal calibration(`>>2` 정규화·2.56V↔3.3V 도메인·ADC offset/gain).
- [ ] **설계상 이연(slice 2)**: DHCP 핫플러그(링크 드롭 후 재획득, 현재 LINKWAIT→UP 단방향) / SERIAL boot-skip.

## Failed Approaches (Don't Repeat These) — 이번 세션 진단 함정

- **static IP가 `.128` 복귀 기대 → 틀림(`.70`으로 굳음)**: "RAM-only 재리스" deferred의 원래 기대는 무효. DHCP 리스가 static과 동일한 `cfg->ether_ip`에 미러되어 LCD 저장 시 FRAM에 누출(아래 Code Context + 메모리). **이 동작을 "버그"로 고치려 들지 말 것** — 사용자 수용, 의도된 동작.
- **`ping .70`로 SERIAL 여부 판단 → 못 함**: 브링업이 무조건(SERIAL 포함)이라 W5500이 SERIAL 모드에서도 static IP `.70`을 갖고 ICMP에 응답(TCP 서버만 게이트오프). SERIAL 여부는 **openocd로 `g_cfg` 직독** 또는 RTU 응답으로 판단.
- **첫 RTU read 1회 transient 타임아웃**: 포트 워밍업으로 첫 mbpoll이 무응답 → 재시도하면 정상. config가 맞으면 1회 타임아웃에 속지 말고 재시도.
- **시리얼 캡처 함정(이전 세션부터 누적)**: `cat /dev/...`(인자형식)은 9600 리셋 → garbage. **리다이렉트 형식 필수**. `tr`는 `LC_ALL=C`. 부팅 mon은 **물리 전원사이클**(openocd reset은 안 나옴). `pkill -x cat`(`-f`는 과매칭).

## Key Decisions

| Decision | Rationale |
|----------|-----------|
| slice-2 deferred 3항목 코드 무수정 종결 | ①② PASS(회귀 없음), ③은 의도된 동작으로 사용자 수용 |
| RAM-only 재리스 동작 그대로 유지 | "DHCP 받은 IP를 static 고정" 워크플로로 합리적(WYSIWYG); eth 모듈 RAM-only는 문자대로 사실, 누출은 LCD 저장 경로 |
| NEXT_STEPS.md 재작성(부분 패치 대신) | 2026-05-25 Stage A/B 시점에 멈춰 Stage C/D 완료 전부 미반영 → 현 상태 기준 재구성이 더 정확 |

## Current State

**Working**: `main` tip `7beb5b4`, 빌드 0-warning(text 52728B/FLASH 40.65%/RAM 16.66%), 호스트 3스위트(`app_reg_calc`+`app_modbus_core`+`app_modbus_tcp_frame`) PASS. 보드 RTU 응답 정상.

**Broken**: 없음.

**Uncommitted Changes**: 5개 파일 modified, **전부 docs/문서 — 코드 변경 0**: `CLAUDE.md`, `HANDOFF.md`, `docs/NEXT_STEPS.md`, `docs/changelog.md`, `docs/superpowers/RESUME.md`. (untracked `.understand-anything/`, `ref/atmega16/M16_reverse/` = 세션 무관 분석 산출물.) → **커밋 권장**(제안 메시지 = ↓ Resume).

**보드 물리 상태**: **SERIAL / addr=1 / 9600 / EVEN(벤치 기본)**, OUT_POWER=55, FRAM `ether_ip=.70` 잔여(무해), tip 빌드 플래시됨.

## Files to Know

| File | Why It Matters |
|------|----------------|
| `docs/NEXT_STEPS.md` | **first-load 가이드(재작성됨)** — 스테이지 현황 표 + 다음 후보 + cheat sheet(RTU/TCP/openocd) |
| `fw/src/app_eth.c` | W5500 비블로킹 브링업 FSM. **DHCP 리스를 `cfg->ether_ip`에 미러(L64-70)** = RAM-only 누출의 출발점 |
| `fw/src/app_config.c` | `app_config_save_all`(L42, L73)이 `cfg->ether_ip`를 FRAM에 기록 = 누출의 종착점 |
| `fw/src/app_modbus.c` | RTU/ETH 분기 + FC06 클램프(50..100, L151-152) + US_COMM 명령. app_eth 무수정 재사용 |
| `fw/src/app_reg.c` | run FSM + on-time ceiling(L234 US_COMM 적용, REG_TRACE 가드) |
| `fw/include/app_modbus_core.h` | 레지스터맵(START 0x1B, STOP 0x1C, STATUS 0x1D, OUT_POWER 0x06 …) |

## Code Context

**RAM-only 누출 체인** (slice-2 ③ 규명 — 메모리 `project_eth_dhcp_static_persist` 전문):
```
dhcp_ip_assign() : cfg->ether_ip[] ← 리스 .70   (app_eth.c:64-70, eth는 FRAM 미기록)
  → comm/ether 페이지 진입: temp_ether_ip = cfg->ether_ip = .70 + 렌더 (app_lcd_render.c:147-153)  ← LCD에 .70 표시
  → DATA_SAVE → app_config_save_all(cfg) : FRAM ← cfg->ether_ip(.70)  (app_config.c:611,73)
  → 재부팅: cfg->ether_ip ← FRAM(.70)  (app_config.c:118)
  → static 경로: ni.ip = cfg->ether_ip = .70 → "[eth] up ip=192.168.1.70"  (app_eth.c:116-130)
```

**Modbus 레지스터** (wire addr → mbpoll `-r` = wire+1): OUT_POWER 0x06→7 / RESET 0x19→26 / SEEK 0x1A→27 / START 0x1B→28 / STOP 0x1C→29 / STATUS 0x1D→30(bit0=US_run). FC06 OUT_POWER 클램프 50..100. comm_mode 0=SERIAL/1=ETH_STATIC/2=ETH_DHCP, parity 0=EVEN/1=ODD/2=NONE, mb_baud[]={2400,4800,9600,19200,38400,115200}.

## Resume Instructions

1. **사전 점검**:
   ```bash
   cd /Users/tknoh/dev/work/gds_us_ctrl && git status   # 5 doc 파일 modified(미커밋), 코드 0
   env -u STM32_TOOLCHAIN cmake -S fw -B fw/build -G Ninja && env -u STM32_TOOLCHAIN cmake --build fw/build   # 0-warning
   make -C fw/test test   # 3 PASS
   ```
2. **doc 커밋(권장)** — 이 세션 산출물:
   ```bash
   git add CLAUDE.md HANDOFF.md docs/NEXT_STEPS.md docs/changelog.md docs/superpowers/RESUME.md
   git commit -m "docs: 문서정비 + slice-2 deferred HW 종결 (j) — NEXT_STEPS 재작성, CLAUDE.md 갱신, RTU/ICON_RUN PASS·DHCP→static persist 규명"
   ```
3. **다음 작업 선택** (사용자 콜):
   - 신규 스테이지 → `superpowers:brainstorming`(weld-cycle / SEEK·RESET / overload). 진입 절차 = `docs/NEXT_STEPS.md` §3.
   - B-SEAM OSC + 6b cal → 실 초음파 rig 필요.

## ETH E2E 재현 (보드 + W5500 + 망, 필요 시)

1. 플래시: `openocd -f fw/openocd/stm32f410.cfg -c "program fw/build/gds_us_ctrl.elf verify reset exit"`
2. mon 캡처(**리다이렉트 형식 + 115200**): `{ stty 115200 cs8 -parenb -cstopb raw -echo; exec cat; } < /dev/cu.usbserial-AB0MLYXA > /tmp/mon.log &`
3. 파싱: `LC_ALL=C tr -d '\000' < /tmp/mon.log | LC_ALL=C tr -s ' ' | grep -aE '\[boot|\[eth|\[mb|\[cfg'`
4. LCD `comm_mode=ETH_STATIC`(또는 DHCP) SAVE → **물리 전원사이클** → `[eth] up ip=…`(static) / `[eth] dhcp lease ip=…`(DHCP)
5. `mbpoll -m tcp -a 1 -t 4 -r 1 -c 12 -1 <ip>`(FC03) / `-r 28 <ip> 1`(START) / `-r 7 <ip> 80`(FC06)
6. RTU 회귀 시: LCD SERIAL/addr=1 → `pkill -x cat` → `mbpoll -m rtu -a 1 -b 9600 -P even -t 4 -r 7 <DEV> [val]`. config 의심 시 openocd `g_cfg`@(nm 확인)+0x2A 8바이트 직독.

## Warnings

- **시리얼 캡처 함정 = Failed Approaches 절 필독**. **ping으로 SERIAL/ETH 모드 판단 불가**(브링업 무조건이라 SERIAL서도 .70 ICMP 응답).
- **ETH 설정 저장 = DHCP→static persist 주의**(CLAUDE.md 작업 주의사항 + 메모리). static 특정 IP 필요 시 LCD IP 필드 직접 입력.
- **DHCP/HAL/PHY 글루는 호스트 테스트 없음** → 통합 cpp-reviewer + advisor + HW E2E가 유일 게이트.
- **B-SEAM 랜딩 시**: `app_modbus.c` apply_writes의 pot-guard NOTE(스테일 `g_measure`).
- **이전 slice-2 핸드오프는 본 문서로 대체**: Stage C/D 전부 완료, 진행 중 코드 작업 없음.
