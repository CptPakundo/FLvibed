"""gate_harvest.py - a leg gate's segments as A-arm (baseline) samples for `splits.py sprt` (WORK_QUEUE H18 (1), the owner,
30 Oct 2026: a ~70 min gate should feed queued work - the autopilot's speedrun times - not only answer "no regression").
A later A/B set of a speed change then flies only its B runs on the gate's build.

A gate leg's segment (checkpoint k-1 -> k of docs\\checkpoints.tsv) is an A sample only when ALL hold (fail closed: any
missing datum excludes, with its reason):
  (a) the leg ran the DEFAULT behaviour in that segment: the gate's switches (--apset, as given to flgate.ps1 -ApSet)
      are none, the measurement-only `situation_shadow 1`, or `situation_arbiter 1` with every arbiter decision in the
      segment an `agree` (`situation arbiter agree|differ` in the leg's FLController.log; agree = the body's own decision
      and plan executed, autopilot_objective.cpp ArbiterExecute) and no `side-effect ... refused` (the game-first gate
      withheld a start the body would have made). A switch the log shows but --apset does not declare excludes the leg
      (the switch set is not recorded per leg anywhere else: flleg.ps1 sends it, the log has only its lines). A decision
      within BOUNDARY_S of a checkpoint excludes both segments it touches; one before the leg's start, the first.
      --strict: a differ / refusal also excludes every later segment (its state - ApplyWrites, position - carries on).
  (b) the build: legs.tsv `commit` is the build stamp (flleg.ps1: build\\dev's, since splits.STAMP_SINCE) and clean (no
      +dirty); every sample carries it - sprt's A and B must be one build (the proposed sprt hook refuses otherwise).
  (c) the segment ended (its checkpoint reached) before any failure: a failed leg's earlier segments count, the one it
      died in and every later one do not.
  (d) not a stop-at run (splits.stopat_row), an HD Edition instance (splits.hde_row), a full leg from its start save.
  (e) the leg finished and its segments are in research\\experience\\legs\\splits.tsv (what sprt reads) with the same value
      as this script's own match of the log (splits.our_splits); a leg still running (only its transcript), with no
      legs.tsv row or not yet in the split history (splits.py update runs after the gate's queue) is left out.
The leg total is a sample (cp `(leg)`) when the leg is a full PASS (splits.full_pass_row) and every segment is one.

  python -I tools\\gate_harvest.py --gate <tag> [--apset 'k v,k v'] [--gate <tag2> [--apset ...]] ... [--strict]
         [--build <commit>] [--out <file.tsv>] [--quiet]
      --apset applies to the --gate before it. Prints per leg the samples and every excluded segment with its reason;
      --out writes the samples (gate, leg, cp, n, dir, seg, at, build, apset) for `splits.py sprt --a-samples <file>`
      (a proposed hook: not in splits.py yet). Exit 0 = read, 2 = refused arguments.
  python -I tools\\gate_harvest.py selftest
"""
import csv
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))   # (-I leaves the script's folder off the path)
import splits  # noqa: E402  (read-only use: checkpoints, archived_runs, log_events, our_splits, history_by_run, rows)

BOUNDARY_S = 1.0     # a decision this close to a checkpoint: both segments it touches are excluded
SEG_TOL = 0.15       # this script's segment vs splits.tsv's (both fmt() to 0.1 s)
# the switches whose ON value keeps the default flight (a)
ALLOWED = {'situation_shadow': {'0', '1'}, 'situation_arbiter': {'0', '1'}}
LINE = re.compile(r'situation (shadow|arbiter) (agree|differ) (\S+)')
REFUSED = re.compile(r'side-effect (\S+) .*\brefused\b')
FOLDER = re.compile(r'^\d{4}-\d\d-\d\d_\d{4,6}_(.+)_i(\d+)$')
HEADER = ['gate', 'leg', 'cp', 'n', 'dir', 'seg', 'at', 'build', 'apset']


def parse_apset(s):
    """'k v,k v' -> ({k: v}, None) or (None, why): only the switches of ALLOWED (fail closed)."""
    out = {}
    for part in [p.strip() for p in (s or '').split(',') if p.strip()]:
        kv = part.split()
        if len(kv) != 2:
            return None, f'--apset entry {part!r}: want "key value"'
        k, v = kv
        if k not in ALLOWED or v not in ALLOWED[k]:
            return None, (f'--apset {k} {v}: not a default-flight switch ({", ".join(sorted(ALLOWED))} 0/1) - a gate with '
                          'another switch flew a changed behaviour and gives no A samples')
        out[k] = v
    return out, None


def switch_events(log, t0):
    """[(t rel to t0, kind, text)]: kind shadow / agree / differ / refused, from the leg's FLController.log."""
    out = []
    with open(log, encoding='latin-1') as f:
        for line in f:
            m = splits.TS.match(line)
            if not m:
                continue
            t = splits.clock(*m.groups())
            if t < t0 - 43200:
                t += 86400.0     # a leg across midnight (as our_splits)
            a = LINE.search(line)
            if a:
                kind = 'shadow' if a.group(1) == 'shadow' else a.group(2)
                out.append((t - t0, kind, line[a.start():].strip()[:160]))
                continue
            r = REFUSED.search(line)
            if r:
                out.append((t - t0, 'refused', line[r.start():].strip()[:160]))
    return out


def fail_time(ev, t0):
    """seconds from t0 of the first failure event (mission FAILURE / player death) at or after the start, else None."""
    for t, k, _ in ev:
        if t < t0 - 43200:
            t += 86400.0
        if k == 'fail' and t >= t0 - 5:
            return t - t0
    return None


def judge_run(cps, row, log, t0, hist, apset, strict=False):
    """One gate leg -> (run reason or '', [(k, cp, seg, at, reason)], total or None). reason '' = an A sample.
    cps = the leg's checkpoints; row = its legs.tsv row (+ 'full'); hist = {cp: (at, seg)} from splits.tsv or None."""
    segs = []
    why = ''
    commit = row.get('commit') or ''
    if not row.get('full'):
        why = 'not a full leg from its start save'
    elif splits.stopat_row(row):
        why = 'stop-at run'
    elif not splits.hde_row(row):
        why = f'instance {row.get("instance")!r} is not an HD Edition one'
    elif not commit or commit.endswith('+dirty') or '+' in commit:
        why = f'build {commit or "(none)"} not clean'
    elif (row.get('date') or '') < splits.STAMP_SINCE:
        why = f'before {splits.STAMP_SINCE}: the commit column is no build stamp'
    elif t0 is None:
        why = 'no leg start in the transcript'
    elif hist is None:
        why = 'not in the split history yet (splits.py update)'
    if why:
        return why, [(k, cp['cp'], None, None, why) for k, cp in enumerate(cps)], None
    ev = splits.log_events(log)
    sp = splits.our_splits(cps, ev, t0, True, None, {})
    sw = switch_events(log, t0)
    kinds = set(k for _, k, _ in sw)
    if 'shadow' in kinds and apset.get('situation_shadow') != '1':
        why = 'the log shows situation shadow lines, --apset does not declare situation_shadow 1'
    elif kinds & {'agree', 'differ'} and apset.get('situation_arbiter') != '1':
        why = 'the log shows situation arbiter lines, --apset does not declare situation_arbiter 1'
    if why:
        return why, [(k, cp['cp'], None, None, why) for k, cp in enumerate(cps)], None
    bad = [(t, k, x) for t, k, x in sw if k in ('differ', 'refused')]
    tf = fail_time(ev, t0)
    first_bad = min((t for t, _, _ in bad), default=None)
    for k, cp in enumerate(cps):
        end = sp[k]
        start = sp[k - 1] if k else 0.0
        if end is None or start is None:
            segs.append((k, cp['cp'], None, None, 'checkpoint not timed (not reached / not seen / its previous not seen)'))
            continue
        seg = end - start
        h = hist.get(cp['cp'], (None, None))
        r = ''
        if tf is not None and end >= tf:
            r = f'the leg failed at {tf:.1f} s, before this checkpoint'
        else:
            lo = -1e9 if k == 0 else start - BOUNDARY_S
            hit = [(t, kd, x) for t, kd, x in bad if lo < t <= end + BOUNDARY_S]
            if hit:
                t, kd, x = hit[0]
                r = f'{kd} at {t:.1f} s: {x}' + (f' (+{len(hit) - 1} more)' if len(hit) > 1 else '')
            elif strict and first_bad is not None and first_bad <= end + BOUNDARY_S:
                r = f'--strict: after the {first_bad:.1f} s differ / refusal'
            elif h[1] is None:
                r = 'splits.tsv has no segment value for it'
            elif abs(h[1] - seg) > SEG_TOL:
                r = f'splits.tsv says {h[1]:.1f} s, the log {seg:.1f} s: mismatch'
        segs.append((k, cp['cp'], round(seg, 1), round(end, 1), r))
    total = None
    if splits.full_pass_row(row) and all(not r and s is not None for _, _, s, _, r in segs):
        total = segs[-1][3]
    return '', segs, total


def gate_runs(tag):
    """(archived runs of the gate (splits.archived_runs dicts), [folders without a legs.tsv row / still running])"""
    rx = re.compile(r'-gate-' + re.escape(tag) + r'$')
    runs = [a for a in splits.archived_runs() if rx.search(a['label'])]
    have = set(os.path.basename(a['dir']) for a in runs)
    other = []
    for f in sorted(os.listdir(splits.LEGS)):
        m = FOLDER.match(f)
        if m and rx.search(m.group(1)) and f not in have and os.path.isdir(os.path.join(splits.LEGS, f)):
            only_tr = os.listdir(os.path.join(splits.LEGS, f)) == ['transcript.txt']
            other.append((f, 'still running (only its transcript)' if only_tr else 'no legs.tsv row / no log (unfinished?)'))
    return runs, other


def harvest(gates, strict=False, build=None, quiet=False):
    """gates = [(tag, apset dict)] -> (sample rows, printed lines)"""
    legs = splits.checkpoints()
    hist = splits.history_by_run()
    rows, out = [], []
    for tag, apset in gates:
        runs, other = gate_runs(tag)
        aps = ','.join(f'{k} {v}' for k, v in sorted(apset.items())) or '-'
        out.append(f'== gate {tag} (apset {aps}): {len(runs)} finished leg(s)' + (f', {len(other)} left out' if other else ''))
        for f, why in other:
            out.append(f'  {f}: LEFT OUT - {why}')
        nseg = nok = 0
        for a in sorted(runs, key=lambda a: splits.LEG_ORDER.index(a['leg']) if a['leg'] in splits.LEG_ORDER else 99):
            name = os.path.basename(a['dir'])
            cps = legs.get(a['leg'])
            if not cps:
                out.append(f'  {name}: LEFT OUT - leg {a["leg"]} has no checkpoints')
                continue
            h = hist.get(name)
            hcp = h['cp'] if h else None
            why, segs, total = judge_run(cps, a, os.path.join(a['dir'], 'FLController.log'), splits.t0_of(a['dir']),
                                         hcp, apset, strict)
            if not why and build and a['commit'] != build:
                why = f'build {a["commit"]}, --build {build}'
                segs = [(k, c, s, t, why) for k, c, s, t, _ in segs]
                total = None
            ok = [s for s in segs if not s[4]]
            nseg += len(segs)
            nok += len(ok)
            out.append(f'  {a["leg"]:5} {name}  {a["outcome"]} {a["secs"]} s  build {a["commit"]}: '
                       f'{len(ok)}/{len(segs)} segment(s) A' + (f', leg total {total:.1f} s A' if total is not None else '') +
                       (f'  - LEFT OUT: {why}' if why else ''))
            for k, cp, seg, at, r in segs:
                if not r:
                    rows.append([tag, a['leg'], cp, k + 1, name, splits.fmt(seg), splits.fmt(at), a['commit'], aps])
                    if not quiet:
                        out.append(f'      A   {cp:20} {seg:7.1f}')
                elif not why and not quiet:
                    out.append(f'      --  {cp:20} {"" if seg is None else f"{seg:7.1f}":7}  {r}')
            if total is not None:
                rows.append([tag, a['leg'], '(leg)', len(cps), name, '', splits.fmt(total), a['commit'], aps])
        out.append(f'   gate {tag}: {nok} of {nseg} segment(s) A samples')
    builds = sorted(set(r[7] for r in rows))
    out.append(f'HARVEST {sum(r[2] != "(leg)" for r in rows)} segment samples, {sum(r[2] == "(leg)" for r in rows)} leg totals '
               f'over {len(gates)} gate(s); build(s) {", ".join(builds) or "-"}' +
               ('  (several builds: one sprt A arm takes ONE build - --build)' if len(builds) > 1 else ''))
    return rows, out


# ---------------------------------------------------------------- selftest (synthetic legs, no game data read)
def selftest():
    import tempfile
    ok = True
    fails = []

    def check(name, got, want):
        nonlocal ok
        if got != want:
            ok = False
            fails.append(f'  {name}: got {got!r}, want {want!r}')

    cps = [dict(cp='MX.a', ours='obj:100'), dict(cp='MX.b', ours='obj:200'), dict(cp='MX.c', ours='obj:300'),
           dict(cp='MX.end', ours='success')]
    # t0 = 10:00:00; checkpoints at 20, 50, 90, 120 s
    base = ['[10:00:20.000] event objective count=1 title=1 "M" text=2 "x" obj=100 "a"',
            '[10:00:50.000] event objective count=1 title=1 "M" text=2 "x" obj=200 "b"',
            '[10:01:30.000] event objective count=1 title=1 "M" text=2 "x" obj=300 "c"',
            '[10:02:00.000] mission-message type=SUCCESS']
    hist = {'MX.a': (20.0, 20.0), 'MX.b': (50.0, 30.0), 'MX.c': (90.0, 40.0), 'MX.end': (120.0, 30.0)}
    row = dict(outcome='PASS', detail='SUCCESS total 120 s', commit='abc1234', instance='2', date='2026-10-30 12:00',
               full=True)
    t0 = splits.clock(10, 0, 0)
    arb = {'situation_arbiter': '1'}

    def run(extra, r=None, h=hist, aps=arb, strict=False):
        lines = sorted(base + extra, key=lambda l: l[:14])
        with tempfile.TemporaryDirectory() as d:
            p = os.path.join(d, 'FLController.log')
            with open(p, 'w', encoding='latin-1') as f:
                f.write('\n'.join(lines) + '\n')
            return judge_run(cps, r or row, p, t0, h, aps, strict)

    def verdict(res):
        return [('A' if not x[4] else x[4].split(':')[0].split(' at ')[0]) for x in res[1]]

    agree = '[10:00:30.000] event situation arbiter agree pass mode=travel plan=agree ok=1'
    res = run([agree])
    check('all agree', verdict(res), ['A', 'A', 'A', 'A'])
    check('all agree segs', [x[2] for x in res[1]], [20.0, 30.0, 40.0, 30.0])
    check('all agree total', res[2], 120.0)
    # a differ at 70 s: exactly segment MX.c (50 -> 90)
    res = run([agree, '[10:01:10.000] event situation arbiter differ lane mode=travel plan=kind ok=1'])
    check('differ excludes its segment', verdict(res), ['A', 'A', 'differ', 'A'])
    check('differ: no leg total', res[2], None)
    res = run([agree, '[10:01:10.000] event situation arbiter differ lane mode=travel plan=kind ok=1'], strict=True)
    check('strict excludes the rest', verdict(res), ['A', 'A', 'differ', '--strict'])
    # within 1 s of a checkpoint (50 s): both neighbours
    res = run(['[10:00:50.500] event situation arbiter differ lane mode=travel plan=kind ok=1'])
    check('boundary differ', verdict(res), ['A', 'differ', 'differ', 'A'])
    # before the start: the first segment
    res = run(['[09:59:58.000] event situation arbiter differ lane mode=travel plan=kind ok=1'])
    check('differ before t0', verdict(res), ['differ', 'A', 'A', 'A'])
    # a refused side effect
    res = run(['[10:01:40.000] side-effect dock target=5 step=0x1 rule=dock refused why=docked'])
    check('refused', verdict(res), ['A', 'A', 'A', 'refused'])
    # agree with plan= differing: the body's plan executes - still A
    res = run(['[10:00:30.000] event situation arbiter agree arrive mode=travel plan=radius:900!=1000 ok=1'])
    check('plan differ on agree', verdict(res), ['A', 'A', 'A', 'A'])
    # undeclared switches
    res = run([agree], aps={})
    check('undeclared arbiter', res[0].startswith('the log shows situation arbiter'), True)
    res = run(['[10:00:30.000] event situation shadow differ jump old=x new=lane'], aps={})
    check('undeclared shadow', res[0].startswith('the log shows situation shadow'), True)
    res = run(['[10:00:30.000] event situation shadow differ jump old=x new=lane'], aps={'situation_shadow': '1'})
    check('shadow differ is measurement', verdict(res), ['A', 'A', 'A', 'A'])
    # no switch, no lines: the plain default
    check('plain gate', verdict(run([], aps={})), ['A', 'A', 'A', 'A'])
    # a failure at 100 s: segments ended before count, the one it died in not
    fb = [l for l in base if 'SUCCESS' not in l] + ['[10:01:40.000] player-death']
    with tempfile.TemporaryDirectory() as d:
        p = os.path.join(d, 'FLController.log')
        open(p, 'w', encoding='latin-1').write('\n'.join(fb) + '\n')
        frow = dict(row, outcome='FAIL', detail='FAILED (player-death) total 100 s')
        res = judge_run(cps, frow, p, t0, {k: v for k, v in hist.items() if k != 'MX.end'}, {}, False)
    check('failed leg', verdict(res), ['A', 'A', 'A', 'checkpoint not timed (not reached / not seen / its previous not seen)'])
    check('failed leg: no total', res[2], None)
    # run-level exclusions
    check('dirty', run([], r=dict(row, commit='abc1234+dirty'))[0].startswith('build abc1234+dirty not clean'), True)
    check('stop-at', run([], r=dict(row, detail='UNTIL total 50 s'))[0], 'stop-at run')
    check('named instance', run([], r=dict(row, instance='vanilla11c'))[0].startswith('instance'), True)
    check('pre-stamp', run([], r=dict(row, date='2026-10-28 12:00'))[0].startswith('before'), True)
    check('part run', run([], r=dict(row, full=False))[0], 'not a full leg from its start save')
    check('no history', run([], h=None)[0], 'not in the split history yet (splits.py update)')
    # the history disagrees with the log
    res = run([], h=dict(hist, **{'MX.b': (50.0, 33.0)}))
    check('history mismatch', verdict(res), ['A', 'splits.tsv says 33.0 s, the log 30.0 s', 'A', 'A'])
    # --apset parsing
    check('apset ok', parse_apset('situation_shadow 1, situation_arbiter 1')[0], {'situation_shadow': '1', 'situation_arbiter': '1'})
    check('apset other switch', parse_apset('pre_stage 1')[0], None)
    check('apset malformed', parse_apset('situation_arbiter')[0], None)
    check('apset empty', parse_apset('')[0], {})
    # gate_runs: a running folder (only a transcript) is left out, never read
    with tempfile.TemporaryDirectory() as d:
        old = splits.LEGS
        try:
            splits.LEGS = d
            os.mkdir(os.path.join(d, '2026-10-30_1600_M05-gate-zz_i1'))
            open(os.path.join(d, '2026-10-30_1600_M05-gate-zz_i1', 'transcript.txt'), 'w').write('x')
            open(os.path.join(d, 'legs.tsv'), 'w').write('\t'.join(['date', 'label', 'save', 'outcome', 'secs', 'boot',
                                                                     'fps', 'commit', 'instance', 'detail']) + '\n')
            orig = splits.archived_runs
            splits.archived_runs = lambda: []
            runs, other = gate_runs('zz')
            splits.archived_runs = orig
        finally:
            splits.LEGS = old
    check('running folder', other, [('2026-10-30_1600_M05-gate-zz_i1', 'still running (only its transcript)')])
    print('\n'.join(fails))
    print('selftest ' + ('OK' if ok else 'FAILED'))
    return 0 if ok else 1


def main(a):
    if a[:1] == ['selftest']:
        return selftest()
    gates, strict, build, outp, quiet, i = [], False, None, None, False, 0
    while i < len(a):
        x = a[i]
        if x in ('--gate', '--apset', '--build', '--out') and (i + 1 >= len(a) or a[i + 1].startswith('--')):
            print(f'{x} needs a value\n' + __doc__)
            return 2
        if x == '--gate':
            gates.append([a[i + 1], {}])
        elif x == '--apset':
            if not gates:
                print('--apset belongs to the --gate before it')
                return 2
            aps, why = parse_apset(a[i + 1])
            if why:
                print(why)
                return 2
            gates[-1][1] = aps
        elif x == '--build':
            build = a[i + 1]
        elif x == '--out':
            outp = a[i + 1]
        elif x == '--strict':
            strict = True
            i += 1
            continue
        elif x == '--quiet':
            quiet = True
            i += 1
            continue
        else:
            print(f'unknown argument {x!r}\n' + __doc__)
            return 2
        i += 2
    if not gates:
        print(__doc__)
        return 2
    rows, out = harvest([tuple(g) for g in gates], strict, build, quiet)
    print('\n'.join(out))
    if outp:
        with open(outp, 'w', encoding='utf-8', newline='') as f:
            w = csv.writer(f, delimiter='\t', lineterminator='\n')
            w.writerow(HEADER)
            w.writerows(rows)
        print(f'written {outp}: {len(rows)} row(s)')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
