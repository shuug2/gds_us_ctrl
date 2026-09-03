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
    $<$<CONFIG:Debug>:-Og>
    $<$<CONFIG:Debug>:-g3>
    $<$<CONFIG:Debug>:-gdwarf-2>
    $<$<CONFIG:Release>:-O2>
    $<$<CONFIG:MinSizeRel>:-Os>
)

add_compile_definitions(
    STM32F410Rx
    USE_HAL_DRIVER
    HSE_VALUE=16000000U    # 보드 16MHz X-tal (vendor conf 기본 25MHz 오버라이드 — vendor 무편집)
)

# 제품 모델(기능 티어) — H/W는 동일(hw-revA), F/W 기능셋으로만 갈린다.
# 값 검증은 define.h의 #error가 한다 (오타 -DMODEL=REMOET → 컴파일 실패).
#   cmake -B build-remote -DMODEL=REMOTE -G Ninja   또는   MODEL=remote ./fw.sh
set(MODEL "STD" CACHE STRING "제품 모델 — STD | REMOTE")
add_compile_definitions(MODEL_${MODEL})

# 원격 활성화 게이트 우회 — 벤치 전용 탈출구. MODEL=REMOTE 빌드에서만 의미가 있다
# (STD 빌드에는 게이트 코드 자체가 안 들어간다).
# 인터록 스위치(PC8)가 달린 PCB가 나오기 전까지는 REMOTE 빌드로 게이트를 켤 물리적
# 수단이 없어 모든 Modbus 명령/cfg 쓰기가 막힌다 = mbpoll 벤치 흐름이 죽는다.
# ⚠ 기본은 OFF(게이트 유효) — 플래그를 깜빡하면 "제한적"으로 실패하지, 안전 기능이
# 조용히 사라지지 않는다. PC8 실장 보드가 검증되면 이 옵션과 코드의 #ifdef를 함께 제거.
option(REMOTE_EN_GATE_BYPASS "벤치용: 원격 활성화 게이트 우회 (PC8 실장 전까지 한시)" OFF)
if(REMOTE_EN_GATE_BYPASS)
    add_compile_definitions(REMOTE_EN_GATE_BYPASS)
    message(WARNING "REMOTE_EN_GATE_BYPASS=ON — 원격 활성화 게이트가 무력화된 벤치 빌드입니다. 출하 빌드에 사용 금지.")
endif()

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
    # Phase 2:
    ${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c
    ${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim.c
    ${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim_ex.c
    ${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_i2c.c
    # Stage D — regulation ADC:
    ${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_adc.c
    # Stage C slice 2a — W5500 SPI:
    ${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_spi.c
)

add_library(stm32_hal STATIC
    ${HAL_SOURCES}
    ${VENDOR}/Core/Src/system_stm32f4xx.c
)
target_include_directories(stm32_hal SYSTEM PUBLIC
    ${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Inc
    ${VENDOR}/Drivers/CMSIS/Device/ST/STM32F4xx/Include
    ${VENDOR}/Drivers/CMSIS/Include
    ${VENDOR}/Core/Inc
)
target_compile_options(stm32_hal PRIVATE -Wno-unused-parameter)

# vendor WIZnet ioLibrary (W5500) — warning-isolated, SYSTEM includes
set(WIZNET ${VENDOR}/wiznet)
add_library(wiznet STATIC
    ${WIZNET}/Ethernet/wizchip_conf.c
    ${WIZNET}/Ethernet/socket.c
    ${WIZNET}/Ethernet/W5500/w5500.c
    ${WIZNET}/Internet/DHCP/dhcp.c
)
target_include_directories(wiznet SYSTEM PUBLIC
    ${WIZNET}/Ethernet
    ${WIZNET}/Ethernet/W5500
    ${WIZNET}/Internet/DHCP
)
# 3rd-party: do not hold it to our -Wall -Wextra -Wundef -Wshadow gate
target_compile_options(wiznet PRIVATE -w)

# 어플리케이션 — Phase 1 sources
file(GLOB APP_SOURCES src/*.c drivers/*.c)
set(STARTUP ${VENDOR}/startup_stm32f410rbtx.s)
set(LDSCRIPT ${VENDOR}/STM32F410RBTX_FLASH.ld)

add_executable(${PROJECT_NAME}.elf
    ${APP_SOURCES}
    ${STARTUP}
)
target_include_directories(${PROJECT_NAME}.elf PRIVATE include)
target_link_libraries(${PROJECT_NAME}.elf PRIVATE stm32_hal wiznet)

target_link_options(${PROJECT_NAME}.elf PRIVATE
    -T${LDSCRIPT}
    -Wl,--gc-sections
    -Wl,-Map=${PROJECT_NAME}.map,--cref
    -Wl,--print-memory-usage
    -Wl,--no-warn-rwx-segments
    --specs=nano.specs
    -u _printf_float
    -lc -lm
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
