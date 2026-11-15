// Builds one texture atlas from the game's flat icon sheets for HUD 2 and answers each icon's texture and UV.
#include "flicons.h"

#include "flicons_decode.h"
#include "log.h"
#include "render.h"

#include <sstream>

namespace flicons
{
static volatile LONG g_state = 0; // 0 idle, 1 loading, 2 pixels ready, 3 failed, 4 built
static HANDLE g_thread = nullptr;
static std::vector<u32> g_bigPx, g_smallPx; // the worker's output (read by the game thread after state 2)
static std::string g_source, g_why;         // which file / why it failed (the worker writes before the state change)
static int g_sheetPx = 0;                   // the sheets' size read (1024 HDE, 256 vanilla)
static render::Image g_big, g_small;
static void* g_device = nullptr;

// ---------------------------------------------------------------- the worker: file -> pixels (flicons_decode.cpp)
static bool LoadFile(const std::wstring& path)
{
    Atlas a;
    if (!ReadAtlas(path, a))
        return false;
    g_bigPx.swap(a.bigPx);
    g_smallPx.swap(a.smallPx);
    g_sheetPx = a.sheetPx;
    return true;
}

static DWORD WINAPI Worker(LPVOID)
{
    wchar_t exe[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::wstring dir = exe;
    const size_t slash = dir.find_last_of(L'\\');
    dir = slash == std::wstring::npos ? L"" : dir.substr(0, slash + 1); // the EXE folder
    const std::wstring data = dir + L"..\\DATA\\INTERFACE\\";
    for (const wchar_t* f : kSheetFiles)
        if (LoadFile(data + f))
        {
            for (const wchar_t* p = f; *p; ++p)
                g_source += static_cast<char>(*p);
            InterlockedExchange(&g_state, 2);
            return 0;
        }
    g_why = "no flat icon sheet (DATA\\INTERFACE\\buttontextures_flaticons.txm / buttontextures.txm) could be read";
    InterlockedExchange(&g_state, 3);
    return 0;
}

bool Ready(void* device)
{
    const LONG st = g_state;
    if (st == 4)
    {
        if (device == g_device && g_big.texture)
            return true;
        // a new device: rebuild from the kept pixels
        render::FreeImage(g_big), render::FreeImage(g_small);
        InterlockedExchange(&g_state, 2);
        return false;
    }
    if (st == 0)
    {
        InterlockedExchange(&g_state, 1);
        g_thread = CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr);
        if (!g_thread)
        {
            g_why = "worker thread not started";
            InterlockedExchange(&g_state, 3);
        }
        return false;
    }
    if (st == 2 && device)
    {
        if (render::BuildImage(g_big, g_bigPx, kBigW, kBigH) && render::BuildImage(g_small, g_smallPx, kSmallW, kSmallH))
        {
            g_device = device;
            InterlockedExchange(&g_state, 4);
            Log("flicons: atlas built from %s (%d icons, %d px sheets)", g_source.c_str(), static_cast<int>(Count), g_sheetPx);
            return true;
        }
        g_why = "texture not created";
        InterlockedExchange(&g_state, 3);
        Log("flicons: %s", g_why.c_str());
    }
    if (st == 3)
    {
        static bool logged = false;
        if (!logged)
            logged = true, Log("flicons: off (%s): HUD 2 draws its own shapes", g_why.c_str());
    }
    return false;
}

bool Uv(Icon icon, float sizePx, void** texture, float uv[4])
{
    if (g_state != 4 || icon < 0 || icon >= Count)
        return false;
    const bool smallPx = SmallFor(sizePx);
    const render::Image& img = smallPx ? g_small : g_big;
    if (!img.texture)
        return false;
    CellUv(icon, smallPx, uv);
    *texture = img.texture;
    return true;
}

void Shutdown()
{
    if (g_thread)
    {
        WaitForSingleObject(g_thread, 10000);
        CloseHandle(g_thread);
        g_thread = nullptr;
    }
    render::FreeImage(g_big);
    render::FreeImage(g_small);
    g_device = nullptr;
    if (g_state == 4)
        InterlockedExchange(&g_state, 2);
}

std::string Status()
{
    static const char* kNames[] = {"idle", "loading", "decoded", "failed", "ready"};
    std::ostringstream o;
    const LONG st = g_state;
    o << "flicons " << (st >= 0 && st <= 4 ? kNames[st] : "?") << " source=" << (g_source.empty() ? "-" : g_source) << " sheets=" << g_sheetPx;
    if (st == 3)
        o << " why=" << g_why;
    return o.str();
}
} // namespace flicons
