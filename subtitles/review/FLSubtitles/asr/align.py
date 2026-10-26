"""Align the shooting script with the recordings scene by scene and build the subtitle draft.

    align.py --out <dir> --script <shooting_script.txt>    (reads <dir>/index.tsv and <dir>/asr.tsv)

Why: the final recordings don't always follow the script's line ids. Lines were split into several
clips (Dexter s032a_0501 -> 0501 "Ah." + 0502), clips were inserted and later ones renumbered
(Sinclair s028x_0704 -> 0705), long lines were cut to their first words. So per scene (DX_<scene>_...)
the script lines and the clips are both put in line-number order, their words are aligned
(difflib), and every clip takes the script words its audio covers: the script's wording, spelling
and punctuation where the recording agrees with it ("prefer the script"), the transcription
where it doesn't match.

Writes (game content, keep outside the repository):
  <dir>/aligned.tsv   id, voice, speaker, text, source, match, asr text, notes
      source: script      clip text taken from the script (match = share of transcribed words found)
              asr         no reliable script match: transcription text
              asr-only    line has no script scene at all (GCS fragments, generic pilots, ...)
      notes:  words where the transcription differs inside the matched script span (review)
      flsubs: the FLSubs (GPL-3.0) human transcription of the clip, if --flsubs is given
  stdout: summary
"""
import argparse
import difflib
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))  # run with python -I: no implicit script dir
from script_check import ID_LINE, spoken  # noqa: E402
import flsubs  # noqa: E402

DX = re.compile(r"^DX_([A-Z0-9]+)_([0-9]+)([A-Z]?)(?:_(.*))?$", re.I)
MIN_MATCH = 0.6


def norm_words(display: str) -> list:
    t = display.lower().replace("’", "'").replace("‘", "'")
    return [w.strip("'") for w in re.split(r"[^a-z0-9']+", t) if w.strip("'")]


def script_lines(path: Path) -> list:
    """Ordered (id, speaker, spoken text) in document order."""
    lines = path.read_text(encoding="utf-8-sig").replace("\r", "").split("\n")
    out, i = [], 0
    while i < len(lines):
        m = ID_LINE.match(lines[i])
        if not m:
            i += 1
            continue
        speaker = lines[i + 1].strip() if i + 1 < len(lines) else ""
        j, text = i + 2, []
        while j < len(lines) and lines[j].strip() and not ID_LINE.match(lines[j]):
            if not lines[j].strip().startswith("("):  # delivery-note lines, also unclosed: "(Determined"
                text.append(lines[j].strip())
            j += 1
        out.append((m.group(1).upper(), speaker, spoken(" ".join(text))))
        i = j
    return out


def sort_key(lid: str):
    m = DX.match(lid)
    return (int(m.group(2)), m.group(3).upper(), lid.upper()) if m else (0, "", lid.upper())


def tidy(text: str) -> str:
    text = re.sub(r"^[\s.…,;:-]+", "", text).strip()
    text = re.sub(r"\s*\.\.\.\s*$", "...", text)
    text = re.sub(r"[,;:–-]+$", "...", text)  # clip ends mid-sentence
    return text[:1].upper() + text[1:] if text else text


def align_scene(script: list, clips: list):
    """script: [(id, speaker, text)], clips: [(key, asr text)] both in line order.
    Returns {key: (text, speaker, match, notes)} for clips with a reliable script match."""
    # script word stream: (norm word, display token index, script line index)
    disp, sw, owner = [], [], []
    for li, (_, _, text) in enumerate(script):
        for tok in text.split():
            disp.append((tok, li))
            for w in norm_words(tok):
                sw.append(w)
                owner.append(len(disp) - 1)
    aw, clip_of = [], []
    for ci, (_, asr, *_) in enumerate(clips):
        for w in norm_words(asr):
            aw.append(w)
            clip_of.append(ci)
    sm = difflib.SequenceMatcher(a=sw, b=aw, autojunk=False)
    lo, hi = [None] * len(aw), [None] * len(aw)  # asr word -> script word range (equal or substituted)
    equal = [False] * len(aw)
    for op, i1, i2, j1, j2 in sm.get_opcodes():
        if op == "equal":
            for k in range(j2 - j1):
                lo[j1 + k] = hi[j1 + k] = i1 + k
                equal[j1 + k] = True
        elif op == "replace":
            # Joined/split words first: a heard "godspeed" is the script's "God speed" (and vice versa).
            i, j = i1, j1
            while i < i2 and j < j2:
                if i + 1 < i2 and aw[j] == sw[i] + sw[i + 1]:
                    lo[j], hi[j], equal[j] = i, i + 1, True
                    i, j = i + 2, j + 1
                elif j + 1 < j2 and sw[i] == aw[j] + aw[j + 1]:
                    lo[j] = hi[j] = lo[j + 1] = hi[j + 1] = i
                    equal[j] = equal[j + 1] = True
                    i, j = i + 1, j + 2
                else:
                    break
            i1, j1 = i, j
            # the rest: proportional ranges
            n, m = j2 - j1, i2 - i1
            for k in range(n):
                if m <= 0:
                    lo[j1 + k] = hi[j1 + k] = max(0, i1 - 1)
                    continue
                lo[j1 + k] = i1 + (k * m) // n
                hi[j1 + k] = max(lo[j1 + k], i1 + ((k + 1) * m) // n - 1)
    spans = {}
    for ci, (key, asr, *_) in enumerate(clips):
        idx = [j for j in range(len(aw)) if clip_of[j] == ci]
        hits = [j for j in idx if equal[j]]
        if not idx or not hits:
            continue
        pos = [j for j in idx if lo[j] is not None]
        a0, a1 = min(lo[j] for j in pos), max(hi[j] for j in pos)
        spans[ci] = [a0, a1, idx, hits]
    # Edge snapping: a clip that starts/ends on a mismatched word ("Great" for "Good") absorbs up to two
    # neighbouring script words of the same line that no other clip claims.
    claimed = set()
    for a0, a1, _, _ in spans.values():
        claimed.update(range(a0, a1 + 1))
    for ci, sp in spans.items():
        a0, a1, idx, _ = sp
        line = lambda i: disp[owner[i]][1]  # noqa: E731
        if not equal[idx[0]]:
            for _ in range(2):
                if a0 > 0 and a0 - 1 not in claimed and line(a0 - 1) == line(a0):
                    a0 -= 1
                    claimed.add(a0)
        if not equal[idx[-1]]:
            for _ in range(2):
                if a1 + 1 < len(sw) and a1 + 1 not in claimed and line(a1 + 1) == line(a1):
                    a1 += 1
                    claimed.add(a1)
        sp[0], sp[1] = a0, a1
    line = lambda i: disp[owner[i]][1]  # noqa: E731
    # A clip never keeps words of a script line it doesn't match at all (another speaker's line, e.g.
    # Razor 1 took "We're losing containment!" from the battleship's line before it).
    for ci, sp in spans.items():
        a0, a1, idx, hits = sp
        own = {line(lo[j]) for j in hits}
        while a0 < a1 and line(a0) not in own:
            a0 += 1
        while a1 > a0 and line(a1) not in own:
            a1 -= 1
        sp[0], sp[1] = a0, a1
    # ... but it may take the unclaimed rest of its own line (end only) when the clip has spare time for
    # those words: the battleship's 6.6 s clip was heard only as "A reactor is hit." but also holds
    # "We're losing containment! Ahhhhhhh!". Lines cut short in the recording ("Doyle Lonnigan." in
    # 1.3 s) and unspoken cues ("Follow me.") on clips without spare time stay out.
    claimed = set()
    for a0, a1, _, _ in spans.values():
        claimed.update(range(a0, a1 + 1))
    for ci, sp in spans.items():
        a0, a1 = sp[0], sp[1]
        sec = clips[ci][2] if len(clips[ci]) > 2 else 0.0
        if sec <= 0:
            continue
        b1 = a1
        while b1 + 1 < len(sw) and b1 + 1 not in claimed and line(b1 + 1) == line(a1):
            b1 += 1
        extra = b1 - a1
        spoken = len(sp[2])  # words the transcription heard in this clip
        spare = sec - spoken / 2.5  # time left after the heard words at a slow pace
        if extra and spare >= extra / 2.5 + 0.8:
            claimed.update(range(a1 + 1, b1 + 1))
            sp[1] = b1
    result = {}
    for ci, (a0, a1, idx, hits) in spans.items():
        key = clips[ci][0]
        match = len(hits) / len(idx)
        # short clips: one substituted word (often a name: "Doyle" heard as "Sam") still counts, the script wins
        short_ok = len(idx) <= 4 and (a1 - a0 + 1) == len(idx) and len(hits) >= len(idx) - 1
        if match < MIN_MATCH and not short_ok:
            continue
        if (a1 - a0 + 1) > 1.5 * len(idx) + 3:  # span much longer than what was said: not trustworthy
            continue
        d0, d1 = owner[a0], owner[a1]
        text = tidy(" ".join(t for t, _ in disp[d0:d1 + 1]))
        lines = [disp[d][1] for d in range(d0, d1 + 1)]
        speaker = script[max(set(lines), key=lines.count)][1]
        span_words = sw[a0:a1 + 1]
        asr_words = [aw[j] for j in idx]
        notes = []
        for op, i1, i2, j1, j2 in difflib.SequenceMatcher(a=span_words, b=asr_words, autojunk=False).get_opcodes():
            if op != "equal":
                notes.append(f"[{' '.join(span_words[i1:i2])}] -> [{' '.join(asr_words[j1:j2])}]")
        result[key] = (text, speaker, match, "; ".join(notes))
    return result


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--script", required=True)
    ap.add_argument("--flsubs", help="FLSubs subtitles.xml (English) as a third text source")
    a = ap.parse_args()
    out = Path(a.out)
    rows = [l.split("\t") for l in (out / "index.tsv").read_text(encoding="utf-8").splitlines()[1:]]
    asr = {}
    for l in (out / "asr.tsv").read_text(encoding="utf-8").splitlines():
        k, _, t = (l.split("\t") + ["", ""])[:3]
        asr[k] = t

    scenes = {}
    for sid, spk, text in script_lines(Path(a.script)):
        m = DX.match(sid)
        if m:
            scenes.setdefault(m.group(1).upper(), {})[sid] = (sid, spk, text)  # last definition wins
    by_scene = {}
    for r in rows:
        m = DX.match(r[1])
        if m and f"{r[2]}/{r[1]}" in asr:
            by_scene.setdefault(m.group(1).upper(), []).append(r)

    aligned = {}
    for scene, rs in by_scene.items():
        if scene not in scenes:
            continue
        script = sorted(scenes[scene].values(), key=lambda s: sort_key(s[0]))
        rs = sorted(rs, key=lambda r: sort_key(r[1]))
        # The same line id can exist several times (a bank entry and a story sound, or two banks):
        # align each id once, then give the result to every copy.
        clips, copies = [], {}
        for r in rs:
            key, lid = f"{r[2]}/{r[1]}", r[1].upper()
            if lid not in copies:
                copies[lid] = []
                clips.append((lid, asr[key], float(r[4] or 0)))  # (id, transcription, seconds)
            copies[lid].append(key)
        for lid, res in align_scene(script, clips).items():
            for key in copies[lid]:
                aligned[key] = res

    fls = flsubs.load(a.flsubs) if a.flsubs else {}
    counts = {}
    with open(out / "aligned.tsv", "w", encoding="utf-8", newline="\n") as fo:
        fo.write("id\tvoice\tspeaker\ttext\tsource\tmatch\tasr\tnotes\tflsubs\n")
        for kind, lid, voice, src, sec, how, speaker, text, f in rows:
            key = f"{voice}/{lid}"
            hyp = asr.get(key, "")
            m = DX.match(lid)
            if key in aligned:
                t, spk, match, notes = aligned[key]
                source = "script"
            elif m and m.group(1).upper() in scenes:
                t, spk, match, notes, source = hyp, speaker, 0.0, "", "asr"
            else:
                t, spk, match, notes, source = hyp, speaker, 0.0, "", "asr-only"
            counts[(kind, source, bool(notes))] = counts.get((kind, source, bool(notes)), 0) + 1
            fl = fls.get(flsubs.key_for_row(kind, voice, f), ("",))[0]
            fo.write(f"{lid}\t{voice}\t{spk}\t{t}\t{source}\t{match:.2f}\t{hyp}\t{notes}\t{fl}\n")
    for (kind, source, has_notes), n in sorted(counts.items()):
        print(f"{kind:12} {source:9} {'differs' if has_notes else 'agrees ' if source == 'script' else '       '} {n}")


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    main()
