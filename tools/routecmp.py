"""Route check: every checkpoint segment of a leg run, our flown route against a reference route (the user, 24 Oct 2026:
"I'm concerned you weren't able to figure the pathing issue from comparing to research data" - the M12 gen 1 -> 2 hop
flew 4.0-4.8 km into the lair's inner wall while the user's try 3 there is one level curve of ~3.3 km; the segment times
and speed profiles never showed the route, the routes side by side did). docs\\TESTING.md "The test loop" step 5.

Usage (OCR venv python, -I):  routecmp.py <leg folder> [<leg folder> ...] [--ref <motion.tsv>]
  Segments from research\\experience\\legs\\splits.tsv (the run's checkpoints, seconds from the leg start), the leg start
  from the folder's transcript, the motion trace aligned to the log on the first system change both record. The reference
  is a motion trace of a reference flight (default for M12: the user's try 3, research\\experience\\reference\\); each of
  our segments is matched to the reference stretch between the points nearest our segment's ends.
Per segment: ours s / path m / straight m, reference s / path m, path ratio, mean / max distance from the reference line,
mean height offset, seconds under 100 m/s, seconds under the geometry guard. FLAG when the path is over 1.15x the
reference's (LONGER), the mean distance over 100 m or the max over 250 m (OFF-ROUTE), more than 3 s under 100 m/s beyond
the reference's own (SLOW), more than 2 s under the guard (GUARD). Where no reference covers a segment the runners' data
still applies: ref.py's speed / phase profile for that section, read side by side with ours. A reference is a guide, not
the target (the user, 24 Oct 2026: try 3 "is only pretty good, not perfect - some noisy flight and suboptimal timing"):
OFF-ROUTE is a lead to read in the trace; a cleaner, shorter, faster line than the reference's is the aim.
"""
import csv
import math
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LEGS = os.path.join(ROOT, 'research', 'experience', 'legs')
REFS = {'M12': os.path.join(ROOT, 'research', 'experience', 'reference', 'M12_user_2026-10-23_try3_motion.tsv')}
TS = re.compile(r'^\[(\d+):(\d+):(\d+)\.(\d+)\]')


def clock_line(line):
    m = TS.match(line)
    if not m:
        return None
    h, mi, s, ms = (int(g) for g in m.groups())
    return h * 3600 + mi * 60 + s + ms / 1000.0


def read_motion(path):
    with open(path, encoding='utf-8', errors='replace') as f:
        out = []
        for r in csv.DictReader(f, delimiter='\t'):
            try:
                out.append(dict(t=int(r['ms']) / 1000.0, p=(float(r['x']), float(r['y']), float(r['z'])),
                                v=float(r['speed']), g=r.get('guard') == '1', sys=r['system'], act=r['action']))
            except (ValueError, KeyError):
                pass
    return out


def offset(log_path, rows):
    """motion seconds - log clock seconds, from the first system change of the trace and the log's first line naming
    the new system after it."""
    for i in range(1, len(rows)):
        if rows[i]['sys'] != rows[i - 1]['sys']:
            new = rows[i]['sys']
            with open(log_path, encoding='utf-8', errors='replace') as f:
                for line in f:
                    if 'system=' + new in line:
                        c = clock_line(line)
                        if c is not None:
                            return rows[i]['t'] - c
            return None
    return None


def leg_t0(folder):
    tr = os.path.join(folder, 'transcript.txt')
    m = re.search(r'^(\d\d):(\d\d):(\d\d) \+\s+0 ', open(tr, encoding='utf-8', errors='replace').read(), re.M)
    return int(m.group(1)) * 3600 + int(m.group(2)) * 60 + int(m.group(3)) if m else None


def segments(folder):
    name = os.path.basename(folder.rstrip('\\/'))
    with open(os.path.join(LEGS, 'splits.tsv'), encoding='utf-8') as f:
        rows = [r for r in csv.DictReader(f, delimiter='\t') if r['dir'] == name and r['at']]
    rows.sort(key=lambda r: float(r['at']))
    return rows


def plen(pts):
    return sum(math.dist(pts[i - 1], pts[i]) for i in range(1, len(pts)))


def seg_dist(p, a, b):
    ab = [b[i] - a[i] for i in range(3)]
    l2 = sum(x * x for x in ab)
    t = 0 if l2 < 1e-6 else max(0.0, min(1.0, sum((p[i] - a[i]) * ab[i] for i in range(3)) / l2))
    return math.dist(p, [a[i] + ab[i] * t for i in range(3)])


def line_dist(p, poly):
    return min(seg_dist(p, poly[i - 1], poly[i]) for i in range(1, len(poly))) if len(poly) > 1 else math.dist(p, poly[0])


def thin(rows, step=0.5):
    out, last = [], -1e9
    for r in rows:
        if r['t'] - last >= step:
            out.append(r)
            last = r['t']
    return out


def slow_secs(rows):
    return sum(rows[i]['t'] - rows[i - 1]['t'] for i in range(1, len(rows)) if rows[i]['v'] < 100)


def guard_secs(rows):
    return sum(rows[i]['t'] - rows[i - 1]['t'] for i in range(1, len(rows)) if rows[i]['g'])


def check(folder, ref_rows):
    log = os.path.join(folder, 'FLController.log')
    rows = read_motion(os.path.join(folder, 'FLMotion.tsv'))
    off, t0 = offset(log, rows), leg_t0(folder)
    segs = segments(folder)
    print(f'== {os.path.basename(folder)}')
    if off is None or t0 is None or not segs:
        print('  (no alignment / no segments)')
        return
    # (the log clock and the transcript clock are both local time; the motion's = log + off)
    prev_at, ref_i = 0.0, 0
    print(f'  {"segment":16} {"ours s":>6} {"path":>6} {"str":>6} | {"ref s":>6} {"path":>6} | {"ratio":>5} '
          f'{"dev":>5} {"max":>5} {"dy":>5} {"slow":>5} {"rslow":>5} {"guard":>5}  flags')
    for s in segs:
        at = float(s['at'])
        a, b = t0 + prev_at + off, t0 + at + off
        prev_at = at
        ours = [r for r in rows if a <= r['t'] <= b]
        if len(ours) < 3:
            continue
        sysid = ours[len(ours) // 2]['sys']
        ours = [r for r in ours if r['sys'] == sysid]
        if len(ours) < 3:
            continue
        pts = [r['p'] for r in ours]
        line = f'  {s["cp"]:16} {b - a:6.1f} {plen(pts):6.0f} {math.dist(pts[0], pts[-1]):6.0f} |'
        ref = [r for r in ref_rows if r['sys'] == sysid] if ref_rows else []
        flags = []
        if ref:
            i0 = min(range(ref_i, len(ref)), key=lambda i: math.dist(ref[i]['p'], pts[0]), default=None)
            i1 = min(range(i0, len(ref)), key=lambda i: math.dist(ref[i]['p'], pts[-1]), default=None) if i0 is not None else None
            if i0 is not None and i1 is not None and i1 > i0 and math.dist(ref[i0]['p'], pts[0]) < 400 \
                    and math.dist(ref[i1]['p'], pts[-1]) < 400:
                ref_i = i1
                rs = ref[i0:i1 + 1]
                rpts = [r['p'] for r in thin(rs, 0.25)]
                devs = [line_dist(r['p'], rpts) for r in thin(ours, 0.5)]
                dy = sum(p[1] for p in pts) / len(pts) - sum(p[1] for p in rpts) / len(rpts)
                ratio = plen(pts) / max(1.0, plen(rpts))
                mdev = sum(devs) / len(devs)
                sl, rsl = slow_secs(ours), slow_secs(rs)
                line += (f' {rs[-1]["t"] - rs[0]["t"]:6.1f} {plen(rpts):6.0f} | {ratio:5.2f} {mdev:5.0f} {max(devs):5.0f} '
                         f'{dy:5.0f} {sl:5.1f} {rsl:5.1f} {guard_secs(ours):5.1f}')
                if ratio > 1.15:
                    flags.append('LONGER')
                if mdev > 100 or max(devs) > 250:
                    flags.append('OFF-ROUTE')
                if sl - rsl > 3:
                    flags.append('SLOW')
            else:
                line += f' {"-":>6} {"-":>6} | {"":5} {"":5} {"":5} {"":5} {slow_secs(ours):5.1f} {"":5} {guard_secs(ours):5.1f}'
        else:
            line += f' {"-":>6} {"-":>6} | {"":5} {"":5} {"":5} {"":5} {slow_secs(ours):5.1f} {"":5} {guard_secs(ours):5.1f}'
        if guard_secs(ours) > 2:
            flags.append('GUARD')
        print(line + '  ' + ' '.join(flags))


def main():
    args = sys.argv[1:]
    ref_path = None
    if '--ref' in args:
        k = args.index('--ref')
        ref_path = args[k + 1]
        del args[k:k + 2]
    if not args:
        print(__doc__)
        return
    for a in args:
        folder = a if os.path.isdir(a) else os.path.join(LEGS, a)
        leg = re.search(r'_(M\d\d[ab]?)', os.path.basename(folder))
        rp = ref_path or (REFS.get(leg.group(1)) if leg else None)
        ref_rows = read_motion(rp) if rp and os.path.isfile(rp) else []
        check(folder, ref_rows)


if __name__ == '__main__':
    main()
