/*
 * ============================================================================
 * m16_controller_prg.c
 * ============================================================================
 * ATmega16 펌웨어 역공학 C 변환
 * 원본: m16_controller_prg.hex (Intel HEX, 16KB)
 *
 * ※ 이 코드는 ATmega16 공식 데이터시트(Doc.2466)의 I/O 레지스터 맵을
 *    기준으로 정확하게 매핑하였습니다.
 *
 * ============================================================================
 * 인터럽트 벡터 테이블 (Byte 주소 = Word주소 × 2)
 * ============================================================================
 *   벡터번호  바이트주소   인터럽트 소스           핸들러
 *   ──────── ────────── ──────────────────── ──────────
 *     1       0x0000     RESET                → 0x008A (startup)
 *     2       0x0004     INT0                 미사용 (→ 0x0000)
 *     3       0x0008     INT1                 미사용
 *     4       0x000C     TIMER2 COMP          미사용
 *     5       0x0010     TIMER2 OVF           미사용
 *     6       0x0014     TIMER1 CAPT          미사용
 *     7       0x0018     TIMER1 COMPA         미사용
 *     8       0x001C     TIMER1 COMPB         미사용
 *     9       0x0020     TIMER1 OVF           → 0x03FE ★
 *    10       0x0024     TIMER0 OVF           → 0x0280 ★
 *    11       0x0028     SPI STC              미사용
 *    12       0x002C     USART RXC            미사용
 *    13       0x0030     USART UDRE           미사용
 *    14       0x0034     USART TXC            미사용
 *    15       0x0038     ADC                  → 0x00EA ★
 *    16       0x003C     EE_RDY               미사용
 *    17       0x0040     ANA_COMP             미사용
 *    18       0x0044     TWI                  미사용
 *    19       0x0048     INT2                 미사용
 *    20       0x004C     TIMER0 COMP          미사용
 *    21       0x0050     SPM_RDY              미사용
 *
 * ============================================================================
 * I/O 레지스터 매핑 (ATmega16 데이터시트 기준)
 * ============================================================================
 *   I/O주소  레지스터       사용 내용
 *   ─────── ──────────── ──────────────────────────────
 *   0x04    ADCL          ADC 결과 하위 바이트
 *   0x05    ADCH          ADC 결과 상위 바이트
 *   0x06    ADCSRA        ADC 제어/상태 레지스터
 *   0x07    ADMUX         ADC 멀티플렉서 선택
 *   0x08    ACSR          아날로그 비교기 제어
 *   0x09    UBRRL         USART 보레이트 하위
 *   0x0A    UCSRB         USART 제어 B
 *   0x0B    UCSRA         USART 제어 A
 *   0x11    DDRD          포트D 방향
 *   0x12    PORTD         포트D 출력
 *   0x13    PINC          포트C 입력
 *   0x14    DDRC          포트C 방향
 *   0x15    PORTC         포트C 출력
 *   0x17    DDRB          포트B 방향
 *   0x18    PORTB         포트B 출력
 *   0x1A    DDRA          포트A 방향
 *   0x1B    PORTA         포트A 출력
 *   0x20    UBRRH/UCSRC   USART 보레이트 상위/제어 C
 *   0x22    ASSR          비동기 상태
 *   0x25    TCCR2         타이머2 제어
 *   0x2C    TCNT1L        타이머1 카운터 하위
 *   0x2D    TCNT1H        타이머1 카운터 상위
 *   0x2E    TCCR1B        타이머1 제어 B
 *   0x2F    TCCR1A        타이머1 제어 A
 *   0x30    SFIOR         특수 기능 I/O
 *   0x32    TCNT0         타이머0 카운터
 *   0x33    TCCR0         타이머0 제어
 *   0x34    MCUCSR        MCU 제어/상태
 *   0x35    MCUCR         MCU 제어
 *   0x39    TIMSK         타이머 인터럽트 마스크
 *   0x3A    GIFR          일반 인터럽트 플래그
 *   0x3B    GICR          일반 인터럽트 제어
 *   0x3C    OCR0          타이머0 비교값
 *   0x3D    SPL           스택 포인터 하위
 *   0x3E    SPH           스택 포인터 상위
 *   0x3F    SREG          상태 레지스터
 *
 * ============================================================================
 * 핀 배치 분석 (하드웨어 초기화 기반)
 * ============================================================================
 *   포트A: PA0~PA7  전체 입력 (ADC 채널)
 *          PA2~PA7  내부 풀업 활성 (PORTA=0xFC)
 *          PA0: ADC 채널 0 (센서 입력 1)
 *          PA1: ADC 채널 1 (센서 입력 2)
 *
 *   포트B: PB0~PB7  전체 출력 (DDRB=0xFF)
 *          PB0: LED/상태 출력 (g_run_flag 연동)
 *          PB1: LED/상태 출력 (g_blink_active 연동)
 *          PB2: 디스플레이 Digit 0 선택 (active low)
 *          PB3: 디스플레이 Digit 1 선택 (active low)
 *          PB4: 디스플레이 Digit 2 선택 (active low)
 *          PB5: 통신 상태/하트비트 LED
 *
 *   포트C: PC0,1,6,7 출력 / PC2~5 입력 (DDRC=0xC3)
 *          PC0: 출력 (보조 제어)
 *          PC1: 출력 (보조 제어)
 *          PC2: 버튼 입력 1 (PINC.2, 내부 풀업)
 *          PC3: 버튼 입력 2 (PINC.3, 내부 풀업)
 *          PC4: 외부 입력 (PINC.4)
 *          PC6: ADC 레벨 표시 출력
 *          PC7: 출력 (g_blink_state 연동)
 *
 *   포트D: PD0~PD7  전체 출력 (DDRD=0xFF)
 *          7-세그먼트 디스플레이 세그먼트 데이터 (반전 로직)
 *
 * ============================================================================
 * 컴파일 방법
 * ============================================================================
 *   avr-gcc -mmcu=atmega16 -DF_CPU=8000000UL -Os -o m16_ctrl.elf m16_controller_prg.c
 *   avr-objcopy -O ihex m16_ctrl.elf m16_ctrl.hex
 *   avrdude -p atmega16 -c [programmer] -U flash:w:m16_ctrl.hex
 *
 * ============================================================================
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <stdint.h>

/* ============================================================================
 * 룩업 테이블 (프로그램 메모리, 0x0058~0x0088)
 * ADC 값 → 스케일링 변환용 테이블 (21 entries × 16bit)
 * ============================================================================ */
const uint16_t lookup_table[24] PROGMEM = {
    0x03FF, 0x03CC, 0x0399, 0x0366, 0x0333, 0x0300,  /* 0~5 */
    0x02CD, 0x029A, 0x0267, 0x0234, 0x0201, 0x01CE,  /* 6~11 */
    0x01B9, 0x0168, 0x0135, 0x0102, 0x00CF, 0x009C,  /* 12~17 */
    0x0069, 0x0036, 0x000F, 0x0004, 0x0004, 0x0054   /* 18~23 */
};

/* ============================================================================
 * 전역 변수 (SRAM 매핑, 0x0160~0x01D3)
 * ============================================================================ */

/* --- ADC 관련 --- */
volatile uint16_t adc_buffer[2];           /* 0x0160~0x0163: ADC ch0, ch1 원시 값 */
volatile uint8_t  adc_ch_idx;              /* 0x021E: 현재 ADC 채널 인덱스 (0 or 1) */

/* --- 버튼 관련 --- */
volatile uint8_t  btn_debounce_val;        /* 0x0166: 현재 감지된 버튼 코드 */
volatile uint8_t  btn_prev_val;            /* 0x0167: 이전 버튼 코드 */
volatile uint8_t  btn_confirmed_val;       /* 0x0168: 확인된(디바운스완료) 버튼 코드 */
volatile uint8_t  btn_repeat_cnt;          /* 0x0169: 버튼 반복 카운터 */
volatile uint8_t  btn_key_code;            /* 0x0164: 최종 키 코드 */
volatile uint8_t  btn_release_flag;        /* 0x0165: 릴리스 플래그 */
volatile uint8_t  btn_state;               /* 0x016C: 버튼 처리 상태 */
volatile uint8_t  btn_pressed_flag;        /* 0x016D: 버튼 눌림 상태 (1=pressed) */
volatile uint8_t  btn_hold_flag;           /* 0x016E: 버튼 홀드 상태 */

/* --- ADC 처리/평균화 --- */
volatile uint16_t adc_ch0_sum_raw;         /* 0x01A9~0x01AA: ch0 누적용 (16bit 부분) */
volatile uint16_t adc_ch0_sum_ext;         /* 0x01AB~0x01AC: ch0 누적용 (확장) */
volatile uint16_t adc_ch0_avg;             /* 0x01B9~0x01BA: ch0 평균값 */
volatile uint16_t adc_ch0_sample_cnt;      /* 0x01BB~0x01BC: ch0 샘플 카운터 */

volatile uint16_t adc_ch1_sum_raw;         /* 0x01AD~0x01AE: ch1 누적용 */
volatile uint16_t adc_ch1_sum_ext;         /* 0x01AF~0x01B0: ch1 누적용 (확장) */
volatile uint16_t adc_ch1_avg;             /* 0x018D~0x018E: ch1 평균값 */
volatile uint16_t adc_ch1_sample_cnt;      /* 0x01BD~0x01BE: ch1 샘플 카운터 */

volatile uint16_t adc_ch0_result;          /* 0x01C1~0x01C2: ch0 최종 결과 */
volatile uint16_t adc_scaled_value;        /* 0x01B5~0x01B6: 스케일링된 최종 값 */

/* --- 타이머/카운터 --- */
volatile uint16_t timer0_tick_cnt;         /* 0x01B7~0x01B8: Timer0 틱 카운터 */
volatile uint16_t timer0_aux_cnt;          /* 0x0182~0x0183: Timer0 보조 카운터 */
volatile uint16_t timer0_process_cnt;      /* 0x01BF~0x01C0: Timer0 처리 카운터 */
volatile uint16_t timer1_tick_cnt;         /* 0x0173~0x0174: Timer1 측정용 카운터 */

/* --- 디스플레이 --- */
volatile uint8_t  disp_digit_idx;          /* 0x01A4: 현재 표시 자릿수 (0~2) */
volatile uint8_t  disp_seg_data;           /* 0x01D3: 현재 세그먼트 데이터 */
volatile uint8_t  disp_led_pattern;        /* 0x019F: PORTD용 LED/세그먼트 패턴 */
volatile uint8_t  disp_digit0_data;        /* 0x01A0: Digit0 세그먼트 데이터 */
volatile uint8_t  disp_digit1_data;        /* 0x01A1: Digit1 세그먼트 데이터 */

/* --- 상태 플래그 --- */
volatile uint8_t  g_main_state;            /* 0x0195: 메인 동작 상태 (0/1) */
volatile uint8_t  g_run_flag;              /* 0x0196: 실행 플래그 */
volatile uint8_t  g_blink_state;           /* 0x0189: 깜박임 상태 */
volatile uint8_t  g_blink_active;          /* 0x018A: 깜박임 활성 */
volatile uint8_t  g_blink_phase;           /* 0x0188: 깜박임 위상 */
volatile uint8_t  g_output_enable;         /* 0x0177: 출력 활성 플래그 */
volatile uint8_t  g_display_enable;        /* 0x0187: 디스플레이 갱신 플래그 */
volatile uint16_t g_display_period;        /* 0x018F~0x0190: 디스플레이 주기 */
volatile uint8_t  g_display_off_cnt;       /* 0x01A3: 디스플레이 OFF 카운터 */
volatile uint8_t  g_display_raw;           /* 0x0186: 디스플레이 원시 값 */
volatile uint8_t  g_output_counter;        /* 0x017A: 출력 카운터 */
volatile uint8_t  g_pwm_phase;             /* 0x0178: PWM 위상 데이터 */
volatile uint8_t  g_pwm_source;            /* 0x0175: PWM 소스 데이터 */
volatile uint8_t  g_pwr_state;             /* 0x017C: 전원/모드 상태 */
volatile uint8_t  g_timing_cnt;            /* 0x0179: 타이밍 카운터 */
volatile uint8_t  g_timing2_cnt;           /* 0x017B: 타이밍 카운터 2 */
volatile uint8_t  g_counter_81;            /* 0x0181: 보조 카운터 */
volatile uint8_t  g_comm_seq_state;        /* 0x0180: 통신 시퀀스 상태 */

/* --- 통신/프로토콜 --- */
volatile uint8_t  g_comm_done;             /* 0x01CB: 통신 완료 플래그 */
volatile uint8_t  g_comm_active;           /* 0x01CC: 통신 활성 플래그 */
volatile uint8_t  g_comm_step;             /* 0x01CD: 통신 단계 */
volatile uint8_t  g_comm_flag;             /* 0x019C: 통신 보조 플래그 */
volatile uint8_t  g_comm_flag2;            /* 0x01C5: 통신 보조 플래그 2 */
volatile uint8_t  g_comm_wait_cnt;         /* 0x01CA: 통신 대기 카운터 */
volatile uint8_t  g_comm_wait2_cnt;        /* 0x01D1: 통신 대기 카운터 2 */
volatile uint8_t  g_error_flag;            /* 0x019E: 에러 플래그 */

/* --- 스케일링/연산 --- */
volatile uint16_t scale_input;             /* 0x01A5~0x01A6: 스케일링 입력 */
volatile uint8_t  g_ch0_data;              /* 0x0197: ch0 데이터 */

/* ============================================================================
 * 함수 프로토타입
 * ============================================================================ */

/* 초기화 */
void hw_init(void);                     /* 0x1AD2: 하드웨어 초기화 */

/* ADC 처리 */
void adc_process_channels(void);        /* 0x196C: ADC 채널 데이터 처리 */
void adc_calc_scaled_output(void);      /* 0x1A5C: ADC 스케일링 연산 */

/* 버튼 처리 */
void btn_scan_and_debounce(void);       /* 0x0142: 버튼 스캔/디바운스 (ADC ISR 내) */
void btn_get_keycode(void);             /* 0x0264: 버튼 키코드 읽기 */
void btn_clear_state(void);             /* 0x0258: 버튼 상태 초기화 */
uint8_t btn_check_repeat(void);         /* 0x1BCE: 버튼 반복 체크 (carry=초과) */

/* 메인 루프 */
void main_loop_process(void);           /* 0x1226: 메인 루프 (ADC 레벨 판정) */

/* 디스플레이 */
void display_multiplex(void);           /* 0x1692: 3-digit 멀티플렉스 출력 */
void display_set_segments(uint8_t val); /* 0x2064: 세그먼트 데이터 → PORTD */
void display_invert_bits(uint8_t val);  /* 0x16F6: 비트 반전 변환 */

/* 출력/LED 제어 */
void output_level_process(void);        /* 0x138C: 출력 레벨 처리 (lookup 테이블) */
void output_direct_process(void);       /* 0x15BC: 직접 출력 처리 */

/* 통신/시퀀스 */
void comm_sequence_handler(void);       /* 0x034E: 통신 시퀀스 처리 */
void comm_start_sequence(uint8_t code); /* 0x0670: 시퀀스 시작 */

/* 유틸리티 */
uint16_t get_adc_timer1_value(void);    /* 0x1FF4: Timer1 기반 ADC 값 읽기 */
void set_output_mode_a(uint8_t val);    /* 0x1FFE: 출력 모드 A 설정 */
void set_output_mode_b(uint8_t val);    /* 0x200A: 출력 모드 B 설정 */
void set_output_mode_c(uint8_t val);    /* 0x2016: 출력 모드 C 설정 */
void set_output_full(void);             /* 0x2022: 출력 최대 설정 */
uint8_t compare_with_scaled(uint16_t val); /* 0x202A: 스케일 값 비교 */
void set_output_reduced(uint8_t val);   /* 0x2038: 출력 감소 설정 */
void inc_counter_16bit(volatile uint16_t *ptr); /* 0x1BEC: 16bit 카운터 증가 */
void init_blink_flag(void);             /* 0x1C34: 깜박임 플래그 초기화 */
uint8_t check_zero_flag(void);          /* 0x1BF8: 제로 플래그 체크 */
void clear_release_counters(void);      /* 0x1BE0: 릴리스 카운터 초기화 */
void inc_counter_81(void);              /* 0x1C14: counter_81 증가 */
void set_run_flag(void);                /* 0x1C24: g_run_flag = 1 */
void clear_counter_81(void);            /* 0x1C2C: counter_81 = 0 */
void clear_run_flag(void);              /* 0x1C3C: g_run_flag = 0 */

/* 수학 연산 (라이브러리) */
uint32_t mul_32x16(uint16_t a, uint16_t b);   /* 곱셈 */
uint16_t div_32by16(uint32_t num, uint16_t den); /* 나눗셈 */


/* ============================================================================
 * ADC 변환 완료 인터럽트 (벡터 0x0038 → 0x00EA)
 * ============================================================================
 * - 10-bit ADC 결과를 채널별 버퍼에 저장
 * - ADC 채널 처리 함수 호출 (평균화)
 * - 다음 채널로 전환 후 다시 변환 시작
 * ============================================================================ */
ISR(ADC_vect)
{
    uint16_t adc_val;
    uint8_t  ch_idx = adc_ch_idx;

    /* ADC 결과 읽기 (ADCL 먼저 읽어야 함 - 데이터시트 규정) */
    adc_val = ADCL;                           /* in r30, 0x04 */
    adc_val |= ((uint16_t)ADCH << 8);        /* in r31, 0x05 */

    /* 채널별 버퍼에 저장: adc_buffer[ch_idx] = adc_val */
    adc_buffer[ch_idx] = adc_val;

    /* ADC 데이터 처리 (평균화, 스케일링) */
    adc_process_channels();                   /* call 0x196C */

    /* 다음 채널로 전환 */
    adc_ch_idx++;
    if (adc_ch_idx >= 2) {
        adc_ch_idx = 0;
    }

    /* ADMUX 갱신: 상위 비트(REFS) 유지, MUX 채널 변경 */
    /* ADMUX = (ADMUX & 0xC0) | adc_ch_idx                        */
    /* 원본: subi r30, 0xC0 → r30 = ch_idx - 0xC0 (= ch_idx+0x40) */
    /*        out 0x07, r30                                         */
    /*        in r30, 0x07; ori r30, 0xC0; out 0x07, r30           */
    uint8_t admux_val = adc_ch_idx;
    admux_val = (admux_val - 0xC0);  /* + 0x40 effect: REFS0=1 (AVCC) */
    ADMUX = admux_val;
    admux_val = ADMUX;
    admux_val |= 0xC0;              /* REFS1:0 = 11 → 내부 2.56V 또는 AVCC */
    ADMUX = admux_val;

    /* ADC 재시작: ADEN | ADSC | ADIE | prescaler */
    ADCSRA = 0xCF;                            /* out 0x06, 0xCF */
    /* 0xCF = 1100_1111
     *  ADEN=1, ADSC=1, ADATE=0, ADIF=0, ADIE=1, ADPS=111(/128) */
}

/* ============================================================================
 * 버튼 스캔 및 디바운스 (0x0142, ADC ISR 또는 Timer1 ISR에서 호출)
 * ============================================================================
 * - PINC.2, PINC.3 에서 버튼 상태 읽기
 * - 5회 연속 동일 입력 시 확정 (디바운스)
 * - 버튼 코드: 0x32(50) = 긴 누름, 0x0A(10) = 짧은 누름
 * ============================================================================ */
void btn_scan_and_debounce(void)
{
    if (btn_pressed_flag == 1) {
        /* 이미 버튼이 눌린 상태 → 후처리로 점프 */
        goto btn_post_process;
    }

    if (btn_hold_flag != 0) {
        /* 홀드 상태: 양쪽 버튼 모두 릴리스 확인 */
        if (!(PINC & (1 << 2)))   /* PC2 = 0 (pressed) → 아직 누름 */
            goto btn_end;
        if (!(PINC & (1 << 3)))   /* PC3 = 0 (pressed) → 아직 누름 */
            goto btn_end;
        /* 양쪽 다 릴리스됨 */
        if (btn_check_repeat())
            goto btn_exit_release;
        /* 릴리스 확인 완료 */
        btn_release_flag = 0;
        btn_state = 0;
        btn_hold_flag = 0;
        btn_repeat_cnt = 0;
        g_error_flag = 0;
        clear_release_counters();      /* 0x1BE0 */
        goto btn_end;

btn_exit_release:
        goto btn_end;
    }

    /* 버튼 스캔 */
    if (!(PINC & (1 << 3))) {           /* PINC.3 = 0 → 버튼2 눌림 */
        btn_debounce_val = 50;          /* 0x32: 긴 누름 코드 */
        goto btn_check;
    }
    if (!(PINC & (1 << 2))) {           /* PINC.2 = 0 → 버튼1 눌림 */
        btn_debounce_val = 10;          /* 0x0A: 짧은 누름 코드 */
        goto btn_check;
    }

btn_check:
    if (btn_debounce_val == 0)
        goto btn_end;

    if (btn_prev_val != btn_debounce_val) {
        /* 값이 바뀜 → 새로운 디바운스 시작 */
        btn_prev_val = btn_debounce_val;
        btn_repeat_cnt = 0;
        goto btn_end;
    }

    /* 연속 동일 값 감지 → 반복 카운터 체크 */
    if (btn_check_repeat())             /* 0x1BCE: cnt++ → cnt >= 5이면 carry set */
        goto btn_confirmed;

    goto btn_end;

btn_confirmed:
    /* 디바운스 완료 → 버튼 확정 */
    btn_hold_flag = 1;
    btn_confirmed_val = btn_debounce_val;
    btn_pressed_flag = 1;
    btn_repeat_cnt = 0;
    btn_prev_val = 0;
    btn_debounce_val = 0;
    comm_start_sequence(btn_confirmed_val);  /* 0x10F6 → 키 코드에 따른 시퀀스 시작 */
    goto btn_end;

btn_post_process:
    /* 버튼 후처리 (0x0204 이후) */
    btn_repeat_cnt = 0;

    if (btn_state == 1) {
        if (btn_release_flag != 0)
            return;
        /* 릴리스 타이머 증가 */
        inc_counter_16bit((volatile uint16_t *)&btn_debounce_val); /* placeholder */
        /* ... 릴리스 타임아웃 체크 (0x012C = 300) ... */
    }
    return;

btn_end:
    return;
}

/* ============================================================================
 * 버튼 반복 카운터 체크 (0x1BCE)
 * ============================================================================
 * cnt++; return (cnt >= 5) ? 1 : 0;
 * ============================================================================ */
uint8_t btn_check_repeat(void)
{
    btn_repeat_cnt++;
    return (btn_repeat_cnt >= 5) ? 1 : 0;
}

/* ============================================================================
 * Timer0 오버플로우 인터럽트 (벡터 0x0024 → 0x0280)
 * ============================================================================
 * 주기: TCNT0 preload = 0xF0 (240)
 *       오버플로우까지 16 카운트
 *       클럭 = F_CPU/1024
 *       F_CPU=8MHz → 주기 ≈ 16 × 128µs = 2.048ms
 *
 * 기능:
 *   1. 하트비트 LED (PB5) - 약 1초 주기
 *   2. LED 상태 출력 (PC7, PB1, PB0)
 *   3. ADC 스케일링 연산 호출
 *   4. 출력 레벨 처리
 *   5. 디스플레이 멀티플렉싱
 *   6. 통신 시퀀스 관리
 * ============================================================================ */
ISR(TIMER0_OVF_vect)
{
    /* Timer0 리로드 */
    TCNT0 = 0xF0;                             /* out 0x32, 0xF0 */

    /* ---- 하트비트 LED (PB5) ---- */
    inc_counter_16bit(&timer0_tick_cnt);      /* timer0_tick_cnt++ */
    if (timer0_tick_cnt >= 0x03E9) {          /* 1001 counts ≈ 2초 */
        timer0_tick_cnt = 0x044C;             /* ???? 리셋 to 1100 */
        PORTB |= (1 << 5);                   /* PB5 = 1 (LED ON) */
    } else if (timer0_tick_cnt >= 0x01F4) {   /* 500 counts ≈ 1초 */
        if (g_comm_active == 0) {
            comm_sequence_handler();          /* 0x034E */
        }
        PORTB &= ~(1 << 5);                  /* PB5 = 0 (LED OFF) */
    }

    /* ---- 보조 카운터 증가 ---- */
    inc_counter_16bit(&timer0_aux_cnt);

    /* ---- 깜박임 상태 → PC7 LED 제어 ---- */
    if (g_blink_state != 0) {
        PORTC &= ~(1 << 7);                  /* PC7 = 0 (LED ON) */
    } else {
        PORTC |= (1 << 7);                   /* PC7 = 1 (LED OFF) */
    }

    /* ---- g_blink_active → PB1 LED 제어 ---- */
    if (g_blink_active != 0) {
        PORTB &= ~(1 << 1);                  /* PB1 = 0 (LED ON) */
    } else {
        PORTB |= (1 << 1);                   /* PB1 = 1 (LED OFF) */
    }

    /* ---- g_run_flag → PB0 LED 제어 ---- */
    if (g_run_flag != 0) {
        PORTB &= ~(1 << 0);                  /* PB0 = 0 (LED ON) */
    } else {
        PORTB |= (1 << 0);                   /* PB0 = 1 (LED OFF) */
    }

    /* ---- ADC 스케일링 연산 ---- */
    adc_calc_scaled_output();                 /* 0x1A5C */

    /* ---- 출력 레벨 처리 ---- */
    output_level_process();                   /* 0x138C */

    /* ---- 디스플레이 멀티플렉싱 ---- */
    display_multiplex();                      /* 0x1692 */

    /* ---- 메인 상태에 따른 분기 ---- */
    if (g_main_state != 0) {
        output_direct_process();              /* 0x15BC */
    } else {
        /* EEPROM/통신 처리 */
        /* 0x1066 → 0x1BF8 → 0x0C36 */
    }
}

/* ============================================================================
 * Timer1 오버플로우 인터럽트 (벡터 0x0020 → 0x03FE)
 * ============================================================================
 * 주기: TCNT1 preload = 0xFFB1
 *       오버플로우까지 79 카운트
 *       클럭 = F_CPU/1024
 *       F_CPU=8MHz → 주기 ≈ 79 × 128µs ≈ 10.1ms
 *
 * 기능:
 *   1. ADC 결과 기반 디스플레이 갱신 판정
 *   2. 버튼 입력 처리
 *   3. 출력 타이밍 관리
 *   4. PINA.4 (외부 입력) 모니터링
 * ============================================================================ */
ISR(TIMER1_OVF_vect)
{
    /* Timer1 리로드 */
    TCNT1H = 0xFF;                            /* out 0x2D, 0xFF */
    TCNT1L = 0xB1;                            /* out 0x2C, 0xB1 */

    /* ---- 동작 모드에 따른 분기 ---- */
    if (g_main_state != 0)
    {
        /* --- 모드 1: Timer1 틱 카운터 증가 --- */
        inc_counter_16bit(&timer1_tick_cnt);
    }
    else
    {
        /* --- 모드 0: 전원/모드 처리 --- */
        /* 0x06D2 관련 처리 */
        /* 전원 상태 체크 및 버튼 처리 */
        btn_scan_and_debounce();              /* 0x0142 */
        /* ... 추가 처리 ... */
    }

    /* ---- 깜박임 주기 관리 ---- */
    if (g_blink_phase == 0) {
        /* 깜박임 OFF 상태 */
    } else {
        /* 깜박임 활성 → 타이밍에 따라 ON/OFF 전환 */
        if (g_output_enable != 0) {
            inc_counter_16bit(&timer0_process_cnt);
            if (timer0_process_cnt >= 11) {
                /* 일정 시간 경과 후 깜박임 OFF */
                g_output_enable = 0;
                timer0_process_cnt = 0;
                g_blink_phase = 1;
                g_blink_active = 1;
            }
        } else if (g_blink_phase == 1) {
            inc_counter_16bit(&timer0_process_cnt);
            if (timer0_process_cnt >= 4) {
                g_output_enable = 0;
                timer0_process_cnt = 0;
                g_blink_phase = 0;
                g_blink_active = 0;
            }
        }
    }

    /* ---- ADC ch1 평균값 기반 디스플레이 갱신 ---- */
    if (adc_ch1_avg >= 0x0321) {              /* 801 이상 */
        /* 디스플레이/출력 리셋 */
        g_display_raw = 0;
        g_output_counter = 0;
    } else {
        /* PWM 위상 체크 */
        uint8_t pwm_val = g_pwm_source;
        pwm_val &= 0x04;
        /* ... 위상 비교 및 출력 카운터 관리 ... */
        if (g_output_counter >= 0x1A) {       /* 26회 이상 */
            g_output_enable = 1;
            g_output_counter = 0;
        }
    }

    /* ---- 디스플레이 ON/OFF 관리 ---- */
    if (g_output_enable != 0)
    {
        if (g_display_raw == 0) {
            /* 디스플레이 데이터 없음 */
            g_display_raw = 0;
        } else if (g_display_enable == 0) {
            /* 디스플레이 시작 */
            g_display_enable = 1;
            g_display_period = adc_ch1_avg;
        } else {
            /* PINA.4 모니터링 */
            if (!(PINA & (1 << 4))) {
                /* PA4 = 0 → 입력 감지 */
                g_display_off_cnt = 0;
                if (g_display_period != 0) {
                    g_display_period--;
                } else {
                    /* 주기 완료 → 디스플레이 리셋 */
                    g_display_enable = 0;
                    g_blink_state = 0;
                    g_display_raw = 0;
                }
            } else {
                /* PA4 = 1 → OFF 카운터 증가 */
                g_display_off_cnt++;
                if (g_display_off_cnt >= 6) {
                    /* 타임아웃 → 리셋 */
                    g_display_enable = 0;
                    g_display_off_cnt = 0;
                    g_blink_state = 0;
                    g_display_raw = 0;
                }
            }
        }
    }
}

/* ============================================================================
 * ADC 채널 데이터 처리 (0x196C)
 * ============================================================================
 * - 채널 0: 10 샘플 누적 후 평균 → adc_ch0_avg
 * - 채널 1: 50 샘플 누적 후 평균 → adc_ch1_avg
 * ============================================================================ */
void adc_process_channels(void)
{
    if (adc_ch_idx == 0)
    {
        /* ==== 채널 0 처리 ==== */
        uint16_t raw = adc_buffer[0];

        /* 32비트 누적 */
        /* 여기서 0x20BA 함수: 입력값을 스케일링 변수에 저장 */
        /* 0x20D0 함수: 누적 처리 */
        /* 0x20E2 함수: 확장 가산 */
        adc_ch0_sum_raw += raw;
        /* (실제로는 32비트 확장 덧셈) */

        /* 샘플 카운터 증가 */
        inc_counter_16bit(&adc_ch0_sample_cnt);
        if (adc_ch0_sample_cnt < 10)          /* 10 샘플 미만 → 계속 누적 */
            return;

        /* 10 샘플 평균 계산 */
        /* 0x20D0: 누적값 읽기 */
        /* 0x20F8: 32비트 확장 읽기 */
        /* 0x211A: 나눗셈 (÷10) */
        adc_ch0_avg = adc_ch0_sum_raw / 10;

        /* 누적 초기화 */
        adc_ch0_sum_raw = 0;
        adc_ch0_sum_ext = 0;
        adc_ch0_sample_cnt = 0;
    }
    else
    {
        /* ==== 채널 1 처리 ==== */
        uint16_t raw = adc_buffer[1];

        /* 32비트 누적 */
        adc_ch1_sum_raw += raw;

        /* 샘플 카운터 증가 */
        inc_counter_16bit(&adc_ch1_sample_cnt);
        if (adc_ch1_sample_cnt < 50)          /* 50 샘플 미만 → 계속 누적 */
            return;

        /* 50 샘플 평균 계산 */
        adc_ch1_avg = adc_ch1_sum_raw / 50;

        /* 누적 초기화 */
        adc_ch1_sum_raw = 0;
        adc_ch1_sum_ext = 0;
        adc_ch1_sample_cnt = 0;
    }
}

/* ============================================================================
 * ADC 스케일링 연산 (0x1A5C)
 * ============================================================================
 * timer0_process_cnt가 11 이상이면 실행
 * 룩업 테이블로 adc_ch0_avg를 스케일링 → adc_scaled_value
 * 결과를 0x03E8(1000) 이하로 클리핑
 * ============================================================================ */
void adc_calc_scaled_output(void)
{
    if (timer0_process_cnt < 11)
        return;

    timer0_process_cnt = 11;  /* 클리핑 */
    adc_ch0_result = adc_ch0_avg;

    /* 룩업 테이블 기반 스케일링 (0x213A) */
    uint32_t temp = (uint32_t)adc_ch0_result;
    /* ... 곱셈/나눗셈 연산 ... */

    /* 최대값 클리핑 */
    if (temp >= 0x03E8) {
        adc_scaled_value = 0x03E8;            /* 최대 1000 */
        return;
    }
    /* 최소값 체크 */
    if (temp < 3) {
        adc_scaled_value = 0;
        return;
    }
    /* 정상 범위: 비트 시프트로 스케일 조정 */
    adc_scaled_value = (uint16_t)(temp >> 6);
}

/* ============================================================================
 * 메인 루프 처리 (0x1226)
 * ============================================================================
 * ADC 평균값(timer1_tick_cnt)을 10단계 임계값과 비교하여
 * LED 출력 패턴(disp_led_pattern)과 PORTC.6을 설정
 *
 * 임계값 (ADC 단위)  |  패턴값  |  PORTD 출력  |  PC6
 * ──────────────────|─────────|─────────────|──────
 *   < 0x0029 (41)   |  0x01   |  0x04       |  SET
 *   < 0x0051 (81)   |  0x03   |  0x0C       |  -
 *   < 0x0079 (121)  |  0x07   |  0x1C       |  CLR
 *   < 0x00A1 (161)  |  0x0F   |  0x3C       |  -
 *   < 0x00C9 (201)  |  0x1F   |  0x7C       |  -
 *   < 0x00F1 (241)  |  0x3F   |  0xFC       |  -
 *   < 0x0119 (281)  |  0x7F   |  0xFC+0x01  |  -
 *   < 0x0141 (321)  |  0xFF   |  0xFC       |  -
 *   < 0x0169 (361)  |  0xFF   |  0xFD+0x07  |  -
 *   < 0x0191 (401)  |  (계산) |  (계산)     |  -
 *   >= 0x0191       |  0x00   |  종료       |  -
 * ============================================================================ */
void main_loop_process(void)
{
    uint16_t val;

    timer1_tick_cnt = 0;

    while (1)
    {
        val = timer1_tick_cnt;

        if (val < 0x0029) {               /* < 41 */
            disp_led_pattern = 0x01;
            set_output_mode_a(0x04);
            PORTC |= (1 << 6);           /* PC6 = 1 */
            continue;
        }
        if (val < 0x0051) {               /* < 81 */
            disp_led_pattern = 0x03;
            set_output_mode_b(0x0C);
            continue;
        }
        if (val < 0x0079) {               /* < 121 */
            disp_led_pattern = 0x07;
            set_output_mode_a(0x1C);
            PORTC &= ~(1 << 6);          /* PC6 = 0 */
            continue;
        }
        if (val < 0x00A1) {               /* < 161 */
            disp_led_pattern = 0x0F;
            set_output_mode_b(0x3C);
            continue;
        }
        if (val < 0x00C9) {               /* < 201 */
            disp_led_pattern = 0x1F;
            set_output_mode_a(0x7C);
            continue;
        }
        if (val < 0x00F1) {               /* < 241 */
            disp_led_pattern = 0x3F;
            set_output_mode_b(0xFC);
            continue;
        }
        if (val < 0x0119) {               /* < 281 */
            disp_led_pattern = 0x7F;
            disp_digit0_data = 0xFC;
            disp_digit1_data = 0x01;
            continue;
        }
        if (val < 0x0141) {               /* < 321 */
            disp_led_pattern = 0xFF;
            set_output_mode_c(0xFC);
            continue;
        }
        if (val < 0x0169) {               /* < 361 */
            disp_led_pattern = 0xFF;
            disp_digit0_data = 0xFD;
            disp_digit1_data = 0x07;
            continue;
        }
        if (val < 0x0191) {               /* < 401 */
            /* 계산된 출력 */
            set_output_full();
            /* 추가 비트 연산으로 출력 결정 */
            continue;
        }

        /* >= 401: 범위 초과 → 정지 */
        g_main_state = 0;
        break;
    }
}

/* ============================================================================
 * 디스플레이 멀티플렉싱 (0x1692)
 * ============================================================================
 * 3-digit 7세그먼트 LED를 시분할 구동
 *
 *   DDRD = 0xFF → 전체 출력, 세그먼트 데이터
 *   PB2: Digit 0 선택 (active low)
 *   PB3: Digit 1 선택 (active low)
 *   PB4: Digit 2 선택 (active low)
 *
 *   구동 순서:
 *     1) DDRD = 0xFF (출력 확인)
 *     2) PORTD = 0xFF (모든 세그먼트 OFF)
 *     3) PB2,3,4 = 1 (모든 digit OFF)
 *     4) 현재 digit 세그먼트 데이터를 PORTD에 출력
 *     5) 해당 digit 선택핀 = 0 (active low)
 *     6) digit 인덱스 순환 (0→1→2→0)
 * ============================================================================ */
void display_multiplex(void)
{
    /* 모든 세그먼트 OFF (블랭킹) */
    DDRD  = 0xFF;                             /* out 0x11, 0xFF */
    /* 짧은 지연 (7 사이클) */
    asm volatile("nop\nnop\nnop\nnop\nnop\nnop\nnop\n");
    PORTD = 0xFF;                             /* out 0x12, 0xFF (all OFF) */

    /* 모든 digit 선택 해제 */
    PORTB |= (1 << 2);                       /* PB2 = 1 (Digit0 OFF) */
    PORTB |= (1 << 3);                       /* PB3 = 1 (Digit1 OFF) */
    PORTB |= (1 << 4);                       /* PB4 = 1 (Digit2 OFF) */

    /* 현재 digit에 따라 세그먼트 데이터 출력 */
    switch (disp_digit_idx)
    {
        case 0:
            /* Digit 0: disp_led_pattern 기반 세그먼트 출력 */
            display_set_segments(disp_led_pattern | 0x01);
            PORTB &= ~(1 << 2);              /* PB2 = 0 (Digit0 ON) */
            break;

        case 1:
            /* Digit 1: disp_digit0_data 기반 */
            display_set_segments(disp_digit0_data);
            PORTB &= ~(1 << 3);              /* PB3 = 0 (Digit1 ON) */
            break;

        case 2:
            /* Digit 2: disp_digit1_data 기반 (통신 상태 포함) */
            /* check_zero_flag, g_comm_flag 등 복합 조건 */
            display_set_segments(disp_digit1_data);
            PORTB &= ~(1 << 4);              /* PB4 = 0 (Digit2 ON) */
            break;
    }

    /* digit 인덱스 순환 */
    disp_digit_idx++;
    if (disp_digit_idx >= 3) {
        disp_digit_idx = 0;
    }
}

/* ============================================================================
 * 세그먼트 데이터 출력 (0x2064)
 * ============================================================================
 * 입력 바이트의 각 비트를 반전하여 PORTD에 출력
 * (공통 양극 7-세그먼트: 0=ON, 1=OFF)
 * ============================================================================ */
void display_set_segments(uint8_t val)
{
    /* 비트 반전 변환 (0x16F6) */
    /* 각 비트를 개별적으로 반전하여 disp_seg_data에 저장 */
    uint8_t result = 0;
    for (uint8_t i = 0; i < 8; i++) {
        if (val & (1 << i))
            result &= ~(1 << i);   /* 해당 비트 = 0 (세그먼트 ON) */
        else
            result |= (1 << i);    /* 해당 비트 = 1 (세그먼트 OFF) */
    }
    disp_seg_data = result;

    /* PORTD에 출력 */
    PORTD = disp_seg_data;                    /* out 0x12, disp_seg_data */
}

/* ============================================================================
 * 출력 레벨 처리 (0x138C) - 룩업 테이블 기반
 * ============================================================================
 * g_main_state = 0 일 때 실행
 * 프로그램 메모리의 룩업 테이블(21 entries)을 스택에 복사하고,
 * 각 엔트리를 adc_scaled_value와 비교하여 출력 레벨 결정
 * ============================================================================ */
void output_level_process(void)
{
    if (g_main_state != 0)
        return;

    /* 룩업 테이블을 스택에 복사 (21 entries × 2 bytes = 42 bytes) */
    uint16_t table[21];
    for (uint8_t i = 0; i < 21; i++) {
        table[i] = pgm_read_word(&lookup_table[i]);
    }

    /* 테이블 순서대로 비교: table[i] > adc_scaled_value 이면 해당 레벨 */
    for (uint8_t i = 0; i < 21; i++) {
        if (table[i] > adc_scaled_value) {
            /* 해당 레벨에 맞는 출력 설정 */
            switch (i) {
                case 0: case 1: case 2: case 3:
                    set_output_full();
                    set_output_mode_c(0xFC);
                    break;
                case 4:
                    set_output_full();
                    disp_digit0_data = table[i] & 0xFF;
                    disp_digit1_data = 0x01;
                    break;
                case 5:
                    set_output_full();
                    set_output_mode_b(table[i] & 0xFF);
                    break;
                case 6:
                    disp_led_pattern = 0xFF;
                    set_output_mode_b(0x7F);
                    break;
                case 7:
                    disp_led_pattern = 0xFF;
                    set_output_mode_b(0x3F);
                    break;
                case 8:
                    disp_led_pattern = 0xFF;
                    set_output_mode_b(0x1F);
                    break;
                case 9:
                    disp_led_pattern = 0xFF;
                    set_output_mode_b(0x0F);
                    break;
                case 10:
                    disp_led_pattern = 0xFF;
                    set_output_mode_b(0x07);
                    break;
                case 11:
                    disp_led_pattern = 0xFF;
                    set_output_mode_b(0x03);
                    break;
                case 12:
                    disp_led_pattern = 0xFF;
                    set_output_mode_b(0x01);
                    break;
                case 13:
                    disp_led_pattern = 0xFF;
                    set_output_reduced(0xFF);
                    break;
                case 14:
                    disp_led_pattern = 0x7F;
                    set_output_reduced(0x7F);
                    break;
                case 15:
                    disp_led_pattern = 0x3F;
                    set_output_reduced(0x3F);
                    break;
                case 16:
                    disp_led_pattern = 0x1F;
                    set_output_reduced(0x1F);
                    break;
                case 17:
                    disp_led_pattern = 0x0F;
                    set_output_reduced(0x0F);
                    break;
                case 18:
                    disp_led_pattern = 0x07;
                    set_output_reduced(0x07);
                    break;
                case 19:
                    disp_led_pattern = 0x03;
                    set_output_reduced(0x03);
                    break;
                case 20:
                    disp_led_pattern = 0x01;
                    set_output_reduced(0x01);
                    break;
                default:
                    disp_led_pattern = 0x00;
                    set_output_reduced(0x00);
                    break;
            }
            return;
        }
    }
    /* 모든 테이블 값 초과 → 최소 출력 */
    disp_led_pattern = 0x00;
    set_output_reduced(0x00);
}

/* ============================================================================
 * 유틸리티 함수들
 * ============================================================================ */

/* 16비트 카운터 증가 (0x1BEC) */
void inc_counter_16bit(volatile uint16_t *ptr)
{
    (*ptr)++;
}

/* 깜박임 플래그 초기화 (0x1C34) */
void init_blink_flag(void)
{
    g_blink_active = 0;
}

/* 제로 체크 (0x1BF8) */
uint8_t check_zero_flag(void)
{
    /* 0x21B6 함수 호출 → r30 비교 후 반환 */
    return 0;  /* placeholder */
}

/* 릴리스 카운터 초기화 (0x1BE0) */
void clear_release_counters(void)
{
    /* 0x016A, 0x016B = 0 */
    /* (btn 관련 카운터 초기화) */
}

/* counter_81 증가 (0x1C14) */
void inc_counter_81(void)
{
    g_counter_81++;
}

/* g_run_flag = 1 (0x1C24) */
void set_run_flag(void)
{
    g_run_flag = 1;
}

/* counter_81 = 0 (0x1C2C) */
void clear_counter_81(void)
{
    g_counter_81 = 0;
}

/* g_run_flag = 0 (0x1C3C) */
void clear_run_flag(void)
{
    g_run_flag = 0;
}

/* 출력 모드 A 설정 (0x1FFE) */
void set_output_mode_a(uint8_t val)
{
    disp_digit0_data = val;
    disp_digit1_data = 0x30;
}

/* 출력 모드 B 설정 (0x200A) */
void set_output_mode_b(uint8_t val)
{
    disp_digit0_data = val;
    disp_digit1_data = 0x00;
}

/* 출력 모드 C 설정 (0x2016) */
void set_output_mode_c(uint8_t val)
{
    disp_digit0_data = val;
    disp_digit1_data = 0x03;
}

/* 출력 최대 설정 (0x2022) */
void set_output_full(void)
{
    disp_led_pattern = 0xFF;
}

/* 스케일 값 비교 (0x202A) */
/* adc_scaled_value와 비교, carry flag 반환 */
uint8_t compare_with_scaled(uint16_t val)
{
    return (val < adc_scaled_value) ? 1 : 0;
}

/* 출력 감소 설정 (0x2038) */
void set_output_reduced(uint8_t val)
{
    disp_led_pattern = val;
    set_output_mode_b(0x00);
}

/* ============================================================================
 * 통신 시퀀스 처리 (0x034E)
 * ============================================================================
 * 상태 머신 기반 통신 시퀀스
 * g_comm_done → g_comm_step → g_comm_seq_state 순서로 진행
 * ============================================================================ */
void comm_sequence_handler(void)
{
    if (g_comm_done != 0)
        return;

    if (g_comm_step == 0) {
        /* 단계 0: 카운터 증가, 6 도달 시 다음 단계 */
        inc_counter_81();
        if (g_counter_81 < 6)
            return;
        set_run_flag();
        g_comm_step = g_run_flag;
        clear_counter_81();
        g_comm_seq_state = g_counter_81;    /* = 0 */
    }
    else if (g_comm_seq_state == 0) {
        /* 상태 0: 카운터 21 대기 */
        inc_counter_81();
        if (g_counter_81 < 0x15)            /* 21 */
            return;
        clear_counter_81();
        g_run_flag = 0;
        g_comm_seq_state = 1;
    }
    else if (g_comm_seq_state == 1) {
        /* 상태 1: 카운터 1 대기 */
        inc_counter_81();
        if (g_counter_81 < 1)
            return;
        init_blink_flag();
        g_counter_81 = 0;
        g_comm_seq_state = 2;
    }
    else if (g_comm_seq_state == 2) {
        /* 상태 2: 시퀀스 완료 */
        clear_run_flag();
        /* g_blink_state 설정 및 타이머 체크 */
        inc_counter_81();
        if (g_counter_81 < 11)
            return;
        /* 시퀀스 완료 → 플래그 설정 */
        clear_counter_81();
        g_comm_seq_state = 0;
        g_comm_active = 1;
        g_comm_done = 1;
    }
}

/* ============================================================================
 * 시퀀스 시작 (0x10F6)
 * ============================================================================ */
void comm_start_sequence(uint8_t code)
{
    /* 버튼 코드에 따른 시퀀스 시작 처리 */
    /* ... */
}

/* ============================================================================
 * 하드웨어 초기화 (0x1AD2)
 * ============================================================================
 * ATmega16 데이터시트 기준 정확한 레지스터 설정
 * ============================================================================ */
void hw_init(void)
{
    /* ========== 포트 초기화 ========== */

    /* 포트A: ADC 입력 (PA0,PA1 사용) */
    DDRA  = 0x00;      /* out 0x1A, 0x00  → 전체 입력 */
    PORTA = 0xFC;      /* out 0x1B, 0xFC  → PA2~PA7 풀업 (PA0,PA1 = ADC 무풀업) */

    /* 포트B: LED/디스플레이 제어 출력 */
    DDRB  = 0xFF;      /* out 0x17, 0xFF  → 전체 출력 */
    PORTB = 0xDF;      /* out 0x18, 0xDF  → PB5=0(LED OFF), 나머지=1 */
    /* 0xDF = 1101_1111
     *  PB7=1, PB6=1, PB5=0, PB4=1, PB3=1, PB2=1, PB1=1, PB0=1 */

    /* 포트C: 혼합 (PC0,1,6,7=출력 / PC2~5=입력) */
    DDRC  = 0xC3;      /* out 0x14, 0xC3  → 1100_0011 */
    PORTC = 0xBC;      /* out 0x15, 0xBC  → 1011_1100 (입력핀 풀업) */
    /* PC7=1(out), PC6=0(out), PC5=1(in풀업), PC4=1(in풀업),
     * PC3=1(in풀업), PC2=1(in풀업), PC1=1(out), PC0=1(out) */

    /* 포트D: 7-세그먼트 디스플레이 데이터 */
    DDRD  = 0xFF;      /* out 0x11, 0xFF  → 전체 출력 */
    PORTD = 0xFF;      /* out 0x12, 0xFF  → 전체 HIGH (세그먼트 OFF) */

    /* ========== 타이머 초기화 ========== */

    /* ASSR: 비동기 동작 비활성 */
    ASSR = 0x00;       /* out 0x22, 0x00 */

    /* --- Timer0: Normal 모드, Prescaler /1024 --- */
    TCCR0 = 0x05;      /* out 0x33, 0x05
                         * 0000_0101: CS02:0 = 101 → clk/1024 */
    TCNT0 = 0x9B;      /* out 0x32, 0x9B  → preload 155
                         * 오버플로우까지: 256-155 = 101 카운트
                         * F_CPU=8MHz: 101 × 128µs ≈ 12.9ms */
    OCR0  = 0x00;      /* out 0x3C, 0x00 */

    /* --- Timer1: Normal 모드, Prescaler /1024 --- */
    TCCR1A = 0x00;     /* out 0x2F, 0x00  → Normal 모드 */
    TCCR1B = 0x05;     /* out 0x2E, 0x05  → clk/1024 */
    TCNT1H = 0xFF;     /* out 0x2D, 0xFF */
    TCNT1L = 0xB1;     /* out 0x2C, 0xB1  → preload 0xFFB1 = 65457
                         * 오버플로우까지: 65536-65457 = 79 카운트
                         * F_CPU=8MHz: 79 × 128µs ≈ 10.1ms */
    OCR1AH = 0x00;     /* out 0x2B, 0x00 */
    OCR1AL = 0x00;     /* out 0x2A, 0x00 */
    OCR1BH = 0x00;     /* out 0x29, 0x00 */
    OCR1BL = 0x00;     /* out 0x28, 0x00 */

    /* --- Timer2: 비활성 --- */
    TCCR2 = 0x00;      /* out 0x25, 0x00 */
    TCNT2 = 0x00;      /* out 0x24, 0x00 */
    OCR2  = 0x00;      /* out 0x23, 0x00 */

    /* ========== 인터럽트 설정 ========== */

    /* Timer 인터럽트 마스크 */
    TIMSK = 0x05;       /* out 0x39, 0x05
                         * 0000_0101: TOIE1=1, TOIE0=1
                         * Timer1 OVF + Timer0 OVF 인터럽트 활성 */

    /* 외부 인터럽트: 읽고 그대로 다시 쓰기 (변경 없음) */
    {
        uint8_t tmp = GICR;
        GICR = tmp;     /* in+out 0x3B */
    }

    /* MCU 제어 */
    MCUCR  = 0x00;      /* out 0x35, 0x00 */
    MCUCSR = 0x00;      /* out 0x34, 0x00 */
    GIFR   = 0x00;      /* out 0x3A, 0x00 */

    /* ========== 통신 모듈 비활성 ========== */

    /* USART: 완전 비활성 */
    UCSRA = 0x00;       /* out 0x0B, 0x00 */
    UCSRB = 0x00;       /* out 0x0A, 0x00 */
    UBRRH = 0x00;       /* out 0x20, 0x00 (URSEL=0 → UBRRH) */
    UCSRC = 0x00;       /* out 0x20, 0x00 */
    UBRRL = 0x00;       /* out 0x09, 0x00 */

    /* SPI: 비활성 */
    SPCR = 0x00;        /* out 0x0D, 0x00 */
    SPSR = 0x00;        /* out 0x0E, 0x00 */

    /* ========== 아날로그 비교기 비활성 ========== */
    ACSR = 0x80;        /* out 0x08, 0x80
                         * ACD=1: 아날로그 비교기 전원 OFF (소비전류 절감) */

    /* ========== 특수 기능 I/O ========== */
    SFIOR = 0x00;       /* out 0x30, 0x00 */

    /* ========== ADC 초기화 ========== */

    /* ADMUX: AVCC 기준전압, 채널 0 */
    ADMUX = 0x40;       /* out 0x07, 0x40
                         * REFS1:0 = 01 → AVCC (외부 AREF 캡에 연결)
                         * ADLAR = 0 → 우측 정렬
                         * MUX4:0 = 00000 → ADC0 (PA0) */

    /* ADCSRA: ADC 활성, 변환 시작, 인터럽트 활성, Prescaler /32 */
    ADCSRA = 0xCD;      /* out 0x06, 0xCD
                         * 1100_1101:
                         *  ADEN  = 1 (ADC 활성)
                         *  ADSC  = 1 (변환 시작)
                         *  ADATE = 0 (자동 트리거 비활성)
                         *  ADIF  = 0
                         *  ADIE  = 1 (인터럽트 활성)
                         *  ADPS2:0 = 101 → Prescaler /32
                         *  F_CPU=8MHz: ADC클럭 = 250kHz (권장 50~200kHz 범위 초과) */

    /* EEPROM 관련 체크 */
    if (check_zero_flag()) {
        /* EEPROM에서 설정값 읽기 */
        /* ... */
    }
}

/* ============================================================================
 * main() - 프로그램 진입점 (0x1B62)
 * ============================================================================ */
int main(void)
{
    /* 하드웨어 초기화 */
    hw_init();                            /* call 0x1AD2 */

    /* PB5 = 0 (하트비트 LED 초기 OFF) */
    PORTB &= ~(1 << 5);                  /* cbi 0x18, 5 */

    /* 인터럽트 비활성 (초기 설정 중) */
    cli();                                /* 0x1B68 */

    /* 전역 변수 초기화 */
    g_run_flag = 0;                       /* 0x0196 = 0 */
    init_blink_flag();                    /* g_blink_active = 0 */
    g_blink_state = 0;                    /* 0x0189 = r30 (from init_blink_flag) */

    /* 포트C: 부가 출력 초기 OFF */
    PORTC &= ~(1 << 1);                  /* cbi 0x15, 1 */
    PORTC &= ~(1 << 0);                  /* cbi 0x15, 0 */
    PORTC &= ~(1 << 6);                  /* cbi 0x15, 6 */

    /* PORTD: 전체 HIGH (세그먼트 OFF) */
    PORTD = 0xFF;                         /* out 0x12, 0xFF */

    /* PB2,3,4 = 0 (디스플레이 digit 선택 OFF) */
    PORTB &= ~(1 << 2);                  /* cbi 0x18, 2 */
    PORTB &= ~(1 << 3);                  /* cbi 0x18, 3 */
    PORTB &= ~(1 << 4);                  /* cbi 0x18, 4 */

    /* 메인 상태 = 1 (정상 동작 모드) */
    g_main_state = 1;                     /* 0x0195 = 1 */

    /* 인터럽트 활성화 → ADC, Timer0, Timer1 동작 시작 */
    sei();                                /* 0x1B8E */

    /* ===== 메인 루프 ===== */
    while (1)
    {
        main_loop_process();              /* call 0x1226 */
        /* 0x1B94: rjmp -2 → 무한 루프 */
    }

    return 0;  /* 도달 불가 */
}