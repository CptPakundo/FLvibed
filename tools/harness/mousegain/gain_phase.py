import csv, sys, statistics, datetime
path, start, W, H = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
t0 = datetime.datetime.strptime(start, '%Y-%m-%d %H:%M:%S.%f')
marks = []
for a in sys.argv[5:]:
    c, l = a.split('=', 1)
    marks.append(((datetime.datetime.strptime(start[:11] + c, '%Y-%m-%d %H:%M:%S.%f') - t0).total_seconds(), l))
marks.sort()
def phase(t):
    n = 'P0'
    for mt, l in marks:
        if t >= mt: n = l
    return n
rows = []
with open(path, newline='') as f:
    for r in csv.DictReader(f):
        rows.append((int(r['t_us']) / 1e6, int(r['rawX']), int(r['rawY']), int(r['rawN']), int(r['cx']), int(r['cy']), int(r['wx']), int(r['wy']), r['flags'], int(r['fg'])))
idx = [k for k in range(1, len(rows)) if rows[k][3] != rows[k - 1][3]]
ev = {}
multi = 0
for n, k in enumerate(idx[:-1]):
    a, nxt = rows[k - 1], idx[n + 1]
    if rows[k][3] - a[3] != 1: multi += 1
    if rows[nxt][0] - rows[k][0] > 0.012 or rows[k][3] - a[3] != 1 or not rows[k][9]: continue
    seg = rows[k - 1:nxt]
    for ax, ri, ci, wi, lim in (('x', 1, 4, 6, W), ('y', 2, 5, 7, H)):
        d = rows[k][ri] - a[ri]
        if not d or any(s[ci] <= 0 or s[ci] >= lim - 1 for s in seg): continue
        step = (rows[nxt - 1][ci] - a[ci]) * (1 if d > 0 else -1)
        exc = max(abs(s[wi] - a[wi]) for s in seg)
        ev.setdefault(phase(rows[k][0]), []).append((abs(d), step, exc, ax))
print('rows with more than one raw event between samples:', multi)
B = [(1, 3), (3, 6), (6, 12), (12, 24), (24, 48), (48, 1000)]
print('cursor px per raw count, per raw event, by the event\'s counts (both axes); in brackets the pointer\'s logical px per count')
for ph, e in ev.items():
    line = f'{ph:26s}'
    for lo, hi in B:
        g = [x for x in e if lo <= x[0] < hi]
        line += (f' {statistics.median(x[1] / x[0] for x in g):5.2f} ({statistics.median(x[2] / x[0] for x in g):.2f}) n={len(g):4d} |' if len(g) >= 8 else f'   -   n={len(g):4d} |')
    print(line)
print(' ' * 26 + ''.join(f'  counts {lo}-{hi}'.ljust(24) for lo, hi in B))
