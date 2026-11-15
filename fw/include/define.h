/* fw/include/define.h — 빌드 시 고르는 모델 브랜드 + 제품 모델 + 버전 문자열.
 *
 * samd20은 브랜드 매크로를 main.c:27-30에, VERSION_MSG를 define.h:31에 두었다.
 * 포트에서는 둘을 이 파일 하나로 모은다. 소비처:
 *   브랜드      -> app_lcd_send_model_str()  (app_lcd.c, VP MODEL_NAME 0x1000, 11B)
 *   VERSION_MSG -> app_lcd_change_page()     (app_lcd_render.c, VP DISP_VERSION 0x1330, 20B)
 */
#ifndef DEFINE_H
#define DEFINE_H

/*--- 보드 리비전 — cmake -DHW_REV=V30|V31 이 고른다 (CMakeLists.txt:45, HW_REV=v31 ./fw.sh) ---
 *   V30  기본. 부저 PA2, mon = USART6(PC6/PC7, RS-485 와 공유)
 *   V31  부저 PB9, mon = USART2(PA2 TX/PA3 RX, 헤더 J3) + UART 부트로더 점프
 * 🔴 V31 이미지를 V30 보드에 올리면 mon 출력이 부저 라인(PA2)으로 나간다 — 반대면 부저 무음.
 *    핀 분기는 io.c(부저)·mon 드라이버·main.c 배선에만 둔다. */
#if (defined(HW_V30) + defined(HW_V31)) != 1
#error "define.h: 보드 리비전은 정확히 하나만 정의할 것 (cmake -DHW_REV=V30|V31)"
#endif

/*--- 모델 브랜드(업체) ---
 * V30: cmake -DBRAND=<이름> 이 고른다 (CMakeLists.txt, BRAND=moohan ./fw.sh) — 정확히 하나.
 * V31: 브랜드 매크로 없음 — 런타임에 LCD VP 0x1500(BRAND_VP)의 브랜드 번호로 판별한다
 *      (app_lcd_brand_refresh). fw.sh 가 -DBRAND=NONE 을 넘기고 CMake 는 매크로를 안 만든다.
 * 브랜드 번호표 정본 = app_brand.h brand_t (1 GDSONIC · 2 MOOHAN · 3 DIAMT · 4 POWERTECH · 5 MAKETECH,
 *   그 외 → GDSONIC). 모델명 규칙도 app_brand. 설계 = docs/superpowers/specs/2026-11-15-lcd-brand-id-design.md
 * 업체 추가 = app_brand + 아래 #if 합에 한 항 + app_lcd.c BRAND_BUILD 에 #elif + hw/lcd/lcd_V30/README.md 표 한 줄.
 */
#if defined(HW_V31)
#if (defined(GDSONIC) + defined(DIAMT) + defined(POWERTECH) + defined(MOOHAN) \
   + defined(MAKETECH)) != 0
#error "V31 은 런타임 브랜드 — BRAND 지정 금지"
#endif
#elif (defined(GDSONIC) + defined(DIAMT) + defined(POWERTECH) + defined(MOOHAN) \
     + defined(MAKETECH)) != 1
#error "define.h: 모델 브랜드는 정확히 하나만 정의할 것 (cmake -DBRAND=GDSONIC|DIAMT|POWERTECH|MOOHAN|MAKETECH)"
#endif

/*--- 제품 모델(기능 티어) — cmake -DMODEL=CTRL|REMOTE 가 고른다 (CMakeLists.txt:40) ---
 * 브랜드와 직교하는 축이다. H/W는 hw-revA로 동일하고 F/W 기능셋으로만 갈린다.
 *   CTRL     기존 기능셋 — legacy 디바이스와 동일 (기본값. 2026-10-27 전 이름 STD)
 *   REMOTE   + 원격기 연동 (활성화 게이트·확장 레지스터)
 *   ⚠ 장비 타입 model_type 의 std(0=hand 1=multi 2=std, LCD STD 페이지)와는 별개다 — 그쪽은 그대로.
 *
 * 🔴 기능 분기 #if defined(MODEL_REMOTE)는 글루 호출부 한 곳에만 둘 것.
 *    순수 모듈(FSM·계산)은 항상 컴파일한다 — 그래야 fw/test host 스위트가
 *    모델과 무관하게 전부 돌고, 테스트 매트릭스가 2배로 늘지 않는다.
 */
#if (defined(MODEL_CTRL) + defined(MODEL_REMOTE)) != 1
#error "define.h: 제품 모델은 정확히 하나만 정의할 것 (cmake -DMODEL=CTRL|REMOTE)"
#endif

/*--- PC8 원격 인터록 극성 (REMOTE 전용) ---
 * 1 = 반전 — **최종 상태** (사용자 결정 2026-11-15: PC8 인터록 실장 PCB 는 만들지 않는다).
 *     풀업만 걸린 HIGH를 "허용"으로 읽는다.
 *     🔴 이 상태에서는 fail-safe가 반대다 — 단선·커넥터 탈락이 불허가 아니라 허용이 된다.
 * 0 = 원안 active-LOW fail-safe — 닫힘=LOW=허용 / 열림·단선·탈락=HIGH=불허. (폐기된 원안)
 *
 * 위험(물리 인터록 없음, 원격 START 탭 한 번)은 사용자가 수용하고 배포를 허용했다.
 * 판정은 app_modbus.c의 remote_en_step() 한 곳에서만 갈린다 — 하류 FSM·강제 로직 무관.
 * 아래 버전 문자열의 ! 표식(= 물리 인터록 없는 판)이 이 값을 따라간다. */
#define REMOTE_EN_INTERLOCK_INVERTED 1

/*--- 버전 문자열 ---
 * DISP_VERSION VP에 정확히 20바이트 raw 송신 (NUL 미송신) — 공백 패딩 포함
 * 20자를 지킬 것. 길이는 app_lcd_render.c의 _Static_assert가 강제한다.
 *
 * 표기 규칙 — **번호 = 기능 티어, 날짜 = 그 빌드**. 같은 번호에 다른 날짜는 정상이다.
 *   3.0.x  CTRL    레거시-동등 기능셋. 번호를 동결한다(출하 기준점 표기, 사용자 결정).
 *   3.1.x  REMOTE  원격기 연동 티어.
 * 접미어 R = REMOTE 모델, ! = 인터록 극성 반전판 = 물리 인터록 없음(위 매크로, 최종). 현장에서 LCD로 구분한다.
 *
 * ⚠ Modbus 소비자의 capability 판별은 이 문자열이 아니다 — 버전 레지스터는 없고
 *   0x31 CFG_CAP / 0x2A REMOTE_CAP이 그 일을 한다(app_modbus_core.h의 판정표).
 *   그래서 CTRL 번호를 동결해도 소비자를 오도하지 않는다.
 *
 * 이력:
 *   V2.9.1_250629    samd20 마지막 (define.h:31) — fix ethernet comm, fix error msg clear
 *   V3.0.0_260725    STM32F410 통합 포트 = 레거시-동등 기준선 (tag hw-revA_fw-3.0.0)
 *   V3.0.0_260905    STD — 번호 동결, 날짜만 진행. IWDG · Modbus 무음 손상 fix 2건 ·
 *                    F-A 레지스터 확장(0x1E~0x29 · 0x2E~0x31) 포함
 *   V3.1.0R_260905   REMOTE 티어 첫 릴리즈 (tag hw-revA_fw-3.1.0) — 지금은 ! 판(11-15 최종 확정)
 */
#if defined(MODEL_REMOTE)
#  if REMOTE_EN_INTERLOCK_INVERTED
#define VERSION_MSG "V3.1.0R!_260911     "
#  else
#define VERSION_MSG "V3.1.0R_260911      "
#  endif
#else
#define VERSION_MSG "V3.0.0_260911       "
#endif

#endif /* DEFINE_H */
