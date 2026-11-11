// The subtitles module's host part (started by core\flvibed.cpp when the module is on): a client of the event router
// (core\host\router.h) with the hooks only this module needs; forwards events to the subtitles logic (core\loader.h:
// FLvibedLogic.dll, hot-reloadable, in the dev build, linked in in the release build). Hooks chain with other plugins:
// detours follow an existing JMP, vtable/IAT patches call the previous pointer.
#include "common.h"

#include "../api.h"
#include "flexe.h"
#include "hook.h"
#include "loader.h"
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
static int g_implGeneration = 0; // the logic asked for: 0 = not yet (the first movie or frame does)
static bool g_faulted = false;

static void HostLogWrite(const char* line) { LogTo(g_logChannel, line); }

static bool ImplOk() { return g_impl && !g_faulted; }

static void Fault(const char* where, DWORD code)
{
    g_faulted = true;
    Log("FATAL: exception 0x%08X in %s; logic disabled until reload", code, where);
}

// ------------------------------------------------------------------ the logic (core\loader.h)
static void StopLogic()
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
}

static bool StartLogic(const void* table)
{
    const ImplApi* api = static_cast<const ImplApi*>(table);
    if (!api || api->version != kImplApiVersion)
    {
        Log("LoadImpl: incompatible logic");
        return false;
    }
    g_impl = api;
    g_faulted = false;
    __try
    {
        g_impl->Init(&g_host);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Fault("init", GetExceptionCode());
        return false;
    }
    Log("FLvibed " FLV_VERSION ": logic started (%s)", flv_logic_loader()->Name());
    return true;
}

static bool LoadImpl()
{
    ++g_implGeneration;
    return StartLogic(flv_logic_loader()->Open(kLogicSubtitles));
}

// ------------------------------------------------------------------ debug channel
static int g_channel = -1; // the debug channel (core\host\commands.h)

static void PollCommands() { cmd::Poll(g_channel); }

static void PingVerb(int, const char*, char* reply, size_t len) { strcpy_s(reply, len, "pong"); }

// Every other line: the logic's.
static void LogicCommand(int, const char* cmd, char* reply, size_t len)
{
    if (ImplOk())
    {
        reply[0] = 0;
        __try
        {
            g_impl->Command(cmd, reply, static_cast<int>(len));
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            strcpy_s(reply, len, "command faulted");
        }
    }
    else
        strcpy_s(reply, len, "logic DLL not loaded");
}

// ------------------------------------------------------------------ the frame (the router's event)
static void GuardedFrame(double dt)
{
    __try
    {
        if (g_implGeneration == 0)
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

static void BeforeGame(double dt)
{
    if (ShmBlock* s = shm::Get())
        ++s->heartbeat;
    GuardedFrame(dt);
}

// ------------------------------------------------------------------ HUD indicator parts (0x559900)
// The target indicators draw their brackets and off-screen arrows part by part through exe::HudDrawPart (cdecl, 6
// arguments); their `this` is in ebp at those call sites. A naked entry keeps the caller's ebp, then the C++ hook runs
// with the caller's arguments and return address: the logic may move an arrow. The caller's own position vector is
// moved (its stack copy): the arrow's remaining parts and its health / shield bars, built after the parts from the
// same vector, follow it (with a moved copy only, the bars stayed under the HUD); the indicators' mouse
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

// ------------------------------------------------------------------ maneuver bar layout (the router's event)
// The game re-lays the bar out every frame; the logic rescales it right after (space_narrow 4), before drawing.
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

static void OnManeuverLayout(u32) { GuardedManeuverLayout(); }

// ------------------------------------------------------------------ comm receipt (the router's event)
// The record: +0 from, +4 to, +3C voice, +44 msg ids, +84 count.
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

static void OnComm(void*, const u8* rec) { GuardedComm(rec); }

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
            hk::Owner own(router::OwnerOf(router::kSubtitles));
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

// ------------------------------------------------------------------ movies (the router's event: core\host\moviehooks.h)
// The startup movies play before the first frame update, so the logic is loaded here if needed.
static void GuardedMovie(int event, void* hwnd, const char* path)
{
    __try
    {
        if (g_implGeneration == 0)
            LoadImpl();
        if (ImplOk() || (g_impl && event == 2)) // a faulted logic still gets the stop: its teardown
            g_impl->OnMovie(event, hwnd, path);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Fault("movie", GetExceptionCode());
    }
}

// Each pass of the movie loop, before Movie.dll's: the frame update does not run during movies, so the debug channel lives here.
static void MoviePoll()
{
    if (ShmBlock* s = shm::Get())
        ++s->heartbeat;
    __try
    {
        PollCommands();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Fault("movie poll", GetExceptionCode());
    }
}

// ------------------------------------------------------------------ base-room characters (the router's event)
static void OnRoomCharacter(void* character, bool created, int live)
{
    if (created)
        Log("room character created %p (%d live)", character, live);
    else
        Log("room character destroyed %p", character);
}

// ------------------------------------------------------------------ UI pass (the router's cursor pass)
// Freelancer draws its cursor there, last in the frame's UI rendering: the subtitles go under it.
static void GuardedUi()
{
    __try
    {
        if (ImplOk() && router::D3DDevice())
            g_impl->OnRenderUi();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Fault("ui overlay", GetExceptionCode());
    }
}

// ------------------------------------------------------------------ Direct3D 8: Present (the device itself: the router's)
constexpr int kDevPresent = 15; // IDirect3DDevice8::Present

typedef HRESULT(WINAPI* PresentFn)(void* dev, const RECT* src, const RECT* dst, HWND wnd, const void* dirty);
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
    if (dev == router::D3DDevice())
        GuardedPresent();
    return g_origPresent(dev, src, dst, wnd, dirty);
}

// Like every other logic call: a fault stops this module, not the game inside Direct3D's Reset / CreateDevice.
static void DeviceLost()
{
    __try
    {
        if (g_impl) // a faulted logic too: its state blocks would fail the game's Reset under d3d8to9
            g_impl->OnDeviceLost();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Fault("device lost", GetExceptionCode());
    }
}

static void DeviceReset()
{
    __try
    {
        if (ImplOk())
            g_impl->OnDeviceReset();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Fault("device reset", GetExceptionCode());
    }
}

static void DeviceCreated(void** vt)
{
    // (a refused patch returns null: the previous pointer stays for a hook already installed elsewhere)
    if (vt[kDevPresent] != PresentHook)
    {
        hk::Owner own(router::OwnerOf(router::kSubtitles));
        if (void* o = hk::PatchVtable(vt, kDevPresent, PresentHook))
            g_origPresent = static_cast<PresentFn>(o);
    }
    DeviceReset();
}

// ------------------------------------------------------------------ the maneuver bar's place
// The space subtitles' top panel takes the top centre ("move top HUD element (ship flight modes) to the
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

// The comm window under the moved bar ("the maneuver bar renders over NPC portraits when they're
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
void Start(HMODULE)
{
    g_logChannel = router::LogChannel(router::kSubtitles);
    Log("FLSubtitles host starting (build %s %s)", __DATE__, __TIME__);

    hk::Owner own(router::OwnerOf(router::kSubtitles));
    static const LogicClient client = {StopLogic, StartLogic, HostLogWrite};
    flv_logic_loader()->Register(kLogicSubtitles, &client);
    static const router::Client events = {BeforeGame, nullptr,     nullptr,       nullptr,
                                          GuardedUi,  DeviceLost,  DeviceReset,   DeviceCreated,
                                          OnComm,     OnManeuverLayout, OnRoomCharacter,
                                          nullptr,    nullptr,     nullptr,
                                          GuardedMovie, MoviePoll};
    router::Register(router::kSubtitles, &events);

    shm::Init();
    if (ShmBlock* s = shm::Get())
    {
        g_channel = cmd::Open(router::kSubtitles, &s->cmdSeq, &s->cmdAck, s->cmd, sizeof(s->cmd), s->reply, sizeof(s->reply),
                              LogicCommand);
        cmd::Add(g_channel, "ping", false, PingVerb);
        cmd::Add(g_channel, "movieskip", false, moviehooks::SkipVerb);
    }
    g_host.version = kHostApiVersion;
    g_host.dllDir = DllDir().c_str();
    g_host.LogWrite = HostLogWrite;
    g_host.GetD3DDevice = router::D3DDevice;
    g_host.GetRoomCharacters = router::RoomCharacters;
    g_host.GetMovieGraph = moviehooks::Graph;
    g_host.ShiftManeuverBar = HostShiftManeuverBar;
    g_host.ShiftCommWindow = HostShiftCommWindow;
    g_host.GetSettingsBus = flv_settings_bus; // v6: the bus in FLvibed.dll (core\settings_bus.cpp)
    g_host.SetIntroOverride = nullptr;              // v7: the Movies module's since P6
    g_host.GetHudBus = flv_hud_bus;                 // v8: core\hud_bus.cpp
    g_host.GameLoads = router::ContentLoads;        // v9

    // FindVoice: push ecx; mov eax,[esp+8] (5 bytes)
    g_origFindVoice = static_cast<FindVoiceFn>(hk::Detour(exe::FindVoice, FindVoiceHook, 5, "scene and bar subtitles"));
    // HudDrawPart: sub esp,28h; push ebx; mov ebx,[esp+30h] (8 bytes)
    g_origHudDrawPart = static_cast<HudDrawPartFn>(hk::Detour(
        exe::HudDrawPart, reinterpret_cast<void*>(HudDrawPartEntry), 8, "off-screen arrows kept clear of the HUD"));
    // PlayArch: push ebx; mov ebx,[esp+8] (5 bytes)
    g_origPlayArch = static_cast<PlayArchFn>(hk::Detour(exe::PlayArch, PlayArchHook, 5, "voice line subtitles"));
    Log("Game hooks: findvoice=%p", g_origFindVoice);

    moviehooks::Install();
}
