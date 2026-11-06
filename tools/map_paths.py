r"""After a cut: every path of a moved file, rewritten from docs\module_map.tsv (OCR venv python, -I).

A kind=file row whose file is gone from disk, and whose name is on disk once under modules\ or core\, is a move. The script then rewrites
that path in the tracked docs, scripts and tables, and in the map's own rows. History stays as it was written (SKIP). Line numbers
are tools\map_lines.py's: run it after this, with --fix. A row with no single new home is named and left alone.

Usage: map_paths.py          report the moves, write nothing
       map_paths.py --fix    write them
"""
import csv
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAP = os.path.join(ROOT, "docs", "module_map.tsv")
SKIP = ("HANDOFF_history.txt", "CHANGELOG.md", "CHECKUP_HISTORY.md", "tests/fingerprints", "docs/module_map.tsv", "subtitles/review", "docs/MODULES_PLAN.md")
KINDS = ("*.md", "*.txt", "*.ps1", "*.py", "*.js", "*.json", "*.tsv", "*.h", "*.cpp") # source comments go stale too: P4 and P5 each left some


def tracked(*patterns):
    out = subprocess.run(["git", "-C", ROOT, "ls-files", *patterns], capture_output=True, text=True).stdout
    return [line for line in out.split("\n") if line]


def moves():
    with open(MAP, encoding="utf-8", newline="") as f:
        rows = [r for r in csv.DictReader(f, delimiter="\t") if r["kind"] == "file"]
    homes = {}
    for path in tracked("modules/*", "core/*"):
        if os.path.exists(os.path.join(ROOT, path)):
            homes.setdefault(os.path.basename(path), []).append(path)
    found, lost = [], []
    for row in rows:
        old = row["where"].replace("\\", "/").strip()
        if os.path.exists(os.path.join(ROOT, old)):
            continue
        home = homes.get(os.path.basename(old), [])
        if len(home) == 1:
            found.append((old, home[0]))
        else:
            lost.append(old)
    return found, lost


def rewrite(text, rel, found):
    for old, new in found:
        for a, b in ((old, new), (old.replace("/", "\\"), new.replace("/", "\\")), (old.replace("/", "\\\\"), new.replace("/", "\\\\"))):
            text = text.replace(a, b)
        # inside a module's own tree its docs write paths from its root: `src\impl\x.cpp` in controller\ is ..\ from the new home
        tree = old.split("/")[0] + "/"
        if rel.startswith(tree) and old.startswith(tree):
            short, target = old[len(tree):], "../" + new
            for a, b in ((short, target), (short.replace("/", "\\"), target.replace("/", "\\"))):
                text = re.sub(r"(?<![\w/\\.])" + re.escape(a), lambda m: b, text)
            # docs one folder below the tree's root: `..\src\impl\x.h`
            if rel.count("/") >= 2:
                up = "../" + old[len(tree):]
                for a, b in ((up, "../../" + new), (up.replace("/", "\\"), ("../../" + new).replace("/", "\\"))):
                    text = text.replace(a, b)
        # a stem in a doc: `x.*` for x.h and x.cpp
        stem_old, stem_new = old.rsplit(".", 1)[0] + ".*", new.rsplit(".", 1)[0] + ".*"
        for a, b in ((stem_old, stem_new), (stem_old.replace("/", "\\"), stem_new.replace("/", "\\"))):
            text = text.replace(a, b)
    return text


def main():
    fix = "--fix" in sys.argv
    found, lost = moves()
    for old, new in found:
        print(f"  {old} -> {new}")
    for old in lost:
        print(f"  NO SINGLE HOME: {old}")
    changed = 0
    for rel in tracked(*KINDS):
        if any(s in rel for s in SKIP):
            continue
        path = os.path.join(ROOT, rel.replace("/", "\\"))
        try:
            raw = open(path, encoding="utf-8", newline="").read()
        except (UnicodeDecodeError, FileNotFoundError):
            continue
        text = rewrite(raw, rel, found)
        if text != raw:
            changed += 1
            if fix:
                open(path, "w", encoding="utf-8", newline="").write(text)
    raw = open(MAP, encoding="utf-8", newline="").read()
    out = []
    for line in raw.split("\n"):
        cells = line.split("\t")
        if len(cells) >= 3:
            for old, new in found:
                if cells[2].startswith(old):
                    cells[2] = new + cells[2][len(old):]
        out.append("\t".join(cells))
    if fix:
        open(MAP, "w", encoding="utf-8", newline="").write("\n".join(out))
    print(f"map_paths: {len(found)} moves, {len(lost)} without a home, {changed} files {'rewritten' if fix else 'would change'}")
    return 1 if lost else 0


if __name__ == "__main__":
    sys.exit(main())
