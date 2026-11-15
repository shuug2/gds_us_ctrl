"""lcdset report — 업체별 icl 칸 그림·기준 대비 차이·검사 결과를 HTML 한 장으로."""
import html
import os
import time

import dgus
from lcdset import SLOTS, base_of, check_brand, dwin, _read

PAGE_NAMES = {0: "LOGO", 1: "MODEL_SETUP", 3: "RUN_MULTI/HAND", 5: "SETUP_MULTI", 7: "SETUP_HAND",
              9: "RUN_STD", 10: "SETUP_STD1", 12: "SETUP_STD2D", 13: "SETUP_STD2T", 15: "SETUP_STD3",
              17: "WARNING", 19: "SETUP_MH2", 21: "SETUP_MHC", 23: "SETUP_STDC", 25: "SETUP_MHE",
              27: "SETUP_STDE"}

CSS = """
:root{--bg:#f6f7f9;--card:#fff;--fg:#1d2330;--mute:#667085;--line:#d9dde3;
--pass:#1a7f37;--warn:#9a6700;--fail:#cf222e;--hl:#fff4d6}
@media (prefers-color-scheme:dark){:root{--bg:#14171c;--card:#1d2129;--fg:#e6e9ef;--mute:#98a2b3;
--line:#2f3542;--pass:#3fb950;--warn:#d29922;--fail:#f85149;--hl:#3a3018}}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--fg);
font:14px/1.5 -apple-system,"Apple SD Gothic Neo",sans-serif}
main{max-width:1200px;margin:0 auto;padding:16px}h1{font-size:20px}h2{font-size:17px;margin-top:32px}
nav a{margin-right:12px}.card{background:var(--card);border:1px solid var(--line);border-radius:8px;padding:12px 16px;margin:12px 0}
.PASS{color:var(--pass)}.WARN{color:var(--warn)}.FAIL{color:var(--fail)}
.lvl{font-weight:600;display:inline-block;width:3.2em}
table{border-collapse:collapse;width:100%}td,th{border-top:1px solid var(--line);padding:6px;vertical-align:top;text-align:left}
tr.diff{background:var(--hl)}img{max-width:100%;image-rendering:pixelated;border:1px solid var(--line)}
.bg img{width:320px}.grid{display:flex;flex-wrap:wrap;gap:8px}
.ic{border:1px solid var(--line);border-radius:6px;padding:6px;font-size:12px;text-align:center;min-width:90px}
.ic.diff{background:var(--hl);border-color:var(--warn)}.ic img{max-height:64px;min-width:32px}
.mute{color:var(--mute)}summary{cursor:pointer;font-weight:600}
"""


def _dump_icl(work, brand, icl_name, out_dir):
    """icl 칸들을 jpg 로 쓰고 → {ID: (상대경로, 폭, 높이, 항목)}"""
    path = os.path.join(dwin(work, brand), icl_name)
    if not os.path.isfile(path):
        return {}
    icl = dgus.parse_icl(_read(path))
    sub = os.path.join("img", brand)
    os.makedirs(os.path.join(out_dir, sub), exist_ok=True)
    out = {}
    for i, e in enumerate(icl.entries):
        if e is None:
            continue
        rel = os.path.join(sub, f"{icl_name.split('.')[0]}_{i:02d}.jpg")
        with open(os.path.join(out_dir, rel), "wb") as f:
            f.write(dgus.entry_to_jpeg(e))
        out[i] = (rel, e.width, e.height, e)
    return out


def _checks_html(results):
    rows = "".join(f'<div><span class="lvl {lvl}">{lvl}</span>{html.escape(msg)}</div>' for lvl, msg in results)
    return f'<div class="card">{rows}</div>'


def _bg_table(mine, base, base_name, allowed):
    rows = []
    for i in sorted(set(mine) | set(base)):
        m, b = mine.get(i), base.get(i)
        differs = base_name is not None and (m and m[3]) != (b and b[3])
        tag = ("허용(업체 칸)" if i in allowed else "⚠ 기준과 다름") if differs else ""
        cell = lambda x: f'<img loading="lazy" src="{x[0]}">' if x else '<span class="mute">(없음)</span>'
        base_cell = f"<td>{cell(b)}</td>" if base_name else ""
        rows.append(f'<tr class="{"diff" if differs else ""}"><td>#{i}<br><span class="mute">'
                    f'{PAGE_NAMES.get(i, "")}</span><br>{tag}</td><td>{cell(m)}</td>{base_cell}</tr>')
    head = f"<th>기준 {base_name}</th>" if base_name else ""
    return f'<table class="bg"><tr><th>ID</th><th>이 세트</th>{head}</tr>{"".join(rows)}</table>'


def _icon_grid(mine, base, base_name, allowed):
    cells = []
    for i in sorted(mine):
        rel, w, h, e = mine[i]
        b = base.get(i)
        differs = base_name is not None and (b is None or b[3] != e)
        extra = f'<br><span class="mute">기준:</span><br><img src="{b[0]}">' if differs and b else ""
        note = ("허용" if i in allowed else "⚠ 다름") if differs else ""
        cells.append(f'<div class="ic {"diff" if differs else ""}"><img loading="lazy" src="{rel}">'
                     f'<br>#{i} <span class="mute">{w}×{h}</span> {note}{extra}</div>')
    return f'<div class="grid">{"".join(cells)}</div>'


def _brand_section(cfg, work, name, out_dir, dumps):
    b = cfg["brands"][name]
    base = base_of(cfg, name)
    base_name = None if base == name else base
    parts = [f'<h2 id="{name}">{name} <span class="mute">— {b["family"]} 계열, FW BRAND = '
             f'{b["fw_brand"] or "(펌웨어에 없음)"}, {"기준 세트" if not base_name else "기준 " + base}</span></h2>']
    if not os.path.isdir(dwin(work, name)):
        need = [f"{k}: {b[k]} ({SLOTS[k][2][0]}×{SLOTS[k][2][1]} jpg)" for k in SLOTS if k in b]
        parts.append('<div class="card">DWIN_SET 없음 — 필요한 입력: ' + html.escape(", ".join(need)) +
                     f' → <code>lcdset.py build {name}</code></div>')
        return "".join(parts)
    parts.append(_checks_html(check_brand(cfg, work, name)))
    for icl_name, render in (("32.icl", _bg_table), ("48.icl", _icon_grid)):
        key = (name, icl_name)
        dumps.setdefault(key, _dump_icl(work, name, icl_name, out_dir))
        bkey = (base, icl_name)
        dumps.setdefault(bkey, _dump_icl(work, base, icl_name, out_dir))
        allowed = {SLOTS[k][1] for k in SLOTS if k in b and SLOTS[k][0] == icl_name}
        title = "배경 32.icl" if icl_name == "32.icl" else "아이콘 48.icl"
        parts.append(f"<details open><summary>{title}</summary>"
                     f"{render(dumps[key], dumps[bkey] if base_name else {}, base_name, allowed)}</details>")
    return "".join(parts)


def write_report(cfg, work, brands, out_dir=None):
    out_dir = out_dir or os.path.join(work, "_report")
    os.makedirs(out_dir, exist_ok=True)
    dumps = {}
    nav = " ".join(f'<a href="#{n}">{n}</a>' for n in brands)
    body = "".join(_brand_section(cfg, work, n, out_dir, dumps) for n in brands)
    page = (f'<!doctype html><html lang="ko"><head><meta charset="utf-8">'
            f'<meta name="viewport" content="width=device-width,initial-scale=1"><title>LCD 세트 보고서</title>'
            f'<style>{CSS}</style></head><body><main><h1>DGUS LCD 세트 보고서</h1>'
            f'<p class="mute">작업 원본 {html.escape(work)} · {time.strftime("%Y-%m-%d %H:%M")} · '
            f'노란 칸 = 기준 세트와 다른 칸</p><nav>{nav}</nav>{body}</main></body></html>')
    path = os.path.join(out_dir, "index.html")
    with open(path, "w", encoding="utf-8") as f:
        f.write(page)
    return path
