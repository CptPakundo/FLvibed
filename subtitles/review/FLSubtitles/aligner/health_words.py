"""Health of the word timings across EVERY line (no ground truth needed): the symptoms the owner sees on the timeline.

    python -I health_words.py [word_times.tsv]       (default: out/word_times_cache.tsv, the aligner's own rows)

Why (the owner, 1 Nov 2026: "some words get almost no time at all, others get too much or start too early or too
late"; "the major concern is overall alignment issues across all lines"): bench_words.py scores the few hand-placed
lines; this counts the visible failures on all ~25,000 lines plus the movies and mixes, per line kind. A word lasts
until the next placed word starts (the last until end_ms), as the review tool's timeline shows it - unless the file
has ends_ms. Standard library only.

Symptoms per word: near zero (< 40 ms), stacked (the same start as its neighbour), unplaced (-1), long (more than
LONG_PER_LETTER ms per letter and over LONG_MIN ms: a pause swallowed, or the next word placed late), last word
without length (< 60 ms). Per line: lines with any symptom.
"""
import csv
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
NEAR_ZERO, LAST_MIN, LONG_PER_LETTER, LONG_MIN = 40, 60, 180, 900


def rows(path):
    with open(path, encoding="utf-8", newline="") as f:
        yield from csv.DictReader((l for l in f if not l.startswith("#")), delimiter="\t")


def letters(w):
    return max(1, len(re.sub(r"[^A-Za-z0-9]", "", w)))


def main():
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "out" / "word_times_cache.tsv"
    kind = {}
    for r in rows(ROOT / "data" / "subtitles.tsv"):
        kind[(r["id"].lower(), r["voice"].lower())] = (r["kind"], r["text"])
    texts = {}  # the movies' and mixes' lines: (track id, voice, text hash)
    for t, col in (("movies", "movie"), ("mixes", "mix")):
        p = ROOT / "data" / f"{t}.tsv"
        if p.exists():
            for r in rows(p):
                texts[(r[col].lower(), col, fnv(r["text"]))] = (t, r["text"])
    c = defaultdict(Counter)
    for r in rows(path):
        lid, voice = r["id"].lower(), r["voice"].lower()
        k, text = kind.get((lid, voice), (None, None))
        if k is None:
            k, text = texts.get((lid, voice, r["text_fnv"]), (None, None))
        if k is None or fnv(text) != r["text_fnv"]:
            continue
        words = text.split()
        s = [int(x) for x in r["starts_ms"].split(",")]
        if len(s) != len(words):
            continue
        end_ms = int(r["end_ms"] or 0)
        if (r.get("ends_ms") or "").strip():
            e = [int(x) for x in r["ends_ms"].split(",")]
        else:
            e = [next((x for x in s[i + 1:] if x >= 0), end_ms) if s[i] >= 0 else -1 for i in range(len(s))]
        cc = c[k]
        cc["lines"] += 1
        cc["words"] += len(words)
        bad = Counter()
        for i, w in enumerate(words):
            if s[i] < 0:
                bad["unplaced"] += 1
                continue
            d = e[i] - s[i]
            if any(s[j] == s[i] for j in (i - 1, i + 1) if 0 <= j < len(s)):
                bad["stacked"] += 1
            elif d < NEAR_ZERO:
                bad["near zero"] += 1
            if d > LONG_MIN and d > LONG_PER_LETTER * letters(w):
                bad["long"] += 1
        if s[-1] >= 0 and e[-1] - s[-1] < LAST_MIN:
            bad["last no length"] += 1
        for b, n in bad.items():
            cc[b] += n
        if bad:
            cc["lines with any"] += 1
    cols = ["lines", "words", "unplaced", "stacked", "near zero", "long", "last no length", "lines with any"]
    print(f"{path}\n| kind | " + " | ".join(cols) + " |")
    print("|---" * (len(cols) + 1) + "|")
    tot = Counter()
    for k in sorted(c, key=lambda k: -c[k]["lines"]):
        tot.update(c[k])
        print(f"| {k} | " + " | ".join(fmt(c[k], x) for x in cols) + " |")
    print("| all | " + " | ".join(fmt(tot, x) for x in cols) + " |")


def fmt(cc, x):
    if x in ("lines", "words"):
        return str(cc[x])
    base = cc["lines"] if x in ("last no length", "lines with any") else cc["words"]
    return f"{cc[x]} ({100 * cc[x] / max(base, 1):.1f}%)"


def fnv(s):
    h = 0x811C9DC5
    for b in (s or "").encode("utf-8"):
        h = ((h ^ b) * 0x01000193) & 0xFFFFFFFF
    return f"{h:08x}"


if __name__ == "__main__":
    main()
