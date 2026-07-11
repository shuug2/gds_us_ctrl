# Handoff: 2026-07-11 FW 벤치 세션 — 신규 3건 중 2건 HW PASS + OVTIME 경고화면 복귀 버그 발견·수정·검증, 다음 = HMI

**Generated**: 2026-07-11 (세션 마감, 풀배선 벤치 + ETH .199 인터랙티브)
**Branch**: `main` tip `88faf08`(+이 docs 커밋) — **main `88faf08`까지 push 완료**(세션 중 확인) — 남은 push = 이 docs 커밋 + **태그 `hw-revA_fw-stage-mbtcp-hardening`**(사람 터미널: `git push origin main && git push origin hw-revA_fw-stage-mbtcp-hardening`)
**Status**: FW 벤치 사실상 종결(전류계 항목만 이월). **★ 다음 세션 = HMI Task 8** (`~/dev/work/gds_us_hmi`, 별도 repo).

> **요약**: 2026-07-08 코드-완료분 3건을 재플래시 후 벤치 검증 — **#1 유령 런 소멸 PASS**(워밍업 중 press→release 무출력 + hold-to-run + RESET 체인 케이스), **#2 REMOTE icon PASS**(TCP 폴링 중 ON/중단 ~1s 후 OFF, **V30 에셋 0x120e 렌더 첫 입증**), **energy/OVTIME 타이밍 PASS**(TIMEOVER=1s → t≈1.0–1.09s ×3 재현 = 리뷰 MEDIUM 해소). #3 EMA 반응 체감·전류 0.60A는 전류계 미준비로 이월. 벤치 중 **사용자가 신규 버그 발견**: OVTIME 경고화면이 RESET 후에도 복귀 안 됨 → 근본원인 2중(원격 클리어 시 페이지 복귀 부재 + `show_error()`의 `lcd_status` 스탬프 누락) → **fix 2커밋(`83498e7`+`88faf08`) + HW 재검증 PASS**(Modbus RESET 자동복귀 / 터치 RESET 즉시복귀).

## Goal

① 2026-07-08 신규 3건 HW 벤치 검증(체크리스트=직전 HANDOFF §Resume) ② 벤치 중 발견된 OVTIME 경고화면 복귀 버그 수정·재검증.

## Completed

- [x] **재플래시**: `78a1e43` 코드 → (fix 후) 최종 **`88faf08` 코드가 보드에 플래시됨** (Verified OK ×3회).
- [x] **#1 유령 런 소멸 PASS**: 전원 인가 → 4s 내 RUN press 유지 → 워밍업 후 release → 무출력 확인 + 정상 hold-to-run + RESET 체인(1.2s) 중 press 케이스 무해.
- [x] **#2 REMOTE icon PASS**: mbpoll TCP 500ms 연속 폴링 중 ON → 중단 ~1s 후 OFF. V30 에셋의 VP 0x120e 실렌더 첫 확인(Stage C 스킵분 해소).
- [x] **energy/OVTIME 타이밍 PASS**(EMA 리뷰 MEDIUM): EN_ENERGY=1+TIMEOVER=1s+START → STATUS=8이 t≈1.04/1.08/1.09s 발화(3회 일관, 종전과 동일) + RESET 복구(bit3→0). energy 모드의 560ms ceiling 대체 동작 정상.
- [x] **`83498e7` fix(lcd)**: 원격(Modbus)/물리 RESET fault 클리어 시 경고 페이지 미복귀 — `app_lcd_fault_cleared()` 신설(app_lcd_tick 미러의 error_status nonzero→0 엣지 호출, LCD_WARNING일 때만+E-stop 보류). cpp APPROVE-W-C(0C/0H, MEDIUM/LOW 반영).
- [x] **`88faf08` fix(lcd)**: `show_error()`에 `state->lcd_status=LCD_WARNING` 스탬프 누락 보완(legacy main.c:4232 쌍 상실) — **진짜 근본원인**. 부수 교정: `app_lcd_in_run_page()` weld SETUP-freeze 게이트가 OVTIME 경고 중 오탐하던 것 해소. cpp 재검증 APPROVE.
- [x] **fix HW 재검증 PASS**: 테스트 A(OVTIME→Modbus RESET→**화면 자동 복귀**) + 테스트 B(OVTIME→**터치 RESET 즉시 복귀**+STATUS 클리어).
- [x] 게이트: our-code 0-warning + host 13스위트 PASS(양 커밋) + 테스트 잔재 원복(EN_ENERGY=0, TIMEOVER=8).
- [x] 메모리 갱신: [[project-ovtime-energy-run]](버그 2건+교훈) / [[project-bench-test-env]](model_type ceiling 함정).

## Not Yet Done

- [ ] **★ HMI SP1 Task 8 실보드 E2E** — `~/dev/work/gds_us_hmi` 폴더 세션 + **그쪽 HANDOFF.md**로 진입. RS-485 어댑터 연결 필요(현재 미접속, 보드는 ETH_STATIC이라 LCD에서 SERIAL/addr=1 복원 필요). 이 repo `docs/superpowers/research/2026-07-05-rs485-first-write.md` §6(첫-write 재현 절차) 지참.
- [ ] (이월 2회째) **전류 표시 0.60A 실측** + **#3 EMA 반응 체감**(과하면 `app_reg.c` `d/2`→`d/4` τ≈200ms 폴백) + **energy-exit 실전류**(에너지-도달 정상정지) — 전류계 준비된 FW 벤치 세션.
- [ ] push (사람 터미널) — **main은 `88faf08`까지 push 완료 확인** — 남은 것 = 이 docs 커밋 + **태그** `git push origin main && git push origin hw-revA_fw-stage-mbtcp-hardening`.
- [ ] 6b 잔여 / B-SEAM — ⏸ 사용자 보류 유지.
- [ ] 후속 소소(변경 없음): app_eth STATIC_UP 링크 재폴링 / KA 무송신-피어 / defer Minor(ledger) / handle_key_multi RESET OVLD 비트 휘발성(LOW).

## Failed Approaches (Don't Repeat These)

- **OVTIME 화면 fix 1차 = `83498e7` 단독** — HW에서 실패. `app_lcd_fault_cleared()`의 `lcd_status != LCD_WARNING` 게이트가 항상 조기 리턴: **`app_lcd_show_error()`가 물리 페이지만 전환(dgus_set_page)하고 `state->lcd_status`를 스탬프하지 않아**(legacy main.c:4232-4233은 쌍) state는 런 페이지로 남아 있었음. cpp-reviewer도 이 전제를 오독(스탬프가 있다고 단정)한 채 APPROVE — **HW 벤치가 유일하게 잡음**. 교훈: **`dgus_set_page()` 호출은 반드시 `state->lcd_status` 스탬프와 쌍** (다른 모든 호출부는 쌍인데 show_error만 outlier였음).
- **mbpoll 쓰기 값 위치**: 값은 **IP 뒤** (`mbpoll ... 192.168.1.199 1`). 테스트 스크립트 helper가 값을 IP 앞에 넣어 mbpoll이 host="1"로 접속 시도 → "START write FAILED" 오진으로 사이클 낭비. 또 **부팅 직후 첫 TCP 트랜잭션들은 조용히 실패 가능**(link-up 직후) + W5500 단일 소켓에 연속 접속 시 간헐 거부 → 스크립트는 재시도(0.4s 간격 ×3) 필수.

## Key Decisions

| Decision | Rationale |
|----------|-----------|
| 복귀 로직 = 미러 nonzero→0 엣지 단일 지점 (`app_lcd_fault_cleared`) | legacy는 Modbus/물리 RESET 핸들러 각각이 페이지 복귀(main.c:4356-4370/4605-4617) — 포팅 레이어링상 LCD가 페이지 소유이므로 클리어 소스 전부(REMOTE/물리/터치)를 fault 표면 한 곳에서 커버. 터치 경로는 이미 복귀해 자연 no-op |
| `show_error`에 스탬프 추가(내부 상태 정합) > 게이트 완화 | 스탬프가 samd20-충실 + `app_lcd_in_run_page()`(weld SETUP-freeze)·`set_overload(false)` 분기 등 lcd_status 소비자 전반의 정합을 함께 회복 |
| Modbus 직접런 무정지 = 무수정 (회귀 아님) | 운영 ceiling은 slice-D 설계상 **HAND 모드의 COMM/REMOTE 전용** — 보드 잔재 model_type=multi(1)에서는 30s 안전 캡만 정상. NEXT_STEPS §2.3-a 잔재 목록과 일치, 메모리 기록 |
| fix 2건 main 직접 커밋 | 벤치-수정 관례(2026-07-05 6커밋 선례) — 커밋 전 cpp-review + 당일 HW 재검증 동승 |

## Current State

**Working**: main `88faf08` — 보드에 플래시됨·검증됨. 빌드 0-warning, host 13스위트 PASS, working tree clean(이 docs 커밋 제외).

**보드**: `88faf08` 코드, **풀배선 리그 + 이더넷(ETH_STATIC 192.168.1.199)**, RS-485 어댑터 미접속. 테스트 잔재 설정 유지(§2.3-a: model_type=multi(1), TIMEOVER=8, EN_ENERGY=0, OUT_POWER=56/ON_TIME=56, cal_val=1, freq_cal_val=0).

**Uncommitted Changes**: 없음(docs 커밋 후).

## Files to Know

| File | Why It Matters |
|------|----------------|
| `fw/src/app_lcd_input.c` | `app_lcd_fault_cleared()` 신설(경고→런 페이지 복귀, E-stop 보류+텍스트 재기록) + 복귀 가드 패밀리(`lcd_may_restore_run_page`/`run_page_for_mode`) |
| `fw/src/app_lcd.c` | `app_lcd_tick` fault 미러 — 0→nonzero=show_error / **nonzero→0=fault_cleared**(신규 엣지) |
| `fw/src/app_lcd_disp.c` | `app_lcd_show_error()` — **`lcd_status=LCD_WARNING` 스탬프**(신규; dgus_set_page와 쌍 규칙 주석) |
| `ref/samd20/main.c:4356-4370, 4605-4617, 4232-4233` | legacy 대조 지점(RESET 핸들러 페이지 복귀 / lcd_status 스탬프 쌍) |
| `docs/superpowers/research/2026-07-05-rs485-first-write.md` | HMI Task 8 세션 지참물(§6 재현 절차) |

## Code Context

**신규 복귀 함수** (`fw/src/app_lcd_input.c`):
```c
void app_lcd_fault_cleared(void)   /* app_lcd_tick 미러가 error_status nonzero→0 엣지에 호출 */
{
    lcd_app_state_t *state = app_lcd_state();
    if (state->lcd_status != LCD_WARNING) return;          /* 임의 페이지 납치 금지 */
    if (lcd_may_restore_run_page()) {                      /* estop==0 && error_status==0 */
        state->lcd_status = run_page_for_mode(state->sys_mode);
        dgus_set_page(state->lcd_status);
    } else if (app_estop_active() != 0u) {
        dgus_write_text(VP_ERROR_MSG, "E-STOP");           /* 경고 유지 — 원인 갱신 */
    }
}
```

**OVTIME 벤치 재현** (mbpoll 1-based, TCP .199): EN_ENERGY `-r 21`=1, TIMEOVER `-r 10`=1 → START `-r 28` 값 1 → ~1.0s 후 STATUS `-r 30`=8 → RESET `-r 26` 값 1 → STATUS=0+**화면 자동 복귀**. 원복: `-r 21`=0, `-r 10`=8.

## Resume Instructions

**HMI 세션 (최우선)**:
1. `cd ~/dev/work/gds_us_hmi` 로 **새 세션** → 그쪽 `HANDOFF.md` 읽기 (SP1 Task 8 = 실보드 E2E 5항목, tip `c196c73` 미머지).
2. 준비물: RS-485 어댑터 연결 + 보드를 LCD에서 **SERIAL/addr=1/9600/EVEN** 복원(현재 ETH_STATIC) + 이 repo research doc §6.
3. Task 8 PASS 시 머지 = SP1 종료. 첫-write 간헐 무효 재현 절차 병행.

**FW 전류 벤치 세션 (전류계 준비되면)**:
1. 플래시 불필요(보드=main 최신). 유휴 표시 0.00 확인 → RUN 유지 → 전류계 0.6A ↔ 표시 0.60A(~0.3s 정착).
2. 부하 변화 추종 체감(#3) — 노이즈 과하면 `app_reg.c` `d/2`→`d/4` 한 줄 폴백.
3. EN_ENERGY=ON 실전류 energy-exit(에너지-도달 정상정지) 확인.

## Warnings

1. **Modbus 직접런은 이 보드 설정(model_type=multi)에서 자동정지 없음** — 30s 안전 캡뿐. 테스트 후 반드시 STOP(`-r 29` 값 1). ceiling 회귀 테스트는 model_type=hand(0) 전제(LCD SETUP에서 변경 — 단 EMSW 배선 상태에서 model 전환 시 E-stop 유발 주의, rig 노트 R1).
2. **mbpoll**: 쓰기 값은 IP 뒤 / 부팅 직후·연속 트랜잭션 간헐 실패 → 재시도 / 주소 1-based(-r N = wire N-1), STATUS=30/START=28/STOP=29/RESET=26.
3. 빌드 시 vendor 헤더 경고 3건(wiznet socket.h)은 pre-existing — our-code 0-warning 게이트와 무관.
