"""Word timings of every subtitle line in its own recording (Qwen3-ForcedAligner-0.6B, CUDA) -> data/word_times.tsv.

    .venv\\Scripts\\python -I align_words.py [--all] [--check] [--batch 32] [--retry-suspects]
    .venv\\Scripts\\python -I align_words.py --movies-only [--fix-windows] [--movie-dir <DATA\\MOVIES>]
    (any of them: --no-second-opinion = Qwen's timings only, without the CTC check below)

Every aligned line gets the second opinion (SecondOpinion, 1 Nov 2026): two CTC aligners correct the words where both
agree and Qwen is far off, then every word gets a minimum length. How good the timings are is measured, not assumed:
bench_words.py (against the owner's hand-placed timings) and health_words.py (symptoms on every line).

The intro movies (data/movies.tsv) and the scene mixes (data/mixes.tsv) are aligned in every run when their audio is at
hand (out/movies/<movie>.wav, extracted once from the install's .wmv; out/mixes/<mix>.wav from the mix's clip; never
played): see "the intro movies" below. A line with 2+ words unplaced
gets the padded retry (place_retry: the consensus of the clip as is and padded with silence).

Why (user, 22 Oct 2026): pages of a long line were timed by a character estimate (King's 15 s M01 briefing ran
ahead of his voice); with each word's start the runtime turns a page when its first word is spoken. The text is
known, so this is forced alignment of the reviewed text, not transcription.

Incremental: a line keeps its row while its text (FNV-1a of the UTF-8 text field) and its audio (SHA-1 of the file)
are unchanged; anything else is aligned again (the user: "anything that was changed would need to have its forced
alignment re-run"). The pre-commit hook runs this whenever data/subtitles.tsv is staged (tools/install-hooks.ps1);
the runtime also ignores a row whose text hash no longer matches its line. --check only reports stale rows (exit 1).

Row: id, voice, text_fnv (hex), audio_sha1, end_ms (end of the last word), starts_ms (one per whitespace-separated
word of the text, comma-separated; -1 = a word the aligner could not place), ends_ms (last column, one per word: where
a measured pause cuts it, else the next word's start - word_ends; the runtime reads columns 1-6), collapsed (how many: piled at the clip's
very end, or sharing one start with no time to share; >= 2 lists the line in out/word_times_suspect.tsv - garble on
purpose, a text that says more than the recording, or the aligner's weakness on a line's last words). A line keeps its
placed words (pages turn on them; user, 22 Oct 2026: the Order ship's "Schultzky is a traitor!" page came too late on
the estimate); only one with most words unplaced ships no row. Short words that share a start with their neighbour (the
aligner's 80 ms steps; user: "small words like 'to' or 'a' ... don't get highlighted") share the time up to the next
word by their letters. Numerals are spelled out for the aligner. The aligner's word starts sit at the start of a pause
before the word (it splits silence between neighbours); a start inside a measured pause (the clip's own noise floor +
8 dB, >= 0.15 s, 10 ms frames) moves to the pause's end. Lines whose words cannot be matched to the aligner's units are
left out (listed in out/word_times_failed.tsv) and keep the estimate.
"""
import argparse
import csv
import hashlib
import os
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor
from contextlib import contextmanager
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent  # FLSubtitles/
os.environ.setdefault("HF_HOME", str(HERE / ".cache" / "hf"))
os.environ.setdefault("HF_HUB_DISABLE_SYMLINKS_WARNING", "1")

RATE = 16000
MODEL_ID = "Qwen/Qwen3-ForcedAligner-0.6B"
HEADER = ["id", "voice", "text_fnv", "audio_sha1", "end_ms", "starts_ms", "collapsed", "ends_ms"]


def cell(row, h):
    """A row's field as written: empty numbers as 0, except ends_ms (empty = not measured: a word runs to the next)."""
    return row.get(h) or ("" if h == "ends_ms" else "0")


def fnv1a(s: str) -> str:
    h = 0x811C9DC5
    for b in s.encode("utf-8"):
        h = ((h ^ b) * 0x01000193) & 0xFFFFFFFF
    return f"{h:08x}"


@contextmanager
def replace_on_close(path: Path):
    """Writes to a temp file next to path and swaps it in when done: a crash (the pre-commit hook killed, a full
    disk) leaves the previous file whole - data/word_times.tsv is a shipped product (asr/tsvio.py does the same)."""
    tmp = path.with_name(f"{path.name}.{os.getpid()}.tmp")
    try:
        with open(tmp, "w", encoding="utf-8", newline="") as f:
            yield f
            f.flush()
            os.fsync(f.fileno())
        os.replace(tmp, path)
    except BaseException:
        tmp.unlink(missing_ok=True)
        raise


def sha1(path: str) -> str:
    return hashlib.sha1(Path(path).read_bytes()).hexdigest()


def norm(s: str) -> str:
    return "".join(c for c in s.lower() if c.isalnum())


ONES = "zero one two three four five six seven eight nine ten eleven twelve thirteen fourteen fifteen sixteen " \
       "seventeen eighteen nineteen".split()
TENS = "_ _ twenty thirty forty fifty sixty seventy eighty ninety".split()


def spell(n: int) -> str:
    if n < 20:
        return ONES[n]
    if n < 100:
        return TENS[n // 10] + ("" if n % 10 == 0 else " " + ONES[n % 10])
    if n < 1000:
        return ONES[n // 100] + " hundred" + ("" if n % 100 == 0 else " " + spell(n % 100))
    if n < 1000000:
        return spell(n // 1000) + " thousand" + ("" if n % 1000 == 0 else " " + spell(n % 1000))
    return " ".join(ONES[int(d)] for d in str(n))


def spoken(word: str) -> str:
    """The word as the aligner can hear it: numerals spelled out ("4" -> "four", "4." stays punctuated)."""
    import re
    return re.sub(r"\d+", lambda m: " " + spell(int(m.group())) + " ", word).strip()


def decode(path: str):
    import numpy as np
    raw = subprocess.run(["ffmpeg", "-v", "error", "-i", path, "-f", "f32le", "-ac", "1", "-ar", str(RATE), "-"],
                         check=True, capture_output=True).stdout
    return np.frombuffer(raw, dtype=np.float32)


def pauses(audio, min_len=0.15):
    import numpy as np
    fr = RATE // 100
    n = len(audio) // fr
    if n < 2:
        return []
    rms = np.sqrt(np.mean(audio[: n * fr].reshape(n, fr) ** 2, axis=1) + 1e-12)
    db = 20 * np.log10(rms / rms.max())
    quiet = db < np.percentile(db, 10) + 8
    out, s = [], None
    for k, q in enumerate(list(quiet) + [False]):
        if q and s is None:
            s = k
        elif not q and s is not None:
            if (k - s) / 100 >= min_len:
                out.append((s / 100, k / 100))
            s = None
    return out


def match(text: str, units, gaps):
    """Per text word its start (s) and the end of the last word; None when the words do not match the units."""
    words = text.split()
    starts, k = [], 0
    pending = 0  # words without letters ("-", "...") take the next word's start
    end = 0.0
    for w in words:
        target = norm(spoken(w))
        if not target:
            pending += 1
            continue
        acc, first = "", None
        while acc != target:
            if k >= len(units):
                return None
            u = norm(units[k].text)
            if first is None:
                first = units[k]
            acc += u
            end = units[k].end_time
            k += 1
            if not target.startswith(acc):
                return None
        s = first.start_time
        for p0, p1 in gaps:  # a start inside a pause, or just before it (the aligner spans it), belongs at its end
            if p0 - 0.15 <= s < p1:
                s = min(p1, max(s, end - 0.05))
                break
        starts += [s] * (pending + 1)
        pending = 0
    if pending:
        starts += [end] * pending
    if k != len(units) or not starts:
        return None
    for p0, p1 in gaps:  # the last word's end inside the closing pause: the pause's start
        if p0 < end <= p1:
            end = max(p0, starts[-1])
    return spread(words, starts, end)


def spread(words, starts, end):
    """Words the aligner gave no time of their own (several sharing one start: its 80 ms steps swallow short words -
    user, 22 Oct 2026: "small words like 'to' or 'a' ... don't get highlighted") share the time up to the next word's
    start by their letters. A run with no later start to share (the line's last words collapsed onto its end) or of
    three or more words cannot be placed: counted as collapsed (a suspect line)."""
    out, collapsed, i = list(starts), 0, 0
    while i < len(out):
        j = i
        while j + 1 < len(out) and starts[j + 1] == starts[i]:
            j += 1
        if j > i:
            nxt = starts[j + 1] if j + 1 < len(starts) else end
            if j - i + 1 >= 3 or nxt - starts[i] < 0.06 * (j - i + 1):
                collapsed += j - i + 1
                for k in range(i, j + 1):
                    out[k] = -0.001  # unknown: the runtime estimates a page that starts here and does not pop it
            else:
                weights = [max(1, len(norm(spoken(w)))) for w in words[i : j + 1]]
                t, total = starts[i], sum(weights)
                for k, wgt in zip(range(i, j + 1), weights):
                    out[k] = round(t, 3)
                    t += (nxt - starts[i]) * wgt / total
        i = j + 1
    return out, end, collapsed


class Unit:
    """One aligner unit moved onto another clock (the padded retries)."""
    def __init__(self, text, start_time, end_time):
        self.text, self.start_time, self.end_time = text, start_time, end_time


def place(model, auds, texts, pad=0.0):
    """Aligns each (audio, text) pair; per pair (starts, end, collapsed) on the audio's own clock, or None (the words
    did not match the aligner's units). pad: seconds of silence added on both sides first (the retry below)."""
    import numpy as np
    out = [None] * len(auds)
    ok = [i for i, au in enumerate(auds) if len(au) > RATE // 20]
    if not ok:
        return out
    z = np.zeros(int(pad * RATE), dtype=np.float32)
    res = model.align(audio=[(np.concatenate([z, auds[i], z]) if pad else auds[i], RATE) for i in ok],
                      text=[" ".join(spoken(w) for w in texts[i].split()) for i in ok],
                      language=["English"] * len(ok))
    for i, units in zip(ok, res):
        clip = len(auds[i]) / RATE
        if pad:  # back onto the clip's clock; a word placed in the padding sits at the clip's edge
            units = [Unit(u.text, min(clip, max(0.0, u.start_time - pad)), min(clip, max(0.0, u.end_time - pad)))
                     for u in units]
        m = match(texts[i], units, pauses(auds[i]))
        if m is None:
            continue
        starts, end, collapsed = m
        # words piled onto the clip's very end were not placed either
        atEnd = [k for k, s in enumerate(starts) if s >= clip - 0.08]
        if len(atEnd) >= 2:
            for k in atEnd:
                starts[k] = -0.001
        unplaced = sum(1 for s in starts if s < 0)
        out[i] = (starts, end, max(collapsed, unplaced))
    return out


# The padded retry (26 Oct 2026). On short or radio-filtered clips whose speech runs from the first sample to the last
# the aligner often piles a run of words onto one start (the 8 story and 2 Neural Net lines that shipped no timings:
# "Unknown contacts are in range." placed "are in range" at the clip's end). The same clip with silence on both sides
# places them, but differently per padding - so no single run is trusted: each word takes the start that at least two
# of the runs (as is + three paddings) agree on within TOL, else it stays unplaced.
PADS = (0.3, 0.6, 1.0)
TOL = 0.25
MIN_WORD = 0.08  # the aligner's step


def consensus(results):
    rs = [r for r in results if r]
    if len(rs) < 2:
        return None
    n = len(rs[0][0])
    starts = []
    for k in range(n):
        # a start that leaves the words after it less than MIN_WORD each before that run's end is not a candidate
        # (two runs can agree on a word placed just before the pile at the clip's end)
        vals = sorted(r[0][k] for r in rs if r[0][k] >= 0 and (r[1] is None or r[1] - r[0][k] >= MIN_WORD * (n - k - 1)))
        best = []
        for a in range(len(vals)):
            grp = [v for v in vals if vals[a] <= v <= vals[a] + TOL + 1e-6]
            if len(grp) > len(best):
                best = grp
        starts.append(round(best[len(best) // 2], 3) if len(best) >= 2 else -0.001)
    last = -1.0
    for k, s in enumerate(starts):  # in order: a start before an earlier word's is not trusted
        if s < 0:
            continue
        if s < last:
            starts[k] = -0.001
        else:
            last = s
    # an end of None: that run's speech ran to its window's edge (movie mode) - not a measurement
    ends = sorted(r[1] for r in rs if r[1] is not None) or [max(starts) + MIN_WORD]
    return starts, ends[len(ends) // 2], sum(1 for s in starts if s < 0)


def place_retry(model, auds, texts):
    """place(), then the padded retry for every pair with 2+ words unplaced (or no match): the consensus is taken when it
    leaves fewer words unplaced."""
    first = place(model, auds, texts)
    redo = [i for i, r in enumerate(first) if r is None or r[2] >= 2]
    if redo:
        runs = [place(model, [auds[i] for i in redo], [texts[i] for i in redo], pad) for pad in PADS]
        for j, i in enumerate(redo):
            c = consensus([first[i]] + [run[j] for run in runs])
            if c and (first[i] is None or c[2] < first[i][2]):
                first[i] = c
    return first


# The second opinion (1 Nov 2026; the owner: "some words get almost no time at all, others get too much or start too
# early or too late"; measured by bench_words.py / health_words.py, docs/REVIEW_PROCESS.md "Word timings"). Qwen's
# aligner has failures no post-processing fixes: a line's last word put on the clip's very end (15 % of all lines had a
# last word without length), words out of order ("attack." before "We are under"), words unplaced. Two CTC character
# aligners of a different kind (MMS-300m forced aligner - ctc-forced-aligner's / torchaudio MMS_FA's model - and
# wav2vec2-base-960h, WhisperX's English default) align the same text; where the two agree within SO_AGREE and Qwen's
# start is more than SO_FAR away (or unplaced), the word takes their mean. Everywhere else Qwen's start stays (on the
# owner's hand-placed lines it is the closest of all). Then every word gets at least SO_MIN (in order) and the last word
# an end after its start. Lines with symptoms 24 % -> 8.5 %; the owner's lines: mean start error 80 -> 71 ms.
SO_MODELS = ("MahmoudAshraf/mms-300m-1130-forced-aligner", "facebook/wav2vec2-base-960h")
SO_AGREE, SO_FAR, SO_MIN = 0.12, 0.2, 0.06


def ctc_path(logp, tokens, blank):
    """CTC forced alignment (Viterbi over the blank-interleaved tokens): per token index its (first, last) frame."""
    import numpy as np
    T, S = logp.shape[0], 2 * len(tokens) + 1
    ext = np.full(S, blank)
    ext[1::2] = tokens
    neg = -1e30
    dp = np.full(S, neg)
    dp[0] = logp[0, blank]
    dp[1] = logp[0, ext[1]]
    back = np.zeros((T, S), dtype=np.int8)  # 0 stay, 1 from s-1, 2 from s-2
    skip = np.zeros(S, dtype=bool)
    skip[3::2] = ext[3::2] != ext[1:-2:2]  # a token may skip the blank before it unless it repeats the last token
    for t in range(1, T):
        prev = np.stack([dp, np.concatenate([[neg], dp[:-1]]), np.where(skip, np.concatenate([[neg, neg], dp[:-2]]), neg)])
        arg = prev.argmax(0)
        dp = prev[arg, np.arange(S)] + logp[t, ext]
        back[t] = arg
    s = S - 1 if dp[S - 1] >= dp[S - 2] else S - 2
    spans = {}
    for t in range(T - 1, -1, -1):
        if s % 2:
            f, l = spans.get(s // 2, (t, t))
            spans[s // 2] = (min(f, t), max(l, t))
        s -= int(back[t, s])
    return spans


class SecondOpinion:
    """The two CTC aligners (loaded on first use, CUDA, fp16; transformers from the Qwen environment, the models in
    .cache/hf). ok = False when they cannot load: the lines keep Qwen's timings (fail closed, said once)."""

    def __init__(self):
        self.models, self.ok = None, True

    def load(self):
        if self.models is None and self.ok:
            try:
                import torch
                from transformers import AutoProcessor, Wav2Vec2ForCTC
                self.models = [(AutoProcessor.from_pretrained(m), Wav2Vec2ForCTC.from_pretrained(m).to("cuda").eval().half())
                               for m in SO_MODELS]
            except Exception as e:  # noqa: BLE001 - any load failure: no second opinion this run
                print(f"second opinion off: {e}")
                self.ok = False
        return self.ok

    def words(self, audio, text):
        """Per model, per word of text: (start, end) in seconds on the audio's clock, or None (not placed)."""
        import re
        import numpy as np
        import torch
        words, out = text.split(), []
        for proc, model in self.models:
            vocab = proc.tokenizer.get_vocab()
            blank, upper, sep = vocab.get("<pad>", 0), "A" in vocab, vocab.get("|")
            toks, owner = [], []
            for i, w in enumerate(words):
                t = re.sub(r"[^a-z' ]", "", spoken(w).lower().replace("-", " "))
                chars = [ch for ch in (t.upper() if upper else t) if ch != " " and ch in vocab]
                if chars and toks and sep is not None:
                    toks.append(sep); owner.append(-1)
                for ch in chars:
                    toks.append(vocab[ch]); owner.append(i)
            got = [None] * len(words)
            if toks and len(audio) > RATE // 10:
                x = proc(audio, sampling_rate=RATE, return_tensors="pt").input_values.to("cuda").half()
                with torch.no_grad():
                    logp = torch.log_softmax(model(x).logits[0].float(), -1).cpu().numpy()
                if logp.shape[0] >= len(toks):
                    fps = logp.shape[0] / (len(audio) / RATE)
                    for k, (f, l) in ctc_path(logp, np.array(toks), blank).items():
                        i = owner[k]
                        if i >= 0:
                            s0, e0 = got[i] or (f / fps, (l + 1) / fps)
                            got[i] = (min(s0, f / fps), max(e0, (l + 1) / fps))
            out.append(got)
        return out

    def apply(self, audio, text, m, off=0.0):
        """Qwen's (starts, end, collapsed) on the line's clock -> the same with the second opinion; audio[0] is at off
        on that clock. Unchanged when the models are off or the result is None."""
        if m is None or not self.load():
            return m
        starts, end, _ = m
        starts = list(starts)
        clip_end = off + len(audio) / RATE
        a, b = self.words(audio, text)
        for i in range(len(starts)):
            if a[i] and b[i] and abs(a[i][0] - b[i][0]) <= SO_AGREE:
                c = off + (a[i][0] + b[i][0]) / 2
                if starts[i] < 0 or abs(starts[i] - c) > SO_FAR:
                    starts[i] = round(c, 3)
        last = None
        for i, s in enumerate(starts):  # in order, each word at least SO_MIN
            if s < 0:
                continue
            if last is not None and s < last + SO_MIN:
                starts[i] = s = round(last + SO_MIN, 3)
            last = s
        if starts and starts[-1] >= 0 and end < starts[-1] + SO_MIN:  # the last word ends after its start
            ce = [off + x[-1][1] for x in (a, b) if x[-1]]
            end = min(max([starts[-1] + SO_MIN] + ce), max(clip_end, starts[-1] + SO_MIN))
        return starts, end, sum(1 for s in starts if s < 0)


# Word ends (1 Nov 2026; the owner: words "get too much" time). Rows store starts (the runtime turns pages on them); the
# review tool's timeline drew a word up to the next one, so a pause between sentences looked like a long last word.
# Column ends_ms (the runtime reads columns 1-6 only) ends a word where a measured pause (pauses(): the clip's noise
# floor + 8 dB, >= 0.15 s) begins that runs up to the next word - when that leaves the word END_PER_LETTER per letter
# (in a noisy scene mix a soft word passes as "pause": "Officially" would have kept 80 ms) and a gap of END_GAP; else up
# to the next word. Only the audio decides (no model): on 400 random lines 206 pauses shown (median 0.28 s), "long"
# words halved; rows aligned before get theirs on the next full run (fill_ends).
END_PER_LETTER, END_GAP = 0.05, 0.1


def word_ends(text, starts, end, gaps, off=0.0):
    """Per word its end (s, the starts' clock; -1 = unplaced); gaps: pauses() of audio that begins at off."""
    words, out = text.split(), []
    for i, s in enumerate(starts):
        if s < 0:
            out.append(-0.001)
            continue
        n = next((x for x in starts[i + 1:] if x >= 0), None)
        e = end if n is None else n
        if n is not None:
            least = max(SO_MIN, END_PER_LETTER * len(norm(spoken(words[i]))))
            for p0, p1 in gaps:
                p0, p1 = p0 + off, p1 + off
                if s + least <= p0 <= n - END_GAP and p1 >= n - 0.1:
                    e = p0
                    break
        out.append(max(e, s))
    return out


def ends_field(ends):
    return ",".join(str(round(e * 1000)) if e >= 0 else "-1" for e in ends)


def fill_ends(row, text, audio, off=0.0):
    """Adds ends_ms to an aligned row that has none (rows aligned before 1 Nov 2026), from its audio."""
    if row.get("ends_ms") or audio is None:
        return row
    starts = [int(x) / 1000 if int(x) >= 0 else -0.001 for x in row["starts_ms"].split(",")]
    if len(starts) != len(text.split()):
        return row
    return dict(row, ends_ms=ends_field(word_ends(text, starts, int(row["end_ms"] or 0) / 1000, pauses(audio), off)))


# ------------------------------------------------------------------ the intro movies (data/movies.tsv)
# Movie mode (26 Oct 2026, the owner: "run the forced alignment on the intros"). The narration runs over music without
# pauses, so each line is aligned inside its own window: movies.tsv start - MOVIE_PAD .. end + MOVIE_PAD of the movie's
# audio track (out/movies/<movie>.wav, extracted with ffmpeg from the install's DATA\MOVIES\<movie>.wmv, never played).
# Rows: id = the movie, voice = MOVIE_VOICE, text_fnv of the line's text (the runtime finds the line by movie + text
# hash, so a changed text keeps its estimate), audio_sha1 = SHA-1 of the window's samples + bounds (a moved window is
# aligned again), end_ms / starts_ms on the MOVIE clock (ms from the movie's start). The window check (suspects, and
# --fix-windows) works on these rows: out/movie_word_times_suspect.tsv, out/movie_window_changes.tsv.
#
# The scene mixes (data/mixes.tsv, asr/mix_align.py; 29 Oct 2026) are the same kind of table: lines on a track's own
# clock (the mix's), aligned the same way in their windows, rows with id = the mix and voice MIX_VOICE; their audio is
# the mix's clip from out/index.tsv, decoded once into out/mixes/<mix>.wav. Each table is a Track.
MOVIE_VOICE = "movie"
MIX_VOICE = "mix"
MOVIE_PADS = (0.4, 0.75, 1.25)  # the three windows of a line (seconds either side of its start / end)
MOVIE_PAD = max(MOVIE_PADS)
EDGE = 0.06


class Track:
    """A table of lines on a track's own clock: data/<name>.tsv with the track id in column `col`, word-time rows with
    voice `voice`, its side products out/<prefix>_word_times_suspect.tsv and out/<prefix>_window_changes.tsv."""

    def __init__(self, name, col, voice, prefix, extra=()):
        self.name, self.col, self.voice, self.prefix = name, col, voice, prefix
        self.path = ROOT / "data" / f"{name}.tsv"
        self.header = [col, "start", "end", "speaker", "text", "note", *extra]


MOVIES = Track("movies", "movie", MOVIE_VOICE, "movie")
MIXES = Track("mixes", "mix", MIX_VOICE, "mix", ("reviewed",))  # reviewed: the review tool's mark (mix_review.py)
TRACKS = (MOVIES, MIXES)
MOVIES_HEADER = MOVIES.header


def read_movies(track=MOVIES):
    """data/<track>.tsv: (comment lines, rows as dicts in file order); the track id is also under "movie"."""
    comments, rows, head = [], [], None
    if not track.path.exists():
        return comments, rows
    with open(track.path, encoding="utf-8", newline="") as f:
        for raw in f:
            raw = raw.rstrip("\r\n")
            if raw.startswith("#"):
                comments.append(raw)
                continue
            if not raw:
                continue
            p = raw.split("\t")
            if head is None:
                head = p
                continue
            r = dict(zip(head, p + [""] * (len(head) - len(p))))
            r["movie"] = r.get(track.col, "")
            rows.append(r)
    return comments, rows


def movie_audio(movie, movie_dir, track=MOVIES, index=None):
    """The track's audio, 16 kHz mono, extracted once into out/<track>/ (git-ignored): a movie's from its .wmv in
    movie_dir, a mix's from its clip (index: {(id, voice): path}); None when neither the extract nor the source exists."""
    wav = ROOT / "out" / track.name / f"{movie}.wav"
    if not wav.exists():
        if track is MIXES:
            src = Path((index or {}).get((movie, "mixes"), ""))
        else:
            src = Path(movie_dir) / f"{movie}.wmv"
        if not src.is_file():
            return None
        wav.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(["ffmpeg", "-v", "error", "-nostdin", "-y", "-i", str(src), "-vn", "-ac", "1", "-ar",
                        str(RATE), "-c:a", "pcm_s16le", str(wav)], check=True)
    return decode(str(wav))


def movie_window(r, audio, pad=None):
    """(window start on the movie clock, its samples, the signature of the line's widest window and its bounds)."""
    s, e = float(r["start"]), float(r["end"])
    pad = max(MOVIE_PADS) if pad is None else pad
    w0 = max(0.0, s - pad)
    w1 = min(len(audio) / RATE, e + pad)
    clip = audio[int(w0 * RATE):int(w1 * RATE)]
    sig = hashlib.sha1(clip.tobytes() + f"{w0:.3f}-{w1:.3f}".encode()).hexdigest()
    return w0, clip, sig


def movie_findings(r, row):
    """Why a movie line's window may be wrong, from its aligned row ([] = none), and whether it is unambiguous (every
    word placed: the window may be corrected from it)."""
    s, e = float(r["start"]), float(r["end"])
    words = len(r["text"].split())
    if not row:
        return ["no alignment (the words did not match)"], False
    starts = [int(x) / 1000 for x in row["starts_ms"].split(",")]
    placed = [x for x in starts if x >= 0]
    end = int(row["end_ms"]) / 1000
    out = []
    unplaced = words - len(placed)
    if unplaced:
        out.append(f"{unplaced} of {words} words unplaced (piled at the window's end or on one start)")
    if placed and placed[0] < s - 0.15:
        out.append(f"speech starts {s - placed[0]:.2f} s before the line")
    if placed and placed[0] > s + 0.5:
        out.append(f"speech starts {placed[0] - s:.2f} s after the line")
    if end > e + 0.15:
        out.append(f"speech ends {end - e:.2f} s after the line")
    if placed and placed[-1] >= e + MOVIE_PAD - 0.1:
        out.append("last word at the window's very end (the line may run on)")
    return out, unplaced == 0 and bool(placed)


def fix_windows(rows, aligned):
    """Moves a line's start to its first word and lengthens its end to its last word's end (+0.2 s) when the alignment
    is unambiguous (every word placed) and differs by more than 0.15 s; never past the next line of the same movie,
    never the text. Returns the change log rows."""
    new = {}
    for i, r in enumerate(rows):  # first every start, then the ends (capped by the next line's new start)
        row = aligned.get(i)
        found, clear = movie_findings(r, row)
        if not r["start"] or not clear:
            continue
        s, e = float(r["start"]), float(r["end"])
        starts = [int(x) / 1000 for x in row["starts_ms"].split(",")]
        end = int(row["end_ms"]) / 1000
        ns = s if abs(starts[0] - s) <= 0.15 else int(starts[0] * 20) / 20  # 50 ms steps, rounded down
        ne = max(e, int((end + 0.2) * 20 + 0.999) / 20) if end > e - 0.1 else e  # 50 ms steps, rounded up
        new[i] = (ns, ne, found)
    log = []
    for i, (ns, ne, found) in new.items():
        r = rows[i]
        s, e = float(r["start"]), float(r["end"])
        nxt = next((new[j][0] if j in new else float(rows[j]["start"]) for j in range(i + 1, len(rows))
                    if rows[j]["movie"] == r["movie"] and rows[j]["start"]), None)
        if nxt is not None:
            ne = min(ne, nxt)
        if ns >= ne:
            continue
        if abs(ns - s) > 1e-6 or abs(ne - e) > 1e-6:
            log.append([r["movie"], f"{s:.2f}", f"{e:.2f}", f"{ns:.2f}", f"{ne:.2f}",
                        "; ".join(found) or "first word / end", r["text"], r.get("note", "")])
            r["start"], r["end"] = f"{ns:.2f}", f"{ne:.2f}"
    # a line not corrected keeps its end, but never past a corrected next line's start
    for i, r in enumerate(rows):
        if i in new or not r["start"]:
            continue
        nxt = next((new[j][0] for j in range(i + 1, len(rows)) if rows[j]["movie"] == r["movie"] and rows[j]["start"]
                    and j in new), None)
        nxt_any = next((j for j in range(i + 1, len(rows)) if rows[j]["movie"] == r["movie"] and rows[j]["start"]), None)
        if nxt is not None and nxt_any in new and float(r["end"]) > nxt > float(r["start"]):
            log.append([r["movie"], r["start"], r["end"], r["start"], f"{nxt:.2f}", "the next line starts earlier now",
                        r["text"], r.get("note", "")])
            r["end"] = f"{nxt:.2f}"
    return log


def write_movies(comments, rows, track=MOVIES):
    with replace_on_close(track.path) as f:
        for c in comments:
            f.write(c + "\n")
        f.write("\t".join(track.header) + "\n")
        for r in rows:
            f.write("\t".join(r.get(h, "") for h in track.header) + "\n")


def align_movies(model_get, have, movie_dir, batch, track=MOVIES, index=None, so=None):
    """Every line of a track table aligned in its window (incremental by text + window). have: earlier rows of the
    track's voice keyed (track id, text_fnv); so: the second opinion (None = off), on the line's window +-0.1 s.
    Returns (rows by line index, lines, tracks without audio)."""
    _, rows = read_movies(track)
    audio, missing = {}, set()
    for r in rows:
        if r["movie"] not in audio and r["movie"] not in missing:
            a = movie_audio(r["movie"], movie_dir, track, index)
            if a is None:
                missing.add(r["movie"])
            else:
                audio[r["movie"]] = a
    out, todo = {}, []
    for i, r in enumerate(rows):
        if not r["text"].strip() or not r["start"]:
            continue
        key = (r["movie"], fnv1a(r["text"]))
        if r["movie"] in missing:  # no audio here: an earlier row for the same text stays (it cannot be re-checked)
            if key in have:
                out[i] = have[key]
            continue
        w0, clip, sig = movie_window(r, audio[r["movie"]])
        old = have.get(key)
        if old and old["audio_sha1"] == sig:
            v0, vclip, _ = movie_window(r, audio[r["movie"]], 0.1)
            out[i] = fill_ends(old, r["text"], vclip, v0)
        else:
            todo.append((i, r, w0, clip, sig))
    if todo:
        model = model_get()
        for b in range(0, len(todo), batch):
            chunk = todo[b:b + batch]
            # three windows per line (MOVIE_PADS): music has no pauses to anchor the first word, so a word's start
            # counts only where at least two windows agree (consensus); a word that moves with the window stays -1
            runs = []
            for pad in MOVIE_PADS:
                wins = [movie_window(r, audio[r["movie"]], pad) for _, r, _, _, _ in chunk]
                res = place_retry(model, [w[1] for w in wins], [c[1]["text"] for c in chunk])
                # a word placed on the window's very start (or speech ending on its very end) is where the window
                # cut it, not where it is: no candidate (else a correction chases its own window)
                runs.append([None if m is None else (
                    [w[0] + s if s >= 0 and not (s < EDGE and w[0] > 0) else -0.001 for s in m[0]],
                    None if m[1] > len(w[1]) / RATE - EDGE else w[0] + m[1], m[2]) for w, m in zip(wins, res)])
            for j, (i, r, w0, clip, sig) in enumerate(chunk):
                m = consensus([run[j] for run in runs])
                v0, vclip, _ = movie_window(r, audio[r["movie"]], 0.1)
                if m is not None and so is not None:
                    m = so.apply(vclip, r["text"], m, v0)
                if m is None:
                    continue
                starts, end, collapsed = m
                out[i] = {"id": r["movie"], "voice": track.voice, "text_fnv": fnv1a(r["text"]), "audio_sha1": sig,
                          "end_ms": str(round(end * 1000)),
                          "starts_ms": ",".join(str(round(s * 1000)) if s >= 0 else "-1" for s in starts),
                          "collapsed": str(collapsed),
                          "ends_ms": ends_field(word_ends(r["text"], starts, end, pauses(vclip), v0))}
    print(f"{track.name}: {len(rows)} lines, {len(out)} aligned ({len(todo)} new), no audio: {sorted(missing) or 'none'}")
    return out, rows, missing


# ------------------------------------------------------------------ hand-placed timings (data/word_overrides.tsv)
# The review tool's timeline (asr/word_overrides.py; 29 Oct 2026) stores a line's word starts by hand for one text
# (text_fnv): data/word_times.tsv ships them in place of the aligner's row; the cache keeps the aligner's own.
def read_overrides():
    path, out = ROOT / "data" / "word_overrides.tsv", {}
    if path.exists():
        with open(path, encoding="utf-8", newline="") as f:
            for r in csv.DictReader((l for l in f if not l.startswith("#")), delimiter="\t"):
                out[(r["id"].lower(), r["voice"].lower(), r["text_fnv"])] = r
    return out


OVERRIDE_HEADER = ["id", "voice", "text_fnv", "starts_ms", "ends_ms", "end_ms", "by", "when", "text", "realign"]


def realign_overrides(model_get, overrides, index, movie_dir):
    """Places the words an edit changed in a hand-timed line (column realign, asr/word_overrides.py carry()): each run of
    them is force-aligned inside the gap between its kept neighbours (their end .. the next one's start), so the words
    the reviewer placed stay where they are. A word the aligner cannot place keeps its estimate. Rewrites the file."""
    todo = [r for r in overrides.values() if (r.get("realign") or "").strip()]
    if not todo:
        return 0
    model, done = model_get(), 0
    tracks = {t.voice: t for t in TRACKS}
    audio_of = {}
    for r in todo:
        words = r["text"].split()
        starts = [int(x) for x in r["starts_ms"].split(",")]
        ends = [int(x) for x in r["ends_ms"].split(",")]
        pend = sorted({int(x) for x in r["realign"].split(",") if x.strip().isdigit() and int(x) < len(words)})
        key = (r["id"], r["voice"])
        if key not in audio_of:
            t = tracks.get(r["voice"].lower())
            if t:
                audio_of[key] = movie_audio(r["id"], movie_dir, t, index)
            else:
                f = next((p for (i, v), p in index.items() if i.lower() == r["id"].lower() and v.lower() == r["voice"].lower()), None)
                audio_of[key] = decode(f) if f else None
        audio = audio_of[key]
        if audio is None:
            continue
        total = len(audio) / RATE * 1000
        runs, cur = [], []
        for j in pend:
            if cur and j != cur[-1] + 1:
                runs.append(cur); cur = []
            cur.append(j)
        if cur:
            runs.append(cur)
        for run in runs:
            j1, j2 = run[0], run[-1]
            g0 = next((ends[j] for j in range(j1 - 1, -1, -1) if starts[j] >= 0 and j not in pend), None)
            g1 = next((starts[j] for j in range(j2 + 1, len(words)) if starts[j] >= 0 and j not in pend), None)
            est = [starts[j] for j in run if starts[j] >= 0] + [ends[j] for j in run if ends[j] >= 0]
            g0 = g0 if g0 is not None else max(0, (min(est) if est else 0) - 1000)
            g1 = g1 if g1 is not None else min(total, (max(est) if est else total) + 1000)
            if g1 - g0 < 60:
                continue
            w0 = max(0.0, g0 / 1000 - 0.05)
            clip = audio[int(w0 * RATE):int(min(total, g1 + 50) / 1000 * RATE)]
            res = place_retry(model, [clip], [" ".join(words[j] for j in run)])[0]
            if res is None:
                continue
            got, end, _ = res
            for k, j in enumerate(run):  # inside the gap: never over the kept words around it
                if got[k] >= 0:
                    starts[j] = int(min(max((w0 + got[k]) * 1000, g0), g1 - 40))
            for k, j in enumerate(run):  # a word ends where the next starts; the run's last at the aligner's end
                if k < len(run) - 1:
                    if starts[run[k + 1]] >= 0:
                        ends[j] = starts[run[k + 1]]
                else:
                    ends[j] = min(int((w0 + end) * 1000), int(g1))
                if ends[j] <= starts[j]:
                    ends[j] = starts[j] + 40
        r["starts_ms"] = ",".join(map(str, starts))
        r["ends_ms"] = ",".join(map(str, ends))
        r["end_ms"] = str(max([e for s, e in zip(starts, ends) if s >= 0] or [int(r["end_ms"] or 0)]))
        r["realign"] = ""
        done += 1
    path = ROOT / "data" / "word_overrides.tsv"
    head = [l for l in path.read_text(encoding="utf-8").splitlines() if l.startswith("#")]
    with replace_on_close(path) as f:
        for l in head:
            f.write(l + "\n")
        f.write("\t".join(OVERRIDE_HEADER) + "\n")
        for _, r in sorted(overrides.items()):
            f.write("\t".join(r.get(h) or "" for h in OVERRIDE_HEADER) + "\n")
    print(f"hand-placed timings: {done} edited lines' changed words placed in their gaps")
    return done


def shipped(lid, voice, text, row, overrides):
    """The row data/word_times.tsv ships for a line: its override's timings when there is one for this text."""
    o = overrides.get((lid.lower(), voice.lower(), fnv1a(text)))
    if o is None:
        return row
    return {"id": lid, "voice": voice, "text_fnv": fnv1a(text), "audio_sha1": (row or {}).get("audio_sha1", "manual"),
            "end_ms": o["end_ms"], "starts_ms": o["starts_ms"], "collapsed": "0", "ends_ms": o.get("ends_ms") or ""}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--all", action="store_true", help="align every line again")
    ap.add_argument("--check", action="store_true", help="report stale or missing rows only")
    ap.add_argument("--batch", type=int, default=32)
    ap.add_argument("--retry-suspects", action="store_true",
                    help="align again the current lines with 2+ words unplaced (the padded retry)")
    ap.add_argument("--movies-only", action="store_true",
                    help="the intro movies and scene mixes only (data/movies.tsv, data/mixes.tsv)")
    ap.add_argument("--fix-windows", action="store_true",
                    help="movies, mixes: correct the windows the alignment places unambiguously (logged), then align again")
    ap.add_argument("--overrides-only", action="store_true",
                    help="only the hand-placed timings (data/word_overrides.tsv): place the words edits changed in them "
                         "(the model loads only then), rewrite data/word_times.tsv with them")
    ap.add_argument("--no-second-opinion", action="store_true",
                    help="Qwen's timings only (no CTC second opinion: SecondOpinion)")
    ap.add_argument("--movie-dir", default=str(ROOT.parents[2] / "Install" / "Freelancer HD Edition" / "DATA" / "MOVIES"),
                    help="where <movie>.wmv is found when out/movies/<movie>.wav is not extracted yet")
    a = ap.parse_args()

    _model = []

    def model_get():
        if not _model:
            import torch
            from qwen_asr import Qwen3ForcedAligner
            _model.append(Qwen3ForcedAligner.from_pretrained(MODEL_ID, dtype=torch.bfloat16, device_map="cuda:0"))
        return _model[0]

    so = None if a.no_second_opinion else SecondOpinion()
    index = {}
    with open(ROOT / "out" / "index.tsv", encoding="utf-8") as f:
        for r in csv.DictReader(f, delimiter="\t"):
            # relative paths (extracted clips, "..\out\clips\...") are relative to the pipeline folder asr/
            p = Path(r["file"])
            index[(r["id"], r["voice"])] = str(p if p.is_absolute() else (ROOT / "asr" / p).resolve())
    lines = []
    with open(ROOT / "data" / "subtitles.tsv", encoding="utf-8", newline="") as f:
        head = None
        for raw in f:
            raw = raw.rstrip("\r\n")
            if not raw or raw.startswith("#"):
                continue
            p = raw.split("\t")
            if head is None:
                head = p
                continue
            r = dict(zip(head, p))
            if (r["id"], r["voice"]) in index and r["text"].strip():
                lines.append(r)
    out_path = ROOT / "data" / "word_times.tsv"
    cache_path = ROOT / "out" / "word_times_cache.tsv"  # every aligned row, suspects included (local)
    have, have_tracks = {}, {t.voice: {} for t in TRACKS}
    src = cache_path if cache_path.exists() else out_path
    if src.exists() and not a.all:
        with open(src, encoding="utf-8", newline="") as f:
            for r in csv.DictReader((l for l in f if not l.startswith("#")), delimiter="\t"):
                if r["voice"] in have_tracks:
                    have_tracks[r["voice"]][(r["id"], r["text_fnv"])] = r
                else:
                    have[(r["id"], r["voice"])] = r

    t0 = time.time()
    todo, keep, failed = [], {}, []
    if a.movies_only or a.overrides_only:
        keep = have
    else:
        with ThreadPoolExecutor(16) as pool:
            sigs = list(pool.map(lambda r: sha1(index[(r["id"], r["voice"])]), lines))
        for r, sig in zip(lines, sigs):
            key = (r["id"], r["voice"])
            old = have.get(key)
            if old and old["text_fnv"] == fnv1a(r["text"]) and old["audio_sha1"] == sig and not (
                    a.retry_suspects and int(old.get("collapsed") or 0) >= 2):
                keep[key] = old
            else:
                todo.append((r, sig))
        print(f"{len(lines)} lines with audio: {len(keep)} current, {len(todo)} to align ({time.time() - t0:.1f}s)")
    if a.check:
        for r, _ in todo[:20]:
            print(f"  stale: {r['id']} {r['voice']}")
        sys.exit(1 if todo else 0)

    if todo:
        model = model_get()
        with ThreadPoolExecutor(16) as pool:
            for b in range(0, len(todo), a.batch):
                chunk = todo[b : b + a.batch]
                auds = list(pool.map(lambda x: decode(index[(x[0]["id"], x[0]["voice"])]), chunk))
                res = place_retry(model, auds, [r["text"] for r, _ in chunk])
                if so is not None:
                    res = [so.apply(au, r["text"], m) for au, (r, _), m in zip(auds, chunk, res)]
                for au, (r, sig), m in zip(auds, chunk, res):
                    key = (r["id"], r["voice"])
                    if m is None:
                        failed.append(r)
                        keep.pop(key, None)
                        continue
                    starts, end, collapsed = m
                    keep[key] = {"id": r["id"], "voice": r["voice"], "text_fnv": fnv1a(r["text"]), "audio_sha1": sig,
                                 "end_ms": str(round(end * 1000)),
                                 "starts_ms": ",".join(str(round(s * 1000)) for s in starts),
                                 "collapsed": str(collapsed),
                                 "ends_ms": ends_field(word_ends(r["text"], starts, end, pauses(au)))}
                if (b // a.batch) % 50 == 0:
                    print(f"  {b + len(chunk)}/{len(todo)} ({time.time() - t0:.0f}s)", flush=True)
    if not (a.movies_only or a.overrides_only):  # rows aligned before word ends existed: theirs from the audio
        bare = [r for r in lines if not keep.get((r["id"], r["voice"]), {"ends_ms": "-"}).get("ends_ms")]
        if bare:
            with ThreadPoolExecutor(16) as pool:
                rows = pool.map(lambda r: fill_ends(keep[(r["id"], r["voice"])], r["text"],
                                                    decode(index[(r["id"], r["voice"])])), bare)
                for r, row in zip(bare, rows):
                    keep[(r["id"], r["voice"])] = row
            print(f"word ends measured for {len(bare)} rows aligned before ({time.time() - t0:.0f}s)")

    # the intro movies and the scene mixes: aligned when their audio is here, else their earlier rows stay (text hash
    # still matching)
    movie_out = []
    for track in TRACKS:
        have_t = have_tracks[track.voice]
        if a.overrides_only:  # the earlier rows of the current lines, as they are
            _, mlines = read_movies(track)
            movie_out += [(r, have_t[(r["movie"], fnv1a(r["text"]))]) for r in mlines
                          if r["text"].strip() and (r["movie"], fnv1a(r["text"])) in have_t]
            continue
        movie_rows, mlines, _ = align_movies(model_get, have_t, a.movie_dir, a.batch, track, index, so)
        if a.fix_windows:
            comments, rows = read_movies(track)
            changes = fix_windows(rows, movie_rows)
            if changes:
                write_movies(comments, rows, track)
                log_path = ROOT / "out" / f"{track.prefix}_window_changes.tsv"  # appended: every pass stays logged
                new_log = not log_path.exists()
                with open(log_path, "a", encoding="utf-8", newline="") as f:
                    if new_log:
                        f.write(f"when\t{track.col}\tstart\tend\tnew_start\tnew_end\twhy\ttext\tnote\n")
                    stamp = time.strftime("%Y-%m-%d %H:%M")
                    for c in changes:
                        f.write(stamp + "\t")
                        f.write("\t".join(c) + "\n")
                print(f"{track.name}: {len(changes)} windows corrected (out/{log_path.name}); aligning them again")
                have_t.update({(r["id"], r["text_fnv"]): r for r in movie_rows.values()})
                movie_rows, mlines, _ = align_movies(model_get, have_t, a.movie_dir, a.batch, track, index, so)
            else:
                print(f"{track.name}: no window to correct")
        msus = []
        for i, r in enumerate(mlines):
            if not r["text"].strip() or not r["start"]:
                continue
            found, _ = movie_findings(r, movie_rows.get(i))
            if found:
                msus.append([r["movie"], r["start"], r["end"], "; ".join(found), r["text"], r.get("note", "")])
        sus = ROOT / "out" / f"{track.prefix}_word_times_suspect.tsv"
        with replace_on_close(sus) as f:
            f.write(f"{track.col}\tstart\tend\twhy\ttext\tnote\n")
            for s in msus:
                f.write("\t".join(s) + "\n")
        print(f"{track.name}: {len(msus)} suspect lines (out/{sus.name})")
        movie_out += [(r, movie_rows[i]) for i, r in enumerate(mlines) if i in movie_rows]

    overrides = read_overrides()
    realign_overrides(model_get, overrides, index, a.movie_dir)  # edited hand-timed lines: their changed words
    with replace_on_close(out_path) as f:
        f.write("# Word starts of each subtitle line in its recording (FLSubtitles/aligner/align_words.py, "
                "Qwen3-ForcedAligner-0.6B; hand-placed timings from data/word_overrides.tsv). Generated, do not edit.\n")
        f.write("\t".join(HEADER) + "\n")
        for r in lines:
            row = shipped(r["id"], r["voice"], r["text"], keep.get((r["id"], r["voice"])), overrides)
            # a line with most of its words unplaced (garbled or noisy on purpose, or a text to check) ships no
            # timings: the runtime keeps its estimate (fail closed; user, 22 Oct 2026: "some lines definitely have
            # noise and/or garbled voices on purpose"); fewer unplaced words ship as -1 (the rest still turn pages).
            # Every row stays in the cache below for the incremental run.
            if row and int(row.get("collapsed") or 0) * 2 <= len(r["text"].split()):
                f.write("\t".join(cell(row, h) for h in HEADER) + "\n")
        # the intro movies' and scene mixes' lines (voice "movie" / "mix", times on the track's clock; data.cpp skips
        # them - no line has that voice -, movie.cpp reads the movies')
        for r, row in movie_out:
            row = shipped(row["id"], row["voice"], r["text"], row, overrides)
            if int(row.get("collapsed") or 0) * 2 <= len(r["text"].split()):
                f.write("\t".join(cell(row, h) for h in HEADER) + "\n")
    if a.overrides_only:  # the cache and the side lists stay as the last aligning run wrote them
        print(f"done: data/word_times.tsv with {len(overrides)} hand-placed timings")
        return
    with replace_on_close(cache_path) as f:
        f.write("\t".join(HEADER) + "\n")
        for r in lines:
            row = keep.get((r["id"], r["voice"]))
            if row:
                f.write("\t".join(cell(row, h) for h in HEADER) + "\n")
        for r, row in movie_out:
            f.write("\t".join(cell(row, h) for h in HEADER) + "\n")
    if a.movies_only:  # the dialogue's side products stay as the last full run wrote them
        print(f"done in {time.time() - t0:.0f}s (movies only)")
        return
    fail_path = ROOT / "out" / "word_times_failed.tsv"
    with replace_on_close(fail_path) as f:
        f.write("id\tvoice\ttext\n")
        for r in failed:
            f.write(f"{r['id']}\t{r['voice']}\t{r['text']}\n")
    # Words the aligner could not place pile up at the clip's very end: two or more there mean the text says more than
    # the recording (a transcription to check - found 22 Oct 2026: Battleship Unity's garbled DX_M04_0217_UNITY)
    sus_path = ROOT / "out" / "word_times_suspect.tsv"
    suspects = [r for r in lines if int(keep.get((r["id"], r["voice"]), {}).get("collapsed", "0") or 0) >= 2]
    with replace_on_close(sus_path) as f:
        # words = the line's word count, ratio = collapsed / words: rank the list without re-reading subtitles.tsv
        f.write("id\tvoice\tcollapsed\twords\tratio\ttext\n")
        for r in suspects:
            col, words = int(keep[(r['id'], r['voice'])]['collapsed']), len(r['text'].split())
            f.write(f"{r['id']}\t{r['voice']}\t{col}\t{words}\t{col / max(words, 1):.2f}\t{r['text']}\n")
    print(f"done in {time.time() - t0:.0f}s: {len(keep)} rows, {len(failed)} not matched ({fail_path.name}), "
          f"{len(suspects)} with 2+ words the aligner could not place ({sus_path.name})")


if __name__ == "__main__":
    main()
