# Gain per injected event: cursor px per injected count, by the event's counts (x axis), from mouse_gain_rec.ps1's CSV.
import csv, sys, statistics
for path in sys.argv[1:]:
    rows = []
    with open(path, newline='') as f:
        for r in csv.DictReader(f):
            rows.append((int(r['t_us']) / 1e6, int(r['injX']), int(r['cx']), int(r['rawX']), int(r['fg']), r['flags']))
    idx = [k for k in range(1, len(rows)) if rows[k][1] != rows[k - 1][1]]
    by = {}
    for n, k in enumerate(idx[:-1]):
        a, nxt = rows[k - 1], idx[n + 1]
        d = rows[k][1] - a[1]
        if rows[nxt][0] - rows[k][0] > 0.02 or not d:
            continue
        step = (rows[nxt - 1][2] - a[2]) * (1 if d > 0 else -1)
        by.setdefault(abs(d), []).append(step / abs(d))
    real = abs(rows[-1][3] - rows[0][3])
    print(path.split('\\')[-1], f'rows {len(rows)}, injected events {len(idx)}, real mouse counts meanwhile {real}, in front {sum(r[4] for r in rows)}/{len(rows)}, flags {sorted(set(r[5] for r in rows))}')
    print('   ' + ' | '.join(f'{n} counts: {statistics.median(g):.2f} px/count (n={len(g)}, min {min(g):.2f} max {max(g):.2f})' for n, g in sorted(by.items()) if len(g) >= 5))
