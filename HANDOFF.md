# Handoff: Stage D slice 2b — RUN 명령 게이트 (코드+리뷰 완료, HW 재검증만 잔여)

**Generated**: 2026-06-10 b (리뷰 세션 마감)
**Branch**: `feat/stage-d-slice2b-run-gate` (main 미머지, tip `9a54703`)
**Status**: ✅ **코드 완료 + 통합 cpp-reviewer APPROVED(b78cbbf+27d45c3, CRITICAL/HIGH 0).** ⏸ **Blocked on HW: 보드 미연결 — RUN HW 재검증만 남음** → 통과 시 머지 + 태그 `hw-revA_fw-stage-d2b`.

> **요약**: V30 RUN 버튼 quirk fix(`b78cbbf`)와 M16 충실도 정정(`27d45c3`: per-START 램프 폐기→부팅 1회 워밍업, TOUCH on-time ceiling 추가, 1ms ADC 페이스)까지 코드·리뷰 전부 끝났다. 남은 것은 보드 재연결 후 **변경된 기대 거동**(워밍업 1회, RUN 즉시 구동·램프 없음, 500ms ceiling 자동정지, release swallow 1회)의 HW 재검증뿐이다. ⚠ 2026-06-08 이전 핸드오프의 "RUN press→램프 0→401" 합격기준은 **폐기됨** — 이 문서 기준만 사용.
>
> 1차 핸드오프 = `docs/superpowers/RESUME.md`(SessionStart 자동 로드). 본 문서는 HW 절차 상세. 응답 **한국어**(코드/식별자 영어). 워크스페이스 규칙: **코드는 요청한 부분만 수정**. graphify 미사용(2026-06-10 폐기).

## Goal

SAMD20 명령 FSM의 **RUN 게이트**를 흡수해 regulation core를 터치 RUN 명령 구동으로 전환 + M16 디스어셈블리 교차검증으로 확정된 **원본충실 의미론**(부팅 1회 워밍업, START 즉시 level-follow) 반영. OSC 물리출력(B-SEAM)·SEEK/RESET 효과·overload·weld-cycle·Modbus = deferred.

## Completed

- [x] **slice 2b 코드 3커밋**(`decfc28`/`ed2093f`/`4264bab`) — run FSM·ICON_RUN·SYS_PIC_NOW. 2026-06-08 통합 리뷰 APPROVED. (상세 = changelog 2026-06-08)
- [x] **HW 부분검증 2026-06-08**: 부팅 IDLE PASS + RESET/SEEK 라우팅 PASS(hook→`app_reg_command` HW 확인).
- [x] **`b78cbbf`** — V30 RUN 버튼 `KEY_MULTI(0x1080) data=0`(양 edge) → live `us_run_status`로 START(IDLE)/RUN_RELEASE(running) 매핑. spec §4.4.
- [x] **`27d45c3`** — 의미론 정정 3건(2026-06-10 분석 문서 = 권위 소스):
  - ① M16 램프 = **부팅 1회 워밍업**(per-START 소프트스타트는 오독) → boot ~4s 워밍업(명령 무시·sel=0) 후 **START 즉시 `sel=reg_scale(ch0_avg)`, 램프 없음**.
  - ② **TOUCH run on-time ceiling**: `limit_on_time×10ms`(기본 500ms, 패널 편집, 0=off) — 의도적 안전 추가(원본은 REMOTE/COMM+HAND 전용). ceiling 정지 후 잔여 release가 START로 역매핑되는 것은 `swallow_start` 1회 소비로 보정.
  - ③ ADC **1ms 양채널**(ch0_avg 10ms / ch1_avg 50ms).
- [x] **통합 cpp-reviewer(b78cbbf+27d45c3) = APPROVED**, CRITICAL/HIGH 0 (2026-06-10 b). 코너 케이스 전수: data=0 매핑(더블 프레스·release 유실·ceiling 후 orphaned release·swallow 누출/오흡수 없음·SYS_PIC_NOW 상호작용), 워밍업 단발성, ceiling/ADC 산술, ISR 무race. 스펙 편차 4건 = 전부 분석 문서 승인 확인.
- [x] **`c10b0aa`** — 리뷰 비차단 M2/L1 주석 반영(코드 무변경). **`9a54703`** — changelog/RESUME.
- [x] 빌드 0-warning: main `fw/build/` 28.67% / 트레이스 `fw/build-trace/` 29.00%(REG_TRACE+LCD_TRACE_RX, **2026-06-10 b 재빌드 완료 = 최신 소스 포함, 재플래시만 하면 됨**). 호스트 `make -C fw/test test` PASS.

## Not Yet Done

- [ ] **보드 재연결 → HW 재검증**(▼ Resume — **합격기준이 2026-06-08과 다름**, 아래 기준만 사용).
- [ ] 통과 → `superpowers:finishing-a-development-branch`(머지) + 태그 `hw-revA_fw-stage-d2b`.
- [ ] slice 2b 머지 후 → **stacked 브랜치 `refactor/stage-d-m1-cfg-param-injection` rebase/머지**(2026-06-12, base = 본 브랜치 tip `322a779`: M1 파라미터 주입 + `us_on_time_200m` 공급, cpp-reviewer APPROVED-WITH-COMMENTS 반영 완료). 머지 시 LV_TIME 바 HW 확인(런 중 200ms 단위 fill, 정지 후 latch). ⚠ 본 브랜치/`fw/build-trace/`는 무변경 — 아래 Resume 절차 그대로 유효.
- [ ] (DEFERRED) SEEK/RESET 효과(RESET→SEEK 500ms 자동 체인 의미론 = 분석 문서 §5 필수)·overload(M_OVLD 극성 실측 필수)·weld-cycle·Modbus·OSC 구동(B-SEAM = 3선 active-LOW 미러 확인으로 축소)·6b cal·LCD 갭 2건(아이콘 엣지/work_cnt — `us_on_time_200m`은 2026-06-12 해소).

## Failed Approaches (반복 금지)

- **slice 2a per-START 소프트스타트 램프**: M16 디스어셈블리 오독(0x0195 재진입 가능으로 봄). 실제는 nonzero 쓰기 @0x1B8A 1곳(부팅)뿐, 램프 중 OSC off+명령 무시 @0x041E → **부팅 1회 워밍업으로 회귀**(`27d45c3`). 램프를 RUN에 다시 붙이지 말 것.
- **RUN data=3/4 기대**(samd20 포팅 그대로): V30 DGUS 에셋이 RUN만 data=0을 양 edge 반환(HW-traced 2026-06-08, RESET=1/SEEK=2 정상) → 미발화. 에셋 수정(Option A)은 편집 DWIN 프로젝트 부재로 불가 → 펌웨어 적응 `b78cbbf`(Option B, 사용자 결정).
- **ADC 2ms/1채널 교대**(40/400ms 평균창): 원본 8.3/42ms 대비 5~10× 느림 → 1ms 양채널로 복원(`27d45c3`).
- **`-DCMAKE_C_FLAGS` 단독 지정**: 툴체인 CPU 플래그를 덮어써 ARM-mode 빌드 실패. 트레이스 빌드는 기존 `fw/build-trace/` 캐시 재사용.

## Key Decisions

| Decision | Rationale |
|----------|-----------|
| 부팅 1회 워밍업(~4s, 명령 무시·sel=0) + START 즉시 level-follow | M16 원본충실(디스어셈블리 교차검증, 사용자 결정 = 충실 회귀) |
| TOUCH ceiling `limit_on_time×10ms`(기본 500ms, 0=off) | 원본 비충실 **의도적 안전 추가** — V30 data=0 quirk가 release를 유실하면 무한구동 위험 |
| `swallow_start` 1회 소비 | ceiling 정지 후 잔여 release가 data=0 로직에서 START로 역매핑되는 것을 흡수; 리뷰어가 누출/오흡수 없음 검증 |
| V30 RUN = data=0 → state 매핑(B) | data=0이 RUN 고유라 모호성 0; self-syncing(edge 유실→다음 press가 복구); FSM/enum 무변경 |
| START 가드 `==US_IDLE` / SYS_PIC_NOW mid-run→RUN_RELEASE | advisor/리뷰어 catch (2026-06-08, spec §4.2/§4.3) |
| 리뷰 M1(순환 의존)·L2(reg_ramp_level 잔류) defer | M1=후속 슬라이스 리팩터링; L2=검증 레퍼런스 의도적 보존 |

## Current State

**Working**: tip `9a54703` = main(`396ec18`=slice 2a 머지) + slice 2b 커밋들. working tree clean. 빌드/테스트 전부 통과(위 Completed).

**Broken**: 없음. 차단 = 보드 미연결(ST-LINK·usbserial 모두 부재 확인됨 2026-06-10 b).

**Uncommitted**: 없음. untracked = `ref/atmega16/M16_reverse/`(분석 ground-truth, 커밋 여부 사용자 결정) + `.understand-anything/`(둘 다 정상).

## Files to Know

| File | Why It Matters |
|------|----------------|
| `docs/superpowers/analysis/2026-06-10-samd20-m16-ipc-semantics-verified.md` | **의미론 권위 소스**(스펙 §4.2/§3.1 일부 대체) — 워밍업/ceiling/ADC/RESET-SEEK 체인/B-SEAM 축소 |
| `docs/superpowers/specs/2026-06-08-stage-d-slice2b-run-gate-design.md` | §4.3 SYS_PIC_NOW · §4.4 V30 RUN data=0 |
| `fw/src/app_reg.c` | run FSM + 워밍업 + ceiling + swallow_start + ADC 페이스 (`27d45c3`) |
| `fw/include/app_reg.h` | `app_reg_command`/`app_reg_tick` 계약(ceiling은 `app_lcd_cfg()` 라이브 읽기) |
| `fw/src/app_lcd_input.c` | `handle_key_multi` data=0 매핑(`b78cbbf`) + same-burst stale 노트(`c10b0aa`) |
| `fw/src/app_lcd_disp.c` | ICON_RUN 엣지 렌더 |

## Code Context

```c
/* app_reg.c 핵심 상태 (superloop 단일스레드, ISR 비접근) */
main_state==1   /* 부팅 워밍업: app_reg_init()에서만 set, rc 0→401(10ms 페이스)→0 단방향, 명령 무시+sel=0 */
US_CMD_START    /* 가드: main_state==0 && us_run_status==US_IDLE && !swallow_start → US_TOUCH, sel=reg_scale(ch0_avg) 즉시 */
ceiling         /* US_TOUCH && limit_on_time!=0 && (now-run_start_ms) > lim*10ms → 정지 + swallow_start=1 */
/* app_lcd_input.c handle_key_multi: data=0 → us_run_status==US_IDLE ? START(+set_pot) : RUN_RELEASE */
```

REG_TRACE 출력: `[reg] cmd=<us_cmd> run=<us_run_status> st=<main_state> rc=<ramp_counter> sel=<sel> band=<band>` (+ `[lcd-hook] us_command=N`, `[lcd] rx vp=.. data=..`).

## Resume Instructions — RUN HW 재검증 (⚠ 합격기준 변경판)

1. **보드 연결 확인**: `ls /dev/cu.usbserial-* /dev/cu.usbmodem*` (ST-LINK + USART6 mon @115200). BOOT0=GND.
2. **재플래시**(트레이스 빌드 = 최신, 재빌드 불필요):
   ```bash
   openocd -f fw/openocd/stm32f410.cfg -c "program fw/build-trace/gds_us_ctrl.elf verify reset exit"
   ```
3. **캡처**(단일 fd):
   ```bash
   DEV=$(ls /dev/cu.usbserial-* | head -1)
   { stty -f "$DEV" 115200 cs8 -parenb -cstopb raw -echo; cat; } < "$DEV" > /tmp/reg-mon.log &
   tr -d '\000' < /tmp/reg-mon.log | tr -s ' ' | grep -E '\[(reg|lcd-hook|lcd\] rx|boot)'
   ```
4. **합격기준** (이전 "RUN press→램프" 기준 폐기):
   - **부팅 워밍업 1회**: 리셋 직후 `st=1 rc` 0→401(~4s) 동안 `sel=0`·`run=0`; 이 동안 패널 명령(RESET/SEEK/RUN) **무시**됨; rc=401 → `st=0` 단방향(재진입 없음).
   - **RUN 누름**(워밍업 후): `[lcd] rx vp=0x1080 data=0` → `[lcd-hook] us_command=0` → `[reg] cmd=0 run=2` + **`sel=reg_scale(ch0_avg)` 즉시**(전압 미주입이면 idle 노이즈 플로어 ≈0~소값, 램프 상승 없음) + **ICON_RUN 점등**.
   - **ceiling 자동정지**(기본 limit_on_time=50=500ms): press 후 ~500ms에 `run=0`+ICON_RUN 소등+`sel=0`. 이어 손을 떼면 release data=0이 START로 역매핑되지만 **swallow 1회 소비 → 재시동 없음**이 정상.
   - **500ms 내 release**: `us_command=3`(RUN_RELEASE) → `run=0` + ICON_RUN 소등 + VAR_POWER=last_power latch.
   - **재press**: 정상 재시동(swallow 잔류 없음). 긴 hold 테스트가 필요하면 패널에서 `limit_on_time=0`(off) 설정 후 반복.
   - **무회귀**: RESET `cmd=2`/SEEK `cmd=1` no-op 유지, OSC PB2/PB10/PB14 idle-HIGH, LCD 네비.
   - 실패 시: `[lcd] rx`로 실제 vp/data 재확인(에셋 인코딩 변형 가능성) → `superpowers:systematic-debugging` 재진입.
5. **통과 →** `rm -rf fw/build-trace` → `superpowers:finishing-a-development-branch`(머지, origin push는 local-authoritative 정책 확인) + 태그 `hw-revA_fw-stage-d2b` → changelog/RESUME/메모리 "done" 갱신.

## Warnings

- **이전 HANDOFF(2026-06-08)의 RUN 합격기준(press→rc 0→401 램프)은 폐기** — `27d45c3` 의미론과 모순. 이 문서 §4 기준만 사용.
- 모든 cmake는 `env -u STM32_TOOLCHAIN`. `-DCMAKE_C_FLAGS` 단독 지정 금지.
- clangd/LSP 진단은 노이즈 — 게이트 = cmake 0-warning + `make -C fw/test test`.
- ceiling 기본 500ms 때문에 "hold하면 계속 구동"이 **아님** — 길게 잡고 싶으면 패널 limit_on_time=0. 검증 중 혼동 주의.
- `reg_ramp_level()`이 코드에 남아 있는 것은 의도(검증 레퍼런스, 리뷰 L2) — 지우지 말 것.
- 보드 USB가 세션 중 분리될 수 있음 — 단계마다 장치 존재 재확인.
- (pre-existing) `app_lcd_disp.c:24` "No callers yet" 주석 stale — 후속 정리 후보.
