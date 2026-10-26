"""A tiny synthetic pipeline output (no game content, no audio) for the review tool's tests: the conventions
come from the repository's own rule files (data/style_rules.tsv, name_rules.tsv, chatter_rules.tsv,
asr/glossary.txt) and the game's chatter grammar (<root>/docs/chatter_grammar.tsv)."""
import shutil
import sys
import tempfile
from pathlib import Path

ASR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ASR))
REVIEW = ASR.parents[1]       # subtitles/review
ROOT = ASR.parents[3]         # the FLvibed repository
DATA = ASR.parent / "data"

DRAFT_HEAD = ["id", "voice", "speaker", "text", "source", "fragment", "review", "note", "speech to text", "FLSubs"]
V = "pilot_m_test_m01"  # one test voice (family pilot_m_test_m01)

# id, voice, text, source, fragment, speech to text, FLSubs, sec
LINES = [
    # the pieces of one dock request (chatter_grammar.tsv 06ED6DD0), as the data stores them
    ("gcs_misc_thisis+", V, "this is", "asr", "1", "this is", "", 0.5),
    ("gcs_refer_faction_li_n_short", V, "Liberty Navy", "asr", "1", "Liberty Navy", "", 0.8),
    ("gcs_refer_formationdesig_01", V, "Alpha", "asr", "1", "Alpha", "", 0.5),
    ("gcs_misc_number_1", V, "1", "asr", "1", "one", "", 0.3),
    ("gcs_misc_dash", V, "dash", "asr", "1", "dash", "", 0.3),
    ("gcs_misc_number_2", V, "2", "asr", "1", "two", "", 0.3),
    ("gcs_misc_to", V, "to", "asr", "1", "to", "", 0.2),
    ("gcs_refer_base_Li01_01_Base-", V, "Planet Manhattan", "asr", "1", "Planet Manhattan", "", 0.9),
    ("gcs_dockrequest_request_03+", V, "i'm requesting", "asr", "0", "I'm requesting", "", 0.7),
    ("gcs_dockrequest_todock-", V, "to dock", "asr", "0", "to dock", "", 0.4),
    # one actor's takes of a whole line (voice, voice + a, a vanilla copy) and another actor
    ("gcs_combat_cheer_01", "pilot_f_take_m02", "Got him, he's going down!", "asr", "0", "Got him, he's going down!",
     "", 1.0),
    ("gcs_combat_cheer_01", "pilot_f_take_m02a", "Got 'im, he's going down!", "asr", "0", "Got 'im, he's going down!",
     "", 1.0),
    # other words at the same length: an intentional variant, not a take
    ("gcs_combat_cheer_01", "pilot_f_take_m02b_vanilla", "Splash one, that bandit is history!", "asr", "0",
     "Splash one, that bandit is history!", "", 1.0),
    ("gcs_combat_cheer_01", "pilot_m_other_m03", "Got him, he's going down!", "asr", "0", "Got him, he's going down!",
     "", 1.0),
    # a story line (kept as written; voice "*" in the corrections)
    ("DX_M01_0100_TRENT", "trent", "We fly to Rheinland now.", "script", "0", "We fly to Rhineland now.",
     "We fly to Rheinland now.", 2.0),
    ("DX_M01_0110_JUNI", "juni", "Copy that, Trent.", "script", "0", "Copy that, Trent.", "Copy that, Trent.", 1.5),
]
GAMENAMES = [
    ("gcs_refer_formationdesig_1", "Alpha", ""), ("gcs_refer_formationdesig_2", "Beta", ""),
    ("gcs_refer_base_Li01_01_Base", "Planet Manhattan", ""), ("gcs_refer_faction_li_n", "Liberty Navy", ""),
    ("gcs_refer_faction_rh_n", "Rheinland Military", "Rheinland"),
]


def write_tsv(path: Path, head: list, rows: list) -> None:
    path.write_text("\n".join(["\t".join(head)] + ["\t".join(str(x) for x in r) for r in rows]) + "\n",
                    encoding="utf-8", newline="\n")


def make(tmp: Path = None):
    """Returns (out dir, corrections path, glossary path) of a fresh synthetic project in a temp folder."""
    tmp = Path(tmp or tempfile.mkdtemp(prefix="flsub_review_test_"))
    out, data = tmp / "out", tmp / "data"
    out.mkdir(parents=True, exist_ok=True)
    data.mkdir(parents=True, exist_ok=True)
    write_tsv(out / "draft.tsv", DRAFT_HEAD,
              [(i, v, "", t, src, frag, "", "", asr, fl) for i, v, t, src, frag, asr, fl, _ in LINES])
    write_tsv(out / "index.tsv", ["id", "voice", "file", "kind", "sec", "text"],
              [(i, v, str(tmp / "no_audio" / f"{v}_{i}.wav"), "story" if i.startswith("DX_") else "bank", sec,
                t if i.startswith("DX_") else "") for i, v, t, _, _, _, _, sec in LINES])
    write_tsv(out / "aligned.tsv", ["id", "voice", "speaker", "text", "source", "match", "asr", "notes", "flsubs"],
              [(i, v, "", t, "script", "1.00", asr, "", fl) for i, v, t, src, _, asr, fl, _ in LINES
               if src == "script"])
    write_tsv(out / "gamenames.tsv", ["prefix", "name", "short", "kind", "file"],
              [(p, n, s, "test", "") for p, n, s in GAMENAMES])
    # the aligner's rows (FLSubtitles/aligner): one current line with its last two words piled at the clip's end,
    # one timed for an earlier text (stale), the rest without timings; one line it could not match
    import review_server
    write_tsv(out / "word_times_cache.tsv", ["id", "voice", "text_fnv", "audio_sha1", "end_ms", "starts_ms", "collapsed"],
              [("DX_M01_0110_JUNI", "juni", review_server.fnv1a("Copy that, Trent."), "x", "1500", "0,1490,-1", "2"),
               ("DX_M01_0100_TRENT", "trent", "00000000", "x", "2000", "0,300,600,900,1200", "0"),
               ("gcs_misc_thisis+", V, review_server.fnv1a("this is"), "x", "500", "0,250", "0")])
    write_tsv(out / "word_times_failed.tsv", ["id", "voice", "text"],
              [("gcs_combat_cheer_01", "pilot_m_other_m03", "Got him, he's going down!")])
    for f in ("style_rules.tsv", "name_rules.tsv", "chatter_rules.tsv"):
        shutil.copyfile(DATA / f, data / f)
    corr = data / "corrections.tsv"
    corr.write_text("# test corrections\nid\tvoice\ttext\tsource\tnote\twhen\tby\n", encoding="utf-8", newline="\n")
    return out, corr, ASR / "glossary.txt"


def data(tmp: Path = None):
    """A loaded review_server.Data over the synthetic project (stdout of the loaders kept quiet)."""
    import contextlib
    import io
    import review_server
    out, corr, glossary = make(tmp)
    with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
        return review_server.Data(out, corr, glossary)


def line(d, lid: str, voice: str):
    return next(x for x in d.lines if x["id"] == lid and x["voice"] == voice)
