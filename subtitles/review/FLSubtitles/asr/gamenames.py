"""Names the game's chatter fragments speak, taken from the game's own data.

    gamenames.py --game <install> --out <dir>      writes <dir>/gamenames.tsv

The chatter system links voice fragments to game objects through `msg_id_prefix` keys: systems,
bases and solar objects (universe.ini, system INIs), factions (faction_prop.ini, names from
initialworld.ini [Group]), commodities (goods.ini), ship types (shiparch.ini), mission bases
(mbases.ini). A fragment msg id starts with that prefix (`gcs_refer_base_Li01_01_Base` +
`-`/`+`/`_short`/`_long`...). Names come from the resource DLLs (ids >> 16 = DLL index).
Columns: prefix, name, short name, kind, file.
"""
import argparse
import ctypes
import re
import sys
from ctypes import wintypes
from pathlib import Path

NAME_KEYS = ("strid_name", "ids_name")


def ini_sections(path: Path):
    """Yield (section name, {key: [values]}) from a plain-text INI."""
    sec, vals = None, {}
    for line in path.read_text(encoding="latin-1", errors="replace").splitlines():
        line = line.split(";")[0].strip()
        if not line:
            continue
        if line.startswith("["):
            if sec is not None:
                yield sec, vals
            sec, vals = line.strip("[]").strip().lower(), {}
        elif "=" in line and sec is not None:
            k, v = line.split("=", 1)
            vals.setdefault(k.strip().lower(), []).append(v.strip())
    if sec is not None:
        yield sec, vals


class Strings:
    def __init__(self, game: Path):
        exe = game / "EXE"
        names = ["resources.dll"]
        sec = None
        for line in (exe / "freelancer.ini").read_text(encoding="latin-1").splitlines():
            s = line.split(";")[0].strip()
            if s.startswith("["):
                sec = s.strip("[]").lower()
            elif sec == "resources" and s.lower().startswith("dll"):
                names.append(s.split("=", 1)[1].strip())
        k32 = ctypes.WinDLL("kernel32", use_last_error=True)
        k32.LoadLibraryExW.restype = wintypes.HMODULE
        k32.LoadLibraryExW.argtypes = (wintypes.LPCWSTR, wintypes.HANDLE, wintypes.DWORD)
        self.user32 = ctypes.WinDLL("user32")
        self.user32.LoadStringW.argtypes = (wintypes.HMODULE, wintypes.UINT, wintypes.LPWSTR, ctypes.c_int)
        self.dlls = [k32.LoadLibraryExW(str(exe / n), None, 0x2) for n in names]  # LOAD_LIBRARY_AS_DATAFILE

    def get(self, ids) -> str:
        try:
            ids = int(ids)
        except (TypeError, ValueError):
            return ""
        i = ids >> 16
        if ids <= 0 or i >= len(self.dlls) or not self.dlls[i]:
            return ""
        buf = ctypes.create_unicode_buffer(512)
        n = self.user32.LoadStringW(self.dlls[i], ids & 0xFFFF, buf, 512)
        return " ".join(buf.value[:n].split()) if n > 0 else ""


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--game", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    game = Path(a.game)
    data = game / "DATA"
    strings = Strings(game)

    groups = {}
    for sec, v in ini_sections(data / "initialworld.ini"):
        if sec == "group" and "nickname" in v:
            groups[v["nickname"][0].lower()] = (v.get("ids_name", [""])[0], v.get("ids_short_name", [""])[0])

    files = []
    for f in sorted(data.rglob("*.ini")):
        try:
            files.append((f, list(ini_sections(f))))
        except OSError:
            continue
    # any nickname -> name ids (bases in universe.ini, commodities in the equipment INIs, ...)
    nick_ids = {}
    for f, sections in files:
        for sec, v in sections:
            if "nickname" in v:
                for k in NAME_KEYS:
                    if k in v:
                        nick_ids.setdefault(v["nickname"][0].lower(), v[k][0])
                        break
    # A system's own [system] entry beats objects that share its prefix (jump holes, gates).
    rank = {"system": 0, "mbase": 1, "base": 1, "faction": 2, "good": 3, "ship": 4}
    rows = {}
    for f, sections in files:
        for sec, v in sections:
            if "msg_id_prefix" not in v:
                continue
            prefix = v["msg_id_prefix"][0]
            name = short = ""
            kind = sec
            if "affiliation" in v and f.name.lower() == "faction_prop.ini":
                ids, sids = groups.get(v["affiliation"][0].lower(), ("", ""))
                name, short, kind = strings.get(ids), strings.get(sids), "faction"
            else:
                for k in NAME_KEYS:
                    if k in v:
                        name = strings.get(v[k][0])
                        break
                if not name and "nickname" in v:
                    name = strings.get(nick_ids.get(v["nickname"][0].lower()))
            old = rows.get(prefix.lower())
            # Several goods reuse one voice prefix (VIPs and prisoners "say" boron): the name comes from
            # the entry the prefix is named after (commodity_boron for gcs_gen_commodity_boron).
            tail = prefix.lower().rsplit("_", 1)[-1]
            own = "nickname" in v and v["nickname"][0].lower().endswith(tail)
            better = old is None or (name and not old[1]) or \
                (name and rank.get(kind, 9) < rank.get(old[3], 9)) or \
                (name and own and kind == old[3] == "good")
            if better:
                rows[prefix.lower()] = (prefix, name, short, kind, str(f.relative_to(data)))
    # System prefixes speak the system: its name comes from universe.ini [system] (some systems' prefix
    # is only declared on objects inside them, e.g. a jump hole in St01).
    for p, r in list(rows.items()):
        m = re.match(r"gcs_refer_system_([a-z]+\d+)$", p)
        if m and m.group(1) in nick_ids:
            name = strings.get(nick_ids[m.group(1)])
            if name:
                rows[p] = (r[0], name, r[2], "system", "Universe\\universe.ini")
    # Formation designations (ship codenames "Alpha 3-7"): gcs_refer_formationdesig_N speaks string
    # 197808 + N - 1 (Alpha ... Omega, Red ... Yellow, Matsu ... Yanagi); voice_properties.ini declares
    # 29 of them (permutation_count = gcs_refer_formationdesig, 29), faction_prop.ini picks ranges.
    for n in range(1, 30):
        name = strings.get(197808 + n - 1)
        rows[f"gcs_refer_formationdesig_{n}"] = (f"gcs_refer_formationdesig_{n}", name, "", "designation",
                                                 "resources (string 197808+)")
    out = Path(a.out)
    with open(out / "gamenames.tsv", "w", encoding="utf-8", newline="\n") as fo:
        fo.write("prefix\tname\tshort\tkind\tfile\n")
        for r in sorted(rows.values(), key=lambda r: r[0].lower()):
            fo.write("\t".join(r) + "\n")
    kinds = {}
    for r in rows.values():
        kinds.setdefault(r[3], [0, 0])
        kinds[r[3]][0] += 1
        kinds[r[3]][1] += bool(r[1])
    for k, (n, named) in sorted(kinds.items()):
        print(f"{k:12} {n:5} prefixes, {named} with a name")


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    main()
