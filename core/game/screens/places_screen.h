// FLvibed - the reads of the NeuroNet's left column (LOG's model and INFO's shell; audit 70b F4a, moved from
// modules\hud\src\nnpage.cpp unchanged): where the player is, what stands near, what a base offers.
#pragma once
#include "places_model.h"
#include "game/gamedata.h"

#include <vector>

namespace screens
{
// Where the player is now (the docked base's names, else the HUD's system and zone).
Here ReadHere();
// What stands in this system and the player has found, nearest first (the target menu's own test: no lane ring, nothing
// undiscovered), each with what the data says of it; the ships near, nearest first. Docked: both empty.
void ReadPlaces(std::vector<Place>& places, std::vector<Place>& ships);
// A base in the words of the data: what it offers ("SHIPS, EQUIPMENT, TRADE, BAR"; "" nothing).
std::wstring Offers(const gamedata::BaseFacts& b);
} // namespace screens
