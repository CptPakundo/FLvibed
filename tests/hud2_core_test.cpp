// HUD 2's game-free core (modules\hud\src\hud2_frame.h: hud2_frame.cpp, hud2_parts.cpp, hud2_target.cpp, hud2_cue.cpp,
// hud2_click_draw.cpp) linked and run with no game source (audit 71b B3c): a frame stepped and drawn from what a program
// gives it - its clocks, its settings, its hotspot sink - into the draw kit's list back end (synthetic source: block
// glyphs, disc icons). It proves the link and the inputs, not the look: the kit's lists are compared in game by the
// bench's captures.
#include "test.h"

#include "../modules/hud/src/hud2_frame.h"
#include "../tools/hudbench/kitlist.h"

namespace
{
int g_hotStarts = 0, g_hotRects = 0, g_listHead = 0;
bool g_recording = false;
u32 FixedMs() { return 123456; }
unsigned long long FixedMs64() { return 123456; }
double FixedSeconds() { return 123.456; }
void HotStart(bool on) { ++g_hotStarts, g_recording = on; }
void HotRect(int slot, float x0, float, float x1, float)
{
    if (!g_recording)
        return;
    ++g_hotRects;
    if (slot == hud2::kHotListHead && x1 > x0)
        ++g_listHead;
}
int Hovered() { return hud2::kHotListHead; }
} // namespace

void Test_hud2_core()
{
    std::string why;
    CHECK(kitlist::Open(kitlist::Source::Synthetic, L"", why) == 0);
    hud2::FrameHost host;
    host.tickMs = FixedMs, host.tickMs64 = FixedMs64, host.seconds = FixedSeconds;
    host.hotStart = HotStart, host.hotRect = HotRect, host.hotHovered = Hovered;

    // a keyboard-and-mouse flight frame, shown at once (no transition), the threat list's header standing
    static hud2::FrameState st; // (a few KB of model: off the stack)
    hud2::FrameIn in;
    in.dt = 1.0f / 60;
    in.ctx.space = true, in.ctx.pad = false;
    in.want = true;
    in.parts.opt.hudTransition = 0, in.parts.opt.hudThreats = true;
    in.parts.family = glyphs::Family::PlayStation, in.parts.opt.glyphLook = 1;
    in.screenW = 1280, in.screenH = 800;
    hud2::StartOut so;
    CHECK(hud2::StepStart(host, in, st, so) == hud2::Phase::Live);
    CHECK(so.hotOn && st.m.kbm && st.t == 1.0f);
    hud2::StepOut out;
    hud2::Step(host, in, st, out);
    CHECK(st.L.W == 1280 && st.L.H == 800 && st.L.k > 0);
    // the inputs the parts read are the ones handed in, not a game's
    CHECK_NEAR(hud2::PartsSeconds(), 123.456, 1e-3);
    CHECK(hud2::PartsGlyphs().family == glyphs::Family::PlayStation && hud2::PartsGlyphs().look == 1);
    CHECK(hud2::HotHovered() == hud2::kHotListHead);

    // hud_size handed in: Large's margins without the global (deckmode::g_on stays off)
    const float mxNormal = st.L.mx;
    hud2::FrameIn large = in;
    large.parts.size.deck = true;
    hud2::Step(host, large, st, out);
    CHECK(!deckmode::g_on && st.L.mx < mxNormal);
    hud2::Step(host, in, st, out);
    CHECK_NEAR(st.L.mx, mxNormal, 1e-4);

    // the pass: the parts into the kit's open pass, the hotspots to the sink, the cut mark at the end
    hud2::DrawIn din;
    din.parts = in.parts;
    CHECK(hud2kit::Begin(kitlist::Device(), 1280, 800, hud2::KitTextPx(st.L.H, st.L.k)));
    hud2::Draw(host, din, st);
    hud2kit::End();
    std::vector<kitlist::Pass> passes = kitlist::Take();
    CHECK(passes.size() == 1);
    if (passes.size() != 1)
        return;
    const kitlist::Pass& p = passes[0];
    size_t verts = 0;
    for (int l = 0; l < hud2kit::LCount; ++l)
        verts += p.v[l].size();
    CHECK(verts > 0);
    CHECK(!p.marks.empty() && std::string(p.marks.back().name) == "parts.end");
    CHECK(g_hotStarts == 1 && g_hotRects > 0 && g_listHead == 1);

    // an in-flight cutscene shows only the subtitles (issues 12 and 92): the chat window is held hidden (feed), also on a
    // launch from a loaded save, before any flight laid HUD 2 out (W 0: the chat window stood bare there before)
    static hud2::FrameState fresh;
    hud2::FrameIn cut;
    cut.dt = 1.0f / 60;
    cut.ctx.cutscene = true;
    cut.want = true;
    hud2::StartOut cso;
    CHECK(hud2::StepStart(host, cut, fresh, cso) == hud2::Phase::Away);
    CHECK(fresh.L.W == 0 && cso.feed);
    cut.want = false; // the classic HUD: the game's own chat window, untouched
    CHECK(hud2::StepStart(host, cut, fresh, cso) == hud2::Phase::Away && !cso.feed);
}
