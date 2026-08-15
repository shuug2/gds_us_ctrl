# NEXT_STEPS — 다음 세션 진입 가이드

> CLAUDE.md 에 명시된 first-load 문서. 새 세션 시작 시 본 파일을 가장 먼저 읽고 진행 상황 + 다음 작업을 확인.
>
> **본 문서 최신화: 2026-08-15** — 문서/깃 대조 갱신(코드 무변경). ⓐ **push 상태 정정**: 코드·docs는 이미 push 완료(main == origin/main == `1364e5e`) — 실제 미푸시는 **태그 7개**(`-eth-reapply`/`-fram-robust`/`-mbtcp-hardening`/`-physio-b`/`-physio-d`/`-power-ch1`/`-weld4`). ⓑ **미머지 브랜치 `refactor/ponytail-cleanup` 편입**(main+12커밋, origin 푸시됨, HW 재검증 3항목 게이트 — §2.2). ⓒ **원격 제어 활성화 게이트 로드맵 편입**(2026-08-02 정책 승인·미구현, `specs/2026-08-02-remote-enable-gate-decision.md` — §2.2). 보드는 여전히 main `61524c1`.
> (직전 최신화 2026-07-19 — 풀배선 벤치: 사용자 신규 8건(fix/기능) 전건 HW PASS. **`6af9882`** 표시 데드밴드 20→14(최소 표시 0.15A) · **`a46eaf3`** 부팅 유령 SEEK 소멸(bak zero-init) · **`2ea5c2d`+`2cee1cc`** 부팅 beep(전원 직후) · **`6e30499`** fault 부저 알람 글루 · **`789f347`** 경고 페이지 터치 토글 반전 fix · **`519d908`** SYS_HORN horn-down 포팅 · **`61524c1`** STD weld OVTIME 알람. **USOUT(PB4)=코드 정상·PCB 원인**(무수정). 무변경 결정=EMA α=1/2·숫자 피크홀드·cal_val=16. 보드=`61524c1` 플래시됨(⚠세션 말미 전원 OFF·잔재 불확정). **★ 다음=HMI Task 8**(gds_us_hmi). 상세=루트 `HANDOFF.md`. — ⚠"push 미실행=코드 8+docs+태그"는 stale, 위 2026-08-15 ⓐ 참조.)
> (직전 최신화 2026-07-11 — FW 벤치: 신규 3건 중 #1 유령 런·#2 REMOTE icon·energy/OVTIME 타이밍 HW PASS + OVTIME 경고화면 복귀 fix 2커밋 `83498e7`/`88faf08`.)
> (직전 최신화 2026-07-08 — 사용자 신규 3건 전부 코드-완료: 부팅 터치 유령 런 fix(`e26e15b`) + REMOTE icon(`60792da`) + 전류 EMA τ≈100ms(`78a1e43`).)
> (직전 최신화 2026-07-05 — weld 사이클 E2E 전항목 PASS(풀배선 벤치) + 벤치 수정 6커밋(§1.1 표 하단): 클럭 **HSI→HSE**(주파수/타이밍 편차 원천 제거), 전류 표시 실동작(rig-fit), RESET/SEEK 물리 구동(**스윕 주체=보드측 실증** — B-SEAM 최대 미지수 해소), E-stop LCD/부저, SENSOR ON/OFF, overload 아이콘-only. HW-gated 백로그 대폭 축소(§1.2/§2.2). 보드 상태=§2.3-a.)
> (직전 최신화 2026-07-02 — 전면 감사 반영: §1.1 표에 i2c-pot/ovtime 추가, §1.3 적용 결정 7건(D0~D6) 신설. 발견 상세 = git 이력의 HANDOFF 2026-07-02판.)
> (직전 최신화 2026-06-20 — weld 슬라이스3 + SEEK/RESET HW 검증·머지·푸시 완료.) 변경 이력 = `docs/changelog.md`(최신 위), 세션별 상태 로그 = `docs/superpowers/RESUME.md`(SessionStart 자동 로드).
>
> **⚠⚠ git 히스토리 재작성됨 (2026-06-20)**: `git filter-repo`로 전체 282커밋 author 이메일을 `nogari@gmail.com`으로 재작성(shuug2 GitHub 연결용) → **모든 커밋 해시 변경**. 본 문서·RESUME·changelog·메모리에 적힌 옛 해시(`f6a7eee`/`49ca2c7`/`d32d014` 등)는 **더 이상 존재하지 않음**(`git show` 시 "Not a valid object name"). **안정 레퍼런스 = 태그**; 현재 해시는 `git log --oneline`로 확인. origin = `git@github.com:shuug2/gds_us_ctrl.git`(SSH, filter-repo가 origin 제거 후 재추가). main `1fa5938`(이후 HANDOFF 커밋으로 진행) = origin 동기. 이메일 인증(shuug2 Settings→Emails)은 사용자 미완료(소급 적용).

---

## 1. 현재 상태 (2026-06-20)

**통합 핵심 기능 대부분 흡수 완료.** STM32F410RBT 단일 MCU로 기존 SAMD20 + ATmega16 기능을 통합 중. LCD·레귤레이션·Modbus(RTU+TCP)까지 main에 있고, 남은 것은 대부분 **실 초음파/가변전압이 있어야 검증 가능한 출력·효과 계층**.

### 1.1 스테이지 현황 (전부 main 머지 완료)

| Stage | 내용 | tag |
|-------|------|-----|
| Phase 1+2 | 부트스트랩 — 96 MHz HSI×12 + TIM11 1ms tick + USART6 mon + PB3 heartbeat | `b8afe1c` (merge) |
| Stage A | DGUS LCD I/O — wire 통신 + 1Hz cadence | `hw-revA_fw-stage-a` |
| Stage B | LCD application 데이터 사전 셋업 (`init_lcd_mode` 포팅) | `hw-revA_fw-stage-b` |
| LCD full port | LCD 전체 거동 포팅 (comm 표시 등, DGUS 에셋 root) | `hw-revA_fw-stage-lcd` |
| Stage D | ATmega16 흡수 — 레귤레이션 compute · 상태머신 · soft-start · RUN 게이트 · m1(param 주입) | `hw-revA_fw-stage-d` / `-d2` / `-d2b` |
| Stage C | Modbus 흡수 — slice 1 RTU(USART6) + slice 2 TCP(W5500 static+DHCP) | `hw-revA_fw-stage-c1` / `-c2b` |
| Weld-cycle | 공압 프레스 사이클 FSM 흡수 — slice 1 DELAY + slice 2 energy_ctrl exit + slice 3 multi_ctrl 2단 진폭 (host + HW-regression verified; 사이클/스테핑 자체 E2E·에너지 절대값은 6b/슬라이스4) | `hw-revA_fw-stage-weld1` / `-weld2` / `-weld3` |
| SEEK/RESET | samd20 공진 RESET/SEEK 명령 효과 — 순수 FSM + 글루, 자동 체인(RESET→600ms→SEEK→600ms→해제) + ICON + 양방향 RUN 직교 (host + HW verified; 물리 OSC 효과는 hook stub → B-SEAM/6b) ✅ 2026-07-02 D1 완료: SR_TICKS 50→60 충실화(레거시 실거동 600ms/leg) 커밋됨 — 보드 재검증은 다음 HW 세션 | `hw-revA_fw-stage-seekreset` |
| I2C_POT 진폭 | U4 디지털 포텐셔미터(@0x28) 실구동 — set_pot(%)/set_amp(raw) 공용 `drivers/i2c_pot.c` + 부팅 초기값 (HW ACK PASS) | `hw-revA_fw-stage-i2c-pot` |
| OVTIME | energy 직접런 종료 쌍(에너지-도달 정상정지 + ERR_OVTIME fault) — `app_reg_tick`이 `reg_run_limits_t` 주입 구조로 변경(이후 머지의 기준) | `hw-revA_fw-stage-ovtime` |
| Physical IO b/d | FREQ_IN 측정(TIM5, 실신호 34.46kHz 실증) + 물리 명령 B_START/B_RESET/PC11(SEEK\|EMSW 이중역할)+E-stop 레벨-추종+overload 인프라+OSC boot-init+buzzer/SOL/USOUT (HW: 버튼/체인/E-stop 활성·차단·자동해제 PASS; EMSW 물리 해제-추종=미배선 보류, overload 실동작=이연) | `hw-revA_fw-stage-physio-b` / `-physio-d` |
| 표시 ch1 분리 | curr_amp/power/energy 표시·에너지 적분 입력을 ch1(소비전류)로 — ✅ **2026-07-05 실동작**(도메인 정합×6+gain 7/5 rig-fit+EMA+피크/유지 표시, 600mA 앵커; 정밀 보정=6b) | `hw-revA_fw-stage-power-ch1` + `3d2f414` |
| Weld slice4 | TRIGGER 모드+양손 트리거 FSM+안전 abort+진입 게이팅+SETUP 게이트+D2 클램프 M1~M4+I-1+H1/D4 래치 — ✅ **사이클 E2E 전항목 PASS 2026-07-05**(§7.3 1~6·8+SETUP 게이트, 풀배선 벤치) | `hw-revA_fw-stage-weld4` |
| 벤치 수정 2026-07-05 | SENSOR ON/OFF 동적 갱신(`b19823a`) · E-stop 경고 페이지+복귀 가드(`cbbfe19`) · E-stop 부저+overload 아이콘-only(`c3b3f27`) · **RESET/SEEK 물리 OSC 구동**(`29803ae`, 스윕 주체=보드측 실증) · **클럭 HSI→HSE**(`e72dbe4`, 주파수/타이밍 편차 원천 제거) · 전류 표시(`3d2f414`) — 전건 cpp-review APPROVE | main 직접 커밋 |

> ⚠ `hw-revA_fw-stage-c2a`는 **없음** — pre-refactor slice 2a는 1s PHY-폴 버그 보유라 태그하지 않음. `-c2b`가 static+DHCP 전부 커버.

**slice-2 deferred HW (2026-06-13 j 전수 종결, 코드 무수정)**:
- ① **ICON_RUN over TCP = PASS** — START→US_COMM run→on-time ceiling 자동정지(실측 537–617ms = 560ms)→IDLE + ICON_RUN 육안
- ② **RTU FC06 회귀 = PASS** — FC03 미러(`55/56/567` = TCP 동일) + FC06 클램프(80/30→50/120→100/55), slice 2 무회귀
- ③ **RAM-only 재리스 = 동작 규명·수용** — DHCP 리스 IP가 LCD `comm_mode=ETH_STATIC` 저장 시 static IP로 FRAM에 굳음(가설 맞음, 무수정). 상세 = 메모리 `project_eth_dhcp_static_persist`

### 1.2 남은 작업 (spec 미작성 → brainstorming부터)

> ⚠ **2026-07-02 감사로 전제 변경**: "코딩 가능한 계층 전부 완료"는 더 이상 사실이 아님 — 전면 코드 리뷰(CRITICAL 1+HIGH 4+MEDIUM 9)로 **HW 없이 가능한 수정 작업이 새로 생김**. 상세 발견 = 루트 `HANDOFF.md`(2026-07-02), 적용 결정 = 아래 **§1.3**. 아래 기존 목록은 여전히 HW-gated.

- ~~**weld 사이클 E2E (slice4 이연분)**~~ — ✅ **전항목 PASS 2026-07-05** (풀배선 벤치): §7.3 1~6·8 + SETUP 게이트/동결 + EMSW 해제-추종(d' 이월) 전부 종결. SETUP-overload SOL 노트는 9-b 동결·재개 실물 확인으로 갈음.
- **B-SEAM OSC 물리 구동** — **대부분 선행 완료 2026-07-05**: RESET/SEEK 라인 실구동 머지(`29803ae`) + "명령 3선 active-LOW 레벨 미러" 가설 **실증**(RESET 592ms→SEEK 591ms 레그 + FREQ_IN 스윕-정착 34115→34508Hz = **스윕 주체=OSC 보드측 확정** — 최대 미지수 해소). 잔여 = 스코프 파형 정밀 관측(안전 극성/폴라리티 sanity), PB12(OSC2) 용도(출력 구동 금지 유지), 진폭 추종.
- **6b signal calibration** — **범위 축소 2026-07-05**: 주파수 보정 **불필요화**(HSE 전환으로 0.01% 일치, freq_cal_val=0 유지), 전류 1차 캘리브레이션 완료(600mA 앵커, gain 7/5 + cal_val 트림 경로 확립). 잔여 = 전류 정밀/다점 보정, ch0(레귤레이션) 도메인·물리단위, weld energy 누산 절대 E2E + divisor(`REG_ENERGY_DIV=250`), ADC offset, **EMA↔에너지 적분 디커플링**(`3d2f414` 리뷰 MEDIUM — 표시 EMA가 energy-exit 판정에 물림, 적분은 비필터 값으로 분리; 명시 이연 결정).
  - **(하위 항목) #7 출력이상 ERR_OUTERR 포팅** — samd20 출력이상 검출(`curr_amp <= USOUT_TH(25)` ×`USOUT_ERR_CNT_MAX(8)`, multi 모드, main.c:4318-4338)은 **트리거 `re_outerr_issued=1`이 주석처리(4333)=legacy에서 비활성**. 실 `curr_amp`(ADC 측정 진폭)에 의존하므로 **6b 종속** — 6b로 진폭 절대 보정이 서야 검출 임계가 의미를 가짐. 살리려면 ① 트리거 주석 해제 ② 실 curr_amp. fault 표면 인프라(error_status/ICON_OUTERR/`MB_STATUS_OUTERR=0x10`/RESET 복구)는 이미 main에 완비(미공급). 분석 = `docs/superpowers/specs/2026-06-28-ovtime-energy-run-design.md` §1·§8.
- ~~**overload 보호**~~ — ✅ **실동작 E2E 사용자 벤치 PASS 2026-07-05 c** (체인 전체 정상, 코드 무변경 — physical-io 이월 3건[EMSW·OSC 구동·overload] 전부 소진). 잔여 LOW: handle_key_multi RESET의 OVLD/OUTERR 비트 휘발성(후속).
- **[2026-07-05 c 신규 완료 참고]** seek/reset 중 측정값 라이브 표시(`e2003e1`, us_on_status 복원+벤치 4항목 PASS) / 표시 전류 전달함수 재정의(`54e5220`, 재앵커 ch1≈126 + 오프셋 제거: GAIN 59/126·OFFSET 0·DEADBAND 20 — ⚠ **최종 벤치 0.60A 표시 명시 확인 미기록 = 다음 벤치 첫 항목**; 중간 구간 2점 fit은 6b).

**설계상 이연(slice 2)**: DHCP 핫플러그(링크 드롭 후 재획득 — 현재 LINKWAIT→UP 단방향), SERIAL boot-skip.

### 1.3 2026-07-02 감사 적용 결정 (사용자 확정, 대화식 의논)

발견 상세·파일:라인 = 루트 `HANDOFF.md`(2026-07-02). 결정 7건:

| # | 결정 | 실행 시점 |
|---|------|-----------|
| D0 | ✅ **완료 2026-07-02** — **C1**(CRITICAL, `app_lcd_input.c` dispatch `data_len<3` 가드) 단독 커밋 `eabeab0`, cpp-reviewer APPROVED | ~~다음 코딩 세션 첫 커밋~~ |
| D1 | ✅ **완료 2026-07-02** — seek/reset **600ms 충실화** `SR_TICKS` 50→60 + host 테스트 경계 갱신, 커밋 `85811fc`, cpp-reviewer APPROVED (레거시 실거동 `us_reset_cnt > 5` 0-시작 100ms = 600ms/leg) | ~~다음 코딩 세션~~ |
| D2 | ✅ **CODE-COMPLETE 2026-07-04 (slice4 브랜치, 미머지 HW 게이트)** — M1(`51fb9e2` limit_energy=0=off)/M2(`0aa91ad` 글루 mo_out belt-and-braces)/M3(`17a07e3` FRAM comm idx)/M4(`9792c5b` LV_* 10케이스+LOW-1) + **I-1 보강**(`1cb76bd` output_power FRAM 로드 클램프 — 최종 리뷰가 같은 계열 사각지대로 발견) | ~~weld slice4~~ |
| D3 | **H3+H2 = 'fram-i2c-robustness' 슬라이스**(fram_read_* status 전파+실패 필드 팩토리 폴백/경고, I2C1 bus-unstick+err_count 표면 배선) / **H4+IWDG는 별도 슬라이스** — 📐 **spec 완료 2026-07-02**(`docs/superpowers/specs/2026-07-02-fram-i2c-robustness-design.md`; 확정: 경고=mon만·unstick=init 1회·write 무변경·INIT_FLAG 읽기실패=factory-write 금지). ✅ **HW 회귀 PASS + MERGED 2026-07-04**(main `be2fac9` --no-ff, tag `hw-revA_fw-stage-fram-robust`, 브랜치 삭제; HW=부팅 FRAM 저장값 유지 폴백 미발동+LCD 육안 / START→STATUS 1×4→0 ceiling 무회귀 / FC06 write→리셋→리로드; mon `[cfg]` 캡처는 RS-485 DE 미제어로 생략—①③④가 FRAM 로드 입증) | ~~코딩 세션 (HW=검증만)~~ |
| D4 | ✅ **CODE-COMPLETE 2026-07-04 (slice4 브랜치 첫 커밋 `4541ef4`, 미머지 HW 게이트)** — WELD 모드 래치(CYL1→WELD 전이 시점 단일 스냅샷)+전이 카운터 리셋, host 3테스트 | ~~weld slice4 선결~~ |
| D5 | 미머지 통합 = **코딩 세션에서 reconcile 선행**(각 브랜치→현 main rebase+`app_reg_tick` 시그니처 semantic 통합+board.c 병합, 빌드+host PASS까지), 순서 **b→d→ch1**(b=독립·최고령, d가 a·c 포함). ✅ **reconcile 완료 2026-07-04 d** — 3브랜치가 main 위 선형 스택(b'=main+4, d'=b'+28, ch1'=d'+5), `reg_run_limits_t` 7필드 통합, ceiling 이중화×ovtime 병합(spec §5.2 — ⚠TOUCH 운영 ceiling 제외 등 의도된 거동 변화 2건), board.c=d판 byte-identical, 단계별 0-warning+host 8/12/12 PASS, d'/ch1' cpp-review 게이트 통과. backup=`backup/pre-d5-*` 3개. spec/plan=`2026-07-04-d5-reconcile-*`. ✅ **HW 검증+전단위 머지 완료 2026-07-04**(패널 rig 세션 — b' `0cc34a8`/d' `46055c9`/ch1' `e973721`/slice4 `b571da0`, 태그 4개; 거동 변화 2건 실증) | ~~완료~~ |
| D6 | Modbus/ETH 중 **M7만 먼저**(LCD static IP 저장→가동 중 W5500 즉시 반영 경로), M6/M8/M9는 **HMI 착수 시 'modbus-tcp-hardening'** ✅ **M7 HW E2E PASS + MERGED 2026-07-04**(main `6467d67` --no-ff, tag `hw-revA_fw-stage-eth-reapply`). ✅ **M6/M8/M9 'modbus-tcp-hardening' HW E2E PASS + MERGED 2026-07-06**(main `7c474e1` --no-ff, tag `hw-revA_fw-stage-mbtcp-hardening`; 최종 리뷰 HIGH=coalesced FC06 apply 굶김→FC06-후-워크-종료 fix + 벤치 fix 2건=vendored recv 논블로킹 순서-뒤집힘 우회·W5500 KA 10s[선재 락아웃]; RS-485 첫-write 조사 보고=research/) — **감사 큐 D0~D6 완전 소진** | ~~전부 완료~~ |

부수: stale 주석 정정(`app_reg.h:42` "no-op", `app_modbus.c:105/109`, `fw/test/Makefile:1-3`)은 해당 파일을 건드리는 첫 코드 커밋에 동승.

---

## 2. 다음 세션 진입 시 First Step

### 2.1 사전 점검

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl   # main repo
git status                             # working tree clean 기대 (untracked .understand-anything/, ref/atmega16/M16_reverse/ = 무관)
git log --oneline -5
git tag -l 'hw-revA*'                  # 위 §1.1 태그들 확인

# 빌드 + 호스트 테스트 sanity
env -u STM32_TOOLCHAIN cmake -S fw -B fw/build -G Ninja
env -u STM32_TOOLCHAIN cmake --build fw/build      # 0-warning 기대
make -C fw/test test                                # 5 스위트 PASS 기대 (reg_calc/modbus_core/tcp_frame/weld_fsm/seek_reset_fsm)
```

### 2.2 다음 작업 후보

**2026-08-15 현재 우선순위**: ① HMI Task 8 → ② ponytail-cleanup 머지 → ③ 전류 실측 → ④ 원격 게이트 구현 → ⑤ 6b·B-SEAM(보류).

- **★ `refactor/ponytail-cleanup` HW 재검증 → 머지** (main 대비 +12커밋, base `8eaac71`, tip `753778d`, **origin 푸시됨·미머지**). 내용 = 07-19 리팩토링 4스테이지(죽은코드 삭제 / `app_lcd_input.c` 1038→622 분할 + 신규 `app_lcd_comm.c` / `app_reg_tick` 118→57 헬퍼 추출 / 전 269함수 주석 통일 — 앞 2·4스테이지는 바이너리 동일 입증) + 07-25 4커밋(`define.h` 브랜드/버전 분리 5종 · **MAKETECH** `SMT-{H|A|S}{freq}D` · **ether IP 편집 커서 fix** `e8e84fb` · `fw.sh`). 게이트 = 벤치 3항목:
  1. LCD SETUP comm/ether 편집 + DATA_SAVE 저장/복귀 (분할로 코드가 이동한 경로)
  2. 직접런 560ms ceiling + OVTIME 무회귀 (`app_reg_tick` 헬퍼 추출 경로)
  3. Modbus FC03/FC06 스모크
  \+ 07-25분 육안 2건: SETUP 모델명 문자열 / IP 편집이 백스페이스 1회로 지워지는지.
  PASS 시 `git merge --no-ff` (태그 불요). ⚠ 보드 **미플래시**(현 `61524c1`) — 검증하려면 이 브랜치로 빌드·플래시 필요. ⚠ 브랜치 전환 시 `app_lcd_comm.c` 생감 → `cmake -B build` 재구성 필수(`./fw.sh`가 자동 처리).
- **원격 제어 활성화 게이트 구현** — 2026-08-02 정책 승인, **이 저장소 미구현·착수 전**. 결정 기록 = `docs/superpowers/specs/2026-08-02-remote-enable-gate-decision.md`, 설계 정본 = `~/dev/work/gds_us_remote`. 요지 = **현행 펌웨어에 원격 제어 권한 게이트가 없다**(Modbus 도달 가능한 누구나 `START(0x1B)` 쓰기 가능, 물리 인터록 없음, 30s 절대 상한이 유일 backstop; `mb_write_reg`가 미사용 영역 write도 "성공" 에코 → 활성화 오판 위험). 필요 = 레지스터 `0x2A~0x2D`(LCD 전용 활성화·비영속·링크 침묵/E-STOP 해제·capability probe) + `0x1E~0x29`(comm/eth 노출, staging+commit·교차 경로만 허용). **착수 전 사용자 협의 2건** = 활성 창 길이/링크 침묵 임계, LCD 활성화 UI 방식(DGUS 자산 변경 여부). 원격기 파일럿(STOP·읽기·파라미터만)은 블로킹되지 않으나 **원격 START의 유일 선행**. 진입 절차 = §3(brainstorming부터).
- ~~**[2026-07-06 사용자 신규 등록 3건]**~~ — ✅ **전부 코드-완료 2026-07-08**(`e26e15b`/`60792da`/`78a1e43`, 전건 cpp-review 0 Crit/High, main 직접 커밋): ① 부팅 터치 유령 런 = data=0 물리 토글 fix ② REMOTE icon = samd20 case 9 포팅(1s hold) ③ 전류 EMA α 1/8→1/2(τ≈100ms). **벤치 검증 미실시**(보드 미플래시) — 체크리스트 = 루트 `HANDOFF.md` §Resume (유령 런 소멸/REMOTE 에셋 렌더/반응 체감+energy 타이밍[리뷰 MEDIUM]).
- **HMI SP1 Task 8 실보드 E2E** — 진입 = `~/dev/work/gds_us_hmi` 폴더 세션 + 그쪽 HANDOFF.md (이 repo 아님). RS-485 어댑터 필요. **병행: RS-485 첫-write 재현 절차 실행** — `docs/superpowers/research/2026-07-05-rs485-first-write.md` §6 (전원사이클→첫 FC06 ×10 기록; 최유력=글리치 병합, V-A/V-B 시나리오 분리).
- **6b signal calibration 잔여** — ⏸ **사용자 보류(2026-07-05 c)**. 전류 다점/2점 fit(낮은 부하 실측점 — 오프셋 제거로 중간 구간 편차 가능), ch0 도메인, weld energy 절대 E2E + divisor, EMA↔에너지 적분 디커플링(리뷰 MEDIUM), OUTERR(하위 항목).
- **B-SEAM 잔여** — ⏸ **사용자 보류(2026-07-05 c)**. 스코프 파형 정밀 관측 + PB12 용도 + 진폭 추종 (구동·스윕 주체는 해소).
- **후속 소소(비긴급)**: app_eth STATIC_UP 링크 재폴링 부재(선재 — KA로 완화됨, 근본 수정은 링크 FSM 확장) / KA 무송신-피어 잔여(Modbus 실질 무해) / defer Minor 목록 = `.superpowers/sdd/progress.md`(mbtcp) + 구 ledger들.
- 진입 절차 = **§3** (brainstorming → spec → writing-plans → subagent-driven → finishing).
- ⚠ 머지/푸시 정책: origin(SSH) — 머지 후 `git push origin main` + 태그 푸시(§6). **2026-08-15 실측: 코드·docs는 push 완료**(main == origin/main == `1364e5e`, `refactor/ponytail-cleanup`도 푸시됨). **미푸시 = 태그 7개**: `hw-revA_fw-stage-` + `eth-reapply`/`fram-robust`/`mbtcp-hardening`/`physio-b`/`physio-d`/`power-ch1`/`weld4` → 사람 터미널 `git push origin --tags`.

### 2.3-a 보드 현 상태 (2026-07-19 마감 — 최신)

- **main `61524c1` 플래시·검증됨** (2026-07-18~19: 신규 8건 fix/기능). 벤치 PASS = 데드밴드 0.15A·부팅 유령 SEEK 소멸·부팅 beep·fault 부저 알람·토글 반전 fix·SYS_HORN·STD weld OVTIME 알람.
- **USOUT(PB4)=PCB 이슈**(펌웨어 무관·무수정): 코드/극성/핀 정상 확인 완료 — 재조사 불필요.
- ⚠ **세션 말미 보드 전원 OFF 관측**(USOUT 조사 중 SWD 전압 0.003V) — 재개 시 **전원/ST-LINK 먼저 확인**.
- **풀배선 리그** 유지: 양손 SW_START1/2(PC12/PB11)·SENSE_DN/UP(PA11/PA12)·EMSW(PC11)·B_START/B_RESET + 실 혼/실린더 + 전류 sense(PB1) + 이더넷(W5500). RS-485 어댑터 미접속.
- ⚠ **잔재 설정 불확정**: 세션 중 STD/HAND 모드·EN_ENERGY·EN_MULTI·horn·TIMEOVER 등 다수 토글(전원 OFF로 최종값 미확인). 재개 시 LCD/SWD로 **model_type·comm_mode·EN_* 실측 후 복원**. comm=ETH_STATIC .199 유지 추정(미확인). HMI Task 8 진입 시 LCD에서 SERIAL/addr=1/9600/EVEN 복원 필요.
- ⚠ **model_type=multi(1) 함정**: Modbus 직접런 운영 ceiling 미적용(slice-D 설계=HAND 전용, 30s 안전 캡만) — 테스트 후 반드시 STOP(-r 29). ceiling 회귀 시험은 model_type=hand 전제(전환 시 EMSW E-stop 유발 주의, rig 노트 R1). **PC11=EMSW NC 배선 평시 LOW**(multi에서 SEEK 역할 — E-stop 스위치 조작 시 SEEK 발화는 듀얼롤 고유 거동).
- ⚠ 이월(3회째, 전류계 세션): **전류 표시 0.60A**(RUN+전류계 0.6A↔표시 0.60A, 유휴 0.00) + **energy-exit 실전류**. (EMA 체감은 2026-07-18 종결: α=1/2 유지 확정 — 숫자는 피크홀드라 α 무관, 바그래프만 실시간.)
- ⚠ 빌드마다 bss 주소 이동 — SWD read 전 현재 ELF에서 `arm-none-eabi-gdb -batch -ex "p/x &심볼"` 재확보 필수. 비침습 샘플러 = openocd TCL 루프(read_memory, halt 없음, ~1.4ms/샘플).
- ⚠ mbpoll: 쓰기 값은 **IP 뒤**(`mbpoll ... .199 1`) / 부팅 직후·연속 TCP 트랜잭션 간헐 실패 → 재시도(0.4s ×3) 필수.
- ⚠ push(2026-08-15 정정): 코드·docs 완료(main == origin/main == `1364e5e`). **미푸시 = 태그 7개** → `git push origin --tags`(사람 터미널).
- ⚠ **보드 ≠ 최신 브랜치**: 보드는 main `61524c1`. 미머지 `refactor/ponytail-cleanup`(+12커밋)은 **미플래시** — 그 브랜치 검증 세션은 빌드·플래시부터 시작(`./fw.sh flash`).

### 2.3 보드 현 상태 (2026-06-20 마감 시점)

- **SERIAL / addr=1 / 9600 / EVEN** (벤치 기본; USART6=Modbus 점유 → mon 115200 비가용, mon 필요 시 LCD에서 addr=NONE), OUT_POWER=55, EN_MULTI=0·ON_TIME=56(테스트 후 복원), FRAM `ether_ip=.70` 잔여(무해). **seek-reset/main 펌웨어**(태그 `hw-revA_fw-stage-seekreset` = 현재 main) 플래시됨. ⚠ ON_TIME(직접런 ceiling)은 **clamp max=100**(1000ms). ⚠ Modbus 검증 시 mbpoll 플래그는 인라인(zsh word-split 안 함); 함수명은 alias 피해 `mbread`/`mbspin`/`mbwrite`; DISP_ENERGY=wire **0x05**(`-r 6`); 첫 트랜잭션 `Invalid CRC`면 더미 read 1회 후 재시도.
- ETH 재검증 시: LCD에서 `comm_mode=ETH_STATIC`(static `.70`) 또는 `ETH_DHCP` 전환 + SAVE + **물리 전원사이클**. ETH E2E 재현 절차 = 루트 `HANDOFF.md`(⚠ 시리얼 캡처 함정 절 필독).

---

## 3. 신규 스테이지 진입 절차 (subagent-driven-development 패턴)

1. **Worktree 생성** (선택, 격리 작업 시):
   ```bash
   cd /Users/tknoh/dev/work/gds_us_ctrl
   git worktree add ../gds_us_ctrl-<stage> -b feat/<stage-name>
   ```
   (단일 슬라이스/소규모는 main에서 직접 브랜치도 가능 — 최근 stage-c/d 슬라이스는 feature 브랜치 직접 사용)

2. **`superpowers:brainstorming`** — 결정점(범위/구조/데이터 source/API 표면/이연 범위) 탐색 후 사용자 확정.

3. **spec 작성 + self-review** — `docs/superpowers/specs/<YYYY-MM-DD>-<stage>-design.md`.

4. **`superpowers:writing-plans`** — `docs/superpowers/plans/<YYYY-MM-DD>-<stage>.md` (Task 분해, HW-gated Task 분리).

5. **`superpowers:subagent-driven-development`** — Task별 fresh subagent + 2-stage 리뷰(spec compliance + cpp-reviewer). 호스트 게이트(빌드 0-warning + 테스트) 통과 후, HW E2E는 보드 게이트로 분리.

6. **finishing-a-development-branch** — HW 검증 통과 시 머지(`--no-ff`) + 태그 `hw-revA_fw-stage-<x>` → **origin(SSH) 푸시**(`git push origin main && git push origin --tags`). ⚠ 에이전트 샌드박스 환경은 SSH 인증 불가 → push는 사람 터미널에서.

> drift 발견 시: spec 정정 commit → plan verbatim sync → 코드 first-time commit.
> subagent dispatch 가드: worktree/브랜치 only, 메인 무관 touch ✗, doc regen 자동 ✗, 코드 변경 ✗(read-only review), 빌드 시도 ✗(controller가 sanity).

---

## 4. 환경 / 알려진 이슈

### 4.1 빌드 환경
- `$STM32_TOOLCHAIN` env var stale → **`env -u STM32_TOOLCHAIN cmake ...` 필수**.
- 툴체인: `arm-none-eabi-gcc 15.2.1`(homebrew), `cmake`, `ninja`, `openocd 0.12.0`.
- ST-LINK V3 (`/dev/cu.usbmodem*`) — Cortex-M4 r0p1 (6 HW BP 한계).
- 빌드 syntax check: `arm-none-eabi-gcc -fsyntax-only` exit=0 + warning 0 (clangd LSP 노이즈 무시).

### 4.2 펌웨어 구조 핵심
- HAL 핸들 단일 정의 = `src/periph.c`, extern = `include/periph.h`.
- 페리페럴 GPIO는 그 드라이버가 직접 책임(예 `drivers/usart.c`가 PC6/PC7 AF).
- `fw/vendor/` = ST HAL/CMSIS + WIZnet ioLibrary(핀 `220ca7a6`, `_WIZCHIP_=W5500`, 경고격리 lib) — read-only, 편집 ✗.
- MCU 클럭 96 MHz, source of truth = `fw/src/clock.c`.

### 4.3 시리얼 캡처 (USART6 mon, 115200) — ⚠ 함정
- **리다이렉트 형식 필수**: `{ stty 115200 cs8 -parenb -cstopb raw -echo; exec cat; } < /dev/cu.usbserial-AB0MLYXA > /tmp/mon.log &` (`cat /dev/...` 인자형식은 포트를 9600으로 리셋 → garbage).
- 비-UTF8 글리치: `LC_ALL=C tr -d '\000'`로 바이트 처리. 종료: `pkill -x cat`(`-f 'cat'`은 과매칭).
- 깨끗한 부팅 mon = **물리 전원사이클**(openocd reset은 boot 버스트 안 나옴).
- mon ↔ Modbus RTU = USART6 공유. SERIAL+addr!=0면 Modbus가 점유 → USART6=comm 속도(9600 9E)·mon 게이트오프. addr=NONE으로 풀리면 `usart6_init()`이 **115200 8N1로 정확히 복원**(SWD 검증됨). ETH 모드는 mon 동작.
- ⚠ **baud 의심 시 추측 말고 SWD로 USART6 레지스터 직독**(시리얼 캡처는 macOS 포트 재개방 플레이키 + 상태 thrash로 신뢰 낮음): `mdw 0x40011408`(BRR) → **833(0x341)=115200 / 10000(0x2710)=9600**(BRR=PCLK2 96MHz/baud). `mdw 0x4001140C`(CR1) → M(bit12)=0·PCE(bit10)=0 → mon 8N1 / M=1·PCE=1 → Modbus 9E. (2026-06-14 e: "mon baud 미복원 버그" 의심 → controlled SWD 실험으로 **버그 아님** 규명. live addr은 g_cfg `mdb 0x20000a86`로 확인.)

### 4.4 보드 BOOT0 — 해결됨 (2026-05-26)
- BOOT0(U2.60)→GND 연결로 평범한 `reset run` 플래시 부팅. force-jump 워크어라운드 불필요. (메모리 `project_board_boot0_workaround`)

### 4.5 회로 핵심 (V30 회로도 + DGUS 자료로 해소)
- ATmega16 PA4=초음파 출력개시 입력, PC0=overload 출력, PC1/PC4=초음파 보드 신호 입력. 7-세그먼트 없음(DGUS 단독). `I2C_POT`=U4 외부 I2C 디지털 포텐셔미터 @0x28(EEPROM과 I2C1 공유, 진폭 제어 실체).

### 4.6 graphify
- **사용 중단 (2026-06-10)** — 재생성 ✗.

---

## 5. 빠른 명령어 cheat sheet

### 5.1 빌드 + 호스트 테스트
```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
env -u STM32_TOOLCHAIN cmake -S fw -B fw/build -G Ninja && env -u STM32_TOOLCHAIN cmake --build fw/build
arm-none-eabi-size fw/build/gds_us_ctrl.elf
make -C fw/test test
```

### 5.2 플래시 + 시리얼 mon
```bash
openocd -f fw/openocd/stm32f410.cfg -c "program fw/build/gds_us_ctrl.elf verify reset exit"
# mon (USART6, 115200) — §4.3 리다이렉트 형식
{ stty 115200 cs8 -parenb -cstopb raw -echo; exec cat; } < /dev/cu.usbserial-AB0MLYXA > /tmp/mon.log &
LC_ALL=C tr -d '\000' < /tmp/mon.log | LC_ALL=C tr -s ' ' | grep -aE '\[boot|\[eth|\[mb|\[cfg'
```

### 5.3 Modbus 검증 (mbpoll)
```bash
# RTU (SERIAL/addr=1/9600/EVEN) — RS-485 마스터, AB0MLYXA, pkill -x cat 먼저
mbpoll -m rtu -a 1 -b 9600 -P even -t 4 -r 7 -c 1 -1 /dev/cu.usbserial-AB0MLYXA      # FC03 read OUT_POWER
mbpoll -m rtu -a 1 -b 9600 -P even -t 4 -r 7 /dev/cu.usbserial-AB0MLYXA 80           # FC06 write (clamp 50..100)
# TCP (comm_mode=ETH_STATIC/DHCP) — 별도 소켓, cat 무관
mbpoll -m tcp -a 1 -t 4 -r 1 -c 12 -1 <board-ip>                                     # FC03 mirror
mbpoll -m tcp -a 1 -t 4 -r 28 <board-ip> 1                                           # START(reg 28); STATUS=reg 30, STOP=reg 29
```
> 레지스터(wire→`-r`=wire+1): OUT_POWER 0x06→7 / RESET 0x19→26 / SEEK 0x1A→27 / START 0x1B→28 / STOP 0x1C→29 / STATUS 0x1D→30. mb_baud[]={2400,4800,9600,19200,38400,115200}, parity 0=EVEN/1=ODD/2=NONE. comm_mode 0=SERIAL/1=ETH_STATIC/2=ETH_DHCP.

### 5.4 RAM cfg 직독 (openocd) — comm/ether 설정 확인
```bash
arm-none-eabi-nm fw/build/gds_us_ctrl.elf | grep ' g_cfg'   # 주소 재확인 (build마다 변동)
# g_cfg+0x2A = [comm_address, speed_idx, parity_idx, comm_mode, ether_ip[4]]
openocd -f fw/openocd/stm32f410.cfg -c "init" -c "halt" -c "mdb 0x20000a86 8" -c "resume" -c "exit"
```

---

## 6. 응답 / 작업 정책

- **응답 언어**: 한국어 (코드 / commit / 파일 경로 / 식별자는 영어). 메모리 `feedback_korean_responses`.
- **코드 수정 범위**: 요청한 부분만 (워크스페이스 규칙). `ref/`·`fw/vendor/` 편집 ✗.
- **워크플로**: `superpowers:subagent-driven-development`(Task별 fresh subagent + 2-stage 리뷰). HW-gated Task는 분리.
- **머지**: `--no-ff` + 태그 `hw-revA_fw-stage-<x>` → **origin(SSH) 푸시**(`git@github.com:shuug2/gds_us_ctrl.git`). 과거 "local-authoritative, push ✗" 정책은 2026-06-20부로 origin 푸시 사용으로 변경. ⚠ 샌드박스에선 SSH 인증 불가 → push는 사람 터미널. ⚠ 옛 해시는 filter-repo 재작성으로 무효 — 태그로 참조.
- **컨텍스트**: 50% 임계 일시정지 정책(메모리 `feedback_context_50pct_pause`). `/context` 정기 점검.

---

## 7. 참조 문서

- 변경 이력: `docs/changelog.md` (최신 위)
- 세션별 상태 로그(자동 로드): `docs/superpowers/RESUME.md`
- 다음 세션 핸드오프: 루트 `HANDOFF.md` (남은 작업 HW-gated + filter-repo 재작성 컨텍스트)
- 핀 매핑: `docs/pinmap.md`
- 요구사항: `docs/requirements.md`
- ATmega16 분석: `docs/superpowers/analysis/` (regulation-core-verified, samd20-m16-ipc-semantics-verified, atmega16-io-behavior 등)
- 프로젝트 컨벤션: `CLAUDE.md` (root)
- ref 코드(수정 ✗): `ref/samd20/`, `ref/atmega16/`
- 과거 RESUME archive: `docs/superpowers/historical/`

---

> **본 문서 갱신 시점**: 2026-06-20 (weld 슬라이스3 + SEEK/RESET HW 검증·머지·origin 푸시 완료 + filter-repo 히스토리 재작성 반영; 코딩 스테이지 전부 완료, 남은 작업 HW-gated)
> **다음 갱신 시점**: HW-gated 스테이지(slice4/B-SEAM/6b/overload) 착수·완료 시, 또는 신규 스테이지 시작 시
