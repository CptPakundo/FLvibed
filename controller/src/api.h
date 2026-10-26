// Interface between the controller module's host (in FLvibed.dll: hooks, injection, shared memory) and its
// logic (gamepad mapping, UI navigation, overlay). In the dev build the logic is FLControllerImpl.dll, rebuilt
// and hot-reloaded while the game keeps running; in the release build it is linked into FLvibed.dll.
#pragma once
#include "common.h"

struct ShmBlock;

// The current objective step (host API v11 ObjectiveNow): the objective's text id, its 'R' values (reputation
// instances: space::ObjectiveObject resolves them) and its navmarker (the HUD's "Mission Waypoint").
struct ObjectiveStep
{
    u32 textId;
    u32 refs[4];
    int refCount;
    bool hasNav;
    float nav[3];
    u32 navSystem;
};

// A ship Content.dll spawned (host API v17 SpawnedShips): its space id, archetype id and exact spawn position - the
// mission script's ships spawn at their [MsnShip] / [MsnFormation] position (mission::MapSpawns).
struct SpawnRec
{
    u32 id;
    u32 arch;
    float pos[3];
};

// Host API version: v1 = initial, v2 adds SetSteer and LastRealMouseTick, v3 adds PushEvent and
// IdsText, v4 adds SetHudHitSuppression, v5 adds GetRoomCharacters, v6 adds GuardEvade, v7 adds SetKeepRunning, v8
// adds JumpInCount, v9 adds CommPlayer, v10 adds MissionObjects, v11 MissionMarks + ObjectiveRefs, v12 PlayerHit, v13
// ObjectiveTitle, v14 DockReply, v15 ObjectiveSteps, v16 CommHeardAt, v17 SpawnedShips, v18 the UI build (25 Oct 2026:
// GetSettingsBus, the host-created native buttons SetUiButton / UiButtonRect, OptionsPageState / ShowOptionsPage,
// LastRealKeyTick, RealMouseDown), v19 RealMouseMotion (the HUD track's request, 25 Oct 2026: the physical mouse's motion
// in pixels for the device switch's threshold), v20 SetCursorHidden + MissionResult (the integration of the screen
// tracks, 25 Oct 2026), v21 GetHudBus (HUD 2, 25 Oct 2026: core\hud_bus.h), v22 SetHudHotspots + HudHotspotHovered (HUD 2's
// keyboard-and-mouse clicks, 25 Oct 2026), v23 UiGeneration (Extended HUD 2, 25 Oct 2026: windows opened / closed without
// polling), v24 ObjectiveText (HUD 2's objective line as the game formats it, 25 Oct 2026), v25 ObjectiveReread (the
// objective a save load restores without an objective event, 25 Oct 2026), v26 WindowMode (who provides borderless
// windowed, 26 Oct 2026: controller\src\host\winmode.h), v27 RealMouseWheel (the options page's wheel, 26 Oct 2026). A
// logic DLL must check `version` before using functions added in later versions.
constexpr u32 kHostApiVersion = 27;
// Logic DLL API version, checked by the host when loading the logic DLL (exact match): v2 adds OnRenderUnderHud, v3
// adds OnUiButton and OnRawKey (the UI build).
constexpr u32 kImplApiVersion = 3;

// v18: FLvibed's native buttons, created by the host through the game's own icon-button factory (0x5A0C90) so the game's
// hit test, hover, click sound and click-vs-fire arbitration apply (UI_PLAN.md 2, 3.1): the host creates, places and
// shows them, routes their clicks to the logic (ImplApi v3 OnUiButton) and lets the logic set their state.
enum UiButton : int
{
    kUiBtnBar = 1,         // the fifth maneuver-bar button (the Advanced Autopilot), parented to HUD_Maneuvers
    kUiBtnOptionsPage = 2, // the FLVIBED page button in the game's Options screen (NN_Preferences' free slot)
    kUiBtnPauseFlvibed = 3, // the sixth button of the F1 pause menu (NN_Pause), FLVIBED
    // v22: HUD 2's hotspots, kUiBtnHud2 + slot (slot 0 .. kUiBtnHud2Count - 1, the logic's numbering: SetHudHotspots)
    kUiBtnHud2 = 100,
    kUiBtnHud2Count = 32
};

// v22: one HUD 2 hotspot as the logic wants it this frame: a rectangle in back-buffer pixels (x0 < x1, y0 < y1) and
// whether it is shown (an unshown one is hidden and not hit-testable).
struct HudHotspot
{
    float x0, y0, x1, y1;
    int shown;
};

struct SettingsBus;
struct HudBus;

struct HostApi
{
    u32 version;
    const char* dllDir; // directory of FLvibed.dll (the game's EXE folder), trailing backslash
    ShmBlock* shm;      // debug/test channel (may be null)

    void (*LogWrite)(const char* line);

    // Commands polled through Freelancer's IsCommandHeld
    void (*SetHeld)(int cmd, bool held);
    void (*ClearAllHeld)();
    void (*SetKeyTrace)(bool on);

    // Virtual mouse (DirectInput injection)
    void (*MouseMoveBy)(float dx, float dy);
    void (*MouseMoveTo)(float x, float y);
    void (*MouseReleaseTarget)();
    void (*MouseWheel)(int notches);
    void (*MouseButton)(int index, bool down);
    bool (*MouseButtonDown)(int index);
    void (*MouseReset)();
    void (*MouseStats)(char* out, int outLen);
    bool (*MouseIsCaptured)();
    void (*MouseCapture)();

    // D3D8 device used by Freelancer (null until created)
    void* (*GetD3DDevice)();

    // Flight steering override: while active, Freelancer's mouse-flight steering uses
    // (x, y) in [-1, 1] (fraction of half-screen, +y = nose up) instead of the cursor.
    void (*SetSteer)(bool active, float x, float y);

    // GetTickCount() of the last physical mouse movement/button activity.
    u32 (*LastRealMouseTick)();

    // v3: append a line to the game event log (see events.h); text of a string resource id (UTF-8).
    void (*PushEvent)(const char* text);
    // IdsText: kept for the layout; the logic calls core\text.cpp directly since 24 Oct 2026 (impl\shims.cpp).
    void (*IdsText)(u32 ids, char* out, int outLen);

    // v4: while on, Freelancer's UI hit test (0x59DD80) ignores every control under the given root
    // windows (the flight HUD): nothing is hovered, so the aim cursor can cross the HUD without
    // stopping mouse-flight steering or clicking HUD buttons. The host copies the list.
    void (*SetHudHitSuppression)(bool on, const uintptr_t* roots, int count);

    // v5: live base-room characters (bar patrons, dealers, story characters; class vtable 0x5CB930), oldest
    // first, tracked by detours on their constructor 0x443ED0 / destructor 0x4440C0. Returns the count.
    // Use them natively (impl/room.cpp): vt[10] hover-enter, vt[11] hover-leave, vt[12] click.
    int (*GetRoomCharacters)(void** out, int max);

    // v6: guard the game's Evade behaviour (Common.dll) so it can run on the player ship: `vtable` is the vtable of
    // the player's Evade behaviour object (behaviour manager +0x174 + 5 * 4). The host verifies the code and patches
    // the update once (lives in the host: survives logic reloads). True when the guard is in place.
    bool (*GuardEvade)(void** vtable);

    // v7: keep the game running while its window is not in focus (the game, the mod and the harness go on; the
    // physical mouse is released to Windows and ignored, the virtual mouse keeps working).
    void (*SetKeepRunning)(bool on);

    // v8: how many times the client has reported a jump-in complete to the server (detour on exe::JumpInComplete,
    // the game's own end of the jump sequence: the ship is placed in the new system and flying). -1 if not hooked.
    int (*JumpInCount)();

    // v9: the client comm player (the radio: plays comms one after another), captured from its comm-start method
    // (detour on exe::Comm, chained with the subtitles module's); null until the first comm. Its state: docs\
    // FREELANCER_RE.md "comm player state".
    void* (*CommPlayer)();

    // v10: the objects the mission points the player at, newest first: objects the server marks for the player
    // (pub::Player::MarkObj, the HUD's mission marker; unmarked ones drop out) and the current objective's target
    // objects ('R' entries of pub::Player::SetObjectives). Returns the count. The player's own information.
    int (*MissionObjects)(u32* out, int max);

    // v11: the two halves of MissionObjects apart. MissionMarks: marked space ids, newest first. ObjectiveRefs: the
    // current objective's 'R' values as sent - reputation instances (FmtStr::append_rep_instance), which overlap the
    // small space ids of ships, so v10's merged list can mix them up (space::ObjectiveObject resolves them).
    int (*MissionMarks)(u32* out, int max);
    int (*ObjectiveRefs)(u32* out, int max);
    // v11: the current objective step (entry 0 of the last SetObjectives); returns its sequence number (changes with
    // every SetObjectives call, 0 = none yet). out may be null.
    u32 (*ObjectiveNow)(ObjectiveStep* out);

    // v12: the last hit on the player ship as the game's HUD damage indicator receives it (detours on its two inputs):
    // the world point, kind 1 = a damage source (exe::DmgIndSource: for a round that hit, 1000 x its velocity back from
    // the hit point; for an explosion near the ship, its centre), 2 = a contact hit (exe::DmgIndContact: collisions),
    // GetTickCount of the hit. Returns a sequence number that grows
    // with every hit (0 = none yet); out pointers may be null.
    u32 (*PlayerHit)(float pos[3], u32* kind, u32* tick);

    // v13: the mission's title string id given with the last SetObjectives (22602 "ESCAPE FROM LIBERTY"), 0 none yet -
    // the mission's intent for the objective mode (a survival title keeps the survival stance for the whole mission).
    u32 (*ObjectiveTitle)();

    // v14: the server's last answer to a player request (IClient Send_FLPACKET_SERVER_REQUEST_RETURNED, slot 83:
    // ship, kind, response): kind 0 = dock (bases, rings, gates, holes), 2 = lane (seen as the 2nd table); response
    // for kind 0 as the client voices it (exe 0x546D00): 0 denied hostile, 1 denied locked, 2 / 5 refused
    // (cannot_dock), 3 queued ("dock in use - entering queue", waiting_to_dock), 4 granted. Returns a sequence
    // number that grows with every answer (0 = none yet); out pointers may be null.
    u32 (*DockReply)(u32* ship, u32* kind, u32* response);

    // v15: every entry of the last SetObjectives (a route objective: M07 "Fly to Planet Kyushu" = 5 entries, lane rings
    // then the planet; ObjectiveNow gives entry 0 only), each with its 'R' values and navmarker; returns the count.
    int (*ObjectiveSteps)(ObjectiveStep* out, int max);

    // v16: when a comm line (its id = CreateID of the line's nickname, as a script's Cnd_CommComplete names it) was last
    // sent (GetTickCount64; 0 = not this game session) - a script's state observed through what it says (session 8:
    // M10's "botzler_dies" disarms Von Claussen's leash on DX_M10_0460B_BOTZLER).
    unsigned long long (*CommHeardAt)(u32 line);

    // v17: the ships spawned in this game session, oldest first (the last 2048), copied into `out` (at most `max`, the
    // newest when more); returns the count copied. Session 12: the mission's ships by their spawn position (pre_aggro).
    int (*SpawnedShips)(SpawnRec* out, int max);

    // v18: the settings bus (core\settings_bus.h): every module's registry in one place, for the options page.
    const SettingsBus* (*GetSettingsBus)();
    // v18: state of a host-created native button: shown (the control visible and hit-testable), lit (the maneuver
    // bar's active-yellow / the selected page's caption), enabled (grey when not). The host applies it at once and
    // re-applies it after the game's own per-frame layout (ManeuverLayout). A button the host could not create (a
    // symbol unresolved: fails closed) ignores this and UiButtonRect returns false.
    void (*SetUiButton)(int id, bool shown, bool lit, bool enabled);
    // v18: the button's projected screen rectangle (pixels: centre and half extents), false when it does not exist or is
    // hidden - the logic draws the bar button's icon / the page button's caption over it (render::Sprite / Text).
    bool (*UiButtonRect)(int id, float* cx, float* cy, float* hw, float* hh);
    // v18: the game's Options screen (NN_Preferences): 0 closed, 1 open on one of the game's pages, 2 open on FLvibed's
    // page (the host hid the game's page controls; the logic draws the page in the panel area; any game page button,
    // RETURN / MAIN MENU or the close X puts the game's page back and the state returns to 1 / 0).
    int (*OptionsPageState)();
    // v18: switch the open Options screen to FLvibed's page (true) or back to the game's page (false); false when the
    // screen is not open or the page switch is unavailable (a symbol unresolved).
    bool (*ShowOptionsPage)(bool on);
    // v18: GetTickCount() of the last keyboard key the player pressed (the game's own key handler, FLvibed's injected
    // keys and commands excluded) - the input-device service's keyboard half (UI_PLAN.md 6.2). 0 = none yet.
    u32 (*LastRealKeyTick)();
    // v18: the physical mouse's button state (0 left, 1 right, 2 middle; DirectInput, the virtual mouse excluded).
    bool (*RealMouseDown)(int index);
    // v19: the physical mouse's motion since the last call, in window pixels (DirectInput counts scaled as the game
    // scales them; the virtual mouse excluded); true when it moved. The device service's threshold (UI_PLAN.md 6.2).
    bool (*RealMouseMotion)(int* dx, int* dy);
    // v20: hide the game's cursor arrow (its draw inside exe::CursorUpdate is skipped; the mouse poll, the hit test and
    // the hover keep working) while on - the pad screens with the pad cursor hidden (UI_PLAN.md 6.2). Off = the game's
    // own drawing. Fails closed: without the game's cursor-shown flag (exe::CursorShown unresolved) nothing changes.
    void (*SetCursorHidden)(bool on);
    // v20: a sequence number that grows with every mission result the client receives (pub::Player::DisplayMissionMessage
    // of the SUCCESS / FAILURE kinds, as events.cpp names them: the death / failure dialog, "Mission Result: Success",
    // a job's result); `type` (may be null) receives 1 success / 2 failure of the latest. 0 = none yet.
    u32 (*MissionResult)(u32* type);
    // v21: the HUD bus (core\hud_bus.h): HUD 2 publishes its layout there, the subtitles logic reads it (and answers).
    const HudBus* (*GetHudBus)();
    // v22: HUD 2's clickable areas in its keyboard-and-mouse form (docs\UI_HUD2.md 5.4; docs\FREELANCER_RE.md "HUD 2
    // hotspots"). The host keeps kUiBtnHud2Count invisible native icon buttons (no mesh: nothing drawn; the game's factory
    // 0x5A0C90, as the bar button) inside one host-created top-level control that stays visible while the game's HUD
    // roots are hidden, so the game's own hit test, hover (cursor, steering and fire stop over them as over its HUD), click
    // sound and click-vs-fire arbitration apply; a click reaches the logic as ImplApi OnUiButton(kUiBtnHud2 + slot).
    // Called every frame while HUD 2 shows on keyboard and mouse with the whole set (`count` <= kUiBtnHud2Count, slots
    // past `count` hidden): the host compares it with the last set and places / shows / hides only the buttons that
    // changed (and re-places all when the UI camera changes). A set not renewed for 2 frames is hidden (the logic stopped
    // or unloaded). Returns the number of hotspots the host serves (0: unavailable - a symbol unresolved or the controls
    // not created: fails closed, HUD 2 has no clicks).
    int (*SetHudHotspots)(const HudHotspot* spots, int count);
    // v22: the hotspot under the game's cursor (the game's hovered control): kUiBtnHud2 + slot, 0 none.
    int (*HudHotspotHovered)();
    // v23: the UI generation: a counter that grows with every control the game adds to its flat control list
    // (exe::UiControlAdd, each control's Init) or removes from it (exe::UiControlRemove, the FlControl destructors), so
    // the logic learns that a window opened or closed without polling: compare it once per frame and resolve the open
    // windows (by name / vtable, docs\FREELANCER_RE.md "Extended HUD 2: window art and events") only when it moved. It
    // moves in bursts (a window = one burst of tens of controls; removes count ~3 per control, one per destructor level)
    // and stays still in calm rooms, NavBar room changes and calm flight (measured); other controls the game makes itself
    // (the arrival line, launch / dock) move it too, so it says "the tree changed", not "a window opened". 0 = unavailable
    // (a symbol unresolved or a detour refused: fails closed - the logic falls back to its own lookup).
    u32 (*UiGeneration)();
    // v24: the current objective step's text exactly as the game shows it: the entry's FmtStr through the game's own
    // formatter (exe::FmtStrFormat) inside the SetMissionObjectives hook - values filled in, a code without a handler
    // (vanilla 1.1's "%M") dropped. Returns the objective sequence (ObjectiveNow's); `out` "" = formatter unavailable.
    u32 (*ObjectiveText)(wchar_t* out, int max);
    // v25: re-reads the current objective from the game's own client copy (exe::ClientObjective: what the NeuroNet and
    // the objective banner show) and takes it over - text id, formatted text, 'R' values, steps; a new sequence - when it
    // differs from the hooked one. A save load restores a mission's objective there without any SetMissionObjectives
    // call; the logic calls this once per mode change out of the menus (event-driven, no polling). Returns the sequence.
    u32 (*ObjectiveReread)();
    // v26: who provides borderless windowed now (winmode.h ProviderNow): "flvibed", "game", "wrapper" (`detail`: its
    // d3d8.dll), "off", "unverified", "undecided".
    const char* (*WindowMode)(char* detail, int len);
    // v27: the physical mouse wheel's whole notches since the last call (+ = up; the virtual mouse's excluded) - the
    // options page's scrolling (the owner, 26 Oct 2026: its scroll bars did not take the mouse).
    int (*RealMouseWheel)();
};

struct ImplApi
{
    u32 version;
    void (*Init)(const HostApi* host);
    void (*Shutdown)();
    void (*OnFrame)(double dt);
    // Debug command not handled by the host; writes a reply.
    void (*Command)(const char* line, char* reply, int replyLen);
    // Called inside Freelancer's UI rendering, after the HUD/UI and before the cursor (over the HUD).
    void (*OnRenderOverlay)();
    // Called when the D3D device is about to be reset / was reset.
    void (*OnDeviceLost)();
    void (*OnDeviceReset)();
    // v2: called at the start of Freelancer's UI pass (exe::UiRender), after the 3D scene and before any HUD/UI
    // control: what is drawn here lies under the HUD. Only on frames where the game draws its UI.
    void (*OnRenderUnderHud)();
    // v3: a host-created native button (UiButton) was clicked (the game's own click path: hit test, release, sound).
    void (*OnUiButton)(int id);
    // v3: a keyboard key as the game's key handler sees it (virtual-key code; down = press, else release), before the
    // game acts on it; `textField` = a text field (the chat's CTID, a save description) has the focus, so letters are
    // typing, not commands. Returns true to swallow the key (the game does not see it): the autopilot key, the options
    // page's keyboard navigation while it is open. Called on the game's thread.
    bool (*OnRawKey)(int vk, bool down, bool textField);
};

// Exported by FLControllerImpl.dll (dev build)
typedef const ImplApi*(__cdecl* GetImplApiFn)();
