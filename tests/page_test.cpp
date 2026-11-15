// modules\hud\src\pages\page.h (pure): the registry's pure half (which page claims a window), the stack (a page and at
// most one dialog, input to the top) and a page's prompts from its bindings (audit 70b 2.1-2.2).
#include "test.h"

#include "../modules/hud/src/pages/page.h"

using page::Kind;
using page::PageCore;
using pageinput::Binding;
using pageinput::Role;

namespace
{
bool FinalStage(uint32_t root) { return root == 2; } // NN_Dealer's kind byte 2, as a fake
int MayBuy(const void* m, int) { return *static_cast<const int*>(m); }

PageCore Core(const char* id, Kind k, bool (*is)(uint32_t) = nullptr)
{
    PageCore c = {};
    c.id = id, c.family = "test", c.kind = k, c.claim.Is = is;
    return c;
}
} // namespace

void Test_page()
{
    // The registry: one lookup by the class; one class, two pages, told apart by Is; an unknown class shows natively.
    {
        const PageCore dealer = Core("ship_dealer", Kind::Page), stage = Core("final_stage", Kind::Page, FinalStage);
        page::Registry reg;
        CHECK(reg.Add(0x5E1000, &stage) && reg.Add(0x5E1000, &dealer) && reg.Count() == 2);
        CHECK(!reg.Add(0x5E1000, &dealer) && reg.Count() == 2); // twice: refused
        CHECK(!reg.Add(0, &dealer));                             // an unresolved class: nothing claims by it
        CHECK(reg.Find(0x5E1000, 2) == &stage);
        CHECK(reg.Find(0x5E1000, 1) == &dealer);
        CHECK(reg.Find(0x5F0000, 2) == nullptr); // another mod's window class
        reg.Clear();
        CHECK(reg.Find(0x5E1000, 1) == nullptr);
    }
    // The stack: a page and at most one dialog; input to the top; a dialog answered first.
    {
        const PageCore p = Core("inventory", Kind::Page), q = Core("status", Kind::Page);
        const PageCore d = Core("jettison", Kind::Dialog), e = Core("confirm", Kind::Dialog);
        page::Stack s;
        CHECK(s.Top() == nullptr && !s.OpenPage(&d));
        CHECK(s.OpenPage(&p) && s.Top() == &p);
        CHECK(!s.OpenDialog(&q) && s.OpenDialog(&d) && s.Top() == &d && s.Page() == &p);
        CHECK(!s.OpenDialog(&e) && s.Top() == &d); // one dialog at a time
        CHECK(!s.OpenPage(&q) && s.Page() == &p);  // the dialog is answered first
        s.CloseDialog();
        CHECK(s.Top() == &p && s.OpenPage(&q) && s.Top() == &q);
        s.CloseAll();
        CHECK(s.Top() == nullptr);
    }
    // The prompts: an action's word while its May is 0; the way out always; no word, no prompt.
    {
        screens::ActionDesc acts[2] = {};
        acts[0].id = 1, acts[0].May = MayBuy;
        acts[1].id = 2; // no May: always
        PageCore c = Core("commodity", Kind::Page);
        c.actions = acts, c.nActions = 2;
        const Binding b[] = {
            {0x1000, 0, 1, L"BUY", true, Role::Action, 0},
            {0x4000, 0, 2, L"INFO", false, Role::Action, 0},
            {0x8000, 0, -1, nullptr, false, Role::Action, 0},
            {0x2000, 27, -1, L"CLOSE", false, Role::Close, 0},
        };
        int out[promptlane::kMax];
        int cash = 0;
        CHECK(page::Prompts(c, b, 4, &cash, 0, out) == 3 && out[0] == 0 && out[1] == 1 && out[2] == 3);
        cash = 1316; // the game's refusal: the prompt goes
        CHECK(page::Prompts(c, b, 4, &cash, 0, out) == 2 && out[0] == 1 && out[1] == 3);
    }
}
