// The strip of words: nnstrip.h. Its place: navmap_layout.h; its tabs: core\game\nntabs.h.
#include "nnstrip.h"

#include "game/deckmode.h"
#include "game/nntabs.h"
#include "game/nnwin.h"
#include "game/textfit.h"
#include "skinchip.h"
#include "ui/glyphs.h"
#include "ui/hud2_kit.h"
#include "ui/views.h"
#include "ui/winplates.h"

#include <sstream>

namespace nnstrip
{
using namespace hud2kit;

static bool g_pad = false, g_standing = false;
static navlayout::Rect g_box;
static int g_tab = -1;

// The scrim of the five windows (R8, 6.9): the full map's, set by the owner's frames.
constexpr float kScrim = 0.78f;

// A dealer shares the inventory's command and a room's window can hold the manager too: those screens have their own frame.
static bool OwnFrameShown()
{
    using namespace winplates;
    for (const Win& w : g_wins)
        if ((w.fam == kFamDealer || w.fam == kFamShipDealer || w.fam == kFamJobBoard || w.fam == kFamOffer || w.fam == FamilyIndex("news") ||
             w.fam == FamilyIndex("repair")) &&
            Shown(w.root))
            return true;
    return false;
}

static bool Wanted()
{
    return winplates::g_on && !winplates::g_scene && !winplates::g_modalOpen && !views::PanelOwnsHotspots() && nnwin::OpenTab() >= 0 && !OwnFrameShown();
}

void Update(float dt, bool pad)
{
    g_pad = pad;
    nnwin::Frame(dt);
    skinchip::Frame(g_standing);
}

void BeginPass()
{
    g_standing = false;
    skinchip::HotBegin();
}

float ScrimAlpha() { return Wanted() ? kScrim : 0; }

static void Press(int tab) { nnwin::Go(nntabs::kTabs[tab].cmd); }
static void CloseChip(int) { nnwin::Close(); }

bool Draw()
{
    using namespace navlayout;
    if (!Wanted())
        return false;
    const float W = winplates::g_W, H = winplates::g_H;
    const bool mp = nnwin::Multiplayer();
    StripIn in;
    in.W = W, in.H = H, in.side = deckmode::SideW(), in.laneTop = deckmode::LaneTopH(), in.laneBottom = deckmode::LaneBottomH();
    in.floorH = textfit::g_floorH, in.tabs = nntabs::Count(mp), in.bumpers = g_pad;
    const float glyphH = 0.032f * H;
    in.bumperW = g_pad ? GlyphWidth(glyphs::LB, glyphH) : 0;
    auto measure = [](int i, float size) { return TextWidth(nntabs::kTabs[i].word, size); };
    const float closeW = TextWidth(L"CLOSE", kWordH * H);
    const StripOut o = Strip(in, measure, closeW);
    g_box = o.box;
    g_tab = nntabs::IndexOf(nnwin::Held() ? nnwin::Held() : nnwin::OpenCommand(), mp);
    Plate(o.box.x0, o.box.y0, o.box.x1, o.box.y1, 0.008f * H, kTL | kBR, 0.80f, kChrome, 0.45f);
    if (g_pad)
    {
        const float cy = (o.box.y0 + o.box.y1) / 2;
        Glyph(glyphs::LB, o.lbX, cy, glyphH, Argb(kChrome, 1));
        Glyph(glyphs::RB, o.rbX, cy, glyphH, Argb(kChrome, 1));
    }
    const int hover = skinchip::Hovered();
    for (int i = 0; i < in.tabs; ++i)
    {
        const int slot = skinchip::Hot(o.word[i], Press, i);
        skinchip::Tab(o.word[i], nntabs::kTabs[i].word, o.size, i == g_tab, slot >= 0 && slot == hover);
    }
    if (!g_pad) // (the pad has its B pill there)
    {
        const int slot = skinchip::Hot(o.close, CloseChip, 0);
        skinchip::Chip(o.close, L"CLOSE", o.size, slot >= 0 && slot == hover ? skinchip::Look::Hover : skinchip::Look::Plain);
        winplates::g_keep.push_back({o.close.x0, o.close.y0, o.close.x1, o.close.y1});
    }
    winplates::g_keep.push_back({o.box.x0, o.box.y0, o.box.x1, o.box.y1});
    g_standing = true;
    return true;
}

bool Rect(float out[4])
{
    if (!g_standing)
        return false;
    out[0] = g_box.x0, out[1] = g_box.y0, out[2] = g_box.x1, out[3] = g_box.y1;
    return true;
}

void Shutdown()
{
    g_standing = false;
    skinchip::GiveBack();
    nnwin::SetSlideSeconds(0);
}

std::string Command(std::istream& in)
{
    std::string what;
    in >> what;
    if (what == "go") // go <0..6>: a press on that tab, as a click
    {
        int t = -1;
        in >> t;
        if (t < 0 || t >= nntabs::Count(nnwin::Multiplayer()))
            return "nnstrip go <0.." + std::to_string(nntabs::Count(nnwin::Multiplayer()) - 1) + ">";
        return nnwin::Go(nntabs::kTabs[t].cmd) ? "nnstrip: pressed" : "nnstrip: no window open";
    }
    if (what == "step") // step <-1|1>: the bumpers' press
    {
        int d = 1;
        in >> d;
        return nnwin::Step(d) ? "nnstrip: stepped" : "nnstrip: no window open";
    }
    if (what == "close")
        return nnwin::Close() ? "nnstrip: closing" : "nnstrip: no window open";
    if (what == "slide") // slide <seconds>: each NeuroNet slide, 0 the game's (a probe's: no setting asks for it yet)
    {
        float s = 0;
        in >> s;
        const bool ok = nnwin::SetSlideSeconds(s);
        return std::string(ok ? "nnstrip slide " : "nnstrip slide refused, reads ") + std::to_string(nnwin::SlideSeconds());
    }
    std::ostringstream o;
    float r[4] = {};
    const bool up = Rect(r);
    o << "nnstrip standing=" << up << " tab=" << g_tab << " open=0x" << std::hex << nnwin::OpenCommand() << " held=0x" << nnwin::Held() << std::dec
      << " mp=" << nnwin::Multiplayer() << " pad=" << g_pad << " slide=" << nnwin::SlideSeconds() << " rect=" << r[0] << "," << r[1] << "," << r[2] << "," << r[3] << " "
      << skinchip::Status();
    return o.str();
}
} // namespace nnstrip
