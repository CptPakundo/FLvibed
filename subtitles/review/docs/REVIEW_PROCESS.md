# Subtitle review: how it works, what we learned (session 2 audit)

State on 17 Oct 2026: 24,963 shipped lines (`FLSubtitles/data/subtitles.tsv`); review paused with 8 lines
open (section 5). Next stage: the runtime display (FLvibed: modules/subtitles/docs/DISPLAY_PLAN.md).

## 1. Running it

**The review tool, standalone** (from any folder, or double-click the file): `subtitles\review\review.cmd`: it opens
http://127.0.0.1:8765/ in the browser and leaves no window open (the owner, 31 Oct 2026: "as simple to start (and stop)
as a single web page"; the page plays each clip as it is shown). Double-clicked again while it runs, it shows the
running tool's page. **Stop**: the page's Stop button, or close its last page (the tool stops itself a little later:
15 s after the page said bye, at most 3 minutes after its last sign of life - a hidden tab's timers slow down - and
never during a rebuild or an alignment). Every save is already on disk. Its output goes to `FLSubtitles\out\review_server.log`;
a start failure shows in a message box. This is `review_server.py --app` under `pythonw`. With arguments the console
stays (`review.cmd --open`: the same in this window, Ctrl+C stops it; `review.cmd --no-open`: the server only). It needs only Python's standard library (3.10+):
`review.cmd` takes the review venv `FLSubtitles\asr\.venv` (`uv sync` there), else FLvibed's
`research\tools\ocr\.venv`, else `py -3` / `python`. The environment's launch config `subtitle-review` starts the
same server. Options (`review.cmd --help`): `--check` (prerequisites + load the data, exit 0 = ready), `--port`,
`--open`, `--no-audio` (serves no clips: testing the page without sound), `--corrections <file>` (a scratch file:
Rebuild is refused there), `--reviewer`. What happens at start, and why:

| Check / behaviour | What it does |
|---|---|
| Prerequisites | Python 3.10+; the pipeline outputs it cannot run without, each missing one named with what makes it: `out/draft.tsv` (merge.py), `out/index.tsv` (build_index.py, needs the game install), `out/aligned.tsv` (align.py), `out/gamenames.tsv` (gamenames.py); `asr/glossary.txt`. Exit 2 when one is missing |
| One server per corrections file | an exclusive lock on `<corrections>.tsv.lock` (git-ignored; the system releases it when the process ends, crash included): a second server on the same file is refused at once (exit 3) instead of overwriting the first one's saves; the running one writes its address to `<corrections>.tsv.url` (git-ignored), so a second `--app` start opens that page instead (exit 0) |
| Open pages | each page says alive every 20 s (`/api/alive`) and bye when it closes (`/api/bye`, a beacon); `--app` stops once none is left (above); `/api/quit` (the Stop button) stops it at once, after any save or rebuild running. Console programs it starts (ffmpeg, the aligner) open no window |
| Port | 127.0.0.1 only (never the network), taken exclusively (Windows' default socket reuse would let a second server share it); when 8765 is busy, the next free port, said in the window |
| Name audit | `out/name_audit.tsv` ("Names the game doesn't know") is recomputed when it is older than the draft, the game text / names, the script or `name_audit.py`; Rebuild always recomputes it |
| Input files | every TSV is read defensively (`asr/tsvio.py`): bad bytes are replaced and a row with more fields than its header is reported with file and line (window + Stats), never a crash or a silent loss; a file missing a column the tool reads stops it with the file named |
| Writes | every decisions file (`corrections.tsv`, `samples.tsv`, `bulk_accepted.tsv`, `rereview_notes.tsv`, unsure.tsv) and every merge.py output is written to a temp file and swapped in: a crash never leaves half a file |
| Bad requests | answered 400 / 500 with the message; the server keeps running |

**Tests** (seconds, no game content, no audio, no server): `review.cmd test`. A synthetic project with the
repository's own rule files: fit rules, sentence joining against the runtime's C++ `JoinPieces` fixtures (read from
`<root>\modules\subtitles\src\impl\impl.cpp`) and the game's grammar, one actor's takes, saving / undo bytes, word timings and
suspects, "not sure" marks, the shared normalisers (`merge.squash`, `conv_form`, `strip_final_period`), the reader /
atomic writer, and the publish copy rule (the copies in `<root>\modules\subtitles\data\` match the products in HEAD and are
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
  the same hook publishes the products into `<root>\modules\subtitles\data\` within that commit (`tools/publish.ps1`; FLvibed's
  CLAUDE.md "Subtitle data"). New lines on the suspect list (`out/word_times_suspect.tsv`) are worth a look.
  Every aligned line also gets the CTC second opinion (two other aligners correct the words Qwen gets far wrong; how
  the timings are measured and what was tried: section 8).
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
- **The scene mixes** (29 Oct 2026, the owner; gf_sounds.ini `mix_*`, 19 room scenes as one track each, 5 s - 3.4 min)
  have their lines in `data/mixes.tsv` (same columns as movies.tsv, `mix` first; times on the mix's clock), drafted by
  `asr/mix_align.py`: the shooting script's lines of the mix's scene (speaker, spelling) matched utterance by utterance
  to NVIDIA Nemotron-3.5-Transcribe's speaker-tagged words (`--diarize`; runtime and model:
  `docs/RESEARCH.md` section 6); where the recording says other words, or speech is not in the script, the line carries
  the recording's words and a `[check]` note (the rules: the script's docstring). The aligner treats them like the
  movies (voice `mix`, audio `out/mixes/<mix>.wav` from the clip; `out/mix_word_times_suspect.tsv`,
  `out/mix_window_changes.tsv`). Rebuild the draft only before a review (it overwrites the file):
  `nemo-speech transcribe <16 kHz wav dir> -r --diarize -f json --output-dir <dir>` then
  `.venv\Scripts\python -I mix_align.py --script <shooting_script.txt> --heard <dir>`, then the aligner with
  `--movies-only --fix-windows` (`mix_align.py` refuses to overwrite reviewed lines without `--force`). **In game**:
  `<root>\modules\subtitles\src\impl\mixcues.cpp` shows them when the mix plays (29 Oct 2026; published with the data). **Reviewing them** (the tool's list "Scene mixes: their lines", or a mix in the Story order; `asr/mix_review.py`):
  each line is an item playing its window of the mix (Space, L loops it); edit text, speaker and window, Enter saves
  and marks it reviewed (column `reviewed`); "Use heard" / "Use script" fill the text box from the note; Split… (then
  click the word the second line starts with), Insert line after, Delete line, Fit window to words. Every edit
  re-aligns in the background (the aligner, `--movies-only`, incremental; the word strip refreshes when it is done);
  a scratch `--corrections` copy never runs it.
- **The timeline** (29 Oct 2026, the owner: alignment edited like Adobe Premiere's timeline): every line with audio shows
  its words as cards from start to end (keys and gestures: the page's help line and the comment above `tlInit` in
  `review.html`). Ctrl+S saves the line's timings to **`data/word_overrides.tsv`** (`asr/word_overrides.py`; keyed by
  the line and its text's hash), which wins over the aligner: `align_words.py` ships them in `data/word_times.tsv` on
  every run (`--overrides-only` does only that, the tool runs it after a save); its cache keeps its own rows, so
  "Back to aligner" always works. **An edited text** (a mix line's save, or a rebuild that changes a line's text) keeps
  the hand-placed timings: words whose letters are unchanged keep their start and length; changed or inserted words get
  an estimate in their gap (the old words' time, else between the kept neighbours; neighbours join when the gap is too
  small) and are listed in the row's `realign` column; the aligner then aligns only those words inside that gap of the
  audio and clears the list (`realign_overrides`). Lines timed only by the aligner are simply re-aligned whole.

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
| Bracketed sound cues ("[static]", "[sigh]", "[coughs]") | kept, SDH style (the owner, 31 Oct 2026: all 10 shipped, three of them cue-only lines). Not a "script direction": the shooting script's directions are parenthesised and worded differently ("(Distortion)", "(Coughing)"); ours are FLSubs' or the owner's own descriptions of what is heard |
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

## 5. State (main text review done 1 Nov 2026, 32a4e25d; the owner is on the scene mixes)

Every main review list is done: the last 8 script lines (speech to text heard other words: the actor's own
small words, "And she's LSF", "Ah, more of the same", "gonna"/"we'll") went to the owner's ear on 1 Nov
2026 (32a4e25d). Now: the scene mixes (305 lines, 164 `[check]`, 19 mixes; section 1), then the later
stages below. Word timings: section 8. Done by contextual passes (Claude reads every line with its context, fixes what context
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

## 7. The page's design (usability sweep, 31 Oct 2026)

The owner: "a lot of text in this tool ... not spaced properly ... a serious usability sweep", and a research round on
the best-received interfaces for this kind of work: three readers (subtitle and transcript editors; annotation and
game localisation tools; design guidance for dense expert tools), each citing its pages. What `review.html` applies:

| Rule | Source | In the page |
|---|---|---|
| One spacing scale; closer inside a group than between groups | Carbon / Atlassian spacing (4-8-16-24-32), NN/g proximity | `--s1`..`--s5` (4 / 8 / 12 / 16 / 24 px); a card's blocks 16 px apart, what belongs to the block above 8 px (`.tight`) |
| Few sizes, two weights | Carbon productive type set (12 / 14 / 16 px), Primer (line-height ~1.5) | 12 px labels and facts, 14 px context, 16 px the line and its reads; 400 / 600 |
| Speaker names in a column of their own, once per run | screenplay / transcript practice; BBC uses "NAME:" inline only where there is no column | every list of lines (`linesHtml`): [time] \| NAME \| text; the name blank for the same speaker's next line (tooltip keeps it) |
| Status apart from metadata; no pill for every field | Atlassian lozenge vs tag vs badge, Carbon tags | voice / length / source as plain text (`.facts`); lozenges only for the line kind, the review flag, "not sure" |
| Diffs: one quiet tint, not colour alone | WCAG 1.4.1 / 1.4.3; Primer's muted attention colour; Rev / Sonix / SyncWords' subtle confidence tints | one `<mark>` per run of changed words, amber tint + underline, theme-aware (`--diff`) |
| Show only what carries information | Aegisub's columns appear only when used | a read equal to read 1 says "same as 1" (still pickable) |
| Shortcuts: on the controls, a short strip, the full list on demand | NN/g accelerators ("a full list is a last resort"), GitHub `?`, Descript Ctrl+/, Amara's always-visible strip | `?` (or the header's Keys) opens the grouped overview; each view shows its 6-7 keys under its controls; the 40-key paragraph is gone |
| Long warnings don't push the work down | - | a note folds to three lines, a click shows all |
| Start and stop like a web page | the owner | section 1 (`review.cmd`, Stop button, auto-stop) |

**Workflow additions** (the owner said build them, 31 Oct 2026):

| What | Source | How it works |
|---|---|---|
| The agreed read comes pre-selected | Prodigy's review recipe (it pre-selects the majority answer) | each read shows which others it matches: `= 3 4` same text, `≈ 2` same words (case / punctuation differ). The read with the most agreement is selected (exact counts double; a tie keeps the lower number), so Enter picks it. A line already saved pre-selects its saved read; with no agreement nothing is selected. Prodigy's warning holds: a mistake every read shares slips through, so listen first |
| Your last decisions beside the work | Prodigy's sidebar (the last 10 decisions, reopenable) | up to 10, newest first, per browser (localStorage `flsubs-review-recent`); a click reopens the line; undo takes the newest off. Beside the cards from 1380 px wide, below them on narrower screens |
| Click a word card to play from it | Rev, Auphonic, Descript | a plain click (no drag, no modifier) plays from that word while the clip is paused. While it plays, a click only selects, so selecting cards for K stays as it was. The timeline already follows the playhead |
| F / G: the first / last half second of the speech | Aegisub's start / end of selection (its `e` / `d`; ours are taken: E edits, D slows) | the speech is the placed words (first start to last end), a mix line's window, else the clip. The stop is checked every frame (a hidden tab's frames slow to about one a second, so it overshoots there) |
| Why it needed a change (optional) | Netflix logs every fix with its reason | C cycles misheard, spelling, name, punctuation, other, none (or click a chip). The pick or typed text is saved with `reason: <why>` in the decision's note column; it shows which kind of mistake the drafts make most. The note is "; "-separated parts: merge.py and the server look for `kept exactly as typed` as one part, so a reason beside it changes nothing (unit tests) |

Answering before the clip ends was already possible (Common Voice users ask for it).

**Moving freely along a line** (the owner, 1 Nov 2026: "I can't freely move along the timeline of a line"): **Space**
plays / pauses from the playhead, as in every player and editor (it used to replay from the start: that is **R** now,
a mix line's window from its start); a click on the timeline's empty track (also inside a mix line's window) or on
its ruler moves the playhead there, playing or paused as it was (a drag on the track still selects cards); **,** /
**.** step it 0.25 s (**Shift**: 1 s); the playhead's time shows in the timeline bar. A mix line stops at its
window's end only while its window plays (R, ▶ Window, opening the line); after any other seek it plays on, so the
speech around a window can be heard. All of this needs the server to answer byte-range requests for the clips
(`send_media`, 206 Partial Content): served whole, a browser cannot seek in the audio and every seek - the player's
bar included - fell back to 0 (fixed 1 Nov 2026; a test page with in-memory clips had hidden it). **Split** opens the new second line, which plays from its first word: the cut is
that word's start (aligned or placed by hand; an even share of the window only when the line has no timings yet).
**Open (the owner, 1 Nov 2026, not built):** "Splitting lines is obtuse - I shouldn't be forced to remember to handle
overlaps, newly split lines need to be properly separated by default." The plan: Split cuts in the pause between the
two words (the clip's envelope, `review_server.peaks`; the word ends `ends_ms` of section 8 give the gap); saving or
dragging a window moves an overlapping neighbour's edge itself (a rolling edit, at least 0.1 s apart) and says so in
the status line. Where: `mix_review.py` (`split`, `save`), the window drag in `review.html`; tests in
`tests/test_mix_review.py`; check on the scratch copy (`subtitle-review-test`, `--no-audio`), then restart the owner's
tool (Stop, `review.cmd`).

Key sources: carbondesignsystem.com/elements/spacing/overview, carbondesignsystem.com/elements/typography/type-sets,
atlassian.design/foundations/spacing, atlassian.design/components/lozenge/usage, primer.style/product/primitives/typography,
nngroup.com/articles/gestalt-proximity, nngroup.com/articles/ui-accelerators, w3.org/WAI/WCAG22/Understanding/use-of-color,
bbc.co.uk/accessibility/forproducts/guides/subtitles, partnerhelp.netflixstudios.com (English USA Timed Text Style Guide),
prodi.gy/docs/review, prodi.gy/docs/api-web-app, humansignal.com/blog/building-a-better-ui-for-audio-transcription-at-scale,
aegisub.org/docs/latest/timing, help.descript.com (keyboard shortcuts, correct mode), auphonic.com/blog/2024/03/21/new-auphonic-transcript-editor,
softpicker.com/best-free-subtitle-editors (Subtitle Edit "crowded with options"). Pages behind logins or that did not
load (G2, EZTitles, Amara support, Rev support) were used only through search summaries.

## 8. Word timings: how good they are (1 Nov 2026)

The owner: the forced alignment is "sometimes baffling: some words get almost no time at all, others get too much or
start too early or too late"; "the major concern is overall alignment issues across all lines" (their own hand-placed
lines are "only mostly" right too). Two measures, both in `FLSubtitles/aligner/` (standard library, any venv):

- **`bench_words.py`** - against the owner's hand-placed timings (`data/word_overrides.tsv`; grows by itself with every
  line timed on the timeline). Start / end error, share within 50 / 100 ms, near-zero words. `--split` scores the words
  the owner **moved** (where the aligner was wrong) apart from those **kept** (left where the aligner put them: they
  score 0 for that aligner by construction, so read the shares there, not the mean). On 1 Nov: 10 lines, 111 words, all
  hard cases (Battleship Unity under a klaxon, one scene mix); 43 moved, 68 kept.
- **`health_words.py`** - every line, no ground truth: the symptoms the owner sees, per line kind (unplaced, stacked,
  near zero < 40 ms, long > 180 ms per letter and > 0.9 s, last word < 60 ms, lines with any).

**What was wrong** (found with these): Qwen3-ForcedAligner puts a line's last word on the clip's very end when the speech
runs to the last sample (the game's clips are cut tight: 15 % of all lines had a last word without length), places some
words out of order or not at all, and the pause snap moves a soft word's start to just before its own end when a noisy
floor (scene mixes) makes the whole word look like a pause (+300..+500 ms, zero length).

**What was tried** (benchmark: all words; mean / p90 start error ms, share within 50 ms; moved words within 100 ms):

| Candidate | mean | p90 | <= 50 | moved <= 100 | Verdict |
|---|---|---|---|---|---|
| Qwen pipeline (shipped until 1 Nov) | 80 | 310 | 69 % | 40 % | the closest single aligner |
| Qwen raw word spans (no post-processing, best window) | 112 | 280 | 49 % | 51 % | starts ~75 ms early: the pause snap helps |
| wav2vec2-base-960h CTC (WhisperX's English default) | 157 | 246 | 42 % | 60 % | late, fails after the klaxon |
| wav2vec2-large-960h-lv60-self CTC | 200 | 418 | 46 % | 49 % | loses the Unity line entirely |
| MMS-300m forced aligner CTC (ctc-forced-aligner, torchaudio MMS_FA) | 130 | 307 | 32 % | 44 % | early |
| Montreal Forced Aligner 3.4.3, english_us_arpa | 126 | 310 | 37 % | 44 % | worse; 8 GB environment |
| Qwen + snap fix / last-word fix / near-start spreading | 84 | 220 | 58 % | 51 % | fixes moved words, breaks kept ones |
| Median of 4-5 aligners (bias fitted leave-one-line-out) | 90-97 | 222-258 | 43-57 % | 49-56 % | same trade-off |
| **Qwen + CTC second opinion (adopted)** | **71** | **220** | **69 %** | 42 % | kept 99 %, near zero 6 -> 0 |

On every line (`health_words.py`, the cache before vs the same rows with the second opinion; 25,357 lines):

| | lines with any symptom | unplaced | near zero | long | last word < 60 ms |
|---|---|---|---|---|---|
| before | 24.3 % | 2.5 % | 3.2 % | 2.4 % | 15.1 % of lines |
| after | **8.5 %** | 0.9 % | 0.0 % | 1.5 % | 0 % |

On 400 random dialogue lines (150 story, 150 chatter, 100 unique) Qwen agreed with the two CTC aligners (where they agree
with each other) within 100 ms on 89 % of words, 182 words off by > 200 ms; with the second opinion 95 % and 4 (this
agreement is partly circular - the CTC aligners are the judges - so it only shows where Qwen was far off; the health
counts and the owner's lines are the independent measures).

**The second opinion** (`align_words.py` `SecondOpinion`): MMS-300m and wav2vec2-base-960h (CTC character models
through the aligner venv's transformers; 1.2 GB + 0.36 GB in `.cache/hf`, downloaded on first use) align the same
text; where both agree within 120 ms and Qwen's start is more than 200 ms away or unplaced, the word takes their mean
(6.8 % of all starts); every word then gets at least 60 ms in order, and a last word an end after its start (the CTC
end, inside the clip). Movies and mixes get it on the line's window +-0.1 s. Fails closed (models missing: Qwen's
timings, said once); `--no-second-opinion` turns it off. Cost: about 9.5 minutes for all dialogue lines on the RTX 5090
on top of Qwen's 7 (a few seconds for the pre-commit hook's changed lines). Output format and the incremental run are
unchanged (rows aligned before stay until `--all`; the hand-placed timings still win).

**Re-aligned everything** on the owner's word (1 Nov 2026, `align_words.py --all`, 1,022 s for 24,957 dialogue lines,
95 movie and 309 mix lines; mix windows unchanged): the shipped rows now measure as the "after" rows above (lines with
symptoms 8.5 %, mean start error on the owner's lines 72 ms); lines with 2+ words unplaced 1,048 -> 246; the
hand-placed timings ship unchanged.

**The owner's hand-timed mix lines switched to the new alignment** (the owner, 1 Nov 2026: "use this new alignment data
on my hand-placed ones"): the 9 lines of `mix_s002xe` now ship the aligner's timings (their rows left
`data/word_overrides.tsv`); they stay the benchmark's ground truth in `aligner/bench_truth.tsv`, and the aligner rows the
owner started from in `aligner/bench_ref.tsv` (`--split`'s reference, since the cache now holds the new rows).
Battleship Unity (`DX_M04_0217_UNITY`) followed on the owner's word, although the new alignment still squeezes its
klaxon section ("order!" 60 ms, "All" 1.35 s, "I" 430 ms early): no hand-placed timings ship now; Unity's stays in
`bench_truth.tsv` (a timeline edit in the review tool can put any line back). Quirk seen on the way: two lines of one mix with the same text (same
hash) share one word-time row under `--overrides-only` (`mix_s010x`; the runtime finds mix lines by text hash, so it
cannot tell them apart either).

**Open, for the owner:**
- **The owner's placements sit 100-300 ms after the speech onsets** on many scene-mix words (frames of the speech-band
  envelope: the owner's start falls at the end of the word's first loud burst, the aligners' at its beginning), while on
  the Unity line the owner kept the aligner's starts. The tool's playback and the aligner decode the same samples (the
  clip's raw MP3 vs ffmpeg's WAV decode: identical), so the likely cause is audio output latency on the listening side
  (Bluetooth headphones add ~150-250 ms the page's playhead does not show). Worth checking before more hand timing: if
  so, every hand-placed start is late by that much in game, where the sound has no such delay.
- **The benchmark has only hard cases.** A dozen ordinary dialogue lines timed on the timeline (story, chatter) would
  make `bench_words.py` representative of the 25,000 lines that matter most.

**Pauses shown as gaps** (1 Nov 2026, the owner's go; before, the timeline drew a word up to the next one, so a pause
between sentences looked like a long word): rows carry a last column **`ends_ms`** (the runtime reads columns 1-6 only;
every reader of the tool takes columns by name). `align_words.word_ends` ends a word where a measured pause (the clip's
noise floor + 8 dB, >= 0.15 s) begins that runs up to the next word, when that leaves the word 50 ms per letter (a soft
word in a noisy scene mix passes as "pause": "Officially" would have kept 80 ms) and a gap of at least 0.1 s; else the
word runs to the next one. Only the audio decides, no model; old rows got theirs on the next run (`fill_ends`, 89 s for
all dialogue). Tuned on the 400-line sample (50 ms per letter keeps 206 of 217 pauses); on all lines "long" words 1,942
-> 1,355 (story 740 -> 426), lines with any symptom 8.5 % -> 6.7 %; starts unchanged. Against the owner's hand ends
(drawn edge to edge) ends within 100 ms 74 % -> 71 %, as expected. The review tool shows them: `mix_review.timing`
takes the ends (`ms_list`), `review_server.word_info` and `MixLines.word_info` pass them; a line with no `ends_ms` (a
row written before) still runs each word to the next.
