# IWDG 워치독 슬라이스 — 실행 계획

> **문서 요약**: 승인된 설계 spec(`docs/superpowers/specs/2026-09-04-iwdg-watchdog-design.md`, 이하 "spec")을 코드 커밋 2개 + HW 벤치 1개로 분해한다. **T-1** = 리셋 원인 배너(`app.c` — `RCC->CSR` 읽기→클리어→`[boot] … rst=0xNN[ IWDG]` + SWD 용 static, 거동 무변화), **T-2** = IWDG 기동·kick·DBGMCU freeze(`CMakeLists.txt` HAL 모듈 주입 2줄 + `periph.{c,h}` 핸들 + `main.c` TODO 2곳 치환, 최초의 거동 변화) + 문서 동승(requirements FW3-6·changelog), **T-3** = HW 벤치 V-1~V-6(throwaway hang 빌드, 보드 게이트, 코드 커밋 없음). T-1 을 먼저 두는 이유 = T-3 의 판정(`rst=0x24 IWDG`)이 배너에 의존한다. host 스위트 신설 없음(순수 로직 부재 — spec §6.1), 게이트 = 우리 코드 0-warning + 기존 16스위트 무회귀 + self-review 체크리스트. 함정 절에 **`HAL_SOURCES` 변경은 `fw.sh` 스탬프가 감지 못 해 `reconfig` 필수**, vendor conf 무편집, CSR 읽기/클리어 순서, hang 주입 코드 커밋 금지, SWD halt 예외 1건을 명시한다.

**작성일**: 2026-09-04 / **정본 spec**: 위 링크 / **브랜치**: `feat/iwdg-watchdog` (base = 현 `feat/remote-status-bits` tip `34a5d43` 또는 main — 다른 작업 비의존이라 어느 쪽이든 충돌 0. 원격 스택 벤치 전에 main 에 먼저 넣고 싶으면 main 기반으로)

---

## 0. 공통 전제

### 0.1 검증 명령 (전 Task 공용)

```sh
./fw.sh reconfig   # T-2 는 HAL_SOURCES 가 바뀌므로 반드시 reconfig (§함정 1)
./fw.sh            # 이후 증분
./fw.sh test       # host 16 스위트 무회귀 (= make -C fw/test)
```

- "0-warning" = 우리 코드(`src/`, `drivers/`, `include/`)에서 `-Wall -Wextra -Wundef -Wshadow` 경고 0(`fw/CMakeLists.txt:19`). vendor HAL 은 `-Wno-unused-parameter` 로 격리돼 있어 `stm32f4xx_hal_iwdg.c` 추가로 경고가 새면 그것은 우리 몫이 아니다 — 단 **에러**는 막아야 한다.
- host "pass" = 16 스위트 전부 `all tests passed` + make exit 0. 이 슬라이스는 host 코드를 건드리지 않으므로 **결과가 바뀌면 그 자체가 이상 신호**.
- 보드 불요(T-1·T-2). T-3 만 보드.

### 0.2 무변경 파일 (게이트 원칙)

| 파일 | 근거 |
|---|---|
| `fw/vendor/**` | read-only. `stm32f4xx_hal_conf.h:60` 의 주석 처리된 `HAL_IWDG_MODULE_ENABLED` 를 **풀지 말 것** — CMake 주입으로 켠다(`HSE_VALUE` 선례 `CMakeLists.txt:33`) |
| `fw/src/irq.c` | `Error_Handler`/fault 핸들러 무변경 — IWDG 가 `while(1)` 을 외부에서 끊는 것이 설계(spec §1) |
| `fw/src/app_config.c`, `fw/drivers/i2c1.c`, `fw/drivers/usart6_mb.c` | 블로킹 구간 안에 kick 을 넣지 않는다(spec §3.2 단일 kick) |
| `fw/include/app_modbus_core.h`, `fw/src/app_modbus*.c` | 레지스터/STATUS 계약 불변(spec §3.4) |
| `fw/test/**` | 신설 스위트 없음 |
| `fw/src/clock.c` | RCC 설정 무변경 — LSI 는 IWDG enable 이 자동 기동 |

### 0.3 Task 간 의존

```
T-1 (배너, 거동 무변화) → T-2 (IWDG 기동, 거동 변화) → T-3 (HW 벤치)
```

T-1/T-2 를 한 커밋으로 합치지 말 것 — T-2 가 최초 거동 변화라 회귀 시 이등분 지점이 된다.

---

## T-1 — 리셋 원인 배너 (`app.c`)

### 목표

`RCC->CSR` 리셋 플래그를 부팅 배너 1줄로 표면화하고 SWD 정적 read 용 static 에 보관한다. 거동 변화 없음(로그 문구만).

### 변경 파일

| 파일 | 위치 | 변경 |
|---|---|---|
| `fw/src/app.c` | `:20` 근처 파일 static 영역 | `static uint8_t s_boot_rst;   /* RCC->CSR[31:24] @boot — SWD 정적 read 진단용 (IWDG=0x20 비트) */` |
| `fw/src/app.c` | `:29-30` (`mon_init();` + `mon_writeln("[boot] gds_us_ctrl stage-b ready");`) | 아래 블록으로 치환 |

### 구현 지시

```c
    mon_init();
    /* 리셋 원인 — CSR 플래그는 POR 또는 RMVF 로만 지워지므로 읽은 뒤 즉시 클리어
     * (안 지우면 이전 부팅의 IWDG 플래그가 다음 부팅에 남는다). 기대값:
     * 전원 0x0E(BOR|PIN|POR) / NRST 0x04 / IWDG 0x24(IWDG|PIN). spec §2.7 */
    s_boot_rst = (uint8_t)(RCC->CSR >> 24);
    __HAL_RCC_CLEAR_RESET_FLAGS();
    mon_printf("[boot] gds_us_ctrl ready rst=0x%02X%s\r\n", (unsigned)s_boot_rst,
               (s_boot_rst & (uint8_t)(RCC_CSR_IWDGRSTF >> 24)) ? " IWDG" : "");
```

- `app.c` 는 이미 `stm32f4xx_hal.h` 를 include(`:2`) → `RCC`, `RCC_CSR_IWDGRSTF`, `__HAL_RCC_CLEAR_RESET_FLAGS` 전부 가용. 신규 include 없음.
- **순서 규약**: 읽기 → 클리어 → 출력. 클리어를 먼저 하면 값이 사라진다.
- 이 시점은 `app_modbus_init()`(`main.c:64`) 이전이라 `mon_set_enabled(false)` 가 아직 안 걸려 있다 → 배너는 comm_mode 무관하게 USART6 115200 로 나간다(spec §2.7). 이 사실을 바꾸지 말 것(배너를 뒤로 옮기지 말 것).
- "stage-b" 잔재 문구는 이 줄을 건드리는 김에 제거(부수 리팩토링 아님 — 같은 줄).

### 완료 조건

- [ ] `static uint8_t s_boot_rst` 1개 외 신규 전역 없음
- [ ] 읽기→클리어→출력 순서
- [ ] 우리 코드 0-warning(`-Wshadow`: 지역변수 이름이 static 과 충돌하지 않게 — 위 코드는 지역변수 없음) + host 16 무회귀
- [ ] 리뷰 게이트: self-review 3항목(순서·include 무추가·배너 위치 불변). 별도 리뷰어 불요(5줄)

---

## T-2 — IWDG 기동 · kick · DBGMCU freeze + 문서 동승

### 목표

슈퍼루프 진입 직전 IWDG 를 공칭 5 s 로 기동하고 루프 말미에서 kick 한다. 최초의 거동 변화(spec §4).

### 변경 파일

| 파일 | 위치 | 변경 |
|---|---|---|
| `fw/CMakeLists.txt` | `:30-34` `add_compile_definitions(...)` | `HAL_IWDG_MODULE_ENABLED` 1줄 추가(주석: vendor conf `:60` 주석 처리분을 CMake 로 켬 — vendor 무편집, `HSE_VALUE` 와 동일 패턴) |
| `fw/CMakeLists.txt` | `:77` (`stm32f4xx_hal_spi.c` 다음, `)` 앞) | `# IWDG 워치독:` + `${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_iwdg.c` |
| `fw/src/periph.c` | 말미(`:11`) | `IWDG_HandleTypeDef hiwdg;   /* IWDG — 슈퍼루프 워치독 (main.c 기동/kick) */` |
| `fw/include/periph.h` | 말미(`:12`) | `extern IWDG_HandleTypeDef hiwdg;` |
| `fw/src/main.c` | `:34` `/* TODO Stage A: iwdg_init(2000); */` | **삭제** |
| `fw/src/main.c` | `:28` 매크로 영역 | 상수 2개 + `_Static_assert` (아래) |
| `fw/src/main.c` | `:69` (`app_eth_init();` 블록 뒤, `while (1)` 앞) | 기동 블록 (아래) |
| `fw/src/main.c` | `:72` `/* TODO Stage A: HAL_IWDG_Refresh(&hiwdg); */` | `HAL_IWDG_Refresh(&hiwdg);` 로 치환 |
| `docs/requirements.md` | `:92` | FW3-6 "워치독" 에 ✅ + spec 경로 |
| `docs/changelog.md` | `[Unreleased]` 최상단 | 항목 1개(아래 문안) |

### 구현 지시

**`main.c` 상수** (`:28` `BOOT_BEEP_MS` 아래):

```c
/* IWDG — 슈퍼루프 워치독. LSI 32 kHz 공칭, 데이터시트 17~47 kHz →
 * 256·(624+1)/f_LSI = 공칭 5.0 s, 실범위 3.4~9.4 s. 런타임 단일 반복 최악 2.6 s
 * (FRAM 버스 사망 시 save_all 38×50 ms + RTU TX @2400)에 31 % 마진.
 * 기동은 슈퍼루프 진입 직전 — 부팅 체인(최악 12 s)은 감시 밖(전 구간 타임아웃 유계).
 * spec docs/superpowers/specs/2026-09-04-iwdg-watchdog-design.md §3 */
#define IWDG_PRESC    IWDG_PRESCALER_256
#define IWDG_RELOAD   624u
_Static_assert(IWDG_RELOAD <= IWDG_RLR_RL, "IWDG reload exceeds 12-bit RLR");
```

**`main.c` 기동 블록** (`app_eth_init();` 주석 블록 뒤, `while (1)` 앞):

```c
    /* 워치독 기동 — 여기서부터 매 iter kick. 한 번 켜면 리셋 외 해제 불가(RM0401).
     * freeze = gdb halt 중 카운터 정지(./fw.sh gdb 보호; 실행 중·미연결 시 무영향).
     * Init 실패(HAL_TIMEOUT = LSI 무응답)는 조치 불가 — enable 은 이미 끝났고, LSI 가
     * 죽었다면 IWDG 도 안 돈다(spec §5). */
    __HAL_DBGMCU_FREEZE_IWDG();
    hiwdg.Instance       = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESC;
    hiwdg.Init.Reload    = IWDG_RELOAD;
    (void)HAL_IWDG_Init(&hiwdg);
```

**`main.c` 루프**:

```c
    while (1) {
        app_loop_iter();
        HAL_IWDG_Refresh(&hiwdg);   /* 단일 kick 지점 — 블로킹 함수 내부 kick 금지 (spec §3.2) */
    }
```

- `main.c` 는 `stm32f4xx_hal.h`(`:2`) + `periph.h` 를 이미 include 하나? → **`periph.h` 는 현재 main.c 에 없다**(`:2-22` 확인). `#include "periph.h"` 1줄 추가(`hiwdg` extern). `IWDG_PRESCALER_256`·`IWDG_RLR_RL`·`__HAL_DBGMCU_FREEZE_IWDG` 는 `stm32f4xx_hal.h` 경유로 들어온다(모듈 enable 시 conf `:375-377` 이 `stm32f4xx_hal_iwdg.h` 를 include).
- `_Static_assert` 는 C11(`CMAKE_C_STANDARD 11`, `:10`) 가용. `IWDG_RLR_RL` = `0xFFF`(`stm32f410rx.h:4102`).
- **HAL vs 직접 레지스터**: spec §3.7 = HAL 채택. 사용자가 Q2 에서 직접 레지스터를 고르면 CMake/periph 변경을 빼고 main.c 에 `IWDG->KR=0xCCCCu; IWDG->KR=0x5555u; IWDG->PR=IWDG_PR_PR_2|IWDG_PR_PR_1; IWDG->RLR=IWDG_RELOAD; IWDG->KR=0xAAAAu;` / kick `IWDG->KR=0xAAAAu;` — **순서(enable 먼저)를 HAL 과 동일하게** 유지할 것.

**changelog 문안(초안)**:

```
### 2026-09-XX — IWDG 워치독 (요구사항 FW3-6, 감사 D3 잔여)

- **`<hash>` feat(wd) 리셋 원인 배너**: `[boot] … rst=0xNN[ IWDG]` — `RCC->CSR` 읽기→RMVF 클리어. SWD 정적 read 용 `s_boot_rst`. 거동 무변화.
- **`<hash>` feat(wd) IWDG 기동+kick**: 슈퍼루프 진입 직전 공칭 5 s(÷256/624, LSI 편차 3.4~9.4 s), `while(1)` 단일 kick, DBGMCU freeze. HAL 모듈은 CMake 주입(`HAL_IWDG_MODULE_ENABLED`, vendor 무편집). **의도적 legacy 이탈**(samd20·M16 모두 WDT 비활성): hang → ≤9.4 s 자동 리셋, `Error_Handler`/HardFault → 재부팅 루프(H4 영구 lock 해소). Modbus 계약 불변. HW 벤치 = spec §6.2 V-1~V-6 (대기).
```

### 완료 조건

- [ ] `./fw.sh reconfig` 후 링크 성공(`HAL_IWDG_Init`/`HAL_IWDG_Refresh` undefined reference 없음 = HAL_SOURCES 반영 증거)
- [ ] 우리 코드 0-warning + host 16 무회귀 + FLASH/RAM 사용량 기록(예상 +≈300 B FLASH)
- [ ] `main.c` 에 kick 1곳, 기동 1곳, TODO 주석 0개
- [ ] `fw/vendor/**` diff 0 (`git status` 로 확인)
- [ ] `irq.c`·`app_config.c`·`i2c1.c`·`usart6_mb.c`·`app_modbus*` diff 0
- [ ] 리뷰 게이트: **`/code-review` 1회**(HAL 시퀀스·include·freeze 위치·상수 산술 재계산: 256×625/32000 = 5.000). 지적 중 CRITICAL/HIGH 만 반영, 그 외는 spec 결정 우선

---

## T-3 — HW 벤치 (보드 게이트, 코드 커밋 없음)

### 목표

spec §6.2 V-1~V-6 실행. 결과(실측 LSI, CSR 관측값)를 spec §2.5/§2.7 에 기록하는 docs 커밋 1개.

### 절차 요약 (상세 판정 = spec §6.2 표)

1. **V-1** 전원 투입 → 배너 `rst=0x0E`, 1분 무재부팅.
2. **V-2** false-trip 없음: ETH_STATIC + mbpoll 연속 + DATA_SAVE + START/STOP ≥10분 / **RTU 2400** 전환 후 동일 ≥10분. 종료 후 baud 복원.
3. **V-3** throwaway 빌드 — `app_loop_iter()` 말미(`app.c:182` `app_buzzer_tick();` 뒤)에 아래를 **로컬 편집만**(커밋 금지, 완료 후 `git checkout fw/src/app.c`):
   ```c
   /* THROWAWAY — IWDG V-3 hang 주입. 커밋 금지. */
   if (sys_tick_get_ms() > 20000u) { mon_writeln("[wd] hang"); __disable_irq(); for (;;) {} }
   ```
   타임스탬프 캡처(예: `python -c` 로 pyserial 라인별 `time.monotonic()` 기록 — `sleep` 차단 환경 주의)로 `[wd] hang`→`[boot]` 간격 = 실측 timeout → `f_LSI ≈ 256×625/T`. 배너 `rst=0x24 IWDG`.
4. **V-3b** 위 블록 앞에 `app_reg_command(US_CMD_START, (uint8_t)US_COMM);` 1줄 추가(`app.c` 는 `app_reg.h` include 있음) → 출력 ON 상태 hang → 리셋 시 PB14 HIGH/ICON_RUN 소등/STATUS 0. ⚠ **혼 무부하 + 전원 스위치 대기**(spec §4 ②).
5. **V-4** 전원 재투입 `0x0E` / openocd `./fw.sh reset` `0x04`. 관측값이 다르면 spec §2.7 기대값을 **관측값으로 교체**.
6. **V-5 (선택)** `./fw.sh gdb` halt 30 s → continue → 무리셋. SWD halt 금지의 1회 예외(spec §6.2 명시).
7. **V-6** START→STATUS ceiling [514,617] ms 유지.

### 완료 조건

- [ ] V-1~V-4·V-6 PASS(V-5 선택)
- [ ] throwaway 편집 원복 확인(`git diff` 0)
- [ ] docs 커밋: spec §2.5 실측 LSI·§2.7 CSR 관측값·§6.2 결과 / `docs/changelog.md` 벤치 PASS 표기 / `docs/NEXT_STEPS.md §1.2` IWDG 항목 ✅
- [ ] 머지 = `--no-ff`, 태그 `hw-revA_fw-stage-iwdg`(기존 `hw-revA_fw-stage-*` 관례)

---

## 커밋 분할 (순서 고정)

| Task | subject |
|---|---|
| T-1 | `feat(wd): 부팅 배너에 리셋 원인 — RCC->CSR 읽기·클리어 + SWD static` |
| T-2 | `feat(wd): IWDG 워치독 기동+kick — 공칭 5s, 슈퍼루프 단일 kick, DBGMCU freeze (legacy 이탈, 사용자 승인)` |
| T-3 | `docs(wd): IWDG HW 벤치 결과 — 실측 LSI·CSR 관측값` |

각 body 에 spec § 참조 + 게이트 통과 사실(0-warning / host 16 / reconfig 링크). T-2 body 에 **거동 변화 ①~③ 사용자 승인** 문구 필수(커밋 `0ab2608` 관례).

---

## 함정 (구현자 필독)

1. **`HAL_SOURCES` 변경은 `fw.sh` 가 감지 못 한다** — 스탬프(`build/.src-glob`)는 `src/*.c drivers/*.c` 만 본다. T-2 후 반드시 `./fw.sh reconfig`. 안 하면 `HAL_IWDG_Init` undefined reference. `build-remote/` 도 쓰면 `MODEL=remote ./fw.sh reconfig` 별도.
2. **vendor conf 를 열지 말 것** — `stm32f4xx_hal_conf.h:60` 주석을 푸는 게 "더 쉬워 보여도" read-only 규율 위반. CMake `add_compile_definitions` 만.
3. **CSR 읽기 → 클리어 순서** — 뒤집으면 항상 0x00. 그리고 클리어를 빼먹으면 IWDG 리셋 뒤 전원 재투입에도 `IWDG` 가 찍힌다(V-4 가 잡는다).
4. **kick 은 `main.c` 한 곳** — `save_all`/RTU TX 안에 "안전하게" kick 을 넣고 싶어지면 spec §3.2 를 다시 읽을 것. timeout 근거가 무너진다.
5. **`_Static_assert` 위치** — 함수 밖 파일 스코프. `-Wundef` 는 `#if` 값 평가에만 걸리므로 무관.
6. **hang 주입 코드는 절대 커밋 금지** — T-3 은 로컬 편집 + `git checkout` 원복. 리뷰어는 `app.c` diff 에 `for (;;)` 가 있으면 즉시 반려.
7. **SWD halt 금지의 예외는 V-5 하나** — 그것도 "리셋 안 남" 만 판정. halt 중 LCD/FSM 이상은 sys_tick 정지 아티팩트(`feedback_swd_halt_breaks_board_validation`).
8. **IWDG 는 끌 수 없다** — T-3 에서 재부팅 루프에 빠진 보드는 `./fw.sh flash` 로 정상 빌드를 올리면 끝(리셋 halt 중 IWDG 미기동, 락아웃 없음). 옵션바이트 `WDG_SW` 는 건드리지 말 것(하드웨어 워치독 = 부팅 12 s 에 즉사).
9. **첫 주기 quirk** — `HAL_IWDG_Init` 이 `HAL_TIMEOUT` 을 내면 첫 주기가 리셋값(0.5 s)일 수 있으나 루프 첫 kick 이 수 ms 내 새 RLR 로 reload → 무해. 로그 추가하지 말 것(main.c 에 mon include 늘리지 않기).
10. **원격기/HMI 통보** — 계약 변경은 없지만 "IWDG 리셋 = TCP 단절 + STATUS 0-리셋 + `REMOTE_EN` 재평가" 는 `gds_us_remote` 에 **상의 아닌 통보**(`feedback_cross_session_protocol`), `gds_us_hmi` 에도 통보. 벤치 PASS 후.
