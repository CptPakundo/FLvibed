"""Hand-placed word timings (data/word_overrides.tsv): the review tool's timeline writes them, the aligner ships them.

The owner (29 Oct 2026): alignment edited like a video editor's timeline, word cards dragged in time. A line's override
holds every word's start and end (ms; -1 = not placed) and the line's end, for one text: it is keyed by the line (id,
voice; a movie or mix line: the track id with voice "movie" / "mix") and the FNV-1a of its text, so a changed text
falls back to the aligner (the words must be timed again, as for every changed line). The aligner's own rows stay in
out/word_times_cache.tsv (so "back to the aligner" is always there); aligner/align_words.py writes data/word_times.tsv
with these timings in place of its own (`--overrides-only` does only that, without the model).
Columns: id, voice, text_fnv, starts_ms, ends_ms, end_ms, by, when, text (the text, for people reading the file),
realign (word indices the aligner still has to place: words an edit changed, estimated until then - see carry()).
An edited text keeps the hand-placed timings of its unchanged words (Overrides.retext); the aligner places the changed
ones inside the gap between their kept neighbours and clears the list.
"""
import difflib
import re
import time
from pathlib import Path

import tsvio

HEADER = ["id", "voice", "text_fnv", "starts_ms", "ends_ms", "end_ms", "by", "when", "text", "realign"]


def wkey(w: str) -> str:
    return re.sub(r"[^\w']", "", w.lower().replace("’", "'"))


def carry(old_text: str, starts: list, ends: list, new_text: str, end_ms: int = 0):
    """Timings for an edited text from the old text's (the owner, 29 Oct 2026: "the words would either be re-aligned or
    kept in identical spots and lengths ... given the original alignment, original text and then the updated version").
    A word whose letters are unchanged (punctuation and capitals aside) keeps its start and end; a changed or added
    stretch gets an estimate inside the gap between the kept words around it (by letters; the old stretch's time when
    it replaced one) and is listed for the aligner to place in that gap. Returns (starts, ends, realign indices)."""
    old, new = old_text.split(), new_text.split()
    ok, nk = [wkey(w) for w in old], [wkey(w) for w in new]
    ns, ne, todo = [-1] * len(new), [-1] * len(new), []
    sm = difflib.SequenceMatcher(None, ok, nk, autojunk=False)
    ops = sm.get_opcodes()
    for tag, i1, i2, j1, j2 in ops:
        if tag == "equal":
            for d in range(i2 - i1):
                ns[j1 + d], ne[j1 + d] = starts[i1 + d], ends[i1 + d]
    last_end = max([e for e in ends if e >= 0] + [end_ms, 0])
    for tag, i1, i2, j1, j2 in ops:
        if tag not in ("replace", "insert"):
            continue
        # the gap: the old stretch's time if it was timed, else between the kept neighbours
        old_s = [starts[i] for i in range(i1, i2) if starts[i] >= 0]
        old_e = [ends[i] for i in range(i1, i2) if ends[i] >= 0]
        prev = next((ne[j] for j in range(j1 - 1, -1, -1) if ne[j] >= 0), 0)
        nxt = next((ns[j] for j in range(j2, len(new)) if ns[j] >= 0), last_end)
        g0, g1 = (min(old_s), max(old_e)) if old_s and old_e else (prev, nxt)
        # no room (an inserted word between two words that touch): the kept neighbours join the stretch, one at a time
        # on each side, until it has ~120 ms per word - they are re-aligned with it; the rest stays where it was
        for _ in range(3):
            if g1 - g0 >= 120 * (j2 - j1):
                break
            if j1 > 0 and ns[j1 - 1] >= 0:
                j1 -= 1
                g0 = ns[j1]
            if j2 < len(new) and ns[j2] >= 0:
                g1 = ne[j2]
                j2 += 1
        if g1 - g0 < 40 * (j2 - j1):  # still no room: the aligner places them (not placed until then)
            for j in range(j1, j2):
                ns[j] = ne[j] = -1
            todo += range(j1, j2)
            continue
        letters = [max(1, len(nk[j])) for j in range(j1, j2)]
        t = g0
        for j, n in zip(range(j1, j2), letters):
            dur = (g1 - g0) * n / sum(letters)
            ns[j], ne[j] = int(t), int(t + dur)
            t += dur
        todo += range(j1, j2)
    return ns, ne, sorted(todo)


def fnv1a(s: str) -> str:
    h = 0x811C9DC5
    for b in s.encode("utf-8"):
        h = ((h ^ b) * 0x01000193) & 0xFFFFFFFF
    return f"{h:08x}"


def ints(s: str) -> list:
    out = []
    for x in (s or "").split(","):
        try:
            out.append(int(x))
        except ValueError:
            out.append(-1)
    return out


class Overrides:
    def __init__(self, path: Path):
        self.path = path
        self.load()

    def load(self):
        self.rows = {}
        if self.path.exists():
            for r in tsvio.read_tsv(self.path):
                self.rows[(r["id"].lower(), r["voice"].lower(), r["text_fnv"])] = r

    def get(self, lid: str, voice: str, text: str, fnv: str = None):
        """{starts, ends, end_ms, by, when, realign} for the line's current text (fnv: its hash, if known), or None."""
        if not self.rows:
            return None
        r = self.rows.get((lid.lower(), voice.lower(), fnv or fnv1a(text)))
        if r is None:
            return None
        return {"starts": ints(r["starts_ms"]), "ends": ints(r["ends_ms"]), "end_ms": int(r["end_ms"] or 0),
                "by": r["by"], "when": r["when"],
                "realign": [int(x) for x in (r.get("realign") or "").split(",") if x.strip().isdigit()]}

    def retext(self, lid: str, voice: str, old_text: str, new_text: str, by: str) -> bool:
        """The line's text changed: its hand-placed timings follow (carry); the changed words wait for the aligner."""
        o = self.get(lid, voice, old_text)
        if o is None or old_text == new_text:
            return False
        starts, ends, todo = carry(old_text, o["starts"], o["ends"], new_text, o["end_ms"])
        self.rows.pop((lid.lower(), voice.lower(), fnv1a(old_text)), None)
        if any(s >= 0 for s in starts):
            self.put(lid, voice, new_text, starts, ends, by, todo, check=False)
        else:
            self.write()
        return True

    def put(self, lid: str, voice: str, text: str, starts: list, ends: list, by: str, realign=(), check=True):
        """Stores one line's timings (checked: one start and end per word, each word forwards, in order)."""
        words = text.split()
        if len(starts) != len(words) or len(ends) != len(words):
            raise ValueError(f"{len(words)} words, {len(starts)} starts, {len(ends)} ends")
        last = -1
        for s, e in zip(starts, ends):
            if s < 0 or not check:
                continue
            if e <= s or s < last - 5:  # 5 ms of slack for rounding
                raise ValueError("a word ends before it starts, or starts before the word before it ends")
            last = e
        placed = [e for s, e in zip(starts, ends) if s >= 0]
        if not placed:
            raise ValueError("no word placed")
        self.rows[(lid.lower(), voice.lower(), fnv1a(text))] = {
            "id": lid, "voice": voice, "text_fnv": fnv1a(text), "starts_ms": ",".join(str(int(s)) for s in starts),
            "ends_ms": ",".join(str(int(e)) for e in ends), "end_ms": str(int(max(placed))), "by": by,
            "when": time.strftime("%Y-%m-%d %H:%M"), "text": " ".join(words),
            "realign": ",".join(str(i) for i in realign)}
        self.write()

    def drop(self, lid: str, voice: str, text: str) -> bool:
        gone = self.rows.pop((lid.lower(), voice.lower(), fnv1a(text)), None) is not None
        if gone:
            self.write()
        return gone

    def write(self):
        head = ("# Hand-placed word timings (the review tool's timeline; asr/word_overrides.py). The aligner ships them in\n"
                "# data/word_times.tsv in place of its own while the line's text (text_fnv) is unchanged. ms, -1 = not placed.\n")
        body = "\t".join(HEADER) + "\n" + "".join(
            "\t".join(" ".join(str(r.get(h, "")).split()) for h in HEADER) + "\n"
            for _, r in sorted(self.rows.items()))
        tsvio.atomic_write(self.path, head + body)
