"""Story order: the order in which the story's dialogue plays in the game, derived from the game's own data.

    <python> -I story_order.py [--install <Freelancer HD Edition folder>] [--out <tsv>] [--dethorn <dethorn.exe>]

Writes data/story_order.tsv (ids, order and provenance only: no game text), read by the review tool's "Story order"
view (docs/REVIEW_PROCESS.md section 6). How the order is derived:

1. **Campaign order**: the story missions' scripts in the order Content.dll's story code lists them (the strings
   `Missions\\M01a\\m01a.ini` ... `Missions\\M13\\m13.ini` in DLLS\\bin\\Content.dll); a room scene ini Content.dll loads
   itself (not a mission script: `M001a_s006x_Li01_02_nrml.ini`) plays at the end of the mission whose folder holds it.
2. **A mission's dialogue** (DATA\\MISSIONS\\Mxx\\mxx.ini): a walk of the trigger graph from the `InitState = ACTIVE`
   triggers on an estimated clock. A trigger fires when its condition is met: `Cnd_True` at once, `Cnd_Timer` after
   its seconds, `Cnd_CommComplete` when that line has played (each line takes its clip length, `sec` in
   subtitles.tsv; comms queue one after another on the radio), `Cnd_WatchTrigger` when the watched trigger fired;
   a condition the player meets (distance, kills, docking, entering a room, ...) is taken in the order the triggers
   became active, one at a time once nothing timed is left (the main path). Firing runs the trigger's actions in
   file order: `Act_ActTrig` / `Act_DeactTrig` (de)activate triggers, `Act_SendComm` / `Act_EtherComm` /
   `Act_StartDialog` ([Dialog] `Line`s) play lines, `Act_CallThorn` plays a cutscene's lines at their times,
   `Act_AddRTC` a room scene's (its [CharacterEncounter] `action` / `offer` / `decision` / `accept` scenes in that
   order; `reject` / `decline` = a branch), `Act_ChangeState = SUCCEED` ends the mission.
3. **Branches** (marked, placed right after the point where their trigger became active): triggers that fail the
   mission (`Act_ChangeState = FAIL`, or only lead to one), fire on an incident (`Cnd_ProjHit*`, `Cnd_TetherBroke`),
   on a declined offer (`Cnd_MsnResponse = reject`), were deactivated by another trigger before their condition
   (an alternative), or still waited when the mission ended; with the triggers they activate.
4. **Cutscenes / room scenes** (.thn): their SOUND entities' `START_SOUND` events by time. Text .thn files (HDE
   rewrites some) are read as they are; compiled ones are decoded with Jason Hood's Dethorn (adoxa's Freelancer
   tools, `<root>\\research\\thn_work\\EXE\\dethorn.exe`, which uses the game's own THORN code) into a cache
   (`<root>\\research\\thn_work\\decoded\\`, git-ignored, renewed when the .thn changes).
Lines no mission walk reaches are listed as "not placed" with the reason (in a trigger never activated, in a
scene no script starts, named by no mission script or scene). The scene mixes (gf_sounds.ini `mix_*`) are placed
like any other sound. Limits: the clock is an estimate (player-driven steps are taken in activation order), so
parallel threads of a mission (two comms racing a timer) can come out in another order than a given play-through.
"""
import argparse
import hashlib
import heapq
import itertools
import re
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))  # run with python -I: no implicit script dir
import tsvio  # noqa: E402

REPO_ROOT = HERE.parents[3]
DATA_DIR = HERE.parent / "data"
OUT_FILE = DATA_DIR / "story_order.tsv"
COLUMNS = ["order", "mission", "section", "id", "play", "at", "via", "source"]
NOT_PLACED = "not placed"

PLAYER_STEP = 20.0     # estimated seconds for a step the player makes (only orders, never shown as a fact)
DEFAULT_SEC = 3.0      # a line without a known clip length
MAX_FIRES = 3          # a trigger re-armed in a loop fires at most this often in the walk
MAX_EVENTS = 50000
INCIDENT = {"cnd_tetherbroke"}  # the player leaves the escort
# met by hits in a fight (often the NPCs' own, a scripted beat): after the other player-driven conditions
LATE = {"cnd_projhit", "cnd_projhitshiptolbl", "cnd_healthdec", "cnd_systemexit"}  # (leaving the mission's system)
# what the player does (moves, docks, answers, kills): before conditions only NPCs meet (an NPC reaching a point)
PLAYER_ACTS = {"cnd_baseenter", "cnd_baseexit", "cnd_spaceenter", "cnd_spaceexit", "cnd_locenter", "cnd_locexit",
               "cnd_charselect", "cnd_msnresponse", "cnd_popupdialog", "cnd_rtcdone", "cnd_launchcomplete",
               "cnd_playerlaunch", "cnd_systementer", "cnd_systemexit", "cnd_jumpincomplete", "cnd_tlentered",
               "cnd_tlexited", "cnd_intradelane", "cnd_playermaneuver", "cnd_inspace", "cnd_lootacquired",
               "cnd_cargoscanned", "cnd_hasmsn", "cnd_destroyed"}


def wait_tier(cond: str, args: list) -> int:
    """Which player-driven condition is taken first when several wait: 0 = the player's own act, 1 = one only
    NPCs meet, 2 = hits / damage in a fight (often a scripted beat inside it)."""
    if cond in LATE:
        return 2
    if cond in PLAYER_ACTS or any(a.lower() == "player" for a in args):
        return 0
    return 1


# where the player must be for a condition: the walk finishes what can be done where the player is before moving
IN_BASE = {"cnd_locenter", "cnd_locexit", "cnd_charselect", "cnd_msnresponse", "cnd_rtcdone"}
TO_BASE = {"cnd_baseenter"}
TO_SPACE = {"cnd_spaceenter", "cnd_launchcomplete", "cnd_playerlaunch", "cnd_baseexit"}
NEUTRAL = {"cnd_popupdialog", "cnd_hasmsn"}


def side_rank(cond: str, loc, started: bool) -> int:
    """0 = met where the player is, 1 = the move away from here, 2 = needs the other side first (loc: "space",
    "base" or None = unknown; started: the player has made a step in this mission)."""
    if loc is None or cond in NEUTRAL:
        return 0
    if cond in TO_SPACE:
        return 0 if loc == "space" and not started else (1 if loc == "base" else 2)
    if cond in TO_BASE:
        return 0 if loc == "base" and not started else (1 if loc == "space" else 2)
    if cond in IN_BASE:
        return 0 if loc == "base" else 2
    return 0 if loc == "space" else 2


def loc_after(cond: str, loc):
    if cond in TO_BASE or cond in IN_BASE:
        return "base"
    if cond in TO_SPACE:
        return "space"
    return loc if cond in NEUTRAL else "space"


def approach(cond: str, args: list):
    """(target, radius) of an "inside" distance condition (Cnd_DistShip / DistVec = inside, <ship>, <target or
    x, y, z>, <radius>), else None: of two on the same target the player meets the wider one first."""
    if cond not in ("cnd_distship", "cnd_distvec", "cnd_distcircle") or len(args) < 4 or args[0].lower() != "inside":
        return None
    try:
        return ",".join(a.lower() for a in args[1:-1]), float(args[-1])
    except ValueError:
        return None


def player_dies(cond: str, args: list) -> bool:
    return cond == "cnd_destroyed" and bool(args) and args[0].lower() == "player"


# ------------------------------------------------------------------------------------------------ readers
def parse_ini(text: str) -> list:
    """A text ini as [(section name lower-case, [(key lower-case, value, line number)], line number)]."""
    out, cur = [], None
    for no, raw in enumerate(text.splitlines(), 1):
        line = raw.split(";", 1)[0].strip()
        if not line:
            continue
        if line.startswith("[") and "]" in line:
            cur = (line[1:line.index("]")].strip().lower(), [], no)
            out.append(cur)
        elif cur is not None:
            k, _, v = line.partition("=")
            cur[1].append((k.strip().lower(), v.strip(), no))
    return out


def args_of(value: str) -> list:
    return [a.strip() for a in value.split(",")]


ENTITY = re.compile(r'entity_name\s*=\s*"([^"]*)"(.*?)(?=entity_name\s*=|\Z)', re.S)
TEMPLATE = re.compile(r'template_name\s*=\s*"([^"]*)"')
IS_SOUND = re.compile(r"\btype\s*=\s*SOUND\b")
START_SOUND = re.compile(r'\{\s*([0-9.eE+-]+)\s*,\s*START_SOUND\s*,\s*\{\s*"([^"]+)"')


def parse_thn(text: str) -> list:
    """A decoded (Lua) .thn scene: [(seconds, sound template)] of its START_SOUND events, by time."""
    templates = {}
    for m in ENTITY.finditer(text):
        body = m.group(2)
        t = TEMPLATE.search(body)
        if t and IS_SOUND.search(body):
            templates[m.group(1)] = t.group(1)
    events = []
    for i, m in enumerate(START_SOUND.finditer(text)):
        name = m.group(2)
        events.append((float(m.group(1)), i, templates.get(name, name)))
    events.sort()
    return [(t, name) for t, _, name in events]


# ------------------------------------------------------------------------------------------------ game files
class GameFiles:
    """The install's DATA folder (paths case-insensitive, as the game reads them); .thn decoded through the cache."""

    def __init__(self, install: Path, dethorn: Path = None, cache: Path = None):
        self.data = install / "DATA"
        self.install = install
        self.dethorn = dethorn
        self.cache = cache
        self._dirs = {}
        self.problems = []

    def find(self, rel: str):
        p = self.data
        for part in re.split(r"[\\/]+", rel.strip()):
            if not part:
                continue
            if not p.is_dir():
                return None
            if p not in self._dirs:
                self._dirs[p] = {x.name.lower(): x for x in p.iterdir()}
            p = self._dirs[p].get(part.lower())
            if p is None:
                return None
        return p

    def rel(self, path: Path) -> str:
        return "DATA\\" + str(path.relative_to(self.data)).replace("/", "\\")

    def ini(self, rel: str):
        p = self.find(rel)
        return (tsvio.read_text(p), self.rel(p)) if p and p.is_file() else (None, rel)

    def thn(self, rel: str):
        """(Lua text, source path) of a .thn scene, or (None, why)."""
        p = self.find(rel)
        if not p or not p.is_file():
            return None, f"{rel} (missing)"
        raw = p.read_bytes()
        if not raw.startswith(b"\x1bLua"):
            return raw.decode("latin-1"), self.rel(p)
        if not self.cache:
            return None, f"{self.rel(p)} (compiled; no decoder)"
        digest = hashlib.sha1(raw).hexdigest()
        out = self.cache / (str(p.relative_to(self.data)) + ".lua")
        stamp = out.with_suffix(".sha1")
        if not (out.exists() and stamp.exists() and stamp.read_text().strip() == digest):
            if not (self.dethorn and self.dethorn.exists()):
                self.problems.append(f"no Dethorn at {self.dethorn}: {self.rel(p)} not decoded")
                return None, f"{self.rel(p)} (compiled; Dethorn missing)"
            out.parent.mkdir(parents=True, exist_ok=True)
            work = out.parent / p.name
            shutil.copyfile(p, work)
            subprocess.run([str(self.dethorn), p.name], cwd=out.parent, capture_output=True, timeout=120)
            lua = work.with_suffix(".lua")
            work.unlink(missing_ok=True)
            if not lua.exists():
                self.problems.append(f"Dethorn could not decode {self.rel(p)}")
                return None, f"{self.rel(p)} (Dethorn failed)"
            lua.replace(out)
            stamp.write_text(digest)
        return tsvio.read_text(out), self.rel(p)

    def start_loc(self):
        r"""Where a new game starts: EXE\NewPlayer.fl's [Player] has a system and no base = in space."""
        p = self.install / "EXE" / "NewPlayer.fl"
        if not p.exists():
            return None
        keys = {k.strip().lower() for k, _, v in (ln.partition("=") for ln in tsvio.read_text(p).splitlines()) if v}
        return "base" if "base" in keys else ("space" if "system" in keys else None)

    def startup_movies(self) -> list:
        r"""The movie stems EXE\freelancer.ini's [Startup] plays, in order (movie_file = movies\FL_Intro.wmv)."""
        ini = self.install / "EXE" / "freelancer.ini"
        text = tsvio.read_text(ini) if ini.exists() else ""
        return [re.split(r"[\\/]", v)[-1].rsplit(".", 1)[0].lower()
                for name, keys, _ in parse_ini(text) if name == "startup" for k, v, _ in keys if k == "movie_file"]

    def movie_files(self) -> dict:
        d = self.find("MOVIES")
        return {p.stem.lower(): p.name for p in d.iterdir() if p.suffix.lower() == ".wmv"} if d else {}

    def campaign(self) -> list:
        """[(mission label, script path)] in Content.dll's order, plus the room scene inis it loads itself."""
        dll = self.install / "DLLS" / "bin" / "Content.dll"
        if not dll.exists():
            return [], []
        blob = dll.read_bytes().decode("latin-1")
        missions, scenes = [], []
        for m in re.finditer(r"(?i)missions\\(m\d+[a-z]?)\\([a-z0-9_]+\.ini)", blob):
            folder, name = m.group(1), m.group(2)
            rel = f"MISSIONS\\{folder}\\{name}"
            if re.fullmatch(r"(?i)m\d+[a-z]?\.ini", name):
                if all(rel.lower() != r.lower() for _, r in missions):
                    missions.append((folder[0].upper() + folder[1:], rel))
            elif self.find(rel):
                scenes.append((folder, rel))
        return missions, scenes


# ------------------------------------------------------------------------------------------------ the walk
class Mission:
    """One mission script's triggers and dialogs."""

    def __init__(self, text: str, source: str):
        self.source = source
        self.triggers, self.dialogs, self.order = {}, {}, []
        for name, keys, no in parse_ini(text):
            nick = next((v for k, v, _ in keys if k == "nickname"), "")
            if name == "trigger" and nick:
                t = {"nick": nick, "line": no, "system": next((v for k, v, _ in keys if k == "system"), ""),
                     "init": any(k == "initstate" and v.lower() == "active" for k, v, _ in keys),
                     "conds": [(k, args_of(v)) for k, v, _ in keys if k.startswith("cnd_")],
                     "acts": [(k, args_of(v), n) for k, v, n in keys if k.startswith("act_")]}
                self.triggers[nick.lower()] = t
                self.order.append(nick.lower())
            elif name == "dialog" and nick:
                self.dialogs[nick.lower()] = [args_of(v) for k, v, _ in keys if k == "line"]
        self._fail = {}

    def targets(self, t, act="act_acttrig"):
        return [a[0].lower() for k, a, _ in t["acts"] if k == act and a and a[0]]

    def fails(self, nick: str, stack=()) -> bool:
        """The trigger fails the mission, or everything it activates leads only to a failure."""
        if nick in self._fail:
            return self._fail[nick]
        t = self.triggers.get(nick)
        if t is None or nick in stack:
            return False
        if any(k == "act_changestate" and a and a[0].upper() == "FAIL" for k, a, _ in t["acts"]):
            r = True
        else:
            nxt = [x for x in self.targets(t) if x in self.triggers]
            r = bool(nxt) and all(self.fails(x, stack + (nick,)) for x in nxt)
        self._fail[nick] = r
        return r

    def branch_reason(self, t):
        """Why a trigger is not on the main path (None = it is)."""
        cond = t["conds"][0] if t["conds"] else ("cnd_true", [])
        if any(k == "act_changestate" and a and a[0].upper() == "FAIL" for k, a, _ in t["acts"]):
            return "branch: fails the mission"
        if self.fails(t["nick"].lower()) and cond[0] != "cnd_true":
            return "branch: leads to a mission failure"
        if cond[0] in INCIDENT:
            return f"branch: only if it happens ({cond[0][4:]})"
        if player_dies(*cond):
            return "branch: if the player's ship is destroyed"
        if cond[0] == "cnd_msnresponse" and cond[1] and cond[1][0].lower() == "reject":
            return "branch: if the player declines the mission"
        return None


class Walk:
    """Walks one mission's trigger graph (module docstring, 2-3); rows go to `emit`."""

    def __init__(self, game, mission: Mission, label: str, known: dict, loc=None):
        self.game, self.m, self.label, self.known = game, mission, label, known
        self.loc, self.started, self.room_free = loc, False, 0.0
        self.rows, self.seq = [], itertools.count()
        self.now, self.comm_free = 0.0, 0.0
        self.heap, self.player = [], []
        self.active, self.fired, self.anchor, self.why_off = {}, {}, {}, {}
        self.wait_comm, self.wait_watch, self.line_end = {}, {}, {}
        self.armed = set()  # activated, not yet fired or deactivated (branch triggers too): Cnd_WatchTrigger's state
        self.ended = False

    # rows -------------------------------------------------------------------------------------------
    def emit(self, line_id, at, section, via, source, play="main", key=None):
        k = line_id.lower()
        if k not in self.known:
            return
        self.rows.append({"mission": self.label, "section": section, "id": self.known[k][0], "play": play,
                          "at": at, "via": via, "source": source, "key": key or (at, 0, next(self.seq))})

    def sec(self, line_id):
        return self.known.get(line_id.lower(), (line_id, DEFAULT_SEC))[1] or DEFAULT_SEC

    def where(self, t):
        return f"{self.m.source}:{t['line']}"

    def section_of(self, t):
        return f"in space ({t['system']})" if t["system"] else "in space"

    # playing ------------------------------------------------------------------------------------------
    def comm(self, line_id, t, act, clock, play, key_base=None):
        start = max(clock["now"], clock["free"])
        end = start + self.sec(line_id)
        clock["free"] = end
        if play == "main":
            self.line_end[line_id.lower()] = end
            for w in self.wait_comm.pop(line_id.lower(), []):
                self.push(end, w)
        self.emit(line_id, start, self.section_of(t), f"trigger {t['nick']} > {act}", self.where(t), play,
                  None if key_base is None else (key_base[0], 1, next(self.seq)))
        return end

    def scene(self, rel, t, act, at, play, key_base=None, section=None):
        lua, src = self.game.thn(rel)
        name = re.split(r"[\\/]", rel)[-1]
        if lua is None:
            self.emit_missing.append((rel, src))
            return at
        end = at
        for sec, tmpl in parse_thn(lua):
            line_end = at + sec + self.sec(tmpl)
            if tmpl.lower() in self.known and play == "main":
                self.line_end[tmpl.lower()] = line_end
                for w in self.wait_comm.pop(tmpl.lower(), []):
                    self.push(line_end, w)
            self.emit(tmpl, at + sec, section or f"cutscene {name}",
                      f"trigger {t['nick']} > {act} > {name} @ {sec:.2f} s", src, play,
                      None if key_base is None else (key_base[0], 1, next(self.seq)))
            if tmpl.lower() in self.known:
                end = max(end, line_end)
        return end

    def room(self, rel, t, at, play, key_base=None):
        """An Act_AddRTC room scene ini: its encounters' scenes in order; a declined offer is a branch."""
        text, src = self.game.ini(rel)
        if text is None:
            self.emit_missing.append((rel, rel))
            return at
        clock = at
        for name, keys, _ in parse_ini(text):
            if name != "characterencounter":
                continue
            loc = next((v for k, v, _ in keys if k == "location"), "")
            stem = re.split(r"[\\/]", rel)[-1].rsplit(".", 1)[0]
            section = f"room scene {stem} ({loc})" if loc else f"room scene {stem}"
            decide = clock
            scenes = {k: v for k, v, _ in keys}
            for k in ("action", "offer", "decision", "accept"):  # the order they play (not the file's key order)
                if k in scenes:
                    clock = self.scene(scenes[k], t, f"Act_AddRTC > {k}", clock, play, key_base, section)
                    if k == "decision":
                        decide = clock
            for k, v, _ in keys:
                if k in ("reject", "decline"):
                    p = "branch: if the player declines the offer" if play == "main" else play
                    self.scene(v, t, f"Act_AddRTC > {k}", decide, p, (decide,) if play == "main" else key_base,
                               section)
        return clock

    # the trigger graph --------------------------------------------------------------------------------
    def push(self, at, nick):
        heapq.heappush(self.heap, (at, next(self.seq), nick))

    def activate(self, nick, by=None):
        t = self.m.triggers.get(nick)
        if t is None or nick in self.active or nick in self.armed:
            return
        self.active[nick] = next(self.seq)
        self.anchor.setdefault(nick, max(self.now, self.comm_free, self.room_free))  # after what is queued
        self.set_armed(nick, True)
        why = self.m.branch_reason(t)
        if why:
            self.why_off.setdefault(nick, why)
            del self.active[nick]
            return
        cond, a = t["conds"][0] if t["conds"] else ("cnd_true", [])
        if cond == "cnd_true":
            self.push(self.now, nick)
        elif cond == "cnd_timer":
            try:
                self.push(self.now + float(a[0]), nick)
            except (ValueError, IndexError):
                self.push(self.now, nick)
        elif cond == "cnd_commcomplete" and a:
            line = a[0].lower()
            if line in self.line_end:
                self.push(max(self.now, self.line_end[line]), nick)
            else:
                self.wait_comm.setdefault(line, []).append(nick)
        elif cond == "cnd_watchtrigger" and a:  # = <trigger>, ON / ACTIVE (armed) | OFF (not armed) | COMPLETE (fired)
            state = a[1].upper() if len(a) > 1 else "ON"
            x = a[0].lower()
            if state == "COMPLETE":
                if self.fired.get(x):
                    self.push(self.now, nick)
                else:
                    self.wait_watch.setdefault((x, "fired"), []).append(nick)
            else:
                on = state != "OFF"
                if (x in self.armed) == on:
                    self.push(self.now, nick)
                else:
                    self.wait_watch.setdefault((x, on), []).append(nick)
        else:
            self.player.append((wait_tier(cond, a), self.active[nick], nick, cond, approach(cond, a)))

    def set_armed(self, nick, on):
        if (nick in self.armed) == on:
            return
        (self.armed.add if on else self.armed.discard)(nick)
        for w in self.wait_watch.pop((nick, on), []):
            self.push(self.now, w)

    def fire(self, nick):
        if nick not in self.active or self.fired.get(nick, 0) >= MAX_FIRES:
            return
        del self.active[nick]
        self.set_armed(nick, False)
        self.fired[nick] = self.fired.get(nick, 0) + 1
        for w in self.wait_watch.pop((nick, "fired"), []):
            self.push(self.now, w)
        t = self.m.triggers[nick]
        clock = {"now": self.now, "free": self.comm_free}
        succeed = False
        for act, a, _ in t["acts"]:
            if not a or not a[0]:
                continue
            if act == "act_acttrig":
                self.activate(a[0].lower(), nick)
            elif act == "act_deacttrig":
                if self.active.pop(a[0].lower(), None) is not None:
                    self.why_off.setdefault(a[0].lower(), f"branch: alternative (deactivated by {t['nick']})")
                self.set_armed(a[0].lower(), False)
            elif act == "act_changestate" and a[0].upper() == "SUCCEED":
                succeed = True
            elif act == "act_sendcomm" and len(a) > 2:
                self.comm(a[2], t, "Act_SendComm", clock, "main")
            elif act == "act_ethercomm" and len(a) > 3:
                self.comm(a[3], t, "Act_EtherComm", clock, "main")
            elif act == "act_startdialog":
                for ln in self.m.dialogs.get(a[0].lower(), []):
                    if len(ln) > 2:
                        self.comm(ln[2], t, f"Act_StartDialog {a[0]}", clock, "main")
            elif act == "act_callthorn":
                self.scene(a[0], t, "Act_CallThorn", self.now, "main")
            elif act == "act_addrtc":
                # one room scene after another (a scene plays whole when the player is in its room)
                self.room_free = self.room(a[0], t, max(self.now, self.room_free), "main")
            else:  # any other action naming a line
                for x in a:
                    if x.lower() in self.known:
                        self.comm(x, t, act, clock, "main")
        self.comm_free = clock["free"]
        if succeed:
            self.ended = True

    def run(self):
        self.emit_missing = []
        for nick in self.m.order:
            if self.m.triggers[nick]["init"]:
                self.activate(nick)
        events = 0
        while not self.ended and events < MAX_EVENTS:
            events += 1
            if self.heap:
                at, _, nick = heapq.heappop(self.heap)
                self.now = max(self.now, at)
                self.fire(nick)
                continue
            waiting = [x for x in self.player if x[2] in self.active]
            if not waiting:
                break
            first = min(waiting, key=lambda x: (x[0] == 2, side_rank(x[3], self.loc, self.started), x[0], x[1]))
            if first[4]:  # a wider circle around the same target is met on the way in
                first = max((x for x in waiting if x[4] and x[4][0] == first[4][0]), key=lambda x: x[4][1])
            self.player.remove(first)
            nick = first[2]
            self.loc, self.started = loc_after(first[3], self.loc), True
            # the player acts once the radio and any room scene are through
            self.now = max(self.now, self.comm_free, self.room_free) + PLAYER_STEP
            self.fire(nick)
        end = self.now
        for nick in self.active:  # armed, never met
            self.why_off.setdefault(nick, "branch: still waiting when the mission ended")
        self.branches()
        return end

    def branches(self):
        """Lines of the triggers off the main path, after the point their trigger became active."""
        done = set()
        for nick in sorted(self.why_off, key=lambda n: self.anchor.get(n, 0)):
            self.expand(nick, self.why_off[nick], self.anchor.get(nick, 0), done, 0)

    def expand(self, nick, why, at, done, depth):
        t = self.m.triggers.get(nick)
        if t is None or nick in done or self.fired.get(nick) or depth > 6:
            return
        done.add(nick)
        key = (at,)
        clock = {"now": at, "free": at}
        for act, a, _ in t["acts"]:
            if not a or not a[0]:
                continue
            if act == "act_sendcomm" and len(a) > 2:
                self.comm(a[2], t, "Act_SendComm", clock, why, key)
            elif act == "act_ethercomm" and len(a) > 3:
                self.comm(a[3], t, "Act_EtherComm", clock, why, key)
            elif act == "act_startdialog":
                for ln in self.m.dialogs.get(a[0].lower(), []):
                    if len(ln) > 2:
                        self.comm(ln[2], t, f"Act_StartDialog {a[0]}", clock, why, key)
            elif act == "act_callthorn":
                self.scene(a[0], t, "Act_CallThorn", at, why, key)
            elif act == "act_addrtc":
                self.room(a[0], t, at, why, key)
            elif act == "act_acttrig":
                self.expand(a[0].lower(), why, at, done, depth + 1)
            elif act not in ("act_deacttrig", "act_changestate"):
                for x in a:
                    if x.lower() in self.known:
                        self.comm(x, t, act, clock, why, key)


# ------------------------------------------------------------------------------------------------ the whole story
def line_refs(text: str, known: dict) -> set:
    return {w.lower() for w in re.findall(r"[A-Za-z0-9_+\-]+", text)} & set(known)


# ------------------------------------------------------------------------------------------------ the intro movie
INTRO = "Intro movie"


def fnv1a(s: str) -> str:
    h = 0x811C9DC5
    for b in s.encode("utf-8"):
        h = ((h ^ b) * 0x01000193) & 0xFFFFFFFF
    return f"{h:08x}"


def movie_id(movie: str, text: str, nth: int = 1) -> str:
    """A movie line's id in the story order (movies.tsv has no line ids): <movie>#<FNV-1a of its text>, -<n> for the
    n-th line of the same text in that movie. It survives a retiming (start / end moved); a text edit gives a new id."""
    return f"{movie}#{fnv1a(' '.join(text.split()))}" + (f"-{nth}" if nth > 1 else "")


def movie_rows(path: Path) -> list:
    """[(movies.tsv row, its story order id)], in file order (rows without a movie or a time left out)."""
    out, seen = [], {}
    for r in tsvio.read_tsv(path) if path.exists() else []:
        try:
            float(r["start"]), float(r["end"])
        except (KeyError, ValueError):
            continue
        if not r.get("movie"):
            continue
        k = (r["movie"], " ".join(r.get("text", "").split()))
        seen[k] = seen.get(k, 0) + 1
        out.append((r, movie_id(r["movie"], r.get("text", ""), seen[k])))
    return out


def movie_lines(path: Path) -> list:
    """[(movie, start, end, id)] of movies.tsv (the intro lines on the movie clock), in file order."""
    return [(r["movie"], float(r["start"]), float(r["end"]), mid) for r, mid in movie_rows(path)]


def intro_rows(startup: list, present: dict, lines: list) -> tuple:
    r"""The movie lines before the first mission: (rows, not placed {id lower: reason}). startup = the movie stems
    freelancer.ini [Startup] plays, in order; present = {stem: file name} of DATA\MOVIES; lines = movie_lines().
    The installed intro first (as it plays at startup), then each other variant of it (FLvibed's movie_intro option
    plays one instead), each under its own heading, every line by its start on the movie clock."""
    rows, not_placed, seq = [], {}, itertools.count()
    variants = sorted({x[0] for x in lines})
    played = [m for m in startup if m in variants]
    for rank, movie in enumerate(played + [m for m in variants if m not in played]):
        if movie in played:
            section, play = f"{movie}.wmv: plays at startup (freelancer.ini [Startup] movie_file)", "main"
        elif any(movie.startswith(p) for p in played):
            there = movie in present
            section = f"{movie}.wmv: instead of the installed intro (FLvibed's movie_intro option)" + \
                ("" if there else "; not in this install")
            play = "alternative intro (movie_intro option)"
        else:
            for m, _, _, mid in lines:
                if m == movie:
                    not_placed[mid.lower()] = "a movie no startup entry or movie option plays"
            continue
        src = "EXE\\freelancer.ini [Startup]" if movie in played else \
            "DATA\\MOVIES\\" + present.get(movie, movie + ".wmv")
        for m, start, _, mid in sorted(x for x in lines if x[0] == movie):
            rows.append({"mission": INTRO, "section": section, "id": mid, "play": play, "at": start,
                         "via": f"{movie}.wmv @ {start:.2f} s", "source": src, "key": (-1, rank, start, next(seq))})
    return rows, not_placed


def derive(game, missions: list, scenes: list, known: dict, intro: list = ()) -> tuple:
    """(rows, not placed {id lower: reason}, problems). missions = [(label, script rel)], scenes = room scene inis
    the story code loads itself [(mission folder, rel)]; known = {id lower: (id, seconds)}; intro = intro_rows()'
    rows (placed before the first mission)."""
    rows, problems, never = list(intro), [], {}
    loc = game.start_loc() if hasattr(game, "start_loc") else None
    for mi, (label, rel) in enumerate(missions):
        text, src = game.ini(rel)
        if text is None:
            problems.append(f"{rel}: missing")
            continue
        m = Mission(text, src)
        w = Walk(game, m, label, known, loc)
        end = w.run()
        loc = w.loc
        for folder, srel in scenes:
            if folder.lower() == rel.split("\\")[1].lower():
                fake = {"nick": "(Content.dll story code)", "line": 0, "system": "", "conds": [], "acts": []}
                w.where = lambda t, s=srel: s  # noqa: E731
                w.room(srel, fake, end + PLAYER_STEP, "main")
        for r in w.rows:
            r["key"] = (mi,) + tuple(r["key"])
        rows += w.rows
        problems += [f"{src}: {x} not readable ({why})" for x, why in w.emit_missing]
        for nick in m.order:  # lines in triggers the walk never reached
            if nick in w.anchor:
                continue
            t = m.triggers[nick]
            text_t = " ".join(",".join(a) for _, a, _ in t["acts"])
            for d in [a[0].lower() for k, a, _ in t["acts"] if k == "act_startdialog" and a]:
                text_t += " " + " ".join(",".join(x) for x in m.dialogs.get(d, []))
            for k in line_refs(text_t, known):
                never.setdefault(k, f"in trigger {t['nick']} ({src}:{t['line']}), never activated from the start")
    rows.sort(key=lambda r: r["key"])
    seen, out = set(), []
    for r in rows:  # each line once: where it first plays (the main path wins over a branch at the same point)
        k = r["id"].lower()
        if k in seen:
            continue
        seen.add(k)
        out.append(r)
    # a main-path occurrence later than a branch one: keep the line on the main path
    main_ids = {r["id"].lower() for r in rows if r["play"] == "main"}
    out = [r for r in out if r["play"] == "main" or r["id"].lower() not in main_ids]
    placed = {r["id"].lower() for r in out}
    missing = {k for k in main_ids if k not in placed}
    if missing:
        seen = set(placed)
        for r in rows:
            k = r["id"].lower()
            if k in missing and r["play"] == "main" and k not in seen:
                seen.add(k)
                out.append(r)
        out.sort(key=lambda r: r["key"])
    for i, r in enumerate(out, 1):
        r["order"] = i
    placed = {r["id"].lower() for r in out}
    not_placed = {k: v for k, v in never.items() if k not in placed}
    return out, not_placed, problems


def scene_index(game) -> dict:
    """{id lower: [.thn paths]} over every story / mission scene (for the "not placed" reasons)."""
    idx = {}
    for top in ("MISSIONS", "SCRIPTS\\STORY"):
        base = game.find(top)
        if not base:
            continue
        for p in sorted(base.rglob("*")):
            if p.suffix.lower() == ".thn":
                lua, src = game.thn(str(p.relative_to(game.data)))
                for _, tmpl in parse_thn(lua or ""):
                    idx.setdefault(tmpl.lower(), []).append(src)
    return idx


def known_lines(subtitles: Path, game) -> dict:
    """{id lower: (id, seconds)}: the story lines of subtitles.tsv and the scene mixes (gf_sounds.ini mix_*)."""
    known = {}
    for r in tsvio.read_tsv(subtitles):
        if r.get("kind") == "story":
            try:
                sec = float(r.get("sec") or 0)
            except ValueError:
                sec = 0.0
            known.setdefault(r["id"].lower(), (r["id"], sec))
    text, _ = game.ini("AUDIO\\gf_sounds.ini")
    for name, keys, _ in parse_ini(text or ""):
        nick = next((v for k, v, _ in keys if k == "nickname"), "")
        if name == "sound" and nick.lower().startswith("mix_"):
            known.setdefault(nick.lower(), (nick, 0.0))
    return known


def write(path: Path, rows: list, not_placed: dict, known: dict) -> None:
    head = ["# Story order (generated by asr/story_order.py from the game's own data; do not edit): the order in which",
            "# the story's dialogue plays - campaign order from Content.dll, each mission's trigger graph walked from its",
            "# start trigger, cutscene / room scene lines by their START_SOUND times (docs/REVIEW_PROCESS.md section 6).",
            "# play: main = the main path; branch: <why> = off it (placed where its trigger became active). at = estimated",
            "# seconds from the mission's start (player steps estimated). via / source = the trigger, action and file.",
            "# Ids and provenance only, no game text. Rows with order '-' were not placed (play = the reason).",
            "\t".join(COLUMNS)]
    body = [f"{r['order']}\t{r['mission']}\t{r['section']}\t{r['id']}\t{r['play']}\t{r['at']:.1f}\t{r['via']}\t"
            f"{r['source']}" for r in rows]
    for k in sorted(not_placed, key=lambda k: known[k][0].lower()):
        body.append(f"-\t{NOT_PLACED}\t\t{known[k][0]}\t{not_placed[k]}\t\t\t")
    tsvio.atomic_write(path, "\n".join(head + body) + "\n")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--install", type=Path, default=REPO_ROOT / "Install" / "Freelancer HD Edition")
    ap.add_argument("--subtitles", type=Path, default=DATA_DIR / "subtitles.tsv")
    ap.add_argument("--movies", type=Path, default=DATA_DIR / "movies.tsv")
    ap.add_argument("--out", type=Path, default=OUT_FILE)
    ap.add_argument("--dethorn", type=Path, default=REPO_ROOT / "research" / "thn_work" / "EXE" / "dethorn.exe")
    ap.add_argument("--cache", type=Path, default=REPO_ROOT / "research" / "thn_work" / "decoded")
    a = ap.parse_args()
    sys.stdout.reconfigure(encoding="utf-8")
    if not (a.install / "DATA").is_dir():
        sys.exit(f"no game install at {a.install} (--install)")
    game = GameFiles(a.install, a.dethorn, a.cache)
    known = known_lines(a.subtitles, game)
    missions, scenes = game.campaign()
    if not missions:
        sys.exit("Content.dll lists no story missions (DLLS\\bin\\Content.dll)")
    mlines = movie_lines(a.movies)
    for m, start, end, mid in mlines:
        known[mid.lower()] = (mid, end - start)
    intro, intro_np = intro_rows(game.startup_movies(), game.movie_files(), mlines)
    rows, not_placed, problems = derive(game, missions, scenes, known, intro)
    not_placed.update(intro_np)
    placed = {r["id"].lower() for r in rows}
    idx = scene_index(game)
    for k in known:
        if k in placed or k in not_placed:
            continue
        if k in idx:
            files = sorted(set(idx[k]))
            not_placed[k] = f"in scene {files[0]}{' (+%d more)' % (len(files) - 1) if len(files) > 1 else ''}, " \
                            f"which no mission script starts (Act_CallThorn / Act_AddRTC)"
        else:
            not_placed[k] = "no mission script or story / mission scene names it"
    write(a.out, rows, not_placed, known)
    by = {}
    for r in rows:
        by.setdefault(r["mission"], [0, 0])[0 if r["play"] == "main" else 1] += 1
    print(f"{a.out}: {len(rows)} placed ({sum(v[0] for v in by.values())} main path, "
          f"{sum(v[1] for v in by.values())} branch), {len(not_placed)} not placed, of {len(known)} story lines, mixes and movie lines")
    for mname, (mn, br) in by.items():
        print(f"  {mname}: {mn} main, {br} branch")
    for p in problems + game.problems:
        print("  problem:", p)


if __name__ == "__main__":
    main()
