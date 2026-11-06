"""hud2t.py - the targeting display as a comparison of concepts, for the owner: an overview, a sheet per concept, then the best of each in the hard cases.

    research\\tools\\ocr\\.venv\\Scripts\\python.exe -I tools\\uimock\\hud2t.py

Real frames under drawn marks; the game's own bracket, label and bars are painted out where a version replaces them. Nothing here is decided or built.
Written to research\\evidence\\hud2_gate\\decisions\\Target_*.jpg. Background: docs\\research\\target-info-display.md.
"""
import math
import os
import sys

import cv2
import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageFilter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hud2d as d  # noqa: E402
import hud2f as f  # noqa: E402
import hud2g as g  # noqa: E402
import hud2s as hs  # noqa: E402
import hudmock as hm  # noqa: E402

K4, DECK = g.K4, g.DECK
INK, CHROME, ACTIVE, AMBER, HOSTILE = g.INK, g.CHROME, g.ACTIVE, g.AMBER, g.HOSTILE
SHIELD, HULL = (78, 132, 255), (214, 62, 62)
NAVY = (6, 8, 22)
T = 0.024      # the floor: every word a version adds


def rgb(p):
    return Image.open(p).convert('RGB')


# ---- painting the game's marks out
def heal(im, mask, r):
    a = cv2.cvtColor(np.asarray(im.convert('RGB')), cv2.COLOR_RGB2BGR)
    out = cv2.inpaint(a, mask.astype(np.uint8) * 255, r, cv2.INPAINT_TELEA)
    return Image.fromarray(cv2.cvtColor(out, cv2.COLOR_BGR2RGB)).convert('RGBA')


def rect(H, c, x0, y0, x1, y1):
    return int(c[0] + x0 * H), int(c[1] + y0 * H), int(c[0] + x1 * H), int(c[1] + y1 * H)


def erase_bracket(p, c, k_ring, on_meter=False):
    """The game's bracket corners, label, level digit, numbers and bars round the bracket's centre `c`. HUD 2's own ring, meter and speed, and the exhaust, are
    masked with them, so nothing bleeds from them, and put back afterwards."""
    im = Image.open(p).convert('RGB')
    a = np.asarray(im).astype(int)
    Hh, Ww = a.shape[:2]
    r, gg, b = a[..., 0], a[..., 1], a[..., 2]
    grow = max(2, int(Hh * 0.0016))
    k = np.ones((grow * 2 + 1, grow * 2 + 1), np.uint8)
    wide = max(2, int(Hh * 0.0034))
    kw = np.ones((wide * 2 + 1, wide * 2 + 1), np.uint8)
    mask = np.zeros(a.shape[:2], np.uint8)

    def zone(box, m):
        x0, y0, x1, y1 = box
        z = np.zeros_like(mask)
        z[y0:y1, x0:x1] = 1
        return z & m.astype(np.uint8)
    reddish = (r > gg + 18) & (r > b + 18)
    for sx, sy in ((-1, -1), (1, -1), (-1, 1), (1, 1)):
        xs, ys = sorted((sx * 0.066, sx * 0.137)), sorted((sy * 0.028, sy * 0.098))
        mask |= cv2.dilate(zone(rect(Hh, c, xs[0], ys[0], xs[1], ys[1]), reddish), kw)
    white = (r > 95) & (gg > 95) & (b > 95)
    words = np.zeros_like(mask)
    words |= zone(rect(Hh, c, -0.09, -0.112, 0.09, -0.05), white)
    words |= zone(rect(Hh, c, 0.05, -0.095, 0.112, -0.03), white)
    words |= zone(rect(Hh, c, -0.138, 0.045, -0.028, 0.098), white)
    words |= zone(rect(Hh, c, 0.05, 0.045, 0.138, 0.098), white)
    mask |= cv2.dilate(words, kw)      # their dark outline as well
    x0, y0, x1, y1 = rect(Hh, c, -0.052, 0.05, 0.058, 0.096)
    barpx = ((r > gg + 30) & (r > b + 30)) | ((b > r + 30) & (b > gg + 30))
    if on_meter:
        mask[y0:y1, x0:x1] = 1         # the bars and their dark bed, whose edge the meter crosses
    else:
        mask |= cv2.dilate(zone((x0, y0, x1, y1), barpx), k)
    # what stays
    yy, xx = np.mgrid[0:Hh, 0:Ww]
    R = 0.10 * k_ring * Hh
    rad = np.hypot(xx - Ww / 2, yy - Hh / 2)
    band = 0.0075 * Hh
    sat = ((r > 150) & (gg < 110) & (b < 120)) | ((b > 170) & (r < 130))
    ring = (np.abs(rad - R) < band) & sat & ~((np.abs(xx - Ww / 2) < 0.075 * Hh) & (yy > Hh / 2))
    ring = cv2.morphologyEx(ring.astype(np.uint8), cv2.MORPH_OPEN, np.ones((3, 3), np.uint8)) > 0
    stay = ring.copy()
    box = np.zeros(a.shape[:2], bool)
    e = int(0.03 * Hh)
    box[y0 - e:y1 + e, x0 - e:x1 + e] = True
    if on_meter:
        core = np.zeros(a.shape[:2], bool)
        core[y0:y1, int(c[0] - 0.032 * Hh):x1] = True
        keep = (box & (r > 200) & (gg > 165) & (b < 130)) | (core & (r > 200) & (gg > 200) & (b > 200))
        stay |= keep
        mask |= (box & barpx).astype(np.uint8) | cv2.dilate(keep.astype(np.uint8), kw)
    out = np.asarray(heal(im, (mask > 0) | stay, max(3, int(Hh * 0.003))).convert('RGB')).copy()
    out[stay] = np.asarray(im)[stay]
    return Image.fromarray(out).convert('RGBA')


def erase_group_marks(im, region):
    """The game's white marks on the group's ships, and nothing else: small thin bright L shapes."""
    a = np.asarray(im.convert('RGB')).astype(int)
    Hh = im.height
    x0, y0, x1, y1 = region
    bright = ((a[..., 0] > 170) & (a[..., 1] > 170) & (a[..., 2] > 170)).astype(np.uint8)
    n, lab, st, _ = cv2.connectedComponentsWithStats(bright[y0:y1, x0:x1], connectivity=8)
    mask = np.zeros(a.shape[:2], np.uint8)
    for i in range(1, n):
        x, y, w, h, area = st[i]
        if 0.0018 * Hh < max(w, h) < 0.016 * Hh and area / float(w * h) < 0.8:
            mask[y0:y1, x0:x1][lab == i] = 1
    mask = cv2.dilate(mask, np.ones((3, 3), np.uint8))
    return heal(im, mask, 3)


def erase_box(im, box, dx):
    """A box refilled with the sky `dx` pixels away, its edge feathered, so the stars go on."""
    x0, y0, x1, y1 = box
    m = Image.new('L', (x1 - x0, y1 - y0), 0)
    e = max(3, (y1 - y0) // 8)
    ImageDraw.Draw(m).rectangle((e, e, x1 - x0 - e, y1 - y0 - e), fill=255)
    m = m.filter(ImageFilter.GaussianBlur(e / 2))
    out = im.copy()
    out.paste(im.crop((x0 + dx, y0, x1 + dx, y1)), (x0, y0), m)
    return out


# ---- the scenes
class Scene:
    def __init__(self, tag, size, src, clean, k, ring_cells, targets, crop, wide):
        self.tag, self.size, self.src, self.clean, self.k = tag, size, src, clean, k
        self.cells, self.targets, self.crop, self.wide = ring_cells, targets, crop, wide
        self.W, self.H = size
        self.cx, self.cy, self.R = self.W / 2, self.H / 2, 0.10 * k * self.H


FAR = [('1', dict(sel=True)), ('2', {}), ('3', {}), ('M', {}), ('T', {})]
LIT = [('1', dict(sel=True, reach=True)), ('2', dict(reach=True)), ('3', {}), ('M', dict(reach=True)), ('T', dict(reach=True))]


def tgt(x, y, half, name, dist, sh=1.0, hu=1.0, cls='FIGHTER  LV 1'):
    return dict(x=x, y=y, half=half, name=name, dist=dist, sh=sh, hu=hu, cls=cls)




def save_im(im, n):
    os.makedirs(d.TMP, exist_ok=True)
    p = os.path.join(d.TMP, n + '.png')
    im.convert('RGBA').save(p)
    return p


def build():
    fr, f2 = g.frame, d.f2
    S = {}
    a, b = fr(K4, 'fight__size_normal_pad'), fr(DECK, 'fight__size_normal_pad')
    S['one'] = [Scene('4K', K4, a, save_im(erase_bracket(a, (1363, 1136), 0.875), 't_one4'), 0.875, FAR, [tgt(1358, 1118, 0.030, 'Ken Norris', '495 m')], (700, 680, 2500, 1420), (700, 0, 3840, 1420)),
                Scene('1280x800', DECK, b, save_im(erase_bracket(b, (526, 450), 0.79), 't_one8'), 0.79, FAR, [tgt(526, 446, 0.030, 'Ken Norris', '522 m')], (250, 250, 900, 515), (250, 0, 1280, 515))]
    S['one'][0].card, S['one'][1].card = (2868, 200), (943, 62)
    a, b = f2(K4, 'bracket_900_pad'), f2(DECK, 'bracket_900_pad')
    S['ring'] = [Scene('4K', K4, a, save_im(erase_bracket(a, (1913, 1160), 1.0, True), 't_ring4'), 1.0, LIT, [tgt(1905, 1150, 0.030, 'Alex Klamczynski', '496 m')], (1240, 640, 2600, 1440), (1240, 0, 3840, 1440)),
                 Scene('1280x800', DECK, b, save_im(erase_bracket(b, (633, 409), 1.08, True), 't_ring8'), 1.08, LIT, [tgt(632, 407, 0.030, 'Ken Norris', '504 m', 0.26)], (390, 232, 890, 526), (390, 0, 1280, 526))]
    S['ring'][0].card, S['ring'][1].card = (2776, 210), (884, 70)
    a, b = fr(K4, 'fight__pad'), fr(DECK, 'fight__size_k100_pad')
    ia = erase_box(erase_group_marks(Image.open(a).convert('RGBA'), (1650, 800, 2150, 960)), (3480, 960, 3790, 1110), -340)
    ib = erase_box(erase_group_marks(Image.open(b).convert('RGBA'), (520, 200, 720, 275)), (850, 300, 962, 362), -120)
    S['three'] = [Scene('4K', K4, a, save_im(ia, 't_three4'), 1.0, [], [tgt(1915, 890, 0.042, 'Ken Norris', '225 m'), tgt(1761, 878, 0.024, 'Eric Noonan', '374 m'), tgt(2030, 886, 0.024, 'Andrew Farhat', '610 m')], (1240, 640, 2720, 1210), (1200, 0, 3840, 1400)),
                  Scene('1280x800', DECK, b, save_im(ib, 't_three8'), 0.90, [], [tgt(608, 250, 0.042, 'Ken Norris', '137 m'), tgt(547, 231, 0.024, 'Eric Noonan', '571 m'), tgt(679, 250, 0.024, 'Andrew Farhat', '688 m')], (350, 140, 870, 362), (380, 0, 1280, 500))]
    S['three'][0].card, S['three'][1].card = (2776, 300), (924, 70)
    return S


# ---- the marks
def line(s, pts, col, w, edge=True):
    q0 = max(1.5, w)

    def fn(dr, P, q):
        pp = [P(*p) for p in pts]
        if edge:
            dr.line(pp, fill=NAVY + (200,), width=max(2, int((q0 + max(2, s.H * 0.0012)) * q)))
        dr.line(pp, fill=col + (255,), width=max(1, int(q0 * q)))
    xs, ys = [p[0] for p in pts], [p[1] for p in pts]
    pad = q0 * 3 + 4
    s.shape(fn, box=(min(xs) - pad, min(ys) - pad, max(xs) + pad, max(ys) + pad))


def ticks(s, t, col=HOSTILE, scale=1.0):
    h = s.h(t['half']) * scale
    arm = h * 0.42
    w = max(1.6, s.h(0.0020))
    for sx in (-1, 1):
        for sy in (-1, 1):
            x, y = t['x'] + sx * h, t['y'] + sy * h
            line(s, [(x - sx * arm, y), (x, y), (x, y - sy * arm)], col, w)


def pips(s, x, y, w, frac, col, n=12, h=0.005):
    s.segbar(x, y, w, s.h(h), frac, col, n=n, gap=max(1.0, s.h(0.0012)))


def bars2(s, x, y, w, t, n=12):
    pips(s, x, y, w, t['sh'], SHIELD, n)
    pips(s, x, y + s.h(0.008), w, t['hu'], HULL, n)


def parens(s, t, r=None, n=7):
    """The ring's own grammar, small: shield to the left, hull to the right."""
    r = r if r is not None else s.h(t['half']) * 1.28
    w = max(2.0, s.h(0.0034))
    s.arcbar(t['x'], t['y'], r, 138, 222, w, t['sh'], SHIELD, n=n, gapdeg=4, reverse=True)
    s.arcbar(t['x'], t['y'], r, -42, 42, w, t['hu'], HULL, n=n, gapdeg=4, reverse=True)


def side_of(sc, t):
    return -1 if t['x'] < sc.cx else 1


# concept 1: on the ship
def v1a(s, sc, ts):
    t = ts[0]
    parens(s, t)
    s.text((t['x'], t['y'] + s.h(t['half']) * 1.28 + s.h(0.020)), t['dist'], s.h(T), fill=INK, anchor='mm')


def v1b(s, sc, ts):
    t = ts[0]
    parens(s, t)
    r = s.h(t['half']) * 1.28
    s.text((t['x'], t['y'] - r - s.h(0.018)), t['name'], s.h(T), fill=INK, anchor='mm')
    s.text((t['x'], t['y'] + r + s.h(0.018)), t['dist'], s.h(T), fill=INK, anchor='mm')


def v1c(s, sc, ts):
    t = ts[0]
    h = s.h(t['half'])
    y = t['y'] - h * 1.15
    d_ = s.h(0.011)

    def fn(dr, P, q):
        dr.polygon([P(t['x'] - d_, y - d_), P(t['x'] + d_, y - d_), P(t['x'], y + d_ * 0.6)], fill=HOSTILE + (255,), outline=NAVY + (255,))
    s.shape(fn, box=(t['x'] - d_ - 3, y - d_ - 3, t['x'] + d_ + 3, y + d_ + 3))
    pips(s, t['x'] - h * 0.9, t['y'] + h * 1.1, h * 1.8, t['hu'], HULL, 8)


# concept 2: a tag on one side
def v2a(s, sc, ts, side=None):
    t = ts[0]
    ticks(s, t)
    sd = side if side is not None else side_of(sc, t)
    x = t['x'] + sd * (s.h(t['half']) + s.h(0.014))
    an = 'rm' if sd < 0 else 'lm'
    y = t['y'] - s.h(0.032)
    s.text((x, y), t['name'], s.h(T), fill=INK, anchor=an)
    s.text((x, y + s.h(0.028)), t['dist'], s.h(T), fill=INK, anchor=an)
    w = s.h(0.105)
    bars2(s, x - w if sd < 0 else x, y + s.h(0.046), w, t)


def v2b(s, sc, ts):
    t = ts[0]
    ticks(s, t, scale=0.8)
    sd = side_of(sc, t)
    x = t['x'] + sd * (s.h(t['half']) * 0.8 + s.h(0.012))
    txt = t['name'] + '   ' + t['dist']
    s.text((x, t['y']), txt, s.h(T), fill=INK, anchor='rm' if sd < 0 else 'lm')


def v2c(s, sc, ts):
    t = ts[0]
    ticks(s, t, scale=0.8)
    sd = side_of(sc, t)
    w, h = s.h(0.235), s.h(0.098)
    gap = s.h(t['half']) * 0.8 + s.h(0.014)
    x0 = t['x'] - gap - w if sd < 0 else t['x'] + gap
    y0 = t['y'] - h / 2
    s.plate((x0, y0, x0 + w, y0 + h), chamfer=s.h(0.010), alpha=205, rim_alpha=120, accent=('l', HOSTILE) if sd > 0 else None)
    if sd < 0:
        s.shape(lambda dr, P, q: dr.rectangle([P(x0 + w - s.h(0.003), y0), P(x0 + w, y0 + h)], fill=HOSTILE + (255,)), box=(x0 + w - s.h(0.004), y0, x0 + w, y0 + h))
    tx = x0 + s.h(0.012)
    r = x0 + w - s.h(0.012)
    s.text((tx, y0 + s.h(0.019)), 'HOSTILE', s.h(0.021), fill=HOSTILE, shadow=False)
    s.text((r, y0 + s.h(0.019)), t['cls'], s.h(0.021), fill=CHROME, anchor='rm', shadow=False)
    s.text((tx, y0 + s.h(0.047)), t['name'], s.h(T), fill=INK, shadow=False)
    s.text((r, y0 + s.h(0.047)), t['dist'], s.h(T), fill=INK, anchor='rm', shadow=False)
    bars2(s, tx, y0 + s.h(0.069), w - s.h(0.024), t, 18)


# concept 3: anchored to the reticle
def seat(s, sc, t, width=0.17):
    """Directly above the cue: name, then distance beside it, then the bars."""
    yb = sc.cy - sc.R * 0.93 - s.h(0.013) - s.h(0.012)
    x = sc.cx
    w = max(s.h(width), s.textlen(t['name'], s.h(T)) + s.textlen(t['dist'], s.h(T)) + s.h(0.026))
    bars2(s, x - w / 2, yb - s.h(0.013), w, t, 16)
    s.text((x - w / 2, yb - s.h(0.032)), t['name'], s.h(T), fill=INK, anchor='lm')
    s.text((x + w / 2, yb - s.h(0.032)), t['dist'], s.h(T), fill=INK, anchor='rm')


def v3a(s, sc, ts):
    ticks(s, ts[0], scale=0.8)
    seat(s, sc, ts[0])


def v3b(s, sc, ts):
    t = ts[0]
    ticks(s, t, scale=0.8)
    r = sc.R + s.h(0.024)
    w = max(2.0, s.h(0.0042))
    s.arcbar(sc.cx, sc.cy, r, 150, 210, w, t['sh'], SHIELD, n=10, gapdeg=1.6, reverse=True)
    s.arcbar(sc.cx, sc.cy, r, -30, 30, w, t['hu'], HULL, n=10, gapdeg=1.6, reverse=True)
    s.text((sc.cx + r + s.h(0.012), sc.cy - s.h(0.014)), t['dist'], s.h(T), fill=INK, anchor='lm')
    s.text((sc.cx + r + s.h(0.012), sc.cy + s.h(0.014)), t['name'], s.h(T), fill=INK, anchor='lm')


def v3c(s, sc, ts):
    t = ts[0]
    ticks(s, t, scale=0.8)
    ang = math.atan2(t['y'] - sc.cy, t['x'] - sc.cx)
    r = sc.R + s.h(0.016)
    ex, ey = sc.cx + math.cos(ang) * r, sc.cy + math.sin(ang) * r
    d_ = s.h(0.010)
    nx, ny = math.cos(ang), math.sin(ang)

    def fn(dr, P, q):
        dr.polygon([P(ex + nx * d_, ey + ny * d_), P(ex - ny * d_ * 0.7, ey + nx * d_ * 0.7), P(ex + ny * d_ * 0.7, ey - nx * d_ * 0.7)], fill=HOSTILE + (255,), outline=NAVY + (255,))
    s.shape(fn, box=(ex - d_ * 2, ey - d_ * 2, ex + d_ * 2, ey + d_ * 2))
    an = 'rm' if nx < 0 else 'lm'
    x = ex + nx * d_ * 1.6
    y = ey + ny * d_ * 1.6 - s.h(0.016)
    if math.hypot(t['x'] - ex, t['y'] - ey) < s.h(0.12):    # close to the ring it lifts clear of the ship
        y -= s.h(0.062)
    s.text((x, y), t['name'], s.h(T), fill=INK, anchor=an)
    s.text((x, y + s.h(0.028)), t['dist'], s.h(T), fill=INK, anchor=an)


# concept 4: linked by a line
def v4a(s, sc, ts):
    t = ts[0]
    ticks(s, t, scale=0.8)
    ex, ey = sc.card
    h = s.h(t['half']) * 0.8
    sx = 1 if ex > t['x'] else -1
    line(s, [(ex, ey), (t['x'] + sx * h, t['y'] - h)], CHROME, max(1.4, s.h(0.0012)))


def v4b(s, sc, ts):
    t = ts[0]
    ticks(s, t, scale=0.8)
    sd = side_of(sc, t)
    x = sc.cx + sd * (sc.R + s.h(0.13))
    y = sc.cy - s.h(0.15)
    w = s.h(0.17)
    x0 = x - w if sd < 0 else x
    s.text((x0, y), t['name'], s.h(T), fill=INK)
    s.text((x0 + w, y), t['dist'], s.h(T), fill=INK, anchor='rm')
    bars2(s, x0, y + s.h(0.020), w, t, 16)
    h = s.h(t['half']) * 0.8
    knee = (x0 + (w if sd < 0 else 0), y + s.h(0.040))
    line(s, [(x0, y + s.h(0.040)), (x0 + w, y + s.h(0.040))], CHROME, max(1.4, s.h(0.0012)))
    tx, ty = t['x'], t['y'] - h
    line(s, [knee, (tx + (knee[0] - tx) * 0.3, ty - s.h(0.03)), (tx, ty)], CHROME, max(1.4, s.h(0.0012)))


# ---- the sheets
VERSIONS = [
    ('1a', 'Own-ring echo', v1a, 'Small shield and hull arcs round the ship, the player\'s ring in miniature; distance under it.', 'Diegetic, sparse, round the ship. No name at the target: the card has it.'),
    ('1b', 'Echo with words', v1b, 'The same arcs, the name above, the distance below.', 'Diegetic, dense, spread round. Words stand on both sides of the ship, so more sky is taken.'),
    ('1c', 'Caret', v1c, 'An attitude caret over the ship, one row of hull pips under it, no words.', 'Diegetic, the sparsest. Everything else is a look at the card.'),
    ('2a', 'Tag, no plate', v2a, 'Ticks; name, distance and two pip bars on the side away from the ring. The research\'s form A.', 'One side, dense. Outlined text over the scene: bright scenes still to prove.'),
    ('2b', 'One line', v2b, 'Small ticks; name and distance on one line.', 'One side, sparse. No health at the target.'),
    ('2c', 'Mini card', v2c, 'A small glass chip riding beside the ship: attitude, class, name, distance, bars.', 'Panel, dense. Certain on any background; the largest thing that moves, and it hides sky.'),
    ('3a', 'Seat over the cue', v3a, 'Small ticks on the ship; name, distance and bars in one fixed place above the cue.', 'Panel, fixed. Nothing moves with the ship but the ticks; the eye travels to the reticle, not the target.'),
    ('3b', 'Second ring', v3b, 'The target\'s shield and hull as a second pair of arcs outside the player\'s ring, words at the right.', 'Panel, dense, spread round the reticle. Two pairs of arcs to tell apart.'),
    ('3c', 'Bearing tag', v3c, 'A pointer on the ring at the target\'s bearing; name and distance beside it, sliding round as the bearing changes.', 'Panel, one side, sparse. Says where the target lies; moves when the bearing moves, not with every twitch of the ship.'),
    ('4a', 'Card tether', v4a, 'Small ticks; a hairline from the target card to the ship. No words added.', 'Panel, spread across the screen, the sparsest in new text. A long line across the scene.'),
    ('4b', 'Slot and line', v4b, 'Small ticks; a line to a tag in a fixed slot beside the ring. The research\'s form C.', 'Panel, spread. Words stay still, the line swings as the ship moves.'),
]
CONCEPTS = [
    ('1', 'On the ship', 'The information is worn by the ship: diegetic, no plate.', ('1a', '1b', '1c')),
    ('2', 'A tag on one side', 'The information rides beside the ship, on the side away from the ring.', ('2a', '2b', '2c')),
    ('3', 'Anchored to the reticle', 'The ship gets only a small mark; the words stay where the eye already is.', ('3a', '3b', '3c')),
    ('4', 'Linked by a line', 'The words stay in a panel; a line says which ship they mean.', ('4a', '4b')),
]
BEST = {'1': '1b', '2': '2a', '3': '3a', '4': '4a'}
VMAP = {v[0]: v for v in VERSIONS}


def render(sc, key):
    """A version on a scene; returns the image and the share of the 4K frame it changed."""
    if key == 'today':
        s = hs.Skin(sc.src)
    else:
        s = hs.Skin(sc.clean)
    f.cue(s, sc.k, sc.cells)
    base = Image.open(d.save(s, 'tb_' + sc.tag)).convert('RGB')
    if key != 'today':
        VMAP[key][2](s, sc, sc.targets)
    im = Image.open(d.save(s, 'tv_' + sc.tag)).convert('RGB')
    diff = np.asarray(ImageChops.difference(im, base)).sum(2) > 24
    return im, float(diff.mean()) * 100


def crop(im, box):
    return im.crop(box)


def concept_sheets(S):
    shares = {}
    for num, title, why, keys in CONCEPTS:
        rows = []
        for k in keys:
            v = VMAP[k]
            pair = []
            for sc in S['one']:
                im, sh = render(sc, k)
                box = sc.wide if k.startswith('4') else sc.crop
                pair.append(crop(im, box))
                if sc.size == K4:
                    shares[k] = sh
            rows.append((f'{k}. {v[1]}', f'{v[3]} {v[4]} Adds {shares[k]:.2f} % of the screen.', pair, False))
        d.sheet(f'Target_{num}_{title.lower().replace(" ", "_")}', f'Concept {num}. {title}', why, rows,
                'Left 4K, right 1280x800. Real: scene, ship, ring, card, rows. Painted out: the game\'s bracket, label and bars. Drawn: the version and the weapon cue. Frames at k 0.875 / 0.79.')
    return shares


def tile_sheet(name, title, question, tiles, foot, cols=2):
    """tiles: (label, lines, [images]) in a grid; each tile holds its images side by side."""
    ft, fq, fl, fn = hm.font(50), hm.font(34), hm.font(34), hm.font(26)
    W, pad = d.W, 16
    cw = (W - pad * (cols + 1)) // cols
    probe = ImageDraw.Draw(Image.new('RGB', (8, 8)))
    made = []
    for label, text, ims in tiles:
        n = len(ims)
        w = (cw - 6 * (n - 1)) // n
        rs = [d.fit(i.convert('RGB'), w) for i in ims]
        lines = d.wrap(probe, text, fn, cw)
        hgt = 40 + max(r.height for r in rs) + 8 + 30 * len(lines) + 14
        made.append((label, lines, rs, hgt))
    ql = d.wrap(probe, question, fq, W - 2 * pad)
    fl_ = d.wrap(probe, foot, fn, W - 2 * pad)
    y = 20 + 58 + 42 * len(ql) + 16
    pos = []
    for i in range(0, len(made), cols):
        row = made[i:i + cols]
        for j, m in enumerate(row):
            pos.append((m, pad + j * (cw + pad), y))
        y += max(m[3] for m in row)
    img = Image.new('RGB', (W, y + 30 * len(fl_) + 18), d.BG)
    dr = ImageDraw.Draw(img)
    dr.text((pad, 20), title, fill=(255, 226, 120), font=ft)
    yy = 78
    for t in ql:
        dr.text((pad, yy), t, fill=(226, 240, 252), font=fq)
        yy += 42
    for (label, lines, rs, hgt), x, y0 in pos:
        dr.text((x, y0), label, fill=(200, 225, 245), font=fl)
        xx = x
        for r in rs:
            img.paste(r, (xx, y0 + 40))
            xx += r.width + 6
        ty = y0 + 40 + max(r.height for r in rs) + 8
        for t in lines:
            dr.text((x, ty), t, fill=(160, 195, 225), font=fn)
            ty += 30
    yy = y + 4
    for t in fl_:
        dr.text((pad, yy), t, fill=(130, 160, 190), font=fn)
        yy += 30
    img.save(os.path.join(d.OUT, name + '.jpg'), quality=87)
    print(name, img.size)


def overview(S, shares):
    tiles = []
    ims = [crop(render(sc, 'today')[0], sc.crop) for sc in S['one']]
    tiles.append(('Today', 'The game\'s bracket, label and bars, with HUD 2 round them; the cue, ruled but not built, is drawn in.', ims))
    for v in VERSIONS:
        ims = [crop(render(sc, v[0])[0], sc.wide if v[0].startswith('4') else sc.crop) for sc in S['one']]
        tiles.append((f'{v[0]}. {v[1]}', f'{v[3]} Adds {shares[v[0]]:.2f} %.', ims))
    tile_sheet('Target_0_overview', 'Targeting display: four concepts', 'The same fighter at about 500 m, every version beside today\'s. Each tile: 4K left, 1280x800 right.', tiles,
               'Real: scene, ship, ring, card. Drawn: the version, the weapon cue. The game\'s bracket, label and bars painted out. Nothing here is decided. Words at the 0.024 H floor. Costs: share of the 4K screen the version changes.')


def hard(S):
    for case, name, title, q, foot in (
            ('ring', 'Target_5_inside_the_ring', 'Hard case: the target inside the ring', 'The fight\'s commonest moment. The best version of each concept, the cue lit.',
             'Left 4K, right 1280x800. Real: scene, target, ring, card. Painted out: the game\'s bracket, label and bars. Drawn: the version and the cue. "Best" is this sheet\'s pick to draw, not a decision.'),):
        rows = [('Today', 'The label stands on the cue\'s row, and the bars on the meter.', [crop(render(sc, 'today')[0], sc.crop) for sc in S[case]], False)]
        for num, ttl, why, keys in CONCEPTS:
            k = BEST[num]
            rows.append((f'{k}. {VMAP[k][1]}', RING_NOTE[k], [crop(render(sc, k)[0], sc.wide if k.startswith('4') else sc.crop) for sc in S[case]], False))
        d.sheet(name, title, q, rows, foot)


RING_NOTE = {
    '1b': 'The small arcs stand inside the player\'s own: two pairs of parentheses, and the distance lands on the meter and the speed.',
    '2a': 'A tag that rides stays on the target, so it lies across the player\'s arc. The research parks it, which is 3a.',
    '3a': 'Nothing shares the cue\'s row; the words are the same in every fight.',
    '4a': 'The line runs from the card down through the ring\'s top.',
}


def three(S):
    def on(sc, draw, wide=False):
        s = hs.Skin(sc.clean)
        draw(s, sc, sc.targets)
        return crop(Image.open(d.save(s, 'th_' + sc.tag)).convert('RGB'), sc.wide if wide else sc.crop)

    def c1(s, sc, ts):
        v1b(s, sc, ts)
        for t in ts[1:]:
            parens(s, t)

    def c2(s, sc, ts):
        t0 = ts[0]
        ticks(s, t0)
        x = t0['x']
        y = t0['y'] + s.h(t0['half']) + s.h(0.022)
        s.text((x, y), t0['name'], s.h(T), fill=INK, anchor='mm')
        s.text((x, y + s.h(0.026)), t0['dist'], s.h(T), fill=INK, anchor='mm')
        bars2(s, x - s.h(0.05), y + s.h(0.044), s.h(0.10), t0)
        for t, sd in ((ts[1], -1), (ts[2], 1)):
            ticks(s, t, scale=0.8)
            xx = t['x'] + sd * (s.h(t['half']) * 0.8 + s.h(0.012))
            yy = t['y'] - s.h(0.040)
            s.text((xx, yy), t['name'] + '   ' + t['dist'], s.h(T), fill=INK, anchor='rm' if sd < 0 else 'lm')

    def c3(s, sc, ts):
        v3a(s, sc, ts)
        for t in ts[1:]:
            ticks(s, t, scale=0.8)

    def c4(s, sc, ts):
        v4a(s, sc, ts)
        ex = sc.card[0]
        ry = ROWS[sc.size]
        for t, y in zip(ts[1:], ry):
            ticks(s, t, scale=0.8)
            h = s.h(t['half']) * 0.8
            line(s, [(ex, y), (t['x'] + h, t['y'] - h)], CHROME, max(1.4, s.h(0.0012)))

    ROWS = {K4: (698, 788), DECK: (236, 265)}
    rows = [('Today', 'Only the target carries a mark; the rows say who, not where.', [crop(rgb(sc.src), sc.wide) for sc in S['three']], False)]
    for k, fn_, note in (('1b', c1, 'Every ship wears its arcs, the target its words too. Three pairs of arcs at once.'),
                         ('2a', c2, 'The target\'s tag, and one line for each other ship, kept apart by going left, right and under.'),
                         ('3a', c3, 'The seat holds the target only; the others get small ticks, and the rows still name them. At 4K this group flies right under the seat.'),
                         ('4a', c4, 'A line from each row to its ship: which row is which ship, at the price of three lines.')):
        rows.append((f'{k}. {VMAP[k][1]}', note, [on(sc, fn_, k == '4a') for sc in S['three']], False))
    d.sheet('Target_6_three_ships', 'Hard case: three ships close together', 'The best version of each concept on a tight group above the ring.', rows,
            'Left 4K, right 1280x800. No real frame holds three hostiles in clear view: these three are the player\'s own group standing in for hostiles, the middle one really the armoured transport. Their white marks and the off-screen arrow are painted out, and Ken Norris is placed on the middle ship. The cue is left out: at 4K this group stands exactly on its row. Real: scene, ships, ring, card, rows. Drawn: the version.')


def main():
    S = build()
    shares = concept_sheets(S)
    overview(S, shares)
    hard(S)
    three(S)


if __name__ == '__main__':
    main()
