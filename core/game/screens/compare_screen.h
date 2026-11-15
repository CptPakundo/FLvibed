// FLvibed - the compare block's read (moved from modules\hud\src\nnpage_kit.h ReadCompare in audit 70b F3a, unchanged; that
// name stays there and calls this): what the pages show of an item against what the ship carries, from the game's data
// (gamedata Equip / Against, the tagged item, equip_rules' words). Drawn by modules\hud\src\pages\page_kit.h DrawCompare.
#pragma once
#include "compare_model.h"

namespace screens
{
// Item `arch` against the ship: `mounted` = the item sits on the ship (it compares only with the tagged one). `ok` false
// when the item has no numbers.
CompareBlock ReadCompare(u32 arch, bool mounted);
} // namespace screens
