"""Runs the review tool's unit tests (stdlib unittest; a synthetic project, no game content, no audio, no server).

    subtitles\\review\\review.cmd test
    <python> -I subtitles\\review\\FLSubtitles\\asr\\tests\\run_tests.py [-v]

Exit code 0 = all passed. One line per failure plus the summary; -v lists every test.
"""
import sys
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent))

if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    sys.stderr.reconfigure(encoding="utf-8")
    suite = unittest.defaultTestLoader.discover(str(HERE), pattern="test_*.py", top_level_dir=str(HERE))
    res = unittest.TextTestRunner(verbosity=2 if "-v" in sys.argv else 1).run(suite)
    sys.exit(0 if res.wasSuccessful() else 1)
