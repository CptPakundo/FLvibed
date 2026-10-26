// FLvibed - a ship archetype's display name (shiparch.ini [Ship] ids_name) as the game shows it ("Starflier"), for HUD 2's
// ship panel. Common.dll Archetype::GetShip(id) -> the archetype (an Archetype::Root: its id checked through
// Root::get_id, ids_name at +0x14 as for equipment, core\game\hudinfo.cpp EquipIds); the text through core\text. Cached
// per archetype; "" unknown (fails closed). Compiled into each module part that uses it.
#pragma once
#include "common.h"

#include <string>

namespace shipname
{
// The ship archetype's display name (ai::Self::shipArch), "" unknown.
std::wstring Name(u32 shipArch);
} // namespace shipname
