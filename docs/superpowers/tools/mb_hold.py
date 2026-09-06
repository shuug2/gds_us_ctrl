#!/usr/bin/env python3
"""mb_hold.py — hold-to-run 벤치용 유지 신호 송신기 (spec 2026-09-06 §8 H-2/H-3/H-9).

사용:  python3 mb_hold.py [host] [period_ms] [start_val]
       host      기본 192.168.1.199
       period_ms 유지 신호 주기, 기본 150 (H-9 는 550 / 650)
       start_val 첫 쓰기 값, 기본 2 (H-5 는 0 = START 안 보내고 keep 만)
동작:  START=start_val 1회 → period 마다 START=3 + STATUS(0x1D) bit0 출력.
       Ctrl-C = 손 뗌. 이후 2 s 동안 100 ms 마다 US 를 찍어 트립 시점을 잡는다.
⚠ mbpoll 은 이 환경에서 동작하지 않는다(bench-results §4). nc -z 금지.
"""
import sys, time
from mb_tcp import MB

host      = sys.argv[1] if len(sys.argv) > 1 else '192.168.1.199'
period    = (float(sys.argv[2]) if len(sys.argv) > 2 else 150.0) / 1000.0
start_val = int(sys.argv[3]) if len(sys.argv) > 3 else 2

with MB(host) as mb:
    t0 = time.monotonic()
    if start_val:
        mb.write(0x1B, start_val)
        print(f"{0.000:7.3f}s START={start_val}", flush=True)
    next_t = time.monotonic()
    try:
        while True:
            mb.write(0x1B, 3)
            us = mb.r1(0x1D) & 1
            print(f"{time.monotonic()-t0:7.3f}s keep  US={us}", flush=True)
            next_t += period
            time.sleep(max(0.0, next_t - time.monotonic()))
    except KeyboardInterrupt:
        print("--- keep 중단 (손 뗌) ---", flush=True)
    t_rel = time.monotonic()
    for _ in range(20):
        us = mb.r1(0x1D) & 1
        print(f"{time.monotonic()-t0:7.3f}s  +{(time.monotonic()-t_rel)*1000:4.0f}ms  US={us}", flush=True)
        if us == 0:
            break
        time.sleep(0.1)
    else:
        mb.write(0x1C, 1)                     # 워치독 FAIL — 마스터가 직접 STOP
        print("!!! 워치독 미트립 — STOP 송신 (H-3 FAIL)", flush=True)
