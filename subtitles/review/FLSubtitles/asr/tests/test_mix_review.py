"""The scene mix lines in the review tool (mix_review.py) on a synthetic data/mixes.tsv: listing, saving, the window
checks, split / insert / delete, fitting to word times, and that a scratch file never starts the aligner."""
import tempfile
import threading
import unittest
from pathlib import Path

import mix_review
import tsvio

ROWS = [
    ["mix_a", "1.00", "3.00", "Trent", "Hello there, Juni.", "[check] heard: hello there juni", ""],
    ["mix_a", "3.50", "6.00", "Juni", "We have to go now.", "", ""],
    ["mix_b", "0.50", "2.00", "King", "Ready.", "", ""],
]


class MixLines(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        root = Path(self.tmp.name)
        (root / "data").mkdir()
        (root / "out").mkdir()
        self.path = root / "data" / "mixes.tsv"
        tsvio.atomic_write(self.path, "# test\n" + "\t".join(mix_review.HEADER) + "\n" +
                           "".join("\t".join(r) + "\n" for r in ROWS))
        # word rows of the first line (the aligner's cache format): starts at 1.10 / 1.60 / 2.20 s, ends 2.80 s
        tsvio.atomic_write(root / "out" / "word_times_cache.tsv",
                           "id\tvoice\ttext_fnv\taudio_sha1\tend_ms\tstarts_ms\tcollapsed\n"
                           f"mix_a\tmix\t{mix_review.fnv1a(ROWS[0][4])}\tx\t2800\t1100,1600,2200\t0\n")
        clips = {"mix_a": {"n": 7, "sec": 10.0}, "mix_b": {"n": 8, "sec": 4.0}}
        self.m = mix_review.MixLines(self.path, root / "out", clips, threading.Lock())

    def tearDown(self):
        self.tmp.cleanup()

    def n(self, k):
        return mix_review.MixLines.n_of(k)

    def test_scratch_file_never_aligns(self):
        self.assertFalse(self.m.aligner.enabled)

    def test_queue_and_counts(self):
        self.assertEqual(self.m.queue(False), [self.n(0), self.n(1), self.n(2)])
        self.assertEqual(self.m.open_count(), 3)
        self.assertEqual(self.m.queue(False, "juni"), [self.n(0), self.n(1)])

    def test_item(self):
        it = self.m.item(self.n(0))
        self.assertEqual((it["clip"], it["index"], it["count"]), (7, 1, 2))
        self.assertEqual(it["heard"], "hello there juni")
        self.assertEqual(it["words"]["state"], "current")
        self.assertEqual([w["ms"] for w in it["words"]["words"]], [1100, 1600, 2200])
        self.assertEqual(self.m.item(self.n(1))["words"]["state"], "none")

    def test_save_marks_reviewed_and_writes(self):
        r = self.m.edit(self.n(1), {"action": "save", "text": " We  have to go! ", "speaker": "Juni",
                                    "start": 3.4, "end": 6.1}, "user")
        self.assertTrue(r["ok"])
        rows = tsvio.read_tsv(self.path)
        self.assertEqual(rows[1]["text"], "We have to go!")
        self.assertEqual((rows[1]["start"], rows[1]["end"]), ("3.40", "6.10"))
        self.assertTrue(rows[1]["reviewed"].startswith("user "))
        self.assertEqual(self.m.queue(False), [self.n(0), self.n(2)])

    def test_bad_window_refused(self):
        self.assertIn("error", self.m.edit(self.n(2), {"action": "save", "text": "Ready.", "start": 2, "end": 1}, "u"))
        self.assertIn("error", self.m.edit(self.n(2), {"action": "save", "text": "Ready.", "start": 5, "end": 9}, "u"))
        self.assertIn("error", self.m.edit(self.n(2), {"action": "save", "text": " ", "start": 0.5, "end": 2}, "u"))

    def test_last_line_end_past_the_clip_is_trimmed(self):  # a mix's last line could not be saved (owner, 1 Nov 2026)
        self.assertTrue(self.m.edit(self.n(2), {"action": "save", "text": "Ready.", "start": 1, "end": 4.08}, "u")["ok"])
        self.assertEqual(tsvio.read_tsv(self.path)[2]["end"], "4.00")  # mix_b's clip is 4.0 s long

    def test_one_word_line_cannot_be_split(self):
        self.assertIn("one-word", self.m.edit(self.n(2), {"action": "split", "at": 1}, "u")["error"])

    def test_split_at_a_timed_word(self):
        r = self.m.edit(self.n(0), {"action": "split", "at": 2}, "user")
        self.assertTrue(r["ok"] and r["structure"])
        self.assertEqual(r["n"], self.n(1))  # the new second line is the one shown (and played from its start)
        rows = tsvio.read_tsv(self.path)
        self.assertEqual([x["text"] for x in rows[:2]], ["Hello there,", "Juni."])
        self.assertEqual((rows[0]["end"], rows[1]["start"], rows[1]["end"]), ("2.20", "2.20", "3.00"))

    def test_insert_and_delete(self):
        r = self.m.edit(self.n(0), {"action": "insert"}, "user")
        rows = tsvio.read_tsv(self.path)
        self.assertEqual(len(rows), 4)
        self.assertEqual((rows[1]["start"], rows[1]["end"], rows[1]["text"]), ("3.00", "3.50", ""))
        self.assertEqual(r["n"], self.n(1))
        self.m.edit(self.n(1), {"action": "delete"}, "user")
        self.assertEqual([x["text"] for x in tsvio.read_tsv(self.path)], [r[4] for r in ROWS])

    def test_fit_window_to_words(self):
        self.m.edit(self.n(0), {"action": "fit"}, "user")
        row = tsvio.read_tsv(self.path)[0]
        self.assertEqual((row["start"], row["end"]), ("1.10", "3.00"))  # end 2.8 + 0.2 s
        self.assertIn("error", self.m.edit(self.n(1), {"action": "fit"}, "user"))  # no word times


if __name__ == "__main__":
    unittest.main()
