# Analyse mouse_gain_rec.ps1's CSV: cursor pixels per raw count, per phase and per speed.
# usage: gain_analyse.py <csv> <start wall clock 'YYYY-MM-DD HH:MM:SS.fff'> <width> <height> [HH:MM:SS.fff=label ...]
import csv, sys, datetime, statistics

path, start, W, H = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
t0 = datetime.datetime.strptime(start, '%Y-%m-%d %H:%M:%S.%f')
marks = []
for a in sys.argv[5:]:
    clock, label = a.split('=', 1)
    m = datetime.datetime.strptime(start[:11] + clock, '%Y-%m-%d %H:%M:%S.%f')
    marks.append(((m - t0).total_seconds(), label))
marks.sort()

rows = []
with open(path, newline='') as f:
    for r in csv.DictReader(f):
        rows.append((int(r['t_us']) / 1e6, int(r['rawX']), int(r['rawY']), int(r['rawN']), int(r['injX']), int(r['injY']),
                     int(r['cx']), int(r['cy']), int(r['wx']), int(r['wy']), r['flags'], int(r['fg'])))
print(f'rows {len(rows)}, {rows[-1][0]:.1f} s; flags seen: {sorted(set(r[10] for r in rows))}; fg values {sorted(set(r[11] for r in rows))}')

def phase_of(t):
    name = 'P0 start'
    for mt, label in marks:
        if t >= mt:
            name = label
    return name

WIN = 0.1
# windows: state at window edges (last row at or before the edge)
edges = []
i = 0
t = rows[0][0]
end = rows[-1][0]
last = rows[0]
while t <= end:
    while i < len(rows) and rows[i][0] <= t:
        last = rows[i]
        i += 1
    edges.append((t, last))
    t += WIN

wins = {}
for (ta, a), (tb, b) in zip(edges, edges[1:]):
    ph = phase_of(ta)
    drx, dry, dn = b[1] - a[1], b[2] - a[2], b[3] - a[3]
    dix, diy = b[4] - a[4], b[5] - a[5]
    dcx, dcy = b[6] - a[6], b[7] - a[7]
    dwx, dwy = b[8] - a[8], b[9] - a[9]
    clampx = a[6] <= 0 or a[6] >= W - 1 or b[6] <= 0 or b[6] >= W - 1
    clampy = a[7] <= 0 or a[7] >= H - 1 or b[7] <= 0 or b[7] >= H - 1
    wins.setdefault(ph, []).append((drx, dry, dn, dix, diy, dcx, dcy, dwx, dwy, clampx, clampy, a[11] and b[11], ta))

def med(v):
    return statistics.median(v) if v else float('nan')

print('\nper phase: gain = cursor px per raw count, over 100 ms windows away from the screen edge, by raw speed (counts per 100 ms)')
buckets = [(3, 20), (20, 60), (60, 150), (150, 400), (400, 100000)]
for ph, ws in wins.items():
    n = len(ws)
    rawpath = sum(abs(w[0]) + abs(w[1]) for w in ws)
    curpath = sum(abs(w[5]) + abs(w[6]) for w in ws)
    injpath = sum(abs(w[3]) + abs(w[4]) for w in ws)
    ptrpath = sum(abs(w[7]) + abs(w[8]) for w in ws)
    events = sum(w[2] for w in ws)
    moving = sum(1 for w in ws if w[2] > 0)
    ghost = sum(1 for w in ws if w[0] == 0 and w[1] == 0 and w[3] == 0 and w[4] == 0 and (abs(w[5]) + abs(w[6])) > 2)
    lost = sum(1 for w in ws if (abs(w[0]) + abs(w[1])) > 10 and w[5] == 0 and w[6] == 0 and not (w[9] or w[10]))
    flip = sum(1 for w in ws if abs(w[0]) > 10 and abs(w[5]) > 10 and (w[0] > 0) != (w[5] > 0) and not w[9])
    print(f'\n== {ph}: {n * WIN:.0f} s, raw path {rawpath} counts in {events} events ({events / max(1, moving) / WIN:.0f} events/s while moving), cursor path {curpath} px, pointer path {ptrpath}, injected path {injpath}')
    print(f'   cursor moved with no raw motion: {ghost} windows; raw motion with a still cursor (not at an edge): {lost}; x direction flipped: {flip}; not in front: {sum(1 for w in ws if not w[11])}')
    for axis, ri, ci, cl in (('x', 0, 5, 9), ('y', 1, 6, 10)):
        line = f'   {axis}:'
        for lo, hi in buckets:
            g = [w[ci] / w[ri] for w in ws if lo <= abs(w[ri]) < hi and not w[cl] and w[11]]
            if len(g) >= 3:
                q = statistics.quantiles(g, n=10) if len(g) >= 10 else [min(g)] * 9
                line += f'  [{lo}-{hi}) n={len(g)} med {med(g):.2f} p10 {q[0]:.2f} p90 {q[-1] if len(g) >= 10 else max(g):.2f} |'
            else:
                line += f'  [{lo}-{hi}) n={len(g)} |'
        print(line)

# the raw report timing: gaps between rows in which the raw count grew, while moving
print('\nraw event timing per phase (gap between successive raw-count changes, ms, while the mouse moves; gaps > 50 ms dropped)')
gaps = {}
prevn = None
prevt = None
for r in rows:
    if prevn is not None and r[3] != prevn:
        if prevt is not None:
            g = (r[0] - prevt) * 1000
            if g < 50:
                gaps.setdefault(phase_of(r[0]), []).append(g)
        prevt = r[0]
    prevn = r[3]
for ph, g in gaps.items():
    g.sort()
    print(f'   {ph}: n={len(g)} med {med(g):.2f} p90 {g[int(len(g) * 0.9)]:.2f} p99 {g[int(len(g) * 0.99)]:.2f} max {g[-1]:.1f}')

# the cursor update timing: gaps between successive cursor changes while moving (the game's frame pacing as the mouse sees it)
print('\ncursor update timing per phase (gap between successive cursor changes, ms; gaps > 100 ms dropped)')
gaps = {}
prevc = None
prevt = None
for r in rows:
    c = (r[6], r[7])
    if prevc is not None and c != prevc:
        if prevt is not None:
            g = (r[0] - prevt) * 1000
            if g < 100:
                gaps.setdefault(phase_of(r[0]), []).append(g)
        prevt = r[0]
    prevc = c
for ph, g in gaps.items():
    g.sort()
    print(f'   {ph}: n={len(g)} med {med(g):.2f} p90 {g[int(len(g) * 0.9)]:.2f} p99 {g[int(len(g) * 0.99)]:.2f} max {g[-1]:.1f}')
