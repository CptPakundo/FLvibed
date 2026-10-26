"""Subtitle review tool: listen to a clip, compare the suggested reads, pick one or type your own.

    review_server.py --out <pipeline out dir> --corrections <corrections.tsv> --glossary <glossary.txt>
                     [--port 8765]
    then open http://127.0.0.1:8765/

Reads draft.tsv (merge.py), aligned.tsv, index.tsv, gamenames.tsv. Every decision is written to the
corrections file at once (the merge gives it top priority): a picked read as-is; a typed read fitted to
the conventions (names in the game's/script's spelling, callsign numbers as digits, fragment form for
chatter pieces) unless the reviewer keeps the exact typing. Local only (binds to 127.0.0.1).
"""
import argparse
import contextlib
import difflib
import json
import os
import random
import signal
import socket
import subprocess
import time
import re
import struct
import sys
import threading
import traceback
from datetime import datetime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, unquote, urlparse

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))  # run with python -I: no implicit script dir
import merge  # noqa: E402
import story_order  # noqa: E402
import tsvio  # noqa: E402
from align import DX, sort_key  # noqa: E402

LOCK = threading.Lock()
# who saves (the corrections file's `by` column): --reviewer, else $FLVIBED_REVIEWER, else "user"
REVIEWER = os.environ.get("FLVIBED_REVIEWER") or "user"
# the Bonus DVD shooting script (name_audit.py's reference), in FLvibed's git-ignored research folder
SCRIPT = merge.REPO_ROOT / "research" / "BonusDVD" / "extra_bonus_assets" / "docs" / \
    "story_scripts(shooting)" / "shooting_script.txt"


def clean(text: str) -> str:
    return " ".join(text.replace("\t", " ").split())


def fnv1a(s: str) -> str:
    """The aligner's text hash (FLSubtitles/aligner/align_words.py fnv1a; the runtime's LoadWordTimes checks it too)."""
    h = 0x811C9DC5
    for b in s.encode("utf-8"):
        h = ((h ^ b) * 0x01000193) & 0xFFFFFFFF
    return f"{h:08x}"


def number(s) -> float:
    """A seconds field; an empty or broken one (a hand edit) counts as 0 instead of stopping the tool."""
    try:
        return float(s or 0)
    except ValueError:
        return 0.0


# The pipeline outputs the tool cannot start without (in --out), the columns it reads, and what makes them
# (docs/REVIEW_PROCESS.md section 1). The L0 steps need the game install and the GPU; merge.py is seconds.
REQUIRED_COLUMNS = {
    "draft.tsv": ("id", "voice", "speaker", "text", "source", "fragment", "review", "note", "speech to text", "FLSubs"),
    "index.tsv": ("id", "voice", "file", "kind", "sec", "text"),
    "aligned.tsv": ("id", "voice", "text", "source"),
    "gamenames.tsv": ("prefix", "name", "short"),
}
MAKES = {
    "index.tsv": "build_index.py (reads the game install; then transcribe.py, align.py, report.py)",
    "aligned.tsv": "align.py (after build_index.py and transcribe.py)",
    "gamenames.tsv": "gamenames.py (reads the game install)",
    "draft.tsv": "merge.py --out ..\\out --corrections ..\\data\\corrections.tsv --glossary glossary.txt",
}


def append_atomic(path: Path, text: str) -> None:
    """Appends to a decisions file through a whole-file atomic rewrite: a crash never leaves a half row."""
    if text:
        old = path.read_bytes().decode("utf-8") if path.exists() else ""
        if old and not old.endswith("\n"):  # a hand edit without a final line end: never glue two rows
            old += "\n"
        tsvio.atomic_write(path, old + text)


# Review lists, most worth an ear first. (key, menu label)
GROUPS = [
    # out/repeats.tsv: one entry per actor (all takes); a save applies to every take of that actor
    ("rep_unclear", "Repeated lines: unclear & Claude's guesses"),
    ("rep_tie", "Repeated lines: takes still disagree"),
    ("rep_settled", "Repeated lines: settled by rule, majority or game name"),
    ("recording", "Recording wording, chosen automatically"),
    ("names", "Names the game doesn't know"),
    ("merged", "Chatter/base: FLSubs and speech to text disagreed"),
    # chatter lines sharing one text (any voice, any id): one decision for the whole batch
    ("batch", "Same text, several clips: one decision each"),
    ("nnvoice", "Neural Net voice"),
    ("asr_sentences", "Chatter/base sentences: speech to text only"),
    ("asr_fragments", "Fragments: speech to text only"),
    ("context", "Misheard words fixed from other characters' versions of the line"),
    ("forms", "Contractions/casual forms: the form speech to text heard"),
    ("script_heard", "Script lines: speech to text heard other words"),
    ("script", "Script lines (all sources agree word for word)"),
    ("agreed", "Chatter/base: FLSubs and speech to text agreed"),
    ("gamenames", "Game names (text from the game files: spot-check only)"),
    ("screams", "Screams (style decision pending)"),
    ("mixes", "Scene mixes (need timed cues: later stage)"),
    ("orphans", "Orphan clips (no INI uses them, no copy in use: probably unused)"),
]
# fragments whose text is the game's own name for the object (bases, systems, factions, goods, ship
# types, formation designations, numbers): nothing to decide, only an ear for mispronounced ones
GAMENAME = re.compile(r"^(gcs_refer_(base|system|faction|shiparch|formationdesig)_|gcs_gen_commodity_|gcs_misc_number_)",
                      re.I)
BATCHABLE = {"merged", "asr_sentences", "asr_fragments", "agreed"}
# lists a random sample can settle (story/mission script lines included: all sources agree there)
SAMPLEABLE = {"merged", "batch", "asr_sentences", "asr_fragments", "script", "script_heard", "forms", "context", "agreed",
              "gamenames",
              "nnvoice"}


# Base/bar conversations (GCS_ML_/GCS_FM_ NPC lines, GCS_Trent_ replies): what each scene code is, from the
# game's GCS scene files (DATA/Scripts/GCS/*_<code>*.thn) and the lines recorded for it. Shown as context.
SCENE = {
    "A01a": "the NPC asks to see your ID/card before talking",
    "A02a": "the NPC has nothing for you right now", "A02c": "the NPC wants you gone (you're unwelcome)",
    "A02d": "the NPC has checked you: you're cleared", "A02e": "the NPC has nothing available for you",
    "A03a": "faction rebuff, first half: \"<Faction> doesn't…\" (joined with an A04a ending)",
    "A04a": "faction rebuff, second half: the ending after \"<Faction> doesn't …\"",
    "D01a": "greeting a newcomer: first time here?", "D01b": "greeting someone who let them down last time",
    "D01c": "greeting someone who did good work last time", "D01d": "you came back without finishing their mission",
    "D01e": "greeting a returning visitor", "D01f": "you came back again soon",
    "D01h": "the NPC thinks you've met before", "D01k": "warm welcome back",
    "D02a": "Trent confirms (\"Yeah.\")", "D02b": "Trent apologises for last time and asks for work",
    "D02c": "Trent: just looking around again", "D02d": "Trent: wanted to see what else they have",
    "D02e": "Trent gives his name", "D03a": "the NPC remembers Trent",
    "D04a": "the NPC says they can help and asks your name (\"…, Mister…?\")",
    "D04b": "the bartender says they hear things and asks your name",
    "D04c": "the equipment dealer introduces themself and asks your name",
    "D04d": "the commodity trader introduces themself and asks your name",
    "D04e": "the ship dealer introduces themself and asks your name",
    "D05a": "Trent gives his name", "D05b": "Trent thanks them and asks for work", "D05c": "Trent asks for work",
    "D06a": "the NPC names who they work for: \"I work…\" + \"for <faction>\" (joined)",
    "D07a": "the NPC's faction runs this base", "D07b": "the NPC's faction doesn't run the base but has a deal",
    "F01a": "the NPC offers a rumor", "F01b": "the NPC offers information", "F01d": "the NPC offers a rumor",
    "F01e": "the NPC offers information", "F01f": "the NPC offers to repair your reputation for a fee",
    "F01j": "the NPC offers a mission", "F02a": "Trent: tell me more / I'm listening",
    "F03b": "how secret the info is: not widely known", "F03c": "how secret the info is: a real secret",
    "F03d": "how secret the info is: dangerous to know",
    "F04a": "you can't take the mission: no jump-gate access to that system",
    "F04d": "you can't take the mission: not enough cargo space",
    "F04f": "you can't take the mission: you're already on one for someone else",
    "F04g": "Trent asks how hard/dangerous the offered mission is",
    "F05a": "answer to Trent's question: the mission is easy", "F05b": "answer: the mission is medium",
    "F05c": "answer: the mission is hard", "F06a": "Trent: okay, give me the details",
    "F07a": "Trent takes the job", "F07b": "Trent declines", "F07c": "Trent declines",
    "F08b": "the NPC tells the rumor/info", "F08d": "you can't afford the info/service",
    "F09b": "Trent thanks them for the info", "F10a": "the NPC says good luck (job taken)",
    "F10b": "the NPC says goodbye", "V01a": "a dealer invites you to browse",
    "V05b": "a dealer asks if you found everything (after buying)", "W01a": "greeting as you walk in",
    "W01b": "greeting (bar)", "W01c": "the NPC asks what you need", "W02a": "base welcome: \"Welcome to <base>,\"",
    "W02b": "base welcome, second half: \"run by <owner>\" / \"a <type> Base\"", "W03a": "the NPC offers to show you something",
    "W03b": "the bartender offers a drink", "W04a": "Trent asks to do business", "W04b": "Trent declines a drink, asks for info",
    "W04c": "Trent asks what they have",
}
WHO = {"ML": "male NPC", "FM": "female NPC", "Trent": "Trent"}
PREFIX = {"rmb_": "Mission Commission (random mission) radio", "rms_": "an enemy pilot in a random mission",
          "nag_": "a mission character nagging/reminding Trent", "bc_": "a wingman's formation/behaviour call",
          "mb_": "mission briefing", "gcs_combat_": "combat radio chatter", "gcs_dock": "docking radio (station/ATC)",
          "gcs_": "radio chatter"}


def words(t: str) -> list:
    return re.findall(r"[a-z0-9']+", t.lower())


def same_words(t: str) -> list:
    """Words as the conventions write them (Mr./Mister, all right/alright, hyphens): reads that differ only
    in what a style rule decides compare equal."""
    return [merge.key(w) for w in merge.conv_form(t).split() if merge.key(w)]


REP_GROUP = {"unclear": "rep_unclear", "guess": "rep_unclear", "tie": "rep_tie", "settled": "rep_settled"}


def script_half(ln, draft, names) -> str:
    """script: speech to text heard the script's words, or only misheard a name it could not know (and
    FLSubs agrees with the script, spelling variants aside); script_heard: the actor may have said other
    words (a sample of 100: 0 of 83 vs 7 of 17 needed a change)."""
    asr, fl = ln["asr"], ln["flsubs"]
    if not asr or words(asr) == words(draft):
        return "script"
    if merge.names_only(draft, asr, names) and (not fl or merge.names_only(draft, fl, names)):
        return "script"
    return "script_heard"


def group_of(ln, odd_names, repeats, names=None) -> str:
    lid, src = ln["id"].lower(), ln["source"]
    if src == "context: other voices" and ln["kind"] != "bank-orphan" and "scream" not in lid:
        return "context"
    if src.startswith("form:") and ln["kind"] != "bank-orphan" and "scream" not in lid:
        return "forms"
    squash = merge.squash
    rep = repeats.get((lid, ln["family"]))
    if rep:
        return REP_GROUP[rep["status"]]
    if ln["kind"] == "bank-orphan":
        return "orphans"
    if lid.startswith("mix_"):
        return "mixes"
    if "scream" in lid:
        return "screams"
    if ln["voice"] == "nnvoice":
        return "nnvoice"
    if src == "recording":
        return "recording"
    if GAMENAME.match(lid) and src in ("asr", "asr+flsubs", "flsubs"):
        return "gamenames"
    if any(re.sub(r"[^\w'-]", "", w) in odd_names for w in ln["draft"].split()):
        return "names"
    if src == "asr+flsubs":
        return "merged" if ln["flsubs"] and squash(ln["flsubs"]) != squash(ln["asr"]) else "agreed"
    if src == "asr":
        return "asr_fragments" if ln["fragment"] else "asr_sentences"
    if src in ("script", "judged: script", "judged: recording"):
        # where speech to text heard other words than the script, the actor often said them (a sample of
        # 100: 7 of 17 such lines needed a change, 0 of 83 where every source matched word for word)
        return script_half(ln, ln["draft"], names)
    return "other"


GROUP_KEYS = {k for k, _ in GROUPS}


class Data:
    def __init__(self, out: Path, corrections: Path, glossary: Path):
        self.out, self.corr_path = out, corrections
        tables = {}
        for name, cols in REQUIRED_COLUMNS.items():  # a file without a column the tool reads stops it clearly
            tables[name] = merge.tsv(out / name)
            tsvio.require_columns(out / name, tables[name], cols)
        self.names = merge.Names(tables["gamenames.tsv"], tsvio.read_text(glossary))
        files = {(r["voice"], r["id"]): (r["file"], r["kind"], r["sec"], r["text"])
                 for r in tables["index.tsv"]}
        aligned = {(r["voice"], r["id"]): r for r in tables["aligned.tsv"]}
        self.lines = []
        for i, r in enumerate(tables["draft.tsv"]):
            k = (r["voice"], r["id"])
            f, kind, sec, script_by_id = files.get(k, ("", "", "0", ""))
            al = aligned.get(k, {})
            self.lines.append({
                "n": i, "id": r["id"], "voice": r["voice"], "speaker": r["speaker"], "kind": kind,
                "sec": number(sec), "file": f, "draft": r["text"], "source": r["source"],
                "fragment": r["fragment"] == "1", "review": r["review"], "note": r["note"],
                "asr": r["speech to text"], "flsubs": r["FLSubs"],
                "script": al.get("text", "") if al.get("source") == "script" else "",
                "script_by_id": script_by_id,
            })
        # review groups (what may still need an ear), see GROUPS
        audit = out / "name_audit.tsv"
        odd_names = {r["word"] for r in merge.tsv(audit) if r["class"] in ("variant", "unknown")} if audit.exists() else set()
        rp = out / "repeats.tsv"
        self.repeats = {(r["id"].lower(), r["family"]): r for r in merge.tsv(rp)} if rp.exists() else {}
        for ln in self.lines:
            ln["family"] = merge.family(ln["voice"])
            ln["group"] = group_of(ln, odd_names, self.repeats, self.names)
        # the same recording (byte-identical clip) under several ids/voices: one entry, the copy in use
        # stands for the others (not an orphan, not a vanilla bank), a save covers every copy
        md5 = merge.audio_md5(out)
        units = {}
        for ln in self.lines:
            units.setdefault(md5.get((ln["voice"], ln["id"]), f"#{ln['n']}"), []).append(ln)
        rank = {"rep_unclear": 0, "rep_tie": 1, "rep_settled": 2}
        for members in units.values():
            members.sort(key=lambda x: (x["kind"] == "bank-orphan", "_vanilla" in x["voice"],
                                        rank.get(x["group"], 9), x["n"]))
            for x in members:
                x["copies"] = members
                if x is not members[0]:
                    x["group"] = "copy"
        # One decision for every clip that reads (or must read) the same, whichever NPC says it:
        # - a game-name piece (base, system, faction, good, ...): the same words in every voice, by id
        # - other chatter, the actor's settled takes included: the same text (letters and digits)
        squash = merge.squash
        self.batches = {}
        for ln in self.lines:
            if DX.match(ln["id"]):
                continue
            if ln["group"] == "gamenames":
                key = "id:" + ln["id"].lower()
            elif (ln["group"] in BATCHABLE or ln["group"] == "rep_settled") and squash(ln["draft"]):
                key = "text:" + squash(ln["draft"])
            else:
                continue
            self.batches.setdefault(key, []).append(ln)
        for key, members in list(self.batches.items()):
            if len(members) < 2:
                del self.batches[key]
                continue
            for x in members:
                x["batch"] = key
                if key.startswith("text:"):
                    x["group"] = "batch"
        # same line id spoken by other voices (context for chatter)
        self.by_id, self.by_voice = {}, {}
        for ln in self.lines:
            self.by_id.setdefault(ln["id"].lower(), []).append(ln)
            self.by_voice.setdefault(ln["voice"], []).append(ln)
        # scene neighbours: DX lines ordered by scene + line number
        self.scene = {}
        for ln in self.lines:
            m = DX.match(ln["id"])
            if m:
                self.scene.setdefault(m.group(1).upper(), []).append(ln)
        for v in self.scene.values():
            v.sort(key=lambda ln: sort_key(ln["id"]))
        self.load_corrections()
        self.undo, self.unbuilt = [], 0
        self.by_key = {(ln['id'].lower(), ln['voice'].lower()): ln for ln in self.lines}
        self.load_sampling()
        self.load_grammar(merge.GRAMMAR)  # game data (FLvibed\docs), not per corrections file
        self.rules = merge.load_name_rules(corrections.parent / "name_rules.tsv")
        self.style = merge.load_name_rules(corrections.parent / "style_rules.tsv")
        # A joined piece whose reads say the same words differs only in what the sentence rules decide
        # (capital at the start, end mark): nothing for the user to choose (user, session 2)
        # words the game writes itself (infocards, news, mission text), the DVD script uses or the user saved:
        # a read made only of such words has no mishearing like "toroids" or "Bonshu"
        src = ((out / "game_text.tsv").read_text(encoding="utf-8") if (out / "game_text.tsv").exists() else "") + \
            "\n".join(t for _, _, _, t in files.values()) + "\n".join(c[2] for c in self.corr.values() if c)
        self.known = {merge.key(x) for x in re.findall(r"[\w'’]+", re.sub(r"<[^>]+>", " ", src))}
        self.known |= self.names.vocab
        self.chatter = merge.load_chatter_rules(corrections.parent / "chatter_rules.tsv")
        kw = corrections.parent / "known_words.tsv"  # words Claude checked in their line (contextual pass)
        if kw.exists():
            self.known |= {merge.key(r["word"]) for r in merge.tsv(kw)}
        self.settled = {}
        for ln in self.lines:
            if ln["group"] in BATCHABLE | {"batch", "context", "forms"}:
                why = self.rule_settled(ln)
                if why:
                    self.settled[(ln["id"].lower(), ln["voice"].lower())] = why
        # out/rereview.tsv: lines whose saved choice conflicts with the unified conventions or other lines
        self.rereview, self.resaved, self.rereview_now = {}, set(), {}
        rr = out / "rereview.tsv"
        if rr.exists():
            for r in merge.tsv(rr):
                k = (r["id"].lower(), "*" if r["voice"] == "*" else merge.family(r["voice"]))
                self.rereview.setdefault(k, []).append(r["why"])
                self.rereview_now.setdefault(k, (r["you saved"], r["subtitle now"]))
        self.load_word_times()
        self.rank_suspects()
        self.load_story_order(story_order.OUT_FILE)  # game data (generated, committed), not per corrections file
        self.load_unsure()

    # ---------------------------------------------------------------- corrections file
    def load_corrections(self):
        self.header, self.corr = [], {}
        if self.corr_path.exists():
            for i, line in enumerate(tsvio.read_text(self.corr_path).splitlines(), 1):
                if line.startswith("#"):
                    self.header.append(line)
                elif line and not line.startswith("id\t"):
                    f = line.split("\t")
                    if len(f) > 7 or len(f) < 3 or not f[0]:
                        # kept as it is (written back unchanged), but said: a hand edit may have broken it
                        print(f"warning: {self.corr_path.name}:{i}: {len(f)} fields (id voice text [source note when "
                              f"by] expected)", file=sys.stderr)
                        tsvio.PROBLEMS.append((str(self.corr_path), i, f"{len(f)} fields"))
                    c = (f + [""] * 7)[:7] if len(f) <= 7 else f  # id voice text source note when by
                    self.corr[(c[0].lower(), c[1].lower())] = c
        self.index_reviewed_words()

    def index_reviewed_words(self):
        """(line id, words) of every reviewed generic chunk: decides the same chunk in other voices/takes."""
        self.reviewed_words = {(c[0].lower(), merge.squash(c[2])): c
                               for c in self.corr.values() if not DX.match(c[0])}

    def same_as_review(self, ln):
        """A generic chunk with the same line id and words as one you reviewed in another voice or take:
        your decision covers it (not written to the corrections file). Returns that correction or None."""
        if DX.match(ln["id"]) or self.line_kind(ln) not in ("chunk", "line"):
            return None
        return self.reviewed_words.get((ln["id"].lower(), merge.squash(ln["draft"])))

    def decided(self, ln):
        return (self.correction_for(ln) is not None or self.same_as_review(ln) is not None
                or (ln["id"].lower(), ln["voice"].lower()) in self.bulk
                or (ln["id"].lower(), ln["voice"].lower()) in getattr(self, "settled", ()))

    # ---------------------------------------------------------------- sampling a list (data/samples.tsv)
    # A list is checked on a random sample of its entries (~50 decisions); if the sample shows the drafts
    # are good, the rest of the list is accepted in bulk (data/bulk_accepted.tsv: not listened to, open to
    # a manual review at any time: a correction always wins). A sampled entry counts as wrong when the
    # saved words differ from the draft it had when sampled (punctuation and case aside).
    SAMPLE_SIZE = 50

    def load_sampling(self):
        self.samples, self.bulk = {}, {}
        sp, bp = self.corr_path.parent / "samples.tsv", self.corr_path.parent / "bulk_accepted.tsv"
        if sp.exists():
            for r in merge.tsv(sp):
                self.samples.setdefault(r["list"], []).append(r)
        if bp.exists():
            for r in merge.tsv(bp):
                self.bulk[(r["id"].lower(), r["voice"].lower())] = r
        # hand-picked lists (data/picks.tsv: id, voice, list): a few example lines Claude puts in front of
        # the user to settle a question about a bigger list; each stays until the user saves it
        # (column "context": the scene/situation written out for a line that needs it, shown on the line)
        self.picks, self.pick_context = {}, {}
        pp = self.corr_path.parent / "picks.tsv"
        if pp.exists():
            for r in merge.tsv(pp):
                ln = self.by_key.get((r["id"].lower(), r["voice"].lower()))
                if ln:
                    self.picks.setdefault(r["list"], []).append(ln)
                    if r.get("context"):
                        self.pick_context[(r["id"].lower(), r["voice"].lower())] = r["context"]

    def sample_rows(self, group):
        """The sample lines that belong to a list; a script-list sample counts for the half it fell in."""
        out = []
        for lst, rows in self.samples.items():
            for r in rows:
                ln = self.by_key.get((r["id"].lower(), r["voice"].lower()))
                if not ln:
                    continue
                g = lst
                if lst == "script":
                    g = script_half(ln, r["draft"], self.names)
                if g == group:
                    out.append((r, ln))
        return out

    def sample_status(self, group):
        rows = self.sample_rows(group)
        done = wrong = 0
        for r, ln in rows:
            c = self.correction_for(ln)
            if c:
                done += 1
                wrong += words(c[2]) != words(r["draft"])
        return {"size": len(rows), "done": done, "wrong": wrong}

    def make_sample(self, group, more=False):
        """A fixed random sample of the list's open entries (one per decision: batch, actor's takes)."""
        if group not in GROUP_KEYS or (self.sample_rows(group) and not more):
            return self.sample_status(group)
        entries = self.queue({"mode": group})
        rnd = random.Random(f"flsubtitles sample {group} {len(self.samples.get(group, []))}")  # another round: other lines
        pick = sorted(rnd.sample(entries, min(self.SAMPLE_SIZE, len(entries))))
        today = time.strftime("%Y-%m-%d")
        path = self.corr_path.parent / "samples.tsv"
        add = []
        if not path.exists():
            add.append("# Random samples of review lists (review tool). draft = the text the line had when sampled.\n")
            add.append("list\tid\tvoice\tdraft\tdate\n")
        for n in pick:
            ln = self.lines[n]
            add.append(f"{group}\t{ln['id']}\t{ln['voice']}\t{ln['draft']}\t{today}\n")
        with LOCK:
            append_atomic(path, "".join(add))
        for n in pick:
            ln = self.lines[n]
            self.samples.setdefault(group, []).append({"list": group, "id": ln["id"], "voice": ln["voice"],
                                                       "draft": ln["draft"], "date": today})
        return self.sample_status(group)

    def accept_rest(self, group):
        """Accept every open line of a sampled list as drafted (data/bulk_accepted.tsv)."""
        st = self.sample_status(group)
        if not st["size"] or st["done"] < st["size"]:
            return None
        rate = f"{st['wrong']}/{st['size']} wrong in the sample"
        today = time.strftime("%Y-%m-%d")
        path = self.corr_path.parent / "bulk_accepted.tsv"
        add, count = [], 0
        if not path.exists():
            add.append("# Lines accepted as drafted after a sample of their review list (not listened to; a\n"
                       "# correction overrides them; review them by hand any time). text = the draft accepted.\n")
            add.append("id\tvoice\ttext\tlist\tsample\tdate\n")
        accepted = {}
        for ln in self.lines:
            if ln["group"] == group and not self.decided(ln):
                members = self.batch_members(ln, show_done=True) if ln.get("batch") else [ln]
                for x in members:
                    k = (x["id"].lower(), x["voice"].lower())
                    if self.decided(x) or k in accepted:
                        continue
                    add.append(f"{x['id']}\t{x['voice']}\t{x['draft']}\t{group}\t{rate}\t{today}\n")
                    accepted[k] = {"text": x["draft"], "list": group}
                    count += 1
        with LOCK:
            append_atomic(path, "".join(add))
        self.bulk.update(accepted)  # only once the file holds them
        return {"accepted": count, "sample": rate}

    def rr_key(self, ln):
        """ln's Re-review entry: one per actor (its takes), or one for a line flagged in every voice."""
        for k in ((ln["id"].lower(), ln["family"]), (ln["id"].lower(), "*")):
            if k in self.rereview:
                return k
        return None

    def drop_rereview_note(self, lid: str):
        """A flagged line (data/rereview_notes.tsv) the reviewer has re-saved, changed or not, is settled."""
        path = self.corr_path.parent / "rereview_notes.tsv"
        if not path.exists():
            return
        lines = tsvio.read_text(path).splitlines()
        keep = [l for l in lines if l.startswith("#") or l.startswith("id\t") or l.split("\t")[0].lower() != lid.lower()]
        if len(keep) != len(lines):
            with LOCK:
                tsvio.atomic_write(path, "\n".join(keep) + "\n")

    def correction_for(self, ln):
        """The saved text for this clip, or for any byte-identical copy of it."""
        for x in ln.get("copies", [ln]):
            c = self.corr.get((x["id"].lower(), x["voice"].lower())) or self.corr.get((x["id"].lower(), "*"))
            if c:
                return c
        return None

    def takes(self, ln):
        """Every take of this line by the same actor (repeated lines), else just this one."""
        if DX.match(ln["id"]) or ln["id"].lower().startswith("0x"):
            return [ln]  # story lines: one text for every copy anyway; hash-named copies go with their twin
        # a take whose clip is also another line's recording (pilot_f_leg_m01b reuses its defendleader_02
        # clip as _03) says that other line: it is not a take of this one
        # and a take the recording says differently by more than a word or two is an intentional variant
        return [x for x in self.by_id.get(ln["id"].lower(), []) if x["family"] == ln["family"] and
                (x is ln or (all(c["id"].lower() in (x["id"].lower(),) or c["id"].lower().startswith("0x")
                                 for c in x.get("copies", [x])) and
                             merge.same_line(x["draft"], ln["draft"], x["sec"], ln["sec"])))]

    def save_correction(self, ln, text, source, note):
        # re-review: keeping the earlier text against the conventions is a decision: it is kept exactly
        now = self.rereview_now.get(self.rr_key(ln))
        if now and now[1] != now[0] and clean(text) == clean(now[0]) and not note:
            note = "kept exactly as typed"
        # story/mission lines (DX ids) are the same words in every copy; chatter differs per voice,
        # but one actor's takes of a repeated line read the same (user, session 1)
        if self.rr_key(ln):
            self.resaved.add(self.rr_key(ln))
        self.drop_rereview_note(ln["id"])
        when = datetime.now().isoformat(timespec="seconds")
        with LOCK:
            undo = self.undo[-1] if self.undo else {}
            for t in ([ln] if DX.match(ln["id"]) else self.takes(ln)):
                for x in t.get("copies", [t]):  # byte-identical recordings
                    voice = "*" if DX.match(x["id"]) else x["voice"]
                    for k in ((x["id"].lower(), x["voice"].lower()), (x["id"].lower(), voice.lower())):
                        undo.setdefault(k, self.corr.get(k))  # what this save replaces (None = nothing)
                    self.corr.pop((x["id"].lower(), x["voice"].lower()), None)
                    self.corr[(x["id"].lower(), voice.lower())] = [x["id"], voice, clean(text), source, clean(note), when, REVIEWER]
            self.write_corrections()

    def write_corrections(self):
        rows = sorted(self.corr.values(), key=lambda c: (c[0].lower(), c[1].lower()))
        head = self.header or ["# User-verified subtitle text. Highest priority in the merge."]
        # the when / by columns exist from the first save on; a row nobody dated keeps its old five fields
        body = "\n".join(head + ["id\tvoice\ttext\tsource\tnote\twhen\tby"] +
                         ["\t".join(c if c[5] or c[6] else c[:5]) for c in rows]) + "\n"
        tsvio.atomic_write(self.corr_path, body)  # temp file + replace: a crash never leaves half a file
        self.index_reviewed_words()

    def undo_last(self):
        """Puts back what the last save replaced (one save = one pick or typed text, batch included)."""
        with LOCK:
            if not self.undo:
                return None
            group = self.undo.pop()
            for k, old in group.items():
                if old is None:
                    self.corr.pop(k, None)
                else:
                    self.corr[k] = old
            self.write_corrections()
            return [k[0] for k in group]

    # ---------------------------------------------------------------- queue
    def options(self, ln):
        opts, seen = [], set()
        draft_label = {"script": "Draft (script)", "recording": "Draft (recording wording)",
                       "judged: script": "Draft (judged: script)", "judged: recording": "Draft (judged: recording)",
                       "asr+flsubs": "Draft (merged)", "asr": "Draft (speech to text)",
                       "correction": "Draft (your earlier pick)", "claude's guess": "Draft (Claude's guess)",
                       "majority of takes": "Draft (majority of takes)", "form: heard": "Draft (speech to text's form)",
                       "context: other voices": "Draft (matches other characters)"}.get(ln["source"], "Draft")
        rep = self.repeats.get((ln["id"].lower(), ln["family"]))
        heard = [(f"Take {v.strip()}", t) for v, _, t in
                 (p.partition(": ") for p in rep["takes"].split(" | "))] if rep else []
        # re-review: the text the conventions make of the saved one comes first (pick it to accept them)
        now = self.rereview_now.get(self.rr_key(ln)) if self.rr_key(ln) not in self.resaved else None
        if now and now[1] != now[0]:
            opts.append({"label": "Conventions applied", "text": clean(now[1])})
            opts.append({"label": "Your saved text (keep exactly)", "text": clean(now[0])})
            seen.update({clean(now[1]), clean(now[0])})
        for label, text in ((draft_label, ln["draft"]), ("Script", ln["script"]),
                            ("Script (by line id)", ln["script_by_id"]),
                            ("Speech to text", ln["asr"]), ("FLSubs", ln["flsubs"]), *heard):
            t = clean(text)
            if t and t not in seen:
                seen.add(t)
                opts.append({"label": label, "text": t})
            elif t:  # the same text from another source: say so on the option it joins
                for o in opts:
                    if o["text"] == t and label.split(" (")[0] not in o["label"]:
                        o["label"] += " = " + label.split(" (")[0]
        return opts

    def queue(self, q):
        mode, search = q.get("mode", "high"), q.get("q", "").lower().strip()
        show_done = q.get("done", "0") == "1"
        out, listed = [], set()
        if mode.startswith("pick:"):  # a hand-picked list, in its own order
            return [ln["n"] for ln in self.picks.get(mode[5:], [])
                    if show_done or self.correction_for(ln) is None]
        if mode in ("suspects", "unsure"):  # cross-list views: the aligner's suspects (most suspicious first),
            def found(ln):                  # the lines marked "not sure" (until the mark is cleared)
                return not search or search in ln["id"].lower() or search in ln["voice"].lower() \
                    or search in ln["draft"].lower()
            keyed = ((ln, (ln["id"].lower(), ln["voice"].lower())) for ln in self.lines)
            if mode == "unsure":
                return [ln["n"] for ln, k in keyed if k in self.unsure and found(ln)]
            out = [ln["n"] for ln, k in keyed if k in self.suspects and found(ln)
                   and (show_done or not self.suspect_done(ln))]
            key = lambda n: (-self.suspects[(self.lines[n]["id"].lower(), self.lines[n]["voice"].lower())][0], n)  # noqa: E731
            return sorted(out, key=key)
        if mode == "storyorder":  # every story line + scene mix in the order the game plays them (story_order.tsv)
            unrev = q.get("unrev", "0") == "1"
            for r in self.story_rows:
                n = r["n"]
                if n is None or n in listed:
                    continue
                listed.add(n)
                if n < 0:  # a movie line: "unreviewed" = its note asks for a listen ([check])
                    mv = self.movie[r["id"].lower()]
                    if unrev and not mv.get("note", "").startswith("[check]"):
                        continue
                    if search and search not in (mv.get("text", "") + " " + r["section"]).lower():
                        continue
                    out.append(n)
                    continue
                ln = self.lines[n]
                if unrev and (self.decided(ln) or ln["voice"] == "mixes"):
                    continue
                if search and search not in ln["id"].lower() and search not in ln["voice"].lower()                         and search not in ln["draft"].lower() and search not in r["section"].lower():
                    continue
                out.append(n)
            return out
        if mode.startswith("sample:"):  # a list's random sample, in its own order
            for r, ln in self.sample_rows(mode[7:]):
                if show_done or self.correction_for(ln) is None:
                    out.append(ln["n"])
            return out
        for ln in self.lines:
            done = self.decided(ln)
            if mode == "judged":  # Claude's unsure decisions (bulk-accepted), listed until you save them
                done = self.correction_for(ln) is not None
            if mode == "settled":  # lines the conventions decide, open to a spot-check
                done = self.correction_for(ln) is not None
            if mode == "rereview":  # reviewed lines the rules changed; each leaves the list once re-saved
                done = self.rr_key(ln) in self.resaved
            if done and not show_done:
                continue
            if mode in GROUP_KEYS and ln["group"] != mode:
                continue
            if mode == "story" and not (ln["kind"] == "story" or DX.match(ln["id"])):
                continue
            if mode == "settled" and (ln["id"].lower(), ln["voice"].lower()) not in self.settled:
                continue
            if mode == "judged" and not self.is_judged(ln):
                continue
            if mode == "rereview":
                if self.rr_key(ln) is None or self.rr_key(ln) in listed:
                    continue  # one entry per line id (a line can exist as a bank entry and a story sound)
                listed.add(self.rr_key(ln))
            if mode.startswith("rep_"):  # one entry per actor: the first take stands for all
                if (ln["id"].lower(), ln["family"]) in listed:
                    continue
                listed.add((ln["id"].lower(), ln["family"]))
            if mode in GROUP_KEYS and ln.get("batch"):  # one entry per batch: its first open clip stands for all
                if ln["batch"] in listed:
                    continue
                listed.add(ln["batch"])
            if mode == "chatter" and (ln["kind"] == "story" or DX.match(ln["id"])):
                continue
            if search and search not in ln["id"].lower() and search not in ln["voice"].lower() \
                    and search not in ln["draft"].lower():
                continue
            out.append(ln["n"])
        if mode == "story":
            out.sort(key=lambda n: (DX.match(self.lines[n]["id"]).group(1).upper() if DX.match(self.lines[n]["id"])
                                    else "~", sort_key(self.lines[n]["id"])))
        elif mode == "batch":  # biggest batches first: most clips settled per decision
            out.sort(key=lambda n: -len(self.batch_members(self.lines[n])))
        elif mode in GROUP_KEYS:  # the same line by different voices next to each other
            out.sort(key=lambda n: (self.lines[n]["id"].lower(), self.lines[n]["voice"]))
        return out

    def is_judged(self, ln):
        """A decision Claude was unsure of: a data/auto_decisions.tsv reason or a bulk_accepted.tsv sample
        note starting "[check]"."""
        b = self.bulk.get((ln["id"].lower(), ln["voice"].lower()))
        return (ln["source"].startswith("judged") and ln["note"].startswith("[check]")) or \
            bool(b and b.get("sample", "").startswith("[check]"))

    def batch_members(self, ln, show_done=False):
        """The clips of ln's same-text batch still to review (all of them with show_done)."""
        return [x for x in self.batches.get(ln.get("batch"), [])
                if show_done or x is ln or not self.decided(x)]

    def modes(self):
        """Menu entries with the number of lines still to review."""
        todo, seen = {k: 0 for k, _ in GROUPS}, set()
        for ln in self.lines:
            if ln["group"] in todo and not self.decided(ln):
                # counted per decision: per actor for repeated lines, per text for batches
                k = (ln["batch"] if ln.get("batch") else
                     (ln["id"].lower(), ln["family"]) if ln["group"].startswith("rep_") else ln["n"])
                if k in seen:
                    continue
                seen.add(k)
                todo[ln["group"]] += 1
        rr = len([k for k in self.rereview if k not in self.resaved])
        judged = sum(1 for ln in self.lines if self.is_judged(ln) and self.correction_for(ln) is None)
        groups = []
        for k, label in GROUPS:
            has = bool(self.sample_rows(k))
            groups.append({"mode": k, "label": label, "count": todo[k], "sample": self.sample_status(k)
                           if has else None, "can_sample": k in SAMPLEABLE})
            if has:
                st = self.sample_status(k)
                groups.append({"mode": "sample:" + k, "label": f"   ↳ sample: {st['done']}/{st['size']} checked, "
                               f"{st['wrong']} wrong", "count": st["size"] - st["done"]})
        later = {"screams", "mixes", "orphans", "gamenames"}
        for g in groups:
            g["section"] = "later" if g["mode"].split(":")[-1] in later else "review"
        return ([{"mode": "rereview", "label": "Re-review (your lines changed by rules)", "count": rr,
                  "section": "review"}] +
                [{"mode": "pick:" + k, "label": "Picked: " + k, "section": "review",
                  "count": sum(1 for ln in v if self.correction_for(ln) is None)} for k, v in self.picks.items()] +
                [{"mode": "judged", "label": "Claude was unsure: check these", "count": judged, "section": "review"},
                 {"mode": "suspects", "label": "Alignment suspects: text vs recording (most suspicious first)",
                  "count": sum(1 for ln in self.lines if (ln["id"].lower(), ln["voice"].lower()) in self.suspects
                               and not self.suspect_done(ln)), "section": "review"},
                 {"mode": "unsure", "label": "Marked not sure (X)", "count": len(self.unsure), "section": "review"}] +
                groups +
                [{"mode": "storyorder", "label": "Story order: dialogue and scene mixes as the game plays them",
                  "count": None, "section": "browse"},
                 {"mode": "story", "label": "Story & missions, in scene order", "count": None, "section": "browse"},
                 {"mode": "settled", "label": "Settled by the rules (spot-check)", "count": len(self.settled),
                  "section": "browse"},
                 {"mode": "all", "label": "All lines", "count": None, "section": "browse"}])

    # ---------------------------------------------------------------- word timings (the aligner) and suspects
    # FLSubtitles/aligner/align_words.py writes each line's word starts: out/word_times_cache.tsv (every row, the
    # suspects included; data/word_times.tsv is the shipped subset), out/word_times_failed.tsv (lines it could not
    # match). A row is current while its text hash (FNV-1a of the subtitle text) matches the line's text now.
    ALIGNED_SINCE = "2026-10-22"  # the aligner's first run: a save from then on was made with the timings there
    SPW_LOW, SPW_HIGH = 0.55, 2.5  # seconds per word against the corpus median (3+ words): text too long / short

    def load_word_times(self):
        self.word_times, self.align_failed = {}, set()
        for path in (self.out / "word_times_cache.tsv", self.corr_path.parent / "word_times.tsv"):
            if path.exists():
                for r in merge.tsv(path):
                    starts = []
                    for s in r.get("starts_ms", "").split(","):
                        try:
                            starts.append(int(s))
                        except ValueError:
                            starts.append(-1)
                    self.word_times[(r["id"].lower(), r["voice"].lower())] = {
                        "fnv": r.get("text_fnv", ""), "end_ms": int(number(r.get("end_ms"))), "starts": starts,
                        "collapsed": int(number(r.get("collapsed")))}
                break  # the cache holds every row; the shipped file only when there is no cache
        fp = self.out / "word_times_failed.tsv"
        if fp.exists():
            self.align_failed = {(r["id"].lower(), r["voice"].lower()) for r in merge.tsv(fp)}

    def word_info(self, ln):
        """The line's words with their start (ms; -1 = not placed) as the aligner timed the text it has now."""
        k = (ln["id"].lower(), ln["voice"].lower())
        words, row = ln["draft"].split(), self.word_times.get(k)
        if row is None:
            return {"state": "failed" if k in self.align_failed else "none", "end_ms": 0, "collapsed": 0,
                    "words": [{"w": w, "ms": -1} for w in words]}
        current = row["fnv"] == fnv1a(ln["draft"]) and len(row["starts"]) == len(words)
        return {"state": "current" if current else "stale", "end_ms": row["end_ms"], "collapsed": row["collapsed"],
                "words": [{"w": w, "ms": row["starts"][i] if current else -1} for i, w in enumerate(words)]}

    def rank_suspects(self):
        """Lines whose text may not be what the recording says, scored (most suspicious first): words the aligner
        piled at the clip's end or could not place, lines it could not match at all, a seconds-per-word far from
        the corpus median (text too long or too short for the clip); a single source (speech to text only) ranks
        higher. {(id, voice): (score, [reasons])}."""
        spw = sorted(ln["sec"] / len(ln["draft"].split()) for ln in self.lines
                     if ln["sec"] > 0 and len(ln["draft"].split()) >= 3)
        self.spw_median = spw[len(spw) // 2] if spw else 0
        self.suspects = {}
        for ln in self.lines:
            n = len(ln["draft"].split())
            if not n or ln["id"].lower().startswith(("0x", "mix_")):
                continue
            k = (ln["id"].lower(), ln["voice"].lower())
            score, why = 0.0, []
            if k in self.align_failed:
                score += 6
                why.append("the aligner could not match these words to the recording")
            info = self.word_info(ln)
            if info["state"] == "current" and info["collapsed"]:
                col = info["collapsed"]
                score += (2 if col >= 2 else 0) + 4 * col / n
                why.append(f"{col} of {n} words piled at the clip's end or not placed")
            if n >= 3 and ln["sec"] > 0 and self.spw_median:
                r = ln["sec"] / n / self.spw_median
                if r < self.SPW_LOW or r > self.SPW_HIGH:
                    score += 1.5
                    why.append(f"{ln['sec'] / n:.2f} s per word (median {self.spw_median:.2f}): the text seems "
                               + ("long" if r < 1 else "short") + " for the clip")
            if score and ln["source"] in ("asr", "asr-only"):
                score += 0.5
                why.append("one source only (speech to text)")
            if score >= 1.5:
                self.suspects[k] = (round(score, 2), why)

    def suspect_done(self, ln):
        """Listened to since the timings exist: a save in the tool dated from the aligner's first run on."""
        c = self.correction_for(ln)
        return bool(c and c[5] >= self.ALIGNED_SINCE)

    # ---------------------------------------------------------------- "not sure" flags (data/unsure.tsv)
    def load_unsure(self):
        self.unsure_path = self.corr_path.parent / "unsure.tsv"
        self.unsure = {}
        if self.unsure_path.exists():
            for r in merge.tsv(self.unsure_path):
                self.unsure[(r["id"].lower(), r["voice"].lower())] = r

    def set_unsure(self, ln, on: bool, note: str = ""):
        """Marks a line "not sure" (a second listen, or a question for later), or clears the mark."""
        k = (ln["id"].lower(), ln["voice"].lower())
        with LOCK:
            if on:
                self.unsure[k] = {"id": ln["id"], "voice": ln["voice"], "note": clean(note),
                                  "when": datetime.now().isoformat(timespec="seconds"), "by": REVIEWER}
            else:
                self.unsure.pop(k, None)
            rows = sorted(self.unsure.values(), key=lambda r: (r["id"].lower(), r["voice"].lower()))
            tsvio.atomic_write(self.unsure_path, "# Lines marked \"not sure\" in the review tool (X): to hear again or "
                               "decide later. Cleared from the tool.\nid\tvoice\tnote\twhen\tby\n" +
                               "".join(f"{r['id']}\t{r['voice']}\t{r['note']}\t{r['when']}\t{r['by']}\n" for r in rows))

    # ---------------------------------------------------------------- story order (data/story_order.tsv)
    # The order the story's dialogue plays in the game, derived from the game's data by story_order.py (missions in
    # campaign order, each mission's trigger graph, cutscene / room scene lines by time; branches marked; lines it
    # could not place last, with the reason). The view walks the lines in that order.
    def load_story_order(self, path: Path):
        self.story_rows, self.story_at, self.story_file, self.movie = [], {}, path, {}
        # the intro movie's lines (data/movies.tsv): shown in place, edited in that file (no clip of their own)
        mpath = path.parent / "movies.tsv"
        for r, mid in story_order.movie_rows(mpath):  # keyed by movie + text hash: a retiming keeps the id
            self.movie[mid.lower()] = r
        if not path.exists():
            return
        by_id = {}
        for ln in self.lines:
            by_id.setdefault(ln["id"].lower(), []).append(ln)
        for r in tsvio.read_tsv(path):
            cands = by_id.get(r.get("id", "").lower(), [])
            ln = next((x for x in cands if x["kind"] == "story" or x["voice"] == "mixes"), cands[0] if cands else None)
            row = {k: r.get(k, "") for k in ("order", "mission", "section", "id", "play", "at", "via", "source")}
            row["n"] = ln["n"] if ln else None
            if ln is None and row["id"].lower() in self.movie:  # a movie line: a negative item number
                row["n"] = -(len(self.story_rows) + 1)
            self.story_rows.append(row)
            if row["n"] is not None:
                self.story_at.setdefault(row["n"], len(self.story_rows) - 1)

    def story_headings(self):
        """Mission / scene headings of the story order: [{label, i (row index), n (its first line)}]."""
        out, last = [], None
        for i, r in enumerate(self.story_rows):
            if r["n"] is None:
                continue
            head = (r["mission"], r["section"])
            if head != last:
                last = head
                label = "Not placed" if r["mission"] == story_order.NOT_PLACED else f"{r['mission']} · {r['section']}"
                if not out or out[-1]["label"] != label:
                    out.append({"label": label, "i": i, "n": r["n"]})
        return out

    def story_info(self, n):
        """The line's place in the story order with the lines around it (None if it has none)."""
        i = self.story_at.get(n)
        if i is None:
            return None
        r = self.story_rows[i]
        near = []
        for j in range(max(0, i - 4), min(len(self.story_rows), i + 5)):
            x = self.story_rows[j]
            if x["n"] is None or x["mission"] != r["mission"] or x["section"] != r["section"] and x["n"] < 0:
                continue
            if x["n"] < 0:
                mv = self.movie[x["id"].lower()]
                near.append({"n": x["n"], "speaker": mv.get("speaker", ""), "id": x["id"], "text": mv.get("text", ""),
                             "section": x["section"], "play": x["play"], "current": j == i})
                continue
            ln = self.lines[x["n"]]
            cur = self.correction_for(ln)
            mix = ln["voice"] == "mixes"
            near.append({"n": x["n"], "speaker": ln["speaker"] or ln["voice"], "id": ln["id"],
                         "text": "(scene mix: no subtitles yet)" if mix else (cur[2] if cur else ln["draft"]),
                         "section": x["section"], "play": x["play"], "current": j == i})
        return {**r, "index": i, "rows": len(self.story_rows), "near": near,
                "mix": n >= 0 and self.lines[n]["voice"] == "mixes", "placed": r["mission"] != story_order.NOT_PLACED}

    def movie_item(self, n):
        """A movie line of the story order (read-only here: data/movies.tsv holds it, on the movie clock)."""
        r = self.story_rows[self.story_at[n]]
        mv = self.movie[r["id"].lower()]
        return {"n": n, "movie": {k: mv.get(k, "") for k in ("movie", "start", "end", "speaker", "text", "note")},
                "story": self.story_info(n)}

    def stats(self):
        """Coverage at a glance: lines, how they were decided, open entries per list, input problems."""
        kinds, how = {}, {"saved by you": 0, "accepted after a sample": 0, "settled by the rules": 0,
                          "same words as a review": 0, "draft, no decision recorded": 0}
        for ln in self.lines:
            k = ln["kind"] or "?"
            kinds[k] = kinds.get(k, 0) + 1
            key = (ln["id"].lower(), ln["voice"].lower())
            if self.correction_for(ln) is not None:
                how["saved by you"] += 1
            elif key in self.bulk:
                how["accepted after a sample"] += 1
            elif key in self.settled:
                how["settled by the rules"] += 1
            elif self.same_as_review(ln) is not None:
                how["same words as a review"] += 1
            else:
                how["draft, no decision recorded"] += 1
        timing = {}
        for ln in self.lines:
            s = self.word_info(ln)["state"]
            timing[s] = timing.get(s, 0) + 1
        return {"lines": len(self.lines), "reviewed": len(self.corr), "kinds": kinds, "decided": how,
                "word_timings": timing, "suspects": len(self.suspects), "unsure": len(self.unsure),
                "lists": {m["mode"]: m["count"] for m in self.modes() if m.get("count") is not None},
                "unbuilt_saves": self.unbuilt, "input_problems": [list(p) for p in tsvio.PROBLEMS[:50]]}

    def item(self, n):
        if n < 0:
            return self.movie_item(n)
        ln = self.lines[n]
        ctx = []
        m = DX.match(ln["id"])
        if m:
            sc = self.scene[m.group(1).upper()]
            ids = [x["id"].upper() for x in sc]
            i = ids.index(ln["id"].upper())
            seen = set()
            for x in sc[max(0, i - 3):i + 4]:
                if x["id"].upper() in seen:
                    continue
                seen.add(x["id"].upper())
                cur = self.correction_for(x)
                ctx.append({"id": x["id"], "speaker": x["speaker"], "text": cur[2] if cur else x["draft"],
                            "current": x["id"].upper() == ln["id"].upper()})
        else:  # chatter: the same line as the other voices say it
            for x in self.by_id.get(ln["id"].lower(), [])[:12]:
                cur = self.correction_for(x)
                ctx.append({"id": x["id"], "speaker": x["voice"], "text": cur[2] if cur else x["draft"],
                            "current": x is ln})
        cur = self.correction_for(ln)
        note = " · ".join(self.rereview.get(self.rr_key(ln), [])) or ln["note"]
        same = self.same_as_review(ln) if self.correction_for(ln) is None else None
        why = self.settled.get((ln["id"].lower(), ln["voice"].lower()))
        if why and cur is None:
            note = f"settled by the rules: {why} · pick or type to override"
        if same:
            note = f"decided by your review of the same line ({same[1]}): {same[2]} · pick or type to override"
        rep = self.repeats.get((ln["id"].lower(), ln["family"]))
        if rep:
            n_takes = len(self.takes(ln))
            note = f"{rep['why']} · saving applies to all {n_takes} takes of {ln['family']}"
        copies = [{"n": x["n"], "id": x["id"], "voice": x["voice"]} for x in ln.get("copies", []) if x is not ln]
        batch = []
        if ln.get("batch"):
            for x in self.batch_members(ln):
                batch.append({"n": x["n"], "id": x["id"], "voice": x["voice"], "sec": x["sec"], "draft": x["draft"],
                              "current": x is ln})
        return {**{k: ln[k] for k in ("n", "id", "voice", "speaker", "kind", "sec", "source", "fragment",
                                      "review", "draft")}, "note": note, "copies": copies, "batch": batch,
                "options": self.options(ln), "context": ctx, "sentence": self.sentence(ln),
                "line_kind": self.line_kind(ln),
                "scene": self.pick_context.get((ln["id"].lower(), ln["voice"].lower()), "") or self.auto_context(ln),
                "saved": cur[2] if cur else None,
                "saved_source": cur[3] if cur else None,
                "words": self.word_info(ln),
                "suspect": self.suspects.get((ln["id"].lower(), ln["voice"].lower())),
                "unsure": self.unsure.get((ln["id"].lower(), ln["voice"].lower())),
                "story": self.story_info(n)}

    # ---------------------------------------------------------------- chatter fragment in a sentence
    # Slots of data/chatter_grammar.tsv: the line ids that can fill them
    SLOTS = {"^base-": r"gcs_refer_base_.*-", "^dest-": r"gcs_refer_(base|system)_.*-", "^good": r"gcs_gen_commodity_.*",
             "^ship": r"gcs_refer_shiparch_.*", "^number": r"gcs_misc_number_\d+", "^number-": r"gcs_misc_number_\d+-",
             "@mbid": r"gcs_dhail_mbid_.*"}
    NAME_PARTS = {"faction": r"gcs_refer_faction_.*_short", "desig": r"gcs_refer_formationdesig_\d+",
                  "number": r"gcs_misc_number_\d+", "number-": r"gcs_misc_number_\d+-", "dash": r"gcs_misc_dash"}
    PREFER = [r"gcs_refer_base_li01_01_base-", r"gcs_refer_formationdesig_0?1", r"gcs_misc_number_1-?",
              r"gcs_gen_commodity_gold"]

    # Base and bar speech scenes (DATA\Scripts\GCS\*.thn) that join two pieces (merge.BASE_ROLES)
    BASE_SCENES = [
        ("GCS W02aF", [r"GCS_(ML|FM)_W02aF_.*", r"GCS_(ML|FM)_W02bF\d+"], "a base NPC introduces the base"),
        ("GCS A03aG+A04aG", [r"GCS_(ML|FM)_A03aG_.*_grp", r"GCS_(ML|FM)_A04aG\d+"],
         "a bar NPC turns Trent away over his reputation with a faction"),
        ("GCS D06aG", [r"GCS_(ML|FM)_D06aG00", r"GCS_(ML|FM)_D06aG_.*_grp"], "a bar NPC says who they work for"),
    ]

    def load_grammar(self, path: Path):
        self.grammar = []
        if path.exists():
            for r in merge.tsv(path):
                self.grammar.append((r["builder"], r["template"].split(), r["situation"]))
        self.grammar += self.BASE_SCENES
        self.grammar_roles = merge.load_grammar_roles(path)

    QUESTION = re.compile(r"(?i)^(what|who|whom|whose|where|when|why|how|is|are|am|was|were|do|does|did|can|could|"
                          r"will|would|shall|should|have|has|had|may|might|must|got|need|want|any|anything|"
                          r"something|you|your|first|new|name|back)\b")

    def rule_settled(self, ln):
        """Why the conventions already decide this line, or None. Two independent reads (speech to text and
        FLSubs; the draft is made from them) must say the same words, and every difference between them and
        the draft must be one a rule decides (user, session 2: "don't ask what a rule already answers"):
        - commas (they follow speech to text), capitals (FLSubs / game names), Mr./Mister, all right, hyphens;
        - a joined piece: its start capital and end mark, a trailing "..." (the game joins and ends it);
        - a FLSubs "?" on a line built as a question ("Hazardous?", "We've met, correct?", "Is it…").
        A "!" against "." (how the actor sounded) is not a rule and stays for the user."""
        a, f, d = ln["asr"], ln["flsubs"], ln["draft"]
        if not d:
            return None
        chunk = self.line_kind(ln) == "chunk"
        # a piece that names a game object: the game's own name for it wins (user's policy, session 1)
        game = [w.lower() for w in self.names.for_id(ln["id"])]
        if merge.WELCOME.match(ln["id"]):  # "Welcome to <base>,": the base the id names, as the game writes it
            return "a base intro: the base's name comes from the game files (by the line id)"
        if game and chunk and all(w in d.lower() for w in game):
            return f"the game's own name for this piece ({' '.join(self.names.for_id(ln['id']))})"
        # every word where the draft and the (rule-applied) speech-to-text read differ is a game name on the
        # draft's side ("Welcome to Bruchsal" / "Broxson", "the Bundschuh" / "Bunchu"): game names win
        if a:
            dw, aw = same_words(d), same_words(self.ruled(ln, a))
            ops = [o for o in difflib.SequenceMatcher(a=dw, b=aw, autojunk=False).get_opcodes() if o[0] != "equal"]
            small = {"the", "a", "an", "of", "and"}
            vocab = self.names.vocab
            # the draft's side: names (an article may come along); speech to text's side: no other real name
            # ("Police" for the game's "Military" is the actor's own word, not a misspelling). One actor's
            # "Bretonia Mining and Minerals" is an outlier the user saved by hand, not a rule (user, session 2)
            # (a chunk's dropped article alone, "the ALG" / "A L G", is the piece's template word)
            if ops and any(w in vocab for w in dw) and all(
                    (dw[i1:i2] and all(w in vocab or w in small for w in dw[i1:i2]) and
                     not any(w in vocab for w in aw[j1:j2])) or
                    (chunk and all(w in small for w in dw[i1:i2] + aw[j1:j2])) for _, i1, i2, j1, j2 in ops):
                return "the game's spelling of a name speech to text misheard (game names win)"
            if not ops and chunk and any(w in vocab for w in dw):
                return "speech to text heard the game's own name exactly"
        if re.match(r"(?i)gcs_misc_dash", ln["id"]):
            return "the spoken \"dash\" of a callsign: the sentence rules write it as \"-\" (Alpha 1-2)"
        # two or more separate recordings (other voices, or one actor's separate takes) whose own speech-to-
        # text read, or the user's saved text, is exactly these words, all of them known words (the game's own
        # text, the script, the user's saves, known_words.tsv): the words are settled; the rules set the rest
        if all(w in self.known or w.isdigit() for w in same_words(d)):
            target = same_words(d)
            recs = set()
            for x in self.batches.get(ln.get("batch"), []) + self.by_id.get(ln["id"].lower(), []):
                c = self.correction_for(x)
                if same_words(self.ruled(x, x["asr"])) == target or (c and same_words(c[2]) == target):
                    recs.add(x.get("copies", [x])[0]["n"])  # byte-identical copies are one recording
            if len(recs) >= 2:
                return f"{len(recs)} separate recordings read exactly these words, all of them known words"
        if not (a and f):
            return None

        def norm(t, end=True):
            t = merge.conv_form(t)
            t = re.sub(r"\bmr\b\.?|\bmister\b", "mister", t)
            # a trailing "..." is the game's to drop on a joined piece; on a whole line it is a choice
            t = re.sub(r"(\.\.\.|…)", "." if chunk else "…", t)
            toks = re.findall(r"[a-z0-9']+|[.!?…]+", t)  # a run of marks stays one token ("?!" is not "?")
            while toks and not re.match(r"[a-z0-9']", toks[-1]) and (chunk or not end):
                toks.pop()
            return toks
        if not norm(a) or len({tuple(norm(t)) for t in (a, f, d)}) == 1:
            if len({tuple(norm(t)) for t in (a, f, d)}) == 1:
                return "a joined piece: start capital, end mark and commas are set by the sentence rules" if chunk \
                    else "the reads differ only in commas, capitals or spelling the rules set"
            return None
        # the only difference is the final mark: FLSubs' "?" on a question
        if len({tuple(norm(t, end=False)) for t in (a, f, d)}) == 1 and f.rstrip().endswith("?") \
                and d.rstrip().endswith("?"):
            last = re.split(r"(?<=[.!?])\s+", d.strip())[-1]
            if self.QUESTION.match(last) or len(words(last)) <= 2 or re.search(r",\s*\w+\?$", last):
                return "a question: FLSubs' \"?\" kept (the rules keep FLSubs' marks)"
        return None

    def ruled(self, ln, text):
        """A raw read with the user's rules applied (names, chatter mishearings, spelling), as merge.py does."""
        t = re.sub(r"\b([A-Z])\.? (?=[A-Z]\b)", r"\1", text)  # spelled out: "A L G" -> "ALG"
        t = re.sub(r"\b([A-Z]{2,})\.? ([A-Z])\b", r"\1\2", t)
        t = re.sub(r"\bEdison\.? (?=Trent\b)", "", t)  # the glossary hint read back before "Trent"
        t = merge.apply_name_rules(t, self.rules)
        if not DX.match(ln["id"]):
            for ids, rx, rep, note, voices in self.chatter:
                if ids.search(ln["id"]) and (voices is None or voices.fullmatch(ln["voice"])):
                    t = merge.apply_style_rules(t, [(rx, rep, note)])
        return merge.apply_style_rules(t, self.style)

    def auto_context(self, ln):
        """A light, generated context: who speaks in what situation (base scene code, line id words), the
        other voices' versions of the same line, and for story lines the lines around it."""
        lid = re.sub(r"^\$B", "", ln["id"])
        parts = []
        m = re.match(r"(?i)GCS_(ML|FM|Trent)_([A-Z]\d\d[a-z])", lid)
        if m:
            who = WHO.get(m.group(1), m.group(1))
            parts.append(f"Base/bar conversation ({who}): {SCENE.get(m.group(2), 'scene ' + m.group(2))}.")
        elif DX.match(lid):
            sc = self.scene.get(DX.match(lid).group(1).upper(), [])
            i = next((k for k, x in enumerate(sc) if x is ln), None)
            if i is not None:
                near = [x for x in sc[max(0, i - 2):i + 3] if x is not ln]
                said = lambda x: (self.correction_for(x) or (0, 0, x["draft"]))[2]  # noqa: E731
                before = " / ".join(f"{x['voice']}: \"{said(x)}\"" for x in sc[max(0, i - 2):i])
                after = " / ".join(f"{x['voice']}: \"{said(x)}\"" for x in sc[i + 1:i + 3])
                parts.append(f"Story scene {DX.match(lid).group(1).upper()}."
                             + (f" Before: {before}." if before else "") + (f" After: {after}." if after else ""))
        else:
            what = next((v for k, v in PREFIX.items() if lid.lower().startswith(k)), "")
            topic = re.sub(r"^(gcs_|rmb_|rms_|nag_|bc_|mb_)", "", lid.lower()).strip("-_")
            topic = re.sub(r"_?\d+[-+]?$", "", topic).strip("+-").replace("_", " ")
            if what or topic:
                s = f"{what}{': ' if what else ''}{topic}."
                parts.append(s[:1].upper() + s[1:])
            for s in self.sentence(ln)[:1]:
                parts.append(f"The game joins it as: \"{s['rendered']}\" ({s['situation']}).")
        # what other voices say for the same line id (reviewed text first)
        sib = [x for x in self.by_id.get(ln["id"].lower(), []) if x["family"] != ln["family"]]
        seen, alts = set(), []
        for x in sorted(sib, key=lambda x: self.correction_for(x) is None):
            t = (self.correction_for(x) or (0, 0, x["draft"]))[2]
            if t and t.lower() not in seen:
                seen.add(t.lower())
                alts.append(f"\"{t}\"")
        if alts:
            parts.append("Other voices say: " + " / ".join(alts[:6]) + ("…" if len(alts) > 6 else "") + ".")
        return " ".join(parts)

    def line_kind(self, ln):
        """story: a whole subtitle, kept as written (story, campaign and unique mission dialogue, and
        their vanilla copies); nnvoice: the Neural Net's whole messages; chunk: a generic piece the
        runtime joins into a sentence and types out (user, session 2)."""
        if any(DX.match(x["id"]) for x in ln.get("copies", [ln])):
            return "story"
        if ln["voice"] == "nnvoice":
            return "nnvoice"
        # generic radio chatter, random-mission taunts, names, base/bar speech, Trent's bar replies;
        # mission characters' own lines (nag_, bc_, rmb_, mb_ ...) are unique dialogue, kept as written
        if not merge.GENERIC_LINE.match(ln["id"]):
            return "story"
        # a generic piece the game joins with others, or a generic line that is a whole sentence itself
        return "chunk" if merge.is_joined(ln["id"], self.grammar_roles) else "line"

    def pick(self, voice, rx, ln=None):
        """The current line if it fits rx, else this voice's preferred / first line whose id fits rx."""
        if ln is not None and re.fullmatch(rx, ln["id"], re.I):
            return ln
        alt = re.search(r"\(([\w|]+)\)(?!\?)", rx)  # "(todock|tomoor|toland)": the first one the voice has
        if alt:
            for a in alt.group(1).split("|"):
                x = self.pick(voice, rx[:alt.start()] + a + rx[alt.end():])
                if x:
                    return x
            return None
        lines = [x for x in self.by_voice.get(voice, []) if re.fullmatch(rx, x["id"], re.I)]
        if not lines:
            return None
        faction = {"ill": r"gcs_refer_faction_fc_lr_short", "mil": r"gcs_refer_faction_li_n_short",
                   "leg": r"gcs_refer_faction_li_n_short"}
        prefer = list(self.PREFER) + [faction[k] for k in faction if f"_{k}_" in voice]
        for p in prefer:
            for x in lines:
                if re.fullmatch(p, x["id"], re.I):
                    return x
        return lines[0]

    def fill(self, tok, voice, ln):
        """Lines for one grammar token (a ship name slot gives several), or None if the voice lacks them."""
        if tok in ("@full", "@full:", "@short"):
            parts = (["faction"] if tok != "@full:" else []) + ["desig"] +                 (["number", "dash", "number"] if tok != "@short" else ["number-"])
            out = []
            for i, part in enumerate(parts):
                rx = self.NAME_PARTS[part]
                if part == "number" and i == len(parts) - 1:  # second number of "1-2"
                    rx = r"gcs_misc_number_2"
                x = self.pick(voice, rx, ln)
                if x is None and part == "faction":
                    continue
                if x is None:
                    return None
                out.append(x)
            return out
        rx = self.SLOTS.get(tok, tok)
        x = self.pick(voice, rx, ln)
        return [x] if x else None

    def sentence(self, ln):
        """The radio sentences the game builds around this chatter line (data/chatter_grammar.tsv, read
        from Content.dll), made of the same voice's own clips, so a piece can be heard where the game
        puts it. Returns up to three [{situation, pieces}]."""
        lid, v = ln["id"], ln["voice"]
        if DX.match(lid) or lid.lower().startswith(("0x", "mix_")):
            return []
        name_rx = "|".join(f"(?:{r})" for r in self.NAME_PARTS.values())
        out = []
        for builder, toks, situation in self.grammar:
            def fits(t):
                if t in ("@full", "@full:", "@short"):
                    return re.fullmatch(name_rx, lid, re.I)
                return re.fullmatch(self.SLOTS.get(t, t), lid, re.I)
            if not any(fits(t) for t in toks):
                continue
            pieces, ends = [], set()
            for i, t in enumerate(toks):
                got = self.fill(t, v, ln)
                if got is None:
                    pieces = None
                    break
                pieces += got
                # a sentence ends after the last piece and after a "-" piece (or a name ending in one) that a
                # new sentence follows, not after one a name completes ("...headed to" + Fuchu Prison). An
                # @short ends in its number- piece (id gcs_misc_number_N-), so it closes like any "-" piece, as
                # JoinPieces (C++) decides by the id. The "to" rule (no period before a piece starting with
                # "to") is applied by merge.render_sentence.
                closes = t.endswith("-") or t == "@short"
                if i == len(toks) - 1 or (closes and not toks[i + 1].startswith(("@", "^"))):
                    ends.add(len(pieces) - 1)
            if not pieces or len(pieces) < 2 or not any(x is ln for x in pieces):
                continue
            items = []
            for k, x in enumerate(pieces):
                c = self.correction_for(x)
                items.append({"n": x["n"], "id": x["id"], "text": c[2] if c else x["draft"], "current": x is ln,
                              "ends": k in ends})
            typed = merge.render_sentence([(x["text"], x["ends"]) for x in items])
            out.append({"situation": situation, "builder": builder, "pieces": items, "rendered": typed})
            if len(out) == 3:
                break
        return out

    # ---------------------------------------------------------------- fitting typed text
    def fit(self, ln, typed: str):
        """Typed text, adjusted to the conventions; returns (text, [changes])."""
        text, changes = clean(typed), []
        # names: the script's own names for this line, the game's names for this fragment, glossary/game vocab
        # Only real names: capitalized mid-sentence in the script, or known to the game/glossary
        # (a sentence start like "Wish us luck" is not a name).
        script = (ln["script"] or ln["script_by_id"]).split()
        ref = [re.sub(r"('s)?[^\w'-]*$", "", t) for i, t in enumerate(script)
               if t[:1].isupper() and merge.key(t) not in merge.COMMON and
               (merge.key(t) in self.names.vocab or (i > 0 and not re.search(r"[.!?…]$", script[i - 1])))]
        ref += self.names.for_id(ln["id"])
        out = []
        for t in text.split():
            core = re.sub(r"('s)?[^\w'-]*$", "", t)
            new = t
            # respell names only: the typed word is capitalized or the reference is a known name
            if len(merge.key(core)) >= 4:
                ref_here = [n for n in ref if core[:1].isupper() or merge.key(n) in self.names.vocab]
                for n in ref_here:
                    n = re.sub(r"('s)?[^\w'-]*$", "", n)
                    if merge.key(core) != merge.key(n) and merge.name_match(core, n):
                        new = n + t[len(core):]  # "Rhineland" -> "Rheinland"
                        break
            if new != t:
                changes.append(f"{t} → {new}")
            out.append(new)
        text = " ".join(out)
        new = merge.apply_name_rules(text, self.rules)  # global rules: "Doyle" -> "Sam", "Dom'Kavash"
        if new != text:
            changes.append("global name rules (data/name_rules.tsv)")
            text = new
        new = merge.apply_style_rules(text, self.style)  # conventions: "Dammit" -> "Damn it", "..."
        if new != text:
            changes.append("spelling/punctuation conventions (data/style_rules.tsv)")
            text = new
        # callsigns: "Alpha Two" -> "Alpha 2" (the script writes callsign numbers as digits)
        def digits(m):
            return f"{m.group(1)} {merge.NUMWORD[m.group(2).lower()]}"
        desig = "|".join(w for n in range(1, 30) for w in self.names.for_id(f"gcs_refer_formationdesig_{n}"))
        if desig:
            new = re.sub(rf"\b({desig}|Patrol [A-Z]\w+)\s+({'|'.join(merge.NUMWORD)})\b", digits, text,
                         flags=re.I)
            if new != text:
                changes.append("callsign numbers as digits")
                text = new
        if ln["fragment"]:
            new = merge.fragment_form(ln["id"], text, self.names)
            if new != text:
                changes.append("fragment form (no end punctuation, lowercase start unless a name)")
                text = new
        if self.line_kind(ln) in ("chunk", "line"):  # its place in the game's sentences (merge.apply_grammar)
            new = merge.apply_grammar(ln["id"], text, self.grammar_roles, self.names)
            if new != text:
                changes.append("chunk: no sentence-ending period (the game sentence adds it), case from its place")
                text = new
        return text, changes


class Handler(BaseHTTPRequestHandler):
    data: Data = None

    def log_message(self, *args):
        pass

    def send(self, code, body: bytes, ctype: str):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def js(self, obj, code=200):
        self.send(code, json.dumps(obj, ensure_ascii=False).encode("utf-8"), "application/json; charset=utf-8")

    def guarded(self, fn):
        """One request's handler: a bad request (unknown line number, missing field) answers 400, a fault in the
        tool 500 with its message - the server keeps running either way; a page that went away is ignored."""
        try:
            fn()
        except (ConnectionAbortedError, ConnectionResetError, BrokenPipeError):
            pass
        except (KeyError, ValueError, IndexError, TypeError) as e:
            with contextlib.suppress(OSError):
                self.js({"error": f"bad request: {type(e).__name__}: {e}"}, 400)
        except Exception as e:  # noqa: BLE001 - reported, never fatal for the server
            traceback.print_exc()
            with contextlib.suppress(OSError):
                self.js({"error": f"{type(e).__name__}: {e}"}, 500)

    def do_GET(self):
        self.guarded(self.get)

    def do_POST(self):
        self.guarded(self.post)

    def get(self):
        u = urlparse(self.path)
        q = {k: v[0] for k, v in parse_qs(u.query).items()}
        d = self.data
        if u.path == "/":
            self.send(200, (HERE / "review.html").read_bytes(), "text/html; charset=utf-8")
        elif u.path == "/api/modes":
            self.js(d.modes())
        elif u.path == "/api/stats":
            self.js(d.stats())
        elif u.path == "/api/queue":
            items = d.queue(q)
            self.js({"items": items, "total": len(d.lines), "reviewed": len(d.corr),
                     **({"story_index": [d.story_at.get(n) for n in items], "headings": d.story_headings(),
                         "story_file": d.story_file.exists()} if q.get("mode") == "storyorder" else {})})
        elif u.path == "/api/item":
            self.js(d.item(int(q["n"])))
        elif u.path.startswith("/audio/"):
            if getattr(Handler.args, "no_audio", False):  # tests: the page loads, nothing can play
                return self.send(404, b"audio off (--no-audio)", "text/plain")
            ln = d.lines[int(unquote(u.path[7:]))]
            f = Path(ln["file"])
            if not f.is_absolute():  # older indexes stored bank clips relative to this folder
                f = (HERE / f).resolve()
            if not f.exists():
                return self.send(404, f"clip not found: {f}".encode(), "text/plain")
            body, ctype = audio(f)
            self.send(200, body, ctype)
        else:
            self.send(404, b"not found", "text/plain")

    def post(self):
        u = urlparse(self.path)
        body = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))) or b"{}")
        d = self.data
        if u.path == "/api/save":
            d.undo.append({})  # one undo step per save, batch included
            # a same-text batch: the clips the reviewer kept ticked (always the shown one)
            for n in sorted({int(body["n"]), *(int(x) for x in body.get("batch", []))}):
                d.save_correction(d.lines[n], body["text"], body.get("source", "user"), body.get("note", ""))
            d.unbuilt += 1
            return self.js({"ok": True, "reviewed": len(d.corr), "unbuilt": d.unbuilt})
        if u.path == "/api/fit":  # POST: the typed text stays out of the URL
            text, changes = d.fit(d.lines[int(body["n"])], body.get("text", ""))
            return self.js({"text": text, "changes": changes})
        if u.path == "/api/unsure":  # {n, on, note}: mark / clear "not sure" (data/unsure.tsv)
            ln = d.lines[int(body["n"])]
            d.set_unsure(ln, bool(body.get("on", True)), body.get("note", ""))
            return self.js({"ok": True, "unsure": d.unsure.get((ln["id"].lower(), ln["voice"].lower()))})
        if u.path == "/api/undo":
            ids = d.undo_last()
            d.unbuilt += 1 if ids else 0
            return self.js({"ok": bool(ids), "ids": ids or [], "reviewed": len(d.corr)})
        if u.path == "/api/sample":
            return self.js(d.make_sample(body["mode"], bool(body.get("more"))))
        if u.path == "/api/accept_rest":
            res = d.accept_rest(body["mode"])
            return self.js(res or {"error": "finish the sample first"}, 200 if res else 409)
        if u.path == "/api/rebuild":
            return self.js(*rebuild())
        self.send(404, b"not found", "text/plain")


REBUILD = threading.Lock()


def refresh_name_audit(a) -> str:
    """Re-runs name_audit.py's computation in-process on the current draft and rewrites out/name_audit.tsv / .md
    (Data reads the "Names the game doesn't know" list from it). When an input is missing or the audit fails
    the earlier file stays in use. The FLSubs vocabulary comes from the --flsubs xml if given, else from the
    draft's FLSubs column. Returns a one-line note."""
    out, script = Path(a.out), Path(a.script)
    need = [out / "draft.tsv", out / "game_text.tsv", out / "gamenames.tsv", script]
    missing = [p.name for p in need if not p.exists()]
    if missing:
        return "name audit: kept the earlier name_audit.tsv (missing " + ", ".join(missing) + ")"
    try:
        import name_audit
        if a.flsubs:
            import flsubs
            texts = [t for t, _, _ in flsubs.load(a.flsubs).values()]
        else:
            texts = [r["FLSubs"] for r in merge.tsv(out / "draft.tsv")]
        rows, term_rows = name_audit.compute(out, script, texts)
        name_audit.write_reports(out, rows, term_rows)
        return f"name audit: {len(name_audit.odd_names(rows))} names the game doesn't know"
    except Exception as e:  # the review flow must not stop for a side list
        return f"name audit: kept the earlier name_audit.tsv ({type(e).__name__}: {e})"


def rebuild():
    """Runs merge.py with the tool's own inputs and swaps in the fresh data: Re-review, the actor's takes,
    same-recording copies and list counts are current without a restart. Only for the project's own
    corrections file (a scratch file must not write data/subtitles.tsv)."""
    a = Handler.args
    if Path(a.corrections).resolve() != (HERE.parent / "data" / "corrections.tsv").resolve():
        return {"error": "rebuild only works on the project's corrections file"}, 409
    if not REBUILD.acquire(blocking=False):
        return {"error": "a rebuild is already running"}, 409
    try:
        run = subprocess.run([sys.executable, "-I", str(HERE / "merge.py"), "--out", a.out, "--corrections",
                              a.corrections, "--glossary", a.glossary], capture_output=True, text=True,
                             encoding="utf-8")
        if run.returncode:
            return {"error": run.stderr[-2000:]}, 500
        audit_note = refresh_name_audit(a)  # the "names the game doesn't know" list follows the new draft
        Handler.data = Data(Path(a.out), Path(a.corrections), Path(a.glossary))
        keep = ("subtitles.tsv:", "re-review:")
        summary = [l for l in run.stdout.splitlines() if l.startswith(keep) or l.startswith("  ")][:14]
        return {"ok": True, "summary": summary + [audit_note]}, 200
    finally:
        REBUILD.release()


def audio(path: Path):
    """Freelancer's dialogue is MP3 inside a RIFF/WAVE wrapper (format 0x55); browsers play the raw MP3."""
    b = path.read_bytes()
    if b[:4] == b"RIFF" and b[8:12] == b"WAVE":
        o, fmt = 12, 0
        while o + 8 <= len(b):
            cid, size = b[o:o + 4], struct.unpack_from("<I", b, o + 4)[0]
            if cid == b"fmt ":
                fmt = struct.unpack_from("<H", b, o + 8)[0]
            elif cid == b"data" and fmt == 0x55:
                return b[o + 8:o + 8 + size], "audio/mpeg"
            o += 8 + size + (size & 1)
    return b, "audio/wav"


def name_audit_stale(a) -> bool:
    """out/name_audit.tsv is missing or older than one of its inputs (the draft, the game text and names, the
    script, the audit's own code): the "Names the game doesn't know" list would be computed on old data."""
    out = Path(a.out)
    audit = out / "name_audit.tsv"
    if not audit.exists():
        return True
    inputs = [out / "draft.tsv", out / "game_text.tsv", out / "gamenames.tsv", Path(a.script), HERE / "name_audit.py"]
    newest = max((p.stat().st_mtime for p in inputs if p.exists()), default=0)
    return newest > audit.stat().st_mtime


def preflight(a) -> list:
    """What the tool needs before it can start, each missing piece named with what makes it. Returns problems."""
    problems = []
    if sys.version_info < (3, 10):
        problems.append(f"Python 3.10 or newer is needed (this is {sys.version.split()[0]}); the review folder's "
                        r"venv: FLSubtitles\asr\.venv (uv sync in FLSubtitles\asr)")
    out = Path(a.out)
    if not out.is_dir():
        problems.append(f"no pipeline output folder {out} (git-ignored game content: build it with the L0 pipeline, "
                        "docs/REVIEW_PROCESS.md section 1)")
    else:
        for name in REQUIRED_COLUMNS:
            if not (out / name).exists():
                problems.append(f"missing {out / name} - made by {MAKES[name]}")
    if not Path(a.glossary).exists():
        problems.append(f"missing {a.glossary} (tracked in the repository: git checkout it)")
    corr = Path(a.corrections)
    if not corr.parent.is_dir():
        problems.append(f"no folder for the corrections file {corr}")
    return problems


def hold_lock(corrections: Path):
    """One server per corrections file (two would overwrite each other's saves): an exclusive lock on
    <corrections>.lock, held while the process lives and released by the system when it ends (crash included).
    Returns the open lock file, or None when another server holds it."""
    f = open(corrections.with_name(corrections.name + ".lock"), "a+", encoding="utf-8")
    try:
        if os.name == "nt":
            import msvcrt
            f.seek(0)
            msvcrt.locking(f.fileno(), msvcrt.LK_NBLCK, 1)
        else:
            import fcntl
            fcntl.flock(f, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except OSError:
        f.close()
        return None
    return f


class Server(ThreadingHTTPServer):
    """Windows' SO_REUSEADDR (HTTPServer's default) would let a second server bind a port that is in use and
    steal half its requests: take the port exclusively there instead."""
    allow_reuse_address = os.name != "nt"
    daemon_threads = True

    def server_bind(self):
        if os.name == "nt" and hasattr(socket, "SO_EXCLUSIVEADDRUSE"):
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
        super().server_bind()


def bind(port: int, tries: int = 10):
    """A server on 127.0.0.1 only (never the network): the asked port, else the next free one."""
    for p in range(port, port + tries):
        try:
            return Server(("127.0.0.1", p), Handler)
        except OSError as e:
            who = "another review tool" if review_tool_at(p) else "another program"
            print(f"port {p} is taken ({who}: {e.strerror or e})", flush=True)
            if p == port and who == "another review tool":
                print(f"  - a review tool already runs at http://127.0.0.1:{p}/ ; two servers on one corrections "
                      "file would overwrite each other's saves. Stop that one first (Ctrl+C in its window).",
                      flush=True)
                return None
    print(f"no free port in {port}-{port + tries - 1}: pass --port", flush=True)
    return None


def review_tool_at(port: int) -> bool:
    import urllib.request
    try:
        with urllib.request.urlopen(f"http://127.0.0.1:{port}/api/stats", timeout=1) as r:
            return r.status == 200 and b"lines" in r.read(4096)
    except Exception:  # noqa: BLE001 - anything else on that port
        return False


def main():
    global REVIEWER
    ap = argparse.ArgumentParser(description="Subtitle review tool (local web page on 127.0.0.1). Paths default "
                                             "to this repository's own folders, from any working directory.")
    ap.add_argument("--out", default=str(HERE.parent / "out"), help="pipeline output folder (FLSubtitles/out)")
    ap.add_argument("--corrections", default=str(HERE.parent / "data" / "corrections.tsv"))
    ap.add_argument("--glossary", default=str(HERE / "glossary.txt"))
    ap.add_argument("--port", type=int, default=8765, help="first port to try (default 8765; the next free one "
                                                           "is taken when it is busy)")
    ap.add_argument("--open", action="store_true", help="open the page in the default browser once it is up "
                                                        "(the page plays each clip as it is shown)")
    ap.add_argument("--no-audio", action="store_true", help="serve no clips (testing the page without sound)")
    ap.add_argument("--check", action="store_true", help="check the prerequisites and load the data, then exit "
                                                         "(0 = ready)")
    ap.add_argument("--script", default=str(SCRIPT), help="shooting_script.txt (name audit)")
    ap.add_argument("--flsubs", help="FLSubs subtitles.xml for the name audit (default: the draft's FLSubs column)")
    ap.add_argument("--reviewer", default=REVIEWER, help="written to the corrections file's `by` column")
    a = ap.parse_args()
    REVIEWER = a.reviewer
    Handler.args = a
    problems = preflight(a)
    if problems:
        print("The review tool cannot start:\n" + "\n".join("  - " + p for p in problems), flush=True)
        return 2
    srv = None
    if not a.check:  # the port first: a busy one is said at once (requests wait in the queue while loading)
        lock = hold_lock(Path(a.corrections))
        if lock is None:
            print(f"another review tool already works on {a.corrections}: two would overwrite each other's saves."
                  f"\n  Use the one that runs (http://127.0.0.1:{a.port}/ or the port its window shows), or stop it "
                  "first (Ctrl+C in its window).", flush=True)
            return 3
        Handler.lock = lock  # held until the process ends
        srv = bind(a.port)
        if srv is None:
            return 3
    if name_audit_stale(a):  # the "Names the game doesn't know" list follows the current draft
        print(refresh_name_audit(a), flush=True)
    t0 = time.time()
    Handler.data = Data(Path(a.out), Path(a.corrections), Path(a.glossary))
    print(f"loaded {len(Handler.data.lines)} lines, {len(Handler.data.corr)} reviewed ({time.time() - t0:.0f} s)"
          + (f"; {len(tsvio.PROBLEMS)} input problem(s), see the warnings above" if tsvio.PROBLEMS else ""),
          flush=True)
    if a.check:
        print("ready", flush=True)
        return 0
    url = f"http://127.0.0.1:{srv.server_address[1]}/"
    print(f"review tool: {url}   (Ctrl+C stops it)", flush=True)
    if a.open:
        import webbrowser
        webbrowser.open(url)
    if hasattr(signal, "SIGBREAK"):  # Ctrl+Break and a closed console window stop it like Ctrl+C
        def stop(*_):
            raise KeyboardInterrupt
        signal.signal(signal.SIGBREAK, stop)
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        print("stopping...", flush=True)
    finally:
        srv.server_close()
    print("stopped (every save was already written)", flush=True)
    return 0


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    sys.stderr.reconfigure(encoding="utf-8")
    sys.exit(main())
