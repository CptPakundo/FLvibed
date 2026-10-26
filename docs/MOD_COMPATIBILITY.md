# Other mods and plugins: what they change and how FLvibed copes

FLvibed must stay robust and general across Freelancer.exe variants and the mods players run with it (user, 17-18 Oct
2026). Most of the classic plugins are Jason Hood's (adoxa, http://adoxa.altervista.org/freelancer/ - the user verified
the author; tools and sources may be downloaded for research: `research\tools\adoxa\`, git-ignored). His sources say
exactly what each plugin patches, so this list is read from them, not guessed.

## Player installs of the release (26 Oct 2026, the owner: "test right now that the mod works under normal user conditions of installation")
The published v0.1 files, downloaded from the release, installed the way a player does into game folders first stripped
of every FLvibed file and setting (the setup's uninstall), then started from those files alone (no `deploy.ps1`), in the
background (`FLVIBED_BACKGROUND`, a test requirement): HD Edition 0.7.1 (a mirror of the install HDE's installer made)
and the original 1.1 (the copied CD install + decrypted exe; test-only: flplusplus saves in the folder, the windowed
patch), each with **FLvibed-0.1-Setup.exe** and with **FLvibed-0.1.zip by hand** (README steps; the line lands after
HDE's / vanilla's `@include FL_DevOnlyLibs.ini`, the last [Libraries] line). All four: the installed DLL = the published
one; main menu in 3-6 s; both modules "FLvibed 0.1: logic started"; 202 / 202 game addresses verified, no faults; only
`FLvibed.ini` before the first start, `FLController.ini` (154 commented defaults) written at it, `FLSubtitles.ini` on
the first settings save; M01 in space (Save7798) with HUD 2 on the pad, King's story comm as a subtitle, the classic HUD;
the original game took borderless (auto) itself and Windows' Agency FB (installed on this PC, as the disc installer
does). Uninstall after play: everything out, dacom.ini without the line. Evidence `research\experience\runs\
player_install_2026-10-26\` (`tools\pi.ps1`, run.log, shots, logs). **Two setup bugs found and fixed** (tools\setup,
`test_setup.ps1` 93 checks): the Windows Apps entry was keyed on the path as typed (installed by the real folder,
uninstalled through a junction: the entry stayed) - now the folder's final path (junctions, links, 8.3 names, mapped
drives), and the uninstall removes any FLvibed entry of the same folder; the game's DLLs stay locked a moment after it
closes (an uninstall right after: FLvibed.dll not deleted, the dacom line already out) - a lock in a writable folder is
retried for up to 10 s. **Not covered:** a disc install in `Program Files (x86)` (the elevation path, VirtualStore for the
settings), a PC without Agency FB (Segoe UI fallback), a real player start in front with sound, FLUF with the setup
(it puts FLvibed below FLUF = order B), the HDE mirrors' inert dev `FL*Impl*.dll` hard links.

## Loaded by HD Edition 0.7.1 (EXE\dacom.ini, freelancer.ini)

jflp (Jason's Freelancer Patch), console, HudShift, HUDless, MP3codec, MPRep, numeric (Numeric Damage), ranks,
storyfactions, Territory, TurretZoom, WheelScroll, Zone, Hail, MultiIntro, callsign; plus HDE's own (flplusplus,
FLSharp, HudFacility / HudWeaponGroups / HudTarget / HudStatus, DelayDisplay, gls_workaround). They hook the same
Freelancer.exe / Common.dll code FLvibed does: FLvibed's detours follow an existing JMP and its vtable / IAT patches
call the previous pointer (CLAUDE.md "Hooks chain"), and every address it uses is verified by signature
(core\flexe.h), so a patched spot fails closed instead of crashing. Exception handled since 25 Oct 2026: a hooker that removes and re-writes its own jump on every call (FLUF,
below) would erase a jump chained after it, so FLvibed hooks that hooker's handler instead (either load order).

## Plugins that change what FLvibed reads or plans

| Plugin (adoxa) | What it changes (from its source) | FLvibed |
|---|---|---|
| StoryLvl | the money needed to advance a level during the story (Content.dll table at its v1.1 address 0x6fced28 = the one FLvibed found; defaults 3500 / 5200 / 7000 / 10000 / 16000 / 36000 / 30000), from `DATA\MISSIONS\storylvl.ini` | handled: the host's `worth-target` event carries the game's own target (SetMoneyNeededToNextRank) and `worth` the net worth the story code computes; storylvl.ini / 3500 only as fallbacks |
| EngClass | engines and power plants become mountable, classed equipment (hp_TYPE_engine/power_special_N on hardpoints instead of `internal`) | handled in tools\flmeta.js: a ship's power hardpoints (also the "TYPE MAX" shorthand) make every fitting plant sold at the level a candidate in the loadout optimizer (verified on a synthetic copy); engines are not ranked (all share the same top speed in vanilla) |
| Moors | player ships may use moors (the ships' `mission_property` changes to can_use_med_moors / can_use_large_moors) | handled: Test-FLSmallShipDock applies the game's rule (the ship's `mission_property` vs the object's docking spheres in solararch.ini; the ship from `pilot arch=`) |
| Lane Speed | a `lane_speed` per lane ring archetype (solararch.ini), player only; Common.dll's go_tradelane constant otherwise | handled: the autopilot measures every ride and learns the lane's speed (route::LearnLaneSpeed; verified 2502 vs 2500 in vanilla); the first ride on a lane plans with the game's law |
| Match Job | job board difficulty by the player's level (`DATA\MISSIONS\RankDiffSP.ini`: rank, min, max) instead of the bases' ranges | partly: the offers seen are the truth (reward -> exact difficulty via diff2money.ini); unvisited boards are still estimated from mbases.ini (open: RankDiffSP.ini as the prior when present) |
| Equip Mass | equipment and cargo add to the ship's mass | handled: the flight model reads the mass from the game at run time (PhySys::GetMass) |
| Console | single-player command console (HDE loads it) | the harness uses it (Send-FLConsole) |
| Damage Per Fire, Critical Hit, Healing, Random Damage | how damage is dealt | the combat model's damage numbers would need the same rules (not modelled) |
| Lane / jump / dock plugins (Trade Lane Fail, Jump Out Point, Random Jumps, Dock Key) | lane failures, jump exits, locked docks | travel re-plans on disruption and after jumps; a failed lane looks like a disruption (not tested) |

## FLUF (Freelancer Universal Framework, The Starport) - tested build nightly-2026-10-16-03-03: verified in game in both load orders (A3, 26 Oct 2026), with recorded FLUF conflicts
Full analysis, sources and the test plan: `docs\research\fluf.md` "FLUF in play" (the nightly = FLUF 627ea2c, FLSDK
ae8c857; its UI elements B, hook sites C, test assumptions D, what FLvibed needs E, known issues F). **Status (26 Oct
2026, Phase A3): run in game beside the nightly on vanilla 1.1 in both `[Libraries]` orders and the boot-loader form,
against the same game without FLUF; release build with the A3 fixes 3b4bb4a (the probe on FLUF's first frame) and 2394c05
(HUD 2's context line / the pad legend under FLUF's windows).** The table per item and order, the install state and the
evidence: `research\experience\runs\fluf_nightly_2026-10-25\TABLE.md`. In brief:

| Item | FLvibed above FLUF | FLUF above FLvibed |
|---|---|---|
| FLUF starting on the game's own data | **FLUF bug (recorded, not in its tracker): endless modal "ERROR ... [system] value pos missing parameter 2" boxes** - FLUF's universe cache reads 2-value `pos` / 1-2-value zone `size` lines as 3-value vectors (6,758 boxes by source on vanilla data); the test instance used a test-only text copy of universe.ini and the system files with those vectors padded (never shipped, never written by FLvibed) | same |
| Detection, hooks (`fluf`, `hooks`) | works (after 3b4bb4a): aware; 4 + 2 hooks moved to FLUF's handlers, FrameUpdate inside FLUF's detour (both run); 195 / 195 symbols | works: 5 + 3 hooks at FLUF's handlers |
| FLUF's windows' state + rectangles, keep-outs (HUD 2, Extended HUD 2, prompt lane, subtitles), `fluf_windows` 0 / 1 / 2, classic HUD | works | works |
| The pad on FLUF's options window / map (cursor, A / X, B, prompt bar), one cursor | works (after 2394c05 no stale context prompts) | works |
| The pad on the palette and the keys' yield to its text box | not verifiable in a background (unfocused) instance (FLUF's palette gets no keyboard focus there, `WantTextInput` 0); **attended by the owner (26 Oct 2026, release e145662): 18 / 18 harness checks pass** - focus after a click into the box, the keys to the palette, backspace, the D-pad on its list, the prompts, A runs (the B leak to free flight found there fixed in 0eca73d) | **attended check open** (the owner) |
| Flight, a fight, the autopilot, Evade owned by FLUF, a base + dealer, comm subtitles, FLvibed's options page, the harness | works | works |
| The A2 fail-closed items (A3: `setter unknown`, `server economy hooks 4 of 10`, `virtual ... not found`, lane law at defaults) | **fixed (step 2, 3582c29):** FLVIBED caption and re-pitched pause boxes, economy hooks 10 of 10, no refused virtual lookup, `lanelaw` = game (2500 / 312.5 / 400) | same |
| FLUF's own effects (recorded) | its first run sets the resolution to the screen size (#111); its always-on camera fixes make the chase camera follow more loosely (the ship leaves the frame in hard turns) | same |

Step 2 (26 Oct 2026, TABLE.md "Step 2"): the lane law read in both orders (no ride needed: `lanelaw`); the hit cues'
`hitlook: 0 effects` is vanilla 1.1's binary (BINI) DATA inis, with or without FLUF (FLvibed's ini reader reads text
only; HDE ships text) - a vanilla gap, not FLUF's. Not reached: FLUF's toasts, the mission briefing's map. Recorded: the
game's own target label at the screen's top-left corner in A3 frames where FLUF's camera showed no own ship (not
FLvibed's drawing; not reproduced).

Regression pass (26 Oct 2026, release 7ed97a5, own copy `v11ns3`, both orders; `research\experience\runs\coordinator\regression.md`):
the options rows (FLUF DETECTED), Extended HUD 2 in a base, a dealer opened and closed with pad B, M01 flight with HUD 2
and the classic HUD, story comms as subtitles and chatter to the chat feed, 202 / 202 symbols - all pass. **Open owner
checks (attended):** the palette run in order B; FLUF alone (FLvibed out of dacom.ini) with Alt+Enter; the game's
tutorial boxes on the pad; FLvibed's borderless window by the player (Alt+Tab, Alt+Enter, a fullscreen-configured start,
the cursor at the screen edges).

**Verified (repository, release notes, source):** active - 1,325 commits, nightly builds (last 16 Oct 2026), stable
2.0.0 (28 Mar 2026), developers Lazrius, Aingar, Haste; home https://codeberg.org/TheStarport/FLUF (GitHub copy archived);
licence AGPL-3.0. A client + server framework: `fluf/FLUF.dll` (ImGui UI, hooks, managers) plus modules chosen in
`fluf/FLUF.yml`; installed via `dacom.ini`: `dll_search_path = .\fluf` (and `.\fluf\modules`), `dacom_fix.dll` first
in `[Libraries]`, `fluf/FLUF.dll` last; needs the VS2026 x86 runtime; hard-coded v1.1 addresses. It **refuses to load
beside jflp, FLSharp, DelayDisplay, MP3codec, storyfactions, callsign** (all HDE 0.7.1 plugins) and six other adoxa
plugins (lanespeed, fixedmounts, healing, nonavmap, equipmass, ammolimit): **no FLUF on HD Edition**; FLvibed + FLUF
means vanilla 1.1 (decrypted exe) or a FLUF-based mod (Discovery 5.3 ships a 1.x-layout build). The older generations:
TheStarport's FLHook archived 16 Jul 2026 (FLUF is its declared successor; server work continues in FLHDE/FLHook);
JFLP is adoxa's finished patch.

| FLvibed site | FLUF | FLvibed since 25 Oct 2026 (da1363f, e2611cd; reviewed for the nightly 26 Oct 2026: `research\experience\runs\fluf_nightly_2026-10-25\A2_REVIEW.md`) - unit-tested on a simulated FLUF and on the nightly's handler entry bytes; run in game beside the nightly in A3 (TABLE.md) |
|---|---|---|
| FrameUpdate, UiRender, CursorUpdate, WinKey, HandleKey, PlayArch | FLSDK `FunctionDetour`: removes its jump, calls the original, re-writes the jump, every call | **Either order: both run.** FLUF first: `hk::Detour` follows FLUF's jump and hooks FLUF's handler (code FLUF never rewrites; its first instructions moved into FLvibed's trampoline). FLvibed first: FLvibed's jump becomes FLUF's saved "original" - both ran, but FLvibed's caller was FLUF, which broke WinKey's real-key test (the autopilot key, the page) and the subtitles' caller checks on PlayArch (comm / scene voice lines); at the first frame `hk::AdoptRestoreStyle` moves those hooks to FLUF's handler too. Result in both orders: FLvibed outermost, then FLUF, then the game; the game as FLvibed's caller. A handler whose start cannot be moved is refused with a log line naming the feature (fails closed). Not covered: FLUF's own direct calls of the original (e.g. `SoundApi::CreateSoundSource` for FLUF's sounds) bypass FLvibed's hook - they are FLUF's sounds, not the game's voice lines |
| UiControlRemove (+9 patched by FLUF's HudManager) | `PatchAssembly` 0x59DC09 inside FLvibed's signature | the signature has wildcards at +9..+13 (HDE's HudFacility puts its own jump there): resolves in either order |
| Evade update guard (Common.dll vtable slot 7) | FLUF's `VTableHook` owns the slot (re-implemented update; it throws on a vtable patched before it) | FLvibed leaves the slot alone, logs the owner once, and the autopilot dodges with its own slide bursts and breaks (`dodge` 2 falls back) |
| Game cursor drawing (0x41F33E-0x41F6E9) | NOPed; ImGui draws the cursor at the game's own cursor position (0x616840 / 44 = `exe::MouseX` / `MouseY`) and ignores CursorShown | the pad cursor drives that position, so FLUF's arrow follows it; FLvibed's hidden cursor (pad screens) also clears the game's current cursor (`exe::CurrentCursor`) around the call its hook wraps, so FLUF's arrow hides too |
| D3D8 (CreateDevice, Reset, Present), Content.dll IAT, server vtables | code detours in d3d8.dll / server.dll | FLvibed swaps pointers: chains in either order |
| Modules `ui_overhaul` (ImGui dealer), `objective_tracker`, `smooth_strafing`, `combat_ai` | replace / overlap vanilla screens and behaviours | not checked; the dealer overhaul likely hides the vanilla dealer FLvibed's pad screens build on |
| Control classes' message slot vt[0xA8] (the nightly's `HudManager::CreateControlDetour`) | `PerformDetour` written into every control class it creates | **fixed (3582c29, verified A + B):** SetProp accepts FLUF's forwarder when the class's original slot in Freelancer.exe's file (`hk::OriginalPointer`) is one of the six known setters - the call runs FLUF's suppression check and the game's setter; the HUD 2 click box (a private vtable FLUF never recorded) forwards to the game's setter from the file. Anything else still fails closed. A control FLUF suppresses (its palette open: `ManeuverFrame` and every later sibling) drops the call: the bar's frame shift counts what the frame holds after the call, not what it asked for (the owner's attended run: each palette open had crept the classic bar's frame right by half a pitch, the free-flight button left of it; fixed 26 Oct 2026) |
| IServerImpl slots (server.dll; 6 of FLvibed's 10 economy hooks) | pointer swaps from content.dll's load (FLUF keeps the replaced pointer and calls it) | **fixed (3582c29):** the slot is found in server.dll's file and FLvibed chains over FLUF's pointer (restore-style code only): `server economy hooks 10 of 10` in both orders |
| Common.dll vtables of CShip, CSolar, CLoot, CEGun ... (FLUF `VTableHook`) | FLUF pointers in the slots | **fixed (3582c29):** `hk::VtableSlot` reads the class's entries from common.dll's file when the live ones end at a foreign pointer; calls go through the live slot (FLUF's hook, which chains) - no `not found` in either order |
| common.dll lane law (`core\game\route.cpp`) | `AllowAdjustableTradelaneSpeed` E9 at the signature's first byte | **fixed (3582c29):** when that jump leads into FLUF the law is read from the original code (`hk::FindInModuleOriginal`): FLUF's speed manipulator's base is 2500, the game's constant, and no shipped FLUF module overrides it - a FLUF module that changed lane speed would make the law's speed wrong (the autopilot still measures each ride) |

**Logs:** each host writes `FLUF loaded before FLvibed ...` or `FLUF not loaded (yet)` at start and, at the first
frame, `no FLUF` or `FLUF present: n hook(s) at its handlers (m moved there now), k inside its detour`, plus one line
per hook at a handler (`Hook <name>: FLUF owns the function ... hooked at its handler FLUF.dll+0x...`). The awareness
layer adds `fluf: FLUF nightly-2026-10-16 (dacom_fix), ImGui 1.92.7 WIP: aware ...` (or `... co-residency only (why)`)
and one `fluf: open <kinds>` line per change in FLController.log, the subtitles' `display: FLUF ...` line; the controller's
debug command `fluf` lists the build, status, policy, text input, mouse and every keep-out; `hooks` (both modules) every
detour's mode and place.

### The FLUF-awareness layer (A2, 26 Oct 2026: built, unit-tested; in game A3: works in both orders, see above)
- **Detect once** (`core\fluf.*`, one instance in FLvibed.dll served by the HUD bus v4 `ReadFluf`): `fluf.dll` loaded;
  the build by FLUF.dll's PE TimeDateStamp + SizeOfImage (the version resource is always 1.0.0): nightly-2026-10-16
  (0x6AD1955A / 0x1944D000) = aware, stable 2.0.0 = co-residency only, anything else = co-residency only + one log line;
  the install form (FLUF_boot / dacom_fix) and the nightly-class exports only in the log. No FLUF: nothing changes.
- **Live state + rectangles:** for the known build, after `GetVersion() == "1.92.7 WIP"`, FLUF's exported Dear ImGui
  (`GetCurrentContext`, `GetIO`, `GetFrameCount`) and ImGui's window list, field offsets measured on the ImGui headers
  FLUF ships with its compile definitions; every region is tied to the exports first (io at context+IO, io.Ctx,
  FrameCount, DisplaySize, the list's size / capacity, every window's Ctx / Name / LastFrameActive) - one failed check =
  co-residency only for good (fails closed). Once per FLUF ImGui frame (lazily, whoever asks first): every open
  top-level window by kind - palette `fluf_cmd_palette`, options / Player Status (one ImGui window "Player Status": the
  name cannot tell which), nav map `NavMap*` (the briefing's too), developer (`Debug Window`, `DEBUG:*`), toasts
  `##TOAST*`, any other (module windows) - its rectangle, FLUF's click rule, `io.WantTextInput`, whether the cursor lies
  over a window FLUF takes the mouse over.
- **Keep-outs** (FLUF's ImGui draws over FLvibed's overlay and eats the mouse over its windows): HUD 2's parts, Extended
  HUD 2's docked overlay (strip, character card, decision words) ease to 0.15 alpha under a FLUF window; the prompt lane
  places its pills clear of FLUF's windows like any window; the subtitles' block moves below a window covering its top
  (the flight top panel under the top-centre palette, with its frame and comm portrait) or above one over its bottom edge.
- **Input** (the rules in one place: `core\fluf_rules.h`, unit test `fluf_probe`): while FLUF's text box has the keys
  (`WantTextInput`) **or its command palette is open, its search box focused or not** (the owner, 26 Oct 2026: "much
  like a chat text box being open - it takes focus for all input"), FLvibed holds all its own keyboard input (the
  autopilot key, the options page's keys, every FLvibed key) and the keys pass to FLUF untouched; with `fluf_windows` 0
  too. While **any FLUF window that takes input** is open the pad drives it instead of FLvibed's dispatch - generalised
  from ImGui's own state (26 Oct 2026, the owner's attended run: a palette command's window "Move System", kind "other",
  was not driven): a window takes input when FLUF's click rule gives it the mouse and ImGui hit-tests it (not
  `ImGuiWindowFlags_NoMouseInputs`; a background-less module text such as the load-limit line does not), never the
  developer windows (the Debug Window is open all the time beside the debugger module) or a toast. Left stick = the
  game's cursor FLUF reads, A / X = mouse buttons, right stick = wheel, B closes the focused window (ImGui's NavWindow,
  else the frontmost): FLUF's own windows (palette, options / status, map) by the Escape FLUF handles (it closes the
  topmost; the palette clears first), any other window - a palette command's (`Adjust Cash`, `Move System`, `Message`,
  ...: `ImGui::Begin` with a close button, which FLUF's Escape never reaches: `UiManager::ProcessEscapeKey` walks only
  its FlWindows) or a module's - by its title bar's close button through the game's cursor (ImGui's layout: a FontSize
  square FramePadding in from the bar's right end; the pad cursor goes there, the left button goes down and up, every
  step capped in frames; `fluf: B closed '<name>'` in the log). On the palette (with keyboard focus) the D-pad steps its
  list and A runs the command; the prompt bar says so. FLUF's search box is not focused on open: FLUF never calls
  imcp's `CommandPalette::FocusSearchBox` (627ea2c). What exists (checked 26 Oct 2026, not used): FLUF.dll exports
  `Fluf::GetCommandHandler` and `Imcp::CommandPalette::FocusSearchBox` (const; it only sets the context's
  `nextCommandPaletteActions.focusSearchBox`, consumed by the palette's next frame: imcp `command_palette.cpp` 111-117,
  167 / 184), but the palette instance is the private `CommandHandler::commandPalette` (a `unique_ptr` member, no
  export, nothing to tie its offset to) and the call dereferences the current imcp context unchecked in a release build
  (an `IM_ASSERT` only) - not clean enough to call; the player focuses the box with the pad's cursor + A or a click.
  The log names every FLUF window by its ImGui name once per change (`fluf: windows ...`). **Press ownership**
  (`core\press_gate.h`, unit test `press_gate`; the owner's attended run 26 Oct 2026: the B that closed `Move System`
  and the B that closed the palette were each also `pad B free_flight tap`): a press belongs to the pad consumer that
  had the pad when it went down - the flight dispatch, the pad on FLUF's windows, the menu / window cursor each keep a
  gate, and a button or stick already down on a frame that consumer did not have the pad stays hidden from it until
  released (a dispatcher hold it had begun is cancelled, no tap). So the B / A that closed a FLUF window, the palette, a
  game popup or a pad menu never reaches flight, and an A held in flight is no mouse button on the window that opened
  under it (it was: `buttons=0x1`). **One cursor:** FLUF draws its arrow at the game's cursor (the pad moves it);
  FLvibed draws none of its own; the pad screens' hide clears the game's current cursor so FLUF's arrow hides too.
- **Hotkeys:** FLUF claims USER_YES (palette) and USER_NO (options; both unbound until the player binds them), USER_CANCEL
  while its escape-closeable window is open, the mouse buttons / wheel over its windows, USER_NAV_MAP (its map always).
  FLvibed binds no USER_YES / USER_NO; its own raw keys yield while FLUF types (above); its pad commands Escape / nav map /
  mouse follow FLUF's rules natively (the pad's map button opens FLUF's map, which the pad then drives). A player who
  binds FLUF's palette or options key to FLvibed's autopilot key (`autopilot_key`) gets both: rebind one of them
  (`autopilot_key` on the options page, or FLUF's line in keymap.ini).
- **Setting** `fluf_windows` ([Controller], options page Controller / SYSTEM): 0 IGNORE (co-residency only; keys still
  yield to FLUF's text box), 1 PLAYER WINDOWS, 2 ALL (default since 26 Oct 2026, the owner: also FLUF's developer
  windows - its Debug Window, open at every start at (60, 60), covers HUD 2's nav / the strip's left end, which fade).
  FLUF's windows stay on top; FLvibed's HUD parts fade under them, each part on its own, and are never moved (the owner).
  The row's note on the options page: NO FLUF INSTALLED / FLUF DETECTED / FLUF DETECTED - CO-RESIDENCY ONLY.
- **Alt+Enter (FLUF's ScreenControl, recorded 26 Oct 2026):** FLUF re-routes Alt+Enter to its own toggle, which flips
  the fullscreen byte of the game's display settings, swaps its activation patch (`SetBackGroundRunPatch`, the window
  procedure's WM_ACTIVATEAPP path) and calls the game's SetupScreen directly. In the owner's attended run (vanilla 1.1,
  4K at 200 %, the game DPI-unaware at 1920x1080) the switch to exclusive fullscreen froze the game after two
  `D3D8 device Reset -> 0x88760868` (D3DERR_DEVICELOST: the device could not be reset at that moment - not a resource
  left behind, which answers D3DERR_INVALIDCALL). FLvibed's part: its Reset hooks only pass through and release first;
  it holds no D3DPOOL_DEFAULT object (textures MANAGED, DrawPrimitiveUP, no render targets); its state blocks go before
  every Reset - the controller's shared renderer's state block did not (fixed 26 Oct 2026). A forced in-place Reset
  (`d3dreset`, background test starts) shows the game itself does not recover from a failed Reset on vanilla 1.1 (no
  further Reset, the game hangs; with and without FLUF), and on HDE retries every second. **Found and fixed (26 Oct
  2026):** on HDE the subtitles' comm-portrait capture (a back-buffer readback, `CopyRects` to a system-memory surface,
  10 Hz while a comm plays) left something of D3D9's default pool behind under DxWrapper / d3d8to9, so every later
  Reset - the game's own recovery, a resolution change, Alt+Tab out of fullscreen - failed D3DERR_INVALIDCALL for good
  (bisected: everything but `CopyRects` = recovers). The readback now runs only on the system's own Direct3D 8
  (`render::ReadbackSafe`: the device's code in the Windows system directory; the log says `render: back-buffer
  readback on / off`); under a wrapper the portrait is not held through its closing animation. Verified: HDE 4K and 4:3,
  both modules, 6 of 6 forced Resets recovered (before: the game stuck after the second). On vanilla 1.1's system D3D8
  the readback stays on - whether it does the same there cannot be shown (vanilla never retries a failed Reset): an
  owner question. The owner can tell FLUF's part from FLvibed's by Alt+Enter with FLUF alone (FLvibed.dll out of
  dacom.ini), attended. **Since 26 Oct 2026 FLvibed's borderless window** (`borderless` 1 auto, the default: vanilla 1.1
  has no borderless mode and FLUF adds none) turns every display setup into a windowed one, so FLUF's Alt+Enter never
  reaches exclusive fullscreen any more: it switches between FLvibed's borderless window and the game's bordered one
  (FLUF's request = a copy of the applied settings with the fullscreen byte flipped; FLvibed sees the flip). Verified
  in both load orders (`winmode altenter`, the same call FLUF makes; Resets D3D_OK). FLUF still swaps its activation
  patch for "fullscreen" on every other press (the game then pauses out of focus; FLvibed's `run_in_background` covers
  it). `docs\kb\display.md` "Window mode path".
- **Limits (by design or open):** one frame of lag (the snapshot is FLUF's last frame); the first keys typed in the very
  frame the palette opens may still reach FLvibed; the options page's FLUF note shows FLUF DETECTED in game (both
  orders, regression pass 26 Oct 2026: `research\experience\runs\regression_2026-10-26\shots\A_options_borderless.png`); typing into FLUF's palette from the pad (FLvibed's on-screen keyboard) is not wired; FLUF modules' windows are
  keep-outs as "other" but untested.

**Verified:** unit tests (`build.cmd test`): `fluf_hooks` (FLSDK's algorithm verbatim on test functions - FLvibed
first + adopt, FLUF first, two FLvibed parts before / after FLUF, an unmovable handler refused, 2.0.0's overwrite, no
FLUF; and the nightly's six handler entry bytes moved whole and hooked), `fluf_probe` (a fake ImGui state in FLUF's
layout: the build table, the window names, rectangles / text input / mouse rule / policy, every layout check broken in
turn fails closed); dev and release builds clean. Earlier: FLvibed alone after da1363f on HDE 0.7.1 and vanilla 1.1
(`research\experience\runs\fluf_compat_2026-10-25\`). In game beside the nightly: A3 (above, TABLE.md).
### History: in game beside FLUF stable 2.0.0 (25 Oct 2026, release build of e2611cd) - not the target build
*Unverified per the owner (26 Oct 2026: "assume nothing concrete related to FLUF happened") and a different build from
the target nightly (2.0.0's HandleKey overwrite and helper DLLs are gone in the nightly; the nightly adds the nav map,
Evade, control-class and server hooks this run never met). Kept as the record of what was run.*
FLUF stable 2.0.0 (tag `stable-2026-03-28-18-16`, `FLUF.tar.xz` 55,651,208 bytes, SHA-256 `bd5635f2...d830b390`,
AGPL-3.0; the owner's download OK, `Install\downloads\fluf\`, never in git) on a copy of the vanilla 1.1 instance
(`Install\instances\vanilla11_fluf`), FLUF's files per its docs; `FreelancerSDK.dll`, `freetype.dll`, `libcurl.dll`
also beside Freelancer.exe (2.0.0 ships no `dacom_fix.dll`, without which `fluf/FLUF.dll` does not load); the default
`FLUF.yml` (`debugger.dll` module) with the console sink off for background tests. Each order against the same instance
without FLUF; table and frames `research\experience\runs\fluf_compat_2026-10-25\` (`results_fluf_2026-10-25.tsv`).

| Item | FLvibed above FLUF | FLUF above FLvibed |
|---|---|---|
| FLvibed's hooks | HandleKey, WinKey, CursorUpdate at FLUF's handlers; FrameUpdate stays inside FLUF's detour (both run; no caller check there); 195 / 195 symbols, nothing refused | HandleKey, WinKey, CursorUpdate, FrameUpdate at FLUF's handlers; nothing refused |
| HandleKey (pad commands) | **fixed in FLvibed (e2611cd)**: 2.0.0 overwrites HandleKey with `mov ecx, handler; jmp ecx` (not a FunctionDetour); before the fix FLvibed's logic refused HandleKey (no pad command) | the same fix (before it: signature refused) |
| Pad cursor, FLUF's ImGui arrow (shown / hidden on pad screens) | works | works |
| Options page, Extended HUD 2 base screens, HUD 2, classic HUD, pad throttle, WinKey real keys, comm subtitles with portrait, autopilot combat, Evade through the guard | works (frames match the no-FLUF run) | works |
| FLUF's palette / extended options window | open with their keys (USER_YES / USER_NO renamed); typing reaches the palette; its own key closes it (Esc does not: FLUF's palette eats every key while focused); Esc closes the options window | the same |

**Recorded conflicts (player-facing, not fixed in FLvibed):** FLUF's default `debugger.dll` window ("Debug Window",
top left) covers Extended HUD 2's location strip and the classic maneuver bar's left end until the player closes it or
drops the module; FLvibed's bound keys (the autopilot key K, the options page) act even while FLUF's palette has text
focus (FLvibed sees keys first in either order). **FLUF 2.0.0 does not hook the Evade vtable** (EvadeBehaviorHooks is
newer): the guard installs as without FLUF; the foreign-owner path (FLUF HEAD) is built but not reachable with 2.0.0.
**Not verified:** hit cues, intro-movie and room-scene subtitles, the nav map, FLUF modules beyond `debugger.dll`,
Discovery's FLUF build, FLUF on HDE's exe without HDE's plugins.

## The vanilla game's Freelancer.exe (checked 25 Oct 2026, sigtool check --scan)
`research\Vanilla\Freelancer\EXE\Freelancer.exe` (the CD install; FileVersion 1, 0, 0, 0; 3,005,956 bytes; link
timestamp 0x3E401B79) is **SafeDisc-protected**: sections `stxt774` / `stxt371`, its `.text` encrypted on disk (entropy
7.99 bits / byte vs HDE's 6.49). All 187 signed symbols of `core\flexe.h` are unresolved there - not because the code
differs: HDE 0.7.1's Freelancer.exe has the **same link timestamp and the same 1,812 KB `.text`**, decrypted (plus a larger
`.rsrc`). FLvibed verifies its signatures in memory at run time, after decryption, but SafeDisc's driver does not run on
Windows 10 / 11 (blocked since 2015), so the CD's program does not start there at all. **Requirement: a decrypted
Freelancer.exe of this build** - HDE provides it (verified); the community's unprotected 1.1 programs likely qualify (not
verified). Whether the decrypted vanilla game without HDE's plugins runs every feature (HUD 2's own-ship view = HDE's
HudStatus.dll; the arrival lines = Territory.dll / Zone.dll): the in-game test below.
A decrypted vanilla 1.1 program (the owner's cracked exe, 3,005,956 bytes, MD5 033C10EB8648DE7A8F2009091672880A; checked
25 Oct 2026 with `sigtool check --scan`): first 174 / 187; **since the re-cut of the same day 187 / 187 statically, every
one at the v1.1 address** (also with `--shift 16 --hooked`; HDE 0.7.1 and the upstream HDE repository's exe stay 187 /
187). **In game on vanilla 1.1: 187 / 187 at run time too, nothing refused** (25 Oct 2026, below).
- **What failed, and why:** UiRender, DmgIndContact, ClearWaypoints, LetterboxAmount, CursorName, MouseFlightTimer,
  PlayerThrottle, RoomInfoOwner, PrefsShowPage, TextCtlSetProperty, CursorShown, ToggleButtonActivate and
  NavMapSetPosition had signatures (cut from HDE's exe) that ran into the **alignment padding between functions**. There
  HDE's exe holds NOPs, the cracked vanilla exe leftover call bytes (`E8 rel32`, once `FF 15 [IAT]`) from its decryption -
  after a `ret`, never executed. Of the 1,322 byte runs in which the two `.text` sections differ, 1,285 are such padding;
  the other 37 are HDE's real code edits, **none inside a signature span**.
- **The re-cut:** the 7 code symbols now start at the function entry (ClearWaypoints ends at its `ret`); the 6 data
  symbols moved to another instruction that references the same global (LetterboxAmount 0x41A104, CursorName 0x41DE4A,
  MouseFlightTimer 0x4C7D6D, PlayerThrottle 0x4C7B0A, RoomInfoOwner 0x435DF8, CursorShown 0x41F30A). The two detoured
  ones keep identical stolen bytes in both programs (UiRender `B8 08 10 00 00`, DmgIndContact `83 EC 3C 56 8B F1`).
- **Behaviour notes (per symbol in `core\flexe.h`):** 12 of the 13 cover identical executed code - no behaviour change
  expected. **TextCtlSetProperty**: the function is identical, but HDE rewrites 26 bytes of the function just before it
  (0x58B2D5: vanilla steps a float at +0x37C there, by the bytes a text animation timer) - **behaviour may differ on
  vanilla: verify in game** (how the texts FLvibed sets appear). Not covered statically: the features that use HDE's
  plugins (HUD 2's own-ship view = HudStatus.dll; the arrival lines = Territory.dll / Zone.dll) and HDE's other code edits.
- So **HD Edition 0.7.1 stays the tested base**; the decrypted vanilla program resolves every symbol and runs the mod
  (in-game test below), with the degradations listed there.

### In-game test on vanilla 1.1 (25 Oct 2026, release build of 92a9509)
Instance `Install\instances\vanilla11\Freelancer` (git-ignored): the CD tree (1.0), the 1.1 patch's Common / Server /
RemoteClient / DALib / FLServer / Content.dll over it, the decrypted exe; 1280x960 windowed, background start, muted.
Evidence: `research\experience\runs\vanilla11_2026-10-25\` (frames named by moment, both modules' logs of both
sessions, the M01a runner output). New game -> M01a "Mission Result: Success" at Pittsburgh in one run.

**What a player needs (the requirement):** the game installed with the official installer (it shows the EULA, which the
player accepts there; it also installs the Agency FB fonts into Windows\Fonts and writes the HKLM install keys - the
copied test tree has none of these and single player needs no key: the game itself writes only HKCU `...\Microsoft
Games\Freelancer\1.0 FIRSTRUN`), the official 1.1 patch, a **decrypted Freelancer.exe** (SafeDisc does not run on
Windows 10 / 11), then FLvibed: `EXE\FLvibed.dll`, the line `FLvibed.dll` in `EXE\dacom.ini` `[Libraries]` (after
`.\flmaterials.dll`), `EXE\FLvibed.ini` (`[Modules]` subtitles / controller = 1), `EXE\FLSubtitles\*.tsv` and
`EXE\FLvibed\autopilot.3db`. Nothing else (no d3d8to9 / DxWrapper: FLvibed hooks the native D3D8). Window modes:
vanilla 1.1 has exclusive fullscreen or a bordered window (`-w`, or the windowed byte patch); with FLvibed's `borderless`
(auto, the default) either becomes a borderless window covering the monitor at its resolution (`docs\kb\display.md`
"Window mode path"); sharp at high DPI only with a DPI-aware exe (the compatibility flag HIGHDPIAWARE).
**Test-only additions (not a player requirement):** HDE's `flplusplus.dll` with only `save_in_directory` /
`screenshots_in_directory = true` and neutral scales (vanilla writes to Documents\My Games otherwise), HDE's windowed
byte patch on the instance's exe copy (file 0x1B16CC = 0; the same bytes as HDE's exe), a pre-made `SAVE\PerfOptions.ini`
(1280x960, volumes 0). Documents\My Games stayed untouched; `AppData\Local\Freelancer\FLSpew.txt` is the game's own.

| Feature | Verdict | Why / evidence |
|---|---|---|
| Startup | works | both modules up, symbols 187 / 187 v1.1 (0 by search, 0 unresolved), nothing refused except the maneuver bar shift ("no HudShift relocation": HudShift is HDE's) |
| Startup movies: skip, fit | works | skipped natively; `fl_intro_vanilla` recognised (226.8 s), fitted 1280x528 in 1280x960 |
| Intro movie subtitles | works (since FIX-VIEWS, 25 Oct 2026) | was: lines active, overlay window present, no text seen - not reproduced in the follow-up (the first run already showed them, with the FIX-TEXT font loader in the build); now drawn in a child window of the game window (window capture and the taskbar thumbnail show them), transitions at the display rate (`subtitles\docs\DISPLAY_PLAN.md` "Movies"; `research\experience\runs\fix_views_2026-10-25\movie_vanilla`) |
| Main menu, pad prompts | works | "A SELECT" pill (`01_mainmenu_pad`) |
| New game, Freeport 7 prologue, Manhattan scenes | works | prologue comms recognised; base scene subtitles drawn (`08_bar_sub0`: "Trent: ...") |
| Rooms on the pad | works | D-pad walks the 7 bar characters in screen order, the game's name card over the head (`09_bar_dpad`) |
| ui_style 1 (rooms, dealer, job board) | degraded | glass panels, LB / RB pills, the location strip and cards work (`10`, `11`, `13`); "PLANET MANHAT..." cut; no bottom pills: promptbar "no bottom row or column fits at 1280x960" (last-resort lanes) |
| ui_style 0 | works | the game's look back live (`14_dealer_ui0`) |
| Launch, space cutscene + comms | works | Donau intro cutscene with subtitle cards (`15_after_base`); the card overlaps the NeuroNet strip at 4:3 |
| HUD 2 (hud_style 1) | degraded | ring, nav, threat list, target card, escorts, loadout, context pill work (`16`, `19`, `20`); text cuts / overlaps (below). Fixed by FIX-VIEWS (25 Oct 2026, `docs\UI_HUD2.md` 5.12): the own-ship view (the status panel's own view through the panel base's routines, no HudStatus.dll), the target card's view (its view mode), the flat icons from vanilla's 256 px sheets (`fix_views_2026-10-25\views`) |
| Classic HUD (hud_style 0) | works | the game's HUD with glyph badges and the autopilot legend (`17_space_classic`); the comm subtitle box covers the top-centre HUD icons |
| Hits | not judged | only 2-5 % shield hits came (`21_hit0-2`); no hull hit |
| Comm portrait | works (since FIX-VIEWS, 25 Oct 2026) | was: at the game's comm-window place, under HUD 2's nav, overlapping its rows (`16`, `23_comm1`). Now where the top panel holds no slot for it (4:3) the game's comm window is placed live under HUD 2's nav from the HUD bus every frame - the same code and place on HDE 1600x1200 and vanilla (`fix_views_2026-10-25\comm`). **Since fix-text 5 (25 Oct 2026) with its dialogue on both HUDs** (the owner saw it stuck at the left in the Donau fight): HUD 2 at 4:3 puts it above its line inside the panel; the classic HUD (bar centred: no HudShift) beside its line's box below the bar (`fix_text5_2026-10-25\vanilla_b2_*`, M01a: King, Newark Station) |
| Advanced Autopilot / harness | works | `M01a.ps1` (plan B while waiting) flew the Donau fight, TL146, the Brandt, the ambush; the out-of-formation tail TL34 -> Pittsburgh by `Start-FLTradeLane` / `Start-FLTravel`: SUCCESS; `[Pilot] pilot_military_hard` missing in vanilla data (defaults used) |
| Pause menu | works | FLVIBED button, A / B pills ("below-obstacle lane") (`25_pause_menu`) |
| TextCtlSetProperty (text animations) | works | location typewriter text, objectives, the FLVIBED button text normal; nothing odd seen |
| Sound | works | silenced in-process (1025 buffers by 11 s) plus volumes 0 |

**Causes found (fixes left to the follow-up track; any fix must derive from the live game, same on every exe):**
- *Flat icons off* (own-ship disc, no target icon fallback): `core\game\flicons.cpp` reads only 1024x1024 sheets;
  vanilla's `DATA\INTERFACE\ButtonTextures.txm` is 0.96 MB (HDE's 18 MB, 1024 sheets): "no flat icon sheet could be
  read". The target view itself is called (`hud2` view target draws=36912, refused 0) but shows nothing on vanilla
  (native d3d8 instead of d3d8to9 is the main difference; not isolated). HudStatus.dll absent -> no own-ship view (expected).
- *Text cut with "..." everywhere* (nav "Destroy the...", "Dock with t...", "Awaiting Mi...", place "NEW YORK ·
  LIBERTY..."; autopilot "FIGHT..."; target class "FIGHTE..."; ship "Starfl..."; strip "PLANET MANHAT..."; overlap
  "NEXT" / "HOSTILES" in the target list header; escort "Transport USV Brandt" overruns its box; all at 1280x960):
  the main cause is the **font, not 4:3**: `gamefont` loads only `DATA\FONTS\fonts.ini [FontFiles]` (HDE ships the
  TTFs there); vanilla's fonts.ini has none (the installer puts Agency FB in Windows\Fonts), so FLvibed falls back to
  Segoe UI, much wider than Agency FB, against the same width caps.
- *Portrait*: vanilla has no HudShift, so the maneuver bar cannot move (refused, above) and the 4:3 top panel holds no
  portrait slot (since fix-text 5 the portrait goes beside its line's box, or above its line in HUD 2's panel); before,
  the comm window fell back to its native place shifted below HUD 2's nav (`comm window shift -0.2006`),
  read when the window opens, so a nav that grows rows later (place, autopilot line) runs under the portrait.
- *Follow-up evidence*: one weapon row "1 Justice Mk I" for two Justice guns (classic HUD lists 1 and 2, `17` vs `16`);
  "Transport USV Brandt" (20 chars) past the escort box (`20`, `21_hit1`); the own-ship disc (`16`, `23_comm1`).
**The official 1.1 patch** (`flpatch.exe`, 1,657,440 bytes, MD5 b2621b2a33c519d8b599cb3579be4c02; its cabinet FILESUSA.CAB
listed / extracted with 7-Zip, not run) replaces only Common.dll, Server.dll, Content.dll, RemoteClient.dll, DALib.dll,
FLServer.exe and readmes - **no Freelancer.exe**: the program is the same in 1.0 and 1.1 (one link timestamp, 0x3E401B79),
so the cracked exe above is a valid 1.1 program. The CD folder `research\Vanilla\Freelancer` is **1.0** (its DLLs differ
from the patch's). HDE 0.7.1 ships the patch's RemoteClient.dll unchanged and its own builds of Common.dll, Server.dll,
Content.dll and DALib.dll (none matches 1.0 or the 1.1 patch). FLvibed reaches those DLLs by their exports / import
tables (not by flexe.h signatures); whether every read works on the 1.1 patch's builds is not tested.

## Vanilla 1.0 vs HD Edition 0.7.1 (decoded with adoxa's TINI, 18 Oct 2026)

`node tools\flmeta.js --data research\vanilla_text` (the vanilla DATA decoded by TINI, which uses Freelancer's own
ini reader): every player ship, gun, shield and price is identical; HDE no longer sells the Starflier (vanilla: at most
bases, level 0); **242 of 404 NPC loadouts changed** in HDE - their shields, mostly to the class matching the ship (e.g.
Navy Defenders 1124 -> 1225), weapons unchanged. FLvibed reads the data it runs on, so both work.
