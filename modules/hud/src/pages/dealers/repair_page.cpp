// FLvibed - the repair page's description (audit 70b F3a; basepage.cpp's repair as it was): it claims the game's NN_Repair
// window and keeps it off the screen by its own setter for as long as the page stands (the window does the work); its
// model is the window's rows and sum and the ship's facts (core\game\screens\repair_screen.*). Its input is its own
// (page.h PageInput): the D-pad walks the rows, A ticks, X ticks all or none, a held Y repairs (a hold: a tap never spends),
// the right stick and the wheel walk the card; a click takes a row and on the row in focus ticks it. No click spends.
#include "repair_page.h"

#include "../pagelist.h"
#include "../pagerun.h"
#include "game/screens/repair_screen.h"
#include "ui/glyphs.h"

namespace repairpage
{
using repairscreen::Model;

static void Init(void* m) { new (m) Model(); }
static void Done(void* m) { static_cast<Model*>(m)->~Model(); }
static bool Read(void* m, u32 root) { return repairscreen::Read(*static_cast<Model*>(m), root); }
static void Visit(const void* m, page::FieldVisitor& v)
{
    const Model& r = *static_cast<const Model*>(m);
    v.Field("rows", static_cast<double>(r.rows.size()));
    v.Field("sel", r.sel);
    v.Field("total", static_cast<double>(r.total));
    v.Field("canMend", r.canMend);
}

static pageinput::HoldState g_repairHold; // Y: a repair spends, so it is a hold with a fresh press (controller's dealer_rules.h)

// A row's tick, the one in focus (the window's own toggle).
static void Tick(const Model& m)
{
    if (m.sel < static_cast<int>(m.rows.size()))
        pagerun::Act(repairscreen::kActTick, m.rows[m.sel].slot);
}
// The focus moved to row `i`: its card from its top, its card and compare read at once.
static void Focus(Model& m, int i)
{
    m.sel = i, m.cardFirst = 0;
    repairscreen::Refocus(m);
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
    if (m.rows.size() > 1)
        add(DU, L"ITEM", DD);
    if (!m.rows.empty())
    {
        add(A, m.rows[m.sel].ticked ? L"UNTICK" : L"TICK");
        add(X, L"TICK ALL / NONE");
    }
    if (m.canMend)
        add(Y, L"REPAIR", -1, true, pageinput::HoldProgress(g_repairHold));
    if (m.cardLines > m.cardRoom)
        add(RS, L"SCROLL CARD");
    return n;
}

static unsigned Pad(void* model, unsigned pressed, unsigned held, float dt)
{
    Model& m = *static_cast<Model*>(model);
    const int n = static_cast<int>(m.rows.size());
    if ((pressed & XINPUT_GAMEPAD_DPAD_UP) && m.sel > 0)
        Focus(m, m.sel - 1);
    if ((pressed & XINPUT_GAMEPAD_DPAD_DOWN) && m.sel + 1 < n)
        Focus(m, m.sel + 1);
    if (pressed & XINPUT_GAMEPAD_A)
        Tick(m);
    if (pressed & XINPUT_GAMEPAD_X)
        pagerun::Act(repairscreen::kActAll, 0);
    // a repair spends: a hold, with a fresh press (dealer_rules.h); A only ticks
    if (pageinput::StepHold(g_repairHold, (held & XINPUT_GAMEPAD_Y) != 0, dt))
        pagerun::Act(repairscreen::kActRepair, 0);
    m.cardFirst += (held & views::kPagePadRsDown) && m.cardFirst < m.cardLines ? 1 : 0;
    m.cardFirst -= (held & views::kPagePadRsUp) ? 1 : 0;
    return XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT | XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_X | XINPUT_GAMEPAD_Y;
}

static void Wheel(void* model, int notches) { static_cast<Model*>(model)->cardFirst -= notches; }

// A click takes the row, and on the row in focus ticks it (with the mouse's form, at once); no click here spends.
static void Hit(void* model, int action, int arg, bool pad)
{
    Model& m = *static_cast<Model*>(model);
    if (action != repairscreen::kHitRow || arg < 0 || arg >= static_cast<int>(m.rows.size())) // (a click on a row the last read took away)
        return;
    const bool again = arg == m.sel;
    Focus(m, arg);
    if (again || !pad)
        Tick(m);
}

static const page::PageInput kInput = {Prompts, Pad, Wheel, Hit};

const page::PageDesc kRepair = {
    {kId, "dealers", page::Kind::Page, {repairscreen::kWindowClass, repairscreen::kWindowName, nullptr, nullptr}, page::Park::Away,
     page::Pause::None, nullptr, 0, nullptr, 0, repairscreen::kActions, repairscreen::kActionCount, nullptr, nullptr, 0, &kInput},
    {sizeof(Model), Init, Read, repairscreen::Stamp, Visit, nullptr, 0, Done},
    View,
};
PAGE_REGISTER(kRepair);
} // namespace repairpage
