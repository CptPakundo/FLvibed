// FLvibed - the LOG page's description (audit 70b F4a; nnpage.cpp's LOG as it was): it stands over the game's
// NeuroNetStoryStar window, which stays on the screen (its journal's text shows through the page's hole, the HUD module's
// windowtext places it); its model is the objective, what the player has found near and where the player is
// (core\game\screens\mlog_screen.*). Its input is its own (page.h PageInput): the D-pad's sides take the window's two tabs,
// Y shows the map while there is an objective; the D-pad up / down and the right stick scroll the journal, which is the
// pad's own (padbases.cpp: the window's scroll bar), so the page only names it.
#include "mlog_page.h"

#include "../pagelist.h"
#include "../pagerun.h"
#include "game/screens/mlog_screen.h"
#include "ui/glyphs.h"

namespace mlogpage
{
using mlogscreen::Model;

static void Init(void* m) { new (m) Model(); }
static void Done(void* m) { static_cast<Model*>(m)->~Model(); }
// (false: no state of the page's follows a read; its first line is the journal's, which the game scrolls: the view hands it
// to PageState::first each frame, and a true here would set it to 0 until the next draw)
static bool Read(void* m, u32 root)
{
    mlogscreen::Read(*static_cast<Model*>(m), root);
    return false;
}
static void Visit(const void* m, page::FieldVisitor& v)
{
    const Model& l = *static_cast<const Model*>(m);
    v.Field("mission", l.mission);
    v.Field("places", static_cast<double>(l.places.size()));
    v.Field("journal", l.journal);
    v.Text("objective", l.objective.c_str());
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
    add(DL, L"MISSION / JOURNAL", DR);
    add(DU, L"SCROLL", DD);
    if (!m.objective.empty())
        add(Y, L"SHOW ON MAP");
    return n;
}

static unsigned Pad(void* model, unsigned pressed, unsigned, float)
{
    const Model& m = *static_cast<const Model*>(model);
    if (pressed & XINPUT_GAMEPAD_DPAD_LEFT)
        pagerun::Act(mlogscreen::kActTab, 1);
    if (pressed & XINPUT_GAMEPAD_DPAD_RIGHT)
        pagerun::Act(mlogscreen::kActTab, 0);
    if ((pressed & XINPUT_GAMEPAD_Y) && !m.objective.empty())
        pagerun::Act(mlogscreen::kActShowOnMap, 0);
    return XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT | XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_X | XINPUT_GAMEPAD_Y;
}

// The wheel scrolls the game's journal itself (its own control): nothing of the page's moves.
static void Wheel(void*, int) {}

static void Prepare(void* m, u32 root) { mlogscreen::Prepare(*static_cast<Model*>(m), root); }

static const page::PageInput kInput = {Prompts, Pad, Wheel, nullptr, Prepare};

const page::PageDesc kLog = {
    {kId, "neuronet", page::Kind::Page, {mlogscreen::kWindowClass, mlogscreen::kWindowName, nullptr, nullptr}, page::Park::Hosted,
     page::Pause::World, nullptr, 0, nullptr, 0, mlogscreen::kActions, mlogscreen::kActionCount, nullptr, nullptr, 0, &kInput},
    {sizeof(Model), Init, Read, mlogscreen::Stamp, Visit, nullptr, 0, Done},
    View,
};
PAGE_REGISTER(kLog);
} // namespace mlogpage
