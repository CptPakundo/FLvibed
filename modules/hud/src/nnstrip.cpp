// The strip of words: nnstrip.h. Its place: navmap_layout.h; its tabs: core\game\nntabs.h.
#include "nnstrip.h"

#include "game/baseinfo.h"
#include "game/deckmode.h"
#include "game/nntabs.h"
#include "game/nnwin.h"
#include "game/room.h"
#include "game/textfit.h"
#include "skinchip.h"
#include "str.h"
#include "ui/glyphs.h"
#include "ui/hud2_kit.h"
#include "ui/ui.h"
#include "ui/views.h"
#include "ui/winplates.h"

#include <sstream>

namespace nnstrip
{
using namespace hud2kit;

static bool g_pad = false, g_standing = false;
static navlayout::Rect g_box;
static int g_tab = -1;
static float g_in = 0, g_dt = 0; // the slide in: 0 at the top left, 1 in place
static bool g_roomUp = false;    // the room's words stand this frame: the click box is theirs too, and the pages' chips ride on it
static float g_left = 0, g_foot = 0, g_leftNext = 0, g_footNext = 0, g_read = 1;
constexpr float kSlideIn = 0.30f;

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

// The log after a death or a failed mission (single player): no tab leads anywhere from it, its page is nnpage's (issue 50).
static bool DeathShown() { return !nnwin::Multiplayer() && uiclick::FindVisible("LoadAutosaveButton"); }

bool Wanted()
{
    return winplates::g_on && !winplates::g_scene && !winplates::g_modalOpen && !views::PanelOwnsHotspots() && nnwin::OpenTab() >= 0 && !OwnFrameShown() && !DeathShown();
}

void Update(float dt, bool pad)
{
    g_pad = pad;
    g_dt = dt;
    g_read += dt;
    nnwin::Frame(dt);
    skinchip::Frame(g_standing || g_roomUp);
}

void BeginPass()
{
    g_standing = g_roomUp = false;
    g_left = g_leftNext, g_foot = g_footNext, g_leftNext = g_footNext = 0;
    skinchip::HotBegin();
}

float ScrimAlpha() { return Wanted() ? kScrim : 0; }

static void Press(int tab) { nnwin::Go(nntabs::kTabs[tab].cmd); }
static void CloseChip(int) { nnwin::Close(); }

bool Draw()
{
    using namespace navlayout;
    if (!Wanted())
    {
        g_in = 0;
        return false;
    }
    const float W = winplates::g_W, H = winplates::g_H;
    const bool mp = nnwin::Multiplayer();
    // along the top of the screen, centred: the bottom lane is the prompts' (the owner, 7 Nov 2026). It slides in from the flight HUD's NEURAL NET at the top left, its words fading up
    StripIn in;
    in.W = W, in.H = H, in.side = deckmode::SideW(), in.laneTop = deckmode::EdgeH(), in.laneBottom = deckmode::EdgeH() + (deckmode::LaneBottomH() - deckmode::LaneTopH());
    in.floorH = textfit::g_floorH, in.tabs = nntabs::Count(mp), in.bumpers = g_pad;
    const float glyphH = 0.032f * H;
    in.bumperW = g_pad ? GlyphWidth(glyphs::LB, glyphH) : 0;
    auto measure = [](int i, float size) { return TextWidth(nntabs::kTabs[i].word, size); };
    const float closeW = TextWidth(L"CLOSE", kWordH * H);
    StripOut o = Strip(in, measure, closeW);
    const float shift = (W - o.box.W()) / 2 - o.box.x0;
    o.box.x0 += shift, o.box.x1 += shift, o.lbX += shift, o.rbX += shift;
    for (int i = 0; i < in.tabs; ++i)
        o.word[i].x0 += shift, o.word[i].x1 += shift;
    o.close.y0 = deckmode::LaneTopH() * H, o.close.y1 = deckmode::LaneBottomH() * H;
    g_box = o.box;
    g_tab = nntabs::IndexOf(nnwin::Held() ? nnwin::Held() : nnwin::OpenCommand(), mp);
    g_in = std::min(1.0f, g_in + g_dt / kSlideIn);
    const float rest = 1 - g_in, eased = 1 - rest * rest * rest;
    SetOffset(-shift * (1 - eased), 0);
    SetAlpha(eased);
    Plate(o.box.x0, o.box.y0, o.box.x1, o.box.y1, 0.008f * H, kTL | kBR, 0.80f, kChrome, 0.45f);
    if (g_pad)
    {
        const float cy = (o.box.y0 + o.box.y1) / 2;
        Glyph(glyphs::LB, o.lbX, cy, glyphH, Argb(kChrome, 1));
        Glyph(glyphs::RB, o.rbX, cy, glyphH, Argb(kChrome, 1));
        if (baseinfo::Docked()) // View closes the NeuroNet docked as in flight: its badge just left of the strip (the owner's yes to Shockwave, 7 Nov 2026)
            Glyph(glyphs::VIEW, o.box.x0 - 0.010f * H - GlyphWidth(glyphs::VIEW, glyphH) / 2, cy, glyphH, Argb(kChrome, 1));
    }
    const int hover = skinchip::Hovered();
    for (int i = 0; i < in.tabs; ++i)
    {
        const int slot = skinchip::Hot(o.word[i], Press, i);
        skinchip::Tab(o.word[i], nntabs::kTabs[i].word, o.size, i == g_tab, slot >= 0 && slot == hover);
    }
    SetOffset(0, 0);
    SetAlpha(1);
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

// ---- the room's words
struct RoomWord
{
    std::wstring word;
    std::string label;
    float x = 0; // the game's button's centre: the words stand in the buttons' order
};
static std::vector<RoomWord> g_rooms, g_acts;
static std::vector<std::pair<std::string, std::wstring>> g_said; // a label's word, read once
static std::string g_here;

// A room's word is its hotspot's label ("ShipDealer": SHIP DEALER; the game's hint names the room's character more than the room), an action's the game's own hint, else the label
static const std::wstring& Said(const room::Hotspot& h, bool action)
{
    for (const auto& s : g_said)
        if (s.first == h.label)
            return s.second;
    std::wstring w = action ? baseinfo::HintText(h.ctl) : L"";
    if (w.empty())
    {
        std::string s = h.label;
        for (const char* cut : {"IDS_HOTSPOT_", "IDS_NN_"})
            if (s.rfind(cut, 0) == 0)
                s = s.substr(strlen(cut));
        for (size_t i = 0; i < s.size(); ++i)
        {
            if (s[i] == '_' || (i && isupper(static_cast<unsigned char>(s[i])) && islower(static_cast<unsigned char>(s[i - 1]))))
                w += L' ';
            if (s[i] != '_')
                w += static_cast<wchar_t>(static_cast<unsigned char>(s[i]));
        }
    }
    g_said.push_back({h.label, str::Upper(w)});
    return g_said.back().second;
}

// At most 10 Hz, from the game's list as it stands now (the buttons are made anew per room).
static void ReadRoom()
{
    g_read = 0;
    g_rooms.clear(), g_acts.clear();
    g_here = winplates::CurrentRoomKey();
    for (const room::Hotspot& h : room::Hotspots())
    {
        winplates::Box b;
        if (!h.ctl || !winplates::FullyShown(h.ctl) || !winplates::RectOf(h.ctl, b) || !b.box)
            continue;
        const bool action = h.control.rfind("ButtonControl", 0) != 0;
        RoomWord r;
        r.word = Said(h, action), r.label = h.label, r.x = b.cx;
        (action ? g_acts : g_rooms).push_back(r);
    }
    auto byX = [](const RoomWord& a, const RoomWord& b) { return a.x < b.x; };
    std::sort(g_rooms.begin(), g_rooms.end(), byX);
    std::sort(g_acts.begin(), g_acts.end(), byX);
}

static float RowWidth(const std::vector<RoomWord>& v, float size, float gap)
{
    float w = gap * (static_cast<float>(v.size()) - 1);
    for (const RoomWord& r : v)
        w += TextWidth(r.word, size);
    return w;
}

static void Pick(const std::vector<RoomWord>& v, int i)
{
    std::string what;
    if (i >= 0 && i < static_cast<int>(v.size()))
        room::Goto(v[i].label, &what); // (busy: the game would have swallowed the click too)
}
static void RoomClick(int i) { Pick(g_rooms, i); }
static void ActClick(int i) { Pick(g_acts, i); }
static void TabClick(int i)
{
    if (!nnwin::Go(nntabs::kTabs[i].cmd))
        nnwin::Press(nntabs::kTabs[i].cmd);
}

// A row of words over [x0, x1] x [y0, y1]: the air shared out, each word's click reaching half way to the next and the row's whole height.
static void Row(const std::vector<RoomWord>& v, float x0, float x1, float y0, float y1, float size, skinchip::Action act, bool here)
{
    const float air = (x1 - x0 - RowWidth(v, size, 0)) / static_cast<float>(v.size());
    const float cy = (std::max(y0, 0.0f) + y1) / 2, h = size * 1.7f;
    const int hover = skinchip::Hovered();
    float x = x0;
    for (size_t i = 0; i < v.size(); ++i)
    {
        const float w = TextWidth(v[i].word, size);
        const int slot = skinchip::Hot({x, std::max(y0, 0.0f), x + w + air, y1}, act, static_cast<int>(i));
        skinchip::Tab({x + air / 2, cy - h / 2, x + air / 2 + w, cy + h / 2}, v[i].word, size, here && !_stricmp(v[i].label.c_str(), g_here.c_str()), slot >= 0 && slot == hover);
        x += w + air;
    }
}

void DrawRoom()
{
    using namespace navlayout;
    const winplates::DockedGeo& d = winplates::g_docked;
    if (!winplates::g_on || !d.live || winplates::g_modalOpen || winplates::g_scene)
        return;
    // every frame: at 10 Hz a room change kept the room before's words on the new bar for up to 6 frames (the owner: "frame-perfect", 7 Nov 2026); a few
    // controls' rectangles, the words cached by label
    ReadRoom();
    if (g_rooms.empty())
        return;
    const float W = winplates::g_W, H = winplates::g_H, pad = kStripPad * H, glyphH = 0.038f * H;
    const float side = g_pad ? GlyphWidth(glyphs::LB, glyphH) + pad : 0;
    // View opens the NeuroNet docked (padbases DockedView): its badge on the NeuroNet's own words, as LB / RB on the rooms' (the owner's yes to Shockwave,
    // 7 Nov 2026; the lanes are full, so not a pill)
    const float viewW = g_pad && d.nn ? GlyphWidth(glyphs::VIEW, glyphH) + pad : 0; // (at a dealer too, where the bumpers are the page's and LB / RB go)
    std::vector<RoomWord> tabs;
    for (int i = 0; d.nn && i < nntabs::Count(nnwin::Multiplayer()); ++i)
    {
        RoomWord r;
        r.word = nntabs::kTabs[i].word;
        tabs.push_back(r);
    }
    // the plates cover the game's icons and their old glass; the largest words that leave the strip its room and the NeuroNet's plate clear; else the old look stands
    const float cx = (d.bx0 + d.bx1) / 2, nativeW = d.bx1 - d.bx0 + 2 * 0.029f * H, nnRight = d.nx1 + 0.020f * H, nnNative = d.nx1 - d.nx0 + 0.046f * H;
    const float least = W * deckmode::SideW() + 0.30f * H, floorPx = textfit::g_floorH * H;
    float size = kWordH * H, barW = 0, nnW = 0;
    for (bool fits = false; !fits; size -= 0.001f * H)
    {
        const float s = std::max(size, floorPx);
        for (float gap : {kWordGap * H, kWordGapMin * H})
        {
            barW = std::max(RowWidth(g_rooms, s, gap) + 2 * (pad + side), nativeW);
            nnW = d.nn ? std::max(RowWidth(tabs, s, gap) + 2 * pad + viewW, nnNative) : 0;
            if (cx - barW / 2 >= least && (!d.nn || cx + barW / 2 + 0.012f * H <= nnRight - nnW))
            {
                fits = true;
                break;
            }
        }
        if (fits)
        {
            size = s + 0.001f * H;
            break;
        }
        if (size <= floorPx)
            return;
    }
    size -= 0.001f * H;
    const float x0 = cx - barW / 2, x1 = cx + barW / 2, y0 = -0.02f * H, y1 = std::max(d.by1 + 0.020f * H, d.nn ? d.ny1 + 0.023f * H : 0); // (one foot: the two rows of words on one line)
    float foot = y1;
    if (d.sub && !g_acts.empty())
    {
        const float scx = (d.sx0 + d.sx1) / 2, sw = std::max(RowWidth(g_acts, size, kWordGap * H) + 2 * pad, d.sx1 - d.sx0 + 2 * 0.039f * H);
        const float sx0 = scx - sw / 2, sx1 = scx + sw / 2;
        foot = std::max(d.sy1 + 0.012f * H, y1 + size * 1.7f);
        Plate(sx0, y1 - 0.01f * H, sx1, foot, 0.008f * H, kBL | kBR, 1.0f, kChrome, 0.45f);
        Row(g_acts, sx0 + pad, sx1 - pad, y1, foot, size, ActClick, false);
        winplates::g_keep.push_back({sx0, y1, sx1, foot});
    }
    Plate(x0, y0, x1, y1, 0.008f * H, kBL | kBR, 1.0f, kChrome, 0.45f);
    if (d.glyphs)
    {
        Glyph(glyphs::LB, x0 + pad + side / 2 - pad / 2, y1 / 2, glyphH, Argb(kInk, 1));
        Glyph(glyphs::RB, x1 - pad - side / 2 + pad / 2, y1 / 2, glyphH, Argb(kInk, 1));
    }
    Row(g_rooms, x0 + pad + side, x1 - pad - side, y0, y1, size, RoomClick, true);
    winplates::g_keep.push_back({x0, 0, x1, y1});
    if (d.nn)
    {
        Plate(nnRight - nnW, y0, nnRight, y1, 0.008f * H, kBL, 1.0f, kChrome, 0.45f);
        if (viewW > 0)
            Glyph(glyphs::VIEW, nnRight - nnW + pad + (viewW - pad) / 2, y1 / 2, glyphH, Argb(kInk, 1));
        Row(tabs, nnRight - nnW + pad + viewW, nnRight - pad, y0, y1, size, TabClick, false);
        winplates::g_keep.push_back({nnRight - nnW, 0, nnRight, y1});
    }
    g_leftNext = x0, g_footNext = foot, g_roomUp = true;
}

float RoomLeft() { return g_left; }
float RoomFoot() { return g_foot; }

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
    if (what == "rooms")
    {
        std::ostringstream o;
        o << "rooms up=" << g_roomUp << " left=" << g_left << " foot=" << g_foot << " here=" << g_here << "\n";
        for (const RoomWord& r : g_rooms)
            o << "room " << r.label << " \"" << str::Narrow(r.word) << "\"\n";
        for (const RoomWord& r : g_acts)
            o << "act " << r.label << " \"" << str::Narrow(r.word) << "\"\n";
        o << skinchip::Status();
        return o.str();
    }
    if (what == "hit") // hit <x> <y>: the control the game's own hit test finds there (which of two stands on top)
    {
        float x = 0, y = 0;
        in >> x >> y;
        const u32 c = static_cast<u32>(ui::HitTest(x, y));
        return std::string("hit ") + (c ? ui::ControlName(c) : "-");
    }
    if (what == "room" || what == "act" || what == "tab") // a click on that word
    {
        int i = -1;
        in >> i;
        if (what == "room")
            RoomClick(i);
        else if (what == "act")
            ActClick(i);
        else if (i >= 0 && i < nntabs::Count(nnwin::Multiplayer()))
            TabClick(i);
        return "nnstrip: clicked";
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
