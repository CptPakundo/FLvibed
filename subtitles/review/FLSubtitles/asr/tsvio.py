"""Defensive file input and crash-safe output for the pipeline and the review tool (stdlib only).

read_tsv: a tab-separated file with a header row -> list of dicts. Never stops on one bad row: undecodable
bytes are replaced (U+FFFD) and reported, a row with more fields than the header is reported with its file and
line number (the extra fields are dropped, as before), a short row is padded with "" (editors trim trailing
tabs: not a problem). Reports go to stderr and to PROBLEMS, so a tool can show them.

atomic_open / atomic_write: write to a temporary file next to the target, flush it to disk, then replace the
target in one step - a crash or a full disk leaves the previous file intact, never half a file. Same bytes as a
plain open(path, "w", encoding="utf-8", newline=...) write.
"""
import contextlib
import os
from contextlib import contextmanager
import sys
import time
from pathlib import Path

PROBLEMS = []  # (file, line or 0, message), in the order found
SHOW_PER_FILE = 5


def _report(path: Path, line: int, msg: str, shown: dict) -> None:
    PROBLEMS.append((str(path), line, msg))
    n = shown.get(path, 0) + 1
    shown[path] = n
    if n <= SHOW_PER_FILE:
        print(f"warning: {path.name}:{line}: {msg}" if line else f"warning: {path.name}: {msg}", file=sys.stderr)


def read_text(path: Path) -> str:
    """UTF-8 text (a byte-order mark is dropped); undecodable bytes become U+FFFD and are reported by line."""
    path = Path(path)
    raw = path.read_bytes()
    try:
        return raw.decode("utf-8-sig")
    except UnicodeDecodeError:
        text = raw.decode("utf-8-sig", errors="replace")
        shown = {}
        for i, l in enumerate(text.splitlines(), 1):
            if "�" in l:
                _report(path, i, "not valid UTF-8 (bytes replaced by �)", shown)
        return text


def read_tsv(path: Path, skip_comments: bool = True) -> list:
    """Rows of a header + rows TSV as dicts (header names as keys). Blank lines are skipped, and lines starting
    with "#" unless skip_comments is False."""
    path = Path(path)
    numbered = [(i, l) for i, l in enumerate(read_text(path).splitlines(), 1)
                if l and not (skip_comments and l.startswith("#"))]
    if not numbered:
        _report(path, 0, "empty (no header row)", {})
        return []
    head = numbered[0][1].split("\t")
    rows, shown = [], {}
    for i, l in numbered[1:]:
        f = l.split("\t")
        if len(f) > len(head):
            _report(path, i, f"{len(f)} fields, the header has {len(head)}: the extra ones are ignored", shown)
        rows.append(dict(zip(head, f + [""] * (len(head) - len(f)))))
    extra = shown.get(path, 0) - SHOW_PER_FILE
    if extra > 0:
        print(f"warning: {path.name}: {extra} more row(s) like that", file=sys.stderr)
    return rows


def require_columns(path: Path, rows: list, cols) -> None:
    """Stops with a clear message when a file lacks a column the code reads (an old or foreign file)."""
    if rows:
        missing = [c for c in cols if c not in rows[0]]
        if missing:
            raise SystemExit(f"error: {path} has no column(s) {', '.join(missing)} - regenerate it (see the "
                             f"review tool's README: docs/REVIEW_PROCESS.md section 1)")


def _replace(tmp: Path, path: Path) -> None:
    # Windows refuses the replace while another process has the target open without sharing (an editor, a
    # virus scanner, git): retry for a few seconds before giving up (the temporary file is then removed)
    for attempt in range(40):
        try:
            os.replace(tmp, path)
            return
        except PermissionError:
            if attempt == 39:
                raise
            time.sleep(0.1)


@contextmanager  # (imported by name: the privacy hook reads "@module.name" as an address)
def atomic_open(path: Path, newline: str = "\n", encoding: str = "utf-8"):
    """with atomic_open(p) as fo: fo.write(...) - the target changes only when the block finishes."""
    path = Path(path)
    tmp = path.with_name(f"{path.name}.{os.getpid()}.tmp")
    try:
        with open(tmp, "w", encoding=encoding, newline=newline) as fo:
            yield fo
            fo.flush()
            os.fsync(fo.fileno())
        _replace(tmp, path)
    except BaseException:
        with contextlib.suppress(OSError):
            tmp.unlink()
        raise


def atomic_write(path: Path, text: str, newline: str = "\n", encoding: str = "utf-8") -> None:
    with atomic_open(path, newline=newline, encoding=encoding) as fo:
        fo.write(text)
