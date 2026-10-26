"""Ordnance use at static-target holds, per target, from leg folders' FLController.log (the user, 24 Oct 2026: "some
generator runs don't quite make full use of torpedoes and mines, leave a few seconds on the table").

Per target (from its first static-hold event to its target-destroyed): seconds, torpedoes launched against the launcher's
2 s refire (possible = the hold's seconds after the first launch / 2 + 1), the longest gap between torpedoes, salvo checks
with ready=0, mines dropped, missiles launched, the leave_on_kill prediction (doomed -> kill).
Usage: holdstats.py <leg folder> [<leg folder> ...]   (or a glob's expansion; a label regex with --label)
"""
import os
import re
import sys

FL = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LEGS = os.path.join(FL, 'research', 'experience', 'legs')
TS = re.compile(r'^\[(\d\d):(\d\d):(\d\d)\.(\d+)\] event autopilot (\S+) ?(.*)$')
REFIRE = 2.0   # torpedo01_mark01 refire (docs\kb\generated\guns.tsv); the report is for the M12 / M13 loadout


def secs(m):
    return int(m.group(1)) * 3600 + int(m.group(2)) * 60 + int(m.group(3)) + int(m.group(4)) / 1000.0


def holds(log):
    """Each target's hold, ended by its own kill (a doomed one is left before it: leave_on_kill)."""
    out, by, cur = [], {}, None
    for line in open(log, encoding='latin-1'):
        m = TS.match(line)
        if not m:
            continue
        t, kind, rest = secs(m), m.group(5), m.group(6)
        tid = re.search(r'target=(\d+)', rest)
        if kind == 'static-hold' and tid and tid.group(1) not in by:
            cur = dict(id=tid.group(1), t0=t, torp=[], ready0=0, checks=0, mines=0, missiles=0, doomed=None, end=None)
            by[cur['id']] = cur
            out.append(cur)
        if kind == 'target-destroyed':
            k = re.search(r'id=(\d+)', rest)
            if k and k.group(1) in by and by[k.group(1)]['end'] is None:
                by[k.group(1)]['end'] = t
            continue
        h = by.get(tid.group(1)) if tid else cur
        if not h or h['end'] is not None:
            continue
        if kind == 'torpedo-launch':
            h['torp'].append(t)
        elif kind == 'missile-launch':
            h['missiles'] += 1
        elif kind == 'salvo-check':
            h['checks'] += 1
            h['ready0'] += ' ready=0 ' in rest
        elif kind == 'doomed':
            h['doomed'] = t
        elif kind == 'hold-mine':
            h['mines'] += 1
    return out


def main(args):
    if args and args[0] == '--label':
        rx = re.compile(args[1])
        args = [os.path.join(LEGS, d) for d in sorted(os.listdir(LEGS)) if rx.search(d)]
    print(f'{"run":44} {"#":>2} {"hold s":>6} {"torp":>4} {"poss":>4} {"maxgap":>6} {"rdy0":>4} {"mines":>5} {"msl":>4} {"doom->kill":>10}')
    for d in args:
        log = os.path.join(d, 'FLController.log')
        if not os.path.isfile(log):
            continue
        for n, h in enumerate(holds(log), 1):
            if h['end'] is None:
                continue
            dur = h['end'] - h['t0']
            tp = h['torp']
            poss = int((h['end'] - tp[0]) / REFIRE) + 1 if tp else 0
            gaps = [b - a for a, b in zip(tp, tp[1:])] + ([h['end'] - tp[-1]] if tp else [])
            dk = f'{h["end"] - h["doomed"]:.1f}' if h['doomed'] else '-'
            print(f'{os.path.basename(d)[:44]:44} {n:2} {dur:6.1f} {len(tp):4} {poss:4} {max(gaps) if gaps else 0:6.1f} '
                  f'{h["ready0"]:4} {h["mines"]:5} {h["missiles"]:4} {dk:>10}')


if __name__ == '__main__':
    main(sys.argv[1:])
