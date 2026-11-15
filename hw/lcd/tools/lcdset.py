#!/usr/bin/env python3
"""lcdset — 업체별 DGUS LCD 세트 관리 CLI. 사용법은 같은 폴더 README.md.

  list                       업체 표와 상태
  check  [업체...]           계약·무결성 검사 (FAIL 이 있으면 종료 코드 1)
  build  <업체> [--force]    기준 세트 + 로고/레벨 바 → <작업원본>/<업체>/DWIN_SET
  report [업체...] [--open]  HTML 보고서 (icl 칸 그림, 기준 세트 대비 차이, 검사 결과)
  sync-preview <업체>        작업 원본 → 정본 rsync 미리보기 (실행은 하지 않는다)
"""
import sys

sys.dont_write_bytecode = True  # 저장소에 __pycache__ 를 남기지 않는다

import argparse  # noqa: E402
import json  # noqa: E402
import os  # noqa: E402
import shlex  # noqa: E402
import shutil  # noqa: E402
import subprocess  # noqa: E402
import time  # noqa: E402
import unicodedata  # noqa: E402

import dgus  # noqa: E402

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
CANON_DIR = os.path.normpath(os.path.join(TOOLS_DIR, "..", "lcd_V30"))
WORK_DIR = os.environ.get("LCD_WORK", os.path.expanduser("~/dev/work/gdsonic/gds_lcd_V30"))

# 업체별로 바꿔 끼우는 칸: 키 → (icl, ID, (폭, 높이), DWIN_SET 에 원본도 복사할 이름)
SLOTS = {
    "logo": ("32.icl", 0, (800, 480), "00_logo.jpg"),
    "bar": ("48.icl", 11, (64, 16), None),
}
CONTRACT_BINS = ("13TouchFile.bin", "14ShowFile.bin", "22_Config.bin")
IGNORED = {".DS_Store", "Thumbs.db"}


class LcdError(Exception):
    pass


def load_brands(path=os.path.join(TOOLS_DIR, "brands.json")):
    with open(path, encoding="utf-8") as f:
        cfg = json.load(f)
    for name, b in cfg["brands"].items():
        if b.get("family") not in cfg["families"]:
            raise LcdError(f"brands.json: {name} family '{b.get('family')}' 없음")
        unknown = set(b) - {"family", "fw_brand"} - set(SLOTS)
        if unknown:
            raise LcdError(f"brands.json: {name} 모르는 키 {sorted(unknown)}")
    return cfg


def base_of(cfg, name):
    if name not in cfg["brands"]:
        raise LcdError(f"업체 '{name}' 가 brands.json 에 없음 (있는 것: {', '.join(cfg['brands'])})")
    return cfg["families"][cfg["brands"][name]["family"]]


def dwin(work, name):
    return os.path.join(work, name, "DWIN_SET")


def _read(path):
    with open(path, "rb") as f:
        return f.read()


def icl_diff_ids(base_path, path):
    a, b = dgus.parse_icl(_read(base_path)), dgus.parse_icl(_read(path))
    n = max(len(a.entries), len(b.entries))
    get = lambda icl, i: icl.entries[i] if i < len(icl.entries) else None
    return [i for i in range(n) if get(a, i) != get(b, i)]


def _listing(d):
    return {f for f in os.listdir(d) if f not in IGNORED and not f.startswith(".")}


# ---------- check ----------

def check_brand(cfg, work, name):
    """→ [(수준 PASS|WARN|FAIL, 메시지)]"""
    d = dwin(work, name)
    if not os.path.isdir(d):
        return [("FAIL", f"DWIN_SET 없음: {d}")]
    res = []
    for icl in ("32.icl", "48.icl"):
        try:
            n = sum(e is not None for e in dgus.parse_icl(_read(os.path.join(d, icl))).entries)
            res.append(("PASS", f"{icl} 무결성(CRC·크기·오프셋), 칸 {n}개"))
        except (OSError, dgus.DgusFormatError) as e:
            res.append(("FAIL", f"{icl}: {e}"))
    try:
        for page, ok, desc in dgus.check_mode_icons(_read(os.path.join(d, "14ShowFile.bin"))):
            res.append(("PASS" if ok else "FAIL", f"p{page} 모드 아이콘 VP 0x140c (0x140b 없음) — {desc}"))
    except (OSError, dgus.DgusFormatError) as e:
        res.append(("FAIL", f"14ShowFile.bin: {e}"))
    for f in sorted(_listing(d)):
        if f.lower().endswith(".cfg"):
            bad = dgus.cfg_kills_touch(_read(os.path.join(d, f)))
            res.append(("FAIL", f"{f}: 0x20=5A·0x21=10 — 2025 배치 터치가 죽는 값") if bad
                       else ("PASS", f"{f}: 터치 설정 공장값(0x20/0x21 ≠ 5A/10)"))
    base = base_of(cfg, name)
    if name != base:
        res.extend(_compare_with_base(cfg, work, name, base))
    return res


def _compare_with_base(cfg, work, name, base):
    d, bd = dwin(work, name), dwin(work, base)
    res = []
    for f in CONTRACT_BINS:
        same = _read(os.path.join(d, f)) == _read(os.path.join(bd, f))
        res.append(("PASS" if same else "FAIL", f"{f} = 기준 {base}" if same else f"{f} ≠ 기준 {base} (계약 파일)"))
    only_base, only_me = _listing(bd) - _listing(d), _listing(d) - _listing(bd)
    if only_base or only_me:
        res.append(("WARN", f"파일 목록 차이 — 기준에만 {sorted(only_base)}, 여기에만 {sorted(only_me)}"))
    brand = cfg["brands"][name]
    for icl in ("32.icl", "48.icl"):
        allowed = {SLOTS[k][1] for k in SLOTS if k in brand and SLOTS[k][0] == icl}
        diff = icl_diff_ids(os.path.join(bd, icl), os.path.join(d, icl))
        extra = sorted(set(diff) - allowed)
        if extra:
            res.append(("WARN", f"{icl} 기준 {base} 와 다른 칸 {len(extra)}개(허용 {sorted(allowed)} 외): {extra}"))
        else:
            res.append(("PASS", f"{icl} 기준 {base} 대비 다른 칸 = {sorted(diff)} (허용 {sorted(allowed)})"))
    return res


# ---------- build ----------

def build_brand(cfg, work, name, force=False):
    base = base_of(cfg, name)
    if name == base:
        raise LcdError(f"{name} 는 기준 세트 — build 대상 아님 (DGUS 툴에서 편집)")
    brand = cfg["brands"][name]
    inputs = {k: os.path.join(work, name, brand[k]) for k in SLOTS if k in brand}
    missing = [p for p in inputs.values() if not os.path.isfile(p)]
    if missing:
        raise LcdError("입력 그림 없음:\n  " + "\n  ".join(missing))
    out = dwin(work, name)
    if os.path.exists(out) and not force:
        raise LcdError(f"{out} 이미 있음 — 덮으려면 --force (기존 것은 DWIN_SET.bak-<시각> 으로 옮김)")
    import jpeg_enc  # 무거운 인코더는 build 때만

    tmp = out + ".tmp"
    shutil.rmtree(tmp, ignore_errors=True)
    shutil.copytree(dwin(work, base), tmp, ignore=shutil.ignore_patterns(*IGNORED))
    log = [f"base={base}", f"time={time.strftime('%Y-%m-%d %H:%M:%S')}"]
    for key, src in inputs.items():
        icl_name, eid, size, copy_as = SLOTS[key]
        path = os.path.join(tmp, icl_name)
        icl = dgus.parse_icl(_read(path))
        template = icl.entries[eid] if eid < len(icl.entries) else None
        if template is None:
            raise LcdError(f"기준 {base} {icl_name} #{eid} 가 비어 있음 — 템플릿 없음")
        entry, mode = jpeg_enc.encode_entry(src, template, size)
        with open(path, "wb") as f:
            f.write(dgus.build_icl(dgus.replace_entry(icl, eid, entry)))
        if copy_as:
            shutil.copyfile(src, os.path.join(tmp, copy_as))
        log.append(f"{key}: {icl_name} #{eid} <- {os.path.relpath(src, os.path.join(work, name))} ({mode})")
    if os.path.exists(out):
        os.rename(out, out + time.strftime(".bak-%Y%m%d-%H%M%S"))
    os.rename(tmp, out)
    with open(os.path.join(work, name, "BUILD.txt"), "w", encoding="utf-8") as f:
        f.write("lcdset build\n" + "\n".join(log) + "\n")
    return log


# ---------- sync-preview ----------

def _nfc_twins(src, dst):
    """이름이 NFC 정규화로만 다른 파일 쌍 → [(상대경로 src, 상대경로 dst, 내용 같음?)]"""
    def walk(root):
        out = {}
        for dp, _, fs in os.walk(root):
            for f in fs:
                rel = os.path.relpath(os.path.join(dp, f), root)
                out[rel] = unicodedata.normalize("NFC", rel)
        return out
    s, d = walk(src), walk(dst)
    d_by_nfc = {v: k for k, v in d.items()}
    return [(rs, d_by_nfc[n], _read(os.path.join(src, rs)) == _read(os.path.join(dst, d_by_nfc[n])))
            for rs, n in s.items() if n in d_by_nfc and d_by_nfc[n] != rs]


def sync_preview(work, canon, name):
    src, dst = os.path.join(work, name), os.path.join(canon, name)
    if not os.path.isdir(src):
        raise LcdError(f"작업 원본 없음: {src}")
    notes, excludes = [], [".DS_Store", "Thumbs.db"]
    if not os.path.isdir(dst):
        notes.append(f"정본에 {name}/ 없음 — 신규 업체, 전체가 추가된다")
    else:
        for rs, rd, same in _nfc_twins(src, dst):
            if same:
                excludes += [os.path.basename(rs), os.path.basename(rd)]
                notes.append(f"이름 인코딩만 다름(NFD/NFC), 내용 같음 → 제외: {rs}")
            else:
                notes.append(f"⚠ 이름 인코딩만 다르고 내용도 다름 — 수동 확인: {rs}")
    cmd = ["rsync", "-rcn", "--delete", "--itemize-changes"]
    for e in excludes:
        cmd += ["--exclude", e]
    cmd += [src + "/", dst + "/"]
    # 옛 윈도우 파일명(UTF-8 아님)이 섞여 있다 — 깨진 글자는 대체 문자로 보여 준다
    res = subprocess.run(cmd, capture_output=True, encoding="utf-8", errors="replace")
    if res.returncode != 0:
        raise LcdError(f"rsync 실패: {res.stderr.strip()}")
    lines = []
    for line in res.stdout.splitlines():
        if line.startswith((".d", ".f..T")) or not line.strip():
            continue
        path = line.split(" ", 1)[-1]
        if line.startswith(">fc"):
            s, d = os.path.join(src, path), os.path.join(dst, path)
            if os.path.exists(d) and os.path.getmtime(s) < os.path.getmtime(d):
                line += "    ⚠ 역행 의심: 작업 원본이 정본보다 오래됨"
        lines.append(line)
    run = " ".join(shlex.quote(c) for c in cmd if c != "--itemize-changes").replace("-rcn", "-rc", 1)
    return notes, lines, run


# ---------- CLI ----------

def cmd_list(cfg, a):
    for name, b in cfg["brands"].items():
        base = base_of(cfg, name)
        d = dwin(a.work, name)
        inputs = [f"{k}={'있음' if os.path.isfile(os.path.join(a.work, name, b[k])) else '없음'}"
                  for k in SLOTS if k in b]
        role = "기준" if name == base else f"← {base}"
        print(f"{name:10} {b['family']:5} {role:10} FW={b['fw_brand'] or '(미정)':10} "
              f"DWIN_SET={'있음' if os.path.isdir(d) else '없음'}  {' '.join(inputs)}")


def cmd_check(cfg, a):
    names = a.brands or [n for n in cfg["brands"] if os.path.isdir(dwin(a.work, n))]
    failed = False
    for n in names:
        print(f"== {n}")
        for lvl, msg in check_brand(cfg, a.work, n):
            failed |= lvl == "FAIL"
            print(f"  {lvl:4} {msg}")
    return 1 if failed else 0


def cmd_build(cfg, a):
    for line in build_brand(cfg, a.work, a.brand, a.force):
        print("  " + line)
    print(f"→ {dwin(a.work, a.brand)}")
    return cmd_check(cfg, argparse.Namespace(work=a.work, brands=[a.brand]))


def cmd_report(cfg, a):
    import report
    path = report.write_report(cfg, a.work, a.brands or list(cfg["brands"]), a.out)
    print(f"→ {path}")
    if a.open:
        subprocess.run(["open", path])
    return 0


def cmd_sync_preview(cfg, a):
    notes, lines, run = sync_preview(a.work, a.canon, a.brand)
    for n in notes:
        print("  * " + n)
    print("\n".join(lines) if lines else "  (차이 없음)")
    print(f"\n확인 후 실행할 명령:\n  {run}")
    return 0


def main(argv=None):
    p = argparse.ArgumentParser(prog="lcdset", description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--work", default=WORK_DIR, help=f"작업 원본 (기본 {WORK_DIR}, 환경변수 LCD_WORK)")
    p.add_argument("--canon", default=CANON_DIR, help=f"정본 (기본 {CANON_DIR})")
    sub = p.add_subparsers(dest="cmd", required=True)
    sub.add_parser("list").set_defaults(fn=cmd_list)
    s = sub.add_parser("check"); s.add_argument("brands", nargs="*"); s.set_defaults(fn=cmd_check)
    s = sub.add_parser("build"); s.add_argument("brand"); s.add_argument("--force", action="store_true"); s.set_defaults(fn=cmd_build)
    s = sub.add_parser("report"); s.add_argument("brands", nargs="*")
    s.add_argument("--out", default=None, help="출력 폴더 (기본 <작업원본>/_report)")
    s.add_argument("--open", action="store_true", help="브라우저로 연다"); s.set_defaults(fn=cmd_report)
    s = sub.add_parser("sync-preview"); s.add_argument("brand"); s.set_defaults(fn=cmd_sync_preview)
    a = p.parse_args(argv)
    try:
        return a.fn(load_brands(), a) or 0
    except (LcdError, dgus.DgusFormatError) as e:
        print(f"오류: {e}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
