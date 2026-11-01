"""digest.py - a deterministic digest of leg / set / gate output (WORK_QUEUE H3, IDEAS T27; 30 Oct 2026): read the
digest, not the logs (Working method 23). One line per leg from research\\experience\\legs\\legs.tsv (the authoritative
outcome) and the leg's folder (its transcript), then one summary line. Every failure is classed:
  death      the player died (a game result)
  failstate  a fail state / checkpoint limit (a game result)
  missionfail the mission's own "Mission Failed" message (a game result: read why)
  harness    the harness or the infrastructure failed (boot, no answer, over budget, a throw): a HARNESS BUG to fix,
             never a result (Working method 23)
  unknown    no rule matched - read that one leg
Fails closed: a leg folder of the selection with no legs.tsv row is UNJUDGED; a gate row whose leg has no legs.tsv row
too; an unreadable transcript is said so. Exit 0 = every leg judged and no harness / unknown class; 1 otherwise.

  python -I tools\\digest.py <label regex>        e.g.  'M13-dt60'   or  '^gate-q3s2'
  python -I tools\\digest.py --gate <tag>         the gate's sets\\gate_<tag>.tsv rows (verdict OK / SLOWER / FAIL) + the legs
  python -I tools\\digest.py --selftest
"""
import csv
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LEGS = os.path.join(ROOT, 'research', 'experience', 'legs')
TSV = os.path.join(LEGS, 'legs.tsv')

HARNESS = re.compile(r'deploy threw|does not answer|no answer to|OVER BUDGET|not reached after|exception|throw|'
                     r'timed out|TIMEOUT|refused|cannot|unknown pad button|Freelancer exited', re.I)


def classify(outcome, detail):
    """The failure class of one legs.tsv row (None for a PASS)."""
    if outcome == 'PASS':
        return None
    d = detail or ''
    if 'player-death' in d:
        return 'death'
    if 'FAIL STATE' in d or 'CHECKPOINT' in d:
        return 'failstate'
    if 'mission-message type=FAILURE' in d:
        return 'missionfail'  # the mission's own "Mission Failed" (a game result: read why)
    if HARNESS.search(d):
        return 'harness'
    return 'unknown'


def rows(path=TSV):
    with open(path, encoding='utf-8', newline='') as f:
        return list(csv.DictReader(f, delimiter='\t'))


def folder_of(label, instance, folders):
    want = f'_{label}_i{instance}'
    hits = [f for f in folders if f.endswith(want)]
    return sorted(hits)[-1] if hits else None


def transcript_errors(folder):
    """The transcript's harness-looking lines (at most 2), or a note that it is missing."""
    p = os.path.join(LEGS, folder, 'transcript.txt')
    if not os.path.exists(p):
        return ['(no transcript)']
    try:
        with open(p, encoding='utf-8', errors='replace') as f:
            lines = [l.strip() for l in f if HARNESS.search(l) and 'Host Application' not in l]
    except OSError:
        return ['(transcript unreadable)']
    return [l[:140] for l in lines[:2]]


def digest(label_rx, gate=None):
    rx = re.compile(label_rx)
    all_rows = rows()
    sel = [r for r in all_rows if rx.search(r.get('label', ''))]
    folders = [f for f in os.listdir(LEGS) if os.path.isdir(os.path.join(LEGS, f))]
    out, classes, bad = [], {}, 0
    seen = set()
    for r in sel:
        c = classify(r['outcome'], r.get('detail', ''))
        classes[c or 'pass'] = classes.get(c or 'pass', 0) + 1
        f = folder_of(r['label'], r.get('instance', ''), folders)
        seen.add(f)
        extra = ''
        if c in ('harness', 'unknown'):
            bad += 1
            extra = ' | ' + ' / '.join(transcript_errors(f) if f else ['(no folder)'])
        out.append(f"{r['label']:<22} i{r.get('instance', '?'):<2} {r['outcome']:<5} {r['secs']:>5} s  "
                   f"{(c or 'pass').upper():<9} {(r.get('detail') or '')[:70]}{extra}")
    # fails closed: a folder of the selection without its row
    for f in sorted(folders):
        m = re.match(r'^\d{4}-\d{2}-\d{2}_\d{4}_(.+)_i(\w+)$', f)
        if m and rx.search(m.group(1)) and f not in seen and not any(
                r['label'] == m.group(1) and r.get('instance') == m.group(2) for r in sel):
            bad += 1
            out.append(f"{m.group(1):<22} i{m.group(2):<2} UNJUDGED (a leg folder with no legs.tsv row: the run did not finish)")
    if gate:
        gp = os.path.join(LEGS, 'sets', f'gate_{gate}.tsv')
        if not os.path.exists(gp):
            out.append(f'gate {gate}: no sets\\gate_{gate}.tsv')
            bad += 1
        else:
            with open(gp, encoding='utf-8', newline='') as f:
                g = list(csv.DictReader(f, delimiter='\t'))
            v = {}
            for r in g:
                v[r['verdict']] = v.get(r['verdict'], 0) + 1
            slow = [f"{r['leg']} {r['verdict']} {int(r['diff']):+d} s" if r['diff'].lstrip('-').isdigit()
                    else f"{r['leg']} {r['verdict']}" for r in g if r['verdict'] != 'OK']
            out.append(f"gate {gate}: {len(g)} legs - " + ', '.join(f'{k} {n}' for k, n in sorted(v.items())) +
                       (f"; not OK: {'; '.join(slow)}" if slow else ''))
    summary = (f"DIGEST {label_rx}: {len(sel)} legs - " + ', '.join(f'{k} {n}' for k, n in sorted(classes.items())) +
               (f" - {bad} need reading (harness / unknown / unjudged)" if bad else ' - every leg judged'))
    return out + [summary], bad


def selftest():
    ok = True
    cases = [('PASS', '', None), ('FAIL', 'FAILED (FAILED: 628 3798500 player-death) total 121 s', 'death'),
             ('FAIL', 'FAILED (FAIL STATE (CHECKPOINT M13.bsdown not reached: segment over 130 s', 'failstate'),
             ('FAIL', 'boot try 2 failed: the game does not answer', 'harness'),
             ('FAIL', 'no answer to \'state\' within 5000 ms', 'harness'), ('FAIL', 'something new', 'unknown'),
             ('FAIL', 'FAILED (FAILED: 1411 6214625 mission-message type=FAILURE text=23350 "Mission Failed.', 'missionfail')]
    for o, d, want in cases:
        got = classify(o, d)
        if got != want:
            ok = False
            print(f'FAIL classify({o!r}, {d[:40]!r}) = {got}, want {want}')
    print('digest selftest: ' + ('PASS' if ok else 'FAIL'))
    return 0 if ok else 1


if __name__ == '__main__':
    a = sys.argv[1:]
    if a[:1] == ['--selftest']:
        sys.exit(selftest())
    if a[:1] == ['--gate'] and len(a) >= 2:
        lines, bad = digest(f'-gate-{re.escape(a[1])}$', gate=a[1])  # the gate's labels: <leg>-gate-<tag>

    elif a:
        lines, bad = digest(a[0])
    else:
        print(__doc__)
        sys.exit(2)
    print('\n'.join(lines))
    sys.exit(1 if bad else 0)
