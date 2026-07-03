# M7: LCD 저장 → 가동 중 ETH 재적용 (eth-reapply) 설계

> **요약**: LCD MULTI comm 페이지에서 ether IP/NM/GW 또는 comm_mode를 저장하면 재부팅 없이
> W5500에 즉시 반영한다. LCD 커밋이 dirty 플래그를 세우고(`app_lcd_hook_ether_apply` 확장),
> `app_eth_tick()`이 이를 소비해 phase별 재적용(`eth_reapply`)을 수행한다. 재적용 시
> TCP 서버 소켓(sock0)을 강제 close해 stale ESTABLISHED로 인한 접속 차단을 방지한다.
> samd20은 DATA_SAVE에서 `close_tcps`+`network_init`을 즉시 재호출했으므로(main.c:3327-3403)
> 본 스테이지는 신기능이 아니라 **레거시 거동 복원**이다. 감사 결정 큐 D6(M7).
> host 테스트 없음(HAL/vendor 글루) — 게이트 = cpp-review + HW E2E(§6).

- 날짜: 2026-07-04
- 브랜치(예정): `feat/eth-reapply-m7`
- 근거: 감사 D6/M7 (`docs/NEXT_STEPS.md` §1.3, `HANDOFF.md` 2026-07-02)
- 접근 결정: **A안(dirty-flag + tick 재적용)** — 사용자 승인 2026-07-04.
  - B안(hook 직접 호출) 기각: app_lcd↔app_eth 모듈 사이클(M1 discipline 위반), 이점 없음.
  - C안(매 tick netinfo 비교) 기각: DHCP 리스가 `cfg->ether_ip`에 RAM 미러되므로
    비교가 자기-트리거되어 무한 재적용 루프 위험.

---

## 1. 현황과 갭

Modbus 게이트 측은 이미 per-tick live-cfg 재평가로 mode 전환에 대응한다
(RTU 점유 해제 `app_modbus.c` apply_config / TCP poll 게이트 `comm_mode!=SERIAL && app_eth_available()`).
빠진 것은 **app_eth의 netinfo/DHCP 라이프사이클 재적용**이며 갭은 4개:

| # | 갭 | 현상 |
|---|---|---|
| G1 | `ETH_STATIC_UP` 중 IP 변경 | W5500 SIPR 미갱신 — 재부팅 전까지 옛 IP로 서비스 (M7 본체) |
| G2 | `STATIC_UP` 중 mode→DHCP 전환 | DHCP 클라이언트 미기동, `s_available=true`인 채 옛 static IP 서비스 |
| G3 | `DHCP_RUN` 중 mode→STATIC/SERIAL 전환 | DHCP 안 멈춤 + static netinfo 미적용 |
| G4 | LCD 커밋이 mode-only 변경 시 hook 미발화 | `app_lcd_input.c` commit_comm_mode_and_ether()가 ether 필드 변경시에만 hook 호출 → G2·G3은 트리거 자체가 없음 |

부수 발견 2건 (본 설계에 포함):

- **F1 (sock0 stale-ESTABLISHED 차단)**: TCP 서버는 소켓 1개(sock0)뿐. IP 변경 후 죽은
  피어와의 연결이 `SOCK_ESTABLISHED`로 남으면(피어 FIN/RST가 새 주소 체계에서 도달하지
  않으면 keep-alive도 없어 영구) listen 소켓이 없어 새 IP로의 접속이 영구 차단될 수 있다.
  → 재적용 시 sock0 강제 close 필수.
- **F2 (`eth_apply_on_link` DHCP 분기 available 누락)**: DHCP 분기가 `s_available`을
  건드리지 않는다. 부팅 경로(LINKWAIT→)에선 이미 false라 무해했지만, 재적용 경로
  (STATIC_UP→DHCP)에서는 **true가 남는 버그**가 된다. → 분기에 `s_available = false;` 명시.

## 2. 데이터 흐름

```
LCD DATA_SAVE (MULTI comm 페이지)
  └ commit_comm_mode_and_ether()            [app_lcd_input.c:505-548]
      조건 확장: ether_changed OR mode_changed → hook 발화     (G4 수정)
  └ app_lcd_hook_ether_apply()              [app_lcd.c:54-61]
      기존 mon 로그 유지 + s_ether_dirty = true
  └ (다음 superloop iter) app_eth_tick()    [app_eth.c]
      app_lcd_ether_dirty_take() 로 소비(consume-and-clear)
      → eth_reapply()                        (G1·G2·G3 수정)
```

tick-폴 방식은 comm_reconfigure hook이 passive인 이유와 같은 **M1 discipline**
(app_lcd↔소비자 include 사이클 회피) 패턴이다. cfg는 hook 발화 전에 이미 커밋돼
있으므로 재적용은 파라미터 없이 live cfg만 읽는다.

## 3. 컴포넌트 변경

### 3.1 `app_lcd_input.c` — commit_comm_mode_and_ether()

- `mode_changed` 지역 변수: `temp_comm_mode != cfg->comm_mode`를 **대입 전에** 판정.
- hook 발화 조건: 기존 `ether_changed` → `ether_changed || mode_changed`.
- 기존 0xFF 시드 가드(fix B)·shadow 커밋 로직은 무변경.

### 3.2 `app_lcd.c` + `app_lcd.h` — dirty 플래그

- `app_lcd.c`에 `static bool s_ether_dirty;` + hook 본문에 `s_ether_dirty = true;` 추가
  (기존 mon_printf 유지).
- `app_lcd.h`에 getter 선언:

```c
/* consume-and-clear: LCD ether/comm_mode 커밋 이후 첫 호출만 true.
 * 소비자 = app_eth_tick (재적용 트리거). */
bool app_lcd_ether_dirty_take(void);
```

### 3.3 `app_eth.c` — eth_reapply()

`app_eth_tick()` 진입부(phase switch 앞)에서:

```c
if (app_lcd_ether_dirty_take()) {
    eth_reapply();
}
```

dirty는 모든 phase에서 소비(clear)된다 — DOWN/LINKWAIT은 no-op이지만 어차피
자기 경로가 live cfg를 읽으므로 정보 손실 없음.

`static void eth_reapply(void)` phase별 전이:

| 현재 phase | 동작 |
|---|---|
| `ETH_DOWN` | no-op (칩 없음 — 기존 정책대로 재시도 없음) |
| `ETH_LINKWAIT` | no-op (link-up 시 `eth_apply_on_link()`가 live cfg를 읽어 자동 반영) |
| `ETH_STATIC_UP` | `app_modbus_tcp_reset()` → `eth_apply_on_link()` 재호출. cfg가 STATIC/SERIAL이면 새 netinfo 적용(available 유지·phase 유지), DHCP면 DHCP 기동 경로로 전이(available=false, phase=DHCP_RUN) |
| `ETH_DHCP_RUN` | cfg가 여전히 DHCP → **no-op(리스 보존)**. STATIC/SERIAL로 바뀌었으면 `DHCP_stop()` → `app_modbus_tcp_reset()` → `eth_apply_on_link()` |

- `eth_apply_on_link()` DHCP 분기에 `s_available = false;` 추가 (F2).
- 재적용 진입 로그 1줄: `[eth] reapply mode=%u phase=%u`.
- 신규 include: `app_modbus_tcp.h` (헤더 사이클 없음 — c-파일 수준 단방향).
- STATIC_UP 중 케이블이 뽑힌 상태의 재적용: `wizchip_setnetinfo`는 레지스터 쓰기라
  무해. link-drop 복구는 기존대로 범위 밖(slice-2a spec §1.1 유지).

### 3.4 `app_modbus_tcp.c` + `.h` — app_modbus_tcp_reset()

```c
/* eth 재적용 시 호출: sock0을 닫아 stale ESTABLISHED가 새 IP 접속을
 * 막는 것을 방지 (F1). control_tcp가 다음 poll에서 재open+listen. */
void app_modbus_tcp_reset(void) { close(MB_TCP_SOCK); }
```

`SOCK_CLOSED` 상태에서 `close()`는 no-op(무해). `g_tcp_active`(app_modbus.c)는
건드리지 않음 — holding[] 베이스라인은 유효하고, mode가 SERIAL로 바뀌면
기존 게이트가 자연히 리셋.

## 4. 에러 처리

신규 실패 경로 없음: `wizchip_setnetinfo`/`DHCP_stop`/`close`는 void 또는 무시 가능.
DHCP 재기동 실패는 기존 `DHCP_FAILED` 재시도 루프가 흡수. 칩 부재(`ETH_DOWN`)는 no-op.

## 5. 명시적 비범위

- link-drop(케이블 뽑힘) 복구 — slice-2a부터 이연 유지.
- Modbus(FC06)로 IP 변경 — 레지스터 맵에 ether 없음, HMI 스테이지(M6/M8/M9) 몫.
- DHCP 리스 → STATIC 저장 시 리스 IP가 FRAM에 굳는 기존 함정
  (`project_eth_dhcp_static_persist`) — 의도된 동작 유지, 무수정.
- per-unit MAC — slice-2 spec §8 이연 유지.

## 6. 검증

**host 테스트 없음** — 전 변경이 HAL/vendor/socket 글루(slice-2 교훈: 이 계층의
게이트는 통합 cpp-review + HW E2E뿐). 기존 host 7스위트 무회귀 + our-code 0-warning은
유지 게이트.

**HW E2E 체크리스트** (W5500 + DHCP 네트워크 벤치):

1. ETH_STATIC 부팅 → 가동 중 LCD로 IP 변경 저장 → **재부팅 없이** 새 IP ping+FC03,
   옛 IP 무응답, mon `[eth] reapply`+`[eth] up ip=<new>`.
2. STATIC→DHCP 전환 저장 → 리스 취득(`[eth] dhcp lease`)+FC03+LCD 리스 IP 표시.
3. DHCP→STATIC 전환 저장 → static IP 적용 (⚠ 리스가 `ether_ip`에 미러돼 있으므로
   IP 필드를 **직접 입력**할 것 — persist 함정).
4. ETH→SERIAL 저장 → RTU 응답 + TCP 무응답 / SERIAL→ETH 재전환 → TCP 복귀
   (⚠ SERIAL→ETH 방향은 reapply 시점에 RTU가 USART6 점유 중이라 mon 로그 억제 —
   기능으로 판정, 로그 부재는 정상).
5. 직접런 ceiling 무회귀 (START→STATUS 1×N→0).

## 7. 구현 단위 (plan 입력)

| # | 단위 | 파일 | 성격 |
|---|---|---|---|
| U1 | LCD 커밋 조건 확장 (G4) | `app_lcd_input.c` | 조건 1곳 + 지역변수 |
| U2 | dirty 플래그 + getter | `app_lcd.c`, `app_lcd.h` | 소규모 |
| U3 | sock0 reset API (F1) | `app_modbus_tcp.c/.h` | 1함수 |
| U4 | eth_reapply + F2 + tick 배선 | `app_eth.c` | 본체 |
| U5 | docs (changelog/NEXT_STEPS) | docs | 통상 |

U1~U3는 상호 독립, U4가 U2·U3에 의존. 전 단위 합산도 소규모라 단일 브랜치.
