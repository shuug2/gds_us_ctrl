# V3.1.1R_261115_hw31 — V31 보드용 (단일 바이너리)

> **문서 요약**: REMOTE 티어 **`V3.1.1R_261115`**(V31 은 `!` 없음) 의 **V31 보드용 보관본** — 업체 구분 없는 `.bin` 1개. 업체는 LCD 세트 `22_Config.bin` VP `0x1500` 브랜드 번호로 런타임 판별. 태그 `hw-revA_fw-stage-v31-brand`(`1d64b89`) 클린 빌드. **V31 보드 플래시·LCD 육안 확인 완료**(2026-11-15). 이 릴리스에 든 것 = V31 단일 바이너리 + LCD 런타임 브랜드 · `MODEL_TYPE(0x18)` E-STOP 가드. V30 보드용은 `../V3.1.1R_261115/`.

| 항목 | 값 |
|---|---|
| 태그 / 커밋 | `hw-revA_fw-stage-v31-brand` = `1d64b89` (`main`) |
| 툴체인 | arm-none-eabi-gcc 15.2.1 (Arm GNU Toolchain 15.2.Rel1) · cmake 4.3.1 · Ninja · openocd 0.12.0 |
| 빌드 | 태그 체크아웃 깨끗한 워크트리에서 `HW_REV=v31 MODEL=remote ./fw.sh` → `fw/build-v31-remote/` (BRAND 축 없음) |
| 산출 | FLASH 68,724 B (52.43 %) · `gds_us_ctrl_hw31_v3.1.1R_261115.bin` SHA `ec05d9b3…` |
| 대상 보드 | **V31**(부저 PB9 · mon USART2 J3 · UART 부트로더 점프) · STM32F410RBT. 🔴 V30 보드에 올리면 mon 출력이 부저로 나간다 |
| 업체 | LCD 세트가 정한다 — VP `0x1500` 1 gdsonic · 2 moohan · 3 dongin(DIAMT) · 4 powertech · 5 maketech, 0·미지·읽기 실패 → gdsonic. 정본 `fw/include/app_brand.h`. 업체 전환 = 그 업체 `DWIN_SET` 을 SD 로 |
| 보드 확인 | ✅ 2026-11-15 이 파일 그대로 openocd 플래시 `Verified OK` → LCD `V3.1.1R_261115` · `GDS-…`(gdsonic 세트) 육안 |
| CTRL 빌드 · `.elf` | 미포함 |

## 이 폴더에서 바로 플래시

```bash
cd releases/V3.1.1R_261115_hw31
shasum -a 256 -c SHA256SUMS
openocd -f stm32f410.cfg -c "program gds_us_ctrl_hw31_v3.1.1R_261115.bin 0x08000000 verify reset exit"
# 또는 ST-LINK 없이 UART(J3, 앱이 살아 있을 때) = releases/README.md §4-E
```

## 플래시 후 확인

LCD 버전 = **`V3.1.1R_261115`** (`!` 없음 = V31 보드 표식, 3.1.1~). 인터록 반전판(물리 인터록 없음)인 것은 V30 과 같다. 모델명 = LCD 세트의 업체(번호 없는 세트 = GDS-).

## 재현 확인

```bash
git checkout hw-revA_fw-stage-v31-brand
rm -rf fw/build-v31-remote && HW_REV=v31 MODEL=remote ./fw.sh
shasum -a 256 fw/build-v31-remote/gds_us_ctrl.bin   # SHA256SUMS 의 .bin 줄과 같아야 한다
```

## 이 릴리스에 든 것

- **V31 단일 바이너리 + LCD 런타임 브랜드** — 부팅·패널 재기동 때 VP `0x1500` 읽기(최대 3회×120 ms), RAM 만. 벤치 B-1~B-3·B-5 PASS. 설계 `docs/superpowers/specs/2026-11-15-lcd-brand-id-design.md`
- **`MODEL_TYPE(0x18)` E-STOP 가드** — CTRL/REMOTE 벤치 PASS (REMOTE 는 원격 게이트가 바깥층)
- V31 보드 지원(USART2 mon · UART 부트로더 점프, V-1~V-10 PASS 2026-11-14) — 보관본은 이번이 처음

## 알려진 제한

- 부팅 4 s 뒤 LCD 를 꽂으면 패널이 SYS_PIC_NOW 를 보내지 않아 로고 화면에서 멈춘다(전원 재투입으로 복구). 기존 동작, 양산(부팅 시 LCD 연결) 영향 없음 — `HANDOFF.md` 열린 항목.
- moohan·dongin LCD 세트의 브랜드 번호 기록은 lcd 세션 진행 중 — 번호 없는 세트는 gdsonic 모델명이 나온다.

## 🔴 배포 가능 여부

**배포 가능.** 인터록 반전판 위험 수용·배포 금지 해제(2026-11-15), MODEL_TYPE 가드 포함 — V31 출하 보류 조건(가드 포함 새 보관본) 충족.
