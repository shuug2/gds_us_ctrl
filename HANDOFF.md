# Handoff: 다음 세션 — 남은 작업(전부 HW-gated) 진입 가이드

**Generated**: 2026-06-20
**Branch**: `main` (tip `1fa5938`) — origin과 완전 동기화 (`git status` = `## main...origin/main`).
**Status**: 🟢 깨끗한 체크포인트 — 진행 중인 작업 없음. 코딩된 스테이지는 **전부 main + origin에 푸시 완료**. 남은 작업은 **전부 HW-gated**(실 초음파 rig/스코프 또는 물리 입력 필요).

> 이전 HANDOFF(보드 세션 weld3+seek-reset 검증)는 완료로 supersede. 이력 = `docs/changelog.md`(2026-06-19) / `docs/superpowers/RESUME.md`(2026-06-19 블록, 자동 로드).

## ⚠ 가장 먼저 알아야 할 것 — git 히스토리 재작성됨 (2026-06-20)

`git filter-repo`로 **전체 282커밋의 author 이메일을 `nogari@gmail.com`으로 재작성**했다(GitHub `shuug2` 계정 연결용). 결과:

- **모든 커밋 해시가 바뀜.** 옛 문서(RESUME/changelog/메모리/이 HANDOFF의 이전 버전)에 적힌 해시(`f6a7eee`, `4c536bf`, `a209ac1`, `2e5297f` 등)는 **더 이상 존재하지 않음**. `git show <옛해시>` 하면 "Not a valid object name". 내용·커밋 메시지·트리는 동일, SHA만 새로 계산됨.
- **안정 레퍼런스 = 태그.** 해시 대신 태그로 참조할 것:

| 태그 | 새 커밋 | 내용 |
|---|---|---|
| `hw-revA_fw-stage-seekreset` | `34367b1` | seek-reset 머지 (최신 기능) |
| `hw-revA_fw-stage-weld3` | `d2669a1` | weld multi 스테핑 머지 |
| `hw-revA_fw-stage-weld2` / `-weld1` | (서버 동기) | weld energy / DELAY |
| `hw-revA_fw-stage-c2b` / `-c1` | | Modbus TCP / RTU |
| `hw-revA_fw-stage-d2b` / `-d2` / `-d` / `-lcd` / `-b` / `-a` | | 이전 스테이지 |

- 옛 문서에서 해시가 안 맞아도 **당황 말 것** — `git log --oneline`로 현재 해시 확인, 또는 태그/커밋 메시지로 매칭.
- origin은 filter-repo가 한 번 제거 → SSH로 재추가됨: `origin = git@github.com:shuug2/gds_us_ctrl.git`. push는 SSH 인증(키 등록됨, `Hi shuug2!` 확인).

## Goal

기존 SAMD20 + ATmega16 기능을 STM32F410RBT 단일 MCU로 통합. **코드/host-test로 가능한 계층은 전부 완료**(LCD·레귤레이션·Modbus RTU+TCP·weld 1~3·seek-reset). 남은 것은 **실 출력/물리 효과 계층** — 실 초음파 rig·가변전압·스코프·물리 입력이 있어야 검증 가능.

## Completed (전부 main + origin)

- [x] Phase 1+2 · Stage A · B · LCD full port · Stage D(ATmega16 흡수: 레귤레이션·상태머신·RUN 게이트·m1)
- [x] Stage C: Modbus RTU + TCP(static/DHCP)
- [x] weld-cycle 슬라이스1(DELAY) · 2(energy_ctrl exit) · 3(multi_ctrl 2단 진폭)
- [x] SEEK/RESET 효과 (순수 FSM + 글루, ICON, 양방향 RUN 직교)
- [x] git 히스토리 author 재작성 + origin(SSH) 푸시 (main `1fa5938` + 태그 12개)

## Not Yet Done — 남은 작업 (전부 HW-gated, spec 미작성 → brainstorming부터)

우선순위/의존성 순. **slice4 외에는 아직 spec 없음** → 각각 `superpowers:brainstorming → spec → writing-plans → subagent-driven` 거칠 것.

1. **weld 슬라이스4** (사이클 자체 E2E의 핵심): TRIGGER 모드 + **물리 SW_START1/2** + 위치센서 + 실 SOL_DN(PB5) GPIO + 안전 abort + **config-validation 클램프**. ⚠ **must-fix(slice1~3 누적 LOW)**: LCD `app_lcd_input.c:752` `LV_OUT_POWER` [50,100] 클램프(Modbus는 클램프하나 LCD 경로 누락 → 진폭 언더플로) · `limit_mo_out`/`limit_energy`/`limit_out_time` config-validation. samd20 참조 `ref/samd20/main.c:1404-1466`(물리 start switch). **weld-START 상호작용 결정**: seek-reset START guard가 US_CYCLE START도 게이팅 — slice4 물리 트리거 도입 시 의식적 결정(현재 weld dormant라 inert). 슬라이스1~3 거동의 실 사이클/스테핑 E2E도 여기서.
2. **B-SEAM OSC 물리 구동**: seek/reset/weld의 실제 `CTRL_OSC*` 주파수 출력 (현재 전부 hook stub=mon 로그만). M16 흡수로 출력 극성 미확인 → 실 초음파 rig + 스코프 필요. 메모리 `project_m16_regulation_verified`(B-SEAM = OSC output binding이 핵심 blocker).
3. **6b signal calibration**: 진폭/주파수/에너지 **절대 보정** (벤치는 DISP_POWER=0 무신호 → 누산/진폭 절대값 검증 불가; 로직은 host-test로만 검증됨). divisor `REG_ENERGY_DIV=250` 등 실 rig에서 확정.
4. **overload 스테이지**: OVLD(PC0 입력)→RESET→자동 SEEK 복구 — **seek-reset FSM 재사용** 설계됨. samd20 overload 시퀀스 포팅.

## Failed Approaches / 반복 주의 (이전 세션 교훈)

- **CMake GLOB stale-build**: `fw/CMakeLists.txt:90` `file(GLOB src/*.c)`는 configure 시점에만 평가 → 새 소스 추가된 브랜치 전환 후 `cmake --build`(증분)만 돌리면 새 `.c` 미링크(`undefined reference` + "dangerous relocation"). **브랜치 전환/소스 추가 후 반드시 `cmake -B build -G Ninja` reconfigure.**
- **START 트리거 통합 금지**(weld): 물리 SW_START=사이클 / 패널·Modbus START=직접 초음파 — **분리 유지**. 과거 통합했다 되돌림. slice4에서 재라우팅 금지.
- **mbpoll 첫 트랜잭션 CRC**: RS-485 첫 호출이 종종 `Invalid CRC`(라인 settling) → 더미 read 1회 후 진행. 함수명은 zsh alias 피해 `mbread`/`mbspin`/`mbwrite` 사용.
- **ICON/짧은 transient 검증**: 560ms blink은 육안 추적 난망 → ON_TIME 확대(clamp max=100=1s) + 반복 트리거 + 콘솔 마커. STATUS bit 없는 RESET/SEEK는 **대조법**([A]유휴=발생 ↔ [B]직교=무시)으로 차이 가시화.

## Current State

**Working**: main 전부 빌드 0-warning(FLASH 42.11%/RAM 16.77%), host **5스위트 PASS**(reg_calc/modbus_core/tcp_frame/weld_fsm/seek_reset_fsm). origin 동기화 완료.

**Broken**: 없음.

**Uncommitted**: 없음(`.understand-anything/`·`ref/atmega16/M16_reverse/` 기존 untracked, 무관). ⚠ `git stash list`에 옛 auto-stash 1개 잔존 가능(checkout 시 생성, 불필요하면 `git stash drop`).

## Files to Know

| File | Why It Matters |
|------|----------------|
| `docs/superpowers/RESUME.md` | 자동 로드 권위 상태 — 2026-06-19 블록 최신 (단 해시는 재작성 전 값) |
| `docs/NEXT_STEPS.md` | CLAUDE.md 지정 first-load 문서 — ⚠ **stale(2026-06-17 b)**, slice3/seek-reset 미머지로 표기됨. 갱신 필요 |
| `ref/samd20/main.c` | 포팅 소스 원본 (slice4 물리 start `1404-1466`, overload 등) |
| `fw/include/app_modbus_core.h` | Modbus 레지스터 맵 (검증·외부 HMI 계약) |
| `fw/include/app_config.h` | `g_cfg`@`0x20000a5c` 구조체 (SWD 직독 디코드) |
| `fw/src/app_seek_reset.c` | overload가 재사용할 FSM/글루 |

## Resume Instructions (다음 세션 시작)

1. `docs/NEXT_STEPS.md` + `docs/superpowers/RESUME.md`(자동 로드) 읽기. ⚠ 거기 해시는 **재작성 전** 값 — 현재 해시는 `git log --oneline`로 확인.
2. 현 상태 확인: `git status`(=`## main...origin/main` 동기) / `git log --oneline -5` / `git tag`.
3. **HW 있으면** → 남은 작업 1~4 중 선택. 보드 검증 패턴(아래 보드 절) 재사용.
4. **HW 없으면** → slice4/overload **brainstorming + spec**만 선행 가능(설계는 코드 없이). 단 절대 검증은 HW 세션으로.

### 보드 검증 절 (HW 세션용)

보드 기본 = **SERIAL/addr=1/9600/EVEN**(USART6=Modbus 점유 → mon 비가용). RS-485=`/dev/cu.usbserial-AB0MLYXA`, ST-LINK=`/dev/cu.usbmodem114303`.
```bash
cd fw && env -u STM32_TOOLCHAIN cmake -B build -G Ninja && env -u STM32_TOOLCHAIN cmake --build build   # ⚠ 새 소스면 reconfigure 필수
env -u STM32_TOOLCHAIN cmake --build build --target flash
# 통신: mbpoll -m rtu -a 1 -b 9600 -P even -t 4 -r 30 -c 1 -1 /dev/cu.usbserial-AB0MLYXA   (첫 Invalid CRC면 1회 더)
# SWD config: openocd -f openocd/stm32f410.cfg -c "init" -c "halt" -c "mdw 0x20000a5c 16" -c "resume" -c "shutdown"
```
- 레지스터(mbpoll `-r`=addr+1): RESET=26 / SEEK=27 / START=28 / STOP=29 / STATUS=30(bit0=초음파on) / WORK_CNTL=2 / OUT_POWER=7 / ON_TIME=8. ⚠ DISP_ENERGY=6(0x05).
- **ON_TIME(직접런 ceiling) clamp max=100**(1000ms). 테스트로 바꿨으면 56 복원(FC06=FRAM 영속).

## Warnings

- **옛 해시 신뢰 금지** — filter-repo 재작성으로 모든 SHA 변경(위 §). 태그/메시지로 참조.
- **GitHub 이메일 인증 (사용자 액션, 미완료)**: 커밋이 shuug2 프로필로 연결되려면 `nogari@gmail.com`을 shuug2 계정 Settings → Emails에 등록·인증해야 함(소급 적용). 인증 전에도 코드·히스토리는 정상 푸시됨.
- **push = SSH 전용**: `origin = git@github.com:shuug2/gds_us_ctrl.git`. HTTPS 자격증명 없음. 에이전트 샌드박스 환경에선 push 인증 불가 → 사람 터미널에서.
- **물리 OSC 효과 전부 미검증**(hook stub만) — seek/reset/weld 실제 주파수 거동은 B-SEAM/6b까지 미확인.
- **빌드 env**: `$STM32_TOOLCHAIN` stale → `env -u STM32_TOOLCHAIN` 필수.
- **ether_ip=192.168.1.70**: 옛 DHCP 리스 잔존(FRAM), 무해. SERIAL 모드라 미사용.
