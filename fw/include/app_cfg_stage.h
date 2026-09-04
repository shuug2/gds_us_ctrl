/* fw/include/app_cfg_stage.h — comm/ethernet staging + commit 순수 로직. HAL-free.
 *
 * 통신 설정은 **그 값을 쓰는 데 쓰이는 링크 자체**를 제어한다. 값 레지스터 방식
 * (FC06 → 즉시 cfg → 즉시 FRAM)을 그대로 쓰면 셋 다 터진다:
 *   ① FC16 미지원이라 IP 6칸을 원자적으로 못 써 반쪽 IP 가 FRAM 에 영속되고
 *   ② comm_speed 반영 즉시 UART 가 재초기화돼 나머지를 쓸 기회가 사라지며
 *   ③ 예외응답이 없어 실패가 침묵인데, 링크가 끊기면 유일한 진실인 read-back
 *      자체가 불가능해진다.
 * 그래서 staged 버퍼에 모아 두었다가 CFG_CTRL=1 한 번으로 일괄 커밋한다.
 * **부분 커밋은 없다** — 부분 반영은 마스터가 무엇이 적용됐는지 모르게 만든다.
 *
 * legacy samd20 대응물 없음(신규 기능).
 * spec: docs/superpowers/specs/2026-08-16-comm-eth-register-extension-design.md */
#pragma once
#include <stdint.h>
#include "app_modbus_core.h"   /* CFG_STAT_* */
#include "app_config.h"        /* CFG_COMM_{ADDR_MIN,ADDR_MAX,SPEED_IDX_MAX,PARITY_IDX_MAX} */

/* staged 편집 자동 폐기 (마스터 크래시로 남은 반쪽 편집이 미러를 계속 가리는 것과
 * LCD 동시 편집 충돌 창을 제한). 벤치 단축 빌드가 -D 로 덮어쓸 수 있게 가드. */
#ifndef CFG_STAGE_TIMEOUT_MS
#define CFG_STAGE_TIMEOUT_MS  30000u
#endif

/* calibration 원격 쓰기 클램프 (C-3). 대칭.
 *
 * 🔴 근거: `cal_val` 은 표시 보정이 **아니라 제어 루프 입력**이다 —
 *   cal_val → reg_current_from_adc → disp_amp → curr_power → acc_energy →
 *   weld 에너지 EXIT 판정, 그리고 app_reg 의 비사이클 가동 정지 판정.
 *   범위 밖 값은 화면이 아니라 **기계의 정지 시점**을 왜곡한다.
 *
 * 값 선정: 넉넉하게 잡았다. LCD 편집 경로에는 클램프가 없고(app_lcd_input.c:456)
 *   실사용 트림은 한 자리~두 자리다(현장 cal_val=16 / freq_cal_val=40, 데드밴드 14).
 *   ±1000 은 그보다 훨씬 넓어 **정당한 보정을 절대 막지 않으면서**, 32767 같은
 *   값이 없는 전류를 만들어내는 것만 차단한다. 물리 보정 여지를 좁히는 쪽이
 *   레지스터 하나 지키는 것보다 나쁘다 — 벤치 실측 후 재조정 가능. */
#define CFG_CAL_MAX    1000
#define CFG_CAL_MIN  (-1000)

/* u16 wire → int16 cfg. 2의 보수 해석 후 부호 도메인에서 클램프한다.
 * 순서가 중요하다: u16 상태로 비교하면 음수가 65000대 양수로 보여 전부 통과한다. */
int16_t cfg_cal_from_wire(uint16_t wire);

/* staged 그룹 9개. 순서는 레지스터 0x1E~0x20 / 0x22~0x27 과 같다. */
enum {
    CFG_STG_ADDR = 0,
    CFG_STG_SPEED,
    CFG_STG_PARITY,
    CFG_STG_IP_H,
    CFG_STG_IP_L,
    CFG_STG_NM_H,
    CFG_STG_NM_L,
    CFG_STG_GW_H,
    CFG_STG_GW_L,
    CFG_STG_COUNT
};

/* 교차 경로 규칙(DG-12)이 통째로 이 두 마스크에 걸려 있다 — host 테스트가 고정. */
#define CFG_STG_SERIAL_MASK  0x0007u   /* ADDR | SPEED | PARITY */
#define CFG_STG_ETHER_MASK   0x01F8u   /* IP_H | IP_L | NM_H | NM_L | GW_H | GW_L */

/* 커밋이 도착한 전송 경로. 파일 스코프 플래그가 아니라 인자로 넘기는 이유:
 * 호출부가 늘거나 순서가 바뀌어도 컴파일 에러로 드러난다. */
typedef enum {
    MB_LINK_RTU = 0,
    MB_LINK_TCP = 1
} mb_link_t;

typedef struct {
    uint16_t val[CFG_STG_COUNT];
    uint16_t dirty;          /* 비트마스크 (1u << CFG_STG_*) */
    uint32_t last_write_ms;  /* 타임아웃 기준 — staged 쓰기마다 갱신 */
    uint8_t  stat;           /* CFG_STAT_* — 0x29 가 그대로 미러 */
} cfg_stage_t;

void    cfg_stage_init(cfg_stage_t *s);
uint8_t cfg_stage_dirty(const cfg_stage_t *s, uint8_t idx);
void    cfg_stage_write(cfg_stage_t *s, uint8_t idx, uint16_t v, uint32_t now_ms);
void    cfg_stage_discard(cfg_stage_t *s);        /* DISCARD — IDLE 복귀 */
void    cfg_stage_tick(cfg_stage_t *s, uint32_t now_ms);   /* 타임아웃 감시 */

/* 커밋 검증 (범위 → same-link → 가동중, 순서 고정).
 * 반환 1 = 호출측이 staged 값을 cfg 에 일괄 반영할 것. 0 = 아무것도 하지 말 것.
 * 거부 시 사유가 s->stat 에 들어가고 staged 는 **유지**된다 — 마스터가 잘못된
 * 필드만 고쳐 재시도할 수 있어야 한다. dirty 가 없으면 무해한 no-op(반환 0). */
uint8_t cfg_stage_commit(cfg_stage_t *s, mb_link_t link, uint8_t running);
