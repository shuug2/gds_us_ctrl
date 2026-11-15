/* fw/test/test_boot_magic.c — host unit tests, mon RX 매직 매처.
 * 계약: 매직 14 B 가 연속으로 들어온 순간에만 true. 오탐 없음(V-8). */
#include <stdio.h>
#include <string.h>
#include "boot_magic.h"

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

/* 문자열을 넣고 true 가 된 횟수, 마지막 바이트에서 true 였는지 */
static unsigned feed_str(boot_magic_t *m, const char *s, unsigned *hit_last)
{
    unsigned hits = 0u;
    size_t n = strlen(s);
    for (size_t i = 0u; i < n; i++) {
        unsigned h = boot_magic_feed(m, (uint8_t)s[i]) ? 1u : 0u;
        hits += h;
        if (hit_last != NULL) { *hit_last = h; }
    }
    return hits;
}

/* 1. 매직 그대로 → 마지막 바이트에서 1 회 */
static void test_exact_match(void)
{
    boot_magic_t m; unsigned last = 0u;
    boot_magic_init(&m);
    CHECK_EQ(feed_str(&m, BOOT_MAGIC_STR, &last), 1u);
    CHECK_EQ(last, 1u);
}

/* 2. 앞에 로그 잡음·겹치는 접두("!!!") 뒤에도 일치 */
static void test_noise_and_overlap(void)
{
    boot_magic_t m; unsigned last = 0u;
    boot_magic_init(&m);
    CHECK_EQ(feed_str(&m, "[boot] x\r\n!!!GDSUS_BOOT!!", &last), 1u);
    CHECK_EQ(last, 1u);
    boot_magic_init(&m);
    CHECK_EQ(feed_str(&m, "!!GDSUS!!GDSUS_BOOT!!", &last), 1u);
}

/* 3. 부분·변형·한 글자 끼움 → 0 (V-8 오탐 없음) */
static void test_no_false_positive(void)
{
    boot_magic_t m;
    boot_magic_init(&m);
    CHECK_EQ(feed_str(&m, "!!GDSUS_BOOT!", NULL), 0u);
    boot_magic_init(&m);
    CHECK_EQ(feed_str(&m, "!!gdsus_boot!!", NULL), 0u);
    boot_magic_init(&m);
    CHECK_EQ(feed_str(&m, "!!GDSUS_B OOT!!", NULL), 0u);
    boot_magic_init(&m);
    CHECK_EQ(feed_str(&m, "", NULL), 0u);
    for (unsigned i = 0u; i < 1000u; i++) {          /* 0 바이트 연속(창 초기값)도 매직 아님 */
        CHECK_EQ(boot_magic_feed(&m, 0u), 0u);
    }
}

/* 4. 연속 두 번 → 두 번 */
static void test_repeat(void)
{
    boot_magic_t m;
    boot_magic_init(&m);
    CHECK_EQ(feed_str(&m, BOOT_MAGIC_STR BOOT_MAGIC_STR, NULL), 2u);
}

int main(void)
{
    test_exact_match();
    test_noise_and_overlap();
    test_no_false_positive();
    test_repeat();
    if (failures != 0) { printf("boot_magic: %d FAIL\n", failures); return 1; }
    printf("boot_magic: PASS\n");
    return 0;
}
