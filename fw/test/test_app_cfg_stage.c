/* fw/test/test_app_cfg_stage.c — host unit tests, comm/eth staging + commit 순수 로직.
 * spec: docs/superpowers/specs/2026-08-16-comm-eth-register-extension-design.md §5
 * legacy samd20 대응물 없음(신규 기능) — 충실도 기준은 spec 자체.
 *
 * 이 모듈이 지키는 핵심 = **부분 커밋 없음**. 검증 하나라도 걸리면 아무것도
 * 반영하지 않는다. 부분 반영은 마스터가 "무엇이 적용됐는지" 모르게 만든다. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "app_cfg_stage.h"

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

/* 유효한 serial 한 벌을 staged 로 올린다 */
static void stage_serial_ok(cfg_stage_t *s, uint32_t t)
{
    cfg_stage_write(s, CFG_STG_ADDR,   7u, t);
    cfg_stage_write(s, CFG_STG_SPEED,  2u, t);
    cfg_stage_write(s, CFG_STG_PARITY, 1u, t);
}

static void test_init_idle(void)
{
    cfg_stage_t s;
    cfg_stage_init(&s);
    CHECK_EQ(s.dirty, 0u);
    CHECK_EQ(s.stat,  CFG_STAT_IDLE);
}

/* staged 쓰기 = 버퍼에만. dirty 마크 + STAGED 전이 */
static void test_write_marks_dirty(void)
{
    cfg_stage_t s;
    cfg_stage_init(&s);
    cfg_stage_write(&s, CFG_STG_SPEED, 5u, 1000u);
    CHECK_EQ(cfg_stage_dirty(&s, CFG_STG_SPEED), 1u);
    CHECK_EQ(cfg_stage_dirty(&s, CFG_STG_ADDR),  0u);
    CHECK_EQ(s.val[CFG_STG_SPEED], 5u);
    CHECK_EQ(s.stat, CFG_STAT_STAGED);
}

/* DISCARD = 폐기 + IDLE 복귀 */
static void test_discard(void)
{
    cfg_stage_t s;
    cfg_stage_init(&s);
    stage_serial_ok(&s, 1000u);
    cfg_stage_discard(&s);
    CHECK_EQ(s.dirty, 0u);
    CHECK_EQ(s.stat,  CFG_STAT_IDLE);
}

/* 타임아웃 — 마스터가 죽어 남은 반쪽 편집이 미러를 영원히 가리는 것을 막는다 */
static void test_timeout_discards(void)
{
    cfg_stage_t s;
    cfg_stage_init(&s);
    cfg_stage_write(&s, CFG_STG_ADDR, 7u, 1000u);

    cfg_stage_tick(&s, 1000u + CFG_STAGE_TIMEOUT_MS - 1u);
    CHECK_EQ(s.dirty, 1u << CFG_STG_ADDR);      /* 아직 유효 */
    CHECK_EQ(s.stat,  CFG_STAT_STAGED);

    cfg_stage_tick(&s, 1000u + CFG_STAGE_TIMEOUT_MS);
    CHECK_EQ(s.dirty, 0u);
    CHECK_EQ(s.stat,  CFG_STAT_REJ_TIMEOUT);
}

/* dirty 가 없으면 타임아웃이 발화하지 않는다 (IDLE 을 REJ_TIMEOUT 으로 오염 ✗) */
static void test_timeout_needs_dirty(void)
{
    cfg_stage_t s;
    cfg_stage_init(&s);
    cfg_stage_tick(&s, 999999u);
    CHECK_EQ(s.stat, CFG_STAT_IDLE);
}

/* 각 staged 쓰기가 타임아웃 시계를 갱신한다 */
static void test_write_refreshes_timeout(void)
{
    cfg_stage_t s;
    cfg_stage_init(&s);
    cfg_stage_write(&s, CFG_STG_ADDR,  7u, 1000u);
    cfg_stage_write(&s, CFG_STG_SPEED, 2u, 1000u + CFG_STAGE_TIMEOUT_MS - 1u);
    cfg_stage_tick(&s, 1000u + CFG_STAGE_TIMEOUT_MS + 10u);
    CHECK_EQ(s.stat, CFG_STAT_STAGED);          /* 두 번째 쓰기 기준이라 아직 안 죽음 */
}

/* 범위 거부 — 경계값 포함 */
static void test_commit_range(void)
{
    cfg_stage_t s;

    cfg_stage_init(&s);                                     /* ADDR=0 = RTU 자살 */
    cfg_stage_write(&s, CFG_STG_ADDR, 0u, 1000u);
    CHECK_EQ(cfg_stage_commit(&s, MB_LINK_TCP, 0u), 0u);
    CHECK_EQ(s.stat, CFG_STAT_REJ_RANGE);
    CHECK_EQ(s.dirty, 1u << CFG_STG_ADDR);                  /* 거부해도 staged 는 유지 */

    cfg_stage_init(&s);
    cfg_stage_write(&s, CFG_STG_ADDR, 248u, 1000u);
    CHECK_EQ(cfg_stage_commit(&s, MB_LINK_TCP, 0u), 0u);
    CHECK_EQ(s.stat, CFG_STAT_REJ_RANGE);

    cfg_stage_init(&s);                                     /* 경계 통과 */
    cfg_stage_write(&s, CFG_STG_ADDR, 247u, 1000u);
    CHECK_EQ(cfg_stage_commit(&s, MB_LINK_TCP, 0u), 1u);

    cfg_stage_init(&s);
    cfg_stage_write(&s, CFG_STG_SPEED, CFG_COMM_SPEED_IDX_MAX + 1u, 1000u);
    CHECK_EQ(cfg_stage_commit(&s, MB_LINK_TCP, 0u), 0u);
    CHECK_EQ(s.stat, CFG_STAT_REJ_RANGE);

    cfg_stage_init(&s);
    cfg_stage_write(&s, CFG_STG_PARITY, CFG_COMM_PARITY_IDX_MAX + 1u, 1000u);
    CHECK_EQ(cfg_stage_commit(&s, MB_LINK_TCP, 0u), 0u);
    CHECK_EQ(s.stat, CFG_STAT_REJ_RANGE);
}

/* ether 필드에는 검증이 없다 — LCD 편집 경로에 없는 규칙을 원격에만 발명하지 않는다 */
static void test_ether_unvalidated(void)
{
    cfg_stage_t s;
    cfg_stage_init(&s);
    cfg_stage_write(&s, CFG_STG_IP_H, 0xFFFFu, 1000u);
    cfg_stage_write(&s, CFG_STG_IP_L, 0xFFFFu, 1000u);
    CHECK_EQ(cfg_stage_commit(&s, MB_LINK_RTU, 0u), 1u);
    CHECK_EQ(s.stat, CFG_STAT_COMMIT_OK);
}

/* DG-12 — 자기 링크를 끊는 커밋은 거부. 커밋 링크가 살아 있어야
 * read-back 으로 결과를 확인할 수 있고, 그것이 이 제품군의 유일한 진실이다. */
static void test_same_link_reject(void)
{
    cfg_stage_t s;

    cfg_stage_init(&s);                       /* RTU 로 serial 커밋 = 자살 */
    stage_serial_ok(&s, 1000u);
    CHECK_EQ(cfg_stage_commit(&s, MB_LINK_RTU, 0u), 0u);
    CHECK_EQ(s.stat, CFG_STAT_REJ_SAME_LINK);

    cfg_stage_init(&s);                       /* TCP 로 ether 커밋 = 자살 */
    cfg_stage_write(&s, CFG_STG_IP_H, 0xC0A8u, 1000u);
    CHECK_EQ(cfg_stage_commit(&s, MB_LINK_TCP, 0u), 0u);
    CHECK_EQ(s.stat, CFG_STAT_REJ_SAME_LINK);
}

/* 교차 경로는 통과 — 이것이 DG-12 설계의 존재 이유다 */
static void test_cross_link_ok(void)
{
    cfg_stage_t s;

    cfg_stage_init(&s);                       /* TCP 로 serial 커밋 */
    stage_serial_ok(&s, 1000u);
    CHECK_EQ(cfg_stage_commit(&s, MB_LINK_TCP, 0u), 1u);
    CHECK_EQ(s.stat,  CFG_STAT_COMMIT_OK);
    CHECK_EQ(s.dirty, 0u);                    /* 커밋 성공 시 dirty 해제 */

    cfg_stage_init(&s);                       /* RTU 로 ether 커밋 */
    cfg_stage_write(&s, CFG_STG_IP_H, 0xC0A8u, 1000u);
    cfg_stage_write(&s, CFG_STG_IP_L, 0x0163u, 1000u);
    CHECK_EQ(cfg_stage_commit(&s, MB_LINK_RTU, 0u), 1u);
    CHECK_EQ(s.stat, CFG_STAT_COMMIT_OK);
}

/* 가동 중 커밋 거부 — 가동 중 통신 링크 재초기화를 막는다 */
static void test_running_reject(void)
{
    cfg_stage_t s;
    cfg_stage_init(&s);
    stage_serial_ok(&s, 1000u);
    CHECK_EQ(cfg_stage_commit(&s, MB_LINK_TCP, 1u), 0u);
    CHECK_EQ(s.stat, CFG_STAT_REJ_RUNNING);
}

/* 검증 순서 = 범위 → same-link → 가동중 (spec §5.3).
 * 셋 다 걸리는 입력이 REJ_RANGE 로 나와야 순서가 지켜진 것이다. */
static void test_validation_order(void)
{
    cfg_stage_t s;
    cfg_stage_init(&s);
    cfg_stage_write(&s, CFG_STG_ADDR, 0u, 1000u);     /* 범위 ✗ + serial + RTU + 가동중 */
    CHECK_EQ(cfg_stage_commit(&s, MB_LINK_RTU, 1u), 0u);
    CHECK_EQ(s.stat, CFG_STAT_REJ_RANGE);

    cfg_stage_init(&s);                                /* same-link ✗ + 가동중 */
    stage_serial_ok(&s, 1000u);
    CHECK_EQ(cfg_stage_commit(&s, MB_LINK_RTU, 1u), 0u);
    CHECK_EQ(s.stat, CFG_STAT_REJ_SAME_LINK);
}

/* 부분 커밋 없음 — 한 필드가 범위를 벗어나면 멀쩡한 필드도 반영되지 않는다 */
static void test_no_partial_commit(void)
{
    cfg_stage_t s;
    cfg_stage_init(&s);
    cfg_stage_write(&s, CFG_STG_SPEED, 2u,   1000u);   /* 유효 */
    cfg_stage_write(&s, CFG_STG_ADDR,  248u, 1000u);   /* 무효 */
    CHECK_EQ(cfg_stage_commit(&s, MB_LINK_TCP, 0u), 0u);
    CHECK_EQ(s.stat, CFG_STAT_REJ_RANGE);
    CHECK_EQ(cfg_stage_dirty(&s, CFG_STG_SPEED), 1u);  /* 유효 필드도 그대로 대기 */
}

/* 빈 커밋은 stat 을 **건드리지 않는다**. 예전엔 무조건 COMMIT_OK 로 덮었는데,
 * 그러면 아래 test_timeout_not_reported_as_commit_ok 의 사고가 난다. */
static void test_empty_commit_keeps_stat(void)
{
    cfg_stage_t s;
    cfg_stage_init(&s);
    CHECK_EQ(cfg_stage_commit(&s, MB_LINK_RTU, 0u), 0u);
    CHECK_EQ(s.stat, CFG_STAT_IDLE);      /* staging 이 없었으니 IDLE 그대로 */
}

/* 🔴 타임아웃으로 폐기된 staging 에 커밋하면 **성공으로 보고되면 안 된다.**
 * 사고 시나리오: 마스터가 ether 6칸을 staged → 재시도/링크 딸꾹질로 다음 FC06 이
 * 30초를 넘김 → staging 이 조용히 폐기됨 → 마스터가 CFG_CTRL=1 을 보내고
 * CFG_STAT=2(COMMIT_OK)를 읽음 → **새 IP 가 적용된 줄 안다.** 아무것도 안 바뀌었고
 * REJ_TIMEOUT 증거까지 지워진 상태다. */
static void test_timeout_not_reported_as_commit_ok(void)
{
    cfg_stage_t s;
    cfg_stage_init(&s);
    cfg_stage_write(&s, CFG_STG_IP_H, 0xC0A8u, 1000u);
    cfg_stage_tick(&s, 1000u + CFG_STAGE_TIMEOUT_MS);
    CHECK_EQ(s.stat,  CFG_STAT_REJ_TIMEOUT);
    CHECK_EQ(s.dirty, 0u);

    CHECK_EQ(cfg_stage_commit(&s, MB_LINK_RTU, 0u), 0u);
    CHECK_EQ(s.stat, CFG_STAT_REJ_TIMEOUT);   /* 사유가 살아남아야 한다 */
}

/* 거부 사유도 마찬가지로 빈 커밋에 지워지지 않는다 */
static void test_reject_reason_survives_empty_commit(void)
{
    cfg_stage_t s;
    cfg_stage_init(&s);
    cfg_stage_write(&s, CFG_STG_ADDR, 0u, 1000u);
    CHECK_EQ(cfg_stage_commit(&s, MB_LINK_TCP, 0u), 0u);
    CHECK_EQ(s.stat, CFG_STAT_REJ_RANGE);
    cfg_stage_discard(&s);                     /* dirty 비움 */
    CHECK_EQ(s.stat, CFG_STAT_IDLE);           /* DISCARD 는 명시적 초기화라 IDLE */
}

/* COMMIT_OK 는 다음 staged 쓰기에서 해제된다 (§5.5) */
static void test_stat_clears_on_next_write(void)
{
    cfg_stage_t s;
    cfg_stage_init(&s);
    stage_serial_ok(&s, 1000u);
    CHECK_EQ(cfg_stage_commit(&s, MB_LINK_TCP, 0u), 1u);
    CHECK_EQ(s.stat, CFG_STAT_COMMIT_OK);

    cfg_stage_write(&s, CFG_STG_SPEED, 3u, 2000u);
    CHECK_EQ(s.stat, CFG_STAT_STAGED);
}

/* 그룹 마스크가 실제 인덱스와 맞는지 — 교차 규칙 전체가 이 상수에 걸려 있다 */
static void test_group_masks(void)
{
    CHECK_EQ(CFG_STG_SERIAL_MASK,
             (1u << CFG_STG_ADDR) | (1u << CFG_STG_SPEED) | (1u << CFG_STG_PARITY));
    CHECK_EQ(CFG_STG_ETHER_MASK,
             (1u << CFG_STG_IP_H) | (1u << CFG_STG_IP_L) |
             (1u << CFG_STG_NM_H) | (1u << CFG_STG_NM_L) |
             (1u << CFG_STG_GW_H) | (1u << CFG_STG_GW_L));
    CHECK_EQ(CFG_STG_SERIAL_MASK & CFG_STG_ETHER_MASK, 0u);   /* 겹침 ✗ */
    CHECK_EQ(CFG_STG_COUNT, 9u);
}

/* C-1/C-3 — calibration 은 int16 인데 레지스터는 u16 이다. 이 변환에 부호 사고가
 * 산다: 명시하지 않으면 음수 cal 이 65000대 양수로 읽힌다.
 * 클램프가 필요한 이유는 표시가 아니다 — cal_val 은 제어 루프 입력이라
 * (cal_val → disp_amp → curr_power → acc_energy → weld 에너지 EXIT 판정,
 *  그리고 app_reg 의 비사이클 가동 정지 판정) 범위 밖 값이 **기계의 정지 시점**을
 * 왜곡한다. */
static void test_cal_from_wire(void)
{
    /* 양수 · 음수 2의 보수 왕복 */
    CHECK_EQ(cfg_cal_from_wire(16u),     16);
    CHECK_EQ(cfg_cal_from_wire(0u),      0);
    CHECK_EQ(cfg_cal_from_wire(0xFFFFu), -1);      /* 65535 = -1, 65534 = -2 ... */
    CHECK_EQ(cfg_cal_from_wire(0xFFF0u), -16);

    /* 경계 — 클램프는 부호 도메인에서 일어나야 한다 */
    CHECK_EQ(cfg_cal_from_wire(CFG_CAL_MAX),        CFG_CAL_MAX);
    CHECK_EQ(cfg_cal_from_wire(CFG_CAL_MAX + 1u),   CFG_CAL_MAX);
    CHECK_EQ(cfg_cal_from_wire((uint16_t)CFG_CAL_MIN),        CFG_CAL_MIN);
    CHECK_EQ(cfg_cal_from_wire((uint16_t)(CFG_CAL_MIN - 1)),  CFG_CAL_MIN);

    /* 극단값이 조용히 통과하지 않는다 */
    CHECK_EQ(cfg_cal_from_wire(0x7FFFu), CFG_CAL_MAX);   /* +32767 */
    CHECK_EQ(cfg_cal_from_wire(0x8000u), CFG_CAL_MIN);   /* -32768 */

    /* 대칭 — 한쪽만 넓으면 트림이 한 방향으로 치우친다 */
    CHECK_EQ(CFG_CAL_MAX, -CFG_CAL_MIN);
}

int main(void) {
    test_init_idle();
    test_write_marks_dirty();
    test_discard();
    test_timeout_discards();
    test_timeout_needs_dirty();
    test_write_refreshes_timeout();
    test_commit_range();
    test_ether_unvalidated();
    test_same_link_reject();
    test_cross_link_ok();
    test_running_reject();
    test_validation_order();
    test_no_partial_commit();
    test_empty_commit_keeps_stat();
    test_timeout_not_reported_as_commit_ok();
    test_reject_reason_survives_empty_commit();
    test_stat_clears_on_next_write();
    test_group_masks();
    test_cal_from_wire();
    if (failures) { printf("%d check(s) FAILED\n", failures); return 1; }
    printf("app_cfg_stage: all tests passed\n");
    return 0;
}
