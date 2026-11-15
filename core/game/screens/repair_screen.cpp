// The repair page's reads and actions: repair_screen.h. The reads are basepage.cpp's ReadMends and what its DrawRepair read
// as it drew, the actions its MendTick / MendAll / MendGo and the close, moved unchanged (audit 70b F3a).
#include "repair_screen.h"

#include "compare_screen.h"
#include "parked.h"
#include "screen_rules.h"
#include "game/baseinfo.h"
#include "game/equip_rules.h"
#include "game/gamedata.h"
#include "game/hudinfo.h"
#include "game/room.h"
#include "flexe.h"
#include "safemem.h"
#include "text.h"
#include "ui/winplates.h"

namespace repairscreen
{
constexpr u32 kTickState = 0x3A0; // (the toggle's state, 0 ticked, 1 not: read in game, FREELANCER_RE "The docked pages")
static int g_soonMs = -1;         // an action asks the rows to be read again this soon (ms), -1 none

static void ReadRows(Model& m)
{
    std::vector<Row> rows;
    std::vector<baseinfo::Item> gear = baseinfo::Equipment();
    for (int i = 0; i < kRows; ++i)
    {
        const u32 box = screens::Ctl("RepairCheckBox%d", i);
        if (!box)
            continue;
        Row r;
        r.slot = i;
        u32 state = 0; // (an unreadable state read 0, ticked, as before)
        safemem::Read(box + kTickState, &state, 4);
        r.ticked = state == 0;
        winplates::TextOf(screens::Ctl("RepairItemName%d", i), r.name);
        std::wstring price;
        winplates::TextOf(screens::Ctl("RepairItemPrice%d", i), price);
        r.price = screens::Number(price);
        // the rows stand in the list's order, the damaged ones only: each takes the first damaged item of its name, so two of a kind keep their own health (the game's bar holds no share to read)
        for (baseinfo::Item& it : gear)
            if (it.arch && it.health >= 0 && it.health < 1 && hudinfo::EquipName(it.arch) == r.name)
            {
                r.arch = it.arch, r.health = it.health;
                it.arch = 0;
                break;
            }
        r.hull = !r.arch;
        rows.push_back(r);
    }
    m.rows.swap(rows);
    m.sel = std::max(0, std::min(m.sel, static_cast<int>(m.rows.size()) - 1));
}

void Refocus(Model& m)
{
    const int n = static_cast<int>(m.rows.size());
    u32 key = 0, info = 0;
    if (n && !m.rows[m.sel].hull)
    {
        m.compare = screens::ReadCompare(m.rows[m.sel].arch, true);
        key = m.rows[m.sel].arch, info = gamedata::EquipInfo(key);
    }
    else
    {
        m.compare = screens::CompareBlock();
        const gamedata::ShipFacts ship = gamedata::Ship(baseinfo::ShipArch());
        if (!ship.ok)
            return; // (no card stands: the one read last stays, as the old page's key did)
        key = ship.arch, info = ship.infoIds[1];
    }
    if (key != m.cardKey)
    {
        m.cardKey = key, m.cardFirst = 0;
        m.cardText = text::InfoCard(info);
    }
}

std::wstring TotalText() { return screens::TextIn("RepairTotalCost"); }
bool CanMend() { return uiclick::FindVisible("RepairOkButton") != 0 && screens::Number(TotalText()) > 0; }

uint64_t Stamp()
{
    static uint64_t frames = 0;
    return ++frames;
}

bool Read(Model& m, u32)
{
    const u32 now = GetTickCount();
    if (g_soonMs >= 0)
        m.rowsDue = (now + static_cast<u32>(g_soonMs)) | 1, g_soonMs = -1;
    bool rows = false;
    if (!room::NNWindowBusy() && (m.rowsDue == 0 || static_cast<int>(now - m.rowsDue) >= 0)) // (0: the open's first read, at once)
    {
        m.rowsDue = (now + 200) | 1;
        ReadRows(m);
        rows = true;
    }
    m.total = screens::Number(TotalText()), m.credits = baseinfo::Credits();
    m.canMend = CanMend();
    const gamedata::ShipFacts ship = gamedata::Ship(baseinfo::ShipArch());
    m.shipName = baseinfo::ShipName(), m.shipOk = ship.ok;
    m.shipHull = ship.ok ? equiprules::ValueWord(ship.hull) : std::wstring();
    Refocus(m);
    return rows;
}

const hk::Sym* const kWindowClass = &exe::VtRepair;

// ---- the actions: the window's own buttons
static bool Tick(u32, int slot)
{
    const u32 box = screens::Ctl("RepairCheckBox%d", slot);
    if (box)
        uiclick::Click(box), g_soonMs = 50;
    return box != 0;
}
static bool All(u32, int)
{
    const u32 c = uiclick::FindVisible("RepairToggleChecks");
    if (c)
        uiclick::Click(c), g_soonMs = 50;
    return c != 0;
}
static bool Mend(u32, int)
{
    if (!CanMend() || room::NNWindowBusy())
        return false;
    uiclick::Click(uiclick::FindVisible("RepairOkButton")), g_soonMs = 250;
    return true;
}
static bool Close(u32, int) { return screens::PressClose(kCloseButton); }

const screens::ActionDesc kActions[] = {
    {kActTick, "tick", L"TICK", nullptr, nullptr, nullptr, Tick, nullptr, screens::Route::Borrowed, screens::Route::Borrowed, nullptr, false,
     screens::Answer::Now, 0, nullptr, nullptr},
    {kActAll, "all", L"TICK ALL / NONE", nullptr, nullptr, nullptr, All, nullptr, screens::Route::Borrowed, screens::Route::Borrowed, nullptr,
     false, screens::Answer::Now, 0, nullptr, nullptr},
    {kActRepair, "repair", L"REPAIR", nullptr, nullptr, nullptr, Mend, nullptr, screens::Route::Borrowed, screens::Route::Borrowed, nullptr,
     false, screens::Answer::Now, 0, nullptr, nullptr},
    {kActClose, "close", L"CLOSE", nullptr, nullptr, nullptr, Close, nullptr, screens::Route::Borrowed, screens::Route::Borrowed, nullptr,
     false, screens::Answer::Now, 0, nullptr, nullptr},
};
const int kActionCount = sizeof(kActions) / sizeof(kActions[0]);
} // namespace repairscreen
