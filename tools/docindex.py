#!/usr/bin/env python
"""The docs as a graph (29 Oct 2026; the owner: information organised in an easily accessible, "graphable" way).

Every tracked .md file is a node: its size, last change (commit date), title (first heading), and the docs that refer
to it (a backticked or linked path ending in its folder + name, or its bare name when no other doc has that name).
Generated on demand, never committed (it would be stale after every commit).

Usage (the repository's Python: research\\tools\\ocr\\.venv\\Scripts\\python.exe -I):
    tools\\docindex.py              the index: one line per doc, by folder (size KB, last change, in / out links, title)
    tools\\docindex.py --json F     the graph as JSON (nodes + edges) into F, e.g. for a viewer
    tools\\docindex.py --orphans    docs nothing refers to (exit 1 when any; tools\\drift_check.ps1 "doc-graph")
    tools\\docindex.py --stale N    docs unchanged for N+ days while files in their folder changed since
"""
import json
import os
import re
import subprocess
import sys
from collections import defaultdict
from datetime import date

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ENTRY = ("CLAUDE.md", "README.md")  # the maps: reached from outside (the session start, the repository page)
LOADED = ("tools/claude/agents/", "tools/claude/skills/")  # read by Claude Code itself (tools\claude\README.md)


def git(*args):
    return subprocess.run(["git", "-C", ROOT, *args], capture_output=True, text=True, check=True).stdout


def build():
    files = [f for f in git("ls-files").splitlines() if f]
    docs = [f for f in files if f.endswith(".md")]
    texts = {d: open(os.path.join(ROOT, d), encoding="utf-8", errors="replace").read() for d in docs}
    # last change per file in one pass over the log
    last = {}
    cur = None
    for line in git("log", "--name-only", "--format=@%cs").splitlines():
        if line.startswith("@"):
            cur = line[1:]
        elif line and line not in last:
            last[line] = cur
    names = defaultdict(list)
    for d in docs:
        names[d.rsplit("/", 1)[-1].lower()].append(d)
    edges = set()
    for src, text in texts.items():
        low = text.replace("\\", "/").lower()
        for dst in docs:
            if dst == src:
                continue
            parts = dst.lower().split("/")
            tail2 = "/".join(parts[-2:])
            name = parts[-1]
            hit = tail2 in low if len(parts) > 1 else False
            if not hit and len(names[name]) == 1 and re.search(r"(?<![\w.-])" + re.escape(name) + r"(?![\w-])", low):
                hit = True
            if not hit and len(parts) > 1:   # "`missions\M01a.md`, `M01b.md`": the folder named on the same line
                pat = re.compile(r"(?<![\w.-])" + re.escape(name) + r"(?![\w-])")
                hit = any(parts[-2] + "/" in l and pat.search(l) for l in low.splitlines())
            if not hit and len(parts) > 1 and name in ENTRY:
                hit = re.search(r"(?<![\w.-])" + re.escape(parts[-2] + "/") + r"(?![\w-])", low) is not None
            if hit:
                edges.add((src, dst))
    nodes = []
    for d in docs:
        title = next((l.lstrip("# ").strip() for l in texts[d].splitlines() if l.startswith("#")), "")
        nodes.append({"path": d, "kb": round(len(texts[d].encode("utf-8")) / 1024, 1), "changed": last.get(d, "?"),
                      "in": sorted(s for s, t in edges if t == d), "out": sorted(t for s, t in edges if s == d),
                      "title": title[:80]})
    return files, nodes, sorted(edges), last


def main(argv):
    sys.stdout.reconfigure(encoding="utf-8")   # titles carry non-ASCII (the console's code page cannot print them)
    files, nodes, edges, last = build()
    if "--json" in argv:
        out = argv[argv.index("--json") + 1]
        with open(out, "w", encoding="utf-8") as f:
            json.dump({"nodes": nodes, "edges": [{"from": a, "to": b} for a, b in edges]}, f, indent=1)
        print(f"{len(nodes)} docs, {len(edges)} links -> {out}")
        return 0
    if "--orphans" in argv:
        orphans = [n["path"] for n in nodes if not n["in"] and n["path"].rsplit("/", 1)[-1] not in ENTRY
                   and not n["path"].startswith(LOADED)]
        for o in orphans:
            print(f"orphan: {o}")
        print(f"doc graph: {len(nodes)} docs, {len(edges)} links, {len(orphans)} orphan(s)")
        return int(bool(orphans))
    if "--stale" in argv:
        # A doc is stale when it is N+ days old while code it is about changed since: the code files in its folder, and
        # (29 Oct 2026: the module docs sit in docs\ folders without code, so the folder rule missed them all) every
        # code file the doc names by a file name unique in the repository ("aim.cpp", "flctl.ps1"). A line
        # "Verified against <commit>" (the doc re-read against the code without an edit) dates the doc to that commit.
        days = int(argv[argv.index("--stale") + 1])
        today = date.today()
        code = [f for f in files if f.endswith((".cpp", ".h", ".ps1", ".py", ".js"))]
        byname = defaultdict(list)
        for f in code:
            byname[f.rsplit("/", 1)[-1].lower()].append(f)
        stale = 0
        for n in nodes:
            if n["changed"] == "?":
                continue
            changed = n["changed"]
            text = open(os.path.join(ROOT, n["path"]), encoding="utf-8", errors="replace").read()
            m = re.search(r"Verified against:?\s+`?([0-9a-f]{7,40})", text)
            if m:
                try:
                    changed = max(changed, git("log", "-1", "--format=%cs", m.group(1)).strip() or changed)
                except subprocess.CalledProcessError:
                    pass
            age = (today - date.fromisoformat(changed)).days
            if age < days:
                continue
            folder = n["path"].rsplit("/", 1)[0] if "/" in n["path"] else ""
            about = {f for f in code if folder and f.startswith(folder + "/")}
            low = text.lower()
            for name, paths in byname.items():
                if len(paths) == 1 and re.search(r"(?<![\w.-])" + re.escape(name) + r"(?![\w-])", low):
                    about.add(paths[0])
            newer = sorted((f for f in about if last.get(f, "0") > changed), key=lambda f: last.get(f, ""), reverse=True)
            if newer:
                stale += 1
                print(f"{n['path']}: unchanged {age} days, {len(newer)} code file(s) it is about changed since "
                      f"(newest: {', '.join(f.rsplit('/', 1)[-1] for f in newer[:4])})")
        print(f"stale docs (>= {days} days): {stale}")
        return 0
    folder = None
    for n in sorted(nodes, key=lambda n: (n["path"].rsplit("/", 1)[0] if "/" in n["path"] else "", n["path"])):
        f = n["path"].rsplit("/", 1)[0] if "/" in n["path"] else "."
        if f != folder:
            folder = f
            print(f"\n{folder}/")
        print(f"  {n['path'].rsplit('/', 1)[-1]:<34} {n['kb']:>6.1f} KB  {n['changed']}  in {len(n['in']):>2}  "
              f"out {len(n['out']):>2}  {n['title']}")
    print(f"\n{len(nodes)} docs, {len(edges)} links")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
