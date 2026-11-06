"""Speedrun splits: the story's checkpoints, the runners' splits, ours, par, fail limits, LiveSplit files.

The checkpoint list is docs\\checkpoints.tsv (hand-curated, one row per checkpoint; the user, 24 Oct 2026: "a set list of
checkpoints throughout the game's story ... right before or right after a notable event ... where player interactivity
is involved"). A leg (docs\\LEGS.md) starts at its accept; each checkpoint ends a segment. Columns:
  ours     how our FLController.log shows it: obj:<id> (the objective first shown after the previous checkpoint),
           land / launch / cutscene / control (the next dock / launch / space->cutscene / HUD back in space after a
           launch or a space scene), kill:<n> (the n-th destroyed target the mission marked), success (Mission Result),
           save:<ids> (the mission's checkpoint save pub::Save with that description id - the log's `mission-save
           ids=<ids>`, 8 ms after the game's "GAME SAVED" message 1890 the runners' splits end at: M01b.end 32120)
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
  limits             fail limits + pace data for the live check -> tools\\harness\\missions\\leg_checkpoints.tsv
  replay             the limits replayed on every archived leg: good runs failed (must be none), bad runs caught
  table              docs\\SPLITS.md (generated: par, runners, our PB / gold / sum of best, per checkpoint)
  run <leg dir|log>  one run's splits against par / PB / gold (-t0 <hh:mm:ss> with a bare log)
  compare <A> <B>    two run sets (label regexes) segment by segment: medians, WORSE / better beyond the noise
  sprt <A> <B>       the sequential verdict of set B against set A (one segment with --seg CP, else the leg):
                     KEEP / REVERT / CONTINUE (one more batch on B) / INCONCLUSIVE (B at the cap) / NODATA (stop);
                     options --gain s --alpha --beta --cap
  sprt-replay        that rule replayed over the history's consecutive sets: runs it would have used, its verdicts
                     against the fixed compare's
  progress [<leg>]   the progress review: every set per leg, NEW BEST / UNRELIABLE, NO PROGRESS (--since)
  selftest           checks of the SPRT math (known LLR values, the bounds, censoring, the guards)
  lss                LiveSplit splits files docs\\livesplit\\<leg>.lss (comparisons: par, WR run, runners' best
                     segments, our PB; our gold as best segments)
  update             history, limits, table, lss (flleg.ps1 after every leg: PB / gold / limits stay current)
  all                build, history, limits, replay, table, lss
"""
import csv
import datetime
import difflib
import json
import math
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
LIVE = os.path.join(FL, 'tools', 'harness', 'missions', 'leg_checkpoints.tsv')
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
        sv = re.search(r'event mission-save ids=(\d+) ', line)
        if sv:
            ev.append((t, 'save', sv.group(1)))
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


def our_splits(cps, ev, t0, full=True, startcp=None, skips=None):
    """Cumulative seconds from t0 per checkpoint (None = not reached / not seen). Matching as documented above; a
    checkpoint not seen does not block the later ones. A part run (from a checkpoint save) starts at the checkpoint
    whose objective the load shows first - never at an earlier one (a later launch is no M10.control). A segment
    practice run (startcp: from a save Leg.ps1 -SaveAt made at that checkpoint) has it at 0.0 (the leg start) and
    counts kills after the save's. skips (a dict, filled when given): an obj:<id> checkpoint not seen whose objective
    the script skipped - since the previous checkpoint a later checkpoint's objective showed, or one whose text id lies
    between this checkpoint's and the next obj checkpoint's (M05: 22867 posted in the jump tunnel, 22870 shown) -> its
    index: the seconds from t0 that objective showed (judge: soft; Leg.ps1 the same rule live)."""
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
                if ev[j][1] == kind and (kind not in ('obj', 'save') or ev[j][2] == val):
                    hit = j
                    break
        if hit is None:
            if kind == 'obj' and skips is not None and val.isdigit():
                # (Leg.ps1's rule: a later checkpoint's objective, or one between this one's id and the next obj
                # checkpoint's - text ids are not in story order across a leg)
                rest = [c['ours'].strip()[4:] for c in cps[n + 1:] if c['ours'].strip().startswith('obj:')]
                for j in range(max(pos, prev_hit + 1), len(ev)):
                    if ev[j][1] == 'fail':
                        break
                    o = ev[j][2]
                    if ev[j][1] != 'obj' or o in ('0', '21660') or not o.isdigit():
                        continue
                    if o in rest or (rest and rest[0].isdigit() and int(val) < int(o) < int(rest[0])):
                        skips[n] = ev[j][0] - t0
                        break
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


def hde_row(r):
    """A run on the HD Edition test install (the main one or a numbered mirror): the tables and verdicts are HDE's. A
    named instance (vanilla11c: vanilla 1.1, flleg -Instance by name since 29 Oct 2026) is another game build."""
    return re.fullmatch(r'\d*', r.get('instance') or '') is not None


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
        if not cand or not leg or not hde_row(r):
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
        skips = {}
        sp = our_splits(legs[a['leg']], log_events(os.path.join(a['dir'], 'FLController.log')), t0, a['full'], a['startcp'], skips)
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
                         a['commit'], os.path.basename(a['dir']), cp['cp'], k + 1, fmt(sp[k]), fmt(seg), fmt(skips.get(k))])
    # skip = an obj checkpoint the script skipped: when the later objective showed (our_splits; judge: soft)
    wtsv(HIST, ['date', 'label', 'save', 'start', 'outcome', 'secs', 'commit', 'dir', 'cp', 'n', 'at', 'seg', 'skip'], rows)
    print(f'{HIST}: {len(rows)} rows from {len(set(r[7] for r in rows))} runs')


def history_by_run():
    runs = {}
    for r in rtsv(HIST):
        v = runs.setdefault(r['dir'], dict(info=r, cp={}, skip={}))
        v['cp'][r['cp']] = (num(r['at']), num(r['seg']))
        if num(r.get('skip')) is not None:
            v['skip'][r['cp']] = num(r['skip'])
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
            # never below the PB run's own segment + SEG_OURS_ADD: the best run must pass its own limits (29 Oct 2026:
            # the new M13 PB, 892 s, took 84.5 s at M13.sinclair against an 82 s runner bound - Leg.ps1 would have
            # stopped the best line there)
            pb = S['pb'][k]
            pbprev = S['pb'][k - 1] if k > 0 else 0.0
            if pb is not None and pbprev is not None and pb - pbprev + SEG_OURS_ADD > seg_lim:
                seg_lim, by = pb - pbprev + SEG_OURS_ADD, 'pb'
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


def judge(leg, L, sp, t_end, full, skip=None):
    """First limit broken: (time it broke, cp, why) or None. sp = cumulative splits; t_end = the run's last second.
    The same rules as Leg.ps1's live check: the next checkpoint is due by the last one's time + its segment limit (and
    by its cumulative limit in a full leg). A kill:<n> checkpoint is soft - our log names only the kills of the
    autopilot's own target (M12: a generator destroyed by a missile in flight after the target moved on is not logged) -
    so when it is due unseen, its limit carries over to the next checkpoint; every other checkpoint is hard, except an
    obj checkpoint the script skipped (skip: index -> when the later objective showed, our_splits): soft when that
    objective came before the limits (live, Leg.ps1 skips it then)."""
    prev_t = 0.0 if full else None
    lim = 0.0
    for k, r in enumerate(L[leg]):
        t = sp[k]
        lim += r['seg_lim']
        sk = (skip or {}).get(k)
        soft = r['ours'].startswith('kill:') or (
            t is None and sk is not None and (prev_t is None or sk <= prev_t + lim) and not (full and sk > r['cum_lim']))
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
            j = judge(leg, L, sp, end, full, {k: v['skip'][cp['cp']] for k, cp in enumerate(legs[leg]) if cp['cp'] in v['skip']})
        else:
            # a part run: only the segments it timed (both ends seen, not from its load-time first checkpoint)
            j = None
            for k, cp in enumerate(legs[leg]):
                s = v['cp'].get(cp['cp'], (None, None))[1]
                if s is not None and s > L[leg][k]['seg_lim']:
                    j = (sp[k] - s + L[leg][k]['seg_lim'], cp['cp'], f'segment over {L[leg][k]["seg_lim"]:.0f} s')
                    break
        pbend = st[leg]['pb'][-1]
        # (a finished leg only: a stop-at run - 'UNTIL', its last checkpoint unseen - is no good run; full_pass_row's
        # rule, 30 Oct 2026: M13-q4s2-b's 197 s stop-at run was "failed" at M13.bsdown)
        good = (v['info']['outcome'] == 'PASS' and full and pbend is not None and end <= 1.10 * pbend
                and sp[-1] is not None)
        note = ''
        if j and good:
            # the own-history bound tightens as our gold improves: a good run older than the gold that now bounds the
            # segment is superseded, not wrongly failed
            r = next(x for x in L[leg] if x['cp'] == j[1])
            pbrun = os.path.basename(st[leg]['pbrun'] or '')
            if r['by'] == 'gold' and name < r['goldrun']:
                note = f'  (good run, superseded by the gold of {r["goldrun"]})'
            elif pbrun and name < pbrun:
                # (the cumulative deadline is the PB's: a run older than the PB that tightened it is superseded too; so
                # is a segment over any bound - the PB floor that let it pass was its own while it was the PB: 30 Oct
                # 2026, mpb23's 84.5 s M13.sinclair after the gate's new 879 s PB)
                note = f'  (good run, superseded by the PB of {pbrun})'
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
    for b in bad_good:  # (named: drift_check's line only counted them, 29 Oct 2026)
        print(f'  good run failed: {b}')
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


# ---------------------------------------------------------------- the sequential verdict (SPRT)
# docs\research\test-efficiency.md section 1 (IDEAS T1, 29 Oct 2026): a batched two-sample GSPRT per target segment -
# clear wins and losses stop after 1-2 batches of 3, close calls run to the cap. The fixed n = (2 sd / g)^2 per side
# (runs_needed) has ~29 % power to see a true gain g.
SPRT_ALPHA, SPRT_BETA, SPRT_CAP, SPRT_BATCH = 0.05, 0.2, 12, 3
SPRT_GAIN_MIN, SPRT_GAIN_X = 2.0, 0.02   # g = max(2 s, 2 % of the segment's PB) unless --gain
SPRT_SIGMA_MIN = 1.0                     # the noise floor: identical times (or one run a side) never make V zero
WINSOR_K, MAD_SD = 3.0, 1.4826           # winsorise above median + 3 MAD (the MAD scaled to a standard deviation, at
                                         # least SPRT_SIGMA_MIN): a censored DNF counts, but never as 10 normal runs
SPRT_FAIL_EXCESS = 2.0                   # REVERT when B fails 2 or more runs more than A (A's rate scaled to B's runs)
SPRT_MIN_A = 2                           # A (the finished set) needs 2+ values: one run has no spread of its own, and a
                                         # single censored DNF decided a KEEP (gate-mpb23 -> mpb24 M01b, 1 + 1 runs)


def sprt_bounds(alpha, beta):
    """(REVERT at or below, KEEP at or above): Wald's ln(beta / (1 - alpha)), ln((1 - beta) / alpha)."""
    return math.log(beta / (1 - alpha)), math.log((1 - beta) / alpha)


def winsor_cap(xs):
    med = statistics.median(xs)
    mad = statistics.median([abs(x - med) for x in xs])
    return med + WINSOR_K * max(MAD_SD * mad, SPRT_SIGMA_MIN)


def pooled_sd(a, b):
    df = len(a) + len(b) - 2
    if df < 1:
        return None
    ss = sum((x - statistics.fmean(a)) ** 2 for x in a) if len(a) > 1 else 0.0
    ss += sum((x - statistics.fmean(b)) ** 2 for x in b) if len(b) > 1 else 0.0
    return math.sqrt(ss / df)


SPRT_HIST_DF = 4      # the sets' own spread caps the history's from this many degrees of freedom (nA + nB - 2) on
SPRT_HIST_Z = 1.2816  # ... at its upper 90 % confidence bound (z of the chi-square's 10 % quantile)


def chi2_quantile(p_z, df):
    """The chi-square quantile for a standard normal z (Wilson-Hilferty; df 4: 1.06 vs the exact 1.064)."""
    h = 2.0 / (9.0 * df)
    return df * max(1e-9, 1.0 - h + p_z * math.sqrt(h)) ** 3


def sigma_rule(sd_hist, ps, df):
    """Which runs' spread counts (M13-MIMIC, 30 Oct 2026: mm3's sprt could never call - the history's 12 s came from
    M13.sinclair runs before timer_destroy changed the segment, the sets' own runs spread 2.0 s): the history (the last
    RECENT full passes) guards small sets against an under-read spread, so it counts while the sets' own runs allow it -
    from SPRT_HIST_DF degrees of freedom on it is capped at the pooled sd's upper 90 % confidence bound
    (sqrt(df s^2 / chi2_0.10(df))); with fewer, or no pooled sd, as before: max(history, pooled, floor).
    -> (sigma, the history's capped value or None)."""
    hist = sd_hist or 0.0
    capped = None
    if ps is not None and df >= SPRT_HIST_DF and hist > 0:
        ucb = ps * math.sqrt(df / chi2_quantile(-SPRT_HIST_Z, df))
        if hist > ucb:
            capped = hist = ucb
    return max(hist, ps or 0.0, SPRT_SIGMA_MIN), capped


def sprt_llr(a, b, g, sd_hist):
    """a, b = values (lower = faster). Pooled winsorising, sigma = sigma_rule(sd_hist, pooled sd), d = mean(a) - mean(b)
    (positive = B faster), V = sigma^2 (1/nA + 1/nB), LLR = g (d - g/2) / V. -> dict (a, b winsorised)."""
    cap = winsor_cap(a + b)
    wa, wb = [min(x, cap) for x in a], [min(x, cap) for x in b]
    ps = pooled_sd(wa, wb)
    sigma, hist_capped = sigma_rule(sd_hist, ps, len(wa) + len(wb) - 2)
    d = statistics.fmean(wa) - statistics.fmean(wb)
    V = sigma ** 2 * (1 / len(wa) + 1 / len(wb))
    return dict(llr=g * (d - g / 2) / V, d=d, sigma=sigma, pooled=ps, cap=cap, hist_capped=hist_capped,
                capped=sum(x > cap for x in a + b), a=wa, b=wb)


def sprt_rule(A, B, g, sd_hist, alpha=SPRT_ALPHA, beta=SPRT_BETA, cap=SPRT_CAP, leg=None):
    """The verdict. A, B = [(y, failed)] (a failed run's y already censored at its fail limit); leg = None or (TA, TB,
    gL, sdL, k): the leg totals of the full runs (censored the same way), the leg's gain and spread and its number of
    segments for the reversed check (H1: B slower by gL, at alpha / k) - it forces REVERT, as does a fail excess of B.
    The procedure (docs\\TESTING.md "The test loop" 4): A is the finished set, B grows by batches of SPRT_BATCH - no LLR
    verdict before B's first full batch (the guards may revert earlier), INCONCLUSIVE once B reaches the cap (with A
    fixed the LLR tends to g (d - g/2) nA / sigma^2 and may never leave the bounds: a cap on both sides never fired).
    No value on either side, or one on A, is NODATA (a label regex that matches nothing must not read as 'fly more')."""
    lo, hi = sprt_bounds(alpha, beta)
    out = dict(verdict='CONTINUE', llr=0.0, lo=lo, hi=hi, nA=len(A), nB=len(B), d=None, g=g, sigma=None, why=[])
    if not A or not B:
        out.update(verdict='NODATA', why=[f'set {"A" if not A else "B"} has no value for this target: check its label '
                                          'regex and that its runs reach the checkpoint'])
        return out
    if len(A) < SPRT_MIN_A:
        out.update(verdict='NODATA', why=[f'set A has {len(A)} value: the rule needs {SPRT_MIN_A}+ (fly A again, or judge '
                                          'it with compare)'])
        return out
    r = sprt_llr([y for y, _ in A], [y for y, _ in B], g, sd_hist)
    out.update(llr=r['llr'], d=r['d'], sigma=r['sigma'], pooled=r['pooled'], cap=r['cap'], capped=r['capped'],
               hist_capped=r['hist_capped'])
    # the LLR B's current mean difference can reach with A as it is (nB -> infinity): between the bounds = A too small
    out['reach'] = g * (r['d'] - g / 2) * len(A) / r['sigma'] ** 2
    fa, fb = sum(f for _, f in A), sum(f for _, f in B)
    excess = fb - fa * len(B) / len(A)
    out.update(fails=(fa, fb), excess=excess)
    if excess >= SPRT_FAIL_EXCESS:
        out['why'].append(f'B failed {fb} of {len(B)}, A {fa} of {len(A)}: {excess:.1f} more than A\'s rate (>= {SPRT_FAIL_EXCESS:.0f})')
    if leg:
        TA, TB, gL, sdL, k = leg
        if TA and TB:
            rr = sprt_llr(TB, TA, gL, sdL)   # reversed: positive d = B slower
            bound = math.log((1 - beta) / (alpha / k))
            out['legcheck'] = (rr['llr'], bound, len(TA), len(TB), -rr['d'], gL)
            if rr['llr'] >= bound:
                out['why'].append(f'leg total: B slower by {rr["d"]:.1f} s (reversed LLR {rr["llr"]:.2f} >= {bound:.2f}, alpha/{k})')
    if out['why']:
        out['verdict'] = 'REVERT'
    elif len(B) < SPRT_BATCH:
        out['why'].append(f'B has {len(B)} value(s): no verdict before its first batch of {SPRT_BATCH}')
    elif r['llr'] >= hi:
        out['verdict'] = 'KEEP'
    elif r['llr'] <= lo:
        out['verdict'] = 'REVERT'
    elif len(B) >= cap:
        out['verdict'] = 'INCONCLUSIVE'
        out['why'].append(f'B at the cap ({cap}) between the bounds: revert')
    return out


def sprt_values(runs, cps, Lleg, k):
    """One leg's runs (history_by_run values) -> [dict(y, fail, cens, inst, date, dir)] in time order. k = checkpoint
    index (its segment), or None for the leg total (full runs only). A failed run that should have timed the segment but
    did not (DNF) counts as the segment's fail limit (the leg: the last checkpoint's cumulative limit) - pessimistic
    censoring; a PASS whose log does not show the checkpoint gives no value. A failed run that never reached the
    segment's start (checkpoint k-1 untimed: it died in an earlier segment) is no sample of segment k (TIMER-DESTROY /
    M13-MIMIC, 30 Oct 2026: two S415 runs that died in the first battleship were booked as DNFs of M13.sinclair /
    M13.osiris - sigma 21 / 39, CONTINUE on 3 vs 3 disjoint samples); that death belongs to its own segment."""
    out = []
    for v in sorted(runs, key=lambda v: (v['info']['date'], v['info']['dir'])):
        i = v['info']
        fail = i['outcome'] != 'PASS'
        if k is None:
            if i['start'] != 'full':
                continue
            t = v['cp'].get(cps[-1]['cp'], (None,))[0]
            y, cens = (t, False) if t is not None and not fail else (Lleg[-1]['cum_lim'], True)
            if t is None and not fail:
                continue
        else:
            s = v['cp'].get(cps[k]['cp'], (None, None))[1]
            if s is not None:
                y, cens = s, False
            else:
                # did the run start early enough to time segment k? (a part run's first two checkpoints are untimed,
                # a segment practice run times from its save's checkpoint on: splits.py history)
                j0 = next((j for j, cp in enumerate(cps) if v['cp'].get(cp['cp'], (None,))[0] is not None), None)
                covers = i['start'] == 'full' or (j0 is not None and k > j0 + (1 if i['start'] == 'part' else 0))
                reached = k == 0 or v['cp'].get(cps[k - 1]['cp'], (None,))[0] is not None
                if not fail or not covers or not reached:
                    continue
                y, cens = Lleg[k]['seg_lim'], True
        m = re.search(r'_i(\d+)$', i['dir'])
        # (a segment's failure is its own DNF: a run that timed the segment and failed later is no failure of it -
        # M13-MIMIC mm5's judge, 30 Oct 2026: mm4-b_i1 passed M13.bsdown at 85.3 s, stalled at the Osiris, and counted
        # among A's 5 bsdown failures)
        out.append(dict(y=y, fail=fail if k is None else cens, cens=cens, inst=m.group(1) if m else '', date=i['date'],
                        dir=i['dir']))
    return out


def sprt_target(legs, st, ref, leg, k, gain):
    """g and the history spread of the target (segment k or, k None, the leg)."""
    S = st[leg]
    if k is None:
        base, sd = S['pb'][-1] or ref[leg]['par'][-1], S['sd']
    else:
        pbk = S['pb'][k]
        pbprev = S['pb'][k - 1] if k else 0.0
        base = pbk - pbprev if pbk is not None and pbprev is not None else (S['gold'][k] or ref[leg]['parseg'][k])
        sd = S['segsd'][k]
    return (gain if gain is not None else max(SPRT_GAIN_MIN, SPRT_GAIN_X * base)), sd, base


def sprt_judge(legs, ref, st, L, leg, k, runsA, runsB, gain=None, alpha=SPRT_ALPHA, beta=SPRT_BETA, cap=SPRT_CAP):
    """The rule on two sets of one leg's runs -> (result, A values, B values)."""
    cps = legs[leg]
    va, vb = sprt_values(runsA, cps, L[leg], k), sprt_values(runsB, cps, L[leg], k)
    g, sd, base = sprt_target(legs, st, ref, leg, k, gain)
    ta, tb = sprt_values(runsA, cps, L[leg], None), sprt_values(runsB, cps, L[leg], None)
    gL, sdL, _ = sprt_target(legs, st, ref, leg, None, None)
    res = sprt_rule([(x['y'], x['fail']) for x in va], [(x['y'], x['fail']) for x in vb], g, sd, alpha, beta, cap,
                    leg=([x['y'] for x in ta], [x['y'] for x in tb], gL, sdL, len(cps)))
    res.update(base=base, sd_hist=sd)
    return res, va, vb


def sprt_line(r):
    f = lambda x, s='+.1f': 'nan' if x is None else format(x, s)
    return (f'SPRT {r["verdict"]} LLR={r["llr"]:.2f} bounds=[{r["lo"]:.2f},{r["hi"]:.2f}] nA={r["nA"]} nB={r["nB"]} '
            f'd={f(r["d"])} g={r["g"]:.1f} sigma={f(r["sigma"], ".1f")}')


SPRT_USAGE = ('usage: sprt <set A> <set B> | sprt --a-samples <file> <set B>  [--seg CP] [--gain s] [--alpha 0.05] '
              '[--beta 0.2] [--cap 12]')
SPRT_OPTS = {'--seg': str, '--gain': float, '--alpha': float, '--beta': float, '--cap': int, '--a-samples': str}


def sprt_args(a):
    """sprt's arguments (after the command word; options anywhere) -> (options, None) or (None, why). Fail closed: an
    unknown option, an option without its value, a value out of range or a bad label regex is refused, never defaulted.
    --a-samples <file> (tools\\gate_harvest.py --out) is the A arm: then one label (B)."""
    o, pos, i = dict(seg=None, gain=None, alpha=SPRT_ALPHA, beta=SPRT_BETA, cap=SPRT_CAP, a_samples=None), [], 0
    while i < len(a):
        x = a[i]
        if not x.startswith('--'):
            pos.append(x)
            i += 1
            continue
        if x not in SPRT_OPTS:
            return None, f'unknown option {x}; {SPRT_USAGE}'
        if i + 1 >= len(a) or a[i + 1].startswith('--'):
            return None, f'{x} needs a value; {SPRT_USAGE}'
        try:
            o[x[2:].replace('-', '_')] = SPRT_OPTS[x](a[i + 1])
        except ValueError:
            return None, f'{x} {a[i + 1]!r} is not a number'
        i += 2
    want = 1 if o['a_samples'] else 2
    if len(pos) != want:
        return None, f'{len(pos)} set label(s), want {want}; {SPRT_USAGE}'
    if not (0 < o['alpha'] < 1 and 0 < o['beta'] < 1 and o['alpha'] + o['beta'] < 1):
        return None, f'alpha {o["alpha"]} / beta {o["beta"]}: each in (0, 1), together under 1 (the bounds\' signs)'
    if o['cap'] < SPRT_BATCH:
        return None, f'cap {o["cap"]}: at least one batch ({SPRT_BATCH})'
    if o['gain'] is not None and not (math.isfinite(o['gain']) and o['gain'] > 0):
        return None, f'gain {o["gain"]}: a positive number of seconds'
    for p in pos:
        try:
            re.compile(p)
        except re.error as e:
            return None, f'label regex {p!r}: {e}'
    o['A'], o['B'] = (None, pos[0]) if o['a_samples'] else pos
    return o, None


def harvest_arm(path, runs, B):
    """--a-samples (tools\\gate_harvest.py --out; WORK_QUEUE H18 (1), the owner 30 Oct 2026: gates feed the speed work): the A
    arm = those gate legs' eligible segments only. Each listed run becomes a copy of its history entry whose other segments
    carry no value and whose outcome reads PASS (an eligible segment ended before any failure: A gets no DNF - the stricter
    side for B's fail guard); its leg total counts only with a '(leg)' row. Fail closed: an unreadable file, a run not in
    the history, a value or build that differs from the history's, more than one build in the file, a B run on another
    build (an A/B is one build with a switch: the gate's build)."""
    try:
        rows = rtsv(path)
    except OSError as e:
        return None, str(e)
    if not rows or not {'dir', 'cp', 'seg', 'build'} <= set(rows[0]):
        return None, f'{path}: no sample rows (tools\\gate_harvest.py --out)'
    builds = set(r['build'] for r in rows)
    if len(builds) != 1:
        return None, f'{len(builds)} builds in the file ({", ".join(sorted(builds))}): one arm, one build (gate_harvest.py --build)'
    bb = set(v['info']['commit'] for v in B) - builds
    if bb:
        return None, f'B ran on {", ".join(sorted(bb))}, the samples on {next(iter(builds))}: not one build'
    A = {}
    for r in rows:
        v = runs.get(r['dir'])
        if v is None:
            return None, f'{r["dir"]} is not in the split history'
        if v['info']['commit'] != r['build']:
            return None, f'{r["dir"]}: the file says build {r["build"]}, the history {v["info"]["commit"]}'
        a = A.get(r['dir'])
        if a is None:
            a = A[r['dir']] = dict(info=dict(v['info'], outcome='PASS', start='harvest'),
                                   cp={c: (t, None) for c, (t, _) in v['cp'].items()}, skip={})
        if r['cp'] == '(leg)':
            a['info']['start'] = 'full'
            continue
        at, seg = v['cp'].get(r['cp'], (None, None))
        if seg is None or num(r['seg']) is None or abs(seg - num(r['seg'])) > 0.15:
            return None, f'{r["dir"]} {r["cp"]}: the file says {r["seg"]}, the history {fmt(seg) or "-"}'
        a['cp'][r['cp']] = (at, seg)
    return list(A.values()), None


def cmd_sprt(legs, ref, st, o):
    """sprt <A> <B> [--seg CP] [--gain s] [--alpha 0.05] [--beta 0.2] [--cap 12]: the sequential verdict of set B (the
    change) against set A (label regexes as compare takes them; o = sprt_args). The first line is the verdict; exit code
    0 always (advice: the caller decides). A driver loops: CONTINUE = one more batch of 3 on B; KEEP / REVERT /
    INCONCLUSIVE stop; anything else (NODATA, an unknown first word) stops too - fail closed."""
    seg, gain, alpha, beta, cap = o['seg'], o['gain'], o['alpha'], o['beta'], o['cap']
    runs = history_by_run()
    rxb = re.compile(o['B'])
    B = [v for v in runs.values() if rxb.search(v['info']['label'])]
    if o['a_samples']:
        A, why = harvest_arm(o['a_samples'], runs, B)
        if why:
            print(f'SPRT NODATA --a-samples: {why}')
            return
    else:
        rxa = re.compile(o['A'])
        A = [v for v in runs.values() if rxa.search(v['info']['label'])]
    k, leg = None, None
    if seg:
        hits = [(lg, j) for lg in LEG_ORDER for j, cp in enumerate(legs[lg]) if cp['cp'] == seg or cp['cp'].endswith('.' + seg)]
        shared = [(lg, j) for lg, j in hits if any(leg_of(v['info']['label']) == lg for v in A + B)]
        hits = shared or hits
        if len(hits) != 1:
            print(f'SPRT NODATA checkpoint {seg!r}: ' + (', '.join(legs[lg][j]['cp'] for lg, j in hits) + ' match' if hits else 'no such checkpoint (docs\\checkpoints.tsv)'))
            return
        leg, k = hits[0]
    else:
        both = [lg for lg in LEG_ORDER if any(leg_of(v['info']['label']) == lg for v in A) and any(leg_of(v['info']['label']) == lg for v in B)]
        if len(both) != 1:
            print('SPRT NODATA ' + (f'the sets share legs {", ".join(both)}: name --seg or narrow the labels' if both else 'no leg with runs in set A and set B'))
            return
        leg = both[0]
    A = [v for v in A if leg_of(v['info']['label']) == leg]
    B = [v for v in B if leg_of(v['info']['label']) == leg]
    both = set(v['info']['dir'] for v in A) & set(v['info']['dir'] for v in B)
    if both:   # one regex matches the other's runs ('M12-s10' finds base and core): a run on both sides judges nothing
        print(f'SPRT NODATA {len(both)} run(s) in both sets: anchor the label regexes (e.g. \'{o["B"]}$\')')
        return
    L = limits(legs, ref, st)
    r, va, vb = sprt_judge(legs, ref, st, L, leg, k, A, B, gain, alpha, beta, cap)
    print(sprt_line(r))
    tgt = f'segment {legs[leg][k]["cp"]} ({k + 1} of {len(legs[leg])})' if k is not None else 'the leg total (full runs)'
    print(f'  {leg}, target {tgt}; g {r["g"]:.1f} s = ' +
          ('--gain' if gain is not None else f'max({SPRT_GAIN_MIN:.0f} s, {SPRT_GAIN_X:.0%} of {r["base"]:.1f} s, the PB\'s)') +
          f'; alpha {alpha} beta {beta}, cap {cap} on B (A = the finished set, B in batches of {SPRT_BATCH})')
    for nm, rx, vs in (('A', o['A'] or o['a_samples'], va), ('B', o['B'], vb)):
        print(f'  {nm} {rx}: {len(vs)} values ({sum(x["cens"] for x in vs)} censored at the fail limit), '
              f'{sum(x["fail"] for x in vs)} failed: ' + ' '.join(fmt(x['y']) + ('*' if x['cens'] else '') for x in vs))
    if r.get('sigma') is not None:
        print(f'  winsorised above {r["cap"]:.1f} s (median + {WINSOR_K:.0f} MAD): {r["capped"]} value(s); sigma = max(history '
              f'{fmt(r["sd_hist"]) or "-"} (last {RECENT} full passes), pooled {fmt(r["pooled"]) or "-"}, floor {SPRT_SIGMA_MIN:.0f}) = {r["sigma"]:.1f}')
        if r.get('hist_capped') is not None:
            print(f'  the history\'s {fmt(r["sd_hist"])} is more than the sets\' own runs allow: capped at their pooled sd\'s '
                  f'upper 90 % bound {r["hist_capped"]:.1f} (df {r["nA"] + r["nB"] - 2} >= {SPRT_HIST_DF}; '
                  'docs\\TESTING.md "The test loop" 4)')
    if 'legcheck' in r:
        lr, bd, na, nb, dl, gL = r['legcheck']
        print(f'  guard leg total: A {na} / B {nb} full runs, B faster by {dl:+.1f} s; reversed LLR {lr:.2f} (REVERT at >= {bd:.2f}, g {gL:.1f})')
    else:
        print('  guard leg total: not checked (no full runs on one side)')
    if 'fails' in r:
        print(f'  guard fails: A {r["fails"][0]}, B {r["fails"][1]} (excess {r["excess"]:.1f}, REVERT at >= {SPRT_FAIL_EXCESS:.0f})')
    if r['verdict'] == 'CONTINUE' and 'reach' in r and r['lo'] < r['reach'] < r['hi']:
        print(f'  reach: with A at {r["nA"]} values, B\'s difference so far tends to LLR {r["reach"]:.2f} as B grows, '
              f'inside the bounds: more runs on B alone end INCONCLUSIVE unless B\'s mean moves - fly A again (flarms) '
              f'if the call matters')
    for w in r['why']:
        print(f'  why: {w}')


def fixed_verdict(legs, st, leg, A, B):
    """The fixed rule as compare prints it, on the leg: KEEP when the leg's median is better beyond the noise
    (max(2 s, its spread, 5 %)) and no segment WORSE; REVERT when the leg is WORSE or a segment is; else NEUTRAL."""
    cps = legs[leg]
    tot = lambda R: [v['cp'][cps[-1]['cp']][0] for v in R if v['cp'].get(cps[-1]['cp'], (None,))[0] is not None]
    ta, tb = tot(A), tot(B)
    worse = 0
    for k, cp in enumerate(cps):
        xa = [v['cp'][cp['cp']][1] for v in A if v['cp'].get(cp['cp'], (None, None))[1] is not None]
        xb = [v['cp'][cp['cp']][1] for v in B if v['cp'].get(cp['cp'], (None, None))[1] is not None]
        if xa and xb:
            ma = statistics.median(xa)
            worse += statistics.median(xb) - ma > max(2.0, st[leg]['segsd'][k] or 0.0, 0.05 * ma)
    if not ta or not tb:
        return 'REVERT' if worse else 'NEUTRAL', worse
    ma, mb = statistics.median(ta), statistics.median(tb)
    noise = max(2.0, st[leg]['sd'] or 0.0, 0.05 * ma)
    if mb - ma > noise or worse:
        return 'REVERT', worse
    return ('KEEP' if ma - mb > noise else 'NEUTRAL'), worse


def cmd_sprt_replay(legs, ref, st):
    """The measurement test-efficiency.md asks for before the rule is adopted: every pair of consecutive sets of a leg
    (full legs from its start save - stop-at runs left out -, 3+ runs each, cut as progress cuts them) replayed as the
    test loop prescribes it (TESTING.md 4): A whole (the finished set), B's runs in time order in batches of 3, through
    the sprt rule on the leg total; runs it would have used vs flown, its verdict vs the fixed rule's. The replay's
    shortcut: g and the history spread are today's (PB, last 10 passes), not those of the set's day. Also the pairing
    measurement (IDEAS T6): the sd of A-B differences paired by instance and order vs the unpaired sd."""
    L = limits(legs, ref, st)
    stop = {(r['date'], r['label'], r.get('instance') or '') for r in rtsv(os.path.join(LEGS, 'legs.tsv')) if stopat_row(r)}
    def inst(i):   # the evidence folder ends _i<instance> (archived_runs)
        m = re.search(r'_i(\d*)$', i['dir'])
        return m.group(1) if m else ''
    runs = [v for v in history_by_run().values() if v['info']['start'] == 'full'
            and (v['info']['date'], v['info']['label'], inst(v['info'])) not in stop]
    bydir = {v['info']['dir']: v for v in runs}
    rows = [dict(v['info'], _dir=v['info']['dir']) for v in runs]
    sets = {}
    for first, name, rs in run_sets(rows):
        if len(rs) >= 3:
            sets.setdefault(leg_of(name), []).append((first, name, [bydir[r['_dir']] for r in rs]))
    tot = dict(pairs=0, flown=0, used=0, flownB=0, usedB=0, out=0, agree=0, dis=[], verdicts={}, ratios=[])
    # every segment of every pair as the target (the rule's intended use); B's runs apart (A is never cut), REVERTs forced
    # by a guard apart (a REVERT's why lists only guards: the LLR's REVERT adds none)
    segv, segflown, segused, segflownB, segusedB, segguard = {}, 0, 0, 0, 0, 0

    def replay(leg, k, A, B):
        j = 0
        while True:
            j += 1
            ub = B[:SPRT_BATCH * j]
            r, _, _ = sprt_judge(legs, ref, st, L, leg, k, A, ub)
            if r['verdict'] != 'CONTINUE' or len(ub) == len(B):
                return r, A, ub

    print(f'{"pair (A -> B)":64} {"flown":>5} {"used":>4}  {"sprt":12} {"fixed":8} {"LLR":>6}')
    for leg in LEG_ORDER:
        ss = sorted(sets.get(leg, []), key=lambda x: (x[0], x[1]))
        for (fa, na, A), (fb, nb, B) in zip(ss, ss[1:]):
            A = sorted(A, key=lambda v: (v['info']['date'], v['info']['dir']))
            B = sorted(B, key=lambda v: (v['info']['date'], v['info']['dir']))
            r, ua, ub = replay(leg, None, A, B)
            used, flown = len(ua) + len(ub), len(A) + len(B)
            for k in range(len(legs[leg])):
                rs, sa, sb = replay(leg, k, A, B)
                if rs['verdict'] == 'NODATA':
                    continue
                v = rs['verdict'] if rs['verdict'] != 'CONTINUE' else 'ran out'
                segv[v] = segv.get(v, 0) + 1
                segflown += flown
                segused += len(sa) + len(sb)
                segflownB += len(B)
                segusedB += len(sb)
                segguard += rs['verdict'] == 'REVERT' and bool(rs['why'])
            seq = r['verdict'] if r['verdict'] != 'CONTINUE' else 'ran out'
            fixed, worse = fixed_verdict(legs, st, leg, A, B)
            tot['pairs'] += 1
            tot['flown'] += flown
            tot['used'] += used
            tot['flownB'] += len(B)
            tot['usedB'] += len(ub)
            tot['verdicts'][seq] = tot['verdicts'].get(seq, 0) + 1
            if seq == 'ran out':
                tot['out'] += 1
            else:
                keep_s, keep_f = seq == 'KEEP', fixed == 'KEEP'
                if keep_s == keep_f:
                    tot['agree'] += 1
                else:
                    tot['dis'].append(f'{leg} {na} -> {nb}: sprt {seq}, fixed {fixed}')
            print(f'{(na + " -> " + nb)[:64]:64} {flown:5} {used:4}  {seq:12} {fixed:8} {r["llr"]:6.2f}')
            # pairing (T6): A's and B's runs on the same instance, in order
            va, vb = sprt_values(A, legs[leg], L[leg], None), sprt_values(B, legs[leg], L[leg], None)
            diffs = []
            for inst in sorted(set(x['inst'] for x in va) & set(x['inst'] for x in vb)):
                pa, pb = [x['y'] for x in va if x['inst'] == inst], [x['y'] for x in vb if x['inst'] == inst]
                diffs += [p - q for p, q in zip(pa, pb)]
            ps = pooled_sd([x['y'] for x in va], [x['y'] for x in vb])
            if len(diffs) >= 3 and ps:
                tot['ratios'].append(statistics.stdev(diffs) / (math.sqrt(2) * ps))
    if not tot['pairs']:
        print('sprt-replay: no pair of consecutive sets with 3+ runs')
        return
    judged = tot['pairs'] - tot['out']
    print(f'sprt-replay: {tot["pairs"]} pairs, verdicts ' + ', '.join(f'{k} {v}' for k, v in sorted(tot['verdicts'].items())) +
          f'; runs used {tot["used"]} of {tot["flown"]} flown ({1 - tot["used"] / tot["flown"]:.0%} saved; A whole, B side '
          f'{tot["usedB"]} of {tot["flownB"]}, {1 - tot["usedB"] / tot["flownB"]:.0%}); "ran out" = no verdict within the '
          f'runs flown ({tot["out"]}); keep / not-keep agrees with the fixed rule in {tot["agree"]} of {judged}, '
          f'disagreements {len(tot["dis"])}')
    for d in tot['dis']:
        print(f'  disagreement: {d}')
    if segflown:
        print(f'per segment (every segment of every pair as the target): {sum(segv.values())} tests, verdicts ' +
              ', '.join(f'{k} {v}' for k, v in sorted(segv.items())) +
              f' (REVERT by a guard {segguard}); runs used {segused} of {segflown} flown ({1 - segused / segflown:.0%} saved; '
              f'B side {segusedB} of {segflownB}, {1 - segusedB / segflownB:.0%})')
    rs = tot['ratios']
    if rs:
        print(f'pairing (T6): {len(rs)} pairs with 3+ same-instance pairs; paired sd / unpaired sd (sqrt 2 x pooled): '
              f'median {statistics.median(rs):.2f}, range {min(rs):.2f}-{max(rs):.2f} (under 1 = pairing cuts the noise)')


def cmd_selftest():
    """Known values of the SPRT math (no data read); exit 1 on a failure."""
    bad = []

    def ok(name, got, want, tol=1e-3):
        if (isinstance(want, str) and got != want) or (not isinstance(want, str) and abs(got - want) > tol):
            bad.append(f'{name}: got {got}, want {want}')

    lo, hi = sprt_bounds(0.05, 0.2)
    ok('bound lo ln(0.2/0.95)', lo, -1.5581)
    ok('bound hi ln(0.8/0.05)', hi, 2.7726)
    ok('bounds 0.05/0.05', sprt_bounds(0.05, 0.05)[1], 2.9444)
    # d 2, sigma 1 (history), V = 2/3, LLR = 2 (2 - 1) / (2/3) = 3 -> KEEP
    r = sprt_rule([(10, False)] * 3, [(8, False)] * 3, 2.0, 1.0)
    ok('llr known', r['llr'], 3.0)
    ok('keep', r['verdict'], 'KEEP')
    # the same at d 0: LLR = 2 (0 - 1) / (2/3) = -3 -> REVERT
    ok('revert', sprt_rule([(10, False)] * 3, [(10, False)] * 3, 2.0, 1.0)['verdict'], 'REVERT')
    # d 1 = g/2: LLR 0 -> CONTINUE; at the cap INCONCLUSIVE
    ok('continue', sprt_rule([(10, False)] * 3, [(9, False)] * 3, 2.0, 1.0)['verdict'], 'CONTINUE')
    ok('cap', sprt_rule([(10, False)] * 12, [(9, False)] * 12, 2.0, 1.0)['verdict'], 'INCONCLUSIVE')
    # sigma = max(history, pooled): pooled sd of [9, 11] and [7, 9] = sqrt((2 + 2) / 2) = 1.414
    r = sprt_rule([(9, False), (11, False)], [(7, False), (9, False)], 2.0, 0.5)
    ok('pooled sigma', r['sigma'], math.sqrt(2))
    ok('llr pooled', r['llr'], 2 * (2 - 1) / (2 * (1 / 2 + 1 / 2)))
    # the sigma rule: the history counts up to the sets' pooled sd's upper 90 % bound once df >= 4 (Wilson-Hilferty)
    ok('chi2 0.10 df 4', chi2_quantile(-SPRT_HIST_Z, 4), 1.064, 0.01)
    ok('chi2 0.10 df 10', chi2_quantile(-SPRT_HIST_Z, 10), 4.865, 0.02)
    ok('hist kept under df 4', sigma_rule(12.0, 2.0, 3)[0], 12.0)
    ok('hist capped', sigma_rule(12.0, 2.0, 7)[0], 2.0 * math.sqrt(7 / chi2_quantile(-SPRT_HIST_Z, 7)))
    ok('hist within the bound kept', sigma_rule(2.5, 2.0, 7)[0], 2.5)
    ok('pooled over hist', sigma_rule(1.5, 2.0, 7)[0], 2.0)
    ok('no pooled', sigma_rule(12.0, None, 7)[0], 12.0)
    # winsorising: [10, 10, 10, 11, 100] -> median 10, MAD 0 -> floor 1 -> cap 13
    ok('winsor cap', winsor_cap([10, 10, 10, 11, 100]), 13.0)
    # a segment run's death: in the target segment it is its DNF; before it (the previous checkpoint untimed) no sample
    cps3 = [dict(cp='S.a'), dict(cp='S.b'), dict(cp='S.c')]
    lim3 = [dict(seg_lim=50.0, cum_lim=50.0), dict(seg_lim=60.0, cum_lim=110.0), dict(seg_lim=70.0, cum_lim=180.0)]
    run = lambda d, cp: dict(info=dict(outcome='FAIL', start='seg', date='d', dir=d), cp=cp)
    died_b = run('x_i1', {'S.a': (0.0, None)})                      # died in S.b (S.a timed: the start)
    died_c = run('x_i2', {'S.a': (0.0, None), 'S.b': (40.0, 40.0)})  # died in S.c
    ok('dnf in its segment', str([x['y'] for x in sprt_values([died_b, died_c], cps3, lim3, 1)]), '[60.0, 40.0]')
    ok('no sample before it', str([x['y'] for x in sprt_values([died_b, died_c], cps3, lim3, 2)]), '[70.0]')
    # ... and a run that timed the segment, then failed later, is a timed sample, not a failure of it
    ok('later failure no fail', str([x['fail'] for x in sprt_values([died_b, died_c], cps3, lim3, 1)]), '[True, False]')
    # censoring: a DNF at its fail limit (100) on B pulls it slower, capped at median + 3 MAD
    r = sprt_llr([10] * 3, [10, 10, 100], 2.0, 1.0)
    ok('censored capped', r['capped'], 1)
    ok('censored at the cap', r['b'][2], 13.0)
    ok('censored d', r['d'], -1.0)
    ok('censored revert', sprt_rule([(10, False)] * 3, [(10, False), (10, False), (100, True)], 2.0, 1.0)['verdict'], 'REVERT')
    # the fail guard: B 2 fails more than A forces REVERT over a KEEP-sized gain
    r = sprt_rule([(10, False)] * 3, [(5, True), (5, True), (5, False)], 2.0, 1.0)
    ok('fail guard', r['verdict'], 'REVERT')
    # the reversed leg check: segment KEEP, leg total B 20 s slower (g 2, sigma 1, k 10) -> REVERT
    r = sprt_rule([(10, False)] * 3, [(7, False)] * 3, 2.0, 1.0, leg=([300.0] * 3, [320.0] * 3, 2.0, 1.0, 10))
    ok('leg guard', r['verdict'], 'REVERT')
    ok('leg guard bound', r['legcheck'][1], math.log(0.8 / (0.05 / 10)))
    r = sprt_rule([(10, False)] * 3, [(7, False)] * 3, 2.0, 1.0, leg=([300.0] * 3, [299.0] * 3, 2.0, 1.0, 10))
    ok('leg guard passes', r['verdict'], 'KEEP')
    ok('no A', sprt_rule([], [(1, False)], 2.0, 1.0)['verdict'], 'NODATA')
    # an empty B is a label regex that matches nothing (or runs that miss the checkpoint): stop, never 'fly more'
    ok('no B', sprt_rule([(1, False)] * 3, [], 2.0, 1.0)['verdict'], 'NODATA')
    ok('one A', sprt_rule([(10, False)], [(8, False)] * 3, 2.0, 1.0)['verdict'], 'NODATA')
    # no LLR verdict before B's first batch (d 4: LLR 7.2 would KEEP) - but the guards may revert at once
    ok('B under a batch', sprt_rule([(10, False)] * 3, [(6, False)] * 2, 2.0, 1.0)['verdict'], 'CONTINUE')
    ok('guard under a batch', sprt_rule([(10, False)] * 3, [(5, True), (5, True)], 2.0, 1.0)['verdict'], 'REVERT')
    # the cap is B's (A is the finished set): A 3 / B 12 between the bounds -> INCONCLUSIVE; A 12 / B 3 -> CONTINUE
    ok('cap on B', sprt_rule([(10, False)] * 3, [(9, False)] * 12, 2.0, 1.0)['verdict'], 'INCONCLUSIVE')
    ok('A long, B short', sprt_rule([(10, False)] * 12, [(9, False)] * 3, 2.0, 1.0)['verdict'], 'CONTINUE')
    # reach: g (d - g/2) nA / sigma^2 = 2 (1.5 - 1) 3 / 1 = 3
    ok('reach', sprt_rule([(10, False)] * 3, [(8.5, False)] * 3, 2.0, 1.0)['reach'], 3.0)
    # the arguments: options anywhere, anything not understood refused (fail closed)
    o, why = sprt_args(['--seg', 'gen1', 'M12-a$', 'M12-b$', '--gain', '3'])
    ok('args parse', (o or {}).get('seg', '') + '|' + str((o or {}).get('gain')) + '|' + (o or {}).get('B', ''), 'gen1|3.0|M12-b$')
    for bad_args in (['a', 'b', '--gain'], ['a', 'b', '--gain', '--seg', 'x'], ['a', 'b', '--alpha', '0'],
                     ['a', 'b', '--beta', '0.96'], ['a', 'b', '--cap', '0'], ['a', 'b', '--gain', '-3'],
                     ['a', 'b', '--gain', 'nan'], ['M12-(', 'b'], ['a'], ['a', 'b', 'c'], ['a', 'b', '--sge', 'x']):
        ok('args refuse ' + ' '.join(bad_args), 'refused' if sprt_args(bad_args)[1] else 'accepted', 'refused')
    # an obj checkpoint the script skipped (M05: 22867 posted in the jump tunnel, 22870 shown): soft, not a fail
    cps = [dict(cp='a', ours='launch'), dict(cp='b', ours='obj:22867'), dict(cp='c', ours='obj:22870'), dict(cp='d', ours='land')]
    ev = [(100.0, 'launch', ''), (150.0, 'obj', '22865'), (160.0, 'obj', '22870'), (200.0, 'land', '')]
    skips = {}
    sp = our_splits(cps, ev, 90.0, True, None, skips)
    ok('skip found', str(sorted(skips.items())), str([(1, 70.0)]))
    ok('skip splits', str(sp), str([10.0, None, 70.0, 110.0]))
    lim = {'S': [dict(cp=c['cp'], ours=c['ours'], seg_lim=100.0, cum_lim=1000.0) for c in cps]}
    ok('skip judged soft', str(judge('S', lim, sp, 110.0, True, skips)), 'None')
    ok('unskipped hard', str(judge('S', lim, sp, 400.0, True)), str((110.0, 'b', 'segment over 100 s')))
    ok('skip after the limit', str(judge('S', lim, sp, 400.0, True, {1: 150.0})), str((110.0, 'b', 'segment over 100 s')))
    # the in-between form (M05's real ids: 22867 skipped, 22870 shown, the next checkpoint 22875), and no cascade: a
    # high id out of story order (M11.ishole 24125) skips nothing that is not before a later checkpoint's objective
    cps2 = [dict(cp='a', ours='launch'), dict(cp='b', ours='obj:22867'), dict(cp='c', ours='obj:22875'), dict(cp='d', ours='land')]
    skips = {}
    our_splits(cps2, [(100.0, 'launch', ''), (160.0, 'obj', '22870'), (180.0, 'obj', '22875')], 90.0, True, None, skips)
    ok('skip between', str(sorted(skips.items())), str([(1, 70.0)]))
    cps3 = [dict(cp='a', ours='launch'), dict(cp='b', ours='obj:24055'), dict(cp='c', ours='obj:24060'), dict(cp='d', ours='land')]
    skips = {}
    our_splits(cps3, [(100.0, 'launch', ''), (160.0, 'obj', '24125')], 90.0, True, None, skips)
    ok('no cascade', str(sorted(skips.items())), '[]')
    # save:<ids> (M01b.end = the checkpoint save 32120 at GAME SAVED, s7arb3 i3 16:50:32.604): the log line parsed, only
    # that save's id matches (the leg's earlier saves 32110 / 32115 and the GAME SAVED message itself do not)
    import tempfile
    with tempfile.TemporaryDirectory() as td:
        lg = os.path.join(td, 'FLController.log')
        with open(lg, 'w', encoding='latin-1') as f:
            f.write('[16:44:58.493] event mission-save ids=32110 result=0 "Mission 01: Continue the Search Pattern"\n'
                    '[16:49:13.430] event objective count=1 title=22000 "MISSION 1" text=22005 "" obj=22050 "Destroy"\n'
                    '[16:50:32.596] event mission-message type=7 text=1890 "GAME SAVED"\n'
                    '[16:50:32.604] event mission-save ids=32120 result=0 "Mission 01: Criminal Base Destroyed"\n'
                    '[16:50:48.380] event objective count=1 title=22000 "MISSION 1" text=22005 "" obj=22055 "Fly"\n')
        lev = log_events(lg)
    ok('save parsed', str([e for e in lev if e[1] == 'save']), str([(clock(16, 44, 58, 493), 'save', '32110'),
                                                                    (clock(16, 50, 32, 604), 'save', '32120')]))
    cps4 = [dict(cp='p', ours='obj:22050'), dict(cp='e', ours='save:32120')]
    sp = our_splits(cps4, lev, clock(16, 44, 0), True)
    ok('save split', str([s if s is None else round(s, 3) for s in sp]), str([round(clock(16, 49, 13, 430) - clock(16, 44, 0), 3),
                                                          round(clock(16, 50, 32, 604) - clock(16, 44, 0), 3)]))
    print(f'selftest: {"OK" if not bad else "FAILED"} ({len(bad)} failure(s))')
    for b in bad:
        print('  ' + b)
    return not bad


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


SET_MIN_RUNS = 2   # flset.ps1's default is -Runs 2: a set of one instance's default side counts (T28, 29 Oct 2026)
SET_GAP_H = 6.0    # a label reused more than 6 h after its last run is another attempt, also on the same commit string
# The commit column names the build only since flleg.ps1 records build\dev's stamp (ca6e675, 29 Oct 2026 06:49): before,
# it was HEAD at the leg's start + "+dirty" from a plain git status that the leg bookkeeping itself dirtied (buildlock.ps1's
# header: 699 of 881 rows) - one set's first batch clean, the rest +dirty, a handoff commit mid-set another HEAD. Cut by
# it there and genuine sets fall apart (measured 29 Oct 2026: M04 flipped to NO PROGRESS on halves of its own sets).
STAMP_SINCE = '2026-10-29 06:49'


def stopat_row(r):
    """A legs.tsv row of a stop-at run (flset / flleg -StopAt: Leg.ps1 ends it at the checkpoint, detail 'UNTIL total
    <s> s', outcome PASS, secs = the time to that checkpoint) - not a full leg, never a set's time (134 rows on 29 Oct
    2026, 42 from start saves: two of them under one label made a NEW BEST of a part leg once 2-run sets counted)."""
    return (r.get('detail') or '').startswith('UNTIL')


def full_pass_row(r):
    """THE definition of a full leg's PASS in legs.tsv (WORK_QUEUE H10 #13, 30 Oct 2026: a stop-at run was taken for a
    leg's best twice - flgate 78752d8, legtable d9f445a): outcome PASS, detail 'SUCCESS total ...' (the leg finished:
    never a stop-at 'UNTIL' run), an HD Edition instance (hde_row). tools\\flgate.ps1 and tools\\legtable.js apply the same
    three tests; `splits.py bestcheck` (drift_check `leg-bests`) checks LEGS.md's bests against this one."""
    return r.get('outcome') == 'PASS' and (r.get('detail') or '').startswith('SUCCESS') and hde_row(r)


def cmd_bestcheck():
    """Per leg of docs\\legs_reference.tsv: the best full leg from its start save by full_pass_row vs docs\\LEGS.md's
    'Ours (best full leg)'. Prints one line; exit 1 on a mismatch."""
    best = {}
    for r in rtsv(os.path.join(LEGS, 'legs.tsv')):
        try:
            s = int(float(r.get('secs') or 0))
        except ValueError:
            continue
        if full_pass_row(r) and s > 0:
            k = (r.get('save') or '').lower()
            best[k] = min(best.get(k, s), s)
    shown = {}
    with open(os.path.join(FL, 'docs', 'LEGS.md'), encoding='utf-8') as f:
        inside = False   # (only the table under legtable.js's "Ours (best full leg)" header)
        for line in f:
            c = [x.strip() for x in line.strip().strip('|').split('|')]
            if len(c) >= 5 and c[4].startswith('Ours (best full leg)'):
                inside = True
            elif inside and not line.startswith('|'):
                break
            elif inside and re.fullmatch(r'M\d\d\w*', c[0]) and re.fullmatch(r'\d+|-', c[4]):
                shown.setdefault(c[0], c[4])
    if not shown:
        print("leg bests: LEGS.md's 'Ours (best full leg)' table not found")
        return False
    bad = []
    for r in rtsv(LEGREF):
        want = best.get((r.get('start_save') or '').lower())
        got = shown.get(r['leg'])
        if got is None:
            continue
        if str(want if want is not None else '-') != got:
            bad.append(f"{r['leg']} LEGS.md {got} vs legs.tsv {want if want is not None else '-'}")
    print('leg bests: ' + ('; '.join(bad) if bad else f'{len(shown)} legs agree with full_pass_row'))
    return not bad


def run_sets(rows, split=True):
    """The sets of one or more legs: rows (dicts with date, label, commit) sharing a label, in time order; with split a
    reused label is cut where the build differs (the commit stamp flleg.ps1 records, on rows since STAMP_SINCE) or a gap
    of over SET_GAP_H hours lies between two runs (also two dirty builds of one commit carry the same stamp). ->
    [(first date, name, rows)]; a cut label's sets are named <label>@<commit> (#n for the later ones on one stamp)."""
    by = {}
    for r in sorted(rows, key=lambda r: r['date']):
        by.setdefault((leg_of(r['label']), r['label']), []).append(r)
    out = []
    for (leg, label), rs in by.items():
        groups = [[rs[0]]]
        for r in rs[1:]:
            last = groups[-1][-1]
            gap = (datetime.datetime.strptime(r['date'][:16], '%Y-%m-%d %H:%M') -
                   datetime.datetime.strptime(last['date'][:16], '%Y-%m-%d %H:%M')).total_seconds() / 3600.0
            first = groups[-1][0]
            build = r['date'] >= STAMP_SINCE and first['date'] >= STAMP_SINCE and r.get('commit', '') != first.get('commit', '')
            if split and (build or gap > SET_GAP_H):
                groups.append([r])
            else:
                groups[-1].append(r)
        seen = {}
        for g in groups:
            name = label
            if len(groups) > 1:
                c = g[0].get('commit', '') or '?'
                seen[c] = seen.get(c, 0) + 1
                name = f'{label}@{c}' + (f'#{seen[c]}' if seen[c] > 1 else '')
            out.append((g[0]['date'], name, g))
    return sorted(out, key=lambda x: (x[0], x[1]))


def cmd_progress(legs, ref, args):
    """The progress review (docs\\TESTING.md "AFK sessions" 5; the user, 24 Oct 2026: continuous testing and changes must
    not loop without meaningful improvement): per leg, every set (runs sharing a label and a build - run_sets: a reused
    label is cut where the commit stamp changes or 6 h lie between runs - full legs from the leg's start save, stop-at
    runs left out, at least SET_MIN_RUNS runs) in time order - runs, PASS rate, the PASS median, its difference to the best set before it, and
    par. Flags: NEW BEST (better by max(2 s, 1 %)), UNRELIABLE (PASS rate under 5 / 6), and per leg NO PROGRESS when the
    last 3 sets brought no new best - the signal to change the target or the approach, not to run another set. The last
    line per leg is the verdict (tools\\claude\\hooks\\guard.js set-no-progress, drift_check.ps1 progress read it).
    Usage: progress [<leg>] [--since 'YYYY-MM-DD HH:MM']; --min-runs n / --no-split reproduce older counts (3, no cut:
    before 29 Oct 2026) for measurements"""
    want = next((x for x in args[1:] if re.match(r'^M\d\d[ab]?$', x)), None)
    since = args[args.index('--since') + 1] if '--since' in args else ''
    minruns = int(args[args.index('--min-runs') + 1]) if '--min-runs' in args else SET_MIN_RUNS
    starts = {r['leg']: r['start_save'] for r in rtsv(LEGREF)}
    rows = []
    for r in rtsv(os.path.join(LEGS, 'legs.tsv')):
        leg = leg_of(r['label'])
        if not leg or (want and leg != want) or r['save'] != starts.get(leg) or r['date'] < since or not hde_row(r) \
                or stopat_row(r):
            continue
        rows.append(r)
    by_leg = {}
    for first, name, rs in run_sets(rows, split='--no-split' not in args):
        if len(rs) >= minruns:
            by_leg.setdefault(leg_of(name), []).append((first, name, [(r['outcome'], num(r['secs'])) for r in rs]))
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
        if best is None:   # (no set with a PASS yet: sets of 2 runs made this reachable, 29 Oct 2026)
            print(f'  no set with a PASS yet; {verdict}')
        else:
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
    held = False
    if writes:
        # several legs end at once (one flleg per instance): one writer at a time
        import time
        for _ in range(120):
            try:
                os.close(os.open(lock, os.O_CREAT | os.O_EXCL))
                held = True
                break
            except FileExistsError:
                try:
                    if time.time() - os.path.getmtime(lock) > 300:
                        os.remove(lock)   # a writer that died
                except FileNotFoundError:
                    pass                  # released meanwhile: retry at once
                    continue
                time.sleep(0.5)
        else:
            # never write beside another writer, and never delete its lock
            sys.exit('splits.lock is held by another writer (60 s): nothing written')
    try:
        dispatch(legs, c, a)
    except Exception as e:
        if c != 'sprt':
            raise
        # sprt's contract: the first line is the verdict, exit code 0 - a crash is a NODATA (stop), never a traceback
        print(f'SPRT NODATA error {type(e).__name__}: {e}')
    finally:
        if held and os.path.exists(lock):
            os.remove(lock)


def dispatch(legs, c, a):
    if c == 'build':
        cmd_build(legs)
        return
    if c == 'history':
        cmd_history(legs)
        return
    if c == 'selftest':
        if not cmd_selftest():
            sys.exit(1)
        return
    if c == 'bestcheck':
        if not cmd_bestcheck():
            sys.exit(1)
        return
    if c == 'sprt':
        so, why = sprt_args(a[1:])
        if why:
            print(f'SPRT NODATA {why}')
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
    if c == 'sprt':
        cmd_sprt(legs, ref, st, so)
    if c == 'sprt-replay':
        cmd_sprt_replay(legs, ref, st)
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
