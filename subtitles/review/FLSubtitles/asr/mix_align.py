"""The scene mixes' lines (gf_sounds.ini mix_*: whole room scenes as one track) -> data/mixes.tsv.

    .venv\\Scripts\\python -I mix_align.py --script <shooting_script.txt> --heard <dir> [--out ...]

Why (the owner, 29 Oct 2026): the mixes had no subtitles; their lines exist in no clip of their own, so they get cues
on the mix's own clock, like the intro movies (data/movies.tsv). Sources, ranked:
- what the recording says, with word times and a speaker tag per word: NVIDIA Nemotron-3.5-Transcribe's JSON per mix
  (NeMo-Speech.cpp `transcribe <dir> -r --diarize --format json --output-dir <out>`: <out>/mixes/<mix>.json; input
  16 kHz PCM WAV, decoded from the game's MP3-in-WAV with ffmpeg). Its word times hold on long mixes; the punctuated
  run without --diarize collapses them (mix_s002xe: 50 words inside 72-74 s), so it is not used;
- the shooting script: the scene's lines in order with their speaker tags (the mix's scene: mix_s019a = DX_S019A_*,
  mix_s027a/b/c = DX_S027X_*, mix_s002xe = DX_S002X_*): the speaker (display name as for every line,
  merge.speaker_name + data/speakers.tsv), the spelling and punctuation, the line breaks.
The heard words are cut into utterances (a pause >= PAUSE s or a change of speaker tag); each utterance is matched to
the script lines after the last one matched (up to three lines at once: an actor runs lines together), word by word, so
each heard word gets a script line or none. Per script line, from its heard words:
- the same words: the script's text;
- nearly the same (each side >= CLOSE of the other's words): the script's text, [check] heard: ...;
- a part of the line (the mix starts or ends in it, or the take dropped words): that part of the script's text
  ("..." where it is cut), [check];
- other words: the recording's words (the review's rule: the recording is the subtitle), the script line's speaker,
  [check] with the script's text.
A script line no word matched is a cut line: left out (copyright rule: spoken lines only; listed in the log). An
utterance matching no line becomes a line of the recording's words, named only where its speaker tag is one
character's among the matched lines ([check]). Style and name rules (data/*_rules.tsv) apply to every text; the
glossary's names give the recording's words their capitals. The aligner (aligner/align_words.py) then force-aligns
each line in its window. Log: out/mix_align.log (cut lines, extra speech, speaker tags per name).
"""
import argparse
import collections
import difflib
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from merge import apply_name_rules, apply_style_rules, load_name_rules, load_speakers, speaker_name  # noqa: E402
from script_check import ID_LINE, spoken  # noqa: E402
from tsvio import atomic_write, read_tsv  # noqa: E402

HEADER = ["mix", "start", "end", "speaker", "text", "note", "reviewed"]  # reviewed: the review tool's mark
PAUSE = 0.45  # s of silence between two heard words that ends an utterance
MATCH = 0.6   # share of an utterance's words a script line must match to own it (and 3 words, or all of 1-2)
CLOSE = 0.75  # share of words both ways for "nearly the same"
AHEAD = 25    # script lines searched past the last matched one
NUMBERS = {str(n): w for n, w in enumerate(
    "zero one two three four five six seven eight nine ten eleven twelve thirteen fourteen fifteen sixteen seventeen "
    "eighteen nineteen twenty".split())}


def wkey(w: str) -> str:
    k = re.sub(r"[^a-z0-9']", "", w.lower().replace("’", "'")).strip("'")
    return NUMBERS.get(k, k)  # numerals and number words alike ("Freeport 7" = "freeport seven")


def script_scenes(path: Path) -> list:
    """[(line id, raw speaker tag, spoken text)] in script order. A block: id line, speaker line, optional (note)
    lines, text lines; some blocks have no speaker line (DX_S010X_0001_ORE_RNNR01.WAV): the tag comes from the id."""
    lines = path.read_text(encoding="utf-8-sig").replace("\r", "").split("\n")
    out, i = [], 0
    while i < len(lines):
        m = ID_LINE.match(lines[i])
        if not m:
            i += 1
            continue
        lid = m.group(1).upper()
        j, body = i + 1, []
        while j < len(lines) and lines[j].strip() and not ID_LINE.match(lines[j]):
            body.append(lines[j].strip())
            j += 1
        if body and not re.search(r"[a-z]", re.sub(r"\([^)]*\)", "", body[0])):
            tag = body.pop(0)
        else:  # the speaker from the id: DX_S010X_0001_ORE_RNNR01 -> ORE RNNR01
            tag = " ".join(re.sub(r"^DX_S\d+[A-Z]*_(\d+_)+", "", lid).split("_"))
        text = spoken(" ".join(b for b in body if not b.startswith("(")))
        if text:
            out.append((lid, tag, text))
        i = j
    return out


def scene_lines(mix: str, script: list) -> list:
    """The script lines of a mix's scene: the longest id prefix that has lines (S072AA -> S072A, S027A -> S027X)."""
    base = mix[4:].upper()
    num = re.match(r"S\d+", base).group(0)
    for p in [base[:n] for n in range(len(base), len(num), -1)] + [num + "X"]:
        got = [s for s in script if s[0].startswith(f"DX_{p}_")]
        if got:
            return got
    return []


def heard_words(path: Path) -> list:
    """[(word as heard, key, start, end, speaker tag)] of the diarized run."""
    words = json.loads(path.read_text(encoding="utf-8")).get("words", [])
    return [(w["word"], wkey(w["word"]), w["start"], w["end"], w.get("speaker", -1)) for w in words if wkey(w["word"])]


def utterances(heard: list) -> list:
    """[[heard index]]: cut at a pause or a change of speaker tag."""
    out = []
    for j, h in enumerate(heard):
        if out and h[2] - heard[j - 1][3] < PAUSE and h[4] == heard[j - 1][4]:
            out[-1].append(j)
        else:
            out.append([j])
    return out


def casing(glossary: Path, table: dict) -> list:
    """[(regex, name)] longest first: the glossary's names and the speakers' display names as whole phrases ("the
    order" -> "The Order", while a lone "the" stays)."""
    names = glossary.read_text(encoding="utf-8").split("Vocabulary:", 1)[-1].split(",") + list(table.values())
    names = sorted({n.strip().rstrip(".") for n in names if n.strip()[:1].isupper()}, key=len, reverse=True)
    return [(re.compile(r"(?<![\w'])" + re.escape(n) + r"(?![\w'])", re.I), n) for n in names]


def as_sentence(words: list, cased: list) -> str:
    """The diarized run's words (no capitals, no punctuation) as a sentence, names in their capitals."""
    text = " ".join(w.capitalize() if w == "i" or w.startswith("i'") else w for w in words)
    for rx, name in cased:
        text = rx.sub(name, text)
    return (text[:1].upper() + text[1:] + ("" if text[-1:] in ".?!-" else ".")) if text else ""


def match(keys: list, lines: list, first: int) -> tuple:
    """The best owner of an utterance among the script lines from `first` (up to three consecutive lines): (matched
    share, {utterance position: line index}) or (0, {})."""
    best = (0.0, {})
    for l0 in range(first, min(len(lines), first + AHEAD)):
        for n in (1, 2, 3):
            span = range(l0, min(len(lines), l0 + n))
            ks, own = [], []
            for li in span:
                ks += lines[li][3]; own += [li] * len(lines[li][3])
            sm = difflib.SequenceMatcher(None, keys, ks, autojunk=False)
            got = {}
            for a, b, size in sm.get_matching_blocks():
                for d in range(size):
                    got[a + d] = (own[b + d], b + d - own.index(own[b + d]))
            share = len(got) / len(keys)
            if share > best[0] + 1e-9:  # ties: the earlier, shorter span
                best = (share, got)
    return best


def build(mix: str, script: list, heard: list, table: dict, rules: list, style: list, cased: dict, log: list) -> list:
    lines = [(lid, tag, text, [wkey(w) for w in text.split() if wkey(w)])
             for lid, tag, text in scene_lines(mix, script)]
    own = collections.defaultdict(list)  # script line -> [(heard index, script word position)]
    extra, last = [], 0
    for u in utterances(heard):
        keys = [heard[j][1] for j in u]
        share, got = match(keys, lines, last)
        if share < MATCH and last:  # an earlier line again (a mix part can repeat or reorder its scene)
            share, got = match(keys, lines, 0)
        if share < MATCH or (len(keys) <= 2 and share < 1) or (len(keys) > 2 and len(got) < 3):
            extra.append(u)  # 1-2 common words are no evidence ("this", "a")
            continue
        lis = sorted({li for li, _ in got.values()})
        last = lis[-1]
        for pos, j in enumerate(u):  # an unmatched heard word goes with the nearest matched one before it (else after)
            near = got.get(pos) or next((got[p] for p in range(pos - 1, -1, -1) if p in got), None) or \
                next(got[p] for p in range(pos + 1, len(u)) if p in got)
            own[near[0]].append((j, got[pos][1] if pos in got else None))
    rows, tags = [], collections.defaultdict(collections.Counter)
    for li, (lid, tag, text, skeys) in enumerate(lines):
        if li not in own:
            log.append(f"{mix}: cut (not heard): {lid} {text}")
            continue
        js = [j for j, _ in own[li]]
        hit = sorted(p for _, p in own[li] if p is not None)
        name = speaker_name("mixes", tag, table)
        for j in js:
            tags[heard[j][4]][name] += 1
        hkeys = [heard[j][1] for j in js]
        said = " ".join(heard[j][0] for j in js)
        toks = text.split()
        if hkeys == skeys:
            out, note = text, ""
        elif len(hit) >= CLOSE * len(skeys) and len(hit) >= CLOSE * len(hkeys):
            out, note = text, f"[check] heard: {said}"
        elif len(hit) >= CLOSE * len(hkeys) and len(hit) >= 2 and len(hit) >= CLOSE * (hit[-1] - hit[0] + 1):
            # a part of the line: its script words (only where the recording says that stretch: mix_s002xe's
            # newscaster skips a sentence in the middle of the script's line)
            kpos = [n for n, t in enumerate(toks) if wkey(t)]
            p0, p1 = kpos[hit[0]], kpos[hit[-1]]
            out = ("..." if p0 > 0 else "") + " ".join(toks[p0:p1 + 1]) + ("..." if p1 < len(toks) - 1 else "")
            note = f"[check] part of the script line {lid} (the mix starts or ends in it, or the take drops words); heard: {said}"
        else:
            out = as_sentence([heard[j][0] for j in js], cased)
            note = f"[check] the recording's words (speech to text); the script line {lid}: {text}"
        out = apply_style_rules(apply_name_rules(out, rules), style)
        rows.append([mix, heard[min(js)][2], heard[max(js)][3], name, out, note])
    for u in extra:  # speech no script line covers: named only where its speaker tag is one character's
        t = collections.Counter(heard[j][4] for j in u).most_common(1)[0][0]
        named = tags[t].most_common(1)
        name = named[0][0] if named and named[0][1] >= 5 and named[0][1] >= 0.8 * sum(tags[t].values()) else ""
        said = apply_style_rules(apply_name_rules(as_sentence([heard[j][0] for j in u], cased), rules), style)
        rows.append([mix, heard[u[0]][2], heard[u[-1]][3], name, said,
                     "[check] not in the script: the recording's words (speech to text)" +
                     ("" if name else "; speaker unknown")])
        log.append(f"{mix}: extra speech @ {heard[u[0]][2]:.2f} ({name or 'speaker unknown'}): {said}")
    for t, c in sorted(tags.items(), key=lambda x: str(x[0])):
        log.append(f"{mix}: speaker tag {t}: " + ", ".join(f"{n or '(no name)'} {k}" for n, k in c.most_common()))
    rows.sort(key=lambda r: r[1])
    for a, b in zip(rows, rows[1:]):  # a window never runs into the next line
        a[2] = min(a[2], b[1])
    return rows


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--script", type=Path, required=True)
    ap.add_argument("--heard", type=Path, required=True, help="Nemotron JSON of the --diarize run: <dir>/mixes/<mix>.json")
    ap.add_argument("--out", type=Path, default=HERE.parent / "data" / "mixes.tsv")
    ap.add_argument("--force", action="store_true", help="overwrite a file with reviewed lines (they are lost)")
    a = ap.parse_args()
    data = HERE.parent / "data"
    if a.out.exists() and not a.force and any(r.get("reviewed") for r in read_tsv(a.out)):
        sys.exit(f"{a.out} has reviewed lines (the review tool): a new draft would replace them (--force)")
    script = script_scenes(a.script)
    table = load_speakers(data / "speakers.tsv")
    rules, style = load_name_rules(data / "name_rules.tsv"), load_name_rules(data / "style_rules.tsv")
    cased = casing(HERE / "glossary.txt", table)
    mixes = sorted(r["id"] for r in read_tsv(HERE.parent / "out" / "index.tsv") if r["voice"] == "mixes")
    rows, log = [], []
    for mix in mixes:
        hj = a.heard / "mixes" / f"{mix}.json"
        if not hj.exists():
            log.append(f"{mix}: no speech-to-text run ({hj})")
            continue
        got = build(mix, script, heard_words(hj), table, rules, style, cased, log)
        rows += got
        print(f"{mix}: {len(got)} lines ({sum(1 for r in got if r[5])} to check)")
    head = ("# The scene mixes' lines (FLSubtitles): gf_sounds.ini mix_* play whole room scenes as one track. Times in\n"
            "# seconds from the mix's start. Spoken lines only. Built by asr/mix_align.py (shooting script lines matched to\n"
            "# Nemotron-3.5-Transcribe's words and speaker tags), force-aligned by aligner/align_words.py (voice \"mix\");\n"
            "# a note starting [check] needs a listen; reviewed = who reviewed it in the review tool (mix_review.py), and\n"
            "# when. The game shows them when the mix plays (modules/subtitles/src/impl/mixcues.cpp).\n")
    body = "\t".join(HEADER) + "\n" + "".join(
        f"{r[0]}\t{r[1]:.2f}\t{r[2]:.2f}\t{r[3]}\t{r[4]}\t{r[5]}\t\n" for r in rows)
    atomic_write(a.out, head + body)
    (HERE.parent / "out" / "mix_align.log").write_text("\n".join(log) + "\n", encoding="utf-8")
    print(f"{a.out}: {len(rows)} lines in {len({r[0] for r in rows})} mixes; log out/mix_align.log")


if __name__ == "__main__":
    main()
