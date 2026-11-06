"""docs\\module_map.tsv's line numbers, re-derived from the tree (OCR venv python, -I). Moves shift every row after them: P2, P3 and P4
each left rows behind (tools\\module_lint.py names those past their file's end; the rest go wrong silently).

For each row whose `where` is file:line, the row's own key is looked for in that file: a command's verb in quotes, a hook's
`exe::Name` at its Detour, a HostApi / ImplApi member's `(*Name)`, a setting's name in quotes, a frame row's first identifier that the file
still holds in code. Found: the row gets that line. Not found: the row keeps its file and loses its number, which is honest. (Frame rows were
left alone until P9: each package left them past their file's end.)

Usage: map_lines.py          report how many rows would move, exit 0
       map_lines.py --fix    write them
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAP = os.path.join(ROOT, "docs", "module_map.tsv")
TEXT = {}


def lines(source):
    if source not in TEXT:
        path = os.path.join(ROOT, source.replace("/", os.sep))
        TEXT[source] = open(path, encoding="utf-8", errors="replace").read().split("\n") if os.path.exists(path) else None
    return TEXT[source]


def keys(kind, name):
    if kind == "command":
        return ['"' + name.split()[0] + '"']
    if kind == "setting":
        return ['"' + name.split(".")[-1] + '"']
    if kind in ("hostapi", "implapi"):
        member = name.split(".")[-1]
        return ["(*" + member + ")", member + ";"]
    if kind == "hook":
        m = re.search(r"exe::(\w+)", name)
        if m:
            return ["hk::Detour(exe::" + m.group(1), "exe::" + m.group(1)]
        m = re.search(r"!(\w+)", name)
        return [m.group(1)] if m else []
    if kind == "frame":
        words = re.findall(r"[A-Za-z_]\w*(?:::[A-Za-z_]\w*)*", name)
        words = [w for w in words if len(w) > 3 and w not in ("HostApi", "ImplApi", "return")]
        return words + [w.split("::")[-1] for w in words if "::" in w]
    return []


def main():
    fix = "--fix" in sys.argv
    raw = open(MAP, encoding="utf-8", newline="").read()
    nl = "\r\n" if "\r\n" in raw else "\n"
    rows = raw.replace("\r\n", "\n").rstrip("\n").split("\n")
    moved = lost = 0
    out = []
    for line in rows:
        c = line.split("\t")
        m = re.match(r"(.+):(\d+)$", c[2]) if len(c) >= 3 else None
        ks = keys(c[0], c[1]) if m else []
        text = lines(m.group(1)) if m else None
        if ks and text is not None:
            hit = next((i for k in ks for i, l in enumerate(text, 1) if k in l and not (c[0] == "frame" and l.lstrip().startswith("//"))), 0)
            where = f"{m.group(1)}:{hit}" if hit else m.group(1)
            if where != c[2]:
                moved += 1 if hit else 0
                lost += 0 if hit else 1
                c[2] = where
        out.append("\t".join(c))
    print(f"map_lines: {moved} rows move, {lost} lose their number")
    if fix:
        open(MAP, "w", encoding="utf-8", newline="").write(nl.join(out) + nl)
    return 0


if __name__ == "__main__":
    sys.exit(main())
