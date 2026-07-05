# modbus-tcp-hardening (M6+M8+M9) + RS-485 첫-write 조사 설계

> **요약**: 2026-07-02 감사 잔존 MEDIUM 3건 해소 — **M6** 한 recv에 붙어 온
> coalesced/파이프라인 TCP 요청의 2번째 폐기 → 누적 버퍼 + 순수 프레임 워커로
> 순차 응답, **M8** ETH→SERIAL 전환 시 TCP 소켓 ESTABLISHED 방치 → tcp_active
> 하강 엣지에서 소켓/수신상태 정리, **M9** blocking 소켓의 슈퍼루프 스톨(최대
> ~1.6s) → `SF_IO_NONBLOCK` 전환 + CLOSE_WAIT 벨트 타이머. 부가로 벤치 노트
> "RS-485 첫 write 간헐 무효"의 코드 레벨 원인 조사(수정은 원인 확정+저위험일
> 때만). 검증 = host 신규 워커 스위트 + HW E2E(이더넷 벤치, 사용자 확정).
> 브랜치 `feat/modbus-tcp-hardening`, 머지+태그 `hw-revA_fw-stage-mbtcp-hardening`
> = HW 게이트.

- 발견 정본: 2026-07-02 감사 HANDOFF (git `5ca13b8:HANDOFF.md` §MEDIUM)
- 트리거: NEXT_STEPS §2.2 — HMI SP1 구현 완료로 D6 잔존 조건 발화, HMI SP2(FC06
  쓰기) 전 처리 권장
- 사용자 결정 (2026-07-05 c): M9=논블로킹 전환(레거시 samd20 process_tcp.c:37도
  blocking — **의도적 강화 편차**) / RS-485 조사 포함(코드 레벨, 벤치 RS-485
  미접속이라 재현은 Task 8 세션) / HW E2E 이번에 진행(이더넷 연결 가능)

## 1. 현황과 갭

`app_modbus_tcp.c` (slice 2a 산물, M7 때 `app_modbus_tcp_reset()` 추가):

- **M6** (`:52-85` poll): `recv` 1회 → `mb_tcp_build_response` 1회. 한 TCP
  세그먼트에 MBAP 프레임 2개가 오면(파이프라인 마스터/Nagle coalescing) 2번째는
  버려지고 응답 없음 → 마스터 타임아웃. 또한 `avail > sizeof s_rxbuf` 클램프
  (`:63-65`)는 프레임 경계 무시 절단이라 잘린 꼬리가 다음 recv에서 앞부분 없는
  garbage가 됨 (기존 spec §3 가정 "recv당 정확히 1 완전 프레임"의 명시적 한계).
- **M8** (`app_modbus.c:311-347` tick): RTU-점유 분기(`:314`)와 eth-불가
  분기(`:346`)가 `g_tcp_active=0`만 하고 소켓 방치 → ETH→SERIAL 전환 후에도
  W5500 sock0이 ESTABLISHED 유지(피어에겐 살아있는 연결, 응답만 없음). M7의
  `app_modbus_tcp_reset()`은 eth_reapply 경로에서만 호출됨.
- **M9** (`:30-50` control_tcp, `:66,76`): 소켓이 blocking 모드(`socket()` flag
  0) — ① `SOCK_CLOSE_WAIT`의 `disconnect()`는 DISCON 후 CLOSED까지 대기, 피어
  무응답이면 칩 RTO(~1.6s)까지 **슈퍼루프 전체 정지** ② `send()`도 dead-peer면
  SEND_OK/TIMEOUT까지 동급 스톨 ③ 반환값 전부 `(void)` — 실패 관측 불가.
  `recv`는 `RX_RSR>0` 가드(`:59-62`)로 실질 논블로킹이라 무관.

## 2. 컴포넌트 변경

### 2.1 `app_modbus_tcp_frame.{h,c}` — 순수 프레임 워커 (M6, host-test)

```c
typedef enum { MB_TCP_FR_NEED_MORE = 0, MB_TCP_FR_OK, MB_TCP_FR_DESYNC } mb_tcp_fr_t;
mb_tcp_fr_t mb_tcp_frame_peek(const uint8_t *buf, uint16_t len, uint16_t *frame_len);
```

- 누적 버퍼 선두에서 완전 프레임 1개의 wire 길이(`6 + be16(buf[4..5])`)를 판정.
- `len < 6` → NEED_MORE. proto id(buf[2..3]) != 0 또는 length 필드 < 2 또는
  length 필드 > MB_FRAME_MAX(=`mb_tcp_build_response :20`의 수용 경계와 동일) →
  **DESYNC** (호출측이 누적 버퍼 전체 폐기 — 스트림 재동기는 마스터 재시도에
  위임). 그 외 `len < frame_len` → NEED_MORE, 충족 → OK.
- 기존 `mb_tcp_build_response`는 무변경 (완전 프레임 1개 입력 계약 유지 — 워커가
  그 계약을 상류에서 보장하는 구조).

### 2.2 `app_modbus_tcp.c` — 누적 수신 + 워커 루프 + 논블로킹 (M6+M9)

- **수신 누적**: `s_rxbuf` → `s_acc[2*(MB_TCP_MBAP_LEN+1u+MB_FRAME_MAX)]`(264B) +
  `s_acc_len`. poll마다 `recv`는 남은 공간만큼(`RX_RSR` 클램프), 누적 뒤 워커
  루프.
- **워커 루프**: poll당 최대 `MB_TCP_FRAMES_PER_POLL=4` 프레임 —
  `mb_tcp_frame_peek` OK → `mb_tcp_build_response`(슬라이스) → 응답을 TX 누적
  버퍼에 append → FC06이면 `app_modbus_apply_writes()` 후 **그 프레임에서 워크
  즉시 종료**(2026-07-05 whole-branch review HIGH 반영: `apply_writes`는
  one-change-per-call else-if 체인이고 클램프 잔여(`holding`=raw vs cfg=클램프)
  재동기가 poll 뒤 `mirror_live()`뿐이라, 같은 poll에서 두 번째 FC06을 apply
  하면 잔여 재발견으로 굶겨 write가 에코만 나가고 조용히 유실됨. FC06-후-종료
  = RTU와 동일한 1-write-per-mirror 사이클 + read-after-write stale 차단 +
  FRAM save poll당 1회 상한). 잔여 프레임은 누적 버퍼로 다음 poll 이월.
  루프 종료 후 **단일 send** — 벤더 논블로킹 send는 직전 send의 SENDOK(피어
  ACK) 미도래 시 SOCK_BUSY를 반환하므로(socket.c:531-550 확인) 프레임별 연속
  send는 2번째부터 드롭됨; MBAP 응답 스트림은 자체 구분되므로 코얼레스드
  1-send가 정확+안전. NEED_MORE → 잔여 `memmove` 선두 이동. DESYNC →
  `s_acc_len=0` (폐기, 이미 빌드된 응답은 send). 상한 도달 시 잔여는 다음
  poll로 자연 이월.
- **논블로킹 전환**: `socket(..., SF_IO_NONBLOCK)`. `send()` 반환 관측 — 전송
  바이트 != out_len(SOCK_BUSY 포함) → **응답 드롭** + mon 로그 1줄(마스터
  재시도가 Modbus 표준 회복 경로; 재전송 큐 없음 = KISS).
- **CLOSE_WAIT**: 논블로킹 `disconnect()`(SOCK_BUSY 즉시 반환) + 벨트 타이머 —
  CLOSE_WAIT 최초 진입 시 `sys_tick_get_ms()` 스탬프, 체류
  `MB_TCP_CLOSEWAIT_MAX_MS=500` 초과 시 `close()` 강제 (poll 횟수 기반은
  슈퍼루프 주기가 가변이라 시간 보장 안 됨). CLOSE_WAIT 이탈 시 스탬프 리셋.
- **연결 엣지 리셋**: `Sn_IR_CON` 소비 시(`:34-36`) `s_acc_len=0` — 이전 피어의
  stale partial이 새 연결 첫 프레임에 붙는 것 차단.
- **`app_modbus_tcp_reset()` 확장**: `close()` + `s_acc_len=0` + CLOSE_WAIT
  카운터 리셋 (M7 호출처 eth_reapply도 자동 수혜).

### 2.3 `app_modbus.c` — tcp_active 하강 엣지 정리 (M8)

- `g_tcp_active` 1→0 전이 두 곳(RTU-점유 분기 `:314`, eth-불가 else `:346`)을
  공통 헬퍼로 — 전이일 때만 `app_modbus_tcp_reset()` 호출 (매 tick 중복 close
  방지; close()는 CLOSED 소켓에 무해하나 SPI 트래픽 절약).
- eth-불가 분기는 `app_eth_available()==0`(링크 다운/칩 부재)에도 진입 —
  링크다운 중 close 시도는 무해(레지스터 쓰기), 링크 복귀 시 CLOSED에서 재오픈
  정상 (기존 FSM 그대로).

### 2.4 RS-485 첫 write 간헐 무효 — 코드 조사 (수정 아님)

정적 분석 대상: `drivers/usart6_mb.c`(DMA circular RX 프레이밍/idle 검출/open
직후 첫 프레임), DE 턴어라운드(하드웨어 자동 vs FW), `mb_core_decode` FC06 →
`app_modbus_apply_writes` 체인(에코 캡처 시점·mirror_live 순서), mbpoll 접속
직후 라인 글리치 바이트 시나리오. **산출물 = 원인 후보 보고**(HANDOFF 기록 +
벤치 재현 계획, HMI Task 8 세션 입력). 원인이 확정적이고 수정이 저위험일 때만
같은 브랜치에 fix 커밋 (아니면 조사 보고로 종료 — 사용자 결정).

## 3. 에러 처리

- DESYNC = 누적 폐기 후 무응답 (마스터 타임아웃-재시도). 연결 강제 종료는 안 함
  (단일 소켓 서버 — 잘못 끊으면 정상 마스터도 재연결 비용).
- send 실패 = 드롭 + mon 로그. FC06 apply는 **응답 send 시도 후** 기존 순서 유지
  (에코는 decode 시점 캡처라 wire 불변 — 기존 주석 `:78-82` 계약 유지).
- 벨트 close()는 graceful FIN 포기(abrupt) — 0.5s 내 DISCON 미완료면 피어가
  이미 비정상이므로 수용.

## 4. 명시적 비범위

- TCP keep-alive, 멀티 소켓, 진짜 스트리밍 재조립(정상 Modbus 마스터 상정 —
  경계 partial 이월까지만), RTU 경로 코드 변경(조사만), 감사 M5/LOW 항목,
  DHCP/링크 FSM(`app_eth.c`) 변경.
- Modbus 코어(`app_modbus_core.c`)와 `mb_tcp_build_response` 무변경.

## 5. 검증

### 5.1 host (신규 + 회귀)

- `test_app_modbus_tcp_frame`에 워커 케이스 추가: 단일 완전 / 2-coalesced /
  3+ 상한 / 경계 분할(NEED_MORE→이어붙임 OK) / proto!=0 / length<2 /
  length 과대 DESYNC / 6바이트 미만.
- 기존 전 스위트 PASS + 0-warning 빌드.

### 5.2 HW E2E (이더넷 벤치 — 머지 게이트)

1. **M6**: 파이프라인 스크립트(python, 한 TCP 세그먼트에 FC03 요청 2개 연속
   write) → 응답 2개 수신. mbpoll 단일 요청 회귀 병행.
   **+ coalesced FC06 케이스(리뷰 HIGH 회귀)**: 한 세그먼트에
   [FC06 OUT_POWER=30(클램프 대상)][FC06 ON_TIME=100] → 응답 2개(에코) 수신
   후 FC03 read-back으로 **OUT_POWER=50(클램프)·ON_TIME=100 둘 다 적용** 확인
   — 워크-종료 이월(두 번째 write는 다음 poll에서 apply)이 유실 없이 동작함을
   입증. ⚠ FC03-only 파이프라인은 이 결함을 못 잡음(리뷰 지적).
   **+ [FC06][FC03 같은 세그먼트]**: FC06 클램프 write + 같은 reg FC03을 한
   세그먼트로 → FC03 응답이 다음 poll(mirror 후)로 지연돼 **클램프된 값**을
   반환하는지 확인 (재리뷰 이월 노트 — stale 미러 스냅샷 아님을 입증).
2. **M8**: ETH 연결(ESTABLISHED, SWD `Sn_SR` read=0x17) → LCD comm_mode→SERIAL
   저장 → `Sn_SR`=CLOSED(0x00) 확인 + 피어 소켓 종료 관측 → ETH 재전환 →
   재연결 정상.
3. **M9**: ESTABLISHED 중 케이블 분리 → FC03 폴링 마스터가 죽은 상태에서 보드
   LCD 반응성 유지 + TCL 샘플러로 슈퍼루프 tick 연속성(>100ms 공백 없음) →
   케이블 복귀 후 재연결.
4. **회귀**: mbpoll -m tcp FC03 미러/FC06 클램프+에코, 직접-초음파 ceiling
   무회귀, SERIAL RTU는 어댑터 미접속이라 mode 전환 무크래시만.

## 6. 구현 단위 (plan 입력)

- **T1**: `mb_tcp_frame_peek` 순수 함수 + host 스위트 (TDD)
- **T2**: `app_modbus_tcp.c` 누적/워커/논블로킹/벨트 + `app_modbus_tcp_reset()`
  확장 + `app_modbus.c` 하강 엣지 정리 (T2 착수 전 벤더 `socket.c`의
  SF_IO_NONBLOCK send/disconnect 반환 계약 grep 선행 확인 — slice 2a 관례)
- **T3**: RS-485 첫 write 조사 보고 (코드 전용)
- **T4**: 통합 cpp-review → HW E2E(§5.2) → 머지+태그
