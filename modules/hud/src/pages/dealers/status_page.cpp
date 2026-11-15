// FLvibed - the STATUS page's description (audit 70b F4b; nnpage.cpp's STATUS as it was): it stands over the game's
// NN_PlayerInfo window, which stays on the screen (the focused faction's card, the game's own, shows through the page's
// hole); its model is the pilot's numbers and the reputation list (core\game\screens\status_screen.*). Its input is its own
// (page.h PageInput): the D-pad walks the factions (up / down a row, left / right a page; the right stick too: the shell's),
// X shows the next filter, Y sorts the other way; the wheel scrolls the list; a click takes a faction. It acts on nothing of
// the game's: the card follows the focus (status_screen.cpp Follow).
#include "status_page.h"

#include "../pagelist.h"
#include "../pagerun.h"
#include "game/screens/status_screen.h"
#include "ui/glyphs.h"

namespace statuspage
{
using statusscreen::Model;

static void Init(void* m) { statusscreen::Open(*new (m) Model()); }
static void Done(void* m) { static_cast<Model*>(m)->~Model(); }
static bool Read(void* m, u32 root) { return statusscreen::Read(*static_cast<Model*>(m), root); }
static void Visit(const void* m, page::FieldVisitor& v)
{
    const Model& s = *static_cast<const Model*>(m);
    v.Field("reps", static_cast<double>(s.reps.size()));
    v.Field("shown", s.focus->list.count);
    v.Field("sel", s.focus->list.sel);
    v.Field("filter", s.focus->filter);
    v.Field("byName", s.focus->byName);
    v.Field("card", s.card);
    v.Field("cardShown", s.cardShown);
    v.Text("cardName", s.cardName.c_str());
}

// X, and the mouse's SHOW chip: the next filter, the focus back on the first faction.
static void NextFilter(Model& m)
{
    statusscreen::Focus& f = *m.focus;
    f.filter = statusrules::NextFilter(f.filter), f.list.sel = 0, f.list.first = 0;
    statusscreen::ShowReps(m);
}

static int Prompts(const void* model, views::PagePrompt* out, int max)
{
    using namespace glyphs;
    const Model& m = *static_cast<const Model*>(model);
    int n = 0;
    auto add = [&](int id, const wchar_t* word, int id2 = -1)
    {
        if (n < max)
            out[n++] = {id, id2, word, false};
    };
    add(DU, L"FACTION", DD); // (the right stick walks them too; the card has nothing to scroll)
    {
        static std::wstring next;
        next = L"SHOW ";
        next += statusrules::FilterWord(statusrules::NextFilter(m.focus->filter));
        add(X, next.c_str());
    }
    add(Y, m.focus->byName ? L"SORT BY STANDING" : L"SORT BY NAME");
    return n;
}

static unsigned Pad(void* model, unsigned pressed, unsigned, float)
{
    Model& m = *static_cast<Model*>(model);
    screens::List& l = m.focus->list;
    bool stepped = false;
    if (pressed & XINPUT_GAMEPAD_DPAD_UP)
        stepped |= l.Step(-1);
    if (pressed & XINPUT_GAMEPAD_DPAD_DOWN)
        stepped |= l.Step(1);
    if (pressed & XINPUT_GAMEPAD_DPAD_LEFT)
        stepped |= l.Step(-l.rows);
    if (pressed & XINPUT_GAMEPAD_DPAD_RIGHT)
        stepped |= l.Step(l.rows);
    if (stepped)
        statusscreen::Stepped(m); // (the card follows: Follow)
    if (pressed & XINPUT_GAMEPAD_X)
        NextFilter(m);
    if (pressed & XINPUT_GAMEPAD_Y)
        m.focus->byName = !m.focus->byName, statusscreen::ShowReps(m);
    return XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT | XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_X | XINPUT_GAMEPAD_Y;
}

// The wheel scrolls the list (the focus stays inside it). The runtime calls this once every frame, right after the read, so
// the card follows the focus here, after the wheel's scroll, as the shell's update did before.
static void Wheel(void* model, int notches)
{
    Model& m = *static_cast<Model*>(model);
    if (notches)
        m.focus->list.Scroll(-notches);
    statusscreen::Follow(m);
}

// A click on a faction takes the focus (its card follows); a click on SHOW <next> is the pad's X.
static void Hit(void* model, int action, int arg, bool)
{
    Model& m = *static_cast<Model*>(model);
    if (action == statusscreen::kHitRow)
        m.focus->list.sel = arg, m.focus->list.Fit();
    else if (action == statusscreen::kHitFilter)
        NextFilter(m);
}

static const page::PageInput kInput = {Prompts, Pad, Wheel, Hit, nullptr};

const page::PageDesc kStatus = {
    {kId, "neuronet", page::Kind::Page, {statusscreen::kWindowClass, statusscreen::kWindowName, nullptr, nullptr}, page::Park::Hosted,
     page::Pause::World, nullptr, 0, nullptr, 0, nullptr, 0, nullptr, nullptr, 0, &kInput},
    {sizeof(Model), Init, Read, statusscreen::Stamp, Visit, nullptr, 0, Done},
    View,
};
PAGE_REGISTER(kStatus);
} // namespace statuspage
