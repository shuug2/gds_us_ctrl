# Stage C slice 2b — Modbus TCP (W5500, DHCP) 설계

> **요약**: slice 2a의 W5500 + 표준 Modbus TCP 서버에 **DHCP 클라이언트**를 더해
> `comm_mode==ETH_DHCP`(2)일 때 IP/NM/GW/DNS를 동적으로 획득하는 슬라이스. 신규 = WIZnet
> ioLibrary `Internet/DHCP/dhcp.{c,h}` 벤더(2a와 같은 핀 커밋) + `app_eth`에 DHCP 라이프사이클
> (`app_eth_init` DHCP 분기 + 신규 `app_eth_tick()` 슈퍼루프 구동 + assign/conflict 콜백).
> 코어/프레이밍/`app_modbus_tcp`/`spi1`는 **무수정**. 획득 모델 = **논블로킹 슈퍼루프**(samd20 충실):
> 부팅은 블록하지 않고, 리스 획득 전엔 `app_eth_available()==false`로 TCP 서버 대기.
> 획득 IP = **RAM만**(in-RAM cfg 미러로 LCD 표시), FRAM 미영속(samd20 충실). 실패(서버 없음/리스
> 실패) = **미획득 유지 + 계속 재시도**(non-fatal); IP conflict = 라이브러리 decline/재요청,
> **halt 안 함**(samd20 `while(1)` 폐기). 호스트 테스트 추가 없음(DHCP는 HAL/소켓 계층, 순수
> 프레이밍 유닛 무변경); 실통신 E2E(`mbpoll -m tcp` + DHCP 서버 망)는 보드 + W5500 연결 후(HW-gated).
>
> 승인: 사용자 brainstorming 2026-06-13 (① 범위 = DHCP 획득만[핫플러그 재init·SERIAL boot-skip
> 이연] ② 논블로킹 슈퍼루프 획득 ③ 미획득 유지+재시도 ④ RAM만 미영속).

## 1. 배경 / 목표

- slice 2a = 코어 재사용 + W5500 표준 Modbus TCP 서버(**static IP만**, `comm_mode!=SERIAL`이면
  cfg static netinfo 적용). 코드 완료(host-complete, 통합 cpp-reviewer APPROVED-WITH-COMMENTS),
  HW E2E(Task 9) 보드 게이트 대기, **미머지**(branch `feat/stage-c-modbus-tcp-static`).
- **slice 2b = DHCP 획득 경로 추가**: `comm_mode==ETH_DHCP`(2)일 때 DHCP 클라이언트로 IP/NM/GW/DNS를
  동적 획득 → W5500 적용 → TCP 서버 가동. `comm_mode==ETH_STATIC`(1)/`SERIAL`(0) 경로는 무변경.
- 포팅 방침 = 전송/네트워크는 검증된 라이브러리 재사용(공식 WIZnet ioLibrary `Internet/DHCP`),
  획득 라이프사이클은 samd20 충실(논블로킹 1s 틱 + 콜백). 단 samd20의 conflict→`while(1)` halt는
  컨트롤러에서 불가하므로 **non-fatal로 정정**.
- **base = slice 2a tip**(`feat/stage-c-modbus-tcp-static` `faf89d5`). 2b는 2a의 W5500 스택에
  의존하므로 stack 브랜치 `feat/stage-c-modbus-tcp-dhcp`.

### 1.1 범위 결정 (사용자)

slice 2a가 남긴 후속 3건 중 **DHCP 획득만** 2b로. 나머지는 명시 이연:
- **핫플러그 재init**(PHY-링크 복구 시 재init/재획득; samd20 `PHYStatus_check` 경로) → 후속 슬라이스.
  2b는 boot 시점 링크 업인 경우만 DHCP 시작(2a의 boot-only init 한계 그대로).
- **SERIAL boot-skip**(comm_mode==SERIAL이면 `app_eth_init` 스킵해 ~1s PHY 폴링 제거) → 후속.

## 2. 모듈 구조 (2a 위에 DHCP 라이프사이클만 추가)

| 계층 | 신규/수정 | 책임 | HAL/HW 의존 |
|---|---|---|---|
| 벤더 | `fw/vendor/wiznet/Internet/DHCP/dhcp.{c,h}` (신규) | 공식 ioLibrary DHCP 클라이언트(핀 커밋 `220ca7a6`). deps = 이미 벤더된 `socket.{c,h}`/`wizchip_conf.{c,h}`. `wiznet` 경고격리 lib + SYSTEM include에 추가 | UDP 소켓 (콜백 경유) |
| 브링업 | `fw/src/app_eth.c` + `fw/include/app_eth.h` (**수정**) | `app_eth_init()`에 DHCP 분기 추가(링크 업 + DHCP 모드 → `DHCP_init` + `reg_dhcp_cbfunc`, avail=false). **신규 `app_eth_tick()`** = DHCP 구동(매 호출 `DHCP_run()` + 1s `DHCP_time_handler()`). assign/conflict 콜백 | W5500/소켓 (간접) |
| 통합 | `fw/src/main.c` (**수정**) | `app_loop_iter`에서 `app_eth_tick()` 매 이터레이션 호출 | — |
| 무변경 | `app_modbus_tcp.{c,h}`, `app_modbus_tcp_frame.{c,h}`, `app_modbus_core.{c,h}`, `app_modbus.{c,h}`, `spi1.{c,h}` | 코어/프레이밍/전송/통합 게이트 전부 그대로. `app_modbus_tick`의 ETH 게이트(`comm_mode!=SERIAL && app_eth_available()`)는 **무변경**으로 재사용 | — |

### 2.1 `app_eth_available()` 의미 확장 (게이트 무변경 재사용)

2a에서 `app_eth_available()` = "칩+링크 업"이었고 static 모드에선 `app_eth_init()`에서 netinfo 적용
직후 true. 2b는 그 의미를 **"칩+링크 업 + IP 준비"(= TCP 서버 가동 가능)** 로 확장:
- **static 모드**: `app_eth_init()`에서 setnetinfo 직후 true (2a와 동일 — IP 즉시 준비).
- **DHCP 모드**: `app_eth_init()`은 `DHCP_init`만 하고 **false**. assign 콜백(리스 ACK)에서 netinfo
  적용 후 **true**. 리스 상실/conflict 시 false로 되돌림(라이브러리 재획득).

→ `app_modbus_tick`의 게이트 `comm_mode!=SERIAL && app_eth_available()`는 **코드 수정 없이** DHCP에서
"리스 전엔 TCP 미기동, 리스 후 기동"으로 올바르게 동작.

## 3. DHCP 라이프사이클 (samd20 `main.c` DHCP 블록 포팅)

```
boot  app_eth_init():
  ① spi1_init + 콜백 등록 + reset + wizchip_init  (2a 그대로)
  ② PHY 링크 polling (~1s non-fatal)  (2a 그대로)
       링크 다운 → return false, avail=false  (이번 세션 eth 미가용; 핫플러그 재init 이연)
  ③ 링크 업:
       comm_mode==ETH_STATIC → wizchip_setnetinfo(cfg static) + avail=true   (2a 그대로)
       comm_mode==ETH_DHCP   → DHCP_init(SOCK_DHCP, s_dhcp_buf)
                                + reg_dhcp_cbfunc(dhcp_ip_assign, dhcp_ip_assign, dhcp_ip_conflict)
                                + avail=false   (리스 대기)
       (comm_mode==SERIAL이어도 init은 시도 — 2a 거동 유지, SERIAL boot-skip은 이연)

loop  app_eth_tick()  (app_loop_iter에서 매 이터레이션):
  DHCP 클라이언트 활성(comm_mode==ETH_DHCP && 링크 업에서 DHCP_init 됨)일 때만:
    ① 1s 게이트(sys_tick_get_ms) → DHCP_time_handler()    // 활성 내내(획득 중 포함) — §3.2 정정
    ② DHCP_run() 매 호출 → 반환 처리:
         DHCP_IP_ASSIGN / DHCP_IP_CHANGED → assign 콜백이 이미 netinfo 적용 + avail=true
         DHCP_IP_LEASED → (이미 리스됨) avail 유지
         DHCP_FAILED    → 미획득 유지(avail=false), 라이브러리 재시도 cadence 지속
  static/SERIAL → no-op.

assign 콜백  dhcp_ip_assign():
  getIPfromDHCP/getGW/getSN/getDNS → wiz_NetInfo → wizchip_setnetinfo(NETINFO_DHCP)
  + in-RAM cfg->ether_ip/nm/gw 미러(LCD 표시용, FRAM 커밋 ✗)
  + avail=true + mon "[eth] dhcp lease ip=a.b.c.d"

conflict 콜백  dhcp_ip_conflict():
  mon "[eth] dhcp ip conflict" + avail=false   // 라이브러리가 DECLINE 후 재요청; halt 안 함
```

이후 흐름: assign으로 avail=true → `app_modbus_tick` ETH 분기가 `app_eth_available()` 통과 →
획득 IP로 TCP 서버(소켓 0) 가동(2a 그대로). 리스 갱신은 1s `DHCP_time_handler`로 라이브러리 자동
처리(갱신 시 assign/update 콜백 재호출 — 보통 동일 IP 재적용).

### 3.1 소켓 배치

- **DHCP = UDP 소켓 1**(`SOCK_DHCP=1`), **TCP 서버 = 소켓 0**(2a `MB_TCP_SOCK=0`). 2a의 `wizchip_init`은
  8소켓 각 2KB(TX/RX 16KB씩 = W5500 최대) 할당 — 소켓 1은 그 할당 재사용. (samd20은 sock6 사용했으나
  저번호 묶음 유지 위해 sock1 선택 — 기능 무관.)
- DHCP 버퍼 = 정적 `uint8_t s_dhcp_buf[1024]`(min `RIP_MSG_SIZE` 548B, 여유 1KB; samd20은 2KB 공유).

### 3.2 samd20 대비 정정 (1s 틱 무조건)

samd20은 `DHCP_time_handler()`를 `comm_status==ETH_NO_ERROR`(리스 후)에만 호출하나, ioLibrary
`DHCP_run()`의 재전송 타임아웃은 `dhcp_tick_1s`에 의존 → **획득 중에도 1s 틱이 필요**하다(없으면
DISCOVER 재전송 안 됨). 따라서 2b는 **DHCP 클라이언트 활성 내내 1s마다 `DHCP_time_handler()` 호출**
(획득 중 + 리스 후 모두). ioLibrary 계약 준수 = 의도적 정정.

### 3.3 폐기/정정된 samd20 거동

- conflict→`while(1)` halt → **폐기**(non-fatal: 로그 + avail=false + 라이브러리 재요청).
- `DHCP_FAILED` 후 NETINFO_STATIC 폴백 → **폐기**(DHCP 모드 static ether는 0.0.0.0라 무의미; 미획득
  유지 + 재시도로 대체 — §사용자 결정).

## 4. 영속 / FRAM

- **새 FRAM 슬롯 ✗.** comm_mode==DHCP면 FRAM `ether_ip/nm/gw`는 0 유지(samd20/2a factory 관례).
- 획득 IP는 **in-RAM `cfg->ether_ip/nm/gw` 미러만**(LCD가 라이브 표시) — FRAM 커밋 안 함. 리스 갱신마다
  FRAM 쓰기 없음(웨어 회피).
- DHCP 모드 LCD SAVE는 기존 `commit_comm_mode_and_ether` 거동(ether=0 유지) 그대로 — 획득 IP가 static
  설정으로 오영속되지 않음. (in-RAM 미러는 표시 전용, save 경로와 분리.)
- 부팅 직후~리스 전: cfg->ether_ip = 0.0.0.0(또는 직전 RAM 값) → LCD "0.0.0.0"/acquiring 표시.

## 5. comm_mode 점유 / 활성화 (2a와 동일, 변경점만)

- boot init은 2a 그대로(comm_mode 무관 시도, non-fatal). DHCP 분기만 추가(§3).
- TCP 서버 게이팅 `comm_mode!=SERIAL && app_eth_available()` = **무변경**(§2.1로 DHCP에서 자동 정합).
- USART6 점유: slice 1 규칙이 `comm_mode==ETH`면 RTU/mon에서 해제 — DHCP도 ETH라 동일(mon 자유).
- 런타임 SERIAL↔ETH_DHCP 전환: 2a처럼 다음 tick부터 분기 전환. 단 boot에서 DHCP_init이 안 됐으면
  (예: boot 시 SERIAL이었음) 런타임에 DHCP 시작은 **안 함**(boot-only init 한계 — 핫플러그 이연과
  동일 제약). 명시 문서화.

## 6. 테스트 / 게이트

- **호스트 테스트 추가 없음**: DHCP는 HAL/소켓/콜백 계층(`app_eth`/벤더)으로 순수 분리 가능한 새 로직이
  없음(프레이밍 유닛 `app_modbus_tcp_frame` 무변경). 기존 3스위트(`app_reg_calc`·`app_modbus_core`·
  `app_modbus_tcp_frame`) 유지.
- **빌드 게이트**: main 0-warning(`-Wall -Wextra -Wundef -Wshadow`, 우리 코드) + 호스트 3스위트 PASS.
  벤더 dhcp.c는 `wiznet` 경고격리 lib(`-w`)에 들어가므로 게이트 무영향(2a와 동일).
- **HW-gated (보드 + W5500 + DHCP 서버 망 연결 후)**:
  - ① comm_mode=ETH_DHCP 설정 → 전원사이클 → mon `[eth] dhcp lease ip=a.b.c.d` 확인(획득).
  - ② 그 획득 IP로 `mbpoll -m tcp -a 1 -t 4 ...` → slice-2a/slice-1 매트릭스 미러(FC03 미러 / FC06
    클램프+FRAM / START→560ms ceiling+ICON_RUN+STOP / 레지스터 보존 / work_cnt).
  - ③ **DHCP 서버 없는 망** 케이스: mon "acquiring" 지속, `app_eth_available()` false, TCP 타임아웃,
    RTU/mon/LCD 무영향(non-fatal 입증).
  - ④ **RTU FC06 회귀 spot-check**(SERIAL 복귀): 2a Task 9 동일 — apply-path 무회귀 가드.
  - ⑤ (가능 시) 리스 갱신/만료 후 재획득(장시간 — best-effort).

## 7. 브랜치 / 머지 순서

- 브랜치 `feat/stage-c-modbus-tcp-dhcp`, **base = slice 2a tip** `faf89d5`(stack — 2a의 W5500 스택
  의존). 2a가 HW E2E 후 변경되면 2b rebase.
- 머지: 호스트 게이트(0-warning + 3스위트) 통과 후 코드 완료. HW E2E는 보드 세션. 2a처럼 HW E2E PASS 후
  머지 + 태그 `hw-revA_fw-stage-c2b`(사용자 결정). 2a 머지 선행 권장(2a→2b 순서).

## 8. 미해소 디테일 (구현 시 확정, 코딩 차단 ✗)

1. **벤더 dhcp.{c,h} includes 해소**: 구현 0단계(Task 1)에서 controller가 `Internet/DHCP/dhcp.c`의
   `#include`가 기존 벤더 `socket.h`/`wizchip_conf.h`로 해소되는지 + DNS/util 의존이 없는지 확인(2a
   Task 1 방식). 추가 의존 발견 시 같은 핀 커밋에서 벤더.
2. **DHCP API 이름/시그니처**: `DHCP_init(uint8_t sn, uint8_t* buf)`, `int8_t DHCP_run(void)`,
   `void DHCP_time_handler(void)`, `void DHCP_stop(void)`,
   `reg_dhcp_cbfunc(void(*assign)(void), void(*update)(void), void(*conflict)(void))`,
   `getIPfromDHCP/getGWfromDHCP/getSNfromDHCP/getDNSfromDHCP(uint8_t*)`, 반환 enum
   `DHCP_IP_ASSIGN/CHANGED/LEASED/FAILED` — Task 1에서 벤더 헤더로 확정해 후속 Task 프롬프트에 주입
   (2a 방식). samd20 참조(`ref/samd20/W5500/dhcp.h`, `main.c`)와 대조.
3. **소켓 번호**: DHCP=1 잠정(TCP=0). wizchip_init의 소켓 버퍼 배분(2a: 각 2KB)과 충돌 없는지 확인.
4. **1s 게이트 위치**: `app_eth_tick()` 내 `static uint32_t s_dhcp_prev_ms` + `(now - prev) >= 1000`
   (sys_tick_get_ms). `app_reg`/`app_lcd`의 ms-게이트 패턴과 동일.
5. **assign 시 cfg RAM 미러 쓰기 안전성**: `app_lcd_cfg()`가 반환하는 라이브 cfg에 ether_ip 미러 쓰기 —
   LCD save 경로(commit_comm_mode_and_ether)가 DHCP 모드에서 ether=0 유지하는지 재확인(오영속 방지).

## 9. Deferred (이번 슬라이스 범위 밖)

- **핫플러그 재init**: PHY-링크 복구 감지(boot 후 케이블 연결) → 재init/재획득(samd20 `PHYStatus_check`
  + `comm_status` 상태머신). 별도 슬라이스.
- **SERIAL boot-skip**: comm_mode==SERIAL이면 `app_eth_init` 스킵(부팅 ~1s PHY 폴링 제거). 별도 정리.
- 런타임 SERIAL→DHCP 전환 시 즉시 DHCP 시작(현재는 boot-only init).
- per-unit MAC, INT 구동 RX, 획득 IP FRAM 캐시, 멀티소켓 — 2a §8/§9와 공통 이연.
