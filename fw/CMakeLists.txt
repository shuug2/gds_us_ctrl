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
    # Phase 2:
    ${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c
    ${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim.c
    ${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim_ex.c
    ${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_i2c.c
    # Stage D — regulation ADC:
    ${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_adc.c
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

# 어플리케이션 — Phase 1 sources
file(GLOB APP_SOURCES src/*.c drivers/*.c)
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
