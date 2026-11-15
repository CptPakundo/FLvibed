// modules\hud\src\pages\pagelist.h (pure): the pages' list, filled by each page's own file (audit 70b F1b): a page added by
// adding files, found by its id, a second page with the same id refused and counted, every kind of description listed.
#include "test.h"

#include "../modules/hud/src/pages/pagelist.h"

#include <cstdio>

using page::Kind;

void Test_pagelist_legacy();

namespace
{
page::PageDesc Desc(const char* id)
{
    page::PageDesc d = {};
    d.core.id = id, d.core.family = "test", d.core.kind = Kind::Page;
    return d;
}

// the module's own list, as a page file fills it (its registrar runs with the file's statics, before main)
const page::PageDesc kStaticPage = Desc("static_page");
PAGE_REGISTER(kStaticPage);
} // namespace

void Test_pagelist()
{
    // A page file's registration reached the module's list before the tests ran.
    CHECK(kStaticPage_registered.ok && pagelist::All().Find("static_page") &&
          pagelist::All().Find("static_page")->desc == &kStaticPage);
    // Every kind, found by id; the entry carries the whole description.
    {
        pagelist::List l;
        const page::PageDesc p = Desc("info");
        page::DialogDesc d = {};
        d.core.id = "jettison", d.core.kind = Kind::Dialog;
        page::HostedDesc h = {};
        h.core.id = "options", h.core.kind = Kind::Hosted;
        page::MapDesc m = {};
        m.core.id = "navmap", m.core.kind = Kind::Map;
        CHECK(pagelist::Registrar(p, l).ok && pagelist::Registrar(d, l).ok && pagelist::Registrar(h, l).ok &&
              pagelist::Registrar(m, l).ok);
        CHECK(l.Count() == 4 && l.Refused() == 0);
        CHECK(l.Find("info")->desc == &p && l.Find("jettison")->core->kind == Kind::Dialog);
        CHECK(l.Find("navmap")->desc == &m && l.At(2).core == &h.core);
        CHECK(l.Find("nope") == nullptr && l.Find(nullptr) == nullptr);
    }
    // Refused, and counted: an id twice (two families' files with one id), no id, null.
    {
        pagelist::List l;
        const page::PageDesc a = Desc("log"), b = Desc("log"), none = Desc(nullptr), empty = Desc("");
        CHECK(pagelist::Registrar(a, l).ok && !pagelist::Registrar(b, l).ok);
        CHECK(!pagelist::Registrar(none, l).ok && !pagelist::Registrar(empty, l).ok && !l.Add(nullptr, &a));
        CHECK(l.Count() == 1 && l.Refused() == 4 && l.Find("log")->desc == &a);
    }
    // Full: the 65th page is refused, the first 64 stay.
    {
        pagelist::List l;
        static char ids[pagelist::List::kMax + 1][8];
        static page::PageDesc descs[pagelist::List::kMax + 1];
        int ok = 0;
        for (int i = 0; i <= pagelist::List::kMax; ++i)
        {
            snprintf(ids[i], sizeof ids[i], "p%d", i);
            descs[i] = Desc(ids[i]);
            ok += l.Add(&descs[i].core, &descs[i]);
        }
        CHECK(ok == pagelist::List::kMax && l.Refused() == 1 && l.Find("p0") && !l.Find("p64"));
    }

    // The pages not on the runtime yet (audit 70b F2b): the rest of HUD 2 asks the list, never a page's file.
    Test_pagelist_legacy();
}

namespace
{
bool g_shell = false, g_base = false, g_info = false;
int g_draws = 0, g_afters = 0, g_unders = 0;
bool ShellStands() { return g_shell || g_base; } // (the NeuroNet shell stands with a base page, as nnpage::On)
bool BaseStands() { return g_base; }
bool InfoStands() { return g_info; }
bool Yes() { return true; }
bool ShellCovers(int fam) { return fam == 1 || fam == 2; }
bool BaseCovers(int fam) { return fam == 1 || fam == 3; }
void ShellDraw() { ++g_draws; }
void ShellAfter() { ++g_afters; }
void BaseUnder() { ++g_unders; }
} // namespace

void Test_pagelist_legacy()
{
    const pagelist::LegacyPage shell = {"neuronet", ShellStands, ShellCovers, nullptr, ShellDraw, ShellAfter, nullptr};
    const pagelist::LegacyPage info = {"neuronet.info", InfoStands, nullptr, Yes, nullptr, nullptr, nullptr};
    const pagelist::LegacyPage base = {"base", BaseStands, BaseCovers, BaseStands, nullptr, nullptr, BaseUnder};
    pagelist::LegacyList l;
    CHECK(l.Add(&shell) && l.Add(&info) && l.Add(&base) && l.Count() == 3);
    // refused: null, no id, no Stands, a second "base"
    const pagelist::LegacyPage noStands = {"x", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
    const pagelist::LegacyPage noId = {"", ShellStands, nullptr, nullptr, nullptr, nullptr, nullptr};
    CHECK(!l.Add(nullptr) && !l.Add(&noStands) && !l.Add(&noId) && !l.Add(&base) && l.Refused() == 4 && l.Count() == 3);

    // Nothing stands: no page, no window covered, nothing off the screen; Draw and After still run (the page decides), Under not.
    g_shell = g_base = g_info = false, g_draws = g_afters = g_unders = 0;
    CHECK(!l.Standing() && !l.Covers(1) && !l.Covers(2) && !l.Covers(3) && !l.OffScreen());
    l.Draw(), l.After(), l.Under();
    CHECK(g_draws == 1 && g_afters == 1 && g_unders == 0);

    // A NeuroNet page: its windows covered, the base's not; nothing off the screen.
    g_shell = true;
    CHECK(l.Standing() && l.Covers(1) && l.Covers(2) && !l.Covers(3) && !l.OffScreen());
    // INFO beside it: its controls off the screen.
    g_info = true;
    CHECK(l.OffScreen());
    // A base page alone: the shell stands with it (both cover), off the screen, its ground drawn under.
    g_shell = g_info = false, g_base = true;
    CHECK(l.Standing() && l.Covers(1) && l.Covers(2) && l.Covers(3) && !l.Covers(4) && l.OffScreen());
    l.Under();
    CHECK(g_unders == 1);

    // Full: the ninth is refused.
    pagelist::LegacyList full;
    static char ids[pagelist::LegacyList::kMax + 1][8];
    static pagelist::LegacyPage entries[pagelist::LegacyList::kMax + 1];
    int ok = 0;
    for (int i = 0; i <= pagelist::LegacyList::kMax; ++i)
    {
        snprintf(ids[i], sizeof ids[i], "l%d", i);
        entries[i] = {ids[i], ShellStands, nullptr, nullptr, nullptr, nullptr, nullptr};
        ok += full.Add(&entries[i]);
    }
    CHECK(ok == pagelist::LegacyList::kMax && full.Refused() == 1);
}
