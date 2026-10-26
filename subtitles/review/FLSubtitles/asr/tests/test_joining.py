"""Sentence joining: merge.render_sentence (the review tool's "In game:" preview) against the runtime's C++
JoinPieces fixtures (<root>/subtitles/src/impl/impl.cpp JoinTest, parsed from the source so the two lists can
never drift apart), and the tool's own sentence builder over the game's grammar (docs/chatter_grammar.tsv)."""
import re
import tempfile
import unittest

import fixture
import merge
import review_server

IMPL = fixture.ROOT / "subtitles" / "src" / "impl" / "impl.cpp"
STR = r'"((?:[^"\\]|\\.)*)"'


def unescape(s: str) -> str:
    return re.sub(r"\\(.)", r"\1", s)


def cpp_fixtures():
    """[(name, [(id, text)], expected)] from JoinTest's fixture table."""
    src = IMPL.read_text(encoding="utf-8")
    block = src[src.index("const std::vector<Fixture> fixtures = {"):]
    block = block[:block.index("\n    };")]
    out = []
    for chunk in re.split(r"\n        \{(?=\")", block)[1:]:
        name = unescape(re.match(STR, chunk).group(1))
        pieces = [(i, unescape(t)) for i, t in re.findall(r'\{"([^"]+)", L' + STR + r"\}", chunk)]
        expect = unescape(re.search(r"\}\},\s*L" + STR, chunk).group(1))
        out.append((name, pieces, expect))
    return out


class JoinPiecesFixtures(unittest.TestCase):
    def test_fixtures_parsed(self):
        self.assertGreaterEqual(len(cpp_fixtures()), 4, f"JoinTest's fixture table not found in {IMPL}")

    def test_render_sentence_equals_joinpieces(self):
        for name, pieces, expect in cpp_fixtures():
            with self.subTest(name):
                # JoinPieces decides by the id: a trailing "-" closes the sentence (and the last piece)
                parts = [(t, i.endswith("-") or k == len(pieces) - 1) for k, (i, t) in enumerate(pieces)]
                self.assertEqual(merge.render_sentence(parts), expect)

    def test_rules(self):
        r = merge.render_sentence
        self.assertEqual(r([("roger", True), ("stand by...", True)]), "Roger. Stand by...")
        self.assertEqual(r([("this is", False), ("Alpha 1 dash 2", True)]), "This is Alpha 1-2.")
        self.assertEqual(r([("where are you going?", True), ("hold", True)]), "Where are you going? Hold.")
        self.assertEqual(r([("cut off–", True)]), "Cut off–")
        self.assertEqual(r([("Planet Manhattan", True), ("to Alpha 1", True)]), "Planet Manhattan to Alpha 1.")


class GrammarSentences(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.d = fixture.data(cls.tmp.name)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_grammar_tokens_compile(self):
        roles = merge.load_grammar_roles(merge.GRAMMAR)
        self.assertTrue(roles, "docs/chatter_grammar.tsv gave no roles")
        for builder, toks, _ in self.d.grammar:
            for t in toks:
                if not t.startswith("@"):
                    re.compile(review_server.Data.SLOTS.get(t, t))

    def test_dock_request_as_the_runtime_joins_it(self):
        expect = next(e for n, _, e in cpp_fixtures() if n.startswith("dock request"))
        ln = fixture.line(self.d, "gcs_misc_to", fixture.V)
        got = [s["rendered"] for s in self.d.sentence(ln) if s["builder"] == "06ED6DD0"]
        self.assertEqual(got, [expect])

    def test_story_lines_have_no_sentence(self):
        self.assertEqual(self.d.sentence(fixture.line(self.d, "DX_M01_0100_TRENT", "trent")), [])


if __name__ == "__main__":
    unittest.main()
