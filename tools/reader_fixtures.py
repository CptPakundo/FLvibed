"""Golden fixtures for the legs.tsv readers (audit D5, 31 Oct 2026: ~9 harness bugs came from readers misreading the
data - stop-at rows taken for bests, a rotated sprt A, per-reader regexes and no shared fixtures).

tools\\fixtures\\reader_rows.tsv holds hand-made legs.tsv rows with the expected class of each (want_full, want_stopat).
Checks:
  1. splits.py full_pass_row / stopat_row (THE definitions) give each row its expected class;
  2. the readers that re-implement full_pass_row (tools\\legtable.js, tools\\flgate.ps1) still contain its three tests
     (outcome PASS, detail starting SUCCESS, an HDE instance '^\\d*$') - a static check: a reader that drops one fails.
Run it after any edit of a legs.tsv reader:  research\\tools\\ocr\\.venv\\Scripts\\python.exe -I tools\\reader_fixtures.py
Prints one line per failure and a summary; exit 1 on a failure.
"""
import csv
import importlib.util
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))


def load_splits():
    spec = importlib.util.spec_from_file_location('splits', os.path.join(HERE, 'splits.py'))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def main():
    bad = []
    sp = load_splits()
    with open(os.path.join(HERE, 'fixtures', 'reader_rows.tsv'), encoding='utf-8', newline='') as f:
        rows = list(csv.DictReader(f, delimiter='\t'))
    for r in rows:
        full, stop = int(bool(sp.full_pass_row(r))), int(bool(sp.stopat_row(r)))
        if full != int(r['want_full']) or stop != int(r['want_stopat']):
            bad.append(f"splits.py {r['label']} ({r['why']}): full {full} stopat {stop}, "
                       f"want {r['want_full']} / {r['want_stopat']}")
    # the re-implementations: each must still hold the three tests of full_pass_row
    tests = {
        'legtable.js': ["r.outcome === 'PASS'", "startsWith('SUCCESS')", '/^\\d*$/'],
        'flgate.ps1': ["outcome -eq 'PASS'", "detail -like 'SUCCESS*'", "instance -match '^\\d*$'"],
    }
    for name, needles in tests.items():
        with open(os.path.join(HERE, name), encoding='utf-8') as f:
            src = f.read()
        for n in needles:
            if n not in src:
                bad.append(f'{name}: the full-pass test {n!r} is gone (keep it equal to splits.py full_pass_row)')
    for b in bad:
        print('FAIL ' + b)
    print(f'reader fixtures: {"OK" if not bad else "FAILED"} ({len(rows)} rows, {len(bad)} failure(s))')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
