/* fw/src/app_horn.c — SYS_HORN(horn-down) 글루: 10ms tick에 FSM 진행,
 * 솔레노이드 레벨 엣지에만 io 구동.
 *
 * legacy SYS_HORN(samd20 main.c:1427-1441/1634-1644): 별도 시스템 상태 —
 * 양손 START가 솔 토글로 소비되고 초음파/weld 사이클은 완전 배제. 배제는
 * 게이트 2곳(app_reg_start_allowed() 모든 소스 START 차단 + app_weld_tick 동결)
 * 이 **새 START** 를 막고, 진행 중인 런은 app_horn_tick 이 매 tick RUN_RELEASE 로
 * 세운다(2026-09-06 벤치 H-12 — 게이트만으로는 horn 진입 전에 시작된 런이 계속
 * 나갔다; legacy 는 SYS_HORN 이면 RUN 분기 자체가 안 돈다 main.c:1427/1634).
 * 모드 진입/이탈(LCD STD SETUP DATA_SAVE → app_lcd_hook_horn)·E-stop 강제
 * OFF는 순수 FSM(app_horn_fsm) 소관. */
#include "app_horn.h"
#include "app_horn_fsm.h"
#include "app_input.h"     /* app_estop_active */
#include "app_reg.h"       /* app_reg_run_src / app_reg_command — horn 중 진행 런 강제 정지 */
#include "io.h"
#include "sys_tick.h"
#include "mon.h"

#define HORN_TICK_MS  10u

static uint32_t s_prev_ms;
static uint8_t  s_mode;
static uint8_t  s_sol_last;

/* horn 글루 초기화 */
void app_horn_init(void)
{
    horn_fsm_init();
    s_prev_ms  = sys_tick_get_ms();
    s_mode     = 0u;
    s_sol_last = 0u;
}

/* horn 모드 진입/이탈 */
void app_horn_set_mode(bool on)
{
    uint8_t m = on ? 1u : 0u;
    if (m != s_mode) {
        /* 진입/이탈 전이 시 솔 무조건 OFF (legacy 3459/3468). write-on-change
         * 캐시(s_sol_last)를 우회하는 직접 write 필수 — weld 글루가 자기
         * s_sol_last로 내려놓은 SOL은 이쪽 캐시(0)와 비교해선 절대 안 꺼짐
         * (리뷰 CRITICAL: SETUP 중 동결된 mid-CYL1 SOL이 horn 진입 후 영구
         * 잔류). app_input E-stop 엣지의 무조건 io_sol_dn(false) 패턴과 동일.
         * 모드 무변화 재저장(매 STD SAVE)엔 무조작 — legacy 3466 'if SYS_HORN'
         * 게이트 등가 (동결 weld SOL을 일반 저장이 건드리면 안 됨). */
        io_sol_dn(false);
        s_sol_last = 0u;
        mon_printf("[horn] mode=%u SOL_DN off\r\n", (unsigned)m);
    }
    s_mode = m;
}

/* horn 모드 활성 여부 */
uint8_t app_horn_mode_active(void)
{
    return s_mode;
}

/* horn 10ms tick 처리 */
void app_horn_tick(void)
{
    uint32_t now = sys_tick_get_ms();
    if ((uint32_t)(now - s_prev_ms) < HORN_TICK_MS) {
        return;
    }
    s_prev_ms = now;

    horn_in_t in = {
        .mode  = s_mode,
        .key1  = io_read_key1(),
        .key2  = io_read_key2(),
        .estop = app_estop_active(),
    };
    uint8_t sol = horn_fsm_step(&in);
    if (sol != s_sol_last) {
        s_sol_last = sol;
        io_sol_dn(sol != 0u);
        mon_printf("[horn] SOL_DN %s\r\n", (sol != 0u) ? "on" : "off");  /* 벤치 mon 관측 */
    }

    /* horn 모드 중 초음파 배제 — 진행 런 강제 정지 (app_input E-stop 패턴,
     * source-matched RUN_RELEASE, idempotent). 게이트 2곳은 새 START 만 막아서
     * horn 진입 전에 시작된 런(탭·LCD·물리·COMM·hold)이 계속 나갔다(벤치 H-12).
     * hold 세션은 다음 hold_wdt_step 이 run_is_comm=0 을 보고 조용히 끝난다.
     * 지연-0 접근자를 쓴다 — g_measure 미러는 1-iter lag. */
    if (s_mode != 0u) {
        uint8_t src = app_reg_run_src();
        if (src != (uint8_t)US_IDLE) {
            app_reg_command(US_CMD_RUN_RELEASE, src);
        }
    }
}
