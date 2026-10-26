"""Read FLSubs / "Freelancer Subtitles" subtitles.xml (Lorash 2014-16, L@Zar0 2019; GPL-3.0) as a text source.

Entries are keyed by audio file below DATA\\AUDIO (story sounds: `DIALOGUE\\TRENT\\x.wav`) or by
voice bank + hash (`rvp151.utf|0x80062682`). `key_for_row` builds the same key for a row of our index.
Static entries cover the whole clip; Dynamic entries hold timed parts (start, end, text).
"""
import xml.etree.ElementTree as ET
from pathlib import Path


def load(xml_path) -> dict:
    """key -> (text, parts [(start, end, text)], text area)"""
    out = {}
    for e in ET.parse(xml_path).getroot():
        key = e.get("AudioFile", "").lower()
        if e.tag == "Static":
            text, parts = " ".join((e.text or "").split()), []
        else:
            parts = [(p.get("Start"), p.get("End"), " ".join((p.text or "").split())) for p in e]
            text = " ".join(t for _, _, t in parts)
        out[key] = (text, parts, e.get("TextArea", ""))
    return out


def key_for_row(kind: str, voice: str, file: str) -> str:
    if kind == "story":
        p = file.replace("/", "\\").lower()
        i = p.find("\\data\\audio\\")
        return p[i + len("\\data\\audio\\"):] if i >= 0 else p
    return f"{voice}.utf|0x{Path(file).stem[2:].upper()}".lower()
