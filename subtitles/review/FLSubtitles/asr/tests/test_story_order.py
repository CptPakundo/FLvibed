"""The story order's derivation (asr/story_order.py) on a small synthetic mission (no game content), and the check that
the committed data/story_order.tsv covers every story line, scene mix and intro movie line of the products."""
import unittest

import fixture  # noqa: F401  (puts asr/ on the path)
import story_order as so
import tsvio

SCRIPT = """
[Mission]
mission_title = 1

[Dialog]
nickname = dlg_b
Line = king, Player, LINE_B1
Line = king, Player, LINE_B2

[Trigger]
nickname = start
InitState = ACTIVE
Cnd_True = no_params
Act_SendComm = king, Player, LINE_A
Act_ActTrig = after_a
Act_ActTrig = king_dies
Act_ActTrig = flag

[Trigger]
nickname = flag
Cnd_Destroyed = nobody, 1

[Trigger]
nickname = after_a
Cnd_CommComplete = LINE_A
Act_StartDialog = dlg_b
Act_ActTrig = reach_far
Act_ActTrig = reach_near
Act_ActTrig = watcher

[Trigger]
nickname = watcher
Cnd_WatchTrigger = flag, ACTIVE
Act_EtherComm = station, 1, Player, LINE_W

[Trigger]
nickname = reach_near
Cnd_DistVec = inside, Player, 0, 0, 0, 500
Act_CallThorn = missions\\m99\\scene.thn
Act_ActTrig = dock

[Trigger]
nickname = reach_far
Cnd_DistVec = inside, Player, 0, 0, 0, 5000
Act_SendComm = king, Player, LINE_C

[Trigger]
nickname = dock
Cnd_BaseEnter = Base_01
Act_AddRTC = missions\\m99\\offer.ini
Act_ChangeState = SUCCEED

[Trigger]
nickname = king_dies
Cnd_Destroyed = king, 1
Act_SendComm = king, Player, LINE_F
Act_ChangeState = FAIL, 1

[Trigger]
nickname = orphan
Cnd_True = no_params
Act_SendComm = king, Player, LINE_ORPHAN
"""

# a decoded scene: the SOUND entities and their START_SOUND events (listed out of time order on purpose)
SCENE = """
entities = {
	{ entity_name = "s_late", type = SOUND, template_name = "SCN_LATE" },
	{ entity_name = "cam", type = CAMERA, template_name = "" },
	{ entity_name = "s_early", type = SOUND, template_name = "SCN_EARLY" },
}
events = {
	{ 2.5, START_SOUND, { "s_late" }, { duration = 1 } },
	{ 0.25, START_SOUND, { "s_early" }, { duration = 1 } },
}
"""
ROOM = """
[CharacterEncounter]
Location = Base_01, Bar
decline = scripts\\story\\decline.thn
accept = scripts\\story\\accept.thn
offer = scripts\\story\\offer.thn
"""


def one(name):
    return '{ entity_name = "e", type = SOUND, template_name = "%s" }\n{ 0.5, START_SOUND, { "e" } }' % name


class FakeGame:
    files = {"missions\\m99\\m99.ini": SCRIPT, "missions\\m99\\offer.ini": ROOM}
    scenes = {"missions\\m99\\scene.thn": SCENE, "scripts\\story\\offer.thn": one("ROOM_OFFER"),
              "scripts\\story\\accept.thn": one("ROOM_ACCEPT"), "scripts\\story\\decline.thn": one("ROOM_DECLINE")}

    def ini(self, rel):
        return self.files.get(rel.lower()), rel

    def thn(self, rel):
        return self.scenes.get(rel.lower()), rel


IDS = ["LINE_A", "LINE_B1", "LINE_B2", "LINE_C", "LINE_W", "LINE_F", "LINE_ORPHAN", "SCN_EARLY", "SCN_LATE",
       "ROOM_OFFER", "ROOM_ACCEPT", "ROOM_DECLINE", "NEVER_USED"]


class StoryOrderDerivation(unittest.TestCase):
    def setUp(self):
        known = {i.lower(): (i, 2.0) for i in IDS}
        self.rows, self.not_placed, self.problems = so.derive(
            FakeGame(), [("M99", "missions\\m99\\m99.ini")], [], known)
        self.ids = [r["id"] for r in self.rows]
        self.by = {r["id"]: r for r in self.rows}

    def test_main_path_order(self):
        main = [r["id"] for r in self.rows if r["play"] == "main"]
        # start's comm, then the dialog after it, the watcher (its flag is armed), the wider approach circle first,
        # the cutscene's lines by time (not by listing), then the room scene: offer before accept
        self.assertEqual(main, ["LINE_A", "LINE_B1", "LINE_B2", "LINE_W", "LINE_C", "SCN_EARLY", "SCN_LATE",
                                "ROOM_OFFER", "ROOM_ACCEPT"])

    def test_branches_marked(self):
        self.assertTrue(self.by["LINE_F"]["play"].startswith("branch: fails the mission"))
        self.assertTrue(self.by["ROOM_DECLINE"]["play"].startswith("branch: if the player declines"))
        # a branch sits right after the point its trigger became active (the start), before the later main lines
        self.assertLess(self.ids.index("LINE_F"), self.ids.index("LINE_C"))

    def test_provenance(self):
        r = self.by["SCN_LATE"]
        self.assertIn("trigger reach_near > Act_CallThorn > scene.thn @ 2.50 s", r["via"])
        self.assertEqual(self.by["LINE_A"]["source"], "missions\\m99\\m99.ini:10")  # the [Trigger] line
        self.assertEqual([r["order"] for r in self.rows], list(range(1, len(self.rows) + 1)))

    def test_not_placed(self):
        self.assertIn("line_orphan", self.not_placed)
        self.assertIn("never activated", self.not_placed["line_orphan"])
        self.assertNotIn("LINE_ORPHAN", self.ids)

    def test_thn_parser(self):
        self.assertEqual(so.parse_thn(SCENE), [(0.25, "SCN_EARLY"), (2.5, "SCN_LATE")])

    def test_intro_movie_first(self):
        lines = [("fl_intro_full", 3.0, 4.0, "full@3"), ("fl_intro", 9.0, 10.0, "hd@9"), ("fl_intro", 2.0, 3.0, "hd@2"),
                 ("other", 1.0, 2.0, "other@1")]
        rows, not_placed = so.intro_rows(["mgs_logo", "fl_intro"], {"fl_intro": "fl_intro.wmv"}, lines)
        self.assertEqual([r["id"] for r in rows], ["hd@2", "hd@9", "full@3"])
        self.assertEqual(rows[0]["play"], "main")
        self.assertTrue(rows[2]["play"].startswith("alternative"))
        self.assertIn("not in this install", rows[2]["section"])
        self.assertIn("other@1", not_placed)
        known = {i.lower(): (i, 2.0) for i in IDS}
        out, _, _ = so.derive(FakeGame(), [("M99", "missions\\m99\\m99.ini")], [], known, rows)
        self.assertEqual(out[0]["id"], "hd@2")  # before the first mission

    def test_movie_id_survives_retiming(self):
        a = so.movie_id("fl_intro", "Commencing pod lockdown.")
        self.assertEqual(a, so.movie_id("fl_intro", " Commencing  pod lockdown. "))  # timing / spacing: same id
        self.assertNotEqual(a, so.movie_id("fl_intro", "Commencing pod lockdown!"))
        self.assertEqual(so.movie_id("fl_intro", "Copy.", 2), so.movie_id("fl_intro", "Copy.") + "-2")


class StoryOrderCoverage(unittest.TestCase):
    def test_every_story_line_placed_or_listed(self):
        """Every story line of subtitles.tsv, and every intro movie line of movies.tsv, is in data/story_order.tsv,
        placed or under "not placed" with a reason (rerun asr/story_order.py when the products change)."""
        order = tsvio.read_tsv(so.OUT_FILE)
        listed = {r["id"].lower() for r in order}
        for r in order:
            if r["mission"] == so.NOT_PLACED:
                self.assertTrue(r["play"], f"{r['id']}: not placed without a reason")
        story = {r["id"].lower() for r in tsvio.read_tsv(so.DATA_DIR / "subtitles.tsv") if r.get("kind") == "story"}
        movies = {mid.lower() for _, _, _, mid in so.movie_lines(so.DATA_DIR / "movies.tsv")}
        missing = sorted((story | movies) - listed)
        self.assertEqual(missing, [], f"{len(missing)} lines missing from story_order.tsv: {missing[:10]}")


if __name__ == "__main__":
    unittest.main()
