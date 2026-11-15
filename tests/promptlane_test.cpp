// modules\hud\src\pages\promptlane.h (pure): the pages' prompt rule (the owner, 12 Nov 2026): measured width first, then a
// second row where the page has room, a prompt giving way only as the last resort and never the way out.
#include "test.h"

#include "../modules/hud/src/pages/promptlane.h"

using pageinput::Role;
using promptlane::Prompt;
using promptlane::Space;

namespace
{
Prompt A(float w, int prio = 0) { return Prompt{w, Role::Action, prio}; }
Prompt C(float w) { return Prompt{w, Role::Close, 0}; }
Prompt B(float w) { return Prompt{w, Role::Back, 0}; }
Space Row(float w1, bool second = false, float w2 = 0, float gap = 10) { Space s; s.row1 = w1, s.secondRow = second, s.row2 = w2, s.gap = gap; return s; }
bool Kept(const promptlane::Fit& f, int i) { return f.row[i] >= 0; }
} // namespace

void Test_promptlane()
{
    // Width, not a count: seven narrow prompts fit one row (today's lane takes five, promptfit.h kLaneMax).
    {
        const Prompt p[] = {A(50), A(50), A(50), A(50), A(50), A(50), C(60)};
        const promptlane::Fit f = promptlane::FitPrompts(p, 7, Row(500));
        CHECK(f.ok && f.rows == 1 && f.nGave == 0 && !f.overflow);
        for (int i = 0; i < 7; ++i)
            CHECK(f.row[i] == 0);
        CHECK(f.x[0] == 0 && f.x[1] == 60 && f.x[6] == 360); // in the page's order, a gap apart
    }
    // Exactly the free width fits; a pixel more does not.
    {
        const Prompt p[] = {A(100), C(90)};
        CHECK(promptlane::FitPrompts(p, 2, Row(200)).rows == 1);
        const promptlane::Fit f = promptlane::FitPrompts(p, 2, Row(199));
        CHECK(f.nGave == 1 && f.gaveWay[0] == 0 && Kept(f, 1)); // no second row: the action gives way, CLOSE stays
    }
    // Too wide for one row: a second row when the page has room, nobody gives way.
    {
        const Prompt p[] = {A(120), A(120), A(120), A(120), C(80)};
        const promptlane::Fit f = promptlane::FitPrompts(p, 5, Row(400, true, 400));
        CHECK(f.rows == 2 && f.nGave == 0);
        CHECK(f.row[0] == 0 && f.row[1] == 0 && f.row[2] == 0 && f.row[3] == 1 && f.row[4] == 1); // the first row filled in order
        CHECK(f.x[3] == 0 && f.x[4] == 130);
    }
    // The same prompts with no room for a second row: give way, the last action first, until they fit; CLOSE stays.
    {
        const Prompt p[] = {A(120), A(120), A(120), A(120), C(80)};
        const promptlane::Fit f = promptlane::FitPrompts(p, 5, Row(400));
        CHECK(f.rows == 1 && f.nGave == 2 && f.gaveWay[0] == 3 && f.gaveWay[1] == 2);
        CHECK(Kept(f, 0) && Kept(f, 1) && !Kept(f, 2) && !Kept(f, 3) && Kept(f, 4));
    }
    // A full row keeps the close prompt even when it is first in the order and every action is wider.
    {
        const Prompt p[] = {C(80), A(300), A(300)};
        const promptlane::Fit f = promptlane::FitPrompts(p, 3, Row(400));
        CHECK(Kept(f, 0) && f.nGave == 1 && f.gaveWay[0] == 2 && Kept(f, 1));
        // ... and when it is the latest prompt with the lowest priority (the one an action would be dropped for)
        const Prompt q[] = {A(300, 1), A(300, 1), C(80)};
        const promptlane::Fit g = promptlane::FitPrompts(q, 3, Row(400));
        CHECK(Kept(g, 2) && g.nGave == 1 && g.gaveWay[0] == 1 && Kept(g, 0));
    }
    // Two rows still too few: give way only then, and the second row is still used.
    {
        const Prompt p[] = {A(150), A(150), A(150), A(150), A(150), B(100)};
        const promptlane::Fit f = promptlane::FitPrompts(p, 6, Row(320, true, 320));
        CHECK(f.rows == 2 && f.nGave == 2 && Kept(f, 5));
        CHECK(Kept(f, 0) && Kept(f, 1) && Kept(f, 2) && !Kept(f, 3) && !Kept(f, 4));
    }
    // Priority decides who gives way: the lowest, ties the later one.
    {
        const Prompt p[] = {A(150, 0), A(150, 5), A(150, 1), C(80)};
        const promptlane::Fit f = promptlane::FitPrompts(p, 4, Row(400));
        CHECK(f.nGave == 1 && f.gaveWay[0] == 0 && Kept(f, 1) && Kept(f, 2) && Kept(f, 3)); // 150 + 150 + 80 + 2 gaps = 400
        const Prompt q[] = {A(150, 2), A(150, 5), A(150, 2), A(150, 2), C(80)};
        const promptlane::Fit g = promptlane::FitPrompts(q, 5, Row(400));
        CHECK(g.nGave == 2 && g.gaveWay[0] == 3 && g.gaveWay[1] == 2 && Kept(g, 0) && Kept(g, 1)); // ties: the later one first
    }
    // Only the way out left and still too wide: it stays, flagged.
    {
        const Prompt p[] = {A(100), C(300), B(300)};
        const promptlane::Fit f = promptlane::FitPrompts(p, 3, Row(250));
        CHECK(f.overflow && Kept(f, 1) && Kept(f, 2) && !Kept(f, 0));
        const promptlane::Fit g = promptlane::FitPrompts(p, 3, Row(250, true, 250));
        CHECK(g.overflow && g.rows == 2 && Kept(g, 1) && Kept(g, 2));
    }
    // No prompts; more than the storage is refused.
    {
        CHECK(promptlane::FitPrompts(nullptr, 0, Row(100)).ok && promptlane::FitPrompts(nullptr, 0, Row(100)).rows == 0);
        Prompt many[promptlane::kMax + 1];
        for (Prompt& q : many)
            q = A(1);
        CHECK(!promptlane::FitPrompts(many, promptlane::kMax + 1, Row(1e6f)).ok);
        CHECK(promptlane::FitPrompts(many, promptlane::kMax, Row(1e6f)).rows == 1);
    }
    // RowWidth: the widths and the gaps between them.
    {
        const Prompt p[] = {A(10), A(20), A(30)};
        const bool use[] = {true, false, true};
        CHECK(promptlane::RowWidth(p, 3, use, 5) == 45);
    }
}
