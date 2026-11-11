"""specpatch.py - applies one patch (a JSON file in tools\\uimock\\patches\\) to BOTH halves of the Feature Tour: the
mock-up's source (part2_data.js: the proposal the page starts from) and a spec document (the owner's `ui/spec` from the
artifact, or docs\\UI_SPEC.json), so what was built lands in the mock-up and in the spec the same way (the standing order,
tools\\uimock\\README.md). Patches are additive: notes appended to a screen state or a decision, a row's default / value /
help replaced, a state extra's field set. Nothing of the owner's other edits is touched.

  python -I tools\\uimock\\specpatch.py <patch.json> --js tools\\uimock\\part2_data.js --spec <spec.json> [--out <patched.json>]

patch.json: { "add_rows": {"<tab id>/<section label>": {"first": true | "after": "<row key>", "rows": [{"key", "label", "kind": "toggle" | "picker" |
"slider", "def", "options" | "min" / "max" / "step" / "unit", "help", "tier"}]}}, "rows": {"<key>": {"def": .., "value": ..,
"help": ".."}}, "states": {"<screen>/<state>": {"notes": [".."], "prompts": [[["<key>", ..], "<label>", "hold"?], ..] (the
list replaced whole), "extras": {"<t>": {"<field>": value}}}}, "decisions": {"<id>": {"note": ".."}}, "top": {"<key>": <block>},
"bindings": {"presets/<PRESET>/flight/<input>" | "common/<context>/<input>": {"tap" | "hold" | "dbl": "<action>"}},
"remove_rows": ["<key>", ..], "new_rows": [row, ..], "retab": [{"id", "label", "presetKey"?, "head"?, "sections": [{"label", "head"?, "indent"?, "rows": ["<key>", ..]}]}],
"presets": {"<group>": [{"name", "values": {..}}]} }. "top" sets a spec-only top-level block (the spec half only; e.g. "hud2_presets", Q7).
"retab" rebuilds the tabs from the rows by key, new_rows included: every existing row must land once; the mock-up's TABS block is then
written from the patched spec, so it needs --spec as well. "presets" sets whole groups in both halves. A decision note is appended to the
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


def remove_rows_js(src, patch):
    """patch['remove_rows'] in the mock-up source: the row's line goes; a section close it carried moves to the line before."""
    lines = src.split('\n')
    for key in patch.get('remove_rows', []):
        i = next((i for i, l in enumerate(lines) if re.match(r"\s*[TPSKB]\('%s', " % re.escape(key), l)), None)
        if i is None:
            continue
        closed = re.match(r".*\)(\s*\] \},?\s*)$", lines[i])
        if closed:
            if not lines[i - 1].rstrip().endswith('),'):
                sys.exit('row %s closes its section and is its only row: remove the section by hand' % key)
            lines[i - 1] = lines[i - 1].rstrip()[:-1] + closed.group(1).rstrip()
        del lines[i]
    return '\n'.join(lines)


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
                    if 'prompts' in p:  # a state's prompt list replaced whole: [[keys, label, v], ...]
                        pm = ', '.join("Pm(%s, %s%s)" % (
                            ('[' + ', '.join(js_literal(k) for k in q[0]) + ']') if len(q[0]) > 1 else js_literal(q[0][0]),
                            js_literal(q[1]), (', ' + js_literal(q[2])) if len(q) > 2 and q[2] else '') for q in p['prompts'])
                        line, n = re.subn(r"prompts: \[(?:Pm\((?:[^()'\[\]]|\[[^\]]*\]|'(?:[^'\\]|\\.)*')*\)(?:, )?)*\]",
                                          lambda _: 'prompts: [' + pm + ']', line, count=1)
                        if n == 0:
                            sys.exit('prompts not found in %s' % key)
                    for t, fields in p.get('extras', {}).items():
                        for f, v in fields.items():
                            # the extra "{ t: 'wheel', ... sel: 0 }" -> replace the field inside that extra's braces
                            def rep(mm):
                                body = mm.group(0)
                                return re.sub(r"\b%s: (?:\[(?:[^\[\]]|\[[^\]]*\])*\]|[^,}]+)" % re.escape(f),
                                              lambda _: "%s: %s" % (f, js_literal(v)), body)
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
                old = re.sub(r"\\(.)", r"\1", mm.group(1))  # the source's escapes undone: js_literal escapes again
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
                if 'label' in fields:  # (0.2.1: a row's label and a picker's options replaced too)
                    line = re.sub(r"(\('%s', )'(?:[^'\\]|\\.)*'" % re.escape(key),
                                  lambda mm: mm.group(1) + js_literal(fields['label']), line, count=1)
                if 'options' in fields and kind == 'P':
                    opts = '[' + ', '.join(js_literal(o) for o in fields['options']) + ']'
                    line = re.sub(r"\[[^\]]*\]", lambda mm: opts, line, count=1)
        out.append(line)
    return '\n'.join(out)


def row_js(r):
    """A spec row as the mock-up source writes it (T / P / S / K / B helpers)."""
    o = {}
    for f in ('help', 'tier', 'unit', 'parent', 'words', 'needs', 'preset', 'btn', 'head', 'bundle', 'off_help', 'value_help'):
        if f in r and r[f] not in (None, '', [], {}, False, 'player', 'live'):
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
    if r['kind'] == 'key':
        return "K(%s, %s, %s, %s)" % (k, lab, js_literal(r['def']), opts)
    if r['kind'] == 'button':
        return "B(%s, %s, %s)" % (k, lab, opts)
    sys.exit('row kind %s not supported' % r['kind'])


def retab_js(src, spec):
    """The mock-up's TABS block written from the spec (patch['retab']): the rows moved tab, so a line-by-line patch cannot follow them."""
    lines = src.split('\n')
    start = next(i for i, l in enumerate(lines) if l.startswith('const TABS = ['))
    end = next(i for i in range(start, len(lines)) if lines[i].startswith('];'))
    out = []
    for ti, t in enumerate(spec['tabs']):
        head = "  { id: %s, label: %s, " % (js_literal(t['id']), js_literal(t['label']))
        for f in ('presetKey', 'head'):
            if t.get(f):
                head += "%s: %s, " % (f, js_literal(t[f]))
        out.append(head + 'sections: [')
        for si, s in enumerate(t['sections']):
            extra = ''.join(", %s: %s" % (f, js_literal(s[f])) for f in ('head', 'indent') if s.get(f))
            out.append("    { label: %s, rows: [" % js_literal(s['label']) if not extra else
                       "    { label: %s%s, rows: [" % (js_literal(s['label']), extra))
            body = ['      ' + row_js(r) for r in s['rows']]
            out.append(',\n'.join(body) + ' ] }' + (',' if si + 1 < len(t['sections']) else ' ] }' + (',' if ti + 1 < len(spec['tabs']) else '')))
    lines[start + 1:end] = out
    return '\n'.join(lines)


def presets_js(src, patch):
    """patch['presets'] in the mock-up source: each group's lines inside const PRESETS, replaced or added before its close."""
    lines = src.split('\n')
    for group, items in patch.get('presets', {}).items():
        start = next(i for i, l in enumerate(lines) if l.startswith('const PRESETS = {'))
        end = next(i for i in range(start, len(lines)) if lines[i].startswith('};'))
        text = ["  %s: [" % group] + ["    { name: %s, values: %s }%s" % (js_literal(p['name']), json.dumps(p['values'], ensure_ascii=False),
                                                                        ',' if i + 1 < len(items) else '') for i, p in enumerate(items)] + ['  ],']
        at = next((i for i in range(start, end) if lines[i].startswith('  %s: [' % group)), None)
        if at is None:
            lines[end - 1] = lines[end - 1].rstrip().rstrip(',') + ','
            lines[end:end] = text
        else:
            close = next(i for i in range(at, end) if lines[i].startswith('  ]'))
            lines[at:close + 1] = text
    return '\n'.join(lines)


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
            # The section's last row carries the section's close: the close moves to the new last row, or the new row
            # lands outside the section and every allRows() on the page throws (6 Nov 2026, pad_sdl_fallback).
            closed = re.match(r"(.*\))\s*\] \},\s*$", lines[ai])
            if closed:
                lines[ai] = closed.group(1) + ','
                new[-1] = new[-1].rstrip().rstrip(',') + ' ] },'
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


def binding_entry(spec, path):
    """The spec's {tap, hold, dbl} of a binding path (patch['bindings'])."""
    parts = path.split('/')
    node = spec['bindings']
    for p in parts:
        if not isinstance(node, dict) or p not in node:
            sys.exit('binding %s not in the spec' % path)
        node = node[p]
    return node


def patch_bindings_js(src, patch):
    """patch['bindings'] in the mock-up source: the const FLIGHT_<PRESET> block, or the context's line of COMMON."""
    lines = src.split('\n')
    for path, fields in patch.get('bindings', {}).items():
        parts = path.split('/')
        if parts[0] == 'presets' and len(parts) == 4 and parts[2] == 'flight':
            start = next((i for i, l in enumerate(lines) if l.startswith('const FLIGHT_%s = {' % parts[1])), None)
            end = None if start is None else next(i for i in range(start, len(lines)) if lines[i].startswith('};'))
        elif parts[0] == 'common' and len(parts) == 3:
            top = next(i for i, l in enumerate(lines) if l.startswith('const COMMON = {'))
            start = next((i for i in range(top, len(lines)) if lines[i].startswith('  %s: {' % parts[1])), None)
            end = start
        else:
            sys.exit('binding path %s: presets/<PRESET>/flight/<input> or common/<context>/<input>' % path)
        if start is None:
            sys.exit('binding %s not in the mock-up source' % path)
        inp = parts[-1]
        hits = 0
        for i in range(start, end + 1):
            def rep(mm):
                args = re.findall(r"'([^']*)'", mm.group(1))
                b = dict(zip(('tap', 'hold', 'dbl'), args + ['none'] * (3 - len(args))))
                b.update(fields)
                vals = [b['tap'], b['hold'], b['dbl']]
                while len(vals) > 1 and vals[-1] == 'none':
                    vals.pop()
                return "%s: Bd(%s)" % (inp, ', '.join(js_literal(v) for v in vals))
            lines[i], n = re.subn(r"\b%s: Bd\(([^)]*)\)" % re.escape(inp), rep, lines[i])
            hits += n
        if hits != 1:
            sys.exit('binding %s: %d matches in the mock-up source' % (path, hits))
    return '\n'.join(lines)


def patch_spec(doc, patch):
    spec = doc['spec'] if 'spec' in doc and 'tabs' in doc['spec'] else doc
    for path, fields in patch.get('bindings', {}).items():
        binding_entry(spec, path).update(fields)
    if 'retab' in patch:
        pool = {r['key']: r for t in spec['tabs'] for s in t['sections'] for r in s['rows']}
        for r in patch.get('new_rows', []):
            pool[r['key']] = full_row(r)
        placed = [k for t in patch['retab'] for s in t['sections'] for k in s['rows']]
        missing = sorted(set(pool) - set(placed))
        twice = sorted({k for k in placed if placed.count(k) > 1})
        unknown = sorted(set(placed) - set(pool))
        if missing or twice or unknown:
            sys.exit('retab: rows left out %s, placed twice %s, unknown %s' % (missing, twice, unknown))
        tabs = []
        for t in patch['retab']:
            tab = {k: v for k, v in t.items() if k != 'sections'}
            tab['sections'] = [dict({k: v for k, v in s.items() if k != 'rows'}, rows=[pool[k] for k in s['rows']]) for s in t['sections']]
            tabs.append(tab)
        spec['tabs'] = tabs
    spec.get('presets', {}).update(patch.get('presets', {}))
    # remove_rows: a setting that left the game (11 Nov 2026, movie_intro). A row already gone is fine (the spec file
    # and the owner's document may be at different points); a row still under a parent or bundle is not checked here.
    for key in patch.get('remove_rows', []):
        for t in spec['tabs']:
            for sec in t['sections']:
                sec['rows'] = [r for r in sec['rows'] if r['key'] != key]
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
        if 'prompts' in p:
            st['prompts'] = [{'keys': q[0], 'label': q[1], 'v': q[2] if len(q) > 2 else ''} for q in p['prompts']]
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
    # top: whole spec-only data blocks the page does not render (e.g. hud2_presets, Q7): set / replace the key; the
    # page's applySpec keeps unknown top-level keys and saves them back. The page's own keys are refused (use the ops above).
    for key, value in patch.get('top', {}).items():
        if key in ('tabs', 'presets', 'bindings', 'screens', 'decisions', 'bar', 'device', 'notes', 'v', '_about'):
            sys.exit('top: %s is a key the page owns' % key)
        spec[key] = value
    return doc


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('patch')
    ap.add_argument('--js')
    ap.add_argument('--spec')
    ap.add_argument('--out')
    a = ap.parse_args()
    patch = json.load(open(a.patch, encoding='utf-8'))
    if 'retab' in patch and not (a.js and a.spec):
        sys.exit('retab needs --js and --spec: the mock-up takes its TABS block from the patched spec')
    doc = None
    if a.spec:
        doc = json.load(open(a.spec, encoding='utf-8'))
        doc = patch_spec(doc, patch)
        out = a.out or a.spec
        json.dump(doc, open(out, 'w', encoding='utf-8', newline='\n'), ensure_ascii=False, indent=1)
        print('patched', out)
    if a.js:
        src = open(a.js, encoding='utf-8').read()
        src = patch_bindings_js(add_rows_js(remove_rows_js(src, patch), patch), patch)
        src = presets_js(patch_js(src, patch), patch)
        if 'retab' in patch:
            src = retab_js(src, doc['spec'] if 'spec' in doc and 'tabs' in doc['spec'] else doc)
        open(a.js, 'w', encoding='utf-8', newline='\n').write(src)
        print('patched', a.js)


if __name__ == '__main__':
    main()
