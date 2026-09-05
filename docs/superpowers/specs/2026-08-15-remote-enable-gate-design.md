# 원격 제어 활성화 게이트 — 설계 spec (F-B, 0x2A~0x2D)

> **문서 요약**: 컨트롤러 LCD에서만 켤 수 있는 **비영속 원격 활성화 게이트**를 신설한다. 게이트가 닫혀 있으면 Modbus(US_COMM) 경로의 **START/SEEK/RESET + cfg 쓰기**가 무시되고, **STOP과 모든 읽기는 무조건 통과**한다. 구현은 순수 FSM(`app_remote_en_fsm`, host-tested) + `app_modbus.c` 글루 + LCD 조작/표시 글루의 3층이며, 상태는 FRAM에도 holding 테이블에도 두지 않는다. 해제 조건은 창 만료(10분)·링크 침묵(10초)·E-STOP·LCD 수동 4종이고, 해제는 **신규 명령만 차단**할 뿐 진행 중인 런은 기존 종료 경로로 끝난다. 원격기가 구/신 펌웨어를 구분할 수 있도록 `0x2A`에 매직값을 매 tick 복원하는 capability probe를 둔다. **legacy samd20에 대응물이 없는 신규 기능**이며, 안전 규격 적합 기능이 아니다(§13).

**작성일**: 2026-08-15
**브랜치**: `feat/remote-enable-gate` (base = `refactor/ponytail-cleanup` `77beb7a`)
**선행 결정 기록**: `docs/superpowers/specs/2026-08-02-remote-enable-gate-decision.md`
**설계 정본(원격기 측)**: `~/dev/work/gds_us_remote/docs/superpowers/specs/2026-08-02-remote-enable-safety.md`

> 본 spec의 모든 `file:line`은 **이 브랜치 기준**이다. main 기준 좌표와 다르다(ponytail이 `app_lcd_input.c`를 분할했다).

---

## 1. 배경 — 지금 무엇이 열려 있는가

코드로 확인한 현재 상태:

| 사실 | 근거 |
|---|---|
| 원격 제어 권한 게이트가 **없다**. Modbus에 도달 가능한 누구나 `START(0x1B)`에 1을 쓸 수 있다 | `fw/include/app_modbus_core.h:15-44` 레지스터 맵 전체에 권한 개념 부재 |
| START의 유일한 관문은 `app_reg_start_allowed()`이고, 그 조건은 warm-up/IDLE/seek-reset/fault/overload/E-stop/horn뿐 — **소스 권한 조건 없음** | `fw/src/app_reg.c:132-155` |
| 무조건 backstop은 30초 절대 상한 하나뿐 | `fw/src/app_reg.c:44` `ON_TIME_SAFETY_MS 30000u`, 발화 `:379-388` |
| `mb_write_reg`는 `addr < 50`이면 **무조건 저장하고 에코**한다. 미사용 영역 write도 "성공"하고 read-back까지 통과한다 | `fw/src/app_modbus_core.c:70-84` |
| `0x1E~0x31`(20개) 미사용 | `MB_REG_COUNT = 50` (`app_modbus_core.h:9`), 맵 끝 = `MB_REG_STATUS 0x1D` (`:44`) |

마지막 항목이 안전에 직결된다: 게이트가 없는 **구 펌웨어에 원격기를 붙이면, 원격기가 활성화 레지스터에 쓰고 성공 응답을 받아 "활성화됐다"고 오판한 뒤 START를 보낼 수 있다.** §5.1 capability probe가 이를 막는다.

---

## 2. 범위

### 2.1 In (v1)

- 레지스터 `0x2A`(`REMOTE_CAP`) / `0x2B`(`REMOTE_EN`) / `0x2C`(`REMOTE_EN_LEFT`) 신설, `0x2D` 예약(주석만).
- 순수 FSM `app_remote_en_fsm` + host 테스트 스위트(15번째).
- `app_modbus_apply_writes()` 선두 게이트 + 매 tick 미러 3종.
- LCD 활성/해제 조작 + 활성 중 상시 식별 표시.
- 해제 4조건: 창 만료 / 링크 침묵 / E-STOP / LCD 수동.

### 2.2 Out (명시적 제외)

| 제외 항목 | 이유 |
|---|---|
| **comm/eth 레지스터 확장(F-A, `0x1E~0x29`)** | 별도 워크스트림. 승인 게이트·주소 공간 모두 분리돼 비의존이고, staging+commit·교차 경로·지연 적용까지 포함해 F-B보다 크다. 같은 파일을 건드리므로 동시 진행은 자기 충돌. **후속 스테이지.** |
| `0x2D` 2단계 승인(원격 요청 → LCD 승인) | 예약만. 향후 승격 경로 |
| `modbus-contract.md` 갱신 | 벤치 PASS 후 `gds_us_remote` 측 절차(F-07). 이 저장소 몫은 실측이 spec과 다를 때의 역반영 통지까지 |
| 원격기 측 probe/구현 | `gds_us_remote` 몫 |
| Modbus 인증·암호화 | 프로토콜 범위 밖. §13 잔여 위험으로 문서화 |

### 2.3 legacy 관계

**samd20에 대응 블록이 없는 신규 기능이다.** 포팅이 아니므로 legacy 충실도 기준이 적용되지 않는다. 대신 기존 구조와의 정합(미러 의미론·else-if 체인 순서·소스 분리)을 충실도 기준으로 삼는다.

---

## 3. 확정 결정 (2026-08-15 사용자)

| # | 결정 | 비고 |
|---|---|---|
| DG-15a | **활성 창 = 10분** | 초기값. 파일럿 실측 후 조정(VR-13) |
| DG-15b | **링크 침묵 임계 = 10초** | 초기값. 기존 `MB_REMOTE_HOLD_MS`(1000, `app_modbus.c:39`)와는 **별도 상수** |
| DG-16 | **LCD UI = 신규 DGUS 자산(A안)** | 롱프레스=활성 / 짧은 탭=해제. §8 |
| — | 베이스 = `refactor/ponytail-cleanup` 위 스택 | 게이트가 건드릴 `app_lcd_input.c`가 곧 분할 대상 |

선행 결정 기록(2026-08-02)에서 이미 확정된 것: 활성화 주체=LCD 전용 / 비영속 / 가동 중 해제는 신규 명령만 차단 / 게이트 범위=START·SEEK·RESET·cfg 쓰기(STOP·읽기 제외) / 게이트 위치=`US_COMM` 경로만.

---

## 4. 레지스터 계약

| addr | 이름 | R/W | 값 |
|---|---|---|---|
| `0x2A` | `MB_REG_REMOTE_CAP` | R (write는 미러가 덮음) | 매직 **`0x5201`** 고정 |
| `0x2B` | `MB_REG_REMOTE_EN` | R | 0=DISABLED / 1=ENABLED / 2=DIS_TIMEOUT / 3=DIS_LINK / 4=DIS_ESTOP / 5=DIS_LCD |
| `0x2C` | `MB_REG_REMOTE_EN_LEFT` | R | 잔여 활성 시간(초). 비활성 시 0 |
| `0x2D` | (예약) | — | 미사용. 미러하지 않음 |

`MB_REG_COUNT`는 **50 그대로**(변경 불요 — `0x2D < 50`). `mb_core_decode`/`mb_write_reg`는 **무변경**.

> 🔴 **2026-09-05 결정 변경 — E-STOP 래치 폐기.** 이 문서의 래치 규정(§5.2 엣지 판정,
> §10 불변식의 "레벨 해제로 자동 부활 안 함", 아래 문단)은 **더 이상 코드와 일치하지
> 않는다.** `0x2B`는 **어떤 사유도 래치하지 않는다** — E-STOP 은 레벨 추종이라 풀리면
> 게이트가 스스로 `1`로 복귀한다.
>
> 이유: 래치의 유일한 청소 경로가 "스위치 OFF→ON 재무장"인데 PC8 인터록이 미실장이라
> 그 동작을 할 수단이 없다. 원격기 벤치 실측에서 `remote_en 1 -> 4` 이후 8분간 전이가
> 없었고, 원격기 화면은 "기기 스위치를 재무장하라"는 **수행 불가능한 조치**를 안내하고
> 있었다. E-STOP 한 번 = 컨트롤러 재부팅 전까지 원격 제어 사망.
>
> 잃는 것: "E-STOP 해제 후 사람이 한 번 더 재확인"하는 의식. 해제 ~0.5s 뒤 원격 기동이
> 다시 가능해진다. 남는 E-STOP 차단은 3층 — `app_reg.c` START 가드(레벨, 소스 무관) ·
> `app_input.c` 매 tick RUN_RELEASE 강제 · 원격기 `gds_safety`의 STATUS bit1 잠금.
> 근거는 E-STOP 스위치 자체가 기계 앞 사람의 의도적 조작이라는 점(사용자 결정).
>
> ✅ **VR-7 재시험 PASS (2026-09-05, `gds_us_remote` 실측)**: 컨트롤러 `23ee66c` 에서
> `4 → 1` 자가 복귀가 **6회 전부** 관측됐다 — 재부팅도 스위치 재무장도 없었다. 대조군인
> 구 펌웨어 `66a2411` 은 같은 인가·해제 반복에 **8분간 전이 0건**이었다. 복귀 지연은
> 원격기 폴 주기(~60ms)가 관측 하한이라 밀리초 값은 못 재지만, 한 로그에서 `1→4` 와
> `4→1` 이 **94ms 안에** 둘 다 찍혀 컨트롤러 반응이 폴 주기보다 빠름은 확인됐다
> (10ms tick 과 모순 없음). 정밀값이 필요하면 컨트롤러 mon 이 정본이다.
>
> ⚠ **짧은 인가는 `4` 를 통째로 놓칠 수 있다.** 인가·해제가 소비자 폴 한 주기 안에
> 끝나면 원격기는 `4` 를 아예 못 본다. 게이트로서는 정상(원인이 사라졌다)이지만
> **"E-STOP 눌렀는데 로그에 없다" 는 벤치 오진의 자리**다 — 상태 부재를 미동작의
> 근거로 쓰지 말 것. `STATUS bit1` 은 레벨이라 같은 창에서 더 잘 잡힌다.


---

## 5. 설계

### 5.1 capability probe

`mirror_live()`가 매 tick `holding[0x2A] = 0x5201`을 **무조건 복원**한다. 원격기 판별 절차:

```
① 0x2A read           → 값 확인
② 0x2A에 P를 write     (P = 0x5201도 0도 아닌 임의값, 예 0x1234)
③ 0x2A read-back
   → P가 그대로 남아 있으면  = 구 펌웨어 (게이트 없음, 원격 START 금지)
   → 0x5201로 복원돼 있으면  = 신 펌웨어 (게이트 있음)
```

이는 `MODEL_FREQ`/`MODEL_TYPE`가 이미 쓰는 "read-only = 미러가 덮음" 의미론(`app_modbus.c:100-101`)과 동형이다.

> ⚠ **probe write 값은 0이 아니어야 한다.** 링크 전이 시 `mb_core_init()`이 테이블을 0으로 리셋하므로(§5.4), P=0이면 "구 펌웨어가 0을 유지"와 "신 펌웨어가 막 리셋됨"이 구분되지 않는다. 결정 기록 §4.1의 예시값 `0x0000`은 이 이유로 채택하지 않는다 — **원격기 spec에 역반영할 것**.

### 5.2 상태 머신 (`app_remote_en_fsm`)

**입력**: `now_ms`, LCD enable 이벤트, LCD disable 이벤트, 마지막 유효 요청 ms + 유효 플래그, E-STOP 레벨.
**출력**: 상태 코드(0~5), 잔여 초.

```
DISABLED --lcd_enable (단, E-STOP 레벨 비활성일 때만)--> ENABLED
                                                          expiry  = now + 600s
                                                          silence = 미무장

ENABLED --now >= expiry------------------------> DISABLED(2 TIMEOUT)
ENABLED --silence 무장 && now-last_req >= 10s--> DISABLED(3 LINK)
ENABLED --E-STOP 상승 엣지--------------------> DISABLED(4 ESTOP)
ENABLED --lcd_disable-------------------------> DISABLED(5 LCD)
ENABLED --lcd_enable (재조작)-----------------> ENABLED (창 갱신 + silence 재-미무장)
```

**침묵 타이머는 활성화 후 첫 유효 요청부터 무장한다.** 기존 `s_remote_ms`(`app_modbus.c:40`)는 활성화 이전 값일 수 있어, 그대로 쓰면 "LCD에서 켰는데 원격기가 아직 안 붙음 → 10초 만에 DIS_LINK"가 된다. 무장 전에는 창 만료만이 게이트를 닫는다. — **결정 기록에 없던 보완이며, host 테스트로 고정한다.**

**E-STOP은 레벨이 아니라 상승 엣지**로 판정한다. 레벨 추종이면 E-STOP 해제 시 게이트가 자동 부활하는데, "재활성은 LCD 조작으로만"이라는 결정과 모순되기 때문이다. 대신 **E-STOP 레벨이 활성인 동안 enable 요청은 거부**한다(엣지가 영영 안 오는 구멍 차단).

**원격 활동은 창을 연장하지 않는다.** 연장 수단은 LCD 재활성화뿐이다(결정 기록 §3.1).

### 5.3 게이트 배치와 불변식

배치는 `app_modbus_apply_writes()`(`fw/src/app_modbus.c:124`) **선두 1곳**. 이 seam이 맞다는 근거:

- RTU(`app_modbus.c:361`)와 TCP(`app_modbus_tcp.c`의 poll)가 **이 함수 하나를 공유**한다 → 두 전송로를 한 번에 덮는다.
- `app_reg` 쪽에 넣으면 안 된다: 같은 `app_reg_start_allowed()`를 US_TOUCH(패널)·US_REMOTE(물리 버튼)·US_CYCLE(weld)이 공용하므로 **로컬 조작까지 막힌다**. → **`app_reg.c` 무변경.**

게이트가 닫혀 있을 때의 동작:

```c
if (!remote_enabled) {
    /* 명령 3종: 디스패치 없이 소거 (아래 불변식) */
    holding[RESET] = 0; holding[SEEK] = 0; holding[START] = 0;
    /* STOP은 무조건 허용 (안전 방향). 디스패치는 ==1일 때만, 소거는 값 불문 */
    if (holding[STOP] == 1) { app_reg_command(US_CMD_RUN_RELEASE, US_COMM); }
    holding[STOP] = 0;
    return;                 /* cfg 체인 전체 skip */
}
```

> **소거는 STOP에도 적용된다** (2026-08-15 리뷰 반영). 소거를 `== 1` 검사 안에 두면
> `STOP = 2` 같은 비-1 write가 영구 잔류한다 — `0x1C`는 미러 대상이 아니고 게이트
> 개방 경로의 else-if도 `== 1`만 매치하므로, 이후 모든 FC03 읽기가 유령 pending
> STOP을 보고한다. 유령 디스패치는 없지만(양 경로 모두 `== 1` 검사) 관측값이 거짓말을
> 한다. 명령 4종 전부 "디스패치는 조건부, 소거는 무조건"이 규칙이다.

> **⚠ 불변식 — 소거는 생략 불가.** 명령 레지스터는 미러 대상이 아니므로(`mirror_live()`가 `0x19~0x1C`를 건드리지 않음, `app_modbus.c:66-121`) 무시만 하면 `1`이 홀딩에 잔류한다. 그러면 나중에 게이트가 열린 뒤 **아무 FC06이나 도착하는 순간 else-if 체인(`:133-157`)이 stale START를 디스패치**한다. host + 벤치 양쪽 검증 항목으로 고정한다(§10 T-9, §11 VR-3).

cfg 쓰기 거부는 별도 조치가 필요 없다: 체인을 skip하면 `mirror_live()`가 다음 tick에 `cfg` 값으로 홀딩을 되돌리므로(`:75-105`), 원격기 read-back이 자동으로 불일치를 보인다. **예외 응답은 보내지 않는다** — samd20이 Modbus exception을 만들지 않는 기존 계약(`app_modbus_core.c:212-213`)과 동형.

### 5.4 비영속 보장

게이트 상태는 **`app_modbus.c` 파일 스코프 static**에 둔다. 두 곳 모두 불가:

- **`holding[]` 불가** — `mb_core_init()`이 RTU 획득 시(`apply_config`)와 TCP 활성 시(`app_modbus.c:373`) 테이블을 0으로 리셋한다. 상태를 여기 두면 링크 전이마다 소실된다.
- **`app_config_t`/FRAM 불가** — 비영속이 요구사항. `fw/include/app_config.h` 및 `app_config.c` **무변경**.

매 tick 미러가 필수인 이유도 같다: 리셋 직후 홀딩이 0이어도 곧바로 `mirror_live()` 베이스라인이 따라오므로(`:374`, RTU는 `apply_config` 내), 0-읽기 창은 외부에서 관측되지 않는다(VR-11로 확증).

### 5.5 해제 시 진행 중인 런

**아무 것도 하지 않는다.** `app_reg`를 건드리지 않으므로 진행 런은 기존 경로로만 끝난다:

| 종료 경로 | 근거 |
|---|---|
| 원격 STOP (게이트와 무관하게 상시 허용) | §5.3 |
| 30초 절대 상한 | `app_reg.c:44`, `:379-388` |
| HAND on-time ceiling | `app_reg.c` auto-terminate |
| energy 도달 종료 | 〃 |
| E-STOP (자체 경로, 즉시 정지) | `app_input.c` |

소프트웨어 판단에 의한 급정지가 오히려 더 위험할 수 있다는 결정 기록 §3.1의 판단을 그대로 따른다.

---

## 6. 모듈 구조

| 파일 | 성격 |
|---|---|
| `fw/src/app_remote_en_fsm.c` / `fw/include/app_remote_en_fsm.h` | **신규 · 순수** — HAL·전역 무의존, 입력 주입. host-tested |
| `fw/test/test_app_remote_en_fsm.c` + `fw/test/Makefile` | **신규** — 15번째 스위트 |
| `fw/src/app_modbus.c` | 인스턴스 + tick step + 미러 3종 + apply 게이트 + 접근자 |
| `fw/include/app_modbus.h` | `app_remote_en_set(bool)` / `app_remote_en_state(void)` / `app_remote_en_left_s(void)` 노출 |
| `fw/include/app_modbus_core.h` | 레지스터 매크로 3종 + 상태 코드 + 매직 |
| `fw/src/app_lcd_input.c` | dispatch case 추가(§8) |
| `fw/src/app_lcd_disp.c` | 아이콘 + 잔여초 write-on-change |
| `fw/include/dgus_lcd.h` | 신규 VP 3종 정의 |

**신규 .c 파일이 생기므로 브랜치 전환·첫 빌드 시 cmake 재구성이 필요하다** — `./fw.sh`가 `build/.src-glob` 스탬프로 자동 처리한다.

FSM step은 `app_modbus_tick()` 선두에서 **RTU/TCP/미점유 분기와 무관하게 매 tick** 호출한다. 게이트가 SERIAL·ETH·미점유 어느 상태에서도 시간이 흐르고 만료돼야 하기 때문이다.

---

## 7. 상수

```c
#define REMOTE_EN_WINDOW_S         600u   /* 10분 — DG-15a, 파일럿 후 조정 */
#define REMOTE_EN_LINK_SILENCE_S    10u   /* 10초 — DG-15b, VR-13으로 확정 */
#define MB_REG_REMOTE_CAP_MAGIC 0x5201u
```

`MB_REMOTE_HOLD_MS`(1000, REMOTE 아이콘 홀드)와 **혼동 금지** — 목적도 시간 스케일도 다르다.

---

## 8. LCD 조작·표시 (DG-16 = A안, 신규 자산)

### 8.1 자산 요구사항 (사용자 작업)

| 용도 | VP(제안) | 요구사항 |
|---|---|---|
| 게이트 버튼 | `0x1086` | **touch-down과 touch-up 양쪽 이벤트를 모두 송신**해야 한다. `SETUP_MODEL(0x1084)`와 동일한 컨트롤 종류로 제작. `ENERGY_EN(0x1085)` 옆 빈 주소 |
| 상태 아이콘 | `0x1155` | `ICON_RESET~OUTERR`(`0x1150~0x1154`) 다음 빈 자리 |
| 잔여 초 | `0x1211` | u16. `LV_RUN_MODE(0x1210)`와 `LV_ENERGY_VAL(0x1212)` 사이 빈 자리 |

VP 주소는 `fw/include/dgus_lcd.h` 실사용 목록 대조로 고른 **제안값**이며, **자산 반입 후 실물 트레이스로 확정한다.**

### 8.2 조작 의미론

기존 `long_press_released()`(`fw/src/app_lcd_input.c:273-312`, `KEY_HOLD_MS 2000` `:47`)를 **그대로 재사용**한다. 이 헬퍼는 이 패널의 quirk(0x1084가 down/up 양쪽에 `data==0`을 보내고 `data==2`는 안 보냄)를 이미 흡수하고 있으며, `key_press_vp`로 VP별 분리를 보장하므로 세 번째 롱프레스 버튼이 추가돼도 간섭이 없다.

- **롱프레스(≥2s) 릴리스 = 활성화** — 오조작 방어
- **짧은 탭 = 해제** — 비대칭(끄는 쪽은 쉬워야 안전 방향)
- **활성 중 롱프레스 = 창 갱신**

### 8.3 ⚠ 자산 추론 구현 금지

자산 반입 후 **`LCD_TRACE_RX` 빌드로 실물 터치 트레이스를 먼저 찍는다.** 이 프로젝트에는 자산 추론이 실패한 전례가 두 건 있다: RUN 버튼이 양 엣지 모두 `KEY_MULTI=0`을 반환한 건(2026-06-08), `DISP_REMOTE(0x120e)` 렌더가 미검증으로 남아 있다가 2026-07-11에야 실증된 건. **Task 3은 트레이스 없이 착수하지 않는다.**

---

## 9. 표시 정책

- 활성 중 **상시 식별 표시**가 필수다(결정 기록의 요구). 아이콘은 write-on-change, 잔여 초는 1초 cadence.
- 기존 `DISP_REMOTE(0x120e)`는 "통신 활동 1초 홀드"로 **의미를 유지**한다. 게이트 상태와 통신 활동은 별개 개념이며, 하나의 아이콘에 두 의미를 실으면 조작자가 "원격이 붙어 있다"와 "원격이 기동할 수 있다"를 구분하지 못한다.

---

## 10. Task 분해

| # | 내용 | 게이트 |
|---|---|---|
| **T-1** | `app_remote_en_fsm.{c,h}` 순수 FSM + host 스위트 신설, `test/Makefile` 15번째 등록 | host green |
| **T-2** | `app_modbus_core.h` 매크로 3종 + 상태 코드 + 매직 (디코더 무변경) | 크로스 빌드 0-warning |
| **T-3** | `app_modbus.c` 인스턴스 + tick step + `mirror_live()` 3종 미러 + 접근자 3종(`app_modbus.h`) | 0-warning + 기존 14스위트 무회귀 |
| **T-4** | `app_modbus_apply_writes()` 게이트 (**소거 불변식 포함**) | 〃 |
| **T-5** | ⛔ *자산 대기* — `dgus_lcd.h` VP 정의 + `app_lcd_input.c` dispatch case + `app_lcd_disp.c` 표시 | 트레이스 선행 |
| **T-6** | 통합 리뷰 + `docs/requirements.md`·`changelog.md` 갱신 | 리뷰 0 Crit/High |
| **T-7** | **벤치 VR-1~VR-13** (§11) — ponytail 3항목 재검증·머지를 같은 세션 선두에 배치 | 전항목 PASS |
| **T-8** | `--no-ff` 머지 + 태그 `hw-revA_fw-stage-remote-gate` + 원격기 측에 실측 통지 | — |

T-1~T-4는 **자산 없이 지금 진행 가능**하다. T-5만 자산 게이트다.

### ⚠ T-5 이전의 중간 상태 — `REMOTE_EN_GATE_BYPASS` (2026-08-15 리뷰 반영)

T-4까지만 있으면 **게이트를 켤 수단이 없어 영구 DISABLED**다. 그 결과 모든 원격
START/SEEK/RESET과 cfg 쓰기가 RTU·TCP 양쪽에서 무시되고, **이 repo의 HW 검증이
전적으로 의존하는 mbpoll 흐름과 `gds_us_hmi` 레지스터 계약이 함께 죽는다.**
보드에서 되살릴 방법도 없다. 계획이 T-5를 자산 대기로 분리하면서 이 중간 상태를
다루지 않은 누락이다.

해소책 = **한시적 컴파일-타임 우회**:

```sh
cmake -S fw -B build -G Ninja -DREMOTE_EN_GATE_BYPASS=ON   # 벤치 전용
```

- **기본은 OFF**(게이트 유효). 폴라리티가 핵심이다 — 플래그를 깜빡하면 "제한적"으로
  실패하지, 안전 기능이 조용히 사라지지 않는다. 반대 폴라리티(기본 무효 + 활성화
  플래그)는 잊었을 때 게이트가 있는 척하며 없는 상태가 되므로 채택하지 않는다.
- ON이면 `app_modbus_init()`이 부팅 mon에 `*** REMOTE ENABLE GATE BYPASSED ***`를
  찍는다 — 플래그가 출하 빌드에 남는 것이 이 탈출구의 유일한 새 위험이라, 조용히
  지나가게 두지 않는다.
- 우회해도 FSM step과 미러 3종은 그대로 돈다 — `0x2A~0x2C` 관측(VR-2/VR-10 probe)은
  우회 빌드에서도 가능하다. 무력화되는 것은 `apply_writes`의 강제뿐이다.
- **T-5 머지 시 CMake 옵션과 `app_modbus.c`의 `#ifndef`를 함께 제거한다.**

> 이 브랜치는 **T-5 없이 main에 머지하지 않는다.** 게이트를 켤 수 없는 게이트는
> 제품을 죽이는 것과 같다.

호스트 테스트가 반드시 덮어야 하는 케이스(T-1):

| 케이스 | 확인 |
|---|---|
| 부팅 = DISABLED(0) | 초기 상태 |
| enable → ENABLED, LEFT = 창 길이 | 진입 |
| **침묵 무장 규칙** — enable 후 요청이 한 번도 없으면 침묵으로 안 꺼짐 | §5.2 보완분 |
| 첫 유효 요청 후 침묵 임계 경과 → DIS_LINK(3) | 무장 후 |
| 임계 미만 순단은 무해제 | 오탐 방지 |
| 창 만료 → DIS_TIMEOUT(2) | 만료 |
| **원격 활동이 창을 연장하지 않음** | 결정 §3.1 |
| E-STOP 상승 엣지 → DIS_ESTOP(4); 레벨 해제로 자동 부활 안 함 | §5.2 |
| **E-STOP 레벨 활성 중 enable 요청 거부** | 구멍 차단 |
| lcd_disable → DIS_LCD(5) | 수동 |
| 재활성 = 창 갱신 + 침묵 재-미무장 + 사유 래치 해제 | 재진입 |
| LEFT 산술 (경계·wrap) | u32 랩 안전 |

---

## 11. 벤치 검증 (T-7)

규칙: **SWD halt 금지**, mbpoll + LCD 육안만. 레지스터는 1-based(`-r` = wire+1): `0x2A`→43, `0x2B`→44, `0x2C`→45.

| # | 항목 |
|---|---|
| VR-1 | 회귀 기준: `0x00~0x1D` 전 덤프 + 직접런 560ms ceiling + FC06 클램프 (F-A가 재사용할 기준 덤프) |
| VR-2 | 부팅 직후 `-r 43 -c 3` = `0x5201 / 0 / 0` |
| VR-3 | 비활성: START(`-r 28 1`) → STATUS bit0 불변·무발진 / RESET·SEEK 동일 / cfg 쓰기 → read-back 미러 복원. **+ stale latch**: 비활성 START → 활성화 → 무관 FC06 1회 → 기동 없어야 함 |
| VR-4 | LCD 활성화 → `0x2B`=1, LEFT 감소. **연속 폴(-l) 유지 상태에서** START→기동, STOP→정지 |
| VR-5 | 창 만료 → `=2`. (10분이 길면 bench-short 상수 빌드로 타이머 검증 + 릴리스 값은 LEFT 시작점만 확인) |
| VR-6 | 침묵: 폴 중단/케이블 분리 → 임계 후 `=3`, 임계 미만 순단 무해제. **RTU·TCP 각각** |
| VR-7 | ✅ **PASS 2026-09-05** — EMSW(PC11, model_type=std) E-STOP → 즉시 `=4`, **해제 → 자가 복귀 `=1`**(래치 폐기 후 판정 반전). 6회 관측. ⚠ rig 노트 R1 |
| VR-8 | LCD 수동 해제 → `=5` |
| VR-9 | 활성 중 **물리 전원 재인가** → `=0` (FRAM 무흔적) |
| VR-10 | **probe (최우선)**: 구펌웨어 플래시 → `0x2B`에 1 선주입 → `0x2A`에 `0x1234` write → read-back 잔류 = 구FW 판정 → 신펌웨어 재플래시 → 매직 복원 = 신FW 판정 |
| VR-11 | comm_mode 전환(LCD SAVE)으로 링크 전이 → 다음 read에서 3종 미러 복원 + 활성 상태 유지 |
| VR-12 | 활성 창 중 제2 마스터 START 수용 **실증·기록** (§13 잔여 위험 근거) |
| VR-13 | 침묵 임계 오탐/미탐 실측 → DG-15b 확정 근거 |

⚠ 벤치에서 mbpoll **단발** 명령을 쓰면 침묵 해제가 계속 발화한다. VR-4 이후는 연속 폴(`-l`) 스크립트를 띄운 상태로 진행할 것.

> 글루 계층은 host 커버가 원리적으로 불가하다. DHCP 세션(2026-06-13)에서 리뷰 3회를 통과한 코드가 HW에서만 잡힌 전례가 있다 — **벤치가 유일한 게이트다.**

---

## 12. 미결 / 후속

- `REMOTE_EN_WINDOW_S`·`REMOTE_EN_LINK_SILENCE_S` 최종값 = 파일럿 실측(VR-13) 후 확정.
- VP 3종 주소 = 자산 반입 후 트레이스로 확정.
- probe 판별값 비영(非零) 요구 → **원격기 spec 역반영 필요**(§5.1).
- F-A(comm/eth 확장) = 별도 스테이지.

---

## 13. 리스크·잔여 위험

1. **안전 등급 기능이 아니다.** 소프트 플래그는 활성화 시점의 인간 판단 기록일 뿐, 그 이후 누가 위험구역에 들어갔는지 감지하지 못한다. **safety function으로 credit 될 수 없고**, 실기 적용 전 현장·안전 담당 확인이 필요하다. 운영 절차(활성화 = 현장 통제 책임 인수, 표지, 제어망 분리, 교육)로 보완해야 한다.
2. **Modbus에 인증이 없다.** 활성 창 안에서는 그 버스·네트워크의 **어떤 마스터든** 기동시킬 수 있다. 활성화는 컨트롤러 단위이지 특정 원격기 단위가 아니다 — VR-12로 실증·기록한다.
3. **원격 STOP은 비상정지가 아니다.** 물리 E-STOP만이 비상정지다.
4. **`mb_write_reg` 에코 문제**: FC06은 게이트 여부와 무관하게 항상 성공 에코한다(`app_modbus_core.c:70-84`). 거부는 read-back/STATUS로만 보인다 — probe 규율을 모르는 제3 도구는 "성공"으로 오독한다. 구조적 한계이므로 계약 문서에 명기한다.
5. **stale 명령 latch-through** (§5.3 불변식): 소거를 빠뜨리면 게이트 해제 후 유령 START. host + 벤치 양쪽 고정.
6. **미검증 리팩토링 위 스택**: ponytail이 아직 HW 게이트를 통과하지 않았다. 완화 = 첫 HW 세션 선두에서 ponytail 3항목 선처리 → 머지 → 이 브랜치 재베이스. 게이트 자체 벤치가 회귀를 재커버한다.
7. **DGUS 자산 quirk 전력** (§8.3): 트레이스 없이 T-5 착수 금지.
8. **침묵 타이머 오발**: 원격기 재연결 지터로 허가가 풀릴 수 있다. 무장 규칙(§5.2) + VR-13 실측으로 완화.
9. **VR-10이 구펌웨어 재플래시를 요구**한다 — 태그 빌드 왕복 절차를 벤치 스크립트에 명시할 것.
