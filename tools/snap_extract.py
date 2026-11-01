"""Pull the situation snapshots that the controller logs at each arbiter / shadow differ ("snap| " lines,
controller\\src\\impl\\situation_snap.cpp LogDiffer) out of an FLController.log into *.snap fixtures, for the host-free
replay: build\\test\\fltest.exe -snaps <out dir>.

    python -I tools\\snap_extract.py <FLController.log | run folder> <out dir>

Prints one line per snapshot (file, its #arbiter / #shadow verdict line); exit 1 when the log has none.
Why: gate s7arb2's M13 differ (30 Oct 2026) could not be explained from its run - no snapshot was kept.
"""
import os
import re
import sys

MARK = re.compile(r"^\[[^\]]*\] snap\| ?(.*)$")


def extract(log_path):
    snaps, cur = [], None
    with open(log_path, encoding="utf-8", errors="replace") as f:
        for raw in f:
            m = MARK.match(raw.rstrip("\r\n"))
            if not m:
                continue
            line = m.group(1)
            if line.startswith("--- differ "):
                cur = []
                snaps.append(cur)
            elif cur is not None:
                cur.append(line)
    return snaps


def main(argv):
    if len(argv) != 3:
        print(__doc__)
        return 2
    src, out = argv[1], argv[2]
    if os.path.isdir(src):
        src = os.path.join(src, "FLController.log")
    snaps = extract(src)
    if not snaps:
        print(f"no snapshots in {src}")
        return 1
    os.makedirs(out, exist_ok=True)
    for i, lines in enumerate(snaps, 1):
        name = os.path.join(out, f"differ_{i:02d}.snap")
        with open(name, "w", encoding="utf-8", newline="\n") as f:
            f.write("\n".join(lines) + "\n")
        verdict = next((l for l in lines if l.startswith("#arbiter ") or l.startswith("#shadow ")), "(no verdict line)")
        print(f"{name}: {verdict[:200]}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
