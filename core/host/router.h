// FLvibed - the event router: the one owner of the hooks two or more modules need (docs\MODULES_PLAN.md section 2). A module's
// host part registers a Client and gets these events in the fixed order of the table in router.cpp. A hook one module needs
// stays in that module's part and installs only when the module is on.
//
// A Client function guards its own calls into the logic and keeps its module's fault flag and fail-safe: a fault stops that
// module, and the router goes on to the next one. Null = the module takes no part in that event.
#pragma once

namespace router
{
enum Module : int
{
    kController = 0,
    kSubtitles = 1,
    kMovies = 2,
    kModules = 3
};

struct Client
{
    void (*beforeGame)(double dt); // each frame, before the game's own
    void (*afterGame)(double dt);  // each frame, after it
    // A dialog's own loop never returns to the frame update: its frames are served from the cursor pass.
    void (*modalFrame)(double dt);
    bool (*logicOnStack)(); // with modalFrame: a logic call is on the stack, so the module's modal frames wait
    void (*cursorPass)();   // the UI pass's end, before the game draws its cursor
    void (*deviceLost)();
    void (*deviceReset)();
    void (*deviceCreated)(void** vtable);             // a new device: its vtable, for the module's own slots
    void (*comm)(void* player, const u8* record);     // a comm starts playing (exe::Comm)
    void (*maneuverLayout)(u32 bar);                  // the game has laid the maneuver bar out
    void (*roomCharacter)(void* character, bool created, int live); // a base-room character came or went
    void (*underHud)();                        // the UI pass's start, after the 3D scene, before any control
    bool (*rawKey)(int vk, bool down, bool textField); // a physical key; true: the game never sees it
    void (*uiButton)(int id);                  // one of FLvibed's native buttons was clicked
    // The movie event (moviehooks.h): 0 = PlayMovie(hwnd, path), 1 = UpdateMovie, each pass of the movie loop, 2 = StopMovie.
    void (*movie)(int event, void* hwnd, const char* path);
    void (*moviePoll)();                       // each pass, before Movie.dll's
    const char* (*moviePath)(const char* path); // what PlayMovie really plays
};

// The start, from DllMain, before any module's part: the logs, the symbols, the shared hooks, the core's own.
void Start(HINSTANCE self, bool subtitles, bool controller, const char* rows, const bool* moduleRows);
int LogChannel(int module);
bool ModuleOn(int row); // a [Modules] row as resolved (core\modules_rules.h)
// The Autopilot module's host part, while it is on; without it the calls fail closed.
struct ApHost
{
    int (*JumpInCount)();
    bool (*GuardEvade)(void** vtable);
    bool (*SetGunAim)(bool on, const float* world);
};
void SetApHost(const ApHost* host);
int ApJumpInCount();
bool ApGuardEvade(void** vtable);
bool ApSetGunAim(bool on, const float* world);
// Who provides borderless: the Display module's reader, "off" without it.
void SetWindowMode(const char* (*reader)(char* detail, int len));
const char* WindowMode(char* detail, int len);
void Register(int module, const Client* client);

// Whose hook rows these are (hk::Owner), for the `hooks` reply of each debug channel: a module's own, the ones every module
// on shares, and the core's own, listed by the channel the core's plain log lines go to.
constexpr u32 OwnerOf(int module) { return 1u << module; }
u32 SharedOwner();
u32 CoreOwner();

// The services behind the merged hooks.
void* D3DDevice();                       // IDirect3DDevice8 used by the game; null until created
int RoomCharacters(void** out, int max); // live base-room characters, oldest first
void SetCursorHidden(bool on);           // the game's arrow stays hidden through the cursor pass
void SetLoadDraw(void (*draw)());        // called in each load frame, its scene open; null: the game's own screen alone
void* ManeuverLayoutOriginal();          // the hook's "original", null when refused: for the status lines
bool RawKeysWanted();
bool RawKey(int vk, bool down, bool textField);
void ButtonClicked(int id);
void D3dResetReply(char* reply, size_t len);
void MovieEvent(int event, void* hwnd, const char* path);
void MoviePoll();
const char* MoviePath(const char* path);

// Game loads: each loads a fresh Content.dll. The count runs from the start; a subscriber hears the loads after it subscribed
// (a notification from the loader: no logic calls in it).
u32 ContentLoads();
bool OnContentLoaded(void (*fn)(HMODULE content));
} // namespace router
