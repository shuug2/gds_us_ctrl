/* fw/test/test_app_reg_calc.c — host unit tests for the pure regulation
 * compute layer (reg_scale, reg_output_level). No HAL, no hardware.
 * Verifies the disasm-adjudicated facts SCALE-04/05/06 and C1/C2 / §4.3 table. */
#include <stdio.h>
#include <stdint.h>
#include "app_reg_calc.h"

static int failures = 0;

#define CHECK_EQ(expr, expected) do {                                       \
    unsigned long a_ = (unsigned long)(expr);                               \
    unsigned long e_ = (unsigned long)(expected);                           \
    if (a_ != e_) {                                                         \
        printf("FAIL %s:%d  %s = %lu, expected %lu\n",                      \
               __FILE__, __LINE__, #expr, a_, e_);                          \
        failures++;                                                         \
    }                                                                       \
} while (0)

/* SCALE-04 (input ceiling), SCALE-05 (input floor), SCALE-06 (×6).
 * Faithful, do NOT normalize: the function is intentionally discontinuous
 * (999->5994 but 1000->1000) because the clip tests the INPUT and the ×6 path
 * is unbounded. Keep these vectors verbatim. */
static void test_reg_scale(void) {
    CHECK_EQ(reg_scale(0), 0);        /* < 3 -> floor */
    CHECK_EQ(reg_scale(2), 0);        /* < 3 -> floor */
    CHECK_EQ(reg_scale(3), 18);       /* 3 falls through -> ×6 (no off-by-one) */
    CHECK_EQ(reg_scale(4), 24);       /* ×6 */
    CHECK_EQ(reg_scale(999), 5994);   /* ×6, NOT clamped to 1000 */
    CHECK_EQ(reg_scale(1000), 1000);  /* input >= 1000 -> 1000 */
    CHECK_EQ(reg_scale(1023), 1000);  /* input >= 1000 -> 1000 */
}

/* C2: first i in 0..20 with lookup_table[i] < scaled (strictly-less);
 * no match -> 21. Table §4.3: table[0]=0x03FF=1023 ... table[20]=0x000F=15. */
static void test_reg_output_level(void) {
    CHECK_EQ(reg_output_level(0), 21);     /* below all -> no match */
    CHECK_EQ(reg_output_level(15), 21);    /* table[20]=15; 15<15 false -> no match */
    CHECK_EQ(reg_output_level(16), 20);    /* table[20]=15 < 16 -> i=20 */
    CHECK_EQ(reg_output_level(1023), 1);   /* table[0]=1023<1023 false; table[1]=972<1023 -> i=1 */
    CHECK_EQ(reg_output_level(1024), 0);   /* table[0]=1023 < 1024 -> i=0 */
    CHECK_EQ(reg_output_level(5994), 0);   /* huge -> i=0 */
}

/* §4.3: table values byte-match, strictly decreasing for idx 0..20. */
static void test_table_values(void) {
    static const uint16_t expect[21] = {
        0x03FF,0x03CC,0x0399,0x0366,0x0333,0x0300,0x02CD,0x029A,
        0x0267,0x0234,0x0201,0x01CE,0x01B9,0x0168,0x0135,0x0102,
        0x00CF,0x009C,0x0069,0x0036,0x000F
    };
    for (int i = 0; i < 21; i++) {
        CHECK_EQ(reg_lookup_table[i], expect[i]);
        if (i > 0) {
            if (!(reg_lookup_table[i] < reg_lookup_table[i-1])) {
                printf("FAIL table not strictly decreasing at idx %d\n", i);
                failures++;
            }
        }
    }
}

int main(void) {
    test_reg_scale();
    test_reg_output_level();
    test_table_values();
    if (failures) { printf("%d check(s) FAILED\n", failures); return 1; }
    printf("all checks PASSED\n");
    return 0;
}
