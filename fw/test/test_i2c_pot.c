/* fw/test/test_i2c_pot.c — host unit tests for the pure I2C_POT amplitude
 * conversion (pot_dac_from_power). No HAL, no hardware: includes only the
 * HAL-free i2c_pot.h inline. Verifies the samd20 (output_power-50)*255/100
 * formula over [50,100] plus the underflow guard (<=50 -> 0) and upper
 * saturation (>=100 -> 127). spec: docs/superpowers/specs/2026-06-28-i2c-pot-amplitude-actuation-design.md §4/§7.1. */
#include <stdio.h>
#include <stdint.h>
#include "i2c_pot.h"

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

/* §4 공식: 50~100 구간은 samd20 (power-50)*255/100과 비트-동일. */
static void test_pot_dac_midrange(void) {
    CHECK_EQ(pot_dac_from_power(51),  2);    /* (1*255)/100 = 2  */
    CHECK_EQ(pot_dac_from_power(75),  63);   /* (25*255)/100 = 63 */
    CHECK_EQ(pot_dac_from_power(90),  102);  /* (40*255)/100 = 102 */
    CHECK_EQ(pot_dac_from_power(99),  124);  /* (49*255)/100 = 124 */
}

/* §4 하한: power<=50 -> 0 (언더플로 가드; samd20는 무가드라 49↓에서 거동 차이). */
static void test_pot_dac_floor(void) {
    CHECK_EQ(pot_dac_from_power(50), 0);     /* 가드 적중 + 공식((0*255)/100=0)도 동일 → off-by-one 아님 */
    CHECK_EQ(pot_dac_from_power(49), 0);     /* 가드: 음수 언더플로 방지 */
    CHECK_EQ(pot_dac_from_power(0),  0);
}

/* §4 상한: power>=100 -> 127 (= (100-50)*255/100 = 127). */
static void test_pot_dac_ceiling(void) {
    CHECK_EQ(pot_dac_from_power(100), 127);
    CHECK_EQ(pot_dac_from_power(120), 127);  /* 클램프 */
    CHECK_EQ(pot_dac_from_power(255), 127);  /* 극단 클램프 */
}

int main(void) {
    test_pot_dac_midrange();
    test_pot_dac_floor();
    test_pot_dac_ceiling();
    if (failures) { printf("%d check(s) FAILED\n", failures); return 1; }
    printf("all checks PASSED\n");
    return 0;
}
