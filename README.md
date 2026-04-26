# gds_us_ctrl — GD-SONIC Ultrasonic Controller

STM32F410RBT 기반 초음파 컨트롤러 펌웨어 + 하드웨어.

## Structure
- `hw/` — KiCad 회로 및 PCB 설계
- `fw/` — STM32F410RBT 펌웨어 (CubeMX + CMake)
- `docs/` — 핀맵, 요구사항, 변경 로그

## Target
- **MCU:** STM32F410RBT (Cortex-M4F, 100 MHz, 128 KB Flash, 32 KB RAM, LQFP64)
- **Toolchain:** `arm-none-eabi-gcc` (`$STM32_TOOLCHAIN`)
- **SDK:** STM32 HAL/CMSIS (`$STM32_SDK`)

## Build (TBD)
```bash
cd fw
cmake -B build -G Ninja
cmake --build build
```

## Flash
```bash
# OpenOCD or J-Link
```

## Tagging
`hw-revA_fw-1.0.0` 형식으로 H/W rev + F/W 버전 함께 관리.
