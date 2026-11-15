# V3.1.2R_261115 — V30 보드용 (업체별 4개)

> **문서 요약**: REMOTE 티어 `V3.1.2R!_261115` 의 **V30 보드용 보관본** — 업체별 `.bin` 4개(gdsonic·moohan·diamt·powertech). 태그 `hw-revA_fw-stage-hold-runtime`(`da41e7a`) 깨끗한 트리 클린 빌드. 이 릴리스에 든 것 = 원격 hold 런(START=2) HAND RUN TIME ceiling 제외(+ 3.1.1 의 MODEL_TYPE 가드). ⚠ **V30 보드 미플래시**. V31 보드용은 `../V3.1.2R_261115_hw31/`.

| 항목 | 값 |
|---|---|
| 태그 / 커밋 | `hw-revA_fw-stage-hold-runtime` = `da41e7a` (`main`) |
| 툴체인 | arm-none-eabi-gcc 15.2.1 (Arm GNU Toolchain 15.2.Rel1) · cmake 4.3.1 · Ninja · openocd 0.12.0 |
| 빌드 | 태그 체크아웃 깨끗한 워크트리에서 `HW_REV=v30 BRAND=<brand> MODEL=remote ./fw.sh` → `fw/build-v30-<brand>-remote/` |
| 산출 | FLASH 67,952 B (51.84 %) — 4개 같은 크기 |
| 대상 보드 | **V30** · STM32F410RBT. 🔴 V31 보드에 올리지 말 것(부저 무음·mon 불통) |
| 브랜드 / LCD 자산 | `gds_us_ctrl_<brand>_v3.1.2R_261115.bin` — gdsonic `ead0b383…` · moohan `b2f337e8…` · diamt `f97dcecf…`(LCD `hw/lcd/lcd_V30/dongin/`) · powertech `e4cad179…` |
| 보드 확인 | **미실시** — V31 보드로만 확인(hold 수정은 V31 HAND 벤치 PASS, 모델·보드 무관 코드) |
| CTRL 빌드 · `.elf` | 미포함 |

## 이 폴더에서 바로 플래시

```bash
cd releases/V3.1.2R_261115
shasum -a 256 -c SHA256SUMS
openocd -f stm32f410.cfg -c "program gds_us_ctrl_<brand>_v3.1.2R_261115.bin 0x08000000 verify reset exit"
```

다른 도구·실패 대응 = `releases/README.md` §4·§6. 양산 = `production/README.md`.

## 플래시 후 확인

LCD 버전 = **`V3.1.2R!_261115`** — `!` = V30 보드 표식(V31 은 `!` 없음). 인터록 반전판(물리 인터록 없음)은 V30·V31 같다.

## 재현 확인

```bash
git checkout hw-revA_fw-stage-hold-runtime
rm -rf fw/build-v30-<brand>-remote && HW_REV=v30 BRAND=<brand> MODEL=remote ./fw.sh
shasum -a 256 fw/build-v30-<brand>-remote/gds_us_ctrl.bin   # SHA256SUMS 의 해당 줄과 같아야 한다
```

## 이 릴리스에 든 것 (`V3.1.1R_261115` 이후)

- **원격 hold 런 RUN TIME 제외** — START=2(워치독 무장) 런은 HAND `limit_on_time` ceiling 에서 빠진다(LCD 터치 RUN 과 동형). 탭 START=1 · energy/OVTIME · 30 s 안전 ceiling · 워치독 600 ms 는 그대로. 벤치(V31 HAND, RTU 38400, HMI) PASS.

상세 = `docs/changelog.md` [3.1.2] · `HANDOFF.md`.

## 직전 버전과의 관계

단일 앱 이미지 전체 교체. 설정 = 외부 FRAM 보존, cfg 레이아웃·Modbus 레지스터 맵 무변경.

## 🔴 배포 가능 여부

**배포 가능.** 인터록 반전판(물리 인터록 없음)은 사용자 위험 수용·배포 금지 해제(2026-11-15). ⚠ V30 보드 실기 확인은 하지 않았다.
