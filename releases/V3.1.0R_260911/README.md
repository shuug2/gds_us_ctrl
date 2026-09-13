# V3.1.0R_260911

> **문서 요약**: REMOTE 티어 `V3.1.0R!_260911` 빌드의 **보관본**. `gds_us_ctrl_v3.1.0R_260911.bin` 은 `fw/build-remote/gds_us_ctrl.bin` 을 이름만 바꾼 것(내용 동일). 재빌드 없이 그대로 플래시하고, 재현 빌드의 대조 기준(`SHA256SUMS`)이다. ⚠ **벤치 PASS 전 보관본** — 2026-09-13 사용자 지시로 벤치 착수 전에 만들었다. 태그는 아직 없고, 보드에 올라간 이미지와 바이트 동일(같은 해시)임은 확인했다. 벤치가 코드를 바꾸면 이 폴더는 새 날짜 폴더로 **대체**된다.

| 항목 | 값 |
|---|---|
| 태그 / 커밋 / 브랜치 | **태그 미발행**(벤치 PASS 후 `hw-revA_fw-stage-lcd-echo` 예정) · 빌드 트리 `8bd0417` = 코드 `3be6a5d` + docs 3 (`feat/modbus-write-lcd-echo`, PR #1 `3f82c06` 위) |
| 툴체인 | arm-none-eabi-gcc 15.2.1 (Arm GNU Toolchain 15.2.Rel1) · cmake 4.3.1 · Ninja · openocd 0.12.0 |
| 빌드 | `rm -rf fw/build fw/build-remote && MODEL=remote ./fw.sh` — `git status --porcelain` 빈 트리(`8bd0417`) |
| 산출 | FLASH 67,192 B (51.26 %) · RAM 6,440 B (19.65 %) |
| 대상 보드 | hw-revA · STM32F410RBT (128 KB) · DGUS 패널 자산 = 사용자 2026-09-13 수정본(저장소 `hw/lcd/dgus/` 와의 일치는 미확인, HANDOFF 열린 항목) |
| STD 빌드 | **미포함** — 이번 벤치 대상이 아니라 검증되지 않았다 |
| `.elf` | 미포함 — 태그 체크아웃 후 클린 빌드로 재생성(빌드 경로가 박혀 해시 대조 대상도 아님) |

## 이 폴더에서 바로 플래시

```bash
cd releases/V3.1.0R_260911
shasum -a 256 -c SHA256SUMS
openocd -f stm32f410.cfg -c "program gds_us_ctrl_v3.1.0R_260911.bin 0x08000000 verify reset exit"
```

다른 도구(CubeProgrammer CLI / J-Link)와 실패 대응은 `releases/README.md` §4·§6.

## 플래시 후 확인

LCD 첫 화면 버전 = `V3.1.0R!_260911` (`!` 는 인터록 반전판 표식, 파일명엔 없음).

## 재현 확인

```bash
git checkout 8bd0417          # 태그 발행 후엔 그 태그
rm -rf fw/build-remote && MODEL=remote ./fw.sh
shasum -a 256 fw/build-remote/gds_us_ctrl.bin   # SHA256SUMS 의 .bin 줄과 같아야 한다
```

2026-09-13 실측: 클린 빌드 해시 = 보드 플래시(2026-09-13, `3be6a5d`)에 쓴 이전 빌드 해시와 동일 `2a813206…` → 보드가 이미 이 이미지다.

## 이 릴리스에 든 것 (릴리즈 3.1.0 `hw-revA_fw-3.1.0` = `V3.1.0R_260905` 이후)

- **원격 hold-to-run 워치독** — `0x32 FEAT_CAP` bit0 · START 1/2/3 · T=600 ms · `MB_REG_COUNT` 51 (벤치 PASS, `hw-revA_fw-stage-hold-wdt`)
- **horn 모드가 진행 런을 세움** — legacy SYS_HORN 동등 복원(`96dc7d5`)
- **바이트 동일 리팩토링** — PR #1, `.bin` 무변경(거동 변화 없음)
- **원격 FC06 쓰기 → LCD VP 에코** — 19 VP + STD RUN 텍스트 재기록(방법 A) — ⚠ 벤치 미착수
- **F1** SETUP1 진입 horn shadow 시드 정정 — Safe+Horn 동시 SAVE 시 horn 모드가 꺼지던 결함(legacy 복원) — ⚠ 벤치 미착수
- 버전 문자열 `_260906` → `_260911`

상세 = `HANDOFF.md` 최상단 · `docs/changelog.md` · `docs/superpowers/plans/2026-09-11-modbus-write-lcd-echo.md`.

## 직전 버전과의 관계

부트로더 없음 — 단일 앱 이미지라 항상 전체 교체. 설정은 외부 FRAM 이라 보존되고 cfg 레이아웃 변경 없음. Modbus 계약(레지스터 51칸·`0x32`·START 값)은 `_260906` 과 동일 — 원격기 통보 완료분.

## 🔴 배포 가능 여부

**현장 배포 금지.** 인터록 반전판(`fw/include/define.h` `REMOTE_EN_INTERLOCK_INVERTED 1`, LCD `!`) — PC8 미실장 HIGH=허용이라 단선·커넥터 탈락이 "허용" 이 된다. 해제 조건 = PC8 실장 PCB + 매크로 `0` 원복 + A-1·A-5·A-13 재시험 PASS(`HANDOFF.md` 최상단). 추가로 이 보관본은 **에코·F1 벤치 미통과**다.
