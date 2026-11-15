/* fw/src/boot_magic.c — mon RX 매직 매처 (순수). */
#include <string.h>
#include "boot_magic.h"

/* 창 비우기 */
void boot_magic_init(boot_magic_t *m) { memset(m->win, 0, sizeof m->win); }

/* 창을 한 칸 밀고 새 바이트를 끝에 넣은 뒤 비교 */
bool boot_magic_feed(boot_magic_t *m, uint8_t b)
{
    memmove(m->win, m->win + 1, BOOT_MAGIC_LEN - 1u);
    m->win[BOOT_MAGIC_LEN - 1u] = b;
    return memcmp(m->win, BOOT_MAGIC_STR, BOOT_MAGIC_LEN) == 0;
}
