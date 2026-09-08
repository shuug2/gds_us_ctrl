# 바이트 동일 리팩토링 — 설계 spec (2026-09-08)

> **문서 요약**: 50줄 초과 함수(38개)와 800줄 초과 파일(`app_modbus.c` 1개)을 **두 모델(STD·REMOTE) `.bin` 이
> 시작 커밋과 바이트 동일**하다는 조건 아래 정리한다. HW 벤치 없이 머지하기 위한 조건이다. 사전 감사
> (`docs/superpowers/research/2026-09-08-refactor-audit.md`)의 실측에 따라 허용되는 수단은 두 가지뿐이다 —
> ① 함수 안 장문 주석을 **함수 선언 바로 위로** 옮기기(바이너리 무영향), ② 판정 구조는 호출자에 남기고 직선
> 본체만 **`static inline __attribute__((always_inline)) void`** 헬퍼로 뽑기. 빌드가 `-Og` 라 plain `static`
> 헬퍼는 실제 call 로 남아 바이너리가 바뀌므로 else-if 체인 분할·switch 재그룹·파일 분할은 **하지 않는다**.
> 결과: 38개 중 ~34개가 50줄 아래로 내려가고, `apply_writes`·`lcd_input_dispatch`·`change_page`·`weld_fsm_step`
> 4개는 구조적으로 불가해 **예외로 명시**한다. 슬라이스 7개, 슬라이스마다 커밋 1개, 커밋마다 `.bin` sha256 대조.

작성 2026-09-08 · 대상 main `b61ef0f` · 툴체인 `arm-none-eabi-gcc 15.2.1 20251203` (Arm GNU Toolchain 15.2.Rel1)
근거 = 감사 보고서(위 경로, 이하 "감사"). 이 문서가 **구현 정본**이다. 구현 plan 은 별도(`plans/2026-09-08-refactor-byte-identical.md`).

---

## 0. 확정 결정 (2026-09-08 사용자)

| # | 결정 | 귀결 |
|---|---|---|
| 1 | 목표 = **긴 함수·큰 파일 분할, 거동 불변** (주석 정리·구조 재편·legacy 잔재 제거는 선택하지 않음) | 의미 변경 0. 외부 계약(Modbus·LCD) 무변경 |
| 2 | 합격 기준 = **바이너리 바이트 동일** (host 테스트 + HW 벤치 기준은 선택하지 않음) | HW 세션 불필요. 대신 수단이 §3 으로 제한된다 |
| 3 | 접근 = **바이트 동일 트랙만**. 구조 함수 4개의 HW 벤치 트랙은 **범위 밖** | 4개는 §6 예외 목록 |
| 4 | 함수 안 장문 주석의 행선지 = **같은 파일, 함수 선언 바로 위** (docs 이동은 선택하지 않음) | 근거가 코드 옆에 남는다. `app_modbus.c` 는 이력 서술만 16줄 이상 압축 |

---

## 1. 배경 — 왜 수단이 이렇게 좁은가

감사의 실측 3건이 설계를 결정했다.

1. **빌드는 `-Og`** (`fw/CMakeLists.txt:23`, Debug 강제 `:14-16`). `-Og` 는 `-finline-functions-called-once` 가 **비활성**이라
   (감사 A-1, `gcc -Q --help=optimizers` 실측) 한 번만 호출되는 `static` 헬퍼도 실제 함수 + `bl` 로 남는다.
2. **spike 결과** (감사 B, 두 모델 `.bin` sha256 대조):
   - else-if 체인 13분기 → `static bool` 헬퍼: ✗ (+144 B). `always_inline` 도 ✗ (+40 B). `void` + `bool *save` 도 ✗ (+56 B, 스택 스필).
   - 게이트 블록 → `static bool gate_reject()`: ✗ (+16 B). **판정을 호출자에 두고 본체만 `always_inline void`: ○.**
   - switch case 5개 재그룹 + 내부 switch: ✗. **case 라벨 유지 + 본체만 `always_inline void`: ○.**
   - 파일 분할: `-ffunction-sections` + `*(.text*)` 입력 순서 배치라 함수 이동 = 이후 주소 전부 이동 → ✗ (논증, 감사 B-4).
3. **재현성 전제 성립** (감사 A-2/A-3): 클린 재빌드 간 `.bin` 동일, `__LINE__`/`__FILE__`/`__DATE__`/`__TIME__`/`assert(` 0건,
   빈 줄 삽입 시 `.bin` 불변·`.elf` 만 변화 → 비교 대상은 **`.bin`**.

인벤토리(감사 C): 50줄 초과 함수 **38개**(전체 301개). 그중 **21개는 주석 제외 코드 ≤50줄**. 800줄 초과는 `app_modbus.c`(815 =
코드 469 / 주석 311 / 빈 36) 하나. host 스위트가 직접 호출하는 함수 7개.

---

## 2. 범위

### 2.1 In

- §4 슬라이스 1~7 의 함수·파일. 전부 `fw/src`, `fw/drivers`.
- 신규 검증 스크립트 `fw/tools/bin-same.sh` (§5).
- 문서: 이 spec, plan, `docs/changelog.md` 항목.

### 2.2 Out (명시적 제외)

| 항목 | 이유 |
|---|---|
| else-if 체인·switch 구조 변경, 테이블 기반 디스패치 | 감사 B ✗. HW 벤치 트랙(범위 밖) |
| 파일 분할(함수를 새 `.c` 로) | 감사 B-4 ✗. 필요도 없다(`app_modbus.c` 는 주석으로 800↓, 감사 C-3) |
| `bool`/값 반환 헬퍼, out-포인터 인자 | 감사 B ✗ |
| 함수-static 로컬(`app_loop_iter` i2c 관측, `disp_step` prev_*)을 헬퍼로 이동 | `.bss` 배치 변화 가능(추측) — 시도하지 않음 |
| `app_config_load` 분할 | `&fail`/반환값 구조라 ✗ 확률 높음. host `test_app_config.c` 게이트 트랙으로 분리(범위 밖) |
| `app_lcd_change_page` 중복 블록 DRY(비-인라인 공통화) | 코드 크기 변화 = ✗ |
| `fw/vendor/`, `ref/`, 최적화 레벨 변경 | 읽기 전용 / 바이너리 전체 변화 |
| 주석 **삭제** | 결정 4 — 이동·압축만. 압축 대상은 changelog·spec 에 이미 있는 이력 서술(날짜·커밋 해시 나열)에 한정 |
| `CLAUDE.md`·`HANDOFF.md`·`NEXT_STEPS.md` 갱신 | 요청 범위 밖. 필요하면 사용자가 별건으로 |

---

## 3. 불변식 — 헬퍼 추출 규칙 (감사 B-3, 실측 ○ 조건만)

구현자는 아래를 **전부** 지킨다. 하나라도 어기면 `.bin` 이 달라진 실측 사례가 있다.

| # | 규칙 | 위반 시 실측 |
|---|---|---|
| H1 | 헬퍼는 `static inline __attribute__((always_inline)) void` | plain `static` → call 잔존 (+16~144 B) |
| H2 | 헬퍼는 값을 반환하지 않고, 호출자는 헬퍼 결과로 분기하지 않는다 | `bool` 반환 → `movs r3,#1` 물질화 (+8~40 B) |
| H3 | 판정 구조(`if` 조건·else-if 체인·`case` 라벨·`break`·`return`)는 **호출자에 남긴다**. 헬퍼에는 부수효과만 있는 직선 블록 | case 재그룹 → 이중 디스패치 ✗ |
| H4 | 인자는 **이미 존재하는 포인터·스칼라 값만**. 호출자 로컬 변수의 주소를 새로 잡아 넘기지 않는다 | `&save` → 스택 스필 (+56 B) |
| H5 | 헬퍼는 같은 번역 단위에, 호출 함수 바로 위에 둔다. 파일-static 변수(`g_mb`, `s_stg`, `s_ren`, `g_reg`, `g_measure` 등)는 헬퍼가 직접 접근 | — |
| H6 | 헬퍼 안 `return` 은 **미실측** → 쓰지 않는다. early-return 이 있는 블록은 추출 대상에서 제외 | (추측) |
| H7 | 슬라이스마다 두 모델 `.bin` sha256 대조가 **게이트**다. 규칙을 지켜도 리터럴 풀·레지스터 할당이 어긋날 수 있다 → 다르면 **되돌리고 보류**, 우회 시도 금지 | 감사 B-3 5 |

> **H3 각주(구현 판정 R9)**: H3 의 "판정 구조" 는 **호출자 측 라우팅**(if/else-if 분기 진입·`case`·`break`·`return`)을 뜻한다.
> 옮겨진 블록 **안에 원래 있던** 내부 if 체인(예: `gate_reject_body` 의 blocked 선택, `start_cmd_body` 의 `sv` 체인)은
> 함께 옮겨도 된다 — 감사 b-2 실증 ○ 패턴이며 슬라이스 4 에서 `.bin` SAME 으로 재확인.

주석 이동 규칙(슬라이스 1):

| # | 규칙 |
|---|---|
| C1 | 함수 본문 안의 **5줄 이상** 주석 블록을 함수 선언 바로 위(기존 함수 헤더 주석과 합쳐 한 블록)로 옮긴다. 옮긴 자리에는 필요하면 한 줄 포인터(`/* 게이트 닫힘 분기 — 함수 헤더 §2 */`)만 남긴다. 그래도 50줄을 넘으면 2~4줄 블록 → 1줄 독립 주석 순으로 더 옮기고, 옮긴 블록 바로 위의 빈 줄도 함께 제거한다(plan D6 — 감사 산식이 빈 줄과 짧은 블록을 빼먹었다). 코드 줄 사이의 구분용 빈 줄 삭제도 허용(`parser_step`·`energy2str`·`app_modbus_tick`) |
| C2 | 문장은 **삭제하지 않는다.** 예외 = `app_modbus.c` 의 이력 서술(날짜·커밋 해시·"구 주석은 ~라고 했는데" 류)로, changelog·spec 에 이미 있는 내용만 16줄 이상 압축한다. 압축한 문장은 커밋 메시지에 어느 문서에 있는지 적는다 |
| C3 | 코드 줄은 **한 글자도** 바꾸지 않는다(들여쓰기 포함). 판정은 **주석·빈 줄 제거 스트림 diff** — 파일마다 `arm-none-eabi-gcc -fpreprocessed -dD -E -P -x c -` 로 주석을 벗기고 빈 줄을 지운 전후 스트림이 동일해야 한다(plan Task 1 Step 3 루프, 기대 `SAME_CODE` × 파일 수). `git diff -w --ignore-blank-lines` 는 큰 주석 블록이 시그니처 근처로 이동하면 hunk 오정렬로 동일 코드 줄을 −/+ 로 보여 **판정에 쓰지 않는다**(참고용) |
| C4 | `.bin` 대조는 그대로 한다(라인 이동은 무영향이 실증됐지만 게이트는 유지) |

---

## 4. 슬라이스

각 슬라이스 = 커밋 1개. 커밋 전 `fw/tools/bin-same.sh` → `SAME`, `./fw.sh test` PASS, STD·REMOTE 빌드 경고 0. 좌표는 main `b61ef0f` 기준(감사 C-1).

| # | 슬라이스 | 대상 (`파일:라인`) | 헬퍼 (이름 — 담는 블록) | 예상 결과 |
|---|---|---|---|---|
| **1** | 주석 재배치 | `app_modbus.c` 전체(815→≤799) + 코드 ≤50줄인 **21개 함수**: `app_reg_command`(:168-272) `app_loop_iter`(:99-186) `app_modbus_tick`(:742-815) `remote_en_fsm_step` `app_init` `app_input_tick` `parser_step` `app_reg_tick` `app_overload_tick` `handle_key_multi` `usart1_init` `main` `apply_config` `data_save_commit` `commit_comm_mode_and_ether` `cfg_stage_commit` `spi1_init` `i2c1_bus_unstick` `app_lcd_init_mode` `app_lcd_disp_step`(코드 55 — 주석만으로는 근접, 슬라이스 7 후보) `energy2str`/`mb_core_decode`/`process_ip_char` 는 이미 코드 ≤50·주석 적음 → 확인만 | 0 | plan 실측(Task 1): 23함수 검토 · 22함수 편집 · **21함수 ≤50**(`app_lcd_disp_step` 은 코드 55 라 57 잔존 → §6). `app_modbus.c` 815→**753**(이력 압축 39줄 + 리플로 19줄 — 슬라이스 2·4 헬퍼 +39줄을 미리 상쇄해 최종 792). 실측: 커밋 `4e15dfe` 시점 **752**, fix 라운드 `7430310` 의 K12 안전 근거 복원 +1 = **753** — 커밋 `4e15dfe` 메시지의 "815→753" 은 brief 산술 오차(그 커밋 실제값 752)이므로 이 행이 정정본이다. 바이너리 무영향 **보장** |
| **2** | `mirror_live` 3분할 | `app_modbus.c:180-287` (108, 코드 62) | `mirror_cfg_fields(cfg)` — WORK_CNT~TIMEOVER 15개 대입(연속 블록) / `mirror_disp_status(cfg, m, running)` — DISP_* 4개 + B-5 cfg 6개 + CAL 2개 + `mb_status_in_t` 합성 + STATUS / `mirror_stage_and_gate(cfg)` — COMM_MODE·CFG_STAT·CAP·FEAT·HORN·staged 루프·`#if MODEL_REMOTE` 블록. (plan: 초안의 "WORK_CNT~EN_SAFTY·CAL" 은 DISP_* 4줄이 사이에 끼어 연속 블록이 아니라 문장 순서를 지키기 위해 연속 범위로 재절단) | 본체 13줄(plan 실측). 가장 깨끗한 실증 패턴(정적 변수만 접근, 기존 포인터) |
| **3** | 순차 함수 2개 | `app_reg.c:324-404` `reg_publish_measure`(81, 코드 54) / `usart6_mb.c:46-113` `usart6_mb_open`(68, 코드 53) | `publish_sr_edge(sr)` / `publish_amp_power(live)` / `publish_copy_out(now, active, live, freq_cal_val)` — 3개(plan D3: 본체 마지막 줄이 `live` 를 써서 값 인자 추가) · `mb_uart_reinit(speed_idx, parity_idx)` / `mb_dma_init()` — 2개 | 두 함수 ≤50 |
| **4** | `apply_writes` 본체 추출 | `app_modbus.c:290-647` (358, 코드 191) | `gate_reject_body()` — `#ifndef REMOTE_EN_GATE_BYPASS` 블록의 소거·STOP 통과·로그·CFG_CTRL 소거(`return` 은 호출자) **실증 ○** / `start_cmd_body(cfg, sv, now)` — START 값 if-체인 본체 / `cfg_ctrl_commit_body(cfg, d)` — 커밋 통과분 반영(`stg_apply_to_cfg` + ether 훅). `if (cfg_stage_commit(...) != 0u)` 판정과 `save = true` 는 호출자에 남긴다(H3/H4). plan D2: 본체가 `link` 를 안 쓰고 dirty 스냅샷 `d` 를 써서 `(cfg, d)` | 코드 191 → **157**(plan 실측, 358→255줄). **≤50 불가 → §6 예외**. 13개 클램프 분기는 **한 글자도 건드리지 않는다** |
| **5** | `lcd_input_dispatch` case 본체 | `app_lcd_input.c:413-637` (225, 코드 170) | `handle_sys_pic_now(state, cfg, data16)` **실증 ○** / `handle_setup_param_enter(state)` — SETUP_PARAM·MOOHAN 공통 본체(두 case 각각에서 호출, 인라인이라 코드 중복은 유지됨) / `mo_time_clamp_echo(cfg)` — LV_MO_TIME1/2 의 **공통 꼬리 4줄**만(plan D1: 두 case 의 첫 줄 대입이 달라 한 헬퍼에 담으면 `vp` 분기를 새로 만들어야 함 → 대입은 case 에 남긴다) / `handle_run_mode(state, cfg, data16)` | 코드 170 → **143**(plan 실측, 225→182줄). **≤50 불가 → §6 예외**. 35개 `case … break` 뼈대 불변 |
| **6** | FSM step case 본체 | `app_weld_fsm.c:121-290` `weld_fsm_step`(170, 코드 141) / `app_osc_init_fsm.c:24-100` `osc_init_fsm_step`(77, 코드 68) / `app_seek_reset_fsm.c:22-81` `seek_reset_fsm_step`(60, 코드 50) | `weld_abort_body(out)` `weld_step_cyl1(in)` `weld_step_weld(in, out)` `weld_step_cyl2(out)`(plan D4: CYL2 본체가 `in` 을 안 읽음) · `osc_step_wait_h(in)` `osc_step_wait_l(in)` `osc_step_reset(out)` `osc_step_seek(out)`(plan D5: RESET/SEEK 펄스 본체의 상수가 달라 한 헬퍼로 못 담음, wait_h 만으론 63줄) · `sr_step_reset(out)` `sr_step_seek(out)` — 전부 case 라벨 유지, `in`/`out` 기존 포인터 | host 스위트 3개(`test_app_weld_fsm` 21함수 등)가 추가 안전망. plan 실측: `weld_fsm_step` 72(코드 53)·`weld_step_weld` 58 잔존 → §6 기록 |
| **7** | 조건부 — 시도 후 ≠ 이면 되돌리고 보류 | `app_lcd_render.c:40-242` `change_page`(로컬 배열 `buf/addr_str/ipbuf` 포인터 전달) / `app_weld.c:97-227` `app_weld_tick`(로컬 `out` 구조체 주소) / `app_modbus_tcp.c:123-233` `tcp_poll`(`off/tx_len` 출력 필요 — recv 블록만) / `app_lcd_disp.c:51-126` `disp_compute_output`(`fill_upper_band`/`fill_lower_band`, 값 인자) / `app_lcd_input.c:332-389` `handle_std_setup_param`(`goto_setup1/2`) | 각 1~3 | H4 경계 사례. 첫 시도 `.bin` ≠ 이면 **되돌리고 spec §6 에 "보류" 기록**. 우회 재시도 금지 |

미분할 결정: `app_lcd_send_model_str`(93줄이나 `#if` 브랜드 4블록 중 컴파일되는 것은 1블록 ~25줄) — 그대로 둔다.

---

## 5. 검증

### 5.1 기준 해시 (시작 커밋 `b61ef0f`, 클린 빌드, 감사 A-2)

| 산출물 | sha256 | 크기 |
|---|---|---|
| STD `fw/build/gds_us_ctrl.bin` | `fd66f6e7bb9d6ff03f7ea1831797167813b3db4613de66e6c84841b1860e278f` | 66,696 B |
| REMOTE `fw/build-remote/gds_us_ctrl.bin` | `c5223adac03c85861e242e592125bf163114c847f383d6304f5d2e0771f7373f` | 67,008 B |

같은 커밋·같은 툴체인(15.2.1)이면 이 값이 나와야 한다. 다른 값이면 리팩토링을 시작하기 전에 원인을 찾는다(툴체인 버전 차이가 첫 후보).

### 5.2 스크립트 `fw/tools/bin-same.sh` (신규)

```sh
#!/bin/sh
# 두 모델을 빌드해 .bin sha256 을 기준과 비교. 사용: bin-same.sh baseline | bin-same.sh
set -eu; cd "$(dirname "$0")/../.."
./fw.sh >/dev/null && MODEL=remote ./fw.sh >/dev/null
cur=$(shasum -a 256 fw/build/gds_us_ctrl.bin fw/build-remote/gds_us_ctrl.bin | cut -d' ' -f1 | paste -sd' ' -)
base=fw/.bin-baseline
[ "${1:-}" = baseline ] && { printf '%s\n' "$cur" >"$base"; echo "baseline: $cur"; exit 0; }
[ "$cur" = "$(cat "$base")" ] && echo "SAME  $cur" || { echo "DIFF  base=$(cat "$base")  cur=$cur"; exit 1; }
```

- 기준 파일은 `fw/.bin-baseline` (`.gitignore` 에 추가 — `fw/build*/` 안에 두면 클린 빌드에 지워진다). 값은 §5.1 과 이 spec 에도 적혀 있다.
- 비교는 **`.bin` 만**. `.elf` 는 DWARF 라인 테이블·`DW_AT_comp_dir` 때문에 라인·경로에 민감하다(감사 A-2 실증).
- `DIFF` 시 진단 순서: `arm-none-eabi-nm -S --size-sort` 전후 비교로 크기가 바뀐 심볼 → 그 함수만 `objdump -d` diff. 원인이 H1~H6 위반이면 고치고, 아니면 **되돌린다**(H7).

### 5.3 슬라이스 게이트 (커밋 조건)

1. `fw/tools/bin-same.sh` → `SAME`
2. `./fw.sh test` → host 17스위트 PASS (테스트 코드 무변경)
3. STD·REMOTE 빌드 **our-code 경고 0** (`-Wall -Wextra -Wundef -Wshadow`; `grep -E 'warning:|error:' | grep -v '/vendor/'` 출력 없음). vendor 헤더에서 나오는 경고는 리팩토링 전부터 있는 것이라 제외
4. 슬라이스 1 만: C3 의 스트림 diff 루프에서 `CODE CHANGED` 0
5. 함수 길이 재측정(감사의 `funclen.py` 방식 — 중괄호 균형): 대상 함수가 ≤50 이 됐는지, 새 헬퍼 중 >50 인 것은 §6 에 기록

### 5.4 최종 게이트 (머지 전)

- 브랜치 tip 에서 `rm -rf fw/build fw/build-remote` 후 클린 빌드 → `.bin` sha256 == §5.1
- `git diff b61ef0f..HEAD --stat` 에 `fw/vendor/`·`ref/`·`fw/test/` 변경 0
- 50줄 초과 함수 재집계: 38 → **13** (실측, 전체 328함수). 초안의 "≤6" 도 plan D9 의 "11"(슬라이스 7 전부 SAME 가정)도 미달 — H7 로 보류한 5건 중 `reg_publish_measure`(81) · `disp_compute_output`(61) 두 함수가 더해졌기 때문이다(`app_weld_tick`·`tcp_poll` 은 D9 에도 이미 있었고, 보류라 원래 길이 131·111 로 들어온다). 내역 = §6 표

  | 함수 | 줄수(코드) | 왜 남는가 |
  |---|---|---|
  | `app_modbus_apply_writes` | 256 (157) | 36분기 else-if 뼈대 + 13 클램프 분기 |
  | `app_lcd_input_dispatch` | 188 (149) | 35 `case…break` 뼈대 |
  | `app_weld_tick` | 131 (95) | 7-2 보류(`.bin` ≠) — 지정 초기화 22줄 |
  | `app_modbus_tcp_poll` | 111 (75) | 7-3 보류(`.bin` ≠) — `off`/`tx_len` 출력 |
  | `app_lcd_send_model_str` | 93 (83) | §4 미분할 결정 |
  | `reg_publish_measure` | 81 (54) | 슬라이스 3 보류(`.bin` ≠) |
  | `app_lcd_change_page` | 78 (57) | 8분기 뼈대 + set_page 꼬리 |
  | `weld_fsm_step` | 72 (53) | abort 판정·엣지 래치·타이머·READY/HOLD/default 뼈대 |
  | `app_config_load` | 64 (56) | 미착수(`fail++` 누산) |
  | `disp_compute_output` | 61 (54) | 7-4 헬퍼 보류 — 주석 이동만 |
  | `weld_step_weld` | 58 (49) | 헬퍼 잔존, 더 쪼개지 않음(§8) |
  | `app_lcd_disp_step` | 57 (55) | 함수-static 엣지 블록 §2.2 제외 |
  | `render_run_std` | 53 (48) | 헬퍼 잔존(RUN_STD 본체 49줄 + `n` 선언) |

---

## 6. 예외 목록 — 바이트 동일 조건에서 50줄 불가

| 함수 | 이유 | 이번 결과 | 후속(범위 밖) |
|---|---|---|---|
| `app_modbus_apply_writes` | 36분기 else-if 체인 뼈대만 코드 ≥110줄. 체인 분할은 감사 b-1 계열 3변형 전부 ✗ | 본체 3추출(`gate_reject_body`/`start_cmd_body`/`cfg_ctrl_commit_body`) → **256줄(코드 157)** | HW 벤치 트랙: 체인 → 테이블. 회귀 = `plans/2026-09-05-bench-results.md` FC06 클램프 27항목 재사용 |
| `app_lcd_input_dispatch` | 35 `case…break` 뼈대 ~105줄. 재그룹은 감사 b-3 ✗ | 본체 **3**추출(`handle_sys_pic_now`/`handle_setup_param_enter`/`handle_run_mode`) → **188줄(코드 149)**. `mo_time_clamp_echo`(LV_MO_TIME1/2 공통 꼬리 4줄)는 `.bin` ≠ (STD `ef9dbaa8…eee32a` / REMOTE `98823184…dd3297`, dispatch 크기 `0x3fc` 불변·명령 바이트 상이) → **보류** | HW 벤치 트랙: 핸들러 테이블. LCD 터치 E2E |
| `app_lcd_change_page` | 8분기 + 공통 꼬리, 로컬 배열 포인터 전달이 H4 경계 | 슬라이스 7-1 **SAME**(`render_run_std`/`render_setup_main`/`render_comm_page`/`render_comm_tail`) → 203→**78줄(코드 57)**, 헬퍼 `render_run_std` **53줄(코드 48)** 잔존 | HW 벤치 트랙: STDC/MHC·STDE/MHE 중복 블록 DRY |
| `weld_fsm_step` | WELD case 자체가 ~55줄 | case 본체 4추출 → **72줄(코드 53)**(abort 판정·엣지 래치·타이머·READY/HOLD/default 뼈대) + 헬퍼 `weld_step_weld` **58줄(코드 49)** 잔존 | host `test_app_weld_fsm.c` 21함수가 있어 **host 게이트 트랙**으로 구조 변경 가능 |
| `app_config_load` | `fail++` 누산 구조 | 미착수 (**64줄**, 코드 56) | host `test_app_config.c` 게이트 트랙 |
| `app_weld_tick` | `weld_in_t` 지정 초기화 22줄 + 트리거 스캔 — 헬퍼화 시 코드 변화 가능(§4) | 슬라이스 7-2 `.bin` ≠ (STD `5d0d5222…d9a334` / REMOTE `180a6e07…1b6527`, −4 B) → **보류**, **131줄(코드 95)** 잔존 | 글루 — host 트랙 없음, HW 벤치 |
| `app_modbus_tcp_poll` | 워커 루프가 `off`/`tx_len` 출력 요구 = out-포인터 ✗ | 슬라이스 7-3 `.bin` ≠ (STD `2fa973b6…0cb035` / REMOTE `c1cb8a50…11ff3a`, Δ0 크기·바이트 상이) → **보류**, **111줄(코드 75)** 잔존 | HW 벤치(TCP) |
| `reg_publish_measure` | 순차 3블록 — 슬라이스 3 대상이었으나 리터럴 풀/레지스터 할당이 어긋남 | 슬라이스 3 `.bin` ≠ (STD `7c8150ae…4b937f` / REMOTE `98003830…a84358`, `nm` 0x118→0x110 = −8 B) → **보류**, **81줄(코드 54)** 잔존 | host `test_app_reg*` 게이트 트랙 |
| `disp_compute_output` | `fill_upper_band`/`fill_lower_band` 값 인자가 H4 경계 | 슬라이스 7-4 헬퍼 `.bin` ≠ (STD `9c5ec0d2…b06494` / REMOTE `9f2aa9ca…948c97`, Δ0) → 헬퍼 **보류**, C1 주석 이동만 랜딩 → 76→**61줄**(코드 54 불변) | HW 벤치 트랙(바그래프 육안) |
| `app_lcd_disp_step` | 코드 55; 엣지 블록은 함수-static(`prev_run_on`·`prev_remote_on`) → §2.2 제외, switch 10 case 재그룹 ✗ | 슬라이스 1 주석 이동만 → **57줄(코드 55)** | HW 벤치 트랙(ICON_RUN/REMOTE 육안) |
| `app_lcd_send_model_str` | §4 미분할 결정(`#if` 브랜드 4블록 중 컴파일되는 것은 1블록 ~25줄) | 무변경 **93줄(코드 83)** | — |

예외는 이 표가 정본이다. `CLAUDE.md` 에는 쓰지 않는다(결정 범위 밖).

보류(H7) 5건은 전부 **첫 시도에서 되돌렸고 우회 재시도는 하지 않았다**. 되돌린 헬퍼 10개 =
`publish_sr_edge`/`publish_amp_power`/`publish_copy_out` · `mo_time_clamp_echo` ·
`weld_sensor_mirror`/`weld_dispatch_out` · `tcp_recv_accumulate`/`tcp_flush` ·
`fill_upper_band`/`fill_lower_band`. 랜딩한 헬퍼는 **27개**(계획 37 중).

---

## 7. 실행

| 항목 | 내용 |
|---|---|
| 브랜치 | `refactor/byte-identical` (main `b61ef0f` 에서) |
| 커밋 | 슬라이스당 1개. 메시지 `refactor(<모듈>): <함수> <N>분할 — .bin 동일 (STD fd66…e278f / REMOTE c522…7373f)`. 슬라이스 1 은 `refactor(comments): …`, 압축한 이력 문장의 소재 문서를 본문에 |
| 순서 | 1 → 2 → 3 → 4 → 5 → 6 → 7. 1 이 끝나면 함수 줄수 재집계로 2~7 대상 확정 |
| 분담 | spec·plan = Fable(이 문서 + plan). **구현 = Opus 세션**이 plan 대로. 슬라이스마다 `bin-same.sh` 출력을 커밋 메시지 또는 PR 본문에 |
| 머지 | 전 슬라이스 SAME + §5.4 → `--no-ff` 머지. HW 태그 없음(바이너리가 태그 `hw-revA_fw-stage-hold-wdt` 빌드와 동일하므로 그 태그의 벤치 결과를 그대로 승계) |
| 문서 | `docs/changelog.md` `[Unreleased]` 에 1항목(무엇·검증·예외 4개). 이 spec §6 에 슬라이스 7 결과 기입 |

---

## 8. 리스크

| 리스크 | 완화 |
|---|---|
| H1~H6 을 지켜도 리터럴 풀·레지스터 할당 차이로 `.bin` ≠ | H7: 되돌리고 보류. 우회(인자 순서 바꾸기 등) 시도 금지 — 시도 자체가 시간 소모이고 결과 예측 불가 |
| 주석 이동 중 문장 손실 | C2 + 리뷰 규칙 "diff 에서 `-` 된 주석 줄은 전부 `+` 로 다른 위치에 존재"(압축 대상 제외). 압축 문장은 커밋 본문에 소재 명시 |
| 헬퍼 이름이 의미를 잘못 붙여 오해 유발 | 이름은 감사 C-1 제안을 따르고, 헬퍼 위 1줄 주석에 "무엇을 하는 블록인가"만 |
| `always_inline` 헬퍼가 >50줄 (`weld_step_weld`) | 규칙 위반으로 보지 않고 §6 에 기록. 더 쪼개지 않는다(쪼갤수록 H4 경계에 가까워진다) |
| 툴체인 업데이트로 기준 해시가 바뀜 | §5.1 에 툴체인 버전 고정. 바뀌면 시작 커밋을 새 툴체인으로 재빌드해 baseline 갱신 후 진행 |
| 슬라이스 4·5 가 "이름 붙이기" 수준이라 가치가 작다는 반론 | 인지한 대가(결정 3). 진짜 분할은 HW 벤치 트랙(§6)으로 넘긴다 |

---

## 9. 참조

- 감사 보고서: `docs/superpowers/research/2026-09-08-refactor-audit.md` (spike 로그·objdump 분석·인벤토리 전수·되돌림 확인)
- 빌드: `fw/CMakeLists.txt:14-27`(Debug/-Og), `:19`(섹션 분리), `:91`(GLOB 링크 순서), `:113`(gc-sections), `:120`(POST_BUILD `.bin`)
- 사용자 규칙: `~/.claude/rules/common/coding-style.md`(함수 <50줄, 파일 <800줄)
- 프로젝트 규칙: `CLAUDE.md`(HW 게이트·태깅), 메모리 `feedback-fable-plans-opus-implements`, `feedback-confirm-before-code-change`
