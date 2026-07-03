# Handoff: 감사 큐 D0~D6 전부 종결 (D3 fram-robust + D6 eth-reapply 머지) — 다음 = D5 reconcile

**Generated**: 2026-07-04 (c 세션 마감)
**Branch**: `main` (`d9fb326`, **origin 동기 = push 완료**)
**Status**: Ready — 2026-07-02 감사 결정 큐 **D0~D6 전부 종결**. 다음 = **D5(미머지 reconcile b→d→ch1, 코딩 세션)**

> **요약**: 2026-07-04 하루 3세션으로 감사 큐 잔여분을 전부 닫음.
> ① **D3 'fram-i2c-robustness'** HW 회귀 PASS → 머지 `be2fac9` + tag `hw-revA_fw-stage-fram-robust`.
> ② **D6 'eth-reapply(M7)'** brainstorming→spec→plan→subagent-driven 4 Task→최종 opus 리뷰(0 Crit/0 Imp)→**HW E2E 6항목 PASS**(인터랙티브: 사용자 LCD + 네트워크/RTU 검증) → 머지 `6467d67` + tag `hw-revA_fw-stage-eth-reapply`.
> 모든 게이트 GREEN: our-code 0-warning + host 7스위트 PASS. 남은 백로그 = D5(reconcile) + HW-gated(weld slice4/B-SEAM/6b/overload) + HMI-트리거(M6/M8/M9, H4+IWDG).

## Goal

STM32F410RBT 단일 MCU 통합 펌웨어의 2026-07-02 전면 감사 수정 큐(D0~D6) 실행 완료. 이번 세션 몫 = D3 HW 회귀→머지, D6 설계→구현→HW E2E→머지.

## Completed

- [x] **D3 HW 회귀 + 머지** (`be2fac9`, tag `hw-revA_fw-stage-fram-robust`): 부팅 FRAM 저장값 유지(폴백 미발동)+LCD 육안 / ceiling 무회귀 / FC06 write→리셋→리로드. mon `[cfg]` 캡처는 사용자 결정으로 생략(RS-485 DE 미제어, ①③이 FRAM 로드 입증).
- [x] **D6 eth-reapply(M7) 전체 사이클** (`6467d67`, tag `hw-revA_fw-stage-eth-reapply`): LCD DATA_SAVE의 ether/comm_mode 변경을 재부팅 없이 W5500에 반영(samd20 main.c:3327-3403 거동 복원). 코드 4커밋+리뷰반영 1커밋(bc46671→625e651).
- [x] **M7 HW E2E 6항목 PASS**: mode-only SERIAL→ETH(G4 실증) / IP .70→.199 즉시 반영(본체) / STATIC→DHCP(리스+LCD 표시, F2) / DHCP→STATIC(직접 입력) / ETH→SERIAL 복귀 / ceiling 무회귀(1×4→0≈560ms).
- [x] docs: changelog·NEXT_STEPS(§1.3 D3/D6 ✅, §2.2 큐)·RESUME(3개 세션 항목)·메모리(`project-audit-2026-07`)·SDD ledger.

## Not Yet Done

- [ ] **D5 reconcile (다음 작업, 코딩 세션)**: 미머지 3단위를 현 main(`d9fb326`)에 rebase — 순서 **b→d→ch1**:
  1. `feat/physical-io-slice-b` (FREQ_IN 측정 — 독립·최고령, 방치위험 1순위)
  2. `feat/physical-io-slice-d` (물리 명령 입력+E-stop — a⊂c⊂d superset; **board.c OSC OD를 main이 먼저 보유 → reconcile 필요**)
  3. `feat/output-power-graph-ch1` (표시값 ch1 분리)
  - **`app_reg_tick` 3-way 시그니처 semantic 통합 필수**: main=ovtime `reg_run_limits_t`(기준) / slice-d=+model_type / ch1=+cal_val
  - 빌드+host PASS까지가 코딩 세션 몫. **머지/태그는 HW 검증 후**(기존 정책).
- [ ] 나머지 백로그: D2/D4(weld slice4 일괄), H4+IWDG 별도 슬라이스, HW-gated(B-SEAM/6b/overload), M6/M8/M9(HMI 착수 시) — `docs/NEXT_STEPS.md` §1.2/§1.3.

## Failed Approaches (Don't Repeat These)

코드 차원 실패 없음(전 Task 1회 통과). **벤치 함정 3건** (M7 E2E 중 규명, 시간 소모 큼):

1. **M7 플래시 직후 RTU 무응답 → 펌웨어 회귀로 오인**. 실제 = 직전 D3 세션에서 사용자 LCD 확인 중 **addr=NONE이 FRAM에 저장**돼 있었음. 규명 경로: main 재플래시 이분 실험(main도 무응답=펌웨어 무죄) → SWD 정적 read로 `g_cfg.comm_address=0` 확정. **시그니처**: ping 응답(+SERIAL에서도 eth가 static netinfo 적용) + TCP :502 닫힘 + RTU 무응답 = "SERIAL+addr=NONE".
2. **빈 IP 스캔을 믿고 .71/.74 사용 → 타 기기 선점으로 판정 불가**. 시험 네트워크(192.168.1.x)가 붐빔 — ping 스캔 직후에도 기기가 나타남. **항상 ARP MAC 대조로 보드(00:08:dc:78:91:71) 여부 확인**. .199처럼 높은 대역이 안전.
3. **LCD IP 키패드 오입력**: "74" 입력이 "4"로 커밋됨(7 미등록). 저장 전 LCD 화면 값 확인 필수. RAM 검증 = SWD 정적 read `g_cfg+46..49`.

절차 노트: **Task 3 구현자(haiku)가 존재하지 않는 테스트 스위트명을 리포트에 기재**(허위 증거) → 컨트롤러 독립 재검증으로 해소. **haiku 구현자 리포트의 검증 섹션은 신뢰 금지, 직접 재검증할 것.**

## Key Decisions

| Decision | Rationale |
|----------|-----------|
| M7 = A안(dirty-flag + `app_eth_tick` 재적용) | hook 직접 호출은 app_lcd↔app_eth 사이클(M1 위반), 매-tick netinfo 비교는 DHCP 리스 RAM-미러가 자기-트리거 → 무한 재적용 위험 |
| `app_modbus_tcp_reset()`(sock0 강제 close) 필수 | 단일소켓 서버 — IP 변경 후 stale ESTABLISHED가 남으면 listen 소켓 부재로 새 IP 접속 영구 차단(F1) |
| `eth_apply_on_link` DHCP 분기 `s_available=false` 명시 | 부팅 경로에선 무해했지만 재적용(STATIC_UP→DHCP)에선 true 잔존 버그(F2) — 최종 리뷰가 "load-bearing fix"로 확인 |
| mode-only 변경도 ether hook 발화(G4) | 기존 커밋 로직은 ether 필드 변경시에만 발화 → STATIC↔DHCP 전환이 트리거 없음 |
| D3 mon `[cfg]` 캡처 생략 | RS-485 DE 미제어로 mon 수신 불가 + 다른 3항목이 FRAM 로드 정상을 입증(사용자 결정) |

## Current State

**Working**: main = 감사 큐 D0~D6 전부 반영. LCD 저장으로 IP/모드가 가동 중 즉시 전환됨(HW 실증). origin 동기(push 완료).

**Board**: M7 머지 코드 적재(플래시=브랜치 tip, 코드 동일). **SERIAL/addr=1/9600/EVEN, OUT_POWER=56, FRAM ether_ip=192.168.1.199**(이전 .70에서 변경 — ETH 시험 시 참고).

**Uncommitted**: 없음 (untracked `ref/signal/`은 이 세션과 무관, 그대로 둠).

## Files to Know

| File | Why It Matters |
|------|----------------|
| `fw/src/app_eth.c` | `eth_reapply()` phase별 재적용 + F2. 4-phase 머신(DOWN/LINKWAIT/STATIC_UP/DHCP_RUN) |
| `fw/src/app_lcd_input.c` | `commit_comm_mode_and_ether()` — hook 발화 조건 `ether_changed\|\|mode_changed` |
| `fw/src/app_lcd.c` / `fw/include/app_lcd.h` | `app_lcd_ether_dirty_take()` consume-and-clear (M1 discipline 패턴) |
| `fw/src/app_modbus_tcp.c` | `app_modbus_tcp_reset()` — sock0 close, FSM이 2 poll 내 재-listen |
| `docs/superpowers/specs/2026-07-04-eth-reapply-m7-design.md` | M7 설계 정본 (§6 HW E2E 체크리스트 포함) |
| `.superpowers/sdd/progress.md` | SDD ledger — Task별 커밋/리뷰/Minor defer 목록 |

## Code Context

M7 데이터 흐름 (superloop 순서가 안전성의 근거 — LCD dispatch → `app_eth_tick` → `app_modbus_tick`):

```c
/* app_lcd.h — LCD 커밋 후 첫 호출만 true */
bool app_lcd_ether_dirty_take(void);

/* app_eth.c: app_eth_tick() 진입부 */
if (app_lcd_ether_dirty_take()) { eth_reapply(); }
/* eth_reapply: STATIC_UP → tcp_reset+re-apply / DHCP_RUN(모드유지) → 리스 보존
 *             DHCP_RUN(이탈) → DHCP_stop+tcp_reset+re-apply / DOWN·LINKWAIT → no-op */

/* app_modbus_tcp.h */
void app_modbus_tcp_reset(void);   /* close(sock0) — 다음 2 poll 내 재-listen */
```

D5 reconcile 대상 시그니처 충돌 (semantic 통합 필요):

```c
/* main(기준, ovtime): */ void app_reg_tick(const reg_run_limits_t *limits);
/* slice-d:            */ +model_type 주입   /* ch1: */ +cal_val 주입
```

## Resume Instructions

1. sanity: `git log --oneline -3`(main `d9fb326`) + `make -C fw/test`(7스위트 PASS) + `git tag -l 'hw-revA*'`(`-fram-robust`/`-eth-reapply` 확인).
2. **D5 착수(코딩 세션, 보드 불필요)**: `docs/NEXT_STEPS.md` §1.3 D5 행 + 메모리 `project-physical-io-layer`/`project-osc-boot-init`/`project-output-power-graph-ch1` 숙지 → brainstorming부터(reconcile 전략: rebase vs merge, 충돌 단위 실측 후 plan).
   - 순서 b→d→ch1. slice-d rebase 시 **board.c OSC OD 중복**(main이 선보유) reconcile.
   - 완료 기준 = 3단위 모두 현 main 위에서 빌드+host PASS. 머지/태그는 이후 HW 세션.
3. 브랜치 전환 시 **cmake reconfigure 필수**: `env -u STM32_TOOLCHAIN cmake -S fw -B fw/build -G Ninja` (GLOB 규칙 — slice 브랜치들은 신규 소스 파일 보유).
4. HW 세션이면: 검증 규칙 = mbpoll/LCD만, SWD halt 금지(정적 1회 read 예외), serial stty 9600 8E1 리셋 습관.

## Warnings

- ⚠ **보드 FRAM ether_ip=.199로 변경됨** — ETH 시험 시 .70이 아님. DHCP 리스는 .70으로 나옴(서버 기억).
- ⚠ **시험 네트워크 IP 충돌 잦음** — 보드 판별은 반드시 ARP MAC(00:08:dc:78:91:71).
- ⚠ **부분실패 잔여 리스크(defer)**: 부팅 시 일부 FRAM read 실패 → 기본값 동작 중 LCD 저장하면 기본값이 FRAM에 굳음(D3 의도된 범위, write-hardening 후속 후보).
- ⚠ M7 defer Minor 2건: sock0 재-listen은 실제 2 poll(주석 "다음 poll"은 표현 과잉) / `[eth] reapply` 로그는 no-op 경로에서도 출력(phase 필드로 구분 가능).
- ⚠ vendor wiznet `socket.h` 경고 3건 = pre-existing(full rebuild에서만 노출), our-code 0-warning 판정과 무관.
- ⚠ 미착수 감사 항목(D2/D4/H4/M6/M8/M9)의 파일:라인 근거 = `docs/NEXT_STEPS.md` §1.3 + 메모리 `project-audit-2026-07` (+git: `git show 49edfb8:HANDOFF.md`).
