// The STATUS page's reads: status_screen.h. nnpage.cpp's ReadStatus, ShowReps and ShowCard, moved unchanged (audit 70b F4b);
// its two clocks (the read's, the card's) are moments of GetTickCount now, where they were sums of the frames' dt.
#include "status_screen.h"

#include "game/hudinfo.h"
#include "game/room.h"
#include "flexe.h"
#include "hook.h"
#include "log.h"
#include "safemem.h"
#include "ui/ui.h"
#include "ui/winplates.h"

#include <algorithm>
#include <cstdio>

namespace statusscreen
{
// The window's own fields (docs\research\neuronet-windows-facts.md)
constexpr u32 kRepBegin = 0x410, kRepEnd = 0x414; // {reputation id, float} pairs

static Focus g_kept; // (the page's statics before: never reset)

static u32 R32(u32 a)
{
    u32 v = 0;
    safemem::Read(a, &v, 4);
    return v;
}
static float RF(u32 a)
{
    float v = 0;
    safemem::Read(a, &v, 4);
    return v;
}

void Open(Model& m)
{
    m.focus = &g_kept;
    g_kept.list.count = 0;
}
const Focus& Kept() { return g_kept; }

uint64_t Stamp()
{
    static uint64_t frames = 0;
    return ++frames;
}

void ShowReps(Model& m)
{
    Focus& f = *m.focus;
    m.shown.clear();
    for (const Rep& r : m.reps)
        if (statusrules::Passes(f.filter, r.v))
            m.shown.push_back(r);
    const bool byName = f.byName;
    std::stable_sort(m.shown.begin(), m.shown.end(), [byName](const Rep& a, const Rep& b) { return byName ? a.name < b.name : a.v < b.v; });
    f.list.count = static_cast<int>(m.shown.size());
    f.list.Fit();
}

static void ReadStatus(Model& m, u32 root)
{
    perf::Scope ps("nn.ReadStatus");
    const u32 begin = R32(root + kRepBegin), end = R32(root + kRepEnd);
    if (begin && end > begin && (end - begin) / 8 < 512)
    {
        std::vector<Rep> reps;
        for (u32 p = begin, i = 0; p < end; p += 8, ++i)
        {
            Rep r;
            r.id = R32(p), r.index = static_cast<int>(i), r.v = RF(p + 4);
            r.name = hudinfo::RepName(r.id);
            if (!r.name.empty())
                reps.push_back(r);
        }
        m.reps.swap(reps);
        ShowReps(m);
    }
    // (the first read of an open window may come before the server's answer.) The game hides its stats under the faction card, which stands as long as the page,
    // and goes on writing them there (Total Time counts on: issue 73): read hidden then
    const bool card = m.card;
    const auto find = [card](const char* n) { return card ? uiclick::Find(n) : uiclick::FindVisible(n); };
    std::wstring* head = m.focus->head;
    for (int i = 0; i < kStats; ++i)
    {
        char name[32];
        sprintf_s(name, "PlayerInfoHeading%d", i);
        std::wstring h, s;
        if (winplates::TextOf(find(name), h))
            head[i] = statusrules::Heading(h);
        else if (!card)
            head[i].clear();
        sprintf_s(name, "PlayerInfoStat%d", i);
        if (winplates::TextOf(find(name), s))
            m.stat[i] = s;
        else if (!card)
            m.stat[i].clear(); // (under the card a row the game does not answer keeps what was read)
    }
}

bool Read(Model& m, u32 root)
{
    m.root = root;
    const u32 now = GetTickCount();
    if (m.readDue != 0 && static_cast<int>(now - m.readDue) < 0)
        return false;
    m.readDue = (now + 500) | 1;
    ReadStatus(m, root);
    return true;
}

void Stepped(Model& m) { m.stepAt = GetTickCount() | 1; }

// The game's own refresh of a step - STATUS's faction card, INVENTORY's selection (the card it shows) - is not per step: each new card is laid out and
// measured again (windowtext: up to 6.4 ms a step on this PC, issue 68 - a Deck several times that), which a held or tapped walk paid at 20 steps a second.
// The page's own focus moves at once; the game follows a lone tap at once too, a walk when it rests (0.1 s) or every 0.25 s (nnpage.cpp SyncDue: INVENTORY's).
static bool CardDue(const Model& m, u32 now) { return !m.stepAt || now - m.stepAt >= 100 || !m.cardAt || now - m.cardAt >= 250; }

// The focused faction's card stands beside the list, the game's own: no press to read it, where the game's window took one to open it and one to close it.
void Follow(Model& m)
{
    const screens::List& l = m.focus->list;
    if (l.count <= 0 || !exe::StatusFactionCard.ok() || !m.root || room::NNWindowBusy() || m.stat[0].empty())
        return;
    const Rep& r = m.shown[l.sel];
    const u32 now = GetTickCount();
    if (r.index == m.cardShown || !CardDue(m, now))
        return;
    m.cardAt = now | 1;
    {
        perf::Scope ps("nn.StatusCard");
        hk::Call<void(__thiscall*)(u32, int)>(exe::StatusFactionCard, m.root, r.index);
    }
    m.cardShown = r.index, m.card = true, m.cardName = r.name;
    // (the card stands as long as the page: its close button has nothing to close, and it stood over the text)
    for (const char* n : {"NavMapCloseInfocardButton", "NavMapCloseInfocardText"})
        if (const u32 c = uiclick::Find(n))
        {
            u8 flags = 0;
            if (safemem::Read(c + 0x6C, &flags, 1))
                safemem::Write(c + 0x6C, static_cast<u8>(flags & 0xFC));
        }
}

const hk::Sym* const kWindowClass = &exe::VtPlayerInfo;
} // namespace statusscreen
