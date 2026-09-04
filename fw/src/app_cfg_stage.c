/* fw/src/app_cfg_stage.c — comm/eth staging + commit 순수 로직 (spec §5). */
#include "app_cfg_stage.h"

/* staging 초기화 */
void cfg_stage_init(cfg_stage_t *s)
{
    for (uint8_t i = 0u; i < (uint8_t)CFG_STG_COUNT; i++) {
        s->val[i] = 0u;
    }
    s->dirty         = 0u;
    s->last_write_ms = 0u;
    s->stat          = CFG_STAT_IDLE;
}

/* 해당 필드가 staged 상태인가 */
uint8_t cfg_stage_dirty(const cfg_stage_t *s, uint8_t idx)
{
    return ((s->dirty & (uint16_t)(1u << idx)) != 0u) ? 1u : 0u;
}

/* staged 쓰기 — 버퍼에만. cfg·FRAM·통신 실계 무영향. */
void cfg_stage_write(cfg_stage_t *s, uint8_t idx, uint16_t v, uint32_t now_ms)
{
    s->val[idx]      = v;
    s->dirty        |= (uint16_t)(1u << idx);
    s->last_write_ms = now_ms;
    /* 직전 커밋/거부 사유는 여기서 해제된다 (§5.5) — 새 편집이 시작됐으므로. */
    s->stat          = CFG_STAT_STAGED;
}

/* DISCARD */
void cfg_stage_discard(cfg_stage_t *s)
{
    s->dirty = 0u;
    s->stat  = CFG_STAT_IDLE;
}

/* 타임아웃 감시. dirty 가 없으면 아무 일도 하지 않는다 — IDLE 을 REJ_TIMEOUT 으로
 * 오염시키면 마스터가 있지도 않은 편집이 만료된 줄 안다. */
void cfg_stage_tick(cfg_stage_t *s, uint32_t now_ms)
{
    if (s->dirty == 0u) {
        return;
    }
    if ((uint32_t)(now_ms - s->last_write_ms) >= CFG_STAGE_TIMEOUT_MS) {
        s->dirty = 0u;
        s->stat  = CFG_STAT_REJ_TIMEOUT;
    }
}

/* 커밋 검증 */
uint8_t cfg_stage_commit(cfg_stage_t *s, mb_link_t link, uint8_t running)
{
    uint16_t d = s->dirty;

    if (d == 0u) {
        s->stat = CFG_STAT_COMMIT_OK;   /* 반영할 것이 없다 = 무해한 no-op */
        return 0u;
    }

    /* (1) 범위. ether 는 검증하지 않는다 — LCD 편집 경로에 유효성 검사가 없고,
     * LCD 에 없는 규칙을 원격 경로에만 발명하지 않는다. 잘못된 IP 는 조작자 책임. */
    if (((d & (1u << CFG_STG_ADDR)) != 0u) &&
        ((s->val[CFG_STG_ADDR] < CFG_COMM_ADDR_MIN) ||
         (s->val[CFG_STG_ADDR] > CFG_COMM_ADDR_MAX))) {
        s->stat = CFG_STAT_REJ_RANGE;
        return 0u;
    }
    if (((d & (1u << CFG_STG_SPEED)) != 0u) &&
        (s->val[CFG_STG_SPEED] > CFG_COMM_SPEED_IDX_MAX)) {
        s->stat = CFG_STAT_REJ_RANGE;
        return 0u;
    }
    if (((d & (1u << CFG_STG_PARITY)) != 0u) &&
        (s->val[CFG_STG_PARITY] > CFG_COMM_PARITY_IDX_MAX)) {
        s->stat = CFG_STAT_REJ_RANGE;
        return 0u;
    }

    /* (2) same-link (DG-12). 자기 링크를 끊는 커밋은 확인 수단 자체를 없앤다 —
     * 성공했는지 실패했는지 마스터가 영원히 알 수 없는 상태가 된다. */
    if (((link == MB_LINK_RTU) && ((d & CFG_STG_SERIAL_MASK) != 0u)) ||
        ((link == MB_LINK_TCP) && ((d & CFG_STG_ETHER_MASK)  != 0u))) {
        s->stat = CFG_STAT_REJ_SAME_LINK;
        return 0u;
    }

    /* (3) 가동 중 — 가동 중 통신 링크 재초기화를 막는다. */
    if (running != 0u) {
        s->stat = CFG_STAT_REJ_RUNNING;
        return 0u;
    }

    s->dirty = 0u;
    s->stat  = CFG_STAT_COMMIT_OK;
    return 1u;
}

/* calibration wire 변환 + 클램프 (C-1/C-3) */
int16_t cfg_cal_from_wire(uint16_t wire)
{
    int16_t v = (int16_t)wire;          /* 2의 보수 해석이 먼저 */
    if (v > CFG_CAL_MAX) { return CFG_CAL_MAX; }
    if (v < CFG_CAL_MIN) { return CFG_CAL_MIN; }
    return v;
}
