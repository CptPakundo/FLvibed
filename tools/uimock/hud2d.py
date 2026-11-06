"""hud2d.py - one decision sheet per D-number of modules\\hud\\docs\\UI_HUD2_CRITIQUE.md 7: the options side by side, labelled, readable on a phone.

    research\\tools\\ocr\\.venv\\Scripts\\python.exe -I tools\\uimock\\hud2d.py [D1 D2 ...]

On real frames (research\\evidence\\hud2_gate\\frames, frames2); what is drawn is said on each sheet. Written to research\\evidence\\hud2_gate\\decisions\\.
"""
import os
import sys

from PIL import Image, ImageDraw, ImageFilter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hud2g as g  # noqa: E402
import hud2s as hs  # noqa: E402
import hudmock as hm  # noqa: E402

OUT = os.path.join(g.GATE, 'decisions')
TMP = os.path.join(OUT, '_work')
K4, DECK = g.K4, g.DECK
INK, CHROME, DIM, ACTIVE, AMBER, HOSTILE = g.INK, g.CHROME, g.DIM, g.ACTIVE, g.AMBER, g.HOSTILE
W = 1280
BG = (9, 11, 24)
PICK = "the critique's pick"


def f2(size, n):
    return os.path.join(g.GATE, 'frames2', g.tag(size), n + '.png')


def wrap(d, s, font, width):
    out, line = [], ''
    for w_ in s.split(' '):
        c = (line + ' ' + w_).strip()
        if d.textlength(c, font=font) > width and line:
            out.append(line)
            line = w_
        else:
            line = c
    out.append(line)
    return out


def fit(im, w):
    w = int(w)
    return im.resize((w, int(im.height * w / im.width)), Image.LANCZOS)


def sheet(name, title, question, options, foot=''):
    """options: (label, note, [images], pick). Images: one across, or two side by side."""
    ft, fq, fl, fn = hm.font(50), hm.font(36), hm.font(40), hm.font(30)
    probe = ImageDraw.Draw(Image.new('RGB', (10, 10)))
    pad = 20
    blocks, y = [], 20
    tl = wrap(probe, title, ft, W - 2 * pad)
    ql = wrap(probe, question, fq, W - 2 * pad)
    y += 58 * len(tl) + 44 * len(ql) + 18
    for label, note, ims, pick in options:
        nl = wrap(probe, note, fn, W - 2 * pad) if note else []
        y0 = y
        y += 52 + 36 * len(nl)
        n = len(ims)
        placed = []
        if n:
            w = (W - pad * (n + 1)) // n
            rs = [fit(i.convert('RGB'), w) for i in ims]
            for j, im in enumerate(rs):
                placed.append((im, pad + j * (w + pad), y))
            y += max(im.height for im in rs) + 8
        blocks.append((y0, label, nl, placed, pick))
        y += 22
    fl_ = wrap(probe, foot, fn, W - 2 * pad) if foot else []
    img = Image.new('RGB', (W, y + 36 * len(fl_) + 16), BG)
    d = ImageDraw.Draw(img)
    yy = 20
    for t in tl:
        d.text((pad, yy), t, fill=(255, 226, 120), font=ft)
        yy += 58
    for t in ql:
        d.text((pad, yy), t, fill=(226, 240, 252), font=fq)
        yy += 44
    for y0, label, nl, placed, pick in blocks:
        d.line([(pad, y0 - 6), (W - pad, y0 - 6)], fill=(80, 110, 140), width=2)
        d.text((pad, y0), label, fill=(252, 243, 68) if pick else (200, 225, 245), font=fl)
        if pick:
            d.text((W - pad, y0 + 8), PICK, fill=(252, 243, 68), font=fn, anchor='ra')
        ny = y0 + 50
        for t in nl:
            d.text((pad, ny), t, fill=(160, 195, 225), font=fn)
            ny += 36
        for im, x, yi in placed:
            img.paste(im, (x, yi))
    yy = y
    for t in fl_:
        d.text((pad, yy), t, fill=(130, 160, 190), font=fn)
        yy += 36
    os.makedirs(OUT, exist_ok=True)
    img.save(os.path.join(OUT, name + '.jpg'), quality=87)
    print(name, img.size)


def ring_crop(p, size, half=0.21, cy=0.5):
    w, h = size
    c = int(h * half)
    return Image.open(p).convert('RGB').crop((w // 2 - c, int(h * cy) - c, w // 2 + c, int(h * cy) + c))


def save(s, n):
    os.makedirs(TMP, exist_ok=True)
    p = os.path.join(TMP, n + '.png')
    s.save(p)
    return p


# ---- the cue, on a real bracket
def cue2(src, n, size, k, cells, seat, deck=False):
    s = hs.Skin(src, deck)
    cx, cy = s.W / 2, s.H / 2
    R = s.h(0.10 * k)
    cw, c, gp, sep = s.h(0.020), s.h(0.026), s.h(0.003), s.h(0.008)
    pos = []
    if seat == 'above':
        gaps = [(sep if cells[i][0].isdigit() and not cells[i + 1][0].isdigit() else gp) for i in range(len(cells) - 1)]
        x = cx - (len(cells) * cw + sum(gaps)) / 2
        y = cy - R * 0.93 - c / 2
        for i in range(len(cells)):
            pos.append((x, y))
            x += cw + (gaps[i] if i < len(cells) - 1 else 0)
    else:
        x = cx + R + s.h(0.034 * k)
        tot = len(cells) * c + (len(cells) - 1) * gp
        y = cy - tot / 2
        for i in range(len(cells)):
            pos.append((x, y))
            y += c + gp
    for (d_, sel, lit, ok), (x, y) in zip(cells, pos):
        col = AMBER if not ok else ACTIVE if sel else CHROME
        lit = lit and ok
        s.plate((x - 1, y - 1, x + cw + 1, y + c + 1), chamfer=cw * 0.24, alpha=215, rim_alpha=0, corners=(1, 0, 1, 0))
        if lit or sel:
            def f(dr, P, q, x=x, y=y, col=col, lit=lit):
                ch = cw * 0.24
                pts = [(x + ch, y), (x + cw, y), (x + cw, y + c - ch), (x + cw - ch, y + c), (x, y + c), (x, y + ch)]
                if lit:
                    dr.polygon([P(*t) for t in pts], fill=col + (255,))
                else:
                    dr.line([P(*t) for t in pts + [pts[0]]], fill=col + (255,), width=int(s.h(0.0016) * q))
            s.shape(f, box=(x - 4, y - 4, x + cw + 4, y + c + 4))
        s.text((x + cw / 2, y + c / 2), d_, s.h(0.024), fill=(6, 7, 22) if lit else col, anchor='mm', shadow=not lit)
    return save(s, n)


REACH = [('1', True, True, True), ('2', False, True, True), ('3', False, False, True), ('M', False, True, True), ('T', False, True, True)]
LOCK = [('1', True, True, True), ('2', False, True, True), ('3', False, False, True), ('M', False, False, True), ('T', False, False, True)]
DIMC = (0x5B, 0x8C, 0xAA)     # chrome at 70 %: 3.1:1 against yellow, so "selected" is not colour alone (d4_channels_research.md)
NAVYD = (6, 7, 22)


def cell(s, x, y, cw, c, t, label, sel=False, reach=False, ok=True, lock=None, glow=False):
    """One mark, by channel: COLOUR = selected (yellow against dim chrome). FILL = in reach, all or nothing. FORM = out (amber, a slash, never filled), locked (a tick above, or `lock='glow'`).
    The glow also stands for the one short pulse at any change."""
    ch = cw * 0.24
    pts = [(x + ch, y), (x + cw, y), (x + cw, y + c - ch), (x + cw - ch, y + c), (x, y + c), (x, y + ch)]
    col = AMBER if not ok else ACTIVE if sel else DIMC
    reach = reach and ok
    if lock == 'glow' or glow:
        lay = Image.new('RGBA', (int(s.W), int(s.H)), (0, 0, 0, 0))
        ImageDraw.Draw(lay).rectangle((x - cw * 0.12, y - cw * 0.12, x + cw * 1.12, y + c + cw * 0.12), fill=(ACTIVE if sel else INK) + (230,))
        s.paste(lay.filter(ImageFilter.GaussianBlur(cw * 0.32)))
    s.plate((x - 1, y - 1, x + cw + 1, y + c + 1), chamfer=ch, alpha=225, rim_alpha=0, corners=(1, 0, 1, 0))

    def f(dr, P, q):
        if reach:
            dr.polygon([P(*p) for p in pts], fill=(ACTIVE if sel else CHROME) + (255,))
        elif sel:
            dr.line([P(*p) for p in pts + pts[:1]], fill=col + (255,), width=max(1, int(cw * 0.08 * q)))
    s.shape(f, box=(x - 4, y - 4, x + cw + 4, y + c + 4))
    s.text((x + cw / 2, y + c / 2), label, t, fill=NAVYD if reach else (CHROME if not sel and ok and False else col), anchor='mm', shadow=not reach)
    if not ok:
        def sl(dr, P, q):
            dr.line([P(x + cw * 0.08, y + c * 0.92), P(x + cw * 0.92, y + c * 0.08)], fill=AMBER + (255,), width=max(1, int(cw * 0.10 * q)))
        s.shape(sl, box=(x - 2, y - 2, x + cw + 2, y + c + 2))
    if lock == 'tick':
        def tk(dr, P, q):
            dr.polygon([P(x + cw * 0.14, y - c * 0.10), P(x + cw * 0.5, y - c * 0.36), P(x + cw * 0.86, y - c * 0.10)], fill=INK + (255,))
        s.shape(tk, box=(x - 2, y - c * 0.5, x + cw + 2, y))


def cue3(src, n, k, cells, seat='above', deck=False):
    """cells: (label, kwargs of cell)."""
    s = hs.Skin(src, deck)
    cx, cy = s.W / 2, s.H / 2
    R = s.h(0.10 * k)
    cw, c, gp, sep = s.h(0.020), s.h(0.026), s.h(0.003), s.h(0.008)
    if seat == 'above':
        gaps = [(sep if cells[i][0].isdigit() and not cells[i + 1][0].isdigit() else gp) for i in range(len(cells) - 1)]
        x = cx - (len(cells) * cw + sum(gaps)) / 2
        y = cy - R * 0.93 - c / 2
        for i, (lb, kw) in enumerate(cells):
            cell(s, x, y, cw, c, s.h(0.024), lb, **kw)
            x += cw + (gaps[i] if i < len(cells) - 1 else 0)
    else:
        x = cx + R + s.h(0.034 * k)
        y = cy - (len(cells) * c + (len(cells) - 1) * gp * 3) / 2
        for lb, kw in cells:
            cell(s, x, y, cw, c, s.h(0.024), lb, **kw)
            y += c + gp * 3
    return save(s, n)


SEL = dict(sel=True)
SC_FAR = [('1', SEL), ('2', {}), ('3', {}), ('M', {}), ('T', {})]
SC_GUNS = [('1', dict(sel=True, reach=True)), ('2', dict(reach=True)), ('3', {}), ('M', dict(reach=True)), ('T', dict(reach=True))]
SC_LOCK = [('1', dict(sel=True, reach=True)), ('2', dict(reach=True)), ('3', {}), ('M', dict(reach=True, lock='tick')), ('T', dict(reach=True, lock='tick'))]
SC_LOCKG = [('1', dict(sel=True, reach=True)), ('2', dict(reach=True)), ('3', {}), ('M', dict(reach=True, lock='glow')), ('T', dict(reach=True, lock='glow'))]
SC_OUT = [('1', dict(sel=True, reach=True)), ('2', dict(reach=True)), ('M', dict(reach=True, lock='tick')), ('T', dict(ok=False))]


def chart():
    os.makedirs(TMP, exist_ok=True)
    p = os.path.join(TMP, 'chart.png')
    rows = [('1', {}, 'A GROUP', 'dim chrome digit on a dark cell'), ('1', SEL, 'SELECTED', 'COLOUR: yellow, and a hairline'), ('2', dict(reach=True), 'IN REACH', 'FILL: solid, the digit dark'),
            ('1', dict(sel=True, reach=True), 'SELECTED, IN REACH', 'fill and colour together'), ('M', dict(reach=True, lock='tick'), 'LOCKED', 'FORM: a tick above; M and T only'),
            ('T', dict(ok=False), 'NO ROUNDS / CANNOT FIRE', 'FORM: amber and a slash, never filled'), ('M', dict(reach=True, glow=True), 'JUST CHANGED', 'GLOW: one pulse, 150-300 ms, then gone')]
    Image.new('RGBA', (1240, 150 * len(rows) + 20), (12, 16, 36, 255)).save(p)
    s = hs.Skin(p)
    f1, f2_ = 54, 40
    for i, (lb, kw, a, b) in enumerate(rows):
        y = 26 + i * 150
        cell(s, 40, y + 16, 76, 99, 92, lb, **kw)
        s.text((160, y + 42), a, f1, fill=INK)
        s.text((160, y + 94), b, f2_, fill=CHROME)
    return Image.open(save(s, 'chart_final'))


def d4():
    k4, kd = 1.0, 1.08
    src, srd = f2(K4, 'bracket_900_kbm'), f2(DECK, 'bracket_900_kbm')

    def pair(sc, n):
        return [ring_crop(cue3(src, f'{n}4', k4, sc), K4, 0.17), ring_crop(cue3(srd, f'{n}d', kd, sc, deck=True), DECK, 0.17)]
    sheet('D4_cue_language', 'D4. The cue: one meaning per channel', 'Colour says which group is selected. Fill says in reach. Form says locked or out. Glow says something just changed. Read with UI / UX research, sources below.',
          [('The language', '', [chart()], False),
           ('Far off: nothing in reach', 'Left 4K, right 1280x800. True frames, the marks drawn.', pair(SC_FAR, 'c1'), False),
           ('About 500 m: guns of group 1 and 2, M and T in reach', 'Fill only. A lit M does not mean locked.', pair(SC_GUNS, 'c2'), False),
           ('Locked', 'The tick is the game\'s lock, on M and T only.', pair(SC_LOCK, 'c3'), False),
           ('Out of torpedoes', '', pair(SC_OUT, 'c4'), False),
           ('Option: lock as a steady glow, the owner\'s avenue', 'Research expects glow and fill to read as one channel on a 16 x 21 px mark, and it was tested at no such size: judge from these frames, 1280x800 on the right.', pair(SC_LOCKG, 'c5'), False),
           ('Why this split', 'At this size fill, brightness and glow act as one channel, so only one of them can mean a state; colour plus form is the strongest independent pair (Healey; CHI 2026). Chrome and amber are too close by hue alone, so amber takes a slash (WCAG 1.4.1, XAG 102). Red is taken by hostility, so lock is not red though most games use it. Partly filled marks cannot be judged at a glance (FAA HF guidance), so fill is all or nothing. Reload is off the cue: gun refire would flicker over the 3 Hz rule.', [], False)],
          'Unknown: no real lock has been seen in 48 samples, so the tick is unproven in play. A state holds at least 250 ms. Option on request: a 2 px refire bar under M and T, behind a setting. research\\evidence\\hud2_gate\\d4_channels_research.md has the sources.')


def d5():
    rows = []
    for size, k, deck, word in ((K4, 1.0, False, '4K'), (DECK, 1.08, True, '1280x800')):
        src = f2(size, 'bracket_900_pad')
        a = cue3(src, f'd5a{size[0]}', k, SC_LOCK, 'above', deck)
        b = cue3(src, f'd5b{size[0]}', k, SC_LOCK, 'beside', deck)
        rows.append((word, a, b))
    sheet('D5_cue_seat', 'D5. Where does the weapon cue sit?', 'A real bracket centred on the ring: its name stands right where the row goes.',
          [('A. Above the ring, on small dark cells', 'As asked; it covers the name when centred, and the card also shows the name.', [ring_crop(rows[0][1], K4), ring_crop(rows[1][1], DECK)], True),
           ('B. A column beside the ring', 'Clears the label; where MW4 had it. The player said "above", from memory.', [ring_crop(rows[0][2], K4), ring_crop(rows[1][2], DECK)], False)],
          'Left 4K, right 1280x800. Real: scene, bracket, name, ring. Drawn: the cue.')


# ---- alerts
def radiation_base(size, off):
    w, h = size
    box = (int(w / 2 + h * 0.40), int(h * 0.04), int(w / 2 + h * 0.56), int(h * 0.19))

    def yellow(n):
        return sum(1 for r, gg, b in Image.open(g.frame(size, n)).convert('RGB').crop(box).getdata() if r > 200 and gg > 180 and b < 90)
    names = ('radiation__kbm', 'radiation__pad', 'radiation_full__kbm', 'radiation_full__pad')
    return g.frame(size, (min if off else max)(names, key=yellow))


def r5_form(size, deck):
    s = hs.Skin(radiation_base(size, True), deck)
    k = 1.08 if deck else 1.0
    s.text((s.h(0.075 if not deck else 0.03), s.h(0.135 if not deck else 0.135)), 'RADIATION  ·  LEAVE THE ZONE', s.h(0.026 * k), fill=AMBER)
    cx, cy, R = s.W / 2, s.H / 2, s.h(0.10 * k)
    r = R + s.h(0.034 * k)
    wd = s.h(0.007 * k)

    def f(d, P, q):
        d.arc([P(cx - r, cy - r), P(cx + r, cy + r)], 100, 124, fill=HOSTILE + (255,), width=int(wd * q))
    s.shape(f, box=(cx - r, cy - r, cx + r, cy + r + wd))
    s.text((s.W - s.h(0.03), s.h(0.975 if deck else 0.97) - s.h(0.20)), 'LAG 640 MS', s.h(0.024), fill=DIM, anchor='rm')
    return save(s, f'd6_{size[0]}')


def d6():
    a = g.frame(K4, 'radiation__kbm')
    today = radiation_base(K4, False)
    sheet('D6_radiation_tail_lag', 'D6. Radiation, tail, lag', 'Three warnings that HUD 2 has no home for. The native icons go; what replaces them?',
          [('Today', 'The game\'s icons, over the card and every window (issue 27).', [Image.open(today)], False),
           ('A. Cards in the danger stack', 'Consistent with issue 21, "every danger showing"; a standing card can push others out, and the tail has no cure to name.', [Image.open(os.path.join(g.OUT, 'alerts_3840x2160.png'))], False),
           ('', '1280x800', [Image.open(os.path.join(g.OUT, 'alerts_1280x800.png'))], False),
           ('B. Amber nav line, a mark on the ring, one small word', 'Radiation: a standing line, a card once on entry. Tail: an unlabelled mark at its bearing, like the hit arc: must be learned. Lag: multiplayer only.', [Image.open(r5_form(K4, False))], True),
           ('', '1280x800', [Image.open(r5_form(DECK, True))], True)],
          'Real: scene and today\'s HUD, a real MISSILE INCOMING card where one stood. Drawn: the new forms. Lag shown here though it is multiplayer only.')


# ---- D1
def d1():
    # logo
    src = f2(K4, 'menu_nologo')
    s = hs.Skin(src)
    s.text((s.h(0.075), s.h(0.075)), 'FREELANCER', s.h(0.085), fill=INK, track=0.22)
    s.line([(s.h(0.075), s.h(0.125)), (s.h(0.92), s.h(0.125))], col=CHROME, w=s.h(0.0016), alpha=200)
    mark = save(s, 'd1_wordmark')
    w, h = K4
    # the cursor: the game's, then HUD 2's at the same tip, over the menu's scene (its pixels from the frame with the cursor on the button)
    im = Image.open(f2(K4, 'menu_cursor_free')).convert('RGBA')
    other = Image.open(f2(K4, 'menu_cursor_hover')).convert('RGBA')
    tip = (int(w * 0.5), int(h * 0.55))
    b = (tip[0] - 40, tip[1] - 40, tip[0] + int(h * 0.075), tip[1] + int(h * 0.09))
    im2 = im.copy()
    im2.paste(other.crop(b), b[:2])
    p = save(im2, 'd1_nocursor')
    s = hs.Skin(p)
    z = h * 0.052

    def arrow(d, P, q):
        x, y = tip
        pts = [(x, y), (x + z * 0.20, y + z), (x + z * 0.40, y + z * 0.62), (x + z * 0.86, y + z * 0.56)]
        d.polygon([P(*t) for t in pts], fill=(10, 14, 40, 235))
        d.line([P(*t) for t in pts + pts[:1]], fill=CHROME + (255,), width=int(h * 0.0022 * q))
    s.shape(arrow, box=(tip[0] - 6, tip[1] - 6, tip[0] + z + 6, tip[1] + z + 6))
    hc = save(s, 'd1_hudcursor')

    def cc(pth):
        c = int(h * 0.15)
        return Image.open(pth).convert('RGB').crop((tip[0] - c, tip[1] - c, tip[0] + c, tip[1] + c))
    logo = Image.open(f2(K4, 'menu_cursor_free')).convert('RGB').crop((0, int(h * 0.03), int(w * 0.5), int(h * 0.42)))
    mk = Image.open(mark).convert('RGB').crop((0, int(h * 0.03), int(w * 0.5), int(h * 0.42)))
    sheet('D1_logo_and_cursor', 'D1. How far does "HUD 2\'s, not the game\'s" reach?', 'Said of a menu arrow, then made a rule. Chrome surely. The logo and the cursor too?',
          [('A. Keep the game\'s logo and cursor', 'Identity stays the game\'s. Neither fixes a legibility fault. A real logo over a new UI is the seam a complete skin is meant to remove.', [logo, cc(f2(K4, 'menu_cursor_free'))], True),
           ('B. HUD 2\'s wordmark and arrow', 'A complete skin, "all new". Costs a drawn set of cursor shapes, and the wordmark and arrow here are first drafts.', [mk, cc(hc)], False)],
          'Real: the scene, the menu, the game\'s logo and cursor. Drawn: wordmark, arrow, at the same place and size. The critique also offers the cursor as an opt-in.')


# ---- D2
def d2():
    im = Image.open(g.frame(K4, 'calm__kbm')).convert('RGBA')
    b = (262, 76, 560, 142)
    im.paste(im.filter(ImageFilter.GaussianBlur(40)).crop(b), b[:2])
    p = save(im, 'd2_base')
    s = hs.Skin(p)
    s.label((286, 108), 'NEURAL NET', s.h(0.024), col=INK, track=0.10)
    nn = save(s, 'd2_nav')
    rows = {}
    for lead, names, key in (('NEURONET', ['LOG', 'INVENTORY', 'MAP', 'STATUS', 'HELP'], 'a'), ('NEURAL NET', ['LOG', 'INVENTORY', 'MAP', 'STATUS', 'INFO'], 'b')):
        for size, deck in ((K4, False), (DECK, True)):
            s = hs.Skin(g.no_box(size, 'nn_inventory__pad'), deck)
            s.tabs('INVENTORY', True, names=names, lead=lead)
            rows[key, size] = save(s, f'd2_{key}{size[0]}')

    def strip(p_, size):
        w, h = size
        return Image.open(p_).convert('RGB').crop((0, int(h * 0.86), int(w * 0.52), h))

    def top(p_):
        return Image.open(p_).convert('RGB').crop((0, 0, 1500, 330))
    sheet('D2_neuronet_or_neural_net', 'D2. NEURONET or NEURAL NET?', 'The game\'s 185 strings say Neural Net; its window reads NEURAL NET LOG. NeuroNet is the code\'s name, and the word the owner has used since 18 Oct.',
          [('A. NEURONET, HELP', 'FLvibed\'s voice, short.', [top(g.frame(K4, 'calm__kbm'))], False),
           ('', '4K', [strip(rows['a', K4], K4)], False),
           ('', '1280x800', [strip(rows['a', DECK], DECK)], False),
           ('B. NEURAL NET, INFO', 'The game\'s own word; wider, and it fits at 1280x800 here.', [top(nn)], True),
           ('', '4K', [strip(rows['b', K4], K4)], True),
           ('', '1280x800', [strip(rows['b', DECK], DECK)], True)],
          'Top: the flight label (A real, B redrawn). Strips: window real, strip drawn; the word above the strip is the mock\'s place for it: the design has no such word yet.')


# ---- D7
def d7():
    labels = [('recS', 'Small'), ('recN', 'Normal'), ('recL', 'Large')]

    def row(size, names):
        out = []
        for n in names:
            im = Image.open(f2(size, 'floor_' + n)).convert('RGB')
            w, h = size
            out.append(im.crop((int(w * 0.70), int(h * 0.76), w, h)))
        return out
    a, b = K4, DECK
    sheet('D7_floor_and_sizes', 'D7. The text floor and the three sizes', 'Every crop a real frame of the bottom right corner. Each row: Small, Normal, Large.',
          [('A. Floor 0.024 H on Normal, 0.021 H on Small only', '4K. k 0.88, 1.0, 1.18.', row(a, ['recS', 'recN', 'recL']), True),
           ('', '1280x800. Small keeps Normal\'s text (0.0216 H).', row(b, ['recS', 'recN', 'recL']), True),
           ('B. One floor, 0.024 H, no exception', '4K. Small\'s panels with the floor unmoved: k 0.88. Normal and Large as A.', row(a, ['oneS', 'recN', 'recL']), False),
           ('', '1280x800.', row(b, ['oneS', 'recN', 'recL']), False),
           ('C. The first guess: 0.75 / 0.875 / 1.0', '4K. Small is smaller than classic; every present player\'s HUD shrinks.', row(a, ['guessS', 'guessN', 'guessL']), False),
           ('', '1280x800. Lower case near 6.6 px on Small: under Valve\'s 9.', row(b, ['guessS', 'guessN', 'guessL']), False)],
          'All in game by a probe build, HUD 2 Standard, pad, `hud2 sim combat` over a planet. Crops are scaled to fit: judge in the full frames, frames2\\.')


# ---- D8, D9
def d8():
    w, h = K4
    p = os.path.join(TMP, 'navy.png')
    os.makedirs(TMP, exist_ok=True)
    Image.new('RGBA', K4, (6, 7, 22, 255)).save(p)
    s = hs.Skin(p)
    s.t(s.WH / 2, 0.11, 'CHOOSE YOUR LOOK', 0.05, INK, 'mm')
    s.t(s.WH / 2, 0.17, 'You can change this at any time in Options.', 0.03, CHROME, 'mm')
    fw = 0.74
    for i, (n, word) in enumerate((('fight__pad', 'HUD 2'), ('fight__classic', 'CLASSIC'))):
        x = 0.05 + i * (fw + 0.015) + 0.0
        im = fit(Image.open(g.frame(K4, n)).convert('RGBA'), s.h(fw))
        s.paste(im, (s.h(x), s.h(0.25)))
        s.chip((x + 0.14, 0.72, x + fw - 0.14, 0.80), word, focus=i == 0)
    s.lane([('A', 'SELECT'), ('D', 'CHOOSE')])
    pg = save(s, 'd8_page')
    sheet('D8_first_run', 'D8. A first-run look choice?', 'One page in setup, or none.',
          [('A. One HUD 2 / Classic page', 'Defuses the likeliest resentment; one more step.', [Image.open(pg)], True),
           ('B. No page', 'Straight to the menu; the choice is one setting away, and the README announces HUD 2.', [Image.open(g.front('mainmenu', K4))], False)],
          'Drawn: the page. The two pictures in it are real frames. The page\'s home (setup or first start) is open.')


def d9():
    size = K4
    w, h = size
    out = {}
    for key, items in (('a', ['CONTINUE', 'NEW GAME', 'LOAD GAME', 'MULTIPLAYER', 'OPTIONS', 'EXIT']), ('b', ['NEW GAME', 'LOAD GAME', 'MULTIPLAYER', 'OPTIONS', 'EXIT'])):
        im = Image.open(g.front('mainmenu', size)).convert('RGBA')
        soft = im.filter(ImageFilter.GaussianBlur(h * 0.02))
        b = (0, int(h * 0.24), int(w * 0.26), int(h * 0.86))
        im.paste(soft.crop(b), b[:2])
        p = save(im, f'd9base{key}')
        s = hs.Skin(p)
        x0, x1, bh = 0.044 * s.WH, 0.2275 * s.WH, 0.056
        step = 0.1072 if key == 'b' else 0.090
        for i, c in enumerate(items):
            y = 0.267 + i * step
            s.chip((x0, y, x1, y + bh), c, focus=i == 0)
        if key == 'a':
            s.small(x1 + 0.02, 0.267 + bh / 2, 'MISSION 2  ·  MANHATTAN', CHROME, 0.024)
        out[key] = save(s, f'd9{key}')
    sheet('D9_continue', 'D9. CONTINUE on the main menu?', 'The owner set it aside as a feature, not a skin (13.3). The critique would trade the wordmark\'s cost for it: one press to resume.',
          [('A. CONTINUE on top', 'One press to resume; "latest save" has its own failure cases.', [Image.open(out['a'])], True),
           ('B. The menu as it is', 'A skin stays a skin.', [Image.open(out['b'])], False)],
          'Real: scene and logo. Drawn: the chips. Focus as built.')


def d3():
    sheet('D3_settled', 'D3. One release, or a 0.3.1 now?', 'Already answered by the owner on 6 Nov 2026: 0.3.5 = the refactor and all bug fixes; 0.4.0 = the HUD 2 overhaul and the options menu.',
          [('Nothing to decide here', 'The critique\'s pick was 0.3.1, then 0.4.0, then the grown map in 0.5.0. The one piece left open: whether the grown map ships in 0.4.0 or after it (R7).', [], False)])


ALL = {'D1': d1, 'D2': d2, 'D3': d3, 'D4': d4, 'D5': d5, 'D6': d6, 'D7': d7, 'D8': d8, 'D9': d9}

if __name__ == '__main__':
    os.makedirs(OUT, exist_ok=True)
    for n in (sys.argv[1:] or list(ALL)):
        ALL[n]()
