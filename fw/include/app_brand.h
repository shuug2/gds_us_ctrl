/* fw/include/app_brand.h — 업체(브랜드)별 LCD 모델명 규칙. 순수(HAL 없음), host 테스트 대상.
 * 번호표 정본 = 이 enum (lcdset brands.json 과 같은 숫자). 0·모르는 번호 = GDSONIC. */
#pragma once
#include <stdint.h>

typedef enum { BRAND_GDSONIC = 1, BRAND_MOOHAN = 2, BRAND_DIAMT = 3,
               BRAND_POWERTECH = 4, BRAND_MAKETECH = 5 } brand_t;

#define BRAND_VP             0x1500u   /* LCD 세트가 브랜드 번호를 담는 VP (V31 런타임 판별) */
#define BRAND_MODEL_STR_LEN  11u       /* MODEL_NAME 페이로드, [10] = NUL */

/* 1..5 → 그 값, 그 외(0 포함) → BRAND_GDSONIC */
brand_t brand_from_id(uint16_t id);

/* 브랜드·freq(0..5)·type(0..2) → 모델명 11 B. 범위 밖 freq/type = 첫 케이스,
 * 범위 밖 brand = GDSONIC 규칙. */
void brand_model_str(brand_t b, uint8_t freq, uint8_t type, uint8_t out[BRAND_MODEL_STR_LEN]);
