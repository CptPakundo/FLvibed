// FLvibed - why the tractor refused an item (GitHub issue 33): the rules, pure. The game knows the reason and drops it: CETractor::VerifyTarget returns a
// TractorFailureCode, and the client's handler (exe::TractorNotify) turns every code from 3 to 6 into the same "Tractor Failure" voice line. core\host\tractorhook.*
// keeps the code the game queued that line for; HUD 2's card and the subtitles' Neural Net line show the game's own text for it (docs\FREELANCER_RE.md "Tractor failure codes").
#pragma once
#include "common.h"
#include "str.h"

#include <string>

namespace tractorfail
{
// TractorFailureCode (Common.dll CETractor::VerifyTarget). 1 no target and 2 out of range have their own handling in the game and no reason to give.
enum Reason : int
{
    kNone = 0,
    kHold = 3,      // the item's volume > CShip::get_cargo_hold_remaining; asked first, so a full hold hides the other three
    kBatteries = 4, // get_shield_battery_capacity_remaining 0: the ship's shield_battery_limit
    kNanobots = 5,  // get_repair_kit_capacity_remaining 0: the ship's nanobot_limit
    kAmmo = 6,      // get_ammo_capacity_remaining 0: munition, mine or countermeasure at MAX_PLAYER_AMMO (Common.dll), mounted gun or not
};

constexpr u32 kEventRefused = 0x14; // CEquip::Notify from CETractor::AddTarget, data -> the code
constexpr u32 kEventArmLost = 0x15; // from TractorArm::Update (a pull that stopped being possible), data -> the arm, code at word 8

// The reason an event carries; kNone for every other event and code. `word0` / `word8` are the data's words, read by the caller.
inline int ReasonOf(u32 event, int word0, int word8)
{
    const int code = event == kEventRefused ? word0 : event == kEventArmLost ? word8 : 0;
    return code >= kHold && code <= kAmmo ? code : kNone;
}

// The game's own line for the reason (resources.dll; 932, 933 and 975 are what its equipment dealer says, 1841 a cargo refusal). 0 = none.
inline u32 TextIds(int reason)
{
    switch (reason)
    {
    case kHold: return 1841;     // "You don't have enough cargo space."
    case kBatteries: return 933; // "Storage capacity for Shield Batteries is full."
    case kNanobots: return 932;  // "Storage capacity for Nanobots is full."
    case kAmmo: return 975;      // "Storage capacity for this ammo is full."
    default: return 0;
    }
}

// The line waits its turn in the game's Neural Net queue (at most 4 lines): a record older than this belongs to some other line.
constexpr u32 kFreshMs = 8000;
inline bool Fresh(u32 ageMs) { return ageMs <= kFreshMs; }

// The game's sentence in the Neural Net's voice: capitals, no full stop. Empty stays empty: no text, no reason shown.
inline std::wstring Shout(std::wstring s)
{
    while (!s.empty() && (s.back() == L'.' || s.back() == L' '))
        s.pop_back();
    return str::Upper(std::move(s));
}

// The Neural Net line with its reason, the one caption of a player without HUD 2's cards.
inline std::wstring Caption(const std::wstring& line, const std::wstring& why)
{
    const std::wstring s = Shout(why);
    return s.empty() ? line : line + L": " + s;
}
} // namespace tractorfail
