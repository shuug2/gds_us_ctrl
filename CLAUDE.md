# CLAUDE.md — gds_us_ctrl

## 프로젝트 개요

**GD-SONIC 초음파 컨트롤러** 펌웨어 + 하드웨어 프로젝트.

기존에 **ATSAMD20** + **ATmega16** 두 MCU로 나뉘어 있던 기능을 **STM32F410RBT** 하나로 통합하는 것이 목표.

---

## 대상 MCU

| 항목 | 내용 |
|------|------|
| MCU | STM32F410RBT |
| Core | ARM Cortex-M4F @ 96 MHz |
| Flash | 128 KB |
| RAM | 32 KB |
| Package | LQFP64 |
| Toolchain | arm-none-eabi-gcc (`$STM32_TOOLCHAIN`) |
| SDK | STM32 HAL/CMSIS (`$STM32_SDK`) |

---

## 이전 MCU 구성 (통합 전)

| MCU | 역할 |
|-----|------|
| ATSAMD20 (Cortex-M0+) | (역할 확인 필요 — `ref/samd20/` 참조) |
| ATmega16 (AVR 8-bit) | (역할 확인 필요 — `ref/atmega16/` 참조) |

이전 코드는 `ref/` 디렉토리에 보관. 수정 없이 포팅 참조용으로만 사용.

---

## 디렉토리 구조

```
gds_us_ctrl/
├── fw/                       # STM32F410RBT 펌웨어 (CMake, CubeMX UI 미사용)
│   ├── vendor/               # ST HAL/CMSIS read-only in-tree (수정 ✗)
│   ├── include/              # 우리 공용 헤더
│   ├── src/                  # main, app, board, clock, sys_tick, irq, periph
│   ├── drivers/              # usart, tim, mon_usart6 (페리페럴 init + driver)
│   ├── openocd/              # ST-LINK + GDB attach 설정
│   ├── CMakeLists.txt
│   └── arm-none-eabi-gcc.cmake
├── hw/               # KiCad 회로 및 PCB 설계
│   ├── schematics/
│   ├── pcb/
│   └── bom/
├── docs/             # 핀맵, 요구사항, 변경 로그
│   ├── pinmap.md
│   ├── requirements.md
│   └── changelog.md
└── ref/              # 이전 MCU 참조 코드 (수정 금지)
    ├── samd20/       # ATSAMD20 원본 코드
    └── atmega16/     # ATmega16 원본 코드
```

---

## 빌드 · 플래시

루트의 `fw.sh`를 쓴다. 어느 디렉토리에서 실행해도 된다.

```bash
./fw.sh            # 빌드 (기본)
./fw.sh flash      # 빌드 + 플래시 (ST-LINK 필요)
./fw.sh reset      # 보드 리셋만
./fw.sh test       # host 테스트 (fw/test)
./fw.sh gdb        # 빌드 + GDB 접속
./fw.sh reconfig   # cmake 강제 재구성 후 빌드
```

스크립트가 대신 피해주는 함정 2개 — **수동으로 cmake를 부를 땐 직접 챙겨야 한다**:

1. `$STM32_TOOLCHAIN`이 stale 경로를 가리켜 cmake가 실패 → `env -u STM32_TOOLCHAIN cmake ...`
   (상세: `docs/superpowers/historical/2026-05-05-RESUME.md` §2.2)
2. `CMakeLists.txt:91`의 `file(GLOB src/*.c drivers/*.c)`는 **configure 타임 고정** → `.c`가
   추가/삭제된 브랜치로 전환한 뒤 증분 빌드만 하면 새 파일이 링크되지 않아 undefined
   reference로 터진다. `fw.sh`는 소스 목록을 `build/.src-glob` 스탬프와 비교해 달라졌을
   때만 재구성한다.

수동 등가 명령:

```bash
cd fw
env -u STM32_TOOLCHAIN cmake -B build -G Ninja                # 재구성
env -u STM32_TOOLCHAIN cmake --build build                    # 빌드
env -u STM32_TOOLCHAIN cmake --build build --target flash     # 플래시
# openocd 직접: openocd -f openocd/stm32f410.cfg \
#     -c "program build/gds_us_ctrl.elf verify reset exit"
```

플래시 타깃 정의 = `fw/CMakeLists.txt:123` (`reset`:130 / `server`:135 / `gdb`:140),
어댑터 설정 = `fw/openocd/stm32f410.cfg` (ST-LINK / hla_swd / stm32f4x).
성공 판정은 openocd 출력의 `** Verified OK **` + `** Resetting Target **`.

> ⚠ 런타임 검증에 SWD gdb halt 금지 — sys_tick이 멈춰 오진을 부른다. 검증은 mbpoll +
> LCD 육안으로. SWD는 플래시 또는 부팅 직후 정적 read 1회만.

---

## 태깅 규칙

`hw-revA_fw-<버전>` 형식 — H/W rev + F/W 버전 함께 관리. 두 종류를 쓴다:

| 종류 | 형식 | 뜻 |
|---|---|---|
| 릴리즈 | `hw-revA_fw-3.1.0` | 소스 트리 릴리즈. `docs/changelog.md` 의 `## [x.y.z]` 섹션과 1:1 |
| 스테이지 | `hw-revA_fw-stage-<이름>` | HW 검증을 통과한 중간 스택의 안정 레퍼런스. 릴리즈 아님 |

**버전 번호 = 기능 티어, 날짜 접미어 = 그 빌드** (`fw/include/define.h`):

- `3.0.x` = **STD** (레거시-동등 기능셋) — 번호를 동결하고 날짜만 진행한다. `V3.0.0_260905`
- `3.1.x` = **REMOTE** (원격기 연동) — `V3.1.0R_260905`, 접미어 `R` 이 모델 표식
- 접미어 `!` = PC8 인터록 극성 반전판. `REMOTE_EN_INTERLOCK_INVERTED` 를 따라가므로 원복을 잊어도 LCD 가 어긋나지 않는다

⚠ Modbus 소비자의 capability 판별은 버전 문자열이 아니다(버전 레지스터 없음, LCD 전용) —
`0x31 CFG_CAP` / `0x2A REMOTE_CAP` 이 그 일을 한다. 그래서 STD 번호 동결이 소비자를 오도하지 않는다.

---

## 작업 시 주의사항

- `ref/` 코드는 **읽기 전용**. 직접 수정하지 않는다.
- 펌웨어 주요 기능은 `docs/requirements.md`에 업데이트.
- 핀 할당 변경 시 `docs/pinmap.md`를 함께 수정.
- `fw/vendor/`는 read-only — ST SDK 업그레이드 외엔 절대 편집 ✗.
- 페리페럴 GPIO는 그 드라이버가 직접 책임 (`drivers/usart.c`가 PC6/PC7 AF 설정).
- HAL 핸들 변수는 `src/periph.c`에 단일 정의, `include/periph.h`로 extern.
- **MCU 간 통신 = 순수 GPIO 시그널링** (이전 SAMD20↔ATmega16 통신은 UART/SPI/TWI 미사용).
- **ETH/네트워크 설정 저장 주의**: DHCP 리스 IP는 static과 **동일한 `cfg->ether_ip`** 필드에 미러되므로, 리스가 떠 있는 상태에서 `comm_mode=ETH_STATIC`로 바꿔 LCD 저장하면 그 리스 IP가 static IP로 FRAM에 굳음 (의도된 동작 — 메모리 `project_eth_dhcp_static_persist`). static 테스트 시 IP 필드를 직접 입력할 것.

---

## 다음 세션 시작 시

**먼저 `docs/NEXT_STEPS.md`를 읽고 진행 상황과 다음 작업을 확인.**

🔴🔴 **배포 금지 (2026-09-05~)**: `8f33c5f` 이후 **PC8 인터록 극성이 반전**돼 있다(미실장 HIGH=허용). **단선·커넥터 탈락이 "허용" 이 되어 인터록 보호가 없다.** `gds_us_remote` 의 "STD 에도 원격 기동 / 확인 없이 탭" 결정과 합쳐지면 **원격 START = 탭 한 번 + 물리 인터록 없음**이다. 해제 조건 = **PC8 실장 PCB + 극성 원복(`fw/include/define.h` `REMOTE_EN_INTERLOCK_INVERTED` → `0`) + A-1·A-5·A-13 재시험 PASS**. 상세 = `HANDOFF.md` 최상단.

**현재 진행 (2026-09-06, 마감)**: **원격 hold-to-run 워치독 — 설계→구현→HW 벤치→머지 완료.** main `b8d33ee`(머지 `9b8e53b`, 태그 `hw-revA_fw-stage-hold-wdt`), origin 동기, 브랜치 정리. 보드 = **REMOTE `V3.1.0R!_260906`**(`96dc7d5` 빌드, LCD 에 `!` 표식), cfg 무변경, horn OFF. 배포 금지(PC8)는 그대로. 같은 날 **릴리즈 3.1.0 컷**(위 태깅 규칙 신설). 진입 = `docs/superpowers/RESUME.md` 최상단 블록 + `HANDOFF.md`. ⚠ **벤치 중 LCD 를 만지지 말 것** — SETUP 저장이 horn down 을 재전송해 START 가 게이트에 막힌다(bench-results 2026-09-06 §4-5). 아래 블록은 이전 세션.

**현재 진행 (2026-09-05, 벤치 세션 마감)**: **통합 벤치 PASS — 원격기 동등성 스택 + IWDG 둘 다 main 머지·태그 완료.** main `a9d844a`, 브랜치·태그 전부 origin 동기(미푸시 0). 🔴 **보드를 건드리기 전에 `docs/superpowers/plans/2026-09-05-bench-results.md` §4(벤치 환경 함정)를 먼저 읽을 것** — `nc -z` 금지 / TCP connect ≠ MCU 생존 / mbpoll 동작 불가(대체 = `docs/superpowers/tools/mb_tcp.py`). ✅ **RTU baud 9600 원복 완료(2026-09-05)** — `speed_idx 4→2`, 교차 커밋 TCP→serial, FRAM 영속. 단 실반영은 다음 SERIAL 전환 시(상세 = `HANDOFF.md` 열린 항목 1). ★ 남은 최우선 = **RS-485 어댑터 게이트 항목**(FA-6/7/12 · mon 캡처 · RTU 무회귀). ✅ **2026-09-05 추가: Modbus 결함 2건 수정 + 벤치 회귀 27항목 PASS** — `e569137` WORK_CNTL 가짜 리셋(host 가 유일한 게이트, 벤치 재현 불가) · `66a2411` stale-미러 레이스(`mirror_live()` 를 **디코드 앞**으로 이동; ⚠ 그 지점부터 `apply_writes` 사이에 cfg 쓰기를 넣으면 창이 다시 열린다). 보드 = `405f95e` REMOTE 빌드(2026-09-05 c 재플래시; 3.1.0 과 거동 동일, LCD 문자열은 구판). 실행 41항목 전건 PASS, 펌웨어 결함 0건. 태그 `hw-revA_fw-stage-remote-parity` / `hw-revA_fw-stage-iwdg`. 결과·환경 함정 = `docs/superpowers/plans/2026-09-05-bench-results.md`. **남은 검증은 전부 배선·PCB·육안 게이트** — ★ **A 섹션(게이트 강제) 전체가 PC8 실장 PCB 대기**. 아래 블록은 벤치 전 기록.

(이전) **원격기 기능 동등성 스택 CODE-COMPLETE — 남은 것은 HW 벤치 하나.** 2026-08-30 요구사항(`docs/superpowers/specs/2026-08-30-remote-parity-requirements.md`)의 **A·B-1~B-5·C 전항목**을 브랜치 `feat/remote-status-bits`(main `2f74611` 위 **20커밋, origin 동기**)에 구현했다. FLASH 50.38% / host 16 스위트 / 경고 0 / 코드리뷰 6건 반영 완료.

**★ 다음 세션 진입 = `docs/superpowers/plans/2026-09-04-remote-parity-bench-checklist.md`** (통합 벤치 체크리스트 — 실행 순서·mbpoll 번호표·보드 잔재·**할 수 없는 항목** 포함).

주요 신설:
1. **제품 모델 축 STD/REMOTE** — H/W 동일(hw-revA), F/W 기능셋으로만 갈린다. `MODEL=remote ./fw.sh` → `build-remote/`. **게이트는 REMOTE 전용**. 버전 문자열로 구분(`V3.0.0` / `V3.0.0R`)
2. **원격 활성화 게이트 = PC8 물리 인터록** (active-LOW+풀업 **fail-safe**, 만료 없음, `DIS_ESTOP` 만 래치)
3. **F-A comm/eth staging+commit** `0x1E~0x29` · **calibration** `0x2E/0x2F` · **HORN_CMD** `0x30` · **CFG_CAP** `0x31`
4. **TCP 연결 자동 복구** (앱 유휴 타임아웃 12s)

⚠ **레지스터 여유 6칸** — FC03 응답 상한 57칸(현재 51칸, `0x32 FEAT_CAP` 2026-09-06). 그 이상은 소비 측 스냅샷 원자성이 깨진다 (`app_modbus_core.h` `MB_REG_COUNT` 주석).

열린 항목:
1. **★ HW 벤치** — 보드 + **PC8 실장 PCB**(회로 수정 진행 중) + 패널 배선 필요. PC8 없이도 **STD 빌드로 S·M·B34·MOD·CAL·NET·FA 는 가능**
2. **HMI SP1 Task 8 실보드 E2E** — `~/dev/work/gds_us_hmi`(별도 repo)
3. **컨트롤러 측 `MODEL_TYPE` 차단 여부** — `gds_us_remote` 사용자 판단 대기(한 줄, 자리는 코드 주석에 표시)
4. **전류 0.60A 실측** / **6b·B-SEAM**(사용자 보류)
5. **IWDG 워치독 = 코드-완료** (`feat/iwdg-watchdog`, main+4, origin 동기; spec/plan `2026-09-04-iwdg-watchdog*`) — **T-3 HW 벤치 V-1~V-6 대기**. rsb 와 `merge-tree` 충돌 0 → 같은 벤치 세션에서 통합 빌드로 시험

**세션 간 협업**: `gds_us_remote`(세션명 `esp32-firmware-verification`)와 **상의**, `gds_us_hmi` 에는 **통보**. ⚠ **계약 문서는 양쪽 규율상 벤치 PASS 후에만 갱신.**

✅ push: **2026-09-04 실측 — 브랜치 전부 origin 동기, 태그 미푸시 0.** 로컬 전용이던 6개는 정리됨: `feat/symmetric-stop`(main 조상) 삭제 + `backup/pre-d5-*` 3 · `feat/physical-io-slice-a/c` 2 삭제(D5 reconcile 로 b'/d'/ch1' 이 이미 HW 검증·머지·태그 — 안정 레퍼런스는 `hw-revA_fw-stage-physio-b`/`-physio-d`/`-power-ch1`). 과거 "5개" 기록은 `feat/symmetric-stop` 을 빠뜨린 부정확한 수치였다.
⚠ model_type=multi(1) 잔재에서 Modbus 직접런 ceiling 미적용=정상(HAND 전용 설계, 30s 캡만 — 테스트 후 STOP 필수).

상세 진입 = 루트 `HANDOFF.md` + `docs/NEXT_STEPS.md`, 세션별 상태 = `docs/superpowers/RESUME.md`(자동 로드), 변경 이력 = `docs/changelog.md`.

해소된 핵심 질문 (V30 회로도 + ATmega16 분석으로 확정):
1. ATmega16 PA4 = 초음파 출력개시 신호 입력 / PC0 = overload 출력 / PC1·PC4 = 초음파 보드 신호 입력
2. 7-세그먼트 = 없음 (DGUS LCD 단독)
3. `I2C_POT` = U4 외부 I2C 디지털 포텐셔미터 @0x28 (EEPROM과 I2C1 버스 공유, 진폭 제어 실체)

확정 결정:
- RTOS 미사용 (슈퍼루프 유지)
- W5500 (SPI1), DGUS LCD (USART1), I2C EEPROM 모두 유지
- Modbus Serial OR TCP 택일 모드 — **구현 완료**(RTU=USART6 점유 / TCP=W5500, `comm_mode`로 택일)
- CubeMX UI 미사용, HAL/CMSIS는 `fw/vendor/` in-tree 카피 (Phase 1+2 spec 결정)
- MCU 클럭 96 MHz — **HSE(16MHz X-tal) PLL, HSI 폴백** (2026-07-05 HSI→HSE 전환; source of truth는 `fw/src/clock.c`, HSE_VALUE는 CMake 주입)
