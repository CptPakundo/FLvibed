"""Speedrun splits: the story's checkpoints, the runners' splits, ours, par, fail limits, LiveSplit files.

The checkpoint list is docs\\checkpoints.tsv (hand-curated, one row per checkpoint; the user, 24 Oct 2026: "a set list of
checkpoints throughout the game's story ... right before or right after a notable event ... where player interactivity
is involved"). A leg (docs\\LEGS.md) starts at its accept; each checkpoint ends a segment. Columns:
  ours     how our FLController.log shows it: obj:<id> (the objective first shown after the previous checkpoint),
           land / launch / cutscene / control (the next dock / launch / space->cutscene / HUD back in space after a
           launch or a space scene), kill:<n> (the n-th destroyed target the mission marked), success (Mission Result)
  runner   how the three recordings show it: b:<BANNER TEXT> (the next objective banner, research\\evidence\\speedrun\\
           runs\\<run>\\banners.tsv), ev:<type> (the next event of that type, events.tsv), hud (the space HUD back,
           reference\\events_dense_run<n>.tsv hud_on), end (the leg's end); r1 / r2 / r3
           = seconds from the leg start read by hand (they win over the automatic match; src says where from)
  scene    room-scene seconds the runners watched inside the segment (r1/r2/r3) - HDE's Esc skips them, so they are
           taken out of the runners' times ("scene-free", as docs\\LEGS.md)

Par = the golden run's splits: per leg the runner with the fastest scene-free leg time (the user: "the golden benchmark
would be the fastest speedrun"); a checkpoint that run's footage does not show is placed by the other runs' share of
the segment. Targets (the user): every mission's whole time <= par; never far behind at a checkpoint; a catastrophic
mistake or delay fails the run at once (limits below, from the runners' segments and our own history).

Commands (OCR venv python, -I):
  build              docs\\splits_reference.tsv (runners + par per checkpoint) and the report of what is missing
  history            our splits of every archived leg -> research\\experience\\legs\\splits.tsv
  limits             fail limits + pace data for the live check -> controller\\tools\\missions\\leg_checkpoints.tsv
  replay             the limits replayed on every archived leg: good runs failed (must be none), bad runs caught
  table              docs\\SPLITS.md (generated: par, runners, our PB / gold / sum of best, per checkpoint)
  run <leg dir|log>  one run's splits against par / PB / gold (-t0 <hh:mm:ss> with a bare log)
  compare <A> <B>    two run sets (label regexes) segment by segment: medians, WORSE / better beyond the noise
  lss                LiveSplit splits files docs\\livesplit\\<leg>.lss (comparisons: par, WR run, runners' best
                     segments, our PB; our gold as best segments)
  update             history, limits, table, lss (flleg.ps1 after every leg: PB / gold / limits stay current)
  all                build, history, limits, replay, table, lss
"""
import csv
import datetime
import difflib
import json
import os
import re
import statistics
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
FL = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CPF = os.path.join(FL, 'docs', 'checkpoints.tsv')
REFOUT = os.path.join(FL, 'docs', 'splits_reference.tsv')
LEGREF = os.path.join(FL, 'docs', 'legs_reference.tsv')
SPLITSMD = os.path.join(FL, 'docs', 'SPLITS.md')
LIVE = os.path.join(FL, 'controller', 'tools', 'missions', 'leg_checkpoints.tsv')
LSSDIR = os.path.join(FL, 'docs', 'livesplit')
LEGS = os.path.join(FL, 'research', 'experience', 'legs')
HIST = os.path.join(LEGS, 'splits.tsv')
SCENES = os.path.join(FL, 'docs', 'missions', 'scene_saves.tsv')
RUNS = os.path.join(FL, 'research', 'evidence', 'speedrun', 'runs')
DENSE = os.path.join(FL, 'research', 'evidence', 'speedrun', 'reference')
RUNNERS = ['run1_TM4S13', 'run2_OldOak', 'run3_SydMontague']
RUNNER_NAMES = ['TM4S13 (WR)', 'OldOak', 'SydMontague']
# M01 is one leg in the recordings; our M01a / M01b split at the Pittsburgh dock (the docking animation's end): the runs'
# legs.tsv notes (run 1 "Pittsburgh dock end 657", run 2 "Pittsburgh dock v565", run 3 "M01a = 153.95-562.5")
PITTSBURGH = [657.0, 565.0, 562.5]
LEG_ORDER = ['M01a', 'M01b', 'M02', 'M03', 'M04', 'M05', 'M06', 'M07', 'M08', 'M09', 'M10', 'M11', 'M12', 'M13']


def rtsv(path):
    with open(path, encoding='utf-8', errors='replace') as f:
        return list(csv.DictReader(f, delimiter='\t'))


def wtsv(path, header, rows):
    with open(path, 'w', encoding='utf-8', newline='') as f:
        w = csv.writer(f, delimiter='\t', lineterminator='\n')
        w.writerow(header)
        w.writerows(rows)


def num(s):
    try:
        return float(s)
    except (TypeError, ValueError):
        return None


def fmt(x, nd=1):
    return '' if x is None else (f'{x:.{nd}f}'.rstrip('0').rstrip('.') if nd else f'{x:.0f}')


def checkpoints():
    legs = {}
    for r in rtsv(CPF):
        legs.setdefault(r['leg'], []).append(r)
    return legs


# ---------------------------------------------------------------- the runners
def norm(s):
    return re.sub(r'[^A-Z ]', '', s.upper().replace('’', "'")).replace('  ', ' ').strip()


def bmatch(b, o):
    if len(b) < 8:
        return 0
    B, O = ' ' + b + ' ', ' ' + o + ' '
    if O in B or (len(b) >= 14 and B in O and O.find(B) == 0):
        return 1
    if b.split()[0] != o.split()[0]:
        return 0
    return difflib.SequenceMatcher(None, b, o).ratio()


def runner_window(i, leg):
    rows = {r['leg']: r for r in rtsv(os.path.join(RUNS, RUNNERS[i], 'legs.tsv'))}
    if leg in ('M01a', 'M01b'):
        a, b = float(rows['M01']['start_video_s']), float(rows['M01']['end_video_s'])
        return (a, PITTSBURGH[i]) if leg == 'M01a' else (PITTSBURGH[i], b)
    r = rows[leg]
    return float(r['start_video_s']), float(r['end_video_s'])


def runner_times(leg, cps, maxseg=None):
    """Per run: the cumulative seconds from the leg start at each checkpoint (raw, scenes included) + how found.
    maxseg (second pass): per checkpoint the longest plausible segment - an automatic match beyond it is an OCR gap
    (run 2's M02 lane banner unread: the next "DOCK WITH THE TRADE LANE", 517 s later, matched), not a time."""
    out = []
    for i, run in enumerate(RUNNERS):
        a, b = runner_window(i, leg)
        ban = []
        for row in csv.reader(open(os.path.join(RUNS, run, 'banners.tsv'), encoding='utf-8', errors='replace'), delimiter='\t'):
            f = num(row[0]) if row else None
            if f is not None and a - 2 <= f <= b + 2 and len(row) > 2:
                ban.append((f, norm(row[2])))
        evs = [(num(r['video_s']), r['type'], r['detail']) for r in rtsv(os.path.join(RUNS, run, 'events.tsv'))
               if num(r['video_s']) is not None and a - 2 <= num(r['video_s']) <= b + 2]
        huds = [num(r['video_s']) for r in rtsv(os.path.join(DENSE, f'events_dense_run{i + 1}.tsv'))
                if r['type'] == 'hud_on' and num(r['video_s']) is not None and a - 2 <= num(r['video_s']) <= b + 2]
        prev = a
        used = a   # the last banner a checkpoint took
        ts, how = [], []
        hand = [num(cp['r%d' % (i + 1)]) for cp in cps]
        slack = 0.0   # the bounds of checkpoints this run missed since its last one found
        for k, cp in enumerate(cps):
            man = hand[k]
            spec = cp['runner'].strip()
            # an automatic match never passes the next hand-read checkpoint of this run (OCR gaps made the next
            # banner of the same text, minutes later, match)
            upto = next((a + h for h in hand[k + 1:] if h is not None), b) + 0.5
            if maxseg and spec != 'end':
                if maxseg[k] is None:
                    slack = None
                elif slack is not None:
                    upto = min(upto, prev + slack + maxseg[k])
            auto = None
            if spec == 'end':
                auto = b
            elif spec.startswith('b:'):
                # as on our side, a banner up to LOOKBACK s before the previous checkpoint counts (run 2's M04 lane
                # banner 1 s before the HUD came back), at the previous checkpoint's time
                want = norm(spec[2:])
                for f, t in ban:
                    if max(prev - LOOKBACK, used) + 0.5 < f <= upto and bmatch(t, want) >= 0.8:
                        auto, used = max(f, prev), f
                        break
            elif spec == 'hud':
                auto = next((f for f in huds if prev + 0.5 < f <= upto), None)
            elif spec.startswith('ev:'):
                typ = spec[3:]
                for f, t, d in evs:
                    if prev + 0.5 < f <= upto and t == typ:
                        auto = f
                        break
            v = None
            if man is not None:
                v, h = a + man, 'hand'
                if auto is not None and abs(auto - v) > 3:
                    h = f'hand (auto {auto - a:.0f})'
            elif auto is not None:
                v, h = auto, 'auto'
            else:
                h = 'missing'
            ts.append(None if v is None else v - a)
            how.append(h)
            if v is not None:
                prev, slack = v, 0.0
            elif maxseg and slack is not None and maxseg[k] is not None:
                slack += maxseg[k]
        out.append((ts, how))
    return out


def scenes(cp):
    s = cp['scene'].strip()
    return [float(x) for x in s.split('/')] if s else [0.0, 0.0, 0.0]


def reference(legs):
    """leg -> dict: adjusted runner cumulatives, par cumulative / segments, runner best / worst segments, golden run."""
    ref = {}
    for leg in LEG_ORDER:
        cps = legs[leg]
        rt = runner_times(leg, cps)
        # second pass: bound each automatic match by the other runs' segment (3x their median, at least +90 s)
        maxseg = []
        for k in range(len(cps)):
            ss = []
            for i in range(3):
                ts = rt[i][0]
                a0 = ts[k - 1] if k else 0.0
                if a0 is not None and ts[k] is not None:
                    ss.append(ts[k] - a0)
            maxseg.append(max(3 * statistics.median(ss), statistics.median(ss) + 90) if len(ss) >= 2 else None)
        rt = runner_times(leg, cps, maxseg)
        adj = []
        for i in range(3):
            ts, _ = rt[i]
            acc, row = 0.0, []
            for k, cp in enumerate(cps):
                acc += scenes(cp)[i]
                row.append(None if ts[k] is None else ts[k] - acc)
            # a room scene that began inside the docking animation (run 1 Holstein: the scene from v11272, the dock's
            # end v11279) reaches into the previous segment: pull earlier checkpoints down, the leg total unchanged
            low = None
            for k in range(len(row) - 1, -1, -1):
                if row[k] is None:
                    continue
                if low is not None and row[k] > low:
                    row[k] = low
                low = row[k]
            adj.append(row)
        ends = [a[-1] for a in adj]
        g = min(range(3), key=lambda i: ends[i] if ends[i] is not None else 1e9)
        # par: the golden run; a checkpoint it lacks is placed by the other runs' share of the bracketing segment
        par = list(adj[g])
        for k in range(len(cps)):
            if par[k] is not None:
                continue
            lo = max((j for j in range(k) if par[j] is not None), default=None)
            hi = min((j for j in range(k + 1, len(cps)) if par[j] is not None), default=None)
            p0 = par[lo] if lo is not None else 0.0
            p1 = par[hi]
            fr = []
            for i in range(3):
                if i == g:
                    continue
                a0 = adj[i][lo] if lo is not None else 0.0
                if a0 is None or adj[i][k] is None or adj[i][hi] is None or adj[i][hi] <= a0:
                    continue
                fr.append((adj[i][k] - a0) / (adj[i][hi] - a0))
            par[k] = p0 + (p1 - p0) * (statistics.median(fr) if fr else (k - (lo if lo is not None else -1)) / (hi - (lo if lo is not None else -1)))
        segs = []
        for k in range(len(cps)):
            rs = []
            for i in range(3):
                a0 = adj[i][k - 1] if k else 0.0
                if a0 is not None and adj[i][k] is not None:
                    rs.append(adj[i][k] - a0)
                else:
                    rs.append(None)
            segs.append(rs)
        ref[leg] = dict(cps=cps, raw=[r[0] for r in rt], how=[r[1] for r in rt], adj=adj, golden=g, par=par,
                        segs=segs, parseg=[par[k] - (par[k - 1] if k else 0.0) for k in range(len(cps))])
    return ref


def cmd_build(legs):
    ref = reference(legs)
    rows, missing = [], []
    for leg in LEG_ORDER:
        R = ref[leg]
        for k, cp in enumerate(R['cps']):
            s = R['segs'][k]
            known = [x for x in s if x is not None]
            rows.append([leg, cp['cp'], k + 1] + [fmt(R['adj'][i][k]) for i in range(3)] + [fmt(x) for x in s] +
                        [fmt(R['par'][k]), fmt(R['parseg'][k]), RUNNERS[R['golden']].split('_')[0],
                         fmt(min(known)) if known else '', fmt(max(known)) if known else '',
                         ' / '.join(R['how'][i][k] for i in range(3)), cp['segment']])
            for i in range(3):
                if R['adj'][i][k] is None:
                    missing.append(f'{cp["cp"]} r{i + 1}')
    wtsv(REFOUT, ['leg', 'cp', 'n', 'r1', 'r2', 'r3', 'r1_seg', 'r2_seg', 'r3_seg', 'par', 'par_seg', 'golden',
                  'runner_best_seg', 'runner_worst_seg', 'how', 'segment'], rows)
    print(f'{REFOUT}: {len(rows)} checkpoints; runner times missing ({len(missing)}): {", ".join(missing)}')
    for leg in LEG_ORDER:
        R = ref[leg]
        print(f'{leg:5} par {R["par"][-1]:7.1f} (golden {RUNNERS[R["golden"]]})  runs ' +
              ' / '.join(fmt(R['adj'][i][-1]) for i in range(3)))
    # sanity: every run's raw cumulative must rise
    for leg in LEG_ORDER:
        R = ref[leg]
        for i in range(3):
            last = -1
            for k, t in enumerate(R['raw'][i]):
                if t is None:
                    continue
                if t < last:
                    print(f'NOT RISING: {leg} r{i + 1} at {R["cps"][k]["cp"]} {t:.1f} < {last:.1f}')
                last = t
    return ref


# ---------------------------------------------------------------- our runs
TS = re.compile(r'^\[(\d+):(\d+):(\d+)\.(\d+)\]')


def clock(h, m, s, ms=0):
    return int(h) * 3600 + int(m) * 60 + int(s) + int(ms) / 1000.0


def log_events(log):
    """(t, kind, value) in log order; t = clock seconds."""
    ev, marks = [], set()
    for line in open(log, encoding='latin-1'):
        m = TS.match(line)
        if not m:
            continue
        t = clock(*m.groups())
        o = re.search(r'event objective .* obj=(\d+) ', line)
        if o:
            ev.append((t, 'obj', o.group(1)))
            continue
        e = re.search(r'event mode (\S+) -> (\S+)', line)
        if e:
            if e.group(2) == 'docked' and e.group(1) != 'menu':
                ev.append((t, 'land', ''))
            elif e.group(1) == 'docked':
                ev.append((t, 'launch', ''))
            elif e.group(1) == 'space' and e.group(2) == 'cutscene':
                ev.append((t, 'cutscene', ''))
            if e.group(2) in ('space', 'space-ui') and e.group(1) in ('cutscene', 'docked'):
                ev.append((t, 'control', ''))   # the HUD back in space (after the launch / a space scene)
            continue
        mk = re.search(r'event mark id=(\d+) state=1', line)
        if mk:
            marks.add(mk.group(1))
            continue
        kd = re.search(r'target-destroyed id=(\d+)', line)
        if kd and kd.group(1) in marks and int(kd.group(1)) > 100000:
            ev.append((t, 'kill', ''))
            continue
        if 'mission-message type=SUCCESS' in line:
            ev.append((t, 'success', ''))
        elif 'mission-message type=FAILURE' in line or 'player-death' in line:
            ev.append((t, 'fail', ''))
    return ev


LOOKBACK = 5.0   # an objective set up to 5 s before the previous checkpoint still counts (the race's "Beat Hovis"
                 # banner 3 s before the HUD returns; M07's agents with it) - its time is then the previous one's


def our_splits(cps, ev, t0, full=True, startcp=None):
    """Cumulative seconds from t0 per checkpoint (None = not reached / not seen). Matching as documented above; a
    checkpoint not seen does not block the later ones. A part run (from a checkpoint save) starts at the checkpoint
    whose objective the load shows first - never at an earlier one (a later launch is no M10.control). A segment
    practice run (startcp: from a save Leg.ps1 -SaveAt made at that checkpoint) has it at 0.0 (the leg start) and
    counts kills after the save's."""
    out = []
    ev = [((t + 86400.0) if t < t0 - 43200 else t, k, v) for t, k, v in ev]   # a leg across midnight
    pos = 0
    while pos < len(ev) and ev[pos][0] < t0 - 5:
        pos += 1
    kpos = pos
    prev_t, prev_hit = None, -1
    first, koff = 0, 0
    if startcp:
        first = next((k for k, cp in enumerate(cps) if cp['cp'] == startcp), len(cps)) + 1
        # kills before the save = the n of the last kill:<n> at or before it (not the rows: kill:3, kill:6 legs)
        koff = max([int(cp['ours'].strip()[5:]) for cp in cps[:first] if cp['ours'].startswith('kill:')] or [0])
        prev_t = t0
    elif not full:
        o = next((e for e in ev[pos:] if e[1] == 'obj' and e[2] not in ('0', '21660')), None)
        first = next((k for k, cp in enumerate(cps) if o and cp['ours'] == 'obj:' + o[2]), len(cps))
    for n, cp in enumerate(cps):
        if n < first:
            out.append(0.0 if startcp and n == first - 1 else None)
            continue
        spec = cp['ours'].strip()
        kind, _, val = spec.partition(':')
        hit = None
        if kind == 'kill':
            n = int(val) - koff
            c = 0
            for j in range(kpos, len(ev)):
                if ev[j][1] == 'kill':
                    c += 1
                    if c == n:
                        hit = j
                        break
        else:
            start = pos
            if prev_t is not None:
                while start > 0 and ev[start - 1][0] >= prev_t - LOOKBACK:
                    start -= 1
            for j in range(start, len(ev)):
                if j == prev_hit:
                    continue
                if ev[j][1] == 'fail' and j >= pos:
                    break
                if ev[j][1] == kind and (kind != 'obj' or ev[j][2] == val):
                    hit = j
                    break
        if hit is None:
            out.append(None)
            continue
        t = ev[hit][0] if prev_t is None else max(ev[hit][0], prev_t)
        out.append(t - t0)
        prev_t, prev_hit = t, hit
        pos = max(pos, hit + 1)
    return out


def leg_of(label):
    m = re.match(r'(M\d\d[ab]?)', label or '')
    return m.group(1) if m else None


def t0_of(folder):
    tr = os.path.join(folder, 'transcript.txt')
    if not os.path.isfile(tr):
        return None
    m = re.search(r'^(\d\d):(\d\d):(\d\d) \+\s+0 ', open(tr, encoding='utf-8', errors='replace').read(), re.M)
    return clock(*m.groups()) if m else None


def archived_runs():
    """Every legs.tsv row with its evidence folder (the newest folder of that label and instance started before)."""
    dirs = sorted(x for x in os.listdir(LEGS) if os.path.isdir(os.path.join(LEGS, x)))
    starts = {r['leg']: r['start_save'] for r in rtsv(LEGREF)}
    # segment practice saves (Leg.ps1 -SaveAt): the save's file name -> the checkpoint it was made at
    segsave = {r['name']: r['step'] for r in rtsv(SCENES) if r.get('source') == 'leg-checkpoint'} if os.path.isfile(SCENES) else {}
    out = []
    for r in rtsv(os.path.join(LEGS, 'legs.tsv')):
        key = r['date'][:10] + '_' + r['date'][11:13] + r['date'][14:16]
        cand = [x for x in dirs if x.endswith('_' + r['label'] + '_i' + r['instance']) and x[:15] <= key]
        leg = leg_of(r['label'])
        if not cand or not leg:
            continue
        # (back-to-back legs on one instance: the next leg's folder starts in the minute this one ends - by the start
        # time alone this row took that folder, two rows on one folder (24 Oct 2026, flset.ps1); the folder whose start
        # minute + the leg's seconds and boot lands nearest the row's end minute wins)
        def mins(s):  # 'YYYY-MM-DD_HHMM' -> minutes (one day's span suffices here; the date part orders cand)
            return int(s[11:13]) * 60 + int(s[13:15]) + (0 if s[:10] == key[:10] else -1440)
        try:
            dur = (float(r['secs'] or 0) + float(r.get('boot') or 0)) / 60.0
            pick = min(cand[-3:], key=lambda x: abs(mins(x[:15]) + dur - mins(key)))
        except ValueError:
            pick = cand[-1]
        d = os.path.join(LEGS, pick)
        if not os.path.isfile(os.path.join(d, 'FLController.log')):
            continue
        out.append(dict(r, dir=d, leg=leg, full=starts.get(leg) == r['save'], startcp=segsave.get(r['save'])))
    return out


def cmd_history(legs):
    rows = []
    for a in archived_runs():
        t0 = t0_of(a['dir'])
        if t0 is None or a['leg'] not in legs:
            continue
        sp = our_splits(legs[a['leg']], log_events(os.path.join(a['dir'], 'FLController.log')), t0, a['full'], a['startcp'])
        # a part run (from a checkpoint save) sees its first checkpoint when the load re-sends the objective, not
        # when it happened: segments start at its second checkpoint seen. A segment practice run ('seg') times its
        # first segment from the load (a start at rest: compared between sets, never a gold - ours_stats)
        first = next((k for k, t in enumerate(sp) if t is not None), None)
        for k, cp in enumerate(legs[a['leg']]):
            prev = sp[k - 1] if k else (0.0 if a['full'] else None)
            if not a['full'] and not a['startcp'] and k - 1 == first:
                prev = None
            seg = sp[k] - prev if sp[k] is not None and prev is not None else None
            rows.append([a['date'], a['label'], a['save'], 'full' if a['full'] else ('seg' if a['startcp'] else 'part'), a['outcome'], a['secs'],
                         a['commit'], os.path.basename(a['dir']), cp['cp'], k + 1, fmt(sp[k]), fmt(seg)])
    wtsv(HIST, ['date', 'label', 'save', 'start', 'outcome', 'secs', 'commit', 'dir', 'cp', 'n', 'at', 'seg'], rows)
    print(f'{HIST}: {len(rows)} rows from {len(set(r[7] for r in rows))} runs')


def history_by_run():
    runs = {}
    for r in rtsv(HIST):
        runs.setdefault(r['dir'], dict(info=r, cp={}))['cp'][r['cp']] = (num(r['at']), num(r['seg']))
    return runs


def ours_stats(legs):
    """Per leg: PB (the fastest full PASS run's cumulative splits), gold (best segment of any run, PASS or not -
    LiveSplit's best segments), the segment times of good runs (full PASS runs within 10 % of the PB)."""
    runs = history_by_run()
    st = {}
    for leg in LEG_ORDER:
        cps = legs[leg]
        full = [v for v in runs.values() if leg_of(v['info']['label']) == leg and v['info']['start'] == 'full'
                and v['info']['outcome'] == 'PASS' and v['cp'].get(cps[-1]['cp'], (None,))[0] is not None]
        pb = min(full, key=lambda v: v['cp'][cps[-1]['cp']][0]) if full else None
        gold, goldrun, segs_good = [], [], []
        for k, cp in enumerate(cps):
            best, who = None, None
            for name, v in runs.items():
                if leg_of(v['info']['label']) != leg or v['info']['start'] == 'seg':
                    continue
                s = v['cp'].get(cp['cp'], (None, None))[1]
                if s is not None and s >= 0 and (best is None or s < best):
                    best, who = s, name
            gold.append(best)
            goldrun.append(who)
        good = [v for v in full if pb and v['cp'][cps[-1]['cp']][0] <= 1.10 * pb['cp'][cps[-1]['cp']][0]]
        for k, cp in enumerate(cps):
            segs_good.append([v['cp'][cp['cp']][1] for v in good if v['cp'].get(cp['cp'], (None, None))[1] is not None])
        # spread: the last RECENT full passes (the current behaviour's run-to-run noise) -> how many runs judge a change
        recent = sorted(full, key=lambda v: v['info']['dir'])[-RECENT:]
        tots = [v['cp'][cps[-1]['cp']][0] for v in recent]
        sd = statistics.stdev(tots) if len(tots) >= 3 else None
        segsd = []
        for cp in cps:
            xs = [v['cp'][cp['cp']][1] for v in recent if v['cp'].get(cp['cp'], (None, None))[1] is not None]
            segsd.append(statistics.stdev(xs) if len(xs) >= 3 else None)
        st[leg] = dict(pb=[pb['cp'].get(cp['cp'], (None,))[0] for cp in cps] if pb else [None] * len(cps),
                       pbrun=pb['info']['dir'] if pb else None, gold=gold, goldrun=goldrun, good=segs_good,
                       nfull=len(full), ngood=len(good), sd=sd, segsd=segsd, nrecent=len(tots),
                       runs=runs_needed(sd, pb['cp'][cps[-1]['cp']][0] if pb else None))
    return st


RECENT = 10   # the spread of a leg / segment = the standard deviation of its last 10 full passes


def runs_needed(sd, pb):
    """Runs per side to judge a change (the old fixed "3 runs" against M12's 369-524 s spread, handoff 24 Oct): enough
    that a real gain of max(5 s, 2 % of the PB) stands out of the noise: n = (2 sd / gain)^2, 3..8 (unknown spread: 3)."""
    if sd is None or pb is None:
        return 3
    gain = max(5.0, 0.02 * pb)
    return int(min(8, max(3, -(-(2 * sd / gain) ** 2 // 1))))


# ---------------------------------------------------------------- limits (fail states) and the replay
# A segment over its limit (or a checkpoint missed by its cumulative deadline) is a catastrophic mistake or delay: the
# run cannot be a good one any more and stops (the user, 24 Oct 2026). Both limits are the larger of a runner bound and
# an own-history bound, so they catch what is far outside both the runners' worst and our own best, and tighten by
# themselves as our gold / PB improve. The constants are chosen by the replay (cmd_replay): no good archived run may fail.
SEG_RUNNER_X, SEG_RUNNER_ADD = 2.0, 45.0      # runner bound: 2x the runners' worst segment, at least +45 s
SEG_OURS_X, SEG_OURS_ADD = 1.5, 20.0          # own bound: 1.5x our gold segment + 20 s
CUM_PB_X, CUM_PB_ADD = 1.15, 30.0             # cumulative: 1.15x our PB's split + 30 s ...
CUM_PAR_X = 1.6                               # ... or 1.6x par where we have no PB split
BEHIND_ADD, BEHIND_X = 10.0, 0.05             # pace: "behind" = over par by more than max(10 s, 5 % of par)


def limits(legs, ref, st):
    out = {}
    for leg in LEG_ORDER:
        R, S = ref[leg], st[leg]
        rows = []
        for k, cp in enumerate(R['cps']):
            known = [x for x in R['segs'][k] if x is not None]
            rw = max(known) if known else R['parseg'][k]
            seg_lim, by = max(SEG_RUNNER_X * rw, rw + SEG_RUNNER_ADD), 'runners'
            if S['gold'][k] is not None and SEG_OURS_X * S['gold'][k] + SEG_OURS_ADD > seg_lim:
                seg_lim, by = SEG_OURS_X * S['gold'][k] + SEG_OURS_ADD, 'gold'
            pb = S['pb'][k]
            cum_lim = CUM_PB_X * pb + CUM_PB_ADD if pb is not None else CUM_PAR_X * R['par'][k] + SEG_RUNNER_ADD
            rows.append(dict(cp=cp['cp'], ours=cp['ours'], par=R['par'][k], parseg=R['parseg'][k], gold=S['gold'][k],
                             pb=pb, seg_lim=seg_lim, cum_lim=cum_lim, segment=cp['segment'], by=by,
                             goldrun=os.path.basename(S['goldrun'][k] or '')))
        out[leg] = rows
    return out


def cmd_limits(legs, ref, st):
    L = limits(legs, ref, st)
    rows = []
    for leg in LEG_ORDER:
        for k, r in enumerate(L[leg]):
            rows.append([leg, r['cp'], k + 1, r['ours'], fmt(r['par']), fmt(r['parseg']), fmt(r['pb']), fmt(r['gold']),
                         fmt(r['seg_lim'], 0), fmt(r['cum_lim'], 0), r['segment']])
    wtsv(LIVE, ['leg', 'cp', 'n', 'ours', 'par', 'par_seg', 'pb', 'gold', 'seg_limit', 'cum_limit', 'segment'], rows)
    print(f'{LIVE}: {len(rows)} checkpoints')
    return L


def judge(leg, L, sp, t_end, full):
    """First limit broken: (time it broke, cp, why) or None. sp = cumulative splits; t_end = the run's last second.
    The same rules as Leg.ps1's live check: the next checkpoint is due by the last one's time + its segment limit (and
    by its cumulative limit in a full leg). A kill:<n> checkpoint is soft - our log names only the kills of the
    autopilot's own target (M12: a generator destroyed by a missile in flight after the target moved on is not logged) -
    so when it is due unseen, its limit carries over to the next checkpoint; every other checkpoint is hard."""
    prev_t = 0.0 if full else None
    lim = 0.0
    for k, r in enumerate(L[leg]):
        t = sp[k]
        lim += r['seg_lim']
        soft = r['ours'].startswith('kill:')
        broken = []   # (due, why): the earlier of the two limits is the one a live check meets first
        if prev_t is not None:
            due = prev_t + lim
            if soft and t is not None and t > due:
                t = None   # live, it was given up (carried over) at its due time - a later kill no longer counts
            if (t is not None and t > due) or (t is None and not soft and t_end > due):
                broken.append((due, f'segment over {lim:.0f} s'))
        if full and not soft:
            if (t is None and t_end > r['cum_lim']) or (t is not None and t > r['cum_lim']):
                broken.append((r['cum_lim'], f'not at {r["cp"]} by {r["cum_lim"]:.0f} s'))
        if broken:
            due, why = min(broken)
            return due, r['cp'], why
        if t is not None:
            prev_t, lim = t, 0.0
        elif not soft and prev_t is not None:
            lim = 0.0   # a hard checkpoint never seen in a run that went on (a part run's start): restart from the next
            prev_t = None
    return None


def cmd_replay(legs, ref, st, quiet=False):
    L = limits(legs, ref, st)
    runs = history_by_run()
    bad_good, caught, saved, lines = [], 0, 0.0, []
    for name, v in sorted(runs.items()):
        leg = leg_of(v['info']['label'])
        if leg not in L:
            continue
        full = v['info']['start'] == 'full'
        sp = [v['cp'].get(cp['cp'], (None,))[0] for cp in legs[leg]]
        end = num(v['info']['secs']) or 0
        if full:
            j = judge(leg, L, sp, end, full)
        else:
            # a part run: only the segments it timed (both ends seen, not from its load-time first checkpoint)
            j = None
            for k, cp in enumerate(legs[leg]):
                s = v['cp'].get(cp['cp'], (None, None))[1]
                if s is not None and s > L[leg][k]['seg_lim']:
                    j = (sp[k] - s + L[leg][k]['seg_lim'], cp['cp'], f'segment over {L[leg][k]["seg_lim"]:.0f} s')
                    break
        pbend = st[leg]['pb'][-1]
        good = v['info']['outcome'] == 'PASS' and full and pbend is not None and end <= 1.10 * pbend
        note = ''
        if j and good:
            # the own-history bound tightens as our gold improves: a good run older than the gold that now bounds the
            # segment is superseded, not wrongly failed
            r = next(x for x in L[leg] if x['cp'] == j[1])
            if r['by'] == 'gold' and name < r['goldrun']:
                note = f'  (good run, superseded by the gold of {r["goldrun"]})'
            else:
                bad_good.append(f'{name}: {j[2]} at {j[1]} ({end:.0f} s run)')
        if j and v['info']['outcome'] != 'PASS':
            caught += 1
            saved += max(0.0, end - j[0])
        if j:
            lines.append(f'{name:45} {v["info"]["outcome"]:5} {end:6.0f} s  stop at {j[0]:6.0f} s  {j[1]}: {j[2]}{note}')
    if not quiet:
        for l in lines:
            print(l)
    print(f'replay: {len(lines)} runs stopped; failed runs stopped early: {caught} (saved {saved:.0f} s); '
          f'GOOD RUNS FAILED: {len(bad_good)}')
    for b in bad_good:
        print('  ' + b)
    return bad_good


# ---------------------------------------------------------------- reports
def cmd_table(legs, ref, st):
    L = limits(legs, ref, st)
    out = ['# Speedrun splits: checkpoints, par, our bests', '',
           '**Generated by `tools\\splits.py table` - do not edit.** The checkpoint list is `docs\\checkpoints.tsv`; the',
           "method is `docs\\TESTING.md` \"Speedrun splits\". Seconds from the leg's start (its accept), runners' room scenes",
           'removed (HDE skips them). **Par** = the golden run (per leg the fastest of the three recordings, scene-free);',
           '**PB** = our fastest full PASS leg; **gold** = our best time for that segment in any run; **sum of best** = the',
           'sum of our golds; **limit** = a segment longer than this fails the run at once (catastrophic mistake or delay);',
           f'**spread** = the standard deviation of the last {RECENT} full passes (leg) or of their segment; **runs** = how many',
           'runs per side judge a change to that leg (a 2 % / 5 s gain out of the noise).',
           f'Generated {datetime.datetime.now():%Y-%m-%d %H:%M}.', '']
    out += ['| Leg | Par | Golden run | Our PB | PB - par | Sum of our best | Gold - par | Full passes | Spread | Runs |',
            '|---|---|---|---|---|---|---|---|---|---|']
    tot = [0.0, 0.0, 0.0]
    for leg in LEG_ORDER:
        R, S = ref[leg], st[leg]
        sob = sum(g for g in S['gold'] if g is not None) if all(g is not None for g in S['gold']) else None
        pb = S['pb'][-1]
        tot[0] += R['par'][-1]
        tot[1] += pb or 0
        tot[2] += sob or 0
        out.append(f'| {leg} | {R["par"][-1]:.0f} | {RUNNER_NAMES[R["golden"]]} | {fmt(pb, 0) or "-"} | '
                   f'{"" if pb is None else f"{pb - R["par"][-1]:+.0f}"} | {fmt(sob, 0) or "-"} | '
                   f'{"" if sob is None else f"{sob - R["par"][-1]:+.0f}"} | {S["nfull"]} | '
                   f'{fmt(S["sd"], 0) or "-"} (n {S["nrecent"]}) | {S["runs"]} |')
    out.append(f'| **All** | **{tot[0]:.0f}** | | **{tot[1]:.0f}** | **{tot[1] - tot[0]:+.0f}** | **{tot[2]:.0f}** | '
               f'**{tot[2] - tot[0]:+.0f}** | | | |')
    out.append('')
    for leg in LEG_ORDER:
        R, S = ref[leg], st[leg]
        out += [f'## {leg}', '',
                f'Par {R["par"][-1]:.0f} s ({RUNNER_NAMES[R["golden"]]}); our PB {fmt(S["pb"][-1], 0) or "-"} s '
                f'({os.path.basename(S["pbrun"]) if S["pbrun"] else "no full pass"}).', '',
                '| # | Checkpoint | Segment (what happens) | Par seg | Runners seg (1/2/3) | Gold | Gold - par | Spread | Limit | Par at | PB at | PB - par |',
                '|---|---|---|---|---|---|---|---|---|---|---|---|']
        for k, cp in enumerate(R['cps']):
            g, pb = S['gold'][k], S['pb'][k]
            out.append(f'| {k + 1} | {cp["cp"]} | {cp["segment"]} | {R["parseg"][k]:.0f} | ' +
                       ' / '.join(fmt(x, 0) or '-' for x in R['segs'][k]) +
                       f' | {fmt(g, 0) or "-"} | {"" if g is None else f"{g - R["parseg"][k]:+.0f}"} | '
                       f'{fmt(S["segsd"][k], 0) or "-"} | '
                       f'{L[leg][k]["seg_lim"]:.0f} | {R["par"][k]:.0f} | {fmt(pb, 0) or "-"} | '
                       f'{"" if pb is None else f"{pb - R["par"][k]:+.0f}"} |')
        out.append('')
    open(SPLITSMD, 'w', encoding='utf-8').write('\n'.join(out) + '\n')
    print(f'{SPLITSMD} written')


def run_report(legs, ref, st, path, t0=None):
    d = path if os.path.isdir(path) else os.path.dirname(path)
    log = os.path.join(d, 'FLController.log') if os.path.isdir(path) else path
    label = os.path.basename(d)
    leg = leg_of(re.sub(r'^\d{4}-\d\d-\d\d_\d{4}_', '', label))
    if t0 is None:
        t0 = t0_of(d)
    if leg is None or t0 is None:
        print('need a leg folder (label M..) with transcript, or a log and -t0')
        return
    L = limits(legs, ref, st)
    a = next((a for a in archived_runs() if os.path.basename(a['dir']) == label), None)
    full, startcp = (a['full'], a['startcp']) if a else (True, None)
    sp = our_splits(legs[leg], log_events(log), t0, full, startcp)
    print(f'{label}: {leg} ({"full leg" if full else "part run"}), par {ref[leg]["par"][-1]:.0f} s')
    print(f'{"checkpoint":18} {"at":>7} {"seg":>6} {"par":>6} {"+-par":>7} {"gold":>6} {"+-gold":>7} {"PB at":>7} {"+-PB":>7}  pace')
    prev = 0.0 if full else None
    for k, r in enumerate(L[leg]):
        t = sp[k]
        if startcp and r['cp'] == startcp:
            prev = 0.0
            continue
        if t is None:
            print(f'{r["cp"]:18} {"-":>7}')
            prev = None
            continue
        seg = t - prev if prev is not None else None
        pace = 'AHEAD' if t <= r['par'] else ('ok' if t - r['par'] <= max(BEHIND_ADD, BEHIND_X * r['par']) else 'BEHIND')
        if not full:
            pace = ''   # a part run's clock starts mid-leg: only its segments compare
        flag = ''
        if seg is not None and r['gold'] is not None and seg < r['gold']:
            flag = ' GOLD'
        if seg is not None and seg > r['seg_lim']:
            flag += ' OVER LIMIT'
        print(f'{r["cp"]:18} {t:7.1f} {fmt(seg):>6} {r["parseg"]:6.1f} {t - r["par"]:+7.1f} {fmt(r["gold"]):>6} '
              f'{"" if seg is None or r["gold"] is None else f"{seg - r["gold"]:+7.1f}":>7} {fmt(r["pb"]):>7} '
              f'{"" if r["pb"] is None else f"{t - r["pb"]:+7.1f}":>7}  {pace}{flag}')
        prev = t
    if full and sp[-1] is not None:
        par, pb = ref[leg]['par'][-1], st[leg]['pb'][-1]
        print(f'LEG {leg} {sp[-1]:.1f} s: par {par:.0f} ({sp[-1] - par:+.1f}, ' +
              ('MEETS PAR' if sp[-1] <= par else 'behind par') +
              (f'), PB {pb:.0f} ({sp[-1] - pb:+.1f})' if pb is not None else ')'))


def cmd_compare(legs, ref, st, pa, pb_):
    """Two sets of runs (label regexes, e.g. 'M12-gate-s9y' 'M12-gate-s9z', or two gate labels across legs), checkpoint
    by checkpoint: the median segment of each set, the difference, and WORSE / BETTER when it exceeds the segment's
    noise (max(2 s, its spread, 5 %)) - the verdict of a change set and the gate's regression check (TESTING.md)."""
    runs = history_by_run()
    sets = []
    for p in (pa, pb_):
        rx = re.compile(p)
        sets.append([v for v in runs.values() if rx.search(v['info']['label'])])
    for leg in LEG_ORDER:
        A = [v for v in sets[0] if leg_of(v['info']['label']) == leg]
        B = [v for v in sets[1] if leg_of(v['info']['label']) == leg]
        if not A or not B:
            continue
        S = st[leg]
        print(f'== {leg}: A {len(A)} runs ({", ".join(v["info"]["outcome"] + " " + v["info"]["secs"] for v in A)}) | '
              f'B {len(B)} runs ({", ".join(v["info"]["outcome"] + " " + v["info"]["secs"] for v in B)})')
        print(f'{"checkpoint":18} {"A med":>7} {"B med":>7} {"B-A":>7} {"noise":>6} {"par":>6}  verdict')
        worse = better = 0
        for k, cp in enumerate(legs[leg]):
            xa = [v['cp'][cp['cp']][1] for v in A if v['cp'].get(cp['cp'], (None, None))[1] is not None]
            xb = [v['cp'][cp['cp']][1] for v in B if v['cp'].get(cp['cp'], (None, None))[1] is not None]
            if not xa or not xb:
                print(f'{cp["cp"]:18} {fmt(statistics.median(xa)) if xa else "-":>7} {fmt(statistics.median(xb)) if xb else "-":>7}')
                continue
            ma, mb = statistics.median(xa), statistics.median(xb)
            noise = max(2.0, S['segsd'][k] or 0.0, 0.05 * ma)
            v = 'WORSE' if mb - ma > noise else ('better' if ma - mb > noise else '')
            worse += v == 'WORSE'
            better += v == 'better'
            print(f'{cp["cp"]:18} {ma:7.1f} {mb:7.1f} {mb - ma:+7.1f} {noise:6.1f} {ref[leg]["parseg"][k]:6.1f}  {v}')
        ta = [v['cp'][legs[leg][-1]['cp']][0] for v in A if v['cp'].get(legs[leg][-1]['cp'], (None,))[0] is not None]
        tb = [v['cp'][legs[leg][-1]['cp']][0] for v in B if v['cp'].get(legs[leg][-1]['cp'], (None,))[0] is not None]
        tot = f'leg median A {statistics.median(ta):.1f} B {statistics.median(tb):.1f}' if ta and tb else 'leg: no full finishes on one side'
        print(f'{tot}; segments worse {worse}, better {better}')


def lss_time(s):
    if s is None:
        return None
    h, r = divmod(s, 3600)
    m, sec = divmod(r, 60)
    return f'{int(h):02d}:{int(m):02d}:{sec:010.7f}'


def cmd_lss(legs, ref, st):
    os.makedirs(LSSDIR, exist_ok=True)
    from xml.sax.saxutils import escape
    for leg in LEG_ORDER:
        R, S = ref[leg], st[leg]
        comps = {'Personal Best': S['pb'], 'Par (golden run)': R['par'],
                 'WR run (TM4S13)': R['adj'][0], 'OldOak': R['adj'][1], 'SydMontague': R['adj'][2]}
        rb, acc = [], 0.0
        for k in range(len(R['cps'])):
            known = [x for x in R['segs'][k] if x is not None]
            acc += min(known) if known else R['parseg'][k]
            rb.append(acc)
        comps['Runners best segments'] = rb
        x = ['<?xml version="1.0" encoding="UTF-8"?>', '<Run version="1.7.0">', '  <GameIcon />',
             '  <GameName>Freelancer: HD Edition (FLvibed autopilot)</GameName>',
             f'  <CategoryName>{leg} (speedrun leg, scene-free)</CategoryName>', '  <LayoutPath />',
             '  <Metadata><Run id="" /><Platform usesEmulator="False">PC</Platform><Region /><Variables /></Metadata>',
             '  <Offset>00:00:00</Offset>', '  <AttemptCount>0</AttemptCount>', '  <AttemptHistory />', '  <Segments>']
        for k, cp in enumerate(R['cps']):
            x.append('    <Segment>')
            x.append(f'      <Name>{escape(cp["cp"].split(".", 1)[1])}</Name>')
            x.append('      <Icon />')
            x.append('      <SplitTimes>')
            for name, vals in comps.items():
                t = lss_time(vals[k])
                x.append(f'        <SplitTime name="{escape(name)}">' + (f'<RealTime>{t}</RealTime>' if t else '') + '</SplitTime>')
            x.append('      </SplitTimes>')
            g = lss_time(S['gold'][k])
            x.append('      <BestSegmentTime>' + (f'<RealTime>{g}</RealTime>' if g else '') + '</BestSegmentTime>')
            x.append('      <SegmentHistory />')
            x.append('    </Segment>')
        x += ['  </Segments>', '  <AutoSplitterSettings />', '</Run>']
        open(os.path.join(LSSDIR, f'{leg}.lss'), 'w', encoding='utf-8').write('\n'.join(x) + '\n')
    print(f'{LSSDIR}: {len(LEG_ORDER)} splits files')


def cmd_progress(legs, ref, args):
    """The progress review (docs\\TESTING.md "AFK sessions" 5; the user, 24 Oct 2026: continuous testing and changes must
    not loop without meaningful improvement): per leg, every set (runs sharing a label, full legs from the leg's start
    save, at least 3 runs) in time order - runs, PASS rate, the PASS median, its difference to the best set before it,
    and par. Flags: NEW BEST (better by max(2 s, 1 %)), UNRELIABLE (PASS rate under 5 / 6), and per leg NO PROGRESS when
    the last 3 sets brought no new best - the signal to change the target or the approach, not to run another set.
    Usage: progress [<leg>] [--since 'YYYY-MM-DD HH:MM']"""
    want = next((x for x in args[1:] if re.match(r'^M\d\d[ab]?$', x)), None)
    since = args[args.index('--since') + 1] if '--since' in args else ''
    starts = {r['leg']: r['start_save'] for r in rtsv(LEGREF)}
    sets = {}
    for r in rtsv(os.path.join(LEGS, 'legs.tsv')):
        leg = leg_of(r['label'])
        if not leg or (want and leg != want) or r['save'] != starts.get(leg) or r['date'] < since:
            continue
        s = sets.setdefault((leg, r['label']), dict(first=r['date'], runs=[]))
        s['runs'].append((r['outcome'], num(r['secs'])))
    by_leg = {}
    for (leg, label), s in sets.items():
        if len(s['runs']) >= 3:
            by_leg.setdefault(leg, []).append((s['first'], label, s['runs']))
    for leg in LEG_ORDER:
        if leg not in by_leg:
            continue
        par = ref[leg]['par'][-1] if leg in ref and ref[leg].get('par') else None
        print(f'== {leg}' + (f'  (par {par:.0f} s)' if par else ''))
        best, since_best = None, 0
        for first, label, runs in sorted(by_leg[leg]):
            ok = sorted(x for o, x in runs if o == 'PASS' and x)
            med = statistics.median(ok) if ok else None
            rate = len(ok) / len(runs)
            flags = []
            if med is not None and (best is None or med < best - max(2.0, best * 0.01)):
                flags.append('NEW BEST' if best is not None else 'first')
                best, since_best = med, 0
            else:
                since_best += 1
            if rate < 5 / 6:
                flags.append('UNRELIABLE')
            d = f'{med - best:+6.1f}' if med is not None and best is not None else '      '
            print(f'  {first}  {label:32} {len(runs)} runs  PASS {len(ok)}/{len(runs)}  '
                  f'median {med if med is not None else float("nan"):6.1f}  vs best {d}  {" ".join(flags)}')
        verdict = 'NO PROGRESS in the last %d sets - change the target or the approach' % since_best if since_best >= 3 else \
            '%d set(s) since the last new best' % since_best
        print(f'  best set median {best:.1f} s{f" (par {par:.0f}, {best - par:+.1f})" if par else ""}; {verdict}')


def main():
    a = sys.argv[1:]
    if not a:
        print(__doc__)
        return
    legs = checkpoints()
    c = a[0]
    writes = c in ('build', 'history', 'limits', 'table', 'lss', 'all', 'update')
    lock = os.path.join(LEGS, 'splits.lock')
    if writes:
        # several legs end at once (one flleg per instance): one writer at a time
        import time
        for _ in range(120):
            try:
                os.close(os.open(lock, os.O_CREAT | os.O_EXCL))
                break
            except FileExistsError:
                if time.time() - os.path.getmtime(lock) > 300:
                    os.remove(lock)   # a writer that died
                time.sleep(0.5)
    try:
        dispatch(legs, c, a)
    finally:
        if writes and os.path.exists(lock):
            os.remove(lock)


def dispatch(legs, c, a):
    if c == 'build':
        cmd_build(legs)
        return
    if c == 'history':
        cmd_history(legs)
        return
    ref = reference(legs)
    if c == 'all':
        cmd_build(legs)
    if c in ('all', 'update'):
        cmd_history(legs)
    st = ours_stats(legs)
    if c in ('limits', 'all', 'update'):
        cmd_limits(legs, ref, st)
    if c in ('replay', 'all'):
        cmd_replay(legs, ref, st, quiet=(c == 'all'))
    if c in ('table', 'all', 'update'):
        cmd_table(legs, ref, st)
    if c in ('lss', 'all', 'update'):
        cmd_lss(legs, ref, st)
    if c == 'compare':
        cmd_compare(legs, ref, st, a[1], a[2])
    if c == 'progress':
        cmd_progress(legs, ref, a)
    if c == 'run':
        t0 = None
        if '-t0' in a:
            h, m, s = a[a.index('-t0') + 1].split(':')
            t0 = clock(h, m, s)
        run_report(legs, ref, st, a[1], t0)


if __name__ == '__main__':
    main()
