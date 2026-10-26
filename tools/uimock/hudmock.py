"""hudmock.py - pixel mock-ups of pad-HUD proposals, composited onto real 4K game frames.

Draws in the HUD's own language (the panel base, the glass rim, the cyan header bar, Agency FB) so a proposal can
be judged at full resolution against the game before anything is built. Inputs are frames captured with the pad
layer off (debug `padhud off`), outputs go to research\\evidence\\controller_ui\\hud_redesign\\ (git-ignored).

    research\\tools\\ocr\\.venv\\Scripts\\python.exe -I tools\\uimock\\hudmock.py [--out DIR]

Sizes are fractions of the frame height H (the mod's rule); coordinates of the game's own panels are measured on
the 4K captures of 25 Oct 2026 (HudShift layout) and scale with H.
"""
import argparse
import math
import os
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
EVID = os.path.join(ROOT, 'research', 'evidence', 'controller_ui', 'hud_redesign')
FONTS = os.path.join(ROOT, 'Install', 'Freelancer HD Edition', 'DATA', 'FONTS')

# The HUD's palette (sampled from the 4K frames; display.cpp HudPanel)
BASE = (13, 11, 26)
GLASS = (22, 24, 62)
RIM = (150, 196, 228)
CHROME = (130, 201, 243)
CYAN = (24, 172, 226)
ACTIVE = (252, 243, 68)
DIM = (96, 120, 150)
FACE = {'A': (110, 200, 80), 'B': (232, 80, 66), 'X': (74, 146, 236), 'Y': (240, 196, 52)}

SS = 4  # supersampling for glyphs and shapes
COLORED = True  # face-button letters in their family colours (False: the HUD's neutral tint)


def font(size, bold=True):
    return ImageFont.truetype(os.path.join(FONTS, 'AGENCYB.TTF' if bold else 'AGENCYR.TTF'), max(1, int(size)))


class Canvas:
    def __init__(self, base_path):
        self.im = Image.open(base_path).convert('RGBA')
        self.W, self.H = self.im.size

    def h(self, f):
        return f * self.H

    def paste(self, layer, xy=(0, 0)):
        self.im.alpha_composite(layer, (int(round(xy[0])), int(round(xy[1]))))

    # --- erase: rebuild a region from the space around it (for removed HUD parts) ---
    def erase(self, box, src_box=None):
        import numpy as np
        import cv2
        x0, y0, x1, y1 = map(int, box)
        if src_box:  # clone a patch of plain space
            sx0, sy0 = map(int, src_box[:2])
            patch = self.im.crop((sx0, sy0, sx0 + (x1 - x0), sy0 + (y1 - y0)))
            self.im.paste(patch, (x0, y0))
            return
        arr = cv2.cvtColor(np.array(self.im.convert('RGB')), cv2.COLOR_RGB2BGR)
        mask = np.zeros(arr.shape[:2], np.uint8)
        mask[y0:y1, x0:x1] = 255
        out = cv2.inpaint(arr, mask, 9, cv2.INPAINT_TELEA)
        self.im = Image.fromarray(cv2.cvtColor(out, cv2.COLOR_BGR2RGB)).convert('RGBA')

    # --- the HUD's panel: glass fill, rim with glow, chamfered corners, optional cyan foot bar ---
    def panel(self, box, chamfer=None, foot=False, alpha=170, rim=RIM, rim_w=None, glow=True):
        x0, y0, x1, y1 = box
        c = chamfer if chamfer is not None else self.h(0.018)
        rw = rim_w or max(2, self.h(0.0016))
        w, hh = int(x1 - x0 + 40), int(y1 - y0 + 40)
        lay = Image.new('RGBA', (w * SS, hh * SS), (0, 0, 0, 0))
        d = ImageDraw.Draw(lay)
        o = 20 * SS
        X0, Y0, X1, Y1, C = o, o, o + (x1 - x0) * SS, o + (y1 - y0) * SS, c * SS
        poly = [(X0 + C, Y0), (X1 - C, Y0), (X1, Y0 + C), (X1, Y1 - C), (X1 - C, Y1), (X0 + C, Y1), (X0, Y1 - C), (X0, Y0 + C)]
        d.polygon(poly, fill=GLASS + (alpha,))
        # the faint inner gradient the HUD's glass has (lighter top)
        grad = Image.new('RGBA', lay.size, (0, 0, 0, 0))
        gd = ImageDraw.Draw(grad)
        for i in range(0, int((Y1 - Y0) * 0.45), SS):
            a = int(38 * (1 - i / ((Y1 - Y0) * 0.45)))
            gd.line([(X0, Y0 + i), (X1, Y0 + i)], fill=(120, 150, 220, a))
        m = Image.new('L', lay.size, 0)
        ImageDraw.Draw(m).polygon(poly, fill=255)
        lay = Image.composite(Image.alpha_composite(lay, grad), lay, m)
        d = ImageDraw.Draw(lay)
        if foot:
            fh = self.h(0.008) * SS
            d.polygon([(X0 + C * 0.6, Y1 - fh), (X1 - C * 0.6, Y1 - fh), (X1 - C, Y1), (X0 + C, Y1)], fill=CYAN + (235,))
        d.line(poly + [poly[0]], fill=rim + (255,), width=int(rw * SS), joint='curve')
        lay = lay.resize((w, hh), Image.LANCZOS)
        if glow:
            g = Image.new('RGBA', lay.size, (0, 0, 0, 0))
            gl = Image.new('RGBA', (w * SS, hh * SS), (0, 0, 0, 0))
            ImageDraw.Draw(gl).line(poly + [poly[0]], fill=rim + (150,), width=int(rw * SS * 3))
            g = gl.resize((w, hh), Image.LANCZOS).filter(ImageFilter.GaussianBlur(self.h(0.003)))
            self.paste(g, (x0 - 20, y0 - 20))
        self.paste(lay, (x0 - 20, y0 - 20))

    def text(self, xy, s, size, fill=CHROME, bold=True, anchor='lm', shadow=True):
        lay = Image.new('RGBA', self.im.size, (0, 0, 0, 0))
        d = ImageDraw.Draw(lay)
        f = font(size, bold)
        if shadow:
            sh = Image.new('RGBA', self.im.size, (0, 0, 0, 0))
            ImageDraw.Draw(sh).text(xy, s, font=f, fill=(0, 0, 0, 200), anchor=anchor)
            self.paste(sh.filter(ImageFilter.GaussianBlur(max(1, size * 0.06))))
        d.text(xy, s, font=f, fill=fill + (255,) if len(fill) == 3 else fill, anchor=anchor)
        self.paste(lay)
        return d.textlength(s, font=f)

    def textlen(self, s, size, bold=True):
        return ImageDraw.Draw(Image.new('L', (1, 1))).textlength(s, font=font(size, bold))

    # --- glyphs: the HUD's chrome; face buttons keep their family colour on the letter ---
    def glyph(self, kind, cx, cy, size, tint=CHROME, colored=None, hold=None, lit=False, alpha=255):
        if colored is None:
            colored = COLORED
        S = int(size * SS * 1.6)
        lay = Image.new('RGBA', (S, S), (0, 0, 0, 0))
        d = ImageDraw.Draw(lay)
        r = size * SS / 2
        c = S / 2
        lw = max(SS, int(size * SS * 0.075))
        ring = ACTIVE if lit else tint
        fill = BASE + (225,)
        fs = size * SS
        if kind in FACE:
            d.ellipse([c - r, c - r, c + r, c + r], fill=fill, outline=ring + (255,), width=lw)
            col = FACE[kind] if colored else ring
            d.text((c, c + fs * 0.03), kind, font=font(fs * 0.72), fill=col + (255,), anchor='mm')
        elif kind in ('LB', 'RB'):
            w2, h2 = r * 1.25, r * 0.72
            d.rounded_rectangle([c - w2, c - h2, c + w2, c + h2], radius=h2 * 0.9, fill=fill, outline=ring + (255,), width=lw)
            d.text((c, c + fs * 0.02), kind, font=font(fs * 0.58), fill=ring + (255,), anchor='mm')
        elif kind in ('LT', 'RT'):
            w2, h2 = r * 0.82, r * 1.0
            d.rounded_rectangle([c - w2, c - h2, c + w2, c + h2], radius=w2 * 0.55, fill=fill, outline=ring + (255,), width=lw)
            d.text((c, c + fs * 0.06), kind, font=font(fs * 0.5), fill=ring + (255,), anchor='mm')
        elif kind in ('R3', 'L3', 'RS', 'LS'):
            d.ellipse([c - r, c - r, c + r, c + r], fill=fill, outline=ring + (255,), width=lw)
            ri = r * 0.68
            d.ellipse([c - ri, c - ri, c + ri, c + ri], outline=ring + (200,), width=max(SS, lw // 2))
            d.text((c, c + fs * 0.03), kind[0], font=font(fs * 0.55), fill=ring + (255,), anchor='mm')
            if kind.endswith('3'):  # the click: a small arrow on top
                d.polygon([(c - r * 0.25, c - r * 1.02), (c + r * 0.25, c - r * 1.02), (c, c - r * 0.78)], fill=ring + (255,))
        elif kind.startswith('D'):  # D-pad, the pressed arm white: DU DD DL DR, or D for the bare cross
            a = r * 0.36
            arms = {'U': (c - a, c - r, c + a, c - a), 'D': (c - a, c + a, c + a, c + r),
                    'L': (c - r, c - a, c - a, c + a), 'R': (c + a, c - a, c + r, c + a)}
            cross = [(c - a, c - r), (c + a, c - r), (c + a, c - a), (c + r, c - a), (c + r, c + a), (c + a, c + a),
                     (c + a, c + r), (c - a, c + r), (c - a, c + a), (c - r, c + a), (c - r, c - a), (c - a, c - a)]
            d.polygon(cross, fill=fill)
            for k, b in arms.items():
                if kind[1:] == k:
                    d.rectangle([b[0] + lw, b[1] + lw, b[2] - lw, b[3] - lw], fill=(255, 255, 255, 255))
            d.line(cross + [cross[0]], fill=ring + (255,), width=lw, joint='curve')
        elif kind == 'VIEW':
            q = r * 0.42
            d.ellipse([c - r, c - r, c + r, c + r], fill=fill, outline=ring + (255,), width=lw)
            d.rectangle([c - q * 1.2, c - q * 1.1, c + q * 0.5, c + q * 0.4], outline=ring + (255,), width=lw)
            d.rectangle([c - q * 0.5, c - q * 0.4, c + q * 1.2, c + q * 1.1], fill=ring + (255,))
        elif kind == 'MENU':
            d.ellipse([c - r, c - r, c + r, c + r], fill=fill, outline=ring + (255,), width=lw)
            for k in (-1, 0, 1):
                d.line([(c - r * 0.45, c + k * r * 0.32), (c + r * 0.45, c + k * r * 0.32)], fill=ring + (255,), width=lw)
        if hold is not None:  # a hold ring around the glyph (0..1 filled)
            rr = r * 1.24
            hw = max(SS, lw * 2 // 3)
            if hold >= 1:  # the legend's "hold" mark: a broken outer ring (four arcs)
                for k in range(4):
                    d.arc([c - rr, c - rr, c + rr, c + rr], k * 90 + 12, k * 90 + 78, fill=ring + (230,), width=hw)
            else:          # a hold in progress: the filled part of the ring
                d.arc([c - rr, c - rr, c + rr, c + rr], 0, 360, fill=DIM + (140,), width=hw)
                if hold > 0:
                    d.arc([c - rr, c - rr, c + rr, c + rr], -90, -90 + 360 * hold, fill=ring + (255,), width=hw)
        lay = lay.resize((S // SS, S // SS), Image.LANCZOS)
        if alpha < 255:
            lay.putalpha(lay.getchannel('A').point(lambda v: v * alpha // 255))
        self.paste(lay, (cx - S / SS / 2, cy - S / SS / 2))

    # --- simple line icons in the HUD's chrome (consumables the base frame has no icon for) ---
    def icon(self, kind, cx, cy, size, col=CHROME):
        S = int(size * SS * 1.4)
        lay = Image.new('RGBA', (S, S), (0, 0, 0, 0))
        d = ImageDraw.Draw(lay)
        c, r, lw = S / 2, size * SS / 2, max(SS, int(size * SS * 0.07))
        if kind == 'flare':  # countermeasure: a flare burst
            for k in range(8):
                a = k * math.pi / 4
                r0 = r * (0.25 if k % 2 else 0.35)
                d.line([(c + r0 * math.cos(a), c + r0 * math.sin(a)), (c + r * math.cos(a) * (0.75 if k % 2 else 1.0),
                        c + r * math.sin(a) * (0.75 if k % 2 else 1.0))], fill=col + (255,), width=lw)
            d.ellipse([c - r * 0.18, c - r * 0.18, c + r * 0.18, c + r * 0.18], fill=col + (255,))
        elif kind == 'mine':
            d.ellipse([c - r * 0.55, c - r * 0.55, c + r * 0.55, c + r * 0.55], outline=col + (255,), width=lw)
            for k in range(6):
                a = k * math.pi / 3 + math.pi / 6
                d.line([(c + r * 0.55 * math.cos(a), c + r * 0.55 * math.sin(a)), (c + r * 0.95 * math.cos(a),
                        c + r * 0.95 * math.sin(a))], fill=col + (255,), width=lw)
            d.ellipse([c - r * 0.15, c - r * 0.15, c + r * 0.15, c + r * 0.15], fill=col + (255,))
        elif kind == 'missile':
            d.polygon([(c - r, c + r * 0.12), (c + r * 0.6, c + r * 0.12), (c + r, c), (c + r * 0.6, c - r * 0.12),
                       (c - r, c - r * 0.12)], outline=col + (255,), width=lw)
            d.polygon([(c - r, c - r * 0.12), (c - r * 0.7, c - r * 0.42), (c - r * 0.55, c - r * 0.12)], fill=col + (255,))
            d.polygon([(c - r, c + r * 0.12), (c - r * 0.7, c + r * 0.42), (c - r * 0.55, c + r * 0.12)], fill=col + (255,))
        elif kind == 'gun':
            d.line([(c - r, c), (c + r * 0.4, c)], fill=col + (255,), width=lw * 2)
            for k in range(3):
                x = c + r * 0.55 + k * r * 0.22
                d.ellipse([x - lw, c - lw, x + lw, c + lw], fill=col + (255,))
        elif kind == 'burner':
            d.polygon([(c - r, c - r * 0.4), (c - r * 0.2, c), (c - r, c + r * 0.4)], outline=col + (255,), width=lw)
            d.polygon([(c - r * 0.1, c - r * 0.4), (c + r * 0.7, c), (c - r * 0.1, c + r * 0.4)], outline=col + (255,), width=lw)
        lay = lay.resize((S // SS, S // SS), Image.LANCZOS)
        self.paste(lay, (cx - S / SS / 2, cy - S / SS / 2))

    def dot(self, cx, cy, r, col=ACTIVE):
        lay = Image.new('RGBA', (int(r * 2 + 4) * SS, int(r * 2 + 4) * SS), (0, 0, 0, 0))
        ImageDraw.Draw(lay).ellipse([2 * SS, 2 * SS, (2 + 2 * r) * SS, (2 + 2 * r) * SS], fill=col + (255,))
        lay = lay.resize((lay.width // SS, lay.height // SS), Image.LANCZOS)
        self.paste(lay, (cx - r - 2, cy - r - 2))

    def icon_from(self, src, box, cx, cy, height, floor=55):
        """One of the HUD's own icons lifted off its panel: the dark glass keyed out by brightness."""
        part = src.crop(tuple(map(int, box))).convert('RGBA')
        lum = part.convert('L').point(lambda v: 0 if v < floor else min(255, (v - floor) * 3))
        part.putalpha(lum)
        s = height / part.height
        part = part.resize((max(1, int(part.width * s)), max(1, int(height))), Image.LANCZOS)
        self.paste(part, (cx - part.width / 2, cy - part.height / 2))
        return part.size

    def crop_from(self, src, box, to_xy, scale=1.0):
        part = src.crop(tuple(map(int, box)))
        if scale != 1.0:
            part = part.resize((int(part.width * scale), int(part.height * scale)), Image.LANCZOS)
        self.paste(part.convert('RGBA'), to_xy)
        return part.size

    def save(self, path, crops=None, preview=1920):
        os.makedirs(os.path.dirname(path), exist_ok=True)
        rgb = self.im.convert('RGB')
        rgb.save(path)
        if preview:
            rgb.resize((preview, int(preview * self.H / self.W)), Image.LANCZOS).save(path.replace('.png', '_1080.jpg'), quality=90)
        for name, box in (crops or {}).items():
            rgb.crop(tuple(map(int, box))).save(path.replace('.png', f'_crop_{name}.png'))
