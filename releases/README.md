# releases/ — 양산 바이너리 SWD 플래시 가이드

> **문서 요약**: `releases/<버전>/` 보관본을 **ST-LINK(SWD) 하나로** STM32F410RBT 보드에 올리는 절차다. 도구는 둘 중 하나 — **① STM32CubeProgrammer CLI**(빌드 환경 없는 PC) 또는 **② OpenOCD**(개발 PC, `fw.sh` 와 같은 경로). 부트로더가 없는 단일 이미지(`0x08000000`)라 파일은 `.bin` 하나뿐이다. 플래시 전 `SHA256SUMS` 대조, 플래시 후 LCD 버전 문자열이 폴더명과 같은지 확인이 판정 기준이다. 원격기 `gds_us_remote/releases/` 와 같은 구조(2026-09-13 두 저장소 공통 결정).

## 1. 폴더 규칙

```
releases/
├── README.md                              ← 이 문서 (버전 무관)
└── V3.1.0R_260911/                        ← 버전 문자열 = 폴더명 (define.h VERSION_MSG, 공백·! 제거)
    ├── gds_us_ctrl_<brand>_v3.1.0R_260911.bin  ← REMOTE 앱. fw/build-v30-<brand>-remote/gds_us_ctrl.bin 을 이름만 바꿔 복사 (내용 동일)
    ├── gds_us_ctrl_<brand>_hw31_v3.1.0R_260911.bin ← 같은 앱의 V31 보드용 (HW_REV=v31). V30 보드에 올리지 말 것
    ├── gds_us_ctrl_<brand>_v3.0.0_260911.bin   ← CTRL(옛 STD) 앱 (같이 검증됐을 때만)
    ├── gds_us_ctrl_<brand>_v3.1.0R_260911.elf  ← 심볼 (선택, SHA 대상 아님 — 빌드 경로가 박혀 재현 불가)
    ├── stm32f410.cfg                      ← fw/openocd/stm32f410.cfg 복사본 (어댑터 설정)
    ├── SHA256SUMS                         ← .bin + cfg
    └── README.md                          ← 그 버전의 내용·재현 방법·배포 가능 여부
```

- 폴더명은 **git 태그가 아니라 버전 문자열**이다 — ctrl 의 태그는 `hw-revA_fw-…` 형식이라 버전이 안 담긴다(루트 `CLAUDE.md` 태깅 규칙). 태그↔커밋은 각 버전 README 에 적는다.
- 파일명에 **브랜드(업체, 소문자)** 를 넣는다(2026-10-05~) — 펌웨어는 브랜드별로 LCD 모델명만 다르고 버전 문자열은 같아서, 한 버전 폴더에 브랜드별 `.bin` 이 나란히 놓인다(`BRAND=moohan MODEL=remote ./fw.sh`). 그 전 보관본 `V3.1.0R_260911/` 은 브랜드 없는 이름이며 GDSONIC 이다.
- **V31 보드용은 브랜드 뒤에 `_hw31`**(2026-10-27~). 부저·mon 핀이 달라 V30 보드에 올리면 mon 출력이 부저로 나간다. LCD 버전 문자열은 리비전과 무관하게 같으므로 **구분은 파일명·SHA 뿐**이다. V30 은 표식 없는 지금 이름.
- 파일명에 **`!` 를 넣지 않는다.** 인터록 반전판(= 물리 인터록 없음) 여부·배포 가능 여부는 각 버전 README 의 배포 항목이 정본이다. LCD 에는 `!` 가 그대로 보인다(§5).

🔴 **플래시는 반드시 이 폴더의 보관본으로.** 작업 트리 `fw/build*/` 는 태그 뒤 커밋·수정된 `define.h` 날짜가 섞여 들어가도 파일명이 같아 구분이 안 된다.
🔴 **현장 배포 가능 여부는 각 버전 README 를 본다** — PC8 인터록 극성 반전이 최종 상태로 확정돼 배포 금지는 해제됐다(사용자 결정 2026-11-15, 루트 `HANDOFF.md`). 버전별 벤치 미통과 등은 각 README 에 따로 적는다.

## 2. 하드웨어 연결

| 항목 | 값 |
|---|---|
| 어댑터 | ST-LINK/V2 · V2-1 · V3 (SWD). `fw/openocd/stm32f410.cfg` = `interface/stlink` + `hla_swd` |
| USB 장치 | VID `0483`, PID `3748`(V2) / `374b`(V2-1) / `374e` `374f` `3753`(V3) |
| 배선 | SWDIO · SWCLK · GND · NRST(선택, `srst_only`). 보드는 자체 전원 |
| MCU | STM32F410RBT — Flash 128 KB @ `0x08000000`, BOOT0=GND(앱 직부팅) |
| 설정 저장 | 외부 FRAM(I2C) — 플래시 소거와 무관, **설정은 보존된다** |

어댑터 찾기:

```bash
# macOS
system_profiler SPUSBDataType | grep -A3 -i st-link
# Linux
lsusb | grep 0483
# Windows: 장치 관리자 → 범용 직렬 버스 장치 → "ST-Link Debug"
```

안 보이면: 케이블 교체 → 허브 말고 직결 → 다른 openocd/gdb 서버(`./fw.sh gdb`)가 어댑터를 잡고 있지 않은지 확인.

## 3. 무결성 확인 (플래시 전)

```bash
cd releases/V3.1.0R_260911
shasum -a 256 -c SHA256SUMS        # 전 줄 OK
```

한 줄이라도 `FAILED` 면 그 폴더는 쓰지 않는다.

## 4-A. STM32CubeProgrammer CLI 로 플래시 (빌드 환경 없는 PC)

설치 (1회): ST 사이트에서 STM32CubeProgrammer. CLI = `STM32_Programmer_CLI`(macOS 는 앱 번들 안 `.../STM32CubeProgrammer.app/Contents/MacOs/bin/`).

```bash
cd releases/V3.1.0R_260911
STM32_Programmer_CLI -c port=SWD mode=UR reset=HWrst \
  -w gds_us_ctrl_v3.1.0R_260911.bin 0x08000000 -v -rst
```

성공 판정: `File download complete` → `Download verified successfully` → 리셋.

## 4-B. OpenOCD 로 플래시 (개발 PC)

```bash
cd releases/V3.1.0R_260911
openocd -f stm32f410.cfg \
  -c "program gds_us_ctrl_v3.1.0R_260911.bin 0x08000000 verify reset exit"
```

성공 판정: `** Programming Finished **` → `** Verified OK **` → `** Resetting Target **`. `verify` 를 빼지 않는다. `.bin` 이라 주소 `0x08000000` 을 반드시 적는다(`.elf` 와 달리 주소가 안 박혀 있다).

## 4-C. J-Link (있을 때만)

```bash
JLinkExe -device STM32F410RB -if SWD -speed 4000
J-Link> loadbin gds_us_ctrl_v3.1.0R_260911.bin 0x08000000
J-Link> r
J-Link> g
J-Link> q
```

## 4-D. 생산라인 = SEGGER Flasher Portable PLUS

양산 플래시는 **`production/`**(루트) 의 업체별 슬롯 묶음으로 한다 — 절차·기준표 = `production/README.md`. 이 폴더의 보관본이 그 입력이다(`production/models.txt`).

## 4-E. UART 부트로더 (V31 보드만, 앱이 살아 있을 때)

V31 보드는 모니터 헤더 **J3**(1 = 3.3 V 레일 — **연결 금지**, 2 = RX, 3 = TX, 4 = GND)로 ST-LINK 없이 굽는다. 파일은 **`_hw31` 이 붙은 것만**. 3.3 V TTL USB-UART 어댑터의 TX→J3.2, RX→J3.3, GND→J3.4.

1. 매직 송신 (mon 설정 115200 8N1) — 앱이 표식을 남기고 리셋 → ROM 부트로더(USART2, **8E1**, 자동 보율)
   ```bash
   stty -f /dev/cu.usbserial-XXXX 115200 cs8 -parenb -cstopb && printf '!!GDSUS_BOOT!!' > /dev/cu.usbserial-XXXX   # macOS
   ```
   ```powershell
   $p = New-Object System.IO.Ports.SerialPort COM5,115200,None,8,One; $p.Open(); $p.Write('!!GDSUS_BOOT!!'); $p.Close()   # Windows
   ```
   mon 에 `[boot] magic -> ROM bootloader` 한 줄이 나오고 이후 침묵하면 진입한 것이다.
2. 굽기 (둘 중 하나)
   ```bash
   STM32_Programmer_CLI -c port=COM5 br=115200 p=even -w gds_us_ctrl_<brand>_hw31_v<버전>.bin 0x08000000 -v
   stm32flash -b 115200 -w gds_us_ctrl_<brand>_hw31_v<버전>.bin -v /dev/cu.usbserial-XXXX
   ```
3. 판정: **전원 재투입** → §5 LCD 버전. (부트로더에서 그냥 리셋해도 표식은 이미 지워져 정상 부팅한다)

- 다운로드 중 LCD 터치 금지 — 부트로더가 USART1(LCD 선)도 듣는다.
- 앱이 죽어 매직을 못 받는 보드는 §4-A/B(SWD). 설정(FRAM)은 보존.
- 근거·설계 = `docs/superpowers/specs/2026-09-20-uart-mon-port-design.md` §0·§6. 🔴 **벤치(V-2~V-10) 전 — 실보드 미검증 절차다.**

## 5. 플래시 후 확인

보드 재부팅 → LCD 첫 화면 버전 문자열(VP `DISP_VERSION`) 을 본다:

```text
V3.1.0R!_260911      ← REMOTE 반전판 (! 는 REMOTE_EN_INTERLOCK_INVERTED=1 표식)
V3.1.0R_260911       ← REMOTE 정상판
V3.0.0_260911        ← CTRL (옛 STD)
```

- `!` 를 뺀 나머지가 폴더명과 다르면 다른 이미지가 올라간 것이다.
- `!` 가 보이는데 버전 README 가 정상판이라 하면 보관본이 잘못 만들어진 것이다 — 그 폴더 안 씀.
- 부팅 로그: V30 = mon USART6(RS-485 DE 미제어로 캡처 불안정) / V31 = 헤더 J3 115200 8N1 `[boot] …`. 판정 정본은 LCD 육안.

⚠️ **런타임 검증에 SWD halt 금지** — sys_tick 이 멈춰 오진한다(루트 `CLAUDE.md`). 플래시 후엔 어댑터를 놓고 LCD·Modbus 로 본다.

## 6. 실패 대응

| 증상 | 원인 | 조치 |
|---|---|---|
| `Error: open failed` / `no device found` | 어댑터 미인식·케이블·다른 프로세스 점유 | §2 절차. `pkill openocd` 후 재시도 |
| `Error: init mode failed (unable to connect to the target)` | 보드 전원 없음·SWD 배선·NRST 미연결 | 전원 확인 → SWDIO/SWCLK/GND 재확인 → cfg 의 `reset_config` 그대로 |
| `LIBUSB_ERROR_ACCESS` (Linux) | udev 권한 | ST-LINK udev 규칙 설치 또는 `sudo` |
| `Verified OK` 인데 LCD 버전이 다름 | 다른 폴더·작업 트리 빌드를 올림 | §3 부터 다시. 파일명이 아니라 SHA 를 믿는다 |
| `Verified OK` 인데 LCD 무표시 | LCD 자산(DGUS) 구버전 또는 USART1 배선 | 펌웨어 문제 아님 — `hw/lcd/lcd_V30/<업체>/DWIN_SET/` 자산 대조(2026-09-13 사례) |
| 플래시 후 설정이 초기값 | FRAM 문제 (플래시와 무관) | 펌웨어 재플래시로는 안 바뀜 — FRAM/I2C 점검 |
| 부팅 직후 멈춤 + ST-LINK 붙어 있음 | halt 잔재 | 어댑터 분리 → 전원 재투입 |

## 7. 새 릴리스를 여기에 넣을 때

루트 `CLAUDE.md` 「양산 바이너리 보관」 절의 ①~⑧ 을 명령으로 옮긴 것이다.

```bash
git checkout <hw-revA_fw-… 태그>
git status --porcelain                        # 비어 있어야 함 — 수정 트리 빌드 금지
grep VERSION_MSG fw/include/define.h          # 날짜가 태그 시점과 같은지 사람이 확인 (git describe 안 씀)
V=V3.1.0R_260911                              # VERSION_MSG 에서 공백·! 제거
mkdir -p releases/$V
# 리비전·브랜드마다 반복. 빌드 폴더 = fw/build-<hw>-<brand>-<model>/ (2026-10-27 이전 태그는 옛 이름 build[-<brand>]-remote/)
H=v30; B=gdsonic                              # H=v31 이면 파일명에 _hw31
D=fw/build-$H-$B-remote; rm -rf $D            # 클린 빌드
HW_REV=$H BRAND=$B MODEL=remote ./fw.sh
T=; [ $H = v31 ] && T=_hw31
cp $D/gds_us_ctrl.bin releases/$V/gds_us_ctrl_${B}${T}_v${V#V}.bin
cp $D/gds_us_ctrl.elf releases/$V/gds_us_ctrl_${B}${T}_v${V#V}.elf   # 선택
# CTRL(옛 STD) 도 검증됐을 때만: MODEL=ctrl 로 같은 절차, 파일명 버전 = v3.0.0_<날짜>
cp fw/openocd/stm32f410.cfg        releases/$V/
cd releases/$V
shasum -a 256 *.bin stm32f410.cfg > SHA256SUMS
```

그다음:

1. **그 보관본을 §4 로 보드에 플래시** → §5 LCD 버전 육안 (build/ 가 아니라 보관본으로)
2. 버전 README.md 작성 (아래 템플릿)
3. 보관 폴더 커밋 — 태그보다 뒤 커밋이 정상 — 푸시
4. 원격기(`gds_us_remote`) 통보 — 계약 변경이 있을 때만

빌드는 재현 가능하다(`__DATE__` 미사용, 같은 툴체인이면 같은 `.bin` — `fw/tools/bin-same.sh` 가 그 전제로 동작). SHA 가 다르면 툴체인 버전부터 의심.

### 버전 README.md 템플릿

```markdown
# V3.1.0R_260911

> **문서 요약**: 태그 `hw-revA_fw-…` 의 빌드 산출물 보관본. `gds_us_ctrl_v3.1.0R_260911.bin` 은
> `build-v30-gdsonic-remote/gds_us_ctrl.bin` 을 이름만 바꾼 것. 재빌드 없이 플래시하고, 재현 빌드의 대조 기준이다.

| 항목 | 값 |
|---|---|
| 태그 / 커밋 / 브랜치 | `hw-revA_fw-…` = `abc1234` (`main`) |
| 툴체인 | arm-none-eabi-gcc 15.2.1 (Arm GNU Toolchain 15.2.Rel1) · cmake 4.3.1 · openocd 0.12.0 |
| 빌드 | `rm -rf fw/build-v30-gdsonic-remote && MODEL=remote ./fw.sh` — `git status` 깨끗한 태그 트리 |
| 대상 보드 | V30 (파일명 `_hw31` 이면 V31) · STM32F410RBT (128 KB) · LCD 자산 `hw/lcd/lcd_V30/<업체>/DWIN_SET/` 버전 |

## 이 폴더에서 바로 플래시
openocd -f stm32f410.cfg -c "program gds_us_ctrl_v3.1.0R_260911.bin 0x08000000 verify reset exit"

## 재현 확인
git checkout <태그> → 클린 빌드 → shasum -a 256 -c SHA256SUMS

## 이 릴리스에 든 것 (직전 보관본 이후)
…  상세는 docs/changelog.md / HANDOFF.md

## 직전 버전과의 관계
부트로더 없음 — 단일 앱 이미지라 항상 전체 교체. FRAM 설정은 보존. (cfg 레이아웃이 바뀌었으면 여기 명기)

## 🔴 배포 가능 여부
인터록 반전판(`REMOTE_EN_INTERLOCK_INVERTED 1`, LCD `!`) = 물리 인터록 없음(최종 상태). 배포 가능 — 수용한 위험 = HANDOFF.md. (벤치 미통과 항목이 있으면 여기 명기)
```
