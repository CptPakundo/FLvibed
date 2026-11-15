# FLvibed controller module — reverse-engineering notes (module-specific)

Game facts (addresses, structures, vtables, mission/save formats) live in the shared notes:
`..\..\docs\FREELANCER_RE.md` (FLvibed\docs) — read them before touching hooks,
and add newly verified game facts there, not here. This file only records how FLController uses them.
Verified against 038eb1f (29 Oct 2026: paths corrected; the table is the core of the host's hooks since 17 Oct - the
full set is every `hk::Detour` / IAT / vtable patch in `src\host\*.cpp` (main.cpp, events.cpp the economy and Content.dll
hooks, uibtn.cpp the native buttons, winmode.cpp borderless windowed) and `..\..\core\host\*.cpp` (router.cpp the shared hooks,
input.cpp, dimouse.cpp, focus.cpp, fixes.cpp), each address a signed symbol in
`..\..\core\flexe.h` with its comment).

## Hooks installed by the host (`src/host/main.cpp`)
| Address | Use |
|---|---|
| 0x5B2890 FrameUpdate | → `impl->OnFrame` |
| 0x5784A0 IsCmdHeld / 0x576410 HandleKey | command injection for pad bindings (event mask from the keymap) |
| 0x41F150 mouse poll + cursor draw | → `impl->OnRenderOverlay()` (inside UI rendering, before the cursor) — overlay hook point |
| 0x59E300 UI pass (exe::UiRender) | entry → `impl->OnRenderUnderHud()` (after the 3D scene, before every HUD/UI control: under the HUD); not called while [0x67DCF0] > 0 (the game skips its UI) |
| 0x4C7CB0 mouse-flight steering | override: pad direction instead of cursor offset. Sets timer 0x673550 = 1 for the call and restores timer/engaged flag (0x673554) afterwards, so the "MOUSE FLIGHT" label does not stick. On release, sends neutral frames (FL keeps the last direction). `steer_scale` 0.77 = saturation |
| 0x59DD80 UI HitTest | detour (8-byte prologue) hides HUD roots while the pad or bot flies, so hover does not block steering/aim (HUD transparency, HostApi v4 `SetHudHitSuppression`) |
| DirectInput mouse | injection of cursor motion/buttons (needs FL in the foreground) |
| RP8.dll IAT → CreateDevice/Reset | D3D8 device capture (`GetD3DDevice`) |
| Content.dll imports (server.dll pub::) | event log (objectives, popups, messages, comms, failure) + `IdsText` |
| 0x443ED0 / 0x4440C0 room character ctor / dtor | live list of base-room characters (HostApi v5 `GetRoomCharacters`); `..\..\core\game\room.cpp` uses their vtable natively: vt[10] hover-enter (sets the offer cursor), vt[11] leave, vt[12] click = talk. Debug: `talkers`, `talk <n or offer>` (added from the FLSubtitles session, 17 Oct 2026) |

## Logic-side choices
- Projection: `fl::WorldToScreen` uses the current camera's fields directly (FLUF's 0x40F4F0 failed from our
  frame update), read once under a guard by `fl::ReadCamera` (named offsets, 29 Oct 2026); UI positions use the UI
  camera 0x67DBF8 (`ui.cpp`).
- Lead cross: Common.dll's exported `CShip::get_tgt_lead_fire_pos` on the ship's cobject() (`fl.cpp`).
- Hostility: attitude ≤ −0.55 counts as hostile (`aim.h` kHostileAttitude: aim assist, bot, select-by-firing).
- Lists: load/save rows are hit by the per-class row metrics (no arrow-key support in FL lists).
- D3D8 vtable calls: `VCall<>()` from `..\..\core\hook.h` with index constants.
- Command ids: enum in `../core/game/fl.h`.

## Gotchas
Moved here unchanged from the module map (`..\CLAUDE.md` "Gotchas" points here; the documents diet, 13 Nov 2026).
- Send commands with their keymap event mask (keydown-only commands toggle twice on key-up).
- Menus react to raw keys (`fl::TapKey(VK_ESCAPE)`), not commands.
- Windowed modes release the mouse at the window edge; the pad re-captures it.
- A `WH_MOUSE_LL` hook of ours and the real mouse (14 Nov 2026, the owner's PC, real DualSense + mouse, the SDL backend's hooks;
  `..\..\..\audit_2026-11-12\mouse_trace_2.csv`). **Measured:** Freelancer.exe's `SetCursorPos` warps never reached the hook as injected moves
  (an own-warp counter stayed 0 for 80 s) and no event was dropped, the pad idle. Yet before the pad was first used Windows' pointer left the
  centre by more than 40 px in 189 of 1,365 samples and the game's cursor crossed the screen in 60-130 ms; after the pad was used once, 1 of
  569 at the same hand speed. **Nothing ties that to the hook:** the lead read it at the time as the hook speeding the mouse up, with no
  measurement behind it; three later recordings (14 Nov 2026, `docs\FREELANCER_RE.md` "speed-dependent gain") found the cursor's
  speed-dependent gain in the game's own DirectInput mouse, also with FLvibed not loaded, and raw mouse input (`mouse_raw`) now replaces
  it. The mouse hook is installed only with `pad_mapper_mouse` 1 (`src\host\sdlpad.cpp`, `padbackend::FilterFlags`) because nothing needs
  it unless a mapper also sends mouse input for the pad: one hook less in the system's mouse chain. The keyboard hook stays.
- UI positions: always project through the UI camera (pixel = center + scale x xy / -z).
- Bar characters are 3D hotspots (vtable 0x5CB930), not UI controls: the pad reaches them through `room::Hover` / `Talk` (`padbases.*`).
- USER_NEXT / PREV_TARGET (the RS click's next-enemy fallback) walks the HUD contact list under the player's current filter: with the Important filter it can find nothing. Native, filter-independent targeting: `..\..\core\game\space.*`, `..\..\docs\FREELANCER_RE.md` "Targets, contacts, docking".
- Events (objectives, popups, comms) come from import hooks in Content.dll, which the game reloads (after a death, say): re-applied at its load notification (`..\..\core\host\events.cpp` ContentModuleLoaded), `events::Update` as the backup.
- Story missions have timers and nags that fail the mission: read the script before testing in one.
- The player's maneuvers are AI directives on the ship's behaviour manager; their state is readable (`pilot`, `Get-FLPilot`; `..\..\docs\FREELANCER_RE.md` "Maneuvers, waypoints, lanes, jumps, zones"). The game's autopilots fly straight into minefields and damage zones: harness flights use `..\..\tools\harness\flroute.ps1`.
- Do not use the quick-load routine 0x5B2C70: rooms come up without characters (`Load-FLSave` uses the menus).
- Guns fire by their own fire slots, never fire-all (it launches missiles too); the equip "active" flag reads 0 for droppers and launchers (count them by ammo).
