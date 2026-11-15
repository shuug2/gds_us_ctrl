# V3.1.2R_261115_hw31 — V31 보드용 (단일 바이너리)

> **문서 요약**: REMOTE 티어 **`V3.1.2R_261115`**(V31 은 `!` 없음) 의 **V31 보드용 보관본** — 업체 구분 없는 `.bin` 1개(업체 = LCD 세트 VP `0x1500`). 태그 `hw-revA_fw-stage-hold-runtime`(`da41e7a`) 클린 빌드. 이 릴리스에 든 것 = 원격 hold 런(START=2) HAND RUN TIME ceiling 제외. V30 보드용은 `../V3.1.2R_261115/`.

| 항목 | 값 |
|---|---|
| 태그 / 커밋 | `hw-revA_fw-stage-hold-runtime` = `da41e7a` (`main`) |
| 툴체인 | arm-none-eabi-gcc 15.2.1 (Arm GNU Toolchain 15.2.Rel1) · cmake 4.3.1 · Ninja · openocd 0.12.0 |
| 빌드 | 태그 체크아웃 깨끗한 워크트리에서 `HW_REV=v31 MODEL=remote ./fw.sh` → `fw/build-v31-remote/` |
| 산출 | FLASH 68,764 B (52.46 %) · `gds_us_ctrl_hw31_v3.1.2R_261115.bin` SHA `15f5e05a…` |
| 대상 보드 | **V31**(부저 PB9 · mon USART2 J3 · UART 부트로더 점프). 🔴 V30 보드에 올리면 mon 출력이 부저로 나간다 |
| 업체 | LCD 세트가 정한다 — VP `0x1500` 1 gdsonic · 2 moohan · 3 dongin(DIAMT) · 4 powertech · 5 maketech, 그 외 → gdsonic. 업체 전환 = 그 업체 `DWIN_SET` 을 SD 로 |
| 보드 확인 | ✅ 2026-11-15 이 파일 그대로 openocd 플래시 `Verified OK` → 전원 재투입 → LCD `V3.1.2R_261115` · `GDS-…`(gdsonic 세트) 육안 |
| CTRL 빌드 · `.elf` | 미포함 |

## 이 폴더에서 바로 플래시

```bash
cd releases/V3.1.2R_261115_hw31
shasum -a 256 -c SHA256SUMS
openocd -f stm32f410.cfg -c "program gds_us_ctrl_hw31_v3.1.2R_261115.bin 0x08000000 verify reset exit"
# 또는 ST-LINK 없이 UART(J3, 앱이 살아 있을 때) = releases/README.md §4-E
```

## 플래시 후 확인

LCD 버전 = **`V3.1.2R_261115`** (`!` 없음 = V31 보드 표식). 모델명 = LCD 세트의 업체.
⚠ SWD 플래시 직후 RTU 무응답이 한 번 관찰됐다(전원 재투입으로 복구, 미재현) — 플래시 후 전원 재투입 권장.

## 재현 확인

```bash
git checkout hw-revA_fw-stage-hold-runtime
rm -rf fw/build-v31-remote && HW_REV=v31 MODEL=remote ./fw.sh
shasum -a 256 fw/build-v31-remote/gds_us_ctrl.bin   # SHA256SUMS 의 .bin 줄과 같아야 한다
```

## 이 릴리스에 든 것 (`V3.1.1R_261115_hw31` 이후)

- **원격 hold 런 RUN TIME 제외** — START=2(워치독 무장) 런은 HAND `limit_on_time` ceiling 에서 빠진다(LCD 터치 RUN 과 동형). 탭 START=1 · energy/OVTIME · 30 s 안전 ceiling · 워치독 600 ms 는 그대로. 벤치(V31 HAND, RTU 38400, Windows HMI): hold 7.1 s 유지·뗌 정지 · HMI 상한 10 s · 탭 START 500 ms 정지 · 케이블 탈거 → `[mb] hold wdt trip`.

## 알려진 제한

- 부팅 4 s 뒤 LCD 를 꽂으면 로고 화면에서 멈춘다(전원 재투입으로 복구, 기존 동작, 양산 무관).

## 🔴 배포 가능 여부

**배포 가능.** 인터록 반전판 위험 수용·배포 금지 해제(2026-11-15), MODEL_TYPE 가드 포함.
