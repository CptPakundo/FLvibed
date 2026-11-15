import csv, sys, statistics
path, W, H = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
rows = []
with open(path, newline='') as f:
    for r in csv.DictReader(f):
        rows.append((int(r['t_us']) / 1e6, int(r['rawX']), int(r['rawY']), int(r['rawN']), int(r['cx']), int(r['cy']), int(r['wx']), int(r['wy']), r['flags'], int(r['fg'])))
print(path.split('\\')[-1], 'rows', len(rows))
# flags other than the main value: list each run with the cursor and pointer around it
main = statistics.mode(r[8] for r in rows)
k = 0
shown = 0
while k < len(rows):
    if rows[k][8] != main and rows[k][0] > 8 and shown < 12:
        j = k
        while j < len(rows) and rows[j][8] != main: j += 1
        if j < len(rows):
            a, b = rows[max(0, k - 1)], rows[min(len(rows) - 1, j)]
            print(f'  flags {sorted(set(r[8] for r in rows[k:j]))} for {1000 * (rows[j][0] - rows[k][0]):.1f} ms at t={rows[k][0]:.3f}: cursor {a[4]},{a[5]} -> {b[4]},{b[5]}, pointer {a[6]},{a[7]} -> {b[6]},{b[7]}, fg {a[9]}->{b[9]}')
            shown += 1
        k = j
    else:
        k += 1
# edge episodes on x and y
for ax, ri, ci, wi, lim in (('x', 1, 4, 6, W), ('y', 2, 5, 7, H)):
    eps = []
    k = 1
    while k < len(rows):
        at = rows[k][ci] <= 0 or rows[k][ci] >= lim - 1
        if at and not (rows[k - 1][ci] <= 0 or rows[k - 1][ci] >= lim - 1) and rows[k][8] == main and rows[k][0] > 8:
            side = 1 if rows[k][ci] >= lim - 1 else -1
            j = k
            while j < len(rows) and (rows[j][ci] <= 0 or rows[j][ci] >= lim - 1): j += 1
            if j >= len(rows): break
            seg = rows[k:j + 1]
            raw0 = rows[k][ri]
            peak = max((s[ri] - raw0) * side for s in seg)          # counts pushed past the edge
            back = peak - (rows[j][ri] - raw0) * side               # counts moved back before the cursor left the edge (incl. the leaving event)
            # the leaving step: cursor distance from the edge at row j and the next few ms
            leave = abs(rows[j][ci] - (lim - 1 if side > 0 else 0))
            pw = [s[wi] for s in seg]
            eps.append((rows[k][0], rows[j][0] - rows[k][0], peak, back, leave, min(pw), max(pw), rows[k - 1][wi]))
            k = j
        k += 1
    print(f'{ax} edge episodes: {len(eps)}')
    big = [e for e in eps if e[2] >= 20]
    if big:
        print(f'  with >= 20 counts pushed past the edge: {len(big)}; counts moved back before the cursor left: median {statistics.median(e[3] for e in big):.0f}, p90 {sorted(e[3] for e in big)[int(len(big) * .9)]:.0f}, max {max(e[3] for e in big):.0f}; ratio back/pushed median {statistics.median(e[3] / e[2] for e in big):.2f}')
        print(f'  the leaving step (px from the edge at the first sample off it): median {statistics.median(e[4] for e in big):.0f}, p90 {sorted(e[4] for e in big)[int(len(big) * .9)]:.0f}, max {max(e[4] for e in big):.0f}')
        print(f'  pointer during contact (logical): min {min(e[5] for e in big)}, max {max(e[6] for e in big)}; pointer rest before contact: {sorted(set(e[7] for e in big))[:12]}')
        for e in big[:10]:
            print(f'    t={e[0]:.2f} s for {e[1]:.2f} s: pushed {e[2]} counts past, moved back {e[3]} before leaving, leaving step {e[4]} px, pointer {e[5]}..{e[6]}')
