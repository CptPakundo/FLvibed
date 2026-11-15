// FLvibed - the BEST PRICES page's description (audit 70b F3a; basepage.cpp's prices as it was): it stands over the game's
// commodity list by base (NN_CommodityByBase, found by its name: no class symbol yet) and keeps it off the screen by its
// own setter for as long as the page stands; its model is the window's rows, in the order that answers the question
// (core\game\screens\prices_screen.*). Its input is its own (page.h PageInput): the D-pad walks the focus row (the window
// follows), the wheel the rows' window.
#include "prices_page.h"

#include "../pagelist.h"
#include "../pagerun.h"
#include "game/screens/prices_screen.h"
#include "ui/glyphs.h"

namespace pricespage
{
using pricesscreen::Model;

static void Init(void* m) { new (m) Model(); }
static void Done(void* m) { static_cast<Model*>(m)->~Model(); }
static bool Read(void* m, u32 root) { return pricesscreen::Read(*static_cast<Model*>(m), root); }
static void Visit(const void* m, page::FieldVisitor& v)
{
    const Model& p = *static_cast<const Model*>(m);
    v.Field("rows", static_cast<double>(p.quotes.size()));
    v.Field("sel", p.list.sel);
    v.Field("first", p.list.first);
}

static int Prompts(const void* model, views::PagePrompt* out, int max)
{
    const Model& m = *static_cast<const Model*>(model);
    int n = 0;
    if (m.list.count > 1 && n < max)
        out[n++] = {glyphs::DU, glyphs::DD, L"BASE", false, 0};
    return n;
}

static unsigned Pad(void* model, unsigned pressed, unsigned, float)
{
    Model& m = *static_cast<Model*>(model);
    // the D-pad walks the focus row as on every list, the window follows (it scrolled the window: stuck after one step, Shockwave)
    if (pressed & XINPUT_GAMEPAD_DPAD_UP)
        m.list.Step(-1);
    if (pressed & XINPUT_GAMEPAD_DPAD_DOWN)
        m.list.Step(1);
    return XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT | XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_X | XINPUT_GAMEPAD_Y;
}

static void Wheel(void* model, int notches) { static_cast<Model*>(model)->list.Scroll(-notches); }

static const page::PageInput kInput = {Prompts, Pad, Wheel, nullptr};

const page::PageDesc kPrices = {
    {kId, "dealers", page::Kind::Page, {nullptr, pricesscreen::kWindowName, nullptr, nullptr}, page::Park::Away, page::Pause::None, nullptr,
     0, nullptr, 0, pricesscreen::kActions, pricesscreen::kActionCount, nullptr, nullptr, 0, &kInput},
    {sizeof(Model), Init, Read, pricesscreen::Stamp, Visit, nullptr, 0, Done},
    View,
};
PAGE_REGISTER(kPrices);
} // namespace pricespage
