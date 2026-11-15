"""The game's recorded frames (`hud2 bench record`, modules/hud/src/hud2_record.h; tests/bench_record.ps1) checked.

  record_check.py check <dir>          every record in <dir> whole and tied to its list; its over pass against the kit
                                       dumps taken with the recorder on and off (<state>_on.txt, <state>_off.txt)
  record_check.py compare <dirA> <dirB> the records of two game starts: per state whether the lists are equal, and which
                                       record members differ (the start-to-start noise, by name and count)

check exits 1 on a record that is not whole (unreadable, not flstate/0, no list beside it, the list's hash or its
inputs' fingerprint not the record's, Update not a live frame, Render not drawn, fewer than six layer prints) and on a
record whose over pass is in neither dump; a frame found in one dump only is reported (a state whose draw moves from
frame to frame, as some sims' fades do), not failed. compare always exits 0: it names, it does not judge.
Run with the project's Python: research\\tools\\ocr\\.venv\\Scripts\\python.exe -I tools\\hudbench\\record_check.py ...
"""
import collections
import glob
import json
import os
import re
import sys

MARKER = "flbench" + "-" + "private"
LIVE = 2  # hud2::Phase::Live


def fnv64(data):
    # the project's FNV-1a 64 (core/game/fontraster.cpp Fnv, `hud2 kitdump`): its offset basis is the decimal
    # 1469598103934665603 = 0x14650FB0739D0383, one digit short of the standard 14695981039346656037; kept, the
    # fingerprints written since B1 use it
    h = 0x14650FB0739D0383
    for b in data:
        h = ((h ^ b) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return "%016x" % h


def dump_passes(path):
    """The passes of a `hud2 kitdump` text: each a tuple of six (count, hash)."""
    out = []
    if not os.path.exists(path):
        return None
    with open(path, encoding="ascii", errors="replace") as f:
        for line in f:
            if line.startswith("pass "):
                layers = re.findall(r"\| L\d n=(\d+) tex=\S+ h=([0-9a-f]{16})", line)
                out.append(tuple((int(n), h) for n, h in layers))
    return out


def check(folder):
    problems, notes = [], []
    records = sorted(glob.glob(os.path.join(folder, "*.flstate.json")))
    both = one = 0
    dumps = {}
    for path in records:
        name = os.path.basename(path)
        try:
            with open(path, "rb") as f:
                raw = f.read()
            rec = json.loads(raw.decode("utf-8"))
        except Exception as e:  # noqa: BLE001 (any failure to read is the finding)
            problems.append(f"{name}: unreadable ({e})")
            continue
        if rec.get("schema") != "flstate/0" or rec.get("privacy") != MARKER:
            problems.append(f"{name}: not a private flstate/0 record")
            continue
        ref = rec["list"]
        list_path = os.path.join(folder, ref["file"])
        if not os.path.exists(list_path):
            problems.append(f"{name}: its list {ref['file']} is missing")
            continue
        with open(list_path, "rb") as f:
            list_raw = f.read()
        if fnv64(list_raw) != ref["hash"]:
            problems.append(f"{name}: the list's hash {fnv64(list_raw)} is not the record's {ref['hash']}")
        lst = json.loads(list_raw.decode("utf-8"))
        r = lst.get("record", {})
        if r.get("file") != name or r.get("inputs") != ref["inputs"]:
            problems.append(f"{name}: the list's record {r} does not name this record and its inputs {ref['inputs']}")
        if lst.get("schema") != "fldl/0" or lst.get("privacy") != MARKER:
            problems.append(f"{name}: the list is not a private fldl/0 list")
        if rec["update"]["phase"] != LIVE or not rec["render"]["drawn"]:
            problems.append(f"{name}: not a live frame drawn (phase {rec['update']['phase']}, drawn {rec['render']['drawn']})")
        drawn = sum(p["count"] for p in ref["over"]) if len(ref["over"]) == 6 else -1
        if drawn < 0 or (drawn > 0 and not ref["sets"]):
            problems.append(f"{name}: {len(ref['over'])} over layer prints, {len(ref['sets'])} font sets")
            continue
        if drawn == 0:
            notes.append(f"{name}: HUD 2's over pass is empty (nothing drawn in this state)")
        if [p["count"] for p in ref["over"]] != [len(l["v"]) // 5 for l in lst["passes"]["over"]["layers"]]:
            notes.append(f"{name}: the list's layer sizes are not the prints' counts (a layer without a texture is not listed)")
        over = tuple((p["count"], p["hash"]) for p in ref["over"])
        state = rec["state"]
        if state not in dumps:
            dumps[state] = (dump_passes(os.path.join(folder, state + "_on.txt")), dump_passes(os.path.join(folder, state + "_off.txt")))
        on, off = dumps[state]
        in_on = on is not None and over in on
        in_off = off is not None and over in off
        if in_on and in_off:
            both += 1
        elif in_on or in_off:
            one += 1
            notes.append(f"{name}: its over pass is in the {'on' if in_on else 'off'} dump only (the state moves between frames?)")
        elif on is None and off is None:
            notes.append(f"{name}: no kit dumps beside it")
        else:
            problems.append(f"{name}: its over pass is in neither kit dump (the recorder's copy is not what was drawn)")
    # what was drawn with the recorder on against off: the distinct passes of each state's two dumps
    same_sets = 0
    for state, (on, off) in sorted(dumps.items()):
        if on is None or off is None:
            continue
        if set(on) == set(off):
            same_sets += 1
        else:
            notes.append(f"{state}: the passes drawn with the recorder on differ from those drawn with it off "
                         f"({len(set(on) - set(off))} on only, {len(set(off) - set(on))} off only)")
    for n in notes:
        print("NOTE", n)
    for p in problems:
        print("FAIL", p)
    print(f"RESULT {len(records)} records, {both} in both dumps, {one} in one, {len(problems)} failures; "
          f"{same_sets} of {len(dumps)} states drew the same passes with the recorder on and off")
    return 1 if problems or not records else 0


def leaves(x, path, out):
    if isinstance(x, dict):
        for k, v in x.items():
            leaves(v, f"{path}.{k}", out)
    elif isinstance(x, list):
        for i, v in enumerate(x):
            leaves(v, f"{path}[{i}]", out)
        out[path + ".len"] = len(x)
    else:
        out[path] = x


def list_diff(pa, pb):
    """How two lists of one state differ: vertex counts per layer, else the largest move and the colours that differ."""
    la, lb = (json.load(open(p, encoding="utf-8")) for p in (pa, pb))
    counts, moved, colours, uvs = [], 0.0, 0, 0
    for pas in ("under", "over"):
        for x, y in zip(la["passes"][pas]["layers"], lb["passes"][pas]["layers"]):
            va, vb = x["v"], y["v"]
            if len(va) != len(vb):
                counts.append(f"{pas}.{x['name']} {len(va) // 5}/{len(vb) // 5}")
                continue
            for i in range(0, len(va), 5):
                moved = max(moved, abs(va[i] - vb[i]), abs(va[i + 1] - vb[i + 1]))
                colours += va[i + 2] != vb[i + 2]
                uvs += va[i + 3] != vb[i + 3] or va[i + 4] != vb[i + 4]
    if counts:
        return "vertex counts " + ", ".join(counts)
    return f"max move {moved:.4g} px, {colours} colours, {uvs} uv differ"


def compare(a, b):
    tally = collections.Counter()
    states = []
    for pa in sorted(glob.glob(os.path.join(a, "*.flstate.json"))):
        pb = os.path.join(b, os.path.basename(pa))
        if not os.path.exists(pb):
            continue
        ra, rb = (json.load(open(p, encoding="utf-8")) for p in (pa, pb))
        la, lb = {}, {}
        leaves(ra, "", la)
        leaves(rb, "", lb)
        differ = sorted(re.sub(r"\[\d+\]", "[]", k) for k in set(la) | set(lb) if la.get(k) != lb.get(k))
        for k in set(differ):
            tally[k] += 1
        same_list = ra["list"]["over"] == rb["list"]["over"] and ra["list"]["under"] == rb["list"]["under"]
        how = "" if same_list else list_diff(os.path.join(a, ra["list"]["file"]), os.path.join(b, rb["list"]["file"]))
        states.append((os.path.basename(pa), same_list, len(set(differ)), how))
    for name, same, n, how in states:
        print(f"{name}: lists {'EQUAL' if same else 'DIFFER (' + how + ')'}, {n} record members differ")
    print("record members that differ (in how many records):")
    for k, n in tally.most_common():
        print(f"  {n:4d} {k}")
    print(f"RESULT {len(states)} records compared, {sum(1 for s in states if s[1])} with equal lists")
    return 0


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "check":
        sys.exit(check(sys.argv[2]))
    if len(sys.argv) == 4 and sys.argv[1] == "compare":
        sys.exit(compare(sys.argv[2], sys.argv[3]))
    print(__doc__)
    sys.exit(2)
