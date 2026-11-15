"""The module boundary check (the module split plan, private; docs\\ARCHITECTURE.md is the public view): a module includes only core and its own files; and the two interfaces' zones.

Reads every kind=file row of docs\\module_map.tsv (file -> owner module), then each listed source's #include lines, and
reports each include that crosses from one module into another. Every source has one owner: a file marked SPLIT or ?, a
source the map does not list, or a file under modules\\<name>\\ owned by another module is a failure of --enforce.

The zones (tools\\hud_zones.tsv; docs\\ARCHITECTURE.md "The two interfaces"): every source of core\\ and modules\\ is HUD 2
only, shared by both interfaces, or classic only. The uses of HUD 2 from outside it are counted (HUD2_USES below) against a
baseline that may only fall; a classic-only file includes nothing of HUD 2.
The counter's known limits: it reads one line at a time and names, not meaning, so it does not see a namespace alias of views or of a hud2
namespace, a reference to a view held under another name (auto& h = views::hud; h.Flight(...)), a function pointer to an entry, a call split
across lines, a new wrapper in core\\ui\\views.h not named Hud* or Skin*, or a change that removes one use and adds another (the total stays).
A reviewer reads --list for those.

Usage (OCR venv python, -I):
    module_lint.py            report; exit 1 on a crossing out of a cut module (one with files under modules\\) or more uses of HUD 2
    module_lint.py --enforce  exit 1 on any crossing, an unowned file, a file outside its owner's folder, a source in no zone, or a count of
                              HUD 2's uses other than the baseline (drift_check runs this)
    module_lint.py --list     every crossing and every use of HUD 2, one per line, not only the counts
    module_lint.py --changed <path> ... | --staged | --range <a>..<b>
                              the classic checks the changed files need (paths, or git's staged index, or a commit range); exit 1 when a
                              changed source is in no zone
    module_lint.py --selftest the zone and counter rules on in-memory sources, and a new use refused on the real tree

The page framework's rules (audit 70b F1b; pages_report below): a view includes only the draw kits and a screen's model, a file name is
unique in its tree, a page calls the game only through its screen's table, and the borrowed uses of the game's windows may only fall.

HUD 2's game-free core (audit 71b B3c; bench_report below): the frame and its parts include no game header, hold no mutable static
beside the parts' binding, and read nothing of the host's live state that the frame hands in (BENCH_CORE); the report and --enforce
refuse each. Its limits: a declaration written as `static Foo x(1);` reads as a function, as does a static whose type holds a
parenthesis (`static decltype(x) g = 0;`), a variable template with nested <> is missed,
and a live read is not seen through a name not listed in BENCH_HIDDEN, a `using namespace` or namespace alias, a call split across lines,
or an allowed header's inline function that reads a global (promptfit::LaneLayout / LaneHold read deckmode; the core calls neither).
The unit build, which links the core with no game source, catches a call into the game; a reviewer reads the rest.
"""
import collections
import csv
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAP = os.path.join(ROOT, "docs", "module_map.tsv")
ZONES = os.path.join(ROOT, "tools", "hud_zones.tsv")
INCLUDE = re.compile(r'\s*#\s*include\s+"([^"]+)"')
UNOWNED = {"SPLIT", "?"}
GUARDED_VIEWS = [("modules/hud/src/hud2_entry.cpp", "(?:hud|skin)"), ("modules/hud/src/extras/extras.cpp", "extras"),
                 ("modules/autopilot/src/autopilot_view.cpp", "autopilot")]
FOLDER = {"Autopilot": "autopilot", "Controller": "controller", "Display": "display", "HUD": "hud", "Movies": "movies", "NPC": "npc",
          "Speedrun": "speedrun", "Subtitles": "subtitles"}

# ---- the zones
SOURCE_EXT = (".cpp", ".h", ".inc")
ZONE_NAMES = ("hud2", "shared", "classic", "door")
# The uses of HUD 2 from outside the hud2 zone and the door, as counted by hud2_uses. The count may only fall: when it falls, lower this number to
# the new count in the same commit, so the next change cannot climb back to it unseen.
HUD2_USES = 54
# The door's own uses (core\ui\hud_door.*, zone "door"): core reaches HUD 2 only through it. Counted apart, by the same rule: it may only fall.
HUD2_DOOR_USES = 16
# The classic checks a change to a HUD 2 file needs as well while HUD 2's entries still run under hud_style 0. Empty since audit R4c-1 (13 Nov 2026):
# the door enters and leaves HUD 2, and the classic HUD calls nothing of it (tests\hud_modules.ps1 -Row classic counts 0).
HUD2_STANDDOWN = []
# The checks the zone table names (its `checks` column): id -> what to run.
CHECKS = {
    "classic_frames": "tests\\classic_frames.ps1: the pinned frame set of the classic HUD (hud_style 0: flight, a dealer walk, a room)",
    "nn_pages": "tests\\nn_pages.ps1: the NeuroNet pages walked, pad and keyboard-and-mouse",
    "pixels": "tests\\pixels.ps1 -Classic and tests\\pixels.ps1 (default), each against the old build (-Dll) by tests\\pixels_compare.py",
    "hud_modules": "tests\\hud_modules.ps1: the HUD module's rows (the classic look with the HUD module off)",
    "window_prompts": "tests\\window_prompts.ps1: a window keeps its pad prompts",
    "ship_buy": "tests\\ship_buy_pad.ps1 -Skins 0: the game's own dealer windows on the pad",
    "dealer_focus": "tests\\dealer_focus.ps1: the equipment dealer's focus walk",
    "fault_giveback": "tests\\fault_giveback.ps1: a fault gives the game back",
    "goal_panel": "tests\\autopilot_modules.ps1: frames the goal panel",
    "page": "tests\\page_modules.ps1 and tests\\page_no_controller.ps1: the options page",
    "sweep_subtitles": "tools\\harness\\flsweep.ps1 -Only display-subtitles: its classic half is pinned to hud_style 0",
    "sweep_modules": "tools\\harness\\flsweep.ps1 -Only modules-matrix: every module set",
    "sweep_rooms": "tools\\harness\\flsweep.ps1 -Only base-rooms: a room reached on the pad, a launch",
}
# What counts as a use of HUD 2 from a file outside the hud2 zone (comments and string literals left out; each match is one use):
#   view     a call through the HUD's or the skin's view: views::hud.<entry>(...), views::skin.<entry>(...), views::Hud<name>(...),
#            views::Skin<name>(...). A null test before the call is not a second use. The data shapes the views declare (HudContext, HudRect,
#            MapChip, PagePrompt, kPagePad*) are not uses: each travels with a call that is counted.
#   bus      a call of HUD 2's half of the HUD bus: ->ReadLayout(...), ->PublishLayout(...) (core\hud_bus.h HudLayout).
#   include  an #include of a hud2-zone file (one per line: the calls behind it are not counted again).
#   name     a qualified name in a namespace that only hud2-zone files of core declare (indicators::, roletext::...): core's headers reach
#            every module without an include (build.cmd's core_all.h), so the name is the use.
#   style    a read of the interface choice, hud_style or ui_style: .hudStyle / .uiStyle not written to (their rows in core\ui\hud_rows.h aside).
VIEW_TYPES = {"HudContext", "HudRect", "HudView", "SkinView"}
#   using    a using-declaration of a HUD or skin view name, `using namespace views`, or `using namespace` of a hud2 core namespace.
# #define bodies are scanned like code, so a macro that wraps a use counts once where it is defined.
USE_PATTERNS = [
    ("view", re.compile(r"\bviews\s*::\s*(?:hud|skin)\s*\.\s*(\w+)\s*\(")),
    ("view", re.compile(r"\bviews\s*::\s*((?:Hud|Skin)\w*)\s*\(")),
    ("bus", re.compile(r"(?:->|\.)\s*((?:Read|Publish)Layout)\s*\(")),
    ("style", re.compile(r"(?:->|\.)\s*(hudStyle|uiStyle)\b(?!\s*=(?!=))")),
    ("using", re.compile(r"\busing\s+views\s*::\s*((?:Hud|Skin)\w*|hud\b|skin\b)")),
    ("using", re.compile(r"\busing\s+namespace\s+(views)\b")),
]
STYLE_HOMES = {"core/ui/hud_rows.h"}
CLASSIC_NEVER = {"core/ui/hud2_kit.h"}  # HUD 2's look: a classic-only file includes neither it nor a hud2-zone file
NAMESPACE = re.compile(r"^\s*namespace\s+(\w+)", re.M)


def norm(path):
    return path.replace("\\", "/").strip()


def read_map():
    """{repository path: owner} from the kind=file rows."""
    with open(MAP, encoding="utf-8", newline="") as f:
        rows = list(csv.DictReader(f, delimiter="\t"))
    return {norm(r["where"] or r["name"]): r["owner"].strip() for r in rows if r["kind"] == "file"}


def resolve(source, target, owners):
    """The repository path an include names, or None for core and system headers."""
    here = os.path.dirname(source)
    candidate = norm(os.path.normpath(os.path.join(here, target)))
    if candidate in owners:
        return candidate
    base = os.path.basename(target)
    same_folder = norm(os.path.join(here, base))
    if same_folder in owners:
        return same_folder
    matches = [p for p in owners if os.path.basename(p) == base]
    # a bare name shared by two modules (api.h, shm.h) stays inside the includer's own tree
    tree = source.split("/")[0]
    matches = [p for p in matches if p.startswith(tree + "/")] or matches
    return matches[0] if len(matches) == 1 else None


def stale_lines():
    """Rows whose `where` names a line past the end of its file: a move left the map behind (P2's audit found 23)."""
    with open(MAP, encoding="utf-8", newline="") as f:
        rows = list(csv.DictReader(f, delimiter="\t"))
    lengths = {}
    stale = []
    for row in rows:
        match = re.match(r"(.+):(\d+)$", norm(row["where"] or ""))
        if not match:
            continue
        source, number = match.group(1), int(match.group(2))
        if source not in lengths:
            path = os.path.join(ROOT, source)
            if not os.path.exists(path):
                stale.append(f"{row['kind']} {row['name']}: {source} does not exist")
                lengths[source] = None
                continue
            with open(path, encoding="utf-8", errors="replace") as f:
                lengths[source] = sum(1 for _ in f)
        if lengths[source] is not None and number > lengths[source]:
            stale.append(f"{row['kind']} {row['name']}: {source}:{number} is past the end ({lengths[source]} lines)")
    return stale


def unguarded():
    """A fault stops one module only if every way into it is guarded (core\\game\\modslots.h Guard): an entry published bare in a view stops the whole entry."""
    found = []
    for path, view in GUARDED_VIEWS:
        text = open(os.path.join(ROOT, path), encoding="utf-8", errors="replace").read()
        entries = re.findall(r"\bv\.\w+ = ([^,;]+)", text)
        for body in re.findall(r"views::" + view + r"\s*=\s*\{([^}]+)\}", text):
            entries += [e.strip() for e in re.split(r",(?![^(]*\))", body)]
        if not entries:
            found.append(f"{path}: no entries read: this check no longer sees the view")
        found += [f"{path}: {e.strip()}" for e in entries if not e.strip().startswith("G(")]
    return found


# ---- the zones and the uses of HUD 2


def read_zones(path=ZONES):
    """The zone table's rows: [{path, zone, checks: [ids], why}], comment lines left out."""
    with open(path, encoding="utf-8", newline="") as f:
        lines = [line for line in f if line.strip() and not line.startswith("#")]
    rows = []
    for r in csv.DictReader(lines, delimiter="\t"):
        checks = [c for c in (r["checks"] or "").split() if c != "-"]
        rows.append({"path": norm(r["path"]), "zone": r["zone"].strip(), "checks": checks, "why": (r["why"] or "").strip()})
    return rows


def zone_row(path, rows):
    """The row whose path is the longest prefix of `path`, or None."""
    best = None
    for row in rows:
        if path.startswith(row["path"]) and (best is None or len(row["path"]) > len(best["path"])):
            best = row
    return best


def zone_problems(rows, sources):
    """What --enforce refuses in the table itself: an unknown zone or check, a row that matches no source, a source in no row."""
    found = []
    for row in rows:
        if row["zone"] not in ZONE_NAMES:
            found.append(f"{row['path']}: zone '{row['zone']}' is none of {', '.join(ZONE_NAMES)}")
        found += [f"{row['path']}: check '{c}' is not in CHECKS" for c in row["checks"] if c not in CHECKS]
        if not any(s.startswith(row["path"]) for s in sources):
            found.append(f"{row['path']}: the row matches no source (a moved or deleted file?)")
    found += [f"{s}: in no zone (add a row to tools\\hud_zones.tsv)" for s in sources if zone_row(s, rows) is None]
    return found


def strip_code(text):
    """The text with comments and string and character literals blanked, newlines kept (so line numbers hold)."""
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == "/" and text.startswith("//", i):
            j = text.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
        elif c == "/" and text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append(re.sub(r"[^\n]", " ", text[i:j]))
            i = j
        elif c == "R" and text.startswith('R"', i) and (i == 0 or not (text[i - 1].isalnum() or text[i - 1] == "_") or text[i - 1] in "LuU8"):
            m = re.match(r'R"([^(\s]{0,16})\(', text[i:])
            if not m:
                out.append(c)
                i += 1
                continue
            end = text.find(")" + m.group(1) + '"', i + m.end())
            j = n if end < 0 else end + len(m.group(1)) + 2
            out.append(re.sub(r"[^\n]", " ", text[i:j]))
            i = j
        elif c in "\"'":
            j = i + 1
            while j < n and text[j] != c and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            j = min(j + 1, n)
            out.append(re.sub(r"[^\n]", " ", text[i:j]))
            i = j
        else:
            out.append(c)
            i += 1
    return "".join(out)


def resolve_source(source, target, sources):
    """The source an #include names: beside the includer, under core\\ (the include path), or the one file of that name in its tree."""
    here = os.path.dirname(source)
    for candidate in (norm(os.path.normpath(os.path.join(here, target))), "core/" + norm(target)):
        if candidate in sources:
            return candidate
    base = os.path.basename(target)
    matches = [p for p in sources if os.path.basename(p) == base]
    tree = source.split("/")[0] if source.startswith("core/") else "/".join(source.split("/")[:2])
    matches = [p for p in matches if p.startswith(tree + "/")] or matches
    return matches[0] if len(matches) == 1 else None


def hud2_names(texts, rows):
    """The namespaces only hud2-zone files of core declare."""
    zone = {p: (zone_row(p, rows) or {}).get("zone") for p in texts}
    inside, outside = set(), set()
    for path, text in texts.items():
        names = set(NAMESPACE.findall(strip_code(text)))
        (inside if zone[path] == "hud2" and path.startswith("core/") else outside).update(names)
    return sorted(inside - outside)


def hud2_uses(texts, rows, door=False):
    """Every use of HUD 2 from a file outside the hud2 zone and the door, as (path, line, kind, what), from {path: text}; with door, the door's
    own uses instead."""
    zone = {p: (zone_row(p, rows) or {}).get("zone") for p in texts}
    names = hud2_names(texts, rows)
    patterns = list(USE_PATTERNS)
    if names:
        patterns.append(("name", re.compile(r"(?<![\w:])(" + "|".join(names) + r")\s*::")))
        patterns.append(("using", re.compile(r"\busing\s+namespace\s+(" + "|".join(names) + r")\b")))
    sources = set(texts)
    uses = []
    for path in sorted(texts):
        if zone[path] == "hud2" or (zone[path] == "door") != door:
            continue
        text = texts[path]
        for number, line in enumerate(text.split("\n"), 1):
            match = INCLUDE.match(line)
            if match:
                target = resolve_source(path, match.group(1), sources)
                if target and zone.get(target) == "hud2":
                    uses.append((path, number, "include", target))
        for number, line in enumerate(strip_code(text).split("\n"), 1):  # (#include paths are strings: blanked, so only #define bodies match)
            for kind, pattern in patterns:
                for m in pattern.finditer(line):
                    if kind == "view" and m.group(1) in VIEW_TYPES:
                        continue
                    if kind == "style" and path in STYLE_HOMES:
                        continue
                    uses.append((path, number, kind, m.group(1)))
    return uses


def classic_includes(texts, rows):
    """A classic-only file's includes of HUD 2: a hud2-zone file or HUD 2's draw kit."""
    zone = {p: (zone_row(p, rows) or {}).get("zone") for p in texts}
    sources = set(texts)
    found = []
    for path in sorted(p for p in texts if zone[p] == "classic"):
        for number, line in enumerate(texts[path].split("\n"), 1):
            match = INCLUDE.match(line)
            target = resolve_source(path, match.group(1), sources) if match else None
            if target and (zone.get(target) == "hud2" or target in CLASSIC_NEVER):
                found.append(f"{path}:{number} includes {target}")
    return found


# ---- HUD 2's game-free core (audit 71b B3c; modules\hud\src\hud2_frame.h): the frame and its parts as functions of what they are given, so a
# program with no game (the design bench, the unit build) runs them. Three rules keep that true:
#   includes  a core file includes only core files and the game-free headers below; those headers include only each other and the core (so the
#             rule holds through every include, not only the first); no device or input header (<d3d8.h>, <dinput.h>, <xinput.h>) anywhere in it.
#   statics   no mutable static: no file-scope or namespace-scope variable that is not const / constexpr, no function-local or class static that
#             is not, no inline or extern variable that is not. Its state is the caller's (FrameState, PartsState). The one exception is the
#             parts' binding (BENCH_BOUND: pointers to the caller's host, settings and state, set by BindParts), named here; the list may only shrink.
#   hidden    no read of the host's live state that the frame hands in as a copy (FrameHost, PartsIn): the settings (uiopt::Get, hudrows::g), the
#             glyph family and look and confirm_button (glyphs::Current / LookFor / FaceColours / Menu, the kit's Glyph forms without a GlyphStyle),
#             hud_size's globals (deckmode::g_on and its margins, hudsize::g_*, textfit::g_floorH / MinSize, promptfit::Hud2Scale), the clocks.
# A declaration the statics rule cannot tell from a function (`static Foo x(1);`) is read as a function: write such a state as `= Foo(1)`.
BENCH_CORE = ["modules/hud/src/" + f for f in (
    "hud2_frame.h", "hud2_frame.cpp", "hud2_parts.cpp", "hud2_target.h", "hud2_target.cpp", "hud2_cue.h", "hud2_cue.cpp",
    "hud2_click_draw.cpp", "hud2_impl.h", "hud2_layout.h", "hud2_bars.h", "hud2_notice.h", "hud2_preset.h", "hud2_state_io.h")]
BENCH_HOST_FREE = {"core/common.h", "core/hud_bus.h", "core/settings.h", "core/log.h", "core/str.h", "core/ui_meta.h", "core/ui/ease.h",
                   "core/ui/hud2_kit.h", "core/ui/glyphs.h", "core/ui/presets.h", "core/ui/hud_context.h", "core/ui/hud_rows.h", "core/ui/uiopt.h",
                   "core/game/flicons.h", "core/game/promptfit.h", "core/game/hudsize.h", "core/game/deckmode.h", "core/game/textfit.h",
                   "core/game/hudwake.h", "core/game/promptshow.h", "core/game/render.h", "core/game/fontraster.h", "modules/hud/src/hud2.h"}
BENCH_BOUND = {("modules/hud/src/hud2_parts.cpp", n) for n in ("g_host", "g_in", "g_noState", "g_state")}
BENCH_ANGLE = re.compile(r'\s*#\s*include\s*<\s*(d3d\w*|dinput\w*|xinput\w*|ddraw)\.h\s*>', re.I)
BENCH_HIDDEN = re.compile(r"\b(uiopt\s*::\s*Get\s*\(|hudrows\s*::\s*g\b|glyphs\s*::\s*(?:Current|LookFor|FaceColours|Menu)\s*\("
                          r"|deckmode\s*::\s*(?:g_on|FlightSide|FlightTopBottom|SideW|EdgeH|LaneTopH|LaneBottomH)\b|hudsize\s*::\s*g_\w+"
                          r"|textfit\s*::\s*(?:g_floorH|MinSize)\b|promptfit\s*::\s*Hud2Scale\b|GetTickCount(?:64)?\b|timeGetTime\b"
                          r"|QueryPerformanceCounter\b|perf\s*::\s*Now\b|Glyph(?:Width)?\s*\(\s*(?!PartsGlyphs\s*\(|style\b))")
DECL_SKIP = re.compile(r"^(using|typedef|template|friend|return|static_assert|namespace|case|default|goto|break|continue|if|for|while|do|switch|"
                       r"else|delete|throw|try|catch|public|private|protected)\b")


def bench_includes(texts):
    """The core's includes outside the core and the game-free headers, and the game-free headers' own."""
    sources = set(texts)
    allowed = set(BENCH_CORE) | BENCH_HOST_FREE
    found = []
    for path in sorted(p for p in allowed if p in texts):
        for number, line in enumerate(texts[path].split("\n"), 1):
            if BENCH_ANGLE.match(line):
                found.append(f"{path}:{number} includes a device or input header: {line.strip()}")
            match = INCLUDE.match(line)
            if not match:
                continue
            target = resolve_source(path, match.group(1), sources)
            if target not in allowed:
                found.append(f"{path}:{number} includes {target or match.group(1)}, not game-free (BENCH_HOST_FREE)")
    return found


def _top_level(s, ch):
    """The index of `ch` outside (), [], {} and <> in s, or -1."""
    depth = 0
    for i, c in enumerate(s):
        if c in "([{<":
            depth += 1
        elif c in ")]}>" and depth:
            depth -= 1
        elif c == ch and depth == 0 and not (ch == "=" and (s[i + 1:i + 2] == "=" or s[i - 1:i] in "!<>=")):
            return i
    return -1


def declared_variable(stmt):
    """(name, mutable) when a statement declares a variable, else None. `stmt` is stripped code; a struct body is '{...}'."""
    s = " ".join(stmt.split())
    s = re.sub(r"^template\s*<[^;{]*?>\s*", "", s)  # a variable template is a variable (its <> unnested: a nested one is missed)
    # labels before the declaration (public: static int s;  case 1: static int s;): the declaration after them is read
    s = re.sub(r"^(?:(?:public|private|protected|default)\s*:(?!:)\s*|case\b[^;]*?[^:]:(?!:)\s*)+", "", s)
    if not s or DECL_SKIP.match(s) or "operator" in s:
        return None
    body = s.find("{...}")
    if body >= 0:  # (static) struct X { ... } name;
        after = s[body + 5:].strip()
        name = re.match(r"\*?\s*(\w+)", after)
        return (name.group(1), True) if name else None
    eq = _top_level(s, "=")
    left = (s[:eq] if eq >= 0 else s).strip()
    if re.search(r"\(\s*[*&]", left):  # a function pointer: void (*fn)() = ...
        name = re.search(r"\(\s*[*&]\s*(?:const\s+)?(\w+)", left).group(1)
        const = re.search(r"\(\s*[*&]\s*const\b", left) is not None or "constexpr" in left.split()
        return name, not const
    if _top_level(left, "(") >= 0 or "(" in left:
        return None  # a function (or `Foo x(1)`: see the rule's note)
    name = re.search(r"(\w+)\s*(?:\[[^\]]*\]\s*)*$", left)
    words = re.findall(r"\w+|[*&]", left)
    if not name or len(words) < 2:
        return None if eq < 0 else (name.group(1), True) if name else None
    if "constexpr" in words:
        return name.group(1), False
    last_ptr = max((i for i, w in enumerate(words) if w in "*&"), default=-1)
    const = "const" in words[last_ptr + 1:] if last_ptr >= 0 else "const" in words
    if last_ptr >= 0 and words[last_ptr] == "&" and "const" in words[:last_ptr]:
        const = True  # a const reference
    return name.group(1), not const


def mutable_statics(text):
    """[(line, name)]: the mutable statics of a source (see the rules above)."""
    code = strip_code(text)
    code = re.sub(r"(?m)^[ \t]*#.*(?:\\\n.*)*$", lambda m: re.sub(r"[^\n]", " ", m.group(0)), code)  # preprocessor lines
    found = []
    stack = []  # "ns", "block", "init" (an initializer's braces: a lambda's body is checked as a block), or ("type", its header, its start)
    pending, start, tail = "", 0, False  # tail: after an initializer's braces, up to its ';' (the declaration was checked at its '{')

    def check(stmt, at):
        kinds = [k if isinstance(k, str) else "type" for k in stack]
        scope = "ns" if all(k == "ns" for k in kinds) else kinds[-1]
        if scope != "ns" and not re.search(r"\b(static|thread_local)\b", stmt):
            return  # (a member or a local without `static` is its owner's, not a static)
        got = declared_variable(stmt)
        if got and got[1]:
            found.append((code.count("\n", 0, at + len(stmt) - len(stmt.lstrip())) + 1, got[0]))

    def in_decltype(s):
        """Whether the end of `s` lies inside a decltype's own parentheses (an earlier, closed decltype does not count)."""
        at = s.rfind("decltype(")
        while at >= 0:
            depth = 0
            for ch in s[at + 8:]:
                depth += (ch == "(") - (ch == ")")
                if depth == 0:
                    break
            if depth > 0:
                return True
            at = s.rfind("decltype(", 0, at)
        return False

    for i, c in enumerate(code):
        if c == "{" and "decltype(" in pending and in_decltype(pending):
            stack.append("expr")  # a braced expression inside decltype(...): part of the statement (struct P<decltype(T{})> : ...)
            pending += c
        elif c == "}" and stack and stack[-1] == "expr":
            stack.pop()
            pending += c
        elif c == "{":
            if _top_level(pending, "=") >= 0 and not tail:
                check(pending, start)
                stack.append("init")
            elif re.search(r"\bnamespace\b[^()]*$|\bextern\s*$", pending):
                stack.append("ns")
            elif re.search(r"\b(struct|class|union|enum)\b", pending) and "(" not in pending:
                stack.append(("type", pending, start))
            elif not tail and declared_variable(pending):  # a brace initialiser: static int g_x{0};
                check(pending, start)
                stack.append("init")
            else:
                stack.append("block")
            pending, start = "", i + 1
        elif c == "}":
            kind = stack.pop() if stack else "block"
            if isinstance(kind, tuple):  # a type's body: a variable may follow it (static struct {...} g_x;)
                pending, start = kind[1] + " {...} ", kind[2]
            else:
                tail = tail or (kind == "init" and "init" not in stack)
                pending, start = "", i + 1
        elif c == ";":
            if not tail:
                check(pending, start)
            pending, start, tail = "", i + 1, False
        else:
            pending += c
    return found


def bench_statics(texts):
    """[(path, line, name)]: the core's mutable statics, the binding's included."""
    return [(p, n, name) for p in BENCH_CORE if p in texts for n, name in mutable_statics(texts[p])]


def bench_hidden(texts):
    """The core's reads of the host's live state."""
    found = []
    for path in BENCH_CORE:
        if path in texts:
            for number, line in enumerate(strip_code(texts[path]).split("\n"), 1):
                found += [f"{path}:{number} {m.group(1).strip()}" for m in BENCH_HIDDEN.finditer(line)]
    return found


def bench_report(listing, overlay=None):
    """The game-free core's lines of the report; returns (enforce failures, report failures)."""
    texts = read_texts(tracked_sources(), overlay)
    missing = [p for p in BENCH_CORE if p not in texts] + sorted(p for p in BENCH_HOST_FREE if p not in texts)
    includes = bench_includes(texts)
    statics = bench_statics(texts)
    unbound = [s for s in statics if (s[0], s[2]) not in BENCH_BOUND]
    gone = sorted(b for b in BENCH_BOUND if not any((s[0], s[2]) == b for s in statics))
    hidden = bench_hidden(texts)
    for path in missing:
        print(f"  BENCH CORE FILE MISSING: {path} (a moved file: update BENCH_CORE / BENCH_HOST_FREE in tools\\module_lint.py)")
    for line in includes:
        print("  BENCH CORE INCLUDES THE GAME: " + line)
    for path, number, name in unbound:
        print(f"  BENCH CORE HOLDS A MUTABLE STATIC: {path}:{number} {name} (its state belongs in FrameState / PartsState)")
    for path, name in gone:
        print(f"  BENCH CORE'S BINDING SHRANK: {path} {name} is gone: take it out of BENCH_BOUND in tools\\module_lint.py in this commit")
    for line in hidden:
        print("  BENCH CORE READS THE HOST'S LIVE STATE: " + line + " (hand it in: hud2_frame.h FrameHost, PartsIn)")
    if listing:
        for path, number, name in statics:
            print(f"  bound {path}:{number} {name}")
    print(f"module_lint: bench core: {len(BENCH_CORE)} files, {len(includes)} includes outside the game-free set, {len(unbound)} mutable statics "
          f"(the binding's {len(statics) - len(unbound)} of {len(BENCH_BOUND)} aside), {len(hidden)} live reads of the host")
    bad = bool(missing or includes or unbound or hidden)
    return bad or bool(gone), bad


# ---- the page framework's rules (audit 70b F1b; docs\ARCHITECTURE.md "The page framework")
# A page is files: its view in modules\hud\src\pages\<family>\, its model, rules and actions in core\game\screens\, registered by
# pagelist.h. Three rules keep that true while the dealers' and NeuroNet's pages move out of basepage.cpp and nnpage.cpp:
#   views     a view (pages\...\<name>_view.cpp) includes only the pages' draw kit (page_view.h, page_kit.h), HUD 2's draw kit
#             (ui\hud2_kit.h) and a screen's model (core\game\screens\<window>_model.h): it draws a model, it reads nothing of the game.
#   names     a file name is unique in its tree (core\, each module): the build names each wrapper by the file's stem
#             (build.cmd :wrapmodule), and an include resolves by its name, so two pages\<family>\list_view.cpp would collide.
#   borrowed  the calls that work the game's own window controls from a page (a click or scroll of its control, a control thrown
#             off screen, a screen message), in the HUD module and the screens: the count may only fall (PAGES_BORROWED), as the
#             pages' actions move from the game's buttons to the model's own calls.
#   actions   pages\ calls no game function itself (no exe:: symbol, hk::Call, uiclick click or scroll, room::SendScreenMsg): its
#             actions go through a screen's table and pagerun's runner (core\game\screens\action.h). The runtime is the exception:
#             it parks the game's window by the window's own setter (PAGES_RUNTIME).
PAGES_DIR = "modules/hud/src/pages/"
SCREENS_DIR = "core/game/screens/"
PAGES_RUNTIME = ("modules/hud/src/pages/pagerun.",)
VIEW_KIT = {"modules/hud/src/pages/page_view.h", "modules/hud/src/pages/page_kit.h", "core/ui/hud2_kit.h"}
PAGES_BORROWED = 40
BORROW_PATTERNS = [re.compile(r"\buiclick\s*::\s*(Click|Scroll)\s*\("), re.compile(r"\b(PlaceAt)\s*\("),
                   re.compile(r"\broom\s*::\s*(SendScreenMsg)\s*\(")]
PAGES_FORBIDDEN = re.compile(r"\b(exe\s*::|hk\s*::\s*Call\b|uiclick\s*::\s*(?:Click|Scroll)\b|room\s*::\s*SendScreenMsg\b)")


def view_includes(texts):
    """A view's includes outside the draw kits and the screens' models."""
    sources = set(texts)
    found = []
    for path in sorted(p for p in texts if p.startswith(PAGES_DIR) and p.endswith("_view.cpp")):
        for number, line in enumerate(texts[path].split("\n"), 1):
            match = INCLUDE.match(line)
            if not match:
                continue
            target = resolve_source(path, match.group(1), sources)
            if not (target in VIEW_KIT or (target or "").startswith(SCREENS_DIR) and target.endswith("_model.h")):
                found.append(f"{path}:{number} includes {target or match.group(1)}")
    return found


def duplicate_names(sources):
    """File names used twice in one tree (core\\, or one module)."""
    seen = collections.defaultdict(list)
    for s in sources:
        parts = s.split("/")
        tree = "core" if parts[0] == "core" else "/".join(parts[:2])
        seen[(tree, parts[-1])].append(s)
    return [f"{name} in {tree}: " + ", ".join(sorted(paths)) for (tree, name), paths in sorted(seen.items()) if len(paths) > 1]


def borrowed_uses(texts):
    """[(path, line, what)]: the calls working the game's window controls, in the HUD module and the screens."""
    uses = []
    for path in sorted(p for p in texts if p.startswith(("modules/hud/", SCREENS_DIR))):
        for number, line in enumerate(strip_code(texts[path]).split("\n"), 1):
            for pattern in BORROW_PATTERNS:
                uses += [(path, number, m.group(1)) for m in pattern.finditer(line)]
    return uses


def pages_calls(texts):
    """A page file's own calls into the game (the runtime aside)."""
    found = []
    for path in sorted(p for p in texts if p.startswith(PAGES_DIR) and not p.startswith(PAGES_RUNTIME)):
        for number, line in enumerate(strip_code(texts[path]).split("\n"), 1):
            found += [f"{path}:{number} {m.group(1).replace(' ', '')}" for m in PAGES_FORBIDDEN.finditer(line)]
    return found


def pages_report(listing, overlay=None):
    """The page framework's lines of the report; returns (enforce failures, report failures)."""
    texts = read_texts(tracked_sources(), overlay)
    views = view_includes(texts)
    names = duplicate_names(list(texts))
    calls = pages_calls(texts)
    borrowed = borrowed_uses(texts)
    for line in views:
        print("  VIEW INCLUDES MORE THAN THE KIT AND ITS MODEL: " + line)
    for line in names:
        print("  NAME USED TWICE IN ONE TREE: " + line)
    for line in calls:
        print("  A PAGE CALLS THE GAME ITSELF (an action belongs in its screen's table): " + line)
    if listing:
        for path, number, what in borrowed:
            print(f"  borrowed {path}:{number} {what}")
    rose, fell = len(borrowed) > PAGES_BORROWED, len(borrowed) < PAGES_BORROWED
    if rose:
        print(f"  PAGES' BORROWED USES ROSE: {len(borrowed)}, the baseline is {PAGES_BORROWED}: a new call that works the game's window "
              "(--list names each one); give the page the model's own action instead")
    if fell:
        print(f"  PAGES' BORROWED USES FELL: {len(borrowed)}, the baseline is {PAGES_BORROWED}: set PAGES_BORROWED = {len(borrowed)} in "
              "tools\\module_lint.py in this commit")
    print(f"module_lint: pages: {len(views)} view includes outside the kit, {len(names)} names used twice, {len(calls)} game calls from "
          f"pages, {len(borrowed)} borrowed uses of the game's windows (baseline {PAGES_BORROWED})")
    bad = bool(views or names or calls or rose)
    return bad or fell, bad


def ratchet(count, baseline, name="HUD2_USES", what="HUD 2 USES"):
    """(ok under --enforce, ok in a report, the message): the count may only fall, and a fall lowers the baseline."""
    if count > baseline:
        return False, False, (f"{what} ROSE: {count}, the baseline is {baseline}: a new use of HUD 2 (--list names each one). "
                              "Reach HUD 2 from its own zone, or move the code into it")
    if count < baseline:
        return False, True, f"{what} FELL: {count}, the baseline is {baseline}: set {name} = {count} in tools\\module_lint.py in this commit"
    return True, True, ""


def tracked_sources():
    """Core's and the modules' sources, tracked or new (not ignored)."""
    git = ["git", "-C", ROOT, "ls-files", "--cached", "--others", "--exclude-standard", "core", "modules"]
    files = subprocess.run(git, capture_output=True, text=True).stdout.split("\n")
    return sorted({f for f in files if f.endswith(SOURCE_EXT) and os.path.exists(os.path.join(ROOT, f))})


def read_texts(sources, overlay=None):
    texts = {}
    for source in sources:
        with open(os.path.join(ROOT, source), encoding="utf-8", errors="replace") as f:
            texts[source] = f.read()
    texts.update(overlay or {})
    return texts


def zone_report(listing, overlay=None):
    """The zones' lines of the report; returns (enforce failures, report failures)."""
    rows = read_zones()
    texts = read_texts(tracked_sources(), overlay)
    counts = collections.Counter((zone_row(s, rows) or {}).get("zone") or "none" for s in texts)
    problems = zone_problems(rows, list(texts))
    for line in problems:
        print("  ZONE: " + line)
    print(f"module_lint: zones: {len(texts)} sources, hud2 {counts['hud2']}, shared {counts['shared']}, classic {counts['classic']}, "
          f"door {counts['door']}, {len(problems)} problems (tools\\hud_zones.tsv)")
    bad_classic = classic_includes(texts, rows)
    for line in bad_classic:
        print("  CLASSIC INCLUDES HUD 2: " + line)
    uses = hud2_uses(texts, rows)
    kinds = collections.Counter(u[2] for u in uses)
    if listing:
        for path, number, kind, what in uses:
            print(f"  use {path}:{number} {kind} {what}")
    ok_enforce, ok_report, message = ratchet(len(uses), HUD2_USES)
    if message:
        print("  " + message)
    print(f"module_lint: {len(uses)} uses of HUD 2 from outside it (baseline {HUD2_USES}): "
          + ", ".join(f"{k} {kinds[k]}" for k in ("view", "bus", "include", "name", "style", "using")))
    core_outside = [u for u in uses if u[0].startswith("core/")]
    for path, number, kind, what in core_outside:
        print(f"  CORE REACHES HUD 2 OUTSIDE THE DOOR: {path}:{number} {kind} {what} (call it through core\\ui\\hud_door.h)")
    door = hud2_uses(texts, rows, door=True)
    if listing:
        for path, number, kind, what in door:
            print(f"  door {path}:{number} {kind} {what}")
    door_enforce, door_report, door_message = ratchet(len(door), HUD2_DOOR_USES, "HUD2_DOOR_USES", "THE DOOR'S USES")
    if door_message:
        print("  " + door_message)
    print(f"module_lint: {len(door)} uses of HUD 2 by the door (baseline {HUD2_DOOR_USES})")
    return (bool(problems or bad_classic or core_outside or not ok_enforce or not door_enforce),
            bool(bad_classic or core_outside or not ok_report or not door_report))


# ---- the classic checks of a change


def changed_files(argv):
    """The changed paths: given after --changed, or git's staged index (--staged), or a commit range (--range a..b)."""
    if "--staged" in argv:
        out = subprocess.run(["git", "-C", ROOT, "diff", "--cached", "--name-only"], capture_output=True, text=True)
    elif "--range" in argv:
        at = argv.index("--range")
        if at + 1 >= len(argv):
            raise SystemExit("module_lint: --range needs a commit range (a..b)")
        out = subprocess.run(["git", "-C", ROOT, "diff", "--name-only", argv[at + 1]], capture_output=True, text=True)
        if out.returncode:
            raise SystemExit("module_lint: git diff " + argv[at + 1] + ": " + out.stderr.strip())
    else:
        at = argv.index("--changed")
        paths = []
        for arg in argv[at + 1:]:
            if arg.startswith("--"):
                break
            full = os.path.abspath(arg) if os.path.isabs(arg) or os.path.exists(arg) else os.path.join(ROOT, arg)
            rel = os.path.relpath(full, ROOT)
            paths.append(norm(rel if not rel.startswith("..") else arg))
        return paths
    return [norm(p) for p in out.stdout.split("\n") if p.strip()]


def checks_for(paths, rows):
    """[(path, zone or None, [check ids])] and the union in CHECKS order; a path outside core\\ and modules\\ or not a source has none."""
    per = []
    union = set()
    for path in paths:
        if not path.startswith(("core/", "modules/")) or not path.endswith(SOURCE_EXT):
            per.append((path, "", []))
            continue
        row = zone_row(path, rows)
        if row is None:
            per.append((path, None, []))
            continue
        checks = list(row["checks"])
        if row["zone"] == "hud2":
            checks += [c for c in HUD2_STANDDOWN if c not in checks]
        per.append((path, row["zone"], checks))
        union.update(checks)
    return per, [c for c in CHECKS if c in union]


def changed_report(argv):
    rows = read_zones()
    per, union = checks_for(changed_files(argv), rows)
    unzoned = [p for p, zone, _ in per if zone is None]
    sources = [x for x in per if x[1]]
    print(f"module_lint: {len(per)} changed files, {len(sources)} sources in a zone")
    for path, zone, checks in per:
        if zone is None:
            print(f"  {path}: IN NO ZONE (add a row to tools\\hud_zones.tsv)")
        elif zone:
            note = " (HUD 2's entries still run under hud_style 0)" if zone == "hud2" and checks else ""
            print(f"  {path}: {zone}: {' '.join(checks) or 'no classic check'}{note}")
    for c in sorted({c for _, _, checks in per for c in checks if c not in CHECKS}):
        print(f"  WARNING: check '{c}' of tools\\hud_zones.tsv is not in CHECKS: it is not run (fix the row; --enforce refuses it)")
    if union:
        print("module_lint: run these classic checks:")
        for c in union:
            print(f"  {c:15} {CHECKS[c]}")
    else:
        print("module_lint: no classic check")
    return 1 if unzoned else 0


# ---- the self-test


def selftest():
    """The rules on in-memory sources, then the real tree with one new use added in memory: --enforce must refuse it."""
    bad = []

    def check(ok, what):
        print(("  ok    " if ok else "  FAIL  ") + what)
        if not ok:
            bad.append(what)

    rows = [{"path": "core/", "zone": "shared", "checks": [], "why": ""},
            {"path": "core/game/hud2only.", "zone": "hud2", "checks": [], "why": ""},
            {"path": "modules/hud/", "zone": "hud2", "checks": [], "why": ""},
            {"path": "modules/hud/src/cue.", "zone": "shared", "checks": ["pixels"], "why": ""},
            {"path": "modules/pad/", "zone": "shared", "checks": ["classic_frames", "window_prompts"], "why": ""},
            {"path": "modules/pad/src/layer.", "zone": "classic", "checks": ["classic_frames"], "why": ""}]
    texts = {
        "core/game/hud2only.h": "namespace hud2only\n{\ninline int Wake() { return 1; }\n}\n",
        "core/ui/views.h": "namespace views\n{\nstruct HudContext {};\ninline bool HudActive() { return hud.Active && hud.Active(); }\n}\n",
        "modules/hud/src/hud2.h": "namespace hud2 { void Update(); }\n",
        "modules/hud/src/hud2.cpp": '#include "hud2.h"\nvoid f() { views::HudActive(); views::hud.Flight(1); }\n',
        "modules/hud/src/cue.cpp": '#include "hud2.h"\n',
        "modules/pad/src/menu.cpp": ("// views::HudActive() in a comment\n"
                                     'const char* s = "views::SkinOn()";\n'
                                     "if (views::hud.Overlay)\n"
                                     "    views::hud.Overlay(d);\n"
                                     "views::HudContext hc;\n"
                                     "if (bus->ReadLayout && bus->ReadLayout(&l)) {}\n"
                                     "u.hudStyle = h.hudStyle;\n"
                                     "int w = hud2only::Wake();\n"),
        "modules/pad/src/layer.cpp": '#include "../../hud/src/hud2.h"\n#include "ui/hud2_kit.h"\n',
        "core/ui/hud2_kit.h": "namespace hud2kit {}\n",
    }
    uses = hud2_uses(texts, rows)
    got = sorted((p, k, w) for p, _, k, w in uses)
    want = sorted([("modules/hud/src/cue.cpp", "include", "modules/hud/src/hud2.h"),
                   ("modules/pad/src/menu.cpp", "view", "Overlay"),
                   ("modules/pad/src/menu.cpp", "bus", "ReadLayout"),
                   ("modules/pad/src/menu.cpp", "style", "hudStyle"),
                   ("modules/pad/src/menu.cpp", "name", "hud2only"),
                   ("modules/pad/src/layer.cpp", "include", "modules/hud/src/hud2.h")])
    check(got == want, f"uses counted: a view call, the bus's layout, a read of hud_style, a HUD 2 core name, includes; not inside HUD 2, "
                       f"not a comment, a string, a null test, a type or a write ({len(got)} of {len(want)})")
    if got != want:
        print("        got  " + repr(got))
    # the cheap dodges: each must count as one use
    dodges = [("a #define body", "#define HUD_UP views::HudActive()\nbool a = HUD_UP;\n", "view"),
              ("spaces around ::", "bool a = views :: HudActive();\n", "view"),
              ("spaces in a view entry's call", "views :: hud . Flight(1);\n", "view"),
              ("hud_style through a pointer", "int s = o->hudStyle;\n", "style"),
              ("the bus by value", "g_bus.ReadLayout(&l);\n", "bus"),
              ("a using-declaration of a view helper", "using views::HudActive;\n", "using"),
              ("a using-declaration of the HUD's view", "using views::hud;\n", "using"),
              ("using namespace views", "using namespace views;\n", "using"),
              ("using namespace of a HUD 2 core namespace", "using namespace hud2only;\n", "using"),
              ("a HUD 2 core name with spaces", "int w = hud2only :: Wake();\n", "name")]
    for what, line, kind in dodges:
        one = {p: t for p, t in texts.items() if p != "modules/pad/src/menu.cpp"}
        one["modules/pad/src/menu.cpp"] = line
        found = [u for u in hud2_uses(one, rows) if u[0] == "modules/pad/src/menu.cpp"]
        check(len(found) == 1 and found[0][2] == kind, f"a dodge counted as one {kind} use: {what} ({len(found)})")
    check(classic_includes(texts, rows) == ["modules/pad/src/layer.cpp:1 includes modules/hud/src/hud2.h",
                                            "modules/pad/src/layer.cpp:2 includes core/ui/hud2_kit.h"],
          "a classic-only file's includes of HUD 2 and of its draw kit are refused")
    check(zone_problems(rows + [{"path": "modules/gone/", "zone": "shared", "checks": ["nope"], "why": ""}], list(texts) + ["tools/x.cpp"])
          == ["modules/gone/: check 'nope' is not in CHECKS", "modules/gone/: the row matches no source (a moved or deleted file?)",
              "tools/x.cpp: in no zone (add a row to tools\\hud_zones.tsv)"],
          "the table refuses an unknown check, a row matching nothing, a source in no zone")
    check(zone_row("modules/hud/src/cue.cpp", rows)["zone"] == "shared" and zone_row("modules/hud/src/hud2.cpp", rows)["zone"] == "hud2",
          "the longest prefix wins")
    per, union = checks_for(["modules/pad/src/menu.cpp", "modules/hud/src/hud2.cpp", "docs/X.md", "modules/new/a.cpp"], rows)
    check(per[0][2] == ["classic_frames", "window_prompts"] and per[1][2] == HUD2_STANDDOWN and per[2] == ("docs/X.md", "", [])
          and per[3][1] is None and union == [c for c in CHECKS if c in {"classic_frames", "window_prompts", *HUD2_STANDDOWN}],
          "--changed: a shared file's checks, HUD 2's stand-down check, nothing for a doc, a new folder in no zone")
    door_rows = rows + [{"path": "core/ui/door.", "zone": "door", "checks": [], "why": ""}]
    door_texts = dict(texts, **{"core/ui/door.cpp": "void Frame() { views::hud.Flight(1); }\nint s = hudrows::g.hudStyle;\n"})
    check(hud2_uses(door_texts, door_rows) == hud2_uses(texts, rows)
          and sorted((k, w) for _, _, k, w in hud2_uses(door_texts, door_rows, door=True)) == [("style", "hudStyle"), ("view", "Flight")],
          "the door's uses are counted apart from the uses from outside HUD 2")
    check(ratchet(5, 5)[0] and not ratchet(6, 5)[0] and not ratchet(6, 5)[1] and not ratchet(4, 5)[0] and ratchet(4, 5)[1],
          "the ratchet: at the baseline it passes; a rise fails; a fall fails --enforce until the baseline is lowered")
    # the real tree: at the baseline, and with one new use in a shared file (in memory only: no source is written)
    rows = read_zones()
    sources = tracked_sources()
    real = read_texts(sources)
    base = hud2_uses(real, rows)
    check(len(base) == HUD2_USES, f"the real tree is at the baseline ({len(base)} uses, baseline {HUD2_USES})")
    target = "modules/controller/src/impl/padbases.cpp"
    edited = dict(real)
    edited[target] = real[target] + "\nstatic bool NewHud2Branch() { return views::HudActive(); }\n"
    after = hud2_uses(edited, rows)
    new = [u for u in after if u not in base]
    print("        the zones' report with that line added (what --enforce prints):")
    fails_enforce, fails_report = zone_report(False, {target: edited[target]})
    check(len(after) == len(base) + 1 and len(new) == 1 and new[0][2:] == ("view", "HudActive") and fails_enforce and fails_report,
          f"a new views::HudActive() in {target} is refused, by --enforce and by the plain report")
    check(hud2_names(real, rows) == ["indicators", "promptshow", "roletext"], # (gamedata, equiprules shared since audit 78_)
          "the names of HUD 2's core files: " + ", ".join(hud2_names(real, rows)))
    check(zone_problems(rows, sources) == [], "every source of core\\ and modules\\ is in one zone, every row matches a source")
    # the page framework's rules (audit 70b F1b): each planted violation is found, the allowed forms are not
    pt = {"modules/hud/src/pages/page_view.h": "", "modules/hud/src/pages/page_kit.h": "", "core/ui/hud2_kit.h": "",
          "core/game/screens/nnhelp_model.h": "", "core/game/screens/nnhelp_screen.h": "", "modules/hud/src/nnpage.h": "",
          "modules/hud/src/pages/neuronet/info_view.cpp": ('#include "../page_view.h"\n#include "../page_kit.h"\n#include "ui/hud2_kit.h"\n'
                                                          '#include "game/screens/nnhelp_model.h"\n#include <vector>\n'
                                                          '#include "game/screens/nnhelp_screen.h"\n#include "../../nnpage.h"\n'),
          "modules/hud/src/pages/neuronet/info_page.cpp": ("// exe::X in a comment\nvoid A() { hk::Call<int>(1); }\n"
                                                          "bool B() { return exe::Thing.ok(); }\n"),
          "modules/hud/src/pages/pagerun.cpp": "void Park() { hk::Call<void>(exe::ControlSetPosition, 0); }\n",
          "core/game/screens/market_screen.cpp": "static bool Buy(u32 r) { return uiclick::Click(r, \"Buy\"); } // uiclick::Click(\n",
          "modules/hud/src/basepage.cpp": "void F() { nnkit::PlaceAt(c, 1, 2); room::SendScreenMsg(r, 1); }\n"}
    check(view_includes(pt) == ["modules/hud/src/pages/neuronet/info_view.cpp:6 includes core/game/screens/nnhelp_screen.h",
                                "modules/hud/src/pages/neuronet/info_view.cpp:7 includes modules/hud/src/nnpage.h"],
          "a view includes only the draw kits and a screen's model: " + "; ".join(view_includes(pt)))
    check(duplicate_names(["core/a/x.h", "core/b/x.h", "modules/hud/src/x.h", "modules/hud/src/pages/dealers/list_view.cpp",
                           "modules/hud/src/pages/neuronet/list_view.cpp", "modules/npc/src/list_view.cpp"])
          == ["x.h in core: core/a/x.h, core/b/x.h",
              "list_view.cpp in modules/hud: modules/hud/src/pages/dealers/list_view.cpp, modules/hud/src/pages/neuronet/list_view.cpp"],
          "a file name twice in one tree is refused, once in each of two trees is not")
    check(pages_calls(pt) == ["modules/hud/src/pages/neuronet/info_page.cpp:2 hk::Call", "modules/hud/src/pages/neuronet/info_page.cpp:3 exe::"],
          "a page's own game call is refused (not in a comment; the runtime's park allowed): " + "; ".join(pages_calls(pt)))
    check(sorted(w for _, _, w in borrowed_uses(pt)) == ["Click", "PlaceAt", "SendScreenMsg"],
          "the borrowed uses: a control's click, a control thrown, a screen message (not in a comment)")
    real_borrowed = len(borrowed_uses(real))
    check(real_borrowed == PAGES_BORROWED, f"the real tree is at the pages' borrowed baseline ({real_borrowed}, baseline {PAGES_BORROWED})")
    check(view_includes(real) == [] and duplicate_names(sources) == [] and pages_calls(real) == [],
          "the real tree: views include only the kits and models, every name once in its tree, no page calls the game")
    # HUD 2's game-free core (audit 71b B3c): the statics rule on in-memory sources, each planted violation found, the allowed forms not
    st = ("namespace hud2\n{\n"
          "static FrameHost g_host;\n"                     # 3  found (a mutable file-scope static)
          "static const PartsIn kNoIn;\n"                  #    const: not found
          "static const PartsIn* g_in = &kNoIn;\n"         # 5  found (a mutable pointer to const)
          "static const wchar_t* const kWords[2] = {L\"A\", L\"B\"};\n"
          "constexpr const wchar_t* kList[2] = {L\"A\", L\"B\"};\n"
          "int g_plain = 1;\n"                             # 8  found (namespace scope, no keyword)
          "inline float g_inline = 0;\n"                   # 9  found
          "extern int g_extern;\n"                         # 10 found
          "static void (*g_fn)() = nullptr;\n"             # 11 found (a function pointer)
          "static double NoSeconds() { return 0; }\n"
          "float Draw(const Model& m, float a = 1.0f);\n"
          "struct S\n{\n    float t = 0;\n    static int s_count;\n    static constexpr int kN = 2;\n    bool Up() const { return t > 0; }\n};\n"
          "void F()\n{\n    static std::vector<int> rows;\n    static const int kCmds[2] = {1, 2};\n    int local = 0;\n"
          "    auto f = [&](int x) { static int s_in = x; return x; };\n    for (int i = 0; i < 2; ++i) { local += i; }\n}\n"
          "static struct\n{\n    float update = 0;\n} g_perf;\n"
          "static int g_brace{0};\nstatic Foo g_obj{};\nstatic const int kBrace{2};\n"  # brace initialisers (the verifier's finding)
          "template <class T> T g_tpl = T();\ntemplate <class T> constexpr T kTpl = T();\n"
          "void G()\n{\n    static int s_brace{3};\n    try { local(); } catch (...) {}\n}\n"
          "class C\n{\npublic: static int s_pub;\nprivate:\n    static int s_priv = 0;\nprotected: int m_member;\n};\n"  # after a label
          "void H(int x)\n{\n    switch (x)\n    {\n    case K::A: static int s_case = 0; break;\n    default: static int s_def; break;\n    }\n}\n"
          "template <class T, class = void> struct Takes : std::false_type\n{\n};\n"  # braces inside decltype: no variable
          "template <class T> struct Takes<T, std::void_t<decltype(T{(void(0), Any{})})>> : std::true_type\n{\n};\n"
          "static int g_after_decltype = 0;\n"  # found: the scan is back in step after the specialisation
          "void D()\n{\n    std::vector<decltype(x)> v(Make([] { static int s_after_closed = 0; return 1; }));\n}\n"  # found (B4a verifier)
          "} // namespace hud2\n")
    got_st = [name for _, name in mutable_statics(st)]
    want_st = ["g_host", "g_in", "g_plain", "g_inline", "g_extern", "g_fn", "s_count", "rows", "s_in", "g_perf", "g_brace", "g_obj",
               "g_tpl", "s_brace", "s_pub", "s_priv", "s_case", "s_def", "g_after_decltype", "s_after_closed"]
    check(got_st == want_st, "the bench core's statics rule: mutable file, namespace, inline, extern, function-pointer, class, local and lambda statics "
                             "found; const, constexpr, functions, members and locals not (" + ", ".join(got_st) + ")")
    planted_static = BENCH_CORE[2]  # hud2_parts.cpp
    real_bench = bench_includes(real) + bench_hidden(real)
    real_statics = [s for s in bench_statics(real) if (s[0], s[2]) not in BENCH_BOUND]
    check(not real_bench and not real_statics and all(p in real for p in BENCH_CORE),
          "the real tree: the bench core includes only game-free headers, holds no mutable static beside its binding, reads no live host state")
    with_static = dict(real, **{planted_static: real[planted_static] + "\nnamespace hud2\n{\nstatic int g_planted = 0;\n}\n"})
    check([s[2] for s in bench_statics(with_static) if (s[0], s[2]) not in BENCH_BOUND] == ["g_planted"],
          "a planted static in " + planted_static + " is found")
    with_include = dict(real, **{BENCH_CORE[4]: '#include "game/fl.h"\n' + real[BENCH_CORE[4]]})  # hud2_target.cpp
    check(any("game/fl.h" in line for line in bench_includes(with_include)), "a planted game include in " + BENCH_CORE[4] + " is found")
    with_read = dict(real, **{BENCH_CORE[1]: real[BENCH_CORE[1]] + "\nstatic bool Planted() { return uiopt::Get().hudThreats && GetTickCount(); }\n"})
    check(len(bench_hidden(with_read)) == 2, "a planted live read of the settings and the clock in " + BENCH_CORE[1] + " is found")
    print("        the bench core's report with the planted static (what --enforce prints):")
    fails_enforce, fails_report = bench_report(False, {planted_static: with_static[planted_static]})
    check(fails_enforce and fails_report, "--enforce and the plain report refuse the planted static")
    print(f"selftest: {'OK' if not bad else 'FAILED'} ({len(bad)} failure(s))")
    return not bad


def main(argv=None):
    argv = sys.argv[1:] if argv is None else argv
    if "--selftest" in argv:
        return 0 if selftest() else 1
    if "--changed" in argv or "--staged" in argv or "--range" in argv:
        return changed_report(argv)
    enforce = "--enforce" in argv
    listing = "--list" in argv
    owners = read_map()
    # the zones first: tools\drift_check.ps1 takes the last line printed as its row, and that stays the module boundary's summary
    zones_enforce, zones_report = zone_report(listing)
    pages_enforce, pages_fail = pages_report(listing)  # the page framework's rules (audit 70b F1b)
    bench_enforce, bench_fail = bench_report(listing)  # HUD 2's game-free core (audit 71b B3c)
    zones_enforce, zones_report = zones_enforce or pages_enforce or bench_enforce, zones_report or pages_fail or bench_fail
    stale = stale_lines()
    print(f"module_lint: {len(stale)} map rows point past their file")
    for line in stale[:20]:
        print("  " + line)
    pairs = collections.Counter()
    crossings = []
    unowned = 0
    missing = []
    for source, owner in sorted(owners.items()):
        path = os.path.join(ROOT, source)
        if not os.path.exists(path):
            missing.append(source)
            continue
        if owner in UNOWNED:
            unowned += 1
            continue
        with open(path, encoding="utf-8", errors="replace") as f:
            for number, line in enumerate(f, 1):
                match = INCLUDE.match(line)
                if not match:
                    continue
                target = resolve(source, match.group(1), owners)
                if target is None:
                    continue
                to = owners[target]
                if to in UNOWNED or to == owner or to == "core":
                    continue
                pairs[(owner, to)] += 1
                crossings.append(f"{source}:{number} {owner} -> {to} ({os.path.basename(target)})")
    # most of core is not in the map, and a module's header included there is the crossing that matters most
    core_files = subprocess.run(["git", "-C", ROOT, "ls-files", "core"], capture_output=True, text=True).stdout.split("\n")
    for source in core_files:
        if source in owners or not source.endswith((".cpp", ".h")):
            continue
        with open(os.path.join(ROOT, source), encoding="utf-8", errors="replace") as f:
            for number, line in enumerate(f, 1):
                match = INCLUDE.match(line)
                target = resolve(source, match.group(1), owners) if match else None
                if target and target.startswith("modules/"):
                    pairs[("core", owners[target])] += 1
                    crossings.append(f"{source}:{number} core -> {owners[target]} ({os.path.basename(target)})")
    print(f"module_lint: {len(owners)} files mapped, {unowned} without one owner yet, {len(crossings)} crossings")
    for (a, b), count in pairs.most_common():
        print(f"  {a} -> {b}: {count}")
    if listing:
        print("\n".join(crossings))
    for source in missing:
        print(f"  MISSING: {source} (in the map, not on disk)")
    # a cut module (its files are under modules\) reaches the rest only through core, from the day it is cut
    cut = {owner for path, owner in owners.items() if path.startswith("modules/")}
    out_of_cut = [c for c in crossings if c.split(" ")[1] in cut]
    for line in out_of_cut:
        print("  CUT MODULE CROSSES: " + line)
    print(f"module_lint: cut modules {sorted(cut)}, {len(out_of_cut)} crossings out of them")
    # .gitignore is a whitelist: P5's first commit left a module's new files behind, and it still built here
    git = ["git", "-C", ROOT, "ls-files", "--others", "modules"]
    loose = [f for f in subprocess.run(git, capture_output=True, text=True).stdout.split("\n") if f]
    for path in loose:
        print("  NOT TRACKED: " + path)
    misplaced = sorted(f"{p}: owner {o}" for p, o in owners.items() if p.startswith("modules/") and p.split("/")[1] != FOLDER.get(o))
    tracked = subprocess.run(["git", "-C", ROOT, "ls-files", "modules"], capture_output=True, text=True).stdout.split("\n")
    unlisted = sorted(f for f in tracked if f.endswith((".cpp", ".h")) and f not in owners)
    for line in misplaced:
        print("  NOT IN ITS OWNER'S FOLDER: " + line)
    for line in unlisted:
        print("  NOT IN THE MAP: " + line)
    print(f"module_lint: {len(misplaced)} files outside their owner's folder, {len(unlisted)} sources not in the map")
    bare = unguarded()
    for line in bare:
        print("  VIEW ENTRY WITHOUT A GUARD: " + line)
    print(f"module_lint: {len(bare)} view entries without a guard")
    if out_of_cut or loose or bare or zones_report or (enforce and (crossings or missing or unowned or misplaced or unlisted or zones_enforce)):
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
