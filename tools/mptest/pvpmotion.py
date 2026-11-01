"""How a client flew its fights: FLMotion.tsv rows of the Combat mode (mode 7), per fight (a gap > 2 s starts a new
one). Speed (median / p90 / max), the share of time on afterburner, at full throttle / throttle set by the pursuit, strafing
(slide), and the turn rate of the nose (deg/s, from the forward vector of consecutive rows: median / p90). Answers "did it maneuver or
sit and shoot" (the owner, 28 Oct 2026, watching autopilot PvP: "both ships barely moved").

Usage: python -I pvpmotion.py <FLMotion.tsv> [label]
"""
import math
import sys


def pct(v, p):
    if not v:
        return float("nan")
    s = sorted(v)
    return s[min(len(s) - 1, int(p * (len(s) - 1) + 0.5))]


def main():
    path = sys.argv[1]
    label = sys.argv[2] if len(sys.argv) > 2 else path
    with open(path, encoding="utf-8", errors="replace") as f:
        head = f.readline().rstrip("\n").split("\t")
        col = {n: i for i, n in enumerate(head)}
        rows = [l.rstrip("\n").split("\t") for l in f]
    fights, cur, last = [], [], None
    for r in rows:
        if len(r) < len(head) or r[col["mode"]] != "7":
            continue
        ms = int(r[col["ms"]])
        if last is not None and ms - last > 2000 and cur:
            fights.append(cur)
            cur = []
        cur.append(r)
        last = ms
    if cur:
        fights.append(cur)
    print(f"{label}: {len(fights)} fight(s) in combat mode")
    for n, fr in enumerate(fights, 1):
        t = (int(fr[-1][col["ms"]]) - int(fr[0][col["ms"]])) / 1000.0
        speed = [float(r[col["speed"]]) for r in fr]
        ab = sum(1 for r in fr if r[col["ab"]] == "1") / len(fr)
        # (no engine-kill column: `engine` is the cruise engine's state)
        full = sum(1 for r in fr if float(r[col["throttle"]]) >= 0.99) / len(fr)
        # throttle -1 = the pursuit sets it (steer "pursue"), not reverse thrust
        rev = sum(1 for r in fr if float(r[col["throttle"]]) <= -0.99) / len(fr)
        slide = sum(1 for r in fr if r[col["slide"]] not in ("0", "")) / len(fr)
        turns = []
        for a, b in zip(fr, fr[1:]):
            dt = (int(b[col["ms"]]) - int(a[col["ms"]])) / 1000.0
            if dt <= 0:
                continue
            u = [float(a[col[k]]) for k in ("fx", "fy", "fz")]
            v = [float(b[col[k]]) for k in ("fx", "fy", "fz")]
            d = max(-1.0, min(1.0, sum(x * y for x, y in zip(u, v))))
            turns.append(math.degrees(math.acos(d)) / dt)
        p0 = [float(fr[0][col[k]]) for k in ("x", "y", "z")]
        far = max(math.dist(p0, [float(r[col[k]]) for k in ("x", "y", "z")]) for r in fr)
        print(
            f"  fight {n}: {t:.1f} s, {len(fr)} rows | speed median {pct(speed, 0.5):.0f} p90 {pct(speed, 0.9):.0f} "
            f"max {max(speed):.0f} m/s | afterburner {ab:.0%} throttle full {full:.0%} by-pursuit {rev:.0%} "
            f"strafe {slide:.0%} | "
            f"nose turn median {pct(turns, 0.5):.0f} p90 {pct(turns, 0.9):.0f} deg/s | farthest from start {far:.0f} m"
        )


if __name__ == "__main__":
    main()
