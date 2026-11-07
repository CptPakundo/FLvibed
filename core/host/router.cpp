// FLvibed - the event router and the hooks the modules share (router.h).
#include "all.h"
#include "modules_rules.h"

namespace router
{
static const Client* g_clients[kModules] = {};
static bool g_on[kModules] = {};
static int g_logChannels[kModules] = {-1, -1, -1};
static bool g_mouseHere = false; // a background test start without the controller: the virtual mouse is served from here

// ------------------------------------------------------------------ the table
// The order of every event, traced from the two hosts as they chained (docs\MODULES_PLAN.md): the subtitles' detours went in
// first and each later detour chains in front, so at every shared site the controller ran outermost - first in whatever
// comes before the game's own call, last in whatever comes after it.
static constexpr int kFrameOrder[] = {kController, kSubtitles};
static constexpr int kAfterGameOrder[] = {kSubtitles, kController};
static constexpr int kModalOrder[] = {kController}; // the subtitles' frame never ran in a dialog's own loop
static constexpr int kDeviceLostOrder[] = {kController, kSubtitles};
static constexpr int kDeviceResetOrder[] = {kSubtitles, kController};
static constexpr int kCommOrder[] = {kController, kSubtitles};
static constexpr int kManeuverOrder[] = {kSubtitles, kController};
static constexpr int kCharacterOrder[] = {kSubtitles};
static constexpr int kMovieOrder[] = {kMovies, kSubtitles, kController}; // the picture first, as one module did it: the lines draw over it; the pad's skip
// The cursor pass (CursorUpdateHook): the controller's overlay, then under the cursor hide the subtitles, then the game: the
// subtitles draw over the controller's overlay, under the cursor.

template <size_t N, class F> static void Each(const int (&order)[N], F call)
{
    for (int m : order)
        if (const Client* c = g_clients[m])
            call(*c);
}

// A line every module's log had while each host hooked the same thing.
static void SayAll(const char* fmt, ...)
{
    char line[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    line[sizeof(line) - 1] = 0;
    for (int channel : g_logChannels)
        if (channel >= 0)
            LogTo(channel, line);
}

// ------------------------------------------------------------------ the frame
typedef void(__cdecl* FrameUpdateFn)(double dt);
static FrameUpdateFn g_origFrameUpdate = nullptr;

// Modal loops: some of the game's dialogs run their own loop (Freelancer.exe 0x4740F0 and its siblings around
// 0x473A00..0x474600: Sleep(10), the message pump, the UI update and render - 0x473DC4 UiRender, 0x473F35 CursorUpdate)
// and never return to the frame update until closed: the Multiplayer button's "LAN / INTERNET" prompt (verified 28 Oct
// 2026: the pad and the debug channel went silent there), the connection dialogs. A second CursorUpdate without a
// frame update in between is such a loop: the frame is served from there (commands, the virtual mouse, the logic's
// OnFrame) - only while no logic call is on the stack (a click from the logic is deferred to the frame's end, so this
// holds for the front end's buttons).
static u32 g_frameSerial = 0, g_cursorSerial = 0;
static bool g_modal = false;
static u32 g_modalFrames = 0;

static void ModalFrame()
{
    static LARGE_INTEGER freq{}, last{};
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    if (!freq.QuadPart)
        QueryPerformanceFrequency(&freq);
    const double dt = g_modal && last.QuadPart ? double(now.QuadPart - last.QuadPart) / double(freq.QuadPart) : 0.016;
    last = now;
    if (!g_modal)
    {
        g_modal = true;
        g_modalFrames = 0;
        bool waits = false;
        Each(kModalOrder, [&](const Client& c) { waits = waits || (c.logicOnStack && c.logicOnStack()); });
        Log("modal loop: the game runs a dialog's own loop - frames served from its render%s",
            waits ? " (a logic call is on the stack: commands and logic wait)" : "");
    }
    ++g_modalFrames;
    Each(kModalOrder, [&](const Client& c) {
        if (c.modalFrame)
            c.modalFrame(dt > 0.25 ? 0.25 : dt);
    });
}

static void GuardedMouseUpdate()
{
    __try
    {
        dimouse::Update();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        g_mouseHere = false;
        Log("FATAL: exception 0x%08X in the virtual mouse; it stays off", GetExceptionCode());
    }
}

static void __cdecl FrameUpdateHook(double dt)
{
    ++g_frameSerial;
    if (g_modal)
    {
        g_modal = false;
        Log("modal loop: ended after %u frames", g_modalFrames);
    }
    Each(kFrameOrder, [&](const Client& c) {
        if (c.beforeGame)
            c.beforeGame(dt);
    });
    if (g_mouseHere)
        GuardedMouseUpdate();
    g_origFrameUpdate(input::TakeCommandFrame() ? 0.0 : dt); // (a command under the NeuroNet pause: its answer on a frame with no time, input.cpp)
    Each(kAfterGameOrder, [&](const Client& c) {
        if (c.afterGame)
            c.afterGame(dt);
    });
    // every DLL is loaded at the first frame: FLUF loaded after FLvibed moves FLvibed's hooks to its handlers; again
    // when a hook saw FLUF as its caller (a FLUF hook made later)
    static bool adopted = false;
    if (!adopted || hk::AdoptPending())
    {
        adopted = true;
        hk::AdoptRestoreStyle("host");
    }
}

// ------------------------------------------------------------------ the cursor pass
// Freelancer polls the mouse and draws its cursor here, last in the frame's UI rendering.
typedef void(__cdecl* CursorUpdateFn)();
static CursorUpdateFn g_origCursorUpdate = nullptr;
static bool g_cursorHidden = false;

// The pad screens hide the game's arrow while the pad cursor is "hidden" (the focus carries on without it). The game's own
// cursor-shown flag (exe::CursorShown, 0x6107DC) gates the draw half of CursorUpdate after its mouse poll: cleared around the
// game's call and put back, so nothing of the game's state changes.
void SetCursorHidden(bool on) { g_cursorHidden = on && exe::CursorShown.ok(); }

static void SubtitlesPass()
{
    if (const Client* c = g_clients[kSubtitles])
        if (c->cursorPass)
            c->cursorPass();
}

static void __cdecl CursorUpdateHook()
{
    if (g_cursorSerial == g_frameSerial && g_frameSerial && g_clients[kController])
        ModalFrame();
    g_cursorSerial = g_frameSerial;
    if (const Client* c = g_clients[kController])
    {
        if (c->cursorPass)
            c->cursorPass();
        dimouse::BackgroundCursor();
    }
    else if (g_mouseHere)
        dimouse::BackgroundCursor();
    if (g_cursorHidden && exe::CursorShown.ok())
    {
        const u8 shown = Mem<u8>(exe::CursorShown);
        Mem<u8>(exe::CursorShown) = 0;
        // Beside FLUF this hook wraps FLUF's handler, which draws an ImGui arrow for the game's current cursor and
        // ignores CursorShown: no current cursor for that call either (the game reads it only in the draw half the
        // flag skips), put back after unless the game set another meanwhile.
        const bool fluf = exe::CurrentCursor.ok() && hk::ModeOf(exe::CursorUpdate.get()) == hk::HookAtHandler;
        const u32 cursor = fluf ? Mem<u32>(exe::CurrentCursor) : 0;
        if (cursor)
            Mem<u32>(exe::CurrentCursor) = 0;
        SubtitlesPass();
        g_origCursorUpdate();
        if (cursor && !Mem<u32>(exe::CurrentCursor))
            Mem<u32>(exe::CurrentCursor) = cursor;
        Mem<u8>(exe::CursorShown) = shown;
    }
    else
    {
        SubtitlesPass();
        g_origCursorUpdate();
    }
}

// ------------------------------------------------------------------ the UI pass (exe::UiRender)
// Freelancer's UI pass: each state's frame calls it after its 3D scene; it draws every visible control (the space HUD too), then the
// cursor. Drawing first puts the drawing between the scene and the HUD.
typedef void(__cdecl* UiRenderFn)();
static UiRenderFn g_origUiRender = nullptr;

static void __cdecl UiRenderHook()
{
    uibtn::BeforeUiDraw(); // the game's Options page stays hidden under FLvibed's page
    if (exe::UiRenderSkip.ok())
        if (const Client* c = g_clients[kController])
            if (c->underHud)
                c->underHud();
    g_origUiRender();
}

// ------------------------------------------------------------------ the load's frame (exe::UiBatchClose at exe::LoadBracketCall)
// A load skips the UI pass, so nothing above runs in it. The game's frame 0x578F70 closes its batch with the scene still open: the one place that sees every
// load frame, called on every path. The thunk runs for every UI batch too, so only the load's call site counts. The game never waits: the drawing is the
// logic's, one guarded call, and a fault gives the native screen back for good.
typedef void(__cdecl* BatchCloseFn)();
static BatchCloseFn g_origBatchClose = nullptr;
static void (*g_loadDraw)() = nullptr;

void SetLoadDraw(void (*draw)()) { g_loadDraw = draw; }

static void GuardedLoadDraw()
{
    __try
    {
        g_loadDraw();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        g_loadDraw = nullptr;
        Log("FATAL: exception 0x%08X in the load's frame; the game's own loading screen from now on", GetExceptionCode());
    }
}

static void __cdecl BatchCloseHook()
{
    const uintptr_t from = reinterpret_cast<uintptr_t>(_ReturnAddress());
    g_origBatchClose();
    if (g_loadDraw && exe::LoadBracketCall.ok() && from == exe::LoadBracketCall.get() + 5)
        GuardedLoadDraw();
}

bool RawKeysWanted() { return g_clients[kController] && g_clients[kController]->rawKey; }
bool RawKey(int vk, bool down, bool textField) { return g_clients[kController]->rawKey(vk, down, textField); }

void ButtonClicked(int id)
{
    if (const Client* c = g_clients[kController])
        if (c->uiButton)
            c->uiButton(id);
}

// ------------------------------------------------------------------ comm receipt (exe::Comm)
// thiscall(comm player, ?, record) ret 8 (the body reads the record from [esp+44h] = the 2nd argument): a comm starts playing.
typedef void(__fastcall* CommFn)(void* self, void* edx, u32 a1, const u8* rec);
static CommFn g_origComm = nullptr;

static void __fastcall CommHook(void* self, void* edx, u32 a1, const u8* rec)
{
    Each(kCommOrder, [&](const Client& c) {
        if (c.comm)
            c.comm(self, rec);
    });
    g_origComm(self, edx, a1, rec);
}

// ------------------------------------------------------------------ maneuver bar layout (exe::ManeuverLayout)
// The game re-lays the bar out every frame: the modules work on it right after, before it is drawn.
typedef void(__fastcall* ManeuverLayoutFn)(void* self, void* edx);
static ManeuverLayoutFn g_origManeuverLayout = nullptr;

static void __fastcall ManeuverLayoutHook(void* self, void* edx)
{
    g_origManeuverLayout(self, edx);
    Each(kManeuverOrder, [&](const Client& c) {
        if (c.maneuverLayout)
            c.maneuverLayout(reinterpret_cast<u32>(self));
    });
}

void* ManeuverLayoutOriginal() { return reinterpret_cast<void*>(g_origManeuverLayout); }

// ------------------------------------------------------------------ base-room characters (vtable 0x5CB930)
// Constructor 0x443ED0 thiscall(this, a1, a2) ret 8, destructor 0x4440C0 thiscall(this). Both start with an SEH frame:
// push -1; push 5B97C8h (7 bytes, no relative addressing).
typedef void*(__fastcall* CharCtorFn)(void* self, void* edx, void* a1, void* a2);
typedef void(__fastcall* CharDtorFn)(void* self, void* edx);
static CharCtorFn g_origCharCtor = nullptr;
static CharDtorFn g_origCharDtor = nullptr;
static void* g_chars[128];
static int g_charCount = 0;

static void* __fastcall CharCtorHook(void* self, void* edx, void* a1, void* a2)
{
    void* r = g_origCharCtor(self, edx, a1, a2);
    Each(kCharacterOrder, [&](const Client& c) {
        if (c.roomCharacter)
            c.roomCharacter(self, true, g_charCount + 1);
    });
    if (g_charCount < 128)
        g_chars[g_charCount++] = self;
    return r;
}

static void __fastcall CharDtorHook(void* self, void* edx)
{
    Each(kCharacterOrder, [&](const Client& c) {
        if (c.roomCharacter)
            c.roomCharacter(self, false, g_charCount);
    });
    for (int i = 0; i < g_charCount; ++i)
        if (g_chars[i] == self)
        {
            memmove(&g_chars[i], &g_chars[i + 1], sizeof(void*) * (g_charCount - i - 1));
            --g_charCount;
            break;
        }
    g_origCharDtor(self, edx);
}

int RoomCharacters(void** out, int max)
{
    const int n = std::min(max, g_charCount);
    memcpy(out, g_chars, sizeof(void*) * n);
    return n;
}

// ------------------------------------------------------------------ Direct3D 8
// One chain, one owner: Direct3DCreate8 -> CreateDevice -> Reset. Each slot is patched by the one hook, which closes the
// ping-pong two hosts had on a second CreateDevice (each saw the other's hook in the slot and took it for the original).
namespace d3d8idx
{
constexpr int D3D_CreateDevice = 15;
constexpr int Dev_Reset = 14;
} // namespace d3d8idx

typedef void*(WINAPI* Direct3DCreate8Fn)(UINT sdk);
typedef HRESULT(WINAPI* CreateDeviceFn)(void* d3d, UINT adapter, DWORD type, HWND wnd, DWORD flags, void* pp,
                                        void** outDevice);
typedef HRESULT(WINAPI* ResetFn)(void* dev, void* pp);
static Direct3DCreate8Fn g_origCreate8 = nullptr;
static CreateDeviceFn g_origCreateDevice = nullptr;
static ResetFn g_origReset = nullptr;
static void* g_device = nullptr;

void* D3DDevice() { return g_device; }

// The presentation parameters of the last CreateDevice / successful Reset (D3DPRESENT_PARAMETERS, D3D8: 13 DWORDs) - for
// the test-only `d3dreset`.
static u32 g_lastPp[13] = {};
static bool g_havePp = false;

static void DeviceLost()
{
    Each(kDeviceLostOrder, [](const Client& c) {
        if (c.deviceLost)
            c.deviceLost();
    });
}

static HRESULT WINAPI ResetHook(void* dev, void* pp)
{
    if (dev == g_device)
        DeviceLost();
    HRESULT hr = g_origReset(dev, pp);
    SayAll("D3D8 device Reset -> 0x%08X", hr);
    if (dev == g_device && SUCCEEDED(hr) && pp)
        memcpy(g_lastPp, pp, sizeof(g_lastPp)), g_havePp = true;
    if (dev == g_device && SUCCEEDED(hr))
        Each(kDeviceResetOrder, [](const Client& c) {
            if (c.deviceReset)
                c.deviceReset();
        });
    return hr;
}

// `d3dreset` (the Alt+Enter freeze of the owner's run): a background test start only (never the player's
// game) resets the device in place with its own last parameters, through the device's vtable - every Reset hook in the
// chain runs (this one, FLUF's) as for the game's own Reset (a resolution change). D3D_OK = no D3DPOOL_DEFAULT resource or
// state block of anyone stood in the way; D3DERR_INVALIDCALL = something was not released. The reply names
// TestCooperativeLevel before and after.
void D3dResetReply(char* reply, size_t len)
{
    if (!focus::Forced())
    {
        strcpy_s(reply, len, "d3dreset: background test starts only");
        return;
    }
    if (!g_device || !g_havePp)
    {
        strcpy_s(reply, len, "d3dreset: no device / parameters yet");
        return;
    }
    void** vt = *static_cast<void***>(g_device);
    typedef HRESULT(WINAPI * TclFn)(void*);
    const TclFn tcl = reinterpret_cast<TclFn>(vt[3]); // IDirect3DDevice8::TestCooperativeLevel
    u32 pp[13];
    memcpy(pp, g_lastPp, sizeof(pp));
    const HRESULT before = tcl(g_device);
    HRESULT hr = E_FAIL;
    __try
    {
        hr = reinterpret_cast<ResetFn>(vt[d3d8idx::Dev_Reset])(g_device, pp);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        hr = E_UNEXPECTED;
    }
    const HRESULT after = tcl(g_device);
    Log("d3dreset: TestCooperativeLevel 0x%08X, Reset -> 0x%08X, then 0x%08X", before, hr, after);
    sprintf_s(reply, len, "d3dreset: cooperative 0x%08X, Reset 0x%08X, cooperative after 0x%08X", before, hr, after);
}

static void LostFor(int module)
{
    if (const Client* c = g_clients[module])
        if (c->deviceLost)
            c->deviceLost();
}

static HRESULT WINAPI CreateDeviceHook(void* d3d, UINT adapter, DWORD type, HWND wnd, DWORD flags, void* pp,
                                       void** outDevice)
{
    HRESULT hr = g_origCreateDevice(d3d, adapter, type, wnd, flags, pp, outDevice);
    SayAll("D3D8 CreateDevice -> 0x%08X device=%p", hr, outDevice ? *outDevice : nullptr);
    if (SUCCEEDED(hr) && outDevice && *outDevice)
    {
        const bool replaced = g_device != nullptr;
        if (replaced)
            LostFor(kSubtitles);
        g_device = *outDevice;
        if (pp)
            memcpy(g_lastPp, pp, sizeof(g_lastPp)), g_havePp = true;
        void** vt = *static_cast<void***>(g_device);
        // (a refused patch returns null: the previous pointer stays for a hook already installed elsewhere)
        if (vt[d3d8idx::Dev_Reset] != ResetHook)
        {
            hk::Owner own(SharedOwner());
            if (void* o = hk::PatchVtable(vt, d3d8idx::Dev_Reset, ResetHook))
                g_origReset = static_cast<ResetFn>(o);
        }
        if (const Client* c = g_clients[kSubtitles])
            if (c->deviceCreated)
                c->deviceCreated(vt);
        if (replaced)
            LostFor(kController);
    }
    return hr;
}

static void* WINAPI Direct3DCreate8Hook(UINT sdk)
{
    void* d3d = g_origCreate8(sdk);
    SayAll("Direct3DCreate8(%u) -> %p", sdk, d3d);
    if (d3d)
    {
        void** vt = *static_cast<void***>(d3d);
        if (vt[d3d8idx::D3D_CreateDevice] != CreateDeviceHook)
        {
            hk::Owner own(SharedOwner());
            if (void* o = hk::PatchVtable(vt, d3d8idx::D3D_CreateDevice, CreateDeviceHook))
                g_origCreateDevice = static_cast<CreateDeviceFn>(o);
        }
    }
    return d3d;
}

// ------------------------------------------------------------------ game loads
// Every game load loads a fresh Content.dll: its loader notification (ntdll LdrRegisterDllNotification) counts one. The
// first game after the start reuses the Content.dll loaded at the start (docs\FREELANCER_RE.md).
struct LdrUStr
{
    USHORT length, maxLength;
    const wchar_t* buffer;
};
struct LdrNotifyData
{
    ULONG flags;
    const LdrUStr* fullName;
    const LdrUStr* baseName;
    void* base;
    ULONG size;
};
static volatile LONG g_contentLoads = 0;
static void (*volatile g_contentSubscriber)(HMODULE) = nullptr;

static VOID CALLBACK ContentModuleLoaded(ULONG reason, const LdrNotifyData* d, void*)
{
    static const wchar_t kName[] = L"content.dll";
    const size_t len = sizeof(kName) / sizeof(kName[0]) - 1;
    if (reason != 1 || !d || !d->baseName || !d->baseName->buffer || d->baseName->length / sizeof(wchar_t) != len ||
        _wcsnicmp(d->baseName->buffer, kName, len) != 0)
        return;
    InterlockedIncrement(&g_contentLoads);
    if (auto fn = g_contentSubscriber)
        fn(static_cast<HMODULE>(d->base));
}

u32 ContentLoads() { return static_cast<u32>(g_contentLoads); }

static bool g_watchOn = false;

bool OnContentLoaded(void (*fn)(HMODULE content))
{
    g_contentSubscriber = fn;
    return g_watchOn;
}

static void WatchContentLoads()
{
    typedef LONG(NTAPI * RegisterFn)(ULONG, void*, void*, void**);
    static void* cookie = nullptr;
    auto reg = reinterpret_cast<RegisterFn>(GetProcAddress(GetModuleHandleA("ntdll.dll"), "LdrRegisterDllNotification"));
    g_watchOn = reg && reg(0, reinterpret_cast<void*>(ContentModuleLoaded), nullptr, &cookie) == 0;
    SayAll("game load watch (Content.dll loads) %s", g_watchOn ? "on" : "FAILED");
}

// ------------------------------------------------------------------ the start
u32 SharedOwner() { return (g_on[kController] ? OwnerOf(kController) : 0) | (g_on[kSubtitles] ? OwnerOf(kSubtitles) : 0); }
u32 CoreOwner() { return OwnerOf(g_on[kController] ? kController : kSubtitles); }
int LogChannel(int module) { return g_logChannels[module]; }
void Register(int module, const Client* client) { g_clients[module] = client; }

void MovieEvent(int event, void* hwnd, const char* path)
{
    Each(kMovieOrder, [&](const Client& c) {
        if (c.movie)
            c.movie(event, hwnd, path);
    });
}

void MoviePoll()
{
    Each(kMovieOrder, [&](const Client& c) {
        if (c.moviePoll)
            c.moviePoll();
    });
}

const char* MoviePath(const char* path)
{
    Each(kMovieOrder, [&](const Client& c) {
        if (c.moviePath)
            path = c.moviePath(path);
    });
    return path;
}

static void InstallShared()
{
    hk::Owner own(SharedOwner());
    WatchContentLoads();
    // FrameUpdate: push ecx; push ebp; push esi; xor ebp,ebp (5 bytes)
    g_origFrameUpdate = static_cast<FrameUpdateFn>(hk::Detour(exe::FrameUpdate, FrameUpdateHook, 5, "every module's frame"));
    // Comm: sub esp,34h; push ebx; push ebp (5 bytes)
    g_origComm = static_cast<CommFn>(hk::Detour(exe::Comm, CommHook, 5, "comm subtitles, the comm player"));
    // CursorUpdate: mov eax, fs:[0] (6 bytes)
    g_origCursorUpdate =
        static_cast<CursorUpdateFn>(hk::Detour(exe::CursorUpdate, CursorUpdateHook, 6, "the overlay and subtitle drawing"));
    // ManeuverLayout: sub esp,34h; push ebp; push esi; mov ebp,ecx (7 bytes)
    g_origManeuverLayout = static_cast<ManeuverLayoutFn>(
        hk::Detour(exe::ManeuverLayout, reinterpret_cast<void*>(ManeuverLayoutHook), 7,
                   "the bar's autopilot button, the scaled maneuver bar (narrow screens)"));
    // CharCtor / CharDtor: push -1; push <SEH handler> (7 bytes). Both or neither (the list must stay consistent): the
    // destructor first and the constructor only once it is in - a list filled without its removals would keep freed pointers.
    if (exe::CharCtor.ok() && exe::CharDtor.ok())
    {
        g_origCharDtor = static_cast<CharDtorFn>(hk::Detour(exe::CharDtor, CharDtorHook, 7, "room characters"));
        if (g_origCharDtor)
            g_origCharCtor = static_cast<CharCtorFn>(hk::Detour(exe::CharCtor, CharCtorHook, 7, "room characters"));
    }
    else
        SayAll("Hooks CharCtor/CharDtor NOT installed: signature mismatch - room characters off");
    // Direct3D: Freelancer's render pipeline (RP8.dll) creates the D3D8 object after plugins load.
    HMODULE rp8 = GetModuleHandleA("rp8.dll");
    g_origCreate8 = static_cast<Direct3DCreate8Fn>(
        hk::PatchImport(rp8, "d3d8.dll", "Direct3DCreate8", reinterpret_cast<void*>(Direct3DCreate8Hook)));
    SayAll("Direct3DCreate8 import hook %s (rp8=%p)", g_origCreate8 ? "installed" : "NOT FOUND", rp8);
}

static const bool* g_moduleRows = nullptr;
bool ModuleOn(int row) { return g_moduleRows && row >= 0 && row < modules::kRows && g_moduleRows[row]; }

static const ApHost* g_apHost = nullptr;
void SetApHost(const ApHost* host) { g_apHost = host; }
int ApJumpInCount() { return g_apHost ? g_apHost->JumpInCount() : -1; }
bool ApGuardEvade(void** vtable) { return g_apHost && g_apHost->GuardEvade(vtable); }
bool ApSetGunAim(bool on, const float* world) { return g_apHost && g_apHost->SetGunAim(on, world); }

static const char* (*g_windowMode)(char* detail, int len) = nullptr;
void SetWindowMode(const char* (*reader)(char* detail, int len)) { g_windowMode = reader; }
const char* WindowMode(char* detail, int len)
{
    if (g_windowMode)
        return g_windowMode(detail, len);
    if (detail && len > 0)
        detail[0] = 0;
    return "off";
}

// The first thing to read in a player's log from an older Windows: which one, and whether VxKex runs the game (it rewrites imports and
// can change what the look-ups find).
static void LogWindows()
{
    typedef LONG(WINAPI * RtlGetVersionFn)(OSVERSIONINFOW*);
    const auto getVersion = reinterpret_cast<RtlGetVersionFn>(GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlGetVersion"));
    OSVERSIONINFOW v = {};
    v.dwOSVersionInfoSize = sizeof(v);
    if (getVersion && getVersion(&v) == 0)
        Log("windows %lu.%lu build %lu%s", v.dwMajorVersion, v.dwMinorVersion, v.dwBuildNumber, GetModuleHandleA("KexDll.dll") ? ", VxKex" : "");
}

void Start(HINSTANCE self, bool subtitles, bool controller, const char* rows, const bool* moduleRows)
{
    g_moduleRows = moduleRows;
    g_on[kSubtitles] = subtitles;
    g_on[kController] = controller;
    if (subtitles)
        g_logChannels[kSubtitles] = LogOpen(self, "FLSubtitles.log");
    if (controller)
        g_logChannels[kController] = LogOpen(self, "FLController.log");
    Log("modules: %s", rows);
    LogWindows();
    win7sim::Install(self);

    // Every Freelancer.exe address is verified by its signature; a hook whose code is not what we expect is not installed
    // (its feature stays off, logged), the rest works.
    exe::ResolveAll("host");
    hk::LogRestoreStyleAtStart("host");

    InstallShared();
    hk::Owner own(CoreOwner());
    input::InstallKeys();
    uibtn::Install(); // FLvibed's native buttons: the Options screen, the pause menu
    // UiRender: mov eax,1008h (5 bytes)
    g_origUiRender = static_cast<UiRenderFn>(hk::Detour(exe::UiRender, UiRenderHook, 5, "drawing under the HUD"));
    // UiBatchClose: mov eax, [abs] (5 bytes)
    if (ModuleOn(modules::kHud) && exe::UiBatchClose.ok() && exe::LoadBracketCall.ok())
        g_origBatchClose = static_cast<BatchCloseFn>(hk::Detour(exe::UiBatchClose, BatchCloseHook, 5, "the loading screen"));
    focus::Start(ModuleOn(modules::kDisplay));
    fixes::Install();
    chatlines::Install(); // the server's word on the autopilot; its tag line stays out of the chat, whichever modules run
    tractorhook::Install(); // two listeners: HUD 2 and the subtitles both show the reason
    if (!controller)
        pagehost::Start();
    g_mouseHere = !controller && focus::Forced();
}
} // namespace router
