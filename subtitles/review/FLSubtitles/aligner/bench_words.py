"""How close word timings come to the owner's hand-placed ones (data/word_overrides.tsv, the ground truth).

    python -I bench_words.py [name=]<word_times.tsv> ...     (default: out/word_times_cache.tsv, the aligner's own rows)
    python -I bench_words.py --words <file> ...              (also every word's error, worst first)
    python -I bench_words.py --split <file> ...              (also scored on the words the owner moved / kept)

Why (the owner, 1 Nov 2026: the forced alignment is "sometimes baffling: some words get almost no time at all, others get
too much or start too early or too late"): every change to the aligner or its post-processing is scored here before it
ships. Standard library only (any venv). The ground truth grows by itself: every line the owner times on the review
tool's timeline (by = a person, not empty) is scored, keyed by line, voice and text hash. Hand timings the owner later
switched back to the aligner stay in aligner/bench_truth.tsv (same columns); both files are read, the shipping one wins.

A candidate file has word_times.tsv's columns (id, voice, text_fnv, end_ms, starts_ms; ms, -1 = not placed) and may add
ends_ms (one end per word). Without ends_ms a word ends where the next placed word starts, the last at end_ms - what
the review tool's timeline shows (mix_review.timing). Measures per word: start error (candidate - truth), end error,
"near zero" = a placed word shorter than NEAR_ZERO ms, unplaced = -1 or no row for the line (counted, not scored).

The truth is biased towards the aligner the owner started from: a word they left where it was scores 0 for that aligner
(and a near-equal placement elsewhere scores its offset). --split therefore also scores two subsets, against the
aligner's own rows (--ref, the cache): "moved" = words the owner moved more than KEPT_TOL ms (where that aligner was
wrong: what a better one must fix) and "kept" = the rest (where it was good enough: what a better one must not break;
read the <=50 / <=100 shares there, not the mean).
"""
import argparse
import csv
import statistics
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent  # FLSubtitles/
NEAR_ZERO = 40  # ms; the shortest word the owner placed is 50 ms
KEPT_TOL = 20  # ms


def fnv1a(s: str) -> str:
    h = 0x811C9DC5
    for b in s.encode("utf-8"):
        h = ((h ^ b) * 0x01000193) & 0xFFFFFFFF
    return f"{h:08x}"


def rows(path: Path):
    with open(path, encoding="utf-8", newline="") as f:
        yield from csv.DictReader((l for l in f if not l.startswith("#")), delimiter="\t")


def ints(s):
    return [int(x) for x in (s or "").split(",") if x.strip()]


def truth(path: Path, out=None):
    """{(id, voice, text_fnv): (words, starts, ends)} of the hand-placed lines (added to out)."""
    out = {} if out is None else out
    if not path.exists():
        return out
    for r in rows(path):
        if not (r.get("by") or "").strip() or (r.get("realign") or "").strip():
            continue  # not placed by a person, or words still waiting for the aligner
        s, e = ints(r["starts_ms"]), ints(r["ends_ms"])
        words = r["text"].split()
        if len(s) == len(e) == len(words) and fnv1a(r["text"]) == r["text_fnv"]:
            out[(r["id"].lower(), r["voice"].lower(), r["text_fnv"])] = (words, s, e)
    return out


def ends_of(starts, end_ms):
    out = []
    for i, s in enumerate(starts):
        nxt = next((x for x in starts[i + 1:] if x >= 0), end_ms)
        out.append(nxt if s >= 0 else -1)
    return out


def predictions(path: Path):
    out = {}
    for r in rows(path):
        s = ints(r["starts_ms"])
        e = ints(r.get("ends_ms")) if (r.get("ends_ms") or "").strip() else ends_of(s, int(r["end_ms"] or 0))
        out[(r["id"].lower(), r["voice"].lower(), r["text_fnv"])] = (s, e)
    return out


def score(gt, pred):
    """Per-word records [(key, i, word, start error, end error, length)] and the unplaced count."""
    recs, unplaced = [], 0
    for key, (words, gs, ge) in sorted(gt.items()):
        p = pred.get(key)
        for i, w in enumerate(words):
            if p is None or len(p[0]) != len(words) or p[0][i] < 0:
                unplaced += 1
                continue
            ps, pe = p[0][i], p[1][i]
            recs.append((key, i, w, ps - gs[i], pe - ge[i], pe - ps))
    return recs, unplaced


def summary(recs, unplaced):
    n = len(recs) + unplaced
    se = [abs(r[3]) for r in recs]
    ee = [abs(r[4]) for r in recs]

    def share(v, lim):
        return f"{100 * sum(1 for x in v if x <= lim) / n:.0f}%" if n else "-"

    def stat(v, f):
        return f"{f(v):.0f}" if v else "-"

    p90 = lambda v: sorted(v)[int(0.9 * (len(v) - 1))]
    return {"words": str(n), "unplaced": str(unplaced),
            "start mean": stat(se, statistics.mean), "start median": stat(se, statistics.median), "start p90": stat(se, p90),
            "start <=50": share(se, 50), "start <=100": share(se, 100),
            "end mean": stat(ee, statistics.mean), "end median": stat(ee, statistics.median), "end p90": stat(ee, p90),
            "end <=50": share(ee, 50), "end <=100": share(ee, 100),
            "near zero": str(sum(1 for r in recs if r[5] < NEAR_ZERO)),
            "bias start": stat([r[3] for r in recs], statistics.mean), "bias end": stat([r[4] for r in recs], statistics.mean)}


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("pred", nargs="*", help="[name=]path of a word_times-style TSV")
    ap.add_argument("--truth", nargs="+", default=[str(HERE / "bench_truth.tsv"), str(ROOT / "data" / "word_overrides.tsv")],
                    help="hand-timing files, read in order (a later file wins for the same line)")
    ap.add_argument("--words", action="store_true", help="every word's error, worst start first")
    ap.add_argument("--per-line", action="store_true", help="the summary per line too")
    ap.add_argument("--split", action="store_true", help="also the words the owner moved / kept (against --ref)")
    ap.add_argument("--ref", nargs="+", default=[str(HERE / "bench_ref.tsv"), str(ROOT / "out" / "word_times_cache.tsv")],
                    help="the aligner rows the owner started from (--split); the first file with the line wins - "
                         "bench_ref.tsv keeps those of the lines timed before 1 Nov 2026 (Qwen alone)")
    a = ap.parse_args()
    gt = {}
    for t in a.truth:
        truth(Path(t), gt)
    a.truth = ", ".join(a.truth)
    if not gt:
        sys.exit("no hand-placed lines in " + a.truth)
    preds = a.pred or [f"aligner={ROOT / 'out' / 'word_times_cache.tsv'}"]
    subsets = {}
    if a.split:
        ref = {}
        for p in reversed(a.ref):  # the first file wins
            if Path(p).exists():
                ref.update(predictions(Path(p)))
        for key, (words, gs, _) in gt.items():
            p = ref.get(key)
            for i in range(len(words)):
                kept = p is not None and len(p[0]) == len(words) and p[0][i] >= 0 and abs(p[0][i] - gs[i]) <= KEPT_TOL
                subsets.setdefault("kept" if kept else "moved", set()).add((key, i))
    table = []
    for spec in preds:
        name, _, path = spec.rpartition("=")
        name = name or Path(path).stem
        pred = predictions(Path(path))
        recs, unplaced = score(gt, pred)
        table.append((name, summary(recs, unplaced)))
        for sub in ("moved", "kept"):
            if sub in subsets:
                sr = [r for r in recs if (r[0], r[1]) in subsets[sub]]
                table.append((f"  {name} {sub}", summary(sr, len(subsets[sub]) - len(sr))))
        if a.per_line:
            for key in sorted(gt):
                lr = [r for r in recs if r[0] == key]
                lu = sum(1 for _ in gt[key][0]) - len(lr)
                table.append((f"  {key[0]} {key[2]}", summary(lr, lu)))
        if a.words:
            print(f"\n{name}: words by start error (ms)")
            print("line\ttext_fnv\t#\tword\tstart_err\tend_err\tlength")
            for r in sorted(recs, key=lambda r: -abs(r[3])):
                print(f"{r[0][0]}\t{r[0][2]}\t{r[1]}\t{r[2]}\t{r[3]:+d}\t{r[4]:+d}\t{r[5]}")
    cols = list(table[0][1])
    print(f"\nground truth: {len(gt)} hand-placed lines, {sum(len(v[0]) for v in gt.values())} words "
          f"({a.truth}); errors in ms, shares of all words (unplaced count as misses)")
    w0 = max(len(t[0]) for t in table)
    print("| " + "candidate".ljust(w0) + " | " + " | ".join(cols) + " |")
    print("|" + "-" * (w0 + 2) + "|" + "|".join("-" * (len(c) + 2) for c in cols) + "|")
    for name, s in table:
        print("| " + name.ljust(w0) + " | " + " | ".join(s[c].rjust(len(c)) for c in cols) + " |")


if __name__ == "__main__":
    main()
