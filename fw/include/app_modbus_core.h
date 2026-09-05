/* fw/include/app_modbus_core.h — pure Modbus slave core shared by RTU (slice 1)
 * and TCP (slice 2). samd20 modbus.c port: CRC16, FC 01/02/03/04/05/06 decode,
 * holding/coil tables. HAL-free — host-tested (fw/test). The transport layer
 * hands in ONE complete frame (RTU: addr..crc); the app layer owns what the
 * registers MEAN (app_modbus.c mirror/apply passes). */
#pragma once
#include <stdint.h>

#define MB_REG_COUNT    50u    /* samd20 holdingReg[50] */
/* 🔴 확장 한계 — 다음에 레지스터를 늘리려는 사람이 먼저 읽을 것.
 * ① **프레임 상한 57 레지스터.** FC03 응답 = 3 + N*2 + CRC2 이고 MB_RESP_MAX=125
 *    이므로 N<=57 이다(현재 50칸 = 105 B). 여유는 **7칸**뿐이다.
 * ② 그 이상이 필요해지면 소비 측이 **폴링 블록을 쪼개 두 번 읽어야 하고, 두 읽기
 *    사이의 원자성이 깨진다.** 원격기(gds_us_remote)·gds_us_hmi 의 화면 판정은
 *    "스냅샷 하나가 진실"을 전제로 설계돼 있어, 이건 우리만의 제약이 아니다.
 *    → 확장 전에 **어느 필드가 같은 프레임에 있어야 하는가**부터 합의할 것.
 *    (2026-09-04 gds_us_remote 와 상호 확인) */
#define MB_COIL_COUNT   50u    /* samd20 coils[50] */
#define MB_FRAME_MAX    125u   /* samd20 received[125] */
#define MB_RESP_MAX     125u   /* samd20 response[125]; FC03 all-50-regs = 105 B */

/* H_REG register map (samd20 modbus.h verbatim) */
#define MB_REG_WORK_CNTH    0x00u
#define MB_REG_WORK_CNTL    0x01u   /* write 0 = work counter reset (samd20 main.c:4539) */
#define MB_REG_DISP_POWER   0x02u
#define MB_REG_DISP_AMP     0x03u
#define MB_REG_DISP_FREQ    0x04u
#define MB_REG_DISP_ENERGY  0x05u
#define MB_REG_OUT_POWER    0x06u
#define MB_REG_ON_TIME      0x07u
#define MB_REG_ENERGY       0x08u
#define MB_REG_TIMEOVER     0x09u
#define MB_REG_DELAY1       0x0Au
#define MB_REG_DELAY2       0x0Bu
#define MB_REG_DELAY3       0x0Cu
#define MB_REG_TRIGGER2     0x0Du
#define MB_REG_TRIGGER3     0x0Eu
#define MB_REG_MULTI_T1     0x0Fu
#define MB_REG_MULTI_T2     0x10u
#define MB_REG_MULTI_O1     0x11u
#define MB_REG_MULTI_O2     0x12u
#define MB_REG_RUN_MODE     0x13u
#define MB_REG_EN_ENERGY    0x14u
#define MB_REG_EN_MULTI     0x15u
#define MB_REG_EN_SAFTY     0x16u
/* B-5(2026-08-30 요구사항) 이후 **R/W**. 구 주석 "read-only: mirror overwrites
 * (samd20 faithful)" 는 `deb48bb` 가 apply 분기를 열면서 낡았다 — 벤치 MOD-2/3
 * 실측 확인(2026-09-05). LCD 편집 경로(app_lcd_input.c:447-453)와 **동형**이라
 * 범위 클램프가 없다: 범위 밖 값은 저장되고 표기만 퇴화한다.
 * 🔴 MODEL_TYPE 은 PC11 의 의미를 바꿔 **살아있는 E-stop 을 해제할 수 있다.**
 * 컨트롤러는 거부하지 않는다(2026-09-04 사용자 결정 — LCD 경로에도 같은 가드가
 * 없어 원칙 일관성을 택했다). 막아야 한다면 자리는 app_modbus.c 의 MODEL_TYPE
 * 분기이고 조건은 app_estop_active() 하나다. */
#define MB_REG_MODEL_FREQ   0x17u   /* R/W — 0..5 = 15/20/30/35/40/50 kHz (범위 밖 = 15 표기로 퇴화) */
#define MB_REG_MODEL_TYPE   0x18u   /* R/W — 0 hand / 1 multi / 2 std */
#define MB_REG_RESET        0x19u   /* command: consume-and-clear */
#define MB_REG_SEEK         0x1Au   /* command: consume-and-clear */
#define MB_REG_START        0x1Bu   /* command: consume-and-clear */
#define MB_REG_STOP         0x1Cu   /* command: consume-and-clear */
#define MB_REG_STATUS       0x1Du

/* 원격 제어 활성화 게이트 (2026-08-15 spec §4) — samd20 대응물 없는 신규.
 * 0x1E~0x29 = F-A(comm/eth 확장). MB_REG_COUNT(50) 불변. */
/* --- F-A: comm/ethernet 확장 (0x1E~0x29) ---
 * 값 레지스터가 아니라 **staging + commit** 이다. 통신 설정은 그 값을 쓰는 데
 * 쓰이는 링크 자체를 제어하므로, 즉시 반영하면 반쪽 IP 가 FRAM 에 영속되고 첫
 * 필드 반영 순간 링크가 끊겨 나머지를 쓸 기회가 사라진다.
 * staged 쓰기는 실계 무영향 → CFG_CTRL=1 한 번으로 일괄 커밋. 부분 커밋 없음.
 * spec: docs/superpowers/specs/2026-08-16-comm-eth-register-extension-design.md */
#define MB_REG_COMM_ADDR        0x1Eu  /* R/W staged — 슬레이브 주소 1..247 */
#define MB_REG_COMM_SPEED       0x1Fu  /* R/W staged — 보드레이트 index 0..5 */
#define MB_REG_COMM_PARITY      0x20u  /* R/W staged — 패리티 index 0..2 */
#define MB_REG_COMM_MODE        0x21u  /* R only — 0 SERIAL / 1 ETH_STATIC / 2 ETH_DHCP.
                                        * 원격 쓰기 ✗: SERIAL↔ETH 전환은 어느 링크로
                                        * 커밋해도 한쪽을 끊는 본질적 자기참조라
                                        * 교차 규칙으로 못 푼다. LCD 전용 유지 */
#define MB_REG_ETHER_IP_H       0x22u  /* R/W staged — ip[0]<<8 | ip[1] */
#define MB_REG_ETHER_IP_L       0x23u  /* R/W staged — ip[2]<<8 | ip[3] */
#define MB_REG_ETHER_NM_H       0x24u
#define MB_REG_ETHER_NM_L       0x25u
#define MB_REG_ETHER_GW_H       0x26u
#define MB_REG_ETHER_GW_L       0x27u
#define MB_REG_CFG_CTRL         0x28u  /* W cmd — 1 COMMIT / 2 DISCARD. 값 불문 무조건 소거 */
#define MB_REG_CFG_STAT         0x29u  /* R — CFG_STAT_* (미러) */

/* CFG_STAT(0x29) 코드. 거부 사유는 다음 staged 쓰기까지 래치된다.
 * ⚠ 게이트 거부와는 **다른 층**이다 — 게이트 사유는 0x2B 를 읽어 안다. */
#define CFG_STAT_IDLE           0u
#define CFG_STAT_STAGED         1u   /* dirty 필드 존재, 커밋 대기 */
#define CFG_STAT_COMMIT_OK      2u
#define CFG_STAT_REJ_RANGE      3u
#define CFG_STAT_REJ_SAME_LINK  4u   /* 자기 링크 그룹 커밋 시도 (DG-12) */
#define CFG_STAT_REJ_RUNNING    5u
#define CFG_STAT_REJ_TIMEOUT    6u   /* staging 만료 폐기 */

/* --- B-2: calibration (2026-08-30 요구사항). 2026-08-01 "컨트롤러 전용" 제외
 * 결정이 번복된 항목이다 — 목표가 "모든 기능 동일"이 되면서 포함으로 바뀌었다.
 * 🔴 인코딩은 **int16 2의 보수** (C-1). 명시하지 않으면 음수 cal 이 65000대
 *    양수로 읽히는 조용한 사고가 난다. 쓰기는 CFG_CAL_MIN..MAX 로 클램프(C-3). */
/* B-4 조작: horn-down 모드 원격 on/off. 관측은 STATUS 의 MB_STATUS_HORN 비트다.
 * cfg 필드가 아니라 **비영속 RAM 상태**(재부팅 시 소실)이지만, 미러/apply 형태는
 * 다른 cfg 필드와 동형으로 둔다 — 쓰기는 체인이 받고 미러가 실제 상태를 되비춘다.
 * ⚠ 모드 전이 시 솔레노이드가 무조건 OFF 된다(legacy 안전 조치). */
#define MB_REG_HORN_CMD         0x30u  /* R/W — 1 = horn-down 모드 */

/* --- F-A capability probe (0x31) ---
 * 🔴 REMOTE_CAP(0x2A)로 F-A 를 판별하면 안 된다: 게이트는 REMOTE 모델 전용이라
 * STD 는 0x2A 에 매직을 싣지 않는데, **F-A(0x1E~0x29)는 두 모델 모두에 있다.**
 * 0x2A 로 판별하면 F-A 를 지원하는 STD 유닛을 미지원으로 오판한다.
 * 그래서 F-A 전용 capability 를 따로 두고 **모델 무관 무조건 미러**한다.
 *
 * 소비 측 판정:
 *   0x31 매직 + 0x2A 매직 -> 신 펌웨어 · REMOTE 모델 (게이트 있음)
 *   0x31 매직 + 0x2A 잔류 -> 신 펌웨어 · STD 모델   (F-A 는 됨, 게이트 없음)
 *   0x31 잔류            -> 구 펌웨어 (comm 화면을 0.0.0.0 으로 그리면 안 됨)
 *
 * 구 펌웨어는 0x31 에 아무것도 쓰지 않고 mb_core_init 이 0-리셋하므로 잔류는
 * 0(또는 마스터가 쓴 값)이다 — 어느 쪽이든 매직과 같을 수 없다. 따라서 소비 측은
 * **쓰기 없이 읽기만으로** 판별할 수 있다(probe 왕복 불요). */
#define MB_REG_CFG_CAP          0x31u
#define MB_REG_CFG_CAP_MAGIC    0xFA01u

#define MB_REG_CAL_VAL          0x2Eu  /* R/W int16 — ⚠ 표시 보정이 아니라 제어 루프 입력 */
#define MB_REG_FREQ_CAL_VAL     0x2Fu  /* R/W int16 — 표시 전용 */

#define MB_REG_REMOTE_CAP       0x2Au  /* R: capability probe — 미러가 매 tick 매직 복원 */
#define MB_REG_REMOTE_EN        0x2Bu  /* R: 게이트 상태 0~5 (사유는 다음 활성화까지 래치) */
#define MB_REG_REMOTE_EN_LEFT   0x2Cu  /* 결번 — 구 "잔여 활성 초". 레벨 스위치는
                                        * 만료가 없다(요구사항 A-3). 항상 0 미러 */
/* 0x2D reserved — 2단계 승인(원격 요청→LCD 승인) 승격 경로 (spec §2.2). 미러 ✗ */

/* probe 매직. 원격기는 이 주소에 매직도 0도 아닌 값 P를 쓰고 read-back 한다:
 * P가 남아 있으면 구 펌웨어(게이트 없음), 매직으로 복원돼 있으면 신 펌웨어.
 * ⚠ P는 0이면 안 된다 — 링크 전이의 mb_core_init 0-리셋과 구분되지 않는다. */
#define MB_REG_REMOTE_CAP_MAGIC 0x5201u

/* 0x2B wire 값 — app_remote_en_fsm.h의 REN_*와 값 일치.
 * test_app_remote_en_fsm의 cross-check가 이 일치를 고정한다. */
#define MB_REMOTE_EN_DISABLED     0u
#define MB_REMOTE_EN_ENABLED      1u
#define MB_REMOTE_EN_DIS_TIMEOUT  2u   /* 결번 — 창 만료(A-3으로 폐기). 생산 안 함 */
#define MB_REMOTE_EN_DIS_LINK     3u
#define MB_REMOTE_EN_DIS_ESTOP    4u
#define MB_REMOTE_EN_DIS_LCD      5u   /* 결번 — LCD 수동 해제(T-5 폐기). 생산 안 함 */

/* STATUS bits (samd20 modbus.h). OVLD set by app_overload (슬라이스 C);
 * ESTOP/OVTIME/OUTERR stay 0 (estop/weld machinery deferred — spec §3.1). */
#define MB_STATUS_US      0x01u
#define MB_STATUS_ESTOP   0x02u
#define MB_STATUS_OVLD    0x04u
#define MB_STATUS_OVTIME  0x08u
#define MB_STATUS_OUTERR  0x10u
/* 원격 관측용 신규 비트 (2026-08-30 요구사항 B-3/B-4). samd20 에 대응물 없음 —
 * 주소를 안 먹으려고 STATUS 여유 비트로 돌린 것이다. 소비자 = 원격기 · gds_us_hmi. */
#define MB_STATUS_SENSOR  0x20u   /* B-3: SENSE_DN 감지 중 (STD TRIGGER RUN 의 "SENSOR ON") */
#define MB_STATUS_HORN    0x40u   /* B-4: horn-down 모드 — 서 있으면 모든 소스의 START 가 차단된다.
                                   * 이 비트가 없으면 원격기는 거부를 성공한 전송과 구분할 수 없다. */

/* STATUS 합성 입력. 극성 변환(SENSE_DN active-LOW 등)은 글루가 끝내고 넣는다 —
 * 이 모듈은 "무엇이 참인가"만 받고 비트 배치만 책임진다. */
typedef struct {
    uint8_t running;   /* us_run_status != US_IDLE */
    uint8_t estop;
    uint8_t ovld;
    uint8_t ovtime;
    uint8_t sensor;    /* 1 = 감지 중 */
    uint8_t horn;      /* 1 = horn-down 모드 */
} mb_status_in_t;

/* 0 이 아닌 입력은 정확히 해당 비트 1개로 정규화된다. */
uint16_t mb_status_bits(const mb_status_in_t *in);

/* decode mode (samd20 decode_comm(mode)): RTU checks slave addr + CRC,
 * TCP (slice 2, MBAP-stripped PDU) skips both. */
#define MB_MODE_RTU  0u
#define MB_MODE_TCP  1u

typedef struct {
    uint16_t holding[MB_REG_COUNT];
    uint8_t  coils[MB_COIL_COUNT];   /* FC 01/05 work; NO app mapping (samd20 faithful) */
    uint8_t  device_addr;
    /* 직전 FC06 이 쓴 레지스터 주소 (없으면 MB_REG_NONE). 앱 계층의 staged 스캔이
     * "마스터가 실제로 건드린 칸"을 알기 위해 쓴다 — 전수 비교로는 stale 미러와
     * 구분되지 않는다. 코어 거동에는 영향 없는 순수 관측값. */
    uint16_t last_write_addr;
} mb_core_t;

#define MB_REG_NONE  0xFFFFu

/* work counter 리셋 요청 판정 — "마스터가 이번 FC06 으로 WORK_CNTL 에 0 을 썼는가".
 *
 * 🔴 미러가 만든 0 과 마스터가 쓴 0 을 반드시 구분해야 한다. mirror_live() 는
 * holding[WORK_CNTL] 에 (uint16_t)work_cnt 를 싣는 하위-워드 미러라, work_cnt 가
 * **0 이 아닌 65536 의 배수**이면 미러 자체가 0 을 만든다. 그 상태에서 값 비교만
 * 하면 앞 분기에 안 걸린 **아무 FC06 이나**(staged 쓰기·같은 값 재쓰기 등) 리셋으로
 * 읽혀 생산 카운트가 FRAM 째로 날아가고(복구 불가), else-if 체인이라 그 메시지의
 * staged 쓰기까지 함께 탈락한다. last_write_addr 로 "이번 메시지가 실제로 건드린
 * 칸"을 확인해 가른다 — 같은 글루의 staged 스캔이 쓰는 기법과 같다.
 *
 * work_cnt 는 32비트 전체를 본다(하위 워드가 아니라): samd20 은 하위 워드만 비교해
 * 65536 배수에서 리셋을 조용히 무시했고, 2026-09-04 에 그 이탈이 승인됐다. LCD
 * 경로(app_lcd_input.c:385)도 32비트를 본다. */
uint8_t mb_work_cnt_reset_req(const mb_core_t *mb, uint32_t work_cnt);

/* Zero both tables + set the slave address (samd20 init_modbus tail). */
void mb_core_init(mb_core_t *mb, uint8_t device_addr);

/* Modbus CRC16 (poly 0xA001, init 0xFFFF), returned BYTE-SWAPPED like samd20
 * make_crc: high byte of the return = FIRST CRC byte on the wire (lo-first),
 * so emit resp[n] = crc>>8, resp[n+1] = crc&0xFF. */
uint16_t mb_crc16(const uint8_t *buf, uint8_t len);

/* Decode one complete frame and build the response into resp.
 * Returns the response length to transmit (0 = stay silent: other address,
 * bad CRC, unsupported FC, malformed/out-of-range — samd20 never sends Modbus
 * exception responses, faithful). *fc_out = function code actually processed
 * (0 if none); the app layer runs its write-apply pass when *fc_out == 0x06. */
uint8_t mb_core_decode(mb_core_t *mb, const uint8_t *frame, uint8_t len,
                       uint8_t mode, uint8_t resp[MB_RESP_MAX], uint8_t *fc_out);
