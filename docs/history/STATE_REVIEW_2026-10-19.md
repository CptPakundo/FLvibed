# FLvibed state review — 19 Oct 2026

Written after the audit + regression sweep (all sections pass). Goals as the user set them; status per goal, then
directions.

## Big goals

| Goal | State | Verdict |
|---|---|---|
| One DLL, fewest prerequisites, each module alone or together | Release build = one `FLvibed.dll`, static CRT, module switches; release section of the sweep passes | **Met** |
| Robust across Freelancer.exe variants | 100 exe symbols verified by signature, fail closed per feature; HDE 0.7.1 tested, current HDE repo checked on paper only | **Mostly met** (upstream HDE never run) |
| **Controller: play the whole game with a pad, controller-first UI** (the module's *primary* goal) | Flight, combat (gimbal aim), menus/dialogs by D-pad, native room interaction, native target selection all work. The pad UI itself (`TASK_GLYPHS_HUD.md`: 7 phases) is **design only**; 5 of 7 open design questions unanswered | **Behind** — most effort went to the autopilot/harness |
| Subtitles: all spoken dialogue, standards-conform, movies fitted | Display stage works (comms, chatter, bar, Neural Net, intros, movie fit); review paused at 8 lines; Next: chatter timing, bar patron names, aspect ratios, settings page | **Close** |
| Advanced Autopilot (predictive, zero-input, product toggle) | Travel (lanes, gates, fast docks, avoidance), combat (targeting w/ line of sight, missiles, CMs, repairs, evasion, loot), hold/scenic rest, escort/formation. M01a, M01b, M02 pass end to end; 11 of 14 missions have no script yet | **Strong progress**, product UX (`AUTOPILOT_UX.md`) not built |
| Test harness that never stalls | Event-driven, background, plan B + stall layer, scene saves, sweep, screenshots + video + live-log cross-check | **Strong** |
| Publishing | No README / license / onboarding; code license undecided; data license decided (GPL-3.0) | **Not started** (by design: user decides) |

## Small goals / standing items

- Borderless windowed is the test mode; 4:3 / 21:9 / fullscreen / other DPI untested (both modules).
- Real Elite pad: flight played by the user; menus/bases only with the virtual pad.
- Torpedoes, mines, cruise disruptors as player weapons: untested.
- Turret view (gunner mode, cinematic idling): idea only.
- NPC names show `%s0 %s1` templates in contacts.
- Harness `Start-FLJump` flies the slow native goto (navigation section 669 s).

## What this week taught (process)

Most fix/retest rounds came from knowledge that existed but was not consulted (death zones in the system files,
mission set pieces in the scripts, native behaviour not measured) or from deciding on one source (frames without the
log, the autopilot without the mission's marks). Answered by: the game knowledge base (approved), the pre-change
checklist, the cross-referencing guideline, check-ins with sheets + live log.

## Directions (recommended order)

1. **Game knowledge base + pre-change checklist** (approved; starts now). Cheap per use, saves the most rounds.
2. **Controller-first UI, phase 1** (input-device service + renderer spike) after the user answers open questions
   1, 2, 4, 5, 6 in `TASK_GLYPHS_HUD.md`. This is the primary goal and has no code yet.
3. **Autopilot target model**: mission-marked targets first (capital ships, solar-type targets, torpedo runs), then
   ships, then opportunistic platforms — before scripting M03+.
4. **Mission coverage**: M03 onward with scene saves, using the knowledge base's mission pages; one per session.
5. **Subtitles**: finish the 8 review lines, then the display Next list (chatter timing, patron names, aspect
   ratios, settings page with preview).
6. **Display matrix**: 4:3 / 21:9 / fullscreen / DPI pass for both modules (the main mode is covered).
7. **Publishing prep** when the user says: README, licenses, onboarding.

Decisions waiting on the user: UI open questions (above), gunner mode / turret view, the "advanced" autopilot preset,
fast vs native docks as default, loadout symmetry.
