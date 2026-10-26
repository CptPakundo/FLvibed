# Freelancer.exe (v1.1, as shipped by HD Edition 0.7.1) — reverse-engineering notes

Game facts shared by all mod projects in this environment. All addresses are virtual addresses in
Freelancer.exe (image base 0x400000, file offset = VA - 0x400000 for .text/.rdata/.data), verified in game
against the HDE binary unless marked "(FLUF)" etc. Annotated disassembly: `disasm\Freelancer.ann.asm`,
read ranges with `tools\dis.ps1 -From 0x.. -To 0x..`. Note that `Vanilla\Freelancer\EXE\Freelancer.exe`
is the CD's v1.0 binary — addresses differ there, and its code is SafeDisc-encrypted on disk (sections
`stxt774` / `stxt371`, ciphertext in `.text`): it cannot be read or matched statically.
Credits: FLUF / FLSDK / ChaosMod (TheStarport), flplusplus, FLHDE windowed-mode-mouse-fix.

When a project verifies a new game fact, add it here (and keep project-specific hook choices in the
project's own notes).

## Signatures (how FLvibed uses these addresses; 17 Oct 2026)
FLvibed never trusts a fixed address. Every Freelancer.exe address it hooks, calls, reads or compares is a symbol
in `core\flexe.h`: name, kind, v1.1 address, signature (hex bytes, `??` = any byte) and the offset of the address
in it (`hk::Sym`, `core\hook.h`).
- **Code** (detour targets, called functions, button-class handlers, return addresses of known callers): the
  address is `off` bytes into the signature. Function entries start their signature at the entry, so the fast path
  checks exactly the bytes a detour steals.
- **Data** (globals, camera objects, vtables): the signature is an instruction that references the global; the
  value is read from its operand (`off` = the operand). The death camera is only referenced at +4 (its rotation,
  `CamDeathRotation`); some cameras (cockpit, chase, turret, reverse, static) have no immediate reference at all.
- **Resolution** (once per module part at startup, `exe::ResolveAll`, ~0.1 ms): the signature must match at the
  v1.1 location (data: and the operand must hold the v1.1 address). A detour JMP already at a code address counts
  as matching: its 5 bytes and up to 3 padding NOPs are skipped (other plugins, or FLvibed's own host when a logic
  part resolves). Otherwise the signature is searched in `.text` (~65 ms for the whole table) and must match
  exactly once; a data operand must point into the image.
- **Fail closed**: an unresolved symbol refuses its use (`hk::Detour` installs nothing, `hk::Call` returns R(),
  `Mem` reads zero and drops writes, `.is()` is false); the module log names the symbol and the feature that stays
  off (`Hook Comm (v1.1 0x4CE000) NOT installed: signature mismatch - comm subtitles off`). Groups that only make
  sense together are gated together (room character ctor + dtor, the waypoint functions, the mouse-flight
  globals of the steering override, the key-state globals of FireCommand).
- **Signatures** come from `build\sigtool\sigtool.exe gen <exe> code <va> [stolen bytes] | data <va>` (`build.cmd
  sigtool`): bytes that look like addresses (a value inside the image, the rel32 of E8 / E9 / 0F 8x) become
  wildcards, so a relinked build still matches; the shortest window with at least 12 literal bytes that is unique in
  `.text` wins; code signatures stay unique with their first 8 bytes ignored (a foreign JMP there), data signatures
  avoid the first 8 bytes of every code symbol (FLvibed's own detours overwrite them).
  **Never span the alignment padding between functions** (25 Oct 2026): builds differ there without differing in
  code - HDE's exe holds NOPs, a decrypted vanilla 1.1 exe leftover `E8 rel32` / `FF 15` call bytes after the `ret`
  (1,285 of the 1,322 differing byte runs of the two `.text` sections). `sigtool gen` picks the shortest unique window
  and so often takes padding (`C3 90 90 ... <entry>`): check its output, start code signatures at the entry (or end them
  at the `ret`), and cut data signatures on another referencing instruction when the shortest one touches padding
  (`sigtool gen` on the vanilla exe often offers one). Then `check --scan` on every exe below must find every symbol (195 / 195 today).
- **Checks**: `sigtool check <exe>` resolves the table against a file; `--scan` skips the fast path, `--shift N`
  moves the whole `.text` (a stand-in for another build: code moves, globals stay), `--hooked` puts a JMP on every
  function entry. HDE 0.7.1: 82/82 at v1.1; with `--scan --shift 16 --hooked` 82/82 found at the expected
  addresses. In game (dev build with `set CL=/DFLV_SIG_SCAN_ONLY`: search only, all HDE plugins loaded) all four
  module parts found all 82 at their v1.1 addresses. Not tested against a real other build: none is readable here
  (v1.0 is SafeDisc-encrypted; newer HDE patches keep these addresses, `HDE_UPSTREAM.md`). 18 Oct 2026, 88 symbols:
  `check --scan` finds all 88 in HDE 0.7.1 and in the current HDE repository's exe (a different file); 0 in the
  CD's v1.0 (SafeDisc sections `stxt774` / `stxt371`, encrypted `.text`; SafeDisc does not run on Windows 10/11, so
  every exe in use today is 1.1-based and unprotected). 25 Oct 2026, 187 symbols: `check --scan` (and `--scan --shift 16
  --hooked`) finds all 187 at their v1.1 addresses in HDE 0.7.1, the HDE repository's exe and a decrypted vanilla 1.1
  exe (the owner's, read in place, never copied; `MOD_COMPATIBILITY.md` "The vanilla game's Freelancer.exe"). 25 Oct 2026
  (da1363f), 195 symbols: `check`, `--scan` and `--shift 16 --hooked` find all 195 on the same three exes; Discovery's
  hotfix exe also 195 / 195 (`docs\research\discovery.md`).
- **The game's DLLs** (Common.dll, Server.dll, ...) differ between builds too: Common.dll of the v1.0 CD, HDE 0.7.1 and
  the current HDE repository are three different files (all report version 1,0,0,0); their exports are stable
  (v1.0's 3856 all present in HDE, which adds 3; HDE 0.7.1 = repo). So FLvibed reaches them by name, never by offset:
  - exported functions by their mangled names (GetProcAddress); the mangled name fixes the signature;
  - **virtual functions** by name (`hk::Virtual`, table `core\fldll.h`): the slot is where the DLL's exported vtable
    (`??_7IObjInspectImpl@@6BIObjInspect@@@`, `??_7CELauncher@@6B@`) holds the method's own export, found once;
    all 11 used today matched the formerly hard-coded slots (debug `vtslots`);
  - **data and code without an export** by signature in the DLL's code (`hk::FindInModule` / `FindModuleData`, unique
    match required). The dock range float: v1.0 Common.dll +0x19B518, HDE +0x19C518 (a fixed offset read the wrong
    memory in v1.0).
  - Vtable calls inside Freelancer.exe check the slot's target against a signed symbol before the call
    (`exe::IconButtonActivate.is(vt[0xB4 / 4])`), and use exact `__thiscall` types with the argument count of the
    callee's `ret N` (the game's own call sites show the arguments; a `push 0` before a getter call is the next call's
    last argument, not the getter's).

## Plugin loading
- DLLs listed in `EXE\dacom.ini` `[Libraries]` are LoadLibrary'd at engine init; no exports needed.
  RP8.dll (render pipeline) is loaded before plugins and imports `d3d8.dll!Direct3DCreate8`
  (HDE's d3d8.dll is a wrapper, dgVoodoo/DxWrapper). Direct3DCreate8 is called after plugin load.

## Frame / game state
| Address | Meaning |
|---|---|
| 0x5B2890 | `void __cdecl FrameUpdate(double dt)` — prologue `51 55 56 33 ED 57` (detour 5 bytes) |
| 0x42D740 | `bool __cdecl GamePaused()` |
| 0x54BAF0 | `IObjRW* __cdecl GetPlayerIObj()` (null when not in space) |
| 0x673354 / 0x673358 | player system id / base id (base != 0 ⇒ docked) |
| 0x67337C / 0x673378 | ship archetype id / ship id |
| 0x679738 | current camera ptr: cockpit 0x678BA4, chase 0x678D48, turret 0x678F60, reverse 0x679098, death 0x6791A4, cutscene 0x6792F8, static 0x67953C, docking 0x679634; 0 in bases |
| 0x67ECA0 | main HWND |
| 0x6679F0 | u8 fullscreen flag (written by WindowStyle 0x424870) |
| 0x4249C0 / 0x424870 | SetupScreen(HWND, request*, applied*) - every display mode change; WindowStyle(HWND, fullscreen, w, h); settings 0x679BC0 (+8 w, +0xC h, +0x25 fullscreen); the window exists before the DACOM libraries load (`kb\display.md` "Window mode path") |
| 0x610850 / 0x610854 | render width / height (ints) |
| 0x679BC8 / 0x679BCC | window width / height (flplusplus) |

## Focus and running in the background (verified in game 17 Oct 2026)
- **The frame runs only while the byte `AppActive` 0x610865 is set** (0x424F30 skips the frame otherwise; with an
  unusable D3D device it sleeps 1 s per pass, 0x424FBF). The window procedure's WM_ACTIVATE (0x5B264C) calls
  `SetAppActive(bool)` 0x424C60 (cdecl), which stores it and calls 0x420350: inactive = the DirectInput mouse is
  unacquired and set non-exclusive; active = exclusive + foreground again. `IsActive` 0x424CB0 = AppActive &&
  [0x610864] && [0x6679F8] (rendering, 0x578F8B).
- Kept open (FLvibed `run_in_background`: the game's SetAppActive still runs with the real state, then the gate is
  set): game, mod and harness run out of focus. The unacquired mouse is not polled at all then (GetDeviceState never
  called): the virtual cursor is written to MouseX / MouseY / MouseButtons before the cursor update 0x41F150.
- **Activation at start:** 0x4248F3 `SetWindowPos(HWND_TOPMOST)` + `ShowWindow(SW_SHOWNORMAL)`, plus the graphics
  wrappers (d3d8 / dxwrapper / d3d9 import ShowWindow, SetWindowPos, SetForegroundWindow, BringWindowToTop,
  SetActiveWindow). Made non-activating, the window still became the foreground window once at start (a path that
  imports do not show; Windows lets a process started from the foreground process activate): the host hands the
  foreground back to the user's window within 0.1 s.
- DirectInput mouse cooperative level: foreground + exclusive while active (flags 5, 0x41F782); reads only work for
  the foreground window.

## Mouse
| Address | Meaning |
|---|---|
| 0x616840 / 0x616844 | cursor X / Y in render pixels (FL's own software cursor) |
| 0x616848 | wheel (notches) |
| 0x616850 | buttons: bit0 L, bit1 R, bit2 M (… 8 buttons) |
| 0x616858 | ptr to current cursor; name string at +4 (`arrow`, `cross`, `talk_story`, …; see DATA\mouse.ini) |
| 0x6167B8 | mouse object (`this` for the functions below) |
| 0x6167C8 (+0x10) | `IDirectInputDevice8A*` (vtable idx 9 GetDeviceState, 10 GetDeviceData) |
| 0x6167CC (+0x14) | flags: bit0 captured, bit1 absolute axes, bit4(0x10) clamp at edges (fullscreen) instead of releasing, bit6(0x40) buffered reads give buttons only (motion from GetDeviceState) |
| +0x24 / +0x28 | DI buffer size / buffer ptr; +0x2C DIMOUSESTATE2 (immediate) |
| 0x41F150 | per-frame mouse poll + cursor draw (called inside UI rendering, before the cursor). Prologue `64 A1 00 00 00 00` (6) |
| 0x41FA10 / 0x41FD90 | buffered read / immediate read |
| 0x41F970 | edge handling: clamp (flag 0x10) or report "left window" ⇒ release |
| 0x4201A0 | (this, bool) capture/uncapture: sync with OS cursor, ShowCursor |
| 0x420350 | (bool capture, w, h, fullscreen) ; 0x424C60 `SetMouseCapture(bool)` wrapper. Each capture writes flag 0x10 from its `fullscreen` argument (0x420541: `shl bl,4` into 0x6167CC), which the callers take from the fullscreen flag 0x6679F0 - so a borderless window (fullscreen 0) releases the mouse at every screen edge and recaptures it with a jump. FLvibed sets 0x10 while the game window covers its monitor (`dimouse.cpp` ClampInBorderless; verified by the owner in game 26 Oct 2026) |
| 0x4207B0 | button event dispatch (key codes -1-i for button i, double-click via GetDoubleClickTime / 4 px) |
| scale | pixels = DI counts × width × 0.00125 (double 0x5C9680) × float [0x6107D0]; wheel = lZ / 120 |
- Freelancer reads the DirectInput mouse only while it is the foreground window (cooperative level):
  injected mouse movement needs focus, command/key injection does not.
- Windowed modes release the mouse at the window edge (0x41F970).
- **Hiding the arrow (verified in game 25 Oct 2026, HostApi v20 SetCursorHidden):** 0x41F150 is two halves - the mouse
  poll (buffered / immediate reads, the edge handling, the button events, up to 0x41F30A), then the cursor draw, which
  runs only while the byte **0x6107DC** (`exe::CursorShown`; getter 0x41DD90, setter 0x41DDA0(bool)) is set: the
  cursor object [0x616858] (+0x24 its mesh, +0x50 its blend mode) drawn through the DALib pipeline's render states and
  0x41D9C0 at [0x616840]/[0x616844], then the trail. FLvibed's controller host clears that byte around the game's call
  and puts it back (the pad screens with the pad cursor hidden): the poll, the hit test and the hover run as before (the
  hovered control's cursor name still changes), only the arrow is not drawn.
- **The current cursor [0x616858]** (`exe::CurrentCursor`, read from disassembly 25 Oct 2026): the poll half never reads
  it (first read at 0x41F317, after the CursorShown gate at 0x41F30A); written at 0x41DF07 / 0x41E6FE. FLUF's ImGui
  cursor (it NOPs 0x41F33E-0x41F6E9) draws the arrow this names at [0x616840]/[0x616844] and ignores CursorShown, so
  FLvibed's hide also clears it around the call when its hook wraps FLUF's handler (`docs\research\fluf.md` 8).

## Keyboard / commands
| Address | Meaning |
|---|---|
| 0x577850 | `bool __cdecl WinKey(UINT msg, WPARAM, LPARAM)` — FL's keyboard message handler (WM_KEYDOWN/UP/CHAR/SYS*) |
| 0x576410 | `bool __cdecl HandleKey(int cmd, int category)` — dispatch to registered handlers; category 1 = keyboard (respects focus handler). Prologue `81 EC 80 00 00 00` (6) |
| 0x67C290 | u8: current event is key-down (1) / key-up (0) — set this before calling HandleKey |
| 0x67C27C / 0x67C280 / 0x67C278 | current vk / modifiers (1 shift, 4 ctrl, 0x10 alt) / current cmd |
| 0x67C284/88/8C | last msg / wParam / lParam ; 0x67C291 repeat flag |
| 0x5784A0 | `bool __cdecl IsCmdHeld(int cmd)` — polls bound keys (GetKeyState / mouse bits). Prologue `8B 0D 58 C2 67 00` (6). Used e.g. for USER_FIRE_WEAPONS (0xC7) at 0x51D115 |
| 0x67C258 | keymap list sentinel; node+8 = entry {cmd, ids_name, ids_info, event mask (1 down, 2 up, 4 repeat, 8 shift_ignore), ?, keys_begin, keys_end}; key = {int key (vk, or -1 L/-2 R/-3 M mouse), u32 mods} |
| 0x614DD8 | `const char*` command name table (index = command id, 0..0xCB; USER_NONE=0 … USER_SCREEN_SHOT=0xCB) |
| 0x67C3A4 | handler registry (map cmd → handler objects; handler vt[0](cmd)) |
| 0x67C3AC | keyboard focus handler (ptr into the modal window object that owns keyboard focus) |
| 0x577F50 | translate key binding to display text (FLUF) |
| 0x576B60 / 0x5772D0 | keymap load / save (FLUF) |
- Raw keys go first to UI as `HandleKey(USER_KEY=1)` (vk in 0x67C27C); if unhandled, bound commands
  are dispatched. Menus (options, dialogs) react to raw Escape; bases have no Escape menu —
  the game menu is `USER_PAUSE` (F1). Commands with no key bound can still be fired via HandleKey.
- Send injected commands with their keymap event mask: keydown-only commands toggle twice if they
  also receive the key-up.

## Flight
| Address | Meaning |
|---|---|
| 0x4C7CB0 | mouse-flight steering, once per frame (called from 0x54B0E9). Prologue `83 EC 18 53 56` (5) |
| | active if LMB held (0x616850 bit0) or toggle u8 0x67354C; engage after timer 0x673550 ≥ 0.25 (0x612270) or drag; [0x673554] engaged flag; press pos 0x673558/5C |
| | direction = ((mx − W/2)/(W/2), (my − H/2)/(H/2), 0) (invert-Y option 0x17 via 0x5792B0) → `IBehaviorManager::update_current_behavior_direction` |
| 0x673550 / 0x673554 | float mouse-flight timer (engaged when ≥ 0.25 s) / u8 engaged flag. Code outside the steering function (e.g. 0x4C86DC, the HUD "MOUSE FLIGHT" label) treats timer ≥ 0.25 as "mouse flight on" |
| 0x4C7C80 | `bool IsMouseFlightToggleOn()` |
| 0x4C7AF0 | `void __cdecl SetPlayerThrottle(float -1..1)` — stores 0x673564, updates behavior throttle, cancels cruise at 0 |
| 0x4C7900–0x4C7BA0 | wrappers: direction, afterburner, auto level, auto avoidance, slide strafe burst(StrafeDir), brake reverse, cruise, engage engine, throttle (+getter fld [0x673564] ~0x4C7A20) |
| 0x51A7xx | camera/turret look controller (not ship steering) |
| 0x51A7D0 | the turret camera's look update (`this` = the camera, `0x54BC50` returns the turret camera 0x678F60 = `exe::CamTurret`; turret view is on exactly when CurrentCamera 0x679738 points at it - FLvibed's turret flag follows that each frame, 26 Oct 2026): while mouse flight is engaged (LMB 0x616850 bit0 or the toggle 0x4C7C80) after its delay, yaw / pitch move by the cursor's offset ((mx - W/2)/(W/2) x [+0x128] x dt, likewise y with [+0x12C]); otherwise by the look inputs [+0x11C] / [+0x120] x the same rates; the result is smoothed toward the target angles (0x51AA47, factor dt x [+0x130]); pitch clamped to +-1.047 rad |
| 0x51AC20 | the look commands' handler (sub-object at camera +0xF8, registered for commands 0x32..0x36 at 0x51A640): USER_LOOK_ROTATE_CAMERA_LEFT / RIGHT / UP / DOWN (50..53) set the yaw (+0x24 = camera +0x11C) / pitch (+0x28 = +0x120) input to -/+ the rate [+0x2C] while the event's key is down and 0 on its key-up; RESET (54) zeroes +0x14..+0x20. keymap.ini gives them `state = keydown, keyup`. A key-up lost (sent while a window has the cursor) leaves the camera turning: FLvibed sends the four key-ups again when flight resumes and at every turret toggle (verified by the owner 26 Oct 2026) |
| 0x40F4F0 | WorldToScreen `(__stdcall)(*(u32*)0x6164DC, Vector& out, Vector& in)` (FLUF) — returned "not visible" for every point when called from a FrameUpdate detour (not understood); projecting with the camera fields below works |
- Steering response: turn rate = offset × 1.3 × max_rate, max_rate = steering_torque / angular_drag
  (shiparch.ini; Starflier 25000/15000 = 1.667 rad/s = 95.5 °/s, measured 91–94). Offset is
  ((mx−W/2)/(W/2), (H/2−my)/(H/2)); both axes saturate at ~0.77, linear below. Measured on the Starflier only.
- When the steering input stops, the ship keeps the last direction: send a neutral direction to stop turning.
- The steering function returns early while any UI control is hovered (0x59DAA0 returns the hovered
  control, 0x67DCD8) — also with the Space toggle (0x67354C) on, which does not disable HUD hover:
  mouse players cannot steer or aim through the HUD.
- Guns fire at the cursor (aim circle), not along the nose: with the chase camera the screen center is
  not the nose direction. Aiming = putting the cursor on the lead ("prediction") cross.
- Cursor over a space HUD button (maneuver bar etc.) switches the cursor to `arrow`; held fire there
  clicks the button (e.g. Cruise).

## Cameras and projection
- 3D camera objects (current one at [0x679738], e.g. chase 0x678D48) share the UI camera layout: +4 3x3
  rotation (columns = camera axes, looks along −z), +0x28 position, +0x68/+0x6C pixel center, +0x78/+0x7C
  scale (±2056 at 2160p), +0xA4 near plane. pixel = center + scale × (view.xy / −view.z), view = Rᵀ(p − pos).
- UI camera: see 0x67DBF8 below (same formula, scale ±2800 at 2160p).

## Objects (IObjInspect / CShip)
- IObjInspect vtable: vt[0] get_position, vt[1] get_velocity, vt[2] get_angular_velocity, vt[3]
  get_orientation (Matrix, columns = ship axes, forward = −z), vt[11] `get_physical_radius(float&, Vector&)`
  (police fighters ~8–10 m), vt[26] `get_attitude_towards(float&, const IObjInspect*)` (target → player;
  hostile police after console `rep l H` read −0.60..−0.90, friendlies +0.65), vt[29] get_target(IObjRW*&)
  (the IObjRW* doubles as a target identity), vt[84] cobject().
- Vector returns are thiscall with a hidden result pointer (call as `__fastcall(this, edx, out)`).
- vt[23] is NOT is_cruise_active(bool&) (overload order differs; calling it faulted). vt[66]
  get_tgt_lead_fire_pos(u16 gun sub-obj id, Vector&) → −2 with id 0.
- Lead point (HUD red prediction cross): Common.dll `?get_tgt_lead_fire_pos@CShip@@QBE_NAAVVector@@@Z` on
  the CShip from cobject(); returns false when out of gun range.

## Targets, contacts, docking (verified in game 17 Oct 2026)
- **Control exclusion** (22 Oct 2026; Common.dll `CEqObj::set_control_exclusion` 0x4C2E0 writes `CEqObj+0x190`,
  `is_control_excluded` 0x4C2F0 tests a mask - verified in the HDE 0.7.1 Common.dll; `space::PlayerControlExclusion`,
  event `control-exclusion a -> b`). The game's sequences lock the player out by it: **any** dock maneuver's start sets 4
  (space state 0x54B4A6, with the letterbox and the docking camera when the port type has 0x120 = docking ring |
  station), a launch 6, a jump and a landing 7, a trade lane 3. Its end clears it: a launch's end, a jump's state 5 -> 0
  (0x54384B), a lane's end, leaving space (DestroySpaceState 0x54AF86). In space the letterbox and the player's steering
  follow bit 4 (0x51D0B4: excluded -> letterbox in, cursor off; not -> out). **Airlocks** (solar type AIRLOCK_GATE, bit
  0x400; dock point type 8: the Dyson sphere door, the Nomad lair and core) switch the system without any of those
  sequences (SystemSwitchOut 0x53D63E: only gate types 0x40 / 0x800 jump out; else straight to the switch, then
  `CShip::airlock_in` = full throttle + beam): a Dock maneuver into one leaves exclusion 4 for good - letterbox, no
  steering, the ship flying straight (M13 measured 0 -> 4 at the airlock dock, 4 to the mission's end; M12's lair the
  same). The player flies into the opening door instead (2020 footage M13 19:41-19:43, M12 5:12 / 5:52: HUD up, no
  docking camera): crossing the docking sphere (`docking_sphere = airlock, HpDockMountA, 233`) under the mission's dock
  grant switches the system. Sources: Freelancer-Decomp (space_state.cpp, space_camera_game.cpp, client_gameobj.cpp,
  client_iclient_space.cpp, common_cship.cpp), FLHookSDK Enums.hpp (ObjectType, DockType Airlock = 8).
- **Target cycling walks the HUD contact list** under its current filter (`ImportantFilterButton`, `ShipsFilterButton`,
  `SolarFilterButton`, `LootFilterButton`, `AllFilterButton`): USER_NEXT/PREV_TARGET (0x1D/0x1E) are handled at
  0x4D3E87.. on the contact list control (rows: std::list at control +0xB0, row +0x10 object id), which selects a row
  with 0x4CFFA0 `thiscall(index)`. With the Important filter and nothing important around, the list is empty and the
  commands are not handled (`no target`). Never rely on the filter the player picked.
- **Selecting a target natively, filter-independent:** 0x54BCB0 `bool cdecl(action*)` is the game's object action
  dispatcher (current camera vt[0x70]/vt[0x74], then the player ship's controller vt[0x1CC]); the contact list sends a
  0x28-byte action {+0 0x1C USER_SET_TARGET, +0x10 space object id, +0x14 Common.dll SUBOBJ_ID_NONE, byte +0x26 = 1}.
  Rows of type 4 (0x4C46A0: 24-byte entries at 0x672978, count 0x673374) and 3 (list at 0x672950) are other sources.
- **Objects of the system** (client side, filter-independent): Common.dll `CObject::FindFirst(Class)` / `FindNext()`
  (classes CSolar 0x303, CShip 0x503; CObject +0x50 system id), `CSimple::get_id` / `get_type` (archetype type bits:
  planet 2, docking ring 0x20, jump gate 0x40, trade lane 0x80, station 0x100, ...), `CEqObj::get_name` (string id:
  "Fort Bush"; NPC ships give a pilot-name format string), `is_dock` (dockable: bases, docking rings; not lane rings),
  `get_dock_target` (the base it leads to), `is_targetable`; position server.dll `pub::SpaceObj::GetLocation(id, Vector&,
  Matrix&)` (its result code is not 0 on success: check the vector). The player ship id at 0x673378 outlives the ship
  after a death: check GetPlayerIObj 0x54BAF0. Solar ids are fixed (Fort Bush 2661040712, Baltimore Shipyard
  3197945416, Manhattan Docking Ring 2755957258), the player ship id changes with each load.
- **Docking:** USER_MANEUVER_DOCK (0x79) on the current target is a **toggle**: a second press cancels a running dock;
  a press without a target is handled and arms nothing visible. "Handled" does not mean the request went out: the
  base answers over comms (`pub::SpaceObj::SendComm` from the base to the player ship: "Your request to dock is
  granted. Proceed to dock 1."). Then (user, 17 Oct 2026) the docking queue clears, the dock autopilot flies to the
  docking animation start point (mode space, ~80 m/s, cruise is ignored: Fort Bush from 2 km 24 s), then the docking
  cutscene runs on the **docking camera 0x679634** (IsPlayerInCutscene set; Fort Bush ~15 s) and the player is docked.
  USER_CANCEL during the docking cutscene skips it (docked 0.3 s later, room ready 0.7 s later, no pause menu).
  Planets land through their docking ring the same way (Manhattan: 8.6 s from the launch position, skipped). Neither
  the client's (`CEqObj::get_dock_target` of the player ship) nor the server's (`pub::SpaceObj::GetDockingTarget`)
  dock target changes on a request. The key is refused (not handled) while the ship is still leaving a dock after a
  launch. The base just left can be docked again at once (2.5 s).
- **Docking geometry and where the cutscene starts (research 18 Oct 2026, user idea: the player's fast dock).**
  Common.dll `IObjInspect::get_dock_hardpoints(int index, TERMINAL_TYPE*, Transform* mount, Transform* p1, Transform*
  p2, float* radius)` (virtual, by name: core\fldll.h; Transform = 3x3 rotation then position; debug `dockpoints` on
  the current target) gives every bay: a straight approach line p2 (outermost) -> p1 -> mount. Data: solararch.ini
  `docking_sphere = <type>, HpDockMount*, <radius>, <script>`. Fort Bush: 6 bays, 3 of type 1 (player berths, radius
  10, p2 ~265 m out) and 3 of type 4 (larger); Baltimore Shipyard type 1 x2 (4.5 m), type 4 x3, type 5; Manhattan's
  docking ring type 6 (40 m; p2 112 m out). Trade lane rings answer -4 (no dock hardpoints). Gates not read yet.
  IBehaviorManager::get_docking_port(IObjRW*&, int&) (debug `ai dockport`) holds the assigned bay.
  **Normal dock at Fort Bush from 2 km (SaveC702):** op 7 for 24.6 s at ~80 m/s, no port held during the approach;
  bay 2 (the one facing the ship) appears at **p2** and the docking cutscene starts 0.1 s later - 11 m from p2, 272 m
  from the mount. So the animation starts at the bay's outer approach point p2.
  **Shortcut test:** the game's goto to 30 m outside bay 2's p2 took 20.3 s (cruise charge, slow arrival) and stopped
  161 m short (the HUD drops a waypoint within ~150 m); from there the request reached the cutscene in 4.1 s - total
  24.4 s, no gain. The gain needs a player-style approach (full throttle / afterburner, braking late, the request just
  before p2). Open: how the game picks the bay (nearest free player-size berth to the ship?), the grant's latency,
  whether a request while still moving fast keeps the speed, rings / gates / holes, occupied bays (queue).
  **Fast dock results (autopilot fast_dock, 18 Oct 2026, Fort Bush from SaveC702):** only the position at the request
  matters (user): requested early, the game's dock takes the ship and slows it (full afterburner, request 120 m before
  p2: 4.2 s to the cutscene; 80 m 3.2 s; 40 m 0.8 s; 10 m / at p2 0.3-0.4 s; 20 m past p2 2.7 s - it turns back). Best:
  full speed, request 0-10 m before p2, no braking: 10.3 s total vs the game's dock 24.6 s (Baltimore Shipyard 17.7 vs
  24.8). The game assigned the bay whose p2 is nearest every time so far. New York - Colorado gate: one bay, type 7,
  radius 225, p2 ~460 m before the mount.
- **Flight model (core\game\flight.*, debug `perf`; game data, verified against real flight 18 Oct 2026):** Common.dll
  `CShip::collect_force_components(throttle, ThrustEquipType 1 normal / 2 thruster / 4 cruise, F&, drag&)` and
  `PhySys::GetMass`: m dv/dt = F - d v, top speed F / d exactly (Starflier: engine 48000 / 600 = 80 m/s, + afterburner
  120000 = 200, full reverse -9600 = -16; mass 100, tau = m / d = 0.17 s; or_elite mass 150). Measured: afterburner
  alone from rest (throttle 0: 72000 -> top 120) 111.8 at 0.42 s, model 110; full reverse from 120 m/s 63 / 29 / 6.5 at
  0.11 / 0.20 / 0.31 s, model 54 / 25 / 5 (one frame later). **Cruise does not follow the force law:** CEEngine
  GetCruiseChargeTime 5 s at the engine's top speed (IsCruiseEngaged true from the request), then linear 60 m/s^2 =
  EngineEquipConsts CRUISING_SPEED / CRUISE_ACCEL_TIME (300 / 5) to CRUISING_SPEED 300; leaving it: the force law again
  (280 -> 182 at 0.11 s, model 183). Straight runs from rest (flight::PlanRun, simulated with these laws): afterburner
  wins below ~2.5 km on the Starflier, cruise beyond (2 km 11.4 vs 12.7 s; 5 km 26.4 vs 22.7). The fast dock's
  predictions matched its runs within 0.1 s while lined up from the start; ~1 s short with a turn at the staging point
  (turns are not modelled yet). CShip::get_time_to_accelerate / get_distance_travelled take the throttle first.
- The planet launch's short cutscene after the launch sequence also uses the docking camera; a USER_CANCEL there did
  not open the pause menu (M01 launch).
- **Content.dll is unloaded and reloaded by the game** (seen when loading a game from the death dialog): IAT patches
  in it are lost and must be re-applied (FLController `events::Update` re-checks the import slot every second).

## Ray test: the real hulls, moving parts included (verified in game 23 Oct 2026; code core\game\space.cpp)
- Common.dll `PhySys::FindRayCollisions(system, from, to, RayHit*, n)` returns hits {CObject*, position, velocity, crc}
  on the real collision hulls (`space::RayHitDistance`, `SightBlocker`; debug `ap ray`, `ap scan`, `ap rayraw` = every
  raw hit with its object class, id and crc).
- A solar's **moving part** (a `Cons\Rev` joint driven by the model's animation script) answers as **its own object of
  class 0x1** (CObject +0x4C), not as the solar (0x303): the M12 core's reactor wheel gave id 4284637536, crc
  0x02CE3710. Its hits follow the animation in real time (rings of rays 1 s apart: the six spokes moved ~37 deg/s =
  the keyframes' 36 deg/s). Before 23 Oct 2026 the filters kept only 0x303 / 0x1003 and the wheel was invisible - the
  user's ship struck a spoke where the scan read "clear". Class 0x1 now counts as solid.
- Animation keyframes are in the .cmp: `Animation\Script\<name>\Joint map N\Channel\Header` = (frame count, interval,
  type) and `Frames` = (time, angle in rad) pairs for a Rev joint; `Cmpnd\Cons\Rev` = parent[64], child[64], origin,
  offset, rotation (3x3), axis, min / max (`tools\utf-list.ps1 -Extract`).

## Spawning ships (verified in game 18 Oct 2026; code core\game\spawn.cpp, debug `spawn`)
- server.dll `?Create@SpaceObj@pub@@YAHAAIABUShipInfo@12@@Z` with pub::SpaceObj::ShipInfo (0xC0 bytes, FLHookSDK
  layout; OwnerList<CargoDesc> = vptr + 16 bytes left zero), the NPC template from DATA\MISSIONS\npcships.ini
  [NPCShipArch] (ship_archetype, loadout, level dN, pilot, state_graph), reputation by `?Alloc@Reputation@pub@@YAHAAH
  ABUFmtStr@@1@Z` (FmtStr: Common.dll ctor `??0FmtStr@@QAE@IPBUVal@0@@Z`, begin_mad_lib 16163 = "%s0 %s1") +
  GetReputationGroup / SetAffiliation; personality by pub::AI::SubmitState(SetPersonalityParams). Creates ships
  reliably (any template; hostile attitude by faction, or fc_uk_grp + SetAttitude(npc, player, -0.9)).
- **ShipInfo.flag:** Create returns -4 for a flag that does not fit the current game session: 5 works in a fresh
  process, after a save reload another value (seen 267, 310, 325 - near the current object ids); the spawner probes
  (a refused Create creates nothing) and keeps the accepted one. The accepted flag climbs with the session's object
  ids (19 Oct 2026: ~4090 after ~2 h of test spawns, ids past 4400 - a probe capped at 4095 then failed every spawn);
  the probe now runs upward from the last accepted flag, wrapping at 65535.
- **Spawned NPCs fight when their job's scene_toughness_threshold allows it (solved 18 Oct 2026).** A lone spawn
  with basic_job_no_formation (threshold HARD) of level 1-4 never picked the player (it judges the scene too tough;
  encounter NPCs come in groups); the same spawn engages within ~1 s with levels >= 5 (tested 5, 10, 19, 30, 50), or
  at its template level with a HARDEST job (basic_job_no_formation_no_flee, job_light_fighter_a) - with or without
  the Trail order and the forced target, in a private faction (fc_uk_grp + SetAttitude -0.9) or a real hostile one
  (fc_lr_grp). Spawner default since: basic_job_no_formation_no_flee. Test variants: `spawn ... job= pilot= order=
  base= force= level=`. The notes below are the earlier search.
- Earlier (before the fix): spawned NPCs did not start fights. They fly / trail when ordered (SubmitFor Trail with fireWeapons), get
  the template's pilot (fixed: angles in radians), a solo job (basic_job_no_formation; templates' pilots often carry
  basic_job_formation with force_attack_formation = True, wingmen that fight only under an encounter leader) and
  allow_player_targeting, a forced target (`?submit_forced_target@AI@pub@@YAHII@Z` returns 1) - and still never pick
  the player within 15 s, near Manhattan or in a Rogue zone. Once (17 Oct, first test) police spawns joined a fight
  their patrol had started; once a spawn given a live Rogue's raw personality (`spawn brainfrom`) engaged in the
  Rogue zone. Raw diff of a live police NPC vs the file-built personality (`ai rawdiff`): non-zero words at +0x108,
  +0x228, +0x270, +0x29c, +0x35c, +0x39c..+0x3c0 that the field table never sets (some look like pointers). Next
  step if needed: trace how the encounter system initialises its NPCs (zone behaviour, encounter brain).
- [Pilot] files give angles in degrees; the game's Personality holds radians (live: trail_lock_cone_angle 0.5236
  for 30). ai::LoadPilot converted nothing until 18 Oct 2026 (fixed).

## Weapons, munitions, countermeasures (verified in game 17 Oct 2026 unless marked; code core\game\space.cpp)
- **Own guns / launchers:** `CEquipManager` at CEqObj +0xE4; Common.dll `??0CEquipTraverser@@QAE@H@Z` (thiscall,
  EquipmentClass; Gun = 1 << 7 also covers missile launchers, CM = 1 << 6, Mine = 1 << 5 - `space::PlayerMines` counts
  the mine droppers' ammo as the countermeasures are counted, verified on Save1148: HpMine01 ammo 23) + `?Traverse@CEquipManager@@QAEPAVCEquip@@AAVCEquip
  Traverser@@@Z` walk them in the ship's weapon order. CEquip archetype +0xC, isActive +0x10; CELauncher projectile
  archetype +0x4C, refireDelayElapsed +0x50 (seconds since its last shot, live); Archetype::Launcher power_usage
  +0x8C, refire_delay +0x90, muzzle_velocity +0x94 (match weapon_equip.ini for li_gun01_mark01: 5.09 / 0.12 / 750).
  `USER_FIRE_WEAPON<n>` fires one weapon alone - **not** in the traverser's order (corrected 18 Oct 2026: on the M02
  ship the traverser lists the Javelin first, the game fires it on slot 2): slot n is entry n-1 of the HUD status
  window's list. "HUD_Status" control (FlControl name +0x18; the named control is the object's sub-object at +0x38,
  vtable 0x5D8504; constructor 0x4DA170 registers the commands); its command handler 0x4DBA50 switches on cmd - 0x3A
  (byte table 0x4DC994, jump table 0x4DC8A4; USER_FIRE_WEAPON1 0xA1 -> 0x4DBEC2) and fires entry n-1 of the vector
  at object +0x590 / +0x594 = control +0x558 / +0x55C: 36-byte entries, u16 sub-object id first (M02 ship: 39 gun
  HpWeapon01, 38 Javelin HpWeapon02, 42 gun HpWeapon03, 43 other equipment). Hardpoint name order matched it there.
  CExternalEquip mountedHardpoint (char*) +0x20. **USER_FIRE_WEAPONS (fire all) fires the missile launcher too** (a
  launcher in the active group: 3 s held launched a Javelin, 18 Oct 2026) - the autopilot holds each gun's own slot.
- **Munition flight (live archetype, matches weapon_equip.ini, 18 Oct 2026):** Projectile lifeTime +0x70 (13.75);
  Munition motorId +0x8C, seeker +0x90, seekerRange +0x98 (1000), **seeker_fov_deg stored as its cosine** +0x9C
  (35 -> 0.819152: a half-angle), maxAngularVelocity +0xA0 (2.90333); Common.dll `?GetMotor@Archetype@@YAPBUMotorData
  @1@I@Z` (motor id) -> delay +8, accel +0xC (22), lifetime +0x10 (2.75).
- **Munitions:** Archetype::Munition hull_damage +0x80, seeker +0x90 (0 none, 1 dumb, 2 seeker), cruise_disruptor
  +0xAC (byte); Archetype::Root explosionArchId +0x24 -> Common.dll `?GetExplosion@Archetype@@YAPAUExplosion@1@ABUID_
  String@@@Z` -> Explosion radius +0x4, hull_damage +0xC (missiles carry their damage there: missile01_mark01 489,
  radius 16; offsets from FLSDK, the explosion read is not yet confirmed in game).
- **Missiles in flight:** CObject class CGUIDED_OBJECT 0xA7 (FindFirst/FindNext like ships); CGuided::get_target
  (`?get_target@CGuided@@QBEPAUIObjRW@@XZ`, IObjRW vt[8] = get_id), CProjectile owner +0xE4, archetype CObject +0x88;
  position by pub::SpaceObj::GetLocation. Police missiles homing on the player read as seeker 2 (homing).
- **Countermeasures** (DATA): [CounterMeasure] range 1000 m, diversion_pctg 70, lifetime 3 s; dropper refire 0.25 s.
  They divert homing munitions within their range by chance (data meaning; mines not verified). Cruise disruptors:
  seeker range 2.5-3 km, detonation 12.5 m, motor 0.9 s at 523 m/s2. (HDE ge_s_cm_03: range 1000 m, diversion 90 %.)
  The Nomads' `nomad_cruise_disruptor01` (session 11, DATA): seeker 2,700 m / 120 deg, muzzle 360 m/s + motor 1,152 m/s2
  for 0.625 s (~1,000 m/s after), lifetime 3.125 s, blast 25 m / 530 hull, gun refire 0.5 s - inside ~3 km no charging
  ship outruns it; in M12's Omicron Minor 13 of 38 runs had charge resets, ~5 s each (`docs\LEG_DOSSIER.md` M12).
  Mines `mine01_mark01` (DATA): seek_dist 400, top_speed 60, lifetime 10 s, owner_safe_time 4 s, blast 16 m / 326 hull.
- **Missile launch speed (measured 18 Oct 2026, Javelin, `missiles` positions):** the munition inherits the ship's
  velocity: v = v_ship + muzzle (30) + motor accel x t (22 m/s2 for 2.75 s), then constant (no drag): at rest 32 ->
  91 m/s; launched at 116 m/s 149 -> 207-212 m/s (116 + 30 + 60.6 = 206.6).
- **Engine kill (USER_MANEUVER_ENGINEKILL, measured 18 Oct 2026):** no drag while the engine is killed - the speed
  is held (120 m/s after the afterburner: 110-124 for ~8 s; 106 m/s from cruise: 104-108); engine back on at zero
  throttle: 0 within ~0.5 s. Cruise ends (cruise=False). **Engine kill and the cruise charge (measured 23 Oct 2026, M12
  ship after the Osiris launch, motion trace 0.5 s, the user's disrupted-cruise idea):** after the afterburner (160-200
  m/s), engine kill holds ~93-100 m/s; a cruise press then turns the engine back on - 89 -> 80 m/s within 0.5 s, the
  charge runs at the engine's 80 m/s (charging ~5 s, then the ramp to 300 m/s in ~3 s); an engine kill during a charge
  cancels the charge (engine state 0) and the ship coasts at ~100 m/s. The speed during a charge is the engine's,
  whatever came before (afterburner then plain charge alike: 200 -> 105 -> 80). The keyboard command USER_ENGINE_TOGGLE
  (same day) is no speed-holding kill here: after the afterburner the speed fell to the throttle's 40 m/s within 1 s; tapped
  0.3 s or 3 s into a charge it changed nothing (80 m/s, cruise speed ~8.5 s after the press as without it). Open: the afterburner held ~120 m/s by position sampling
  while `perf` gives a 200 m/s burner top (force 120000 / drag 600) - check with the flight recorder.
- **Prices:** Common.dll `?find_by_archetype@GoodList@@YAPBUGoodInfo@@I@Z` (cdecl, archetype id) -> GoodInfo, price
  float +88 (FLSDK Goods.hpp): the goods the game loaded (every goods file, mods, binary INIs). Javelin ammo
  missile01_mark01_ammo: 25 credits (vanilla weapon_good.ini), sold in 170 vanilla market entries.
- **Loot drops (HDE 0.7.1 Content.dll, disassembled 18 Oct 2026; matches adoxa's KnowledgeBase formula):**
  `[mLootProps] drop_properties = chance%, min_worth, worth_mult, min, max1, max2` loaded at 0x6F04CB0 (FLUF: content
  +0x64CB0) into 28-byte entries {id, chance x 0.01 (+4), min_worth +8, worth_mult +0xC, min +0x10, max1 +0x14,
  max2 +0x18} at the vector 0x6FD05F0; lookup by id 0x6F05060, defaults (default_equipment / default_commodity)
  0x6F05000 / 0x6F05030; the roll in 0x6F5D0F0..0x6F5D26B, per item the dying ship carries (`carried` = its count):
  `worth` = a float sum over a list (call 0x6F48070 per element; presumably the involved players' worth - not
  decoded) -> int; worth < min_worth: 0 drops; else n = worth_mult ? min((worth - min_worth) / worth_mult, carried)
  : carried (0 when worth = min_worth); n = min(n + min, max1); n = min(n, carried); p = chance x n;
  drops = floor(p) + (rand()/32768 < frac(p)); drops = min(drops, max2). Expected drops ~ min(p, max2). Javelin ammo
  `5, 5000, 500, 0, 40, 15`: 0.05 per 500 worth above 5000, up to 2 per kill (n 40) - capped by what the ship carries.

### Turret reach (verified in game 22 Oct 2026; space::GunsCovering, debug `cover <id> [dist..]`)
- Common.dll `CEGun::CanPointAt(const Vector& point, float tolerance)` (0x6298860, thiscall): the direction from
  `GetTurretOrigin()` (Vector by hidden pointer) to the point in the turret frame (`ComputeTurretFrame`), its two joint
  angles against the joint limits (joints at +0x1FC / +0x218, -1 = none: the gun's axis only) widened by `tolerance`
  (rad). A first virtual call (+8) that returns true makes it false. `GetMunitionRange()` = the projectile's reach.
- Guns are found through the CEqObj equip manager (+0xE4, CEquipTraverser class bit 7) of a CShip **or a CSolar**
  (M13's `nomadcap` is a MISSION_SATELLITE solar `d_n_battleship`, 120000 hit points, 15 `nomad_turret01_mark01`).
- Measured on `nomadcap` (26 directions in its frame x 300 / 600 / 900 / 1200 m): no gun reaches 300 m out along its
  long axis (fore and aft) or 300 m up-aft; elsewhere 2-7 of the 15 at every distance - the blind spots are close in,
  as the user's 2020 M13 footage shows (parked inside the hull envelope, ~45 s kill at full hull).

### Gun rounds and shield damage (verified in game 19 Oct 2026; space::IncomingRounds / TrackRounds, debug `rounds`)
- Gun rounds are CBeam objects, CObject class 0x67 (CBEAM_OBJECT = 0x60 | the projectile mask 7), in the class list
  `CObject::FindFirst/FindNext` walks - but they do NOT carry the system id at CObject +0x50 (none matched while
  firing): collect them unfiltered. Common.dll exports read them: `CBeam::get_last_pos`, `get_velocity` (by value),
  `CProjectile::get_owner`, `CBeam::munitionarch`.
- A round's straight path vs the ship's own box (`CObject::get_surface_extents`, virtual - the CShip override; the
  Starflier (-5.3,-3.9,-4.2)-(5.3,3.5,9.0)) + 1 m predicts hits: 96 of 96 rounds tracked to their closest approach
  vanished then (NPC gunners aim at the centre: 0-3 m).
- **Shield damage per hit = (hull_damage x constants.ini [ShieldEquipConsts] HULL_DAMAGE_FACTOR (0.5) + energy_damage)
  x weaponmoddb.ini shield_mod (weapon_type vs the shield's shield_type, e.g. W_Laser01 vs S_Graviton01 0.8)**:
  predicted / measured shield + hull loss 796 / 823 (1.03) under 2 police fighters (hull_damage alone: 0.41).

### Hit effects (verified in game 19 Oct 2026; table docs\kb\hit_effects.md, tools\hitfx.js / aledump.js)
- A projectile hitting a shield draws the shield's `shield_hit_effects` effect (st_equip.ini, picked by the hit's
  damage: 0 / 100 / 500) AND the munition's `munition_hit_effect`; on a bare hull (shield offline) only the munition's.
  Captured at ~150-185 fps (subtitles debug `grab ... [step]`, controller debug `shot`: research\experience\shield_hits).
- Alchemy (.ale) name hashes ("FLCRC32": Node_Name, property names, effect names = [VisEffect] `effect_crc`) are CRC-32
  (init / final xor 0xFFFFFFFF) over **alchemy.dll's own table** (t[1] = 0x09073096; the standard table's is
  0x77073096) - no standard CRC-32 matches. The KnowledgeBase's `Emitter_InitialLifeSpan` is `Emitter_InitLifeSpan`.
- server.dll `pub::SpaceObj::DrainShields(uint id)` takes a shield offline at once (GetShieldHealth: ~0, up = 0); it
  rebuilds slowly and is up again only later (not within 40 s in one test). HDE Console's `draintgt` did nothing here.
  Code space::DrainShields, debug `drainshield <id>` / `shieldof <id>` (test aids).

## Lane permission (Act_PlayerCanTradelane; code 19 Oct 2026, space::LaneOpen, debug `laneopen <ring> [toward]`)
- A lane request reaches Content.dll's handler (HDE 0.7.1: 0x06F6FCB0, called from the server-event switch at
  0x06F5E07E) with `{ship id, ring id, toward ring id}`. It gets the mission's per-player state by ship id
  (0x06F49BC0, cdecl(const uint*), null = no mission state: granted) and asks `check(state, ring, toward)`
  (0x06F47EE0, thiscall, ret 8); 1 or 2 -> it answers `pub::SpaceObj::LaneResponse(ship, 1)` = denied, else it
  schedules the dock (response 4). FLvibed finds the handler by signature and takes both helpers from its own call
  operands (space::LaneOpen).
- `check`: the result starts from the state's byte +0x34 (the mission's global "player can tradelane"): false -> 1
  (denied). A ring-pair list at +0x3C..+0x40 (8-byte entries, filled by `Act_PlayerCanTradelane = false, A, B`)
  inverts that for a listed pair: **`false, A, B` = all lanes closed EXCEPT A <-> B** (read from the code; the runbooks'
  open question). Verified in game: free roam (SaveC705) every ring 1; M03 SaveS009 every one of 51 rings 0 (the scene
  where a lane to California Minor was denied); **M03 step 5** (campaign Save1e7d, after objective 22430, the mission's
  `false, Ring_42, Ring_43`): 42 -> 43 = 1, 43 -> 42 = 0, every other lane 0 - the exception is one direction, A -> B.
- The travel planner leaves out a lane whose every ring pair (its direction) is denied (route.cpp Closed): S009 plans
  a direct flight, free roam still uses lanes.

## Maneuvers, waypoints, lanes, jumps, zones (verified in game 17 Oct 2026 unless marked)
- **Player maneuvers are AI directives.** The player ship's maneuver handler 0x4D8E20 (thiscall(action, ..), a switch
  over the maneuver) builds a `pub::AI::Directive*Op` (Goto 0x4D902F, Face, Trail, Strafe, Formation 0x4D91E5) and
  calls `IBehaviorManager::set_current_directive` on the ship's behaviour manager (`CEqObj::get_behavior_interface` of
  the player CShip). **Autopilot state:** IBehaviorManager +0xC0 = `pub::AI::OpType` of the running behaviour (FLSDK
  AIEnums: 2 Goto, 7 Dock, 9 InstantTradeLane, 10 Formation, 19 FreeFlight), +0xBC = maneuvers locked. The op changes
  in the frame of the key press. Seen: free flight 19 at rest; **Goto 2** while the goto runs (the autopilot switches
  cruise on and off itself), back to 19 on arrival; **Dock 7** from the dock request through the whole approach and
  **the whole trade lane ride**; Formation 10 (see below). Harness: debug `pilot`, `Get-FLPilot`.
- **Goto (USER_MANEUVER_GOTO) on the target** flies there with cruise and ends by itself: ~140 m from a waypoint
  (stop distance 100 m for waypoints, type 0x200, 0x4D9111), ~360 m from a station's centre (Missouri, from 6 km:
  31 s). Without a target the handler builds a goto to a point ahead of the ship. The goto flies **straight through
  hazards** (no zone avoidance seen).
- **Waypoints:** the player's waypoint list is a global array of 24-byte entries at 0x672978 (position, +0xC system
  id, +0x10 object id or 0, +0x14 sequence number), count 0x673374, at most 99. `0x4C4780 bool cdecl(const Vector*, u32
  system, u32 object)` appends (the nav map's Set Waypoint: 0x493A7C, system = the shown system), `0x4C46C0` clears all
  (also drops the waypoint object, 0x4F4CD0), `0x4C46E0 void cdecl(int index)` removes one (0 <= index < count; the
  later entries move down - `exe::RemoveWaypoint`, `space::RemoveWaypoint`, verified 25 Oct 2026: a plotted 3-entry
  route to the Colorado gate -> 2 on the pad's X), `0x4C46A0` returns entry i,
  `0x4C4690` the count. `0x4CEEB0 void cdecl(int index)` selects entry i the way a click on its contact list row does:
  with an object id it targets that object, otherwise the HUD waypoint manager ([0x674BC8], 0x4F42E0) creates a client
  solar of archetype `waypoint` named `waypoint_target%d` (space id = CreateID of that name; type bit 0x200) and
  targets it (action 0x1C). On arrival the HUD removes the reached entry, queues an NN line and selects the next one
  (0x4F4058..0x4F4116). Mission waypoints arrive through 0x53C370 (clears the list, then appends). Server-side
  `pub::SpaceObj::GetLocation` does not know the waypoint object (its contacts distance is wrong): use the target's
  position (`targetinfo`). Harness: debug `waypoint x y z` / `waypoint clear`.
- **Trade lanes = the dock maneuver on a lane ring** (a lane ring's `is_dock` is false, the dock maneuver still works):
  op 7 during the approach (cruise on), then `CShip::is_using_tradelane` true with
  `get_tradelane_entrance_ring` = the ring entered and `get_tradelane_exit_ring` = the next ring ahead (it reads the
  entry ring itself in the first frames, then advances ring by ring); at the end of the lane both go to 0 and the op to
  free flight. No comm reply to the player for a lane. Li01 ring 178 -> 169: approach from 3.7 km 37 s, ride 35 s.
  **Direction (user, 17 Oct 2026): set by the angle from which the ship docks once in range of the docking animation
  start point**: approaching ring 170 from the side away from 171 rode 170 -> 178, ring 177 from the side away from
  176 rode 177 -> 169 (verified both ways). Code: `IObjInspectImpl::get_lane_direction(const Transform& ship, bool*)` on
  the ring = the sign of (ring - ship) . ring z axis (Common.dll RVA 0x60E30).
- **Lane ride timing (research 18 Oct 2026, flight recorder `rec`, matches Common.dll's LaneAction update RVA
  0x619D0):** lanes = system file `[Object]`s with `Archetype = Trade_Lane_Ring`, `prev_ring` / `next_ring` (chain; an
  end ring lacks one), rings ~7.5 km apart. The ride moves the ship by position (`get_velocity` reads ~2 m/s: use
  position deltas). Constants in Common.dll .rdata (community limit-breaking table, values confirmed in the HDE file):
  lane speed 2500 m/s (RVA 0x13F3CC float), entry ramp 0.125 of it per second (0x13F440), exit speed 400 m/s (exit
  distance constant 400 at 0x13F448), 0.4 (0x13F458, the end phase). Measured, Li01 174 -> 169 (30 km): the game's dock
  approach ends with the ship nearly stopped at the entry ring (20.3 s from ~1.4 km at 80 m/s); ramp linear 312.5 m/s^2
  (0 -> 2500 in 8 s, 10 km; 174 -> 173 6.68 s); full speed 7.5 km in 3.00 s; the last segment slows linearly with
  distance 2500 -> 400 (6.52 s, model L / 2100 x ln 6.25 = 6.55 s); then 0.87 s to free flight, 213 m past the end
  ring. Ride 23 s vs ~100 s by cruise. `get_tradelane_exit_ring` = the next ring ahead (not the lane's end).
- **Lane entry geometry and the fast entry (18 Oct 2026, Li01 rings 177 / 178, autopilot fast_dock on a ring):** a ride
  starts at a ring hardpoint, `HpLeftLane` / `HpRightLane` (254 m below / 252 m above the ring's centre on Li01, one
  track per direction): `IObjInspect::get_lane_start(ring, next&, start&, dir&)` called on the PLAYER ship (on the ring
  it answers -4) returns that point, the lane direction and the next ring for the ship as it is; `get_lane_direction(
  shipTransform, bool*)` on the ring is the side test (the sign of (ring - ship) . the ring's z axis; the bool's meaning
  differs per ring - relative to its orientation). Both flip in the same frame as the ship crosses the ring's plane;
  the nose does not matter (tested at rest on one side with the nose either way). After a dock request the game stops
  the ship (like full reverse: ~45 m from 293 m/s) and starts the ride once it rests within ~15 m of the start;
  stopped 40 m short it creeps forward (2.2 s), stopped past the plane it backs up (3.8 s). From 2.7 km (SaveC703 start,
  ring 178): the game's own entry 24.5 s; fast entry (cruise straight at a point 10 m short of the start, request when
  the predicted stop lands there and get_lane_start picks the wanted way) 15.3 s, request -> ride 0.8 s. From the far
  side (ring 177 towards 178): fly through the ring, request on the first frame the game picks the wanted way (just past
  the plane): right direction, request -> ride 3.4 s (stopped ~50 m past, crept back). The run plan (flight::PlanRun
  with the turn at the top turn rate, the cruise charge during it) predicted the requests within 0.1 s.
- **Obstacles and avoidance (18 Oct 2026; code core\game\space Obstacles / StrictAt, core\game\avoid):** field rocks and
  mines are client objects: Common.dll CObject::FindFirst / FindNext class 0x1003 (CAsteroid; `is_mine`), 0x4001
  (CDynamicAsteroid); position EngineObject::get_position, size CSimple::get_physical_radius_r (rocks 2-62 m, Li01
  mines 60 m, the Starflier 10 m). The game creates field objects only ~1.5 km around the ship. The player's auto
  avoidance toggle (u8 0x674AA4, setter 0x4C79D0, USER_TOGGLE_AUTO_AVOIDANCE) is off by default; the native goto's
  avoidance lives in Common.dll's goto behaviour. **Planets (measured 19 Oct 2026, flight recorder):** the native goto
  from SaveC702 to a point straight behind planet Manhattan (death zone SPHERE r 4100, damage 2000000) flies straight
  in at cruise until ~600 m from the death zone's surface, brakes hard (300 -> ~78 m/s), then slides along the sphere at
  a steady 600 m (590-603 m, cruise speed) and peels off to the target (closest 515 m); identical with the auto
  avoidance toggle on (0x674AA4 = 1). FLvibed treats lethal zone spheres (damage >= 1000) as obstacles with the same
  600 m berth (or 2 s of travel), planned ahead instead of braking. FLvibed's own: one smooth path through everything ahead (elastic band,
  25 m points over ~5 s of travel, pushed out of each clearance and pulled straight; bends vs the turning circle speed /
  turn rate). Pittsburgh high-density debris, 12 km (SaveC703 + console pos, godmode): 4/4 runs 0 contacts, 46.0 s at
  cruise; straight 11 contacts, 47.1 s. Strict fields (slowing allowed) from the field files' flags
  `mine_danger_objects`, `gas_danger_objects`, `object_density_high`, `DAMAGING` and zones with `damage` (minus the
  field's exclusion corridors): Zone 21 minefield 9.5 km: 0 mine contacts after the start, 64 s; straight 10 mine
  contacts, 40 s. Mines drift towards the ship.
- **Zone damage in a minefield (24 Oct 2026, data + logs):** `Zone_Rh04_minefield` (M10's shipyard field, SPHERE 7000,
  `damage = 200`, `edge_fraction = 0.05`, mines `mine_radiate`) is the only minefield zone with `damage` in HDE 0.7.1.
  Measured: the hull falls exactly 200 points/s (~1.7 %/s on the M10 ship) at a full, steady shield - zone damage skips
  the shield - on the entry leg after the Bruchsal launch (-17700 .. -19600 x); none during the fight inside
  `Zone_Rh04_nomad_shipyard_exclusion` (SPHERE 3500, flags 196608) nor on the exit along tracks 5-14 (BOX 150 x 150, flags
  131072, the field file's exclusions). **Uncertain:** whether the exclusions cut the damage (the entry flew near tracks 1-4
  too) - probe in game.
- **Script-locked docks (M10 Holstein, 24 Oct 2026):** `Act_LockDock = Player, <base>, lock` makes a dock request answer
  kind 0 response 1 (`dock_denied_locked`) and the game's best-path plot to that base return no way at all (the waypoint
  list stays empty) until the unlock.
- **Leaving a lane early = USER_MANEUVER_FREE_FLIGHT** (op 19 at once): the ride decelerates linearly 2500 -> 0 in
  ~1.0 s over ~1.25 km, then free flight from rest (Li01 146 -> 141, exited at 142). The dock toggle (USER_MANEUVER_
  DOCK again) does nothing during a ride (6 s tested). Freelancer.exe never calls CShip::request_exit_tradelane.
- **Disruptions:** twice on Li01 174 / 175 a ride stopped the same way (2500 -> 0 in ~1 s) mid-lane where hostiles
  (attitude -0.3 / -0.65, a fight with police) were within 1-5 km; no NN line was caught. Planner: re-plan after any
  unplanned lane end.
- **Best path (the nav map's, and the source of objective routes):** Freelancer.exe 0x4C4EB0 `void cdecl(const
  Vector* start, const Vector* end, u32 end object, u32 end system)` (signed `exe::PlotBestPath`; FLUF uses it) sends
  IServer RequestBestPath (0x34 bytes: start / end system + position); the server's answer **replaces the player's
  waypoint list** (exe::Waypoints): lane pairs (entry ring, exit ring), gates, the destination; across systems too
  (Li01 -> Li02: rings 141 / 146, 147 / 154, 127 / 133, Li01_to_Li02, then Li02 rings 1 / 6, the point). It respects the
  save's locked gates: SaveC702 (story locks) gave an empty list to Li02, SaveC703 (all open) the route above. Debug
  `bestpath <object> | <x> <y> <z> [system]`, `routes` (waypoint list + the contact list's type-3 list, raw).
- **Dock request range: 50 km.** Common.dll compares the squared distance with the float at Common.dll+0x19C518
  (2.5e9; code at RVA 0x63880..0x638AC, the site HDE patches; vanilla 10 km). A request from farther is handled but
  starts nothing (Texas gate at 58 km); the server's answer reaches the player ship through 0x547250 (response 0 queues
  the NN line `nnv_dock_too_far`, id at 0x678B1C). Jump gates and holes from within the range: the dock autopilot
  cruises there (Alaska gate from 34 km).
- **Zones:** server.dll `pub::System::InZones(system, const Vector&, float radius, u32* ids, u32 max, u32& count)`
  lists the zones containing a point with the game's own geometry; zone ids = CreateID of the zone nickname (verified:
  `Zone_Li01_zone21` 2820208901). `pub::Zone::InZone(zone, pos, radius)` tests one. Hazards come from the system files:
  a field (`[Asteroids]` / `[Nebula]` `file`, `zone`) is dangerous when its field file's `[properties] flag`s include
  `danger_density_*`, `*_danger_objects` or `DAMAGING` (Zone 21: `mine_danger_objects`, `danger_density_high`), and its
  `[Exclusion Zones]` are free of it (Zone 21 excludes `Zone_Li01_to_Li05` around the Alaska gate and the corridor
  `Zone_Li01_zone21_lane_access`, a BOX rotated -70° about y whose long axis ends at the gate: zone `rotate` yaw maps
  local z to (sin θ, 0, cos θ)); zones with `damage` > 0 hurt (planet / sun cores 2000000 / 10000, radiation 40..50).
  **The Goto and dock autopilots fly straight into them:** the dock autopilot to the Alaska gate from Missouri crossed
  Zone 21 and the mines killed the ship (17 Oct 2026). Harness: `flroute.ps1` plans around them (via the lane-access
  corridor the ship reached the Alaska gate unharmed).
- **Waypoint arrival distances** (HUD, 0x4F4016..0x4F4058): within 150 m (0x5D97B4) the reached waypoint is removed
  and the next selected; the NN "reached" line at 300 m (0x5CA230 = 100 is the goto stop distance). A Goto whose
  target disappears keeps running and flies straight ahead (seen: op 2 with target 0, the ship far past the point).
- **Jumps:** the dock maneuver on a gate keeps op 7 through the approach; the jump tunnel follows (vanilla 10 s), then
  the system id 0x673354 changes. **After the jump-in the op still reads 7 (dock) with no target** until the next
  maneuver; a dock press then would toggle it off (the harness ends it with free flight first). New York -> Texas
  from 1.7 km: 16 s to flying in Li04; the arrival point (70959, -83, 67090) is 175 km from the Texas gate back
  (`Li04_to_Li01` at 12436, 0, -91073).
- **Locked gates and holes refuse with the NN line `info_access_denied`** (`nn-message` event, hash 2261456396): the
  gate stays closed, op back to free flight. The locks belong to the story runtime: Content.dll keeps them
  (`Act_LockDock = Player, <gate>, lock|unlock`, list `HClocked_gates`) and saves them as `[Player] locked_gate =
  <id>` (`[locked_gates] npc_locked_gate` for NPCs). A free-roam save made from an early story state locks every
  New York gate and hole. Other refusal lines (voices ini): `nnv_dock_too_far`, `nnv_jump_gate_access_denied`,
  `nnv_trade_lane_access_denied`, `nnv_formation_request_denied`, `nnv_dock_denied_destination_hostile`,
  `nnv_trade_lane_end_docking_not_allowed`; `nnv_dock_denied_standing_by` exists in voices_recognizable.ini but no code
  names it (nor its hash 0x933DDC45) - the queue line the game plays is `waiting_to_dock` ("DOCK IN USE - ENTERING QUEUE").
- **Dock replies (20 Oct 2026, static + verified in game):** the server answers every player request through the
  client interface, FLHookSDK IClient slot 83 `Send_FLPACKET_SERVER_REQUEST_RETURNED(client, ship, kind, response, x)`
  (exe vtable 0x5E6674, the client object's - set at 0x5B084A; slot 83 = 0x53BF80, which finds the ship and calls its
  handler 0x546D00: a listener at ship+0xEC first, then the player's NN line from two tables by response). Kind 0 =
  dock (rings, bases, gates), kind 2 = lane, kind 1 = formation (response 0 seen). Kind 0 responses: 0
  `dock_denied_hostile`, 1 `dock_denied_locked`, 2 / 5 `cannot_dock`, **3 `waiting_to_dock` = queued** (x = -1), 4
  `dock_granted`; kind 2: 0-2 / 5 `cannot_dock`, 3 none, 4 granted. Seen: the reply ~15 ms after the request; a queue
  at the Li01->Li02 gate = 3 then 4 after 2 s .. 27 s. Host API v14 `DockReply`, event `request-reply`. The
  maneuver results go elsewhere (0x547250, by code: 0 `nnv_dock_too_far`, 1 `nnvoice_trade_lane_disrupted`, 5
  `nnv_formation_lost`).
- **Formation (USER_MANEUVER_FORMATION on a ship):** op 10 at once when accepted; the player joins the target's wing,
  so `IObjInspect::get_formation_leader` reports the **wing leader** (target 117 -> leader 116), and cruise is shared
  with the wing. Some ships do not accept (two fighters: the op stayed 19, no NN line seen). Free flight
  (USER_MANEUVER_FREE_FLIGHT) leaves it (leader 0).
- **HD Edition console (single player, chat):** `cspd <speed>` sets `EngineEquipConsts::CRUISING_SPEED` (Common.dll
  export, vanilla 300) and raises `PhySys::ANOM_LIMITS_MAX_VELOCITY` (the autopilots use the new value; faster cruise
  overshoots more); `pos x, y, z` relocates the ship; `jump [nickname]` relocates it next to an object (with no
  argument the target; after it relocated the ship to the Texas gate the ship died within 11 s without godmode,
  cause not confirmed: the harness places the ship itself, `Move-FLShipNear`). Source: research\repos\Console.

## AI on the player ship (verified in game 17 Oct 2026; code core\game\ai.*, research controller\docs\NPC_AI_RESEARCH.md)
- **Maneuver dispatcher** 0x4D8E20 `bool thiscall(PlayerManeuvers = *(0x674A58), int op, bool flag)`, ret 8, ops
  -1..19 (jump table). Works on the current target, reports to the server like the maneuver bar. Verified: **Trail 3**
  (holds full speed toward the target; dogfights a hostile fighter at 40-150 m; a cruising patrol cannot be caught),
  **Face 15** (aligned in ~1.6 s, holds), Goto 2, Dock 7, FreeFlight 19. Strafe 13 ends at once (on a fighter).
  The dispatcher's Trail op: target +0xC, distances +0x10/+0x14 from PlayerManeuvers +0x3AC/+0x3B0 (100 / 0); Face op:
  kind +0xC (1 object, 2 point), target +0x10, point +0x14, byte +0x44 = 1; IDirectiveInfo {1, 0, op}.
- **Server directives** `pub::AI::SubmitDirective(playerShipId, op)` (server.dll; ops built with Common.dll's
  exported constructors): **1 = accepted**. Goto (point or ship, range, thrust, cruise) works; **Follow** works (op 17:
  catches up with cruise up to ~300 m/s, then holds ~400-500 m) but ends at once when the leader is already beyond
  maxDistance; **Idle = a full stop** (speed 0 in ~1 s, then free flight): this is `Act_PobjIdle` - it stops the
  player's autopilot, it does not freeze the player (SubmitDirective returns 1 for it, yet it acts; 18 Oct 2026).
  While a mission's Idle parks the player (M13 launch, ~26 s) the behaviour manager reads op freeflight, unlocked, at a
  directive priority > 0 (22 Oct 2026): the player may fly - `ai::MissionHolds` no longer counts it as a hold.
  A mission's `Act_DockRequest` (M12 core exit) holds a dock port with no request of the player's: any new directive
  in the next frames cancels it (22 Oct 2026: the ship then never left the core).
  Cancel works. **Trail / Strafe / Evade / DrasticEvade / Face return 5 (refused for the player ship)**; Face works
  through the maneuver dispatcher on the selection (it keeps tracking the object and creeps forward ~2.5 m/s until
  ended, 18 Oct 2026). Vanilla mission scripts park ships with `StayInRange = <point|object>, <radius>` (150 uses,
  a Content.dll leash) and `Idle` (m01a `stop_player`).
- **Client directives** `IBehaviorManager::set_current_directive(info {1,0,op}, op)` on the player's own manager (what
  the dispatcher does): Trail with any distance works. A Trail whose target is destroyed keeps running headless (op 3,
  no crash): re-issue it.
- **Behaviour objects** (Common.dll): the manager holds one per op at `+0x174 + op*4` (21), made by named factories
  (registry 0x63FC8E4[op], e.g. "Evade" = 5, "DrasticEvade" = 18). Behaviour vtable: +0x0C begin, +0x1C update
  (returns 1 running, 2 finished; also stored at +0x10), +0x28 set from the op.
  Ops (size, vtable): 1 Buzz (0x354, 0x639F564), 2 Goto (0x3FC), 3 Trail (0x248, 0x639FA6C), 4 Flee (0xE8,
  0x639F64C), 5 Evade (0x2C0, 0x639F874), 6 Idle (0x40), 7 Dock (0x470), 8 TradeLane (0x234), 9 InstantTradeLane
  (0x5C), 10 Formation (0x2B8), 11 Large Ship (0x180), 12 Cruise (0x2C), 13 Strafe (0x1BC), 14 Guide (0x124), 15 Face
  (0x1DC), 16 Loot (0x180; set-op resolves the object id at op +0xC), 17 Follow (0x2BC), 18 DrasticEvade (0x44), 19
  FreeFlight (0x400), 20 Delay (0x48). Buzz, Flee, Large Ship, Cruise ignore the op (set-op 0x62C2F40 returns 1):
  they work from the ship's own target / threat; they have no exported directive op but run client-side with a bare
  op buffer. **Crash audit (17 Oct 2026):** every op on the player ship against a live fighter, the fighter then
  destroyed mid-op: no crash (with the Evade guard). Buzz, Flee and Trail keep running headless after the target
  died (no crash in 15 s); with no target, Buzz and Flee end at once. Server directives Face and Formation return 5.
- **Evade (op 5)**: op +0xC = threat id; set-op (0x62C8630) resolves it in the system's object table into a watcher
  at behaviour +0x2A0 (0 / 8 = no object; cleared when the object goes). The update (0x62C7D60, vtable 0x639F874 slot
  7) checks the threat (exists, in range) **only for NPC ships** (`[mgr]->vt+0xBC` = player-controlled skips it) and
  finishes (2) otherwise; for the player ship a missing threat reaches 0x62E4500 with null = **AV at
  Common.dll+0x84509** (the crash seen 17 Oct 2026: the autopilot's threat died 0.4 s into the Evade). FLvibed's host
  guard (controller host, API v6 GuardEvade, code verified by signature) applies the NPC rule to the player: null
  threat = finished. With it, verified 17 Oct 2026: Evade on a live fighter runs ~2.6 s (the personality's dodge on
  the manager's personality) then free flight; threat destroyed mid-Evade x3: guard ends it, no crash. Without the
  guard ai.cpp refuses client Evade (-104); a threat id that resolves to nothing: -105.
- **DrasticEvade (op 18)** has no threat: op +0xC mode (0 = slide along x/y at +0x10, 1 = full reverse; any other
  value fails at once), +0x1C seconds (ctor default 1.0; behaviour default 4). Verified: mode 0 x=1 -> strafes right
  at the ship's strafe speed (50 m/s) for the time; mode 1 -> reverse (16 m/s); then free flight. The old "crash" was
  the target id written into the mode field (failed at once).
- The manager's personality (`set_personality`, [manager+8]+0x1584) drives the player ship's own behaviours (Evade's
  dodge thresholds at +0x230.. are read from it): ai::ApplyShipPersonality / RestoreShipPersonality.
- **NPC brain does not apply to the player ship.** `pub::AI::SubmitState(SetPersonalityParams)` on the player returns
  0 and changes nothing (server get_state_graph_id(player) = 0: the server does not own the client-created manager;
  "PLAYER" graph id = 1). Client-side `set_personality` (copies 0xF6 dwords = 0x3D8 bytes to [manager+8]+0x1584;
  verified by read-back) + `set_state_graph` + `refresh_state_graph` take, but nothing evaluates the state graph for
  the player: no autonomous behaviour switching. The AI also **never fires for the player** (BaseOp fireWeapons does
  nothing): firing stays with the client input (FLvibed's gimbal aim + USER_FIRE_WEAPONS).
- **Personality** (pub::AI::Personality, 0x3D8 bytes, FLSDK layout matches: trail_distance at 0x140, Job at 0x2A0);
  `DATA\MISSIONS\pilots_population.ini` + `pilots_story.ini`: 319 [Pilot] (with `inherit`) and 603 sections of style
  blocks referenced by `*_id`; the field names in the blocks are the struct's field names.
- Hull: `CSimple::get_relative_health` (Common.dll) 0..1, for NPCs (contacts `health=`) and the player.
- Attitude of any ship towards the player by id: server.dll `pub::SpaceObj::GetRep(id, int&)` +
  `pub::Reputation::GetAttitude(rep, playerRep, float&)` (-1 hostile .. 1 friendly; police after `rep l H` -0.9,
  friendly traffic 0.65); a ship's target: `CShip::get_target` -> IObjRW, id at vt[8] (attackers of the player).
- **The player ship's capability** (Common.dll, verified 17 Oct 2026): `CShip::get_farthest_active_gun_range(float&)`
  (Justice Mk I: 600 m), `CEqObj::get_power_ratio`, `CShip::get_repair_kit_capacity_remaining` /
  `get_shield_battery_capacity_remaining` (free room) + `CShip::shiparch()` Archetype::Ship +0x140 / +0x144 (nanobot /
  battery limits; ge_fighter = Starflier 10 / 10, matches shiparch.ini) = the counts aboard; the shield: equipment
  manager at CEqObj +0xE4, `CEquipManager::FindFirst(1 << 8)` -> `CEShield::GetHitPoints` / `GetMaxHitPoints`
  (Starflier 600). Repairs: the commands USER_REPAIR_HEALTH / USER_REPAIR_SHIELD (the HUD's buttons).
  **Using N units (verified 21 Oct 2026):** the buttons end in the client ship's `IObjAffect::use_item(u16 cargo id, u32
  amount, u32 ship id or -1)` - IObjRW + 4, vtable slot 22 (+0x58), Freelancer.exe 0x535470 (`exe::UseItem`, in the
  vtables at 0x5DE1BC / 0x5DFCEC / 0x5E0284) - with the count that fills the bar. It finds the cargo (CEquipManager
  FindByID, CECargo::cast), takes min(amount, held = CECargo +0x28), checks the archetype class (vtable +0x10:
  Archetype::AClassType RepairKit 26 / ShieldBattery 27) and sends `IServer::SPRequestUseItem({ship, id, amount},
  client)` through the server pointer [0x67ECD0] (0x5355ED, vtable +0xE0). Called with amount 1: batteries 3 -> 2,
  shield 0.47 -> 0.90 of 600 (`ai::UseConsumable`, debug `usebattery <n>` / `usenano <n>`).
- **Spawns:** Content.dll spawns every NPC in single player (random encounters and mission ships alike) through
  pub::SpaceObj::Create (ShipInfo: system +4, archetype +8, position +0xC; the FLHookSDK offsets of the later fields do
  not match this version), one spawn helper (Content.dll +0x936A0) and one caller of it: no in-game flag tells a
  mission ship apart (verified 17 Oct 2026 with M01a). Mission ships spawn exactly at the mission ini's [MsnShip] /
  [MsnFormation] position or the position of their Act_SpawnShip / Act_SpawnFormation line (harness
  Get-FLMissionShips). Loot: CreateLoot; mission solars: CreateSolar.
- **Loot value and room:** CLoot::contents_arch (Archetype::Equipment; its id via Archetype::Root::get_id) and
  get_units; CShip::get_space_for_cargo_type(arch) = units the hold takes; CShip::get_cargo_hold_remaining (volume),
  Archetype::Ship +0x100 hold size (Starflier 20); base prices from goods.ini [Good] by CreateID(nickname).
  Tractor reach: the mounted tractor (equipment class 1 << 20, CEquip::EquipArch) -> [Tractor] max_length in
  DATA\EQUIPMENT (Starflier 1500 m). USER_COLLECT_LOOT = "Tractor Beam (All Items)", USER_TRACTOR_BEAM = one item.
- **Area danger:** pub::System::InZones at the ship + the system file's `encounter = <name>, <level>, <chance>` (zone
  nickname -> CreateID; universe.ini [system] file); pub::Player::GetRank(client 1) (near Fort Bush: level 2, rank 0).
- **Slides:** USER_MANEUVER_SLIDE_EVADE_UP / DOWN work (33 m/s on the Starflier) as LEFT / RIGHT; HDE's keymap lists
  them unbound.
- **Loot** objects are CObject class 0x803 (CLoot, a CSimple: not a CEqObj), found with `CObject::FindFirst(0x803)`;
  `CLoot::get_name` gives the item's name (Shield Battery, Justice Mk II, Water...). The game's **Loot behaviour
  (op 16) works on the player ship** (client directive, loot id at op +0xC): it flies to the loot and tractors it
  (a battery from ~1 km in 6 s; the count went up). `destroytarget` kills drop no loot; real kills do.
- Combat numbers (starter Starflier, 2x Justice Mk I, near Fort Bush vs police Patriots, godmode): lead pursuit at
  ~150 m kills ~2.7 fighters / min; the game's Trail at the personality distance (~290 m) with gimbal aim 0 in 90 s.
- Dock refusal NN lines seen as `nn-message <hash>`: 2430537031 `nnv_dock_denied_destination_hostile` (the base's
  faction is hostile, e.g. after the console `rep l H`).

## HUD 2 readers (25 Oct 2026; code core\game\hudinfo.*, debug `hudinfo [wg|engine|lock|tractor|obj <id>|sys [id]|equip <arch>]`)
Verified in game on instance 5 (SaveC705, HDE 0.7.1, 4K) unless marked; screenshots `research\experience\runs\hud2_readers_2026-10-25\`.
- **Weapon groups** live in Freelancer.exe, not Common.dll: a manager object at **0x674C0C** (`exe::WeaponGroups`). Current
  group (0-based) at +0x28, getter 0x4F6170 (`exe::WeaponGroupCurrent`), setter 0x4F6180. Stored groups: vector of
  Group* at +0xC (begin) / +0x10 (end); Group {+4 / +8 / +0xC vector of VC6 std::string (0x10 bytes each: +4 char*, +8
  length, +0xC capacity) = the hardpoint names, +0x10 int group index}. HUD_Status's command handler (0x4DBA50, jump table
  0x4DC8A4 / byte table 0x4DC994): USER_WEAPON_GROUP1..6 (0x91..0x96, 0x4DC415..) = save the current selection into the
  current group (0x4F5580: every active gun / CM / mine, CEquipTraverser class 0xE0, by hardpoint name), set the current
  group, load group n (0x4F5920: switches the weapons on / off), HUD_Status +0x180 = n; USER_ASSIGN_WEAPON_GROUPn
  (0xB1..) = 0x4F5580(n) alone; USER_FIRE_WEAPON_GROUPn 0xAB.. -> 0x4DCC60(n). A group appears in the vector only once
  saved (SaveC705: only group 1 = HpWeapon01 + HpWeapon02; switching to 2 stored an empty group 2 and switched every
  weapon off). **The current group's stored copy is stale until the player switches away: its live membership is the
  weapons switched on now** (CEquip::IsActive). The HUD's group buttons (HudWeaponGroupButton x3) come from HDE's
  HudWeaponGroups.dll plugin; the 6 groups are the game's. Verified: `tap USER_WEAPON_GROUP2` -> group 2, the "2" lit
  yellow (group2-081150.png), back to 1 -> bits 0x1 on both group-1 weapons.
- **Equipment display names:** Archetype::GetEquipment(id) -> Archetype::Root, id +8 (= Root::get_id), **ids_name
  +0x14** (Azrael, Javelin Missile, Lavablade Mk II = the HUD weapon list). The archetype of a mounted item: CEquip::
  EquipArch -> Root::get_id. (Root **+4** is an unrelated field: 426997752 for the Azrael whose id is 3058004237;
  `space::Weapon::archId` read it until 25 Oct 2026 and now calls Root::get_id.)
- **System name:** Universe::get_system(id) -> ISystem: id +0x4C, **strid_name +0x68** (New York = 196609); the game's
  own string formatter (FmtStr, 0x41CC3A) calls the ISystem's vt[3], which returns the same value (checked). Zones: the
  formatter calls IZone vt[0x84] for a zone's ids_name; hudinfo takes the zones at the ship from pub::System::InZones
  and the name / size from the system file (only named zones have `ids_name`; the smallest containing zone wins).
- **Faction / level:** pub::SpaceObj::GetRep -> pub::Reputation::GetAffiliation -> GetShortGroupName ("Rogues",
  "Police" for Fort Bush). **Level = Reputation::Vibe::GetRank(rep)** (Common.dll; Freelancer.exe imports it):
  spawned fc_lr_pi_fighter_d3 = 3, matching the target panel's "LEVEL: 3" (target69-081212.png); police escort 2;
  server pub::Reputation::GetRank gives the same as a float. The target panel names a station's type itself ("Police
  Base" for Fort Bush); hudinfo's class word comes from CSimple::get_type bits (STATION, FIGHTER, ...).
- **Afterburner:** CShip::any_thrusters_on (1 while USER_AFTERBURN is held, 0 after). **Engine kill:** the player's
  CEEngine (CEquipManager::FindFirst(1 << 17), vtable ??_7CEEngine@@6BCInternalEquip@@@) - USER_MANEUVER_ENGINEKILL
  clears both CEquip::IsActive (+0x10) and CEEngine::IsControllerEnabled (+0x30); a second press restores them.
  USER_ENGINE_TOGGLE changed neither. **Cruise disruption:** CEEngine::EngageCruise(false, disrupted=true) sets the
  charge clock (+0x50, GetCruiseChargeTimeElapsed) to -CRUISE_DISRUPT_TIME (Common.dll export, 5.0 s in HDE); it counts
  up to 0 - so a negative clock = disrupted, (5 + clock) s ago (static reading of Common.dll RVA 0x36250; not yet seen
  in game: spawned fc_lr_pi_fighter_d3 never disrupted a cruising ship). The game's cruise line reads the same clock
  (ratio < 0: "Cruise Disrupted - n", HUD_CruiseProgress 0x4D5EB0).
- **Tractor:** CETractor::CountActiveArms > 0 while it pulls (USER_COLLECT_LOOT after a kill: 1 for ~1 s, then the 6
  loot objects vanished into the hold; USER_TRACTOR_BEAM with nothing targeted did nothing). Loot in reach = CLoot
  objects within the tractor's [Tractor] max_length (Starflier 1500 m). (`destroytarget` kills of spawned pirates DID
  drop loot here: 6 items.)
- **Missile lock:** CEGun::GetSecondsTillLock = max(0, Munition time_to_lock (+0x94) - the gun's lock clock (+0x26C)),
  only for seeker = LOCK (+0x90 = 2); every munition in HDE's data has `time_to_lock = 0`, so the lock is immediate:
  MissileLock() reads 1 with a guided launcher and a ship targeted, -1 without either and on a solar (the gun's clock
  runs on any target: a station read 1; the seekers home on ships). There is no lock-on progress to show.
- **The trade lane run** (verified 25 Oct 2026, SaveC705, the lane from Fort Bush's junction to Norfolk Shipyard, 9 rings
  in 29 s): CShip::get_tradelane_exit_ring = the next ring ahead (the entrance ring stays the ring entered); the chain
  from it by the system file's next_ring / prev_ring (the side that reaches the exit from the entrance) to the last ring;
  **the destination = the last ring's `tradelane_space_name`** (only a lane's end rings carry one: the place at that end).
  The game advances the exit ring a little after the ship has passed it, so the distance left is measured along the lane
  (a straight distance to the exit ring grew at every ring). On a lane the client's position (Pilot::pos) lags and jumps;
  the server's (pub::SpaceObj::GetLocation) is smooth: the ride's speed 2498 m/s, constant until the ship slows into the
  last ring (an ETA from the momentary speed jumped 8 -> 20 -> 2 s there; hudinfo uses the ride's top speed).
- **Docking and jumps** (verified 25 Oct 2026; the harness skips the base docking cutscene, so a base dock's sequence was
  not seen): a dock reads requested (the dock op), then granted (the server's DockReply 4: "Proceed to dock 3"), the bay
  index only while the port is held (the sequence). **A jump is a dock with the gate:** requested, **queued** (DockReply 3:
  57 s at the California gate), granted, then the sequence (the port held, control excluded: index 0) and the tunnel
  (the port still held), the system changes ~1.5 s before the effect times end (space::JumpSeconds: 12.4 s for New York
  <-> California), then the jump-in, the only part where **CShip::is_jumping** reads true, and the far gate's port held in
  the dock maneuver for a few more seconds. A load into space also reads is_jumping and a held port for ~10 s.
- **Formation:** CShip::get_formation_followers of the leader lists the wing (SaveC703, a freighter's: its two escorts
  and the player). **Zone name:** verified at the Jersey Debris Field (ids_name 261210). Entering / leaving a named zone
  also shows the game's own line (NN_ArrivalText, "UI" below).
- **Credits:** pub::Player::InspectCash(client 1) (500000 on SaveC705).
- **Mission timer:** Freelancer shows no countdown: the story's timers are hidden `Cnd_Timer` conditions (381 in the
  mission files) and no Act_ shows a time. MissionTimer() is always -1.

## HUD panels (the scanline look; verified 21 Oct 2026)
The flight HUD's windows are UTF models (`DATA\INTERFACE\HUD\hud_shipinfo.cmp`, `hud_target.cmp`, `hud_maneuverbox*`,
`hud_nnbox*`; HDE ships `_darkhud` variants). In `hud_shipinfo.cmp`: materials `HUD_NN_tray` (base,
`backgroundpattern.tga` 1024² DXT1, Oc 0.2), `HUD_frame` (`HUD_gradientblue.tga` edge glow), `ANIM_HUD_scanline`
(`refreshtest2.tga`, 64² RGBA: colour 160, 209, 255, alpha 50 at one edge fading to 0 over a quarter of the width),
`ANIM_HUD_edgepulse` (`edgecolor.tga`), on parts `Part_scanline*` / `Part_edgepulse*`. `MaterialAnim\anim_hud_scanline`:
MACount 1, MAFlags 2, MADeltas (8, 0.25, 0, 0, 0) - the scanline texture scrolls 0.25 per second. **Measured in game**
(the empty panel at 4K, every frame for 5.6 s; `research\experience\runs\hud_scanline_1021`): a horizontal line ~6 px
thick (+26 / +35 / +41 RGB) with a glow trailing ~100 px above it, sweeping **down** the panel's full height, looping
every **4.0 s**; the base reads (13, 11, 26) over dark space, the frame's inner glow (45, 56, 73) fading over ~12 px. The
subtitles' `background_style 1` reproduces it (subtitles\docs\DISPLAY_PLAN.md "Backing").
- **Frame profile** (4K, across a bottom panel's top edge): ~2 px rising, the line (118, 151, 180) ~3 px, a bevel from
  (70, 77, 98) to the base over ~16 px; corners cut 22 px wide x 45 px tall. No dark rim (a dark gap between two
  panels' lines is the space between them). The top frames (maneuver bar, NeuroNet box) differ: 45-degree corners and a
  thicker, brighter glassy edge. The edges facing the screen breathe ~+-6 % over 2 s (`edgepulse`); the side edges
  only brighten where the scanline crosses them.
- **The HUD appears in one frame** after a launch (edge brightness 23 -> 83 between two frames, ~0.24 s after space
  mode, once the letterbox bars have gone): no slide, no fade (recorded at 150 fps, `runs\hud_scanline_1021\launch_grab`).
- **Positions are float immediates read when the HUD is built** (HudShift's `position` addresses; `C7 44 24 nn imm32`,
  the float being the imm32): the maneuver bar = `ManeuverFrameX` 0x4D7BBA, `ManeuverRegionX` 0x4D9711 and the
  buttons' x, loaded by `fld dword [p]` at 0x4D8B5D (vanilla p = the shared constant 0.02 at 0x5D1FE4; HudShift
  repoints p at its own float in the filler at 0x4D8BEC). A HudShift group shift of dx moves the frame and buttons by
  dx x 0.5 (they sit at depth 0.5) and the region by dx: on screen dx x 1.2963 H pixels (the flight UI camera's x scale,
  2800 at 2160; the menus' camera reads 2742). Values written before a game load take effect at that load. 4K, 16:9:
  the frame spans 1419..2425 px (0.2329 H half width), the buttons 1506..2334; the NeuroNet box 3226..3818 (22 px from
  the right edge, its lower edge 69 px); the cruise box hangs under the bar's left end to 0.153 H.
- **The comm window** (the speaker's portrait, its name lines, the generic icon; HudShift group Comm, top left): y
  constants `CommTextY` 0x4CB7E3 (0.229, scale 1), `CommWindowY` 0x4CD7A6 (0.65; HudShift's scale 2.392 moves the frame
  12 % farther than the text - measured 158 vs 141 px at 4K - 2.135 keeps them together), `CommIconY` 0x4CD86A (0.0986,
  0.362), x beside each. **Read each time a comm window opens** (a write shows with the next comm, no reload). Native at
  4K: the portrait frame 195..440 px (0.090 H top), the name lines from ~455 px - right under the bar's left end once
  the bar is moved there (21 Oct 2026, the cruise box over the portrait).
- **The comm window live** (verified in game 21 Oct 2026 against the Freelancer-Decomp hud_comm.cpp): the HUD's comm
  receiver (`exe::HudCommInstance` 0x6748F8, null outside flight) holds the player's channel at +0xB4 (+0x1BC its caption
  control "CommText", +0x170 the head body, +0x174 no-portrait flag) and its window (FCommWindow) at +0xEC: +0 open,
  +0x24 win_pos (HDE 0.7.1: -1.4746, 0.5265, -2.392 after HudShift), +0x30 btn_pos (0, 0, -1), +0x3C win_size (0.2,
  0.2), +0x44 open fraction. **Laid out every frame** from win_pos / win_size: the head renders into the viewport between
  the window's corners, so writing them moves and scales the head live (234 px at 4K natively; a flat or narrow size
  squashes the render, down to 2 px works). Its thin outline (`DrawFrame` 0x4CC440, the HUD chrome colour, ~130, 201,
  243) is a fixed 0.207-unit square around win_pos at win_pos's depth: its size follows the depth only. In a
  letterboxed scene (dock / launch animations) Dispatch gives a comm no window at all, decided at its arrival. +0x5B4 is the generic
  icon "LowEndCommButton" (`interface\HUD\HUD_genericcomm.3db`: a radio tower in a thin frame), an icon button: its mesh
  moves only through its position setter (`SetControlPosition`), not a written +0xA8. What shows (Dispatch 0x4CE000,
  Appearance 0x4CB250, Update 0x4CD9D0): a comm to the player, not from the player, with a costume (any of head, body,
  hands, accessories non-zero) and no letterbox opens the window - the live head, or the generic icon with the caption
  when no body can be built or character detail (PerfOptions slot 5, `0x67ED24`) is 0; a comm without a costume, chatter
  between others, or one during the letterbox opens none (audio only). Stations' dock replies carry a costume (an ATC
  officer). The caption = the ship's pilot name and its group name, else the object's name, else the message's
  caption IDS (record +0x40). **Do not `pub::SpaceObj::SendComm` a costume whose parts the mission has not preloaded**
  (the M06 buoy's `no_head, robot_body_a` outside M06 crashed the game twice; subtitles debug `comm`).
- **The objective banner can be moved while it shows:** its control "MissionObjectiveText" (created per showing) keeps
  its position at FlControl +0xA8 and the game does not reset it; written each frame, the banner types on in the new
  place. Its text starts 0.0125 H below the control's anchor (anchor 363 px, text 390-465 px at 4K, 16:9).
- **Target indicators and off-screen arrows** (traced 21 Oct 2026 with the controller's `findvt` / `watch` / `snap` +
  `diff`): one object, vtable 0x5D950C (`exe::VtHudIndicators`; 0x93C bytes, factory 0x4F28A0 registered in a class
  table at 0x5D9678, ctor 0x4E8FE0, dtor 0x4EA7B0; an embedded control at +0x38, vt 0x5D9454, overriding slot +0x24
  with 0x4F17C0; not in the named control list). Its per-frame code 0x4EC450 calls the alerts (0x4F1800 trail -
  `get_closest_trailing_ship` -, 0x4F18D0) and the indicators. Each indicator part is drawn by `exe::HudDrawPart`
  0x559900 `bool cdecl(part record, const Vector* pos, const Matrix* rot, float scale, int, float)` with pos in UI
  camera space (`exe::UiCamera` projects it: the code calls its vt+0x34). The part records come from the HUD's data
  by name (0x55A360 looks them up) into fields +0x7A0..+0x7D8 (the map is in core\flexe.h `VtHudIndicators`); the
  arrow function (0x4F0D85-0x4F128D, `this` in ebp) pins the arrows inside a frame ~211 px from the left / right, 157 px
  from the top and 2003 px from the bottom at 4K - under the bottom panels, the maneuver bar and the NeuroNet box - and
  draws the current target's health / shield bars after the parts (0x4F1322-0x4F17A9, immediate-mode quads) from the
  same position vector. The engine instances in the model holders (+0x450.. +0x540, 0x28 bytes each, vt 0x5D2EFC) do
  not hold the positions. The bottom panels' background images (TargetBackground, StatusBackground) do not project
  like their buttons (their depth does not nest the same way): anchor on the buttons.

## UI (NeuroNet / FlControl family)
| Address | Meaning |
|---|---|
| 0x67DBE8 | flat linked list of ALL controls: node {+0 prev?, +4 next, +8 FlControl*} |
| FlControl | +0 vtable, +4 parent, +8 next sibling, +0xC first child, +0x14 name len, +0x18 name[32], +0x3C 3x3 matrix, +0x6C state (bits0\|1 visible, bit3 enabled), +0xA8 pos (Vector, relative to parent), +0xB4 box half extents (hit size), +0xCC hint ids |
| 0x67DBF8 | UI camera: +4 3x3 rotation (identity), +0x68/+0x6C pixel center, +0x78/+0x7C scale (±2800 at 2160p), +0xA4 near (0.1). pixel = center + scale × (pos.xy / −pos.z) |
| 0x4BDD40 | camera: pixel → direction (this=camera, out, x, y) ; ray normalized by 0x41A390 |
| 0x67DBCC | current mouse ray (normalized Vector) |
| 0x59DD80 | `FlControl* __cdecl HitTest(const Vector* ray)` — topmost visible+enabled control, uses each control's vt[0x10](ray) and priority vt[0x6C]. Prologue `sub esp,8; mov eax,[0067DBE8]` (8 bytes) |
| 0x67DCD8 | hovered control (vt[0x3C] enter, vt[0x38] hover, vt[0x40] leave with the ray); 0x59DAA0 returns it |
| 0x59E080 | per-frame UI update (computes hover); 0x59E300 UI render (FLUF hook) |
| Neural Net hull / engine warnings | (verified in code 19 Oct 2026) the player ship's hull damage handler (0x545FBA..0x546038) plays NNVoice `hull_breach_imminent` ("HULL BREACH IMMINENT", id at 0x678B50, via 0x489E80) once when the relative hull (CSimple::get_relative_health) drops below the float at 0x5DFB40 (0.2; exe::HullCriticalFrac), flag ship +0x154 set; the flag clears when the hull is back at or above it. `engine_critical` (0x678B4C) plays when a damaged collision group of the player ship (CArchGroup type bits 0x3F00) drops low (0x545A15). Both are subtitled in the NN column (danger severity) |
| NPC names | (verified 20 Oct 2026) a ship's pilot name is its reputation's name record: Common.dll `Reputation::Vibe::GetName(rep, FmtStr& name, FmtStr& short, const u16*& cached)`; when nothing is cached Freelancer.exe 0x41D440 formats `name` with **0x41C970** `int thiscall(const FmtStr*, wchar_t* out, int maxChars) ret 8` (exe::FmtStrFormat: loads the template string - 16163 "%s0 %s1" - and fills in the values) and caches it with SetName. NPC names come from faction_prop.ini `firstname_male` / `lastname` string-id ranges (FmtStr::append_string x2). FLvibed: space::RepName, spawner names its ships the same way |
| HUD_DamageIndicator | **the HUD's hit-direction arcs** (verified 19 Oct 2026): instance [0x674A10]; 4 arcs, intensity floats at +0x38C, +0x3B8, +0x3E4, +0x410 (+0x410 = the right arc, seen in game; each covers ~90 deg), cap 1.0, fade over ~1 s. Inputs: 0x4D6F10 thiscall(source point) - ahead of the ship = no arc, roughly straight behind = +0.35 on all four, else +0.7 on the arc toward it; fed by 0x4D6CD0 (from 0x545ED0, vtable slot 0x1A4 of the player's ship class 0x5DFD6C: a round hit; source = hit point + 1000 x the round's velocity backwards) and 0x4D6F00 (from 0x5363F2: an explosion within its radius of the player, its centre). 0x4D6D40 thiscall(point): contact hits (0x545E90, slot 0x1A0: collision physics 0x525480). So the arcs only show off-screen sources; FLvibed hooks 0x4D6F10 / 0x4D6D40 for the exact point (exe::DmgIndSource / DmgIndContact). **Not every input is damage** (25 Oct 2026, HUD 2 FIX2, instance 5, SaveC705 vs Rogues): source reports (kind 1) came with no hull or shield change within 0.5 s (3 in one fight; the explosion path 0x4D6F00 feeds the indicator for any blast within its radius), so HUD 2's hit flash pairs a report with a hull / shield drop |
| 0x59E300 | **UI pass** (verified 19 Oct 2026): returns at once while [0x67DCF0] > 0; sets the UI camera 0x67DBF8, draws every visible control of 0x67DBE8 (sorted by +0x70 layer / +0x68 depth, vt[0x34] draw) - the whole space HUD included - then 0x417F10, 0x563AF0 and the cursor 0x41F150. Called by each state's frame after its 3D scene; in space 0x54B0C2..0x54B123: camera (state +0xBA0) update, BeginScene check 0x424F30, the world draw (0x4EC400, camera vt[0x78]), then 0x59E300. Its entry = between the 3D scene and the HUD (FLvibed's under-HUD draw point) |
| 0x59DA10 | `FindControl(const char* name)` |
| 0x59DB20 | CreateControl(name, type, a3, a4) (FLUF); 0x5A0C90 etc. create button/text/... (FLUF AbstractVanillaInterface) |
| vtables | 0x5E15F4 NN icon button, 0x5E622C text button, 0x5E614C MMOk button, 0x5E476C text, 0x5E4F24 title text, 0x5E4D7C static image, 0x5CB584 NavBar, 0x5D462C NN_Pause, 0x5D24F4 mission message |
| front end | 0x5E2844 main-menu button (static objects 0x67B804/0x67B8F4/0x67B9E4/0x67BAD4/0x67BBC4 = New/Load/MP/Options/Exit; root 0x67B780): translation +0x60, rect (w,h) at +0xB4 spanning right/down. 0x5E5AF4 front text button, 0x5E58D4 label, 0x5E5CB4 list column (rows internal), 0x67AE10 load-game root |
- Names seen: base nav bar `ButtonControl1..5` (top row: services/launch), `RoomControl1..n`,
  NeuroNet buttons `NNNavmapButtonRoot`, `NNInventoryButtonRoot`, `NNMissionLogButtonRoot`,
  `NNPlayerInfoButtonRoot`, `NNHelpButtonRoot`; tutorial popup `NeuroNetMissionMessage` with
  `MMOkButton` (the exe also names `MMYesButton`, `MMNoButton`, `MMLaterButton`: pub::Player::PopUpDialog's `buttons`
  bits 0x1 YES / 0x2 NO / 0x4 LATER / 0x8 OK; the story's tutorial boxes are 0x8; verified 26 Oct 2026 in M01 on
  vanilla 1.1 and HDE 0.7.1: `uiclick` on MMOkButton closes the box in space and space-ui. With the pad flying, the
  cursor is the crosshair, so the box does not switch the mode to space-ui: find it by its buttons, not by the mode); pause menu `NN_Pause` with `PauseButtonBox0..4`; save dialog `LoadSave_*`.
- Space HUD controls (hit-testable): ManeuverButton0-3, ManeuverCruiseButton, contact list icons +
  filters, TargetScan/Comm, NextEnemyButton, HudRepair/UseBattery/Loot buttons, HudWeaponGroupButton ×3.
- Lists: in-game load/save list column vt 0x5E5F9C, front end 0x5E5CB4. A double click (two clicks within
  GetDoubleClickTime) on a row loads it. Lists do not react to arrow keys. Hit rows as fractions of the
  render height H: front end row 0.0318·H, first row hit center 0.5H − 0.2593H; in game row 0.0290·H, first
  0.5H − 0.2391H (hit cells sit half a row above the drawn text).
- Base launch: planets have a sub-button (RoomControl under the Launch Pad room icon) that only appears
  when the story allows launching.
- **Objective banner (19 Oct 2026, in game):** the HUD's typed objective text at the top centre ("FLY TO RESEARCH
  STATION WILLARD", 0.17-0.225 H) is the text control **"MissionObjectiveText"** (vt 0x5E4F24), created at 0x4BB270
  through the text-control factory 0x5A0E30 with the fonts.ini nickname "MissionObjective" (Agency FB, 0.06 H) and
  stored at +0x33C of its owner. It exists (visible, `uiclick::FindVisible`) only while the banner shows: ~4 s after a
  load, longer in flight as it types; it also comes on each objective step and with USER_DISPLAY_LAST_OBJECTIVE (0x6E).
  The other user of the font, 0x48EE82, is the nav map's "MissionBriefingText". (A memory scan found the uppercase
  text only in the shared wide scratch buffer 0x66DC60 and short-lived heap copies.)
- **Fonts:** `DATA\FONTS\fonts.ini` [FontFiles] lists the game's font files (AGENCYB / AGENCYR / ARIALUNI.TTF);
  [TrueType] nicknames map faces and heights (fraction of the screen height): HUD and labels Agency FB (HudSmall
  0.029, ItemName 0.035, MissionObjective 0.06), body text Arial Unicode MS (ListText / ToolTip 0.03).
- **Flight HUD top centre (HDE 0.7.1, 16:9, measured on 4K frames):** the maneuver bar ends 0.086 H; a small box
  hangs under its left end to 0.153 H.

### The flight HUD's roots and hiding them (HUD 2's switch; verified 25 Oct 2026, HDE 0.7.1, instances 5 / 6 / 8, SaveC705; code `controller\src\impl\hudvis.*`)
- **Six top-level controls (parent 0) hold the whole flight HUD:** `HUD_Maneuvers` (the maneuver bar, the cruise box,
  FLvibed's autopilot button), `NN_MainButton` (the NeuroNet box), `HUD_Target` (the target panel), `HUD_ContactList`
  (the contact list and its filters), `HUD_Status` (the status / weapon panel, nanobots, batteries, the weapon groups),
  `HUD_Core` (the gauges). A seventh, `HUD_CruiseProgress`, draws the cruise line under the reticle. Not under them (kept
  by HUD 2): the comm window (vt 0x5D764C, unnamed), `HUD_Waypoint`, `Targetable_Objects` (the indicators, vt 0x5D9454),
  `HUD_DamageIndicator`, `MsgWin` (chat), the scanner window `NN_Scanner` (top-level, opened by USER_SCAN_CARGO).
- **The state byte (+0x6C):** bit 1 = drawn and hit-testable; bit 0 alone does nothing visible (clearing only bit 0
  keeps the control drawn; bit 1 alone hides and disables it); bit 3 = enabled. A parent's cleared bits hide its whole
  subtree (the panels draw their children: the base panel draw 0x4E7890 walks +0xC / +0x8 and calls each visible child's
  vt[0x34]). **The game re-shows `HUD_Maneuvers`, `HUD_ContactList` and `HUD_CruiseProgress` every frame** (their bits
  were set again each frame: measured rewrites = frames); the target, status, NeuroNet and gauge roots stay as written.
- **The 3D views belong to the panels' owners, not to a child:** the panel control is the owner's sub-object at +0x38;
  the base draw calls the owner's vt[0x38] right after drawing its background child (+0x494, when +0x498 is set). Target:
  `exe::TargetViewDraw` 0x4E3C90 (needs the target +0x398 and +0x648 == 1); the player's ship: HDE's `HudStatus.dll`
  replaces the status owner's slot (vanilla's is the empty stub 0x401E90). **Called directly on the owner while its panel
  is hidden, a view draws alone** (no panel art). Its rectangle = owner +0x4B4 (centre x, y, z = -1 in UI camera space)
  and +0x4C0 / +0x4C4 (half extents, 0.11 natively), read at every draw: 0x4E70E0 projects the rectangle and sets the view
  camera's viewport (+0x3B8) - shifted by -0.0125 x the screen width (48 px at 3840, its argument), so a requested centre is
  moved right by that. Writing the fields moves and scales the view live; put back, the panels draw as before. The view's
  object fills ~45 % of the rectangle. (Moving the background child's +0xA8 or zeroing its matrix +0x3C changes nothing:
  the panels' images are placed when the HUD is built.)
- **The views on vanilla 1.1 (FIX-VIEWS, 25 Oct 2026, instance vanilla11b + HDE instance 4):** the target view draws only
  while the target owner's **view mode +0x648 == 1** (setter 0x4E5460 thiscall(int), getter 0x4E5470); the game sets 1
  only when the player picks a target in the contact list (0x4CFE76, through the global target owner 0x674B04); the
  constructors write 0 - on vanilla the mode stayed 0 (targets from keys / FLvibed), so TargetViewDraw returned at once
  (HDE's was 1). +0x648 / +0x534 (the selected subtarget) / +0x594.. are the target class's own fields (its constructor
  0x4E08xx; the status class 0x4DA1xx shares only the base 0x4E6020). The base (both panels): +0x398 watch holder (the
  status owner's = the player's ship, object id 10, on vanilla too), +0x3A0 scene, +0x3B8 view camera, +0x4B4 rectangle.
  HDE's HudStatus.dll's view (its vt[0x38] replacement, 0x5D85F4) = option 10 (0x5792B0), RenderGetInt(0xBA2 viewport),
  ViewSetViewport 0x4E70E0(owner, 1, -0.0125f), ViewPrepare 0x4E7070 (scene + parts coloured by health, 0x4E6CB0),
  (holder - 8)->vt[0x10]() = the engine object, ViewDrawObject 0x406510(owner + 0x3A0, owner + 0x3B8, object),
  RenderSetCamera(UI camera 0x67DBF8), RenderSetViewport 0x421620(x, y, w, h, file, line). The same calls draw the ship
  on vanilla (the status panel's camera is placed by the game there too). Option table 0x679BE8 (0x5792B0(i), i < 0x46):
  10 = 1, 11 = 1, 12 = 0 on vanilla 1.1 (the same PerfOptions as HDE).
- **The hover help (ToolTip):** one instance, global 0x67244C (ctor 0x4BD690, vt 0x5D5F7C, the top-level `ToolTip`);
  its update 0x4BE2B0 -> 0x4BDDA0 runs from the UI update 0x59E080 every frame (before the UI pass) and starts the box
  after the cursor rests on a control; its hide 0x4BE390 = state bit 1 off + its showing flag +0x159 = 0 (called by the
  rooms, menus, the target panel 0x4E4B04 ...). With no hover the update itself clears a poked state bit the next frame.
- **The flat icon sheets on vanilla 1.1:** `DATA\INTERFACE\ButtonTextures.txm` (0.96 MB) holds Montage32 / Montage64 /
  HUD_targetingelement as 256 x 256 uncompressed TGA MIP0 (HDE: 1024 x 1024) with the same grid and meanings (shaded
  grey art instead of HDE's flat white); HUD_talkelement besides.
- **The NeuroNet windows are inside NN_MainButton:** the nav map `NeuroNetNavMap` (vt 0x5D2F84) is a child of the switch
  button `NNNavmapButton` (likewise inventory / log / player info / help under `NN<name>Button`); a window is open while
  its switch button shows (bit 1; the `...ButtonRoot` icons are the box's buttons). Hiding the root hides an open window.
- **The cruise line:** `HUD_CruiseProgress` (vt 0x5D7EBC, its text built by 0x4D5DB0) draws its text child `CruiseProgressText` itself
  (0x4D5EB0): while the engine charges "%s - %d%%" with IDS 1413 "CRUISE ENGINE CHARGING - WEAPONS OFFLINE", while the
  charge clock is negative (disrupted) "%s - %d" with IDS 1615 "Cruise Disrupted" and the seconds left (ceil).
- **The arrival / zone line:** `NN_ArrivalText` (vtable 0x5CE85C, created per showing by CreateControl, its pointer at
  `exe::ArrivalText` 0x66D398; child text `ArrivalText` +0x32C, fonts "PageHeader"): "%s System." (IDS 1373), "%s System,
  %s Space." (1374, the house from the system's strid_name 196609.. by the table 0x45B1A0: 1249 LIBERTY, 1250 BRETONIA,
  1252 RHEINLAND, 1251 KUSARI), "%s, %s System." (1372), and the zone entries ("Entered the Jersey Debris Field." / "Left
  the ..."). The game shows the child after a delay (bits |= 3) and deletes the control when the line has run; it waits
  while a comm window shows; the objective banner's code (0x4BC990) waits while the child is shown (bit 1) - hide the
  parent, not the child.
- **More top-level controls (the game's control list `exe::UiControls` 0x67DBE8: nodes {+4 next, +8 control}; HUD 2's
  fix round, 25 Oct 2026, instance 5, SaveC705, debug `hud2 tree roots`):** `MsgWin` (chat, vt 0x5CFFC4), `CTID` (the chat
  input, top-level, vt 0x5CFEC4, state bit 1 while open), `ToolTip` (vt 0x5D5F7C: the mouse-over box "Police Base /
  Fort Bush" by the cursor - on the pad the cursor is the aim point, so it sat on the reticle), `Rollover` (vt 0x5D5454),
  `HUD_Waypoint` (vt 0x5D96CC), `Targetable_Objects` (vt 0x5D9454), `HUD_DamageIndicator` (vt 0x5D804C; **re-shown by the
  game every frame**, like HUD_Maneuvers), the comm window (vt 0x5D764C, unnamed), `NN_GroupControl`, `BriefingParentWindow`,
  `LoadingProgressBar`. Each hides by its state byte as the panel roots do.
- **`Targetable_Objects`' children are the HUD's alert icons:** `ReticleRangeText`, `MissileAlert` (the red missile in a
  ring at (0.224, 0.213, -0.6) UI camera space - the classic target panel's top-left corner; it blinks while a missile homes
  on the player: the game sets its visible bit on and off), `TrailAlert` (-0.224, 0.213), `RadiationAlert` (0.224, 0.177),
  `LagAlert` (-0.224, 0.177). Clearing `MissileAlert`'s bits 0|1 every frame (before the UI pass) hides it alone; the
  brackets, their name / distance label ("Fort Bush 1341-M") and bars are the parent's own draw (no child controls).
- **A trade lane ring's name on the target panel / contact list** ("Colorado Jump Gate->Fort Bush"; verified against the
  classic HUD 25 Oct 2026): built by 0x4E8D30 (called from the name builder at 0x4E884A for an object of type bit 7) - it
  walks the ring's links (+0x1D8, +0x1DC: prev / next) to both ends of the lane, takes each end's
  `CSolar::get_tradelane_space_name` (the system file's `tradelane_space_name`, only on the end rings) and joins them with
  "->", ordered by the side of the ring's plane the viewer is on (the end on the viewer's side first). The panel's first
  line then reads "<faction> Trade Lane Ring". `hudinfo::LaneName` reproduces it from the system file and the lane links.
- **The NeuroNet box's owner** keeps its buttons at +0x370..+0x3A4 (built at 0x45BC8D..: `NNMissionLogButton` +0x378,
  `NNMissionLogButtonRoot` +0x394), copied to +0x3B8..+0x3EC; the "new information" flash was not located (open).
- **"Unknown ..." names:** `exe::ObjectUnknown` 0x4D4C70 `bool cdecl(CSimple*)` = a solar (class 0x303) neither visited
  (CSolar::GetVisitValue bit 0) nor in the player's visit list (0x4C4DA0); the target panel (0x4E4094), the contact list
  (0x4D360A) and the indicators (0x4ECB73) then name it by `exe::ObjectUnknownIds` 0x4D4D50 (type bits +0xE0): IDS 1184
  Unknown Moon, 1185 Planet, 1186 Sun, 1187 Unknown, 1188 Jump Gate, 1189 Trade Lane Ring, 1190 Station, 1191 Object.
  SaveC705: both New York gates read "Unknown Jump Gate" (the classic panel too); Fort Bush its name.
- **Not the game's:** the red boxed "SHIELD FAILED" / "HULL BREACH" lines in flight are the subtitles module's Neural Net
  column (danger style), not a native control.
- **The arrival line's house and HDE's plugins** (verified 25 Oct 2026, instance 5): 0x45AFF0 picks the house by the
  system's strid_name - 196609 (lea at 0x45B020, cmp 0x17, the byte table 0x45B1A0 = `exe::ArrivalHouse`: 0 Liberty
  1249, 1 Bretonia 1250, 2 Rheinland 1252, 3 Kusari 1251, 4 none -> "%s System." 1373). **HDE's Territory.dll (Jason
  Hood) detours 0x45B020** (a JMP there in game) and words the line from `DATA\UNIVERSE\Territory.ini` ([Systems]:
  `format = %s System, Border Worlds.` + `systems = Bw01-10, St02c`; its [Houses] is commented out in 0.7.1). **The zone
  lines ("Entered / Left the Jersey Debris Field.") are HDE's Zone.dll** ("Entered %Zv1." / "Left %Zv1.", its own
  string resources, FmtStr::append_zone_id; `Zone.dll upper` in dacom.ini), typed through the same NN_ArrivalText. A
  line that comes while another runs reuses the control (0x45B490 -> 0x45AFF0 on it); a new control may land at the old
  one's address, and its text child's bits are not cleared for a reused line: neither the pointer nor the child is an
  event (HUD 2 words the lines from its own readings). At a load / launch into space the line is 0x45AE90 / 0x45B1C0's
  "%s, %s System." (1372).
- **The target view needs the target panel shown once per target** (verified 25 Oct 2026): the owner's +0x398 (the
  watched object: pointer + 8 -> the CSimple, its id +0xB0) is set by the panel's own frame while it is shown; a target
  chosen while the panel is hidden left the view on the old object (nothing drawn for a fighter at 2 km). Showing the
  root's bits from the frame update until the UI pass hides it again (HUD 2's `hudvis::PulseTargetPanel`, one frame,
  nothing drawn) builds it. The view draws nothing over a pass that wrote the back buffer's **alpha** before it (a
  backing drawn with COLORWRITEENABLE 0xF: the target view vanished, the ship view stayed; 0x7 = fine). The view draws
  only its object (no background, nothing of the rectangle outside it: a rectangle 2x the window let a long ship run
  past the card). **0x4D4C70 reads "unknown" for a route waypoint** from our call, while the target panel shows
  "Waypoint" (the HUD's targetable entry decides first): `hudinfo::DisplayName` skips the test for type bit 9.
  A trade lane ring's classic panel names it by its lane ("Planet Manhattan->Fort Bush"); `DisplayName` gives "Trade
  Lane Ring" (open).

## Room buttons ignored (solved 18 Oct 2026; code core\game\room.cpp uiclick::Click)
- Room buttons (RoomControlN / ButtonControlN, icon button class vt 0x5E15F4, mouse handler 0x55B950) end in the
  NavBar's message handler (NavBar vt[0xA8] = 0x43EE80, `ret 0Ch`: msg, control id, 0). It acts on message **0x26**
  only: gates UiBusy 0x67DCC8, NavBar +0x32C+0x18 / +0x34C+0x18, RoomInfo +0x1338 (all swallow the click), then the
  hotspot record by control (list +0x380, node +0x20) and a dispatch by record type (+0x38: 5 / 7 / 8 room change;
  others 0x440AC0, e.g. Launch, type 4 -> 0x440F94: equipment check 0x4422B0, ship 0x67337C, window "NavBar").
- The icon button's mouse handler sends **0x27** to the owner when the button's byte +0x330 has bit 0x80 (a mode
  that the hover logic sets and clears), else runs the button's action vt[0xB4] = 0x55A660 (a command at +0x34C, or
  0x26 to the owner). FLvibed's click copied that choice without a real hover: the bit could be set (Bar in a fresh
  session, Launch after a few loads) and every room button was ignored, for the rest of the session in the worst
  case. Fix: icon buttons run their action vt[0xB4] directly (signed `exe::IconButtonActivate`): 30/30 load+launch
  cycles (failed by the 3rd before), Bar / Cityscape / NN buttons work.

## Base rooms / characters
- Bar characters are 3D objects, not UI controls. Class vtable 0x5CB930 (constructors write it at
  0x443F79 and 0x4440E0). vt slot 10 = 0x445A90 hover-enter (sets `talk_*` cursor by type,
  +0x2F8 hovered flag), slot 11 = 0x445BE0 hover-leave, slot 12 = 0x445C10 click. Cursor strings
  at 0x5CBB6C.. (`talk_story`, `talk_mission`, `talk_rumor`, `talk_bribe`, `talk_blowoff`).
- Open: the room object container / picking loop that enumerates characters.
- **Identity and conversation (verified in game 22 Oct 2026, Manhattan bar, byte diffs idle / talking / after):** +0x160
  u32 = the character's mbases.ini `individual_name` (bartender 244609, officers 220010-220012; patrons without one 0 or
  1); byte +0x36C = 1 on the player's conversation partner while the conversation runs, 0 after (one character at a
  time). Byte +0x32C goes 0 -> 1 on the first talk and stays (not a partner flag; its word mixes other bytes).
- **Native use (verified in game 17 Oct 2026, both mods):** constructor 0x443ED0 `thiscall(this, a1, a2)` ret 8
  (object 0x3B0 bytes, created at 0x435C4C), destructor 0x4440C0 `thiscall(this)` (scalar deleting dtor 0x4440A0);
  both start `6A FF 68 C8 97 5B 00` (7-byte detours). Tracking them gives the room's live characters (Manhattan bar:
  7). Slots take two unused args (pass 0): vt[10] hover-enter sets the cursor that names the offer, read at
  [0x616858]+4; vt[11] hover-leave restores the arrow; vt[12] click starts the conversation (story scene, rumour,
  bribe, mission offer) exactly like a mouse click. Fields: +0x300 type (2 commodity, 3 ship, 4 equipment dealer),
  +0x1B0 offer key (lookup in map 0x668708; entry+0x220 offer: 0 blowoff, 1 bribe, 2 rumor, 3 mission, 4 info,
  5 story), +0x2F8 hovered flag. Cursor strings 0x5CBB6C.. include `talk_info`, `talk_*_dealer`.
- Characters are not FlControls (not in the control list 0x67DBE8).
- **Screen place (verified in game 25 Oct 2026, Manhattan bar / trader / equipment at 4K, ROOMS track):** +0x2F4 -> the
  character's pick box (vt 0x5CB9EC; +4 = the character): +0x10 int x, y, w, h in screen pixels, top-left origin, kept
  current by the per-frame update 0x4449D0 (which also writes the world position +0x37C and rotation +0x388 from the
  engine); the native name card's box starts at its x (0x444C09..). `baseinfo::CharacterScreenRect`; the pad walks the
  characters left to right by its centre. Room cameras pan: the order changes with the view (the room's camera is not
  `CurrentCamera`, which reads 0 in rooms).
- **Conversation flag:** +0x36C is 1 on the partner AND on the player's own room character (Trent, talk_blowoff, no
  affiliation line) while a bar talk runs, 0 after (the rumour / job talk, 25 Oct 2026). The rumour / info answer is the
  offer dialog with `OfferDialogClose2Button` (CLOSE) and no Accept / Decline; the conversation lasts until it is closed.
- **The player's own character's click (verified in game 25 Oct 2026, Manhattan launch pad, Trent the only character):**
  vt[12] click on Trent (talk_blowoff, no affiliation line) starts nothing - no conversation flag, no line, the room
  unchanged over 3 s. The pad offers no A on him (padbases).
- **Room hotspots → controls (verified in game 17 Oct 2026):** `RoomControl1..9` and `ButtonControl1..9` (names
  formatted at 0x43F335 / 0x43F3B3 from 0x5CB81C / 0x5CB80C) are numbered **per room**, not by function (Manhattan:
  ButtonControl1 = Cityscape/exit in every room, but RoomControl1 = News in the bar, Repair in the ship dealer). The
  room UI builder 0x43F2E0 runs on the **NavBar** control (vt 0x5CB584) and keeps one record per hotspot in an MSVC6
  std::list at NavBar +0x37C (head node at +0x380): node +0 next, +0x10 label (std::string data, length at +0x14;
  the room switch such as `Bar`, `Trader`, `Equipment`, `ShipDealer`, `Cityscape`, else the hotspot name), +0x20
  the FlControl, +0x24 the hotspot. Hotspot (room vector at roomInfo+0x40 → +0x198/+0x19C; roomInfo =
  0x43A460 thiscall(0x668708)): +0 name (`IDS_HOTSPOT_BAR`, `IDS_HOTSPOT_NEWSVENDOR`, ...), +4 room_switch,
  +8 virtual room (`Deck` / `Planetscape` special-cased), +0x10 behaviour (1 room switch, 2 news, 3 mission vendor,
  6 dealer front desk, 7 ship dealer, 12 repair). Clicking the record's control natively (button click, UI
  section) does what the hotspot does. FLController / FLSubtitles debug `hotspots` / `goto <label>` use it.
- **Launch** is a separate record labelled `IDS_HOTSPOT_LAUNCH` (no hotspot data, behaviour 0) in the room with the
  ship (planet: Cityscape, which has `[PlayerShipPlacement] launching_script`; stations: Deck). It exists only while
  launching is allowed: absent in story mission 1 before Juni's job is accepted. Clicking it natively launches
  (docked -> cutscene -> space, verified on a free-roam Manhattan save).
- **When the NavBar acts on a room button:** its click handler 0x43EE80 (vt 0x5CB584 slot 0xA8, message 0x26)
  returns without acting while [0x67DCC8] is set (written at 0x45C914/0x45C921), while NavBar +0x32C or +0x34C
  is busy (byte +0x18, read by 0x55E280; set during a room's entry transition, up to ~9 s), or while the room
  info's byte +0x1338 is set (0x44C4A0). Then it finds the record by control: dealers (record +0x30 = 5/7/8)
  open their windows via 0x449D10, room switches go through 0x441700 (transition lookup "%d," in the current
  hotspot's +0x1C..+0x28 strings, camera `Camera_Track_1`). Open: in the M01 save, the Bar refused re-entry
  with every gate clear (likely the M01 bar cutscenes queued at the first entry, `Act_AddRTC`).
- **The NeuroNet window manager's slides drop a close** (verified 26 Oct 2026, HDE 0.7.1 4K / 4:3 and vanilla 1.1 with
  and without FLUF; `core\flexe.h` NNManager / NNWindowClose / TransitionRunning, `room::NNWindowBusy`): the docked
  windows (dealers, job board, news, ship dealer, repair) are opened and closed by the object at [0x66D3F0] (+0x3A8 the
  open window, +0x3AC its kind 0x63..0x6A; the equipment dealer = 0x66). The dealer's close button (`Inventory_Close`, an
  icon button) reaches NN_Dealer's message handler 0x4822A0 (0x26: the id at +0x4D8) -> 0x45A460 -> the manager's close
  0x45CF60, which **returns doing nothing** while its transition objects +0x330 or +0x350 run (0x55E280 = byte +0x18)
  or no window is open. Right after a window's controls show, both run in turn for ~0.5 s (the open slide; the first
  ~0.3 s at 190 fps measured: B / Escape / the close click at 10-60 frames after `ItemButton1` showed = lost, at 150+
  frames = closed); a close clears +0x3A8 at once and runs +0x350 then +0x330 (~0.25 s) while the room's camera move
  starts. A press in a slide is lost, not queued - the game's own Escape too (vanilla behaviour). A dealer hotspot
  click during the close slide opens nothing (`goto` now answers `busy: nn-window`). In that close frame the game
  stops drawing the room bar's and the NeuroNet box's art while their controls still read shown.

## Equipment dealer: buying natively (verified in game 20 Oct 2026, Manhattan; code core\game\room.cpp, flctl Invoke-FLBaseRestock)
- Opened by the Equipment room's hotspot `IDS_HOTSPOT_EQUIPMENTDEALER` (`goto`; every room's NavBar also lists the
  room button `IDS_HOTSPOT_EQUIPMENTDEALER_ROOM`: match the exact name). Controls: filters `Filter_Weapons`,
  `Filter_Ammo`, `Filter_Equipment`, `Filter_CargoBay`; rows `ItemButton1..5` / `ItemView01..05` (vt 0x5E614C,
  clickable like text buttons), `Inventory_Scroll`, `Inventory_Close`. Manhattan lists: Weapons 18, Ammo 31,
  Equipment 7 (shields, thruster), **Cargo Bay 2 = nanobots `ge_s_repair_01`, shield batteries `ge_s_battery_01`**.
- The rows' parent is the **dealer list** (vt 0x5D192C): +0x3A4 selected row index in the whole filtered list
  (-1 none), +0x978 side (3 here). `exe::DealerSelectedGood` 0x483580 `u32 thiscall(list)` = that row's good id (walks
  the base market [0x66873C] via 0x43A510). The dealer's buy 0x47EAB0 `thiscall(window, count)` buys it: GFGoodBuy
  wrapper 0x438220(base, good, count) (server [0x67ECD0], client [0x673344]), then the stock, the client's cargo.
- A selected row that can be bought shows `QuantitySlider` (a scroll bar, vt 0x5E55AC) and `BuySellTradeButton`
  (vt 0x5E614C); the slider position is the quantity, range 1 .. n with its top position = n - 1 page = what fits.
  Nothing more fits = neither control appears. Clicking BuySellTradeButton buys: the game sends GFGoodBuy
  {base, 0, good, count}, ReqAddItem(good, "BAY", count, 1.0, unmounted), ReqChangeCash(-price) itself (3 nanobots:
  -600). About 0.5 s after a purchase the dealer clears the selection (+0x3A4 = -1, slider hidden): a row clicked
  before that is deselected again.
- **Scroll bars** (vt 0x5E55AC, mouse handler `exe::ScrollBarClick` 0x596D30): range +0x3C0 .. +0x3C4, position
  +0x3C8, page +0x3CC; its page click = position += page, `ScrollBarClamp` 0x595CA0, `ScrollBarLayout` 0x595840,
  parent vt[0x5C](0xF005, bar, 0), window vt[0xA8](0x3E, bar, (int)position). `uiclick::Scroll` does exactly that
  (debug `scroll <Control> <delta> [page]`); one unit = one list row. The controller's D-pad navigation uses it: up /
  down on a list's edge row scrolls the row's sibling scroll bar one row (verified on Manhattan's 31-row Ammo list).
- **The player's equipment list (server side, readable docked):** server.dll `pub::Player::EnumerateCargo` (export
  0x74D20 in this build) walks it and reports only UNMOUNTED items to its CargoEnumerator. Its code gives the layout:
  player records = [global array] with [global count], stride 0x418, "in use" +0x348 of record (client-1); the list
  (MSVC6 std::list) at array + client * 0x418 - 0x1A0, head at +4; node +0x0A u16 sub-object id, +0x0C archetype id,
  +0x10 CacheString hardpoint (char*), +0x14 bool mounted, +0x18 float health, +0x1C int count. Debug `equipment`
  (core\game\space.cpp) reads it after checking every instruction byte it relies on. Mounted launchers + ammo
  counts (verified 20 Oct 2026, Save1148: torpedo / 2 missile / mine / CM launchers mounted, their ammo as cargo).
- Purchases are net-worth neutral: cash -x, ship assets +x (verified at 4 bases), so the story's net-worth gates do not
  move; restock fills what fits (the slider's top = capacity minus carried: br_elite empty -> 20 = its capacity).
  Verified at Li01_01 (Manhattan), Br05_01, Ku01_05, Li01_12 (nanobots + batteries under Cargo Bay at every one).

## Base windows and NeuroNet windows: control names (verified in game 25 Oct 2026, HDE 0.7.1, Manhattan, 4K; the pad screens of controller\src\impl\padbases.cpp)
Every window is a root control named after its class; a window is open when its root is visible (`visible <name>`).
- **Mission offer** (a bar character's job, a story offer): root `ScriptSequence_OfferDialog` (class NN_OfferDialog),
  `OfferDialogAcceptButton` / `OfferDialogDeclineButton` (text buttons, centres 1593 / 2187 x 1539, half 189 x 81 at
  4K), `OfferDialogCloseButton` (icon, 2781 x 567), `OfferDialogOfferText`, `OfferDialogTitle`. While it is up the
  NavBar is busy (`ui-flag 67DCC8`); the room's talk scene before it reads `busy: scene`.
- **Job board** `NN_MissionVendor` (the bar's IDS_HOTSPOT_MISSIONVENDOR, behaviour 3): five rows
  `MissionVendorSummaryBox0..4` (text buttons, 1537 x 594 + 115 i, half 458 x 58) with `MissionVendorIconFrame0..4`
  (957 x same y), per row `...SummaryDest / Title / Reward / Icon N`; the list's scroll bar
  `MissionVendorSummaryScroll` (vt 0x5E55AC; Manhattan: range 0..9, one unit = one row; **not in the D-pad navigation's
  item set** - the pad code scrolls it natively on the edge rows), `MissionVendorAcceptButton` (2612 x 1121, half 249 x
  49), `MissionVendorCloseButton` (3012 x 416), the chosen job's `MissionVendorMissionTitle / Dest / Reward /
  FullText / Reject`. Clicking a row (uiclick::Click) selects it: the description follows.
- **News** `NN_News` (IDS_HOTSPOT_NEWSVENDOR, behaviour 2): `NewsHeadlineBox0..3` + `NewsHeadlineIconFrame0..3`
  (the same geometry as the job rows), `NewsHeadlineHeadline0..3`, `NewsStoryHeadline / Text / Icon`, `NewsTicker`,
  `NewsCloseButton` (3025 x 405). No scroll bar (four headlines).
- **Commodity trader / equipment dealer** (one window class `NN_Dealer`, vt 0x5D192C, plus the player's side
  `NN_Inventory`, vt 0x5D0EEC): the lists' rows `ItemButton1..5` / `ItemView01..05` (both windows use the names: `#2`
  = the second visible one, the player's), per row `ItemMesh N`, `ItemName0N`, `ItemPrice0N`, `ItemGoodDeal0N`
  (the price mark), the player's rows `ItemQuantity0N`, `EquipToggle N` (vt 0x5E1764 ToggleButtonControl: the mount
  checkbox), `ItemDamage N`, `CargoMissionText N`; `Inventory_Scroll`, `Inventory_Close` (3240 x 459); the transfer
  panel once a row is selected: `BuySellTradeButton` (3365 x 1412, half 300 x 58), `BuyText` / `BuyArrow`,
  `TransferItemButton / View / Mesh / Name / Quantity / Price / GoodDeal`, `QuantityPrompt`, `QuantitySlider` (1944 x
  999, half 324 x 27), `QuantityText`, `QuantityPrice`, `QuantityTotal` ("Quantity: / Price: / Total:" lines),
  `QuantityEdit` (vt 0x5E501C, a text field), `PlusMinus`; the headers `PlayerTitle`, `PlayerCash`, `WelcomeText /
  Line / Help`, `Full_Cargobar`, `CargoLabel`, `InfoCard_Info` (the infocard), `ShipView_Info`; the equipment
  dealer's filters `Filter_Weapons / _Ammo / _Equipment / _CargoBay` (icon buttons, children of the NN_Inventory side).
  A window's side for any control: walk the parent chain to `NN_Dealer` (the dealer's) or `NN_Inventory` (the
  player's). The text controls (vt 0x5E4F24) keep no readable string in their first 0x3A8 bytes (the pointer at +0x338
  is a render object): the pad's quantity line reads the slider (position = the quantity, range top - 1 = what fits),
  not the game's price text.
- **Ship dealer** `NN_ShipTrader` (vt 0x5D593C; IDS_HOTSPOT_SHIPDEALER, behaviour 7): the offered ships
  `ShipTraderShipNameFrame0..2` (2907 x 797 + 148 i, half 300 x 74) + `ShipTraderShipNameIconFrame0..2` (2511 x),
  per ship `ShipTraderShipName / Class / Price / Mesh N`; the player's `ShipTraderCurrentShip / Frame / Name / Class /
  Price / Mesh / Text`; `ShipTraderCancelButton` (3237 x 450). Clicking a ship row selects it: the middle panel shows
  the 3D view `ShipTrader3DShip`, `ShipTraderViewName / Class`, the infocard `ShipInfoCard_Facts / Stats / Values`;
  the buy button is `ShipTraderBuyButton` (with `ShipTraderBuytext`; Freelancer.exe's string table) - it did not
  appear on SaveC701 (rank 0: "You must be level 1 to purchase this"), unverified in game.
- **NeuroNet windows docked:** mission log = root `NeuroNetStoryStar` with the tabs `MissionButton` (1454 x 1394) /
  `LogButton` (2359 x 1394) (text buttons), `StoryStarHistoryText`, `ScrollV_1`, `StoryStarCloseButton` (2816 x 388);
  player status = `NN_PlayerInfo`: `PlayerInfoFactionFrame0..5` (text buttons: the faction rows) with `...Name N`,
  `PlayerInfoUnfriendlyBar N` / `PlayerInfofriendlyBar N`, `FactionInfoCard`, `PlayerInfoScroll`,
  `PlayerInfoHeading0..12` / `PlayerInfoStat0..12` (the statistics, in the same window: no REPUTATION / STATS tabs),
  `PlayerInfoCloseButton` (2816 x 365); help = `NN_Help` with `PlayerInfoText`, `ScrollV_2` and **a close button also
  named `PlayerInfoCloseButton`** (2798 x 368). The inventory button (`NNInventoryButtonRoot`) opens nothing while
  docked (the inventory shows inside the dealer windows: `NNInventoryButton`).
- **Nav map** (verified in game 25 Oct 2026, SaveC702, 4K): the root is **`NeuroNetNavMap`** (class NN_NavMap, vt
  0x5D2F84), **under the HUD's NeuroNet root `NN_MainButton`** - so are the inventory, log, status and help windows:
  "a window over the HUD's roots" does not see them (the mode detector reads their roots by name). Visible in the
  system view: `NavMapUniverseButton` (2604 x 842, half 34), `NavMapLabelButton` (2691), the filters
  `NavMapPhysicalFilter` (2924) / `PoliticalFilter` (3011) / `PatrolFilter` (3099) / `mineableFilter` (3187) /
  `LegendFilter` (3274) / `BaseListFilter` (3362), `NavMapCloseButton` (3590 x 842); the universe view shows
  `NavMapShipButton` (back to the ship's system) instead of the universe button; `NavMapSystemName`, the grid
  `NavMapGridLetterA..H` / `NavMapGridNumber1..8`, the base list `NavMapBaseText0..14` / `NavMapBaseSystemText0..14` +
  `NavMapBaseListScroll`, `NavMapInfoCard / InfoText / InfoTitle`, `NavMapLeftWindow` / `NavMapRightWindow`. **Not
  FlControls while nothing is selected**: the zoom (`NavMapInZoom` / `NavMapOutZoom` exist in the string table but
  were never visible), `NavMapSetWaypointButton`, `NavMapClearWaypointButton`, `NavMapClearAllWaypointsButton`,
  `NavMapBestPathButton`, `NavMapSelectObjectButton`, `NavMapJumpGateButton` (the string table also has
  `NavMapContinueButton` (the mission briefing's), `NavMapCloseInfocardButton`, `NavMapAllFilter / MissionFilter /
  FriendlyFilter / HostileFilter`, `NavMapMissionObjective1..3` + `...Scroll`, `NavMapAbortMissionButton` + `...Yes /
  NoButton`). The map's objects are drawn by the map (hotspots `NAV_*`: `NAV_addwaypoint`, `NAV_zoomin`,
  `NAV_universebutton` ...), not FlControls. The pad's route: `space::PlotBestPath` to a chosen object fills the
  waypoint list the way the map's own best path does (Fort Bush -> Colorado gate: 3 entries), and the autopilot's
  travel flies it.
- **ToggleButtonControl (vt 0x5E1764: the player's `EquipToggle N` mount boxes, the Options screen's `opt_toggle N`;
  verified in game 25 Oct 2026, `uiclick::Click`):** its mouse handler vt[0x4C] = **0x55CD20** `exe::ToggleButtonClick`
  picks the click sound by the state (+0x3A0: 0 -> the std::string +0x3A4 / sound id +0x3C4, 1 -> +0x3B4 / +0x3C8) into
  +0x360 / +0x374 and hands over to the icon button handler 0x55B950, whose release runs the control's action vt[0xB4] =
  **0x55CC90** `exe::ToggleButtonActivate`: the command +0x34C through 0x5763B0 when set, else msg 0x26 (id, 0) to the
  owner window (parent vt[8]), then the state +0x3A0 flips and the owner gets msg 0x30 (id, the new state). The
  equipment dealer acts on the 0x26: the row's item is unmounted / mounted (server `equipment`: HpWeapon01 -> BAY and
  back) and **the inventory list re-sorts at once** - mounted items first, then the empty hardpoint rows (Mine,
  Countermeasure: `ItemView N` without an `EquipToggle N`), then the unmounted ones - so the row under the focus shows
  another item after each toggle. A purchased weapon with a free hardpoint is mounted by the purchase itself (the
  Lavablade went to HpWeapon03 on buying).
- **Control placement, the frame transform (verified in game 25 Oct 2026, `ui::Collect`):** the 4x3 matrix at +0x3C
  (3x3 at +0x3C, translation at +0x60) is the control's parent frame. The window classes (NN_Dealer, NN_Inventory, the
  other NeuroNet windows) place themselves absolutely - NN_Dealer / NN_Inventory at (-0.644, 0.330, -1.288) at 4K 16:9,
  their own +0xA8 stays 0 and their parent is the unrelated icon button `NNInventoryButton` (+0xA8 (0, -0.075, -1.25))
  - and stamp that frame into every child, so a child's world position = its frame x its own +0xA8, NOT the parents'
  +0xA8 chain (which put the player's rows at the screen centre and the dealer's off screen). Controls under the plain
  roots (the HUD, the NeuroNet buttons) have a zero frame and the chain holds. Each dealer row: +0xA8 (1.084, -0.131)
  dealer side / (0.248, -0.131) player side, box half (0.138, 0.034) -> 300 x 74 px, 148 px row pitch; the toggles at
  x 0.047, 0.021 above the row; `Filter_*` (0.226, -0.463); `Inventory_Close` (1.251, -0.040); `BuySellTradeButton`
  (0.645, -0.073). `NNInventoryButton` itself is a 40 px invisible hit-testable button that lands under the player's
  second row; the game's hit test gives the row there, so `ui::CollectAll` drops tree items whose centre the hit test
  gives to another control.
- **The game's hover out of focus:** with the game in the background the hovered-control pointer (0x67DCD8) is not
  recomputed when the virtual cursor moves (it stayed on `MissionVendorSummaryBox4` with the cursor on
  `MissionVendorIconFrame4`); the hit test (`ui::HitTest` = 0x59DD80 with the cursor's ray) answers for any position.
  Code that needs "the control under the cursor" uses the hit test.
- **Room character hover** (`room::Hover`): the hovered flag +0x2F8 stays set (re-asserted twice a second) and the
  game shows the character's name card ("Liberty Police, Inc. / Detective Kim Bailey") and the `talk_*` cursor - the
  game's own highlight follows a native hover.

## Extended HUD 2: window art and events (25 Oct 2026, HDE 0.7.1; symbols `core\flexe.h` block "Extended HUD 2", HostApi v23)
Sources: the FEASIBILITY probe (`research\evidence\controller_ui\hud2x_feasibility.md`, trees `hud2x_trees.md`, frames
`research\experience\runs\hud2x_probe_2026-10-25\`), the CAPTURE track's layers (`research\experience\runs\
hud2x_layers_2026-10-25\README.md`, `*_rects.json`), the HOST-X track's disassembly and counter test (instance 4, below).
- **Two families of window art**, both removable with every native text, list, button, 3D view and hit box kept:
  1. **Art children**: static image controls (`exe::VtImage` 0x5E4D7C, draw 0x588770) - `ManeuverFrame` / `ManeuverTab`
     (NavBar), `NNBackground`, the job board's / news' Background + Headlines / Story / Video frames, `PauseFrame` +
     `Pause_BlackPlane`, `LoadSave_*`, `OfferDialogBackground`, `OptionsBox` ... Hide by clearing **bit 1 only** of +0x6C
     (bit 0 stays: `uiclick::FindVisible` and the harness still see them; leaves, so nothing else hides).
  2. **Self-drawn art**: the window (or button) draws its own mesh. `exe::WindowBaseDraw` (0x5A0760, the NeuroNet window
     base draw) draws the mesh instance at **+0x80 while its handle +0x84 != -1** (0x420D60), tinted by the shared global
     0x679BB4 (87 C3 E0 C0), then the children (0x59E5C0 = the FlControl base's draw, vtable slot +0x34) - so +0x84 = -1
     removes only that art. Classes: NN_Inventory (INV_biginforcard.cmp = all four dealer panels; NN_Dealer has none),
     NN_ShipTrader, NN_PlayerInfo / NeuroNetStoryStar / NN_Help (CHAR_infowindow.cmp), NeuroNetNavMap
     (navmap_background.cmp; its side windows are art children), the save screen's FRONT_button frames. The draw slot
     (+0x34 of the vtable) is WindowBaseDraw itself only for NN_Help; NN_Inventory / NN_Dealer 0x479700, NavBar 0x43E6C0,
     NN_Pause 0x488960, NN_ShipTrader 0x4B9260, NeuroNetNavMap 0x4926D0, NeuroNetStoryStar 0x4BC940, NN_PlayerInfo
     0x4A8200 call or jump into it (exe 0.7.1, read 25 Oct 2026). Trust +0x84 only on the listed classes (by vtable,
     `exe::Vt*`): on the main-menu buttons +0x84 is no handle (0x7FFF). The game did not rewrite +0x84 while a window
     lived; windows are created per open and destroyed on close.
- **Text colour**: TextControl (`exe::VtTitleText` 0x5E4F24, draw 0x58A7F0) keeps its colour at **+0x348, bytes R G B A**
  (default 0xFFE0C387 = #87C3E0); a field write recolours it and the game kept it across a row refill. Not on the rich
  text (`exe::VtText` 0x5E476C: +0x348 holds 0x370, its colours are markup) nor on text buttons (0x5E614C, the native
  selection yellow). Recolouring the art (+0x348 / +0x410 / +0x414 on a static image) has no visible effect: the look is
  the texture, and the tints are shared globals.
- **The NavBar re-shows its frame / tab every frame**: `ManeuverFrame` / `ManeuverTab` (and its buttons) get bit 1 back
  within a frame, in a plain room too (the layers' finding; the feasibility probe had seen it only with a window open):
  re-clear them each frame from the under-HUD pass (one compare + byte write each), as the flight HUD's roots are. The NN
  box's children and every other art child / +0x84 stay as written. The main menu re-creates its `bImg` frames each
  time it shows (new addresses).
- **Hit boxes vs drawn frames**: a control's hit box (+0xB4 halves around +0xA8) is not where its art draws: the pause
  buttons' drawn frame sits **~0.027 H below** the hit box (from the layered captures' rects vs pixel regions,
  `pause_docked_rects.json`; `docs\UI_HUD2_EXTENDED.md` 9). Window roots and static images have no usable projected rect
  (`ui::ControlRect` gives the frame origin with a zero or bogus half): take an art's extents from fixed fractions of H
  per class (measured once), the content's from the content controls' rects.
- **Learning that windows opened / closed without polling (the UI generation, HostApi v23 `UiGeneration`)**:
  - **Add**: `exe::UiControlAdd` 0x59DBF0, `void cdecl(FlControl*)`: appends to the flat control list (`exe::UiControls`,
    list object {count, head, tail} at 0x67DBE4); every control class's Init calls it right after SetParent (0x59DD00)
    and then clears the deleted bit 2 of +0x6C (e.g. 0x4B506F, 0x5815B4, 0x59A4E4; ~20 call sites). CreateControl
    0x59DB20 (the class-name factory FLUF hooks) does not add.
  - **Remove**: `exe::UiControlRemove` 0x59DC00, `bool cdecl(FlControl*)`: sets bit 2 of +0x6C (already set: returns 1 at
    once), clears the hovered / focus globals (0x67DCDC, 0x67DCE0, `exe::HoveredControl`), detaches from the parent and
    moves the list node to the pending list (0x67DBD8; the UI update purges it, 0x59E1E1..). The FlControl destructors
    call it (0x59FAFF, 0x59FB46, 0x59FBC2: one call per destructor level, so ~3 calls per control - the later ones return
    at once) as do ~20 class destructors. Game shutdown's clear-all 0x59D6D0 inlines the same (not hooked). The base
    constructor 0x59F860 / destructor keep a live-control count at 0x67DCD4 (not used).
  - **Other plugins**: HDE's **HudFacility.dll** (adoxa) puts a JMP at 0x59DC09 inside UiControlRemove (the
    not-deleted branch) - the signature wildcards it and FLvibed's entry detour steals 0x59DC00-06 only; **FLSharp.dll**
    patches NN_ShipTrader's operator new size at 0x4B9739 (0x448 -> 0x454), so `exe::VtShipTrader`'s factory signature
    wildcards the size. Both seen in game 25 Oct 2026 (the first start refused the two symbols; fixed, 175 / 175).
  - **The counter**: the controller host detours both (chained `hk::Detour`, gated together; one increment each, nothing
    else); `UiGeneration()` = 1 + adds + removes, 0 = unavailable. Host debug `uigen` (the controller channel, answered by the
    host: `uigen <gen> adds <n> removes <n>`). Measured on instance 4, SaveC701 (Manhattan), dev build: calm Cityscape 300 frames and
    calm Bar 300 frames **unchanged**; a room change by the NavBar (Cityscape -> Bar, Bar -> Trader) **unchanged** (the
    bar and its buttons are kept and relabelled); job board open +48 adds in one burst ~0.6 s after the click (the
    room's walk-up first), then still for 300 frames while open; closed +141 removes (+3 a few frames later); pause +16
    adds, resume +48 removes; the commodity dealer +129 adds (+3 removes) ~2 s after the hotspot click, still for 300
    frames, closed +387 removes; launch to space then calm flight 400 frames **unchanged**. So: no per-frame churn in calm
    rooms or flight; a window = one burst. The logic compares once per frame and resolves windows only when it moved.
- **The open slide and +0x84** (25 Oct 2026, instance 3, code `uiskin.cpp` Repose): a FlControl's pose setters (vtable +0x14 position, +0x1C orientation; the base implementations 0x5A0880 / 0x5A0950, which the base class also calls on every child with the parent's values) store the pose at +0x60 / +0x3C and hand it to the engine only while +0x84 holds a mesh handle. A window whose art was hidden (+0x84 = -1) during its open slide keeps the mesh at its first pose: restoring +0x84 alone leaves the art blank (off screen) until the window reopens; calling the two setters again with the stored values (children included) brings it back at once.
  **The setters are signed** (`core\flexe.h` block "Extended HUD 2", 25 Oct 2026; found at their v1.1 addresses in HDE 0.7.1 and the current HDE repository's exe): `ControlSetPosition` 0x5A0880 and `ControlSetOrientation` 0x5A0950 (both `void __thiscall(FlControl*, const float*)`, `ret 4`), plus the two position overrides that call the base first - `InventorySetPosition` 0x47FBB0 (NN_Inventory and NN_Dealer, vt 0x5D0EEC / 0x5D192C: the base, then the partner window at +0x360 through its own vt[0x14] with the same position, so the dealers' panel pair moves as one) and `NavMapSetPosition` 0x4948A0 (the base, then the position plus a fixed offset at +0x3C8). The vt[0x14] / vt[0x1C] of the other signed window classes (from the exe's vtables): base on NavBar, pause, save / load, Options, maneuvers, job board, news, ship dealer, repair, confirm, mission log, help, Player Status, offer, NeuroNet box; own overrides on the icon button (0x55B4A0 / 0x55B5C0: offsets by vt[0xA4], not a base call), the main-menu family and ToolTip (0x4B5020 / 0x4B5040), the texts (0x57B0A0, 0x58AE70), the image (0x5896B0). Repose calls a slot only when it holds one of the signed setters (else no repose for that control, logged once).

## Extended HUD 2 readers (verified in game 25 Oct 2026, HDE 0.7.1, instance 6, SaveC701 docked Manhattan, 4K; code `core\game\baseinfo.*`, `controller\src\impl\uistrip.cpp`, debug `uistrip [state] | data | research <ids n|equip|gun a|arch a|feel nick|hints> | hover <i>`; signed sites `core\flexe.h` block "Extended HUD 2 readers")
- **The strip's values:** base = `pub::GetBaseStridName(strid, base)` (exe::PlayerBase) -> "Planet Manhattan"; system as
  in "HUD 2 readers" but from exe::PlayerSystem (hudinfo's reads the ship: "" docked); ship = `pub::Player::GetShipID` ->
  shipname ("Starflier" = ge_fighter); hold = Archetype::Ship +0x100 (hold_size 20 for ge_fighter, shiparch.ini) minus
  `pub::Player::GetRemainingHoldSize`; credits = InspectCash. **The room:** `pub::Player::GetLocation` ->
  `pub::GetLocationNickname` = the base's nickname + "_" + the room ("Li01_01_ShipDealer"); that suffix equals the label
  of the NavBar hotspot record that switches to the room, and **that room button's hint IDS (+0xCC) is the game's own
  room name**: ButtonControl1..5 at Manhattan = 6501 "Launch Pad" (Cityscape), 6506 "Bar", 1319 "Commodity Trader",
  1320 "Equipment Dealer", 1321 "Ship Dealer"; RoomControl1 in the bar 6502 "News".
- **Hint IDS:** FlControl property message 3 (0x5A13C0's jump table at 0x5A1638 -> 0x5A15A9) stores it at **+0xCC**; it is
  the tooltip text the game shows on hover and stays readable while the control is visible.
- **The price marks (`ItemGoodDeal0N`, static images, only in the commodity trader: created when the dealer's kind
  +0x978 = 1):** the row fill (0x47CEFF..0x47D03A) compares the base's price (0x438060 on the base's market entry) with the
  good's GoodInfo and sets both the mesh (table 0x611A4C: 0 green circle, 1 red circle, 2 yellow circle, 3 green square,
  4 red square, 5 yellow square, `Interface\NeuroNet\Inventory\INV_*.3db`) and the hint: **dealer's list** (+0x360 = 0)
  price <= GoodInfo +0x5C -> IDS 1047 "This is rated a good buy price on this base." (green), < +0x60 -> 1049 (yellow),
  else 1048 (red); **the player's list** (+0x360 = the dealer) price >= +0x64 -> 1050 (green), <= +0x68 -> 1051 (red), else
  1052 (yellow). Manhattan: Boron $155 / Consumer Goods $66 / Luxury $180 green (1047), Food Rations $50 yellow (1049),
  H-Fuel $360 red (1048) - read back from +0xCC, matching the squares on screen.
- **A pending purchase:** NN_Dealer (vt 0x5D192C) +0x360 = 0, +0x3A4 the selected row (>= 0), NN_Inventory's +0x3A4 = -1
  (its +0x360 points at the dealer), `BuySellTradeButton` visible. `QuantitySlider` only for goods bought in numbers
  (range +0x3C0 = 1 .. +0x3C4 = n + 1, position +0x3C8 = the quantity: Consumer Goods 1..21 with 20 free); **single
  equipment (a gun) shows no slider**, the quantity is 1. The unit price = `pub::Market::GetPrice(base, good)` (Consumer
  Goods 66, Lavablade Mk II 860 = the panels' Price / Total); the good id is the equipment archetype id (same nickname);
  Archetype::Equipment **volume +0x5C** (Common.dll's reader at 0x62F438F: commodities 1, guns 0).
- **Gun archetypes (Archetype::Gun, class-checked by its exported vtable):** refire_delay +0x90, muzzle_velocity +0x94,
  the projectile's id +0x98 (CELauncher's ctor 0x62992B0 looks it up), **hp_gun_type a vector of u32 at +0xC0 (begin) /
  +0xC4 (end)** (Justice Mk I = {15}; class 2 guns another value: a class 2 hardpoint takes both, so "same type" alone
  misses the obvious comparison - the compare falls back to the first mounted gun of the same kind); Munition hull_damage
  +0x80, explosion id +0x24, lifetime +0x70, seeker +0x90 (2 = LOCK). Lavablade Mk II: 122.4 per shot, 2.00 / s, 600 m =
  the infocard's "Hull Damage Per Shot 122, Refire Rate 2.00, Range 600m"; Justice Mk I 16.3, 8.33 / s, 600 m. The
  mounted guns docked come from the server's equipment list (the EnumerateCargo walk of "Equipment dealer" above).
- **The ship dealer (NN_ShipTrader, vt 0x5D593C):** +0x3CC offered ships (3), **+0x3D0 the selected one** (-1 none),
  +0x3A8[i] its price (7600 / 17020 / 9170 = the list), **+0x3A4 = credits + the current ship's trade-in as the game counts
  it** (506750 with 500000 credits: 6750 = the hull's $4,800 shown in the panel plus its equipment), +0x3FC[i] the refusal
  (1 = level: "You must be level %d to purchase this." IDS 1563 with **+0x408[i] the level**: Patriot 1, Startracker 4,
  Rhino 1 on SaveC701; 2 = IDS 458782 "You must have a reputation of at least %d%% with the base owners ..."), else
  price > +0x3A4 -> IDS 1178 on `ErrorHelp` (+0x3A0); no refusal -> `ShipTraderBuyButton`. Sites 0x4B8F23 / 0x4B8F55 /
  0x4B904F. The 0.65 at +0x3F8 is unexplained.
- **Room characters' name card:** while hovered (+0x2F8) the per-frame code (0x4449D0, site 0x444BE8) shows the wide string
  at **+0x2FC**, lines separated by '\n': faction, [role,] name ("Liberty Population / Manhattan Bartender / Bob
  Swaffar", "Liberty Police, Inc. / Detective Kim Bailey"); the player's own character in the room is "Trent" (one line,
  no affiliation). The card is the top-level control **`ToolTip`** (vt 0x5D5F7C): its position projects to the card's
  top-left corner (739, 1022 at 4K = the drawn box's corner; its extents are garbage). **That position is the ToolTip's
  frame translation: +0x3C frame, translation at +0x60 (x, y, z in UI camera space, z < 0)** - measured 25 Oct 2026 at
  4K (VERIFY, instance 8): (-0.0846, 0.0686, -1) projects to 1682, 886, the native box's corner 1684, 889. **+0xA8 is not
  a position in this class** (+0xAC holds 0.625): `ui::ControlRect`, which adds +0xA8 as other controls keep their
  position there, put Kim Bailey's card at y -864 (off the top of the screen). Read +0x60 for the ToolTip
  (`uistrip.cpp` TipCorner).
- **The ToolTip's re-placement by the hover code, and moving it** (read and verified 25 Oct 2026, instance 3, SaveC701,
  3840x2160 and 1280x800; code `uistrip.cpp` NativeCard, class signed as `exe::VtToolTip` 0x5D5F7C): the global ToolTip
  is at 0x67244C. While hovered, the character's per-frame code (0x444C09) passes the box's top-left minus 30 px (x, y -
  30, converted to UI camera space with z = -[0x67DC9C]) and the text to 0x4BE370 -> 0x4BDEA0, which re-places the card
  (0x4BD880 -> 0x4BDA70: clamps it inside the screen, then vt[0x14] = 0x4B5020) **only when the text (+0xF4) or the passed
  position (+0x13C / +0x140, z +0x144) changed** - with the room camera's sway that is every 0.5-1.3 s, so a single write
  of +0x60 is undone then. **Its draw (0x4BDFE0) projects +0x60 itself every frame** (0x4BE0B2 -> 0x417730) and sizes the
  box from **+0xB4 / +0xB8, the text's extents in pixels** (written by the draw from 0x414570; the drawn box measured 114 x
  66 px at 4K for +0xB4 110 / +0xB8 63, "Trent"), shown while +0x6C bit 2 is set (cleared on hover end: 0x4BE390). So a
  write of +0x60 after the frame update and before the UI pass (OnRenderUnderHud) **holds every frame**: 8 shots over 4 s
  of sway all drew the card at the written place, the game's own +0x13C kept updating. Two characters can claim the hover
  (+0x2F8: the pad focus's and the cursor's); the one that placed the card last is the one whose box matches +0x13C.
  The injected DirectInput cursor does not pick a character in a background window (no hover); the game's hover slot
  (`room::Hover`, slot 10) does.
- **A list row's "selected" flag** (text buttons, vt 0x5E614C: `MissionVendorSummaryBoxN`, `NewsHeadlineBoxN`, the
  dealers' `ItemView0N`): **+0x35C bit 0x10** (0x60 -> 0x70 on the row the game selects, 0x70 -> 0x60 on the one it
  leaves; +0x384 bit 2 moves with it; the job board window's +0x378 / +0x44C hold the selected row's index). Measured 25 Oct
  2026, instance 3, SaveC701, by native clicks and a diff of the rows and windows; `uiskin.cpp` SelectionFor.
- **Player Status' faction names are not coloured through +0x348**: every `PlayerInfoFactionNameN` reads 87 C3 E0 FF
  (the default) while the game draws them white / green (SaveC705, 25 Oct 2026) - the attitude colour is applied
  elsewhere (not found); a recolour of the hostile names via +0x348 has no effect.
- **The front-end load screen's controls have no projected extents**: `ui::ControlRect` of `CharSelDesc` gives the list's
  first row's top-left (1560, 510 at 4K) with half 0, `LoadCharacter` a point that is not its centre (1732, 982). [+0x2F4]+0x10 holds four ints
  (729, 968, 296, 494 for Elaine Frazer) that the card's placement starts from - not the card's rectangle itself.
  **Conversation partner:** +0x36C = 1 on the giver *and* on Trent while a talk runs (take the one with an affiliation).
- **Attitude by the card's faction line:** initialworld.ini [Group] ids_name -> nickname -> `pub::Reputation::
  GetReputationGroup`, the player's rep (`pub::Player::GetRep`, 1) and `pub::Reputation::GetReputation(rep, group,
  feel)`: li_n_grp 0.91, li_p_grp 0.65, fc_lr_grp -0.65 (friendly >= 0.6, hostile <= -0.6). **Names are not unique:**
  "Liberty Navy" is both li_n_grp and fc_ln_grp (0.91 / -0.65): such a name counts only when all its groups agree.
- **Job board:** the type word is the hint IDS of `MissionVendorIconFrameN` / `SummaryBoxN` and of the detail's
  `MissionVendorIcon` (13050 "Kill Ships", 13052 "Assassinate"); "Difficulty : n" and "Reward : n" are lines of the detail
  text itself (and of the offer dialog's `OfferDialogOfferText`) - no separate field read.
- **Text controls (vt 0x5E4F24) have no extents:** ui::ControlRect gives their box's top-left point (the anchor), e.g.
  `MissionVendorMissionReward`, `OfferDialogTitle`; the art `OfferDialogBackground` has none either.
## Saves (verified in game 17 Oct 2026)
- **Writing a save natively (18 Oct 2026):** server.dll `?Save@pub@@YAHII@Z` `pub::Save(client, ids)` - what missions'
  `Act_Save` calls (Content.dll imports it; FLvibed logs `mission-save ids=`); the game picks the file name (SaveXXXX,
  hex) and describes it by string `ids`. Returns 0. Also `?SaveGame@IServerImpl@@UAEXABUCHARACTER_ID@@PBGI@Z` (the
  save menu's path, not used). **A save made mid-mission restores the story mission at that point** (position,
  objective, its ships respawned), not at the last checkpoint. Names outside the hex pattern (`SaveS204.fl`) are listed
  and load normally. **But not every moment loads true** (session 11, 24 Oct 2026, M12): a save written while the story
  waits (at rest before a banner) does; one written **at** a trigger's moment (a generator's kill) loads stuck on the old
  objective with the target gone; and after any mid-mission load the objective reads "Awaiting" until the script sets
  the next one. The file lists the live triggers (`[TriggerSave] trigger = <CreateID>` + their condition state). A native
  save in flight costs ~120 ms of the harness's wall time, the game drawing on (~150-190 fps: no stall).
- **Act_NagDistTowards** (session 11, M09 / M07 scripts): `OBJ, <name>, <nagger>, <object>, <fail ids>` or `POS, <name>,
  <nagger>, x, y, z, <fail ids>[, <dist>][, NAG_ALWAYS]`, set together with an objective: the player not closing on the
  target draws the nagger's lines (~20 s apart in M09) and then fails the mission with the ids (M09: "You failed to fly to
  the Blood Dragon Hideout" after ~63 s held in a fight). `core\game\mission.cpp` TowardsNagOf / NextStepGoals.
- **The load screen lists at most 256 saves** (verified 20 Oct 2026: 261 files in the folder, `saves` = 256, the
  requested one missing). Test runs keep adding the missions' own key saves (`Act_Save`), so `deploy.ps1` moves the
  oldest SaveXXXX files to `SAVE\overflow\` above 240 (never deletes).
- Freelancer reads the content of every save when it starts (the load screens list that startup scan): replacing
  a listed save's file while the game runs still loads the startup content, and a new file is not listed until a
  restart. Tools must place a save before starting the game.
- 0x5B2C70 `LoadGame(const CHARACTER_ID& cid /* char[512] file name */, bool)` only **requests** the load: copies
  the id to 0x67EA78 and sets the pending state [0x67ECC4] = 7, processed by the next frames. Called alone, the base
  rooms come up without characters; the load screens wrap it with the steps that make a full load (below). Loading
  by name from any state works through those screens (verified 17 Oct 2026, both directions space/docked, < 1 s).
- **Visit record: what the player has been exposed to** (22 Oct 2026; saves decoded with `tools\flsave.js`, layout from
  FLSDK / FLHookSDK `FLCoreServer.h`, `Freelancer\GameData.hpp` - not yet read at runtime): the game keeps one flag
  byte per object the player has met - bases, solars, zones, factions - saved as `visit = <id hash>, <flags>` lines in
  `[Player]` (BC46 Save7e1c, the M01a start: 2 lines - Manhattan 30, a faction 65; Save3989, M13: 1,809 lines - 1,173
  x 41, 543 x 1, 43 x 65, 25 x 31, 20 x 45, ...). Bits (`VisitFlag`): 1 Known, 2 Unknown, 4 MineableZone, 8
  ActivelyVisited ("used by patrol paths and looted wrecks"), 16 Solar, 32 Zone, 64 Faction, 0x80 Unknowable; so 41 =
  a zone flown through, 31 = a solar visited, 65 = a faction met. Server side per player: `PlayerData` visit map
  `st6::map<uint, char>` at +0x3A0 (FLSDK; SetVisitedState / FLPACKET_COMMON_SET_VISITED_STATE send changes to the
  client); client side `CSolar` visit value (+121 in FLSDK's layout), `ISystem` / `IZone` visit bytes (the ini's
  initial `visit`), the known-base commodity data at 0x66DABC (prices the player has seen; FLSDK `GameData::knownBases`,
  `visitFlags` 0x673320). This is the autopilot's exposure model for the knowledge scope `exposed`
  (`controller\docs\AUTOPILOT_UX.md` 2a.2): read it, never keep a list of our own. Before any runtime read: signed
  symbols in `core\flexe.h` and a verification against a save's lines.

- **The HUD's target distance is a surface distance, clamped at 0** (22 Oct 2026, from the speedrun recordings, vanilla
  HUD): run 1 reads "0-M" on the target label and in the contact list while sitting against the cruiser RNC
  Saarbrucken's mid-hull, and a generator reads 0 as it blows (`research\evidence\speedrun\reference\frames\`). Whether
  that surface is the bounding sphere or the real hull is not yet checked in game. The vanilla HUD's left bottom box
  is the thruster / afterburner energy gauge (drains on the afterburner, refills at rest), not the throttle.

## Main menu, load screens, pause menu (verified in game 17 Oct 2026)
- **Front-end buttons** (main menu and its screens) are FlControls with their own mouse handlers; a release does
  `parent->vt[0x5C](0x3004, id, 0)` if enabled (the screen plays the click sound): handler 0x5664A0 (main menu:
  enabled byte +0xE6, id +0xEC) and 0x59A3D0 (character select etc.: enabled +0x498, id +0x4A4).
- **Main menu** = static object 0x67B780 (vt 0x5E27CC, handler vt[0x5C] = 0x566BD0). Unnamed buttons with ids
  0x10 New Game, 0x03 Load Game, 0x04 Multiplayer (confirmation dialog 0x4740F0), 0x0A Options, 0x0B Exit
  (0x471F30); ids 0x10/0x03/0x0A are acted on after a short transition (+0x568 id, +0x56C pending, then +0x80).
- **Main menu load screen** = character select, static 0x67AE10 (vt 0x5E2A84, handler 0x56C360): buttons
  LoadCharacter (+0xC8, id 0x3003), DeleteCharacter (+0xCC), MainMenu; save list = list control at +0xD8 (vt
  0x5E5CB4, also the `CharSelDesc` control): +0x8A4 rows, +0x8AC selected (-1 none), +0x8C row data (CHARACTER_ID =
  the save's file name, e.g. `SaveC702.fl`; rows in the screen's sort order), +0x48C row objects (vt[0x10] = item
  id), +0x8A2 enabled. The list's click (mouse handler 0x5A2710): selection = hovered row (+0x7C), then
  `parent->vt[0x5C](0xF001 click / 0xF002 double-click, item id, row)`; a selection enables LoadCharacter.
- **In-game load/save screen** `NN_LoadSave` (created by 0x488970 `cdecl(mode)`: mode 1 load, 2 save; name
  0x5D1D38): save list vector at +0x344/+0x348 (36-byte entries, entry +0 -> CHARACTER_ID); load-by-index 0x4880F0
  `thiscall(index)` = what its Load button (`LoadSave_LoadGame`, +0x5B0, enabled +0x5D5, selected row +0xE88) and
  a double-click do: LoadGame, then 0x4A7690, closes the screen, 0x4120D0(1).
- **Pause menu** `NN_Pause` (name 0x5D04D4): opened by 0x4A75C0 `cdecl()` = the Escape key's path (its single
  caller is the key dispatcher; refuses when [0x67DCC8] is set or other gates hold). Buttons `PauseButtonBox0..4` at
  +0x338+4i with action codes at +0x360+4i (per context): 1 Resume, 2 Save (docked only), 3 Load, 4 Options,
  5 Quit; action 0x4A7210 `thiscall(code)` (code 3 -> 0x488970(1), code 2 -> 0x488970(2)).
- Death and mission failure show the same dialog `NeuroNetMissionMessage` (0x4BB6xx..): `LoadAutosaveButton`,
  Main Menu, `LoadSavegameButton` (text buttons, native click). Verified 17 Oct 2026: self-destruct (death) and King
  destroyed in M01 ("Mission Failed. King has died."), then Load-FLSave from the dialog.
- Destroying a space object the game's way: server.dll `pub::SpaceObj::Destroy(u32 id, DestroyType)` (export
  `?Destroy@SpaceObj@pub@@YAHIW4DestroyType@@@Z`, Fuse = 1 explodes as if killed). Player ship id at 0x673378.
  IObjInspect `get_id()` is vtable **slot 8** (FLSDK's `// 32` comments are byte offsets: get_target 116 = slot 29).
- Mission offers (RTC CharacterEncounter, e.g. M01's offer in the Equipment room, autoplay): decision window
  `OfferDialogAcceptButton` / `OfferDialogDeclineButton` / `OfferDialogCloseButton` (the always-present BtnAccept /
  BtnDecline of `RequestDisplay` are other windows, state 0x0A when hidden).
- **Control state byte +0x6C**: bit 0 = shown (0x0B shown, 0x0A / 0x08 hidden). Room scenes (story / bar RTCs) hide
  every room button (0x08) while the NavBar itself stays shown; during an RTC that follows a room entry the NavBar
  can instead stay busy (`navbar+32C`) for the whole scene.
- `0x41A3E0` = IsPlayerInCutscene (Console plugin source; float [0x6166B0] > [0x5C7474]); the Neural Net voice
  queue waits on it. **Verified 21 Oct 2026** (`research\tools\cutscene_float_probe.ps1`): [0x6166B0] is the cinematic
  letterbox, 0..1 - 0 in a plain room, ramping to 1 within 0.6 s at a room cutscene (M01 Juni bar scene, M04 Benford's
  Van Pelt scene) and during room entry transitions, back to 0 within 0.6 s of a USER_CANCEL skip. Van Pelt's scene
  keeps the room buttons shown (and at times no NavBar), so only this tells it: `exe::IsPlayerInCutscene`, room.cpp
  NavBusy "scene" first; the harness skips it like the others (M04: Benford docked 2.5 s instead of ~200 s).
- **Escape** is the game command `USER_CANCEL` (keymap key 27; key dispatcher 0x5B2440 code 0x14 -> pause menu
  0x4A75C0 when nothing else takes it). Sent through the command system (FLController `tap USER_CANCEL`) it skips
  base animations like the key does (verified 17 Oct 2026): a room's entry transition (NavBar busy, `navbar+32C`:
  Bar ready 9.8 s -> 1.8 s) and the planet launch sequence (mode docked, NavBar gone: launch 14 s -> 3.5 s; the
  short launch cutscene after it is not shortened), and **room story scenes** (M01 Juni bar scene: skipped after 2
  of ~21 lines, room ready 0.2 s later; the mission continues normally: offer in Equipment). With nothing running it
  opens the pause menu, so send it only while one of those states holds. Skipping story cinematics with Esc is an
  **HD Edition feature** (CHANGELOG v0.7: "Skip story cinematics (excluding space cutscenes) with the Esc key, like
  in the Freelancer Beta version"; github.com/FLHDE/freelancer-hd-edition): in-space mission cutscenes
  (Act_CallThorn, mode `cutscene`) are not skippable by design (M01 intro: flight 10.9 s after a cancel = played
  out). Re-tested 17 Oct 2026 at the user's request (they remembered skipping one): in the M01 intro on the story
  camera 0x6792F8 (about 50 s, one camera throughout, Donau scenes included) none of these skipped it or opened the
  pause menu: `USER_CANCEL` (not handled), Escape through the key handler 0x577850, a posted WM_KEYDOWN/UP Escape,
  and a real OS key press (SendKeys) with the game in the foreground. Landing / docking: the docking cutscene is skipped by it ("Targets, contacts, docking"; HDE v0.5 multiplied the
  maximum docking initiation distance by 5; HDE after 0.7.1 removes the limit, Common.dll 0x0638AC: `HDE_UPSTREAM.md`). (`skip_transition_scripts`, a freelancer.ini key read at 0x5B58B0, is the
  player-facing way to drop room transitions; not used for tests.)
- Load from the main menu: the menu's own screen transition ~2.5 s, then < 1 s to space.
- Chat input: opening it (USER_CHAT) shows a control named `CTID` within a frame; it closes on Enter.
- **Pause menu in space** (verified 25 Oct 2026, 4K): four slots, `PauseButtonBox0..3` = codes 1 Resume, 3 Load, 4 Options,
  5 Quit (Save only when docked), centres x 1920, y 721 / 990 / 1260 / 1529, half 490 x 78 (`uiitems`). Quit asks in
  `NN_Confirm` with `Confirm_delBtnYes` / `Confirm_delBtnNo` (text buttons, native click).

- **Save screen, chat field, load lists on the pad (verified in game 25 Oct 2026, HDE 0.7.1, 4K and 720p; code
  `controller\src\impl\padmenus.cpp`, `newgame.cpp`, `osk.cpp`):** the in-game `NN_LoadSave` in save mode (pause code 2)
  has `LoadSave_SaveGame`, `LoadSave_Delete`, `LoadSave_MainMenu`, `LoadSave_ResumeGame`, `LoadSave_Close` (the X),
  `LoadSave_DescripButton` (the description field's frame), the unnamed game list (vt `VtGameList`) + `LoadSave_Scroll`,
  and the description edit **`LoadSave_Edit2`** (vtable 0x5E5BD4, a text-edit class: its key handler object sits at +0x498
  and is the keyboard focus 0x67C3AC while the screen is up; the text = entries of 12 bytes (char + flags) in a vector at
  +0x4C8 / +0x4CC / +0x4D0, the caret +0x49C, the maximum length +0x510 (30 here), case mode +0x50C (1 upper, 2 lower), an
  allowed-character set at +0x514 when +0x52A is set, IME at +0x52C; vt[0xA8] = TextSetProperty). Its char handler
  vt[0xC8] 0x57CEB0 takes `USER_CHAR` (WM_CHAR through exe::WinKey 0x577850, iswcntrl refused, the limit enforced), its
  key handler vt[0xC4] 0x57C9B0 takes `USER_KEY`: Backspace / Delete erase, Home / End / arrows move the caret, Enter
  notifies the owner with vt[0x5C](0xF003, edit, 0), Escape 0xF004. **Typing through the game's own message handler
  works out of focus** (fl::SendChar / debug `type`): FLvibed's on-screen keyboard and the description pre-fill use it;
  the sources compile as ANSI, so a non-ASCII literal (the middle dot) arrives as two characters. The load mode has
  `LoadSave_LoadGame` instead of SaveGame. **The chat field `CTID` (vt 0x5CFEC4) is itself the edit** (key focus = CTID +
  0x498) and the mode stays `space` while it is open; Enter sends and closes it.
- **List rows (both the front-end list vt 0x5E5CB4 and the game list vt 0x5E5F9C):** +0x7C = the hovered row (the data
  index the click handler selects; read it instead of computing rows from the cursor - `ui::ListHoveredRow`), +0x8A4 the
  row count, +0x8A8 the visible rows (20), +0x8AC the selected row (front; the in-game screen keeps its selection at
  NN_LoadSave +0xE88), the scroll bar's position +0x3C8 = the first visible row. The drawn row pitch is **0.029 H on
  both lists** at 4K; the first row's text centre lies 0.2486 H above the screen's middle on the front list, 0.2246 H in
  game. **The hit rows** (measured 25 Oct 2026 by sweeping the cursor down each list and reading +0x7C, 4K): the front
  list's row 0 spans 0.2639 .. 0.2343 H above the middle (centre 0.2491 = the text's), the in-game list's 0.2593 ..
  0.2296 (centre 0.2447, 0.7 row above its text); both pitch 0.0293 H (`ui::ListRows`; the 0.0318 H it had for the
  front list drifted a row after a dozen rows, the stepping re-anchors on +0x7C now).
- **The pending load (verified in game 25 Oct 2026, `menu::PendingLoad`, debug `pendingload`):** the CHARACTER_ID at
  **0x67EA78** (`exe::PendingLoadId`, a char buffer: "SaveC702.fl") is the save the game is about to load: 0x5B2C70
  `cdecl(const CHARACTER_ID*, bool)` copies it there (Common.dll CHARACTER_ID::operator=) and requests game state 7
  ([0x67ECC4]), the loading state machine 0x5B2A30 (case 1) loads it through 0x5AB1F0 (an FDUMP "..." and [0x67ECBD]
  when it fails). Callers: the in-game load screen's load-by-index (0x488144, i.e. `LoadSave_LoadGame` / `loadsave`)
  and the death dialog (0x4BC8D5). It is written the moment the load is requested and **never cleared afterwards** -
  not by the load, not at the main menu (0x5B0810, which zeroes its first byte, has no direct caller) - and the main
  menu's character select does not write it (its load takes another path), so only a CHANGE of the value names a load
  (newgame.cpp); a stale value is the previous in-game load.
- **Clicks out of focus:** the virtual mouse's buttons never reach the game while it is not the foreground window (its
  DirectInput mouse is read only in the foreground: dimouse stateCalls = 0), so background tests of pad flows must act
  natively (uiclick::Click, the lists' selection fields, menu::Load); the cursor position still moves.
- **The main menu's buttons slide in** from the left over ~0.3 s after the menu appears (their rect x is negative
  meanwhile): a first-focus snap must wait until the button is on screen. The front buttons never report as the
  hovered control (0x67DCD8 stays 0): find them by rectangle.
- **The death / mission-failed dialog** after `selfdestruct`: `LoadAutosaveButton`, `LoadSavegameButton` (MMOk text
  buttons) appear ~2 s after the death camera; the pause menu's quit asks in `NN_Confirm` (`Confirm_delBtnYes` / `No`),
  the front-end DELETE asks in the same `NN_Confirm`.

## Options screen `NN_Preferences` (verified in game 25 Oct 2026, HDE 0.7.1, 4K 16:9; screenshots `research\experience\ui_2026-10-25\`)
- One class for both entries (the main menu's Options and the pause menu's code 4). Page buttons are icon buttons
  (vt 0x5E15F4) named `interfaceButton` (GENERAL), `keymapButton` (CONTROLS), `VideoButton` (PERFORMANCE),
  `AudioButton` (AUDIO), `CreditsButton` (main menu only), `MainMenuButton` (MAIN MENU / in game RETURN TO GAME); the
  selected page's caption is yellow. **Main menu**: centres x 996, y 492 + 238 i (i = 0..4), MainMenuButton y 1920, half
  364 x 73: **one free slot at y 1682**. **In game**: x 1080, y 506 + 216 i (i = 0..3), MainMenuButton y 1800: **two free
  slots (1369, 1585)**; a close X `PreferencesCloseButton` (3124, 433). Page title centred at x ~2340 (y 346 / 432 in
  game). Rows: label x 1619, pitch 98 px; `opt_toggle N` (vt 0x5E1764 ToggleButtonControl, centre x 3082, half 42, with
  ENABLED / DISABLED text ending x ~3010), `opt_scroll N` (vt 0x5E55AC ScrollControl, centre x 2781, half 351 x 27, a
  value caption to its left: 0.30 / Average / 120), the resolution picker (a box x 2458..3140 with up / down arrows);
  `opt_set_defaults_btn` (1878, 1906) and `opt_cancel_btn` (2763, 1906) (MMOk text buttons vt 0x5E614C). The Controls
  page: three sub-tab buttons across the top (SHIP CONTROLS 1655..2029 x 503..586, USER INTERFACE, MULTIPLAYER) over a
  PRIMARY / SECONDARY key table. Rollover help: one small Arial line at the top right of the screen ("Activates cruise
  engines."). Semantics: no OK / Apply - values apply as they change; CANCEL reverts; DEFAULT resets the page.
- **The maneuver bar is built in code from `hud.ini [Maneuvers]`** (read 25 Oct 2026 in the disassembly, 0x4D7B50..):
  the frame = the static-control factory 0x5A0DB0 with name "ManeuverFrame" (0x5CB68C) and its mesh (0x5D81E0); then
  one icon button per record through the button factory **0x5A0C90**, named `ManeuverButton%d` (0x5D8430); the records
  are 0x148 bytes each at HUD_Maneuvers +0x380 (+0x138 the maneuver op: 2 Goto, 7 Dock, 10 Formation, 19 Free Flight;
  the lit / grey meshes), the button pointers at +0x36C + 4 i - **four slots, fixed**. The per-frame layout
  `exe::ManeuverLayout` 0x4D88F0 walks a constant table {0, 2, 1, 3}, sets each button's state from
  `IBehaviorManager::external_player_evaluate` and the current op (+0xC0), places the shown buttons at x = base -
  count / 2 x pitch + i x pitch (pitch = the float at 0x5D845C; vt[0xA8] msg 6 = set position, msg 0xC = lit, 0x33 =
  colour 0x679B94), then the tab / cruise button (0x4D8BF0). In space at 4K (bar moved top-left by the subtitles):
  `ManeuverButton0..3` centres x 189 + 224 i, y 85, half 78 at depth 0.5; `ManeuverCruiseButton` (189, 248) half 39.
  A fifth button is therefore not a data change: FLvibed creates its own control with the same factory, parents it to
  `HUD_Maneuvers`, places it in its `ManeuverLayout` observer after the game's pass, and routes its click.

### FLvibed's native buttons (built and verified in game 25 Oct 2026, HDE 0.7.1; code `controller\src\host\uibtn.cpp`)
- **The icon button factory 0x5A0C90** (`exe::CreateIconButton`, `ret 3Ch`, `this` = the parent control; the argument
  list from the game's call sites 0x4D7C49 (maneuver buttons), 0x4A9C62 (page buttons) and FLUF's declaration):
  `FlControl* thiscall(parent, const char* name, const char* mesh, const char* mesh2, const char* clickSound, const
  char* action, const Vector* pos, const char* hardpoint, const Vector* mouseHalf, bool noMeshRender, bool actionButton,
  bool noColourChange, bool noZEnable, u32 tipIds, u32 tip2, u32 command)`. It is `CreateControl(name, "ButtonControl",
  parent)` (0x59DB20: the class registry [0x67DCEC], the name copied into +0x18, at most 31 chars) plus the button's
  Init 0x55AC30 with arguments 3..16: the mesh name is copied into +0xD0 and looked up in the mesh table [0x679A48]
  (0x24-byte entries, name at +4; loaded through vt[0x98] when missing), `clickSound` becomes the std::string at +0x360
  (its buffer pointer at +0x364) and the sound id at +0x374 (-1 = none; "ui_select_item" for the HUD and page buttons),
  `action` is parsed by 0x55A490 into the command +0x34C / hint +0xCC, `pos` -> +0xA8, `mouseHalf` -> +0xB4 (the hit
  half extents: the maneuver buttons 0.014 x 0.014 at depth 0.5 = 78 px at 2160p; the page buttons 0.13 x 0.013),
  `tipIds` -> +0xCC, `tip2` -> +0x32C, `command` -> +0x34C (the maneuver buttons: their maneuver's command; the cruise
  button 0x45), the bools into +0x330 / +0x331 bits (actionButton = +0x331 bit 0; the maneuver buttons pass 1, the page
  buttons 0). A null mesh is fine (no mesh at all: the control is hit-testable and invisible). The maneuver records are
  0x148 bytes: the mesh name string at +0x28, the grey mesh at +0xAC, the tip ids at +0x12C, the command at +0x134,
  the op at +0x138. The mesh name is relative to DATA, and the loader resolves `..`: **a mesh outside DATA loads**
  (`..\EXE\FLvibed\autopilot.3db`, verified with a copy of `mnvr_goto.3db` and with FLvibed's own file).
- **HUD_Maneuvers** is a HUD part object whose window (the FlControl named "HUD_Maneuvers", class vtable 0x5D8344 =
  `exe::VtManeuvers`) sits at part + 0x38; `ManeuverLayout` 0x4D88F0 takes the part, the window's own methods take the
  window: vt[9] = the builder 0x4D7B50 (InitElements), vt[0xA8] = `exe::ManeuversMessage` 0x4D8DA0. Window-relative:
  the four buttons +0x334 + 4 i, the records vector +0x348 / +0x34C, the frame-size cache +0x37C, the frame +0x380,
  the tab +0x384, the cruise button +0x38C (part-relative = + 0x38: +0x36C, +0x380, +0x3B4, +0x3B8, +0x3BC, +0x3C4).
  The message handler acts on 0x26 only: it finds the button among the four (+0x334) and runs its record's maneuver
  (0x4D9470); an id it does not know does nothing - so a fifth button's own action message is harmless, and FLvibed's
  detour on the handler takes 0x26 / 0x27 for its control first (the icon button sends 0x27 when its hover bit 0x80 is
  set, else runs vt[0xB4] -> 0x26: "Room buttons ignored" above).
- **The frame** (`ManeuverFrame`, a static image) is sized by `exe::ManeuverFrameSize` 0x4D95B0 `thiscall(part, int
  shown)` (-1 = count the applicable records itself), called by the layout every frame: it switches the mesh to
  `Interface\HUD\HUD_maneuverbox<n>.cmp` (n = 2..9, the table at 0x612654, 9 for more) when the count differs from the
  cache (+0x37C), through msg 4 (set mesh by name) and re-reads the mesh box into +0x3D0. The layout's x pattern for n
  shown buttons: `base(n) - n/2 x pitch + i x pitch` with base = the (HudShift-relocated) float of `ManeuverBarXRef`
  (0.02) for even n, 0.0 for odd, pitch = `exe::ManeuverPitch` 0.04 (224 px at 2160p: buttons and frame both at depth
  0.5); adding a button on the right therefore shifts a centred group left by half a pitch, so FLvibed instead keeps
  the game's four where they are, asks the sizer for n + 1 (a detour adds one when its button is shown) and moves the
  frame right by half a pitch (its position through msg 6; idempotent over HudShift's / the subtitles' rewrites).
  The tab (`ManeuverTab`, 0x4D8BF0) follows the current maneuver's button (its position read with msg 7, x doubled for
  depth 1) and the cruise button hangs under the tab (x - 0.032).
- **The control state byte +0x6C:** bit 1 (0x02) is what the UI pass tests - 0x59E300 draws every root with bit 1 set
  through its vt[0x34], and a window's draw skips children without it; bit 0 alone is the "listed / hit" bit (a text with
  bit 0 cleared and bit 1 set still draws: the Options page's labels stayed on screen until both were cleared). The
  game's own hide (`and 0xFC`) clears both; show = `or 3`. Bit 3 (0x08) = enabled (hit-testable).
- **NN_Preferences page switch:** vt[9] = `exe::PrefsInitElements` 0x4A9790 builds the screen; the page buttons sit at
  +0x34C keymapButton (page 0, CONTROLS), +0x350 interfaceButton (page 1, GENERAL), +0x354 VideoButton (2),
  +0x358 AudioButton (3), +0x35C CreditsButton (4, main menu only), +0x360 MainMenuButton, +0x348 PreferencesCloseButton
  (in game only); +0x32D = 1 in game (0 in the main menu); the title text ("Screen Title") +0x344; the captions are
  TextControls (`InterfaceText` +0x368, `keymapText`, `VideoText`, `AudioText`, `CreditsText`, `mainMenuText`); the
  current page +0x7F4, the page being switched to +0x7F8. The page buttons are made with mesh
  `Interface\NeuroNet\Inventory\FRONT_button.cmp`, x -0.30 (in game) / -0.33 (main menu), the first at y 0.205 / 0.21,
  pitch 0.077 / 0.085, mouse half 0.13 x 0.013, then 0x55BFC0(1) sets +0x335 = 1 (that is all it does). **+0x331 bit 1
  (0x02) set = the button takes the mouse:** its mouse handler 0x55B950 starts `test [ebx+331h],2 / je` (skips
  everything); the factory sets it (0x0E), the current page's button has it clear (the selected look, not clickable).
  Hover works without it (0x2C enter, 0x25 each hovered frame, 0x2D leave to the owner's vt[0xA8]); the click does not
  (verified 26 Oct 2026: FLvibed had it backwards and its FLVIBED page button took no mouse click on any install). The handler vt[0xA8] =
  `exe::PrefsMessage` 0x4AC0A0 (`u32 thiscall(msg, FlControl* id, int)`): in the normal state msg 0x26 compares `id`
  with the page buttons (0x4AD2A1..), sets +0x7F8, hides the old page through the per-page function (0x4AEAB0 /
  0x4AEBB0 / 0x4AECA0 / 0x4AED70; credits inline) and shows the new one through 0x4AEE30 (sounds 2 and 5, the per-page
  show 0x4AE4B0.. which also sets +0x7F4 and the title via 0x4347E0(ids) + msg 0x1D); msg 0x25 / 0x46 = hover enter /
  leave for the rollover help (0x4B2130), 0x29 / 0x3E the sliders and toggles, 0x4F the gamma. `exe::PrefsShowPage`
  0x4ABDF0 `thiscall(bool hide)` is the game's own hide (1) / show (0) of the current page's interactive controls (used
  around the key-capture overlay); it leaves the labels (`opt_text N`, `opt_titleText N`, `opt_toggleText N`,
  `opt_smallText N`) and `video_mode_*` alone, and the game re-shows the page's texts each frame - FLvibed's page
  switch therefore also clears bits 0|1 of every visible child of NN_Preferences except the furniture
  (`Options_BlackPlane`, `OptionsBox`, the page buttons and their captions, the close X) and re-clears them at the
  start of every UI pass; any other 0x26 (a page button, RETURN TO GAME / MAIN MENU, the X) restores first. The page
  content controls are named `opt_toggle N` / `opt_toggleText N`, `opt_scroll N` / `opt_smallText N`, `opt_text N`,
  `opt_titleText N`, `opt_set_defaults_btn` / `_txt`, `opt_cancel_btn` / `_txt`, `video_mode_cycle` / `_box` / `_box2`
  / `_text`. The free slot: after CREDITS in the main menu (y 1682 at 2160p), after AUDIO in game.
- **TextControl** (class vtable 0x5E4F24 `exe::VtTitleText`, the factory 0x5A0E30 `exe::CreateTextControl`, `ret 2Ch`:
  `thiscall(parent, name, fontNickname, const Vector* pos, int, float, float, float, u32 ids, int, int, int)` - the page
  captions pass ("ButtonText", pos, 0, 0.258, 0.015, 1.25, ids, 1, 0, 1), the pause captions (0, 0.27, 0.015, 1.5, 0, 1,
  0, 0)): vt[0xA8] = `exe::TextCtlSetProperty` 0x58B360, msg 0x1D = raw text (`const wchar_t*`), 0x1E = a string id,
  6 = the position; the caption colour at +0x348 (the globals 0x679B94 `UiColourSelected` = the selected page's yellow,
  0x679BB0 `UiColourNormal`). So an added button can carry a caption without a string id.
- **NN_Pause** (vtable 0x5D462C): vt[9] = `exe::PauseInitElements` 0x4A6BA0 builds `Pause_BlackPlane`, `PauseFrame`
  (`Interface\NeuroNet\Options\escapewindow2.cmp`), `PauseTitle`, then n boxes `PauseButtonBox%d` (the text / MMOk
  button class 0x5E614C through the factory 0x5A0D00 `exe::CreateWireControl`, `ret 24h`: `thiscall(parent, name,
  sound, const Vector* pos, const Vector* dims, hardpoint, int, u32 hint, int, int)` with no sound, dims (0.175, 0.014,
  0) -> +0xB4, the last box flagged +0x35D bit 0) at +0x338 + 4 i and captions `PauseButtonText%d` at +0x34C + 4 i
  (+0.005 above the box), action codes +0x360 + 4 i. The layout depends on the count: 4 boxes at y 0.1284 - 0.0963 i,
  5 (docked, with Save) at 0.149 - 0.0825 i - one more box moves the first up by 0.0206 and tightens the pitch by
  0.0138, and the same step once more (0.1696 - 0.0687 i) seats six inside the frame (a sixth simply appended fell
  below it). The handler vt[0xA8] = `exe::PauseMessage` 0x4A7130: 0x26 finds the box among the five (+0x338) and runs
  its code through 0x4A7210 (an unknown box does nothing); 0x42 = set position; 0x3006 = the Escape path (resume).
  Captions: msg 0x1E with ids 0x71D.. on the texts, msg 3 with 0x33A.. on the boxes (the rollover help).
- **The text / MMOk button class** (vtable 0x5E614C: `PauseButtonBox%d`, `opt_set_defaults_btn`, `opt_cancel_btn`):
  vt[0xA8] = `exe::TextButtonSetProperty` 0x5A4690 (id 0x2E its own, 6 = the position through ControlSetProperty).
- **Raw keys:** `exe::WinKey` 0x577850 `bool cdecl(UINT msg, WPARAM, LPARAM)` handles WM_KEYDOWN..WM_SYSKEYUP (a jump
  table on msg - 0x100): it stores the vk in 0x67C27C, the repeat flag 0x67C291 (bit 30 of lParam), the down flag
  0x67C290, the modifiers 0x67C280 from GetKeyState, then HandleKey(USER_KEY = 1 or USER_CHAR = 2, category) and, when
  not handled, the bound commands. FLvibed's detour tells the player's keys from its own injected ones (fl::SendKey calls
  the same entry from the logic DLL) by the return address: inside Freelancer.exe's image = the game's window procedure
  = a physical key. **Keyboard focus:** a window takes it with 0x577B40(&window + 0x32C) (NN_Pause at 0x4A701D); a text
  edit (class "UITextEdit", factory 0x5A1900) takes it with its handler at edit + 0x498 (0x57C750); HandleKey hands
  category-1 keys only to the handler that equals [0x67C3AC] when one holds the focus. So "a text field has the focus"
  = [0x67C3AC] - 0x498 is a live control.
- **Icon meshes** (`tools\mk3db.py`, verified 25 Oct 2026 with FLvibed's own `autopilot.3db` on the bar): a HUD icon
  `.3db` is a UTF with `VMeshPart\VMeshRef` (60 bytes: headerSize 0x3C, VMeshLibId, startVertex, vertexCount,
  startIndex, indexCount, startMesh, meshCount, bbox max/min xyz, centre, radius), `VMeshWire\VWireData`, `Material
  library\<name>\{Type "DcDtOcOt", Dt_name "<x>.tga", Dt_flags 0x40}` + `Material count`, `Texture library\<x>.tga\MIP0`
  (an uncompressed 32-bit TGA). The game's icons keep their vertices in the shared `DATA\INTERFACE\interface.generic.vms`
  (`VMeshLibrary\interface.generic-102.vms\VMeshData`: 479 meshes of 4 vertices, FVF 0x102); a self-contained 3db
  embeds its own `VMeshLibrary\<lib>.vms\VMeshData` (meshType 1, surfaceType 4, meshCount, indexCount, FVF 0x112 =
  XYZ|NORMAL|TEX1 32-byte vertices, vertexCount; per mesh {materialCrc, startVertex, endVertex, refVertexCount,
  0xCC}; the u16 indices; the vertices), and the VMeshRef's library id is the CRC of that node's name. **The CRC is
  Freelancer's own CRC-32** (DACOM GetCRC32: a table that is not the standard polynomial's, over the lower-cased name,
  init and final xor 0xFFFFFFFF; FLUF's `Utf::Crc32` reproduces it; `mk3db.py crc`): crc("interface.generic-102.vms")
  = 0x1351B6D4 (the goto icon's library id), crc("HUD_MNVR_goto") = 0x1B3E1E7C (its mesh's material id); with the
  standard CRC the mesh loads but nothing draws (no material match). A quad of half size 0.0155 matches the game's
  icons (their bbox +-0.016). The button draws it untinted (white texture = white) until msg 0x33 (the colour's address,
  flag 1 = grey) and msg 0xC (lit) are sent as the layout sends them.

### HUD 2 hotspots (built and verified in game 25 Oct 2026, HDE 0.7.1, instance 6, SaveC705, 4K and 1280x720; code `controller\src\host\uibtn.cpp` "HUD 2's hotspots", HostApi v22)
- **A top-level control of our own.** The icon button factory 0x5A0C90 with `this` = 0 makes a root like the HUD's: it
  passes the parent to `CreateControl` 0x59DB20 (registry by type name at [0x67DCEC] +0xC, then the class's create with
  (parent, 0)), whose vt[0x24] (0x55A6B0 for the icon button) calls `SetParent` 0x59DD00 (null parent: +4 = 0, no child
  link) and adds the control to the flat list (0x59DBF0). Roots are updated each frame by the UI update 0x59E080 (every
  list entry with +4 == 0: vt[0x2C] when bit 0 is set, else vt[0x30]) and drawn by the UI pass when bit 1 is set.
- **The hit test 0x59DD80** takes a control only with bits 0, 1 and 3 of +0x6C set **and bit 1 on every ancestor**
  (the +4 chain); the winner is the highest vt[0x6C] priority, else the deepest (ancestor count), ties by the frame z
  +0x68. So the children of a hidden HUD root can never be hit, and a root with bit 3 clear and a zero box is never hit
  itself but lets its children be. The icon button's box test (vt[0x10] 0x5A0340 -> 0x5A1130) uses frame +0x60 + pos
  +0xA8 (+ +0xC0) and the half extents +0xB4 (all three zero = never hit); a hit sends msg 0x25 to the parent.
- **The click** (the icon button's mouse handler 0x55B950): a press (event {-1, 2}; {-1, 3} = a double click, sets
  +0x330 bit 0x80) sends 0x28 to the parent and captures; the release ({-1, 1}) re-runs the hit test with the cursor
  ray, plays the click sound (+0x374, or the std::string at +0x364), then sends **0x27** to the parent after a double
  click or runs vt[0xB4] (`exe::IconButtonActivate` 0x55A660: the command +0x34C through 0x5763B0, else **0x26** (id =
  the button, 0) to the parent's vt[0xA8]). With no parent both are dropped, so the hotspots sit in a box that owns
  them; the box's vt[0xA8] is ours through a private copy of the icon button's vtable (46 slots, 0x5E15F4..0x5E16A8,
  vt[-1] copied too; slot 0 the scalar deleting destructor, slot 42 = 0xA8 `IconButtonSetProperty` 0x55BC30, slot 45 =
  0xB4 Activate). A control under a modal window (0x59E6A0: some control's vt[8]() object has +0x2D0 set) takes clicks
  only when its parent's vt[8]() object has +0x2D0 set - the HUD's own rule, so the hotspots ignore clicks under a
  NeuroNet window as the HUD does. Click-vs-fire: while a control is hovered (0x67DCD8, 0x59DAA0) the steering (0x4C7D62)
  and the HUD's other mouse users (0x4EC6B9, 0x4ED91F) stand down and the mouse button goes to the control - the same
  for these buttons as for the game's (not verified with a physical click: a background instance reads no mouse).
- **A mesh-less icon button must have +0x338 cleared.** The Init 0x55AC30 sets +0x338 (the mesh table entry, 0x24-byte
  entries at [0x679A48]) only inside `if (mesh)` (0x55AC69 / 0x55ACEC); the ctor 0x55A560 leaves it, so a button made
  without a mesh keeps the allocation's garbage there, and its draw (0x55B3EF -> 0x559AA0 reads [+0x338]+0x20) faults
  on the first UI pass (seen: AV at 0x559ABB). 0 = no entry, which the draw handles. (The bar button always has a mesh.)
- `exe::MouseWheel` 0x616848: the wheel's accumulated position in notches (the poll adds each read's lZ / 120;
  0x41FC6A / 0x41FC9A); a frame's turn = the change since the last read (0x41F282 / 0x41F296 do the same).
- **The nav map lives under NN_MainButton** (the NeuroNet box root, `NavMap*` controls with root NN_MainButton in
  `uiitems`): USER_NAV_MAP opens it (mode space-ui) with the root shown; with the root hidden by HUD 2's switch the same
  command leaves the mode `space` and nothing visible (25 Oct 2026, instance 6).

## Direct3D 8
- Device reachable via RP8.dll IAT → `IDirect3D8` vt 15 `CreateDevice` → device vt 14 `Reset`.
- IDirect3DDevice8 vtable indices (from memory of d3d8.h — verify before relying on them):
  3 TestCooperativeLevel, 14 Reset, 15 Present, 20 CreateTexture(w,h,levels,usage,fmt,pool,out),
  34 BeginScene, 35 EndScene, 40 SetViewport, 41 GetViewport, 50 SetRenderState,
  52 BeginStateBlock, 53 EndStateBlock, 54 ApplyStateBlock(DWORD), 55 CaptureStateBlock(DWORD),
  56 DeleteStateBlock(DWORD), 57 CreateStateBlock(type, DWORD*), 61 SetTexture(stage, tex),
  63 SetTextureStageState, 72 DrawPrimitiveUP, 76 SetVertexShader(FVF), 88 SetPixelShader.
  IDirect3DTexture8: 16 LockRect, 17 UnlockRect. The Windows SDK has no d3d8.h.
- Reference overlay implementation: `repos\FLUF\FLUF\Source\Client\Ui\ImGui\ImGuiD3D8.cpp`.

## Missions, strings, saves
- Mission scripts run in DLLS\bin\Content.dll and talk to the player through server.dll's pub:: API.
  IAT-hooking Content.dll's imports gives exact events: `pub::Player::SetMissionObjectives(const u32& client,
  const u32&, const MissionObjective*, u32 count, const FmtStr& title, u8, const FmtStr& text)`,
  `SetMissionObjectiveState`, `PopUpDialog(u32, const FmtStr& caption, const FmtStr& msg, u32 buttons)`,
  `DisplayMissionMessage(const u32&, const FmtStr&, MissionMessageType {0 Failure, 1 dialog, 2 objective
  text, 3 Success}, bool)`, `SendNNMessage`, `pub::SpaceObj::SendComm`. FmtStr +4 = string resource id.
  The mission results the controller host names SUCCESS / FAILURE in its `mission-message` event (type 1 and 3 or a
  "Mission Result" text, type 0 or a "... Failure" text) are counted as HostApi v20 `MissionResult` (a sequence + the
  latest type 1 success / 2 failure): every leg's PASS log ends in `mission-message type=SUCCESS text=1231 "Mission
  Result: Success"`. A plain death (self-destruct outside a mission) sends no mission message, only the death dialog.
- `SendComm`'s line ids are CreateID of the dialogue line's nickname as the mission script writes it (lower-cased
  `dx_m10_0460b_botzler` = 0x899657CC, `dx_m10_0461_vonclaussen` = 0x9B942303 - the M10 log's comm events, verified 23
  Oct 2026): a script's `Cnd_CommComplete = <line>` can be matched to the comms seen (HostApi v16 CommHeardAt).
- A script can set an objective and replace it in the same instant (M04 Mactan, 24 Oct 2026, every run of s12c / s12d:
  "Destroy the Rheinland Assault Wing" 22685 and "Awaiting Mission Objective" 21660 at the same millisecond, the wing
  marked by Act_MarkObj): the HUD shows the placeholder; the marks are what is left of the step. Watch the marks, not
  only the newest objective.
- The mission's own ships are told apart from encounter traffic only by where they spawn: Content.dll spawns both through
  one helper; a mission ship appears exactly at its [MsnShip] / [MsnFormation] position or its Act_SpawnShip /
  Act_SpawnFormation position (formation members as the next spawns within 1 km, in `ship =` order) - the harness's
  Get-FLMissionShips and, since session 12, the logic's mission::MapSpawns (host API v17 SpawnedShips). Mission ships
  with `random_name = true` share no unique pilot name. **Unverified:** whether the game reuses ship space ids after a
  load (the logic's spawn list spans the process; it maps only spawns after the last menu - autopilot `NoteMenu`).
- String ids: dll index = ids >> 16 (0 resources.dll, then freelancer.ini [Resources] DLL order),
  LoadStringW(ids & 0xFFFF).
- Content.dll strings: story states `Mission_01a` ... `Mission_13`, `No_Mission` (free roam).
- Story missions have timers and nags that fail the mission (scripts: DATA\MISSIONS\Mxx\*.ini).
- Save files (HDE: plain text): `[StoryInfo] Mission`, `[MissionState]` clamps, `[TriggerSave]`/`[Msn*]`/
  `[NNObjective]` mission state, `[mPlayer] can_dock/can_tl` (missions revoke docking), `[Player] tstamp`
  (FILETIME hi,lo; load lists sort by it, AUTO SAVE always first; the list is read at game start),
  `description` = hex UTF-16. Saves/settings live in the game's user-data folder: Common.dll
  `?GetUserDataPath@@YA_NQAD@Z` (vanilla `<Documents>\My Games\Freelancer\`; HDE's flplusplus detours it:
  `save_folder_name` under My Games, or `save_in_directory` = `<install>\SAVE` - the test install uses the latter).
  The game's log goes to `%LOCALAPPDATA%\Freelancer\FLSpew.txt` regardless.
- **MissionMessageType, corrected (18 Oct 2026, in game):** 0 = story failure, 1 = success ("Mission Result:
  Success", 1231); a job board mission reports its failure with type 2 (13093 "...You ran from the battle") and its success with
  type 3 (327731 "You have made %F0v1 very pleased."; no pub::Player::AdjustCash call from Content.dll for the
  reward); types 8 and 10 come at a job's accept. The host names a message FAILURE / SUCCESS by its "Mission Result" text when the type is
  not 0 / 1.
- **Objective entries (18 Oct 2026, in game):** `MissionObjective` = 28 bytes: +0 type, +4 the argument chain, +8 the
  objective's text id. Chain node {+0 next, +4 value, +8 u16 kind} = a `FmtStr::Val` (Common.dll's `FmtStr::append_*`
  write the kinds: `S` append_system, `R` append_rep_instance, `O` append_spaceobj_id, ...; disassembly 19 Oct 2026):
  `S` system hash, `R` a **reputation instance** (pub::SpaceObj::GetRep of the object the HUD marks; small for ships and
  overlapping their small space ids - M03 Walker = 8; a job's lane ring and M03's dock ring arrived as space ids:
  resolve by rep, by id only for non-ships, `space::ObjectiveObject`), `N` a navmarker -> {+0 system, +4 ?, +8 x, +0xC y,
  +0x10 z} (the HUD's "Mission Waypoint"; a job sets no waypoint list, only these; an `N` at 0,0,0 is no marker - M01a's
  21875 "Dock with the Trade Lane" carries one beside its object TL146, read as a marker it pointed at the sun: FLvibed
  ignores it since 26 Oct 2026, `events.cpp` NavSet), `F` a number. The chain is freed
  after the call: read it inside the hook. Entry 0 is the current step, the others the plan ahead (a Kill Ships job:
  dock the marked lane -> exit ring -> "Kill all ships near the mission waypoint" at the last N). Host events
  `objective ... target= nav= system=` and `objective-step <i> ...`.
  **The entry's +4 is a FmtStr** ({+0 the Val chain, +4 the string id}: all exe::FmtStrFormat 0x41C970 reads,
  disassembly 25 Oct 2026): the game's formatter turns it into the shown text. Its codes (jump table 0x41D3B4 / index
  0x41D3E8): `%B %F %G %I %L %N %R %S %Z %d %s` (+ digits = the value's index, `v<digit>` a variant) take the chain value
  of that kind and index; any other letter, or a code without a value, is dropped. Vanilla 1.1's objective strings carry
  `%M` ("Destroy the Hostile Ships%M", 21830; HDE's strings do not): dropped, verified in game on both (host v24
  ObjectiveText, HUD 2's headline).
- **The client's copy of the objectives and save loads (25 Oct 2026, in game):** every save load reloads Content.dll
  (the controller host's IAT hooks are re-applied ~0.5-1 s after each load, `events: Content.dll hooks installed`), and
  the mission's objective is restored during the load, before the hooks are back: **a mission save loaded in a running
  game brings no objective event** (only the first load after the start is seen: Content.dll was hooked before it).
  The game keeps its own copy: Freelancer.exe 0x4C4FF0 `const void* cdecl(int index)` (`exe::ClientObjective`; count
  0x4C4FB0) returns entry `index` of the list at 0x672950 (first node +0x14 / +0x18 = a vector of 28-byte entries: +0 the
  FmtStr {+0 the Val chain, +4 the string id}, +0x18 the type, FLSharp: 0xA = a space objective), null past the end -
  what the NeuroNet's Current Information window (0x475A94 -> exe::FmtStrFormat) and the banner show. Read right after
  the hooks' re-install it holds the loaded save's objective (HDE: SaveC705 -> Save17f7 21830, -> Save1e7d 22425, ->
  M09_ck_arch 23635; a free-roam save: empty; vanilla 1.1 the same, "...Ships%M" formatted to "Destroy the Hostile
  Ships"). Host v25 ObjectiveReread takes it over then (event `objective-reread count= obj=`); its 'N' values are not
  read (unverified in the client copy).
- **Marks:** server.dll `pub::Player::MarkObj` forwards to the client interface (rpclocal.dll in single player,
  FLHookSDK IClient slot 75 Send_FLPACKET_SERVER_MARKOBJ); a job never calls it - its marks are the objectives' `R`
  arguments. Host event `mark id= state=` (vtable hook, found by the call's signature in Server.dll). A save load started
  inside a flight reloads rpclocal.dll (its vtable comes back unpatched; the client vtable sits at rpclocal.dll +0x33978
  in HDE 0.7.1) and the load's marks go out before the next frame: the host re-patches from a DLL load notification
  (events.cpp ClientModuleLoaded), so M03's convoy marks arrive after SaveC705 -> SaveS008 too (verified 26 Oct 2026).
- **Job board offers (18 Oct 2026):** Content.dll creates them with `pub::GF::MissionVendorOfferCreate(const
  MissionVendorOfferDescription&)` once per base (kept across save loads) and removes them with
  `MissionVendorOfferDestroy(ulong* key)` (accepted / renewed). Description: +0 base hash, +4 index, +0xC job type (1
  Kill Ships; 3 portrait icon; 6 cargo icon; 5 seen; others unconfirmed), +0x58 reward (credits). The board
  (`MissionVendorSummaryBox0..4`, `MissionVendorAcceptButton`, the Bar hotspot IDS_HOTSPOT_MISSIONVENDOR, behaviour 3)
  lists a base's live offers in key order. Difficulty is only in the text arguments. Host events `offer key= base=
  type= reward=`, `offer-gone key=`; harness Get-FLJobOffers / Start-FLJob / Invoke-FLJob.
- **What a job pays (game data, 18 Oct 2026):** `DATA\MISSIONS\mbases.ini` [MBase] per base: [MVendor] `num_offers`,
  [BaseFaction] `faction`, `weight` (share of the board), `mission_type = DestroyMission, min difficulty, max
  difficulty, weight`; the reward is `DATA\RandomMissions\diff2money.ini` at the offer's difficulty (0 -> 1800,
  0.0821 -> 2200, 0.11238 -> 2655, 0.20513 -> 3219, 0.351 -> 3919, 0.48 -> 4792 ...; rankdiff.ini's FreeTime values
  are points on the same scale). New York: Liberty Navy at West Point (Li01_09, 70 % of offers) and Norfolk Shipyard
  (Li01_07) go to 0.21 (up to 3219); every other lawful board stops at 0.11 (2655); Buffalo / Rochester (Rogues,
  Outcasts, Junkers) go to 0.35-0.48 but are hostile to a lawful player. Observed boards matched (Manhattan
  2000-2550, Pittsburgh 2350-2550). **The story stage sets the window** (adoxa's RE in the Starport KnowledgeBase,
  research\repos\KnowledgeBase docs\guides\random-missions-editing-tutorial.md): single player, rank_diff =
  `rankdiff.ini` for the current free time (MissionNum), f = 1.3689^3; hi = the range's maxDiff, lo = max(minDiff,
  maxDiff / f) when rank_diff x f >= maxDiff, else max(minDiff, rank_diff / f); no offer when rank_diff x f <=
  minDiff; difficulty = hi - cos(rnd x pi/2) x (hi - lo). After M01b (0.112387): Manhattan 0.044-0.112 = 2013-2655
  credits (seen 2000-2550), Navy 0.08-0.205 = 2200-3219. **What a job sends:** one npcranktodiff.ini row per wave
  (NPC level), the ship count from a weighted draw (single player: 2 ships 3, 3 ships 9, 4 ships 6, 5-8 ships 2/1/1/1,
  never 1) whose column matches the difficulty. Harness: Get-FLJobBases ranks boards by this expected pay.
- **Story progression by net worth (Content.dll story object, 18 Oct 2026):** `[StoryInfo] MissionNum` counts story
  states (`mission_xx_loaded`, `_accepted`, `freetime_xx_yy`); at a mission's end the next free time sets
  `delta_worth` = net worth now + a per-free-time amount: FreeTime 01-02 **3500**, 02-03 5200, 03-04 7000, 04-05
  10000, 05-06 16000, 06-07 0, 07-08 36000, 08-09 30000, later 0 (floats written in the story object's constructor,
  Content.dll 0x06EA5E59..). Net worth = a float from one server.dll import (by its use the asset value: ship, equipment, cargo; import name not
  checked) + the player's cash (int). On the story
  object's events 0 and 0x100B, when net worth >= delta_worth (and no other mission runs), MissionNum moves on and the
  next mission loads (Mission_02 at MissionNum 7: objective 22210 "Meet Juni in the Manhattan Bar"). `delta_worth` -1
  = no free time. **Confirmed** by adoxa's StoryLvl plugin source (it patches the same table, v1.1 0x6fced28 at
  Content.dll base 0x6ea0000; same defaults); mods may change the amounts (docs\MOD_COMPATIBILITY.md).
  The imports (Content.dll IAT, 18 Oct 2026): net worth = `pub::Player::GetAssetValue(const u32& client, float&)` +
  `pub::Player::InspectCash(const u32& client, int&)`; the target goes to `pub::Player::SetMoneyNeededToNextRank(u32
  client, int target)` (host event `worth-target <n>`; debug `worth [client]`, single-player client 1).
- **Net worth = asset value + cash, with assets valued below their price (Server.dll GetAssetValue, read 18 Oct
  2026; verified: SaveC701 6750 = 0.8 x 6000 + 0.3 x 2500 + 1200):** the ship's hull good price x 0.8; each cargo /
  equipment entry: if its good's flag at +0x70 is set (cargo, consumables), count x price; otherwise (int)(item
  health x price) x count x 0.3. So **buying equipment lowers net worth by 70 % of its price** (a ship by 20 %, damage
  lowers it further): during free time, buy only after the target is met (the next target is set at the next
  mission's end).
- **Cutscene lengths:** every mission `.thn` scene's `duration`, read with adoxa's Dethorn: docs\missions\CUTSCENES.md.

## Voices and dialogue audio
- **Neural Net voice (verified in game 17 Oct 2026):** voice `NNVoice` (string 0x5D24A8) resolved once at 0x489DE0
  (id cached at 0x671F5C, Voice* at 0x671F70). Lines go through a queue object at 0x671F50 (list head +4, count +8,
  max 4, a msg already queued is skipped): enqueue `0x489E80 cdecl(msg id, 0)`; player 0x489A80 thiscall(entry)
  (entry +0 msg, +8 sound handle): gate 0x41A3E0 (float 0x6166B0 > [0x5C7474] blocks), loads the sound if needed
  (0x553660 / 0x553590), `Voice vt[5]` GetSound, starts it with 0x4285F0, which calls PlayArchetype 0x428680 from
  0x42862E. A line whose sound is not loaded yet can be dropped (first request). 57 call sites queue fixed ids from
  globals filled by CreateID at startup: launch checks (must_have_mounted_*), dealers (mounted,
  loaded_into_cargo_hold, insufficient_cargo_space, not_enough_money, ship_purchase_complete, repair_complete),
  cargo_jettisoned, cancelled, trade lane disrupted, scanning_cargo, incoming missile/torpedo, objective_reached,
  danger / warning / sun corona / atmosphere, equipment destroyed, shield states, repair droids,
  target_out_of_range, waiting_to_dock, nnv_dock_too_far, click_on_characters_and_buttons. Room changes queue none.

Data facts verified by parsing the HDE files. The Freelancer.exe addresses are from static disassembly
only (not traced in game yet).
- **No dialogue text ships with the game**: the resource DLLs (string tables + RT_HTML infocards) hold
  news, rumors (infocards in MiscText), offers, objectives and logs, but no transcripts of voiced lines.
  Mission scripts and .thn files only reference line ids.
- `CreateID` (Common.dll, nickname hash): lowercase the string; `h = (h >> 8) ^ T[(u8)h ^ c]` with the
  CRC table of polynomial `0xA001 << 14`; then byte-swap h and return `(h >> 2) | 0x80000000`.
- **Voice banks**: `freelancer.ini [Data] voices = ...` → `DATA\AUDIO\voices_*.ini`: `[Voice] nickname`
  (or `extend` = add lines to an existing voice), then `[Sound] msg = <line id>`, `attenuation`, optional
  `duration`, `priority`. Audio file = `DATA\AUDIO\<voice nickname>.utf`. UTF root children are named
  `0x%08X` = CreateID(msg), and each leaf is a complete RIFF WAVE (format 0x55 = MP3, mono, 11025/22050 Hz).
  All 24,002 msg entries of HDE 0.7.1 resolve. 114 banks, 26,769 clips, ~758 min.
- UTF file: "UTF " magic, +8 node block offset, +24 string block offset, +36 data block offset. Node
  (44 bytes): +0 next sibling, +4 name offset, +8 flags (0x10 dir, 0x80 leaf), +16 first child (dir) or
  data offset (leaf), +24 data size.
- **Story sounds** (cutscenes, room scenes): `DATA\AUDIO\story_sounds.ini` `[Sound] nickname, type = voice,
  file = Audio\Dialogue\<speaker>\<nickname>.wav` (MP3 in WAV, ~52 min). A `.thn` SOUND entity has
  `template_name = <nickname>`, and an event `{ t, START_SOUND, { "<entity>" }, { duration = d } }`.
- Line ids name the speaker: `DX_M01_0100_KING` (mission comm), `dx_s006x_0002_King` (story scene s006x),
  `GCS_ML_*` (base speech fragments), `gcs_*` / `rms_*` (space chatter fragments; `+`/`-` suffix =
  intonation variant). Mission `[Dialog] Line = <from>, <to>, <msg>`; `Act_EtherComm = <voice>,
  <name ids>, <to>, <msg>, -1, <head>, <body>[, <comm costume>]`.
- Space and base chatter is built from fragments (faction, formation designation, number, base, system,
  commodity).
- Fragment ↔ game object: `msg_id_prefix = <prefix>` in system INIs (systems: `gcs_refer_system_Li01`),
  mbases.ini (bases: `gcs_refer_base_Li01_01_Base`, name = universe.ini [Base] strid_name of the same
  nickname), faction_prop.ini (`gcs_refer_faction_li_lsf`, names = initialworld.ini [Group]
  ids_name / ids_short_name of `affiliation`), goods.ini (`gcs_gen_commodity_boron`, name = the
  equipment entry of the same nickname) and shiparch.ini. Several goods reuse another good's prefix
  (commodity_vips and commodity_prisoners both use `gcs_gen_commodity_boron`). Msg ids append
  suffixes: `-`/`+`, `_base-`, `_short`.
- Formation designations ("Alpha 3-7"): `gcs_refer_formationdesig_N` speaks string 197808 + N − 1
  (Alpha, Beta, …, Omega, Red, …, Yellow, Matsu, …, Yanagi; 29 in all, `permutation_count =
  gcs_refer_formationdesig, 29` in voice_properties.ini). faction_prop.ini `formation_desig = first,
  last` string ids picks each faction's range. Ids sometimes zero-padded (`_07`).
- GCS variant counts: voice_properties.ini `permutation_count = <msg base>, N`.
- Bases name their "run by" fragment: universe.ini [Base] `BGCS_base_run_by = W02bF35` (base speech
  `GCS_ML_W02bF35` = "Run by …").
- Some lines exist twice: e.g. `DX_M01_1525_KING` is a bank entry (king.utf) and a story sound in
  DIALOGUE\MISSION13.
- Scene mixes: gf_sounds.ini `mix_s002xe` … `mix_s072d` (19 files, up to 3.4 min) carry whole room
  scenes as one track.

| Address (Freelancer.exe) | Meaning (static) |
|---|---|
| 0x430B00 | load a voices INI (called from the [Data] loader at 0x5B0DA3, key string "voices" 0x5E6A68) |
| 0x667E18 | voice registry (std::map voice id → Voice; head node ptr 0x667E1C, size 0x667E28) |
| 0x430AB0 | `Voice* __cdecl FindVoice(u32 voiceId)` (null if unknown). Callers: 0x457200, 0x489DF1, 0x4CAE5B, 0x4CDF56, 0x4CE015, 0x4CE20A, 0x4CE560, 0x553597, 0x554D64 |
| Voice vt[5] (+0x14) | `Sound* GetSound(u32 msgId)` (null if the voice has no such line); the returned object's vt[14] (+0x38) gives an int used as a maximum (priority?) |
| 0x4CDF40 | iterates a comm record: voice id +0x3C, msg ids from +0x44, count +0x84 (likely the client-side comm = fragment list) |
| 0x4571C0 | GCS variant pick: the msg name's trailing `_N` → random variant `rand()*N >> 15`, then CreateID + FindVoice + GetSound |

**Dialogue playback, verified in game (FLSubtitles, 17 Oct 2026, HDE 0.7.1):**

| Address (Freelancer.exe) | Meaning |
|---|---|
| 0x4CE000 | comm received by the client comm player: `thiscall(player, ?, CommRecord*)`, ret 8 (the record is the **2nd** argument). Record: +0 sender obj id, +4 receiver obj id, +0x3C voice id, +0x44 msg ids (u32[16]), +0x84 count. Called when the comm starts playing (the intro's Freeport 7 lines arrived 5–12 s apart, matching the clips); covers mission comms, ATC and NPC chatter (one call per assembled message). Prologue `83 EC 34 53 55` (5) |
| comm player state | (18 Oct 2026, verified in game) The comm player ("RadioReceiver", factory 0x4CD2E0: 0x5B8 bytes, ctor 0x4CD350, vtable 0x5D764C; update method 0x4CD9D0 = vt[11]) is `this` of 0x4CE000 (the controller host captures it: HostApi v9 CommPlayer). **+0x460 byte = a comm is playing** (set when one starts, cleared by the update when 0x4CBC40 reports the current one finished; stays 1 across back-to-back lines of a conversation); +0x45C current comm's priority; +0x454 the pending-comm queue (0x4CDE50 pops the next). Debug `commplaying`, `commstate` |
| 0x428680 | `u8 __cdecl PlayArchetype(SoundArchetype*, flags*, handle* out, int)`: every sound archetype started, at the moment it starts; nonzero = started. The archetype's nickname id (CreateID) is at **+4**. Callers: 0x4CB18D first piece of a comm, 0x4CC020 each following piece (real per-piece start times), 0x4575A5 THN SOUND entity start (cutscenes, room and bar scenes, scene mixes). Prologue `53 8B 5C 24 08` (5) |
| 0x42AE40 | `SoundArchetype* __cdecl FindArchetype(const ID_String* id)` (story_sounds.ini / sounds INIs, loader 0x42B030, archetype ctor 0x42B7B0 with base vtable 0x5CA604) |
| 0x4570A0 | THN SOUND entity: resolves `$BGCS_Base_Name` / `$BGCS_Base_Run_By` and `VoiceProfile_*` lines via FindVoice + Voice vt[5] GetSound (return 0x457216) just before starting them; 0x457081 = `CreateID(template_name)` + FindArchetype for story sounds and `mix_*` |
| Voice vt[5] callers | 0x5535B3 / 0x431546 preload a mission's lines when it loads; 0x554D7B releases them; 0x4CDF7D, 0x4CE231, 0x4CB020, 0x4CE58F = comm path; 0x457216 = room scene |
| 0x431012 | voices INI loader: CreateID of every msg at startup |
| 0x46A150 | `void __cdecl ChatPrint(int, const void* rdl, u32 size)`: adds Freelancer rich text (RDL nodes {u32 type, u32 len, payload}: 1 TRA {u32 attrs (0xBBGGRR00 colour, bits 1 bold 2 italic 4 underline), u32 mask}, 2 text (UTF-16, NUL), 6 style u16, e.g. 0x5C12 dialog) to the chat log, also in single player (technique from ChaosMod) |
- New game opening (Freeport 7) plays as `mode=cutscene` with camera 0x6792F8 and mission comms; the landing at
  Manhattan is the scene mix `mix_s002xe`; the "PERSONAL ENTRY" texts are DisplayMissionMessage, no voice.
- Room scenes (bar conversations) are letterboxed but run with camera 0 (base), not the cutscene camera.
- server.dll (in process in single player): `?GetRep@SpaceObj@pub@@YAHIAAH@Z`, `?GetAffiliation@Reputation@pub@@YAHABHAAI@Z`,
  `?GetShortGroupName@Reputation@pub@@YAHABIAAI@Z` (string id → "Police", "DSE", "Gateway"),
  `?GetAttitude@Reputation@pub@@YAHABH0AAM@Z` (attitude of a sender towards the player's rep) work from the client.
- Flight HUD geometry (HDE, Advanced Widescreen HUD, 3840x2160; scales with the height, anchored to the corners):
  bottom-left panels end 0.613 H from the left, bottom-right ones start 0.564 H from the right, both 0.30 H tall;
  throttle bar 0.044 H; maneuver bar at the top centre down to 0.155 H; the chat log grows upwards from 0.30 H on
  the left; system name / objective texts type at the top left (~0.21 H).
- Front end: Escape on the main menu opens the Exit dialog (`ExitButtonBox0` = Yes, `ExitButtonBox1` = No); Escape
  skips the startup movies, Space does not. The rumour/offer window in bars: `OfferDialogCloseButton` /
  `OfferDialogClose2Button`. Bar character hover cursors: `talk_story` (story star), `talk_rumor`, `talk_bribe`,
  `talk_blowoff`, `talk_mission`.
- Load lists sort by `[Player] tstamp` read when the game starts; changing a save's tstamp later does not reorder
  the list until a restart.

**Chatter grammar (Content.dll, HDE, static disassembly via dumpbin, not traced in game).** NPC radio chatter
is built server-side in Content.dll: about 70 builder functions (0x06ED6DD0–0x06EE6C30 at the HDE load
address 0x06EA0000) each append msg ids in a fixed order to one comm and send it. The full table, one
template per builder, is `docs\chatter_grammar.tsv` (header explains the columns). Helpers:

| Address (Content.dll) | Meaning (static) |
|---|---|
| 0x06EB5770 | append a msg id to the comm being built (ids come from `pub::GetNicknameId`, Server.dll import at [0x06FB3684]; Common.dll `CreateID` is [0x06FB3000]) |
| 0x06F937C0 | generic `vector<T*>` insert (also queues line objects into a conversation) |
| 0x06EEAF10 | variant: `"<base>_%02d<suffix>"`, random N within the voice's `permutation_count` |
| 0x06EEB210 | ship name part: 0 = `gcs_dhail_mbid_trader(s)/patrol(lers)` + `_leg`/`_ill`, 1 = `gcs_refer_formationdesig_%02d`, 2/3 = the numbers (`gcs_misc_number_%d`, `-` when it ends the sentence) |
| 0x06ED6800 | send the built comm |

Example, an NPC docking request (0x06ED6DD0): `gcs_misc_thisis+`, the speaker's faction `_short`
(`gcs_refer_faction_player_short` for the player), designation, number, `gcs_misc_dash`, number,
`gcs_misc_to`, the base's `msg_id_prefix` + `-`, `gcs_dockrequest_request_0N+`, then
`gcs_dockrequest_todock-`/`tomoor-`/`toland-` by the dock type ("This is Liberty Navy Alpha 1-2 to Planet
Manhattan. I'm requesting to dock."). `_p`/`_s` msg variants are the plural ("we") / singular ("I") forms.

**Chatter triggers (Content.dll, HDE, static disassembly, not traced in game).** Per-builder conditions are
in the `trigger` column of `chatter_grammar.tsv`; the machinery that applies to all of them:
- Each builder is the constructor of a 0x30-byte line object (vt[1] = "built OK"; +0x28 = string id
  19000 + number of pieces, the "Comm STRIDS" ordering table in the resources). A conversation object
  queues its lines; the chatter manager (global object 0x06FD1C08, controller id at 0x06FD1C74,
  heartbeat 1 s) keeps the running conversations.
- **Speaker gate** (every builder): the speaker's voice (`pub::SpaceObj::GetVoiceID`) must have the role
  bits in `voices_*.ini [mVoiceProp] Voice = ..., supports_roles = ...` (parser 0x06FA692C, test 0x06FA6E30 = all bits set):
  combat_lev_1 0x4000, combat_lev_2 0x4, combat_lev_3 0xC004 (= both + 0x8000), scan 0x8, hail_hailee 0x10,
  hail_dock 0x20, dockrequest_dock 0x40, dockrequest_docker 0x80, behavior 0x100, misc_chatter 0x200,
  fidget 0x400, combat_dock 0x800, random_mission_ship 0x1000, mission_commission 0x2000. HDE voices: 17
  full pilot voices (dockrequest_docker, scan, combat_lev_3, hail_hailee, misc_chatter, fidget,
  random_mission_ship), 12 combat_lev_2, 12 combat_lev_1, 3 base voices (hail_dock, dockrequest_dock,
  combat_dock). A missing role (or a missing line) drops the line and the rest of its conversation.
- Every builder also has `if (rand()/32768 != 0) skip` (e.g. 0x06ED6E95): always 0, so it never blocks.
  Real 50/50 choices use `(rand()*2) >> 15`.
- **Dispatcher** 0x06EE8D70 (`this` = 0x06FD1C08) takes an event whose vt[0] returns the kind 1-25 and
  creates the conversation: k1 0x06EE2000 dock hail, k2 0x06EE6E60 docking, k3 0x06EE8880 launch,
  k5 0x06EE4CD0 player hail, k6-16 0x06EE2C20 combat, k17-24 0x06EE55F0 scan, k25 0x06EE7B00 fidget.
- **Event gate** 0x06EEBD10(kind, mgr, A, B), record 0x06EEB920 (last 10 per kind): global switch
  0x06FCE368 = !clamp (mission `Act_GCSClamp = true` / mission ini `gcs_clamp` silences all chatter); an
  NPC that is not owned by the global controller 0x06FD0E64 (inferred: mission-scripted ships) only
  passes kinds 6, 7, 8, 10, 11, 13-16; kinds 1-4 need A (0x06F73A40) and pass at once when B is the
  player (so bases always answer the player); otherwise a same-kind
  event recorded within 10 km (distance² < 1e8) of A (or B) and younger than the kind's cooldown blocks
  it, then `rand()/32768 < probability`. Hardcoded config 0x06F5CDA0: cooldown k2 12 s, k25 30 s,
  k6 6 s, k8 6 s, k7 0 s, k9-11 and k13-16 3 s, all others 15 s (k1, k3, k12); probability k6 0.05,
  k7 0.4, others 1. The dispatcher gates k1-3; the manager's message handler gates k6-16 and the fidget
  timer k25; k5 and the scan kinds have no cooldown.
- Shared conditions: "NPC" = found in Content's per-system ship list (0x06F73990); the player's ship is
  never in it (and trent_voice has no supports_roles), so the player never speaks chatter. Formation: list from 0x06F66220 (formation id at
  ship+0x154), leader = its first member; "follower" = ship+0x14C set and +0x154 != -1. The leader of a
  formation does the full exchange only when its **ship id % 5 == 0** (deterministic per ship);
  otherwise each ship just says its own line. Job type = ship+0x178 → vt[0x28]: 2 trader, 3 and 4
  patrol (3 = waypoint patrol, has a waypoint count via vt[0x4C]); job vt[0x34] = target (^dest).

| Kind | Source (static) |
|---|---|
| k1 dock hail | manager message 0x2D (a ship comes near a base, inferred; sets a 30 s timer); ambient NPC, not a follower |
| k2 docking | docking request handler 0x06F6E110 (response r of 0x06F6D820, posted unless r = 5), queue re-check 0x06F6E9D0 (r = 4, "now cleared" flag), trade lane handler 0x06F6FCB0 (r = 4, then `pub::SpaceObj::LaneResponse`). Base answer by r: 0 responsehostile, 1 none, 2 denied/nofit, 3 standby/delayed, 4 granted (then `pub::SpaceObj::Dock`). Target type (`GetType`): 0x40 jump gate / 0x800 jump hole → convoy gate lines, 0x80 trade lane ring → tradelanestart, 0x400 airlock → none, else base docking |
| k3 launch | launch handlers 0x06F5D690 / 0x06F607E0 |
| k5 player hail | manager message 0x1F (0x06EE9900; inferred to be the player's hail) |
| k6 hit | engine message 0x0D: damage > 0, cause 2-5 or 7, victim hull (`GetRelativeHealth`) < 1.0 |
| k7 destroyed | engine messages 0x38 (destroyed ship) and 0x39 with field +0x10 = 1 (killer) |
| k8 in sights | `ContentCallback::ship_in_sights_event` → message 0x100E |
| k9 / k11 / k10 | `ContentCallback::combat_state_notification` (0x06F34090) → message 0x1001: type 0 → k9 engage, 1 (too far) / 2 (too tough) → k11, 3 / 4 → k10 combat over (3 = followers say rejoiningform) |
| k12 help | 0x06F70580: an NPC within 5 km joins a fight on another NPC's side (sets its attitude) → message 0x1003 |
| k13 drift | `ContentCallback::combat_drift_event` → message 0x1002 |
| k14 flee | `ContentCallback::request_flee_destination` (0x06F34CE0) → message 0x1004 with the FleeReason: 0 tough, 2 damaged, 3 noweapons, else other |
| k15 friendly fire | player-damage handler 0x06F6FF90 → message 0x1005 (first field 0) |
| k16 new leader | formation leader replaced (0x06F5E680) → message 0x1009 |
| k17-24 scan | scan state machine 0x06F0CBC0 / 0x06F0DA00: 17 announce and 18 nothing found only with faction_prop `scan_announce = true`; 19 order drop (scan_for_cargo good found); 20 NPC complied (comply + happy, player: happy); 21 NPC refused (`AI::get_scan_response` 1/2); 22 angry; 23 silent; 24 loot destroyed |
| k25 fidget | manager timer (message 0x20, 0x06EEA890): each 1 s heartbeat a 1/120 roll, an ambient formation leader (not in combat) within 2.5 km of a player; topic roll r: < 1/30 rumor, < 0.2 status (whatstatus + every wingman's saystatus, or the waypoint question), else simple / cargo talk |

Thresholds: taking_damage_worst when hull < 0.49, inflicting_damage_worst when the target's hull < 0.33;
saystatus damagelarge < 0.30, medium < 0.75, small < 1.0, else none; base hails are hostile at attitude
≤ -0.6; dock type 1 berth (todock / clear_berth), 3-5 moor (tomoor / clear_moor), 6 docking ring (toland /
clear_ring, no number), 2 none; docking numbers above 20 are read digit by digit (0x06EEB590).
Needs an in-game trace: what engine messages 0x2D, 0x38, 0x39, 0x0D and 0x1F are exactly, the owner of
controller 0x06FD0E64, the NPC predicates vt[1]/vt[2], job types 3 vs 4, how queued lines are timed.

**Base and bar speech (data files, HDE).** Room conversations are Lua 3.2 bytecode scenes in
`DATA\Scripts\GCS\GCS_{FM,ML}{Sit,Stand}_<code>_NN[_MP].thn` (637 files); each SOUND entity names
`VoiceProfile_<msg>`. A `$`-prefixed msg is a placeholder Freelancer.exe resolves per base
(error text "No BGCS_base_run_by specified for base %s, cannot resolve voice profile message %s"); the
voice banks also hold a default clip under the placeholder name. Scenes that join two pieces into one
sentence: W02aF `$BGCS_Base_Name` (= `GCS_ML_W02aF_<base>`, "Welcome to Harris") + `$BGCS_Base_Run_By`
(= `GCS_ML_<universe.ini BGCS_base_run_by>`, e.g. W02bF37 "run by LWB", W02bF35 "a Liberty Planet");
A03aG `$BGCS_Faction_Rebuff_ML/FM` (= `GCS_ML_A03aG_<faction>_grp`, "The Corsairs don't") then scene
A04aG (`GCS_ML_A04aGNN`, "waste any time hunting down people who cross them."); D06aG `GCS_ML_D06aG00`
("Oh, well, I work") + `$BGCS_My_Faction_ML/FM` (= `GCS_ML_D06aG_<faction>_grp`, "for the Corsairs").
Trent's bar replies are `$BGCS_Trent_<code>P0N_<variant>` (voice trent_voice).

## Movies (Movie.dll, verified in game 17 Oct 2026)
- Startup movies: `freelancer.ini` `[Startup] movie_file` lines (HDE comments out the MGS/DA logos by default).
  Freelancer.exe imports `PlayMovie(HWND, bool, const char* path)`, `UpdateMovie()` (polled in a loop while it
  plays; the game frame loop does not run), `StopMovie()` from Movie.dll.
- Movie.dll (2003) plays through DirectShow: `CoCreateInstance(CLSID_FilterGraph)` (its own IAT), the old Video
  Renderer as a child window of class `VideoRenderer` inside the game window, stretched to the whole client area
  (aspect not kept). The picture placement can be changed without touching any window through the graph's
  `IBasicVideo` (IDispatch-based vtable): 28 SetSourcePosition(l,t,w,h) in native video pixels (crops), 31
  SetDestinationPosition(l,t,w,h) in renderer-window pixels, 32 GetDestinationPosition, 33
  SetDefaultDestinationPosition, 34 GetVideoSize; verified 17 Oct 2026 (S_OK, takes effect at once, Movie.dll does
  not reset it). Resizing the child window instead (SetWindowPos) needs SetDefaultDestinationPosition afterwards,
  leaves stale pixels around a smaller window, and disturbs capture tools.
- Intro files: HDE `fl_intro.wmv` = HD intro (1440x960, 104 MB, 223.6 s), `fl_intro_vanilla.wmv` = 2003 intro
  (720x480, 39 MB, 226.8 s); Bonus DVD full intro 720x480, ~451 s. Encoded bars (cropdetect): HD picture
  1438x594 at y 182, vanilla 718x296 at y 92; DA logo 1440x838 at y 60; MGS logo 1280x960 without bars.

## HD Edition
- Console plugin (console.dll): commands are typed in chat, e.g. `launch` (did nothing in story mission 1,
  blocked by the mission), `rep l H`. Enter must be a real window message (PostMessage WM_KEYDOWN/WM_CHAR/
  WM_KEYUP VK_RETURN); FL's own key handler path does not submit the chat line.
- HudShift.dll moves HUD elements (`repos\HudShift`).
- Tutorial popups reference keyboard keys ("N key", "F5 key", "press F1").
