"""hud2i.py - HUD 2's own identity for the owner's approval (D1): the wordmark and the cursor set. Nothing here is built.

    research\\tools\\ocr\\.venv\\Scripts\\python.exe -I tools\\uimock\\hud2i.py

Shapes come from HUD 2's language: chamfers top-left and bottom-right, a light-blue hairline, navy glass, Agency FB, one yellow for active. The cursor set covers all 19
names of the game's DATA\\mouse.ini, one meaning each; colours as HUD 2: attitude red / green, neutral chrome.
"""
import math
import os
import sys

from PIL import Image, ImageDraw, ImageFilter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hud2d as d  # noqa: E402
import hud2g as g  # noqa: E402
import hud2s as hs  # noqa: E402
import hudmock as hm  # noqa: E402

K4, DECK = g.K4, g.DECK
INK, CHROME, ACTIVE, AMBER, HOSTILE, FRIEND = g.INK, g.CHROME, g.ACTIVE, g.AMBER, g.HOSTILE, g.FRIEND
NAVY = (10, 14, 40)
SS = 4


def poly(dr, pts, fill=None, line=None, w=1):
    if fill:
        dr.polygon(pts, fill=fill)
    if line:
        dr.line(pts + pts[:1], fill=line, width=w, joint='curve')


def arc(dr, c, r, a0, a1, col, w):
    dr.arc([c[0] - r, c[1] - r, c[0] + r, c[1] + r], a0, a1, fill=col, width=w)


# ---- the cursor set. Every shape draws in a box of size z with its hot spot at `hot` (fractions)
def arrow_pts(z):
    return [(0, 0), (0.24 * z, 0.92 * z), (0.40 * z, 0.60 * z), (0.56 * z, 0.60 * z), (0.80 * z, 0.52 * z)]


def c_arrow(dr, z, lw, col=CHROME):
    p = [(x + z * 0.08, y + z * 0.06) for x, y in arrow_pts(z)]
    poly(dr, p, NAVY + (235,), col + (255,), lw)


def reticle(dr, z, lw, col, gap=True, dot=True):
    c = (z / 2, z / 2)
    r = z * 0.36
    for a in (45, 135, 225, 315):
        arc(dr, c, r, a - 26, a + 26, col + (255,), lw)
    if dot:
        q = z * 0.035
        dr.ellipse([c[0] - q, c[1] - q, c[0] + q, c[1] + q], fill=col + (255,))


def c_cross(dr, z, lw):
    reticle(dr, z, lw, CHROME)


def c_neutral(dr, z, lw):
    reticle(dr, z, lw, INK)


def c_hostile(dr, z, lw):
    reticle(dr, z, lw, HOSTILE)
    corners(dr, z, lw, HOSTILE)


def c_friendly(dr, z, lw):
    reticle(dr, z, lw, FRIEND)


def corners(dr, z, lw, col):
    a, b = z * 0.06, z * 0.94
    t = z * 0.14
    for x, y, sx, sy in ((a, a, 1, 1), (b, a, -1, 1), (a, b, 1, -1), (b, b, -1, -1)):
        dr.line([(x + sx * t, y), (x, y), (x, y + sy * t)], fill=col + (255,), width=lw)


def fire(dr, z, lw, col):
    reticle(dr, z, lw, col, dot=False)
    c = z / 2
    for a in (0, 90, 180, 270):
        x, y = math.cos(math.radians(a)), math.sin(math.radians(a))
        dr.line([(c + x * z * 0.10, c + y * z * 0.10), (c + x * z * 0.26, c + y * z * 0.26)], fill=col + (255,), width=lw)


def c_fire(dr, z, lw):
    fire(dr, z, lw, HOSTILE)


def c_fire_friendly(dr, z, lw):
    fire(dr, z, lw, FRIEND)


def c_fire_neutral(dr, z, lw):
    fire(dr, z, lw, INK)


def c_no_fire(dr, z, lw):
    reticle(dr, z, lw, AMBER, dot=False)
    dr.line([(z * 0.26, z * 0.74), (z * 0.74, z * 0.26)], fill=AMBER + (255,), width=lw)


def c_stretch(dr, z, lw):
    c = z / 2
    a, b = z * 0.10, z * 0.90
    h = z * 0.13
    dr.line([(a, c), (b, c)], fill=CHROME + (255,), width=lw)
    poly(dr, [(a, c), (a + h * 1.3, c - h), (a + h * 1.3, c + h)], NAVY + (235,), CHROME + (255,), lw)
    poly(dr, [(b, c), (b - h * 1.3, c - h), (b - h * 1.3, c + h)], NAVY + (235,), CHROME + (255,), lw)


def talk(word):
    def f(dr, z, lw):
        a, b, t, u = z * 0.02, z * 0.98, z * 0.14, z * 0.66
        ch = z * 0.10
        p = [(a + ch, t), (b, t), (b, u - ch), (b - ch, u), (z * 0.42, u), (z * 0.24, z * 0.88), (z * 0.24, u), (a, u), (a, t + ch)]
        poly(dr, p, NAVY + (235,), CHROME + (255,), lw)
        fnt = hm.font(int(z * (0.30 if len(word) < 6 else 0.235)))
        dr.text((z / 2, (t + u) / 2), word, fill=INK + (255,), font=fnt, anchor='mm')
    return f


SET = [('arrow', c_arrow, 'MENUS'), ('stretch', c_stretch, 'A SLIDER, A RESIZE'), ('cross', c_cross, 'FREE AIM'), ('neutral', c_neutral, 'OVER A NEUTRAL'), ('hostile', c_hostile, 'OVER A HOSTILE'),
       ('friendly', c_friendly, 'OVER A FRIEND'), ('fire', c_fire, 'FIRING'), ('fire_neutral', c_fire_neutral, 'FIRING, NEUTRAL'), ('fire_friendly', c_fire_friendly, 'FIRING, A FRIEND'),
       ('no_fire', c_no_fire, 'CANNOT FIRE'), ('talk_mission', talk('JOB'), 'TALK: A JOB'), ('talk_info', talk('INFO'), 'TALK: INFO'), ('talk_rumor', talk('RUMOR'), 'TALK: RUMOR'),
       ('talk_story', talk('STORY'), 'TALK: THE STORY'), ('talk_bribe', talk('BRIBE'), 'TALK: A BRIBE'), ('talk_blowoff', talk('...'), 'NOTHING TO SAY'),
       ('talk_equip_dealer', talk('GEAR'), 'EQUIPMENT DEALER'), ('talk_commodity_dealer', talk('GOODS'), 'COMMODITY DEALER'), ('talk_ship_dealer', talk('SHIPS'), 'SHIP DEALER')]


def glyph(fn, px):
    """A shape at px, drawn large and scaled down, with a dark halo so it reads on any scene."""
    z = px * SS
    pad = int(z * 0.12)
    lay = Image.new('RGBA', (z + 2 * pad, z + 2 * pad), (0, 0, 0, 0))
    inner = Image.new('RGBA', (z, z), (0, 0, 0, 0))
    fn(ImageDraw.Draw(inner), z, max(SS, int(z * 0.055)))
    lay.paste(inner, (pad, pad), inner)
    a = lay.split()[3].filter(ImageFilter.MaxFilter(2 * (SS // 2) + 1)).filter(ImageFilter.GaussianBlur(SS))
    halo = Image.new('RGBA', lay.size, (0, 0, 0, 0))
    halo.putalpha(a.point(lambda v: int(v * 0.75)))
    halo.alpha_composite(lay)
    return halo.resize((halo.width // SS, halo.height // SS), Image.LANCZOS), pad // SS


def stamp(im, fn, xy, px):
    gl, pad = glyph(fn, px)
    im.alpha_composite(gl, (max(0, int(xy[0] - pad)), max(0, int(xy[1] - pad))))


# ---- the wordmark, three forms
def wm_line(s, x, y, hgt):
    s.text((s.h(x), s.h(y)), 'FREELANCER', s.h(hgt), fill=INK, track=0.30)
    w = s.textlen('FREELANCER', s.h(hgt), track=0.30) / s.H
    s.line([(s.h(x), s.h(y + hgt * 0.72)), (s.h(x + w), s.h(y + hgt * 0.72))], col=CHROME, w=s.h(0.0018), alpha=220)
    s.line([(s.h(x), s.h(y + hgt * 0.72)), (s.h(x + w * 0.10), s.h(y + hgt * 0.72))], col=ACTIVE, w=s.h(0.0040))


def wm_plate(s, x, y, hgt):
    w = s.textlen('FREELANCER', s.h(hgt), track=0.26) / s.H
    s.glass((x, y - hgt * 0.92, x + w + 0.08, y + hgt * 0.92), content=False, alpha=0.85, accent=('l', ACTIVE))
    s.text((s.h(x + 0.04), s.h(y)), 'FREELANCER', s.h(hgt), fill=INK, track=0.26)


def wm_ring(s, x, y, hgt):
    r = hgt * 0.78
    cx, cy = s.h(x + r), s.h(y)
    R = s.h(r)
    wd = s.h(0.0065)
    s.arcbar(cx, cy, R, 138, 222, wd, 1.0, (78, 132, 255), n=7, gapdeg=2.0)
    s.arcbar(cx, cy, R, -42, 42, wd, 1.0, (214, 62, 62), n=7, gapdeg=2.0)
    s.arcbar(cx, cy, R, 68, 112, wd * 0.8, 1.0, hs.h2.POWER, n=4, gapdeg=3.0)
    s.text((s.h(x + 2 * r + 0.035), s.h(y)), 'FREELANCER', s.h(hgt), fill=INK, track=0.26)


def d1():
    w, h = K4
    forms = [('A. Wordmark and rule', wm_line, 'The word in wide caps, HUD 2\'s hairline, the yellow of "active" on its first tenth.'),
             ('B. On a plate', wm_plate, 'The word on the glass every panel uses: it reads on any scene, and is the plainest fit with the skin.'),
             ('C. With the ring', wm_ring, 'The reticle ring beside the word: shield blue, hull red, power gold: the HUD\'s own face as the mark.')]
    def clean(size):
        """The game's menu with its logo painted out: a feathered blur of the sky behind it (the probe's hidden-logo frames carry ReShade's start banner)."""
        im = Image.open(g.front('mainmenu', size)).convert('RGBA')
        ww, hh = size
        m_ = Image.new('L', size, 0)
        ImageDraw.Draw(m_).rectangle((0, 0, int(ww * 0.46), int(hh * 0.215)), fill=255)
        m_ = m_.filter(ImageFilter.GaussianBlur(hh * 0.012))
        im.paste(im.filter(ImageFilter.GaussianBlur(hh * 0.05)), (0, 0), m_)
        return d.save(im, f'id_menu_{ww}')

    def top(p, size):
        return Image.open(p).convert('RGB').crop((0, 0, int(size[0] * 0.56), int(size[1] * 0.40)))
    opts = [('Today: the game\'s logo', 'Real, as the game draws it. Left 4K, right 1280x800, the top left of each.', [top(g.front('mainmenu', K4), K4), top(g.front('mainmenu', DECK), DECK)], False)]
    for i, (word, fn, note) in enumerate(forms):
        res = []
        for size, deck in ((K4, False), (DECK, True)):
            s = hs.Skin(clean(size), deck)
            fn(s, 0.075 if not deck else 0.05, 0.095, 0.075)
            res.append(top(d.save(s, f'id_logo{i}_{size[0]}'), size))
        opts.append((word, note, res, False))
    d.sheet('D1a_logo', 'D1. What replaces the logo?', 'Three drafts for the owner to choose from or send back. Nothing is built.', opts,
            'Real: the menu scene, the game\'s buttons (the chips of the skin come later). Drawn: each mark, in Agency FB. The logo is a draw of its own (0x565A10); a hook can swap it.')
    # the set, large
    cell, px = 300, 150
    cols = 4
    rows = -(-len(SET) // cols)
    sheet = Image.new('RGBA', (cols * cell + 40, rows * (cell - 60) + 20), (16, 22, 44, 255))
    dr = ImageDraw.Draw(sheet)
    f = hm.font(26)
    for i, (n, fn, word) in enumerate(SET):
        x, y = 20 + (i % cols) * cell, 14 + (i // cols) * (cell - 60)
        stamp(sheet, fn, (x + 70, y + 8), px)
        dr.text((x + 8 + cell / 2 - 10, y + 182), word, fill=(200, 225, 245), font=f, anchor='mm')
    # true size on real frames
    z4 = int(h * 0.052)

    def on(frame_p, spots, size):
        im = Image.open(frame_p).convert('RGBA')
        zz = int(size[1] * 0.052)
        for fn, fx, fy in spots:
            stamp(im, fn, (size[0] * fx, size[1] * fy), zz)
        return im

    deal = on(g.frame(K4, 'equipment__kbm'), [(c_arrow, 0.385, 0.30)], K4)
    room = on(g.frame(K4, 'room__kbm'), [(talk('JOB'), 0.42, 0.52)], K4)
    fight = on(g.frame(K4, 'fight__kbm'), [(c_hostile, 0.60, 0.34)], K4)
    deck_d = on(g.frame(DECK, 'equipment__kbm'), [(c_arrow, 0.385, 0.30)], DECK)
    real = Image.open(d.f2(K4, 'dealer_cursor')).convert('RGB')
    d.sheet('D1b_cursor', 'D1. What replaces the cursor?', 'All 19 shapes the game uses, each with one meaning. A draft for the owner to choose from or send back. Nothing is built.',
            [('The set, enlarged', 'Arrow for menus; four-part reticle for flight, colour by attitude plus a form for each state; a word in a bubble for rooms: words, not the game\'s pictures.', [sheet.convert('RGB')], False),
             ('Today, the game\'s arrow', 'Real.', [real], False),
             ('HUD 2\'s arrow, at true size', '0.052 H, as the game\'s own is about. Dealer at 4K and 1280x800.', [deal.convert('RGB'), deck_d.convert('RGB')], False),
             ('In a room, in a fight', 'True pixels at 4K, cropped: frames real, cursors drawn.',
              [room.convert('RGB').crop((int(w * 0.42) - 300, int(h * 0.52) - 240, int(w * 0.42) + 360, int(h * 0.52) + 420)),
               fight.convert('RGB').crop((int(w * 0.60) - 300, int(h * 0.34) - 240, int(w * 0.60) + 360, int(h * 0.34) + 420))], False)],
            'Position, clicks and hover stay the game\'s. Colour never alone: hostile adds corners, fire adds spokes, no-fire a bar. The game\'s `talk_*` names decide the word.')


if __name__ == '__main__':
    d1()
