"""Batch speech-to-text for FLSubtitles (Qwen3-ASR-1.7B via Transformers, CUDA).

Input: a TSV with `id<TAB>audio path` per line (any format ffmpeg reads, incl. Freelancer's MP3-in-WAV).
Output: a TSV with `id<TAB>seconds<TAB>text`. The output is game content: write it outside the repository.

    uv run python -I transcribe.py --list clips.tsv --out text.tsv [--glossary glossary.txt] [--batch 64] [--batch-seconds 600]

The output is appended to and ids already in it are skipped, so an interrupted run resumes.

The model is cached in .cache/ next to this script (ignored by git).
"""
import argparse
import re
import os
import warnings
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

HERE = Path(__file__).resolve().parent
os.environ.setdefault("HF_HOME", str(HERE / ".cache" / "hf"))
os.environ.setdefault("HF_HUB_DISABLE_SYMLINKS_WARNING", "1")  # Windows without developer mode
warnings.filterwarnings("ignore", category=UserWarning, module="huggingface_hub")

import numpy as np  # noqa: E402
import torch  # noqa: E402
from transformers import AutoModelForMultimodalLM, AutoProcessor  # noqa: E402

MODEL_ID = "Qwen/Qwen3-ASR-1.7B-hf"
RATE = 16000


def decode(path: str) -> np.ndarray:
    """Decode any audio file to 16 kHz mono float32 with ffmpeg."""
    raw = subprocess.run(
        ["ffmpeg", "-v", "error", "-i", path, "-f", "f32le", "-ac", "1", "-ar", str(RATE), "-"],
        check=True, capture_output=True,
    ).stdout
    return np.frombuffer(raw, dtype=np.float32)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--list", required=True)
    ap.add_argument("--out", required=True, help="appended to; ids already in it are skipped (resume)")
    ap.add_argument("--glossary", help="text file: names/terms to bias recognition (one prompt)")
    ap.add_argument("--batch", type=int, default=64, help="max clips per batch")
    ap.add_argument("--batch-seconds", type=float, default=600, help="max audio seconds per batch")
    ap.add_argument("--chunk", type=int, default=2048, help="clips decoded and sorted at a time")
    args = ap.parse_args()

    done = set()
    if Path(args.out).exists():
        done = {l.split("\t", 1)[0] for l in Path(args.out).read_text(encoding="utf-8").splitlines() if l}
    clips = []
    for line in Path(args.list).read_text(encoding="utf-8").splitlines():
        if line.strip():
            cid, path = line.split("\t")[:2]
            if cid not in done:
                clips.append((cid, path))
    print(f"{len(clips)} clips to do ({len(done)} already done)", file=sys.stderr)
    if not clips:
        return
    prompt = None
    if args.glossary:
        prompt = Path(args.glossary).read_text(encoding="utf-8").strip()

    t0 = time.time()
    processor = AutoProcessor.from_pretrained(MODEL_ID)
    model = AutoModelForMultimodalLM.from_pretrained(MODEL_ID, device_map="cuda", dtype=torch.bfloat16)
    print(f"model loaded in {time.time() - t0:.1f}s", file=sys.stderr)

    terms = [t.strip().rstrip(".").lower() for t in (prompt or "").split("Vocabulary:", 1)[-1].split(",")
             if t.strip()] if prompt else []

    def echoed(text: str) -> bool:
        """The glossary read back as the transcription (screams, near-silence): three glossary terms in
        glossary order, back to back with only separators between them ("Edison Trent, Trent, Jun'ko
        Zane"). A real line naming characters ("Quintaine and Sinclair ... Orillion") doesn't match."""
        if len(terms) < 3:
            return False
        low = text.lower()
        sep = r"[\s,.;:!?-]+"
        for i in range(len(terms) - 2):
            pat = sep.join(r"\b" + re.escape(t) + r"\b" for t in terms[i:i + 3])
            if re.search(pat, low):
                return True
        return False

    def run(batch, use_prompt=True):
        kwargs = {"language": ["English"] * len(batch)}
        if prompt and use_prompt:
            kwargs["prompt"] = [prompt] * len(batch)
        inputs = processor.apply_transcription_request([a for _, a in batch], **kwargs).to(model.device, model.dtype)
        # Speech runs ~3-4 tokens per second. A cap scaled to the longest clip stops the occasional
        # repetition loop on tiny clips from stalling the whole batch at a fixed 400 tokens.
        longest = max(len(a) for _, a in batch) / RATE
        with torch.inference_mode():
            ids = model.generate(**inputs, max_new_tokens=int(min(1200, 16 + 8 * longest)))
        return processor.decode(ids[:, inputs["input_ids"].shape[1]:], return_format="transcription_only")

    pool = ThreadPoolExecutor(max_workers=os.cpu_count() or 8)
    first = True
    total_sec, gpu_time, n_done = 0.0, 0.0, 0
    with open(args.out, "a", encoding="utf-8", newline="\n") as out:
        for c0 in range(0, len(clips), args.chunk):
            part = clips[c0:c0 + args.chunk]
            decoded = list(zip([c for c, _ in part], pool.map(decode, [p for _, p in part])))
            decoded.sort(key=lambda x: len(x[1]))  # similar lengths per batch
            if first:  # one-time CUDA setup (~10 s), kept out of the speed figure
                run(decoded[:1])
                first = False
            i = 0
            while i < len(decoded):
                batch, secs = [], 0.0
                while i < len(decoded) and len(batch) < args.batch:
                    s = len(decoded[i][1]) / RATE
                    if batch and secs + s > args.batch_seconds:
                        break
                    batch.append(decoded[i])
                    secs += s
                    i += 1
                t1 = time.time()
                texts = run(batch)
                redo = [k for k, t in enumerate(texts) if echoed(t)]
                if redo:  # glossary read back: transcribe those clips again without it
                    for k, t in zip(redo, run([batch[k] for k in redo], use_prompt=False)):
                        texts[k] = t
                gpu_time += time.time() - t1
                total_sec += secs
                for (cid, a), text in zip(batch, texts):
                    out.write(f"{cid}\t{len(a) / RATE:.2f}\t{' '.join(text.split())}\n")
                out.flush()
                n_done += len(batch)
            print(f"{n_done}/{len(clips)}  {total_sec / 60:.0f} min audio  RTFx {total_sec / max(gpu_time, 1e-6):.0f}"
                  f"  elapsed {time.time() - t0:.0f}s", file=sys.stderr, flush=True)
    print(f"done: {n_done} clips, {total_sec / 60:.1f} min audio, GPU {gpu_time:.0f}s "
          f"(RTFx {total_sec / max(gpu_time, 1e-6):.0f}), wall {time.time() - t0:.0f}s", file=sys.stderr)


if __name__ == "__main__":
    main()
