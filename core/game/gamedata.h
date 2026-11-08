// FLvibed - what the game's own data files say about equipment, ships, the player's system and bases, for the pages of modules\hud\src\nnpage.* and dealerpage.*:
// read once through ini::Read (text or BINI, whatever the mod ships), cached, every answer failing closed (empty = unknown). Nothing here shows a place: the caller keeps the game's
// discovery test (hudinfo::Undiscovered). Rules: core\game\equip_rules.h. Compiled into each module part that uses it (core\game).
#pragma once
#include "common.h"
#include "game/equip_rules.h"

#include <string>
#include <vector>

namespace gamedata
{
// An item's numbers (guns through baseinfo::GunOf; the rest from the equipment inis), null unknown.
const equiprules::Spec* Equip(u32 arch);
// An item's infocard text id (ids_info), 0 unknown: text::InfoCard.
u32 EquipInfo(u32 arch);
// What a launcher fires (a gun's, mine or countermeasure dropper's projectile_archetype: for a missile, mine or flare the ammo sold), 0 none or unknown.
u32 Fires(u32 launcher);
// A ship archetype's hardpoints (shiparch.ini hp_type), empty unknown.
const std::vector<equiprules::Hp>& ShipHardpoints(u32 shipArch);

// A ship archetype's numbers for a dealer: class word, hold, hull, what it mounts.
struct ShipFacts
{
    u32 arch = 0, nameIds = 0, infoIds[3] = {};
    float hull = 0, hold = 0, nanobots = 0, batteries = 0;
    int guns = 0, gunClass = 0, turrets = 0, torpedoes = 0, shieldClass = 0;
    bool ok = false;
};
ShipFacts Ship(u32 shipArch);
// By the name the game shows ("Patriot"), the dealer's rows keep no id that reads.
ShipFacts ShipNamed(const std::wstring& name);

// What the item would stand against on the player's ship: the weakest mounted item on a hardpoint that takes it.
struct Versus
{
    u32 arch = 0;         // the mounted item, 0 none
    bool fits = false;    // a hardpoint of this ship takes it
    bool freeHp = false;  // and one of those is free
    bool hasSlot = false; // the item goes on a hardpoint at all
    bool tagged = false;  // `arch` is the item the player tagged (TagCompare), not the weakest that would go
};
Versus Against(u32 arch);
// The mounted item the store's items compare with where it could be replaced by them (issue 41's sibling 44: the weakest of a kind stood for every gun);
// 0 clears. By archetype: two of one item have the same numbers.
void TagCompare(u32 arch);
u32 TaggedCompare();
// The hardpoints free now under `filter` (-1 all).
std::vector<equiprules::Hp> FreeHardpoints(int filter);
// The player's ship's hardpoints of an inventory filter in their fixed order (equiprules::Slots), mounted or not.
std::vector<equiprules::Hp> ShipSlots(int filter);
// The mountable items in the hold that one hardpoint takes.
std::vector<u32> HoldItemsFor(const equiprules::Hp& hp);

// ---- the world
struct Faction
{
    std::string nick;
    u32 nameIds = 0;
    float weight = 0, standing = 0; // the player's standing, 0 unknown
};
// A base's facts: mbases.ini, its rooms, its market.
struct Good
{
    u32 arch = 0;
    float price = 0;  // the base's price (goods.ini price x its factor)
    float factor = 1; // against the good's base price
    bool sold = false; // false: only bought here
};
// What a base's dealer asks of the buyer of a good (its market line): the level, and the standing with the base's owners (-1 none).
struct Need
{
    u32 arch = 0;
    int level = 0;
    float rep = -1;
};
struct BaseFacts
{
    u32 id = 0, nameIds = 0;
    std::string owner;       // mbases.ini local_faction: the "base owners" of a reputation refusal
    std::vector<Need> needs; // by good (its equipment / ship id), only the goods that ask something
    std::vector<Faction> factions; // who keeps an office here, the heaviest first
    bool bar = false, trader = false, equipment = false, ships = false;
    int offersMin = 0, offersMax = 0; // the job board's size
    std::vector<Good> goods;          // commodities
    bool ok = false;
};
const BaseFacts& Base(u32 baseId);
// The player's system: who flies there, what stands in it.
struct SolarFact
{
    u32 id = 0;           // the object's id: hudinfo::Undiscovered
    u32 base = 0;         // its base, 0 none
    u32 owner = 0;        // reputation ids_name
    std::string goesTo;   // a gate or a hole's system nickname, "" none
    std::wstring goesToName;
};
struct SystemFacts
{
    u32 id = 0;
    std::vector<Faction> factions; // the zones' encounters, the heaviest first
    std::vector<SolarFact> solars;
    int fields = 0, nebulae = 0;
    bool ok = false;
};
const SystemFacts& System(u32 systemId);
// A group's display name's ids by nickname (initialworld.ini), 0 unknown.
u32 FactionIds(const std::string& nick);
// The player's standing with a group by nickname; false unknown.
bool Standing(const std::string& nick, float& out);
} // namespace gamedata
