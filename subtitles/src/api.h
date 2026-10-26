// Interface between the subtitles module's host (in FLvibed.dll: hooks, D3D capture, debug channel) and its
// logic (subtitle data, timing, layout, drawing). In the dev build the logic is FLSubtitlesImpl.dll, rebuilt
// and hot-reloaded while the game keeps running; in the release build it is linked into FLvibed.dll.
#pragma once
#include "common.h"

// Host API version: v1 = initial, v2 adds GetRoomCharacters, v3 adds GetMovieGraph, v4 adds ShiftManeuverBar, v5 adds
// ShiftCommWindow, v6 adds GetSettingsBus (the UI build, 25 Oct 2026), v7 SetIntroOverride (the movie_intro row), v8
// GetHudBus (HUD 2: core\hud_bus.h). A logic DLL must check `version` before using later additions.
constexpr u32 kHostApiVersion = 8;

struct SettingsBus;
struct HudBus;
// Logic DLL API version, checked by the host when loading the logic DLL (v5: OnHudPart, v6: OnManeuverLayout).
constexpr u32 kImplApiVersion = 6;

struct HostApi
{
    u32 version;
    const char* dllDir; // directory of FLvibed.dll (the game's EXE folder), trailing backslash
    void (*LogWrite)(const char* line);
    void* (*GetD3DDevice)(); // IDirect3DDevice8 used by Freelancer (null until created)
    // v2: live base-room characters (class vtable 0x5CB930: bar patrons, dealers, story characters), oldest
    // first, tracked by detours on their constructor 0x443ED0 / destructor 0x4440C0. Returns the count.
    int (*GetRoomCharacters)(void** out, int max);
    // v3: the DirectShow filter graph (IGraphBuilder*) Movie.dll created for the movie playing, or null.
    void* (*GetMovieGraph)();
    // v4: moves the space HUD's maneuver bar horizontally by dx HUD units (HudShift's group units: the UI camera's x
    // scale in pixels per unit; 0 = where the game / HudShift put it) - its position constants, read when the HUD is
    // built, so it takes effect at the next game load. Idempotent; re-applied over HudShift's own rewrites. False when
    // refused (a symbol unresolved, or the buttons' x is not HudShift's relocated float: the vanilla one is shared).
    bool (*ShiftManeuverBar)(float dx);
    // v5: moves the comm window (the speaker's portrait, its name lines, the generic icon; HudShift's group Comm)
    // vertically by dy HUD units (negative = down; 0 = where the game / HudShift put it), the same way: its position
    // constants, idempotent, re-applied over HudShift's rewrites. False when a symbol did not resolve.
    bool (*ShiftCommWindow)(float dy);
    // v6: the settings bus (core\settings_bus.h): the logic registers its [Subtitles] registry (display.cpp kDefs, the
    // movie keys included) at Init and unregisters at Shutdown, so the options page (controller logic) lists, changes
    // and saves the subtitle settings live.
    const SettingsBus* (*GetSettingsBus)();
    // v7: which intro plays ([Subtitles] movie_intro): when the game starts a startup movie whose file name contains
    // "intro" (freelancer.ini [Startup] movies\FL_Intro.wmv), PlayMovie gets `path` instead (a path as the game passes
    // them, relative to EXE\ or absolute; the file must exist); null = as installed. The host keeps a copy.
    void (*SetIntroOverride)(const char* path);
    // v8: the HUD bus (core\hud_bus.h): where HUD 2 (the controller logic) draws, so the dialogue panel, the chat
    // column, the objective banner and the arrows keep clear; the logic answers with where its panel landed.
    const HudBus* (*GetHudBus)();
};

// A comm (radio message) as Freelancer.exe receives it at 0x4CE000: a voice and up to 16 line ids.
struct CommInfo
{
    u32 from, to;  // sender / receiver object ids
    u32 voice;     // voice id (CreateID of the voice nickname)
    u32 lines[16]; // msg ids
    int count;
    const void*
        record; // the raw record (the offsets read: impl.cpp CommWord / OnComm; ..\..\docs\FREELANCER_RE.md "Comms")
};

struct ImplApi
{
    u32 version;
    void (*Init)(const HostApi* host);
    void (*Shutdown)();
    void (*OnFrame)(double dt);
    // Debug command (from tools\flsub.ps1); writes a reply.
    void (*Command)(const char* line, char* reply, int replyLen);
    // Called inside Freelancer's UI rendering, before the mouse cursor is drawn (0x41F150): the
    // overlay draws here, under the cursor, inside the game's own scene.
    void (*OnRenderUi)();
    // Called in Present, after the game has drawn the frame: draws the overlay when the UI pass did not
    // run this frame (cutscenes, movies).
    void (*OnPresent)();
    // Called when the D3D device is about to be reset / was reset.
    void (*OnDeviceLost)();
    void (*OnDeviceReset)();
    // Trace / event hooks
    void (*OnComm)(const CommInfo& comm);
    void (*OnVoiceLookup)(u32 voice, u32 msg, void* sound, u32 caller);
    // A sound archetype starts playing (Freelancer.exe 0x428680): story sounds, scene mixes, effects.
    // id = the archetype's nickname id (CreateID of the story_sounds.ini nickname).
    void (*OnSoundStart)(u32 id, const void* archetype, u32 caller);
    // Movies (Movie.dll, before the frame loop runs: the host loads the logic DLL early for them).
    // event 0 = PlayMovie(hwnd, path), 1 = UpdateMovie (each pass of the movie loop), 2 = StopMovie.
    void (*OnMovie)(int event, void* hwnd, const char* path);
    // v5: a HUD indicator part is about to be drawn (exe::HudDrawPart): callerEbp = the caller's ebp (the target indicators'
    // `this` at their call sites), pos = its position in UI camera space. Returns true with `out` set to draw it there
    // instead (the off-screen arrows kept clear of the HUD's panels and the subtitles).
    bool (*OnHudPart)(u32 callerEbp, const void* part, const float* pos, float* out);
    // v6: the game has just laid the maneuver bar out (exe::ManeuverLayout, every frame in flight): the logic may
    // rescale it now, before it is drawn (space_narrow 4).
    void (*OnManeuverLayout)();
};

typedef const ImplApi*(__cdecl* GetImplApiFn)();
