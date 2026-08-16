# comm/ethernet 레지스터 확장 — 설계 spec (F-A, 0x1E~0x29)

> **문서 요약**: 현재 LCD 에서만 편집 가능한 comm/ethernet 저장변수(슬레이브 주소·보드레이트·패리티·IP/NM/GW)를 예약 영역 `0x1E~0x29` 에 노출해 **원격에서도 편집**할 수 있게 한다. 핵심은 값 레지스터가 아니라 **staging + commit 프로토콜**이다: 통신 설정은 *그 값을 쓰는 데 사용 중인 링크 자체*를 제어하므로, 기존 방식(FC06 → 즉시 cfg 반영 → 즉시 FRAM)을 쓰면 반쪽 IP 가 실계에 영속되고 첫 필드 반영 즉시 UART 가 재초기화돼 나머지를 쓸 기회가 사라진다. 따라서 staged 버퍼에 모아 두었다가 `CFG_CTRL=1` 한 번으로 일괄 커밋하고, **부분 커밋은 없다**. 자기 링크를 끊는 커밋은 **펌웨어가 거부**한다(DG-12 확정) — 커밋 링크가 살아 있어야 read-back 으로 결과를 확인할 수 있고, 이 제품군의 쓰기 계약은 "read-back 이 유일한 진실"이기 때문이다. 거부는 예외응답이 아니라 `CFG_STAT` 레지스터로만 통지한다(무예외응답 설계 유지). `comm_mode`(SERIAL↔ETH 전환)는 어느 링크로 커밋해도 한쪽을 반드시 끊는 본질적 자기참조라 **v1 원격 쓰기 불가**로 두고 LCD 전용을 유지한다. calibration 과 MODEL_SETUP 은 사용자 범위 결정으로 제외됐다. **legacy samd20 에 대응물이 없는 신규 기능**이다.

**작성일**: 2026-08-16
**상태**: **초안 — 미검토**
**작성 경위**: 사용자가 2026-08-16 **DG-11(확장 스펙 승인)** 과 **DG-12(same-link 커밋 정책 = 펌웨어 거부)** 를 확정. 그때까지 원격기(`gds_us_remote`) 쪽에 제안서만 있고 이 저장소엔 전용 스펙이 없어 구현 근거가 부족했다 — 그 공백을 메우는 문서다.
**내용 원천**: `~/dev/work/gds_us_remote/docs/superpowers/specs/2026-08-01-modbus-register-extension.md` (소비 측 제안 정본). **본 문서가 펌웨어 구현 정본이다** — 두 문서가 갈리면 실코드를 가진 이쪽이 이긴다(F-B 에서 probe 판별값·창 상수가 그렇게 정정됐다).
**관련**: F-B 게이트 spec `2026-08-15-remote-enable-gate-design.md` (주소 공간 인접, apply 체인 공유) / 이더넷 재적용 `2026-07-04-eth-reapply-m7-design.md`

> ⚠️ **브랜치 전제**: 본 spec 의 코드 좌표는 **`feat/remote-enable-gate` 기준**이다. F-A 는 `mirror_live()`·`apply_writes()` 를 F-B 와 같은 자리에서 건드리므로, **F-B(T-8) 머지 이후 main 기준으로 재베이스한 뒤 착수**하는 것이 충돌이 가장 적다. F-B 보다 먼저 착수하면 양쪽이 같은 함수를 재작성하게 된다.

---

## 1. 배경 — 지금 무엇이 막혀 있나

컨트롤러의 통신 설정은 **전부 LCD 전용**이다. 원격기(`gds_us_remote`)는 컨트롤러 LCD 화면을 재현하는 것이 목표인데, comm/ethernet 페이지만은 재현할 레지스터가 없어 화면을 만들 수 없다. 현장에서 슬레이브 주소 하나를 바꾸려고 기계 앞까지 가야 한다.

예약 영역은 이미 있다 — `MB_REG_COUNT = 50`(0x00~0x31)인데 정의된 것은 0x00~0x1D 뿐이고, F-B 가 0x2A~0x2C 를 가져가면서 **0x1E~0x29 를 F-A 몫으로 비워 뒀다**(`fw/include/app_modbus_core.h`, "0x1E~0x29는 F-A(comm/eth 확장) 예약으로 비워 둔다" 주석).

⚠️ **예약 영역은 "비어 있음"이 아니라 "쓰기를 흡수함"이다.** `mb_write_reg` 는 `addr < 50` 이면 무조건 `holding[addr]` 에 저장하고 에코한다. 즉 **지금도 0x1E~0x31 에 쓰기가 "성공"하고 read-back 까지 통과한다** — 미러가 덮지 않으므로 값이 잔류하는 무의미한 스크래치다. 이 특성은 확장 전후 모두 함정이며 §5.9 에서 다룬다.

---

## 2. 범위

### 2.1 In (v1)

- 레지스터 `0x1E~0x29` 신설 — comm serial 3종 + comm_mode 1종(R) + ethernet 6종 + 제어 2종(`CFG_CTRL`/`CFG_STAT`).
- staging 버퍼 + 일괄 커밋 프로토콜, 커밋 검증 3단(범위·same-link·가동 중).
- `CFG_STAT` 상태/거부 사유 노출.
- staging 타임아웃, 링크 전이 시 폐기.
- 순수 로직 분리 + host 테스트 스위트 신설.

### 2.2 Out (명시적 제외)

| 항목 | 근거 |
|---|---|
| `cal_val` / `freq_cal_val` (calibration) | **사용자 범위 결정** — 컨트롤러 전용, 원격기·HMI 미사용(읽기 포함). 기술적으로는 노출 가능하나 안 한다 |
| MODEL_SETUP(1) 화면 재현 | **사용자 범위 결정**. ⚠️ 단 `MODEL_FREQ(0x17)`·`MODEL_TYPE(0x18)` **읽기는 기존 계약 그대로 유지** — 원격기 전 화면 라우팅의 전제다 |
| `comm_mode` 원격 쓰기 | 본질적 자기참조(§5.4). v1 은 `0x21` R 전용. LCD 전용 유지 |
| commit-confirmed(확인 타임아웃 후 자동 롤백) | DG-12 가 거부로 확정돼 "커밋 후 확인 불가" 시나리오 자체가 없다. 롤백 타이머·이중 영속의 복잡도와 자체 오류 모드만 남는다. 후속 옵션으로만 기록 |
| `horndown`, 센서 상태 | `app_config_t` 필드가 아니다 — 저장변수가 아니라 라이브 상태. 별도 논의(STATUS 비트 후보) |
| FC16(다중 레지스터 쓰기) 지원 | 범위 밖. staging 이 원자성 문제를 이미 해결하므로 FC16 이 필요 없다 |

### 2.3 legacy 관계

**legacy samd20 에 대응물이 없는 신규 기능이다.** samd20 은 통신 설정 원격 노출 자체가 없었다. 따라서 "samd20 충실"이 설계 제약으로 작용하지 않는 영역이며, 반대로 **기존 코어(`mb_core_decode`/`mb_write_reg`)는 건드리지 않는다** — 회귀 범위를 키우지 않기 위해서다(§6).

---

## 3. 확정 결정

| # | 결정 | 출처 |
|---|---|---|
| DG-11 | **확장 스펙 승인** — 할당 표(0x1E~0x29, comm/ethernet 만)·staging+commit 프로토콜·교차 경로 규칙·`comm_mode` 원격 쓰기 금지 | 2026-08-16 사용자 |
| DG-12 | **same-link 커밋 = 펌웨어 거부**. 확인 UX 동반 허용안은 채택하지 않는다 | 2026-08-16 사용자 |
| (범위) | calibration·MODEL_SETUP 제외, `MODEL_FREQ`/`MODEL_TYPE` 읽기 유지 | 2026-08-01 사용자 3차 결정 |

**DG-12 근거**: 커밋 링크가 살아 있어야 read-back 으로 반영을 확인할 수 있다. 이 제품군의 쓰기 계약은 *"read-back 이 유일한 진실"* 이고 펌웨어는 예외응답을 만들지 않으므로, 자기 링크를 끊는 커밋은 **확인 수단 자체를 없앤다** — 성공했는지 실패했는지 마스터가 영원히 알 수 없는 상태가 된다.

**수용하는 대가**: 물리 배선이 한쪽뿐인 현장은 그 링크의 설정을 원격에서 바꿀 수 없다. 그 경우 **LCD 로 바꾼다**. 이는 현행(전부 LCD 전용)보다 나빠지는 것이 없고, 잘못된 커밋으로 원격지 장비가 도달 불능이 되는 최악 시나리오를 구조적으로 제거한다. 완화는 현장 요구가 실증된 뒤에만 재검토한다.

---

## 4. 레지스터 계약

`MB_REG_COUNT` 는 **50 그대로**(0x29 < 50). `mb_core_decode` / `mb_write_reg` **무변경**.

| addr | 이름 | R/W | 인코딩 | 커밋 검증 | 대응 cfg |
|---|---|---|---|---|---|
| `0x1E` | `MB_REG_COMM_ADDR` | R/W **staged** | u16, 슬레이브 주소 | **1..247 외 거부** | `comm_address` |
| `0x1F` | `MB_REG_COMM_SPEED` | R/W staged | u16, index 0..5 (2400/4800/9600/19200/38400/115200) | `> CFG_COMM_SPEED_IDX_MAX` 거부 | `comm_speed_idx` |
| `0x20` | `MB_REG_COMM_PARITY` | R/W staged | u16, index 0..2 (EVEN/ODD/NONE) | `> CFG_COMM_PARITY_IDX_MAX` 거부 | `comm_parity_idx` |
| `0x21` | `MB_REG_COMM_MODE` | **R** (미러가 매 tick 덮음) | 0=SERIAL / 1=ETH_STATIC / 2=ETH_DHCP | — (v1 원격 쓰기 불가) | `comm_mode` |
| `0x22` | `MB_REG_ETHER_IP_H` | R/W staged | `ip[0]<<8 \| ip[1]` | 없음 | `ether_ip[0..1]` |
| `0x23` | `MB_REG_ETHER_IP_L` | R/W staged | `ip[2]<<8 \| ip[3]` | 없음 | `ether_ip[2..3]` |
| `0x24` | `MB_REG_ETHER_NM_H` | R/W staged | 동일 패킹 | 없음 | `ether_nm[0..1]` |
| `0x25` | `MB_REG_ETHER_NM_L` | R/W staged | | 없음 | `ether_nm[2..3]` |
| `0x26` | `MB_REG_ETHER_GW_H` | R/W staged | | 없음 | `ether_gw[0..1]` |
| `0x27` | `MB_REG_ETHER_GW_L` | R/W staged | | 없음 | `ether_gw[2..3]` |
| `0x28` | `MB_REG_CFG_CTRL` | **W** (consume-and-clear) | **1=COMMIT, 2=DISCARD**, 그 외 무시 | — | — |
| `0x29` | `MB_REG_CFG_STAT` | **R** (미러) | §5.5 상태 코드 | — | — |

**staged 그룹 = 9개**: `0x1E~0x20` + `0x22~0x27`. `0x21`(R)·`0x28`(cmd)·`0x29`(R)은 staged 가 아니다.

### 4.1 인코딩 결정 — IP 는 2옥텟/레지스터

1옥텟/레지스터면 IP·NM·GW 만 12개를 먹어 총 18개(예비 2칸)가 된다. 2옥텟이면 6개, 총 12개(예비는 F-B 편입 후 `0x2E~0x31` 4칸). 주소당 FC06 쓰기도 4회 → 2회로 줄어 **RS-485 첫 write 간헐 무효**(기존 계약 함정) 노출 기회가 절반이다. 기존 `WORK_CNTH/L` 상하위 분할 선례와도 일관된다.

원자성은 인코딩과 무관하다 — staging 이 구조로 해결한다.

### 4.2 cfg 필드 실체 (확인됨)

`fw/include/app_config.h` 의 `app_config_t`:
```c
uint8_t  comm_address, comm_speed_idx, comm_parity_idx, comm_mode;
uint8_t  ether_ip[4], ether_nm[4], ether_gw[4];
```
전부 `uint8_t` 다. u16 레지스터 ↔ u8 필드 변환에서 **상위 바이트 유실**이 생길 수 있으므로, 커밋 시 범위 검증이 대입보다 **먼저** 와야 한다(§5.3).

검증 상한은 매직 넘버를 쓰지 않는다 — `app_config.h` 에 이미 있다:
```c
#define CFG_COMM_SPEED_IDX_MAX   5u
#define CFG_COMM_PARITY_IDX_MAX  2u
```
`COMM_ADDR` 은 대응 상수가 없으므로 `CFG_COMM_ADDR_MIN/MAX`(1/247)를 같은 헤더에 신설한다.

> ⚠️ **`ether_*` 에는 검증을 두지 않는다.** LCD 편집 경로에 값 유효성 검사가 없기 때문이다 — **LCD 에 없는 규칙을 원격 경로에만 발명하지 않는다.** 잘못된 IP 는 조작자 책임이고, 이는 현행 LCD 동작과 동일하다.

---

## 5. 설계

### 5.1 왜 staging + commit 인가

comm/ethernet 은 **그 값을 쓰는 데 쓰이는 링크 자체를 제어**한다. 기존 값 레지스터 방식을 그대로 쓰면 셋 다 터진다:

1. **FC16 미지원** → IP 6개 레지스터를 원자적으로 못 쓴다 → 반쪽 IP 가 실계에 즉시 반영되고 FRAM 에 영속된다.
2. **`comm_speed` 반영 즉시** 다음 tick 에 UART 재초기화 → 나머지 필드를 쓸 기회가 없다.
3. **예외응답이 없어** 실패가 침묵인데, 링크가 끊기면 유일한 진실인 read-back 자체가 불가능해진다.

### 5.2 프로토콜

```
마스터                          펌웨어 (staged 그룹)
  │ FC06 COMM_SPEED=5      →   staged[SPEED]=5, dirty 세트 (cfg 불변 · FRAM 불변)
  │ FC03 read-back         ←   0x1F = 5   (dirty 인 동안 미러 대신 staged 값)
  │ FC06 ETHER_IP_H=...    →   (여러 필드 자유 순서, 각각 read-back 확인)
  │ FC06 CFG_CTRL=1        →   검증(§5.3) → 통과: cfg 일괄 반영 + 훅 + save_all 1회
  │                            거부: 아무것도 반영 안 함
  │ FC03 CFG_STAT          ←   2=COMMIT_OK / 3..6=거부 사유
```

| 규칙 | 내용 |
|---|---|
| staging 쓰기 | staged 레지스터 FC06 → **staged 버퍼에만** 저장 + dirty 마크. cfg·FRAM·통신 실계 무영향 |
| read-back | dirty 레지스터는 미러 대신 **staged 값** 반환 → "쓰기 후 read-back 필수" 계약이 staging 에도 그대로 성립 |
| 비-dirty 미러 | dirty 아닌 staged 레지스터는 매 tick cfg 미러(화면 렌더용 라이브 값) |
| COMMIT | `CFG_CTRL=1` → dirty 전 필드 검증 → **한 번에** cfg 반영 → 훅 발화 → `app_config_save_all()` **1회** → dirty 전체 해제 |
| **부분 커밋 없음** | 검증 하나라도 실패 → **전체 거부**, 아무것도 반영하지 않는다. 부분 반영은 마스터가 "무엇이 적용됐는지" 모르게 만든다 |
| DISCARD | `CFG_CTRL=2` → staged 폐기, dirty 해제, 미러 복귀 |
| staging 타임아웃 | 마지막 staged 쓰기 후 `CFG_STAGE_TIMEOUT_MS`(30 s) 내 COMMIT 없으면 자동 폐기 + `CFG_STAT=REJ_TIMEOUT`. 마스터 크래시로 남은 반쪽 편집이 미러를 계속 가리는 것과 LCD 동시 편집 충돌 창을 제한 |
| 링크 전이 폐기 | RTU 획득/해제·TCP 활성 전이(`mb_core_init` 지점)에서 staging **무조건 폐기** |

### 5.3 커밋 검증 (순서대로, 하나라도 걸리면 전체 거부)

1. **범위** — `COMM_ADDR` 1..247, `COMM_SPEED` ≤ `CFG_COMM_SPEED_IDX_MAX`, `COMM_PARITY` ≤ `CFG_COMM_PARITY_IDX_MAX`. ether 검증 없음(§4.2). → `REJ_RANGE`
   - `COMM_ADDR = 0`(NONE)은 **RTU 자체를 죽이므로 원격 커밋 금지**. LCD 는 허용하더라도 원격은 막는다 — 이건 "LCD 에 없는 규칙 발명"이 아니라 **원격 경로에서만 복구 불능이 되는 값**이라서다.
2. **same-link** (§5.4) → `REJ_SAME_LINK`
3. **가동 중** — US 가동·SEEK/RESET 진행 중이면 **전면 거부**. 가동 중 통신 링크 재초기화를 막는다. → `REJ_RUNNING`

### 5.4 same-link 거부 (DG-12)

| 커밋이 도착한 경로 | 거부 대상 | 허용 |
|---|---|---|
| **RTU** | serial 그룹(`0x1E~0x20`) dirty 포함 시 거부 | ether 그룹 커밋 |
| **TCP** | ether 그룹(`0x22~0x27`) dirty 포함 시 거부 | serial 그룹 커밋 |

**`comm_mode`(0x21)는 v1 원격 쓰기 불가.** SERIAL↔ETH 전환은 어느 링크로 커밋해도 반드시 한쪽을 끊는 **본질적 자기참조**라 교차 규칙으로 풀리지 않는다. LCD 전용을 유지한다. (원격기 화면에서 serial/ethernet 설정 페이지 간 *이동* 은 순수 UI 내비게이션이라 무관하다.)

**판정 지점 — 전송 경로 식별 (확인됨, 구현 가능)**

`app_modbus_apply_writes(void)` 는 현재 **인자가 없고**, 호출부가 정확히 둘이다:

| 호출부 | 경로 |
|---|---|
| `fw/src/app_modbus.c` (RTU tick 경로, FC06 수신 시) | RTU |
| `fw/src/app_modbus_tcp.c` (TCP poll 경로, FC06 프레임 처리 시) | TCP |

두 호출부가 구조적으로 분리돼 있으므로 판정이 가능하다. **채택: 시그니처에 전송 인자 추가** — `app_modbus_apply_writes(mb_link_t link)`.

- 파일 스코프 플래그를 호출 직전에 세팅하는 대안은 채택하지 않는다: 호출부가 늘거나 순서가 바뀌면 조용히 틀리고, 컴파일러가 잡아주지 못한다. 인자면 호출부 추가 시 **컴파일 에러로 드러난다.**
- 헤더 변경 1줄 + 호출부 2곳 수정이 전부다.

**거부 통지** — 예외응답을 만들지 않는다(무예외응답 설계 유지). `CFG_STAT` 레지스터로만 통지하며, `CFG_CTRL` 은 **수용 여부와 무관하게 소거**된다(§5.8).

### 5.5 `CFG_STAT` 코드 (R, 매 tick 미러)

| 값 | 이름 | 의미 |
|---|---|---|
| 0 | `CFG_STAT_IDLE` | staging 없음 |
| 1 | `CFG_STAT_STAGED` | dirty 필드 존재, 커밋 대기 |
| 2 | `CFG_STAT_COMMIT_OK` | 마지막 커밋 성공 (다음 staged 쓰기 또는 DISCARD 시 해제) |
| 3 | `CFG_STAT_REJ_RANGE` | 범위 검증 실패 |
| 4 | `CFG_STAT_REJ_SAME_LINK` | 자기 링크 그룹 커밋 시도 |
| 5 | `CFG_STAT_REJ_RUNNING` | 가동 중 커밋 시도 |
| 6 | `CFG_STAT_REJ_TIMEOUT` | staging 만료 폐기됨 |

**마스터 규약**: COMMIT 후 반드시 `CFG_STAT` 를 폴링해 수용/거부를 확인한다 — 기존 "명령 후 STATUS 확인" 규약과 동형이다. 거부 사유는 다음 staged 쓰기까지 래치된다.

### 5.6 적용(재초기화) 시점

| 그룹 | 방식 | 근거 |
|---|---|---|
| serial | cfg 반영만 하면 **다음 tick `apply_config()` 가 재초기화**. RTU 응답은 blocking 송신이라 커밋 응답이 구 설정으로 송신 완료된 뒤에만 재초기화된다 — **추가 지연 불필요** |
| ether | `app_lcd_hook_ether_apply()` 의 dirty-플래그 경로 재사용. 단 **커밋 응답 송출 여유로 `CFG_ETHER_APPLY_DELAY_MS`(500 ms) 지연 적용** — TCP 는 blocking 보장이 없어 즉시 재초기화 시 커밋 응답이 유실될 수 있다 |

> ⚠️ **미확인 — T-1 착수 시 이 브랜치에서 재확인할 것**: 위 두 행의 근거(`apply_config()` 의 tick 감지 재초기화 / RTU blocking TX / `app_lcd_hook_ether_apply` 의 dirty 플래그 소비)는 **원격기 제안서의 조사 기록을 인용한 것이고 본 문서 작성 시 재검증하지 않았다.** ponytail 리팩터가 `app_lcd_input.c` 를 분할했으므로 좌표가 이동했을 수 있다. 확인 방법: `app_modbus.c` 의 `apply_config()` 와 `usart6_mb.c` 의 송신 함수, `app_lcd.c` 의 ether 훅을 직접 읽고 §7 상수에 실측 근거를 적는다.

DG-12(거부) 덕분에 **커밋에 사용한 링크는 절대 끊기지 않는다** → 같은 링크에서 `CFG_STAT` 와 재미러 값 read-back 으로 적용을 확인할 수 있다.

### 5.7 F-B 게이트와의 상호작용

- **`CFG_CTRL=1`(COMMIT)은 `REMOTE_EN==1` 을 요구한다** — 통신 설정 변경은 실계 변경이다. staged 쓰기 자체는 실계 무영향이므로 **게이트 대상이 아니다**(staging 은 자유, 커밋만 게이트).
- **체인 순서**: 게이트 판정이 먼저, `CFG_CTRL` 처리가 나중. 게이트가 닫혀 있으면 `CFG_CTRL` 은 **디스패치 없이 소거**되고 `CFG_STAT` 는 변하지 않는다.
  - ⚠️ **F-B 의 소거 불변식을 따를 것**: "디스패치는 조건부, 소거는 무조건". `CFG_CTRL` 도 `== 1` / `== 2` 검사 **밖에서** 무조건 소거해야 한다. 소거를 조건 안에 두면 `CFG_CTRL=7` 같은 값이 영구 잔류하고, `0x28` 은 미러 대상이 아니므로 이후 모든 FC03 읽기가 유령 pending 커밋을 보고한다.
- 게이트 거부와 `CFG_STAT` 거부는 **다른 층이다**. 게이트 거부 시 `REMOTE_EN` 을 읽어 사유를 안다. `CFG_STAT` 는 게이트를 통과한 커밋의 검증 결과만 담는다. 두 값을 한 코드 공간에 합치지 않는다.

### 5.8 LCD SAVE 와의 충돌

LCD 도 같은 cfg 를 편집한다(shadow → SAVE 커밋 구조).

| 상황 | 거동 |
|---|---|
| staging 중 LCD SAVE | **비-dirty 필드는 즉시 재미러**(LCD 값이 보임), **dirty 필드는 staged 유지** |
| 이후 원격 COMMIT | **last-commit-wins** — staged 값이 LCD 값을 덮는다. 기존 값 레지스터와 같은 철학 |
| 방어 | 1차는 **원격기 편집 세션의 conflict 감지**(미러 값이 편집 시작 시점과 달라졌으면 경고). 펌웨어는 lock 을 두지 않는다 |

**펌웨어에 편집 lock 을 두지 않는 이유**: lock 은 해제 실패 시 LCD 를 잠그는 새 고장 모드를 만든다. 컨트롤러 LCD 는 항상 최종 권한을 가져야 한다(F-B 와 동형). staging 타임아웃(30 s)이 충돌 창을 이미 제한한다.

### 5.9 예약 영역 쓰기 흡수 함정

구현 **전** 에 0x1E~0x29 에 쓰기 테스트를 하면 "성공처럼 보이는 무의미 쓰기"가 되고, 구현 **후** 엔 같은 쓰기가 staging 을 만든다. 완화:

1. 커밋 없는 staged 값은 30 s 후 자동 폐기.
2. 실계 반영은 `CFG_CTRL=1` 명시 커밋에만.
3. **계약 갱신(벤치 PASS) 전까지 어떤 도구도 0x1E+ 에 쓰지 않는다**는 운용 규칙. ⚠️ 예외는 F-B 의 capability probe 하나뿐이며 그것은 `0x2A` 단일 주소다.

---

## 6. 모듈 구조

F-B 가 `app_remote_en_fsm` 을 순수 FSM 으로 뽑아 host 테스트한 선례를 따른다.

| 파일 | 성격 | 내용 |
|---|---|---|
| `fw/include/app_cfg_stage.h` / `fw/src/app_cfg_stage.c` | **신규 · 순수 로직 · host 테스트 대상** | staged 버퍼(9), dirty 비트마스크, 타임아웃, 검증 3단, `CFG_STAT` 전이. **ESP-IDF·HAL·FRAM 의존 없음** — 입력은 구조체, 출력은 구조체 |
| `fw/test/test_app_cfg_stage.c` | 신규 | 위 모듈의 host 스위트 (**16번째 스위트**) |
| `fw/include/app_modbus_core.h` | 수정 | `MB_REG_*` 12개 + `CFG_STAT_*` 코드 정의. `MB_REG_COUNT` 무변경 |
| `fw/include/app_config.h` | 수정 | `CFG_COMM_ADDR_MIN/MAX` 신설 |
| `fw/include/app_modbus.h` | 수정 | `app_modbus_apply_writes(mb_link_t)` 시그니처 변경, `mb_link_t` 정의 |
| `fw/src/app_modbus.c` | 수정 | `mirror_live()` 에 미러 추가(비-dirty 조건부), apply 체인에 `CFG_CTRL` + staged 스캔 분기, tick 타임아웃 검사, RTU 호출부 인자 |
| `fw/src/app_modbus_tcp.c` | 수정 | TCP 호출부 인자 |
| `fw/src/app_lcd.c` | 확인만 | ether 훅 재사용 — 신규 코드 없어야 정상 |

**`mb_core_decode` / `mb_write_reg` 는 건드리지 않는다.** samd20 충실 코어의 인터페이스 변경은 회귀 범위가 크다. staged 쓰기 감지는 apply 패스의 **스캔 분기 1개**로 한다: staged 범위를 순회하며 `holding[i] != (dirty ? staged[i] : cfg 미러값)` 인 첫 필드를 staged 에 반영. FC06 1회당 apply 1회 호출이라 "한 번에 한 필드"라는 기존 특성과 정합하며, **else-if 체인 구조를 유지할 수 있다**(추가 분기 = 명령 1 + 스캔 1). 테이블 구동 재작성 불필요.

---

## 7. 상수

```c
#define CFG_STAGE_TIMEOUT_MS       30000u  /* staged 편집 자동 폐기 */
#define CFG_ETHER_APPLY_DELAY_MS     500u  /* 커밋 응답 송출 여유 */
#define CFG_COMM_ADDR_MIN              1u
#define CFG_COMM_ADDR_MAX            247u
```
`CFG_COMM_SPEED_IDX_MAX`(5) · `CFG_COMM_PARITY_IDX_MAX`(2)는 `app_config.h` 에 **이미 있다** — 재정의하지 말고 그대로 쓴다.

> 두 타이밍 상수 모두 **초기값이며 벤치(§9)에서 확정**한다. `CFG_ETHER_APPLY_DELAY_MS` 는 특히 근거가 약하다(§5.6 미확인 항목).

---

## 8. Task 분해

| # | 내용 | 완료 판정 |
|---|---|---|
| **T-1** | §5.6 미확인 3건 재확인 + 레지스터/상수 정의(`app_modbus_core.h`, `app_config.h`) | 확인 결과가 본 spec §5.6·§7 에 반영됨. 빌드 통과 |
| **T-2** | 순수 모듈 `app_cfg_stage` + host 스위트 (**TDD — 테스트 먼저**) | 16번째 스위트 통과. 검증 3단·타임아웃·`CFG_STAT` 전이 전수. 커버리지 기존 기준 충족 |
| **T-3** | `mirror_live()` 통합 — `COMM_MODE`·`CFG_STAT` 무조건 미러, staged 9종 비-dirty 조건부 미러 | FC03 으로 12개 블록이 읽히고 값이 cfg 와 일치 |
| **T-4** | apply 체인 통합 — `CFG_CTRL` 명령군 추가(무조건 소거), staged 스캔 분기, `mb_link_t` 인자 전달, 링크 전이 폐기 | staging→커밋→재미러 왕복이 RTU 에서 동작 |
| **T-5** | F-B 게이트 연동 — 커밋에 `REMOTE_EN==1` 요구, 체인 순서 확정 | 게이트 닫힘 상태에서 커밋이 소거되고 `CFG_STAT` 불변 |
| **T-6** | `/code-review high` | 지적 반영 완료 |
| **T-7** | 벤치 VR-1~13 (§9) | 전 항목 PASS |
| **T-8** | main 머지 + 태그 | 태그 `hw-revA_fw-stage-comm-eth-ext` |

**T-2 는 T-1 완료 전에도 착수 가능**하다(순수 로직은 미확인 3건과 무관). T-3 이후는 직렬.

⚠️ **F-B(T-8) 머지 이후 재베이스한 뒤 T-3 부터 진행할 것** — `mirror_live()`/`apply_writes()` 를 F-B 와 같은 자리에서 건드린다.

---

## 9. 벤치 검증 (T-7)

| # | 항목 | 판정 |
|---|---|---|
| VR-1 | 기본 상태: 부팅 직후 `CFG_STAT=0`, staged 9종이 cfg 값 미러 | FC03 1블록 판독 일치 |
| VR-2 | staged 쓰기: `COMM_SPEED` 쓰기 후 read-back = 쓴 값, **cfg·FRAM 불변** | read-back 일치 + LCD comm 페이지에 변화 없음 + 재부팅 시 구 값 |
| VR-3 | `CFG_STAT` 전이: staged 쓰기 후 1(STAGED) | 폴링 확인 |
| VR-4 | DISCARD: `CFG_CTRL=2` → 미러 복귀, `CFG_STAT=0` | read-back 이 cfg 값으로 복귀 |
| VR-5 | **교차 커밋 성공(TCP→serial)**: TCP 접속 상태에서 serial 그룹 staged → 커밋 → `CFG_STAT=2` | 커밋 링크(TCP) 생존 + 새 보드레이트로 RTU 재접속 성공 |
| VR-6 | **교차 커밋 성공(RTU→ether)**: RTU 접속 상태에서 IP staged → 커밋 → `CFG_STAT=2` | 커밋 링크(RTU) 생존 + 새 IP 로 TCP 접속 성공 |
| VR-7 | **same-link 거부(RTU)**: RTU 접속 중 serial 그룹 커밋 시도 | `CFG_STAT=4`, **cfg 불변**, RTU 링크 생존 |
| VR-8 | **same-link 거부(TCP)**: TCP 접속 중 ether 그룹 커밋 시도 | `CFG_STAT=4`, cfg 불변, TCP 링크 생존 |
| VR-9 | 범위 거부: `COMM_ADDR=0` 또는 `=248` 커밋 | `CFG_STAT=3`, 전체 미반영(다른 dirty 필드도 반영 안 됨 — **부분 커밋 없음 확인**) |
| VR-10 | 가동 중 거부: US 가동 중 커밋 | `CFG_STAT=5`, cfg 불변 |
| VR-11 | 타임아웃: staged 후 30 s 방치 | `CFG_STAT=6`, 미러 복귀 |
| VR-12 | 링크 전이 폐기: staged 상태에서 RTU↔TCP 전이 | staging 폐기, 미러 복귀 |
| VR-13 | 게이트 연동: `REMOTE_EN=0` 상태에서 커밋 | `CFG_CTRL` 소거됨(잔류 없음), `CFG_STAT` 불변, cfg 불변 |
| VR-14 | LCD 동시 편집: staging 중 LCD SAVE | 비-dirty 재미러 / dirty 유지 / 이후 커밋 last-wins |
| VR-15 | 영속: 커밋 후 재부팅 | 새 값 유지 (`save_all` 1회 동작 확인) |

**VR-5·VR-6 이 이 스펙의 핵심이다** — 교차 경로 커밋이 실제로 링크를 살려두는지가 DG-12 설계의 전제다.

---

## 10. 미확인 / 미결

### 미확인 (T-1 에서 확인)

| # | 항목 | 확인 방법 |
|---|---|---|
| U-1 | `apply_config()` 가 cfg 의 comm 필드 변경을 tick 감지해 USART6 를 재초기화하는가 | `fw/src/app_modbus.c` 의 `apply_config()` 직독 |
| U-2 | RTU 응답 송신이 blocking 이라 커밋 응답이 재초기화 전에 나가는가 | `fw/src/usart6_mb.c` 송신 함수 직독 |
| U-3 | `app_lcd_hook_ether_apply()` 의 dirty 플래그를 `app_eth_tick` 이 소비하는가 / ponytail 분할 후 좌표 | `fw/src/app_lcd.c` + eth tick 직독 |
| U-4 | `us_on_status`(가동 중 판정)를 apply 경로에서 읽을 접근자가 있는가 | F-B 가 E-STOP 을 `app_estop_active()` 로 읽은 선례 확인 |
| U-5 | `CFG_ETHER_APPLY_DELAY_MS = 500` 의 근거 | 벤치 VR-6 에서 실측 |

### 미결 (사용자 결정)

| # | 항목 | 비고 |
|---|---|---|
| **DG-14** | F-02(벤치)용 **분리 보드** 확보 가능 여부 | 확보 불가 시 T-7 은 파일럿 대상 컨트롤러를 쓰게 되어 **P-21 이후로 순연**된다. VR-5~VR-8 은 통신 설정을 실제로 바꾸는 시험이라 파일럿 장비로 하기 위험하다 |

### 후속 옵션 (v1 제외, 기록만)

- `comm_mode` 원격 전환 — 본질적 자기참조. commit-confirmed 롤백을 도입해야 성립한다.
- commit-confirmed(자동 롤백) — DG-12 완화를 현장이 요구할 때만.
- 예비 `0x2E~0x31` 4칸 후보: `ENERGY_H`(u32 상위), `CFG_DIRTY`(staging 비트맵 디버그), 센서 상태, horndown 명령, calibration(범위 결정 번복 시).

---

## 11. 계약 문서 규율

`gds_us_remote/docs/reference/modbus-contract.md`(계약 정본)는 **T-7 벤치 전 항목 PASS 후에만** 갱신한다. 구현 완료만으로는 갱신하지 않는다 — F-B 도 같은 규율을 적용 중이다.

갱신 시 함께 고칠 것: 현재 "0x1E~0x31은 현재 미사용" 서술 → `0x1E~0x29` 사용 / `0x2A~0x2C` F-B 사용 / `0x2D~0x31` 예비.

`gds_us_hmi` 는 계약 공유 소비자다. 이 확장은 **기존 0x00~0x1D 를 건드리지 않으므로** SP1(읽기전용) 동작에 영향이 없다 — 다만 계약 갱신 시 통지한다.
