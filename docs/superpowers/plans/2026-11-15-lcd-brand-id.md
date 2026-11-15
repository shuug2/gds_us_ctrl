# LCD 브랜드 번호 (V31 단일 바이너리) Implementation Plan

> **문서 요약**: 설계 `specs/2026-11-15-lcd-brand-id-design.md` 를 4개 태스크로 구현한다 — ① 브랜드 규칙을 순수 모듈로 분리(host 90조합 골든 테스트, V30 거동 불변) ② DGUS 16비트 VP 읽기 + V31 런타임 브랜드(부팅·패널 재기동 때 VP 0x1500 읽기) ③ V31 빌드에서 BRAND 축 제거(fw.sh·CMake·define.h) ④ 문서. 마지막에 사용자와 V31 벤치(게이트 = 22_Config 로드 확인). 게이트 FAIL 이면 브랜치 폐기.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** V31 펌웨어 하나가 LCD 세트의 브랜드 번호(VP 0x1500)로 업체별 LCD 모델명을 고르게 한다.

**Architecture:** 5개 브랜드의 모델명 규칙을 순수 모듈 `app_brand.c` 로 옮기고(규칙 코드 한 벌), V30 은 컴파일 타임 브랜드 상수를, V31 은 부팅·패널 재기동 때 DGUS 에서 읽은 번호를 넘긴다. 번호 0·모르는 값·읽기 실패 = GDSONIC.

**Tech Stack:** C11, STM32 HAL, arm-none-eabi-gcc, CMake/Ninja(`./fw.sh`), host 테스트 `fw/test/Makefile`(cc).

**Spec:** `docs/superpowers/specs/2026-11-15-lcd-brand-id-design.md`

**작업 위치:** worktree `/Users/tknoh/dev/work/gdsonic/gds_us_ctrl/.claude/worktrees/feat-lcd-brand-id`, 브랜치 `feat/lcd-brand-id`. 메인 체크아웃의 `hw/lcd/tools/` 미커밋 파일은 lcd 세션 것 — 건드리지 않는다.

## Global Constraints

- 브랜드 번호: 0 = 미설정→GDSONIC, 1 GDSONIC, 2 MOOHAN, 3 DIAMT, 4 POWERTECH, 5 MAKETECH. 6 이상 → GDSONIC (6 SAMA · 7 SONOSYS 는 예약만, 코드 없음).
- 브랜드 VP = `0x1500`. 결과는 RAM 에만(FRAM 저장 금지).
- V30 빌드는 지금처럼 `-DBRAND=` 컴파일 타임. V31 빌드는 BRAND 축 없음.
- 모델명 문자열은 현 업체별 빌드와 **글자 단위로 동일**(11바이트, `s[10]=='\0'`, 범위 밖 freq/type 은 현 default 케이스와 동일).
- `fw/vendor/`·`ref/` 수정 금지. 런타임 검증에 SWD halt 금지. 요청 범위 밖 코드 수정 금지.
- 빌드: `./fw.sh` (worktree 의 fw.sh), 수동 cmake 시 `env -u STM32_TOOLCHAIN`. 새 `.c` 추가 → fw.sh 가 glob 스탬프로 재구성.
- 커밋 메시지 형식 `<type>: <설명>` + 트레일러 2줄(아래 각 커밋 단계).

## Review Focus

1. **패널이 늦게 뜸**(부팅 4 s `dgus_wait_ready` 실패) → 부팅 읽기 실패로 GDSONIC, 패널이 뜨면 `handle_sys_pic_now` 재읽기로 그 업체 모델명. — Task 2 벤치 항목 B-4.
2. **`dgus_read_word` 가 RX 링의 다른 프레임(터치)을 버린다** — 재읽기는 패널 재기동 순간에만 호출(운전 중 호출 경로 추가 금지). — Task 2 리뷰 체크.
3. **범위 밖 freq/type**(FRAM 오염 값 9 등) → 현 default 케이스와 같은 문자열. — Task 1 테스트 `test_out_of_range_matches_default`.
4. **V30 빌드 무회귀** — 4브랜드 V30 `.bin` 의 모델명 거동 동일(host 골든) + 빌드 경고 0. — Task 1·3 검증.
5. **V31 빌드에 BRAND 를 넘김**(`HW_REV=v31 BRAND=moohan`) → 혼동 방지: fw.sh 가 경고 1줄 출력 후 무시, 폴더 `build-v31-<model>/`. — Task 3 검증.

---

### Task 1: 브랜드 규칙 순수 모듈 + host 골든 테스트

**Files:**
- Create: `fw/include/app_brand.h`, `fw/src/app_brand.c`, `fw/test/test_app_brand.c`
- Modify: `fw/src/app_lcd.c:102-180` (`app_lcd_send_model_str` 본체를 위임으로), `fw/test/Makefile` (스위트 추가, 헤더 주석 수 18→19)

**Interfaces:**
- Produces (`app_brand.h`):
  ```c
  typedef enum { BRAND_GDSONIC = 1, BRAND_MOOHAN = 2, BRAND_DIAMT = 3,
                 BRAND_POWERTECH = 4, BRAND_MAKETECH = 5 } brand_t;
  #define BRAND_VP             0x1500u
  #define BRAND_MODEL_STR_LEN  11u
  brand_t brand_from_id(uint16_t id);                  /* 1..5 → 그 값, 그 외 GDSONIC */
  void    brand_model_str(brand_t b, uint8_t freq, uint8_t type, uint8_t out[BRAND_MODEL_STR_LEN]);
  ```
  enum 이 범위 밖(캐스트 오염)이면 `brand_model_str` 도 GDSONIC 규칙.
- `app_lcd_send_model_str(freq, type)` 시그니처 불변. 내부 = `brand_model_str(app_lcd_brand(), freq, type, s); dgus_write_bytes(MODEL_NAME, s, 11);`
- Produces (`app_lcd.h`): `brand_t app_lcd_brand(void);` — 이 태스크에서는 V30 컴파일 타임 상수만 반환(아래 `BRAND_BUILD`). Task 2 가 V31 분기 추가.
- `app_lcd.c` 안: `#if defined(GDSONIC) → BRAND_BUILD BRAND_GDSONIC` … 5개 매핑, 없으면 `BRAND_GDSONIC`(Task 3 에서 V31 은 브랜드 매크로가 없어진다).

- [ ] **Step 1: 골든 테스트 작성** — `test_app_brand.c` (기존 `test_boot_magic.c` 형식 따름)
  - `test_golden_all_brands`: 5 브랜드 × freq 0..5 × type 0..2 = 90 개 기대 문자열 표(11바이트, 마지막 `\0`)를 **현재 `app_lcd.c:102-180` 의 각 `#if` 블록을 손으로 풀어** 적는다. 예: `(BRAND_GDSONIC,3,0)→"GDS-35H   "`, `(BRAND_POWERTECH,2,1)→"PTW- 735MD"`, `(BRAND_MOOHAN,0,2)→"MH-1515DS "`, `(BRAND_MAKETECH,1,1)→"SMT-A20D  "`, `(BRAND_DIAMT,5,2)→"DIS-50S   "`. `memcmp(out, exp, 11)==0`.
  - `test_out_of_range_matches_default`: 각 브랜드에서 `(freq=9,type=9)` 출력 == `(freq=0,type=0)` 출력.
  - `test_brand_from_id`: `0→GDSONIC`, `1..5→같은 값`, `6,7,0x0200,0xFFFF→GDSONIC`.
  - `test_bad_enum_uses_gdsonic`: `brand_model_str((brand_t)0, 0,0, out)` == GDSONIC `(0,0)`.
- [ ] **Step 2: 실패 확인** — `make -C fw/test test` → 링크 실패(`brand_model_str` 미정의).
- [ ] **Step 3: 구현** — `app_brand.c` 에 브랜드별 `static void` 함수 5개(현 `#if` 블록 본문을 그대로 이동, 주석·legacy 출처 포함) + switch 디스패치. `app_lcd.c` 는 위임만 남긴다.
- [ ] **Step 4: 통과 확인** — `make -C fw/test test` 전 스위트 PASS. 그리고 `./fw.sh`·`BRAND=moohan ./fw.sh`·`BRAND=powertech MODEL=remote ./fw.sh`·`BRAND=maketech ./fw.sh` 경고 0.
- [ ] **Step 5: 커밋** — `git add` 위 파일들 + spec·plan 문서, 메시지 `refactor: 브랜드 모델명 규칙을 순수 모듈 app_brand 로 분리 (host 골든 90조합)` + 트레일러:
  ```
  Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
  Claude-Session: https://claude.ai/code/session_011oqqP4QhazQL29nDN5E9Tg
  ```

### Task 2: 16비트 VP 읽기 + V31 런타임 브랜드

**Files:**
- Modify: `fw/drivers/dgus_lcd.c:179-184,293`, `fw/include/dgus_lcd.h:165`, `fw/src/app_lcd.c`, `fw/include/app_lcd.h`, `fw/src/app.c:61-62`, `fw/src/app_lcd_input.c:433-447`

**Interfaces:**
- Consumes: `brand_from_id`, `BRAND_VP`, `app_lcd_brand()` (Task 1).
- `dgus_read_var(uint16_t vp)` — 파라미터를 16비트로(기존 호출자는 uint8_t 를 넘기므로 무변경). `dgus_read_word` 의 `(uint8_t)` 캐스트 제거. 와이어 프레임은 VP 하위 8비트 기존 호출(SYS_PIC_NOW 등)에서 바이트 동일.
- Produces (`app_lcd.h`): `void app_lcd_brand_refresh(void);` — `#if defined(HW_V31)` 일 때만 본체: `dgus_read_word(BRAND_VP, &v, DGUS_READ_REPLY_TIMEOUT_MS)` 최대 `BRAND_READ_RETRIES = 3u` 회, 성공 시 `s_brand = brand_from_id(v)`, 실패 시 `s_brand = BRAND_GDSONIC`. 매 호출 `mon_printf("[lcd] brand vp=0x%04X raw=0x%04X ok=%u -> %u\r\n", ...)`(실패면 raw=0). V30 은 빈 함수. `app_lcd_brand()` 는 V31 에서 `s_brand`(초기값 `BRAND_GDSONIC`) 반환.
- 호출 위치 2곳(설계 §6.2): ① `app.c` — `mon_printf("[lcd] ready=...")` 직후, `app_lcd_init_mode` 전. ② `handle_sys_pic_now` — 가드 통과 블록 안, `app_lcd_send_model_str` 직전. 다른 곳에 추가 금지(Review Focus 2).

- [ ] **Step 1: 구현** (글루 — host 하네스 없음, 게이트는 벤치)
- [ ] **Step 2: 빌드** — `HW_REV=v31 MODEL=remote ./fw.sh`, `./fw.sh` 경고 0, host 테스트 PASS.
- [ ] **Step 3: V30 와이어 무변경 확인** — 이 태스크 전후 `build-v30-gdsonic-ctrl/gds_us_ctrl.bin` 비교: `dgus_read_var` 시그니처 변경 외 차이는 허용하지만, `fw/tools/bin-same.sh` 로 비교해 결과(동일/상이)를 리뷰에 보고.
- [ ] **Step 4: 커밋** — `feat: V31 런타임 브랜드 — 부팅·패널 재기동 때 LCD VP 0x1500 읽기` + 트레일러.

### Task 3: V31 빌드에서 BRAND 축 제거

**Files:**
- Modify: `fw.sh:11-31`(주석·폴더 이름), `fw/CMakeLists.txt:47-52`, `fw/include/define.h:8-21`

**Interfaces:**
- `fw.sh`: `hw_lc=v31` 이면 `build=build-v31-$model_lc`, BRAND 환경변수가 설정돼 있으면 `[fw.sh] V31 은 BRAND 를 LCD 세트가 정한다 — BRAND=<값> 무시` 한 줄 출력, cmake 에 `-DBRAND=NONE`. V30 은 현행 그대로.
- CMake: `BRAND` 가 `NONE` 이면 `add_compile_definitions` 생략.
- `define.h`: `#if defined(HW_V31)` → 브랜드 매크로가 **하나도 없어야** 함(있으면 `#error "V31 은 런타임 브랜드 — BRAND 지정 금지"`), 그 외 → 현행 "정확히 하나" 검사. 주석에 브랜드 번호표·VP 0x1500·설계 문서 경로.
- HW_REV 검사(`define.h:42`)가 브랜드 검사보다 아래 있으면 순서 맞춤.

- [ ] **Step 1: 구현**
- [ ] **Step 2: 검증** — `HW_REV=v31 ./fw.sh` → `fw/build-v31-ctrl/` 생성·경고 0 / `HW_REV=v31 MODEL=remote ./fw.sh` → `build-v31-remote/` / `HW_REV=v31 BRAND=moohan MODEL=remote ./fw.sh` → 경고 1줄 + `build-v31-remote/` 재사용 / V30 4브랜드 × 2모델 빌드 경고 0 / host PASS. 옛 `build-v31-<brand>-*` 폴더는 지우지 말고 보고만.
- [ ] **Step 3: 커밋** — `build: V31 빌드에서 BRAND 축 제거 (build-v31-<model>/)` + 트레일러.

### Task 4: 문서

**Files:**
- Modify: `CLAUDE.md`(빌드 축 표·명령 예·보관 파일명 규칙), `docs/changelog.md`([Unreleased]), `releases/README.md`(V31 파일명 `gds_us_ctrl_hw31_v<버전>.bin`), `production/README.md`("다음 보관본부터 — V31 생산·출하 흐름" 절: 업체 전환 = LCD 세트 SD 만, 이 설계로 대체 — 게이트 PASS 전제 표기), `hw/lcd/lcd_V30/README.md`(브랜드 VP 0x1500·번호표 한 줄)

- [ ] **Step 1: 갱신** — 각 문서 기존 서식 유지, 설계 문서 링크.
- [ ] **Step 2: 커밋** — `docs: V31 런타임 브랜드(LCD VP 0x1500) — 빌드 축·보관 파일명·출하 흐름` + 트레일러.

### Task 5: 벤치 (사용자와 함께, 서브에이전트 아님)

시험 세트 = gdsonic `DWIN_SET` 복사본의 `22_Config.bin` 오프셋 `0x2A00`(=VP 0x1500×2)에 `00 02`(MOOHAN, 빅엔디언 가정) — 정본은 건드리지 않고 scratchpad 에 만든다. 보드 = V31, mon = J3 115200.

- B-1 (게이트): 시험 세트 SD → 전원 재투입 → mon `[lcd] brand vp=0x1500 raw=… ok=… -> …` 줄로 판정:
  - `ok=1 raw=0x0002` → **PASS**.
  - `ok=1 raw=0x0200` → 엔디언 반대 — 시험 세트 바이트 순서 정정 후 재시험.
  - `ok=1 raw=0x0000` → 패널 전원은 유지한 채 MCU 만 리셋(`HW_REV=v31 MODEL=remote ./fw.sh reset`) 후 mon 줄 재확인: 따뜻한 읽기에서 `0x0002` = 타이밍 문제(너무 일찍 읽음, 지연으로 해결 가능) / 여전히 0 = **게이트 FAIL → 브랜치 폐기, 업체별 .bin 유지**.
  - `ok=0` → RD 무응답(통신 문제) — `[lcd] ready=` 줄 확인 후 재시험. 게이트 FAIL 아님.
- B-2: 같은 상태에서 LCD 모델명 = `MH-…`(MOOHAN 규칙).
- B-3: 정본 gdsonic 세트(번호 칸 0) → `GDS-…`.
- B-4: LCD 커넥터 뺀 채 부팅 → 5 s 뒤 연결 → 패널이 뜨면 mon 재읽기 줄 + 시험 세트 기준 `MH-…`. 재읽기 줄이 없으면 같은 핸들러가 직전에 찍는 `[lcd-hook] us_command=` 줄도 없는지 확인 → 없으면 handle_sys_pic_now 미발화 = 기존 메커니즘(패널이 SYS_PIC_NOW 프레임을 자발 송신하는지) 문제로 분리 기록, 브랜드 결함 아님.
- B-5: Modbus 무회귀 1항목(FC03 0x00 읽기) + 운전 START/STOP 1회.
- B-6: 런 중 패널 재기동(LCD 커넥터 순간 분리/재연결) → mon 타임스탬프로 refresh 블록 시간 측정, 무응답 시 ≤360 ms; 출력 정지 확인.

**벤치 결과 (2026-11-15, V31 REMOTE)**: B-1 PASS(`raw=0x0002 ok=1 -> 2`, 빅엔디언 확정; 첫 시도 0 은 시험 세트 미적재) · B-2 PASS(`MH-…`) · B-3 PASS(`GDS-…`) · B-4 ⚠ 늦게 꽂은 LCD 가 SYS_PIC_NOW 를 안 보내 로고에서 멈춤(us_command 줄도 없음 = 기존 메커니즘, 브랜드 결함 아님 — HANDOFF 열린 항목) · B-5 PASS(FC03 + START→0.55 s ceiling→STOP) · B-6 측정 불가(B-4 와 같은 경로). 게이트 PASS → 머지.
