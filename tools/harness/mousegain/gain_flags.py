# Flags timeline and gain per flags value / per raw event, from mouse_gain_rec.ps1's CSV.
import csv, sys, statistics, datetime
path, start, W, H = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
t0 = datetime.datetime.strptime(start, '%Y-%m-%d %H:%M:%S.%f')
rows = []
with open(path, newline='') as f:
    for r in csv.DictReader(f):
        rows.append((int(r['t_us']) / 1e6, int(r['rawX']), int(r['rawY']), int(r['rawN']), int(r['cx']), int(r['cy']), int(r['wx']), int(r['wy']), r['flags'], int(r['fg'])))

def clock(t):
    return (t0 + datetime.timedelta(seconds=t)).strftime('%H:%M:%S.%f')[:-3]

print('flags timeline (changes; runs shorter than 30 ms are counted, not listed)')
prev, since, short = None, 0, 0
for r in rows:
    if r[8] != prev:
        if prev is not None:
            if r[0] - since < 0.03:
                short += 1
            else:
                print(f'  {clock(since)} .. {clock(r[0])}  {prev}  ({r[0] - since:.2f} s)')
        prev, since = r[8], r[0]
print(f'  {clock(since)} .. end  {prev}; short runs {short}')

# gain per flags value, 100 ms windows
WIN = 0.1
buckets = [(3, 20), (20, 60), (60, 150), (150, 400), (400, 100000)]
edges, i, t, last = [], 0, rows[0][0], rows[0]
while t <= rows[-1][0]:
    while i < len(rows) and rows[i][0] <= t:
        last = rows[i]; i += 1
    edges.append((t, last)); t += WIN
byflag = {}
for (ta, a), (tb, b) in zip(edges, edges[1:]):
    if a[8] != b[8] or not (a[9] and b[9]):
        continue
    byflag.setdefault(a[8], []).append((b[1] - a[1], b[2] - a[2], b[4] - a[4], b[5] - a[5],
                                        a[4] <= 0 or a[4] >= W - 1 or b[4] <= 0 or b[4] >= W - 1, a[5] <= 0 or a[5] >= H - 1 or b[5] <= 0 or b[5] >= H - 1))
print('\ngain (cursor px per raw count) per flags value')
for fl, ws in sorted(byflag.items()):
    line = f'  {fl} ({len(ws) * WIN:.0f} s):'
    for lo, hi in buckets:
        g = [w[2] / w[0] for w in ws if lo <= abs(w[0]) < hi and not w[4]] + [w[3] / w[1] for w in ws if lo <= abs(w[1]) < hi and not w[5]]
        line += f' [{lo}-{hi}) n={len(g)}' + (f' med {statistics.median(g):.2f}' if len(g) >= 3 else '') + ' |'
    print(line)

# per raw event: the event's counts, the pointer's excursion and the cursor's step that follow (until the next raw event, at most 12 ms)
print('\nper raw event (x axis): counts in the event -> the pointer excursion (logical px, max |wx - rest|) and the cursor step (px) before the next event')
ev = []
idx = [k for k in range(1, len(rows)) if rows[k][3] != rows[k - 1][3]]
for n, k in enumerate(idx[:-1]):
    a, nxt = rows[k - 1], idx[n + 1]
    if rows[nxt][0] - rows[k][0] > 0.012 or rows[k][3] - a[3] != 1 or not rows[k][9]:
        continue
    dx = rows[k][1] - a[1]
    seg = rows[k - 1:nxt]
    rest = a[6]
    exc = max((abs(s[6] - rest) for s in seg), default=0)
    step = rows[nxt - 1][4] - a[4]
    edge = any(s[4] <= 0 or s[4] >= W - 1 for s in seg)
    if dx and not edge:
        ev.append((abs(dx), exc, step * (1 if dx > 0 else -1), rows[k][0]))
for lo, hi in [(1, 3), (3, 6), (6, 12), (12, 24), (24, 48), (48, 1000)]:
    g = [e for e in ev if lo <= e[0] < hi]
    if len(g) >= 5:
        print(f'  counts [{lo}-{hi}) n={len(g)}: pointer excursion per count med {statistics.median(e[1] / e[0] for e in g):.3f}, cursor step per count med {statistics.median(e[2] / e[0] for e in g):.2f}, cursor step per pointer px med '
              + (f'{statistics.median(e[2] / e[1] for e in g if e[1]):.2f}' if sum(1 for e in g if e[1]) >= 3 else 'n/a') + f', events with no pointer excursion {sum(1 for e in g if not e[1])}')
