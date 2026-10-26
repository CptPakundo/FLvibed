"""The publish copy rule (<root>/CLAUDE.md "Subtitle data"): <root>/subtitles/data/{subtitles,movies,word_times}.tsv
are byte-identical copies of the review products in FLSubtitles/data/, written only by tools/publish.ps1 (the
pre-commit hook). Checked in the last commit (git blob ids) and in the working tree (a hand edit of a copy)."""
import shutil
import subprocess
import unittest
from unittest import skipUnless  # (by name: the privacy hook reads "@module.name" as an address)

import fixture

PRODUCTS = ("subtitles.tsv", "movies.tsv", "word_times.tsv")
SRC = "subtitles/review/FLSubtitles/data/"
DST = "subtitles/data/"


def git(*args):
    return subprocess.run(["git", *args], cwd=fixture.ROOT, capture_output=True, text=True, encoding="utf-8")


@skipUnless(shutil.which("git") and (fixture.ROOT / ".git").exists(), "no git repository")
class PublishedCopies(unittest.TestCase):
    def blob(self, path):
        r = git("rev-parse", f"HEAD:{path}")
        return r.stdout.strip() if r.returncode == 0 else None

    def test_committed_copies_identical(self):
        for f in PRODUCTS:
            with self.subTest(f):
                src, dst = self.blob(SRC + f), self.blob(DST + f)
                self.assertIsNotNone(src, f"{SRC + f} not committed")
                self.assertEqual(src, dst, f"{DST + f} differs from the review product in HEAD: run "
                                           "subtitles/review/tools/publish.ps1 (or commit through the hook)")

    def test_working_copies_not_hand_edited(self):
        for f in PRODUCTS:
            with self.subTest(f):
                # a copy may only differ from HEAD while its product does too (staged together by the hook)
                copy_changed = git("diff", "--quiet", "HEAD", "--", DST + f).returncode != 0
                product_changed = git("diff", "--quiet", "HEAD", "--", SRC + f).returncode != 0
                self.assertFalse(copy_changed and not product_changed,
                                 f"{DST + f} was changed by hand: never edit the published copies")


if __name__ == "__main__":
    unittest.main()
