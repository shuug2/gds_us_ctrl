# lcd-V2 DGUS 자산 분석 (2026-09-27)

## 문서 요약

> ⚠ **2026-10-13 후속**: 이 문서의 대상 `~/dev/work/lcd-V2` 는 구판(TCP 화면 없음)이다. 정본은 이후 사용자가 제공한 `~/dev/work/gds_lcd_V30` → 저장소 `hw/lcd/lcd_V30/`. "기준 세트 = lcd_v25_new/DWIN_SET" 서술은 그 시점 사용자 지정이었고 대체됐다. bin 형식·계약 공유 검증 결과는 유효.

`~/dev/work/lcd-V2/lcd_v25_new/`(202 MB, 1225 파일)의 DWIN DGUS II 자산을 읽기 전용으로 인벤토리·해시·바이너리 파싱해 펌웨어 계약(`fw/include/dgus_lcd.h`)과 대조했다. 기준 세트 = `lcd_v25_new/DWIN_SET`(GDSONIC, 사용자 지정 정본).

- **완성 DWIN 세트는 4종**: `DWIN_SET`(GDSONIC 기준) · `DONGIN/DWIN_SET`(=DIAMT) · `MOOHAN/DWIN_SET` · `SONOSYS/DWIN_SET`. `SAM-A/DWIN_SET`은 로고 2장뿐(미완). `DWIN_SOURCE`(및 `DONGIN/DWIN_SOURCE`, 동일)는 2022-01 구판. `32_bg_*`/`48_icon_*` 는 icl 소스 이미지 폴더(사용자 확인). `DWIN_SET_portrait` 는 같은 25장의 BMP(1152054 B, 800×480) 변환본 — 세로 세트가 아니다.
- **"계열 간 VP·키코드 공유, 위치만 다름" 주장은 성립한다(예외 5건, §5)**: 기준 ↔ DONGIN ↔ MOOHAN/SONOSYS 의 13TouchFile 터치 키(VP·키값·범위) 차이 0 또는 미세(p1 0x1060/0x1070 '+' 버튼 1개 부재 등), 14ShowFile VP 집합은 MOOHAN/SONOSYS 에서 **p3/p9 의 LV_TIME 바(0x1184~0x1197, 20 VP) 40 위젯 부재** + 텍스트 인코딩 바이트 차이뿐. 좌표(X/Y/영역)만 배경마다 다르다.
- **저장소 `hw/lcd/dgus/` 는 lcd-V2 의 어느 세트와도 바이너리 불일치** — 기준 세트에 **COMM 페이지 25/27 + VP 0x140b/0x140c/0x140d/0x140f + IP/NM/GW 텍스트(0x1470/0x1480/0x1490) + 배경 25~28(`gds_lcd_v20/`) + 아이콘 56~59(소스 없음)** 를 얹은 상위판. lcd-V2 에는 이 확장이 반영된 세트가 **하나도 없다**(MOOHAN 만이 아니라 기준 GDSONIC 세트 자체가 TCP 화면 없음).
- **MOOHAN/SONOSYS = 밝은색 계열(배경 평균 휘도 173~214/255), 나머지 = 검은색 계열(30~60)**. MOOHAN 과 SONOSYS 는 로고(0페이지)만 다르고 bin·icl·폰트 전부 동일 계열(13/14/22 bin 바이트 동일).
- 업체↔BRAND: GDSONIC=`DWIN_SET`/`32_bg_gdsonicg`, DIAMT=`DONGIN`/`32_bg_dongin`, POWERTECH=`32_bg_powertech`(로고만 다른 배경 소스, 완성 세트 없음), MAKETECH=`32_bg_mktech`(동일), MOOHAN=`MOOHAN/`, 신규 SAMA=`SAM-A/`(로고+배경 3장), 신규 SONOSYS=`SONOSYS/`.
- bin 형식 해독은 알려진 값으로 검증됨(§4). 14ShowFile 페이지 테이블 = `dgus_lcd.h` 페이지 ID 와 1:1. **13TouchFile 은 32 B 고정 정렬이 아니다**(sync_return 0x08 = 48 B) — 이전 기록(2026-09-12 §5)의 "32B 레코드"는 정정 필요.
- 파서·스크립트: `/private/tmp/claude-501/-Users-tknoh-dev-work-gds-us-ctrl/79afe23c-53a2-47ee-b6bd-019af88cdee9/scratchpad/` 의 `dgus_parse.py`(13/14/22 bin) · `icl_extract.py`(icl→JPEG) · `pix.py`(sips 픽셀 비교) · `contract.py`(계약 diff) · 결과 `contract_out.txt`/`contracts.json`/`icl_pixmatch.json`/`hashes_nfc.json`.

---

## 1. 인벤토리

루트 `lcd_v25_new/` 파일: `DWprj.hmi`(DGUS 프로젝트 INI — 800×480, SP 0x5000, ProVersion 647, 페이지 00~24 이미지 목록) · `DWprj.tft`(.NET 직렬화, DwinTerminal 7.3.6) · `DWprj.dhmi`(DGUSII_Interface 헤더) · `T5LCFG.CFG` · `DisplayConfig.xls`/`TouchConfig.xls`(DGUS 툴 VP 표 내보내기, OLE) · `test.dgus`(2022-01 DGUS 프로젝트, 위젯 이름 `model_str/save_rk/energy_sl/…` 포함) · `gds_lcd_v25*.pn`(**Pencil 프로젝트**(zip: Document.xml/pages/images) — 화면 디자인 원본, 2024-02/2024-11/2025-03) · `lcd_v24_outerror.7z`, `image.zip`, `manual_pic.zip` · 폰트 `0_nanum_squere_ASC.HZK`, `0_verdana_ASC.HZK`.

| 폴더 | 역할(추정 근거) | 파일 | 13/14/22 bin | 32.icl | 48.icl |
|---|---|---|---|---|---|
| `DWIN_SET` | **완성 세트, GDSONIC 기준(정본)** — hmi 페이지 00~24 | 94 | ✓ (2024-02-07 / 2023-07-07) | 1954388 | 275720 |
| `DONGIN/DWIN_SET` | 완성 세트, DIAMT | 36 | ✓ (2023-08-18) | 1931264 | 240224 |
| `MOOHAN/DWIN_SET` | 완성 세트, MOOHAN(밝은색) + bmp 25장 | 62 | ✓ (2024-12-04 / 22_Config 2024-11-26) | 1821780 | 145220 |
| `SONOSYS/DWIN_SET` | 완성 세트, SONOSYS(밝은색) = MOOHAN 복제 + 로고 + 23~26_standard_setup4 | 123 | ✓ (MOOHAN 과 바이트 동일) | 1807404 | 145220(동일) |
| `SAM-A/DWIN_SET` | **미완** — `00_logo.bmp/jpg` 2장뿐 (2026-04-08) | 2 | ✗ | ✗ | ✗ |
| `DWIN_SOURCE` = `DONGIN/DWIN_SOURCE` | **구판**(2022-01 bin, 페이지 1~17, 48.icl 37개) — 전 파일 해시 동일 | 33 | ✓ (2022-01-07) | ✗ | 196608 |
| `32_bg_gdsonicg` / `_powertech` / `_mktech` | 배경 소스(00~24 jpg) + **13/14/22 bin 사본** + 32.icl | 29 | ✓ (13Touch=DONGIN 세트 #4, 14Show 는 어느 세트와도 불일치 #11) | 각각 다름 | – |
| `32_bg_dongin` | 배경 소스, bin 없음 | 26 | ✗ | 2025472 | – |
| `DONGIN/32_bg_dongin` | 배경 소스 + `(from Home-pc)` 충돌 복제 13장 | 37 | ✗ | =DONGIN/DWIN_SET | – |
| `MOOHAN/32_bg_mh`, `SONOSYS/32_bg_sono` | 배경 소스(00~26, 23~26 standard_setup4) | 28 | ✗ | =각 DWIN_SET | – |
| `SAM-A/32_bg_sa` | 배경 소스 4장(로고 + MOOHAN 01~03 동일) | 4 | ✗ | ✗ | – |
| `48_icon_green` / `48_icon_red` / `DONGIN/48_icon_green` | 아이콘 소스(이름 있는 파일 1~55 / 1~53) + 48.icl; `bg/` 에 어두운 배경 25장 | 56/56/54 | – | – | 251260/251308/192512 |
| `48_icon_mh`, `MOOHAN/48`, `SONOSYS/48_icon` | 아이콘 소스(번호 파일 1~55) | 56/55/56 | – | – | 275720(=DWIN_SET) / – / 145220(=MOOHAN) |
| `gds_lcd_v20` | **25~28_multi_hand_setup3e** 배경 4장(2026-08-30) — 저장소 세트 p25~28 소스 | 4 | – | – | – |
| `DONGIN/gds_lcd_v20` | 구판 아이콘 1~12 (2021-11) | 12 | – | – | – |
| `DWIN_SET_portrait` | 기준 배경 25장 BMP 변환본(800×480, 2024-04-25) — "portrait" 아님 | 25 | – | – | – |
| `TFT`, `*/TFT` | DGUS 툴 페이지별 .tft(위젯 배치 직렬화) + Dropbox 충돌본 | 51/8/28/8/36 | – | – | – |
| `image`, `DONGIN/image`, `manual_pic` | 디자인 참고 jpg/png(2022, 2024-06) | 11/11/4 | – | – | – |
| `48`, `*/48`, `*/ICON`, `*/image`, `*/48_icon`(SAM-A) | 빈 폴더 | 0 | – | – | – |

업체 폴더 루트의 `DWprj.hmi`: SAM-A = SONOSYS(동일 해시, 23/24 = standard_setup3, ProVersion 648) ≠ MOOHAN(647) ≠ DONGIN(642, 23/24 = multi_hand_setup3). `gds_lcd_v25_mh.pn` 은 SAM-A/MOOHAN/SONOSYS 3곳 동일(루트본과 다름). `DisplayConfig.xls` 도 3곳 동일.

## 2. 세트 간 동일/상이 (sha256)

### 2.1 bin · icl · 폰트

| 파일 | REPO | DWIN_SET(기준) | 32_bg_gdsonicg/powertech/mktech | DONGIN/DWIN_SET | MOOHAN = SONOSYS | DWIN_SOURCE(구) |
|---|---|---|---|---|---|---|
| 13TouchFile.bin | #1 6898 B | #2 5266 | #4 5266 | #4 5266 | #5 5282 | #3 3042 |
| 14ShowFile.bin | #6 22400 | #7 22016 | #11 22016 | #9 21984 | #10 20736 | #8 20960 |
| 22_Config.bin | #12 | #13 | #13 | #13 | #15 | #14 |
| 32.icl | #16 2327504 | #17 1954388 | #17 / #21 / #22 | #18 1931264 | #19 / #20(로고만 다름) | 없음 |
| 48.icl | #24 278924 | #25 275720 (= `48_icon_mh/48.icl`) | 없음 | #27 240224 | #28 145220 | #26 196608 |
| 0_verdana_ASC.HZK, 1_verdana.bin, 23_나눔스퀘어.bin | 전 세트 동일 | | | | | |
| T5LCFG_*.cfg/.bak | 전 세트 동일(DWIN_SOURCE 만 구 `..._DGUS.CFG`) | | | | | |

- 기준 세트의 bin 3종과 바이트 동일한 세트는 **없다**. `32_bg_gdsonicg/powertech/mktech` 3곳의 bin 은 서로 동일하며 13Touch/22_Config 는 DONGIN 세트와 같고 14Show 만 다른 판(#11, 크기 같음) — bin 배포 시점의 스냅샷으로 추정.
- MOOHAN/SONOSYS 13Touch(5282 B) ≠ 기준(5266 B): 16 B 차이 = sync_return 레코드 1개(§5).
- 저장소 `hw/lcd/dgus/` 는 lcd-V2 에 없는 판(2026-05-31 에셋 수정 + p25~28). §5.1 참조.

### 2.2 배경 jpg(NN_name.jpg) — 세트별 동일 그룹

- `DWIN_SET` 00~24 = `32_bg_gdsonicg` = `48_icon_green/bg` = `32_bg_dongin`(단 00/03/04 는 DONGIN 판) = REPO(단 21~24 는 REPO 고유 판). `32_bg_powertech`, `32_bg_mktech` 는 **00_logo 만** 다르고 01~24 는 기준과 동일.
- `DONGIN/DWIN_SET` = `DONGIN/32_bg_dongin`(01~24 전부 DONGIN 고유 판, 09/17/18 은 기준과 동일). `DONGIN/32_bg_dongin/*(from Home-pc).jpg` 13장은 다른 판(동기화 충돌본).
- `MOOHAN/DWIN_SET` = `MOOHAN/32_bg_mh` = `SONOSYS/DWIN_SET`(로고 제외) = `SONOSYS/32_bg_sono`. `SAM-A/32_bg_sa` 01~03 = MOOHAN 판. SONOSYS 의 `23~26_standard_setup4*.jpg` = MOOHAN `23/24_standard_setup3*` 재명명(동일 해시).
- `DWIN_SET/23_standard_setup3.jpg`·`24_standard_setup3_bg.jpg`(2024-04-25) = **MOOHAN 의 21/22_multi_hand_setup3** 와 동일 해시(밝은색 그림이 기준 폴더에 섞여 있음) — 기준 32.icl 의 23/24 페이지는 `23/24_multi_hand_setup3.jpg`(검은색) 로 빌드돼 있어 이 두 파일은 icl 에 쓰이지 않았다.

### 2.3 아이콘 jpg(N.jpg)

- `DWIN_SET/1~55.jpg` = REPO = `SONOSYS/DWIN_SET`(29~31, 36~39, 42~49, 52~53 은 SONOSYS 가 MOOHAN 판) ; `48_icon_mh/N.jpg` 는 1~9, 29~31, 36~39, 42~49, 52~54 만 기준과 같고 10~28, 32~35, 40~41, 50~51, 55 는 다른 판 ; `MOOHAN/48/N.jpg` 는 전부 고유 판.
- `48_icon_green` vs `48_icon_red`: 11번(`11_lv_gr` / `11_lv_rd`) 만 다름(레벨바 색). `DONGIN/48_icon_green` = green 에서 54/55(overload_off/on) 빠짐.

## 3. icl ↔ 소스 이미지 매핑 (픽셀 비교)

`.icl` 은 `DGUS_3` 헤더 + BE32 오프셋 테이블(인덱스 = 이미지 번호) + 이미지별 10 B(w,h,0x0272,len) + JPEG. DWIN 툴이 JPEG 를 재인코딩(SOS 길이 15, EOI 없음, JFIF DPI 만 다름)하므로 해시 대조는 불가 → sips 로 디코드해 평균 절대 오차(MAD, 0~255)로 판정(동일 소스 = 0.0, 다른 그림 ≥ 8).

| icl | 이미지 수 · 크기 | 소스 폴더(MAD 0.0) |
|---|---|---|
| `DWIN_SET/32.icl` = `32_bg_gdsonicg/32.icl` | 25 (800×480) | 00~24 전부 `32_bg_gdsonicg`(= 기준 DWIN_SET jpg) |
| `32_bg_powertech/32.icl`, `32_bg_mktech/32.icl` | 25 | 00 = 각 폴더 로고, 01~24 = gdsonicg 와 동일 → **업체 차이는 로고 1장** |
| `32_bg_dongin/32.icl`(루트) | 25 | 00/03/04 = DONGIN 판, 나머지 = gdsonicg 판 (혼합 빌드) |
| `DONGIN/DWIN_SET/32.icl` = `DONGIN/32_bg_dongin/32.icl` | 25 | `DONGIN/32_bg_dongin` (09/17/18 은 기준과 공용) |
| `MOOHAN/DWIN_SET/32.icl` = `MOOHAN/32_bg_mh/32.icl` | 27 (00~26) | `MOOHAN/32_bg_mh`; 25/26 = 23/24 와 동일 그림 |
| `SONOSYS/DWIN_SET/32.icl` = `SONOSYS/32_bg_sono/32.icl` | 27 | 00 만 SONOSYS 로고, 01~26 = MOOHAN 과 동일 |
| REPO `32.icl` | 29 (00~28) | 00~20 = 기준, 21/22 = REPO 고유 jpg, **23/24 = 어느 폴더에도 없는 그림**, 25~28 = `gds_lcd_v20/` |
| `DWIN_SET/48.icl` = `48_icon_mh/48.icl` | 55 (110×70, 280×90, 320×50 등) | 1~9, 10~28, 32~35, 40~41, 50~51 = `DWIN_SET/N.jpg`; 29~31, 38~39, 42~49, 52, 54 = `48_icon_mh/N.jpg`; **11, 13, 34, 36, 37, 53, 55 는 정확 소스 없음** |
| REPO `48.icl` | 59 (110×50, 160×50, 256×64 등 — 기준과 **치수 다름**) | 1~55 = `48_icon_green`(11 = red 판 `11_lv_rd`), **56~59(COMM/DHCP 아이콘) 소스 없음** |
| `48_icon_green/48.icl`, `48_icon_red/48.icl` | 55 | 각 폴더 자신(red 는 11 만 다름) |
| `DONGIN/DWIN_SET/48.icl` = `DONGIN/48_icon_green/48.icl` | 53 | `48_icon_green` 판(54/55 없음) |
| `MOOHAN/DWIN_SET/48.icl` = `SONOSYS/DWIN_SET/48.icl` = `SONOSYS/48_icon/48.icl` | 55 (110×60, 140×50, 80×30 등) | `MOOHAN/48` (10, 12, 14~22 는 기준과 공용) |
| `DWIN_SOURCE/48.icl` | 37 | 구판, 34~37 소스 없음 |

업체별 차이의 icl 수준 위치: **배경(32.icl)은 로고 페이지 0 이 주 차이**(POWERTECH/MAKETECH 는 그것뿐; DIAMT 는 01~24 대부분 재작업판, 09/17/18 공용; MOOHAN/SONOSYS 는 전 페이지 밝은색 판). **아이콘(48.icl)은 검은색 계열(48_icon_green 계보)과 밝은색 계열(MOOHAN/48 계보)이 치수까지 다른 두 라이브러리**이고, 기준 `DWIN_SET/48.icl` 은 그 둘과도 다른 제3판(48_icon_mh 계보)이다.

## 4. bin 형식 해독 (확정 / 추정 / 미상)

파서 `dgus_parse.py`(stdlib). 검증: 기준·REPO 14ShowFile 페이지 테이블 = {1,3,5,7,9,10,12,13,15,17,19,21,23(,25,27)} = `dgus_lcd.h` 페이지 ID 전부 일치; REPO p27 `0x140c var_icon vmin0 vmax1 icon56~57 lib0x30`, p27 `0x140d icon58~59` ; 13Touch `0x140b return_key key 0/1/2`(p21/23 은 0/1, p25/27 은 0/1/2), `0x1080 key 1/2`, `0x1050 key 1(save)/0(cancel)` — 모두 기존 기록·펌웨어 상수와 일치.

**14ShowFile.bin** — 확정: 헤더 8 B(`14 'DGUS_2' 10`) + 페이지 테이블(엔트리 4 B = count LE16 + offset BE16, 페이지 p 엔트리 = 0x10+4p) + 32 B 위젯 레코드 `5A type SP(2) len(2) VP(2) X(2) Y(2) …`. 타입 실측 = 0x00 var_icon(X Y Vmin Vmax IconMin IconMax Lib Mode — 확정), 0x10 data_var([14]=lib [15]=font [16]=align [17]=int [18]=dec [19]=vp_mode [20]=unit_len — 추정, int/dec 는 값으로 확인), 0x11 text(색 Xs Ys Xe Ye text_len(2) font0 font1 xdots ydots encode — text_len 확정, [28] encode 바이트는 추정: 0x22/0x42/0x82/0xa2/0x02 관측, 상위 비트 의미 미상), 0x02 slider(Vbegin Vend Xbegin Xend icon Y xadj mode lib — 값으로 확정). 그 외 타입은 세트에 없음.

**13TouchFile.bin** — 확정: 레코드 = `page(2) Xs Ys Xe Ye(8) PicNext(2, 0xFF00 = 전환 없음) PicOn(2) FE type FE VP(2) …`; **32 B 고정이 아니라 type 0x08(sync_return) 만 48 B** → 이후 레코드가 16 B 밀린다(이전 32 B 가정으로는 45번째 이후가 전부 오독됨). 타입: 0x05 return_key([20:22]=키값), 0x02 incr_adjust([20]=+/−, [22:24]=step, [24:26]=min, [26:28]=max, [28]=loop; [21] 미상), 0x03 drag([20:28]=유효영역, [28:30]=min, [30:32]=max), 0x08 sync_return(`FE 01 VPsrc VPdst len` 누름 / `FE 00 …` / 뗌 `FE 01 VPsrc2 VPdst len` — 펌웨어가 시드하는 0x1081/0x1082→0x1080, 0x1085/0x1086→0x1084 와 일치; 필드 배치는 추정).

**22_Config.bin** — 확정: 131076 B = VP 0x0000~0xFFFF 초기값(BE16) + 4 B. 비제로 VP: 0x1000~0x1003 모델명 문자열, 0x1201(=10), 0x1300~0x1332, 0x1350 "Emergency", 0x1460/0x1464/0x146a 통신 텍스트, (REPO/DWIN_SOURCE 만) 0x1470~0x1495 IP/NM/GW 문자열. 기준 = "GDW-35M", REPO/MOOHAN = "GD-35M"; MOOHAN/SONOSYS 의 0x1460~0x146d 는 EUC-KR 한글(`b5cf cec4 bfc6 bcbc`) 초기값.

## 5. 계약 비교 (좌표 제외: 페이지·VP·위젯 타입·터치 VP/키값/범위·아이콘 범위·표시 길이)

기준 `DWIN_SET`: 페이지 13개, 표시 위젯 175, 터치 162. 전문 = `contract_out.txt`.

| 세트 | 표시: 기준에만 / 세트에만 / 값차이 | 터치: 기준에만 / 세트에만 / 값차이 | 22_Config |
|---|---|---|---|
| REPO | 0 / **12** (p21·23 `0x140c`; p25·27 `0x140c 0x140d 0x1470 0x1480 0x1490`) / 6 (p21·23 `0x1460/1464/146a` encode 0xa2→0x22) | 0 / **10** (p21·23 `0x140b` 0/1; p25·27 `0x1020 0x1050 0x140b`(0/1/2) `0x140f` 키패드 0~9,66,68,69,71,73,77) / 0 | 모델명 + IP 문자열 초기값 |
| `32_bg_*` bins | 0 / 0 / 6 (`0x1000` text_len 10→11 p1·3·9; p1 `0x1412` int 4→3; p3 `0x1153` icon 54~55→3~4; p17 `0x1350` len 10→15 enc 0x22→0x42) | 0 / 0 / 0 | 동일 |
| DONGIN/DWIN_SET | 2 (`0x1330` 버전 p5·p10) / 1 (`0x1330` p1) / 6 (위와 동일 6건) | 0 / 0 / 0 | 동일 |
| MOOHAN = SONOSYS | **40** (p3·p9 `0x1184~0x1197` LV_TIME 바 20 VP × 2) / 0 / 5 (p9 `0x1300/1310/1320` enc 0x82→0x02, p21·23 `0x146a` enc 0xa2→0x22) | 2 (p3·p9 `0x1100` SETUP_PARAM 키 = 설정 진입 버튼) / 0 / 5 (p1 `0x1060`·`0x1070` '+' 버튼 없음(−만), p5 `0x1202` step 10/100 → 1, p7 `0x1202` 1 → 10/100, p3·9·10 sync_return 1개 추가) | 모델명 "GD-35M", 0x1460~ 한글 |
| DWIN_SOURCE(구) | 46 / 13 / 33 | 41 / 9 / 29 | 구판 — 비교 대상 아님 |

**주장 검증 결과**: 검은색(기준·DONGIN) ↔ 밝은색(MOOHAN·SONOSYS) 사이에서 VP 주소·터치 키값·아이콘 인덱스 범위·표시 길이는 공유되고 좌표만 다르다. **예외**: ① MOOHAN/SONOSYS p3/p9 에 `LV_TIME` 바(0x1184~0x1197) 위젯 없음(펌웨어 `app_lcd_disp.c` 가 20워드를 쓰지만 표시 안 됨) ② MOOHAN/SONOSYS p3/p9 에 `SETUP_PARAM 0x1100` 키 없음(설정 진입 경로 확인 필요 — sync_return 추가분이 대체일 가능성) ③ p1 `0x1060/0x1070` '+' 키 부재 ④ p5/p7 `0x1202` 에너지 스텝 교차 ⑤ 텍스트 encode 바이트(0x02/0x22/0x42/0x82/0xa2) 가 세트마다 다름 — 이 바이트가 REPO 05-31 수정("page-show 시 VP 자동 로드")과 관련된 필드일 가능성(미상).

### 5.1 dgus_lcd.h 대조 (S=표시 페이지, T=터치 페이지)

- **전 세트 공통으로 없는 매크로**: `SETUP_MODEL 0x1084`(펌웨어 7회 사용 — sync_return 이 0x1085/0x1086→0x1084 로 간접 기록), `SETUP_PARAM_MOOHAN 0x1094`(3회), `ICON_OUTERR 0x1154`(1회), `LV_ENERGY_VAL 0x1212`(2회 기록, 표시 없음), `DISP_MULTI 0x1208`, `M_DATA_SAVE 0x1250`(미사용).
- **REPO 에만 있는 VP**: `LV_COMM_MODE 0x140b`(T21,23,25,27), `DISP_COMM_MODE 0x140c`(S21,23,25,27), `DISP_EN_DHCP 0x140d`(S25,27), `LV_ETHER_KEY 0x140f`(T25,27), `COMM_IP/NM/GW_TXT 0x1470/0x1480/0x1490`(S25,27) — 펌웨어 사용 횟수 2/11/6/5/4/2/2. **기준 GDSONIC 세트에도 없다.**
- 표시 길이 대조: `MODEL_NAME 0x1000` 위젯 text_len 10 vs 펌웨어 11 B(NUL 포함, 정상) ; `DISP_VERSION 0x1330` text_len **16** vs 펌웨어 20 B → 17자 이후 잘림(현재 문자열 14자) ; `VP_ERROR_MSG 0x1350` len 10 vs `dgus_write_text` 10 B ; `COMM_IP_TXT` len 15 vs 펌웨어 16 B ; `COMM_ADDR/SPEED/PARITY_TXT` 4/6/4 = 펌웨어 4/6/4 ; `DISP_STD_DATA1` len 12 vs 최대 11 B.
- 세트에 있으나 헤더에 없는 VP: 없음(sync_return 의 0x1081/0x1082/0x1086 은 펌웨어가 `KEY_MULTI+1`, `SETUP_MODEL+1` 로 시드 — 일치).

### 5.2 MOOHAN(=SONOSYS) 세트가 기준 대비 빠진 것

| 구분 | 누락 |
|---|---|
| 페이지 | 기준과 같은 13페이지(1~23) 보유. **TCP 페이지 25/27 은 기준에도 없음** → 기준 대비 페이지 누락 0. REPO 대비 누락 = 25(MHE)·27(STDE) 배경·위젯·터치 전부 |
| 표시 VP | p3·p9 `LV_TIME 0x1184~0x1197`(40 위젯). REPO 대비 추가 누락: p21·23 `0x140c`; p25·27 `0x140c 0x140d 0x1470 0x1480 0x1490` |
| 터치 | p3·p9 `SETUP_PARAM 0x1100` 키; p1 `0x1060/0x1070` '+' 키. REPO 대비 추가 누락: p21·23 `0x140b`(serial/eth 선택), p25·27 `0x140b` 0/1/2, `0x140f` 키패드 16키, `0x1020`(nav 1/2/3)·`0x1050`(save/cancel) |
| 아이콘 | 48.icl 55개(치수 다른 자체 라이브러리). REPO 대비 56~59(comm serial/eth, DHCP off/on) 없음 — 밝은색 판 소스도 없음 |
| 배경 | 00~26 보유. REPO 대비 25~28(`multi_hand_setup3e`, 밝은색 판 없음) 없음. 기준 대비: 기준 23/24 = multi_hand_setup3(검정) 인데 MOOHAN 23/24 = standard_setup3 그림(hmi 항목명도 standard) — 페이지 23 의 그림 의미가 두 계열에서 다름 |
| 초기값 | 0x1460~0x146d 한글 초기값(펌웨어가 덮어씀), 모델명 "GD-35M" |

다른 세트의 같은 누락: DONGIN/DWIN_SET 및 `32_bg_*` bins 는 기준과 동일 수준(TCP 없음, LV_TIME 있음). SAM-A 는 세트 자체 없음(로고·배경 3장·프로젝트 파일만). SONOSYS 는 MOOHAN 과 동일.

## 6. 업체 ↔ 펌웨어 BRAND 매핑

| 폴더/소스 | BRAND(`define.h`) | 모델 문자열 | 계열(배경 휘도 p0/p3/p9/p10) | 완성 세트 |
|---|---|---|---|---|
| `DWIN_SET`, `32_bg_gdsonicg`, `48_icon_green/red` | GDSONIC | "GDS-20H" | 검은색 (59/30/51/47) | ✓ 기준 |
| `DONGIN/`, `32_bg_dongin`, `DONGIN/48_icon_green` | DIAMT (사용자 확인) | "DIS-20H" | 검은색 (59/32/51/47) | ✓ |
| `32_bg_powertech` | POWERTECH | "PTW-2020DH" | 검은색 (55/30/51/47) — 로고만 다름 | ✗ (배경 소스 + bin 사본만) |
| `32_bg_mktech` | MAKETECH | "SMT-H15D" | 검은색 (로고 페이지만 흰색 247) | ✗ (동일) |
| `MOOHAN/`, `48_icon_mh`(?) | MOOHAN | "MH-1520DH" | **밝은색** (210/178/214/197) | ✓ |
| `SAM-A/` | 신규 `SAMA`(BRAND 미구현, 문자열 규칙 미정) | – | 밝은색 추정(배경 소스 = MOOHAN 판, 로고만 고유) | ✗ 로고만 |
| `SONOSYS/` | 신규 `SONOSYS`(BRAND 미구현, 문자열 규칙 미정) | – | **밝은색** (173/178/214/197) | ✓ (MOOHAN 복제 + 로고) |

불일치: 펌웨어 5 BRAND 중 POWERTECH·MAKETECH 는 완성 세트 없음(로고 배경 소스만). 폴더 중 SAM-A·SONOSYS 는 BRAND 없음(신규). `48_icon_mh` 는 이름과 달리 MOOHAN 세트가 아니라 **기준 GDSONIC `DWIN_SET/48.icl` 의 소스**다.

## 7. 특이 세트

- `DWIN_SET_portrait`: 25장 BMP(모두 1152054 B = 800×480×3+54), 기준 배경의 무손실 변환본. 세로 화면 세트 아님(파일명만 portrait).
- `DWIN_SOURCE` = `DONGIN/DWIN_SOURCE`: 2022-01 판(페이지 0~17, 터치 p0 `0x5000` 키, `0x1004`/`0x1409` 등 폐기 VP). 현행과 무관.
- `gds_lcd_v20/`: 이름과 달리 2026-08-30 생성된 p25~28 배경 4장 = 저장소 세트 p25~28 소스. lcd-V2 의 어느 DWIN_SET 에도 미반영.
- 저장소 `hw/lcd/dgus/`: 기준 세트 + p25/27 + 0x140b~0x140f + 21~28 배경 + 아이콘 56~59 + 22_Config IP 초기값. `48.icl` 이 기준(`48_icon_mh` 계보, 275720 B)이 아니라 `48_icon_green` 계보(278924 B, 치수 다름) — README 의 "NN.jpg = 페이지 배경" 서술은 오류(1~55.jpg 는 아이콘 소스, 배경은 32.icl).
- `TFT/*_Conflict.tft`, `DONGIN/32_bg_dongin/*(from Home-pc).jpg`: Dropbox 동기화 충돌 복제본.

## 8. 사용자 확인 필요 사항

1. GDSONIC 패널에 실제 올라간 `48.icl` 은 기준 `DWIN_SET`(=48_icon_mh, 275720 B) 인가 저장소판(48_icon_green 계보 + 56~59, 278924 B) 인가? 두 라이브러리는 아이콘 치수가 달라 같은 14ShowFile 에서 다르게 그려진다.
2. 저장소 세트의 아이콘 56~59(comm serial/eth, DHCP)와 배경 23/24 원본 이미지가 lcd-V2 에 없다 — 디자이너 측 보관 여부.
3. 기준 `DWIN_SET` 은 TCP 페이지(25/27)·VP 0x140b~0x140f 가 없다 — "현재 펌웨어에 맞는 세트"라면 저장소판과의 관계(저장소판이 기준의 후속인지, 기준이 되돌아간 것인지).
4. MOOHAN/SONOSYS p3/p9 에 `LV_TIME` 바와 `SETUP_PARAM 0x1100` 키가 없는 것이 의도(밝은색 디자인 사양)인지 누락인지.
5. p1 `MODEL_FREQ/TYPE` '+' 키가 MOOHAN 에 없는 것(−키만으로 순환?)이 의도인지.
6. 텍스트 위젯 encode 바이트(0x02/0x22/0x42/0x82/0xa2) 의 의미 — DGUS 툴에서 확인 가능하면 05-31 "page-show 자동 로드" 수정의 실체를 이 바이트로 특정할 수 있다.
7. `DISP_VERSION 0x1330` 표시 폭 16자 vs 펌웨어 20 B 전송 — 허용 범위인지.
8. SAMA/SONOSYS 모델 문자열 규칙(미정), SAM-A 세트 제작 여부.
9. `32_bg_gdsonicg/powertech/mktech` 안의 bin 사본(14Show #11) 이 어느 시점 판인지(어느 세트와도 불일치).
