"""Merge all text sources into the subtitle draft (the merge rules of RESEARCH.md section 9).

    merge.py --out <dir> --corrections <corrections.tsv> --glossary <glossary.txt>
    reads <dir>/index.tsv, aligned.tsv, classified.tsv, gamenames.tsv
    writes <dir>/draft.tsv, <dir>/review_high.tsv, <dir>/review_low.tsv

Priority, highest first:
 1. User corrections (data/corrections.tsv). Then judged decisions for low-priority lines
    (data/auto_decisions.tsv: script / recording text / unsure, each with a reason).
 2. Script lines: the script text. Where FLSubs and speech to text agree against it (the actor said
    something else) the recording's wording, written as FLSubs has it, with the script's spelling of
    names; a name replaced by a different name ("Doyle" -> "Sam") keeps the script and goes to review.
 3. Lines without script: speech to text, merged word by word with FLSubs: names and game terms from
    FLSubs, ordinary words from speech to text; then spelling of names fixed from the game's own name
    tables (the names this fragment refers to via msg_id_prefix).
 4. Global name rules (data/name_rules.tsv, next to the corrections) on every final text, corrections
    included: "Doyle" -> "Sam" (Lonnigan), "Daam K'Vosh" -> "Dom'Kavash".
 5. Fragments of composed chatter get the fragment form: no end punctuation, lowercase start unless
    the first word is a name, numbers as digits.
Review: high = undecided conflicts; low = script kept although speech to text heard other words
(FLSubs sided with the script: usually right, but not always).
"""
import argparse
import collections
import difflib
import hashlib
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))  # run with python -I: no implicit script dir
from tsvio import atomic_open, atomic_write, read_tsv  # noqa: E402

STOP = {"a", "an", "the", "to", "of", "in", "on", "at", "for", "from", "and", "or", "is", "are", "be", "by", "with",
        "welcome", "station", "base", "planet", "system", "border", "outpost", "depot", "the"}
# Capitalized words that are not names (sentence starts, interjections)
COMMON = set("""a an the i im ive ill id we our us you your it its this that these those he she they them there here
what how why who where when now but and so well yes no ok okay oh ah hey look listen let lets go come get good great
nice right sir mister mr doctor dr my me his her their if then just thanks thank sorry please wait stop hold all
damn dammit hell god""".split())
DIALOGUE = re.compile(r"^dx_", re.I)  # story and mission dialogue (script lines)
GAME_AREA = re.compile(r"_((?:li|br|rh|ku|bw|iw|ew|hi)\d\d_\d\d)", re.I)


def tsv(path: Path, skip_comments=True):
    """Header + rows TSV as dicts; a bad row or bad bytes are reported with file and line, never a crash (tsvio)."""
    return read_tsv(path, skip_comments)


def key(word: str) -> str:
    k = re.sub(r"[^a-z0-9]", "", word.lower().replace("’", "'"))
    return {"mr": "mister", "dr": "doctor"}.get(k, k)


def conv_form(text: str) -> str:
    """Lower case with the spellings a style rule decides folded - curly apostrophe, "alright" = "all right",
    hyphens as spaces: the review tool's word comparisons (same_words, rule_settled)."""
    return re.sub(r"\balright\b", "all right", text.lower().replace("’", "'").replace("-", " "))


def squash(text: str) -> str:
    """Letters and digits only, lower case: the comparison form of a line's words (punctuation and case aside)."""
    return re.sub(r"[^a-z0-9]", "", text.lower())


def strip_final_period(text: str) -> str:
    """The text without its sentence-ending "." (not an ellipsis); the runtime adds that period (user's rule)."""
    return re.sub(r"(?<!\.)\.$", "", text.strip())


def similar(a: str, b: str, cut=0.6) -> bool:
    return difflib.SequenceMatcher(a=key(a), b=key(b)).ratio() >= cut


def name_match(a: str, b: str, cut=0.75) -> bool:
    """Same name, misspelled: close spelling, or the same letters reordered ("Tegaki" / "Tekagi")."""
    ka, kb = key(a), key(b)
    return difflib.SequenceMatcher(a=ka, b=kb).ratio() >= cut or (len(ka) >= 4 and sorted(ka) == sorted(kb))


class Names:
    """Game names (gamenames.tsv) + glossary words; per-fragment name lists via msg_id_prefix."""

    def __init__(self, gamenames: list, glossary: str):
        self.by_prefix = {}
        self.full = {}  # prefix -> (name, short name)
        self.vocab = set()
        self.cased = {}  # key -> the game's capitals for a name word ("lsf" -> "LSF")
        self.ordinary = set()  # name words that are also everyday words ("space", "ship"): see learn_ordinary
        for r in gamenames:
            ws = [w for n in (r["name"], r["short"]) for w in n.split() if w]
            self.by_prefix[r["prefix"].lower()] = ws
            self.full[r["prefix"].lower()] = (r["name"], r["short"])
            self.vocab.update(key(w) for w in ws if w[:1].isupper())
            for w in ws:
                if w[:1].isupper() and key(w) not in STOP and key(w) not in COMMON and key(w) not in GENERIC:
                    self.cased.setdefault(key(w), re.sub(r"[^\w'-]+$", "", w))
        for term in glossary.split("Vocabulary:", 1)[-1].split(","):
            for w in term.split():
                w = re.sub(r"[^\w'-]+$", "", w)
                if w[:1].isupper() and key(w) not in STOP and key(w) not in COMMON and key(w) not in GENERIC:
                    self.cased.setdefault(key(w), w)
        gl = glossary.split("Vocabulary:", 1)[-1]
        for term in gl.split(","):
            for w in term.split():
                if w[:1].isupper():
                    self.vocab.add(key(w))
        self.vocab -= STOP | COMMON | {""}  # "The Order" must not make "The" a name

    def for_id(self, lid: str) -> list:
        """Name words a fragment refers to: by msg_id_prefix, or by the base/system code in the id."""
        low = re.sub(r"(formationdesig_)0+(\d)", r"\1\2", lid.lower())  # "_07" = designation 7
        best = []
        for p, ws in self.by_prefix.items():
            if low.startswith(p) and len(p) > len(best[0] if best else ""):
                best = [p, ws]
        if best:
            return best[1]
        m = GAME_AREA.search(lid)
        if m:
            return self.by_prefix.get(f"gcs_refer_base_{m.group(1).lower()}_base", [])
        m = re.search(r"_((?:fc|li|br|rh|ku|co|gd|pi)_[a-z]+)_grp$", low)
        if m:
            return self.by_prefix.get(f"gcs_refer_faction_{m.group(1)}", [])
        return []

    def learn_ordinary(self, texts):
        """Words speech to text writes lowercase mid-sentence more often than capitalized are everyday
        words, even if a game name contains them ("Deep Space Engineering" vs "launch to space")."""
        low, cap = {}, {}
        for text in texts:
            toks = text.split()
            for i, t in enumerate(toks[1:], 1):
                if re.search(r"[.!?…]$", toks[i - 1]):
                    continue
                w = re.sub(r"[^\w'-]", "", t)
                if not w:
                    continue
                d = cap if w[:1].isupper() else low
                d[key(w)] = d.get(key(w), 0) + 1
        self.ordinary = {k for k in self.cased if low.get(k, 0) > cap.get(k, 0)}

    def faction_names(self, lid: str):
        """(name, short name) of the faction a gcs_refer_faction_* fragment speaks."""
        low = lid.lower()
        best = max((p for p in self.full if low.startswith(p)), key=len, default=None)
        return self.full[best] if best else ("", "")

    def namelike(self, tokens: list, first_index: int, context: list = None) -> bool:
        """Name or game term: in the vocabulary, or capitalized mid-sentence (not after . ! ?)."""
        for i, t in enumerate(tokens):
            k = key(t)
            if k in self.vocab:
                return True
            pos = first_index + i
            prev = (context[pos - 1] if context and pos > 0 else (tokens[i - 1] if i > 0 else ""))
            sentence_start = pos == 0 or re.search(r"[.!?…]$", prev) is not None
            if t[:1].isupper() and not sentence_start and k not in COMMON:
                return True
        return False


def fix_names(text: str, names: list) -> str:
    """Respell words that sound like one of the given name words ("Altina" -> "Altona"); also joins
    a split mishearing ("boon shoe" -> "Bundschuh")."""
    if not names:
        return text
    toks = text.split()
    out, i = [], 0
    while i < len(toks):
        done = False
        for n in names:
            if len(key(n)) < 4:
                continue
            for span in (2, 1):
                if i + span > len(toks):
                    continue
                # a two-word mishearing ("boon shoe") never includes a function word or a word that is
                # already right ("to Yonagi", "Station Perna")
                if span == 2 and any(key(t) in STOP or key(t) in {key(x) for x in names} or t[:1].isupper()
                                     for t in toks[i:i + 2]):
                    continue
                cand = "".join(key(t) for t in toks[i:i + span])
                if cand != key(n) and difflib.SequenceMatcher(a=cand, b=key(n)).ratio() >= 0.7:
                    tail = re.search(r"[^\w']*$", toks[i + span - 1]).group(0)
                    out.append(n + tail)
                    i += span
                    done = True
                    break
            if done:
                break
        if not done:
            t = toks[i]
            core = re.sub(r"[^\w'-]+$", "", t)
            for n in names:  # right word, wrong case: "independent" -> "Independent"
                if len(key(n)) >= 4 and key(core) == key(n) and core != n:
                    t = n + t[len(core):]
                    break
            out.append(t)
            i += 1
    return " ".join(out)


def word_merge(asr: str, fl: str, names: Names):
    """FLSubs text as the base (human punctuation and capitals). Where the words differ: names and game
    terms from FLSubs, ordinary words from speech to text (FLSubs has typos: "peace of parchment").
    Returns (text, unresolved conflict?)."""
    a, f = asr.split(), fl.split()
    ka, kf = [key(t) for t in a], [key(t) for t in f]
    out, conflict = [], False

    def with_punct(words: list, like: list) -> list:
        """ASR words, but keep FLSubs' punctuation after the segment."""
        if not words:
            return words
        tail = re.search(r"[^\w']*$", like[-1]).group(0) if like else ""
        return words[:-1] + [re.sub(r"[^\w']*$", "", words[-1]) + tail]

    ops = difflib.SequenceMatcher(a=ka, b=kf, autojunk=False).get_opcodes()
    # a word the two only put in a different place ("should have just told" / "should just have told")
    # shows up as an FLSubs-only insert plus a speech-to-text-only delete: count it once, where speech to
    # text has it (ordinary words follow speech to text)
    moved = collections.Counter(k for op, i1, i2, _, _ in ops if op == "delete" for k in ka[i1:i2])
    for op, i1, i2, j1, j2 in ops:
        if op == "equal":
            out.extend(f[j1:j2])
            continue
        if op == "insert" and all(moved[k] > 0 for k in kf[j1:j2]):
            for k in kf[j1:j2]:
                moved[k] -= 1
            continue
        fl_names = names.namelike(f[j1:j2], j1, f)
        asr_names = names.namelike(a[i1:i2], i1, a)
        if fl_names and not asr_names:
            out.extend(f[j1:j2])
        elif fl_names and asr_names:
            vocab_fl = any(key(t) in names.vocab for t in f[j1:j2])
            vocab_asr = any(key(t) in names.vocab for t in a[i1:i2])
            if vocab_asr and not vocab_fl:
                out.extend(with_punct(a[i1:i2], f[j1:j2]))
            else:
                out.extend(f[j1:j2])
                if not vocab_fl:
                    conflict = conflict or not all(similar(x, y) for x, y in zip(a[i1:i2], f[j1:j2]))
        elif op == "insert":
            out.extend(f[j1:j2])  # words only FLSubs heard (often quiet starts/ends): keep
        else:
            out.extend(with_punct(a[i1:i2], f[j1:j2]))
    text = add_commas(repair_marks(" ".join(out), asr), asr)
    return (text[:1].upper() + text[1:]) if fl[:1].isupper() else text, conflict


def repair_marks(text: str, asr: str) -> str:
    """FLSubs' punctuation landed mid-sentence where it splits the line elsewhere than speech to text does: a
    sentence mark before a lowercase word ("Trent leads. as they", "the feeling? you're"). Such a word gets
    speech to text's mark at that word instead (a sentence mark there capitalises the next word; none drops
    it). Found in the disagreed list, session 2: 20 of 229 merged lines."""
    t, a = text.split(), asr.split()
    stray_cap = lambda x, y: (not re.search(r"[.!?…:]$", x) and y[:1].isupper()  # noqa: E731
                              and key(y) in COMMON - {"i", "im", "ive", "ill", "id", "mister", "mr", "doctor", "dr", "god",
                                                    "the", "a", "an"})  # articles start names: The Ring
    if not any((re.search(r"[.!?]$", x) and y[:1].islower()) or stray_cap(x, y) for x, y in zip(t, t[1:])):
        return text
    amap = {}
    for op, i1, i2, j1, j2 in difflib.SequenceMatcher(a=[key(x) for x in t], b=[key(x) for x in a],
                                                       autojunk=False).get_opcodes():
        if op == "equal":
            amap.update({i1 + k: j1 + k for k in range(i2 - i1)})
    for i in range(len(t) - 1):
        if re.search(r"[.!?]$", t[i]) and t[i + 1][:1].islower() and not t[i].endswith("..."):
            mark = re.search(r"[^\w']*$", a[amap[i]]).group(0) if i in amap else ""
            t[i] = re.sub(r"[.!?]+$", "", t[i]) + mark
            if re.search(r"[.!?]$", mark):
                t[i + 1] = t[i + 1][:1].upper() + t[i + 1][1:]
        # a capital left over from FLSubs' sentence split, where speech to text has the word lowercase
        # mid-sentence ("on a mission, and It's for", "If You want")
        if stray_cap(t[i], t[i + 1]) and i + 1 in amap and a[amap[i + 1]][:1].islower():
            t[i + 1] = t[i + 1][:1].lower() + t[i + 1][1:]
    return " ".join(t)


def add_commas(text: str, asr: str) -> str:
    """FLSubs often leaves out commas: around a form of address ("Hello friend", "serious Trent") and between
    clauses ("to find it's"). Where speech to text puts a comma between two words that follow each other in
    both reads and FLSubs has no mark there, add it (user's contextual pass, session 2)."""
    t, a = text.split(), asr.split()
    ops = difflib.SequenceMatcher(a=[key(x) for x in t], b=[key(x) for x in a], autojunk=False).get_opcodes()
    for op, i1, i2, j1, j2 in ops:
        if op != "equal":
            continue
        for k in range(i2 - i1 - 1):  # word pairs inside an equal run
            i, x, y = i1 + k, t[i1 + k], a[j1 + k]
            # after a sentence's first word only for interjections and linking words ("Okay,", "Sorry,",
            # "Actually,"): "Sure you want to know?", "Damn Intel was wrong!" must keep no comma
            first = i == 0 or re.search(r"[.!?…]$", t[i - 1]) is not None
            if first and not (key(x) in INTERJECTIONS | OPENERS or key(x).endswith("ly")):
                continue
            if y.endswith(",") and re.search(r"[\w']$", x):
                t[i] = x + ","
    return " ".join(t)


def script_names(script_text: str, other: str, names: "Names"):
    """Write `other` with the script's spelling of names; report a name swapped for a different name.
    Names = words in the game/glossary vocabulary (sentence-start capitals like "We" are not names)."""
    s, o = script_text.split(), other.split()
    ks, ko = [key(t) for t in s], [key(t) for t in o]

    def starts_sentence(toks, i):
        return i == 0 or re.search(r"[.!?…]$", toks[i - 1]) is not None

    script_names_set = {re.sub(r"('s)?[^\w'-]*$", "", t) for i, t in enumerate(s)
                        if t[:1].isupper() and (key(t) in names.vocab or not starts_sentence(s, i))}
    out, swapped = [], False
    for op, i1, i2, j1, j2 in difflib.SequenceMatcher(a=ks, b=ko, autojunk=False).get_opcodes():
        if op == "replace" and (i2 - i1) == (j2 - j1) <= 2:
            for x, y in zip(s[i1:i2], o[j1:j2]):
                is_name = key(x) in names.vocab or (x[:1].isupper() and key(x) not in COMMON)
                if is_name and key(x) != key(y) and y[:1].isupper():
                    if similar(x, y):
                        out.append(re.sub(r"[^\w'-]+$", "", x) + re.search(r"[^\w']*$", y).group(0))
                    else:
                        swapped = True  # "Doyle" heard as "Sam": a person decides
                        out.append(y)
                else:
                    out.append(y)
        elif op != "delete":
            out.extend(o[j1:j2])  # long rewrites: as recorded
    # respell names inside rewritten passages with the script's spelling ("Tegaki's" -> "Tekagi's")
    fixed = []
    for t in out:
        core = re.sub(r"('s)?[^\w'-]*$", "", t)
        for n in script_names_set:
            # only names (capitalized words) are respelled: "for"/"four" must not become "Fort" (Fort Bush)
            if core[:1].isupper() and core != n and key(core) != key(n) and len(key(n)) >= 4 \
                    and len(key(core)) >= 4 and key(core) not in NUMWORD and key(core) not in COMMON \
                    and name_match(core, n):
                t = n + t[len(core):]
                break
        fixed.append(t)
    return " ".join(fixed), swapped


NUMWORD = {w: str(i) for i, w in enumerate("zero one two three four five six seven eight nine ten eleven twelve "
                                          "thirteen fourteen fifteen sixteen seventeen eighteen nineteen twenty".split())}
GENERIC = {"station", "base", "outpost", "depot", "facility", "mining", "research", "planet", "shipyard", "battleship",
           "border", "prison", "the", "of", "freeport", "gas", "miner", "refinery", "plant", "smelter", "factory",
           "colony", "academy", "fort", "dock", "ring", "hospital", "ruins", "field", "jump", "hole", "gate", "system"}


def referent_text(lid: str, text: str, names: "Names") -> str:
    """Fragments that name one game object: text from the game's name, shaped like what was spoken."""
    low = lid.lower()
    ws = names.for_id(lid)
    if not ws:
        return text
    if low.startswith("gcs_refer_formationdesig_"):
        return " ".join(ws)
    if low.startswith(("gcs_refer_system_", "gcs_refer_faction_", "gcs_gen_commodity_", "gcs_refer_shiparch_")):
        # one known system / faction / commodity / ship type: the game's name in the spoken frame
        # ("the Tau-29 system", "the Ageira"); faction "_short" fragments speak the short name
        name = " ".join(ws)
        if low.startswith("gcs_refer_faction_"):
            full, short = names.faction_names(lid)
            name = short if "_short" in low and short else full or name
        t = text.lower()
        lead = "the " if re.match(r"\s*(the|a)\b", t) and not name.lower().startswith("the ") else ""
        tail = " system" if "system" in t and "system" not in name.lower() else ""
        spoken = key(re.sub(r"^\s*(the|a)\s+|\s*system\W*$", "", t))
        if spoken == key(name) + "s" and not name.endswith("s"):
            name += "s"  # spoken plural: "Basic Alloys"
        return lead + name + tail
    if not low.startswith(("gcs_refer_base_", "gcs_refer_system_", "gcs_refer_faction_", "gcs_gen_commodity_")):
        return fix_names(text, ws)
    distinct = [w for w in ws if key(w) not in GENERIC and len(key(w)) >= 1 and not key(w) in STOP]
    lead = "the " if text.lower().startswith("the ") and key(ws[0]) != "the" else ""
    # whole name heard (spacing aside): "Sidearms" = "Side Arms", "LD14" = "LD-14"
    if difflib.SequenceMatcher(a=key(text.replace("the ", "", 1) if lead else text), b=key("".join(ws))).ratio() >= 0.8:
        return lead + " ".join(ws)
    toks, out, hit, used = text.split(), [], False, set()
    for t in toks:
        kt = NUMWORD.get(key(t), key(t))
        if (kt in STOP or kt in GENERIC) and kt not in {key(w) for w in ws}:
            out.append(t)  # "the", "system", "planet": kept as spoken
            continue
        cands = [w for w in ws if w not in used]
        best = max(cands, key=lambda w: difflib.SequenceMatcher(a=kt, b=key(w)).ratio()) if cands else ""
        r = difflib.SequenceMatcher(a=kt, b=key(best)).ratio() if best else 0
        tail = re.search(r"[^\w'-]*$", t).group(0)
        if best and (r >= 0.45 and len(kt) >= 3 or kt == key(best)):
            out.append(best + tail)
            used.add(best)
            hit = hit or best in distinct
        elif key(t) in GENERIC or key(t) in STOP or len(key(t)) < 3 or t[:1].isdigit():
            out.append(t)
        elif not low.startswith("gcs_refer_base_"):
            out.append(t)
        # base fragments: drop words that match nothing ("Edison" in "Edison Trenton")
    if low.startswith("gcs_refer_base_") and not hit and distinct:
        return " ".join(distinct)  # mishearing beyond repair ("bowman"): the base's own name
    return " ".join(out) if out else " ".join(ws)


def sentence_case(text: str, names: "Names", partial: bool = False) -> str:
    """ALL-CAPS lines (Neural Net voice lines as FLSubs wrote them) in normal sentence case; names and
    acronyms keep the game's capitals ("LSF", "Manhattan"). partial: also lowercase stray all-caps words
    in otherwise normal lines (not used for script dialogue, where caps mark emphasis: "We've been HIT!")."""
    letters = [c for c in text if c.isalpha()]
    if len(letters) < 4 or sum(c.isupper() for c in letters) < 0.8 * len(letters):
        if not partial:
            return text
        # mixed line (FLSubs caps merged with speech to text): lowercase stray all-caps words, keep acronyms
        def low(m):
            w = m.group(0)
            return w if key(w) in names.cased and names.cased[key(w)].isupper() else w.lower()
        return re.sub(r"\b[A-Z]{3,}\b", low, text)
    out, start = [], True
    for t in text.split():
        core = re.sub(r"^[^\w']+|[^\w']+$", "", t)
        k = key(core)
        if k in names.cased and k not in names.ordinary:
            new = names.cased[k]
        elif k in ("i", "im", "ill", "ive", "id"):
            new = core.lower().capitalize()
        else:
            new = core.lower()
            if start:
                new = new[:1].upper() + new[1:]
        out.append(t.replace(core, new, 1) if core else t)
        start = re.search(r"[.!?…]$", t) is not None or t in ("-", "–")
    return " ".join(out)


def apply_continuity(text: str, op: dict, names: "Names") -> str:
    """data/continuity.tsv: split-sentence clips. end: put this punctuation at the end ("." "!" "–"; a
    trailing "..."/comma is replaced by "–", a missing mark is added); start=lower: the clip continues
    the previous one: strip a leading "..." and start lowercase (names and "I" keep their capital)."""
    t = text.strip()
    end = op.get("end", "")
    if end == "–":
        t = re.sub(r"[\s.,;:!?…–-]*$", "", t) + "–"
    elif end and re.search(r"[A-Za-z0-9,]$", t):
        t = re.sub(r",$", "", t) + end
    if op.get("start") == "lower":
        t = re.sub(r"^\.\.\.\s*", "", t)
        first = re.match(r"[A-Za-z']+", t)
        if first and key(first.group(0)) not in names.vocab and first.group(0) not in ("I", "I'm", "I'll", "I've", "I'd"):
            t = t[:1].lower() + t[1:]
    return t


def load_name_rules(path: Path) -> list:
    """data/name_rules.tsv: [(compiled regex, replacement)] applied to every final subtitle."""
    if not path.exists():
        return []
    return [(re.compile(r["pattern"], re.I), r["replacement"], r.get("note", "")) for r in tsv(path) if r.get("pattern")]


def apply_name_rules(text: str, rules: list) -> str:
    for rx, rep, *_ in rules:
        text = rx.sub(rep, text)
    return text


def apply_style_rules(text: str, rules: list) -> str:
    """data/style_rules.tsv: like name rules, but a lowercase replacement takes the capital of what it
    replaces ("Dammit!" -> "Damn it!")."""
    for rx, rep, *_ in rules:
        def sub(m, rep=rep):
            if rep.startswith("="):  # "=text": written exactly as given, whatever the capital it replaces
                return m.expand(rep[1:])
            out = m.expand(rep)
            if rep[:1].islower() and m.group(0)[:1].isupper():
                out = out[:1].upper() + out[1:]
            return out
        text = rx.sub(sub, text)
    return text


def load_chatter_rules(path: Path) -> list:
    """data/chatter_rules.tsv: [(id regex, compiled pattern, replacement, note, voice regex or None)], NPC
    chatter and base speech only; the optional voices column limits a rule to some voices."""
    if not path.exists():
        return []
    return [(re.compile(r["ids"], re.I), re.compile(r["pattern"], re.I), r["replacement"], r.get("note", ""),
             re.compile(r["voices"], re.I) if r.get("voices") else None)
            for r in tsv(path) if r.get("pattern")]


def base_name_form(lid: str, text: str, names: "Names") -> str:
    """Base fragments: the game's own name for the base, whatever the actor called it ("Prison station
    Fuchu" -> "Fuchu Prison"); a spoken leading "the" stays (user, session 1)."""
    ws = names.for_id(lid)
    if not ws:
        return text
    name = " ".join(ws)
    lead = "the " if re.match(r"\s*the\b", text, re.I) and not name.lower().startswith("the ") else ""
    return lead + name


ASR_DIR = Path(__file__).resolve().parent
# The FLvibed repository root (this file: <root>/subtitles/review/FLSubtitles/asr/merge.py).
REPO_ROOT = ASR_DIR.parents[3]
# How Content.dll joins chatter pieces: a game fact, kept in FLvibed's docs with the other verified game facts.
GRAMMAR = REPO_ROOT / "docs" / "chatter_grammar.tsv"


def audio_md5(out: Path) -> dict:
    """(voice, id) -> MD5 of the clip's bytes, so byte-identical recordings (orphan bank copies, vanilla
    banks, a bank line that is also a story sound) are one decision. Cached in out/audio_md5.tsv by
    path, size and modification time."""
    cache_path, cache = out / "audio_md5.tsv", {}
    if cache_path.exists():
        for r in tsv(cache_path):
            cache[r["file"]] = (r["size"], r["mtime"], r["md5"])
    res, rows, missing = {}, [], 0
    for r in tsv(out / "index.tsv"):
        f = Path(r["file"])
        if not f.is_absolute():
            f = (ASR_DIR / f).resolve()
        try:
            st = f.stat()
        except OSError:
            # the clip is not on disk (the install moved or is absent): its cached fingerprint still names the
            # recording, so byte-identical copies stay one decision, and the cache keeps the row
            c = cache.get(str(f))
            if c:
                res[(r["voice"], r["id"])] = c[2]
                rows.append((str(f), *c))
                missing += 1
            continue
        size, mtime = str(st.st_size), str(int(st.st_mtime))
        c = cache.get(str(f))
        h = c[2] if c and c[:2] == (size, mtime) else hashlib.md5(f.read_bytes()).hexdigest()
        res[(r["voice"], r["id"])] = h
        rows.append((str(f), size, mtime, h))
    with atomic_open(cache_path) as fo:
        fo.write("file\tsize\tmtime\tmd5\n")
        for row in sorted(set(rows)):
            fo.write("\t".join(row) + "\n")
    if missing:
        print(f"warning: {missing} clips of index.tsv are not on disk (its paths point into the game install): "
              "their cached fingerprints are used", file=sys.stderr)
    return res


OPEN_SLOTS = {"^good", "^ship", "^number", "@mbid"}  # names the sentence runs straight on after


def load_grammar_roles(path: Path) -> list:
    """data/chatter_grammar.tsv (how Content.dll joins chatter pieces): for each piece pattern,
    (regex, nonfinal, continuation, start). nonfinal: in every sentence it appears in, more words follow
    without a sentence end (no end punctuation). continuation: in every sentence it continues one
    (starts lowercase). start: in every sentence it opens one (capital)."""
    if not path.exists():
        return []
    closed = {"@short", "^base-", "^dest-", "^number-"}  # slots that end a sentence
    roles = {}
    for r in tsv(path):
        toks = r["template"].split()
        for i, t in enumerate(toks):
            if t.startswith(("@", "^")):
                continue
            prev = toks[i - 1] if i else None
            # ends the sentence: last piece, or a "-" piece followed by a new sentence (not by a name that
            # completes it: "Looks like we're not the only ones headed to" + Fuchu Prison)
            final = i == len(toks) - 1 or (t.endswith("-") and not toks[i + 1].startswith(("@", "^")))
            nonfinal = not final
            # lowercase only where the words run straight on: after a good / ship / number, or after a piece
            # with no intonation mark ("to dock" + "is granted."); a "+" piece ("Attention, all wingmen",
            # "I'm not engaging") and a callsign are followed by a sentence of its own
            cont = prev is not None and (prev in OPEN_SLOTS or
                                         (not prev.startswith(("@", "^")) and not prev.endswith(("-", r"\+"))))
            start = prev is None or prev in closed or (not prev.startswith(("@", "^")) and prev.endswith("-"))
            a = roles.setdefault(t, [True, True, True, True, False])
            a[0] &= nonfinal
            a[1] &= cont
            a[2] &= start
            a[3] &= final
            a[4] |= len(toks) > 1  # joined with other pieces in at least one sentence
    return [(re.compile(t, re.I), nf, ct, st, fi, ch) for t, (nf, ct, st, fi, ch) in roles.items()]


# Base and bar speech: scenes in DATA\Scripts\GCS\*.thn play two voice pieces as one sentence (session 1):
#   GCS_*Stand_W02aF: $BGCS_Base_Name ("Welcome to Harris,") + $BGCS_Base_Run_By ("run by LWB.", "a Liberty Planet.")
#   GCS_*Stand_A03aG + A04aG: $BGCS_Faction_Rebuff ("The Corsairs don't") + A04aG ("waste any time ...")
#   GCS_*Stand_D06aG: GCS_ML_D06aG00 ("Oh, well, I work") + $BGCS_My_Faction ("for the Corsairs.")
# and the hailed ship's type in a base hail (Content.dll 0x06ED97E0): "Incoming" Rhino "trader", this is ...
# (regex, nonfinal, continuation, sentence start, end mark: "," / "." / None)
BASE_ROLES = [(re.compile(rx, re.I), nf, ct, st, end) for rx, nf, ct, st, end in [
    (r"GCS_(ML|FM)_W02aF_.*|\$BGCS_Base_Name", True, False, True, ","),
    (r"GCS_(ML|FM)_W02bF\d+|\$BGCS_Base_Run_By", False, True, False, "."),
    (r"GCS_(ML|FM)_A03aG_.*_grp|\$BGCS_Faction_Rebuff_(ML|FM)", True, False, True, None),
    (r"GCS_(ML|FM)_A04aG\d+", False, True, False, "."),
    (r"GCS_(ML|FM)_D06aG00", True, False, True, None),
    (r"GCS_(ML|FM)_D06aG_.*_grp|\$BGCS_My_Faction_(ML|FM)", False, True, False, "."),
    (r"gcs_dhail_mbid_.*", True, True, False, None),
]]


GENERIC_LINE = re.compile(r"^(gcs_|rms_|\$BGCS_|GCS_)", re.I)


def apply_grammar(lid: str, text: str, roles: list, names: "Names") -> str:
    """End mark and first-letter case of a chatter / base speech piece from its place in the game's
    sentences (data/chatter_grammar.tsv, BASE_ROLES). The game shows no text; the subtitle runtime joins
    the pieces of a sentence and adds the sentence's end mark itself (user, session 1). So:
    - no line the runtime assembles (chatter, names, base/bar speech, Trent's replies; one piece or
      several) carries a sentence-ending "." ("Planet Manhattan" ends "...to Planet Manhattan" as well as
      names a place mid-sentence); "?", "!" and "..." stay (tone), and so does the joining comma
      ("Welcome to Harris,");
    - a piece the game follows with more words carries no "!" either ("I'm requesting" + "to dock");
    - a piece that continues a sentence starts lowercase ("...full of Gold" + "fetches a good price"),
      one that always opens a sentence starts with a capital ("Acknowledged").
    Story and mission dialogue (dx_) and the Neural Net voice are whole subtitles and keep their marks."""
    base = [(nf, ct, st, end) for rx, nf, ct, st, end in BASE_ROLES if rx.fullmatch(lid)]
    chat = [(nf, ct, st, ch) for rx, nf, ct, st, fi, ch in roles if rx.fullmatch(lid)]
    hits = base or chat
    nonfinal = lid.endswith("+") or (hits and all(h[0] for h in hits))
    cont = hits and all(h[1] for h in hits)
    start = hits and all(h[2] for h in hits)
    # generic lines the runtime assembles (radio chatter, random-mission taunts, names, base/bar speech,
    # Trent's bar replies); mission characters' own lines (nag_, bc_, rmb_, mb_ ...) are unique dialogue
    chunk = bool(hits) or bool(GENERIC_LINE.match(lid))
    t = text.strip()
    if not chunk:
        return t
    if not is_joined(lid, roles):
        # a whole sentence on its own (a comm of one piece, a taunt, a bar line, Trent's reply): it keeps
        # its end mark like a story line (user, session 2)
        if t and not re.search(r"[.!?…–-]$", t):
            t += "."
        return t
    if nonfinal:
        # the game runs straight on into the next piece: no end mark, and no "..." either: FLSubs used it
        # to say the clip stops mid-sentence ("Ah well, I work..." + "for the Corsairs"), not a pause (user).
        # A whole line that trails off ("...or else...") is not a joined piece and keeps it.
        t = re.sub(r"\s*(?:\.\.\.|…|[.!,])$", "", t).rstrip()
    else:
        t = re.sub(r"(?<![.…])[.,]$", "", t)  # the sentence's "." is added when the pieces are joined
    if start:  # the piece opens the sentence
        t = t[:1].upper() + t[1:]
    if cont:
        first = re.match(r"[A-Za-z']+", t)
        if first and key(first.group(0)) not in names.vocab and first.group(0) not in ("I", "I'm", "I'll", "I've", "I'd"):
            t = t[:1].lower() + t[1:]
    if base and base[0][3] == ",":
        t = re.sub(r"\s*[.!,;:]*$", "", t) + ","
    return t


def is_joined(lid: str, roles: list) -> bool:
    """A generic line the game joins with other pieces into a sentence (Content.dll chatter with more
    than one piece, the GCS base-speech pairs, names and numbers): it carries no sentence-ending period;
    the runtime adds it. Everything else generic is a whole sentence of its own."""
    return (any(rx.fullmatch(lid) for rx, *_ in BASE_ROLES) or bool(FRAGMENT.search(lid))
            or any(r[5] for r in roles if r[0].fullmatch(lid)))


def render_sentence(parts: list) -> str:
    """The subtitle the runtime types out for one assembled message (chatter_grammar.tsv header): parts =
    [(chunk text, ends a sentence)]. Joined with single spaces, "1 dash 2" written "1-2", each sentence
    starting with a capital and ending with "." unless it ends with ? ! or ...; the stored chunks carry no
    sentence-ending period (user, session 2). The runtime DLL does the same (FLvibed subtitles/src/impl/impl.cpp
    JoinPieces; its `jointest` fixtures keep the two equal), including the rule of 22 Oct 2026 (user: "the period
    after the speaking NPC doesn't make sense when the next word is to"): a piece followed by one that starts with
    the word "to" continues the sentence - no period before it, "to" stays lower case ("This is Planet Manhattan to
    Alpha 1."). Preview only: this function feeds the review tool's "In game:" line, never data/subtitles.tsv."""
    out, new = "", True
    for k, (text, end) in enumerate(parts):
        t = text.strip()
        if not t:
            continue
        if new:
            t = t[:1].upper() + t[1:]
        out = f"{out} {t}" if out else t
        new = bool(re.search(r"[!?]$|(?<!\.)\.$", t))  # an ellipsis trails off inside the sentence
        if k + 1 < len(parts) and re.match(r"to(?![^\W\d_])", parts[k + 1][0].strip(), re.I):
            out = strip_final_period(out)  # as JoinPieces: the next piece goes on with "to"
            new = False
            continue
        if end:
            if not re.search(ENDED, out):
                out += "."
            new = True
    out = re.sub(r"\b(\d+) dash (\d+)\b", r"\1-\2", out, flags=re.I)
    if out and not re.search(ENDED, out):
        out += "."
    return out


ENDED = r"[.!?…–-]$"  # already ends: a mark, an ellipsis, or a cut-off dash ("Lost-"): no "." after it


def callsign_digits(text: str, names: "Names") -> str:
    """Callsign numbers as digits, in story lines too (user, session 2): "Gamma Six" -> "Gamma 6",
    "Alpha One Two" -> "Alpha 1-2"; an Omega system as the game writes it: "Omega Five system" -> "Omega-5
    system"."""
    desig = sorted({w for n in range(1, 30) for w in names.for_id(f"gcs_refer_formationdesig_{n}")}, key=len, reverse=True)
    if not desig:
        return text
    num = "|".join(sorted(NUMWORD, key=len, reverse=True))

    def sub(m):
        a = NUMWORD[m.group(2).lower()]
        b = NUMWORD[m.group(3).lower()] if m.group(3) else ""
        if m.group(1).lower() == "omega" and m.group(4):
            return f"{m.group(1)}-{a}{m.group(4)}"
        return f"{m.group(1)} {a}" + (f"-{b}" if b else "") + (m.group(4) or "")
    return re.sub(rf"\b(?-i:({'|'.join(desig)}))[\s-]+({num})\b(?:[\s-]+({num})\b)?(\s+system\b)?", sub, text, flags=re.I)


SPEAKER_KEEP_CAPS = {"LSF", "LPU", "SA", "CV", "SAR", "IMG", "GMG", "BMM", "LWB", "II", "III"}


def load_speakers(path: Path) -> dict:
    return {r["match"]: r["name"] for r in tsv(path)} if path.exists() else {}


def speaker_name(voice: str, raw: str, table: dict) -> str:
    """Display name for the runtime's speaker row: a named voice's one name everywhere (data/speakers.tsv
    voice:), else the shooting script's tag without stage notes ("(CONT'D)", "(O.S.)", "#"), title-cased
    with acronyms kept and fixed by speakers.tsv raw:. Ids and generic voices ship no name: the runtime
    uses the name the game shows for the sender."""
    if "voice:" + voice in table:
        return table["voice:" + voice]
    tag = re.sub(r"\s+", " ", re.sub(r"\([^)]*\)|#", "", raw)).strip()
    if not tag or tag == voice or re.search(r"[a-z]", tag):
        return ""
    if "raw:" + tag in table:
        return table["raw:" + tag]
    return " ".join(w if w in SPEAKER_KEEP_CAPS else "-".join(p.capitalize() for p in w.split("-"))
                    for w in tag.split())


def write_snapshot(path: Path, report: Path, rows: list, why: list, kind_of, secs: dict = None,
                   speakers: dict = None) -> None:
    """data/subtitles.tsv: the subtitle text as it stands (the product), one row per (id, voice), sorted,
    deterministic, committed with every change. Before overwriting it, the changes against the previous
    build are summarised (by source and by the rule that caused them) into out/subtitles_diff.txt and the
    console, so a rule that touches more than intended shows up at once. Unused hash-named vanilla
    copies and scene mixes (no timed cues yet) are left out."""
    keep = [i for i, r in enumerate(rows) if not r[0].lower().startswith(("0x", "mix_"))]
    keep.sort(key=lambda i: (rows[i][0].lower(), rows[i][1].lower()))
    old = {}
    if path.exists():
        for r in tsv(path):
            old[(r["id"], r["voice"])] = r["text"]
    changed, added, lines = collections.Counter(), 0, []
    samples = collections.defaultdict(list)
    for i in keep:
        k, t = (rows[i][0], rows[i][1]), rows[i][3]
        if k not in old:
            added += 1
        elif old[k] != t:
            cause = why[i].split("; ")[0] if why[i] else rows[i][4]
            changed[cause] += 1
            if len(samples[cause]) < 5:
                samples[cause].append(f"    {k[0]} {k[1]}: {old[k]!r} -> {t!r}")
    removed = len(set(old) - {(rows[i][0], rows[i][1]) for i in keep})
    total = sum(changed.values())
    lines.append(f"subtitles.tsv: {len(keep)} lines, {total} changed, {added} new, {removed} gone")
    # the previous build may itself be uncommitted work: also count against the last commit
    try:
        head = subprocess.run(["git", "show", "HEAD:./" + path.name], cwd=path.parent, capture_output=True,
                              text=True, encoding="utf-8").stdout
        committed = {(r[0], r[1]): r[-1] for r in (l.split("\t") for l in head.splitlines())
                     if len(r) >= 5 and not r[0].startswith("#")}
        if committed:
            since = sum(1 for i in keep if committed.get((rows[i][0], rows[i][1]), rows[i][3]) != rows[i][3])
            lines.append(f"  since the last commit: {since} lines differ")
    except OSError:
        pass
    for cause, n in changed.most_common():
        lines.append(f"  {n:5d}  {cause}")
        lines.extend(samples[cause])
    atomic_write(report, "\n".join(lines) + "\n", newline=None)  # platform line ends, as before
    print("\n".join(lines[:1] + [l for l in lines[1:] if not l.startswith("    ")][:12]))
    with atomic_open(path) as fo:
        fo.write("# FLSubtitles subtitle text (GPL-3.0: contains FLSubs text). Generated by asr/merge.py, do not edit:\n"
                 "# change data/corrections.tsv or the rules instead. kind: story = shown as saved; chunk = joined\n"
                 "# into sentences by the runtime (no sentence-ending period); unique = mission character line;\n"
                 "# nn = Neural Net message. sec = clip length (s); speaker = display name (data/speakers.tsv;\n"
                 "# empty: the runtime uses the sender's in-game name).\n")
        fo.write("id\tvoice\tkind\tsource\tsec\tspeaker\ttext\n")
        for i in keep:
            sec = (secs or {}).get((rows[i][1], rows[i][0]), 0.0)
            who = speaker_name(rows[i][1], rows[i][2], speakers or {})
            fo.write(f"{rows[i][0]}\t{rows[i][1]}\t{kind_of(i)}\t{rows[i][4]}\t{sec:.2f}\t{who}\t{rows[i][3]}\n")


UK_US = {"artefact": "artifact", "artefacts": "artifacts", "defence": "defense", "rumours": "rumors",
         "rumour": "rumor", "analysing": "analyzing", "manoeuvre": "maneuver", "saviour": "savior",
         "colour": "color", "favour": "favor", "honour": "honor", "armour": "armor", "centre": "center"}


def spelling_key(t: str) -> str:
    """A word as written, spelling variants aside: case, hyphens, British spelling ("Xeno-Archeology")."""
    k = re.sub(r"[^a-z0-9']", "", t.lower())
    return UK_US.get(k, k).replace("archeolog", "archaeolog")


def names_only(script: str, other: str, names: "Names") -> bool:
    """`other` says the script's words except for names (the game's / glossary's names, or capitalized
    words inside a sentence): a read that could not know the name ("Donow" for the ship "Donau",
    "Ryland's" for "Rheinland's") rather than an actor who said something else."""
    s, o = script.split(), other.split()
    ks, ko = [spelling_key(t) for t in s], [spelling_key(t) for t in o]
    for op, i1, i2, j1, j2 in difflib.SequenceMatcher(a=ks, b=ko, autojunk=False).get_opcodes():
        if op == "equal":
            continue
        if op != "replace":
            return False
        for i in range(i1, i2):
            core = re.sub(r"^[^\w']+|[^\w']+$", "", s[i])
            sentence_start = i == 0 or re.search(r"[.!?…]$", s[i - 1])
            if not (key(core) in names.vocab or (core[:1].isupper() and key(core) not in COMMON and not sentence_start)):
                return False
    return True


EXPAND = {
    "i'm": "i am", "you're": "you are", "we're": "we are", "they're": "they are", "it's": "it is",
    "that's": "that is", "what's": "what is", "there's": "there is", "here's": "here is", "he's": "he is",
    "she's": "she is", "who's": "who is", "where's": "where is", "don't": "do not", "doesn't": "does not",
    "didn't": "did not", "can't": "cannot", "won't": "will not", "isn't": "is not", "aren't": "are not",
    "wasn't": "was not", "weren't": "were not", "haven't": "have not", "hasn't": "has not",
    "hadn't": "had not", "couldn't": "could not", "wouldn't": "would not", "shouldn't": "should not",
    "i'll": "i will", "you'll": "you will", "we'll": "we will", "they'll": "they will", "he'll": "he will",
    "she'll": "she will", "it'll": "it will", "that'll": "that will", "i'd": "i would", "you'd": "you would",
    "we'd": "we would", "they'd": "they would", "he'd": "he would", "she'd": "she would", "i've": "i have",
    "you've": "you have", "we've": "we have", "they've": "they have", "gonna": "going to",
    "wanna": "want to", "gotta": "got to", "'em": "them", "em": "them", "ya": "you", "'cause": "because",
    "cause": "because", "lemme": "let me", "gimme": "give me", "kinda": "kind of", "outta": "out of",
    "dunno": "do not know", "c'mon": "come on", "'til": "until", "til": "until", "can not": "cannot",
}


def style_tokens(text: str) -> list:
    """The words of a read with contractions and casual forms written out and numbers as digits: two reads
    with the same style tokens say the same thing, formally or informally."""
    out = []
    for t in re.findall(r"[a-z0-9']+", text.lower().replace("’", "'")):
        if t in NUMWORD:
            out.append(NUMWORD[t])
        elif t in EXPAND:
            out.extend(EXPAND[t].split())
        else:
            out.append(t.strip("'"))
    return out


def casualness(text: str) -> int:
    return sum(1 for t in re.findall(r"[a-z0-9']+", text.lower().replace("’", "'")) if t in EXPAND)


def form_pick(reads: list):
    """Reads that differ only in contractions, casual forms or number words ([(label, text)], the current
    draft first): the form speech to text heard (user's review of 35 such lines: right in 34), written as
    FLSubs writes it when FLSubs heard the same form. None when the reads say different words, differ in
    nothing, or there is no speech-to-text read."""
    reads = [(l, t) for l, t in reads if t and t.strip()]
    heard = dict(reads)
    if len(reads) < 2 or "asr" not in heard:
        return None
    base = style_tokens(reads[0][1])
    if any(style_tokens(t) != base for _, t in reads[1:]) or len({casualness(t) for _, t in reads}) < 2:
        return None  # other words, or no difference in form at all
    fl = heard.get("flsubs")
    return fl if fl and casualness(fl) == casualness(heard["asr"]) else heard["asr"]


WELCOME = re.compile(r"^GCS_(ML|FM)_W02aF_", re.I)


def welcome_form(text: str, ws: list) -> str:
    """A base intro ("Welcome to <base>,", GCS_ML_W02aF_<base>): the base's name words as the game writes
    them ("Erinmore" -> Arranmore, "Bond Station" -> Bonn Station, "Westfallen" -> Westfalen); the rest as
    spoken ("the", "Station")."""
    if not ws:
        return text
    plain = lambda w: key(w.replace("ç", "c").replace("Ç", "C"))  # noqa: E731
    distinct = [w for w in ws if key(w) not in GENERIC]
    m = re.match(r"(\s*welcome to\s+)(.*?)([,.!]*)$", text, re.I)
    if not m or not distinct:
        return text
    out, used = [], set()
    for t in m.group(2).split():
        core = re.sub(r"^[^\w']+|[^\w']+$", "", t)
        if key(core) in ("the", "a", "an", "of"):
            out.append(core.lower())  # "the Westfalen"
            continue
        if key(core) in GENERIC:
            out.append(core[:1].upper() + core[1:])  # "Station", "Luxury Liner"
            continue
        if key(core) in NUMWORD and NUMWORD[key(core)] in ws:
            out.append(NUMWORD[key(core)])  # "Freeport Six" -> Freeport 6, as the game writes it
            continue
        best = max((w for w in distinct if w not in used), key=lambda w: difflib.SequenceMatcher(a=plain(core), b=plain(w)).ratio(), default=None)
        if best and difflib.SequenceMatcher(a=plain(core), b=plain(best)).ratio() >= 0.45:
            out.append(best)
            used.add(best)
        else:
            out.append(core)
    return m.group(1) + " ".join(out) + m.group(3)


EXPAND_KEY = {key(k): v for k, v in EXPAND.items()}
INTERJECTIONS = {"ah", "oh", "uh", "um", "hmm", "well", "hey", "so", "now", "yeah", "yes", "okay", "ok", "alright",
                 "right", "look", "listen", "huh", "eh"}
# sentence openers that take a comma (add_commas)
OPENERS = {"sorry", "no", "please", "thanks", "besides", "anyway", "still", "however", "perhaps", "maybe", "indeed",
           "regardless", "otherwise", "meanwhile", "unfortunately", "fortunately", "actually", "frankly", "honestly"}


def context_pick(draft: str, reads: list, sibs: dict, own_family: str, vocab: set = frozenset(), place_only=False):
    """sibs = {family: [reads of this line id by that actor]}. The read (speech to text or FLSubs) that
    matches another actor's version best, if it beats the draft clearly: at least 0.9 similar to some
    other actor's read and 0.1 more than the draft is. Needs two other actors (one could share a slip)."""
    others = [t for f, ts in sibs.items() if f != own_family for t in ts if t]
    if len({f for f, ts in sibs.items() if f != own_family and any(ts)}) < 2:
        return None
    words = lambda t: [key(w) for w in t.split() if key(w)]  # noqa: E731
    sib_words = [words(t) for t in others]

    def score(t):
        w = words(t)
        return max((difflib.SequenceMatcher(a=w, b=s, autojunk=False).ratio() for s in sib_words), default=0)
    base = score(draft)
    best = max((t for t in reads if t and t.strip()), key=score, default=None)
    if not best or words(best) == words(draft) or score(best) < 0.85 or score(best) <= base:
        return None
    # a mishearing, not an actor's own wording: the draft's words that change are said by no other
    # actor ("enraged", "hostel"), the read's replacements by some ("in range", "hostile")
    known = {w for s in sib_words for w in s}
    a, b = words(draft), words(best)
    gone, new, spots = set(), set(), []
    for op, i1, i2, j1, j2 in difflib.SequenceMatcher(a=a, b=b, autojunk=False).get_opcodes():
        if op != "equal":
            gone.update(a[i1:i2])
            new.update(b[j1:j2])
            # only swapped words (a misheard word); an added or dropped word, a casual form ("you new" /
            # "you're new": the form heard wins) or an interjection ("Ah," / "Well,") is the actor's own
            casual = any(EXPAND_KEY.get(x, "").split()[:1] == [y] or EXPAND_KEY.get(y, "").split()[:1] == [x]
                         for x, y in zip(a[i1:i2], b[j1:j2]))
            spots.append((a[max(i1 - 1, 0):i2 + 1], b[max(j1 - 1, 0):j2 + 1]) if op == "replace" and
                         i2 - i1 == j2 - j1 and not casual and not set(a[i1:i2] + b[j1:j2]) & INTERJECTIONS
                         else ([], [None]))
    if gone & vocab:  # never trade a game name (Bundschuh) for a guess
        return None
    by_words = not place_only and not gone & known and new <= known and score(best) - base >= 0.1
    # or by place: at each change, two other actors say the read's words with their neighbours and none
    # the draft's ("switching to full speed" x10, "switching a full" nowhere; user, session 2)
    has = lambda s, g: any(s[k:k + len(g)] == g for k in range(len(s) - len(g) + 1))  # noqa: E731
    fams = [(f, [words(t) for t in ts if t]) for f, ts in sibs.items() if f != own_family]
    by_place = all(sum(any(has(s, nb) for s in ss) for f, ss in fams) >= 2
                   and not any(has(s, na) for f, ss in fams for s in ss) for na, nb in spots)
    return best if by_words or by_place else None


def word_diff(a: str, b: str) -> int:
    """How many words two reads differ by (the longer read's words not matched in order)."""
    wa, wb = re.findall(r"[a-z0-9']+", a.lower()), re.findall(r"[a-z0-9']+", b.lower())
    same = sum(m.size for m in difflib.SequenceMatcher(a=wa, b=wb, autojunk=False).get_matching_blocks())
    return max(len(wa), len(wb)) - same


def same_line(a: str, b: str, sec_a: float = 0, sec_b: float = 0) -> bool:
    """Two takes of one actor say the same line: at most two words differ, or one read is a fraction of
    the other's words while the clips are as long (speech to text lost words on a short clip: "Power" for
    "Pull out and head to"). Takes that say different words at length are intentional variants (user,
    session 2); a much longer clip is another recording ("...full of titanium fetches a good price")."""
    if word_diff(a, b) <= 2:
        return True
    na, nb = len(re.findall(r"[a-z0-9']+", a.lower())), len(re.findall(r"[a-z0-9']+", b.lower()))
    as_long = sec_a and sec_b and abs(sec_a - sec_b) <= 0.2 * max(sec_a, sec_b)
    return bool(as_long and min(na, nb) < 0.6 * max(na, nb))


def family(voice: str) -> str:
    """One actor's takes of the same line: pilot_f_ill_m02, ..._m02a, ..._m02b_vanilla -> pilot_f_ill_m02."""
    return re.sub(r"(?<=\d)[ab]?(?:_vanilla)?$", "", voice.lower())


FRAGMENT = re.compile(r"^(gcs_refer_|gcs_misc_|gcs_gen_|gcs_ml_w02bf|bgcs_)|_grp$|^\$BGCS_(Base_|My_Faction|Faction_Rebuff)", re.I)  # Trent's $BGCS_Trent_ lines are whole sentences
NUMBER = re.compile(r"^gcs_misc_number_(\d+)", re.I)


def fragment_form(lid: str, text: str, names: Names) -> str:
    m = NUMBER.match(lid)
    if m:
        return m.group(1)
    t = text.strip()
    t = re.sub(r"[.!?…]+$", "", t).strip()
    first = t.split()[0] if t.split() else ""
    if first and not (key(first) in names.vocab or (len(first) > 1 and first.isupper())):
        t = t[:1].lower() + t[1:]
    return t


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--corrections", required=True)
    ap.add_argument("--glossary", required=True)
    a = ap.parse_args()
    out = Path(a.out)
    names = Names(tsv(out / "gamenames.tsv"), Path(a.glossary).read_text(encoding="utf-8"))
    aligned = tsv(out / "aligned.tsv")
    script_by_id = {(r["voice"], r["id"]): r["text"] for r in tsv(out / "index.tsv") if r["text"]}
    names.learn_ordinary(r["asr"] for r in aligned)
    classes = {(r["voice"], r["id"]): r["class"] for r in tsv(out / "classified.tsv")}
    rules = load_name_rules(Path(a.corrections).parent / "name_rules.tsv")
    style = load_name_rules(Path(a.corrections).parent / "style_rules.tsv")
    chatter = load_chatter_rules(Path(a.corrections).parent / "chatter_rules.tsv")
    grammar = load_grammar_roles(GRAMMAR)
    guess_path = Path(a.corrections).parent / "repeat_guesses.tsv"
    guesses = {(r["id"].lower(), r["family"].lower()): r for r in tsv(guess_path)} if guess_path.exists() else {}
    cont_path = Path(a.corrections).parent / "continuity.tsv"
    continuity = {r["id"].lower(): r for r in tsv(cont_path)} if cont_path.exists() else {}
    judged_path = Path(a.corrections).parent / "auto_decisions.tsv"
    # keyed (id, voice); voice "" = every voice of that id (older rows)
    judged = {(r["id"].lower(), r["voice"].lower()): r for r in tsv(judged_path)} if judged_path.exists() else {}
    corr, exact = {}, set()
    for r in tsv(Path(a.corrections)):
        corr[(r["id"].lower(), r["voice"].lower())] = r["text"]
        if r.get("note", "") == "kept exactly as typed":  # Shift+Enter in the review tool
            exact.add((r["id"].lower(), r["text"]))  # every voice that saved these exact words

    rows, stats, rereview, take_text, rule_why = [], {}, {}, [], []
    # hash-named vanilla clips of story/mission lines are story lines too (whole subtitles)
    md5 = audio_md5(out)
    story_audio = {h for (v, i), h in md5.items() if DIALOGUE.match(i)}
    # what each actor's reads of a line id say (speech to text, FLSubs, a reviewed text): context for
    # choosing between one clip's reads (context_pick)
    siblings = {}
    for r in aligned:
        if DIALOGUE.match(r["id"]) or r["id"].lower().startswith(("0x", "mix_")):
            continue
        reads = siblings.setdefault(r["id"].lower(), {}).setdefault(family(r["voice"]), [])
        c = corr.get((r["id"].lower(), r["voice"].lower()))
        # not FLSubs: it often reused one transcription for every voice of a line, so its typos would
        # look like agreement between actors
        reads.extend([c] if c else [r["asr"]])
    for r in aligned:
        lid, voice = r["id"], r["voice"]
        c = classes.get((voice, lid), "")
        text, src, review, note = r["text"], r["source"], "", ""
        fl, asr = r.get("flsubs", ""), r["asr"]
        if re.fullmatch(r"\W*(Edison Trent|Edison|Trent)\W*", asr) and fl and not re.search(r"Edison|Trent", fl):
            asr = ""  # the glossary read back on an unclear clip; FLSubs heard the line
        if src == "script":
            if c == "words: FLSubs hears the recording too":
                # the recording's wording: ordinary words from speech to text, names from FLSubs,
                # then the script's spelling of names
                heard, _ = word_merge(asr, fl, names) if asr else (fl, False)
                cand, swapped = script_names(r["text"], heard, names)
                if swapped:
                    review, note = "high", "a name differs between script and recording"
                else:
                    text, src = cand, "recording"
            elif c == "words":
                review = "high"
            elif c == "words: FLSubs agrees with script":
                review, note = "low", "speech to text heard other words"
        else:
            if asr and fl:
                text, conflict = word_merge(asr, fl, names)
                src = "asr+flsubs" if text != asr else "asr"
                if conflict:
                    review, note = "high", "FLSubs and speech to text disagree on a name"
            elif fl:
                text, src = fl, "flsubs"
            elif asr:
                text, src = asr, "asr"
            # the script for this line id (not aligned to the clip) says FLSubs' words and speech to text
            # heard something that sounds the same: a mishearing ("What Rheinland is?" for "What?
            # Rheinlanders?", "Hi D." for "ID?"; user, session 2). Not other wording ("I'm already committed"
            # vs the script's "I'm on assignment": FLSubs sometimes copied the script) and not sounds.
            wk = lambda t: [key(w) for w in t.split() if key(w)]  # noqa: E731
            sid = script_by_id.get((voice, lid), "")
            if fl and asr and sid and wk(sid) == wk(fl) != wk(asr) and len(wk(asr)) <= 4 and \
                    "".join(wk(fl)) != "".join(wk(asr)) and \
                    difflib.SequenceMatcher(a="".join(wk(fl)), b="".join(wk(asr))).ratio() >= 0.75 and \
                    not all(re.fullmatch(r"(a*h+a*)+|n+o+|o+h+|u+h+|ya+y?|ye+|aa+h*", w) for w in wk(fl)):
                text, src, review, note = fl, "flsubs = script", "", "the script for this line says FLSubs' words"
            text = referent_text(lid, text, names)
            if review == "high" and names.for_id(lid):
                review, note = "", ""  # the game's own name for this fragment settled it
        if lid.lower().startswith("mix_"):
            review, note = "", "scene mix: needs timed parts (FLSubs Dynamic parts / forced alignment)"
        if FRAGMENT.search(lid):
            text = fragment_form(lid, text, names)
            frag = "1"
        else:
            frag = ""
        if not frag and not lid.lower().startswith("mix_"):
            # the reads differ only in contractions / casual forms / number words: the form speech to text heard
            pick = form_pick([("draft", text), ("script", r["text"] if r["source"] == "script" else ""),
                              ("asr", asr), ("flsubs", fl)])
            if pick:
                text, src, review, note = pick, "form: heard", "", "contraction / casual form as speech to text heard it"
            # the same line id spoken by other characters: when one read of this clip matches their versions
            # far better than the draft does, it is the one ("You are now enraged" vs FLSubs "You are now in
            # range", which other characters say word for word; user)
            elif not DIALOGUE.match(lid) and not WELCOME.match(lid):
                pick = context_pick(text, [asr, fl], siblings.get(lid.lower(), {}), family(voice), names.vocab)
                if pick:
                    text, src, review, note = pick, "context: other voices", "", "the read that matches other characters' versions of this line"
        # a name fragment: "run my" / "the" where nine voices say "run by" / "a" (not a piece that is a game
        # name itself: the game's spelling settles those)
        elif frag and not WELCOME.match(lid) and not names.for_id(lid):
            pick = context_pick(text, [asr, fl], siblings.get(lid.lower(), {}), family(voice), names.vocab, place_only=True)
            if pick and fragment_form(lid, pick, names) != text:  # a pick the piece's form undoes is no change
                text, src, review, note = fragment_form(lid, pick, names), "context: other voices", "", \
                    "the read that matches other characters' versions of this line"
        # FLSubs wrote "gonna" where speech to text wrote "going to": the actor said "gonna" (speech to text
        # usually hears it, 180 lines; the user's reviews chose FLSubs' "gonna" both times it didn't)
        if fl and len(re.findall(r"(?i)\bgonna\b", fl)) == len(re.findall(r"(?i)\bgoing to\b", text)) > 0 \
                and not re.search(r"(?i)\bgoing to\b", fl):
            text = re.sub(r"\b(G|g)oing to\b", lambda m: m.group(1) + "onna", text)
        if WELCOME.match(lid):  # a base intro: the base's name as the game writes it
            text = welcome_form(text, names.for_id(lid))
        ck = (lid.lower(), voice.lower())
        dec = judged.get((lid.lower(), voice.lower())) or judged.get((lid.lower(), ""))
        # judged low-priority line, a Neural Net message (all judged, session 2) or a decision made for this
        # voice's line (a list decided in bulk by the user's rule) (data/auto_decisions.tsv)
        if dec and (review == "low" or voice.startswith("nnvoice") or dec["voice"]):
            if dec["decision"] == "script":
                review, src, note = "", "judged: script", dec["reason"]
            elif dec["decision"] == "recording" and dec["text"]:
                text, review, src, note = dec["text"], "", "judged: recording", dec["reason"]
            else:
                note = "judged unsure: " + dec["reason"]
        if ck in corr or (lid.lower(), "*") in corr:
            text, src, review, note = corr.get(ck) or corr[(lid.lower(), "*")], "correction", "", ""
        saved = text
        why = []
        # all-caps text is FLSubs' habit (and the script's for shouts); the user's saved capitals stay ("CNC
        # only", "YEE HAW!", "NOOOO!": user, session 2)
        t = text if src == "correction" else sentence_case(text, names, partial=not DIALOGUE.match(lid))
        if t != text:
            why.append("all-caps words to normal case")
        for rx, rep, rnote in rules:
            t2 = apply_name_rules(t, [(rx, rep, rnote)])
            if t2 != t:
                why.append(rnote or rx.pattern)
                t = t2
        keep_typed = src == "correction" and (lid.lower(), text) in exact
        if not DIALOGUE.match(lid) and not keep_typed:  # radio chatter mishearings (data/chatter_rules.tsv)
            for ids, rx, rep, rnote, voices in chatter:
                if ids.search(lid) and (voices is None or voices.fullmatch(voice)):
                    t2 = apply_style_rules(t, [(rx, rep, rnote)])
                    if t2 != t:
                        why.append(rnote or rx.pattern)
                        t = t2
        if lid.lower().startswith("gcs_refer_base_") and not keep_typed:
            t2 = base_name_form(lid, t, names)
            if t2 != t:
                why.append("the base's name as the game writes it")
                t = t2
        if not keep_typed:  # "kept exactly as typed" skips conventions
            for rx, rep, rnote in style:
                t2 = apply_style_rules(t, [(rx, rep, rnote)])
                if t2 != t:
                    why.append(rnote or rx.pattern)
                    t = t2
        story = DIALOGUE.match(lid) or md5.get((voice, lid)) in story_audio
        if not story and not keep_typed and not voice.startswith("nnvoice"):  # place in the game's sentence
            t2 = apply_grammar(lid, t, grammar, names)
            if t2 != t:
                why.append("piece inside a sentence: no end mark before more words / lowercase continuation")
                t = t2
        t2 = callsign_digits(t, names)
        if t2 != t:
            why.append("callsign numbers as digits")
            t = t2
        op = continuity.get(lid.lower())
        if op and not keep_typed:
            t2 = apply_continuity(t, op, names)
            if t2 != t:
                why.append("split sentence: " + op.get("reason", ""))
                t = t2
        if voice.startswith("nnvoice") and not keep_typed:  # the ship's computer: ALL CAPS, no end period (user)
            t2 = strip_final_period(t).upper()
            if t2 != t:
                why.append("Neural Net voice in all caps")
                t = t2
        text = t
        # a reviewed line the rules changed: back to the reviewer; dropping only its sentence-ending "."
        # (the runtime adds it, user's rule) is no change of wording
        if src == "correction" and text != saved and \
                strip_final_period(saved) != strip_final_period(text):
            # one entry per actor (the takes are saved together); story lines once for every copy
            rkey = (lid.lower(), "*" if DIALOGUE.match(lid) else family(voice))
            rereview.setdefault(rkey, (lid, voice, "; ".join(why), saved, text))
        stats[src] = stats.get(src, 0) + 1
        rows.append([lid, voice, r["speaker"], text, src, frag, review, note, asr, fl])
        take_text.append(saved)
        rule_why.append("; ".join(why))

    # Repeated lines: one actor's takes (voice + a/b, vanilla copies) read the same. Majority read wins
    # (user, session 1); reviewed takes never change; no majority -> review. Claude's guesses for takes
    # that could not be settled (data/repeat_guesses.tsv) go into the draft and to review.
    ids_of = {}
    secs = {(r["voice"], r["id"]): float(r["sec"] or 0) for r in tsv(out / "index.tsv")}
    for row in rows:
        h = md5.get((row[1], row[0]))
        if h and not row[0].lower().startswith("0x"):  # hash names are the same line's vanilla copy
            ids_of.setdefault(h, set()).add(row[0].lower())
    fams = {}
    for i, row in enumerate(rows):
        # chatter only; screams wait for their style decision; hash-only ids (vanilla bank copies
        # nothing references) are left alone; a clip that is also another line's recording (the bank
        # reuses it) says that other line, so it is no take of this one
        reused = len(ids_of.get(md5.get((row[1], row[0])), ())) > 1
        if not (DIALOGUE.match(row[0]) or "scream" in row[0].lower() or row[0].lower().startswith("0x") or reused):
            fams.setdefault((row[0].lower(), family(row[1])), []).append(i)
    repeats = []
    for (lid, fam), idx in fams.items():
        g = guesses.get((lid, fam))
        # takes the recording says differently by more than a word or two are intentional variants,
        # not mishearings (user, session 1): only the biggest cluster of near-identical takes is one line
        clusters = []
        for i in idx:  # compared after the rules (mishearing fixes), with the clips' lengths
            for c in clusters:
                if same_line(rows[i][3], rows[c[0]][3], secs.get((rows[i][1], rows[i][0]), 0),
                             secs.get((rows[c[0]][1], rows[c[0]][0]), 0)):
                    c.append(i)
                    break
            else:
                clusters.append([i])
        idx = max(clusters, key=len)
        if len(idx) < 2 and not g:
            continue
        before = {key(take_text[i]) for i in idx}
        votes = {}
        for i in idx:
            votes.setdefault(key(rows[i][3]), []).append(i)
        ranked = sorted(votes.values(), key=len, reverse=True)
        status, why = "", ""
        if g:
            status = "guess" if g["text"] else "unclear"
            why = g["note"]
            if g["text"]:
                for i in idx:
                    if rows[i][4] != "correction":
                        rows[i][3], rows[i][4] = g["text"], "claude's guess"
        elif len(ranked) > 1:
            if len(ranked[0]) > len(ranked[1]):
                spellings = [rows[i][3] for i in ranked[0]]
                fixed = [rows[i][3] for i in ranked[0] if rows[i][4] == "correction"]
                # most common spelling; equal counts: the first in sorted order, so every run agrees
                win = fixed[0] if fixed else max(sorted(set(spellings)), key=spellings.count)
                for i in idx:
                    if rows[i][4] != "correction" and rows[i][3] != win:
                        rows[i][3], rows[i][4] = win, "majority of takes"
                status, why = "settled", "majority of the actor's takes"
            else:
                status, why = "tie", "the actor's takes disagree, no majority"
                for i in idx:
                    if rows[i][4] != "correction":
                        rows[i][6], rows[i][7] = "high", why
        elif len(before) > 1:
            status, why = "settled", "rules (chatter mishearings, game names) made the takes agree"
        if status:
            takes = " | ".join(f"{rows[i][1]}: {take_text[i]}" for i in idx)
            repeats.append([rows[idx[0]][0], fam, status, rows[idx[0]][3], why, takes])
    # The same chunk in other voices and takes (same line id, same words) is written the way the user
    # wrote it: their "!" / "?" and inner punctuation ("All systems have failed! Losing power!", user)
    written = {}
    for row in rows:
        if row[4] == "correction" and not DIALOGUE.match(row[0]):
            written.setdefault((row[0].lower(), squash(row[3])), collections.Counter())[row[3]] += 1
    restyled = 0
    for row in rows:
        w = written.get((row[0].lower(), squash(row[3])))
        if w and row[4] != "correction" and not DIALOGUE.match(row[0]):
            best = w.most_common(1)[0][0]
            if row[3] != best:
                row[3], row[4] = best, "your review (same words)"
                restyled += 1
    print(f"same words as a reviewed chunk: {restyled} lines took its punctuation")
    # The same recording (byte-identical clip) under several ids or voices reads the same: a reviewed
    # copy gives its text to the others.
    same = {}
    for i, row in enumerate(rows):
        h = md5.get((row[1], row[0]))
        if h:
            same.setdefault(h, []).append(i)
    copied = 0
    for idx in same.values():
        fixed = [i for i in idx if rows[i][4] == "correction"]
        for i in idx:
            if fixed and rows[i][4] != "correction" and rows[i][3] != rows[fixed[0]][3]:
                rows[i][3], rows[i][4], rows[i][6] = rows[fixed[0]][3], "correction (same recording)", ""
                copied += 1
    print(f"same recording: {sum(1 for v in same.values() if len(v) > 1)} clips with copies; "
          f"{copied} copies took a reviewed text")
    with atomic_open(out / "repeats.tsv") as fo:
        fo.write("id\tfamily\tstatus\ttext\twhy\ttakes\n")
        for rp in repeats:
            fo.write("\t".join(rp) + "\n")
    print("repeated lines:", ", ".join(f"{s} {sum(1 for rp in repeats if rp[2] == s)}"
                                      for s in ("unclear", "guess", "tie", "settled")))

    write_snapshot(ASR_DIR.parent / "data" / "subtitles.tsv", out / "subtitles_diff.txt", rows, rule_why,
                   lambda i: ("story" if DIALOGUE.match(rows[i][0]) or md5.get((rows[i][1], rows[i][0])) in story_audio
                              else "nn" if rows[i][1] == "nnvoice" else "chunk" if GENERIC_LINE.match(rows[i][0])
                              else "unique"),
                   secs, load_speakers(ASR_DIR.parent / "data" / "speakers.tsv"))
    head = "id\tvoice\tspeaker\ttext\tsource\tfragment\treview\tnote\tspeech to text\tFLSubs\n"
    with atomic_open(out / "draft.tsv") as fo:
        fo.write(head)
        for row in rows:
            fo.write("\t".join(row) + "\n")
    for level in ("high", "low"):
        sel = [row for row in rows if row[6] == level]
        with atomic_open(out / f"review_{level}.tsv") as fo:
            fo.write(head)
            for row in sel:
                fo.write("\t".join(row) + "\n")
        print(f"review {level}: {len(sel)}")
    # plus lines flagged as inconsistent (data/rereview_notes.tsv) while the saved text is still the flagged one
    notes_path = Path(a.corrections).parent / "rereview_notes.tsv"
    if notes_path.exists():
        current = {k[0]: v for k, v in corr.items()}
        final = {row[0].lower(): row[3] for row in rows}
        for n in tsv(notes_path):
            lid = n["id"].lower()
            if current.get(lid) == n["flagged text"]:
                old = rereview.get((lid, "*"))
                why = (old[2] + "; " if old else "") + n["why"]
                rereview[(lid, "*")] = (n["id"], "*", why, n["flagged text"], final.get(lid, n["flagged text"]))
    with atomic_open(out / "rereview.tsv") as fo:
        fo.write("id\tvoice\twhy\tyou saved\tsubtitle now\n")
        for row in rereview.values():
            fo.write("\t".join(row) + "\n")
    print(f"re-review: {len(rereview)} reviewed lines changed by the rules or flagged (rereview.tsv)")
    print("draft sources:", ", ".join(f"{k} {v}" for k, v in sorted(stats.items(), key=lambda kv: -kv[1])))
    print(f"fragments: {sum(1 for row in rows if row[5])}")


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    main()
