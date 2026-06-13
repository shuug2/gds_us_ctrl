# Stage C slice 2a — Modbus TCP (W5500, static IP) 설계

> **요약**: slice 1에서 만든 공유 Modbus 코어(`app_modbus_core`, `MB_MODE_TCP` 분기 준비됨)를
> **W5500 이더넷 + 표준 Modbus TCP 서버**로 재사용하는 슬라이스. 신규 = WIZnet ioLibrary
> 벤더(공식 upstream) + SPI1 드라이버 + W5500 브링업(`app_eth`) + TCP 앱 글루(`app_modbus_tcp`).
> 레지스터 의미(미러/클램프/FRAM/명령/점유)는 RTU와 **전량 공유** — 전송층 + 프레이밍만 다름.
> 코어는 무수정(slice 1 호스트 검증 보존). 코드 + 프레이밍 호스트 테스트는 HW 없이 완결;
> 실통신 E2E(`mbpoll -m tcp`)는 보드 + W5500 연결 후(HW-gated). static IP = cfg(FRAM);
> **DHCP는 slice 2b로 분리.**
>
> 승인: 사용자 brainstorming 2026-06-13 (범위 2a-static / 표준 Modbus TCP 프레이밍 /
> 공식 ioLibrary / boot 1회 init + comm_mode==ETH 게이팅).

## 1. 배경 / 목표

- 기존 SAMD20은 Modbus 슬레이브를 Serial(RS-485) **또는** TCP(W5500) 택일 모드로 제공
  (`comm_mode`: 0=SERIAL / 1=ETH_STATIC / 2=ETH_DHCP — FRAM 영속, LCD 스테이지에서 편집·영속
  포팅 완료; `cfg->ether_ip/nm/gw[4]` 존재).
- slice 1 = 코어 + RTU(HW E2E PASS·머지 `e351cad`, tag `hw-revA_fw-stage-c1`). **slice 2a =
  코어 재사용 + W5500 TCP 서버(static IP)**. 목표: HW 없이 코드·프레이밍 테스트 완결, 보드 연결
  후 E2E만.
- 포팅 방침 = 전송/네트워크는 검증된 라이브러리 재사용(공식 WIZnet ioLibrary), 프레이밍은
  **표준 Modbus TCP 준수**(samd20의 비표준 raw 응답 quirk는 폐기 — 아래 §3.3), 레지스터 의미는
  samd20/RTU 충실.

### 1.1 표준 준수 결정 (samd20 quirk 폐기)

samd20 TCP 경로(`main.c:5083-5100` + `modbus.c:286`)는 RX에서 MBAP 6바이트를 스트립한 뒤
응답을 **raw RTU 형식(`[unit, fc, data, CRC_hi, CRC_lo]`, MBAP 에코 없음)** 그대로 `send_tcps`로
송신한다 — 표준 Modbus TCP 마스터(`mbpoll -m tcp`)가 기대하는 `[MBAP 에코 7B + PDU, CRC 없음]`과
불일치(samd20은 비표준 커스텀 마스터와 통신한 것으로 추정). slice 1 RTU를 검증한 도구가 표준
`mbpoll`이므로, slice 2a는 **표준 Modbus TCP를 구현**한다. 코어는 무수정으로 두고(항상 CRC 부착)
**TCP 글루에서 코어 응답의 trailing CRC 2바이트를 절단 + MBAP 헤더를 래핑**한다.

## 2. 모듈 구조 (slice 1 3계층 패턴 미러)

| 계층 | 신규/수정 | 책임 | HAL/HW 의존 |
|---|---|---|---|
| 벤더 | `fw/vendor/wiznet/` (신규) | 공식 WIZnet ioLibrary: `Ethernet/wizchip_conf.{c,h}`, `Ethernet/socket.{c,h}`, `Ethernet/W5500/w5500.{c,h}`. config = `wizchip_conf.h`(`_WIZCHIP_=W5500`, 소켓 버퍼 크기). **DHCP·loopback 제외**(2b/불요). read-only 벤더(ST HAL과 동급 취급, config 헤더만 우리 소유) | W5500 (콜백 경유) |
| 전송 드라이버 | `fw/drivers/spi1.c` + `fw/include/spi1.h` (신규) | SPI1 마스터 init + byte/burst read·write + CS select/deselect + W5500 reset. ioLibrary 콜백 등록 대상 | SPI1, GPIO |
| 브링업 | `fw/src/app_eth.c` + `fw/include/app_eth.h` (신규) | `reg_wizchip_*_cbfunc` 등록 → reset 시퀀스 → 버전 체크 → PHY 링크 **non-fatal 타임아웃** → `wizchip_setnetinfo`(cfg static IP/NM/GW + 하드코딩 MAC). samd20 `ethernet.c` 등가. `app_eth_init()`→avail, `app_eth_available()` | 간접 |
| 앱 글루 | `fw/src/app_modbus_tcp.c` + `fw/include/app_modbus_tcp.h` (신규) | `app_modbus_tcp_poll()`: 소켓 FSM(port 502) + recv → MBAP strip/검증 → **공유 코어** decode(`MB_MODE_TCP`) → 쓰기 시 **공유 apply 패스** → MBAP wrap + CRC 절단 → send | 간접 |
| 통합 | `app_modbus.{c,h}`/`main.c`/`app.c` (수정) | boot eth init 배선; `app_modbus_tick()`에서 `comm_mode==ETH && eth_available`→TCP poll, `==SERIAL`→기존 RTU (상호배타 = 택일). 공유 `g_mb` + apply 헬퍼 노출 | — |

### 2.1 코어/apply 공유 (재구현 금지)

`app_modbus_tcp.c`는 RTU(`app_modbus.c`)와 **동일한 `mb_core_t g_mb` 인스턴스**와 **동일한
write-apply 패스**(OUT_POWER 클램프 체인 + work_cnt=0 리셋 + 명령 dispatch + `save_all` FRAM
커밋)를 사용한다. 이를 위해 `app_modbus.c`가 보유한 apply 로직을 내부 헬퍼로 노출
(`app_modbus_apply_write()` 형태)하여 두 전송층이 공유. 레지스터 미러(`app_modbus_tick()`의 라이브
값→holding[] 갱신)도 공통 — TCP/RTU 어느 쪽이 활성이든 동일 테이블을 본다.

## 3. 데이터 흐름 — 표준 Modbus TCP 프레이밍

```
W5500 recv() → req = [MBAP: txn(2) proto(2) len(2) unit(1)] + [PDU: fc + data...]
  ① 검증: 최소 7바이트 수신, proto == 0x0000, MBAP len 필드 ≥ 2, 실제 수신 ≥ (6 + len) — 길이 sane
  ② pdu_len = MBAP len 필드 값 (= unit + PDU 바이트수 = 코어에 넘길 frame 길이),
       pdu_for_core = &req[6] = [unit, fc, data...]   (slice1 코어가 frame[0]=addr/frame[1]=fc 기대;
       addr=unit은 MB_MODE_TCP에서 스킵)  — samd20 buf[6+i] 스트립과 동일하되 길이는 고정 12가
       아니라 MBAP len에서 도출(표준 정합, samd20의 고정 12바이트 복사 quirk 폐기)
  ③ n = mb_core_decode(&g_mb, pdu_for_core, pdu_len, MB_MODE_TCP, resp, &fc)   // addr+CRC 스킵
       resp = [unit, fc, data, CRC_hi, CRC_lo]      // 코어는 모드 무관 항상 CRC 부착(무수정)
       n == 0 → malformed/미지원 → 무응답 (코어 silent 충실)
  ④ fc == 0x06 → app_modbus_apply_write()  (clamp + FRAM commit + 명령 dispatch) — RTU와 공유
  ⑤ MBAP 래핑 + CRC 절단:
       tcp_resp[0..1] = req[0..1]            // txn id 에코
       tcp_resp[2..3] = 0x0000               // proto id
       tcp_resp[4..5] = (n - 2)              // length = unit + PDU 바이트수 (CRC 제외)
       tcp_resp[6..]  = resp[0 .. n-3]       // [unit, fc, data] — trailing CRC 2B 절단
       tcp_resp[6]    = req[6]               // unit id = 요청 unit 에코(표준 준수; 코어 resp[0]은
                                             //   device_addr라 -a≠device_addr 마스터 대비 덮어씀)
       tcp_len = 6 + (n - 2) = n + 4
  ⑥ send(SOCK_TCPS, tcp_resp, tcp_len)
```

RX 6바이트 스트립은 samd20과 동일. **차이는 응답(⑤)만** — MBAP 에코 + CRC 제거(표준 준수).
malformed·미지원 FC = 무응답(예외 응답 생성 안 함, samd20/RTU 충실).

### 3.1 지원 FC / 레지스터맵

slice 1 코어 그대로 (FC 01/02/03/04/05/06; H_REG 0x00~0x1D). 읽기 미러·쓰기 클램프·명령
consume-and-clear·STATUS 비트(bit0=US, 나머지 deferred)는 §slice1 spec과 동일. TCP는 전송만 다름.

### 3.2 소켓 FSM (samd20 `process_tcp.c control_tcps` 포팅)

단일 소켓 `SOCK_TCPS=0`, 단일 연결, 서버 listen(port 502). `getSn_SR(sn)` 기반:
`SOCK_CLOSED`→`socket(sn, Sn_MR_TCP, 502, 0)` → `SOCK_INIT`→`listen(sn)` → `SOCK_LISTEN` →
`SOCK_ESTABLISHED`(`Sn_IR_CON` 처리) → recv/응답 → `SOCK_CLOSE_WAIT`→`disconnect(sn)`. 매 poll 1회
상태 전이 (samd20 충실).

### 3.3 폐기된 samd20 quirk (문서화)

samd20 `decode_comm(mode!=0)`의 `send_tcps(0, response, send_cnt)` raw 송신 = 폐기. 우리는
§3-⑤ 표준 MBAP 래핑으로 대체. (RX 스트립 의미론은 동일하게 유지.)

## 4. W5500 브링업 + SPI1 드라이버

### 4.1 핀 (pinmap §SPI1 — Ethernet, 확정)

| 핀 | 역할 | 구성 |
|---|---|---|
| PA5 | SPI1_SCK | AF5 |
| PA6 | SPI1_MISO | AF5 |
| PA7 | SPI1_MOSI | AF5 |
| PA4 | ETH_SS (CS) | **소프트웨어 GPIO 출력** ⚠ HW NSS(AF) 아님 — W5500은 멀티바이트 프레임 동안 CS를 LOW로 유지해야 하므로 바이트당 자동 토글하는 HW NSS 사용 불가. idle = HIGH |
| PC5 | ETH_NRST | GPIO 출력 (reset 시퀀스) |
| PC4 | ETH_INT | GPIO 입력 (2a 폴링 — 미사용, 입력 구성만) |

### 4.2 SPI1 드라이버 (`spi1.c`)

- SPI1 마스터, 8-bit, **CPOL=0/CPHA=0 (Mode 0)** (W5500), MSB-first. prescaler = APB2(96MHz)에서
  W5500 안전 클럭(~12MHz 이하)으로 선정 (구현 시 확정, §8).
- `spi1_xfer_byte(uint8_t)→uint8_t` (blocking transceive), `spi1_burst_read/write(buf, len)`,
  `spi1_cs_low()/cs_high()`, `spi1_eth_reset()` (PC5 토글 + delay, samd20 `wiznet_reset` 등가:
  NRST HIGH→LOW→100ms→HIGH→200ms).
- ioLibrary 콜백 등록: `reg_wizchip_cs_cbfunc(cs_low, cs_high)`,
  `reg_wizchip_spi_cbfunc(read_byte, write_byte)`,
  `reg_wizchip_spiburst_cbfunc(burst_read, burst_write)` (upstream API).

### 4.3 브링업 (`app_eth.c`) — non-fatal init

- 순서(samd20 `ethernet_init` 등가): 콜백 등록 → `spi1_eth_reset()` → `getVERSIONR()` 체크 →
  PHY 링크 polling(`ctlwizchip(CW_GET_PHYLINK)`), **최대 N회(≈99×10ms) 후 실패 시 반환** →
  `wizchip_setnetinfo(&netinfo)`.
- **non-fatal**: W5500 미실장/링크 OFF(현 보드) → `eth_available=false`,
  `mon_printf("[eth] unavailable")`, **부팅 계속**. comm_mode==ETH여도 TCP 경로 스킵.
  RTU/mon/LCD 무영향.
- `netinfo`: ip/sn(nm)/gw = `cfg->ether_ip/ether_nm/ether_gw`, mac = **하드코딩 상수**
  (기본 samd20 값 `{0x00,0x08,0xdc,0x78,0x91,0x71}` 재사용 — per-unit 설정은 후속, §8),
  dns = 무관, dhcp = `NETINFO_STATIC`.

## 5. comm_mode 점유 / 활성화

- **boot 1회 init**: `app_eth_init()`을 `main.c`(app_init 이후, cfg 로드 후)에서 1회 호출.
  comm_mode 무관하게 시도(non-fatal). 결과 `eth_available` 기록.
- **TCP 서버 게이팅**: `app_modbus_tick()`에서
  `cfg->comm_mode != COMM_SERIAL && eth_available` → `app_modbus_tcp_poll()` (≈1ms cadence,
  samd20 `tick_1ms & 0x01` 등가),
  `cfg->comm_mode == COMM_SERIAL` → 기존 RTU 경로. **상호배타 = 택일.**
- **USART6**: slice 1 점유 규칙이 이미 `comm_mode==ETH`면 RTU/mon에서 USART6 해제
  (`[mb] release(mode=1)`, HW E2E 실증). 따라서 ETH 모드에서 mon(USART6)은 자유 — TCP와 충돌 없음.
- comm_mode를 LCD에서 SERIAL↔ETH로 전환 시: 다음 tick부터 분기가 바뀜(재init 불요 —
  W5500은 boot에서 이미 init). static IP를 LCD에서 바꾸면 재적용은 2a 범위에선 reboot 또는
  reconfigure hook (구현 시 결정, §8) — 기본은 boot netinfo 사용.

## 6. 테스트 / 게이트

- **호스트 테스트** (`fw/test/test_app_modbus_tcp.c` 신규): MBAP 프레이밍 글루를 순수 검증
  (W5500/socket/SPI 모킹 없이, 프레이밍 함수만 분리 호출):
  - FC03 요청 raw 바이트 → PDU 추출(strip 6) → 코어 decode → MBAP-wrapped 응답(txn 에코·
    proto 0·length 정확·**trailing CRC 없음**) 바이트 단위 확인.
  - FC06 쓰기 라운드트립 + apply 패스 호출 확인.
  - MBAP 검증 실패(proto≠0 / length 불일치 / 과소길이) → 무응답.
  - 코어 silent(미지원 FC) → 무응답.
  - 프레이밍 함수는 `app_modbus_tcp.c`에서 순수 부분(`mb_tcp_frame_response()` 등)을 분리해
    호스트 링크 가능하게 구성 (HAL/socket 비의존).
- **빌드 게이트**: main 0-warning(`-Wall -Wextra -Wundef -Wshadow`) + 호스트
  `app_reg_calc`·`app_modbus_core`·`app_modbus_tcp` 전부 PASS. 벤더 ioLibrary는 경고 격리
  (벤더 헤더 `-isystem` 또는 타깃별 플래그 — 구현 시 확정, §8).
- **HW-gated (보드 + W5500 연결 후)**: LCD/FRAM으로 comm_mode=ETH_STATIC + static IP 설정 →
  `mbpoll -m tcp -a 1 -t 4 -r <ref> -c <n> <ip>` 매트릭스 = slice 1 RTU 6항목 미러:
  ① FC03 폴링(라이브 값 = 부팅 cfg 일치) ② FC06 OUT_POWER write + 클램프(30→50/120→100) +
  reset 후 FRAM 영속 ③ START→STATUS bit0=1 + 560ms ceiling 자동정지 + STOP + ICON_RUN 시각 ④
  occupancy serial↔eth 전환(eth에서 mon 복구·RTU 타임아웃) ⑤ 레지스터 보존 ⑥ work_cnt FC06 0.
  + 링크업·소켓 connect/established·재연결(disconnect→재listen).

## 7. 브랜치 / 머지 순서

- 브랜치 `feat/stage-c-modbus-tcp-static` (또는 `-slice2a-tcp`), **base = main**
  (slice 1 머지 완료 `e351cad`, 스택 불요 — main 단독).
- 머지: 호스트 게이트 통과 후 코드 머지 가능(HW E2E는 보드 세션). 또는 slice 1처럼 HW E2E
  PASS 후 머지 + 태그 `hw-revA_fw-stage-c2a` (사용자 결정).

## 8. 미해소 디테일 (구현 시 확정, 코딩 차단 ✗)

1. **MAC 주소**: 기본 samd20 값 재사용(충실). per-unit MAC(FRAM 필드 추가) = 후속.
2. **SPI1 클럭 prescaler**: APB2(96MHz) ÷ presc → W5500 안전(~12MHz 이하, max ~33–80MHz). 구현 시
   레지스터 r/w sanity로 확정.
3. **PC4 INT**: 2a는 폴링(매 tick control_tcps) → INT 미사용(입력 구성만). 인터럽트 구동 = 후속.
4. **벤더 경고 격리**: ioLibrary가 우리 0-warning 게이트(`-Wall -Wextra -Wundef -Wshadow`)에
   걸리면 `-isystem` include 또는 벤더 타깃 별도 플래그. 구현 시 확정.
5. **static IP 런타임 변경 적용**: LCD에서 IP 변경 시 즉시 재적용(reconfigure hook) vs reboot
   적용. 기본 = boot netinfo. (DHCP 도입 2b에서 재론.)
6. **공식 ioLibrary 버전/경로**: `Wiznet/ioLibrary_Driver` 최신 태그 고정 + 디렉토리 트리 정리
   (필요한 `Ethernet/*`만 vendoring). 구현 0단계에서 확정.

## 9. Deferred (이번 슬라이스 범위 밖)

- **slice 2b**: DHCP 클라이언트 (`Internet/DHCP/dhcp.c` ~975줄) + comm_mode==ETH_DHCP 경로 +
  DHCP 소켓·재시도·리스 갱신.
- per-unit MAC 설정, 멀티 소켓/동시 연결, keepalive·socket 옵션 튜닝, INT 구동 RX.
- STATUS 비트 ESTOP/OVLD/OVTIME/OUTERR, DISP_FREQ/ENERGY 라이브화, weld-cycle (RTU와 공통,
  별도 슬라이스).
