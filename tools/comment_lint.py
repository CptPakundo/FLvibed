"""The comment standard (docs\\COMMENTS.md) as a report (OCR venv python, -I).

Over core, modules\\controller\\src, modules\\subtitles\\src and modules: a source file without a header comment before its first #include, and comment
lines with a dated narrative or an owner's quote. Counts per folder; the worst files with --list. Report only: exit 0.

Usage: comment_lint.py [--list] [--headers]   (--headers: every file without a header)
"""
import collections
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FOLDERS = ("core", "modules")
MONTH = r"(?:Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)[a-z]*"
DATED = re.compile(r"\b\d{1,2} " + MONTH + r"(?: 20\d\d)?\b")
QUOTE = re.compile(r"\((?:the )?(?:owner|user)\b|\b(?:the owner|the user)(?:'s)?,? \d|\bthe owner:")


def sources():
    for folder in FOLDERS:
        base = os.path.join(ROOT, folder)
        for path, dirs, files in os.walk(base):
            dirs[:] = [d for d in dirs if d not in ("review", "tools", "docs", "data")]
            for name in files:
                if name.endswith((".cpp", ".h")):
                    yield folder, os.path.join(path, name)


def check(path):
    header = False
    dated = quotes = 0
    seen_code = False
    for line in open(path, encoding="utf-8", errors="replace"):
        s = line.strip()
        if not seen_code and s.startswith("//"):
            header = True
        if s.startswith(("#include", "#pragma")) is False and s and not s.startswith("//"):
            seen_code = True
        if s.startswith("#include"):
            seen_code = True
        at = line.find("//")
        if at >= 0:
            c = line[at:]
            dated += 1 if DATED.search(c) else 0
            quotes += 1 if QUOTE.search(c) else 0
    return header, dated, quotes


def main():
    listing = "--list" in sys.argv
    per = collections.defaultdict(lambda: [0, 0, 0, 0])
    rows = []
    for folder, path in sources():
        header, dated, quotes = check(path)
        p = per[folder]
        p[0] += 1
        p[1] += 0 if header else 1
        p[2] += dated
        p[3] += quotes
        rows.append((dated + quotes, os.path.relpath(path, ROOT), header, dated, quotes))
    print("comment_lint: files, no header, dated lines, owner quotes")
    for folder in sorted(per):
        p = per[folder]
        print(f"  {folder:18} {p[0]:5} {p[1]:5} {p[2]:6} {p[3]:6}")
    if "--headers" in sys.argv:
        for row in sorted(rows, key=lambda r: r[1]):
            if not row[2]:
                print(row[1].replace(os.sep, "/"))
    if listing:
        for n, rel, header, dated, quotes in sorted(rows, reverse=True)[:30]:
            print(f"  {rel}: dated {dated}, quotes {quotes}{'' if header else ', NO HEADER'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
