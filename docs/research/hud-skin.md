# HUD 2 as a complete skin: total conversions, map screens, menus and dialogs, Freelancer's own mods
Status: current
Checked against: FLvibed 3c9c1390 (6 Nov 2026, short form: 7 watched-path commits: b6f3be60 and 0fdfdc9f the design, 8d8243b3 and 5eac39f7 the probes in the design, 3c9c1390 the rulings and build order, 10998491 and 8292a7c5 fixes on the hidden HUD; U42-U46 stay adopted, nothing of the skin built); HDE 0.7.1 test install; HDE upstream as in `research\repos\`
Dirty paths at check: another session's uncommitted the private work queue, `tools/uimock/options.py` (not judged); 950b25cc, 4a270d9e and 3c9c1390 landed during the round, read
Watch: modules/hud/src/uiskin.cpp, modules/hud/src/hudvis.cpp, core/ui/winplates.cpp, modules/hud/docs/UI_HUD2_SKIN.md, modules/controller/src/impl/padmap.h
Evidence: research\evidence\hud2_skin\ (01-04 reader + verifier notes per topic; 05 front-end facts, 06 the native flash, 07 nav map facts; `mock\` the frames)
Sources: four readers and four adversarial verifiers, 5 Nov 2026, each with its URL list in the evidence; Ghidra over the HDE 0.7.1 exe; provenance tags as in each file: [F] / [page] read, [S] / [snippet] excerpt only, [memory] unconfirmed, [calc], [judgement]

## What it means for FLvibed
*6 Nov 2026 (3c9c1390, short form):* still a design: the probes' results and the owner's rulings are in `UI_HUD2_SKIN.md`, no overhaul code (HANDOFF). Two fixes show the cost of a hidden native root, the risk behind U46: USER_NEXT / PREV_TARGET found nothing because the contact list builds in its draw (10998491), and the throttle keys stopped with the maneuver bar hidden (8292a7c5). U47: INFO, not HELP.

*5 Nov 2026, P11f (c64b2ad0):* the design is `modules\hud\docs\UI_HUD2_SKIN.md`; this note is why.

1. **Legibility and density outweigh seams.** The first reading, "players name seams first", did not survive: over EVE's whole Photon thread a rough keyword count by author gave density 66, transparency 35, readability 34, moved or hidden 22, seams 11 of 352. New glass under existing controls has a public failure, Apple's Liquid Glass (NN/g). So: plates near opaque, 4.5:1 measured against the worst background, no padding that costs a row (U42).
2. **The method is right and has no Freelancer precedent.** No mod restyled a native screen in place: art swap, coordinate patches, plugin controls and overlays are what the community has. FLUF's map is the nearest: it removes the native draw at 0x4926D0 and leaves the window alive, a replacement and not a skin. ElvUI is the nearest model elsewhere, itself a hybrid (U43).
3. **Coverage is a checked claim or nothing.** Mods mostly warn; `uiskin seams` makes every shown unclaimed control a named failure (U43).
4. **The nav map can grow in place.** Decompiled, unproven: the frame comes off, two fields enlarge the map, clicks follow (U44). The pad: free cursor on the canvas, focus in the panels, a non-cursor route to every target (Microsoft: "enable mouse mode" where XY focus is not practical; all functionality by D-pad alone).
5. **Dialogs.** Safe focus on risky ones, B the safe answer, verbs with a speed bump, no hold to confirm a destructive action (XAG 115), which FLvibed's one-second save delete breaks (U45).
6. **The flash** at screen loads is in the code: `06_native_flash.md`, UI_HUD2_SKIN 3.1 (U46).
7. **The pad cannot reach log, inventory, status or help today.** One strip of words on every NeuroNet window closes it (U47; `10_neuronet_routes.md`).
8. **The weapon group cue:** MW4's boxes stand beside the crosshair and vanilla MW5 has no such cue; the written rule is BattleTech: Firestorm's, per group in range (U48; `08_mechwarrior_cue_reader.md`, `modules\hud\docs\UI_HUD2_GROUP_CUE.md`). No verifier ran on this reader; its frames were read by the session.

## Findings
### Total conversions (`01_total_conversion_reader.md`, `_verify.md`: 3 of 13 held as written, 2 wrong, 8 weakened)
- **Held:** ElvUI's shape: 118 skin files on 56 shared functions, a master switch and 41+ toggles, each loaded under `xpcall`. Limits: 15 of the 56 are WoW widget skins, the files serve seven game flavours, `xpcall` guards the load only and there is no rollback.
- **Wrong:** "seams first" (above); "instant toggles are renderer swaps": RuneLite and ScummVM swap without a restart and Warcraft III's swap is not instant. **Reversibility decides:** remember every write, restore on off, restore only your own value. `uiskin.cpp` does all three, stricter than any source read.
- **Weakened:** only ElvUI truly strips art, and it is a hybrid; complaints also name readability and transparency; OpenRA's loading screen lies outside its chrome; D2R's "UI stays modern" unproven; SkyUI only warns; a Collections freeze blamed on a skin was DX12.
- "Complete" never meant everything: WoW's login is out of reach, Nordic UI needs patches.

### Map screens (`02_map_screens_reader.md`, `_verify.md`: 7 of 15 held, 7 weakened, 1 wrong)
- **Held:** hybrid pad model (cite Microsoft, not the blog rehost); triggers zoom, bumpers change view or tab (Starfield, Stellaris); Valve's 9 / 12 px; colour never alone; declutter by priority (web cartography, no game source).
- **Weakened:** friction over snap rests on Destiny and CommonUI's `HoverSlowdownFactor`, the No Man's Sky "counter-case" was a centre-screen snap; only No Man's Sky shows category cycling on a pad; X4 and EVE act through context menus, not selection cards; EVE keeps a toolbar on the map and Odyssey's legend is called too nested; Material 1 is retired and its 375 ms is mobile.
- **Wrong:** "none found changes the in-game window": FLUF core replaces the nav map, and JFLP and FLSharp touch its content and waypoints.
- **Gap:** no published map-area share, panel width or transition duration for any game; such numbers need frames.

### Menus, dialogs, loading, handhelds (`03_menus_dialogs_reader.md`, `_verify.md`: 16 of 25 held, 8 weakened, 1 wrong)
- **Held:** XAG 101 floor 26 px at 1080p (28 is Amazon's); Valve 9 / 12 px at 30 cm; Xbox Handheld 9 / 12 px at 720p, 14 / 18 at 1080p; B = safe answer; verb labels with Windows' speed bump; XR-001's limits; tips only on long loads (Apple); M3's motion tokens unmaintained.
- **Wrong:** one scrim number: published 30-60 %. HUD 2's 70 % is the owner's own, above the range.
- **Weakened:** NN/g says no looped animation under 1 s, not "no loading screen"; reduced motion is fades for Apple, removal for Windows and NN/g; the game's cursor is a width-scaled software mesh, so D3D9's limits and the DPI table do not decide it; "a pad must have focus" is false in general; red for destructive is disputed (Apple's two pages differ).
- **Missed by the reader:** XAG 115, the direct rule on holds and on save overwrite; XAG 112 on menus; GAG's quick start for continue-first.
- **Gap:** death screens, save rows and server browsers have thin primary guidance.

### Freelancer's mods (`04_freelancer_mods_reader.md`, `_verify.md`: 7 of 13 held, 6 weakened; four further claims wrong; the decomp's addresses matched Ghidra)
- **Held:** layout is data for few screens (navbar.ini, PAGE files, hud.ini); HDE has no two skins: Dark is an install-time rename of about 78 art files, flat icons one radio choice, the map background a third checkbox; the main menu's background is a real base and the logo is drawn outside the control tree, before `Win3DDrawAll` (0x59E300 = UiRender); `intro.ini` is nearly dead; HDE's installer can rename the font family and rewrite `fixed_height`; FLUF refuses HDE's plugins, so FLUF's screens meet vanilla-based installs; the decomp's 65 WINFACTORY names and `IntroFocus`'s 19 states are the best public screen list.
- **Weakened:** "three means only": six or seven, and FLUF suppresses a native draw in place; loading: the bar and text are real controls drawn by hand, so a hook on their slots or the bracket 0x59E500 / 0x59E5A0 is tighter than Present; the map's frame mesh is not drawn first: 0x4926D0 hand-draws side controls, briefing and objectives from a private list, and hiding the frame is untested; `ui_overhaul`'s trade finalising exists in code; FLAR's effect on the 2D screens is unverifiable.
- **Wrong:** `fluf.md` line 192 does list "Select Character"; FLUF's back ends include D3D11; the menu buttons' art loads from `NeuroNet\Inventory`, not `OBJECTS_darkhud`. RmlUi: none at clone 627ea2c, its history is public (1daa51b).

## Gaps
Everything game-side is decompiled and unproven until the probes of UI_HUD2_SKIN 12 run. Frames of other games' screens were not studied: map proportions and transitions in the design are judgement. Discovery and ModDB pages refused fetches. No source on crash boxes under a skin.
