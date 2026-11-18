/* fw/test/test_app_brand.c — host unit tests, 브랜드 모델명 규칙(app_brand).
 * 계약: 5 브랜드 × freq 0..5 × type 0..2 = 90 조합이 분리 전 app_lcd.c 의
 * 업체별 #if 블록 출력과 바이트 동일(11 B, [10] = NUL). */
#include <stdio.h>
#include <string.h>
#include "app_brand.h"

static int failures = 0;
#define CHECK_EQ(expr, expected) do {                                       \
    unsigned long a_ = (unsigned long)(expr);                              \
    unsigned long e_ = (unsigned long)(expected);                          \
    if (a_ != e_) {                                                         \
        printf("FAIL %s:%d  %s = %lu, expected %lu\n",                      \
               __FILE__, __LINE__, #expr, a_, e_);                          \
        failures++;                                                         \
    }                                                                       \
} while (0)

typedef struct { brand_t b; uint8_t freq; uint8_t type; const char *exp; } golden_t;

/* 분리 전 app_lcd_send_model_str 를 브랜드별로 host 컴파일해 뽑은 값. */
static const golden_t k_golden[] = {
    { BRAND_GDSONIC, 0, 0, "GDS-15H   " },
    { BRAND_GDSONIC, 0, 1, "GDS-15M   " },
    { BRAND_GDSONIC, 0, 2, "GDS-15S   " },
    { BRAND_GDSONIC, 1, 0, "GDS-20H   " },
    { BRAND_GDSONIC, 1, 1, "GDS-20M   " },
    { BRAND_GDSONIC, 1, 2, "GDS-20S   " },
    { BRAND_GDSONIC, 2, 0, "GDS-30H   " },
    { BRAND_GDSONIC, 2, 1, "GDS-30M   " },
    { BRAND_GDSONIC, 2, 2, "GDS-30S   " },
    { BRAND_GDSONIC, 3, 0, "GDS-35H   " },
    { BRAND_GDSONIC, 3, 1, "GDS-35M   " },
    { BRAND_GDSONIC, 3, 2, "GDS-35S   " },
    { BRAND_GDSONIC, 4, 0, "GDS-40H   " },
    { BRAND_GDSONIC, 4, 1, "GDS-40M   " },
    { BRAND_GDSONIC, 4, 2, "GDS-40S   " },
    { BRAND_GDSONIC, 5, 0, "GDS-50H   " },
    { BRAND_GDSONIC, 5, 1, "GDS-50M   " },
    { BRAND_GDSONIC, 5, 2, "GDS-50S   " },
    { BRAND_MOOHAN, 0, 0, "MH-1515DH " },
    { BRAND_MOOHAN, 0, 1, "MH-1515DM " },
    { BRAND_MOOHAN, 0, 2, "MH-1515DS " },
    { BRAND_MOOHAN, 1, 0, "MH-1520DH " },
    { BRAND_MOOHAN, 1, 1, "MH-1520DM " },
    { BRAND_MOOHAN, 1, 2, "MH-1520DS " },
    { BRAND_MOOHAN, 2, 0, "MH-1535DH " },
    { BRAND_MOOHAN, 2, 1, "MH-1535DM " },
    { BRAND_MOOHAN, 2, 2, "MH-1535DS " },
    { BRAND_MOOHAN, 3, 0, "MH-1535DH " },
    { BRAND_MOOHAN, 3, 1, "MH-1535DM " },
    { BRAND_MOOHAN, 3, 2, "MH-1535DS " },
    { BRAND_MOOHAN, 4, 0, "MH-1540DH " },
    { BRAND_MOOHAN, 4, 1, "MH-1540DM " },
    { BRAND_MOOHAN, 4, 2, "MH-1540DS " },
    { BRAND_MOOHAN, 5, 0, "MH-1550DH " },
    { BRAND_MOOHAN, 5, 1, "MH-1550DM " },
    { BRAND_MOOHAN, 5, 2, "MH-1550DS " },
    { BRAND_DIAMT, 0, 0, "DIS-15H   " },
    { BRAND_DIAMT, 0, 1, "DIS-15M   " },
    { BRAND_DIAMT, 0, 2, "DIS-15S   " },
    { BRAND_DIAMT, 1, 0, "DIS-20H   " },
    { BRAND_DIAMT, 1, 1, "DIS-20M   " },
    { BRAND_DIAMT, 1, 2, "DIS-20S   " },
    { BRAND_DIAMT, 2, 0, "DIS-30H   " },
    { BRAND_DIAMT, 2, 1, "DIS-30M   " },
    { BRAND_DIAMT, 2, 2, "DIS-30S   " },
    { BRAND_DIAMT, 3, 0, "DIS-35H   " },
    { BRAND_DIAMT, 3, 1, "DIS-35M   " },
    { BRAND_DIAMT, 3, 2, "DIS-35S   " },
    { BRAND_DIAMT, 4, 0, "DIS-40H   " },
    { BRAND_DIAMT, 4, 1, "DIS-40M   " },
    { BRAND_DIAMT, 4, 2, "DIS-40S   " },
    { BRAND_DIAMT, 5, 0, "DIS-50H   " },
    { BRAND_DIAMT, 5, 1, "DIS-50M   " },
    { BRAND_DIAMT, 5, 2, "DIS-50S   " },
    { BRAND_POWERTECH, 0, 0, "PTW-2515DH" },
    { BRAND_POWERTECH, 0, 1, "PTW-2515MD" },
    { BRAND_POWERTECH, 0, 2, "PTW-2515SD" },
    { BRAND_POWERTECH, 1, 0, "PTW-2020DH" },
    { BRAND_POWERTECH, 1, 1, "PTW-2020MD" },
    { BRAND_POWERTECH, 1, 2, "PTW-2020SD" },
    { BRAND_POWERTECH, 2, 0, "PTW- 735DH" },
    { BRAND_POWERTECH, 2, 1, "PTW- 735MD" },
    { BRAND_POWERTECH, 2, 2, "PTW- 735SD" },
    { BRAND_POWERTECH, 3, 0, "PTW- 735DH" },
    { BRAND_POWERTECH, 3, 1, "PTW- 735MD" },
    { BRAND_POWERTECH, 3, 2, "PTW- 735SD" },
    { BRAND_POWERTECH, 4, 0, "PTW- 740DH" },
    { BRAND_POWERTECH, 4, 1, "PTW- 740MD" },
    { BRAND_POWERTECH, 4, 2, "PTW- 740SD" },
    { BRAND_POWERTECH, 5, 0, "PTW- 750DH" },
    { BRAND_POWERTECH, 5, 1, "PTW- 750MD" },
    { BRAND_POWERTECH, 5, 2, "PTW- 750SD" },
    { BRAND_MAKETECH, 0, 0, "SMT-H15D  " },
    { BRAND_MAKETECH, 0, 1, "SMT-A15D  " },
    { BRAND_MAKETECH, 0, 2, "SMT-S15D  " },
    { BRAND_MAKETECH, 1, 0, "SMT-H20D  " },
    { BRAND_MAKETECH, 1, 1, "SMT-A20D  " },
    { BRAND_MAKETECH, 1, 2, "SMT-S20D  " },
    { BRAND_MAKETECH, 2, 0, "SMT-H35D  " },
    { BRAND_MAKETECH, 2, 1, "SMT-A35D  " },
    { BRAND_MAKETECH, 2, 2, "SMT-S35D  " },
    { BRAND_MAKETECH, 3, 0, "SMT-H35D  " },
    { BRAND_MAKETECH, 3, 1, "SMT-A35D  " },
    { BRAND_MAKETECH, 3, 2, "SMT-S35D  " },
    { BRAND_MAKETECH, 4, 0, "SMT-H40D  " },
    { BRAND_MAKETECH, 4, 1, "SMT-A40D  " },
    { BRAND_MAKETECH, 4, 2, "SMT-S40D  " },
    { BRAND_MAKETECH, 5, 0, "SMT-H50D  " },
    { BRAND_MAKETECH, 5, 1, "SMT-A50D  " },
    { BRAND_MAKETECH, 5, 2, "SMT-S50D  " },
};

/* 1. 90 조합 골든 */
static void test_golden_all_brands(void)
{
    CHECK_EQ(sizeof k_golden / sizeof k_golden[0], 90u);
    for (size_t i = 0u; i < sizeof k_golden / sizeof k_golden[0]; i++) {
        const golden_t *g = &k_golden[i];
        uint8_t out[BRAND_MODEL_STR_LEN];
        memset(out, 0xAA, sizeof out);
        brand_model_str(g->b, g->freq, g->type, out);
        if (memcmp(out, g->exp, BRAND_MODEL_STR_LEN) != 0) {   /* exp 의 NUL 까지 11 B */
            printf("FAIL golden b=%u f=%u t=%u got \"%.10s\" exp \"%s\"\n",
                   (unsigned)g->b, (unsigned)g->freq, (unsigned)g->type,
                   (const char *)out, g->exp);
            failures++;
        }
    }
}

/* 2. 범위 밖 freq/type → default = (0,0) */
static void test_out_of_range_matches_default(void)
{
    for (int b = BRAND_GDSONIC; b <= BRAND_MAKETECH; b++) {
        uint8_t a[BRAND_MODEL_STR_LEN], d[BRAND_MODEL_STR_LEN];
        brand_model_str((brand_t)b, 9u, 9u, a);
        brand_model_str((brand_t)b, 0u, 0u, d);
        CHECK_EQ(memcmp(a, d, BRAND_MODEL_STR_LEN), 0);
    }
}

/* 3. 번호 → 브랜드 (0·모르는 번호 = GDSONIC) */
static void test_brand_from_id(void)
{
    CHECK_EQ(brand_from_id(0u), BRAND_GDSONIC);
    for (uint16_t id = 1u; id <= 5u; id++) { CHECK_EQ(brand_from_id(id), id); }
    CHECK_EQ(brand_from_id(6u), BRAND_GDSONIC);
    CHECK_EQ(brand_from_id(7u), BRAND_GDSONIC);
    CHECK_EQ(brand_from_id(0x0200u), BRAND_GDSONIC);
    CHECK_EQ(brand_from_id(0xFFFFu), BRAND_GDSONIC);
}

/* 4. 오염된 enum → GDSONIC 규칙 */
static void test_bad_enum_uses_gdsonic(void)
{
    uint8_t a[BRAND_MODEL_STR_LEN], d[BRAND_MODEL_STR_LEN];
    brand_model_str((brand_t)0, 0u, 0u, a);
    brand_model_str(BRAND_GDSONIC, 0u, 0u, d);
    CHECK_EQ(memcmp(a, d, BRAND_MODEL_STR_LEN), 0);
}

int main(void)
{
    test_golden_all_brands();
    test_out_of_range_matches_default();
    test_brand_from_id();
    test_bad_enum_uses_gdsonic();
    if (failures == 0) { printf("test_app_brand: all PASS\n"); return 0; }
    printf("test_app_brand: %d FAIL\n", failures);
    return 1;
}
