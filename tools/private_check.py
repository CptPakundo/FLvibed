#!/usr/bin/env python
"""The private-file check of the published export (the design bench's guard, audit 71b section 6).

The public repository gets `git archive` exports (CLAUDE.md Rules: Git). This check reads what such an export would hold
(the archive itself, so `export-ignore` is applied exactly as git applies it) and refuses it when it holds:
  private-folder  a path under one of the bench's private folders (tools/hudbench/private/, tools/hudbench/tour/fallback/,
                  tests/bench/): their export-ignore lines are missing or broken
  recording       a recorded state or display list by name (*.flstate.json, *.fldl.json, any .flstate. / .fldl. form)
  game-file       a game texture file (*.txm)
  atlas           an atlas image by name (an image named *atlas*, font_<set>_<px>*, icons_<px>*), or any image, SVG or
                  font file under tools/hudbench/ outside tour/fallback/ (the bench's public part is code only)
  marker          a file holding the private marker (MARKER below: the bench's writers put it in every state, private
                  list and atlas manifest, and in a PNG text chunk), also as UTF-16 / UTF-32, behind JSON \\u escapes,
                  inside compressed PNG text chunks, or in the file's name
  private-copy    a file equal to one in the local private stores (build/bench/, research/experience/bench/), also when
                  only its line ends differ: a renamed atlas or recording, wherever it was put
  guard-lines     the exported .gitattributes lacks one of the three export-ignore lines
Zip, tar and gzip files in the export are opened (two levels, 512 MB each) and their members judged as 'file!member'.
Out of reach by design (a name / marker / hash guard): a renamed atlas with no marker and no store copy, an edited
copy, a recording without the marker, the marker split or encoded otherwise (base64, URL encoding, lookalike hyphens).
Two exports are judged: HEAD (what a release would publish) and the index (`git write-tree`: what the next commit would
publish), so a forbidden file is refused as soon as it is staged. The marker is never written literally in a public file
(this one builds it from two parts), so the check does not refuse itself.

Usage (the repository's Python: research\\tools\\ocr\\.venv\\Scripts\\python.exe -I):
    tools\\private_check.py              judge HEAD and the index; one line per refusal (target, path, rule, why);
                                        exit 0 clean, 1 refused, 2 the check could not run (git failed)
    tools\\private_check.py --head-only  judge HEAD alone
    tools\\private_check.py --selftest   plant one file of each kind in a throwaway repository (the real .gitattributes
                                        copied in) and prove each is refused, and that the private folders pass while
                                        export-ignored; exit 0 when every case holds
tools\\package.ps1 stops on it beside docrefs.py --export; tools\\drift_check.ps1 runs it as `private`.
"""
import hashlib
import io
import os
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MARKER = "flbench" + "-private"
PRIVATE_FOLDERS = ("tools/hudbench/private/", "tools/hudbench/tour/fallback/", "tests/bench/")
STORES = ("build/bench", "research/experience/bench")
IMAGE_EXT = (".png", ".bmp", ".tga", ".dds", ".jpg", ".jpeg", ".webp", ".gif", ".tif", ".tiff")
ASSET_EXT = IMAGE_EXT + (".svg", ".ttf", ".otf", ".ttc", ".fon", ".woff", ".woff2")
RECORDING = re.compile(r"\.(flstate|fldl)(\.|$)", re.I)  # .flstate.json, .fldl.json and any packed or renamed form
GAME_FILE = re.compile(r"\.txm$", re.I)                 # the game's texture files (the icon atlas' source)
ATLAS_NAME = re.compile(r"atlas|^font_.+_\d+|^icons_\d+", re.I)
MIN_COPY_BYTES = 64  # smaller files (empty stubs, one-line configs) are not compared with the stores
NEST_DEPTH = 2       # archives inside the export (zip, tar, gzip) are opened this deep
NEST_BYTES = 512 << 20  # and up to this much unpacked per exported file


def git(root, *args, binary=False):
    r = subprocess.run(["git", "-C", root, *args], capture_output=True)
    if r.returncode:
        raise RuntimeError(f"git {' '.join(args)}: exit {r.returncode}: {r.stderr.decode(errors='replace').strip()}")
    return r.stdout if binary else r.stdout.decode("utf-8", errors="replace").strip()


def export_files(root, treeish):
    """{path: bytes} of what `git archive <treeish>` publishes (files only)."""
    data = git(root, "archive", "--format=tar", treeish, binary=True)
    out = {}
    with tarfile.open(fileobj=io.BytesIO(data)) as tar:
        for m in tar.getmembers():
            if m.isfile():
                out[m.name] = tar.extractfile(m).read()
    return out


def png_texts(data):
    """The decompressed contents of a PNG's zTXt and compressed iTXt chunks (tEXt is plain and seen by the byte scan)."""
    out = []
    if not data.startswith(b"\x89PNG\r\n\x1a\n"):
        return out
    i = 8
    while i + 8 <= len(data):
        n = int.from_bytes(data[i:i + 4], "big")
        kind = data[i + 4:i + 8]
        body = data[i + 8:i + 8 + n]
        i += 12 + n
        try:
            if kind == b"zTXt":
                key, _, rest = body.partition(b"\0")
                out.append(zlib.decompress(rest[1:]))
            elif kind == b"iTXt":
                key, _, rest = body.partition(b"\0")
                if rest[:1] == b"\1":
                    # compression flag, method, language tag \0, translated keyword \0, text
                    parts = rest[2:].split(b"\0", 2)
                    if len(parts) == 3:
                        out.append(zlib.decompress(parts[2]))
        except zlib.error:
            out.append(b"")
        # (no stop at IEND: a chunk appended after it is still in the file)
    return out


FORMS = tuple(MARKER.encode(e) for e in ("ascii", "utf-16-le", "utf-16-be", "utf-32-le", "utf-32-be"))
JSON_ESCAPE = re.compile(rb"\\u([0-9a-fA-F]{4})")


def has_marker(data):
    low = data.lower()
    if any(f in low for f in FORMS):
        return "in its bytes"
    if b"\\u" in data:
        plain = JSON_ESCAPE.sub(lambda m: chr(int(m.group(1), 16)).encode("utf-8", errors="replace"), data).lower()
        if any(f in plain for f in FORMS):
            return "behind \\u escapes"
    if any(any(f in t.lower() for f in FORMS) for t in png_texts(data)):
        return "in a compressed PNG text chunk"
    return ""


def unpack(name, data):
    """[(member name, bytes)] of a zip, tar or gzip file (empty for anything else or a broken one)."""
    out = []
    try:
        if data[:4] == b"PK\x03\x04":
            import zipfile
            with zipfile.ZipFile(io.BytesIO(data)) as z:
                out = [(i.filename, z.read(i)) for i in z.infolist() if not i.is_dir()]
        elif data[:2] == b"\x1f\x8b":
            import gzip
            inner = gzip.decompress(data)
            base = name.rsplit("/", 1)[-1]
            out = [(base[:-3] if base.lower().endswith(".gz") else base + ".unpacked", inner)]
        elif len(data) > 262 and data[257:262] == b"ustar":
            with tarfile.open(fileobj=io.BytesIO(data)) as t:
                out = [(m.name, t.extractfile(m).read()) for m in t.getmembers() if m.isfile()]
    except Exception:
        return []
    total = 0
    kept = []
    for n, d in out:
        total += len(d)
        if total > NEST_BYTES:
            break
        kept.append((n, d))
    return kept


def entries(path, data, depth=0):
    """The exported file and, for an archive, its members (as 'outer!member'), NEST_DEPTH deep."""
    yield path, data
    if depth < NEST_DEPTH:
        for n, d in unpack(path, data):
            yield from entries(f"{path}!{n}", d, depth + 1)


def store_hashes(root, sizes):
    """sha256 -> store path for every file in the local private stores whose size is one of `sizes`."""
    out = {}
    for s in STORES:
        base = os.path.join(root, *s.split("/"))
        for d, dirs, fs in os.walk(base):
            for f in fs:
                p = os.path.join(d, f)
                try:
                    if os.path.getsize(p) not in sizes:
                        continue
                    with open(p, "rb") as fh:
                        raw = fh.read()
                    rel = os.path.relpath(p, root).replace("\\", "/")
                    # as stored, and with CRLF as LF: an export of a text file may differ only in line ends
                    for form in (raw, raw.replace(b"\r\n", b"\n")):
                        out.setdefault(hashlib.sha256(form).hexdigest(), rel)
                except OSError:
                    continue
    return out


def judge(files, root):
    """[(path, rule, why)] for one export's {path: bytes}; an archive's members are judged as 'outer!member'."""
    found = []
    flat = {}
    for path in sorted(files):
        for epath, data in entries(path, files[path]):
            flat[epath] = data
            name = epath.replace("!", "/").rsplit("/", 1)[-1]
            low = epath.lower()
            for f in PRIVATE_FOLDERS:
                if low.startswith(f):
                    found.append((epath, "private-folder", f"under {f}, which the export must leave out (.gitattributes export-ignore)"))
            if RECORDING.search(name):
                found.append((epath, "recording", "a recorded state or display list (.flstate / .fldl, any packed or renamed form) is never published"))
            if GAME_FILE.search(name):
                found.append((epath, "game-file", "a game texture file (.txm) is never published"))
            if low.endswith(IMAGE_EXT) and ATLAS_NAME.search(name):
                found.append((epath, "atlas", "an atlas image by its name (*atlas*, font_<set>_<px>, icons_<px>)"))
            elif low.endswith(ASSET_EXT) and low.startswith("tools/hudbench/") and not low.startswith("tools/hudbench/tour/fallback/"):
                found.append((epath, "atlas", "an image or font under tools/hudbench/: the bench's public part is code only"))
            if any(f.decode() in low for f in FORMS[:1]):
                found.append((epath, "marker", "the private marker in its name"))
            where = has_marker(data)
            if where:
                found.append((epath, "marker", f"holds the private marker {where}"))
    sizes = {len(d) for d in flat.values() if len(d) >= MIN_COPY_BYTES}
    sizes |= {len(d.replace(b"\r\n", b"\n")) for d in flat.values() if len(d) >= MIN_COPY_BYTES}
    hashes = store_hashes(root, sizes) if sizes else {}
    if hashes:
        for path in sorted(flat):
            d = flat[path]
            if len(d) >= MIN_COPY_BYTES:
                for form in (d, d.replace(b"\r\n", b"\n")):
                    h = hashlib.sha256(form).hexdigest()
                    if h in hashes:
                        found.append((path, "private-copy", f"equal to the private store's {hashes[h]}"))
                        break
    ga = files.get(".gitattributes", b"").decode("utf-8", errors="replace")
    lines = {" ".join(l.split()) for l in ga.splitlines()}
    for f in PRIVATE_FOLDERS:
        if f"{f} export-ignore" not in lines:
            found.append((".gitattributes", "guard-lines", f"the line '{f} export-ignore' is missing"))
    return found


def check(root, head_only=False, quiet=False):
    """Judge HEAD and (unless head_only) the index; returns the refusals as (target, path, rule, why)."""
    targets = [("HEAD", "HEAD")]
    if not head_only:
        targets.append(("index", git(root, "write-tree")))
    out = []
    seen = set()
    for label, treeish in targets:
        for path, rule, why in judge(export_files(root, treeish), root):
            # the index repeats what HEAD already holds: name a refusal once, at the first target that has it
            if (path, rule, why) in seen:
                continue
            seen.add((path, rule, why))
            out.append((label, path, rule, why))
            if not quiet:
                print(f"REFUSED {label}: {path}: {rule}: {why}")
    return out


# ---------------------------------------------------------------- selftest
def _png(text_chunk=None):
    """A 1x1 PNG, optionally with one extra chunk (kind, body) before IDAT."""
    def chunk(kind, body):
        return len(body).to_bytes(4, "big") + kind + body + zlib.crc32(kind + body).to_bytes(4, "big")
    ihdr = chunk(b"IHDR", (1).to_bytes(4, "big") + (1).to_bytes(4, "big") + bytes([8, 6, 0, 0, 0]))
    extra = chunk(*text_chunk) if text_chunk else b""
    idat = chunk(b"IDAT", zlib.compress(b"\0\0\0\0\0"))
    return b"\x89PNG\r\n\x1a\n" + ihdr + extra + idat + chunk(b"IEND", b"")


def selftest():
    real_ga = open(os.path.join(ROOT, ".gitattributes"), "rb").read()
    tmp = tempfile.mkdtemp(prefix="flv_private_check_")
    fails = 0
    try:
        def run(*a):
            git(tmp, "-c", "user.name=private_check", "-c", "user.email=dev@flvibed.invalid", *a)

        def write(rel, data):
            p = os.path.join(tmp, *rel.split("/"))
            os.makedirs(os.path.dirname(p), exist_ok=True)
            with open(p, "wb") as fh:
                fh.write(data)

        def case(title, plant, want_rule, want_path, ga=None, head=False):
            """plant {rel: bytes}, stage them (and commit when head), expect want_rule on want_path (None: clean)."""
            nonlocal fails
            if ga is not None:
                write(".gitattributes", ga)
                run("add", "-f", ".gitattributes")
            for rel, data in plant.items():
                write(rel, data)
                run("add", "-f", rel)
            if head:
                run("commit", "-q", "-m", title)
            got = check(tmp, head_only=head, quiet=True)
            hit = [g for g in got if g[1] == want_path and g[2] == want_rule] if want_rule else []
            ok = bool(hit) if want_rule else not got
            fails += not ok
            shown = "; ".join(f"{g[0]} {g[1]} {g[2]}" for g in got) or "nothing refused"
            print(f"{'ok  ' if ok else 'FAIL'} {title}: {shown}")
            # undo: back to the clean base commit, index and tree
            run("reset", "-q", "--hard", "base")
            run("clean", "-qfdx")

        git(tmp, "init", "-q")
        write(".gitattributes", real_ga)
        write("README.md", b"public\n")
        write("tools/hudbench/web/replay.js", b"// public code\n")
        run("add", "-f", ".")
        run("commit", "-q", "-m", "base")
        run("tag", "base")
        stripped = b"\n".join(l for l in real_ga.splitlines() if not any(l.strip() == f"{f} export-ignore".encode() for f in PRIVATE_FOLDERS))
        big = bytes(range(256)) * 4
        case("clean base", {}, None, None)
        case("the three private folders, export-ignored: pass", {
            "tools/hudbench/private/unit.fldl.json": b"{}",
            "tools/hudbench/tour/fallback/font/A.svg": b"<svg/>",
            "tests/bench/calm.flstate.json": b"{}"}, None, None)
        for f in PRIVATE_FOLDERS:
            case(f"{f} without its export-ignore line", {f + "notes.txt": b"x"}, "private-folder", f + "notes.txt", ga=stripped)
        case("the guard lines removed", {}, "guard-lines", ".gitattributes", ga=stripped)
        case("a state outside tests/bench", {"docs/calm.flstate.json": b"{}"}, "recording", "docs/calm.flstate.json")
        case("a display list outside the private folder", {"docs/x/card.FLDL.json": b"{}"}, "recording", "docs/x/card.FLDL.json")
        case("an atlas by name", {"docs/images/font_label_26.png": _png()}, "atlas", "docs/images/font_label_26.png")
        case("an icon atlas by name", {"docs/icons_128.png": _png()}, "atlas", "docs/icons_128.png")
        case("any image under tools/hudbench", {"tools/hudbench/web/pretty.png": _png()}, "atlas", "tools/hudbench/web/pretty.png")
        case("the marker in a tEXt chunk", {"docs/a.png": _png((b"tEXt", b"Comment\0" + MARKER.encode()))}, "marker", "docs/a.png")
        case("the marker in a zTXt chunk", {"docs/b.png": _png((b"zTXt", b"Comment\0\0" + zlib.compress(MARKER.encode())))}, "marker", "docs/b.png")
        case("the marker in a compressed iTXt chunk", {"docs/c.png": _png((b"iTXt", b"Comment\0\1\0\0\0" + zlib.compress(MARKER.encode())))}, "marker", "docs/c.png")
        case("the marker in UTF-16 text", {"docs/notes.txt": ("x " + MARKER.upper()).encode("utf-16")}, "marker", "docs/notes.txt")
        case("the marker in a renamed json", {"docs/data/table.json": ('{"privacy": "%s"}' % MARKER).encode()}, "marker", "docs/data/table.json")
        case("the marker in UTF-32 text", {"docs/n32.txt": MARKER.encode("utf-32")}, "marker", "docs/n32.txt")
        case("the marker behind JSON \\u escapes", {"docs/esc.json": ('{"p": "%s"}' % "".join("\\u%04x" % ord(c) for c in MARKER)).encode()}, "marker", "docs/esc.json")
        case("a zTXt chunk after IEND", {"docs/late.png": _png() + _png((b"zTXt", b"C\0\0" + zlib.compress(MARKER.encode())))[8:]}, "marker", "docs/late.png")
        case("the marker in a file name", {f"docs/{MARKER}.txt": b"x"}, "marker", f"docs/{MARKER}.txt")
        import gzip
        case("the marker inside a gzip", {"docs/notes.gz": gzip.compress(MARKER.encode())}, "marker", "docs/notes.gz!notes")
        zbuf = io.BytesIO()
        import zipfile
        with zipfile.ZipFile(zbuf, "w", zipfile.ZIP_DEFLATED) as z:
            z.writestr("sky/calm.flstate.json", "{}")
            z.writestr("icons_128.png", _png())
        case("a state inside a zip", {"docs/bundle.zip": zbuf.getvalue()}, "recording", "docs/bundle.zip!sky/calm.flstate.json")
        case("an atlas inside a zip", {"docs/bundle.zip": zbuf.getvalue()}, "atlas", "docs/bundle.zip!icons_128.png")
        case("a packed state by name", {"docs/calm.flstate.json.gz": b"x"}, "recording", "docs/calm.flstate.json.gz")
        case("a game texture file", {"docs/flaticons.txm": b"x"}, "game-file", "docs/flaticons.txm")
        case("a font or SVG under tools/hudbench", {"tools/hudbench/web/glyph.svg": b"<svg/>"}, "atlas", "tools/hudbench/web/glyph.svg")
        write("build/bench/cache/font_label_26.png", big)
        case("a renamed copy of a store file", {"docs/images/harmless.bin": big}, "private-copy", "docs/images/harmless.bin")
        text = b"line of a recorded list\n" * 8
        write("research/experience/bench/run1/list.txt", text)
        case("a store copy that differs in line ends", {"docs/copy.txt": text.replace(b"\n", b"\r\n")}, "private-copy", "docs/copy.txt")
        write("research/experience/bench/run1/list.txt", text)
        case("a store copy the export writes with CRLF", {"docs/copy2.txt": text}, "private-copy", "docs/copy2.txt")
        shutil.rmtree(os.path.join(tmp, "build"), ignore_errors=True)
        case("a committed state, judged at HEAD", {"docs/old.flstate.json": b"{}"}, "recording", "docs/old.flstate.json", head=True)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    print(f"selftest: {'all cases hold' if not fails else f'{fails} case(s) FAILED'}")
    return 1 if fails else 0


def main(argv):
    if "--selftest" in argv:
        return selftest()
    try:
        got = check(ROOT, head_only="--head-only" in argv)
    except RuntimeError as e:
        print(f"private_check: could not run: {e}")
        return 2
    target = "HEAD" if "--head-only" in argv else "HEAD and the index"
    print(f"private_check: {'OK, nothing private in the export of ' + target if not got else f'{len(got)} refusal(s): the export of {target} would publish private bench files'}")
    return 1 if got else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
