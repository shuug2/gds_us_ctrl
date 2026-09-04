/* fw/include/app_modbus_core.h — pure Modbus slave core shared by RTU (slice 1)
 * and TCP (slice 2). samd20 modbus.c port: CRC16, FC 01/02/03/04/05/06 decode,
 * holding/coil tables. HAL-free — host-tested (fw/test). The transport layer
 * hands in ONE complete frame (RTU: addr..crc); the app layer owns what the
 * registers MEAN (app_modbus.c mirror/apply passes). */
#pragma once
#include <stdint.h>

#define MB_REG_COUNT    50u    /* samd20 holdingReg[50] */
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
#define MB_REG_MODEL_FREQ   0x17u   /* read-only: mirror overwrites (samd20 faithful) */
#define MB_REG_MODEL_TYPE   0x18u   /* read-only: mirror overwrites (samd20 faithful) */
#define MB_REG_RESET        0x19u   /* command: consume-and-clear */
#define MB_REG_SEEK         0x1Au   /* command: consume-and-clear */
#define MB_REG_START        0x1Bu   /* command: consume-and-clear */
#define MB_REG_STOP         0x1Cu   /* command: consume-and-clear */
#define MB_REG_STATUS       0x1Du

/* 원격 제어 활성화 게이트 (2026-08-15 spec §4) — samd20 대응물 없는 신규.
 * 0x1E~0x29는 F-A(comm/eth 확장) 예약으로 비워 둔다. MB_REG_COUNT(50) 불변. */
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
} mb_core_t;

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
