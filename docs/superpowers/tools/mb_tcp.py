#!/usr/bin/env python3
"""벤치용 최소 Modbus TCP 마스터 — mbpoll 대체.

⚠ 이 환경에서 mbpoll 은 동작하지 않는다(`Connection failed: No route to host`,
   같은 순간 python socket 은 성공. 샌드박스 해제로도 동일). 2026-09-05 벤치는
   전부 이 클라이언트로 수행했다.

사용법 (주소는 wire 기준 0-based — 체크리스트의 `-r N` 은 N-1):
    import sys; sys.path.insert(0, 'docs/superpowers/tools')
    from mb_tcp import MB
    with MB() as m:                    # 기본 192.168.1.199:502 unit 1
        print(m.read(0x00, 50))        # FC03
        print(m.r1(0x1D))              # STATUS
        print(m.write(0x06, 80))       # FC06 + read-back 반환

🔴 생존 판정은 반드시 FC03 트랜잭션으로 — TCP connect 성공은 MCU 생존을 뜻하지
   않는다(W5500 이 SYN 을 자체 응답하므로 MCU 가 halt/hang 이어도 붙는다).
🔴 `nc -z` 류 무-데이터 프로브 금지 — 구 펌웨어에서는 소켓을 영구 고착시킨다.
   상세 = docs/superpowers/plans/2026-09-05-bench-results.md §4
"""

class MB:
    def __init__(self, host='192.168.1.199', port=502, unit=1, timeout=3.0, retries=15):
        self.a, self.unit, self.to, self.retries = (host, port), unit, timeout, retries
        self.s, self.tid = None, 0

    def connect(self):
        last = None
        for _ in range(self.retries):
            try:
                self.s = socket.create_connection(self.a, self.to)
                self.s.settimeout(self.to)
                self.s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                return self
            except OSError as e:
                last, _ = e, time.sleep(2)
        raise last

    def close(self):
        if self.s:
            self.s.close()
            self.s = None

    def __enter__(self): return self.connect()
    def __exit__(self, *a): self.close()

    def _tx(self, pdu):
        self.tid = (self.tid + 1) & 0xFFFF
        self.s.sendall(struct.pack('>HHHB', self.tid, 0, len(pdu) + 1, self.unit) + pdu)
        h = b''
        while len(h) < 7:
            c = self.s.recv(7 - len(h))
            if not c: raise IOError('peer closed')
            h += c
        need, body = struct.unpack('>H', h[4:6])[0] - 1, b''
        while len(body) < need:
            c = self.s.recv(need - len(body))
            if not c: raise IOError('peer closed')
            body += c
        if body[0] & 0x80: raise IOError(f'modbus exception {body[1]}')
        return body

    def read(self, addr, count=1):
        r = self._tx(struct.pack('>BHH', 3, addr, count))
        return list(struct.unpack('>%dH' % count, r[2:2 + r[1]]))

    def r1(self, addr): return self.read(addr, 1)[0]

    def write(self, addr, val):
        self._tx(struct.pack('>BHH', 6, addr, val & 0xFFFF))
        return self.r1(addr)          # write 후 read-back
