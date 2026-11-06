"""The module boundary check (docs\\MODULES_PLAN.md): a module includes only core and its own files.

Reads every kind=file row of docs\\module_map.tsv (file -> owner module), then each listed source's #include lines, and
reports each include that crosses from one module into another. Every source has one owner: a file marked SPLIT or ?, a
source the map does not list, or a file under modules\\<name>\\ owned by another module is a failure of --enforce.

Usage (OCR venv python, -I):
    module_lint.py            report; exit 1 only on a crossing out of a cut module (one with files under modules\\)
    module_lint.py --enforce  exit 1 on any crossing, an unowned file or a file outside its owner's folder (drift_check runs this)
    module_lint.py --list     every crossing, one per line, not only the counts
"""
import collections
import csv
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAP = os.path.join(ROOT, "docs", "module_map.tsv")
INCLUDE = re.compile(r'\s*#\s*include\s+"([^"]+)"')
UNOWNED = {"SPLIT", "?"}
GUARDED_VIEWS = [("modules/hud/src/hud_module.cpp", "(?:hud|skin)"), ("modules/autopilot/src/autopilot_view.cpp", "autopilot")]
FOLDER = {"Autopilot": "autopilot", "Controller": "controller", "Display": "display", "HUD": "hud", "Movies": "movies", "NPC": "npc",
          "Speedrun": "speedrun", "Subtitles": "subtitles"}


def norm(path):
    return path.replace("\\", "/").strip()


def read_map():
    """{repository path: owner} from the kind=file rows."""
    with open(MAP, encoding="utf-8", newline="") as f:
        rows = list(csv.DictReader(f, delimiter="\t"))
    return {norm(r["where"] or r["name"]): r["owner"].strip() for r in rows if r["kind"] == "file"}


def resolve(source, target, owners):
    """The repository path an include names, or None for core and system headers."""
    here = os.path.dirname(source)
    candidate = norm(os.path.normpath(os.path.join(here, target)))
    if candidate in owners:
        return candidate
    base = os.path.basename(target)
    same_folder = norm(os.path.join(here, base))
    if same_folder in owners:
        return same_folder
    matches = [p for p in owners if os.path.basename(p) == base]
    # a bare name shared by two modules (api.h, shm.h) stays inside the includer's own tree
    tree = source.split("/")[0]
    matches = [p for p in matches if p.startswith(tree + "/")] or matches
    return matches[0] if len(matches) == 1 else None


def stale_lines():
    """Rows whose `where` names a line past the end of its file: a move left the map behind (P2's audit found 23)."""
    with open(MAP, encoding="utf-8", newline="") as f:
        rows = list(csv.DictReader(f, delimiter="\t"))
    lengths = {}
    stale = []
    for row in rows:
        match = re.match(r"(.+):(\d+)$", norm(row["where"] or ""))
        if not match:
            continue
        source, number = match.group(1), int(match.group(2))
        if source not in lengths:
            path = os.path.join(ROOT, source)
            if not os.path.exists(path):
                stale.append(f"{row['kind']} {row['name']}: {source} does not exist")
                lengths[source] = None
                continue
            with open(path, encoding="utf-8", errors="replace") as f:
                lengths[source] = sum(1 for _ in f)
        if lengths[source] is not None and number > lengths[source]:
            stale.append(f"{row['kind']} {row['name']}: {source}:{number} is past the end ({lengths[source]} lines)")
    return stale


def unguarded():
    """A fault stops one module only if every way into it is guarded (core\\game\\modslots.h Guard): an entry published bare in a view stops the whole entry."""
    found = []
    for path, view in GUARDED_VIEWS:
        text = open(os.path.join(ROOT, path), encoding="utf-8", errors="replace").read()
        entries = re.findall(r"\bv\.\w+ = ([^,;]+)", text)
        for body in re.findall(r"views::" + view + r"\s*=\s*\{([^}]+)\}", text):
            entries += [e.strip() for e in re.split(r",(?![^(]*\))", body)]
        if not entries:
            found.append(f"{path}: no entries read: this check no longer sees the view")
        found += [f"{path}: {e.strip()}" for e in entries if not e.strip().startswith("G(")]
    return found


def main():
    enforce = "--enforce" in sys.argv
    listing = "--list" in sys.argv
    owners = read_map()
    stale = stale_lines()
    print(f"module_lint: {len(stale)} map rows point past their file")
    for line in stale[:20]:
        print("  " + line)
    pairs = collections.Counter()
    crossings = []
    unowned = 0
    missing = []
    for source, owner in sorted(owners.items()):
        path = os.path.join(ROOT, source)
        if not os.path.exists(path):
            missing.append(source)
            continue
        if owner in UNOWNED:
            unowned += 1
            continue
        with open(path, encoding="utf-8", errors="replace") as f:
            for number, line in enumerate(f, 1):
                match = INCLUDE.match(line)
                if not match:
                    continue
                target = resolve(source, match.group(1), owners)
                if target is None:
                    continue
                to = owners[target]
                if to in UNOWNED or to == owner or to == "core":
                    continue
                pairs[(owner, to)] += 1
                crossings.append(f"{source}:{number} {owner} -> {to} ({os.path.basename(target)})")
    # most of core is not in the map, and a module's header included there is the crossing that matters most
    core_files = subprocess.run(["git", "-C", ROOT, "ls-files", "core"], capture_output=True, text=True).stdout.split("\n")
    for source in core_files:
        if source in owners or not source.endswith((".cpp", ".h")):
            continue
        with open(os.path.join(ROOT, source), encoding="utf-8", errors="replace") as f:
            for number, line in enumerate(f, 1):
                match = INCLUDE.match(line)
                target = resolve(source, match.group(1), owners) if match else None
                if target and target.startswith("modules/"):
                    pairs[("core", owners[target])] += 1
                    crossings.append(f"{source}:{number} core -> {owners[target]} ({os.path.basename(target)})")
    print(f"module_lint: {len(owners)} files mapped, {unowned} without one owner yet, {len(crossings)} crossings")
    for (a, b), count in pairs.most_common():
        print(f"  {a} -> {b}: {count}")
    if listing:
        print("\n".join(crossings))
    for source in missing:
        print(f"  MISSING: {source} (in the map, not on disk)")
    # a cut module (its files are under modules\) reaches the rest only through core, from the day it is cut
    cut = {owner for path, owner in owners.items() if path.startswith("modules/")}
    out_of_cut = [c for c in crossings if c.split(" ")[1] in cut]
    for line in out_of_cut:
        print("  CUT MODULE CROSSES: " + line)
    print(f"module_lint: cut modules {sorted(cut)}, {len(out_of_cut)} crossings out of them")
    # .gitignore is a whitelist: P5's first commit left a module's new files behind, and it still built here
    git = ["git", "-C", ROOT, "ls-files", "--others", "modules"]
    loose = [f for f in subprocess.run(git, capture_output=True, text=True).stdout.split("\n") if f]
    for path in loose:
        print("  NOT TRACKED: " + path)
    misplaced = sorted(f"{p}: owner {o}" for p, o in owners.items() if p.startswith("modules/") and p.split("/")[1] != FOLDER.get(o))
    tracked = subprocess.run(["git", "-C", ROOT, "ls-files", "modules"], capture_output=True, text=True).stdout.split("\n")
    unlisted = sorted(f for f in tracked if f.endswith((".cpp", ".h")) and f not in owners)
    for line in misplaced:
        print("  NOT IN ITS OWNER'S FOLDER: " + line)
    for line in unlisted:
        print("  NOT IN THE MAP: " + line)
    print(f"module_lint: {len(misplaced)} files outside their owner's folder, {len(unlisted)} sources not in the map")
    bare = unguarded()
    for line in bare:
        print("  VIEW ENTRY WITHOUT A GUARD: " + line)
    print(f"module_lint: {len(bare)} view entries without a guard")
    if out_of_cut or loose or bare or (enforce and (crossings or missing or unowned or misplaced or unlisted)):
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
