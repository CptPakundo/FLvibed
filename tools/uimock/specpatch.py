"""specpatch.py - applies one patch (a JSON file in tools\\uimock\\patches\\) to BOTH halves of the living spec: the
mock-up's source (part2_data.js: the proposal the page starts from) and a spec document (the owner's `ui/spec` from the
artifact, or docs\\UI_SPEC.json), so what was built lands in the mock-up and in the spec the same way (the standing order,
tools\\uimock\\README.md). Patches are additive: notes appended to a screen state or a decision, a row's default / value /
help replaced, a state extra's field set. Nothing of the owner's other edits is touched.

  python -I tools\\uimock\\specpatch.py <patch.json> --js tools\\uimock\\part2_data.js --spec <spec.json> [--out <patched.json>]

patch.json: { "add_rows": {"<tab id>/<section label>": {"first": true | "after": "<row key>", "rows": [{"key", "label", "kind": "toggle" | "picker" |
"slider", "def", "options" | "min" / "max" / "step" / "unit", "help", "tier"}]}}, "rows": {"<key>": {"def": .., "value": ..,
"help": ".."}}, "states": {"<screen>/<state>": {"notes": [".."],
"extras": {"<t>": {"<field>": value}}}}, "decisions": {"<id>": {"note": ".."}} }. A decision note is appended to the
existing note (" | " between). The spec file's top-level shape is either the artifact's document ({spec: {...}}) or the
spec itself (docs\\UI_SPEC.json with an `_about` line).
"""
import argparse
import json
import re
import sys


def js_literal(v):
    if isinstance(v, str):
        return "'" + v.replace("\\", "\\\\").replace("'", "\\'") + "'"
    if v is None:
        return 'null'
    if isinstance(v, bool):
        return 'true' if v else 'false'
    return json.dumps(v)


def patch_js(src, patch):
    lines = src.split('\n')
    out = []
    screen = None
    in_screens = False
    for line in lines:
        if line.startswith('const SCREENS = ['):
            in_screens = True
        elif in_screens and line.startswith('];'):
            in_screens = False
        if in_screens:
            m = re.match(r"\s*\{ id: '([a-z_]+)', group:", line)
            if m:
                screen = m.group(1)
            m = re.match(r"\s*\{ id: '([a-z_]+)', label:", line)
            if m and screen:
                key = screen + '/' + m.group(1)
                p = patch.get('states', {}).get(key)
                if p:
                    for t, fields in p.get('extras', {}).items():
                        for f, v in fields.items():
                            # the extra "{ t: 'wheel', ... sel: 0 }" -> replace the field inside that extra's braces
                            def rep(mm):
                                body = mm.group(0)
                                return re.sub(r"\b%s: [^,}]+" % re.escape(f), "%s: %s" % (f, js_literal(v)), body)
                            line, n = re.subn(r"\{ t: '%s'[^{}]*\}" % re.escape(t), rep, line)
                            if n == 0:
                                sys.exit('extra %s not found in %s' % (t, key))
                    for note in p.get('notes', []):
                        lit = js_literal(note)
                        if 'notes: [' in line:
                            # append before the notes array's own closing bracket: scan from "notes: [" with quote
                            # awareness (the state's line goes on with "] } ] }," when it is the screen's last state)
                            start = line.index('notes: [') + len('notes: [')
                            depth, i, quote = 1, start, None
                            while i < len(line) and depth > 0:
                                c = line[i]
                                if quote:
                                    if c == '\\':
                                        i += 1
                                    elif c == quote:
                                        quote = None
                                elif c in "'\"":
                                    quote = c
                                elif c == '[':
                                    depth += 1
                                elif c == ']':
                                    depth -= 1
                                i += 1
                            idx = i - 1  # the matching ']'
                            empty = line[start:idx].strip() == ''
                            line = line[:idx] + ('' if empty else ', ') + lit + line[idx:]
                        else:
                            # the state has no notes yet: add the array before the state object's closing "}"
                            m2 = re.search(r"\s*\}(\s*\]\s*\},?\s*|\s*,?\s*)$", line)
                            idx = m2.start() if m2 else line.rfind('}')
                            line = line[:idx].rstrip() + ', notes: [' + lit + ']' + line[idx:]
        m = re.match(r"\s*\{ id: '([a-z_]+)', q: '", line)
        if m and m.group(1) in patch.get('decisions', {}):
            note = patch['decisions'][m.group(1)].get('note', '')
            def rep_note(mm):
                old = mm.group(1)
                return "note: " + js_literal((old + ' | ' if old else '') + note)
            line, n = re.subn(r"note: '((?:[^'\\]|\\.)*)'", rep_note, line)
            if n == 0:
                sys.exit('decision %s has no note field' % m.group(1))
        for key, fields in patch.get('rows', {}).items():
            m2 = re.match(r"(\s*)([TPSKB])\('%s', " % re.escape(key), line)
            if m2:
                kind = m2.group(2)
                if 'def' in fields:
                    d = js_literal(fields['def'])
                    if kind == 'P':
                        line = re.sub(r"(\]\s*,\s*)'[^']*'(\s*,\s*\{)", lambda mm: mm.group(1) + d + mm.group(2), line, count=1)
                    elif kind in 'TK':
                        line = re.sub(r"('%s', '[^']*', )([^,]+)(,)" % re.escape(key), lambda mm: mm.group(1) + d + mm.group(3), line, count=1)
                    elif kind == 'S':
                        line = re.sub(r"(, [-0-9.]+, [-0-9.]+, [-0-9.]+, )[-0-9.]+(, \{)", lambda mm: mm.group(1) + d + mm.group(2), line, count=1)
                if 'help' in fields:
                    line = re.sub(r"help: '(?:[^'\\]|\\.)*'", "help: " + js_literal(fields['help']), line, count=1)
        out.append(line)
    return '\n'.join(out)


def row_js(r):
    """A spec row as the mock-up source writes it (T / P / S helpers)."""
    o = {}
    for f in ('help', 'tier', 'unit', 'parent', 'words', 'needs'):
        if f in r and r[f] not in (None, '', [], 'player', 'live'):
            o[f] = r[f]
    opts = '{ ' + ', '.join('%s: %s' % (k, js_literal(v) if not isinstance(v, list) else
                                       '[' + ', '.join(js_literal(x) for x in v) + ']') for k, v in o.items()) + ' }'
    k, lab = js_literal(r['key']), js_literal(r['label'])
    if r['kind'] == 'toggle':
        return "T(%s, %s, %s, %s)" % (k, lab, js_literal(r['def']), opts)
    if r['kind'] == 'picker':
        return "P(%s, %s, [%s], %s, %s)" % (k, lab, ', '.join(js_literal(x) for x in r['options']), js_literal(r['def']),
                                           opts)
    if r['kind'] == 'slider':
        return "S(%s, %s, %s, %s, %s, %s, %s)" % (k, lab, r['min'], r['max'], r['step'], js_literal(r['def']), opts)
    sys.exit('row kind %s not supported' % r['kind'])


def full_row(r):
    """Fills a new row's defaults the way the mock-up's R() does (id, value, needs, tier, parent; sliders' unit / words)."""
    out = dict(id=r['key'], key=r['key'], label=r['label'], kind=r['kind'], help='', needs='live', tier='player',
               parent=None)
    if r['kind'] == 'slider':
        out.update(unit='', words=[])
    out.update(r)
    out['value'] = r['def']
    return out


def add_rows_js(src, patch):
    """patch['add_rows'] = {"<tab id>/<section label>": {"first": true|false, "rows": [row, ...]}}: inserts the rows'
    lines at the start (first) or the end of that section's rows in the mock-up source."""
    for where, p in patch.get('add_rows', {}).items():
        tab, label = where.split('/', 1)
        lines = src.split('\n')
        ti = next((i for i, l in enumerate(lines) if re.match(r"\s*\{ id: '%s', label:" % re.escape(tab), l)), None)
        if ti is None:
            sys.exit('tab %s not in the mock-up source' % tab)
        si = next((i for i in range(ti, len(lines)) if re.match(r"\s*\{ label: '%s', rows: \[" % re.escape(label), lines[i])),
                  None)
        if si is None:
            sys.exit('section %s/%s not in the mock-up source' % (tab, label))
        indent = re.match(r"(\s*)", lines[si + 1]).group(1)
        new = [indent + row_js(r) + ',' for r in p['rows'] if ("'%s'" % r['key']) not in src]
        if not new:
            continue
        if p.get('after'):
            ai = next((i for i in range(si + 1, len(lines)) if ("('%s'," % p['after']) in lines[i]), None)
            if ai is None:
                sys.exit('row %s not in %s' % (p['after'], where))
            lines[ai + 1:ai + 1] = new
        elif p.get('first', True):
            lines[si + 1:si + 1] = new
        else:
            j = si + 1
            while not lines[j].rstrip().endswith('] },') and not lines[j].rstrip().endswith('] } ] },'):
                j += 1
            lines[j:j] = new
        src = '\n'.join(lines)
    return src


def patch_spec(doc, patch):
    spec = doc['spec'] if 'spec' in doc and 'tabs' in doc['spec'] else doc
    for where, p in patch.get('add_rows', {}).items():
        tab, label = where.split('/', 1)
        t = next((t for t in spec['tabs'] if t['id'] == tab), None)
        if not t:
            sys.exit('tab %s not in the spec' % tab)
        sec = next((s for s in t['sections'] if s.get('label') == label), None)
        if not sec:
            sys.exit('section %s not in the spec' % where)
        have = {r['key'] for s in t['sections'] for r in s['rows']}
        new = [full_row(r) for r in p['rows'] if r['key'] not in have]
        if p.get('after'):
            ai = next((i for i, r in enumerate(sec['rows']) if r['key'] == p['after']), None)
            if ai is None:
                sys.exit('row %s not in %s' % (p['after'], where))
            sec['rows'][ai + 1:ai + 1] = new
        else:
            sec['rows'] = (new + sec['rows']) if p.get('first', True) else (sec['rows'] + new)
    rows = {}
    for t in spec['tabs']:
        for sec in t['sections']:
            for r in sec['rows']:
                rows[r['key']] = r
    for key, fields in patch.get('rows', {}).items():
        if key not in rows:
            sys.exit('row %s not in the spec' % key)
        for f, v in fields.items():
            rows[key][f] = v
    states = {}
    for s in spec['screens']:
        for st in s['states']:
            states[s['id'] + '/' + st['id']] = st
    for key, p in patch.get('states', {}).items():
        if key not in states:
            sys.exit('state %s not in the spec' % key)
        st = states[key]
        for t, fields in p.get('extras', {}).items():
            hit = [e for e in st.get('extras', []) if e.get('t') == t]
            if not hit:
                sys.exit('extra %s not in %s' % (t, key))
            hit[0].update(fields)
        st.setdefault('notes', [])
        for note in p.get('notes', []):
            if note not in st['notes']:
                st['notes'].append(note)
    for did, p in patch.get('decisions', {}).items():
        hit = [d for d in spec['decisions'] if d['id'] == did]
        if not hit:
            sys.exit('decision %s not in the spec' % did)
        old = hit[0].get('note', '')
        if p['note'] not in old:
            hit[0]['note'] = (old + ' | ' if old else '') + p['note']
    return doc


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('patch')
    ap.add_argument('--js')
    ap.add_argument('--spec')
    ap.add_argument('--out')
    a = ap.parse_args()
    patch = json.load(open(a.patch, encoding='utf-8'))
    if a.js:
        src = open(a.js, encoding='utf-8').read()
        src = add_rows_js(src, patch)
        open(a.js, 'w', encoding='utf-8', newline='\n').write(patch_js(src, patch))
        print('patched', a.js)
    if a.spec:
        doc = json.load(open(a.spec, encoding='utf-8'))
        doc = patch_spec(doc, patch)
        out = a.out or a.spec
        json.dump(doc, open(out, 'w', encoding='utf-8', newline='\n'), ensure_ascii=False, indent=1)
        print('patched', out)


if __name__ == '__main__':
    main()
