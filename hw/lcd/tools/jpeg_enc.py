"""DGUS ICL 항목용 baseline JPEG 인코더 (표준 라이브러리만).

DGUS 툴이 만든 기존 항목의 JPEG 머리(626 B: 양자화·허프만 표, SOS)를 **그대로 재사용**하고
스캔 데이터만 새로 만든다 — 패널이 받는 머리 형식이 툴 출력과 같아진다.
입력 그림은 macOS `sips` 로 BMP 로 풀어 읽는다.
"""
import math
import os
import struct
import subprocess
import tempfile

from dgus import DGUS_JPEG_HEAD_LEN, MIN_TAIL_PAD, DgusFormatError, IclEntry

ZIGZAG = (0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5, 12, 19, 26, 33, 40, 48,
          41, 34, 27, 20, 13, 6, 7, 14, 21, 28, 35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23,
          30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63)
_COS = [[(math.sqrt(0.5) if u == 0 else 1.0) * 0.5 * math.cos((2 * x + 1) * u * math.pi / 16)
         for x in range(8)] for u in range(8)]
EXPECTED_SAMPLING = (0x22, 0x11, 0x11)  # 4:2:0 — DGUS 툴 출력


# ---------- 입력 그림 ----------

def load_rgb(path: str):
    """그림 파일 → (폭, 높이, [(r,g,b)...] 행 우선). sips 로 BMP 변환 후 읽는다."""
    with tempfile.TemporaryDirectory() as tmp:
        bmp = os.path.join(tmp, "in.bmp")
        res = subprocess.run(["sips", "-s", "format", "bmp", path, "--out", bmp],
                             capture_output=True, text=True)
        if res.returncode != 0 or not os.path.exists(bmp):
            raise DgusFormatError(f"sips 변환 실패: {path}: {res.stderr.strip()}")
        with open(bmp, "rb") as f:
            return _parse_bmp(f.read())


def _parse_bmp(b: bytes):
    if b[:2] != b"BM":
        raise DgusFormatError("BMP 아님")
    data_off, w, h, bpp = struct.unpack("<I", b[10:14])[0], *struct.unpack("<ii", b[18:26]), struct.unpack("<H", b[28:30])[0]
    if bpp not in (24, 32):
        raise DgusFormatError(f"BMP {bpp}bpp 미지원")
    top_down, h = h < 0, abs(h)
    step = bpp // 8
    stride = (w * step + 3) & ~3
    px = []
    for y in range(h):
        row = data_off + (y if top_down else h - 1 - y) * stride
        px.extend((b[row + x * step + 2], b[row + x * step + 1], b[row + x * step]) for x in range(w))
    return w, h, px


# ---------- 템플릿 머리 ----------

def _segments(head: bytes):
    i, out = 2, []
    while i < len(head):
        if head[i] != 0xFF:
            raise DgusFormatError(f"JPEG 머리 마커 깨짐 @{i}")
        marker, length = head[i + 1], struct.unpack(">H", head[i + 2:i + 4])[0]
        out.append((marker, i, head[i + 4:i + 2 + length]))
        if marker == 0xDA:
            break
        i += 2 + length
    return out


def _huff_codes(counts, symbols):
    codes, code, k = {}, 0, 0
    for length in range(1, 17):
        for _ in range(counts[length - 1]):
            codes[symbols[k]] = (code, length)
            code, k = code + 1, k + 1
        code <<= 1
    return codes


def _parse_head(head: bytes):
    """JPEG 머리 → 표 원본들. baseline·8비트 양자화 표만. APPn·COM 은 무시."""
    qt, dht, comps, sel, sof_at, size, sos_end = {}, {}, None, None, None, None, None
    for marker, at, seg in _segments(head):
        if marker == 0xDB:
            i = 0
            while i < len(seg):
                if seg[i] >> 4:
                    raise DgusFormatError("16비트 양자화 표 미지원")
                qt[seg[i] & 15] = bytes(seg[i + 1:i + 65])
                i += 65
        elif marker == 0xC0:
            sof_at, size = at, struct.unpack(">HH", seg[1:5])[::-1]  # (폭, 높이)
            comps = tuple((seg[6 + 3 * c + 1], seg[6 + 3 * c + 2]) for c in range(seg[5]))
        elif marker == 0xC4:
            i = 0
            while i < len(seg):
                n = 17 + sum(seg[i + 1:i + 17])
                dht[(seg[i] >> 4, seg[i] & 15)] = bytes(seg[i + 1:i + n])
                i += n
        elif marker == 0xDA:
            sel = tuple((seg[2 + 2 * c] >> 4, seg[2 + 2 * c] & 15) for c in range(seg[0]))
            sos_end = at + 2 + len(seg) + 2
        elif not (0xE0 <= marker <= 0xEF or marker == 0xFE):
            raise DgusFormatError(f"지원 밖 마커 0x{marker:02x} (baseline·재시작 없음만)")
    if comps is None or sel is None:
        raise DgusFormatError("JPEG 머리에 SOF0/SOS 없음")
    return {"qt": qt, "dht": dht, "comps": comps, "sel": sel, "sof_at": sof_at, "size": size, "sos_end": sos_end}


def _tables_signature(h):
    """스캔 데이터 해석에 필요한 것 — 같으면 스캔을 그대로 옮겨도 된다."""
    return (tuple(h["qt"][q] for _, q in h["comps"]), h["comps"], h["sel"],
            tuple(h["dht"].get((0, d)) for d, _ in h["sel"]), tuple(h["dht"].get((1, a)) for _, a in h["sel"]))


def parse_template(head: bytes):
    """DGUS 머리 → (성분별 (양자화, DC 부호, AC 부호), SOF 위치). 4:2:0 만."""
    if len(head) != DGUS_JPEG_HEAD_LEN:
        raise DgusFormatError(f"템플릿 머리 {len(head)} B (기대 {DGUS_JPEG_HEAD_LEN})")
    h = _parse_head(head)
    if tuple(s for s, _ in h["comps"]) != EXPECTED_SAMPLING:
        raise DgusFormatError("템플릿이 YCbCr 4:2:0 아님")
    codes = lambda raw: _huff_codes(raw[:16], raw[16:])
    tables = [(h["qt"][q], codes(h["dht"][(0, d)]), codes(h["dht"][(1, a)]))
              for (_, q), (d, a) in zip(h["comps"], h["sel"])]
    return tables, h["sof_at"]


def passthrough_scan(src: bytes, template_head: bytes):
    """입력 JPEG 가 템플릿과 같은 표·샘플링이면 → (크기, 스캔+EOI). 아니면 None."""
    if src[:2] != b"\xff\xd8":
        return None
    try:
        h = _parse_head(src)
    except (DgusFormatError, struct.error, IndexError):
        return None
    if _tables_signature(h) != _tables_signature(_parse_head(template_head)):
        return None
    end = src.rfind(b"\xff\xd9")
    return (h["size"], src[h["sos_end"]:end + 2]) if end > h["sos_end"] else None


# ---------- 인코딩 ----------

class _Bits:
    def __init__(self):
        self.out, self.acc, self.n = bytearray(), 0, 0

    def put(self, code, length):
        self.acc = (self.acc << length) | code
        self.n += length
        while self.n >= 8:
            self.n -= 8
            byte = (self.acc >> self.n) & 0xFF
            self.out.append(byte)
            if byte == 0xFF:
                self.out.append(0)  # 바이트 스터핑
        self.acc &= (1 << self.n) - 1

    def flush(self):
        if self.n:
            self.put((1 << (8 - self.n)) - 1, 8 - self.n)


def _fdct(blk):
    tmp = [0.0] * 64
    for y in range(8):
        row = blk[y * 8:y * 8 + 8]
        for u in range(8):
            c = _COS[u]
            tmp[y * 8 + u] = sum(c[x] * row[x] for x in range(8))
    out = [0.0] * 64
    for u in range(8):
        col = [tmp[y * 8 + u] for y in range(8)]
        for v in range(8):
            c = _COS[v]
            out[v * 8 + u] = sum(c[y] * col[y] for y in range(8))
    return out


def _encode_block(bits, blk, prev_dc, qt, dc_codes, ac_codes):
    coef = _fdct(blk)
    zz = []
    for k in range(64):
        x = coef[ZIGZAG[k]] / qt[k]
        zz.append(int(x + 0.5) if x >= 0 else -int(-x + 0.5))
    diff = zz[0] - prev_dc
    _put_value(bits, dc_codes, 0, diff)
    run = 0
    for v in zz[1:]:
        if v == 0:
            run += 1
            continue
        while run > 15:
            bits.put(*ac_codes[0xF0])
            run -= 16
        _put_value(bits, ac_codes, run << 4, v)
        run = 0
    if run:
        bits.put(*ac_codes[0x00])
    return zz[0]


def _put_value(bits, codes, prefix, v):
    size = abs(v).bit_length()
    bits.put(*codes[prefix | size])
    if size:
        bits.put(v if v > 0 else v + (1 << size) - 1, size)


def _planes(w, h, px):
    pw, ph = (w + 15) & ~15, (h + 15) & ~15
    y_p, cb_p, cr_p = [], [], []
    for yy in range(ph):
        src = min(yy, h - 1) * w
        for xx in range(pw):
            r, g, b = px[src + min(xx, w - 1)]
            y_p.append(0.299 * r + 0.587 * g + 0.114 * b - 128)
            cb_p.append(-0.168736 * r - 0.331264 * g + 0.5 * b)
            cr_p.append(0.5 * r - 0.418688 * g - 0.081312 * b)
    cw = pw // 2
    sub = lambda p: [(p[2 * y * pw + 2 * x] + p[2 * y * pw + 2 * x + 1] + p[(2 * y + 1) * pw + 2 * x]
                      + p[(2 * y + 1) * pw + 2 * x + 1]) / 4 for y in range(ph // 2) for x in range(cw)]
    return pw, ph, y_p, sub(cb_p), sub(cr_p)


def _block(plane, stride, x0, y0):
    return [plane[(y0 + y) * stride + x0 + x] for y in range(8) for x in range(8)]


def encode_scan(w, h, px, tables):
    pw, ph, y_p, cb_p, cr_p = _planes(w, h, px)
    (qy, dcy, acy), (qcb, dccb, accb), (qcr, dccr, accr) = tables
    bits, dc = _Bits(), [0, 0, 0]
    for my in range(0, ph, 16):
        for mx in range(0, pw, 16):
            for oy, ox in ((0, 0), (0, 8), (8, 0), (8, 8)):
                dc[0] = _encode_block(bits, _block(y_p, pw, mx + ox, my + oy), dc[0], qy, dcy, acy)
            dc[1] = _encode_block(bits, _block(cb_p, pw // 2, mx // 2, my // 2), dc[1], qcb, dccb, accb)
            dc[2] = _encode_block(bits, _block(cr_p, pw // 2, mx // 2, my // 2), dc[2], qcr, dccr, accr)
    bits.flush()
    return bytes(bits.out) + b"\xff\xd9"


def encode_entry(path: str, template: IclEntry, expect_size):
    """그림 → (template 과 같은 머리를 쓰는 ICL 항목, 방식). expect_size=(폭,높이) 불일치면 거부.

    방식 "passthrough": 입력 jpg 가 DGUS 형식(같은 양자화·허프만 표, 4:2:0)이면 스캔을 그대로 옮긴다 — 무손실.
    방식 "encode": 그 밖의 그림은 픽셀을 풀어 다시 인코딩한다 — 색 보간 때문에 약간 손실.
    """
    head = template.body[:template.head_len]
    tables, sof_at = parse_template(head)
    with open(path, "rb") as f:
        same = passthrough_scan(f.read(), head)
    if same:
        (w, h), scan, mode = same[0], same[1], "passthrough"
    else:
        w, h, px = load_rgb(path)
        scan, mode = None, "encode"
    if (w, h) != tuple(expect_size):
        raise DgusFormatError(f"{path}: {w}×{h} — {expect_size[0]}×{expect_size[1]} 이어야 함")
    if scan is None:
        scan = encode_scan(w, h, px, tables)
    head = head[:sof_at + 5] + struct.pack(">HH", h, w) + head[sof_at + 9:]
    pad = MIN_TAIL_PAD + (-(len(scan) + MIN_TAIL_PAD)) % 4  # DGUS 툴과 같은 패딩 규칙
    return IclEntry(w, h, len(head), head + scan + b"\0" * pad), mode
