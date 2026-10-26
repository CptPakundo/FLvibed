"""The review tool's core logic on a synthetic project: fitting typed text to the conventions, one actor's
takes, saving / undoing decisions (bytes on disk), and the shared normalisers."""
import contextlib
import io
import tempfile
import unittest

import fixture
import merge
import review_server

V = fixture.V


class Base(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.d = fixture.data(self.tmp.name)

    def tearDown(self):
        self.tmp.cleanup()

    def ln(self, lid, voice):
        return fixture.line(self.d, lid, voice)


class Fit(Base):
    """review_server.Data.fit: typed text -> the conventions (docs/REVIEW_PROCESS.md section 2)."""

    def fit(self, lid, voice, typed):
        return self.d.fit(self.ln(lid, voice), typed)

    def test_style_rules(self):  # data/style_rules.tsv: "dammit" -> "damn it", "alright" -> "all right"
        text, changes = self.fit("DX_M01_0110_JUNI", "juni", "Alright, dammit, Trent.")
        self.assertEqual(text, "All right, damn it, Trent.")
        self.assertTrue(any("style_rules" in c for c in changes))

    def test_script_name_spelling(self):  # a name typed misspelled takes the script's spelling
        text, changes = self.fit("DX_M01_0100_TRENT", "trent", "We fly to Rhineland now.")
        self.assertEqual(text, "We fly to Rheinland now.")
        self.assertIn("Rhineland → Rheinland", changes[0])

    def test_callsign_digits(self):
        text, _ = self.fit("DX_M01_0110_JUNI", "juni", "Alpha Two, copy that.")
        self.assertEqual(text, "Alpha 2, copy that.")

    def test_story_line_keeps_its_end_mark(self):
        self.assertEqual(self.fit("DX_M01_0110_JUNI", "juni", "Copy that, Trent.")[0], "Copy that, Trent.")

    def test_fragment_and_chunk_form(self):  # a joined piece: no sentence-ending period, lowercase start
        text, changes = self.fit("gcs_misc_thisis+", V, "This is.")
        self.assertEqual(text, "this is")
        self.assertTrue(changes)

    def test_whitespace_cleaned(self):
        self.assertEqual(self.fit("DX_M01_0110_JUNI", "juni", "  Copy \t that,   Trent. ")[0], "Copy that, Trent.")


class LineKinds(Base):
    def test_kinds(self):
        self.assertEqual(self.d.line_kind(self.ln("DX_M01_0100_TRENT", "trent")), "story")
        self.assertEqual(self.d.line_kind(self.ln("gcs_misc_thisis+", V)), "chunk")
        self.assertEqual(self.d.line_kind(self.ln("gcs_combat_cheer_01", "pilot_m_other_m03")), "line")


class Takes(Base):
    """One actor's takes (voice, voice+a/b, _vanilla) read the same unless they differ by more than two words
    at the same clip length (an intentional variant)."""

    def test_family(self):
        self.assertEqual(merge.family("pilot_f_take_m02b_vanilla"), "pilot_f_take_m02")
        self.assertEqual(merge.family("pilot_f_take_m02a"), "pilot_f_take_m02")

    def test_same_line(self):
        self.assertTrue(merge.same_line("Got him!", "Got 'im!"))
        # speech to text lost words on a clip as long as the other: still the same line
        self.assertTrue(merge.same_line("Pull out and head to the gate", "Power", 1.0, 1.1))
        self.assertFalse(merge.same_line("Pull out and head to the gate", "Power", 1.0, 3.0))  # another recording
        self.assertFalse(merge.same_line("Got him, he's going down!", "Splash one, that bandit is history!", 1.0, 1.0))

    def test_takes_of_one_actor(self):
        takes = {x["voice"] for x in self.d.takes(self.ln("gcs_combat_cheer_01", "pilot_f_take_m02"))}
        self.assertEqual(takes, {"pilot_f_take_m02", "pilot_f_take_m02a"})  # not the variant, not the other actor

    def test_story_line_is_its_own_take(self):
        ln = self.ln("DX_M01_0100_TRENT", "trent")
        self.assertEqual(self.d.takes(ln), [ln])


class Saving(Base):
    """Saves go to the corrections file at once (atomic), cover the actor's takes, story lines as voice "*";
    undo restores the file byte for byte."""

    def save(self, lid, voice, text, note=""):
        self.d.undo.append({})
        self.d.save_correction(self.ln(lid, voice), text, "user", note)

    def rows(self):
        return [l.split("\t") for l in self.d.corr_path.read_text(encoding="utf-8").splitlines()
                if l and not l.startswith(("#", "id\t"))]

    def test_save_covers_takes_and_undo_restores_bytes(self):
        before = self.d.corr_path.read_bytes()
        self.save("gcs_combat_cheer_01", "pilot_f_take_m02", "Got him, he's going down!")
        self.assertEqual(sorted(r[1] for r in self.rows()), ["pilot_f_take_m02", "pilot_f_take_m02a"])
        self.assertTrue(all(r[2] == "Got him, he's going down!" and r[6] for r in self.rows()))  # text, by
        self.assertTrue(self.d.decided(self.ln("gcs_combat_cheer_01", "pilot_f_take_m02a")))
        self.d.undo_last()
        self.assertEqual(self.d.corr_path.read_bytes(), before)

    def test_story_line_saved_for_every_voice(self):
        self.save("DX_M01_0100_TRENT", "trent", "We fly to Rheinland now!")
        self.assertEqual([r[:3] for r in self.rows()], [["DX_M01_0100_TRENT", "*", "We fly to Rheinland now!"]])

    def test_reload_reads_what_was_saved(self):
        self.save("DX_M01_0110_JUNI", "juni", "Copy that, Trent!")
        self.d.corr = {}
        self.d.load_corrections()  # what the file holds, read back
        self.assertEqual(self.d.correction_for(self.ln("DX_M01_0110_JUNI", "juni"))[2], "Copy that, Trent!")

    def test_malformed_row_kept_and_reported(self):
        p = self.d.corr_path
        p.write_text(p.read_text(encoding="utf-8") + "x\ty\tz\ts\tn\tw\tb\textra\n", encoding="utf-8", newline="\n")
        err = io.StringIO()
        with contextlib.redirect_stderr(err):
            self.d.load_corrections()
        self.assertIn("8 fields", err.getvalue())
        self.save("DX_M01_0110_JUNI", "juni", "Copy that.")
        self.assertIn("x\ty\tz\ts\tn\tw\tb\textra", p.read_text(encoding="utf-8"))  # not cut, not lost


class Normalisers(unittest.TestCase):
    """The one token normaliser / squash / final-period rule merge.py and the tool share."""

    def test_squash(self):
        self.assertEqual(merge.squash("Got 'im, Alpha-1!"), "gotimalpha1")

    def test_conv_form_and_same_words(self):
        self.assertEqual(merge.conv_form("Alright, A-OK’s"), "all right, a ok's")
        self.assertEqual(review_server.same_words("Alright, Mr. Trent"), review_server.same_words("all right mister trent"))

    def test_strip_final_period(self):
        self.assertEqual(merge.strip_final_period("Roger. "), "Roger")
        self.assertEqual(merge.strip_final_period("Wait..."), "Wait...")
        self.assertEqual(merge.strip_final_period("Go!"), "Go!")


class WordTimings(Base):
    """The aligner's word starts in the tool (word strip) and the suspicion-ranked list."""

    def test_states(self):
        w = self.d.word_info(self.ln("DX_M01_0110_JUNI", "juni"))
        self.assertEqual((w["state"], [x["ms"] for x in w["words"]]), ("current", [0, 1490, -1]))
        self.assertEqual(self.d.word_info(self.ln("DX_M01_0100_TRENT", "trent"))["state"], "stale")
        stale = self.d.word_info(self.ln("DX_M01_0100_TRENT", "trent"))
        self.assertTrue(all(x["ms"] == -1 for x in stale["words"]))  # never show times of another text
        self.assertEqual(self.d.word_info(self.ln("gcs_combat_cheer_01", "pilot_m_other_m03"))["state"], "failed")
        self.assertEqual(self.d.word_info(self.ln("gcs_misc_to", V))["state"], "none")

    def test_fnv_matches_the_aligner(self):  # FNV-1a 32 of the UTF-8 text (align_words.py, the runtime)
        self.assertEqual(review_server.fnv1a(""), "811c9dc5")
        self.assertEqual(review_server.fnv1a("a"), "e40c292c")

    def test_suspects_ranked(self):
        q = self.d.queue({"mode": "suspects"})
        ids = [self.d.lines[n]["id"] for n in q]
        self.assertEqual(ids[:2], ["gcs_combat_cheer_01", "DX_M01_0110_JUNI"])  # failed match first, then piled
        self.assertNotIn("DX_M01_0100_TRENT", ids)  # stale timings are not evidence
        why = self.d.suspects[("dx_m01_0110_juni", "juni")][1]
        self.assertTrue(any("piled" in x for x in why))

    def test_suspect_done_after_a_dated_save(self):
        self.d.undo.append({})
        self.d.save_correction(self.ln("DX_M01_0110_JUNI", "juni"), "Copy that, Trent.", "user", "")
        self.assertNotIn(self.ln("DX_M01_0110_JUNI", "juni")["n"], self.d.queue({"mode": "suspects"}))
        self.assertIn(self.ln("DX_M01_0110_JUNI", "juni")["n"], self.d.queue({"mode": "suspects", "done": "1"}))


class NotSure(Base):
    def test_mark_and_clear(self):
        ln = self.ln("gcs_misc_to", V)
        self.d.set_unsure(ln, True, "  hear   again ")
        self.assertEqual(self.d.queue({"mode": "unsure"}), [ln["n"]])
        self.d.load_unsure()  # read back from data/unsure.tsv
        self.assertEqual(self.d.unsure[("gcs_misc_to", V)]["note"], "hear again")
        self.d.set_unsure(ln, False)
        self.d.load_unsure()
        self.assertEqual(self.d.unsure, {})


class Stats(Base):
    def test_stats_add_up(self):
        s = self.d.stats()
        self.assertEqual(s["lines"], len(fixture.LINES))
        self.assertEqual(sum(s["decided"].values()), len(fixture.LINES))


if __name__ == "__main__":
    unittest.main()
