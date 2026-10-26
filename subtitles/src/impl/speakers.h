// Speaker name colours by character (user, 21 Oct 2026; docs\DISPLAY_PLAN.md "Speaker colours"): every story speaker
// in data\speaker_colors.tsv (owned here; side / story colours fitted by tools\speaker_colors.py) - the main cast its
// own colour, side characters their own shade of their faction, the story's stations / ships / wings a faction tint or
// a hostile red - with a stance (friendly / neutral / hostile, per-mission exceptions). Speakers it does not list keep
// the game's relation colours (display.cpp). The dialogue's who-speaks-near-whom graph checks the table at load.
#pragma once
#include "common.h"

#include <string>

namespace speakers
{
// Loads the table and checks it against data::Lines() (after every data load); logs what the check finds.
bool Load(const std::string& path);
// 0xRRGGBB of a listed speaker, 0 if not listed - or if `hostileNow` turns a story speaker (station, ship, wing) hostile
// against its usual stance: then the caller's hostile relation colour.
u32 Rgb(const std::wstring& speaker, bool hostileNow);
// The table's stance for this speaker on this line (its mission's exception, else the usual stance) is hostile.
bool Hostile(const std::wstring& speaker, const std::string& lineId);
// Every listed speaker's name, concatenated (the characters the name font must hold; display::SeedGlyphs).
std::wstring Names();
// Debug `colors [name]`: every listed speaker's colour, tier, family, stance and closest neighbours; the check's findings.
std::string Report(const std::string& filter);
} // namespace speakers
