"""Proper-noun audit: are the subtitle draft's names spelled the way the game spells them?

    name_audit.py --out <dir> --script <shooting_script.txt> [--flsubs <subtitles.xml>]
    reads <dir>/draft.tsv, <dir>/game_text.tsv (dump-resources.ps1 output: every string and infocard),
    <dir>/gamenames.tsv; writes <dir>/name_audit.md and <dir>/name_audit.tsv

Reference order (the game is canonical; wikis follow it): game text (strings + infocards) > shooting
script > FLSubs. For every capitalized word or name the draft uses mid-sentence:
  ok        spelled exactly like the game text
  case      the game writes it with other capitals ("neural net" -> "Neural Net")
  variant   a near-miss of a game spelling ("Tegaki" -> "Tekagi", "Rhineland" -> "Rheinland")
  script    not in the game text but in the script / FLSubs (names that only appear in dialogue)
  unknown   in no reference: check by ear
Multi-word game terms (Trade Lane, Jump Hole, ...) are checked for consistent capitals too.
"""
import argparse
import collections
import html
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))  # run with python -I
import merge  # noqa: E402
from script_check import parse_script  # noqa: E402

WORD = re.compile(r"[A-Za-z][A-Za-z'’-]*[A-Za-z]|[A-Za-z]")
TERMS = ["Trade Lane", "Jump Hole", "Jump Gate", "Neural Net", "Docking Ring", "Cruise Engine", "Tractor Beam",
         "Nav Map", "Battleship", "Freeport", "Planet", "Outpost", "Shipyard", "Proteus Tome", "Hyper Gate"]


def strip_rdl(text: str) -> str:
    text = re.sub(r"<[^>]+>", " ", text.replace("\\n", " "))
    return html.unescape(text)


def cap_words(text: str):
    """Capitalized words not at a sentence start (names), with their position-independent form."""
    toks = text.split()
    for i, t in enumerate(toks):
        for w in WORD.findall(t):
            w = w.replace("’", "'")
            if not w[:1].isupper():
                continue
            start = i == 0 or re.search(r"[.!?…:\"]$", toks[i - 1]) is not None
            yield w, start


def compute(out: Path, script_path: Path, flsubs_texts=()):
    """The audit itself: (rows, term_rows) for <out>/draft.tsv against <out>/game_text.tsv, <out>/gamenames.tsv,
    the shooting script and the FLSubs texts (any iterable of strings). Writes nothing (review_server.py
    calls this in-process after every Rebuild; main() below writes the reports)."""
    # reference vocabularies: word -> count, per source (exact case kept)
    game = collections.Counter()
    mid = collections.Counter()  # every word mid-sentence in game prose, exact case (for the case check)
    for line in (out / "game_text.tsv").read_text(encoding="utf-8").splitlines():
        parts = line.split("\t", 2)
        if len(parts) == 3:
            text = strip_rdl(parts[2])
            game.update(w for w, _ in cap_words(text))
            toks = text.split()
            for i, t in enumerate(toks[1:], 1):
                if re.search(r"[.!?…:\"]$", toks[i - 1]):
                    continue
                w = re.sub(r"[^\w'-]", "", t.replace("’", "'"))
                if w and not w.isupper():
                    mid[w] += 1
    for r in merge.tsv(out / "gamenames.tsv"):
        game.update(w for n in (r["name"], r["short"]) for w, _ in cap_words("x " + n))
    script = collections.Counter()
    for _, (spk, text) in parse_script(Path(script_path)).items():
        script.update(w for w, _ in cap_words(text))
    fls = collections.Counter()
    for text in flsubs_texts:
        fls.update(w for w, _ in cap_words(text))
    game_lower = collections.defaultdict(collections.Counter)
    for w, n in game.items():
        game_lower[w.lower()][w] += n

    # the draft's names
    draft = merge.tsv(out / "draft.tsv")
    used = collections.Counter()
    where = collections.defaultdict(list)
    for r in draft:
        for w, start in cap_words(r["text"]):
            if start or merge.key(w) in merge.COMMON or len(w) < 2:
                continue
            used[w] += 1
            if len(where[w]) < 3:
                where[w].append(r["id"])
        # lowercase words the game writes capitalized (case check), e.g. "neural net"
        for t in r["text"].split():
            w = re.sub(r"[^\w'-]", "", t)
            cap = w[:1].upper() + w[1:]
            # the game capitalizes this word mid-sentence (>= 80 %, >= 10 times): a game term or name
            if w and w.islower() and len(w) > 3 and mid[cap] >= 10 and mid[cap] >= 4 * mid[w]:
                used[w] += 1
                if len(where[w]) < 3:
                    where[w].append(r["id"])

    game_words = [w for w, n in game.items() if n >= 2 and not w.isupper()]
    known_lower = {w.lower() for w in list(game) + list(script) + list(fls)}
    rows = []
    for w, n in used.most_common():
        base = re.sub(r"'s$", "", w)
        if w.isupper() and len(w) > 2:
            cls, ref = "caps", ""  # ALL-CAPS line (Neural Net voice lines as FLSubs wrote them)
        elif w in game or base in game:
            cls, ref = "ok", w
        elif w.islower() and mid[w[:1].upper() + w[1:]] >= 4 * mid[w]:
            cls, ref = "case", w[:1].upper() + w[1:]
        elif w.lower() in game_lower:
            ref = game_lower[w.lower()].most_common(1)[0][0]
            cls = "ok" if ref == w or w.islower() else "case"
        elif w.lower() in known_lower or base.lower() in known_lower:
            cls, ref = "script", ""  # in the script / FLSubs: a word of the dialogue, not a game name
        else:
            near = [g for g in game_words if g[0].lower() == w[0].lower() and
                    merge.difflib.SequenceMatcher(a=merge.key(w), b=merge.key(g)).ratio() >= 0.8]
            near.sort(key=lambda g: -game[g])
            cls, ref = ("variant", near[0]) if near else ("unknown", "")
        rows.append((cls, w, n, ref, game.get(ref, 0) if ref else 0, script.get(w, 0), fls.get(w, 0),
                     " ".join(where[w])))

    # multi-word terms: capitalization variants in the draft
    term_rows = []
    blob = "\n".join(r["text"] for r in draft)
    for t in TERMS:
        forms = collections.Counter(m.group(0) for m in re.finditer(re.escape(t), blob, re.I))
        gforms = collections.Counter(m.group(0) for m in re.finditer(
            re.escape(t), "\n".join(strip_rdl(l) for l in (out / "game_text.tsv").read_text(encoding="utf-8").splitlines()), re.I))
        if len(forms) > 1 or (forms and gforms and forms.most_common(1)[0][0] != gforms.most_common(1)[0][0]):
            term_rows.append((t, dict(forms), dict(gforms.most_common(3))))
    return rows, term_rows


def odd_names(rows) -> set:
    """The words the game's text and the script do not explain (the review tool's "Names the game doesn't know")."""
    return {r[1] for r in rows if r[0] in ("variant", "unknown")}


def write_reports(out: Path, rows, term_rows) -> list:
    """Writes <out>/name_audit.tsv and name_audit.md; returns the markdown lines."""
    with open(out / "name_audit.tsv", "w", encoding="utf-8", newline="\n") as fo:
        fo.write("class\tword\tuses\tgame spelling\tgame count\tscript count\tFLSubs count\texamples\n")
        for r in rows:
            fo.write("\t".join(str(x) for x in r) + "\n")
    by = collections.Counter(r[0] for r in rows)
    md = ["# Proper-noun audit", "", f"{len(rows)} distinct capitalized words used mid-sentence in the draft: " +
          ", ".join(f"{k} {v}" for k, v in by.most_common()), ""]
    for cls in ("variant", "case", "unknown", "caps", "script"):
        sel = [r for r in rows if r[0] == cls]
        md += [f"## {cls} ({len(sel)})", "", "| word | uses | game spelling | game | script | FLSubs | e.g. |",
               "|---|---|---|---|---|---|---|"]
        md += [f"| {r[1]} | {r[2]} | {r[3]} | {r[4]} | {r[5]} | {r[6]} | {r[7].split()[0] if r[7] else ''} |"
               for r in sel[:80]]
        md.append("")
    md += ["## Game terms: capitalization in the draft vs the game", "", "| term | draft forms | game forms |",
           "|---|---|---|"]
    md += [f"| {t} | {f} | {g} |" for t, f, g in term_rows]
    (out / "name_audit.md").write_text("\n".join(md) + "\n", encoding="utf-8")
    return md


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--script", required=True)
    ap.add_argument("--flsubs")
    a = ap.parse_args()
    out = Path(a.out)
    texts = []
    if a.flsubs:
        import flsubs
        texts = [t for t, _, _ in flsubs.load(a.flsubs).values()]
    rows, term_rows = compute(out, Path(a.script), texts)
    md = write_reports(out, rows, term_rows)
    by = collections.Counter(r[0] for r in rows)
    print("\n".join(md[:3]))
    for cls in ("variant", "case", "unknown", "script"):
        print(cls, by.get(cls, 0))


COMMON_LOWER = set("""about after again against base bases border cargo cruise dock docking engine engines field gate gates
hole holes lane lanes mining ring ship ships station stations system systems planet planets outpost outposts
research battleship cruiser cruisers fighter fighters transport transports pirates pirate police navy order
guild house houses""".split())

if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    main()
