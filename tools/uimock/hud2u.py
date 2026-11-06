"""hud2u.py - the targeting display, round 2: the owner's refinement of hud2t.py's sheets. Concepts 3 and 4 are gone; concept 1 (worn by the ship) and concept 2 (card, then tag)
go on, with the target inside the ring and three close ships.

    research\\tools\\ocr\\.venv\\Scripts\\python.exe -I tools\\uimock\\hud2u.py

Real frames under drawn marks, as hud2t.py. Nothing here is decided or built. Written to research\\evidence\\hud2_gate\\decisions\\Target2_*.
"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hud2d as d  # noqa: E402
import hud2f as f  # noqa: E402
import hud2s as hs  # noqa: E402
import hud2t as tt  # noqa: E402

K4, DECK = tt.K4, tt.DECK
INK, CHROME, HOSTILE = tt.INK, tt.CHROME, tt.HOSTILE
SHIELD, HULL, NAVY, T = tt.SHIELD, tt.HULL, tt.NAVY, tt.T
GLASS = hs.h2.GLASS if hasattr(hs, 'h2') and hasattr(hs.h2, 'GLASS') else (14, 20, 44)
HOLD, EASE = 2.0, 0.35       # the card's stay and the change to the tag, in s: this sheet's proposal
SHOW = 3.0                   # how long a number stays after a hit, in s: this sheet's proposal


# ---- marks
def caret(s, x, y, size=0.011, col=HOSTILE):
    q = s.h(size)

    def fn(dr, P, k):
        dr.polygon([P(x - q, y - q), P(x + q, y - q), P(x, y + q * 0.7)], fill=col + (255,), outline=NAVY + (255,))
    s.shape(fn, box=(x - q - 3, y - q - 3, x + q + 3, y + q + 3))


def vbar(s, x, y0, hgt, frac, col, n=9, ghost=None):
    """Upright pips, filling from the bottom."""
    w = max(2.0, s.h(0.0048))
    gap = max(1.0, s.h(0.0014))
    sh = (hgt - gap * (n - 1)) / n
    lit = frac * n
    empty = (col[0] // 5 + 14, col[1] // 5 + 14, col[2] // 5 + 22)

    def fn(dr, P, k):
        for i in range(n):
            yb = y0 + hgt - i * (sh + gap)
            c = col + (255,) if i < lit else (255, 255, 255, 170) if ghost and i < ghost * n else empty + (210,)
            dr.rectangle([P(x - w / 2, yb - sh), P(x + w / 2, yb)], fill=c)
    s.shape(fn, box=(x - w, y0 - 2, x + w, y0 + hgt + 2))


def hbar(s, x, y, w, frac, col, n=12, ghost=None):
    s.segbar(x, y, w, s.h(0.005), frac, col, n=n, gap=max(1.0, s.h(0.0012)), ghost=ghost)


def c1(s, t, bars='h', name='c', dmg=None, words=True):
    """Concept 1, refined: the caret, straight bars, the words. dmg = (which, was): remaining shield or hull in numbers, and the piece just lost."""
    h = s.h(t['half'])
    x, y = t['x'], t['y']
    sh, hu = t['sh'], t['hu']
    gs = dmg[1] if dmg and dmg[0] == 's' else None
    gh = dmg[1] if dmg and dmg[0] == 'h' else None
    cy_ = y - h * 1.22
    caret(s, x, cy_)
    if bars == 'v':
        a = h * 2.2
        vbar(s, x - h * 1.45, y - a / 2, a, sh, SHIELD, ghost=gs)
        vbar(s, x + h * 1.45, y - a / 2, a, hu, HULL, ghost=gh)
        low = y + a / 2 + s.h(0.006)
        if dmg:
            sd = -1 if dmg[0] == 's' else 1
            s.text((x + sd * (h * 1.45 + s.h(0.012)), y), str(int((sh if dmg[0] == 's' else hu) * 100)), s.h(T), fill=SHIELD if dmg[0] == 's' else HULL, anchor='rm' if sd < 0 else 'lm')
    else:
        w = max(h * 2.7, s.h(0.095))
        hbar(s, x - w / 2, y + h * 1.12, w, sh, SHIELD, ghost=gs)
        hbar(s, x - w / 2, y + h * 1.12 + s.h(0.0085), w, hu, HULL, ghost=gh)
        low = y + h * 1.12 + s.h(0.016)
        if dmg:
            yy = y + h * 1.12 + (s.h(0.0025) if dmg[0] == 's' else s.h(0.011))
            s.text((x + w / 2 + s.h(0.010), yy), str(int((sh if dmg[0] == 's' else hu) * 100)), s.h(T), fill=SHIELD if dmg[0] == 's' else HULL, anchor='lm')
    if not words:
        return
    z = s.h(T)
    if name == 'a':       # the name over the caret, the distance under the bars
        s.text((x, cy_ - s.h(0.026)), t['name'], z, fill=INK, anchor='mm')
        s.text((x, low + s.h(0.016)), t['dist'], z, fill=INK, anchor='mm')
    elif name == 'b':     # all the words over the caret
        s.text((x, cy_ - s.h(0.026)), t['name'] + '   ' + t['dist'], z, fill=INK, anchor='mm')
    else:                 # all the words under the bars
        s.text((x, low + s.h(0.016)), t['name'] + '   ' + t['dist'], z, fill=INK, anchor='mm')


def lerp(a, b, e):
    return a + (b - a) * e


def card_tag(s, sc, t, p, ticks=True, sd=None):
    """Concept 2, refined: p = 0 the mini card, 1 the tag without a plate; between them the same words travel, the plate fades and shrinks."""
    e = p * p * (3 - 2 * p)
    if ticks:
        tt.ticks(s, t, scale=0.8)
    sd = sd if sd is not None else tt.side_of(sc, t)
    z = s.h(T)
    wn, wd = s.textlen(t['name'], z), s.textlen(t['dist'], z)
    w, h = s.h(0.235), s.h(0.098)
    gap = s.h(t['half']) * 0.8 + s.h(0.014)
    x0 = t['x'] - gap - w if sd < 0 else t['x'] + gap
    y0 = t['y'] - h / 2
    tx, r = x0 + s.h(0.012), x0 + w - s.h(0.012)
    xt = t['x'] + sd * gap
    bw = s.h(0.105)
    tag_box = (xt - max(wn, bw) if sd < 0 else xt, t['y'] - s.h(0.046), xt if sd < 0 else xt + max(wn, bw), t['y'] + s.h(0.036))
    box = tuple(lerp(a, b, e) for a, b in zip((x0, y0, x0 + w, y0 + h), tag_box))
    if e < 0.995:
        s.plate(box, chamfer=s.h(0.010), alpha=int(205 * (1 - e)), rim_alpha=int(120 * (1 - e)))
        ax = box[2] - s.h(0.003) if sd < 0 else box[0]
        al = int(255 * (1 - e))
        s.shape(lambda dr, P, q: dr.rectangle([P(ax, box[1]), P(ax + s.h(0.003), box[3])], fill=HOSTILE + (al,)), box=(ax - 2, box[1] - 2, ax + s.h(0.003) + 2, box[3] + 2))
    fade = max(0.0, 1 - e * 3.5)
    if fade > 0:    # the class line rides in the plate and goes first
        s.text((box[0] + s.h(0.012), box[1] + s.h(0.019)), 'HOSTILE', s.h(0.021), fill=HOSTILE, shadow=False, alpha=int(255 * fade))
        s.text((box[2] - s.h(0.012), box[1] + s.h(0.019)), t['cls'], s.h(0.021), fill=CHROME, anchor='rm', shadow=False, alpha=int(255 * fade))
    # the name lifts to its own line first, then the words slide in, so they never cross
    drop, slide = min(1.0, e / 0.45), max(0.0, (e - 0.35) / 0.65)
    nl = lerp(tx, xt - wn if sd < 0 else xt, e)
    dl = lerp(r - wd, xt - wd if sd < 0 else xt, slide)
    s.text((nl, lerp(y0 + s.h(0.047), t['y'] - s.h(0.032), drop)), t['name'], z, fill=INK)
    s.text((dl, lerp(y0 + s.h(0.047), t['y'] - s.h(0.004), drop)), t['dist'], z, fill=INK)
    bx = lerp(tx, xt - bw if sd < 0 else xt, e)
    bwid = lerp(w - s.h(0.024), bw, e)
    by = lerp(y0 + s.h(0.069), t['y'] + s.h(0.014), e)
    tt.bars2(s, bx, by, bwid, t, 14)


def block(s, x, y, sd, t):
    """The words and bars at a seat, growing away from the ring."""
    z = s.h(T)
    an = 'rm' if sd < 0 else 'lm'
    s.text((x, y - s.h(0.020)), t['name'] + '   ' + t['dist'], z, fill=INK, anchor=an)
    w = s.h(0.13)
    tt.bars2(s, x - w if sd < 0 else x, y - s.h(0.002), w, t, 14)


# ---- on a scene
def draw(sc, fn, box=None, behind=False):
    s = hs.Skin(sc.clean)
    if behind:
        fn(s, sc)
        s.paste(reticle_layer(sc))
        f.cue(s, sc.k, sc.cells)
    else:
        if sc.cells:
            f.cue(s, sc.k, sc.cells)
        fn(s, sc)
    return Image.open(d.save(s, 'u_' + sc.tag)).convert('RGB').crop(box or sc.crop)


def reticle_layer(sc):
    """The player's own ring, meter and speed, lifted from the frame, to stand over what is behind them."""
    im = Image.open(sc.clean).convert('RGBA')
    a = np.asarray(im.convert('RGB')).astype(int)
    r, g_, b = a[..., 0], a[..., 1], a[..., 2]
    Hh, Ww = a.shape[:2]
    yy, xx = np.mgrid[0:Hh, 0:Ww]
    rad = np.hypot(xx - Ww / 2, yy - Hh / 2)
    sat = ((r > 150) & (g_ < 110) & (b < 120)) | ((b > 170) & (r < 130))
    near = np.abs(xx - Ww / 2) < 0.075 * Hh
    ring = (np.abs(rad - sc.R) < 0.0075 * Hh) & sat & ~(near & (yy > Hh / 2))
    low = near & (yy > Hh / 2 + 0.03 * Hh) & (yy < Hh / 2 + sc.R + 0.03 * Hh)
    meter = low & (r > 190) & (g_ > 150) & (b < 140)
    speed = low & (np.abs(xx - Ww / 2) < 0.03 * Hh) & (r > 205) & (g_ > 205) & (b > 205)
    m = Image.fromarray(((ring | meter | speed) * 255).astype(np.uint8))
    out = im.copy()
    out.putalpha(m)
    return out


ONE4, ONE8 = (960, 830, 1800, 1400), (360, 322, 700, 556)


def pair(S, key, fn, boxes=None, behind=False):
    return [draw(sc, fn, boxes[i] if boxes else None, behind) for i, sc in enumerate(S[key])]


def tgt0(sc):
    return sc.targets[0]


# ---- 1. worn by the ship
def sheet1(S):
    rows = []
    for bars, bn in (('v', 'Upright bars at the sides'), ('h', 'Two bars under the ship')):
        for nm, nn in (('a', 'name over the caret, distance under'), ('b', 'all the words over the caret'), ('c', 'all the words under')):
            tag = ('V' if bars == 'v' else 'H') + nm
            rows.append((f'{tag}. {bn}; {nn}', PICKS.get(tag, ''), pair(S, 'one', lambda s, sc, b=bars, n=nm: c1(s, tgt0(sc), b, n), (ONE4, ONE8)), tag == 'Hc'))
    d.PICK = "this sheet's pick"
    hit = dict(sh=0.62, hu=1.0)
    hurt = dict(sh=0.0, hu=0.74)

    def dmg(s, sc, what, v, b, n):
        t = dict(tgt0(sc), **v)
        c1(s, t, b, n, what)
    rows.append(('After a hit on the shield', f'The remaining shield in numbers for {SHOW:.0f} s, faded over 0.4 s; another hit starts it again. White marks what was just lost.',
                 pair(S, 'one', lambda s, sc: dmg(s, sc, ('s', 0.80), hit, 'h', 'c'), (ONE4, ONE8)), False))
    rows.append(('After a hit on the hull', 'The same, for the hull, once the shield is down.', pair(S, 'one', lambda s, sc: dmg(s, sc, ('h', 0.86), hurt, 'h', 'c'), (ONE4, ONE8)), False))
    rows.append(('The same with upright bars', 'The number stands beside the bar it belongs to.', pair(S, 'one', lambda s, sc: dmg(s, sc, ('s', 0.80), hit, 'v', 'c'), (ONE4, ONE8)), False))
    d.sheet('Target2_1_on_the_ship', 'Worn by the ship, refined', 'Caret over the ship, straight bars, so the circle stays the player\'s. Both bar forms, three places for the words.', rows,
            f'Left 4K, right 1280x800. Real: scene, ship, ring. Painted out: the game\'s bracket, label and bars. Drawn: the marks and the cue. The {SHOW:.0f} s and 0.4 s are this sheet\'s proposal. Bars sized by eye from the ship\'s pixels.')


PICKS = {
    'Va': 'Words above and below, bars either side: the widest shape, it reads as a frame round the ship.',
    'Vb': 'The ship stands clear below the words; the words sit high, away from the bars.',
    'Vc': 'The upright bars and a line under them make a cup; the caret stands alone.',
    'Ha': 'Two lines of words apart, the bars between them and the ship: the tallest shape.',
    'Hb': 'Words on top, bars below, the ship between: it reads as two pieces.',
    'Hc': 'One block under the ship, the caret alone above. The ship stays bare from the sides and the top: the neatest shape, and it needs the least room beside the ship.',
}


# ---- 2. card to tag
def sheet2(S):
    rows = []
    stages = ((0.0, f'0 to {HOLD:.1f} s: the mini card', 'The new target, in full: attitude, class, name, distance, bars.'),
              (0.3, f'{HOLD + 0.1:.1f} s', 'The class line goes first, the plate thins, the name lifts to its own line.'),
              (0.6, f'{HOLD + 0.2:.1f} s', 'The words travel towards the ship, the plate draws in round them; they never cross.'),
              (1.0, f'From {HOLD + EASE:.2f} s: the tag', 'No plate: name, distance, two bars. It stays until the target changes.'))
    for p, lab, note in stages:
        rows.append((lab, note, pair(S, 'one', lambda s, sc, p=p: card_tag(s, sc, tgt0(sc), p)), False))
    d.sheet('Target2_2_card_to_tag', 'The mini card, then the tag', f'One thing in two stages: the card for {HOLD:.1f} s, a {EASE:.2f} s change, then the tag. A new target starts it again.', rows,
            f'Left 4K, right 1280x800. Real: scene, ship, ring. Drawn: the marks and the cue. Times are this sheet\'s proposal. Under `skin_motion` FADES the card fades to the tag where it stands; NONE cuts. Moving: Target2_2_card_to_tag.gif, on a still frame: the ship does not move in it.')
    frames, durs = [], []
    a, b = S['one']
    steps = [(0.0, 1500)] + [(i / 8, 44) for i in range(1, 8)] + [(1.0, 1700)]
    for p, ms in steps:
        ia = draw(a, lambda s, sc: card_tag(s, sc, tgt0(sc), p), (760, 800, 2120, 1400))
        ib = draw(b, lambda s, sc: card_tag(s, sc, tgt0(sc), p), (240, 300, 760, 530))
        ia = d.fit(ia, 640)
        ib = ib.resize((640, int(ib.height * 640 / ib.width)), Image.LANCZOS)
        fr = Image.new('RGB', (1290, max(ia.height, ib.height)), d.BG)
        fr.paste(ia, (0, 0))
        fr.paste(ib, (650, 0))
        frames.append(fr.quantize(colors=256, method=Image.Quantize.FASTOCTREE, dither=Image.Dither.NONE))
        durs.append(ms)
    frames[0].save(os.path.join(d.OUT, 'Target2_2_card_to_tag.gif'), save_all=True, append_images=frames[1:], duration=durs, loop=0)


# ---- 3. inside the ring
def seats(s, sc, t, which):
    sd = tt.side_of(sc, t)
    if which == 'flank':
        return sc.cx + sd * (sc.R + s.h(0.022)), sc.cy, sd
    return sc.cx + sd * s.h(0.10 * sc.k), sc.cy - sc.R * 0.93, sd    # outside the ring's low-shield and hull numbers (+-0.072 k)


def sheet3(S):
    rows = [('Today', 'The label on the cue\'s row, the bars on the meter.', [tt.crop(tt.render(sc, 'today')[0], sc.crop) for sc in S['ring']], False)]

    def c1_seat(s, sc, which):
        t = tgt0(sc)
        caret(s, t['x'], t['y'] - s.h(t['half']) * 1.22)
        x, y, sd = seats(s, sc, t, which)
        block(s, x, y, sd, t)

    def c2_seat(s, sc, which):
        t = tgt0(sc)
        tt.ticks(s, t, scale=0.8)
        x, y, sd = seats(s, sc, t, which)
        block(s, x, y, sd, t)

    rows.append(('Worn by the ship, behind the reticle', 'It stays on the ship, and the player\'s ring, meter and speed stand in front of it.', pair(S, 'ring', lambda s, sc: c1(s, tgt0(sc), 'h', 'c'), behind=True), False))
    rows.append(('Worn by the ship, seat at the flank', 'The caret stays on the ship; words and bars just outside the arc on the target\'s side, changing side only past the middle. Side alert cards start near here: they would need the same room.', pair(S, 'ring', lambda s, sc: c1_seat(s, sc, 'flank')), False))
    rows.append(('Worn by the ship, seat at the shoulder', 'The same, in line with the cue, above the arc\'s end: one row to read across.', pair(S, 'ring', lambda s, sc: c1_seat(s, sc, 'shoulder')), False))
    rows.append(('Tag, behind the reticle', 'Ticks and tag stay on the ship, the ring in front; the words are cut where an arc crosses them.', pair(S, 'ring', lambda s, sc: card_tag(s, sc, tgt0(sc), 1.0), behind=True), False))
    rows.append(('Tag, seat at the flank', 'Ticks on the ship, the tag beside the arc.', pair(S, 'ring', lambda s, sc: c2_seat(s, sc, 'flank')), False))
    rows.append(('Tag, seat at the shoulder', 'Ticks on the ship, the tag in line with the cue.', pair(S, 'ring', lambda s, sc: c2_seat(s, sc, 'shoulder')), False))
    d.sheet('Target2_3_inside_the_ring', 'Inside the ring: behind it, or a seat just outside', 'The circle stays the player\'s. No anchor at the target card.', rows,
            'Left 4K, right 1280x800. Real: scene, target, ring, meter, speed. Painted out: the game\'s bracket, label and bars. Drawn: the marks and the cue. Behind: the ring, meter and speed are lifted from the frame and laid over the marks. The move between ship and seat is not drawn.')


# ---- 4. three ships
FOURTH = {K4: (1886, 868), DECK: (620, 235)}


def nontarget(s, t, sd, form, concept):
    h = s.h(t['half'])
    x, y = t['x'], t['y']
    if concept == 1:
        caret(s, x, y - h * 1.15, 0.009)
    elif form == 'min':
        tt.line(s, [(x - h * 0.8, y - h * 0.4), (x - h * 0.8, y - h * 0.8), (x - h * 0.4, y - h * 0.8)], HOSTILE, max(1.6, s.h(0.0020)))
        tt.line(s, [(x + h * 0.4, y + h * 0.8), (x + h * 0.8, y + h * 0.8), (x + h * 0.8, y + h * 0.4)], HOSTILE, max(1.6, s.h(0.0020)))
    else:
        tt.ticks(s, t, scale=0.8)
    z = s.h(T)
    tx = x + sd * (h + s.h(0.010))
    an = 'rm' if sd < 0 else 'lm'
    if form == 'name':
        s.text((tx, y - h * 1.2), t['name'], z, fill=INK, anchor=an)
    elif form == 'dist':
        s.text((tx, y - h * 1.2), t['dist'], z, fill=INK, anchor=an)
    elif form == 'bar':
        w = h * 2.2
        hbar(s, x - w / 2, y + h * 1.1, w, t['hu'], HULL, 7)


def fourth(sc, s):
    x, y = FOURTH[sc.size]
    return dict(x=x, y=y, half=0.018, name='', dist='', sh=1, hu=1)


def sheet4(S):
    rows = [('Today', 'Only the target carries a mark; the others none.', [Image.open(sc.src).convert('RGB').crop(sc.crop) for sc in S['three']], False)]

    def scene(s, sc, concept, form):
        t0, l, r = sc.targets
        if concept == 1:
            c1(s, t0, 'h', 'c')
        else:
            tt.ticks(s, t0)
            z = s.h(T)
            y = t0['y'] + s.h(t0['half']) + s.h(0.022)
            s.text((t0['x'], y), t0['name'], z, fill=INK, anchor='mm')
            s.text((t0['x'], y + s.h(0.026)), t0['dist'], z, fill=INK, anchor='mm')
            tt.bars2(s, t0['x'] - s.h(0.0525), y + s.h(0.044), s.h(0.105), t0)
        nontarget(s, l, -1, form, concept)
        nontarget(s, r, 1, form, concept)
        nontarget(s, fourth(sc, s), 1, 'min', concept)

    forms = {1: (('name', 'name', 'Each other ship keeps its caret and its name, on its outer side. Where words would touch, that ship falls to the bare caret: the fourth ship here, tucked against the target.'),
                 ('dist', 'distance', 'The caret and the distance only: short enough to fit more crowds.'),
                 ('bar', 'one bar', 'The caret and one row of hull pips, no words.'),
                 ('min', 'bare caret', 'The caret alone on every other ship: the form when overlap cannot be avoided.')),
             2: (('name', 'name', 'Each other ship keeps small ticks and its name. Where words would touch, it falls to two opposite ticks: the fourth ship here.'),
                 ('dist', 'distance', 'Small ticks and the distance only.'),
                 ('min', 'bare ticks', 'Two opposite ticks on every other ship: the form when overlap cannot be avoided.'))}
    for c, lab in ((1, 'Worn by the ship'), (2, 'Tag')):
        for form, fl, note in forms[c]:
            rows.append((f'{lab}: others by {fl}', note, pair(S, 'three', lambda s, sc, c=c, form=form: scene(s, sc, c, form)), False))
    d.sheet('Target2_4_three_ships', 'Close ships: the target in full, the others minimized', 'The target keeps the whole form. Each other ship takes the richest form that touches nothing, and falls a step when it would. No anchor at the target card.', rows,
            'Left 4K, right 1280x800. No real frame holds three hostiles in clear view: these are the player\'s own group standing in for hostiles, the middle one really the armoured transport, and a fourth ship tucked against it. Their white marks and the off-screen arrow are painted out, and Ken Norris is placed on the middle ship. The cue is left out: at 4K this group stands exactly on its row. Real: scene, ships, ring. Drawn: the marks.')


def overview(S):
    one = S['one']
    hit = dict(sh=0.62)

    def tile(fn, key='one', boxes=None, behind=False):
        return pair(S, key, fn, boxes, behind)
    tiles = [
        ('Today', 'The game\'s bracket, label and bars.', [tt.crop(tt.render(sc, 'today')[0], b) for sc, b in zip(one, (ONE4, ONE8))]),
        ('Worn by the ship: bars under', 'Caret over the ship; two straight bars and one line of words under it. This sheet\'s pick.', tile(lambda s, sc: c1(s, tgt0(sc), 'h', 'c'), boxes=(ONE4, ONE8))),
        ('Worn by the ship: bars at the sides', 'Caret over the ship; upright bars either side, words under.', tile(lambda s, sc: c1(s, tgt0(sc), 'v', 'c'), boxes=(ONE4, ONE8))),
        ('After a hit', f'The remaining shield or hull in numbers for {SHOW:.0f} s.', tile(lambda s, sc: c1(s, dict(tgt0(sc), **hit), 'h', 'c', ('s', 0.80)), boxes=(ONE4, ONE8))),
        (f'Card, 0 to {HOLD:.0f} s', 'The new target in full.', tile(lambda s, sc: card_tag(s, sc, tgt0(sc), 0.0))),
        ('Card to tag', f'A {EASE:.2f} s change: the plate fades, the words travel.', tile(lambda s, sc: card_tag(s, sc, tgt0(sc), 0.6))),
        ('The tag', 'No plate, until the target changes.', tile(lambda s, sc: card_tag(s, sc, tgt0(sc), 1.0))),
        ('Inside the ring: behind', 'It stays on the ship, the ring in front.', tile(lambda s, sc: c1(s, tgt0(sc), 'h', 'c'), 'ring', behind=True)),
    ]

    def seat_fn(which):
        def fn(s, sc):
            t = tgt0(sc)
            caret(s, t['x'], t['y'] - s.h(t['half']) * 1.22)
            x, y, sd = seats(s, sc, t, which)
            block(s, x, y, sd, t)
        return fn
    tiles.append(('Inside the ring: seat at the flank', 'Words and bars just outside the arc.', tile(seat_fn('flank'), 'ring')))
    tiles.append(('Inside the ring: seat at the shoulder', 'In line with the cue.', tile(seat_fn('shoulder'), 'ring')))

    def crowd(form):
        def fn(s, sc):
            t0, l, r = sc.targets
            c1(s, t0, 'h', 'c')
            nontarget(s, l, -1, form, 1)
            nontarget(s, r, 1, form, 1)
            nontarget(s, fourth(sc, s), 1, 'min', 1)
        return fn
    tiles.append(('Close ships: others by name', 'The target in full, the others caret and name, the tucked ship a bare caret.', tile(crowd('name'), 'three')))
    tiles.append(('Close ships: bare marks', 'When overlap cannot be avoided.', tile(crowd('min'), 'three')))
    tt.tile_sheet('Target2_0_overview', 'Targeting display, round 2', 'The owner\'s two concepts, refined. Each tile: 4K left, 1280x800 right.', tiles,
                  'Real: scene, ships, ring. Drawn: the marks and the cue. The game\'s bracket, label and bars painted out. Nothing here is decided. Close ships: the player\'s own group standing in for hostiles.')


def main():
    S = tt.build()
    S['ring'][0].crop, S['ring'][1].crop = (1100, 640, 2740, 1440), (330, 232, 950, 526)    # room for a seat
    sheet1(S)
    sheet2(S)
    sheet3(S)
    sheet4(S)
    overview(S)


if __name__ == '__main__':
    main()
