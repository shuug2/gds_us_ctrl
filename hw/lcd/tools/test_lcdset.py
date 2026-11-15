"""lcdset 자기검사 — `python3 test_lcdset.py` (작업 원본 실데이터 사용, 없으면 건너뜀)."""
import sys

sys.dont_write_bytecode = True

import glob  # noqa: E402
import math  # noqa: E402
import os  # noqa: E402
import shutil  # noqa: E402
import tempfile  # noqa: E402
import unittest  # noqa: E402

import dgus  # noqa: E402
import jpeg_enc  # noqa: E402
import lcdset  # noqa: E402

WORK = lcdset.WORK_DIR
HAVE_WORK = os.path.isdir(os.path.join(WORK, "gdsonic", "DWIN_SET"))
MIN_PSNR_DB = 30.0  # 재인코딩 하한 — 64×16 바의 날카로운 색 경계는 4:2:0 형식 자체가 깎는다(실측 32.9)


def _read(p):
    with open(p, "rb") as f:
        return f.read()


class TestFormat(unittest.TestCase):
    def test_crc16_modbus_known_vector(self):
        self.assertEqual(dgus.crc16_modbus(b"123456789"), 0x4B37)

    def test_cfg_touch_kill_detected(self):
        cfg = bytearray(0x30)
        self.assertFalse(dgus.cfg_kills_touch(bytes(cfg)))
        cfg[0x20], cfg[0x21] = 0x5A, 0x10
        self.assertTrue(dgus.cfg_kills_touch(bytes(cfg)))

    def test_replace_entry_out_of_range_rejected(self):
        icl = dgus.Icl(b"\x04\x00", (None,))
        with self.assertRaises(dgus.DgusFormatError):
            dgus.replace_entry(icl, 5, dgus.IclEntry(1, 1, 0, b""))


@unittest.skipUnless(HAVE_WORK, f"작업 원본 없음: {WORK}")
class TestRealData(unittest.TestCase):
    def test_icl_round_trip_is_byte_identical(self):
        files = glob.glob(os.path.join(WORK, "*", "**", "*.icl"), recursive=True)
        self.assertTrue(files)
        for f in files:
            b = _read(f)
            self.assertEqual(dgus.build_icl(dgus.parse_icl(b)), b, f)

    def test_corrupted_icl_crc_rejected(self):
        b = bytearray(_read(os.path.join(WORK, "gdsonic", "DWIN_SET", "48.icl")))
        b[-1] ^= 0xFF
        with self.assertRaises(dgus.DgusFormatError):
            dgus.parse_icl(bytes(b))

    def test_mode_icon_check_catches_0x140b(self):
        show = bytearray(_read(os.path.join(WORK, "gdsonic", "DWIN_SET", "14ShowFile.bin")))
        self.assertTrue(all(ok for _, ok, _ in dgus.check_mode_icons(bytes(show))))
        show[0x5747] = 0x0B  # p27 아이콘 VP → 0x140b (09-13 결함 재현)
        bad = [p for p, ok, _ in dgus.check_mode_icons(bytes(show)) if not ok]
        self.assertEqual(bad, [27])

    def _psnr_vs_source(self, src, entry):
        with tempfile.TemporaryDirectory() as tmp:
            out = os.path.join(tmp, "x.jpg")
            with open(out, "wb") as f:
                f.write(dgus.entry_to_jpeg(entry))
            a, b = jpeg_enc.load_rgb(src)[2], jpeg_enc.load_rgb(out)[2]
        mse = sum((x - y) ** 2 for p, q in zip(a, b) for x, y in zip(p, q)) / (len(a) * 3)
        return 10 * math.log10(255 ** 2 / max(mse, 1e-9))

    def test_dgus_format_jpg_is_passed_through_losslessly(self):
        icl = dgus.parse_icl(_read(os.path.join(WORK, "gdsonic", "DWIN_SET", "48.icl")))
        src = os.path.join(WORK, "dongin", "48_icon_red", "11_lv_rd_64x16.jpg")
        e, mode = jpeg_enc.encode_entry(src, icl.entries[11], (64, 16))
        self.assertEqual(mode, "passthrough")
        self.assertTrue(_read(src).rstrip(b"\0").endswith(e.body[e.head_len:].rstrip(b"\0")))
        self.assertGreater(self._psnr_vs_source(src, e), 90)  # 같은 계수 → 사실상 동일

    def test_other_images_are_reencoded_with_template_tables(self):
        icl = dgus.parse_icl(_read(os.path.join(WORK, "gdsonic", "DWIN_SET", "48.icl")))
        src = os.path.join(WORK, "dongin", "48_icon_red", "11_lv_rd_64x16.jpg")
        with tempfile.TemporaryDirectory() as tmp:
            png = os.path.join(tmp, "bar.png")
            jpeg_enc.subprocess.run(["sips", "-s", "format", "png", src, "--out", png], capture_output=True, check=True)
            e, mode = jpeg_enc.encode_entry(png, icl.entries[11], (64, 16))
            self.assertEqual(mode, "encode")
            self.assertGreater(self._psnr_vs_source(png, e), MIN_PSNR_DB)
        self._assert_dgus_layout(e, icl.entries[11])

    def _assert_dgus_layout(self, e, template):
        body_len = len(e.body) - e.head_len
        self.assertEqual(e.head_len, dgus.DGUS_JPEG_HEAD_LEN)
        self.assertEqual(e.body[:e.head_len], template.body[:template.head_len])  # 머리 = 툴 출력
        self.assertEqual(body_len % 4, 0)
        trimmed = e.body.rstrip(b"\0")
        self.assertTrue(trimmed.endswith(b"\xff\xd9"))
        self.assertGreaterEqual(len(e.body) - len(trimmed), dgus.MIN_TAIL_PAD)

    def test_wrong_image_size_rejected(self):
        icl = dgus.parse_icl(_read(os.path.join(WORK, "gdsonic", "DWIN_SET", "48.icl")))
        src = os.path.join(WORK, "dongin", "48_icon_red", "11_lv_rd_64x16.jpg")
        with self.assertRaises(dgus.DgusFormatError):
            jpeg_enc.encode_entry(src, icl.entries[11], (800, 480))

    def test_build_changes_only_brand_slots(self):
        with tempfile.TemporaryDirectory() as tmp:
            shutil.copytree(os.path.join(WORK, "gdsonic", "DWIN_SET"), os.path.join(tmp, "gdsonic", "DWIN_SET"))
            os.makedirs(os.path.join(tmp, "newco"))
            logo = os.path.join(WORK, "dongin", "32_bg_dongin", "00_logo.jpg")
            bar = os.path.join(WORK, "dongin", "48_icon_red", "11_lv_rd_64x16.jpg")
            cfg = {"families": {"black": "gdsonic"},
                   "brands": {"gdsonic": {"family": "black", "fw_brand": "GDSONIC"},
                              "newco": {"family": "black", "fw_brand": None, "logo": logo, "bar": bar}}}
            lcdset.build_brand(cfg, tmp, "newco")
            base, new = os.path.join(tmp, "gdsonic", "DWIN_SET"), os.path.join(tmp, "newco", "DWIN_SET")
            self.assertEqual(lcdset.icl_diff_ids(os.path.join(base, "32.icl"), os.path.join(new, "32.icl")), [0])
            self.assertEqual(lcdset.icl_diff_ids(os.path.join(base, "48.icl"), os.path.join(new, "48.icl")), [11])
            self.assertEqual(_read(os.path.join(new, "00_logo.jpg")), _read(logo))
            levels = {lvl for lvl, _ in lcdset.check_brand(cfg, tmp, "newco")}
            self.assertEqual(levels, {"PASS"})
            with self.assertRaises(lcdset.LcdError):  # 덮어쓰기는 --force 필요
                lcdset.build_brand(cfg, tmp, "newco")


if __name__ == "__main__":
    unittest.main(verbosity=2)
