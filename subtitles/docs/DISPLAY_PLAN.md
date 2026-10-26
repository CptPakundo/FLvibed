# In-game subtitle display: the brief (stage 3, started 17 Oct 2026)

The user's vision and requirements, verbatim in substance. Work from this; record findings and decisions
here as the stage goes on (keep it lean).

## What to show where

- **Scripted dialogue and dialogue directed at the player** (story cutscenes, mission comms, base/bar
  conversations with Trent, mission characters' radio to Trent): a conventional **lower-centre** subtitle
  area that neither overlaps nor is overlapped by the game's UI, where possible.
- **Generic sentence-mixed NPC chatter in space not directed at the player** (gcs_ pieces the game joins:
  docking, combat, patrol chatter): secondary; it could populate the game's **built-in chat** (HD Edition
  is expected to make it more accessible in single player: verify).
- Which line is which: `data/subtitles.tsv` kind column (story / unique / chunk / nn) and the review tool's
  line kinds; chatter sentences are assembled at runtime from pieces (`merge.render_sentence`, joining rule
  in `data/chatter_grammar.tsv`; builders and triggers read from Content.dll).

### Priority and the side chat (the owner, 26 Oct 2026)
The owner, watching M01 (vanilla 1.1 + FLUF): King's `DX_M01_0310_KING` ("Trent, we can't let them get away. Engage the
enemy!", with his portrait) gone from the screen 0.9 s later while chatter (`gcs_combat_hasinsights_04`) played, and the
LPU Beta leader's story line cutting Newark's `DX_M01_0320`. **The design (new default):** story lines and dialogue take
centre stage as the primary subtitles and hold their place until their audio ends; NPC lines that do not open the comm
portrait go to the game's side chat (its chat window), so chatter never replaces or covers a story line; a player option
also copies the story lines into the chat (`chat_all_dialogue`, default off: the chat log then holds every spoken line).
- **The split, from what the game decides for that comm** (`impl.cpp` OnComm, no name lists): **dialogue** = a scripted
  story line (the data's `story` / `unique` kind: the mission's DX_ ids) or a comm the game opens its portrait for (to the
  player, from someone else, with a costume: Dispatch's rule, `..\..\docs\FREELANCER_RE.md` "The comm window live"; dock
  replies, a wingman to Trent); **side chat** = everything else on the comm path: joined gcs_ chatter between others,
  broadcasts (to=0: "Attention, all ships ... This is Trenton Outpost"), a story character's own chatter voice ("King ·
  LSF: Trent, I have a ship on my tail!" is a broadcast), a costume-less comm. Before: any comm to the player counted as
  dialogue. Neural Net, scenes and bars unchanged.
- **Nothing lost:** every side-chat line goes into the game's chat log (`chat.cpp` Print through ChatFlush 0x46A150, the
  name and colour as the subtitles'); while HUD 2 shows (its chat window hidden) its comm feed draws it too, and what the
  feed drops as stale stays in the log (the chat input shows it). `chatter 1` (as subtitles) takes only free room: with
  the dialogue stack full the line goes to the chat window; it never replaces a dialogue line of the same name, and pushes
  out only chatter.
- **The cut lines were the stack's height limit** (traced on HDE 4K, the same scene, the probe in
  `research\experience\runs\subtitles_priority_2026-10-26\`): the top-anchored stack retires its oldest line when it
  would pass the crosshair limit, and it counted lines that were already fading - King's long `0270` fading out pushed
  Newark's `0320` out at 4.0 of its 4.75 s, then Beta's line at 0.5 s when Newark's `0280` came. Now a fading line
  counts no more, the lines already pushed out and chatter give way first, and **a comm whose voice is still heard holds
  its place** (also against the same speaker's next line), as Insert's overlap rule already did for the two-speaker cap.
- **Verified 26 Oct 2026** (release, background, Save7798 = M01 from the Donau's fall into the first dogfight; `state` /
  `shown` / `layout` / `pages` polled, a game-window frame at King's `0310` and at every change after it; `run.log`,
  `shots\`): HDE 0.7.1 4K classic HUD (instance 1) and 1600x1200 (instance 9), vanilla 1.1 with HUD 2 (`hud_style 2`),
  vanilla 1.1 + FLUF nightly (own copy `v11nsub`, FLvibed above FLUF, HUD 2, `chat_all_dialogue 1`): every story line
  shown for its span or until its audio ended; King's `0310` held 4.2 s with his portrait while chatter ("I am under
  attack.", "I've got a visual on hostile target ...") went to the chat window; every spoken line in either the
  subtitles or the chat feed; with the option, every story line copied into the chat (FLUF leaves the chat path
  0x46A150 alone: it swaps only IClientImpl slot 0, multiplayer payloads). Seen, not changed (as before): at 4:3 under
  HUD 2 and under FLUF's Debug Window (the panel moved below it) a two-speaker panel reaches the crosshair ring for the
  0.5 s the older line fades.
- **Fixed 26 Oct 2026: the fading line's space is released first.** A fading line keeps its slot until it is gone, so
  the stack's real height still counted it; when that would carry a top-anchored stack past its limit, the fading lines
  (oldest first) now go at once and the stack closes up (`display.cpp`, after the retire passes). Measured with the
  same scene, `layout` every 2 frames for 60 s (`research\experience\runs\fix_bleak_bar_2026-10-26\t3_sub.ps1`): HDE
  0.7.1 1600x1200 under HUD 2 - before 30 polls over the limit, 3-block stacks, lowest bottom 0.498 H; after 2 polls, at
  most 2 blocks, 0.369 H (limit 0.360 H). **Open (owner):** what still crosses the limit is two comms whose voices both
  still play (the voice-hold rule above wins over the limit): vanilla 1.1 + FLUF at 1920x1080 under the Debug Window
  (stack anchored at 0.16 H) King + Newark reach 0.513 H, at 3840x2160 0.373 H (limit 0.328 H). FLUF sizes the game to
  the screen at every start, so 4:3 with FLUF cannot be set on this monitor.

## Do it "the right way" from the start

Look up current game-subtitle standards and guidance online first (accessibility guidelines for game
subtitles, e.g. Game Accessibility Guidelines, BBC subtitle guidelines, Xbox/PlayStation accessibility
guidance, research by Ian Hamilton et al.) and follow them:
- a few **size** options, **background opacity** options (letterbox / box behind text);
- **speaker names** on a separate row (or a clear prefix), with **contextual speaker name colour**;
- line length and line count limits, reading-speed-based timing, sensible defaults;
- resolution / aspect ratio / DPI agnostic (Borderless Windowed is the main mode; dev screen 3840x2160 at
  200 %), coexisting with the controller module and other plugins (hooks chain, never exclusive).

### What the standards say (researched 17 Oct 2026)

| Topic | Guidance | Source |
|---|---|---|
| Size | PC minimum 18 px body height at 1080p (36 at 4K), console 26 px; subtitles should default larger than menu text; scalable to at least 200 % | XAG 101/104 |
| Size (advocacy) | no smaller than 46 px at 1080p, by default or via options | Game Accessibility Guidelines, Ian Hamilton |
| Background | solid or semi-opaque box behind the text; opacity adjustable 0–100 %; colour configurable | XAG 104, GAG |
| Line length / count | ≤ 40 characters per line (BBC 37–38), at most 2 lines (3 exceptionally); break at sensible points | XAG 104, BBC |
| Timing | 160–180 wpm, about 0.3 s per word minimum | BBC |
| Speaker | name when the speaker changes (or after a 1–2 min pause); colour may help but never alone; "(on radio)" style cues for off-screen speakers | XAG 104 |
| Case / font | mixed case, a sans-serif option, good letter spacing | XAG 101/104 |
| Categories | main dialogue, background chatter and sound captions toggled separately | XAG 104 (The Outer Worlds example) |
| Preview | show how the settings look, ideally in game context | XAG 104 (Forza example) |
| Ubisoft (user's reference) | AC Origins/Odyssey/Valhalla, Far Cry 6, Division 2, Skull and Bones: several sizes, background on/off with opacity, speaker names on by default, speaker name colour customisable, sound captions | Can I Play That, Ubisoft News |

### Design (decided 17 Oct 2026; revise with test results)

- **Two channels.** Lower-centre subtitles for story cutscenes, mission comms, bar/base speech aimed at
  Trent, Neural Net; the game's chat window for ambient chatter in space (ChaosMod shows the chat in single
  player through Freelancer.exe 0x46A150 with rich-text nodes: proven route). Per-category on/off
  (story, comms, bar, Neural Net, chatter → chat / subtitles / off).
- **Layout.** Speaker name on its own row above the text, in the speaker colour (option: inline
  "Name: text" or off). At most 2 text lines, balanced with the top line shorter, never breaking after an
  article/preposition; longer lines become consecutive subtitles split at sentence/clause boundaries,
  timed by character share of the clip. Width limit 42 characters' worth of the font (`display.cpp` `kMaxChars`; BBC 37-38, XAG ~40).
- **Size** as a share of the render height (resolution/DPI/aspect agnostic): Small 3.0 %, **Medium 3.6 %
  (default)**, Large 4.4 %, Extra large 5.4 %, Huge 7.2 % (font em height; Medium ≈ 39 px at 1080p,
  78 px at 4K). Font: Segoe UI semibold (fallback Arial/Tahoma), glyph atlas rasterized at the exact size.
- **Background** black box per line, **opacity 60 % default** (0/25/50/75/100); drop shadow on by default.
  Text colour white (option yellow).
- **Speaker colours** (user, 21 Oct 2026: "each character keeps one consistent colour everywhere, and characters who
  appear together get clearly different colours"; built the same day, `src\impl\speakers.cpp`, `display.cpp`
  `SpeakerColor`). The name row carries the meaning; colour is a second cue. **Approved by the user after the third
  round (21 Oct 2026: "the balance feels right, keep it").**
  - **Trent** is always the player's amber (FFD27A), whichever path his line comes through. Bug fixed with it: his
    M01a Donau lines arrive as comms, where every named speaker was an "ally" (light blue, King's colour).
  - **Three tiers** (user, 21 Oct 2026, second round: "important characters should have a colour unique to them ...
    lesser importance and lesser word count (in this order) may share ... the more generic an NPC is, the closer to the
    colour actual generic NPCs use"), in `data\speaker_colors.tsv` (owned here: speaker, tier, family, colour, why):
    - **Main cast** (13 + Trent; importance first, then words: Lonnigan and President Jacobi speak little but carry the
      plot): each its own colour that fits the character - their look in the game (the 2020 playlist's room scenes:
      Juni's blue LSF uniform, King's name, Sinclair's lavender suit, Quintaine's red sash, the Blood Dragons' red and
      mauve for Ozu), their faction and role. Faction families keep a colour temperature: Liberty blues (Juni sky,
      Walker navy, King royal purple, Jacobi pearl rose), Bretonia steel / lilac / rose (Tobias, Sinclair, Quintaine),
      Kusari raspberry / wisteria (Ozu, Lord Hakkera: Kusari nobility, purple sails), the Order teal (Orillion;
      Lonnigan, framed as Orillion, a washed-out teal), Rheinland ice / slate (Von Claussen, Botzler).
    - **Side characters** (named people with small parts: Van Pelt, Vance, Dexter Hovis, Kress, Hans Fischer, Cobo, the
      newscaster, the Manhattan bartender ...): each its own shade of the family (user, after seeing the Donau cutscene:
      "secondary characters that belong to a faction still noticeably separate colours - this is still dialogue from a
      story mission"): 0.06 from the rest of the family, 0.04 from everyone else, 0.10 from whoever they talk to.
    - **Story speakers** (third round, same evening; the stations, ships, wings, crowds of the story: Newark Station, the
      LSF fighters, the Donau, the Bundschuh pilots, the Spectator): the family's light tint (Liberty light blue, as the
      user asked for Newark; Kusari pink, Order teal, Rheinland ice, civil warm near-white; never near-white for a
      faction), or a red from the hostile family when their stance is hostile; they may share where they never meet,
      0.06 apart when together, 0.08 from a character they talk to.
    - **Open-world speakers** (not in the table: chatter, unscripted comms): the game's relation colours by the
      sender's live attitude, as the HUD contact list shows them (measured on the footage: friendly green, hostile red,
      the selected target yellow; neutral white): friendly 8DDB85 (light blue until 21 Oct, next to Juni), neutral warm
      white F2E3B3, hostile FF7A6B, Neural Net mint.
  - **Stance** (third round: "the Order ship that was talking should have a hostile colour" - in M01 the Order is the
    enemy, and the game's reputation does not say so): a column per speaker, `friendly` / `neutral` / `hostile` plus
    per-mission exceptions (`friendly m04:hostile`: the Battleship Unity is the jump gate control in M02, the hunter in
    M04; Rheinland Escort 1 neutral in M01, hostile in M07), read from each speaker's lines. A hostile stance (or the
    sender's live attitude) adds the marker; a story speaker hostile only in that mission falls back to the hostile red,
    characters keep their colour.
  - **The fitting tool** `tools\speaker_colors.py` (run after changing a row; `--check` only checks): keeps the main cast
    as authored and fits side and story colours around the family anchors, the rules first (any shortfall loses to any
    colour that keeps them), then the least move from the anchor. The in-game checker (`speakers.cpp`) applies the same
    rules at every data load.
  - **How the main colours were fitted**: an anchor per character (OKLCH: the colour that fits, Juni fixed at her
    approved sky blue), then a search moved each as little as possible (hue +-40, lightness +-0.10, chroma 0.05-0.17)
    until the hard rules held: characters who speak within 3 turns of each other in a mission (`dx_mNN`) or scene
    (`dx_sNNN`, script order by the id's number) at least 0.10 apart in OKLab; all main colours at least 0.08 apart
    (unique, not just apart in their scenes: a first pass left three similar teals); at least 0.10 from the reserved
    colours (Trent amber, friendly green, neutral white, hostile red, Neural Net mint, target yellow); at least 6:1
    contrast on the HUD navy (13, 11, 26) (WCAG AA: 4.5:1). The reserved colours leave the cool half of the wheel (teal
    to rose) for characters, so lightness and chroma separate what hue cannot. Rejected: an automatic colouring from
    a six-colour palette (the first build the same day: distinct, but the colours meant nothing - King pink); a
    maximin palette (muddy olives); near-white names (no cue beside the white text inline).
  - **Hostile intent** (user: colours "change subtly to reflect whether they're hostile ... the colour is still them"):
    tried as a shift toward the hostile red (25-45 % in OKLab, chroma kept) - it fails on a cool cast: red is opposite
    blue and teal, so the shift sends each character somewhere else (Juni lavender, Orillion green = friendly, Lonnigan
    tan), never "red-ish". Built instead (`hostile_marker 1`): the name keeps its colour and gets the HUD contact list's
    own hostile sign, a small red triangle, before it (drawn from boxes: no font glyph needed; inline names in the
    letterbox bar get it hanging left of the row). One meaning, the game's own visual language, and not colour alone.
    Hostile = the sender's live attitude <= -0.6 or a taunt (`rms_`), the comm path; scene lines carry no relation.
    **Off by default since 22 Oct 2026** (user: keep it at the name colour "without adding that triangle. It may have
    confusing connotations I'd rather avoid"): `hostile_marker 0`; when switched on it now points down (user, same day); a generic hostile speaker still reads in the hostile
    red (relation colour), story characters keep their own colour.
  - **The checker**: at every data load the table is checked against the subtitle data's who-speaks-near-whom graph
    (`speakers.cpp`: listed names missing from the data, characters who appear together closer than 0.10, main colours
    closer than 0.08 or near a reserved colour); findings go to the log. Research (21 Oct 2026): subtitle guidance
    (XAG 104, BBC via Clearcast / Limecraft) - colour as a second cue beside the name, one colour per speaker kept for
    the whole work, sufficient contrast; players asked for colour by faction / disposition (Game Developer, "Making
    subtitles that don't suck"); the game's HUD: red hostile, green friendly. Nomads: violet-pink (Tekagi's possessed
    eyes, the Artifact's crystal in the footage) - no Nomad speaks, so no colour is spent on them.
  - Debug `colors [name]`: each listed character's colour, tier, family and closest coloured neighbours, then the
    checker's findings; `layout` reports each block's `name=RRGGBB`; the sweep's `display-subtitles` section checks
    Trent amber and one colour per speaker across contexts and modes.
- **Timing:** appears when the clip starts, stays for the clip duration (shipped `sec`) plus `linger` (1.0 s; 0.6
  until 21 Oct 2026), at least the reading minimum (0.3 s/word, at least 1.25 s; since 22 Oct 2026 every timing is rounded up to 250 ms steps - user: "I'd rather we do timings in 250ms increments" - the reading minimum and the time on screen included). A line pushed out by a newer speaker (the
  two-speaker cap; one speaker in letterboxed scenes) stays `retire_delay` (1.25 s since 22 Oct 2026; 1.0 before) longer, at least until its reading
  minimum, never past its own end, one such line at a time. Pages of a long line share the whole time on screen
  (audio + linger) by their characters; timing them over the audio alone was tried on 22 Oct 2026 and reverted (user:
  the next page came before King had finished the last; "I preferred overall subtitle behavior before this change").
  The user's suggestion, built the same day: **word timings by forced alignment** - the review (`subtitles\review\`) aligns
  every line's reviewed text with its recording (`FLSubtitles/aligner/align_words.py`, Qwen3-ForcedAligner-0.6B on the
  GPU, ~30 ms a clip; a word start inside a measured pause moves to the pause's end) into `data/word_times.tsv`
  (published with the text; its pre-commit hook re-aligns every changed line - user: "anything that was changed would
  need to have its forced alignment re-run"). The runtime takes a row only while its text hash matches the line
  (data.cpp `LoadWordTimes`; else the estimate) and turns each page when its first word is spoken; the first page
  from the line's start, the last through the linger; pieces of one comm back to back; joined chatter stays estimated.
  Verified 22 Oct 2026 (4K dev, debug `pages`): King's M01 briefing through the comm path turns at 4.48 / 7.68 / 10.57 s,
  on his pauses. Words the aligner could not place (piled on the clip's end, or several on one start with no time to
  share: deliberate garble, a text to check, or its weakness on a line's last words) ship as -1: a page starting on one
  keeps the estimate, the rest still turn on the words; a line with most words unplaced ships no timings. Short words
  sharing a start with a neighbour (its 80 ms steps) share the time to the next word, so they pop too (user, same day:
  "small words like 'to' or 'a' ... don't get highlighted").
  **Overlapping comms** (user, same day, footage of the Donau's fall: lines "no longer appear"): the game plays some comms
  at once (Newark Station's "Red Alert" to King under the Donau's own line). A new speaker removed a line whose audio
  still played at once - the rule for skipping through scenes - so Newark's second page never showed (traced: the same
  with and without word timings, `word_timing 0`). Now a comm whose voice is still heard stays, kicked up (over the
  picture in a letterboxed scene), with its pages, until its audio ends; scene lines keep the skip rule. Out of the
  cutscene the flight stack's 0.44 H crosshair limit waits for the top panel (up to 2 s): in the moment before the HUD
  is up the stack stands lower (the fallback place, under the GAME SAVED banner) and dropped Newark for good; now its
  second page shows in the panel (`crosshair_handover_fixed_075.9s.png`).
  **Spoken-word pop** (`word_pop` 1; user, same day: "subtly 'popping' words being said", "as visually consistent with
  the game as possible"): on a word-timed line the word being spoken grows to 108 % over 120 ms and settles over 200 ms
  after the next word starts (smoothstep), continuously - the text font rasterised at 108 %, drawn scaled down to the
  moment's size with linear filtering (`render::TextScaled`) - centred on its own place, the row never reflowing; the
  yellow text colour also lifts toward white. The growth never closes a word gap (fix-text 3, 25 Oct 2026: at 108 % a
  long word in Agency FB, whose space is narrow, met its neighbour - "yourcontrols", "showingthelocation"): each side
  may take at most a fifth of the space (`textfit::GrowCap`), so a long word pops less wide than 108 %
  (`research\experience\runs\fix_text3_2026-10-25`). Rejected on the way (`research\experience\runs\word_timing_1022`): a
  crossfade of two sizes (a grey double image, `pop_crossfade_rejected.png`), four solid size steps (user: "still looked a
  bit choppy", `pop_steps_choppy.png`); the continuous one: `pop_continuous.png` (4K, 20 fps sampling of 170 fps grabs). (User, 21 Oct 2026: short lines of a quick exchange
  "appear to move too fast"; `retire_delay 0` = the old instant cut). A line whose audio the next one cut short
  (the player skipping through a scene) still goes at once (user, same day: the grace kept lines up while skipping).
  In the letterbox bar (one speaker at a time) the grace holds only while both lines fit in the bar - except before the
  pushed-out line's reading minimum: then it stays, over the picture above the bar, until it has had it (user, same
  day, the M01a Donau cutscene: quick radio lines unreadable; "allow a new line to push an older line"; holding the new
  line back was rejected - lines stay in sync with the audio). Since 22 Oct 2026 (user, the Donau cutscene again: "some
  subtitles linger even when the next line has started to be spoken ... not expected") letterboxed scenes give a short
  grace, `retire_delay_cinematic` (0 at first, then 0.35 s, 0.6 s, now 0.75 s on the 250 ms steps - user, same day: "just a tiny bit more time for subtitles
  that get kicked away by a new one"): footage from the user (M01a launch, 60 fps) showed the old line holding its place at
  full strength for the grace with the new one under it - read as the new line arriving late. Now the old line is
  kicked up at once, gliding up (a jump read as abrupt), and fades from the new line's first frame, linearly over
  0.5 + 0.75 s = 1.25 s (0.35 "disappeared a tiny bit too quickly"); a line not yet readable is held up there until its reading minimum, then fades as usual. Grab:
  `research\experience\runs\comm_portrait_slot_1021\cutscene_kickup_4k.png`. **The same speaker going on** replaces their
  previous line in place at once, everywhere (user, 22 Oct 2026, a recorded session: the new line sat under the old,
  which kept its slot while it faded, and "does not show up immediately"); different speakers keep the push-out. Flight keeps `retire_delay`. Traced on the M01a launch + Donau scene (layout every
  100 ms): overlaps 0.15-0.5 s, the fade only. Known limit: a new line that cuts the old one's audio
  short counts as skipping and still replaces it at once.
- **Position:** lower centre, above the bottom HUD; the free band is computed from the HUD controls'
  projected rectangles (UI camera), with a fixed fallback; offset option.
- **Settings:** `EXE\FLSubtitles.ini` first (with a live preview command); an in-game options page later.
- **Intro movies:** (done; since 26 Oct 2026 force-aligned, "Movies: word-timed pages" below) both `fl_intro.wmv` (HDE HD intro) and `fl_intro_vanilla.wmv` need narration text
  and timings: transcribe with the speech-to-text pipeline, review, ship as timed lines.

### Look and motion (19 Oct 2026, user: "dynamic, legible, visually appealing and in keeping with the game's tone")

Studied at 5 fps (video only, `research\videos\subtitles\`): The Last of Us Part II (4u_Gg0vM9hc, subtitles on),
Star Wars Outlaws (Lvgb0rSWDQw), Cyberpunk 2077 Phantom Liberty (76MHAmKuh-U), Spider-Man 2 (PhusRjKlB8M); Alan Wake 2
and Death Stranding 2 excerpts had (almost) no subtitles. Findings:
- **TLOU2:** a bottom-anchored queue of one line per utterance, newest at the bottom, at most two; a new line fades in
  ~0.2 s; an ended line fades out ~0.3 s keeping its slot, then the other glides into place (~0.3 s, eased); a third
  line retires the oldest. White text, inline "Name:", soft shadow, no box.
- **Outlaws:** name on its own row in a speaker accent (Kay cyan, enemies cream), white text, soft shadow, no box; a
  block that grows a row eases upward (~0.4 s); pages replace in place.
- **Cyberpunk:** tone from the game's UI - its HUD font and palette for the text, the name in a red accent with a
  hanging indent, a faint dark backing; the name fades in.
- **Spider-Man 2:** inline name plus a speaker icon, white text with a shadow.
- XAG 104: at most 2 lines (3 exceptionally), name when the speaker changes, adjustable backing; nothing on motion.

Decisions (built 19 Oct 2026):
- **The game's own typefaces** (`font 0`, default): `DATA\FONTS\fonts.ini` [FontFiles] loaded for the process
  (`AddFontResourceExW` FR_PRIVATE, as the game uses them) - speaker names in **Agency FB Bold** (the HUD labels, the
  objective banner, "Neural Net"), 1.2x the text size (condensed face); text in **Arial Unicode MS** (the game's body
  text: lists, tooltips, infocards; widest coverage), rows at 0.98x its line height. A face GDI would substitute is
  refused and Segoe UI semibold used (`font 1` = Segoe UI always).
- **Colours:** names by character (Trent amber, each story character its own colour) or relation ("Speaker colours"
  above); text white (option yellow).
- **Backing:** one panel per speaker. `background_style 1` (default since 21 Oct 2026, an experiment the user asked
  for: "identical to the game's space HUD ... that scanline animated background"): the HUD panel look, measured on
  the game's own panel in flight (`..\..\docs\FREELANCER_RE.md` "HUD panels"): violet-navy base (13, 11, 26), inner
  edge glow (45, 56, 73) over ~12 px at 4K with a brighter top line, and the scanline - a pale blue line (refreshtest2's
  160, 209, 255) with a glow trailing above it, sweeping down the panel every 4 s. Opacity scales with
  `background_opacity`. Side by side with the HUD panel: `research\experience\runs\hud_scanline_1021\compare_sheet.png`
  (the HUD's chamfered corners and frame mesh are not copied). `background_style 0`: the soft translucent navy panel
  (0x081422), feathered over ~0.12 em.
- **Inline names** (inside the letterbox bar): every page of a long line starts with `Name:` (user, 21 Oct 2026: keep
  the speaker on each part); a later page that no longer fits two rows with it goes without.
- **Motion** (`motion 1`, default; 0 = instant): TLOU2's model - fade in 0.125 s (user, 22 Oct 2026: "cut down channel open (both text and portrait) to 125ms"), fade out 0.5 s in the slot (0.18 / 0.30 before the 250 ms steps), then the
  stack glides (exponential ease, tau 0.07 s, ~0.2 s to settle); at most two speakers, a third retires the oldest; the
  anchor glides too (banner, context changes snap).
- **Flight: the top panel** (`space_position 2`, default since 21 Oct 2026; user: "move the top HUD element (ship
  flight modes) to the top left ... space subtitles to the very top ... no HUD element when there are no subtitles ...
  fade in from the upper edge in a chamfered background box that behaves exactly like the existing bottom HUD
  elements"):
  - **Fix-text 4 (25 Oct 2026):** the speaker's name rows (the name, the name / faction pair in the tab) take the one fit
    rule (`core\game\textfit.h` via `uidraw::FitText`, cached): shrunk to the 0.024 H / 14 px floor, then cut with an
    ellipsis, inside the panel's own room (the rows' width beside the portrait; the tab's room less the smallest
    portrait, 0.05 H); the tab's two rows share the smaller size. The panel's room ends at the maneuver bar's **live**
    right edge: the frame beyond its rightmost shown button and FLvibed's fifth button (`FLvibedAutopilotButton`)
    where it reaches further (was ManeuverButton3 alone: the comm portrait overlapped the fifth button); a hidden bar
    gives no top room (the panel goes below the top row). Debug `say <id> [voice] as <speaker>` shows a line under any
    name. Frames `research\experience\runs\fix_text4_2026-10-25\` (run.log).
  - **Fix-text 5 (25 Oct 2026):** the bar's frame, widened for the fifth button, ends in a top right ear 0.023 H beyond
    that button's box (measured 1600x1200 and 4K; was taken as 0.008 H): the ear ran 4 px under the 4:3 name tab at its
    leftmost (long names, the portrait tab: tab edge at bar edge + gap) and 3 px under the 4K panel's top left corner.
    The tab layout also tests the panel + tab (the portrait inside one of them) as one shape against the bar's live
    rectangle: the tab must stay right of the bar's edge + gap (after the body's on-screen clamp) and the room between
    the bar and the NeuroNet box must hold the smallest tab (0.16 H); else the panel goes below the top row (as
    `space_narrow` 3) until it hides (`layout` shows `tab-noroom`). Verified 1600x1200, 1024x768 and 4K, classic HUD,
    size 4 / text_size 150 and 4K default. **The portrait stays with its dialogue** (the owner, vanilla's Donau fight at
    1280x960: "the comm portrait was stuck at the left"; it took the nav rule below because the rows beside it had fewer
    than 20 characters, and on the classic HUD without a top panel it stayed at the game's place): where the rows beside
    it are too narrow, the portrait sits above its line inside the panel, centred, the rows full width below (HUD 2 at
    4:3, HDE and vanilla); without a top panel (vanilla's classic HUD: no HudShift, the bar centred, the lines below it)
    it sits beside its line's box at its top, left of it (right where the left has no room). Verified vanilla 1.1 M01a
    (the Donau fight: King, Newark Station) HUD 2 and classic, HDE 1600x1200 HUD 2 and classic (the tab, unchanged).
    Frames `research\experience\runs\fix_text5_2026-10-25\` (run.log; `vanilla_b2_*`, `1600_b2_*`).
  - The **maneuver bar moves to the top left**, mirroring the NeuroNet box at the top right (22 px from the edge at
    4K): the HUD's own position constants, as HudShift moves its ManeuverBar group, by the subtitles host (HostApi v4,
    `ShiftManeuverBar`; `..\..\docs\FREELANCER_RE.md` "HUD panels"). It applies when the HUD is built (a game load).
    The **comm window** (the speaker's portrait and its name lines, top left) moves down with it (user, 21 Oct 2026:
    "the maneuver bar renders over NPC portraits ... those portraits need to be pushed down, along with their
    accompanying text line"): its top 0.010 H below the cruise box under the bar's left end (the box read live; HostApi
    v5 `ShiftCommWindow`, HudShift's Comm group, the window's scale measured 2.135), from the next comm on; not moved
    while the bar stays centred. Verified 4K borderless and 1600x1200 (name-tab layout): M04's opening comms, frames in
    `research\experience\runs\comm_portrait_1021`.
    **The portrait inside the panel** (`comm_portrait 1`, default; user, 21 Oct 2026: "have the portrait moved INSIDE the
    subtitle box during space flight ... surely there's a game-native way to gracefully handle" speakers without one):
    while the game's comm window is open the logic writes its live position and size each frame (FREELANCER_RE "The
    comm window live") so it sits in a square slot at the panel's left (native size, 0.11 H at most), beside the line
    the game opened it for (the newest comm to the player with a costume), the rows centred right of it; the panel is
    deep enough for it, the dividers start right of it, its base and scanline leave a hole there (drawn over it, the
    head showed dimmed), and the slot glides with the rows. The game decides what shows, so every case is native: the
    live head; the generic radio-tower icon (no body built, character detail 0) moved and scaled the same way; no
    window for a comm without a costume, chatter between others or a letterboxed scene - no slot. The game's caption
    under the window is hidden while it is in the panel (the name row names the speaker) and comes back if the slot
    goes first. **Channel closed** (user, 21 Oct 2026: the portrait vanished when the voice ended while the line
    lingered; a frozen last frame was tried and rejected - "the transition ... was stark, and then the frozen portrait
    did not look good" - the user's alternative: an animation that "signifies 'channel closed' in a way that fits
    Freelancer's HUD language"): the slot's picture is captured from the back buffer every 100 ms while live; the frame
    the game closes its window, the last picture folds into a line (0.25 s, washing out to the HUD scanline's pale
    blue), the line shrinks to the centre and goes (0.25 s), and the game's outline - drawn by us once the window is gone
    (130, 201, 243, 1 px at 4K) - shrinks away with it (user: remove the outline "in a way that fits"). **Opening**, the
    mirror (user: "if you have a fade out ... make a fade in", the outline included): a line grows from the centre
    (62.5 ms), the game's outline growing with it, then the live window unfolds from the line (62.5 ms): 125 ms in all (user, 22 Oct 2026; 0.25 + 0.25 s before) - its win_size
    written smaller (the head render squashes with its viewport) and its depth moved so the game's fixed-size outline
    (DrawFrame: 0.207 units around win_pos, the picture 0.200) has the size wanted (this also fits it to a shrunk tab
    portrait). The panel's base leaves a hole for the picture (for the outline once the window is whole). After the
    closing the slot is released: the panel glides narrower and the rows glide over, keeping their line breaks (a line
    laid out beside the portrait keeps that width). **`fade_with_channel 1`** (default; user: "dialogue can ... fade
    out right after the portrait fades out ... make it work like that for all space dialogue"; a user-facing switch
    planned): the line the game opened its window for retires right after the closing animation, never before its
    reading minimum; its slot stays until it has faded (nothing shifts during the fade). A new comm window takes over
    at once. Grabs (4K, 4:3): `research\experience\runs\comm_portrait_slot_1021\channel_*.png`; debug `grab ... comm`
    records from the next comm to the player. Holding the
    game's window open longer was rejected: its comm channel would stay busy and delay the next comm (game logic).
    Grab at 4K: `research\experience\runs\comm_portrait_slot_1021\channel_closed_4k.png`. In letterboxed scenes (dock
    and launch animations) the game opens no comm window at all (Dispatch's letterbox test), and a comm begun there
    stays without one after the bars go: native, no slot. **Narrow screens (tab layout):** the tab widens and deepens to hold the portrait left of the name
    (user: "just about enough room ... if you expand the name part of the rectangle a bit"); a "name · faction" speaker
    takes two rows there, the name over the faction, as the game's caption does; the portrait shrinks to fit (not below
    0.05 H: then the game's place). The neck where the tab meets the body is chamfered like the outer corners (user, 21
    Oct 2026). The tab may use the whole room between the bar and the NeuroNet box, which is wider on the right; when
    it needs more than the centred room the panel moves right of centre (body on screen, clear of the cruise box; the
    Neural Net column moves with it) - user, 22 Oct 2026, "push the NPC name to the right to give space for the
    portrait" (`tab_shifted_4x3.png`: "Escort 1 Off. Overcash" with its portrait). When the portrait's line is not the
    tab's (a second speaker), it sits beside its
    line in the body and glides into the tab once the older line leaves. Verified 21 Oct 2026, 4K borderless and
    1600x1200, dev and release: station dock replies (head), the same at character detail 0 (icon), a 999-frame grab
    of the slide-in; `research\experience\runs\comm_portrait_slot_1021`. Debug `layout` reports the slot.
  - **Speaker names** (user, 21 Oct 2026: "the name of the NPC to appear, and not just their faction ... for both ships
    and bases/solars"): a generic speaker (no shipped name: chatter pieces, dock replies) is named as the game's comm
    caption names it - a ship's pilot (with rank: "Off. Tia Cartwright"), else the object's name ("Fort Bush") - then
    its faction: "Fort Bush · Police" (`space::SpeakerName`, the reputation's name record, so long pilot names are not
    cut at 23 characters as in HDE 0.7.1's own caption; FLSharp 0.9.2's `pilot_names_fix` does the same for the game).
    In the subtitles and the chat window alike; the faction alone when the object has no name or is out of the system.
    **Bar characters** (user, 22 Oct 2026: "Bar NPC dialogue scenes don't include their names") are named the same way
    from the base's own list (mbases.ini [GF_NPC]: individual_name, affiliation, voice): the room character object
    holds its individual_name at +0x160; of the room's characters with the line's voice the speaker is the conversation
    partner (byte +0x36C = 1 while the player talks to it; +0x32C, used first, stays set after a talk), or the only one
    with that voice; else no name. A multi-line name (a fixer's card: group / title / name) shows as "name · title".
    One conversation (user, 22 Oct 2026, notes on b81d522): the faction only on the character's first line, then the name
    alone ("Bob Swaffar · Manhattan Bartender", then "Bob Swaffar"; the same-speaker replace compares the name before
    " · "); a new conversation (the partner byte, followed every frame) fades the previous one's lines at once (the
    bartender's last line stood in the next character's scene). A long inline name with a short line ("Agent Cheryl
    Farley · Liberty Security Force: Hello.") had no valid row break, and the page was cut after "Agent" (the rest
    uncoloured, the "Hello." page flashing by, the name repeated - the user's "duplicated text"): the break right after
    the name is now allowed as the last resort, never one inside it. Verified at Manhattan (4K, dev): the two officers
    sharing rvp111 told apart, "Kim Bailey · Liberty Police, Inc." then "Kim Bailey"; the Agent line whole and coloured.
    In the chat window (user, 21-22 Oct 2026, layout 2 of a four-layout mockup, `chat_layout_mockup.png`: it "doesn't
    go so horizontal as to endanger the center of the screen") such a speaker is "Name:" over the faction (70 % of the
    name's colour) with the text in a column beside them (rows padded with spaces measured in the chat's fonts; two
    spaces after the wider of the two), the first two rows one entry of the log (paragraph node, type 5), a short spacer
    row before each such message (STYLE_CONSOLE, Arial 22). Other speakers keep "Name: text". Sentence rules for the
    chatter (user, 22 Oct 2026): a sentence that would not fit in the rest of a row starts its own row, and so does a clause after a comma, semicolon, colon or dash ("add commas and em dashes"); "This is [x]. To
    [y]" joins as "This is [x] to [y]" (impl.cpp JoinPieces). Shots in `research\experience\runs\comm_portrait_slot_1021`:
    `chat_hanging_4k.png`, `chat_hanging_4x3.png`, `chat_sentence_rules_4x3.png`, `chat_clause_rule_4x3.png`. Open
    (user): 4:3 "looks busy" - options mocked up (`chat_4x3_options.png`: smaller text, narrower column, fewer kept
    messages, no faction row), waiting for the user's pick. Then the user's idea (22 Oct 2026), the narrow-screen
    default (`chat_layout_narrow 1`, 0 = the column; kept by the user after the comparison, 22 Oct 2026): a header row - the name, the faction beside it dimmer - with the
    dialogue under it at full width; on the same four messages it is ~17 % shorter than the column and reads calmer
    (`chat_4x3_column_vs_header.png`, two fresh 4:3 games). Wider screens: `chat_layout_wide` (1 = the header row,
    the default since the user picked it from the 4K comparison `chat_16x9_column_vs_header.png`, 22 Oct 2026; 0 = the
    column). The column remains selectable on both.
    On every aspect ratio (user, 21 Oct 2026: "move the bar left"). **Narrow screens** (4:3, 5:4: aspect < ~1.42, where
    the room between the moved bar and the NeuroNet box holds fewer than 26 characters a row) use `space_narrow`, the
    user's pick from a 4:3 comparison of six layouts (`research\experience\runs\hud_scanline_1021\narrow_4x3\
    comparison_4x3_v2.png`): **5, the name tab (default)** - the first speaker's name in a tab at the very top between
    the bar and the NeuroNet box, the rows in a body stepping out below the top HUD row (from 0.095 H), as wide as the
    space under the bar's lower right allows (clear of the cruise box); a body hardly wider than the tab is one
    rectangle. The others: 1 the panel squeezed at the top (rows of two or three words), 2 the same one font size
    smaller, 3 the panel just below the top HUD row, 4 the bar scaled (0.588) to mirror the NeuroNet box with the panel
    at the top (the game re-lays the bar every frame: rescaled right after, `exe::ManeuverLayout`), 0 the bar centred
    with the lines below it.
  - **One HUD panel** at the top centre holds the dialogue stack (a thin divider between speakers): the bottom panels'
    frame (line, bevel, chamfered corners - here the lower ones), base, scanline and edge breathing, no backing per
    speaker. Its width follows the widest row (at least 0.30 H), at most the live room between the bar's right edge and
    the NeuroNet box's left edge (both read from the HUD controls every 250 ms) less a gap; rows start 0.016 H from the
    top. Where the room is short (the bar not moved yet), the lines use `space_position 1`.
  - **Motion:** it slides down from the top edge while fading in (0.25 s, ease-out) when a line shows, its width and
    height glide as speakers come and go (tau 0.07 s), and it slides back up (0.25 s, ease-in) after the last line;
    nothing is on screen without lines. (The game's own HUD appears in one frame after a launch - the slide is the
    user's request, not a copy.) Recorded at 169 fps: the edge never moved more than 24 px between frames.
  - **The objective banner** (typed at 0.18-0.215 H) moves down below the panel while it is out and back with it.
  - **Legibility:** base (13, 11, 26) at `background_opacity` 60 %: over any background the backdrop stays at or below
    grey 110 - white text at >= 5.1:1 (WCAG AA) before its shadow; the scanline's 6 px pass lowers that to ~3.4:1 for
    ~0.05 s per row every 4 s.
  - Neural Net lines keep their column, right of the panel and under the NeuroNet box.
- **Performance** (measured 21 Oct 2026, debug `perf`: the module's own CPU time per frame per entry point; the same
  flight scene, 8 s windows in the order off / idle / panel / panel / idle / off, background game): no measurable
  frame-rate difference anywhere - 4K 16:9 189-191 fps in every condition and with the subtitles module not loaded at
  all (190.0-190.5); 4:3 (layout 5) 199-200 fps. CPU per frame: enabled and idle (arrows confined) ~10.5 us, the
  two-speaker top panel showing ~58-67 us (almost all its drawing), worst single frame ~0.2 ms - about 1.3 % of a
  200 fps frame, 0.4 % at 60 fps. Two idle lookups removed after the first measurement (the bar layout hook and the
  banner check walked the control list every frame: idle 15 -> 10.5 us).
- **Off-screen arrows clear of the HUD** (`hud_arrows 1`, default; user, 21 Oct 2026: "instead of these pointers having
  the entire window, they'll be dynamically confined between the static HUD elements (and subtitles too)"): every
  arrow part of the target indicators (current target with its outline / hat, non-selected, waypoint, loot, player,
  their backgrounds; brackets and on-screen hats untouched) is moved along its ray from the screen centre to just
  inside the nearest keep-out rectangle it lies in - the bottom panel pairs (anchored on their buttons, only while
  open), the gauge bar, the maneuver bar and its cruise box, the NeuroNet box, the subtitles drawn in the last 100 ms
  (top panel, every block) - each grown by the arrow's half length (0.045 H). The arrow keeps its direction; its
  health / shield bars follow it (the game's own position vector is moved, `..\src\host\main.cpp` HudDrawPartHook).
  Measured (4K, turning, 160 fps, `arrowtrace`): the drawn arrow follows the HUD's outline, its per-frame steps the same
  as the game's (99th percentile 35 vs 38 px), no frame stepping more than 40 px where the game's moved less (no pops
  at the corners). Evidence `research\experience\runs\hud_scanline_1021\arrows2`, `arrows4`, `arrowtrace2.png`.
- **Flight position (`space_position 1`):** hanging just below the maneuver bar and the box under its left end (0.162 H; the bar ends
  0.086 H, the box 0.153 H), closer to the top as the user asked; while the game's objective banner shows (its
  "MissionObjectiveText" control exists: `..\docs\FREELANCER_RE.md`) the stack glides below it (0.235 H) and back
  after. The stack must end above 0.44 H (crosshair 0.5 H): else the oldest speaker retires.
- **Neural Net severity** (`neural_net_severity 1`, default; HUD principles: pre-attentive, one meaning per cue, < 3 Hz,
  never colour alone): the NN column's colour by the line id's severity (`display::NnSeverity`, ids are
  language-neutral) - info green, warning amber (refusals, nags, out of area, insufficient, expired cargo, formation
  lost), danger red-orange (incoming missiles / hostiles, hull breach, shield failed, equipment lost, hazards, mission
  / objective failed) - plus brightness pulses toward white from the line's start: one (warning) or two (danger) at
  2 Hz. An objective destroyed / completed / reached stays info (the player's success). Debug `nnsev <id...>`.
  Verified 19 Oct 2026: all 159 NN ids classified as listed; HULL BREACH IMMINENT via the game's queue pulses twice.
- Verified 19 Oct 2026 (4K, out of focus, screenshots): game fonts load (Agency FB 94 px, Arial Unicode MS 78 px),
  two speakers stack, the older fades and the rest glides up, the stack sits under the banner and glides up when it
  ends. ~~Not yet seen: bases / cutscenes / movies with the new look (bottom-anchored, same code).~~ (Seen since: the
  letterboxed scenes, bases and movies, 21 Oct 2026 - "Next" 3 below.)

### FLUF's windows (26 Oct 2026; built, in game pending - `..\..\docs\MOD_COMPATIBILITY.md` FLUF)
With FLUF (nightly-2026-10-16) the HUD bus v4 (`ReadFluf`) names FLUF's open ImGui windows; FLUF draws them over this
module's text. `display.cpp` `Place` (over the former placement, now `PlaceBase`) moves the block clear of any that cover
its column: a block hanging from the top (the flight top panel - FLUF's command palette sits at the top centre, 0.3 W x
400 px) goes below the window together with its panel frame and comm portrait (`panelTop`, `bodyTop`), a block standing
on its bottom edge goes above a window over that edge while at least 0.35 H is left (else it stays). The keep-outs follow
the controller's `fluf_windows` (2 = every FLUF window, the default since 26 Oct 2026 - the owner; 1 = its player windows; this module alone keeps 2); `inBar` letterbox placements are
untouched. No FLUF or an older bus: the placement exactly as before. Logged once: `display: FLUF ... - the dialogue keeps
clear of FLUF's open windows`.

### HUD 2 (25 Oct 2026; `..\..\docs\UI_HUD2.md`, the bus `..\..\core\hud_bus.h`)
The owner: HUD 2 must keep everything this module does ("find a spot for character portraits", every option of its
menu) and be light (no per-frame searches). `display.cpp` section "HUD 2":
- **When**: the bus's layout advanced within our last 2 frames and says `active` (subtitles HostApi v8 `GetHudBus`; an
  older host or a stale layout = HUD 2 absent: every placement exactly as before). Read once per frame (one copy); with HUD
  2 no HUD control is walked (the native rooms are read afresh the frame it goes, and the placement holds HUD 2's room up
  to 0.3 s until the game's HUD is readable again: one re-place, no detour through the fallback). The panel's place,
  width and height glide as always (tau 0.07 s), so the switch is a short glide; rows re-break once.
- **space_position 2** (default): the top panel inside `topFree` (as HUD 2 publishes it), centred on the screen and moved
  aside as far as its width needs; rows as wide as the band less the panel's sides; the portrait slot, the channel
  animations, `fade_with_channel`, the motion: unchanged. Every `space_narrow` value maps to this panel (the bar the
  layouts arrange is hidden; `topFree` is the room); 2 keeps its smaller font on narrow screens. Rows never narrower
  than 20 characters: a narrower band (HUD 2's first build publishes 0.105 W at 4:3) is spilled over evenly on both
  sides. A band too narrow for
  the rows beside the portrait (fewer than 20 characters left) puts the portrait above its line in the panel, centred
  (fix-text 5, 25 Oct 2026; was: the game's place below the nav).
  The stack ends above the band's bottom or the ring's top, whichever is lower. The maneuver bar stays shifted as
  configured (it returns with the game's HUD); the objective banner moves below the panel as before.
- **space_position 1**: the same band without the panel (a backing per speaker), from the nav's top line, below the
  objective banner while it shows. **0**: between HUD 2's ship panel and loadout, above its context line (its usual
  place when none shows); `bottom_space` < 0.3 or a gap under 28 characters: today's fallback.
- **Neural Net column** (every position): under HUD 2's target card, in its width; while no card shows, at the top
  right (18 characters wide, at HUD 2's right margin) - and a band too narrow for 20 characters a row then reaches to
  that column (the card's place is free); severity colours and pulses unchanged.
- **Comm window's own place** (the last resort since fix-text 5, which puts it above its line in a narrow band's panel
  and beside its line's box without a panel; `comm_portrait 0`, position 0):
  below the nav (HostApi v5 `ShiftCommWindow`, from the next comm on). Since 25 Oct 2026 (FIX-VIEWS) with `comm_portrait`
  on and a line showing, the window is placed **live every frame** under HUD 2's nav rectangle from the bus (x at the
  nav's left, 0.012 H below it, its native size up to 0.11 H; `PlaceCommWindow`, the same slot mechanism as the panel's),
  so a nav that grows rows never runs under it - the same code on every build (no HudShift assumption): HDE 1600x1200 and
  vanilla 1280x960 put it in the same place, HDE 4K keeps it in the panel (`..\..\research\experience\runs\
  fix_views_2026-10-25\comm`). Open: at 4:3 in the keyboard form the panel (topFree) covers the nav's last keycap.
- **Chat column**: the game's chat window cannot be moved from here (no symbol yet); its rows wrap inside `chatFree`
  (`ChatRowMax`: the band's right edge, the log starting 0.01 H from the left) - its native bottom (0.70 H) lies inside
  the mock-up's band.
- **Off-screen arrows** (`hud_arrows`): the keep-outs are HUD 2's nav, target, ship, loadout, context and alert (each
  reaching to the screen edge it lies near), plus the subtitles as before; the ring is not one.
- **Back on the bus** every flight frame (`HudSubtitles`): the dialogue panel as displayed (with its slide; without the
  panel the blocks' extent; empty when nothing shows) and the chat column (while chatter goes to the chat window).
- Letterboxed scenes, movies, bases, sizes, opacity, styles, speaker colours, timing: untouched (no HUD 2 there).
- Debug `hud2sim on|off`: publishes the mock-up's layout (iteration 3) and outlines its parts (yellow) and free bands
  (green, dashed); a real publisher wins (the sim yields while anyone else publishes). `layout` reports `hud2` (state,
  rectangles), `hud2-subs` (what was published) and `hud2-place` (the room used).
- Verified 25 Oct 2026 on instance 7 (SaveC702, dev build, sim on / off): 4K borderless, 1280x720 borderless, 1600x1200
  windowed - the panel in the band with the station's portrait (dock replies), the Neural Net column under the card, the
  chat rows inside the band, positions 0 / 1, the arrows moved by HUD 2's keep-outs, and the same frames with the sim
  off as before; then against HUD 2's first build (`set hud_style 2`, the sim yielding to it): 4K and 720p the panel
  with the portrait between its nav and target card, the switch back to the game's HUD a short glide; at 4:3 its
  `topFree` is 0.105 W (its nav rectangle 0.48 W), so the panel spills over its target card - left to HUD 2's layout.
  Module CPU with HUD 2 and a line showing: ~57 us a frame (as before). `research\experience\runs\hud2_subs_1025\`.

### Built and verified in game (17 Oct 2026)

Runtime: the subtitles module of `FLvibed.dll` (host: hooks, D3D8 capture, debug channel; never reloaded) and its
logic (`FLSubtitlesImpl.dll` in the dev build, hot-reloaded by `..\tools\deploy.ps1 -Impl`; linked into
`FLvibed.dll` in the release build). Data: `EXE\FLSubtitles\subtitles.tsv` (id, voice, kind, source, sec,
speaker, text). Settings: `EXE\FLSubtitles.ini` (`config save`). Game facts: `..\docs\FREELANCER_RE.md`
"Voices and dialogue audio".

| Source | Hook | Status |
|---|---|---|
| Mission comms, ATC, NPC chatter | comm receipt 0x4CE000 (whole message, sender, receiver) | verified (opening, Manhattan traffic) |
| Story cutscenes, room/bar scenes | PlayArchetype 0x428680, caller 0x4575A5; voice from the scene lookup 0x457216 | verified: Manhattan bar entry scene, Juni's M01 scene (21 lines), offer, refusal and re-offer scenes, bar patrons |
| Chatter pieces timing | PlayArchetype callers 0x4CB18D / 0x4CC020 | logged; used later to type chatter in step |
| Neural Net | PlayArchetype, caller 0x42862E (the Neural Net queue player 0x489A80 starts lines via 0x4285F0); comm path for NN messages in comms | verified with `nn <id>` (the game's own queue): INSUFFICIENT FUNDS, SHIP PURCHASE COMPLETE, CARGO JETTISONED, REPAIR COMPLETE in Neural Net green; the 6 silent 0.1 s clips have no text and show nothing |
| Intro movies | Movie.dll imports (PlayMovie / UpdateMovie / StopMovie), logic DLL loaded at the first movie; lines from `data/movies.tsv` on the movie clock; drawn by the software renderer into a child window of the game window above the video renderer's (since 25 Oct 2026; the layered owned window only as the fallback: "Movies" below) | verified: HD intro (`fl_intro`), original (`fl_intro_vanilla`), full original from the Bonus DVD (`fl_intro_full`, only if a player installs it: HDE has no option for it); told apart by ASF duration and size |

Decisions taken from in-game screenshots (3840x2160 borderless):
- **Flight: top third**, at 0.235 H: below the maneuver bar and below the objective banner the game types at the
  top centre (first tried at 0.17 H: it overlapped "DESTROY THE HOSTILE SHIPS"); centred, clear of the ship (chase
  view), the comm portrait (top left), the crosshair (the stack stays above 0.40 H, older speaker dropped), the
  chat log and the bottom panels; leaves the bottom for future controller prompts. Option `space_position 0` =
  between the bottom panels above the throttle bar.
- **Movies: bottom, 2.5 % up** (`bottom_movie`), inside the intro's lower letterbox bar.
- **Movies fit the screen** (user request, 17 Oct 2026; Movie.dll stretched them to the window): aspect kept;
  black bars encoded in a video are zoomed out of view (`data/movie_frames.tsv`: picture area per movie, measured
  with ffmpeg cropdetect; unlisted movies fit whole, frame size from the ASF header). How: only inside the
  renderer, through the filter graph's `IBasicVideo` (host captures the graph from Movie.dll's CoCreateInstance,
  HostApi v3 `GetMovieGraph`): source rectangle = the picture area, destination = the largest centred rectangle of
  its aspect in the renderer window; the overlay paints the margins opaque black. **The windows keep their size**
  (user, 17 Oct 2026: resizing the renderer window disturbed capture tools such as NVIDIA ShadowPlay; the first
  version moved/resized it).
  Verified: HD and original intros (2.42:1 picture spans the width; also in the release build), MGS logo (4:3,
  pillarboxed), DA logo (1.72:1 picture, thin black side margins painted by the overlay; release build).
- **Movies: how the lines get on screen (FIX-VIEWS, 25 Oct 2026; `src\impl\movie.cpp`).** What plays a movie, on HDE
  0.7.1 and on vanilla 1.1 alike (one Movie.dll of 2003): DirectShow's graph `MovieSource -> WMVideo Decoder DMO ->
  "Video Renderer" = VMR-7` {B87BEB7B-...} in windowed mode (logged per movie), its own child window of class
  `VideoRenderer` inside the game window. The game's frame loop - and its Direct3D Present - stops while a movie plays
  (the loop at 0x5B18FD polls UpdateMovie), so nothing drawn with the game's device is seen and **OBS's Game Capture
  sees no movie at all** (neither the video nor lines: nothing is presented through Direct3D; a real OBS test needs the
  owner). Paths, in order (logged as `movie: lines in ...`):
  1. **A child window of the game window** (class `FLSubtitlesMovieText`, `WS_CHILD | WS_CLIPSIBLINGS`, on top; the
     renderer's window gets `WS_CLIPSIBLINGS` so its frames never paint over it). It is opaque, so its window region
     is what shows: the margins around the fitted picture (black; replaces the overlay's margins) + per line its box
     when it has one, else only its text's pixels (glyphs + outline + shadow, alpha >= 24: read as outlined text over
     the picture). Part of the game window: window capture (WGC / BitBlt / PrintWindow with full content) and the
     **taskbar thumbnail** show it - verified by a Windows.Graphics.Capture grab of the game window alone on vanilla
     1280x960 windowed (DPI-unaware, 200 %), HDE 3840x2160 borderless and HDE 1600x1200 windowed
     (`..\..\research\experience\runs\fix_views_2026-10-25\movie_*`), the owner saw the lines in the taskbar preview.
     A child cannot be translucent (layered child windows need a Windows 8 manifest the game lacks), hence the region.
  2. Fallback, when the child cannot be made: the old click-through layered window owned by the game window (blends a
     translucent box; not in captures of the game window, not in the thumbnail - an owned window is a window of its own).
  Not used: the VMR-7's `IVMRMixerBitmap` (offered, see the log) blends only inside the video's own rectangle, while the
  lines sit in the letterbox bar outside the fitted picture; drawing the video itself through the game's Direct3D (the
  only path Game Capture would see) would replace Movie.dll's renderer - not built.
- **Movies: the rate (why they were choppy).** Movie.dll's `UpdateMovie` starts with `Sleep(300)` (Movie.dll
  0x10001730), so the movie loop passes ~3.3 times a second (measured 2.2-3.4 per s) while the video plays on
  DirectShow's own thread; the lines (and their fades) changed only at those passes - and a line started up to 0.3 s
  late. Now Movie.dll's import of `Sleep` is chained while a movie plays: the same total wait in slices - one composition
  frame each (`DwmFlush`) while a transition runs, 50 ms otherwise - with the line clock and a redraw between them; the
  window is repainted only when the picture's key changes (texts, positions, opacities). Measured over the intro
  (transition frames = the gaps between picture updates while a fade / glide runs; 240 Hz display): before one update
  per pass (~300 ms gaps); after vanilla 1280x960 median 4.2 ms / p95 6.9 ms, HDE 1600x1200 4.2 / 8.2 ms, HDE 3840x2160
  4.4 / 12.5 ms (the stop line `movie: ... stopped after ...`).
- **Movies: the box only where the picture needs it** (owner, 25 Oct 2026): a line whose box lies wholly outside the
  picture (on the letterbox bars, from the fit's destination = `data/movie_frames.tsv`) has none; inside the picture a
  64 x 36 luminance grid of the current video frame (`IBasicVideo::GetCurrentImage`, read on a worker thread once a
  second only while a line reaches into the picture: 57 ms at 720x480, up to 0.6 s at 1440x960 - never on the loop's
  thread) judges it: busy = max(spread / 40, change since the last grid / 20), box on at 0.55 (0.45 at a line's first
  look), off at 0.35 (hysteresis). The child window shows the box or not (a hard cut, judged once a second: no flicker);
  the layered fallback blends it by how busy. Without a box the text keeps a 4-way outline and its shadow. Open: whether
  the 0.6 s read-back at 1440x960 costs the HD intro a frame (the VMR-7's IQualProp counts in windowed mode are not
  usable: 18-65 frames drawn reported for 50 s); the owner's eye on the HD intro decides, else read the frame from the
  game window's own picture instead.
  **Answered 26 Oct 2026 - the reads are OFF:** on the disc install (original 1.1) the owner heard the intro's audio
  break up while its lines showed. Measured on the intro (75 s each, the stop line's new audio figures from the audio
  renderer's IAMAudioRendererStats): FLvibed hands-off 2,255 frames / 0 discontinuities; subtitles without reads 2,257 /
  0; **with the reads 8 frames drawn, 50 discontinuities** (the read stalls the renderer; one read took 441 ms in the
  owner's run). **The owner then decided: no box on movie lines at all** (26 Oct 2026) - the text's outline and shadow
  carry it (verified over the intro's picture; `display::SetMovieBoxDefault`). Debug `moviesample on` brings the reads
  and the judged box back, for research only.
- **Movies: word-timed pages** (owner, 26 Oct 2026: "run the forced alignment on the intros"). The intro lines were
  timed by `intro_align.py` (speech to text in 2 s windows, about +-0.5 s) and paged by the estimate. Now the review's
  aligner force-aligns each line inside its window of the movie's audio (`review\FLSubtitles\aligner\align_words.py`,
  three windows +-0.4 / 0.75 / 1.25 s, a word counts where two agree; `review\docs\REVIEW_PROCESS.md` section 1) into
  `data/word_times.tsv` rows with id = the movie, voice `movie`, times on the movie clock. `movie.cpp` reads them at
  load (`movie: word timings for N of M lines`), matched by movie + the text's FNV-1a hash and the word count (a text
  changed since its alignment, or a missing row, keeps the estimate; `movie_words.h`, unit test
  `tests\movie_words_test.cpp`), and hands `display::Show` the starts from the line's own start: a movie line pages
  and pops on its words exactly like dialogue; -1 words keep the estimate for their page. 83 of 90 intro lines timed
  (fl_intro 20 / 22, fl_intro_vanilla 19 / 22, fl_intro_full 44 / 46); 52 windows the alignment placed unambiguously
  moved to the words (`review\FLSubtitles\out\movie_window_changes.tsv`); lines to listen to:
  `review\FLSubtitles\out\movie_word_times_suspect.tsv`.
- **Cutscenes and bases: bottom, 2.5 % up**, inside the lower letterbox bar of cutscenes and room scenes.
- Chatter → chat window as "Faction: sentence" in the attitude colour (server.dll names); `chat_all_dialogue 1`
  (user request) copies story/comm/NN lines there too as a history. Since 26 Oct 2026 the split is the game's portrait
  decision ("Priority and the side chat" above).
- Speaker colours: Trent amber, story characters their own colour, others by relation (21 Oct 2026, "Speaker colours").
- Line breaking: whole sentences per page while two rows fit; a row ends at a sentence end where possible; never
  after an article/preposition or between two capitalised words (names).

### Both modules on one screen (user, 17 Oct 2026)

The modules must work alone and together, and the controller will add its own HUD elements (glyph prompts).
Plan (now simpler: both live in FLvibed.dll): a shared layout registry in `core\` where each module publishes the
screen rectangles it currently occupies (normalised to the screen height: owner, rect, priority, expiry) and lays
itself out around the others' rectangles and the game's HUD controls (projected through the UI camera, as the
controller's ui.cpp does) instead of fixed fractions. Until then the fixed fractions above are measured on the
HDE HUD.

### Testing tools (both modules, no cursor where the game allows it)
- `..\tools\deploy.ps1 -Save <file>` / `Load-FLSave <file>`: load any save by file name, natively, from any state.
- Native game actions (no cursor), in both modules' debug channels (`..\core\game\room.cpp`): `talkers` /
  `talk <n|story|rumor…>` (room characters), `click <Control>` (text, OK and icon buttons: rooms, HUD, NN menu,
  popups); the controller's `Select-FLItem` clicks natively first.
- Waiting on the game, not timers: `Watch-FLGame` (modal dialogs incl. tutorial popups, offers, the mission
  briefing; events; mode), `Wait-FLSShown` (the next subtitle / chat line, also during movies), `Close-FLModal`.
- `Set-FLSIntro -Intro <file> [-NoLogos] / -Restore`: choose the test install's startup movies.
- Rooms by function, not by control number: `hotspots` lists label → control (RoomControlN / ButtonControlN are
  numbered per room), `goto <label>` clicks it natively (`goto bar`, `goto cityscape`, `goto news`).
- `nn <line id>`: queue a Neural Net line through the game's queue (a first request may be dropped while its
  sound loads: send it again). `state` reports `frames`; deploy.ps1 waits for it to skip the movies.

### Next
(22 Oct 2026: paused at a stopping point by the user, "more to come later".)
0. (Done: the portrait inside the top panel and the speakers' own names, seen and kept by the user 21-22 Oct 2026; bar
   characters named from the room, 22 Oct. Not yet seen: the portrait at 21:9.)
0b. (26 Oct 2026, e145662: the comm portrait's capture - a back-buffer readback while a comm plays - runs only on the
   system's own Direct3D 8, because under HDE's DxWrapper / d3d8to9 it broke every later device Reset;
   `..\..\docs\MOD_COMPATIBILITY.md` FLUF "Alt+Enter".) Open: **under HDE the portrait is no longer held through its
   closing animation** (a follow-up: hold it without a readback); the owner's question whether to keep the readback on
   vanilla's system D3D8 (kept on; a failed Reset cannot be shown there, the original game never retries).
1. Chatter typed in step with its pieces: each piece now has word timings (`word_times.tsv`), joined chatter does not
   use them yet (its pieces play back to back - the comm path's offset sum would do). Blowoff patrons have no
   `individual_name` in mbases.ini: no name. (Neural Net: done.)
2. (Done another way, 18 Oct 2026: campaign saves for every mission - the BC46 set - and catalogued scene saves,
   `..\..\docs\missions\SAVES.md`.)
3. Aspect ratios 4:3 / 21:9, fullscreen, windowed; both modules' shared screen layout (above). *21 Oct 2026: the sweep's
   `display-matrix` section shows the flight subtitle in place at 1600x1200, 2560x1080, 1280x720, 3840x2160 and in a
   DPI-unaware process (`..\..\docs\kb\display.md`); fullscreen only attended; the movie lines (since 25 Oct 2026 a child
   window, "Movies" above) verified at 3840x2160 borderless, 1600x1200 windowed and vanilla's 1280x960 windowed in a
   DPI-unaware process; exclusive fullscreen not tried.
   The `display-subtitles` section judges real dialogue (room scene, space cutscene, flight comms) in 4:3 and 21:9 by
   the module's `layout` report: the rules hold in every mode. Found: the game's letterbox bars are 0.140 H; a name row
   + two text rows (0.147 H) tops the bar by 0.03 H and a second speaker stacks over the picture - decided 21 Oct 2026:
   `letterbox_mode` 0, one speaker inline in the bar (above, and `..\CLAUDE.md`);
   the field research (`SUBTITLE_PRACTICES.md`: guidelines, the options and defaults of TLOU2 / GoW Ragnarök /
   Cyberpunk / Ubisoft / Spider-Man 2 / Horizon, and how Outlaws places subtitles in letterboxed scenes) proposes a
   letterbox mode: one speaker at a time, either inside the bar with inline names and no backing, or Outlaws-style
   resting on the bar in the picture with the name row.*
4. In-game settings page with a preview (XAG 104).
5. For the user (in the review, `subtitles\review\`): listen to the `[check]` lines in `FLSubtitles/data/movies.tsv` and
   the movie lines the forced alignment flags (`FLSubtitles/out/movie_word_times_suspect.tsv`) (radio
   chatter under music); "Sa-14" in their saved correction of DX_M01_0010_FREEPORT_7_BRIDGE (the line before
   says "SA-14").
6. For the user: a convention for deliberately garbled audio in the text; the aligner's suspect list
   (`FLSubtitles/out/word_times_suspect.tsv` in `subtitles\review\`). Scene mixes (several speakers in one clip) need the user's
   own subtitles before they can be timed.

## Testing

**Granted by the user (17 Oct 2026):** create the HDE test install (official installer, UAC prompt the user
approves); run the game from it, load saves, drive missions with the controller mod's tools and take
screenshots of the game window without asking each time; search online for campaign saves, then show the
exact files (source, name, size) for one approval before downloading.


- FLvibed's HDE test install `..\Install\Freelancer HD Edition\` (made with the official installer from
  `..\research\Vanilla\`; admin prompt), run the game, load saves, take screenshots when the other tools
  are not enough.
- Saves: searched 17 Oct 2026 (GitHub code search, The Starport, ModDB, savegame.pro/.info/world,
  savesforgames, vgtimes, igroport/bestgamer, Jason Hood's page): only new-game, end-game and cheat saves,
  nothing spread across the story. Plan: make our own by playing the campaign (harness), saving at each
  mission start, so loadout, reputation, credits and the next-level target are real (user: never invent them).
- (original brief) Saves spread across the campaign: look online for HD Edition saves (priority) or vanilla saves; list
  the files (source, size) and ask before downloading.
- Follow each mission's script closely when completing objectives (timers and nags fail missions); monitor
  mission flags when needed. The controller module's tools and knowledge (`..\controller\`: test saves,
  mission runbooks, harness) are part of the same repository. Game facts: `..\docs\FREELANCER_RE.md`.
- Goal: test subtitles the way a player would see them, mostly without the user's attention; the user
  will test personally later.
