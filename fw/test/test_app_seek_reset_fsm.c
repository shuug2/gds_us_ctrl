/* fw/test/test_app_seek_reset_fsm.c — host unit tests for the pure SEEK/RESET
 * FSM core. No HAL, no hardware. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "app_seek_reset_fsm.h"

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

static void test_init_idle(void)
{
    seek_reset_fsm_init();
    CHECK_EQ(seek_reset_fsm_state(), SR_IDLE);
}

int main(void)
{
    test_init_idle();
    if (failures) { printf("test_app_seek_reset_fsm: %d FAILED\n", failures); return 1; }
    printf("test_app_seek_reset_fsm: all passed\n");
    return 0;
}
