# V3.1.1R_261115 — V30 보드용 (업체별 4개)

> **문서 요약**: REMOTE 티어 `V3.1.1R!_261115` 의 **V30 보드용 보관본** — 업체별 `.bin` 4개(gdsonic·moohan·diamt·powertech). 태그 `hw-revA_fw-stage-v31-brand`(`1d64b89`) 깨끗한 트리 클린 빌드. 이 릴리스에 든 것 = `MODEL_TYPE(0x18)` E-STOP 가드 · 모델명 규칙 순수 모듈 분리(거동 동일). ⚠ **V30 보드 미플래시** — 모델명 동일성은 host 골든 테스트(90조합) 근거. V31 보드용은 `../V3.1.1R_261115_hw31/`.

| 항목 | 값 |
|---|---|
| 태그 / 커밋 | `hw-revA_fw-stage-v31-brand` = `1d64b89` (`main`) |
| 툴체인 | arm-none-eabi-gcc 15.2.1 (Arm GNU Toolchain 15.2.Rel1) · cmake 4.3.1 · Ninja · openocd 0.12.0 |
| 빌드 | 태그 체크아웃 깨끗한 워크트리에서 `HW_REV=v30 BRAND=<brand> MODEL=remote ./fw.sh` → `fw/build-v30-<brand>-remote/` (빌드 폴더 없는 상태에서) |
| 산출 | FLASH 67,912 B (51.81 %) — 4개 같은 크기 |
| 대상 보드 | **V30** · STM32F410RBT (128 KB). 🔴 V31 보드에 올리지 말 것(부저 무음·mon 불통) |
| 브랜드 / LCD 자산 | `gds_us_ctrl_<brand>_v3.1.1R_261115.bin` — gdsonic `03b5cdea…` · moohan `cdd01c63…` · diamt `e96e7d31…`(LCD `hw/lcd/lcd_V30/dongin/`) · powertech `23c861eb…` |
| 보드 확인 | **미실시** — 이번 발행은 V31 보드로만 확인(사용자 결정 2026-11-15). 모델명 규칙 분리는 host 골든 90조합 바이트 일치, 가드는 V31 보드 CTRL/REMOTE 벤치 PASS |
| CTRL 빌드 · `.elf` | 미포함 |

## 이 폴더에서 바로 플래시

```bash
cd releases/V3.1.1R_261115
shasum -a 256 -c SHA256SUMS
openocd -f stm32f410.cfg -c "program gds_us_ctrl_<brand>_v3.1.1R_261115.bin 0x08000000 verify reset exit"
```

다른 도구·실패 대응 = `releases/README.md` §4·§6. 양산 = `production/README.md`.

## 플래시 후 확인

LCD 버전 = **`V3.1.1R!_261115`** — `!` 는 3.1.1~ **V30 보드 표식**(V31 은 `!` 없음). 인터록 반전판(물리 인터록 없음)인 것은 V30·V31 같다. 모델명 = 업체 규칙(GDS-/MH-/…).

## 재현 확인

```bash
git checkout hw-revA_fw-stage-v31-brand
rm -rf fw/build-v30-<brand>-remote && HW_REV=v30 BRAND=<brand> MODEL=remote ./fw.sh
shasum -a 256 fw/build-v30-<brand>-remote/gds_us_ctrl.bin   # SHA256SUMS 의 해당 줄과 같아야 한다
```

2026-11-15 실측: `cb7645e`(버전만 올린 커밋)와 `1d64b89` 두 트리의 V30 빌드 해시 4개 동일.

## 이 릴리스에 든 것 (`V3.1.0R_260911` 이후)

- **`MODEL_TYPE(0x18)` E-STOP 가드** — E-STOP 활성 중 Modbus 쓰기 침묵 거부(`0x17`·`0x30` 제외, LCD 경로 레거시). 실보드(V31) PASS. `fw/src/app_modbus.c:585`
- **모델명 규칙 순수 모듈 `app_brand`** — `.bin` 바이트 상이, 출력 동일(host 골든)
- 버전 `_260911` → `V3.1.1R!_261115`

상세 = `docs/changelog.md` [3.1.1] · `HANDOFF.md`.

## 직전 버전과의 관계

단일 앱 이미지 전체 교체. 설정 = 외부 FRAM 보존, cfg 레이아웃 무변경. Modbus 레지스터 맵 무변경(가드는 거동만 — remote 통보 완료).

## 🔴 배포 가능 여부

**배포 가능.** 인터록 반전판(물리 인터록 없음)은 사용자 위험 수용·배포 금지 해제(2026-11-15). `V3.1.0R_260911` 에 없던 MODEL_TYPE 가드가 들어 있다. ⚠ V30 보드 실기 확인은 하지 않았다(위 "보드 확인").
