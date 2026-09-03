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

/*--- 버전 문자열 ---
 * DISP_VERSION VP에 정확히 20바이트 raw 송신 (NUL 미송신) — 공백 패딩 포함
 * 20자를 지킬 것. 길이는 app_lcd_render.c의 _Static_assert가 강제한다.
 * 접미어 R = REMOTE 모델. 같은 버전의 두 바이너리를 현장에서 LCD로 구분한다.
 * 이력:
 *   V2.9.1_250629   samd20 마지막 (define.h:31) — fix ethernet comm, fix error msg clear
 *   V3.0.0_260725   STM32F410 통합 포트 (= tag hw-revA_fw-3.0.0, 기능 티어 분기 이전)
 */
#if defined(MODEL_REMOTE)
#define VERSION_MSG "V3.0.0R_260725      "
#else
#define VERSION_MSG "V3.0.0_260725       "
#endif

#endif /* DEFINE_H */
