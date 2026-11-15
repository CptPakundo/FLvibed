// FLvibed - the pages' list (the HUD 2 page framework: audit 70b F1b): every page registers itself from its own file, so a
// family adds a page by adding files to its folder (modules\hud\src\pages\<family>\, globbed by build.cmd), with no shared
// table to edit. A page file says `PAGE_REGISTER(kMyPage);` beside its description; the runtime (pagerun) walks the list
// at its start. Pure: no game include. tests\pagelist_test.cpp holds the rules.
#pragma once
#include "page.h"

#include <cstring>

namespace pagelist
{
struct Entry
{
    const page::PageCore* core; // the description's core (its id, its claim, its bindings and actions)
    const void* desc;           // the whole description: a PageDesc, DialogDesc, HostedDesc or MapDesc, as core->kind says
};

class List
{
public:
    static constexpr int kMax = 64;
    // false: refused (null, no id, an id already listed, or full); the refusal is counted for the runtime's verb
    bool Add(const page::PageCore* core, const void* desc)
    {
        if (!core || !desc || !core->id || !core->id[0] || n_ >= kMax || Find(core->id))
        {
            ++refused_;
            return false;
        }
        e_[n_].core = core, e_[n_].desc = desc, ++n_;
        return true;
    }
    const Entry* Find(const char* id) const
    {
        for (int i = 0; id && i < n_; ++i)
            if (!strcmp(e_[i].core->id, id))
                return &e_[i];
        return nullptr;
    }
    int Count() const { return n_; }
    const Entry& At(int i) const { return e_[i]; }
    int Refused() const { return refused_; }

private:
    Entry e_[kMax] = {};
    int n_ = 0;
    int refused_ = 0;
};

// The one list of the module (a function's static: filled by the pages' registrars at the DLL's start, whatever the order
// their files' statics run in; a hot reload starts a new DLL with a new list).
inline List& All()
{
    static List list;
    return list;
}

// A page file's registration, run with its file's statics.
struct Registrar
{
    bool ok;
    explicit Registrar(const page::PageDesc& d, List& to = All()) : ok(to.Add(&d.core, &d)) {}
    explicit Registrar(const page::DialogDesc& d, List& to = All()) : ok(to.Add(&d.core, &d)) {}
    explicit Registrar(const page::HostedDesc& d, List& to = All()) : ok(to.Add(&d.core, &d)) {}
    explicit Registrar(const page::MapDesc& d, List& to = All()) : ok(to.Add(&d.core, &d)) {}
};

// ---------------------------------------------------------------- pages not on the runtime yet
// A page still drawn by its own file (nnpage.cpp's NeuroNet pages, basepage.cpp's base pages) until it moves onto the runtime
// (audit 70b F3, F4). What the rest of HUD 2 asks of a standing page (the skin, the game's text, the strip, the HUD's roots)
// it asks the runtime (pagerun.h Standing, Covers, ControlsOffScreen, the Draw hooks), which answers from these entries and
// so names no page's file. Each file registers its own (LEGACY_PAGE_REGISTER). A hook's order between two entries is not
// defined: one entry per hook today.
struct LegacyPage
{
    const char* id;
    bool (*Stands)();        // the page stands this frame
    bool (*Covers)(int fam); // it is the game's window of that family (core\ui\winplates.h): the window gets no plate or focus; null none
    bool (*OffScreen)();     // its window's controls stand off the screen now (nothing of the game's text to place); null never
    void (*Draw)();          // over the game's UI, every frame (the page decides what); null none
    void (*After)();         // after the UI pass (a 3D view over the page), every frame; null none
    void (*Under)();         // under the game's UI pass, while it stands; null none
};

class LegacyList
{
public:
    static constexpr int kMax = 8;
    // false: refused (null, no id, an id already listed, or full)
    bool Add(const LegacyPage* p)
    {
        if (!p || !p->id || !p->id[0] || !p->Stands || n_ >= kMax || Find(p->id))
        {
            ++refused_;
            return false;
        }
        e_[n_++] = p;
        return true;
    }
    const LegacyPage* Find(const char* id) const
    {
        for (int i = 0; id && i < n_; ++i)
            if (!strcmp(e_[i]->id, id))
                return e_[i];
        return nullptr;
    }
    int Count() const { return n_; }
    const LegacyPage& At(int i) const { return *e_[i]; }
    int Refused() const { return refused_; }

    // Some page stands.
    bool Standing() const
    {
        for (int i = 0; i < n_; ++i)
            if (e_[i]->Stands())
                return true;
        return false;
    }
    // A standing page is the game's window of family `fam`.
    bool Covers(int fam) const
    {
        for (int i = 0; i < n_; ++i)
            if (e_[i]->Covers && e_[i]->Stands() && e_[i]->Covers(fam))
                return true;
        return false;
    }
    // A standing page keeps its window's controls off the screen.
    bool OffScreen() const
    {
        for (int i = 0; i < n_; ++i)
            if (e_[i]->OffScreen && e_[i]->Stands() && e_[i]->OffScreen())
                return true;
        return false;
    }
    void Draw() const
    {
        for (int i = 0; i < n_; ++i)
            if (e_[i]->Draw)
                e_[i]->Draw();
    }
    void After() const
    {
        for (int i = 0; i < n_; ++i)
            if (e_[i]->After)
                e_[i]->After();
    }
    void Under() const
    {
        for (int i = 0; i < n_; ++i)
            if (e_[i]->Under && e_[i]->Stands())
                e_[i]->Under();
    }

private:
    const LegacyPage* e_[kMax] = {};
    int n_ = 0;
    int refused_ = 0;
};

// The module's one list of them (a function's static, as All()).
inline LegacyList& Legacy()
{
    static LegacyList list;
    return list;
}

struct LegacyRegistrar
{
    bool ok;
    explicit LegacyRegistrar(const LegacyPage& p, LegacyList& to = Legacy()) : ok(to.Add(&p)) {}
};
} // namespace pagelist

// In a page's file, after its description: registers it with the module's list.
#define PAGE_REGISTER(desc) static const pagelist::Registrar desc##_registered(desc)
// In a file whose pages are not on the runtime yet, after its entry: registers it with the module's list of them.
#define LEGACY_PAGE_REGISTER(entry) static const pagelist::LegacyRegistrar entry##_registered(entry)
