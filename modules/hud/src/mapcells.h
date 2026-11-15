// HUD 2 map's influence cells (GitHub #61, the owner's design v2B): on the political layer with the filter ALL, who operates where in the shown system as
// hex cells (core\game\influence.h), with the legend "WHO OPERATES HERE". The system's data is loaded and the cells computed off the game's thread, once per
// system shown and again when the player's relation to one of its factions flips; until then the game's zones stay (core\host\uihooks.cpp). Classic HUD:
// nothing here runs.
#pragma once
#include "common.h"
#include "navmap_layout.h"

#include <string>

namespace mapcells
{
// Once a frame HUD 2's map shows: starts or picks up the work for the shown system. Returns the flags for hostcalls SetQuietZones (uihooks::kZoneCells
// when the cells stand in for the political fill this frame).
int Frame(u32 root);
// The cells over the map's port and the legend in `legend` (from its top), when the political layer's filter is ALL and the cells are ready.
void Draw(u32 root, const navlayout::Rect& port, float H, const navlayout::Rect& legend);
// The worker joined, the cells dropped (HUD 2's give-back list).
void Shutdown();
// Debug: the state line (`navnew cells`).
std::string Status();
} // namespace mapcells
