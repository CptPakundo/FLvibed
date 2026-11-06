"""core\\game is one instance (docs\\MODULES_PLAN.md P1): the proof, from the release link's map.

A core header that ends up inside a module's namespace gives that module its own copy of every inline variable in it, and
nothing fails: the copy just never moves (a frozen game clock, a stale mode). So this reads build\\release\\FLvibed.map and
checks that
  1. no logic module holds anything in a namespace that also exists at global scope (a module's copy of a core namespace);
  2. the state that must exist once does: gameclock, deckmode, the exe:: symbols, the dll:: virtuals, at global scope, once.
The host's own exe:: copy (namespace flv_host: the host has its instance of core\\hook.cpp, the logic its own) is reported, not
refused; there is one.
Usage: python -I tools\\core_once.py [map]   exit 0 = holds. Prints the facts tools\\fingerprint.ps1 stores as core_once.txt.
"""
import os
import re
import sys

MUST_BE_ONE = {
    'gameclock': '?g_nowSec@gameclock@@',
    'deckmode': '?g_on@deckmode@@',
    'exe': '?FrameUpdate@exe@@',
    'dll': '?ObjGetPosition@dll@@',
}
MODULES = ('flv_subtitles_impl', 'flv_controller_impl', 'flv_npc_impl', 'flv_speedrun_impl', 'flv_movies_impl', 'flv_autopilot_impl', 'flv_hud_impl')


def scopes(symbol):
    """The enclosing scopes of a mangled name, innermost first; None for what is not a plain ?name@scope@@ form."""
    m = re.match(r'\?[A-Za-z_][A-Za-z0-9_]*@((?:[A-Za-z_][A-Za-z0-9_]*@)*)@', symbol)
    return None if not m else [s for s in m.group(1).split('@') if s]


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(root, 'build', 'release', 'FLvibed.map')
    symbols = set()
    for line in open(path, encoding='latin-1'):
        m = re.match(r'\s*[0-9a-f]{4}:[0-9a-f]{8}\s+(\?\S+)', line)
        if m:
            symbols.add(m.group(1))
    if not symbols:
        print('no symbols read from ' + path)
        return 1

    plain, in_module = set(), {}
    for s in symbols:
        sc = scopes(s)
        if not sc:
            continue
        owner = [x for x in sc if x.startswith('flv_')]
        if not owner:
            plain.update(sc)
        elif owner[0] in MODULES and len(sc) > 1:
            in_module.setdefault(sc[-2], set()).add(owner[0])  # the namespace right inside the module's

    failed = False
    for name in sorted(n for n in in_module if n in plain):
        print('FAIL: namespace %s exists at global scope and in %s' % (name, ', '.join(sorted(in_module[name]))))
        failed = True
    for what, prefix in MUST_BE_ONE.items():
        here = sorted(s for s in symbols if s.startswith(prefix))
        copies = sorted(s for s in symbols if s.startswith(prefix[:-1]) and s not in here)
        logic = [s for s in copies if any(m in s for m in MODULES)]
        hosts = [s for s in copies if s not in logic]
        print('%-10s global %d, logic copies %d, host copies %d' % (what, len(here), len(logic), len(hosts)))
        if len(here) != 1 or logic:
            print('FAIL: %s must exist once, at global scope' % what)
            failed = True
    print('core_once: ' + ('FAILED' if failed else 'holds'))
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
