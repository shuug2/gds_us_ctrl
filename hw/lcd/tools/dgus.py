"""DGUS T5L 자산 파일 형식 — ICL 라이브러리 읽기/쓰기, 14ShowFile·T5LCFG 검사.

ICL (`32.icl` 배경, `48.icl` 아이콘), 2026-11-15 실파일로 역분석·왕복 검증:
  0x00 "DGUS_3" | 0x06 CRC16(Modbus, LE, 0x08~끝) | 0x08 크기 BE32(= 파일-8)
  0x0C 2B(원본 그대로 보존) | 0x0E 최대 ID LE16
  0x10 ID 0..최대 별 절대 오프셋 BE32 (0 = 빈 칸), 이어서 항목이 ID 순으로 붙는다.
  항목 = 폭 BE16 · 높이 BE16 · 머리길이 k BE16 · 본문길이 L BE32 + JPEG(k+L 바이트).
  DGUS 툴의 JPEG: baseline 4:2:0, 머리 k=626 고정(SOS 길이를 15로 써서 0 3바이트 덧붙임),
  끝은 EOI 뒤 0 패딩 8바이트 이상·4바이트 정렬.
"""
import struct
from dataclasses import dataclass, replace
from typing import Optional, Tuple

ICL_MAGIC = b"DGUS_3"
ICL_HEADER_LEN = 0x10
ENTRY_HEAD_LEN = 10
DGUS_JPEG_HEAD_LEN = 626
MIN_TAIL_PAD = 8

SHOW_PAGE_TABLE = 0x10
SHOW_RECORD_LEN = 32
MODE_ICON_PAGES = (21, 23, 25, 27)
VP_MODE_ICON = 0x140C   # 펌웨어 DISP_COMM_MODE
VP_COMM_BUTTON = 0x140B  # 터치 return 값 — 아이콘에 묶으면 09-13 결함

CFG_TOUCH_KILL = {0x20: 0x5A, 0x21: 0x10}  # 2025 배치 터치가 죽는 값


class DgusFormatError(ValueError):
    pass


def crc16_modbus(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc


@dataclass(frozen=True)
class IclEntry:
    width: int
    height: int
    head_len: int  # JPEG 머리 길이 k
    body: bytes    # JPEG k+L 바이트 (패딩 포함)


@dataclass(frozen=True)
class Icl:
    reserved: bytes                         # 0x0C 2바이트
    entries: Tuple[Optional[IclEntry], ...]  # 인덱스 = ID


def parse_icl(data: bytes) -> Icl:
    if data[:6] != ICL_MAGIC:
        raise DgusFormatError("ICL 매직 DGUS_3 아님")
    size = struct.unpack(">I", data[8:12])[0]
    if size != len(data) - 8:
        raise DgusFormatError(f"ICL 크기 필드 {size} != 파일-8 {len(data) - 8}")
    crc = struct.unpack("<H", data[6:8])[0]
    if crc != crc16_modbus(data[8:]):
        raise DgusFormatError("ICL CRC 불일치")
    max_id = struct.unpack("<H", data[14:16])[0]
    entries = []
    for i in range(max_id + 1):
        off = struct.unpack(">I", data[ICL_HEADER_LEN + 4 * i: ICL_HEADER_LEN + 4 * i + 4])[0]
        if off == 0:
            entries.append(None)
            continue
        if off + ENTRY_HEAD_LEN > len(data):
            raise DgusFormatError(f"ID {i} 오프셋 범위 밖")
        w, h, k, body_len = struct.unpack(">HHHI", data[off: off + ENTRY_HEAD_LEN])
        start = off + ENTRY_HEAD_LEN
        if start + k + body_len > len(data):
            raise DgusFormatError(f"ID {i} 본문이 파일 끝을 넘음")
        entries.append(IclEntry(w, h, k, data[start: start + k + body_len]))
    return Icl(data[12:14], tuple(entries))


def build_icl(icl: Icl) -> bytes:
    n = len(icl.entries)
    table, blobs = [], []
    pos = ICL_HEADER_LEN + 4 * n
    for e in icl.entries:
        if e is None:
            table.append(0)
            continue
        table.append(pos)
        blob = struct.pack(">HHHI", e.width, e.height, e.head_len, len(e.body) - e.head_len) + e.body
        blobs.append(blob)
        pos += len(blob)
    body = icl.reserved + struct.pack("<H", n - 1) + b"".join(struct.pack(">I", o) for o in table) + b"".join(blobs)
    sized = struct.pack(">I", len(body) + 4) + body  # 크기 = 파일 - 8 = (4 + body)
    return ICL_MAGIC + struct.pack("<H", crc16_modbus(sized)) + sized


def replace_entry(icl: Icl, entry_id: int, entry: IclEntry) -> Icl:
    if not 0 <= entry_id < len(icl.entries):
        raise DgusFormatError(f"ID {entry_id} 범위 밖 (0~{len(icl.entries) - 1})")
    entries = icl.entries[:entry_id] + (entry,) + icl.entries[entry_id + 1:]
    return replace(icl, entries=entries)


def entry_to_jpeg(entry: IclEntry) -> bytes:
    """보기용 표준 JPEG — DGUS 의 SOS 길이 15 를 표준(6+2·Ns)으로 고치고 0 패딩을 뗀다."""
    head, scan = entry.body[:entry.head_len], entry.body[entry.head_len:].rstrip(b"\0")
    sos = head.rfind(b"\xff\xda")
    if sos < 0:
        raise DgusFormatError("JPEG 머리에 SOS 없음")
    ns = head[sos + 4]
    std_len = 6 + 2 * ns
    fixed = head[:sos + 2] + struct.pack(">H", std_len) + head[sos + 4: sos + 2 + std_len]
    return fixed + scan


def show_records(show: bytes, page: int):
    """14ShowFile 의 page 레코드들 → [(오프셋, VP)]."""
    pos = SHOW_PAGE_TABLE + 4 * page
    count = struct.unpack("<H", show[pos:pos + 2])[0]
    off = struct.unpack(">H", show[pos + 2:pos + 4])[0]
    recs = []
    for i in range(count):
        r = off + i * SHOW_RECORD_LEN
        if show[r] != 0x5A:
            raise DgusFormatError(f"p{page} 레코드 {i} @0x{r:x} 동기 바이트 0x5A 아님")
        recs.append((r, struct.unpack(">H", show[r + 6:r + 8])[0]))
    return recs


def check_mode_icons(show: bytes):
    """p21/23/25/27: 표시 VP 0x140c 가 있고 0x140b 가 없어야 한다. → [(page, ok, 설명)]"""
    out = []
    for p in MODE_ICON_PAGES:
        vps = [vp for _, vp in show_records(show, p)]
        ok = VP_MODE_ICON in vps and VP_COMM_BUTTON not in vps
        out.append((p, ok, "표시 VP: " + ", ".join(f"0x{v:04x}" for v in vps)))
    return out


def cfg_kills_touch(cfg: bytes) -> bool:
    return all(len(cfg) > a and cfg[a] == v for a, v in CFG_TOUCH_KILL.items())
