// FLvibed - the FLUF probe (26 Oct 2026; docs\research\fluf.md "FLUF in play" E, docs\MOD_COMPATIBILITY.md FLUF): knows
// whether FLUF (The Starport's framework) is loaded, which build, and - for a known build - its ImGui windows' live state
// and rectangles, read from the Dear ImGui FLUF.dll exports. One instance in FLvibed.dll (compiled plain beside
// core\hud_bus.cpp, which serves it as HudBus::ReadFluf); pure parts testable (tests\fluf_probe_test.cpp).
//
// Robust / fails closed: the build id (FLUF.dll's PE TimeDateStamp + SizeOfImage; every FLUF DLL's version resource says
// 1.0.0) must be a known one AND the ImGui version string the expected one AND the layout checks below must pass before a
// single window field is read; anything else = co-residency only (no windows) + one status line. Efficient: a snapshot
// only when FLUF's ImGui frame count moved (once per frame at most), one pass over ImGui's window list (~10-30 windows).
// Never writes FLUF's or ImGui's memory, never calls anything that changes ImGui state.
#pragma once
#include "hud_bus.h"

namespace fluf
{
// Dear ImGui field offsets for one ImGui build as FLUF compiles it (x86, MSVC ABI; IMGUI_USE_WCHAR32,
// IMGUI_ENABLE_FREETYPE, IMGUI_DEFINE_MATH_OPERATORS: FLUF cmake\Utils.cmake 75-87). Measured with offsetof on the
// headers FLUF ships (development\include\imgui, IMGUI_VERSION "1.92.7 WIP" 19266, docking).
struct ImGuiLayout
{
    const char* version;            // GetVersion() must equal this
    u32 ctxIO, ctxFrameCount, ctxWindows;
    u32 ioDisplaySize, ioWantCaptureMouse, ioWantTextInput, ioMousePos, ioCtx;
    u32 winCtx, winName, winFlags, winPos, winSize, winActive, winWasActive, winHidden, winLastFrameActive;
    u32 flagNoBackground, flagChild, flagTooltip;
    // bus v5 (26 Oct 2026, the same headers): the focused window, the close button's place, pass-through windows
    u32 ctxNavWindow, ctxStyleFramePadding;          // ImGuiContext::NavWindow, ::Style + ImGuiStyle::FramePadding
    u32 winHasCloseButton, winTitleBarHeight, winCollapsed, winSizeFull, winRootWindow;
    u32 flagNoTitleBar, flagNoMouseInputs;
};
extern const ImGuiLayout kImGui_1_92_7_WIP;

// The ImGui exports the snapshot uses (FLUF.dll's, or a test's fakes). All __cdecl, no arguments.
struct ImGuiApi
{
    void* (*GetCurrentContext)();
    void* (*GetIO)();
    int (*GetFrameCount)();
};

// A known FLUF build: its id and whether the awareness layer reads its windows.
struct KnownBuild
{
    u32 timeDateStamp, sizeOfImage;
    const char* name;
    const ImGuiLayout* layout; // null = co-residency only (no window reads)
};
// The build table (nightly-2026-10-16 aware; stable 2.0.0 co-residency only); null when unknown.
const KnownBuild* FindBuild(u32 timeDateStamp, u32 sizeOfImage);

// What a window name means (fluf.md B): HudFlufKind, or FlufNone for windows that are never keep-outs (ImGui's implicit
// "Debug##Default").
u8 KindOf(const char* name);

// One snapshot of the ImGui state through `api` + `L` into `out` (windows, textInput, mouseOver; not present / build /
// status / policy). False when a layout check fails (`why` says which): the caller then stops reading for good. The
// caller guards it with SEH. `policy` filters: 0 no windows, 1 without FlufDebug, 2 all.
// `extra` (optional): each listed window's name, input rule, focus and close button (bus v5).
bool Snapshot(const ImGuiApi& api, const ImGuiLayout& L, int policy, HudFluf& out, char* why, size_t whyLen,
              HudFlufExtra* extra = nullptr);

// FLvibed.dll's live probe (hud_bus.cpp): detect once, then a snapshot whenever FLUF's ImGui frame moved.
bool Read(HudFluf* out);
void SetPolicy(int policy);
bool ReadExtra(HudFlufExtra* out); // the extras of the snapshot Read last returned
} // namespace fluf
