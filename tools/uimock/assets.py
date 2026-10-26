"""Assets for the interactive mock-up: 1080p backgrounds with the game's page area cleaned, control sprites,
the maneuver bar pieces. Source: the 4K screenshots of 25 Oct 2026."""
import sys, os
from PIL import Image, ImageDraw, ImageFont

SRC, OUT, FONTS = sys.argv[1], sys.argv[2], sys.argv[3]
os.makedirs(OUT, exist_ok=True)
agency = lambda px: ImageFont.truetype(FONTS + "/AGENCYR.TTF", px)
WHITE = (236, 243, 255)

g = Image.open(SRC + "/options_general-003103.png").convert("RGB")
a = Image.open(SRC + "/options_audio-003125.png").convert("RGB")
c = Image.open(SRC + "/options_controls-003118.png").convert("RGB")
ig = Image.open(SRC + "/options_ingame-003243.png").convert("RGB")
hud = Image.open(SRC + "/hud_space-003200.png").convert("RGB")


def tile_fill(img, box, tile):
    w, h = box[2] - box[0], box[3] - box[1]
    y = 0
    while y < h:
        t = tile.crop((0, 0, w, min(tile.height, h - y)))
        img.paste(t, (box[0], box[1] + y))
        y += t.height


def text_c(d, xy, s, font, fill):
    w = d.textlength(s, font=font)
    d.text((xy[0] - w / 2, xy[1]), s, font=font, fill=fill)


# ---- main menu background: the page area cleaned with the Audio page's empty band, GENERAL unselected (white)
bg = g.copy()
tile_fill(bg, (1540, 300, 3300, 1850), a.crop((1540, 1300, 3300, 1850)))
d = ImageDraw.Draw(bg)
fill = bg.getpixel((760, 492))
d.rectangle((760, 450, 1232, 534), fill=fill)
text_c(d, (996, 448), "GENERAL", agency(78), WHITE)
bg.resize((1920, 1080), Image.LANCZOS).save(OUT + "/bg_main.jpg", quality=86)

# ---- in-game background: flat fill sampled from a clean spot, GENERAL unselected
bg2 = ig.copy()
d = ImageDraw.Draw(bg2)
d.rectangle((1540, 380, 3300, 1850), fill=ig.getpixel((2000, 500)))
fill = bg2.getpixel((840, 506))
d.rectangle((840, 464, 1320, 548), fill=fill)
text_c(d, (1080, 462), "GENERAL", agency(78), WHITE)
bg2.resize((1920, 1080), Image.LANCZOS).save(OUT + "/bg_ingame.jpg", quality=86)

# ---- the page button sprite (the AUDIO button with its caption wiped)
btn = g.crop((560, 1206 - 118, 1432, 1206 + 118)).copy()
d = ImageDraw.Draw(btn)
d.rectangle((200, 76, 672, 160), fill=btn.getpixel((200, 118)))
btn.resize((436, 118), Image.LANCZOS).save(OUT + "/btn_page.png")

# ---- the sub-tab box (SHIP CONTROLS with its caption wiped)
tab = c.crop((1655, 503, 2029, 586)).copy()
d = ImageDraw.Draw(tab)
d.rectangle((8, 8, 366, 75), fill=tab.getpixel((20, 40)))
tab.resize((187, 42), Image.LANCZOS).save(OUT + "/tab_box.png")

# ---- toggles, slider arrows, picker arrows
g.crop((3082 - 44, 1290 - 44, 3082 + 44, 1290 + 44)).resize((44, 44), Image.LANCZOS).save(OUT + "/toggle_on.png")
g.crop((3082 - 44, 1584 - 44, 3082 + 44, 1584 + 44)).resize((44, 44), Image.LANCZOS).save(OUT + "/toggle_off.png")
g.crop((2430, 999 - 36, 2500, 999 + 36)).resize((35, 36), Image.LANCZOS).save(OUT + "/slider_left.png")
g.crop((3070, 999 - 36, 3140, 999 + 36)).resize((35, 36), Image.LANCZOS).save(OUT + "/slider_right.png")
g.crop((3085, 570, 3140, 638)).resize((28, 34), Image.LANCZOS).save(OUT + "/picker_arrows.png")
# the DEFAULT / CANCEL buttons as sprites (captions wiped)
for name, cx in (("btn_wide", 1878),):
    b = g.crop((cx - 210, 1906 - 50, cx + 210, 1906 + 50)).copy()
    d = ImageDraw.Draw(b)
    d.rectangle((40, 24, 380, 76), fill=b.getpixel((40, 50)))
    b.resize((210, 50), Image.LANCZOS).save(OUT + "/" + name + ".png")

# ---- the maneuver bar pieces (1080p = half size)
bar = hud.crop((0, 0, 1400, 330))
bar.crop((0, 0, 962, 330)).resize((481, 165), Image.LANCZOS).save(OUT + "/bar_left.png")
bar.crop((958, 0, 966, 330)).resize((4, 165), Image.LANCZOS).save(OUT + "/bar_mid.png")
bar.crop((962, 0, 1400, 330)).resize((219, 165), Image.LANCZOS).save(OUT + "/bar_right.png")
print("ok", os.listdir(OUT))
