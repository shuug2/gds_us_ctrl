# Phase 1+2 Bootstrap Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** STM32F410RBT 보드를 96 MHz로 부팅시키고 (Phase 1) PB3 LED 1 Hz 토글 + USART6 시리얼로 1초마다 "[t=NNNN ms] hello" 출력 (Phase 2)까지 도달.

**Architecture:** CubeMX UI 미사용. STM32 HAL/CMSIS는 ST 공식 SDK에서 한 번 vendor 폴더로 in-tree 카피하고 read-only로 동결. 모든 어플리케이션 코드(`main.c`, 페리페럴 init, IRQ 핸들러, 모듈)는 직접 작성. 빌드는 CMake + arm-none-eabi-gcc 툴체인, 플래시/디버그는 OpenOCD + ST-LINK.

**Tech Stack:** STM32 HAL F4 v1.27 (vendor in-tree), arm-none-eabi-gcc 13.x, CMake 3.22+, Ninja, OpenOCD 0.12+

**Spec reference:** `docs/superpowers/specs/2026-04-26-phase1-2-bootstrap-design.md`

---

## File Structure

### Files to acquire (from STM32CubeF4 SDK)
- `fw/vendor/Drivers/STM32F4xx_HAL_Driver/{Inc,Src}/**`
- `fw/vendor/Drivers/CMSIS/Device/ST/STM32F4xx/{Include,Source}/**`
- `fw/vendor/Drivers/CMSIS/Include/**`
- `fw/vendor/Core/Inc/stm32f4xx_hal_conf.h` (template로 복사 후 본 plan에서 편집)
- `fw/vendor/Core/Src/system_stm32f4xx.c`
- `fw/vendor/startup_stm32f410rbtx.s`
- `fw/vendor/STM32F410RBTX_FLASH.ld`

### Files to create (new — application)
- `fw/CMakeLists.txt`
- `fw/arm-none-eabi-gcc.cmake`
- `fw/.gitignore`
- `fw/openocd/stm32f410.cfg`
- `fw/openocd/debug.gdb`
- `fw/include/clock.h`, `app.h`, `board.h`, `sys_tick.h`, `mon.h`, `periph.h`
- `fw/src/main.c`, `clock.c`, `irq.c`, `app.c`, `board.c`, `sys_tick.c`, `periph.c`
- `fw/drivers/usart.c`, `tim.c`, `mon_usart6.c`

### Files to move
- `fw/cube/gds_usctrl.ioc` → `docs/historical/gds_usctrl.ioc`

### Files to delete
- `fw/cube/` 디렉토리 (gds_usctrl.ioc 이동 후 비어있음)

### Files to modify (after Phase 2 verified)
- `CLAUDE.md` (CubeMX 미사용 / 96 MHz / `fw/vendor/` 구조)
- `docs/pinmap.md` (PB3 heartbeat 임시 표시)
- `docs/changelog.md` (Phase 1+2 완료 기록)
- `docs/superpowers/RESUME.md` (역할 종료 — 삭제 또는 archive)

### Files responsible for what
| 파일 | 책임 |
|------|------|
| `vendor/**` | ST가 제공하는 HAL/CMSIS/startup/linker. 우리는 절대 편집 ✗ |
| `arm-none-eabi-gcc.cmake` | CMake 툴체인 — cross-compiler 경로, CPU/FPU 플래그 |
| `CMakeLists.txt` | 빌드 entry — vendor lib + 우리 sources + 링크 + flash/debug 타겟 |
| `openocd/stm32f410.cfg` | ST-LINK + STM32F4 target 정의 |
| `openocd/debug.gdb` | GDB 자동 attach 스크립트 (load + break main) |
| `include/clock.h`, `src/clock.c` | `clock_init()` 96 MHz, `Error_Handler()` |
| `src/main.c` | Reset 후 진입점. HAL_Init → clock_init → 페리페럴 init → app_init → loop |
| `src/irq.c` | NMI/HardFault/SysTick/TIM11 IRQ 핸들러 |
| `include/periph.h`, `src/periph.c` | `huart6`, `htim11` HAL 핸들 변수 단일 정의 |
| `drivers/usart.c` | `usart6_init()` — PC6/PC7 AF8 + 115200 8N1 |
| `drivers/tim.c` | `tim11_init()` — PSC=95, ARR=999, NVIC enable |
| `drivers/mon_usart6.c` | `mon_*` API의 USART6 backend (HAL blocking TX) |
| `include/board.h`, `src/board.c` | `board_init()` (PB3 + CTRL_OSC0..4 GPIO out LOW), `board_heartbeat_toggle()` |
| `include/sys_tick.h`, `src/sys_tick.c` | TIM11 1 kHz monotonic 카운터 |
| `include/mon.h` | Monitor UART 인터페이스 — `mon_init/write/writeln/printf` |
| `include/app.h`, `src/app.c` | `app_init()` 배너, `app_loop_iter()` 1초 cadence |

---

## Task 1: Vendor 시드 파일 획득 (STM32CubeF4 SDK 카피)

**Files:**
- Acquire from: `https://github.com/STMicroelectronics/STM32CubeF4` (release 태그 v1.27.1 또는 ST 공식 SDK 설치 경로 `~/STM32Cube/Repository/STM32Cube_FW_F4_VX.X.X/`)
- Create: `fw/vendor/Drivers/STM32F4xx_HAL_Driver/{Inc,Src}/`
- Create: `fw/vendor/Drivers/CMSIS/Device/ST/STM32F4xx/{Include,Source}/`
- Create: `fw/vendor/Drivers/CMSIS/Include/`
- Create: `fw/vendor/Core/Inc/`, `fw/vendor/Core/Src/`

- [ ] **Step 1: STM32CubeF4 SDK 다운로드 (없으면)**

```bash
cd /tmp
git clone --depth 1 --branch v1.27.1 https://github.com/STMicroelectronics/STM32CubeF4.git
# 또는 ST 공식: STM32CubeMX 설치 후 ~/STM32Cube/Repository/STM32Cube_FW_F4_V1.27.1/ 사용
export STM32CUBE_F4=/tmp/STM32CubeF4
ls $STM32CUBE_F4/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal.h && echo "OK"
```

Expected: `OK` 출력. SDK 디렉토리 구조 확인됨.

- [ ] **Step 2: vendor 디렉토리 구조 생성**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
mkdir -p fw/vendor/Drivers/STM32F4xx_HAL_Driver/{Inc,Src}
mkdir -p fw/vendor/Drivers/CMSIS/Device/ST/STM32F4xx/{Include,Source}
mkdir -p fw/vendor/Drivers/CMSIS/Include
mkdir -p fw/vendor/Core/Inc fw/vendor/Core/Src
```

Expected: 디렉토리만 생성됨, 파일 0개.

- [ ] **Step 3: HAL Driver 카피 (Inc + Src 전체)**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
cp -r $STM32CUBE_F4/Drivers/STM32F4xx_HAL_Driver/Inc/* fw/vendor/Drivers/STM32F4xx_HAL_Driver/Inc/
cp -r $STM32CUBE_F4/Drivers/STM32F4xx_HAL_Driver/Src/* fw/vendor/Drivers/STM32F4xx_HAL_Driver/Src/
ls fw/vendor/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal.h
ls fw/vendor/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c
```

Expected: 두 ls 모두 파일 경로 출력 (HAL 라이브러리 전체 카피됨).

- [ ] **Step 4: CMSIS Device 카피**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
cp -r $STM32CUBE_F4/Drivers/CMSIS/Device/ST/STM32F4xx/Include/* fw/vendor/Drivers/CMSIS/Device/ST/STM32F4xx/Include/
cp $STM32CUBE_F4/Drivers/CMSIS/Device/ST/STM32F4xx/Source/Templates/system_stm32f4xx.c fw/vendor/Core/Src/
cp $STM32CUBE_F4/Drivers/CMSIS/Device/ST/STM32F4xx/Source/Templates/gcc/startup_stm32f410rx.s fw/vendor/startup_stm32f410rbtx.s
ls fw/vendor/Drivers/CMSIS/Device/ST/STM32F4xx/Include/stm32f410rx.h
ls fw/vendor/Core/Src/system_stm32f4xx.c
ls fw/vendor/startup_stm32f410rbtx.s
```

Expected: 세 ls 모두 파일 경로 출력.

> Note: `startup_stm32f410rx.s`를 `startup_stm32f410rbtx.s`로 rename — 패키지 변종(RBT) 명시. 내용은 동일.

- [ ] **Step 5: CMSIS Core 카피**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
cp -r $STM32CUBE_F4/Drivers/CMSIS/Include/* fw/vendor/Drivers/CMSIS/Include/
ls fw/vendor/Drivers/CMSIS/Include/core_cm4.h
```

Expected: `core_cm4.h` 등 CMSIS 코어 헤더 카피됨.

- [ ] **Step 6: Linker script 카피**

ST의 Cube_FW에는 보드별 example에 linker script가 있음. STM32F410RB-Nucleo example에서 가져옴:

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
find $STM32CUBE_F4/Projects/NUCLEO-F410RB -name '*FLASH.ld' | head -1
# 결과 예: /tmp/STM32CubeF4/Projects/NUCLEO-F410RB/Templates/STM32CubeIDE/STM32F410RBTX_FLASH.ld

cp $(find $STM32CUBE_F4/Projects/NUCLEO-F410RB -name '*FLASH.ld' | head -1) fw/vendor/STM32F410RBTX_FLASH.ld
ls fw/vendor/STM32F410RBTX_FLASH.ld
```

Expected: `STM32F410RBTX_FLASH.ld` vendor 폴더에 존재.

- [ ] **Step 7: hal_conf 템플릿 카피**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
cp fw/vendor/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_conf_template.h fw/vendor/Core/Inc/stm32f4xx_hal_conf.h
ls fw/vendor/Core/Inc/stm32f4xx_hal_conf.h
```

Expected: `stm32f4xx_hal_conf.h` vendor/Core/Inc/에 존재. 다음 task에서 편집.

- [ ] **Step 8: Commit vendor seed**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
git add fw/vendor/
git -c commit.gpgsign=false commit -m "fw: import STM32CubeF4 vendor sources (HAL + CMSIS + startup + linker)

Source: STM32CubeF4 v1.27.1 (https://github.com/STMicroelectronics/STM32CubeF4)
Status: read-only in-tree copy. Do not edit (regen via SDK upgrade only)."
```

Expected: 커밋 완료. `git status`로 vendor 깨끗.

---

## Task 2: `.ioc` 파일을 historical 폴더로 격하

**Files:**
- Move: `fw/cube/gds_usctrl.ioc` → `docs/historical/gds_usctrl.ioc`
- Delete: `fw/cube/` (빈 디렉토리)

- [ ] **Step 1: `docs/historical/` 디렉토리 생성 + .ioc 이동**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
mkdir -p docs/historical
git mv fw/cube/gds_usctrl.ioc docs/historical/gds_usctrl.ioc
rmdir fw/cube
ls fw/cube 2>/dev/null && echo "STILL EXISTS" || echo "GONE"
ls docs/historical/gds_usctrl.ioc
```

Expected: "GONE" + `docs/historical/gds_usctrl.ioc` 경로 출력.

- [ ] **Step 2: `docs/historical/README.md` 작성**

```bash
cat > /Users/tknoh/dev/work/gds_us_ctrl/docs/historical/README.md <<'EOF'
# Historical Artifacts

이 폴더의 파일들은 **read-only 보존 자료**입니다. 수정하거나 의존하지 마세요.

## gds_usctrl.ioc

STM32CubeMX 프로젝트 파일. 2026-04-26 brainstorming에서 CubeMX UI 의존을 제거하기로
결정한 후, 회로 핀 할당 이력 추적 목적으로만 보존. 펌웨어 빌드는 본 파일을 참조하지 않으며
실제 핀 매핑의 source of truth는 `docs/pinmap.md`입니다.
EOF
```

- [ ] **Step 3: Commit move**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
git add docs/historical/
git -c commit.gpgsign=false commit -m "docs: archive .ioc as historical reference

Per Phase 1+2 bootstrap design: CubeMX UI workflow removed.
Pin map source of truth is docs/pinmap.md, not the .ioc."
```

---

## Task 3: 툴체인 파일 작성

**Files:**
- Create: `fw/arm-none-eabi-gcc.cmake`

- [ ] **Step 1: 툴체인 파일 작성**

```cmake
# fw/arm-none-eabi-gcc.cmake
set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

if(DEFINED ENV{STM32_TOOLCHAIN})
    set(TOOLCHAIN_PREFIX "$ENV{STM32_TOOLCHAIN}/arm-none-eabi-")
else()
    set(TOOLCHAIN_PREFIX "arm-none-eabi-")
endif()

set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_OBJCOPY      ${TOOLCHAIN_PREFIX}objcopy CACHE INTERNAL "")
set(CMAKE_OBJDUMP      ${TOOLCHAIN_PREFIX}objdump CACHE INTERNAL "")
set(CMAKE_SIZE         ${TOOLCHAIN_PREFIX}size    CACHE INTERNAL "")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CPU_FLAGS "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard")
set(CMAKE_C_FLAGS_INIT   "${CPU_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${CPU_FLAGS}")
set(CMAKE_ASM_FLAGS_INIT "${CPU_FLAGS} -x assembler-with-cpp")
```

작성: `/Users/tknoh/dev/work/gds_us_ctrl/fw/arm-none-eabi-gcc.cmake`

- [ ] **Step 2: 컴파일러 PATH 동작 확인**

```bash
arm-none-eabi-gcc --version | head -1
```

Expected: `arm-none-eabi-gcc (...) 13.x.x` 또는 그 이상.

설치 안 됐으면: `brew install --cask gcc-arm-embedded` (macOS) 또는 ST의 STM32CubeCLT 설치.

- [ ] **Step 3: Commit**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
git add fw/arm-none-eabi-gcc.cmake
git -c commit.gpgsign=false commit -m "fw: add arm-none-eabi-gcc CMake toolchain file"
```

---

## Task 4: `.gitignore` 작성

**Files:**
- Create: `fw/.gitignore`

- [ ] **Step 1: gitignore 작성**

```
build/
*.map
*.bin
*.hex
*.elf
*.su
```

작성: `/Users/tknoh/dev/work/gds_us_ctrl/fw/.gitignore`

- [ ] **Step 2: Commit**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
git add fw/.gitignore
git -c commit.gpgsign=false commit -m "fw: add .gitignore for build artifacts"
```

---

## Task 5: `stm32f4xx_hal_conf.h` Phase 1 모듈 selection 편집

**Files:**
- Modify: `fw/vendor/Core/Inc/stm32f4xx_hal_conf.h` (vendor 시드 카피본을 1회 편집)

> 본 파일은 vendor read-only 원칙의 예외 — 모듈 enable/disable이 vendor 카피본에 위치한 컨벤션이라 부득이 편집. spec §0.1 Q5 + §2.2 결정 사항.

- [ ] **Step 1: hal_conf.h를 열어 Phase 1 모듈만 활성화**

`/Users/tknoh/dev/work/gds_us_ctrl/fw/vendor/Core/Inc/stm32f4xx_hal_conf.h`를 열고:

(a) 파일 상단 Module Selection 섹션에서 다음 라인을 **주석 해제**:
```c
#define HAL_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
```

(b) 다음 라인은 **모두 주석 처리** (Phase 2/Stage A에서 추가):
```c
/* #define HAL_ADC_MODULE_ENABLED */
/* #define HAL_CAN_MODULE_ENABLED */
/* #define HAL_CRC_MODULE_ENABLED */
/* #define HAL_DCMI_MODULE_ENABLED */
/* #define HAL_DAC_MODULE_ENABLED */
/* #define HAL_DFSDM_MODULE_ENABLED */
/* ... 나머지 모든 _MODULE_ENABLED */
/* #define HAL_I2C_MODULE_ENABLED */
/* #define HAL_I2S_MODULE_ENABLED */
/* #define HAL_IWDG_MODULE_ENABLED */
/* #define HAL_LPTIM_MODULE_ENABLED */
/* #define HAL_RNG_MODULE_ENABLED */
/* #define HAL_RTC_MODULE_ENABLED */
/* #define HAL_SD_MODULE_ENABLED */
/* #define HAL_SMARTCARD_MODULE_ENABLED */
/* #define HAL_SPI_MODULE_ENABLED */
/* #define HAL_TIM_MODULE_ENABLED */
/* #define HAL_UART_MODULE_ENABLED */
/* #define HAL_USART_MODULE_ENABLED */
/* #define HAL_WWDG_MODULE_ENABLED */
```

(c) `#define HSE_VALUE` 라인 — STM32F410의 default 25 MHz로 두되 사용 안 함 (HSI 사용). 변경 없음.

(d) `#define TICK_INT_PRIORITY` 라인 — `0x0F`로 default 되어있으면 **`0x00`으로 변경** (HAL tick이 어플리케이션 IRQ 선점):
```c
#define TICK_INT_PRIORITY            ((uint32_t)0x00U)
```

(e) `#define USE_FULL_ASSERT` 라인 — 주석 처리 그대로 (Phase 1에선 미사용).

- [ ] **Step 2: Commit**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
git add fw/vendor/Core/Inc/stm32f4xx_hal_conf.h
git -c commit.gpgsign=false commit -m "fw: configure HAL modules for Phase 1 (RCC/GPIO/DMA/CORTEX/FLASH/PWR)

TICK_INT_PRIORITY = 0 so HAL SysTick preempts application IRQs.
Phase 2 will add UART + TIM. Other modules deferred to Stage A/C."
```

---

## Task 6: `clock.h` + `clock.c` 작성 (Phase 1 핵심)

**Files:**
- Create: `fw/include/clock.h`
- Create: `fw/src/clock.c`

- [ ] **Step 1: `clock.h` 작성**

```c
/* fw/include/clock.h */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

void clock_init(void);
void Error_Handler(void);

#ifdef __cplusplus
}
#endif
```

- [ ] **Step 2: `clock.c` 작성**

```c
/* fw/src/clock.c */
#include "stm32f4xx_hal.h"
#include "clock.h"

void clock_init(void) {
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    osc.OscillatorType       = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState             = RCC_HSI_ON;
    osc.HSICalibrationValue  = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState         = RCC_PLL_ON;
    osc.PLL.PLLSource        = RCC_PLLSOURCE_HSI;
    osc.PLL.PLLM             = 8;
    osc.PLL.PLLN             = 96;
    osc.PLL.PLLP             = RCC_PLLP_DIV2;
    osc.PLL.PLLQ             = 4;
    osc.PLL.PLLR             = 2;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) Error_Handler();

    clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                       | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_3) != HAL_OK) Error_Handler();
}
```

- [ ] **Step 3: Commit**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
git add fw/include/clock.h fw/src/clock.c
git -c commit.gpgsign=false commit -m "fw: add clock_init() — HSI x12 = 96 MHz with FLASH_LATENCY_3"
```

---

## Task 7: 최소 IRQ 핸들러 (`irq.c` Phase 1)

**Files:**
- Create: `fw/src/irq.c`

- [ ] **Step 1: `irq.c` 작성**

```c
/* fw/src/irq.c */
#include "stm32f4xx_hal.h"

void NMI_Handler(void)        { while (1) {} }
void HardFault_Handler(void)  { while (1) {} }   /* TODO Stage A: register dump via mon_printf */
void MemManage_Handler(void)  { while (1) {} }
void BusFault_Handler(void)   { while (1) {} }
void UsageFault_Handler(void) { while (1) {} }
void SVC_Handler(void)        { /* unused */ }
void DebugMon_Handler(void)   { /* unused */ }
void PendSV_Handler(void)     { /* unused */ }

void SysTick_Handler(void) { HAL_IncTick(); }

void Error_Handler(void) { __disable_irq(); while (1) {} }
```

- [ ] **Step 2: Commit**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
git add fw/src/irq.c
git -c commit.gpgsign=false commit -m "fw: add minimal Phase 1 IRQ handlers (faults, SysTick, Error_Handler)"
```

---

## Task 8: `main.c` Phase 1 형태 (빈 while 루프)

**Files:**
- Create: `fw/src/main.c`

- [ ] **Step 1: Phase 1 main.c 작성**

```c
/* fw/src/main.c — Phase 1 form */
#include "stm32f4xx_hal.h"
#include "clock.h"

int main(void) {
    HAL_Init();
    clock_init();

    /* Phase 1 verification: GDB로 PC 위치 확인. Phase 2 modules는 Task 17에서 추가 */
    while (1) {
        __NOP();
    }
}
```

- [ ] **Step 2: Commit**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
git add fw/src/main.c
git -c commit.gpgsign=false commit -m "fw: add Phase 1 main.c (HAL_Init + clock_init + idle loop)"
```

---

## Task 9: `CMakeLists.txt` Phase 1 형태

**Files:**
- Create: `fw/CMakeLists.txt`

- [ ] **Step 1: Phase 1 CMakeLists 작성**

```cmake
# fw/CMakeLists.txt
cmake_minimum_required(VERSION 3.22)

if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/arm-none-eabi-gcc.cmake")
endif()

project(gds_us_ctrl C ASM)

set(CMAKE_C_STANDARD          11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS        OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

add_compile_options(
    -Wall -Wextra -Wundef -Wshadow
    -ffunction-sections -fdata-sections
    -fno-common
    -fstack-usage
    $<$<CONFIG:Debug>:-Og;-g3;-gdwarf-2>
    $<$<CONFIG:Release>:-O2>
    $<$<CONFIG:MinSizeRel>:-Os>
)

add_compile_definitions(
    STM32F410Rx
    USE_HAL_DRIVER
)

# vendor HAL static library — Phase 1 sources only
set(VENDOR ${CMAKE_CURRENT_SOURCE_DIR}/vendor)

set(HAL_SOURCES
    ${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c
    ${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c
    ${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c
    ${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c
    ${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c
    ${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr.c
    ${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr_ex.c
    ${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c
    ${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ex.c
    ${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c
    ${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma_ex.c
)

add_library(stm32_hal STATIC
    ${HAL_SOURCES}
    ${VENDOR}/Core/Src/system_stm32f4xx.c
)
target_include_directories(stm32_hal PUBLIC
    ${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Inc
    ${VENDOR}/Drivers/CMSIS/Device/ST/STM32F4xx/Include
    ${VENDOR}/Drivers/CMSIS/Include
    ${VENDOR}/Core/Inc
)
target_compile_options(stm32_hal PRIVATE -Wno-unused-parameter)

# 어플리케이션 — Phase 1 sources
set(APP_SOURCES
    src/main.c
    src/clock.c
    src/irq.c
)
set(STARTUP ${VENDOR}/startup_stm32f410rbtx.s)
set(LDSCRIPT ${VENDOR}/STM32F410RBTX_FLASH.ld)

add_executable(${PROJECT_NAME}.elf
    ${APP_SOURCES}
    ${STARTUP}
)
target_include_directories(${PROJECT_NAME}.elf PRIVATE include)
target_link_libraries(${PROJECT_NAME}.elf PRIVATE stm32_hal)

target_link_options(${PROJECT_NAME}.elf PRIVATE
    -T${LDSCRIPT}
    -Wl,--gc-sections
    -Wl,-Map=${PROJECT_NAME}.map,--cref
    -Wl,--print-memory-usage
    --specs=nano.specs
    -lc -lm -lnosys
)

add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.hex
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}.elf>
    COMMENT "Generating .bin / .hex / size"
)

# Flash / debug — OPENOCD_CFG는 Task 11에서 작성
set(OPENOCD_CFG ${CMAKE_CURRENT_SOURCE_DIR}/openocd/stm32f410.cfg)

add_custom_target(flash
    DEPENDS ${PROJECT_NAME}.elf
    COMMAND openocd -f ${OPENOCD_CFG}
        -c "program $<TARGET_FILE:${PROJECT_NAME}.elf> verify reset exit"
    USES_TERMINAL
)

add_custom_target(reset
    COMMAND openocd -f ${OPENOCD_CFG} -c "init; reset; exit"
    USES_TERMINAL
)

add_custom_target(server
    COMMAND openocd -f ${OPENOCD_CFG}
    USES_TERMINAL
)

add_custom_target(gdb
    DEPENDS ${PROJECT_NAME}.elf
    COMMAND ${TOOLCHAIN_PREFIX}gdb $<TARGET_FILE:${PROJECT_NAME}.elf>
        -x ${CMAKE_CURRENT_SOURCE_DIR}/openocd/debug.gdb
    USES_TERMINAL
)
```

- [ ] **Step 2: Commit**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
git add fw/CMakeLists.txt
git -c commit.gpgsign=false commit -m "fw: add Phase 1 CMakeLists.txt (HAL lib + main/clock/irq + flash targets)"
```

---

## Task 10: Phase 1 빌드 확인

**Files:**
- (build only — no source changes)

- [ ] **Step 1: CMake configure**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl/fw
cmake -B build -G Ninja
```

Expected: 출력 끝부분에 `-- Configuring done` + `-- Generating done` + `-- Build files have been written to: ...build`. 에러 없음.

- [ ] **Step 2: Build**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl/fw
cmake --build build 2>&1 | tail -20
```

Expected:
- 경고 없음 (`-Wall -Wextra` 통과)
- `gds_us_ctrl.elf` 링크 성공
- `Memory region   Used Size   ...` 출력 (FLASH 5–15 KB, RAM 1 KB 이하)
- `.bin / .hex / size` 산출 메시지

- [ ] **Step 3: 산출물 확인**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl/fw/build
ls gds_us_ctrl.{elf,bin,hex,map}
arm-none-eabi-size gds_us_ctrl.elf
```

Expected: 4 파일 모두 존재. size 출력에 `text` 약 5–15 KB.

- [ ] **Step 4: 빌드 산출물 git 미추적 확인**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
git status
```

Expected: `build/`, `*.elf` 등 untracked로 표시되지 ✗ — `.gitignore` 정상.

> 이 task는 commit 없음 (build verification only).

---

## Task 11: OpenOCD 설정

**Files:**
- Create: `fw/openocd/stm32f410.cfg`
- Create: `fw/openocd/debug.gdb`

- [ ] **Step 1: OpenOCD 설치 확인**

```bash
openocd --version 2>&1 | head -1
```

Expected: `Open On-Chip Debugger 0.12.0` 또는 그 이상.

설치 안 됐으면: `brew install open-ocd` (macOS).

- [ ] **Step 2: `stm32f410.cfg` 작성**

```bash
mkdir -p /Users/tknoh/dev/work/gds_us_ctrl/fw/openocd
```

```tcl
# fw/openocd/stm32f410.cfg
source [find interface/stlink.cfg]
transport select hla_swd
source [find target/stm32f4x.cfg]
reset_config srst_only srst_nogate
```

작성: `/Users/tknoh/dev/work/gds_us_ctrl/fw/openocd/stm32f410.cfg`

- [ ] **Step 3: `debug.gdb` 작성**

```gdb
# fw/openocd/debug.gdb
target extended-remote :3333
set print pretty on
set pagination off
monitor reset halt
load
break main
continue
```

작성: `/Users/tknoh/dev/work/gds_us_ctrl/fw/openocd/debug.gdb`

- [ ] **Step 4: OpenOCD config 문법 확인**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl/fw
openocd -f openocd/stm32f410.cfg -c "init; exit" 2>&1 | tail -10
```

Expected: ST-LINK 연결되었으면 `Info: stm32f4x.cpu: hardware has 6 breakpoints` 등 + clean exit. 보드 미연결이면 `Error: open failed` (이건 정상 — 보드 없을 때 기대 동작).

- [ ] **Step 5: Commit**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
git add fw/openocd/
git -c commit.gpgsign=false commit -m "fw: add OpenOCD config for ST-LINK + STM32F4 + GDB attach script"
```

---

## Task 12: Phase 1 보드 검증 (HW 연결 필요)

**Files:**
- (verification — no source changes)

> ⚠️ 이 task는 ST-LINK 프로브가 보드에 연결돼있어야 진행 가능. 미연결 상태면 spec 6.5 체크리스트의 Phase 1 항목으로 미루고 Task 13으로 진행.

- [ ] **Step 1: 플래시**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl/fw
cmake --build build --target flash 2>&1 | tail -20
```

Expected: `** Verified OK **` + `** Resetting Target **` 출력.

- [ ] **Step 2: GDB attach (별도 터미널 2개 필요)**

터미널 1:
```bash
cd /Users/tknoh/dev/work/gds_us_ctrl/fw
cmake --build build --target server
```

터미널 2:
```bash
cd /Users/tknoh/dev/work/gds_us_ctrl/fw
cmake --build build --target gdb
```

Expected: GDB 프롬프트 + main 진입점에 break, continue. 코드가 `while(1) __NOP()`에 도착.

- [ ] **Step 3: GDB에서 PC + SystemCoreClock 확인**

GDB 프롬프트에서:
```
Ctrl-C    (중단)
(gdb) info reg pc
(gdb) print SystemCoreClock
(gdb) call SystemCoreClockUpdate()
(gdb) print SystemCoreClock
(gdb) quit
```

Expected:
- `pc` = `main()` 또는 그 이후 어딘가 (`while(1)` 루프 안)
- `print SystemCoreClock` after `SystemCoreClockUpdate()` = `96000000`

- [ ] **Step 4: Phase 1 완료 메모를 commit message로 기록**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
git -c commit.gpgsign=false commit --allow-empty -m "checkpoint: Phase 1 verified on hardware

- Flash: OK
- Boot: PC reaches main() while(1)
- Clock: SystemCoreClock = 96000000 (96 MHz)"
```

---

## Task 13: Phase 2 HAL 모듈 추가 (`hal_conf.h` 편집)

**Files:**
- Modify: `fw/vendor/Core/Inc/stm32f4xx_hal_conf.h`

- [ ] **Step 1: UART + TIM 모듈 활성화**

`/Users/tknoh/dev/work/gds_us_ctrl/fw/vendor/Core/Inc/stm32f4xx_hal_conf.h`에서 다음 두 라인의 주석 해제:
```c
#define HAL_TIM_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED
```

- [ ] **Step 2: Commit**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
git add fw/vendor/Core/Inc/stm32f4xx_hal_conf.h
git -c commit.gpgsign=false commit -m "fw: enable HAL_UART + HAL_TIM modules for Phase 2"
```

---

## Task 14: HAL 핸들 단일 정의 (`periph.h` + `periph.c`)

**Files:**
- Create: `fw/include/periph.h`
- Create: `fw/src/periph.c`

- [ ] **Step 1: `periph.h` 작성**

```c
/* fw/include/periph.h */
#pragma once
#include "stm32f4xx_hal.h"

extern UART_HandleTypeDef huart6;
extern TIM_HandleTypeDef  htim11;
```

- [ ] **Step 2: `periph.c` 작성**

```c
/* fw/src/periph.c */
#include "periph.h"

UART_HandleTypeDef huart6;
TIM_HandleTypeDef  htim11;
```

- [ ] **Step 3: Commit**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
git add fw/include/periph.h fw/src/periph.c
git -c commit.gpgsign=false commit -m "fw: add periph.h/c for huart6 + htim11 single definition"
```

---

## Task 15: USART6 driver

**Files:**
- Create: `fw/drivers/usart.c`

- [ ] **Step 1: `usart.c` 작성**

```c
/* fw/drivers/usart.c */
#include "stm32f4xx_hal.h"
#include "periph.h"
#include "clock.h"  /* Error_Handler */

void usart6_init(void) {
    __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIO_InitTypeDef g = {
        .Pin       = GPIO_PIN_6 | GPIO_PIN_7,
        .Mode      = GPIO_MODE_AF_PP,
        .Pull      = GPIO_NOPULL,
        .Speed     = GPIO_SPEED_FREQ_VERY_HIGH,
        .Alternate = GPIO_AF8_USART6,
    };
    HAL_GPIO_Init(GPIOC, &g);

    __HAL_RCC_USART6_CLK_ENABLE();
    huart6.Instance          = USART6;
    huart6.Init.BaudRate     = 115200;
    huart6.Init.WordLength   = UART_WORDLENGTH_8B;
    huart6.Init.StopBits     = UART_STOPBITS_1;
    huart6.Init.Parity       = UART_PARITY_NONE;
    huart6.Init.Mode         = UART_MODE_TX_RX;
    huart6.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart6.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart6) != HAL_OK) Error_Handler();
}
```

- [ ] **Step 2: Commit**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
git add fw/drivers/usart.c
git -c commit.gpgsign=false commit -m "fw: add usart6_init driver (PC6/7 AF8, 115200 8N1, blocking only)"
```

---

## Task 16: TIM11 driver

**Files:**
- Create: `fw/drivers/tim.c`

- [ ] **Step 1: `tim.c` 작성**

```c
/* fw/drivers/tim.c */
#include "stm32f4xx_hal.h"
#include "periph.h"
#include "clock.h"

void tim11_init(void) {
    __HAL_RCC_TIM11_CLK_ENABLE();
    htim11.Instance               = TIM11;
    htim11.Init.Prescaler         = 95;
    htim11.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim11.Init.Period            = 999;
    htim11.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim11.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim11) != HAL_OK) Error_Handler();

    HAL_NVIC_SetPriority(TIM1_TRG_COM_TIM11_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(TIM1_TRG_COM_TIM11_IRQn);
}
```

- [ ] **Step 2: Commit**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
git add fw/drivers/tim.c
git -c commit.gpgsign=false commit -m "fw: add tim11_init driver (1 kHz IRQ, NVIC priority 5)"
```

---

## Task 17: Board module

**Files:**
- Create: `fw/include/board.h`
- Create: `fw/src/board.c`

- [ ] **Step 1: `board.h` 작성**

```c
/* fw/include/board.h */
#pragma once

void board_init(void);
void board_heartbeat_toggle(void);
```

- [ ] **Step 2: `board.c` 작성**

```c
/* fw/src/board.c */
#include "stm32f4xx_hal.h"
#include "board.h"

#define HB_PORT   GPIOB
#define HB_PIN    GPIO_PIN_3       /* CON_OVLD heartbeat 임시 — Stage A에서 본용도 복원 */
#define CTRL_OSC_PINS (GPIO_PIN_10 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15)

void board_init(void) {
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef out = {
        .Mode  = GPIO_MODE_OUTPUT_PP,
        .Pull  = GPIO_NOPULL,
        .Speed = GPIO_SPEED_FREQ_LOW,
    };
    HAL_GPIO_WritePin(HB_PORT, HB_PIN, GPIO_PIN_RESET);
    out.Pin = HB_PIN;
    HAL_GPIO_Init(HB_PORT, &out);

    HAL_GPIO_WritePin(GPIOB, CTRL_OSC_PINS, GPIO_PIN_RESET);
    out.Pin = CTRL_OSC_PINS;
    HAL_GPIO_Init(GPIOB, &out);
}

void board_heartbeat_toggle(void) {
    HAL_GPIO_TogglePin(HB_PORT, HB_PIN);
}
```

- [ ] **Step 3: Commit**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
git add fw/include/board.h fw/src/board.c
git -c commit.gpgsign=false commit -m "fw: add board module (PB3 heartbeat + CTRL_OSC0..4 safe LOW)"
```

---

## Task 18: sys_tick module

**Files:**
- Create: `fw/include/sys_tick.h`
- Create: `fw/src/sys_tick.c`

- [ ] **Step 1: `sys_tick.h` 작성**

```c
/* fw/include/sys_tick.h */
#pragma once
#include <stdint.h>

void sys_tick_init(void);
uint32_t sys_tick_get_ms(void);
void sys_tick_handle_irq(void);
```

- [ ] **Step 2: `sys_tick.c` 작성**

```c
/* fw/src/sys_tick.c */
#include "stm32f4xx_hal.h"
#include "periph.h"
#include "clock.h"
#include "sys_tick.h"

static volatile uint32_t s_ms = 0;

void sys_tick_init(void) {
    if (HAL_TIM_Base_Start_IT(&htim11) != HAL_OK) Error_Handler();
}

uint32_t sys_tick_get_ms(void) {
    return s_ms;   /* 32-bit read는 Cortex-M4에서 atomic */
}

void sys_tick_handle_irq(void) {
    s_ms++;
}
```

- [ ] **Step 3: Commit**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
git add fw/include/sys_tick.h fw/src/sys_tick.c
git -c commit.gpgsign=false commit -m "fw: add sys_tick module (TIM11-driven 1 kHz monotonic counter)"
```

---

## Task 19: Monitor UART interface + USART6 backend

**Files:**
- Create: `fw/include/mon.h`
- Create: `fw/drivers/mon_usart6.c`

- [ ] **Step 1: `mon.h` 작성**

```c
/* fw/include/mon.h */
#pragma once
#include <stddef.h>
#include <stdint.h>

void mon_init(void);
void mon_write(const uint8_t *buf, size_t len);
void mon_writeln(const char *s);
int  mon_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
```

- [ ] **Step 2: `mon_usart6.c` 작성**

```c
/* fw/drivers/mon_usart6.c */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "stm32f4xx_hal.h"
#include "periph.h"
#include "mon.h"

void mon_init(void) { /* huart6는 usart6_init이 이미 초기화 */ }

void mon_write(const uint8_t *buf, size_t len) {
    HAL_UART_Transmit(&huart6, (uint8_t *)buf, len, HAL_MAX_DELAY);
}

void mon_writeln(const char *s) {
    mon_write((const uint8_t *)s, strlen(s));
    mon_write((const uint8_t *)"\r\n", 2);
}

int mon_printf(const char *fmt, ...) {
    char buf[128];
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n > 0) {
        size_t emit = (n < (int)sizeof buf) ? (size_t)n : sizeof buf - 1;
        mon_write((const uint8_t *)buf, emit);
    }
    return n;
}
```

- [ ] **Step 3: Commit**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
git add fw/include/mon.h fw/drivers/mon_usart6.c
git -c commit.gpgsign=false commit -m "fw: add mon interface + USART6 blocking backend (mon_init/write/writeln/printf)"
```

---

## Task 20: app module (1초 cadence)

**Files:**
- Create: `fw/include/app.h`
- Create: `fw/src/app.c`

- [ ] **Step 1: `app.h` 작성**

```c
/* fw/include/app.h */
#pragma once

void app_init(void);
void app_loop_iter(void);
```

- [ ] **Step 2: `app.c` 작성**

```c
/* fw/src/app.c */
#include "stm32f4xx_hal.h"
#include "app.h"
#include "board.h"
#include "sys_tick.h"
#include "mon.h"

static uint32_t s_last_beat_ms = 0;

void app_init(void) {
    sys_tick_init();
    mon_init();
    mon_writeln("[boot] gds_us_ctrl phase2 ready");
    s_last_beat_ms = sys_tick_get_ms();
}

void app_loop_iter(void) {
    uint32_t now = sys_tick_get_ms();
    if ((uint32_t)(now - s_last_beat_ms) >= 1000) {
        s_last_beat_ms = now;
        board_heartbeat_toggle();
        mon_printf("[t=%lu ms] hello\r\n", (unsigned long)now);
    }
}
```

- [ ] **Step 3: Commit**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
git add fw/include/app.h fw/src/app.c
git -c commit.gpgsign=false commit -m "fw: add app module (banner + 1 Hz heartbeat + hello cadence)"
```

---

## Task 21: `main.c` Phase 2 형태로 업데이트

**Files:**
- Modify: `fw/src/main.c`

- [ ] **Step 1: Phase 2 main.c 작성 (Phase 1 버전 덮어쓰기)**

```c
/* fw/src/main.c — Phase 2 form */
#include "stm32f4xx_hal.h"
#include "clock.h"
#include "app.h"

extern void usart6_init(void);   /* drivers/usart.c */
extern void tim11_init(void);    /* drivers/tim.c */
extern void board_init(void);    /* src/board.c */

int main(void) {
    HAL_Init();
    clock_init();      /* 96 MHz */
    /* TODO Stage A: iwdg_init(2000); */
    usart6_init();     /* PC6/PC7 + 115200 8N1 */
    tim11_init();      /* 1 kHz IRQ enabled, base not started yet */
    board_init();      /* GPIO out + CTRL_OSC0..4 LOW */
    app_init();        /* sys_tick start, mon banner */

    while (1) {
        app_loop_iter();
        /* TODO Stage A: HAL_IWDG_Refresh(&hiwdg); */
    }
}
```

- [ ] **Step 2: Commit**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
git add fw/src/main.c
git -c commit.gpgsign=false commit -m "fw: update main.c to Phase 2 form (calls all init + app loop)"
```

---

## Task 22: `irq.c` 업데이트 (TIM11 IRQ 추가)

**Files:**
- Modify: `fw/src/irq.c`

- [ ] **Step 1: `irq.c` 끝에 TIM11 IRQ 핸들러 + HAL callback 추가**

`/Users/tknoh/dev/work/gds_us_ctrl/fw/src/irq.c`의 마지막 줄 다음에 추가:

```c

#include "periph.h"
#include "sys_tick.h"

void TIM1_TRG_COM_TIM11_IRQHandler(void) {
    HAL_TIM_IRQHandler(&htim11);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM11) {
        sys_tick_handle_irq();
    }
}
```

> Note: `#include "periph.h"`와 `#include "sys_tick.h"`를 파일 상단으로 옮길지 끝에 둘지는 스타일 — 끝에 두면 Phase 1 버전과 diff 깔끔. 위 추가 그대로 OK.

- [ ] **Step 2: Commit**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
git add fw/src/irq.c
git -c commit.gpgsign=false commit -m "fw: add TIM11 IRQ handler + HAL_TIM_PeriodElapsedCallback for sys_tick"
```

---

## Task 23: `CMakeLists.txt`에 Phase 2 sources 추가

**Files:**
- Modify: `fw/CMakeLists.txt`

- [ ] **Step 1: HAL_SOURCES 리스트에 UART/TIM 추가**

`/Users/tknoh/dev/work/gds_us_ctrl/fw/CMakeLists.txt`에서 `set(HAL_SOURCES ...)` 블록의 마지막 항목 (`stm32f4xx_hal_dma_ex.c`) 뒤에 다음 3줄 추가:

```cmake
    # Phase 2:
    ${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c
    ${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim.c
    ${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim_ex.c
```

- [ ] **Step 2: APP_SOURCES를 GLOB으로 변경 + drivers 추가**

`set(APP_SOURCES ...)` 블록을 통째로 다음으로 교체:

```cmake
file(GLOB APP_SOURCES src/*.c drivers/*.c)
```

- [ ] **Step 3: 링커 옵션에 `-u _printf_float` 추가**

`target_link_options(${PROJECT_NAME}.elf ...)` 블록에서 `--specs=nano.specs` 라인 뒤에:

```cmake
    -u _printf_float
```

- [ ] **Step 4: Commit**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
git add fw/CMakeLists.txt
git -c commit.gpgsign=false commit -m "fw: extend CMakeLists for Phase 2 (UART/TIM HAL + drivers/ glob + printf_float)"
```

---

## Task 24: Phase 2 빌드 확인

- [ ] **Step 1: 클린 빌드**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl/fw
rm -rf build
cmake -B build -G Ninja
cmake --build build 2>&1 | tail -30
```

Expected:
- 경고 없음
- `gds_us_ctrl.elf` 링크 성공
- `Memory region   Used Size   ...` FLASH 약 25–35 KB, RAM 약 2 KB
- `.bin / .hex / size` 산출 메시지

- [ ] **Step 2: size 상세 확인**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl/fw/build
arm-none-eabi-size -A -d gds_us_ctrl.elf | head -20
```

Expected: `.text` 약 25–35 KB, `.data` < 1 KB, `.bss` < 2 KB.

> Phase 2 verification 빌드까지 클린 — commit 없음.

---

## Task 25: Phase 2 보드 검증 (HW 연결 필요)

> ⚠️ ST-LINK + USB-Serial(USART6 PC6/PC7 연결) 모두 필요.

- [ ] **Step 1: 플래시**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl/fw
cmake --build build --target flash 2>&1 | tail -10
```

Expected: `** Verified OK **`.

- [ ] **Step 2: 시리얼 터미널 연결**

```bash
# macOS
ls /dev/cu.usb* | head -3
# 결과 예: /dev/cu.usbserial-XXXX
screen /dev/cu.usbserial-XXXX 115200
# 또는
# minicom -D /dev/ttyUSB0 -b 115200    (Linux)
```

Expected: 부팅 직후 다음 출력 + 매초 hello:
```
[boot] gds_us_ctrl phase2 ready
[t=1000 ms] hello
[t=2000 ms] hello
[t=3000 ms] hello
```

종료: `Ctrl-A` → `K` → `y` (screen)

- [ ] **Step 3: PB3 LED (또는 오실로스코프) 확인**

육안: PB3 LED가 1초 ON, 1초 OFF (= 2초 주기 사각파).
오실로스코프: PB3 핀에서 0.5 Hz 사각파 측정.

- [ ] **Step 4: 5분 안정성 확인**

screen 터미널에서 5분 관찰. hello 출력 멈춤 없음, t값 단조증가, 누락 없음.

- [ ] **Step 5: GDB로 s_ms 단조성 확인 (선택)**

```bash
# 터미널 1: cmake --build build --target server
# 터미널 2: arm-none-eabi-gdb build/gds_us_ctrl.elf
```
GDB:
```
(gdb) target extended-remote :3333
(gdb) print s_ms
$1 = 12345
(gdb) continue
Ctrl-C  (10초 후)
(gdb) print s_ms
$2 = 22345    # +10000 ± 10
(gdb) quit
```

Expected: 10초 사이 약 10000 증가.

- [ ] **Step 6: Phase 2 완료 메모를 commit으로 기록**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
git -c commit.gpgsign=false commit --allow-empty -m "checkpoint: Phase 2 verified on hardware

- Flash: OK
- Boot banner: '[boot] gds_us_ctrl phase2 ready' on USART6 @ 115200 8N1
- Heartbeat: PB3 LED 0.5 Hz square wave
- Hello cadence: '[t=Nms] hello' every 1 s, t monotonic
- Stability: 5 min run no stalls/faults"
```

---

## Task 26: 문서 동기화 — `CLAUDE.md`

**Files:**
- Modify: `CLAUDE.md`

- [ ] **Step 1: CLAUDE.md 업데이트 — 4가지 항목**

`/Users/tknoh/dev/work/gds_us_ctrl/CLAUDE.md`에서:

(a) "Core" 라인을 변경:
```
- Core | ARM Cortex-M4F @ 100 MHz |
```
→
```
- Core | ARM Cortex-M4F @ 96 MHz |
```

(b) "디렉토리 구조" 섹션의 fw/ 부분을 다음으로 교체:

```
├── fw/                       # STM32F410RBT 펌웨어 (CMake, CubeMX UI 미사용)
│   ├── vendor/               # ST HAL/CMSIS read-only in-tree (수정 ✗)
│   ├── include/              # 우리 공용 헤더
│   ├── src/                  # main, app, board, clock, sys_tick, irq, periph
│   ├── drivers/              # usart, tim, mon_usart6 (페리페럴 init + driver)
│   ├── openocd/              # ST-LINK + GDB attach 설정
│   ├── CMakeLists.txt
│   └── arm-none-eabi-gcc.cmake
```

(c) "빌드" 섹션의 명령은 그대로 두되 (`cmake -B build -G Ninja` 등 동일):

(d) "작업 시 주의사항" 섹션에서 다음 두 라인 삭제:
- `CubeMX 재생성 후 직접 수정한 코드가 덮어씌워지지 않도록 USER CODE BEGIN/END 섹션 준수.`
- (`.ioc` 관련 라인 있으면 함께 삭제)

그리고 다음 라인 추가:
```
- `fw/vendor/`는 read-only — ST SDK 업그레이드 외엔 절대 편집 ✗.
- 페리페럴 GPIO는 그 드라이버가 직접 책임 (`drivers/usart.c`가 PC6/PC7 AF 설정).
- HAL 핸들 변수는 `src/periph.c`에 단일 정의, `include/periph.h`로 extern.
```

(e) "다음 세션 시작 시" 섹션의 `docs/NEXT_STEPS.md`로 가는 부분은 유지하되, "확정 결정" 리스트에 다음 추가:
```
- CubeMX UI 미사용, HAL/CMSIS는 vendor in-tree 카피 (Phase 1+2 spec 결정)
- MCU 클럭 96 MHz (HSI x12, source of truth는 fw/src/clock.c)
```

- [ ] **Step 2: Commit**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
git add CLAUDE.md
git -c commit.gpgsign=false commit -m "docs: sync CLAUDE.md with Phase 1+2 design (96 MHz, fw/vendor, no CubeMX UI)"
```

---

## Task 27: 문서 동기화 — `docs/pinmap.md`

**Files:**
- Modify: `docs/pinmap.md`

- [ ] **Step 1: PB3 (CON_OVLD) 항목에 heartbeat 임시 표시 추가**

`/Users/tknoh/dev/work/gds_us_ctrl/docs/pinmap.md`에서 `PB3` 또는 `CON_OVLD` 라인을 찾아 비고/Note 컬럼에 추가:

```
* heartbeat 1 Hz toggle (Phase 2~Stage A 임시. Stage A에서 본용도 복원)
```

- [ ] **Step 2: Commit**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
git add docs/pinmap.md
git -c commit.gpgsign=false commit -m "docs: mark PB3 (CON_OVLD) as temporary heartbeat for Phase 2"
```

---

## Task 28: 문서 동기화 — `docs/changelog.md`

**Files:**
- Modify: `docs/changelog.md`

- [ ] **Step 1: changelog 항목 추가**

`/Users/tknoh/dev/work/gds_us_ctrl/docs/changelog.md`의 가장 위(또는 적절한 날짜 위치)에:

```markdown
## 2026-04-26 — Phase 1+2 Bootstrap 완료

- CubeMX UI 의존 제거 결정 (Q2). HAL/CMSIS vendor in-tree 카피로 동결.
- 디렉토리 재편: `fw/cube/` → `fw/vendor/` + `fw/{src,include,drivers}/` 신설.
- Phase 1: HSI×12 = 96 MHz 부팅, 빈 main while 루프 도달 GDB 검증.
- Phase 2: TIM11 1 kHz 틱, USART6 115200 8N1 monitor, PB3 1 Hz heartbeat.
- 빌드 시스템: CMake + arm-none-eabi-gcc 툴체인 + OpenOCD/ST-LINK flash/debug.
- 산출 spec: `docs/superpowers/specs/2026-04-26-phase1-2-bootstrap-design.md`
- 산출 plan: `docs/superpowers/plans/2026-04-26-phase1-2-bootstrap.md`
- `.ioc` 격하: `docs/historical/gds_usctrl.ioc` (read-only 보존).
```

- [ ] **Step 2: Commit**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
git add docs/changelog.md
git -c commit.gpgsign=false commit -m "docs: changelog Phase 1+2 bootstrap completion"
```

---

## Task 29: `RESUME.md` 정리

**Files:**
- Delete or archive: `docs/superpowers/RESUME.md`

- [ ] **Step 1: archive 폴더로 이동 (역할 종료, 이력 보존)**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
mkdir -p docs/superpowers/archive
git mv docs/superpowers/RESUME.md docs/superpowers/archive/2026-04-26-RESUME.md
```

- [ ] **Step 2: SessionStart 훅 비활성화 (선택)**

`.claude/settings.json`의 SessionStart 훅이 archive된 RESUME.md를 가리키면 다음 세션에서 에러 발생 가능. 훅을 제거하거나 새 위치로 갱신:

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
cat .claude/settings.json
```

만약 훅이 RESUME.md를 참조하면 해당 항목 제거. 결정 필요한 사항이라 본 task에서는 파일만 이동 + 사용자에게 훅 정리 여부 확인 요청 (실행 시 prompt).

- [ ] **Step 3: Commit**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
git add docs/superpowers/
git -c commit.gpgsign=false commit -m "docs: archive RESUME.md (Phase 1+2 brainstorming complete)"
```

---

## Task 30: graphify 재생성

**Files:**
- Regenerate: `graphify-out/`

> 메모리 등록한 규칙 (`feedback_graphify_after_docs.md`): 문서/코드가 변경되면 graphify 재생성.

- [ ] **Step 1: graphify 스킬 호출**

대화 중 `/graphify` 슬래시 커맨드 입력. 옵션 없이 호출 → skill이 변경된 파일들을 스캔.

또는 본 plan을 자동 실행 중인 경우 다음으로 명시 호출:

```
Skill: graphify
```

Expected: `graphify-out/{graph.html,GRAPH_REPORT.md,graph.json}` 갱신.

- [ ] **Step 2: 결과 확인**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
ls -la graphify-out/
cat graphify-out/GRAPH_REPORT.md | head -30
```

Expected: 모든 파일의 mtime이 방금 시각, GRAPH_REPORT에 새 모듈(`board`, `sys_tick`, `mon`, `app`) 노드가 보임.

- [ ] **Step 3: Commit**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
git add graphify-out/
git -c commit.gpgsign=false commit -m "docs: regenerate graphify after Phase 1+2 implementation"
```

---

## Final verification

모든 task 완료 후:

- [ ] `git log --oneline | head -35` — 약 30개의 작은 commit
- [ ] `cd fw && rm -rf build && cmake -B build -G Ninja && cmake --build build` — 클린 빌드 무경고
- [ ] `cd fw && cmake --build build --target flash` — 보드에 플래시 성공
- [ ] 시리얼 터미널 5분 관찰 — `[t=N ms] hello` 단조 증가, 누락 없음
- [ ] `git status` — clean (untracked: build/, *.elf, *.bin, *.hex, *.map만 — `.gitignore`로 무시됨)
- [ ] CLAUDE.md / pinmap / changelog 모두 업데이트 commit
- [ ] graphify-out/ 새 모듈 반영
