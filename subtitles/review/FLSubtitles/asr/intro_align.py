"""Timed narration for the intro movies (FLSubtitles): speech to text plus window timing, no aligner model.

The movies' narration runs over music without pauses, so they cannot be cut at silences. Instead:
1. transcribe the whole audio in 30 s chunks (5 s overlap) -> the narration text (to correct by hand);
2. transcribe short windows (2 s, every 0.5 s) -> what is heard around each moment;
3. with the corrected sentences (--sentences, one per line), each sentence starts in the first window that
   holds its first words and ends in the last window holding its last words, searching forward in order.

    uv run python -I intro_align.py --audio fl_intro.wav --out intro_dir [--sentences fixed.txt]

Writes <out>/<name>.chunks.tsv (step 1), <name>.windows.tsv (step 2) and, with --sentences,
<name>.timed.tsv (start, end, text). The output is game content: keep it out of the repository.
"""
import argparse
import re
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))  # run with -I: import the sibling module explicitly
import transcribe as tr  # noqa: E402


def load(path: Path) -> np.ndarray:
    return tr.decode(str(path))


def words(t: str) -> list:
    return re.findall(r"[a-z0-9']+", t.lower())


_MODEL = None


def model():
    """The pipeline's speech-to-text model (transcribe.py), loaded once."""
    global _MODEL
    if _MODEL is None:
        proc = tr.AutoProcessor.from_pretrained(tr.MODEL_ID)
        m = tr.AutoModelForMultimodalLM.from_pretrained(tr.MODEL_ID, device_map="cuda", dtype=tr.torch.bfloat16)
        _MODEL = (m, proc)
    return _MODEL


def transcribe_batch(audios: list) -> list:
    m, proc = model()
    inputs = proc.apply_transcription_request(audios, language=["English"] * len(audios)).to(m.device, m.dtype)
    longest = max(len(x) for x in audios) / tr.RATE
    with tr.torch.inference_mode():
        ids = m.generate(**inputs, max_new_tokens=int(min(1200, 16 + 8 * longest)))
    return proc.decode(ids[:, inputs["input_ids"].shape[1]:], return_format="transcription_only")


def run_asr(clips: list, out: Path) -> list:
    """clips: [(id, float32 audio)] -> [(id, text)], cached in out (TSV id<TAB>text)."""
    done = {}
    if out.exists():
        for l in out.read_text(encoding="utf-8").splitlines():
            k, _, v = l.partition("\t")
            done[k] = v
    todo = [(k, a) for k, a in clips if k not in done]
    if todo:
        for i in range(0, len(todo), 32):
            batch = todo[i:i + 32]
            texts = transcribe_batch([a for _, a in batch])
            with open(out, "a", encoding="utf-8", newline="\n") as fo:
                for (k, _), t in zip(batch, texts):
                    done[k] = t.replace("\t", " ").replace("\n", " ")
                    fo.write(f"{k}\t{done[k]}\n")
    return [(k, done[k]) for k, _ in clips]


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--audio", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--sentences")
    ap.add_argument("--win", type=float, default=2.0)
    ap.add_argument("--hop", type=float, default=0.5)
    a = ap.parse_args()
    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    name = Path(a.audio).stem
    audio = load(Path(a.audio))
    sr = tr.RATE
    n = len(audio) / sr

    chunks = [(f"{s:07.2f}", audio[int(s * sr):int(min(n, s + 30) * sr)]) for s in np.arange(0, n, 25.0)]
    for k, t in run_asr(chunks, out / f"{name}.chunks.tsv"):
        print(k, t)

    starts = np.arange(0, max(0.0, n - a.win) + 1e-6, a.hop)
    windows = [(f"{s:07.2f}", audio[int(s * sr):int((s + a.win) * sr)]) for s in starts]
    wins = [(float(k), set(words(t))) for k, t in run_asr(windows, out / f"{name}.windows.tsv")]

    if not a.sentences:
        return
    rows, pos = [], 0
    for sent in [l.strip() for l in Path(a.sentences).read_text(encoding="utf-8").splitlines() if l.strip()]:
        w = [x for x in words(sent) if len(x) > 2] or words(sent)
        head, tail = set(w[:3]), set(w[-3:])
        start = end = None
        for i in range(pos, len(wins)):
            if start is None and len(head & wins[i][1]) >= min(2, len(head)):
                start, pos = wins[i][0], i
            if start is not None and len(tail & wins[i][1]) >= min(2, len(tail)):
                end = wins[i][0] + a.win
                pos = i
                # extend while the tail is still heard
                while pos + 1 < len(wins) and len(tail & wins[pos + 1][1]) >= min(2, len(tail)):
                    pos += 1
                    end = wins[pos][0] + a.win
                break
        # A phrase first heard in the window starting at s (and not in the one before) began about
        # s + win - hop; its last words, heard last in the window at s', end about s' + hop + ~0.8 s.
        if start is not None:
            start += a.win - a.hop
        if end is not None:
            end = end - a.win + a.hop + 0.8
        if start is not None:  # short lines heard within one window: at least ~0.3 s per word
            end = max(end or 0.0, start + 0.3 * len(words(sent)))
        rows.append([start, end, sent])
    for i in range(1, len(rows)):  # one line at a time: a line ends where the next begins
        if rows[i][0] is not None and rows[i - 1][1] is not None and rows[i - 1][1] > rows[i][0]:
            rows[i - 1][1] = rows[i][0]
    for s, e, t in rows:
        print(f"{s}\t{e}\t{t}")
    with open(out / f"{name}.timed.tsv", "w", encoding="utf-8", newline="\n") as fo:
        fo.write("start\tend\ttext\n")
        for s, e, t in rows:
            fo.write(f"{'' if s is None else f'{s:.2f}'}\t{'' if e is None else f'{e:.2f}'}\t{t}\n")


if __name__ == "__main__":
    main()
