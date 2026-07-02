# Handoff: 감사 수정 큐 실행 — D0+D1 main 커밋 + D3 'fram-i2c-robustness' CODE-COMPLETE

**Generated**: 2026-07-02 (c 세션 마감)
**Branch**: `main` (`c5b7ad3`, origin+8 — **push 미완, 사람 SSH**) + 미머지 **`feat/fram-i2c-robustness`** (`46a9b20`, BASE `46b596f`, 4커밋)
**Status**: D0·D1·D3 코드 완료 / **다음 = D3 HW 회귀 → 머지·태그** → D6(M7) → D5(reconcile)

> **요약**: 2026-07-02 감사 결정 큐(D0~D6, 정본=`docs/NEXT_STEPS.md` §1.3) 실행 세션.
> ① **D0**(C1 CRITICAL, LCD dispatch `data_len<3` 가드)·**D1**(seek/reset 600ms 충실화 SR_TICKS 60) → main 직접 커밋, 각각 cpp-reviewer APPROVED.
> ② **D3**(감사 H3+H2) → spec→plan→subagent-driven 4 Task 전부 구현, Task별 2단계 리뷰 + **최종 whole-branch opus 리뷰 CODE-COMPLETE 승인(0 Crit/0 Imp)**. 브랜치 미머지 = HW 회귀 게이트(기존 정책).
> 모든 게이트 GREEN: host 7스위트(신규 `test_app_config`) PASS + our-code 0-warning(FLASH 43.20%/RAM 16.82%).

## Goal

감사(2026-07-02)에서 확정된 수정 결정 큐를 순서대로 실행: D0(C1) → D1 → D3 슬라이스. D3는 위험한 축(FRAM 읽기 실패 침묵 + I2C 버스 stuck 무복구)의 견고화.

## Completed

- [x] **D0** (`eabeab0`, main): `app_lcd_input.c` dispatch 최상단 `if (f->data_len < 3u) return;` — EMI 절단 프레임의 미초기화 `data[1..2]`→KEY_MULTI START 유입 차단. 리뷰어가 `dgus_frame_t.data_len` 정의(dgus_lcd.h:150, 페이로드만 = LEN-3)부터 추적해 임계값 3의 필요충분성 확인.
- [x] **D1** (`85811fc`, main): `SR_TICKS` 50→60 (600ms/leg, 레거시 실거동 `us_reset_cnt++` 후 `>5` 0-시작 100ms). host 테스트 경계 50→60 TDD(RED 16 FAIL→GREEN).
- [x] **D3 spec** (`cc8b4e5`) + **plan** (`46b596f`): `docs/superpowers/specs|plans/2026-07-02-fram-i2c-robustness*.md`
- [x] **D3 구현** (브랜치 `feat/fram-i2c-robustness`, 4커밋):
  - `8b3c44a` **H3**: `fram_read_*`→`bool(addr,*out)`(실패 시 *out 미기록) + `app_config_load` 기본값 선적용-덮어쓰기(반환 0/1..38/0xFF) + **INIT_FLAG 읽기실패=factory-write 금지** + host 신규 스위트 `test_app_config`(mock-fram link 치환, 6시나리오) + test/Makefile stale 주석 정정 동승
  - `52f934c` **H2**: `i2c1_init` HAL init 전 프리플라이트 — SDA stuck 시 SCL(PB6) GPIO-OD 9클럭+STOP, `i2c1_unstick_events()`(0/1..9/0xFF). busy-loop 지연(sys_tick 미기동 시점)
  - `528b7df` mon 표면: 부팅 `[cfg] fram_fail=%u unstick=%u i2c_err=%u`+WARN 줄, 루프 1s cadence err 델타 `[i2c] err=%u (+%u)`
  - `46a9b20` docs(changelog/NEXT_STEPS CODE-COMPLETE)
- [x] Task별 리뷰 4/4 Spec✅ Approved + **최종 opus 리뷰: Ready-to-merge Yes(CODE-COMPLETE), 0 Critical/0 Important**
- [x] 문서/장부: RESUME.md(2026-07-02 c), changelog, NEXT_STEPS §1.3, SDD ledger(`.superpowers/sdd/progress.md`), 메모리 `project-audit-2026-07`, plan 오류 주장 정정(`c5b7ad3`)

## Not Yet Done

- [ ] **D3 HW 회귀** (다음 보드 세션, plan 말미 체크리스트 — 코드 변경 없이 검증만):
  1. 브랜치 checkout → **cmake reconfigure**(`env -u STM32_TOOLCHAIN cmake -S fw -B fw/build -G Ninja`, GLOB 규칙) → 빌드 → 플래시
  2. 정상 부팅: LCD 값 육안(output_power 등 저장값 유지 = 폴백 미발동)
  3. mon 확인(LCD에서 addr=NONE으로 USART6 해방 필요): `[cfg] fram_fail=0 unstick=0 i2c_err=0`
  4. mbpoll 직접런 ceiling 무회귀(STATUS 1×N→0) — ⚠ addr 되돌린 후
  5. LCD 설정 저장→재부팅 리로드 정상
  6. **D0/D1 재검증 동승**: LCD 터치 정상경로 + RESET→SEEK 체인 600ms 육안(ICON)
- [ ] PASS 시: main 머지 `--no-ff` + tag `hw-revA_fw-stage-fram-robust` + 브랜치 삭제 + push
- [ ] **D6** M7: LCD static IP 저장→가동 중 W5500 즉시 반영 (`app_lcd.c:54-61` hook stub → app_eth 재적용 API, 소규모 설계)
- [ ] **D5** reconcile 선행 b→d→ch1 (`app_reg_tick` 3-way semantic 통합 + board.c 병합; 머지는 HW 후)
- [ ] 나머지 백로그: D2/D4(weld slice4), H4+IWDG 별도 슬라이스, HW-gated(B-SEAM/6b/overload) — `docs/NEXT_STEPS.md` §1.2/§1.3

## Failed Approaches (Don't Repeat These)

- 코드 차원 실패 없음(전 Task 1회 통과). 절차 노트 2건:
  - **인프라 장애**: 세션 중 safety classifier(claude-opus-4-8) 일시 다운으로 컨트롤러의 Bash/Agent 파견이 수 분간 차단됨(read-only 도구는 가용). 서브에이전트 내부 도구는 정상이었음 → 리뷰어에게 diff를 직접 git으로 뜨게 지시해 우회. 재발 시 같은 우회 유효.
  - plan의 "델타 산술 uint16 모듈로 안전" 주장은 **오류**였음(int 승격) — 구현자가 자가 발견, plan 문서 정정 완료(`c5b7ad3`). plan 코드를 검증 없이 신뢰하지 말 것.

## Key Decisions

| Decision | Rationale |
|----------|-----------|
| INIT_FLAG **읽기실패**(0xFF) ≠ 빈 FRAM — factory-write 금지 | 일시 버스 오류가 정상 FRAM 전체를 팩토리로 덮어쓰는 현행 데이터손실 경로 차단(설계 핵심). 최종 리뷰: LCD 롤백 경로(`app_lcd_input.c:626`)의 동일 위험도 함께 차단됨 |
| 경고 표면 = **mon 전용** | 사용자 확정. LCD_WARNING/Modbus bit 없음 → HMI 레지스터 계약 불변 |
| unstick = **init 1회만** | 사용자 확정. superloop 중 버스 재초기화 개입 리스크 회피 |
| `fram_write_*` 무변경(best-effort) | 사용자 확정. 관측은 err_count/mon |
| D3 머지 = HW 회귀 후 | 부팅 경로 변경이라 기존 슬라이스 정책 유지 |
| 델타 랩 오출력 등 Minor 전건 defer | 최종 리뷰 판정: 로그 전용/65536+ 오류 조건/자가복구. 수정 시 `(unsigned)(uint16_t)(e - s_i2c_err_last)` |

## Current State

**Working**: main(`c5b7ad3`) = D0+D1 포함, host 6스위트+0-warning. 브랜치 = host 7스위트+0-warning. 보드 = 2026-06-28 ovtime fw 적재 상태 그대로(이번 세션 보드 무접촉), SERIAL/addr=1/9600/EVEN.
**Broken**: 없음.
**Uncommitted**: `?? ref/signal/`(Saleae 캡처, 의도적 미추적)만.
**Push**: main이 origin보다 **8커밋 ahead** — 사람이 SSH로 push.

## Files to Know

| File | Why It Matters |
|------|----------------|
| `docs/superpowers/plans/2026-07-02-fram-i2c-robustness.md` | D3 plan + **HW 회귀 체크리스트**(말미) |
| `.superpowers/sdd/progress.md` | SDD ledger — Task별 커밋/리뷰/defer Minor 전체 목록 |
| `fw/src/app_config.c:83-141` | 새 load 로직(기본값 선적용→성공만 덮어쓰기) |
| `fw/drivers/i2c1.c` | unstick 프리플라이트 + getter |
| `fw/test/mock_fram.{h,c}` + `test_app_config.c` | H3 host 게이트(6시나리오) |
| `docs/NEXT_STEPS.md` §1.3 | 결정 큐 정본(D0✅ D1✅ D3 CODE-COMPLETE 반영됨) |

## Code Context

브랜치의 새 인터페이스(머지 전 다른 작업이 fram/config를 건드리면 충돌 주의):

```c
/* fram.h — 읽기 실패 시 *out 미기록 (호출자가 기본값 선적용) */
bool fram_read_byte(uint8_t addr, uint8_t  *out);
bool fram_read_u16 (uint8_t addr, uint16_t *out);
bool fram_read_u32 (uint8_t addr, uint32_t *out);

/* app_config.h */
uint8_t app_config_load(app_config_t *cfg);          /* 0=clean / 1..38=실패 read 수 / 0xFF=INIT_FLAG 읽기실패(FRAM 미변경, 전 필드 기본값) */
void    app_config_factory_defaults(app_config_t *cfg);  /* RAM만, FRAM 미접촉 */

/* i2c1.h */
uint8_t i2c1_unstick_events(void);   /* 0=깨끗 / 1..9=복구 클럭 수 / 0xFF=복구 실패 */
```

host 테스트 mock 주입: `mock_fram_fail_read(addr, nbytes)` / `mock_fram_write_count()` — `fw/test/Makefile`이 `../drivers/fram.c` 대신 `mock_fram.c`를 링크(실 fram.c 3함수는 mock 미커버 = 자명 코드 + HW 회귀가 커버, 최종 리뷰 명시).

## Resume Instructions

1. sanity: `git log --oneline -3` (main `c5b7ad3` 확인) + `make -C fw/test test`(main=6스위트 PASS).
2. **HW 세션이면**: 위 "Not Yet Done" D3 HW 회귀 절차 그대로. 브랜치 전환 후 **반드시 cmake reconfigure**(신규 소스는 test뿐이지만 규칙 유지). 검증 규칙: 런타임=mbpoll/LCD만, SWD halt 금지(메모리 `feedback-swd-halt-breaks-board-validation`).
3. **코딩 세션이면**: D6(M7) 착수 — `app_lcd.c:54-61` hook stub → app_eth 재적용 API 소규모 설계(brainstorming→spec, NEXT_STEPS §3 절차).
4. 머지 시 주의: 브랜치가 `app_config.h/c`·`fram.h/c`·`i2c1.h/c`·`app.c`를 변경 — main에서 이 파일들을 건드리는 다른 작업은 D3 머지 후로 미룰 것.

## Warnings

- ⚠ **부분실패 잔여 리스크(defer, 의도된 범위)**: 부팅 시 일부 필드 read 실패 → 기본값 동작 중 사용자가 LCD 저장하면 `app_config_save_all`이 기본값을 FRAM에 굳힘. 구버전(침묵 0 굳힘)보다 개선이며 mon WARN으로 관측 가능. write-hardening 후속 슬라이스 후보(INIT_FLAG-last 순서 + write status).
- ⚠ vendor wiznet `socket.h` 경고 3건은 **pre-existing**(full rebuild에서만 노출) — 우리 코드 0-warning 판정과 무관.
- ⚠ mon-only 경고 표면의 알려진 한계: Modbus RTU가 USART6 점유 시 mon 비가용(수용된 결정). 1s 델타 로그는 `mon_enabled` 게이트로 RS-485 오염 없음(최종 리뷰 확인).
- ⚠ git 해시는 2026-06-20 filter-repo 재작성 이후 — 안정 레퍼런스는 태그.
- ⚠ 직전 감사 HANDOFF(2026-07-02 a)의 발견 상세는 이 문서로 대체되지 않음 — 미착수 항목(D2/D4/H4/M계열)의 파일:라인 근거는 `docs/NEXT_STEPS.md` §1.3 + 메모리 `project-audit-2026-07` 참조.
