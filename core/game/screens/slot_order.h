// FLvibed - the ship's items in its slots' fixed order (issue 52, the owner: a slot's place is part of the ship), for the lists of
// the ship's items (the equipment dealer's YOUR SHIP, the NeuroNet's INVENTORY). Moved from modules\hud\src\nnpage_kit.h in audit
// 70b F3b, where the names stay as using-declarations, so the dealer's screen (market_screen.cpp) reads its rows in core.
#pragma once
#include "common.h"
#include "game/equip_rules.h"
#include "game/gamedata.h"
#include "safemem.h"

#include <string>
#include <vector>

namespace screens
{
// An EquipDesc's hardpoint, lower case ("" in the hold): its name through the pointer at +8 (baseinfo's WalkEquipment reads the same, node +0x10).
inline std::string DescHardpoint(u32 desc)
{
    char hp[33] = {};
    u32 p = 0;
    safemem::Read(desc + 8, &p, 4);
    if (!p || !safemem::Read(p, hp, 32))
        return std::string();
    return equiprules::Lower(hp);
}

// A list of the ship's items in its slots' fixed order: every hardpoint of the filter where it stands, what is mounted there or an
// EMPTY row; then the rest (the hold, a hardpoint not read) in the game's order. `hpOf(row)` the row's hardpoint, `empty(i)` the EMPTY
// row of free[i]; `free` comes back holding the empty slots in that order.
template <class R, class HpOf, class Empty>
std::vector<R> InSlotOrder(const std::vector<R>& rows, int filter, std::vector<equiprules::Hp>& free, HpOf hpOf, Empty empty)
{
    std::vector<R> out;
    std::vector<bool> used(rows.size(), false);
    free.clear();
    for (const equiprules::Hp& h : gamedata::ShipSlots(filter))
    {
        size_t at = rows.size();
        for (size_t i = 0; i < rows.size() && at == rows.size(); ++i)
            if (!used[i] && hpOf(rows[i]) == h.name)
                at = i;
        if (at < rows.size())
        {
            used[at] = true;
            out.push_back(rows[at]);
            continue;
        }
        free.push_back(h);
        out.push_back(empty(static_cast<int>(free.size()) - 1));
    }
    for (size_t i = 0; i < rows.size(); ++i)
        if (!used[i])
            out.push_back(rows[i]);
    return out;
}
} // namespace screens
