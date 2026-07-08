# Handoff: 2026-07-08 세션 — 사용자 신규 3건 전부 코드-완료 (부팅 터치 유령 런 fix + REMOTE icon + 전류 EMA 100ms), HW 벤치 게이트

**Generated**: 2026-07-08 (세션 마감, 보드 미사용 코딩 세션)
**Branch**: `main` tip `78a1e43`(+이 docs 커밋) — **⚠ push 미실행** (사람 터미널: `git push origin main && git push origin hw-revA_fw-stage-mbtcp-hardening` — 태그는 이전 세션분이 아직 origin에 없음, main 커밋은 세션 시작 시점까지는 동기였음)
**Status**: 코드 3건 완료·리뷰 통과·커밋됨. **보드에 미플래시** — 다음 보드 세션에서 재플래시 + 벤치 체크리스트(§Resume) 실행.

> **요약**: 2026-07-06 등록된 사용자 신규 3건을 모두 코드-완료. ⑴ **부팅 딜레이 터치 유령 런**(`e26e15b`) — 근본원인 = V30 RUN 버튼 data=0 양엣지 quirk의 런-상태 매핑이 "조용히 거부되는 START"(부팅 워밍업 ~4s·seek/reset 체인·E-stop/overload/fault)마다 press/release 페어링을 반전시킴 → 물리 release가 START로 재매핑되어 쥐지 않은 런 시작 + 이후 탭마다 정지→재시작(사용자 관측 "터치 무시 + 계속 출력 + 전원사이클 필요"와 정확 일치). 수정 = **입력 레이어 물리 토글**(`s_run_key_down`) + SYS_PIC_NOW 재앵커, app_reg 무수정. ⑵ **REMOTE icon**(`60792da`) — samd20 DISP_REMOTE(case 9) 충실 포팅: 유효 Modbus 요청 흐르는 동안 ON, 마지막 요청 후 1s에 OFF. ⑶ **전류 표시 EMA**(`78a1e43`) — α 1/8→1/2(50ms 커밋 유지), τ≈400ms→≈100ms. 전건 cpp-review 통과(0 Crit/High).

## Goal

2026-07-06 세션 마감 시 등록된 사용자 신규 3건 처리:
1. 부팅 딜레이 시 터치 에러 (증상 상세: 초음파 계속 출력·터치 무시·전원사이클 필요)
2. 원격제어 진행 중 REMOTE icon 점등
3. 전류 표시 업데이트 반응 100ms

## Completed

- [x] **#1 `e26e15b` fix(lcd)**: data=0 매핑을 런-상태 기반 → 물리 토글로 교체. systematic-debugging으로 근본원인 코드-레벨 확정(아래 Code Context). cpp-review APPROVE-WITH-COMMENTS(0C/0H, LOW 주석 2건 반영).
- [x] **#2 `60792da` feat(lcd)**: REMOTE icon. `app_modbus_note_remote()`/`app_modbus_remote_active()`(1s hold, 랩-세이프) + RTU/TCP 디코드 성공 지점 스탬프 + LCD disp 엣지-쓰기(ICON_RUN 패턴). cpp-review APPROVE(0C/0H).
- [x] **#3 `78a1e43` feat(reg)**: ch1 표시 EMA α 1/8→1/2 (τ≈100ms; "업데이트 주기 100ms"를 응답시간으로 해석 — 사용자 통지됨). cpp-review APPROVE-WITH-COMMENTS(0C/0H, MEDIUM=에너지 커플링 벤치 재확인 이월).
- [x] 게이트: our-code 0-warning 빌드(FLASH 48.58%/RAM 19.36%) + host 13스위트 PASS (3건 공통).

## Not Yet Done

- [ ] **HW 벤치 검증 3건 전부** (§Resume 체크리스트) — 이 세션은 보드 미사용, **보드는 이전(mbtcp-hardening `434e007` tip) 코드 그대로**.
- [ ] (이월) **전류 표시 0.60A 재확인** — `54e5220` 전달함수 재정의 후 명시 벤치 확인 미기록. EMA가 빨라져 이제 정착 관측이 쉬움.
- [ ] (리뷰 MEDIUM) **energy 모드 energy-exit/OVTIME 타이밍 재확인** — EMA α 변경이 적분 입력의 스파이크 추종을 높임(구조 무변경, 실거동 확인 필요).
- [ ] HMI SP1 Task 8 실보드 E2E (gds_us_hmi 폴더, RS-485) + RS-485 첫-write 재현 병행.
- [ ] 6b 잔여 / B-SEAM 잔여 — ⏸ 사용자 보류 유지.
- [ ] 후속 소소(변경 없음): app_eth STATIC_UP 링크 재폴링 / KA 무송신-피어 / defer Minor(ledger) / handle_key_multi RESET OVLD 비트 휘발성(LOW).

## Failed Approaches (Don't Repeat These)

- **#1의 1차 설계 = reg 레이어 "거부 시 swallow 무장"(arm-on-reject)** — `app_reg_command`에서 TOUCH START가 거부될 때마다 `swallow_start=1`을 무장해 다음 mapped-START(실은 release)를 소비시키는 안. **결함**: overload/E-stop **force-stop 경로**(`app_overload.c:69/85`, `app_input.c:77`)에서는 버튼을 쥔 채 런이 강제 정지되고, 이때 도착하는 "거부되는 START"는 press가 아니라 **물리 release** — 이걸 press로 오인해 무장하면 페어링이 다시 반전됨(overload 해제 후 첫 press가 먹히고 그 release가 유령 런 시작). **reg 레이어에서는 거부된 START가 press인지 release인지 원리적으로 구분 불가.** → 입력 레이어 물리 토글로 전환(정보가 존재하는 곳에서 해결).
- (참고) 구 코드 주석의 "self-syncing (a dropped edge is corrected by the next press)" 주장은 **거부-press 케이스에 성립하지 않았음** — pre-D5엔 TOUCH ceiling+swallow가 우연히 재동기해 줬는데 D5의 "TOUCH 운영 ceiling 제외" 결정으로 그 우연한 복구 경로가 사라져 있었다.

## Key Decisions

| Decision | Rationale |
|----------|-----------|
| #1 물리 토글(입력 레이어) > 런-상태 매핑/reg 레이어 패치 | data=0은 물리 엣지당 정확히 1회(2026-06-08 HW 트레이스) → 토글이 물리 진실을 그대로 재구성, 모든 silent-reject 창구(워밍업/체인/E-stop/overload/fault/동일-드레인)에 불변. 유일한 드리프트 = 유실/중복 프레임(트레이스상 무발생) + SYS_PIC_NOW 재앵커로 봉쇄 |
| #1 swallow 메커니즘 존치(app_reg 무수정) | 토글 하에서 ceiling-stop 잔여 swallow는 기존 RELEASE-while-IDLE 재동기 분기(`app_reg.c:201-206`)가 자동 정리 — 제거는 범위 밖 |
| #2 REMOTE = 활동 기반 1s hold (모드/명령 기반 아님) | legacy samd20 충실(modbus_comm_cnt>100 ≈ 1s). "원격제어 진행중" = 마스터가 실제 폴링 중 |
| #3 "업데이트 주기 100ms" = 응답 τ≈100ms 해석(α=1/2) | 커밋 주기(50ms)를 100ms로 늘리면 오히려 느려짐 — 등록 당시 의도("반응 ×2")와 결합해 응답시간으로 해석. 벤치에서 과하게 튀면 α=1/4(τ≈200ms) 한 줄 폴백 |
| 3건 모두 main 직접 커밋(브랜치 없음) | 소규모 벤치-수정 관례(D0/D1·2026-07-05 벤치 6커밋 선례) — 커밋 전 cpp-review + HW 확인은 다음 보드 세션 동승 |

## Current State

**Working**: main `78a1e43` — 빌드 0-warning, host 13스위트 PASS, 리뷰 3건 통과. working tree clean(이 docs 커밋 제외).

**보드**: **이번 코드 미플래시**. 이전 상태 그대로 = mbtcp-hardening 머지 코드(`434e007` tip 동일), 풀배선 리그 + 이더넷(ETH_STATIC 192.168.1.199), OUT_POWER=56/ON_TIME=56, cal_val=1, 테스트 잔재 설정은 NEXT_STEPS §2.3-a.

**Uncommitted Changes**: 없음(docs 커밋 후).

## Files to Know

| File | Why It Matters |
|------|----------------|
| `fw/src/app_lcd_input.c` | #1 본체: `s_run_key_down` 토글(157행 부근 static + data=0 분기) + SYS_PIC_NOW 재앵커 |
| `fw/src/app_reg.c` | 워밍업 게이트(`:159`)·guard(`app_reg_start_allowed`)·swallow(`:99-109,161-169,201-206`) — #1이 의존하는 무수정 반경 / #3 EMA(`reg_acquire_step` ch1 분기 `d / 2`) |
| `fw/src/app_modbus.c` | #2 활동 상태 + `note_remote`/`remote_active` + RTU 스탬프(`mb_core_decode` 성공 시) |
| `fw/src/app_modbus_tcp.c` | #2 TCP 스탬프(`mb_tcp_build_response` 성공 시 — poll당 최대 4회, 무해) |
| `fw/src/app_lcd_disp.c` | #2 소비자: `DISP_REMOTE`(0x120e) 엣지-쓰기(ICON_RUN 패턴 바로 아래) |
| `ref/samd20/main.c:5187-5199` | #2 legacy 원본(case 9) — 포팅 충실도 대조용 |

## Code Context

**#1 토글 매핑** (`app_lcd_input.c`, data=0 분기):
```c
static uint8_t s_run_key_down;          /* 물리 RUN 키 상태; SYS_PIC_NOW에서 0 리셋 */
...
} else if (data16 == 0) {               /* V30: press/release 모두 data=0 */
    s_run_key_down ^= 1u;
    if (s_run_key_down != 0u) { US_CMD_START + set_pot } else { US_CMD_RUN_RELEASE }
}
```
거부된 press → 그 release는 RELEASE-while-IDLE no-op(+swallow 정리). 워밍업 중 press-in/release-out도 유령 런 없음 — 대신 "워밍업 중 시작한 hold는 워밍업 후에도 시작 안 됨"(다시 누르면 정상; M16 레벨-추종과의 의도적 편차, 안전 방향).

**#2 활동 접근자** (`app_modbus.h`):
```c
void app_modbus_note_remote(void);      /* 유효 요청 디코드 시 스탬프 (RTU/TCP) */
bool app_modbus_remote_active(void);    /* 마지막 요청 후 1s 유지 */
```

**#3 EMA** (`app_reg.c` reg_acquire_step): `s_ch1_filt_x16 += d / 2` (구 `d / 8`), 50ms 커밋 — 스텝 75%@100ms/97%@250ms.

## Resume Instructions

**FW 벤치 세션 (최우선)** — 플래시 후 순서대로:

1. `env -u STM32_TOOLCHAIN cmake --build fw/build` → `openocd -f fw/openocd/stm32f410.cfg -c "program fw/build/gds_us_ctrl.elf verify reset exit"` (⚠ 플래시↔ELF 대조 습관 유지)
2. **(이월) 전류 0.60A**: RUN 정착 + 전류계 0.6A ↔ 표시 0.60A, 유휴 0.00. EMA 빨라졌으므로 ~0.3s 내 정착 기대.
3. **#1 유령 런 소멸**: 전원 인가 → **4초 내** RUN 터치해 누른 채 → 워밍업 끝난 뒤 손 뗌 → **초음파 출력 없어야 함**(구 펌웨어는 여기서 유령 런). 이어서 정상 hold-to-run(누르면 켜지고 떼면 꺼짐) 확인. 추가: RESET 체인(1.2s) 중 press→체인 후 release도 무해 확인.
4. **#2 REMOTE icon**: mbpoll FC03 반복 폴링(RTU 또는 TCP) 중 icon ON → 폴링 중단 ~1s 후 OFF. ⚠ V30 에셋이 VP 0x120e를 실제 렌더하는지 이번이 첫 확인(Stage C 때 스킵) — 안 뜨면 FW가 아니라 에셋 매핑 의심(SWD로 dgus 전송 확인 가능).
5. **#3 반응 체감**: 부하 변화 시 표시가 ~0.1-0.3s 내 추종 + 노이즈 튐 허용 수준인지. 과하면 `app_reg.c` `d / 2`→`d / 4`(τ≈200ms) 폴백.
6. **(리뷰 MEDIUM) energy 타이밍**: EN_ENERGY=ON 직접런 energy-exit/OVTIME 발화 타이밍이 종전과 유의미하게 다르지 않은지.
7. 통과 시 태그 없음(스테이지 아님) — push만: `git push origin main && git push origin hw-revA_fw-stage-mbtcp-hardening` (사람 터미널).

**HMI 세션** → `~/dev/work/gds_us_hmi` + 그쪽 HANDOFF.md (RS-485; 이 repo `docs/superpowers/research/2026-07-05-rs485-first-write.md` §6 지참).

## Warnings

1. **빌드 시 vendor 헤더 경고 3건**(wiznet `socket.h` declared-static-never-defined)은 vendor-도메인 pre-existing — our-code 0-warning 게이트와 무관.
2. **mbpoll 주소 1-based**(-r N = wire N-1) 관행 유지. STATUS=reg 30, START=28, STOP=29.
3. 기존 승계: SWD halt 금지(비침습 샘플러) / bss 주소 빌드마다 이동 / LCD 터치+B_START 웨지→전원사이클 / stty 잔재.
4. `#1` 관련: 토글은 **중복 data=0 프레임**에도 드리프트(리뷰 LOW — HW 트레이스상 무발생) — 만약 벤치에서 "한 번 눌렀는데 START+RELEASE 연발" 관측되면 중복 프레임 의심.
