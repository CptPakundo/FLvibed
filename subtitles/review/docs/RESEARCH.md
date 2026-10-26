# Freelancer Subtitles — research (session 1)

Static research on the HD Edition 0.7.1 data (read-only, from the controller mod's test install) and the
web. Nothing here is verified in game yet (`<root>` = the FLvibed repository root); game facts that hold for every mod went to
`<root>\docs\FREELANCER_RE.md` ("Voices and dialogue audio").

## 1. Does the game ship any dialogue text?

**No, not for spoken lines.** Checked:
- All string tables and infocards of the 8 resource DLLs (`<root>\tools\dump-resources.ps1`, 18,120 entries):
  no voiced dialogue. They do hold the text of everything the game already *shows* as text: news
  (e.g. ids 15001+), bar rumors (infocards in MiscText, e.g. 131196, 393427), mission offers, objectives,
  popups, Neural Net logs.
- Mission scripts (`DATA\MISSIONS\Mxx\*.ini`): only line ids (`Line = escort, Player, DX_M01_0100_KING`,
  `Act_SendComm`, `Act_EtherComm`), plus speaker-name string ids for EtherComm (e.g. 196773).
- Cutscene scripts (`.thn`, `DATA\SCRIPTS\STORY`), voice INIs, `story_sounds.ini`, FLDOCS, JFLP docs:
  ids, timings and file names only. Voice banks contain only audio.

## 2. Existing transcripts and earlier subtitle mods

| Source | Coverage | Notes |
|---|---|---|
| **FLSubs** by Lorash (László József Csöndes, Starport, 2014–16) | the engine; little text of its own | GPL-3.0. Inspected, see 2.2 |
| **Freelancer Subtitles** by L@Zar0 (FLSubs update, 2019) and a Russian edition (FreelSUBS_RUS_EXTRA, 2023–24) | **9,782 lines** (forum posts undersold it): all cutscenes, mission comms and base speech | GPL-3.0. Inspected, see 2.2 |
| wedge009.net mission transcripts (missions 1–13) | story + in-flight comms with speaker names, no line ids | CC BY-SA 4.0 (one-way compatible with GPL-3.0); one of the FLSubs data sources |
| FL_XFMod wiki "Transcripts" (fandom) | Prelude + Mission 1 only (~100 lines) | community transcription; page text CC BY-SA, but the dialogue itself is the game's |
| Freelancer wiki mission pages (fandom) | prose walkthroughs, a few quotes | not a script |
| **Freelancer Bonus DVD** (2003; archive.org item `freelancer-2003-audio-cd-ost-plus-bonus-dvd`) | **the shooting script with line ids**, see 2.1 | Copyrighted Microsoft material; a local copy lives in `<root>\research\BonusDVD\` (ignored by git) |

### 2.1 Bonus DVD shooting script (inspected)
The DVD's data part (`extra_bonus_assets\docs\`) has `story_scripts(shooting)\shooting_script.doc`
(2 MB Word file, dated Oct 2002; converted to UTF-8 text with Word, 17,016 lines), character vignette
scripts (for the DVD's vignette MP3s, not in the game), `story_scripts(early)\Freelancer - First Draft.txt`
(early draft, not useful) and `fl_opening_narration.doc`.

Format: `SCENE:` headers (157: space RTCs, room scenes such as `S006X: PITTSBURGH - LANDING PAD`,
offer/decline/reoffer scenes), camera directions, then one block per line:
```
<LINE ID>          e.g. DX_M01_0010_FREEPORT_7_BRIDGE or DX_S006X_0101_KING.WAV
<SPEAKER>          e.g. FREEPORT 7 BRIDGE, KING (O.S.), LIBERTY NEWSCASTER (VO)
(<delivery note>)  e.g. (Filter - very business like)   [optional]
<text>             may contain (A beat), [Death Scream], [cruiser contingent...]
```
**Coverage against HDE 0.7.1** (ids normalized: uppercase, `.WAV` stripped):

| Game lines | Exact id in script | + same line number, other speaker tag | Not in script |
|---|---|---|---|
| Story sounds (`dx_s*`, 962) | 878 (91 %) | +44 → 922 (96 %) | 40 |
| Mission comms `DX_M*` + recognizable `DX_*` (1,416) | 1,254 (89 %) | +106 → 1,360 (96 %) | 56: mostly later pickups (`_0386B`, `_0640C`), M06 has the most |

The script also has 780 ids the game doesn't use (cut or renumbered lines). **Not covered at all:** GCS
base speech and space chatter (~21,000 fragments), and generic named-voice lines without DX ids.
**Not checked yet:** whether the text matches the final recordings. Actors and later rewrites may
differ from an Oct 2002 script. Next step: compare a sample against speech-to-text output.

So the story + mission text exists for ~96 % of lines (speaker names included). It is our main source
for those lines; only the spoken text goes into the shipped subtitles (see 5).

### 2.2 FLSubs / "Freelancer Subtitles" (inspected, session 1)
The user supplied `Freelancer English Subtitles.zip` (L@Zar0, 14 May 2019: tool + source + data) and
`FreelSUBS_RUS_EXTRA.rar` (2023–24: Russian and the same English data, translated resource DLLs, and a
"ForWeakPC" low-detail asset pack). Both sit in the project root, ignored by git; they were inspected as
data only (nothing run).

**How it works**
- A proxy `d3d8.dll` loads the *System32* d3d8.dll directly, which bypasses HD Edition's own d3d8
  wrapper. It hooks DirectSound buffers (Create/Duplicate/Lock/Unlock/Play), keeps a copy of each
  buffer's decoded PCM, and at `Play` looks the line up by CRC32C of that PCM plus its size.
- `Converter.exe` (C#) decodes each listed clip with Windows ACM, computes the same key and compiles
  `subtitles.xml` → `flsc.dat`/`flsd.dat`. Draws with Direct2D and needs `LOCKABLE_BACKBUFFER = true`.
- Text areas are fixed pixel boxes for one chosen resolution (`subtitles.cfg`, 4:3 recommended). The
  converter must be re-run after every change.
- It also replaces `Freelancer.exe` (stack size raised to 2 MB) and ships its own JFLP-based mission
  INIs, .thn scripts, story_sounds/gf_sounds INIs and some re-cut audio (split "b" clips, dummy mixes) to
  improve sync. Installed over HD Edition these would revert HDE's own fixes; the mission INIs already
  differ (factions, names, waypoints).
- Result: not usable as a runtime for us: it fights HD Edition's renderer and files, has a fixed
  layout, and an ENB/ReShade d3d8 conflicts with it. Its idea (identify a line by its audio) works
  without any game knowledge, but our id-based hooks are exact.

**The data (`subtitles.xml`)**: entries keyed by audio file (`DIALOGUE\TRENT\dx_s004x_0401_trent.wav`)
or bank + hash (`rvp151.utf|0x80062682`), so they map 1:1 onto our index. Static entries show for the
whole clip; Dynamic entries have timed parts (`<Part Start="0.05" End="5.48">`), including the scene
mixes. English and Russian measured against our index:

| Group | Lines in game | EN covered | RU covered |
|---|---|---|---|
| Cutscene sounds | 981 | 972 | 973 |
| Mission comms (`DX_*` in banks) | 1,416 | 1,415 | 1,415 |
| Named characters (recognizable) | 1,359 | 1,274 | 1,280 |
| Bar/base speech (base_male + base_female) | 5,256 | 5,256 | 5,256 |
| Space chatter (space_male + space_female) | 15,970 | 865 | 865 |
| Orphan bank clips | 2,768 | 0 | 0 |

- English accuracy: 2.8 % word difference from our script-aligned text (9,585 words), 6.1 % from the
  transcription of the recordings (34,953 words; that figure also contains our model's own errors).
- The Russian text prefixes speakers ("Диспетчер: ..."). The English text doesn't.
- The English data cites community transcripts as sources (wedge009.net missions 1–13, allreadable.com,
  voidserver.com).
- License: the readme puts the tool under GPL-3.0 ("free and can be used or even modified by anyone").
  The data has no separate license, so treat it as GPL-3.0 too.

**What it is good for (pending the user's call on licensing)**: a human transcription of all 5,256 base
speech fragments and 865 space-chatter lines (where no script exists), timings for the scene mixes, a
third opinion in the review list, and Russian/Spanish text for later language options. Shipping any of
its text makes our subtitle data GPL-3.0 and needs credit to Lorash and L@Zar0 (and the Russian
editor). Using it only to check our own text avoids that.

## 3. How a line id maps to audio (verified statically, every line resolves)

Two separate systems:

**A. Voice banks**: mission comms, Trent's lines in flight, ATC/docking, NPC pilot chatter, bar/base GCS
speech.
- `EXE\freelancer.ini [Data] voices = ...` lists `DATA\AUDIO\voices_*.ini`. `[Voice] nickname = king`
  (or `extend = king` to add lines to an existing voice), then `[Sound] msg = DX_M01_0100_KING`
  (+ `attenuation`, `duration`, `priority`).
- Audio: `DATA\AUDIO\<voice nickname>.utf`. Each child of the UTF root is named `0x%08X` =
  `CreateID(msg)` (Freelancer's nickname hash, `<root>\tools\flhash.ps1`), and each leaf is a complete RIFF WAVE
  file (MP3 in WAV, format 0x55, 11/22 kHz mono). Checked: **24,002 of 24,002** msg entries resolve.
- 114 banks, 26,769 clips, **~758 minutes** of audio. Most of it is short fragments.

**B. Story sounds**: cutscenes and room scenes.
- `DATA\AUDIO\story_sounds.ini`: `[Sound] nickname = dx_s006x_0002_King, type = voice,
  file = Audio\dialogue\king\dx_s006x_0002_King.wav` (961 voice entries).
- `.thn` scripts: a SOUND entity with `template_name = dx_s006x_0002_King`, then
  `{ 5.109, START_SOUND, { "dx_s006x_0002_King_9" }, { duration = 10.703 } }`.
- 999 WAV files in `DATA\AUDIO\DIALOGUE\<speaker>\`, **~52 minutes**.

**Speaker names**: the id suffix names the speaker (`..._KING`, `..._STATION_NEWARK`,
`dx_s039x_0101_hakkera`). The voice nickname names it too (`king`, `m01a_newark`, `pilot_f_leg_m01`).
EtherComm lines carry a name string id. Story ids are ordered by scene and line number
(`dx_s<scene>_<line>_<speaker>`, `DX_M<mission>_<line>_<speaker>`). The display name is a small curated
table (about 100 named voices); generic voices get their role ("Liberty Navy pilot") or the sending ship's
in-game name.

### Lines by dialogue type
| Type | Source | Count | Text on screen today? |
|---|---|---|---|
| Story cutscenes, room scenes | story sounds (B) | 999 clips, 52 min | no |
| Mission comms (`DX_Mxx_*`) | `voices_mission01..13.ini` (+ extends of king, juni, trent_voice, ...) | ~1,420 lines | no |
| Named characters outside missions | `voices_recognizable.ini` | 1,359 | no |
| Bar / base GCS speech (greetings, offer intros, "know"/rumor intros) | `voices_base_male/female.ini` (`GCS_ML_*`) | 5,256 fragments | **rumors, news and offer text: yes** (infocards) |
| Space chatter: NPC combat, patrol, trade, ATC/docking | `voices_space_male/female.ini` (`gcs_*`, `rms_*`) | ~15,970 fragments | no |
| News | text only (10 voiced `li_news` lines are cutscene lines) | — | yes |

**Composite lines**: space and base chatter is built from fragments at run time: a faction, then a
formation designation (`gcs_refer_formationdesig_22`), numbers (`gcs_misc_number_16`), bases
(`gcs_refer_base_Li01_14_base-`), systems, commodities. The `+`/`-` suffix picks the intonation variant.
Client comm playback takes a voice plus a list of msg ids (Freelancer.exe 0x4CDF40 reads up to 16 ids at
+0x44, count at +0x84). One subtitle = the fragments' texts joined in order. Variant pick: 0x4571C0
replaces the trailing `_N` of a GCS name with a random variant (`rand() * N`).

## 4. Candidate hook points

Shared facts: voice registry 0x667E18, `Voice* __cdecl FindVoice(u32 voiceId)` 0x430AB0,
`Voice::vt[5]` (+0x14) `GetSound(u32 msgId)`. Nine callers of FindVoice: 0x457200, 0x489DF1, 0x4CAE5B,
0x4CDF56, 0x4CE015, 0x4CE20A, 0x4CE560, 0x553597, 0x554D64.

| Candidate | Catches | Pros / cons |
|---|---|---|
| **Voice::GetSound (vtable slot, voice object)** or the comm player around 0x4CDF40–0x4CE560 | every bank line: comms, GCS, ATC, bar speech, with voice + msg id | exact ids. Some callers only query (e.g. a priority scan at 0x4CDF40), so we must tell "start playing" apart from "look up". Vtable patch = chains cleanly |
| Cutscene sound start (Thorn `START_SOUND` → story sound nickname) | story/room cutscenes | exact nickname + the duration from the .thn; the handler is not located yet (strings in Freelancer.exe and THORN.dll) |
| Sound-manager level (`DALib::Sound` ISoundManager in SoundManager.dll, 0x5C6D44) | every sound | a single choke point, but loses the ids: we would need a data-pointer → (bank, hash) map. Lorash's FLSubs worked on "active audio sources" |
| Content.dll IAT `pub::SpaceObj::SendComm` (controller mod already logs it) | scripted mission comms, server side | ids and sender; no timing (playback is client side and queued); single player only |

Plan: start with a live trace (log FindVoice callers with voice/msg ids, and story sound starts). Pick
the narrowest points that give id + real start time. Measure the end time from the clip duration (the
WAV header, or `duration =`).

**Coexistence with FLController**: both load from `dacom.ini [Libraries]`. Rules: IAT and vtable patches
store and call the previous pointer. Inline detours use a chaining detour (if the target already starts
with a JMP, the trampoline follows it; FLController's `hk::Detour` already does this). Never restore
original bytes on unload. Prefer different addresses where possible. For drawing: FLController uses
0x41F150 (cursor draw, inside UI rendering) as its overlay point; subtitles can chain the same detour, or
patch the D3D8 device's EndScene through the vtable (chains naturally). Must test both load orders.

**Rendering**: own D3D8 drawing (device captured the FLController way: RP8.dll IAT → CreateDevice/Reset).
Text sized from the render height (0x610854), not pixels or DPI. A font atlas rasterized at run time
(GDI or ImGui, MIT; FLUF has an ImGui D3D8 backend) keeps it sharp at 1080p–4K and in any aspect or
Borderless Windowed. Options: size, background box opacity, speaker names, per-type toggles (ambient
chatter off by default?).

## 5. Getting the text: decision (user, session 1)

**The mod ships the subtitle text itself** together with the runtime DLL. Players supply their own
Freelancer / HD Edition install; no game files or audio are ever shipped. Basis: Microsoft's
[Game Content Usage Rules](https://www.xbox.com/en-US/developers/rules) (free, non-commercial fan
Items) and two decades of tolerated Freelancer modding. Conditions:
- Ship only what is spoken in game plus speaker names: no stage directions or delivery notes from the
  shooting script, no cut lines.
- Free and non-commercial, no ads; README and release page carry the notice:
  "Freelancer © Microsoft Corporation. FLSubtitles was created under Microsoft's "Game Content Usage
  Rules" using assets from Freelancer, and it is not endorsed by or affiliated with Microsoft." plus a
  link to the Rules.
- The text lives in its own data file(s), separate from the DLL, so it could be pulled and replaced by a
  local build if Microsoft ever objects.
- Still local git only until the user says otherwise; the text files may be committed once this is the
  plan of record (they are the product), audio and game files never.

**How we produce the text (on our side, once):**
1. **Index**: parse voices_*.ini, banks, story_sounds.ini, mission scripts and .thn → one row per line:
   id, voice, speaker, source file/hash, duration, mission/scene, order.
2. **Script**: story and mission lines (~96 %) take text and speaker from the Bonus DVD shooting
   script by line id (`<root>\research\BonusDVD\`), spoken text only.
3. **Speech to text** (whisper.cpp, MIT, native Windows + CUDA; or ffmpeg's whisper filter) on all
   ~13.5 h: checks the script lines (flag where script and recording disagree) and supplies everything
   else, mainly the ~21,000 chatter fragments. Glossary prompt from the game's own names
   (NameResources: systems, bases, factions, characters). MP3-in-WAV decoded with ffmpeg.
4. **Review**: the user corrects flagged lines; corrections go into the shipped file.
5. **Assemble**: subtitle file(s) keyed by msg id / story nickname. Composite chatter is assembled at
   run time from fragment texts; number fragments formatted.

Then the runtime DLL (`FLSubtitles.dll`) loads the file and shows lines from the hooks. Build order
follows the goal: cutscenes + mission comms, bar/base, news (already text) and other dialogue, then
ambient chatter (fragment assembly).

## Open questions
- Which FindVoice caller is "start playing", and does a comm's fragment list play back to back?
  (Trace live.)
- Where cutscene sounds start (Thorn sound entity → story sound).
- Whether anything plays voices outside both systems (e.g. the intro movie is a video file: its speech is
  not covered).
- Whisper accuracy on 11 kHz radio-filtered voices and on names (Juni, Lonnigan, Orillion, Tekagi):
  measure on a sample before committing to the pipeline.
- How far the shooting script's text drifts from the final recordings (sample against speech to text).

## 6. Speech to text (set up, session 1)

**Model: Qwen3-ASR-1.7B** (`Qwen/Qwen3-ASR-1.7B-hf`, Apache-2.0). On the Hugging Face Open ASR
Leaderboard (15 Oct 2026) it is the most accurate open-weight model: 5.40 % average WER. Only paid APIs
rank higher. OpenAI's whisper-large-v3 is at 7.16 % and whisper-large-v3-turbo at 7.64 %. It accepts a
free-text context prompt, which we use as a name glossary.

Environment: `FLSubtitles/asr/` is a uv project (Python 3.13, torch 2.14.1+cu130, transformers 5.20).
`.venv/` and the model cache `.cache/` are git-ignored.
- `transcribe.py --list clips.tsv --out text.tsv [--glossary glossary.txt] [--batch 64]`: decodes any
  audio with ffmpeg (handles the MP3-in-WAV) and runs batched bf16 generation on the GPU.
- `script_check.py sample|compare`: samples story clips that have script text and measures the WER of
  the script against the transcription.
- `glossary.txt`: character, faction, system and place names.
- Setup: `uv sync --python <path from "uv python find 3.13">`. On this machine uv can't create its
  Python minor-version link, so pass the interpreter path explicitly. Run Python with `-I`.

**First measurement** (80 random story clips with script text, 4.5 min of audio):

| Run | Identical lines | WER script vs recording |
|---|---|---|
| no glossary | 43/80 | 6.9 % |
| with glossary | 60/80 | 3.6 % |

- With the glossary, names come out right (Orillion, Quintaine, Rheinland, Jun'ko, Tekagi, Hakkera).
  Still wrong: "Bundschuh" → "Bunshu", "Juni" → "Junie" (once).
- Most remaining differences are the actors departing from the Oct 2002 script ("You are not" for
  "You're not"; "Actually, I did, Colonel." was never recorded; added "Mmm"). There the transcription
  is the more faithful subtitle.
- Merge rule to design: script text by default; where the transcription differs by whole words, flag
  the line or take the transcription's words (keeping the script's spelling of names).
- Speed: 76× realtime on short clips at batch 64 (RTX 5090, after a ~10 s one-time CUDA warm-up), so
  the whole ~13.5 h corpus takes roughly 10–15 min.

## 7. Composite chatter: design notes (user reminder, session 1)
The game builds most space and base speech from short fragments: a ship's codename (formation
designation + numbers), its faction, source and destination base/system, commodity and so on. One comm =
a voice + up to 16 msg ids played back to back (Freelancer.exe 0x4CDF40 reads them from the comm
record: ids at +0x44, count at +0x84).
Consequences for the subtitle data:
- Store fragments in a **fragment form**: no terminal punctuation, a lowercase start unless the
  fragment starts with a name, numbers as digits (`gcs_misc_number_16` → "16"). Speech to text and
  FLSubs return fragments as finished sentences ("The Omega-41 system.", "for Daumann
  Construction."), so this needs a normalization pass.
- Assemble at run time: join the fragments in play order, capitalize the first letter, and add the end
  punctuation for the whole sentence. Callsigns: designation + numbers → "Alpha 3-7" (exact pattern to
  check in game).
- `+` / `-` suffix of a msg id: probably the intonation variant (`-` = falling / sentence end,
  `+` = rising / continues). Hypothesis: confirm with the hook trace before using it for punctuation.
- Show the subtitle when the first fragment starts and grow it as fragments play (FLSubs did this with
  left-aligned text so it doesn't jump), or show the whole assembled line at once (we know all ids when
  the comm starts). To decide when the runtime exists.

## 8. Full corpus: index, transcription, three-source comparison (session 1)

Pipeline (`FLSubtitles/asr/`, outputs in the git-ignored `FLSubtitles/out/`):
`build_index.py` (every voiced line + extracted bank clips + script text by id) → `transcribe.py`
(Qwen3-ASR, glossary) → `align.py` (scene-level script alignment + FLSubs column) → `report.py`
(three-source vote, `review.tsv`, `report.md`).

**Index** (HDE 0.7.1): 981 story sounds (74.5 min, incl. 19 scene mixes in gf_sounds.ini, up to
3.4 min each), 24,001 named bank lines (692 min), 2,768 orphan bank clips no voices INI names (66 min):
**27,750 clips, ~13.9 h**. Lines get script text by exact id: 877 story / 1,252 bank, plus 44 / 52 by
line number.

**Transcription**: all 27,750 clips in ~25 min of GPU time. Batch 256 for short clips; tiny clips are
overhead-bound at batch 64. Problems found and fixed:
- Repetition loops on very short clips: the token cap now scales with clip length.
- 21 tiny or silent clips echoed the glossary prompt. They were re-run without it. 0.1 s Neural Net
  placeholders are left empty; one-word fragments (`gcs_misc_to`/`_from`) take their text from FLSubs
  or the id.

**Script vs recordings: ids drift.** Final recordings split script lines (Dexter s032a_0501 →
0501 + 0502), insert clips and renumber (Sinclair s028x_0704 → 0705), or cut long lines to their first
words (newscast s014b_1001 = "Doyle Lonnigan."; the rest of the script line was never used). So
`align.py` aligns words per scene instead of trusting ids: each clip takes the script words its audio
covers (script spelling and punctuation), and clips the script doesn't match fall back to the
transcription.

**Three-source result** (2,126 clips get script text; 25,624 have none):

| Script lines | Clips | Draft |
|---|---|---|
| agrees with the recording | 1,517 | script |
| only fillers/contractions/number style differ | 195 | script |
| only spelling/name variants differ | 145 | script |
| words differ, FLSubs agrees with the script (speech-to-text error) | 152 | script |
| words differ, FLSubs agrees with the recording (actor deviated) | 88 | **recording wording (FLSubs text)**: proposed |
| words differ, FLSubs has no say | 29 | script, review |

| Lines without script | Clips |
|---|---|
| FLSubs = speech to text | 5,792 |
| FLSubs ≈ speech to text (cosmetic) | 1,053 |
| FLSubs differs from speech to text | 816 → review |
| speech to text only (mostly space chatter and orphans) | 17,957 |

Where FLSubs and speech to text disagree without a script, FLSubs usually has the names right ("Nansei
Research Station", "Bundschuh", "Curacao", "Vierlande", "Orbital Spa and Cruise"). Speech to text is
right where FLSubs has typos ("strategically alliance", "it's owners", "HULL BREACH AMMOUNT").

**Fourth source, shipped by the game**: base, system and faction fragments
(`GCS_ML_W02aF_<base>_NN`, `gcs_refer_base_<base>_base-`, `gcs_refer_system_<sys>-`,
`gcs_refer_faction_*`) name things whose display names are in the game's own string tables
(NameResources). Those can be taken from the game directly. That settles most name disputes and is
copyright-clean.

**Proposed merge rules** (to confirm):
1. Script lines: script text (the user's rule: prefer the script when in doubt). Exception: the 88
   lines where FLSubs and the recording agree against the script take the recording's wording
   (FLSubs text). The 29 two-way disagreements go to review with the script as the default.
2. Lines without script: names from the game's string tables where the fragment refers to a
   base/system/faction. Otherwise FLSubs text when it agrees with speech to text (human punctuation),
   review when it doesn't (816), speech to text alone for the rest.
3. Everything gets the fragment normalization of section 7 before it ships.

Licensing of the result: FLSubs (GPL-3.0) and wedge009.net (CC BY-SA 4.0, one-way compatible with
GPL-3.0) text in the data → the shipped subtitle data is GPL-3.0 with credits: Lorash (FLSubs),
L@Zar0, the Russian edition's editor, wedge009.net and the other transcript sites they cite. The user
is happy to credit everyone (session 1).

## 9. Merge rules, as built (session 1)

`gamenames.py` → `merge.py` (after `align.py` and `report.py`). Output `FLSubtitles/out/draft.tsv` (id,
voice, speaker, text, source, fragment flag, review level, note, speech to text, FLSubs), plus
`review_high.tsv` / `review_low.tsv`. User corrections: `FLSubtitles/data/corrections.tsv` (committed;
id case-insensitive, voice `*` = every copy of the line).

Priority:
1. **User corrections** (listened in game).
2. **Script lines**: the script text. Where FLSubs and speech to text agree against it, the recording's
   wording: FLSubs words and punctuation, ordinary-word typos replaced from speech to text, names in the
   script's spelling (also letter swaps: "Tegaki" → "Tekagi"). A short name swap ("Doyle" heard as
   "Sam") keeps the script and goes to high review. Where only speech to text disagrees (FLSubs sides
   with the script): script, low review (the user found one such case wrong: King "Great work").
3. **Lines without script**: FLSubs as the base (human punctuation/capitals); where words differ, names
   and game terms from FLSubs and ordinary words from speech to text. Fragments that name one game object
   take the name from the game (`gamenames.tsv`):
   - formation designations exactly
   - bases: their distinctive name words, respelled. "bryson" → "Briesen", "Edison Trenton" →
     "Trenton"; when nothing matches, the base's own name ("bowman" → "Beaumont")
   - systems, factions, commodities: respelled to the game's spelling and case
4. **Fragment form** for composed chatter: no end punctuation, lowercase start unless a name, numbers
   as digits.
5. Scene mixes (`mix_*`) are tagged for timed parts (next step), not reviewed as single lines.

Alignment fixes made on the way: substituted words map to word ranges; joined/split words map exactly
("godspeed" = "God speed"); clip edges snap to unclaimed words of the same line ("Good" in "Good
work…"); a line id that exists twice (bank entry + story sound) is aligned once. Later (user report,
Razor 1 took the battleship's "We're losing containment!"): a clip never keeps words of a script line it
doesn't match at all, and takes the unclaimed rest of its own line (end only) only when the clip has
spare time for it (heard words at 2.5 words/s + the extra words at 2.5 words/s + 0.8 s). Script
delivery-note lines (starting with "(", also unclosed ones like "(Determined") are skipped.

Result (HDE 0.7.1, 27,750 clips): speech to text 21,779, FLSubs+speech to text 3,838, script 2,046,
recording wording 78, corrections 3, empty 6 (silent placeholders); 10,920 fragments.
**Review: 34 high, 152 low.**

## 10. Review tool (session 1)

`FLSubtitles/asr/review_server.py` + `review.html`: a local web app (Python standard library,
127.0.0.1 only). Start it with the browser pane's `subtitle-review` launch config (environment root
`.claude/launch.json`) or `.venv/Scripts/python -I review_server.py` and open http://127.0.0.1:8765/.
- Lists: high review, low review, sources disagree, story & missions in scene order, chatter, all;
  search by id/voice/text; reviewed lines are hidden unless "show reviewed".
- Per line: the clip (the game's MP3-in-WAV served as plain MP3, auto-plays), speaker/voice/source
  badges, up to 7 neighbouring scene lines for context, and the reads: draft (with its source), script
  (aligned), script by line id, speech to text, FLSubs. Identical reads are shown once.
- Picking: click or 1–5 selects a read; double-click, its Pick button or Enter picks it (a single click
  never saves). Shift+1–5 or the Edit button copy a read into the text box to edit · E edit the
  draft · Enter save typed text · Shift+Enter save exactly as typed · Space/R replay · P/← back through
  every line seen (also saved ones; the history survives reloads; re-saving returns to where you were) · N/→ forward/skip ·
  Esc leave the text box.
- Typed text is fitted to the conventions before saving. The adjustment is shown, and Shift+Enter or
  "Keep my exact text" overrides it:
  - names in the script's or the game's spelling, also letter swaps ("Rhineland" → "Rheinland",
    "Curacao" → "Curaçao", "Tegaki's" → "Tekagi's")
  - callsign numbers as digits ("Gamma Six" → "Gamma 6")
  - fragment form for chatter pieces
- Every decision goes straight into `FLSubtitles/data/corrections.tsv` (story/mission lines with voice
  `*`, chatter per voice; source = "user picked: <read>" / "user typed"). Re-run `merge.py` to rebuild
  the draft with them.

## 11. Global name rules, glossary echo, system fragments (session 1)
- `FLSubtitles/data/name_rules.tsv` (user decisions, applied by merge.py to every final subtitle including
  corrections, and by the review tool's fitting): "Doyle" → "Sam" (Lonnigan; the recordings say Sam),
  "Daam K'Vosh" / "Daam-K'Vosh" / "Dom Kavash" → "Dom'Kavash".
- Glossary echo: with the name prompt, the model sometimes reads the glossary back on screams and
  near-silence ("Edison Trent. Trent. Jun'ko Zane. Juni. King…"; 12 clips, incl. 9 combat screams, two
  Omicron system fragments, Orillion's DX_M13_0960 and the scene mix mix_s051a). Fixed in asr.tsv;
  `transcribe.py` now detects three glossary terms back to back and re-runs that clip without the
  prompt. Lines that merely name several characters are not affected.
- The glossary also pulled system fragments towards its entries ("Omega-3" for every Omicron system; 102
  of 782 system fragments were wrong). System fragments now take the exact game name in their spoken
  frame ("the Tau-29 system"); names from universe.ini [system] (St01 = Omicron Minor). Check: 782/782
  system and 3,240/3,240 base fragments carry their game name.
- Open style question: combat screams (gcs_combat_scream_*) transcribe as "Oh." / "Ah!" or nonsense.
  Decide a convention ("Aaah!", "[screams]", or no subtitle).

## 12. Proper-noun audit (session 1)
`FLSubtitles/asr/name_audit.py` → `out/name_audit.md` / `.tsv`. Reference: the game's own text (all
strings + infocards via `game_text.tsv` = dump-resources.ps1 output, plus gamenames.tsv); then script;
then FLSubs. Every capitalized word used mid-sentence in the draft is classed ok / case / variant /
unknown / caps / script; multi-word game terms are checked for consistent capitals.
- Faction, commodity and ship-type fragments (like systems before) now take the exact game name in the
  spoken frame ("the Ageira", "MOX", "Basic Alloys"; faction `_short` fragments the short name). This
  fixed "Jirum"/"Jereb"/"a chiro" (Ageira), "Max Stewart" (MOX) and another glossary echo.
- Result: 874 distinct names: 551 ok; 25 variants + 28 unknown (two thirds in orphan bank clips that no
  INI references); 80 case differences against the game's capitals; 133 words in ALL-CAPS lines (Neural
  Net voice lines as FLSubs wrote them). The game capitalizes its terms consistently ("Trade Lane" 439:8,
  "Jump Hole" 429:4, "Jump Gate" 255:7, "Neural Net" 183:0, "Nav Map", "Docking Ring"); the draft mixes
  them. Proposals pending the user's decision.
- Decided (user): game terms capitalized as the game writes them, everywhere: Trade Lane, Jump Hole,
  Jump Gate, Neural Net, Nav Map, Docking Ring (incl. plurals and one-word forms) via name_rules.tsv.
  ALL-CAPS lines (Neural Net voice as FLSubs wrote it) become sentence case; stray all-caps words in
  non-dialogue lines are lowercased, acronyms kept (LSF, GMG, ALG, MOX ...). Name capitals come from
  the game/glossary unless speech to text shows the word is everyday ("launch to space", "ship
  dealer"). Script dialogue keeps its emphasis capitals for now ("We've been HIT!", "RAZOR 2").

## 13. Judged low-priority lines (session 1)
Low-priority lines = script lines where FLSubs agrees with the script and only speech to text differs.
A one-off script (`auto_resolve.py`, deleted 24 Oct 2026; its result is `auto_decisions.tsv`) classed each
differing word group cosmetic / soundalike / real (`out/auto_candidates.tsv`); Claude then judged the rest with the neighbouring lines, mission context
and known names. Decisions: `FLSubtitles/data/auto_decisions.tsv` (id, script | recording + text |
unsure, reason), applied by merge.py below the user's corrections. Of 150 open lines: 130 script
(speech to text misheard: "Heaven Claussen", "Chicago" for Shinkaku, glossary "Edison" echoes ...), 11
recording wording (script typos the actors didn't say — "gonna to", "find to your friend", "use select
the", "had anything had to do" — and real deviations confirmed by FLSubs), 9 unsure (speech to text heard
extra/different content words) left in the low-priority list. The review tool lists all judged lines
under "Judged by Claude (audit)" with their reasons; a pick there becomes a user correction.

## 14. Spelling and punctuation conventions (session 1)
`FLSubtitles/data/style_rules.tsv`, applied by merge.py after the name rules to every subtitle, and by the
review tool's fitting; a lowercase replacement takes the capital of what it replaces. Basis: game text,
then standard spelling, then the user's consistent picks. "damn it" (not "dammit"), "all right",
"okay", "Mr."/"Dr." before a name, "organized", "homeworld", "safehouse", "bullseye", "shake-up",
"babysit", "jar-head", "xenoarchaeology", "uh-huh"/"mm-hmm"/"hmm", "whoa"; "..." attached to the word
before; spaced dashes as " – "; single "!", "?!"; "The Order" mid-sentence (game 65:20); "Rogue(s)"
capitalized. Not unified (spoken variants): c'mon/come on, gotta/got to, gonna, ya. A correction saved
with Shift+Enter ("kept exactly as typed") skips these conventions (name rules still apply).
Re-review: `out/rereview.tsv` (generated) lists the user's picks that conflict with the conventions or
other lines (17 lines in session 1); the review tool shows them under "Re-review".
Also fixed: the recording-wording respell turned "for"/"four" into "Fort" next to "Fort Bush"
(dx_m01_1042_king); only capitalized non-number words are respelled now.
- Re-review rule (user, session 1): EVERY reviewed line whose subtitle differs from what the user saved goes
  back to the user. merge.py writes `out/rereview.tsv` on every run: each correction changed by the
  all-caps fix, name rules or conventions, with the rules that changed it, plus lines flagged in
  `data/rereview_notes.tsv` (inconsistent with other lines; dropped once re-saved). The review tool's
  "Re-review" list shows them (one per line id) and drops a line once it is re-saved.

## 15. Split sentences across clips (user decision, session 1)
Convention: a sentence that continues in the speaker's next clip ends without punctuation (or with
"..." where the script marks a pause) and the next clip starts lowercase; "..." only for real pauses or
trailing off; "–" when cut off by someone or something else. Claude judged all 127 candidate clip
pairs (scene line order) → `FLSubtitles/data/continuity.tsv` (58 lines: 18 continuations, 8 cut-offs,
32 complete sentences that lacked end punctuation, incl. the Jump Hole timer), applied by merge.py
after the conventions (also to corrections; changes go to Re-review). Style rule added: an ellipsis
between words gets a space ("Liberty...thanks" -> "Liberty... thanks").
Open: the Freeport 7 bridge death scream (DX_M01_0035) reads "Hey]" — part of the scream-style question.

## 16. Review lists (session 1, after the first review passes)
The review tool's menu comes from the server (/api/modes) with the number of lines still to review:
Re-review; then, most worth an ear first: recording wording chosen automatically (75); names the game
doesn't know (79); chatter/base where FLSubs and speech to text disagreed and were merged automatically
(1,233); Neural Net voice (159); chatter/base sentences with speech to text only (9,451); fragments with
speech to text only (9,232); script lines where all sources agree (1,871); chatter/base where FLSubs and
speech to text agreed (2,392); screams (280, style pending); scene mixes (19, need timed cues); orphan
clips (2,768, probably unused). Also: story & missions in scene order, judged-by-Claude audit, all lines.
Chatter lists sort the same line id by voice together and show the other voices' versions as context.
