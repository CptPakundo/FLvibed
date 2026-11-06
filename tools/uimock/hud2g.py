"""hud2g.py - HUD 2's overhaul mocked on REAL game frames (the P11f gate, docs\\MODULES_PLAN.md; modules\\hud\\docs\\UI_HUD2_SKIN.md 9, 12).

    research\\tools\\ocr\\.venv\\Scripts\\python.exe -I tools\\uimock\\hud2g.py [recipe ...]

Reads research\\evidence\\hud2_gate\\frames\\<WxH>\\ (the game's own frames, probes\\capture.ps1), writes research\\evidence\\hud2_gate\\mock\\. What stands in a
frame as the game drew it: the scene, the map (its real grown box with the labels following, from the probes), the flight HUD of today. What is drawn here: only
what the design adds or changes. Each recipe says which is which in `NOTES`, which the contact sheet prints.
"""
import os
import sys

from PIL import Image, ImageDraw, ImageFilter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hud2s as hs  # noqa: E402
import hudmock as hm  # noqa: E402

GATE = os.path.join(hm.ROOT, 'research', 'evidence', 'hud2_gate')
OUT = os.path.join(GATE, 'mock')
hs.OUT = OUT
hs.TMP = os.path.join(OUT, '_base')
K4, DECK = (3840, 2160), (1280, 800)
INK, CHROME, DIM, ACTIVE, AMBER, HOSTILE, FRIEND = hs.INK, hs.CHROME, hs.DIM, hs.ACTIVE, hs.AMBER, hs.HOSTILE, hs.FRIEND
RECIPES, NOTES = {}, {}


# ---- true to the build (the critique, section 5): chrome glyphs of the frames' own family, the focus as uiskin draws it
def chrome_glyph(self, kind, cx, cy, size, *a, **k):
    """As built: a chrome ring, no coloured faces, PlayStation shapes like every real lane in these frames (the pad here is a DualSense)."""
    r = size / 2
    lw = max(1.6, size * 0.075)

    def f(d, P, q):
        col, W = CHROME + (255,), max(1, int(lw * q))
        if kind in ('LB', 'RB'):
            x, y = r * 1.5, r * 0.82
            c = y * 0.5
            pts = [(cx - x + c, cy - y), (cx + x - c, cy - y), (cx + x, cy - y + c), (cx + x, cy + y - c), (cx + x - c, cy + y), (cx - x + c, cy + y), (cx - x, cy + y - c), (cx - x, cy - y + c)]
            d.line([P(*t) for t in pts + pts[:1]], fill=col, width=W)
        elif kind == 'D':
            a_, b_ = r * 0.36, r
            pts = [(-a_, -b_), (a_, -b_), (a_, -a_), (b_, -a_), (b_, a_), (a_, a_), (a_, b_), (-a_, b_), (-a_, a_), (-b_, a_), (-b_, -a_), (-a_, -a_)]
            pts = [(cx + x, cy + y) for x, y in pts]
            d.line([P(*t) for t in pts + pts[:1]], fill=col, width=W)
        else:
            d.ellipse([P(cx - r, cy - r), P(cx + r, cy + r)], outline=col, width=W)
            s_ = r * 0.46
            if kind == 'A':
                d.line([P(cx - s_, cy - s_), P(cx + s_, cy + s_)], fill=col, width=W)
                d.line([P(cx - s_, cy + s_), P(cx + s_, cy - s_)], fill=col, width=W)
            elif kind == 'B':
                d.ellipse([P(cx - s_, cy - s_), P(cx + s_, cy + s_)], outline=col, width=W)
            elif kind == 'X':
                d.rectangle([P(cx - s_, cy - s_), P(cx + s_, cy + s_)], outline=col, width=W)
            else:
                pts = [(cx, cy - s_ * 1.1), (cx + s_, cy + s_ * 0.8), (cx - s_, cy + s_ * 0.8)]
                d.line([P(*t) for t in pts + pts[:1]], fill=col, width=W)
    self.shape(f, box=(cx - size, cy - size, cx + size, cy + size))
    if kind in ('LB', 'RB'):
        self.text((cx, cy), 'L1' if kind == 'LB' else 'R1', size * 0.52, fill=CHROME, anchor='mm', shadow=False)


FOCUS_WORD = [CHROME]   # as built: the game's blue on the mustard (3.7:1); the critique's R9 sets INK


def focus_true(self, r):
    """uiskin FocusFill: active yellow at 32 %, 0.85 edges top and bottom, a 0.005 H bar outside the left edge."""
    x0, y0, x1, y1 = self.px(r)
    g, bw, e = self.h(0.008), self.h(0.005), max(1, self.h(0.002))

    def f(d, P, s):
        d.rectangle([P(x0, y0), P(x1, y1)], fill=ACTIVE + (82,))
        d.rectangle([P(x0, y0), P(x1, y0 + e)], fill=ACTIVE + (217,))
        d.rectangle([P(x0, y1 - e), P(x1, y1)], fill=ACTIVE + (217,))
        d.rectangle([P(x0 - g - bw, y0), P(x0 - g, y1)], fill=ACTIVE + (255,))
    self.shape(f, box=(x0 - g - bw - 2, y0 - 2, x1 + 2, y1 + 2))


def chip_true(self, r, s, current=False, focus=False):
    x0, y0, x1, y1 = self.px(r)
    self.plate((x0, y0, x1, y1), chamfer=self.h(0.008), alpha=225, rim_alpha=120 if not current else 200, corners=(1, 0, 1, 0))
    if focus:
        self.focus(r)
    if current:
        self.line([(x0 + self.h(0.010), y1 - self.h(0.004)), (x1 - self.h(0.010), y1 - self.h(0.004))], col=ACTIVE, w=self.h(0.004))
    size = 0.030 * self.ts
    while self.textlen(s, self.h(size)) > (x1 - x0) * 0.92 and size > 0.024:
        size -= 0.001
    self.text(((x0 + x1) / 2, (y0 + y1) / 2), s, self.h(size), fill=FOCUS_WORD[0] if focus else ACTIVE if current else INK, anchor='mm')


hs.Skin.glyph = chrome_glyph
hs.Skin.focus = focus_true
hs.Skin.chip = chip_true


def recipe(fn):
    RECIPES[fn.__name__] = fn
    return fn


def tag(size):
    return f'{size[0]}x{size[1]}'


def frame(size, name):
    return os.path.join(GATE, 'frames', tag(size), name + '.png')


def mapsq(size, name):
    """The real map's square from a frame of the probes' tier: box 0.757, read back in game as (1083, 124) 1696 px at 4K: not the screen's centre."""
    w, h = size
    side = round(h * 1696 / 2160)
    x, y = round(w / 2 + h * (1083 + 848 - 1920) / 2160 - side / 2), round(h * 124 / 2160)
    return Image.open(frame(size, name)).convert('RGB').crop((x, y, x + side, y + side))


def no_box(size, name):
    """A frame with the native NeuroNet box painted out (a wide blur of its own surroundings): the strip replaces it."""
    im = Image.open(frame(size, name)).convert('RGBA')
    w, h = size
    b = (int(w - h * 0.30), 0, w, int(h * 0.075))
    soft = im.filter(ImageFilter.GaussianBlur(h * 0.05))
    im.paste(soft.crop(b), b[:2])
    os.makedirs(hs.TMP, exist_ok=True)
    p = os.path.join(hs.TMP, f'nobox_{name}_{tag(size)}.png')
    im.save(p)
    return p


def use_frame(size, name, box=True):
    """hud2s's recipes take their scene through `base`: here it is the game's frame, whatever the recipe asked for."""
    p = frame(size, name) if box else no_box(size, name)
    hs.base = lambda screen, sz, nm: p


@recipe
def navmaps():
    hs.LABELS[:] = []            # the game's own labels are in the frame
    for size, deck in ((K4, False), (DECK, True)):
        use_frame(size, 'map_real_bare', box=False)
        hs.clean_map = lambda size=size: mapsq(size, 'map_real_bare')
        for pad in (True, False):
            n = f'navmap_{tag(size)}_{"pad" if pad else "kbm"}'
            hs.navmap(size, n, pad=pad, deck=deck, ends=hs.navmap(size, n, pad=pad, deck=deck))
            NOTES[n] = 'REAL: the map, its grid lines, objects and labels at the grown size (poked in game). DRAWN: scrim, panels, chips, strip, grid letters, selection.'


@recipe
def universe():
    use_frame(K4, 'map_real_universe_bare', box=False)
    hs.universe_square = lambda: mapsq(K4, 'map_real_universe_bare')
    hs.universe_4k()
    NOTES['navmap_universe_4k_pad'] = 'REAL: the universe art at the grown size. DRAWN: everything around it.'


def strip_on(size, deck, win, word, pad):
    """The strip on a real NeuroNet window; the native box's icons go (the scene's own pixels from the same camera)."""
    src = f'nn_{win}__{"pad" if pad else "kbm"}'
    s = hs.Skin(no_box(size, src), deck)
    s.tabs(word, pad)
    n = f'strip_{win}_{tag(size)}_{"pad" if pad else "kbm"}'
    s.finish(n)
    NOTES[n] = 'REAL: the window as built today. DRAWN: the strip; the box of icons painted out with the scene.'


@recipe
def strips():
    for win, word in (('inventory', 'INVENTORY'), ('status', 'STATUS'), ('log', 'LOG'), ('help', 'HELP')):
        for size, deck in ((K4, False), (DECK, True)):
            strip_on(size, deck, win, word, True)
        strip_on(K4, False, win, word, False)


@recipe
def dialogs():
    for size, deck in ((K4, False), (DECK, True)):
        use_frame(size, 'pause__pad')
        n = f'quit_{tag(size)}_pad'
        hs.confirm(size, n, 'QUIT TO THE MAIN MENU?', 'Progress since your last save will be lost.', 'KEEP PLAYING', 'QUIT ANYWAY', deck=deck, scene='x')
        NOTES[n] = 'REAL: the pause menu behind the scrim. DRAWN: the dialog.'
        n = f'quit_{tag(size)}_kbm'
        hs.confirm(size, n, 'QUIT TO THE MAIN MENU?', 'Progress since your last save will be lost.', 'KEEP PLAYING', 'QUIT ANYWAY', deck=deck, scene='x', pad=False)
        NOTES[n] = 'K&M: no lane. REAL: the pause menu behind. DRAWN: the dialog.'
        FOCUS_WORD[0] = INK
        n = f'quit_{tag(size)}_r9'
        hs.confirm(size, n, 'QUIT TO THE MAIN MENU?', 'Progress since your last save will be lost.', 'KEEP PLAYING', 'QUIT ANYWAY', deck=deck, scene='x')
        FOCUS_WORD[0] = CHROME
        NOTES[n] = 'The critique\'s R9: the block kept, the word in INK.'
        n = f'abort_{tag(size)}_pad'
        hs.confirm(size, n, 'ABORT THE MISSION?', "The mission ends here and can't be picked up again.", 'KEEP GOING', 'ABORT', deck=deck, scene='x')
        NOTES[n] = 'REAL: the scene behind. DRAWN: the dialog. The cost line is a stand-in for the game\'s own text.'


def front(n, size):
    return os.path.join(GATE, 'frames', 'front', f'{n}_{tag(size)}.png')


@recipe
def fronts():
    """The front end on the game's own frames: the scene and logo real, the column and dialog drawn over the buttons as built."""
    for size, deck in ((K4, False), (DECK, True)):
        hs.base = lambda screen, sz, nm, size=size: front('mainmenu', size)
        n = f'mainmenu_{tag(size)}_pad'
        # the game's five buttons, measured from the frame: chips stand exactly on them, as the design's art tier would
        w, h = size
        im = Image.open(front('mainmenu', size)).convert('RGBA')
        soft = im.filter(ImageFilter.GaussianBlur(h * 0.02))
        b = (int(w * 0.80), int(h * 0.945), w, h)
        im.paste(soft.crop(b), b[:2])
        os.makedirs(hs.TMP, exist_ok=True)
        p = os.path.join(hs.TMP, f'menu_{tag(size)}.png')
        im.save(p)
        s = hs.Skin(p, deck)
        x0, x1 = (0.044 * s.WH, 0.2275 * s.WH) if not deck else (0.048 * s.WH, 0.250 * s.WH)
        step, bh = 0.1072, 0.056 if not deck else 0.062
        y0 = 0.295 - bh / 2
        s.glass((x0 - 0.016, y0 - 0.022, x1 + 0.016, y0 + 4 * step + bh + 0.022), content=False, alpha=0.70)
        for i, c in enumerate(['NEW GAME', 'LOAD GAME', 'MULTIPLAYER', 'OPTIONS', 'EXIT']):
            y = y0 + i * step
            s.chip((x0, y, x1, y + bh), c, focus=i == 1)
        s.small(s.WH - s.mx, 0.965 if not deck else 0.975, 'FREELANCER 1.1  ·  HD EDITION 0.7.1  ·  FLVIBED 0.4.0', DIM, 0.024, 'rm')
        s.finish(n)
        NOTES[n] = 'REAL: the scene, the logo. DRAWN: chips on the game\'s own five button places, the plate, the version line (the native one painted out).'
        n = f'exit_{tag(size)}_pad'
        hs.confirm(size, n, 'EXIT FREELANCER?', 'Back to the desktop.', 'STAY', 'EXIT GAME', deck=deck, scene='x', focus_safe=False, amber=False)
        NOTES[n] = 'REAL: the menu behind the scrim. DRAWN: the dialog.'
    hs.loading_4k()
    NOTES['loading_4k'] = 'DRAWN on navy. The native screen is in the sheet beside it. Loads take 1.1-1.2 s: the bar shows for about 0.65 s after the 500 ms delay.'


def alert_card(s, side, y, title, answer, col):
    """A danger beside the ring, as 0.3.0's stack (UI_HUD2.md 5.40): title, its answer."""
    k = 1.08 if s.deck else 1.0
    w, h = s.h(0.25 * k), s.h(0.095 * k)
    cx, cy = s.W / 2, s.H * y
    x1 = cx - s.h(0.205 * k) if side < 0 else cx + s.h(0.205 * k) + w
    x0 = x1 - w
    s.plate((x0, cy - h / 2, x1, cy + h / 2), chamfer=s.h(0.012), alpha=200, rim=col, rim_alpha=230, corners=(1, 1, 1, 1))
    s.text((x0 + s.h(0.020), cy - s.h(0.022 * k)), title, s.h(0.032 * k), fill=col)
    s.text((x0 + s.h(0.020), cy + s.h(0.024 * k)), answer, s.h(0.026 * k), fill=INK)


@recipe
def alerts():
    """Issue 27: the game's flashing icon goes; the warning takes HUD 2's own place, the danger stack beside the ring."""
    for size, deck in ((K4, False), (DECK, True)):
        # the game toggles the icon every 0.7 s: a frame of its off phase is what the deny rule would leave, with nothing painted
        w, h = size
        box = (int(w / 2 + h * 0.40), int(h * 0.04), int(w / 2 + h * 0.56), int(h * 0.19))

        def yellow(n):
            a = Image.open(frame(size, n)).convert('RGB').crop(box).getdata()
            return sum(1 for r, g, b in a if r > 200 and g > 180 and b < 90)
        best = min(('radiation__kbm', 'radiation__pad', 'radiation_full__kbm', 'radiation_full__pad'), key=yellow)
        print(tag(size), best, yellow(best))
        s = hs.Skin(frame(size, best), deck)
        alert_card(s, -1, 0.44, 'RADIATION', 'LEAVE THE ZONE', AMBER)
        alert_card(s, 1, 0.36, 'ON YOUR TAIL', '320 M BEHIND', HOSTILE)
        n = f'alerts_{tag(size)}'
        s.finish(n)
        NOTES[n] = 'REAL: the scene and today\'s HUD, a frame of the icon\'s off phase. DRAWN: RADIATION and ON YOUR TAIL in the danger stack (a proposal: the design had none).'


CUES = [('none lit', [('1', True, False, True), ('2', False, False, True), ('3', False, False, True), ('M', False, False, True), ('T', False, False, True)]),
        ('selected in reach', [('1', True, True, True), ('2', False, False, True), ('3', False, False, True), ('M', False, False, True), ('T', False, False, True)]),
        ('others in reach', [('1', True, False, True), ('2', False, True, True), ('3', False, True, True), ('M', False, True, True), ('T', False, False, True)]),
        ('all in reach', [('1', True, True, True), ('2', False, True, True), ('3', False, True, True), ('M', False, True, True), ('T', False, True, True)]),
        ('no torpedoes left', [('1', True, True, True), ('2', False, True, True), ('M', False, True, True), ('T', False, False, False)]),
        ('one group, guns only', [('1', True, True, True)])]


@recipe
def cues():
    for size, deck, k in ((K4, False, 1.0), (DECK, True, 1.08)):
        crops = []
        for i, (word, cells) in enumerate(CUES):
            n = f'cue_{tag(size)}_{i}'
            hs.cue(frame(size, 'fight__kbm'), n, k, cells, deck)
            NOTES[n] = f'REAL: the fight and today\'s ring. DRAWN: the cue, state: {word}.'
            w, h = size
            c = int(h * 0.20)
            crops.append((word, Image.open(os.path.join(OUT, n + '.png')).convert('RGB').crop((w // 2 - c, h // 2 - c, w // 2 + c, h // 2 + c))))
        cell = crops[0][1].width
        sheet = Image.new('RGB', (cell * 3 + 40, (cell + 60) * 2), (10, 12, 26))
        d = ImageDraw.Draw(sheet)
        for i, (word, im) in enumerate(crops):
            x, y = 10 + (i % 3) * (cell + 10), (i // 3) * (cell + 60)
            sheet.paste(im, (x, y + 50))
            d.text((x + 6, y + 12), word, fill=(200, 225, 245), font=hm.font(30) if hasattr(hm, 'font') else None)
        n = f'cue_states_{tag(size)}'
        sheet.save(os.path.join(OUT, n + '.png'))
        NOTES[n] = 'Every state at true pixels, cropped round the ring: the ring and scene real, the cue drawn. Motion and the 170 ms hold are not shown.'


def power_base(size, src, scene, k):
    """A real frame with the game-drawn power arc taken out: the scene's own pixels through a mask of the arc alone."""
    im = Image.open(frame(size, src)).convert('RGBA')
    sc = Image.open(frame(size, scene)).convert('RGBA')
    w, h = size
    R = 0.10 * k * h
    aw = 0.0065 * k * h * 0.8
    mask = Image.new('L', size, 0)
    d = ImageDraw.Draw(mask)
    cx, cy = w / 2, h / 2
    d.arc([cx - R - aw, cy - R - aw, cx + R + aw, cy + R + aw], 64, 116, fill=255, width=max(3, int(aw * 2.4)))
    im.paste(sc, (0, 0), mask)
    os.makedirs(hs.TMP, exist_ok=True)
    p = os.path.join(hs.TMP, f'power_{src}_{tag(size)}.png')
    im.save(p)
    return p


def power_arc(s, k, frac, form='ends', half=22.0, n=8):
    """Issue 28: the power arc centred, draining from both ends to the middle. `ends`: every segment stands, the lit edge trimmed continuously. `pairs`: whole
    segments, a pair at a time. Angles as the game's: 90 = the bottom."""
    w, h = s.W, s.H
    cx, cy = w / 2, h / 2
    R = 0.10 * k * h
    aw = 0.0065 * k * h * 0.8
    span = 2 * half / n
    gap = 2.2
    lit_half = half * frac
    if form == 'pairs':
        lit_half = span * (-(-frac * n // 2)) if frac > 0 else 0

    def seg(d, P, q, b0, b1, col):
        if b1 <= b0:
            return
        c0, c1 = P(cx - R - aw / 2, cy - R - aw / 2), P(cx + R + aw / 2, cy + R + aw / 2)
        d.arc([c0, c1], b0, b1, fill=col, width=max(1, int(aw * q)))

    def f(d, P, q):
        for i in range(n):
            b0 = 90 - half + i * span + gap / 2
            b1 = 90 - half + (i + 1) * span - gap / 2
            seg(d, P, q, b0, b1, (0, 0, 0, 170))
            seg(d, P, q, max(b0, 90 - lit_half), min(b1, 90 + lit_half), hs.h2.POWER + (255,))
    s.shape(f, box=(cx - R, cy, cx + R, cy + R + 2 * aw))


METERS = [('full', 1.0), ('half', 0.5), ('low', 0.2), ('empty', 0.0)]


@recipe
def meters():
    """Issue 28 on real frames: the ring, speed, scene and cue's place real; the power arc drawn. Every size x full, half, low, empty; then the forms at one size."""
    rows = {K4: [('k 0.88', 'planet_full__size_normal_pad', 0.88), ('k 1.0', 'planet_full__pad', 1.0), ('k 1.18', 'planet_full__size_large_pad', 1.1833)],
            DECK: [('k 0.79', 'planet_full__size_normal_pad', 0.792), ('k 1.08', 'planet_full__pad', 1.08)]}
    f = hm.font(30)
    for size, deck in ((K4, False), (DECK, True)):
        h = size[1]
        c = int(h * 0.17)
        tile = 300
        sheet = Image.new('RGB', (tile * 4 + 50, (tile + 46) * len(rows[size]) + 10), (9, 11, 24))
        d = ImageDraw.Draw(sheet)
        for r, (word, src, k) in enumerate(rows[size]):
            base = power_base(size, src, 'planet_full__scene', k)
            for i, (lvl, fr) in enumerate(METERS):
                n = f'meter_{tag(size)}_{r}_{lvl}'
                s = hs.Skin(base, deck)
                power_arc(s, k, fr)
                s.finish(n)
                im = Image.open(os.path.join(OUT, n + '.png')).convert('RGB')
                w = size[0]
                cr = im.crop((w // 2 - c, h // 2 - c // 3, w // 2 + c, h // 2 + 2 * c - c // 3)).resize((tile, tile), Image.LANCZOS)
                x, y = 10 + i * (tile + 10), r * (tile + 46)
                sheet.paste(cr, (x, y + 42))
                d.text((x + 4, y + 8), f'{word}  {lvl}', fill=(200, 225, 245), font=f)
        n = f'meter_sizes_{tag(size)}'
        sheet.save(os.path.join(OUT, n + '.png'))
        NOTES[n] = 'REAL: scene, ring, speed. DRAWN: the power arc, 44 degrees as today, 8 segments, draining from both ends to the middle, its lit edge continuous.'
    # the forms at k 1, half power, with the cue above: how small it can stay
    forms = [('44 deg, 8 seg, continuous ends', dict(form='ends')), ('44 deg, 8 seg, a pair at a time', dict(form='pairs')),
             ('56 deg, 10 seg, continuous', dict(form='ends', half=28.0, n=10)), ('70 deg, 12 seg, continuous', dict(form='ends', half=35.0, n=12))]
    tile = 620
    size = K4
    h, w = size[1], size[0]
    c = int(h * 0.17)
    sheet = Image.new('RGB', (tile * 2 + 30, (tile + 46) * 2 + 10), (9, 11, 24))
    d = ImageDraw.Draw(sheet)
    base = power_base(size, 'fight__kbm', 'calm__scene', 1.0)
    for i, (word, kw) in enumerate(forms):
        n = f'meter_form_{i}'
        hs.cue(base, n, 1.0, [('1', True, True, True), ('2', False, False, True), ('M', False, True, True), ('T', False, False, True)], False)
        s = hs.Skin(os.path.join(OUT, n + '.png'), False)
        power_arc(s, 1.0, 0.45, **kw)
        s.finish(n)
        im = Image.open(os.path.join(OUT, n + '.png')).convert('RGB')
        cr = im.crop((w // 2 - c, h // 2 - c, w // 2 + c, h // 2 + c)).resize((tile, tile), Image.LANCZOS)
        x, y = 10 + (i % 2) * (tile + 10), (i // 2) * (tile + 46)
        sheet.paste(cr, (x, y + 42))
        d.text((x + 4, y + 8), word, fill=(200, 225, 245), font=f)
    sheet.save(os.path.join(OUT, 'meter_forms.png'))
    NOTES['meter_forms'] = 'At k 1, 45 % power, the weapon cue above: REAL fight and ring, DRAWN cue and arc. Motion not shown.'


def layout_of(size):
    """today's rectangles (hud2 state's layout line), for the size audit's proposal"""
    if size == K4:
        return dict(card=(2999, 64, 3724, 540), ship=(115, 1771, 773, 2095), cons=(3254, 1469, 3724, 1750))
    return None


def lean_real(scene, size, deck):
    """UI_HUD2_SIZE_AUDIT.md on a real frame: the old card and plates replaced by the scene's own pixels (hud2 sim combat on a still ship), the new forms drawn."""
    src = frame(size, f'{scene}__kbm')
    im = Image.open(src).convert('RGBA')
    sc = Image.open(frame(size, f'{scene}__scene')).convert('RGBA')
    w, h = size
    t = 1.08 if deck else 1.0
    card = (int(w * 0.70), 0, w, int(h * 0.26))
    im.paste(sc.crop(card), card[:2])
    os.makedirs(hs.TMP, exist_ok=True)
    p = os.path.join(hs.TMP, f'lean_{scene}_{tag(size)}.png')
    im.save(p)
    s = hs.Skin(p, deck)
    m = s.h(0.03) if not deck else s.h(0.015)
    cw, ch = s.h(0.36 * t), s.h(0.17 * t)
    x1 = w - (m * 1.0 if deck else w * 0.03)
    x0, y0 = x1 - cw, s.h(0.03) if not deck else s.h(0.015)
    y1 = y0 + ch
    s.plate((x0, y0, x1, y1), chamfer=s.h(0.012), alpha=175, rim_alpha=110, corners=(0, 1, 0, 1), accent=('l', HOSTILE))
    # the real emblem or wireframe, from the frame's own card: the 0.13 column stays (the owner's answer 7.3)
    old = Image.open(src).convert('RGBA')
    ox0 = x1 - s.h(0.44 * t)
    col = old.crop((int(ox0 + s.h(0.006)), int(y0 + s.h(0.012)), int(ox0 + s.h(0.125 * t)), int(y0 + s.h(0.158 * t))))
    s.paste(col, (int(x0 + s.h(0.006)), int(y0 + s.h(0.012))))
    tx, r, z = x0 + s.h(0.130 * t), x1 - s.h(0.018), 0.026 * t
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
    n = f'lean_{scene}_{tag(size)}'
    s.finish(n)
    NOTES[n] = 'REAL: the scene, ring, loadout, prompts, escorts, the card\'s emblem or wireframe. DRAWN: the target card at the audit\'s 0.36 x 0.17. The threat rows under it are not redrawn.'


@recipe
def lean():
    for sc in ('planet_full', 'nebula_full', 'sun_full'):
        lean_real(sc, K4, False)
        lean_real(sc, DECK, True)


PAGE_W = 1280   # the contact sheet's width: a phone shows one frame across, its text readable when zoomed once


def g(size, name):
    return frame(size, name)


def m(name):
    return os.path.join(OUT, name + '.png')


def sections():
    a, b = K4, DECK
    s = [('1 Flight sizes beside classic (ALL REAL game frames, sizes run in game by a probe build)', [
            ([(g(a, 'fight__classic'), '4K classic: its list text = k 0.86'), (g(a, 'fight__size_normal_pad'), '4K k 0.88: text as classic. boxed 13.5 %')], ''),
            ([(g(a, 'fight__pad'), '4K k 1.0: today. boxed 17.8 %'), (g(a, 'fight__size_large_pad'), '4K k 1.18 + Deck margins. boxed 24.1 %')], ''),
            ([(g(a, 'fight__size_small_pad'), '4K k 0.75: smaller than classic. boxed 9.9 %'), (g(a, 'fight__size_normal_kbm'), '4K k 0.88, K&M. boxed 14.1 %')], ''),
            ([(g(b, 'fight__classic'), '1280x800 classic'), (g(b, 'fight__size_normal_pad'), '1280x800 k 0.79. boxed 12 %')], ''),
            ([(g(b, 'fight__size_k100_pad'), '1280x800 k 0.90, no Deck mode. boxed 15.7 %'), (g(b, 'fight__size_large_pad'), '1280x800 k 1.08 = today\'s Deck mode. boxed 22 %')], ''),
            ([(g(a, 'sim_travel__size_normal_pad'), 'travel k 0.88'), (g(a, 'sim_travel__size_large_pad'), 'travel k 1.18')], '')]),
         ('2 Legibility over the worst backgrounds (REAL game frames, HUD 2 as built)', [
            ([(g(a, 'planet_full__kbm'), 'bright planet 4K'), (g(a, 'planet_full__classic'), 'classic')], ''),
            ([(g(a, 'sun_full__pad'), 'sun 4K'), (g(a, 'nebula_full__kbm'), 'nebula 4K')], ''),
            ([(g(b, 'planet_full__pad'), 'planet 1280x800'), (g(b, 'sun_full__pad'), 'sun 1280x800')], ''),
            ([(g(a, 'planet_full__size_small_pad'), 'planet SMALL'), (g(b, 'nebula_full__pad'), 'nebula 1280x800')], '')]),
         ('3 The size audit on real frames: the card at 0.36 (card drawn, all else real)', [
            ([(m('lean_planet_full_3840x2160'), '4K planet'), (m('lean_sun_full_3840x2160'), '4K sun')], ''),
            ([(m('lean_planet_full_1280x800'), '1280x800 planet'), (m('lean_nebula_full_1280x800'), '1280x800 nebula')], '')]),
         ('4 Travel, lane, formation, mission (REAL frames; sim = HUD 2 state forced over the real scene)', [
            ([(g(a, 'cruise_real__kbm'), 'cruise, real'), (g(a, 'sim_lane__pad'), 'lane (sim)')], ''),
            ([(g(a, 'sim_formation__pad'), 'formation (sim)'), (g(a, 'mission__pad'), 'a mission moment with the objective banner')], ''),
            ([(g(b, 'mission__pad'), 'mission 1280x800'), (g(a, 'sim_comm__kbm'), 'comm (sim)')], '')]),
         ('5 Nav map: the REAL grown map (poked in game) inside the design', [
            ([(m('navmap_3840x2160_pad'), 'system, 4K, pad')], ''),
            ([(m('navmap_3840x2160_kbm'), 'system, 4K, K&M')], ''),
            ([(m('navmap_1280x800_pad'), 'system, 1280x800, pad')], ''),
            ([(m('navmap_universe_4k_pad'), 'universe, 4K, pad')], ''),
            ([(g(a, 'nn_navmap__kbm'), 'today, K&M'), (g(a, 'nn_navmap__classic'), 'classic')], '')]),
         ('6 Every NeuroNet window with the strip (window real, strip drawn)', [
            ([(m('strip_inventory_3840x2160_pad'), 'inventory 4K pad'), (m('strip_status_3840x2160_pad'), 'status 4K pad')], ''),
            ([(m('strip_log_3840x2160_kbm'), 'log 4K K&M'), (m('strip_help_3840x2160_pad'), 'help 4K pad')], ''),
            ([(m('strip_inventory_1280x800_pad'), 'inventory 1280x800'), (m('strip_status_1280x800_pad'), 'status 1280x800')], ''),
            ([(m('strip_log_1280x800_pad'), 'log 1280x800'), (m('strip_help_1280x800_pad'), 'help 1280x800')], '')]),
         ('7 Quit, confirm, pause', [
            ([(g(a, 'quit_confirm_real_pad'), 'today (real)'), (m('quit_3840x2160_pad'), 'design over the real pause')], ''),
            ([(g(a, 'quit_confirm_real_classic'), 'classic (real)'), (m('quit_1280x800_pad'), 'design 1280x800')], ''),
            ([(m('quit_3840x2160_kbm'), 'design, K&M'), (m('quit_3840x2160_r9'), 'R9: the word in ink on the same block')], ''),
            ([(g(a, 'pause__pad'), 'pause, as built (real)'), (g(b, 'pause__pad'), 'pause 1280x800 (real)')], '')]),
         ('8 Docked screens, as built and classic (REAL frames; docked size presets beside)', [
            ([(g(a, 'equipment__pad'), 'equipment dealer 4K pad'), (g(a, 'equipment__classic'), 'classic')], ''),
            ([(g(b, 'equipment__pad'), 'equipment dealer 1280x800'), (g(b, 'equipment__size_normal_pad'), 'NORMAL at 1280x800')], ''),
            ([(g(a, 'commodity__kbm'), 'commodity 4K K&M'), (g(b, 'commodity__pad'), 'commodity 1280x800')], ''),
            ([(g(a, 'shipdealer__pad'), 'ship dealer 4K'), (g(a, 'jobboard__pad'), 'job board 4K')], ''),
            ([(g(a, 'docked_nn_navmap__pad'), 'nav map docked'), (g(a, 'docked_nn_inventory__pad'), 'inventory docked')], '')]),
         ('9 Radiation: issue 27 today (REAL), and a HUD 2 home (drawn)', [
            ([(g(a, 'radiation__kbm'), 'today in flight: the icon on the card\'s corner'), (g(a, 'radiation_pause__pad'), 'today under pause')], ''),
            ([(m('alerts_3840x2160'), 'proposal 4K'), (m('alerts_1280x800'), 'proposal 1280x800')], '')]),
         ('10 The weapon cue in each state (ring and fight real, cue drawn; true pixels)', [
            ([(m('cue_states_3840x2160'), '4K')], ''),
            ([(m('cue_states_1280x800'), '1280x800')], '')]),
         ('10b Issue 28: the power arc draining to the middle (arc drawn, the rest real)', [
            ([(m('meter_sizes_3840x2160'), '4K: k 0.88, 1.0, 1.18; full, half, low, empty')], ''),
            ([(m('meter_sizes_1280x800'), '1280x800: k 0.79, 1.08')], ''),
            ([(m('meter_forms'), 'how small it can stay, with the cue above: 4K, k 1, 45 %')], '')]),
         ('11 Front end and load (REAL frames from the probes)', [
            ([(front('mainmenu', a), 'main menu 4K, as built'), (m('mainmenu_3840x2160_pad'), 'design: column drawn over it')], ''),
            ([(front('mainmenu', b), 'main menu 1280x800, as built'), (m('mainmenu_1280x800_pad'), 'design')], ''),
            ([(front('exit_dialog', a), 'exit dialog, as built'), (m('exit_3840x2160_pad'), 'design')], ''),
            ([(front('load_game', a), 'load game 4K, as built'), (front('load_game', b), 'load game 1280x800, as built')], ''),
            ([(front('loading', a), 'loading, native (centre region of a grab)'), (m('loading_4k'), 'design, drawn')], ''),
            ([(front('options_general', a), 'options, as built'), (front('mp_servers', a), 'server list, as built, a real LAN server')], ''),
            ([(front('mp_prompt', a), 'MP prompt, as built'), (front('mission_popup', a), 'mission popup, as built')], '')]),
         ('13 A real bracket at the ring, a comm portrait (REAL frames)', [
            ([(os.path.join(GATE, 'frames2', tag(a), 'bracket_900_pad.png'), 'bracket on the ring, comm portrait, 4K pad'), (os.path.join(GATE, 'frames2', tag(a), 'bracket_450_kbm.png'), '4K K&M, target just off centre')], ''),
            ([(os.path.join(GATE, 'frames2', tag(b), 'bracket_900_pad.png'), '1280x800 pad'), (os.path.join(GATE, 'frames2', tag(b), 'bracket_450_kbm.png'), '1280x800 K&M')], ''),
            ([(os.path.join(GATE, 'frames2', tag(a), 'comm_1_map_pad.png'), 'a comm under the map, as built'), (os.path.join(GATE, 'frames2', tag(a), 'bracket_900_classic.png'), 'classic, same moment')], '')]),
         ('12 The tester\'s flash, caught: the first frame of space (REAL grabs, as captured)', [
            ([(os.path.join(GATE, 'probes', 'frames', 'grab', 'ingameload_c702_flash.png'), 'in-game load into space')], ''),
            ([(os.path.join(GATE, 'probes', 'frames', 'grab', 'menuload_c702_1_flash.png'), 'load from the main menu')], '')])]
    return s


def page(i, title, rows):
    f, fs = hm.font(34), hm.font(26)
    pad = 16
    out = []
    y = 70
    for items, _ in rows:
        n = len(items)
        w = (PAGE_W - pad * (n + 1)) // n
        ims = []
        for p, cap in items:
            if not os.path.exists(p):
                print('  no frame', p)
                ims.append((Image.new('RGB', (w, w * 9 // 16), (30, 30, 34)), cap + ' (no frame)'))
                continue
            im = Image.open(p).convert('RGB')
            ims.append((im.resize((w, int(im.height * w / im.width)), Image.LANCZOS), cap))
        hh = max(im.height for im, _ in ims) + 40
        out.append((y, ims, w))
        y += hh + 8
    sheet = Image.new('RGB', (PAGE_W, y + 10), (9, 11, 24))
    d = ImageDraw.Draw(sheet)
    d.text((pad, 18), title[:88], fill=(255, 226, 120), font=f)
    for yy, ims, w in out:
        for j, (im, cap) in enumerate(ims):
            x = pad + j * (w + pad)
            d.text((x, yy), cap, fill=(200, 225, 245), font=fs)
            sheet.paste(im, (x, yy + 34))
    n = f'sheet_{i:02d}.jpg'
    sheet.save(os.path.join(GATE, n), quality=86)
    print(n, sheet.size)


@recipe
def sheets():
    for i, (t, rows) in enumerate(sections(), 1):
        page(i, f'{i} ' + t.split(' ', 1)[1], rows)


if __name__ == '__main__':
    os.makedirs(OUT, exist_ok=True)
    for n in (sys.argv[1:] or list(RECIPES)):
        try:
            RECIPES[n]()
            print('ok', n)
        except FileNotFoundError as e:
            print('MISSING', n, e)
    import json
    p = os.path.join(OUT, 'notes.json')
    old = json.load(open(p, encoding='utf-8')) if os.path.exists(p) else {}
    old.update(NOTES)
    json.dump(old, open(p, 'w', encoding='utf-8'), indent=1)
