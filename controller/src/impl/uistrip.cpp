// Extended HUD 2's docked overlay (uistrip.h; docs\UI_HUD2_EXTENDED.md 10, the mock-up tools\uimock\hud2x.py Screen.strip /
// card / word / deal): the context strip top left (base, room, system; credits, cargo, ship; a purchase the game allows
// as yellow deltas), the character card on a room character's hover (the game's own name card text), and the decision
// numbers beside the windows' native controls (the price marks' words, the equipment compare line, the ship dealer's
// refusal / trade-in / net, the job's type, the offer's giver). Everything from core\game\baseinfo (the game's data,
// fails closed); positions from the native controls' rectangles (ui::ControlRect) and fractions of H. The readers run at
// most 10 Hz (Update); Draw only draws the cached lines - a handful of text quads in one hud2kit pass.
#include "uistrip.h"

#include "debug.h"
#include "flufaware.h"
#include "host.h"
#include "hud2_kit.h"
#include "padbases.h"
#include "log.h"
#include "options_page.h"
#include "osk.h"
#include "ui.h"
#include "uiopt.h"
#include "uiskin.h"
#include "flexe.h"
#include "game/baseinfo.h"
#include "game/render.h"
#include "game/room.h"
#include "text.h"

#include <sstream>

namespace uistrip
{
namespace
{
using namespace hud2kit;

constexpr u32 kGood = 0x60D660, kFair = 0xE8D25C, kHigh = 0xEC6048; // the mock's GOOD / FAIR / HIGH
constexpr u32 kNumber = 0x68A0C8; // CHROME one step dimmer: a decision number is a label, not a headline (critique 2, 2)
constexpr float kRefresh = 0.1f;  // the readers at most 10 Hz

std::vector<DrawnRect> g_drawnRects; // this frame's drawings (pixels): the prompt lane's keep-outs (Drawn)
std::vector<DrawnRect> g_flufLast;   // the last drawn frame's (FLUF's windows over them dim the overlay: Draw)

struct Box // a room character's box on screen (pixels)
{
    float x0, y0, x1, y1;
};
std::vector<Box> OtherBoxes(const std::vector<void*>& chars, void* self, float W, float H);

struct Word // one decision number: text, anchor (pixels), alignment, colour
{
    std::wstring text;
    float x = 0, y = 0, maxW = 0;
    Align align = Right;
    u32 rgb = kNumber;
};

struct State
{
    bool on = false, show = false, scene = false, window = false; // window: one of the base's windows is open
    float clock = 0;
    // the strip
    std::wstring base, room, system, ship;
    long long credits = -1;
    int cargoUsed = -1, cargoCap = -1;
    std::wstring dCredits, dCargo; // the pending purchase's deltas ("" none)
    float barLeft = -1;            // the room bar's left edge (pixels), -1 unknown
    // the card
    bool card = false;
    baseinfo::Card c;
    float cardX = 0, cardY = 0; // the game's card's top-left (its ToolTip control's position)
    bool haveBox = false;       // the hovered character's own box on screen (baseinfo::CharacterScreenRect), pixels
    float boxX0 = 0, boxY0 = 0, boxX1 = 0, boxY1 = 0;
    std::vector<Box> others; // the other characters' boxes then (the card keeps off them too)
    float drawnX = 0, drawnY = 0; // where the card went (state report)
    const char* placed = "-";     // how: above / right / left / below / game
    u32 tipCtl = 0;             // that ToolTip control (uiskin hides it under the card)
    std::wstring offer;         // the pad's offer word for it (padbases::HoveredOffer; "" none)
    bool story = false;         // the story character: the yellow star
    bool talk = false;          // a conversation runs: no card
    // the decision numbers
    std::vector<Word> words;
    std::string why; // why nothing shows (state)
    // drawn
    float bottom = 0;
    float stripRight = 0; // pixels, this frame (0 = no strip)
    int drawn = 0, refreshes = 0;
} g;

std::wstring Money(long long v)
{
    std::wstring d = std::to_wstring(v < 0 ? -v : v), out;
    for (size_t i = 0; i < d.size(); ++i)
    {
        if (i && (d.size() - i) % 3 == 0)
            out += L',';
        out += d[i];
    }
    return L"$" + out;
}

std::wstring Upper(std::wstring s)
{
    for (wchar_t& ch : s)
        ch = static_cast<wchar_t>(towupper(ch));
    return s;
}

bool CtlRect(u32 ctl, ui::Item& it) { return ctl && ui::ControlRect(ctl, it) && it.hw > 0 && it.hh > 0; }

// The game's name card's top-left (pixels): the `ToolTip` control keeps its position as its frame's translation
// (+0x3C frame, its translation at +0x60, UI camera space; measured 25 Oct 2026 at 4K: (-0.0846, 0.0686, -1) = the drawn
// box's corner 1684, 889) - its +0xA8 is not a position in this class (+0xAC holds 0.625), which ui::ControlRect adds:
// the card went off the top of the screen (y -864) and was clamped over the room bar.
// Under ui_style 0 the per-frame hover code (0x444C09..) derives the position from the character's box each frame (x, y -
// 30 px) and re-places the ToolTip whenever that moves (0x4BE370, room camera sway: a single write held 0.5-1.3 s) - so
// NativeCard writes +0x60 every frame before the UI pass, which the ToolTip's draw projects (padbases' offer label follows
// its corner through this read).
bool TipCorner(u32 tip, float& x, float& y)
{
    float p[3] = {};
    __try
    {
        memcpy(p, reinterpret_cast<const void*>(tip + 0x60), sizeof(p));
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
    return p[2] < 0 && ui::Project(p[0], p[1], p[2], x, y);
}

// The window a control belongs to by the names up its parent chain (padbases SideOf): 1 the dealer's, 2 the player's.
int SideOf(u32 ctl)
{
    for (int guard = 0; ctl && guard < 12; ++guard)
    {
        const char* n = ui::ControlName(ctl);
        if (strcmp(n, "NN_Dealer") == 0)
            return 1;
        if (strcmp(n, "NN_Inventory") == 0)
            return 2;
        u32 parent = 0;
        __try
        {
            parent = Mem<u32>(ctl + 4);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return 0;
        }
        ctl = parent;
    }
    return 0;
}

// FLvibed's own modals over the base: the FLVIBED options page (standalone or hosted) and the on-screen keyboard - the
// same set padbases treats as modal (OwnModalOpen). Cheap flags, so Draw checks them every frame too (no 0.1 s lag).
bool OwnModalOpen() { return options_page::IsOpen() || osk::IsOpen(); }

// A modal menu over the base (pause, save / load, options, confirm, FLvibed's own): the strip hides (UI_HUD2_EXTENDED.md 10).
bool ModalOpen()
{
    if (OwnModalOpen())
        return true;
    for (const baseinfo::Ctl& c : baseinfo::Visible())
        if (exe::VtPauseMenu.is(c.vt) || exe::VtGameLoadScreen.is(c.vt) || exe::VtPrefs.is(c.vt) || exe::VtConfirm.is(c.vt))
            return true;
    return false;
}

float Letterbox()
{
    if (!exe::LetterboxAmount.ok())
        return 0;
    __try
    {
        return Mem<float>(exe::LetterboxAmount);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

// ---- the decision numbers, each from its window's native controls
void DealWords(float H)
{
    static const char* kMarks[] = {"ItemGoodDeal01", "ItemGoodDeal02", "ItemGoodDeal03", "ItemGoodDeal04", "ItemGoodDeal05"};
    for (const char* name : kMarks)
        for (int nth = 1; nth <= 2; ++nth)
        {
            const u32 mark = baseinfo::Find(name, nth);
            if (!mark)
                break;
            const baseinfo::Deal d = baseinfo::DealOf(mark);
            ui::Item r;
            if (d == baseinfo::DealUnknown || !CtlRect(mark, r))
                continue;
            Word w;
            w.text = d == baseinfo::DealGood ? L"GOOD" : d == baseinfo::DealFair ? L"FAIR" : d == baseinfo::DealHigh ? L"HIGH" : L"LOW";
            w.rgb = d == baseinfo::DealGood ? kGood : d == baseinfo::DealFair ? kFair : kHigh;
            // the price text sits left of the square on its line: the word goes on the row's name line above it,
            // right-aligned with the square (the row's ItemName0N gives that line; else just above the square)
            char nameCtl[16] = "ItemName0";
            nameCtl[9] = name[13], nameCtl[10] = 0;
            ui::Item nr;
            const u32 nm = baseinfo::Find(nameCtl, nth);
            w.x = r.x + r.hw;
            const bool sameRow = nm && CtlRect(nm, nr) && nr.y < r.y && r.y - nr.y < H * 0.06f && fabsf(nr.x - r.x) < H * 0.4f;
            w.y = sameRow ? nr.y : r.y - r.hh - H * 0.016f;
            w.align = Right;
            g.words.push_back(w);
        }
}

std::wstring Percent(float a, float b) // a vs b as "+18 %", "=" within half a percent
{
    if (!(a > 0) || !(b > 0))
        return L"";
    const int p = static_cast<int>(lroundf((a / b - 1) * 100));
    if (p == 0)
        return L"=";
    return (p > 0 ? L"+" : L"\u2212") + std::to_wstring(p < 0 ? -p : p) + L" %";
}

void CompareLine(float H)
{
    baseinfo::Gun s, m;
    if (!baseinfo::CompareSelected(s, m))
        return;
    // in the transfer panel under the item it shows (TransferItemView): a gun is bought one at a time, so the panel has
    // no quantity slider there - a short column, one value per line, left-aligned with the item
    ui::Item item;
    if (!CtlRect(baseinfo::Find("TransferItemView"), item) && !CtlRect(baseinfo::Find("TransferItemButton"), item))
        return;
    if (baseinfo::Find("QuantitySlider"))
        return; // (not a single item: the slider's place)
    std::vector<std::wstring> lines;
    lines.push_back(L"VS MOUNTED " + Upper(m.name));
    // per second, not per shot (critique 3, 6: "DAMAGE +500 %" for a gun with +43 % hull DPS misled): hull DPS, shield
    // DPS, energy per second, projectile speed, range - each a signed delta from the game's own archetype numbers
    // (each with its priority: what does not fit above the game's "Quantity:" line drops, the lowest first)
    struct Line
    {
        std::wstring text;
        int prio;
    };
    std::vector<Line> cand;
    auto add = [&](const wchar_t* label, const std::wstring& v, int prio)
    {
        if (!v.empty())
            cand.push_back({std::wstring(label) + v, prio});
    };
    add(L"HULL DPS ", Percent(s.hullDps, m.hullDps), 5);
    add(L"SHIELD DPS ", Percent(s.shieldDps, m.shieldDps), 4);
    add(L"ENERGY / S ", Percent(s.energyPs, m.energyPs), 2);
    add(L"SPEED ", Percent(s.speed, m.speed), 1);
    if (s.range > 0 && m.range > 0)
    {
        const int dr = static_cast<int>(lroundf(s.range - m.range));
        cand.push_back({L"RANGE " + (dr == 0 ? std::wstring(L"=") : (dr > 0 ? L"+" : L"\u2212") + std::to_wstring(dr < 0 ? -dr : dr) + L" M"), 3});
    }
    if (cand.empty())
        return;
    // the room down to the game's "Quantity:" line (QuantityText: a text control's anchor = its box's top)
    float top = item.y + item.hh + H * 0.012f, bottom = top + H * 0.032f * (cand.size() + 1);
    ui::Item q;
    if (ui::ControlRect(baseinfo::Find("QuantityText"), q) && q.y > top)
        bottom = std::min(bottom, q.y - H * 0.004f);
    const size_t fit = static_cast<size_t>(std::max(0.0f, (bottom - top) / (H * 0.024f)));
    while (cand.size() + 1 > fit && cand.size() > 1)
    {
        size_t low = 0;
        for (size_t i = 1; i < cand.size(); ++i)
            if (cand[i].prio < cand[low].prio)
                low = i;
        cand.erase(cand.begin() + low);
    }
    for (const Line& l : cand)
        lines.push_back(l.text);
    const float pitch = Clampf((bottom - top) / lines.size(), H * 0.024f, H * 0.032f);
    float y = top + pitch * 0.5f;
    for (const std::wstring& l : lines)
    {
        Word w;
        w.text = l;
        w.x = item.x - item.hw;
        w.y = y;
        w.align = Left;
        w.maxW = item.hw * 2; // the item row's width (the panel's inner width at every aspect)
        g.words.push_back(w);
        y += pitch;
    }
}
void ShipLines(float H)
{
    baseinfo::ShipOffer o;
    if (!baseinfo::SelectedShip(o))
        return;
    ui::Item low;
    bool have = false;
    for (int i = 0; i < 3; ++i)
    {
        char name[40];
        sprintf_s(name, "ShipTraderShipNameFrame%d", i);
        ui::Item r;
        if (CtlRect(baseinfo::Find(name), r) && (!have || r.y > low.y))
            low = r, have = true;
    }
    if (!have)
        return;
    const float right = low.x + low.hw;
    float y = low.y + low.hh + H * 0.032f;
    if (o.refusal)
    {
        Word w; // the game's own refusal first, in amber (design 10: the refusal beside TRADE-IN / NET, then no delta)
        // (critique 3, 6's duplicate was the EQUIPMENT compare's; the ship dealer keeps its NEEDS LEVEL n - FIX2)
        if (o.refusal == 1 && o.level >= 0)
            w.text = L"NEEDS LEVEL " + std::to_wstring(o.level);
        else if (o.refusal == 1)
            w.text.clear();
        else if (o.refusal == 3 && o.price > o.budget)
            w.text = L"NEEDS " + Money(o.price - o.budget) + L" MORE";
        else
            w.text = L"NEEDS REPUTATION"; // IDS 458782 "You must have a reputation of at least %d%% ..." (no value held)
        if (!w.text.empty())
        {
            w.x = right, w.y = y, w.rgb = kWarn, w.align = Right, w.maxW = H * 0.6f;
            g.words.push_back(w);
            y += H * 0.035f;
        }
    }
    if (o.credits >= 0 && o.budget >= o.credits && o.price >= 0)
    {
        const long long tradeIn = o.budget - o.credits, net = o.price - tradeIn;
        Word w;
        // the trade-in as the game counts it for this purchase: the ship and its equipment (+0x3A4 - credits; the panel's
        // own price is the hull's alone)
        w.text = L"TRADE-IN " + Money(tradeIn) + L"  \u00b7  NET " + (net < 0 ? L"+" : L"") + Money(net);
        w.x = right, w.y = y, w.align = Right, w.maxW = H * 0.6f;
        g.words.push_back(w);
        if (!o.refusal)
            g.dCredits = (net >= 0 ? L"\u2212" : L"+") + Money(net);
    }
}

void JobLine(float H)
{
    // the selected job's type: the detail icon's own hint (MissionVendorIcon, IDS 13050.. "Kill Ships", the word the game
    // shows on the rows' hover); under the detail's reward, right-aligned with the ACCEPT JOB button (the reward text
    // control has no extent: its anchor point gives the line). The difficulty the game writes into the detail itself.
    const u32 reward = baseinfo::Find("MissionVendorMissionReward");
    ui::Item r, accept;
    if (!reward || !ui::ControlRect(reward, r) || !CtlRect(baseinfo::Find("MissionVendorAcceptButton"), accept))
        return;
    const std::wstring type = baseinfo::HintText(baseinfo::Find("MissionVendorIcon"));
    if (type.empty())
        return;
    Word w;
    w.text = Upper(type);
    w.x = accept.x + accept.hw;
    w.y = r.y + H * 0.036f;
    w.align = Right;
    g.words.push_back(w);
}
void OfferLine(float H, const std::vector<void*>& chars)
{
    // the offer's giver: the conversation partner (+0x36C) - the player's own character talks too but carries no
    // affiliation line - right-aligned at the top of the offer's body (beside "Difficulty / Reward", whose right side the
    // game leaves empty), faction over name. Anchors: the close button's right edge, the title control's point (a text
    // control: its box's top-left, no extent)
    // (a mission offer only - its ACCEPT button: the rumour / info window is the same dialog with CLOSE alone, and its
    // text fills the body's top where the line sits - it ran over the rumour, ROOMS track 25 Oct 2026)
    const u32 title = baseinfo::Find("OfferDialogAcceptButton") ? baseinfo::Find("OfferDialogTitle") : 0;
    ui::Item r, close;
    if (!title || !ui::ControlRect(title, r) || !CtlRect(baseinfo::Find("OfferDialogCloseButton"), close))
        return;
    for (void* o : chars)
    {
        baseinfo::Card c;
        if (!baseinfo::CharacterTalking(o) || !baseinfo::CharacterCard(o, c) || c.faction.empty())
            continue;
        Word f, n;
        f.text = Upper(c.faction);
        n.text = Upper(c.name);
        f.x = n.x = close.x + close.hw - H * 0.012f;
        f.y = r.y + H * 0.088f;
        n.y = f.y + H * 0.032f;
        f.align = n.align = Right;
        f.maxW = n.maxW = H * 0.40f;
        f.rgb = c.attitude == 1 ? kFriend : c.attitude == -1 ? kHostile : kNumber;
        n.rgb = kInk;
        g.words.push_back(f);
        g.words.push_back(n);
        return;
    }
}
std::vector<void*> Characters()
{
    std::vector<void*> out;
    if (!HostAtLeast(5) || !g_host->GetRoomCharacters)
        return out;
    void* objs[128];
    const int n = g_host->GetRoomCharacters(objs, 128);
    out.assign(objs, objs + std::max(0, std::min(n, 128)));
    return out;
}

void Read()
{
    ++g.refreshes;
    g.words.clear();
    g.card = false;
    g.haveBox = false;
    g.tipCtl = 0;
    g.dCredits.clear(), g.dCargo.clear();
    g.why.clear();
    int sw = 0, sh = 0;
    if (!render::ScreenSize(sw, sh) || sh < 100)
    {
        g.show = false, g.why = "no screen";
        return;
    }
    const float H = static_cast<float>(sh);
    baseinfo::Refresh();
    if (!baseinfo::Docked())
    {
        g.show = false, g.why = "not docked";
        return;
    }
    if (ModalOpen())
    {
        g.show = false, g.why = "modal menu";
        return;
    }
    // a room scene (letterboxed): the strip stands down; the offer dialog the scene brings up still gets its line
    g.scene = Letterbox() > 0.001f;
    g.show = true;
    if (g.scene)
    {
        g.why = "scene: offer line only";
        OfferLine(H, Characters());
        return;
    }
    g.window = baseinfo::Find("NN_Dealer") || baseinfo::Find("NN_ShipTrader") || baseinfo::Find("NN_MissionVendor") ||
               baseinfo::Find("NN_News") || baseinfo::Find("ScriptSequence_OfferDialog") || baseinfo::Find("NN_Repair") ||
               baseinfo::Find("NeuroNetStoryStar") || baseinfo::Find("NN_PlayerInfo") || baseinfo::Find("NN_Help");
    g.base = baseinfo::BaseName();
    g.room = baseinfo::RoomName();
    g.system = baseinfo::SystemName();
    g.ship = baseinfo::ShipName();
    g.credits = baseinfo::Credits();
    if (!baseinfo::Cargo(g.cargoUsed, g.cargoCap))
        g.cargoUsed = g.cargoCap = -1;
    // the room bar's left edge: its icons (ButtonControlN) - the frame reaches ~0.025 H beyond them
    g.barLeft = -1;
    for (const baseinfo::Ctl& c : baseinfo::Visible())
        if (strncmp(c.name, "ButtonControl", 13) == 0)
        {
            ui::Item r;
            if (CtlRect(c.ctl, r) && r.y < H * 0.15f && (g.barLeft < 0 || r.x - r.hw < g.barLeft))
                g.barLeft = r.x - r.hw;
        }
    if (g.barLeft >= 0)
        g.barLeft -= H * 0.025f;
    // a purchase the game allows
    baseinfo::Transfer t;
    if (baseinfo::PendingPurchase(t) && t.unitPrice >= 0)
    {
        g.dCredits = L"\u2212" + Money(llround(static_cast<double>(t.unitPrice) * t.qty));
        if (t.volume > 0)
            g.dCargo = L"+" + std::to_wstring(static_cast<int>(lroundf(t.volume * t.qty)));
    }
    // the decision numbers
    DealWords(H);
    CompareLine(H);
    ShipLines(H);
    JobLine(H);
    const std::vector<void*> chars = Characters();
    OfferLine(H, chars);
    // the card: a hovered room character while no window is over the room and no conversation runs (a character
    // talking to the player, the room's scene: the room layer stands down, ROOMS track 25 Oct 2026)
    g.talk = room::NavBusy() == "scene";
    for (void* o : chars)
        g.talk = g.talk || baseinfo::CharacterTalking(o);
    float tipX = 0, tipY = 0;
    const u32 tipCtl = g.window || g.talk ? 0 : baseinfo::Find("ToolTip");
    // (a corner off the screen = the game's box is not on screen either: no card then, fails closed)
    if (tipCtl && TipCorner(tipCtl, tipX, tipY) && tipX >= 0 && tipY >= 0 && tipX < sw && tipY < sh)
        for (void* o : chars)
            if (baseinfo::CharacterHovered(o) && baseinfo::CharacterCard(o, g.c))
            {
                g.card = true;
                g.tipCtl = tipCtl;
                g.cardX = tipX, g.cardY = tipY;
                float bx = 0, by = 0, bw = 0, bh = 0;
                g.haveBox = baseinfo::CharacterScreenRect(o, bx, by, bw, bh);
                g.boxX0 = bx, g.boxY0 = by, g.boxX1 = bx + bw, g.boxY1 = by + bh;
                g.others = OtherBoxes(chars, o, static_cast<float>(sw), static_cast<float>(sh));
                const wchar_t* w = padbases::HoveredOffer();
                g.offer = w ? w : L"";
                g.story = padbases::HoveredIsStory();
                break;
            }
}

// ---- drawing
// HUD 2's soft scrim: a navy wash with soft edges (rings of falling alpha around the core)
void Scrim(float x0, float y0, float x1, float y1, float H, float alpha)
{
    // the mock's Gaussian blur of 0.022 H: the core fades out over ~0.045 H through 12 rings (each adds its share)
    const float inset = H * 0.022f; // the blur eats into the box as much as it spreads beyond it
    x0 += inset, y0 += inset, x1 -= inset, y1 -= inset;
    if (x1 <= x0 || y1 <= y0)
        return;
    Rect(x0, y0, x1 - x0, y1 - y0, Argb(kNavy, alpha));
    const int rings = 12;
    const float step = H * 0.004f;
    for (int i = 1; i <= rings; ++i)
    {
        const float t = static_cast<float>(i) / (rings + 1);
        const float a = alpha * (1.0f - t) * (1.0f - t), d = step * i, e = step;
        Rect(x0 - d, y0 - d, x1 - x0 + 2 * d, e, Argb(kNavy, a));         // top
        Rect(x0 - d, y1 + d - e, x1 - x0 + 2 * d, e, Argb(kNavy, a));     // bottom
        Rect(x0 - d, y0 - d + e, e, y1 - y0 + 2 * d - 2 * e, Argb(kNavy, a)); // left
        Rect(x1 + d - e, y0 - d + e, e, y1 - y0 + 2 * d - 2 * e, Argb(kNavy, a)); // right
    }
}

struct Seg
{
    std::wstring text;
    float size;
    u32 rgb;
    float track;
    float gapBefore;
};

float SegsWidth(const std::vector<Seg>& v)
{
    float w = 0;
    for (const Seg& s : v)
        w += s.gapBefore + TextWidth(s.text, s.size, s.track);
    return w;
}

float DrawSegs(const std::vector<Seg>& v, float x, float y)
{
    for (const Seg& s : v)
    {
        x += s.gapBefore;
        x += hud2kit::Text(x, y, s.text, s.size, Argb(s.rgb, 1), Left, s.track);
    }
    return x;
}

void DrawStrip(float W, float H, float k)
{
    const float x = W * 0.05f, y = H * 0.05f;
    const float budget = g.barLeft > 0 ? g.barLeft - x - H * 0.03f : W * 0.5f - H * 0.224f - x - H * 0.03f;
    if (budget < H * 0.15f)
        return;
    const float sBase = H * 0.036f * k, sRoom = H * 0.026f * k, sSys = H * 0.024f * k, sLab = H * 0.024f * k,
                sVal = H * 0.030f * k;
    // line 1: base, room, system - wrapping room + system to their own line, then dropping the system
    std::vector<std::vector<Seg>> lines;
    const std::wstring base = Upper(g.base);
    std::vector<Seg> l1;
    if (!base.empty())
    {
        // the fit rule (hud2kit::FitText): the headline steps down toward the 0.024 H floor before it is cut ("PLANET
        // MANHAT..." at 4:3 in the vanilla run)
        const textfit::Result& r = hud2kit::FitText(base, sBase, budget);
        l1.push_back({r.text, r.size, kInk, 0, 0});
    }
    std::vector<Seg> rs;
    if (!g.room.empty())
        rs.push_back({g.room, sRoom, kActive, 0.08f, 0});
    if (!g.system.empty())
        rs.push_back({Upper(g.system), sSys, kChrome, 0.06f, rs.empty() ? 0 : H * 0.014f});
    if (!rs.empty())
    {
        std::vector<Seg> joined = l1;
        for (size_t i = 0; i < rs.size(); ++i)
        {
            Seg s = rs[i];
            if (i == 0 && !joined.empty())
                s.gapBefore = H * 0.016f;
            joined.push_back(s);
        }
        std::vector<Seg> noSystem(joined.begin(), joined.end() - (rs.size() > 1 ? 1 : 0));
        if (SegsWidth(joined) <= budget)
            l1 = joined;
        else if (g.window) // a window over the room: no third line (it would run into the window) - the system drops
        {
            if (SegsWidth(noSystem) <= budget)
                l1 = noSystem;
        }
        else // the room and the system wrap to their own line; the system drops when even that is too wide
        {
            if (SegsWidth(rs) > budget && rs.size() > 1)
                rs.pop_back();
            lines.push_back(l1);
            l1 = rs;
        }
    }
    lines.push_back(l1);
    // line 2: CREDITS / CARGO / SHIP (labels CHROME, values INK, deltas yellow); the ship drops first
    auto item = [&](std::vector<Seg>& v, const wchar_t* lab, const std::wstring& val, const std::wstring& delta)
    {
        v.push_back({lab, sLab, kChrome, 0.06f, v.empty() ? 0 : H * 0.032f});
        v.push_back({val, sVal, kInk, 0, H * 0.010f});
        if (!delta.empty())
            v.push_back({delta, sVal, kActive, 0, H * 0.012f});
    };
    std::vector<Seg> l2, l3;
    if (g.credits >= 0)
        item(l2, L"CREDITS", Money(g.credits), g.dCredits);
    if (g.cargoCap > 0)
        item(l2, L"CARGO", std::to_wstring(g.cargoUsed) + L" / " + std::to_wstring(g.cargoCap), g.dCargo);
    std::vector<Seg> withShip = l2;
    if (!g.ship.empty())
        item(withShip, L"SHIP", g.ship, L"");
    if (SegsWidth(withShip) <= budget)
        l2 = withShip;
    else if (SegsWidth(l2) > budget && l2.size() > 3 && g.window) // no third line over a window: the cargo drops
        l2.resize(l2.size() - (g.dCargo.empty() ? 2 : 3));
    else if (SegsWidth(l2) > budget && l2.size() > 3) // credits and cargo on their own lines
    {
        const size_t cargoAt = l2.size() - (g.dCargo.empty() ? 2 : 3);
        l3.assign(l2.begin() + cargoAt, l2.end());
        l3[0].gapBefore = 0;
        l2.resize(cargoAt);
    }
    if (!l2.empty())
        lines.push_back(l2);
    if (!l3.empty())
        lines.push_back(l3);
    // the scrim behind, then the lines (line 1 centred 0.022 H below the margin, then 0.056 H apart as the mock)
    float wMax = 0;
    for (const auto& l : lines)
        wMax = std::max(wMax, SegsWidth(l));
    const float pitch = H * 0.050f * k;
    const float y0 = y + H * 0.022f * k, yEnd = y0 + pitch * (lines.size() - 1) + H * 0.024f * k;
    Scrim(x - H * 0.030f, y - H * 0.026f, x + wMax + H * 0.036f, yEnd + H * 0.030f, H, 0.62f);
    g_drawnRects.push_back({x - H * 0.030f, y - H * 0.026f, x + wMax + H * 0.036f, yEnd + H * 0.030f});
    for (size_t i = 0; i < lines.size(); ++i)
        DrawSegs(lines[i], x, y0 + pitch * i + (i > 0 ? H * 0.006f * k : 0));
    g.bottom = (yEnd + H * 0.012f) / H;
    g.stripRight = x + wMax + H * 0.036f;
}

// A five-pointed star, filled (the story character's mark, as the game's own yellow star).
void Star(float cx, float cy, float r, u32 argb)
{
    float px[10], py[10];
    for (int i = 0; i < 10; ++i)
    {
        const float a = -1.5707963f + i * 0.62831853f, rr = (i & 1) ? r * 0.42f : r;
        px[i] = cx + cosf(a) * rr, py[i] = cy + sinf(a) * rr;
    }
    for (int i = 0; i < 10; i += 2)
        Tri(px[(i + 9) % 10], py[(i + 9) % 10], px[i], py[i], px[i + 1], py[i + 1], argb);
    float inner[10];
    for (int i = 0; i < 5; ++i)
        inner[2 * i] = px[2 * i + 1], inner[2 * i + 1] = py[2 * i + 1];
    Poly(inner, 5, argb);
}

// The card's place from the hovered character's own box (the owner, 25 Oct 2026: "no character card hides the character
// themselves on-screen"): above the head, centred on the character, a small gap; else beside the character on the side
// facing the screen centre (then the other side, then under the feet) - never over the character's box, inside the
// 0.05 W / 0.05 H safe area, clear of the strip and of uiskin's keep-outs this frame (the room bar, its sub-row, LB / RB,
// the NeuroNet box, the windows' plates - all collected by its geometry pass before the overlay; treated as reaching up
// to the top edge, as the bar and the box do). No search: the box was read with the card (Read, 10 Hz).
// The rules for any card of w x h pixels and a character box (pixels): uistrip's own card (PlaceCard) and the game's own
// ToolTip under ui_style 0 (NativeCard). `strip`: uistrip's strip is a keep-out too.
// The other characters on screen (critique 4: the card's end touched a neighbouring patron's head in the bar): among the
// places those hard rules allow - above the head (centred, slid sideways off a neighbour while still over the character's
// centre, lifted by up to 0.08 H), beside on either side, under the feet - the card takes the one that covers the least of
// them (a neighbour's head - the top 22 % of its box - weighs 20 times its body), then the least moved from the centred
// place above the head, the side places costing a quarter card more (the rules' order when nothing is covered).
static bool PlaceRules(float bx0, float by0, float bx1, float by1, bool strip, float w, float h, float W, float H, float& x,
                       float& y, const char*& placed, const std::vector<Box>* others);

bool PlaceBy(float bx0, float by0, float bx1, float by1, bool strip, float w, float h, float W, float H, float& x, float& y,
             const char*& placed, const std::vector<Box>* others = nullptr)
{
    return PlaceRules(bx0, by0, bx1, by1, strip, w, h, W, H, x, y, placed, others);
}

// The on-screen boxes of the room's characters other than `self` (pixels; off-screen or empty ones left out).
std::vector<Box> OtherBoxes(const std::vector<void*>& chars, void* self, float W, float H)
{
    std::vector<Box> out;
    for (void* o : chars)
    {
        float bx = 0, by = 0, bw = 0, bh = 0;
        if (o == self || !baseinfo::CharacterScreenRect(o, bx, by, bw, bh) || bw <= 0 || bh <= 0 || bx >= W || by >= H ||
            bx + bw <= 0 || by + bh <= 0)
            continue;
        out.push_back({bx, by, bx + bw, by + bh});
    }
    return out;
}

static bool PlaceRules(float bx0, float by0, float bx1, float by1, bool strip, float w, float h, float W, float H, float& x,
                       float& y, const char*& placed, const std::vector<Box>* others)
{
    const float gap = H * 0.012f, sx0 = W * 0.05f, sy0 = H * 0.05f, sx1 = W * 0.95f, sy1 = H * 0.95f;
    // how much of the other characters the card covers (pixels squared, a half gap around each): heads 20x, bodies 1x
    auto covered = [&](float cx, float cy)
    {
        float s = 0;
        if (others)
            for (const Box& o : *others)
            {
                const float ox0 = o.x0 - gap * 0.5f, ox1 = o.x1 + gap * 0.5f, oy0 = o.y0 - gap * 0.5f, oy1 = o.y1 + gap * 0.5f;
                const float headY1 = o.y0 + (o.y1 - o.y0) * 0.22f + gap * 0.5f;
                const float ix = std::min(cx + w, ox1) - std::max(cx, ox0);
                if (ix <= 0)
                    continue;
                const float iy = std::min(cy + h, oy1) - std::max(cy, oy0);
                const float ih = std::min(cy + h, headY1) - std::max(cy, oy0);
                if (iy > 0)
                    s += ix * iy;
                if (ih > 0)
                    s += 19 * ix * ih;
            }
        return s;
    };
    auto overlaps = [](float ax0, float ay0, float ax1, float ay1, float cx0, float cy0, float cx1, float cy1)
    { return ax0 < cx1 && cx0 < ax1 && ay0 < cy1 && cy0 < ay1; };
    // the lowest edge (pixels) the card must stay under in its x range: the keep-outs reaching below `top`, the strip
    auto under = [&](float cx0, float cx1, float top)
    {
        float low = uiskin::LowestKeepOut(cx0, cx1, top);
        if (strip && g.stripRight > 0 && g.bottom > 0 && cx0 < g.stripRight && g.bottom * H > top)
            low = std::max(low, g.bottom * H);
        return low > top ? low + gap : 0.0f;
    };
    auto fits = [&](float cx, float cy)
    {
        return cx >= sx0 - 0.5f && cx + w <= sx1 + 0.5f && cy >= sy0 - 0.5f && cy + h <= sy1 + 0.5f &&
               !overlaps(cx, cy, cx + w, cy + h, bx0 - gap * 0.5f, by0 - gap * 0.5f, bx1 + gap * 0.5f, by1 + gap * 0.5f) &&
               under(cx, cx + w, cy) == 0;
    };
    // the candidates in the rules' order; the best = the least covered, then the least cost (the first on a tie)
    bool have = false;
    float best = 0;
    const float x1st = Clampf((bx0 + bx1 - w) / 2, sx0, sx1 - w), y1st = by0 - gap - h, mid = (bx0 + bx1) / 2;
    auto consider = [&](float cx, float cy, const char* how, float extra)
    {
        if (!fits(cx, cy))
            return;
        const float cost = covered(cx, cy) + (fabsf(cx - x1st) + fabsf(cy - y1st)) * h * 0.3f + extra;
        if (!have || cost < best)
            have = true, best = cost, x = cx, y = cy, placed = how;
    };
    // 1 above the head, centred (slid sideways into the safe area, still above the box); with neighbours also slid off
    // each of them (still over the character's centre) and lifted (up to 0.08 H), each at the first height that clears
    consider(x1st, y1st, "above", 0);
    if (have && best <= 0)
        return true; // (nothing covered, nothing moved: the common case without a search)
    if (others)
    {
        std::vector<float> xs{x1st}, ys{y1st};
        for (const Box& o : *others)
        {
            for (const float cx : {o.x1 + gap * 0.5f, o.x0 - gap * 0.5f - w})
                if (cx <= mid && cx + w >= mid)
                    xs.push_back(Clampf(cx, sx0, sx1 - w));
            const float ly = o.y0 - gap * 0.5f - h - 1; // above its head
            if (ly < y1st && ly >= y1st - H * 0.08f)
                ys.push_back(ly);
        }
        for (const float cx : xs)
            for (const float cy : ys)
                consider(cx, cy, "above", 0);
    }
    // 2 / 3 beside, head high (pushed down under a keep-out, kept inside the safe area), the side facing the centre first
    const bool rightFirst = mid < W / 2;
    for (int i = 0; i < 2; ++i)
    {
        const bool right = rightFirst == (i == 0);
        const float cx = right ? bx1 + gap : bx0 - gap - w;
        float cy = Clampf(by0, sy0, sy1 - h);
        const float down = under(cx, cx + w, cy);
        if (down > 0)
            cy = std::min(down, sy1 - h);
        consider(cx, cy, right ? "right" : "left", w * h * 0.25f * (i + 1));
    }
    // 4 under the feet
    consider(x1st, by1 + gap, "below", w * h * 0.75f);
    return have;
}

bool PlaceCard(float w, float h, float W, float H, float& x, float& y)
{
    return g.haveBox && PlaceBy(g.boxX0, g.boxY0, g.boxX1, g.boxY1, true, w, h, W, H, x, y, g.placed, &g.others);
}

// ---- the game's own name card under ui_style 0 (NativeCard)
// The ToolTip (vt 0x5D5F7C) is placed by the hovered character's per-frame code (0x444C09: the box's x, y - 30 px ->
// 0x4BE370 -> 0x4BDEA0, which re-places it through 0x4BD880 / 0x4BDA70 -> vt[0x14] only when its text or the passed
// position changed) - and its draw (0x4BDFE0) projects +0x60 itself each frame (0x4BE0B2) with its size in pixels at
// +0xB4 / +0xB8 (the text extents, written by the draw; the box adds a few pixels of padding). So a write of +0x60 after
// the frame update and before the UI pass is what the draw uses, every frame, whatever the game re-placed meanwhile.
struct Native
{
    u32 tip = 0;      // the ToolTip control (cached: found at most 10 Hz)
    void* who = nullptr; // the hovered room character
    double nextFind = 0;
    int frames = 0, writes = 0, refused = 0;
    float x = 0, y = 0, w = 0, h = 0;  // last placement (pixels: the card's top-left and size as placed)
    float box[4] = {};                 // the character's box then (x0, y0, x1, y1)
    const char* placed = "-";
    float lastAt[2] = {}; // where the game last put the card (its passed position, pixels; state report)
    int hovered = 0;      // characters claiming the hover at the last find
} g_native;

// (at most 10 Hz) the shown ToolTip and the hovered room character - none under a modal menu (the pause menu's button
// help is the same ToolTip) or while a conversation runs
void NativeFind()
{
    g_native.tip = 0, g_native.who = nullptr;
    baseinfo::Refresh();
    if (!baseinfo::Docked() || ModalOpen())
        return;
    const u32 tip = baseinfo::Find("ToolTip");
    if (!tip)
        return;
    for (void* o : Characters())
        if (baseinfo::CharacterTalking(o))
            return; // a conversation: the game's own place
    // the character whose hover code placed the card last: the ToolTip keeps the position it was passed at +0x13C (x, y,
    // z: UI camera space; 0x4BD8AA), which the hover code derives from the box's top-left (x, y - 30 px) - the nearest
    // hovered box wins (two characters can claim the hover: the cursor's and the pad's)
    float last[3] = {}, lx = 0, ly = 0;
    const bool haveLast = debug::SafeRead(tip + 0x13C, last, sizeof(last)) && ui::Project(last[0], last[1], last[2], lx, ly);
    float best = 1e30f;
    g_native.lastAt[0] = lx, g_native.lastAt[1] = ly, g_native.hovered = 0;
    for (void* o : Characters())
    {
        float bx = 0, by = 0, bw = 0, bh = 0;
        if (!baseinfo::CharacterHovered(o))
            continue;
        ++g_native.hovered;
        const float d = haveLast && baseinfo::CharacterScreenRect(o, bx, by, bw, bh) ? fabsf(bx - lx) + fabsf(by - 30 - ly) : 1e29f;
        if (d < best)
            best = d, g_native.tip = tip, g_native.who = o;
    }
}

// The game's card's place by the rules, clear of the other characters too (their boxes read per frame: a few reads each;
// apart from NativeCard, whose __try allows no objects to unwind).
bool PlaceNative(float bx0, float by0, float bx1, float by1, float w, float h, float W, float H, float& x, float& y)
{
    const std::vector<Box> others = OtherBoxes(Characters(), g_native.who, W, H);
    return PlaceBy(bx0, by0, bx1, by1, false, w, h, W, H, x, y, g_native.placed, &others);
}
} // namespace

void NativeCard(bool scene)
{
    ++g_native.frames;
    if (scene)
    {
        g_native.tip = 0, g_native.who = nullptr, g_native.placed = "-";
        return;
    }
    const double now = static_cast<double>(GetTickCount64()) / 1000.0;
    // the hover's start / end is found at most 10 Hz; a cached hover is re-checked each frame by two byte reads
    if (!g_native.tip || now >= g_native.nextFind)
    {
        if (now >= g_native.nextFind)
            g_native.nextFind = now + kRefresh, NativeFind();
        if (!g_native.tip)
            return;
    }
    const u32 tip = g_native.tip;
    u8 st = 0;
    float sz[2] = {}, p[3] = {}, bx = 0, by = 0, bw = 0, bh = 0;
    int sw = 0, sh = 0;
    if (!baseinfo::CharacterHovered(g_native.who) || !debug::SafeRead(tip + 0x6C, &st, 1) || !(st & 2) ||
        !debug::SafeRead(tip + 0xB4, sz, sizeof(sz)) || !debug::SafeRead(tip + 0x60, p, sizeof(p)) || sz[0] <= 0 ||
        sz[1] <= 0 || p[2] > -0.01f || !render::ScreenSize(sw, sh) || sh < 100 ||
        !baseinfo::CharacterScreenRect(g_native.who, bx, by, bw, bh))
    {
        g_native.placed = "-";
        return;
    }
    u32 vt = 0;
    if (!debug::SafeRead(tip, &vt, 4) || !exe::VtToolTip.is(vt))
    {
        if (!g_native.refused++)
            Log("uistrip: the game's ToolTip 0x%08X has vt 0x%08X, not the signed VtToolTip - its card stays where the game puts it",
                tip, vt);
        g_native.tip = 0;
        return;
    }
    const float W = static_cast<float>(sw), H = static_cast<float>(sh);
    // the box's padding around the text: at 4K the drawn box measured 114 x 65 px for +0xB4 / +0xB8 = 128 x 63 ("Trent"),
    // so the text extents plus this are never under-sized
    const float pad = H * 0.004f;
    const float w = sz[0] + 2 * pad, h = sz[1] + 2 * pad;
    float x = 0, y = 0;
    if (!PlaceNative(bx, by, bx + bw, by + bh, w, h, W, H, x, y))
    {
        g_native.placed = "game";
        return;
    }
    // pixels -> UI camera space at the card's own depth (the inverse of ui::Project: linear in x / y at a fixed z)
    float cx = 0, cy = 0, ux = 0, uy = 0;
    if (!ui::Project(0, 0, p[2], cx, cy) || !ui::Project(1, 1, p[2], ux, uy) || fabsf(ux - cx) < 1e-3f || fabsf(uy - cy) < 1e-3f)
        return;
    const float nx = (x - cx) / (ux - cx), ny = (y - cy) / (uy - cy);
    __try
    {
        reinterpret_cast<float*>(tip + 0x60)[0] = nx;
        reinterpret_cast<float*>(tip + 0x60)[1] = ny;
        ++g_native.writes;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        g_native.tip = 0;
        return;
    }
    g_native.x = x, g_native.y = y, g_native.w = w, g_native.h = h;
    g_native.box[0] = bx, g_native.box[1] = by, g_native.box[2] = bx + bw, g_native.box[3] = by + bh;
}

namespace
{

void DrawCard(float W, float H, float k)
{
    const baseinfo::Card& c = g.c;
    const std::wstring top = Upper(c.role.empty() ? c.faction : c.faction + L"  \u00b7  " + c.role);
    const float sTop = H * 0.024f * k, sName = H * 0.034f * k;
    // the offer (pad mode: padbases' word for the hovered character; the story star in the game's yellow) - right-aligned
    // on the name's line, a label in the card's chrome beside the name it belongs to
    const float sOffer = H * 0.022f * k, starR = H * 0.011f * k, gap = H * 0.022f;
    const float offerW = (g.offer.empty() ? 0 : TextWidth(g.offer, sOffer, 0.08f)) +
                         (g.story ? starR * 2 + (g.offer.empty() ? 0 : H * 0.008f) : 0);
    const float w = std::max(TextWidth(c.name, sName) + (offerW > 0 ? gap + offerW : 0), TextWidth(top, sTop, 0.06f)) +
                    H * 0.05f;
    const float h = (top.empty() ? H * 0.058f : H * 0.085f) * k;
    float x0 = 0, y0 = 0;
    if (!PlaceCard(w, h, W, H, x0, y0))
    {
        // no box known / nowhere free: where the game puts its own card, clamped, never over the strip (the game puts
        // some cards in the top-left corner: Trent on the launch pad at 21:9, the native box at 5, 12)
        x0 = Clampf(g.cardX, W * 0.05f, W * 0.95f - w);
        y0 = Clampf(g.cardY, H * 0.05f, H * 0.95f - h);
        if (g.stripRight > 0 && g.bottom > 0 && x0 < g.stripRight && y0 < g.bottom * H)
            y0 = g.bottom * H + H * 0.012f;
        g.placed = "game";
    }
    g.drawnX = x0, g.drawnY = y0;
    const u32 accent = c.attitude == 1 ? kFriend : c.attitude == -1 ? kHostile : c.attitude == 0 ? kNeutral : 0;
    Plate(x0, y0, x0 + w, y0 + h, H * 0.010f, kTL | kBR, 0.92f, kChrome, 0.43f, accent ? 'l' : 0, accent);
    g_drawnRects.push_back({x0, y0, x0 + w, y0 + h, 1});
    if (!top.empty())
    {
        hud2kit::Text(x0 + H * 0.022f, y0 + H * 0.024f * k, top, sTop, Argb(kChrome, 1), Left, 0.06f);
        hud2kit::Text(x0 + H * 0.022f, y0 + H * 0.058f * k, c.name, sName, Argb(kInk, 1));
    }
    else
        hud2kit::Text(x0 + H * 0.022f, y0 + h / 2, c.name, sName, Argb(kInk, 1));
    if (offerW > 0)
    {
        const float yName = top.empty() ? y0 + h / 2 : y0 + H * 0.058f * k;
        float xr = x0 + w - H * 0.022f;
        if (!g.offer.empty())
            xr -= hud2kit::Text(xr, yName, g.offer, sOffer, Argb(kChrome, 1), Right, 0.08f) + H * 0.008f;
        if (g.story)
            Star(xr - starR, yName, starR, Argb(kActive, 1));
    }
}

void DrawWords(float H, float k)
{
    const float size = H * 0.024f * k;
    for (const Word& w : g.words)
    {
        const float tw = w.maxW > 0 ? hud2kit::TextFit(w.x, w.y, w.text, size, w.maxW, Argb(w.rgb, 1), w.align, 0.06f)
                                    : hud2kit::Text(w.x, w.y, w.text, size, Argb(w.rgb, 1), w.align, 0.06f);
        const float x0 = w.align == Right ? w.x - tw : w.align == Center ? w.x - tw / 2 : w.x;
        g_drawnRects.push_back({x0, w.y - size * 0.6f, x0 + tw, w.y + size * 0.6f, 2});
    }
}
} // namespace

void Update(float dt, bool on)
{
    if (!on)
    {
        g.on = g.show = false;
        g.bottom = 0;
        return;
    }
    if (!g.on)
        g.clock = kRefresh; // read at once when switched on
    g.on = true;
    g.clock += dt;
    if (g.clock >= kRefresh)
    {
        g.clock = 0;
        Read();
    }
}

void Draw(void* device)
{
    g.drawn = 0;
    if (!g_drawnRects.empty())
        g_flufLast = g_drawnRects; // the last drawn frame's rectangles: FLUF's keep-outs are checked against them
    g_drawnRects.clear();
    if (!g.on || !g.show || !device || OwnModalOpen())
    {
        g.bottom = 0;
        return;
    }
    render::SetDevice(device);
    int sw = 0, sh = 0;
    if (!render::ScreenSize(sw, sh) || sh < 100)
        return;
    const uiopt::UiOptions& o = uiopt::Get();
    const float k = std::min(1.3f, std::max(0.8f, o.textSize / 100.0f));
    const float W = static_cast<float>(sw), H = static_cast<float>(sh);
    if (!hud2kit::Begin(device, sw, sh, std::max(8, static_cast<int>(0.026f * H * k + 0.5f))))
        return;
    // FLUF's windows over the strip / card / decision words (flufaware.h): each steps back on its own when a window
    // covers its own rectangles of the last drawn frame (FLUF draws over FLvibed and takes the mouse there; A3 26 Oct
    // 2026: the Debug Window over the strip's left end had dimmed the card at the bottom centre too)
    bool covered[3] = {};
    for (const DrawnRect& r : g_flufLast)
        if (r.part >= 0 && r.part < 3)
            covered[r.part] = covered[r.part] || flufaware::Meets(r.x0, r.y0, r.x1, r.y1, W, H);
    // (Dim eases toward "covered": the whole screen meets a keep-out when one exists, an empty rectangle meets none)
    const flufaware::Slot slots[3] = {flufaware::SlotStrip, flufaware::SlotCard, flufaware::SlotWords};
    float fluf[3];
    for (int p = 0; p < 3; ++p)
        fluf[p] = covered[p] ? flufaware::Dim(slots[p], 0, 0, W, H, W, H) : flufaware::Dim(slots[p], 0, 0, 0, 0, W, H);
    hud2kit::SetOffset(0, 0);
    g.stripRight = 0;
    hud2kit::SetAlpha(fluf[0]);
    if (!g.scene)
        DrawStrip(W, H, k);
    else
        g.bottom = 0;
    hud2kit::SetAlpha(fluf[1]);
    if (g.card)
        DrawCard(W, H, k);
    hud2kit::SetAlpha(fluf[2]);
    DrawWords(H, k);
    hud2kit::SetAlpha(1);
    hud2kit::End();
    g.drawn = 1;
}

const std::vector<DrawnRect>& Drawn() { return g_drawnRects; }

float StripBottom() { return g.on && g.show ? g.bottom : 0; }

float StripRight() { return g.on && g.show && g.bottom > 0 ? g.stripRight : 0; }

u32 CardTooltip() { return g.on && g.show && g.card ? g.tipCtl : 0; }

bool Active() { return g.on; }

bool GameCardCorner(float& x, float& y)
{
    int sw = 0, sh = 0;
    const u32 tip = uiclick::FindVisible("ToolTip");
    return tip && render::ScreenSize(sw, sh) && TipCorner(tip, x, y) && x >= 0 && y >= 0 && x < sw && y < sh;
}

std::string Command(std::istream& in)
{
    std::string op;
    in >> op;
    std::ostringstream o;
    if (op.empty() || op == "state")
    {
        o << "uistrip on=" << g.on << " show=" << g.show << " why=" << (g.why.empty() ? "-" : g.why) << " drawn=" << g.drawn
          << " refreshes=" << g.refreshes << " bottom=" << g.bottom << " barLeft=" << g.barLeft << "\n";
        o << "strip base=\"" << text::Utf8(g.base) << "\" room=\"" << text::Utf8(g.room) << "\" system=\""
          << text::Utf8(g.system) << "\" credits=" << g.credits << " " << text::Utf8(g.dCredits) << " cargo=" << g.cargoUsed
          << "/" << g.cargoCap << " " << text::Utf8(g.dCargo) << " ship=\"" << text::Utf8(g.ship) << "\"\n";
        if (g.card)
        {
            o << "card faction=\"" << text::Utf8(g.c.faction) << "\" role=\"" << text::Utf8(g.c.role) << "\" name=\""
              << text::Utf8(g.c.name) << "\" attitude=" << g.c.attitude << " offer=\"" << text::Utf8(g.offer)
              << "\" story=" << g.story << " game_at " << g.cardX << "," << g.cardY << " drawn_at " << g.drawnX << ","
              << g.drawnY << " placed=" << g.placed << " box=";
            if (g.haveBox)
                o << g.boxX0 << "," << g.boxY0 << "," << g.boxX1 << "," << g.boxY1 << "\n";
            else
                o << "-\n";
        }
        else if (g.talk)
            o << "card none: conversation\n";
        o << "native tip=0x" << std::hex << g_native.tip << std::dec << " placed=" << g_native.placed << " at "
          << g_native.x << "," << g_native.y << " size " << g_native.w << "x" << g_native.h << " box=" << g_native.box[0]
          << "," << g_native.box[1] << "," << g_native.box[2] << "," << g_native.box[3] << " frames=" << g_native.frames
          << " writes=" << g_native.writes
          << " refused=" << g_native.refused << " hovered=" << g_native.hovered << " game_at " << g_native.lastAt[0] << ","
          << g_native.lastAt[1] << "\n";
        for (const Word& w : g.words)
            o << "word \"" << text::Utf8(w.text) << "\" at " << w.x << "," << w.y << " align=" << w.align << "\n";
        return o.str();
    }
    if (op == "data")
        return baseinfo::Status();
    if (op == "research")
    {
        std::string rest;
        std::getline(in, rest);
        return baseinfo::Research(rest);
    }
    if (op == "hover" || op == "unhover") // hover <i>: the game's own hover on room character i (room::Hover)
    {
        int i = -1;
        in >> i;
        const std::vector<void*> chars = Characters();
        if (i < 0 || i >= static_cast<int>(chars.size()))
            return "uistrip: no character " + std::to_string(i);
        const bool ok = op == "hover" ? room::Hover(chars[i]) : room::Unhover(chars[i]);
        return std::string(op) + " " + std::to_string(i) + (ok ? " ok" : " failed");
    }
    return "uistrip [state] | data | research <ids n|equip|gun a|hints> | hover <i> | unhover <i>";
}

void Shutdown() { g = State(); }
} // namespace uistrip
