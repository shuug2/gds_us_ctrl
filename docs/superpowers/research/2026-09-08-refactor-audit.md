# gds_us_ctrl 리팩토링 사전 감사 — 바이트 동일성 전제·spike·인벤토리 (2026-09-08)

## 요약

STD/REMOTE 빌드는 **클린 재빌드 간 `.bin`·`.elf` 모두 바이트 재현**된다(sha256 일치, 타임스탬프·경로 삽입 없음). 라인 이동만으로 바이너리가 바뀌는 지점은 **없다**(`__LINE__`/`__FILE__`/`assert` 미사용, `_Static_assert` 2건은 컴파일 타임 전용) — 빈 줄 3개 삽입 실험에서 `.bin` 동일·`.elf`만 변화(DWARF 라인 테이블). 그러나 빌드가 **`-Og`** 이고 `-Og` 는 `-finline-functions-called-once` 가 **비활성**이라, 긴 함수에서 `static` 헬퍼를 뽑으면 **실제 call 이 생겨 바이너리가 바뀐다**(b-1/b-2/b-3 전부 ×). `static inline __attribute__((always_inline))` 로 바꾸어도 헬퍼가 **값을 반환하거나 로컬 변수를 포인터로 받으면** 여전히 × (`movs r3,#1` 물질화·블록 재배열·스택 스필). 바이트 동일이 실측된 유일한 패턴은 **"판정(if/case 라벨)은 호출자에 남기고, 부수효과만 있는 직선 블록을 `void always_inline` 헬퍼로 뽑되 인자는 이미 존재하는 포인터/값만"** (b-2v·b-3b 두 건 ○). 따라서 **else-if 체인 분할·switch case 재편성은 바이트 동일 불가** → `app_modbus_apply_writes`(358줄)·`app_lcd_input_dispatch`(225줄)의 "50줄 이하" 목표는 구조적으로 HW 벤치(또는 host 테스트) 게이트가 필요하다. 50줄 초과 함수는 **38개**(`src/`+`drivers/`, 총 301개 중)인데 그중 **21개는 주석을 빼면 ≤50 코드 줄**이라 주석 재배치만으로(바이너리 무영향) 통과한다. 800줄 초과 파일은 `app_modbus.c`(815) 하나뿐이고 주석 311줄 중 16줄만 옮겨도 800 아래로 내려간다 — **파일 분할은 불필요**. 권고 순서: ① 주석 재배치 슬라이스(0 위험) → ② void-always_inline 블록 추출(mirror_live·reg_publish_measure·weld_fsm_step case 본체 등, 매 슬라이스 2모델 sha256 대조) → ③ 체인/switch 재구조화는 별도 트랙(HW 벤치 필수). worktree 추적 파일 변경 0건(`git status --short` 공백) 확인 완료.

---

## 0. 범위·방법

- 대상: `fw/src/*.c`(41개) + `fw/drivers/*.c`(13개). `fw/vendor/`·`ref/` 제외.
- 작업 위치: worktree `/Users/tknoh/dev/work/gds_us_ctrl/.claude/worktrees/agent-a0577627e43f2d3e9`, HEAD `b61ef0f`(main).
- 도구: `arm-none-eabi-gcc 15.2.1 (Arm GNU Toolchain 15.2.Rel1)`, cmake/ninja(Homebrew). 빌드는 루트 `./fw.sh`(STD→`fw/build/`) / `MODEL=remote ./fw.sh`(→`fw/build-remote/`).
- 보조 스크립트(모두 scratchpad, repo 밖): `funclen.py`(중괄호 균형 함수 길이·줄 분류), `spike.py`(B 단계 임시 편집), `cmp.sh`(2모델 빌드→sha256 대조→불일치 시 objdump 저장), `fndiff.py`(함수 단위 objdump diff, 주소 정규화).
- 함수 "줄수" 규약: **선언자(함수명이 있는 줄)부터 닫는 `}` 까지 포함**, 중괄호 균형으로 계산. 반환형이 윗줄에 따로 있는 경우 그 줄은 미포함(1줄 차이). 주석·빈 줄 포함.

---

## A. 바이너리 동일성 판정의 전제

### A-1. 빌드 플래그 (근거: `fw/CMakeLists.txt`, `fw/arm-none-eabi-gcc.cmake`)

| 항목 | 값 | 근거 |
|---|---|---|
| 빌드 타입 | `Debug` 기본 강제 | `CMakeLists.txt:14-16` `if(NOT CMAKE_BUILD_TYPE) set(... Debug CACHE STRING "" FORCE)` |
| 최적화 | **`-Og`** (Debug) / `-O2`(Release, 미사용) / `-Os`(MinSizeRel, 미사용) | `CMakeLists.txt:23-27` |
| 디버그 정보 | `-g3 -gdwarf-2` (+ 툴체인 `-g`) | `CMakeLists.txt:24-25`; 실제 ninja FLAGS: `-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard -g -std=c11 -Wall -Wextra -Wundef -Wshadow -ffunction-sections -fdata-sections -fno-common -fstack-usage -Og -g3 -gdwarf-2` (`fw/build/build.ninja`) |
| 섹션 분리 | `-ffunction-sections -fdata-sections` **ON** | `CMakeLists.txt:19` |
| LTO | **없음** (`-flto` 미지정) | 전체 grep 결과 없음 |
| GC | `-Wl,--gc-sections` **ON** | `CMakeLists.txt:113` |
| 링커 스크립트 | `fw/vendor/STM32F410RBTX_FLASH.ld` | `CMakeLists.txt:93,112` |
| 링크 순서 | `file(GLOB APP_SOURCES src/*.c drivers/*.c)` → **glob 정렬 순서**(configure 타임 고정) + startup.s, 그다음 `stm32_hal`·`wiznet` 정적 라이브러리, `--specs=nano.specs -u _printf_float -lc -lm` | `CMakeLists.txt:91-118` |
| 기타 | `-fno-common`, `-fstack-usage`(.su 파일만), Map 파일, `--print-memory-usage` | `CMakeLists.txt:20-21,114-115` |
| 모델 매크로 | `-DMODEL_STD` / `-DMODEL_REMOTE` (`add_compile_definitions(MODEL_${MODEL})`) | `CMakeLists.txt:40-41` |
| 벤치 옵션 | `REMOTE_EN_GATE_BYPASS` 기본 **OFF** (두 빌드 캐시 모두 `:BOOL=OFF`) | `CMakeLists.txt:50-54`; `CMakeCache.txt` |

`-Og` 의 인라인 정책(실측 `arm-none-eabi-gcc -Og -mcpu=cortex-m4 -mthumb -Q --help=optimizers`):

```
-fearly-inlining                 [enabled]
-finline                         [enabled]
-finline-functions               [disabled]
-finline-functions-called-once   [disabled]   ← 핵심
-finline-small-functions         [disabled]
-fipa-sra                        [disabled]
-ftree-sra                       [disabled]
```
(대조 `-O2`: `-finline-functions-called-once [enabled]`, `-finline-small-functions [enabled]`.)
→ **`-Og` 에서는 한 번만 호출되는 `static` 헬퍼도 인라인되지 않고 실제 함수+call 로 남는다.** 이것이 B 결과의 근본 원인이다.

### A-2. 재현성 실측

절차: (1) 두 모델 1차 빌드 → `.bin`/`.elf` 보존, (2) `./fw.sh reconfig` 는 오브젝트가 남아 `ninja: no work to do` 였으므로 **`rm -rf fw/build fw/build-remote` 후 클린 재빌드**, (3) sha256 대조.

| 산출물 | 1차 빌드 | 클린 재빌드 | 판정 |
|---|---|---|---|
| STD `.bin` | `fd66f6e7…e278f` | `fd66f6e7…e278f` | **동일** |
| REMOTE `.bin` | `c5223ada…7373f` | `c5223ada…7373f` | **동일** |
| STD `.elf` | `25797cd8…06cd4` | `25797cd8…06cd4` | 동일(같은 경로에서) |
| REMOTE `.elf` | `7b113c5b…bfeb4` | `7b113c5b…bfeb4` | 동일(같은 경로에서) |

전체 해시: STD `.bin` `fd66f6e7bb9d6ff03f7ea1831797167813b3db4613de66e6c84841b1860e278f`(66,696 B), REMOTE `.bin` `c5223adac03c85861e242e592125bf163114c847f383d6304f5d2e0771f7373f`(67,008 B). FLASH 50.89% / 51.12%.

- CMake POST_BUILD 가 만드는 `gds_us_ctrl.bin`(`CMakeLists.txt:120`) 과 내가 `arm-none-eabi-objcopy -O binary` 로 별도 추출한 `.bin` 의 해시가 **일치** → `fw.sh` 산출 `.bin` 을 그대로 비교 대상으로 써도 된다.
- `.bin` 안의 경로/날짜 문자열 검사(`strings -n 6 | grep -Ei '/Users|\.c:|2026|\d\d:\d\d:\d\d|worktree'`): newlib 내부 `dtoa.c`/`mprec.c` 경로(툴체인 고정 문자열)와 `[boot] gds_us_ctrl ready rst=0x%02X%s` 만 검출. **빌드 경로·`__DATE__`·`__TIME__` 삽입 없음.**
- **`.bin` 을 비교 대상으로 삼는 이유(한 줄):** `.elf` 는 `-g3 -gdwarf-2` 의 DWARF 라인 테이블·`DW_AT_comp_dir`(절대 경로)을 담아 **코드가 같아도 줄 번호/경로가 바뀌면 해시가 바뀌지만**, `.bin` 은 로드 가능 섹션(`.isr_vector/.text/.rodata/.data` 초기값)만 담는다.
  - 실증(라인 이동 실험): `app_modbus.c` `#include <string.h>` 앞에 빈 줄 3개 삽입 → 두 모델 `.bin` **동일**(위 해시 그대로), STD `.elf` 는 `946344ee…` 로 **변화**.
  - 따라서 다른 경로의 worktree/클론에서 빌드한 `.elf` 는 경로 때문에도 달라지므로 **`.elf` 해시는 기준으로 쓰지 말 것**.

### A-3. 라인 민감 매크로 grep

명령: `grep -rnE '__LINE__|__FILE__|__DATE__|__TIME__|\bassert\(|assert_param|USE_FULL_ASSERT|_Static_assert|static_assert' fw/src fw/drivers fw/include`

| 위치 | 내용 | 바이너리 영향 |
|---|---|---|
| `src/app_lcd_render.c:37` | `_Static_assert(sizeof(VERSION_MSG) - 1u == 20u, ...)` | 컴파일 타임 전용, 코드 생성 없음 → **무영향** |
| `src/main.c:42` | `_Static_assert(IWDG_RELOAD <= IWDG_RLR_RL, ...)` | 동상 → **무영향** |
| `include/define.h:53` | 주석에서 `_Static_assert` 언급 | 주석 |
| vendor `stm32f4xx_hal_conf.h:197` | `/* #define USE_FULL_ASSERT 1U */` **주석 처리** → `assert_param()` 은 빈 매크로 | vendor 영역, 무영향 |

`__LINE__`/`__FILE__`/`__DATE__`/`__TIME__`/`assert(` 사용 **0건**. **판정: 라인 이동만으로 `.bin` 이 바뀌는 지점은 없다** (A-2 라인 이동 실험으로 재확인).

---

## B. 인라인 분할 spike (throwaway — 전부 되돌림)

기준 해시 = A-2. 편집은 `spike.py`, 빌드·대조는 `cmp.sh`(두 모델 모두). 각 시도 후 `git checkout -- .`.

### B-1. 결과 표

| # | 대상 | 편집 내용 | 헬퍼 형태 | STD `.bin` | REMOTE `.bin` | 크기 변화(STD) | 판정 |
|---|---|---|---|---|---|---|---|
| b-1 | `app_modbus_apply_writes` (`app_modbus.c:290-647`) | DELAY1~TIMEOVER **13개** cfg 클램프 분기 → `static bool apply_cfg_timing(app_config_t*, bool *save)`; 체인은 `else if (apply_cfg_timing(cfg,&save))` | `static` | ≠ `5324013b…` | ≠ `ff575adb…` | +144 B; 새 심볼 `apply_cfg_timing` 0x174 B, `apply_writes` 0x448→0x364 | **×** |
| b-1i | 동일 | 동일 | `static inline __attribute__((always_inline))` | ≠ `5cc94595…` | ≠ `400df30a…` | +40 B; `apply_writes` 0x448→0x470, 헬퍼 심볼 없음(인라인됨) | **×** |
| b-1v-i | 동일 | 체인 꼬리 전체(DELAY1~마지막 `else` staged 스캔) → `void apply_cfg_tail(app_config_t*, bool *save)`; 호출 `} else { apply_cfg_tail(cfg,&save); }` | `always_inline void` | ≠ `3bdf5081…` | ≠ `79c0f718…` | +56 B; 0x448→0x484 | **×** |
| b-2 | 동일 함수 게이트 블록 (`#ifndef REMOTE_EN_GATE_BYPASS`, :319-373) | `static bool gate_reject(void)` — `if (s_ren.state != REN_ENABLED) {…; return true;} return false;`; 호출 `if (gate_reject()) return;` | `static` | ≠ `c280581e…` | ≠ `79a47514…` | +16 B(REMOTE +24); `gate_reject` 0x88 B, `apply_writes` 0x448→0x3d4 | **×** |
| b-2i | 동일 | 동일 | `always_inline bool` | ≠ `36a64939…` | ≠ `2f42b5c2…` | +8 B; 0x448→0x450 | **×** |
| b-2v | 동일 | **판정은 호출자에 유지** `if (s_ren.state != REN_ENABLED) { gate_reject_body(); return; }`, 본체만 `void gate_reject_body(void)` | `static void` | ≠ `b8ef6e80…` | ≠ `030dce15…` | +8 B | **×** |
| **b-2v-i** | 동일 | 동일 | **`always_inline void`** | **= 기준** | **= 기준** | 0 | **○** |
| b-3 | `app_lcd_input_dispatch` (`app_lcd_input.c:413-637`) | `case LV_DM_DELAY…LV_TM_HOLD` 5개 → 다중 라벨 1개 + `dispatch_limit_time(cfg,vp,data16)` (내부 switch) | `static void` | ≠ `5c1c6f0a…` | ≠ `4f1fbd36…` | ±0 B(STD 크기 같으나 내용 다름), `dispatch_limit_time` 0x82 B, `dispatch` 0x3fc→0x380 | **×** |
| b-3i | 동일 | 동일 | `always_inline void` | ≠ `a368352e…` | ≠ `f754ff1c…` | +8 B; 0x3fc→0x404 (이중 switch) | **×** |
| b-3b | 동일 함수 `case SYS_PIC_NOW:` 본체(:618-631) | case 라벨은 그대로, 본체만 `handle_sys_pic_now(state,cfg,data16)` | `static void` | ≠ `f202b650…` | ≠ `d1f35e8d…` | +8 B; `handle_sys_pic_now` 0x44 B, `dispatch` 0x3fc→0x3c4 | **×** |
| **b-3b-i** | 동일 | 동일 | **`always_inline void`** | **= 기준** | **= 기준** | 0 | **○** |

(해시 앞 8자리만 표기; 전체는 `scratchpad/<label>/` 의 `.bin` 에서 재계산 가능.)

### B-2. 무엇이 달라졌나 (objdump 분석)

- **b-1 (plain static)**: 헬퍼가 독립 함수(0x174 B)로 방출되고 `apply_writes` 에 `bl` 이 생김. 이후 모든 심볼 주소가 밀려 전 이미지 diff 27,889줄(대부분 주소 이동). — `-Og` 가 `-finline-functions-called-once` 를 끄기 때문.
- **b-1i (always_inline bool)**: 헬퍼 심볼은 사라졌으나 `apply_writes` 가 466→487 명령. 함수 단위 diff: **각 클램프 분기 끝에 `movs r3, #1`** 이 추가되고 그 뒤 `cmp/beq` 로 소비 — 기준 코드는 모든 cfg 분기가 `save=true` 로 수렴하는 것을 GCC 가 점프-스레딩해 `app_config_save_all` 로 직행했는데, 헬퍼의 `return true` 가 별도 값으로 물질화되어 그 최적화가 깨졌다. 리터럴 풀(`.word`) 위치도 이동.
- **b-1v-i (always_inline void + `bool *save`)**: `&save` 로 주소가 잡히자 `save` 가 스택 슬롯으로 강제(`-ftree-sra` 비활성) → 분기마다 `movs r3,#1` + `cmp r3,#0; beq` 추가(+56 B). **"로컬 변수를 포인터로 넘기는 헬퍼"는 always_inline 이어도 ×.**
- **b-2i (always_inline bool)**: `apply_writes` 466→468 명령. 진입부 `beq.n` 이 `bne.n` 으로 반전되고 게이트 블록과 본체 체인의 **기본 블록 배치가 재배열** — bool 반환을 소비하는 분기가 원본 `if` 와 다른 CFG 를 만든다.
- **b-2v (plain void)**: 별도 함수 + `bl` (+8 B). **b-2v-i (always_inline void)**: 기준과 **바이트 동일** — 판정과 `return` 이 호출자에 있고 헬퍼는 값 반환 없음, 인자 없음(파일 static 만 접근).
- **b-3 / b-3i**: case 5개를 다중 라벨로 묶고 내부 switch 로 재디스패치 → 외부 switch 의 점프 테이블/비교 트리가 바뀌고 내부 switch 가 추가(이중 디스패치). always_inline 이어도 +8 B. **switch 의 case 재편성은 ×.**
- **b-3b (plain void)**: 별도 함수 0x44 B + `bl` (+8 B). **b-3b-i (always_inline void)**: 기준과 **바이트 동일** — 인자 `state`,`cfg`(이미 존재하는 포인터), `data16`(값), 반환 없음, case 라벨·`break` 는 호출자에 유지.

### B-3. 판정

**"같은 TU 안 static 헬퍼 추출은 바이트 동일" → 조건부 ○.** 조건(실측 2건 ○, 위반 시 실측 ×):

1. `static inline __attribute__((always_inline))` 필수 (`-Og` 는 called-once 인라인을 하지 않는다).
2. 헬퍼는 **`void`** — 값을 반환하지 않고, 호출자가 그 결과로 분기하지 않는다.
3. **판정 구조(if 조건·else-if 체인·case 라벨·`return`/`break`)는 호출자에 남긴다.** 헬퍼에는 부수효과만 있는 **직선 블록**을 넣는다.
4. 인자는 **이미 존재하는 포인터·스칼라 값만**. 호출자 로컬 변수의 **주소를 새로 잡아 넘기지 말 것**(`&save` → 스택 스필 → ×). 헬퍼 안의 `return` 은 허용될 가능성이 있으나 미실측(추측).
5. 매 슬라이스마다 두 모델 `.bin` sha256 대조로 확인 — 위 조건을 지켜도 리터럴 풀·레지스터 할당이 어긋날 수 있어 **사후 대조가 게이트**다.

**불가 판정(×):** else-if 체인의 부분 분할(b-1 계열 전부), bool/tri-state 반환 헬퍼, switch case 재그룹(b-3), 포인터로 넘기는 out-파라미터.

### B-4. 파일 분할(함수를 새 .c 로 이동) — 시도 없이 논증 (추측)

- `-ffunction-sections` + `--gc-sections` 라서 각 함수는 `.text.<name>` 개별 섹션이고, 링커 스크립트는 `*(.text*)` 로 **입력 순서**(오브젝트 순서 → 오브젙트 내 섹션 순서)대로 배치한다. 오브젙트 순서는 `file(GLOB)` 정렬(알파벳). 함수를 다른 .c 로 옮기면 그 함수의 `.text.<name>` 이 **다른 오브젝트 위치**로 이동 → 이후 함수 주소가 전부 밀리고 `bl` 오프셋·리터럴 풀이 바뀐다 → **거의 확실히 ×** (b-1 plain 의 "27,889줄 주소 이동" 과 같은 양상).
- 예외적으로 동일할 수 있는 경우(추측): 옮긴 함수가 원래 오브젝트의 **마지막 섹션**이고 새 .c 가 glob 순서상 **바로 다음**이며 `.rodata`/`.bss` 도 이동이 없을 때 — 실무적으로 기대할 수 없다.
- `static` 함수를 다른 TU 로 옮기면 `static` 을 떼어 외부 링크로 만들어야 하고, 호출은 `bl` 그대로지만 재배치가 달라진다. 또 파일-static 변수(`g_mb`,`s_ren`,`s_stg`,`s_hwd`)를 공유해야 하므로 접근자 추가 = 코드 추가 = ×.
- **결론: 파일 분할은 바이트 동일 불가로 보고 HW 벤치 트랙으로 분리하라.** 다행히 C-3 에 따르면 파일 분할은 필요하지 않다.

---

## C. 인벤토리

### C-1. 50줄 초과 함수 전수 (38개 / 총 301개; `funclen.py funcs src/*.c drivers/*.c`)

컬럼: 줄수 = 선언자~닫는 괄호(주석 포함) / 코드 = 주석·빈 줄 제외 코드 줄. host 테스트 = `fw/test/Makefile` 이 해당 .c 를 링크하고 `test_*.c` 가 함수를 호출하는지(grep). 위험 = B-3 기준(낮음 = void-always_inline 블록 추출로 도달 가능 / 중간 = 조건부·실측 필요 / 높음 = 구조적 ×).

| 파일 | 함수 | 줄수 (코드) | 구조 | 분할 제안 (헬퍼 이름 — 역할) | host 테스트 | 바이트 동일 위험 |
|---|---|---|---|---|---|---|
| app_modbus.c:290-647 | `app_modbus_apply_writes` | 358 (191) | 게이트 블록 + **36분기 else-if 체인** + save | ① `gate_reject_body()` — 게이트 닫힘 소거/로그 본체(**b-2v-i ○ 실증**) ② `start_cmd_body(cfg, sv, now)` — START 탭/hold/keep 분기 본체 ③ `cfg_ctrl_commit_body(cfg, link, &save?)` — F-A 커밋 본체(단, `save` 포인터 금지 → `save=true` 는 호출자에 남길 것) ④ 13개 클램프 분기는 **분할 불가**(b-1 ×). 주석 162줄 재배치 시 ~196줄 | 없음 (`app_modbus.c` 는 host 미링크) | **높음** — 체인 자체가 코드 ≥110줄이라 ≤50 은 구조적 불가. 블록 추출로 ~120 코드줄까지가 한계 |
| app_lcd_input.c:413-637 | `app_lcd_input_dispatch` | 225 (170) | **switch 35 case** | ① `handle_sys_pic_now(state,cfg,data16)` — 패널 리셋 재초기화(**b-3b-i ○ 실증**) ② `handle_setup_param_enter(state)` — SETUP_PARAM/MOOHAN 공통 본체(⚠ 2곳 중복 → 인라인이라 코드 중복 유지, 동일 가능) ③ `handle_mo_time_edit(cfg, vp, data16)` — LV_MO_TIME1/2 본체(각 case 라벨 유지) ④ `handle_run_mode(state,cfg,data16)`. case 재그룹은 **×**(b-3) | 없음 | **높음** — 35개 `case…break` 뼈대만 ~105줄. ≤50 불가; 본체 추출로 ~130줄 |
| app_lcd_render.c:40-242 | `app_lcd_change_page` | 203 (177) | `page` 8분기 else-if + 공통 꼬리 | ① `render_run_std(cfg, buf)` — RUN_STD 3줄 텍스트 ② `render_setup_main(cfg,state)` ③ `render_comm_page(cfg,state,addr_str,ipbuf)` — STDC/MHC·STDE/MHE 공통 본체(현재 소스 중복 2블록) ④ `render_comm_tail(page,state)` | 없음 | **중간** — 판정은 호출자 유지 가능하나 로컬 배열 `buf/addr_str/ipbuf/n` 을 포인터로 넘겨야 함(미실측; b-1v 의 `&save` 와 유사 위험). 헬퍼 안으로 배열 선언을 옮기면 스택 프레임 변화 가능 |
| app_weld_fsm.c:121-290 | `weld_fsm_step` | 170 (141) | FSM `switch(s_run_status)` 5 case + abort 선행 블록 | ① `weld_abort_body(out)` — abort 리셋(단 `return` 은 호출자) ② `weld_step_cyl1(in)` ③ `weld_step_weld(in,out)` — 최대 case(~55줄) ④ `weld_step_cyl2(in,out)` — case 라벨 유지 | `test_app_weld_fsm.c` (21함수) | **낮음~중간** — b-3b 패턴 그대로(인자 `in`,`out` 기존 포인터, 파일 static 접근). WELD case 가 커서 헬퍼 자체 ≥50 가능 |
| app_weld.c:97-227 | `app_weld_tick` | 131 (95) | 순차 단계(게이트→스캔→합성→FSM→출력 디스패치) | ① `weld_dispatch_out(cfg, &out)` — out.* → hook 8분기(`out` 은 이미 로컬 구조체, 포인터 새로 잡음 → 실측 필요) ② `weld_sensor_mirror(cfg, sens_dn)` ③ `weld_in_t` 채우기는 지정 초기화라 헬퍼화 시 코드 변화 가능 | 없음 (`app_weld_fsm.c` 만 host) | **중간** — 로컬 구조체 주소 전달 |
| app_modbus_tcp.c:123-233 | `app_modbus_tcp_poll` | 111 (75) | 순차(recv 누적 → 프레임 워커 루프 → 잔여 이동 → send) | ① `tcp_recv_accumulate()` — recv 블록(static 만 접근, `return` 은 호출자) ② `tcp_flush(off, tx_len)` — memmove+send 꼬리(값 인자) ③ 워커 루프는 `off/tx_len` 출력이 필요 → 포인터 → **×** 후보 | 없음 (`app_modbus_tcp_frame.c` 만 host) | **중간** |
| app_modbus.c:180-287 | `mirror_live` | 108 (62) | 직선 대입 | ① `mirror_cfg_fields(cfg)` ② `mirror_disp_status(cfg, m, running)` ③ `mirror_stage_and_gate(cfg)` — 전부 `g_mb`/`s_stg`/`s_ren` static + 기존 포인터 | 없음 | **낮음** — 이상적인 b-2v-i 패턴. 주석 43줄 재배치만으로도 ≤50 코드 |
| app_reg.c:168-272 | `app_reg_command` | 105 (**48**) | `switch(cmd)` 4 case | 주석 57줄 재배치로 ≤50. 필요 시 `reg_cmd_start(src)`/`reg_cmd_release(src)` (case 내부 `break` → 헬퍼 `return`, 미실측) | 없음 (`app_reg_calc.c` 만 host) | **낮음**(주석) / 중간(헬퍼) |
| app_lcd.c:103-195 | `app_lcd_send_model_str` | 93 (83) | 브랜드 `#if` 4블록 × switch 2 | 실제 컴파일되는 것은 1브랜드(~25줄). 굳이 나누면 `model_str_freq(s)`/`model_str_type(s)` — 로컬 배열 포인터(중간). **권고: 미분할**(컴파일 길이 기준 통과) | 없음 | 중간 |
| app.c:99-186 | `app_loop_iter` | 88 (**47**) | 순차 호출 | 주석 29줄 재배치로 ≤50. `{reg_limits}` 블록 → `reg_step_with_live_limits()`; `{i2c 관측}` 블록은 **함수-static 로컬** 2개 포함 → 헬퍼 이동 시 `.bss` 배치 변화 가능(중간) | 없음 | **낮음**(주석) |
| app_reg.c:324-404 | `reg_publish_measure` | 81 (54) | 순차 | ① `publish_sr_edge(sr)` ② `publish_amp_power(live)` ③ `publish_copy_out(now, active, freq_cal_val)` — 전부 `g_reg/g_measure` static | 없음 | **낮음** |
| app_lcd_disp.c:171-250 | `app_lcd_disp_step` | 80 (55) | 엣지 2블록 + `switch(s)` 10 case(1~2줄) | 주석 21줄 재배치로 ≤50 코드 근접. 엣지 블록은 함수-static(`prev_run_on`,`prev_remote_on`) 포함 → 중간 | 없음 | **낮음**(주석) |
| app_osc_init_fsm.c:24-100 | `osc_init_fsm_step` | 77 (68) | FSM switch 6 case | `osc_step_wait_h(in)` / `osc_step_pulse(out)` — case 라벨 유지 | `test_app_osc_init_fsm.c` | **낮음** |
| app_lcd_disp.c:51-126 | `disp_compute_output` | 76 (54) | 중첩 if + for 6 | ① `fill_upper_band(curr_amp, st)` ② `fill_lower_band(curr_amp, st)` — `level_buf` static, 값 인자 | 없음 | **낮음~중간**(나눗셈·루프 코드 재배치 가능) |
| app_modbus.c:742-815 | `app_modbus_tick` | 74 (**45**) | 순차 + RTU/TCP 분기 | 주석 27줄 재배치로 ≤50. `hold_wdt_block()` ({…} 블록 그대로) | 없음 | **낮음** |
| app_remote_en_fsm.c:19-89 | `remote_en_fsm_step` | 71 (**37**) | 순차 early-return | 주석 29줄 재배치로 ≤50. 분할 불필요 | `test_app_remote_en_fsm.c` | **낮음** |
| app.c:27-96 | `app_init` | 70 (**38**) | 순차 | 주석 22줄 재배치로 ≤50 | 없음 | **낮음** |
| usart6_mb.c:46-113 | `usart6_mb_open` | 68 (53) | 순차(UART init → DMA init → start) | ① `mb_uart_reinit(speed_idx, parity_idx)` ② `mb_dma_init()` — 전역 핸들만 접근 | 없음 | **낮음** |
| app_input.c:36-100 | `app_input_tick` | 65 (**41**) | 순차 + estop 블록 | 주석 19줄 재배치로 ≤50 | 없음 (`app_input_fsm.c` 만 host) | **낮음** |
| app_config.c:88-151 | `app_config_load` | 64 (56) | 순차 read + `fail++` | `load_timing(cfg,&fail)` 류는 **`&fail` 포인터 → ×**; `fail += load_x(cfg)` 는 값 반환 → × 가능성 높음 | `test_app_config.c` (mock_fram) | **높음**(바이트) — host 스위트가 있어 **host 게이트 슬라이스** 후보 |
| dgus_lcd.c:199-261 | `parser_step` | 63 (50) | FSM switch 4 case, case 마다 `return` | bool 반환 구조라 헬퍼화 × — 코드 50줄이라 주석 9줄 재배치로 통과 | 없음 | **낮음**(주석) |
| app_reg.c:471-532 | `app_reg_tick` | 62 (**44**) | 순차 | 주석 재배치 | 없음 | **낮음** |
| app_overload.c:37-98 | `app_overload_tick` | 62 (**35**) | 순차 이벤트 처리 | 주석 23줄 재배치 | 없음 (`app_overload_fsm.c` 만 host) | **낮음** |
| app_lcd_input.c:186-247 | `handle_key_multi` | 62 (**34**) | `data16` 5분기 else-if | 주석 27줄 재배치 | 없음 | **낮음** |
| app_seek_reset_fsm.c:22-81 | `seek_reset_fsm_step` | 60 (50) | FSM switch 3 case | `sr_step_reset(out)`/`sr_step_seek(out)` — 라벨 유지 | `test_app_seek_reset_fsm.c` | **낮음** |
| app_lcd_input.c:332-389 | `handle_std_setup_param` | 58 (52) | `data16` 5분기 else-if | `goto_setup1(state,cfg)` / `goto_setup2(state,cfg)` — 분기 본체(판정은 호출자) | 없음 | **낮음** |
| usart1.c:22-77 | `usart1_init` | 56 (**44**) | 순차 HAL init | 주석 재배치 | 없음 | **낮음** |
| main.c:45-97 | `main` | 53 (**38**) | 순차 | 주석 재배치 | 없음 | **낮음** |
| app_lcd_str.c:67-119 | `energy2str` | 53 (**45**) | else-if 3 | 주석 0줄 — 코드 45줄이라 이미 ≤50 코드 | 없음 | — |
| app_modbus_core.c:172-223 | `mb_core_decode` | 52 (**46**) | switch 6 + return 6 | 코드 46줄 | `test_app_modbus_core.c` | — |
| app_modbus.c:650-701 | `apply_config` | 52 (**41**) | 순차 전이 | 주석 재배치 | 없음 | **낮음** |
| app_lcd_comm.c:346-397 | `data_save_commit` | 52 (**36**) | `lcd_status` 4분기 | 주석 재배치 | 없음 | **낮음** |
| app_lcd_comm.c:278-329 | `commit_comm_mode_and_ether` | 52 (**37**) | 순차 | 주석 재배치 | 없음 | **낮음** |
| app_cfg_stage.c:52-103 | `cfg_stage_commit` | 52 (**35**) | 순차 검증 early-return | 주석 재배치 | `test_app_cfg_stage.c` | **낮음** |
| spi1.c:58-108 | `spi1_init` | 51 (**41**) | 순차 | 주석 재배치 | 없음 | **낮음** |
| i2c1.c:26-76 | `i2c1_bus_unstick` | 51 (**41**) | 순차 + 루프 | 주석 재배치 | 없음 | **낮음** |
| app_lcd_comm.c:129-179 | `process_ip_char` | 51 (**49**) | `key` 4분기 | 코드 49줄 | 없음 | — |
| app_lcd.c:198-248 | `app_lcd_init_mode` | 51 (**31**) | 순차 | 주석 14줄 재배치 | 없음 | **낮음** |

집계:
- 50줄 초과 **38개**. 그중 **코드 줄만 세면 ≤50 인 함수 21개**(굵은 코드 수치) — 주석을 함수 위 블록/헤더/문서로 옮기기만 하면 통과, 바이너리 무영향(A-2 라인 이동 실험).
- 코드 줄도 >50 인 함수 17개: `apply_writes`(191), `dispatch`(170), `change_page`(177), `weld_fsm_step`(141), `app_weld_tick`(95), `send_model_str`(83, 단일 브랜드 ~25), `tcp_poll`(75), `osc_init_fsm_step`(68), `mirror_live`(62), `app_config_load`(56), `disp_step`(55), `publish_measure`(54), `compute_output`(54), `usart6_mb_open`(53), `handle_std_setup_param`(52), `parser_step`(50)·`seek_reset_fsm_step`(50)(경계).
- host 테스트가 함수를 직접 호출하는 것은 7개(`weld_fsm_step`, `osc_init_fsm_step`, `remote_en_fsm_step`, `app_config_load`, `seek_reset_fsm_step`, `mb_core_decode`, `cfg_stage_commit`). 나머지 31개는 HAL 의존 글루라 host 미커버.

### C-2. 400줄 초과 파일 4개 — 줄 분류 (`funclen.py lines`)

| 파일 | 총 | 코드 | 주석 | 빈 줄 |
|---|---|---|---|---|
| `src/app_modbus.c` | **815** | 469 | **311** | 36 |
| `src/app_lcd_input.c` | 637 | 409 | 178 | 51 |
| `src/app_reg.c` | 532 | 320 | 182 | 31 |
| `src/app_lcd_comm.c` | 445 | 312 | 93 | 41 |

(분류 규칙: 빈 줄 = 공백만; 주석 줄 = 주석 제거 후 공백만 남는 줄(`#` 전처리 줄은 코드); 나머지 코드.)

**`app_modbus.c` 판정: 주석만 옮겨도 800줄 아래로 내려간다.** 815 − 800 = **15줄 초과**인데 주석이 311줄(38%)이다. 예: `apply_writes` 게이트 블록 앞 주석 22줄(:298-318) 또는 `mirror_live` 의 설계 주석(:200-204, :226-241, :266-278) 일부를 함수 상단 요약 1블록으로 압축하거나 `docs/` 로 옮기면 충분. 바이너리 무영향(A-2). 별도 슬라이스 후보로 적합.

### C-3. 파일 분할 필요성 결론

- 800줄 초과 파일은 **`app_modbus.c` 하나**뿐이고 주석 재배치로 해소된다 → **파일 분할 불필요**. B-4 논증대로 파일 분할은 바이트 동일이 거의 불가능하므로 하지 않는 것이 맞다.
- 50줄 규칙: 21개는 주석 재배치, ~10개는 void-always_inline 블록 추출로 도달 가능. **`apply_writes`·`dispatch`·(아마) `change_page`·`weld_fsm_step` 4개는 바이트 동일 조건 아래서 ≤50 도달 불가**(체인/switch 뼈대만 50줄 초과) → 규칙 예외 승인 또는 HW 벤치 트랙.

---

## D. 권고

### D-1. 슬라이스 순서 (위험 낮음·가치 높음 먼저; 각 슬라이스 검증 = 두 모델 빌드 → `.bin` sha256 == 기준)

| # | 슬라이스 | 함수 | 예상 헬퍼 | 비고 |
|---|---|---|---|---|
| 1 | **주석 재배치 (0 위험)** | `app_modbus.c` 파일 800↓ + 코드≤50 인 21개 함수의 함수-내 장문 주석을 함수 위/헤더로 이동 | 0 | 코드 무변경. `.bin` 동일이 **보장**(A-2 실증). 리뷰 부담은 "주석 내용 손실 없음" 만 |
| 2 | `mirror_live` 3분할 | `app_modbus.c:180-287` | 3 (`mirror_cfg_fields`, `mirror_disp_status`, `mirror_stage_and_gate`) — 전부 `always_inline void`, 인자 = `cfg`,`m`,`running` | 가장 깨끗한 b-2v-i 패턴 |
| 3 | `reg_publish_measure` 3분할 + `usart6_mb_open` 2분할 | `app_reg.c:324-404`, `usart6_mb.c:46-113` | 3 + 2 | static/전역만 접근, 값 인자 |
| 4 | `apply_writes` 부분 정리 | 게이트 본체(`gate_reject_body` — **○ 실증**), START 본체, CFG_CTRL 커밋 본체(`save=true` 는 호출자) | 3 | ≤50 은 안 되지만 ~120 코드줄로 축소. 체인 13분기는 손대지 말 것 |
| 5 | `dispatch` case 본체 추출 | `SYS_PIC_NOW`(**○ 실증**), SETUP_PARAM/MOOHAN 공통, LV_MO_TIME1/2, LV_RUN_MODE | 4 | case 라벨·`break` 유지. ≤50 불가, ~130줄 |
| 6 | FSM step case 본체 | `weld_fsm_step`, `osc_init_fsm_step`, `seek_reset_fsm_step` | 3~4 + 2 + 2 | host 스위트 3개가 추가 안전망. `weld_step_weld` 가 여전히 >50 일 수 있음 |
| 7 | 조건부(실측 후 결정) | `app_lcd_change_page`(로컬 배열 포인터), `app_weld_tick`(로컬 구조체 포인터), `app_modbus_tcp_poll`(out 파라미터), `disp_compute_output` | — | 첫 시도에서 ≠ 이면 되돌리고 D-3 로 |

### D-2. 검증 스크립트 스케치 (기준 해시 = `fw/build/.bin-baseline`; `fw/build*/` 는 `.gitignore` 됨)

```sh
#!/bin/sh  # fw/tools/bin-same.sh  — 사용: bin-same.sh baseline | bin-same.sh
set -eu; cd "$(dirname "$0")/../.."          # repo 루트 (fw.sh 위치)
./fw.sh >/dev/null && MODEL=remote ./fw.sh >/dev/null
cur=$(shasum -a 256 fw/build/gds_us_ctrl.bin fw/build-remote/gds_us_ctrl.bin | cut -d' ' -f1 | paste -sd' ' -)
base=fw/build/.bin-baseline
[ "${1:-}" = baseline ] && { printf '%s\n' "$cur" >"$base"; echo "baseline: $cur"; exit 0; }
[ "$cur" = "$(cat "$base")" ] && echo "SAME  $cur" || { echo "DIFF  base=$(cat "$base")  cur=$cur"; exit 1; }
```
- 기준은 **리팩토링 시작 커밋에서 `baseline`** 1회 기록(`rm -rf fw/build fw/build-remote` 후 클린 빌드 권장). 값은 A-2 의 `fd66f6e7…e278f` / `c5223ada…7373f` 여야 한다(같은 커밋·같은 툴체인이면).
- `.bin` 만 비교(`.elf` 는 라인·경로에 민감 — A-2). 불일치 시 `arm-none-eabi-nm -S --size-sort` 로 크기 바뀐 심볼을 먼저 보고, `fndiff.py`(scratchpad) 식 함수 단위 objdump diff 로 원인을 좁힌다.
- `fw/build/` 를 지우면 baseline 도 사라지므로 `fw/.bin-baseline`(gitignore 추가) 또는 `docs/`/PR 본문에 해시를 함께 적어 두는 것이 안전하다.

### D-3. 바이트 동일이 구조적으로 불가능한 항목 → "HW 벤치 필요" 로 분리

| 항목 | 이유 | 대안 게이트 |
|---|---|---|
| `app_modbus_apply_writes` 36분기 else-if 체인 분할(테이블화 포함) | b-1 계열 3변형 전부 ×; 체인 뼈대만 >50 코드줄 | HW 벤치(mbpoll FC06 클램프 매트릭스: `docs/superpowers/plans/2026-09-05-bench-results.md` 27항목 재사용) |
| `app_lcd_input_dispatch` switch 재그룹/테이블화 | b-3 ×; 35 case 뼈대 >50 | LCD 육안 + 터치 E2E |
| `app_config_load` 필드 그룹 분할 | `&fail`/반환값 → × 확률 높음 | **host `test_app_config.c` 로 게이트 가능**(HW 불필요) |
| 파일 분할(새 .c 로 이동) 일체 | B-4 링크 순서 논증(추측이지만 확실성 높음) | 불필요(C-3) — 하지 말 것 |
| `app_lcd_change_page` 중복 블록 DRY(STDC/MHC·STDE/MHE 공통화를 **비-인라인**으로) | 코드 크기 축소 = 바이너리 변화 | HW 벤치(comm 페이지 표시 육안) |
| 함수-static 로컬(`app_loop_iter` i2c 블록, `disp_step` prev_* )을 헬퍼로 옮기는 것 | `.bss` 배치 순서 변화 가능(추측) | 실측 후 ≠ 이면 보류 |

---

## 부록

### E-1. 되돌림 확인

모든 spike 후 `git checkout -- .` 실행. 최종:

```
$ git status --short
$ git diff --stat | tail -1
(빈 출력)
```
(`fw/build/`, `fw/build-remote/` 는 `.gitignore`(`fw/.gitignore: build/ build-*/`)로 무시되어 status 에 나타나지 않음.)

### E-2. 실행 명령 요약

```
./fw.sh ; MODEL=remote ./fw.sh                        # 1차 빌드
rm -rf fw/build fw/build-remote ; ./fw.sh ; MODEL=remote ./fw.sh   # 클린 재빌드 (재현성)
shasum -a 256 fw/build/gds_us_ctrl.bin fw/build-remote/gds_us_ctrl.bin
arm-none-eabi-gcc -Og -mcpu=cortex-m4 -mthumb -Q --help=optimizers | grep inline
grep -rnE '__LINE__|__FILE__|__DATE__|__TIME__|\bassert\(|assert_param|USE_FULL_ASSERT|_Static_assert' fw/src fw/drivers fw/include
python3 spike.py <b1|b1v|b2|b2v|b3|b3b> [inline] ; sh cmp.sh <label> ; git checkout -- .
arm-none-eabi-nm -S <elf> | grep -E ' (app_modbus_apply_writes|apply_cfg_timing|gate_reject|...)$'
python3 fndiff.py base1/std.objdump.txt <label>/std.objdump.txt app_modbus_apply_writes
python3 funclen.py funcs fw/src/*.c fw/drivers/*.c ; python3 funclen.py lines <4 files>
```

### E-3. 산출물 위치 (scratchpad, repo 밖)

`/private/tmp/claude-501/-Users-tknoh-dev-work-gds-us-ctrl/71c5ae42-b0bd-4d68-817e-3129cf064724/scratchpad/`
- `base1/` — 기준 `.bin`/`.elf`/objdump (STD·REMOTE)
- `b1/ b1i/ b1vi/ b2/ b2i/ b2v/ b2vi/ b3/ b3i/ b3b/ b3bi/ lineshift/` — 각 spike 의 `.bin`(+불일치 시 objdump), 빌드 로그
- `b3bi.diff` — 바이트 동일했던 b-3b-i 편집의 실제 diff
- `funclen.py spike.py cmp.sh fndiff.py`
