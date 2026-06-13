# Handoff: Stage C slice 1 — Modbus 코어+RTU (HW E2E PASS + main 머지 완료)

**Generated**: 2026-06-13 c (보드 세션 마감)
**Branch**: `main` (modbus 머지 `e351cad`, 태그 `hw-revA_fw-stage-c1`; feature 브랜치 삭제됨)
**Status**: ✅ **DONE — Modbus slice 1(코어+RTU) 실보드 HW E2E 통과, main 머지·태그 완료.** 머지 체인 완료: slice2b ✅ → m1 ✅ → modbus ✅.

> **요약**: `feat/stage-c-modbus-core-rtu`(코드 완결, 최종 cpp-reviewer APPROVED)를 ST-LINK V3로 플래시하고 Mac↔V30 RS-485 + `mbpoll`로 plan §HW-gated 6항목 매트릭스를 전수 검증했다. 5/6 완전 PASS + 1 부분검증(work_cnt non-zero→0 시연은 weld-cycle deferred로 벤치 불가, 호스트 테스트 커버). **펌웨어 코드 변경 0건**(순수 검증). `--no-ff` main 머지 + 태그 후 클린 빌드 0-warning·호스트 PASS. **다음 = Stage C slice 2 (Modbus TCP/W5500) brainstorming.**
>
> 1차 핸드오프 = `docs/superpowers/RESUME.md`(SessionStart 자동 로드, 최상단 "2026-06-13 c" 블록). 본 문서는 HW E2E 재현 절차 + 측정 함정 상세. 응답 **한국어**(코드/식별자 영어). 워크스페이스 규칙: **코드는 요청한 부분만 수정**. origin push ✗(local-authoritative main).

## Goal

samd20 Modbus 슬레이브(레지스터맵 + RTU/RS-485)를 STM32F410 펌웨어에 흡수. slice 1 = 공유 코어 + RTU(USART6 점유 전환). TCP(W5500) = slice 2. 실보드에서 mbpoll 마스터로 폴링/쓰기/명령/점유전환/속도매트릭스 검증.

## Completed

- [x] **HW E2E 6항목 매트릭스 전수 검증** (plan §HW-gated):
  - [x] ① FC03 폴링 0x00~0x1D 덤프 = 부팅 `[cfg]`/`set_pot` 값 전건 일치, STATUS=0 (미러 충실)
  - [x] ② FC06 OUT_POWER 80→80, 클램프 30→50(min)/120→100(max), openocd reset 후 80 유지(FRAM 커밋 입증)
  - [x] ③ START→STATUS bit0=1, 560ms on-time ceiling 자동정지, STOP, ×3 재현, ICON_RUN 육안(사용자)
  - [x] ④ 점유 전환: addr=NONE→release+mon복구+mbpoll타임아웃, addr=1→재획득, comm_mode=ETH→release(comm_mode-only 경로)
  - [x] ⑤ 속도/패리티: 9600/EVEN(8E1, speed=2 parity=0), 115200/NONE(8N1, speed=5 parity=2, reconfigure 경로)
  - [x] ⑥ work_cnt FC06 0 쓰기 수락 (non-zero→0은 벤치 불가 — 아래 Warnings)
- [x] **main 머지** `e351cad`(`--no-ff` 2-parent) + 태그 `hw-revA_fw-stage-c1`
- [x] 머지 후 클린 빌드 0-warning(text 40296B ≈30.7%)·호스트 ×2 PASS, feature 브랜치 삭제
- [x] docs/changelog·RESUME·memory 갱신

## Not Yet Done

- [ ] **Stage C slice 2 = Modbus TCP (W5500, MBAP over SPI1)** — brainstorming부터. 코어에 `mode!=RTU` 스킵 경로 이미 준비됨.
- [ ] HW-gated deferred (전압 가변/실 초음파 필요): SEEK/RESET 효과·overload 검출·weld-cycle 머신(work_cnt 증가 소속)·B-SEAM OSC 물리 구동·6b signal calibration

## Failed Approaches (Don't Repeat These)

- **"8-연속폴 전부 STATUS=1 → ceiling 미발화" 오진**: START 후 STATUS를 8회 빠르게 폴링했더니 전부 1이라 560ms ceiling이 안 걸린 줄 알았다. 실제로는 **mbpoll 1회 호출 오버헤드(~60ms)가 ceiling보다 빨라 8회 전부 560ms 창 안**에 들어간 측정 아티팩트였다. → **정확한 shell `sleep`로 재측정**(0.3s→STATUS=1, 1.3s→STATUS=0)하니 ceiling 정상 작동. 교훈: mbpoll 반복 폴은 타이밍 기준이 못 됨, `sleep`로 절대시간 게이트할 것. (macOS `date +%N`은 미지원 — 타임스탬프 무효.)
- **zsh `MB="mbpoll ..."` 변수**: `$MB -r 7 ...` → `command not found: mbpoll -a 1 ...`. zsh는 bash와 달리 unquoted 변수를 단어분리하지 않는다. → **mbpoll 명령을 매번 인라인**으로 풀어 쓸 것(또는 `${=MB}`).
- **grep `\[1\]` 패턴이 빈 결과**: mbpoll은 출력 인덱스를 **레퍼런스 번호**로 표기(`-r 7 -c 1` → `[7]: 80`, `[1]` 아님). 폴 시작 ref가 1이 아니면 `[1]`로 grep하면 안 됨.

## Key Decisions

| Decision | Rationale |
|----------|-----------|
| 머지 = `--no-ff` 로컬, origin push 생략 | 프로젝트 패턴(local-authoritative main); slice2b·m1과 동일 |
| 태그 = `hw-revA_fw-stage-c1` | H/W rev + F/W 스테이지 규칙(slice2b=d2b 등과 일관) |
| work_cnt 리셋 = 부분검증 수용 | non-zero→0은 weld-cycle 머신(deferred)만 work_cnt 증가 → 벤치 상시 0. 디코드/리셋 분기는 호스트 테스트가 커버 |
| FRAM 영속 = openocd `reset run`으로 검증 | RAM 미러가 FRAM에서 재로드 → 쓴 값 유지 = FRAM 커밋 입증(물리 전원사이클과 동치, 더 빠름) |

## Current State

**Working**: main HEAD `63cf87d`, 클린 빌드 0-warning, 호스트 테스트 PASS. modbus slice 1 = 코어+RTU+통합 전부 동작 검증됨.

**Broken**: 없음.

**Uncommitted Changes**: 없음(작업트리 클린). untracked = `.understand-anything/`, `ref/atmega16/M16_reverse/`(세션 시작부터 untracked, 분석 ground-truth — 커밋 여부 사용자 결정, 무관).

**보드 물리 상태**: SERIAL + addr=1 + 115200 + NONE (Modbus 점유 중), OUT_POWER 55로 복원. 테스트 중 comm 파라미터를 바꿨으니 평상시 설정 복귀는 패널에서.

## Files to Know

| File | Why It Matters |
|------|----------------|
| `fw/src/app_modbus_core.c` / `fw/include/app_modbus_core.h` | 순수 코어(HAL-free): H_REG 맵, CRC16, FC 01/02/03/04/05/06 디코드. 호스트 TDD |
| `fw/drivers/usart6_mb.c` / `.h` | RTU 전송층: DMA2 S1 Ch5 circular RX, samd20 갭 프레이밍, blocking TX, TX후 RX flush. `mb_baud[]`/parity_idx 매핑 |
| `fw/src/app_modbus.c` / `.h` | 통합층: 매 tick 미러, FC06 클램프 체인+`app_config_save_all` FRAM, US_COMM 명령, USART6 점유 전환 |
| `fw/src/app_reg.c` | `app_reg_command(cmd,src)` US_COMM 소스, on-time ceiling(`limit_on_time×10ms`, COMM 적용, line 223-249) |
| `fw/test/test_app_modbus_core.c` | 코어 호스트 테스트(CRC 표준벡터+fence-post+TCP 분기) |
| `docs/superpowers/plans/2026-06-12-stage-c-modbus-slice1-core-rtu.md` | plan + §HW-gated 매트릭스(이번에 실행한 절차) |
| `docs/superpowers/specs/2026-06-12-stage-c-modbus-slice1-core-rtu-design.md` | spec(점유 규칙·레지스터맵·편차) |

## Code Context

**레지스터맵** (`app_modbus_core.h`, samd20 modbus.h verbatim; mbpoll `-r` = wire addr +1):
```
0x00 WORK_CNTH  0x01 WORK_CNTL(=0 write→reset)  0x02 DISP_POWER  0x03 DISP_AMP
0x04 DISP_FREQ  0x05 DISP_ENERGY  0x06 OUT_POWER(clamp 50..100)  0x07 ON_TIME(=limit_on_time)
0x08 ENERGY  0x09 TIMEOVER  0x0A~0x0E DELAY/TRIGGER  0x0F~0x12 MULTI  0x13 RUN_MODE
0x14 EN_ENERGY  0x15 EN_MULTI  0x16 EN_SAFTY  0x17 MODEL_FREQ(RO)  0x18 MODEL_TYPE(RO)
0x19 RESET  0x1A SEEK  0x1B START  0x1C STOP  0x1D STATUS(bit0=US run)
```

**점유 규칙** (`app_modbus.c`, 매 tick 평가): `want = (cfg->comm_mode==SERIAL(0)) && (cfg->comm_address!=0)`. true→acquire, false→release. comm_mode/speed/parity 변화 시 reconfigure(release+acquire).

**mbpoll 매핑** (검증됨):
```
mb_baud[] = {2400,4800,9600,19200,38400,115200}  → idx 2=9600, 3=19200, 5=115200
parity_idx: 0=EVEN(8E1), 1=ODD, 2=NONE(8N1)
```

## Resume Instructions

**다음 작업 = Stage C slice 2 (Modbus TCP/W5500) brainstorming** (HW 불필요로 시작 가능):
1. `cd /Users/tknoh/dev/work/gds_us_ctrl && git checkout main` (이미 main)
2. 빌드/테스트 sanity: `env -u STM32_TOOLCHAIN cmake -S fw -B fw/build -G Ninja && env -u STM32_TOOLCHAIN cmake --build fw/build` (0-warning) + `make -C fw/test test` (둘 다 PASSED)
3. `superpowers:brainstorming`으로 slice 2 시작 — W5500 SPI1, MBAP 헤더, 코어의 `mode!=RTU` 스킵 경로 활용. spec→plan→구현 흐름.

**(참고) Modbus RTU E2E 재현** (보드+RS-485 어댑터 연결 시):
1. 플래시: `openocd -f fw/openocd/stm32f410.cfg -c "program fw/build/gds_us_ctrl.elf verify reset exit"`
2. 패널 comm 저장: SERIAL + addr=1 + 19200 + NONE
3. mon 캡처(점유 전 확인용): `DEV=/dev/cu.usbserial-AB0MLYXA; { stty -f "$DEV" 115200 cs8 -parenb -cstopb raw -echo; cat; } < "$DEV" > /tmp/mb-mon.log &` → `[mb] acquire` 후 mon 침묵
4. **cat 캡처를 죽인 뒤**(같은 포트 공유) mbpoll: `pkill -x cat; mbpoll -a 1 -b 19200 -P none -t 4 -r 1 -c 30 -1 $DEV`
   - Expected: `[1]`~`[30]` 레지스터 덤프, STATUS([30])=0
   - 쓰기: `mbpoll -a 1 -b 19200 -P none -t 4 -r 7 $DEV 80` (인라인, `$MB` 변수 금지)
   - 런: `-r 28 $DEV 1`(START) → `sleep 0.3` 후 `-r 30 -c 1 -1`(STATUS=1) → `sleep 1.0`(STATUS=0, ceiling)

## Warnings

- **mon ↔ Modbus는 같은 USART6 물리선 공유**(AB0MLYXA). Modbus 점유 중엔 mon 침묵이 정상. 트레이스(REG_TRACE/LCD_TRACE_RX) 검증은 **addr=NONE 미점유**에서만. cat 캡처와 mbpoll은 **동시에 같은 포트 못 씀** — mbpoll 전 `pkill -x cat`.
- **work_cnt non-zero→0 리셋은 벤치 시연 불가**: work_cnt는 deferred weld-cycle 머신(energy/multi 런 브랜치, app_reg)만 증가 → 벤치 상시 0. 실 초음파 weld 사이클 구현 후에야 의미 있는 리셋 검증 가능.
- **DISP_POWER/AMP=0(런 중)은 by-design**: 벤치에 전압주입/실 트랜스듀서 없음(B-SEAM deferred) → 측정 피드백 idle floor. 결함 아님.
- **B-SEAM 랜딩 시**: `app_modbus.c` apply_writes의 pot-guard NOTE — 스테일 `g_measure`(2ms 게이트 발행 → same-iter 가드 항상 false). set_pot이 로그 스텁인 동안은 무해하나, 라이브 accessor로 교체 시 수정 필요.
- **이전 HANDOFF(slice 2b)는 폐기**: 이 문서가 최신. slice 2b·m1·modbus 전부 머지 완료.
