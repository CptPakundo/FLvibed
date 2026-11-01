"""The scene mixes' lines in the review tool (data/mixes.tsv, drafted by mix_align.py): list, show, edit, re-align.

The owner (29 Oct 2026): "the subtitles tool [should] properly support a full review and edit of these mixes, complete
with alignment support". A mix line has no clip of its own: it is a window (start / end, seconds on the mix's clock) of
the mix's clip, so the page plays the mix from the line's start and stops at its end. Item numbers: MIX_BASE - k = row k
of mixes.tsv (dialogue lines are >= 0, the story order's movie lines -1 and below, far above MIX_BASE).

Edits (POST /api/mix): save (text, speaker, window; marks the line reviewed: column `reviewed` = who + date), split
before a word, insert an empty line after, delete, fit the window to the aligned words. Every edit rewrites mixes.tsv
atomically under the tool's lock and asks for an alignment: the aligner (aligner/align_words.py --movies-only, its own
environment) runs in the background, one run at a time, again when edits came in during a run; it re-aligns only lines
whose text or window changed (incremental), and the page refreshes the word strip when it is done (GET /api/align).
Only the project's own data/mixes.tsv is aligned (a scratch copy, the unit tests: never).
"""
import subprocess
import sys
import threading
import time
from pathlib import Path

import merge
import tsvio

HERE = Path(__file__).resolve().parent
MIX_BASE = -1_000_000
HEADER = ["mix", "start", "end", "speaker", "text", "note", "reviewed"]
PROJECT_MIXES = HERE.parent / "data" / "mixes.tsv"
ALIGNER = HERE.parent / "aligner"


def fnv1a(s: str) -> str:
    h = 0x811C9DC5
    for b in s.encode("utf-8"):
        h = ((h ^ b) * 0x01000193) & 0xFFFFFFFF
    return f"{h:08x}"


def secs(v) -> float:
    try:
        return float(v)
    except (TypeError, ValueError):
        return 0.0


class Aligner:
    """The background alignment run (one at a time; a request during a run starts another after it)."""

    MODES = {"movies": "--movies-only", "overrides": "--overrides-only"}  # movies: also does the overrides' work

    def __init__(self, enabled: bool, on_done):
        self.enabled, self.on_done = enabled, on_done
        self.lock = threading.Lock()
        self.running, self.pending, self.last, self.runs = False, set(), "", 0

    def request(self, mode: str = "movies"):
        """mode movies: the movie / mix lines whose text or window changed (and the hand-placed timings); overrides:
        only the hand-placed timings (words edits changed in them; data/word_times.tsv)."""
        if not self.enabled:
            self.last = "alignment off (not the project's data folder)"
            return
        with self.lock:
            self.pending.add(mode)
            if self.running:
                return
            self.running = True
        threading.Thread(target=self._run, daemon=True).start()

    def _run(self):
        while True:
            with self.lock:
                mode = "movies" if "movies" in self.pending else "overrides"
                self.pending.clear()
            t0 = time.time()
            py = ALIGNER / ".venv" / "Scripts" / "python.exe"
            try:
                r = subprocess.run([str(py), "-I", str(ALIGNER / "align_words.py"), self.MODES[mode]],
                                   capture_output=True, text=True, encoding="utf-8", cwd=str(ALIGNER),
                                   # no console window flashing up when the tool runs windowless (review.cmd)
                                   creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
                tail = [l for l in r.stdout.splitlines() if l.startswith(("mixes:", "hand-placed", "done:"))]
                self.last = (f"aligned in {time.time() - t0:.0f} s: " + "; ".join(tail)) if r.returncode == 0 \
                    else "the aligner failed: " + (r.stderr or r.stdout)[-400:]
            except OSError as e:
                self.last = f"the aligner did not start: {e}"
            self.runs += 1
            self.on_done()
            with self.lock:
                if not self.pending:
                    self.running = False
                    return

    def status(self):
        return {"running": self.running, "pending": bool(self.pending), "last": self.last, "runs": self.runs}


def ms_list(s: str) -> list:
    """A word-time row's comma-separated ms field ("" -> []; a bad item -> -1)."""
    out = []
    for x in (s or "").split(","):
        try:
            out.append(int(x))
        except ValueError:
            out.append(-1)
    return out if (s or "").strip() else []


def timing(words: list, starts: list, ends: list, end_ms: int, state: str, realign=()):
    """The word strip / timeline's view of a line: per word its start and end (ms; -1 = not placed). Ends come from the
    hand-placed timings or the aligner's ends_ms (a word cut where a measured pause begins, so pauses show as gaps:
    align_words.word_ends); without them a word ends where the next placed one starts (the last at the line's end)."""
    if not ends or len(ends) != len(words):
        ends = []
        for i, s in enumerate(starts):
            nxt = next((x for x in starts[i + 1:] if x >= 0), end_ms)
            ends.append(nxt if s >= 0 else -1)
    return {"state": state, "end_ms": end_ms, "collapsed": 0 if state == "manual" else None,
            "words": [{"w": w, "ms": starts[i], "end": ends[i], "realign": i in realign} for i, w in enumerate(words)]}


class MixLines:
    def __init__(self, path: Path, out: Path, clips: dict, lock, overrides=None, on_aligned=None):
        """path = data/mixes.tsv; out = the pipeline's out/ (word time cache); clips = {mix id lower: the mix clip's
        line (review_server's line dict)}; lock = the tool's write lock; overrides = word_overrides.Overrides (the
        hand-placed timings, shared with the tool's dialogue lines); on_aligned = called after each aligner run."""
        self.path, self.out, self.clips, self.lock, self.overrides = path, out, clips, lock, overrides

        def done():
            self.load_words()
            if self.overrides:
                self.overrides.load()
            if on_aligned:
                on_aligned()
        self.aligner = Aligner(path.resolve() == PROJECT_MIXES.resolve(), done)
        self.load()
        self.load_words()

    # ------------------------------------------------------------ the file
    def load(self):
        self.comments, self.rows = [], []
        if not self.path.exists():
            return
        text = tsvio.read_text(self.path)
        head = None
        for raw in text.splitlines():
            if raw.startswith("#"):
                self.comments.append(raw)
                continue
            if not raw.strip():
                continue
            p = raw.split("\t")
            if head is None:
                head = p
                continue
            self.rows.append(dict(zip(head, p + [""] * len(head))))  # a file without `reviewed` yet: none reviewed
        for r in self.rows:
            for h in HEADER:
                r.setdefault(h, "")

    def write(self):
        body = "".join(c + "\n" for c in self.comments) + "\t".join(HEADER) + "\n" + "".join(
            "\t".join(" ".join(str(r.get(h, "")).split()) for h in HEADER) + "\n" for r in self.rows)
        tsvio.atomic_write(self.path, body)

    def load_words(self):
        """{(mix lower, text fnv): word row} of the aligner's mix rows (its cache holds every row, suspects too)."""
        self.words_by = {}
        for path in (self.out / "word_times_cache.tsv", self.path.parent / "word_times.tsv"):
            if path.exists():
                for r in merge.tsv(path):
                    if r.get("voice") == "mix":
                        self.words_by[(r["id"].lower(), r.get("text_fnv", ""))] = r
                break

    # ------------------------------------------------------------ items
    @staticmethod
    def is_item(n: int) -> bool:
        return n <= MIX_BASE

    @staticmethod
    def n_of(k: int) -> int:
        return MIX_BASE - k

    def row(self, n: int):
        k = MIX_BASE - n
        return self.rows[k] if 0 <= k < len(self.rows) else None

    def order(self):
        """Row indices by mix, then start."""
        return sorted(range(len(self.rows)), key=lambda k: (self.rows[k]["mix"].lower(), secs(self.rows[k]["start"]), k))

    def queue(self, show_done: bool, search: str = "", mix: str = ""):
        out = []
        for k in self.order():
            r = self.rows[k]
            if not show_done and r["reviewed"]:
                continue
            if mix and r["mix"].lower() != mix.lower():
                continue
            if search and search not in (r["mix"] + " " + r["speaker"] + " " + r["text"] + " " + r["note"]).lower():
                continue
            out.append(self.n_of(k))
        return out

    def open_count(self):
        return sum(1 for r in self.rows if not r["reviewed"])

    def of_mix(self, mix: str):
        """[(k, row)] of one mix, by start."""
        return [(k, self.rows[k]) for k in self.order() if self.rows[k]["mix"].lower() == mix.lower()]

    def words(self, r):
        """As review_server.word_info: the line's words with their start (ms on the mix clock; -1 = not placed)."""
        words = r["text"].split()
        o = self.overrides.get(r["mix"], "mix", r["text"]) if self.overrides else None
        if o:  # hand-placed (the timeline)
            return timing(words, o["starts"], o["ends"], o["end_ms"], "manual", o["realign"])
        row = self.words_by.get((r["mix"].lower(), fnv1a(r["text"])))
        if row is None:
            state = "aligning" if self.aligner.running or self.aligner.pending else "none"
            return timing(words, [-1] * len(words), [], 0, state)
        starts = []
        for s in row.get("starts_ms", "").split(","):
            try:
                starts.append(int(s))
            except ValueError:
                starts.append(-1)
        ok = len(starts) == len(words)
        w = timing(words, starts if ok else [-1] * len(words), ms_list(row.get("ends_ms")) if ok else [],
                   int(secs(row.get("end_ms"))), "current" if ok else "stale")
        w["collapsed"] = int(secs(row.get("collapsed")))
        return w

    def findings(self, r, w):
        """Why the line's window or text may be off (as the aligner's suspect list says it)."""
        out = []
        if w["state"] not in ("current", "manual"):
            return out
        placed = [x["ms"] / 1000 for x in w["words"] if x["ms"] >= 0]
        unplaced = len(w["words"]) - len(placed)
        s, e = secs(r["start"]), secs(r["end"])
        if unplaced:
            out.append(f"{unplaced} of {len(w['words'])} words not placed by the aligner")
        if placed and placed[0] < s - 0.15:
            out.append(f"speech starts {s - placed[0]:.2f} s before the window")
        if placed and placed[0] > s + 0.5:
            out.append(f"speech starts {placed[0] - s:.2f} s after the window's start")
        if w["end_ms"] / 1000 > e + 0.15:
            out.append(f"speech ends {w['end_ms'] / 1000 - e:.2f} s after the window")
        return out

    def item(self, n: int, story=None):
        r = self.row(n)
        if r is None:
            return {"n": n, "error": "no such mix line (the file changed: reload the list)"}
        k = MIX_BASE - n
        clip = self.clips.get(r["mix"].lower())
        lines = self.of_mix(r["mix"])
        at = next(i for i, (kk, _) in enumerate(lines) if kk == k)
        near = [{"n": self.n_of(kk), "start": x["start"], "end": x["end"], "speaker": x["speaker"], "text": x["text"],
                 "reviewed": bool(x["reviewed"]), "current": kk == k} for kk, x in lines[max(0, at - 5):at + 6]]
        note = r["note"]
        heard = note.split("heard: ", 1)[1].strip() if "heard: " in note else ""
        script = note.split(": ", 2)[2].strip() if "; the script line " in note and note.count(": ") >= 2 else ""
        w = self.words(r)
        return {"n": n, "mixline": {h: r[h] for h in HEADER}, "index": at + 1, "count": len(lines),
                "clip": clip["n"] if clip else None, "sec": clip["sec"] if clip else 0, "near": near,
                "speakers": sorted({x["speaker"] for _, x in lines if x["speaker"]}),
                "heard": heard, "script": script, "words": w, "findings": self.findings(r, w),
                "align": self.aligner.status(), "story": story}

    # ------------------------------------------------------------ edits
    def edit(self, n: int, body: dict, who: str):
        """One edit; returns {"ok", "n" (the line to show next)} or {"error"}."""
        with self.lock:
            self.load()  # the file as it is now (another tool or a commit may have changed it)
            r = self.row(n)
            if r is None:
                return {"error": "no such mix line (the file changed: reload the list)"}
            k = MIX_BASE - n
            act = body.get("action", "save")
            clip = self.clips.get(r["mix"].lower())
            dur = clip["sec"] if clip and clip["sec"] else 1e9
            stamp = f"{who} {time.strftime('%Y-%m-%d')}"
            show = n
            if act == "save":
                text = " ".join(str(body.get("text", r["text"])).split())
                s, e = secs(body.get("start", r["start"])), secs(body.get("end", r["end"]))
                if not text:
                    return {"error": "empty text: delete the line instead"}
                # an end past the recording is trimmed to it (a mix's last line ends near the clip's end, and the
                # timeline or a draft may put it a little past: refusing made the last line unsavable, 1 Nov 2026)
                e = min(e, dur)
                if not (0 <= s < e):
                    return {"error": f"the window must run forwards inside the mix (0 - {dur:.2f} s)"}
                if self.overrides and text != r["text"]:  # hand-placed timings follow the edit (word_overrides.carry)
                    self.overrides.retext(r["mix"], "mix", r["text"], text, who)
                r.update({"text": text, "speaker": " ".join(str(body.get("speaker", r["speaker"])).split()),
                          "start": f"{s:.2f}", "end": f"{e:.2f}", "reviewed": stamp})
            elif act == "fit":
                w = self.words(r)
                placed = [x["ms"] for x in w["words"] if x["ms"] >= 0]
                if w["state"] != "current" or not placed:
                    return {"error": "no aligned words for this text yet"}
                nxt = [secs(x["start"]) for kk, x in self.of_mix(r["mix"]) if secs(x["start"]) > secs(r["start"])]
                s = int(placed[0] / 50) * 50 / 1000
                e = -(-int(w["end_ms"] + 200) // 50) * 50 / 1000
                if nxt:
                    e = min(e, nxt[0])
                r.update({"start": f"{s:.2f}", "end": f"{min(e, dur):.2f}"})
            elif act == "split":
                words = r["text"].split()
                at = int(body.get("at", 0))
                if len(words) < 2:
                    return {"error": "a one-word line cannot be split (Insert line after adds a line)"}
                if not 0 < at < len(words):
                    return {"error": "split before a word inside the line (not its first word)"}
                w = self.words(r)
                s, e = secs(r["start"]), secs(r["end"])
                # the cut at the word's start (aligned or placed by hand), else an even share of the window
                ms = w["words"][at]["ms"] if w["state"] in ("current", "manual") else -1
                cut = ms / 1000 if s < ms / 1000 < e else s + (e - s) * at / len(words)
                second = dict(r, text=" ".join(words[at:]), start=f"{cut:.2f}", reviewed="")
                o = self.overrides.get(r["mix"], "mix", r["text"]) if self.overrides else None
                if o:  # each half keeps its words' hand-placed timings
                    for half, part in ((" ".join(words[:at]), slice(0, at)), (second["text"], slice(at, None))):
                        if any(x >= 0 for x in o["starts"][part]):
                            self.overrides.put(r["mix"], "mix", half, o["starts"][part], o["ends"][part], who,
                                               check=False)
                    self.overrides.drop(r["mix"], "mix", r["text"])
                r.update({"text": " ".join(words[:at]), "end": f"{cut:.2f}", "reviewed": ""})
                self.rows.insert(k + 1, second)
                show = self.n_of(k + 1)  # the new line opens: its window starts on its first word (owner, 1 Nov 2026)
            elif act == "insert":
                lines = self.of_mix(r["mix"])
                nxt = [secs(x["start"]) for _, x in lines if secs(x["start"]) > secs(r["start"])]
                s = secs(r["end"])
                e = min(nxt[0] if nxt else dur, s + 2.0, dur)
                if e <= s:
                    return {"error": "no room after this line: shorten it first"}
                self.rows.insert(k + 1, {h: "" for h in HEADER} | {"mix": r["mix"], "speaker": r["speaker"],
                                 "start": f"{s:.2f}", "end": f"{e:.2f}", "note": f"added by {stamp}"})
                show = self.n_of(k + 1)
            elif act == "delete":
                if self.overrides:
                    self.overrides.drop(r["mix"], "mix", r["text"])
                del self.rows[k]
                rest = [kk for kk, x in self.of_mix(r["mix"])]
                show = self.n_of(min(k, len(self.rows) - 1)) if not rest else \
                    self.n_of(min(rest, key=lambda kk: abs(kk - k)))
            else:
                return {"error": f"unknown action {act}"}
            self.write()
        self.aligner.request()
        return {"ok": True, "n": show, "structure": act in ("split", "insert", "delete")}
