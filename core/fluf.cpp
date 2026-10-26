// FLvibed - the FLUF probe (core\fluf.h). Compiled plain into FLvibed.dll beside core\hud_bus.cpp (build.cmd :link) and
// into the unit tests (tests\fluf_probe_test.cpp). The game's thread only.
#include "fluf.h"

#include <cmath>
#include <cstdarg>

namespace fluf
{
// offsetof on FLUF's shipped ImGui headers (1.92.7 WIP, 19266, docking; x86; FLUF's compile definitions), 26 Oct 2026
const ImGuiLayout kImGui_1_92_7_WIP = {
    "1.92.7 WIP",
    48,   // ImGuiContext::IO
    4,    // ImGuiContext::FrameCount
    5224, // ImGuiContext::Windows (ImVector<ImGuiWindow*>: Size, Capacity, Data)
    8,    // ImGuiIO::DisplaySize
    140,  // ImGuiIO::WantCaptureMouse
    142,  // ImGuiIO::WantTextInput
    180,  // ImGuiIO::MousePos
    176,  // ImGuiIO::Ctx
    0,    // ImGuiWindow::Ctx
    4,    // ImGuiWindow::Name
    12,   // ImGuiWindow::Flags
    76,   // ImGuiWindow::Pos
    84,   // ImGuiWindow::Size
    245,  // ImGuiWindow::Active
    246,  // ImGuiWindow::WasActive
    253,  // ImGuiWindow::Hidden
    688,  // ImGuiWindow::LastFrameActive
    1 << 7,  // ImGuiWindowFlags_NoBackground
    1 << 24, // ImGuiWindowFlags_ChildWindow
    1 << 25, // ImGuiWindowFlags_Tooltip
    // bus v5 (offsetof on the same headers and definitions, 26 Oct 2026; the fields above measured again: equal)
    8236,    // ImGuiContext::NavWindow
    3252 + 76, // ImGuiContext::Style + ImGuiStyle::FramePadding
    256,     // ImGuiWindow::HasCloseButton
    140,     // ImGuiWindow::TitleBarHeight
    248,     // ImGuiWindow::Collapsed
    92,      // ImGuiWindow::SizeFull
    928,     // ImGuiWindow::RootWindow
    1 << 0,  // ImGuiWindowFlags_NoTitleBar
    1 << 9,  // ImGuiWindowFlags_NoMouseInputs
};

// FLUF.dll's PE TimeDateStamp + SizeOfImage (research\evidence\fluf_nightly\NOTES.txt)
static const KnownBuild kBuilds[] = {
    {0x6AD1955A, 0x1944D000, "nightly-2026-10-16", &kImGui_1_92_7_WIP}, // tag nightly-2026-10-16-03-03 = 627ea2c
    {0x69DD76EF, 0x005BB000, "2.0.0", nullptr},                         // stable 2.0.0 (28 Mar 2026): history
};

const KnownBuild* FindBuild(u32 stamp, u32 size)
{
    for (const KnownBuild& b : kBuilds)
        if (b.timeDateStamp == stamp && b.sizeOfImage == size)
            return &b;
    return nullptr;
}

static bool StartsWith(const char* s, const char* p) { return strncmp(s, p, strlen(p)) == 0; }

u8 KindOf(const char* name)
{
    if (!name || !*name)
        return FlufNone;
    if (StartsWith(name, "Debug##Default")) // ImGui's implicit window (begun every frame, shown only when written to)
        return FlufNone;
    if (strcmp(name, "fluf_cmd_palette") == 0)
        return FlufPalette;
    if (strcmp(name, "Player Status") == 0) // the extended options menu AND the Player Status window (one ImGui window)
        return FlufWindow;
    if (StartsWith(name, "NavMap"))         // NavMap, NavMapObjectiveWindow, NavMapDescriptionWindow
        return FlufMap;
    if (StartsWith(name, "##TOAST"))
        return FlufToast;
    if (strcmp(name, "Debug Window") == 0 || StartsWith(name, "DEBUG:") || StartsWith(name, "Dear ImGui") ||
        StartsWith(name, "ImAnim"))
        return FlufDebug;
    return FlufOther;
}

template <class T> static T At(const u8* base, u32 off) { return *reinterpret_cast<const T*>(base + off); }

static bool Fail(char* why, size_t len, const char* what)
{
    if (why && len)
        strncpy_s(why, len, what, _TRUNCATE);
    return false;
}

bool Snapshot(const ImGuiApi& api, const ImGuiLayout& L, int policy, HudFluf& out, char* why, size_t whyLen,
              HudFlufExtra* extra)
{
    out.textInput = out.mouseOver = out.palette = 0;
    out.count = out.kinds = 0;
    if (extra)
        *extra = {};
    if (!api.GetCurrentContext || !api.GetIO || !api.GetFrameCount)
        return Fail(why, whyLen, "ImGui exports missing");
    const u8* ctx = static_cast<const u8*>(api.GetCurrentContext());
    if (!ctx) // FLUF's ImGui not up yet (its renderer creates the context with the device): nothing open
    {
        out.frame = 0;
        return true;
    }
    // the layout checks: each field read is first tied to a value the exports return
    const u8* io = static_cast<const u8*>(api.GetIO());
    if (io != ctx + L.ctxIO)
        return Fail(why, whyLen, "ImGuiIO is not where the layout puts it");
    if (At<const u8*>(io, L.ioCtx) != ctx)
        return Fail(why, whyLen, "ImGuiIO::Ctx does not name the context");
    const int fc = api.GetFrameCount();
    if (At<int>(ctx, L.ctxFrameCount) != fc)
        return Fail(why, whyLen, "ImGuiContext::FrameCount differs from GetFrameCount");
    const float dw = At<float>(io, L.ioDisplaySize), dh = At<float>(io, L.ioDisplaySize + 4);
    // the context exists but FLUF's ImGui has not begun a frame yet (A3, 26 Oct 2026: FLvibed's first frame comes
    // before FLUF's first NewFrame, which sets DisplaySize - ImGuiIO starts it at (-1, -1)): nothing open, checked
    // again on the next frame
    if (fc == 0 && (dw == -1.0f || dw == 0.0f) && dh == dw)
    {
        out.frame = 0;
        return true;
    }
    if (!(dw >= 64 && dw <= 32768 && dh >= 64 && dh <= 32768))
    {
        if (why && whyLen)
            _snprintf_s(why, whyLen, _TRUNCATE, "ImGuiIO::DisplaySize out of range (%g x %g at frame %d)", dw, dh, fc);
        return false;
    }
    struct Vec
    {
        int size, capacity;
        const u8* const* data;
    };
    const Vec v = At<Vec>(ctx, L.ctxWindows);
    if (v.size < 0 || v.size > 4096 || v.capacity < v.size || (v.size && !v.data))
        return Fail(why, whyLen, "ImGuiContext::Windows is not a window list");
    // ImGui's keyboard focus (bus v5): none, or one of the listed windows
    const u8* nav = At<const u8*>(ctx, L.ctxNavWindow);
    if (nav)
    {
        bool listed = false;
        for (int i = 0; i < v.size && !listed; ++i)
            listed = v.data[i] == nav;
        if (!listed)
            return Fail(why, whyLen, "ImGuiContext::NavWindow is not a listed window");
    }
    const float fpx = At<float>(ctx, L.ctxStyleFramePadding), fpy = At<float>(ctx, L.ctxStyleFramePadding + 4);
    out.frame = static_cast<u32>(fc);
    if (extra)
        extra->frame = out.frame;
    out.textInput = At<bool>(io, L.ioWantTextInput) ? 1 : 0;
    const float mx = At<float>(io, L.ioMousePos), my = At<float>(io, L.ioMousePos + 4);
    for (int i = 0; i < v.size; ++i)
    {
        const u8* w = v.data[i];
        if (!w || At<const u8*>(w, L.winCtx) != ctx)
            return Fail(why, whyLen, "a window of the list does not name the context");
        const char* name = At<const char*>(w, L.winName);
        if (!name)
            return Fail(why, whyLen, "a window without a name");
        const int lfa = At<int>(w, L.winLastFrameActive);
        const bool active = At<bool>(w, L.winActive);
        if (lfa > fc || (active && lfa != fc))
            return Fail(why, whyLen, "a window's LastFrameActive does not fit the frame count");
        const int flags = At<int>(w, L.winFlags);
        if (flags & (static_cast<int>(L.flagChild) | static_cast<int>(L.flagTooltip)))
            continue; // a child lies inside its parent's rectangle; tooltips follow the cursor
        // open = begun this frame or the last one (read between FLUF's frames or after its NewFrame), not hidden
        if (lfa < fc - 1 || At<bool>(w, L.winHidden))
            continue;
        const u8 kind = KindOf(name);
        if (kind == FlufPalette)
            out.palette = 1; // whatever the policy (it takes every key, the owner 26 Oct 2026)
        if (kind == FlufNone || policy <= 0 || (policy == 1 && kind == FlufDebug))
            continue;
        const float x = At<float>(w, L.winPos), y = At<float>(w, L.winPos + 4);
        const float sw = At<float>(w, L.winSize), sh = At<float>(w, L.winSize + 4);
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(sw) || !std::isfinite(sh) || sw <= 0 || sh <= 0)
            continue;
        HudRect r = {Clampf(x / dw, 0, 1), Clampf(y / dh, 0, 1), Clampf((x + sw) / dw, 0, 1), Clampf((y + sh) / dh, 0, 1)};
        if (r.x1 <= r.x0 || r.y1 <= r.y0)
            continue; // off screen
        // FLUF's click rule (UiManager.cpp 237-275): its own windows, or any active window that draws a background
        const bool mouse = kind == FlufPalette || kind == FlufWindow || kind == FlufMap ||
                           (kind != FlufToast && !(flags & static_cast<int>(L.flagNoBackground)));
        out.kinds |= static_cast<u8>(1u << kind);
        if (mouse && mx >= x && mx < x + sw && my >= y && my < y + sh)
            out.mouseOver = 1;
        if (out.count >= kHudFlufMax)
            continue;
        if (extra)
        {
            HudFlufWindowExtra& e = extra->win[out.count];
            strncpy_s(e.name, name, _TRUNCATE);
            // the player's input: FLUF's click rule, and ImGui hit-tests it (not a NoMouseInputs pass-through overlay)
            e.input = mouse && !(flags & static_cast<int>(L.flagNoMouseInputs)) ? 1 : 0;
            e.focused = nav && (nav == w || At<const u8*>(nav, L.winRootWindow) == w) ? 1 : 0;
            // the title bar's close button (ImGui RenderWindowTitleBarContents: FontSize square at the bar's right end,
            // FramePadding in from the edge; the bar is FontSize + 2 FramePadding.y high)
            e.closeX = e.closeY = -1;
            const float tb = At<float>(w, L.winTitleBarHeight), fullW = At<float>(w, L.winSizeFull);
            const float bs = tb - 2 * fpy;
            if (At<bool>(w, L.winHasCloseButton) && !(flags & static_cast<int>(L.flagNoTitleBar)) && std::isfinite(tb) &&
                std::isfinite(fullW) && std::isfinite(fpx) && bs > 1 && tb < sh + 1 && fullW > bs)
            {
                const float cx = x + fullW - fpx - bs * 0.5f, cy = y + fpy + bs * 0.5f;
                if (cx > x && cx < x + fullW && cy > y && cy < y + tb && cx >= 0 && cy >= 0 && cx < dw && cy < dh)
                    e.closeX = cx / dw, e.closeY = cy / dh;
            }
            extra->count = static_cast<u8>(out.count + 1);
        }
        out.win[out.count++] = {r, kind, static_cast<u8>(mouse ? 1 : 0), {0, 0}};
    }
    return true;
}

// ------------------------------------------------------------------ the live probe (FLvibed.dll)
namespace
{
enum class Level
{
    Unprobed,
    None,
    CoResidency,
    Aware,
};
Level g_level = Level::Unprobed;
const ImGuiLayout* g_layout = nullptr;
ImGuiApi g_api = {};
HudFluf g_snap = {};
HudFlufExtra g_extra = {};
int g_policy = 2; // `fluf_windows` until the controller sets it (its default 2: every FLUF window, owner 26 Oct 2026)
int g_lastFrame = -1, g_lastPolicy = -1;
DWORD g_lastProbe = 0;

void SetStatus(const char* fmt, ...)
{
    va_list a;
    va_start(a, fmt);
    _vsnprintf_s(g_snap.status, sizeof(g_snap.status), _TRUNCATE, fmt, a);
    va_end(a);
}

template <class F> F Export(HMODULE m, const char* name) { return reinterpret_cast<F>(GetProcAddress(m, name)); }

void Probe()
{
    g_lastProbe = GetTickCount();
    HMODULE m = GetModuleHandleA("fluf.dll");
    if (!m)
    {
        g_level = Level::None;
        g_snap.build[0] = 0;
        SetStatus("no FLUF");
        return;
    }
    u32 stamp = 0, size = 0;
    __try
    {
        auto base = reinterpret_cast<const u8*>(m);
        auto nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + reinterpret_cast<const IMAGE_DOS_HEADER*>(base)->e_lfanew);
        stamp = nt->FileHeader.TimeDateStamp;
        size = nt->OptionalHeader.SizeOfImage;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    const char* form = GetModuleHandleA("fluf_boot.dll") ? "boot loader" : GetModuleHandleA("dacom_fix.dll") ? "dacom_fix" : "direct";
    // capability exports (the nightly's class; 2.0.0 had neither) - informative only: FLUF's class names may change (#141)
    const bool nightlyClass = GetProcAddress(m, "?SetKeyBlocked@KeyManager@@QAEXW4Key@@_N@Z") &&
                              GetProcAddress(m, "?GetImGuiInterface@FlufClient@@SAPAVImGuiInterface@@XZ");
    const KnownBuild* kb = FindBuild(stamp, size);
    g_level = Level::CoResidency;
    if (!kb)
    {
        strncpy_s(g_snap.build, "unknown", _TRUNCATE);
        SetStatus("FLUF unknown build (stamp %08X, image %08X, %s exports, %s): co-residency only", stamp, size,
                  nightlyClass ? "nightly-class" : "older", form);
        return;
    }
    strncpy_s(g_snap.build, kb->name, _TRUNCATE);
    if (!kb->layout)
    {
        SetStatus("FLUF %s (%s): co-residency only (no window reads for this build)", kb->name, form);
        return;
    }
    g_api.GetCurrentContext = Export<void* (*)()>(m, "?GetCurrentContext@ImGui@@YAPAUImGuiContext@@XZ");
    g_api.GetIO = Export<void* (*)()>(m, "?GetIO@ImGui@@YAAAUImGuiIO@@XZ");
    g_api.GetFrameCount = Export<int (*)()>(m, "?GetFrameCount@ImGui@@YAHXZ");
    auto getVersion = Export<const char* (*)()>(m, "?GetVersion@ImGui@@YAPBDXZ");
    const char* ver = nullptr;
    __try
    {
        ver = getVersion ? getVersion() : nullptr;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ver = nullptr;
    }
    if (!g_api.GetCurrentContext || !g_api.GetIO || !g_api.GetFrameCount || !ver || strcmp(ver, kb->layout->version) != 0)
    {
        SetStatus("FLUF %s (%s): co-residency only (ImGui exports / version not as expected)", kb->name, form);
        return;
    }
    g_layout = kb->layout;
    g_level = Level::Aware;
    SetStatus("FLUF %s (%s), ImGui %s: aware (windows read from its ImGui state)", kb->name, form, ver);
}

bool GuardedFrameCount(int& fc)
{
    __try
    {
        fc = g_api.GetFrameCount();
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool GuardedSnapshot(char* why, size_t len)
{
    __try
    {
        return Snapshot(g_api, *g_layout, g_policy, g_snap, why, len, &g_extra);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        strncpy_s(why, len, "an exception while reading", _TRUNCATE);
        return false;
    }
}
} // namespace

void SetPolicy(int policy) { g_policy = policy < 0 ? 0 : policy > 2 ? 2 : policy; }

bool ReadExtra(HudFlufExtra* out)
{
    if (out)
        *out = g_extra;
    return g_level == Level::Aware && g_extra.frame == g_snap.frame && g_extra.count == g_snap.count;
}

bool Read(HudFluf* out)
{
    if (g_level == Level::Unprobed || (g_level == Level::None && GetTickCount() - g_lastProbe > 1000))
        Probe();
    if (g_level == Level::Aware)
    {
        int fc = 0;
        if (!GuardedFrameCount(fc))
        {
            g_level = Level::CoResidency;
            SetStatus("FLUF %s: co-residency only (ImGui frame count unreadable)", g_snap.build);
        }
        else if (fc != g_lastFrame || g_policy != g_lastPolicy)
        {
            g_lastFrame = fc, g_lastPolicy = g_policy;
            char why[80] = {};
            if (!GuardedSnapshot(why, sizeof(why)))
            {
                g_level = Level::CoResidency;
                SetStatus("FLUF %s: co-residency only (ImGui layout check failed: %s)", g_snap.build, why);
            }
        }
    }
    if (g_level != Level::Aware)
        g_snap.count = g_snap.kinds = g_snap.textInput = g_snap.mouseOver = g_snap.palette = 0, g_snap.frame = 0,
        g_extra = {};
    g_snap.present = g_level == Level::Aware ? 2 : g_level == Level::CoResidency ? 1 : 0;
    g_snap.policy = static_cast<u8>(g_policy);
    if (out)
        *out = g_snap;
    return g_snap.present != 0;
}
} // namespace fluf
