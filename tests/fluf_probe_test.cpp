// The FLUF probe (core\fluf.*) on a fake Dear ImGui state laid out as FLUF's ImGui 1.92.7 WIP: the build table, the
// window names' kinds, the snapshot's rectangles / text input / FLUF's mouse rule / the policy filter, and failing closed
// when a layout check does not hold (each check broken in turn).
#include "fluf.h"
#include "fluf_rules.h"

#include "test.h"

namespace
{
const fluf::ImGuiLayout& L = fluf::kImGui_1_92_7_WIP;
u8 g_ctx[10680];
u8 g_win[8][1080];
const u8* g_list[8];
int g_frame = 100;

void* FakeContext() { return g_ctx; }
void* FakeIO() { return g_ctx + L.ctxIO; }
int FakeFrameCount() { return g_frame; }

template <class T> void Put(u8* base, u32 off, T v) { memcpy(base + off, &v, sizeof(T)); }

void Window(int i, const char* name, float x, float y, float w, float h, int lastFrame, int flags = 0)
{
    memset(g_win[i], 0, sizeof(g_win[i]));
    Put<const void*>(g_win[i], L.winCtx, g_ctx);
    Put<const char*>(g_win[i], L.winName, name);
    Put<int>(g_win[i], L.winFlags, flags);
    Put<float>(g_win[i], L.winPos, x), Put<float>(g_win[i], L.winPos + 4, y);
    Put<float>(g_win[i], L.winSize, w), Put<float>(g_win[i], L.winSize + 4, h);
    Put<bool>(g_win[i], L.winActive, lastFrame == g_frame);
    Put<int>(g_win[i], L.winLastFrameActive, lastFrame);
    g_list[i] = g_win[i];
}

void Context(int windows, bool textInput, float mx, float my)
{
    memset(g_ctx, 0, sizeof(g_ctx));
    u8* io = g_ctx + L.ctxIO;
    Put<int>(g_ctx, L.ctxFrameCount, g_frame);
    Put<float>(io, L.ioDisplaySize, 2000.0f), Put<float>(io, L.ioDisplaySize + 4, 1000.0f);
    Put<const void*>(io, L.ioCtx, g_ctx);
    Put<bool>(io, L.ioWantTextInput, textInput);
    Put<float>(io, L.ioMousePos, mx), Put<float>(io, L.ioMousePos + 4, my);
    struct Vec
    {
        int size, capacity;
        const u8* const* data;
    } v = {windows, 8, g_list};
    Put<Vec>(g_ctx, L.ctxWindows, v);
}

const fluf::ImGuiApi kApi = {FakeContext, FakeIO, FakeFrameCount};

bool Snap(int policy, HudFluf& out)
{
    char why[80] = {};
    out = {};
    return fluf::Snapshot(kApi, L, policy, out, why, sizeof(why));
}
} // namespace

void Test_fluf_probe()
{
    // the build table: the nightly is read, 2.0.0 co-residency only, anything else unknown
    const fluf::KnownBuild* nightly = fluf::FindBuild(0x6AD1955A, 0x1944D000);
    CHECK(nightly && nightly->layout == &fluf::kImGui_1_92_7_WIP);
    const fluf::KnownBuild* stable = fluf::FindBuild(0x69DD76EF, 0x5BB000);
    CHECK(stable && !stable->layout);
    CHECK(!fluf::FindBuild(0x6AD1955A, 0x1944E000));

    // the names (fluf.md "FLUF in play" B)
    CHECK(fluf::KindOf("fluf_cmd_palette") == FlufPalette);
    CHECK(fluf::KindOf("Player Status") == FlufWindow);
    CHECK(fluf::KindOf("NavMap") == FlufMap && fluf::KindOf("NavMapObjectiveWindow") == FlufMap);
    CHECK(fluf::KindOf("Debug Window") == FlufDebug && fluf::KindOf("DEBUG: Target Info") == FlufDebug);
    CHECK(fluf::KindOf("##TOAST0") == FlufToast);
    CHECK(fluf::KindOf("Debug##Default") == FlufNone && fluf::KindOf("") == FlufNone);
    CHECK(fluf::KindOf("Auto Turrets") == FlufOther);

    // a frame: the palette open with the keyboard, the Debug Window, a closed options window, a toast, a child, a
    // tooltip
    g_frame = 100;
    Window(0, "fluf_cmd_palette", 700, 0, 600, 400, 100);
    Window(1, "Debug Window", 60, 60, 300, 200, 100);
    Window(2, "Player Status", 0, 0, 933, 700, 90);            // closed ten frames ago
    Window(3, "##TOAST0", 40, 50, 200, 60, 99, 1 << 9);       // begun last frame (read after FLUF's NewFrame)
    Window(4, "NavMapObjectiveWindow", 10, 10, 50, 50, 100, 1 << 24); // a child
    Window(5, "##Tooltip_00", 0, 0, 50, 20, 100, 1 << 25);
    Context(6, true, 800, 100);
    HudFluf f;
    CHECK(Snap(1, f));
    CHECK(f.frame == 100 && f.textInput == 1);
    CHECK(f.count == 2); // palette + toast (policy 1 leaves the Debug Window out)
    CHECK(f.win[0].kind == FlufPalette && f.win[0].mouse == 1);
    CHECK_NEAR(f.win[0].rect.x0, 0.35f, 1e-6f);
    CHECK_NEAR(f.win[0].rect.x1, 0.65f, 1e-6f);
    CHECK_NEAR(f.win[0].rect.y1, 0.4f, 1e-6f);
    CHECK(f.win[1].kind == FlufToast && f.win[1].mouse == 0);
    CHECK(f.mouseOver == 1); // the cursor over the palette
    CHECK(f.kinds == ((1 << FlufPalette) | (1 << FlufToast)));
    CHECK(Snap(2, f) && f.count == 3 && (f.kinds & (1 << FlufDebug)));
    CHECK(Snap(0, f) && f.count == 0 && f.textInput == 1); // policy 0: no keep-outs, the text input still known

    // the cursor over the Debug Window only: FLUF takes the mouse there (a background) - with policy 2
    Context(6, false, 100, 100);
    CHECK(Snap(2, f) && f.mouseOver == 1 && f.textInput == 0);
    CHECK(Snap(1, f) && f.mouseOver == 0);

    // no context yet: nothing open, not a failure
    {
        const fluf::ImGuiApi none = {[]() -> void* { return nullptr; }, FakeIO, FakeFrameCount};
        char why[80] = {};
        HudFluf n = {};
        CHECK(fluf::Snapshot(none, L, 1, n, why, sizeof(why)) && n.count == 0 && n.frame == 0);
    }

    // a context before FLUF's first NewFrame (frame 0, DisplaySize (-1, -1) or 0 x 0; A3 26 Oct 2026): nothing open, not a failure;
    // a 0 x 0 display at a later frame still fails (below)
    {
        const int keep = g_frame;
        g_frame = 0;
        Context(0, false, 0, 0);
        Put<float>(g_ctx + L.ctxIO, L.ioDisplaySize, 0.0f), Put<float>(g_ctx + L.ctxIO, L.ioDisplaySize + 4, 0.0f);
        HudFluf z;
        CHECK(Snap(1, z) && z.count == 0 && z.frame == 0);
        Put<float>(g_ctx + L.ctxIO, L.ioDisplaySize, -1.0f), Put<float>(g_ctx + L.ctxIO, L.ioDisplaySize + 4, -1.0f);
        CHECK(Snap(1, z) && z.count == 0 && z.frame == 0); // ImGuiIO's constructor value
        g_frame = keep;
    }

    // fails closed: every layout check broken in turn
    auto broken = [&](auto breakIt)
    {
        Context(2, false, 0, 0);
        Window(0, "fluf_cmd_palette", 700, 0, 600, 400, 100);
        Window(1, "Debug Window", 60, 60, 300, 200, 100);
        breakIt();
        HudFluf b;
        return !Snap(1, b);
    };
    CHECK(!broken([] {}));                                                            // (the intact state passes)
    CHECK(broken([] { Put<const void*>(g_ctx + L.ctxIO, L.ioCtx, nullptr); }));       // io.Ctx
    CHECK(broken([] { Put<int>(g_ctx, L.ctxFrameCount, 7); }));                       // FrameCount
    CHECK(broken([] { Put<float>(g_ctx + L.ctxIO, L.ioDisplaySize, 0.0f); }));        // DisplaySize
    CHECK(broken([] { Put<int>(g_ctx, L.ctxWindows, 9); }));                          // Size > Capacity
    CHECK(broken([] { Put<const void*>(g_win[1], L.winCtx, g_win[0]); }));            // a window's Ctx
    CHECK(broken([] { Put<const char*>(g_win[1], L.winName, nullptr); }));            // a window's Name
    CHECK(broken([] { Put<int>(g_win[1], L.winLastFrameActive, 101); }));             // LastFrameActive ahead
    {
        const fluf::ImGuiApi moved = {FakeContext, []() -> void* { return g_ctx + 52; }, FakeFrameCount};
        char why[80] = {};
        HudFluf b = {};
        CHECK(!fluf::Snapshot(moved, L, 1, b, why, sizeof(why)) && why[0]); // ImGuiIO elsewhere: another layout
    }
    CHECK(broken([] { Put<const void*>(g_ctx, L.ctxNavWindow, g_win[7]); }));        // NavWindow not a listed window

    // bus v5 + the input rules (core\fluf_rules.h; the owner's attended run 26 Oct 2026): a palette command's window
    // ("Adjust Cash": ImGui::Begin with a close button) is driven by the pad and closed by its close button; a
    // pass-through overlay (NoMouseInputs) and a background-less module text never own the pad; the Debug Window never
    {
        Context(5, false, 0, 0);
        Put<float>(g_ctx, L.ctxStyleFramePadding, 4.0f), Put<float>(g_ctx, L.ctxStyleFramePadding + 4, 3.0f);
        Window(0, "Debug Window", 60, 60, 300, 200, 100);
        Window(1, "##objective-x", 10, 400, 300, 40, 100, (1 << 9) | (1 << 0)); // NoMouseInputs | NoTitleBar
        Window(2, "##load-limit-text", 10, 500, 300, 40, 100, 1 << 7);          // NoBackground
        Window(3, "Adjust Cash", 800, 300, 400, 200, 100);
        Put<bool>(g_win[3], L.winHasCloseButton, true);
        Put<float>(g_win[3], L.winTitleBarHeight, 19.0f); // FontSize 13 + 2 x 3
        Put<float>(g_win[3], L.winSizeFull, 400.0f), Put<float>(g_win[3], L.winSizeFull + 4, 200.0f);
        Put<const void*>(g_win[3], L.winRootWindow, g_win[3]);
        Window(4, "Patch Notes##1", 100, 100, 500, 500, 90); // closed
        HudFluf s;
        HudFlufExtra x;
        char why[80] = {};
        s = {};
        CHECK(fluf::Snapshot(kApi, L, 2, s, why, sizeof(why), &x));
        CHECK(s.count == 4 && x.count == 4 && x.frame == s.frame && s.palette == 0);
        CHECK(strcmp(x.win[3].name, "Adjust Cash") == 0 && s.win[3].kind == FlufOther);
        CHECK(!fluf::TakesInput(s, &x, 0)); // Debug Window
        CHECK(!fluf::TakesInput(s, &x, 1)); // NoMouseInputs
        CHECK(!fluf::TakesInput(s, &x, 2)); // no background, not FLUF's own: FLUF passes the mouse through
        CHECK(fluf::TakesInput(s, &x, 3));
        CHECK(x.win[1].closeX < 0 && x.win[0].closeX < 0); // no close button
        CHECK_NEAR(x.win[3].closeX, (800 + 400 - 4 - 6.5f) / 2000.0f, 1e-5f);
        CHECK_NEAR(x.win[3].closeY, (300 + 3 + 6.5f) / 1000.0f, 1e-5f);
        s.present = 2, s.policy = 2; // (the live probe's fields)
        CHECK(fluf::OwnsPad(s, &x) && fluf::CloseTarget(s, &x) == 3 && !fluf::CloseByEscape(s, &x, 3));
        // without the command window: nothing owns the pad (the Debug Window stays open all the time)
        Put<int>(g_win[3], L.winLastFrameActive, 50), Put<bool>(g_win[3], L.winActive, false);
        CHECK(fluf::Snapshot(kApi, L, 2, s, why, sizeof(why), &x) && s.count == 3 && s.present == 2 &&
              !fluf::OwnsPad(s, &x));
        CHECK(fluf::CloseTarget(s, &x) == -1);
        // the policy: 0 = nothing owns the pad
        Put<int>(g_win[3], L.winLastFrameActive, 100), Put<bool>(g_win[3], L.winActive, true);
        CHECK(fluf::Snapshot(kApi, L, 0, s, why, sizeof(why), &x) && s.count == 0 && !fluf::OwnsPad(s, &x));
        s.present = 2;
        CHECK(!fluf::OwnsPad(s, &x));

        // focus: the palette and a command window both open, ImGui's focus on the palette -> B acts on the palette (Escape)
        Window(4, "fluf_cmd_palette", 700, 0, 600, 400, 100);
        Put<const void*>(g_win[4], L.winRootWindow, g_win[4]);
        Context(5, false, 0, 0);
        Put<float>(g_ctx, L.ctxStyleFramePadding, 4.0f), Put<float>(g_ctx, L.ctxStyleFramePadding + 4, 3.0f);
        Put<const void*>(g_ctx, L.ctxNavWindow, g_win[3]);
        s = {};
        CHECK(fluf::Snapshot(kApi, L, 1, s, why, sizeof(why), &x) && s.palette == 1);
        s.present = 2, s.policy = 1;
        CHECK(fluf::CloseTarget(s, &x) == 2 && x.win[2].focused && !fluf::CloseByEscape(s, &x, 2)); // (Debug left out)
        Put<const void*>(g_ctx, L.ctxNavWindow, g_win[4]);
        CHECK(fluf::Snapshot(kApi, L, 1, s, why, sizeof(why), &x));
        s.present = 2, s.policy = 1;
        CHECK(fluf::CloseTarget(s, &x) == 3 && fluf::CloseByEscape(s, &x, 3)); // the palette: FLUF's Escape

        // the keys (the owner: the open palette holds FLvibed's keys like a chat box), whatever the policy
        CHECK(fluf::Snapshot(kApi, L, 0, s, why, sizeof(why), &x) && s.palette == 1 && s.textInput == 0);
        s.present = 2;
        CHECK(fluf::HoldsKeys(s));
        s.palette = 0;
        CHECK(!fluf::HoldsKeys(s));
        s.textInput = 1;
        CHECK(fluf::HoldsKeys(s));
        s.present = 0;
        CHECK(!fluf::HoldsKeys(s)); // no FLUF: nothing held

        // an older bus (no extras): the kinds decide as before v5
        HudFluf o = {};
        o.present = 2, o.policy = 1, o.count = 1;
        o.win[0].kind = FlufOther;
        CHECK(fluf::OwnsPad(o, nullptr) && fluf::CloseByEscape(o, nullptr, 0));
        o.win[0].kind = FlufDebug;
        CHECK(!fluf::OwnsPad(o, nullptr));
    }
}
