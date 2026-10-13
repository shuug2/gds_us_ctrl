# 브랜드(업체) 빌드 옵션 — `BRAND=moohan MODEL=remote ./fw.sh flash`

> **문서 요약**: `fw/include/define.h:17-21` 의 주석 토글(`#define GDSONIC` / `//#define DIAMT` …)을 없애고, 제품 모델 축 STD/REMOTE 와 같은 방식(CMake 캐시 변수 + `add_compile_definitions` + define.h `#error` 검증 + `fw.sh` 환경변수 → 조합별 빌드 디렉토리)으로 브랜드를 고른다. 코드 변경은 3파일(define.h −6줄, CMakeLists.txt +6줄, fw.sh +8줄)이고 `app_lcd.c` 의 `#ifdef GDSONIC` 블록은 **그대로**다. 기본값 GDSONIC 의 STD/REMOTE `.bin` 은 현재와 **바이트 동일**(스크래치 사본에서 실측: STD `04423faf…`, REMOTE `2a813206…` = `releases/V3.1.0R_260911/` 보관본). 업체 추가 시 손댈 곳 = define.h 의 `#if` 한 줄 + app_lcd.c 의 `#elif` 블록 하나 + `hw/lcd/lcd_V30/README.md` 표 한 줄. SAMA/SONOSYS 는 모델명 규칙 미정이라 범위 밖.

작성 2026-10-04. 검증은 전부 스크래치 사본(`fw/` 복사 + vendor 심링크)에서 끝냈고 저장소는 미수정.

---

## 1. 현재 상태 (읽은 것)

| 위치 | 내용 |
|---|---|
| `fw/include/define.h:11-26` | 브랜드 5종 주석 토글 + `#if (defined(…)+…) != 1 #error` |
| `fw/include/define.h:37-39` | MODEL 축 검증 — `MODEL_STD`/`MODEL_REMOTE` 는 CMake 가 주입, define.h 는 `#error` 만 |
| `fw/CMakeLists.txt:37-41` | `set(MODEL "STD" CACHE STRING …)` + `add_compile_definitions(MODEL_${MODEL})` |
| `fw.sh:23-27, 33-41` | `MODEL` 환경변수 → `build/` 또는 `build-<model>/`, `.src-glob` 스탬프 재구성 시 `-DMODEL` 재전달 |
| `fw/src/app_lcd.c:103-200` | `app_lcd_send_model_str()` — 브랜드 매크로의 **유일한 소비자** (`#if defined(GDSONIC) \|\| defined(DIAMT)` / `#elif POWERTECH` / `#elif MAKETECH` / `#else MOOHAN`) |
| define.h 포함처 | `app_lcd.c`, `app_lcd_render.c`, `app_modbus.c` 만. **fw/test 호스트 스위트는 define.h 를 포함하지 않는다** → 브랜드 무관 |
| `fw/tools/bin-same.sh` | `./fw.sh` + `MODEL=remote ./fw.sh` 두 빌드의 sha 를 `fw/.bin-baseline` 과 비교 (현재 baseline 파일 없음) |
| 문서 | `CLAUDE.md` 빌드 절(`MODEL=remote ./fw.sh` 언급 :127,:167), `hw/lcd/lcd_V30/README.md` 표 헤더 "BRAND (`fw/include/define.h`)", `releases/README.md` 파일명 `gds_us_ctrl_v<버전>.bin`(브랜드 없음) |

`SETUP_PARAM_MOOHAN`(dgus_lcd.h:84, app_lcd_input.c) 은 DGUS 키 값 이름이지 브랜드 매크로가 아니다 — 무관.

## 2. 설계 결정

| # | 결정 | 선택 | 이유 |
|---|---|---|---|
| D1 | 매크로 이름 | **기존 그대로 `GDSONIC` 등을 CMake 가 `-D` 주입** (`add_compile_definitions(${BRAND})`) | `app_lcd.c` 무변경, `.bin` 바이트 동일 자명. `BRAND_` 접두는 app_lcd.c 6줄 + define.h 를 더 건드리고 얻는 게 없다 |
| D2 | 값 검증 | define.h 의 기존 `#if … != 1 #error` 그대로(메시지에 `cmake -DBRAND=…` 힌트 추가) | MODEL 축과 동일 패턴. CMake 쪽에 브랜드 목록을 두면 **목록이 두 곳**이 된다 → 안 둔다. 오타 `-DBRAND=MOOHN` → `-DMOOHN` → 아무 브랜드도 안 정의 → `#error` (실측 확인) |
| D3 | 빌드 디렉토리 | `build[-<brand>][-<model>]` — 기본 GDSONIC/STD 는 **기존 `build/`·`build-remote/` 유지**, 그 외 `build-moohan/`, `build-moohan-remote/` | 기존 경로·`bin-same.sh`·CLAUDE.md 절차(`rm -rf fw/build fw/build-remote`) 그대로 유효 |
| D4 | 대소문자 | fw.sh 가 MODEL 과 같이 `tr` 로 정규화(디렉토리 소문자 / `-DBRAND` 대문자). CMake 직접 호출은 대문자만 | MODEL 과 동일. CMake 에 `string(TOUPPER)` 를 넣지 않는다(MODEL 도 없음) |
| D5 | 전체 브랜드 빌드 | **새 명령 추가 안 함.** 필요 시 한 줄 루프(§5) | 릴리스 때만 쓰는 일이고 for 루프 한 줄이면 된다 |
| D6 | releases 파일명 | **추천만**: 다음 보관본부터 `gds_us_ctrl_<brand>_v<버전>.bin`(예 `gds_us_ctrl_moohan_v3.1.0R_260911.bin`), 폴더는 버전 공유(`releases/<버전>/` 에 브랜드별 .bin 나란히). 기존 `V3.1.0R_260911/` 은 그대로(=GDSONIC) | 버전 문자열은 브랜드 무관이라 폴더를 나눌 이유가 없고, 파일명에 브랜드가 없으면 같은 폴더에 두 .bin 을 못 둔다. **문서 변경은 별도 결정** — 이 플랜은 손대지 않는다 |
| D7 | 범위 밖 | SAMA/SONOSYS 추가, `bin-same.sh` 확장, releases/README 개정 | 모델명 규칙 미정 / 요청 범위 밖 |

## 3. 태스크

### Task 1 — `fw/include/define.h`: 토글 제거, `#error` 힌트

```diff
@@ -14,15 +14,9 @@
  *   POWERTECH "PTW-2020DH"   MOOHAN  "MH-1520DH "
  *   MAKETECH  "SMT-H15D  "   (samd20에 없던 신규 — type이 숫자 앞에 온다)
  */
-#define GDSONIC
-//#define DIAMT     
-//#define POWERTECH 
-//#define MOOHAN    
-//#define MAKETECH  
-
 #if (defined(GDSONIC) + defined(DIAMT) + defined(POWERTECH) + defined(MOOHAN) \
    + defined(MAKETECH)) != 1
-#error "define.h: 모델 브랜드는 정확히 하나만 정의할 것"
+#error "define.h: 모델 브랜드는 정확히 하나만 정의할 것 (cmake -DBRAND=GDSONIC|DIAMT|POWERTECH|MOOHAN|MAKETECH)"
 #endif
```

주석 정정(바이너리 무영향):
- `:11` `/*--- 모델 브랜드 — 정확히 하나만 활성화 (samd20 main.c:27-30) ---` → `/*--- 모델 브랜드(업체) — cmake -DBRAND=<이름> 이 고른다 (CMakeLists.txt, BRAND=moohan ./fw.sh) ---`
- `:1` 헤더 `빌드 시 고르는 모델 브랜드` 는 이미 맞는 말이라 유지. `:5` 소비처 줄 유지.
- **업체 추가 절차**를 `:16` 블록 끝에 한 줄로: ` * 업체 추가 = 아래 #if 합에 한 항 + app_lcd.c app_lcd_send_model_str() 에 #elif 블록 + hw/lcd/lcd_V30/README.md 표 한 줄.`

### Task 2 — `fw/CMakeLists.txt`: BRAND 캐시 변수 (MODEL 블록 바로 아래, :42 뒤)

```diff
 set(MODEL "STD" CACHE STRING "제품 모델 — STD | REMOTE")
 add_compile_definitions(MODEL_${MODEL})
 
+# 모델 브랜드(업체) — LCD MODEL_NAME 문자열만 갈린다 (app_lcd.c app_lcd_send_model_str).
+# 값 검증은 define.h의 #error가 한다 (오타 -DBRAND=MOOHN → 컴파일 실패). 업체 추가 = define.h + app_lcd.c.
+#   cmake -B build-moohan -DBRAND=MOOHAN -G Ninja   또는   BRAND=moohan ./fw.sh
+set(BRAND "GDSONIC" CACHE STRING "모델 브랜드 — GDSONIC | DIAMT | POWERTECH | MOOHAN | MAKETECH")
+add_compile_definitions(${BRAND})
+
 # 원격 활성화 게이트 우회 — 벤치 전용 탈출구. MODEL=REMOTE 빌드에서만 의미가 있다
```

(CACHE 설명 문자열의 브랜드 나열은 `cmake -LH` 안내용. 정본 목록은 define.h — 업체 추가 때 이 문자열은 **안 고쳐도 동작**한다. 같이 고치면 좋고, 빠뜨려도 틀어지는 건 없다.)

### Task 3 — `fw.sh`: BRAND 환경변수 → 디렉토리 + `-DBRAND` 재전달

```diff
@@ -8,9 +8,12 @@
 #   ./fw.sh reconfig   cmake 강제 재구성 후 빌드
 #
-# 제품 모델(기능 티어)은 MODEL 환경변수로 고른다 — 모델별 빌드 디렉토리를 쓴다.
-#   ./fw.sh flash                 STD    -> build/        (기본, legacy 기능셋)
-#   MODEL=remote ./fw.sh flash    REMOTE -> build-remote/
+# 제품 모델(기능 티어)은 MODEL, 브랜드(업체)는 BRAND 환경변수로 고른다 — 조합별 빌드 디렉토리.
+#   ./fw.sh flash                              GDSONIC STD    -> build/          (기본)
+#   MODEL=remote ./fw.sh flash                 GDSONIC REMOTE -> build-remote/
+#   BRAND=moohan ./fw.sh flash                 MOOHAN  STD    -> build-moohan/
+#   BRAND=moohan MODEL=remote ./fw.sh flash    MOOHAN  REMOTE -> build-moohan-remote/
+# 브랜드 목록 = fw/include/define.h (오타는 #error 로 터진다).
 #
@@ -20,11 +23,15 @@
 cm() { env -u STM32_TOOLCHAIN cmake "$@"; }
 
-# 모델별 빌드 디렉토리 — 두 모델이 서로의 오브젝트를 덮어쓰지 않게 분리한다.
-# cmake 캐시가 MODEL을 기억하므로 재구성 때마다 넘겨 캐시와 디렉토리를 일치시킨다.
+# 조합별 빌드 디렉토리 — 서로의 오브젝트를 덮어쓰지 않게 분리한다. 기본(GDSONIC/STD)은 build/.
+# cmake 캐시가 MODEL/BRAND를 기억하므로 재구성 때마다 넘겨 캐시와 디렉토리를 일치시킨다.
 model_lc=$(printf '%s' "${MODEL:-std}" | tr 'A-Z' 'a-z')
 model_uc=$(printf '%s' "$model_lc" | tr 'a-z' 'A-Z')
-if [ "$model_lc" = std ]; then build=build; else build="build-$model_lc"; fi
+brand_lc=$(printf '%s' "${BRAND:-gdsonic}" | tr 'A-Z' 'a-z')
+brand_uc=$(printf '%s' "$brand_lc" | tr 'a-z' 'A-Z')
+build=build
+[ "$brand_lc" = gdsonic ] || build="$build-$brand_lc"
+[ "$model_lc" = std ]     || build="$build-$model_lc"
 
@@ -34,8 +41,8 @@
     if [ ! -f "$stamp" ] || [ "$list" != "$(cat "$stamp")" ]; then
-        echo "[fw.sh] 소스 목록 변경 감지 → cmake 재구성 (MODEL=$model_uc, $build/)"
-        cm -B "$build" -G Ninja -DMODEL="$model_uc" >/dev/null
+        echo "[fw.sh] 소스 목록 변경 감지 → cmake 재구성 (BRAND=$brand_uc MODEL=$model_uc, $build/)"
+        cm -B "$build" -G Ninja -DMODEL="$model_uc" -DBRAND="$brand_uc" >/dev/null
         printf '%s\n' "$list" >"$stamp"
```

`reset`/`test`/`reconfig` 케이스는 `$build` 를 통해 자동으로 따라온다 — 변경 없음.

### Task 4 — 주석·문서 한 줄씩 (바이너리 무영향)

| 파일 | 변경 |
|---|---|
| `fw/src/app_lcd.c:105` | `/* 브랜드는 define.h가 고른다 (samd20 …` → `/* 브랜드는 cmake -DBRAND 가 주입한다 — 목록·검증은 define.h (samd20 …`. 새 업체 블록은 `#elif defined(MAKETECH)` 뒤, `#else /* MOOHAN */` 앞에 추가 |
| `CLAUDE.md` 빌드·플래시 절(`./fw.sh gdb` 줄 아래) | `BRAND=moohan MODEL=remote ./fw.sh flash   # 브랜드(업체) — 기본 gdsonic, 목록 fw/include/define.h → build-moohan-remote/` 한 줄 추가 |
| `hw/lcd/lcd_V30/README.md` 표 헤더 | `BRAND (\`fw/include/define.h\`)` → `BRAND (\`BRAND=<소문자> ./fw.sh\`, 목록 \`fw/include/define.h\`)` |
| `docs/changelog.md` | Unreleased 항목 한 줄: "브랜드를 빌드 옵션 `-DBRAND`/`BRAND=` 로 전환, 기본 GDSONIC `.bin` 바이트 동일" |

`releases/README.md` 는 **손대지 않는다**(D6 — 사용자 결정 뒤 별도).

## 4. 검증 (스크래치 사본에서 2026-10-04 전건 실측 PASS)

실행 순서 그대로. 저장소에서 할 때는 `fw/build*` 가 이미 있어도 CMakeLists.txt 변경을 ninja 가 감지해 자동 재구성한다.

```bash
cd ~/dev/work/gds_us_ctrl

# 0) 변경 전 기준 sha (Task 1~3 적용 전에 먼저!)
./fw.sh >/dev/null && MODEL=remote ./fw.sh >/dev/null
shasum -a 256 fw/build/gds_us_ctrl.bin fw/build-remote/gds_us_ctrl.bin
#   기대: STD 04423fafb46dc3b244ac1b9a3fa409e0fdebbda5da827a774ac1cc474a6a7429
#        REMOTE 2a813206b75a4e4bc807c0a3961ba1b41c5ab6a705c0843c602fedf272760563  (= releases/V3.1.0R_260911 보관본)
#   또는 fw/tools/bin-same.sh baseline

# 1) 적용 후 기본 빌드 바이트 동일
./fw.sh >/dev/null && MODEL=remote ./fw.sh >/dev/null
shasum -a 256 fw/build/gds_us_ctrl.bin fw/build-remote/gds_us_ctrl.bin     # 0) 과 동일 / 또는 bin-same.sh → SAME

# 2) MOOHAN 빌드 성공 + 디렉토리·캐시 일치
BRAND=moohan MODEL=remote ./fw.sh            # "[fw.sh] … (BRAND=MOOHAN MODEL=REMOTE, build-moohan-remote/)", FLASH 51.32%
grep -E '^(BRAND|MODEL):' fw/build-moohan-remote/CMakeCache.txt   # BRAND:STRING=MOOHAN / MODEL:STRING=REMOTE
BRAND=Moohan ./fw.sh                         # 대소문자 정규화 → build-moohan/, BRAND:STRING=MOOHAN

# 3) 나머지 브랜드 전부 컴파일 (한 줄 루프 — D5)
for b in diamt powertech maketech; do BRAND=$b ./fw.sh >/dev/null && echo "$b OK"; done
#   우리 코드 경고 0. (vendor wiznet socket.h -Wunused-function 3건×2 는 기존과 동일, 브랜드 무관)

# 4) 잘못된 값 → 컴파일 실패
BRAND=moohn ./fw.sh 2>&1 | grep '#error'
#   define.h:19: #error "define.h: 모델 브랜드는 정확히 하나만 정의할 것 (cmake -DBRAND=GDSONIC|…)"
rm -rf fw/build-moohn                        # 빈 디렉토리 잔재 정리 (MODEL 오타 때와 같은 거동)

# 5) host 테스트 (define.h 미포함이라 영향 없음을 확인)
./fw.sh test | grep -c 'all tests passed'    # 17

# 6) (선택) 보드: BRAND=moohan ./fw.sh flash → LCD MODEL_NAME "MH-1520DH" 류, 버전 문자열은 GDSONIC 과 동일
```

**MOOHAN STD .bin sha(참고)**: `f0bbf11c47e169e949986c11192116c93c6fb634a75989f8ae8a29d4a982ee9c`.

## 5. 업체 추가 절차 (이 변경 뒤)

1. `fw/include/define.h` — `#if (defined(…)+…) != 1` 합에 `+ defined(NEWBRAND)`, 위 주석 표에 모델명 예 한 줄, `#error` 문자열에 이름 추가.
2. `fw/src/app_lcd.c` `app_lcd_send_model_str()` — `#elif defined(NEWBRAND)` 블록(모델명 규칙).
3. `hw/lcd/lcd_V30/README.md` 표 한 줄 + `hw/lcd/lcd_V30/<업체>/DWIN_SET/`.
4. (선택) `fw/CMakeLists.txt` CACHE 설명 문자열에 이름 추가 — 동작엔 무관.

빌드: `BRAND=newbrand ./fw.sh`. CMake·fw.sh 코드는 **무변경**.

## 6. 열린 결정 (사용자)

| # | 질문 | 선택지 | 추천 |
|---|---|---|---|
| Q1 | 매크로 이름 | (a) 기존 `GDSONIC` 그대로 주입 / (b) `BRAND_GDSONIC` 접두 | **(a)** — app_lcd.c 무변경 |
| Q2 | 빌드 디렉토리 순서 | (a) `build-moohan-remote` / (b) `build-remote-moohan` | **(a)** — 브랜드가 바깥 축(제품), 모델이 안쪽(기능) |
| Q3 | `./fw.sh all`(전 브랜드×모델) 추가 | (a) 안 함, §4-3 루프 / (b) 추가 | **(a)** — 릴리스 때만, 한 줄로 충분 |
| Q4 | releases 파일명에 브랜드 | (a) 다음 보관본부터 `gds_us_ctrl_<brand>_v<ver>.bin` 전 브랜드 공통(gdsonic 포함) / (b) GDSONIC 만 기존 이름 유지 / (c) 현행 유지 | **(a)** — 같은 폴더에 브랜드별 .bin 나란히, 혼용 규칙 없음. `releases/README.md` 개정은 별도 커밋 |
| Q5 | 기존 `releases/V3.1.0R_260911/` 개명 | (a) 그대로(README 에 "=GDSONIC" 한 줄) / (b) 개명 | **(a)** — SHA256SUMS·보드 이미지 대조 기록 보존 |
