# Handoff: D5 스택 HW 검증 + 전단위 머지 완료 (b'→d'→ch1'→slice4, 태그 4개) — 다음 = push + weld 사이클 E2E 배선 세션

**Generated**: 2026-07-04 (g 세션 마감)
**Branch**: `main` (`b571da0`, **origin ahead 61+ — push 미실행, 사람 SSH**)
**Status**: 미머지 스택 소진 — HW-게이트 백로그는 배선/rig 종속분만 잔존.

> **요약**: 하루 2세션. ⑴ weld 슬라이스4 subagent-driven 10 Task 실행(CODE-COMPLETE, 최종 opus 리뷰 0 Crit/0 Imp, I-1 발견→fix).
> ⑵ 패널 rig HW 세션 — D5 스택 4단위를 순서대로 플래시→검증→`--no-ff` 머지+태그:
> b'(`0cc34a8`, `-physio-b`) / d'(`46055c9`, `-physio-d`) / ch1'(`e973721`, `-power-ch1`) / slice4(`b571da0`, `-weld4`).
> 게이트 GREEN: main host 13스위트 PASS + our-code 0-warning. 감사 D0~D6 + D2/D4 코드·머지 전부 종결.

## HW 검증 결과 (패널 rig, 인터랙티브)

- **b' FREQ_IN**: 리그 OSC 실신호 **34.46kHz** 측정(런 중 34452~34479Hz, ±15Hz) + 정지 시 last_freq 유지 + LCD VAR_FREQ ~344 육안 + ceiling 회귀 PASS.
- **d' 물리 명령+E-stop**: B_START hold-to-run(STATUS 0→1→0 실측+ICON_RUN) / B_RESET→RESET→SEEK 체인 육안 / PC11 SEEK(multi) / **E-stop(std)**: idle-HIGH 즉시 활성(STATUS=0x02, 극성 스펙 일치)+START 차단+multi 복귀 자동 해제(RESET 불필요). **거동 변화 2건 실증**: TOUCH 운용 ceiling 제외(hold 중 계속 동작) / hand COMM=560ms·REMOTE=누르는 중 자동정지 / multi COMM 30s 절대 캡 실동작.
- **ch1'**: 회귀 PASS. ch1 절대값 표시 = **6b 이연** — 런 중 SWD 정적 read `ch1_avg≈2`(노이즈 플로어) = 리그 전류-sense 아날로그 무신호(ch0도 0 → 가시적 회귀 없음). 에너지 적분 교차영향은 머지 커밋에 명기(spec 조건).
- **slice4**: ceiling 회귀 + Modbus OUT_POWER 클램프(120→100/30→50/원복) + **LCD M4 클램프 에코 육안**(delay1 600→500 / OUT_POWER 49→50=LOW-1 경로) + work_cnt=0(사이클 dormant 구조).

## 보류/이연 (배선·rig 게이트 — 다음 HW 세션 몫)

- [ ] **weld 사이클 E2E** (spec §7.3 1~6·8): 양손 SW_START1/2(PC12/PB11)+SENSE_DN/UP(PA11/PA12)+f_safty **배선 후** — DELAY/TRIGGER 사이클, 재장전, safety abort, E-stop abort, RESET 체인 게이팅, 사이클 타이밍/multi 스테핑 tick/energy exit. + **SETUP-overload SOL 거동 노트**(setup 체류∧overload 시 SOL 유지→run 복귀 해제 — 인간 승인된 legacy-충실 거동 확인).
- [ ] **EMSW 물리 해제-추종** (d' 이월): E-stop 라인 배선 후 누름=해제/뗌=재활성 레벨-추종 확인 (활성·차단·자동해제는 이번에 검증됨, host-test 커버).
- [ ] overload 실동작(합성 곤란, slice-c부터 이연) / B-SEAM OSC 물리 구동 / 6b calibration(ch1 절대값+에너지 절대 E2E 포함).
- [ ] push (사람 SSH — main ahead 61+ 커밋 + 태그 4개 신규).

## Failed Approaches / 벤치 함정 (반복 금지)

1. **Modbus START(0x1B)와 STOP(0x1C)은 별도 레지스터** — START 레지스터에 0 write는 no-op(정지 아님). 이 착오로 "정지 불능"을 버그로 오인해 SWD 정적 read까지 감(런 소스 US_COMM 확인) — 실은 레지스터 오용이었고, 그 사이 **30s 절대 캡이 우연히 실증**됨. STOP = `-r 29`(wire 0x1C)에 1 write.
2. **RS-485 첫 write 간헐 무효** — 같은 명령 재시도로 해소. 매 write마다 "Written N references" 출력 확인 필수.
3. **사용자 조작↔컨트롤러 폴링 협응은 타이밍 실패 잦음**(3회 실패) — LCD 모델 전환 등 준비 시간이 캡처 창을 소진. **육안 관찰을 1차 판정으로**, 캡처는 보조. 협응 필요 시 90s+ 창.
4. mbpoll `-r`은 1-based(wire+1): OUT_POWER=wire6→`-r 7` (한 번 `-r 8`=ON_TIME 오기록 — 원복 완료).
5. SWD 정적 read는 `openocd -c "init; echo [read_memory ADDR 32 N]; shutdown"` (halt 없음, `mdw`는 무출력).

## Key Decisions (이 세션)

| Decision | Rationale |
|----------|-----------|
| slice4 = 회귀-기준 머지 (사이클 E2E 이연) | weld1~3 관례 동일; 트리거=물리 입력 전용이라 미배선 상태에서 사이클 발생 불가(dormant) — 사용자 승인 |
| ch1' = 6b-이연 머지 | 전류-sense 무신호 실측(ch1_avg≈2); 파이프라인은 host-test; 스택 구조상 slice4 머지 선행 조건 |
| EMSW 해제-추종 보류 | 라인 미배선; 활성/차단/자동해제(모드 복귀)는 검증 완료 — 사용자 승인 |
| docs 충돌 = 양쪽 이력 보존 | changelog 두 항목 병존, NEXT_STEPS D4=slice4판+D5=main판, RESUME f-블록 삽입, HANDOFF=신판 채택 |

## Current State

- **main `b571da0`** = 감사 큐 전체 + physical-io b/d + ch1 + weld slice4 전부 반영. 미머지 브랜치 없음(잔존 `feat/physical-io-slice-a/c`는 d'에 흡수된 pre-D5 원본, `backup/pre-d5-*` 3개와 함께 참조용 — 삭제는 사용자 판단).
- **보드**: merged main과 동일 코드(slice4 tip 플래시본) 적재. multi 모드/SERIAL/addr=1/9600/EVEN, OUT_POWER=56(원복 확인), FRAM ether_ip=.199. 리그: OSC 신호 상시 공급(PA0), 패널 버튼 B_START/B_RESET/PC11 배선, **weld 양손/센서/EMSW 미배선**.
- 다음 세션 진입 = `docs/NEXT_STEPS.md` §1.2 + 이 문서. 세션 로그 = `docs/superpowers/RESUME.md`(g 블록). SDD ledger = `.superpowers/sdd/progress.md`.
