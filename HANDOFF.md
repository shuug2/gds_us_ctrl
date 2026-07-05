# Handoff: 2026-07-05 c~06 세션 — mbtcp-hardening MERGED(감사 큐 완전 소진) + seek/reset 라이브 표시 + 전류 전달함수 재정의 + overload E2E 종결

**Generated**: 2026-07-06 (세션 마감, 벤치 이어짐 — 후반 이더넷 연결)
**Branch**: `main` tip `51df78e`(+이 docs 커밋) — **⚠ push 미실행** (사람 터미널: `git push origin main && git push origin hw-revA_fw-stage-mbtcp-hardening`)
**Status**: 2026-07-02 감사 큐 **완전 소진**(D0~D6 + 잔존 M6/M8/M9). physical-io 이월 3건 전부 종결. 다음 = 사용자 신규 3건(§백로그 ⓪) / HMI Task 8 / 6b·B-SEAM(보류).

> **요약**: ⑴ **seek/reset 중 측정값 라이브 표시**(`e2003e1`) — legacy `us_on_status` 게이트 복원, 벤치 4항목 PASS. ⑵ **표시 전류 전달함수 재정의**(`54e5220`) — 전일 앵커가 EMA 미정착 오측으로 판명(RUN 실측 표시 1.4A/실제 0.6A), 재앵커 + 사용자 결정 −37 오프셋 제거(GAIN 59/126·OFFSET 0·DEADBAND 20). ⑶ **overload 실동작 E2E 사용자 PASS**(코드 무변경). ⑷ **modbus-tcp-hardening(M6/M8/M9) subagent-driven 풀사이클 + HW E2E 전항목 PASS + MERGED**(`7c474e1`, tag `hw-revA_fw-stage-mbtcp-hardening`) — 최종 리뷰 HIGH 1건(coalesced FC06 apply 굶김)과 벤치 발견 2건(vendored recv 순서-뒤집힘 / stale-ESTABLISHED 락아웃) 포함 해소. ⑸ RS-485 첫-write 조사 보고서 커밋.

## 핵심 성과 (다음 세션에 영향 주는 것)

1. **감사 큐 종결**: 2026-07-02 감사의 모든 결정(D0~D6)이 main에 반영·태그됨. NEXT_STEPS §1.3 표 전 행 ✅.
2. **Modbus TCP 강화 완료**: coalesced/파이프라인 프레임 처리(워커 `mb_tcp_frame_peek` + FC06-후-워크-종료 = 1-write-per-mirror 불변식), ETH→SERIAL 소켓 정리, 논블로킹+CLOSE_WAIT 벨트, W5500 keep-alive 10s. **HMI SP2(FC06 쓰기) 전제 조건 충족**.
3. **⚠ vendored socket.c recv() 함정 (재발 방지 필수 지식)**: 이 벤더 드롭의 비-IPv6 recv()는 논블로킹 체크가 recvsize 체크보다 앞(`socket.c:687-692`) — **NONBLOCK 소켓에선 데이터가 있어도 무조건 SOCK_BUSY**. 업스트림/IPv6 분기와 다름. 우회 = recv 구간만 `ctlsocket(CS_SET_IOMODE)` 블로킹 토글(`app_modbus_tcp.c` 주석). W5500 소켓을 새로 만들 때 이 함정 반드시 상기.
4. **선재 결함 완화**: app_eth `ETH_STATIC_UP`은 링크 재폴링 없음 → 케이블 분리 시 stale-ESTABLISHED 단일소켓 영구 락아웃이던 것을 KA 10s로 자가치유(~20s). 근본(링크 FSM 확장)은 후속 후보.
5. **표시 전류 = 순수 비례**: `disp = ch1×59/126 + cal` (오프셋 0, 데드밴드 20=표시 플로어 0.21A). 앵커 600mA↔ch1≈126↔표시 60(+cal 1). 캘리브레이션 이력/근거는 `app_reg_calc.c:80-93` 주석.

## Not Yet Done (백로그)

- [ ] **⓪ 사용자 신규 3건 (2026-07-06 등록 — 상세는 작업 시작 시 사용자 설명 예정)**:
  1. 부팅 딜레이 시 터치 에러
  2. 원격제어 시 REMOTE icon 점등 (참고: samd20 DISP_REMOTE 표시는 Stage C 때 의도적 스킵)
  3. 전류 표시 필터 반응 ×2 — 현 EMA α=1/8 per 50ms(τ≈400ms, `app_reg.c` reg_acquire_step) → τ≈200ms 방향. ⚠ EMA↔에너지 적분 커플링(6b MEDIUM)과 교차.
- [ ] **전류 표시 0.60A 재확인** — `54e5220` 후 명시 벤치 확인 미기록 (다음 벤치 첫 항목: RUN+전류계 0.6A↔표시 0.60, 유휴 0.00)
- [ ] **HMI SP1 Task 8 실보드 E2E** — gds_us_hmi 폴더 세션, RS-485 필요. **병행: RS-485 첫-write 재현**(`docs/superpowers/research/2026-07-05-rs485-first-write.md` §6)
- [ ] **6b 잔여** ⏸사용자 보류 — 전류 다점/2점 fit(오프셋 제거로 중간 구간 편차 가능), ch0 도메인, 에너지 절대+divisor, EMA↔적분 디커플링, OUTERR
- [ ] **B-SEAM 잔여** ⏸사용자 보류 — 파형 정밀·PB12·진폭 추종
- [ ] 후속 소소: app_eth STATIC_UP 링크 재폴링 / KA 무송신-피어 잔여 / defer Minor(ledger `.superpowers/sdd/progress.md`) / handle_key_multi RESET OVLD 비트 휘발성(LOW)

## Warnings / 벤치 노트

1. **vendored recv() NONBLOCK 함정** — 위 핵심 3. 신규 소켓 코드 작성 시 최우선 확인.
2. **mbpoll 주소 지정**: 1-based(-r N = wire N-1)가 이 프로젝트 관행. `-0` 혼용하면 엉뚱한 레지스터에 쓴다(이번 세션 원복 실수 1회 — ENERGY(wire 8)에 오기입될 뻔, 실제론 write 실패로 무해).
3. **M8 검증 함정**: LCD comm_mode 변경은 **저장까지** 해야 반영 — 미저장 상태를 SWD `g_cfg.comm_mode` 정적 read로 진단(주소는 현재 ELF에서 재확보).
4. 기존 승계: 플래시↔ELF 벡터 대조 / SWD halt 금지·비침습 샘플러(~1.4ms/샘플) / START(0x1B)·STOP(0x1C) 별도 레지스터 / stty 잔재 / LCD 터치+B_START 웨지→전원사이클.

## Current State

- **FW main**: `51df78e` (merge `7c474e1` + changelog). 태그 신규 = `hw-revA_fw-stage-mbtcp-hardening`. **push 미실행(18±커밋+태그)**.
- **보드**: main 최신 코드 플래시(=`434e007` tip과 동일), 풀배선 리그 + **이더넷 연결, ETH_STATIC 192.168.1.199**. OUT_POWER=56/ON_TIME=56 원복, cal_val=1, 나머지 잔재 = NEXT_STEPS §2.3-a.
- **리뷰**: 스테이지 전 커밋 리뷰 통과(태스크별 + whole-branch opus + 벤치 fix 사후 리뷰 양건 APPROVE). E2E 스크립트 3종 = plan Task 4(재사용 가능).
- **ledger**: `.superpowers/sdd/progress.md` = mbtcp-hardening 전 과정 + defer Minor 목록.

## Key Decisions

| Decision | Rationale |
|----------|-----------|
| seek/reset off-엣지 last_freq 래치 없음 | legacy 충실 (사용자 선택 — 종료 후 이전 런 주파수 복귀) |
| 전류 −37 오프셋 제거(순수 비례) | 사용자 결정; 데드밴드 51→20 재정의 동반(구 51은 −37 도메인) |
| FC06-후-워크-종료 | 최종 리뷰 HIGH — apply 체인 one-change-per-call + 클램프 잔여로 뒤 write 유실; RTU 동일 불변식 복원이 최소·정확 |
| recv만 블로킹 토글(vendor 무수정) | fw/vendor read-only 원칙; RSR>0 가드로 무스톨 구조 증명(리뷰 확인) |
| KA 10s (app_eth 링크 FSM은 무수정) | 스테이지 비범위(spec §4) — 칩-레벨 완화가 최소·충분, 근본은 후속 |

## Resume Instructions

- 코딩(HW 불요/간이) → **⓪ 사용자 신규 3건** — 시작 시 사용자에게 상세 설명 요청 후 brainstorming(§3 절차; 3번 필터는 소규모라 벤치-수정 관례 가능).
- HMI → gds_us_hmi 폴더 + 그쪽 HANDOFF.md (RS-485 연결; 이 repo research/의 첫-write 재현 절차 지참).
- FW 벤치 → 전류 0.60A 재확인 → (해제 시) 6b 다점 / B-SEAM.
