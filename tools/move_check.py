r"""The proof that a commit moved code and changed none (OCR venv python, -I): the code lines of two revisions, compared as one multiset.

Comments and blank lines are dropped and spacing is normalised, so a comment edit is no difference and a line moved to another file is
no difference. What stays is what a move really had to change: a `static` that went, a declaration in a header, includes, namespace
lines. Each such line is printed, so a reader judges a short list instead of a long diff. A leading `static ` is compared without it
and counted apart.

Usage: move_check.py <rev>            <rev> against the working tree
       move_check.py <rev-a> <rev-b>  two revisions
       move_check.py ... --quiet      counts only
Exit 0 when no code line differs; 1 otherwise (a split always prints its declarations: read them).
"""
import collections
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FOLDERS = ("core", "modules", "tests")
GENERATED = ("core/ui_meta.h",)


def git(*args):
    return subprocess.run(["git", "-C", ROOT, *args], capture_output=True, check=True).stdout


def strip_comment(line):
    """The line without a // comment, strings left alone."""
    quote = None
    i = 0
    while i < len(line):
        c = line[i]
        if quote:
            if c == "\\":
                i += 1
            elif c == quote:
                quote = None
        elif c in "\"'":
            quote = c
        elif line.startswith("//", i):
            return line[:i]
        i += 1
    return line


def code_lines(text):
    out = collections.Counter()
    block = False
    for raw in text.split("\n"):
        line = raw
        if block:
            end = line.find("*/")
            if end < 0:
                continue
            line = line[end + 2:]
            block = False
        line = strip_comment(line)
        start = line.find("/*")
        if start >= 0 and '"' not in line[:start]:
            end = line.find("*/", start)
            if end < 0:
                block = True
                line = line[:start]
            else:
                line = line[:start] + line[end + 2:]
        line = " ".join(line.split())
        if line:
            out[line] += 1
    return out


def sources(rev):
    if rev is None:
        names = git("ls-files", *FOLDERS).decode().split("\n")
        names += git("ls-files", "--others", "--exclude-standard", *FOLDERS).decode().split("\n")
    else:
        names = git("ls-tree", "-r", "--name-only", rev, *FOLDERS).decode().split("\n")
    total = collections.Counter()
    for name in names:
        if not name.endswith((".cpp", ".h")) or name in GENERATED:
            continue
        if rev is None:
            path = os.path.join(ROOT, name)
            if not os.path.exists(path):
                continue
            with open(path, "rb") as h:
                data = h.read()
        else:
            data = git("show", f"{rev}:{name}")
        total.update(code_lines(data.decode("utf-8", "replace").replace("\r", "")))
    return total


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    quiet = "--quiet" in sys.argv
    if not args or len(args) > 2:
        print(__doc__)
        return 2
    old = sources(args[0])
    new = sources(args[1] if len(args) == 2 else None)
    gone = old - new
    came = new - old
    # `static float X(...)` that became `float X(...)`
    unstatic = 0
    for line in list(gone):
        if line.startswith("static ") and came[line[7:]]:
            n = min(gone[line], came[line[7:]])
            gone[line] -= n
            came[line[7:]] -= n
            unstatic += n
    gone += collections.Counter()
    came += collections.Counter()
    print(f"move_check: {sum(gone.values())} code lines gone, {sum(came.values())} new, {unstatic} lost `static`")
    if not quiet:
        for line, n in sorted(gone.items()):
            print(f"  - {line[:150]}" + (f"  x{n}" if n > 1 else ""))
        for line, n in sorted(came.items()):
            print(f"  + {line[:150]}" + (f"  x{n}" if n > 1 else ""))
    return int(bool(gone or came))


if __name__ == "__main__":
    sys.exit(main())
