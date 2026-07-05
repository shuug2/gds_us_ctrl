# Handoff: 2026-07-05 세션 — HMI SP1 구현 완료(별도 repo) · FW 무변경 · 다음 = 벤치 E2E 세션

**Generated**: 2026-07-05 (세션 마감)
**Branch**: `main` (코드 무변경, **origin 동기화 완료** — 7-04 handoff의 "push 미실행"은 stale, 이미 push됨)
**Status**: FW = HW-게이트 백로그만 잔존. **다음 세션 = 벤치 E2E** (HMI SP1 Task 8 우선).

> **요약**: 이 세션은 FW 코드를 건드리지 않았다. ⑴ **PC HMI(`~/dev/work/gds_us_hmi`, 별도 repo)** — Modbus 계약 재동기화(펌웨어 `e9de17a` 기준) → SP1 brainstorm 종결(QML·단일장치·스파이크-우선) → spec/plan → **Task 1~7 subagent-driven 구현 + 최종 리뷰 "Ready to merge: Yes"** (tip `c196c73`, 미머지; 스파이크로 실보드 FC03 왕복 실증, OUT_POWER=56). 잔여 = **Task 8 실보드 E2E 5항목(벤치 게이트) 후 머지 = SP1 종료**. ⑵ FW 측 확인 2건: LCD RESET/SEEK→OSC는 hook stub(의도된 이연, B-SEAM 게이트) 재확인 / **B-SEAM 준비사항 정리**(아래 표). ⑶ **M6/M8/M9 트리거 발화** — 잔존 조건 "HMI 착수 시"가 성립 → modbus-tcp-hardening이 HW-불요 FW 코딩 후보로 부상.

## Goal (다음 세션)

**벤치 E2E**: ① HMI SP1 Task 8 (5항목, gds_us_hmi 폴더 세션) → PASS 시 gds_us_hmi main 머지. ② 같은 벤치에서 여유 시 FW weld 사이클 E2E(배선 필요) 또는 B-SEAM 측정(스코프 필요).

## Completed (이 세션)

- [x] HMI 계약 재동기화 — `gds_us_hmi/docs/2026-06-14-pc-hmi-brainstorm-summary.md` §3 = `e9de17a` 기준 (주소/클램프 불변, RESET·SEEK 실동작, STATUS 4비트 활성, work_cnt dormant, write read-back 필수 노트)
- [x] HMI SP1 Task 1~7 + 리뷰 루프(계획 코드 결함 5건 수정) + 최종 전체-브랜치 리뷰 — **상세 정본 = `~/dev/work/gds_us_hmi/HANDOFF.md`**
- [x] LCD RESET/SEEK→OSC 확인: FSM/아이콘 완성, `app_seek_reset_hook_signal()`=stub(mon 로그), 물리 PB10/PB2는 boot-init만 구동 — **버그 아님, B-SEAM 이연 그 자체** (`fw/src/app_seek_reset.c:42-48`)
- [x] B-SEAM 준비사항 최신화 (정본 = `docs/superpowers/analysis/2026-06-20-bseam-osc-signal-chain-and-port-fidelity.md` §5/§6 + 아래 델타)

## Not Yet Done (FW 백로그 — 기존 + 신규 1건)

- [ ] **HMI SP1 Task 8 E2E** (벤치) — 최우선. 진입 = gds_us_hmi 폴더 세션 + 그쪽 HANDOFF.md
- [ ] weld 사이클 E2E — 양손 SW_START1/2(PC12/PB11)·SENSE_DN/UP(PA11/PA12)·f_safty **배선 게이트** (spec §7.3 1~6·8 + SETUP-overload SOL 노트 + EMSW 해제-추종 d' 이월)
- [ ] B-SEAM OSC 물리 구동 — 실 rig+스코프 (아래 준비사항)
- [ ] 6b calibration(ch1 절대값·에너지 절대 E2E 포함) / overload 실동작 — rig 게이트
- [ ] **[신규·HW 불요] modbus-tcp-hardening (M6/M8/M9)** — "HMI 착수 시" 조건 발화. HMI SP2(FC06 쓰기) 전 처리 권장. 겸사: **"RS-485 첫 write 간헐 무효"의 FW 측 원인 조사** 포함. 상세 복원 = 2026-07-02 감사 HANDOFF(git 이력, `git log --oneline --all -- HANDOFF.md`로 해당 시점 검색) + `docs/superpowers/specs/2026-07-04-eth-reapply-m7-design.md:129`

## Failed Approaches (Don't Repeat These)

- 이 세션 FW 측 없음. HMI 측(QVariant include 함정, screencapture 권한, 계획 코드 결함 4건 = "계획이 아닌 소스가 정본")은 `gds_us_hmi/HANDOFF.md`에 기록.
- 7-04 벤치 함정 5건(START/STOP 별도 레지스터, 첫 write 무효, 조작-폴링 협응, mbpoll -r 1-based, SWD 정적 read 형식)은 **Warnings로 승계** — 다음 벤치 세션에서 그대로 유효.

## B-SEAM 준비사항 (요약 — 정본은 분석 문서 §5/§6)

**2026-06-20 분석 이후 델타**: F2(U4 I2C_POT 진폭 실구동) **해소·머지됨**(6-28) / 3채널 매핑(RUN→PB14, SEEK→PB2, RESET→PB10, active-LOW 레벨 미러) **확정 — 측정 게이트 아님** / OD 전기설정·boot-init 머지 / PB1=ch1 소비전류 repoint 머지.

| 구분 | 내용 |
|---|---|
| HW 준비물 | **스코프**(⚠ 파워단 고전압·비접지 가능 → 차동/절연 프로브, GND 클립 전 확인) + **실 트랜스듀서/혼 달린 rig**(공진=음향 부하 의존) + 관측 채널(addr=NONE 또는 ETH; SWD 정적 1회·halt 금지) + (최단경로) **동작 원본 M16 보드 가용 여부 확인** |
| 남은 측정 미지수 | ① **SEEK/RESET 스윕 주체**(보드-side vs MCU 생성 — 코드 규모 좌우, 최대 미지수) ② active 지속시간/파형(600ms 레벨 미러 가설) ③ PB12(OSC2) 방향/용도(유일 미확정 — 출력 구동 금지) ④ 폴라리티 sanity ⑤ (겸사) PB0/PB1 ADC 도메인 → 6b 선행 데이터 |
| 안전 | 부팅 OSC off 극성 / **PB12·PB13 출력 구동 금지** / 최소 진폭+짧은 on-time부터 |
| HW 없이 가능 | spec 골격(스윕 주체 2-시나리오 분기) + 코드 동승분: `app_modbus.c` set_pot stale guard→live accessor(리뷰 예약), `app_seek_reset.c` 1-iter stale run_active 재검토, boot-init↔런타임 핀 소유권 조율, stale 주석 3건 |
| ⚠ 선행조건 | 분석 §7-1: **사용자 오프라인 검토(Artifact) 피드백 반영 후 spec** — 검토 여부 사용자 확인 필요 |

## Current State

- **FW main**: `e9de17a`(+이 docs 커밋), working tree clean, origin 동기화, 태그 `-weld4`까지. 미머지 브랜치 없음(`feat/physical-io-slice-a/c`+`backup/pre-d5-*`는 참조 보존).
- **보드**: merged main 코드 적재, **SERIAL/addr=1/9600/EVEN/multi**, OUT_POWER=56, FRAM ether_ip=.199. 리그: OSC 신호 상시(PA0), B_START/B_RESET/PC11 배선, weld 양손/센서/EMSW 미배선.
- **HMI**: branch `feat/sp1-connection-readonly-monitor` tip `c196c73`(+handoff `26048e9`), 미머지, 테스트 17슬롯 green, Qt 6.11.1 설치. RS-485 어댑터 `/dev/tty.usbserial-AB0MLYXA`.

## Resume Instructions (다음 세션)

**A. 벤치 E2E (권장)** — `~/dev/work/gds_us_hmi/`에서 세션 시작 → `HANDOFF.md` 읽기 → Task 8 E2E 5항목(단계별 절차·기대값은 그 문서에) → 전부 PASS → finishing-a-development-branch로 main 머지 = SP1 종료.
1. 보드 전원 ON, `mbpoll -m rtu -a 1 -b 9600 -P even -t 4 -r 1 -c 8 -1 /dev/tty.usbserial-AB0MLYXA` — Expected: reg7=OUT_POWER≈56 (첫 폴 1회 타임아웃 후 성공은 기지 현상)
2. `cd ~/dev/work/gds_us_hmi && cmake --build build && ./build/gds_us_hmi` → E2E-①연결/폴링 ②LCD 터치 START 표시 ③전원 OFF→"무응답"→ON 복귀 ④어댑터 분리(미연결 vs 무응답 **기록**) ⑤30분+leaks 2회

**B. HW 없으면** — 이 repo에서 modbus-tcp-hardening(M6/M8/M9) brainstorming부터 (§3 절차, M6/M8/M9 상세는 git 이력에서 복원).

**C. FW 벤치 잡히면** — weld 사이클 E2E 배선 세션 또는 B-SEAM 측정(위 준비사항 + §7-1 선행조건 확인 후).

## Warnings (벤치 함정 승계 + 신규)

1. **Modbus START(0x1B)·STOP(0x1C) 별도 레지스터** — START에 0 write = no-op. STOP = `-r 29`에 1 write.
2. **RS-485 첫 write 간헐 무효** — 매 write "Written" 확인, 재시도. (FW 원인 조사는 M6/M8/M9 슬라이스에)
3. **조작↔폴링 협응 실패 잦음** — 육안 1차 판정, 협응 필요 시 90s+ 창.
4. mbpoll `-r`은 1-based(wire+1): OUT_POWER=wire6→`-r 7`.
5. SWD 정적 read = `openocd -c "init; echo [read_memory ADDR 32 N]; shutdown"` (halt 금지).
6. HMI E2E-② 중 앱이 포트 점유 → mbpoll 동시 사용 불가, START는 LCD 터치로.
7. LCD RESET/SEEK의 OSC 무동작은 **의도된 이연** — B-SEAM 전까지 고치려 하지 말 것.
8. 펌웨어에서 레지스터/STATUS 의미 변경 머지 시 **HMI 계약 문서 §3 재동기화 필수**.
