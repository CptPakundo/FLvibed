// FLvibed - the ship dealer page's description (audit 70b F3c; basepage.cpp's ship dealer as it was): it claims the game's
// NN_ShipTrader and keeps it off the screen by its own setter for as long as the page stands (the window does the work; its 3D
// view comes back to the screen's middle: the shell, shiptradescreen::ViewBack); its model is the window's ships and the
// chosen one's facts (core\game\screens\shiptrade_screen.*). Its input is its own (pagerun.h PageInput), two sides as the
// dealers' (the owner, 7 Nov 2026, issue 41): D-pad left the list, where the stick and D-pad up / down step the ships; D-pad
// right the ship, where the stick turns it and D-pad up / down scroll its card; the triggers zoom it on either side; a held A
// buys it (the docked pages' one hold); the wheel scrolls the card unless the mouse is over the ship; a click takes a row.
#include "shiptrade_page.h"

#include "../pagelist.h"
#include "../pagerun.h"
#include "game/screens/shiptrade_screen.h"
#include "ui/glyphs.h"

namespace shiptradepage
{
using shiptradescreen::Model;

static void Init(void* m) { new (m) Model(); }
static void Done(void* m) { static_cast<Model*>(m)->~Model(); }
static bool Read(void* m, u32 root) { return shiptradescreen::Read(*static_cast<Model*>(m), root); }
static void Prepare(void* m, u32 root) { shiptradescreen::ReadFrame(*static_cast<Model*>(m), root); }
static void Visit(const void* m, page::FieldVisitor& v)
{
    const Model& r = *static_cast<const Model*>(m);
    v.Field("ships", static_cast<double>(r.offers.size()));
    v.Field("sel", r.sel);
    v.Field("side", r.focus);
}

// A ship chosen: its card from its top, the game's frame of it clicked (the window's choice is read soon after).
static void Pick(Model& m, int i)
{
    m.cardFirst = 0;
    pagerun::Act(shiptradescreen::kActPick, i);
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
    // one side's words at a time, and only the D-pad direction that goes over (the dealers' rule)
    if (m.focus == 0)
    {
        add(DU, L"SHIP", DD);
        if (m.sel >= 0)
            add(DR, L"VIEW SHIP");
    }
    else
    {
        add(DL, L"SHIP LIST");
        add(RS, L"TURN");
        if (m.cardLines > m.cardRoom)
            add(DU, L"SCROLL CARD", DD);
    }
    if (shiptradescreen::Zoomable())
        add(LT, L"ZOOM", RT);
    if (shiptradescreen::CanBuy(m))
        add(A, L"BUY THIS SHIP", -1, true, pageinput::HoldProgress(pagerun::PurchaseHold()));
    return n;
}

static unsigned Pad(void* model, unsigned pressed, unsigned held, float dt)
{
    Model& m = *static_cast<Model*>(model);
    if (pressed & XINPUT_GAMEPAD_DPAD_LEFT)
        m.focus = 0;
    if ((pressed & XINPUT_GAMEPAD_DPAD_RIGHT) && m.sel >= 0)
        m.focus = 1;
    const int n = static_cast<int>(m.offers.size());
    const int rs = (held & views::kPagePadRsUp) ? -1 : (held & views::kPagePadRsDown) ? 1 : 0;
    if (m.focus == 0)
    {
        int step = (pressed & XINPUT_GAMEPAD_DPAD_UP) ? -1 : (pressed & XINPUT_GAMEPAD_DPAD_DOWN) ? 1 : 0;
        if (rs != m.rsWas) // (a ship a push, then repeats while it is held, as a list's stick does)
            step = rs, m.rsRepeat = 0.35f;
        else if (rs && (m.rsRepeat -= dt) <= 0)
            step = rs, m.rsRepeat = 0.12f;
        if (step < 0 && m.sel > 0)
            Pick(m, m.sel - 1);
        if (step > 0 && m.sel + 1 < n)
            Pick(m, m.sel + 1);
    }
    else
    {
        if (pressed & XINPUT_GAMEPAD_DPAD_DOWN)
            m.cardFirst = std::min(m.cardFirst + 3, m.cardLines);
        if (pressed & XINPUT_GAMEPAD_DPAD_UP)
            m.cardFirst = std::max(m.cardFirst - 3, 0);
    }
    float raw[6] = {};
    u32 buttons = 0;
    if (views::padflight.Raw)
        views::padflight.Raw(raw, &buttons);
    if (m.focus == 1)
        shiptradescreen::Turn(raw[2], raw[3], dt);
    shiptradescreen::Zoom(raw[4], raw[5], dt);
    m.rsWas = rs;
    const bool hold = (held & XINPUT_GAMEPAD_A) != 0; // (the docked pages' one hold: pagerun::PurchaseHold)
    if (pageinput::StepHoldIf(pagerun::PurchaseHold(), hold, dt, shiptradescreen::CanBuy(m)))
        pagerun::Act(shiptradescreen::kActBuy, 0);
    return XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT | XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_X | XINPUT_GAMEPAD_Y;
}

// The wheel over the ship is the game's 3D view's (its zoom), anywhere else the card's - and kept from the view: it zoomed as
// the card scrolled (issue 41).
static void Wheel(void* model, int notches)
{
    Model& m = *static_cast<Model*>(model);
    const bool over = shiptradescreen::OverShip();
    shiptradescreen::HoldWheel(!over);
    if (!over)
        m.cardFirst -= notches;
}

// A click on a row: that ship.
static void Hit(void* model, int action, int arg, bool)
{
    if (action == shiptradescreen::kHitRow)
        Pick(*static_cast<Model*>(model), arg);
}

static const page::PageInput kInput = {Prompts, Pad, Wheel, Hit, Prepare};

const page::PageDesc kShipTrade = {
    {kId, "dealers", page::Kind::Page, {shiptradescreen::kWindowClass, shiptradescreen::kWindowName, nullptr, nullptr}, page::Park::Away,
     page::Pause::None, nullptr, 0, nullptr, 0, shiptradescreen::kActions, shiptradescreen::kActionCount, nullptr, nullptr, 0, &kInput},
    {sizeof(Model), Init, Read, shiptradescreen::Stamp, Visit, nullptr, 0, Done},
    View,
};
PAGE_REGISTER(kShipTrade);
} // namespace shiptradepage
