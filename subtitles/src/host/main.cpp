// Subtitles module host, part of FLvibed.dll (started by core/flvibed.cpp when the module is on).
// Owns every hook into the game and forwards events to the subtitles logic: FLSubtitlesImpl.dll
// (hot-reloadable) in the dev build, linked in (FLV_STATIC) in the release build. Hooks chain with the
// other modules and other plugins (the controller hooks some of the same addresses): detours follow an
// existing JMP, vtable/IAT patches call the previous pointer.
#include "common.h"

#include "../api.h"
#include "flexe.h"
#include "hook.h"
#include "log.h"
#include "version.h"
#include "../shm.h"

#include <intrin.h>

// The settings bus: one instance in FLvibed.dll (core\settings_bus.cpp, compiled plain), reached by this C symbol.
extern "C" const SettingsBus* flv_settings_bus();
extern "C" const HudBus* flv_hud_bus();

// ------------------------------------------------------------------ state
static HostApi g_host = {};
static const ImplApi* g_impl = nullptr;
static HMODULE g_implModule = nullptr;
static int g_implGeneration = 0;
static bool g_faulted = false;
static void* g_d3dDevice = nullptr;

static void HostLogWrite(const char* line) { LogWriteLine(line); }
static void* HostGetD3DDevice() { return g_d3dDevice; }

static bool ImplOk() { return g_impl && !g_faulted; }

static void Fault(const char* where, DWORD code)
{
    g_faulted = true;
    Log("FATAL: exception 0x%08X in %s; logic disabled until reload", code, where);
}

// ------------------------------------------------------------------ logic DLL loading
static void UnloadImpl()
{
    if (g_impl)
    {
        __try
        {
            g_impl->Shutdown();
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            Log("impl Shutdown faulted");
        }
    }
    g_impl = nullptr;
    if (g_implModule)
    {
        FreeLibrary(g_implModule);
        g_implModule = nullptr;
    }
}

#ifdef FLV_STATIC
// Release build: the logic is linked into FLvibed.dll (no hot reload; `reload` restarts the same logic).
extern "C" const void* flv_subtitles_impl_api();

static bool LoadImpl()
{
    ++g_implGeneration;
    const ImplApi* api = static_cast<const ImplApi*>(flv_subtitles_impl_api());
    if (!api || api->version != kImplApiVersion)
    {
        Log("LoadImpl: incompatible built-in logic");
        return false;
    }
    g_impl = api;
    g_faulted = false;
    g_impl->Init(&g_host);
    Log("FLvibed " FLV_VERSION ": logic started (built in)");
    return true;
}
#else
static bool LoadImpl()
{
    // Load from a copy so the build can overwrite FLSubtitlesImpl.dll while the game runs.
    const std::string src = DllDir() + "FLSubtitlesImpl.dll";
    char copyName[64];
    sprintf_s(copyName, "FLSubtitlesImpl.run%d.dll", g_implGeneration++ % 2);
    const std::string copy = DllDir() + copyName;
    if (!CopyFileA(src.c_str(), copy.c_str(), FALSE))
    {
        Log("LoadImpl: cannot copy %s (%lu)", src.c_str(), GetLastError());
        return false;
    }
    g_implModule = LoadLibraryA(copy.c_str());
    if (!g_implModule)
    {
        Log("LoadImpl: LoadLibrary failed (%lu)", GetLastError());
        return false;
    }
    auto get = reinterpret_cast<GetImplApiFn>(GetProcAddress(g_implModule, "GetImplApi"));
    const ImplApi* api = get ? get() : nullptr;
    if (!api || api->version != kImplApiVersion)
    {
        Log("LoadImpl: incompatible logic DLL");
        FreeLibrary(g_implModule);
        g_implModule = nullptr;
        return false;
    }
    g_impl = api;
    g_faulted = false;
    g_impl->Init(&g_host);
    Log("Logic DLL loaded (%s)", copyName);
    return true;
}
#endif

// ------------------------------------------------------------------ debug channel
static bool g_moviePlaying = false; // a movie runs (PlayMovie .. UpdateMovie false / StopMovie)
static bool g_skipMovie = false;    // `movieskip`: the next UpdateMovie reports the movie finished

static void PollCommands()
{
    ShmBlock* s = shm::Get();
    if (!s || s->cmdSeq == s->cmdAck)
        return;
    char cmd[sizeof(s->cmd)];
    memcpy(cmd, s->cmd, sizeof(cmd));
    cmd[sizeof(cmd) - 1] = 0;

    if (strcmp(cmd, "reload") == 0)
    {
        UnloadImpl();
        strcpy_s(s->reply, LoadImpl() ? "reloaded" : "reload FAILED (see log)");
    }
    else if (strcmp(cmd, "unload") == 0)
    {
        UnloadImpl();
        strcpy_s(s->reply, "unloaded");
    }
    else if (strcmp(cmd, "ping") == 0)
        strcpy_s(s->reply, "pong");
    else if (strcmp(cmd, "hooks") == 0) // this host's detours: mode, place, the function's own jump now
        hk::HookReport(s->reply, sizeof(s->reply));
    else if (strcmp(cmd, "movieskip") == 0) // end the playing movie the way the game ends a finished one
    {
        g_skipMovie = g_moviePlaying;
        strcpy_s(s->reply, g_moviePlaying ? "skipping movie" : "no movie playing");
    }
    else if (ImplOk())
    {
        s->reply[0] = 0;
        __try
        {
            g_impl->Command(cmd, s->reply, sizeof(s->reply));
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            strcpy_s(s->reply, "command faulted");
        }
    }
    else
        strcpy_s(s->reply, "logic DLL not loaded");
    MemoryBarrier();
    s->cmdAck = s->cmdSeq;
}

// ------------------------------------------------------------------ frame update (0x5B2890)
typedef void(__cdecl* FrameUpdateFn)(double dt);
static FrameUpdateFn g_origFrameUpdate = nullptr;

static void GuardedFrame(double dt)
{
    __try
    {
        if (!g_impl && !g_implModule && g_implGeneration == 0)
            LoadImpl();
        PollCommands();
        if (ImplOk())
            g_impl->OnFrame(dt);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Fault("frame", GetExceptionCode());
    }
}

static void __cdecl FrameUpdateHook(double dt)
{
    if (ShmBlock* s = shm::Get())
        ++s->heartbeat;
    GuardedFrame(dt);
    g_origFrameUpdate(dt);
    // every DLL is loaded at the first frame: FLUF loaded after FLvibed moves FLvibed's hooks to its handlers; again
    // when a hook saw FLUF as its caller (a FLUF hook made later)
    static bool adopted = false;
    if (!adopted || hk::AdoptPending())
    {
        adopted = true;
        hk::AdoptRestoreStyle("subtitles host");
    }
}

// ------------------------------------------------------------------ HUD indicator parts (0x559900)
// The target indicators draw their brackets and off-screen arrows part by part through exe::HudDrawPart (cdecl, 6
// arguments); their `this` is in ebp at those call sites. A naked entry keeps the caller's ebp, then the C++ hook runs
// with the caller's arguments and return address: the logic may move an arrow. The caller's own position vector is
// moved (its stack copy): the arrow's remaining parts and its health / shield bars, built after the parts from the
// same vector, follow it (with a moved copy only, the bars stayed under the HUD, 21 Oct 2026); the indicators' mouse
// test reads the vector before the parts are drawn.
typedef bool(__cdecl* HudDrawPartFn)(const void* part, const float* pos, const void* rot, float scale, int a5,
                                     float a6);
static HudDrawPartFn g_origHudDrawPart = nullptr;
static u32 g_hudPartEbp = 0;

static bool GuardedHudPart(const void* part, const float* pos, float* out)
{
    __try
    {
        return ImplOk() && g_impl->OnHudPart && g_impl->OnHudPart(g_hudPartEbp, part, pos, out);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Fault("hud part", GetExceptionCode());
        return false;
    }
}

static void WriteVector(const float* to, const float* from)
{
    __try
    {
        memcpy(const_cast<float*>(to), from, 3 * sizeof(float));
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

static bool __cdecl HudDrawPartHook(const void* part, const float* pos, const void* rot, float scale, int a5, float a6)
{
    float moved[3];
    if (pos && GuardedHudPart(part, pos, moved))
        WriteVector(pos, moved);
    return g_origHudDrawPart(part, pos, rot, scale, a5, a6);
}

__declspec(naked) static void HudDrawPartEntry()
{
    __asm {
        mov g_hudPartEbp, ebp
        jmp HudDrawPartHook
    }
}

// ------------------------------------------------------------------ maneuver bar layout (0x4D88F0)
// The game re-lays the bar out every frame; the logic rescales it right after (space_narrow 4), before drawing.
typedef void(__fastcall* ManeuverLayoutFn)(void* self, void* edx);
static ManeuverLayoutFn g_origManeuverLayout = nullptr;

static void GuardedManeuverLayout()
{
    __try
    {
        if (ImplOk() && g_impl->OnManeuverLayout)
            g_impl->OnManeuverLayout();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Fault("maneuver layout", GetExceptionCode());
    }
}

static void __fastcall ManeuverLayoutHook(void* self, void* edx)
{
    g_origManeuverLayout(self, edx);
    GuardedManeuverLayout();
}

// ------------------------------------------------------------------ comm receipt (0x4CE000)
// thiscall(comm player, ?, record) ret 8 (the body reads the record from [esp+44h] = the 2nd argument). Record: +0 from, +4 to, +3C voice, +44 msg ids, +84 count.
typedef void(__fastcall* CommFn)(void* self, void* edx, u32 a1, const u8* rec);
static CommFn g_origComm = nullptr;

static void GuardedComm(const u8* rec)
{
    __try
    {
        if (!ImplOk())
            return;
        CommInfo c = {};
        c.from = *reinterpret_cast<const u32*>(rec + 0x00);
        c.to = *reinterpret_cast<const u32*>(rec + 0x04);
        c.voice = *reinterpret_cast<const u32*>(rec + 0x3C);
        c.count = *reinterpret_cast<const int*>(rec + 0x84);
        if (c.count < 0)
            c.count = 0;
        if (c.count > 16)
            c.count = 16;
        memcpy(c.lines, rec + 0x44, sizeof(u32) * c.count);
        c.record = rec;
        g_impl->OnComm(c);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Fault("comm", GetExceptionCode());
    }
}

static void __fastcall CommHook(void* self, void* edx, u32 a1, const u8* rec)
{
    GuardedComm(rec);
    g_origComm(self, edx, a1, rec);
}

// ------------------------------------------------------------------ voices (FindVoice 0x430AB0, Voice vt[5])
// FindVoice(voice id) returns a Voice object; its vt[5] (+0x14) is GetSound(msg id). Every Voice vtable
// seen gets slot 5 patched once, so every line lookup (with its caller) reaches the logic DLL.
typedef void*(__cdecl* FindVoiceFn)(u32 id);
typedef void*(__fastcall* GetSoundFn)(void* self, void* edx, u32 msg);
static FindVoiceFn g_origFindVoice = nullptr;
struct PatchedVt
{
    void** vt;
    GetSoundFn orig;
};
static PatchedVt g_voiceVts[16];
static int g_voiceVtCount = 0;
static u32 g_lastVoiceId = 0;

static void* __fastcall GetSoundHook(void* self, void* edx, u32 msg)
{
    void** vt = *static_cast<void***>(self);
    GetSoundFn orig = nullptr;
    for (int i = 0; i < g_voiceVtCount; ++i)
        if (g_voiceVts[i].vt == vt)
            orig = g_voiceVts[i].orig;
    if (!orig)
        return nullptr;
    void* sound = orig(self, edx, msg);
    const u32 caller = reinterpret_cast<u32>(_ReturnAddress());
    __try
    {
        if (ImplOk())
            g_impl->OnVoiceLookup(g_lastVoiceId, msg, sound, caller);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Fault("voice lookup", GetExceptionCode());
    }
    return sound;
}

static void* __cdecl FindVoiceHook(u32 id)
{
    void* voice = g_origFindVoice(id);
    g_lastVoiceId = id;
    if (voice)
    {
        void** vt = *static_cast<void***>(voice);
        bool known = false;
        for (int i = 0; i < g_voiceVtCount; ++i)
            known |= g_voiceVts[i].vt == vt;
        if (!known && g_voiceVtCount < 16 && vt[5] != reinterpret_cast<void*>(GetSoundHook))
        {
            g_voiceVts[g_voiceVtCount].vt = vt;
            g_voiceVts[g_voiceVtCount].orig =
                static_cast<GetSoundFn>(hk::PatchVtable(vt, 5, reinterpret_cast<void*>(GetSoundHook)));
            ++g_voiceVtCount;
            Log("Voice vtable %p patched (slot 5, previous %p)", vt, g_voiceVts[g_voiceVtCount - 1].orig);
        }
    }
    return voice;
}

// ------------------------------------------------------------------ sound start (0x428680)
// u8 __cdecl PlayArchetype(SoundArchetype*, flags*, handle* out, int): every sound archetype started by
// the game (THN SOUND entities in cutscenes and room scenes: story_sounds.ini, scene mixes; effects).
// The archetype keeps its nickname id at +4 (loader 0x42B030 / constructor 0x42B7B0).
typedef u8(__cdecl* PlayArchFn)(const u8* arch, void* a2, void* a3, int a4);
static PlayArchFn g_origPlayArch = nullptr;

static void GuardedSoundStart(const u8* arch, u32 caller)
{
    __try
    {
        if (ImplOk() && arch)
            g_impl->OnSoundStart(*reinterpret_cast<const u32*>(arch + 4), arch, caller);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Fault("sound start", GetExceptionCode());
    }
}

static u8 __cdecl PlayArchHook(const u8* arch, void* a2, void* a3, int a4)
{
    const u8 r = g_origPlayArch(arch, a2, a3, a4);
    hk::NoteCaller(_ReturnAddress()); // FLUF wrapped this after FLvibed: the hook moves to its handler next frame
    if (r)
        GuardedSoundStart(arch, reinterpret_cast<u32>(_ReturnAddress()));
    return r;
}

// ------------------------------------------------------------------ movies (Movie.dll imports of Freelancer.exe)
// MediaPlayer::PlayMovie(HWND, bool, const char* path) returns long; UpdateMovie() returns bool (still playing),
// StopMovie(). The startup movies play before the first frame update, so the logic DLL is loaded here if needed.
typedef long(__cdecl* PlayMovieFn)(HWND, bool, const char*);
typedef bool(__cdecl* UpdateMovieFn)();
typedef void(__cdecl* StopMovieFn)();
static PlayMovieFn g_origPlayMovie = nullptr;
static UpdateMovieFn g_origUpdateMovie = nullptr;
static StopMovieFn g_origStopMovie = nullptr;

static void GuardedMovie(int event, void* hwnd, const char* path)
{
    __try
    {
        if (!g_impl && !g_implModule && g_implGeneration == 0)
            LoadImpl();
        if (ImplOk())
            g_impl->OnMovie(event, hwnd, path);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Fault("movie", GetExceptionCode());
    }
}

// v7: the intro override (the movie_intro row): the startup movie whose file name contains "intro" plays another file.
static char g_introOverride[MAX_PATH] = {};
static void HostSetIntroOverride(const char* path)
{
    if (path && *path)
        strncpy_s(g_introOverride, path, _TRUNCATE);
    else
        g_introOverride[0] = 0;
    Log("intro override: %s", g_introOverride[0] ? g_introOverride : "as installed");
}

static bool IsIntroMovie(const char* path)
{
    if (!path)
        return false;
    const char* name = strrchr(path, '\\');
    name = name ? name + 1 : path;
    for (const char* p = name; *p; ++p)
        if (_strnicmp(p, "intro", 5) == 0)
            return true;
    return false;
}

static long __cdecl PlayMovieHook(HWND hwnd, bool b, const char* path)
{
    if (g_introOverride[0] && IsIntroMovie(path))
    {
        if (GetFileAttributesA(g_introOverride) != INVALID_FILE_ATTRIBUTES)
        {
            Log("PlayMovie(%s) -> the intro override %s", path, g_introOverride);
            path = g_introOverride;
        }
        else
            Log("intro override %s does not exist: %s plays as installed", g_introOverride, path);
    }
    const long r = g_origPlayMovie(hwnd, b, path);
    g_moviePlaying = r >= 0;
    Log("PlayMovie(%s) -> %ld", path ? path : "", r);
    GuardedMovie(0, hwnd, path);
    return r;
}

// Movie.dll's CoCreateInstance: keep the DirectShow filter graph (CLSID_FilterGraph) so the logic DLL can set
// the video's destination rectangle (fit to screen).
typedef HRESULT(WINAPI* CoCreateFn)(REFCLSID, void*, DWORD, REFIID, void**);
static CoCreateFn g_origCoCreate = nullptr;
static void* g_movieGraph = nullptr;
static const GUID kClsidFilterGraph = {0xE436EBB3, 0x524F, 0x11CE, {0x9F, 0x53, 0x00, 0x20, 0xAF, 0x0B, 0xA7, 0x70}};

static HRESULT WINAPI CoCreateHook(REFCLSID clsid, void* outer, DWORD ctx, REFIID iid, void** out)
{
    const HRESULT hr = g_origCoCreate(clsid, outer, ctx, iid, out);
    if (SUCCEEDED(hr) && out && *out && IsEqualGUID(clsid, kClsidFilterGraph))
        g_movieGraph = *out;
    return hr;
}

static void* HostGetMovieGraph() { return g_movieGraph; }

static void GuardedPoll()
{
    __try
    {
        PollCommands(); // the frame update does not run during movies: keep the debug channel alive
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Fault("movie poll", GetExceptionCode());
    }
}

static bool __cdecl UpdateMovieHook()
{
    if (ShmBlock* s = shm::Get())
        ++s->heartbeat;
    GuardedPoll();
    // `movieskip`: report the movie as finished; the game's movie loop then ends it on its own path (no key input,
    // works out of focus).
    const bool playing = g_skipMovie ? false : g_origUpdateMovie();
    g_skipMovie = false;
    g_moviePlaying = playing;
    GuardedMovie(playing ? 1 : 2, nullptr, nullptr);
    return playing;
}

static void __cdecl StopMovieHook()
{
    g_moviePlaying = false;
    GuardedMovie(2, nullptr, nullptr);
    g_origStopMovie();
    g_movieGraph = nullptr;
}

// ------------------------------------------------------------------ base-room characters (vtable 0x5CB930)
// Constructor 0x443ED0 thiscall(this, a1, a2) ret 8, destructor 0x4440C0 thiscall(this). Both start with
// an SEH frame: push -1; push 5B97C8h (7 bytes, no relative addressing).
typedef void*(__fastcall* CharCtorFn)(void* self, void* edx, void* a1, void* a2);
typedef void(__fastcall* CharDtorFn)(void* self, void* edx);
static CharCtorFn g_origCharCtor = nullptr;
static CharDtorFn g_origCharDtor = nullptr;
static void* g_chars[128];
static int g_charCount = 0;

static void* __fastcall CharCtorHook(void* self, void* edx, void* a1, void* a2)
{
    void* r = g_origCharCtor(self, edx, a1, a2);
    Log("room character created %p (%d live)", self, g_charCount + 1);
    if (g_charCount < 128)
        g_chars[g_charCount++] = self;
    return r;
}

static void __fastcall CharDtorHook(void* self, void* edx)
{
    Log("room character destroyed %p", self);
    for (int i = 0; i < g_charCount; ++i)
        if (g_chars[i] == self)
        {
            memmove(&g_chars[i], &g_chars[i + 1], sizeof(void*) * (g_charCount - i - 1));
            --g_charCount;
            break;
        }
    g_origCharDtor(self, edx);
}

static int HostGetRoomCharacters(void** out, int max)
{
    const int n = std::min(max, g_charCount);
    memcpy(out, g_chars, sizeof(void*) * n);
    return n;
}

// ------------------------------------------------------------------ name hashing trace (Common.dll CreateID)
// RE aid: logs which code hashes dialogue names (story sounds are looked up by CreateID(nickname)).
typedef u32(__cdecl* CreateIDFn)(const char* s);
static CreateIDFn g_origCreateID = nullptr;

static u32 __cdecl CreateIDHook(const char* s)
{
    const u32 id = g_origCreateID(s);
    __try
    {
        if (s && (s[0] == 'd' || s[0] == 'D') && (s[1] == 'x' || s[1] == 'X') && s[2] == '_' ||
            s && _strnicmp(s, "mix_", 4) == 0)
            Log("CreateID(%s) = %08X caller=%08X", s, id, reinterpret_cast<u32>(_ReturnAddress()));
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    return id;
}

// ------------------------------------------------------------------ UI pass (0x41F150: mouse poll + cursor draw)
typedef void(__cdecl* CursorUpdateFn)();
static CursorUpdateFn g_origCursorUpdate = nullptr;

static void GuardedUi()
{
    __try
    {
        if (ImplOk() && g_d3dDevice)
            g_impl->OnRenderUi();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Fault("ui overlay", GetExceptionCode());
    }
}

// Freelancer draws its cursor here, last in the frame's UI rendering: the subtitles go under it.
static void __cdecl CursorUpdateHook()
{
    GuardedUi();
    g_origCursorUpdate();
}

// ------------------------------------------------------------------ Direct3D 8
namespace d3d8idx
{
constexpr int D3D_CreateDevice = 15;
constexpr int Dev_Reset = 14;
constexpr int Dev_Present = 15;
} // namespace d3d8idx

typedef void*(WINAPI* Direct3DCreate8Fn)(UINT sdk);
typedef HRESULT(WINAPI* CreateDeviceFn)(void* d3d, UINT adapter, DWORD type, HWND wnd, DWORD flags, void* pp,
                                        void** outDevice);
typedef HRESULT(WINAPI* ResetFn)(void* dev, void* pp);
typedef HRESULT(WINAPI* PresentFn)(void* dev, const RECT* src, const RECT* dst, HWND wnd, const void* dirty);
static Direct3DCreate8Fn g_origCreate8 = nullptr;
static CreateDeviceFn g_origCreateDevice = nullptr;
static ResetFn g_origReset = nullptr;
static PresentFn g_origPresent = nullptr;

static void GuardedPresent()
{
    __try
    {
        if (ImplOk())
            g_impl->OnPresent();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Fault("present", GetExceptionCode());
    }
}

static HRESULT WINAPI PresentHook(void* dev, const RECT* src, const RECT* dst, HWND wnd, const void* dirty)
{
    if (dev == g_d3dDevice)
        GuardedPresent();
    return g_origPresent(dev, src, dst, wnd, dirty);
}

static HRESULT WINAPI ResetHook(void* dev, void* pp)
{
    if (dev == g_d3dDevice && ImplOk())
        g_impl->OnDeviceLost();
    HRESULT hr = g_origReset(dev, pp);
    Log("D3D8 device Reset -> 0x%08X", hr);
    if (dev == g_d3dDevice && ImplOk() && SUCCEEDED(hr))
        g_impl->OnDeviceReset();
    return hr;
}

static HRESULT WINAPI CreateDeviceHook(void* d3d, UINT adapter, DWORD type, HWND wnd, DWORD flags, void* pp,
                                       void** outDevice)
{
    HRESULT hr = g_origCreateDevice(d3d, adapter, type, wnd, flags, pp, outDevice);
    Log("D3D8 CreateDevice -> 0x%08X device=%p", hr, outDevice ? *outDevice : nullptr);
    if (SUCCEEDED(hr) && outDevice && *outDevice)
    {
        if (g_d3dDevice && ImplOk())
            g_impl->OnDeviceLost();
        g_d3dDevice = *outDevice;
        void** vt = *static_cast<void***>(g_d3dDevice);
        if (vt[d3d8idx::Dev_Reset] != ResetHook)
            g_origReset = static_cast<ResetFn>(hk::PatchVtable(vt, d3d8idx::Dev_Reset, ResetHook));
        if (vt[d3d8idx::Dev_Present] != PresentHook)
            g_origPresent = static_cast<PresentFn>(hk::PatchVtable(vt, d3d8idx::Dev_Present, PresentHook));
        if (ImplOk())
            g_impl->OnDeviceReset();
    }
    return hr;
}

static void* WINAPI Direct3DCreate8Hook(UINT sdk)
{
    void* d3d = g_origCreate8(sdk);
    Log("Direct3DCreate8(%u) -> %p", sdk, d3d);
    if (d3d)
    {
        void** vt = *static_cast<void***>(d3d);
        if (vt[d3d8idx::D3D_CreateDevice] != CreateDeviceHook)
            g_origCreateDevice =
                static_cast<CreateDeviceFn>(hk::PatchVtable(vt, d3d8idx::D3D_CreateDevice, CreateDeviceHook));
    }
    return d3d;
}

// ------------------------------------------------------------------ the maneuver bar's place
// The space subtitles' top panel takes the top centre (user, 21 Oct 2026: "move top HUD element (ship flight modes) to the
// top left"). The bar is placed where HudShift places HUD items: three float constants the game reads when it builds the
// HUD (core\flexe.h "HUD layout constants"), shifted as HudShift shifts its ManeuverBar group (x of the frame and of the
// buttons scale 0.5 - they sit at depth 0.5 - the region 1). The host owns it (never reloaded): each value's base is the
// value found before our first write, or whatever someone else wrote since (HudShift rewrites them on a resolution
// change), so a shift is never applied twice.
struct ShiftedFloat
{
    float* p = nullptr;
    float scale = 1, base = 0, written = 0;
    bool have = false;
};
static ShiftedFloat g_mbFrame, g_mbButtons, g_mbRegion;

static bool ApplyShift(ShiftedFloat& v, float* p, float scale, float dx)
{
    if (!p)
        return false;
    __try
    {
        if (v.p != p)
            v = ShiftedFloat(), v.p = p;
        v.scale = scale;
        const float cur = *p;
        if (!v.have || cur != v.written)
            v.base = cur, v.have = true;
        const float want = v.base + dx * scale;
        if (cur != want && !hk::Write(reinterpret_cast<uintptr_t>(p), &want, sizeof(want)))
            return false;
        v.written = want;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool HostShiftManeuverBar(float dx)
{
    static bool logged = false;
    auto refuse = [&](const char* why)
    {
        if (!logged)
            Log("maneuver bar shift refused: %s", why), logged = true;
        return false;
    };
    if (!exe::ManeuverFrameX.ok() || !exe::ManeuverRegionX.ok() || !exe::ManeuverBarXRef.ok())
        return refuse("a position constant did not resolve (core\\flexe.h HUD layout constants)");
    float* buttons = Mem<float*>(exe::ManeuverBarXRef);
    // Only HudShift's relocated float may move: the vanilla operand is a read-only constant other code shares. The
    // relocated one lives in the code section's filler (executable memory).
    MEMORY_BASIC_INFORMATION mi = {};
    if (!buttons || !VirtualQuery(buttons, &mi, sizeof(mi)) ||
        !(mi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)))
        return refuse("the buttons' x is the shared vanilla constant (no HudShift relocation)");
    const bool ok = ApplyShift(g_mbFrame, reinterpret_cast<float*>(exe::ManeuverFrameX.get()), 0.5f, dx) &&
                    ApplyShift(g_mbButtons, buttons, 0.5f, dx) &&
                    ApplyShift(g_mbRegion, reinterpret_cast<float*>(exe::ManeuverRegionX.get()), 1.0f, dx);
    static float lastDx = -999;
    if (ok && dx != lastDx)
        Log("maneuver bar shift %.4f (frame %.4f, buttons %.4f, region %.4f)", dx, g_mbFrame.written,
            g_mbButtons.written, g_mbRegion.written),
            lastDx = dx;
    return ok;
}

// The comm window under the moved bar (user, 21 Oct 2026: "the maneuver bar renders over NPC portraits when they're
// talking to you ... those portraits need to be pushed down, along with their accompanying text line"): HudShift's
// group Comm (CommText scale 1, the window 2.392 - measured 2.135 -, the generic icon 0.362), shifted vertically like
// the bar. The game reads them when it opens a comm window: a change shows with the next comm.
static ShiftedFloat g_commText, g_commWindow, g_commIcon;
static bool HostShiftCommWindow(float dy)
{
    if (!exe::CommTextY.ok() || !exe::CommWindowY.ok() || !exe::CommIconY.ok())
    {
        static bool logged = false;
        if (!logged)
            Log("comm window shift refused: a position constant did not resolve (core\\flexe.h HUD layout constants)"),
                logged = true;
        return false;
    }
    const bool ok = ApplyShift(g_commText, reinterpret_cast<float*>(exe::CommTextY.get()), 1.0f, dy) &&
                    // HudShift's window scale 2.392 moved the frame 12 % farther than its text (158 vs 141 px at 4K,
                    // 21 Oct 2026: the name lines then touched the frame); 2.135 moves them together
                    ApplyShift(g_commWindow, reinterpret_cast<float*>(exe::CommWindowY.get()), 2.135f, dy) &&
                    ApplyShift(g_commIcon, reinterpret_cast<float*>(exe::CommIconY.get()), 0.362f, dy);
    static float lastDy = -999;
    if (ok && dy != lastDy)
        Log("comm window shift %.4f (text %.4f, window %.4f, icon %.4f)", dy, g_commText.written, g_commWindow.written,
            g_commIcon.written),
            lastDy = dy;
    return ok;
}

// ------------------------------------------------------------------ init
static void Init(HMODULE self)
{
    LogInit(self, "FLSubtitles.log");
    Log("FLSubtitles host starting (build %s %s)", __DATE__, __TIME__);

    // Every Freelancer.exe address is verified by its signature; a hook whose code is not what we expect is not
    // installed (its feature stays off, logged), the rest works.
    exe::ResolveAll("subtitles host");
    hk::LogRestoreStyleAtStart("subtitles host");

    shm::Init();
    g_host.version = kHostApiVersion;
    g_host.dllDir = DllDir().c_str();
    g_host.LogWrite = HostLogWrite;
    g_host.GetD3DDevice = HostGetD3DDevice;
    g_host.GetRoomCharacters = HostGetRoomCharacters;
    g_host.GetMovieGraph = HostGetMovieGraph;
    g_host.ShiftManeuverBar = HostShiftManeuverBar;
    g_host.ShiftCommWindow = HostShiftCommWindow;
    g_host.GetSettingsBus = flv_settings_bus; // v6: the bus in FLvibed.dll (core\settings_bus.cpp)
    g_host.SetIntroOverride = HostSetIntroOverride; // v7
    g_host.GetHudBus = flv_hud_bus;                 // v8: core\hud_bus.cpp

    // FrameUpdate: push ecx; push ebp; push esi; xor ebp,ebp (5 bytes)
    g_origFrameUpdate =
        static_cast<FrameUpdateFn>(hk::Detour(exe::FrameUpdate, FrameUpdateHook, 5, "the whole subtitles logic"));
    // Comm: sub esp,34h; push ebx; push ebp (5 bytes)
    g_origComm = static_cast<CommFn>(hk::Detour(exe::Comm, CommHook, 5, "comm subtitles"));
    // FindVoice: push ecx; mov eax,[esp+8] (5 bytes)
    g_origFindVoice = static_cast<FindVoiceFn>(hk::Detour(exe::FindVoice, FindVoiceHook, 5, "scene and bar subtitles"));
    // CursorUpdate: mov eax, fs:[0] (6 bytes)
    g_origCursorUpdate =
        static_cast<CursorUpdateFn>(hk::Detour(exe::CursorUpdate, CursorUpdateHook, 6, "subtitle drawing"));
    // ManeuverLayout: sub esp,34h; push ebp; push esi; mov ebp,ecx (7 bytes)
    g_origManeuverLayout =
        static_cast<ManeuverLayoutFn>(hk::Detour(exe::ManeuverLayout, reinterpret_cast<void*>(ManeuverLayoutHook), 7,
                                                 "the scaled maneuver bar (narrow screens)"));
    // HudDrawPart: sub esp,28h; push ebx; mov ebx,[esp+30h] (8 bytes)
    g_origHudDrawPart = static_cast<HudDrawPartFn>(hk::Detour(
        exe::HudDrawPart, reinterpret_cast<void*>(HudDrawPartEntry), 8, "off-screen arrows kept clear of the HUD"));
    // PlayArch: push ebx; mov ebx,[esp+8] (5 bytes)
    g_origPlayArch = static_cast<PlayArchFn>(hk::Detour(exe::PlayArch, PlayArchHook, 5, "voice line subtitles"));
    // CharCtor / CharDtor: push -1; push <SEH handler> (7 bytes). Both or neither (the list must stay consistent).
    if (exe::CharCtor.ok() && exe::CharDtor.ok())
    {
        g_origCharCtor = static_cast<CharCtorFn>(hk::Detour(exe::CharCtor, CharCtorHook, 7, "room characters"));
        g_origCharDtor = static_cast<CharDtorFn>(hk::Detour(exe::CharDtor, CharDtorHook, 7, "room characters"));
    }
    else
        Log("Hooks CharCtor/CharDtor NOT installed: signature mismatch - room characters off");
    Log("Game hooks: frame=%p comm=%p findvoice=%p cursor=%p", g_origFrameUpdate, g_origComm, g_origFindVoice,
        g_origCursorUpdate);

    HMODULE exe = GetModuleHandleA(nullptr);
    g_origPlayMovie = static_cast<PlayMovieFn>(hk::PatchImport(
        exe, "movie.dll", "?PlayMovie@MediaPlayer@@YAJPAUHWND__@@_NPBD@Z", reinterpret_cast<void*>(PlayMovieHook)));
    g_origUpdateMovie = static_cast<UpdateMovieFn>(hk::PatchImport(exe, "movie.dll", "?UpdateMovie@MediaPlayer@@YA_NXZ",
                                                                   reinterpret_cast<void*>(UpdateMovieHook)));
    g_origStopMovie = static_cast<StopMovieFn>(
        hk::PatchImport(exe, "movie.dll", "?StopMovie@MediaPlayer@@YAXXZ", reinterpret_cast<void*>(StopMovieHook)));
    g_origCoCreate = static_cast<CoCreateFn>(hk::PatchImport(
        GetModuleHandleA("movie.dll"), "ole32.dll", "CoCreateInstance", reinterpret_cast<void*>(CoCreateHook)));
    Log("Movie hooks: play=%p update=%p stop=%p", g_origPlayMovie, g_origUpdateMovie, g_origStopMovie);

    g_origCreateID = static_cast<CreateIDFn>(hk::PatchImport(
        GetModuleHandleA(nullptr), "common.dll", "?CreateID@@YAIPBD@Z", reinterpret_cast<void*>(CreateIDHook)));
    Log("CreateID trace %s", g_origCreateID ? "installed" : "NOT FOUND");

    // Direct3D: Freelancer's render pipeline (RP8.dll) creates the D3D8 object after plugins load.
    HMODULE rp8 = GetModuleHandleA("rp8.dll");
    g_origCreate8 = static_cast<Direct3DCreate8Fn>(
        hk::PatchImport(rp8, "d3d8.dll", "Direct3DCreate8", reinterpret_cast<void*>(Direct3DCreate8Hook)));
    Log("Direct3DCreate8 import hook %s (rp8=%p)", g_origCreate8 ? "installed" : "NOT FOUND", rp8);
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
        Init(instance); // thread notifications: decided in core\flvibed.cpp
    return TRUE;
}
