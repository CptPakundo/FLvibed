# Subtitle review: how it works, what we learned (session 2 audit)

State on 17 Oct 2026: 24,963 shipped lines (`FLSubtitles/data/subtitles.tsv`); review paused with 8 lines
open (section 5). Next stage: the runtime display (FLvibed: subtitles/docs/DISPLAY_PLAN.md).

## 1. Running it

**The review tool, standalone** (from any folder, or double-click the file): `subtitles\review\review.cmd`: it starts
the server and opens http://127.0.0.1:8765/ in the browser (the owner, 26 Oct 2026; the page plays each clip as it is
shown); `review.cmd --no-open` starts the server only. Ctrl+C (or
closing the window) stops it; every save is already on disk. It needs only Python's standard library (3.10+):
`review.cmd` takes the review venv `FLSubtitles\asr\.venv` (`uv sync` there), else FLvibed's
`research\tools\ocr\.venv`, else `py -3` / `python`. The environment's launch config `subtitle-review` starts the
same server. Options (`review.cmd --help`): `--check` (prerequisites + load the data, exit 0 = ready), `--port`,
`--open`, `--no-audio` (serves no clips: testing the page without sound), `--corrections <file>` (a scratch file:
Rebuild is refused there), `--reviewer`. What happens at start, and why:

| Check / behaviour | What it does |
|---|---|
| Prerequisites | Python 3.10+; the pipeline outputs it cannot run without, each missing one named with what makes it: `out/draft.tsv` (merge.py), `out/index.tsv` (build_index.py, needs the game install), `out/aligned.tsv` (align.py), `out/gamenames.tsv` (gamenames.py); `asr/glossary.txt`. Exit 2 when one is missing |
| One server per corrections file | an exclusive lock on `<corrections>.tsv.lock` (git-ignored; the system releases it when the process ends, crash included): a second server on the same file is refused at once (exit 3) instead of overwriting the first one's saves |
| Port | 127.0.0.1 only (never the network), taken exclusively (Windows' default socket reuse would let a second server share it); when 8765 is busy, the next free port, said in the window |
| Name audit | `out/name_audit.tsv` ("Names the game doesn't know") is recomputed when it is older than the draft, the game text / names, the script or `name_audit.py`; Rebuild always recomputes it |
| Input files | every TSV is read defensively (`asr/tsvio.py`): bad bytes are replaced and a row with more fields than its header is reported with file and line (window + Stats), never a crash or a silent loss; a file missing a column the tool reads stops it with the file named |
| Writes | every decisions file (`corrections.tsv`, `samples.tsv`, `bulk_accepted.tsv`, `rereview_notes.tsv`, unsure.tsv) and every merge.py output is written to a temp file and swapped in: a crash never leaves half a file |
| Bad requests | answered 400 / 500 with the message; the server keeps running |

**Tests** (seconds, no game content, no audio, no server): `review.cmd test`. A synthetic project with the
repository's own rule files: fit rules, sentence joining against the runtime's C++ `JoinPieces` fixtures (read from
`<root>\subtitles\src\impl\impl.cpp`) and the game's grammar, one actor's takes, saving / undo bytes, word timings and
suspects, "not sure" marks, the shared normalisers (`merge.squash`, `conv_form`, `strip_final_period`), the reader /
atomic writer, and the publish copy rule (the copies in `<root>\subtitles\data\` match the products in HEAD and are
not hand-edited). `<root>\tools\drift_check.ps1` runs them as its `subtitle-review` line.

**The pipeline step after a data or rule change** (seconds):
```
cd FLSubtitles\asr
.venv\Scripts\python -I merge.py --out ..\out --corrections ..\data\corrections.tsv --glossary glossary.txt
```
Every save writes its `when` (ISO date-time) and `by` (`--reviewer`, else `$FLVIBED_REVIEWER`, else `user`) into
`corrections.tsv`; Rebuild also refreshes the "Names the game doesn't know" list (`name_audit.py`) from the new draft.

**In the page, besides picking and typing** (the key list is under each line): the **word strip** under the player
shows each word at the start the aligner found for it (click = play from it, `[` / `]` step, `W` hides it; dotted =
not placed, orange = piled at the clip's end); **Alignment suspects** lists the lines whose text may not be what the
recording says, most suspicious first, each with its reasons (the aligner could not match the words, words piled at
the clip's end or unplaced, a seconds-per-word far from the median, speech to text the only source; a line leaves it
once saved in the tool); **X** marks a line "not sure" (`data/unsure.tsv`, list "Marked not sure"; Shift+X with a
note), X again clears it; **Stats** (T): how every line is decided, word timings, what is open per list.
- **Data or rule change** (merge.py, data/*.tsv): press **Rebuild** in the tool (or POST /api/rebuild). It
  re-runs merge.py and reloads, with no restart. Read the summary it prints: `subtitles.tsv: N lines, X changed`
  plus the causes (out/subtitles_diff.txt). "X changed" is against the previous build, which may be
  uncommitted work; "since the last commit: N lines differ" is the net change to check before committing.
- **Tool code change** (review_server.py / review.html): restart the preview server.
- **Commit after every change**, the user's reviews included (local repo, never push). Squash only when the
  user closes a stage. A commit with `subtitles.tsv` staged re-aligns the word timings of the changed lines first
  (pre-commit hook, `FLSubtitles/aligner/align_words.py`, a few seconds plus ~20 s model load; user, 22 Oct 2026:
  "anything that was changed would need to have its forced alignment re-run") and commits `word_times.tsv` with it;
  the same hook publishes the products into `<root>\subtitles\data\` within that commit (`tools/publish.ps1`; FLvibed's
  CLAUDE.md "Subtitle data"). New lines on the suspect list (`out/word_times_suspect.tsv`) are worth a look.
  A line with 2+ words unplaced is aligned again padded with silence (0.3 / 0.6 / 1.0 s; each word takes the start two
  runs agree on - 26 Oct 2026: 6 of the 10 story / Neural Net lines that had shipped no timings now do; suspects 1,779
  -> 1,048); `--retry-suspects` re-runs it on lines aligned before.
- **The intro movies are force-aligned too** (26 Oct 2026; before: `asr/intro_align.py`'s 2 s speech-to-text windows,
  about +-0.5 s, pages by estimate). Every aligner run aligns `data/movies.tsv` when the movie audio is at hand
  (`out/movies/<movie>.wav`, extracted once with ffmpeg from the install's `DATA\MOVIES\<movie>.wmv`; never played):
  each line inside its own window (start / end +-0.4, 0.75 and 1.25 s; a word counts where two windows agree) into
  `word_times.tsv` rows with id = the movie and voice `movie` (times on the movie clock; the runtime finds them by movie
  + text hash). `--movies-only` aligns just them; `--movies-only --fix-windows` moves a window to its words when every
  word is placed (start = first word, end = last word + 0.2 s, never past the next line; the text never changes; logged
  in `out/movie_window_changes.tsv`); `out/movie_word_times_suspect.tsv` lists the lines to listen to (words unplaced,
  speech outside the window). Without the movie files a run keeps the movie rows whose text still matches.

## 2. Conventions (decided by the user; the merge applies them, the tool shows them)

| Topic | Rule |
|---|---|
| Story/campaign/unique mission lines (dx_, and mission characters' nag_/bc_/rmb_/mb_) | kept exactly as written (subtitle = saved text) |
| Generic pieces the game joins (multi-piece chatter, GCS base pairs, names, numbers) | no sentence-ending `.`, `!` or trailing `...`; runtime joins and adds the end mark (`merge.render_sentence`, rule in chatter_grammar.tsv header) |
| Generic lines played alone (one-piece comms, rms_ taunts, bar lines, Trent's replies) | whole sentences: keep their end mark |
| Neural Net voice (the ship's computer) | ALL CAPS, no end period (merge applies it; type it any case) |
| Case inside joined sentences | a continuation starts lowercase, a sentence opener with a capital (grammar) |
| Cut-off dialogue | en dash on the last word, never a period (`Repeat–`) |
| Callsigns and numbers | digits: Gamma 6, Alpha 1-2, Omega-5, Sector C8 |
| Names | the game files' spelling wins (bases, systems, factions, goods); base intros take the base's name words from the id |
| One actor's takes (voice + a/b, vanilla) | same text, unless they differ by more than two words at the same clip length (an intentional variant) |
| Same recording (identical audio) | one text for every copy |
| Same line id + same words in another voice | your review decides it (punctuation included) |
| A word misheard where other pilots say the line the same way | the swapped word other pilots say in that spot ("switching a full" → "to full"); never an added/dropped word, casual form or interjection (merge.context_pick) |
| Contractions / casual forms differing between reads | the form speech to text heard (it was right 34 of 35 times; personality/register guessing failed and was removed); except FLSubs' "gonna" where speech to text wrote "going to" (user chose "gonna" 2 of 2) |
| Sounds (screams, sighs) in story lines | so far the user chose FLSubs' description each time ("[sigh]", "[unintelligible screams]", "Aaaaaaaahhh!"): evidence for the screams decision |
| Spelling | style_rules.tsv (all right, okay, Mr./Dr., Madam President, hush-hush, ...); chatter_rules.tsv (per-id/voice mishearings: bogey, bandit, gaijin (rvp121), juice (rvp106), ...); name_rules.tsv |

## 3. The review loop that works

1. **Split a list by evidence, then sample it.** The biggest win: script lines where all three sources
   matched word for word had 0 errors in 83 samples, so 1,516 lines were accepted in one click. The 17
   where speech to text heard other words had 7 errors, so those stay for listening.
2. **Sample → accept the rest.** "Sample this list" draws 50; check them; "Sample 50 more" if unsure;
   "Accept the rest" writes `data/bulk_accepted.tsv` (not listened to, any line can be reviewed later; a
   correction always wins). A sampled line counts as wrong when its *words* change.
3. **Turn repeated fixes into rules.** When the user fixes the same mishearing twice, add a rule
   (chatter_rules.tsv with an optional voice column), rebuild, check the diff summary.
4. **Re-review** shows every reviewed line a rule changed (one entry per actor). Accept "Conventions
   applied" or "Your saved text (keep exactly)"; the latter sticks.
5. **Claude judges a list** (as Neural Net): fixes go to auto_decisions.tsv; only the decisions Claude is
   unsure of get a `[check]` reason and appear in "Claude was unsure: check these" (the user audits
   just those, not the whole list). Same for bulk_accepted.tsv entries whose sample note starts `[check]`.
6. **Split a mixed list by type, show the user examples** (data/picks.tsv → "Picked: <list>" in the tool; its `context` column is shown on the line, so write out the scene for any line that needs it),
   then bulk-decide the safe types. Disagreed list: long lines without a casual-form difference took the
   draft (speech to text's words; its punctuation where FLSubs' landed wrongly, via auto_decisions.tsv
   rows with a voice); casual-form and short lines need an ear (speech to text writes "going to" for
   "gonna", mishears short clips: "Use some kind of newbie").

Decision-saving machinery in place: identical recordings saved together, an actor's takes saved together,
same-text batches (one decision for many clips), same id + same words settled by your review, game names
from game data, context from other characters' versions (mishearing fixes with a vocabulary guard).

## 4. Lessons: what to avoid

- **Don't guess from theory when evidence exists.** Line-id patterns (dock-request order), character
  personality (register): both wrong. Read the game's code/data (Content.dll, GCS .thn scenes) and the
  recording.
- **Scope a rule before applying it.** Several broad rules overreached and were narrowed:
  - stripping periods from *all* generic lines (whole sentences need theirs);
  - stripping them from mission characters' lines (unique dialogue);
  - keeping FLSubs' "..." on joined pieces (it marked a clip boundary, not a pause).
  Always read the rebuild diff summary (`X changed`, by cause) and look at samples before committing.
- **FLSubs is not independent across voices.** It reused one transcription per line id, so its typos look
  like agreement. Use other voices' speech to text and your reviews as reference, never FLSubs.
- **A game name never loses to a guess** (Bundschuh vs "Bonshu"): speech to text can mishear the same
  name the same way in several clips.
- **Propagation must respect the audio.** A take that is really another recording (pilot_f_leg_m01b's
  defendleader_02 = _03) or a longer clip is not a take of the same line. Saving must reach every take
  (an accepted convention once reached only one).
- **Never edit a reviewed line silently.** Every rule change to a correction lands in Re-review (only
  final-period-only changes are exempt, by the user's rule).
- **Editing regexes or code from the shell is fragile.** Heredocs turned `\b` into backspace characters
  three times and `\t`/`\n` inside Python patch scripts into real tabs/newlines twice. Edit files with the
  Edit tool; write helper scripts with the Write tool; append TSV rows with `printf '%s\t...'` and single
  quotes; check with `cat -A`.
- **Never ask what a rule already decides** (user, repeatedly). `review_server.rule_settled` keeps out of
  the lists (browse "Settled by the rules" to spot-check, ~9,900 clips): reads that differ only in commas,
  capitals, Mr./Mister, all right, hyphens; a joined piece's start capital, end mark and trailing "...";
  FLSubs' "?" on a line built as a question; and same-text clips that several voices say in words the
  game itself uses (no "toroids"/"Bonshu"; data/known_words.tsv adds words Claude checked in their line);
  separate recordings (voices or takes) reading the same words; game names over a misheard spelling. FLSubs' missing commas come from speech to text
  (`merge.add_commas`, not after an ambiguous first word: "Sure you want…", "Damn Intel…"). Left for the
  user: "!" vs "." (intonation), a lone "..." , words only one read has.
- **Every line carries context** (`review_server.auto_context`, shown as "Context:"): the base-scene meaning
  (SCENE table of GCS codes), the chatter situation and the sentence the game builds, story lines around
  it, other voices' versions. A picked line's written context (picks.tsv) overrides it. With context, a
  choice like Trent's "Unpleasant?" needs no question.
- **A contextual pass catches what both sources miss:** homophones and scene meaning ("get around" →
  "get a round" for drinks, "them there credits" in the southern-flavoured race crowd, "a draft" beer).
  Do one before bulk-accepting a list where the sources agree.
- **Audit saved reviews after rule changes** (saved text vs shipped subtitle, by kind of change). 17 Oct:
  no words/commas changed; 62 joined pieces differ only by the end mark the game adds (template); 44
  whole lines saved without their period during the period-stripping overreach got it back; the all-caps
  fix had lowercased the user's typed "CNC" (now FLSubs text only). One actor's own word ("Bretonia Mining
  and Minerals") is an outlier saved by hand, not a rule.
- **A fix repeated by hand belongs in the merge.** Misplaced FLSubs punctuation was first repaired line by
  line (with a rule that also broke real questions twice); `merge.repair_marks` now fixes it at the
  source for every line.

## 5. State (review paused 17 Oct 2026; the in-game display stage started)

Every review list is done except **script lines speech to text heard other words: 8** (the actor's own
small words, heard by both reads: "And she's LSF", "Ah, more of the same", "gonna"/"we'll"; need the
user's ear). Done by contextual passes (Claude reads every line with its context, fixes what context
decides, flags doubts as `[check]`, accepts the rest): speech to text only (256 as heard, 21 fixed),
agreed (229: FLSubs' "!"/"?" fit the scene), script lines (179 script: speech to text misheard or only
numbers/names/spelling differ; 27 recording: both reads agree against a script typo; 3 special),
Neural Net (159; the game plays each message whole). The user prefers: the script's capitals for shouts
(YEE HAW!, NOOOO!, YESSSS!), FLSubs' sound descriptions for screams, "Cease fire!" as the order.
Later stages: screams style (264), game-name spot-check (411), scene-mix timing (19), orphans (128).

## 6. The Story order view (26 Oct 2026)

**What it is** (the owner: "go through story dialogue and in-between mixes in the chronological order they play out
in the game"): list menu, Browse > **Story order**. Every story line (`kind = story` in subtitles.tsv), every scene
mix and every intro movie line in the order the game plays them, under mission / scene headings: the intro movie,
then M01a ... M13. Above each line: its heading, its place (`#n of the story order`), a branch reason if it is off
the main path, where the order comes from (trigger > action > scene @ time, file:line) and the lines around it
(click = go there). The usual reads, editing, word strip and audio; **N / P** move in story order; a save keeps the
line in place. **Jump to a mission / scene** (the heading list) and **unreviewed only** (lines nobody has decided;
movie lines whose note asks for a listen) replace "show reviewed" there. Scene mixes show "no subtitles yet" (their
cues stay a later stage; the speech-to-text draft is shown for reference). Intro movie lines are read-only here
(`data/movies.tsv`, built by `intro_align.py`, windows checked by forced alignment; no clip of their own). Lines the derivation could not place come last
under **Not placed**, each with the reason.

**How the order is derived** (`asr/story_order.py`, generated into `data/story_order.tsv` - ids, order and provenance
only, no game text; rerun it after a product or install change: `.venv\Scripts\python -I story_order.py`, ~10 s;
the unit test fails when a story line or movie line is missing from the file):
- **Intro**: `EXE\freelancer.ini` `[Startup] movie_file` names the intro that plays (`FL_Intro.wmv` = movies.tsv
  `fl_intro`); the other variants of it (`fl_intro_full`, `fl_intro_vanilla`: FLvibed's `movie_intro` option plays one
  instead) follow, each under its own heading; lines by movie time.
- **Campaign order**: the mission scripts in the order Content.dll's story code lists them (`Missions\M01a\m01a.ini`
  ... `M13`); a room scene Content.dll loads itself (`M001a_s006x`, King on the Pittsburgh pad) plays at the end of the
  mission whose folder holds it.
- **A mission**: its trigger graph walked from the `InitState = ACTIVE` triggers on an estimated clock. `Cnd_True` at
  once, `Cnd_Timer` after its seconds, `Cnd_CommComplete` once that line has played (clip length from subtitles.tsv;
  comms queue on the radio), `Cnd_WatchTrigger` by the watched trigger's state (ON / ACTIVE armed, OFF, COMPLETE
  fired). Conditions the player meets wait; once nothing timed is left the walk takes one: what can be done where the
  player is (space / base; a new game starts in space by `EXE\NewPlayer.fl`) before moving, the player's own acts
  before NPC-only ones, the wider of two approach circles first, hits / damage / leaving the system last, then
  activation order; the player moves only once the radio and any room scene are through. Lines come from
  `Act_SendComm`, `Act_EtherComm`, `Act_StartDialog` ([Dialog] `Line`s), `Act_CallThorn` (the cutscene's `START_SOUND`
  events by time), `Act_AddRTC` (room scene ini: `action`, `offer`, `decision`, `accept` in that order, one scene
  after another; `decline` / `reject` = a branch) and any other action naming a line. `Act_ChangeState = SUCCEED` ends
  the mission.
- **Branches** (marked "branch: <why>", placed right after the point their trigger became active, with what they
  activate): a trigger that fails the mission (or only leads to a failure), the player's ship destroyed, the player
  breaking an escort's tether, a declined offer, an alternative deactivated by another trigger, or one still waiting
  when the mission ended. Each line appears once, where it first plays (the main path wins).
- **Scenes**: `.thn` files the HD Edition ships as text are read directly; compiled ones are decoded with Jason Hood's
  Dethorn (`<root>\research\thn_work\EXE\dethorn.exe`, adoxa's tools, already in the research folder; it uses the
  game's own THORN code) into `<root>\research\thn_work\decoded\` (git-ignored), renewed when a .thn changes.

**State (26 Oct 2026)**: 2,345 of 2,486 placed (2,039 main path, 306 branch / alternative intro), 141 not placed:
114 no mission script or scene names (mostly cut lines: DX_M13 18, dx_s038x 12, dx_m10 11, ...), 9 in scenes no
script starts (some decline / reoffer scenes), 18 in triggers never activated (M06's unused race chatter and
Albertson / Quintaine at Leeds, M05's traitor lines, M12's hole countdown).

**Limits**: the clock is an estimate - player steps are a fixed 20 s and parallel threads (a comm racing a timer, two
fights at once) can come out in another order than a given play-through; a room scene added in space is placed where
it was added, though it plays at the next visit of its room; repeated lines appear once. Check a doubtful stretch
against the mission's runbook (`<root>\docs\missions\`) or the footage.
