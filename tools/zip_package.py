"""Writes the player package's zip (tools/package.ps1) with Unix file modes, so the shipped Linux script stays executable.

Compress-Archive (the zip of 0.4.6 and before) stores no Unix mode bits, so `./flvibed-linux.sh` failed with
"Permission denied" after unzipping on Linux or a Steam Deck. Here every entry is marked as made on Unix
(create_system 3) with mode 0644, and every *.sh 0755; the folder itself is the zip's root, as Compress-Archive made
it (FLvibed-<version>/...). Only the standard library: nothing to download.

Usage: python -I tools/zip_package.py <stage folder> <zip path>
       python -I tools/zip_package.py --check <zip path>     prints each *.sh entry's mode; exit 1 if one is not 0755
"""
import os
import sys
import time
import zipfile


def write(stage, out):
    root = os.path.basename(os.path.normpath(stage))
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for folder, dirs, files in os.walk(stage):
            dirs.sort()
            for name in sorted(files):
                full = os.path.join(folder, name)
                arc = root + "/" + os.path.relpath(full, stage).replace(os.sep, "/")
                st = os.stat(full)
                info = zipfile.ZipInfo(arc, time.localtime(st.st_mtime)[:6])
                info.create_system = 3                                   # made on Unix: its mode bits count
                mode = 0o755 if name.endswith(".sh") else 0o644
                info.external_attr = (0o100000 | mode) << 16             # a regular file with that mode
                info.compress_type = zipfile.ZIP_DEFLATED
                with open(full, "rb") as f:
                    z.writestr(info, f.read())


def check(path):
    bad, seen = 0, 0
    with zipfile.ZipFile(path) as z:
        for info in z.infolist():
            seen += info.filename.endswith(".sh")
            if info.filename.endswith(".sh"):
                mode = (info.external_attr >> 16) & 0o777
                ok = info.create_system == 3 and mode == 0o755
                print(f"{info.filename}: made on {'Unix' if info.create_system == 3 else info.create_system}, mode {oct(mode)}"
                      f"{'' if ok else '  NOT EXECUTABLE'}")
                bad += not ok
    if not seen:
        print("no *.sh entry in the zip: the Linux script is missing")
    return 1 if bad or not seen else 0


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "--check":
        sys.exit(check(sys.argv[2]))
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    write(sys.argv[1], sys.argv[2])
    sys.exit(check(sys.argv[2]))
