"""Freelancer text reader (user, 20 Oct 2026: the game renders a finite set of fonts in a uniform way - train our own
model instead of generic OCR, which misses the HUD's small digits). A CRNN line recognizer (CNN -> BiLSTM -> CTC) for
one line of game text: HUD lists (weapons, contacts), target labels ("Rogues - David Larsen" / "156-M"), counts,
banners, dealer screens.

Training data is synthetic, built from the game itself:
  - the fonts the game renders with (DATA\\Fonts: Agency FB regular / bold, Arial Unicode; DATA\\Fonts\\fonts.ini);
  - the game's own strings (tools\\dump-resources.ps1 -> research\\tools\\ocr\\data\\strings.tsv) plus HUD patterns
    (distances "156-M", counts, credits, numbered weapon rows);
  - backgrounds cut from the reference footage's 1 fps thumbnails (research\\videos\\catalogue\\thumbs);
  - degraded like the footage: downscaled, blurred, JPEG-compressed, noise.
Validation: hand-labelled crops of real footage (research\\tools\\ocr\\data\\real\\labels.tsv), never trained on.

Usage (the OCR environment, research\\tools\\ocr\\.venv):
  python flocr.py train [--steps 3000] [--lr 3e-4] [--scratch 1]
                                                            -> research\\tools\\ocr\\models\\flocr.pt (warm start from
                                                               it; early stop on the real validation half; keeps best)
  python flocr.py read <png> [<png> ...]                    -> "<file>\\t<text>\\t<confidence>" per line
  python flocr.py panel <png> [...]                         -> every text line of a HUD panel crop, read
  python flocr.py eval                                      -> accuracy on the real labelled crops
  python flocr.py sample 32                                 -> synthetic samples (research\\tools\\ocr\\data\\samples)
"""
import math
import os
import random
import re
import sys
import time

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

FL = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))  # FLvibed
WORK = os.path.join(FL, 'research', 'tools', 'ocr')
FONTS = os.path.join(FL, 'Install', 'Freelancer HD Edition', 'DATA', 'Fonts')
THUMBS = os.path.join(FL, 'research', 'videos', 'catalogue', 'thumbs')
MODEL = os.path.join(WORK, 'models', 'flocr.pt')

CHARSET = ''.join(chr(c) for c in range(32, 127))  # printable ASCII; index 0 = CTC blank
H = 32       # model input height
MAXW = 512   # model input width (padded)


# ------------------------------------------------------------------ corpus
_PUNCT = {'’': "'", '‘': "'", '“': '"', '”': '"', '–': '-', '—': '-', '…': '...'}


def normalise(t):
    """Game text -> the model's character set: curly quotes / dashes to ASCII, accents dropped (the game writes
    "Death’s Hand": 2,877 strings lost their apostrophe before this, so "Death's" was read as "Death s")."""
    import unicodedata
    t = ''.join(_PUNCT.get(ch, ch) for ch in t)
    t = unicodedata.normalize('NFKD', t)
    return ''.join(ch for ch in t if ch in CHARSET)


def load_corpus():
    lines = set()
    p = os.path.join(WORK, 'data', 'strings.tsv')
    if os.path.exists(p):
        with open(p, encoding='utf-8', errors='ignore') as f:
            for row in f:
                parts = row.rstrip('\n').split('\t')
                if len(parts) < 3 or parts[1] != 'str':
                    continue
                t = normalise(re.sub(r'\s+', ' ', parts[2].replace('\\n', ' ')).strip())
                if not t:
                    continue
                if len(t) <= 40:
                    lines.add(t)
                else:  # long text: word windows of a HUD line's length
                    words = t.split(' ')
                    for i in range(0, len(words), 5):
                        w = ' '.join(words[i:i + random.randint(3, 7)])
                        if 2 <= len(w) <= 40:
                            lines.add(w)
    return sorted(lines)


DATA = os.path.join(FL, 'Install', 'Freelancer HD Edition', 'DATA')
_lex = {}


def strings():
    if 'str' not in _lex:
        s = {}
        p = os.path.join(WORK, 'data', 'strings.tsv')
        with open(p, encoding='utf-8', errors='ignore') as f:
            for row in f:
                c = row.rstrip('\n').split('\t', 2)
                if len(c) == 3 and c[1] == 'str':
                    t = normalise(c[2].strip()).strip()
                    if t:
                        s[int(c[0])] = t
        _lex['str'] = s
    return _lex['str']


def ini_values(path, key):
    with open(path, encoding='latin-1') as f:
        for row in f:
            m = re.match(r'\s*' + key + r'\s*=\s*(.+)', row.split(';')[0], re.I)
            if m:
                yield m.group(1).strip()


def game_lexicon():
    """What the HUD can show, from the game data (20 Oct 2026): contact rows are '<distance> <faction short name> -
    <first> <last>' composed from faction_prop.ini name ranges + initialworld.ini short names; other rows show
    equipment and space-object names."""
    if 'lex' in _lex:
        return _lex['lex']
    s = strings()
    shorts, short_of = set(), {}
    nick = None
    for v in ini_values(os.path.join(DATA, 'initialworld.ini'), r'(?:nickname|ids_short_name)'):
        if v.isdigit():
            if nick and int(v) in s:
                short_of[nick] = s[int(v)]
                shorts.add(s[int(v)])
        else:
            nick = v.lower()
    names = {'first': set(), 'last': set()}
    fac_names = []   # (short, firsts, lasts)
    aff, cur = None, None
    with open(os.path.join(DATA, 'MISSIONS', 'faction_prop.ini'), encoding='latin-1') as f:
        for row in f:
            row = row.split(';')[0].strip()
            m = re.match(r'(\w+)\s*=\s*(.+)', row)
            if row.lower().startswith('[factionprops]'):
                cur = {'first': [], 'last': []}
                fac_names.append(cur)
            if not m or cur is None:
                continue
            k, v = m.group(1).lower(), m.group(2)
            if k == 'affiliation':
                cur['short'] = short_of.get(v.strip().lower(), '')
            elif k in ('firstname_male', 'firstname_female', 'lastname'):
                a, b = [int(x) for x in v.split(',')[:2]]
                lst = [s[i] for i in range(a, b + 1) if i in s]
                cur['last' if k == 'lastname' else 'first'] += lst
                names['last' if k == 'lastname' else 'first'].update(lst)
    items = set()
    for sub in ('EQUIPMENT', 'SHIPS'):
        for fn in os.listdir(os.path.join(DATA, sub)):
            if fn.lower().endswith('.ini'):
                for v in ini_values(os.path.join(DATA, sub, fn), 'ids_name'):
                    if v.isdigit() and int(v) in s and len(s[int(v)]) <= 40:
                        items.add(s[int(v)])
    sysdir = os.path.join(DATA, 'UNIVERSE', 'SYSTEMS')
    for d in os.listdir(sysdir):
        p = os.path.join(sysdir, d, d + '.ini')
        if os.path.exists(p):
            for v in ini_values(p, 'ids_name'):
                if v.isdigit() and int(v) in s and len(s[int(v)]) <= 40:
                    items.add(s[int(v)])
    # story contacts: mission ships' names (Juni, King, Razor-One, Defender ...) with their faction's short name
    # (docs\kb\generated\msn_objects.tsv from tools\kbgen.ps1)
    mission = []
    mp = os.path.join(FL, 'docs', 'kb', 'generated', 'msn_objects.tsv')
    if os.path.exists(mp):
        with open(mp, encoding='utf-8', errors='ignore') as f:
            for row in f:
                c = row.rstrip('\n').split('\t')
                if len(c) > 7 and c[1] in ('ship', 'solar'):   # solars too: "Shield Generator", "Battleship KNF Musashi"
                    nm = ' '.join(s[int(i)] for i in re.findall(r'\d{5,}', c[7]) if int(i) in s)
                    if nm:
                        mission.append((short_of.get(c[4].lower(), ''), nm))
    # equipment that shows a count in the weapon list: launchers whose munition requires ammo, mine and countermeasure
    # droppers (weapon_equip.ini)
    counted, ammo, guns = set(), set(), []
    sec = {}
    text = ''
    for fn in ('weapon_equip.ini', 'misc_equip.ini'):   # countermeasure droppers live in misc_equip
        with open(os.path.join(DATA, 'EQUIPMENT', fn), encoding='latin-1') as f:
            text += f.read() + '\n'
    for row in text.split('\n') + ['[end]']:
        row = row.split(';')[0].strip()
        if row.startswith('['):
            if sec.get('_') == 'munition' and sec.get('requires_ammo', '').lower() == 'true':
                ammo.add(sec.get('nickname', '').lower())
            elif sec.get('_') in ('gun', 'minedropper', 'countermeasuredropper'):
                guns.append(sec)
            sec = {'_': row.strip('[]').lower()}
        elif '=' in row:
            k, v = [x.strip() for x in row.split('=', 1)]
            sec.setdefault(k.lower(), v)
    for g in guns:
        nm = s.get(int(g['ids_name'])) if g.get('ids_name', '').isdigit() else None
        if nm and (g['_'] != 'gun' or g.get('projectile_archetype', '').lower() in ammo):
            counted.add(nm)
    items |= {t for t in s.values() if 'Waypoint' in t and len(t) <= 30}   # contact list: "Mission Waypoint"
    # gates / holes show as "<destination system> Jump Gate|Hole"; trade-lane routes as "<system> -> <system>"
    for v in ini_values(os.path.join(DATA, 'UNIVERSE', 'universe.ini'), 'strid_name'):
        if v.isdigit() and int(v) in s:
            items |= {s[int(v)], s[int(v)] + ' Jump Gate', s[int(v)] + ' Jump Hole'}
    ships = {n for _, n in mission} | {it for it in items if len(it.split(' ')) <= 3}
    _lex['lex'] = {'shorts': sorted(shorts), 'first': sorted(names['first']), 'last': sorted(names['last']),
                   'counted': counted, 'ships': sorted(ships),
                   'factions': [f for f in fac_names if f.get('short') and f['first'] and f['last']],
                   'items': sorted(items), 'mission': sorted(set(mission)),
                   'words': sorted(set(names['first']) | set(names['last']) |
                                   {w for _, n in mission for w in n.split(' ')} |
                                   {w for it in items for w in it.split(' ') if len(w) > 2})}
    _lex['lex']['wordset'] = set(_lex['lex']['words'])
    return _lex['lex']


def distance():
    r = random.random()
    if r < 0.45:
        return str(random.randint(1, 999))
    if r < 0.8:
        return f'{random.randint(1, 99)}.{random.randint(0, 9)}K'
    return f'{random.randint(0, 9999)}-M'


def hud_line(corpus):
    """A line the way the HUD shows text: game strings, numbered rows, counts, distances, credits."""
    r = random.random()
    lex = game_lexicon()
    if r < 0.22 and lex['factions']:   # contact row, composed the way the game names NPCs
        fa = random.choice(lex['factions'])
        return f"{distance()} {fa['short']} - {random.choice(fa['first'])} {random.choice(fa['last'])}"
    if r < 0.30 and lex['mission']:    # story contact: "190 Juni", "556 Kress - Razor-One", "159 Navy - Defender"
        sh, nm = random.choice(lex['mission'])
        return f'{distance()} {sh} - {nm}' if sh and random.random() < 0.6 else f'{distance()} {nm}'
    if r < 0.38 and lex['items']:      # contact row of an object, or a weapon / cargo row
        it = random.choice(lex['items'])
        return f'{distance()} {it}' if random.random() < 0.5 else f'{random.randint(1, 10)} {it}'
    r = random.random()
    corpus = corpus or lex['items'] or ['Freelancer']
    word = random.choice(corpus)
    if r < 0.30:
        return word
    if r < 0.40:
        return word.upper()
    if r < 0.52:
        return f'{random.randint(1, 10)} {word}'                       # weapon / list row
    if r < 0.62:
        return f'{random.randint(1, 10)} {word} {random.randint(0, 99)}'  # row with an ammo count
    if r < 0.72:
        return f'{random.randint(0, 9999)}-M' if random.random() < 0.8 else f'{random.randint(1, 99)}.{random.randint(0, 9)}K'
    if r < 0.80:
        return str(random.randint(0, 99999))
    if r < 0.86:
        return f'CREDITS ${random.randint(0, 999999):,}'
    if r < 0.93:
        a, b = random.choice(corpus), random.choice(corpus)
        return f'{a[:18]} - {b[:18]}'                                   # "Rogues - David Larsen"
    return f'{random.randint(0, 2000)} {word}'                          # contact list row: distance + name


# ------------------------------------------------------------------ rendering
_fonts = {}
_bgs = []


def font(name, px):
    key = (name, px)
    if key not in _fonts:
        _fonts[key] = ImageFont.truetype(os.path.join(FONTS, name), px)
    return _fonts[key]


def backgrounds():
    if not _bgs and os.path.isdir(THUMBS):
        vids = [os.path.join(THUMBS, d) for d in os.listdir(THUMBS)]
        for d in random.sample(vids, min(len(vids), 12)):
            files = os.listdir(d)
            for f in random.sample(files, min(len(files), 40)):
                _bgs.append(os.path.join(d, f))
    return _bgs


# HUD colours: list green, label white, hostile red, neutral / waypoint yellow, info cyan, mission purple
COLOURS = [(120, 230, 120), (235, 235, 235), (230, 70, 60), (235, 220, 90), (130, 200, 235), (190, 120, 230),
           (90, 200, 90)]


def text_layer(text, f, col, gap_cols, squeeze, outline):
    """One line of HUD text as an RGBA strip: optional column gaps (number | name | ammo count), condensed glyphs (the
    HUD draws Agency FB narrower than the font's default width: 5 / 6 read as 3 / 9 until the synthetic text was
    condensed, 20 Oct 2026) and the dark outline of the HUD's text."""
    m = re.match(r'^(\S+) (.+?)(?: (\d{1,3}))?$', text)
    numbered = m and gap_cols and _NUM.match(m.group(1))   # only list rows have columns: "<n> <name> [<count>]"
    parts = ([m.group(1), m.group(2)] + ([m.group(3)] if m.group(3) else [])) if numbered else [text]
    px = f.size
    gaps = [int(px * random.uniform(0.5, 2.2)), int(px * random.uniform(1.5, 9.0))]
    boxes = [f.getbbox(p) for p in parts]
    widths = [bx[2] - bx[0] for bx in boxes]
    _, t, _, b = f.getbbox(text)
    sw = 1 if outline else 0
    W = sum(widths) + sum(gaps[:len(parts) - 1]) + 4 + 2 * sw
    lay = Image.new('RGBA', (max(2, W), max(2, (b - t) + 4 + 2 * sw)), (0, 0, 0, 0))
    d = ImageDraw.Draw(lay)
    x = 2 + sw
    for i, p in enumerate(parts):
        d.text((x - boxes[i][0], 2 + sw - t), p, font=f, fill=col + (255,), stroke_width=sw,
               stroke_fill=(0, 0, 0, random.randint(120, 230)))
        x += widths[i] + (gaps[i] if i < len(gaps) else 0)
    return lay.resize((max(2, int(lay.width * squeeze)), lay.height), Image.BILINEAR)


def render(text, corpus=None):
    """One synthetic line: game font at a HUD size over a footage background, cropped the way real HUD rows are (rows
    above / below cut into the crop, the row off-centre), degraded like YouTube footage."""
    fname = random.choices(['AGENCYR.TTF', 'AGENCYR_CR.ttf', 'AGENCYB.TTF', 'ARIALUNI.TTF'], [0.45, 0.2, 0.2, 0.15])[0]
    px = random.randint(12, 44)
    f = font(fname, px)
    agency = 'AGENCY' in fname.upper()
    squeeze = random.uniform(0.62, 0.95) if agency and random.random() < 0.8 else random.uniform(0.9, 1.05)
    outline = random.random() < 0.7
    cols = random.random() < 0.7
    col = random.choice(COLOURS)
    col = tuple(max(0, min(255, c + random.randint(-25, 25))) for c in col)
    main = text_layer(text, f, col, cols, squeeze, outline)
    lh = main.height
    # real HUD row crops (vidstudy / grid_lines): pitch ~1.0-1.3 row heights, crop ~1.15-1.6, row off-centre
    pitch = int(lh * random.uniform(1.0, 1.3))
    crop_h = int(lh * random.uniform(1.15, 1.6))
    y_main = (crop_h - lh) // 2 + int(lh * random.uniform(-0.2, 0.2))
    pad_x = random.randint(2, 14)
    W = main.width + 2 * pad_x + random.randint(0, 3 * px)
    bgs = backgrounds()
    if bgs and random.random() < 0.85:
        bg = Image.open(random.choice(bgs)).convert('RGB')
        bw, bh = bg.size
        scale = max(W / bw, crop_h / bh, random.uniform(0.6, 2.5))
        bg = bg.resize((max(W, int(bw * scale)), max(crop_h, int(bh * scale))))
        x0, y0 = random.randint(0, bg.width - W), random.randint(0, bg.height - crop_h)
        img = bg.crop((x0, y0, x0 + W, y0 + crop_h))
        if random.random() < 0.6:  # the HUD panels' dark translucent fill
            dark = Image.new('RGB', img.size, (random.randint(5, 40), random.randint(10, 50), random.randint(30, 80)))
            img = Image.blend(img, dark, random.uniform(0.3, 0.85))
    else:
        img = Image.new('RGB', (W, crop_h), tuple(random.randint(0, 60) for _ in range(3)))
    img = img.convert('RGBA')
    # neighbouring rows: real HUD lines in the same font and column layout; the crop cuts them (not part of the label)
    for sign in (-1, 1):
        if random.random() < 0.75:
            other = re.sub(' +', ' ', ''.join(ch for ch in hud_line(corpus or []) if ch in CHARSET)).strip()[:48]
            lay = text_layer(other or 'Navy', f, random.choice(COLOURS) if random.random() < 0.5 else col, cols, squeeze,
                             outline)
            y = y_main + sign * pitch
            if -lay.height < y < crop_h:
                tmp = Image.new('RGBA', img.size, (0, 0, 0, 0))
                tmp.paste(lay, (pad_x + random.randint(-3, 3), y))
                img = Image.alpha_composite(img, tmp)
    tmp = Image.new('RGBA', img.size, (0, 0, 0, 0))
    tmp.paste(main, (pad_x, y_main))
    img = Image.alpha_composite(img, tmp).convert('RGB')
    # degrade: the footage is 1080p / 4K video, often downscaled, H.264-compressed
    if random.random() < 0.7:
        s = max(random.uniform(0.35, 0.9), 11.0 / px)  # text stays >= ~11 px (real HUD rows ~20 px in the crops)
        img = img.resize((max(2, int(img.width * s)), max(2, int(img.height * s))), Image.BILINEAR)
    if random.random() < 0.4:
        img = img.filter(ImageFilter.GaussianBlur(random.uniform(0.2, 1.0)))
    if random.random() < 0.8:
        from io import BytesIO
        buf = BytesIO()
        img.save(buf, 'JPEG', quality=random.randint(15, 80))
        img = Image.open(BytesIO(buf.getvalue())).convert('RGB')
    return img


def to_tensor_image(img):
    """RGB / gray image -> float array (H, w) in 0..1, height H, width scaled (max MAXW)."""
    # the brightest channel, not luminance: red (hostile) text keeps its contrast (in luminance it is ~30 % as bright -
    # the first model read red contact rows as garbage, 20 Oct 2026)
    rgb = img.convert('RGB')
    w = max(8, min(MAXW, int(round(rgb.width * H / max(1, rgb.height)))))
    rgb = rgb.resize((w, H), Image.BILINEAR)
    a = np.asarray(rgb, dtype=np.float32).max(axis=2) / 255.0
    # polarity: the HUD is light text on dark; normalise so text is bright
    if a.mean() > 0.6:
        a = 1.0 - a
    return a


# ------------------------------------------------------------------ model
def build_model(torch, nn):
    class CRNN(nn.Module):
        def __init__(self, n_classes):
            super().__init__()
            def blk(i, o, pool):
                layers = [nn.Conv2d(i, o, 3, padding=1), nn.BatchNorm2d(o), nn.ReLU(inplace=True)]
                if pool:
                    layers.append(nn.MaxPool2d(pool))
                return layers
            self.cnn = nn.Sequential(*blk(1, 64, (2, 2)), *blk(64, 128, (2, 2)), *blk(128, 256, None),
                                     *blk(256, 256, (2, 1)), *blk(256, 384, None), *blk(384, 384, (2, 1)),
                                     *blk(384, 384, (2, 1)))  # 32 -> 1 in height, width / 4
            self.rnn = nn.LSTM(384, 256, num_layers=2, bidirectional=True, batch_first=True, dropout=0.1)
            self.fc = nn.Linear(512, n_classes)

        def forward(self, x):            # x: (B, 1, H, W)
            f = self.cnn(x)              # (B, C, 1, W/4)
            f = f.squeeze(2).permute(0, 2, 1)
            f, _ = self.rnn(f)
            return self.fc(f)            # (B, T, classes)
    return CRNN(len(CHARSET) + 1)


def encode(text):
    return [CHARSET.index(c) + 1 for c in text if c in CHARSET]


def decode(logits_row):
    """Greedy CTC decode of (T, classes) log-probs -> text, mean max probability."""
    import torch
    p = logits_row.softmax(-1)
    best = p.argmax(-1).tolist()
    conf = p.max(-1).values
    out, prev = [], 0
    for k in best:
        if k != prev and k != 0:
            out.append(CHARSET[k - 1])
        prev = k
    return re.sub(' +', ' ', ''.join(out)).strip(), float(conf.mean())  # spacing is not read: columns collapse to one space


# ------------------------------------------------------------------ commands
def batch(torch, items):
    w = max(a.shape[1] for a, _ in items)
    x = torch.zeros(len(items), 1, H, w)
    for i, (a, _) in enumerate(items):
        x[i, 0, :, :a.shape[1]] = torch.from_numpy(a)
    return x


def make_sample(corpus):
    while True:
        t = hud_line(corpus)
        t = re.sub(' +', ' ', ''.join(ch for ch in t if ch in CHARSET)).strip()[:48]
        if t:
            break
    return to_tensor_image(render(t, corpus)), t


def keep_list(b):
    return b


from torch.utils.data import IterableDataset as _IterableDataset  # noqa: E402 (module level for the workers)


PSEUDO = os.path.join(WORK, 'data', 'pseudo')


def real_aug(img):
    """A pseudo-labelled real crop, varied like the footage varies: scale, blur, recompression, side padding."""
    s = random.uniform(0.6, 1.25)
    img = img.resize((max(8, int(img.width * s)), max(8, int(img.height * s))), Image.BILINEAR)
    if random.random() < 0.3:
        img = img.filter(ImageFilter.GaussianBlur(random.uniform(0.2, 0.8)))
    if random.random() < 0.6:
        from io import BytesIO
        buf = BytesIO()
        img.save(buf, 'JPEG', quality=random.randint(30, 90))
        img = Image.open(BytesIO(buf.getvalue())).convert('RGB')
    return img


class Synth(_IterableDataset):
    # module level: Windows starts DataLoader workers by pickling (no local classes / lambdas)
    def __init__(self, real_share=0.0):
        self.real_share = real_share

    def __iter__(self):
        c = load_corpus()
        real = []
        if self.real_share > 0 and os.path.exists(os.path.join(PSEUDO, 'labels.tsv')):
            with open(os.path.join(PSEUDO, 'labels.tsv'), encoding='utf-8') as f:
                real = [r.rstrip('\n').split('\t') for r in f if '\t' in r]
        while True:
            if real and random.random() < self.real_share:
                name, text = random.choice(real)
                yield to_tensor_image(real_aug(Image.open(os.path.join(PSEUDO, name)).convert('RGB'))), text
            else:
                yield make_sample(c)


def worker_init(_):
    random.seed(os.getpid() + int(time.time()))


# real labelled rows: 'real2' = the same rows cut by row_bands from their source frames (the production cropper;
# research\tools\ocr\make_real2.py); 'real' = the first fixed 30 px windows (neighbour rows, clipped tops)
REAL_SET = os.environ.get('FLOCR_REAL', 'real2')


def real_split(part):
    """The hand-labelled real crops: 'val' (even rows: early stopping / model choice) or 'test' (odd rows: the
    reported number, never used to choose) or 'all'."""
    d = os.path.join(WORK, 'data', REAL_SET)
    out = []
    with open(os.path.join(d, 'labels.tsv'), encoding='utf-8') as f:
        rows = [(r.rstrip('\n').split('\t') + [''])[:2] for r in f if r.strip() and not r.startswith('#')]
    for i, (path, truth) in enumerate(rows):
        if part == 'all' or (part == 'val') == (i % 2 == 0):
            out.append((to_tensor_image(Image.open(os.path.join(d, path))), truth, path))
    return out


def score(torch, model, data, dev, snapped=False):
    model.eval()
    ok, c = 0, 0.0
    with torch.no_grad():
        for a, truth, _ in data:
            lg = model(batch(torch, [(a, '')]).to(dev))[0].float().cpu()
            t = decode(lg)[0]
            if snapped:
                t = snap_ctc(torch, lg, t)[0]
            ok += t == truth
            c += cer(t, truth)
    model.train()
    return ok / max(1, len(data)), c / max(1, len(data))


def train(steps=3000, bs=128, init=True, lr=3e-4, every=250, patience=4, real_share=0.0):
    """Efficient training (user, 20 Oct 2026: run 2's real-crop accuracy stopped improving after ~10k of 40k steps):
    warm start from the current model, short cosine schedule, validation on real crops every `every` steps, keep the
    best checkpoint, stop when `patience` checks in a row bring no improvement."""
    import torch
    import torch.nn as nn
    from torch.utils.data import DataLoader, IterableDataset
    corpus = load_corpus()
    lex = game_lexicon()
    print(f"corpus {len(corpus)} lines, {len(lex['factions'])} factions with names, {len(lex['items'])} item names",
          flush=True)
    val = real_split('val')
    print(f'pseudo-real share {real_share}', flush=True)
    dl = DataLoader(Synth(real_share), batch_size=bs, num_workers=8, collate_fn=keep_list, worker_init_fn=worker_init,
                    persistent_workers=True, prefetch_factor=4)
    dev = 'cuda'
    model = build_model(torch, nn).to(dev)
    if init and os.path.exists(MODEL):
        model.load_state_dict(torch.load(MODEL, map_location=dev))
        print('warm start from', MODEL, flush=True)
    opt = torch.optim.AdamW(model.parameters(), lr=lr, weight_decay=1e-4)
    sched = torch.optim.lr_scheduler.OneCycleLR(opt, max_lr=lr, total_steps=steps, pct_start=0.05)
    ctc = nn.CTCLoss(blank=0, zero_infinity=True)
    best, best_step, waits = score(torch, model, val, dev), 0, 0
    print(f'step 0 val exact {best[0]:.3f} cer {best[1]:.3f}', flush=True)
    best_path = MODEL + '.best'
    torch.save(model.state_dict(), best_path)
    t0 = time.time()
    it = iter(dl)
    for step in range(1, steps + 1):
        items = next(it)
        x = batch(torch, items).to(dev)
        targets = [torch.tensor(encode(t)) for _, t in items]
        tl = torch.tensor([len(t) for t in targets])
        y = torch.cat(targets)
        logits = model(x).log_softmax(-1)               # (B, T, C)
        il = torch.full((len(items),), logits.shape[1], dtype=torch.long)
        loss = ctc(logits.permute(1, 0, 2), y, il, tl)
        opt.zero_grad()
        loss.backward()
        torch.nn.utils.clip_grad_norm_(model.parameters(), 5.0)
        opt.step()
        sched.step()
        if step % every == 0 or step == steps:
            v = score(torch, model, val, dev)
            better = (v[0], -v[1]) > (best[0], -best[1])
            print(f'step {step} loss {loss.item():.3f} val exact {v[0]:.3f} cer {v[1]:.3f}'
                  f'{" *" if better else ""} {time.time() - t0:.0f}s', flush=True)
            if better:
                best, best_step, waits = v, step, 0
                torch.save(model.state_dict(), best_path)
            else:
                waits += 1
                if waits >= patience:
                    print(f'early stop: no val gain for {patience} checks', flush=True)
                    break
    os.replace(best_path, MODEL)
    print(f'saved {MODEL} (best val at step {best_step}: exact {best[0]:.3f} cer {best[1]:.3f})', flush=True)


def load():
    import torch
    import torch.nn as nn
    m = build_model(torch, nn)
    m.load_state_dict(torch.load(MODEL, map_location='cpu'))
    m.eval()
    torch.backends.cudnn.benchmark = True   # fixed-shape scan batches: fastest kernels picked once
    return torch, m.to('cuda' if torch.cuda.is_available() else 'cpu')


def read(paths):
    torch, m = load()
    dev = next(m.parameters()).device
    for p in paths:
        a = to_tensor_image(Image.open(p))
        with torch.no_grad():
            lg = m(batch(torch, [(a, '')]).to(dev))[0].float()
        t, c = decode(lg)
        print(f'{p}\t{t}\t{c:.2f}')


def find_lines(img, min_h=8):
    """Text lines of a HUD panel crop: rows whose bright-ink count stands out (the panels are light text on a dark,
    translucent fill), merged into bands; returns (top, bottom) pixel rows with a little margin. Fixed row positions
    cut the scrolling contact list's lines in half (20 Oct 2026)."""
    # text = dense left-right edges in the brightest channel (green / red / yellow text is bright in one channel only);
    # panel fills, borders and space backgrounds are smooth along a row. Row score = strong horizontal steps per row,
    # a row is text when well above the panel's typical row (brightness thresholds missed dim text under a bright
    # border, 20 Oct 2026).
    g = np.asarray(img.convert('RGB'), dtype=np.float32).max(axis=2)
    dx = np.abs(np.diff(g, axis=1))
    ink = (dx > 18).sum(axis=1).astype(np.float32)
    base = np.median(ink)
    on = ink > base + max(3.0, 0.2 * (np.percentile(ink, 95) - base))
    bands, start = [], None
    for y, v in enumerate(on):
        if v and start is None:
            start = y
        elif not v and start is not None:
            if y - start >= min_h // 2:
                bands.append([start, y])
            start = None
    if start is not None and len(on) - start >= min_h // 2:
        bands.append([start, len(on)])
    merged = []
    for b in bands:  # a gap of 1-2 rows inside a line (thin strokes) is the same line
        if merged and b[0] - merged[-1][1] <= 2:
            merged[-1][1] = b[1]
        else:
            merged.append(b)
    h = g.shape[0]
    return [(max(0, t - 3), min(h, b + 3)) for t, b in merged if b - t >= min_h]


def ink_profile(img):
    """Per pixel row of a panel crop: the count of text pixels (colour distance from the row's background; columns
    inked in most rows - scroll bar, panel frame - removed)."""
    a = np.asarray(img.convert('RGB'), dtype=np.float32)
    bg = np.median(a, axis=1, keepdims=True)
    dist = np.sqrt(((a - bg) ** 2).sum(axis=2))
    px = dist > max(40.0, float(np.percentile(dist, 97)) * 0.4)
    px[:, px.mean(axis=0) > 0.45] = False
    return px.sum(axis=1).astype(np.float32)


def snap_row(ink, y_guess, pitch=25.5, search=None):
    """A HUD list row (fixed pitch) cut so it holds its whole text and none of its neighbours: the window of text
    height (0.82 pitch) near y_guess with the most ink inside and the least in the 2 px just outside it. Returns
    (top, bottom) with a 1 px margin. The fixed-offset 30 px windows clipped glyph tops and carried half the next row
    (20 Oct 2026)."""
    th = int(round(0.82 * pitch))
    search = int(round(pitch * 0.4)) if search is None else search
    best, by = None, int(y_guess)
    for y in range(int(y_guess) - search, int(y_guess) + search + 1):
        if y < 2 or y + th + 2 > len(ink):
            continue
        s = ink[y:y + th].sum() - 2.0 * (ink[y - 2:y].sum() + ink[y + th:y + th + 2].sum())
        if best is None or s > best:
            best, by = s, y
    return max(0, by - 1), min(len(ink), by + th + 1)


def row_bands(img, min_h=7):
    """The text rows of a HUD panel crop as (top, bottom) bands, each with a small margin. HUD list rows are separated
    by a few pixels of panel (the fixed-pitch 30 px windows used before mixed the top of a row with half of the next
    and clipped glyph tops: 19 of the last 26 real errors were the distance column, 20 Oct 2026). Ink = pixels well
    above their row's background in the brightest channel; bands split at the ink valleys between rows."""
    a = np.asarray(img.convert('RGB'), dtype=np.float32)
    # ink = colour distance from the row's own background (green text on the blue highlighted row has little
    # brightness contrast but a large colour one)
    bg = np.median(a, axis=1, keepdims=True)
    dist = np.sqrt(((a - bg) ** 2).sum(axis=2))
    px = dist > max(40.0, float(np.percentile(dist, 97)) * 0.4)
    # vertical structures (scroll bar, panel frame) are inked in most rows: not text
    px[:, px.mean(axis=0) > 0.45] = False
    ink = px.sum(axis=1).astype(np.float32)
    if ink.max() <= 0:
        return []
    on = ink > max(1.0, 0.05 * ink.max())
    bands, y = [], 0
    while y < len(on):
        if on[y]:
            t = y
            while y < len(on) and on[y]:
                y += 1
            bands.append([t, y])
        y += 1
    hs = sorted(b - t for t, b in bands if b - t >= min_h)
    typical = hs[len(hs) // 2] if hs else 0
    out = []
    for t, b in bands:   # a band of n touching rows: split at the n-1 deepest valleys near the expected cuts
        n = int(round((b - t) / typical)) if typical else 1
        if n >= 2:
            cuts = [t]
            for k in range(1, n):
                c = t + (b - t) * k // n
                lo, hi = max(t + 1, c - typical // 3), min(b - 1, c + typical // 3)
                cuts.append(lo + int(np.argmin(ink[lo:hi])) if hi > lo else c)
            cuts.append(b)
            out += [[cuts[i], cuts[i + 1]] for i in range(n)]
        else:
            out.append([t, b])
    res = []
    for t, b in out:
        if b - t < min_h:
            continue
        h = b - t
        while t > 0 and ink[t - 1] > 0 and (b - t) < 1.35 * h:    # grow over thin glyph tops / descender tips
            t -= 1
        while b < len(ink) and ink[b] > 0 and (b - t) < 1.5 * h:
            b += 1
        m = max(2, int(round(0.12 * (b - t))))
        res.append((max(0, t - m), min(len(on), b + m)))
    return res


def grid_lines(img, pitch=25.5, line_h=22):
    """HUD lists (weapons, contacts) put their lines on a fixed pitch (25.5 px at 1080p; scale with the frame height):
    the scroll offset is the grid phase that gathers the most text edges; every grid row with text is a line. Robust
    where a dim line (green on the fill) falls below find_lines' threshold."""
    g = np.asarray(img.convert('RGB'), dtype=np.float32).max(axis=2)
    ink = (np.abs(np.diff(g, axis=1)) > 18).sum(axis=1).astype(np.float32)
    h = len(ink)
    best, best_phase = -1.0, 0.0
    for ph in np.arange(0, pitch, 0.5):
        s = 0.0
        y = ph
        while y + line_h <= h:
            s += ink[int(y):int(y + line_h)].sum() - ink[int(y + line_h):int(min(h, y + pitch))].sum() * 2
            y += pitch
        if s > best:
            best, best_phase = s, ph
    cells, y = [], best_phase
    while y + line_h <= h + 2:
        t, b = int(y), int(min(h, y + line_h))
        cells.append((t, b, float(ink[t:b].mean())))
        y += pitch
    if not cells:
        return []
    base, top = float(np.percentile(ink, 10)), max(c[2] for c in cells)  # the gaps between lines: text can fill most rows
    # a row is text when its edges stand out against the panel AND reach a fifth of the strongest row (dim green
    # lines on the fill: half the edges of white ones)
    return [(max(0, t - 2), min(h, b + 2)) for t, b, m in cells if m > base + 0.25 * (top - base)]


def panel_rows(img, pitch=25.5):
    """The rows of a HUD list panel crop: grid_lines finds the rows on the list's fixed pitch (25.5 px at 1080p:
    scale with the frame height), snap_row cuts each one around its whole text. The production cropper: on the
    labelled rows it took the same model from 77.6 % to 99.1 % exact (20 Oct 2026)."""
    ink = ink_profile(img)
    rows, seen = [], set()
    for t, b in grid_lines(img, pitch=pitch, line_h=pitch * 22 / 25.5):
        top, bot = snap_row(ink, t + 2, pitch)
        # a row cut by the panel box (its window at the edge with ink on the edge line) is partial: not read
        if (top <= 1 and ink[0] > 0) or (bot >= len(ink) - 1 and ink[-1] > 0):
            continue
        if top not in seen:
            seen.add(top)
            rows.append((top, bot))
    return rows


def vote(series):
    """Bulk video: the same HUD row read in consecutive frames (a list of snapped reads, '' / None for missing) ->
    the majority read and its share. HUD rows persist for many frames while a misread is usually a one-frame event
    (Mk III read as Mk II once in the fresh check, 20 Oct 2026); distances change, so vote the name part and take the
    distance of the frame of interest."""
    from collections import Counter
    names = [re.sub(r'^\S+ ', '', r) if r and _NUM.match(r.split(' ')[0]) else r for r in series if r]
    if not names:
        return None, 0.0
    top, n = Counter(names).most_common(1)[0]
    return top, n / len(names)


def read_panel(paths, pitch=25.5):
    """Each panel image: every list row (panel_rows), read and snapped (snap_ctc):
    '<file>\\t<row>\\t<top>\\t<text>\\t<valid 1|0>\\t<raw text>\\t<confidence>'. valid = the row matched game data;
    0 = not a text row (HUD icons, a partial row, a non-HUD frame): drop it in bulk use. Fresh frames, checked by eye
    (20 Oct 2026): 96-97 % of valid rows exact, names ~99 %; the distance number is the weak part (2 / 101)."""
    torch, m = load()
    dev = next(m.parameters()).device
    for p in paths:
        img = Image.open(p).convert('RGB')
        for i, (t, b) in enumerate(panel_rows(img, pitch)):
            a = to_tensor_image(img.crop((0, t, img.width, b)))
            with torch.no_grad():
                lg = m(batch(torch, [(a, '')]).to(dev))[0].float().cpu()
            txt, c = decode(lg)
            s, ok = snap_ctc(torch, lg, txt)
            print(f'{p}\t{i}\t{t}\t{s}\t{int(ok)}\t{txt}\t{c:.2f}')


_NUM = re.compile(r'^(?:\d[\d.,]*[KkM]?|\d+-M)$')


def _closest(word, choices, cutoff):
    import difflib
    m = difflib.get_close_matches(word, choices, n=1, cutoff=cutoff)
    return m[0] if m else None


def snap(text):
    return snap_info(text)[0]


# 1080p HUD list panels (x0, y0, x1, y1) and their row pitch: the weapon list (bottom right) and the contact list
# (bottom left). The panels grow UPWARD with the list and their last row sits at y ~1033 (a 9-slot list in M07.5:
# the first box, 760-1010, cut slots 8-9 off, 20 Oct 2026): the boxes cover ~12 rows; partial rows at the box edges
# are dropped by panel_rows.
PANELS = {'weapons': (1570, 600, 1900, 1075), 'contacts': (60, 640, 390, 1075)}
PITCH = 25.5


def scan_many(jobs, panel='weapons', fps=2.0, batch_rows=512, start=0.0, dur=None):
    """Many videos at once in ONE process (one CUDA context): a reader thread per video (ffmpeg decode on the GPU or
    the CPU + row finding) feeds one queue; the main thread reads big mixed batches with the model and writes each
    row to its video's file. jobs = [(path, out_path, 'gpu'|'cpu')]. Measured 20 Oct 2026: separate processes per
    video plateaued at ~39x real time whatever the GPU / CPU decoder mix (GPU time-sliced between the contexts)."""
    import queue
    import subprocess
    import threading
    torch, m = load()
    dev = next(m.parameters()).device
    x0, y0, x1, y1 = PANELS[panel]
    w, h = x1 - x0, y1 - y0
    q = queue.Queue(maxsize=4096)

    def reader(vi, path, decoder):
        vw, vh = video_size(path)
        if decoder == 'gpu' and (vw, vh) == (1920, 1080):
            dec = ['-hwaccel', 'cuda', '-hwaccel_output_format', 'cuda', '-c:v', f'{video_codec(path)}_cuvid',
                   '-crop', f'{y0}x{1080 - y1}x{x0}x{1920 - x1}']
            vf = f'fps={fps},hwdownload,format=nv12,setparams=colorspace=bt709:color_primaries=bt709:' \
                 'color_trc=bt709:range=tv'
        else:
            dec, vf = [], f'fps={fps},scale=1920:1080,crop={w}:{h}:{x0}:{y0}'
        p = subprocess.Popen(['ffmpeg', '-v', 'error'] + dec + ['-ss', str(start), '-i', path]
                             + (['-t', str(dur)] if dur else []) + ['-vf', vf, '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-'],
                             stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
        k = 0
        while True:
            buf = p.stdout.read(w * h * 3)
            if len(buf) < w * h * 3:
                break
            img = Image.frombytes('RGB', (w, h), buf)
            for i, (a, b) in enumerate(panel_rows(img, PITCH)):
                q.put((vi, start + k / fps, i, to_tensor_image(img.crop((0, a, w, b)))))
            k += 1
        p.wait()
        q.put((vi, None, None, None))   # this video is done

    outs = [open(o, 'w', encoding='utf-8') for _, o, _ in jobs]
    threads = [threading.Thread(target=reader, args=(vi, pth, d), daemon=True) for vi, (pth, _, d) in enumerate(jobs)]
    for t in threads:
        t.start()
    live, cache = len(jobs), {}
    while live:
        items = [q.get()]
        while len(items) < batch_rows:
            try:
                items.append(q.get(timeout=0.05))
            except queue.Empty:
                break
        rows = [it for it in items if it[1] is not None]
        live -= len(items) - len(rows)
        if not rows:
            continue
        x = torch.zeros(len(rows), 1, H, MAXW)
        for j, it in enumerate(rows):
            x[j, 0, :, :it[3].shape[1]] = torch.from_numpy(it[3])
        with torch.inference_mode(), torch.autocast('cuda', dtype=torch.float16, enabled=dev.type == 'cuda'):
            lgs = m(x.to(dev, non_blocking=True)).float().cpu()
        for j, (vi, t, i, a) in enumerate(rows):
            lg = lgs[j, :max(1, a.shape[1] // 4 + 1)]
            raw, c = decode(lg)
            if raw not in cache:
                try:
                    cache[raw] = candidates(raw)
                except Exception as e:   # one odd read must never stop a bulk scan: it is an invalid row
                    print(f'flocr: unparsed read {raw!r}: {e}', file=sys.stderr)
                    cache[raw] = set()
            cands = cache[raw]
            if not cands:
                txt, ok = raw, 0
            elif raw in cands or len(cands) == 1:
                txt, ok = (raw if raw in cands else next(iter(cands))), 1
            else:
                txt, ok = max(cands, key=lambda s: ctc_logp(torch, lg, s)), 1
            outs[vi].write(f'{t:.2f}\t{i}\t{txt}\t{ok}\t{raw}\t{c:.2f}\n')
    for f in outs:
        f.close()


def video_size(path):
    import subprocess
    r = subprocess.run(['ffprobe', '-v', 'error', '-select_streams', 'v:0', '-show_entries', 'stream=width,height',
                        '-of', 'csv=p=0', path], capture_output=True, text=True).stdout.strip().split(',')
    return (int(r[0]), int(r[1])) if len(r) == 2 else (0, 0)


def video_codec(path):
    import subprocess
    return subprocess.run(['ffprobe', '-v', 'error', '-select_streams', 'v:0', '-show_entries', 'stream=codec_name',
                           '-of', 'csv=p=0', path], capture_output=True, text=True).stdout.strip()


def scan_video(path, panel='weapons', fps=2.0, out=sys.stdout, start=0.0, dur=None, decoder='gpu'):
    """Bulk reading of a HUD list panel through a whole video (no frame files: ffmpeg streams the panel crop at
    1080p scale): every row of every sampled frame -> '<t>\\t<row>\\t<text>\\t<valid>\\t<raw>\\t<conf>'. Rows of one
    frame are read in one batch on the GPU; reads are cached by raw text (a video repeats the same rows thousands of
    times). Vote / smooth across frames downstream (vote())."""
    import subprocess
    torch, m = load()
    dev = next(m.parameters()).device
    x0, y0, x1, y1 = PANELS[panel]
    w, h = x1 - x0, y1 - y0
    # decode on the GPU (NVDEC), frames stay on the GPU until the fps filter has dropped all but the sampled ones:
    # measured 20 Oct 2026 on the VP9 1080p60 playlist, 60 s of video: CPU decode 12.5 s CPU time, NVDEC 1.0 s, the
    # same wall time per stream - NVDEC frees the cores, so parallel scans scale. The CPU decode is the fallback.
    def cmd(hw):
        if hw:   # cuvid decoder crops in hardware (ffmpeg -h decoder=vp9_cuvid): only the panel ever leaves the GPU
            vw, vh = video_size(path)
            if (vw, vh) == (1920, 1080):
                codec = video_codec(path)
                dec = ['-hwaccel', 'cuda', '-hwaccel_output_format', 'cuda', '-c:v', f'{codec}_cuvid',
                       '-crop', f'{y0}x{1080 - y1}x{x0}x{1920 - x1}']
                # the cuvid crop drops the colour tags; without them the rgb conversion fails (silent CPU fallback)
                vf = f'fps={fps},hwdownload,format=nv12,setparams=colorspace=bt709:color_primaries=bt709:' \
                     'color_trc=bt709:range=tv'
            else:
                dec = ['-hwaccel', 'cuda', '-hwaccel_output_format', 'cuda']
                vf = f'fps={fps},hwdownload,format=nv12,scale=1920:1080,crop={w}:{h}:{x0}:{y0}'
        else:
            dec, vf = [], f'fps={fps},scale=1920:1080,crop={w}:{h}:{x0}:{y0}'
        return ['ffmpeg', '-v', 'error'] + dec + ['-ss', str(start), '-i', path] + (['-t', str(dur)] if dur else []) + \
               ['-vf', vf, '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-']
    p = subprocess.Popen(cmd(decoder == 'gpu'), stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    first = p.stdout.read(w * h * 3)
    if len(first) < w * h * 3 and decoder == 'gpu':   # no NVDEC for this file / machine: decode on the CPU
        p.wait()
        p = subprocess.Popen(cmd(False), stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
        first = p.stdout.read(w * h * 3)
    cache = {}
    k = 0
    pending = []   # (t, row, array) of up to 32 frames: one GPU call for all their rows (profiled 20 Oct 2026: one
    done = False   # call per frame spent half the scan in kernel launches of the LSTM, 14 ms per frame)
    while not done:
        buf, first = (first, None) if first is not None else (p.stdout.read(w * h * 3), None)
        if len(buf) < w * h * 3:
            done = True
        else:
            t = start + k / fps
            k += 1
            img = Image.frombytes('RGB', (w, h), buf)
            for i, (a, b) in enumerate(panel_rows(img, PITCH)):
                pending.append((t, i, to_tensor_image(img.crop((0, a, w, b)))))
            if k % 8:   # 8 frames per GPU call: 32 filled the card's memory with 12 parallel scans (spill to RAM)
                continue
        if not pending:
            continue
        x = torch.zeros(len(pending), 1, H, MAXW)   # fixed shape: cuDNN plans the kernels once (benchmark mode)
        for j, (_, _, a) in enumerate(pending):
            x[j, 0, :, :a.shape[1]] = torch.from_numpy(a)
        with torch.inference_mode(), torch.autocast('cuda', dtype=torch.float16, enabled=dev.type == 'cuda'):
            lgs = m(x.to(dev, non_blocking=True)).float().cpu()
        rows_out, pending = list(enumerate(pending)), []
        for j, (t, i, a) in rows_out:
            lg = lgs[j, :max(1, a.shape[1] // 4 + 1)]   # this row's own time steps (the batch pads to the widest row)
            raw, c = decode(lg)
            key = raw
            if key not in cache:
                cache[key] = candidates(raw)
            cands = cache[key]
            if not cands:
                txt, ok = raw, 0
            elif raw in cands or len(cands) == 1:   # read exactly / nothing to decide: no CTC scoring (the scoring
                txt, ok = (raw if raw in cands else next(iter(cands))), 1   # of every row capped parallel scans)
            else:
                txt, ok = max(cands, key=lambda s: ctc_logp(torch, lg, s)), 1
            out.write(f'{t:.2f}\t{i}\t{txt}\t{ok}\t{raw}\t{c:.2f}\n')
    p.wait()


def _close_n(word, choices, n, cutoff):
    import difflib
    return difflib.get_close_matches(word, choices, n=n, cutoff=cutoff)


def candidates(text):
    """Lexicon-composed alternatives for a raw read (for snap_ctc): the snap_info result plus the near names in the
    same row grammar (up to 5 per name part)."""
    lex = game_lexicon()
    s0, ok = snap_info(text)
    if not ok:   # the gate stays snap_info's: scoring only chooses between names, never admits a rejected row
        return set()   # (with looser candidates the HUD icon row '1 Saev' became a valid '1 Sabre', 20 Oct 2026)
    out = {s0}
    toks = s0.split(' ')
    lead = toks[0] if toks and _NUM.match(toks[0]) and len(toks) > 1 else None
    body = ' '.join(toks[1:]) if lead else s0
    tail = None
    if ' - ' in body:
        left, right = body.split(' - ', 1)
        for r in [right] + _close_n(right, lex['ships'], 5, 0.6):
            out.add(' '.join(x for x in (lead, f'{left} - {r}') if x))
        words = right.split(' ')
        if len(words) <= 3:
            import itertools
            opts = [[w] + _close_n(w, lex['words'], 3, 0.7) for w in words]
            for combo in itertools.product(*opts):
                out.add(' '.join(x for x in (lead, f"{left} - {' '.join(combo)}") if x))
        return out
    btoks = body.split(' ')
    if len(btoks) > 1 and re.fullmatch(r'\d{1,3}', btoks[-1]) and body not in lex['items']:
        tail, body = btoks[-1], ' '.join(btoks[:-1])
    if not body:
        return out
    for it in ([body] if body in lex['items'] else []) + _close_n(body, lex['items'], 5, 0.6):
        keep = tail if tail and (it in lex['counted'] or 'Waypoint' in it) else None
        out.add(' '.join(x for x in (lead, it, keep) if x))
    return out


def ctc_logp(torch, logits_row, text):
    """log P(text | image) under the model (CTC, all alignments)."""
    import torch.nn.functional as F
    tgt = encode(text)
    if not tgt:
        return float('-inf')
    lp = logits_row.float().log_softmax(-1).unsqueeze(1)          # (T, 1, C)
    loss = F.ctc_loss(lp, torch.tensor([tgt]), torch.tensor([lp.shape[0]]), torch.tensor([len(tgt)]),
                      blank=0, reduction='sum', zero_infinity=True)
    return -float(loss)


def snap_ctc(torch, logits_row, raw):
    """Lexicon-constrained decoding (20 Oct 2026): among the lexicon candidates of the raw read, the one the model's
    own CTC likelihood prefers. Settles near-ties string distance cannot (Reaper / Reaver read as 'Reaner'). The raw
    read itself never wins over a lexicon candidate: greedy reads always score high (a 1-letter misread 'Skvrail'
    out-scored the true 'Skyrail' by 6 nats), and the game data is the authority. -> (text, valid)."""
    cands = candidates(raw)
    if not cands:
        return raw, False
    return max(cands, key=lambda c: ctc_logp(torch, logits_row, c)), True


def snap_info(text):
    """Snap a read to what the game can show (game_lexicon) -> (text, valid). valid = the name part matched game data
    (gate for bulk use: non-HUD frames read as short garbage and do not match).
      - '<n> <item> [<count>]' (weapon list): the item from the equipment / object names; the count is kept only for
        equipment that uses ammo (launchers, mine / countermeasure droppers, from weapon_equip.ini), and a trailing
        digit that is part of the name stays ("Sunfury 1", "Scrambler 1").
      - '<distance> <faction> - <name>' (contact list): never a count; the faction from the HUD short names, the name
        whole from ship / mission names or word by word from the faction name lists.
      - '<distance> <name>' (story contact, object): mission names, then item / object names.
    Numbers stay as read; a part with no close match stays as read (and the row is not valid)."""
    lex = game_lexicon()
    toks = text.split(' ')
    if toks and re.fullmatch(r'\d[\d.,]*[KkM]?[.,]', toks[0]):   # "88." -> "88": stray dot after a distance
        toks[0] = toks[0][:-1]
    lead = toks.pop(0) if toks and _NUM.match(toks[0]) and len(toks) > 1 else None
    tail = toks[-1] if len(toks) > 1 and re.fullmatch(r'\d{1,3}', toks[-1]) else None
    rest = ' '.join(toks)
    # a route row "<distance> <from> -> <to>" (the model may read the arrow as " - ", "--", "->")
    m = re.match(r'^(.+?)\s*(?:->|-->|--|>|-)\s*(.+)$', rest)
    if m and not _closest(m.group(1), lex['shorts'], 0.6):
        a, b = _closest(m.group(1), lex['items'], 0.75), _closest(m.group(2), lex['items'], 0.75)
        if a and b:
            return ' '.join(x for x in (lead, f'{a}->{b}') if x), True
    if ' - ' in rest and (not tail or ' - ' in ' '.join(toks[:-1])):   # "X - 5": no name after the dash
        if tail:
            rest = ' '.join(toks[:-1])
        left, right = rest.split(' - ', 1)
        l2 = _closest(left, lex['shorts'], 0.6)
        r2 = _closest(right, lex['ships'], 0.65)
        if not r2:
            words = [w if len(w) < 3 else (_closest(w, lex['words'], 0.8) or w) for w in right.split(' ')]
            r2 = ' '.join(words)
            ok = l2 is not None and all(len(w) < 3 or w in lex['wordset'] for w in words)
        else:
            ok = l2 is not None
        return ' '.join(x for x in (lead, f"{l2 or left} - {r2}") if x), ok
    with_tail = _closest(rest, lex['items'], 0.85)
    if with_tail and tail and with_tail.endswith(' ' + tail):     # "Sunfury 1": the digit is the name's
        return ' '.join(x for x in (lead, with_tail) if x), True
    base = ' '.join(toks[:-1]) if tail else rest
    item = _closest(base, lex['items'], 0.8)
    if item:   # counts: ammo-using equipment; numbered waypoints ("Player Waypoint 4")
        keep = tail if tail and (item in lex['counted'] or 'Waypoint' in item) else None
        return ' '.join(x for x in (lead, item, keep) if x), True
    story = _closest(base, [n for _, n in lex['mission']], 0.75)
    if story:
        return ' '.join(x for x in (lead, story) if x), True
    return text, False


def cer(a, b):
    """Character error rate: edit distance / truth length."""
    d = list(range(len(b) + 1))
    for i, ca in enumerate(a, 1):
        prev, d[0] = d[0], i
        for j, cb in enumerate(b, 1):
            prev, d[j] = d[j], min(d[j] + 1, d[j - 1] + 1, prev + (ca != cb))
    return d[len(b)] / max(1, len(b))


def heldout(torch, m, dev):
    """The held-out real test (data\\heldout: fresh frames verified by eye, never trained on or used to choose): the
    full pipeline output (valid snapped text, '' when rejected) against the truth ('' = must be rejected)."""
    d = os.path.join(WORK, 'data', 'heldout')
    if not os.path.exists(os.path.join(d, 'labels.tsv')):
        return
    n = ok = text_n = text_ok = false_acc = 0
    errs = []
    m.eval()
    for r in open(os.path.join(d, 'labels.tsv'), encoding='utf-8'):
        if not r.strip():
            continue
        name, truth = (r.rstrip('\n').split('\t') + [''])[:2]
        a = to_tensor_image(Image.open(os.path.join(d, name)))
        with torch.no_grad():
            lg = m(batch(torch, [(a, '')]).to(dev))[0].float().cpu()
        s, valid = snap_ctc(torch, lg, decode(lg)[0])
        pred = s if valid else ''
        n += 1
        ok += pred == truth
        if truth:
            text_n += 1
            text_ok += pred == truth
        elif pred:
            false_acc += 1
        if pred != truth:
            errs.append(f'  {name}: truth "{truth}" got "{pred}"')
    print(f'heldout n={n}: exact {100 * ok / max(1, n):5.1f} %  text rows {text_ok}/{text_n}  false accepts {false_acc}')
    print('\n'.join(errs[:20]))


def evaluate():
    """Accuracy on the real crops: raw and lexicon-snapped, for the validation half (used for model choice) and the
    test half (never used to choose: the honest number)."""
    torch, m = load()
    dev = next(m.parameters()).device
    for part in ('val', 'test', 'all'):
        data = real_split(part)
        raw, sn = score(torch, m, data, dev), score(torch, m, data, dev, snapped=True)
        print(f'{part:4} n={len(data):3}  raw exact {100 * raw[0]:5.1f} % cer {raw[1]:.3f}   '
              f'snapped exact {100 * sn[0]:5.1f} % cer {sn[1]:.3f}')
    heldout(torch, m, dev)
    errs = []
    m.eval()
    for a, truth, path in real_split('all'):
        with torch.no_grad():
            lg = m(batch(torch, [(a, '')]).to(dev))[0].float().cpu()
        t, c = decode(lg)
        s = snap_ctc(torch, lg, t)[0]
        if s != truth:
            errs.append(f'  {path}: truth "{truth}" read "{t}" snapped "{s}" ({c:.2f})')
    print(f'{len(errs)} wrong after snapping:')
    print('\n'.join(errs[:60]))


def sample(n):
    out = os.path.join(WORK, 'data', 'samples')
    os.makedirs(out, exist_ok=True)
    corpus = load_corpus()
    with open(os.path.join(out, 'labels.tsv'), 'w', encoding='utf-8') as f:
        for i in range(n):
            t = hud_line(corpus)
            img = render(t, corpus)
            img.save(os.path.join(out, f'{i:04d}.png'))
            f.write(f'{i:04d}.png\t{t}\n')
    print('wrote', n, 'samples to', out)


if __name__ == '__main__':
    cmd = sys.argv[1] if len(sys.argv) > 1 else ''
    args = sys.argv[2:]
    if cmd == 'train':
        kw = {}
        for i in range(0, len(args) - 1, 2):
            if args[i] == '--steps':
                kw['steps'] = int(args[i + 1])
            if args[i] == '--batch':
                kw['bs'] = int(args[i + 1])
            if args[i] == '--lr':
                kw['lr'] = float(args[i + 1])
            if args[i] == '--scratch':   # --scratch 1: no warm start
                kw['init'] = args[i + 1] != '1'
            if args[i] == '--real':      # share of pseudo-labelled real rows (data\pseudo) in each batch
                kw['real_share'] = float(args[i + 1])
        train(**kw)
    elif cmd == 'read':
        read(args)
    elif cmd == 'panel':
        read_panel(args)
    elif cmd == 'eval':
        evaluate()
    elif cmd == 'scan':   # scan <video> [weapons|contacts] [fps] [start s] [duration s]
        scan_video(args[0], args[1] if len(args) > 1 else 'weapons', float(args[2]) if len(args) > 2 else 2.0,
                   start=float(args[3]) if len(args) > 3 else 0.0, dur=float(args[4]) if len(args) > 4 else None)
    elif cmd == 'sample':
        sample(int(args[0]) if args else 32)
    else:
        print(__doc__)
