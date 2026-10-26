# Freelancer Decomp (FreelancerDecomp/Freelancer-Decomp): a full C++ rebuild of the 1.1 client
Status: current
Checked against: FLvibed 77d8655 (26 Oct 2026, coordinator short form before the export: the watched commits since - d89f88c / 79e2bcc smart start + K&M goal panel, 7632ced job objective text, 77d8655 objective-named loot shot, 9ff7fe7 handoff - are autopilot / UI internals; no statement of this note changes; not re-read in depth); before: FLvibed e5c02cc (26 Oct 2026, step 3 short form: 202 symbols, 79 behind - first "What it means" line); before: FLvibed 855745b (25 Oct 2026 ~23:05, whole-set check-up: 31 watched-path commits 073a665..855745b - Extended HUD 2's readers and FREELANCER_RE sections, HUD 2's panel views and objective copy, the subtitles' placement fixes, the FLUF co-residency; 36 new symbols (195), all found in game - first bullet); before: 073a665 (25 Oct 2026 ~12:15, short form after the HUD 2 build: 10 watched-path commits abf84ac..073a665 - `core/flexe.h` +8 symbols (e008407, d0cf8d9, 08d450f, f5f07fe), `docs/FREELANCER_RE.md` "UI" (the flight HUD's roots and hiding them, 1ae14b0; the docs of 5.8 / 5.9), the subtitles display on the HUD bus (f8dc584, 15f5ffc, c2a27eb); no conclusion contradicted; K1 36 behind, two reading leads - first bullet); before: abf84ac (25 Oct 2026 ~07:00, short form after the UI build: 10 watched-path commits 3cfa88c..abf84ac - `core/flexe.h` +24 signed symbols (dd5bce6, 28dd009, 52b6871, 13c41f0, c476760, f1e0ed9), `docs/FREELANCER_RE.md` +264 lines, `docs/kb` hud-controls.md / menu-screens.md, the subtitles display / impl refactor (gamefont moved to core, e42c1f1); no conclusion contradicted; the symbol count moves K1 - see the first bullet); before: 3cfa88c (25 Oct 2026 ~03:30: the 24 Oct refactors and the UI design docs; nothing touched)
Dirty paths at check: none at the start (25 Oct 2026 ~23:05; during the round another session's uncommitted `controller/src/impl/padbases.cpp`, `controller/src/impl/promptbar.cpp`, `docs/UI_HUD2_EXTENDED.md` appeared - not judged); none (25 Oct 2026 ~12:15); none (25 Oct 2026 ~07:00); before: none at the start (24 Oct 2026, 1defa3f); at the end `controller/src/impl/autopilot_frame.cpp`, `controller/src/impl/autopilot_objective.cpp` (another session's uncommitted edits, appeared during the check; not judged here)
Watch: core/flexe.h, core/hook.*, docs/FREELANCER_RE.md, subtitles/src/impl/impl.cpp, subtitles/src/impl/display.cpp, controller/src/impl/autopilot_*.cpp, core/game/route.cpp, core/game/avoid.cpp, docs/kb
Evidence: research\evidence\decomp\ (flexe_xref.txt: 109 of 123 symbols mapped to decomp names; decomp_xref.js regenerates it from a clone)
Sources: https://github.com/FreelancerDecomp/Freelancer-Decomp read from a shallow clone in the session scratchpad (not kept: IDEAS X1); its docs/re, docs/ini and src; GitHub API for commits and pull requests

## What it means for FLvibed

*Check-up 26 Oct 2026 (step 3 short form, HEAD e5c02cc; 13 watched-path commits 855745b..e5c02cc):* **202 symbols (+7;
the cross-reference now 79 behind, K1)** - `NNManager`, `NNWindowClose`, `TransitionRunning` (the dealer close f52815b:
the NeuroNet window manager drops a close during its ~0.5 s slide) and `SetupScreen`, `WindowStyle`, `WindowedStyle`,
`DisplaySettings` (FLvibed's borderless window, 90a8b7f; `docs\kb\display.md` "Window mode path"); all found in game, not
from the decomp. `FREELANCER_RE.md` gained the window-mode path (38d411c), the game's popups (0e41948) and the NeuroNet close
(f52815b). Nothing contradicted.
- **A map of the binary FLvibed hooks**, checkable in minutes because nearly every client function carries its retail
  address: the symbol cross-reference names 109 of 123 `flexe.h` symbols (IDEAS K1); the mission opcode and INI
  references feed the knowledge base (K2, K3); the typed headers can replace SDK folklore (K7).
  *Check-up 25 Oct 2026 (abf84ac):* the UI build added **24 symbols to `flexe.h` (127 -> 151)**: the control factories
  (`CreateIconButton` 0x5A0C90, `CreateTextControl` 0x5A0E30, `CreateWireControl` 0x5A0D00), NN_Preferences
  (`PrefsInitElements` 0x4A9790, `PrefsMessage` 0x4AC0A0, `PrefsShowPage` 0x4ABDF0, `VtPrefs`), NN_Pause, HUD_Maneuvers
  (`VtManeuvers`, `ManeuversMessage` 0x4D8DA0, `ManeuverFrameSize`, `ManeuverPitch`), the TextControl / TextButton /
  IconButton / ToggleButtonControl property, click and activate paths, the UI colour bytes, the cursor-shown flag 0x6107DC,
  `RemoveWaypoint` 0x4C46E0, `PendingLoadId`. All were found in game by the build (no commit of the build cites the decomp;
  `FREELANCER_RE.md` +264 lines, `docs\kb\hud-controls.md` / `menu-screens.md`). The cross-reference (109 of 123) is 28
  symbols behind (K1); the decomp's `nn_*.cpp` / `wincontrol_base.cpp` (U7) are where their names are, and the native row
  controls the build left as replicas (ToggleButtonControl / ScrollControl, `UI_BUILD.md` 5) are the next reading.
  *Check-up 25 Oct 2026 (073a665):* HUD 2 added **8 more (151 -> 159; the cross-reference now 36 behind, K1)**: the
  weapon-group manager and its current group (0x674C0C, 0x4F6170), the mouse wheel (0x616848), the arrival line
  `NN_ArrivalText` (0x66D398) and its house table (0x45B1A0), the target panel's 3D view draw (0x4E3C90), the "Unknown
  ..." test and its ids (0x4D4C70 / 0x4D4D50, also FLSharp's: `flsharp.md`); `FREELANCER_RE.md` "UI" gained the flight
  HUD's roots and how their state byte hides them. Again all found in game, none from the decomp. Two items HUD 2 left
  open are decomp reading first (U7): **the NeuroNet's news flash state** (HUD 2's news dot has no data; no blink field
  found among the box's buttons at +0x370..+0x3A4) and the target bracket's label / bars drawn inside
  `Targetable_Objects`' own draw (vt 0x5D9454) - the decomp's `hud_*` sources would say whether a field or a hook is
  needed (`UI_HUD2.md` 5.9).
  *Check-up 25 Oct 2026 (855745b):* **195 symbols (+36; the cross-reference now 72 behind, K1)** - Extended HUD 2's
  window vtables (13 NeuroNet / base windows, the ToolTip), `UiControlAdd` / `UiControlRemove`, `WindowBaseDraw`, the
  pose setters (`ControlSetPosition` / `ControlSetOrientation`, inventory and nav map position overrides), the ship
  dealer's refusal / level / afford reads, the price marks, HUD 2's panel-view routines (`ViewPrepare`,
  `ViewSetViewport`, `ViewDrawObject`, `RenderSetCamera`, `RenderSetViewport`, `RenderGetInt`; 65cd6dd), the client's
  objective copy `ClientObjective` 0x4C4FF0 (c6158a4) and FLUF's `CurrentCursor` 0x616858. Every one found in game (or,
  for the panel views, in HDE's HudStatus.dll code), none from the decomp; `FREELANCER_RE.md` gained "Extended HUD 2"
  (window art and events, the readers, the pose setters). The news flash state (U7) is still open. The decomp's
  `content_mission_opcodes` reference (K2) and dock code (A11) remain unread; the autopilot had no change since 34ba88f.
- **Native behaviour to read before changing autopilot code**: goto, dock, obstacle avoidance, dock queues, best path,
  guided missiles (IDEAS A11, A14). This matters now: the dock / travel / avoidance code is being changed in the tree.
  **Proven on 22 Oct 2026** (section "Used 22 Oct 2026" below): read file by file through the GitHub API, it named the
  airlock / letterbox lockout in minutes (`FREELANCER_RE.md` "Control exclusion", 36c8ffc). Since the autopilot now
  hands docks over to the game's own dock maneuver (636f9ef, ee2579a, GameDocksUs), `common_dockbehavior.cpp` and
  `DockQueues.cpp` describe code paths FLvibed runs; ask the decomp first for any unexplained native behaviour.
  *Check-up 24 Oct 2026 (1defa3f):* two current open problems are exactly that kind - moving-ship docks that stall
  after the grant (M04 Benford, M06 Hood; `dock_regain` written blind, IDEAS A24 / A11) and the mission opcodes whose
  fail conditions sessions 8-11 learned one failed run at a time (`Act_NagDistTowards`, `Cnd_DistShip` leashes; IDEAS
  K2, A21). Neither was read in the decomp.
  *648d6bd (24 Oct 2026):* `dock_regain` now rescues the Benford stalls by handing the grant to the game's own dock
  (5843e65), so the native dock finishes those docks; why the grant is not followed is still unexplained (A11, A24). The
  Holstein "no way" was `Act_LockDock` (learned from logs, b22ab14) - another opcode the decomp would have named first.
- **Subtitles**: the comm message layout (sender, receiver, caption IDS, permutation IDS) and the letterbox maths
  (IDEAS S1 done, S2, S3, S4 mostly done). **Controller / display**: input, keymap, command dispatch, the WinControl
  and NeuroNet map, the retail 4:3 assumptions (U7, V1).
- **Limits**: vanilla 1.1 only (not an HDE test target), server side partly simplified, per-function status tracker
  not public, AI-generated under the owner's fidelity ledger; a fact without an address is a lead to verify in game.
  Licence: Unlicense for their text, derived from Microsoft's binary: facts and names are free, verbatim code is the
  user's call (IDEAS P4).

## Findings (research note of 21 Oct 2026)

Repository state: commit of 20 Oct 2026, 15 commits. The symbol cross-reference is `research\evidence\decomp\flexe_xref.txt`.

## What it is

- A **from-source C++ reimplementation of the whole Freelancer 1.1 client and every engine DLL**: `freelancer.exe`,
  Common, Server, Content, rpclocal, RP8 (plus a port-only D3D9 renderer RP9), Thorn, alchemy, Shading, RendComp,
  EngBase, SoundManager/Streamer, movie, DALib, dacom, FLServer, the resource DLLs. No retail binary is loaded
  except `EBUEula.dll` and `gundll.dll`. Retail `DATA` is required (vanilla; HD Edition data is not supported yet).
- Public since 17 Oct 2026, owner Giperion (Discord `@Giperion`), **The Unlicense** (public domain for their code;
  game data and retail files stay Microsoft's). About 760,000 lines of C++, Visual Studio 2022 / Win32, DirectX 8.1
  SDK. 41 stars, 13 forks, 5 pull requests in four days (SomeeName: HUD colours, "ships flying through
  asteroids", HUD name formatting, build fixes; Maclay74: replace the 19 `__asm` blocks).
- **How it was made:** Ghidra listings of every module plus a private "decomp-tracker" MCP server, worked by AI
  agents (source tags such as `claude:common-dockmove`, `w5-server-06cf`), curated by the owner with a strict
  fidelity ledger: `docs/re/deviations.md` (every known difference from retail, with the listing address and the
  removal condition), `docs/re/substitutions.md` (how to recognise code that fakes retail behaviour),
  `docs/re/refactoring.md`. The listings (`EXE/Disasm`) and the per-function status tracker are **not published**,
  so the confidence of an individual function is invisible from outside.
- **Documentation worth reading on its own** (`docs/`): `re/freelancer.md` (client architecture), `re/common.md`,
  `re/common_physys.md`, `re/content.md` (controllers, Server -> Content messages, the 65-TU map of Content.dll),
  `re/content_mission_script.md` + `re/content_mission_opcodes.md` (all 44 `Cnd_*` and 85 `Act_*` with argument
  semantics), `re/server.md`, `re/cross_dll_protocol.md`, `re/audio.md`, `re/movie.md`, `hd_edition.md` (a catalogue
  of every HDE byte patch and plugin, planned as C++ switches, unimplemented), and `ini/*.md`: every INI section and
  key the code reads, with type, default and the reading source file.

## How far it is, and what to trust

| Module | State (from the sources and their docs) | Trust for FLvibed |
|---|---|---|
| `freelancer.exe` (`src/freelancer`, 491 files) | line-traced against the listing; nearly every function carries its retail address (`FUN_004ce000`) and often the address ranges of each block | high as a *map*: 109 of FLvibed's 123 `core\flexe.h` symbols are found by address (the 14 missing are mid-function return addresses, two globals and three layout constants); every spot check agreed with `FREELANCER_RE.md` (AppActive 0x610865, camera 0x679738, system id 0x673354, MouseSteer 0x4C7CB0, FrameUpdate, GetPlayerIObj) |
| `Common.dll` | ported; 31 `FULLPORT-DEFERRED` markers (mostly IVP physics), containers modern | good for AI behaviours, archetypes, equipment, `pub::` contract; physics numbers only after checking |
| `Server.dll` | "many `pub::SpaceObj` / `pub::Player` functions are simplified" (their own words) | read for structure, verify numbers |
| `Content.dll` | full translation-unit map, no deferred markers, statuses tracker-only | good for mission scripting and populators |
| HD Edition | vanilla 1.1 only; `jflp.dll` / `HudShift.dll` crash it; HDE data needs unimplemented plugin features (fonts.ini) | not a test target for FLvibed |

Rule of use: a decomp fact that carries a retail address is checkable in minutes against `research\disasm`
(`tools\dis.ps1`); one without an address is a lead to verify in game, as the pre-change checklist requires anyway.
Copying their *code* into a shipped mod is a licence question for the user (their text is public domain, but it is
derived from Microsoft's binary); *facts, names, layouts and semantics* are the same kind of material FLvibed already
takes from FLUF, FLHook and adoxa.

## Actionable now (in the order of the queue on 21 Oct 2026; statuses live in IDEAS.md)

1. **Speaker-colour bug (HANDOFF top of the queue).** *Status: done by the working session the same day (9afe60c
   "Trent always amber", 9009dc1, 683707a); the caption IDS and permutation IDS points remain open (IDEAS S2, S3).* `src/freelancer/Client/hud_comm.cpp` / `hud_comm.h` is the
   comm window FLvibed hooks at 0x4CE000 (`HUD_Receiver::Dispatch(FCommSlot*, FCommMessage*)`). The message layout
   is confirmed with `static_assert`s: `+0x00 Sender`, `+0x04 Receiver`, `+0x08 Costume (0x34)`, `+0x3C VoiceId`,
   **`+0x40 CaptionIds`** (the IDS of the speaker-name line the game itself shows), `+0x44 TextIds[16]`, `+0x84
   TextCount`, **`+0x88 PermutationIds`** (an IDS of an order string such as "2, 0, 1"; 0 = in order), `+0x8C
   Attenuation`, `+0x90 Linger`, `+0x94 Result`. Dispatch itself tests `IsPlayerObjRwId(Msg->Sender)`.
   - Fix lead: in `subtitles\src\impl\impl.cpp OnComm`, `c.from == PlayerShip()` must map to `Relation::Player`
     before the attitude test (Trent's own comm lines currently fall through to Ally).
   - `CaptionIds` is a game-native speaker name FLvibed's `CommInfo` does not carry yet (it falls back to the
     sender's faction); the retail caption is built from the pilot name, the group name, or the object's name
     (`hud_comm.cpp` around line 1178).
   - Check that chunked chatter honours `PermutationIds` (the game may play the pieces out of order; `JoinPieces`
     joins them in record order).
2. **Letterbox placement.** *Status: mostly done (906c8ef "letterbox read from the game's own math"); the slide
   itself remains to check (IDEAS S4).* `space_letterbox.cpp` has the exact retail bar maths: bars converge on aspect 1.85,
   `bars = floor((H - W / ((1.85 - W/H) * amount + W/H)) * 0.5 + 0.5)`, amount animated at 3.0 per second, one shared
   "last camera" slot for the space and room cameras, `IsLetterboxVisible` = amount > 0 (FLvibed's
   `IsPlayerInCutscene` 0x41A3E0, `LetterboxAmount` 0x6166B0, `CinematicAspect` 0x5C9020 all match). With the
   formula the subtitle panel can sit on the bar during the slide, not only at rest (`DISPLAY_PLAN.md` letterbox
   modes).
3. **Name the symbol table.** `research\decomp\flexe_xref.txt` gives, per `flexe.h` symbol, the decomp file, line
   and function name: `UiRender` = `Win3DDrawAll`, `HitTest` = the Win3D pick, `FindVoice` = `LookupClientVoice`,
   `HudDrawPart` = `NnMontageDrawTileAt`, `ManeuverExecute` = `RequestManeuver`, `UseItem` = `IObjAffect` slot 22,
   `ObjectAction` = the 0x28-byte command record router, `PlotBestPath` = `RequestBestPathRetail`, etc. Use it to
   annotate `flexe.h` and `FREELANCER_RE.md`, and grep the clone for callers and callees instead of reading
   disassembly first.
4. **Knowledge base fill.** `docs/re/content_mission_opcodes.md` has the argument semantics of every mission opcode
   (`Act_NagDistLeaving`, `Act_PlayerCanTradelane` ring list, `Act_RandomPopSphere` where `on` *removes* the sphere,
   `Cnd_DistVec` tick modes); `content_mission_script.md` the chapter numbers, completion codes and the money table
   (3500 / 5200 / 7000 / 10000 / 16000 / 36000 / 30000, the same values FLvibed reads at run time);
   `docs/ini/systems-and-bases.md` the shortest-path tables (`systems_shortest_path.ini`, `shortest_legal_path.ini`,
   `shortest_illegal_path.ini`, generated by the game when absent, locked gates skipped). Feed `docs\kb`
   (`msn_triggers.tsv` semantics, `navigation-and-hazards.md`) and `tools\kbgen.ps1 -Check` (which keys the code
   really reads, with defaults).
5. **Autopilot: read the native behaviour before changing route or dock code.** Common: `common_gotobehavior.cpp`
   (89 KB), `common_dockbehavior.cpp` (109 KB: the dock sequence, the coast distance from the engine's `get_speed`,
   cruise lock), `common_obstacleavoidance.cpp` (94 KB), `common_pathfind.cpp`, `common_laneaction.cpp`,
   `common_itlbehavior.cpp`, `common_cruisebehavior.cpp`, `common_behaviormanager.cpp` (113 KB). Content:
   `DockQueues.cpp` (100 KB: the dock queue the harness saw at Leeds, berths vs moors), `SystemConnections.cpp` (the
   server side of `RequestBestPath`), `GotoGuides.cpp` / `Guides.cpp` (`[MetaBehavior]` goto_guide / dock_guide),
   `SpacePaths.cpp`, `TradelaneRequests.cpp`. Server: `server_guided.cpp` (only seeker type 2 tracks; arming creates a
   detonation phantom of radius sqrt(DetonationDistSq); lifetime) for the missile run-in question. Open HANDOFF items
   this touches: the Leeds ring dock 5 s over the prediction, the missile run-in, battery design.
6. **Controller module.** `input_manager.cpp` (DirectInput mouse, the axis scale `width * 0.00125 * sensitivity`
   FLvibed already uses, buffered vs immediate, double-click rule), `menu_keymap.cpp` (the KeyMap command list
   `DAT_0067c258`, `IsCommandHeld`), `hud_command_dispatch.cpp` (the command record `ObjectAction` routes; handlers
   of one id run in reverse registration order and the first `true` eats the event), `player_behavior.cpp`
   (mouse-flight hold timer 0.25 s, engaged flag, throttle global, strafe burst, auto-level / auto-avoidance
   setters). For the controller-first UI (`controller\docs\TASK_GLYPHS_HUD.md`): the `nn_*.cpp` screens,
   `wincontrol_base.cpp` and `win3d.cpp` describe the ~98 WinControl classes and the NeuroNet message ids FLvibed
   drives natively (0x26 button click, `SetProperty` ids), i.e. the foundation for pad-native screens over the
   game's own controls.
7. **Display matrix (4:3 / 21:9 / fullscreen).** `docs/re/freelancer.md` "Widescreen mode" and `deviations.md`
   WIDESCREEN list every retail 4:3 assumption with addresses (the five-entry resolution table at 0x4A982F, the 4:3
   device filter `FUN_004af120`, the camera half-angle functions `FUN_0040fd60` / `FUN_0040f8c0` / `FUN_00556ce0` /
   `FUN_0041a140`, font selection keyed by surface width); `hud_widescreen.cpp` shows HUD windows moved by shifting
   the Win3D camera instead of patching ~385 coordinates (HudShift). Background for `fl::WorldToScreen`, the HUD
   position constants and the unverified display matrix.
8. **Keep it.** *Status: declined for now (user, 21 Oct 2026: pure research session, no clone in the project; IDEAS
   X1).* The cross-reference script is kept as `research\evidence\decomp\decomp_xref.js` and can be re-run against a
   fresh shallow clone when `flexe.h` grows.

## Theoretical, later

- **Running FLvibed on the decomp build: no.** FLvibed verifies retail v1.1 bytes by signature; the decomp is a
  relinked MSVC v143 binary, so nothing matches and nothing should. If the decomp matures, the natural integration is
  source level (FLvibed's features as patches to it), which is a different project from a DLL mod for HDE.
- **The decomp as a debugging oracle.** Built (VS2022 + DirectX 8.1 SDK + vanilla DATA), it is a client with full
  symbols where goto, dock, lane and formation behaviour can be stepped through, instead of hardware breakpoints on
  retail (`controller` debug `watch`). Vanilla only, so a way to answer "what does the AI do here", not a test target.
- **Typed headers instead of SDK folklore.** `src/include/common_iface.h` (IObjInspect: 85 slots with the mangled
  suffix per slot; 26 attitude, 29 target, 36 dock target, 44 formation leader, 74/75 lane direction/start, 84 cobject
  match `core\fldll.h`), `common_cequip.h`, `common_pub.h`, `server_*.h`, `rpc_iclient.h` (IClient, 126 slots) and
  `rpc_iserver.h` (IServer, 92 slots) could replace the FLHookSDK / FLSDK copies in `research\repos` as the reference
  for struct layouts, with the decomp's `static_assert`ed offsets as proof.
- **Their cross-DLL method as a checklist.** `docs/re/cross_dll_protocol.md` lists the traps of reading foreign
  vtable calls (`RET imm16` is the frame width, not the arity; `__thiscall` vs `__stdcall` pop the same bytes; a slot
  declared stdcall pushes `this` silently) - the same class of errors `FREELANCER_RE.md` "Signatures" guards against.
- **Movie module background.** `src/movie` documents the retail DirectShow path: `PlayMovie` builds a
  `CLSID_WMAsfReader` graph and subclasses the window (notification 0x8001, Shift / Escape end the movie),
  `UpdateMovie` sleeps 300 ms and returns a "finished" byte without polling `IMediaEvent`; `MovieWindow.cpp` is where
  the picture is fitted.
- **Comms and chatter as data.** `src/Content/Gcs.cpp` (179 KB) is the radio-chatter state machine; `Voices.cpp`,
  `MissionDialogs.cpp` and `RtcScripts.cpp` the story dialogue plumbing - the server side of the lines the subtitle
  review repository catalogues; useful for the unscripted-comm speaker naming and for the four story subtitle gaps.
- **Related projects.** Librelancer (C#, MIT) is the other engine reimplementation; The Starport hosts FLUF / FLSDK /
  freelancer-retold (already in `research\repos`). The decomp owner is reachable for questions on Discord.

## Used 22 Oct 2026 (session 4, at 0bbc2ee): the M13 / M12 airlock lockout
Read through the GitHub API (`gh api repos/FreelancerDecomp/Freelancer-Decomp/contents/...`, raw files into the
session scratchpad; no clone - IDEAS X1 unchanged): `space_letterbox.cpp`, `space_camera_game.cpp` (0x51D0B4: the
letterbox and the player's lockout follow control exclusion bit 4), `space_state.cpp` (0x54B4A6: any dock start sets 4;
DestroySpaceState 0x54AF86 clears it), `client_gameobj.cpp` (SetJumpState: 7 at jump-out, 0 after state 5),
`client_iclient_space.cpp` (SystemSwitchOut / In: gate types 0x40 / 0x800 jump, everything else switches straight
away / `airlock_in`), `common_cship.cpp` (airlock_in), `common_launchaction.cpp`, `common_ceqobj.cpp`. Verified against
our Freelancer.exe / Common.dll disassembly and in game (event `control-exclusion`). Result: `..\FREELANCER_RE.md`
"Control exclusion", `..\kb\navigation-and-hazards.md` "Airlocks". The decomp named the unidentified 0x455xxx letterbox
class in minutes: worth asking first for any unexplained native behaviour.
