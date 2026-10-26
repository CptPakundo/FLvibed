"""Offline port of core/game/mission.cpp AdvanceGoals (systems compared by nickname), to check what it picks.
Usage: resolver_check.py <MISSIONS dir> [textId system | title:<id> system]  (no args after the dir: sweep every objective)"""
import glob, os, re, sys

def sections(path):
    secs, cur = [], None
    for raw in open(path, encoding='latin-1'):
        line = raw.split(';')[0].strip()
        if not line:
            continue
        m = re.match(r'^\[(.+)\]$', line)
        if m:
            cur = [m.group(1).lower(), []]; secs.append(cur); continue
        if cur is not None and '=' in line:
            k, v = line.split('=', 1); cur[1].append((k.strip().lower(), v.strip()))
    return secs

def fields(v): return [x.strip() for x in v.split(',')]

def load(path):
    s = {'path': path, 'trig': [], 'by': {}, 'obj': {}, 'flist': set(), 'fship': set(), 'solar': [], 'titles': set()}
    for typ, kv in sections(path):
        d = dict(kv); nick = d.get('nickname')
        if typ == 'trigger' and nick:
            t = {'name': nick.lower(), 'system': (d.get('system') or '').lower(), 'cnd': [(k, v) for k, v in kv if k.startswith('cnd_')], 'act': [(k, v) for k, v in kv if k.startswith('act_')]}
            if t['system'] == 'all': t['system'] = ''
            s['by'][t['name']] = len(s['trig']); s['trig'].append(t)
        elif typ == 'nnobjective' and nick:
            ids = []
            f = fields(d.get('type', ''))
            for x in f[1:4]:
                if x.isdigit() and int(x) > 0: ids.append(int(x))
            s['obj'][nick.lower()] = ids
        elif typ == 'msnsolar' and nick:
            s['solar'].append((nick.lower(), (d.get('label') or '').lower()))
        elif typ == 'objlist' and nick:
            for k, v in kv:
                if k in ('follow', 'followplayer', 'stayinrange') and v.lower().startswith('player'):
                    s['flist'].add(nick.lower())
    for t in s['trig']:
        for k, v in t['act']:
            if k == 'act_settitle' and v.strip().isdigit(): s['titles'].add(int(v))
            if k in ('act_giveobjlist', 'act_spawnship'):
                f = fields(v)
                if len(f) >= 2 and f[1].lower() in s['flist']: s['fship'].add(f[0].lower())
    return s

def leads(s, start, current, depth, armed_fail=True):
    # armed_fail=False (destroy goals): only a FAIL reached through triggers that fire at once (Cnd_True / Cnd_Timer)
    # counts - a watcher the chain merely arms (M13 fail_range3: "outside 18 km -> FAIL") is no failure of the goal
    seen, todo, found = set(), [(start, 0, True)], ''
    while todo:
        i, d, sure = todo.pop()
        if i in seen: continue
        seen.add(i)
        for k, v in s['trig'][i]['act']:
            f = fields(v)
            if not f: continue
            if k == 'act_changestate':
                if f[0].lower() == 'fail' and (sure or armed_fail): return 'FAIL'
                if f[0].lower() == 'succeed' and not found: found = 'SUCCEED'
            elif k == 'act_setnnobj' and len(f) == 1:
                o = f[0].lower()
                if o != 'nn_blank' and o not in current and not found: found = o
            elif k == 'act_acttrig' and d < depth:
                j = s['by'].get(f[0].lower())
                if j is not None:
                    c = s['trig'][j]['cnd']
                    todo.append((j, d + 1, sure and len(c) == 1 and c[0][0] in ('cnd_true', 'cnd_timer')))
    return found

def goals(s, current, system):
    shown = set(current)
    armed_by = {}
    for i, t in enumerate(s['trig']):
        for k, v in t['act']:
            if k == 'act_acttrig':
                j = s['by'].get(fields(v)[0].lower())
                if j is not None: armed_by.setdefault(j, []).append(i)
    roots = set(i for i, t in enumerate(s['trig']) for k, v in t['act'] if k == 'act_setnnobj' and len(fields(v)) == 1 and fields(v)[0].lower() in current)
    up = list(roots)
    for _ in range(8):
        nxt = []
        for i in up:
            for p in armed_by.get(i, []):
                if p not in roots: roots.add(p); nxt.append(p)
        up = nxt
    for i, t in enumerate(s['trig']):
        for k, v in t['cnd']:
            if k in ('cnd_systementer', 'cnd_spaceenter') and fields(v)[0].lower() == system: roots.add(i)
    armed, frontier = set(), list(roots)
    for _ in range(3):
        nxt = []
        for r in frontier:
            for k, v in s['trig'][r]['act']:
                if k == 'act_acttrig':
                    j = s['by'].get(fields(v)[0].lower())
                    if j is None or j in armed: continue
                    armed.add(j); t = s['trig'][j]
                    if len(t['cnd']) == 1 and t['cnd'][0][0] in ('cnd_true', 'cnd_timer'): nxt.append(j)
        frontier = nxt
    off = set()
    for r in roots:
        for k, v in s['trig'][r]['act']:
            if k == 'act_deacttrig':
                j = s['by'].get(fields(v)[0].lower())
                if j is not None: off.add(j)
    def system_of(i):
        seen = set()
        while not s['trig'][i]['system'] and i not in seen and armed_by.get(i):
            seen.add(i); i = armed_by[i][0]
        return s['trig'][i]['system']
    out = []
    for i in armed:
        t = s['trig'][i]
        tsys = system_of(i)
        if i in roots or i in off or (tsys and tsys != system) or len(t['cnd']) != 1: continue
        k, v = t['cnd'][0]; f = fields(v)
        if k == 'cnd_distship' and len(f) >= 4 and f[0].lower() == 'inside': what = f'{f[2]} {f[3]} m'
        elif k == 'cnd_distvec' and len(f) >= 6 and f[0].lower() == 'inside': what = f'point {f[2]},{f[3]},{f[4]} {f[5]} m'
        elif k == 'cnd_destroyed' and len(f) >= 2:
            # destroy N of the mission's solars named or labelled f[0] (ships: not yet)
            objs = [n for n, lab in s['solar'] if f[0].lower() in (n, lab)]
            try: cnt = int(f[1])
            except ValueError: continue
            if not objs or cnt <= 0 or cnt > len(objs): continue
            l = leads(s, i, shown, 4, False)
            if l and l != 'FAIL': out.append((t['name'], 'destroy', f'{cnt} of {",".join(objs)}', l))
            continue
        else: continue
        subj = f[1].lower()
        if subj != 'player' and subj not in s['fship']: continue
        l = leads(s, i, shown, 4)
        if not l or l == 'FAIL': continue
        out.append((t['name'], subj, what, l))
    return out

root = sys.argv[1]
scripts = [load(p) for p in glob.glob(os.path.join(root, 'M*', '*.ini'))]
scripts = [s for s in scripts if s['trig']]
if len(sys.argv) >= 4 and sys.argv[2].startswith('title:'):
    # a save loaded mid-mission ("Awaiting Mission Objective"): the script that sets the title, no current objective
    title, sysn = int(sys.argv[2][6:]), sys.argv[3].lower()
    for s in scripts:
        if title in s['titles']:
            print(os.path.basename(s['path']), 'by title', title)
            for g in goals(s, set(), sysn):
                if g[1] == 'destroy': print('  ', g)  # (mission.cpp: destroy goals only by title)
elif len(sys.argv) >= 4:
    tid, sysn = int(sys.argv[2]), sys.argv[3].lower()
    # (mission.cpp: a placeholder objective, or a text more than one script uses, names no step)
    hits = [s for s in scripts if any(tid in ids and o not in ('null', 'nn_blank') for o, ids in s['obj'].items())]
    if len(hits) > 1: print('text', tid, 'is in', len(hits), 'scripts: no step (falls back to the last objective / title)'); hits = []
    for s in hits:
        cur = {o for o, ids in s['obj'].items() if tid in ids and o not in ('null', 'nn_blank')}
        if cur:
            print(os.path.basename(s['path']), sorted(cur))
            for g in goals(s, cur, sysn): print('  ', g)
else:
    for s in scripts:
        for o, ids in s['obj'].items():
            # the objective's system from its type line
            sysn = ''
            for typ, kv in sections(s['path']):
                pass
            for t in s['trig']:
                for k, v in t['act']:
                    if k == 'act_setnnobj' and fields(v)[0].lower() == o:
                        sysn = sysn or t['system']
            if not sysn: continue
            g = goals(s, {o}, sysn)
            if g: print(os.path.basename(s['path']), o, ids[:1], sysn, '->', g)
