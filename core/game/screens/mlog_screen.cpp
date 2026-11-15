// The LOG and death pages' reads and actions: mlog_screen.h. nnpage.cpp's LOG reads (its objective in Update, the places, what
// DrawLog and DrawDeath read as they drew) and its LogTab, moved unchanged (audit 70b F4a).
#include "mlog_screen.h"

#include "places_screen.h"
#include "game/baseinfo.h"
#include "game/fl.h"
#include "game/hostcalls.h"
#include "game/hudinfo.h"
#include "game/nntabs.h"
#include "game/nnwin.h"
#include "game/room.h"
#include "flexe.h"
#include "safemem.h"
#include "ui/ui.h"
#include "ui/winplates.h"

namespace mlogscreen
{
// The window's own fields (docs\research\neuronet-windows-facts.md)
constexpr u32 kLogTab = 0x37C; // 0 the LOG tab, 1 MISSION
// A rich text's scroll bar (only while it overflows) and a scroll bar's position (padbases.cpp StickBar, room.cpp Scroll)
constexpr u32 kRichBar = 0x3F0, kBarPos = 0x3C8;

static u32 R32(u32 a)
{
    u32 v = 0;
    safemem::Read(a, &v, 4);
    return v;
}

uint64_t Stamp()
{
    static uint64_t frames = 0;
    return ++frames;
}

bool Read(Model& m, u32)
{
    // the current step's text as the game formats it ("" with no formatter: hudhost.h HostObjectiveText)
    wchar_t obj[256] = {};
    m.objective = hostcalls::g.ObjectiveText && hostcalls::g.ObjectiveText(obj, 256) && obj[0] ? obj : L"";
    const u32 now = GetTickCount();
    if (m.placesDue != 0 && static_cast<int>(now - m.placesDue) < 0)
        return false;
    m.placesDue = (now + 2000) | 1;
    std::vector<screens::Place> ships; // (INFO's: LOG lists none)
    screens::ReadPlaces(m.places, ships);
    return true;
}

void Prepare(Model& m, u32 root)
{
    u8 tab = 0;
    safemem::Read(root + kLogTab, &tab, 1);
    m.mission = tab != 0;
    m.timer = hudinfo::MissionTimer();
    m.here = screens::ReadHere();
    m.ship = baseinfo::ShipName();
    // the journal's bar, for the test verb (the pad's stick and D-pad move it: padbases.cpp); a bar is a bar by its class
    m.journal = 0;
    const u32 text = uiclick::FindVisible(kTextName), bar = text ? R32(text + kRichBar) : 0;
    float pos = 0;
    if (bar && exe::ScrollBarClick.is(R32(R32(bar) + 0x4C)) && safemem::Read(bar + kBarPos, &pos, 4) && pos > 0 && pos < 1e6f)
        m.journal = static_cast<int>(pos + 0.5f);
}

static const char* const kDeathButton[3][2] = {{"LoadAutosaveButton", "LoadAutosaveText"}, {"MainMenuButton", "MainMenuText"}, {"LoadSavegameButton", "LoadSavegameText"}};
void PrepareDeath(DeathModel& m, u32)
{
    m.mouseX = static_cast<float>(fl::MouseX()), m.mouseY = static_cast<float>(fl::MouseY());
    for (int i = 0; i < 3; ++i)
    {
        const char* const* b = kDeathButton[i];
        DeathButton& d = m.buttons[i];
        d = DeathButton();
        ui::Item it;
        const u32 ctl = uiclick::FindVisible(b[0]);
        if (!ctl || !ui::ControlRect(ctl, it) || it.hw <= 1)
            continue;
        winplates::TextOf(uiclick::Find(b[1]), d.word);
        if (d.word.empty())
            d.word = b[0][0] == 'M' ? L"MAIN MENU" : b[0][4] == 'A' ? L"LOAD AUTOSAVE" : L"LOAD GAME";
        for (wchar_t& c : d.word)
            c = towupper(c);
        d.ok = true, d.x0 = it.x - it.hw, d.y0 = it.y - it.hh, d.x1 = it.x + it.hw, d.y1 = it.y + it.hh;
    }
}

bool DeathShown() { return !nnwin::Multiplayer() && uiclick::FindVisible("LoadAutosaveButton") && uiclick::FindVisible(kTextName); }

const hk::Sym* const kWindowClass = &exe::VtStoryStar;

// ---- LOG's actions
// A tab: the window's own button (borrowed).
static bool Tab(u32, int mission)
{
    if (const u32 c = uiclick::FindVisible(mission ? "MissionButton" : "LogButton"))
        uiclick::Click(c);
    return true;
}
// SHOW ON MAP: the NeuroNet's own map tab, the command the game's tab button sends (as INFO's: nnhelp_screen.cpp).
static bool ShowOnMap(u32, int) { return nnwin::Go(nntabs::kCmdMap); }

const screens::ActionDesc kActions[] = {
    {kActTab, "tab", L"MISSION / JOURNAL", nullptr, nullptr, nullptr, Tab, nullptr, screens::Route::Borrowed, screens::Route::Borrowed, nullptr,
     false, screens::Answer::Now, 0, nullptr, nullptr},
    {kActShowOnMap, "map", L"SHOW ON MAP", nullptr, nullptr, nullptr, ShowOnMap, nullptr, screens::Route::Borrowed, screens::Route::Borrowed,
     nullptr, false, screens::Answer::Now, 0, nullptr, nullptr},
};
const int kActionCount = sizeof(kActions) / sizeof(kActions[0]);
} // namespace mlogscreen
