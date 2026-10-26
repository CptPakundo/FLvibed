"""hud2.py - the ground-up pad HUD ("HUD 2", 25 Oct 2026): component kit and state renders on HUD-free 4K frames.

    research\\tools\\ocr\\.venv\\Scripts\\python.exe -I tools\\uimock\\hud2.py [state ...]

Frames: captured with the game's whole HUD hidden (the HUDless bytes, debug `poke`, instance 8, SaveC705); the
subtitles' lines are painted out (the design places the comm feed itself). Icons: HDE's flat icon sheets
(DATA\\INTERFACE\\buttontextures_flaticons.txm Montage32 / Montage64, HUD_targetingelement), extracted to
research\\evidence\\controller_ui\\hud_redesign\\fl_assets\\ (git-ignored; nothing of the game is committed).
Sizes are fractions of H (the mod's rule); the palette is the HUD's own.
"""
import glob
import math
import os
import sys

from PIL import Image, ImageDraw, ImageFilter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hudmock as hm  # noqa: E402

E = hm.EVID
ASSETS = os.path.join(E, 'fl_assets')
OUT = os.path.join(E, 'hud2')

# ---- palette (the HUD's own, sampled; display.cpp HudPanel) ----
INK = (226, 240, 252)        # primary text: near-white with the HUD's blue bias
CHROME = (130, 201, 243)     # lines, labels, glyph rings
DIM = (92, 128, 160)         # secondary text, empty segments
GLASS = (16, 18, 44)         # panel fill
CYAN = (24, 172, 226)        # accent: the HUD's header bar
ACTIVE = (252, 243, 68)      # the game's active yellow
HOSTILE = (236, 64, 52)      # the game's hostile red
FRIEND = (96, 214, 96)       # the game's friendly green
NEUTRAL = (232, 232, 232)
SHIELD = (78, 132, 255)      # the gauge's blue
HULL = (214, 62, 62)         # the gauge's red
POWER = (232, 210, 92)       # the gauge's yellow
SS = 4


def asset(name):
    return Image.open(os.path.join(ASSETS, name)).convert('RGBA')


_M32 = _M64 = _TGT = None


def m32(c, r):
    global _M32
    _M32 = _M32 or asset('buttontextures_flaticons__Montage32.png')
    return _M32.crop((c * 128, r * 128, c * 128 + 128, r * 128 + 128))


def m64(c, r):
    global _M64
    _M64 = _M64 or asset('buttontextures_flaticons__Montage64.png')
    return _M64.crop((c * 256, r * 256, c * 256 + 256, r * 256 + 256))


def tgt(box):
    global _TGT
    _TGT = _TGT or asset('buttontextures_flaticons__HUD_targetingelement.png')
    return _TGT.crop(box)


# Icons by meaning (HDE flat set; Montage64 = the maneuver bar's, Montage32 = contacts, panels, equipment)
ICON = {
    'cruise': ('64', 0, 0), 'map': ('64', 1, 0), 'free': ('64', 0, 1), 'goto': ('64', 2, 1), 'dock': ('64', 0, 2),
    'formation': ('64', 2, 2), 'comms': ('64', 0, 3), 'player': ('64', 3, 3),
    'fighter': ('32', 0, 6), 'enemy': ('32', 7, 4), 'station': ('32', 7, 6), 'jumpgate': ('32', 0, 7),
    'lane': ('32', 5, 6), 'waypoint': ('32', 2, 6), 'planet': ('32', 7, 7), 'transport': ('32', 1, 7),
    'tractor': ('32', 4, 4), 'important': ('32', 7, 5), 'info': ('32', 0, 4), 'next': ('32', 5, 4),
    'star': ('32', 4, 6), 'mine': ('32', 4, 7), 'group': ('32', 6, 6), 'loot': ('32', 5, 7), 'list': ('32', 2, 5),
}


class HUD(hm.Canvas):
    """hudmock.Canvas plus the HUD 2 kit."""

    def __init__(self, path, clean=None):
        super().__init__(path)
        for box in clean or []:
            self.erase(box)

    op = 1.0   # the current element's opacity (contextual fading: calm states dim the ring, the loadout)

    def paste(self, layer, xy=(0, 0)):
        """alpha_composite clipped to the frame (layers may hang over an edge), at the current opacity."""
        if self.op < 1.0:
            layer = layer.copy()
            layer.putalpha(layer.getchannel('A').point(lambda v, k=self.op: int(v * k)))
        x, y = int(round(xy[0])), int(round(xy[1]))
        cx0, cy0 = max(0, -x), max(0, -y)
        cx1, cy1 = min(layer.width, self.W - x), min(layer.height, self.H - y)
        if cx1 <= cx0 or cy1 <= cy0:
            return
        if (cx0, cy0, cx1, cy1) != (0, 0, layer.width, layer.height):
            layer = layer.crop((cx0, cy0, cx1, cy1))
        self.im.alpha_composite(layer, (x + cx0, y + cy0))

    # -- icons tinted to a colour (the sheets are white on alpha)
    def ficon(self, name, cx, cy, size, col=CHROME, alpha=255):
        sheet, c, r = ICON[name]
        im = m64(c, r) if sheet == '64' else m32(c, r)
        a = im.getchannel('A').point(lambda v: v * alpha // 255)
        lum = im.convert('L')
        a = Image.composite(a, Image.new('L', im.size, 0), lum.point(lambda v: 255 if v > 30 else 0))
        tinted = Image.new('RGBA', im.size, col + (0,))
        tinted.putalpha(a)
        tinted = tinted.resize((int(size), int(size)), Image.LANCZOS)
        self.paste(tinted, (cx - size / 2, cy - size / 2))

    # -- a layer to draw supersampled shapes on, composited at once
    def layer(self):
        return Image.new('RGBA', (self.W * 1, self.H * 1), (0, 0, 0, 0))

    def shape(self, draw_fn, glow=0, box=None):
        """draw_fn(d, s) draws at supersampled scale s into a box-local layer; box = (x0, y0, x1, y1) in frame px."""
        x0, y0, x1, y1 = [int(v) for v in box]
        pad = 40
        w, h = x1 - x0 + 2 * pad, y1 - y0 + 2 * pad
        lay = Image.new('RGBA', (w * SS, h * SS), (0, 0, 0, 0))
        d = ImageDraw.Draw(lay)
        draw_fn(d, lambda x, y: ((x - x0 + pad) * SS, (y - y0 + pad) * SS), SS)
        lay = lay.resize((w, h), Image.LANCZOS)
        if glow:
            self.paste(lay.filter(ImageFilter.GaussianBlur(glow)), (x0 - pad, y0 - pad))
        self.paste(lay, (x0 - pad, y0 - pad))

    # -- the FL segmented bar (the native gauge's blocks, thinner and lighter)
    def segbar(self, x, y, w, h, frac, col, n=20, gap=None, empty=None, ghost=None, align='l'):
        gap = gap if gap is not None else max(2, h * 0.28)
        sw = (w - gap * (n - 1)) / n
        lit = frac * n
        empty = empty or (col[0] // 5 + 14, col[1] // 5 + 14, col[2] // 5 + 22)

        def f(d, P, s):
            for i in range(n):
                k = i if align == 'l' else n - 1 - i
                a = P(x + k * (sw + gap), y)
                b = P(x + k * (sw + gap) + sw, y + h)
                if i < int(lit):
                    d.rectangle([a, b], fill=col + (255,))
                elif i < lit:   # the partial block
                    part = lit - int(lit)
                    d.rectangle([a, b], fill=empty + (200,))
                    if align == 'l':
                        d.rectangle([a, (a[0] + (b[0] - a[0]) * part, b[1])], fill=col + (255,))
                    else:
                        d.rectangle([(b[0] - (b[0] - a[0]) * part, a[1]), b], fill=col + (255,))
                elif ghost and i < ghost * n:   # recently lost: the damage ghost
                    d.rectangle([a, b], fill=(255, 255, 255, 150))
                else:
                    d.rectangle([a, b], fill=empty + (200,))
        self.shape(f, glow=0, box=(x, y, x + w, y + h))

    def arcbar(self, cx, cy, r, a0, a1, width, frac, col, n=14, gapdeg=1.6, empty=None, reverse=False):
        """A segmented arc around (cx, cy) from a0 to a1 degrees (PIL: 0 = 3 o'clock, clockwise)."""
        empty = empty or (40, 48, 80)
        span = (a1 - a0) / n

        def f(d, P, s):
            for i in range(n):
                k = n - 1 - i if reverse else i
                b0 = a0 + k * span + gapdeg / 2
                b1 = a0 + (k + 1) * span - gapdeg / 2
                lit = i < frac * n
                c0 = P(cx - r, cy - r)
                c1 = P(cx + r, cy + r)
                d.arc([c0, c1], b0, b1, fill=(col if lit else empty) + (255 if lit else 170,), width=int(width * s))
        self.shape(f, box=(cx - r - width, cy - r - width, cx + r + width, cy + r + width))

    def line(self, pts, col=CHROME, w=2, alpha=255):
        xs = [p[0] for p in pts]
        ys = [p[1] for p in pts]

        def f(d, P, s):
            d.line([P(*p) for p in pts], fill=col + (alpha,), width=int(w * s), joint='curve')
        self.shape(f, box=(min(xs), min(ys), max(xs), max(ys)))

    def plate(self, box, chamfer=None, alpha=150, rim=CHROME, rim_alpha=110, corners=(1, 0, 1, 0), accent=None):
        """A light glass plate: chamfered on the corners flagged (tl, tr, br, bl), hairline rim, optional accent edge
        ('l' / 't' / 'b' in a colour)."""
        x0, y0, x1, y1 = box
        c = chamfer if chamfer is not None else self.h(0.012)
        tl, tr, br, bl = [c * k for k in corners]
        pts = [(x0 + tl, y0), (x1 - tr, y0), (x1, y0 + tr), (x1, y1 - br), (x1 - br, y1), (x0 + bl, y1), (x0, y1 - bl),
               (x0, y0 + tl)]

        def f(d, P, s):
            d.polygon([P(*p) for p in pts], fill=GLASS + (alpha,))
            d.line([P(*p) for p in pts + [pts[0]]], fill=rim + (rim_alpha,), width=int(max(1.5, self.h(0.0009)) * s))
            if accent:
                side, col = accent
                aw = self.h(0.003)
                if side == 'l':
                    d.rectangle([P(x0, y0 + tl), P(x0 + aw, y1 - bl)], fill=col + (255,))
                elif side == 't':
                    d.rectangle([P(x0 + tl, y0), P(x1 - tr, y0 + aw)], fill=col + (255,))
                elif side == 'b':
                    d.rectangle([P(x0 + bl, y1 - aw), P(x1 - br, y1)], fill=col + (255,))
        self.shape(f, box=box)

    def text(self, xy, s, size, fill=INK, bold=True, anchor='lm', shadow=True, track=0.0, alpha=255):
        """Text in the HUD's face drawn on a local layer (fast); `track` = letter spacing in ems; returns the width."""
        f = hm.font(size, bold)
        meas = ImageDraw.Draw(Image.new('L', (1, 1)))
        widths = [meas.textlength(ch, font=f) for ch in s]
        total = (sum(widths) + track * size * max(0, len(s) - 1)) if track else meas.textlength(s, font=f)
        x, y = xy
        if anchor[0] == 'r':
            x -= total
        elif anchor[0] == 'm':
            x -= total / 2
        pad = int(size * 0.6) + 8
        w, h = int(total) + 2 * pad, int(size * 1.6) + 2 * pad
        lay = Image.new('RGBA', (w, h), (0, 0, 0, 0))
        d = ImageDraw.Draw(lay)
        ya = {'m': 'lm', 't': 'lt', 'b': 'lb', 's': 'ls'}[anchor[1]]
        oy = pad + int(size * 0.8)
        col = fill + (alpha,)
        if track:
            cx = pad
            for ch, cw in zip(s, widths):
                d.text((cx, oy), ch, font=f, fill=col, anchor=ya)
                cx += cw + track * size
        else:
            d.text((pad, oy), s, font=f, fill=col, anchor=ya)
        if shadow:   # a navy outline + soft shadow: text without a panel must hold on the bright nebula (XAG 102)
            k = max(3, int(size * 0.09) | 1)
            sh = Image.new('RGBA', lay.size, (6, 6, 20, 0))
            sh.putalpha(lay.getchannel('A').filter(ImageFilter.MaxFilter(k)).point(lambda v: v * 4 // 5))
            self.paste(sh.filter(ImageFilter.GaussianBlur(max(1.5, size * 0.05))), (x - pad, y - oy))
        self.paste(lay, (x - pad, y - oy))
        return total

    def textlen(self, s, size, bold=True, track=0.0):
        f = hm.font(size, bold)
        meas = ImageDraw.Draw(Image.new('L', (1, 1)))
        if not track:
            return meas.textlength(s, font=f)
        return sum(meas.textlength(ch, font=f) for ch in s) + track * size * max(0, len(s) - 1)

    def label(self, xy, s, size, col=DIM, anchor='lm', track=0.10, bold=True):
        """Small caps label with letter spacing."""
        return self.text(xy, s, size, fill=col, bold=bold, anchor=anchor, shadow=True, track=track)

    def bracket(self, cx, cy, w, h, col=HOSTILE, size=None):
        """The game's target corner brackets (HUD_targetingelement, bottom-left corner piece, rotated per corner)."""
        corner = tgt((0, 768, 256, 1024))
        a = corner.getchannel('A')
        tinted = Image.new('RGBA', corner.size, col + (0,))
        tinted.putalpha(a)
        s = int(size or self.h(0.028))
        tinted = tinted.resize((s, s), Image.LANCZOS)
        bl = tinted
        tl = bl.transpose(Image.FLIP_TOP_BOTTOM)
        br = bl.transpose(Image.FLIP_LEFT_RIGHT)
        tr = tl.transpose(Image.FLIP_LEFT_RIGHT)
        self.paste(tl, (cx - w / 2, cy - h / 2))
        self.paste(tr, (cx + w / 2 - s, cy - h / 2))
        self.paste(bl, (cx - w / 2, cy + h / 2 - s))
        self.paste(br, (cx + w / 2 - s, cy + h / 2 - s))

    def lead(self, cx, cy, size, col=HOSTILE):
        """The game's four-triangle lead marker (HUD_targetingelement)."""
        im = tgt((256, 256, 512, 512))
        a = im.getchannel('A')
        t = Image.new('RGBA', im.size, col + (0,))
        t.putalpha(a)
        t = t.resize((int(size), int(size)), Image.LANCZOS)
        self.paste(t, (cx - size / 2, cy - size / 2))

    def arrow(self, cx, cy, size, angle, col=HOSTILE):
        """The game's off-screen target arrow (HUD_targetingelement, the outlined triangle)."""
        im = tgt((0, 256, 256, 512))
        a = im.getchannel('A')
        t = Image.new('RGBA', im.size, col + (0,))
        t.putalpha(a)
        t = t.resize((int(size), int(size)), Image.LANCZOS).rotate(angle - 224, resample=Image.BICUBIC, expand=True)  # the texture's arrow points 224 deg CCW
        self.paste(t, (cx - t.width / 2, cy - t.height / 2))


def base(name):
    return sorted(glob.glob(os.path.join(E, name + '-*.png')))[-1]
