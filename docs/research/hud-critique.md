# HUD 2 overhaul: the critique round (Freelancer's way, modern AAA, FLvibed's flavour)
Status: current
Checked against: FLvibed 3c9c1390 (6 Nov 2026, short form: 1 watched-path commit, landed during the round: 3c9c1390, the owner's rulings on D1, D2, D4-D7 (UI_HUD2_CRITIQUE.md 7); U47, U48, U50, U52 updated; no overhaul code yet); HDE 0.7.1 test install; the gate's frames (build 559e28dc plus probe verbs)
Dirty paths at check: another session's uncommitted the private work queue, `tools/uimock/options.py` (not judged); 950b25cc, 4a270d9e and 3c9c1390 landed during the round, read
Watch: modules/hud/docs/UI_HUD2_SKIN.md, modules/hud/docs/UI_HUD2_GROUP_CUE.md, modules/hud/docs/UI_HUD2_SIZE_AUDIT.md, modules/hud/src/uiskin.cpp, modules/hud/src/hudvis.cpp, modules/hud/src/windowtext.cpp, modules/hud/src/hud2_parts.cpp, modules/controller/src/impl/padmap.h
Evidence: research\evidence\hud2_critique\ (`lens_*.md`, `attack_*.md`, `verify_*.md`, scripts, crops)
Sources: three lens readers, three attackers, two verifiers, 5 Nov 2026; each lens file ends with its URL list (26, 35 and 13 pages)

## What it means for FLvibed
*6 Nov 2026 (3c9c1390):* the owner ruled on the comparison set: the game's logo kept, the new cursor set, NEURAL NET with INFO not HELP, the weapon cue above the ring with glow, refire bar and tick, the amber nav line for dangers with no cure (U52), the floor 0.024 H on Normal and 0.021 H only on Small. Design only: no overhaul code until the owner's go (HANDOFF).

*5 Nov 2026, P11f gate (39c6609c):* the report is `modules\hud\docs\UI_HUD2_CRITIQUE.md`; this note only points to it and feeds the backlog (U49-U56).

**The balance: Freelancer decides what the player recognises and does; AAA decides how text, menus and input behave; FLvibed's own grammar decides how it looks.**

1. Behaviours are what Freelancer's players praise, not the chrome: no praise found for the blue-glass frames or the type; mouse flight in 8 of 8 reviews read. Thin on today's players (Reddit, Discovery, Metacritic refused).
2. Inner menus are where players' anger lives (Starfield): the base screens are the overhaul's largest gap, by stroke, not contrast (U49).
3. An alert that names its cure was found in no other space game (absence from search, not proof): keep it, and give it to cards only (U52).
4. FLvibed's flavour is grammar and behaviour; the colour is the genre's trope (U56).

## Findings
All in the report: section 2 the balance by area, 3 the flavour, 4 the ranked recommendations, 6 what held, 7 the owner's decisions, 10 what did not survive verification.

## Gaps
No motion seen; no real lock, capital bracket under HDE, Deck or Proton run; 21:9, 4:3 and Focus unread. Elite and Star Citizen rest on excerpts.
