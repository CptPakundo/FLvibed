# Research: index, structure and the rules that keep it current

Research = what the project learned from outside sources (other projects, mods, communities, decompilations,
speedruns) and what it means for FLvibed. It is kept **for use**: by topic, with one backlog of ideas, each idea
with a status and the project state it was last checked against. Timestamps stay, structure comes first.

## Standing orders (user, 21 Oct 2026)

1. **Structured for use.** One note per topic (not per session), the same sections in every note, one deduplicated
   backlog (`IDEAS.md`) that the notes feed. A reader must find "what does this mean for FLvibed now" in the first
   screen of every note.
2. **Never stale.** When the project changes, the ideas touching that area are re-checked and their statuses fixed.
   When new research is done, the whole set goes through the check-up round (below), not only the new part.
3. **Concurrency-safe.** Research runs alongside sessions that change the project. Every note and every idea
   records the FLvibed commit it was checked against; conclusions are checked against the *committed* state, and the
   uncommitted paths present at check time are listed so a later reader knows what was in flight. Research sessions
   write only `docs\research\` and `research\evidence\`; they never touch `HANDOFF.txt`, code, `CLAUDE.md` (beyond
   the pointer to this folder) or the working session's docs. Findings those docs should carry go into `IDEAS.md`
   as items for the working session.

## Where things are

| Path | What |
|---|---|
| `docs\research\README.md` | this index and the process |
| `docs\research\IDEAS.md` | **the backlog**: every actionable or theoretical idea from any research, deduplicated, with id, area, status, source, checked-against |
| `docs\research\<topic>.md` | one note per topic: header (status, checked against, evidence), "What it means for FLvibed" (links to idea ids), then the findings in full |
| `research\evidence\<topic>\` | git-ignored raw material the notes cite: downloaded files, extracted inis, manifests, exes for signature regressions, data tables, scripts |
| `tools\research_check.ps1` | prints HEAD, the dirty paths, and for every note the commits since its checked-against commit that touch its watched paths |

## Notes

| Note | Topic | Checked against | Status |
|---|---|---|---|
| `freelancer-decomp.md` | FreelancerDecomp/Freelancer-Decomp: full C++ rebuild of the 1.1 client; symbol cross-reference | e5c02cc (26 Oct 2026, step 3 short form after the FLUF harmony work, the Reset / dealer / subtitle / pad / borderless fixes: a dated first "What it means" line; 202 / 202 signatures); before: 855745b (25 Oct 2026 ~23:05, whole-set check-up: 195 symbols, the xref 72 behind, all new ones found in game); before: 073a665 (25 Oct 2026 ~12:15, short form after the HUD 2 build: 8 more symbols found in game, 159, the xref 36 behind; the news flash state and the bracket draw as decomp reading); before: abf84ac (the UI build) | current |
| `flsharp.md` | BC46/FLSharp: the HDE plugin that moved from 0.8 (test install) to 0.9.2 (upstream HDE) | e5c02cc (26 Oct 2026, step 3 short form after the FLUF harmony work, the Reset / dealer / subtitle / pad / borderless fixes: a dated first "What it means" line; 202 / 202 signatures); before: 855745b (25 Oct 2026 ~23:05, whole-set check-up: CurrentCursor shared, the ship dealer readers beside FLSharp's dealer patches - no clash, sweep items U4 e / f); before: 073a665 (25 Oct 2026 ~12:15, short form after the HUD 2 build: HUD 2's root hiding and hotspot box read against FLSharp's source - no clash, four second-install sweep items; two more shared addresses); before: abf84ac (the UI build) | current |
| `discovery.md` | Discovery Freelancer 5.3: patched 1.1 exe, FLUF, DSAce; signature regression | e5c02cc (26 Oct 2026, step 3 short form after the FLUF harmony work, the Reset / dealer / subtitle / pad / borderless fixes: a dated first "What it means" line; 202 / 202 signatures); before: 855745b (25 Oct 2026 ~23:05, whole-set check-up: **195 / 195 signatures on Discovery's exe** after the 68755f1 re-cut; 22 still span padding); before: 073a665 (25 Oct 2026 ~12:15, short form after the HUD 2 build: signatures measured - 139 of 151 parsed match, all 12 misses padding; HUD 2's 8 match); before: abf84ac (the UI build) | current |
| `crossfire.md` | Crossfire 2.0.1: launcher-run `EXE2`, anticheat, one-script campaign | e5c02cc (26 Oct 2026, step 3 short form after the FLUF harmony work, the Reset / dealer / subtitle / pad / borderless fixes: a dated first "What it means" line; 202 / 202 signatures); before: 855745b (25 Oct 2026 ~23:05, whole-set check-up: movie subtitles in the game window, vanilla 1.1; S5 / S7 unchanged); before: 073a665 (25 Oct 2026 ~12:15, short form after the HUD 2 build: the comm feed is a new surface, not a new line source; S5 unchanged); before: abf84ac (the UI build: S7 answered in effect) | current |
| `fluf.md` | FLUF (Freelancer Universal Framework, The Starport): status (active, AGPL-3.0, refuses HDE's plugins), every hook it shares with FLvibed, "FLUF in play" for the target build nightly-2026-10-16 (its UI, hook sites, FLvibed's awareness layer, the in-game result in both orders, known issues) | e5c02cc (26 Oct 2026, step 3 short form after the FLUF harmony work, the Reset / dealer / subtitle / pad / borderless fixes: a dated first "What it means" line; 202 / 202 signatures); before: 855745b (25 Oct 2026 ~23:05, whole-set check-up: the da1363f / 5cd02b9 updates kept, three pre-da1363f statements marked; download OKed, run pending); before: 35b71fd (25 Oct 2026 ~23:00, new; FLUF 627ea2c) | current |
| `starport.md` | The Starport, Codeberg/GitHub orgs, KnowledgeBase, adoxa, other decomps: sources not on hand | e5c02cc (26 Oct 2026, step 3 short form after the FLUF harmony work, the Reset / dealer / subtitle / pad / borderless fixes: a dated first "What it means" line; 202 / 202 signatures); before: 855745b (25 Oct 2026 ~23:05, whole-set check-up: the docked-screen control map new ground); before: 073a665 (25 Oct 2026 ~12:15, short form after the HUD 2 build: the flight HUD's control map is new ground too); before: abf84ac (the UI build) | current |
| `speedrun.md` | speedrun.com records, routes, money gates, skips vs the autopilot; **the top-3 recordings analysed frame by frame** (legs, sections vs our legs, fights, docks, money, loadouts) | e5c02cc (26 Oct 2026, step 3 short form after the FLUF harmony work, the Reset / dealer / subtitle / pad / borderless fixes: a dated first "What it means" line; 202 / 202 signatures); before: 855745b (25 Oct 2026 ~23:05, whole-set check-up: **"What it means" re-based** on SPLITS.md 24 Oct 23:55 + dossier M04 through s13b); before: 12bc233 (25 Oct 2026 ~12:15, short form after the HUD 2 build: 15 commits, handoffs and kb only, none changing a finding); before: abf84ac (the UI build; "What it means" re-based on SPLITS.md at 1defa3f) | current |
| `hde-upstream.md` | Switching the test install to the upstream HDE + FLSharp 0.9.2: what differs, what it costs, when (recommendation: second install now, primary at v0.8) | e5c02cc (26 Oct 2026, step 3 short form after the FLUF harmony work, the Reset / dealer / subtitle / pad / borderless fixes: a dated first "What it means" line; 202 / 202 signatures); before: 855745b (25 Oct 2026 ~23:05, whole-set check-up: 195 / 195 static on the upstream exe; vanilla 1.1 a third exe); before: 073a665 (25 Oct 2026 ~12:15, short form after the HUD 2 build: point 1 confirmed statically - every parsed symbol matches on the upstream exe; HUDless differs between the installs); before: abf84ac (the UI build) | current |
| `controller_ui.md` | The controller UI round (25 Oct 2026): flight presets round 2 and cross-genre reflexes, device switching the Ubisoft way, glyph families and pad detection, pad cursors and dense screens, assist presentation, speedrun modes, LiveSplit; feeds `docs\UI_PLAN.md` section 6 | e5c02cc (26 Oct 2026, step 3 short form after the FLUF harmony work, the Reset / dealer / subtitle / pad / borderless fixes: a dated first "What it means" line; 202 / 202 signatures); before: 855745b (25 Oct 2026 ~23:05, whole-set check-up: Extended HUD 2, the bottom prompt lane as a departure from U19, glyph_look, U27 complete); before: 073a665 (25 Oct 2026 ~12:15, short form after the HUD 2 build: U27 built, U24 / U25 superseded, U26 built in HUD 2, departures marked); before 3ebd169 (25 Oct 2026 ~10:30, pad HUD round 3: HUD 2 from the ground up, U27 proposed), 815c9c1 (round 2), abf84ac (25 Oct 2026 ~07:00, short form after the UI build: U15-U23 built, departures marked in place) | current |
| `options_menus.md` | How well-regarded games do options menus (presets, explainers, previews, assists; GAG / XAG 104) and Freelancer's own Options screen measured; feeds `docs\UI_PLAN.md` | e5c02cc (26 Oct 2026, step 3 short form after the FLUF harmony work, the Reset / dealer / subtitle / pad / borderless fixes: a dated first "What it means" line; 202 / 202 signatures); before: 3fa62f0 (25 Oct 2026 ~23:05, whole-set check-up at 855745b + the SETTINGS.md regeneration during the round: ui_style / glyph_look rows; U12 gap stands); before: 073a665 (25 Oct 2026 ~12:15, short form after the HUD 2 build: the HUD section keeps the tier rule; the replica preview misses HUD 2's fonts); before: abf84ac (the UI build: U10-U14 built) | current |
| `llm-npcs.md` | LLM NPCs and AI assistants in games (SkyrimNet, Mantella, CHIM, ImmersiveAI, COVAS:NEXT, ATC products...), and what the same would take in Freelancer (far later) | e5c02cc (26 Oct 2026, step 3 short form after the FLUF harmony work, the Reset / dealer / subtitle / pad / borderless fixes: a dated first "What it means" line; 202 / 202 signatures); before: 855745b (25 Oct 2026 ~23:05, whole-set check-up: HUD bus lanes, room walk / talk, fit rule - building blocks); before: 073a665 (25 Oct 2026 ~12:15, short form after the HUD 2 build: the HUD bus and alert cards as N10's display surface); before: abf84ac (the UI build: the OSK and prompt bar as N2 / N3 building blocks) | current |

Status values: **current** (checked against the commit shown; nothing contradicts it), **partly stale** (some
conclusions overtaken; the note says which), **superseded** (kept for history; see the newer note), **retired**.

## The check-up round (every research session ends with it; every project update triggers the short form)

1. `tools\research_check.ps1`: HEAD, dirty paths, and per note the commits since its check that touch its watched
   paths (declared in the note header as `watch:`).
2. For each touched note: re-read the "What it means for FLvibed" section against the commits; fix statuses in
   `IDEAS.md` (done / superseded / still open), update the note's header (`checked against`, `dirty paths`), and
   mark contradicted findings in place rather than deleting them.
3. New research: add or extend the topic note (never a per-session file), merge its ideas into `IDEAS.md`
   (deduplicate: one idea, several sources), update this index.
4. Short form after a project update (a working session that lands a change in a watched area): steps 1-2 only, for
   the notes the tool lists. The working session may do it itself or leave a line in `IDEAS.md` for the next research
   session.
5. Record the round at the top of `IDEAS.md` ("Last check-up: <date>, HEAD <commit>, dirty: <paths or none>").

## Note header template

```
# <Topic>
Status: current | partly stale | superseded
Checked against: FLvibed <commit> (<date>); HDE 0.7.1 test install; HDE upstream <commit>
Dirty paths at check: <list or none>
Watch: <paths or globs whose change should trigger a re-check>
Evidence: research\evidence\<topic>\ (<what>)
Sources: <where the findings come from; provenance tags [file] [page] [api] [assumed]>

## What it means for FLvibed
<the living part: a few paragraphs, each pointing at IDEAS.md ids>

## Findings
<the full material, dated, never rewritten to hide what was learned>
```
