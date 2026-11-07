// core\game\equip_rules.h (pure): hardpoints from shiparch's lines, which are free, an item against another.
#include "../core/game/equip_rules.h"

#include "test.h"

void Test_equip_rules()
{
    using namespace equiprules;
    std::vector<Hp> hps;
    AddLine(hps, {"hp_gun_special_3", "HpWeapon01", "HpWeapon02"});
    AddLine(hps, {"hp_gun_special_2", "HpWeapon01", "HpWeapon02"});
    AddLine(hps, {"hp_fighter_shield_special_3", "HpShield01"});
    AddLine(hps, {"hp_thruster", "HpThruster01"});
    AddLine(hps, {"hp_bay_external", "HpBay01"});
    CHECK(hps.size() == 5);
    CHECK(hps[0].name == "hpweapon01" && hps[0].types.size() == 2);
    CHECK(SlotWord(BestOf(hps[0])) == L"GUN/MISSILE CLASS 3");
    CHECK(SlotWord(BestOf(hps[2])) == L"LIGHT SHIELD CLASS 3");
    CHECK(SlotWord(BestOf(hps[3])) == L"THRUSTER");
    CHECK(BestOf(hps[4]).word == nullptr);
    CHECK(Accepts(hps[0], "HP_GUN_SPECIAL_2") && !Accepts(hps[0], "hp_gun_special_4"));
    // what is free, by filter; a slot the player cannot see never shows
    const auto free = Free(hps, {"HpWeapon01"}, -1);
    CHECK(free.size() == 3 && free[0].name == "hpweapon02");
    CHECK(Free(hps, {}, 0).size() == 2);
    CHECK(Free(hps, {}, 2).size() == 2);
    // the numbers
    Stat up{L"HULL", 120, 1, L""}, down{L"DRAW", 120, -1, L""}, neutral{L"RATE", 120, 0, L""};
    CHECK(Judge(up, 100) == Verdict::Better && Judge(down, 100) == Verdict::Worse && Judge(neutral, 100) == Verdict::Other);
    CHECK(Judge(up, 120) == Verdict::Same);
    CHECK(DeltaWord(120, 100) == L"+20 %" && DeltaWord(80, 100) == L"-20 %" && DeltaWord(100, 100) == L"=");
    CHECK(DeltaWord(5, 0) == L"+5");
    CHECK(ValueWord(1234.4f) == L"1234" && ValueWord(2.5f) == L"2.5" && ValueWord(3) == L"3");
    float v = 0;
    CHECK(Find({up}, L"HULL", v) && v == 120 && !Find({up}, L"RATE", v));
}
