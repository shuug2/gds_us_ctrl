# 원격 hold-to-run 워치독 — 컨트롤러 설계서 (확정 2026-09-06)

> **문서 요약**: 원격기 요구사항(`2026-09-05-remote-hold-to-run-requirements.md`)에 대한
> 이 저장소 쪽 설계. **새 레지스터 1칸(`0x32 FEAT_CAP` 비트맵, bit0 = hold 워치독)** 과
> **기존 START(0x1B) 의 값 확장(1=탭 · 2=hold 시작 · 3=유지 신호)** 으로 R-1/2/3/4/6/11 을
> 전부 충족한다. 워치독은 **순수 FSM `app_hold_wdt_fsm`(host 테스트) + `app_modbus.c` 글루**
> 로 두고, `app_reg.c` 는 **읽기 전용 접근자 4줄만** 추가한다 — hold 런은 새 소스값이 아니라
> `US_COMM` 그대로이므로 30초 상한·energy·on-time 세 열거(`app_reg.c:410-411, 433-434, 451-452`)
> 가 **무변경**으로 hold 런에 걸린다(R-6 구조적 충족). 워치독 T = **600 ms**(P=150 ms 권고).
> 구 펌웨어 fail-safe = `holding[START] == 1u` 만 디스패치하는 현행 체인(`e5ea557` `app_modbus.c:336`)
> 이 그대로 안전선. `2026-08-02` §3.1 은 한 글자도 안 고친다.
> 레지스터 사용 = 1칸(50→51), **남는 여유 6칸**.
>
> **사용자 결정(2026-09-06, §0)**: 착수 = **PC8 실장 앞** · STD 빌드 **포함**(`#if` 0개) · 패널 TOUCH
> lost-release **범위 밖**(독립 슬라이스) · R-5 **이연**. 그리고 **PC8 의 의미 정정** — 운전 중 껐다
> 켜는 인터록이 아니라 **"원격을 허용할 것인가"를 정하는 초기 설정 스위치**다. 그래서 R-11(게이트
> 닫힘 시 hold 런 정지)은 필수 요구가 아니라 코드 0 으로 얻는 **부수 효과**로 격하하고, 벤치 H-11 은 이연한다.

작성 2026-09-06 · 대상 main `9feb37d`(요구사항 접수 `ed4e218` + 정정 `4663e4c`/`9feb37d`; 코드는 `a048154` 와
동일 — 이후 커밋은 docs 뿐) · 인용한 `파일:라인` 은 전부 직접 읽고 확인한 것.
입력 명세 = `2026-09-05-remote-hold-to-run-requirements.md`. 이 문서가 **펌웨어 구현 정본**이다.

---

## 0. 확정 결정 (2026-09-06 사용자)

| # | 결정 | 귀결 |
|---|---|---|
| D | **PC8 실장 앞에 착수한다** | 코드·host 테스트·TCP 벤치 전부 지금 진행. PC8 재시험(A-1/A-5/A-13)과는 독립 |
| A | **STD 빌드 포함**, `#if` 0개 | STD 는 PC8 게이트조차 없어 원격 START 에 아무 인터록도 없는 모델 — 워치독 가치가 오히려 크다. 원격기 근거 추가: 화면이 STD/REMOTE 를 구분하지 않으므로 STD 만 빼면 **같은 START 버튼이 유닛에 따라 hold/탭으로 갈리는 혼재를 한 현장 안에** 만든다 |
| B | **패널 TOUCH lost-release 는 범위 밖** | 독립 슬라이스(§9-B). 원격 경로가 패널보다 안전해지는 비대칭은 **알고 남긴다** |
| C | **R-5 정지 사유 이연** | 원격기가 차폐 검출을 자기 쪽(`PRESSING` 증거 타이머 ~100 ms)에 두기로 했다(그쪽 S2). 비트맵이라 필요 시 bit1 + 1칸으로 무파손 추가 |

**PC8 의 의미 (사용자 정정)**: PC8 은 **다이나믹하게 껐다 켜는 스위치가 아니다.** 설치 시 "이 유닛에 원격을
허용할 것인가" 를 정하는 **초기 설정 스위치**다. 귀결 셋:

1. **R-11 격하** — "hold 런 도중 스위치 OFF" 는 운용 시나리오가 아니다. 코드는 그대로(게이트 닫힘 분기의 START 값
   불문 소거가 유지 신호를 굶겨 ≤T 트립) 두되, **필수 요구 → 부수 효과**로 적는다. 벤치 **H-11(HV-12) 이연**.
2. **D 의 논리** — "hold-to-run 이 인터록 부재를 가린다" 는 우려는 PC8 이 운전 중 차단층일 때 성립한다. 설정
   스위치면 가릴 층이 없다. 정상 경로만 보면 hold(단절 시 0.6 s)는 현행 탭(30 s)보다 **안전 방향**이다.
3. **용어** — `2026-08-15` 설계·HANDOFF·CLAUDE.md 가 PC8 을 "물리 인터록" 이라 부르는 것은 **과장**이다.
   배포 금지 사유(단선 = "허용" 극성)는 설정 스위치라도 유효하므로 그대로 두고, 개명은 **별건**으로 남긴다.


---

## 1. 한 줄 요약 + R 대응표

**"hold 시작(START=2)으로 시작된 `US_COMM` 런은 유지 신호(START=3)가 T=600 ms 안에 안 오면
글루가 STOP 과 동일한 `RUN_RELEASE(US_COMM)` 를 발행한다. 탭 START(=1)는 손대지 않는다."**

| 요구 | 충족 방법 | 상태 |
|---|---|---|
| R-4 판별자 | `0x32 FEAT_CAP` 비트맵 R 미러(모델 무관 무조건), bit0 = `MB_FEAT_HOLD_WDT`. 판정 = `0x31==0xFA01` + `0x32` 읽기 응답 + bit0 (§2.3) | ✓ |
| R-1 워치독 | `hold_wdt_step()` 트립 → `app_reg_command(US_CMD_RUN_RELEASE, US_COMM)` = STOP 분기(`app_modbus.c:375`)와 동일 호출 → 피크 래치·IDLE·STATUS bit0=0 (`app_reg.c:238-243`) | ✓ |
| R-2 런 구분 | START **값** 으로 구분: 1=탭(기존) / 2=hold 시작. 세션 무장은 **START=2 가 수락된 그 분기에서만** (§4) | ✓ |
| R-3 유지 신호 | START=3. ① 폴링과 별개 FC06 ② armed 아니면 no-op(기동 권한 없음) ③ 명령 레지스터라 consume-and-clear, cfg/FRAM 무접촉 ④ 1-FC06/poll 이월은 T 안 흡수 ⑤ 원격기 부담 = FC06 1건/150 ms | ✓ |
| R-6 기존 정지 경로 승계 | hold 런의 `us_run_status` 는 `US_COMM` 그대로 → 30초 상한(`:410-411`)·energy(`:433-434`)·HAND on-time(`:451-452`)·E-STOP(`app_input.c:72-81`)·STOP·패널 release(대칭 정지) 전부 **코드 무변경으로** 적용 | ✓ |
| R-11 게이트 닫힘 | 유지 신호는 게이트 대상 — 닫힘 분기가 START 를 값 불문 소거(`app_modbus.c:326-328`)하므로 유지 신호가 굶어 ≤T 에 트립. 추가 코드 0. **§0: PC8 은 설정 스위치라 운용 시나리오 아님 → 부수 효과** | 부수 |
| R-5 정지 사유 | **이연** (§9-C). 비트맵이라 나중에 bit 하나로 붙일 수 있다 | 이연 |
| R-7/8/9 불필요 | 동의 — hold 런 = 직접 런(`US_COMM` 동형). weld 경로(`US_CYCLE`) 미사용 | 동의 |
| R-10 ① pot write | START=2 수락 시 1회만(탭과 동형). START=3 은 pot 무접촉 | ✓ |
| R-10 ② 소거 비대칭 | START 분기를 "값 불문 소거 · 값별 디스패치"로 바꿔 대칭화(CFG_CTRL `:378-382` 와 동형) | ✓ |
| R-10 ④ 명명 | `MB_REMOTE_HOLD_MS`(아이콘 1초)와 충돌 회피 → 모듈 `app_hold_wdt_fsm`, 상수 `HOLD_WDT_MS` | ✓ |
| R-10 ⑤ LCD TOUCH | §9-B — 범위 밖 결정(§0) | 범위 밖 |

---

## 2. 레지스터 할당안

### 2.1 신설 · 변경

| 주소 | 이름 | R/W | 의미 | 미러 |
|---|---|---|---|---|
| **0x32** | `MB_REG_FEAT_CAP` | R | 기능 비트맵. `bit0 = MB_FEAT_HOLD_WDT(0x0001)` = hold 워치독 있음. 나머지 비트 0 = 미지원(미래 기능은 비트만 추가) | **모델 무관 무조건** — `mirror_live()` 의 `MB_REG_CFG_CAP` 줄(`app_modbus.c:254`) 바로 옆. 링크 전이 `mb_core_init` 0-리셋도 같은 tick 복원(`:263-267` 선례) |
| 0x1B | `MB_REG_START` (기존) | W cmd | **값 확장**: `1 = MB_START_TAP`(기존 탭, 무변경) / `2 = MB_START_HOLD`(hold 시작 + 워치독 무장) / `3 = MB_START_KEEP`(유지 신호, 기동 권한 없음) / 그 외 = 무시. **값 불문 소거** | 미러 ✗ (기존과 같음) |

`MB_REG_COUNT` **50 → 51**. FC03 전맵 응답 = 3 + 102 + 2 = **107 B** ≤ `MB_RESP_MAX` 125 (`app_modbus_core.h:20`). TCP = 6 + 105 = 111 B ≤ `MB_TCP_RESP_MAX`(129, `app_modbus_tcp_frame.h:12`).

**사용 칸수 = 1. 남는 여유 = 57 − 51 = 6칸.** (유지 신호를 별도 레지스터로 두는 안은 2칸 소비 — 기각 이유 §2.4)

### 2.2 왜 비트맵인가 (원격기의 ⓐⓑⓒ 대비, 한 문단)

ⓐ `0x31` 매직 승격은 **전순서(version) 로 기능 집합을 표현**하는 것이라, "기능 X 는 있고 Y 는 없다"(예: 모델 티어별 컴파일 아웃, 벤치 -D 로 끈 기능)를 말할 수 없고, 기능이 하나 늘 때마다 매직이 바뀌어 **정확 일치로 판정하는 소비자 전부**(원격기 `snapshot.c:122 == 0xFA01`, `gds_us_hmi`)가 그때마다 깨진다. ⓑ STATUS 예비 비트는 **매 프레임 비교되는 상태 워드**에 절대 안 변하는 값을 섞는 것이라 "비트 변화 = 사건"으로 읽는 소비자 로직을 오염시키고, 남은 7비트를 상태가 아닌 것에 소모한다. ⓒ `0x2C` 는 구 의미("잔여 초")를 아는 소비자가 남아 있을 수 있고, REMOTE 전용 미러 블록(`app_modbus.c:273-279`) 안에 있어 어차피 모델 무관 미러로 옮겨야 하며, 값 레지스터라 결국 ⓐ 와 같은 "기능당 매직" 문제로 돌아간다. **비트맵**은 1칸에 16개 독립 기능, 소비자는 "모르는 비트는 무시"만 지키면 재학습이 없고, 구/신 펌웨어 구분은 계속 `0x31` 이 맡는다. 대가는 1칸 + §2.3 의 프로브 1회.

### 2.3 소비 측 판정 절차 (원격기에 전달할 것)

`mb_read_regs` 는 범위 밖 FC03 에 **무응답**(`app_modbus_core.c:54-56`)이므로 `0x32` 가 없는 펌웨어에 51칸을 읽으면 폴링 자체가 죽는다. 따라서:

1. FC03 `0x00×50` — 모든 펌웨어에서 동작(폴링 기본).
2. `holding[0x31] != 0xFA01` → F-A 이전 구 펌웨어 → hold 미지원.
3. `== 0xFA01` → **연결당 1회** FC03 `0x32×1` 프로브:
   - 응답 있음 → 비트맵 획득. `bit0=1` → hold 지원 / `bit0=0` → **확정적 미지원**(신 펌웨어이나 기능 없음 — 예: STD 빌드에서 뺀 경우 §9-A).
   - 무응답(타임아웃 1회) → 비트맵 이전 신 펌웨어(현 main `66a2411` 포함) → 미지원.
4. 지원 시 폴 블록을 51칸으로 넓힌다(원자성 유지).

프로브 타임아웃은 비트맵 방식의 고유 비용이다(새 레지스터는 어떤 방식이든 이 비용을 낸다). `0x31` 하위 바이트를 "프레임 폭" 신호로 올리면 없앨 수 있으나 §2.2 의 정확-일치 소비자 파손 때문에 **채택하지 않는다**(§9-D).

### 2.4 유지 신호를 별도 레지스터가 아니라 START 값으로 두는 이유

- 레지스터 1칸 절약(여유 7→6 대신 7→5).
- **구 펌웨어 fail-safe 가 공짜**: 현행·구 체인은 `== 1u` 만 디스패치(§6). 새 레지스터(예: `0x33`)는 구 펌웨어에서 FC06 자체가 범위 밖 무응답(`app_modbus_core.c:84-85`)이라 역시 안전하지만, hold **시작** 만큼은 어차피 START 에 얹어야 "탭 START 와 같은 가드·pot write·게이트 경로"를 그대로 타므로, 유지 신호만 따로 빼면 표면이 둘로 갈린다.
- else-if 체인 3번째 분기(`app_modbus.c:363`)에서 잡혀 **cfg 체인·staged 스캔·FRAM 에 절대 닿지 않는다**(HV-16).
- 반대 논거(의미 과적재)는 값 3개짜리 enum 으로 헤더에 고정하면 충분히 읽힌다.

---

## 3. 변경점 전수표

| # | 파일:라인 | 무엇을 | 왜 | 거동 변화 |
|---|---|---|---|---|
| 1 | **신규** `fw/include/app_hold_wdt_fsm.h` | 순수 FSM API + `HOLD_WDT_MS`(`#ifndef` 가드, 벤치 -D 덮어쓰기 = `REMOTE_EN_LINK_SILENCE_S` `app_remote_en_fsm.h:21-23` 선례) | 판정 로직을 HAL-free 로 고정 | — |
| 2 | **신규** `fw/src/app_hold_wdt_fsm.c` (~40줄) | `hold_wdt_t{armed,last_keep_ms}` + `init/arm/keep/step/armed`. 컨텍스트 구조체 전달(`cfg_stage_t` `app_modbus.c:57` 선례) — §9-B 에서 TOUCH 재사용 가능하게 | 순수 모듈 | — |
| 3 | **신규** `fw/test/test_app_hold_wdt_fsm.c` + `fw/test/Makefile` (`:25-27` BIN 추가, `:48-49` 패턴 규칙, `:97` clean) | host 스위트 17개째 | 관례 | — |
| 4 | `fw/include/app_modbus_core.h:9` | `MB_REG_COUNT 50u → 51u`; `:10-17` 주석 "현재 50칸/여유 7칸" → 51/6 | 새 칸 | FC03 51칸 읽기 허용, 52칸 무응답 |
| 5 | `app_modbus_core.h:58` 부근 | `MB_START_TAP 1u / MB_START_HOLD 2u / MB_START_KEEP 3u` 정의 + 주석(기동 권한은 1·2 만) | 계약 고정 | — |
| 6 | `app_modbus_core.h:120-121` 뒤 | `MB_REG_FEAT_CAP 0x32u`, `MB_FEAT_HOLD_WDT 0x0001u` + §2.3 판정 절차 주석 | 계약 | — |
| 7 | `app_modbus_core.h:166-169` | "상태 기반 hold 가 필요해지면 그때 capability 를 신설한다" → `0x32` 로 신설했음을 가리키게 | 낡은 주석 | — |
| 8 | `fw/src/app_modbus.c:31` 부근 | `#include "app_hold_wdt_fsm.h"`; `static hold_wdt_t s_hwd;` (`s_stg` `:57` 옆) | 글루 상태 | — |
| 9 | `app_modbus.c:254` 옆 | `g_mb.holding[MB_REG_FEAT_CAP] = MB_FEAT_HOLD_WDT;` **모델 무관** (§9-A 결정에 따라 값만 `#if` 로 갈릴 수 있음) | R-4 | FC03 0x32 = 0x0001 |
| 10 | `app_modbus.c:363-373` START 분기 | `== 1u` → `!= 0u` 로 진입, 값 `switch`: **1** = 기존 그대로(`app_reg_command(START,US_COMM)` + `set_pot`) / **2** = `if (app_reg_start_allowed()) { START 디스패치; set_pot; hold_wdt_arm(&s_hwd, now); } else if (hold_wdt_armed) hold_wdt_keep(...)` (응답 유실 재시도 흡수) / **3** = `hold_wdt_keep(&s_hwd, now)` / default = 무시. 소거는 switch 밖 **값 불문** | R-2·R-3·R-10 ①② | START=2/3 이 의미를 가짐; START≥4 잔류값이 이제 소거됨(전엔 stale) |
| 11 | `app_modbus.c:714` `remote_en_step();` 다음 줄 | `if (hold_wdt_step(&s_hwd, now, app_reg_run_src()==US_COMM)) { app_reg_command(US_CMD_RUN_RELEASE, US_COMM); mon_printf("[mb] hold wdt trip\r\n"); }` — **분기 밖**: RTU 점유/TCP/미점유 어디로 빠져도 시간이 흘러야 한다(`apply_config` 링크 해제 후에도 hold 런은 서야 함 — `:648-650` 의 "keeps running" 은 탭 런 얘기) | R-1 | hold 런에만 새 정지 경로 |
| 12 | `app_modbus.c:683-690` `app_modbus_init` | `hold_wdt_init(&s_hwd);` | 부팅 초기화 | — |
| 13 | `app_modbus.c:336-339` (선택) | 게이트 닫힘 로그에서 `blocked == START && 값==KEEP` 제외 | 스위치 OFF 중 유지 신호 ~7건/s 가 mon 을 덮어 HV-12 판독 방해 | 로그만 |
| 14 | `fw/src/app_reg.c` `app_reg_measure()`(`:275-278`) 옆 + `fw/include/app_reg.h` | `uint8_t app_reg_run_src(void) { return g_reg.us_run_status; }` — publish 2 ms 게이트(`:493-496, :514`)를 거치지 않는 **지연 0** 값 | §4 경계 불변식이 "관측 사이 mutator ≤1" 에 의존하므로 stale 미러(`app_modbus.c:368-371` 가 이미 경고한 그 지연)로는 안 된다 | 없음(읽기 전용) |
| 15 | `fw/test/test_app_modbus_core.c:89-92` | 51칸 = 107 B PASS / 52칸 = 0(무응답) 케이스 추가 | 새 경계 고정 | — |
| 16 | `docs/requirements.md` · `docs/changelog.md` · `HANDOFF.md`/`NEXT_STEPS.md` · 이 문서(`2026-09-06-remote-hold-to-run-design.md`, 구현 정본) | 규칙(CLAUDE.md) | — |
| 17 | `fw/include/define.h:73-78` `VERSION_MSG` 날짜 | 릴리즈 관례(번호 동결·날짜 진행) | 표시만 |
| — | `docs/superpowers/specs/2026-08-02-remote-enable-gate-decision.md` §3.1 | **무변경** (사용자 결정) | §4 | — |
| — | `app_reg.c:410-411, 433-434, 451-452` 세 열거 | **무변경** | R-6 | — |
| — | `app_modbus.c:315-351` 게이트 닫힘 분기 | **무변경**(#13 로그 제외) — START 값 불문 소거가 이미 R-11 을 만든다 | — | — |

순수 FSM 의사코드(전체):

```c
void hold_wdt_arm (hold_wdt_t *w, uint32_t now) { w->armed = 1u; w->last_keep_ms = now; }
void hold_wdt_keep(hold_wdt_t *w, uint32_t now) { if (w->armed) { w->last_keep_ms = now; } }  /* 기동 권한 ✗ */
uint8_t hold_wdt_step(hold_wdt_t *w, uint32_t now, uint8_t run_is_comm)
{
    if (w->armed == 0u)            { return 0u; }
    if (run_is_comm == 0u)         { w->armed = 0u; return 0u; }   /* 누군가 세웠다 — 세션 종료, 트립 아님 */
    if ((uint32_t)(now - w->last_keep_ms) >= HOLD_WDT_MS) { w->armed = 0u; return 1u; }
    return 0u;
}
```

---

## 4. hold 런 경계 강제 — 워치독이 hold 런에만 걸리는 코드적 보장

§3.1 을 안 고쳐도 되는 이유를 코드 구조로 만든다. 세 겹:

**(1) 무장 지점이 정확히 하나.** `hold_wdt_arm()` 호출처는 `MB_START_HOLD` 분기(#10) 하나뿐이고, 그것도 `app_reg_start_allowed()`(`app_reg.c:132-155`)가 참일 때만이다. `src=US_COMM` 에 대해 `start_allowed()` ⇔ `app_reg_command(START)` 수락(`:178-193` — swallow 는 TOUCH 전용)이므로 **"무장됐다 ⇒ 방금 시작된 런은 우리 hold 런"** 이 성립한다. 탭 START(=1)·LCD(`app_lcd.c:46` US_TOUCH)·물리 버튼(`app_input.c:96` US_REMOTE)·weld(`app_weld.c:211` US_CYCLE)는 `s_hwd` 에 접근 경로 자체가 없다. HMI 의 탭 런이 도는 중에 원격기가 START=2 를 보내면 `start_allowed()` 거짓 → 무장 안 됨 → 그 런은 워치독 대상이 아니다.

**(2) 매 tick 생존 확인 = "관측 사이 mutator ≤1" 불변식.** `hold_wdt_step` 은 `app_modbus_tick` 첫머리(#11)에서 `app_reg_run_src()==US_COMM` 을 본다. 연속한 두 관측 사이에 런 소스를 바꿀 수 있는 것은 ⓐ 같은 tick 의 `apply_writes` **최대 1건**(RTU = tick 당 1 프레임 `app_modbus.c:735`; TCP = poll 당 FC06 1건 `app_modbus_tcp.c:195-212`) ⓑ 다음 iteration 의 lcd/weld/overload/input/horn/seek_reset/reg tick(`app.c:111-152`) — 이들은 **정지** 하거나 **TOUCH/REMOTE/CYCLE 로 시작**할 수만 있다. `US_COMM` 을 시작하는 건 ⓐ 뿐인데 ⓐ 는 1건이라 "정지 + 재시작" 을 한 구간에 못 한다. 따라서 **`US_COMM` 이 연속 관측된다 ⇔ 무장 시점의 그 런이다.** 다른 마스터가 STOP→START 로 갈아타면 그 사이 관측에서 IDLE 이 보여 세션이 죽고, 새 탭 런은 무장 없이 돈다. (#14 의 지연 0 접근자가 이 논증의 전제 — 2 ms stale 미러로는 상한 정지 직후의 탭 START 가 세션을 상속받는 구멍이 생긴다.)

**(3) 유지 신호에 권한이 없다.** `hold_wdt_keep` 은 `armed` 일 때만 시각을 갱신한다. 트립·타 경로 정지·E-STOP 뒤에 도착한 START=3 은 no-op 이고, 재기동은 START=2(=새 press) 만 한다. IDLE 에서의 START=3 도 no-op(HV-4).

**§3.1 과의 관계.** 게이트 닫힘 분기(`app_modbus.c:315-351`)는 한 줄도 바뀌지 않는다 — 탭 런은 여전히 게이트 해제 후 자연 종료한다(HV-3 이 회귀 증명). hold 런이 게이트 닫힘에 ≤T 로 서는 것은 **게이트가 세우는 것이 아니라 유지 신호가 굶어 워치독이 세우는 것**이고, 그 워치독은 (1)(2)(3) 에 의해 hold 런 밖으로 나갈 수 없다. 원격기가 요구한 "§3 표에 예외를 개정으로 명기" 는 **2026-08-02 원문 대신 신규 spec(#16)** 이 이 절을 인용하는 것으로 갈음한다(§10).

---

## 5. 타임아웃 T 확정값 + 근거

**T = `HOLD_WDT_MS` = 600 ms. 원격기 유지 주기 P = 150 ms 권고(100 ms 도 허용).**

| 항 | 값 | 출처 |
|---|---|---|
| 원격기 지터(락 경합) | ≤ ~100 ms | 요구사항 §2 "T<200 금지" 근거 |
| 유지 신호가 파라미터 쓰기 뒤에 서는 지연 | ≤ 131 ms | 요구사항 §2 실측 |
| 컨트롤러 측 FRAM 전맵 저장(그 파라미터 쓰기의 `app_config_save_all`) | 2 ms 정상 / **50 ms** I2C 타임아웃 최악 | `app_modbus.c:608-613` |
| TCP 이월 1 tick + 수퍼루프 | ms 대 | `app_modbus_tcp.c:202-212` |
| 정상 유실률 | 0/수천 | 요구사항 §2 |

- **최악 적층 + 유실 1건**: 150(P) + 150(유실 1) + 100 + 131 + 48 = **579 ms < 600** → 살아남는다.
  ⚠ 이중 계상 확인(2026-09-06, 원격기 질의): TCP 경로는 FC06 응답을 **apply_writes(=FRAM 저장) 뒤에** 송신한다
  (`app_modbus_tcp.c:190-196` build_response→append→apply, `:221-222` send). 따라서 원격기 실측 131 ms 에는
  **정상 FRAM(≈2 ms)이 이미 포함**돼 있고, 최악 가산은 50 이 아니라 **50−2 = 48 ms** 다. 결론 불변. T=500 이면 여기서 **오정지**한다(트랩 8). 이것이 원격기 권고 500 대신 600 인 유일한 이유이고, 50 ms FRAM 최악치는 원격기가 몰랐던 컨트롤러 쪽 사실이다.
- **정상 조건 트립 조건**: 연속 3건 유실(150·300·450 결번) → 600 ms 에 트립. 실측 유실 0 이라 확률적으로 무시 가능.
- **단절 시 잔여 가동** ≤ T + 수퍼루프 1회 + publish 2 ms ≈ **0.6 s** (현행 30 s 의 1/50). 상한 1000 ms(UX) 안.
- 하한 200 ms 도 넉넉히 위. host 테스트가 `200 ≤ HOLD_WDT_MS ≤ 1000` 을 고정한다(벤치 -D 가 새는 것 방지).

오정지 완화(트랩 8): ① 위 여유 산정 ② 원격기가 유지 신호를 **파라미터 쓰기보다 앞에** 큐잉(원격기 몫) ③ P=100 으로 내리면 여유 +50 ④ 오정지 = 정지 방향이라 안전, 조작자는 재-press 로 복구 ⑤ 컨트롤러 쪽 "유예 재무장" 은 **두지 않는다**(R-3 ② 위반). R-5 가 있으면 오정지가 화면에 "워치독" 으로 보이지만 이연(§9-C).

⚠ RTU 9600 8E1(보드 현재값)에서는 FC03 50칸 응답만 ≈120 ms 라 폴 16/s 자체가 불가하고, P=150 안에 폴+유지 신호를 넣기 빠듯하다. 38400 이면 ≈30 ms 로 문제없음. HV-15 이연 사유에 이 산술을 붙인다.

---

## 6. 구 펌웨어 fail-safe 성립 근거

- `e5ea557`(원격기가 지목한 F-A 펌웨어) `fw/src/app_modbus.c:336` = `else if (g_mb.holding[MB_REG_START] == 1u)` — **직접 확인**. 현 main `:363` 도 동일. START=2 / 3 은 어느 분기에도 안 걸려 **디스패치 0**, 런 없음.
- 그 이전 펌웨어(F-A 이전, `0x31` 잔류)도 START 분기는 slice-1 부터 `== 1u` 였다(현 main·`e5ea557` 두 지점에서 확인; 최고령 Modbus 태그 `hw-revA_fw-stage-c1` 도 `== 1u` 로 확인 — 즉 **모든 출하 가능 펌웨어에서** START≠1 은 기동하지 않는다).
- `0x32` 는 구 펌웨어에서 **FC06 범위 밖 = 무응답**(`app_modbus_core.c:84-85`), FC03 도 무응답(`:54-56`) → 잘못된 판별로 원격기가 hold 모드를 켜도 컨트롤러 쪽에 기동 경로가 없다. 판별(R-4)이 틀려도 물리 안전선(R-2)은 독립적으로 선다 — 요구사항의 "두 겹".
- 구 펌웨어에서 START=2/3 은 holding 에 **잔류**한다(`== 1u` 만 소거). FC03 0x1B 에 2/3 이 보이는 것뿐이며 무해 — 원격기는 "pending" 으로 해석하지 말 것(전달 사항).
- 벤치 HV-8 = **지금 보드(`66a2411`)에 새 빌드를 올리기 전** START=2 → `US=0` 확인이면 재플래시 없이 끝난다(현 main 도 `== 1u`).

---

## 7. host 테스트 계획 — `fw/test/test_app_hold_wdt_fsm.c`

Makefile: `BIN_HWD := /tmp/gds_test_app_hold_wdt_fsm`, 의존 `../src/app_hold_wdt_fsm.c ../include/app_hold_wdt_fsm.h ../include/app_modbus_core.h`, `test:` 목록·`clean` 추가, 헤더 주석 "총 17".

| # | 케이스 | 고정하는 것 |
|---|---|---|
| 1 | `init` → `armed=0`; `step(run_is_comm=1)` 반복 → 트립 0 | 무장 전엔 절대 안 트립(HMI 탭 런 보호) |
| 2 | 무장 전 `keep` 여러 번 → 여전히 `armed=0`, 트립 0 | R-3 ② 기동 권한 없음(HV-4) |
| 3 | `arm(t0)` → 150 ms 마다 `keep` + `step` 60 s → 트립 0, `armed=1` | HV-1 정상 경로 |
| 4 | `arm(t0)` → keep 없이 `step(t0+599)` = 0 / `step(t0+600)` = **1** → 이후 `armed=0` | 경계값(HV-2/9) |
| 5 | `arm` → `keep` 을 정확히 599 ms 간격으로 → 영구 무트립 | 경계 반대편 |
| 6 | 무장 중 `step(run_is_comm=0)` → 트립 **0**, `armed=0`; 이후 `keep`·`step(run_is_comm=1)` → 재무장 없음 | 타 경로 정지 후 재기동 없음(HV-5/6/7/14) |
| 7 | 트립 후 `keep` 계속 → 두 번째 트립 없음 | R-3 ②(HV-2 이후) |
| 8 | `arm` 중복 호출(응답 유실 재시도) → 세션 1개, `last_keep` 갱신 | 재시도 안전 |
| 9 | `arm(0xFFFFFF00)` → 랩 넘어 `step` 경계 → 트립 정확 | u32 랩 |
| 10 | `200 <= HOLD_WDT_MS && HOLD_WDT_MS <= 1000` | 계약 §2 범위 |
| 11 | `MB_START_TAP==1 / HOLD==2 / KEEP==3` 상호 상이·비0, `MB_FEAT_HOLD_WDT==1`, `MB_REG_FEAT_CAP==0x32 < MB_REG_COUNT` | wire 계약(`test_wire_values_match_contract` 선례) |

`test_app_modbus_core.c` 추가: FC03 51칸 → 107 B / 52칸 → 0 / FC06 `0x32` → 저장(미러가 되돌리는 건 글루) / FC06 `0x33` → 0.

R-6 뮤테이션(“새 소스값을 30초 목록에서 빼면 죽어야”)은 **해당 없음** — 새 소스값을 만들지 않았다. 대신 리뷰 체크리스트로: `git diff` 에 `app_reg.c` 세 열거 라인이 나타나면 리젝트.

`app_modbus.c` 글루는 host 0(기존과 같음) → §8 벤치가 실질 게이트.

---

## 8. HW 벤치 항목 (원격기 HV ↔ 우리 항목)

전부 TCP·`docs/superpowers/tools/mb_tcp.py`(mbpoll 불가 — bench-results §4 ③) · mon 캡처(ETH 모드) · `nc -z` 금지 · 생존 판정은 FC03. 유지 신호 송신 = `mb_tcp.py` 로 START=3 을 150 ms 주기 스레드 송신(스크립트 5줄, 벤치 시 작성).

| 우리 # | 원격기 | 절차 | PASS |
|---|---|---|---|
| H-0 | HV-8 | **새 빌드 플래시 전** 현 보드에 START=2, START=3 → 5 s 폴링 | `US=0` 유지, mon `[reg] cmd` 없음 |
| H-1 | HV-11 | 새 빌드(STD·REMOTE 각 1회): FC03 `0x32×1` → `0x0001`; FC03 51칸 응답 107 B; 52칸 무응답 | 3조건 |
| H-2 | HV-1 | START=2 → keep 150 ms → 15 s | `US=1` 지속, 30 s 전 자연 정지 없음 |
| H-3 | HV-2 | H-2 중 keep 스레드 kill(t0) | mon `[mb] hold wdt trip` t0+600±벤치 지터; FC03 `US 1→0` |
| H-4 | HV-3 | START=1 → 폴링만 | 30 s 상한까지 도는지(무회귀) |
| H-5 | HV-4 | IDLE 에서 keep 만 5 s | `US=0`, mon START 없음 |
| H-6 | HV-5 | hold 중 E-STOP(PC11 std 모드 EMSW 또는 model_type 전환 — `app_modbus.c:541-553` 주의) → 해제 → keep 계속 | `US=0` 유지 |
| H-7 | HV-6 | hold 중 STOP=1 → keep 계속 | `US=0` 유지 |
| H-8 | HV-7 | hold + keep 35 s | 30.0 s 부근 `1→0`, 이후 0 |
| H-9 | HV-9 | keep 간격 550 ms ×3 / 650 ms ×3 | 550 무트립 / 650 트립 각 3회 |
| H-10 | HV-10 | keep 직후 같은 poll 에 OUT_POWER 쓰기(`FRAMES_PER_POLL 4` 이내 연속 송신) | 이월 후 무트립 |
| H-11 | HV-12 | **이연(§0)** — PC8 은 초기 설정 스위치라 "hold 중 OFF" 는 운용 시나리오가 아니다. 필요 시 REMOTE throwaway(`in.sw = holding[0x2D] bit0`, bench-results §3-bis 방식)로 갈음 | — |
| H-12 | HV-13 | hold 중 LCD horn 모드 ON | 거동 기록; keep 으로 재기동 없음 |
| H-13 | HV-14 | hold 중 패널 RUN 탭 → keep 계속 | 정지(V-3), 재기동 없음 |
| H-14 | HV-16 | 코드 검토: START 분기 `save` 미설정 + keep 10/s × 10 분 소크 | mon `[i2c] err` 증가 0, `US` 정상 |
| H-15 | HV-15 | RTU | **이연**(RS-485 어댑터 게이트 + §5 9600 산술) |
| H-16 | — | 회귀: `gds_us_hmi`/mbpoll 흐름 START=1·STOP=1·cfg 쓰기·CFG_CAP `0xFA01` | 무회귀 |

자연 재현 불가: PC8 물리(H-11 가상 갈음) · RTU(이연) · "원격기 크래시" 는 keep 스레드 kill 로 등가.

---

## 9. 결정·이연 목록

**A. STD 빌드에도 넣을 것인가** — ✅ **결정: 넣는다, 모델 무관, `#if` 0개** (§0)
- 근거: HW 의존 없음. STD 는 게이트(PC8)가 없어 원격 START 에 **아무 인터록도 없는 모델** — 워치독의 가치가 오히려 크다. F-A 도 STD 에 있다(`define.h:67-68`).
- REMOTE 전용으로 하려면: 글루 한 곳에 `#if defined(MODEL_REMOTE) #define HWD_FEAT MB_FEAT_HOLD_WDT #else 0` 를 두고 미러 값(#9)과 START=2 디스패치 조건(#10)이 그 상수를 쓰면 `#if` 1곳(정적 폴딩). STD 는 `0x32=0x0000` = "신 펌웨어인데 확정적 미지원" — 사용자가 언급한 그 케이스.
- 어느 쪽이든 순수 FSM·host 스위트는 동일.

**B. 패널 LCD TOUCH lost-release 워치독** — ✅ **결정: 이번 범위 밖, 독립 슬라이스** (§0)
| 안 | 내용 | 대가 |
|---|---|---|
| B-0 빼기 (추천) | 현행 유지(30 s 상한 + `run_key_reanchor` `app_lcd_input.c:173-183`) | 원격 경로가 패널보다 안전해지는 비대칭이 남는다(R-10 ⑤). 벤치 범위 불변 |
| B-1 패널 생존 워치독 | DGUS WR-echo(`app.c:104` `dgus_is_echo`)를 keep 으로 삼아 `hold_wdt_t` 두 번째 인스턴스를 TOUCH 런에 무장 | 패널 **케이블 단절** 만 잡고 페이지 이탈(진짜 lost-release 원인)은 못 잡는다. DGUS 에코 주기·무쓰기 구간 분석 필요, LCD 벤치 항목 +4. 유지 신호 의미가 다르니 T 별도 |
| B-2 페이지 이탈 정지 | 런 페이지를 벗어나는 모든 전이에서 TOUCH 런 `RUN_RELEASE` | 이미 3곳은 reanchor 가 함. 조작자 수동 이동 시 정지는 2026-08-17 실측에서 "운전 지속" 이 현행 — 거동 변화 = 사용자 결정 필요 |
→ B-1/B-2 는 hold 워치독과 **독립 슬라이스**로 두는 것이 벤치 격리에 유리.

**C. R-5 정지 사유 레지스터** — ✅ **결정: 이연** (§0; 원격기도 권장 유지로 판정)
- 완전판은 `reg_stop_run`/`RUN_RELEASE` 전 경로(`app_reg.c:99-112, 238-243`)에 사유를 심어야 하고 E-STOP 강제 해제(`app_input.c:80`, src=런 소스)와 STOP 명령(src=US_COMM)이 인자로 구분 안 돼 시그니처 변경까지 간다. 글루만으로 되는 축소판({STOP·워치독·기타})은 30 s 상한을 못 가른다.
- 비트맵이라 나중에 `bit1 = STOP_REASON` + 레지스터 1칸으로 무파손 추가 가능 — 지금 예약하지 않는다.

**D. `0x31` 하위 바이트를 프레임 폭 신호로 올려 프로브 타임아웃 제거** — 미채택(§2.2·§2.3). 원격기가 정확-일치 판정을 `>=` 로 바꾸고 HMI 통보를 마친 뒤라면 재론 가능.

**E. HAND 모드 on-time 상한이 hold 런에도 걸린다** — TOUCH 는 면제(`app_reg.c:427-428`)인데 COMM 은 대상(`:451-452`). "패널 RUN 버튼과 동등" 에서 벗어나는 지점이나 R-6 ④ 가 명시적으로 승계를 요구했으므로 **무변경**. 면제하려면 새 소스값이 필요해 트랩 1 이 되살아난다 — 기록만.

**F. 유지 신호 게이트 닫힘 로그 스팸(#13)** — 원격기도 `0x2B != 1` 을 보면 keep 송신을 멈추도록 전달.

**G. RTU(HV-15)** — 이연 + §5 baud 산술.

---

## 10. 거절/축소 권고

1. **"§3 표에 hold 예외를 개정으로 명기"(R-1 말미)** → 축소: 2026-08-02 원문 무변경(사용자 결정). 신규 design spec 이 §4 를 실어 관계를 서술하고, 2026-08-15 §5.5 도 손대지 않는다(그 절의 "app_reg 를 건드리지 않는다" 는 여전히 참 — 접근자 4줄은 거동 0).
2. **R-5 정지 사유** → 이연(§9-C). 원격기 표시가 "(추정)" 을 붙이는 대가는 원격기가 이미 수용했다.
3. **유지 신호 전용 레지스터** → 기각(§2.4). 여유 6칸 유지.
4. **워치독 즉시 트립 입력(게이트 닫힘 시 T 대기 없이)** → 불필요. R-11 은 "T 안" 이고 유지 신호 굶김이 코드 0 으로 그것을 만든다.
5. **T 를 레지스터로** → 기각(YAGNI, 계약값은 소비자가 쓰면 안 됨). 컴파일 상수 + `#ifndef` 벤치 노브.
6. **LCD TOUCH 확장** → 이번 범위에서 빼길 권고(§9-B), 독립 슬라이스로.
