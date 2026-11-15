# production/ — 양산 묶음 (생산라인 전달용)

> **문서 요약**: 생산라인에 넘기는 **현재 양산 기준 한 벌**. `controller/<FW 버전>/`(지금 `V3.1.0R_260911`) 안의 파일 = SEGGER Flasher Portable PLUS 루트에 통째 복사(업체별 슬롯), `jflash/<FW 버전>/` = J-Flash PC 로 직접 굽는 프로젝트+HEX, `lcd/<업체>/DWIN_SET/` = 그 업체 LCD 의 SD 카드(FAT32) 루트에 복사. 둘 다 `models.txt` 한 파일에서 `mkproduction.sh` 로 생성한다. 정본은 FW = `releases/<버전>/`, LCD = `hw/lcd/lcd_V30/<업체>/` 이고 여기는 복사·변환본이다. **결과물은 저장소 밖 `gdsonic/production/ctrl/`** 에 생성되고(이 폴더엔 입력·스크립트만), 묶음 이력은 `gdsonic/production/HISTORY.md`(그 폴더 git)가 관리한다(2026-11-15). PC8 인터록 반전판(`!`) 이 최종 상태로 확정돼 **배포 금지 해제**(사용자 결정 2026-11-15).

## 양산 기준표

| 슬롯 | 업체 | FW (LCD 표시 버전) | FW 보관본 | LCD 세트 (커밋) | 배포 |
|---|---|---|---|---|---|
| 1 | gdsonic | `V3.1.0R!_260911` | `releases/V3.1.0R_260911/gds_us_ctrl_v3.1.0R_260911.bin` (`2a813206…`) | `hw/lcd/lcd_V30/gdsonic` (`235266d`, 10-18 `48.icl` #56 시리얼 ON 수정 포함) | ✅ 허용 |
| 2 | moohan | `V3.1.0R!_260911` | `releases/V3.1.0R_260911/gds_us_ctrl_moohan_v3.1.0R_260911.bin` (`7ae37a43…`) | `hw/lcd/lcd_V30/moohan` (`2eb516b`, 10-13) | ✅ 허용 |
| 3 | diamt | `V3.1.0R!_260911` | `releases/V3.1.0R_260911/gds_us_ctrl_diamt_v3.1.0R_260911.bin` (`9566fad5…`) | `hw/lcd/lcd_V30/dongin` (`235266d`, 10-18 `32.icl`·`48.icl`(빨간 아이콘) 갱신 포함, 작업 중 세트) | ✅ 허용 |
| 4 | powertech | `V3.1.0R!_260911` | `releases/V3.1.0R_260911/gds_us_ctrl_powertech_v3.1.0R_260911.bin` (`d071e033…`) | ⚠ 없음 — LCD 세트 미반입 (`models.txt` 에 `-`) | ✅ 허용 |

⚠ **배포 칸 근거**: PC8 인터록 극성 반전판(`REMOTE_EN_INTERLOCK_INVERTED 1`, LCD 버전에 `!`) = 물리 인터록 없음 — 이것이 최종 상태이고 사용자가 위험을 수용해 배포를 허용했다(2026-11-15, 루트 `HANDOFF.md`). **출하 전 이 표의 배포 칸을 반드시 본다.**

## 생산라인 작업

**컨트롤러 (Flasher Portable PLUS)**
1. Flasher 를 USB 로 PC 에 연결 → 드라이브로 보인다
2. `controller/V3.1.0R_260911/` 안의 파일 전부를 드라이브 루트에 덮어쓰기 (버전 폴더 자체가 아니라 그 안의 파일)
3. 선택/전원 버튼으로 슬롯(업체) 선택 → 보드 SWD 연결 → PROG
4. 판정: Flasher 성공 표시 → 보드 전원 재투입 → LCD 버전 = 표의 FW 칸

**컨트롤러 — J-Flash PC 로 직접 (J-Link 연결, 소량·수리용)** — `jflash/V3.1.0R_260911/`
1. J-Flash → File › Open Project → `gds_us_ctrl_f410.jflash`
2. File › Open data file → `<업체>.hex` (주소 0x08000000 이 HEX 안에 들어 있어 입력 불필요)
3. 보드 SWD 연결 → Target › Production Programming (F7) — 걸친 섹터 소거 → 프로그램 → 검증 → 앱 시작
4. 판정: 위 Flasher 와 같음
- 같은 프로젝트+HEX 로 File › Download config & data file to Flasher 도 가능하지만 **Flasher 의 `FLASHER.INI` 다중 슬롯 구성은 갱신되지 않는다** — 양산 Flasher 는 위 `controller/` 복사 방식만 쓴다.

**LCD (SD 카드)**
1. FAT32 SD 카드 루트에 `lcd/<업체>/DWIN_SET/` 폴더를 **폴더째** 복사(이름 `DWIN_SET` 그대로, 파일 개명 금지)
2. 패널 전원 끈 상태로 SD 삽입 → 전원 투입 → 갱신 완료 표시 후 전원 끄고 SD 제거
3. 판정: 업체 로고·배경 육안

## 갱신 (개발 PC, J-Flash 설치 필요)

1. FW = 새 보관본을 `releases/<버전>/` 에 만든다(`releases/README.md` §7). LCD = `hw/lcd/lcd_V30/<업체>/` 에 동기화(그 README)
2. `models.txt` 의 그 업체 줄을 고친다 — 업체 추가 = 한 줄 추가(슬롯 5~). LCD 세트가 아직 없으면 넷째 칸에 `-` (FW 만 생성)
3. `production/mkproduction.sh` → `gdsonic/production/ctrl/` 의 `controller/<버전>/`·`jflash/<버전>/`·`lcd/` 재생성 + 이 README 사본. `.hex`·LCD 는 같은 입력이면 바이트 동일, **`.cfg`/`.dat` 는 헤더에 J-Flash 생성 시각(분)이 들어가 재생성마다 바이트·`SHA256SUMS` 가 바뀐다**(헤더 256 B 뒤 내용은 동일 — 실질 변화 없음). `<버전>` 은 `models.txt` 의 보관본 경로 `releases/<버전>/` 에서 따오며, 슬롯마다 버전이 다르면 멈춘다
4. FW 버전이 바뀌었으면 소스 묶음 `gds_us_ctrl_fw_src_<버전>.zip` 도 새로 만들어 `gdsonic/production/ctrl/` 에 둔다 — `git archive <커밋> fw fw.sh` + `build_production.sh`·`SHA256SUMS`·`README.md`(현 zip 안의 것을 고쳐 씀). 압축을 푼 새 폴더에서 `./build_production.sh` 가 전부 OK 여야 넣는다
5. 위 기준표 갱신 → 이 저장소 커밋 → 3 재실행(README 사본 갱신) → `gdsonic/production/HISTORY.md` 에 한 줄 + 그 폴더 커밋

```
gds_us_ctrl/production/      ← 입력 (이 저장소)
├── models.txt               ← <슬롯> <brand> <FW 보관본> <LCD 세트>  (유일한 수동 편집 파일)
├── mkproduction.sh
└── gds_us_ctrl_f410.jflash  ← STM32F410RB · SWD 4 MHz · 걸친 섹터만 소거→프로그램→검증→앱 시작, OTP 비활성

gdsonic/production/ctrl/     ← 결과물 (저장소 밖, 생산라인에 넘기는 것)
├── README.md                ← 이 문서 사본 (mkproduction.sh 가 복사)
├── controller/<FW 버전>/    ← FLASHER.INI + <brand>.cfg/.dat + SHA256SUMS  (슬롯 N = INI 인덱스 N-1)
├── jflash/<FW 버전>/        ← J-Flash PC 용: gds_us_ctrl_f410.jflash 복사본 + <brand>.hex + SHA256SUMS
├── lcd/<brand>/DWIN_SET/    ← + lcd/SHA256SUMS
└── gds_us_ctrl_fw_src_V3.1.0R_260911.zip  ← 위 FW 4종의 빌드 소스(커밋 `9ff6225`) + `build_production.sh`(4종 클린 빌드·SHA 대조) + 설명 README  (수동 생성, 위 4)
```

- J-Flash 에서 Flasher 로 직접 다운로드하면 `FLASHER.INI` 가 갱신되지 않는다 — 반드시 `controller/<버전>/` 안의 파일을 통째 복사.
- 드라이브·SD 내용 대조 = 각 `SHA256SUMS`.
- Flasher 형식 근거: SEGGER KB "Flasher - Project Management" (`[FILES]`, `DataFileX`/`ConfigFileX`, Portable PLUS 최대 99쌍).
