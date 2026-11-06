#!/usr/bin/env python
"""The doc reference check: every backticked repository path in a tracked .md file must exist.

Two kinds of backticked tokens are checked (the whole token, else its first word):
- rooted paths, whose first part is a top-level folder or file of the repository (`docs\\TESTING.md`,
  `tools\\harness\\flctl.ps1`, `..\\docs\\kb\\`): they must resolve against the repository root, the doc's own folder
  or a module folder (controller\\, subtitles\\, subtitles\\review\\); `..\\` from the root reaches the environment
  folder (`FLvibed\\...` counts from there). `x\\name` without an extension also counts as `x\\name.*`.
- other paths with a file extension or a trailing slash (`missions\\Leg.ps1`, `notes.md`): written relative to a
  folder named nearby, so they count when any tracked file or folder ends with them, or anything in the git-ignored
  local material on disk (LOCAL_POOL: research\\, the review's work folders, the generated kb tables, the
  environment's hooks).
Paths into absent local material (a fresh clone without research\\ or Install\\) are not judged. Skipped: placeholders
(`<n>`, `YYYY`, `Mxx`, `%..%`, `$..`), URLs, game-data paths (`DATA\\..`, `EXE\\..`, `.ini` / `.cmp` / `.3db` names),
evidence tags (`R2\\frames\\..`, docs\\LEG_DOSSIER.md's shorthand) and fragments (`_combat.cpp`, `.launches.tsv`). Not
checked: docs\\research\\ (its own check-up round, docs\\research\\README.md) and
subtitles\\review\\docs\\SUBTITLE_WORKFLOW.md (a proposal: it names files not built yet).

Every relative markdown link in every tracked .md must reach a file too, docs\\research\\ included.

--export judges the public repository: what `git archive` leaves out (`export-ignore`) does not exist there, so a public doc must
not link to it or name it by a rooted path. tools\\package.ps1 stops a release on it.

Usage (the repository's Python: research\\tools\\ocr\\.venv\\Scripts\\python.exe -I):
    tools\\docrefs.py           list broken references (file:line token); exit 1 when any (tools\\drift_check.ps1 "doc-refs")
    tools\\docrefs.py --export  list the public docs' references into export-ignored files; exit 1 when any
"""
import fnmatch
import glob
import io
import os
import re
import subprocess
import sys
import tarfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXTS = ("md", "ps1", "py", "js", "tsv", "json", "cpp", "h", "txt", "cmd", "html", "lss", "toml", "lock", "mkv", "jpg",
        "png", "mp4", "csv", "log", "fl")
NAMED = ("md", "ps1", "py", "js", "tsv", "json", "cpp", "h", "cmd")  # bare file names judged
LOCAL = ("research", "Install", "build")
LOCAL_POOL = ("research", "subtitles/review/FLSubtitles", "docs/kb/generated", "../.claude", "Install/instances")
# docs\research: its own check-up round; SUBTITLE_WORKFLOW.md and modules\autopilot\docs\A37_PLAN.md (Q6's slices): plans
# naming files that do not exist yet
SKIP_DOCS = ("docs/research/", "subtitles/review/docs/SUBTITLE_WORKFLOW.md", "modules/autopilot/docs/A37_PLAN.md",
             "modules/hud/docs/UI_HUD2_SKIN.md")
SKIP_TOKEN = re.compile(r"[<>{}|=]|YYYY|Mxx|nnn|%|\$|afk\.flag|\w\.\.\w|^R\d[\\/]|^https?:|://|^[A-Z]:\\|\.\.\.|^_|^\.\w|^/|(^|[\\/])EXE[\\/]")
GAME = re.compile(r"^(DATA|EQUIPMENT|SHIPS|MISSIONS|UNIVERSE|AUDIO|FX|AI|SAVE|FLDOCS|AppData|Interface"
                  r"|MaterialAnim|VMesh\w*|Cons|Cmpnd)[\\/]", re.I)
MODULES = ("subtitles/review/",) + tuple(f"modules/{m}/" for m in sorted(os.listdir(os.path.join(ROOT, "modules"))))
GENERATED = ("core_all.h", "yvals_core.h")  # written by build.cmd into build\\<config>\\; the second is the compiler's
LINK = re.compile(r"\]\(([^)\s#]+)(?:#[^)\s]*)?\)")


def tracked():
    out = subprocess.run(["git", "-C", ROOT, "ls-files"], capture_output=True, text=True, check=True).stdout
    return [l for l in out.splitlines() if l]


def local_index():
    """Every path in the git-ignored local material that is on disk (relative to the root, forward slashes)."""
    out = []
    for sub in LOCAL_POOL:
        for d, dirs, fs in os.walk(os.path.normpath(os.path.join(ROOT, sub))):
            dirs[:] = [x for x in dirs if x not in (".venv", "__pycache__", ".git")]
            rel = os.path.relpath(d, ROOT).replace("\\", "/")
            out.append(rel + "/")
            out.extend(rel + "/" + f for f in fs)
    return out


def suffix_hit(p, pool):
    if "*" in p:
        return any(fnmatch.fnmatch(f, p) or fnmatch.fnmatch(f, "*/" + p) for f in pool)
    return any(f == p or f.endswith("/" + p) for f in pool)


def exported(files):
    """The files `git archive` keeps, from the archive itself: check-attr reads `tools/claude/ export-ignore` as nothing."""
    archive = subprocess.run(["git", "-C", ROOT, "archive", "--format=tar", "HEAD"], capture_output=True, check=True).stdout
    with tarfile.open(fileobj=io.BytesIO(archive)) as tar:
        kept = {m.name for m in tar.getmembers() if m.isfile()}
    return kept & set(files)


def relative_links(line):
    # a target with no folder and no extension is C in a sentence (`](cmd)`); ../../releases is GitHub's own page
    return [t for t in LINK.findall(line) if not re.match(r"[a-z]+:", t) and not t.startswith("/") and ("/" in t or "." in t) and "releases/" not in t]


def check_links(files):
    """Relative markdown links that reach nothing."""
    bad = 0
    for doc in files:
        if not doc.endswith(".md"):
            continue
        for n, line in enumerate(open(os.path.join(ROOT, doc), encoding="utf-8", errors="replace"), 1):
            for target in relative_links(line):
                full = os.path.normpath(os.path.join(ROOT, os.path.dirname(doc), target.replace("\\", "/")))
                if not os.path.exists(full):
                    bad += 1
                    print(f"{doc}:{n}  link {target}")
    return bad


def check_export(files):
    """A public doc's links and rooted paths into files the export leaves out."""
    public = exported(files)
    private = [f for f in files if f not in public]
    private_set = set(private)
    names = {f for f in private if "/" in f or f.startswith("HANDOFF")}
    folders = ("tools/claude",)
    bad = 0
    for doc in sorted(public):
        if not doc.endswith(".md") or doc in ("CHANGELOG.md", "docs/research/CHECKUP_HISTORY.md") or doc.startswith("docs/spec/"):
            continue
        for n, line in enumerate(open(os.path.join(ROOT, doc), encoding="utf-8", errors="replace"), 1):
            hits = []
            for target in relative_links(line):
                full = os.path.normpath(os.path.join(os.path.dirname(doc), target.replace("\\", "/"))).replace("\\", "/")
                if full in private_set or full.startswith(folders):
                    hits.append(target)
            for tok in re.findall(r"`([^`\n]+)`", line):
                p = re.sub(r"(#.*|:\d+(-\d+)?)$", "", tok.split(" ")[0].rstrip(".,;:)")).replace("\\", "/")
                while p.startswith("../"):
                    p = p[3:]
                if p in names or p.startswith(tuple(f + "/" for f in folders)) or p in folders:
                    hits.append(tok)
            for hit in hits:
                bad += 1
                print(f"{doc}:{n}  {hit}")
    print(f"export references: {bad} into export-ignored files" if bad else "export references: none into export-ignored files")
    return int(bad > 0)


def main():
    files = tracked()
    if "--export" in sys.argv:
        return check_export(files)
    roots = {f.split("/", 1)[0] for f in files} | set(LOCAL) | {"..", "FLvibed"}
    pool = files + sorted({f.rsplit("/", 1)[0] + "/" for f in files if "/" in f})
    bases_all = [ROOT, os.path.dirname(ROOT)] + [os.path.join(ROOT, m) for m in MODULES]
    local = None
    bad = 0
    for doc in files:
        if not doc.endswith(".md") or doc.startswith(SKIP_DOCS):
            continue
        bases = [os.path.dirname(os.path.join(ROOT, doc))] + bases_all
        text = open(os.path.join(ROOT, doc), encoding="utf-8", errors="replace").read()
        text = re.sub(r"```.*?```", lambda m: "\n" * m.group(0).count("\n"), text, flags=re.S)
        for n, line in enumerate(text.splitlines(), 1):
            for tok in re.findall(r"`([^`\n]+)`", line):
                tok = tok.strip()
                heads = [tok, tok.split(" ")[0]] if " " in tok else [tok]
                heads = [re.sub(r"(#.*|:\d+(-\d+)?)$", "", h.rstrip(".,;:)")) for h in heads]
                h0 = heads[-1]
                if SKIP_TOKEN.search(h0) or SKIP_TOKEN.search(tok) or GAME.search(h0) or not re.match(r"^[\w.\-\\/*]+$", h0):
                    continue
                p0 = h0.replace("\\", "/")
                first = p0.split("/", 1)[0]
                name = p0.rstrip("/").rsplit("/", 1)[-1]
                ext = name.rsplit(".", 1)[-1].lower() if "." in name else ""
                rooted = "/" in p0 and first in roots or p0 in roots
                if not rooted and not (ext in EXTS or p0.endswith("/")):
                    continue
                if not rooted and "/" not in p0 and ext not in NAMED:
                    continue
                ok = False
                for h in heads:
                    p = h.replace("\\", "/")
                    for b in bases:
                        full = os.path.normpath(os.path.join(b, p))
                        if os.path.exists(full) or glob.glob(full if "*" in p else full + ".*"):
                            ok = True
                            break
                    if ok:
                        break
                if not ok and rooted:
                    lead = [x for x in p0.split("/") if x not in ("..", "FLvibed")][0]
                    ok = (lead in LOCAL and not os.path.isdir(os.path.join(ROOT, lead))) or suffix_hit(p0, pool)
                if not ok:
                    if local is None:
                        local = local_index()
                    ok = suffix_hit(p0.lstrip("./"), pool) or suffix_hit(p0.lstrip("./"), local)
                if not ok and name not in GENERATED:
                    bad += 1
                    print(f"{doc}:{n}  {tok}")
    bad += check_links(files)
    print(f"doc references: {bad} broken" if bad else "doc references: all resolve")
    return int(bad > 0)


if __name__ == "__main__":
    sys.exit(main())
