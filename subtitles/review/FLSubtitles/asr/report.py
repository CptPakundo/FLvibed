"""Three-source vote: shooting script, speech to text of the recording, FLSubs human transcription.

    report.py --out <dir>     (reads <dir>/aligned.tsv, writes <dir>/review.tsv and <dir>/report.md)

Script lines whose transcription differs only cosmetically keep the script. Real word differences are
settled by FLSubs where it agrees with one side ("FLSubs agrees with script" -> keep script, likely a
speech-to-text error; "FLSubs hears the recording too" -> the actor deviated, review). Lines without
script compare speech to text with FLSubs. Script-vs-transcription differences are sorted into:
  spelling   only spelling/name variants (similar words, e.g. archeology/archaeology, Bretonian/Britonian)
             -> the script spelling is kept, nothing to review
  filler     only interjections/fillers/contractions/number style/word splits differ
             (uh, oh, well; you're/you are; 4/four; Xeno-Archeology/xenoarcheology)
             -> script kept
  words      real word differences: the actor said something else, or the transcription is wrong
             -> review list (the script text stays in the draft until a reviewer decides otherwise)
Both outputs are game content: keep them outside the repository.
"""
import argparse
import difflib
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))  # run with python -I
from compare_all import norm, tokens  # noqa: E402

FILLER = re.compile(r"^(u+h+|u+m+|h+m+|m+h*m*|a+h+|o+h+|e+h+|sh+|ahem|aha|huh|er|well|so|oh|now|and|but|yes|yeah|"
                    r"okay|ok|hey|look|listen|right|just|i|a|the|that|is|are|am|will|would|have|has|not|do|us|"
                    r"you|we|they|it|he|she|let|s|re|ll|ve|d|m|t)$")
CHANGE = re.compile(r"\[([^\]]*)\] -> \[([^\]]*)\]")


NUMBERS = {w: str(i) for i, w in enumerate(
    "zero one two three four five six seven eight nine ten eleven twelve thirteen fourteen fifteen sixteen "
    "seventeen eighteen nineteen twenty".split())}
NUMBERS.update({"thirty": "30", "forty": "40", "fifty": "50", "sixty": "60", "seventy": "70", "eighty": "80",
                "ninety": "90", "hundred": "100", "niner": "9", "oh": "0"})


def numbers(words: list) -> list:
    return [NUMBERS.get(w, w) for w in words]


def similar(a: str, b: str) -> bool:
    return difflib.SequenceMatcher(a=a, b=b).ratio() >= 0.6


def squash(text: str) -> str:
    """Line reduced to its content: contractions expanded, numbers as digits, fillers and spacing dropped."""
    words = norm(tokens(text))
    return "".join(w for w in numbers(words) if not re.fullmatch(r"u+h+|u+m+|h+m+|m+h*m*|a+h+|o+h+|e+h+|sh+|"
                                                                 r"ahem|aha|huh|er|well|so|oh|now|and", w))


def classify_pair(a: str, b: str) -> str:
    """Classify the difference between two free texts (filler / spelling / words)."""
    x, y = norm(tokens(a)), norm(tokens(b))
    notes = "; ".join(f"[{' '.join(x[i1:i2])}] -> [{' '.join(y[j1:j2])}]"
                      for op, i1, i2, j1, j2 in difflib.SequenceMatcher(a=x, b=y, autojunk=False).get_opcodes()
                      if op != "equal")
    return classify(notes, a, b) if notes else "filler"


def classify(notes: str, script: str = "", asr: str = "") -> str:
    if script and asr and squash(script) == squash(asr):
        return "filler"
    kinds = set()
    for s, a in CHANGE.findall(notes):
        sw, aw = s.split(), a.split()
        if numbers(sw) == numbers(aw):  # "Beta 4" spoken "Beta Four"
            kinds.add("spelling")
        elif all(FILLER.match(w) for w in sw + aw):
            kinds.add("filler")
        elif "".join(sw) == "".join(aw) or (len(sw) == len(aw) and all(similar(x, y) for x, y in zip(sw, aw))):
            kinds.add("spelling")
        else:
            kinds.add("words")
    return "words" if "words" in kinds else ("spelling" if "spelling" in kinds else "filler")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    out = Path(a.out)
    head, *rows = [l.split("\t") for l in (out / "aligned.tsv").read_text(encoding="utf-8").splitlines()]
    col = {h: i for i, h in enumerate(head)}
    has_fl = "flsubs" in col

    def same(a: str, b: str) -> bool:
        return bool(a) and bool(b) and squash(a) == squash(b)

    totals, classes, review, every = {}, {}, [], []
    for r in rows:
        src, text, asr, notes = r[col["source"]], r[col["text"]], r[col["asr"]], r[col["notes"]]
        fl = r[col["flsubs"]] if has_fl else ""
        if src == "script":
            c = classify(notes, text, asr) if notes else "agrees"
            if c == "words" and fl:
                if same(fl, asr) or (not same(fl, text) and classify_pair(fl, asr) != "words"):
                    c = "words: FLSubs hears the recording too"     # two sources say the actor deviated
                elif same(fl, text) or classify_pair(fl, text) != "words":
                    c = "words: FLSubs agrees with script"          # likely a speech-to-text error
            group = "script lines"
        else:
            if not asr:
                c = "no transcription"
            elif not fl:
                c = "speech to text only"
            elif same(fl, asr):
                c = "FLSubs = speech to text"
            elif classify_pair(fl, asr) != "words":
                c = "FLSubs ~ speech to text (cosmetic)"
            else:
                c = "FLSubs differs from speech to text"
            group = "lines without script"
        classes.setdefault(group, {})
        classes[group][c] = classes[group].get(c, 0) + 1
        totals[src] = totals.get(src, 0) + 1
        every.append((c, r))
        if c in ("words", "words: FLSubs hears the recording too", "FLSubs differs from speech to text"):
            review.append((c, r))
    review.sort(key=lambda x: (x[0], x[1][col["id"]].lower()))
    header = "class\tid\tvoice\tspeaker\tdraft text\trecording (speech to text)\tFLSubs\tdifferences\n"
    for name, items in (("review.tsv", review), ("classified.tsv", every)):
        with open(out / name, "w", encoding="utf-8", newline="\n") as fo:
            fo.write(header)
            for c, r in items:
                fl = r[col["flsubs"]] if has_fl else ""
                cells = [c] + [r[col[k]] for k in ("id", "voice", "speaker", "text", "asr")] + [fl, r[col["notes"]]]
                fo.write("\t".join(cells) + "\n")
    lines = ["# Script, recordings and FLSubs", "", "| Draft source | Clips |", "|---|---|"]
    lines += [f"| {k} | {v} |" for k, v in sorted(totals.items())]
    for group, d in classes.items():
        lines += ["", f"{group}:", "", "| Class | Clips |", "|---|---|"]
        lines += [f"| {k} | {v} |" for k, v in sorted(d.items(), key=lambda kv: -kv[1])]
    lines += ["", f"review list: {len(review)} clips (review.tsv)"]
    (out / "report.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("\n".join(lines))


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    main()
