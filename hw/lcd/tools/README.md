# lcdset — 업체별 DGUS LCD 세트 관리 CLI

> **문서 요약**: 업체(BRAND)별 DGUS LCD 세트를 만들고·검사하고·HTML 로 보여 주는 명령줄 도구(Python 3.9 표준 라이브러리 + macOS `sips`).
> - 같은 색 계열은 기준 세트(검은색 = `gdsonic`, 흰색 = `moohan`)에서 파생하고, 업체마다 바꿔 끼우는 칸은 **로고(`32.icl` #0)** 와 **레벨 바(`48.icl` #11, 검은색 계열만)** 뿐이다.
> - 업체 목록은 `brands.json` 이 정한다. 작업 원본(`~/dev/work/gdsonic/gds_lcd_V30/<업체>/`)에 입력 그림을 두고 `build` → `check` → `report` 로 확인한다.
> - 정본(`../lcd_V30/`) 반영은 `sync-preview` 로 미리보기만 하고, 실행은 사람이 한다.

## 명령

```zsh
cd hw/lcd/tools
python3 lcdset.py list                    # 업체 표·상태(DWIN_SET·입력 그림 유무)
python3 lcdset.py check [업체...]         # 계약·무결성 검사, FAIL 이면 종료 코드 1
python3 lcdset.py build <업체> [--force]  # 기준 세트 + 로고/바 → <작업원본>/<업체>/DWIN_SET
python3 lcdset.py report [업체...] --open # <작업원본>/_report/index.html
python3 lcdset.py sync-preview <업체>     # 작업 원본 → 정본 rsync 미리보기 + 실행할 명령 출력
python3 lcdset.py export <업체> <빈폴더>  # 패널용 축소 사본 (--dry-run 은 계획만)
python3 test_lcdset.py                    # 자기검사 (작업 원본 실데이터 사용)
```

경로는 `--work`(또는 환경변수 `LCD_WORK`)와 `--canon` 으로 바꿀 수 있다.

## 새 업체 추가

1. `brands.json` 에 한 줄을 추가한다. 형식: `family`(black|white), `fw_brand`(펌웨어 `-DBRAND`, 없으면 `null`), `logo`, 검은색이면 `bar`.
2. 작업 원본 `<업체>/brand_src/` 에 입력 그림을 둔다.
   - `00_logo.jpg` — **800×480**
   - `11_lv_bar_64x16.jpg` — **64×16**, 검은색 계열만
3. `build <업체>` → `report <업체> --open` 으로 그림을 확인한다.
4. 패널(SD 카드)에서 실기 확인 → `sync-preview` → 정본 반영 → ctrl 저장소에 커밋.
   `build` 는 `brand_id` 를 `22_Config.bin` VP 0x1500(파일 오프셋 0x2A00, 2바이트 상위 먼저)에 쓴다. V31 펌웨어가 부팅 때 이 번호로 업체를 정한다(정본 ctrl `fw/include/app_brand.h`: 1 gdsonic · 2 moohan · 3 dongin · 4 powertech · 5 maketech, 6 sama · 7 sonosys 예약).
5. 펌웨어 쪽(`fw/include/define.h` BRAND, `app_lcd.c` 모델명 문자열)은 별도 작업이다.

입력 jpg 가 DGUS 형식(양자화 표 전부 1, 표준 허프만 표, 4:2:0 — 기존 디자인 원본 jpg 들이 이 형식)이면 스캔 데이터를 그대로 옮긴다(`passthrough`, 무손실). 그 밖의 그림(png, 다른 jpg)은 픽셀을 풀어 같은 표로 다시 인코딩한다(`encode`, 4:2:0 색 손실이 약간 있음). 어느 방식이었는지는 `BUILD.txt` 에 남는다.

## export — 패널용 축소 사본 (10 MB → 약 3 MB)

화면은 ASCII 만 쓰고, 패널은 icl·bin·hzk(+lib·cfg)만 읽는다(사용자 2026-11-15). 그래서 SD 카드에는 이것만 담는다:

- **jpg·bak·Thumbs 제외** — 디자인 원본이라 구동에 쓰지 않는다.
- **ASCII 에 안 쓰는 폰트 제외** (`1_verdana.bin`, `23_나눔스퀘어.bin`). 글자 위젯은 모두 GBK 계열 인코딩이라 ASCII 는 Font0 으로 그린다. 숫자 위젯도 전부 lib 0 이다.
- **font0(`0_verdana_ASC.HZK`)은 필요한 폭까지만** 남긴다. 화면이 쓰는 최대 ASCII 폭은 32 이다(글자 = X/2, 숫자 = Font_X). 그래서 3.08 MB → 0.42 MB.
- 원본 세트는 바꾸지 않는다. 대상은 빈 폴더만 받는다(남은 파일도 패널에 올라가기 때문).
- 화면이 바뀌어 전제가 깨지면 export 를 거부한다. 거부 조건:
  - GBK 가 아닌 글자 인코딩
  - 처음 보는 표시 형식
  - 터치 입력 키보드
  - 폭 64 초과

⚠ **큰 글자가 안 보이던 원인으로 추정**: 패널은 폰트 N 을 플래시 N×256 KB 에 쓴다. `1_verdana.bin`(1.1 MB, 0x40000~)이 font0 의 폭 27(일부)~64 영역을 덮어쓴다. 그 영역에 있는 큰 숫자(폭 30·32)와 큰 글자(p1 모델명·p17, 폭 32)가 깨진다. export 사본은 font1 이 없으므로 이 겹침이 사라진다. **실기 확인 필요.**

## check 항목

| 항목 | 수준 |
|---|---|
| `32.icl`·`48.icl` CRC·크기·오프셋 | FAIL |
| p21/23/25/27 모드 아이콘 VP = 0x140c, 0x140b 없음 (09-13 결함) | FAIL |
| `DWIN_SET/*.cfg` 0x20=5A·0x21=10 (2025 배치 터치 죽음) | FAIL |
| `13TouchFile.bin`·`14ShowFile.bin`·`22_Config.bin` = 기준 세트 (VP·키 계약, 22 는 VP 0x1500 2바이트 제외) | FAIL |
| 22_Config VP 0x1500 브랜드 번호 = `brands.json` `brand_id` (gdsonic 은 0 도 PASS) | 다르면 FAIL, 0(미기록)이면 WARN |
| 기준 세트와 파일 목록 차이 | WARN |
| icl 칸이 업체 칸(로고·바) 밖에서 기준과 다름 | WARN |

## 파일 형식 메모

`dgus.py` 머리 주석에 정리했다(2026-11-15 실파일 13개로 왕복 바이트 일치 검증). 요점:

- ICL = `DGUS_3` + CRC16(Modbus LE, 0x08~끝) + 크기 + ID별 오프셋 표 + 항목(폭·높이·626·L + JPEG).
- DGUS 툴의 JPEG 는 SOS 길이를 15 로 쓴다(표준은 12). 그래서 icl 에서 그대로 꺼낸 jpg 는 일반 뷰어가 거부한다. `report` 는 표준으로 고쳐서 꺼낸다.

## 한계

- 만든 icl 은 형식·디코딩만 검증했다. **패널 실기 확인 전에는 양산에 쓰지 않는다.**
- 기준 세트(gdsonic·moohan) 자체의 편집은 DGUS 툴에서 한다. 이 도구는 파생 업체만 만든다.
