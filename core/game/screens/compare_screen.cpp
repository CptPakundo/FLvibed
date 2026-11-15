// The compare block's read: compare_screen.h.
#include "compare_screen.h"

#include "game/equip_rules.h"
#include "game/gamedata.h"
#include "game/hudinfo.h"
#include "ui/hud2_kit.h"

namespace screens
{
static std::wstring Upper(std::wstring s)
{
    for (wchar_t& c : s)
        c = towupper(c);
    return s;
}

CompareBlock ReadCompare(u32 arch, bool mounted)
{
    using namespace hud2kit;
    CompareBlock b;
    const equiprules::Spec* me = gamedata::Equip(arch);
    if (!me || me->stats.empty())
        return b;
    using namespace equiprules;
    // a mounted item compares only with the tagged one, where that sits on a hardpoint this one could go on (Shockwave, 7 Nov: with an empty hold the tag showed nothing)
    gamedata::Versus v = gamedata::Against(arch);
    if (mounted && (!v.tagged || v.arch == arch))
        v = gamedata::Versus();
    const Spec* other = v.arch ? gamedata::Equip(v.arch) : nullptr;
    b.ok = true, b.headRgb = kChrome, b.versus = other != nullptr;
    if (mounted && !(v.tagged && other))
        b.head = L"MOUNTED"; // (else VS ITS NAME (TAGGED) below: the row says MOUNTED, and the middle column is narrow at 1280x800)
    else if (v.hasSlot && !v.fits)
        b.head = L"NO HARDPOINT ON THIS SHIP TAKES IT", b.headRgb = kAmber;
    else if (v.tagged && other) // (the player's choice first, a free hardpoint or not)
        b.head = L"VS " + Upper(hudinfo::EquipName(v.arch)) + L" (TAGGED)", b.headRgb = kActive;
    else if (v.freeHp)
        b.head = L"FITS A FREE HARDPOINT", b.headRgb = kFriend;
    else if (other)
        b.head = L"VS " + Upper(hudinfo::EquipName(v.arch));
    else
        b.head = L"NUMBERS";
    b.slot = SlotWord(SlotOf(me->type));
    for (const Stat& s : me->stats)
    {
        CompareRow row;
        row.label = s.label, row.value = ValueWord(s.value) + L" " + s.unit;
        float theirs = 0;
        if (other && Find(other->stats, s.label, theirs))
        {
            const Verdict j = Judge(s, theirs);
            row.delta = DeltaWord(s.value, theirs), row.deltaRgb = j == Verdict::Better ? kFriend : j == Verdict::Worse ? kHostile : kChrome;
        }
        b.rows.push_back(row);
    }
    return b;
}
} // namespace screens
