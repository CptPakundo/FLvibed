"""hud2x.py - Extended HUD 2 (docs\\UI_HUD2_EXTENDED.md 9): the non-flight screens in HUD 2's skin, on game frames.

    research\\tools\\ocr\\.venv\\Scripts\\python.exe -I tools\\uimock\\hud2x.py [screen ...]

The technique it mocks is the one the build uses: the window's ART is hidden and HUD 2 glass is drawn UNDER the UI
pass, so the game's own texts, rows and buttons draw on top. The frames come in three layers per screen (instance 2,
4K, keyboard-and-mouse mode, FLvibed's pad layer off; research\\experience\\runs\\hud2x_layers_2026-10-25\\):
A = native, B = the art hidden (room + the game's content), C = the art and the content hidden (the room alone).
A mock = C, then FLvibed's layer (scrim, plates, focus bar, strip, prompt pills), then B's pixels where B differs from
C (the content). Sizes are fractions of H; the palette and the kit are HUD 2's (hud2.py).
"""
import json
import os
import sys

from PIL import Image, ImageChops, ImageFilter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hud2 as h2  # noqa: E402
import hudmock as hm  # noqa: E402

LAYERS = os.path.join(hm.ROOT, 'research', 'experience', 'runs', 'hud2x_layers_2026-10-25')
OUT = os.path.join(hm.EVID, 'hud2x')

INK, CHROME, DIM, GLASS, ACTIVE = h2.INK, h2.CHROME, h2.DIM, h2.GLASS, h2.ACTIVE
GOOD, FAIR, HIGH = (96, 214, 96), (232, 210, 92), (236, 96, 72)

PLATE_ALPHA = 0.85   # over rooms (research: 75-85 % for 4.5:1 on the brightest rooms)
SCRIM_ALPHA = 0.70   # modal menus (replaces the black plane; critique 1: 55 % left the room competing)


def layer_path(screen, k):
    return os.path.join(LAYERS, f'{screen}_{k}.png')


def rects(screen):
    p = os.path.join(LAYERS, f'{screen}_rects.json')
    return json.load(open(p, encoding='utf-8')) if os.path.exists(p) else {}




class Screen(h2.HUD):
    """A frame being skinned. Drawing order = the build's: UNDER the UI pass (scrim, glass, focus fill) -> the game's own
    content (layer B, only inside the window's regions) -> FLvibed's OVERLAY (strip, prompt pills, card, words, glyphs)."""

    def __init__(self, screen, B='B'):
        self.screen = screen
        super().__init__(layer_path(screen, 'C'))
        self.B = Image.open(layer_path(screen, B)).convert('RGBA')
        self.R = rects(screen)
        self.regions = []   # where the game's content lives (critique 1 A1: nothing outside them is pasted)
        self.keeps = []     # FLvibed's own native-hosted drawing present in B and C alike (pasted as is, on top)
        self.drops = []     # native content the design hides (state bit 1 on that content control)
        self.over = []      # the overlay, drawn after the content

    def px(self, r):
        return tuple(v * self.H for v in r)

    def region(self, *rs):
        self.regions += list(rs)

    def keep(self, *rs):
        self.keeps += list(rs)

    def drop(self, *rs):
        self.drops += list(rs)

    def later(self, fn, *a, **k):
        self.over.append(lambda: fn(*a, **k))

    # ---- under the UI pass ----
    def scrim(self, alpha=SCRIM_ALPHA):
        self.paste(Image.new('RGBA', (self.W, self.H), (6, 7, 22, int(255 * alpha))))

    def rim(self, box, chamfer, corners, alpha=175, w=0.0013):
        """A firmer rim than hud2's plate hairline: the native cyan frames beside it out-shouted 45 % (critique 1, 6)."""
        x0, y0, x1, y1 = box
        tl, tr, br, bl = [chamfer * k for k in corners]
        pts = [(x0 + tl, y0), (x1 - tr, y0), (x1, y0 + tr), (x1, y1 - br), (x1 - br, y1), (x0 + bl, y1), (x0, y1 - bl),
               (x0, y0 + tl), (x0 + tl, y0)]
        self.line(pts, col=CHROME, w=max(2, self.h(w)), alpha=alpha)

    def glass(self, r, rule=None, accent=None, corners=(1, 0, 1, 0), alpha=PLATE_ALPHA, content=True):
        """One HUD 2 plate per native panel (r in fractions of H). `rule` = (y, x0, x1) of a header rule placed from the
        native title's own extent (critique 1, 2), or None where the panel has its own divider."""
        box = self.px(r)
        c = self.h(0.012)
        self.plate(box, chamfer=c, alpha=int(255 * alpha), rim_alpha=0, corners=corners, accent=accent)
        self.rim(box, c, corners)
        if rule:
            y, a, b = rule
            self.line([(self.h(a), self.h(y)), (self.h(b), self.h(y))], col=CHROME, w=max(2, self.h(0.0012)), alpha=130)
        if content:
            self.region(r)

    def focus(self, r):
        """The one focus style (critique 1, 3): the drawn element's box filled active yellow at 22 % (14 % read khaki), and
        a 0.005 H yellow bar OUTSIDE its left edge (under a native icon cell it vanished)."""
        x0, y0, x1, y1 = self.px(r)
        g = self.h(0.008)

        def f(d, P, s):
            d.rectangle([P(x0, y0), P(x1, y1)], fill=ACTIVE + (72,))
            d.rectangle([P(x0 - g - self.h(0.005), y0), P(x0 - g, y1)], fill=ACTIVE + (255,))
        self.shape(f, box=(x0 - g - self.h(0.006), y0, x1, y1))

    # ---- the overlay ----
    def _scrim_soft(self, box, alpha=150):
        """HUD 2's soft scrim: a navy wash with blurred edges (critique 1, 4: the first one read as a hard 43 % box)."""
        from PIL import ImageDraw
        x0, y0, x1, y1 = [int(v) for v in box]
        pad = int(self.h(0.05))
        lay = Image.new('RGBA', (x1 - x0 + 2 * pad, y1 - y0 + 2 * pad), (0, 0, 0, 0))
        ImageDraw.Draw(lay).rectangle([pad, pad, pad + x1 - x0, pad + y1 - y0], fill=(6, 7, 22, alpha))
        self.paste(lay.filter(ImageFilter.GaussianBlur(self.h(0.022))), (x0 - pad, y0 - pad))

    def strip(self, room, delta=None, ship=True):
        """The context strip (docked): two lines at the safe margin, left of the room bar - the base and the room; credits,
        cargo, ship. A pending trade shows its effect as deltas in yellow (critique 1: compact, no 'before > after')."""
        def draw():
            x, y = self.W * 0.05, self.H * 0.05
            self._scrim_soft((x - self.h(0.015), y - self.h(0.01), x + self.h(0.50), y + self.h(0.135)), alpha=165)
            bw = self.text((x, y + self.h(0.022)), STRIP['base'].upper(), self.h(0.036), fill=INK)
            rw = self.label((x + bw + self.h(0.016), y + self.h(0.024)), room, self.h(0.026), col=ACTIVE, track=0.08)
            self.label((x + bw + rw + self.h(0.030), y + self.h(0.024)), STRIP['system'].upper(), self.h(0.024), col=CHROME)
            cx, cy = x, y + self.h(0.078)
            items = [('CREDITS', STRIP['credits']), ('CARGO', STRIP['cargo'])] + ([('SHIP', STRIP['ship'])] if ship else [])
            for lab, val in items:
                cx += self.label((cx, cy), lab, self.h(0.024), col=CHROME) + self.h(0.010)
                cx += self.text((cx, cy), val, self.h(0.030), fill=INK)
                if delta and lab in delta:
                    cx += self.h(0.010) + self.text((cx + self.h(0.010), cy), delta[lab], self.h(0.030), fill=ACTIVE)
                cx += self.h(0.032)
        self.later(draw)

    def bar_glyphs(self, r):
        x0, y0, x1, y1 = self.px(r)
        self.later(self.glyph, 'LB', x0 - self.h(0.030), (y0 + y1) / 2, self.h(0.032))
        self.later(self.glyph, 'RB', x1 + self.h(0.030), (y0 + y1) / 2, self.h(0.032))

    def pills(self, items, lane='top'):
        """FLvibed's prompt bar in HUD 2's look, in its own lane (critique 1, 1): docked screens = top right under the
        NeuroNet box (the windows reach the bottom edge); menus = the bottom lane under their plates."""
        def draw():
            size, g = self.h(0.028), self.h(0.032)
            right = self.W - self.W * 0.05
            y = self.h(0.112) if lane == 'top' else self.h(0.945)
            x = right
            for glyph, verb in reversed(items):
                w = g + self.h(0.010) + self.textlen(verb, size) + self.h(0.028)
                x0 = x - w
                self.plate((x0, y - self.h(0.024), x, y + self.h(0.024)), chamfer=self.h(0.008), alpha=205, rim_alpha=80,
                           corners=(1, 1, 1, 1))
                self.glyph(glyph, x0 + self.h(0.014) + g / 2, y, g)
                self.text((x0 + self.h(0.014) + g + self.h(0.010), y), verb, size, fill=INK)
                x = x0 - self.h(0.010)
        self.later(draw)

    def card(self, cx, top, faction, name, col):
        """The character card (pad hover in rooms), replacing the game's grey Arial tooltip."""
        def draw():
            X, T = self.h(cx), self.h(top)
            w = max(self.textlen(name, self.h(0.034)), self.textlen(faction.upper(), self.h(0.024)) * 1.12) + self.h(0.05)
            x0 = X - w / 2
            self.plate((x0, T, x0 + w, T + self.h(0.085)), alpha=215, rim_alpha=110, accent=('l', col))
            self.label((x0 + self.h(0.022), T + self.h(0.024)), faction.upper(), self.h(0.024), col=CHROME)
            self.text((x0 + self.h(0.022), T + self.h(0.058)), name, self.h(0.034), fill=INK)
        self.later(draw)

    def word(self, x, y, s, col=None, anchor='rm', size=0.024):
        col = col or (104, 160, 200)   # chrome one step down: a label, not a headline (critique 2, 2)
        self.later(self.label, (self.h(x), self.h(y)), s, self.h(size), col=col, anchor=anchor, track=0.06)

    def deal(self, x, y, w):
        """The price-quality word left of the native square (colour + word; the side's own sense: a dealer's low price
        is GOOD to buy, the player's high offer GOOD to sell)."""
        self.word(x, y, w, col={'GOOD': GOOD, 'FAIR': FAIR, 'HIGH': HIGH, 'LOW': HIGH}[w])

    def scroll(self, r, top, frac):
        """A HUD 2 scroll bar in the native bar's place (critique 2, 6): a 0.005 H track, the thumb from the bar's own
        range and position (padbases already reads them); the native bar's art hidden, the control kept for clicks."""
        x0, y0, x1, y1 = self.px(r)
        cx = (x0 + x1) / 2
        w = self.h(0.005)
        self.drop(r)

        def draw():
            def f(d, P, s):
                d.rectangle([P(cx - w / 2, y0), P(cx + w / 2, y1)], fill=CHROME + (60,))
                t0 = y0 + (y1 - y0) * top
                d.rectangle([P(cx - w, t0), P(cx + w, t0 + (y1 - y0) * frac)], fill=CHROME + (230,))
            self.shape(f, box=(cx - w * 2, y0, cx + w * 2, y1))
        self.later(draw)

    def tab(self, r, caption, current=False):
        """A native page tab with its art hidden: a HUD 2 pill under its caption (mock: the caption re-lettered)."""
        def draw():
            x0, y0, x1, y1 = self.px(r)
            self.plate((x0, y0, x1, y1), chamfer=self.h(0.010), alpha=220, rim_alpha=0, corners=(1, 0, 1, 0))
            self.rim((x0, y0, x1, y1), self.h(0.010), (1, 0, 1, 0), alpha=200 if current else 120)
            if current:
                self.line([(x0 + self.h(0.012), y1 - self.h(0.004)), (x1 - self.h(0.012), y1 - self.h(0.004))], col=ACTIVE,
                          w=self.h(0.004))
            self.text(((x0 + x1) / 2, (y0 + y1) / 2), caption, self.h(0.030), fill=ACTIVE if current else INK, anchor='mm')
        self.later(draw)

    # ---- composition ----
    def content(self, thr=14, gain=5):
        from PIL import ImageDraw
        C = Image.open(layer_path(self.screen, 'C')).convert('RGB')
        diff = ImageChops.difference(self.B.convert('RGB'), C)
        m = diff.convert('L').point(lambda v: 0 if v < thr else min(255, (v - thr) * gain))
        # the game's content is light (text, wireframes, icons); what moved in the room between B and C (patrons, smoke)
        # is mostly dark: keep the light differences only (critique 1 A1)
        light = self.B.convert('L').point(lambda v: 0 if v < 70 else min(255, (v - 70) * 4))
        m = ImageChops.multiply(m, light.filter(ImageFilter.MaxFilter(5)))
        m = m.filter(ImageFilter.MaxFilter(3)).filter(ImageFilter.GaussianBlur(0.8))
        inside = Image.new('L', m.size, 0)
        di = ImageDraw.Draw(inside)
        for r in self.regions:
            di.rectangle(self.px(r), fill=255)
        for r in self.drops:
            di.rectangle(self.px(r), fill=0)
        m = ImageChops.multiply(m, inside)
        top = self.B.copy()
        top.putalpha(m)
        self.im.alpha_composite(top)
        for r in self.keeps:
            box = tuple(int(v) for v in self.px(r))
            self.im.alpha_composite(self.B.crop(box), box[:2])

    def done(self, name=None):
        self.content()
        for fn in self.over:
            fn()
        name = name or self.screen
        self.save(os.path.join(OUT, f'{name}.png'))
        a = layer_path(self.screen, 'A')
        pair = Image.new('RGB', (1920 * 2 + 16, 1080), (10, 10, 20))
        pair.paste(Image.open(a).convert('RGB').resize((1920, 1080), Image.LANCZOS), (0, 0))
        pair.paste(self.im.convert('RGB').resize((1920, 1080), Image.LANCZOS), (1936, 0))
        pair.save(os.path.join(OUT, f'{name}_pair.jpg'), quality=88)


RECIPES = {}


def recipe(fn):
    RECIPES[fn.__name__] = fn
    return fn


# ---- the docked frame every base screen shares: the room bar, the NeuroNet box, the context strip ----
ROOM_X = {'BAR': 0.806, 'TRADER': 0.889, 'EQUIPMENT': 0.975, 'SHIP DEALER': 1.055, 'CITYSCAPE': 0.722}
STRIP = dict(base='Planet Manhattan', system='New York', credits='$500,000', cargo='0 / 50', ship='Starflier')
BAR = (0.665, -0.02, 1.112, 0.083)
NNBOX = (1.488, -0.02, 1.768, 0.066)


def small_plate(s, r, corners):
    x0, y0, x1, y1 = s.px(r)
    s.plate((x0, y0, x1, y1), alpha=int(255 * 0.82), rim_alpha=0, corners=corners)
    s.rim((x0, y0, x1, y1), s.h(0.012), corners)
    s.region(r)


def docked(s, room, delta=None, pad=True, subtab=True, bar=True, ship=True):
    if bar:
        small_plate(s, BAR, (0, 0, 1, 1))
        x0, y0, x1, y1 = s.px(BAR)
        cx, u = s.h(ROOM_X[room]), s.h(0.0045)
        s.line([(cx - s.h(0.030), y1 - u * 1.6), (cx + s.h(0.030), y1 - u * 1.6)], col=ACTIVE, w=u)
        if subtab:   # the current room's sub-buttons (the game's tab art hidden): a small plate under them
            small_plate(s, (ROOM_X[room] - 0.062, 0.088, ROOM_X[room] + 0.062, 0.156), (0, 0, 1, 1))
        small_plate(s, NNBOX, (0, 0, 0, 1))
        if pad:
            s.bar_glyphs(BAR)
    s.strip(room, delta=delta, ship=ship)


@recipe
def room_trader():
    s = Screen('room_trader')
    docked(s, 'TRADER')
    s.card(0.985, 0.505, 'Cryer Pharmaceuticals', 'Captain Will Thrift', h2.NEUTRAL)
    s.pills([('D', 'CHARACTER'), ('A', 'TALK')])
    s.done()


@recipe
def room_bar():
    s = Screen('room_bar')
    docked(s, 'CITYSCAPE', subtab=False)
    s.pills([('A', 'ENTER BAR')])
    s.done()


def jobboard_recipe(kbm=False):
    s = Screen('jobboard')
    docked(s, 'BAR', pad=not kbm)
    s.glass((0.385, 0.150, 1.425, 0.800), rule=(0.216, 0.405, 1.405))
    s.line([(s.h(0.405), s.h(0.556)), (s.h(1.405), s.h(0.556))], col=CHROME, w=max(2, s.h(0.0012)), alpha=110)
    s.focus((0.417, 0.248, 0.9236, 0.302))
    s.scroll((0.958, 0.244, 0.992, 0.552), 0.0, 0.5)
    s.drop((0.500, 0.552, 0.630, 0.584))   # the pad hover's grey tooltip ("Assassinate"): said in words below instead
    s.word(1.290, 0.592, 'ASSASSINATE \u00b7 DIFFICULTY 2', col=CHROME)
    if not kbm:
        s.pills([('D', 'JOB'), ('A', 'ACCEPT'), ('RS', 'SCROLL'), ('B', 'CLOSE')])
    s.done('jobboard_kbm' if kbm else 'jobboard')


@recipe
def jobboard():
    jobboard_recipe()


@recipe
def jobboard_kbm():
    jobboard_recipe(kbm=True)


@recipe
def news():
    s = Screen('news')
    docked(s, 'BAR')
    s.glass((0.385, 0.150, 1.425, 0.930), rule=(0.216, 0.405, 1.405))
    s.line([(s.h(0.405), s.h(0.560)), (s.h(1.405), s.h(0.560))], col=CHROME, w=max(2, s.h(0.0012)), alpha=110)
    s.line([(s.h(0.405), s.h(0.850)), (s.h(1.405), s.h(0.850))], col=CHROME, w=max(2, s.h(0.0012)), alpha=110)
    s.focus((0.505, 0.250, 0.929, 0.302))
    s.pills([('D', 'STORY'), ('B', 'CLOSE')])
    s.done()


def commodity_recipe(kbm=False):
    s = Screen('commodity')
    docked(s, 'TRADER', delta={'CREDITS': '\u2212$66', 'CARGO': '+1'}, ship=False, pad=not kbm)
    s.glass((0.250, 0.170, 0.672, 0.335), rule=(0.262, 0.270, 0.652))   # the player's list is empty: collapsed
    s.word(0.461, 0.300, 'NOTHING TO SELL', anchor='mm')
    s.glass((0.690, 0.170, 1.082, 0.665))
    s.glass((1.096, 0.170, 1.522, 0.630), rule=(0.262, 1.116, 1.502))
    s.glass((0.300, 0.690, 1.440, 0.975), rule=(0.742, 0.320, 1.420))
    s.drop((0.36, 0.225, 0.56, 0.262), (0.26, 0.600, 0.45, 0.660))   # the dealer's credits line and cargo bar (strip)
    s.scroll((1.482, 0.270, 1.506, 0.620), 0.0, 0.62)
    s.focus((1.1144, 0.334, 1.4708, 0.402))
    for y, w in [(0.314, 'GOOD'), (0.383, 'GOOD'), (0.452, 'FAIR'), (0.520, 'HIGH'), (0.589, 'GOOD')]:
        s.deal(1.380, y, w)
    if not kbm:
        s.pills([('D', 'ITEM'), ('LS', 'QUANTITY'), ('A', 'BUY'), ('B', 'CLOSE')])
    s.done('commodity_kbm' if kbm else 'commodity')


@recipe
def commodity():
    commodity_recipe()


@recipe
def commodity_kbm():
    commodity_recipe(kbm=True)


@recipe
def equipment():
    s = Screen('equipment')
    docked(s, 'EQUIPMENT')
    s.glass((0.250, 0.170, 0.672, 0.665), rule=(0.262, 0.270, 0.652))
    s.glass((1.096, 0.170, 1.522, 0.630), rule=(0.262, 1.116, 1.502))
    s.glass((0.690, 0.170, 1.082, 0.500))   # the welcome panel stays (the game toggles its texts itself)
    s.drop((0.36, 0.225, 0.56, 0.262), (0.26, 0.600, 0.45, 0.660))   # credits + cargo bar (strip)
    s.region((0.44, 0.600, 0.70, 0.665))   # the filter icons stay
    s.focus((1.1144, 0.264, 1.4708, 0.333))
    s.word(1.309, 0.655, 'VS MOUNTED JUSTICE MK I:  DAMAGE +18 %  \u00b7  REFIRE =  \u00b7  RANGE \u221250 M', anchor='mm')
    s.pills([('D', 'ITEM'), ('A', 'SELECT'), ('Y', 'FILTER'), ('B', 'CLOSE')])
    s.done()


@recipe
def shipdealer():
    s = Screen('shipdealer')
    docked(s, 'SHIP DEALER')   # the game refuses this purchase (level): no delta
    s.glass((0.262, 0.215, 0.672, 0.430), rule=(0.290, 0.282, 0.652))
    s.glass((0.780, 0.175, 1.000, 0.255))            # the selected ship's name only: the 3D ship floats over the room
    s.glass((0.690, 0.570, 0.960, 0.610))            # "You must be level 1 to purchase this."
    s.glass((1.110, 0.215, 1.522, 0.560), rule=(0.290, 1.130, 1.502))
    s.glass((0.310, 0.690, 1.400, 0.975), rule=(0.742, 0.330, 1.380))
    s.drop((0.40, 0.288, 0.56, 0.312))
    s.focus((1.129, 0.330, 1.486, 0.400))
    s.word(1.500, 0.585, 'TRADE-IN $4,800  \u00b7  NET $2,800', anchor='rm')
    s.word(1.500, 0.620, 'NEEDS LEVEL 1', col=(232, 163, 58), anchor='rm')
    s.pills([('D', 'SHIP'), ('A', 'SELECT'), ('B', 'CLOSE')])
    s.done()


@recipe
def offer_dialog():
    s = Screen('offer_dialog')
    docked(s, 'BAR', bar=False)
    s.glass((0.450, 0.215, 1.320, 0.765), rule=(0.300, 0.470, 1.300), accent=('l', h2.FRIEND))
    s.word(0.885, 0.258, 'LIBERTY POLICE, INC.', anchor='mm')
    s.word(0.885, 0.610, 'REWARD $2,400  \u00b7  ACCEPTING SETS THE MISSION\u2019S FIRST WAYPOINT', anchor='mm')
    s.focus((0.650, 0.684, 0.834, 0.735))
    s.pills([('A', 'ACCEPT'), ('B', 'DECLINE')])
    s.done()


def pause_like(name):
    s = Screen(name)
    s.scrim()
    s.glass((0.620, 0.170, 1.158, 0.800), rule=(0.236, 0.640, 1.138))
    s.focus((0.662, 0.271, 1.116, 0.343))   # the drawn frame (its hit box sits ~0.027 H above it)
    s.pills([('D', 'ITEM'), ('A', 'SELECT'), ('B', 'RESUME')], lane='bottom')
    s.done()


@recipe
def pause_docked():
    pause_like('pause_docked')


@recipe
def pause_space():
    pause_like('pause_space')


def saveload(name, save):
    s = Screen(name)
    s.scrim()
    for y in (0.167, 0.280, 0.393, 0.506):   # the left buttons lose their art with the frame: HUD 2 plates
        s.glass((0.321, y - 0.030, 0.642, y + 0.030), corners=(1, 1, 1, 1), alpha=0.80)
    s.glass((0.321, 0.620, 0.660, 0.860), alpha=0.80)
    s.glass((0.690, 0.150, 1.430, 0.860), rule=(0.222, 0.710, 1.410))
    if save:   # the name field gets a visible field (critique 1, 9)
        x0, y0, x1, y1 = s.px((0.708, 0.290, 1.410, 0.345))
        s.plate((x0, y0, x1, y1), alpha=150, rim_alpha=140, corners=(0, 0, 0, 0))
    s.focus((0.321, 0.137, 0.642, 0.197))
    s.pills([('D', 'ITEM'), ('A', 'SAVE'), ('Y', 'NAME')] if save else [('D', 'ITEM'), ('A', 'LOAD')], lane='bottom')
    s.done()


@recipe
def save_dialog():
    saveload('save_dialog', True)


@recipe
def load_ingame():
    saveload('load_ingame', False)


def nn_window(name, box, rule, room='EQUIPMENT', pills=(('B', 'CLOSE'),), drops=()):
    s = Screen(name)
    docked(s, room)
    s.glass(box, rule=rule)
    s.drop(*drops)
    s.pills(list(pills))
    return s


@recipe
def nn_inventory_docked():
    s = nn_window('nn_inventory_docked', (0.440, 0.150, 1.340, 0.700), (0.207, 0.460, 1.320),
                  drops=((0.590, 0.220, 0.760, 0.250), (0.480, 0.605, 0.620, 0.665)))   # credits + cargo bar (strip)
    s.done()


@recipe
def nn_log_docked():
    nn_window('nn_log_docked', (0.460, 0.150, 1.340, 0.690), (0.207, 0.480, 1.320),
              pills=(('RS', 'SCROLL'), ('B', 'CLOSE'))).done()


@recipe
def nn_playerinfo_docked():
    nn_window('nn_playerinfo_docked', (0.440, 0.150, 1.340, 0.700), (0.207, 0.460, 1.320),
              pills=(('RS', 'SCROLL'), ('B', 'CLOSE'))).done()


@recipe
def nn_help_docked():
    nn_window('nn_help_docked', (0.440, 0.150, 1.340, 0.700), (0.207, 0.460, 1.320),
              pills=(('RS', 'SCROLL'), ('B', 'CLOSE'))).done()


@recipe
def nn_navmap_docked():
    nn_window('nn_navmap_docked', (0.600, 0.150, 1.170, 0.735), None,
              pills=(('LS', 'CURSOR'), ('A', 'SELECT'), ('B', 'CLOSE'))).done()


@recipe
def nn_navmap_space():
    s = Screen('nn_navmap_space')
    s.glass((0.600, 0.120, 1.170, 0.735))
    s.pills([('LS', 'CURSOR'), ('A', 'SELECT'), ('B', 'CLOSE')])
    s.done()


@recipe
def room_trader_43():
    """4:3 by the centre crop of the 16:9 frame (the room bar and the windows are centred; measured in game later): the
    strip's budget = the bar's left edge - the safe margin - 0.03 H; it wraps the room / system onto a second line and
    drops the ship (the inventory has it)."""
    s = Screen('room_trader')
    docked(s, 'TRADER', ship=False)
    s.card(0.985, 0.505, 'Cryer Pharmaceuticals', 'Captain Will Thrift', h2.NEUTRAL)
    s.pills([('D', 'CHARACTER'), ('A', 'TALK')])
    s.content()
    s.over = []
    crop = int((s.W - s.H * 4 / 3) / 2)
    s.im = s.im.crop((crop, 0, s.W - crop, s.H))
    s.W = s.im.width
    STRIP43 = dict(STRIP)
    x, y = s.W * 0.05, s.H * 0.05
    s._scrim_soft((x - s.h(0.015), y - s.h(0.01), x + s.h(0.33), y + s.h(0.17)), alpha=165)
    s.text((x, y + s.h(0.022)), STRIP43['base'].upper(), s.h(0.036), fill=INK)
    rw = s.label((x, y + s.h(0.064)), 'TRADER', s.h(0.026), col=ACTIVE, track=0.08)
    s.label((x + rw + s.h(0.014), y + s.h(0.064)), 'NEW YORK', s.h(0.024), col=CHROME)
    cx, cy = x, y + s.h(0.108)
    for lab, val in [('CREDITS', STRIP['credits']), ('CARGO', STRIP['cargo'])]:
        cx += s.label((cx, cy), lab, s.h(0.024), col=CHROME) + s.h(0.010)
        cx += s.text((cx, cy), val, s.h(0.030), fill=INK) + s.h(0.030)
    bx0 = (0.665 * s.H - crop)
    s.glyph('LB', bx0 - s.h(0.030), s.h(0.031), s.h(0.032))
    s.glyph('RB', (1.112 * s.H - crop) + s.h(0.030), s.h(0.031), s.h(0.032))
    s.save(os.path.join(OUT, 'room_trader_43.png'))


@recipe
def options_ingame_video():
    s = Screen('options_ingame_video')
    s.scrim()
    s.glass((0.710, 0.150, 1.480, 0.920), rule=(0.228, 0.730, 1.460))
    for r, cap, cur in [((0.332, 0.201, 0.668, 0.268), 'GENERAL', False), ((0.332, 0.302, 0.668, 0.367), 'CONTROLS', False),
                        ((0.332, 0.400, 0.668, 0.468), 'PERFORMANCE', True), ((0.332, 0.500, 0.668, 0.568), 'AUDIO', False),
                        ((0.332, 0.600, 0.668, 0.667), 'FLVIBED', False), ((0.332, 0.799, 0.668, 0.867), 'RETURN TO GAME', False)]:
        s.tab(r, cap, cur)
    s.focus((0.740, 0.320, 1.440, 0.348))
    s.pills([('D', 'ROW'), ('LB', 'TAB'), ('A', 'CHANGE'), ('B', 'BACK')], lane='bottom')
    s.done()


def main(argv):
    names = argv or list(RECIPES)
    for n in names:
        RECIPES[n]()
        print(f'{n}: done')


if __name__ == '__main__':
    main(sys.argv[1:])
