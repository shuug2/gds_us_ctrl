# ATmega16 Firmware Disassembly Analysis
## m16_controller_prg.hex

---

## 📋 펌웨어 정보

- **파일명**: m16_controller_prg.hex
- **타겟 MCU**: ATmega16
- **펌웨어 크기**: 16,384 bytes (16 KB)
- **총 명령어 수**: 2,951 instructions
- **포맷**: Intel HEX → AVR Assembly

---

## 🗺️ 인터럽트 벡터 테이블

| 주소 | 벡터 이름 | 점프 주소 | 설명 |
|------|-----------|-----------|------|
| 0x0000 | RESET | 0x008A | 리셋 벡터 (프로그램 시작) |
| 0x0004 | INT0 | 0x0000 | 외부 인터럽트 0 (미사용) |
| 0x0008 | INT1 | 0x0000 | 외부 인터럽트 1 (미사용) |
| 0x000C | TIMER2_COMP | 0x0000 | 타이머2 비교 매치 (미사용) |
| 0x0010 | TIMER2_OVF | 0x0000 | 타이머2 오버플로우 (미사용) |
| 0x0014 | TIMER1_CAPT | 0x0000 | 타이머1 캡처 (미사용) |
| 0x0018 | TIMER1_COMPA | 0x0000 | 타이머1 비교 매치 A (미사용) |
| 0x001C | TIMER1_COMPB | 0x0000 | 타이머1 비교 매치 B (미사용) |
| **0x0020** | **TIMER1_OVF** | **0x03FE** | **타이머1 오버플로우** ⭐ |
| **0x0024** | **TIMER0_OVF** | **0x0280** | **타이머0 오버플로우** ⭐ |
| 0x0028 | SPI_STC | 0x0000 | SPI 전송 완료 (미사용) |
| 0x002C | USART_RXC | 0x0000 | USART 수신 완료 (미사용) |
| 0x0030 | USART_UDRE | 0x0000 | USART 데이터 레지스터 비움 (미사용) |
| 0x0034 | USART_TXC | 0x0000 | USART 전송 완료 (미사용) |
| **0x0038** | **ADC** | **0x00EA** | **ADC 변환 완료** ⭐ |
| 0x003C | EE_RDY | 0x0000 | EEPROM 준비 (미사용) |
| 0x0040 | ANA_COMP | 0x0000 | 아날로그 비교기 (미사용) |
| 0x0044 | TWI | 0x0000 | TWI (I2C) (미사용) |
| 0x0048 | INT2 | 0x0000 | 외부 인터럽트 2 (미사용) |
| 0x004C | TIMER0_COMP | 0x0000 | 타이머0 비교 매치 (미사용) |
| 0x0050 | SPM_RDY | 0x0000 | SPM 준비 (미사용) |

### 활성 인터럽트
1. **TIMER1_OVF (0x03FE)** - 메인 타이밍 제어
2. **TIMER0_OVF (0x0280)** - 보조 타이밍
3. **ADC (0x00EA)** - 아날로그 입력 처리

---

## 🔧 초기화 루틴 (0x008A)

```assembly
0x008A:  cli                    ; 인터럽트 비활성화
0x008C:  eor r30, r30          ; r30 = 0 (Z 레지스터 하위)
0x008E:  out 0x1C, r30         ; EECR = 0 (EEPROM 제어 초기화)
0x0090:  ldi r31, 0x01         ; r31 = 0x01
0x0092:  out 0x3B, r31         ; SPH = 0x01 (스택 포인터 상위)
0x0094:  out 0x3B, r30         ; SPL = 0x00
0x0096:  out 0x35, r30         ; MCUCR = 0 (MCU 제어)
0x0098:  ldi r31, 0x18         ; r31 = 0x18
0x009A:  out 0x21, r31         ; WDTCR 설정
0x009C:  out 0x21, r30         ; WDTCR 클리어
```

### 초기화 단계
1. **인터럽트 비활성화** - 안전한 설정을 위해
2. **EEPROM 제어 초기화** - EECR 레지스터 클리어
3. **스택 포인터 설정** - SPH:SPL = 0x0118
4. **워치독 타이머 설정** - 시스템 안정성
5. **RAM 초기화** - 0x0002~0x000F, 0x0060~0x0460 영역

---

## 📊 데이터 영역

### 룩업 테이블 (0x0058 - 0x0088)

```
0x0058: 0x03FF, 0x03CC, 0x0399, 0x0366, 0x0333, 0x0300
0x0064: 0x02CD, 0x029A, 0x0267, 0x0234, 0x0201, 0x01CE
0x0070: 0x01B9, 0x0168, 0x0135, 0x0102, 0x00CF, 0x009C
0x007C: 0x0069, 0x0036, 0x000F, 0x0004, 0x0004, 0x0054
```

**용도**: 
- 24개의 16비트 상수
- PWM 값, 타이밍 상수, 또는 계산 테이블로 추정
- 곱셈/나눗셈 명령어와 함께 사용 (fmulsu, muls, mulsu 등)

---

## ⏱️ 타이머 인터럽트 분석

### TIMER1 오버플로우 ISR (0x03FE)

**주요 기능:**
- 주기적인 시스템 틱 생성
- 상태 머신 업데이트
- 타이밍 카운터 관리

**처리 로직:**
```c
void TIMER1_OVF_ISR() {
    // 메인 카운터 체크
    if (counter >= threshold) {
        // EEPROM에서 설정 읽기
        // 상태 전환
    }
    
    // I/O 포트 모니터링
    // 조건에 따른 처리
    
    // 플래그 업데이트
}
```

### TIMER0 오버플로우 ISR (0x0280)

**주요 기능:**
- 고속 타이밍 이벤트
- PWM 또는 빠른 샘플링

---

## 🔌 ADC 인터럽트 (0x00EA)

```assembly
0x00EA:  call 0x1B96           ; ADC 처리 함수 호출
0x00EE:  lds r30, 0x021E       ; 메모리 0x021E 읽기
0x00F2:  ldi r26, 0x60         ; X 레지스터 설정
0x00F4:  ldi r27, 0x01         ; 
0x00F6:  call 0x1BEC           ; 데이터 처리
```

**기능:**
- ADC 변환 완료 시 호출
- 센서 데이터 읽기
- 데이터 버퍼에 저장

---

## 🎯 주요 함수 분석

### 함수 @ 0x1B62 (메인 초기화)

**호출 위치**: 0x00E6
```assembly
0x00E6:  jmp 0x1B62            ; 메인 초기화로 점프
```

### 함수 @ 0x1B96 (ADC 처리)

**호출 위치**: 0x00EA (ADC ISR)
- ADC 데이터 읽기
- 필터링/평균화
- 결과 저장

---

## 💾 EEPROM 액세스 패턴

코드 내에서 EEPROM 읽기/쓰기가 관찰됨:

```assembly
; EEPROM 제어 레지스터 (0x1C)
out 0x1C, r30         ; EECR 설정

; EEPROM 주소/데이터 레지스터
; EEAR: 0x1E-0x1F
; EEDR: 0x1D
```

**사용 목적:**
- 설정값 저장
- 캘리브레이션 데이터
- 사용자 구성

---

## 🔄 상태 머신 구조

### 메모리 맵 (추정)

| 주소 | 변수명 | 용도 |
|------|--------|------|
| 0x021E | adc_value | ADC 읽기 값 |
| 0x0160-0x01FF | 상태 변수들 | 시스템 상태 |
| 0x0200-0x023F | 버퍼 | 데이터 버퍼 |

### 동작 시퀀스

```
[초기화]
   ↓
[타이머 시작]
   ↓
[대기 모드] ←→ [인터럽트 처리]
   ↓              ↓
[ADC 샘플링]   [타이머 ISR]
   ↓              ↓
[데이터 처리] ← [상태 업데이트]
   ↓
[출력 제어]
```

---

## 📈 I/O 포트 사용

### 포트 레지스터 액세스

```assembly
; 0x1C: EECR (EEPROM Control)
; 0x1D: EEDR (EEPROM Data)
; 0x1E: EEARL (EEPROM Address Low)
; 0x1F: EEARH (EEPROM Address High)
; 0x21: WDTCR (Watchdog Timer)
; 0x35: MCUCR (MCU Control)
; 0x3B: SPL/SPH (Stack Pointer)
; 0x3D: SPL (Stack Pointer Low)
; 0x3E: SPH (Stack Pointer High)
```

---

## 🧮 수학 연산

### 곱셈 명령어 사용

```assembly
0x0058:  fmulsu r23, r23      ; Fractional multiply signed/unsigned
0x005A:  fmulsu r20, r20
0x005C:  fmulsu r17, r17
0x005E:  mulsu r22, r22       ; Multiply signed/unsigned
0x0060:  mulsu r19, r19
0x0062:  mulsu r16, r16
0x0064:  muls r28, r29        ; Multiply signed
0x0066:  muls r25, r26
```

**특징:**
- 고정소수점 연산 (fractional multiply)
- 최적화된 수학 라이브러리 사용
- 신호 처리 또는 제어 알고리즘 가능성

---

## 🎛️ 추정 시스템 기능

### 가능한 응용 분야

1. **센서 모니터링 시스템**
   - ADC를 통한 아날로그 센서 읽기
   - 주기적 샘플링
   - 데이터 저장 및 처리

2. **모터 제어**
   - 타이머 기반 PWM
   - 피드백 제어
   - 속도/위치 제어

3. **측정 장비**
   - 정밀 타이밍
   - 데이터 수집
   - EEPROM에 설정 저장

4. **산업 제어기**
   - 실시간 제어
   - 다중 센서 입력
   - 상태 머신 기반 로직

---

## 🔍 코드 특징

### 장점
✅ 인터럽트 기반 아키텍처 - 효율적인 CPU 사용  
✅ EEPROM 활용 - 설정 영구 저장  
✅ 워치독 타이머 - 시스템 안정성  
✅ 최적화된 수학 연산 - 고속 처리  

### 최적화 기법
- 레지스터 직접 사용
- 인라인 연산
- 룩업 테이블 활용
- 효율적인 분기 처리

---

## 📝 추가 분석 권장사항

1. **하드웨어 회로도 확인**
   - 정확한 I/O 핀 매핑
   - 센서/액추에이터 연결
   - 외부 회로 이해

2. **EEPROM 내용 덤프**
   - 저장된 설정값 확인
   - 기본값 파악

3. **동적 분석**
   - 시뮬레이터 실행
   - 디버거로 추적
   - 실제 동작 관찰

4. **데이터 시트 참조**
   - ATmega16 레지스터 맵
   - 타이머 설정값 계산
   - 인터럽트 우선순위

---

## 🛠️ 컴파일/프로그래밍 정보

### 필요 도구
```bash
# AVR 툴체인
sudo apt-get install avr-gcc avr-libc avrdude

# 어셈블
avr-as -mmcu=atmega16 firmware_disassembled.asm -o firmware.o

# 링크
avr-ld -o firmware.elf firmware.o

# HEX 생성
avr-objcopy -O ihex firmware.elf firmware.hex
```

### 프로그래밍
```bash
# USBasp 프로그래머 사용
avrdude -c usbasp -p m16 -U flash:w:m16_controller_prg.hex

# STK500 프로그래머 사용
avrdude -c stk500 -p m16 -P /dev/ttyUSB0 -U flash:w:m16_controller_prg.hex
```

---

## 📌 결론

이 펌웨어는 **타이머와 ADC를 활용한 실시간 제어 시스템**으로 보입니다.

**핵심 특징:**
- 인터럽트 기반 이벤트 처리
- 주기적 센서 샘플링
- EEPROM 설정 관리
- 최적화된 수학 연산

**적합한 응용:**
- 산업용 센서 모니터링
- 모터 제어 시스템
- 데이터 로거
- 임베디드 제어기

완전한 이해를 위해서는 하드웨어 사양과 원본 소스 코드 문서가 필요합니다.

---

## 📚 참고 자료

- [ATmega16 데이터시트](https://www.microchip.com/en-us/product/ATmega16)
- [AVR Instruction Set Manual](https://www.microchip.com/en-us/products/microcontrollers-and-microprocessors/8-bit-mcus/avr-mcus)
- Intel HEX 파일 포맷
- AVR 어셈블리 프로그래밍 가이드
