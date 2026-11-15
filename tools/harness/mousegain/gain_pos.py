import csv, sys, statistics
path, W, H, tmin, tmax = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), float(sys.argv[4]), float(sys.argv[5])
rows = []
with open(path, newline='') as f:
    for r in csv.DictReader(f):
        rows.append((int(r['t_us']) / 1e6, int(r['rawX']), int(r['rawY']), int(r['rawN']), int(r['cx']), int(r['cy']), int(r['wx']), int(r['wy']), r['flags'], int(r['fg'])))
idx = [k for k in range(1, len(rows)) if rows[k][3] != rows[k - 1][3]]
ev = []
for n, k in enumerate(idx[:-1]):
    a, nxt = rows[k - 1], idx[n + 1]
    if not (tmin <= rows[k][0] < tmax): continue
    if rows[nxt][0] - rows[k][0] > 0.012 or rows[k][3] - a[3] != 1 or not rows[k][9]: continue
    seg = rows[k - 1:nxt]
    for ax, ri, ci, wi, lim in (('x', 1, 4, 6, W), ('y', 2, 5, 7, H)):
        d = rows[k][ri] - a[ri]
        if not d or any(s[ci] <= 0 or s[ci] >= lim - 1 for s in seg): continue
        step = (rows[nxt - 1][ci] - a[ci]) * (1 if d > 0 else -1)
        pos = a[ci] / lim
        outward = (d > 0) == (pos > 0.5)
        ev.append((abs(d), step, pos, outward, ax, a[wi], abs(max(s[wi] for s in seg) - a[wi]) if d > 0 else abs(min(s[wi] for s in seg) - a[wi])))
print('events', len(ev))
for lo, hi in ((3, 12), (12, 24), (24, 48)):
    print(f'counts {lo}-{hi}: cursor px per count by the cursor\'s place (fraction of the screen from the left / top edge), outward and inward')
    for ax in 'xy':
        line = f'  {ax}:'
        for zlo, zhi in ((0, .1), (.1, .25), (.25, .4), (.4, .6), (.6, .75), (.75, .9), (.9, 1.01)):
            for out in (True, False):
                g = [e[1] / e[0] for e in ev if lo <= e[0] < hi and e[4] == ax and zlo <= e[2] < zhi and e[3] == out]
                line += (f' {statistics.median(g):5.2f}' if len(g) >= 8 else '    - ') + ('o' if out else 'i') + f'({len(g)})'
            line += ' |'
        print(line)
# where does Windows' pointer rest (before each event)?
import collections
rest = collections.Counter((e[5] // 50 * 50) for e in ev if e[4] == 'x')
print('pointer rest x (logical, 50 px bins):', sorted(rest.items()))
rest = collections.Counter((e[5] // 50 * 50) for e in ev if e[4] == 'y')
print('pointer rest y (logical, 50 px bins):', sorted(rest.items()))
