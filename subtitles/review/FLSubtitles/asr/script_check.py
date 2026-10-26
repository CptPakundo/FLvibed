"""Compare the Bonus DVD shooting script against speech-to-text for the same lines.

  sample:  script_check.py sample  --game <HDE install> --script <shooting_script.txt> --n 60 --work <dir>
           writes <dir>/clips.tsv (id, wav path) and <dir>/script.tsv (id, speaker, text)
  compare: script_check.py compare --work <dir>   (after transcribe.py wrote <dir>/asr.tsv)
           prints word error rate of script vs recording and the lines that differ most

Everything written to <dir> is game content: keep it outside the repository.

Library note: this module is imported by align.py, build_index.py and name_audit.py (parse_script, ID_LINE,
spoken); its sample / compare main is the session-1 word-error-rate experiment.
"""
import argparse
import random
import re
import sys
from pathlib import Path

ID_LINE = re.compile(r"^\s*(dx_[a-z0-9_]+?)(\.wav)?\s*$", re.I)


def parse_script(path: Path) -> dict:
    """id -> (speaker, spoken text). Block: id line, speaker line, optional (note) line, text lines."""
    lines = path.read_text(encoding="utf-8-sig").replace("\r", "").split("\n")
    out = {}
    i = 0
    while i < len(lines):
        m = ID_LINE.match(lines[i])
        if not m:
            i += 1
            continue
        lid = m.group(1).upper()
        speaker = lines[i + 1].strip() if i + 1 < len(lines) else ""
        j = i + 2
        text = []
        while j < len(lines) and lines[j].strip() and not ID_LINE.match(lines[j]):
            if not lines[j].strip().startswith("("):  # delivery-note lines, also unclosed: "(Determined"
                text.append(lines[j].strip())
            j += 1
        out[lid] = (speaker, spoken(" ".join(text)))
        i = j
    return out


def spoken(text: str) -> str:
    """Drop delivery notes like (Filter - ...) / (A beat) and bracketed non-speech like [Death Scream]."""
    text = re.sub(r"\([^)]*\)", " ", text)
    text = re.sub(r"\[[^\]]*\]", " ", text)
    return " ".join(text.split())


def words(text: str) -> list:
    text = text.lower().replace("’", "'")
    text = re.sub(r"[^a-z0-9' ]+", " ", text)
    return text.split()


def edit_distance(a: list, b: list) -> int:
    prev = list(range(len(b) + 1))
    for i, x in enumerate(a, 1):
        cur = [i]
        for j, y in enumerate(b, 1):
            cur.append(min(prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (x != y)))
        prev = cur
    return prev[-1]


def story_sounds(game: Path) -> dict:
    """story_sounds.ini: nickname (upper) -> wav path."""
    ini = game / "DATA" / "AUDIO" / "story_sounds.ini"
    out, nick = {}, None
    for line in ini.read_text(encoding="latin-1").splitlines():
        line = line.split(";")[0].strip()
        if line.lower().startswith("nickname"):
            nick = line.split("=", 1)[1].strip().upper()
        elif line.lower().startswith("file") and nick:
            out[nick] = game / "DATA" / line.split("=", 1)[1].strip()
    return out


def cmd_sample(a) -> None:
    script = parse_script(Path(a.script))
    sounds = story_sounds(Path(a.game))
    ids = sorted(k for k in sounds if k in script and len(words(script[k][1])) >= 3 and sounds[k].exists())
    random.seed(a.seed)
    pick = random.sample(ids, min(a.n, len(ids)))
    work = Path(a.work)
    work.mkdir(parents=True, exist_ok=True)
    (work / "clips.tsv").write_text("".join(f"{k}\t{sounds[k]}\n" for k in pick), encoding="utf-8")
    (work / "script.tsv").write_text("".join(f"{k}\t{script[k][0]}\t{script[k][1]}\n" for k in pick),
                                     encoding="utf-8")
    print(f"{len(script)} script lines, {len(ids)} story clips with script text, sampled {len(pick)}")


def cmd_compare(a) -> None:
    work = Path(a.work)
    ref = {}
    for line in (work / "script.tsv").read_text(encoding="utf-8").splitlines():
        k, spk, text = line.split("\t")
        ref[k] = (spk, text)
    rows, errs, total = [], 0, 0
    for line in (work / "asr.tsv").read_text(encoding="utf-8").splitlines():
        k, sec, hyp = line.split("\t")
        r = words(ref[k][1])
        e = edit_distance(r, words(hyp))
        errs += e
        total += len(r)
        rows.append((e / max(1, len(r)), k, ref[k][0], ref[k][1], hyp))
    rows.sort(reverse=True)
    exact = sum(1 for r in rows if r[0] == 0)
    print(f"lines {len(rows)}, identical {exact}, WER script vs recording {100 * errs / max(1, total):.1f}%")
    for rate, k, spk, r, h in rows[: a.show]:
        if rate == 0:
            break
        print(f"\n{k} [{spk}] {100 * rate:.0f}%\n  script: {r}\n  asr:    {h}")


def main() -> None:
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("sample")
    s.add_argument("--game", required=True)
    s.add_argument("--script", required=True)
    s.add_argument("--work", required=True)
    s.add_argument("--n", type=int, default=60)
    s.add_argument("--seed", type=int, default=1)
    c = sub.add_parser("compare")
    c.add_argument("--work", required=True)
    c.add_argument("--show", type=int, default=25)
    a = ap.parse_args()
    {"sample": cmd_sample, "compare": cmd_compare}[a.cmd](a)


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    main()
