// FLvibed - what the NeuroNet's left column says of where the player is (LOG's and INFO's; the HUD 2 page framework, audit
// 70b F4a, moved from modules\hud\src\nnpage.cpp unchanged): the system and the base or zone, and what stands near, as plain
// values. Filled by places_screen.cpp; drawn by modules\hud\src\pages\page_kit.h (Here, PlaceList). Nothing here calls the game.
#pragma once
#include <string>

namespace screens
{
// A place the player has found in this system, or a ship near: its name, what it is (a class, where a gate leads, what a
// base offers; a ship's faction), how far it is (metres) and, for a ship, what it thinks of the player (-1 .. 1).
struct Place
{
    std::wstring name, what;
    float distance = 0, attitude = 0;
};

// Where the player is: docked, the system and the base; in space, the system and the zone.
struct Here
{
    bool docked = false;
    std::wstring system, place;
};
} // namespace screens
