# Handoff: 전면 감사 세션 — 포팅 충실성 + 코드 품질 리뷰 (수정 0건, 보고만)

**Generated**: 2026-07-02
**Branch**: `main` (`4fcf7e8`, == origin/main — **push 완료 상태**, 이전 handoff의 "push 미완"은 stale이었음)
**Status**: 감사 완료 / 수정 미착수 — 다음 세션 = 아래 발견 수정 작업

> **요약**: 이번 세션은 READ-ONLY 전면 감사 2종. ① 프로젝트 상태·레거시 포팅 충실성·미머지 브랜치 감사 → 레거시 실거동과 어긋나는 코드 2건 + 이전 handoff 문서의 사실 오류 4건 발견. ② main 전체 코드 품질 리뷰(영역별 cpp-reviewer 4개) → **CRITICAL 1 + HIGH 4 + MEDIUM 9 + LOW 5** 발견. 코드/문서 수정 일절 없음. 이 문서가 수정 백로그의 단일 진입점.

---

## Goal

사용자 요청: ① 프로젝트 진행정도·남은작업·잘못된 내용 비판 보고 ② main 코드 품질 전면 리뷰. 둘 다 보고만, 수정 금지.

## Completed

- [x] 진행도 확인: 코딩 가능 스테이지 전부 main 머지 완료(태그 14개), host 6스위트 실재, main==origin 동기
- [x] 레거시 포팅 충실성 감사 (samd20 + atmega16, 현존 disasm 재검증 포함)
- [x] 미머지 브랜치 5개 divergence 감사 (merge-tree 실측)
- [x] main 전체 코드 품질 리뷰 (코어FSM / LCD / Modbus·ETH / 플랫폼·드라이버 4영역)
- [x] HANDOFF + 메모리 기록 (이 문서)

## Not Yet Done (수정 백로그 — **2026-07-02 사용자 결정 D0~D6 반영, 정본 = `docs/NEXT_STEPS.md` §1.3**)

다음 코딩 세션 실행 순서 (전부 HW 불필요, 검증만 보드):
- [x] **D0** ✅ 완료 2026-07-02 (`eabeab0`, cpp-reviewer APPROVED): `app_lcd_input.c` dispatch `data_len<3` 가드
- [x] **D1** ✅ 완료 2026-07-02 (`85811fc`, cpp-reviewer APPROVED): `SR_TICKS` 50→60 + host 테스트 경계 갱신
- [ ] **D3** 'fram-i2c-robustness' 슬라이스 — 📐 **spec 완료 2026-07-02** (`docs/superpowers/specs/2026-07-02-fram-i2c-robustness-design.md`): H3 `fram_read_*` bool(addr,*out) 전파 + defaults 선적용-덮어쓰기 로드 + **INIT_FLAG 읽기실패=factory-write 금지**(데이터손실 경로 차단) · H2 init 1회 SCL 9클럭 unstick + mon 표면(부팅 [cfg] 확장+1s err 델타). host=mock-fram 신규 스위트 test_app_config 6시나리오. **다음 = writing-plans → subagent-driven 구현**
- [ ] **D6** M7: LCD static IP 저장→가동 중 W5500 즉시 반영 경로 (`app_lcd.c:54-61` hook stub → app_eth 재적용 API, 소규모 설계 필요)
- [ ] **D5** reconcile 선행 (b→d→ch1): 각 브랜치→현 main rebase + `app_reg_tick` 3-way 시그니처 semantic 통합 + board.c 병합, 빌드+host PASS까지. **머지/태그는 HW 검증 후**
- [ ] stale 주석 동승 정정: `app_reg.h:42` "no-op" / `app_modbus.c:105,109` / `fw/test/Makefile:1-3` — 해당 파일 첫 코드 커밋에 포함

스테이지 게이트 (결정 반영):
- [ ] **D4** weld slice4 **첫 Task = H1 근본수정**(WELD 진입 시 모드 래치 + 상태전이 카운터 리셋, `app_weld_fsm.c:111-161`)
- [ ] **D2** slice4 must-fix = **M1~M4 전체**(LV_* 필드군 클램프 + FRAM comm idx 클램프 + `app_weld.c:64-81` uint8 절단 + limit_energy=0 하한) — OUT_POWER 1개에서 확장
- [ ] H4+IWDG 별도 슬라이스 (ADC 영구 lock 정책은 IWDG와 함께 재검토)
- [ ] M6/M8/M9 = HMI 착수 시 'modbus-tcp-hardening'
- [ ] 기존 HW-gated 계속: B-SEAM(잔여=SEEK/RESET 2선+극성), 6b, overload — `docs/NEXT_STEPS.md` §1.2

## Failed Approaches (Don't Repeat These)

- 없음 (READ-ONLY 세션). 단 리뷰 에이전트 1개가 스트림 오류로 최종 출력 유실 → SendMessage 재개로 복구(재실행 불필요했음).

## 감사 발견 ① — 레거시/문서 대조 (2026-07-02)

**코드가 레거시 실거동과 어긋남**:
1. **seek/reset 600ms vs 500ms off-by-one**: 레거시 `us_reset_cnt++` 후 `> MAX_US_RESET_CNT(5)`, 0-시작 100ms 단위(`ref/samd20/main.c:5388-5409`, `:118`) = cnt 6에서 트립 = **600ms/leg**. fw `s_elapsed >= SR_TICKS(50)` @10ms = **500ms/leg**(`app_seek_reset_fsm.c:44-47`). 레거시 주석 `// x 100mS` *의도*(500ms)엔 맞고 *코드 실거동*(600ms)엔 불충실. 3구간(RESET dwell/RESET→SEEK/SEEK해제) 전부. 충실화=SR_TICKS≈60. **수정 여부 사용자 결정**(실 rig에서 100ms dwell 유의미성).
2. **limit_out_time=0 이중 처리**: `reg_energy_termination`=즉시 OVTIME(레거시 충실) vs `weld_backstop_ticks`(`app_weld_fsm.c:40-42`)=1s floor(레거시 이탈). 두 에너지-종료 경로 0-핸들링 상반.

**이전 handoff(2026-06-28 d)의 사실 오류 — 이 문서로 대체**:
3. ~~"main cffeaaf board.c == slice-d 8006e9c byte-identical"~~ → **거짓**(blob 상이, 47줄 차이). OSC 전기설정(OD/active-LOW/PB2|PB10|PB14) *부분만* 동일. slice-d=superset(heartbeat 제거+board_osc4/reset/seek 추가)은 맞음 → **board.c는 slice-d 머지 시 충돌 확정**.
4. ~~"OSC 명령 제어 seek/reset/RUN 전부 미구현"~~ → **부분 오류**. SEEK→PB2/RESET→PB10 런타임=stub 맞음. **RUN→PB14(board_osc4)는 slice-d에서 실배선**(slice-d `app_reg.c:260` `app_reg_hook_us_output` 실호출). B-SEAM 잔여=SEEK/RESET 2선+극성/스코프 검증.
5. ~~"main ahead origin, push 미완"~~ → main==origin/main==4fcf7e8.
6. ~~"physical-io a→b→c→d 스택"~~ → 실측 ancestry: **a ⊂ c ⊂ d 체인 + b는 독립**(2026-06-21, 최고령 standalone — 방치위험 1순위). 실질 통합 단위=**{d, b, output-power-graph-ch1} 3개뿐**(a/c는 d에 포함, 개별 머지 불필요).

**미머지 통합 충돌 실측**(merge-tree): 3단위 모두 `app_reg.c/h`+`app.c` 충돌. **app_reg_tick 시그니처 3-way 분기**: main(`const reg_run_limits_t*`) vs slice-d(`uint16_t limit_on_time, uint8_t model_type`) vs ch1(`uint16_t limit_on_time, int16_t cal_val`) — 텍스트 병합 불가, semantic reconcile 필수. 순차 머지 시 매번 재충돌 → 선머지 기준 rebase.

**문서 남은작업 누락(안전)**: 물리 ESTOP(slice-d에 구현돼 있으나 §1.2에 연결 미기재)·부저(slice-a에 있음, 동일)·f_safty CYL1 abort 강제(config만 포팅)·SYS_HORN 실동작. 미해소 잔여: PC1 3자 충돌, PB12(OSC2) 방향, 레거시 blink-phase/PA4 display-period 폐기 미기록. provenance: regulation-core 문서 인용 `ref/atmega16/M16_reverse/out/*` repo 부재(사실은 현존 `m16_disasm/firmware_disassembled.asm`으로 재검증 완료 — 유효).

## 감사 발견 ② — 코드 품질 리뷰 (main, 4영역)

### CRITICAL
- **C1** `app_lcd_input.c:667-682`: dispatch가 `data_len` 미검증으로 `f->data[1..2]`(미초기화 스택/직전 프레임 잔재) 사용 → 노이즈 프레임(LEN=4)이 KEY_MULTI START/DATA_SAVE/LV_* 로 흐를 수 있음. 정상 프로토콜에선 불발, **EMI 깨진 프레임에서만** — 초음파 용접기라 노이즈 전제. 가드 패턴은 `dgus_lcd.c:298`에 이미 존재(이식 누락).

### HIGH
- **H1** `app_weld_fsm.c:111-161`: 런중 EN_MULTI/EN_ENERGY 토글 시 stale `s_multi_stage/s_multi_elapsed`/`s_temp_time` 오판 → 조기 종료/FAULT abort. 현재 weld dormant라 잠복 — **slice4 전 필수**.
- **H2** `i2c1.c:47-65`: 버스 stuck 복구 전무(FRAM+POT 공유 → 동반 마비, save_all 최대 ~1.5s 루프 정지) + `i2c1_err_count()` 소비처 0(죽은 observability).
- **H3** `fram.c:5-25`+`app_config.c:79-122`: read 실패 무시 → 필드 조용히 0 로드(limit_on_time=0/limit_out_time=0→즉시OVTIME/output_power=0). 전체 고장은 factory reset로 안전, **부분 실패가 최위험**.
- **H4** `adc1.c:34-52`: 폴링 실패=무조건 `Error_Handler()`(`irq.c:17` `__disable_irq;while(1)`) 영구 lock, IWDG 부재. 의도된 트레이드오프(주석 有)이나 백스톱 없음. `HAL_ADC_Stop` 반환 무시=지연-치명 전이 가능. B-SEAM 배선 시 재평가 필수(현재는 OSC 미구동이라 "출력 켜진 채 고착" 불가).

### MEDIUM (9)
- M1 `app_reg_calc.c:63-76`: `limit_energy=0`+energy_ctrl → START 즉시 무증상 "정상완료"(하한 가드 없음)
- M2 `app_weld.c:64-81`: `limit_mo_out1/2` uint16→uint8 무검증 절단(LCD 무클램프와 결합: 300→44→진폭0 silent)
- M3 `app_lcd_render.c:144,179`+`app_config.c:111-113`: FRAM 로드 comm_speed_idx/parity_idx 무클램프 배열 인덱스 → OOB flash read(터치 경로엔 가드 有=비대칭)
- M4 `app_lcd_input.c:710-762`: LV_* 필드군 LCD 터치 무클램프(레거시 유래 — slice4 must-fix를 OUT_POWER 1개→필드군 전체로 확장)
- M5 `fram.h`: 레이아웃 버전 필드 부재(INIT_FLAG는 초기화 여부만) — M3와 결합 시 실질 위험
- M6 `app_modbus_tcp.c:42-59`: coalesced/파이프라인 TCP 프레임 2번째 요청 폐기(mbpoll 무관, SCADA류 간헐 타임아웃)
- M7 `app_lcd.c:54-61`+`app_eth.c:204-207`: static IP 변경·저장이 가동 중 W5500 미반영(재부팅/링크 재협상 필요)
- M8 `app_modbus.c:239-286`: ETH→SERIAL 전환 시 TCP 소켓 미정리(ESTABLISHED 방치)
- M9 `app_modbus_tcp.c:29,35,66`: blocking 소켓+send/disconnect 반환 무시, CLOSE_WAIT `disconnect()` 최대 ~1.6s 루프 블로킹

### LOW (5)
`weld_backstop_ticks` 폭 확장 부재(주석 "≤10"이 LCD 실상 255와 불일치, 현재 우연히 안전) / `usart1.h` 주석 폐기된 IT 모델 서술(실제 DMA circular) / `_sbrk` 힙-스택 미검사(힙 미사용) / `energy2str` 정확히 100000에서 자릿수 1개 누락(레거시 verbatim) / ether 필드 미선택 숫자키→VP 0x2460 오발신(UX)

### 결함 없음 확인
Modbus 코어 경계/CRC(samd20 unbounded write는 포팅 때 수정됨)·RTU DMA 링버퍼·MBAP 검증 체인·부팅 init 순서·ISR 공유변수(단일 writer 32bit)·tick 32bit 랩 안전·seek/reset FSM 전이/직교·DGUS 파서 자체 경계·TX 빌더.

## Key Decisions

| Decision | Rationale |
|----------|-----------|
| 수정 0건, 보고만 | 사용자 명시 지시 ×2 |
| C1을 유일 CRITICAL로 | 미초기화 데이터가 START 명령으로 흐를 수 있는 유일 경로, EMI 환경 전제 |
| seek/reset off-by-one은 수정 아닌 결정사항 | 레거시 "주석 의도 500ms vs 코드 실거동 600ms" 중 무엇에 충실할지는 실 rig 판단 필요 |
| H1은 slice4 게이트로 | 현재 dormant(트리거 미연결)라 실기 위험 0, 물리 트리거 연결이 활성화 조건 |

## Current State

**Working**: main 빌드/host 전제 무변경(이 세션 코드 무수정). 보드=ovtime fw, SERIAL/addr=1/9600/EVEN.
**Broken**: 없음 (발견들은 잠복/경계 결함).
**Uncommitted**: `?? ref/signal/`(Saleae 캡처, 의도적 미추적)만.

## Files to Know

| File | Why It Matters |
|------|----------------|
| `fw/src/app_lcd_input.c:667-682` | C1 수정 지점(1줄) + M4 클램프 필드군 |
| `fw/drivers/fram.c` + `fw/src/app_config.c` | H3 — status 전파 리팩터 지점 |
| `fw/drivers/i2c1.c` | H2 — bus recovery + err_count 표면 |
| `fw/src/app_weld_fsm.c:111-161` | H1 — slice4 전 필수 |
| `fw/src/app_seek_reset_fsm.c:44-47` + `ref/samd20/main.c:5388-5409` | off-by-one 양쪽 근거 |
| `docs/NEXT_STEPS.md` | §1.1 표에 i2c-pot/ovtime 누락 등 정정 대상 |

## Resume Instructions

1. sanity: `env -u STM32_TOOLCHAIN cmake -S fw -B fw/build -G Ninja && env -u STM32_TOOLCHAIN cmake --build fw/build`(0-warning) + `make -C fw/test test`(6스위트 PASS).
2. 수정 착수 순서 = 위 "Not Yet Done". C1은 1줄+회귀(host 스위트 무영향, LCD 터치 정상경로 HW 확인)로 독립 커밋 가능.
3. H3는 시그니처 변경(전파형)이라 spec→plan→subagent 절차(§NEXT_STEPS §3) 권장. H2는 같은 슬라이스로 묶기 자연스러움.
4. seek/reset off-by-one은 **먼저 사용자에게 물을 것**(600ms 충실 vs 500ms 유지).
5. 미머지 통합 시: slice-b 결정 최우선(독립·최고령) → slice-d(a/c 자동 포함, app_reg_tick semantic reconcile + board.c heartbeat/OSC 병합) → ch1.

## Warnings

- ⚠ 이 문서 이전 버전(2026-06-28 d)의 4개 주장(byte-identical/RUN 미구현/push 미완/a→b→c→d 스택)은 **실측으로 반증됨** — 위 §감사 발견 ① 3~6이 정본.
- ⚠ `fw/build/`·`fw/build-trace/`에 미머지 브랜치 소스 오브젝트 잔재(app_overload/app_buzzer 등) — GLOB 이력, main 소스 실체와 불일치(감사 시 혼동 유발, 실해 없음).
- ⚠ 검증 규칙 유지: 런타임=mbpoll/LCD만, SWD halt 금지(정적 1회만). 브랜치 전환 시 cmake reconfigure.
- ⚠ git 해시는 2026-06-20 filter-repo 재작성 — 안정 레퍼런스는 태그.
