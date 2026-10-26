# BC46/FLSharp: the HDE plugin that moved from 0.8 to 0.9.2
Status: current
Checked against: FLvibed 77d8655 (26 Oct 2026, coordinator short form before the export: the watched commits since - d89f88c / 79e2bcc smart start + K&M goal panel, 7632ced job objective text, 77d8655 objective-named loot shot, 9ff7fe7 handoff - are autopilot / UI internals; no statement of this note changes; not re-read in depth); before: FLvibed 01fcfa9 (26 Oct 2026, the step 3 short form re-confirmed: the only watched commits since, 9327f86 / 01fcfa9, are doc and handoff edits recording what this note already says); before: FLvibed e5c02cc (26 Oct 2026, step 3 short form: 202 / 202 upstream, borderless auto stays out on HDE -> U4 (g) - first "What it means" line); before: FLvibed 855745b (25 Oct 2026 ~23:05, whole-set check-up: 17 watched-path commits 073a665..855745b - Extended HUD 2's host side (HOST-X d1d5bf4, STRIP 84492e9, CARD, Repose), the vanilla 1.1 support and signature re-cut (4042c7d..0d0bcb7), HUD 2 on every build (65cd6dd, 48c712a, c6158a4), the comm portrait 5aa8294, the FLUF co-residency da1363f / 5cd02b9; read against FLSharp's source: one more shared address, the ship dealer's patches beside the new readers - second bullet); before: 073a665 (25 Oct 2026 ~12:15, short form after the HUD 2 build: 7 watched-path commits abf84ac..073a665 in `controller/src/host` (uibtn.cpp: the HUD 2 hotspot box 08d450f, the ToolTip / NeuroNet window handling d0cf8d9 / 95dea02), `core/flexe.h` (+8 symbols, two shared with FLSharp) and `subtitles/src/impl/impl.cpp` (the HUD bus f8dc584); read against FLSharp's source: no clash found, four sweep items - second bullet); before: abf84ac (25 Oct 2026 ~07:00, short form after the UI build: 7 watched-path commits 3cfa88c..abf84ac in `controller/src/host` (uibtn.cpp: the NN_Preferences page button and page switch, the bar button, the raw-key WinKey hook, the real mouse; main.cpp: the CursorUpdate hook clearing the cursor-shown flag) and `core/flexe.h` (+24 symbols); nothing contradicted, one new coexistence question - second bullet); before: 3cfa88c (25 Oct 2026 ~03:30: the 24 Oct refactors and the UI design docs; nothing touched)
Dirty paths at check: none (25 Oct 2026 ~23:05); none (25 Oct 2026 ~12:15); none (25 Oct 2026 ~07:00); before: none at the start (24 Oct 2026, 1defa3f); at the end `controller/src/impl/autopilot_frame.cpp`, `controller/src/impl/autopilot_objective.cpp` (another session's uncommitted edits, appeared during the check; not judged here)
Watch: docs/HDE_UPSTREAM.md, docs/MOD_COMPATIBILITY.md, controller/src/host, core/game/flight.*, core/game/render.*, subtitles/src/impl/impl.cpp, core/flexe.h
Evidence: `research\repos\FLSharp\` (clone of https://github.com/BC46/FLSharp, MIT, 21 Oct 2026; `git pull` only when the user asks); the test install's and the upstream clone's `EXE\FLSharp.dll` / `FLSharp.ini`
Sources: [api] GitHub releases and commits; [file] the two FLSharp.dll / FLSharp.ini pairs in the test install and the upstream clone; [file] FLSharp sources (README, include/Freelancer.h, src/flight_controls.cpp, mouse.cpp, camera.cpp, maneuver_frame.cpp, utils.h, version_check.cpp and others)

## What it means for FLvibed

*Check-up 26 Oct 2026 (step 3 short form, HEAD e5c02cc; 17 watched-path commits 855745b..e5c02cc):* 202 / 202 on the
upstream exe (`research\evidence\discovery\sigcheck_e5c02cc.txt`). New since: FLvibed's own borderless window (90a8b7f /
38d411c: detours at the game's `SetupScreen` and window-style reads) - with `borderless` 1 (auto, the default) it stays
out wherever DxWrapper / d3d8to9 already provide the window, i.e. on both HDE installs, so FLSharp 0.9.x's borderless
mouse / cursor fixes keep meeting HDE's window, not FLvibed's; only `borderless` 2 (forced) puts FLvibed's window under
them - one more U4 item (g) for the second-install sweep. The dealer close (f52815b) reads the NeuroNet window manager,
not NN_ShipTrader's size FLSharp patches. FLUF's layer is inert on HDE (FLUF refuses to load there). Nothing contradicted.
- **It is the biggest plugin difference between the two compatibility targets.** HD Edition loads FLSharp from
  `dacom.ini` (line 40 of the test install). The test install ships **0.8.0** (13 Jul 2025, 17 features); the upstream
  HDE repository ships **0.9.2** plus a 22 Aug 2026 rebuild for a comm-text affiliation fix (43 features). `docs\HDE_UPSTREAM.md`
  has no FLSharp entry yet (IDEAS M1, C6).
- **The 0.9.x features land on code FLvibed touches:** mouse and cursor handling in borderless windowed mode (hooks
  at 0x41F30A and 0x420335, DirectInput Acquire at 0x41F7D3, byte patch at 0x41EA9B), the third-person camera motion
  decoupled from frame rate, an extra call of the maneuver layout 0x4D88F0 after undock, pilot names and affiliation in
  the comm caption, Target Status window fixes (IDEAS U4, U5, U9, S6).
  *Check-up 25 Oct 2026 (abf84ac):* the UI build put FLvibed onto the same screens - **a coexistence question for the
  second-install sweep** (U4, M7), three items: (1) FLvibed detours NN_Preferences' `PrefsInitElements` 0x4A9790,
  `PrefsMessage` 0x4AC0A0 and `PrefsShowPage` 0x4ABDF0 at their entries (dd5bce6; `controller\src\host\uibtn.cpp`) while
  FLSharp 0.9.2's `resolutions.cpp` patches call sites inside the same screen's code (0x4A9AAB, 0x4AC264, 0x4ACEE2,
  `NN_Preferences::SetResolution_Active_Hook` at 0x4AC4B0) - different bytes, one screen, never run together; (2) both
  touch the cursor-shown flag 0x6107DC (FLSharp's `SHOW_MOUSE_CURSOR` beside its `ShowCursor_Hook`; FLvibed's
  `CursorShown`, cleared around `CursorUpdate` while the pad drives, HostApi v20, 28dd009); (3) FLSharp's extra
  `Maneuvers::Update` 0x4D88F0 call after undock now runs FLvibed's chained `ManeuverLayout` observer, which re-places the
  fifth bar button (U9: harmless by design, untested on the upstream install). Of FLSharp's 193 addresses, 0x6107DC is a
  new shared one (K5).
  *Check-up 25 Oct 2026 (073a665), HUD 2:* HUD 2 hides the game's flight HUD per root control by the state byte +0x6C
  (`controller\src\impl\hudvis.*`: roots found once, cached, re-hidden only where the game re-shows them) and adds a
  host-created top-level hotspot box (`controller\src\host\uibtn.cpp CreateHotspots`: the game's icon-button factory, a
  private copy of the icon button's vtable with FLvibed's message handler). Read against FLSharp 0.9.2's source, **no
  clash on paper; four items for the second-install sweep** (U4, M7): (a) `maneuver_frame_fix` (SetVisibility call
  0x4D8592 + `Maneuvers::Update`) re-shows the maneuver bar after undock - the game re-shows that root every frame anyway
  and HUD 2 re-hides it before each UI pass (U9); (b) `sub_target_fix` / `formation_list_fix` test the HUD_Target
  buttons' own visible bits (`FlUiElement::IsVisible` reads +0x6C of the element only), HUD 2 hides the panel's root, so
  the JFLP hotkeys keep working; (c) `target_trade_blink_fix` renders the Request-to-Trade button by hand inside
  HUD_Target's draw (0x4E1C07) - not reached while the root is hidden, except the one frame per new target HUD 2 runs the
  panel shown for its 3D view (MP only); (d) the hotspot box is a top-level control with an unknown vtable for any
  plugin that walks the tree (FLSharp does not). Two more shared addresses (K5): HUD 2's `ObjectUnknown` 0x4D4C70 /
  `ObjectUnknownIds` 0x4D4D50 are FLSharp's `IsSimpleUnvisited` / `GetIdsForUnvisitedSimple`; FLSharp's
  `waypoint_name_fixes` (on in both installs) hooks their call sites, FLvibed calls them directly and so names waypoints
  itself ("Waypoint", f5f07fe) - the same answer by duplication. HUDless (loaded by both installs; the DLL differs between
  them at the same size) acts on the same HUD as HUD 2 - its toggle belongs in the same sweep.
  *Check-up 25 Oct 2026 (855745b), Extended HUD 2, vanilla, FLUF:* 36 more `flexe.h` symbols (195), read against
  FLSharp's source (`research\repos\FLSharp` f58ed01): (1) **one more shared address** - `CurrentCursor` 0x616858 (da1363f;
  FLvibed clears it around FLUF's cursor handler only, i.e. never on HDE, where FLUF refuses to load) is FLSharp's
  `CURRENT_CURSOR` (`cursor_colors.cpp`, `more_cursor_colors`: upstream FLSharp.ini only, not in 0.7.1's), which swaps it
  for its own aim cursors - no overlap in practice, one more K5 row; (2) the ship dealer: FLSharp's
  `ExpandNNShipTraderObjMemory` grows NN_ShipTrader (patch 0x4B9739 inside the factory - d1d5bf4 wildcarded it in
  `VtShipTrader`'s signature) and `print_rep_requirements` (on in both installs) patches 0x4B9010-0x4B9017 / 0x4B9462 /
  0x4B9545; Extended HUD 2's dealer readers (`ShipTraderStatus` 0x4B8F23, `ShipTraderLevel` 0x4B8F55, `ShipTraderAfford`
  0x4B904F) sit outside those bytes and read fixed offsets below FLSharp's appended field - no clash on paper, and
  verified on 0.7.1, which loads the same feature; whether the strip shows FLSharp's reputation refusal sensibly
  (it words refusal 1, the level, only) is a sweep item (U4); (3) the UI generation counter detours `UiControlAdd` /
  `UiControlRemove` (d1d5bf4) - FLSharp touches neither; (4) flplusplus (also in HDE's `dacom.ini`) identifies the ship
  dealer by the same vtable 0x5D593C FLvibed reads (`shippreviewscroll.cpp`) - read only on both sides. The static
  check: 195 / 195 on the upstream exe (`research\evidence\discovery\sigcheck_855745b.txt`; da1363f also ran `sigtool
  check --scan --shift 16 --hooked` there). The in-process check with FLSharp 0.9.2 loaded is still open (C6, M7).
- **Already active in the test install (0.8.0):** rotation lock and auto-level reset from the behaviour manager at
  every launch to space (hook of the call at 0x54B8B9; globals 0x678E40+0x44 and 0x612700), client-server update
  cadence (40 ms, 750 ms in lanes; 0x541602), the waypoint check call at 0x4F4141 (FLUF patches the same site), 127
  resolutions with the main monitor as default, the post-game deadlock fix.
- **Hooking style is orthogonal to FLvibed's:** FLSharp rewrites call sites (`SetRelPointer` at call+1), patches bytes
  and uses a few trampolines; it does not detour FLvibed's function entries. Of its 193 named addresses, 10 are in
  `flexe.h`, all reads or calls of shared things (player object, cutscene flag, mouse position, cursor, docking camera,
  window handle, system id, ship id), plus the call of 0x4D88F0 (IDEAS K5).
- **Version detection:** it logs to Spew when Common.dll / Server.dll are 1.0 (it patches DACOM's version getter to read
  the product build number); FLvibed's own startup version line (IDEAS C3) would sit next to it.

## Findings

### Identity and history
- Author BC46 (also HD Edition's). Repository created 24 Dec 2023, MIT, 20 releases from v0.4 (Feb 2024) to v0.9.2
  (19 Aug 2026); last commit 14 Oct 2026 ("Change FL resource handle type", documentation). Requires the 1.1 patch.
- Build: NMAKE, Visual Studio 2017+. About 40 source files under `src/` and `include/`, one file per fix; every
  address a plain constant; `FLSharp.ini` switches every feature (missing file or key = enabled).
- Credits: adoxa (plugin sources, weapon animation research), Aingar (client-server update improvements and code from
  Discovery's client hook, the camera acceleration fix later taken into FLUF), FLHook SDK, Schmackbolzen, Squaresome
  (the light limit), Venemon (multi-barrel launcher sound, weapon animations).

### Versions in the two HDE targets
| | Test install (HDE 0.7.1) | Upstream HDE clone (7c6186e, 25 Aug 2026) |
|---|---|---|
| `EXE\FLSharp.dll` | 0.8.0, 413,184 B, 13 Jul 2025, SHA-256 0a6d4b68... | 0.9.2 (product 1.0.0.0), 416,256 B, SHA-256 4b982c7e... |
| HDE commits | "Update FL# to v0.8" (14 Jul 2025) | v0.9 pre-release (Dec 2025), v0.9 (27 Feb 2026), v0.9.1 (1 May 2026), v0.9.2 (19 Aug 2026), rebuild 22 Aug 2026 "Fix affiliation of story mission NPCs being shown in comm text" |
| `FLSharp.ini` features | 17 | 43 (26 new) |
| HDE CHANGELOG | credits v0.8 for dynamic-base infocards, the save-file startup crash fix, the alchemy crash fix, waypoint naming | not in the 0.7.1 changelog |

### Features by release (0.8 -> 0.9.2)
- **0.8 (Jul 2025):** dynamic solar infocards; startup crash on malformed saves; alchemy.dll crash at 0x701b; waypoint
  "Unknown Object" and Current Information fixes.
- **0.8.1 (Nov 2025):** test sounds no longer required in `interface_sounds.ini`; Spew logging incl. the 1.0 DLL
  detection; client-to-server update logic revised; waypoint name fixes as a separate option; fallback infocard by
  archetype; crash selecting malformed saves in Load Game; `fc_uk_grp` blank faction; Server Filter crash and game-speed
  bugs; crash on a non-existing DLL in freelancer.ini.
- **0.9 (Feb 2026):** shield capacity shown net of `offline_threshold`; dealer menus not opening on a double click and
  dealer crashes; no kick for re-buying the same ship; formation with hostile group members, group members not shown
  hostile; custom cursor colours for group members and trade requests; random-mission waypoint infocards; ship dealer
  reputation requirement.
- **0.9.1 (Apr 2026):** the quit-message loop fix; alchemy crash at 0x77cb; "GROUP MEMBER" attitude; base info spacing;
  in-game cursor hidden outside the window; Windows cursor flicker at the borders in borderless windowed; long pilot
  names in Current Information, comm text and the cargo dialog; cursor colours over background objects; Closest Enemy
  vs hostile group members.
- **0.9.2 (Aug 2026):** mouse warp to the window centre in (borderless) windowed mode; initial cursor position in
  fullscreen; camera stutter on switching to third person; third-person camera motion decoupled from frame rate;
  maneuver frame flicker after undock; player ship light sources (`lightsource_cone`); light limit raised to 10;
  equipment stat flicker in the inventory; Sub-Target and Formation List hotkeys vs the Target Status window; "Request
  to Trade" blink; dealer crash fix compatible with a location-frame hex edit; a second deadlock fix.

### How it hooks (from `include/utils.h` and the fixes)
- `Patch<T>(addr, value)` byte/word patches; `Hook(location, func, len, jmp)` writes `E8` (call) or `E9` (jmp) plus
  NOPs; `Trampoline` copies the overwritten bytes into a VirtualAlloc'd gateway; `SetRelPointer(call+1, func)`
  rewrites the rel32 of an existing call (the common pattern: `0x54B8B9`, `0x4F4141`, `0x541602`, `0x57432A`,
  `0x4D8592`).
- Flight controls fix: at the post-init dealloc call (0x54B8B9) it copies `rotationLock` and `physicsInfo->autoLevel`
  from the player's `IBehaviorManager` into the globals 0x678E40+0x44 (rotation lock) and 0x612700 (auto-level);
  defaults read from Common.dll +0x7249A / +0x86542.
- Mouse: `ShowMouseCursor_Hook` at 0x41F30A hides the in-game cursor when outside the window (reads MouseX/Y
  0x616840/44 and the window size 0x679BC8/CC); `ShowCursor_Hook` at 0x420335 (19 bytes) forces the Windows cursor
  only outside the client area and clamps MouseX/Y; byte patch 0x41EA9B skips the bottom-edge clamp; `Acquire_Hook`
  at 0x41F7D3 (with 0x41F7D1 patched to load `ecx`) restores the cursor position after DirectInput Acquire in
  windowed mode; fullscreen flag at 0x679BE5 bit 0; the startup-screen object 0x67C3BC marks the first menu visit.
- Camera: hooks the current camera's `MainUpdate` (vtable +0x6C) and the third-person camera's init to feed the
  target's velocity (lane speed from `CShip::get_tradelane_speed` when in a lane) and correct the offset; uses
  `IsPlayerInCutscene` 0x41A3E0 and its own delta time; camera objects turret 0x678F60, cinematics 0x679634, active
  camera pointer 0x6164DC.
- Maneuver frame: `SetVisibility` call at 0x4D8592 redirected; after the original it calls `Maneuvers::Update`
  0x4D88F0 (the function FLvibed names `ManeuverLayout`).
- Other addresses it names: `PLAYER_SYSTEM` 0x673354, resources handle 0x67ECA8, HWND 0x67ECA0, string buffers
  0x66DC60 / 0x66FC60 (length 0x6119F8), time globals 0x667D38 / 0x667D14 / 0x667D20, show-cursor flag 0x6107DC,
  waypoint watcher 0x674BC8, NN_Preferences layout (resolution arrays at +0x8CC..+0x954), HUD_Target button
  pointers (+0x494..+0x618), NN_ShipTrader fields (+0x3CC..+0x448).
- Addresses shared with `flexe.h`: 0x41A3E0 (IsPlayerInCutscene, called), 0x4D88F0 (ManeuverLayout, called),
  0x54BAF0 (GetPlayerIObj), 0x616840 / 0x616844 (MouseX/Y, read and written by the cursor fixes), 0x616858
  (CursorName, cursor colours), 0x673354 (PlayerSystem), 0x673378 (PlayerShipId), 0x679634 (CamDocking), 0x67ECA0
  (MainHwnd).
