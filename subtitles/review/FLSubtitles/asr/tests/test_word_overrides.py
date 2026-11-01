"""Hand-placed word timings (word_overrides.py): storing them, and carrying them over an edited text (unchanged words
keep their spot and length; changed ones are estimated in their gap and listed for the aligner)."""
import tempfile
import unittest
from pathlib import Path

import tsvio
import word_overrides as wo

TEXT = "You think you're safe, but you're not."
STARTS = [100, 300, 600, 900, 1500, 1700, 1900]
ENDS = [300, 600, 900, 1300, 1700, 1900, 2300]


class Carry(unittest.TestCase):
    def test_punctuation_and_capitals_keep_everything(self):
        s, e, todo = wo.carry(TEXT, STARTS, ENDS, "You think you're safe? But you're not!")
        self.assertEqual((s, e, todo), (STARTS, ENDS, []))

    def test_replaced_word_inside_its_old_time(self):
        s, e, todo = wo.carry(TEXT, STARTS, ENDS, "You think you're sound, but you're not.")
        self.assertEqual(todo, [3])
        self.assertEqual((s[3], e[3]), (900, 1300))
        self.assertEqual(s[:3] + s[4:], STARTS[:3] + STARTS[4:])

    def test_insert_between_touching_words_widens_to_neighbours(self):
        s, e, todo = wo.carry(TEXT, STARTS, ENDS, "You really think you're safe, but you're not.")
        self.assertIn(1, todo)
        self.assertTrue(set(todo) <= {0, 1, 2})  # only the neighbours join, nothing further
        self.assertEqual(s[3:], STARTS[2:])  # the rest stays exactly where it was
        self.assertTrue(all(x >= 100 for x in s[:3]) and s[0] < s[1] < s[2] <= 600)

    def test_insert_into_a_pause_stays_there(self):
        s, e, todo = wo.carry(TEXT, STARTS, ENDS, "You think you're safe, well, but you're not.")
        self.assertEqual(todo, [4])
        self.assertTrue(1300 <= s[4] < e[4] <= 1500)
        self.assertEqual(s[5:], STARTS[4:])

    def test_deleted_word_drops_out(self):
        s, e, todo = wo.carry(TEXT, STARTS, ENDS, "You think you're safe, you're not.")
        self.assertEqual((s, todo), (STARTS[:4] + STARTS[5:], []))


class Store(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.path = Path(self.tmp.name) / "word_overrides.tsv"
        self.o = wo.Overrides(self.path)

    def tearDown(self):
        self.tmp.cleanup()

    def test_put_get_reload(self):
        self.o.put("mix_a", "mix", TEXT, STARTS, ENDS, "user")
        got = wo.Overrides(self.path).get("MIX_A", "mix", TEXT)
        self.assertEqual((got["starts"], got["ends"], got["end_ms"]), (STARTS, ENDS, 2300))
        self.assertIsNone(self.o.get("mix_a", "mix", TEXT + " Really."))  # another text: the aligner's

    def test_bad_timings_refused(self):
        with self.assertRaises(ValueError):
            self.o.put("x", "v", "a b", [100, 50], [200, 150], "user")  # b starts before a ends
        with self.assertRaises(ValueError):
            self.o.put("x", "v", "a b", [100], [200], "user")  # one start per word

    def test_retext_moves_the_override(self):
        self.o.put("x", "v", TEXT, STARTS, ENDS, "user")
        self.assertTrue(self.o.retext("x", "v", TEXT, "You think you're sound, but you're not.", "user"))
        self.assertIsNone(self.o.get("x", "v", TEXT))
        got = self.o.get("x", "v", "You think you're sound, but you're not.")
        self.assertEqual(got["realign"], [3])
        rows = tsvio.read_tsv(self.path)
        self.assertEqual((len(rows), rows[0]["realign"]), (1, "3"))

    def test_drop(self):
        self.o.put("x", "v", TEXT, STARTS, ENDS, "user")
        self.assertTrue(self.o.drop("x", "v", TEXT))
        self.assertEqual(tsvio.read_tsv(self.path), [])


if __name__ == "__main__":
    unittest.main()
