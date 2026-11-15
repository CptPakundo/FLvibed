// FLvibed - the death page's description (audit 70b F4a; nnpage.cpp's death page as it was, FREELANCER_RE "The death /
// mission-failed dialog"): it stands over the same window as LOG (NeuroNetStoryStar), told apart by its shell (the load
// buttons stand: mlogscreen::DeathShown); the window stays on the screen, its journal through the page's hole and its three
// buttons where the game put them, HUD 2 chips drawn on them (a click goes through to the game's own button). No action of
// its own, no prompt, no pad: the pad's are padmenus'. Its model is the buttons and the mouse, read right before the view.
#include "death_page.h"

#include "../pagelist.h"
#include "../pagerun.h"
#include "game/screens/mlog_screen.h"

namespace deathpage
{
using mlogscreen::DeathModel;

static void Init(void* m) { new (m) DeathModel(); }
static void Done(void* m) { static_cast<DeathModel*>(m)->~DeathModel(); }
static bool Read(void*, u32) { return false; } // (all it shows is read at the view's moment: Prepare)
static void Visit(const void* m, page::FieldVisitor& v)
{
    const DeathModel& d = *static_cast<const DeathModel*>(m);
    int n = 0;
    for (const mlogscreen::DeathButton& b : d.buttons)
        n += b.ok;
    v.Field("buttons", n);
}
static void Prepare(void* m, u32 root) { mlogscreen::PrepareDeath(*static_cast<DeathModel*>(m), root); }

static const page::PageInput kInput = {nullptr, nullptr, nullptr, nullptr, Prepare}; // (no prompt, no pad: padmenus'; the bindings, none, answer)

const page::PageDesc kDeath = {
    {kId, "neuronet", page::Kind::Page, {mlogscreen::kWindowClass, mlogscreen::kWindowName, nullptr, nullptr}, page::Park::Hosted,
     page::Pause::World, nullptr, 0, nullptr, 0, nullptr, 0, nullptr, nullptr, 0, &kInput},
    {sizeof(DeathModel), Init, Read, nullptr, Visit, nullptr, 0, Done},
    View,
};
PAGE_REGISTER(kDeath);
} // namespace deathpage
