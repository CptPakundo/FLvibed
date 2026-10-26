"""Build the index of every voiced line of a Freelancer / HD Edition install.

    build_index.py --game <install> --script <shooting_script.txt> --out <dir>

Writes into <dir> (game content: keep it outside the repository, e.g. FLSubtitles/out/):
  clips/<voice>/<0xHASH>.wav   every voice-bank entry, extracted as-is (MP3 in WAV)
  index.tsv                    one row per line, columns:
      kind      story | bank | bank-orphan (bank entry no voices INI names)
      id        story sound nickname / bank msg id / 0xHASH for orphans
      voice     voice nickname (bank) or speaker folder (story)
      source    the INI that defines it
      sec       duration from the WAV header
      script    exact | number (same line number, other speaker tag) | -
      speaker   speaker from the script, else derived from the id / voice
      text      spoken text from the script (delivery notes and [..] removed), else empty
      file      audio file path
  transcribe.tsv               id<TAB>file for transcribe.py
"""
import argparse
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))  # run with python -I: no implicit script dir
from flhash import create_id  # noqa: E402
from script_check import parse_script  # noqa: E402

ID_KEY = re.compile(r"^(DX_[A-Z0-9]+_[0-9]+[A-Z]?)_")


def ini_entries(path: Path):
    """Yield (section, key, value) from a plain-text Freelancer INI (comments stripped)."""
    section = None
    for line in path.read_text(encoding="latin-1").splitlines():
        line = line.split(";")[0].strip()
        if not line:
            continue
        if line.startswith("["):
            section = line.strip("[]").strip().lower()
            yield section, None, None
        elif "=" in line:
            k, v = line.split("=", 1)
            yield section, k.strip().lower(), v.strip()


def data_lists(game: Path) -> dict:
    """freelancer.ini [Data]: key -> list of INI paths (relative to DATA)."""
    out = {}
    sec = None
    for s, k, v in ini_entries(game / "EXE" / "freelancer.ini"):
        sec = s
        if sec == "data" and k:
            out.setdefault(k, []).append(game / "DATA" / v)
    return out


def utf_leaves(path: Path):
    """Yield (name, bytes) for every leaf of a UTF file."""
    b = path.read_bytes()
    if b[:4] != b"UTF ":
        raise ValueError(f"not a UTF file: {path}")
    node_off, str_off, data_off = struct.unpack_from("<I", b, 8)[0], struct.unpack_from("<I", b, 24)[0], \
        struct.unpack_from("<I", b, 36)[0]

    def name(o):
        s = str_off + o
        return b[s:b.index(b"\0", s)].decode("latin-1")

    stack = [0]
    while stack:
        rel = stack.pop()
        while True:
            n = node_off + rel
            peer, nm, flags, _, child = struct.unpack_from("<IIIII", b, n)
            if flags & 0x10:
                stack.append(child)
            else:
                size = struct.unpack_from("<I", b, n + 24)[0]
                yield name(nm), b[data_off + child:data_off + child + size]
            if peer == 0:
                break
            rel = peer


def wav_seconds(b: bytes) -> float:
    o, rate, data = 12, 0, 0
    while o + 8 <= len(b):
        cid, size = b[o:o + 4], struct.unpack_from("<I", b, o + 4)[0]
        if cid == b"fmt ":
            rate = struct.unpack_from("<I", b, o + 16)[0]  # average bytes per second
        elif cid == b"data":
            data = size
        o += 8 + size + (size & 1)
    return data / rate if rate else 0.0


def speaker_from_id(lid: str, voice: str) -> str:
    """DX_M01_0100_KING -> KING; dx_s006x_0002_King -> KING; otherwise the voice nickname."""
    m = re.match(r"^DX_[A-Z0-9]+_[0-9]+[A-Z]?_(.+)$", lid.upper())
    if m:
        return m.group(1).replace("_", " ")
    return voice


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--game", required=True)
    ap.add_argument("--script", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    game, out = Path(a.game).resolve(), Path(a.out).resolve()  # absolute paths in the index
    (out / "clips").mkdir(parents=True, exist_ok=True)

    script = parse_script(Path(a.script))
    by_number = {}
    for k, v in script.items():
        m = ID_KEY.match(k)
        if m:
            by_number.setdefault(m.group(1), v)

    def script_for(lid: str):
        u = lid.upper()
        if u in script:
            return "exact", script[u]
        m = ID_KEY.match(u)
        if m and m.group(1) in by_number:
            return "number", by_number[m.group(1)]
        return "-", None

    rows = []
    lists = data_lists(game)

    # story sounds (type = voice)
    for ini in lists.get("sounds", []):
        cur = {}
        for s, k, v in list(ini_entries(ini)) + [("end", None, None)]:
            if k is None:
                if cur.get("type", "").lower() == "voice" and "nickname" in cur and "file" in cur:
                    f = game / "DATA" / cur["file"]
                    if f.exists():
                        sec = wav_seconds(f.read_bytes())
                        how, sv = script_for(cur["nickname"])
                        rows.append(["story", cur["nickname"], f.parent.name.lower(), ini.name, f"{sec:.2f}", how,
                                     sv[0] if sv else speaker_from_id(cur["nickname"], ""), sv[1] if sv else "",
                                     str(f)])
                cur = {}
            else:
                cur[k] = v

    # voice banks
    msgs = {}  # voice -> {hash: (msg, ini)}
    for ini in lists.get("voices", []):
        voice = None
        for s, k, v in ini_entries(ini):
            if k in ("nickname", "extend") and s == "voice":
                voice = v.lower()
            elif k == "msg" and s == "sound" and voice:
                msgs.setdefault(voice, {})[create_id(v)] = (v, ini.name)
    banks = sorted((game / "DATA" / "AUDIO").glob("*.utf"))
    for bank in banks:
        voice = bank.stem.lower()
        known = msgs.get(voice, {})
        vdir = out / "clips" / voice
        vdir.mkdir(exist_ok=True)
        for name, data in utf_leaves(bank):
            if not name.lower().startswith("0x") or data[:4] != b"RIFF":
                continue
            h = int(name, 16)
            f = vdir / f"0x{h:08X}.wav"
            if not f.exists() or f.stat().st_size != len(data):
                f.write_bytes(data)
            sec = wav_seconds(data)
            if h in known:
                msg, src = known[h]
                how, sv = script_for(msg)
                rows.append(["bank", msg, voice, src, f"{sec:.2f}", how,
                             sv[0] if sv else speaker_from_id(msg, voice), sv[1] if sv else "", str(f)])
            else:
                rows.append(["bank-orphan", f"0x{h:08X}", voice, bank.name, f"{sec:.2f}", "-", voice, "", str(f)])

    # one id may appear in several banks (extend): keep ids unique by prefixing the voice for banks
    with open(out / "index.tsv", "w", encoding="utf-8", newline="\n") as f:
        f.write("kind\tid\tvoice\tsource\tsec\tscript\tspeaker\ttext\tfile\n")
        for r in rows:
            f.write("\t".join(c.replace("\t", " ") for c in r) + "\n")
    with open(out / "transcribe.tsv", "w", encoding="utf-8", newline="\n") as f:
        for r in rows:
            f.write(f"{r[2]}/{r[1]}\t{r[8]}\n")

    def count(pred):
        sel = [r for r in rows if pred(r)]
        return len(sel), sum(float(r[4]) for r in sel) / 60

    for kind in ("story", "bank", "bank-orphan"):
        n, m = count(lambda r: r[0] == kind)
        e, _ = count(lambda r: r[0] == kind and r[5] == "exact")
        nb, _ = count(lambda r: r[0] == kind and r[5] == "number")
        print(f"{kind:12} {n:6} lines {m:7.1f} min   script exact {e}, by number {nb}")


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    main()
