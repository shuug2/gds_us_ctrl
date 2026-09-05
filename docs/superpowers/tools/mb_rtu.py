#!/usr/bin/env python3
"""벤치용 최소 Modbus RTU 마스터 — mbpoll 대체 (mb_tcp.py 의 RTU 형제).

⚠ 이 환경에서 mbpoll 은 동작하지 않는다. 상세 =
   docs/superpowers/plans/2026-09-05-bench-results.md §4

사용법 (주소는 wire 기준 0-based — 체크리스트의 `-r N` 은 N-1):
    import sys; sys.path.insert(0, 'docs/superpowers/tools')
    from mb_rtu import MBRTU
    with MBRTU() as m:                 # 기본 9600 8E1, unit 1
        print(m.read(0x00, 50))        # FC03
        print(m.r1(0x1D))              # STATUS
        print(m.write(0x06, 80))       # FC06 + read-back 반환

🔴 mon(USART6 115200 8N1) 과 **같은 어댑터를 공유**한다. mon 캡처용 `cat` 이
   포트를 잡고 있으면 열리지 않는다 → `pkill -x cat` 먼저.
🔴 comm_mode 가 SERIAL 이고 addr != 0 일 때만 보드가 RTU 를 응답한다.
🔴 첫 트랜잭션이 CRC 오류/타임아웃이면 1회 재시도 (포트 재개방 직후 잔재).
"""
import serial, struct, time


def crc16(buf):
    """Modbus CRC16 (poly 0xA001, init 0xFFFF). 반환은 wire 순서(lo, hi)."""
    crc = 0xFFFF
    for b in buf:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return struct.pack('<H', crc)


class MBRTU:
    def __init__(self, port='/dev/cu.usbserial-AB0MLYXA', baud=9600,
                 parity='E', unit=1, timeout=1.0, retries=3):
        self.port, self.baud, self.parity = port, baud, parity
        self.unit, self.to, self.retries = unit, timeout, retries
        self.s = None

    def open(self):
        self.s = serial.Serial(self.port, self.baud, bytesize=8,
                               parity=self.parity, stopbits=1, timeout=self.to)
        time.sleep(0.2)
        self.s.reset_input_buffer()
        return self

    def close(self):
        if self.s:
            self.s.close()
            self.s = None

    def __enter__(self): return self.open()
    def __exit__(self, *a): self.close()

    def _tx(self, pdu, want):
        """want = 응답 PDU 길이(유닛/CRC 제외). 무응답·CRC 오류는 재시도."""
        frame = bytes([self.unit]) + pdu
        frame += crc16(frame)
        last = None
        for _ in range(self.retries):
            self.s.reset_input_buffer()
            self.s.write(frame)
            r = self.s.read(1 + want + 2)
            if len(r) != 1 + want + 2:
                last = IOError('타임아웃 (%d B 수신)' % len(r)); time.sleep(0.1); continue
            if crc16(r[:-2]) != r[-2:]:
                last = IOError('CRC 불일치'); time.sleep(0.1); continue
            if r[1] & 0x80:
                raise IOError('modbus exception %d' % r[2])
            return r[1:-2]
        raise last

    def read(self, addr, count=1):
        r = self._tx(struct.pack('>BHH', 3, addr, count), 2 + 2 * count)
        return list(struct.unpack('>%dH' % count, r[2:2 + 2 * count]))

    def r1(self, addr): return self.read(addr, 1)[0]

    def write(self, addr, val):
        self._tx(struct.pack('>BHH', 6, addr, val & 0xFFFF), 5)
        return self.r1(addr)          # write 후 read-back
