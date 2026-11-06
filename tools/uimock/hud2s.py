"""hud2s.py - HUD 2 as a complete skin (modules\\hud\\docs\\UI_HUD2_SKIN.md): mock frames of the screens still native, for the owner to judge.

    research\\tools\\ocr\\.venv\\Scripts\\python.exe -I tools\\uimock\\hud2s.py [recipe ...]

Frames: hud2x's layered captures (C = the scene alone) and the 5 Nov nav map frame. Written to
research\\evidence\\hud2_skin\\mock\\. Mock, not game: nothing here ran in game, and each frame's limits are in the design's section 9.
"""
import os
import sys

from PIL import Image, ImageFilter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hud2 as h2  # noqa: E402
import hud2x as hx  # noqa: E402
import hudmock as hm  # noqa: E402

OUT = os.path.join(hm.ROOT, 'research', 'evidence', 'hud2_skin', 'mock')
RUNS = os.path.join(hm.ROOT, 'research', 'experience', 'runs')
NAV = os.path.join(RUNS, 'hud28_windows_2026-11-05', 'm4_navmap-014214.png')
TMP = os.path.join(OUT, '_base')
INK, CHROME, DIM, ACTIVE, CYAN = h2.INK, h2.CHROME, h2.DIM, h2.ACTIVE, hm.CYAN
HOSTILE, FRIEND, AMBER = (236, 64, 52), (96, 214, 96), (255, 196, 120)

# The nav map in the 4K frame: the map's square, and the game's labels (left x, centre y, text), in the frame's pixels from CROP.
CROP = (1300, 380)
SQ = (98, 64, 1146, 1112)
LABELS = [(490, 320, 'New York -> Colorado'), (766, 332, 'Planet Pittsburgh'), (822, 367, 'Maine'), (606, 432, '*Fort Bush'),
          (422, 495, '*Planet Manhattan'), (420, 530, '*Newark Station'), (742, 637, '*Battleship Missouri'),
          (378, 715, 'West Point Military Academy'), (205, 775, 'New York -> California'), (828, 830, 'New York -> Texas'),
          (676, 872, 'Buffalo Base'), (473, 920, '*New York -> Magellan')]
LABEL_W = 0.0072   # width of a character in the frame, in H (from the crop)


def clean_map():
    """The map's square with its labels and toolbar taken out: at the larger size the game's labels stay their size."""
    os.makedirs(TMP, exist_ok=True)
    p = os.path.join(TMP, 'navmap_clean.png')
    if os.path.exists(p):
        return Image.open(p).convert('RGB')
    im = Image.open(NAV).convert('RGB').crop((CROP[0], CROP[1], CROP[0] + 1240, CROP[1] + 1240))
    # the toolbar and the title: row 7's starfield over row 1 (the columns line up)
    strip = im.crop((98, 990, 1146, 1056))
    im.paste(strip, (98, 64))
    im.paste(strip.crop((0, 0, 1048, 62)), (98, 132))
    fill = im.filter(ImageFilter.MedianFilter(31))
    from PIL import ImageDraw
    box = Image.new('L', im.size, 0)
    d = ImageDraw.Draw(box)
    for x, y, s in LABELS:
        d.rectangle((x - 6, y - 26, x + len(s) * 15.5 + 8, y + 26), fill=255)
    r, g, b = im.split()
    light = Image.merge('RGB', (r, g, b)).convert('L').point(lambda v: 255 if v > 70 else 0).filter(ImageFilter.MaxFilter(7))
    from PIL import ImageChops
    im.paste(fill, (0, 0), ImageChops.multiply(box, light))
    sq = im.crop(SQ)
    sq.save(p)
    return sq


def base(screen, size, name):
    """The scene layer at a size (the same crop rule as the game: full height, width cut)."""
    os.makedirs(TMP, exist_ok=True)
    p = os.path.join(TMP, f'{name}_{size[0]}x{size[1]}.png')
    im = Image.open(screen).convert('RGBA')
    w, h = size
    s = max(w / im.width, h / im.height)
    im = im.resize((int(im.width * s + 0.5), int(im.height * s + 0.5)), Image.LANCZOS)
    x, y = (im.width - w) // 2, (im.height - h) // 2
    im.crop((x, y, x + w, y + h)).save(p)
    return p


def menu_scene():
    """The main menu's scene: no layer holds it without the buttons, so the button column and the version go under a wide blur (a plate covers them)."""
    os.makedirs(TMP, exist_ok=True)
    p = os.path.join(TMP, 'menu_clean.png')
    if not os.path.exists(p):
        im = Image.open(os.path.join(hx.LAYERS, 'mainmenu_A.png')).convert('RGB')
        soft = im.filter(ImageFilter.GaussianBlur(70))
        for b in ((90, 540, 980, 1680), (3300, 2030, 3840, 2160)):
            im.paste(soft.crop(b), b[:2])
        im.save(p)
    return p


class Skin(hx.Screen):
    def __init__(self, path, deck=False):
        h2.HUD.__init__(self, path)
        self.screen, self.regions, self.keeps, self.drops, self.over = '', [], [], [], []
        self.deck = deck
        self.WH = self.W / self.H                # the screen's width in H
        self.mx = 0.020 if deck else 0.05 * self.WH   # the side margin in H
        self.ts = 1.2 if deck else 1.0           # text scale: Deck mode's 120

    def t(self, x, y, s, size, fill=INK, anchor='lm', track=0.0):
        return self.text((self.h(x), self.h(y)), s, self.h(size * self.ts), fill=fill, anchor=anchor, track=track)

    def fit(self, x, y, s, size, maxw, fill=INK):
        """A text in its room: it steps down, never under the floor (0.024 H)."""
        z = size * self.ts
        while self.textlen(s, self.h(z)) > self.h(maxw) and z > 0.024:
            z -= 0.001
        return self.text((self.h(x), self.h(y)), s, self.h(z), fill=fill, anchor='lm')

    def small(self, x, y, s, fill=CHROME, size=0.024, anchor='lm'):
        return self.label((self.h(x), self.h(y)), s, self.h(size * self.ts), col=fill, anchor=anchor, track=0.08)

    def wrap(self, x, y, s, size, maxw, fill=INK, lead=1.28):
        words, line, n = s.split(' '), '', 0
        px = self.h(size * self.ts)
        for w in words:
            c = (line + ' ' + w).strip()
            if self.textlen(c, px) > self.h(maxw) and line:
                self.t(x, y + n * size * self.ts * lead, line, size, fill)
                line, n = w, n + 1
            else:
                line = c
        self.t(x, y + n * size * self.ts * lead, line, size, fill)
        return y + (n + 1) * size * self.ts * lead

    def panel(self, r, title=None, sub=None, accent=None):
        """HUD 2's one plate: glass, rim, a header and its rule."""
        self.glass(r, corners=(1, 0, 1, 0), content=False, accent=accent)
        y = r[1] + 0.012
        if title:
            y += 0.024 * self.ts
            w = self.fit(r[0] + 0.022, y, title, 0.040, r[2] - r[0] - 0.044) / self.H
            if sub:
                self.small(r[0] + 0.022 + w + 0.014, y + 0.003, sub)
            y += 0.030 * self.ts
            self.line([(self.h(r[0] + 0.018), self.h(y)), (self.h(r[2] - 0.018), self.h(y))], col=CHROME, w=max(2, self.h(0.0012)), alpha=130)
            y += 0.012
        return y

    def section(self, x0, x1, y, s):
        self.small(x0 + 0.022, y + 0.014 * self.ts, s)
        return y + 0.034 * self.ts

    def chip(self, r, s, current=False, focus=False):
        """A button: glass, hairline, its word; the current one yellow with an underline, the focused one the one focus style."""
        x0, y0, x1, y1 = self.px(r)
        self.plate((x0, y0, x1, y1), chamfer=self.h(0.008), alpha=225, rim_alpha=120 if not current else 200, corners=(1, 0, 1, 0))
        if focus:
            self.focus(r)
        if current:
            self.line([(x0 + self.h(0.010), y1 - self.h(0.004)), (x1 - self.h(0.010), y1 - self.h(0.004))], col=ACTIVE, w=self.h(0.004))
        size = 0.030 * self.ts
        while self.textlen(s, self.h(size)) > (x1 - x0) * 0.92 and size > 0.024:
            size -= 0.001
        self.text(((x0 + x1) / 2, (y0 + y1) / 2), s, self.h(size), fill=ACTIVE if current or focus else INK, anchor='mm')

    def lane(self, items):
        """The bottom lane, right-aligned at the margin."""
        size, g = self.h(0.026 * self.ts), self.h(0.032 * self.ts)
        right = self.W - self.h(self.mx)
        y = self.h(0.945 if not self.deck else 0.962)
        x = right
        hh = self.h(0.022 * self.ts)
        for glyph, verb in reversed(items):
            w = g + self.h(0.010) + self.textlen(verb, size) + self.h(0.028)
            x0 = x - w
            self.plate((x0, y - hh, x, y + hh), chamfer=self.h(0.008), alpha=215, rim_alpha=80, corners=(1, 1, 1, 1))
            self.glyph(glyph, x0 + self.h(0.014) + g / 2, y, g)
            self.text((x0 + self.h(0.014) + g + self.h(0.010), y), verb, size, fill=INK)
            x = x0 - self.h(0.010)

    def tabs(self, current, pad=True, names=None, lead=None):
        """The NeuroNet strip: every window one step away, in words, the same place on each. It stands in the bottom lane's left half, beside the
        prompts: the comm panel holds the top centre, so the top band has no room at 1280x800, and here it costs the map no height."""
        names = names or ['LOG', 'INVENTORY', 'MAP', 'STATUS', 'HELP']
        size = self.h(0.026 * self.ts)
        if lead:
            self.small(self.mx, (0.945 if not self.deck else 0.962) - 0.044 * self.ts, lead, CHROME, 0.024)
        px = self.h(0.016)
        hh = self.h(0.022 * self.ts)
        yc = self.h(0.945 if not self.deck else 0.962)
        ws = [self.textlen(n, size) + 2 * px for n in names]
        g = self.h(0.040) if pad else self.h(0.012)
        x = self.h(self.mx)
        total = sum(ws) + 2 * g
        self.plate((x, yc - hh, x + total, yc + hh), chamfer=self.h(0.008), alpha=215, rim_alpha=100, corners=(1, 0, 1, 0))
        if pad:
            self.glyph('LB', x + g / 2 + self.h(0.002), yc, self.h(0.026 * self.ts))
        cx = x + g
        for n, w in zip(names, ws):
            on = n == current
            self.text((cx + w / 2, yc), n, size, fill=ACTIVE if on else INK, anchor='mm')
            if on:
                self.line([(cx + px * 0.6, yc + hh - self.h(0.005)), (cx + w - px * 0.6, yc + hh - self.h(0.005))], col=ACTIVE, w=self.h(0.004))
            cx += w
        if pad:
            self.glyph('RB', cx + g / 2 - self.h(0.002), yc, self.h(0.026 * self.ts))
        else:
            w = self.textlen('CLOSE', size) + 2 * px
            r = self.W - self.h(self.mx)
            self.plate((r - w, yc - hh, r, yc + hh), chamfer=self.h(0.008), alpha=215, rim_alpha=100, corners=(1, 0, 1, 0))
            self.text((r - w / 2, yc), 'CLOSE', size, fill=INK, anchor='mm')

    def finish(self, name):
        os.makedirs(OUT, exist_ok=True)
        self.save(os.path.join(OUT, f'{name}.png'))


RECIPES = {}


def recipe(fn):
    RECIPES[fn.__name__] = fn
    return fn


def navmap(size, name, pad=True, deck=False, ends=None):
    s = Skin(base(hx.layer_path('pause_space', 'C'), size, 'space'), deck)
    s.scrim(0.78)
    top, bot = (0.115, 0.900) if not deck else (0.105, 0.869)   # the Deck: min(0.785 H, W - panels - margins) = 0.764 H (the critique's solver)
    S = bot - top
    gap = 0.018
    WH = s.WH
    pw = (WH - 2 * s.mx - S - 2 * gap) / 2       # each side panel
    mx0 = s.mx + pw + gap                        # the map's left edge
    # the map: the game's own, its box enlarged (B)
    px = int(s.h(S))
    m = clean_map().resize((px, px), Image.LANCZOS).convert('RGBA')
    s.glass((mx0 - 0.006, top - 0.006, mx0 + S + 0.006, bot + 0.006), corners=(1, 0, 1, 0), content=False, alpha=0.95)
    s.paste(m, (s.h(mx0), s.h(top)))
    k = S / 1048 * 1.0
    lab = 0.025 if not deck else 0.0225 * 1.2
    for x, y, t in LABELS:
        s.t(mx0 + (x - SQ[0]) * k, top + (y - SQ[1]) * k, t, lab, (150, 205, 235))
    # grid letters inside the rim, the game's texts moved
    for i in range(8):
        s.small(mx0 + (i + 0.5) * S / 8, bot - 0.014, 'ABCDEFGH'[i], DIM, 0.024, 'mm')
        s.small(mx0 + 0.016, top + (i + 0.5) * S / 8, str(i + 1), DIM, 0.024, 'mm')
    # the selection and the pad's cursor
    sx, sy = mx0 + (492 - SQ[0]) * k, top + (466 - SQ[1]) * k
    s.bracket(s.h(sx), s.h(sy), s.h(0.040), s.h(0.040), col=ACTIVE)
    if pad:
        cx, cy = mx0 + (640 - SQ[0]) * k, top + (590 - SQ[1]) * k
        s.shape(lambda d, P, q: d.ellipse([P(s.h(cx) - s.h(0.016), s.h(cy) - s.h(0.016)), P(s.h(cx) + s.h(0.016), s.h(cy) + s.h(0.016))],
                                          outline=INK + (255,), width=int(s.h(0.0022) * q)),
                box=(s.h(cx) - s.h(0.02), s.h(cy) - s.h(0.02), s.h(cx) + s.h(0.02), s.h(cy) + s.h(0.02)))
    # left: where you are, what to do, the route
    lx0, lx1 = s.mx, s.mx + pw
    y = s.panel((lx0, top, lx1, ends[0] if ends else bot), 'NEW YORK', 'LIBERTY')
    y = s.section(lx0, lx1, y, 'OBJECTIVE')
    y = s.wrap(lx0 + 0.022, y + 0.012, 'Dock with the Trade Lane to Fort Bush', 0.032, pw - 0.044)
    y = s.section(lx0, lx1, y + 0.014, 'ROUTE')
    for i, (a, b) in enumerate([('Trade Lane Ring', '2.1 K'), ('Fort Bush', '14.8 K'), ('New York > Colorado', '31.0 K')]):
        yy = y + (0.012 + i * 0.043) * s.ts
        s.small(lx0 + 0.030, yy, str(i + 1), ACTIVE, 0.026, 'mm')
        dw = s.textlen(b, s.h(0.026 * s.ts)) / s.H + 0.040
        s.fit(lx0 + 0.050, yy, a, 0.032, pw - 0.050 - dw)
        s.small(lx1 - 0.024, yy, b, CHROME, 0.026, 'rm')
    y += (0.012 + 3 * 0.043) * s.ts
    y = s.section(lx0, lx1, y + 0.006, 'VIEW')
    bw = (pw - 0.044 - 0.010) / 2
    bh = 0.046 * s.ts
    s.chip((lx0 + 0.022, y, lx0 + 0.022 + bw, y + bh), 'SYSTEM', current=True)
    s.chip((lx0 + 0.032 + bw, y, lx0 + 0.032 + 2 * bw, y + bh), 'SIRIUS')
    lend = y + bh + 0.022
    # right: layers as words, the selection card with its actions
    rx0, rx1 = mx0 + S + gap, mx0 + S + gap + pw
    y = s.panel((rx0, top, rx1, ends[1] if ends else bot), 'PLANET MANHATTAN', None, accent=('l', FRIEND))
    s.small(rx0 + 0.022, y + 0.012 * s.ts, 'LIBERTY' if deck else 'LIBERTY  ·  PLANET')
    s.small(rx1 - 0.022, y + 0.012 * s.ts, 'C3  ·  9.4 K', CHROME, 0.024, 'rm')
    y = s.wrap(rx0 + 0.022, y + 0.052 * s.ts, 'Headquarters of the Liberty government and the Liberty Navy. Docking rings open to all.', 0.030, pw - 0.044)
    y += 0.010
    cols = [('SET WAYPOINT', True), ('BEST PATH', False), ('ZOOM IN', False), ('CLEAR', False)]
    for i, (c, f) in enumerate(cols):
        xx = rx0 + 0.022 + (i % 2) * (bw + 0.010)
        yy = y + (i // 2) * (bh + 0.010)
        s.chip((xx, yy, xx + bw, yy + bh), c, focus=f and pad)
    y += 2 * (bh + 0.010) + 0.012
    s.line([(s.h(rx0 + 0.018), s.h(y)), (s.h(rx1 - 0.018), s.h(y))], col=CHROME, w=max(2, s.h(0.0012)), alpha=130)
    y = s.section(rx0, rx1, y + 0.006, 'LAYERS')
    for i, c in enumerate(['PHYSICAL', 'POLITICAL', 'PATROLS', 'MINING', 'BASES', 'LEGEND', 'MISSION', 'LABELS']):
        xx = rx0 + 0.022 + (i % 2) * (bw + 0.010)
        yy = y + (i // 2) * (bh + 0.008)
        s.chip((xx, yy, xx + bw, yy + bh), c, current=i == 0)
    rend = y + 4 * (bh + 0.008) + 0.014
    s.tabs('MAP', pad)
    if pad:
        s.lane([('A', 'SELECT'), ('X', 'WAYPOINT'), ('Y', 'TRAVEL'), ('D', 'LAYER'), ('B', 'CLOSE')])
    s.finish(name)
    return lend, rend


@recipe
def navmap_4k():
    navmap((3840, 2160), 'navmap_4k_pad', ends=navmap((3840, 2160), 'navmap_4k_pad'))


@recipe
def navmap_4k_kbm():
    navmap((3840, 2160), 'navmap_4k_kbm', pad=False, ends=navmap((3840, 2160), 'navmap_4k_kbm', pad=False))


@recipe
def navmap_deck():
    navmap((1280, 800), 'navmap_1280x800_pad', deck=True, ends=navmap((1280, 800), 'navmap_1280x800_pad', deck=True))


def universe_square():
    """The universe view's art at the game's size: the corner buttons and the title plate taken out by their neighbours."""
    im = Image.open(os.path.join(RUNS, 'hud28_windows_2026-11-05', 'm4_universe-014215.png')).convert('RGB')
    im = im.crop((CROP[0], CROP[1], CROP[0] + 1240, CROP[1] + 1240))
    im.paste(im.crop((930, 150, 1146, 216)), (930, 64))
    return im.crop(SQ)


@recipe
def universe_4k():
    s = Skin(base(hx.layer_path('pause_space', 'C'), (3840, 2160), 'space'))
    s.scrim(0.78)
    top, bot, gap = 0.115, 0.900, 0.018
    S = bot - top
    pw = (s.WH - 2 * s.mx - S - 2 * gap) / 2
    mx0 = s.mx + pw + gap
    px = int(s.h(S))
    s.glass((mx0 - 0.006, top - 0.006, mx0 + S + 0.006, bot + 0.006), content=False, alpha=0.95)
    s.paste(universe_square().resize((px, px), Image.LANCZOS).convert('RGBA'), (s.h(mx0), s.h(top)))
    lx0, lx1 = s.mx, s.mx + pw
    y = s.panel((lx0, top, lx1, 0.492), 'SIRIUS', 'THE KNOWN SYSTEMS')
    y = s.section(lx0, lx1, y, 'YOU ARE IN')
    s.t(lx0 + 0.022, y + 0.018, 'New York, Liberty', 0.034)
    y = s.section(lx0, lx1, y + 0.050, 'OBJECTIVE')
    y = s.wrap(lx0 + 0.022, y + 0.012, 'Dock with the Trade Lane to Fort Bush', 0.032, pw - 0.044)
    y = s.section(lx0, lx1, y + 0.014, 'VIEW')
    bw, bh = (pw - 0.054) / 2, 0.046
    s.chip((lx0 + 0.022, y, lx0 + 0.022 + bw, y + bh), 'SYSTEM')
    s.chip((lx0 + 0.032 + bw, y, lx0 + 0.032 + 2 * bw, y + bh), 'SIRIUS', current=True)
    rx0 = mx0 + S + gap
    y = s.panel((rx0, top, rx0 + pw, 0.430), 'COLORADO', None, accent=('l', CHROME))
    s.small(rx0 + 0.022, y + 0.012, 'LIBERTY  ·  2 JUMPS')
    y = s.wrap(rx0 + 0.022, y + 0.052, 'Liberty’s frontier with Rheinland: farming worlds and the Colorado jump gates.', 0.030, pw - 0.044)
    y += 0.010
    for i, c in enumerate(['OPEN SYSTEM', 'BEST PATH']):
        xx = rx0 + 0.022 + i * (bw + 0.010)
        s.chip((xx, y, xx + bw, y + bh), c, focus=i == 0)
    s.tabs('MAP')
    s.lane([('A', 'OPEN SYSTEM'), ('X', 'WAYPOINT'), ('D', 'LAYER'), ('B', 'CLOSE')])
    s.finish('navmap_universe_4k_pad')


def confirm(size, name, title, body, safe, act, deck=False, scene='pause_docked', focus_safe=True, amber=True, pad=True):
    """A confirm dialog: the question, what it costs, two verbs. Positions stay the game's: the action left, the safe answer right."""
    s = Skin(base(menu_scene() if scene == 'mainmenu' else hx.layer_path(scene, 'C'), size, 'scene_' + scene), deck)
    s.scrim(0.70)
    w, hh = 0.86, 0.300
    if deck:
        hh = 0.340
    x0 = (s.WH - w) / 2
    y0 = 0.5 - hh / 2
    s.glass((x0, y0, x0 + w, y0 + hh), content=False, accent=('t', AMBER) if amber else None, alpha=0.92)
    s.t(s.WH / 2, y0 + 0.052 * s.ts, title, 0.046, INK, 'mm')
    s.t(s.WH / 2, y0 + 0.112 * s.ts, body, 0.032, CHROME, 'mm')
    bw, bh = 0.36, 0.062 * s.ts
    by = y0 + hh - bh - 0.032
    s.chip((x0 + 0.050, by, x0 + 0.050 + bw, by + bh), act, focus=not focus_safe)
    s.chip((x0 + w - 0.050 - bw, by, x0 + w - 0.050, by + bh), safe, focus=focus_safe)
    if pad:
        s.lane([('A', 'SELECT'), ('D', 'CHOOSE'), ('B', safe)])
    s.finish(name)


@recipe
def quit_4k():
    confirm((3840, 2160), 'quit_confirm_4k_pad', 'QUIT TO THE MAIN MENU?', 'Progress since your last save will be lost.',
            'KEEP PLAYING', 'QUIT ANYWAY')


@recipe
def quit_deck():
    confirm((1280, 800), 'quit_confirm_1280x800_pad', 'QUIT TO THE MAIN MENU?', 'Progress since your last save will be lost.',
            'KEEP PLAYING', 'QUIT ANYWAY', deck=True)


@recipe
def exit_4k():
    confirm((3840, 2160), 'exit_game_4k_pad', 'EXIT FREELANCER?', 'Back to the desktop.', 'STAY', 'EXIT GAME', scene='mainmenu',
            focus_safe=False)


def mainmenu(size, name, deck=False):
    s = Skin(base(menu_scene(), size, 'menu'), deck)
    x0, x1 = s.mx, s.mx + 0.46
    y0 = 0.255
    items = ['NEW GAME', 'LOAD GAME', 'MULTIPLAYER', 'OPTIONS', 'EXIT']
    bh, g = 0.066 * s.ts, 0.018
    s.glass((x0 - 0.014, y0 - 0.020, x1 + 0.014, y0 + 5 * bh + 4 * g + 0.020), content=False, alpha=0.70)
    for i, c in enumerate(items):
        y = y0 + i * (bh + g)
        s.chip((x0, y, x1, y + bh), c, focus=i == 1)
    s.small(s.WH - s.mx, 0.965 if not deck else 0.975, 'FREELANCER 1.1  ·  HD EDITION 0.7.1  ·  FLVIBED 0.4.0', DIM, 0.024, 'rm')
    s.finish(name)


@recipe
def mainmenu_4k():
    mainmenu((3840, 2160), 'mainmenu_4k_pad')


@recipe
def mainmenu_deck():
    mainmenu((1280, 800), 'mainmenu_1280x800_pad', deck=True)


@recipe
def loading_4k():
    os.makedirs(TMP, exist_ok=True)
    p = os.path.join(TMP, 'navy.png')
    Image.new('RGBA', (3840, 2160), (6, 7, 22, 255)).save(p)
    s = Skin(p)
    w = 0.60
    x0 = (s.WH - w) / 2
    s.t(s.WH / 2, 0.470, 'PLANET MANHATTAN', 0.046, INK, 'mm')
    s.small(s.WH / 2, 0.515, 'NEW YORK  ·  LIBERTY', CHROME, 0.026, 'mm')
    s.segbar(s.h(x0), s.h(0.552), s.h(w), s.h(0.012), 0.62, CHROME, n=30)
    s.finish('loading_4k')


@recipe
def servers_4k():
    """The multiplayer server list: PAGE_ServerSelect.page's places (the list, its details, the four buttons); the rows are illustrative."""
    s = Skin(base(menu_scene(), (3840, 2160), 'menu'))
    s.scrim(0.70)
    x0, x1 = 0.26, s.WH - 0.26
    y = s.panel((x0, 0.100, x1, 0.640), 'SERVERS', 'INTERNET  ·  3 OF 41 SHOWN BY FILTER')
    cols = [(x0 + 0.030, 'NAME'), (x1 - 0.520, 'PLAYERS'), (x1 - 0.360, 'PING'), (x1 - 0.220, 'VERSION')]
    for x, c in cols:
        s.small(x, y + 0.012, c)
    rows = [('Discovery RP 24/7', '114 / 256', '38 ms', '1.1'), ('Sirius Revival Community', '27 / 128', '52 ms', '1.1'),
            ('Freelancer Veterans', '6 / 64', '97 ms', '1.1')]
    y += 0.060
    for i, r in enumerate(rows):
        yy = y + i * 0.056
        if i == 0:
            s.focus((x0 + 0.016, yy - 0.024, x1 - 0.016, yy + 0.024))
        for (x, _), v in zip(cols, r):
            s.t(x, yy, v, 0.032, ACTIVE if i == 0 else INK)
    s.glass((x0, 0.654, x1, 0.790), content=False)
    s.small(x0 + 0.022, 0.676, 'DETAILS')
    s.t(x0 + 0.022, 0.716, 'Roleplay server. New players welcome. Rules at the first base.', 0.030)
    s.t(x0 + 0.022, 0.756, 'Password: no. PvP: rules apply.', 0.030, CHROME)
    bw = (x1 - x0 - 3 * 0.014) / 4
    for i, c in enumerate(['BACK', 'SET FILTER', 'REFRESH', 'CONNECT']):
        xx = x0 + i * (bw + 0.014)
        s.chip((xx, 0.806, xx + bw, 0.872), c, focus=i == 3)
    s.lane([('A', 'CONNECT'), ('X', 'REFRESH'), ('Y', 'FILTER'), ('B', 'BACK')])
    s.finish('servers_4k_pad')


def cue(frame, name, k, cells, deck=False, numbers=False):
    """The weapon group cue on a real fight frame: groups, then M and T, in the ring's open top, at the text floor. A mark stands bare; only the
    selected group takes a hairline and only what is in reach a fill. A cell: (label, selected, lit, usable)."""
    s = Skin(frame, deck)
    cx, cy = s.W / 2, s.H / 2
    R = s.h(0.10 * k)
    cw, c, g, sep = s.h(0.020), s.h(0.026), s.h(0.003), s.h(0.008)
    n = len(cells)
    gaps = [(sep if cells[i][0].isdigit() and not cells[i + 1][0].isdigit() else g) for i in range(n - 1)]
    x = cx - (n * cw + sum(gaps)) / 2
    y = cy - R * 0.93 - c / 2
    for i, (d, sel, lit, ok) in enumerate(cells):
        col = AMBER if not ok else ACTIVE if sel else CHROME
        lit = lit and ok
        if lit or sel:
            def f(dr, P, q, x=x, col=col, lit=lit):
                ch = cw * 0.24
                pts = [(x + ch, y), (x + cw, y), (x + cw, y + c - ch), (x + cw - ch, y + c), (x, y + c), (x, y + ch)]
                if lit:
                    dr.polygon([P(*t) for t in pts], fill=col + (255,))
                else:
                    dr.line([P(*t) for t in pts + [pts[0]]], fill=col + (255,), width=int(s.h(0.0016) * q))
            s.shape(f, box=(x - 4, y - 4, x + cw + 4, y + c + 4))
        s.text((x + cw / 2, y + c / 2), d, s.h(0.024), fill=(6, 7, 22) if lit else col, anchor='mm', shadow=not lit)
        x += cw + (gaps[i] if i < n - 1 else 0)
    s.finish(name)


@recipe
def cue_4k():
    f = os.path.join(RUNS, 'focus_frames_2026-10-31', 'standard_combat-205904.png')
    cue(f, 'group_cue_4k', 1.0, [('1', True, True, True), ('2', False, False, True), ('3', False, True, True), ('M', False, True, True), ('T', False, False, True)])


@recipe
def cue_deck():
    f = os.path.join(RUNS, 'hud28_deckmode_2026-11-04', 'deck_combat-233052.png')
    cue(f, 'group_cue_1280x800', 1.08, [('1', True, True, True), ('2', False, True, True), ('3', False, False, True), ('M', False, False, False), ('T', False, True, True)], deck=True)


@recipe
def cue_states():
    """Every state of a cell, for the owner's reading, enlarged."""
    os.makedirs(TMP, exist_ok=True)
    p = os.path.join(TMP, 'cue_states.png')
    Image.new('RGBA', (1600, 900), (10, 12, 34, 255)).save(p)
    s = Skin(p)
    rows = [('1', False, False, True, 'A GROUP'), ('1', True, False, True, 'SELECTED'), ('2', False, True, True, 'IN REACH'),
            ('1', True, True, True, 'SELECTED, IN REACH'), ('M', False, True, True, 'MISSILES IN REACH'), ('T', False, False, False, 'NO TORPEDOES LEFT')]
    y = s.h(0.06)
    c = s.h(0.10)
    for d, sel, lit, ok, word in rows:
        x = s.h(0.10)
        col = AMBER if not ok else ACTIVE if sel else CHROME
        if lit or sel:
            def f(dr, P, q, x=x, y=y, col=col, lit=lit):
                ch = c * 0.22
                pts = [(x + ch, y), (x + c, y), (x + c, y + c - ch), (x + c - ch, y + c), (x, y + c), (x, y + ch)]
                if lit:
                    dr.polygon([P(*t) for t in pts], fill=col + (255,))
                else:
                    dr.line([P(*t) for t in pts + [pts[0]]], fill=col + (255,), width=int(s.h(0.006) * q))
            s.shape(f, box=(x - 4, y - 4, x + c + 4, y + c + 4))
        s.text((x + c / 2, y + c / 2), d, s.h(0.076), fill=(6, 7, 22) if lit else col, anchor='mm', shadow=not lit)
        s.text((x + c + s.h(0.04), y + c / 2), word, s.h(0.060), fill=INK)
        y += c + s.h(0.045)
    s.finish('group_cue_states')


def lean(frame, name, k, card, escort, cross, load, deck, keep_cross=False, plate_escort=False, cells=()):
    """The size audit's proposal on a real fight frame: the card at the spec's width, the escorts and consumables without plates. The old panels are
    painted out with a wide blur of the frame; the loadout, ring, alert and prompts are the frame's own. Rects in pixels."""
    im = Image.open(frame).convert('RGBA')
    soft = im.filter(ImageFilter.GaussianBlur(im.height * 0.05))
    # the consumables' marks, kept: light strokes on no plate
    cr = im.crop(cross)
    alpha = cr.convert('L').point(lambda v: 0 if v < 70 else min(255, (v - 70) * 3))
    cr.putalpha(alpha)
    from PIL import ImageDraw
    mask = Image.new('L', im.size, 0)
    dm = ImageDraw.Draw(mask)
    for r in (card, escort) + (() if keep_cross else (cross,)):
        dm.rectangle(r, fill=255)
    mask = mask.filter(ImageFilter.MaxFilter(9)).filter(ImageFilter.GaussianBlur(im.height * 0.012))
    im.paste(soft, (0, 0), mask)
    os.makedirs(TMP, exist_ok=True)
    p = os.path.join(TMP, name + '_base.png')
    im.save(p)
    s = Skin(p, deck)
    H = s.H
    for b in cells:   # the owner's choice: no outer plate, a small dark cell under each item
        s.plate(b, chamfer=s.h(0.008), alpha=215, rim_alpha=0, corners=(1, 1, 1, 1))
    if not keep_cross:
        s.paste(cr, cross[:2])
    # the card: 0.36 x 0.17, the icon column cut to the icon
    t = 1.08 if deck else 1.0
    w, h = 0.36 * t, 0.170 * t
    x1 = card[2]
    x0 = x1 - s.h(w)
    y0 = card[1] + (s.h(0.012) if deck else 0)
    y1 = y0 + s.h(h)
    s.plate((x0, y0, x1, y1), chamfer=s.h(0.012), alpha=175, rim_alpha=110, corners=(0, 1, 0, 1), accent=('l', HOSTILE))
    tx = x0 + s.h(0.085 * t)
    r = x1 - s.h(0.018)
    z = 0.026 * t
    icon = (x0 + s.h(0.046 * t), (y0 + y1) / 2)
    d = s.h(0.024 * t)
    s.shape(lambda dr, P, q: dr.polygon([P(icon[0] - d, icon[1] - d * 0.7), P(icon[0] + d, icon[1] - d * 0.7), P(icon[0], icon[1] + d)], fill=HOSTILE + (255,)),
            box=(icon[0] - d, icon[1] - d, icon[0] + d, icon[1] + d))
    y = y0 + s.h(0.030 * t)
    s.text((tx, y), 'HOSTILE', s.h(z), fill=HOSTILE)
    s.text((r, y), 'FIGHTER  LV 3', s.h(z), fill=CHROME, anchor='rm')
    y += s.h(0.040 * t)
    s.text((tx, y), 'Dustin Leinenbach', s.h(0.032 * t), fill=INK)
    y += s.h(0.032 * t)
    s.text((tx, y), 'Rogues', s.h(z), fill=HOSTILE)
    s.text((r, y), '427 m', s.h(0.032 * t), fill=INK, anchor='rm')
    y += s.h(0.030 * t)
    bw = r - tx - s.h(0.036 * t)
    s.segbar(tx, y - s.h(0.006), bw, s.h(0.009), 0.30, (78, 132, 255), n=30)
    s.text((r, y), '30', s.h(0.024), fill=(78, 132, 255), anchor='rm')
    y += s.h(0.024 * t)
    s.segbar(tx, y - s.h(0.006), bw, s.h(0.009), 0.74, (214, 62, 62), n=30)
    s.text((r, y), '74', s.h(0.024), fill=(214, 62, 62), anchor='rm')
    # the escorts: free text, inline bars, a soft wash under them
    ex = escort[0] + s.h(0.016)
    n = ['Escort 1 \u00b7 Charackli', 'Escort 2 \u00b7 Cazamias', 'Escort 3 \u00b7 Carlson']
    v = [0.9, 0.7, 0.2]
    ey = escort[3] - s.h(0.100 * t)
    if plate_escort:
        s.plate((ex - s.h(0.012), ey - s.h(0.026 * t), ex + s.h(0.292 * t), ey + s.h(0.106 * t)), chamfer=s.h(0.012), alpha=175, rim_alpha=110)
    else:
        s._scrim_soft((ex - s.h(0.01), ey - s.h(0.02), ex + s.h(0.28 * t), ey + s.h(0.095 * t)), alpha=60)
    for i in range(3):
        yy = ey + i * s.h(0.040 * t)
        s.text((ex, yy), n[i], s.h(z), fill=INK)
        s.segbar(ex + s.h(0.205 * t), yy - s.h(0.005), s.h(0.075 * t), s.h(0.009), v[i], (96, 214, 96), n=10)
    s.finish(name)


@recipe
def lean_4k():
    f = os.path.join(RUNS, 'focus_frames_2026-10-31', 'standard_combat-205904.png')
    lean(f, 'flight_lean_4k', 1.0, (2700, 100, 3660, 590), (180, 1720, 1070, 2060), (3170, 1410, 3640, 1710), None, False, plate_escort=True,
         cells=[(3200, 1440, 3400, 1540), (3420, 1440, 3620, 1540), (3200, 1584, 3400, 1680), (3420, 1584, 3620, 1680)])


@recipe
def lean_deck():
    f = os.path.join(RUNS, 'hud28_deckmode_2026-11-04', 'deck_combat-233052.png')
    lean(f, 'flight_lean_1280x800', 1.08, (880, 0, 1270, 202), (10, 652, 268, 792), (1086, 516, 1270, 656), None, True, plate_escort=True,
         cells=[(1156, 528, 1204, 568), (1104, 566, 1154, 610), (1204, 566, 1252, 610), (1156, 604, 1204, 648), (1162, 572, 1194, 602)])


@recipe
def plates_compare():
    a4 = (os.path.join(RUNS, 'focus_frames_2026-10-31', 'standard_combat-205904.png'), 1.0, (2700, 100, 3660, 590), (180, 1720, 1070, 2060), (3170, 1410, 3640, 1710), None, False)
    ad = (os.path.join(RUNS, 'hud28_deckmode_2026-11-04', 'deck_combat-233052.png'), 1.08, (880, 0, 1270, 202), (10, 652, 268, 792), (1086, 516, 1270, 656), None, True)
    for tag, kc, pe in (('a_both_off', False, False), ('b_consumables_off', False, True), ('c_escorts_off', True, False), ('d_both_kept', True, True)):
        for sz, a in (('4k', a4), ('1280', ad)):
            f, k, c, e, x, l, d = a
            lean(f, 'plates_%s_%s' % (tag, sz), k, c, e, x, l, d, keep_cross=kc, plate_escort=pe)


@recipe
def plates_bright():
    """Worst case: the two corners with and without plates over the brightest scene at hand, the main menu's lit planet."""
    src = Image.open(os.path.join(RUNS, 'hud28_deckmode_2026-11-04', 'deck_combat-233052.png')).convert('RGBA')
    cross = (1086, 516, 1270, 656)
    os.makedirs(TMP, exist_ok=True)
    p = os.path.join(TMP, 'bright.png')
    Image.open(menu_scene()).convert('RGBA').crop((2500, 1250, 3700, 2000)).resize((1280, 800), Image.LANCZOS).save(p)
    for plate, name, cells in ((False, 'plates_bright_off', False), (True, 'plates_bright_on', False), (False, 'plates_bright_cells', True)):
        sk = Skin(p, True)
        cr = src.crop(cross)
        if not plate:
            cr = src.crop((cross[0] + 12, cross[1] + 12, cross[2] - 10, cross[3] - 8))
            cr.putalpha(cr.convert('L').point(lambda v: 0 if v < 70 else min(255, (v - 70) * 3)))
        if cells:   # no outer plate: each item keeps a small dark cell of its own
            for b in ((1156, 528, 1204, 568), (1104, 566, 1154, 610), (1204, 566, 1252, 610), (1156, 604, 1204, 648), (1162, 572, 1194, 602)):
                x0, y0, x1, y1 = b[0] - cross[0] - 12 + 470, b[1] - cross[1] - 12 + 480, b[2] - cross[0] - 12 + 470, b[3] - cross[1] - 12 + 480
                sk.plate((x0, y0, x1, y1), chamfer=sk.h(0.008), alpha=215, rim_alpha=0, corners=(1, 1, 1, 1))
        sk.paste(cr, (470, 480))
        ex, ey, t = sk.h(0.86), sk.h(0.66), 1.08
        if plate:
            sk.plate((ex - sk.h(0.012), ey - sk.h(0.028), ex + sk.h(0.32), ey + sk.h(0.115)), chamfer=sk.h(0.012), alpha=175, rim_alpha=110)
        else:
            sk._scrim_soft((ex - sk.h(0.01), ey - sk.h(0.02), ex + sk.h(0.30), ey + sk.h(0.10)), alpha=60)
        for i, (nm, v) in enumerate((('Escort 1 \u00b7 Charackli', 0.9), ('Escort 2 \u00b7 Cazamias', 0.7), ('Escort 3 \u00b7 Carlson', 0.2))):
            yy = ey + i * sk.h(0.043)
            sk.text((ex, yy), nm, sk.h(0.028), fill=INK)
            sk.segbar(ex + sk.h(0.22), yy - sk.h(0.005), sk.h(0.08), sk.h(0.009), v, (96, 214, 96), n=10)
        sk.finish(name)


if __name__ == '__main__':
    want = sys.argv[1:] or list(RECIPES)
    for n in want:
        RECIPES[n]()
        print('done', n)
