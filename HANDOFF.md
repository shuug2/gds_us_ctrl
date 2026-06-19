# Handoff: 보드 세션 — weld slice3 + seek-reset HW 검증 + 머지 ✅ DONE

**Generated**: 2026-06-19
**Branch**: `main` (tip `8e4ca4f`) — feature 브랜치 2개(`feat/stage-weld-cycle-slice3-multi`, `feat/stage-seek-reset`) **머지 후 삭제**.
**Status**: ✅ DONE — CODE-COMPLETE였던 두 스테이지를 보드에서 검증 후 main 머지 + 태그. 코딩된 스테이지는 **전부 main**. 남은 작업 = HW-gated deferred만.

> 이전 HANDOFF(weld-cycle slice1)는 머지 완료로 supersede. 이력 = `docs/changelog.md`(2026-06-19) / `docs/superpowers/RESUME.md`(2026-06-19 블록, 자동 로드) / 메모리 `project_weld_cycle`·`project_seek_reset`. **다음 스테이지 착수 시 본 HANDOFF을 덮어쓸 것.**

## Goal

보드 연결 세션. CODE-COMPLETE 미머지였던 두 스테이지(weld slice3 multi_ctrl, SEEK/RESET 효과)를 실보드에서 검증하고 main에 머지. seek-reset이 slice3 위 stack이라 **머지 순서 고정**: ① slice3 회귀+머지 → ② seek-reset HW 4항목 → ③ seek-reset 머지.

## Completed

- [x] **① weld slice3 회귀확인 + 머지** — 머지 `a209ac1`(`--no-ff`), 태그 `hw-revA_fw-stage-weld3`. 회귀-1 직접-초음파 ceiling(`1×7→0`=560ms)+ICON_RUN / 회귀-2 EN_MULTI=1 직접 START 동일 ceiling(스테핑 없음=§6 deviation) / 회귀-3 work_cnt=0(START 테스트 전·후) / 회귀-4 FC03 미러=SWD `g_cfg` 직독 전건 일치.
- [x] **② seek-reset HW 4항목** — HW-1 직접-초음파/weld 무회귀(START guard 변경 무영향) / HW-2 자동 체인(RESET→ICON_RESET→ICON_SEEK 육안) / HW-3 SEEK 단발(ICON_SEEK만) / HW-4 양방향 RUN 직교(대조법: 런 중 RESET→STATUS 무중단·체인 없음 / 체인 중 START→STATUS 0·런 시작 안 함).
- [x] **③ seek-reset 머지** — 머지 `4c536bf`(`--no-ff`), 태그 `hw-revA_fw-stage-seekreset`.
- [x] 머지 후 main: 0-warning, FLASH 42.11%/RAM 16.77%, host 5스위트 PASS. 핸드오프(RESUME/changelog/메모리) 갱신.

## Not Yet Done (전부 HW-gated deferred — 실 초음파 rig/스코프 또는 슬라이스4 HW 필요)

- [ ] **B-SEAM OSC 물리 구동**: seek/reset/weld의 실제 `CTRL_OSC*` 출력 (현재 hook stub=mon 로그만). M16 흡수로 극성 미확인 — 실 초음파 rig + 스코프 필요.
- [ ] **6b signal calibration**: 진폭/주파수/에너지 절대 보정 (벤치는 DISP_POWER=0 무신호 → 누산/진폭 절대 E2E 불가, 로직만 host-test 검증됨).
- [ ] **weld 슬라이스4**: TRIGGER 모드 + 물리 SW_START + 위치센서 + 실 SOL_DN(PB5) + 안전 abort + **config-validation 클램프**(LCD `app_lcd_input.c:752` LV_OUT_POWER [50,100] / `limit_mo_out`·`limit_energy`·`limit_out_time`). 사이클/스테핑 자체 E2E도 여기서.
- [ ] **overload 스테이지**: OVLD→RESET→자동 SEEK 복구 — **seek-reset FSM 재사용** 설계됨.
- [ ] **weld-START 상호작용 결정**: seek-reset START guard가 `app_weld.c` US_CYCLE START도 게이팅 — 현재 weld dormant라 inert, 슬라이스4 물리 SW_START 도입 시 의식적 결정 필요(plan 노트).

## Failed Approaches / Gotchas (이번 세션 — 반복 주의)

- **CMake GLOB stale-build (빌드 실패 → reconfigure로 해결)**: seek-reset 브랜치 체크아웃 후 `cmake --build build`(증분)만 돌렸더니 링크 실패:
  ```
  undefined reference to `app_seek_reset_tick`
  (app_seek_reset_tick): Unknown destination type (ARM/Thumb) ... dangerous relocation
  ```
  원인 = `fw/CMakeLists.txt:90` `file(GLOB src/*.c …)`는 **configure 시점에만** 평가 → 브랜치가 추가한 새 소스(`app_seek_reset.c`)를 재-glob 안 함. **해결 = `cmake -B build -G Ninja` reconfigure** (즉시 `app_seek_reset.c.obj` 컴파일). **교훈: 새 소스 추가된 브랜치 전환 후엔 무조건 reconfigure.** (새 .c 없이 기존 파일만 수정한 브랜치=slice3는 증분 빌드 OK.)
- **mbpoll 첫 트랜잭션 CRC 에러**: RS-485 블록/write 첫 호출이 종종 `Invalid CRC`로 실패(라인 settling). → 더미 read 1회로 settle 후 진행. 타임아웃(무응답)과는 다름 — CRC는 응답은 왔는데 프레이밍 깨짐.
- **ICON 560ms 관찰 불가 → 확대/반복**: 단발 560ms blink은 육안 추적 실패. → ① ON_TIME=100(=1s, **clamp max**)로 ceiling 확대 + 반복 트리거(ON/OFF 깜빡 4~6회) ② RESET/SEEK는 mbpoll STATUS에 안 잡혀(no status bit) **반복 트리거 + 콘솔 마커 + 육안**이 유일 검증.
- **HW-4 부정(아이콘 안 뜸) 검증 어려움 → 대조법**: "no icon" 단독 관찰은 애매 → [A]유휴(체인/런 발생) ↔ [B]직교(무시) **대조**로 차이를 가시화. STATUS 패턴(런 무중단 `111111100000` / 런 없음 `0000…`)이 객관 증거. (⚠ HW-4(b)는 "START-during-chain에서 런 시작 안 함"을 관찰한 것 — "START가 펌웨어에 도달했고 거부됐다"로 과대주장 금지; CRC-실패 write와 관찰상 동일. 수렴 증거[STATUS+ICON+host-test+구조적 불변성]로 충분.)

## Key Decisions

| Decision | Rationale |
|----------|-----------|
| 머지 순서 ① slice3 → ② seek-reset 고정 | seek-reset이 slice3 tip `33b7ae9` 위 stack — 순서 뒤집으면 미검증 slice3 코드가 seek-reset 머지에 딸려 들어감 |
| 재플래시 없이 main 검증 종료 | 머지 후 FLASH 크기(42.11%)=HW-테스트한 브랜치 빌드와 동일 + merge-tree 무충돌 + host PASS → main == 테스트한 코드 (프로젝트 관행) |
| HW-4 STATUS 측정 + 대조법 | mon 비가용·RESET/SEEK는 status bit 없음 → 측정 가능한 STATUS(런 유무) + ICON 대조 + host-test + 구조적 불변성의 수렴 증거 |

## Current State

**Working**: main 전부 — Phase1+2·A·B·LCD·D·C·weld-1·2·3·seek-reset. 빌드 0-warning, host 5스위트 PASS. 보드에 main 이미지 플래시됨(재플래시 불요).

**Broken**: 없음.

**Uncommitted Changes**: 없음(워킹트리 클린; `.understand-anything/`·`ref/atmega16/M16_reverse/`는 기존 untracked, 무관).

## Files to Know

| File | Why It Matters |
|------|----------------|
| `docs/superpowers/RESUME.md` | 자동 로드되는 권위 상태 — 2026-06-19 블록이 최신 |
| `docs/superpowers/2026-06-17-board-session-checklist.md` | 이번 세션 실행 체크리스트(reg 맵·mbpoll 명령·머지 순서) |
| `fw/CMakeLists.txt` (line 90) | `file(GLOB src/*.c)` — 새 소스 추가 시 reconfigure 함정 |
| `fw/src/app_seek_reset.c` / `_fsm.c` | seek-reset 글루 + 순수 FSM (이번 머지 산출물) |
| `fw/src/app_weld_fsm.c` / `app_weld.c` | weld FSM (slice3 multi 2단 스테핑) |
| `fw/include/app_modbus_core.h` | Modbus 레지스터 맵 (검증 시 사용; 외부 HMI 계약) |
| `fw/include/app_config.h` | `g_cfg` 구조체 레이아웃 (SWD 직독 디코드용) |

## Resume Instructions (다음 세션 — 보드 검증 패턴 재사용 시)

보드=**SERIAL/addr=1/9600/EVEN**(USART6=Modbus 점유 → mon 비가용). RS-485=`/dev/cu.usbserial-AB0MLYXA`, ST-LINK=`/dev/cu.usbmodem114303`.

1. 브랜치 체크아웃 후 빌드(⚠ 새 소스 있으면 reconfigure 먼저):
   ```bash
   cd fw && env -u STM32_TOOLCHAIN cmake -B build -G Ninja && env -u STM32_TOOLCHAIN cmake --build build
   env -u STM32_TOOLCHAIN cmake --build build --target flash
   ```
   - Expected: `Verified OK` + 0-warning. 실패(`undefined reference`)면 → reconfigure 빠뜨림.
2. 통신 확인: `mbpoll -m rtu -a 1 -b 9600 -P even -t 4 -r 30 -c 1 -1 /dev/cu.usbserial-AB0MLYXA`
   - Expected: `[30]: 0`(STATUS idle). 첫 `Invalid CRC`면 1회 더(settling).
3. SWD config 직독: `openocd -f openocd/stm32f410.cfg -c "init" -c "halt" -c "mdw 0x20000a5c 16" -c "resume" -c "shutdown"`
4. 직접-초음파 회귀: START `-r 28 … 1` → STATUS `-r 30` 버스트 10회 → `1…1 0…0`(560ms ceiling).

## Edge Cases & Warnings

- **레지스터 맵**(mbpoll `-r` = addr+1): RESET=`-r 26`(0x19) / SEEK=`-r 27`(0x1A) / START=`-r 28`(0x1B) / STOP=`-r 29`(0x1C) / STATUS=`-r 30`(0x1D, bit0=초음파 on) / WORK_CNTL=`-r 2` / OUT_POWER=`-r 7` / ON_TIME=`-r 8`. ⚠ DISP_ENERGY=`-r 6`(0x05), 0x16 아님(=EN_SAFTY).
- **ON_TIME(=직접런 ceiling) clamp max=100**(1000ms). 테스트로 바꿨으면 56(560ms) 복원 필수.
- **FC06 write는 FRAM에 영속** — 테스트용 변경(EN_MULTI 등)은 반드시 원복. (이번 세션은 ON_TIME=56·EN_MULTI=0 복원 완료.)
- **mbpoll 함수명 zsh alias 충돌**: `rd`/`gap` 등은 zsh alias → 함수 정의 시 parse error. `mbread`/`mbspin`/`mbwrite` 등 비충돌 이름 사용.
- **물리 OSC 효과는 미검증**(hook stub만) — seek/reset/weld의 실제 주파수 거동은 B-SEAM/6b까지 미확인.
- **ether_ip=192.168.1.70**: 이전 DHCP 리스 잔존(FRAM), 무해. SERIAL 모드라 미사용. (DHCP→static 영속은 메모리 `project_eth_dhcp_static_persist` 참조.)
- **빌드 env**: `$STM32_TOOLCHAIN` stale → `env -u STM32_TOOLCHAIN` 필수.
