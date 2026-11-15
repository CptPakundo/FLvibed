// HUD 2's NPC icons (GitHub 84 + 85; design audit 146_, build 147_): each NPC ship's own top-down outline, drawn from its
// model file (shiparch.ini DA_archetype, any [Data] ships file: mods too) once per archetype on a worker thread, baked per
// pixel size into one managed atlas texture and drawn through hud2kit::ShipIcon (no 3D, no file per frame); and the pilot's
// rank tier from the factions' rank words (faction_prop.ini rank_desig). The pure half: npcicons_rules.h.
#pragma once
#include "common.h"
#include "game/flicons.h"

#include <istream>
#include <string>

namespace npcicons
{
// A ship's class icon (the v1 fallback while its outline is not baked, or when it has none): the atlas Transport for a
// freighter or transport (hudinfo::ClassName's word), else the Fighter; never the Enemy chevron (hostility is the colour).
flicons::Icon ClassIcon(const std::wstring& classWord);
// The rank tier 1..3 of a pilot's shown name (0 none), `wordLen` the characters of its rank word and the space after it.
int Rank(const std::wstring& name, int* wordLen);
// A ship archetype that will be drawn: its outline starts building now (the 10 Hz refresh asks before the draw does).
void Want(u32 arch);
// The test verb's name fixture (`npcicons name <ship nickname> <text>`): an NPC ship of that archetype is shown under
// `text` (a long name, a tier 2 / 3 rank word the traffic did not bring); untouched without one (the player never sets it).
void ShownName(u32 arch, std::wstring& name);
// HUD 2's give-back (its Leave, the module's stop or a logic reload, a fault: hud2_entry.cpp GiveBack): the worker joined,
// the atlas freed, the kit's source cleared; a later Want starts it all again.
void Shutdown();
// Test verb `npcicons [state]`: the counters (outlines built / failed and their cost, bakes and their cost, cells);
// `npcicons name <ship nickname> <text...>` the name fixture above; `npcicons fail <ship nickname>` that archetype draws as
// if its model gave no outline (the class icon: the fallback, forced); `npcicons clear` drops both fixtures.
std::string Command(std::istream& in);
} // namespace npcicons
