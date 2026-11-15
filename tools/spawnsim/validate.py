# GitHub #61: the offline spawn simulator (build\spawnsim\flspawnsim.exe) against the first-sighting censuses.
#   python -I validate.py <DATA dir> <save.fl> <out dir> <census dir>... [--runs N]
# Each census point (the player held at its mean position for the census's dwell, arrived by teleport) is simulated and
# compared with the census the way 145_ "Validation" scored the v4 model: the census measures (the player-bias fix) count
# every ship where it was FIRST seen at a point: the mix = first sightings inside the 5 km disc (lead faction / relation per
# point and per 2.5 km cell with >= 2 sightings), the count = the time-mean ships in the disc first seen inside it (t >= 30 s).
# The simulator gives the same three for the same player (spawnsim::Result); its count is absolute (no calibration factor).
# Relations: the save's `house` values with the HUD's thresholds (+-0.6). Writes <out>\points.tsv, sim.tsv, results.json.
import os, sys, math, json, subprocess
from collections import defaultdict

R_COVER, SKIP, CELL, MIN_CELL = 5000.0, 30, 2500.0, 2
HERE = os.path.dirname(os.path.abspath(__file__))
EXE = os.path.join(HERE, '..', '..', 'build', 'spawnsim', 'flspawnsim.exe')


def parse_ini(path):
    secs, cur = [], None
    for line in open(path, encoding='latin-1'):
        line = line.split(';')[0].strip()
        if line.startswith('['):
            cur = (line.strip('[]').lower(), []); secs.append(cur)
        elif '=' in line and cur:
            k, v = line.split('=', 1); cur[1].append((k.strip().lower(), v.strip()))
    return secs


# the census's group column is the faction's 16-bit hash (CRC-16/CCITT of the lower-case nickname, as v4_eval.py)
T = []
for i in range(256):
    x = i << 8
    for _ in range(8):
        x = ((x << 1) ^ 0x1021) if x & 0x8000 else (x << 1)
    T.append(x & 0xFFFF)


def fachash(s):
    h = 0xFFFF
    for ch in s.lower().encode():
        h = ((h >> 8) ^ T[(h ^ ch) & 0xFF]) & 0xFFFF
    return h


def read_census(d, H2N):
    gid = {}
    for l in list(open(os.path.join(d, 'groups.tsv')))[1:]:
        i, line = l.rstrip('\n').split('\t', 1)
        t = line.split()
        g = int(t[t.index('group') + 1]) if 'group' in t else 0
        gid[i] = H2N.get(g, 'g%d' % g)
    lines = open(os.path.join(d, 'census.tsv')).read().splitlines()
    head = lines[0].split('\t')
    dwell = 180 if 'system' not in head else 150
    steps = open(os.path.join(d, 'steps.txt'), encoding='utf-8', errors='replace').read().splitlines()
    pts = defaultdict(list)
    for l in lines[1:]:
        r = dict(zip(head, l.split('\t')))
        P = tuple(float(r[k]) for k in ('px', 'py', 'pz')); S = tuple(float(r[k]) for k in ('x', 'y', 'z'))
        pts[(r.get('system', 'li01').lower(), r['point'])].append((int(r['t']), P, r['id'], S, gid.get(r['id'], '?')))
    out = {}
    for (sysn, pt), rs in pts.items():
        if not any((' %s : ' % pt) in l and ' samples' in l for l in steps):
            continue  # a point lost to a death before its dwell ended
        n = dwell // 3 - SKIP // 3
        first = {}
        for r in sorted(rs):
            first.setdefault(r[2], r)
        dist = lambda a, b: math.dist(a, b)
        inside = {i for i, r in first.items() if dist(r[3], r[1]) <= R_COVER}
        spawn, cells, local = defaultdict(float), defaultdict(lambda: defaultdict(float)), defaultdict(float)
        for i in inside:
            r = first[i]
            spawn[r[4]] += 1
            cells[(math.floor(r[3][0] / CELL), math.floor(r[3][2] / CELL))][r[4]] += 1
        for t, P, i, S, f in rs:
            if t >= SKIP and dist(S, P) <= R_COVER and i in inside:
                local[f] += 1.0 / n
        pos = [sum(r[1][k] for r in rs) / len(rs) for k in range(3)]
        out[(os.path.basename(d), sysn, pt)] = dict(spawn=dict(spawn), local=dict(local), pos=pos, dwell=dwell,
                                                    cells={c: dict(v) for c, v in cells.items() if sum(v.values()) >= MIN_CELL})
    return out


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    runs = sys.argv[sys.argv.index('--runs') + 1] if '--runs' in sys.argv else '400'
    if '--runs' in sys.argv:
        args.remove(runs)
    assume = sys.argv[sys.argv.index('--assume') + 1] if '--assume' in sys.argv else ''  # spawnsim::Assume overrides
    if assume:
        args.remove(assume)
    data, save, outdir, dirs = args[0], args[1], args[2], args[3:]
    os.makedirs(outdir, exist_ok=True)
    H2N = {fachash(dict(kv)['nickname']): dict(kv)['nickname'].lower() for n, kv in parse_ini(os.path.join(data, 'initialworld.ini'))
           if n == 'group' and 'nickname' in dict(kv)}
    house = {}
    for l in open(save, encoding='latin-1', errors='replace'):
        if l.startswith('house = '):
            v, g = l[8:].strip().split(', ')
            house[g.lower()] = float(v)
    rel = lambda g: 'F' if house.get(g, 0) >= 0.6 else 'H' if house.get(g, 0) <= -0.6 else 'N'
    cen = {}
    for d in dirs:
        cen.update(read_census(d, H2N))
    keys = sorted(cen)
    with open(os.path.join(outdir, 'points.tsv'), 'w') as f:
        for k in keys:
            p = cen[k]['pos']
            f.write('%s\t%s|%s\t%.0f\t%.0f\t%.0f\t%d\n' % (k[1], k[0], k[2], p[0], p[1], p[2], cen[k]['dwell']))
    sim_txt = subprocess.run([EXE, data, os.path.join(outdir, 'points.tsv'), save, runs, '61'] + ([assume] if assume else []),
                             capture_output=True, text=True, check=True).stdout
    open(os.path.join(outdir, 'sim.tsv'), 'w').write(sim_txt)
    sim = defaultdict(lambda: dict(first={}, mean={}, cells=defaultdict(dict), diag=''))
    for l in sim_txt.splitlines():
        t = l.split('\t')
        if t[0] == 'pt':
            sim[(t[1], t[2])]['first'][t[3]] = float(t[4]); sim[(t[1], t[2])]['mean'][t[3]] = float(t[5])
        elif t[0] == 'cell':
            sim[(t[1], t[2])]['cells'][(int(t[3]), int(t[4]))][t[5]] = float(t[6])
        elif t[0] == 'diag':
            sim[(t[1], t[2])]['diag'] = '\t'.join(t[3:])

    def lead(d):
        return max(d, key=d.get) if d and max(d.values()) > 0 else None

    def lead_rel(d):
        r = defaultdict(float)
        for f, v in d.items():
            r[rel(f)] += v
        return lead(r)

    def score(ks):
        s = dict(points=0, lead_f=0, lead_r=0, cells=0, cell_f=0, cell_r=0, err=[], ratio=[], rows=[])
        for k in ks:
            c, m = cen[k], sim[(k[1], '%s|%s' % (k[0], k[2]))]
            tc, tm = sum(c['local'].values()), sum(m['mean'].values())
            s['err'].append(abs(math.log2(max(tm, 0.1) / max(tc, 0.1)))); s['ratio'].append(math.log2(max(tm, 0.1) / max(tc, 0.1)))
            row = dict(point=k[2], census=k[0], census_lead='%s/%s' % (lead(c['spawn']), lead_rel(c['spawn'])),
                       sim_lead='%s/%s' % (lead(m['first']), lead_rel(m['first'])), census_count=round(tc, 2), sim_count=round(tm, 2),
                       census_first={f: v for f, v in sorted(c['spawn'].items(), key=lambda x: -x[1])},
                       sim_first={f: round(v, 2) for f, v in sorted(m['first'].items(), key=lambda x: -x[1]) if v >= 0.05}, diag=m['diag'])
            s['rows'].append(row)
            if c['spawn']:
                s['points'] += 1
                s['lead_f'] += lead(c['spawn']) == lead(m['first'])
                s['lead_r'] += lead_rel(c['spawn']) == lead_rel(m['first'])
            for cell, d in c['cells'].items():
                s['cells'] += 1
                md = m['cells'].get(cell, {})
                s['cell_f'] += lead(d) == lead(md)
                s['cell_r'] += lead_rel(d) == lead_rel(md)
        pct = lambda a, b: round(100.0 * a / b) if b else None
        return dict(points=s['points'], lead_f=pct(s['lead_f'], s['points']), lead_r=pct(s['lead_r'], s['points']), cells=s['cells'],
                    cell_f=pct(s['cell_f'], s['cells']), cell_r=pct(s['cell_r'], s['cells']),
                    count_err=round(sum(s['err']) / len(s['err']), 2), count_bias_log2=round(sum(s['ratio']) / len(s['ratio']), 2), rows=s['rows'])

    groups = defaultdict(list)
    for k in keys:
        groups['%s %s' % (k[1], k[0])].append(k)
        groups[k[1]].append(k)
        groups['all'].append(k)
    res = {g: score(ks) for g, ks in sorted(groups.items())}
    json.dump(res, open(os.path.join(outdir, 'results.json'), 'w'), indent=1)
    for g, r in res.items():
        print('%-45s pts %2d lead F %s%% R %s%% | cells %2d F %s%% R %s%% | count err %.2f bias %+.2f' % (
            g, r['points'], r['lead_f'], r['lead_r'], r['cells'], r['cell_f'], r['cell_r'], r['count_err'], r['count_bias_log2']))
    if '--rows' in sys.argv:
        for r in res['all']['rows']:
            print(r['census'][-6:], r['point'], 'census', r['census_lead'], r['census_count'], r['census_first'], '| sim', r['sim_lead'], r['sim_count'], r['sim_first'])
            print('     ', r['diag'])


if __name__ == '__main__':
    main()
