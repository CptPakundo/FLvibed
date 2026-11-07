// Extended HUD 2's docked overlay (uistrip.h; modules\hud\docs\UI_HUD2_EXTENDED.md 10, the mock-up tools\uimock\hud2x.py Screen.strip /
// card / word / deal): the context strip top left (base, room, system; credits, cargo, ship; a purchase the game allows
// as yellow deltas), the character card on a room character's hover (the game's own name card text), and the decision
// numbers beside the windows' native controls (the price marks' words, the equipment compare line, the ship dealer's
// refusal / trade-in / net, the job's type, the offer's giver). Everything from core\game\baseinfo (the game's data,
// fails closed); positions from the native controls' rectangles (ui::ControlRect) and fractions of H. The readers run at
// most 10 Hz (Update); Draw only draws the cached lines - a handful of text quads in one hud2kit pass.
#include "uistrip.h"
#include "basepage.h"

#include "ui/flufaware.h"
#include "hudhost.h"
#include "nnstrip.h"
#include "nnpage.h"
#include "navnew.h"
#include "ui/hud2_kit.h"
#include "ui/views.h"
#include "safemem.h"
#include "log.h"
#include "ui/options_page.h"
#include "ui/ui.h"
#include "ui/uiopt.h"
#include "ui/gamecard.h"
#include "ui/winplates.h"
#include "uiskin.h"
#include "windowtext.h"
#include "windowtext.h"
#include "flexe.h"
#include "game/baseinfo.h"
#include "game/deckmode.h"
#include "game/render.h"
#include "game/room.h"
#include "str.h"
#include "text.h"

#include <sstream>

using namespace gamecard;
namespace uistrip
{
namespace
{
using namespace hud2kit;

constexpr u32 kGood = 0x60D660, kFair = 0xE8D25C, kHigh = 0xEC6048; // the mock's GOOD / FAIR / HIGH
constexpr u32 kNumber = 0x68A0C8; // CHROME one step dimmer: a decision number is a label, not a headline (critique 2, 2)
constexpr float kWordNameRoom = 0.185f; // a row's name line less the FAIR / GOOD word, fractions of H

std::vector<DrawnRect> g_drawnRects; // this frame's drawings (pixels): the prompt lane's keep-outs (Drawn)
std::vector<DrawnRect> g_flufLast;   // the last drawn frame's (FLUF's windows over them dim the overlay: Draw)


struct Word // one decision number: text, anchor (pixels), alignment, colour
{
    std::wstring text;
    float x = 0, y = 0, maxW = 0;
    Align align = Right;
    u32 rgb = kNumber;
    float scale = 1;
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

std::wstring Upper(std::wstring s) { return str::Upper(std::move(s)); }

bool CtlRect(u32 ctl, ui::Item& it) { return ctl && ui::ControlRect(ctl, it) && it.hw > 0 && it.hh > 0; }


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
            // a name that fills its line has no room for the word, and no smaller font to go to: the mark beside the price still says it
            if (windowtext::TextWidthPx(nm) > kWordNameRoom * H)
                continue;
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
    if (baseinfo::Find("WelcomeText"))
        return; // (nothing is selected: a highlighted row kept its comparison over the welcome text)
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
    // the column takes the panel's whole inner width, from the item's icon on ("VS MOUNTED CATAPULT
    // MIS..." was cut beside free room); a heading that still does not fit takes a second line (a name wider than the
    // whole column alone is still fitted by TextFit)
    float colX0 = item.x - item.hw, colX1 = item.x + item.hw;
    ui::Item button;
    if (CtlRect(baseinfo::Find("TransferItemButton"), button))
        colX0 = std::min(colX0, button.x - button.hw), colX1 = std::max(colX1, button.x + button.hw);
    const float wordK = std::min(1.3f, std::max(0.8f, uiopt::Get().textSize / 100.0f));
    std::vector<std::wstring> lines;
    const std::wstring head = L"VS MOUNTED " + Upper(m.name);
    if (hud2kit::TextWidth(head, H * 0.024f * wordK, 0.06f) <= colX1 - colX0)
        lines.push_back(head);
    else
    {
        lines.push_back(L"VS MOUNTED");
        lines.push_back(Upper(m.name));
    }
    const size_t heads = lines.size();
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
    float top = item.y + item.hh + H * 0.012f, bottom = top + H * 0.032f * (cand.size() + heads);
    ui::Item q;
    if (ui::ControlRect(baseinfo::Find("QuantityText"), q) && q.y > top)
        bottom = std::min(bottom, q.y - H * 0.004f);
    const size_t fit = static_cast<size_t>(std::max(0.0f, (bottom - top) / (H * 0.024f)));
    while (cand.size() + heads > fit && cand.size() > 1)
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
        w.x = colX0;
        w.y = y;
        w.align = Left;
        w.maxW = colX1 - colX0; // the panel's inner width at every aspect
        w.scale = Clampf(pitch / (H * 0.0265f), 1.0f, 1.2f); // (as large as their pitch allows: with one or two lines there is room)
        g.words.push_back(w);
        y += pitch;
    }
}
// One refusal, HUD 2's: the game's own white line in the centre panel said it again (the owner, 6 Nov 2026). Bit 1 of the control's state, the skin's way of hiding; the line comes back
// the moment HUD 2 has none, so a refusal it has no words for still shows
u32 g_shipError = 0;
void ShipError(u32 hide)
{
    u8 st = 0;
    if (g_shipError && g_shipError != hide && winplates::NameIs(g_shipError, "ErrorHelp") && winplates::R8(g_shipError + winplates::kState, st))
        winplates::W8(g_shipError + winplates::kState, st | 2);
    g_shipError = hide;
    if (hide && winplates::R8(hide + winplates::kState, st) && (st & 2))
        winplates::W8(hide + winplates::kState, st & ~2);
}

// The dealers' refusal, as the ship dealer's: HUD 2's amber line for the game's small white one (the owner, 6 Nov 2026). A level reads as there, NEEDS LEVEL n; any other keeps the game's words,
// wrapped in the panel. It stands where the game's line stood, over the item.
void DealError(float H)
{
    const u32 ctl = baseinfo::Find("ErrorHelp");
    std::wstring text;
    ui::Item at, item, icon, close;
    if (!ctl || !CtlRect(baseinfo::Find("Inventory_Close"), close) || !winplates::TextOf(ctl, text) || !ui::ControlRect(ctl, at))
    {
        windowtext::Park(0);
        return;
    }
    for (wchar_t& c : text)
        c = static_cast<wchar_t>(towupper(c));
    const size_t digit = text.find_first_of(L"0123456789");
    if (text.find(L"LEVEL") != std::wstring::npos && digit != std::wstring::npos)
    {
        const size_t end = text.find_first_not_of(L"0123456789", digit);
        text = L"NEEDS LEVEL " + text.substr(digit, end == std::wstring::npos ? end : end - digit);
    }
    const bool row = CtlRect(baseinfo::Find("TransferItemView"), item) && CtlRect(baseinfo::Find("TransferItemButton"), icon);
    const float cx = row ? (icon.x - icon.hw + item.x + item.hw) / 2 : at.x, room = row ? item.x + item.hw - (icon.x - icon.hw) : H * 0.34f;
    // between the panel's top and the item, by what that room holds: the largest size whose lines fit, centred in it (four lines at one size ran over the item)
    const float top = close.y - H * 0.030f, foot = row ? item.y - item.hh - H * 0.010f : top + H * 0.13f;
    std::vector<std::wstring> lines;
    float scale = 1.3f;
    for (;; scale -= 0.05f)
    {
        const float size = H * 0.024f * scale;
        lines.assign(1, L"");
        size_t from = 0;
        while (from < text.size())
        {
            size_t space = text.find(L' ', from);
            space = space == std::wstring::npos ? text.size() : space;
            const std::wstring word = text.substr(from, space - from);
            const std::wstring wider = lines.back().empty() ? word : lines.back() + L" " + word;
            if (hud2kit::TextWidth(wider, size, 0.06f) > room && !lines.back().empty())
                lines.push_back(word);
            else
                lines.back() = wider;
            from = space + 1;
        }
        if (static_cast<float>(lines.size()) * size * 1.25f <= foot - top || scale <= 0.8f)
            break;
    }
    const float size = H * 0.024f * scale, pitch = size * 1.25f;
    float y = (top + foot) / 2 - pitch * (static_cast<float>(lines.size()) - 1) / 2;
    for (const std::wstring& l : lines)
    {
        Word w;
        w.text = l, w.x = cx, w.y = y, w.rgb = kWarn, w.align = Center, w.maxW = room, w.scale = scale;
        g.words.push_back(w);
        y += pitch;
    }
    windowtext::Park(ctl);
}

void ShipLines(float H)
{
    winplates::SetWordsRoom(0);
    u32 hideError = 0;
    struct Apply
    {
        u32& ctl;
        ~Apply() { ShipError(ctl); }
    } applyError{hideError}; // (every way out of here)
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
    winplates::SetWordsRoom(0.046f + (o.refusal ? 0.035f : 0.0f)); // (the plate holds these lines)
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
            hideError = baseinfo::Find("ErrorHelp");
            if (!hideError)
                hideError = g_shipError; // (hidden, it is off the visible list: it stays hidden while the refusal stands)
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
    if (basepage::Hides())
        return; // (the page says it)
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
    w.y = r.y + std::max(H * 0.044f, windowtext::TextHeight(reward) + H * 0.016f); // (windowtext grows the reward)
    w.align = Right;
    w.rgb = kInk, w.scale = 1.35f; // (with the reward, what the job is: the owner, 6 Nov 2026)
    g.words.push_back(w);
}
// The offer's title this far right of where the game put it (fractions of H), through its position property as windowtext moves a text;
// the home is read once per dialog, so the step never adds up
u32 g_titleCtl = 0;
float g_titleHome = 0, g_titleShift = 0;
bool SetLocalX(u32 ctl, float x)
{
    __try
    {
        float pos[3] = {Mem<float>(ctl + 0xA8), Mem<float>(ctl + 0xAC), Mem<float>(ctl + 0xB0)};
        pos[0] = x;
        reinterpret_cast<void(__thiscall*)(u32, int, const float*, int)>(Mem<u32>(Mem<u32>(ctl) + 0xA8))(ctl, 6, pos, 0);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
bool LocalX(u32 ctl, float& x)
{
    __try
    {
        x = Mem<float>(ctl + 0xA8);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
void TitleAt(u32 title, float shift)
{
    if (title != g_titleCtl)
    {
        if (!LocalX(title, g_titleHome))
            return;
        g_titleCtl = title, g_titleShift = 0;
    }
    if (fabsf(shift - g_titleShift) < 0.002f)
        return;
    if (SetLocalX(title, g_titleHome + shift))
        g_titleShift = shift;
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
    {
        // a rumour in the same dialog: its title back home (only the control the step was made on, while it is the dialog's)
        const u32 any = baseinfo::Find("OfferDialogTitle");
        if (any && any == g_titleCtl)
            TitleAt(any, 0);
        return;
    }
    for (void* o : chars)
    {
        baseinfo::Card c;
        if (!baseinfo::CharacterTalking(o) || !baseinfo::CharacterCard(o, c) || c.faction.empty())
            continue;
        Word f, n;
        f.text = Upper(c.faction);
        n.text = Upper(c.name);
        // in the header, left of the title: over the body it ran through a bribe's text, which starts at the top (the owner's order, 7 Nov 2026)
        f.x = n.x = winplates::g_W - (close.x + close.hw - H * 0.012f);
        f.y = r.y + H * 0.002f;
        n.y = f.y + H * 0.026f;
        f.align = n.align = Left;
        // the room up to the title, measured on its own word, and the title steps right for a long name, never into the X: 0.28 H cut
        // "COORDINATOR WILLIAM CAMPBELL" with a third of the header empty (the owner, 7 Nov 2026: "push the word Mission a bit to the right")
        std::wstring word;
        const float titleHalf = winplates::TextOf(title, word) && !word.empty() ? TextWidth(word, H * 0.040f) / 2 : H * 0.07f;
        const float k = std::min(1.3f, std::max(0.8f, uiopt::Get().textSize / 100.0f)), gap = H * 0.024f;
        const float needW = std::max(TextWidth(f.text, H * 0.024f * k, 0.06f), TextWidth(n.text, H * 0.024f * k, 0.06f));
        const float centre = winplates::g_W / 2, most = (close.x - close.hw - gap - titleHalf) - centre;
        const float shift = std::max(0.0f, std::min(most, f.x + needW + gap + titleHalf - centre));
        TitleAt(title, shift / H);
        f.maxW = n.maxW = std::max(H * 0.28f, centre + shift - titleHalf - gap - f.x);
        f.rgb = c.attitude == 1 ? kFriend : c.attitude == -1 ? kHostile : kNumber;
        n.rgb = kInk;
        g.words.push_back(f);
        g.words.push_back(n);
        return;
    }
    TitleAt(title, 0); // (no giver read: the game's place)
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
    // a full-screen NeuroNet page or map has its own title where the strip stands: the game's mission briefing (the map with no tabs) drew both, SIGMA-13 over GAS
    // MINER NAHA (Shockwave, 7 Nov 2026). The map only: with every page it took the dealers' and traders' credits (Shockwave, the same day)
    if (navnew::Grown())
    {
        g.show = false, g.why = "the full-screen map";
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
    DealError(H);
    const size_t shipFrom = g.words.size();
    ShipLines(H);
    // the refusal and the trade-in stand in the centre panel: it has the room, and the list's panel ends under its rows (the owner, 6 Nov 2026)
    ui::Item close;
    if (g.words.size() > shipFrom && CtlRect(baseinfo::Find("ShipTraderCancelButton"), close))
    {
        // (under the ship's preview, whatever its size: the panel ends under them. One size for the lines, the largest the longest leaves inside the panel's edges)
        const float cx = close.x - H * 0.6075f, room = H * 0.35f;
        float widest = 1;
        for (size_t i = shipFrom; i < g.words.size(); ++i)
            widest = std::max(widest, hud2kit::TextWidth(g.words[i].text, H * 0.024f, 0.06f));
        const float scale = Clampf(room / widest, 1.0f, 1.5f), pitch = H * 0.024f * scale * 1.35f, y0 = g.words[shipFrom].y + H * 0.024f * (scale - 1) * 0.6f;
        for (size_t i = shipFrom; i < g.words.size(); ++i)
            g.words[i].x = cx, g.words[i].y = y0 + pitch * static_cast<float>(i - shipFrom), g.words[i].align = Center, g.words[i].maxW = room, g.words[i].scale = scale;
    }
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
                const wchar_t* w = views::DeviceHoveredOffer();
                g.offer = w ? w : L"";
                g.story = views::DeviceHoveredIsStory();
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
    const float x = W * deckmode::SideW(), y = H * deckmode::EdgeH();
    const float barLeft = nnstrip::RoomLeft() > 0 ? nnstrip::RoomLeft() : g.barLeft; // (the words' bar is wider than the icons')
    const float budget = barLeft > 0 ? barLeft - x - H * 0.03f : W * 0.5f - H * 0.224f - x - H * 0.03f;
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
    else if (SegsWidth(l2) > budget && l2.size() > 3 && g.window) // no third line over a window
    {
        // the price's delta drops before the cargo: the dealer's own panel shows the total, the cargo is nowhere else there
        // (CARGO went for "-$1,229" with room beside it, 1280x800, UI_HUD2.md 5.34)
        std::vector<Seg> noDelta;
        if (!g.dCredits.empty())
        {
            noDelta = l2;
            noDelta.erase(noDelta.begin() + 2);
            noDelta[2].gapBefore = H * 0.032f;
        }
        if (!noDelta.empty() && SegsWidth(noDelta) <= budget)
            l2 = noDelta;
        else
            l2.resize(l2.size() - (g.dCargo.empty() ? 2 : 3)); // (the cargo only when even that does not fit)
    }
    else if (SegsWidth(l2) > budget && l2.size() > 3) // credits and cargo on their own lines
    {
        const size_t cargoAt = l2.size() - (g.dCargo.empty() ? 2 : 3);
        l3.assign(l2.begin() + cargoAt, l2.end());
        l3[0].gapBefore = 0;
        l2.resize(cargoAt);
    }
    // (not over a NeuroNet window: the row stood on the status window's title at both sizes, and these windows say the same. The dealers keep it: their deltas are here)
    float nn[4];
    if (!l2.empty() && !nnstrip::Rect(nn))
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

bool PlaceCard(float w, float h, float W, float H, float& x, float& y)
{
    return g.haveBox && PlaceBy(g.boxX0, g.boxY0, g.boxX1, g.boxY1, g.stripRight, g.bottom, w, h, W, H, x, y, g.placed, &g.others);
}

} // namespace

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
    winplates::ClearSkinItems();
    for (const Word& w : g.words)
    {
        const float size = H * 0.024f * k * w.scale;
        const float tw = w.maxW > 0 ? hud2kit::TextFit(w.x, w.y, w.text, size, w.maxW, Argb(w.rgb, 1), w.align, 0.06f)
                                    : hud2kit::Text(w.x, w.y, w.text, size, Argb(w.rgb, 1), w.align, 0.06f);
        const float x0 = w.align == Right ? w.x - tw : w.align == Center ? w.x - tw / 2 : w.x;
        g_drawnRects.push_back({x0, w.y - size * 0.6f, x0 + tw, w.y + size * 0.6f, 2});
        winplates::AddSkinItem(x0, w.y - size * 0.6f, x0 + tw, w.y + size * 0.6f);
    }
}
} // namespace

void Update(float dt, bool on)
{
    if (!on)
    {
        g.on = g.show = false;
        g.bottom = 0;
        ShipError(0);
        windowtext::Park(0);
        winplates::ClearSkinItems();
        return;
    }
    (void)dt;
    g.on = true;
    // every frame, as a dealer always read: at 10 Hz the game's character tooltip showed before the card, the card stayed after the hover, a
    // refusal's ErrorHelp flashed and a room's strip lagged its change (the owner: "frame-perfect", 7 Nov 2026)
    g.clock = 0;
    Read();
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
        o << "native tip=0x" << std::hex << gamecard::NativeState().tip << std::dec << " placed=" << gamecard::NativeState().placed << " at "
          << gamecard::NativeState().x << "," << gamecard::NativeState().y << " size " << gamecard::NativeState().w << "x" << gamecard::NativeState().h << " box=" << gamecard::NativeState().box[0]
          << "," << gamecard::NativeState().box[1] << "," << gamecard::NativeState().box[2] << "," << gamecard::NativeState().box[3] << " frames=" << gamecard::NativeState().frames
          << " writes=" << gamecard::NativeState().writes
          << " refused=" << gamecard::NativeState().refused << " hovered=" << gamecard::NativeState().hovered << " game_at " << gamecard::NativeState().lastAt[0] << ","
          << gamecard::NativeState().lastAt[1] << "\n";
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
