# Fixing subtitles over the long term — workflow proposal

Proposal (session 2). Built since: the committed `subtitles.tsv`, the deterministic majority pick and section 7's
items marked done; the files and scripts it proposes (`draft.py`, `build.py`, `pack.py`, `base.tsv`, `lines.tsv`,
reports) do not exist yet. It covers three phases: **now** (local pipeline + review tool),
**after the runtime DLL** shows subtitles in game, and **after the public repo** has outside
contributors. Goal: fixing a line stays a one-minute job, and the text that ships never changes by
accident.

Settled conventions this builds on (RESEARCH.md 9–16 and the user's rules): story and mission lines
are kept as written; generic chatter/base speech are chunks that the runtime joins with the
Content.dll grammar and ends with "."; cut-off lines end with "–"; callsign numbers are digits; one
actor's takes share one text unless they differ by more than a couple of words; **any change to a
line the user reviewed goes back to the user** (Re-review).

## 0. Where we are (HDE 0.7.1, 16 Oct 2026)

| Draft source (out/draft.tsv) | Lines |
|---|---|
| speech to text only | 21,039 |
| FLSubs + speech to text merged | 3,797 |
| script (all sources agree or judged) | 1,871 |
| user correction | 740 (265 story rows with voice `*`, 473 chunk rows per voice) |
| copied from a review: actor's takes majority / same words / same recording | 178 / 87 / 30 |
| other | 8 |

27,750 clips. Re-review is empty, no line is flagged high/low any more. Observations that shape the
proposal:
- Only the decisions and rules are committed (`FLSubtitles/data/`). The draft and everything it is
  built from (script, speech to text, FLSubs, game text, audio) live in the ignored `out/`. Nobody
  but this machine can rebuild the subtitles, and there is no committed "what ships" file to diff.
  **Done (17 Oct 2026):** `FLSubtitles/data/subtitles.tsv` is the committed what-ships file.
- `out/` holds hand-made snapshots (`draft_before_style.tsv`, `draft_before_style2.tsv`,
  `draft_before_cont.tsv`, `draft_before_linefix.tsv`): regression checks are done by hand today.
- `corrections.tsv` stores the final text, but not against which draft, or what the user saw
  as the in-game sentence. (`when` and `by` columns are written on save since 24 Oct 2026.)
- The review server loads `draft.tsv` once at start: after a save, Re-review, propagation to other
  takes and the lists only update after `merge.py` + a server restart.
- `merge.py` line 836 picks the majority spelling with `max(set(spellings), key=spellings.count)`.
  With equal counts the winner depends on set order, which changes per process (hash randomization,
  `python -I`). Two runs can ship different punctuation for the same take. Fix before any snapshot
  test (sort, then pick). **Done:** the tie is sorted in `merge.py` (repeated-takes majority).

## 1. Principles

1. **Human decisions are the only authority.** Rules and machine drafts propose; a line someone
   listened to is never changed silently by anything.
2. **What ships is a committed file**, rebuilt deterministically from committed inputs. Every change
   to it is visible as a diff in the same commit that caused it.
3. **Every line knows how sure we are** (its tier), so reviewing time goes where errors are likely,
   and metrics are honest.
4. **Reports point at line ids**, never at screen positions or memory of what was said.
5. **Contributors need only the repo and their own game install.** The upstream sources (DVD script,
   speech-to-text model, FLSubs archives) stay a maintainer-only step that runs rarely.

## 2. Data model (a)

### 2.1 Layers

```
 L0 sources (local, never committed)           DVD script, speech to text, FLSubs, game text, audio
        │  stage A: draft.py  (maintainer only, rare: new source, new model, new game version)
        ▼
 L1 machine draft      data/base.tsv           one text per (id, voice) + source + tier, no decisions
        │  stage B: build.py  (anyone, seconds, deterministic, run by CI)
        │     + L2 decisions  data/corrections.tsv, auto_decisions.tsv, continuity.tsv, repeat_guesses.tsv
        │     + L3 rules      data/name_rules.tsv, style_rules.tsv, chatter_rules.tsv
        │     + game facts    data/lines.tsv, chatter_grammar.tsv, gamenames
        ▼
 L4 shipped text       data/subtitles.tsv      committed snapshot = the product (GPL-3.0)
        │  pack.py
        ▼
 release files         subtitles/en/*.txt      what FLvibed.dll loads (runtime columns only)
```

Today `merge.py` does A and B in one pass. Split it in two (same code, two entry points): `draft.py`
writes `base.tsv` from L0 (everything up to, but not including, corrections and rules); `build.py`
applies decisions, rules, the takes/copies passes and the chunk grammar to `base.tsv`. Nothing in
stage B may read `out/`.

### 2.2 What each file is

| File | Kind | Who decides | Scope | Trust |
|---|---|---|---|---|
| `corrections.tsv` | decision | a human who listened (user now; maintainers later) | one line (story: every copy, `*`; chunk: one voice, saved for all takes) | **verified** |
| `auto_decisions.tsv`, `continuity.tsv`, `repeat_guesses.tsv` | decision | Claude, with a reason per row | one line / one actor's takes | **judged** |
| `name_rules.tsv`, `style_rules.tsv` | rule | the user (conventions) | every line | applies to all tiers; changes to verified lines → Re-review |
| `chatter_rules.tsv` | rule | user's answers on mishearings | id/voice regex, chatter only | as above |
| `chatter_grammar.tsv`, `lines.tsv`, gamenames | game fact | read from game files | runtime joining, names, line list | facts; fixed by evidence, not taste |
| `rereview_notes.tsv` | workflow flag | anyone | one line | not shipped |
| `base.tsv` | machine draft | pipeline | every line | tier from its sources |
| `subtitles.tsv` | build output | nobody edits it by hand | every line | carries the tier |

New committed files: `lines.tsv` (id, voice, kind, speaker id, seconds, audio MD5, game build: no
audio, no text: the line list stage B needs instead of `out/index.tsv` and `audio_md5.tsv`),
`base.tsv`, `subtitles.tsv`. `base.tsv` holds the chosen draft text only, never the alternative
reads (raw speech to text, other sources) — the `.gitignore` rule against raw transcripts stays.

### 2.3 Tiers (one column in base.tsv and subtitles.tsv)

| Tier | Meaning | Today |
|---|---|---|
| `verified` | a human listened and saved it | 740 |
| `verified-copy` | follows a verified line: same recording, same words in another voice, or the actor's other takes | ~295 |
| `game` | text taken from the game's own string tables (bases, systems, factions, goods) | part of the 21k |
| `judged` | Claude decided with a written reason | auto/continuity/guesses |
| `agreed` | two independent sources agree (script + recording, FLSubs + speech to text) | ~7,500 |
| `single` | one source only (speech to text) | ~17,900 |

The tier is the queue order for reviewing, the denominator of the metrics, and the first thing a
report shows.

### 2.4 corrections.tsv: three more columns (additive, old rows stay valid)

`id  voice  text  source  note  when  by  heard`
- `when`: date of the save. `by`: `user` now; a GitHub handle later (no e-mail; the privacy hook
  already refuses them).
- `heard`: what the reviewer approved as it appears in game: the shipped text for story lines; for a
  chunk, the rendered sentence shown as "In game:" (the first one). Lets the build tell "the piece is
  unchanged but its sentence now reads differently" (grammar change) from a real text change.

### 2.5 The shipped file

`data/subtitles.tsv`, sorted by (id, voice), UTF-8, LF:

`id  voice  kind  speaker  text  tier  why`

- `kind` = `story` (shown as saved), `chunk` (joined by the runtime grammar, no final "."),
  `nn` (Neural Net message); `voice` = `*` where every copy reads the same.
- `why` = the decision or rule ids that shaped the text (for the diff report; stripped by `pack.py`).
- First line `# FLSubtitles data <version> · en · HDE 0.7.1 · GPL-3.0 · <sha256 of the body>`.
  The runtime logs the version in every report (section 4).
- `pack.py` writes the release form: `id voice kind speaker text` plus the compiled grammar and the
  speaker name table. The DLL hashes ids at load (CreateID), so the files stay human-readable.
- Later languages: `subtitles.<lang>.tsv` with the same keys; the English file is the key list.

## 3. Keeping decisions stable (b)

### 3.1 Invariants the build checks (`build.py --check`, also run by CI and by the review tool)

| # | Invariant | On failure |
|---|---|---|
| I1 | A `verified` line ships exactly what was saved (only the runtime-owned final "." of a chunk may differ) | the line goes to **Re-review**; release blocked |
| I2 | A verified chunk's rendered sentence still equals `heard` | Re-review ("sentence changed: grammar") |
| I3 | A `verified-copy` line equals its source line | build error (propagation bug) |
| I4 | The audio MD5 of a verified line is the one in `lines.tsv` (a game update re-recorded or replaced it) | Re-review ("recording changed") |
| I5 | `build.py` output == committed `subtitles.tsv` | check fails: run `build.py --accept` and commit the diff |
| I6 | Rule fixtures pass (`data/rule_cases.tsv`) | check fails |
| I7 | Two builds in a row are byte-identical (determinism; covers the `max(set(...))` tie) | check fails |
| I8 | Lint: no stage directions `(…)`/`[…]`, no doubled spaces/punctuation, no `...` without a word, chunk rows without final "." | check fails, lists lines |

I1 is today's Re-review rule, kept as is. I2 and I4 close the two holes it has: a chunk can be
untouched while the sentence the player reads changes, and the same id can get new audio.

### 3.2 "Only changes when intended": the snapshot test

`subtitles.tsv` is the snapshot. `build.py --accept` rewrites it and prints a classified diff:

```
build: 27,750 lines, 23 changed
  verified        0 changed            (any change here = Re-review entries, listed)
  verified-copy   2  ← corrections.tsv gcs_combat_brag_05- pilot_f_ill_m01
  single         21  ← style_rules.tsv \bok\b -> okay
  base.tsv        0
```

Rules for intent:
- Every changed line names its cause (decision row, rule row, base change). A change with no cause is
  a build bug.
- The commit that changes an input also commits the new `subtitles.tsv`; the diff summary goes into
  the commit message (now) or a CI comment on the pull request (public).
- A rule change that touches `verified` lines is allowed, but the same commit's Re-review list shows
  every one of them, and a release needs Re-review empty.
- A rule that changes more than ~50 lines gets a sample in the commit message (10 random before →
  after pairs), so a broad pattern bug is caught by reading, not by players.
- The hand-made `out/draft_before_*.tsv` snapshots retire: `git diff data/subtitles.tsv` replaces
  them.

### 3.3 Rule fixtures

`data/rule_cases.tsv`: `id  voice  input  expected  why`. One row per known trap, added whenever a
bug or user report is fixed, e.g. "for" next to "Fort Bush" must stay "for" (dx_m01_1042_king), Razor
1 must not take the battleship's "We're losing containment!", "Liberty...thanks" → "Liberty...
thanks", "Mister...?" alone stays. `build.py --check` runs every row through the same functions.

### 3.4 Re-review stays the user's gate

Re-review lists I1, I2, I4 and `rereview_notes.tsv`, one entry per actor (as now). Picking
"Conventions applied" accepts the rule; "Your saved text (keep exactly)" saves with
`note = kept exactly as typed` (as now). The new part: Re-review is computed on every save and every
rebuild (section 7, item 1), not only when `merge.py` runs, and a release cannot be packed while it is
non-empty.

### 3.5 When stage A runs again

New speech-to-text model, a new FLSubs edition, a fix in `align.py`, or a new HDE version: `draft.py`
writes a new `base.tsv`. The same snapshot diff applies, plus a base report: lines whose base text
changed, by tier. Verified lines are unaffected by definition (decisions sit above the base);
`agreed` → `single` downgrades and new disagreements go to the review lists. A new game version also
diffs `lines.tsv`: new ids → review list, removed ids → rows kept but marked unused, changed audio →
I4.

## 4. Reporting a wrong subtitle from inside the game (c)

### 4.1 In the DLL

- **Ring buffer of played lines**: the hooks record every voice line that starts, with or without
  text: time, voice, id(s) (a chatter comm = up to 16 piece ids), kind, the text shown (rendered
  sentence), the tier, and cheap context (mission id, system, base/room, cutscene name).
- **Report hotkey** (FLSubtitles.ini, default a key free in Freelancer's default keymap and
  FLController's bindings; a pad combo later if FLController exposes one): appends the current
  subtitle, or the last one if none is on screen, plus the two before it, to the reports file, and
  shows a 2 s toast "Reported: <first words>". No typing in game; the reviewer decides what is wrong
  with the audio in front of them.
- **"No subtitle" reports** come for free: a reported entry with no text is a missing line (an
  unhooked path or an id without a row).
- **Debug overlay** `show_ids = 1`: prints `id · voice · tier` under every subtitle. For the user's
  test runs and for contributors writing an issue.
- **Reports file**: `<the game's user-data folder>\FLSubtitles\reports.tsv` - Common.dll
  `GetUserDataPath` (vanilla `Documents\My Games\Freelancer`; HDE's flplusplus may move it, the FLvibed test
  install keeps it in its own `SAVE\`); the EXE folder may sit in Program Files and not be writable. One row per reported line:

  `time  data_version  game_build  kind  voice  ids  shown  tier  context  seconds_since_start`

  No paths, no account or player names (privacy hook rules apply to anything that may be pasted
  into an issue).

### 4.2 In the review tool

- A **Reports** list at the top of the menu (above Re-review). The server reads `reports.tsv` from
  the configured path on start and on "Reload", copies new rows to `out/reports_seen.tsv` (local
  workflow state, not committed) and groups them by (id, voice).
- Each report opens the normal review page for that line, with the shown text as an extra read
  ("Shown in game") and a status:
  - **already fixed**: the current shipped text differs from what was shown (a newer data version);
    one key confirms it and closes the report.
  - **chunk**: the sentence is shown with its pieces; the reviewer fixes the wrong piece (the
    existing In-game preview already plays the sentence from the same voice's clips).
  - **missing**: no text was shown → goes to the DLL's to-do list (`docs/RUNTIME_GAPS.md` or an
    issue), not to corrections.
- A save from a report writes `source = report <date>` into `corrections.tsv`; the report closes.

### 4.3 For players (public)

Same hotkey; the README says: attach `reports.tsv` (or paste its rows) to a "Wrong subtitle" issue.
`import_reports.py` turns the pasted rows into the same Reports list for the maintainer.

## 5. Public repo: contributors (d)

### 5.1 What a contributor needs

The repo (stage B inputs, `build.py`, the review tool) and their own Freelancer + HDE install. The
review tool gets a **game-install mode** (`review_server.py --game "<install>"`): it reads clips from
the player's `.utf` banks and `DIALOGUE\` WAVs via `lines.tsv` (bank + CreateID hash) instead of
`out/clips/`. No audio, script or transcript ever enters the repo. Alternative reads (script, raw
speech to text, FLSubs) are not available to contributors; they review by ear against the shipped
text, which is also how players will hear it.

### 5.2 Three ways to propose a fix, smallest first

1. **Issue form "Wrong subtitle"** (GitHub issue template): line id + voice (from the debug overlay or
   a reports row), shown text, proposed text, where it was heard. No setup needed. A maintainer
   listens and saves it in the review tool (`by` = the reporter's handle, with consent in the form).
2. **Pull request with corrections**: the contributor saves lines in the review tool (game-install
   mode). Their saves go to `data/proposals/<handle>.tsv` (same columns as corrections), never
   straight into `corrections.tsv`.
3. **Pull request with a rule or grammar change**: allowed, but needs the CI diff and must name the
   evidence (game text, Content.dll address, several clips).

### 5.3 Review and acceptance

- **CI** (GitHub Actions, Python only, no game files): `build.py --check` on the PR with the proposals
  applied as if accepted, posts the classified diff (section 3.2) as a comment, fails on lint, fixture
  or determinism errors, and flags any proposal that touches a `verified` line ("changes a line the
  maintainer verified: needs a listen").
- **Maintainer**: the review tool's **Proposals** list loads a branch's `data/proposals/*.tsv`; each
  item shows current vs proposed text with the clip. Accept moves the row into `corrections.tsv`
  (`by` = contributor, `source = proposal #<PR>`), reject leaves a one-line reason. The merge commit
  contains the accepted rows and the new `subtitles.tsv`; the proposals file is emptied.
- **Trust ladder**: two independent listens make a line verified. A first-time contributor's accepted
  line is verified because the maintainer listened; after ~50 accepted lines, a contributor can be
  made a reviewer whose saves count as the listen (the maintainer still sees the CI diff).
- **Story/mission lines** stay "as written": proposals there must quote what the actor says, not
  improve style. Conventions changes (style_rules) remain the maintainer's call.

### 5.4 Licensing and content rules

- **Subtitle data is GPL-3.0** (FLSubs text is in it; wedge009.net CC BY-SA 4.0 is one-way compatible).
  `data/LICENSE` (GPL-3.0 text) + `data/README.md` naming the sources. Contributions are accepted under
  the same license (inbound = outbound), with a DCO `Signed-off-by:` line on commits; no CLA.
- **Code license**: the runtime and tools contain no FLSubs code, so they could be MIT; one GPL-3.0
  license for the whole repo is simpler for contributors. Decision for the user before going public.
- **Microsoft notice** in README and on the release page (RESEARCH.md 5), unchanged.
- **Never in the repo or an issue**: audio, game files, DVD script text beyond the spoken words, stage
  directions, raw transcripts. CONTRIBUTING.md says so; lint I8 catches stage directions.

### 5.5 Credit

- `CREDITS.md`, generated by `pack.py`: the source projects (Lorash for FLSubs, L@Zar0, the Russian
  edition's editor, wedge009.net and the transcript sites FLSubs cites), tools (Qwen3-ASR, Apache-2.0),
  and every `by` handle with its count of accepted lines, plus players whose reports led to fixes
  (opt-in in the issue form).
- Release notes list the contributors of that release and the counts from the diff summary.

## 6. Quality metrics and release checklist (e)

### 6.1 Metrics (`build.py --stats`, shown on a review-tool Stats page and in release notes)

- **Coverage**: lines with text / lines in `lines.tsv`, per kind (story, mission comms, named
  characters, base speech, chatter pieces), counted by line and by audio minutes.
- **Tier mix** per kind: verified, verified-copy, game, judged, agreed, single. Target for the first
  release: story + mission comms 100 % verified or agreed; chatter shown only as it is.
- **Estimated error rate** (the honest accuracy number): before each release, a random audit sample
  of ~50 lines per tier (stratified, `--audit-sample` makes a fixed review list). The reviewer marks
  each right/wrong; errors / sample = estimated error rate for that tier, multiplied by its size =
  estimated wrong lines. Wrong lines found are fixed and become verified. This also tells whether a
  tier is worth a full pass (e.g. if `single` chatter is 8 % wrong, it is).
- **Open work**: Re-review count (must be 0), review lists' counts, open reports and proposals,
  unknown names (name audit), lint warnings.
- **After release**: reports per 1,000 lines played (from reports + the DLL's optional counter), and
  repeats of already-fixed lines (players on old data).

### 6.2 Release checklist

1. `build.py --check` clean: I1–I8, fixtures, determinism.
2. Re-review empty; open reports triaged; story/mission review lists empty.
3. Audit sample done for this release; error rates in the notes.
4. `subtitles.tsv` committed; version bumped (`major.minor.patch`: patch = text fixes, minor = new
   lines/kinds or rules, major = file format change the DLL must know).
5. `pack.py`: release files, compiled grammar, speaker table, CREDITS.md, LICENSE, Microsoft notice;
   hash recorded in the header.
6. DLL loads the packed data (format version check), test install smoke run with `show_ids = 1`: the
   M01 opening cutscene, one mission comm, one bar conversation, one trade-lane chatter exchange, one
   dock hail. No screen coordinates; checked from the DLL log.
7. Content check: no stage directions, no cut lines, no audio or game files in the package.
8. Release notes: diff summary by tier, metrics, contributors. Tag locally; push only when the user
   says so.

## 7. Review tool: near-term improvements (f)

Ranked by value for the effort. Items 1–4 help the current review passes; 5–6 prepare for the DLL.

| # | Improvement | Value | Effort |
|---|---|---|---|
| 1 | **Done.** **Rebuild in place**: after each save, re-run the build for the affected lines (or a "Rebuild" button that re-runs `merge.py` in-process and reloads `draft.tsv`), so Re-review, takes/copies propagation and list counts are current without a restart | high: removes the merge + restart round trip and stale lists | low–medium |
| 2 | **Done.** **Deterministic majority pick** (`merge.py` line 836) and a **draft diff command** (`merge.py --diff <old draft>` printing the classified changes of section 3.2): the first two pieces of the snapshot test, usable before `subtitles.tsv` exists | high: stops silent flip-flops, retires the hand-made snapshots | low |
| 3 | **Partly done** (`when` / `by` since 24 Oct 2026; `heard` not). **`when` / `heard` columns on save** (section 2.4); the tool writes them, merge ignores unknown columns | high long-term (audits, I2) | low |
| 4 | **Done.** **Word-level diff highlighting** between the reads, and **0.75× playback / loop** for hard clips | medium-high: faster, fewer missed one-word differences | low |
| 5 | **"Not sure" button**: saves a flag with a note (`rereview_notes.tsv`-style, `data/unsure.tsv`) instead of text, for a second listen later | medium | low |
| 6 | **Done.** **Undo last save** (Ctrl+Z restores the previous row; git keeps the full history anyway) | medium | low |
| 7 | **Stats page** (`/api/stats`: tiers, coverage, open lists) | medium | low |
| 8 | **Reports list** (section 4.2) | high once the DLL exists | medium |
| 9 | **Game-install audio mode** + **Proposals list** (section 5) | needed for the public repo | medium–high |

## 8. Roadmap

| Phase | Do |
|---|---|
| Now | items 1–4 of section 7; fixtures file with the known traps; tiers computed in the draft |
| Before the DLL's first data file | split `merge.py` into `draft.py` + `build.py`; commit `lines.tsv`, `base.tsv`, `subtitles.tsv`; `build.py --check`; `pack.py` format v1 |
| With the DLL | ring buffer, report hotkey, `show_ids`, reports file; Reports list; the smoke run in the release checklist |
| Before going public | game-install mode, Proposals list, CI workflow, issue template, CONTRIBUTING.md, LICENSE decision, CREDITS.md generation, first audit sample |

## 9. Decisions for the user

1. Commit `base.tsv` and `subtitles.tsv` now (they are the product; the CLAUDE.md copyright rule
   allows curated subtitle files once the format exists)?
2. Repo license: GPL-3.0 for everything, or MIT code + GPL-3.0 data?
3. Report hotkey default (and whether a pad combo through FLController is wanted).
4. Who may become a reviewer whose saves count as verified, and after how many accepted lines.
5. Whether `single`-tier chatter ships on by default or behind an option until its audit error rate is
   known.
