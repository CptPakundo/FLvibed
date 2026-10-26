"""Compare the shooting script with the transcription of every line that has script text.

    compare_all.py --out <dir>     (reads <dir>/index.tsv and <dir>/asr.tsv)

Writes (game content, keep outside the repository):
  <dir>/compare.tsv   class, word-diff %, id, voice, speaker, script text, asr text, changed words
  <dir>/draft.tsv     id, voice, speaker, text, source: the first subtitle draft
                      (script text where the line has one — the script wins when in doubt — else the transcription)
Library note: this file is imported by report.py (norm, tokens); its main (the first draft.tsv) is the
session-1 experiment, superseded by merge.py.
Classes:
  same      identical after normalizing case and punctuation
  cosmetic  only contractions, filler words, spelling variants (alright/all right, OK/okay, hyphens)
  name      only differences in words the script capitalizes mid-sentence (names: ASR misspelling)
  words     real word differences: actor deviated from the script, or an ASR error -> review
"""
import argparse
import difflib
import re
import sys
from pathlib import Path

CONTRACTIONS = {
    "i'm": "i am", "you're": "you are", "we're": "we are", "they're": "they are", "he's": "he is",
    "she's": "she is", "it's": "it is", "that's": "that is", "there's": "there is", "what's": "what is",
    "who's": "who is", "here's": "here is", "where's": "where is", "let's": "let us", "i'll": "i will",
    "you'll": "you will", "we'll": "we will", "they'll": "they will", "he'll": "he will", "she'll": "she will",
    "it'll": "it will", "i've": "i have", "you've": "you have", "we've": "we have", "they've": "they have",
    "i'd": "i would", "you'd": "you would", "we'd": "we would", "they'd": "they would", "he'd": "he would",
    "she'd": "she would", "don't": "do not", "doesn't": "does not", "didn't": "did not", "can't": "can not",
    "cannot": "can not", "won't": "will not", "isn't": "is not", "aren't": "are not", "wasn't": "was not",
    "weren't": "were not", "haven't": "have not", "hasn't": "has not", "hadn't": "had not",
    "wouldn't": "would not", "shouldn't": "should not", "couldn't": "could not", "name's": "name is",
    "alright": "all right", "ok": "okay", "dr": "doctor", "mr": "mister", "gonna": "going to",
    "wanna": "want to", "'em": "them", "y'know": "you know",
}
FILLERS = {"uh", "um", "hm", "hmm", "mmm", "mm", "ah", "oh", "eh", "huh", "er", "well", "so", "okay", "now", "and"}


def tokens(text: str) -> list:
    text = text.replace("’", "'").replace("‘", "'").replace("-", " ")
    return re.sub(r"[^A-Za-z0-9' ]+", " ", text).split()


def norm(words: list) -> list:
    out = []
    for w in words:
        w = w.lower().strip("'")
        out.extend(CONTRACTIONS.get(w, w).split())
    return out


def classify(script: str, asr: str):
    st, at = tokens(script), tokens(asr)
    s, a = [w.lower().strip("'") for w in st], [w.lower().strip("'") for w in at]
    if s == a:
        return "same", 0.0, ""
    ns, na = norm(st), norm(at)
    sm = difflib.SequenceMatcher(a=ns, b=na, autojunk=False)
    changes, edits = [], 0
    for op, i1, i2, j1, j2 in sm.get_opcodes():
        if op != "equal":
            changes.append((ns[i1:i2], na[j1:j2]))
            edits += max(i2 - i1, j2 - j1)
    rate = edits / max(1, len(ns))
    shown = "; ".join(f"[{' '.join(x)}] -> [{' '.join(y)}]" for x, y in changes)
    if not changes:
        return "cosmetic", 0.0, ""
    if all(set(x) | set(y) <= FILLERS for x, y in changes):
        return "cosmetic", rate, shown
    if all("".join(x) == "".join(y) for x, y in changes):  # "xeno archaeology" vs "xenoarchaeology"
        return "cosmetic", rate, shown
    names = {w.lower() for i, w in enumerate(st) if i and w[:1].isupper()}
    if all(x and set(x) <= names and len(x) == len(y) for x, y in changes):
        return "name", rate, shown
    return "words", rate, shown


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    out = Path(a.out)
    rows = [l.split("\t") for l in (out / "index.tsv").read_text(encoding="utf-8").splitlines()[1:]]
    asr = {}
    for l in (out / "asr.tsv").read_text(encoding="utf-8").splitlines():
        k, _, t = (l.split("\t") + ["", ""])[:3]
        asr[k] = t
    stats, cmp_rows, draft = {}, [], []
    for kind, lid, voice, src, sec, how, speaker, text, f in rows:
        key = f"{voice}/{lid}"
        hyp = asr.get(key)
        if text and hyp is not None:
            c, rate, shown = classify(text, hyp)
            stats.setdefault((kind, how), {}).setdefault(c, 0)
            stats[(kind, how)][c] += 1
            cmp_rows.append((c, rate, lid, voice, speaker, text, hyp, shown))
        draft.append((lid, voice, speaker, text or (hyp or ""), "script" if text else ("asr" if hyp else "-")))
    order = {"words": 0, "name": 1, "cosmetic": 2, "same": 3}
    cmp_rows.sort(key=lambda r: (order[r[0]], -r[1]))
    with open(out / "compare.tsv", "w", encoding="utf-8", newline="\n") as fo:
        fo.write("class\tdiff%\tid\tvoice\tspeaker\tscript\tasr\tchanges\n")
        for c, rate, lid, voice, spk, s, h, shown in cmp_rows:
            fo.write(f"{c}\t{100 * rate:.0f}\t{lid}\t{voice}\t{spk}\t{s}\t{h}\t{shown}\n")
    with open(out / "draft.tsv", "w", encoding="utf-8", newline="\n") as fo:
        fo.write("id\tvoice\tspeaker\ttext\tsource\n")
        for r in draft:
            fo.write("\t".join(r) + "\n")
    print(f"{'kind':12} {'match':7} {'lines':>6} {'same':>6} {'cosm.':>6} {'name':>6} {'words':>6}")
    for (kind, how), d in sorted(stats.items()):
        n = sum(d.values())
        print(f"{kind:12} {how:7} {n:6} " + " ".join(f"{d.get(c, 0):6}" for c in ("same", "cosmetic", "name", "words")))
    print(f"draft: {len(draft)} lines, {sum(1 for r in draft if r[4] == 'script')} from script, "
          f"{sum(1 for r in draft if r[4] == 'asr')} from speech to text, {sum(1 for r in draft if r[4] == '-')} empty")


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    main()
