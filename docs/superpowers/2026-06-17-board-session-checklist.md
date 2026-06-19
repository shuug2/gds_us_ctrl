# 보드 세션 체크리스트 — weld3 + seek-reset 머지

> **요약**: 보드 연결 시 한 세션에 ① weld 슬라이스3(multi) 회귀확인+머지 → ② seek-reset HW 4항목 → ③ seek-reset 머지를 순서대로 처리하는 실행 체크리스트. 두 스테이지 모두 CODE-COMPLETE 미머지(host+build verified). seek-reset은 slice3 위 stack이라 **slice3 머지가 선행**. 작성 2026-06-17 c.

---

## 0. 준비 (보드 연결 직후)

- [ ] 보드 환경 확인: **SERIAL / addr=1 / 9600 / EVEN** (USART6=Modbus 점유 → **mon 비가용**; 이전 세션 종료 상태). LCD에서 확인/복원.
- [ ] 도구: `mbpoll` (RS-485 어댑터), openocd/ST-LINK (플래시 + `g_cfg` SWD 직독).
- [ ] ⚠ **mon 비가용**이므로 seek-reset `[sr] ... signal ON/OFF` hook 로그는 못 봄 → **검증은 LCD ICON 육안 + mbpoll STATUS**가 주 수단. (mon 필요 시 ETH 모드로 USART6 해방 가능하나 불요.)

### Modbus 레지스터 맵 (app_modbus_core.h; mbpoll -r = addr+1)

| 명령/상태 | addr | mbpoll -r | 비고 |
|---|---|---|---|
| RESET | 0x19 | 26 | consume-and-clear (write 1) |
| SEEK | 0x1A | 27 | consume-and-clear (write 1) |
| START | 0x1B | 28 | consume-and-clear (write 1) |
| STOP | 0x1C | 29 | consume-and-clear (write 1) |
| STATUS | 0x1D | 30 | bit0(0x01)=초음파 on |
| WORK_CNT reset | 0x01 | 2 | write 0 = work counter reset |

mbpoll 기본형: `mbpoll -m rtu -a 1 -b 9600 -P even -t 4 -r <reg> /dev/cu.usbserial-XXXX [value]` (value 없으면 읽기).

---

## 단계 ① weld 슬라이스3 (multi_ctrl) 회귀확인 + 머지

브랜치 `feat/stage-weld-cycle-slice3-multi` (tip `33b7ae9`, base=main `2e5297f`). 변경=weld FSM/글루 3파일만, 기존 경로 byte-unchanged, weld dormant. 검증 목표 = **기존 직접-초음파 경로 무회귀** (스테핑 자체 E2E는 슬라이스4/실 초음파, 태그 아님).

- [ ] 체크아웃 + 빌드 + 플래시
  ```bash
  git checkout feat/stage-weld-cycle-slice3-multi
  cd fw && env -u STM32_TOOLCHAIN cmake -B build -G Ninja && env -u STM32_TOOLCHAIN cmake --build build
  env -u STM32_TOOLCHAIN cmake --build build --target flash
  ```
- [ ] **회귀-1 직접-초음파 ceiling**: `mbpoll ... -r 28 <dev> 1` (START) → `-r 30` (STATUS) bit0=1 → **~560ms(56×10ms) 후 자동정지**(bit0→0) + LCD **ICON_RUN 점등→소등 육안**.
- [ ] **회귀-2 §6 deviation**: `multi_ctrl=ON`(cfg)인 채 직접 START → **스테핑 안 함**(단일 진폭, 동일 ~560ms ceiling 종료). multi는 weld 사이클(슬라이스4) 전용이라 직접 런에 영향 없음 확인.
- [ ] **회귀-3 work_cnt 0**: FC03 work_cnt reg 직독 → 0 유지 (weld dormant 구조증명; `app_weld_request_start` 호출자 없음).
- [ ] **FC03 config 미러** = SWD `g_cfg` 직독 일치 (OUT_POWER/ON_TIME 등 — 기존 회귀 baseline).
- [ ] 통과 시 머지 + 태그:
  ```bash
  git checkout main && git merge --no-ff feat/stage-weld-cycle-slice3-multi
  git tag hw-revA_fw-stage-weld3        # host + HW-regression verified
  git branch -d feat/stage-weld-cycle-slice3-multi
  ```
  머지 후 `env -u STM32_TOOLCHAIN cmake --build build` 0-warning + `make -C fw/test test` host PASS 재확인.

---

## 단계 ② seek-reset HW 4항목

브랜치 `feat/stage-seek-reset` (tip `31730ca`). ⚠ slice3 위 stack — **①에서 slice3가 main에 머지된 뒤** 진행. (seek-reset 머지 시 slice3 커밋은 이미 main에 있어 git이 중복 없이 seek-reset 고유 커밋만 적용.)

- [ ] 체크아웃 + 빌드 + 플래시
  ```bash
  git checkout feat/stage-seek-reset
  cd fw && env -u STM32_TOOLCHAIN cmake -B build -G Ninja && env -u STM32_TOOLCHAIN cmake --build build
  env -u STM32_TOOLCHAIN cmake --build build --target flash
  ```
- [ ] **HW-1 직접-초음파/weld 무회귀**: ①과 동일 (START `-r 28`→STATUS `-r 30` ~560ms ceiling + ICON_RUN). seek-reset이 app_reg START guard를 건드렸으므로 **정상 경로 무회귀가 핵심**.
- [ ] **HW-2 자동 체인 시각**: 패널 RESET 버튼(또는 `mbpoll -r 26 <dev> 1`) → **ICON_RESET 점등 →(500ms)→ ICON_SEEK 점등 →(500ms)→ 소등**. 1초 시퀀스라 빠름 — 주의 관찰(또는 영상 촬영).
- [ ] **HW-3 SEEK 단발**: 패널 SEEK 버튼(또는 `mbpoll -r 27 <dev> 1`) → **ICON_SEEK만 점등 →(500ms)→ 소등** (ICON_RESET 안 뜸 = 체인 없음).
- [ ] **HW-4 양방향 RUN 직교**:
  - RUN 중(START→running, ICON_RUN 점등) SEEK/RESET 입력 → **무시**(ICON_RESET/SEEK 안 뜸, 런 유지).
  - SEEK 또는 RESET active 중(ICON 떠있는 500ms 윈도우) START 입력 → **무시**(런 시작 안 함, ICON_RUN 안 뜸).
- [ ] (참고) **물리 OSC 효과(주파수 탐색/리셋)는 검증 대상 아님** — 본 스테이지는 hook stub(mon 로그만). 실제 CTRL_OSC* 구동은 B-SEAM/6b(실 초음파 rig + 스코프).

---

## 단계 ③ seek-reset 머지

- [ ] HW 4항목 전부 통과 시:
  ```bash
  git checkout main && git merge --no-ff feat/stage-seek-reset
  git tag hw-revA_fw-stage-seekreset    # host + HW(ICON/직교) verified; 물리 OSC E2E는 B-SEAM
  git branch -d feat/stage-seek-reset
  ```
- [ ] 머지 후 0-warning + host 5스위트 PASS 재확인.
- [ ] 핸드오프 갱신: RESUME/changelog/메모리 `project_weld_cycle`·`project_seek_reset`을 MERGED로.

---

## 머지 순서 주의 (stack)

```
main(2e5297f)
  └─ slice3 (feat/stage-weld-cycle-slice3-multi, 33b7ae9)   ← ① main으로 머지
       └─ seek-reset (feat/stage-seek-reset, 31730ca)        ← ③ slice3 머지 후 main으로 머지
```

slice3가 seek-reset의 base이므로 **반드시 ① slice3→main 먼저, ③ seek-reset→main 나중**. 순서 뒤집으면 seek-reset 머지에 slice3 미검증 코드가 딸려 들어감.
