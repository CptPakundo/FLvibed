# FLUF (Freelancer Universal Framework, The Starport): status, what it patches, FLvibed co-residency
Status: current
Checked against: FLvibed 77d8655 (26 Oct 2026, coordinator short form before the export: the watched commits since - d89f88c / 79e2bcc smart start + K&M goal panel, 7632ced job objective text, 77d8655 objective-named loot shot, 9ff7fe7 handoff - are autopilot / UI internals; no statement of this note changes; not re-read in depth); before: FLvibed 01fcfa9 (26 Oct 2026, the step 3 short form re-confirmed: the only watched commits since, 9327f86 / 01fcfa9, are doc and handoff edits recording what this note already says); before: FLvibed e5c02cc (26 Oct 2026, step 3 short form: 13 watched-path commits 2394c05..e5c02cc - step 2 3582c29 / a6aeac4 / af2adb1 / c093e31 / 524baed, the dealer close f52815b, the pad on FLUF windows and popups 0e41948, the Reset fixes 9c7e5c0 / e145662, press ownership 0eca73d, the classic bar a3cb46c, borderless 90a8b7f / 38d411c; the A2 work-list bullet and the "Fixes the source asks for" list marked fixed in place, E Status gained the after-A3 bullet; 202 / 202 signatures on the four exes, `research\evidence\discovery\sigcheck_e5c02cc.txt`); before: FLvibed 2394c05 (26 Oct 2026, Phase A3: the nightly run in game, both orders - "FLUF in play" header, E Status, F "found by A3"); before: FLvibed 2f69229 (26 Oct 2026, Phase A1 review: the draft below spot-checked against the source and the package - the palette / options / status / debug window rules, the key registrations, the escape and mouse rules, the install - corrected where it was wrong; FLUF's issue tracker read through the Codeberg API: 35 open issues, 97 closed (the 50 most recently updated read), no open pull requests -> "FLUF in play" F); before: FLvibed 176c263 (26 Oct 2026, Phase A1 draft: the section "FLUF in play" for the target build FLUF nightly-2026-10-16-03-03 = upstream 627ea2c, FLSDK ae8c857, imcp d1afcd6, read from the source and the owner-approved package `Install\downloads\fluf_nightly_2026-10-16\` (docs, binaries as data, the shipped PDB); no game run; the 2.0.0 run became history); before: FLvibed 855745b (25 Oct 2026 ~23:05, whole-set check-up: the 3 watched-path commits 6970dd8 / da1363f / 5cd02b9 read against `core\hook.cpp`, `core\flexe.h` and `MOD_COMPATIBILITY.md`; the section 8 / C7 / C8 / M9 / M3 updates of 5cd02b9 kept; three statements written before da1363f marked in place - the `UiControlRemove` +9 refusal, the evade guard, the cursor (section 4, "Smaller collisions"); section 7's download OK recorded); before: 35b71fd (25 Oct 2026 ~23:00), re-checked for the co-residency build da1363f (section 8); FLUF upstream 627ea2c (= the clone `research\repos\FLUF`, nightly-2026-10-16-03-03; still HEAD on Codeberg 25 Oct 2026 ~23:00); FLSDK ae8c857 (`Detour.hpp` last changed 24 Aug 2026, 5522463)
Dirty paths at check: docs/research/* (this change only; 26 Oct 2026 step 3); before: docs/research/fluf.md, docs/MOD_COMPATIBILITY.md, README.md (this change only)
Watch: core/fluf.cpp, core/fluf.h, core/hud_bus.h, controller/src/impl/flufaware.cpp, core/hook.cpp, core/hook.h, core/flexe.h, controller/src/host/main.cpp, controller/src/host/uibtn.cpp, controller/src/host/events.cpp, core/game/route.cpp, controller/src/impl/padmap.cpp, subtitles/src/host/main.cpp, docs/MOD_COMPATIBILITY.md, tests/fluf_hook_test.cpp
Evidence: research\evidence\fluf_nightly\ (the nightly: `NOTES.txt` - identity, imports / exports of FLUF.dll, FLUF_boot.dll, dacom_fix.dll and 2.0.0's FLUF.dll, the PDB publics, handler entry bytes, vanilla bytes at FLUF's patch sites, the 2.0.0 run-log lines the nightly source explains; `overlap_176c263.txt`; the issue tracker of 26 Oct 2026: `issues_open.json` (all 35), `issues_closed.json` (the 50 most recently updated of 97), `pulls_open.json` (none), `issue93_comments.json`, `issue106_comments.json`, `issue108_comments.json`, `issue111_comments.json`, `commits_head_2026-10-26.json` - upstream HEAD unchanged); research\evidence\fluf\ (Codeberg API answers repo / commits / releases / tags of 25 Oct 2026; `overlap.py` + `overlap_35b71fd.txt`: every Freelancer.exe address in FLUF's sources against every `flexe.h` signature span)
Sources: [api] Codeberg / GitHub APIs; [file] the FLUF and FLSDK sources at the commits above (`FLUF\Source\Fluf.cpp`, `Client\Ui\UiManager.cpp`, `Client\Ui\Vanilla\HudManager.cpp`, `Client\KeyManager.cpp`, `Client\FlufClient.cpp`, `Client\SoundApi.cpp`, `Client\ClientPatches.cpp`, `Client\Fixes\*`, `Include\Common\Internal\AI\Behaviors\EvadeBehaviorHooks.hpp`, `FLUFBoot\`, `DacomFix\`, `docs\content\user\*.mdx`, `LICENSE.md`, FLSDK `include\Utils\Detour.hpp`; imcp `src\command_palette.cpp`); FLUF's issue tracker https://codeberg.org/TheStarport/FLUF/issues [api]; FLvibed `core\hook.cpp`, `core\flexe.h`, the hosts. No download besides the API answers, no game run.

## What it means for FLvibed

- **FLUF is alive; JFLP and FLHook are not where development happens.** FLUF: 1,325 commits, nightly builds every few
  days (last 16 Oct 2026), stable 2.0.0 (28 Mar 2026), three active developers. TheStarport's FLHook is archived (16 Jul
  2026, "FLUF is intended to be a replacement"); JFLP (adoxa: JFLP 1.25, jflp.dll 1.27) is a finished patch whose
  site was last updated 25 Apr 2026 (no repository or commit history to measure). FLUF is
  the framework new client mods will be built on, so it is the one co-residency that matters going forward (IDEAS M3).
- **FLUF cannot run on HD Edition.** It refuses to load beside `jflp`, `flsharp`, `delaydisplay`, `mp3codec`,
  `storyfactions`, `callsign` (all loaded by HDE 0.7.1) and six other adoxa plugins (a message box per DLL; the load is
  blocked). FLvibed + FLUF is therefore a **vanilla 1.1 (decrypted exe) or FLUF-based-mod** combination only, never
  HDE + FLUF.
- **The target build is FLUF nightly-2026-10-16-03-03 (owner, 25 Oct 2026: "the very latest tagged version").** What
  it changes for players, for FLvibed and for our tests is the section "FLUF in play" below - from source / docs,
  **run in game in Phase A3 (26 Oct 2026): FLvibed works beside it in both load orders** (two FLvibed fixes, 3b4bb4a /
  2394c05; `research\experience\runs\fluf_nightly_2026-10-25\TABLE.md`), but the nightly itself does not start on the
  game's own data (F, "found by A3"). The 2.0.0 run (history, section 9) is no verdict for the nightly.
- **FLvibed's co-residency code (da1363f / 5cd02b9 / e2611cd, 25 Oct 2026) is an unverified draft** (the owner, 26 Oct
  2026: "assume nothing concrete related to FLUF happened"): its design hooks FLUF's handlers, not over FLUF's jump
  (section 8). For the nightly the six shared sites are plain restore-style `FunctionDetour`s whose handler starts
  that decoder can move ("FLUF in play" C, from the shipped bytes); A2 / A3 keep it only if the nightly run proves it.
  *A2 review (26 Oct 2026, `research\experience\runs\fluf_nightly_2026-10-25\A2_REVIEW.md`): every code piece kept (the
  2.0.0 HandleKey form as 2.0.0-only), unit test 8 on the nightly's handler bytes; the claims of "verified" became
  history / pending. The awareness layer on top of it: "FLUF in play" E, Status. In game: A3.*
- **Known FLUF issues ("FLUF in play" F):** nothing open in FLUF's tracker blocks a single-player run of the nightly;
  open items a test can meet are FLUF's (Thorn scenes zoomed at high resolutions, the options window's resolution
  change crashing - #111) and are recorded, not counted against FLvibed. FLUF is about to replace the Game Settings
  screen (#131) and the character select / new character screens (#73) with ImGui, may stop writing `flspew.txt`
  (#45) and is reshuffling its internals (#141): FLvibed must not depend on those vanilla screens' layout under FLUF,
  on FLSpew alone, or on FLUF's mangled export names alone.
- **New from the nightly source (A2's work list):** FLUF core now replaces the nav map and the mission briefing's map
  with its own ImGui window (FLvibed's pad map screen and the hub's map key then meet FLUF's map); it rewrites every
  UI control class's message slot (FLvibed's native-button setters refuse: the pause-menu caption, the bar button's
  placement, HUD 2's click boxes); it takes six IServerImpl slots (FLvibed's economy hooks: 4 of 10) and slots of
  ten Common.dll vtables (FLvibed's virtual lookup stops early); it patches the instruction FLvibed's lane-law
  signature starts on; it rescales `fonts.ini` from the primary monitor's height. All fail closed; three of them
  already show in the 2.0.0 run's logs ("FLUF in play" C). *[Fixed 26 Oct 2026, step 2 (3582c29, verified in both
  orders): the setters, the economy hooks (10 of 10), the virtual lookups and the lane law now read the game module's
  original image under FLUF's patches; FLUF's map is driven by the pad rule ("FLUF in play" E, Status).]*
- **After A3 (26 Oct 2026, checked at e5c02cc):** the pad drives every input-taking FLUF window and the game's popups
  (0e41948), a B / A that closed a FLUF window no longer reaches the game (0eca73d), the classic maneuver bar no longer
  creeps under FLUF (a3cb46c); FLUF's Alt+Enter on vanilla 1.1 = a D3D8 Reset the game never retries (DEVICELOST, a
  freeze: recorded conflict, `docs\MOD_COMPATIBILITY.md` FLUF "Alt+Enter"), and FLvibed's own borderless (90a8b7f)
  turns FLUF's toggle into borderless <-> bordered windowed. Open owner checks (attended): order B's palette run, FLUF
  alone with Alt+Enter, the tutorial boxes on the pad.
- *History - the bullets below are the 25 Oct 2026 analysis from the HEAD source before da1363f (the 2.0.0 era); kept
  for the reasoning, not a verdict for the nightly:*
- **Compatibility verdict (from source, not tested in game; before da1363f): works only if FLvibed loads before FLUF.** FLUF hooks six
  of FLvibed's detour sites (`FrameUpdate`, `UiRender`, `CursorUpdate`, `WinKey`, `HandleKey`, `PlayArch`) with FLSDK's
  `FunctionDetour`, which **removes its own jump, calls the original, and writes the jump back on every call**.
  - FLvibed first (FLvibed's `FLvibed.dll` above `fluf/FLUF.dll` in `[Libraries]`; FLUF's docs put FLUF last): FLUF
    saves FLvibed's jump as "the original bytes" and restores it for each call - both run, FLUF outermost. Chains.
  - FLUF first: FLvibed chains after FLUF's jump (`hk::Detour` follows the E9), but FLUF's first call restores the
    vanilla bytes over FLvibed's jump and re-writes only its own: **FLvibed's hooks at those six sites run once and
    are then gone, silently** - the whole controller and subtitles logic (both run from `FrameUpdate`), the drawing,
    keys and voice lines. Not fail-closed; the log shows only "chaining existing hook". (IDEAS C7, M9)
- **Smaller collisions, all fail closed or cosmetic:** FLUF patches a jump into `UiControlRemove` at 0x59DC09 (inside
  FLvibed's signature) - only if FLUF is first, Extended HUD 2's control tracking is refused; FLUF owns the Evade
  behaviour's update slot in Common.dll, so FLvibed's lazy evade guard refuses itself (logged, autopilot Evade stays
  off); FLUF NOPs the game's own cursor drawing (0x41F33E-0x41F6E9) and draws an ImGui cursor from Win32 input, which
  may not follow FLvibed's pad cursor (IDEAS C8). D3D8: FLUF detours `Direct3DCreate8` / `CreateDevice` / `Reset` code
  in d3d8.dll, FLvibed swaps vtable / IAT pointers - different layers, they chain either way.
  *[Overtaken by da1363f (25 Oct 2026; checked 855745b): `UiControlRemove`'s signature carries 5 wildcards at +9 (HDE's
  HudFacility.dll jumps there too), so FLUF's patch no longer refuses it in either order; the evade guard leaves a slot
  another module owns alone and says so once, the autopilot dodging with its own slide bursts (dodge 2's fallback); the
  pad cursor drives the game's cursor position FLUF's arrow is drawn at, and FLvibed's hidden cursor clears
  `exe::CurrentCursor` around FLUF's handler (C8 in code). The 2.0.0 run reported all three working (history, section
  9; unverified per the owner's 26 Oct note); for the nightly they are from source until Phase A3 runs them ("FLUF in
  play" D).]*
- **Corrections to the earlier notes** (discovery.md, starport.md, IDEAS M3, `MOD_COMPATIBILITY.md` before 25 Oct):
  "FLUF's detours are plain E9, chainable" holds only for FLUF on top of FLvibed, not the other way round; FLUF also
  detours `HandleKey` 0x576410 (previously named as DSAce's only) and `FrameUpdate` 0x5B2890; 0x59DB20 (FLUF's
  `CreateControl`) is not FLvibed's `UiControlAdd` (0x59DBF0).

## FLUF in play: nightly-2026-10-16-03-03 (the target build)
**Written from source / docs / static bytes; Phase A3 (26 Oct 2026) ran it in game** - both `[Libraries]` orders and the
boot-loader form on vanilla 1.1 against the same game without FLUF, release build with the A3 fixes 3b4bb4a / 2394c05:
`research\experience\runs\fluf_nightly_2026-10-25\TABLE.md` (per item and order; summary in `docs\MOD_COMPATIBILITY.md`
FLUF). Confirmed in game: B's windows, names and rectangles (palette 0.35-0.65 W x 400 px, options 0.933 H x 0.7 H at
the top left, the map with its two side windows, the Debug Window at (60, 60)), C's handler hooks in both orders, the
Evade slot owned by FLUF, the expected fail-closed items of E. New from A3: FLUF does not start on the game's own data
(F, "found by A3"); its first run sets the resolution to the screen size; the chase camera follows more loosely (its
camera fixes); in an unfocused (background) game its palette never gets keyboard focus. Build: tag nightly-2026-10-16-03-03 =
FLUF 627ea2c (15 Oct 2026; the clone `research\repos\FLUF`), FLSDK ae8c857 and imcp (the command palette library)
d1afcd6 = the submodule pins (`research\repos\FLSDK`, `research\repos\imcp`); package `FLUF.tar.xz` SHA-256 74a8147d...
(`Install\downloads\fluf_nightly_2026-10-16\`, the owner's download; AGPL-3.0: nothing of it enters the repository).
Paths below are FLUF source paths (`FLUF\Source\...` unless named otherwise) with line numbers at 627ea2c; the raw
evidence is `research\evidence\fluf_nightly\NOTES.txt`.

### A. Install and flow (what a player gets)
- **Package:** `EXE\fluf\` = `FLUF.dll` (421.7 MB: DWARF debug sections that the loader maps - about 424 MB of the
  32-bit address space, `SizeOfImage` 0x1944D000), `FLUF_boot.dll`, `dacom_fix.dll`, `freetype.dll`, `libcurl.dll`,
  `modules\*.dll` (28), migrations, json/png; `DATA\FONTS\SairaCondensed*.otf`, `DATA\SCRIPTS\FLUF`,
  `DATA\INTERFACE\FLIGHT_MANUAL`; `development\` (headers incl. Dear ImGui 1.92.7 WIP docking, libs, **PDBs**). No
  `FLUF.yml` is shipped. `FLUF.dll` imports `freetype.dll` (beside it in `fluf\`), `D3DCOMPILER_47.dll` (System32)
  and the VC++ x86 runtime; **no `FreelancerSDK.dll` / `libcurl.dll` any more** (2.0.0's helper-DLL copies beside
  Freelancer.exe are not needed).
- **dacom.ini (docs `getting-started.mdx`):** `[DACOM]` `dll_search_path = .\fluf` and `.\fluf\modules`;
  `[Libraries]` first `fluf/dacom_fix.dll` (the docs' text; their ini example writes `dacom_fix.dll`, which does not
  match the package's folder), last `fluf/FLUF.dll`. `dacom_fix.dll` (`DacomFix\DacomFix.cpp` 14-62) patches
  `dacom.dll+0x2683`'s `LoadLibraryA` into `LoadLibraryExA(..., LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | USER_DIRS)` and
  calls `SetDefaultDllDirectories` + `AddDllDirectory` for every `dll_search_path` / `dll_path`: process-wide, later
  bare-name loads search the application folder, System32 and those paths (FLvibed's xinput / dwmapi loads are System32
  DLLs: unaffected).
- **`FLUF_boot.dll` (not in the user docs; `FLUFBoot\FLUFBoot.cpp` 103-162):** an alternative `[Libraries]` entry
  *instead of* `fluf/FLUF.dll`. It exits the game with a message box unless `dacom_fix.dll` is already loaded, loads
  `.\fluf\FLUF.dll` under an SEH guard (a crash in FLUF's DllMain writes `%TEMP%\fl_error_<time>` and offers to open
  it), and exits with "FLUF MISSING VC RUNTIME" / "FLUF MISSING DEPENDENCIES" boxes when the load fails. No exports,
  no hooks of its own. So the module list tells the install form: `FLUF_boot.dll` present = boot loader.
- **All hooks go in at `FLUF.dll`'s DllMain** (`Fluf.cpp` 295-318 -> the `Fluf` constructor: `ClientPatches()` 761,
  the `FrameUpdate` detour 771-773, the Common.dll vtable hooks 826-834): the `[Libraries]` order still decides who
  hooks first.
- **First run, modal Win32 dialogs (#32770):** "No ship classes" (`Client\Fixes\ShipClasses.cpp` 78-86; **Cancel
  calls `std::abort()`**, OK writes `fluf\config\ship_classes.yml`); "No Fonts Loaded" if `clientConfig.loadedFonts`
  is empty (`ImGuiInterface.cpp` 869-875); "DLL file <x> is not allowed to load alongside FLUF" for `jflp`, `flsharp`,
  `delaydisplay`, `lanespeed`, `mp3codec`, `fixedmounts`, `healing`, `storyfactions`, `nonavmap`, `equipmass`,
  `ammolimit`, `callsign` (`Fluf.cpp` 55-57: still no HDE); "Key Already Registered!" + exit when two modules claim
  one command (`KeyManager.cpp` 142-154). FLUF writes `fluf\FLUF.yml` (defaults: `ClientConfig.hpp` 22-46,
  `FlufConfiguration.hpp` 16-60) and creates `fluf\config` (DllMain fails without write access).
- **Every start:** `logSinks` default `[File, Console]` -> a console window "FLUF - Freelancer Unified Framework"
  (`Logging.cpp` 195-196) that takes the foreground; the log `fluf.log` in the game's user-data folder
  (`GetUserDataPath`, `Logging.cpp` 219-222); `writeSpewToLogSinks: true` swaps dacom's `FDUMP` pointer, so FLSpew
  lines gain a `[timestamp] ` prefix (231). ImGui keeps `imgui.ini` (window positions) in the same user-data folder
  (`ImGuiInterface.cpp` 765-783). Every `.ttf/.otf` under `DATA\FONTS` is added process-private (`ClientPatches.cpp`
  127-141).
- **Always-on core behaviour that changes play** (`ClientPatches.cpp` 143-178, `core-features.mdx`): Screen Control
  (`Client\Fixes\ScreenControl.cpp`: with `-w` the main loop treats the app as always active - it keeps simulating
  unfocused; Alt+Enter re-routed to FLUF's fullscreen / windowed toggle), the cursor scale (`ScaleCursor`, below),
  camera acceleration x3 (`[FlufConstants] CAMERA_ACCELERATION_MODIFIER`), hit-ray explosions, wheel scrolling in
  every vanilla list and a ship-dealer scroll bar (`WheelScroll.cpp`), subtarget cycling, max graphics settings on
  the first launch, **unencrypted character saves** (`MiscFixes.cpp` 121-131, server.dll), per-lane tradelane speed
  (`AllowAdjustableTradelaneSpeed`), multicruise, the version mark only if `versionOverride` is set (`Fluf.cpp`
  600-625; 2.0.0 marked it always), **`fonts.ini` rescaled on load from the primary monitor's height, not the game
  window's** (`WidescreenFontScaling.cpp` 74-127: above 1400 px, every size divided by 1 + 0.25 per full 620 px above 1400 -
  / 1.25 at 2160; the font names fall back to an installed font when the named one is missing - the fix
  for issue #93, HUD gauges missing at 4K; on the 2160-px dev monitor every instance gets the scaled fonts, whatever
  its window size), Print Screen writes `png` by default (`screenShotType`, `PrintScreenAltFormats.cpp`; issue #58 -
  FLvibed's harness captures with `PrintWindow` / its own `grab` BMPs: unaffected). Default client module:
  `debugger.dll` only.

### B. Every FLUF UI element (default install: core + `debugger.dll`)
ImGui is drawn in three layers (`UiManager.cpp` 133-156, `ImGuiInterface.cpp` 457-591): *Under* at `UiRender`
(NewFrame, modules' `BeforeUiRender`, under the game's UI), *Over* inside `CursorUpdate` before the game's cursor code
(modules' `OnUiRender`, the palette, the options / status / char-select / nav-map windows, toasts), *Cursor* after it
(FLUF's arrow). Window rules: `FlWindow` (`Client\Ui\ImGui\FlWindow.cpp` 289-363, `FlWindow.hpp` 16-31, 88-89) =
title bar off, `ImGuiCond_Appearing` position / size, escape-closeable by default, position (0,0) pivot (0,0) unless
centred; plain `ImGui::Begin` windows (debugger, palette) are not `FlWindow`s. "W/H" = the ImGui display size = the
game's back buffer.

| Element (ImGui window name) | When / default | Where (rect rule) | Keys / input | Rect + state source |
|---|---|---|---|---|
| FLUF cursor (no window) | always; the game's own cursor drawing is NOPed (0x41F33E-0x41F6E9, `UiManager.cpp` 338-340) | at ImGui's mouse = the game's `MouseX/Y` 0x616840/44; shape = the game's current cursor 0x616858, replaced by `arrow` / `stretch` / `no_fire` while ImGui wants the mouse (`ImGuiInterface.cpp` 352-455); scale from `ReadMouseShapes` | follows whatever moves the game's cursor (FLvibed's pad cursor included) | `exe::MouseX/Y`, `exe::CurrentCursor` (FLvibed's own symbols) |
| **Command palette** `fluf_cmd_palette` - the "search" element: a fuzzy-search text box with a command list | closed; toggled by FLUF_OPEN_COMMAND_PALETTE; SP and MP contexts | top centre: pos (W/2, 0), pivot (0.5, 0), size 0.3 W x 400 px fixed (imcp `src\command_palette.cpp` 448-475) | while open: text focus (`WantTextInput`) - every key goes to it, none to the game; first Esc clears the text, Esc on an empty box or losing ImGui focus closes it (465-506); opening hides `ManeuverFrame` **and every later sibling**, disables them and suppresses their message handler; closing shows them again (`Common\Commands\CommandHandler.cpp` 24-107, 335-361) | `ImGui::FindWindowByName("fluf_cmd_palette")` -> its `Active` / `Pos` / `Size`; or the source rule from the display size |
| **Extended options menu** (the "config menu"; ImGui name `Player Status` - shared with the status window) | closed; FLUF_OPEN_EXTENDED_OPTIONS_MENU toggles; tabs Display (resolution list from `EnumDisplaySettings` >= 1280 wide; picking one calls the game's display change 0x424CE0 = a device reset) and Controls (a placeholder that draws Display again); "chore: disable unfinished options menu" (6d7d166) only removed the main menu's Options redirect (`CustomVanillaHud.cpp` 43-52) | **top-left (0,0)** (no position set: `FlWindow`'s default (0,0), pivot (0,0)), size 4:3 at 70 % of H (0.933 H x 0.7 H), movable while open, re-placed at each opening (`ImGuiCond_Appearing`; `Client\Ui\CustomHud\CustomOptionsWindow.cpp` 258-285); a resolution pick shows a "Resetting in: n" countdown (106) | Esc (USER_CANCEL) closes it; mouse over it is FLUF's | `FlWindow::IsOpen` (export) on the object (offset not public); or `FindWindowByName("Player Status")` - **the same `ImGuiWindow` as the Player Status window** (both `FlWindow`s Begin the title "Player Status", no `##` id): the name cannot tell which is open, and with both open ImGui merges them into one window (the rect is the last Begin's) |
| **Player Status** (`Player Status`) - reputation / stats | closed; opened by the **NeuroNet Player Info button** `NNPlayerInfoButtonRoot`, whose vanilla action is suppressed (`CustomVanillaHud.cpp` 12-19); F8 (USER_PLAYER_STATS) still opens the vanilla screen (FLUF's binding is commented out, `PlayerStatusWindow.cpp` 26-32) | centred, 4:3 at 70 % of H, fixed (607-619) | Esc closes; a text filter box (`WantTextInput` while typing) | as above |
| **Nav map** `NavMap` + children `NavMapObjectiveWindow`, `NavMapDescriptionWindow` (new since 2.0.0) | **replaces the vanilla nav map**: USER_NAV_MAP toggles it and is reported handled - the vanilla map never opens (`NavMapWindow.cpp` 2782-2806); the NeuroNet nav-map button sends HandleKey(99) (`CustomVanillaHud.cpp` 21-28); the **mission briefing**'s map render 0x4926D0 is replaced without calling the original and opens FLUF's map, not Esc-closeable until the briefing ends (0x43D580) - the briefing's `NavMapContinueButton` still works (`NavMap.cpp` 789-848, `NavMap.hpp` 149-162); "focus object" 0x4A2AA0 also focuses FLUF's map | map: centred, 4:3 at 70 % of H, fixed (`NavMapWindow.cpp` 2774-2780); the two children: not read (their own `Render`) | Esc closes outside the briefing; the map's own mouse handling (canvas, context menu) | `FindWindowByName("NavMap")` (+ children) |
| MP character select `Select Character` / `Character Select` | multiplayer only | centred 4:3 at 70 % H | - | - |
| **Debug Window** `Debug Window` (module `debugger.dll`, on by default) | **open at every start, single player** (main menu included); `renderDebuggerMenu = true` (`modules\debugger\Include\Debugger.hpp` 36-42, `Source\Debugger.cpp` 111-120, 248-283) | no position set: ImGui's first-use default (60, 60), then wherever the player drags it (saved in `imgui.ini`); auto-sized; menu bar Tools (Target Info, Curve Gallery) / Demo Windows | its close box hides it until the next start; not Esc-closeable; mouse over it is FLUF's | `FindWindowByName("Debug Window")`; persisted rect in `imgui.ini` `[Window][Debug Window]` |
| Debug sub-windows | off; from the Debug Window's menus | `DEBUG: Target Info` (5 %, 5 %) 500 x 800, no background; `DEBUG: Curve Visualiser` (25 %, 25 %) 1200 x 1000; ImGui / ImAnim demos | - | names as listed |
| Debug quick load | palette command `quick_load` (SP); launch argument `--save-file <name>` presses main-menu buttons through 0x5743A0 (codes 3, 2, 0x12) and loads the save (`Debugger.cpp` 185-245) | - | - | - |
| Toasts `##TOAST<n>` | only on command errors / timeouts / server messages | **top-left**: x 2 % W, y 5 % H + the stack, up to 5, 80 % opacity, no inputs (`Include\Client\UI\ImGui\ImGuiNotify.hpp` 440-503; `RenderNotifications()` is called with its default `rightAligned = false`) | - | names |
| Corner windows | module API only (`ImGuiWindowManager.cpp` 18-57; none in core) | a corner, 10 px padding, stacked | - | - |
| Main-menu button | **none**: 2.0.0's "Open FLUF Config" button is gone (the string is absent from the nightly's binary) | - | - | - |
| Console window, message boxes | section A | Win32 windows of the game process | - | `EnumWindows` |

**Hotkeys and key remaps** (`Client\KeyManager.cpp` / `.hpp`; commands are the game's `USER_*` numbers):
- FLUF_OPEN_COMMAND_PALETTE = **USER_YES (19)**, FLUF_OPEN_EXTENDED_OPTIONS_MENU = **USER_NO (18)**: `RegisterKey`
  renames the command in the INI name table 0x614DD8 (`KeyManager.cpp` 164-166), so both are **unbound** until the
  player adds them to `keylist.ini` / `keymap.ini` (docs). The palette key returns "not handled" (the game also gets
  USER_YES); the options key is handled (`CommandHandler.cpp` 335-361, `UiManager.cpp` 213-217).
- **USER_CANCEL (15)**: closes the topmost escape-closeable `FlWindow` (options, Player Status, nav map outside a
  briefing) and is then eaten; with none open it reaches the game (`UiManager.cpp` 219-233).
- **USER_BUTTON0/1/2 (6/7/8) and USER_WHEEL (4)**: eaten while the game cursor lies inside an active ImGui window
  that draws a background, or any `FlWindow` (`UiManager.cpp` 237-275) - the Debug Window included.
- **USER_NAV_MAP (99)**: FLUF's map (above). USER_PLAYER_STATS: untouched.
- Raw keys: FLUF's outer `WinKey` handler feeds every message to ImGui and, **while `io.WantTextInput`**, returns
  without calling the game (`UiManager.cpp` 115-131). Ctrl+= / - were meant to scale ImGui; the compare uses a signed
  `char`, so it never fires (`ImGuiInterface.cpp` 648-669).
- Alt+Enter: FLUF's windowed / fullscreen toggle (ScreenControl). Optional modules add FLUF_TOGGLE_AUTO_TURRETS,
  FLUF_FLIGHT_MANUAL_OPEN, FLUF_TOGGLE_RAID_UI, FLUF_ABILITIES_ACTIVATE / _DEACTIVATE (not loaded by default).
- New since 2.0.0: `KeyManager::SetKeyBlocked` (any module may block any command; `SimulateKeyPress` bypasses it),
  so with modules loaded a FLvibed command can be dropped silently by FLUF.

**Input, focus, cursor:** ImGui's mouse comes from the game's state, not Win32 (`PollInput`, `ImGuiInterface.cpp`
606-646): position always, buttons and wheel only while ImGui wants the mouse, and then FLUF zeroes the game's wheel
accumulator 0x616848 (= `exe::MouseWheel`). So FLvibed's pad cursor drives FLUF's windows like a mouse. The game
window's Win32 focus is untouched (except the console at start). ImGui focus is internal and sticky: clicks outside
FLUF's windows never reach ImGui, so its last focused window stays "focused" - **"focused" is not "owns input"**; the
two signals FLUF itself acts on are `WantTextInput` (keys) and "cursor inside a window rect" (mouse).

### C. Hook sites FLUF (nightly, core + debugger) shares with FLvibed
Re-derived from the source and the shipped `FLUF.dll` (handler RVAs from the shipped `FLUF.pdb`, entry bytes read
from the DLL; `overlap_176c263.txt` = every exe address in FLUF's sources x all 195 `flexe.h` spans: the same 24 hits
as at 35b71fd).

| FLvibed site | FLUF nightly | Form | Effect on FLvibed (from source) |
|---|---|---|---|
| FrameUpdate 0x5B2890 | `FlufClient::OnUpdate` FLUF.dll+0x3E50D0 (`55 53 57 56 83 EC 0C`) | FunctionDetour (restore per call) | hooked at the handler (da1363f), either order |
| UiRender 0x59E300 | `UiManager::UiRenderDetour` +0x558C30 (`53 57 56 8B 35 imm32`) | FunctionDetour | as above |
| CursorUpdate 0x41F150 | `UiManager::CursorRenderDetour` +0x558CB0 (`53 56 8B 35 imm32`); NOP 0x41F33E-0x41F6E9 | FunctionDetour + NOPs | as above; FLvibed's hidden cursor clears `exe::CurrentCursor` around it (da1363f) |
| WinKey 0x577850 | outer `UiManager::WinKeyDetour` +0x558B90 (`55 53 57 56 8B 6C 24 1C`), inner `MessagePumpKeyHandlerDetour` +0x40D260 (KeyManager is built first, `FlufClient.cpp` 645-646) | two nested FunctionDetours | FLvibed hooks the outer handler: it sees every key before ImGui / FLUF |
| HandleKey 0x576410 | `KeyManager::HandleKeyDetour` +0x40CDB0 (`53 57 56 83 EC 08`) | **FunctionDetour** (2.0.0's 7-byte `B9 imm32 FF E1` overwrite is gone) | handler hook; FLvibed's pad commands then pass FLUF's blocked / overridden keys (USER_CANCEL, mouse buttons, USER_NAV_MAP, 18, 19) |
| PlayArch 0x428680 | `SoundApi::CreateSoundDetour` +0x4105D0 (`55 53 57 56 83 EC 10`) | FunctionDetour | handler hook; FLUF's own sounds bypass FLvibed (section 8) |
| UiControlRemove 0x59DC00 | jump at 0x59DC09 (`OnTerminate`, `Client\Ui\Vanilla\HudManager.cpp` 228-249, 347) | 5-byte E9 | wildcarded +9..+13 (da1363f): matches either order |
| (CreateControl 0x59DB20 - FLvibed calls the factory through `CreateIconButton`) | `HudManager::CreateControlDetour` writes `PerformDetour` (+0x560350) into **vt[0xA8] of every control class it sees** (`HudManager.cpp` 301-335) | FunctionDetour + vtable writes in Freelancer.exe | **FLvibed's `SetProp` (`controller\src\host\uibtn.cpp` 124-140) and the HUD 2 click boxes (913-920) accept only the game's setters in that slot -> refused**: the pause-menu caption, box re-pitch, bar-button show / lit / colour / position, options-button text, HUD 2 hotspots. Fails closed. Seen in the 2.0.0 run's logs ("caption text not set (setter unknown)", only with FLUF): 2.0.0 had it too. A `FlControl` whose suppression bit is set (the palette's `ManeuverFrame` and siblings) gets no messages at all |
| MainMenuButtonClick 0x5664A0 | `FrontButtonClickDetour` +0x560260 | FunctionDetour | FLvibed only calls it; passes unless a FLUF hud vetoes (none in core) |
| IServerImpl vtable (server.dll, SP too) | slots 0x78 ReqAddItem, 0x7C ReqRemoveItem, 0x84 ReqSetCash, 0x88 ReqChangeCash, 0xB4 GFGoodBuy, 0xB8 GFGoodSell and 30 others, written when content.dll loads (`Server\FlufServer.cpp` 891-903, `Include\Server\Internal\IServerHook.hpp` 212-293) | pointer swap | `controller\src\host\events.cpp` `HookServerMethod` finds a slot only while it holds the export: **"server economy hooks 4 of 10"** (logged in both 2.0.0 orders, 10 of 10 without FLUF); SPScanCargo untouched |
| Common.dll vtables | Evade / Buzz / Face behaviour updates re-implemented; CShip, CSolar, CLoot, CMine, CCounterMeasure, CBeam, CGuided, CEShieldGenerator, CEPower, CELauncher, CEGun slots (`Common\CObjectHook.cpp` 99-172; `Fluf.cpp` 826-834) | `VTableHook` (throws "Double hooking detected" on a range patched before it) | Evade: FLvibed leaves the slot (da1363f). **`hk::Virtual` lookups** (`core\hook.cpp` `FindSlot` 960-984) end a vtable at its first entry outside the module, i.e. at FLUF's pointer: virtuals of those classes are refused - `CObject::get_surface_extents` on CShip logged in the 2.0.0 run (only with FLUF); `CELauncher::GetAmmoCount` and the CEGun / CBeam / shield ones are at risk the same way |
| common.dll LaneAction (FLvibed `core\game\route.cpp` 20-27 lane law) | `AllowAdjustableTradelaneSpeed` jumps at common+0x62043 / 0x6209E / 0x614E3 (`Common\Fixes\AllowAdjustableTradelaneSpeed.cpp` 98-108, always on) | 5-byte E9 + NOP | the lane-law signature **starts at 0x62043**: no match -> the law keeps its defaults (`fromGame` false); the speed is now per ship / lane solar (an equipment-manipulated value) |
| ScreenW 0x610850 readers in the mouse code | `ScaleCursor` points five `fild [ScreenW]` operands (0x41FB6C, 0x41FBE0, 0x41FDB9, 0x41FE25, 0x41D9C5) at its own int = primary-monitor height x 4/3 (`WidescreenFontScaling.cpp` 137-151) | operand write | FLvibed's `ScreenW` / `MouseSens` signatures intact; the game's mouse-to-cursor scaling changes - pad cursor speed / reach to test |
| MouseWheel 0x616848 | zeroed while ImGui wants the mouse | data write | wheel reads after FLUF's handler see 0 |
| D3D8 | `Direct3DCreate8` code (un-detoured at its first call), `CreateDevice` (vtable[15]) and `Reset` (vtable[14]) code in d3d8.dll (`UiManager.cpp` 158-211); rp11: its own back end | FunctionDetour | FLvibed swaps vtable / IAT pointers: chains either way |
| Freelancer.exe IClientImpl vtable | slot 0 (0x5E6668, chat payloads, `ClientServerCommunicator.cpp` 427-436) | pointer swap | FLvibed's client hooks are slots 75 / 83: no clash |
| kernel32 `LoadLibraryA/ExA/ExW`, `FreeLibrary`; dacom `FDUMP`; dacom.dll+0x2683 | FunctionDetours / pointer / call patch | - | FLvibed's dev hot reload passes FLUF's loader detours (its DLL names are not on the forbidden list) |

Not shared: FLvibed's other `flexe.h` sites (DmgInd*, Comm, JumpInComplete, IsCmdHeld, SetAppActive, MouseSteer, HitTest,
Char ctor / dtor, the maneuver bar, prefs and pause-menu functions, FindVoice, HudDrawPart) carry no FLUF patch; the
ScreenControl patches (0x5B2497, 0x5B254C, 0x5B264C, 0x5B2665) lie outside every signature.

### D. What our harness and tests assume that the nightly breaks, and the A3 checklist
| Assumption (`docs\TESTING.md`, `controller\docs\HARNESS.md`, the 2.0.0 runner `research\experience\runs\fluf_compat_2026-10-25\tools\ff.ps1`) | Under the nightly (source) | How the test adapts |
|---|---|---|
| FLUF loads with `dll_search_path` alone; its helper DLLs sit beside Freelancer.exe (ff.ps1 `Set-Dacom`) | needs `fluf/dacom_fix.dll` as the first `[Libraries]` entry; only `freetype.dll` is a helper and it stays in `fluf\` | `Set-Dacom` adds `fluf/dacom_fix.dll` first; a fresh copy of `vanilla11` (no 2.0.0 leftovers); one run with `fluf/FLUF_boot.dll` instead of `fluf/FLUF.dll` (the boot form) |
| "FLUF loaded" = a module `fluf.dll` or `FreelancerSDK.dll` | `FLUF.dll`, `dacom_fix.dll`, `freetype.dll` (+ `FLUF_boot.dll`), `debugger.dll` | the module check names these; FLvibed's "FLUF present" log line |
| the start reaches the main menu unattended | first run: "No ship classes" (answer OK - Cancel aborts the game), the console window takes the foreground; any forbidden-DLL / "No Fonts" box blocks | answer OK as before; **seed `fluf\FLUF.yml` before the first start** with `logSinks: [File]` alone (read with `rfl::DefaultIfMissing`: missing keys take their defaults; a malformed file raises a "Config Load Error" box - OK resets, Cancel exits; `Include\Common\Config\ConfigHelper.hpp` 19-51) so the console never opens (background rule); fonts copied to `DATA\FONTS` |
| the game window may lose focus (background tests) | ScreenControl keeps simulating only with `-w`; FLvibed's own `SetAppActive` hook still does it otherwise | unchanged (FLvibed's background mode); note whether the instance starts with `-w` |
| USER_CANCEL reaches the game (`Skip-FLAnimation`, `Close-FLBlocker`, `post 27`) | eaten while an escape-closeable FLUF window is open | before a skip / close: no FLUF window open (state from FLvibed's FLUF probe once A2 has it; until then frames + `FindWindowByName` via a debug command) |
| `tap 99` / the hub / HUD 2's hub button open the vanilla nav map (padmap screen, `controller\src\impl\padmap.*`) | FLUF's ImGui map opens instead; the mission briefing shows FLUF's map | test the pad on FLUF's map (cursor works; magnetism / destination list absent) - a recorded conflict until A2 decides |
| NeuroNet Player Info opens the vanilla screen | FLUF's Player Status window | F8 still vanilla; the NN button = FLUF's window |
| `click` / `visible` on `ManeuverFrame` children work | palette open: hidden, disabled, messages suppressed (siblings too); closing re-shows them **even if HUD 2 had hidden them** | check HUD 2 / Extended HUD 2 after a palette open + close |
| keys posted to the game reach it (`post <vk>`) | not while a FLUF text box has `WantTextInput`; FLvibed's hook still sees them first | judge "keys" from FLvibed's raw-key counter and FLUF's frame |
| mouse clicks / pad A at a HUD element reach the game | eaten over any FLUF window (Debug Window at (60,60) by default) | place the HUD 2 click / dealer tests away from the Debug Window or close it; record the overlap |
| native buttons can be re-labelled / moved (`uibtn`) | refused (vt[0xA8]) - see C | expect "setter unknown" log lines until A2 fixes it; frames of the pause menu and the bar button |
| economy / cargo events from the server hooks | 6 of 10 hooks missing | check `events: server economy hooks` and a dealer purchase event |
| subtitles: comm lines arrive through PlayArch / Comm with the game as caller | unchanged (handler hooks) | comm with subtitles, as before |
| FLSpew lines parse unchanged (`tools\flfreeze.ps1`) | prefixed `[timestamp] ` | tolerate the prefix |
| saves are encrypted | FLUF writes them in plain text | tools that read saves must accept both (the game does) |
| lane travel uses the game's lane law | defaults (signature broken) | a lane ride in the autopilot leg; compare its time with the no-FLUF run |

**A3 checklist** (both `[Libraries]` orders, each against the same fresh instance without FLUF; HUD = HUD 2 /
Extended HUD 2 / classic; device = pad / keyboard-mouse; every cell: frame + FLvibed's and FLUF's logs):
1. Start: dialogs answered, no console, `FLUF present` + "hooked at its handler" for the six sites, no refusal;
   `hooks` in both channels; memory of the process after load (FLUF maps ~424 MB).
2. Main menu: FLUF arrow vs pad cursor (moves, hides on pad screens, reaches all four edges at the instance's
   resolution - ScaleCursor), the Debug Window's rect vs FLvibed's menu prompts, FLvibed's options page (open,
   tabs, pad), the New Game prompt; `--save-file` not used.
3. Each FLUF element x HUD x device: palette (open with tap 19, type, Esc twice, reopen + click outside; bar and
   siblings hidden then shown, HUD 2 / Extended HUD 2 plates / strip / pills / cards intact after closing), options
   menu (tap 18; top-left rect vs HUD 2's top-left; Esc; the resolution tab not exercised - a device reset is its own
   test), Player Status (NN button; Esc), nav map (USER_NAV_MAP from keyboard, the pad hub, HUD 2's hub button; the
   pad cursor on it; Esc), Debug Window (rect vs the strip / pills; click through it; its close box), a toast
   (a palette command that fails) vs the top-left HUD.
4. Flight (pad throttle, steering, HUD 2), a fight (hit cues: shield / hull cues, `hitlook`), the autopilot incl.
   **Evade with FLUF owning the slot** (FLUF's re-implemented Evade / Face / Buzz updates on the player ship) and a
   tradelane ride, a base with a dealer (the vanilla dealer: wheel scroll + ship-dealer scroll bar changes; FLvibed's
   pad dealer screens; a purchase -> economy events), a comm with subtitles and portraits, a mission briefing
   (FLUF's map; `Close-FLBlocker`), the pause menu (the FLVIBED caption).
5. Keyboard-mouse: FLvibed's bound keys while the palette has text focus (they act - a recorded conflict), clicks
   over FLUF windows, the mouse wheel over / beside them.
6. Logs: FLvibed's (refusals, `setter unknown`, `economy hooks`, `virtual ... not found`, lane law), `fluf.log`
   (errors, "Key ... is not an overridable key"), FLSpew. Every failure is first matched against F (FLUF's known
   issues): a match is a recorded conflict with the issue number, not FLvibed's failure.

### E. What FLvibed needs from FLUF for the awareness layer (A2)
- **Detect (once, at the first frame, as today):** `GetModuleHandleA("fluf.dll")`; install form from
  `dacom_fix.dll` / `FLUF_boot.dll`. **Version:** the version resource is always 1.0.0 (every FLUF DLL), so use (a)
  capability exports - nightly-class = `?SetKeyBlocked@KeyManager@@QAEXW4Key@@_N@Z` and
  `?GetImGuiInterface@FlufClient@@SAPAVImGuiInterface@@XZ` present (2.0.0 has neither; it has
  `?GetImGuiInterface@UiManager@@QBEPAVImGuiInterface@@XZ`), (b) `?GetVersion@ImGui@@YAPBDXZ` -> `"1.92.7 WIP"`, (c) the
  build id = `FLUF.dll`'s PE `TimeDateStamp` 0x6AD1955A (+ `SizeOfImage` 0x1944D000) for nightly-2026-10-16 (2.0.0:
  0x69DD76EF / 0x5BB000). A known id or capability set enables the awareness; anything else = co-residency only + one
  log line (the brief's rule).
- **Live state + rectangles:** FLUF exports Dear ImGui (895 functions: `GetCurrentContext`, `GetIO`,
  `FindWindowByName`, `IsWindowHovered`, `IsAnyItemActive`, `DebugCheckVersionAndDataLayout`). Cheapest robust read,
  after FLUF's frame (FLvibed's `CursorUpdate` hook returns after FLUF's handler ended the frame): for each name in
  B (`fluf_cmd_palette`, `Player Status`, `NavMap`, `NavMapObjectiveWindow`, `NavMapDescriptionWindow`, `Debug Window`,
  `DEBUG: Target Info`, `##TOAST0..4`, plus modules' windows) `FindWindowByName` -> `ImGuiWindow` `Active` / `Hidden` /
  `Pos` / `Size` / `Flags`, or walk `ImGuiContext::Windows` with FLUF's own rule (active, background or `FlWindow`,
  `UiManager.cpp` 248-271). The field offsets come from Dear ImGui's headers at FLUF's pin (upstream ocornut/imgui
  docking 04aaf311, MIT - the same headers the package ships), gated by `GetVersion() == "1.92.7 WIP"` and
  `DebugCheckVersionAndDataLayout(...)` with the expected struct sizes; a mismatch = no rectangles (fail closed:
  conservative keep-outs from the source rules in B). Event-driven enough: one name lookup per window per frame, or
  only on frames where `IsWindowHovered(ImGuiHoveredFlags_AnyWindow)` / an `Active` change says something moved.
  Fallback without layouts: the persisted `imgui.ini` (user-data folder; Debug Window's dragged rect) and the source
  rules (palette top centre 0.3 W x 400; options top-left 0.933 H x 0.7 H; status / map centred 0.933 H x 0.7 H).
- **"A FLUF window has the input":** keys - `ImGuiIO::WantTextInput` (exactly FLUF's own rule for swallowing keys;
  `IsAnyItemActive()` as the layout-free stand-in); mouse - the game cursor inside one of the rects above (FLUF's rule
  for eating USER_BUTTON0-2 / USER_WHEEL). Not `IsWindowFocused` (sticky, see B).
- **Hotkey clashes:** FLUF claims USER_YES (19), USER_NO (18) (unbound by default), USER_CANCEL (15) and the mouse
  buttons / wheel (conditional), USER_NAV_MAP (99, always); `SetKeyBlocked` lets modules drop any command. FLvibed's
  own binds (raw keys: the autopilot key, the page) never pass through FLUF's KeyManager, but they fire while FLUF's
  palette has text focus (FLvibed is in front) - A2: yield while `WantTextInput`. FLvibed's pad commands 15 / 99 /
  mouse buttons are subject to FLUF's rules (B).
- **Fixes the source asks for (all fail closed at A2; *fixed 26 Oct 2026 in step 2, 3582c29 - see Status below*):** accept FLUF's `PerformDetour` in vt[0xA8] as a forwarding
  setter (it calls the original by the object's real vtable - so not for FLvibed's private hotspot vtable, whose
  original must be the game's known setter `exe::IconButtonSetProperty`); `HookServerMethod`: find the slot index in
  the module's on-disk vtable (or treat a slot pointing into FLUF as the export's) and chain over FLUF's pointer;
  `FindSlot`: step over entries that point into a restore-style module (FLUF) instead of ending the vtable; the lane
  law: a signature that tolerates the E9 at 0x62043 (the END / RAMP operands are untouched) and a note that FLUF makes
  the speed per lane; padmap: recognise FLUF's map (`NavMap` active) and step aside with a prompt.
- **Config keys worth reading:** `fluf\FLUF.yml` `clientConfig.modules` (which modules - each may add windows / keys),
  `logSinks`, `clientConfig.loadedFonts`; `constants.ini [FlufConstants]`. Not needed for rects.
- **Review notes (26 Oct 2026):** the options window and Player Status share one ImGui window ("Player Status", B):
  one rect / one "open" flag for both is all a name lookup gives - enough for a keep-out, not for naming the window.
  The capability exports are C++-mangled names of classes FLUF is about to restructure (issue #141 "De-friend FLUF"):
  use them with the build id list, never alone. FLvibed's `core\game\gamefont.cpp` reads the font names from
  `fonts.ini` on disk; under FLUF the game uses an in-memory copy with smaller sizes on > 1400-px monitors and a
  substitute where a named font is missing (A) - the game's own text then differs from the no-FLUF baseline and FLvibed
  may name a face the game no longer uses (A3: compare HUD 2 / subtitle text with and without FLUF at 4K).
- **Status (A2, 26 Oct 2026: built and unit-tested; A3 26 Oct 2026: works in game in both orders after fix 3b4bb4a - the
  probe had failed closed for good at FLvibed's first frame, before FLUF's first ImGui NewFrame (DisplaySize (-1, -1));
  `docs\MOD_COMPATIBILITY.md` "The FLUF-awareness layer", review `research\experience\runs\fluf_nightly_2026-10-25\A2_REVIEW.md`,
  result `TABLE.md` there):**
  - Detect: **built** (`core\fluf.*`): build id first (the table above), the capability exports only logged; the ImGui
    version gate; `DebugCheckVersionAndDataLayout` deliberately **not** called (it asserts inside FLUF on a mismatch) -
    the layout is checked by tying each region to the exports instead (io at context+48, io.Ctx, FrameCount,
    DisplaySize, the window list, each window's Ctx / Name / LastFrameActive). Offsets: ImGuiContext IO 48, FrameCount 4,
    Windows 5224; ImGuiIO DisplaySize 8, WantTextInput 142, MousePos 180, Ctx 176; ImGuiWindow Name 4, Flags 12, Pos 76,
    Size 84, Active 245, Hidden 253, LastFrameActive 688 (offsetof on the shipped headers with FLUF's definitions).
  - Live state + rectangles: **built** - the window list walked once per FLUF ImGui frame (not `FindWindowByName` per
    name: one pass also covers module windows); "open" = begun this frame or the last (`LastFrameActive`), so the read
    works between FLUF's frames; HUD bus v4 serves it to both logics.
  - "Has the input": **built** - keys by `WantTextInput`, mouse by FLUF's own rule over the rectangles. **26 Oct 2026
    (the owner's attended run, the owner's rule):** keys also while the palette is open, focused or not ("much like a
    chat text box"); the pad on every window that takes input, decided from ImGui's state (FLUF's click rule and not
    `NoMouseInputs`; never the Debug Window or a toast) - the attended run's "other" window was the palette command
    `Move System` (`Common\Commands\Admin\AdminSystem.cpp`; every admin command opens such a window: Adjust Cash, Base,
    Move, Message, Reputation, System); FLUF's Escape (`UiManager::ProcessEscapeKey`) closes only its FlWindows, so B
    closes a command's window by its title bar's close button. Bus v5 (`ReadFlufExtra`): names, input, focus
    (`ImGuiContext::NavWindow` 8236, checked to be a listed window), the close button (`ImGuiWindow::HasCloseButton` 256,
    `TitleBarHeight` 140, `SizeFull` 92, `RootWindow` 928, `ImGuiStyle::FramePadding` at context+3252+76; offsetof on
    the shipped headers, the older offsets measured again: equal). `FocusSearchBox`: exported but not clean to call
    (`MOD_COMPATIBILITY.md` FLUF "Input"). Checked against f52815b + the working tree.
  - Hotkeys: **documented, FLvibed yields** (MOD_COMPATIBILITY "Hotkeys"); no rebind UI beyond `autopilot_key`.
  - Keep-outs: **built** for HUD 2, Extended HUD 2's docked overlay, the prompt lane, the subtitles; the uiskin plates
    (the game's own windows re-skinned) are not moved - FLUF's windows lie over them as over the game's.
  - padmap vs FLUF's map: **built by the pad rule** - FLUF's map never opens the vanilla map padmap keys on; while FLUF's
    `NavMap` is open the pad drives it with the cursor and the prompt bar.
  - **Fixed in step 2 (26 Oct 2026, 3582c29, verified in game in both orders; TABLE.md "Step 2"):** one mechanism - the
    game module's original image read from its own file (`hk::OriginalAt` / `OriginalPointer` / `FindInModuleOriginal` /
    `VtableSlot`, only after a live read failed) - gives the vt[0xA8] forwarding setter (accepted when the class's
    original slot is a known setter), `HookServerMethod` over FLUF's slots (10 of 10), `FindSlot` past FLUF's
    `VTableHook` pointers, and the lane law behind FLUF's jump (`lanelaw` = game). The options page's FLUF row note
    (af2adb1). `fluf_windows` default 2 (the owner, af2adb1). Unit test `orig_image`.

### F. Known issues (FLUF's tracker, read 26 Oct 2026)
Source: https://codeberg.org/TheStarport/FLUF/issues through the API (`research\evidence\fluf_nightly\issues_open.json`,
`issues_closed.json` = the 50 most recently updated of 97 closed, `pulls_open.json` = none open; comments of #93, #106,
#108, #111). 35 open: 1 labelled bug (#111), the rest enhancements / module requests (most "low prio"). The newest closed
activity is 12 Sep 2026, so every closed issue below predates the nightly (16 Oct) and its fix is in the build.
**Rule for A3: a symptom listed here is FLUF's - recorded as "recorded conflict (FLUF #n)", not counted as FLvibed's
failure, after checking it also happens without FLvibed in the chain when cheap (the FLUF-only start).**

Open - what a player / FLvibed would see, and the effect on our tests:
| # | Title (labels) | What a player / FLvibed sees | A2 / A3 |
|---|---|---|---|
| - | **found by A3, not in FLUF's tracker** (not filed: the owner decides) | at the first start on the game's own data an endless chain of modal "ERROR" boxes "header [system] value pos missing parameter 2" (then `[zone] value size missing parameter 1` ...): `UniverseCache::Initialize` (`Source\Common\UniverseCache.cpp` 161 universe.ini `[system] pos`, 232 / 308 / 316 / 317 the system files' `[Object] pos`, `[Zone] pos` / `size` / `property_fog_color`) reads with FLSDK `INI_Reader::GetVector` (`source\FLCore\Common\INI_Reader.cpp` 180-191: get_value_float(p), (p+1), (p+2) unconditionally); the game's `[system] pos` has 2 values, a sphere zone's `size` 1; every missing parameter = one box from Common.dll's reader: 6,758 on vanilla 1.1 data by source (53 universe + the system files, Li01 493), 956 answered in 60 s in A3 without an end - the game never reaches its menu | A3 ran with a test-only text copy of universe.ini and the 53 system files, those vectors padded to 3 values (`research\experience\runs\fluf_nightly_2026-10-25\tools\bini_universe.py`); attributed by source (a FLUF-only start would take the screen and play sound) |
| 111 | Minor UI/Resolution Bugs (bug, client) | open: Thorn (in-engine) scenes very zoomed in at high resolutions; changing the resolution in FLUF's options window crashes or misbehaves; no way to pick a resolution other than the screen size. Done: the cursor scale (9c0736d), Screen Control moved into core | A3: scenes at 4K look zoomed with FLUF - FLUF's, not FLvibed's subtitles / movie handling (compare the frame without FLvibed if in doubt); never pick a resolution in FLUF's options (B: the Display tab) - a crash there is FLUF's |
| 131 | Override game settings window (client, UI) | planned: the vanilla Game Settings screen re-implemented in ImGui (from the QoL patcher) | **steer clear:** FLvibed's options page / pad screens must not assume the vanilla options screens' layout when FLUF is present; A2 detects FLUF's windows by name, not by the vanilla screen |
| 73 | Character Select / New Character screen overhaul | planned: both screens in ImGui (an early character select exists - B's MP `Character Select`) | **steer clear:** FLvibed's New Game prompt and pad screens on those menus; today SP still uses the vanilla screens |
| 141 | Tech debt: De-friend FLUF | planned: classes-as-namespaces with `friend` -> namespaces and internal headers | the exported C++ names E uses for version detection may change or vanish: build id first, capability exports second, fail closed |
| 125 | Merge FLUF.CrashWalker into core | planned: the crash handler (an update-loop hook) moves into core | another core hook on the `FrameUpdate` path in a later build: re-derive C for any new build |
| 45 | Suppress flspew.txt if fluf.log is created | planned: FLSpew may go to `fluf.log` only | `tools\flfreeze.ps1` / check-ins that read FLSpew need `fluf.log` as a fallback in a later build (the nightly still writes FLSpew, prefixed - D) |
| 76 | Launch arg: console to stdout/stderr | requested: no console window even with the console sink | our `logSinks: [File]` seed (D) stays the way |
| 106 | Freelancer Offset Mega Issue | candidate exe patches (MP / New Game / Load Game menu buttons removable at 0x574634 / 0x5746CA / 0x574707 / 0x574744, 64 simultaneous firings, frame-drop patches 0x42D7B8 / 0x558A10, IPv6) | none applied in the nightly; an address grep finds no `flexe.h` site at them (nearest: `GamePaused` 0x42D740). A mod hiding New Game / Load Game would meet FLvibed's main-menu prompt - check when it lands |
| 61 | Renderer Override module (DX9 / 11 / Vulkan) | planned module; core already has a D3D11 back end for ImGui (`rp11`, C) | a non-D3D8 renderer would bypass FLvibed's D3D8 vtable hooks (drawing): out of scope for the nightly's core; check if a FLUF install loads it |
| 63, 67 | Weapon group indicators; palette favourites | planned: a new HUD element (weapon groups); stars in the palette list | a future HUD element to add to A2's keep-outs by name; the palette's rect rule (B) is unchanged |
| 74, 59 | Free time between M01a and M01b; mission trigger additions (Story Enhancements module) | module behaviour, off by default | the story flow our runbooks assume holds while `story_enhancements` is not loaded (A3: core + debugger only) |

Recently closed (fixed in the nightly) that a test could still meet - a recurrence is FLUF's regression:
| # | Title (closed) | Relevance |
|---|---|---|
| 129, 136, 135 | Re-implement NavMap; detect the Mission Briefing screen; ImGui windows override the cursor (8 Sep) | the origin of B's nav map, briefing map and cursor shapes - the pad map screen's conflict (D) is by design, not a bug |
| 109 | Opening the NavMap crashes the game (19 Jul, high) | a crash on opening FLUF's map with `FLUF.dll!HudManager::OnUpdate` in the stack = FLUF's (A3: tap 99 / the hub / HUD 2's hub button) |
| 108 | Docking on a base in single player no longer saves (19 Jul, high) | the player was put in space instead of undocking; a base test that launches at a random place = this |
| 105 | Crash loading the Mission 1 "Join The Convoy" autosave (13 Jul) | a null in FLUF's `ResourceManager::CObjectFindDetour`; our fixtures load saves - a load crash there = FLUF's |
| 132, 95 | NPC projectiles stop hitting the player after a reload (1 Aug, high, server); can't damage NPCs / lanes (13 Jul, MP) | a fight after a save load with no hits either way = FLUF's; compare with the no-FLUF fight |
| 107 | Exception in dacom.dll when exiting from the menu (20 Jul, low) | a crash on quit; harmless to the result, recorded |
| 119 | All custom UI shut down on a context change (20 Jul) | FLUF closes its windows on dock / launch / main menu: A2's FLUF window state resets there (no stale keep-outs) |
| 93, 92 | HUD gauges missing at 3840x2160; fonts missing from `fonts.ini` (8 Jul) | the fix is the primary-monitor font rescale (A): text sizes differ from the baseline at 4K |
| 117 | Cruise straight from engine kill does not re-activate the engine server-side (20 Jul) | FLUF's fix changes nothing FLvibed reads; the autopilot's engine-kill -> cruise sequence to watch in A3 |
| 24, 30 | Adjustable tradelane speed; NoNavMap | the lane-law signature break (C); FLUF's map can show "no nav map" in configured systems (none by default) |
| 58 | Screenshots as PNG (9 Jul) | Print Screen writes `png` (A); the harness does not use it |

Unmerged branches (not in the nightly; what FLUF is about to change - compare `master...<branch>` through the API):
| Branch | Ahead of master | What it changes | A2 / A3 |
|---|---|---|---|
| `ai` | 42e60e9 (9 Oct 2026, 1 commit) | "hooked and reimplemented all NPC Behaviors & Ideas": `Internal\AI\AiHooks.hpp`, `AiVTables.hpp`, every behaviour (Cruise, Dock, Evade's neighbours DrasticEvade / Flee / Follow / Formation / Goto / Strafe / Trail / ...) and idea (BreakAway, Dodge, DodgeMissile, Face, ...) as Common.dll vtable hooks | **steer clear:** when it merges, many more Common.dll AI vtable slots are FLUF's - FLvibed's Evade guard, the virtual lookups (C) and any AI read must keep treating a non-Common slot as another owner's (fails closed, logged); re-derive C for that build. Not in the nightly: A3 is unaffected |
| `haste/interpolation` | 1 commit (31 Aug 2026) | multiplayer remote-object interpolation (`Fluf.cpp`, packet hooks) | multiplayer only; FLvibed is single player - none |

Verification of this section against the live repository (coordinator, 26 Oct 2026 ~00:20): latest pre-release still
nightly-2026-10-16-03-03 = master HEAD 627ea2c (no newer tag or commit); `docs\` last changed 23 Sep 2026 (before the
tag: the package's docs are the current docs; no separately hosted docs site); open issues 35 and closed 97 match;
open pull requests: none (the `pulls` endpoint and `issues?type=pulls` both empty); branches `master`, `ai`,
`haste/interpolation`, `FlufVersion1` (old, 18 Jun) - the two live ones are in the table above.

## Findings

### 1. Status (verified 25 Oct 2026)
| Fact | Value | Source |
|---|---|---|
| Home | https://codeberg.org/TheStarport/FLUF (created on Codeberg 10 Nov 2025; GitHub `TheStarport/FLUF` archived, "Migrated to https://codeberg.org/TheStarport/FLUF", last push 10 Nov 2025) | [api] codeberg.org/api/v1/repos/TheStarport/FLUF ; api.github.com/repos/TheStarport/FLUF |
| Description | "Freelancer Universal Framework" (the request's "Unified" is not the project's word) | [api] |
| Activity | 1,325 commits; last 627ea2c 15 Oct 2026 (Aingar); the 50 newest (21 Sep - 15 Oct 2026): Lazrius 41, Aingar 7, Haste 2; 35 open issues (one "Freelancer Offset Mega Issue": its addresses are hard-coded). 26 Oct 2026: HEAD still 627ea2c; issues 35 open / 97 closed, no open pull requests ("FLUF in play" F) | [api] commits (X-Total 1325), issues; `fluf_nightly\commits_head_2026-10-26.json` |
| Releases | stable "FLUF 1.0.0" 6 Feb 2026, "1.0.1" 7 Mar 2026, "FLUF 2.0.0 Release Stable" 28 Mar 2026 (FLUF.tar.xz 55.7 MB); nightlies, newest nightly-2026-10-16-03-03 (FLUF.tar.xz 139.4 MB) | [api] releases |
| Licence | GNU AGPL-3.0 (`LICENSE.md`; the API reports none) | [file] |
| Recent work | dx11 renderer support (9 Oct), sound API / per-thruster sounds, key-input blocking (5 Oct), an ImGui equipment-dealer overhaul (Sep), munition extensions | [api] commit messages |
| Docs | https://the-starport.com/forums/docs/fluf (HTTP 200 on 25 Oct 2026); same text in the repository's `docs\content` | [page] [file] |
| SDK | FLSDK, https://codeberg.org/TheStarport/FLSDK (updated 9 Oct 2026; GitHub FLHookSDK archived 1 Jul 2026 -> Codeberg) | [api] |
| Older generations | TheStarport/FLHook archived (last push 16 Jul 2026); its live continuation FLHDE/FLHook (server, pushed 21 Oct 2026); JFLP 1.25 / jflp.dll 1.27 on adoxa's site, page "Updated 25 April, 2026" | [api] [page] http://adoxa.altervista.org/freelancer/ |
| Users | Discovery 5.3 ships a FLUF build of the 1.x layout (`FLUF.dll` + `FLUF.yml` in EXE, `FLUF.UI.dll` as a module; discovery.md 3.7); FLUF 2.0 merged UI and fixes into core. No other mod confirmed as shipping it | [file] discovery evidence; the 2.0.0 release notes |

### 2. What it is (from the source)
- A **client and server framework**: `FLUF.dll` (core: hooks, ImGui UI with D3D8 / OpenGL (FLAR) / D3D11 back ends,
  a command palette, infocard / territory / hardpoint / resource managers, the client-server communicator) plus
  modules (`fluf\modules\*.dll`, chosen in `fluf\FLUF.yml` `clientConfig.modules` / `serverConfig.modules`; 28 in the
  repository: autobuy, combat_ai, objective_tracker, qol_patcher, smooth_strafing, story_enhancements, ui_overhaul,
  ship_dealer_enhancements ...). Server side: PostgreSQL player data, FLServer hooks.
- **Install** (docs `getting-started.mdx`): `dacom.ini [DACOM]` `dll_search_path = .\fluf` and `.\fluf\modules`;
  `[Libraries]` first entry `dacom_fix.dll` (patches `dacom.dll+0x2683`'s `LoadLibraryA` call into
  `LoadLibraryExA` with those search paths and `SetDefaultDllDirectories`, process-wide), **last entry
  `fluf/FLUF.dll`**. `FLUFBoot.dll` (in the source, not in the user docs) is an alternative loader that requires
  `dacom_fix.dll` and loads `.\fluf\FLUF.dll` under an exception handler. Needs the VS2026 x86 runtime. The first run
  writes `fluf\FLUF.yml` and creates `fluf\config`.
- **Everything is installed in `FLUF.dll`'s DllMain** (the `Fluf` constructor -> `FlufClient`): so the `[Libraries]`
  order decides who hooks first.
- **Exe assumptions:** all addresses are hard-coded v1.1 VAs (no signatures); FLUF also detours kernel32's
  `LoadLibraryA` / `LoadLibraryExA` / `LoadLibraryExW` / `FreeLibrary` (module bookkeeping, the forbidden-DLL list)
  and marks the version number (0x67ECB8) so FLUF clients and non-FLUF servers refuse each other.

### 3. The hook mechanism (FLSDK `include\Utils\Detour.hpp`, ae8c857; the same in FLHookSDK 4bdb288)
`FunctionDetour<T>`: `Detour()` copies the first 5 bytes to `data` and writes `E9 rel32`; `CallOriginalFunc()` does
`UnDetour()` (copies `data` back), calls the function, then `Detour()` again (re-saving the 5 bytes found there).
The protection of the 5 bytes stays RWX. `VTableHook<Start, End>` swaps entries of a module's vtable and **throws
"Double hooking detected"** if any entry in its range already points outside that module (i.e. someone hooked first).
Byte patches: `MemUtils::PatchAssembly` (5-byte E9 / E8), `PatchCallAddr`, `WriteProcMem`, `NopAddress`.

FLvibed's `hk::Detour` (`core\hook.cpp`): if the site starts with `E9`, the trampoline jumps to the previous hook and
FLvibed writes its own jump over the site; otherwise it copies the stolen bytes. Symbol resolution skips an `E9` at the
hook offset, so a site FLUF already took still resolves.

| Order | Effect at a shared site |
|---|---|
| FLvibed, then FLUF | FLUF's `data` = FLvibed's jump; every FLUF call restores it, so the call reaches FLvibed's hook and trampoline. Both run. FLvibed's NOP padding after the jump (6-8 stolen bytes) is outside FLUF's 5 bytes. |
| FLUF, then FLvibed | FLvibed's jump replaces FLUF's; FLvibed's trampoline -> FLUF's handler -> `UnDetour` writes the vanilla bytes over FLvibed's jump -> `Detour` writes FLUF's. From the second call on, only FLUF runs. |

### 4. FLvibed's sites that FLUF touches (`overlap_35b71fd.txt`; all `flexe.h` spans x every exe address in FLUF's sources)
The "Either order?" column is the state at 35b71fd. Since da1363f: the six `FunctionDetour` sites chain in either
order (section 8); UiControlRemove's signature has wildcards at +9..+13, so it resolves in either order; the Evade slot
is left to FLUF (own dodges); the cursor hide reaches FLUF's arrow.
| FLvibed symbol (stolen) | FLUF | Either order? |
|---|---|---|
| FrameUpdate 0x5B2890 (5; both hosts' logic) | `FunctionDetour` (`Fluf.cpp`, `FlufClient::OnUpdate`) | FLvibed first only |
| UiRender 0x59E300 (5) | `FunctionDetour` (`UiManager`); plus `PatchAssembly` 0x59E40B inside the function (outside the signature) | FLvibed first only |
| CursorUpdate 0x41F150 (6; controller + subtitles) | `FunctionDetour` (ImGui render / input poll around the call); NOP 0x41F33E-0x41F6E9 (the cursor draw; FLvibed's `CursorShown` / `MouseX` / `MouseButtons` / `MouseWheel` signatures lie outside) | FLvibed first only; the cursor look: C8 |
| WinKey 0x577850 (7) | two `FunctionDetour`s (`UiManager`, `KeyManager`) | FLvibed first only |
| HandleKey 0x576410 (6, key trace) | `FunctionDetour` (`KeyManager`) | FLvibed first only |
| PlayArch 0x428680 (5, voice-line subtitles) | `FunctionDetour` (`SoundApi::CreateSoundDetour`) | FLvibed first only |
| UiControlRemove 0x59DC00 (7) | `PatchAssembly` 0x59DC09 (`HudManager` `OnTerminate`), inside FLvibed's 32-byte signature | FLvibed first: fine (trampoline returns to +7, FLUF's jump at +9 runs after). FLUF first: signature mismatch -> refused, Extended HUD 2 control tracking off (fails closed). *[da1363f: +9 is wildcarded (HudFacility's jump): matched in either order]* |
| Evade update (Common.dll vtable slot 7, FLvibed's lazy guard) | `VTableHook` EvadeBehaviorHooks (Common+0x13F874..0x13F8DC, from DllMain) | the guard sees a non-Common pointer and refuses (logged; autopilot Evade off). *[da1363f: the guard leaves the slot to its owner (logged once, owner named); the autopilot uses its own dodges]* FLvibed never patches this vtable before FLUF's DllMain, so FLUF's throw cannot trigger |
| MainMenuButtonClick, FindControl, FmtStrFormat, ClearWaypoints, AddWaypoint, PlotBestPath, GamePaused, GetPlayerIObj, PlayUiSound, TextCtlSetProperty, Create*Control | FLUF calls them (MainMenuButtonClick also detoured) | FLvibed only calls these: unaffected |
| D3D8: rp8 IAT `Direct3DCreate8`, `IDirect3D8::CreateDevice`, device `Reset` / `Present` vtable entries | `FunctionDetour` on the d3d8.dll code of `Direct3DCreate8`, `CreateDevice` (vtable[15]), `Reset` (vtable[14]) | pointer swap -> FLUF-detoured code: chains either way |
| Content.dll IAT (server.dll imports), IServerImpl / IClient vtable slots | FLUF detours server.dll function code (e.g. `pub::SpaceObj::Create`) | different layers: chains |

Not in this table: FLUF's ~100 byte patches elsewhere in Freelancer.exe / Common.dll / Server.dll (QoL constants,
fixes) - none inside a `flexe.h` signature span; they change behaviour FLvibed reads at run time (e.g. max dock range
Common+0x13F48C, chat counts), covered by the "read at run time, never assume" rule (M4). Module hooks (outside core)
were scanned the same way: only `FLUF.CrashWalker` names `FrameUpdate` (a call).

### 5. Functional overlaps (no hook clash, behaviour to check)
- FLUF's ImGui cursor replaces the game's: FLvibed's pad cursor drives the game's own cursor state [assumed from the
  host's CursorUpdate / CursorShown use, not traced here]; whether ImGui's cursor follows it is unknown (C8).
  *[Answered from source in section 8 (da1363f): it follows - FLUF draws its arrow at `exe::MouseX` / `MouseY`.]*
- `ui_overhaul` replaces the equipment dealer with an ImGui window: FLvibed's controller-first dealer screens
  (`ui_style` 1) build on the vanilla dealer (`VtDealer`); with that module on, expect FLvibed's dealer layer not to
  see a vanilla dealer. `objective_tracker`, `smooth_strafing`, `combat_ai`, `qol_patcher` overlap FLvibed behaviours
  (starport.md row 12).
- `setSaveDirectoryRelativeToExecutable` redirects `GetUserDataPath` (detoured only when on): FLvibed already asks the
  game (C5).
- `dacom_fix` calls `SetDefaultDllDirectories` for the process: later `LoadLibrary` calls by bare name search only the
  application folder, System32 and the added paths. FLvibed's release build loads only system DLLs by name (not
  checked for every call: an in-game item).

### 6. Open questions
- Does Discovery's shipped FLUF build use the same `FunctionDetour` (very likely: the same SDK lineage since FLHookSDK)?
  Not disassembled.
- Does FLUF's ImGui input layer swallow keys FLvibed's `WinKey` hook needs while an ImGui window is open (it hooks
  `WinKey` too, and since 5 Oct has a key-blocking API)? *[Answered from the nightly's source ("FLUF in play" B, C):
  FLvibed's hook sits in front of FLUF's handler and sees every key; FLUF withholds keys only from the game, and only
  while `WantTextInput`; `SetKeyBlocked` acts on HandleKey commands (FLvibed's pad commands pass it).]*
- FLUF on top of HDE's exe without HDE's plugins: FLUF's hard-coded addresses vs HDE's 37 code edits (not checked).

### 8. The hook study and the co-residency design (25 Oct 2026, FLvibed da1363f)
Read from the clone (equal to Codeberg HEAD, checked through the API the same evening; no download): FLSDK
`include\Utils\Detour.hpp`, FLUF `Source\Client\Ui\UiManager.cpp`, `KeyManager.cpp`, `SoundApi.cpp`, `FlufClient.cpp`,
`Ui\ImGui\ImGuiInterface.cpp`, `Common\AI\Behaviors\EvadeBehaviorHooks.cpp`, `Ui\Vanilla\HudManager.cpp`, `Include\ImportFluf.hpp`,
`Common\FlufModule.hpp`; FLvibed's hosts.
- **No public hook / event API usable from a foreign DLL.** FLUF's extension point is a FLUF *module*: a C++ class
  derived from `FlufModule`, exported with `FLUF_API` and loaded by FLUF from `fluf\modules` (`FLUF.yml`), sharing FLUF's
  C++ ABI, STL and runtime (VS2026, the dynamic CRT). FLUF.dll's only C exports are `DummyFunction` and the modules'
  version functions. FLvibed (static CRT, no prerequisites, one DLL) cannot use it without becoming a FLUF module.
- **`FunctionDetour` restores and re-saves per call** (section 3): `Detour()` copies whatever is at the function to
  `data` each time, `CallOriginalFunc()` writes `data` back, calls, `Detour()`s again. So: (a) a jump written over
  FLUF's is erased by FLUF's first call; (b) a jump written before FLUF's becomes FLUF's `data` and survives forever;
  (c) FLUF's handler code (the jump's target) is never rewritten.
- **New finding - "FLvibed first" was only half working.** With FLvibed's jump inside FLUF's `data`, FLvibed's hook is
  called from `CallOriginalFunc`, so its return address is FLUF's code: the controller's WinKey hook counted every
  real key as injected (`InExeImage(_ReturnAddress())`: the autopilot key and the options page's keys dead) and the
  subtitles' `OnSoundStart` / voice lookups compare the caller with `exe::RetComm*` / `RetSceneSound` (comm and scene
  voice-line subtitles lost). FLUF's handlers jump to the game function's address (no call), so a hook at FLUF's
  handler keeps the game as the caller.
- **Design chosen (hk::Detour, `core\hook.cpp`):** follow a target's `E9` while it leads into restore-style code
  (`FLUF.dll` or a module importing it; tests register ranges) and hook there - FLUF's handler - by moving its first
  whole instructions (a small x86-32 length decoder: prefixes, ModRM / SIB / displacements, immediates; E8 / E9 rel32 and
  EB rel8 relocated; jcc / loop / jecxz, int3, far transfers, 0x67, unknown 0F refused) into the trampoline. A jump
  that leads elsewhere (another FLvibed part's stub, a plain plugin) is chained as before. FLvibed's jump targets a stub
  `jmp [ptr]` and the caller's "original" is a stub too, so `hk::AdoptRestoreStyle` (each host, once, after its first
  frame - every DLL is loaded by then) can move a hook that FLUF wrapped later: the old stub passes through to its old
  trampoline, the "original" stub leads into FLUF's handler. Same chain in both orders: FLvibed, FLUF's handler, the
  game. Rejected: re-writing FLvibed's jump after every FLUF call (a per-call fight over FLUF's bytes), finding FLUF's
  `FunctionDetour` objects to edit `data` (a heap search), patching every caller's call instruction (dozens of new
  signatures; function pointers missed).
- **Cursor:** `CursorRenderDetour` = ImGui `Over` layer, the original, `PollInput` (reads the game's `0x616840` / `0x616844`
  / `0x616850` = FLvibed's `exe::MouseX` / `MouseY` / `MouseButtons`, which the pad cursor drives through DirectInput /
  `BackgroundCursor`), then `RenderCursor` - the arrow named by the game's current cursor `0x616858` at ImGui's mouse
  position; CursorShown is not read. So the pad position reaches FLUF by itself; FLvibed's hidden cursor clears
  `exe::CurrentCursor` too (new signed symbol) around the call while its hook wraps FLUF's handler (vanilla reads it only
  after the CursorShown gate at 0x41F30A).
- **Evade:** FLUF's `EvadeBehaviorUpdate` re-implements the update and keeps the vanilla player-ship path without the
  threat check (`IsWithinSurfaceDistance` on a null threat = the crash FLvibed's guard exists for), and FLSDK's
  `VTableHook::Init` throws "Double hooking detected" on a vtable range patched before it. FLvibed therefore never
  touches a slot another module owns (logged once) and its autopilot uses its own dodges.
- **Open, in game only:** whether the decoder moves the start of every handler of the shipped FLUF build (each refusal
  is logged with the bytes); FLUF's ImGui text input vs FLvibed's raw keys (FLvibed now sees a key first: its bound keys
  would act while typing in FLUF's command palette); FLUF's own direct `CallOriginalFunc` calls (`SoundApi::
  CreateSoundSource`: FLUF's sounds) do not pass FLvibed's hook (not the game's lines).

### 9. History: FLUF stable 2.0.0 in play (25 Oct 2026) - not the target build
*History (26 Oct 2026): this subsection and section 7's run describe stable 2.0.0, the build tested first. The owner
chose the nightly (nightly-2026-10-16-03-03) as the target; its behaviour is "FLUF in play" above, and nothing here is a
verdict for it. Kept for the 2.0.0 differences (HandleKey overwrite, helper DLLs) and the run's evidence.*
Sources: the release FLUF stable 2.0.0 (tag `stable-2026-03-28-18-16`, `FLUF.tar.xz` 55,651,208 bytes, SHA-256
`bd5635f2bcaa625258d9a0f7867c3151c6f29017491d3c83524d7841d830b390`, downloaded by the main session on the owner's OK
into `Install\downloads\fluf\`, git-ignored, AGPL-3.0); its source at that tag read on Codeberg (`KeyManager.cpp/.hpp`,
`UiManager.cpp`) and at HEAD in the clone (`ImGuiInterface.cpp`, `CommandHandler.cpp`, `HudManager.cpp`, the user docs
`docs\content\user\*.mdx` - the release's docs.tar.gz holds the same pages, not downloaded); the generated `FLUF.yml`,
`fluf\config\ship_classes.yml`; FLUF's log (`SAVE\fluf.log`); our frames (`research\experience\runs\fluf_compat_2026-10-25\`).

**What 2.0.0 differs from the HEAD source in (read at the tag):** HandleKey is not a `FunctionDetour` but a 7-byte
overwrite `mov ecx, HandleKeyNaked; jmp ecx` (B9 imm32 FF E1) that never restores what it overwrote - its naked
handler replays the original first instructions itself and resumes at 0x576417. Loaded after FLvibed it erased
FLvibed's HandleKey jump and broke the HandleKey signature for every later reader (FLvibed's logic refused
`exe::HandleKey`: no pad command could be sent). The CursorUpdate handler is at FLUF.dll+0xC30D0, WinKey's at +0xC3150,
HandleKey's naked handler at +0x35F80. The ImGui cursor reads 0x616840/44 as in HEAD.

**The install as shipped:** no `dacom_fix.dll` in the 2.0.0 package (its docs still name it), so the dacom.ini
`dll_search_path` lines do nothing and `fluf/FLUF.dll` fails to load (FLSpew: "unable to load dll 'fluf/FLUF.dll',
ignoring") - its imports `FreelancerSDK.dll`, `freetype.dll`, `libcurl.dll` sit in `fluf\`. Working install for the
tests: those three DLLs copied beside Freelancer.exe as well (the loader's application folder). Needs the VS 2015-2022+
x86 runtime (MSVCP140, VCRUNTIME140, MSVCP140_ATOMIC_WAIT). FLUF's own `debugger.dll` module is in the default
`FLUF.yml`.

**First run and ever after (player-facing, no FLvibed involvement):**
- A modal "No ship classes" question (generate `fluf\config\ship_classes.yml`?) blocks the very first start until
  answered; FLUF writes `fluf\FLUF.yml` and `fluf\config\`.
- `logSinks` defaults to File + Console: every start opens a console window ("FLUF - Freelancer Unified Framework")
  that takes the foreground. Logs go to the save folder (`SAVE\fluf.log` here, flplusplus' redirect).
- Main menu: an "Open FLUF Config" ImGui button (bottom right) and FLUF's ImGui arrow instead of the game's.
- Two game commands are renamed and re-purposed (the INI name table 0x614DD8): USER_YES -> FLUF_OPEN_COMMAND_PALETTE,
  USER_NO -> FLUF_OPEN_EXTENDED_OPTIONS_MENU; unbound until the player adds them to keylist/keymap.ini. FLUF also
  takes USER_CANCEL (Esc closes its topmost escape-closeable window first) and USER_BUTTON0/1/2 (mouse buttons over one
  of its windows do not reach the game).
- The command palette (a text box): opening it hides the maneuver bar; while it has text focus FLUF's WinKey handler
  returns without calling the game, so **every key goes to the palette, the game sees none** (Esc included - the
  palette is not one of FLUF's escape-closeable windows; only its own key toggles it shut).
- Its Evade update (section 8), Screen Control (keeps running unfocused, Alt+Enter) in 2.0.0 as the `screen_control`
  module (not on by default), QoL constants and fixes (section 4's note).

**FLvibed against it - inputs and focus (both hook WinKey and HandleKey):**
- FLvibed's hooks sit in front of FLUF's handlers (either order): FLvibed sees raw keys and commands first. Its bound
  keys (the autopilot key K, the options page) act even while FLUF's palette has text focus (FLUF would have eaten
  them); keys FLvibed does not bind pass on to FLUF unchanged. FLvibed binds no USER_YES / USER_NO.
- The pad: FLvibed's commands go through HandleKey (now resolved through FLUF's naked handler); its menu cursor moves
  the game's cursor state, which FLUF's arrow follows; hidden on pad screens (C8).
- Nothing of FLUF steals the game window's focus except the console window at start (a background test turns it off).

**What our harness assumes that FLUF changes, and how each test step adapts:**
| Assumption | Under FLUF | Test step |
|---|---|---|
| the start reaches the main menu unattended | the first-run dialog blocks; the console opens | answer the dialog OK (what a player does); `logSinks: [File]` for background tests only (the console takes the user's screen; no FLvibed conflict hidden) |
| `exe::HandleKey` resolves; `tap` / pad commands go through | 2.0.0's overwrite broke it | fixed in FLvibed (B9 form recognised, hook at the naked handler) |
| Esc / USER_CANCEL reaches the game | FLUF closes its windows first; with the palette focused nothing reaches the game | close the palette with its own key (tap 19), check the frame |
| named game controls (`visible`, `click`) | FLUF hides ManeuverFrame while the palette is open | palette state judged from frames |
| a comm line arrives after the step starts | the mission's first comm can come during the load | the subtitle cursor is taken before the load |
| mission tutorial popups (vanilla M01a "Basics of Combat") | the same as without FLUF (pause the game) | closed natively in the autopilot step, both with and without FLUF |
| save paths | FLUF's `setSaveDirectoryRelativeToExecutable` off by default; flplusplus redirects as before | unchanged |

### 7. History: the in-game test plan and the run beside stable 2.0.0 (25 Oct 2026)
*History (26 Oct 2026): the plan below was written for 2.0.0 and run with it; the nightly's plan is "FLUF in play" D
(Phase A3, a fresh copy of `Install\instances\vanilla11`).*
- **Download:** `FLUF.tar.xz` from https://codeberg.org/TheStarport/FLUF/releases (stable 2.0.0, 55.7 MB, or nightly
  2026-10-16, 139.4 MB) into `research\evidence\fluf\pkg\` (untrusted: own folder, nothing run from there). The VS2026
  x86 runtime if not installed (a system install: UAC, the owner's call).
- **Instance:** a copy of the vanilla 1.1 instance (`Install\instances\vanilla11*`; HDE is excluded by FLUF's
  forbidden list), FLUF per its docs (`dacom_fix.dll` first, `dll_search_path` lines), core only (no modules) first.
- **Run A - FLvibed above `fluf/FLUF.dll`:** both logs; every FLvibed host hook fires (subtitles on a comm, pad
  steering, HUD 2, the bar's autopilot button, key trace), FLUF's ImGui overlay and command palette work, the pad
  cursor is visible; the evade guard logs its refusal. Expected: works.
- **Run B - FLvibed below FLUF:** since da1363f expected: every shared site logs "hooked at its handler", both run
  (was: FLvibed's FrameUpdate stops after the first frame). In both runs check the logs' `FLUF present` line, real keys
  counted as real (`uibtn`), comm subtitles (PlayArch's caller), the pad cursor hiding FLUF's arrow on a pad screen,
  and the evade guard's owner line.
- **Run C - modules:** `ui_overhaul` + the dealer on the pad (`ui_style` 1), `objective_tracker` beside HUD 2.
- Background, muted, `deploy.ps1 -Background` rules (`docs\TESTING.md`); evidence `research\experience\runs\fluf_<date>\`.
- **Run 25 Oct 2026 (runs A and B done, C open):** stable 2.0.0 on `Install\instances\vanilla11_fluf`, the default
  `FLUF.yml` (debugger.dll) minus the console sink, both orders against the same instance without FLUF
  (`research\experience\runs\fluf_compat_2026-10-25\tools\ff.ps1 -Order N|A|B`, `results_fluf_2026-10-25.tsv`, frames
  `fluf<order>_*.png`, `fluf_run.log`). Found and fixed in FLvibed: 2.0.0's HandleKey overwrite (section 9) killed
  every pad command (e2611cd). After it every checked item matches the no-FLUF run in both orders; conflicts recorded:
  FLUF's default debug window over the top-left HUD / strip, FLvibed's bound keys acting while the palette is focused;
  2.0.0 has no Evade hook. The verdict table: `docs\MOD_COMPATIBILITY.md` "In game beside FLUF stable 2.0.0".

### Sources (checked 25 Oct 2026; the issue tracker and HEAD 26 Oct 2026)
https://codeberg.org/TheStarport/FLUF ; https://codeberg.org/api/v1/repos/TheStarport/FLUF (/commits, /releases,
/tags, /issues) ; https://codeberg.org/TheStarport/FLUF/releases ; https://codeberg.org/api/v1/repos/TheStarport/FLSDK ;
https://api.github.com/repos/TheStarport/FLUF ; /TheStarport/FLHook ; /TheStarport/FLHookSDK ; /FLHDE/FLHook ;
https://the-starport.com/forums/docs/fluf ; http://adoxa.altervista.org/freelancer/ . Source files: the clone at
627ea2c (verified equal to upstream HEAD via the API) and FLSDK ae8c857.
