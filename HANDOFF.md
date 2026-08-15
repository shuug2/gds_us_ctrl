# Handoff: 2026-07-19 fw 전체 리팩토링 완료 (브랜치, 머지 = HW 재검증 3항목 게이트)

**Generated**: 2026-07-19 (리팩토링 세션, Fable 5 / ponytail full + plan mode)
**Branch**: `refactor/ponytail-cleanup` tip `753778d` (12커밋, base = main `8eaac71`) — **origin 푸시됨**(2026-08-15 확인). main도 origin과 동기(`1364e5e`).
**Status**: 코드 작업 전부 완료·검증됨. 남은 것 = HW 재검증 3항목 → main 머지 (사용자 벤치 게이트).

> **⟳ 2026-08-15 갱신**: 이 문서는 2026-07-19 리팩토링 4스테이지 시점 기록이다. 이후 **2026-07-25 세션이 4커밋을 더 얹었다**(§Completed 표 하단 4행) — 그 세션은 핸드오프를 남기지 않았다. 또 "로컬 전용, 미푸시"는 stale(위 Branch 줄 정정됨).

> **요약**: fw 전체(src+drivers+include, ~8.5k라인)를 4스테이지로 리팩토링. ① 죽은 코드 삭제(미호출 게터 3+데모 매크로 2, 바이너리 동일) ② `app_lcd_input.c` 분할 1038→622(+신규 `app_lcd_comm.c` 415, cpp-review 바이트 단위 APPROVE) ③ `app_reg_tick` 118→57(safety ceiling/auto-terminate static 헬퍼 추출) ④ 전 269함수 한국어 ≤20자 헤더 주석 통일 — 장문 헤더의 load-bearing 정보(legacy/spec/V30/안전 근거)는 본문 verbatim 이동, **바이너리 완전 동일 입증** + 보존 지표 무손실(samd20 255·main.c: 112·§ 79·V30 9). 전 스테이지 our-code 0-warning + host 14스위트 PASS. 보드는 **미플래시**(여전히 `61524c1`).

## Goal

HW PASS 직후의 안정 상태를 보존하면서 소스 위생 개선: 800라인 규칙 위반 해소, 죽은 코드 제거, 함수 주석을 "≤20자 한국어 기능 설명"으로 통일(포팅 감사 추적자료는 본문 이동 보존 — 사용자 확정).

## Completed (스테이지별 게이트 = 0-warning 빌드 + host 14스위트 PASS)

| 커밋 | 내용 | 검증 |
|---|---|---|
| `b02f5b1` | 죽은 코드: `app_modbus_owns_usart6`, dgus rx/tx 게터 2(백킹 static은 SWD 진단용 보존+주석), `DGUS_DEMO_BOOT_PAGE/UPTIME_VP` | **바이너리 동일** |
| `e195564` | `app_lcd_input.c` 분할 — comm/ether/DATA_SAVE 13함수+전용 매크로 → `app_lcd_comm.c`, 심=`app_lcd_input_priv.h`(7 프로토타입+`run_page_for_mode` 승격) | cpp-review **APPROVE** (바이트 단위 diff: static 7 제거 외 무변경, 커밋 순서 보존, 심볼 충돌 0) |
| `30d001c` | `app_reg_tick` 추출 — `reg_check_safety_ceiling`(30s)/`reg_check_auto_terminate`(energy/on-time) static 헬퍼, 호출 순서 불변 | 〃 (30s ceiling 주석 verbatim 확인) |
| `65ceded`·`e43ebe1`·`ac41cd6` | 전 269함수 주석: 드라이버+시스템(108)/app 코어+weld·modbus·eth(92)/LCD+reg(69). 서브에이전트 6그룹 병렬, 각자 diff 자가검증 | **바이너리 동일**(stage3 대비 cmp) + 보존 지표 8종 무손실 |
| `e46cafd` | changelog 항목 | — |

**2026-07-25 세션 추가분** (별도 핸드오프 없음 — 이 표로 대체):

| 커밋 | 내용 | 검증 |
|---|---|---|
| `7d9b7da` | 모델 브랜드+버전 문자열을 신규 `fw/include/define.h`로 분리 — GDSONIC/DIAMT/POWERTECH/MOOHAN 4종 컴파일-타임 선택(`#error` 다중정의 가드), `VERSION_MSG` 20자 `_Static_assert` | legacy `send_model_str` 대비 **4브랜드 × freq 6 × type 3 = 72조합 바이트 동일**(차분 하네스) + GDSONIC 디스어셈블 명령 동일 |
| `e8e84fb` | **ether IP 편집 커서 fix** — `ip_to_string` 반환 16은 DGUS 쓰기 길이(필드폭)지 글자 수가 아닌데 커서로 써서 백스페이스 `16-strlen`번이 NUL만 먹었음(+`ether_current_number` /10 부수 손상). 커서만 `strlen`으로, 반환 16은 유지(render 쓰기 길이) | samd20 main.c:4094 동일 버그 = **의도적 이탈**. HW PASS(사용자 확인) |
| `ebfd7d5` | **MAKETECH** 브랜드 신설 — `SMT-{H\|A\|S}{freq}D`(type이 숫자 앞 = 기존 4종과 형식 상이) | legacy 대응 블록 없음 → 기대표 대조 18/18 PASS(`[10]`=NUL 포함). GDSONIC 바이너리 무변경(전처리 제외) |
| `753778d` | `fw.sh` 빌드/플래시 스크립트 + `CLAUDE.md` 절차 문서화 — stale `$STM32_TOOLCHAIN`·GLOB configure-타임 함정 자동 처리(`build/.src-glob` 스탬프) | 5브랜드 크로스빌드 0-warning |

> 브랜드 전환 = `fw/include/define.h`의 `#define` 5줄 중 정확히 하나만 주석 해제. `VERSION_MSG`도 같은 파일(현재 `"V3.0.0_260725       "` = 20자 패딩).

## Not Yet Done (순서대로)

- [ ] **★ HW 재검증 3항목** (사용자 벤치; 빌드→플래시 필요 — 보드는 아직 `61524c1`):
  1. LCD SETUP comm/ether 편집 + DATA_SAVE 저장/페이지 복귀 (Stage 2 이동 경로)
  2. 직접런 560ms ceiling + OVTIME 무회귀 (Stage 3 이동 경로)
  3. Modbus FC03/06 스모크
  - 실질 리스크 = 분할·추출 2커밋의 링크 순서 변화뿐(주석/죽은코드는 바이너리 동일이라 무위험).
  - **07-25분 추가 육안 2건**: SETUP 페이지 모델명 문자열(현 빌드=GDSONIC) / **IP 편집 시 백스페이스 1회에 한 글자씩** 지워지는지.
  - **MAKETECH 출하 빌드 시에만**: `define.h`의 `#define MAKETECH` 주석 해제 + `#define GDSONIC` 주석 처리 → 플래시 → SETUP에서 `SMT-H15D` 육안 1회.
- [ ] **머지**: PASS 후 `git checkout main && git merge --no-ff refactor/ponytail-cleanup` (+push, 태그는 리팩토링이라 불요).
- [ ] (이전 세션 이월) **HMI Task 8**(`~/dev/work/gds_us_hmi` + 그쪽 HANDOFF.md, RS-485+SERIAL/addr=1 복원) / **전류 0.60A 실측**(전류계 세션) / 6b·B-SEAM(보류).
- [ ] (2026-08-02 신규, main 쪽) **원격 제어 활성화 게이트** — 정책 승인·미구현. 이 브랜치와 무관하나 머지 후 로드맵 상위. 상세 = main의 `docs/superpowers/specs/2026-08-02-remote-enable-gate-decision.md`.
- [ ] **태그 7개 push**(사람 터미널 `git push origin --tags`) — 코드·docs push는 완료.

## Key Decisions

| Decision | Rationale |
|----------|-----------|
| `reg_ramp_level` 보존 (계획은 삭제였음) | in-code "Kept as the verified table reference (host-tested)" 명시 — M16 disasm 검증 기록. usart1 스텁과 같은 keep-intent 범주 |
| dgus rx/tx 백킹 카운터 보존(게터만 삭제) | SWD 정적 read 진단용(i2c_pot `s_err_count` 선례) — 주석 명시해둠 |
| 장문 헤더 주석 → 본문 verbatim 이동(삭제 아님) | 사용자 확정. legacy 참조=포팅 감사 추적자료 |
| 최종 통합 cpp-review 생략 | 구조분은 이미 바이트 단위 APPROVE, 주석분은 바이너리 동일이 더 강한 증명 |
| **하지 않음 6건(재시도 금지)**: `change_page` STDC/STDE dedup(블록 실제 상이+DGUS write 순서 HW-검증) / `apply_writes` 테이블화(else-if 순서 load-bearing) / `weld_fsm_step` 분할(HW 타이밍) / `app_lcd.h` 어휘 이동(**include 제거 효과 0 실측** — 비LCD 포함자 전원이 LCD 함수 실호출) / `mb_echo` 헬퍼(이득≈0) / `_fsm`·`_core`·`_calc` 접미사 통일(참조 파괴) | 계획 §하지 않음 — 근거 포함 |

## Current State

**Working**: `refactor/ponytail-cleanup` tip — 빌드 0-warning(FLASH 49.35%±), host 14스위트 PASS, working tree clean.

**보드**: **미플래시** — 여전히 main `61524c1` 코드. ⚠ 이전 세션 이월: **전원 OFF 관측**(SWD 0.003V — 재개 시 전원/ST-LINK 먼저) + **잔재 설정 불확정**(model_type/EN_*/comm_mode 실측 필요, ETH_STATIC .199 추정).

**바이너리 검증 방법**(재현): `env -u STM32_TOOLCHAIN cmake -B build -G Ninja && cmake --build build` 후 `arm-none-eabi-objcopy -O binary build/gds_us_ctrl.elf x.bin && cmp` — 주석-only 커밋은 어느 시점끼리든 동일해야 함.

## Files to Know

| File | Why It Matters |
|------|----------------|
| `fw/src/app_lcd_comm.c` (신규) | comm/ether 편집+DATA_SAVE commit/rollback — `app_lcd_input.c`에서 순수 이동. `data_save_commit` 내부 커밋 순서 verbatim(set_pot→comm_mode/ether→serial shadows) |
| `fw/include/app_lcd_input_priv.h` (신규) | input↔comm 분할 심 — 외부 모듈 사용 금지(첫 `_priv.h` 패턴) |
| `fw/src/app_reg.c` | `reg_check_safety_ceiling`/`reg_check_auto_terminate` 신규 static 헬퍼 (본문·주석은 구 블록 그대로) |
| `~/.claude/plans/elegant-soaring-curry.md` | 이 리팩토링의 승인된 계획(조사 결과·하지 않음 근거 포함) |

## 함정 (이 브랜치 작업 시)

- **CMake GLOB**: `file(GLOB src/*.c)`가 configure-time — 브랜치 전환으로 `app_lcd_comm.c`가 생기거나 사라지면 **`cmake -B build` 재구성 필수**(incremental만 하면 undefined ref). [[project-phase12-env]]
- **clangd 진단 노이즈**: include 경로 미설정으로 이 세션 내내 대량 오탐("file not found", "unknown type") — 실제 게이트는 크로스 빌드뿐.
- 주석 스타일: 함수 위 `/* ≤20자 한국어 */` 한 줄 — 새 함수 추가 시 이 관례 유지. load-bearing 설명은 본문 첫 줄 블록 주석.
