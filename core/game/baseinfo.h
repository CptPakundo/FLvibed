// FLvibed - what Extended HUD 2's docked overlay (controller\src\impl\uistrip.*, docs\UI_HUD2_EXTENDED.md 10) reads from
// the game: the base, room, system, credits, cargo and ship for the context strip; a pending dealer purchase; the price
// marks' verdicts; the equipment compare; the ship dealer's selection and refusal; a room character's name card; the
// job board's type word. Every value is the game's own (its UI objects' fields pinned by signed code sites in
// core\flexe.h "Extended HUD 2 readers", server.dll / Common.dll exports, the game's data) and fails closed: unknown =
// -1 / "" / false, never guessed. Cheap: names cached by id; the callers refresh at most 10 Hz. Facts:
// docs\FREELANCER_RE.md "Extended HUD 2 readers". Compiled into each module part that uses it (core\game).
#pragma once
#include "common.h"

#include <string>
#include <vector>

namespace baseinfo
{
// ---- the context strip
// Docked on a base (Freelancer.exe's player base id is set).
bool Docked();
// The base's display name ("Planet Manhattan"), "" unknown.
std::wstring BaseName();
// The room as a word from its nickname ("Li01_01_ShipDealer" -> "SHIP DEALER"), "" unknown.
std::wstring RoomName();
// The player's system ("New York"), "" unknown.
std::wstring SystemName();
// The player ship's archetype name ("Starflier"), "" unknown.
std::wstring ShipName();
// Credits, -1 unknown.
long long Credits();
// The hold: used and capacity (volume units), false unknown.
bool Cargo(int& used, int& capacity);

// ---- the visible controls (one walk of the game's control list; the caller refreshes it at most 10 Hz)
struct Ctl
{
    u32 ctl = 0, vt = 0;
    char name[33] = {};
};
void Refresh();
const std::vector<Ctl>& Visible();
// The n-th (1-based) visible control of that name in the list's order, 0 none.
u32 Find(const char* name, int nth = 1);

// ---- dealers (commodity / equipment: NN_Dealer + NN_Inventory)
// A purchase the game would make now: a dealer row selected and the transfer panel's buy button shown (the game shows it
// only when the good can be bought: it fits, it is affordable, it is stocked). qty = the slider's position (1 without one).
struct Transfer
{
    u32 good = 0;
    int qty = 0;
    float unitPrice = -1; // the base's price (pub::Market::GetPrice), -1 unknown
    float volume = -1;    // per unit (Archetype::Equipment volume), -1 unknown
    int side = 0;         // the dealer's kind: 1 commodity trader, 3 equipment dealer
};
bool PendingPurchase(Transfer& out);
// The game's verdict on a price mark control (ItemGoodDeal0N): its hint IDS as the dealer set it.
enum Deal
{
    DealUnknown = 0,
    DealGood,  // buying: the dealer's low price (green square); selling: a high offer (green circle)
    DealFair,  // yellow
    DealHigh,  // buying: a high price (red square)
    DealLow,   // selling: a low offer (red circle)
};
Deal DealOf(u32 markCtl);
// A gun's numbers from its archetype: damage per shot (hull, munition + explosion), shots per second, range (m).
struct Gun
{
    u32 arch = 0;
    std::wstring name;
    float damage = -1, rate = -1, range = -1;
    // per second, as the compare line shows them (critique 3, 6: per shot misled - DAMAGE +500 % for a slower gun with
    // +43 % hull DPS): hull damage x rate; shield damage x rate = (hull x HULL_DAMAGE_FACTOR + energy_damage) x rate
    // (FREELANCER_RE "Shield damage per hit"; weapon_equip.ini / constants.ini); energy = power_usage x rate;
    // speed = muzzle velocity (m/s). -1 unknown.
    float hullDps = -1, shieldDps = -1, energyPs = -1, speed = -1;
    std::vector<u32> hpTypes; // the hardpoint types it mounts on (hp_gun_type)
    bool homing = false;      // a missile launcher (its munition seeks)
};
bool GunOf(u32 arch, Gun& out);
// The equipment dealer's selected gun and the mounted gun it would compete with (a mounted gun sharing a hardpoint
// type, else the first of the same kind). False when either is unknown / not a gun.
bool CompareSelected(Gun& selected, Gun& mounted);
// The player's equipment list as the server keeps it (mounted and cargo), readable docked.
struct Item
{
    u32 arch = 0;
    int count = 0;
    bool mounted = false;
    std::string hardpoint;
};
std::vector<Item> Equipment();

// ---- the ship dealer (NN_ShipTrader)
struct ShipOffer
{
    int selected = -1;     // 0.. the offered ship selected, -1 none
    int refusal = 0;       // the game's: 0 none, 1 level, 2 other (IDS 0x7001E), 3 credits
    int level = -1;        // the level it needs (refusal 1)
    long long price = -1;  // the offered ship's price
    long long budget = -1; // the credits plus the current ship's trade-in, as the game compares them
    long long credits = -1;
};
bool SelectedShip(ShipOffer& out);

// ---- room characters (the host's live list: room::SetCharacterSource's objects)
// The name card the game shows on hover: lines faction, [role,] name. (Where: the game's `ToolTip` control, whose
// position is the card's top-left corner - the caller reads it.)
struct Card
{
    std::wstring faction, role, name;
    int attitude = -2; // the player's standing with the faction: -1 hostile, 0 neutral, 1 friendly, -2 unknown
};
bool CharacterCard(void* obj, Card& out);
bool CharacterHovered(void* obj);
// The player's conversation partner (+0x36C).
bool CharacterTalking(void* obj);
// The character's box on screen as the game keeps it for picking and its name card (pixels, top-left origin; +0x2F4 ->
// +0x10 int x, y, w, h): the pad walks the characters in screen order by it. False when unknown.
bool CharacterScreenRect(void* obj, float& x, float& y, float& w, float& h);

// ---- the job board / text helpers
// A control's hint text (its +0xCC IDS: the job board's type icon "Assassinate"), "" none.
std::wstring HintText(u32 ctl);

// Debug: one line per reader (`uistrip data`), research dumps (`uistrip research <what>`).
std::string Status();
std::string Research(const std::string& what);
} // namespace baseinfo
