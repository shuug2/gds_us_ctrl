# Handoff: 원격 제어 활성화 게이트 T-1~T-4 CODE-COMPLETE (브랜치, T-5 = DGUS 자산 게이트)

**Generated**: 2026-08-15 (계획 Fable / 구현 Opus) · **재베이스 2026-08-16**
**Branch**: `feat/remote-enable-gate` — **base = `main`** (2026-08-16 ponytail 머지 완료 후 재베이스). 이전 base였던 `refactor/ponytail-cleanup`은 벤치 전항목 PASS로 main에 흡수됨.
**Status**: T-1~T-4 코드 완료·리뷰 반영됨. 남은 것 = **T-5(LCD 조작/표시) = DGUS 자산 대기** → T-6 리뷰 → T-7 벤치 → T-8 머지.

> **요약**: 컨트롤러 LCD에서만 켤 수 있는 비영속 원격 활성화 게이트의 배관 4단계를 구현했다. 순수 FSM(host 15번째 스위트, 12+1 케이스) + 레지스터 계약 `0x2A~0x2D` + `app_modbus.c` 글루(매 tick step / 미러 3종 / 접근자 3종) + `apply_writes` 선두 게이트(명령 소거 불변식 + STOP 상시 통과 + cfg 체인 skip). `/code-review high` 3건 전건 반영. **⚠ 게이트를 켤 수단(T-5)이 아직 없어 기본 빌드는 모든 원격 명령을 막는다 — 벤치는 반드시 `-DREMOTE_EN_GATE_BYPASS=ON`으로 빌드할 것.**

## ⚠ 이 브랜치를 플래시하기 전에 반드시 읽을 것

```sh
cmake -S fw -B build -G Ninja -DREMOTE_EN_GATE_BYPASS=ON   # 벤치 세션 필수
```

T-4까지만 있으면 `app_remote_en_set()`에 호출자가 없어 **게이트가 영구 DISABLED**다.
기본 빌드를 그대로 플래시하면 RTU·TCP 양쪽에서 원격 START/SEEK/RESET과 cfg 쓰기가
전부 무시되고 **보드에서 되살릴 방법이 없다** — 이 repo의 HW 검증이 전적으로
의존하는 mbpoll 흐름과 `gds_us_hmi` 레지스터 계약이 함께 죽는다.

우회 빌드는 CMake 경고 + 부팅 mon `*** REMOTE ENABLE GATE BYPASSED ***`를 낸다.
우회해도 FSM step과 미러 3종은 계속 돌므로 `0x2A~0x2C` 관측은 가능하다(무력화되는
것은 `apply_writes` 강제뿐).

## ✅ 상속된 게이트 — 해소됨 (2026-08-16)

이 브랜치는 `refactor/ponytail-cleanup` 위에 쌓여 있었고 그 브랜치의 HW 재검증이
미결이었다. **2026-08-16 벤치에서 전항목 PASS 후 main에 머지**됐고, 이 브랜치는
새 main 위로 재베이스됐다 — 계획된 순서(계획 §0.3, spec §13.6) 그대로 종결.
따라서 **spec §13.6의 "미검증 리팩토링 위 스택" 리스크는 소멸**했다.
ponytail 벤치 결과 상세 = main의 `HANDOFF.md` §①.

## Completed (전건 host 15스위트 PASS + 크로스 0-warning)

| 커밋 | Task | 내용 |
|---|---|---|
| `2ebba2a` | — | spec `docs/superpowers/specs/2026-08-15-remote-enable-gate-design.md` |
| `7a1db32` | — | plan `docs/superpowers/plans/2026-08-15-remote-enable-gate-t1-t4.md` |
| `767549d` | T-1 | `app_remote_en_fsm.{c,h}` 순수 FSM + host 15번째 스위트(12케이스) |
| `42cf3a2` | T-2 | 레지스터 `0x2A~0x2D` 계약 + cross-check 케이스 13 |
| `f0625ad` | T-3 | 글루 — tick step + `mirror_live()` 미러 3종 + 접근자 3종 |
| `aca7a97` | T-4 | `apply_writes` 선두 게이트 — 소거 불변식 + STOP 통과 |
| `735a9b2` | 리뷰 | LOW 2건 — STOP 값 불문 소거 / LCD 이벤트 상호배타 |
| `6c22931` | 리뷰 | HIGH 1건 — `REMOTE_EN_GATE_BYPASS` 한시 우회 |

FLASH 49.73% → **49.77%**(+48 B, bypass 빌드 49.79%), RAM 19.48% 무변동.
무변경 약속 파일(`app_reg.c`·`app_modbus_core.c`·`app_config.*`·`app_modbus_tcp.c`) diff 0.

## Not Yet Done (순서대로)

- [ ] **★ T-5 — DGUS 자산 작업(사용자) → LCD 조작/표시 글루**. 필요한 자산 3종:

  | 용도 | VP(제안) | 요구사항 |
  |---|---|---|
  | 게이트 버튼 | `0x1086` | **touch-down/up 양쪽 이벤트 송신 필수**(`SETUP_MODEL 0x1084`와 같은 컨트롤 종류). 없으면 롱프레스 판정 불가 |
  | 상태 아이콘 | `0x1155` | `ICON_RESET~OUTERR`(`0x1150~54`) 다음 빈 자리 |
  | 잔여 초 | `0x1211` | u16, `LV_RUN_MODE(0x1210)`↔`LV_ENERGY_VAL(0x1212)` 사이 |

  ⚠ **자산 반입 후 `LCD_TRACE_RX` 실물 트레이스 선행 — 추론 구현 금지**(RUN 키 `KEY_MULTI=0`·`0x120e` 렌더 전례, spec §8.3).
  소비할 접근자는 T-3에서 확정됨(`app_remote_en_set`/`_state`/`_left_s`) — T-3 재작업 불필요.
- [ ] **T-5 머지 시 `REMOTE_EN_GATE_BYPASS` 제거** — CMake 옵션 + `app_modbus.c`의 `#ifndef` 양쪽.
- [ ] **T-6** 통합 리뷰 + `docs/requirements.md`·`changelog.md` 갱신.
- [ ] **T-7** 벤치 VR-1~VR-13 (spec §11). 최우선 = **VR-10 probe**(구펌웨어 왕복), 그다음 VR-3 stale-latch.
- [ ] **T-8** `--no-ff` 머지 + 태그 `hw-revA_fw-stage-remote-gate` + 원격기 측 실측 통지.
- [ ] **F-A(comm/eth `0x1E~0x29`)** = 별도 스테이지, 이 브랜치 범위 밖(spec §2.2).

## Key Decisions

| Decision | Rationale |
|----------|-----------|
| 게이트 seam = `app_modbus_apply_writes()` 선두 1곳 | RTU(`app_modbus.c:361`)·TCP(`app_modbus_tcp.c:172`)가 이 함수를 공유. `app_reg`에 넣으면 같은 `start_allowed()`를 쓰는 US_TOUCH/US_REMOTE/US_CYCLE 로컬 조작까지 막힌다 → `app_reg.c` 무변경 |
| 명령 4종 "디스패치는 조건부, 소거는 무조건" | `0x19~0x1C`는 미러 대상이 아니라 잔류값이 안 지워진다. START 잔류 → 게이트 개방 후 첫 FC06에 유령 발화 / STOP 잔류 → FC03이 유령 pending 보고 |
| 침묵 타이머는 활성화 후 첫 유효 요청부터 무장 | 활성화 이전 스탬프로 무장하면 원격기 접속 전에 DIS_LINK. 판정은 랩 안전(이전 요청은 좌변이 언더플로로 elapsed 초과) — spec 결정 기록에 없던 보완 |
| E-STOP = 레벨 아닌 상승 엣지 + 레벨 중 enable 거부 | 레벨 추종이면 해제 시 자동 부활해 "재활성은 LCD만"과 모순. 엣지만 쓰면 이미 활성인 상태에서 켤 때 구멍 |
| enable 이벤트를 step 마지막에 평가 | 재조작 = 창 갱신 + 침묵 재-미무장 + 사유 래치 해제가 한 경로로 통합. 상호배타는 글루가 보장(리뷰 LOW-3) |
| 상태는 파일 static (holding[]·FRAM 불가) | `mb_core_init`이 링크 전이마다 holding을 0으로 리셋(`:310`/`:373`), FRAM은 비영속 요구 위반 |
| probe 매직 `0x5201`, write-back 값 P는 비영값 | P=0이면 링크 전이 0-리셋과 구분 불가 — 결정 기록의 예시 `0x0000`은 채택 안 함, **원격기 spec 역반영 필요** |
| BYPASS 폴라리티 = 기본 OFF(게이트 유효) | 플래그를 깜빡하면 "제한적"으로 실패해야 한다. 반대 폴라리티는 잊었을 때 게이트가 있는 척하며 없는 상태가 됨 |

## Files to Know

| File | Why It Matters |
|------|----------------|
| `fw/src/app_remote_en_fsm.c` + `fw/include/app_remote_en_fsm.h` (신규) | 순수 FSM. step 평가 순서가 곧 규약(엣지→해제 조건→enable→출력) |
| `fw/test/test_app_remote_en_fsm.c` (신규) | 13케이스. `test_left_arithmetic_boundary_wrap`이 절대 시각 비교 오구현을 잡는 핵심 |
| `fw/src/app_modbus.c:208` 부근 | 게이트 블록(`#ifndef REMOTE_EN_GATE_BYPASS`), 기존 else-if 체인 시작은 `:219` |
| `fw/src/app_modbus.c` `mirror_live()` 말미 | 미러 3종 — 조건 없이 함수 말미여야 세 호출처를 전부 덮는다 |
| `fw/CMakeLists.txt` `option(REMOTE_EN_GATE_BYPASS ...)` | T-5 머지 시 제거 대상 |
| `docs/superpowers/specs/2026-08-15-remote-enable-gate-design.md` | 설계 정본. §10 하단에 중간 상태(BYPASS) 절 |
| `~/dev/work/gds_us_remote/docs/superpowers/specs/2026-08-02-remote-enable-safety.md` | 원격기 측 설계 정본 |

## Warnings

1. **기본 빌드 플래시 금지** — 위 §"플래시하기 전에" 참조. 벤치는 `-DREMOTE_EN_GATE_BYPASS=ON`.
2. **T-5 없이 main 머지 금지** — 켤 수 없는 게이트는 제품을 죽인다(spec §10 하단).
3. **CMake GLOB** — 신규 `.c`(`app_remote_en_fsm.c`)가 있으므로 브랜치 전환 후 `./fw.sh` 경유 필수(증분 빌드만 하면 undefined ref).
4. **이 게이트는 안전 등급 기능이 아니다** — Modbus 무인증이라 활성 창 안에서는 그 버스의 어떤 마스터든 기동시킬 수 있고(VR-12로 실증 예정), 원격 STOP은 비상정지가 아니다. 물리 인터록의 대체재가 아님(spec §13).
5. **SWD halt 금지** — T-7 벤치는 mbpoll + LCD 육안만.
