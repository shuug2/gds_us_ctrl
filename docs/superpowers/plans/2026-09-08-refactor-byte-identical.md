# 바이트 동일 리팩토링 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 50줄 초과 함수 38개와 800줄 초과 파일 1개(`app_modbus.c`)를, STD·REMOTE 두 모델의 `.bin` sha256 이 시작 커밋과 **바이트 동일**하다는 조건 아래 정리한다 — 주석 이동·압축(슬라이스 1)과 `always_inline void` 본체 추출(슬라이스 2~7)만으로.

**Architecture:** 두 수단만 쓴다. ① 함수 안 장문 주석을 함수 선언 바로 위 헤더 블록으로 옮기고, `app_modbus.c` 의 이력 서술은 changelog/spec 에 있는 내용만 압축한다(바이너리 무영향). ② 판정 구조(`if`·`case`·`break`·`return`)는 호출자에 남기고 직선 본체만 `static inline __attribute__((always_inline)) void` 헬퍼로 뽑는다 — 인자는 이미 존재하는 포인터·스칼라 값만. 슬라이스마다 `fw/tools/bin-same.sh` 가 두 모델 `.bin` sha256 을 기준과 대조하는 것이 게이트다(`SAME` 아니면 되돌리고 보류).

**Tech Stack:** arm-none-eabi-gcc 15.2.1 20251203 (Arm GNU Toolchain 15.2.Rel1) · CMake/Ninja (`./fw.sh`, `MODEL=remote ./fw.sh` → `fw/build/`, `fw/build-remote/`) · host 테스트 `fw/test` (`./fw.sh test`, 17 스위트) · `shasum -a 256` · python3 (`fw/tools/funclen.py`)

**Spec:** docs/superpowers/specs/2026-09-08-refactor-byte-identical-design.md (구현 정본). 감사 = docs/superpowers/research/2026-09-08-refactor-audit.md.

> **문서 요약**: Task 0(도구·기준 해시) → Task 1(주석 재배치 — 23함수 검토·22함수 편집·21함수 ≤50 + `app_modbus.c` 815→753) → Task 2~6(헬퍼 추출: `mirror_live`·`reg_publish_measure`·`usart6_mb_open`·`apply_writes`·`lcd_input_dispatch`·FSM step 3개) → Task 7(조건부 5건, ≠ 이면 파일 단위 되돌림) → Task 8(최종 게이트·changelog·spec §6·머지). 모든 코드 스니펫은 스크래치 사본에 실제로 적용해 `arm-none-eabi-gcc -fsyntax-only`(실 빌드 플래그) 로 검증하고 `funclen.py` 로 길이를 측정한 값이다 — "기대 출력" 은 그 측정값. **`.bin` 대조는 실행자가 한다**(plan 작성 시 빌드하지 않았다).

---

## Global Constraints

spec §3 원문 — 헬퍼 추출 규칙(감사 B-3 실측 ○ 조건만). 하나라도 어기면 `.bin` 이 달라진 실측 사례가 있다.

- **H1** 헬퍼는 `static inline __attribute__((always_inline)) void` — 위반 시 실측: plain `static` → call 잔존 (+16~144 B)
- **H2** 헬퍼는 값을 반환하지 않고, 호출자는 헬퍼 결과로 분기하지 않는다 — `bool` 반환 → `movs r3,#1` 물질화 (+8~40 B)
- **H3** 판정 구조(`if` 조건·else-if 체인·`case` 라벨·`break`·`return`)는 **호출자에 남긴다**. 헬퍼에는 부수효과만 있는 직선 블록 — case 재그룹 → 이중 디스패치 ✗
- **H4** 인자는 **이미 존재하는 포인터·스칼라 값만**. 호출자 로컬 변수의 주소를 새로 잡아 넘기지 않는다 — `&save` → 스택 스필 (+56 B)
- **H5** 헬퍼는 같은 번역 단위에, 호출 함수 바로 위에 둔다. 파일-static 변수(`g_mb`, `s_stg`, `s_ren`, `g_reg`, `g_measure` 등)는 헬퍼가 직접 접근
- **H6** 헬퍼 안 `return` 은 **미실측** → 쓰지 않는다. early-return 이 있는 블록은 추출 대상에서 제외
- **H7** 슬라이스마다 두 모델 `.bin` sha256 대조가 **게이트**다. 규칙을 지켜도 리터럴 풀·레지스터 할당이 어긋날 수 있다 → 다르면 **되돌리고 보류**, 우회 시도 금지

spec §3 주석 이동 규칙(슬라이스 1):

- **C1** 함수 본문 안의 **5줄 이상** 주석 블록을 함수 선언 바로 위(기존 함수 헤더 주석과 합쳐 한 블록)로 옮긴다. 옮긴 자리에는 필요하면 한 줄 포인터(`/* 게이트 닫힘 분기 — 함수 헤더 §2 */`)만 남긴다
- **C2** 문장은 **삭제하지 않는다.** 예외 = `app_modbus.c` 의 이력 서술(날짜·커밋 해시·"구 주석은 ~라고 했는데" 류)로, changelog·spec 에 이미 있는 내용만 16줄 이상 압축한다. 압축한 문장은 커밋 메시지에 어느 문서에 있는지 적는다
- **C3** 코드 줄은 **한 글자도** 바꾸지 않는다(들여쓰기 포함). `git diff -w --ignore-blank-lines` 에서 코드 줄 변경 0 이어야 한다
- **C4** `.bin` 대조는 그대로 한다(라인 이동은 무영향이 실증됐지만 게이트는 유지)

기준 해시 (spec §5.1, 시작 커밋 `b61ef0f`, 클린 빌드, 툴체인 `arm-none-eabi-gcc 15.2.1 20251203`):

| 산출물 | sha256 | 크기 |
|---|---|---|
| STD `fw/build/gds_us_ctrl.bin` | `fd66f6e7bb9d6ff03f7ea1831797167813b3db4613de66e6c84841b1860e278f` | 66,696 B |
| REMOTE `fw/build-remote/gds_us_ctrl.bin` | `c5223adac03c85861e242e592125bf163114c847f383d6304f5d2e0771f7373f` | 67,008 B |

브랜치 기점: main tip `629bde2` (= `b61ef0f` + docs 2건; `fw/` 트리는 `b61ef0f` 와 동일 — Task 0 Step 1 이 확인한다). 소스 좌표는 전부 **`b61ef0f` 기준**이고, Task 2 이후는 "Task 1 후 좌표" 를 함께 적는다.

### 매 Task 공통 게이트 (spec §5.3) — 각 Task 의 Step 3 에 그대로 반복해 적어 두었다

```sh
./fw.sh 2>&1 | grep -E 'warning:|error:' | grep -v '/vendor/'                # 기대: 출력 없음 (our-code 경고 0)
MODEL=remote ./fw.sh 2>&1 | grep -E 'warning:|error:' | grep -v '/vendor/'   # 기대: 출력 없음
fw/tools/bin-same.sh   # 기대: SAME  fd66f6e7bb9d6ff03f7ea1831797167813b3db4613de66e6c84841b1860e278f c5223adac03c85861e242e592125bf163114c847f383d6304f5d2e0771f7373f
./fw.sh test 2>&1 | grep -ciE 'all (checks |tests )?passed'                  # 기대: 17
```

`/vendor/` 를 거르는 이유: `app_modbus_tcp.c` 가 포함하는 `fw/vendor/wiznet/Ethernet/socket.h:314/404/439` 의 `static … declared but never defined` 경고 3건은 **리팩토링 전부터** 있는 벤더 헤더 경고다(실 빌드 플래그로 원본 컴파일해 확인). our-code 경고만 0 을 요구한다.

### 이 plan 이 spec §4 제안과 다르게 잡은 것 (실행 전에 읽을 것 — 근거는 각 Task 에)

| # | spec §4 | 이 plan | 이유 |
|---|---|---|---|
| D1 | `handle_mo_time_edit(cfg, vp, data16)` — LV_MO_TIME1/2 본체 | `mo_time_clamp_echo(cfg)` — 두 case 의 **공통 꼬리 4줄**만 | 두 본체는 첫 줄(`limit_mo_time1 = data16` vs `limit_mo_time2 = data16`)이 달라 한 헬퍼로 담으려면 `vp` 분기를 **새로 만들어야** 한다(코드 추가 → H3/H7 위험). 대입은 case 에 남기고 공통 `if` 꼬리만 뽑는다 |
| D2 | `cfg_ctrl_commit_body(cfg, link)` | `cfg_ctrl_commit_body(cfg, d)` | 본체(`stg_apply_to_cfg` + ether 훅)는 `link` 를 안 쓰고 `d`(dirty 스냅샷, `uint16_t` 값)를 쓴다. `link` 를 넣으면 `-Wunused-parameter` |
| D3 | `publish_copy_out(now, active, freq_cal_val)` | `publish_copy_out(now, active, live, freq_cal_val)` | 본체 마지막 줄 `g_measure.us_on_status = live;` 가 `live` 를 쓴다(값 인자, H4 ✓) |
| D4 | `weld_step_cyl2(in, out)` | `weld_step_cyl2(out)` | CYL2 본체는 `in->` 를 한 번도 읽지 않는다 → `in` 은 `-Wunused-parameter` |
| D5 | `osc_step_wait_h(in)` / `osc_step_pulse(out)` 2개 | `osc_step_wait_h(in)` `osc_step_wait_l(in)` `osc_step_reset(out)` `osc_step_seek(out)` 4개 | RESET/SEEK 펄스 본체는 상수(`OSC_RESET_TICKS→OSC_SEEK` / `OSC_SEEK_TICKS→OSC_DONE`)가 달라 한 헬퍼가 못 담는다(상수를 인자로 바꾸면 코드 형태 변화). wait_h 1개만으로는 63줄 → wait_l 추가 |
| D6 | 슬라이스 1 = "5줄 이상 블록" 이동으로 21함수 ≤50 | 5줄 이상 블록(C1) → 부족하면 2~4줄 블록 → 부족하면 1줄 독립 주석까지 이동. 옮기는 블록 **바로 위 빈 줄**도 함께 제거. `parser_step`·`energy2str` 은 case/자릿수 구분 **빈 줄 각 4·3개 삭제** | 감사 C-1 의 "주석 재배치로 ≤50" 산식이 빈 줄과 <5줄 블록을 빼먹었다. 코드 줄은 C3 대로 무변경 — `git diff -w --ignore-blank-lines` 범위 안 |
| D7 | `app_modbus.c` 815→≤799 = 이력 16줄 이상 압축 | 이력 압축(K, 39줄) + **리플로(R)**: 문장 삭제 없이 ≤100 열로 재개행(19줄) + 빈 줄 1 → **753** | 슬라이스 2·4 의 헬퍼 6개가 파일에 **+39줄**을 더한다(시그니처·중괄호·1줄 주석·빈 줄). 16줄만 줄이면 최종 파일이 다시 815 가 된다. 리플로는 C2 가 금지한 "문장 삭제" 가 아니다 |
| D8 | `app_modbus_tick` 주석 27줄 재배치로 ≤50 | 이동 가능 17줄(트레일링 주석 연속줄 10줄은 C3 상 이동 불가) + 이력 압축 K11(−5) + `cfg` 선언 뒤 **빈 줄 1 삭제** → 50 | `mirror_live();   /* 🔴 …` 로 시작하는 트레일링 주석의 연속줄은 코드 줄(773)을 건드리지 않고는 옮길 수 없다 |
| D9 | §5.4 "50줄 초과 38 → ≤6" | 슬라이스 7 이 전부 SAME 이어도 **11** (Task 8 표) | `app_weld_tick`(95 코드줄)·`tcp_poll`(75)·`change_page`(177)·`disp_step`(55 + 함수-static) 은 spec 수단으로 ≤50 도달 불가. spec §6 의 4개 + `send_model_str`(미분할 결정) + `config_load` 에 이 넷과 헬퍼 `render_run_std`(53)·`weld_step_weld`(58) 가 더해진다 |
| D10 | 슬라이스 7 에 `hold_wdt_block()` 없음 (감사 C-1 제안만) | 넣지 않는다 | D8 로 `app_modbus_tick` 이 이미 50 |

---

## 파일 구조

**생성**

- `fw/tools/bin-same.sh` — 두 모델 빌드 → `.bin` sha256 을 `fw/.bin-baseline` 과 비교 (`baseline` 인자로 기록). spec §5.2 원문
- `fw/tools/funclen.py` — 중괄호 균형 함수 길이 측정기(40줄). 출력 `이름 줄수 (code 코드줄수)`
- `fw/.bin-baseline` — 기준 해시 2개 (git 추적 ✗)

**수정 — 도구/설정**

- `fw/.gitignore` — `.bin-baseline` 1줄 추가 (이 파일이 `build/`·`build-*/` 를 담고 있다; 루트 `.gitignore` 는 `fw/build/` 만 알고 `fw/build-remote/` 를 모른다)

**수정 — Task 1 (주석만, 18 파일)**

- `fw/src/app_modbus.c` — 이력 압축 K1~K11 + 리플로 R1~R11 + `app_modbus_tick`·`apply_config` 주석 헤더 이동 + 빈 줄 2 (815→753)
- `fw/src/app_reg.c` — `app_reg_command`·`app_reg_tick`
- `fw/src/app.c` — `app_loop_iter`·`app_init`
- `fw/src/app_remote_en_fsm.c` — `remote_en_fsm_step`
- `fw/src/app_input.c` — `app_input_tick`
- `fw/drivers/dgus_lcd.c` — `parser_step`
- `fw/src/app_overload.c` — `app_overload_tick`
- `fw/src/app_lcd_input.c` — `handle_key_multi`
- `fw/drivers/usart1.c` — `usart1_init`
- `fw/src/main.c` — `main`
- `fw/src/app_lcd_comm.c` — `data_save_commit`·`commit_comm_mode_and_ether`·`process_ip_char`
- `fw/src/app_cfg_stage.c` — `cfg_stage_commit`
- `fw/drivers/spi1.c` — `spi1_init`
- `fw/drivers/i2c1.c` — `i2c1_bus_unstick`
- `fw/src/app_lcd.c` — `app_lcd_init_mode`
- `fw/src/app_lcd_disp.c` — `app_lcd_disp_step`
- `fw/src/app_modbus_core.c` — `mb_core_decode`
- `fw/src/app_lcd_str.c` — `energy2str` (빈 줄 3만)

**수정 — Task 2~7 (헬퍼 추출)**

- `fw/src/app_modbus.c` — Task 2 `mirror_live` 3분할 / Task 4 `apply_writes` 본체 3추출
- `fw/src/app_reg.c` — Task 3 `reg_publish_measure` 3분할
- `fw/drivers/usart6_mb.c` — Task 3 `usart6_mb_open` 2분할
- `fw/src/app_lcd_input.c` — Task 5 `app_lcd_input_dispatch` 본체 4추출 / Task 7 `handle_std_setup_param` 2추출
- `fw/src/app_weld_fsm.c` `fw/src/app_osc_init_fsm.c` `fw/src/app_seek_reset_fsm.c` — Task 6
- `fw/src/app_lcd_render.c` `fw/src/app_weld.c` `fw/src/app_modbus_tcp.c` `fw/src/app_lcd_disp.c` — Task 7

**수정 — 문서 (Task 8)**

- `docs/changelog.md` — `[Unreleased]` 항목 1개
- `docs/superpowers/specs/2026-09-08-refactor-byte-identical-design.md` — §6 표에 슬라이스 7 결과 + 추가 예외 행

**건드리지 않음**: `fw/vendor/`, `ref/`, `fw/test/`, `fw/include/`, `CLAUDE.md`, `HANDOFF.md`, `NEXT_STEPS.md`, `fw/CMakeLists.txt`.

---

### Task 0: 준비 — 브랜치·게이트 스크립트·기준 해시

**Files:**
- Create: `fw/tools/bin-same.sh`
- Create: `fw/tools/funclen.py`
- Modify: `fw/.gitignore` (1줄 추가)

**Interfaces:**
- Produces: `fw/tools/bin-same.sh [baseline]` — 두 모델 빌드 후 `.bin` sha256 2개를 `fw/.bin-baseline` 과 비교. 출력 `SAME  <std> <remote>` / `DIFF  base=… cur=…`(exit 1) / `baseline: <std> <remote>`
- Produces: `python3 fw/tools/funclen.py FILE...` — 파일별 함수 길이. 한 함수 한 줄 `<이름> <줄수> (code <코드줄수>)`

- [ ] **Step 1: 브랜치 + fw/ 트리 동일성 확인**

```sh
cd /Users/tknoh/dev/work/gds_us_ctrl
git status --short            # 기대: 출력 없음 (또는 ?? docs/comm_protocol* 3건만 — 미추적 문서, 무관)
git checkout -b refactor/byte-identical main
git diff b61ef0f HEAD --stat -- fw/    # 기대: 출력 없음 (fw/ 는 b61ef0f 와 동일)
```

- [ ] **Step 2: `fw/tools/bin-same.sh` (spec §5.2 원문)**

```sh
mkdir -p fw/tools
cat > fw/tools/bin-same.sh <<'EOF'
#!/bin/sh
# 두 모델을 빌드해 .bin sha256 을 기준과 비교. 사용: bin-same.sh baseline | bin-same.sh
set -eu; cd "$(dirname "$0")/../.."
./fw.sh >/dev/null && MODEL=remote ./fw.sh >/dev/null
cur=$(shasum -a 256 fw/build/gds_us_ctrl.bin fw/build-remote/gds_us_ctrl.bin | cut -d' ' -f1 | paste -sd' ' -)
base=fw/.bin-baseline
[ "${1:-}" = baseline ] && { printf '%s\n' "$cur" >"$base"; echo "baseline: $cur"; exit 0; }
[ "$cur" = "$(cat "$base")" ] && echo "SAME  $cur" || { echo "DIFF  base=$(cat "$base")  cur=$cur"; exit 1; }
EOF
chmod +x fw/tools/bin-same.sh
```

- [ ] **Step 3: `fw/tools/funclen.py` (40줄 — 아래 전문)**

규약(감사 §0 와 동일): 줄수 = **함수명이 있는 선언자 줄 ~ 닫는 `}`** (주석·빈 줄 포함, 중괄호 균형). 코드줄 = 그 범위에서 주석·빈 줄을 뺀 줄(`#` 전처리 줄은 코드). 주석·문자열·문자 리터럴 안의 중괄호는 무시. `if/while/for/switch/return` 뒤의 `{` 와 초기화자·`typedef struct {` 는 함수가 아니다.

```python
#!/usr/bin/env python3
import re, sys

def strip(src):
    out, i, n = [], 0, len(src)
    while i < n:
        if src.startswith('//', i):
            j = src.find('\n', i); j = n if j < 0 else j
        elif src.startswith('/*', i):
            j = src.find('*/', i + 2); j = n if j < 0 else j + 2
        elif src[i] in '"\'':
            q, j = src[i], i + 1
            while j < n and src[j] != q:
                j += 2 if src[j] == '\\' else 1
            j = min(j + 1, n)
            out.append(q + re.sub(r'[^\n]', ' ', src[i + 1:j - 1]) + q); i = j; continue
        else:
            out.append(src[i]); i += 1; continue
        out.append(re.sub(r'[^\n]', ' ', src[i:j])); i = j
    return ''.join(out)

def report(path):
    s = strip(open(path, encoding='utf-8', errors='replace').read())
    lines = s.split('\n'); i, n = 0, len(s)
    while i < n:
        if s[i] != '{': i += 1; continue
        d, j = 0, i
        while j < n:
            d += (s[j] == '{') - (s[j] == '}')
            if d == 0: break
            j += 1
        k = i - 1
        while k >= 0 and s[k].isspace(): k -= 1
        m = re.search(r'([A-Za-z_]\w*)\s*\([^()]*(?:\([^()]*\)[^()]*)*\)\s*$', s[:k + 1])
        if k >= 0 and s[k] == ')' and m and m.group(1) not in ('if', 'while', 'for', 'switch', 'return'):
            a = s.count('\n', 0, m.start(1)) + 1; b = s.count('\n', 0, j) + 1
            code = sum(1 for ln in lines[a - 1:b] if ln.strip())
            print(f'{m.group(1)} {b - a + 1} (code {code})')
        i = j + 1

for p in sys.argv[1:]: report(p)
```

검증 — 감사 C-1 과 동일해야 한다:

```sh
python3 fw/tools/funclen.py fw/src/*.c fw/drivers/*.c | wc -l                    # 기대: 301
python3 fw/tools/funclen.py fw/src/*.c fw/drivers/*.c | awk '$2>50' | wc -l      # 기대: 38
python3 fw/tools/funclen.py fw/src/app_modbus.c | grep -E '^(mirror_live|app_modbus_apply_writes|app_modbus_tick) '
# 기대:
# mirror_live 108 (code 62)
# app_modbus_apply_writes 358 (code 191)
# app_modbus_tick 74 (code 45)
wc -l fw/src/app_modbus.c        # 기대: 815
```

- [ ] **Step 4: `.gitignore`**

```sh
printf '.bin-baseline\n' >> fw/.gitignore
tail -3 fw/.gitignore            # 기대 마지막 줄: .bin-baseline
```

- [ ] **Step 5: 기준 해시 기록 — 클린 빌드**

```sh
rm -rf fw/build fw/build-remote
fw/tools/bin-same.sh baseline
# 기대(한 줄, 정확히):
# baseline: fd66f6e7bb9d6ff03f7ea1831797167813b3db4613de66e6c84841b1860e278f c5223adac03c85861e242e592125bf163114c847f383d6304f5d2e0771f7373f
cat fw/.bin-baseline             # 같은 두 해시
arm-none-eabi-gcc --version | head -1   # 기대: ... 15.2.1 20251203
```

🔴 **불일치 시 중단·보고.** 리팩토링을 시작하지 않는다(spec §5.1: 툴체인 버전 차이가 첫 후보 — 위 `--version` 출력을 함께 보고). 기준 파일은 `fw/` 직속이므로 이후 `rm -rf fw/build*` 에도 남는다.

- [ ] **Step 6: 게이트 4종 리허설 (변경 0 상태에서 통과 확인)**

```sh
./fw.sh 2>&1 | grep -E 'warning:|error:' | grep -v '/vendor/'                # 기대: 출력 없음
MODEL=remote ./fw.sh 2>&1 | grep -E 'warning:|error:' | grep -v '/vendor/'   # 기대: 출력 없음
fw/tools/bin-same.sh   # 기대: SAME  fd66f6e7bb9d6ff03f7ea1831797167813b3db4613de66e6c84841b1860e278f c5223adac03c85861e242e592125bf163114c847f383d6304f5d2e0771f7373f
./fw.sh test 2>&1 | grep -ciE 'all (checks |tests )?passed'                  # 기대: 17
```

- [ ] **Step 7: Commit**

```sh
git add fw/tools/bin-same.sh fw/tools/funclen.py fw/.gitignore
git commit -m "chore(tools): .bin 동일성 게이트 + 함수 길이 측정기

- fw/tools/bin-same.sh: STD/REMOTE 빌드 → .bin sha256 을 fw/.bin-baseline 과 대조 (spec §5.2 원문)
- fw/tools/funclen.py: 중괄호 균형 함수 길이 (감사 C-1 규약, 301 함수 / >50 = 38 재현)
- fw/.gitignore: .bin-baseline
- 기준: STD fd66f6e7bb9d6ff03f7ea1831797167813b3db4613de66e6c84841b1860e278f / REMOTE c5223adac03c85861e242e592125bf163114c847f383d6304f5d2e0771f7373f (클린 빌드, gcc 15.2.1)"
```

---

### Task 1: 슬라이스 1 — 함수-내 주석 재배치 (spec 목록 23함수) + `app_modbus.c` 이력 압축·리플로

**Files:** (좌표 전부 `b61ef0f` 기준. **같은 파일에 대상이 둘 이상이면 아래쪽 함수부터** 처리한다 — 위쪽 함수의 헤더 병합이 닫는 ` */` 1줄을 더해 아래 좌표를 +1 밀기 때문. `app_modbus.c` 는 압축까지 있으므로 반드시 아래→위 순서.)
- Modify: `fw/src/app_modbus.c` (K1~K11, R1~R11, `app_modbus_tick`:742-815, `apply_config`:650-701)
- Modify: `fw/src/app_reg.c` (`app_reg_tick`:471-532 → `app_reg_command`:168-272)
- Modify: `fw/src/app.c` (`app_loop_iter`:99-186 → `app_init`:27-96)
- Modify: `fw/src/app_remote_en_fsm.c` (`remote_en_fsm_step`:19-89)
- Modify: `fw/src/app_input.c` (`app_input_tick`:36-100)
- Modify: `fw/drivers/dgus_lcd.c` (`parser_step`:199-261)
- Modify: `fw/src/app_overload.c` (`app_overload_tick`:37-98)
- Modify: `fw/src/app_lcd_input.c` (`handle_key_multi`:186-247)
- Modify: `fw/drivers/usart1.c` (`usart1_init`:22-77)
- Modify: `fw/src/main.c` (`main`:45-97)
- Modify: `fw/src/app_lcd_comm.c` (`data_save_commit`:346-397 → `commit_comm_mode_and_ether`:278-329 → `process_ip_char`:129-179)
- Modify: `fw/src/app_cfg_stage.c` (`cfg_stage_commit`:52-103)
- Modify: `fw/drivers/spi1.c` (`spi1_init`:58-108)
- Modify: `fw/drivers/i2c1.c` (`i2c1_bus_unstick`:26-76)
- Modify: `fw/src/app_lcd.c` (`app_lcd_init_mode`:198-248)
- Modify: `fw/src/app_lcd_disp.c` (`app_lcd_disp_step`:171-250)
- Modify: `fw/src/app_modbus_core.c` (`mb_core_decode`:172-223)
- Modify: `fw/src/app_lcd_str.c` (`energy2str`:67-119 — 빈 줄 3만)

**Interfaces:** 없음 (코드 무변경 — 헬퍼 0).

- [ ] **Step 1: 길이 측정(전)**

```sh
python3 fw/tools/funclen.py fw/src/*.c fw/drivers/*.c | grep -E '^(app_reg_command|app_loop_iter|app_modbus_tick|remote_en_fsm_step|app_init|app_input_tick|parser_step|app_reg_tick|app_overload_tick|handle_key_multi|usart1_init|main|apply_config|data_save_commit|commit_comm_mode_and_ether|cfg_stage_commit|spi1_init|i2c1_bus_unstick|app_lcd_init_mode|app_lcd_disp_step|energy2str|mb_core_decode|process_ip_char) '
```

기대(순서는 파일 glob 순):

```
app_loop_iter 88 (code 47)
app_init 70 (code 38)
cfg_stage_commit 52 (code 35)
app_input_tick 65 (code 41)
app_lcd_init_mode 51 (code 31)
data_save_commit 52 (code 36)
commit_comm_mode_and_ether 52 (code 37)
process_ip_char 51 (code 49)
app_lcd_disp_step 80 (code 55)
handle_key_multi 62 (code 34)
energy2str 53 (code 45)
apply_config 52 (code 41)
app_modbus_tick 74 (code 45)
mb_core_decode 52 (code 46)
app_overload_tick 62 (code 35)
app_reg_command 105 (code 48)
app_reg_tick 62 (code 44)
remote_en_fsm_step 71 (code 37)
main 53 (code 38)
parser_step 63 (code 50)
i2c1_bus_unstick 51 (code 41)
spi1_init 51 (code 41)
usart1_init 56 (code 44)
```

- [ ] **Step 2-A: 헤더 병합 규칙 (기계적 — 모든 함수 동일)**

1. 함수 헤더 1줄 주석 `/* 제목 */` → `/* 제목.` (닫는 `*/` 를 떼고 끝에 `.`).
2. 옮기는 블록의 각 줄을 순서대로: 앞 공백 제거 → 끝의 `*/` 제거 → 앞의 `/*` 또는 `*` 제거 → 앞에 ` *` 를 붙인다(줄 안 텍스트의 들여쓰기는 그대로). **블록 첫 줄에는 ` * [태그] ` 를 붙인다.** `*/` 만 있던 줄은 버린다. 원래 ` *` 만 있던 단락 구분줄은 ` *` 로 남긴다.
3. 마지막 블록 뒤에 ` */` 한 줄.
4. 함수 본문에서는 옮긴 블록과 표의 "동반 삭제 빈 줄" 을 지운다. **포인터 줄은 남기지 않는다**(C1 의 "필요하면" — 예산상 0). 코드 줄과 코드 줄 끝의 트레일링 주석은 한 글자도 건드리지 않는다(C3).

예 — `fw/src/app_overload.c:36` 헤더 `/* 과부하 10ms tick 처리 */` + 블록 :58-69 (태그 `active 블록`) 의 병합 결과(첫 3줄·끝 2줄):

```c
/* 과부하 10ms tick 처리.
 * [active 블록] force-stop을 active 레벨에서 매 tick 재시도. app_reg_measure()->
 * us_run_status는 app_reg_tick(step3) 발행 미러라 overload_tick(2.55)보다
 ...
 * weld 기계 사이클 abort는 별개(weld 물리트리거 dormant, 슬라이스 E/weld4).
 */
void app_overload_tick(void)
```

- [ ] **Step 2-B: 20개 함수 — 옮길 블록 표** (블록 = 독립 주석 줄만. 코드 줄 끝에서 시작하는 트레일링 주석의 연속줄은 대상이 아니다)

| 파일 | 함수 (`b61ef0f`) | 헤더 줄 | 옮길 블록 `줄-줄`(줄수) → 태그 | 동반 삭제 빈 줄 | 결과 |
|---|---|---|---|---|---|
| `fw/src/app_reg.c` | `app_reg_tick` :471-532 | :470 | 476-480(5)→`warm-up` · 504-506(3)→`setpoint MUX` · 517-519(3)→`publish` | 475, 503, 516 | 62→**48** |
| `fw/src/app_reg.c` | `app_reg_command` :168-272 | :167 | 172-177(6)→`START` · 181-185(5)→`START swallow` · 189-191(3)→`START guard` · 199-202(4)→`START on_time` · 204-205(2)→`START energy` · 211-237(27)→`RUN_RELEASE` · 245-248(4)→`RUN_RELEASE idle` · 254-258(5)→`SEEK/RESET` (265 의 1줄 `/* alien cmd 흡수 (no-op). */` 는 남긴다) | 없음 | 105→**49** |
| `fw/src/app.c` | `app_loop_iter` :99-186 | :98 | 110(1)→`step 2` · 113-115(3)→`step 2.5` · 118-119(2)→`step 2.55` · 122-124(3)→`step 2.57` · 127-130(4)→`step 2.58` · 133-135(3)→`step 2.6` · 138-140(3)→`step 3` · 155-156(2)→`step 4` · 159-160(2)→`step 5` · 163-164(2)→`step 6` · 180-181(2)→`step 6.5` (101·184 의 1줄은 남긴다) | 109, 112, 117, 121, 126, 132, 137, 154, 158, 162, 179 | 88→**50** |
| `fw/src/app.c` | `app_init` :27-96 | :26 | 34-39(6)→`rst 배너` · 49-53(5)→`LCD ready` · 57-60(4)→`logo dwell` · 92-94(3)→`boot_complete` (31-32·68-69 는 남긴다) | 48, 56, 91 | 70→**49** |
| `fw/src/app_remote_en_fsm.c` | `remote_en_fsm_step` :19-89 | :18 | 30-40(11)→`(2) E-STOP` · 53-60(8)→`(4) DIS_LINK` | 29, 52 | 71→**50** |
| `fw/src/app_input.c` | `app_input_tick` :36-100 | :35 | 54-57(4)→`estop 진입` · 73-77(5)→`estop active` · 91-95(5)→`명령 버튼` (64 는 63 트레일링의 연속줄 — 대상 아님) | 53, 90 | 65→**49** |
| `fw/drivers/dgus_lcd.c` | `parser_step` :199-261 | :198 | 201(1)→`개요` · 215(1)→`PS_GOT_5A` · 222(1)→`PS_GOT_HEADER` · 234(1)→`PS_COLLECTING` · 243-246(4)→`프레임 완성` · 258(1)→`unreachable` (215 를 빼면 `} else if (b == DGUS_SYNC1) {` 블록이 비게 된다 — 정상) | **case 구분 빈 줄 203, 209, 220, 232** (D6) | 63→**50** |
| `fw/src/app_overload.c` | `app_overload_tick` :37-98 | :36 | 58-69(12)→`active 블록` | 없음 | 62→**50** |
| `fw/src/app_lcd_input.c` | `handle_key_multi` :186-247 | :185 | 188-192(5)→`개요` · 223-238(16)→`data=0 토글` | 없음 | 62→**41** |
| `fw/drivers/usart1.c` | `usart1_init` :22-77 | :21 | 24(1)→`1` · 29(1)→`2` · 70-71(2)→`6` (38·51·67 은 남긴다) | 28, 69 | 56→**50** |
| `fw/src/main.c` | `main` :45-97 | :44 | 83-86(4)→`IWDG 기동` (57-59·64-65·72·79-81 은 트레일링 연속줄 — 대상 아님) | 82 | 53→**48** |
| `fw/src/app_lcd_comm.c` | `data_save_commit` :346-397 | :345 | 379-383(5)→`STD path` | 없음 | 52→**47** |
| `fw/src/app_lcd_comm.c` | `commit_comm_mode_and_ether` :278-329 | :277 | 280-284(5)→`개요` · 296-302(7)→`fix B 가드` | 295 | 52→**39** |
| `fw/src/app_lcd_comm.c` | `process_ip_char` :129-179 | :128 | 178(1)→`기타 키` | 없음 | 51→**50** |
| `fw/src/app_cfg_stage.c` | `cfg_stage_commit` :52-103 | :51 | 57-63(7)→`빈 커밋` | 없음 | 52→**45** |
| `fw/drivers/spi1.c` | `spi1_init` :58-108 | :57 | 66(1)→`SCK/MISO/MOSI` | 65 | 51→**49** |
| `fw/drivers/i2c1.c` | `i2c1_bus_unstick` :26-76 | :25 | 28-30(3)→`개요` | 없음 | 51→**48** |
| `fw/src/app_lcd.c` | `app_lcd_init_mode` :198-248 | :197 | 209-215(7)→`fix A 센티널` | 208 | 51→**43** |
| `fw/src/app_lcd_disp.c` | `app_lcd_disp_step` :171-250 | :170 | 173-187(15)→`step 표` · 195-197(3)→`ICON_RUN` · 204-206(3)→`DISP_REMOTE` | 194, 203 | 80→**57** (코드 55 — ≤50 불가, Task 8 §6 기록) |
| `fw/src/app_modbus_core.c` | `mb_core_decode` :172-223 | :171 | 197-198(2)→`memset` | 196 | 52→**49** |
| `fw/src/app_lcd_str.c` | `energy2str` :67-119 | — (주석 0) | 이동 없음 | **71, 73, 117** (D6: 선언 뒤·`first_zero=` 뒤·`return` 앞 빈 줄) | 53→**50** |

- [ ] **Step 2-C: `fw/src/app_modbus.c` — 주석 이동 2함수 + 이력 압축 K + 리플로 R**

(1) 헤더 이동(Step 2-A 규칙):

| 함수 | 헤더 줄 | 옮길 블록 → 태그 | 동반 삭제 빈 줄 | 결과 |
|---|---|---|---|---|
| `app_modbus_tick` :742-815 | :741 | 744-745(2)→`게이트 step` · 747-755(9, **R7 리플로 텍스트로 옮긴다**)→`hold 워치독` · 764(1)→`staging 타임아웃` · 770-771(2)→`RTU 분기` · 800-802(3)→`TCP 분기` (808 은 807 트레일링 연속줄 — 대상 아님) | 799, **768** (D8: `cfg` 선언 뒤 빈 줄) | 74→**50** |
| `apply_config` :650-701 | :649 | 678-681(4)→`해제 분기` | 없음 | 52→**48** |

(2) 압축 K(이력 문장 — C2) / 리플로 R(문장 무삭제 재개행 — D7). 각 항목: **원본 줄 범위를 아래 텍스트로 통째로 교체**한다(들여쓰기 포함, 글자 단위 그대로). 아래→위 순서로 적용하면 좌표가 유효하다.

**K11** `:774-782` (9줄, 773 `mirror_live();   /* 🔴 디코드 **앞**에서 미러한다 — 이 순서가 계약이다.` 의 연속줄. **773 은 그대로**) → 4줄. 소재: changelog 2026-09-05 "1-iteration stale-미러 레이스"

```c
                          * cfg 를 바꾸는 주체(LCD 입력·app_weld work_cnt++·DHCP)는 전부
                          * app_modbus_tick 보다 앞에 있다 — 미러가 tick 말미면 apply_writes 가
                          * 직전 iteration 의 holding[] 을 본다(changelog 2026-09-05 stale-미러).
                          * ⚠ 불변식: 이 지점부터 apply_writes 사이에 cfg 를 쓰는 코드를 넣지 말 것. */
```

**R7** `:747-755` (9줄) → 8줄 — 이 텍스트를 `app_modbus_tick` 헤더로 옮긴다(태그 `hold 워치독`)

```c
    /* hold 워치독 — 분기 밖 첫머리. RTU 점유/TCP/미점유 어디로 빠져도 시간이 흘러야 한다:
     * apply_config 가 링크를 해제해도 hold 런은 T 안에 서야 한다.
     * 🔴 불변식(spec §4 ②): 연속한 두 step 사이에 us_run_status 를 US_COMM 으로 바꿀 수 있는 것은
     * 같은 tick 의 apply_writes **1건**뿐이다(RTU = tick 당 1 프레임, TCP = poll 당 FC06 1건 —
     * app_modbus_tcp.c 의 break). tick 당 FC06 apply 를 2건으로 늘리면 "정지+재시작" 이 한 관측
     * 구간에 들어가 다른 마스터의 탭 런이 hold 세션을 상속받는다 — 그 변경은 이 워치독을 함께
     * 고쳐야 한다. step 은 반드시 apply_writes 보다 **앞**이어야 한다 — 뒤집히면 같은 tick 의
     * keep 이 now_hwd 보다 늦은 시각을 남겨 unsigned 뺄셈이 ≈4.29e9 가 되고 즉시 오트립한다. */
```

**K10** `:618-628` (11줄) → 6줄. 소재: changelog 2026-09-05 "stale-미러 레이스" (예전 mirror_live 위치·2026-09-05 이동 이력)

```c
        /* staged 스캔 — 예약 영역 쓰기를 staging 으로 흡수한다.
         * 🔴 전수 비교(holding != 기대값)는 **stale 미러를 staged 편집으로 오인한다** — 미러가
         * tick 말미에 돌던 시절 아무 FC06 이 옛값을 staged 로 잡아 커밋이 cfg·FRAM 에 되썼다
         * (changelog 2026-09-05 stale-미러). 미러를 디코드 앞으로 옮겨 창은 닫혔지만 **이 가드는
         * 방어선으로 남긴다** — 미러 순서는 app_modbus_tick 의 불변식이고 여기서 재확인할 수단이 없다.
         * "마스터가 이번에 실제로 쓴 주소"만 본다 — 코어가 기록해 준다(mb_core 거동은 그대로). */
```

**K9** `:596-613` (18줄) → 7줄. 소재: changelog 2026-09-04 (`0ab2608` 32비트 비교, 사용자 승인) + 2026-09-05 "WORK_CNTL 가짜 리셋"

```c
        /* CNTL=0 write = work counter reset (samd20 main.c:4539: cfg + FRAM + LCD refresh).
         * ⚠ samd20 이탈(사용자 승인, changelog 2026-09-04 `0ab2608`): 원본은 하위 워드만 비교해
         * work_cnt 가 65536 의 배수일 때 리셋이 조용히 무시됐다 — LCD 경로(app_lcd_input.c:385)처럼
         * 32비트 전체를 비교한다. 거동 차이는 그 한 점에서만, "조용히 실패 → 정상 동작" 방향.
         * 🔴 판정은 mb_work_cnt_reset_req() — 32비트 비교만 하면 **미러가 만든 0**(65536 배수의
         * 하위-워드 미러)과 마스터가 쓴 0 이 구분되지 않아 아무 FC06 이나 카운터를 날렸다
         * (changelog 2026-09-05 가짜 리셋). 술어가 last_write_addr 로 가른다. */
```

**R4** `:571-583` (13줄) → 11줄 (안의 이력 "가드를 두지 않는 것은 사용자 결정이다(2026-09-04): … 일관된다." 4줄 → 2줄 = K12; 소재 changelog 2026-09-04 B-5 "가드 없음 = LCD 경로와 동형(사용자 결정)". 나머지는 리플로만)

```c
        /* B-5 모델 타입. 위와 동형이나 🔴 **부작용이 하나 더 있다**: PC11 의 의미가 model_type 으로
         * 뒤바뀐다(app_input_fsm.c:46-60).
         *   <=1 (hand/multi) -> PC11 = B_SEEK (active-LOW)
         *   ==2 (std)        -> PC11 = EMSW  (active-HIGH 레벨추종)
         * 따라서 이 쓰기 하나가
         *   0/1 -> 2 : PC11 이 HIGH 면 즉시 E-stop 진입 + SOL 강제 OFF
         *   2 -> 0/1 : E-stop 활성 중이면 s_estop_active 가 0 으로 클리어
         * 를 일으킨다. **가드를 두지 않는 것은 사용자 결정**(changelog 2026-09-04 B-5) — LCD 편집
         * 경로(app_lcd_input.c:452)에도 가드가 없어, 원격에만 새 규칙을 만들지 않는다.
         * ⚠ 남는 차이: 원격 조작자는 기계 앞에 없을 수 있다. 거부가 필요해지면
         * app_estop_active() || us_on_status 로 막는 것이 그 자리다. */
```

**R5** `:560-566` (7줄) → 6줄 (리플로만)

```c
        /* B-5 모델 주파수. LCD 편집 경로(app_lcd_input.c:447-450)와 **정확히 동형**: cfg 설정 +
         * 모델명 문자열 갱신이 전부다. sys_mode·런페이지·출력바 임계(ref_lv_*)는 여기서 재파생하지
         * 않는다 — LCD 도 그렇고, 다음 app_lcd_init_mode()(부팅 / SYS_PIC_NOW)에서 갱신된다.
         * 범위 클램프 없음: LCD 에 없는 규칙을 원격에만 발명하지 않는다(사용자 결정). 범위 밖 값은
         * 안전하게 퇴화한다 — send_model_str 은 switch+default(배열 인덱싱 ✗), run_page/ref_lv_* 도
         * else 분기를 갖는다. */
```

**R6** `:550-557` (8줄) → 6줄 (리플로만)

```c
        /* B-4 조작. cfg 가 아니라 비영속 RAM 상태라 save 하지 않는다 — "재부팅 시 소실"은 설계이지
         * 누락이 아니다(원격기에도 그렇게 알렸다). 모드를 켜는 것만으로는 아무것도 움직이지 않는다:
         * 솔레노이드를 실제로 토글하는 것은 기계 앞 조작자의 양손 START 다. 그래서 요구사항이 이것을
         * "일반 설정과 같은 급"으로 분류했다. ⚠ 전이 시 솔레노이드는 무조건 OFF 된다(app_horn_set_mode
         * 안, legacy 3459/3468). horn 모드가 켜지면 모든 소스의 START 가 차단되며, 그 사실은 STATUS 의
         * HORN 비트로 원격에서 읽힌다. */
```

**K8** `:529-539` (11줄) → 6줄. 소재: changelog 2026-09-04 "C-2 EN_SAFTY 0/1 정규화 (`c5c2f7e`) — samd20 이탈, 사용자 승인" (압축한 문장: "이 포트도 그걸 의식적으로 충실 복제하고 있었다", "즉 두 편집 경로가 갈려 있었고, 이 변경은 '원격에만 새 규칙을 발명'하는 것이 아니라 유일하게…")

```c
        /* C-2 (2026-08-30 요구사항): 0/1 정규화.
         * ⚠ samd20 이탈 — 원본 comm 경로는 as-is 저장(main.c:4533), 사용자 승인 후 변경
         * (changelog 2026-09-04 `c5c2f7e`). 근거: LCD 경로는 이미 정규화한다
         * (app_lcd_input.c:518) — 어긋나 있던 Modbus 를 LCD 에 맞춘 것.
         * 기능 동작은 불변 — 소비자(weld trigger FSM)는 != 0 판정이다.
         * 달라지는 것은 read-back 값·FRAM 저장값·DISP_SAFTY 로 보내는 값. */
```

**K7** `:423-428` (6줄) → 4줄 (426-428 "DG-12 로 … TCP 응답 유실 시나리오 자체가 없다 — spec §7 의 500ms 지연 상수는 근거가 사라져 도입하지 않는다(T-1 재확인 결과)." 3줄 → 1줄). 소재: changelog 2026-09-04 F-A "`CFG_ETHER_APPLY_DELAY_MS` 는 폐기"

```c
                    /* LCD SAVE 와 같은 훅을 재사용 — app_eth_tick 이 dirty 를
                     * consume 해 재적용한다. RTU 는 응답을 blocking 으로 먼저
                     * 보내고 나서 apply 를 부르므로(send → apply 순서, 아래 tick)
                     * 지연이 불필요하다. DG-12 로 ether 커밋은 RTU 로만 온다(500ms 지연 상수 폐기 = changelog 2026-09-04 F-A). */
```

**K6** `:383-389` (7줄) → 4줄 (386-389 "구 가드 … 2026-06-12 리뷰 NOTE … 2026-06-28 … (2026-09-04 fix)" 4줄 → 1줄). 소재: changelog 2026-09-04 "`ac7e691` fix(modbus) 원격 START 진폭 pot write 복원"

```c
            /* samd20 comm START 는 같은 자리에서 진폭 pot 을 쓴다(main.c:4400-4401).
             * LCD RUN-press 경로(app_lcd_input.c:217/242)와 동형 — 무조건 write.
             * 거부된 START 여도 출력이 없어 무해(멱등 1바이트).
             * (구 `us_run_status == US_COMM` 가드는 구조적으로 항상 FALSE 였다 — changelog 2026-09-04 `ac7e691`.) */
```

**K5** `:365-367` (3줄) → 2줄. 소재: changelog 2026-07-05 "feat(seek-reset) `29803ae`"

```c
        /* app_reg_command 가 app_seek_reset 에 위임: RESET→SEEK 자동 체인 + 물리 OSC 구동 +
         * fault 클리어 (changelog 2026-07-05 seek-reset `29803ae`). */
```

**R1** `:299-320` (22줄) → 14줄 (리플로만 — 단락 구분 ` *` 4줄 제거, 문장 전부 유지)

```c
    /* 원격 활성화 게이트 (spec §5.3). 닫혀 있으면 명령 3종은 디스패치 없이 소거하고, STOP만
     * 통과시킨 뒤 return으로 cfg 체인 전체를 건너뛴다.
     * ⚠ 소거는 생략 불가 — 명령 레지스터 0x19~0x1C는 미러 대상이 아니라서(mirror_live 위쪽 전수)
     * 무시만 하면 1이 홀딩에 잔류하고, 게이트가 열린 뒤 아무 FC06이나 도착하는 순간 아래 체인이
     * 그 stale START를 디스패치한다. 값 불문 무조건 0 — 1 이외 값도 잔류물을 남기지 않는다.
     * STOP을 아래 기존 분기에 맡기지 않고 여기 복제하는 이유: cfg 쓰기 거부의 유일한 장치가 이
     * return이라, STOP을 fall-through 시키려면 return을 포기해야 하고 그러면 "게이트 닫힘 + cfg
     * 반영"이라는 모순이 생긴다.
     * cfg 거부에 별도 조치가 없는 것은 의도 — 체인을 건너뛰면 다음 tick의 mirror_live()가 holding을
     * cfg 값으로 되돌리므로 원격기 read-back이 불일치를 본다 (예외 응답 없음 = samd20 계약 동형).
     * RTU/TCP가 이 함수를 공유하므로 여기 1곳이 양 전송로 전부다.
     * ⚠ REMOTE_EN_GATE_BYPASS는 T-5(LCD 활성화 조작)가 없는 동안의 한시적 벤치 탈출구다. 게이트를
     * 켤 수단이 아직 없어 기본 빌드는 모든 원격 명령을 막고, 그러면 이 repo의 HW 검증이 의존하는
     * mbpoll 흐름이 죽는다. T-5 머지 시 이 #ifdef와 CMake 옵션을 함께 제거할 것. */
```

**R2** `:270-279` (10줄) → 7줄 (리플로만)

```c
    /* 원격 게이트 미러. CAP는 매직 무조건 복원 = capability probe의 신-펌웨어 판별점("read-only는
     * 미러가 덮음" — MODEL_FREQ/TYPE 위와 동형). 0x2D는 예약이라 미러하지 않는다. 조건 없이 함수
     * 말미에 두어야 세 호출처(apply_config RTU 획득 / tick RTU / tick TCP)가 전부 커버되고, 링크
     * 전이의 mb_core_init 0-리셋도 같은 tick에 즉시 복원된다.
     * ⚠ STD 는 미러하지 않는다 — 인터록이 없는데 CAP 매직을 실으면 원격기가 "이 컨트롤러는 게이트를
     * 지원한다"고 오판한다(A-7의 판별이 정확히 이것). 미러가 없으면 0x2A 는 원격기가 쓴 probe 값 P
     * 가 그대로 남아 구-펌웨어와 같은 판정을 받는다 = 의도한 동작. */
```

**R3** `:227-236` (10줄, 주석 2개) → 7줄 (리플로만 — 두 주석을 한 블록으로)

```c
    /* STATUS bit0 = run active (spec §3.1: us_run_status != US_IDLE). OVTIME = app_reg가 publish한
     * energy 모드 직접런 과대시간 fault(2026-06-28-ovtime spec). OVLD = app_overload_active() 라이브
     * 반영(슬라이스 C). ESTOP = app_estop_active()(슬라이스 D). OUTERR는 6b. SENSOR/HORN = 원격
     * 관측용 신규 비트(요구사항 B-3/B-4). 비트 배치는 mb_status_bits()가 소유 — host 스위트가
     * 겹침·극성까지 고정한다. SEEK/RESET 은 FSM 단일 상태라 두 비트가 동시에 서지 않는다. 글루
     * (app_seek_reset.c)를 거치지 않고 순수 FSM 상태를 직접 읽는다 — 글루가 노출하는 것은 active
     * (직교 판정용) 뿐이고 leg 구분이 없다. app_remote_en_fsm 을 같은 방식으로 읽는 선례가 위에 있다. */
```

**K3** `:214-217` (4줄) → 2줄. 소재: changelog 2026-09-04 B-5 (`deb48bb`) — 압축 문장 "예전에 쓰기가 안 먹은 진짜 원인은 미러가 아니라 apply 체인에 분기가 없어서 값이 무시된 것이었다"

```c
    /* B-5: R/W 다 — 쓰기는 apply 체인이 받고 미러가 결과를 되비춘다(다른 cfg 필드와
     * 동형). 구 "쓰기 안 먹음"의 원인은 apply 체인 분기 부재였다(changelog 2026-09-04 B-5). */
```

**K2** `:182-184` (3줄) → 2줄. 소재: changelog 2026-06-13 `349bd91` "§Deviations 6"

```c
    /* samd20 update_holding_reg(0): live values -> holding mirror. Runs every owned
     * tick — fresher than samd20's post-message refresh, normalizes clamped writes. */
```

**R11** `:154-156` (3줄) → 2줄 (리플로만)

```c
    /* 침묵 입력 = REMOTE 아이콘과 같은 스탬프. note_remote가 유효 디코드 전부에 찍히므로 읽기 요청도
     * 링크 생존 신호다. 진입 이전 값일 수 있으나 FSM의 무장 규칙이 걸러낸다. MB_REMOTE_HOLD_MS와는 무관. */
```

**R10** `:121-126` (6줄) → 4줄 (리플로만 — 단락 ` *` 1줄 제거)

```c
/* 게이트 FSM 1 tick. MODEL_STD 에는 인터록 스위치가 없다 — 게이트를 넣으면 스위치 미장착 유닛에서
 * 유선 Modbus HMI 의 설정 쓰기가 죽는다. 그래서 STD 는 게이트 자체를 두지 않고 상시 개방으로
 * 고정한다. 이 #if 하나가 apply_writes 쪽 분기를 대신하므로 아래 게이트 검사는 두 모델 공통 코드로
 * 남는다 (분기 확산 방지). */
```

**R9** `:51-54` (4줄) → 3줄 (리플로만)

```c
/* 원격 활성화 게이트 상태 (비영속 — holding[]은 링크 전이의 mb_core_init이 0으로 지우고 FRAM 저장은
 * 요구사항 위반이라, 파일 static만 가능). s_ren = 마지막 step 출력 캐시(미러 + apply 게이트가 소비).
 * 조작 입력은 PC8 물리 스위치 레벨뿐이라 1-shot 래치가 필요 없다. */
```

**K1** `:1-10` (10줄, 파일 헤더) → 6줄. 소재: changelog 2026-06-13 `349bd91` (DELAY3→ADDR_TRIGGER2 복붙버그 구조적 해소 · §Deviations 5)

```c
/* fw/src/app_modbus.c — samd20 Modbus slave integration port (spec §3~§5).
 * Mirror pass = update_holding_reg(0) field-for-field; write-apply pass =
 * update_holding_reg(1) one-change-per-message else-if chain with the samd20
 * clamps. FRAM persistence = whole-map app_config_save_all. Occupancy switching =
 * per-tick cfg compare of (comm_mode==SERIAL)&&(addr!=0) — tick-polled, not hook-driven,
 * so no app_lcd<->app_modbus include cycle forms (이력 = changelog 2026-06-13 `349bd91`). */
```

**압축하지 않는 것**(C2 밖): `:133-148` 🔴 PC8 극성 블록(배포 금지 경고 — 원문 유지), `:432-439` serial 그룹 노트(정정문 — spec §5.6 U-1 행이 아직 옛 서술이라 유일한 정본), `:323-327` VR-3 벤치 노트, `:299-320` 의 문장(리플로만).

- [ ] **Step 3: 게이트**

```sh
./fw.sh 2>&1 | grep -E 'warning:|error:' | grep -v '/vendor/'                # 기대: 출력 없음
MODEL=remote ./fw.sh 2>&1 | grep -E 'warning:|error:' | grep -v '/vendor/'   # 기대: 출력 없음
fw/tools/bin-same.sh   # 기대: SAME  fd66f6e7bb9d6ff03f7ea1831797167813b3db4613de66e6c84841b1860e278f c5223adac03c85861e242e592125bf163114c847f383d6304f5d2e0771f7373f
./fw.sh test 2>&1 | grep -ciE 'all (checks |tests )?passed'                  # 기대: 17
```

C3 검증(코드 줄 변경 0) — 주석·빈 줄을 벗긴 코드 스트림이 Task 0 커밋과 동일해야 한다. `<T0>` = Task 0 커밋 해시:

```sh
T0=$(git rev-parse --short HEAD)   # Task 1 커밋 전이므로 HEAD = Task 0 커밋
for f in $(git diff --name-only $T0 -- fw/src fw/drivers); do
  diff <(git show $T0:$f | arm-none-eabi-gcc -fpreprocessed -dD -E -P -x c - | grep -v '^[[:space:]]*$') \
       <(cat $f          | arm-none-eabi-gcc -fpreprocessed -dD -E -P -x c - | grep -v '^[[:space:]]*$') >/dev/null \
    && echo "SAME_CODE $f" || echo "CODE CHANGED: $f"
done
```

기대: `SAME_CODE` 18줄(위 Files 목록의 18 파일), `CODE CHANGED` 0. (spec §5.3 의 `git diff -w --ignore-blank-lines $T0 -- fw/` 도 보되, 큰 주석 블록이 함수 시그니처 근처로 이동하면 git 이 hunk 를 잘못 맞춰 **동일한 코드 줄이 −/+ 쌍으로** 나올 수 있다 — 위 스트림 비교가 판정이다.)

- [ ] **Step 4: DIFF 면** — 슬라이스 1 은 라인 이동만이라 `.bin` 이 달라질 수 없다(감사 A-2 실증). `DIFF` 가 나오면 코드 줄이 건드려진 것이다: 위 C3 루프가 가리키는 파일을 `git checkout -- <파일>` 로 되돌리고 그 파일만 다시 한다. 우회·추측 금지.

- [ ] **Step 5: 길이 측정(후)**

```sh
python3 fw/tools/funclen.py fw/src/*.c fw/drivers/*.c | grep -E '^(app_reg_command|app_loop_iter|app_modbus_tick|remote_en_fsm_step|app_init|app_input_tick|parser_step|app_reg_tick|app_overload_tick|handle_key_multi|usart1_init|main|apply_config|data_save_commit|commit_comm_mode_and_ether|cfg_stage_commit|spi1_init|i2c1_bus_unstick|app_lcd_init_mode|app_lcd_disp_step|energy2str|mb_core_decode|process_ip_char|mirror_live|app_modbus_apply_writes) '
```

기대:

```
app_loop_iter 50 (code 47)
app_init 49 (code 38)
cfg_stage_commit 45 (code 35)
app_input_tick 49 (code 41)
app_lcd_init_mode 43 (code 31)
data_save_commit 47 (code 36)
commit_comm_mode_and_ether 39 (code 37)
process_ip_char 50 (code 49)
app_lcd_disp_step 57 (code 55)
handle_key_multi 41 (code 34)
energy2str 50 (code 45)
mirror_live 99 (code 62)
app_modbus_apply_writes 318 (code 191)
apply_config 48 (code 41)
app_modbus_tick 50 (code 45)
mb_core_decode 49 (code 46)
app_overload_tick 50 (code 35)
app_reg_command 49 (code 48)
app_reg_tick 48 (code 44)
remote_en_fsm_step 50 (code 37)
main 48 (code 38)
parser_step 50 (code 50)
i2c1_bus_unstick 48 (code 41)
spi1_init 49 (code 41)
usart1_init 50 (code 44)
```

```sh
wc -l fw/src/app_modbus.c                                                     # 기대: 753
python3 fw/tools/funclen.py fw/src/*.c fw/drivers/*.c | awk '$2>50' | wc -l   # 기대: 16 (38 − 22; disp_step 57 잔존)
```

- [ ] **Step 6: Commit**

```sh
git add fw/src fw/drivers
git commit -m "refactor(comments): 함수-내 장문 주석 헤더 이동(22함수) + app_modbus.c 이력 압축 815→753 — .bin 동일 (STD fd66…e278f / REMOTE c522…7373f)

- C1: 5줄 이상 블록(+ ≤50 도달에 필요한 2~4줄·1줄 블록)을 함수 선언 위 헤더 블록으로 병합. 코드 줄 무변경
  (주석·빈 줄 제거 스트림 diff 18파일 SAME_CODE). 빈 줄 삭제: 옮긴 블록 바로 위 + parser_step 4·energy2str 3·app_modbus_tick 1
- C2 압축(app_modbus.c): K1 파일 헤더(changelog 2026-06-13 349bd91) · K2 mirror_live 개요(동) · K3 B-5 미러(changelog
  2026-09-04 B-5) · K5 RESET 위임(changelog 2026-07-05 29803ae) · K6 START pot(changelog 2026-09-04 ac7e691) ·
  K7 ether 훅 500ms(changelog 2026-09-04 F-A) · K8 C-2 f_safty(changelog 2026-09-04 c5c2f7e) · K9 work_cnt 리셋
  (changelog 2026-09-04 0ab2608 + 2026-09-05 가짜 리셋) · K10 staged 스캔(changelog 2026-09-05 stale-미러) ·
  K11 mirror 순서(동) · K12 model_type 가드(changelog 2026-09-04 B-5)
- 리플로 R1~R7·R9~R11: 문장 무삭제, ≤100열 재개행 (헬퍼 6개 +39줄을 상쇄해 최종 ≤799 유지)
- 결과: 21함수 ≤50 (app_lcd_disp_step 57 = 코드 55, §6 기록). >50 함수 38→16"
```

---

### Task 2: 슬라이스 2 — `mirror_live` 3분할

**Files:**
- Modify: `fw/src/app_modbus.c` — Task 1 후 `mirror_live` :172-270 (헤더 `/* live 값 mirror */` :171; `b61ef0f` :180-287). 헬퍼 3개는 :171 바로 위에 삽입

**Interfaces:**
- Produces: `static inline __attribute__((always_inline)) void mirror_cfg_fields(const app_config_t *cfg)` — `WORK_CNTH`~`TIMEOVER` 15개 cfg 필드 대입 (Task 1 후 :180-194). H4 ✓ (인자 = 기존 `cfg` 포인터). H5 ✓ (`g_mb` 직접)
- Produces: `static inline __attribute__((always_inline)) void mirror_disp_status(const app_config_t *cfg, const lcd_measure_t *m, uint8_t running)` — DISP_* 4개 + B-5 cfg 6개 + CAL 2개 + STATUS 합성 (Task 1 후 :195-234; `disp_on`·`sr`·`sin` 은 헬퍼 로컬이 된다 — `&sin` 은 원본에도 있던 헬퍼-로컬 주소, H4 의 "호출자 로컬 주소" 가 아니다). H4 ✓ (`cfg`·`m` 기존 포인터, `running` 값)
- Produces: `static inline __attribute__((always_inline)) void mirror_stage_and_gate(const app_config_t *cfg)` — COMM_MODE·CFG_STAT·CFG_CAP·FEAT_CAP·HORN_CMD + staged 루프 + `#if defined(MODEL_REMOTE)` 게이트 미러 (Task 1 후 :236-269). H4 ✓. H5 ✓ (`s_stg`·`s_ren`·`k_stg_reg`·`stg_mirror_val` 직접)
- spec §4 표와 다른 점: spec 은 `mirror_cfg_fields` 에 "WORK_CNT~EN_SAFTY·CAL" 을 넣었지만 그 사이에 DISP_* 4줄(`m` 사용)이 끼어 있어 **연속 블록이 아니다**. 문장 순서를 바꾸지 않기 위해 연속 범위로 자른다: cfg_fields = :180-194(TIMEOVER 까지), disp_status = :195-234(DISP_*·B-5 cfg·CAL·STATUS). 헬퍼 이름·인자는 spec 그대로

- [ ] **Step 1: 길이 측정(전)**

```sh
python3 fw/tools/funclen.py fw/src/app_modbus.c | grep -E '^mirror_live '    # 기대: mirror_live 99 (code 62)
wc -l fw/src/app_modbus.c                                                     # 기대: 753
```

- [ ] **Step 2: 헬퍼 추출**

**before** (Task 1 후 :171-270 — 본문 세 구간의 첫·끝 줄만; 각 구간의 전문은 아래 after 의 헬퍼 본문과 글자 단위 동일):

```c
/* live 값 mirror */
static void mirror_live(void)
{
    /* samd20 update_holding_reg(0): live values -> holding mirror. Runs every owned
     * tick — fresher than samd20's post-message refresh, normalizes clamped writes. */
    const app_config_t  *cfg = app_lcd_cfg();
    const lcd_measure_t *m   = app_lcd_measure();
    uint8_t running = (m->us_run_status != (uint8_t)US_IDLE) ? 1u : 0u;

    g_mb.holding[MB_REG_WORK_CNTH]   = (uint16_t)(cfg->work_cnt >> 16);
    ⋮   (:180-194, 15줄 → mirror_cfg_fields 본문)
    g_mb.holding[MB_REG_TIMEOVER]    = cfg->limit_out_time;
    /* DISP_*: live shows the running peak, stopped shows the latched last
    ⋮   (:195-234, 40줄 → mirror_disp_status 본문)
    g_mb.holding[MB_REG_STATUS]      = mb_status_bits(&sin);

    /* F-A comm/eth 미러. COMM_MODE·CFG_STAT 는 무조건, staged 9종은 **비-dirty 일
    ⋮   (:236-269, 34줄 → mirror_stage_and_gate 본문)
#endif
}
```

**after** (:171-270 을 아래 전체로 교체 — 헬퍼 3개 + 헤더 + 새 `mirror_live`):

```c
/* mirror_live 본체 1/3 — cfg 필드(WORK_CNT~TIMEOVER)를 holding 에 대입 */
static inline __attribute__((always_inline)) void mirror_cfg_fields(const app_config_t *cfg)
{
    g_mb.holding[MB_REG_WORK_CNTH]   = (uint16_t)(cfg->work_cnt >> 16);
    g_mb.holding[MB_REG_WORK_CNTL]   = (uint16_t)(cfg->work_cnt);
    g_mb.holding[MB_REG_DELAY1]      = cfg->limit_delay_time1;
    g_mb.holding[MB_REG_DELAY2]      = cfg->limit_delay_time2;
    g_mb.holding[MB_REG_DELAY3]      = cfg->limit_delay_time3;
    g_mb.holding[MB_REG_TRIGGER2]    = cfg->limit_trigger_time2;
    g_mb.holding[MB_REG_TRIGGER3]    = cfg->limit_trigger_time3;
    g_mb.holding[MB_REG_OUT_POWER]   = cfg->output_power;
    g_mb.holding[MB_REG_ON_TIME]     = cfg->limit_on_time;
    g_mb.holding[MB_REG_ENERGY]      = (uint16_t)cfg->limit_energy;
    g_mb.holding[MB_REG_MULTI_T1]    = cfg->limit_mo_time1;
    g_mb.holding[MB_REG_MULTI_T2]    = cfg->limit_mo_time2;
    g_mb.holding[MB_REG_MULTI_O1]    = cfg->limit_mo_out1;
    g_mb.holding[MB_REG_MULTI_O2]    = cfg->limit_mo_out2;
    g_mb.holding[MB_REG_TIMEOVER]    = cfg->limit_out_time;
}

/* mirror_live 본체 2/3 — DISP_* 4개 + B-5·CAL 미러 + STATUS 비트 합성 */
static inline __attribute__((always_inline)) void mirror_disp_status(const app_config_t *cfg,
                                                  const lcd_measure_t *m, uint8_t running)
{
    /* DISP_*: live shows the running peak, stopped shows the latched last
     * (samd20 main.c:4564-4567 us_on_status mirror). us_on_status = run OR
     * seek/reset active — SEEK/RESET 중에도 라이브 (legacy 4253/4280). STATUS
     * bit0(아래 running)는 run 전용 유지. */
    uint8_t disp_on = m->us_on_status;
    g_mb.holding[MB_REG_DISP_POWER]  = disp_on ? m->max_power : m->last_power;
    g_mb.holding[MB_REG_DISP_AMP]    = disp_on ? m->max_amp   : m->last_amp;
    g_mb.holding[MB_REG_DISP_FREQ]   = disp_on ? m->curr_freq : m->last_freq;
    g_mb.holding[MB_REG_DISP_ENERGY] = disp_on ? (uint16_t)m->curr_energy
                                               : (uint16_t)m->last_energy;
    /* B-5: R/W 다 — 쓰기는 apply 체인이 받고 미러가 결과를 되비춘다(다른 cfg 필드와
     * 동형). 구 "쓰기 안 먹음"의 원인은 apply 체인 분기 부재였다(changelog 2026-09-04 B-5). */
    g_mb.holding[MB_REG_MODEL_FREQ]  = cfg->model_freq;
    g_mb.holding[MB_REG_MODEL_TYPE]  = cfg->model_type;
    g_mb.holding[MB_REG_RUN_MODE]    = cfg->run_mode;
    g_mb.holding[MB_REG_EN_ENERGY]   = cfg->energy_ctrl ? 1u : 0u;
    g_mb.holding[MB_REG_EN_MULTI]    = cfg->multi_ctrl  ? 1u : 0u;
    g_mb.holding[MB_REG_EN_SAFTY]    = cfg->f_safty;
    /* B-2 calibration — int16 를 2의 보수 그대로 싣는다 (C-1). */
    g_mb.holding[MB_REG_CAL_VAL]      = (uint16_t)cfg->cal_val;
    g_mb.holding[MB_REG_FREQ_CAL_VAL] = (uint16_t)cfg->freq_cal_val;
    /* STATUS bit0 = run active (spec §3.1: us_run_status != US_IDLE). OVTIME = app_reg가 publish한
     * energy 모드 직접런 과대시간 fault(2026-06-28-ovtime spec). OVLD = app_overload_active() 라이브
     * 반영(슬라이스 C). ESTOP = app_estop_active()(슬라이스 D). OUTERR는 6b. SENSOR/HORN = 원격
     * 관측용 신규 비트(요구사항 B-3/B-4). 비트 배치는 mb_status_bits()가 소유 — host 스위트가
     * 겹침·극성까지 고정한다. SEEK/RESET 은 FSM 단일 상태라 두 비트가 동시에 서지 않는다. 글루
     * (app_seek_reset.c)를 거치지 않고 순수 FSM 상태를 직접 읽는다 — 글루가 노출하는 것은 active
     * (직교 판정용) 뿐이고 leg 구분이 없다. app_remote_en_fsm 을 같은 방식으로 읽는 선례가 위에 있다. */
    const uint8_t sr = seek_reset_fsm_state();
    const mb_status_in_t sin = {
        .running = running,
        .estop   = app_estop_active(),
        .ovld    = app_overload_active(),
        .ovtime  = (m->error_status & ERR_OVTIME) ? 1u : 0u,
        .sensor  = app_weld_sensor_active(),
        .horn    = app_horn_mode_active(),
        .seek    = (sr == (uint8_t)SR_SEEK)  ? 1u : 0u,
        .reset   = (sr == (uint8_t)SR_RESET) ? 1u : 0u,
    };
    g_mb.holding[MB_REG_STATUS]      = mb_status_bits(&sin);
}

/* mirror_live 본체 3/3 — F-A comm/eth·CAP·FEAT·HORN 미러 + staged 루프 + 원격 게이트 미러 */
static inline __attribute__((always_inline)) void mirror_stage_and_gate(const app_config_t *cfg)
{
    /* F-A comm/eth 미러. COMM_MODE·CFG_STAT 는 무조건, staged 9종은 **비-dirty 일
     * 때만** cfg 라이브 값으로 덮는다 — dirty 인 동안 미러가 덮으면 "쓰기 후
     * read-back" 계약이 staging 에서 깨져 마스터가 자기가 쓴 값을 확인할 수 없다. */
    g_mb.holding[MB_REG_COMM_MODE] = cfg->comm_mode;
    g_mb.holding[MB_REG_CFG_STAT]  = s_stg.stat;
    /* F-A capability — **모델 무관 무조건**. 게이트 CAP(0x2A)와 달리 STD 에서도
     * 실어야 한다: F-A 는 두 모델 모두에 있으므로, 안 실으면 소비 측이 F-A 를
     * 지원하는 STD 유닛을 구 펌웨어로 오판한다. */
    g_mb.holding[MB_REG_CFG_CAP]   = MB_REG_CFG_CAP_MAGIC;
    /* 기능 비트맵 — **모델 무관 무조건**(spec §0 A: STD 도 hold 워치독을 갖는다).
     * 0x31 매직과 조합해 "신 펌웨어인데 비트 0" 이 확정적 미지원으로 읽힌다. */
    g_mb.holding[MB_REG_FEAT_CAP]  = MB_FEAT_HOLD_WDT;
    /* B-4 조작 미러 — 실제 모드 상태를 되비춘다(0/1 정규화는 접근자가 보장). */
    g_mb.holding[MB_REG_HORN_CMD]  = app_horn_mode_active();
    for (uint8_t i = 0u; i < (uint8_t)CFG_STG_COUNT; i++) {
        if (cfg_stage_dirty(&s_stg, i) == 0u) {
            g_mb.holding[k_stg_reg[i]] = stg_mirror_val(cfg, i);
        }
    }

    /* 원격 게이트 미러. CAP는 매직 무조건 복원 = capability probe의 신-펌웨어 판별점("read-only는
     * 미러가 덮음" — MODEL_FREQ/TYPE 위와 동형). 0x2D는 예약이라 미러하지 않는다. 조건 없이 함수
     * 말미에 두어야 세 호출처(apply_config RTU 획득 / tick RTU / tick TCP)가 전부 커버되고, 링크
     * 전이의 mb_core_init 0-리셋도 같은 tick에 즉시 복원된다.
     * ⚠ STD 는 미러하지 않는다 — 인터록이 없는데 CAP 매직을 실으면 원격기가 "이 컨트롤러는 게이트를
     * 지원한다"고 오판한다(A-7의 판별이 정확히 이것). 미러가 없으면 0x2A 는 원격기가 쓴 probe 값 P
     * 가 그대로 남아 구-펌웨어와 같은 판정을 받는다 = 의도한 동작. */
#if defined(MODEL_REMOTE)
    g_mb.holding[MB_REG_REMOTE_CAP]     = MB_REG_REMOTE_CAP_MAGIC;
    g_mb.holding[MB_REG_REMOTE_EN]      = s_ren.state;
    /* 0x2C 는 결번(구 잔여-초). 레벨 스위치는 만료가 없다 — 원격기가 옛 의미로
     * 읽지 않도록 0 으로 고정한다. */
    g_mb.holding[MB_REG_REMOTE_EN_LEFT] = 0u;
#endif
}

/* live 값 mirror */
static void mirror_live(void)
{
    /* samd20 update_holding_reg(0): live values -> holding mirror. Runs every owned
     * tick — fresher than samd20's post-message refresh, normalizes clamped writes. */
    const app_config_t  *cfg = app_lcd_cfg();
    const lcd_measure_t *m   = app_lcd_measure();
    uint8_t running = (m->us_run_status != (uint8_t)US_IDLE) ? 1u : 0u;

    mirror_cfg_fields(cfg);
    mirror_disp_status(cfg, m, running);

    mirror_stage_and_gate(cfg);
}
```

- [ ] **Step 3: 게이트**

```sh
./fw.sh 2>&1 | grep -E 'warning:|error:' | grep -v '/vendor/'                # 기대: 출력 없음
MODEL=remote ./fw.sh 2>&1 | grep -E 'warning:|error:' | grep -v '/vendor/'   # 기대: 출력 없음
fw/tools/bin-same.sh   # 기대: SAME  fd66f6e7bb9d6ff03f7ea1831797167813b3db4613de66e6c84841b1860e278f c5223adac03c85861e242e592125bf163114c847f383d6304f5d2e0771f7373f
./fw.sh test 2>&1 | grep -ciE 'all (checks |tests )?passed'                  # 기대: 17
```

- [ ] **Step 4: DIFF 면** — `git checkout -- fw/src/app_modbus.c`. spec §6 표에 행 추가: `| mirror_live | Task 2 always_inline 3분할에서 .bin ≠ (STD/REMOTE 해시 기록) | 보류 — 99줄 잔존 | HW 벤치 트랙 |`. 진단(spec §5.2)은 기록만: `arm-none-eabi-nm -S --size-sort fw/build/gds_us_ctrl.elf | grep -E ' (mirror_live|app_modbus_tick|apply_config)$'` 전후 크기를 커밋 본문 대신 §6 행에 적는다. 인자 순서 바꾸기 등 우회 재시도 금지(H7). Task 3 으로 진행.

- [ ] **Step 5: 길이 측정(후)**

```sh
python3 fw/tools/funclen.py fw/src/app_modbus.c | grep -E '^(mirror_live|mirror_cfg_fields|mirror_disp_status|mirror_stage_and_gate) '
# 기대:
# mirror_cfg_fields 18 (code 18)
# mirror_disp_status 44 (code 30)
# mirror_stage_and_gate 37 (code 18)
# mirror_live 13 (code 9)
wc -l fw/src/app_modbus.c        # 기대: 772
```

- [ ] **Step 6: Commit**

```sh
git add fw/src/app_modbus.c
git commit -m "refactor(modbus): mirror_live 3분할 — .bin 동일 (STD fd66…e278f / REMOTE c522…7373f)

- always_inline void 헬퍼 3개(mirror_cfg_fields / mirror_disp_status / mirror_stage_and_gate) — 연속 블록 그대로 이동,
  인자 = 기존 cfg·m 포인터 + running 값 (H1~H6). mirror_live 99→13줄
- bin-same.sh SAME · host 17 PASS · our-code 경고 0"
```

---

### Task 3: 슬라이스 3 — `reg_publish_measure` 3분할 + `usart6_mb_open` 2분할

**Files:**
- Modify: `fw/src/app_reg.c` — Task 1 후 `reg_publish_measure` :325-405 (헤더 `/* 측정값 publish 갱신 */` :324; `b61ef0f` :324-404). 헬퍼 3개는 :324 바로 위에
- Modify: `fw/drivers/usart6_mb.c` — `usart6_mb_open` :46-113 (헤더 `/* Modbus 포트 열기 */` :45; Task 1 무변경 파일). 헬퍼 2개는 :45 바로 위에

**Interfaces:**
- Produces: `static inline __attribute__((always_inline)) void publish_sr_edge(uint8_t sr)` — `if (sr != g_reg.sr_disp_active)` 의 **본체**(내부 if/else + `sr_disp_active = sr`). 판정 `if` 는 호출자에 남는다(H3). H4 ✓ (`sr` 값). H5 ✓ (`g_reg`·`g_measure`)
- Produces: `static inline __attribute__((always_inline)) void publish_amp_power(uint8_t live)` — ch1 표시 전류/전력 계산 + 피크 추적 (`disp_amp`·`disp_pwr` 는 헬퍼 로컬). H4 ✓ (`live` 값)
- Produces: `static inline __attribute__((always_inline)) void publish_copy_out(uint32_t now, uint8_t active, uint8_t live, int16_t freq_cal_val)` — 에너지 적분·on-time·g_measure 복사·USOUT 전이. **D3**: spec 의 `(now, active, freq_cal_val)` 에 `live` 추가 (`g_measure.us_on_status = live;` 가 쓴다). H4 ✓ (전부 값)
- Produces: `static inline __attribute__((always_inline)) void mb_uart_reinit(uint8_t speed_idx, uint8_t parity_idx)` — `HAL_UART_DeInit` ~ `HAL_UART_Init` 블록 (`s_baud`·`s_gap_ms` 파일 static 직접 기록). H4 ✓ (함수 인자 값 그대로)
- Produces: `static inline __attribute__((always_inline)) void mb_dma_init(void)` — `hdma_usart6_rx` 초기화 + `__HAL_LINKDMA`. 인자 없음

- [ ] **Step 1: 길이 측정(전)**

```sh
python3 fw/tools/funclen.py fw/src/app_reg.c | grep -E '^reg_publish_measure '   # 기대: reg_publish_measure 81 (code 54)
python3 fw/tools/funclen.py fw/drivers/usart6_mb.c | grep -E '^usart6_mb_open '  # 기대: usart6_mb_open 68 (code 53)
```

- [ ] **Step 2-A: `reg_publish_measure` 헬퍼 추출**

**before** (Task 1 후 `fw/src/app_reg.c:324-405` — 구조만; 세 구간 전문은 아래 헬퍼 본문과 동일. sr-edge 본체는 8칸 들여쓰기였던 것을 헬퍼에서 4칸으로 dedent):

```c
/* 측정값 publish 갱신 */
static void reg_publish_measure(uint32_t now, int16_t freq_cal_val)
{
    /* slice 2b run-gated: ... (3줄) */
    uint8_t active = (uint8_t)(g_reg.us_run_status != (uint8_t)US_IDLE);
    /* SEEK/RESET 중 측정값 라이브 표시 ... (6줄) */
    uint8_t sr = app_seek_reset_active();
    if (sr != g_reg.sr_disp_active) {
        if (sr != 0u) {
        ⋮   (:339-352, 14줄 → publish_sr_edge 본문)
        g_reg.sr_disp_active = sr;
    }
    uint8_t live = (uint8_t)(active || (sr != 0u));
    /* 표시 전류/전력은 ch1(소비전류)에서 — 레귤레이션(ch0/reg_scale)과 분리.
    ⋮   (:355-368, 14줄 → publish_amp_power 본문)
    }
    /* 에너지 적분: active면 curr_power를 acc에 누산(2ms publish cadence) ->
    ⋮   (:369-404, 36줄 → publish_copy_out 본문)
    }
}
```

**after** (:324-405 를 아래 전체로 교체):

```c
/* reg_publish_measure 본체 1/3 — seek/reset 표시 라이브 on/off 엣지 처리 */
static inline __attribute__((always_inline)) void publish_sr_edge(uint8_t sr)
{
    if (sr != 0u) {
        g_reg.max_amp     = 0u;
        g_reg.max_power   = 0u;
        g_reg.last_energy = 0u;
        g_reg.acc_energy      = 0u;
        g_measure.curr_energy = 0u;
    } else {
        g_reg.last_amp    = g_measure.curr_amp;
        g_reg.last_power  = g_measure.curr_power;
        g_reg.last_energy = g_measure.curr_energy;
        g_reg.acc_energy      = 0u;
        g_measure.curr_energy = 0u;
    }
    g_reg.sr_disp_active = sr;
}

/* reg_publish_measure 본체 2/3 — ch1 표시 전류/전력 + 피크 추적 */
static inline __attribute__((always_inline)) void publish_amp_power(uint8_t live)
{
    /* 표시 전류/전력은 ch1(소비전류)에서 — 레귤레이션(ch0/reg_scale)과 분리.
     * SAMD20 cal_real_val 포팅 (spec §3). 피크홀드 비교 소스도 ch1 산출값.
     * 피크 추적/curr_power는 live 게이트 — seek/reset 중에도 갱신 (samd20은
     * ADC 경로 무게이트 추적 + 엣지 제로화, main.c:428-433). */
    uint16_t disp_amp = reg_current_from_adc(g_reg.ch1_avg, g_reg.cal_val);
    g_measure.curr_amp = disp_amp;
    if (live && (disp_amp > g_reg.max_amp)) {
        g_reg.max_amp = disp_amp;
    }
    uint16_t disp_pwr = reg_power_from_amp(disp_amp);
    g_measure.curr_power = live ? disp_pwr : 0u;
    if (live && (disp_pwr > g_reg.max_power)) {
        g_reg.max_power = disp_pwr;
    }
}

/* reg_publish_measure 본체 3/3 — 에너지 적분·on-time·g_measure 복사·USOUT 전이 */
static inline __attribute__((always_inline)) void publish_copy_out(uint32_t now, uint8_t active, uint8_t live, int16_t freq_cal_val)
{
    /* 에너지 적분: active면 curr_power를 acc에 누산(2ms publish cadence) ->
     * curr_energy = acc/250 (samd20 main.c:434-436 구조). idle엔 curr_power=0이라
     * 누산 정지. EXIT 판정은 weld FSM(US_CYCLE)만, 누산/표시는 모든 run 보편. slice2 §5. */
    if (active) {
        g_reg.acc_energy += g_measure.curr_power;
        g_measure.curr_energy = reg_energy_from_acc(g_reg.acc_energy);
    }
    if (active) {
        /* LV_TIME bar: live on-time in 200 ms units from the run-start stamp
         * (samd20 main.c:5223 cadence counter equivalent). TOUCH and COMM runs
         * both stamp run_start_ms at their START edge (app_reg_command); a
         * future REMOTE slice must stamp its own start. When idle the field
         * keeps the last run's final value — samd20 shows the latched
         * last_time when stopped, and disp feeds this one field on both paths
         * (app_lcd_disp.c:183 note). */
        g_measure.us_on_time_200m =
            reg_on_time_200m((uint32_t)(now - g_reg.run_start_ms));
    }
    g_measure.max_power     = g_reg.max_power;
    g_measure.last_power    = g_reg.last_power;
    g_measure.max_amp  = g_reg.max_amp;
    g_measure.last_amp = g_reg.last_amp;
    g_measure.last_energy = g_reg.last_energy;
    /* FREQ_IN: 매 publish에 측정 (SAMD20 calc_freq처럼 run 게이팅 없음 — 무신호면
     * FSM이 0 반환). 표시(VAR_FREQ)/Modbus(MB_REG_DISP_FREQ)는 on?curr:last로
     * 자체 게이팅(기존 배선). slice-B Task 3. */
    g_measure.curr_freq = freq_fsm_compute(freq_cal_val);
    g_measure.last_freq = g_reg.last_freq;
    g_measure.us_run_status = g_reg.us_run_status;
    g_measure.us_on_status  = live;   /* 표시 라이브 게이트 (LCD VAR_ / Modbus DISP_) */
    g_measure.error_status  = g_reg.error_status;   /* OVTIME 등 fault → LCD/Modbus */
    /* USOUT: run 활성(idle 아님)에 출력 enable. 전이에만 hook 구동 (active 재사용). */
    if (active != g_reg.us_out_on) {
        g_reg.us_out_on = active;
        app_reg_hook_us_output(active != 0u);
    }
}

/* 측정값 publish 갱신 */
static void reg_publish_measure(uint32_t now, int16_t freq_cal_val)
{
    /* slice 2b run-gated: curr_power = live setpoint (0 when idle); max_power =
     * running peak during the run; last_power latched on stop (app_reg_command).
     * cycle/freq/energy stay 0 (weld-cycle deferred). */
    uint8_t active = (uint8_t)(g_reg.us_run_status != (uint8_t)US_IDLE);
    /* SEEK/RESET 중 측정값 라이브 표시 (samd20 us_on_status 복원): on-엣지에
     * 피크/에너지 제로화(main.c:4253-4256/4280-4282), off-엣지에 last_* 스냅샷
     * 래치(main.c:4263-4268/4288-4293). last_freq는 legacy 충실로 래치하지
     * 않음(seek off에 freq 래치 없음 — 종료 후 이전 런 주파수로 복귀).
     * RESET→SEEK 자동 체인은 하나의 active 윈도우(체인 경계 latch/re-zero
     * 생략 — legacy와 미세 편차, 피크가 체인 전체에 걸쳐 연속). */
    uint8_t sr = app_seek_reset_active();
    if (sr != g_reg.sr_disp_active) {
        publish_sr_edge(sr);
    }
    uint8_t live = (uint8_t)(active || (sr != 0u));
    publish_amp_power(live);
    publish_copy_out(now, active, live, freq_cal_val);
}
```

- [ ] **Step 2-B: `usart6_mb_open` 헬퍼 추출**

**before** (`fw/drivers/usart6_mb.c:45-113` — 두 구간의 첫·끝 줄; 전문은 헬퍼 본문과 동일):

```c
/* Modbus 포트 열기 */
void usart6_mb_open(uint8_t speed_idx, uint8_t parity_idx)
{
    /* Double-open guard: ... (3줄) */
    if (s_open) {
        return;
    }

    __HAL_RCC_DMA2_CLK_ENABLE();

    /* Re-init USART6 at the Modbus line config. GPIO PC6/PC7 AF8 was set by
    ⋮   (:57-83, 27줄 → mb_uart_reinit 본문)
    }

    /* DMA2 Stream1 Ch5 = USART6_RX (RM0401 DMA2 request map; the Stream2 Ch5
    ⋮   (:85-101, 17줄 → mb_dma_init 본문)
    __HAL_LINKDMA(&huart6, hdmarx, hdma_usart6_rx);

    s_rx_tail    = 0;
    ⋮   (:103-113 그대로)
}
```

**after** (:45-113 을 아래 전체로 교체):

```c
/* usart6_mb_open 본체 1/2 — USART6 를 Modbus 라인 설정(baud/parity)으로 재초기화 */
static inline __attribute__((always_inline)) void mb_uart_reinit(uint8_t speed_idx, uint8_t parity_idx)
{
    /* Re-init USART6 at the Modbus line config. GPIO PC6/PC7 AF8 was set by
     * usart6_init() at boot and is never unconfigured. */
    HAL_UART_DeInit(&huart6);
    huart6.Instance      = USART6;
    huart6.Init.BaudRate = (speed_idx < 6u) ? mb_baud[speed_idx]
                                            : 19200u;   /* samd20 default branch */
    s_baud   = huart6.Init.BaudRate;
    s_gap_ms = (speed_idx < 6u) ? mb_gap_ms[speed_idx] : 2u;
    /* STM32 parity rides in the 9th bit: EVEN/ODD need WORDLENGTH_9B for
     * 8 data bits + parity. 9B+NONE (the HAL's u16-buffer mode) never occurs. */
    if (parity_idx == 0u) {
        huart6.Init.Parity     = UART_PARITY_EVEN;
        huart6.Init.WordLength = UART_WORDLENGTH_9B;
    } else if (parity_idx == 1u) {
        huart6.Init.Parity     = UART_PARITY_ODD;
        huart6.Init.WordLength = UART_WORDLENGTH_9B;
    } else {
        huart6.Init.Parity     = UART_PARITY_NONE;
        huart6.Init.WordLength = UART_WORDLENGTH_8B;
    }
    huart6.Init.StopBits     = UART_STOPBITS_1;
    huart6.Init.Mode         = UART_MODE_TX_RX;
    huart6.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart6.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart6) != HAL_OK) {
        Error_Handler();
    }
}

/* usart6_mb_open 본체 2/2 — DMA2 Stream1 Ch5 circular RX 초기화 + UART 링크 */
static inline __attribute__((always_inline)) void mb_dma_init(void)
{
    /* DMA2 Stream1 Ch5 = USART6_RX (RM0401 DMA2 request map; the Stream2 Ch5
     * alternate is unusable — Stream2 belongs to USART1 RX). Same circular
     * free-running config as usart1.c. */
    hdma_usart6_rx.Instance                 = DMA2_Stream1;
    hdma_usart6_rx.Init.Channel             = DMA_CHANNEL_5;
    hdma_usart6_rx.Init.Direction           = DMA_PERIPH_TO_MEMORY;
    hdma_usart6_rx.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_usart6_rx.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_usart6_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart6_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    hdma_usart6_rx.Init.Mode                = DMA_CIRCULAR;
    hdma_usart6_rx.Init.Priority            = DMA_PRIORITY_HIGH;
    hdma_usart6_rx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_usart6_rx) != HAL_OK) {
        Error_Handler();
    }
    __HAL_LINKDMA(&huart6, hdmarx, hdma_usart6_rx);
}

/* Modbus 포트 열기 */
void usart6_mb_open(uint8_t speed_idx, uint8_t parity_idx)
{
    /* Double-open guard: the app_modbus state machine always closes before
     * reopening; a second open here would silently kill the live DMA stream
     * mid-transfer and reset the ring state. Make it a no-op instead. */
    if (s_open) {
        return;
    }

    __HAL_RCC_DMA2_CLK_ENABLE();

    mb_uart_reinit(speed_idx, parity_idx);

    mb_dma_init();

    s_rx_tail    = 0;
    s_prev_head  = 0;
    s_last_rx_ms = sys_tick_get_ms();

    if (HAL_DMA_Start(&hdma_usart6_rx, (uint32_t)&huart6.Instance->DR,
                      (uint32_t)s_rx_dma_buf, MB_RX_DMA_SIZE) != HAL_OK) {
        Error_Handler();
    }
    SET_BIT(USART6->CR3, USART_CR3_DMAR);
    s_open = true;
}
```

- [ ] **Step 3: 게이트**

```sh
./fw.sh 2>&1 | grep -E 'warning:|error:' | grep -v '/vendor/'                # 기대: 출력 없음
MODEL=remote ./fw.sh 2>&1 | grep -E 'warning:|error:' | grep -v '/vendor/'   # 기대: 출력 없음
fw/tools/bin-same.sh   # 기대: SAME  fd66f6e7bb9d6ff03f7ea1831797167813b3db4613de66e6c84841b1860e278f c5223adac03c85861e242e592125bf163114c847f383d6304f5d2e0771f7373f
./fw.sh test 2>&1 | grep -ciE 'all (checks |tests )?passed'                  # 기대: 17
```

- [ ] **Step 4: DIFF 면** — 두 파일은 독립이다. `DIFF` 면 먼저 `git stash` 없이 한 파일씩 가른다: `git checkout -- fw/drivers/usart6_mb.c` 후 `fw/tools/bin-same.sh` → `SAME` 이면 `usart6_mb_open` 쪽이 원인(usart6_mb 만 §6 "보류" 행 추가, app_reg 는 유지). 여전히 `DIFF` 면 `git checkout -- fw/src/app_reg.c` 하고 usart6 를 다시 적용해 재판정. 둘 다 ≠ 이면 둘 다 §6 기록. 우회 금지.

- [ ] **Step 5: 길이 측정(후)**

```sh
python3 fw/tools/funclen.py fw/src/app_reg.c | grep -E '^(reg_publish_measure|publish_sr_edge|publish_amp_power|publish_copy_out) '
# 기대:
# publish_sr_edge 17 (code 17)
# publish_amp_power 17 (code 13)
# publish_copy_out 39 (code 25)
# reg_publish_measure 20 (code 11)
python3 fw/tools/funclen.py fw/drivers/usart6_mb.c | grep -E '^(usart6_mb_open|mb_uart_reinit|mb_dma_init) '
# 기대:
# mb_uart_reinit 30 (code 26)
# mb_dma_init 20 (code 17)
# usart6_mb_open 26 (code 18)
```

- [ ] **Step 6: Commit**

```sh
git add fw/src/app_reg.c fw/drivers/usart6_mb.c
git commit -m "refactor(reg,usart6): reg_publish_measure 3분할 + usart6_mb_open 2분할 — .bin 동일 (STD fd66…e278f / REMOTE c522…7373f)

- publish_sr_edge(sr) [판정 if 는 호출자] / publish_amp_power(live) / publish_copy_out(now, active, live, freq_cal_val)
  — spec §4 의 시그니처에 live 추가 (us_on_status 대입이 사용). 81→20줄
- mb_uart_reinit(speed_idx, parity_idx) / mb_dma_init() — 전역 핸들·파일 static 직접. 68→26줄
- bin-same.sh SAME · host 17 PASS · our-code 경고 0"
```

---

### Task 4: 슬라이스 4 — `app_modbus_apply_writes` 본체 3추출 (≤50 불가 — spec §6 예외)

**Files:**
- Modify: `fw/src/app_modbus.c` — Task 2 후 `app_modbus_apply_writes` :292-609 (헤더 `/* FC06 write 적용 */` :291; `b61ef0f` :290-647). 헬퍼 3개는 :291 바로 위에. 추출 구간(Task 2 후 좌표): 게이트 본체 :317-353 (`if` :316 과 `return;` :354 는 남김), START 값 체인 :374-392, 커밋 본체 :411-419

**Interfaces:**
- Produces: `static inline __attribute__((always_inline)) void gate_reject_body(void)` — 게이트 닫힘 본체(blocked 판별·명령 3종 소거·STOP 통과·mon 로그·STOP/CFG_CTRL 소거). `#ifndef REMOTE_EN_GATE_BYPASS … #endif` 로 감싼다(BYPASS 빌드에서 미사용 → `-Wunused-function` 방지). **감사 b-2v-i 실증 ○ 패턴 그대로** — 판정 `if (s_ren.state != REN_ENABLED)` 와 `return;` 은 호출자. 인자 없음(H4 ✓), `g_mb`·`s_ren` 직접(H5 ✓), `return` 없음(H6 ✓)
- Produces: `static inline __attribute__((always_inline)) void start_cmd_body(app_config_t *cfg, uint16_t sv, uint32_t now)` — `if (sv == MB_START_TAP) … else if (MB_START_HOLD) … else if (MB_START_KEEP)` 체인 전체(`return` 없음). 소거 `g_mb.holding[MB_REG_START] = 0u` 와 `sv`·`now` 계산은 호출자. H4 ✓ (`cfg` 기존 포인터, `sv`·`now` 값). `s_hwd` 직접(H5 ✓)
- Produces: `static inline __attribute__((always_inline)) void cfg_ctrl_commit_body(app_config_t *cfg, uint16_t d)` — `stg_apply_to_cfg(cfg, d)` + ether 마스크 훅. **D2**: spec 의 `(cfg, link)` 대신 `(cfg, d)` — 본체가 쓰는 것은 dirty 스냅샷 `d` 이고 `link` 는 호출자에 남는 `cfg_stage_commit()` 판정에만 쓰인다. `save = true` 와 `if (cfg_stage_commit(...) != 0u)` 는 **호출자에 남는다**(H4: `&save` 금지). H4 ✓ (`d` 는 값)
- **13개 클램프 분기(DELAY1~TIMEOVER)·RUN_MODE·EN_*·HORN·MODEL_*·CAL·work_cnt·staged 스캔은 한 글자도 건드리지 않는다**(감사 b-1 계열 ✗)

- [ ] **Step 1: 길이 측정(전)**

```sh
python3 fw/tools/funclen.py fw/src/app_modbus.c | grep -E '^app_modbus_apply_writes '   # 기대: app_modbus_apply_writes 318 (code 191)
wc -l fw/src/app_modbus.c                                                                # 기대: 772
sed -n '316p;354p;374p;392p;411p;419p' fw/src/app_modbus.c
# 기대(앵커 6줄):
#     if (s_ren.state != (uint8_t)REN_ENABLED) {
#         return;
#         if (sv == MB_START_TAP) {
#         }
#                 stg_apply_to_cfg(cfg, d);
#                 }
```

- [ ] **Step 2: 헬퍼 추출**

**before ①** (게이트, Task 2 후 :315-355 — 본체 :317-353 은 아래 `gate_reject_body` 본문과 동일, 들여쓰기만 8→4):

```c
#ifndef REMOTE_EN_GATE_BYPASS
    if (s_ren.state != (uint8_t)REN_ENABLED) {
        /* 벤치 관측용(VR-3): 무엇이 막혔는지 mon에 남긴다. 소거 전에 잡아야 한다.
        ⋮   (:317-353, 37줄)
        g_mb.holding[MB_REG_CFG_CTRL] = 0u;
        return;
    }
#endif
```

**after ①**:

```c
#ifndef REMOTE_EN_GATE_BYPASS
    if (s_ren.state != (uint8_t)REN_ENABLED) {
        gate_reject_body();
        return;
    }
#endif
```

**before ②** (START, Task 2 후 :366-393):

```c
    } else if (g_mb.holding[MB_REG_START] != 0u) {
        /* hold-to-run (spec 2026-09-06 §2.1·§4). 소거는 **값 불문** — CFG_CTRL 과 동형.
         * 값 1 = 탭 START(기존 그대로) / 2 = hold 시작 + 워치독 무장 / 3 = 유지 신호 /
         * 그 외 = 무시. 🔴 무장 지점은 이 START=2 분기 하나뿐이고 start_allowed 일
         * 때만이다 — "무장됐다 ⇒ 방금 시작된 런은 우리 hold 런" 이 세션 경계의 첫 겹. */
        uint16_t sv  = g_mb.holding[MB_REG_START];
        uint32_t now = sys_tick_get_ms();
        g_mb.holding[MB_REG_START] = 0u;
        if (sv == MB_START_TAP) {
        ⋮   (:374-392, 19줄 → start_cmd_body 본문)
        }
    } else if (g_mb.holding[MB_REG_STOP] == 1u) {
```

**after ②**:

```c
    } else if (g_mb.holding[MB_REG_START] != 0u) {
        /* hold-to-run (spec 2026-09-06 §2.1·§4). 소거는 **값 불문** — CFG_CTRL 과 동형.
         * 값 1 = 탭 START(기존 그대로) / 2 = hold 시작 + 워치독 무장 / 3 = 유지 신호 /
         * 그 외 = 무시. 🔴 무장 지점은 이 START=2 분기 하나뿐이고 start_allowed 일
         * 때만이다 — "무장됐다 ⇒ 방금 시작된 런은 우리 hold 런" 이 세션 경계의 첫 겹. */
        uint16_t sv  = g_mb.holding[MB_REG_START];
        uint32_t now = sys_tick_get_ms();
        g_mb.holding[MB_REG_START] = 0u;
        start_cmd_body(cfg, sv, now);
    } else if (g_mb.holding[MB_REG_STOP] == 1u) {
```

**before ③** (커밋, Task 2 후 :404-430):

```c
        if (ctrl == 1u) {
            /* 커밋이 dirty 를 지우기 전에 스냅샷 — 무엇을 반영할지가 여기에 있다. */
            uint16_t d = s_stg.dirty;
            /* 가동 중 판정 = us_on_status (run OR seek/reset). 가동 중 통신 링크
             * 재초기화를 막는다. */
            if (cfg_stage_commit(&s_stg, link,
                                 app_lcd_measure()->us_on_status) != 0u) {
                stg_apply_to_cfg(cfg, d);
                if ((d & CFG_STG_ETHER_MASK) != 0u) {
                    /* LCD SAVE 와 같은 훅을 재사용 — app_eth_tick 이 dirty 를
                     * consume 해 재적용한다. RTU 는 응답을 blocking 으로 먼저
                     * 보내고 나서 apply 를 부르므로(send → apply 순서, 아래 tick)
                     * 지연이 불필요하다. DG-12 로 ether 커밋은 RTU 로만 온다(500ms 지연 상수 폐기 = changelog 2026-09-04 F-A). */
                    app_lcd_hook_ether_apply(cfg->comm_mode, cfg->ether_ip,
                                             cfg->ether_nm, cfg->ether_gw);
                }
                /* serial 그룹에는 즉시 재초기화가 **일어나지 않는다** — 그리고
                ⋮   (8줄 그대로)
                save = true;
            }
        } else if (ctrl == 2u) {
```

**after ③**:

```c
        if (ctrl == 1u) {
            /* 커밋이 dirty 를 지우기 전에 스냅샷 — 무엇을 반영할지가 여기에 있다. */
            uint16_t d = s_stg.dirty;
            /* 가동 중 판정 = us_on_status (run OR seek/reset). 가동 중 통신 링크
             * 재초기화를 막는다. */
            if (cfg_stage_commit(&s_stg, link,
                                 app_lcd_measure()->us_on_status) != 0u) {
                cfg_ctrl_commit_body(cfg, d);
                /* serial 그룹에는 즉시 재초기화가 **일어나지 않는다** — 그리고
                ⋮   (8줄 그대로)
                save = true;
            }
        } else if (ctrl == 2u) {
```

**헬퍼 3개** — `/* FC06 write 적용 */`(:291) 바로 위에 이 순서로 삽입(마지막 빈 줄 포함):

```c
#ifndef REMOTE_EN_GATE_BYPASS
/* apply_writes 본체 1/3 — 게이트 닫힘: 명령 소거 + STOP 통과 + mon 로그 + CFG_CTRL 소거 (판정·return 은 호출자) */
static inline __attribute__((always_inline)) void gate_reject_body(void)
{
    /* 벤치 관측용(VR-3): 무엇이 막혔는지 mon에 남긴다. 소거 전에 잡아야 한다.
     * 무음 거부는 "STATUS 무변화"라는 간접 증거만 남겨서, 게이트가 막은 것인지
     * 애초에 요청이 안 온 것인지 컨트롤러 쪽에서 구분할 수 없다.
     * ⚠ mon은 RTU 점유 시 꺼지므로(app_modbus.c apply_config의
     * mon_set_enabled) 이 줄은 ETH 모드에서만 보인다 — VR-3은 TCP로 칠 것. */
    uint8_t blocked = 0u;
    if      (g_mb.holding[MB_REG_RESET] != 0u) { blocked = MB_REG_RESET; }
    else if (g_mb.holding[MB_REG_SEEK]  != 0u) { blocked = MB_REG_SEEK;  }
    else if ((g_mb.holding[MB_REG_START] != 0u) &&
             (g_mb.holding[MB_REG_START] != MB_START_KEEP)) { blocked = MB_REG_START; }
    /* START=KEEP 은 로그에서 제외 — 닫힌 게이트에 초당 ~7건 오면 mon 을 덮는다.
     * 소거는 아래에서 값 불문 그대로(유지 신호가 굶어 ≤T 트립 = R-11 부수 효과). */

    g_mb.holding[MB_REG_RESET] = 0u;
    g_mb.holding[MB_REG_SEEK]  = 0u;
    g_mb.holding[MB_REG_START] = 0u;
    uint8_t stop_passed = (g_mb.holding[MB_REG_STOP] == 1u) ? 1u : 0u;
    if (stop_passed != 0u) {
        app_reg_command(US_CMD_RUN_RELEASE, (uint8_t)US_COMM);
    }
    /* 명령이 걸린 경우에만 찍는다 — cfg 전용 쓰기까지 찍으면 원격기의 주기
     * 파라미터 쓰기(수 초 간격)가 로그를 덮어버린다. cfg 거부는 read-back
     * 미러 복원으로 이미 관측 가능하다(위 주석). */
    if ((blocked != 0u) || (stop_passed != 0u)) {
        mon_printf("[mb] gate closed(state=%u): blocked=0x%02X stop_passed=%u\r\n",
                   (unsigned)s_ren.state, (unsigned)blocked, (unsigned)stop_passed);
    }
    /* STOP도 값 불문 소거 — 디스패치는 ==1일 때만이지만, 소거를 그 안에 두면
     * STOP=2 같은 비-1 write가 영영 잔류해(미러 대상 아님, 아래 체인도 ==1만
     * 매치) FC03 읽기가 유령 pending STOP을 계속 보고한다. */
    g_mb.holding[MB_REG_STOP] = 0u;
    /* F-A: 커밋은 실계 변경이라 게이트 대상이다. 값 불문 소거하되 CFG_STAT 는
     * 건드리지 않는다 — 게이트 거부와 커밋 검증 거부는 다른 층이고, 사유는
     * REMOTE_EN(0x2B)을 읽어 안다. staged 쓰기 자체는 실계 무영향이라
     * 게이트 대상이 아니지만, 이 return 이 스캔 분기도 함께 건너뛴다:
     * 게이트가 닫힌 동안의 staged 편집은 열린 뒤 다시 쓰면 된다. */
    g_mb.holding[MB_REG_CFG_CTRL] = 0u;
}
#endif

/* apply_writes 본체 2/3 — START 값(1 탭 / 2 hold 시작 / 3 유지) 디스패치 */
static inline __attribute__((always_inline)) void start_cmd_body(app_config_t *cfg, uint16_t sv, uint32_t now)
{
    if (sv == MB_START_TAP) {
        app_reg_command(US_CMD_START, (uint8_t)US_COMM);
        /* samd20 comm START 는 같은 자리에서 진폭 pot 을 쓴다(main.c:4400-4401).
         * LCD RUN-press 경로(app_lcd_input.c:217/242)와 동형 — 무조건 write.
         * 거부된 START 여도 출력이 없어 무해(멱등 1바이트).
         * (구 `us_run_status == US_COMM` 가드는 구조적으로 항상 FALSE 였다 — changelog 2026-09-04 `ac7e691`.) */
        app_lcd_hook_set_pot(cfg->output_power);
    } else if (sv == MB_START_HOLD) {
        if (app_reg_start_allowed()) {
            app_reg_command(US_CMD_START, (uint8_t)US_COMM);
            app_lcd_hook_set_pot(cfg->output_power);   /* 탭과 동형, 1회 */
            hold_wdt_arm(&s_hwd, now);
        } else if (hold_wdt_armed(&s_hwd) != 0u) {
            hold_wdt_keep(&s_hwd, now);   /* START=2 응답 유실 재시도 흡수 */
        }
        /* start_allowed 거짓 + 미무장 = 다른 마스터의 탭 런이 도는 중 — 무시.
         * 그 런은 워치독 대상이 아니다(§3.1 무변경의 근거). */
    } else if (sv == MB_START_KEEP) {
        hold_wdt_keep(&s_hwd, now);       /* armed 아니면 no-op = 기동 권한 없음 */
    }
}

/* apply_writes 본체 3/3 — CFG_CTRL=1 커밋 통과분 반영: cfg 대입 + ether 훅 (검증·save 는 호출자) */
static inline __attribute__((always_inline)) void cfg_ctrl_commit_body(app_config_t *cfg, uint16_t d)
{
    stg_apply_to_cfg(cfg, d);
    if ((d & CFG_STG_ETHER_MASK) != 0u) {
        /* LCD SAVE 와 같은 훅을 재사용 — app_eth_tick 이 dirty 를
         * consume 해 재적용한다. RTU 는 응답을 blocking 으로 먼저
         * 보내고 나서 apply 를 부르므로(send → apply 순서, 아래 tick)
         * 지연이 불필요하다. DG-12 로 ether 커밋은 RTU 로만 온다(500ms 지연 상수 폐기 = changelog 2026-09-04 F-A). */
        app_lcd_hook_ether_apply(cfg->comm_mode, cfg->ether_ip,
                                 cfg->ether_nm, cfg->ether_gw);
    }
}

```

- [ ] **Step 3: 게이트**

```sh
./fw.sh 2>&1 | grep -E 'warning:|error:' | grep -v '/vendor/'                # 기대: 출력 없음
MODEL=remote ./fw.sh 2>&1 | grep -E 'warning:|error:' | grep -v '/vendor/'   # 기대: 출력 없음
fw/tools/bin-same.sh   # 기대: SAME  fd66f6e7bb9d6ff03f7ea1831797167813b3db4613de66e6c84841b1860e278f c5223adac03c85861e242e592125bf163114c847f383d6304f5d2e0771f7373f
./fw.sh test 2>&1 | grep -ciE 'all (checks |tests )?passed'                  # 기대: 17
```

- [ ] **Step 4: DIFF 면** — 세 헬퍼는 한 파일이라 가르려면 **③ → ② 순으로 하나씩 되돌려** 재판정한다(`gate_reject_body` 는 실증 ○ 이므로 마지막까지 남긴다): 파일을 `git checkout -- fw/src/app_modbus.c` 로 통째 되돌린 뒤 ① 만 적용 → `bin-same.sh`; `SAME` 이면 ② 추가 → 재판정; `SAME` 이면 ③ 추가 → 재판정. `DIFF` 를 만든 헬퍼만 빼고 커밋, spec §6 의 `app_modbus_apply_writes` 행 "이번 결과" 열에 `start_cmd_body(또는 cfg_ctrl_commit_body) .bin ≠ → 보류` 를 덧쓴다. 인자 재배열·`link` 추가 같은 우회 금지(H7).

- [ ] **Step 5: 길이 측정(후)**

```sh
python3 fw/tools/funclen.py fw/src/app_modbus.c | grep -E '^(app_modbus_apply_writes|gate_reject_body|start_cmd_body|cfg_ctrl_commit_body) '
# 기대:
# gate_reject_body 40 (code 21)
# start_cmd_body 23 (code 17)
# cfg_ctrl_commit_body 12 (code 8)
# app_modbus_apply_writes 255 (code 157)
wc -l fw/src/app_modbus.c        # 기대: 792  (≤799 ✓)
```

`app_modbus_apply_writes` 255 (코드 157) 는 spec §6 예외 — 36분기 else-if 뼈대. spec 의 "~120 코드줄" 추정보다 크다: 13개 클램프 분기(65 코드줄)·B-5/C-2/work_cnt/staged 분기의 주석-외 코드가 그대로 남기 때문. Task 8 에서 §6 "이번 결과" 열을 `255줄(코드 157)` 로 기입.

- [ ] **Step 6: Commit**

```sh
git add fw/src/app_modbus.c
git commit -m "refactor(modbus): apply_writes 본체 3추출 — .bin 동일 (STD fd66…e278f / REMOTE c522…7373f)

- gate_reject_body() [감사 b-2v-i 실증 패턴, #ifndef REMOTE_EN_GATE_BYPASS] / start_cmd_body(cfg, sv, now) /
  cfg_ctrl_commit_body(cfg, d) — 판정 if·return·save=true 는 호출자 (H3/H4). spec §4 의 (cfg, link) 는
  본체가 link 를 안 써 (cfg, d) 로. 13 클램프 분기 무변경. 358→255줄 (§6 예외 유지)
- bin-same.sh SAME · host 17 PASS · our-code 경고 0 · app_modbus.c 792줄"
```

---

### Task 5: 슬라이스 5 — `app_lcd_input_dispatch` case 본체 4추출 (≤50 불가 — spec §6 예외)

**Files:**
- Modify: `fw/src/app_lcd_input.c` — Task 1 후 `app_lcd_input_dispatch` :414-638 (헤더 `/* 터치 키 디스패치 */` :413; `b61ef0f` :413-637). 헬퍼 4개는 :413 바로 위에. 추출 구간(Task 1 후 좌표 = `b61ef0f` +1): SYS_PIC_NOW 주석 :611-618 + case 본체 :620-632 / SETUP_PARAM 본체 :543-549 / SETUP_PARAM_MOOHAN 내부 :553-556 / LV_MO_TIME1 꼬리 :489-492 · LV_MO_TIME2 꼬리 :496-499 / LV_RUN_MODE 본체 :568-576

**Interfaces:**
- Produces: `static inline __attribute__((always_inline)) void handle_sys_pic_now(lcd_app_state_t *state, app_config_t *cfg, uint16_t data16)` — `case SYS_PIC_NOW:` 본체(가드 `if` 포함, 13줄). **감사 b-3b-i 실증 ○ 그대로.** case 라벨·`break` 는 호출자. 위 8줄 설명 주석(`/*--- panel boot …`)은 헬퍼 위로 옮긴다. H4 ✓ (`state`·`cfg` 기존 포인터, `data16` 값). `s_run_key_down` 파일 static 직접(H5 ✓)
- Produces: `static inline __attribute__((always_inline)) void handle_setup_param_enter(lcd_app_state_t *state)` — setup1 페이지 진입 + horn 체크박스 미러 + shadow 리셋 4문장. **두 case 에서 각각 호출**(인라인이라 코드 중복은 유지 — 원본과 동일한 4문장 2벌). MOOHAN 쪽 `if (long_press_released(vp, data16))` 는 호출자. H4 ✓
- Produces: `static inline __attribute__((always_inline)) void mo_time_clamp_echo(app_config_t *cfg)` — **D1**: LV_MO_TIME1/2 의 공통 꼬리 `if (limit_mo_time1 > limit_mo_time2) { … echo }` 4줄만. 대입 `cfg->limit_mo_time1/2 = data16;` 은 각 case 에 남는다. 두 case 에서 각각 호출. H4 ✓
- Produces: `static inline __attribute__((always_inline)) void handle_run_mode(lcd_app_state_t *state, app_config_t *cfg, uint16_t data16)` — `if (data16 == 1) … else if (data16 == 2) …` 9줄. H4 ✓
- 나머지 31개 `case … break` 뼈대와 `switch (vp)` 는 무변경(감사 b-3 ✗)

- [ ] **Step 1: 길이 측정(전)**

```sh
python3 fw/tools/funclen.py fw/src/app_lcd_input.c | grep -E '^app_lcd_input_dispatch '   # 기대: app_lcd_input_dispatch 225 (code 170)
sed -n '413p;489p;496p;543p;553p;568p;611p;619p' fw/src/app_lcd_input.c
# 기대(앵커 8줄):
# /* 터치 키 디스패치 */
#         if (cfg->limit_mo_time1 > cfg->limit_mo_time2) {
#         if (cfg->limit_mo_time1 > cfg->limit_mo_time2) {
#         state->lcd_status = setup1_page_for_mode(state->sys_mode);
#             state->lcd_status = setup1_page_for_mode(state->sys_mode);
#         if (data16 == 1) {                               /* delay mode */
#     /*--- panel boot / page-flip notification — guarded re-init (spec §10) ----
#     case SYS_PIC_NOW:
```

- [ ] **Step 2: 헬퍼 추출** (아래→위 순서로 교체하면 좌표가 유효하다)

**before ①** (SYS_PIC_NOW, :611-633):

```c
    /*--- panel boot / page-flip notification — guarded re-init (spec §10) ----
     * data16==0 means the panel reports it landed on a page (its own splash or a
     * mid-run reset). Re-seed the panel vars + model string + run page, but ONLY
     * when (a) the Stage B boot handshake has finished (boot_complete) and (b) at
     * least 200 ms passed since our own last set_page — otherwise the
     * change_page→set_page→SYS_PIC_NOW→re-init→set_page chain is a feedback loop.
     * app_lcd_init_mode ends in app_lcd_change_page, which refreshes
     * last_set_page_ms, so the 200 ms gate re-arms after each re-init. */
    case SYS_PIC_NOW:
        if (data16 == 0 && state->boot_complete &&
            (uint32_t)(sys_tick_get_ms() - state->last_set_page_ms) >= 200u) {
            /* Panel self-reset mid-run: the held RUN press is lost and no
             * RUN_RELEASE will arrive, so stop the run (UI lost -> stop the
             * actuator). This also re-syncs ICON_RUN: us_run_status -> IDLE
             * makes the next disp_step see a real edge after init_mode clears
             * the icon (spec §4.3). Harmless when already idle. */
            app_lcd_hook_us_command(US_CMD_RUN_RELEASE);
            s_run_key_down = 0u;   /* panel reset: the release edge never arrives */
            app_lcd_var_init();
            app_lcd_send_model_str(cfg->model_freq, cfg->model_type);
            app_lcd_init_mode(cfg);
        }
        break;
```

**after ①**:

```c
    case SYS_PIC_NOW:
        handle_sys_pic_now(state, cfg, data16);
        break;
```

**before ②** (LV_RUN_MODE, :567-577):

```c
    case LV_RUN_MODE:
        if (data16 == 1) {                               /* delay mode */
            cfg->run_mode = MODE_DELAY;
            state->lcd_status = LCD_SETUP_STD2D;
            app_lcd_change_page(state->lcd_status);
        } else if (data16 == 2) {                        /* trigger mode */
            cfg->run_mode = MODE_TRIGGER;
            state->lcd_status = LCD_SETUP_STD2T;
            app_lcd_change_page(state->lcd_status);
        }
        break;
```

**after ②**:

```c
    case LV_RUN_MODE:
        handle_run_mode(state, cfg, data16);
        break;
```

**before ③** (SETUP_PARAM + MOOHAN, :542-558):

```c
    case SETUP_PARAM:
        state->lcd_status = setup1_page_for_mode(state->sys_mode);
        app_lcd_change_page(state->lcd_status);
        /* horn-down 체크박스 = 현재 SYS_HORN 모드 미러 + shadow 리셋 (legacy
         * main.c:3617-3622 verbatim — 저장 시 체크 안 건드리면 temp==0이라
         * 모드 이탈되는 legacy 거동 포함). */
        dgus_write_u16(DISP_HORNDOWN, (uint16_t)app_horn_mode_active());
        state->temp_horndown = 0u;
        break;
    case SETUP_PARAM_MOOHAN:                             /* long-press variant of SETUP_PARAM */
        if (long_press_released(vp, data16)) {
            state->lcd_status = setup1_page_for_mode(state->sys_mode);
            app_lcd_change_page(state->lcd_status);
            dgus_write_u16(DISP_HORNDOWN, (uint16_t)app_horn_mode_active());
            state->temp_horndown = 0u;                   /* legacy 3617-3622 미러 */
        }
        break;
```

**after ③**:

```c
    case SETUP_PARAM:
        handle_setup_param_enter(state);
        break;
    case SETUP_PARAM_MOOHAN:                             /* long-press variant of SETUP_PARAM */
        if (long_press_released(vp, data16)) {
            handle_setup_param_enter(state);
        }
        break;
```

**before ④** (LV_MO_TIME1/2, :487-500):

```c
    case LV_MO_TIME1:
        cfg->limit_mo_time1 = data16;
        if (cfg->limit_mo_time1 > cfg->limit_mo_time2) {
            cfg->limit_mo_time2 = cfg->limit_mo_time1;          /* samd20 main.c:4011-4015 */
            dgus_write_u16(LV_MO_TIME2, cfg->limit_mo_time2);   /* echo new clamped value */
        }
        break;
    case LV_MO_TIME2:
        cfg->limit_mo_time2 = data16;
        if (cfg->limit_mo_time1 > cfg->limit_mo_time2) {
            cfg->limit_mo_time2 = cfg->limit_mo_time1;          /* samd20 main.c:4020-4024 */
            dgus_write_u16(LV_MO_TIME2, cfg->limit_mo_time2);   /* echo new clamped value */
        }
        break;
```

**after ④**:

```c
    case LV_MO_TIME1:
        cfg->limit_mo_time1 = data16;
        mo_time_clamp_echo(cfg);
        break;
    case LV_MO_TIME2:
        cfg->limit_mo_time2 = data16;
        mo_time_clamp_echo(cfg);
        break;
```

**헬퍼 4개** — `/* 터치 키 디스패치 */`(:413) 바로 위에 이 순서로 삽입(마지막 빈 줄 포함):

```c
/*--- panel boot / page-flip notification — guarded re-init (spec §10) ----
 * data16==0 means the panel reports it landed on a page (its own splash or a
 * mid-run reset). Re-seed the panel vars + model string + run page, but ONLY
 * when (a) the Stage B boot handshake has finished (boot_complete) and (b) at
 * least 200 ms passed since our own last set_page — otherwise the
 * change_page→set_page→SYS_PIC_NOW→re-init→set_page chain is a feedback loop.
 * app_lcd_init_mode ends in app_lcd_change_page, which refreshes
 * last_set_page_ms, so the 200 ms gate re-arms after each re-init. */
static inline __attribute__((always_inline)) void handle_sys_pic_now(lcd_app_state_t *state, app_config_t *cfg, uint16_t data16)
{
    if (data16 == 0 && state->boot_complete &&
        (uint32_t)(sys_tick_get_ms() - state->last_set_page_ms) >= 200u) {
        /* Panel self-reset mid-run: the held RUN press is lost and no
         * RUN_RELEASE will arrive, so stop the run (UI lost -> stop the
         * actuator). This also re-syncs ICON_RUN: us_run_status -> IDLE
         * makes the next disp_step see a real edge after init_mode clears
         * the icon (spec §4.3). Harmless when already idle. */
        app_lcd_hook_us_command(US_CMD_RUN_RELEASE);
        s_run_key_down = 0u;   /* panel reset: the release edge never arrives */
        app_lcd_var_init();
        app_lcd_send_model_str(cfg->model_freq, cfg->model_type);
        app_lcd_init_mode(cfg);
    }
}

/* dispatch 본체 — SETUP_PARAM / SETUP_PARAM_MOOHAN 공통: setup1 페이지 진입 + horn 체크박스 미러 (두 case 에서 각각 호출) */
static inline __attribute__((always_inline)) void handle_setup_param_enter(lcd_app_state_t *state)
{
    state->lcd_status = setup1_page_for_mode(state->sys_mode);
    app_lcd_change_page(state->lcd_status);
    /* horn-down 체크박스 = 현재 SYS_HORN 모드 미러 + shadow 리셋 (legacy
     * main.c:3617-3622 verbatim — 저장 시 체크 안 건드리면 temp==0이라
     * 모드 이탈되는 legacy 거동 포함). */
    dgus_write_u16(DISP_HORNDOWN, (uint16_t)app_horn_mode_active());
    state->temp_horndown = 0u;
}

/* dispatch 본체 — LV_MO_TIME1/2 공통 꼬리: time1 > time2 면 time2 를 끌어올리고 에코 (두 case 에서 각각 호출) */
static inline __attribute__((always_inline)) void mo_time_clamp_echo(app_config_t *cfg)
{
    if (cfg->limit_mo_time1 > cfg->limit_mo_time2) {
        cfg->limit_mo_time2 = cfg->limit_mo_time1;          /* samd20 main.c:4011-4015 */
        dgus_write_u16(LV_MO_TIME2, cfg->limit_mo_time2);   /* echo new clamped value */
    }
}

/* dispatch 본체 — LV_RUN_MODE: 1=delay / 2=trigger 로 run_mode 설정 + STD2 페이지 전환 */
static inline __attribute__((always_inline)) void handle_run_mode(lcd_app_state_t *state, app_config_t *cfg, uint16_t data16)
{
    if (data16 == 1) {                               /* delay mode */
        cfg->run_mode = MODE_DELAY;
        state->lcd_status = LCD_SETUP_STD2D;
        app_lcd_change_page(state->lcd_status);
    } else if (data16 == 2) {                        /* trigger mode */
        cfg->run_mode = MODE_TRIGGER;
        state->lcd_status = LCD_SETUP_STD2T;
        app_lcd_change_page(state->lcd_status);
    }
}

```

- [ ] **Step 3: 게이트**

```sh
./fw.sh 2>&1 | grep -E 'warning:|error:' | grep -v '/vendor/'                # 기대: 출력 없음
MODEL=remote ./fw.sh 2>&1 | grep -E 'warning:|error:' | grep -v '/vendor/'   # 기대: 출력 없음
fw/tools/bin-same.sh   # 기대: SAME  fd66f6e7bb9d6ff03f7ea1831797167813b3db4613de66e6c84841b1860e278f c5223adac03c85861e242e592125bf163114c847f383d6304f5d2e0771f7373f
./fw.sh test 2>&1 | grep -ciE 'all (checks |tests )?passed'                  # 기대: 17
```

- [ ] **Step 4: DIFF 면** — `handle_sys_pic_now` 는 실증 ○ 이므로 나머지 셋을 의심한다. `git checkout -- fw/src/app_lcd_input.c` 후 ①만 → `bin-same.sh`; `SAME` 이면 ④(mo_time) 추가 → 재판정; `SAME` 이면 ③ 추가 → 재판정; `SAME` 이면 ② 추가. `DIFF` 를 만든 것만 빼고 커밋하고 spec §6 `app_lcd_input_dispatch` 행 "이번 결과" 에 `<헬퍼명> .bin ≠ → 보류` 를 덧쓴다. 우회 금지.

- [ ] **Step 5: 길이 측정(후)**

```sh
python3 fw/tools/funclen.py fw/src/app_lcd_input.c | grep -E '^(app_lcd_input_dispatch|handle_sys_pic_now|handle_setup_param_enter|mo_time_clamp_echo|handle_run_mode) '
# 기대:
# handle_sys_pic_now 16 (code 11)
# handle_setup_param_enter 10 (code 7)
# mo_time_clamp_echo 7 (code 7)
# handle_run_mode 12 (code 12)
# app_lcd_input_dispatch 182 (code 143)
```

`app_lcd_input_dispatch` 182 (코드 143) 는 spec §6 예외(35 `case…break` 뼈대). Task 8 에서 §6 "이번 결과" 열을 `182줄(코드 143)` 로 기입.

- [ ] **Step 6: Commit**

```sh
git add fw/src/app_lcd_input.c
git commit -m "refactor(lcd): lcd_input_dispatch case 본체 4추출 — .bin 동일 (STD fd66…e278f / REMOTE c522…7373f)

- handle_sys_pic_now(state, cfg, data16) [감사 b-3b-i 실증] / handle_setup_param_enter(state) [2 case 공용] /
  mo_time_clamp_echo(cfg) [LV_MO_TIME1/2 공통 꼬리 — spec 의 handle_mo_time_edit 는 대입 분기 신설이 필요해 대체] /
  handle_run_mode(state, cfg, data16). case 라벨·break 35개 무변경. 225→182줄 (§6 예외 유지)
- bin-same.sh SAME · host 17 PASS · our-code 경고 0"
```

---

### Task 6: 슬라이스 6 — FSM step case 본체 추출 (`weld_fsm_step` 4 · `osc_init_fsm_step` 4 · `seek_reset_fsm_step` 2)

**Files:**
- Modify: `fw/src/app_weld_fsm.c` — `weld_fsm_step` :121-290 (헤더 `/* weld FSM 1틱 진행 */` :120; Task 1 무변경 파일). 헬퍼 4개는 :120 바로 위에. 추출 구간: abort 본체 :129-144 · `case WELD_CYL1:` 본체 :170-181 · `case WELD_WELD:` 본체 :185-239 · `case WELD_CYL2:` 본체 :257-275
- Modify: `fw/src/app_osc_init_fsm.c` — `osc_init_fsm_step` :24-100 (헤더 `/* OSC init FSM 1틱 진행 */` :23). 헬퍼 4개는 :23 바로 위에. 구간: WAIT_H 본체 :30-44 · WAIT_L 본체 :48-57 · RESET 본체 :70-77 · SEEK 본체 :81-87
- Modify: `fw/src/app_seek_reset_fsm.c` — `seek_reset_fsm_step` :22-81 (헤더 `/* SR FSM 1틱 진행 */` :21). 헬퍼 2개는 :21 바로 위에. 구간: RESET 본체 :44-58 · SEEK 본체 :62-71
- host 스위트 `test_app_weld_fsm`(21함수)·`test_app_osc_init_fsm`·`test_app_seek_reset_fsm` 이 세 함수를 직접 호출한다 — 추가 안전망

**Interfaces:**
- Produces: `static inline __attribute__((always_inline)) void weld_abort_body(weld_out_t *out)` — abort 분기 본체(`if (s_run_status == WELD_WELD) out->weld_stop=1` + 래치 12개 클리어 + out 2필드). 판정 `if ((in->abort != 0u) && …)` 와 `return;` 은 호출자(H3/H6). H4 ✓ (`out` 기존 포인터)
- Produces: `static inline __attribute__((always_inline)) void weld_step_cyl1(const weld_in_t *in)` — CYL1 본체. H4 ✓
- Produces: `static inline __attribute__((always_inline)) void weld_step_weld(const weld_in_t *in, weld_out_t *out)` — WELD 본체 55줄 → 헬퍼 58줄 (**>50 — spec §6 기록, 더 쪼개지 않는다**). H4 ✓
- Produces: `static inline __attribute__((always_inline)) void weld_step_cyl2(weld_out_t *out)` — CYL2 본체. **D4**: spec 의 `(in, out)` 에서 `in` 제거(본체가 `in->` 를 읽지 않음)
- Produces: `static inline __attribute__((always_inline)) void osc_step_wait_h(const osc_init_in_t *in)` / `osc_step_wait_l(const osc_init_in_t *in)` / `osc_step_reset(osc_init_out_t *out)` / `osc_step_seek(osc_init_out_t *out)` — **D5**(spec 의 `osc_step_pulse(out)` 1개 대신 reset/seek 2개; wait_l 추가). GAP·DONE·default 는 남긴다
- Produces: `static inline __attribute__((always_inline)) void sr_step_reset(seek_reset_out_t *out)` / `sr_step_seek(seek_reset_out_t *out)` — IDLE 은 남긴다
- 모든 `case X:` 라벨·`break;`·`default:`·`memset`·`out->state = …` 꼬리는 호출자 무변경

- [ ] **Step 1: 길이 측정(전)**

```sh
python3 fw/tools/funclen.py fw/src/app_weld_fsm.c fw/src/app_osc_init_fsm.c fw/src/app_seek_reset_fsm.c | grep -E '^(weld_fsm_step|osc_init_fsm_step|seek_reset_fsm_step) '
# 기대:
# weld_fsm_step 170 (code 141)
# osc_init_fsm_step 77 (code 68)
# seek_reset_fsm_step 60 (code 50)
```

- [ ] **Step 2-A: `weld_fsm_step` — `fw/src/app_weld_fsm.c:120-290` 을 아래 전체로 교체** (헬퍼 4개 + 헤더 + 새 step. 헬퍼 본문 = 원본 case 본체를 4칸 dedent 한 것)

**before** (구조만 — 네 구간의 전문은 아래 헬퍼 본문과 동일):

```c
/* weld FSM 1틱 진행 */
void weld_fsm_step(const weld_in_t *in, weld_out_t *out)
{
    memset(out, 0, sizeof(*out));

    /* abort (slice4 §3.4): ... (3줄) */
    if ((in->abort != 0u) && (s_run_status != WELD_READY)) {
        if (s_run_status == WELD_WELD) {
        ⋮   (:129-144, 16줄 → weld_abort_body 본문)
        out->sol_dn      = s_sol_dn;
        return;
    }
    ⋮   (:147-168 그대로: 엣지 래치, 타이머, switch, case WELD_READY 본체)
    case WELD_CYL1:
        if (s_f_status_start == 0u) {
        ⋮   (:170-181, 12줄 → weld_step_cyl1 본문)
        }
        break;

    case WELD_WELD:
        if (s_f_status_start == 0u) {
        ⋮   (:185-239, 55줄 → weld_step_weld 본문)
        }
        break;

    case WELD_HOLD:
        ⋮   (:243-253 그대로)
        break;

    case WELD_CYL2:
        if (s_f_status_start == 0u) {
        ⋮   (:257-275, 19줄 → weld_step_cyl2 본문)
        }
        break;

    default:
        ⋮   (:279-285 그대로)
    }

    out->run_status = s_run_status;
    out->sol_dn     = s_sol_dn;
}
```

**after**:

```c
/* weld_fsm_step 본체 — abort: 임의 상태 → SOL OFF + READY + 내부 래치 전부 클리어 (판정·return 은 호출자) */
static inline __attribute__((always_inline)) void weld_abort_body(weld_out_t *out)
{
    if (s_run_status == WELD_WELD) {
        out->weld_stop = 1u;
    }
    s_sol_dn         = 0u;
    s_run_status     = WELD_READY;
    s_f_status_start = 0u;
    s_temp_time      = 0u;
    s_multi_stage    = 0u;
    s_multi_elapsed  = 0u;
    s_latched_multi  = 0u;
    s_latched_energy = 0u;
    s_run_mode       = 0u;
    s_dn_pressed     = 0u;
    s_up_pressed     = 0u;
    out->run_status  = s_run_status;
    out->sol_dn      = s_sol_dn;
}

/* weld_fsm_step 본체 — case WELD_CYL1: 첫 진입 SOL_DN ON, TRIGGER 는 dn 엣지 / DELAY 는 타이머로 WELD 전이 */
static inline __attribute__((always_inline)) void weld_step_cyl1(const weld_in_t *in)
{
    if (s_f_status_start == 0u) {
        s_f_status_start = 1u;
        s_sol_dn         = 1u;       /* SOL_DN ON (cylinder descends) */
    } else if (s_run_mode != 0u) {   /* TRIGGER (main.c:1513-1528) */
        if (s_dn_pressed != 0u) {
            s_dn_pressed = 0u;
            enter_weld(in, in->limit_trigger_time2);
        }
        /* dn 없음 -> 무기한 대기 (죽은 legacy CYL_TIMEOUT 충실히 미구현, spec §3.2). */
    } else if (s_temp_time == 0u) {          /* WELD_CYL1 전이 블록 (DELAY) */
        enter_weld(in, in->limit_delay_time2);
    }
}

/* weld_fsm_step 본체 — case WELD_WELD: 첫 진입 진폭+weld_start, 이후 multi > energy > 시간 exit (55줄 — spec §6 기록) */
static inline __attribute__((always_inline)) void weld_step_weld(const weld_in_t *in, weld_out_t *out)
{
    if (s_f_status_start == 0u) {
        s_f_status_start = 1u;
        if (s_latched_multi) {       /* H1: 전이 시점 스냅샷 (in->multi_ctrl 아님 —
                                         무장-직후 1-tick 토글 창 제거) */
            s_multi_stage   = 0u;
            s_multi_elapsed = 1u;  /* weld_start step은 elapsed=1로 시작 (전환 step 포함, slice-1 s_temp_time 정합) */
            out->amplitude  = weld_mo_amplitude(in->limit_mo_out1);  /* 1단 (comp 미적용) */
        } else {
            out->amplitude  = weld_amplitude(in->output_power, s_comp_time);
        }
        out->weld_start  = 1u;       /* glue: US_CYCLE START + pot write */
    } else if (s_latched_multi) {
        /* multi: 2단 진폭 스테핑 (samd20 5232-5258). 우선순위 최상 — energy/시간
         * exit 미발동(아래 else-if 분기 진입 안 함). spec §3.4. H1: 전이 시점
         * 스냅샷(s_latched_multi) 참조 — 런중 in->multi_ctrl 토글 무시. */
        if (s_multi_elapsed < 0xFFFFu) {
            s_multi_elapsed++;                   /* 포화 가드 (time2 매우 클 때 wrap 방지) */
        }
        if (s_multi_stage == 0u && s_multi_elapsed >= in->limit_mo_time1) {
            s_multi_stage   = 1u;
            out->amplitude  = weld_mo_amplitude(in->limit_mo_out2);  /* 2단 (samd20 5242) */
            out->amp_change = 1u;                /* glue: set_amp 재호출 */
        }
        if (s_multi_elapsed >= in->limit_mo_time2) {
            out->weld_stop   = 1u;               /* samd20 5250: WELD->HOLD */
            s_f_status_start = 0u;
            s_run_status     = WELD_HOLD;
            s_temp_time      = hold_time(in);
        }
    } else if (s_latched_energy) {
        /* energy 모드: 에너지 도달 -> 정상 종료(samd20 5272); 미도달 +
         * backstop 만료 -> abort(samd20 5288, 에러 표시는 이연). spec §3.3. H1:
         * 전이 시점 스냅샷(s_latched_energy) 참조 — 런중 in->energy_ctrl 토글 무시. */
        if ((in->limit_energy != 0u) && (in->curr_energy >= in->limit_energy)) {
            out->weld_stop   = 1u;
            s_f_status_start = 0u;
            s_run_status     = WELD_HOLD;
            s_temp_time      = hold_time(in);
        } else if (s_temp_time == 0u) {
            out->weld_stop   = 1u;   /* abort도 US 정지 */
            out->weld_fault  = 1u;   /* glue: fault hook → app_reg_raise_ovtime
                                      * (ERR_OVTIME=legacy SYS_ERROR, 2026-07-18) */
            s_sol_dn         = 0u;   /* 실린더 즉시 상승 */
            s_f_status_start = 0u;
            s_run_status     = WELD_READY;   /* CYL2 미경유, work_cnt++ 없음 */
            s_latched_multi  = 0u;            /* H1: READY 복귀 지점 클리어 */
            s_latched_energy = 0u;
        }
    } else if (s_temp_time == 0u) {
        /* slice-1 시간-exit (energy_ctrl off) — 무회귀. */
        out->weld_stop   = 1u;
        s_f_status_start = 0u;
        s_run_status     = WELD_HOLD;
        s_temp_time      = hold_time(in);
    }
}

/* weld_fsm_step 본체 — case WELD_CYL2: 첫 진입 SOL_DN OFF, TRIGGER 는 up 래치 / DELAY 는 타이머로 READY + cycle_done */
static inline __attribute__((always_inline)) void weld_step_cyl2(weld_out_t *out)
{
    if (s_f_status_start == 0u) {
        s_f_status_start = 1u;
        s_sol_dn         = 0u;       /* SOL_DN OFF (cylinder rises) */
    } else if (s_run_mode != 0u) {   /* TRIGGER (main.c:1618-1629) */
        if (s_up_pressed != 0u) {
            s_up_pressed     = 0u;
            s_f_status_start = 0u;
            s_run_status     = WELD_READY;
            out->cycle_done  = 1u;       /* glue: work_cnt++ */
            s_latched_multi  = 0u;       /* H1: READY 복귀 지점 클리어 */
            s_latched_energy = 0u;
        }
    } else if (s_temp_time == 0u) {
        s_f_status_start = 0u;
        s_run_status     = WELD_READY;
        out->cycle_done  = 1u;       /* glue: work_cnt++ */
        s_latched_multi  = 0u;       /* H1: READY 복귀 지점 클리어 */
        s_latched_energy = 0u;
    }
}

/* weld FSM 1틱 진행 */
void weld_fsm_step(const weld_in_t *in, weld_out_t *out)
{
    memset(out, 0, sizeof(*out));

    /* abort (slice4 §3.4): 임의 상태 -> SOL OFF + READY. WELD 중이면 US 정지 엣지
     * (글루 US_CYCLE RUN_RELEASE — slice-c/d force-stop과 이중 안전). work_cnt 미발행.
     * legacy: E-stop main.c:1415 / SYS_ERROR 1664-1665. */
    if ((in->abort != 0u) && (s_run_status != WELD_READY)) {
        weld_abort_body(out);
        return;
    }

    /* slice4: SENSE_DN/UP 1-shot 엣지 -> 내부 래치(legacy re_dn_pressed/re_up_pressed
     * 재현). 소비는 각 상태 분기(CYL1/CYL2)에서. */
    if (in->dn_edge) { s_dn_pressed = 1u; }
    if (in->up_edge) { s_up_pressed = 1u; }

    /* 10 ms elapsed: active timer counts down (samd20 timer temp_time--). */
    if (s_temp_time > 0u) {
        s_temp_time--;
    }

    switch (s_run_status) {
    case WELD_READY:
        if (in->start) {
            s_run_status     = WELD_CYL1;
            s_run_mode       = in->run_mode;          /* 사이클 단위 래치 (slice4) */
            s_temp_time      = in->limit_delay_time1; /* TRIGGER에선 미사용 (아래 분기) */
            s_f_status_start = 0u;       /* CYL1 first-entry drives SOL_DN ON */
            s_dn_pressed     = 0u;       /* stale 클리어 (main.c:1478) */
        }
        break;

    case WELD_CYL1:
        weld_step_cyl1(in);
        break;

    case WELD_WELD:
        weld_step_weld(in, out);
        break;

    case WELD_HOLD:
        if (s_f_status_start == 0u) {
            s_f_status_start = 1u;
        } else if (s_temp_time == 0u) {
            s_f_status_start = 0u;
            s_run_status     = WELD_CYL2;
            s_temp_time      = in->limit_delay_time1;  /* DELAY용; TRIGGER는 미사용 */
            if (s_run_mode != 0u) {
                s_up_pressed = 1u;   /* legacy 강제 set (main.c:1593 "//-") — CYL2 즉시
                                      * exit. SENSE_UP 실대기 필요 판정 시 이 줄 제거 (spec §3.3). */
            }
        }
        break;

    case WELD_CYL2:
        weld_step_cyl2(out);
        break;

    default:
        /* unreachable in normal operation (s_run_status is only ever a WELD_*
         * value); fail-safe on fault — drop the solenoid (cpp-review LOW-2). */
        s_run_status     = WELD_READY;
        s_sol_dn         = 0u;
        s_latched_multi  = 0u;          /* H1: READY 복귀 지점 클리어 */
        s_latched_energy = 0u;
        break;
    }

    out->run_status = s_run_status;
    out->sol_dn     = s_sol_dn;
}
```

- [ ] **Step 2-B: `osc_init_fsm_step` — `fw/src/app_osc_init_fsm.c:23-100` 을 아래 전체로 교체**

**before** (구조만; 네 case 본체 전문 = 아래 헬퍼 본문, 4칸 dedent):

```c
/* OSC init FSM 1틱 진행 */
void osc_init_fsm_step(const osc_init_in_t *in, osc_init_out_t *out)
{
    memset(out, 0, sizeof(*out));

    switch (s_state) {
    case OSC_WAIT_H:                       /* PB12 H(초음파 출력 시작) 대기 + 폴백 */
        if (in->pb12) {
        ⋮   (:30-44, 15줄 → osc_step_wait_h 본문)
        }
        break;

    case OSC_WAIT_L:                       /* PB12 L(출력 종료) 대기 + 폴백 */
        if (!in->pb12) {
        ⋮   (:48-57, 10줄 → osc_step_wait_l 본문)
        }
        break;

    case OSC_GAP:                          /* 종료 후 150ms 갭 */
        ⋮   (:61-66 그대로)
        break;

    case OSC_RESET:                        /* RESET 펄스 200ms */
        if (s_elapsed < 0xFFFFu) { s_elapsed++; }
        ⋮   (:70-77, 8줄 → osc_step_reset 본문)
        }
        break;

    case OSC_SEEK:                         /* SEEK 펄스 100ms */
        if (s_elapsed < 0xFFFFu) { s_elapsed++; }
        ⋮   (:81-87, 7줄 → osc_step_seek 본문)
        }
        break;

    case OSC_DONE:                         /* terminal: 재실행 없음, 출력 idle */
        break;

    default:
        ⋮   (:94-96 그대로)
    }

    out->state = s_state;
}
```

**after**:

```c
/* osc_init_fsm_step 본체 — case OSC_WAIT_H: PB12 H 디바운스 → WAIT_L, 미감지 900ms 폴백 */
static inline __attribute__((always_inline)) void osc_step_wait_h(const osc_init_in_t *in)
{
    if (in->pb12) {
        if (s_h_debounce < 0xFFu) { s_h_debounce++; }
        if (s_h_debounce >= OSC_H_DEBOUNCE) {   /* 연속 H 샘플 → 스파이크 배제 */
            s_state      = OSC_WAIT_L;
            s_elapsed    = 0u;
            s_h_debounce = 0u;
        }
    } else {
        s_h_debounce = 0u;             /* H 끊김 → 디바운스 리셋 */
        if (s_elapsed < 0xFFFFu) { s_elapsed++; }
        if (s_elapsed >= OSC_WAIT_H_TIMEOUT) {  /* 보드 부재/고장 폴백 */
            s_state   = OSC_WAIT_L;
            s_elapsed = 0u;
        }
    }
}

/* osc_init_fsm_step 본체 — case OSC_WAIT_L: PB12 L → GAP, 미감지 900ms 폴백 */
static inline __attribute__((always_inline)) void osc_step_wait_l(const osc_init_in_t *in)
{
    if (!in->pb12) {
        s_state   = OSC_GAP;
        s_elapsed = 0u;
    } else {
        if (s_elapsed < 0xFFFFu) { s_elapsed++; }
        if (s_elapsed >= OSC_WAIT_L_TIMEOUT) {  /* 출력 안 떨어짐 폴백 */
            s_state   = OSC_GAP;
            s_elapsed = 0u;
        }
    }
}

/* osc_init_fsm_step 본체 — case OSC_RESET: RESET 펄스 레벨 유지, 만료 시 SEEK 펄스 시작 */
static inline __attribute__((always_inline)) void osc_step_reset(osc_init_out_t *out)
{
    if (s_elapsed < 0xFFFFu) { s_elapsed++; }
    if (s_elapsed >= OSC_RESET_TICKS) {
        s_state          = OSC_SEEK;
        s_elapsed        = 0u;
        out->seek_signal = 1u;         /* SEEK 펄스 시작 엣지 (reset off) */
    } else {
        out->reset_signal = 1u;        /* 레벨 유지 */
    }
}

/* osc_init_fsm_step 본체 — case OSC_SEEK: SEEK 펄스 레벨 유지, 만료 시 DONE */
static inline __attribute__((always_inline)) void osc_step_seek(osc_init_out_t *out)
{
    if (s_elapsed < 0xFFFFu) { s_elapsed++; }
    if (s_elapsed >= OSC_SEEK_TICKS) {
        s_state   = OSC_DONE;
        s_elapsed = 0u;                /* 완료 (seek off) */
    } else {
        out->seek_signal = 1u;         /* 레벨 유지 */
    }
}

/* OSC init FSM 1틱 진행 */
void osc_init_fsm_step(const osc_init_in_t *in, osc_init_out_t *out)
{
    memset(out, 0, sizeof(*out));

    switch (s_state) {
    case OSC_WAIT_H:                       /* PB12 H(초음파 출력 시작) 대기 + 폴백 */
        osc_step_wait_h(in);
        break;

    case OSC_WAIT_L:                       /* PB12 L(출력 종료) 대기 + 폴백 */
        osc_step_wait_l(in);
        break;

    case OSC_GAP:                          /* 종료 후 150ms 갭 */
        if (s_elapsed < 0xFFFFu) { s_elapsed++; }
        if (s_elapsed >= OSC_GAP_TICKS) {
            s_state           = OSC_RESET;
            s_elapsed         = 0u;
            out->reset_signal = 1u;        /* RESET 펄스 시작 엣지 */
        }
        break;

    case OSC_RESET:                        /* RESET 펄스 200ms */
        osc_step_reset(out);
        break;

    case OSC_SEEK:                         /* SEEK 펄스 100ms */
        osc_step_seek(out);
        break;

    case OSC_DONE:                         /* terminal: 재실행 없음, 출력 idle */
        break;

    default:
        /* unreachable; fail-safe → 완료 처리 (재초기화 펄스 폭주 방지). */
        s_state = OSC_DONE;
        break;
    }

    out->state = s_state;
}
```

- [ ] **Step 2-C: `seek_reset_fsm_step` — `fw/src/app_seek_reset_fsm.c:21-81` 을 아래 전체로 교체**

**before** (구조만; 두 case 본체 전문 = 아래 헬퍼 본문, 4칸 dedent):

```c
/* SR FSM 1틱 진행 */
void seek_reset_fsm_step(const seek_reset_in_t *in, seek_reset_out_t *out)
{
    memset(out, 0, sizeof(*out));

    switch (s_state) {
    case SR_IDLE:
        ⋮   (:28-40 그대로)
        break;

    case SR_RESET:                        /* cmd 무시 (busy) */
        /* 포화 가드: SR_TICKS=60 고정이라 s_elapsed는 최대 60에서 전이 — 현재는
        ⋮   (:44-58, 15줄 → sr_step_reset 본문)
        }
        break;

    case SR_SEEK:                         /* cmd 무시 (busy) */
        if (s_elapsed < 0xFFFFu) {
        ⋮   (:62-71, 10줄 → sr_step_seek 본문)
        }
        break;

    default:
        ⋮   (:75-77 그대로)
    }

    out->state = s_state;
}
```

**after**:

```c
/* seek_reset_fsm_step 본체 — case SR_RESET: 600ms 레벨 유지 → SEEK 자동 체인 (icon off/on 엣지) */
static inline __attribute__((always_inline)) void sr_step_reset(seek_reset_out_t *out)
{
    /* 포화 가드: SR_TICKS=60 고정이라 s_elapsed는 최대 60에서 전이 — 현재는
     * 미발동이나 config-driven 비교로 확장될 때 대비해 weld 패턴 유지
     * (cpp-review Minor 1). SR_SEEK도 동일. */
    if (s_elapsed < 0xFFFFu) {
        s_elapsed++;
    }
    if (s_elapsed >= SR_TICKS) {      /* 600ms 경과 → SEEK 자동 체인 (samd20 5395-5396) */
        out->reset_icon_off = 1u;     /* reset_signal=0 (memset), off 엣지 */
        out->seek_signal    = 1u;
        out->seek_icon      = 1u;     /* on 엣지 */
        s_state             = SR_SEEK;
        s_elapsed           = 0u;
    } else {
        out->reset_signal = 1u;       /* 레벨 유지 */
    }
}

/* seek_reset_fsm_step 본체 — case SR_SEEK: 600ms 레벨 유지 → IDLE 자동 해제 (icon off 엣지) */
static inline __attribute__((always_inline)) void sr_step_seek(seek_reset_out_t *out)
{
    if (s_elapsed < 0xFFFFu) {
        s_elapsed++;
    }
    if (s_elapsed >= SR_TICKS) {      /* 600ms 경과 → 자동 해제 (samd20 5403-5407) */
        out->seek_icon_off = 1u;      /* seek_signal=0 (memset), off 엣지 */
        s_state            = SR_IDLE;
        s_elapsed          = 0u;
    } else {
        out->seek_signal = 1u;        /* 레벨 유지 */
    }
}

/* SR FSM 1틱 진행 */
void seek_reset_fsm_step(const seek_reset_in_t *in, seek_reset_out_t *out)
{
    memset(out, 0, sizeof(*out));

    switch (s_state) {
    case SR_IDLE:
        if (in->run_active) {
            /* RUN 직교: RUN 중 SEEK/RESET 명령 무시 (spec §3.4). */
        } else if (in->cmd == SR_CMD_RESET) {
            s_state           = SR_RESET;
            s_elapsed         = 0u;
            out->reset_signal = 1u;
            out->reset_icon   = 1u;       /* on 엣지 */
        } else if (in->cmd == SR_CMD_SEEK) {
            s_state           = SR_SEEK;
            s_elapsed         = 0u;
            out->seek_signal  = 1u;
            out->seek_icon    = 1u;       /* on 엣지 */
        }
        break;

    case SR_RESET:                        /* cmd 무시 (busy) */
        sr_step_reset(out);
        break;

    case SR_SEEK:                         /* cmd 무시 (busy) */
        sr_step_seek(out);
        break;

    default:
        /* unreachable (s_state는 항상 SR_*); fail-safe (weld LOW-2 패턴). */
        s_state = SR_IDLE;
        break;
    }

    out->state = s_state;
}
```

- [ ] **Step 3: 게이트**

```sh
./fw.sh 2>&1 | grep -E 'warning:|error:' | grep -v '/vendor/'                # 기대: 출력 없음
MODEL=remote ./fw.sh 2>&1 | grep -E 'warning:|error:' | grep -v '/vendor/'   # 기대: 출력 없음
fw/tools/bin-same.sh   # 기대: SAME  fd66f6e7bb9d6ff03f7ea1831797167813b3db4613de66e6c84841b1860e278f c5223adac03c85861e242e592125bf163114c847f383d6304f5d2e0771f7373f
./fw.sh test 2>&1 | grep -ciE 'all (checks |tests )?passed'                  # 기대: 17  (weld/osc/seek_reset 스위트 포함)
```

- [ ] **Step 4: DIFF 면** — 세 파일은 독립. 파일 단위로 가른다: `git checkout -- fw/src/app_seek_reset_fsm.c` → `bin-same.sh`; `SAME` 이면 seek_reset 만 보류(§6 행 추가 `seek_reset_fsm_step | Task 6 .bin ≠ | 보류 60줄`), 아니면 되돌린 것을 재적용하고 `fw/src/app_osc_init_fsm.c` 를 되돌려 재판정, 그다음 `fw/src/app_weld_fsm.c`. `DIFF` 를 만든 파일만 원본으로 두고 커밋. 우회 금지.

- [ ] **Step 5: 길이 측정(후)**

```sh
python3 fw/tools/funclen.py fw/src/app_weld_fsm.c fw/src/app_osc_init_fsm.c fw/src/app_seek_reset_fsm.c | grep -E '^(weld_fsm_step|weld_abort_body|weld_step_cyl1|weld_step_weld|weld_step_cyl2|osc_init_fsm_step|osc_step_wait_h|osc_step_wait_l|osc_step_reset|osc_step_seek|seek_reset_fsm_step|sr_step_reset|sr_step_seek) '
# 기대:
# weld_abort_body 19 (code 19)
# weld_step_cyl1 15 (code 14)
# weld_step_weld 58 (code 49)
# weld_step_cyl2 22 (code 22)
# weld_fsm_step 72 (code 53)
# osc_step_wait_h 18 (code 18)
# osc_step_wait_l 13 (code 13)
# osc_step_reset 11 (code 11)
# osc_step_seek 10 (code 10)
# osc_init_fsm_step 41 (code 32)
# sr_step_reset 18 (code 15)
# sr_step_seek 13 (code 13)
# seek_reset_fsm_step 37 (code 30)
```

`weld_fsm_step` 72 (코드 53) — abort 판정·엣지 래치·타이머·READY/HOLD/default 뼈대만으로 >50. spec §6 의 예상("case 본체 추출 후 `weld_step_weld` 가 >50 잔존")에 **`weld_fsm_step` 자체 72 도 함께** 기록한다(Task 8). READY(7줄)·HOLD(11줄)·default(4줄)까지 헬퍼로 뽑아도 53 이라 규칙 위반으로 보지 않고 멈춘다(spec §8 "쪼갤수록 H4 경계").

- [ ] **Step 6: Commit**

```sh
git add fw/src/app_weld_fsm.c fw/src/app_osc_init_fsm.c fw/src/app_seek_reset_fsm.c
git commit -m "refactor(fsm): weld/osc/seek_reset step case 본체 추출 (4+4+2) — .bin 동일 (STD fd66…e278f / REMOTE c522…7373f)

- weld: weld_abort_body(out) [판정·return 호출자] / weld_step_cyl1(in) / weld_step_weld(in,out) 58줄(§6) / weld_step_cyl2(out) — spec 의 (in,out) 중 in 미사용 제거. 170→72줄(§6 기록)
- osc: osc_step_wait_h/wait_l(in) + osc_step_reset/seek(out) — spec 의 osc_step_pulse 1개는 상수가 달라 2개로. 77→41줄
- seek_reset: sr_step_reset/seek(out). 60→37줄
- case 라벨·break·memset·out->state 꼬리 무변경. bin-same.sh SAME · host 17 PASS(weld 21함수 포함) · our-code 경고 0"
```

---

### Task 7: 슬라이스 7 — 조건부 5건 (H4 경계 사례). 각 건 독립 — 시도 → 게이트 → `SAME` 이면 유지 / `DIFF` 면 **그 파일만** `git checkout` 으로 되돌리고 spec §6 에 "보류" 기록. 우회 재시도 금지 (H7)

다섯 건은 서로 다른 파일이라 한 건의 되돌림이 다른 건을 건드리지 않는다. 순서대로 7-1 → 7-5 를 진행하고, 살아남은 것만 **커밋 1개**로 묶는다(Step 8). 아래 좌표는 `b61ef0f` = Task 6 후 좌표(이 네 파일은 Task 1~6 이 건드리지 않았다; `app_lcd_input.c` 의 `handle_std_setup_param` 만 Task 1 로 +1).

- [ ] **Step 0: 길이 측정(전)**

```sh
python3 fw/tools/funclen.py fw/src/app_lcd_render.c fw/src/app_weld.c fw/src/app_modbus_tcp.c fw/src/app_lcd_disp.c fw/src/app_lcd_input.c | grep -E '^(app_lcd_change_page|app_weld_tick|app_modbus_tcp_poll|disp_compute_output|handle_std_setup_param) '
# 기대:
# app_lcd_change_page 203 (code 177)
# app_weld_tick 131 (code 95)
# app_modbus_tcp_poll 111 (code 75)
# disp_compute_output 76 (code 54)
# handle_std_setup_param 58 (code 52)
```

#### 7-1. `app_lcd_change_page` — 로컬 배열 포인터 전달 (H4 경계)

**Files:** Modify `fw/src/app_lcd_render.c` — `app_lcd_change_page` :40-242 (헤더 `/* 페이지 렌더+전환 */` :39). 헬퍼 4개는 :39 바로 위에. 추출 구간: RUN_STD 본체 :58-106 · SETUP_HAND/MULTI/STD1 본체 :108-120 · STDC/MHC 공통부 :138-167 · STDE/MHE 공통부 :173-202 (두 공통부는 글자 단위 동일 — 헬퍼 1개를 두 분기에서 호출) · comm 꼬리 본체 :227-232. 호출자 로컬 선언 `uint16_t i;`(:47) 와 `uint8_t  n;`(:48) 은 **삭제**(헬퍼 안으로 이동 — 미삭제 시 `-Wunused-variable`)

**Interfaces:**
- `static inline __attribute__((always_inline)) void render_run_std(const app_config_t *cfg, uint8_t *buf)` — 헬퍼 로컬 `uint8_t n;`. H4 경계: `buf` 는 호출자 로컬 배열의 포인터(원본에서도 `dgus_write_bytes(…, buf, …)`·`time2str(…, &buf[4])` 로 주소가 이미 잡혀 있었다 → 스택 스필 신규 없음이 기대되지만 **미실측**)
- `static inline __attribute__((always_inline)) void render_setup_main(const app_config_t *cfg, lcd_app_state_t *state)` — 기존 포인터 2개(H4 ✓)
- `static inline __attribute__((always_inline)) void render_comm_page(const app_config_t *cfg, lcd_app_state_t *state, uint8_t *addr_str, char *ipbuf)` — 헬퍼 로컬 `uint16_t i; uint8_t n;`. 두 분기에서 각각 호출(인라인 2벌 = 원본의 중복 그대로)
- `static inline __attribute__((always_inline)) void render_comm_tail(uint8_t page, const lcd_app_state_t *state)` — set_page 후 재기록 본체(외곽 `if (page == MHC||STDC||MHE||STDE)` 는 호출자)
- 결과가 `SAME` 이어도 `app_lcd_change_page` 는 78줄(코드 57) — 8분기 else-if 뼈대 + STD2D/STD2T/STD3 분기 + 꼬리. spec §6 예외 유지. `render_run_std` 53줄도 §6 기록

**before** (`:39-242` 중 바뀌는 자리만; 네 구간 전문 = 아래 헬퍼 본문):

```c
/* 페이지 렌더+전환 */
void app_lcd_change_page(uint8_t page)
{
    /* Render one panel page, then switch to it. Verbatim port of samd20
     * change_lcd_page (main.c:2942-3173). */
    app_config_t    *cfg   = app_lcd_cfg();
    lcd_app_state_t *state = app_lcd_state();

    uint16_t i;
    uint8_t  n;                 /* formatter return length (samd20 'temp') */
    uint8_t  addr_str[4];       /* conv_addr2str field (samd20 'temp_str') */
    uint8_t  buf[20];           /* line-build scratch (samd20 global lcd_temp_buf) */
    char     ipbuf[16];         /* ip_to_string scratch */

    /* --- unconditional top (main.c:2947-2955) --- */
    dgus_write_u16(DISP_ENERGY_EN, cfg->energy_ctrl ? 1u : 0u);
    dgus_write_u16(DISP_MULTI_EN,  cfg->multi_ctrl  ? 1u : 0u);

    if (page == LCD_RUN_STD) {
        dgus_write_u16(LV_DM_DELAY,    cfg->limit_delay_time1);
        ⋮   (:58-106, 49줄 → render_run_std 본문)
        }
    } else if (page == LCD_SETUP_HAND || page == LCD_SETUP_MULTI || page == LCD_SETUP_STD1) {
        dgus_write_bytes(DISP_VERSION, (const uint8_t *)VERSION_MSG, 20);
        ⋮   (:108-120, 13줄 → render_setup_main 본문)
        state->temp_comm_mode  = 0xff;
    } else if (page == LCD_SETUP_STD2D) {
        ⋮   (:122-136 그대로 — STD2D / STD2T / STD3·MH2)
    } else if (page == LCD_SETUP_STDC || page == LCD_SETUP_MHC) {
        dgus_write_u16(COMM_ADDR,   cfg->comm_address);
        ⋮   (:138-167, 30줄 → render_comm_page 본문)
        }
        if (state->temp_comm_mode > 0)
            dgus_write_u16(DISP_COMM_MODE, 1);
        else
            dgus_write_u16(DISP_COMM_MODE, 0);
    } else if (page == LCD_SETUP_STDE || page == LCD_SETUP_MHE) {
        dgus_write_u16(COMM_ADDR,   cfg->comm_address);
        ⋮   (:173-202, 30줄 — :138-167 과 동일 → 같은 헬퍼)
        }
        if (state->temp_comm_mode == 0) {
        ⋮   (:203-209 그대로)
    }

    dgus_set_page(page);
    ⋮   (:213-224 그대로)
    if (page == LCD_SETUP_MHC || page == LCD_SETUP_STDC ||
        page == LCD_SETUP_MHE || page == LCD_SETUP_STDE) {
        dgus_write_u16(DISP_COMM_MODE, (state->temp_comm_mode == 0) ? 0u : 1u);
        ⋮   (:227-232, 6줄 → render_comm_tail 본문)
        }
    }
    ⋮   (:234-242 그대로)
}
```

**after** — `:39-242` 를 아래 전체로 교체:

```c
/* change_page 본체 — LCD_RUN_STD: DELAY/TRIGGER 별 D/W(E)/H 3줄 텍스트 */
static inline __attribute__((always_inline)) void render_run_std(const app_config_t *cfg, uint8_t *buf)
{
    uint8_t n;                  /* formatter return length (samd20 'temp') */
    dgus_write_u16(LV_DM_DELAY,    cfg->limit_delay_time1);
    dgus_write_u16(DISP_RUN_MODE,  cfg->run_mode);
    dgus_write_u16(DISP_SAFTY,     cfg->f_safty);
    dgus_write_u16(LV_LIMIT_OUT_T, cfg->limit_out_time);

    if (cfg->run_mode == MODE_DELAY) {
        buf[0] = 'D'; buf[1] = ' '; buf[2] = ':'; buf[3] = ' ';
        n = time2str(cfg->limit_delay_time1, &buf[4]);
        dgus_write_bytes(DISP_STD_DATA1, buf, (uint8_t)(n + 4));

        if (cfg->energy_ctrl) {
            buf[0] = 'E'; buf[1] = ' '; buf[2] = ':'; buf[3] = ' ';
            n = energy2str(cfg->limit_energy, &buf[4]);
            dgus_write_bytes(DISP_STD_DATA2, buf, (uint8_t)(n + 4));
        } else {
            buf[0] = 'W'; buf[1] = ' '; buf[2] = ':'; buf[3] = ' ';
            if (cfg->multi_ctrl)
                n = time2str((uint16_t)(cfg->limit_mo_time1 + cfg->limit_mo_time2), &buf[4]);
            else
                n = time2str(cfg->limit_delay_time2, &buf[4]);
            dgus_write_bytes(DISP_STD_DATA2, buf, (uint8_t)(n + 4));
        }

        buf[0] = 'H'; buf[1] = ' '; buf[2] = ':'; buf[3] = ' ';
        n = time2str(cfg->limit_delay_time3, &buf[4]);
        dgus_write_bytes(DISP_STD_DATA3, buf, (uint8_t)(n + 4));
    } else if (cfg->run_mode == MODE_TRIGGER) {
        buf[0] = 'S'; buf[1] = 'E'; buf[2] = 'N'; buf[3] = 'S';
        buf[4] = 'O'; buf[5] = 'R'; buf[6] = ' '; buf[7] = 'O';
        buf[8] = 'F'; buf[9] = 'F'; buf[10] = '\0';
        dgus_write_bytes(DISP_STD_DATA1, buf, 11);

        if (cfg->energy_ctrl) {
            buf[0] = 'E'; buf[1] = ' '; buf[2] = ':'; buf[3] = ' ';
            n = energy2str(cfg->limit_energy, &buf[4]);
            dgus_write_bytes(DISP_STD_DATA2, buf, (uint8_t)(n + 4));
        } else {
            buf[0] = 'W'; buf[1] = ' '; buf[2] = ':'; buf[3] = ' ';
            if (cfg->multi_ctrl)
                n = time2str((uint16_t)(cfg->limit_mo_time1 + cfg->limit_mo_time2), &buf[4]);
            else
                n = time2str(cfg->limit_trigger_time2, &buf[4]);
            dgus_write_bytes(DISP_STD_DATA2, buf, (uint8_t)(n + 4));
        }

        buf[0] = 'H'; buf[1] = ' '; buf[2] = ':'; buf[3] = ' ';
        n = time2str(cfg->limit_trigger_time3, &buf[4]);
        dgus_write_bytes(DISP_STD_DATA3, buf, (uint8_t)(n + 4));
    }
}

/* change_page 본체 — SETUP_HAND/MULTI/STD1: 버전·on_time·power(+pot)·energy + comm shadow 시드/센티널 */
static inline __attribute__((always_inline)) void render_setup_main(const app_config_t *cfg, lcd_app_state_t *state)
{
    dgus_write_bytes(DISP_VERSION, (const uint8_t *)VERSION_MSG, 20);
    dgus_write_u16(LV_MAX_ON_TIME, cfg->limit_on_time);
    dgus_write_u16(LV_OUT_POWER,   cfg->output_power);
    app_lcd_hook_set_pot(cfg->output_power);   /* samd20 inline I2C_POT write -> hook (실구동) */
    dgus_write_u32(LV_ENERGY_VAL,  cfg->limit_energy);
    dgus_write_u16(LV_ENERGY_EDIT, (uint16_t)cfg->limit_energy);
    dgus_write_u16(DISP_ENERGY_EN, cfg->energy_ctrl ? 1u : 0u);
    dgus_write_u16(LV_LIMIT_OUT_T, cfg->limit_out_time);
    state->temp_parity_idx = cfg->comm_parity_idx;
    state->temp_address    = cfg->comm_address;
    state->temp_speed_idx  = cfg->comm_speed_idx;
    state->temp_cnt_reset  = 0;
    state->temp_comm_mode  = 0xff;
}

/* change_page 본체 — STDC/MHC·STDE/MHE 공통: comm 3필드 에코 + shadow + (첫 진입) ether 텍스트 시드 (두 분기에서 각각 호출) */
static inline __attribute__((always_inline)) void render_comm_page(const app_config_t *cfg, lcd_app_state_t *state,
                                                  uint8_t *addr_str, char *ipbuf)
{
    uint16_t i;
    uint8_t  n;
    dgus_write_u16(COMM_ADDR,   cfg->comm_address);
    dgus_write_u16(COMM_SPEED,  cfg->comm_speed_idx);
    dgus_write_u16(COMM_PARITY, cfg->comm_parity_idx);
    state->temp_parity_idx = cfg->comm_parity_idx;
    state->temp_address    = cfg->comm_address;
    state->temp_speed_idx  = cfg->comm_speed_idx;
    conv_addr2str(cfg->comm_address, addr_str);
    dgus_write_bytes(COMM_ADDR_TXT, addr_str, 4);
    dgus_write_bytes(COMM_SPEED_TXT, comm_speed_txt[cfg->comm_speed_idx], 6);
    dgus_write_bytes(COMM_PARITY_TXT, comm_parity_txt[cfg->comm_parity_idx], 4);
    if (state->temp_comm_mode == 0xff) {
        state->temp_comm_mode = cfg->comm_mode;
        for (i = 0; i < 4; i++) {
            state->temp_ether_ip[i] = cfg->ether_ip[i];
            state->temp_ether_nm[i] = cfg->ether_nm[i];
            state->temp_ether_gw[i] = cfg->ether_gw[i];
        }
        n = ip_to_string(state->temp_ether_ip, ipbuf);
        dgus_write_bytes(COMM_IP_TXT, (const uint8_t *)ipbuf, n);
        n = ip_to_string(state->temp_ether_nm, ipbuf);
        dgus_write_bytes(COMM_NM_TXT, (const uint8_t *)ipbuf, n);
        n = ip_to_string(state->temp_ether_gw, ipbuf);
        dgus_write_bytes(COMM_GW_TXT, (const uint8_t *)ipbuf, n);
        state->ether_current_number   = 0;
        state->ether_current_octet    = 0;
        state->ether_has_input        = false;
        state->ether_ip_input_complete = false;
        state->ether_buffer_pos       = 0;
        state->ether_what_input       = LCD_ETHER_INPUT_NONE;
    }
}

/* change_page 본체 — comm 페이지 set_page 후 DISP_COMM_MODE/EN_DHCP 재기록 (판정은 호출자) */
static inline __attribute__((always_inline)) void render_comm_tail(uint8_t page, const lcd_app_state_t *state)
{
    dgus_write_u16(DISP_COMM_MODE, (state->temp_comm_mode == 0) ? 0u : 1u);
    if (page == LCD_SETUP_MHE || page == LCD_SETUP_STDE) {
        dgus_write_u16(DISP_EN_DHCP,
                       (state->temp_comm_mode == 0)
                           ? 0u : (uint16_t)(state->temp_comm_mode - 1));
    }
}

/* 페이지 렌더+전환 */
void app_lcd_change_page(uint8_t page)
{
    /* Render one panel page, then switch to it. Verbatim port of samd20
     * change_lcd_page (main.c:2942-3173). */
    app_config_t    *cfg   = app_lcd_cfg();
    lcd_app_state_t *state = app_lcd_state();

    uint8_t  addr_str[4];       /* conv_addr2str field (samd20 'temp_str') */
    uint8_t  buf[20];           /* line-build scratch (samd20 global lcd_temp_buf) */
    char     ipbuf[16];         /* ip_to_string scratch */

    /* --- unconditional top (main.c:2947-2955) --- */
    dgus_write_u16(DISP_ENERGY_EN, cfg->energy_ctrl ? 1u : 0u);
    dgus_write_u16(DISP_MULTI_EN,  cfg->multi_ctrl  ? 1u : 0u);

    if (page == LCD_RUN_STD) {
        render_run_std(cfg, buf);
    } else if (page == LCD_SETUP_HAND || page == LCD_SETUP_MULTI || page == LCD_SETUP_STD1) {
        render_setup_main(cfg, state);
    } else if (page == LCD_SETUP_STD2D) {
        dgus_write_u16(LV_DM_DELAY, cfg->limit_delay_time1);
        dgus_write_u16(LV_DM_WELD,  cfg->limit_delay_time2);
        dgus_write_u16(LV_DM_HOLD,  cfg->limit_delay_time3);
        state->temp_comm_mode = 0xff;
    } else if (page == LCD_SETUP_STD2T) {
        dgus_write_u16(LV_TM_WELD, cfg->limit_trigger_time2);
        dgus_write_u16(LV_TM_HOLD, cfg->limit_trigger_time3);
        state->temp_comm_mode = 0xff;
    } else if (page == LCD_SETUP_STD3 || page == LCD_SETUP_MH2) {
        dgus_write_u16(DISP_MULTI_EN, cfg->multi_ctrl ? 1u : 0u);
        dgus_write_u16(LV_MO_OUT1,  cfg->limit_mo_out1);
        dgus_write_u16(LV_MO_OUT2,  cfg->limit_mo_out2);
        dgus_write_u16(LV_MO_TIME1, cfg->limit_mo_time1);
        dgus_write_u16(LV_MO_TIME2, cfg->limit_mo_time2);
        state->temp_comm_mode = 0xff;
    } else if (page == LCD_SETUP_STDC || page == LCD_SETUP_MHC) {
        render_comm_page(cfg, state, addr_str, ipbuf);
        if (state->temp_comm_mode > 0)
            dgus_write_u16(DISP_COMM_MODE, 1);
        else
            dgus_write_u16(DISP_COMM_MODE, 0);
    } else if (page == LCD_SETUP_STDE || page == LCD_SETUP_MHE) {
        render_comm_page(cfg, state, addr_str, ipbuf);
        if (state->temp_comm_mode == 0) {
            dgus_write_u16(DISP_COMM_MODE, 0);
            dgus_write_u16(DISP_EN_DHCP, 0);   /* fix D: clear stale DHCP check on serial */
        } else {
            dgus_write_u16(DISP_COMM_MODE, 1);
            dgus_write_u16(DISP_EN_DHCP, (uint16_t)(state->temp_comm_mode - 1));
        }
    }

    dgus_set_page(page);
    state->last_set_page_ms = sys_tick_get_ms();   /* SYS_PIC_NOW loop guard (spec §10) */

    /* Comm-mode icon re-assert AFTER set_page (panel page 23/27 quirk, verified
     * 2026-05-31). The STD comm pages STDC(23)/STDE(27) do NOT auto-load
     * DISP_COMM_MODE on page-show — only a live write while the page is active
     * updates the serial/ethernet/DHCP icon (the same path the touch handler
     * uses). The MULTI pages MHC(21)/MHE(25) auto-load, so the in-branch write
     * before set_page suffices there; re-asserting is redundant-but-harmless.
     * Proven by repoint cross-test: identical firmware rendered on page 25 shows
     * ethernet correctly, on page 27 shows serial. temp_comm_mode is already
     * seeded (0/1/2) by the branch above on every comm page.
     * See analysis/2026-05-31-std-comm-page27-display-port-faithful.md. */
    if (page == LCD_SETUP_MHC || page == LCD_SETUP_STDC ||
        page == LCD_SETUP_MHE || page == LCD_SETUP_STDE) {
        render_comm_tail(page, state);
    }
#ifdef LCD_TRACE_RX
    /* comm-page display diagnosis: page id + seeded shadow + persisted cfg.
     * 21=MHC 23=STDC 25=MHE 27=STDE. From page+tcm the DISP_COMM_MODE/EN_DHCP
     * writes are fully determined by the render logic above. */
    mon_printf("[lcd] page=%u tcm=%u cm=%u\r\n",
               (unsigned)page, (unsigned)state->temp_comm_mode,
               (unsigned)cfg->comm_mode);
#endif
}
```

- [ ] **7-1 게이트**

```sh
./fw.sh 2>&1 | grep -E 'warning:|error:' | grep -v '/vendor/'                # 기대: 출력 없음
MODEL=remote ./fw.sh 2>&1 | grep -E 'warning:|error:' | grep -v '/vendor/'   # 기대: 출력 없음
fw/tools/bin-same.sh   # 기대: SAME  fd66f6e7bb9d6ff03f7ea1831797167813b3db4613de66e6c84841b1860e278f c5223adac03c85861e242e592125bf163114c847f383d6304f5d2e0771f7373f
./fw.sh test 2>&1 | grep -ciE 'all (checks |tests )?passed'                  # 기대: 17
python3 fw/tools/funclen.py fw/src/app_lcd_render.c | grep -E '^(app_lcd_change_page|render_run_std|render_setup_main|render_comm_page|render_comm_tail) '
# SAME 시 기대: render_run_std 53 (code 48) / render_setup_main 16 (code 16) / render_comm_page 36 (code 36) / render_comm_tail 9 (code 9) / app_lcd_change_page 78 (code 57)
```

- [ ] **7-1 DIFF 면**: `git checkout -- fw/src/app_lcd_render.c` → `fw/tools/bin-same.sh` 로 `SAME` 복귀 확인. spec §6 `app_lcd_change_page` 행 "이번 결과" 열에 `Task 7-1 .bin ≠ (STD/REMOTE 해시) → 보류, 203줄 잔존` 기입. (`SAME` 이면 "이번 결과" = `78줄(코드 57) — render_* 4 헬퍼; render_run_std 53 잔존`.)

#### 7-2. `app_weld_tick` — 로컬 구조체 주소 전달 (H4 경계)

**Files:** Modify `fw/src/app_weld.c` — `app_weld_tick` :97-227 (헤더 `/* weld 글루 10ms tick */` :96). 헬퍼 2개는 :96 바로 위에. 구간: 센서 미러 본체 :149-152 (판정 `if (tin.sens_dn != s_sens_dn_bak)` :148 은 호출자) · out 디스패치 :205-226

**Interfaces:**
- `static inline __attribute__((always_inline)) void weld_sensor_mirror(const app_config_t *cfg, uint8_t sens_dn)` — 본체의 `tin.sens_dn` 을 인자 `sens_dn` 으로(값). H4 ✓
- `static inline __attribute__((always_inline)) void weld_dispatch_out(app_config_t *cfg, const weld_out_t *out)` — out 8분기. 본체의 `out.` 이 `out->` 으로 바뀐다(텍스트 변화, 의미 동일). H4 경계: `&out` 은 원본에서 `weld_fsm_step(&in, &out)` 로 **이미 주소가 잡힌** 로컬 → 스택 상주는 기존과 같다(감사 b-1v 의 `&save` 는 원래 레지스터 변수였던 점이 다르다). 미실측
- `SAME` 이어도 `app_weld_tick` 은 107줄(코드 71) — `weld_in_t` 지정 초기화 22줄·트리거 스캔·게이팅·클램프가 남는다(spec §4 표: "`weld_in_t` 채우기는 지정 초기화라 헬퍼화 시 코드 변화 가능" → 시도하지 않음). Task 8 §6 추가 행

**before** (`:96-227` 중 바뀌는 자리):

```c
/* weld 글루 10ms tick */
void app_weld_tick(void)
{
    ⋮   (:99-147 그대로)
    if (tin.sens_dn != s_sens_dn_bak) {
        if (cfg->run_mode != 0u) {
            app_lcd_weld_sensor_text(tin.sens_dn == 0u);   /* active-LOW: 0=감지=ON */
        }
        s_sens_dn_bak = tin.sens_dn;
    }
    ⋮   (:154-204 그대로)
    if (out.sol_dn != s_sol_last) {
        s_sol_last = out.sol_dn;
        app_weld_hook_sol_dn(out.sol_dn != 0u);
    }
    if (out.weld_start) {
        app_weld_hook_set_amp(out.amplitude);   /* raw DAC, NOT set_pot (double-convert) */
        app_reg_command(US_CMD_START, (uint8_t)US_CYCLE);
    }
    if (out.amp_change) {
        app_weld_hook_set_amp(out.amplitude);   /* mid-WELD 2단 진폭 (US_CYCLE 유지, START 아님) */
    }
    if (out.weld_stop) {
        app_reg_command(US_CMD_RUN_RELEASE, (uint8_t)US_CYCLE);
    }
    if (out.weld_fault) {
        app_weld_hook_fault();
    }
    if (out.cycle_done) {
        cfg->work_cnt++;
        app_config_save_all(cfg);
        app_lcd_set_work_cnt(cfg->work_cnt);
    }
}
```

**after** — 헬퍼 2개를 `:96` 위에 삽입하고, 두 구간을 호출로 교체:

```c
/* app_weld_tick 본체 — SENSE_DN 레벨 변화: TRIGGER 모드면 SENSOR ON/OFF 텍스트, bak 갱신 (판정은 호출자) */
static inline __attribute__((always_inline)) void weld_sensor_mirror(const app_config_t *cfg, uint8_t sens_dn)
{
    if (cfg->run_mode != 0u) {
        app_lcd_weld_sensor_text(sens_dn == 0u);   /* active-LOW: 0=감지=ON */
    }
    s_sens_dn_bak = sens_dn;
}

/* app_weld_tick 본체 — FSM out 이벤트 → SOL_DN hook / set_amp / US_CYCLE START·RELEASE / fault / work_cnt++ */
static inline __attribute__((always_inline)) void weld_dispatch_out(app_config_t *cfg, const weld_out_t *out)
{
    if (out->sol_dn != s_sol_last) {
        s_sol_last = out->sol_dn;
        app_weld_hook_sol_dn(out->sol_dn != 0u);
    }
    if (out->weld_start) {
        app_weld_hook_set_amp(out->amplitude);   /* raw DAC, NOT set_pot (double-convert) */
        app_reg_command(US_CMD_START, (uint8_t)US_CYCLE);
    }
    if (out->amp_change) {
        app_weld_hook_set_amp(out->amplitude);   /* mid-WELD 2단 진폭 (US_CYCLE 유지, START 아님) */
    }
    if (out->weld_stop) {
        app_reg_command(US_CMD_RUN_RELEASE, (uint8_t)US_CYCLE);
    }
    if (out->weld_fault) {
        app_weld_hook_fault();
    }
    if (out->cycle_done) {
        cfg->work_cnt++;
        app_config_save_all(cfg);
        app_lcd_set_work_cnt(cfg->work_cnt);
    }
}

/* weld 글루 10ms tick */
void app_weld_tick(void)
{
    ⋮   (:99-147 그대로)
    if (tin.sens_dn != s_sens_dn_bak) {
        weld_sensor_mirror(cfg, tin.sens_dn);
    }
    ⋮   (:154-204 그대로 — … weld_out_t out; weld_fsm_step(&in, &out); 까지)

    weld_dispatch_out(cfg, &out);
}
```

- [ ] **7-2 게이트**

```sh
./fw.sh 2>&1 | grep -E 'warning:|error:' | grep -v '/vendor/'                # 기대: 출력 없음
MODEL=remote ./fw.sh 2>&1 | grep -E 'warning:|error:' | grep -v '/vendor/'   # 기대: 출력 없음
fw/tools/bin-same.sh   # 기대: SAME  fd66f6e7bb9d6ff03f7ea1831797167813b3db4613de66e6c84841b1860e278f c5223adac03c85861e242e592125bf163114c847f383d6304f5d2e0771f7373f
./fw.sh test 2>&1 | grep -ciE 'all (checks |tests )?passed'                  # 기대: 17
python3 fw/tools/funclen.py fw/src/app_weld.c | grep -E '^(app_weld_tick|weld_sensor_mirror|weld_dispatch_out) '
# SAME 시 기대: weld_sensor_mirror 7 (code 7) / weld_dispatch_out 25 (code 25) / app_weld_tick 107 (code 71)
```

- [ ] **7-2 DIFF 면**: `git checkout -- fw/src/app_weld.c` → `bin-same.sh` `SAME` 확인. spec §6 에 행 추가 `| app_weld_tick | 로컬 out 구조체 주소 전달(H4 경계) — Task 7-2 .bin ≠ | 보류 131줄 | HW 벤치 트랙 |`. (`SAME` 이면 `| app_weld_tick | weld_in_t 지정 초기화 22줄 + 트리거 스캔이 남는다 | 107줄(코드 71) | host 트랙 없음(글루) |`.)

#### 7-3. `app_modbus_tcp_poll` — recv 블록 / flush 꼬리 (값 인자)

**Files:** Modify `fw/src/app_modbus_tcp.c` — `app_modbus_tcp_poll` :123-233 (헤더 `/* TCP 서버 poll */` :122). 헬퍼 2개는 :122 바로 위에. 구간: 수신 누적 :131-155 (`if (s_acc_len == 0u) return;` :156-158 은 호출자 — H6) · flush 꼬리 :217-232

**Interfaces:**
- `static inline __attribute__((always_inline)) void tcp_recv_accumulate(void)` — `space`·`avail`·`iomode`·`got` 헬퍼 로컬, `s_acc`·`s_acc_len` 파일 static 직접. 인자 없음
- `static inline __attribute__((always_inline)) void tcp_flush(uint16_t off, uint16_t tx_len)` — 값 인자 2개(H4 ✓). 워커 루프(`off`/`tx_len` 출력 필요)는 호출자에 남는다(spec §4: out-포인터 ✗)
- `SAME` 이어도 `app_modbus_tcp_poll` 은 72줄(코드 49) — 워커 루프 44줄 + 주석 20줄. Task 8 §6 추가 행

**before** (`:122-233` 중 바뀌는 자리):

```c
/* TCP 서버 poll */
void app_modbus_tcp_poll(void)
{
    control_tcp();

    if (getSn_SR(MB_TCP_SOCK) != SOCK_ESTABLISHED) {
        return;
    }

    /* 수신 누적: 남은 공간만큼만. 최대 프레임(131) < ACC(262)라 완전 프레임
    ⋮   (:131-155, 25줄 → tcp_recv_accumulate 본문)
    }
    if (s_acc_len == 0u) {
        return;
    }
    ⋮   (:160-216 그대로 — 워커 루프)
    if (off != 0u) {
    ⋮   (:217-232, 16줄 → tcp_flush 본문)
    }
}
```

**after** — 헬퍼 2개를 `:122` 위에 삽입, 두 구간을 호출로 교체:

```c
/* tcp_poll 본체 — 소켓 RX 를 누적 버퍼 남은 공간만큼 recv (블로킹 모드 일시 토글) */
static inline __attribute__((always_inline)) void tcp_recv_accumulate(void)
{
    /* 수신 누적: 남은 공간만큼만. 최대 프레임(131) < ACC(262)라 완전 프레임
     * 없이 버퍼가 차는 경우는 DESYNC-급 garbage뿐 → 아래서 폐기됨. */
    uint16_t space = (uint16_t)(MB_TCP_ACC_LEN - s_acc_len);
    uint16_t avail = getSn_RX_RSR(MB_TCP_SOCK);
    if ((avail != 0u) && (space != 0u)) {
        if (avail > space) {
            avail = space;
        }
        /* ⚠ recv만 블로킹 모드로 일시 토글: 이 vendored socket.c의 비-IPv6
         * recv 경로(:687-692)는 논블로킹 체크가 recvsize 체크보다 앞이라
         * NONBLOCK이면 데이터가 있어도 무조건 SOCK_BUSY 반환(업스트림과
         * 순서 뒤집힘 — HW E2E 전면 무응답으로 발견, 2026-07-05). RSR>0
         * 가드 후 호출이라 블로킹 recv도 즉시 반환(RSR은 RECV 커맨드 전엔
         * 감소 불가; 피어 RST/close 시 벤더가 에러 반환 → 스톨 불가).
         * vendor read-only → ctlsocket 공개 API 우회. send/disconnect는
         * NONBLOCK 유지 (M9). */
        uint8_t iomode = SOCK_IO_BLOCK;
        (void)ctlsocket(MB_TCP_SOCK, CS_SET_IOMODE, &iomode);
        int32_t got = recv(MB_TCP_SOCK, &s_acc[s_acc_len], avail);
        iomode = SOCK_IO_NONBLOCK;
        (void)ctlsocket(MB_TCP_SOCK, CS_SET_IOMODE, &iomode);
        if (got > 0) {
            s_acc_len = (uint16_t)(s_acc_len + (uint16_t)got);
        }
    }
}

/* tcp_poll 본체 — 처리한 프레임 제거(partial 선두 이동) + 코얼레스드 응답 send */
static inline __attribute__((always_inline)) void tcp_flush(uint16_t off, uint16_t tx_len)
{
    if (off != 0u) {
        s_acc_len = (uint16_t)(s_acc_len - off);
        if (s_acc_len != 0u) {
            memmove(s_acc, &s_acc[off], s_acc_len);   /* partial 선두 이동 */
        }
    }

    if (tx_len != 0u) {
        int32_t sent = send(MB_TCP_SOCK, s_txacc, tx_len);
        if (sent != (int32_t)tx_len) {
            /* SOCK_BUSY(직전 SENDOK 미도래)/SOCKERR_TIMEOUT(벤더가 close —
             * FSM이 재오픈)/기타 — 드롭, 마스터 재시도 (spec §3). */
            mon_printf("[mbtcp] send drop r=%ld len=%u\r\n",
                       (long)sent, (unsigned)tx_len);
        }
    }
}

/* TCP 서버 poll */
void app_modbus_tcp_poll(void)
{
    control_tcp();

    if (getSn_SR(MB_TCP_SOCK) != SOCK_ESTABLISHED) {
        return;
    }

    tcp_recv_accumulate();
    if (s_acc_len == 0u) {
        return;
    }
    ⋮   (:160-216 그대로 — 워커 루프, `}` 까지)
    tcp_flush(off, tx_len);
}
```

- [ ] **7-3 게이트**

```sh
./fw.sh 2>&1 | grep -E 'warning:|error:' | grep -v '/vendor/'                # 기대: 출력 없음
MODEL=remote ./fw.sh 2>&1 | grep -E 'warning:|error:' | grep -v '/vendor/'   # 기대: 출력 없음
fw/tools/bin-same.sh   # 기대: SAME  fd66f6e7bb9d6ff03f7ea1831797167813b3db4613de66e6c84841b1860e278f c5223adac03c85861e242e592125bf163114c847f383d6304f5d2e0771f7373f
./fw.sh test 2>&1 | grep -ciE 'all (checks |tests )?passed'                  # 기대: 17
python3 fw/tools/funclen.py fw/src/app_modbus_tcp.c | grep -E '^(app_modbus_tcp_poll|tcp_recv_accumulate|tcp_flush) '
# SAME 시 기대: tcp_recv_accumulate 28 (code 18) / tcp_flush 19 (code 16) / app_modbus_tcp_poll 72 (code 49)
```

- [ ] **7-3 DIFF 면**: `git checkout -- fw/src/app_modbus_tcp.c` → `bin-same.sh` `SAME` 확인. spec §6 행 추가 `| app_modbus_tcp_poll | Task 7-3 .bin ≠ | 보류 111줄 | HW 벤치 트랙 |`. (`SAME` 이면 `| app_modbus_tcp_poll | 워커 루프가 off/tx_len 출력을 요구(out-포인터 ✗) | 72줄(코드 49) | host test_app_modbus_tcp_frame 는 프레임 계층만 |`.)

#### 7-4. `disp_compute_output` — 대역 fill 2개 (값 인자) + 15줄 설명 주석 헤더 이동

**Files:** Modify `fw/src/app_lcd_disp.c` — `disp_compute_output` :51-126 (헤더 `/* 출력 파워 바 계산 */` :50; Task 1 은 :171 이후만 건드려 좌표 불변). 헬퍼 2개는 :50 바로 위에. 구간: 상위 대역 :78-95 (`if (curr_amp >= st->ref_lv_20) {…} else {…}`) · 하위 대역 :97-105 (`uint8_t fill;` ~ for 끝). 설명 주석 :54-68 (15줄, C1) 은 헤더로 병합(Task 1 규칙, 태그 `fill 규칙`). 호출자 로컬 `uint16_t span;`(:71) 은 **삭제**(두 헬퍼가 각자 선언 — 미삭제 시 `-Wunused-variable`)

**Interfaces:**
- `static inline __attribute__((always_inline)) void fill_upper_band(uint16_t curr_amp, const lcd_app_state_t *st)` — 헬퍼 로컬 `uint8_t i; uint16_t span;`. H4 ✓ (값 + 기존 포인터). `level_buf` 파일 static 직접(H5 ✓). H4 경계: 호출자 로컬 `span`·`i` 가 헬퍼마다 별도 변수로 갈라진다 → 레지스터 할당이 달라질 수 있다(미실측)
- `static inline __attribute__((always_inline)) void fill_lower_band(uint16_t curr_amp, const lcd_app_state_t *st)` — 동상

**before** (`:50-126`):

```c
/* 출력 파워 바 계산 */
static void disp_compute_output(uint16_t curr_amp, uint8_t out_power,
                                const lcd_app_state_t *st)
{
    /* Output-power bar fill (port of send_outpower_data step==0, main.c:2616-2661).
    ⋮   (:54-68, 15줄 → 헤더로)
     * Then unconditionally: marker slot = 1. */
    uint8_t  i;
    uint8_t  marker;
    uint16_t span;          /* threshold gap, guarded != 0 before dividing */

    if (curr_amp > 10u) {
        level_buf[0] = 1u;
        if (curr_amp > st->ref_lv_1) {
            level_buf[1] = 1u;
            if (curr_amp > st->ref_lv_10) {
                if (curr_amp >= st->ref_lv_20) {
                ⋮   (:78-95, 18줄 → fill_upper_band 본문, 12칸 dedent)
                }
            } else {
                uint8_t fill;
                ⋮   (:97-105, 9줄 → fill_lower_band 본문, 12칸 dedent)
                }
            }
        } else {
        ⋮   (:107-125 그대로)
    level_buf[marker] = 1u;
}
```

**after** — `:50-126` 을 아래 전체로 교체:

```c
/* disp_compute_output 본체 — curr_amp > ref_lv_10: 상위 대역 fill (>= ref_lv_20 full / 아니면 slot 2..9 + 10..10+t) */
static inline __attribute__((always_inline)) void fill_upper_band(uint16_t curr_amp, const lcd_app_state_t *st)
{
    uint8_t  i;
    uint16_t span;
    if (curr_amp >= st->ref_lv_20) {
        for (i = 1u; i < 20u; i++) {
            level_buf[i] = 1u;
        }
    } else {
        uint8_t fill;
        for (i = 2u; i < 10u; i++) {
            level_buf[i] = 1u;
        }
        /* t = (amp-ref_lv_10)*10 / (ref_lv_20-ref_lv_10) */
        span = (uint16_t)(st->ref_lv_20 - st->ref_lv_10);
        fill = (span != 0u)
            ? (uint8_t)(((uint32_t)(curr_amp - st->ref_lv_10) * 10u) / span)
            : 0u;
        for (i = 0u; i < 10u; i++) {
            level_buf[10u + i] = (i < fill) ? 1u : 0u;
        }
    }
}

/* disp_compute_output 본체 — ref_lv_1 < curr_amp <= ref_lv_10: 하위 대역 fill (slot 2..2+t) */
static inline __attribute__((always_inline)) void fill_lower_band(uint16_t curr_amp, const lcd_app_state_t *st)
{
    uint8_t  i;
    uint16_t span;
    uint8_t fill;
    /* t = (amp-ref_lv_1)*8 / (ref_lv_10-ref_lv_1) */
    span = (uint16_t)(st->ref_lv_10 - st->ref_lv_1);
    fill = (span != 0u)
        ? (uint8_t)(((uint32_t)(curr_amp - st->ref_lv_1) * 8u) / span)
        : 0u;
    for (i = 0u; i < 18u; i++) {
        level_buf[2u + i] = (i < fill) ? 1u : 0u;
    }
}

/* 출력 파워 바 계산.
 * [fill 규칙] Output-power bar fill (port of send_outpower_data step==0, main.c:2616-2661).
 *
 * Thresholds (state->ref_lv_1/10/20) seed from model_freq in app_lcd_init_mode.
 * Set-point marker lands at slot (output_power * 20 / 100), clamped to 19.
 *
 *   curr_amp <= 10            -> all 20 slots 0
 *   curr_amp >  10            -> slot[0]=1
 *     curr_amp <= ref_lv_1    -> slots[1..19]=0
 *     curr_amp >  ref_lv_1
 *       curr_amp <= ref_lv_10 -> slots[2..2+t-1]=1 where t=(amp-ref_lv_1)*8/(ref_lv_10-ref_lv_1)
 *       curr_amp >  ref_lv_10
 *         curr_amp >= ref_lv_20 -> slots[1..19]=1 (full)
 *         else                  -> slots[2..9]=1 and slots[10..10+t-1]=1
 *                                  where t=(amp-ref_lv_10)*10/(ref_lv_20-ref_lv_10)
 * Then unconditionally: marker slot = 1.
 */
static void disp_compute_output(uint16_t curr_amp, uint8_t out_power,
                                const lcd_app_state_t *st)
{
    uint8_t  i;
    uint8_t  marker;

    if (curr_amp > 10u) {
        level_buf[0] = 1u;
        if (curr_amp > st->ref_lv_1) {
            level_buf[1] = 1u;
            if (curr_amp > st->ref_lv_10) {
                fill_upper_band(curr_amp, st);
            } else {
                fill_lower_band(curr_amp, st);
            }
        } else {
            for (i = 1u; i < 20u; i++) {
                level_buf[i] = 0u;
            }
        }
    } else {
        for (i = 0u; i < 20u; i++) {
            level_buf[i] = 0u;
        }
    }

    /* Set-point marker: slot (out_power*20/100), clamped to 19 (main.c:2658-2661).
     * samd20 fidelity: this fires unconditionally, so even at curr_amp==0 (all-clear
     * branch above) a single marker dot is set. Intentional — do NOT "fix". */
    marker = (uint8_t)(((uint16_t)out_power * 20u) / 100u);
    if (marker >= 20u) {
        marker = 19u;
    }
    level_buf[marker] = 1u;
}
```

- [ ] **7-4 게이트**

```sh
./fw.sh 2>&1 | grep -E 'warning:|error:' | grep -v '/vendor/'                # 기대: 출력 없음
MODEL=remote ./fw.sh 2>&1 | grep -E 'warning:|error:' | grep -v '/vendor/'   # 기대: 출력 없음
fw/tools/bin-same.sh   # 기대: SAME  fd66f6e7bb9d6ff03f7ea1831797167813b3db4613de66e6c84841b1860e278f c5223adac03c85861e242e592125bf163114c847f383d6304f5d2e0771f7373f
./fw.sh test 2>&1 | grep -ciE 'all (checks |tests )?passed'                  # 기대: 17
python3 fw/tools/funclen.py fw/src/app_lcd_disp.c | grep -E '^(disp_compute_output|fill_upper_band|fill_lower_band) '
# SAME 시 기대: fill_upper_band 23 (code 22) / fill_lower_band 14 (code 13) / disp_compute_output 35 (code 30)
```

- [ ] **7-4 DIFF 면**: `git checkout -- fw/src/app_lcd_disp.c` → `bin-same.sh` `SAME` 확인. spec §6 행 추가 `| disp_compute_output | 로컬 span/i 분할(H4 경계) — Task 7-4 .bin ≠ | 보류 76줄 | HW 벤치 트랙(바그래프 육안) |`. **주석 15줄 헤더 이동만은 되살릴 수 있다**(C1, 바이너리 무영향): 되돌린 뒤 :54-68 만 헤더로 옮기고 `bin-same.sh` `SAME` 확인 → 76→61줄. 그것도 §6 행에 적는다.

#### 7-5. `handle_std_setup_param` — case 1/2 본체 (기존 포인터)

**Files:** Modify `fw/src/app_lcd_input.c` — Task 1 후 `handle_std_setup_param` :333-390 (헤더 `/* SETUP 페이지 내비 */` :332; `b61ef0f` :332-389). 헬퍼 2개는 :332 바로 위에. 구간(Task 1 후 좌표): `data16 == 1` 본체 :342-350 · `data16 == 2` 본체 :352-364

**Interfaces:**
- `static inline __attribute__((always_inline)) void goto_setup1(lcd_app_state_t *state, const app_config_t *cfg)` — if/else + `dgus_set_page`. H4 ✓ (기존 포인터 2개)
- `static inline __attribute__((always_inline)) void goto_setup2(lcd_app_state_t *state, const app_config_t *cfg)` — if/else-if + `app_lcd_change_page`. H4 ✓
- 판정 `if (data16 == 1) … else if (data16 == 2)` 는 호출자(H3). case 3/4/5 본체는 남긴다(≤50 이면 충분)

**before** (`:332-365`):

```c
/* SETUP 페이지 내비 */
static void handle_std_setup_param(uint16_t data16)
{
    /* STD_SETUP_PARAM (0x1020) page-nav cases 1..5 (samd20 main.c:3885-3963).
     * Updates state->lcd_status and switches page. NB case 1 uses set_page only
     * (no render rebuild); cases 2/3/4 use change_lcd_page. */
    lcd_app_state_t *state = app_lcd_state();
    app_config_t    *cfg   = app_lcd_cfg();

    if (data16 == 1) {                                  /* GOTO SETUP PAGE 1 */
        if (state->lcd_status == LCD_SETUP_MH2 ||
            state->lcd_status == LCD_SETUP_MHC ||
            state->lcd_status == LCD_SETUP_MHE) {
            if (cfg->model_type == 0)        state->lcd_status = LCD_SETUP_HAND;
            else if (cfg->model_type == 1)   state->lcd_status = LCD_SETUP_MULTI;
        } else {
            state->lcd_status = LCD_SETUP_STD1;
        }
        dgus_set_page(state->lcd_status);               /* samd20 set_lcd_page only (no rebuild) */
    } else if (data16 == 2) {                           /* GOTO SETUP PAGE 2 */
        if (state->lcd_status == LCD_SETUP_STD1 ||
            state->lcd_status == LCD_SETUP_STD3 ||
            state->lcd_status == LCD_SETUP_STDC ||
            state->lcd_status == LCD_SETUP_STDE) {
            state->lcd_status = (cfg->run_mode == MODE_DELAY)
                                ? LCD_SETUP_STD2D : LCD_SETUP_STD2T;
        } else if (state->lcd_status == LCD_SETUP_MULTI ||
                   state->lcd_status == LCD_SETUP_HAND ||
                   state->lcd_status == LCD_SETUP_MHC ||
                   state->lcd_status == LCD_SETUP_MHE) {
            state->lcd_status = LCD_SETUP_MH2;
        }
        app_lcd_change_page(state->lcd_status);
    } else if (data16 == 3) {                           /* GOTO SETUP PAGE 3 */
```

**after** — 헬퍼 2개를 `:332` 위에 삽입, 두 본체를 호출로 교체:

```c
/* handle_std_setup_param 본체 — case 1: SETUP 페이지 1 로 (MH2/MHC/MHE 면 모델별 HAND/MULTI, 아니면 STD1; set_page 만) */
static inline __attribute__((always_inline)) void goto_setup1(lcd_app_state_t *state, const app_config_t *cfg)
{
    if (state->lcd_status == LCD_SETUP_MH2 ||
        state->lcd_status == LCD_SETUP_MHC ||
        state->lcd_status == LCD_SETUP_MHE) {
        if (cfg->model_type == 0)        state->lcd_status = LCD_SETUP_HAND;
        else if (cfg->model_type == 1)   state->lcd_status = LCD_SETUP_MULTI;
    } else {
        state->lcd_status = LCD_SETUP_STD1;
    }
    dgus_set_page(state->lcd_status);               /* samd20 set_lcd_page only (no rebuild) */
}

/* handle_std_setup_param 본체 — case 2: SETUP 페이지 2 로 (STD 계열 → run_mode 별 STD2D/T, MH 계열 → MH2; change_page) */
static inline __attribute__((always_inline)) void goto_setup2(lcd_app_state_t *state, const app_config_t *cfg)
{
    if (state->lcd_status == LCD_SETUP_STD1 ||
        state->lcd_status == LCD_SETUP_STD3 ||
        state->lcd_status == LCD_SETUP_STDC ||
        state->lcd_status == LCD_SETUP_STDE) {
        state->lcd_status = (cfg->run_mode == MODE_DELAY)
                            ? LCD_SETUP_STD2D : LCD_SETUP_STD2T;
    } else if (state->lcd_status == LCD_SETUP_MULTI ||
               state->lcd_status == LCD_SETUP_HAND ||
               state->lcd_status == LCD_SETUP_MHC ||
               state->lcd_status == LCD_SETUP_MHE) {
        state->lcd_status = LCD_SETUP_MH2;
    }
    app_lcd_change_page(state->lcd_status);
}

/* SETUP 페이지 내비 */
static void handle_std_setup_param(uint16_t data16)
{
    /* STD_SETUP_PARAM (0x1020) page-nav cases 1..5 (samd20 main.c:3885-3963).
     * Updates state->lcd_status and switches page. NB case 1 uses set_page only
     * (no render rebuild); cases 2/3/4 use change_lcd_page. */
    lcd_app_state_t *state = app_lcd_state();
    app_config_t    *cfg   = app_lcd_cfg();

    if (data16 == 1) {                                  /* GOTO SETUP PAGE 1 */
        goto_setup1(state, cfg);
    } else if (data16 == 2) {                           /* GOTO SETUP PAGE 2 */
        goto_setup2(state, cfg);
    } else if (data16 == 3) {                           /* GOTO SETUP PAGE 3 */
```

(이하 case 3/4/5 는 원문 그대로.)

- [ ] **7-5 게이트**

```sh
./fw.sh 2>&1 | grep -E 'warning:|error:' | grep -v '/vendor/'                # 기대: 출력 없음
MODEL=remote ./fw.sh 2>&1 | grep -E 'warning:|error:' | grep -v '/vendor/'   # 기대: 출력 없음
fw/tools/bin-same.sh   # 기대: SAME  fd66f6e7bb9d6ff03f7ea1831797167813b3db4613de66e6c84841b1860e278f c5223adac03c85861e242e592125bf163114c847f383d6304f5d2e0771f7373f
./fw.sh test 2>&1 | grep -ciE 'all (checks |tests )?passed'                  # 기대: 17
python3 fw/tools/funclen.py fw/src/app_lcd_input.c | grep -E '^(handle_std_setup_param|goto_setup1|goto_setup2) '
# SAME 시 기대: goto_setup1 12 (code 12) / goto_setup2 16 (code 16) / handle_std_setup_param 38 (code 32)
```

- [ ] **7-5 DIFF 면**: `app_lcd_input.c` 에는 Task 5 의 커밋된 헬퍼가 있으므로 `git checkout -- fw/src/app_lcd_input.c` 는 **Task 5 커밋 상태**로 되돌린다(안전). `bin-same.sh` `SAME` 확인. spec §6 행 추가 `| handle_std_setup_param | Task 7-5 .bin ≠ | 보류 58줄 | LCD 터치 E2E |`.

- [ ] **Step 8: 슬라이스 7 Commit (살아남은 건만)**

```sh
git status --short fw/     # 기대: 7-1~7-5 중 SAME 인 파일만 M
python3 fw/tools/funclen.py fw/src/*.c fw/drivers/*.c | awk '$2>50'   # Task 8 표와 대조 (전부 SAME 이면 11줄)
git add fw/src/app_lcd_render.c fw/src/app_weld.c fw/src/app_modbus_tcp.c fw/src/app_lcd_disp.c fw/src/app_lcd_input.c
git commit -m "refactor(cond): 조건부 헬퍼 추출 5건 중 N건 — .bin 동일 (STD fd66…e278f / REMOTE c522…7373f)

- SAME: <7-1 change_page render_* 4 / 7-2 weld_tick 2 / 7-3 tcp_poll 2 / 7-4 disp_compute_output 2 / 7-5 std_setup 2 중 살아남은 것 나열>
- DIFF → 되돌림·spec §6 보류: <나열, 없으면 '없음'>
- 각 건 파일 단위 독립 판정 (H7 우회 없음). host 17 PASS · our-code 경고 0"
```

(커밋 메시지의 `N`·나열은 실측으로 채운다. 전부 `DIFF` 면 커밋하지 않고 Task 8 로 간다.)

---

### Task 8: 마무리 — 최종 게이트(spec §5.4) · changelog · spec §6 · `--no-ff` 머지

**Files:**
- Modify: `docs/changelog.md` — `## [Unreleased]` 바로 아래에 항목 1개
- Modify: `docs/superpowers/specs/2026-09-08-refactor-byte-identical-design.md` — §6 표 갱신(기존 5행 "이번 결과" 열 + 추가 행)

- [ ] **Step 1: 최종 게이트 (spec §5.4)**

```sh
git status --short fw/ docs/     # 기대: 출력 없음 (Task 7 까지 전부 커밋)
rm -rf fw/build fw/build-remote
fw/tools/bin-same.sh             # 기대: SAME  fd66f6e7bb9d6ff03f7ea1831797167813b3db4613de66e6c84841b1860e278f c5223adac03c85861e242e592125bf163114c847f383d6304f5d2e0771f7373f
shasum -a 256 fw/build/gds_us_ctrl.bin fw/build-remote/gds_us_ctrl.bin
# 기대:
# fd66f6e7bb9d6ff03f7ea1831797167813b3db4613de66e6c84841b1860e278f  fw/build/gds_us_ctrl.bin
# c5223adac03c85861e242e592125bf163114c847f383d6304f5d2e0771f7373f  fw/build-remote/gds_us_ctrl.bin
git diff b61ef0f..HEAD --stat -- fw/vendor ref fw/test fw/include   # 기대: 출력 없음
git diff b61ef0f..HEAD --stat -- fw/ | tail -1                       # 기대: " 28 files changed, …" (도구 3 + Task 1 18 + usart6_mb.c + FSM 3 + app_lcd_render.c/app_weld.c/app_modbus_tcp.c 3; 7-1/7-2/7-3 이 DIFF 로 빠지면 건당 −1, 7-4/7-5 는 Task 1/5 파일이라 무영향)
./fw.sh test 2>&1 | grep -ciE 'all (checks |tests )?passed'         # 기대: 17
wc -l fw/src/app_modbus.c                                            # 기대: 792  (7-1~7-5 는 이 파일을 건드리지 않는다)
```

- [ ] **Step 2: 50줄 초과 함수 재집계**

```sh
python3 fw/tools/funclen.py fw/src/*.c fw/drivers/*.c | awk '$2>50' | sort -k2 -n -r
```

기대 — 슬라이스 7 이 전부 `SAME` 인 경우 **11개** (spec §5.4 "목표 ≤6" 은 D9 대로 미달 — 사유는 각 행):

| 함수 | 줄수 (코드) | 왜 남는가 | 후속(범위 밖) |
|---|---|---|---|
| `app_modbus_apply_writes` | 255 (157) | 36분기 else-if 뼈대 + 13 클램프 분기 (spec §6) | HW 벤치 트랙: 체인→테이블 |
| `app_lcd_input_dispatch` | 182 (143) | 35 `case…break` 뼈대 (spec §6) | HW 벤치 트랙: 핸들러 테이블 |
| `app_weld_tick` | 107 (71) | `weld_in_t` 지정 초기화 22줄 + 트리거 스캔·게이팅·클램프 (spec §4: 초기화자 헬퍼화는 코드 변화 가능) | 글루 — host 트랙 없음 |
| `app_lcd_send_model_str` | 93 (83) | spec §4 미분할 결정(컴파일되는 브랜드 1블록 ~25줄) | — |
| `app_lcd_change_page` | 78 (57) | 8분기 else-if 뼈대 + STD2D/2T/3 분기 + set_page 꼬리 (spec §6) | HW 벤치 트랙: STDC/MHC·STDE/MHE DRY |
| `app_modbus_tcp_poll` | 72 (49) | 워커 루프가 `off`/`tx_len` 출력을 요구 — out-포인터 ✗ (spec §4) | host `test_app_modbus_tcp_frame` 는 프레임 계층만 |
| `weld_fsm_step` | 72 (53) | abort 판정·엣지 래치·타이머·READY/HOLD/default 뼈대 (spec §6) | host `test_app_weld_fsm` 21함수 게이트 트랙 |
| `app_config_load` | 64 (56) | `fail++` 누산 구조 — 미착수 (spec §6) | host `test_app_config` 게이트 트랙 |
| `weld_step_weld` | 58 (49) | WELD case 본체 55줄 — 더 쪼개지 않음 (spec §6·§8) | 동상 |
| `app_lcd_disp_step` | 57 (55) | 코드 55; 엣지 블록은 함수-static(`prev_run_on`·`prev_remote_on`) — spec §2.2 제외; switch 10 case 재그룹 ✗ | HW 벤치 트랙(ICON/REMOTE 육안) |
| `render_run_std` | 53 (48) | RUN_STD 본체 49줄 + `n` 선언 | — |

슬라이스 7 에서 `DIFF` 로 되돌린 건이 있으면 그 원함수가 원래 길이로 이 표에 들어온다: `app_lcd_change_page` 203 (그리고 `render_run_std` 행 삭제) / `app_weld_tick` 131 / `app_modbus_tcp_poll` 111 / `disp_compute_output` 76(주석만 이동했으면 61) / `handle_std_setup_param` 58. 실측 표를 아래 changelog 항목과 spec §6 에 옮겨 적는다.

- [ ] **Step 3: `docs/changelog.md` — `## [Unreleased]` 바로 아래(기존 `### 2026-09-06 — feat: 원격 hold-to-run 워치독` 항목 위)에 삽입** (`<…>` 는 실측으로 채운다)

```markdown
### 2026-09-08 — refactor: 바이트 동일 리팩토링 — 50줄 초과 함수 38→<N> · app_modbus.c 815→792 · .bin 무변경

- **무엇**: 함수 안 장문 주석을 함수 헤더로 옮기고(22함수, 코드 줄 무변경), 직선 본체를 `static inline __attribute__((always_inline)) void` 헬퍼로 뽑았다(<M>개: `mirror_live` 3 · `reg_publish_measure` 3 · `usart6_mb_open` 2 · `apply_writes` 3 · `lcd_input_dispatch` 4 · FSM step 4+4+2 · 조건부 <K>). else-if 체인·switch case·파일 분할은 하지 않았다(감사 실측 ✗). 외부 계약(Modbus·LCD)·거동 변화 0.
- **검증**: 슬라이스 커밋마다 `fw/tools/bin-same.sh` — STD `fd66f6e7…e278f`(66,696 B) / REMOTE `c5223ada…7373f`(67,008 B) 가 시작 커밋 `b61ef0f` 클린 빌드와 **바이트 동일**(gcc 15.2.1). host 17스위트 PASS · our-code 경고 0 · 슬라이스 1 은 주석·빈 줄 제거 스트림 diff 로 코드 줄 무변경 입증. **HW 벤치 없음** — 바이너리가 태그 `hw-revA_fw-stage-hold-wdt` 빌드와 동일하므로 그 벤치 결과를 승계.
- **app_modbus.c**: 이력 서술(2026-09-04/05 fix 경위 등, changelog 에 있는 것만) 압축 + 리플로로 815→753, 헬퍼 6개 추가 후 792.
- **예외(≤50 불가, spec §6)**: `app_modbus_apply_writes` 255 · `app_lcd_input_dispatch` 182 · `app_weld_tick` 107 · `app_lcd_send_model_str` 93(미분할 결정) · `app_lcd_change_page` 78 · `app_modbus_tcp_poll` 72 · `weld_fsm_step` 72 · `app_config_load` 64 · `weld_step_weld` 58 · `app_lcd_disp_step` 57 · `render_run_std` 53 <— 실측으로 교정>. 진짜 분할은 HW 벤치/host 게이트 트랙(spec §6 후속 열).
- 슬라이스 7 조건부: <7-1~7-5 SAME/DIFF 결과 한 줄>.
- 도구: `fw/tools/bin-same.sh`(두 모델 .bin sha256 대조) · `fw/tools/funclen.py`(중괄호 균형 함수 길이). spec = `docs/superpowers/specs/2026-09-08-refactor-byte-identical-design.md`, plan = `docs/superpowers/plans/2026-09-08-refactor-byte-identical.md`.
```

- [ ] **Step 4: spec §6 표 갱신** (`docs/superpowers/specs/2026-09-08-refactor-byte-identical-design.md` §6). 기존 5행의 "이번 결과" 열을 실측으로 바꾸고 행을 추가한다. 슬라이스 7 이 전부 `SAME` 인 경우의 표:

```markdown
| 함수 | 이유 | 이번 결과 | 후속(범위 밖) |
|---|---|---|---|
| `app_modbus_apply_writes` | 36분기 else-if 체인 뼈대만 코드 ≥110줄. 체인 분할은 감사 b-1 계열 3변형 전부 ✗ | 본체 3추출(gate_reject_body/start_cmd_body/cfg_ctrl_commit_body) → **255줄(코드 157)** | HW 벤치 트랙: 체인 → 테이블. 회귀 = `plans/2026-09-05-bench-results.md` FC06 클램프 27항목 재사용 |
| `app_lcd_input_dispatch` | 35 `case…break` 뼈대 ~105줄. 재그룹은 감사 b-3 ✗ | 본체 4추출 → **182줄(코드 143)** | HW 벤치 트랙: 핸들러 테이블. LCD 터치 E2E |
| `app_lcd_change_page` | 8분기 + 공통 꼬리, 로컬 배열 포인터 전달이 H4 경계 | Task 7-1 **SAME** → **78줄(코드 57)**, `render_run_std` 53 잔존 | HW 벤치 트랙: STDC/MHC·STDE/MHE 중복 블록 DRY |
| `weld_fsm_step` | WELD case 자체가 ~55줄 | case 본체 4추출 → **72줄(코드 53)**(abort/엣지/타이머/READY·HOLD·default 뼈대) + `weld_step_weld` **58줄** 잔존 | host `test_app_weld_fsm.c` 21함수가 있어 **host 게이트 트랙**으로 구조 변경 가능 |
| `app_config_load` | `fail++` 누산 구조 | 미착수 (64줄) | host `test_app_config.c` 게이트 트랙 |
| `app_weld_tick` | `weld_in_t` 지정 초기화 22줄 + 트리거 스캔 — 헬퍼화 시 코드 변화 가능(§4) | Task 7-2 **SAME**(weld_sensor_mirror/weld_dispatch_out) → **107줄(코드 71)** | 글루 — host 트랙 없음, HW 벤치 |
| `app_modbus_tcp_poll` | 워커 루프가 `off`/`tx_len` 출력 요구 = out-포인터 ✗ | Task 7-3 **SAME**(tcp_recv_accumulate/tcp_flush) → **72줄(코드 49)** | HW 벤치(TCP) |
| `app_lcd_disp_step` | 코드 55; 엣지 블록은 함수-static(§2.2 제외), switch 10 case 재그룹 ✗ | 슬라이스 1 주석 이동만 → **57줄** | HW 벤치 트랙(ICON_RUN/REMOTE 육안) |
| `app_lcd_send_model_str` | §4 미분할 결정(컴파일되는 브랜드 1블록 ~25줄) | 무변경 93줄 | — |
```

`DIFF` 로 되돌린 건은 해당 행 "이번 결과" 를 `Task 7-x .bin ≠ (STD <hash8> / REMOTE <hash8>) → 보류, <원래 줄수> 잔존` 으로 쓴다(Task 7 각 건의 DIFF 절 문구).

- [ ] **Step 5: 문서 커밋**

```sh
git add docs/changelog.md docs/superpowers/specs/2026-09-08-refactor-byte-identical-design.md
git commit -m "docs: 바이트 동일 리팩토링 결과 — changelog [Unreleased] + spec §6 예외 표 실측 기입"
```

- [ ] **Step 6: `--no-ff` 머지 (spec §7 — HW 태그 없음)**

```sh
git checkout main
git merge --no-ff refactor/byte-identical -m "merge: refactor/byte-identical — 바이트 동일 리팩토링 (50줄 초과 38→<N>, app_modbus.c 815→792; STD fd66…e278f / REMOTE c522…7373f 무변경)

전 슬라이스 fw/tools/bin-same.sh SAME · 클린 빌드 sha256 == b61ef0f · host 17 PASS · our-code 경고 0.
HW 벤치 없음 — 바이너리가 태그 hw-revA_fw-stage-hold-wdt 빌드와 동일(그 벤치 결과 승계)."
rm -rf fw/build fw/build-remote && fw/tools/bin-same.sh   # main 에서 한 번 더: 기대 SAME …
git log --oneline main -12
```

푸시는 사용자 지시가 있을 때만(`git push origin main refactor/byte-identical`).

---

## 자체 점검 — spec 항목 ↔ Task 대응

| spec | 내용 | 대응 |
|---|---|---|
| §3 H1 | `static inline __attribute__((always_inline)) void` | Task 2~7 모든 헬퍼 시그니처 (Interfaces 열) |
| §3 H2 | 값 반환 없음, 결과로 분기 없음 | 모든 헬퍼 `void`; 호출자는 호출문만 |
| §3 H3 | 판정 구조 호출자 유지 | Task 4 `if (s_ren…)`/`return` · Task 3 `if (sr != …)` · Task 5/6 `case`/`break` · Task 7 `if` 외곽 전부 호출자 |
| §3 H4 | 기존 포인터·스칼라만 | 각 헬퍼 "H4 ✓" 근거; 경계 사례는 Task 7 에만(spec 지정) |
| §3 H5 | 같은 TU, 호출 함수 바로 위, 파일-static 직접 | 모든 헬퍼는 대상 함수 헤더 주석 바로 위에 삽입 |
| §3 H6 | 헬퍼 안 `return` 없음 | Task 4 gate `return` 호출자 · Task 6 abort `return` 호출자 · Task 7-3 `if (s_acc_len==0) return` 호출자 |
| §3 H7 | 슬라이스마다 두 모델 sha256; ≠ 이면 되돌리고 보류 | 각 Task Step 3/4 (bin-same.sh · git checkout · §6 기록 · 우회 금지) |
| §3 C1~C4 | 주석 이동 규칙 | Task 1 Step 2-A(병합 규칙)·2-B(표)·2-C(K/R)·Step 3(C3 스트림 diff)·Step 3 게이트(C4) |
| §4 슬라이스 1 | 21함수 + app_modbus.c 800↓ | Task 1 (D6~D8 로 산식 교정; disp_step 57 은 §6) |
| §4 슬라이스 2 | mirror_live 3분할 | Task 2 (연속 범위로 재절단, 이름·인자 spec 동일) |
| §4 슬라이스 3 | publish 3 + usart6 2 | Task 3 (D3: `live` 인자 추가) |
| §4 슬라이스 4 | apply_writes 3 (gate 실증 ○) | Task 4 (D2: `cfg_ctrl_commit_body(cfg, d)`); 13 클램프 무변경 |
| §4 슬라이스 5 | dispatch 4 (sys_pic 실증 ○) | Task 5 (D1: `mo_time_clamp_echo(cfg)`) |
| §4 슬라이스 6 | FSM 3개 | Task 6 (D4 `weld_step_cyl2(out)`, D5 osc 4개) |
| §4 슬라이스 7 | 조건부 5건 | Task 7-1~7-5, 파일 단위 되돌림 |
| §4 미분할 결정 | `app_lcd_send_model_str` | 건드리지 않음 — Task 8 표 |
| §5.1 기준 해시 | 두 값 + 툴체인 | Global Constraints · Task 0 Step 5 |
| §5.2 스크립트 | bin-same.sh 원문, `.gitignore` | Task 0 Step 2/4 (`fw/.gitignore` — 그 파일이 `build*/` 를 담는다) |
| §5.3 게이트 5항 | SAME / test 17 / 경고 0 / C3 / funclen | 각 Task Step 3·5; C3 는 Task 1 Step 3 |
| §5.4 최종 | 클린 빌드 == 기준 / vendor·ref·test 0 / 재집계 | Task 8 Step 1~2 (목표 ≤6 → 실측 11, D9) |
| §6 예외 표 | 정본 갱신 | Task 8 Step 4 (기존 5행 + 추가 4행) |
| §7 브랜치·커밋·순서·머지·문서 | | Task 0 Step 1(브랜치) · 각 Task Step 6(메시지 형식) · 1→7 순서 · Task 8 Step 6(`--no-ff`, 태그 없음) · Step 3~5(changelog·§6) |
| §8 리스크 | 리터럴 풀·레지스터 / 문장 손실 / 이름 / weld_step_weld>50 / 툴체인 | H7 절차 · C3 스트림 diff + K/R 소재 명시 · 헬퍼 위 1줄 주석 · Task 6 §6 기록 · Task 0 Step 5 버전 확인 |

헬퍼 이름·시그니처 일관성(Interfaces ↔ 코드 ↔ 커밋 메시지): `mirror_cfg_fields(cfg)` `mirror_disp_status(cfg, m, running)` `mirror_stage_and_gate(cfg)` / `publish_sr_edge(sr)` `publish_amp_power(live)` `publish_copy_out(now, active, live, freq_cal_val)` / `mb_uart_reinit(speed_idx, parity_idx)` `mb_dma_init()` / `gate_reject_body()` `start_cmd_body(cfg, sv, now)` `cfg_ctrl_commit_body(cfg, d)` / `handle_sys_pic_now(state, cfg, data16)` `handle_setup_param_enter(state)` `mo_time_clamp_echo(cfg)` `handle_run_mode(state, cfg, data16)` / `weld_abort_body(out)` `weld_step_cyl1(in)` `weld_step_weld(in, out)` `weld_step_cyl2(out)` / `osc_step_wait_h(in)` `osc_step_wait_l(in)` `osc_step_reset(out)` `osc_step_seek(out)` / `sr_step_reset(out)` `sr_step_seek(out)` / `render_run_std(cfg, buf)` `render_setup_main(cfg, state)` `render_comm_page(cfg, state, addr_str, ipbuf)` `render_comm_tail(page, state)` / `weld_sensor_mirror(cfg, sens_dn)` `weld_dispatch_out(cfg, &out)` / `tcp_recv_accumulate()` `tcp_flush(off, tx_len)` / `fill_upper_band(curr_amp, st)` `fill_lower_band(curr_amp, st)` / `goto_setup1(state, cfg)` `goto_setup2(state, cfg)` — 총 **35개**(Task 2 3 · Task 3 5 · Task 4 3 · Task 5 4 · Task 6 10 · Task 7 12).
