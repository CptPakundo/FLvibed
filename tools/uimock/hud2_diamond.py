"""hud2_diamond.py - the art of the small waypoint diamond (modules\\hud\\docs\\UI_HUD2_SKIN.md 6.10, "still owed"): four drafts round the three attitude carets, for the owner.

    research\\tools\\ocr\\.venv\\Scripts\\python.exe -I tools\\uimock\\hud2_diamond.py <sheet.jpg>

A pixel mock: nothing here ran in game. The diamond is drawn from its distance field, so it is mirror-exact and stands on the same pixel grid as the frame under it; the caret and the words are the kit's
(hud2w.mark, hud2u.hbar). Frames are read from the main checkout's research\\evidence\\hud2_gate when this worktree holds none; nothing is written there. A gif of the breath is written beside the sheet.
"""
import math
import os
import sys
import tempfile

import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import hudmock as hm  # noqa: E402

if not os.path.isdir(os.path.join(hm.ROOT, 'research', 'evidence')):      # a worktree: the frames and the fonts live in the main checkout
    hm.ROOT = os.path.abspath(os.path.join(hm.ROOT, '..', 'FLvibed'))
    hm.EVID = os.path.join(hm.ROOT, 'research', 'evidence', 'controller_ui', 'hud_redesign')
    hm.FONTS = os.path.join(hm.ROOT, 'Install', 'Freelancer HD Edition', 'DATA', 'FONTS')

import hud2 as h2  # noqa: E402
import hud2d as d  # noqa: E402
import hud2g as g  # noqa: E402
import hud2t as tt  # noqa: E402
import hud2u as u  # noqa: E402
import hud2w as w  # noqa: E402
import hud2y as y_  # noqa: E402

d.TMP = os.path.join(tempfile.gettempdir(), 'hud2_diamond')
K4, DECK = g.K4, g.DECK
CHROME, ACTIVE, NAVY, INK, T = h2.CHROME, h2.ACTIVE, tt.NAVY, tt.INK, tt.T
HOSTILE, NEUTRAL, FRIEND = tt.HOSTILE, w.NEUTRAL, w.FRIEND
CARETS = (('hostile', HOSTILE, 'solid'), ('neutral', NEUTRAL, 'hollow'), ('friendly', FRIEND, 'open'))

R = 0.027        # the diamond's half-diagonal in H (hud2z.py, as the owner saw it)
CARET = 0.011    # the caret's half-width in H (hud2w.mark)
CLEAR = 0.008    # the raised case: the diamond's lower point stands this far over the object's edge
TOP, BOTTOM, STEADY = 1.0, 0.25, 0.60    # the glow at the top and the bottom of the breath, and under skin_motion FADES and NONE
PERIOD = 2.0     # 0.5 Hz
GLOW = 0.55      # the glow's peak opacity on the line
ARTS = ('A', 'B', 'C', 'D')
NAMES = {'A': 'A. Thin outline', 'B': 'B. Double hairline', 'C': 'C. Corner ticks', 'D': 'D. Filled, soft'}
PICK = 'A'


class Tile(h2.HUD):
    """The kit on a crop of a frame: h() stays the frame's, so everything falls on the frame's own pixels."""

    def __init__(self, im, fh):
        self.im = im.convert('RGBA')
        self.W, self.H = self.im.size
        self.fh = fh

    def h(self, f):
        return f * self.fh


def cover(dist, half):
    """Coverage of a stroke `half` wide each way at signed distance `dist`, one pixel of antialiasing."""
    return np.clip(half - np.abs(dist) + 0.5, 0.0, 1.0)


def over(s, col, a):
    lay = np.zeros(a.shape + (4,), np.uint8)
    lay[..., :3] = col
    lay[..., 3] = np.clip(a * 255 + 0.5, 0, 255).astype(np.uint8)
    s.im.alpha_composite(Image.fromarray(lay, 'RGBA'))


def field(s, x, y):
    yy, xx = np.mgrid[0:s.H, 0:s.W]
    return xx + 0.5 - x, yy + 0.5 - y


def diamond(s, x, y, art, lvl, col=CHROME):
    """(x, y) is the diamond's centre, on a pixel corner. A steady soft navy bed under the glow, so the breath has something to show on over a bright sky."""
    fh = s.fh
    r = R * fh
    dx, dy = field(s, x, y)
    ax, ay = np.abs(dx), np.abs(dy)
    e = (ax + ay - r) / math.sqrt(2.0)          # the distance from the diamond's line, out positive
    sig = 0.0065 * fh
    edge = max(1.0, 0.0007 * fh)
    wide = max(2.0, 0.0026 * fh)
    mask = np.ones_like(e)
    if art == 'A':
        lines = [(0.0, wide)]
    elif art == 'B':
        thin = max(1.0, 0.0011 * fh)
        gap = max(3.0, 0.0030 * fh)
        lines = [(gap / 2, thin), (-gap / 2, thin)]
    elif art == 'C':
        lines = [(0.0, max(2.0, 0.0030 * fh))]
        along = np.abs(ax - ay) / math.sqrt(2.0)                       # 0 at an edge's middle, r / sqrt 2 at a corner
        cut = r / math.sqrt(2.0) * (1.0 - 2 * 0.34)
        mask = np.clip(along - cut + 0.5, 0.0, 1.0)
    else:
        lines = []
    if art == 'D':
        inside = np.clip(0.5 - e / (0.0042 * fh), 0.0, 1.0)
        over(s, NAVY, 0.42 * inside)
        over(s, col, (0.14 + 0.40 * lvl) * inside * (0.55 + 0.45 * ((ax + ay) / r).clip(0, 1) ** 2))
        over(s, col, 0.55 * lvl * np.exp(-(np.maximum(e, 0.0) / sig) ** 2) * (e > 0))
        return
    far = np.min([np.abs(e - o) for o, _ in lines], axis=0)
    soft = mask
    if art == 'C':                                                     # the glow fades past a tick's end instead of stopping there
        soft = np.exp(-(np.maximum(cut - along, 0.0) / sig) ** 2)
    over(s, NAVY, 0.34 * soft * np.exp(-(far / (sig * 1.5)) ** 2))
    over(s, col, GLOW * lvl * soft * np.exp(-(far / sig) ** 2))
    navy = np.max([cover(e - o, wd / 2 + edge) for o, wd in lines], axis=0) * mask
    ink = np.max([cover(e - o, wd / 2) for o, wd in lines], axis=0) * mask
    over(s, NAVY, 0.9 * navy)
    over(s, col, ink)


def caret_in(s, x, y, art, lvl, col, form, dcol=CHROME):
    """The diamond at (x, y) and the caret with its centre of mass on the diamond's centre."""
    if art:
        diamond(s, x, y, art, lvl, dcol)
    w.mark(s, x, y + round(0.433 * s.h(CARET)), col, form)


def form(s, t, x, y, art, lvl, lift=False, dcol=CHROME):
    """The candidate's settled form by type (hud2y.form), the diamond round the caret. `lift`: the diamond's lower point clears the object's edge."""
    hh = s.h(t['half'])
    z = s.h(T)
    line = t['name'] + '   ' + t['dist']
    bw = max(hh * 2.7, s.h(0.095))
    y0 = y + hh * 1.12
    wy = y0 + (s.h(0.032) if t['bars'] else s.h(0.010))
    cy = round(y - hh * 1.22 - 0.433 * s.h(CARET))
    if lift:
        cy = round(y - hh - s.h(CLEAR) - s.h(R))
    caret_in(s, x, cy, art, lvl, t['col'], t['form'], dcol)
    if t['bars']:
        u.hbar(s, x - bw / 2, y0, bw, t['sh'], tt.SHIELD)
        u.hbar(s, x - bw / 2, y0 + s.h(0.0085), bw, t['hu'], tt.HULL)
    s.text((x, wy), line, z, fill=INK, anchor='mm')


def cut(im, fh, cx, cy, wd, ht, fn=None):
    """A wd x ht crop round (cx, cy), drawn on by fn(tile, x, y) with (x, y) the crop's own (cx, cy)."""
    x0, y0 = int(cx) - wd // 2, int(cy) - ht // 2
    s = Tile(im.crop((x0, y0, x0 + wd, y0 + ht)), fh)
    if fn:
        fn(s, int(cx) - x0, int(cy) - y0)
    return s.im.convert('RGB')


def nn(im, k):
    return im.resize((im.width * k, im.height * k), Image.NEAREST)


def frames():
    """Each scene at 4K and at 1280x800, the frame's own pixels; only the playthrough's battleship is 1080p, enlarged x2 for the 4K."""
    fight = [tt.erase_bracket(g.frame(K4, 'fight__size_normal_pad'), (1363, 1136), 0.875), tt.erase_bracket(g.frame(DECK, 'fight__size_normal_pad'), (526, 450), 0.79)]
    station = [Image.open(g.frame(z, 'mission__scene')).convert('RGB') for z in (K4, DECK)]
    planet = [Image.open(g.frame(z, 'planet_full__scene')).convert('RGB') for z in (K4, DECK)]
    planet = [no_green(planet[0], (1760, 980, 2080, 1200), 7), no_green(planet[1], (590, 355, 700, 440), 3)]
    o = y_.SHIP
    src = os.path.join(w.FOOT, o['src'][0] + o['src'][1])
    nd = w.paint(tt.save_im(w.paint(src, o['label']), 'dm_ship'), [o['dist_box']])
    gone = y_.no_diamond(nd, o)
    ship = [gone.convert('RGB').resize(K4, Image.LANCZOS), w.to800(gone)]
    return dict(fight=fight, station=station, planet=planet, ship=ship)


def no_green(im, box, grow):
    """The game's lane bracket, on the ring next to the one used here."""
    a = np.asarray(im.convert('RGB')).astype(int)
    m = np.zeros(a.shape[:2], np.uint8)
    x0, y0, x1, y1 = box
    r, gg, b = a[y0:y1, x0:x1, 0], a[y0:y1, x0:x1, 1], a[y0:y1, x0:x1, 2]
    m[y0:y1, x0:x1] = (gg > r + 30) & (gg > b + 30)
    import cv2
    m = cv2.dilate(m, np.ones((grow, grow), np.uint8))
    return tt.heal(im, m, 4).convert('RGB')


# Each target at 4K, then at 1280x800. half: the object's on-screen half size in H, floor 0.030, cap 0.10 (6.10 "Size").
TARGETS = [
    dict(key='fight', at=((1358, 1118), (526, 446)), half=0.030, name='Ken Norris', dist='495 m', bars=True, sh=1.0, hu=1.0, col=HOSTILE, form='solid', word='A ship, dark sky, hostile'),
    dict(key='station', at=((950, 960), (248, 372)), half=0.10, name='Fort Bush', dist='2.4 km', bars=False, col=FRIEND, form='open', word='A solar, dark sky, friendly'),
    dict(key='ship', at=((1056, 1236), (320, 458)), half=0.10, name='Nomad Battleship', dist='0 m', bars=True, sh=0.0, hu=0.66, col=HOSTILE, form='solid', word='A ship, bright sky, hostile'),
    dict(key='planet', at=((1839, 1086), (566, 401)), half=0.030, name='Trade Lane Ring', dist='1.2 km', bars=False, col=NEUTRAL, form='hollow', word='A solar, bright sky, neutral'),
]
SKY = {'dark': ('fight', (1180, 880), (420, 330)), 'bright': ('planet', (2230, 368), (750, 147))}


def sky_tile(F, which, size, n, art, lvl, col, fm, dcol=CHROME):
    key, a, b = SKY[which]
    i = 0 if size == K4 else 1
    return cut(F[key][i], size[1], *(a if i == 0 else b), n, n, lambda s, x, y: caret_in(s, x, y, art, lvl, col, fm, dcol))


def breath_depth(F, art):
    """How far the breath moves the pixels round the line, in grey levels of 255, at 4K: the mean over the band the glow covers."""
    out = []
    for which in ('dark', 'bright'):
        a = np.asarray(sky_tile(F, which, K4, 200, art, TOP, *CARETS[1][1:]).convert('L')).astype(float)
        b = np.asarray(sky_tile(F, which, K4, 200, art, BOTTOM, *CARETS[1][1:]).convert('L')).astype(float)
        m = np.abs(a - b)
        out.append(float(m[m > 0.5].mean()) if (m > 0.5).any() else 0.0)
    return out


def mirror_check():
    """The diamond must be mirror-exact at both sizes, every draft: the owner's symmetry note."""
    for fh in (2160, 800):
        for art in ARTS:
            s = Tile(Image.new('RGBA', (240, 240), (0, 0, 0, 255)), fh)
            diamond(s, 120, 120, art, TOP)
            a = np.asarray(s.im)[..., :3].astype(int)
            lr, ud = np.abs(a - a[:, ::-1]).max(), np.abs(a - a[::-1]).max()
            print('mirror', fh, art, lr, ud)
            assert lr <= 1 and ud <= 1


class Page:
    def __init__(self, width, title, sub):
        self.W, self.pad, self.y = width, 24, 20
        self.im = Image.new('RGB', (width, 9000), d.BG)
        self.d = ImageDraw.Draw(self.im)
        self.say(title, 58, (255, 226, 120), 66)
        self.say(sub, 36, (226, 240, 252), 44)
        self.y += 10

    def say(self, s, size, col, lead, x=None, width=None):
        f = hm.font(size)
        for t in d.wrap(self.d, s, f, width or self.W - 2 * self.pad):
            self.d.text((x if x is not None else self.pad, self.y), t, fill=col, font=f)
            self.y += lead

    def head(self, s, note=''):
        self.y += 12
        self.d.line([(self.pad, self.y), (self.W - self.pad, self.y)], fill=(80, 110, 140), width=2)
        self.y += 8
        self.say(s, 42, (200, 225, 245), 50)
        if note:
            self.say(note, 30, (160, 195, 225), 36)
        self.y += 6

    def tag(self, x, y, s, size=26, col=(160, 195, 225), anchor='la'):
        self.d.text((x, y), s, fill=col, font=hm.font(size), anchor=anchor)

    def done(self, path):
        im = self.im.crop((0, 0, self.W, self.y + 12))
        im.save(path, quality=93, subsampling=0)
        print(path, im.size)


def artlabel(p, art, x, y, depth=None):
    col = (252, 243, 68) if art == PICK else (200, 225, 245)
    p.tag(x, y, NAMES[art], 32, col)
    if art == PICK:
        p.tag(x, y + 36, 'the pick', 26, col)
    if depth:
        p.tag(x, y + 72, 'breath moves', 24)
        p.tag(x, y + 98, '%.0f dark, %.0f bright' % tuple(depth), 24)


def main():
    out = sys.argv[1]
    mirror_check()
    F = frames()
    depth = {a: breath_depth(F, a) for a in ARTS}
    print(depth)
    LW = 214
    p = Page(2140, TITLE, SUB)
    x0 = p.pad + LW
    yellow = (252, 243, 68)

    # 1: 4K, pixel for pixel
    p.head('At 4K, pixel for pixel', 'Each tile is 148 pixels of a 3840x2160 frame, one to one. Left half the top of the breath, right half the bottom; in each half dark sky, then the brightest cloud; carets hostile, neutral, friendly. Under each name: the mean change the breath makes to the pixels it moves, in grey levels of 255.')
    n, gp = 148, 4
    half = 6 * n + 5 * gp + 3 * gp
    for j, lab in enumerate(('TOP OF THE BREATH', 'BOTTOM OF THE BREATH')):
        xs = x0 + j * (half + 18)
        p.tag(xs, p.y, lab + ':  dark sky', 26)
        p.tag(xs + 3 * (n + gp) + 3 * gp, p.y, 'bright cloud', 26)
    p.y += 34
    for art in ARTS:
        artlabel(p, art, p.pad, p.y, depth[art])
        for j, lvl in enumerate((TOP, BOTTOM)):
            x = x0 + j * (half + 18)
            for which in ('dark', 'bright'):
                for _, col, fm in CARETS:
                    p.im.paste(sky_tile(F, which, K4, n, art, lvl, col, fm), (x, p.y))
                    x += n + gp
                x += 3 * gp
        p.y += n + 6

    # 2: 1280x800, and the enlargements
    p.head('At 1280x800, pixel for pixel, and enlarged', 'Left the 1280x800 tiles one to one, 56 pixels each, top of the breath over bottom. Middle the same tiles at the top, x3, nearest neighbour. Right, for the stroke at 4K: the lower left edge and the caret\'s corner, dark and bright, x2, nearest neighbour.')
    m, k = 56, 3
    p.tag(x0, p.y, 'One to one', 26)
    p.tag(x0 + 6 * (m + 2) + 26, p.y, '1280x800, x3', 26)
    p.tag(x0 + 6 * (m + 2) + 26 + 6 * (m * k + 3) + 26, p.y, '4K, x2', 26)
    p.y += 34
    for art in ARTS:
        artlabel(p, art, p.pad, p.y)
        for r_, lvl in enumerate((TOP, BOTTOM)):
            x = x0
            for which in ('dark', 'bright'):
                for _, col, fm in CARETS:
                    p.im.paste(sky_tile(F, which, DECK, m, art, lvl, col, fm), (x, p.y + r_ * (m + 3)))
                    x += m + 2
                x += 6
        x += 14
        for which in ('dark', 'bright'):
            for _, col, fm in CARETS:
                p.im.paste(nn(sky_tile(F, which, DECK, m, art, TOP, col, fm), k), (x, p.y))
                x += m * k + 3
            x += 6
        x += 14
        for which in ('dark', 'bright'):
            p.im.paste(nn(sky_tile(F, which, K4, n, art, TOP, *CARETS[0][1:]).crop((6, 58, 102, 142)), 2), (x, p.y))
            x += 192 + 4
        p.y += m * k + 6

    # 3: on real targets
    def at(t, i, dy, cw, ch, fn):
        """A crop of the target's frame, its centre dy under the target's centre; fn(tile, x, y) gets the target's centre in the crop."""
        x, y = t['at'][i]
        dy = int(dy)
        return cut(F[t['key']][i], (K4, DECK)[i][1], x, y + dy, cw, ch, lambda s, a, b: fn(s, a, b - dy))

    p.head('On real targets at 4K', 'One to one, the top of the breath: the diamond and what stands under it. Two ships and two solars, all three carets among them. The fighter and the lane ring stand at the floor size, 0.030 H; the station and the battleship at the cap, 0.10 H.')
    cw, ch = 448, 230
    gx = 12
    for i, t in enumerate(TARGETS):
        p.tag(x0 + i * (cw + gx), p.y, t['word'], 26)
    p.y += 34

    def top_of(t, fh):
        return -t['half'] * fh * 1.22 + 0.02 * fh

    for art in ARTS:
        artlabel(p, art, p.pad, p.y)
        for i, t in enumerate(TARGETS):
            p.im.paste(at(t, 0, top_of(t, 2160), cw, ch, lambda s, a, b, t=t: form(s, t, a, b, art, TOP)), (x0 + i * (cw + gx), p.y))
        p.y += ch + 6
    p.tag(p.pad, p.y, PICK + ', bottom', 32, yellow)
    p.tag(p.pad, p.y + 36, 'of the breath', 26, yellow)
    for i, t in enumerate(TARGETS):
        p.im.paste(at(t, 0, top_of(t, 2160), cw, ch, lambda s, a, b, t=t: form(s, t, a, b, PICK, BOTTOM)), (x0 + i * (cw + gx), p.y))
    p.y += ch + 12

    p.head('The pick, the whole form', 'At 4K one to one: bars on the ships, none on the solars, the distance in the words line. The large diamond is gone. Under it the same at 1280x800, one to one, with the diamond x2 beside it, nearest neighbour.')
    ch3 = 730
    widths = (440, 440, 620, 540)      # each as wide as its bars and words
    x = p.pad
    for t, cw3 in zip(TARGETS, widths):
        dy = -t['half'] * 2160 * 1.22 - 0.05 * 2160 + ch3 / 2
        p.im.paste(at(t, 0, dy, cw3, ch3, lambda s, a, b, t=t: form(s, t, a, b, PICK, TOP)), (x, p.y))
        x += cw3 + 12
    p.y += ch3 + 8
    h8, zoom = 272, 110
    x = p.pad
    for t, cw3 in zip(TARGETS, widths):
        w8 = min(230, cw3 - 8 - 2 * zoom)
        dy = -t['half'] * 800 * 1.22 - 0.05 * 800 + h8 / 2
        p.im.paste(at(t, 1, dy, w8, h8, lambda s, a, b, t=t: form(s, t, a, b, PICK, TOP)), (x, p.y))
        dz = -t['half'] * 800 * 1.22 - 0.005 * 800
        p.im.paste(nn(at(t, 1, dz, zoom, zoom, lambda s, a, b, t=t: form(s, t, a, b, PICK, TOP)), 2), (x + w8 + 8, p.y))
        x += cw3 + 12
    p.y += h8 + 10

    # 4: the raised caret
    p.head('The raised caret on a small target', 'As ruled, at the floor size the diamond\'s lower point stands 0.016 H inside the object\'s box: it leaves this fighter about 10 pixels at 4K and 3 at 1280x800, and a ship seen nose up would touch it. Raised, this sheet\'s pick: the point stays %.3f H clear of the box at any size, one rule with no special case. The lift is 0.024 H at the floor (51 pixels at 4K, 19 at 1280x800) and 0.008 H at the cap. The price: the caret stands further from a small ship. A target that is not the waypoint keeps its caret where it stands.' % CLEAR)
    c4, h4, c8, h8 = 440, 420, 176, 170
    xl = p.pad + 110
    x = xl
    for hd in ('4K, as ruled', '4K, raised'):
        p.tag(x, p.y, hd, 26)
        x += c4 + 8
    x += 8
    for hd in ('1280x800, as ruled, one to one, x2', '1280x800, raised, one to one, x2'):
        p.tag(x, p.y, hd, 26)
        x += c8 + 4 + 2 * c8 + 8
    p.y += 34

    def window(t, fh, ch):
        return -t['half'] * fh - (CLEAR + 2 * R + 0.012) * fh + ch / 2

    for t in (TARGETS[0], TARGETS[3]):
        p.tag(p.pad, p.y, 'A ship' if t['bars'] else 'A solar', 32)
        x = xl
        for lift in (False, True):
            p.im.paste(at(t, 0, window(t, 2160, h4), c4, h4, lambda s, a, b, t=t, lift=lift: form(s, t, a, b, PICK, TOP, lift)), (x, p.y))
            x += c4 + 8
        x += 8
        for lift in (False, True):
            a_ = at(t, 1, window(t, 800, h8), c8, h8, lambda s, a, b, t=t, lift=lift: form(s, t, a, b, PICK, TOP, lift))
            p.im.paste(a_, (x, p.y))
            p.im.paste(nn(a_, 2), (x + c8 + 4, p.y))
            x += c8 + 4 + 2 * c8 + 8
        p.y += max(h4, 2 * h8) + 6

    # 5: colour and steady
    p.head('The pick in active yellow, and held steady', 'Left the same art in active yellow #FCF344 instead of chrome #82C9F3: it runs into the neutral caret and shouts over a bright sky. Right chrome held at %d %% of the glow, as under skin_motion FADES and NONE. 4K, one to one.' % (STEADY * 100))
    x = p.pad
    n5 = 164
    for dcol, lvl in ((ACTIVE, TOP), (CHROME, STEADY)):
        for which in ('dark', 'bright'):
            for _, col, fm in CARETS:
                p.im.paste(sky_tile(F, which, K4, n5, PICK, lvl, col, fm, dcol), (x, p.y))
                x += n5 + 4
            x += 8
        x += 4
    p.y += n5 + 12
    p.say(FOOT, 30, (130, 160, 190), 36)
    p.done(out)

    # the breath, moving: every draft, dark over bright, 4K pixels
    fr = []
    steps = 20
    for i in range(steps):
        lvl = BOTTOM + (TOP - BOTTOM) * (0.5 - 0.5 * math.cos(2 * math.pi * i / steps))
        im = Image.new('RGB', (4 * 164 + 4, 2 * 164 + 34), d.BG)
        dr = ImageDraw.Draw(im)
        for c, art in enumerate(ARTS):
            for r_, which in enumerate(('dark', 'bright')):
                im.paste(sky_tile(F, which, K4, 160, art, lvl, *CARETS[(c + r_) % 3][1:]), (4 + c * 164, 34 + r_ * 164))
            dr.text((8 + c * 164, 2), NAMES[art], fill=(200, 225, 245), font=hm.font(26))
        fr.append(im.quantize(colors=255, method=Image.MEDIANCUT, dither=Image.NONE))
    gif = os.path.splitext(out)[0] + '.gif'
    fr[0].save(gif, save_all=True, append_images=fr[1:], duration=int(PERIOD * 1000 / steps), loop=0, disposal=1)
    print(gif)


TITLE = 'The waypoint diamond\'s art: the pick is A, the thin outline'
SUB = 'Why: its line stays whole at both sizes and at the bottom of the breath, so it reads as the game\'s own waypoint shape, made small, and only the glow moves. B\'s two lines clog at 1280x800, C reads as four arrows round the caret, and D nearly goes out on dark sky at the bottom of the breath while covering the most sky. All four: chrome #82C9F3 with a navy outline, 0.054 H across (117 pixels at 4K, 43 at 1280x800), the caret centred on its mass; the line is %.1f pixels at 4K, %.1f at 1280x800.' % (0.0026 * 2160, 0.0026 * 800)
FOOT = ('Real: the fighter, the station and the planet with its lane rings are 3840x2160 and 1280x800 frames of HDE, each size its own capture. The battleship is the playthrough at 1080p, enlarged x2 for the 4K rows and cut to 16:10 for 1280x800; the soft bands on it are the game\'s large diamond painted out, which would not exist in the game. '
        'Painted out: the game\'s bracket and bars on the fighter, its labels on the battleship, the lane bracket on the ring beside the one used. Drawn: the diamond, the caret, the bars, the words. Nothing ran in game. '
        'Stand-ins: the station\'s and the ring\'s names, distances and attitudes, and the ring as a target at all; the fighter is not a waypoint in its frame. The 0.030 H floor is read as 6.10 uses it, the object\'s half size, so the caret keeps its 0.022 H width; the diamond is mirror-exact, checked pixel by pixel. Moving: Diamond_art.gif, 0.5 Hz.')


if __name__ == '__main__':
    main()
