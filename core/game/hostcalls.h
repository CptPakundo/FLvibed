// FLvibed - the host services core's own logic calls (fl, events, mprules). A module's HostApi is that module's; these few are
// handed over here at its Init, so core\game names no module. Null until then: every caller fails closed.
#pragma once
#include "common.h"
#include "game/events.h"
#include "hud_bus.h"
#include "settings_bus.h"
#include "ui_buttons.h"

namespace hostcalls
{
struct Table
{
    void (*SetHeld)(int cmd, bool held);
    void (*ClearAllHeld)();
    void (*PushEvent)(const char* text);
    u32 (*ChatLineAt)(u32 n, u32* sender, wchar_t* text, int max);
    u32 (*ServerTag)(u32* sender, wchar_t* text, int max);
    // the options page's
    const SettingsBus* (*GetSettingsBus)();
    const HudBus* (*GetHudBus)();
    void* (*GetD3DDevice)();
    int (*OptionsPageState)();
    bool (*ShowOptionsPage)(bool on);
    bool (*UiButtonRect)(int id, float* cx, float* cy, float* hw, float* hh);
    void (*SetUiButton)(int id, bool shown, bool lit, bool enabled);
    int (*RealMouseWheel)();
    void (*SetHudHitSuppression)(bool on, const uintptr_t* roots, int count);
    const char* (*WindowMode)(char* detail, int len);
    // the cut modules'
    bool (*ModuleOn)(int row); // a [Modules] row (core\modules_rules.h)
    u32 (*ObjectiveText)(wchar_t* out, int max);
    u32 (*ObjectiveNow)(ObjectiveStep* out);
    int (*MissionMarks)(u32* out, int max);
    u32 (*MissionResult)(u32* type);
    u32 (*MissionSave)(u32* ids);
    void (*LogWrite)(const char* line); // the running entry's log, for LogJoin
    // what flies the ship
    void (*SetSteer)(bool active, float x, float y);
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
    u32 (*LastRealMouseTick)();
    // the records and hooks the autopilot reads
    u32 (*ObjectiveTitle)();
    int (*ObjectiveRefs)(u32* out, int max);
    int (*ObjectiveSteps)(ObjectiveStep* out, int max);
    int (*MissionObjects)(u32* out, int max);
    u32 (*DockReply)(u32* ship, u32* kind, u32* response);
    int (*JumpInCount)();
    u32 (*PlayerHit)(float pos[3], u32* kind, u32* tick);
    unsigned long long (*CommHeardAt)(u32 line);
    int (*SpawnedShips)(SpawnRec* out, int max);
    bool (*GuardEvade)(void** vtable);
    bool (*SetGunAim)(bool on, const float* world);
    int (*SetHudHotspots)(const HudHotspot* spots, int count);
    int (*HudHotspotHovered)();
    u32 (*GameLoads)();
    u32 (*ObjectiveReread)();
    u32 (*UiGeneration)();
    int (*GetRoomCharacters)(void** out, int max);
    void (*SetKeepRunning)(bool on);
    void (*SetCursorHidden)(bool on);
    int (*FixedDt)(int fps);
    void* (*CommPlayer)();
    void (*SetKeyTrace)(bool on);
    void (*DeferToFrameEnd)(void (*fn)(void* ctx), void* ctx); // core's page host only: the controller's entry sets it by uiclick::SetDeferral
    void (*SetSteerLift)(bool underWindow, bool autopilot);
};
inline Table g = {};

// The game's cursor arrow hidden while any source asks: the pad screens, the flight's idle cursor (hud_fade 1, core\game\hudwake.h CursorStep). The host
// hears only the OR. A reloaded logic does not know what the host still holds, so its first call always sends.
enum : unsigned
{
    kCursorHidePad = 1u,
    kCursorHideIdle = 2u
};
inline void CursorHide(unsigned source, bool on)
{
    static unsigned mask = 0;
    static int sent = -1;
    mask = on ? (mask | source) : (mask & ~source);
    const int want = mask != 0 ? 1 : 0;
    if (want == sent)
        return;
    sent = want;
    if (g.SetCursorHidden)
        g.SetCursorHidden(want != 0);
}

inline u32 ObjectiveNow(ObjectiveStep* out) { return g.ObjectiveNow ? g.ObjectiveNow(out) : 0; }
inline int MissionMarks(u32* out, int max) { return g.MissionMarks ? g.MissionMarks(out, max) : -1; }
inline u32 MissionResult(u32* type) { return g.MissionResult ? g.MissionResult(type) : 0; }
inline u32 MissionSave(u32* ids) { return g.MissionSave ? g.MissionSave(ids) : 0; }
} // namespace hostcalls
