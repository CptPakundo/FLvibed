# Fits the speaker name colours in data\speaker_colors.tsv (docs\DISPLAY_PLAN.md "Speaker colours").
# The main cast keeps its authored colours; side characters and story speakers are fitted around their family's
# anchor (or the hostile red), moved as little as possible until the rules hold against the dialogue in
# data\subtitles.tsv (who speaks within 3 turns of whom in a mission or scene). Writes the colours back.
# Usage: research\tools\ocr\.venv\Scripts\python.exe -I subtitles\tools\speaker_colors.py [--check]
import collections, itertools, math, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
DATA = os.path.join(HERE, '..', 'data')
TABLE = os.path.join(DATA, 'speaker_colors.tsv')

def lin(c):
    c /= 255
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4

def oklab(rgb):
    r, g, b = lin(rgb >> 16 & 255), lin(rgb >> 8 & 255), lin(rgb & 255)
    l = (0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b) ** (1 / 3)
    m = (0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b) ** (1 / 3)
    s = (0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b) ** (1 / 3)
    return (0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s, 1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
            0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s)

def lch(L, C, h):
    while C >= 0:
        a, b = C * math.cos(math.radians(h)), C * math.sin(math.radians(h))
        l_, m_, s_ = L + 0.3963377774 * a + 0.2158037573 * b, L - 0.1055613458 * a - 0.0638541728 * b, L - 0.0894841775 * a - 1.2914855480 * b
        l, m, s = l_ ** 3, m_ ** 3, s_ ** 3
        rgb = (4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s, -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s,
               -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s)
        if all(-1e-4 <= c <= 1.0001 for c in rgb):
            out = 0
            for c in rgb:
                c = min(max(c, 0), 1)
                out = out << 8 | round((12.92 * c if c <= 0.0031308 else 1.055 * c ** (1 / 2.4) - 0.055) * 255)
            return out
        C -= 0.005
    return None

def dist(x, y):
    return math.dist(oklab(x), oklab(y))

def lum(rgb):
    return 0.2126 * lin(rgb >> 16 & 255) + 0.7152 * lin(rgb >> 8 & 255) + 0.0722 * lin(rgb & 255)

def contrast(c, navy=0x0D0B1A):
    return (lum(c) + 0.05) / (lum(navy) + 0.05)

# display.cpp NameColor: Trent amber, friendly green, neutral warm white, hostile red, Neural Net mint; the HUD's
# selected-target yellow.
TRENT, FRIENDLY, NEUTRAL, HOSTILE, NN, TARGET = 0xFFD27A, 0x8DDB85, 0xF2E3B3, 0xFF7A6B, 0x8CF5A8, lch(0.90, 0.13, 108)
# family anchors (OKLCH hue, side lightness / chroma, story lightness / chroma): the main cast's colour temperature
FAMILY = {
    'Liberty': (240, 0.80, 0.10, 0.87, 0.07),   # blues
    'Bretonia': (210, 0.78, 0.09, 0.85, 0.06),  # steel
    'Kusari': (335, 0.78, 0.11, 0.85, 0.07),    # raspberry / mauve
    'Order': (190, 0.80, 0.10, 0.86, 0.07),     # teal
    'Rheinland': (220, 0.80, 0.06, 0.86, 0.05), # ice / slate
    'Criminal': (60, 0.80, 0.06, 0.84, 0.05),   # rust-tan (Lane Hackers, Rogues, Bounty Hunters when not hostile)
    'Civil': (85, 0.88, 0.04, 0.90, 0.035),     # warm near-white (Freeports, race officials, bars, news)
}
HOSTILE_ANCHOR = (0.72, 0.16, 30)

def load_table():
    head, rows, comments = None, [], []
    for line in open(TABLE, encoding='utf-8'):
        line = line.rstrip('\r\n')
        if line.startswith('#') or not line:
            comments.append(line)
            continue
        f = line.split('\t')
        if head is None:
            head = f
            continue
        rows.append(dict(zip(head, f + [''] * (len(head) - len(f)))))
    return comments, head, rows

def graph():
    lines = []
    for row in open(os.path.join(DATA, 'subtitles.tsv'), encoding='utf-8'):
        f = row.rstrip('\n').split('\t')
        if len(f) < 7 or f[2] != 'story' or not f[5]:
            continue
        m = re.match(r'dx_(m\d+|s\d+)', f[0], re.I)
        if m:
            lines.append((m.group(1).lower(), f[0].lower(), f[5]))
    lines.sort()
    seqs = collections.defaultdict(list)
    for g, _, s in lines:
        if not seqs[g] or seqs[g][-1] != s:
            seqs[g].append(s)
    W = collections.Counter()
    for seq in seqs.values():
        for i, a in enumerate(seq):
            for j in range(i + 1, min(len(seq), i + 4)):
                if seq[j] != a:
                    W[frozenset((a, seq[j]))] += 1.0 / (j - i)
    return W, {s for _, _, s in lines}

def hostile_default(row):
    return row['stance'].split()[0] == 'hostile'

def anchor(row):
    if row['tier'] == 'story' and hostile_default(row):
        return HOSTILE_ANCHOR
    h, Ls, Cs, Lg, Cg = FAMILY[row['family']]
    return (Ls, Cs, h) if row['tier'] == 'side' else (Lg, Cg, h)

def candidates(row):
    L0, C0, h0 = anchor(row)
    hostile = row['tier'] == 'story' and hostile_default(row)
    side = row['tier'] == 'side'
    hs = range(-15, 16, 5) if hostile else range(-40, 41, 5)
    out = []
    for dh in hs:
        for dL in (-0.10, -0.06, -0.03, 0, 0.03, 0.06, 0.09):
            for dC in (-0.03, -0.015, 0, 0.02, 0.04) + ((0.06,) if side else ()):
                # a faction keeps a visible tint (near-white names lost it: the Donau, the Prison Ship); civil may be pale
                civil = row['family'] == 'Civil'
                L, C = min(0.93 if civil else 0.90, L0 + dL), max(0.02 if civil else 0.045, C0 + dC)
                c = lch(L, C, h0 + dh)
                if c is None or contrast(c) < 6:
                    continue
                # the reserved colours: hostile reds may sit near the hostile colour (that is their meaning)
                near = [TRENT, FRIENDLY, NN, TARGET] + ([] if hostile else [HOSTILE])
                if min(dist(c, r) for r in near) < 0.08:
                    continue
                out.append((c, abs(dh) / 15 + abs(dL) / 0.04 + abs(dC) / 0.02))
    return out

def rules(a, b, col, tier, family, W):
    """Minimum distance a and b need (0: none)."""
    together = W.get(frozenset((a, b)), 0) > 0
    ta, tb = tier[a], tier[b]
    if together:
        named = sum(t in ('main', 'side') for t in (ta, tb))
        return (0.06, 0.08, 0.10)[named] # two characters 0.10; a character and a station / wing 0.08; two of those 0.06
    if ta in ('main', 'side') and tb in ('main', 'side'):
        if ta == tb == 'main':
            return 0.08
        return 0.06 if family.get(a) == family.get(b) else 0.04  # side characters: noticeably their own
    return 0

def main():
    check_only = '--check' in sys.argv
    comments, head, rows = load_table()
    W, spoken = graph()
    col = {'Trent': TRENT}
    tier = {'Trent': 'main'}
    family = {}
    for r in rows:
        tier[r['speaker']] = r['tier']
        family[r['speaker']] = r['family']
        if r['colour']:
            col[r['speaker']] = int(r['colour'], 16)
    fit = [] if check_only else [r for r in rows if r['tier'] in ('side', 'story')]
    cands = {r['speaker']: candidates(r) for r in fit}
    for r in fit:
        col[r['speaker']] = min(cands[r['speaker']], key=lambda x: x[1])[0]
    def violation(s, c):
        v = 0
        for n, cn in col.items():
            if n == s:
                continue
            need = rules(s, n, col, tier, family, W)
            if need and dist(c, cn) < need:
                v += (need - dist(c, cn)) * (1 + math.log1p(W.get(frozenset((s, n)), 0)))
        return v
    order = sorted(fit, key=lambda r: (r['tier'] != 'side', -sum(w for k, w in W.items() if r['speaker'] in k)))
    for _ in range(40):
        changed = False
        for r in order:
            s = r['speaker']
            # the rules first (any shortfall loses to any colour that keeps them), then the least move from the anchor
            def key(x):
                v = violation(s, x[0])
                return (v > 1e-9, v, x[1])
            best = min(cands[s], key=key)[0]
            if best != col[s]:
                col[s], changed = best, True
        if not changed:
            break
    bad = []
    for a, b in itertools.combinations(col, 2):
        need = rules(a, b, col, tier, family, W)
        if need and dist(col[a], col[b]) < need:
            bad.append(f'{a} / {b}: {dist(col[a], col[b]):.3f} < {need}')
    missing = [r['speaker'] for r in rows if r['speaker'] not in spoken and r['tier'] != 'story']
    unlisted = sorted(s for s in spoken if s not in col)
    print('violations:', '; '.join(bad) or 'none')
    print('listed but never in a story scene:', ', '.join(missing) or 'none')
    print('story speakers not listed (relation colours):', ', '.join(unlisted) or 'none')
    if check_only:
        return
    with open(TABLE, 'w', encoding='utf-8', newline='\n') as out:
        out.write('\n'.join(comments) + '\n' + '\t'.join(head) + '\n')
        for r in rows:
            r['colour'] = '%06X' % col[r['speaker']]
            out.write('\t'.join(r[h] for h in head) + '\n')
    print(f'wrote {len(rows)} rows to {TABLE}')

main()
