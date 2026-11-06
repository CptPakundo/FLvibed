"""hud2r.py - before and after sheets for the critique's recommendations, for the owner's rulings: one image per item, from frames already captured.

    research\\tools\\ocr\\.venv\\Scripts\\python.exe -I tools\\uimock\\hud2r.py [r4 r6 r8 r9 r10 r13 r16 r17 bottom]

Written to research\\evidence\\hud2_gate\\decisions\\. Each sheet says what is the game's frame and what is drawn.
"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hud2d as d  # noqa: E402
import hud2f as f  # noqa: E402
import hud2g as g  # noqa: E402
import hud2s as hs  # noqa: E402

K4, DECK = g.K4, g.DECK
INK, CHROME, ACTIVE, AMBER, HOSTILE = g.INK, g.CHROME, g.ACTIVE, g.AMBER, g.HOSTILE


def rgb(p):
    return Image.open(p).convert('RGB')


def keycap(s, x, y, word, size=0.024):
    """A key as HUD 2 draws it, left edge at x, centre at y; returns its right edge."""
    t = s.h(size)
    w = s.textlen(word, t) + s.h(0.022)
    hh = s.h(0.021)
    s.plate((x, y - hh, x + w, y + hh), chamfer=s.h(0.008), alpha=120, rim=CHROME, rim_alpha=240, corners=(1, 1, 1, 1))
    s.text((x + w / 2, y), word, t, fill=CHROME, anchor='mm', shadow=False)
    return x + w


def worded(s, x, y, key, word, col=INK):
    x = keycap(s, x, y, key)
    s.text((x + s.h(0.010), y), word, s.h(0.024), fill=col)
    return x + s.h(0.010) + s.textlen(word, s.h(0.024)) + s.h(0.028)


def paint(im, box, src=None):
    """A box refilled from its own surroundings: a wide blur, or the pixels of `src`."""
    im.paste((src or im.filter(ImageFilter.GaussianBlur(60))).crop(box), box[:2])


def fill_plate(im, box):
    """A box inside a plate: the plate's own colour, from just above it."""
    x0, y0, x1, y1 = box
    col = im.getpixel(((x0 + x1) // 2, y0 - 6))
    ImageDraw.Draw(im).rectangle(box, fill=col)


# ---- R4
def r4():
    w, h = K4
    a = Image.open(g.frame(K4, 'fight__kbm')).convert('RGBA')
    b = a.copy()
    paint(b, (170, 165, 1060, 272))
    fill_plate(b, (2980, 448, 3695, 540))
    s = hs.Skin(d.save(b, 'r4b'))
    y = s.h(0.098)
    x = worded(s, 200, y, '~', 'FREE FLIGHT', ACTIVE)
    yc = 494
    x = worded(s, 2990, yc, 'CTRL+R', 'NEXT')
    worded(s, x, yc, 'CTRL+T', 'CLEAR')
    b_ = d.save(s, 'r4b2')
    s = hs.Skin(d.save(b, 'r4c'))
    x = 200
    for i, (k_, wd) in enumerate((('~', 'FREE FLIGHT'), ('F2', 'GOTO'), ('F3', 'DOCK'), ('F4', 'FORMATION'), ('K', 'AUTOPILOT'))):
        x = worded(s, x, y, k_, wd, ACTIVE if i == 0 else INK)
    x = worded(s, 2990, yc, 'CTRL+R', 'NEXT')
    worded(s, x, yc, 'CTRL+T', 'CLEAR')
    c_ = d.save(s, 'r4c2')
    m = g.frame(K4, 'mission__kbm')
    s = hs.Skin(m)
    rows = [('Dan Mackenzie', '640 m'), ('Lee Whitaker', '810 m'), ('Bret Colson', '1.1 km')]
    x0, x1 = 2860, 3710
    yy = s.h(0.070)
    s.text((x1, yy), 'HOSTILES', s.h(0.024), fill=CHROME, anchor='rm')
    for i, (n, dist) in enumerate(rows):
        y2 = yy + s.h(0.046) * (i + 1)
        s.text((x0 + 60, y2), n, s.h(0.026), fill=INK)
        s.text((x1, y2), dist, s.h(0.026), fill=INK, anchor='rm')
        s.shape(lambda dr, P, q, y2=y2: dr.polygon([P(x0, y2 - 16), P(x0 + 36, y2 - 16), P(x0 + 18, y2 + 16)], fill=HOSTILE + (255,)), box=(x0 - 2, y2 - 20, x0 + 40, y2 + 20))
    m2 = d.save(s, 'r4d')

    def tl(p):
        return rgb(p).crop((0, 0, 1920, 420))

    def tr(p):
        return rgb(p).crop((2700, 0, 3840, 640))
    d.sheet('R4_km_flight', 'R4. K&M: words where the flight HUD has symbols', 'What changes on keyboard and mouse: one lit chip with its key instead of a row of small pictures; the whole row in words on a peek; words on the target buttons; a few threat rows when no target is held.',
            [('A. Today: the maneuver row', 'Five pictures beside keycaps larger than they are.', [tl(g.frame(K4, 'fight__kbm'))], False),
             ('B. One lit chip', 'The running maneuver only, in words.', [tl(b_)], False),
             ('B, on a peek', 'The whole row, worded, for the time of a peek.', [tl(c_)], False),
             ('A. Today: target actions', 'A key and a symbol.', [tr(g.frame(K4, 'fight__kbm'))], False),
             ('B. In words', 'NEXT, CLEAR, as the pad reads.', [tr(b_)], False),
             ('A. Today: no target held', 'Nothing says who is out there.', [tr(m)], False),
             ('B. Threat rows', 'Two to four, at floor size.', [tr(m2)], False)],
            'Real: the frames. Drawn: chip, keys, words, rows; the names and distances in the rows are examples. The pad does not change.')


# ---- R6
def keyline(src, scene, size, k):
    w, h = size
    a = np.asarray(rgb(src)).astype(np.int16)
    b = np.asarray(rgb(scene)).astype(np.int16)
    diff = np.abs(a - b).max(axis=2)
    yy, xx = np.mgrid[0:h, 0:w]
    r = np.hypot(xx - w / 2, yy - h / 2)
    R = 0.10 * k * h
    th = np.degrees(np.arctan2(yy - h / 2, xx - w / 2)) % 360
    arcs = ((th > 136) & (th < 224)) | (th < 44) | (th > 316) | ((th > 66) & (th < 114))
    aw = 0.0065 * k * h
    m = (diff > 46) & arcs & (np.abs(r - R) < aw)     # the arcs alone: a cloud that moved between two frames is no arc
    mi = Image.fromarray((m * 255).astype('uint8'))
    n = max(1, round(0.0011 * h))
    dil = np.asarray(mi.filter(ImageFilter.MaxFilter(2 * n + 1))) > 0
    out = a.copy()
    ring = dil & ~m
    out[ring] = (out[ring] * 0.12 + np.array((6, 7, 22)) * 0.88)
    return Image.fromarray(out.astype('uint8'))


def r6():
    opts = []
    for word, n in (('Over a planet', 'planet_full'), ('Over the sun', 'sun_full')):
        ims_a, ims_b = [], []
        for size, k in ((K4, 1.0), (DECK, 1.08)):
            src, sc = g.frame(size, n + '__pad'), g.frame(size, n + '__scene')
            ims_a.append(d.ring_crop(src, size, 0.17))
            ims_b.append(d.ring_crop(d.save(keyline(src, sc, size, k), f'r6{n}{size[0]}'), size, 0.17))
        opts.append((f'A. Today: {word.lower()}', 'Left 4K, right 1280x800.', ims_a, False))
        opts.append((f'B. With the navy keyline: {word.lower()}', 'A 1-2 px dark edge round every arc.', ims_b, False))
    d.sheet('R6_ring_keyline', 'R6. A navy edge on the ring', 'What changes: the ring\'s arcs get a thin dark edge, as text has its outline, so the fill level holds over bright scenes. No area added.', opts,
            'Real: the frames. B is made from them: the arcs found against the scene frame and edged, so the arcs themselves are the game\'s pixels. Measured today: under 3:1 over a planet; the power arc 1.02-1.43 on the sun.')


# ---- R8
def r8():
    w, h = K4
    box = (950, 323, 2895, 1512)
    src = g.no_box(K4, 'nn_inventory__pad')
    im = Image.open(src).convert('RGBA')
    win = im.crop(box)
    base = im.copy()
    paint(base, box)
    p = d.save(base, 'r8base')
    s = hs.Skin(p, False)
    s.scrim(0.78)
    top, bot = 0.115, 0.900
    x0, x1 = s.mx, s.WH - s.mx
    s.glass((x0, top, x1, bot), content=False, alpha=0.92, corners=(1, 0, 1, 0))
    wh = s.h(bot - top - 0.03)
    ww = int(win.width * wh / win.height)
    s.paste(win.resize((ww, int(wh)), Image.LANCZOS), (int(s.W / 2 - ww / 2), s.h(top + 0.015)))
    s.tabs('INVENTORY', True, names=['LOG', 'INVENTORY', 'MAP', 'STATUS', 'INFO'])
    s.lane([('D', 'ITEM'), ('A', 'USE / MOUNT'), ('X', 'FILTER'), ('B', 'CLOSE')])
    b = d.save(s, 'r8b')
    m = os.path.join(g.OUT, 'navmap_3840x2160_pad.png')
    d.sheet('R8_one_frame', 'R8. One frame for all five windows', 'What changes: LB / RB from the map to the inventory today jumps from a 0.80 H frame to a 0.55 H window, with another left edge, at 0.61 s a press. B keeps one top line, one left edge and one scrim.',
            [('Start: the map', '', [rgb(m)], False),
             ('A. Today: press R1 to INVENTORY', 'The window as built: smaller, centred, its own edges; a 0.61 s slide.', [rgb(os.path.join(g.OUT, 'strip_inventory_3840x2160_pad.png'))], False),
             ('B. The common frame', 'The same edges as the map; the window grows into it.', [rgb(b)], False)],
            'Real: the window\'s own picture, scaled here to the frame: in the build it would be pitch and fonts, not a zoom, and it is unproven (probe 18). Also needs a hook on 0x55E120 for the slide. Drawn: frame, strip, lane.')


# ---- R9
def recolour(src):
    im = np.asarray(rgb(src)).astype(np.int16).copy()
    r, gg, b = im[..., 0], im[..., 1], im[..., 2]
    mus = (r > 70) & (gg > 60) & (b < 95) & (r - b > 25)
    mus = mus & (r < 150) & (gg < 140)      # the block, not a bright room
    need = int(im.shape[1] * 0.10)

    def run(row):
        best = cur = 0
        for v in row:
            cur = cur + 1 if v else 0
            best = max(best, cur)
        return best
    soft = np.asarray(Image.fromarray((mus * 255).astype('uint8')).filter(ImageFilter.MaxFilter(5))) > 0   # the word must not break the run
    ys = np.array([y for y in range(im.shape[0]) if soft[y].sum() >= need and run(soft[y]) >= need])
    if len(ys) == 0:
        return Image.fromarray(im.astype('uint8')), None
    # the largest band
    bands = np.split(ys, np.where(np.diff(ys) > 3)[0] + 1)
    band = max(bands, key=len)
    y0, y1 = band[0], band[-1]
    xs = np.where(mus[y0:y1 + 1].sum(axis=0) > (y1 - y0) * 0.3)[0]
    x0, x1 = xs[0], xs[-1]
    sub = im[y0:y1 + 1, x0:x1 + 1]
    t = np.clip((sub[..., 2].astype(float) - sub[..., 0]) / 90.0 + 0.15, 0, 1)
    t = np.where(sub[..., 2] > sub[..., 0] - 6, t, 0)
    col = np.array(INK)
    sub[:] = sub * (1 - t[..., None]) + col * t[..., None]
    return Image.fromarray(im.astype('uint8')), (int(x0), int(y0), int(x1), int(y1))


def r9():
    opts = []
    for word, n in (('The pause menu', 'pause__pad'), ('The job board', 'jobboard__pad')):
        res = []
        for size in (K4, DECK):
            p = g.frame(size, n)
            im, b = recolour(p)
            h = size[1]
            box = (max(0, b[0] - int(h * 0.05)), max(0, b[1] - int(h * 0.09)), min(size[0], b[2] + int(h * 0.05)), min(h, b[3] + int(h * 0.09)))
            res.append((rgb(p).crop(box), im.crop(box)))
        opts.append((f'A. Today: {word.lower()}', 'The game\'s blue on the mustard: 3.6-3.7:1. Left 4K, right 1280x800.', [res[0][0], res[1][0]], False))
        opts.append((f'B. The word in ink: {word.lower()}', 'The block stays; 5.0-5.7:1.', [res[0][1], res[1][1]], False))
    d.sheet('R9_focus_word', 'R9. The focused word in a lighter colour', 'What changes: only the word on the focused row. The mustard block and its bar stay as built.', opts,
            'Real: the frames; B recolours the word\'s own pixels. The route reaches pause and list text (+0x348), not box buttons or rich text: those keep today\'s.')


# ---- R10
def card_dialog(size, deck, title, body, safe, act, amber, n, base='pause__pad'):
    s = hs.Skin(g.frame(size, base) if base == 'pause__pad' else g.front('mainmenu', size), deck)
    s.scrim(0.70)
    w, hh = (0.86, 0.270) if not deck else (0.92, 0.320)
    x0 = (s.WH - w) / 2
    y0 = 0.5 - hh / 2
    s.glass((x0, y0, x0 + w, y0 + hh), content=False, accent=('l', AMBER if amber else CHROME), alpha=0.92)
    tx = x0 + 0.040
    s.t(tx, y0 + 0.050 * s.ts, title, 0.040, INK, 'lm')
    s.t(tx, y0 + 0.104 * s.ts, body, 0.030, CHROME, 'lm')
    bw, bh = 0.30, 0.058 * s.ts
    by = y0 + hh - bh - 0.028
    s.chip((tx, by, tx + bw, by + bh), safe, focus=True)
    s.chip((tx + bw + 0.024, by, tx + 2 * bw + 0.024, by + bh), act)
    s.lane([('A', 'SELECT'), ('D', 'CHOOSE'), ('B', safe)])
    return d.save(s, n)


def r10():
    a = card_dialog(K4, False, 'QUIT TO THE MAIN MENU?', 'Progress since your last save will be lost.', 'KEEP PLAYING', 'QUIT ANYWAY', True, 'r10a')
    b = card_dialog(DECK, True, 'QUIT TO THE MAIN MENU?', 'Progress since your last save will be lost.', 'KEEP PLAYING', 'QUIT ANYWAY', True, 'r10b')
    c = card_dialog(K4, False, 'EXIT FREELANCER?', 'Back to the desktop.', 'STAY', 'EXIT GAME', False, 'r10c', base='menu')
    d.sheet('R10_dialog', 'R10. Dialogs in the alert card\'s shape', 'What changes: left-aligned, a coloured edge at the side, event, cost, then the answers, as the flight alert cards read; instead of the centred box. Amber only where progress can be lost.',
            [('A. Today\'s design: the centred box', '', [rgb(os.path.join(g.OUT, 'quit_3840x2160_pad.png'))], False),
             ('B. The calm alert card', 'The safe answer first, as the words read left to right: the game\'s order is action first, so this is a change to weigh.', [rgb(a)], False),
             ('B at 1280x800', '', [rgb(b)], False),
             ('B where nothing can be lost', 'No amber: a chrome edge.', [rgb(c)], False)],
            'Real: pause and menu behind. Drawn: the dialog, focus as built. Each dialog class still needs its own probe for the verbs.')


# ---- R13
def r13():
    src = d.f2(K4, 'bracket_900_kbm')
    im = Image.open(src).convert('RGBA')
    fill_plate(im, (3530, 188, 3690, 262))
    s = hs.Skin(d.save(im, 'r13'))
    s.text((3668, 222), '540 m', s.h(0.032), fill=INK, anchor='rm')
    b = d.save(s, 'r13b')

    def card(p):
        return rgb(p).crop((2700, 0, 3840, 560))

    def br(p):
        return d.ring_crop(p, K4, 0.13)
    d.sheet('R13_one_distance', 'R13. One distance for one target', 'What changes: the card reads centre to centre, the bracket the surface, so one ship shows two numbers. B shows the surface value in both, as the game does.',
            [('A. Today: the card says 559 m', '', [card(src)], False),
             ('... and the bracket says 540 m, in the same frame', 'Card larger in 12 of 12 pairs, by 7-20 m on fighters.', [br(src)], False),
             ('B. The card reads the game\'s surface value', 'The cue\'s reach test uses the same number, so card, bracket and cue agree.', [card(b)], False)],
            'No frame here has a capital ship with its bracket: there the gap is the ship\'s radius and could be hundreds of metres (an inference, unseen). Real: the frames. Drawn: the card\'s number.')


# ---- R16
def r16():
    src = g.frame(K4, 'equipment__pad')
    w, h = K4
    a = rgb(src)
    s = hs.Skin(src)
    s.focus((1640 / h, 445 / h, 2230 / h, 562 / h))
    b = d.save(s, 'r16b')
    im = Image.open(b).convert('RGBA')
    paint(im, (3280, 1600, 3840, 2080))
    s = hs.Skin(d.save(im, 'r16c'))
    y = 0.775
    for gl, word in (('D', 'SIDE'), ('A', 'BUY'), ('B', 'CLOSE')):
        s.glyph(gl, s.h(1.545), s.h(y), s.h(0.032))
        s.text((s.h(1.575), s.h(y)), word, s.h(0.026), fill=INK)
        y += 0.052
    b2 = d.save(s, 'r16d')

    def mid(p):
        return rgb(p).crop((1300, 180, 3840, 1200))

    def lane(p):
        return rgb(p).crop((2500, 1500, 3840, 2120))
    d.sheet('R16_hold_buy', 'R16. Buying without a hold', 'What changes: HOLD BUY on a row stays as it is. A second way is added: move the focus up to the game\'s BUY button and tap. Xbox\'s guideline asks for an alternative to a hold on a purchase.',
            [('A. Today: the focus on a row, HOLD BUY', 'The only way to buy on the pad.', [mid(src)], False),
             ('', 'Its prompt.', [lane(src)], False),
             ('B. The focus on BUY', 'One press up from the row; the hold on the row still works.', [mid(b2)], False),
             ('', 'A tap here buys; the lane says so.', [lane(b2)], False)],
            'Real: the dealer. Drawn: the focus on BUY, the lane. Real work: `PanelFocus` only reads focus today, and the line that blocks this tap is there on purpose.')


# ---- R17
def menu(plate):
    w, h = K4
    im = Image.open(g.front('mainmenu', K4)).convert('RGBA')
    soft = im.filter(ImageFilter.GaussianBlur(h * 0.02))
    b = (0, int(h * 0.24), int(w * 0.26), int(h * 0.86))
    im.paste(soft.crop(b), b[:2])
    s = hs.Skin(d.save(im, f'r17m{plate}'))
    x0, x1, bh, step, y0 = 0.044 * s.WH, 0.2275 * s.WH, 0.056, 0.090, 0.267
    items = ['CONTINUE', 'NEW GAME', 'LOAD GAME', 'MULTIPLAYER', 'OPTIONS', 'EXIT']
    if plate:
        s.glass((x0 - 0.016, y0 - 0.022, x1 + 0.016, y0 + 5 * step + bh + 0.022), content=False, alpha=0.70)
    for i, c in enumerate(items):
        s.chip((x0, y0 + i * step, x1, y0 + i * step + bh), c, focus=i == 0)
    return d.save(s, f'r17n{plate}')


def loading(bar, n):
    p = os.path.join(d.TMP, 'navy.png')
    Image.new('RGBA', K4, (6, 7, 22, 255)).save(p)
    s = hs.Skin(p)
    w = 0.60
    x0 = (s.WH - w) / 2
    s.t(s.WH / 2, 0.470, 'PLANET MANHATTAN', 0.046, INK, 'mm')
    s.small(s.WH / 2, 0.515, 'NEW YORK  ·  LIBERTY', CHROME, 0.026, 'mm')
    if bar:
        s.segbar(s.h(x0), s.h(0.552), s.h(w), s.h(0.012), bar, CHROME, n=30)
    return d.save(s, n)


def r17():
    d.sheet('R17_menu_and_loading', 'R17. The menu\'s plate, and the loading screen', 'Two small things. Does the main menu need a plate behind its buttons? And how should a load of 1.1-1.2 s look?',
            [('Menu A. With the column plate', '', [rgb(menu(1))], False),
             ('Menu B. Floating chips, no plate', 'The chips carry their own glass; the critique says the plate adds nothing.', [rgb(menu(0))], False),
             ('Loading A. Today, the game\'s', 'Real, its centre: the bar on black.', [rgb(g.front('loading', K4))], False),
             ('Loading B. The design: a bar after 500 ms', 'It stands for about 0.65 s of a 1.1 s load.', [rgb(loading(0.62, 'r17l1'))], False),
             ('Loading C. The place at once, a bar only past 1.5 s', 'Most loads end before the bar; a slow machine still sees one.', [rgb(loading(0, 'r17l2'))], False)],
            'Real: the menu\'s scene, logo, the native load. Drawn: chips, plate, the loading text. Multiplayer\'s tables stay with the page rule in every case.')


# ---- the bottom of the reticle
def arc_c(s, k, frac, r, half, n, col, wd):
    cx, cy = s.W / 2, s.H / 2
    span = 2 * half / n
    gap = 2.2
    lit = half * frac

    def fn(dr, P, q):
        for i in range(n):
            b0, b1 = 90 - half + i * span + gap / 2, 90 - half + (i + 1) * span - gap / 2
            c0, c1 = P(cx - r - wd / 2, cy - r - wd / 2), P(cx + r + wd / 2, cy + r + wd / 2)
            dr.arc([c0, c1], b0, b1, fill=(0, 0, 0, 170), width=max(1, int(wd * q)))
            if min(b1, 90 + lit) > max(b0, 90 - lit):
                dr.arc([c0, c1], max(b0, 90 - lit), min(b1, 90 + lit), fill=col + (255,), width=max(1, int(wd * q)))
    s.shape(fn, box=(cx - r, cy, cx + r, cy + r + 2 * wd))


def form(base, size, k, deck, kind, thr, spd):
    s = hs.Skin(base, deck)
    h = s.H
    cx, cy, R = s.W / 2, s.H / 2, s.h(0.10 * k)
    g.power_arc(s, k, 0.6)
    tw = 0.0065 * k * h * 0.75
    if kind == 'A':
        s.arcbar(cx, cy, R + 0.022 * k * h, 82, 98, tw, thr, hs.CHROME, n=5, gapdeg=2.2, reverse=True, empty=(40, 48, 80))
        y = R + 0.0494 * k * h
    elif kind == 'B':
        s.arcbar(cx, cy, R - 0.020 * k * h, 82, 98, tw, thr, hs.CHROME, n=5, gapdeg=2.2, reverse=True, empty=(40, 48, 80))
        y = R + 0.0285 * k * h
    else:
        arc_c(s, k, thr, R - 0.020 * k * h, 12.0, 6, CHROME, tw)
        y = R + 0.0285 * k * h
    s.text((cx, cy + y), str(spd), s.h(0.030 * k), fill=INK, anchor='mm')
    return d.ring_crop(d.save(s, 'rb'), size, 0.21, 0.56)


def bottom():
    base = f.speed_base(K4, 'nebula_full__pad', 1.0)
    bd = f.speed_base(DECK, 'nebula_full__pad', 1.08)
    d.sheet('Reticle_bottom_forms', 'The bottom of the reticle: meter, speed, throttle', 'The speed moves under the energy meter. Where does the throttle arc go, and which way does it fill? Each row: throttle 30 %, then full; then 1280x800.',
            [('A. Meter, throttle, speed', 'The throttle stays outside the ring; the speed goes under it. The ring\'s bottom drops 0.040 H.', [form(base, K4, 1.0, False, 'A', 0.3, 84), form(base, K4, 1.0, False, 'A', 1.0, 312), form(bd, DECK, 1.08, True, 'A', 0.3, 84)], False),
             ('B. Throttle inside, meter, speed', 'The shortest stack: the bottom drops 0.019 H. The throttle fills left to right.', [form(base, K4, 1.0, False, 'B', 0.3, 84), form(base, K4, 1.0, False, 'B', 1.0, 312), form(bd, DECK, 1.08, True, 'B', 0.3, 84)], False),
             ('C. As B, the throttle centred too', 'It fills from the middle out, so both bottom arcs follow one rule (the meter drains to the middle).', [form(base, K4, 1.0, False, 'C', 0.3, 84), form(base, K4, 1.0, False, 'C', 1.0, 312), form(bd, DECK, 1.08, True, 'C', 0.3, 84)], False)],
            'Real: scene, ring, ship. Drawn: meter, throttle, speed. In all three the speed stands over the ship\'s nose; the lower-centre rule moves with it.')


ALL = {'r4': r4, 'r6': r6, 'r8': r8, 'r9': r9, 'r10': r10, 'r13': r13, 'r16': r16, 'r17': r17, 'bottom': bottom}

if __name__ == '__main__':
    for n in (sys.argv[1:] or list(ALL)):
        ALL[n]()
