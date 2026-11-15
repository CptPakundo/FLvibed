// FLvibed - the dealer page's description (audit 70b F3b; basepage.cpp's commodity, equipment and final-stage page as it was):
// it claims the game's NN_Dealer in its three modes and keeps it and its partner NN_Inventory off the screen by their own
// setters for as long as the page stands (the windows do the work); its model is the two windows' rows, their transfer panel
// and the ship's facts (core\game\screens\market_screen.*). Its input is its own (pagerun.h PageInput): the D-pad walks a
// list's rows and, left and right, the two lists (at the final stage the three stops: the new ship, BUY SHIP, the dealer); a
// held A trades or buys the ship (a hold: a tap never spends), X filters (or opens BEST PRICES at the commodity trader), Y
// takes the most, LS mounts, RS's click tags the compare, the right stick and the wheel walk the card; a click takes a row.
#include "market_page.h"

#include "../pagelist.h"
#include "../pagerun.h"
#include "game/screens/market_screen.h"
#include "ui/glyphs.h"

namespace marketpage
{
using marketscreen::Model;
using marketscreen::Row;
using marketscreen::kTheirs;
using marketscreen::kYours;

static void Init(void* m) { new (m) Model(); }
static void Done(void* m) { static_cast<Model*>(m)->~Model(); }
static bool Read(void* m, u32 root) { return marketscreen::Read(*static_cast<Model*>(m), root); }
static void Prepare(void* m, u32) { marketscreen::ReadFrame(*static_cast<Model*>(m)); }
static void Visit(const void* m, page::FieldVisitor& v)
{
    const Model& r = *static_cast<const Model*>(m);
    v.Field("mode", r.mode);
    v.Field("side", r.side);
    v.Field("yours", static_cast<double>(r.rows[kYours].size()));
    v.Field("theirs", static_cast<double>(r.rows[kTheirs].size()));
    v.Field("sel", r.list[r.side].sel);
    v.Field("buyStop", r.buyStop);
}

// The focus moved: its card from its top, the game's row selected with it (its transfer panel, its price, its button).
static void Moved(Model& m)
{
    m.cardFirst = 0;
    const int arg = m.FocusArg();
    if (arg >= 0)
        pagerun::Act(marketscreen::kActSelect, arg);
}
static void Filter(Model& m, int i)
{
    pagerun::Act(marketscreen::kActFilter, i);
    marketscreen::Filtered(m);
}
static void Mount(const Model& m)
{
    const Row* r = m.Focus();
    if (m.Mountable())
        pagerun::Act(r->mounted ? marketscreen::kActUnmount : marketscreen::kActMount, static_cast<int>(r->desc));
}
// RS's click on a mounted item: what the store's items compare with; again: back to the weakest that would go
static void ToggleTag(const Model& m)
{
    if (marketscreen::CanTag(m))
        pagerun::Act(marketscreen::kActTag, static_cast<int>(m.Focus()->arch));
}

static int Prompts(const void* model, views::PagePrompt* out, int max)
{
    using namespace glyphs;
    const Model& m = *static_cast<const Model*>(model);
    int n = 0;
    auto add = [&](int id, const wchar_t* word, int id2 = -1, bool hold = false, float progress = 0)
    {
        if (n < max)
            out[n++] = {id, id2, word, hold, progress};
    };
    const float progress = pageinput::HoldProgress(pagerun::PurchaseHold());
    const Row* r = m.Focus();
    const bool final = m.Final(), yours = m.side == kYours;
    if (final && m.buyStop)
    {
        add(DL, L"YOUR ITEMS", DR);
        add(A, L"BUY SHIP", -1, true, progress);
    }
    else
    {
        add(DU, L"ITEM", DD);
        // only the direction that goes over (the owner, 7 Nov 2026): your ship stands left, the dealer right
        add(yours ? DR : DL, final ? (marketscreen::CanBuyFinal() ? L"BUY SHIP" : yours ? L"DEALER" : L"NEW SHIP") : yours ? L"DEALER" : L"YOUR SHIP");
        if (marketscreen::CanTrade(m))
            add(A, final ? (yours ? L"SELL TO DEALER" : L"TAKE BACK") : yours ? L"SELL" : L"BUY", -1, true, progress);
        if (marketscreen::CanTrade(m) && marketscreen::Slider())
        {
            add(LT, L"QUANTITY", RT);
            add(Y, L"MAX");
        }
    }
    if (m.mode == marketscreen::kEquipment || final)
        add(X, L"FILTER");
    if (m.mode == marketscreen::kCommodity && marketscreen::RoutesStand())
        add(X, L"BEST PRICES");
    if (m.Mountable())
        add(LS, r->mounted ? L"UNMOUNT" : L"MOUNT");
    if (marketscreen::CanTag(m))
        add(R3, marketscreen::Tagged(m) ? L"CLICK UNTAG" : L"CLICK COMPARE WITH");
    if (final && marketscreen::BackStands())
        add(B, L"BACK TO SHIPS");
    return n;
}

static unsigned Pad(void* model, unsigned pressed, unsigned held, float dt)
{
    Model& m = *static_cast<Model*>(model);
    if (m.Final())
    {
        // left and right walk the three stops as they stand: the new ship, BUY SHIP, the dealer (dealer_rules.h NextStop, the pad's way before the page)
        const int was = m.buyStop ? 1 : m.side == kYours ? 0 : 2,
                  by = (pressed & (XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_LEFT_SHOULDER)) ? -1 : (pressed & (XINPUT_GAMEPAD_DPAD_RIGHT | XINPUT_GAMEPAD_RIGHT_SHOULDER)) ? 1 : 0;
        int to = std::max(0, std::min(2, was + by));
        if (to == 1 && !marketscreen::CanBuyFinal())
            to = was + 2 * by;
        if (by && to != was && to >= 0 && to <= 2)
        {
            m.buyStop = to == 1;
            if (to != 1)
                m.side = to == 0 ? kYours : kTheirs, Moved(m);
        }
    }
    if (!m.buyStop)
    {
        if (pressed & XINPUT_GAMEPAD_DPAD_UP)
            if (m.list[m.side].Step(-1))
                Moved(m);
        if (pressed & XINPUT_GAMEPAD_DPAD_DOWN)
            if (m.list[m.side].Step(1))
                Moved(m);
    }
    if (!m.Final() && (pressed & (XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_LEFT_SHOULDER)) && m.side != kYours)
        m.side = kYours, Moved(m);
    if (!m.Final() && (pressed & (XINPUT_GAMEPAD_DPAD_RIGHT | XINPUT_GAMEPAD_RIGHT_SHOULDER)) && m.side != kTheirs)
        m.side = kTheirs, Moved(m);
    if ((pressed & XINPUT_GAMEPAD_X) && (m.mode == marketscreen::kEquipment || m.Final()))
        Filter(m, (marketscreen::NowFilter(m) + 1) % m.Filters());
    if ((pressed & XINPUT_GAMEPAD_X) && m.mode == marketscreen::kCommodity)
        pagerun::Act(marketscreen::kActRoutes, 0);
    if ((pressed & XINPUT_GAMEPAD_Y) && !m.buyStop)
        pagerun::Act(marketscreen::kActMax, 0);
    if (pressed & XINPUT_GAMEPAD_LEFT_THUMB)
        Mount(m);
    if (pressed & XINPUT_GAMEPAD_RIGHT_THUMB)
        ToggleTag(m);
    // a held A trades, as a job or a ship is taken (dealer_rules.h): a tap on a row never spends; the hold is the docked pages' one
    const bool down = (held & XINPUT_GAMEPAD_A) != 0;
    if (pageinput::StepHoldIf(pagerun::PurchaseHold(), down, dt, m.buyStop || marketscreen::CanTrade(m)))
    {
        if (m.buyStop)
            pagerun::Act(marketscreen::kActFinalBuy, 0);
        else
            pagerun::Act(marketscreen::kActTrade, m.FocusArg());
    }
    // the right stick walks the card
    m.cardStep += ((held & views::kPagePadRsDown) ? dt : (held & views::kPagePadRsUp) ? -dt : 0) * 8;
    if (!(held & (views::kPagePadRsDown | views::kPagePadRsUp)))
        m.cardStep = 0;
    if (fabsf(m.cardStep) >= 1)
    {
        const int lines = static_cast<int>(m.cardStep);
        m.cardFirst += lines, m.cardStep -= static_cast<float>(lines);
    }
    return XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT | XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_X | XINPUT_GAMEPAD_Y |
           XINPUT_GAMEPAD_LEFT_SHOULDER | XINPUT_GAMEPAD_RIGHT_SHOULDER | XINPUT_GAMEPAD_LEFT_THUMB | XINPUT_GAMEPAD_RIGHT_THUMB;
}

// The wheel: on a list it scrolls that list, on the card the card (where the mouse stood when the view last drew).
static void Wheel(void* model, int notches)
{
    Model& m = *static_cast<Model*>(model);
    if (notches && m.wheelSide >= 0)
        m.list[static_cast<int>(m.wheelSide)].Scroll(-notches);
    else if (notches && m.wheelSide == -1)
        m.cardFirst -= notches;
}

// The clicks that need the model: a row takes the focus there; a tab filters; the trade, mount and tag chips act on the focus.
static void Hit(void* model, int action, int arg, bool)
{
    Model& m = *static_cast<Model*>(model);
    switch (action)
    {
    case marketscreen::kHitRow:
        m.side = arg >= 1000 ? kTheirs : kYours;
        m.list[m.side].sel = arg % 1000;
        m.list[m.side].Fit();
        Moved(m);
        break;
    case marketscreen::kHitTab: Filter(m, arg); break;
    case marketscreen::kHitTrade: pagerun::Act(marketscreen::kActTrade, m.FocusArg()); break;
    case marketscreen::kHitMount: Mount(m); break;
    case marketscreen::kHitTag: ToggleTag(m); break;
    default: break;
    }
}

static const page::PageInput kInput = {Prompts, Pad, Wheel, Hit, Prepare};

const page::PageDesc kMarket = {
    {kId, "dealers", page::Kind::Page, {marketscreen::kWindowClass, marketscreen::kWindowName, nullptr, marketscreen::kPartnerClass}, page::Park::Away,
     page::Pause::None, nullptr, 0, nullptr, 0, marketscreen::kActions, marketscreen::kActionCount, nullptr, nullptr, 0, &kInput},
    {sizeof(Model), Init, Read, marketscreen::Stamp, Visit, nullptr, 0, Done},
    View,
};
PAGE_REGISTER(kMarket);
} // namespace marketpage
