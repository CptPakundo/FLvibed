"""apdiff: a deterministic comparison of archived autopilot leg runs - structure, not equality.

Why (checkup_2026-10-24\\audit_autopilot.md 7 "step 0"): a behaviour-preserving refactor is today verified by the leg
gate (1.4 h of game). The game is non-deterministic, so two runs of one build never match line by line; but a
mechanical refactor that changes a DECISION shows up as a changed event sequence or a changed event-kind mix, and a
changed flight profile shows up in the motion trace. This tool reports those structures side by side, in seconds, from
leg folders: a baseline (one or several runs of the pre-refactor build) and the run(s) under test.

Usage (OCR venv python, -I):
  apdiff.py -Baseline <folder | label>[,<more>] -Run <folder | label>[,<more>] [--tol 0.02] [--kind-abs 4]
            [--kind-rel 0.5] [--segments <re>] [--all-kinds] [--context 5] [--no-collapse] [--quiet]
  A folder is a path or a name under research\\experience\\legs. A set label (legs.tsv `label`, e.g. M04-s12d-padr2)
  means every PASS run of that label's newest build (the commit of its last row); `<label>:<k>` its k-th row in
  legs.tsv order (1-based, negative from the end); `<label>@<commit>` its PASS runs on that commit. The chosen folders
  are printed. Several baseline runs form an ENVELOPE (min..max per metric and per event kind); each run is judged
  against it. Exit code 1 when any run has a flagged segment or a flagged decision kind, 0 otherwise; --quiet prints
  the verdict line(s) only.

Data relied on (one folder per leg run, research\\experience\\legs\\<date>_<HHMM>_<label>_i<n>\\; flleg.ps1 writes it):
  FLController.log   `[HH:MM:SS.mmm] event autopilot <kind> [<sub>] key=value ... free text` = events::Push lines (the
                     controller's event stream; other lines are the harness / host logs). Clock = local wall time.
  FLMotion.tsv       10 rows / s while the autopilot flies (autopilot_frame.cpp MotionTrace), header:
                     ms mode action speed steer throttle ab cruise slide guard clear wall wall_m target tgt_m align
                     x y z engine fx fy fz system. ms = GetTickCount64 (uptime, not wall time); mode = autopilot Mode
                     enum (0 off 1 goto 2 dock 3 follow 4 formation 5 face 6 escort 7 combat 8 travel 9 hold 10 shoot,
                     -1 the player flies); speed m/s; engine 2 = cruising (charged), 1 charging, 0 off; no rows while
                     docked / in scenes (gaps). Aligned to the log clock by matching its mode changes to the log's
                     `autopilot start mode=<name>` lines (median offset; fallback: the first system change, as routecmp.py).
  transcript.txt     the harness transcript; `HH:MM:SS +    0 <text>` = the leg start (t0, 1 s resolution; splits.py).
  phases.tsv         `cp <at> <secs> <name>` rows = the checkpoints as Leg.ps1 judged them live (at = s from t0).
  legs.tsv           the history: date label save outcome secs boot fps commit instance detail (one row per run; the
                     row -> folder match is splits.py archived_runs()).
  splits.tsv         derived by splits.py history: per run and checkpoint `dir cp n at seg` (at = s from t0; precise
                     to the log event). Checkpoint source order here: splits.tsv, phases.tsv, else computed from the
                     log with splits.py (docs\\checkpoints.tsv `ours` specs).

Method:
  (a) events: every `event autopilot` line from t0 - 5 s on is normalised: `key=value` keeps the value only for
      categorical keys (mode, tactic, why, reason, verb, kind, text, step ... KEEP below) and becomes `key=#` otherwise;
      hex ids, numbers, coordinates become `#`; words stay (so `objective leash <nickname>`, `nearest: #/#/ship/#m`,
      `-> burner` keep their categories). The kind = first token (+ the second token's word or key: `objective decide`,
      `fast-dock reply`, `missile-outcome hit`). Consecutive identical lines collapse into one (bow-arc / blind-spot
      spam at 10 Hz). Kinds are classed DECISION (objective, travel, path, fast-dock, hold, approach, start / stop,
      avoid, guards ...) or FIGHT (tactic, target*, missile*, cm, repair*, turn-evade, extend, static-hold, bump ...;
      FIGHT below): a fight's event mix follows the enemy's random behaviour, so fight kinds are printed and flagged
      (lower-case) but count in the verdict only with --all-kinds. Printed per run: the first divergence from the most
      similar baseline run with context, the per-kind counts against the envelope (FLAG when the count lies outside
      [min, max] by more than max(--kind-abs, --kind-rel * the larger count)), kinds in the run / baseline only, and
      per checkpoint segment the sequence similarity (difflib ratio of the segment's collapsed events).
  (b) motion per checkpoint segment (t0 + at of the previous checkpoint .. this one, in motion time): dur (the split),
      dist flown (sum of row steps, gaps > 2 s skipped), mean / max speed, s at cruise (engine 2), s under 100 m/s.
      A metric differs when the run's value lies outside the envelope by more than max(--tol * the larger magnitude,
      floor): floors dur 2 s, dist 100 m, speeds 5 m/s, cruise 2 s, slow 2 s. Segments with fight events in any run
      are marked * in the table. --segments (a regex on the checkpoint name) restricts the verdict to those segments -
      use it for the 2-3 short segments a refactor baseline was flown on.
  (c) outcome, total seconds and the build (legs.tsv commit) of every run.

What it can and cannot claim (calibration on the 24 Oct 2026 archive: M04 s12c / s12d and M12 s11s / s11v, 29 runs on
7 builds; docs\\TESTING.md "The test loop"): with ONE baseline run and --tol 0.02 an identical-build pair of full
legs fails 4-11 of 9-17 segments (fights, dock queues, loiter: durations differ 5-40 %, event sequences from the first
fight on) - 1 vs 1 at 2 % proves nothing about a full leg. A metric or kind the baseline runs themselves spread beyond
the tolerance has no verdict power and is marked ~ (not judged, --strict judges it anyway): with 10 baseline runs the
leave-one-out false-alarm rate was 3 of 11 runs (one segment each: gen3 / exit / cell of M12), with 2 baseline runs
3 of 3 (the envelope is too thin - fly at least 3 baseline runs, better 6); decision kinds false-alarmed in 3 of 29
runs (avoid detour, path arrive, geometry-guard, fast-dock airlock: geometry retries), fight kinds in 12 of 29. The
different-build pairs showed their real change in every judged run on the same segment (s12c -> s12d: M04.hunters
cruise 12 -> 4 s, pre_aggro fights the hunters early; s11s -> s11v: M12.hole 55 -> 60-63 s with 20-24 s under 100 m/s).
So: a FLAG that recurs in every judged run on one segment is a finding; a FLAG in one run of three is noise; `kinds
only in the run / baseline` is where a new or removed code path shows (the s12c dock stall's `fast-dock losing` /
`travel game-dock` / `fast-dock stalled`, the harness's `preset speedrun`). Never proof - a lead to the log.
"""
import argparse
import csv
import difflib
import os
import re
import statistics
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
HERE = os.path.dirname(os.path.abspath(__file__))
FL = os.path.dirname(HERE)
LEGS = os.path.join(FL, 'research', 'experience', 'legs')
TS = re.compile(r'^\[(\d\d):(\d\d):(\d\d)\.(\d+)\] event autopilot (.*)$')
MODES = ['off', 'goto', 'dock', 'follow', 'formation', 'face', 'escort', 'combat', 'travel', 'hold', 'shoot']
# key=value keys whose VALUE is a category (kept); every other value becomes '#'
KEEP = {'mode', 'tactic', 'why', 'reason', 'verb', 'kind', 'opt', 'from', 'text', 'title', 'obj', 'action', 'phase',
        'law', 'pilot', 'state', 'result', 'response', 'dir', 'side', 'step', 'index', 'type', 'part', 'way', 'look',
        'planned', 'use', 'now', 'cruise', 'on-line', 'aligned', 'dodged', 'up', 'dead', 'has-pos', 'settled'}
# kinds (first token) whose counts follow the fight, not our decisions
FIGHT = ('tactic', 'target', 'target-shield', 'target-destroyed', 'target-given-up', 'missile', 'cm', 'bow-arc',
         'blind-spot', 'line-of-fire', 'turn-evade', 'extend', 'repair', 'odds', 'duel', 'launch-held', 'dodge-missile',
         'combat', 'torpedo-launch', 'salvo-check', 'hold-mine', 'static-hold', 'arc', 'bump', 'tractor')
HEX = re.compile(r'0x[0-9A-Fa-f]+')
NUM = re.compile(r'(?<![\w.])-?\d+(?:\.\d+)?')
KV = re.compile(r'([A-Za-z][\w-]*)=(\S*)')
WORD = re.compile(r'^[a-z][a-z-]*$')
FLOORS = dict(dur=2.0, dist=100.0, mean=5.0, max=5.0, cruise=2.0, slow=2.0)
METRICS = ['dur', 'dist', 'mean', 'max', 'cruise', 'slow']


def clock(h, m, s, ms=0):
    return int(h) * 3600 + int(m) * 60 + int(s) + int(ms) / 1000.0


def splits_module():
    """tools\\splits.py (same folder) for the legs.tsv -> folder match and the checkpoint computation."""
    if HERE not in sys.path:
        sys.path.insert(0, HERE)
    import splits  # noqa: E402  (its top level only defines paths and functions)
    return splits


# ---------------------------------------------------------------- the run folders
def archived():
    try:
        return splits_module().archived_runs()
    except Exception as e:  # legs.tsv missing / unreadable: folders still work
        print(f'(legs.tsv not read: {e})', file=sys.stderr)
        return []


def resolve(spec, runs):
    """-> [folders]. A folder (path or name under legs\\), or a legs.tsv label: every PASS run of its newest build;
    <label>:<k> the k-th row; <label>@<commit> the PASS runs on that commit."""
    out = []
    for item in spec.split(','):
        item = item.strip()
        hit = None
        for cand in (item, os.path.join(LEGS, item)):
            if os.path.isdir(cand) and os.path.isfile(os.path.join(cand, 'FLController.log')):
                hit = os.path.abspath(cand)
        if hit:
            out.append(hit)
            continue
        m = re.match(r'^([^:@]+)(?::(-?\d+))?(?:@(\S+))?$', item)
        rows = [r for r in runs if m and r['label'] == m.group(1)]
        if not rows:
            sys.exit(f'apdiff: {item!r} is neither a leg folder nor a label in legs.tsv')
        if m.group(2):
            i = int(m.group(2))
            out.append(rows[i - 1 if i > 0 else i]['dir'])
            continue
        commit = m.group(3) or next((r['commit'] for r in reversed(rows) if r['outcome'] == 'PASS'), rows[-1]['commit'])
        picked = [r['dir'] for r in rows if r['commit'].startswith(commit) and r['outcome'] == 'PASS']
        if not picked:
            sys.exit(f'apdiff: {item!r}: no PASS run on build {commit}')
        out += picked
    return out


def row_of(folder, runs):
    name = os.path.basename(folder)
    for r in runs:
        if os.path.basename(r['dir']) == name:
            return r
    return None


def t0_of(folder):
    tr = os.path.join(folder, 'transcript.txt')
    if not os.path.isfile(tr):
        return None
    m = re.search(r'^(\d\d):(\d\d):(\d\d) \+\s+0 ', open(tr, encoding='utf-8', errors='replace').read(), re.M)
    return clock(*m.groups()) if m else None


# ---------------------------------------------------------------- events
def read_events(log):
    """(clock s, raw text after 'event autopilot') for every autopilot event, in log order; + the log's other lines
    needed here: (clock, mode name) of every `autopilot start mode=`, outcome markers."""
    ev, starts, outcome = [], [], None
    for line in open(log, encoding='latin-1'):
        m = TS.match(line)
        if m:
            t = clock(*m.groups()[:4])
            rest = m.group(5).rstrip()
            ev.append((t, rest))
            s = re.match(r'start mode=(\w+)', rest)
            if s:
                starts.append((t, s.group(1).lower()))
            continue
        if 'mission-message type=SUCCESS' in line:
            outcome = 'PASS'
        elif outcome is None and ('mission-message type=FAILURE' in line or 'player-death' in line):
            outcome = 'FAIL'
    return ev, starts, outcome


def normalise(rest):
    s = KV.sub(lambda m: f'{m.group(1)}={m.group(2)}' if m.group(1) in KEEP else f'{m.group(1)}=#', rest)
    s = HEX.sub('#', s)
    s = NUM.sub('#', s)
    return re.sub(r'\s+', ' ', s).strip()


def kind_of(norm):
    tok = norm.split(' ')
    kind = tok[0].partition('=')[0]
    if len(tok) > 1:
        t1 = tok[1].rstrip(':')
        if '=' in t1:
            kind += ' ' + t1.partition('=')[0]
        elif WORD.match(t1):
            kind += ' ' + t1
    return kind


def is_fight(kind):
    k = kind.split(' ')[0]
    return any(k == p or k.startswith(p + '-') for p in FIGHT)


def collapse(items, on=True):
    """[(t, norm)] -> [(t, norm, n)]: consecutive identical lines become one with a count."""
    out = []
    for t, s in items:
        if on and out and out[-1][1] == s:
            out[-1] = (out[-1][0], s, out[-1][2] + 1)
        else:
            out.append((t, s, 1))
    return out


def kind_counts(seq):
    c = {}
    for _, s, n in seq:
        k = kind_of(s)
        c[k] = c.get(k, 0) + 1
    return c


# ---------------------------------------------------------------- motion
def read_motion(path):
    if not os.path.isfile(path):
        return []
    out = []
    with open(path, encoding='utf-8', errors='replace') as f:
        for r in csv.DictReader(f, delimiter='\t'):
            try:
                out.append(dict(t=int(r['ms']) / 1000.0, mode=int(r['mode']), v=float(r['speed']),
                                p=(float(r['x']), float(r['y']), float(r['z'])), eng=int(r['engine']),
                                sys=r['system']))
            except (ValueError, KeyError):
                pass
    return out


def motion_offset(rows, starts, log):
    """motion seconds (uptime) - log clock seconds. The motion's mode changes are matched to the log's autopilot
    start lines by mode-name sequence (difflib), the offset = the median of the matched pairs' differences."""
    changes = []
    for i, r in enumerate(rows):
        if i == 0 or r['mode'] != rows[i - 1]['mode']:
            if 0 <= r['mode'] < len(MODES):
                changes.append((r['t'], MODES[r['mode']]))
    a = [m for _, m in changes]
    b = [m for _, m in starts]
    diffs = []
    sm = difflib.SequenceMatcher(None, a, b, autojunk=False)
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag == 'equal':
            for i, j in zip(range(i1, i2), range(j1, j2)):
                diffs.append(changes[i][0] - starts[j][0])
    if diffs:
        med = statistics.median(diffs)
        near = [d for d in diffs if abs(d - med) < 1.0]
        return statistics.median(near) if near else med, len(near)
    # fallback: the first system change (routecmp.py)
    for i in range(1, len(rows)):
        if rows[i]['sys'] != rows[i - 1]['sys']:
            for line in open(log, encoding='latin-1'):
                if 'system=' + rows[i]['sys'] in line:
                    m = re.match(r'^\[(\d\d):(\d\d):(\d\d)\.(\d+)\]', line)
                    if m:
                        return rows[i]['t'] - clock(*m.groups()), 1
            break
    return None, 0


def seg_metrics(rows, a, b):
    """rows with a <= t < b (motion time): dist (gaps > 2 s skipped), mean / max speed, s cruising, s under 100 m/s."""
    seg = [r for r in rows if a <= r['t'] < b]
    if len(seg) < 2:
        return dict(dur=b - a, dist=0.0, mean=0.0, max=0.0, cruise=0.0, slow=0.0, rows=len(seg))
    dist = cruise = slow = 0.0
    for i in range(1, len(seg)):
        dt = seg[i]['t'] - seg[i - 1]['t']
        if dt > 2.0:
            continue
        p, q = seg[i - 1]['p'], seg[i]['p']
        dist += ((p[0] - q[0]) ** 2 + (p[1] - q[1]) ** 2 + (p[2] - q[2]) ** 2) ** 0.5
        if seg[i]['eng'] == 2:
            cruise += dt
        if seg[i]['v'] < 100:
            slow += dt
    return dict(dur=b - a, dist=dist, mean=statistics.fmean(r['v'] for r in seg), max=max(r['v'] for r in seg),
                cruise=cruise, slow=slow, rows=len(seg))


# ---------------------------------------------------------------- checkpoints
def checkpoints(folder, t0, row):
    """[(cp name, at s from t0)] in order: splits.tsv, phases.tsv, else computed from the log by splits.py."""
    name = os.path.basename(folder)
    hist = os.path.join(LEGS, 'splits.tsv')
    if os.path.isfile(hist):
        with open(hist, encoding='utf-8', errors='replace') as f:
            rows = [r for r in csv.DictReader(f, delimiter='\t') if r['dir'] == name and r['at']]
        if rows:
            rows.sort(key=lambda r: int(r['n']))
            return [(r['cp'], float(r['at'])) for r in rows], 'splits.tsv'
    ph = os.path.join(folder, 'phases.tsv')
    if os.path.isfile(ph):
        with open(ph, encoding='utf-8', errors='replace') as f:
            cps = [(r['end'], float(r['at'].replace(',', ''))) for r in csv.DictReader(f, delimiter='\t')
                   if r['phase'] == 'cp']
        if cps:
            return cps, 'phases.tsv'
    try:
        sp = splits_module()
        legs = sp.checkpoints()
        leg = sp.leg_of(name.split('_', 2)[2]) if name.count('_') >= 2 else None
        if leg and leg in legs and t0 is not None:
            ev = sp.log_events(os.path.join(folder, 'FLController.log'))
            full = row['full'] if row else True
            at = sp.our_splits(legs[leg], ev, t0, full, row['startcp'] if row else None)
            return [(cp['cp'], a) for cp, a in zip(legs[leg], at) if a is not None], 'log + checkpoints.tsv'
    except Exception as e:
        print(f'(checkpoints from the log failed: {e})', file=sys.stderr)
    return [], 'none'


# ---------------------------------------------------------------- a run
class Run:
    def __init__(self, folder, runs, collapse_on=True):
        self.folder = folder
        self.name = os.path.basename(folder)
        self.row = row_of(folder, runs)
        self.t0 = t0_of(folder)
        log = os.path.join(folder, 'FLController.log')
        ev, starts, outcome = read_events(log)
        self.outcome = (self.row or {}).get('outcome') or outcome or '?'
        self.commit = (self.row or {}).get('commit', '?')
        self.cps, self.cp_source = checkpoints(folder, self.t0, self.row)
        self.total = float(self.row['secs']) if self.row and self.row.get('secs') else (self.cps[-1][1] if self.cps else None)
        if self.t0 is None:  # no transcript: the first autopilot event is the start
            self.t0 = ev[0][0] if ev else 0.0
        self.events = [(t, normalise(r)) for t, r in ev if t >= self.t0 - 5]
        self.seq = collapse(self.events, collapse_on)
        self.kinds = kind_counts(self.seq)
        self.motion = read_motion(os.path.join(folder, 'FLMotion.tsv'))
        self.off, self.anchors = motion_offset(self.motion, starts, log) if self.motion else (None, 0)
        self._bounds = {cp: (a, b) for cp, a, b in self.bounds()}
        self.seg = {}
        for cp, a, b in self.bounds():
            ev_ = self.seg_events(a, b)
            self.seg[cp] = dict(motion=self.seg_motion(a, b), events=[s for _, s, _ in ev_],
                                fight=any(is_fight(kind_of(s)) for _, s, _ in ev_))

    def bounds(self):
        """[(cp, a, b)] in log clock seconds."""
        out, prev = [], self.t0 - 5
        for cp, at in self.cps:
            out.append((cp, prev, self.t0 + at))
            prev = self.t0 + at
        return out

    def seg_events(self, a, b):
        return [(t, s, n) for t, s, n in self.seq if a <= t < b]

    def seg_motion(self, a, b):
        if self.off is None:
            return None
        return seg_metrics(self.motion, a + self.off, b + self.off)

    def head(self, tag):
        return (f'{tag:8} {self.name}  build {self.commit}  {self.outcome} {fmt(self.total, 0)} s  events {len(self.events)} '
                f'({len(self.seq)} collapsed)  checkpoints {len(self.cps)} ({self.cp_source})  motion rows {len(self.motion)}'
                f' (offset from {self.anchors} anchors)')


# ---------------------------------------------------------------- the comparison
def fmt(x, nd=1):
    return '-' if x is None else f'{x:.{nd}f}'


def rng(lo, hi, nd=0):
    return f'{lo:.{nd}f}' if abs(hi - lo) < 0.5 * 10 ** -nd else f'{lo:.{nd}f}..{hi:.{nd}f}'


def hhmmss(t):
    t = int(t)
    return f'{t // 3600 % 24:02d}:{t // 60 % 60:02d}:{t % 60:02d}'


def outside(y, lo, hi, slack):
    return y < lo - slack or y > hi + slack


def judge(base, run, args, say):
    """One run against the baseline envelope. -> (seg_flags [(cp, [metrics])], kind_flags [kinds], fight_flags, ratio)."""
    say()
    say(run.head('run'))
    # (a) the sequence: first divergence from the most similar baseline run
    sb = [s for _, s, _ in run.seq]
    best, ratio = None, -1.0
    for A in base:
        sm = difflib.SequenceMatcher(None, [s for _, s, _ in A.seq], sb, autojunk=False)
        r = sm.ratio()
        if r > ratio:
            best, ratio, bsm = A, r, sm
    say(f'EVENT SEQUENCE: similarity {ratio:.3f} to {best.name} (difflib ratio of the collapsed, normalised sequences)')
    for tag, i1, i2, j1, j2 in bsm.get_opcodes():
        if tag == 'equal':
            continue
        say(f'  first divergence at baseline event {i1 + 1} / run event {j1 + 1} ({tag}):')
        for k in range(max(0, i1 - args.context), i1):
            t, s, n = best.seq[k]
            say(f'    = {hhmmss(t)} {s}' + (f'  (x{n})' if n > 1 else ''))
        for k in range(i1, min(i2, i1 + 8)):
            t, s, n = best.seq[k]
            say(f'    - {hhmmss(t)} {s}' + (f'  (x{n})' if n > 1 else ''))
        if i2 - i1 > 8:
            say(f'    - ... {i2 - i1 - 8} more baseline lines')
        for k in range(j1, min(j2, j1 + 8)):
            t, s, n = run.seq[k]
            say(f'    + {hhmmss(t)} {s}' + (f'  (x{n})' if n > 1 else ''))
        if j2 - j1 > 8:
            say(f'    + ... {j2 - j1 - 8} more run lines')
        break
    else:
        say('  the sequences are identical')

    # (a) kinds against the envelope
    allk = set(run.kinds)
    for A in base:
        allk |= set(A.kinds)
    kinds = sorted(allk, key=lambda k: -max([run.kinds.get(k, 0)] + [A.kinds.get(k, 0) for A in base]))
    kind_flags, fight_flags, only_run, only_base, noisy_kinds = [], [], [], [], []
    say(f'EVENT KINDS (collapsed counts, baseline envelope | run; FLAG when outside by more than max({args.kind_abs}, '
        f'{args.kind_rel:g} x max); f = fight kind{"" if args.all_kinds else ", not in the verdict"}):')
    say(f'  {"kind":34} {"baseline":>10} {"run":>5}  flag')
    for k in kinds:
        bs = [A.kinds.get(k, 0) for A in base]
        lo, hi, y = min(bs), max(bs), run.kinds.get(k, 0)
        f = is_fight(k)
        slack = max(args.kind_abs, args.kind_rel * max(hi, y))
        noisy = not args.strict and hi - lo > slack   # the baseline runs disagree beyond the slack: no verdict power
        flag = not noisy and outside(y, lo, hi, slack)
        if flag:
            (fight_flags if f else kind_flags).append(k)
        if noisy:
            noisy_kinds.append(k)
        if y and not hi:
            only_run.append(k)
        if hi and not y:
            only_base.append(k)
        say(f'  {k:34} {rng(lo, hi):>10} {y:5d}  {"f" if f else " "}{("flag" if f else "FLAG") if flag else ("~" if noisy else "")}')
    say(f'  kinds the baseline itself spreads beyond the slack (~, not judged): {", ".join(noisy_kinds) or "none"}')
    say(f'  kinds only in the run:      {", ".join(only_run) or "none"}')
    say(f'  kinds only in the baseline: {", ".join(only_base) or "none"}')

    # (b) motion per segment against the envelope
    seg_flags, seg_n = [], 0
    pat = re.compile(args.segments) if args.segments else None
    say(f'SEGMENTS (motion per checkpoint segment, baseline envelope | run; FLAG when outside by more than '
        f'max({args.tol:g} x max, floor); ~ = the baseline spreads beyond that itself (not judged); * = fight events in the '
        f'segment; evsim = the segment\'s best event-sequence similarity):')
    say(f'  {"segment":17} {"dur":>17} {"dist m":>19} {"mean m/s":>13} {"max":>13} {"cruise s":>15} {"slow s":>15} {"evsim":>5}  flags')
    cps = [cp for cp, _ in base[0].cps]
    for A in base[1:]:
        cps += [cp for cp, _ in A.cps if cp not in cps]
    for cp in cps:
        if pat and not pat.search(cp):
            continue
        if cp not in run.seg:
            say(f'  {cp:17} (not reached in the run)')
            continue
        bsegs = [A.seg[cp] for A in base if cp in A.seg]
        rs = run.seg[cp]
        fight = rs['fight'] or any(s['fight'] for s in bsegs)
        sim = max(difflib.SequenceMatcher(None, s['events'], rs['events'], autojunk=False).ratio()
                  if (s['events'] or rs['events']) else 1.0 for s in bsegs)
        seg_n += 1
        label = cp + ('*' if fight else '')
        if rs['motion'] is None or any(s['motion'] is None for s in bsegs):
            d = [s['motion']['dur'] if s['motion'] else 0 for s in bsegs]
            say(f'  {label:17} (no motion trace / alignment){"":>88} {sim:5.2f}')
            continue
        bad, noisy, cells = [], [], []
        for m in METRICS:
            vals = [s['motion'][m] for s in bsegs]
            lo, hi, y = min(vals), max(vals), rs['motion'][m]
            slack = max(args.tol * max(abs(hi), abs(y)), FLOORS[m])
            if not args.strict and hi - lo > slack:   # the baseline runs disagree beyond the tolerance: no verdict power
                noisy.append(m)
            elif outside(y, lo, hi, slack):
                bad.append(m)
            nd = 1 if m in ('dur', 'cruise', 'slow') else 0
            cells.append(f'{rng(lo, hi, nd)}|{y:.{nd}f}')
        if bad:
            seg_flags.append((cp, bad))
        w = (17, 19, 13, 13, 15, 15)
        say(f'  {label:17} ' + ' '.join(c.rjust(n) for c, n in zip(cells, w)) + f' {sim:5.2f}  '
            + ('FLAG(' + ','.join(bad) + ')' if bad else '') + (' ~' + ','.join(noisy) if noisy else ''))
    for cp in run.seg:
        if cp not in cps and (not pat or pat.search(cp)):
            say(f'  {cp:17} (not reached in the baseline)')
    return seg_flags, seg_n, kind_flags, fight_flags, ratio


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0], add_help=True)
    ap.add_argument('-Baseline', '--baseline', required=True, help='leg folder(s) or legs.tsv label (the reference; comma-separated)')
    ap.add_argument('-Run', '--run', required=True, help='leg folder(s) or legs.tsv label (under test; comma-separated)')
    ap.add_argument('--tol', type=float, default=0.02, help='relative tolerance per motion metric (default 0.02)')
    ap.add_argument('--kind-abs', type=int, default=4, help='event-kind count slack, absolute (default 4)')
    ap.add_argument('--kind-rel', type=float, default=0.5, help='event-kind count slack, relative to the larger count (default 0.5)')
    ap.add_argument('--segments', default='', help='regex on the checkpoint name: judge these segments only')
    ap.add_argument('--all-kinds', action='store_true', help='fight kinds count in the verdict too')
    ap.add_argument('--strict', action='store_true', help='judge every metric / kind, also where the baseline runs themselves spread beyond the tolerance (~)')
    ap.add_argument('--context', type=int, default=5, help='context lines at the first divergence (default 5)')
    ap.add_argument('--no-collapse', action='store_true', help='do not collapse consecutive identical events')
    ap.add_argument('--quiet', action='store_true', help='print the verdict line(s) only')
    args = ap.parse_args(argv)

    def say(s=''):
        if not args.quiet:
            print(s)

    runs = archived()
    base = [Run(f, runs, not args.no_collapse) for f in resolve(args.baseline, runs)]
    under = [Run(f, runs, not args.no_collapse) for f in resolve(args.run, runs)]
    for A in base:
        say(A.head('baseline'))
    worst = 0
    for R in under:
        seg_flags, seg_n, kind_flags, fight_flags, ratio = judge(base, R, args, say)
        bad = bool(seg_flags or kind_flags or (args.all_kinds and fight_flags))
        worst = max(worst, int(bad))
        builds = sorted({A.commit for A in base})
        same = builds == [R.commit] and R.commit != '?'
        line = (f'VERDICT {"DIFFERENT" if bad else "SAME"} {R.name}: segments flagged {len(seg_flags)}/{seg_n}'
                + (' (' + '; '.join(f'{cp}:{",".join(b)}' for cp, b in seg_flags) + ')' if seg_flags else '')
                + f', decision kinds flagged {len(kind_flags)}' + (' (' + ', '.join(kind_flags) + ')' if kind_flags else '')
                + f', fight kinds flagged {len(fight_flags)}' + (' (' + ', '.join(fight_flags) + ')' if fight_flags else '')
                + f', sequence similarity {ratio:.2f}, outcome {"/".join(A.outcome for A in base)} -> {R.outcome}, '
                + f'total {"/".join(fmt(A.total, 0) for A in base)} -> {fmt(R.total, 0)} s, '
                + ('same build ' if same else 'builds ') + f'{"/".join(builds)} -> {R.commit}')
        print(('' if args.quiet else '\n') + line)
    sys.exit(worst)


if __name__ == '__main__':
    main()
