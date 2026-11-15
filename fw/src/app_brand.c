/* fw/src/app_brand.c — 업체별 LCD 모델명 규칙 (구 app_lcd_send_model_str 의 #if 블록 본문 그대로 이동).
 * samd20 main.c:2379-2586, 브랜드당 1블록. samd20은 freq/type이 범위를
 * 벗어나면 버퍼를 미초기화로 남겼다 — 포트는 default로 첫 케이스를 쓴다. */
#include "app_brand.h"

/* 번호 → 브랜드 */
brand_t brand_from_id(uint16_t id)
{
    return (id >= BRAND_GDSONIC && id <= BRAND_MAKETECH) ? (brand_t)id : BRAND_GDSONIC;
}

/* GDSONIC / DIAMT — samd20 main.c:2379-2426 (GDS) / 2428-2475 (DIS) — 접두 3바이트만 다르다. */
static void model_str_gds_dis(char p0, char p1, char p2, uint8_t freq, uint8_t type, uint8_t s[BRAND_MODEL_STR_LEN])
{
    s[0] = (uint8_t)p0; s[1] = (uint8_t)p1; s[2] = (uint8_t)p2;
    s[3] = '-';
    switch (freq) {
        case 0:  s[4] = '1'; s[5] = '5'; break;
        case 1:  s[4] = '2'; s[5] = '0'; break;
        case 2:  s[4] = '3'; s[5] = '0'; break;
        case 3:  s[4] = '3'; s[5] = '5'; break;
        case 4:  s[4] = '4'; s[5] = '0'; break;
        case 5:  s[4] = '5'; s[5] = '0'; break;
        default: s[4] = '1'; s[5] = '5'; break;
    }
    switch (type) {
        case 0:  s[6] = 'H'; break;
        case 1:  s[6] = 'M'; break;
        case 2:  s[6] = 'S'; break;
        default: s[6] = 'H'; break;
    }
    s[7] = ' '; s[8] = ' '; s[9] = ' '; s[10] = '\0';
}

/* GDSONIC */
static void model_str_gdsonic(uint8_t freq, uint8_t type, uint8_t s[BRAND_MODEL_STR_LEN])
{
    model_str_gds_dis('G', 'D', 'S', freq, type, s);
}

/* DIAMT */
static void model_str_diamt(uint8_t freq, uint8_t type, uint8_t s[BRAND_MODEL_STR_LEN])
{
    model_str_gds_dis('D', 'I', 'S', freq, type, s);
}

/* POWERTECH — samd20 main.c:2477-2530. 30K/35K 모두 " 735" (legacy 그대로). */
static void model_str_powertech(uint8_t freq, uint8_t type, uint8_t s[BRAND_MODEL_STR_LEN])
{
    s[0] = 'P'; s[1] = 'T'; s[2] = 'W'; s[3] = '-';
    switch (freq) {
        case 0:  s[4] = '2'; s[5] = '5'; s[6] = '1'; s[7] = '5'; break;   /* 15K */
        case 1:  s[4] = '2'; s[5] = '0'; s[6] = '2'; s[7] = '0'; break;   /* 20K */
        case 2:  s[4] = ' '; s[5] = '7'; s[6] = '3'; s[7] = '5'; break;   /* 30K */
        case 3:  s[4] = ' '; s[5] = '7'; s[6] = '3'; s[7] = '5'; break;   /* 35K */
        case 4:  s[4] = ' '; s[5] = '7'; s[6] = '4'; s[7] = '0'; break;   /* 40K */
        case 5:  s[4] = ' '; s[5] = '7'; s[6] = '5'; s[7] = '0'; break;   /* 50K */
        default: s[4] = '2'; s[5] = '5'; s[6] = '1'; s[7] = '5'; break;
    }
    switch (type) {
        case 0:  s[8] = 'D'; s[9] = 'H'; break;
        case 1:  s[8] = 'M'; s[9] = 'D'; break;
        case 2:  s[8] = 'S'; s[9] = 'D'; break;
        default: s[8] = 'D'; s[9] = 'H'; break;
    }
    s[10] = '\0';
}

/* MAKETECH — samd20에 대응 블록 없는 신규 브랜드. "SMT-" + type문자 + freq 2자리 + 'D'.
 *   hand "SMT-H15D" / multi "SMT-A15D" / std "SMT-S15D"
 * 패널 선택지는 15K/20K/35K. 나머지 freq 코드는 GDSONIC과 같이 실제 주파수를
 * 그대로 찍는다 (MOOHAN처럼 30K를 35로 접지 않음 — 접을 근거가 legacy에 없음). */
static void model_str_maketech(uint8_t freq, uint8_t type, uint8_t s[BRAND_MODEL_STR_LEN])
{
    s[0] = 'S'; s[1] = 'M'; s[2] = 'T'; s[3] = '-';
    switch (type) {
        case 0:  s[4] = 'H'; break;                                       /* hand */
        case 1:  s[4] = 'A'; break;                                       /* multi */
        case 2:  s[4] = 'S'; break;                                       /* standard */
        default: s[4] = 'H'; break;
    }
    switch (freq) {
        case 0:  s[5] = '1'; s[6] = '5'; break;
        case 1:  s[5] = '2'; s[6] = '0'; break;
        case 2:  s[5] = '3'; s[6] = '0'; break;
        case 3:  s[5] = '3'; s[6] = '5'; break;
        case 4:  s[5] = '4'; s[6] = '0'; break;
        case 5:  s[5] = '5'; s[6] = '0'; break;
        default: s[5] = '1'; s[6] = '5'; break;
    }
    s[7] = 'D'; s[8] = ' '; s[9] = ' '; s[10] = '\0';
}

/* MOOHAN — samd20 main.c:2532-2585. 30K/35K 모두 "1535" (legacy 그대로). */
static void model_str_moohan(uint8_t freq, uint8_t type, uint8_t s[BRAND_MODEL_STR_LEN])
{
    s[0] = 'M'; s[1] = 'H'; s[2] = '-';
    switch (freq) {
        case 0:  s[3] = '1'; s[4] = '5'; s[5] = '1'; s[6] = '5'; break;   /* 15K */
        case 1:  s[3] = '1'; s[4] = '5'; s[5] = '2'; s[6] = '0'; break;   /* 20K */
        case 2:  s[3] = '1'; s[4] = '5'; s[5] = '3'; s[6] = '5'; break;   /* 30K */
        case 3:  s[3] = '1'; s[4] = '5'; s[5] = '3'; s[6] = '5'; break;   /* 35K */
        case 4:  s[3] = '1'; s[4] = '5'; s[5] = '4'; s[6] = '0'; break;   /* 40K */
        case 5:  s[3] = '1'; s[4] = '5'; s[5] = '5'; s[6] = '0'; break;   /* 50K */
        default: s[3] = '1'; s[4] = '5'; s[5] = '1'; s[6] = '5'; break;
    }
    switch (type) {
        case 0:  s[7] = 'D'; s[8] = 'H'; break;
        case 1:  s[7] = 'D'; s[8] = 'M'; break;
        case 2:  s[7] = 'D'; s[8] = 'S'; break;
        default: s[7] = 'D'; s[8] = 'H'; break;
    }
    s[9] = ' '; s[10] = '\0';
}

/* 브랜드 → 규칙 디스패치 */
void brand_model_str(brand_t b, uint8_t freq, uint8_t type, uint8_t out[BRAND_MODEL_STR_LEN])
{
    switch (b) {
        case BRAND_MOOHAN:    model_str_moohan(freq, type, out);    break;
        case BRAND_DIAMT:     model_str_diamt(freq, type, out);     break;
        case BRAND_POWERTECH: model_str_powertech(freq, type, out); break;
        case BRAND_MAKETECH:  model_str_maketech(freq, type, out);  break;
        case BRAND_GDSONIC:
        default:              model_str_gdsonic(freq, type, out);   break;
    }
}
