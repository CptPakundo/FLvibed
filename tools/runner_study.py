"""runner_study.py - the runner study of a speedrun leg must exist and be complete BEFORE any autopilot plan, probe or run
on that leg (the owner, 30 Oct 2026, after saying it several times: "it is a waste of time to NOT first thoroughly
analyze speedrunner legs, their approaches, their use of weapons, their pre-staging and their tactics, and only THEN
formulate a plan for the autopilot"). Enforced by the guard rule `runner-study` (tools\\claude\\hooks\\guard.js) on every
flleg / flset / flarms launch of the leg, and by docs-auditor check 10 for plans and switches.

The study: docs\\research\\runners\\<LEG>.md. Required:
  - a `Sources:` line naming the evidence of all three runs (R1, R2, R3: research\\evidence\\speedrun\\ runs\\*\\events.tsv,
    reference traces, fights.tsv, frames read) - where a run has no data for the leg, say so on that line;
  - per checkpoint of the leg in docs\\checkpoints.tsv (all but <LEG>.control) a heading `## <cp>` with these bullets,
    each with real content (>= 15 characters; "none" / "n/a" must say why):
      - Route:         where they fly, lanes / holes / cruise, distances
      - Pre-staging:   where they are and what they do before the step's banner / trigger
      - Weapons:       guns / missiles / torpedoes / mines used, ranges, refire, target order
      - Consumables:   batteries / nanobots / CMs, when and how many
      - Tactics:       positioning, holds, slides, engine kills, what they ignore
      - Timing:        each runner's seconds for the segment (and par)
      - Risk:          what makes it safe for them, vanilla 1.1 vs HD Edition differences
      - Ours:          what our autopilot does there and the gap
  - a closing `## Plan` section: what the autopilot should copy, in which order, and why (the plan comes from here).

Usage: python -I tools\\runner_study.py check <LEG>   -> "STUDY OK <LEG>" (exit 0) or the missing items (exit 1)
       python -I tools\\runner_study.py selftest
"""
import csv
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FIELDS = ['Route', 'Pre-staging', 'Weapons', 'Consumables', 'Tactics', 'Timing', 'Risk', 'Ours']
MIN = 15


def checkpoints(leg, root=ROOT):
    with open(os.path.join(root, 'docs', 'checkpoints.tsv'), encoding='utf-8') as f:
        return [r['cp'] for r in csv.DictReader(f, delimiter='\t')
                if r['leg'] == leg and r['cp'].startswith(leg + '.') and not r['cp'].endswith('.control')]


def check_text(text, cps):
    missing = []
    src = re.search(r'^Sources:(.*)$', text, re.M)
    if not src:
        missing.append('a "Sources:" line')
    else:
        for r in ('R1', 'R2', 'R3'):
            if not re.search(r'\b' + r + r'\b', src.group(1)):
                missing.append(f'Sources: run {r} (or why it has no data)')
    # sections by "## " headings
    parts = re.split(r'^## +', text, flags=re.M)
    sections = {}
    for p in parts[1:]:
        head, _, body = p.partition('\n')
        sections[head.strip().split()[0] if head.strip() else ''] = body
    for cp in cps:
        body = sections.get(cp)
        if body is None:
            missing.append(f'## {cp}')
            continue
        for fld in FIELDS:
            m = re.search(r'^\s*-\s*' + re.escape(fld) + r':\s*(.*)$', body, re.M)
            if not m or len(m.group(1).strip()) < MIN:
                missing.append(f'{cp}: {fld}')
    plan = sections.get('Plan')
    if plan is None or len(plan.strip()) < 200:
        missing.append('## Plan (what to copy, in which order, why; 200+ characters)')
    return missing


def check(leg, root=ROOT):
    path = os.path.join(root, 'docs', 'research', 'runners', leg + '.md')
    if not os.path.exists(path):
        return [f'docs\\research\\runners\\{leg}.md does not exist']
    with open(path, encoding='utf-8') as f:
        return check_text(f.read(), checkpoints(leg, root))


def selftest():
    cps = ['MX.a', 'MX.b']
    good = 'Sources: R1 events.tsv, R2 frames, R3 no data for this leg (not recorded)\n'
    for cp in cps:
        good += f'## {cp}\n' + ''.join(f'- {f}: ' + 'x' * 20 + '\n' for f in FIELDS)
    good += '## Plan\n' + 'y' * 220 + '\n'
    fails = 0
    if check_text(good, cps):
        fails += 1; print('FAIL good text:', check_text(good, cps))
    bad = good.replace('- Weapons: ' + 'x' * 20, '- Weapons: guns')
    if check_text(bad, cps) != ['MX.a: Weapons', 'MX.b: Weapons']:
        fails += 1; print('FAIL short field:', check_text(bad, cps))
    if 'Sources: run R3 (or why it has no data)' not in check_text(good.replace('R3', 'Rx'), cps):
        fails += 1; print('FAIL R3')
    if '## MX.b' not in check_text(good.replace('## MX.b', '## MX.c'), cps):
        fails += 1; print('FAIL section')
    print(f'selftest: {"OK" if not fails else "FAILED"} ({fails} failure(s))')
    return fails


if __name__ == '__main__':
    a = sys.argv[1:]
    if a[:1] == ['selftest']:
        sys.exit(1 if selftest() else 0)
    if len(a) == 2 and a[0] == 'check':
        miss = check(a[1])
        if not miss:
            print(f'STUDY OK {a[1]}')
            sys.exit(0)
        print(f'STUDY MISSING {a[1]}: ' + '; '.join(miss[:25]) + (f' (+{len(miss) - 25} more)' if len(miss) > 25 else ''))
        sys.exit(1)
    print(__doc__)
    sys.exit(2)
