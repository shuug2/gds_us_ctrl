/* fw/include/define.h — 빌드 시 고르는 모델 브랜드 + 제품 모델 + 버전 문자열.
 *
 * samd20은 브랜드 매크로를 main.c:27-30에, VERSION_MSG를 define.h:31에 두었다.
 * 포트에서는 둘을 이 파일 하나로 모은다. 소비처:
 *   브랜드      -> app_lcd_send_model_str()  (app_lcd.c, VP MODEL_NAME 0x1000, 11B)
 *   VERSION_MSG -> app_lcd_change_page()     (app_lcd_render.c, VP DISP_VERSION 0x1330, 20B)
 */
#ifndef DEFINE_H
#define DEFINE_H

/*--- 모델 브랜드 — 정확히 하나만 활성화 (samd20 main.c:27-30) ---
 * 접두어뿐 아니라 freq/type -> 문자 매핑 자체가 브랜드마다 다르다.
 *   GDSONIC   "GDS-20H   "   DIAMT   "DIS-20H   "
 *   POWERTECH "PTW-2020DH"   MOOHAN  "MH-1520DH "
 *   MAKETECH  "SMT-H15D  "   (samd20에 없던 신규 — type이 숫자 앞에 온다)
 */
#define GDSONIC
//#define DIAMT     
//#define POWERTECH 
//#define MOOHAN    
//#define MAKETECH  

#if (defined(GDSONIC) + defined(DIAMT) + defined(POWERTECH) + defined(MOOHAN) \
   + defined(MAKETECH)) != 1
#error "define.h: 모델 브랜드는 정확히 하나만 정의할 것"
#endif

/*--- 제품 모델(기능 티어) — cmake -DMODEL=STD|REMOTE 가 고른다 (CMakeLists.txt:35) ---
 * 브랜드와 직교하는 축이다. H/W는 hw-revA로 동일하고 F/W 기능셋으로만 갈린다.
 *   STD      기존 기능셋 — legacy 디바이스와 동일 (기본값)
 *   REMOTE   + 원격기 연동 (활성화 게이트·확장 레지스터)
 *
 * 🔴 기능 분기 #if defined(MODEL_REMOTE)는 글루 호출부 한 곳에만 둘 것.
 *    순수 모듈(FSM·계산)은 항상 컴파일한다 — 그래야 fw/test host 스위트가
 *    모델과 무관하게 전부 돌고, 테스트 매트릭스가 2배로 늘지 않는다.
 */
#if (defined(MODEL_STD) + defined(MODEL_REMOTE)) != 1
#error "define.h: 제품 모델은 정확히 하나만 정의할 것 (cmake -DMODEL=STD|REMOTE)"
#endif

/*--- PC8 원격 인터록 극성 (REMOTE 전용) ---
 * 1 = 반전 — 인터록 회로 미실장 구간의 한시 조치. 풀업만 걸린 HIGH를 "허용"으로 읽는다.
 *     🔴 이 상태에서는 fail-safe가 반대다 — 단선·커넥터 탈락이 불허가 아니라 허용이 된다.
 * 0 = 원안 active-LOW fail-safe — 닫힘=LOW=허용 / 열림·단선·탈락=HIGH=불허.
 *
 * 실장 PCB가 오면 0으로 되돌리고 A-1(OFF=불허)·A-5(ON=허용)·A-13(단선=불허) 재시험.
 * 판정은 app_modbus.c의 remote_en_step() 한 곳에서만 갈린다 — 하류 FSM·강제 로직 무관.
 * 아래 버전 문자열의 ! 표식이 이 값을 따라가므로, 원복을 잊어도 LCD가 어긋날 수 없다. */
#define REMOTE_EN_INTERLOCK_INVERTED 1

/*--- 버전 문자열 ---
 * DISP_VERSION VP에 정확히 20바이트 raw 송신 (NUL 미송신) — 공백 패딩 포함
 * 20자를 지킬 것. 길이는 app_lcd_render.c의 _Static_assert가 강제한다.
 *
 * 표기 규칙 — **번호 = 기능 티어, 날짜 = 그 빌드**. 같은 번호에 다른 날짜는 정상이다.
 *   3.0.x  STD     레거시-동등 기능셋. 번호를 동결한다(출하 기준점 표기, 사용자 결정).
 *   3.1.x  REMOTE  원격기 연동 티어.
 * 접미어 R = REMOTE 모델, ! = 인터록 극성 반전판(위 매크로). 현장에서 LCD로 구분한다.
 *
 * ⚠ Modbus 소비자의 capability 판별은 이 문자열이 아니다 — 버전 레지스터는 없고
 *   0x31 CFG_CAP / 0x2A REMOTE_CAP이 그 일을 한다(app_modbus_core.h의 판정표).
 *   그래서 STD 번호를 동결해도 소비자를 오도하지 않는다.
 *
 * 이력:
 *   V2.9.1_250629    samd20 마지막 (define.h:31) — fix ethernet comm, fix error msg clear
 *   V3.0.0_260725    STM32F410 통합 포트 = 레거시-동등 기준선 (tag hw-revA_fw-3.0.0)
 *   V3.0.0_260905    STD — 번호 동결, 날짜만 진행. IWDG · Modbus 무음 손상 fix 2건 ·
 *                    F-A 레지스터 확장(0x1E~0x29 · 0x2E~0x31) 포함
 *   V3.1.0R_260905   REMOTE 티어 첫 릴리즈 (tag hw-revA_fw-3.1.0) — 지금은 ! 판
 */
#if defined(MODEL_REMOTE)
#  if REMOTE_EN_INTERLOCK_INVERTED
#define VERSION_MSG "V3.1.0R!_260905     "
#  else
#define VERSION_MSG "V3.1.0R_260905      "
#  endif
#else
#define VERSION_MSG "V3.0.0_260905       "
#endif

#endif /* DEFINE_H */
