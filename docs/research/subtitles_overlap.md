# Overlapping speakers: what the subtitles show when two comms talk at once
Status: current
Checked against: FLvibed 97e118fb (31 Oct 2026 ~15:25, short form, 97e118fb landed during the round (docs only: AUTOPILOT.md records fx2 - the back-off fires, the stall not fixed; PRESETS / WORK_QUEUE record the lint baseline as run), read and consistent; 1 watched-path commit (32c80fce: UI_HUD2_PRESETS.md status line + 6 item 3, the HUD 2 layout lint; no subtitle, speaker or overlap rule); no effect)
Dirty paths at check: another session's uncommitted `controller/src/impl/autopilot_impl.h`, `controller/src/impl/autopilot_objective.cpp`, `controller/src/impl/autopilot_settings.cpp`, `controller/src/impl/lookahead.{cpp,h}`, `core/fluf.cpp`, `tests/fluf_probe_test.cpp`, `tools/setwatch.sh` (not judged) (31 Oct 2026 ~15:25)
Watch: subtitles/src/impl/display.cpp, subtitles/src/impl/impl.cpp, subtitles/docs/DISPLAY_PLAN.md, subtitles/docs/SUBTITLE_PRACTICES.md, docs/UI_HUD2_PRESETS.md, docs/SETTINGS.md#subtitle|speaker|portrait|chat
Evidence: research\evidence\subtitles_overlap\ (`sources.txt` every URL with how it was read; `voice_priorities.txt` the game's own comm priority ladder)
Sources: guidelines and engine docs [page], game reviews [page], search summaries where a page refused the fetch [search], recollection [memory, unverified], the game's data [file]

**Question (the owner, 30 Oct 2026):** two or more comm speakers talk at once; today the flight top panel stacks them
(at most two blocks, each with its name, the portrait slot for the comm's speaker; DISPLAY_PLAN "Motion": "at most two
speakers, a third retires the oldest"; `display.cpp` Insert ~442), and two comms whose voices both still play stay
together even past the height limit (DISPLAY_PLAN "Open (owner)", 26 Oct 2026). What should the **default** be? The
owner: no stacking of subtitles / portraits by default, one speaker at a time, priority to the more important or the
later one - "but industry standards and accepted behavior is preferred". An "Expanded" preset keeps today's stacking.

## What it means for FLvibed (30 Oct 2026, at 6c9a872)

**There is no written industry rule for simultaneous speakers in games; there is a consistent hierarchy.** Every
guideline caps the subtitle area at **two lines (three exceptionally)** (XAG 104, BBC, Netflix, Hamilton), and every
source that speaks to conflict says **important speech wins over background speech** (Netflix: plot-pertinent dialogue
"always takes precedence over background dialogue"; GAG: important speech covered first, flavour speech by default or a
toggle; Hattner: a "priority display system to manage background VO"; Hamilton: less important subtitles "would drop
off"). The one shipped engine rule (Unreal's subtitle plugin) is exactly the owner's default: **one subtitle per
category at a time, the higher priority wins, a lower one is not shown**. Broadcast practice allows two speakers in one
two-line subtitle (Netflix dual-speaker hyphens, BBC colours), and the TLOU2 / Tomb Raider pattern stacks two - so the
"Expanded" stacking is also accepted practice, as an option. FLvibed's own game already ranks every comm: `voices_*.ini`
`Priority` (0 for missions' speech to the player, -2 police scans, -4 / -5 combat chatter, -6 fragments) and
`COMM_CONFLICT_PRIORITY_CUTOFF = -3` (`voice_priorities.txt`) - the priority can come from the game, not a list (S10).

**Recommended default ("Focused", S10):**
1. **One speaker block on screen** (one name, one portrait), at most two text rows plus the name row (three rows only for
   a page that cannot break); the game's comm window shows one channel anyway (FREELANCER_RE "comm player state": one
   current comm, a priority, a pending queue).
2. **Priority classes, from what the game decides** (the split `impl.cpp` OnComm already makes, DISPLAY_PLAN
   "The split"): **A story** (scripted DX_ lines, the mission's comms, Neural Net stays in its own corner) > **B directed**
   (a comm the game opens its portrait for: dock replies, a wingman or job contact to Trent; voices `Priority` >= -3) >
   **C chatter** (no portrait, gcs_ between others, broadcasts; `Priority` < -3) - C never takes the slot (already the
   side chat).
3. **Higher class holds, lower class waits or goes to the chat log:** a new line of a lower class does not replace a
   shown higher one (Unreal's rule, Netflix's precedence); it goes to the chat feed / log in full (nothing lost: XAG
   "subtitles for all spoken content" is met by the log), and is **not** shown later out of sync.
4. **Same class: the newer one wins** (it is the voice that just started; subtitles stay in sync with the audio - the
   rule already chosen for letterboxed scenes, SUBTITLE_PRACTICES 4). The replaced line fades out over the existing
   0.5 s fade in place (no kick-up, no second block) and goes to the chat log; a line replaced before its reading minimum
   is the known cost - the log and the Expanded preset are the remedies. Conflict to note: Unreal keeps the *older* of
   two equal-priority subtitles (below); the newer-wins choice follows the owner and the sync principle.
5. **Return, not queue:** when the winning line ends and the displaced line's voice still plays for >= 1 s, the
   displaced line comes back (with its name) for the rest of its audio. No source prescribes this; it keeps the default's
   one-block rule while losing less (a FLvibed design choice, mark it so in the spec).
6. **The same speaker going on** keeps today's in-place replace (DISPLAY_PLAN, 22 Oct 2026).
7. **The portrait follows the shown block** (the game's own window already shows one channel).

**Expanded preset:** today's behaviour - up to two speaker blocks stacked (a third retires the oldest), the voice-hold
rule winning over the height limit, `retire_delay` grace; the TLOU2 / Tomb Raider pattern. Letterboxed scenes keep
`letterbox_mode` / `cinematic_single_speaker` in both presets (already one speaker at a time). The setting is one key
(e.g. `space_speakers 1 | 2`), generalising `cinematic_single_speaker` (`display.cpp:95`) to flight; the options page and
the living spec get the row (UI work, not research).

## Findings (30 Oct 2026)

### 1. Guidelines
| Source | On overlap / priority | On lines and speakers |
|---|---|---|
| **XAG 104** (Microsoft, page updated 17 Jun 2026) [page] | no overlap rule; categories "main character dialogue, background character dialogue, or ambient noises" toggled independently | "no more than two lines of subtitles on screen at a time" (three exceptional); name "each time a new speaker begins talking"; colour never alone; "(on radio)" and arrows for off-screen speakers |
| **Netflix English (USA) TTSG** [page] | I.17: "Plot-pertinent dialogue always takes precedence over background dialogue"; II.10 on-screen text: "precedence should be given to the most plot-pertinent message" | "Maximum two lines"; two speakers in one subtitle with hyphens, "a maximum of one speaker per line"; 20 cps adults |
| **BBC Subtitle Guidelines** (bbc.co.uk refused the fetch; via clevercast.com [page] and search summaries [search]) | nothing on overlap found in the summaries; timing is "ultimately an editorial decision" | two lines, three "if ... no important picture information will be obscured"; colour the preferred speaker identification (white, yellow, cyan, green) [search] |
| **Ofcom** TV access services guideline (PDF refused, 403) [search] | - | colours per speaker retained since the 2006 review [search, not read] |
| **FCC** caption quality (403) | - | [memory, unverified]: accuracy, synchronicity, completeness, placement - no overlap rule |
| **DCMP Captioning Key** [page] | the Special Considerations page has nothing on simultaneous speech | identify on- and off-screen speakers; place the caption near the speaker when possible |
| **W3C WAI captions** [page] | nothing on overlap | speakers via WebVTT `<v>` voice tags |
| **Game Accessibility Guidelines** [page] | supplementary speech "can either be included by default, or through a separate toggle", after "all important information is covered" | speaker by colour, name, portrait, position; "combining" them works better |
| **Ian Hamilton** (80.lv; md-subs) [page] | some subtitles "are more important than others (think main dialogue versus background chatter)"; less important ones "would drop off" | two lines (three exceptionally), <= 38 characters; portrait, name or colour, let the player choose |
| **Kari Hattner** (Hangar 13, ex-Crystal Dynamics; talk transcript, partly garbled) [page] | "develop a priority display system to manage background VO"; all critical dialogue subtitled | new subtitles at the bottom pushing older ones up; Tomb Raider added positions to show "two subtitles at the same time" |

### 2. Engines and games
| Game / engine | What shows when lines overlap | Source |
|---|---|---|
| **Unreal Engine** Subtitles and Closed Captions plugin | "Only one of each category ... will display at a time"; the highest priority plays; a lower one "will not be displayed", even on partial overlap; equal priority: "the oldest one queued will continue to display"; default priority 1; categories Subtitle / ClosedCaption / AudioDescription positioned apart | [page] |
| Unreal legacy FSubtitleManager::QueueSubtitles | "higher values have higher priority"; priority 0 not displayed | [page]; a newer page reportedly says "the most recent of the highest priority is displayed" [search, not read] - **conflicts** with the plugin page on ties |
| **Halo Infinite** | Display: **Story Only** (major characters) or **All**; speaker / line highlight | [search] (Waypoint 403); CIPT review [page] confirms speaker labels and colour |
| **The Last of Us Part II** | Story vs Story + Combat (systemic enemy dialogue); names and direction separate toggles | ND blog [search]; CIPT: every audible line subtitled [page]; FLvibed's own frame study (DISPLAY_PLAN "Look and motion", 19 Oct 2026): a bottom queue of at most **two** lines, a third retires the oldest |
| **Star Wars: Squadrons** (space combat) | captions for key characters, pilot and environmental dialogue; usually one line, rarely over two "felt a little overwhelming"; story mode shows the speaker's name / icon with a comm HUD element | CIPT [page] |
| **Black Ops Cold War** | one subtitle switch for cinematic and in-game dialogue, **off by default** | Activision support [page] |
| **Adventure Creator** (Unity adventure framework) | default: subtitles show "only ... the latest to speak"; duplicate-per-line is an opt-in | forum [page] |
| **Elite Dangerous** | NPC messages as text in the comms panel; no spoken-dialogue subtitles found | [search] |
| Ace Combat 7 | radio lines one at a time at the bottom with the callsign; newer radio replaces | [memory, unverified]; reviewers note characters "shouting over each other" [search] |
| Valve Source captions | captions collect in one box, oldest scroll off; `cc_linger_time` | [memory, unverified]; valvesoftware.com refused (403) |
| Mass Effect, X4, Star Citizen / Squadron 42, Battlefield | nothing found on overlap | searches came back empty |

### 3. Freelancer's own model [file]
- `voices_*.ini` `[Sound] Priority` (KnowledgeBase: "-infty to 0"): story mission files carry almost none (8 lines in M01
  at -4, 2 in M07 at -1, 1 in M03 at 1); `voices_space_*`: 0 = `rms_*` (job speech to the player), -2 = `gcs_scan_*`
  (police to the player), -4 / -5 = combat chatter, -6 = joined fragments (`gcs_misc_number`, designations); 
  `voices_recognizable.ini` -3..-5. `constants.ini [CommConsts] COMM_CONFLICT_PRIORITY_CUTOFF = -3` (also
  `WALLA_PRIORITY_CUTOFF = -3`). The cutoff's exact effect (which comm yields on a conflict) is **not verified** in the
  code [assumed: comms below -3 are dropped or not queued when a comm is playing]; the comm player keeps one current
  comm, its priority (+0x45C) and a pending queue (+0x454) (`docs\FREELANCER_RE.md` "comm player state", verified 18 Oct
  2026). The game itself therefore plays one player-channel comm at a time and ranks story / directed above chatter;
  overlaps the owner sees come from the scripted lines started at once (Newark under the Donau, DISPLAY_PLAN
  "Overlapping comms"), not from the comm channel.

### 4. Conflicts and gaps
- **Stack vs single:** Hattner / TLOU2 stack two; Unreal and Adventure Creator default to one; broadcast allows two
  speakers inside one two-line subtitle. All agree on the two-line cap - so both FLvibed presets are inside practice;
  the difference is screen space.
- **Ties:** Unreal's plugin keeps the older equal-priority subtitle; Adventure Creator and (reportedly) Unreal's newer
  API show the latest; broadcast sync rules favour the line whose audio is playing. FLvibed's choice (newer wins) is the
  owner's and the sync principle's.
- **How long a replaced line stays:** no source gives a number. The only numeric linger rules are per-line (Valve
  `cc_linger_time` [memory], Unreal's duration default 3 s [page]); FLvibed's own `retire_delay` 1.25 s and the 0.5 s
  fade are its own decisions (DISPLAY_PLAN "Timing").
- **Not reached:** BBC, Ofcom, FCC and Valve pages refused the fetch; the BBC's own text should be read before quoting it
  as the source of a rule.
