"""The gap to the speedrunners: where our leg times lose to par, by leg, by segment and by KIND of segment.

The owner (30 Oct 2026): a gate result is not "OK" - it is good when a leg time came significantly closer to the
runners' times, reached parity or beat it; the autopilot must BEAT the runners in speedrun mode. This tool reads only
(never writes): docs\\splits_reference.tsv (par per checkpoint = the golden run, splits.py build), docs\\checkpoints.tsv
(the segments and what happens in them), research\\experience\\legs\\splits.tsv (our per-segment history, splits.py
history) and legs.tsv (the legs' results), and docs\\LEG_DOSSIER.md (the runners' notes per leg).

Commands (research\\tools\\ocr\\.venv\\Scripts\\python.exe -I tools\\gapreport.py ...):
  gate <tag> [--prev <tag>]  per leg of the gate <leg>-gate-<tag>: total vs par (s, %), the change vs the previous gate
                             (--prev; else vs the leg's best set median), the 3 segments furthest over par with a runner
                             line from the dossier, the verdict BEAT PAR / AT PAR (within 2 %) / CLOSER (the gap shrank
                             by >= max(5 s, 10 % of the reference gap)) / SAME / FURTHER / FAIL at <checkpoint>, and the
                             summary (sum of gaps, its change)
  rank <leg>                 the leg's checkpoint names, one per line, by segment gap (median of the recent full passes
                             - par), largest first; checkpoints without our data last, in leg order (flgate.ps1
                             -FreshSaves reads this)
  decompose [--list] [--matrix]
                             every segment of the 14 legs classified by KIND (below), (recent median - par) summed per
                             kind, the gold gap beside it, and the top 15 single segments; --list prints every segment's
                             kind and the keyword that decided it, --matrix the gap per leg x kind
  selftest                   checks on synthetic data (classification, verdicts, ranking, medians, dossier match)

"Recent median" = per segment the median over the leg's last RECENT (10) full passes (start save of the leg, PASS, the
end checkpoint reached) - the current behaviour, the same window splits.py uses for the spread. Gold = our best time of
the segment in any full or part run (splits.py's gold). Seconds are scene-free like par (HDE's Esc skips room scenes).

Kinds (from checkpoints.tsv's segment text, its `ours` column; docs\\research\\gap_decomposition.md explains them):
  base      a base visit: room / NPC / restock, Esc on room scenes, the launch and any space-enter scene after it (every
            leg's first segment, ours = control, and "X stop, launch", "room scene")
  fight     killing or defending: fighters, wings, gunboats, platforms, generators, a battleship (ours = kill:<n>)
  escort    pace set by a companion or convoy: meet, formation, escort, follow, re-form, shed a companion
  lane      trade-lane rides (entry queue, the ride, the exit)
  jump      gates and jump holes, incl. "X hole -> Y" (transit and the flight on in the next system)
  dock      the approach to a base / planet and the docking itself (a segment whose activity is "to X, dock")
  cruise    open-space travel to a point: waypoints, "to the ...", the ring race, the M12 core run
  scripted  waits on the script: a spawn ("appears"), a banner, a scan, a tractor pickup, a space scene - only when no
            activity above is named
The first activity named in the text decides (the texts name a segment's parts in order; a mixed segment such as
"Sprague fight, dock" counts for its first part) - an approximation: the logs do not time a segment's parts.
"""
import csv
import os
import re
import statistics
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
FL = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CPF = os.path.join(FL, 'docs', 'checkpoints.tsv')
REFF = os.path.join(FL, 'docs', 'splits_reference.tsv')
LEGREF = os.path.join(FL, 'docs', 'legs_reference.tsv')
DOSSIER = os.path.join(FL, 'docs', 'LEG_DOSSIER.md')
LEGS = os.path.join(FL, 'research', 'experience', 'legs')
HIST = os.path.join(LEGS, 'splits.tsv')
LEGTSV = os.path.join(LEGS, 'legs.tsv')
LEG_ORDER = ['M01a', 'M01b', 'M02', 'M03', 'M04', 'M05', 'M06', 'M07', 'M08', 'M09', 'M10', 'M11', 'M12', 'M13']
RECENT = 10          # = splits.py RECENT
SET_MIN_RUNS = 2     # = splits.py SET_MIN_RUNS (a set of 2+ full legs counts for the best set median)
AT_PAR_X = 0.02      # AT PAR: within 2 % over par
CLOSER_MIN, CLOSER_X = 5.0, 0.10   # CLOSER / FURTHER: the gap moved by >= max(5 s, 10 % of the reference gap)


# ---------------------------------------------------------------- reading
def rtsv(path):
    with open(path, encoding='utf-8', errors='replace') as f:
        return list(csv.DictReader(f, delimiter='\t'))


def num(s):
    try:
        return float(s)
    except (TypeError, ValueError):
        return None


def leg_of(label):
    m = re.match(r'(M\d\d[ab]?)', label or '')
    return m.group(1) if m else None


def load_checkpoints(path=CPF):
    legs = {}
    for r in rtsv(path):
        legs.setdefault(r['leg'], []).append(r)
    return legs


def load_par(path=REFF):
    """{leg: {cp: (par at, par segment)}} from splits_reference.tsv."""
    par = {}
    for r in rtsv(path):
        par.setdefault(r['leg'], {})[r['cp']] = (num(r['par']), num(r['par_seg']))
    return par


def load_runs(path=HIST):
    """{dir: {info: first row, cp: {cp: (at, seg)}}} from splits.tsv."""
    runs = {}
    for r in rtsv(path):
        v = runs.setdefault(r['dir'], dict(info=r, cp={}))
        v['cp'][r['cp']] = (num(r['at']), num(r['seg']))
    return runs


def load_legrows(path=LEGTSV):
    """{(date, label): legs.tsv row} - the leg's outcome and detail (FAIL reason)."""
    return {(r['date'], r['label']): r for r in rtsv(path)} if os.path.isfile(path) else {}


# ---------------------------------------------------------------- our numbers
def full_passes(runs, leg, cps):
    """The leg's full PASS runs that reached its end checkpoint, oldest first (dir names start with the time)."""
    end = cps[-1]['cp']
    out = [v for v in runs.values() if leg_of(v['info']['label']) == leg and v['info']['start'] == 'full'
           and v['info']['outcome'] == 'PASS' and v['cp'].get(end, (None,))[0] is not None]
    return sorted(out, key=lambda v: v['info']['dir'])


def seg_stats(runs, leg, cps, recent=RECENT):
    """Per checkpoint: (median of the last `recent` full passes' segment or None, n, gold or None)."""
    rec = full_passes(runs, leg, cps)[-recent:]
    out = {}
    for cp in cps:
        k = cp['cp']
        xs = [v['cp'][k][1] for v in rec if v['cp'].get(k, (None, None))[1] is not None]
        gold = [v['cp'][k][1] for v in runs.values() if leg_of(v['info']['label']) == leg and v['info']['start'] != 'seg'
                and v['cp'].get(k, (None, None))[1] is not None and v['cp'][k][1] >= 0]
        out[k] = (statistics.median(xs) if xs else None, len(xs), min(gold) if gold else None)
    return out


def rank(cps, stats, par):
    """Checkpoint names by (recent median - par segment), largest first; those without data last, in leg order."""
    known, unknown = [], []
    for i, cp in enumerate(cps):
        med = stats.get(cp['cp'], (None,))[0]
        ps = par.get(cp['cp'], (None, None))[1]
        if med is None or ps is None:
            unknown.append(cp['cp'])
        else:
            known.append((-(med - ps), i, cp['cp']))
    return [k for _, _, k in sorted(known)] + unknown


# ---------------------------------------------------------------- kinds
KINDS = ['base', 'fight', 'escort', 'lane', 'jump', 'dock', 'cruise', 'scripted']
# activity keywords (case-insensitive); the earliest match in the text decides. "the wing / patrol appears" is a spawn
# wait, not a fight; "base explosion" is the M01b space scene; "to the <x>" without a gate / hole / lane is travel
ACTIVITY = [
    ('fight', r"\bfight|\bkill|\bdefen[cs]e\b|\bdefend|\battack|\bwing\b(?! appears)|\bfighters?\b|\bhostiles\b|\bplatforms\b"
              r"|\bgenerators?\b|\bthe battleship\b|\bpatrol\b(?! appears)|\brogues?\b|\benemy ships\b|\bforces\b|interceptors"
              r"|\bblockade\b|\bgunboat\b|\bhunters\b|\bsalvo\b|\bpirates\b|experimental ship|\bchase\b(?! scene)|\bdisable\b"
              r"|\bfleet\b|\bambush\b"),
    ('escort', r"\bescort|\bformation\b|\bfollow\b|\bmeet\b|\bre-form\b|\bshed\b|\bconvoy\b"),
    ('base', r"\broom scenes?\b|\bbase\b(?! explosion| appears)|\bstop\b|\blaunch"),
    ('dock', r"\bdock"),
    ('lane', r"\blanes?\b"),
    ('jump', r"\bgate\b|\bhole\b|->"),
    ('cruise', r"^to the\b(?![^,(]*\b(hole|gate|lanes?)\b)|\bwaypoints?\b|\bhead for\b|\bacross\b|\bfly to\b|\bopen cruise\b"
               r"|\bring race\b|\bafterburner\b"),
]
SCRIPTED = r"\bscenes?\b|\bwait\b|\bappears\b|\bbanner\b|\bturns at\b|\bscan\b|\btractor\b|\buncloaks\b"


def classify(text, ours=''):
    """-> (kind, the keyword that decided it). ours = control -> base, kill:<n> -> fight (the log's own marker)."""
    if ours == 'control':
        return 'base', 'ours=control'
    if (ours or '').startswith('kill:'):
        return 'fight', 'ours=' + ours
    t = text or ''
    best = None
    for kind, rx in ACTIVITY:
        m = re.search(rx, t, re.I)
        if m and (best is None or m.start() < best[2]):
            best = (kind, m.group(0), m.start())
    if best:
        return best[0], best[1]
    m = re.search(SCRIPTED, t, re.I)
    if m:
        return 'scripted', m.group(0)
    return 'cruise', '(destination only)'


# ---------------------------------------------------------------- the dossier
STOP = set('the a an to at of and in on with from by for its it is as or no not one two all their they them s m km '
           'runners runner run only then when once first last into out up per'.split())
GENERIC = set('fight lane lanes dock base launch stop hole gate scene scenes cruise escort waypoint leg wave fighter '
              'fighters ships ship station defend defence formation follow meet'.split())


def split_clauses(s):
    """Split at '; ' and at '. ' before a capital / bold, never inside parentheses."""
    out, depth, cur, i = [], 0, '', 0
    while i < len(s):
        ch = s[i]
        depth += (ch == '(') - (ch == ')' and depth > 0)
        if depth == 0 and (ch == ';' or (ch == '.' and re.match(r'\.\s+[A-Z*]', s[i:i + 3]))) and \
                i + 1 < len(s) and s[i + 1] == ' ':
            out.append(cur + (ch if ch == '.' else ''))
            cur = ''
            i += 2
            continue
        cur += ch
        i += 1
    return out + ([cur] if cur.strip() else [])


def dossier_lines(path=DOSSIER):
    """{leg: [clause, ...]} from each leg's "How each runner does it" bullets (a clause = a bullet's sentence or ';'
    part, prefixed with the bullet's "Run n:" / "All three:")."""
    out = {}
    if not os.path.isfile(path):
        return out
    leg, inside, bullets = None, False, []
    for line in open(path, encoding='utf-8', errors='replace'):
        h = re.match(r'^## (M\d\d[ab]?) - ', line)
        if line.startswith('## '):
            leg, inside = (h.group(1) if h else None), False
            continue
        if leg and line.startswith('**How each runner does it**'):
            inside = True
            continue
        if inside and line.startswith('**'):
            inside = False
            continue
        if inside and leg:
            if line.startswith('- '):
                bullets.append([leg, line[2:].strip()])
            elif line.startswith('  ') and bullets and bullets[-1][0] == leg:
                bullets[-1][1] += ' ' + line.strip()
    for leg, b in bullets:
        m = re.match(r'^((?:Run \d|All three|Runs? [\d /]+)[^:]{0,12}):\s*', b)
        pre = m.group(1) + ': ' if m else ''
        body = b[m.end():] if m else b
        for c in split_clauses(body):
            c = c.strip()
            if len(c) > 12:
                out.setdefault(leg, []).append((pre, c))
    return out


def words(s):
    return [w for w in re.findall(r"[a-z][a-z0-9'-]+", s.lower()) if w not in STOP and len(w) >= 3]


WIDTH = 150


def dossier_match(cp, text, clauses):
    """The clause (pre, text) sharing the most distinctive words with the segment text and the checkpoint's name
    (specific words 2, generic ones 1, matched on their first 5 letters; a shop / trade clause -1 outside a base
    segment); None under a score of 2 or without one specific word. A long clause is shown from the part before its
    first matched word."""
    want = {}
    for w in words(text) + words(re.sub(r'^M\d\d[ab]?\.', '', cp)):
        want[w[:5]] = max(want.get(w[:5], 0), 1 if w in GENERIC else 2)
    shop = classify(text)[0] != 'base'
    best, score = None, 0
    for pre, c in clauses:
        have = {w[:5] for w in words(c)}
        if not any(v == 2 and k in have for k, v in want.items()):
            continue
        s = sum(v for k, v in want.items() if k in have)
        if shop and re.search(r'\b(sells?|sold|buys?|shops?|carries|trade)\b', c):
            s -= 1
        if s > score or (s == score and best is not None and len(c) < len(best[1])):
            best, score = (pre, c), s
    if score < 2:
        return None
    pre, c = best
    if len(pre) + len(c) > WIDTH:
        hits = [(m.start(), want[m.group(0).lower()[:5]]) for m in re.finditer(r"[A-Za-z][A-Za-z0-9'-]+", c)
                if m.group(0).lower()[:5] in want]
        if hits:
            # the window (of ~100 characters) that holds the most matched weight starts at its first hit
            k = max(hits, key=lambda h: (sum(w for p, w in hits if h[0] <= p < h[0] + 100), -h[0]))[0]
            cut = max(c.rfind(', ', 0, k), c.rfind('(', 0, k), c.rfind('; ', 0, k))
            start = cut + 1 if cut >= 0 and k - cut < 60 else max(0, k - 20)
            c = ('...' if start else '') + c[start:].lstrip(' ,;')
    s = pre + c
    return s if len(s) <= WIDTH else s[:WIDTH - 3] + '...'


# ---------------------------------------------------------------- the gate
def verdict(total, par, ref_total):
    """BEAT PAR / AT PAR / CLOSER / SAME / FURTHER / NO REF for a finished leg (total, par, the reference total or
    None) -> (verdict, the gap change or None)."""
    gap = total - par
    if total < par:
        return 'BEAT PAR', (gap - (ref_total - par)) if ref_total is not None else None
    if total <= par * (1 + AT_PAR_X):
        return 'AT PAR', (gap - (ref_total - par)) if ref_total is not None else None
    if ref_total is None:
        return 'NO REF', None
    rgap = ref_total - par
    d = gap - rgap
    thr = max(CLOSER_MIN, CLOSER_X * rgap) if rgap > 0 else CLOSER_MIN
    return ('CLOSER' if d <= -thr else 'FURTHER' if d >= thr else 'SAME'), d


def group_sets(rows):
    """[(name, rows)]: splits.py run_sets (a label = a set, a reused label cut by build stamp / 6 h gaps) when it
    imports, else by label alone."""
    try:
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        import splits   # (read-only use of its set rule: one definition of a set)
        return [(name, rs) for _, name, rs in splits.run_sets(rows)]
    except Exception:   # (splits.py missing or changed: the plain label grouping)
        by = {}
        for r in rows:
            by.setdefault(r['label'], []).append(r)
        return list(by.items())
    finally:
        if sys.path and sys.path[0] == os.path.dirname(os.path.abspath(__file__)):
            sys.path.pop(0)


def best_set_median(runs, leg, cps, before=None, exclude=()):
    """The leg's best set median of the end checkpoint's time: its full passes grouped into sets (group_sets), sets of
    SET_MIN_RUNS+ runs started before `before` (a date string); -> (median, set name) or (None, None)."""
    end = cps[-1]['cp']
    rows = []
    for v in full_passes(runs, leg, cps):
        if v['info']['label'] in exclude or (before and v['info']['date'] >= before):
            continue
        rows.append(dict(v['info'], _t=v['cp'][end][0]))
    best = (None, None)
    for name, rs in group_sets(rows):
        if len(rs) >= SET_MIN_RUNS:
            m = statistics.median(r['_t'] for r in rs)
            if best[0] is None or m < best[0]:
                best = (m, name)
    return best


def gate_runs(runs, tag, leg):
    rx = re.compile(r'^' + re.escape(leg) + r'-gate-' + re.escape(tag) + r'$')
    return sorted((v for v in runs.values() if rx.match(v['info']['label'])), key=lambda v: v['info']['dir'])


def leg_result(v, cps, par):
    """(total at the end checkpoint or None, the last checkpoint reached, its time, par there)."""
    end = cps[-1]['cp']
    tot = v['cp'].get(end, (None,))[0] if v['info']['outcome'] == 'PASS' else None
    last = None
    for cp in cps:
        if v['cp'].get(cp['cp'], (None,))[0] is not None:
            last = cp['cp']
    at = v['cp'][last][0] if last else None
    return tot, last, at, par.get(last, (None,))[0] if last else None


def cmd_gate(cps_all, par_all, runs, legrows, clauses, tag, prev=None):
    print(f'gate {tag}' + (f' vs gate {prev}' if prev else ' vs each leg\'s best set median (before the gate)') +
          ' - gap = ours - par (scene-free seconds); verdicts: BEAT PAR / AT PAR (<= 2 %) / CLOSER / SAME / FURTHER')
    sums = dict(gap=0.0, ref=0.0, n=0)
    counts = {}
    missing = []
    for leg in LEG_ORDER:
        cps, par = cps_all.get(leg), par_all.get(leg, {})
        if not cps:
            continue
        rs = gate_runs(runs, tag, leg)
        if not rs:
            missing.append(leg)
            continue
        v = rs[-1]
        lr = legrows.get((v['info']['date'], v['info']['label']), {})
        p_end = par.get(cps[-1]['cp'], (None,))[0]
        tot, last, at, p_last = leg_result(v, cps, par)
        # the reference: the previous gate's same leg (finished), else the best set median before this gate
        ref, refname = None, None
        pv = gate_runs(runs, prev, leg)[-1] if prev and gate_runs(runs, prev, leg) else None
        if pv is not None:
            ptot = leg_result(pv, cps, par)[0]
            if ptot is not None:
                ref, refname = ptot, f'gate {prev}'
            else:
                refname = f'gate {prev} FAILED'
        if ref is None:
            m, lab = best_set_median(runs, leg, cps, before=v['info']['date'][:16],
                                     exclude={f'{leg}-gate-{tag}'})
            if m is None:
                # no set of 2+ full passes (legs flown mostly in one-run gates): the median of the recent full passes
                end = cps[-1]['cp']
                xs = [r['cp'][end][0] for r in full_passes(runs, leg, cps)
                      if r['info']['date'] < v['info']['date'][:16] and r['info']['label'] != f'{leg}-gate-{tag}'][-RECENT:]
                if xs:
                    m, lab = statistics.median(xs), f'median of the last {len(xs)} full passes'
            elif m is not None:
                lab = f'best set median {lab}'
            if m is not None:
                ref, refname = m, (refname + '; ' if refname else '') + lab
        extra = f' ({len(rs)} runs, the last shown)' if len(rs) > 1 else ''
        if tot is None:
            vd = 'FAIL at ' + (last or 'start')
            why = (lr.get('detail') or '')[:110]
            print(f'== {leg} {vd}{extra}: {lr.get("secs", "?")} s, last checkpoint {last} at {at if at is not None else "-"}'
                  f' (par there {p_last if p_last is not None else "-"}); {why}')
        else:
            vd, d = verdict(tot, p_end, ref)
            gap = tot - p_end
            sums['gap'] += gap
            sums['n'] += 1
            if ref is not None:
                sums['ref'] += ref - p_end
            else:
                sums['ref'] += gap
            refs = (f'ref {ref:.0f} ({refname}, gap {ref - p_end:+.0f})' if ref is not None else 'no reference')
            ch = f', gap change {d:+.0f}' if d is not None else ''
            print(f'== {leg} {vd}{extra}: {tot:.0f} s vs par {p_end:.0f} = {gap:+.0f} s ({100 * gap / p_end:+.1f} %); '
                  f'{refs}{ch}')
        counts[vd.split(' at ')[0]] = counts.get(vd.split(' at ')[0], 0) + 1
        # the 3 segments furthest over par in this run
        segs = []
        for cp in cps:
            s = v['cp'].get(cp['cp'], (None, None))[1]
            ps = par.get(cp['cp'], (None, None))[1]
            if s is not None and ps is not None:
                segs.append((s - ps, cp, s, ps))
        for g, cp, s, ps in sorted(segs, key=lambda x: -x[0])[:3]:
            pg = ''
            if pv is not None:
                s0 = pv['cp'].get(cp['cp'], (None, None))[1]
                if s0 is not None:
                    pg = f', prev {s0 - ps:+.0f}'
            kind = classify(cp.get('segment', ''), cp.get('ours', ''))[0]
            print(f'   {cp["cp"]:18} {s:6.1f} vs par {ps:6.1f} = {g:+6.1f}{pg}  [{kind}] {cp.get("segment", "")[:70]}')
            line = dossier_match(cp['cp'], cp.get('segment', ''), clauses.get(leg, []))
            if line:
                print(f'      runners: {line}')
    if sums['n']:
        print(f'SUMMARY {sums["n"]} finished legs: sum of gaps {sums["gap"]:+.0f} s vs reference {sums["ref"]:+.0f} s '
              f'= change {sums["gap"] - sums["ref"]:+.0f} s; ' + ', '.join(f'{k} {n}' for k, n in sorted(counts.items())))
    else:
        print('SUMMARY no finished leg in this gate')
    if missing:
        print('not in splits.tsv (running, not yet run, or not yet regenerated): ' + ', '.join(missing))


# ---------------------------------------------------------------- decompose
def decompose_rows(cps_all, par_all, runs):
    """[(leg, cp row, kind, keyword, median, n, gold, par seg)] for every segment."""
    out = []
    for leg in LEG_ORDER:
        cps = cps_all.get(leg)
        if not cps:
            continue
        st = seg_stats(runs, leg, cps)
        for cp in cps:
            kind, kw = classify(cp.get('segment', ''), cp.get('ours', ''))
            med, n, gold = st[cp['cp']]
            out.append((leg, cp, kind, kw, med, n, gold, par_all.get(leg, {}).get(cp['cp'], (None, None))[1]))
    return out


def cmd_decompose(cps_all, par_all, runs, args):
    rows = decompose_rows(cps_all, par_all, runs)
    if '--list' in args:
        print(f'{"checkpoint":18} {"kind":9} {"keyword":22} {"median":>7} {"par":>6} {"gap":>7}  segment')
        for leg, cp, kind, kw, med, n, gold, ps in rows:
            g = f'{med - ps:+7.1f}' if med is not None and ps is not None else '      -'
            print(f'{cp["cp"]:18} {kind:9} {kw[:22]:22} {med if med is not None else float("nan"):7.1f} '
                  f'{ps if ps is not None else float("nan"):6.1f} {g}  {cp.get("segment", "")[:80]}')
        print()
    agg = {k: dict(n=0, par=0.0, med=0.0, gap=0.0, pos=0.0, gold=0.0, nd=0) for k in KINDS}
    total = 0.0
    for leg, cp, kind, kw, med, n, gold, ps in rows:
        a = agg[kind]
        a['n'] += 1
        if med is None or ps is None:
            a['nd'] += 1
            continue
        a['par'] += ps
        a['med'] += med
        a['gap'] += med - ps
        a['pos'] += max(0.0, med - ps)
        a['gold'] += (gold - ps) if gold is not None else (med - ps)
        total += med - ps
    print(f'Gap by kind of segment: sum over all {len(rows)} segments of the 14 legs of (median of the last {RECENT} full '
          f'passes - par); gold gap = our best ever segment - par')
    print(f'| Kind | Segments | Par s | Ours s (median) | Gap s | Share | Gap of segments over par | Gold gap s |')
    print('|---|---|---|---|---|---|---|---|')
    for k in sorted(KINDS, key=lambda k: -agg[k]['gap']):
        a = agg[k]
        share = f'{100 * a["gap"] / total:.0f} %' if total else '-'
        nd = f' ({a["nd"]} without data)' if a['nd'] else ''
        print(f'| {k} | {a["n"]}{nd} | {a["par"]:.0f} | {a["med"]:.0f} | {a["gap"]:+.0f} | {share} | {a["pos"]:+.0f} | '
              f'{a["gold"]:+.0f} |')
    print(f'| **all** | {len(rows)} | {sum(a["par"] for a in agg.values()):.0f} | {sum(a["med"] for a in agg.values()):.0f} | '
          f'**{total:+.0f}** | 100 % | {sum(a["pos"] for a in agg.values()):+.0f} | {sum(a["gold"] for a in agg.values()):+.0f} |')
    if '--matrix' in args:
        print()
        print('| Leg | ' + ' | '.join(KINDS) + ' | leg |')
        print('|---' * (len(KINDS) + 2) + '|')
        for leg in LEG_ORDER:
            cells, lt = [], 0.0
            for k in KINDS:
                xs = [med - ps for l, cp, kind, kw, med, n, gold, ps in rows if l == leg and kind == k
                      and med is not None and ps is not None]
                cells.append(f'{sum(xs):+.0f}' if xs else '')
                lt += sum(xs)
            print(f'| {leg} | ' + ' | '.join(cells) + f' | {lt:+.0f} |')
    print()
    print('Top 15 single segments (recent median - par):')
    print('| # | Checkpoint | Kind | Ours (n) | Par | Gap | Gold gap | Segment |')
    print('|---|---|---|---|---|---|---|---|')
    top = sorted((r for r in rows if r[4] is not None and r[7] is not None), key=lambda r: -(r[4] - r[7]))[:15]
    for i, (leg, cp, kind, kw, med, n, gold, ps) in enumerate(top, 1):
        gg = f'{gold - ps:+.0f}' if gold is not None else '-'
        print(f'| {i} | {cp["cp"]} | {kind} | {med:.0f} ({n}) | {ps:.0f} | {med - ps:+.0f} | {gg} | {cp.get("segment", "")[:90]} |')


# ---------------------------------------------------------------- selftest
def cmd_selftest():
    fails = []

    def check(name, got, want):
        if got != want:
            fails.append(f'{name}: got {got!r}, want {want!r}')

    # classification: the earliest activity decides; overrides; the scripted tier; the exclusions
    for text, ours, want in [
        ('Manhattan base, launch + the M02 space-enter scene (55 s)', 'control', 'base'),
        ('cruise hop (charge on the kill), generator 2', 'kill:2', 'fight'),
        ('Sprague fight, dock', 'land', 'fight'),
        ('to Buffalo, dock', 'land', 'dock'),
        ('the Order wing appears', 'obj:1', 'scripted'),
        ('to the arch, the patrol appears', 'obj:1', 'cruise'),
        ('the Tekagi patrol (a kill race against Ozu\'s dialogue nag)', 'obj:1', 'fight'),
        ('to the Leeds hole', 'obj:1', 'jump'),
        ('to the scan area', 'obj:1', 'cruise'),
        ('scan at once (Cosmo acts on the scan)', 'obj:1', 'scripted'),
        ('Benford room scene (runners watch ~200 s; Esc on HDE), launch, station attack', 'obj:1', 'base'),
        ('fleet scenes (68 s) + the Schiller fleet fight', 'obj:1', 'fight'),
        ('the two weapon platforms only, base explosion scene, GAME SAVED', 'obj:1', 'fight'),
        ('lanes to the Tau-31 gate', 'obj:1', 'lane'),
        ('follow Walker to Mitchell, shed the companions, dock', 'land', 'escort'),
        ('across the core (wheel gap, plate hole), tractor the cell in passing', 'obj:1', 'cruise'),
        ('to Battleship Missouri, dock', 'land', 'dock'),
        ('to Sprague', 'obj:1', 'cruise'),
        ('Leeds hole -> Leeds (lanes, ring)', 'land', 'jump'),
    ]:
        check('classify ' + text[:30], classify(text, ours)[0], want)
    # verdicts: thresholds max(5 s, 10 % of the reference gap), AT PAR within 2 %
    check('beat', verdict(990, 1000, 1100)[0], 'BEAT PAR')
    check('at par', verdict(1019, 1000, 1100)[0], 'AT PAR')
    check('closer 10 %', verdict(1180, 1000, 1200)[0], 'CLOSER')        # gap 200 -> 180: -20 = 10 % of 200
    check('same', verdict(1185, 1000, 1200)[0], 'SAME')                 # -15 < 20
    check('further', verdict(1220, 1000, 1200)[0], 'FURTHER')
    check('closer 5 s floor', verdict(1025, 1000, 1030)[0], 'CLOSER')   # gap 30 -> 25: 5 s floor (10 % = 3)
    check('same under 5 s', verdict(1026, 1000, 1030)[0], 'SAME')
    check('no ref', verdict(1100, 1000, None)[0], 'NO REF')
    check('gap change', verdict(1180, 1000, 1200)[1], -20)
    # synthetic history: leg M99 with three checkpoints, 12 full passes (the first 2 old and slow), a part run, a seg run
    cps = [dict(cp='M99.a', ours='control', segment='base'), dict(cp='M99.b', ours='obj:1', segment='Order fight'),
           dict(cp='M99.c', ours='land', segment='to X, dock')]
    par = {'M99.a': (10.0, 10.0), 'M99.b': (30.0, 20.0), 'M99.c': (60.0, 30.0)}
    runs = {}

    def add(d, label, start, outcome, segs, date):
        at, cp = 0.0, {}
        for k, s in zip(['M99.a', 'M99.b', 'M99.c'], segs):
            if s is None:
                break
            at += s
            cp[k] = (at, s)
        runs[d] = dict(info=dict(dir=d, label=label, start=start, outcome=outcome, date=date), cp=cp)
    for i in range(12):
        slow = 100.0 if i < 2 else 0.0
        add(f'2026-10-{10 + i:02d}_0000_M99-set{i // 3}_i1', f'M99-set{i // 3}', 'full', 'PASS',
            [12.0, 30.0 + i + slow, 31.0], f'2026-10-20 {i:02d}:00')   # (hours apart: one set per label)
    add('2026-10-29_0000_M99-p_i1', 'M99-p', 'part', 'FAIL', [None], '2026-10-29 00:00')
    runs['2026-10-29_0000_M99-p_i1']['cp'] = {'M99.b': (50.0, 15.0)}          # a part run's segment: a gold
    add('2026-10-29_0100_M99-s_i1', 'M99-s', 'seg', 'PASS', [5.0, 5.0, 5.0], '2026-10-29 01:00')   # never a gold
    add('2026-10-30_0000_M99-gate-t1_i1', 'M99-gate-t1', 'full', 'FAIL', [12.0, None], '2026-10-30 00:00')
    st = seg_stats(runs, 'M99', cps)
    check('recent window', st['M99.b'][1], 10)                               # the last 10 of 12 full passes
    check('recent median', st['M99.b'][0], statistics.median([30.0 + i for i in range(2, 12)]))
    check('gold incl. part, not seg', st['M99.b'][2], 15.0)
    check('rank', rank(cps, st, par), ['M99.b', 'M99.a', 'M99.c'])            # +16.5, +2, +1
    st2 = dict(st)
    st2['M99.a'] = (None, 0, None)
    check('rank unknown last', rank(cps, st2, par)[-1], 'M99.a')
    check('failed gate total', leg_result(runs['2026-10-30_0000_M99-gate-t1_i1'], cps, par)[:2], (None, 'M99.a'))
    m, lab = best_set_median(runs, 'M99', cps, before='2026-10-30 00:00')
    check('best set median', (m, lab), (12.0 + 34.0 + 31.0, 'M99-set1'))     # set1 = i 3..5: b 33..35 (set0 holds the slow)
    # dossier: the clause with the segment's distinctive words wins; nothing under a score of 2
    cl = [('Run 1: ', 'Glorious: ~44 s fight in the asteroid field with torpedoes'), ('Run 2: ', 'race at cruise from GO'),
          ('Run 3: ', 'sells 30 Glorious torpedoes at the Hood')]
    check('dossier match', dossier_match('M06.gloriousland', 'Glorious defence (torpedoes at the station), dock', cl),
          'Run 1: Glorious: ~44 s fight in the asteroid field with torpedoes')   # (the shop clause scores 1 less)
    check('dossier none', dossier_match('M99.b', 'lanes', cl), None)
    long = [('Run 1: ', 'base 19 s (restock), launch 16 s, to XT-19 85 s, defend 61 s, three waypoint legs of 62 / 62 / '
             '10 s with fighter waves of 41 / 25 / 21 s, platforms only 32 s (v=1060-1092), explosion scene 18 s')]
    check('dossier window', dossier_match('M99.end', 'the two weapon platforms only', long)[:27], 'Run 1: ...platforms only 32')
    if fails:
        print('SELFTEST FAILED')
        for f in fails:
            print('  ' + f)
        return False
    print('SELFTEST OK')
    return True


# ---------------------------------------------------------------- main
def main():
    a = sys.argv[1:]
    if not a or a[0] in ('-h', '--help', 'help'):
        print(__doc__)
        return 0
    c = a[0]
    if c == 'selftest':
        return 0 if cmd_selftest() else 1
    cps_all = load_checkpoints()
    par_all = load_par()
    runs = load_runs()
    if c == 'rank':
        if len(a) < 2 or a[1] not in cps_all:
            print(f'rank: unknown leg {a[1] if len(a) > 1 else "(none)"} (known: {", ".join(cps_all)})', file=sys.stderr)
            return 1
        leg = a[1]
        for k in rank(cps_all[leg], seg_stats(runs, leg, cps_all[leg]), par_all.get(leg, {})):
            print(k)
        return 0
    if c == 'gate':
        if len(a) < 2:
            print('usage: gate <tag> [--prev <tag>]', file=sys.stderr)
            return 1
        prev = a[a.index('--prev') + 1] if '--prev' in a and a.index('--prev') + 1 < len(a) else None
        cmd_gate(cps_all, par_all, runs, load_legrows(), dossier_lines(), a[1], prev)
        return 0
    if c == 'decompose':
        cmd_decompose(cps_all, par_all, runs, a[1:])
        return 0
    print(f'unknown command {c}', file=sys.stderr)
    print(__doc__)
    return 1


if __name__ == '__main__':
    sys.exit(main())
