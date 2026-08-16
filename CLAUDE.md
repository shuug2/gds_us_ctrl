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

`hw-revA_fw-1.0.0` 형식 — H/W rev + F/W 버전 함께 관리.

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

**현재 진행 (2026-08-15)**: **포팅 본체 종료 — main은 안정, 열린 것은 4건.** 2026-07-18~19 사용자 벤치 신규 8건(표시 데드밴드 0.15A·부팅 유령 SEEK·부팅 beep·fault 부저 알람·경고 페이지 터치 토글 반전·SYS_HORN horn-down·STD weld OVTIME 알람) **전건 HW PASS**, USOUT(PB4) 미출력=**PCB 원인 확정**(펌웨어 무수정). 보드=main `61524c1` 플래시·검증됨(⚠세션 말미 전원 OFF·잔재 설정 불확정).

열린 항목:
1. **★ HMI SP1 Task 8 실보드 E2E** — `~/dev/work/gds_us_hmi`(별도 repo) 세션. RS-485 어댑터 + LCD에서 SERIAL/addr=1/9600/EVEN 복원 필요, `docs/superpowers/research/2026-07-05-rs485-first-write.md` §6 지참.
2. ✅ ~~`refactor/ponytail-cleanup` HW 재검증→머지~~ — **완료 2026-08-16**. 리팩토링 4스테이지 + `define.h` 브랜드/버전 분리·MAKETECH·ether IP 커서 fix·`fw.sh`가 전부 main에 있다. 벤치 전항목 PASS(ceiling 실측 [514,578] ms·FC03/06·LCD 3항목).
3. **원격 제어 활성화 게이트 — T-1~T-4 CODE-COMPLETE** (브랜치 `feat/remote-enable-gate`, base=ponytail, origin 푸시됨). 순수 FSM+레지스터 `0x2A~0x2D`+글루+`apply_writes` 게이트 완료, **T-5(LCD 조작/표시)=DGUS 자산 대기**. ⚠ **기본 빌드 플래시 금지** — 게이트를 켤 수단이 없어 원격 명령이 전부 막히고 mbpoll 벤치가 죽는다. 벤치는 `-DREMOTE_EN_GATE_BYPASS=ON`. **T-5 없이 main 머지 금지.** 진입=브랜치 `HANDOFF.md`, 설계=`docs/superpowers/specs/2026-08-15-remote-enable-gate-design.md`. F-A(`0x1E~0x29` comm/eth)는 별도 스테이지.
4. **전류 0.60A 실측**(3회째 이월, 전류계 세션·플래시 불필요) / **6b·B-SEAM**(사용자 보류).

✅ push: **2026-08-16 실측 — 브랜치 3개·태그 21개 전부 origin 동기. 미푸시 없음.**
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
