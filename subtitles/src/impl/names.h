// What the game knows about a comm's sender: faction name and attitude towards the player
// (server.dll pub::SpaceObj / pub::Reputation, the same data the HUD colours contacts by).
#pragma once
#include "common.h"

#include <string>

namespace names
{
struct Sender
{
    bool known = false;
    std::wstring faction; // short group name, e.g. "Liberty Police"
    float attitude = 0;   // towards the player, -1..1
};

Sender Lookup(u32 spaceObj);
// A base's bar character speaking with this voice (DATA\MISSIONS\mbases.ini [MBase] + its [GF_NPC]: voice,
// individual_name, affiliation), as "Name · Faction" (the faction's short group name); "" when the base has no such
// character or several share the voice. dataDir: the game's DATA folder, trailing backslash.
std::wstring BarSpeaker(const std::string& dataDir, u32 baseId, u32 voiceId);
// The same for one room character: its mbases.ini individual_name (room character +0x160) at this base; true with
// `label` ("Name · Faction") and `voice` (its voice id) when the base lists it.
bool BarCharacter(const std::string& dataDir, u32 baseId, u32 nameIds, std::wstring& label, u32& voice);
// Forgets the bar characters read from mbases.ini (debug `reloaddata`): the next lookup reads the file again.
void ResetBarNpcs();
// Text of a Freelancer string resource id (resources.dll + freelancer.ini [Resources] DLLs).
std::wstring IdsText(u32 ids);
} // namespace names
