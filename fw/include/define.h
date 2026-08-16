/* fw/include/define.h — 빌드 시 고르는 모델 브랜드 + 버전 문자열.
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

/*--- 버전 문자열 ---
 * DISP_VERSION VP에 정확히 20바이트 raw 송신 (NUL 미송신) — 공백 패딩 포함
 * 20자를 지킬 것. 길이는 app_lcd_render.c의 _Static_assert가 강제한다.
 * 이력:
 *   V2.9.1_250629   samd20 마지막 (define.h:31) — fix ethernet comm, fix error msg clear
 *   V3.0.0_260725   STM32F410 통합 포트
 */
#define VERSION_MSG "V3.0.0_260725       "

#endif /* DEFINE_H */
