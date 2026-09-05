# Handoff: 통합 벤치 PASS — main 머지·태그 완료, 남은 것은 배선·PCB 게이트

**Generated**: 2026-09-05 (실보드 벤치 세션)
**Branch**: `main` `2b9839e` — origin 동기, **미푸시 브랜치·태그 0**
**Status**: 원격기 동등성 스택 + IWDG **둘 다 벤치 PASS → main 머지·태그 완료.**

> **★ 다음 세션 진입 순서**
> 1. `docs/superpowers/plans/2026-09-05-bench-results.md` **§4 (벤치 환경 함정)** — 보드를 건드리기 전에 반드시. 여기 안 읽으면 세션 초반을 통째로 날린다
> 2. 같은 문서 §3 (미실행 항목과 사유) — "실패"로 오판 방지
> 3. 아래 §열린 항목

---

## 이번 세션 결과

**실행 56항목 전건 PASS, 펌웨어 결함 0건.** 태그 `hw-revA_fw-stage-remote-parity` / `hw-revA_fw-stage-iwdg`.

| 실측 신규값 | |
|---|---|
| IWDG timeout | **4.45 s** (무응답 8.45 − 부팅 4.00) |
| f_LSI 역산 | **≈ 35.9 kHz** (데이터시트 17~47 k 내) |
| 부팅 → Modbus 첫 응답 | **4.00 s** |
| 리셋 원인 | NRST `0x04` / IWDG `0x24` (전원 `0x0E` 미관측) |
| 직접런 ceiling | 555·558·552 ms (기대 560) |

**A 섹션은 가상 PC8 로 14항목 PASS** — `holding[0x2D]` 를 스위치 입력으로 바꾼 throwaway(원복 완료). 빌드는 `MODEL=REMOTE` **순정**이라 게이트 강제는 실제로 돌았다. **A-13(단선=불허)만 원리상 검증 불가.**

**세션 중 나온 코드 변경 3건** (전부 main):
- `ac7e691` 원격 START 진폭 pot write 복원 — 가드가 게시 시점 때문에 **구조적으로 항상 FALSE** 였다
- `7a20785` 감사 stale 주석 6파일 (바이너리 동일)
- `2b9839e` `0x17`/`0x18` 계약 주석 정정 — B-5 이후 R/W (바이너리 동일)

---

## 🔴 열린 항목

### 1. ~~RTU baud 9600 원복~~ — ✅ **완료 (2026-09-05)**
보드가 **38400/EVEN** 인데 소비자 두 곳(`gds_us_hmi` 벤치 · `gds_us_remote` 문서)이 **9600** 을 전제했다. 2026-08-17 세션의 **미기록 변경**으로 판단, 사용자가 **9600 원복 결정**.

**실행**: `gds_us_remote` 세션이 원격기를 다운로드 모드로 세워 TCP 소켓을 비워 줌(케이블 탈거 불요) → 교차 커밋 TCP→serial(FA-5 경로):
`speed 4 / stat 0` → staged `speed 2 / stat 1(STAGED)` → 커밋 → `speed 2 / stat 2(COMMIT_OK)`, **TCP 링크 생존**. `gds_us_remote` 통지 완료(그쪽 계약 문서 line 64 갱신은 그쪽이 처리).

⚠ **RTU 링크에 즉시 반영된 것은 아니다** — 보드가 `ETH_STATIC` 이라 살아있는 RTU 링크가 없고, serial 그룹은 cfg·FRAM 에만 들어간 뒤 **다음에 `comm_mode` 를 SERIAL 로 전환할 때 적용**된다(`app_modbus.c:373-381`). 영속은 확인됨(`save=true` `:383` → `app_config_save_all` `:578`). **물리 UART 실측은 SERIAL 전환 + RS-485 어댑터 게이트**(열린 항목 3).

`CFG_STAT` 은 `2(COMMIT_OK)` 로 남겨 뒀고 `gds_us_remote` 가 **교차 확인 완료**(그쪽 50칸 덤프: `0x1F=2` · `0x29=2` · 기준선 대비 diff 는 이 둘 + 라이브 미러뿐). 커밋 몇 분 뒤에도 `2` 가 잔류한 것이 실측돼, **"커밋 성공 = 값 2 가 아니라 1→2 전이"** 판정이 소스+실측 양쪽 근거로 확정됐다 — 값만 보면 **남의 커밋을 자기 성공으로 오독**한다. 다음 staged 쓰기가 덮는다.

원격기 보드 되살아나 **폴링 재개** — TCP 소켓(1개) 다시 점유 중이다. 벤치로 붙으려면 다시 조율할 것.

### 2. A-13 (단선 = 불허 fail-safe) — **PC8 실장 PCB 대기**
가상 PC8 이 대체하는 성질이 바로 A-13 이라 원리상 불가. 옵토(TLP181)가 극성을 뒤집으므로 **실선을 뽑아** 확인해야 한다. PCB 나오면 **A-1·A-5·A-13 만** 재실행하면 나머지는 이미 확인된 것으로 갈음 가능(입력 소스만 다르고 하류 경로 동일 코드).

### 3. RS-485 어댑터 필요
- **FA-6 / FA-7 / FA-12** 교차 커밋 RTU 방향
- **mon(USART6) 캡처** → **S-P**(pot write 로그) · S-1 · NET-1 로그 · A-2/A-4 로그
- RTU 링크 자체 무회귀 (이번 벤치에서 통째로 못 돌림)

### 4. LCD 육안 항목
S-5 · M-1 · M-2 · MOD-1 · MOD-4 · MOD-7 · B34-2/3/4/9 — 자동화 불가.

### 5. 기타
W-1 전원인가 `rst=0x0E` (물리 전원 재인가) · B34-6 SENSOR(배선) · weld 사이클/`work_cnt`(양손·센서·f_safty 배선) · 전류 0.60A 실측 · 6b·B-SEAM(사용자 보류)

### 6. REMOTE 빌드 요청 예약 — `gds_us_remote` S6 단계 (오늘 아님)
원격기 R2-07 이 게이트 사유 표시(`0x2B` 의 0/3/4)를 화면에 넣는데, 그 검증은 **REMOTE 빌드**가 있어야 한다(STD 는 `REMOTE_CAP(0x2A)` 미러가 없어 게이트 경로를 못 탄다). 그쪽 S6 이 아직 5커밋 뒤라 **요청 시점에 다시 조율**한다.
⚠ 조율 필요 사항: REMOTE 빌드를 올리면 **PC8 미실장 상태에서 게이트가 fail-safe 로 상시 닫혀** 이쪽 벤치의 원격 명령이 전부 막힌다 → **PC8 실장 여부 확인 + 필요 시 BYPASS 빌드**로 갈 것.

---

## 보드 상태 (세션 마감)

**STD 빌드**(main `2b9839e` 상당), ETH_STATIC **192.168.1.199**, unit 1.

`OUT_POWER 77` · `ON_TIME 750` · `ENERGY 3011` · `TIMEOVER 8` · `EN_SAFTY 0` · `MODEL_FREQ 3` · `MODEL_TYPE 2` · `COMM addr 1 / speed 2(9600) / parity 0(EVEN)`  ← 2026-09-05 원복 · `CAL_VAL 16` · `FREQ_CAL_VAL 40` · `HORN_CMD 0` · `CFG_STAT 2(COMMIT_OK, 의도적 잔류 — 열린 항목 1)`

⚠ **원격기가 남긴 잔재 = `RUN_MODE 1`(TRIGGER)**, 기준선은 `0`(DELAY). 원격기 다음 세션 첫 항목으로 원복 예정.
⚠ **`MODEL_TYPE=2(std)` 라 `ON_TIME 750` 은 효력 없다** — on-time ceiling 은 `HAND(0)` 에서만(`app_reg.c:451`). std/multi 는 **30초 절대 ceiling 만** 남으므로 START 시험 후 **STOP 필수**.

---

## 세션 간 협업 (오늘 진행분)

- **`gds_us_remote`**(`gds-us-remote-eb`) — 계약 `F-03` 갱신 완료(`a0dd636` + 정정 `61265da`), 양방향 대조 통과. **서로 하나씩 잡았다**: 그쪽이 우리 헤더 `0x17`/`0x18` 낡은 주석을, 우리가 그쪽 *"staged 쓰기는 게이트 무관"* 오류를(게이트 닫히면 staging 스캔까지 건너뛴다 → `CFG_STAT` 불변 + 미러 복원). 후자는 그쪽 R2-07 설계를 바꿨다.
- **`gds_us_hmi`**(`gds-us-hmi-66`) — 통보 완료. RTU 전용이라 TCP 항목은 무관, IWDG 만 해당(코드 변경 없이 흡수). 진폭 기준 재설정 **불요** 확인(그쪽 벤치에 FC06 쓰기 0건, LCD·패널 기동).
- 다음 세션에 받을 것: 원격기 **IWDG 복귀 실측치**(예상 9~11초) · 모델 전환 결과 · COUNTER RESET / SAFE MODE.

---

## 도구

`docs/superpowers/tools/mb_tcp.py` — **mbpoll 이 이 환경에서 안 되므로** 벤치는 이 클라이언트로 한다. 연결 유지형, 재시도 포함. 주소는 wire(0-based).
