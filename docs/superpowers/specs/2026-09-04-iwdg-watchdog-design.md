# IWDG 워치독 슬라이스 — 설계

> **요약**: `docs/requirements.md` FW3-6 이 요구하고 2026-07-02 감사 D3 가 "H4 + IWDG 는 별도 슬라이스"로 분리한 뒤 로드맵에서 유실됐던 **독립 워치독(IWDG)** 을 넣는다. 실코드 전수 조사 결과 (§2) 슈퍼루프 1회 반복의 **최악 블로킹 ≈ 2.6 s**(FRAM 버스 사망 시 `app_config_save_all` 38 write × 50 ms = 1.90 s + RTU 응답 TX @2400 0.53 s + 잔여)이고 **부팅 체인 최악 ≈ 12.0 s**(DGUS 4.12 s + run-page 재확인 2.28 s + OSC 2.03 s + FRAM 1.95 s + …)이다. 결정: **timeout 공칭 5.0 s**(÷256, RLR=624 → LSI 17~47 kHz 편차 시 **3.40~9.41 s**, 최악 2.6 s 대비 31 % 마진) / **IWDG 기동 = 슈퍼루프 진입 직전**(부팅 체인은 감시 밖 — 전 구간이 타임아웃으로 유계라 무한대기 없음) / **kick = `main.c` `while(1)` 단일 지점** / **DBGMCU freeze 1줄**(`./fw.sh gdb` 보호) / 리셋 원인 = **`RCC->CSR` 를 부팅 `[boot]` mon 배너 1줄 + SWD 정적 read 용 static** 으로만 표면화(Modbus 레지스터·STATUS 비트 ✗ — 계약 변경 없음) / **빌드 플래그 없음**(IWDG 는 소프트웨어 기동이라 플래시 락아웃이 없고, gdb 는 freeze 가 덮는다). HAL 은 `stm32f4xx_hal_iwdg.c` 가 vendor 트리에 있으나 **모듈이 conf 에서 비활성**(`/* #define HAL_IWDG_MODULE_ENABLED */`) → `HSE_VALUE` 선례대로 **CMake 주입 + HAL_SOURCES 1줄**로 켠다(vendor 무편집). **legacy(samd20·ATmega16) 는 둘 다 워치독 비활성** → 이 슬라이스는 **의도적 legacy 이탈**(hang 이 "영구 정지"에서 "≤9.4 s 내 자동 재부팅"으로 바뀜, `Error_Handler`/HardFault 도 재부팅 루프가 됨). host 테스트 가능한 순수 로직은 **없다**(레지스터 설정 + kick 뿐; `_Static_assert` 로 reload 범위만 고정). 검증 = HW 벤치 6항목(의도적 hang 주입 throwaway 빌드 포함, §6.2). 구현 규모 ≈ 5파일 ~20줄.

**작성일**: 2026-09-04 / **대상 브랜치**: `feat/remote-status-bits` 위 또는 main 위 독립(다른 작업 비의존, HW 불요) / **구현 세션**: 별도(Opus)

---

## 1. 배경 / 요구 근거

| 출처 | 내용 |
|---|---|
| `docs/requirements.md:92` FW3-6 | "워치독, 단위 테스트, 통합 테스트, 첫 릴리즈 태그" |
| `docs/NEXT_STEPS.md:61` | 2026-08-16 재편입. "HW 불요 · 1세션 규모 · 다른 작업에 비의존 … 슈퍼루프 kick 지점과 부팅 리셋-원인 판별(IWDG 리셋 vs POR) 설계 필요" |
| `docs/NEXT_STEPS.md:74` D3 | "H4(ADC `Error_Handler` 영구 lock) + IWDG 는 별도 슬라이스" |
| `fw/src/main.c:34,72` | Stage A 부터 남아 있던 TODO 2줄: `iwdg_init(2000)` / `HAL_IWDG_Refresh(&hiwdg)` — **kick 지점은 이미 예약돼 있었다** |

H4 와의 관계: `Error_Handler`(`irq.c:28`, `__disable_irq(); while(1)`)와 fault 핸들러 5종(`irq.c:8-16`)은 **코드 무변경**. IWDG 는 코어와 독립된 LSI 로 돌므로 그 `while(1)` 안에서도 카운트다운이 진행돼 자동으로 리셋한다 — 이것이 H4 의 "영구 lock" 을 푸는 최소 조치다(원인 덤프는 범위 밖, §7).

## 2. 조사 결과 (실코드 확인 — 2026-09-04, 브랜치 `feat/remote-status-bits` `34a5d43`)

### 2.1 슈퍼루프 구조

- **본체** = `main.c:70-73` `while(1){ app_loop_iter(); }`. 부팅 초기화는 `main.c:32-68` 직렬 호출.
- **tick** = TIM11 1 kHz IRQ(`tim.c:6-19`, prescaler 95/period 999 @96 MHz) → `sys_tick_handle_irq` `s_ms++`(`sys_tick.c:27-29`). HAL SysTick 도 별도로 살아 있음(`irq.c:31` `SysTick_Handler→HAL_IncTick`) — `HAL_Delay`/`HAL_GetTick` 기반 HAL 타임아웃은 이걸 쓴다. **ISR 은 TIM11·TIM5(FREQ_IN 캡처) 2개뿐**, 통신 RX 는 전부 DMA circular(USART1 `usart1.c:54-77`, USART6 `usart6_mb.c`) 폴링.
- **`app_loop_iter` 소비자**(`app.c:83-183`, 호출 순):

| # | 호출 | cadence(내부 게이트) | 비고 |
|---|---|---|---|
| 1 | `dgus_rx_poll`+`app_lcd_input_dispatch` | 매 iter | 터치 → DATA_SAVE 등 |
| 2 | `app_lcd_tick` | 4 ms(disp step) | VP 1그룹/step 블로킹 TX |
| 2.5~2.6 | `app_weld_tick`·`app_overload_tick`·`app_input_tick`·`app_horn_tick`·`app_seek_reset_tick` | 10 ms | 순수 FSM + GPIO |
| 3 | `app_reg_tick` | 1 ms ADC pace | 2ch 폴링 ADC |
| 4 | `app_eth_tick` | 100 ms link / 1 s DHCP | non-blocking FSM |
| 5 | `app_modbus_tick` | 매 iter | RTU 1프레임/iter or TCP |
| 6 | I2C err 관측 | 1 s | mon 1줄 |
| 6.5~7 | `app_fault_alarm_tick`·`app_buzzer_tick` | 10 ms | |

### 2.2 블로킹 구간 전수 (kick 주기 상한의 실제 제약)

**공통 상한 근거**: I2C1 `I2C1_TIMEOUT_MS=50`(`i2c1.h:6`) — HAL `HAL_I2C_Mem_Write/Read` 는 함수 진입 시 `tickstart` 하나를 모든 대기 단계(BUSY 25 ms 포함)가 공유하므로 **호출당 ≤ ~50 ms 로 유계**(`stm32f4xx_hal_i2c.c:2486-2566`). USART1 TX 10 ms(`usart1.c:83`), mon TX 50 ms(`mon_usart6.c:9`), RTU TX `len×11000/baud+50`(`usart6_mb.c:175`).

#### 2.2.1 부팅 체인 (`main()` 진입 → `while(1)`)

| 구간 | 위치 | 정상 | **최악** | 상한 근거 |
|---|---|---|---|---|
| HAL/clock/GPIO/USART/I2C/TIM init | `main.c:32-42` | <5 ms | (실패=`Error_Handler` 영구) | 레지스터 설정만 |
| 부팅 beep | `main.c:43-48` | 100 ms | 100 ms | `BOOT_BEEP_MS` 고정 |
| OSC boot-init `run_to_done` | `main.c:49-52`, `app_osc_init.c:22-30`, `app_osc_init_fsm.h:21-26` | ~2.0 s (PB12 H 대기 ≤1.2 s + H 유지 ~0.6 s + 210 ms) | **2.03 s** | WAIT_H 900 + WAIT_L 900 + GAP 150 + RESET 40 + SEEK 20 + tick 10 |
| `dgus_wait_ready` | `app.c:41`, `dgus_lcd.c:312-323`, `dgus_lcd.h:186-187` | 수십 ms | **4.12 s** | 4000 + 마지막 RD 120 |
| 로고 dwell | `app.c:49` | 1000 ms | 1000 ms | `DGUS_LOGO_DWELL_MS` 고정 |
| `app_config_load` | `app.c:51`, `app_config.c:93-150` | ~5 ms | **1.95 s** | **39 read** × 50 ms (INIT_FLAG 실패면 1 read 후 즉시 반환 — 부분 실패가 최악) |
| `app_lcd_hook_set_pot` | `app.c:52` | <1 ms | 50 ms | 1 write |
| `app_lcd_init_mode` | `app.c:53`, `app_lcd.c:198-252` | ~20 ms | 0.14 s | ~14 write × 10 ms |
| `app_lcd_ensure_run_page` | `app.c:57`, `app_lcd.c:255-273`, `dgus_lcd.h:189-190` | ~10 ms | **2.28 s** | 8 × (RD 120 + set_page 10 + 150) + 최종 RD 120 |
| mon 부팅 로그 5줄 | `app.c:31-72` | ~30 ms | 0.25 s | 5 × 50 ms |
| `app_reg_init`~`app_modbus_init` | `main.c:55-64` | <5 ms | (실패=`Error_Handler`) | |
| `app_eth_init` | `main.c:65`, `spi1.c:22-29` | ~70 ms | ~70 ms | `HAL_Delay(2)+HAL_Delay(60)` 고정 + SPI |
| **합계** | | **≈ 3.3 s** | **≈ 12.0 s** | 전 구간 타임아웃 유계 — 무한대기는 `Error_Handler` 뿐 |

#### 2.2.2 런타임 (슈퍼루프 1회 반복 안에서 발생 가능한 블로킹)

| 구간 | 위치 | 정상 | **최악** | 상한 근거 |
|---|---|---|---|---|
| LCD disp step (write 1) | `app_lcd_disp.c:215-245` | 1.4 ms | 10 ms | 16 B @115200 / TX timeout |
| LCD 아이콘·에러 페이지 (write ≤3) | `app_lcd_disp.c:261,268,307,313` | ≤4 ms | 30 ms | 3 × 10 ms |
| **DATA_SAVE 커밋 = `save_all`** | `app_lcd_comm.c:394`, `app_config.c:48-88` | ~5 ms | **1.90 s** | **38 write** × 50 ms |
| DATA_SAVE 취소 = `app_config_load` | `app_lcd_comm.c:410` | ~5 ms | 1.95 s | 39 read × 50 ms |
| weld `cycle_done` → `save_all` | `app_weld.c:222-224` | ~5 ms | 1.90 s | 동일 |
| Modbus FC06 apply → `save_all` | `app_modbus.c:576-581` | ~5 ms | 1.90 s | 동일 (코드 주석이 이미 "50 ms/call 이 최악을 지배" 명시) |
| RTU 응답 TX | `usart6_mb.c:167-177`, baud 표 `:22` | 60 ms @19200 | **0.53 s** | 105 B @2400 = 481 ms + 50 ms |
| TCP send/disconnect | `app_modbus_tcp.c:112,222` | µs | µs | `SF_IO_NONBLOCK` — SOCK_BUSY 즉시 반환(M9) |
| ADC 2ch 폴링 | `adc1.c:47-52`, `app_reg.c:285/299` | ~10 µs | 4 ms → 그 뒤 `Error_Handler`(=H4) | 2 × 2 ms poll |
| I2C_POT set_amp/set_pot | `i2c_pot.c:14-21` | ~0.1 ms | 50 ms | 1 write |
| `mon_printf` | `mon_usart6.c:24` | ≤11 ms | 50 ms/호출 | 128 B @115200 / timeout 50 |
| `app_eth_tick` (link poll·DHCP_run·reapply) | `app_eth.c:132-231` | µs~ms | ~ms | non-blocking FSM, `DHCP_init`/`socket` 즉시 |

**단일 반복 최악(현실적, FRAM 경로 1개)** = save_all 1.90 + RTU TX 0.53 + LCD 0.03 + mon 2줄 0.10 + POT 0.05 ≈ **2.6 s**.
**병리적 케이스** = LCD DATA_SAVE + weld cycle_done + FC06 이 **같은 iter** 에 겹치고 **동시에 I2C 버스가 죽은** 경우 ≈ 6.3 s → 설계 timeout 초과. 이때는 FRAM 이 이미 죽어 있는 degraded 상태(로드도 전 필드 기본값 폴백)이므로 **리셋 1회를 수용**(§5). 정상 버스에서는 어떤 조합도 ≤ 0.7 s.

### 2.3 부팅 순서가 IWDG 시작 시점에 미치는 영향

부팅 체인 최악 12.0 s 는 어떤 합리적 timeout 보다 길다. IWDG 는 **한 번 켜면 끌 수 없다**(RM0401, `IWDG_KR` 0xCCCC 비가역 — 리셋만이 해제). 따라서 "부팅 초반에 켜고 2 s 로 감시" 는 불가능하고, 선택지는 §3.3 의 4개뿐이다. 부팅 체인 안의 대기는 **전부 타임아웃 유계**(OSC 폴백 900 ms×2, DGUS 4 s, I2C 50 ms/호출, run-page 8회) 이고 유일한 무한대기는 `Error_Handler`(HW init 실패) 다.

### 2.4 HAL IWDG 가용성

| 항목 | 상태 | 위치 |
|---|---|---|
| `stm32f4xx_hal_iwdg.{c,h}` | **vendor 트리에 존재**, HAL v1.8.1 | `fw/vendor/Drivers/STM32F4xx_HAL_Driver/{Src,Inc}/` |
| `HAL_IWDG_MODULE_ENABLED` | **주석 처리(비활성)** — vendor read-only | `fw/vendor/Core/Inc/stm32f4xx_hal_conf.h:60`, include 게이트 `:375` |
| `HAL_SOURCES` 목록 | iwdg.c **미포함** | `fw/CMakeLists.txt:55-78` |
| `LSI_VALUE` | 32000 (conf 기본) | `stm32f4xx_hal_conf.h:113-115` |
| `LSI_STARTUP_TIME` | 40 µs — **stm32f410rx.h 에 정의됨**(`HAL_IWDG_DEFAULT_TIMEOUT` 이 요구) | `stm32f410rx.h:718` |
| `HAL_IWDG_Init` | KR=0xCCCC(enable, **LSI 자동 기동**) → 0x5555 → PR/RLR → SR(PVU\|RVU) 클리어 대기 ≤49 ms(`HAL_GetTick` 기반) → KR=0xAAAA | `stm32f4xx_hal_iwdg.c:160-208` |
| `HAL_IWDG_Refresh` | KR=0xAAAA 1줄 | `:237-244` |
| `__HAL_DBGMCU_FREEZE_IWDG()` | `DBGMCU->APB1FZ \|= DBG_IWDG_STOP` | `stm32f4xx_hal.h:82` |

**결론**: `HSE_VALUE` 선례(`CMakeLists.txt:33`, "vendor conf 기본 오버라이드 — vendor 무편집")와 동일하게 `add_compile_definitions(HAL_IWDG_MODULE_ENABLED)` + `HAL_SOURCES` 에 `stm32f4xx_hal_iwdg.c` 1줄이면 켜진다. `-Wundef` 게이트는 `#ifdef` 게이트라 무관.

### 2.5 LSI 특성과 timeout 범위

- STM32F410 데이터시트(DS10557) "LSI oscillator characteristics" — **f_LSI min 17 / typ 32 / max 47 kHz**(F4 패밀리 공통 보증 대역; 상온·정상 전압에서는 32 kHz ±수 %). 구현자는 데이터시트 표 번호를 spec 커밋 시 재확인할 것 — 검색으로 표의 존재만 확인했고 수치는 F4 공통값이다.
- `T = 4 · 2^PR · (RLR+1) / f_LSI` (PR 코드 0~6 = ÷4~÷256, RLR ≤ 0xFFF).

| 프리스케일러 | 최소(RLR=0) | 최대(RLR=4095) @32k | 최대 @47k | 최대 @17k |
|---|---|---|---|---|
| ÷4 | 0.125 ms | 0.512 s | 0.35 s | 0.96 s |
| ÷64 | 2 ms | 8.19 s | 5.6 s | 15.4 s |
| ÷128 | 4 ms | 16.4 s | 11.2 s | 30.8 s |
| ÷256 | 8 ms | 32.8 s | 22.3 s | 61.7 s |

**후보 2개**(§3.1 결정):

| 후보 | PR / RLR | 공칭 | @47 kHz(최단) | @17 kHz(최장) | 최악 2.6 s 대비 마진 |
|---|---|---|---|---|---|
| **A (권장)** | ÷256 / 624 | **5.00 s** | **3.40 s** | 9.41 s | **31 %** |
| B | ÷128 / 999 | 4.00 s | 2.72 s | 7.53 s | 5 % — 너무 얇다 |

### 2.6 디버그 정합 (DBGMCU)

- IWDG 는 코어 halt 와 무관하게 LSI 로 카운트한다 → gdb 로 멈추면 5 s 뒤 타깃이 리셋돼 세션이 깨진다. `__HAL_DBGMCU_FREEZE_IWDG()`(`DBGMCU->APB1FZ.DBG_IWDG_STOP`) 를 세팅하면 **halt 중에만** 카운터가 멈춘다(디버거 미연결 시 무영향, 실행 중 무영향).
- 이 프로젝트: 런타임 SWD halt 는 **금지**(`CLAUDE.md`, 오진 사유). 그러나 `./fw.sh gdb` 타깃(`fw/openocd/debug.gdb`: `monitor reset halt`/`load`/`break main`/`continue`)이 살아 있고, 플래시(`program … verify reset exit`)·`reset` 타깃은 리셋을 동반하므로 IWDG 와 무관. **비침습 `read_memory`(벤치의 SWD 정적 read) 는 halt 가 아니라 영향 없음.**
- IWDG 는 **소프트웨어 기동**(옵션바이트 `WDG_SW`=1 출하 기본, 이 프로젝트는 옵션바이트를 건드린 적 없음) → `reset halt` 직후엔 IWDG 가 안 돌아 **플래시 락아웃이 원리적으로 없다**. 잘못된 timeout 으로 재부팅 루프에 빠져도 재플래시로 복구된다.

### 2.7 리셋 원인 판별

- `RCC->CSR[31:24]` = LPWRRSTF·WWDGRSTF·**IWDGRSTF**·SFTRSTF·PORRSTF·PINRSTF·BORRSTF·RMVF(`stm32f410rx.h:4815-4842`, HAL `RCC_FLAG_*` `stm32f4xx_hal_rcc.h:357-363`, 클리어 `__HAL_RCC_CLEAR_RESET_FLAGS()` `:1205`).
- **플래그는 POR 또는 RMVF 로만 지워진다** → 매 부팅 읽은 뒤 반드시 클리어해야 "이전 부팅의 IWDG" 가 다음 부팅에 남지 않는다.
- **현재 코드는 CSR 을 어디서도 읽거나 지우지 않는다**(grep `RCC->CSR|__HAL_RCC_GET_FLAG|RSTF|DBGMCU` = 0건; `clock.c` 의 `HAL_RCC_OscConfig` 도 RMVF 미접촉). 즉 지금은 모든 부팅이 POR 로 보인다.
- 기대 관측값(`CSR>>24`, 벤치 V-4 에서 실측 기록): 전원 투입 ≈ `0x0E`(BOR\|PIN\|POR), NRST/openocd reset ≈ `0x04`(PIN), **IWDG ≈ `0x24`**(IWDG\|PIN), `NVIC_SystemReset` ≈ `0x14`(미사용).
- **배너 위치와 가시 조건**: `app.c:29-30` `mon_init(); mon_writeln("[boot] …")`. 이 시점은 `app_modbus_init()`(`main.c:64`) **이전**이라 `apply_config` 의 `mon_set_enabled(false)`(RTU 점유) 가 아직 안 걸려 있다 → **comm_mode 와 무관하게 부팅 배너는 항상 USART6 115200 8N1 로 1회 나간다.** 이후 런타임 mon 은 `comm_mode` 가 ETH_* 일 때만 보인다(RESUME 2026-08-17: RS-485 어댑터 청취 가능, auto-DE).

### 2.8 표면화 경로 후보

| 경로 | 비용 | 계약 영향 | 판단 |
|---|---|---|---|
| **mon `[boot]` 배너에 `rst=0xNN` + `IWDG` 표기** | 3줄 | 없음 | **채택** |
| **파일 static `s_boot_rst`** (SWD 정적 read — `s_clk_hsi_fallback`·`s_dgus_tx_timeout_count` 관례) | 1줄 | 없음 | **채택**(배너와 같은 값) |
| LCD 표시 | VP 신설 = DGUS 자산 변경 | 자산 게이트 | ✗ |
| Modbus STATUS 비트 | 하위 바이트 여유 **`0x80` 1비트**(0x01~0x40 사용, `app_modbus_core.h:140-147`) + 상위 바이트 | `gds_us_hmi`·`gds_us_remote` **계약 변경** — 상의/통보 + 벤치 PASS 후 문서 갱신 규율 | ✗ (지금은) |
| Modbus 레지스터 신설 | **여유 7칸뿐**(0x32~0x38; FC03 상한 57칸, `app_modbus_core.h:9-18` — 초과 시 소비 측 스냅샷 원자성 파괴) | 동일 + 7칸 중 1칸 소모 | ✗ |
| FRAM 리셋 카운터 | FRAM 맵 변경 + write | 없음 | ✗ (YAGNI) |

IWDG 리셋은 소비 측에 이미 **간접 가시**다: TCP 연결 단절(W5500 하드리셋 `spi1.c:22-29`) + 부팅 후 STATUS/명령 레지스터 0-리셋 + LCD 로고 재스플래시 1 s + 부팅 beep. 원인 문자열이 필요해지면 그때 STATUS `0x80` 또는 0x32 를 상의해 넣는다(§7).

### 2.9 legacy 워치독 상태 (이탈 판정 근거)

| MCU | 사실 | 위치 |
|---|---|---|
| SAMD20 | `configure_wdt_on/off()` 정의는 있으나 **호출부가 `/* … */` 블록 안**(`main.c:4891-4899`) → 실행 0회. ASF 기본 = WDT 비활성 | `ref/samd20/main.c:1705-1739, 4895` |
| ATmega16 | 부팅 시 `WDTCR=0x18 → 0x00`(WDTOE\|WDE 후 클리어 = **표준 WDT 해제 시퀀스**) | `ref/atmega16/firmware_analysis.md:60-61` |

→ **두 legacy 모두 워치독 없음.** 이 슬라이스는 legacy 에 없던 거동을 추가하는 **의도적 이탈**이다.

## 3. 결정

### 3.1 timeout = 공칭 5.0 s (÷256, RLR 624)

- 근거: 런타임 단일 반복 최악 2.6 s(§2.2.2) × LSI 최속 47 kHz 보정 → 필요 공칭 ≥ 2.6/0.68 = 3.8 s. 4 s 는 마진 5 % 로 얇고, 5 s 는 31 %. 6 s 이상은 hang 중 출력 ON 창(§4 ②)만 늘린다.
- **hang 시 최장 무응답 = 9.41 s**(LSI 17 kHz). legacy 는 무한이었으므로 개선이며, 그 창 동안 소프트웨어 E-stop(PC11 폴링) 도 죽어 있다는 점은 §4 에 명시.
- 상수는 `main.c` 매크로 2개(`IWDG_PRESCALER_256`, `IWDG_RELOAD 624u`) + `_Static_assert(IWDG_RELOAD <= IWDG_RLR_RL)`. 설정 레이어·런타임 변경 ✗.

### 3.2 kick = `main.c` `while(1)` 안 단일 지점

`while(1){ app_loop_iter(); HAL_IWDG_Refresh(&hiwdg); }` — `main.c:72` TODO 자리 그대로. 블로킹 함수 내부 kick(save_all 등) **금지**: kick 을 흩뿌리면 "루프가 살아 있다"는 의미가 희석되고, 이 슬라이스의 timeout 근거(§2.2.2) 가 무효화된다.

### 3.3 IWDG 기동 시점 = 슈퍼루프 진입 직전 (`main.c:69`, `app_eth_init()` 뒤)

| 안 | 내용 | 장점 | 단점 | 판정 |
|---|---|---|---|---|
| **A** | `while(1)` 직전 기동, timeout 5 s | 코드 최소(2줄), 부팅 체인 12 s 와 무관, 단일 kick 원칙 유지 | 부팅 중 `Error_Handler`(HW init 실패) 는 여전히 영구 정지 | **채택** |
| B | `main()` 첫 줄 기동, timeout > 부팅 최악(≥12 s/0.68 ≈ 18 s → 20 s) | 부팅 hang 도 잡음 | 런타임 hang 검출 20~37 s — 목적 상실 | ✗ |
| C | 첫 줄에 32 s 로 기동 → 루프 직전 5 s 로 재설정(PR/RLR 은 enable 후에도 변경 가능) | A + 부팅 hang 을 ~30 s 재부팅 루프로 전환 | +3줄, 부팅 HW 고장이 "정지" 대신 "30 s 마다 beep+로고 루프" 가 돼 SWD 진단만 번거로워짐 — 실익 없음 | ✗ (YAGNI) |
| D | 첫 줄 기동 + 부팅 블로킹 루프 안에서도 kick | 부팅 hang 일부 커버 | 다중 kick — 원칙 위반, 블로킹 루프 4곳 수정 | ✗ |

A 의 잔여 리스크(부팅 중 무한대기) 는 §2.3 대로 `Error_Handler` 뿐이며, 그것은 오늘도 같은 거동이다.

### 3.4 리셋 원인 표면 = mon 배너 + SWD static (Modbus ✗)

`app.c:30` 배너를 `[boot] gds_us_ctrl ready rst=0x%02X%s` 로 바꾸고 `%s` 에 IWDGRSTF 면 `" IWDG"` 를 붙인다. 읽기 → `__HAL_RCC_CLEAR_RESET_FLAGS()` 순서 고정(§2.7). 값은 `static uint8_t s_boot_rst` 에 보관(SWD 정적 read). "stage-b" 잔재 문구는 이 줄을 건드리는 김에 제거.

### 3.5 DBGMCU freeze = 1줄 채택

`__HAL_DBGMCU_FREEZE_IWDG();` 를 IWDG 기동 직전에. `./fw.sh gdb` 흐름을 지키는 비용 0 의 보험. 실행 중·미연결 시 무영향.

### 3.6 빌드 플래그 = 없음

`REMOTE_EN_GATE_BYPASS` 는 **PC8 실장 PCB 부재**라는 실제 차단 요인이 있어 만든 한시 탈출구다(`CMakeLists.txt:42-51`). IWDG 에는 그런 차단 요인이 없다: gdb 는 freeze 가 덮고, 플래시는 락아웃이 없고(§2.6), 벤치 트레이스 빌드의 mon 폭주도 50 ms/줄 × 수 줄 ≪ 3.4 s. 필요가 생기면 그때 `#ifndef IWDG_DISABLE` 3줄 — 지금은 YAGNI.

### 3.7 HAL vs 직접 레지스터 = HAL 채택

| | HAL (`HAL_IWDG_Init/Refresh`) | 직접 레지스터 (`IWDG->KR/PR/RLR` 5줄) |
|---|---|---|
| 건드리는 파일 | CMake +2줄, periph +2줄, main.c | main.c 만 |
| 정확성 | ST 시퀀스 그대로(enable→write access→PR/RLR→PVU/RVU 대기→reload) | 순서·키를 직접 맞춰야 하고 틀리면 **조용히 워치독 부재**(벤치 V-3 에서만 드러남) |
| 판정 | **채택** — "HAL 이 해주는 건 HAL 에" + `main.c:72` TODO 가 이미 `HAL_IWDG_Refresh(&hiwdg)` 를 예약 | 대안. 사용자가 CMake/periph 무접촉을 원하면 전환 가능 |

`HAL_IWDG_Init` 반환값은 `(void)`. 실패(`HAL_TIMEOUT` = SR 플래그 49 ms 내 미클리어 = LSI 무응답) 시점엔 이미 enable 이 끝나 있어 할 수 있는 조치가 없고, LSI 가 죽었다면 IWDG 도 안 돌아 보드는 그냥 동작한다(§5).

## 4. 거동 변화 / legacy 이탈 (사용자 컨펌 대상)

| # | 변화 | 이전(legacy = 현재 코드) | 이후 |
|---|---|---|---|
| ① | **슈퍼루프 hang** | 영구 정지(전원 재투입 필요) | **3.4~9.4 s(공칭 5 s) 내 자동 리셋** → 정상 부팅 |
| ② | hang 중 **초음파 출력 ON 상태** | 무한 지속 | 최장 9.4 s 후 리셋 — 리셋 시 GPIO 전부 Hi-Z → OSC 3선 open-drain 외부 풀업 HIGH = **OFF**(`board.c:4-8`), SOL_DN PB5 Hi-Z(회로상 idle 확인 = 벤치 V-3b). ⚠ 그 창 동안 **소프트웨어 E-stop(PC11 폴링) 도 죽어 있다** — legacy 와 동일한 한계, 창 길이만 유한화 |
| ③ | `Error_Handler`/HardFault 계열 | 영구 정지(H4 "ADC 영구 lock" 포함) | **≤9.4 s 마다 재부팅 루프**. 원인은 배너 `IWDG` 로만 보이고 fault 종류는 남지 않음(§7) |
| ④ | 부팅 배너 | `[boot] gds_us_ctrl stage-b ready` | `[boot] gds_us_ctrl ready rst=0xNN[ IWDG]` |
| ⑤ | IWDG 리셋 후 부팅 | (해당 없음) | POR 과 동일 경로. 단 OSC 보드는 전원 유지라 PB12 펄스 없음 → WAIT_H 900 ms 폴백 후 RESET 40/SEEK 20 ms 펄스 재송출(NRST 리셋과 같은 거동). W5500 하드리셋 → TCP 클라이언트 단절(원격기 재접속 로직 의존, 앱 유휴 12 s 와 별개). STATUS/명령/`REMOTE_EN` 0-리셋(PC8 레벨 스위치라 자동 재평가) |
| ⑥ | `save_all` 도중 리셋 | (해당 없음) | FRAM 맵 부분 갱신(전원 차단과 동일 클래스, CRC 없음) — 정상 버스에서는 save_all 5 ms 라 확률 극소, 죽은 버스에서는 어차피 write 실패 |
| ⑦ | 부팅 시간 | — | `HAL_IWDG_Init` PVU/RVU 대기 ≤49 ms 추가(정상 <1 ms) |

**타이밍 영향 없음**: `HAL_IWDG_Refresh` = 레지스터 write 1회/iter. 560 ms ceiling 등 기존 실측에 영향 없음(벤치 V-6 로 확인).

## 5. 에러 / 실패 semantics

| 상황 | 거동 |
|---|---|
| `HAL_IWDG_Init` = `HAL_TIMEOUT` | enable 은 이미 됨. 첫 주기는 리셋값(RLR 0xFFF·PR 0 → 0.5 s) 로 시작할 수 있으나 루프 첫 kick(수 ms 내) 이 새 RLR 로 reload → 무해. 로그 ✗ |
| LSI 자체 고장 | IWDG 카운트 정지 = 워치독 부재, 보드는 정상 동작(감지 불가 — 수용) |
| 병리적 3중 save_all + 죽은 I2C 버스(§2.2.2) | 리셋 1회 → 부팅 로드 실패 → 전 필드 기본값(fram-robust 슬라이스 거동) — degraded 상태의 리셋으로 수용 |
| 부팅 중 `Error_Handler` | 변화 없음(영구 정지) — §3.3 A 의 잔여 |
| gdb halt | freeze 로 카운터 정지 — 리셋 없음(V-5) |
| 플래시 중 | `reset halt` 상태에서 IWDG 미기동 — 락아웃 없음 |

## 6. 테스트

### 6.1 host

**없음.** 순수 로직이 없다(레지스터 설정 2값 + kick 1줄). 억지 스위트를 만들지 않는다. 컴파일 타임 체크 1개만: `_Static_assert(IWDG_RELOAD <= IWDG_RLR_RL, "IWDG reload > 12-bit")`. 기존 16스위트 무회귀 + 우리 코드 0-warning 이 게이트.

### 6.2 HW 벤치 (구현 세션 후, 보드 확보 시)

리셋 지표 = **부팅 beep(100 ms) + LCD 로고 재스플래시(1 s)** — mon 이 안 보이는 RTU 모드에서도 관측 가능. mon 은 ETH_* 모드에서 RS-485 어댑터 115200 8N1 청취(부팅 배너는 모든 모드에서 1회 나감, §2.7).

| # | 항목 | 절차 | 판정 |
|---|---|---|---|
| V-1 | 정상 부팅 무회귀 | 전원 투입 | 배너 `rst=0x0E`(IWDG 없음), LCD run 페이지, 이후 1분간 beep/로고 재출현 0회 |
| V-2 | 장기 무오작동 (false-trip 없음) | ① ETH_STATIC + mbpoll FC03 50칸 연속 폴링 + LCD DATA_SAVE 3회 + START/STOP 3회 ≥10분 ② **RTU 2400 baud** 로 전환(LCD) 후 FC03 50칸 연속 폴링 + DATA_SAVE 3회 ≥10분(최장 TX 스톨 481 ms 조합) | 두 구간 모두 beep/로고 재출현 0회, ②는 종료 후 LCD 에서 원래 baud 복원 |
| V-3 | **의도적 hang → 리셋** | **throwaway 빌드(커밋 금지)**: `app_loop_iter()` 말미에 `if (sys_tick_get_ms() > 20000u) { mon_writeln("[wd] hang"); __disable_irq(); for(;;){} }` 추가 → 플래시 → 전원 투입 | 20 s 시점부터 **3.4~9.4 s 내**(공칭 5 s) beep+로고 → 배너 `rst=0x24 IWDG`. `[wd] hang`~`[boot]` 간격을 타임스탬프 캡처로 재어 **실측 LSI 를 이 spec 에 기록**. 20 s 마다 반복되는 재부팅 루프 = 정상(throwaway 제거로 종료) |
| V-3b | hang 시 출력 fail-safe | V-3 변형: hang 직전에 `app_reg_command(US_CMD_START, US_COMM)` 호출 → 출력 ON 상태로 hang | 리셋 순간 PB14(RUN, active-LOW) HIGH 복귀 + ICON_RUN 소등 + STATUS 0. ⚠ **안전**: OSC 보드 부하(혼) 미연결 또는 무부하 상태에서만. hang 창(≤9.4 s) 동안 E-stop 소프트웨어 경로가 죽어 있으므로 **전원 스위치를 손에 두고** 진행 |
| V-4 | 플래그 클리어 검증 | V-3 뒤 ① 전원 재투입 ② openocd `reset` | ① `rst=0x0E`(IWDG 잔류 없음 = RMVF 동작) ② `rst=0x04`. 실측값이 기대와 다르면 **값만 spec 에 기록**(비트 조합은 실리콘 관측치가 정본) |
| V-5 (선택) | DBGMCU freeze | `./fw.sh gdb` → `break main` 통과 후 임의 지점 `Ctrl-C` → 30 s 대기 → `continue` | 리셋 없음(배너 재출현 ✗). ⚠ SWD halt 금지 규칙의 **1회 예외** — 이 항목은 "리셋 안 남" 만 보고, halt 중 sys_tick 정지로 인한 표시/FSM 오동작은 판정에서 제외 |
| V-6 | 타이밍 무회귀 | mbpoll START → STATUS `1×N→0` | ceiling 실측 [514,617] ms 대역 유지 |

## 7. 범위 밖 (이연)

- **fault 원인 덤프**(HardFault 레지스터, `Error_Handler` 호출처) — `irq.c:10` TODO 그대로. noinit RAM 에 코드 1워드를 남겨 배너에 찍는 후속이 자연스러우나 지금은 ✗.
- **Modbus 표면**(STATUS `0x80` 또는 레지스터 0x32 "last reset cause") — HMI/원격기가 요구할 때 `gds_us_remote` 와 상의 후. 7칸 제약 명시.
- **sys_tick(TIM11) 정지 감시** — 슈퍼루프가 tick 없이 돌면 kick 은 계속돼 IWDG 가 못 잡는다(모든 10 ms 게이트 정지 = 표시/제어 동결). 별개 결함 클래스, IWDG 목적 밖.
- **WWDG**(윈도 워치독) — 조기 kick 검출은 이 슈퍼루프에 무의미.
- **하드웨어 워치독(옵션바이트 `WDG_SW`=0)** — 부팅 12 s 에 즉사 + 플래시 락아웃 리스크. 금지.
- **부팅 hang 커버(§3.3 C)** — 실익 없어 ✗.

## 8. 구현 단위 요약 (plan 입력)

| 파일 | 변경 | 줄 수(≈) |
|---|---|---|
| `fw/CMakeLists.txt` | `add_compile_definitions` 에 `HAL_IWDG_MODULE_ENABLED` + `HAL_SOURCES` 에 `stm32f4xx_hal_iwdg.c` | 2 |
| `fw/src/periph.c`, `fw/include/periph.h` | `IWDG_HandleTypeDef hiwdg;` 정의/extern (HAL 핸들 단일 정의 규율) | 2 |
| `fw/src/main.c` | `:34` TODO 삭제 → `:69` 기동 블록(매크로 2 + `_Static_assert` + `hiwdg.Init` 3줄 + freeze + `HAL_IWDG_Init`) / `:72` TODO → `HAL_IWDG_Refresh(&hiwdg);` | ~10 |
| `fw/src/app.c` | `:30` 배너 확장(CSR 읽기 → 클리어 → printf) + `static uint8_t s_boot_rst` | ~5 |
| `docs/requirements.md:92` | FW3-6 워치독 항목에 ✅ + spec 링크 | 1 |
| `docs/changelog.md` | 항목 1개 | ~5 |

⚠ 신규 `.c` 없음 → `file(GLOB)` 재구성 불요. 단 `HAL_SOURCES` 변경은 **CMake 재구성 필요**(`./fw.sh reconfig`) — 스탬프는 `src/*.c drivers/*.c` 만 보므로 자동 감지 안 됨.

## 9. 열린 결정 (사용자 컨펌)

| # | 결정 | 선택지 | 권장 |
|---|---|---|---|
| Q1 | timeout | **A 5 s**(3.4~9.4) / B 4 s(2.7~7.5, 마진 5 %) / C 6 s(4.1~11.3) | **A** — false-trip 마진 31 %, hang 창 ≤9.4 s |
| Q2 | 구현 방식 | **HAL**(CMake+periph 접촉) / 직접 레지스터(main.c 5줄) | **HAL** — 조용한 오설정 리스크 제거 |
| Q3 | 리셋 원인 표면 | **mon+SWD static 만** / + STATUS `0x80` / + 레지스터 0x32 | **mon+static** — 계약 불변, 7칸 보존 |
| Q4 | 빌드 플래그 | **없음** / `IWDG_DISABLE` 옵션 | **없음** — 차단 요인 부재, YAGNI |
| Q5 | legacy 이탈 §4 ①~③ 수용 | 수용 / 보류 | **수용** — 요구사항 FW3-6 이 곧 이탈 지시 |
