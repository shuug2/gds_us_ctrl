/* fw/include/cfg_clamp.h — config-validation 클램프 (감사 D2/M4). 순수 인라인,
 * LCD 터치 경로가 사용 (Modbus apply_writes의 인라인 클램프와 동일 범위 유지 —
 * 범위 변경 시 양쪽 함께). host-test: test_app_config. */
#pragma once
#include <stdint.h>

static inline uint16_t cfg_clamp_max(uint16_t v, uint16_t max)
{
    return (v > max) ? max : v;
}

static inline uint16_t cfg_clamp_power(uint16_t v)   /* [50,100] — 진폭 언더플로 가드 */
{
    if (v > 100u) { return 100u; }
    if (v < 50u)  { return 50u; }
    return v;
}
