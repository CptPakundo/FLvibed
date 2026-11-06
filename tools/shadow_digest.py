"""shadow_digest.py - the A37 shadow's verdicts over a set of leg runs (WORK_QUEUE Q6 S5, 30 Oct 2026): counts the
`situation shadow agree|differ <rule>` lines (modules\\autopilot\\src\\situation_snap.cpp, setting situation_shadow) per
rule and lists every differ with its leg, so a measurement gate (tools\\flgate.ps1 -ApSet 'situation_shadow 1') is read
from one digest, not 14 logs (Working method 23). Per leg folder the first source that has shadow lines counts:
FLController.log, else *.events.txt, else transcript.txt (the same event can sit in several - counted once).
Fails closed: a selected leg folder with no readable source is UNREAD; a gate leg with no shadow line at all is listed
(the switch may not have reached it). Exit 0 = every leg read and 0 differ; 1 otherwise.
--baseline (A37 S7, 30 Oct 2026: the arbiter's agree count is circular - both sides read the same memory - and hid a
behaviour change) compares what two gates DID at the same story points: per leg in both, the executed rules per
objective step (the latest `event objective ... obj=<id>` before each decision), consecutive repeats collapsed. Executed
= the rule a shadow line names (the old path's, situation_snap.cpp Shadow) or an arbiter line names (agree: the old and
the proposal's are one; differ: the proposal's is executed, autopilot_objective.cpp ArbiterExecute); a shadow hold
re-check (`held:<why>`) is no decision and is left out. Only steps present in both runs count (a run that died earlier
has fewer); one-sided steps are listed apart. Exit 0 = no step differs, 1 = a step differs, 2 = missing data (a leg in
both gates without a log or without a decision line, a leg still running, no leg in both): fail closed.

  python -I tools\\shadow_digest.py --gate <tag>      the legs labelled <leg>-gate-<tag> (research\\experience\\legs)
  python -I tools\\shadow_digest.py --gate <tag> --baseline <tag2>   the decisions of gate <tag> vs gate <tag2>
  python -I tools\\shadow_digest.py <label regex>     leg folders whose label matches
  python -I tools\\shadow_digest.py <folder>          one run folder (e.g. research\\experience\\runs\\a37s4_...)
  python -I tools\\shadow_digest.py --selftest
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LEGS = os.path.join(ROOT, 'research', 'experience', 'legs')
# (S7, 30 Oct 2026: the arbiter's lines too - `situation arbiter agree|differ <rule> ... plan=agree|<diff>` and
# `side-effect <kind> ... refused`; docs\HARNESS.md. A plan difference and a refusal are counted, not failed:
# plan= is S10's measure of the rules' own plans, a refusal is the game-first gate working)
LINE = re.compile(r'situation (shadow|arbiter) (agree|differ) (\S+)(.*)$')
PLAN = re.compile(r'\bplan=(\S+)')
REFUSED = re.compile(r'side-effect (\S+) (.*\brefused\b.*)$')
FOLDER = re.compile(r'^\d{4}-\d\d-\d\d_\d{4,6}_(.+)_i(\d+)$')


def shadow_lines(text):
    """[(verdict, rule, rest)]: verdict agree / differ (a shadow rule as is, an arbiter's as 'arbiter:<rule>'), 'plan'
    for an arbiter decision whose plan= is not agree, 'refused' for a refused side effect."""
    out = []
    for l in text.splitlines():
        m = LINE.search(l)
        if m:
            rule = m.group(3) if m.group(1) == 'shadow' else 'arbiter:' + m.group(3)
            out.append((m.group(2), rule, m.group(4).strip()))
            p = PLAN.search(m.group(4)) if m.group(1) == 'arbiter' else None
            if p and p.group(1) != 'agree':
                out.append(('plan', rule, m.group(4).strip()))
            continue
        r = REFUSED.search(l)
        if r:
            out.append(('refused', 'side-effect:' + r.group(1), r.group(2).strip()))
    return out


def read_folder(path):
    """(source name, [(verdict, rule, rest)]) from the first source with shadow lines; (None, None) when unreadable."""
    names = sorted(os.listdir(path))
    # (a run folder's *.events.txt are one game each: they count together)
    order = [['FLController.log'], [n for n in names if n.endswith('.events.txt')], ['transcript.txt']]
    seen_any = False
    for group in order:
        got = []
        for n in group:
            p = os.path.join(path, n)
            if not os.path.isfile(p):
                continue
            try:
                with open(p, encoding='utf-8', errors='replace') as f:
                    text = f.read()
            except OSError:
                continue
            seen_any = True
            got += shadow_lines(text)
        if got:
            return ','.join(group), got
    return ('(none)', []) if seen_any else (None, None)


def digest(folders):
    out, per_rule, differs, unread, silent, running, extra = [], {}, [], [], [], [], {}
    for name, path in folders:
        # a leg still flying: flleg copies the game's logs at the leg's end, so only its transcript exists yet
        if FOLDER.match(name) and os.listdir(path) == ['transcript.txt']:
            running.append(name)
            continue
        src, got = read_folder(path)
        if got is None:
            unread.append(name)
            continue
        if not got:
            silent.append(name)
        for verdict, rule, rest in got:
            if verdict in ('plan', 'refused'):
                extra.setdefault(verdict, {}).setdefault(rule, []).append(f'{name}: {rest[:160]}')
                continue
            a, d = per_rule.get(rule, (0, 0))
            per_rule[rule] = (a + (verdict == 'agree'), d + (verdict == 'differ'))
            if verdict == 'differ':
                differs.append(f'  DIFFER {name}: {rule} {rest[:200]}')
    for rule, (a, d) in sorted(per_rule.items()):
        out.append(f'  {rule:<18} agree {a:>4}  differ {d:>3}')
    out += differs
    for verdict, label in (('plan', 'plan differs (arbiter plan=, counted, not failed)'),
                           ('refused', 'side effects refused (the game-first gate)')):
        for rule, ex in sorted(extra.get(verdict, {}).items()):
            out.append(f'  {label}: {rule} x{len(ex)}, e.g. {ex[0]}')
    if silent:
        out.append('  no shadow line: ' + ', '.join(silent))
    if unread:
        out.append('  UNREAD (no log / events / transcript): ' + ', '.join(unread))
    if running:
        out.append('  still running (no logs copied yet): ' + ', '.join(running))
    na = sum(a for a, _ in per_rule.values())
    nd = sum(d for _, d in per_rule.values())
    out.append(f'SHADOW {na} agree / {nd} differ over {len(folders)} leg folder(s), {len(per_rule)} rule(s), '
               f'{len(silent)} silent, {len(unread)} unread, {len(running)} running')
    return out, (nd > 0 or bool(unread) or bool(running) or not folders)


def leg_folders(label_rx):
    rx = re.compile(label_rx)
    res = []
    for f in sorted(os.listdir(LEGS)):
        m = FOLDER.match(f)
        if m and rx.search(m.group(1)) and os.path.isdir(os.path.join(LEGS, f)):
            res.append((f, os.path.join(LEGS, f)))
    return res


# ---------------------------------------------------------------- --baseline: the decision sequences of two gates
# (`event objective count=1 title=... text=... obj=24015 "..."` and `event objective-client seq=2 obj=24015 "..."`)
OBJ = re.compile(r'event objective(?:-client)? (?:count|seq)=\d+ .*?\bobj=(\d+)')
STAMP = re.compile(r'^\[([^\]]+)\]')


def decisions(text):
    """(objs, [(i, rule, line)]) in log order: objs = the objective ids as entered (consecutive repeats collapsed), i =
    the index in objs of the latest one before the decision (-1 before the first); each executed decision (a shadow
    line's rule is the old path's, the executed one; an arbiter line's is the executed one); shadow hold re-checks
    (`held:<why>`) are no decisions and left out."""
    objs, out = [], []
    for l in text.splitlines():
        o = OBJ.search(l)
        if o:
            if not objs or objs[-1] != o.group(1):
                objs.append(o.group(1))
            continue
        m = LINE.search(l)
        if not m or m.group(3).startswith('held'):
            continue
        s = STAMP.match(l)
        out.append((len(objs) - 1, m.group(3), (s.group(1) + ' ' if s else '') + l[m.start():].strip()))
    return objs, out


def recurring(objs):
    """the ids entered at more than one point: the game's placeholders (obj=0 / 1 blank, 21660 'Awaiting Mission
    Objective' between steps) or a step re-entered after a reload"""
    seen, rec = set(), set()
    for o in objs:
        (rec if o in seen else seen).add(o)
    return rec


def keyed(objs, dec, rec):
    """[(step, rule, line)]: a step = its objective id; a recurring id (rec: over BOTH runs, so the keys match) is
    keyed by the last non-recurring id before it, `24125>21660` = the placeholder after step 24125"""
    out = []
    for i, rule, line in dec:
        if i < 0:
            out.append(('-', rule, line))
            continue
        k = objs[i]
        if k in rec:
            anchor = next((objs[j] for j in range(i - 1, -1, -1) if objs[j] not in rec), '-')
            k = f'{anchor}>{k}'
        out.append((k, rule, line))
    return out


def by_step(dec):
    """{step: ([distinct rules, consecutive repeats collapsed], {rule: first line})}"""
    res = {}
    for step, rule, line in dec:
        seq, first = res.setdefault(step, ([], {}))
        if not seq or seq[-1] != rule:
            seq.append(rule)
        first.setdefault(rule, line)
    return res


def gate_legs(tag):
    """{leg: (folder, path)}; a leg run twice in a gate: the latest run (the earlier ones named in 'reruns')"""
    legs, reruns = {}, []
    for f, p in leg_folders(f'-gate-{re.escape(tag)}$'):
        leg = FOLDER.match(f).group(1)[:-len(f'-gate-{tag}')]
        if leg in legs:
            reruns.append(legs[leg][0])
        legs[leg] = (f, p)
    return legs, reruns


def leg_decisions(path):
    """decisions() of the leg's FLController.log; None = no data (no log, unreadable, still running, no decision line)"""
    if os.listdir(path) == ['transcript.txt']:
        return None, 'still running'
    try:
        with open(os.path.join(path, 'FLController.log'), encoding='utf-8', errors='replace') as f:
            objs, dec = decisions(f.read())
    except OSError:
        return None, 'no FLController.log'
    if not dec:
        return None, 'no shadow / arbiter decision line'
    return (objs, dec), ''


def compare(tag, base, gate, baseline):
    """gate / baseline: {leg: (folder, path)}. (lines, exit code 0 / 1 / 2)"""
    out, ndiff, nlegs, missing, one = [], 0, 0, [], []
    for leg in sorted(set(gate) | set(baseline)):
        if leg not in gate or leg not in baseline:
            out.append(f'  leg {leg} only in ' + (tag if leg in gate else base) + ' (not compared)')
            continue
        g, why_g = leg_decisions(gate[leg][1])
        b, why_b = leg_decisions(baseline[leg][1])
        if g is None or b is None:
            missing += [f'{gate[leg][0]}: {why_g}'] if g is None else []
            missing += [f'{baseline[leg][0]}: {why_b}'] if b is None else []
            continue
        nlegs += 1
        rec = recurring(g[0]) | recurring(b[0])
        g, b = by_step(keyed(*g, rec)), by_step(keyed(*b, rec))
        for step in sorted(set(g) | set(b), key=lambda s: (s == '-', s)):
            if step not in g or step not in b:
                side, seq = (tag, g[step][0]) if step in g else (base, b[step][0])
                one.append(f'  one-sided {leg} obj={step}: only in {side}: {" > ".join(seq)}')
                continue
            (gs, gf), (bs, _) = g[step], b[step]
            if set(gs) == set(bs):
                continue
            ndiff += 1
            bonly = list(dict.fromkeys(r for r in bs if r not in gs))
            gonly = list(dict.fromkeys(r for r in gs if r not in bs))
            out.append(f'  DIFFER {leg} obj={step}: {base}-only [{", ".join(bonly)}] {tag}-only [{", ".join(gonly)}]'
                       f'  ({base}: {" > ".join(bs)} | {tag}: {" > ".join(gs)})')
            for r in gonly:
                out.append(f'    first {r}: {gf[r][:200]}')
    out += one
    if missing:
        out.append('  MISSING DATA (fail closed): ' + '; '.join(missing))
    out.append(f'DECISIONS {tag} vs {base}: {nlegs} legs, {ndiff} steps differ'
               f' ({len(one)} one-sided steps, {len(missing)} leg run(s) without data)')
    return out, (2 if missing or not nlegs else 1 if ndiff else 0)


def selftest():
    import tempfile
    ok = True
    # --baseline: the decision stream and its comparison
    sh = ('[11:40:04.998] event objective count=1 title=24000 "M" text=24005 "OBJECTIVE: x" obj=24015 "Meet" nav=1,2,3\n'
          '[11:40:29.684] event situation shadow agree ship-target old=follow,14,follow,keep=0 new=ship-target text=24015\n'
          '[11:40:30.000] event situation shadow agree ship-target old=follow,14,follow,keep=0 new=ship-target\n'
          '[11:40:31.000] event situation shadow agree held:jump old=hold,0,rest,keep=1 new=held\n'
          '[11:40:37.232] event objective-client seq=5 obj=24020 "Fly" steps=1\n'
          '[11:40:37.686] event situation shadow differ jump old=travel,7,jump,keep=0 new=lane\n'
          '[11:40:38.000] event objective count=1 title=24000 "M" text=24005 "x" obj=24030 "Dock"\n'
          '[11:40:39.000] event situation shadow agree dock old=dock,5,dock,keep=0 new=dock\n')
    objs, d = decisions(sh)
    ok &= objs == ['24015', '24020', '24030']                                      # -client repeats the same id
    ok &= [(objs[i], r) for i, r, _ in d] == [('24015', 'ship-target'), ('24015', 'ship-target'), ('24020', 'jump'),
                                              ('24030', 'dock')]
    ok &= d[0][2].startswith('11:40:29.684 situation shadow agree ship-target')
    ar = ('[12:58:00.000] event objective count=1 title=24000 "M" text=24005 "x" obj=24015 "Meet"\n'
          '[12:58:21.766] event situation arbiter agree ship-target mode=follow target=14 old=ship-target,follow plan=agree\n'
          '[12:58:22.000] event situation arbiter agree pre-stage-next mode=travel old=pre-stage-next,travel plan=agree\n'
          '[12:58:23.000] event situation arbiter agree ship-target mode=follow old=ship-target,follow plan=agree\n'
          '[12:58:24.000] event objective-client seq=5 obj=24020 "Fly" steps=1\n'
          '[12:58:25.000] event situation arbiter differ lane mode=travel old=jump,travel plan=kind ok=1\n'
          '[12:58:26.000] event objective count=1 title=24000 "M" text=24005 "x" obj=24040 "Later"\n'
          '[12:58:27.000] event situation arbiter agree rest mode=hold old=rest,hold plan=agree\n')
    bs = by_step(keyed(*decisions(ar), set()))
    ok &= bs['24015'][0] == ['ship-target', 'pre-stage-next', 'ship-target']    # a repeat collapses only when consecutive
    ok &= bs['24015'][1]['pre-stage-next'].startswith('12:58:22.000 situation arbiter agree pre-stage-next')
    ok &= bs['24020'][0] == ['lane']                                               # differ: the proposal's is executed
    # a placeholder between steps (S7: pre-stage-next after a jump step, hidden when every 21660 was one key)
    ph = ('event objective count=1 title=1 "M" text=2 "x" obj=24125 "Take"\nevent situation shadow agree jump old=x\n'
          'event objective count=1 title=1 "M" text=2 "x" obj=1 " "\n'
          'event objective count=1 title=1 "M" text=2 "x" obj=21660 "Awaiting"\nevent situation shadow agree {a} old=x\n'
          'event objective count=1 title=1 "M" text=2 "x" obj=24050 "Fly"\nevent situation shadow agree arrive old=x\n'
          'event objective count=1 title=1 "M" text=2 "x" obj=1 " "\n'
          'event objective count=1 title=1 "M" text=2 "x" obj=21660 "Awaiting"\nevent situation shadow agree {b} old=x\n')
    po, pd = decisions(ph.format(a='ship-target', b='pre-stage-next'))
    ok &= recurring(po) == {'1', '21660'}
    ok &= [s for s, _, _ in keyed(po, pd, recurring(po))] == ['24125', '24125>21660', '24050', '24050>21660']
    with tempfile.TemporaryDirectory() as d2:
        def leg(name, text):
            p = os.path.join(d2, name)
            os.mkdir(p)
            if text is not None:
                open(os.path.join(p, 'FLController.log'), 'w', encoding='utf-8').write(text)
            return (name, p)
        b1, g1 = leg('2026-10-30_1100_M11-gate-b_i3', sh), leg('2026-10-30_1200_M11-gate-g_i3', ar)
        lines, code = compare('g', 'b', {'M11': g1}, {'M11': b1})
        ok &= code == 1 and lines[-1].startswith('DECISIONS g vs b: 1 legs, 2 steps differ (2 one-sided steps, 0 ')
        ok &= any(l.startswith('  DIFFER M11 obj=24015: b-only [] g-only [pre-stage-next]') for l in lines)
        ok &= any(l.startswith('    first pre-stage-next: 12:58:22.000 situation arbiter') for l in lines)
        ok &= any(l.startswith('  DIFFER M11 obj=24020: b-only [jump] g-only [lane]') for l in lines)
        ok &= sum(l.startswith('  one-sided M11 obj=') for l in lines) == 2         # 24030 / 24040: not counted
        lines, code = compare('b', 'b', {'M11': b1}, {'M11': b1})
        ok &= code == 0 and lines[-1].startswith('DECISIONS b vs b: 1 legs, 0 steps differ')
        p1 = leg('2026-10-30_1102_M12-gate-b_i2', ph.format(a='ship-target', b='pre-stage-next'))
        p2 = leg('2026-10-30_1202_M12-gate-g_i2', ph.format(a='pre-stage-next', b='ship-target'))
        lines, code = compare('g', 'b', {'M12': p2}, {'M12': p1})    # the same rule set over the mission, swapped places
        ok &= code == 1 and lines[-1].startswith('DECISIONS g vs b: 1 legs, 2 steps differ')
        ok &= any(l.startswith('  DIFFER M12 obj=24125>21660: b-only [ship-target] g-only [pre-stage-next]') for l in lines)
        s1 = leg('2026-10-30_1300_M11-gate-s_i3', 'event objective count=1 title=1 "M" text=2 "x" obj=5 "y"\nnoise\n')
        n1 = leg('2026-10-30_1301_M12-gate-s_i2', None)
        lines, code = compare('s', 'b', {'M11': s1, 'M12': n1}, {'M11': b1, 'M12': b1})
        ok &= code == 2 and 'MISSING DATA' in lines[-2] and 'no FLController.log' in lines[-2]   # silent: fail closed
        lines, code = compare('g', 'b', {'M01': g1}, {'M02': b1})
        ok &= code == 2 and lines[-1].startswith('DECISIONS g vs b: 0 legs')                     # nothing in both
    # the digest
    t = ('37 37251187 situation shadow agree threat-fight old=escort,15 new=threat-fight\n'
         'x situation shadow differ lane old=dock new=lane why=a\nnoise\n')
    got = shadow_lines(t)
    ok &= got == [('agree', 'threat-fight', 'old=escort,15 new=threat-fight'), ('differ', 'lane', 'old=dock new=lane why=a')]
    arb = ('situation arbiter agree lane mode=dock plan=agree ok=1\n'
           'situation arbiter agree arrive mode=travel plan=radius:900!=1000 ok=1\n'
           'situation arbiter differ rest mode=hold plan=agree\n'
           'side-effect dock target=5 step=0x1 rule=dock refused why=docked\nside-effect travel target=6 step=0x2 rule=arrive keep\n')
    g2 = shadow_lines(arb)
    ok &= [v for v, _, _ in g2] == ['agree', 'agree', 'plan', 'differ', 'refused']
    ok &= g2[2][1] == 'arbiter:arrive' and g2[4][1] == 'side-effect:dock'
    with tempfile.TemporaryDirectory() as d:
        a, b, c = (os.path.join(d, n) for n in ('2026-10-30_1200_M01a-gate-x_i1', '2026-10-30_1201_M02-gate-x_i2', 'empty'))
        for p in (a, b, c):
            os.mkdir(p)
        open(os.path.join(a, 'FLController.log'), 'w').write(t)
        open(os.path.join(a, 'transcript.txt'), 'w').write(t)          # the same events again: not counted twice
        open(os.path.join(b, 'transcript.txt'), 'w').write('RESULT PASS\n')
        lines, bad = digest([('a', a), ('b', b), ('c', c)])
        ok &= bad and lines[-1].startswith('SHADOW 1 agree / 1 differ over 3') and '1 silent, 1 unread' in lines[-1]
        ok &= any(l.startswith('  DIFFER a: lane') for l in lines)
        lines, bad = digest([('a', a)])
        ok &= bad                                                     # a differ fails
        open(os.path.join(a, 'FLController.log'), 'w').write(t.replace('differ', 'agree'))
        lines, bad = digest([('a', a)])
        ok &= not bad and lines[-1].startswith('SHADOW 2 agree / 0 differ')
    print('selftest ' + ('OK' if ok else 'FAILED'))
    return 0 if ok else 1


def main(a):
    if a[:1] == ['--selftest']:
        return selftest()
    if len(a) == 4 and a[0] == '--gate' and a[2] == '--baseline':
        gate, rg = gate_legs(a[1])
        base, rb = gate_legs(a[3])
        lines, code = compare(a[1], a[3], gate, base)
        if rg or rb:
            lines.insert(-1, '  earlier runs of a leg left out (the latest counts): ' + ', '.join(rg + rb))
        print('\n'.join(lines))
        return code
    if '--baseline' in a:
        print(__doc__)
        return 2
    if a[:1] == ['--gate'] and len(a) >= 2:
        folders = leg_folders(f'-gate-{re.escape(a[1])}$')
    elif a and os.path.isdir(a[0]):
        folders = [(os.path.basename(os.path.normpath(a[0])), a[0])]
    elif a:
        folders = leg_folders(a[0])
    else:
        print(__doc__)
        return 2
    lines, bad = digest(folders)
    print('\n'.join(lines))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
