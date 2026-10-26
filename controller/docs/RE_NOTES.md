# FLvibed controller module — reverse-engineering notes (module-specific)

Game facts (addresses, structures, vtables, mission/save formats) live in the shared notes:
`..\..\docs\FREELANCER_RE.md` (FLvibed\docs) — read them before touching hooks,
and add newly verified game facts there, not here. This file only records how FLController uses them.

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
| 0x443ED0 / 0x4440C0 room character ctor / dtor | live list of base-room characters (HostApi v5 `GetRoomCharacters`); `impl/room.cpp` uses their vtable natively: vt[10] hover-enter (sets the offer cursor), vt[11] leave, vt[12] click = talk. Debug: `talkers`, `talk <n or offer>` (added from the FLSubtitles session, 17 Oct 2026) |

## Logic-side choices
- Projection: `fl::WorldToScreen` uses the current camera's fields directly (FLUF's 0x40F4F0 failed from our
  frame update); UI positions use the UI camera 0x67DBF8 (`ui.cpp`).
- Lead cross: Common.dll `CShip::get_tgt_lead_fire_pos` via IObjInspect vt[84] cobject() (`fl.cpp`).
- Hostility: attitude (vt[26]) ≤ −0.55 counts as hostile (aim assist, bot, select-by-firing).
- Lists: load/save rows are hit by the per-class row metrics (no arrow-key support in FL lists).
- D3D8 vtable calls: `VCall<>()` from `src/hook.h` with index constants.
- Command ids: enum in `src/fl.h`.
