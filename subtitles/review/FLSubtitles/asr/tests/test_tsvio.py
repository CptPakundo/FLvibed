"""tsvio: defensive reading (never a crash on one bad row) and atomic writing (never half a file)."""
import contextlib
import io
import tempfile
import unittest
from pathlib import Path

import fixture  # noqa: F401  (puts asr/ on the path)
import tsvio


class ReadTsv(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.dir = Path(self.tmp.name)
        tsvio.PROBLEMS.clear()

    def tearDown(self):
        self.tmp.cleanup()

    def read(self, raw: bytes, **kw):
        p = self.dir / "t.tsv"
        p.write_bytes(raw)
        err = io.StringIO()
        with contextlib.redirect_stderr(err):
            rows = tsvio.read_tsv(p, **kw)
        return rows, err.getvalue()

    def test_plain(self):
        rows, err = self.read(b"# comment\na\tb\n1\t2\n\n3\t4\n")
        self.assertEqual(rows, [{"a": "1", "b": "2"}, {"a": "3", "b": "4"}])
        self.assertEqual(err, "")

    def test_short_row_padded_silently(self):
        rows, err = self.read(b"a\tb\tc\n1\n")
        self.assertEqual(rows, [{"a": "1", "b": "", "c": ""}])
        self.assertEqual(err, "")

    def test_long_row_reported_with_line(self):
        rows, err = self.read(b"# c\na\tb\n1\t2\t3\n")
        self.assertEqual(rows, [{"a": "1", "b": "2"}])
        self.assertIn("t.tsv:3:", err)
        self.assertEqual(tsvio.PROBLEMS[0][1], 3)

    def test_bad_bytes_replaced_and_reported(self):
        rows, err = self.read(b"a\tb\nok\tfine\nbad\t\xff\xfe\n")
        self.assertEqual(rows[0], {"a": "ok", "b": "fine"})
        self.assertEqual(rows[1]["a"], "bad")
        self.assertIn("t.tsv:3:", err)

    def test_bom_and_crlf(self):
        rows, _ = self.read("﻿a\tb\r\n1\t2\r\n".encode("utf-8"))
        self.assertEqual(rows, [{"a": "1", "b": "2"}])

    def test_empty_file(self):
        rows, err = self.read(b"")
        self.assertEqual(rows, [])
        self.assertIn("empty", err)

    def test_comments_kept_when_asked(self):
        rows, _ = self.read(b"a\n#x\n", skip_comments=False)
        self.assertEqual(rows, [{"a": "#x"}])

    def test_missing_column_stops_clearly(self):
        with self.assertRaises(SystemExit) as e:
            tsvio.require_columns(Path("x.tsv"), [{"a": "1"}], ("a", "b"))
        self.assertIn("b", str(e.exception))


class AtomicWrite(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.p = Path(self.tmp.name) / "decisions.tsv"

    def tearDown(self):
        self.tmp.cleanup()

    def test_bytes_as_plain_write(self):
        tsvio.atomic_write(self.p, "a\tb\n1\t2\n")
        self.assertEqual(self.p.read_bytes(), b"a\tb\n1\t2\n")
        tsvio.atomic_write(self.p, "x\n", newline=None)  # platform line ends, like Path.write_text
        plain = Path(self.tmp.name) / "plain.txt"
        plain.write_text("x\n", encoding="utf-8")
        self.assertEqual(self.p.read_bytes(), plain.read_bytes())

    def test_failure_keeps_the_old_file(self):
        self.p.write_bytes(b"old\n")
        with self.assertRaises(RuntimeError):
            with tsvio.atomic_open(self.p) as fo:
                fo.write("half a fi")
                raise RuntimeError("crash mid-write")
        self.assertEqual(self.p.read_bytes(), b"old\n")
        self.assertEqual([x.name for x in self.p.parent.iterdir()], ["decisions.tsv"])  # no temp file left


if __name__ == "__main__":
    unittest.main()
