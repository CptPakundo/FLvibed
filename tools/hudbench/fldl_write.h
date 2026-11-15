// FLvibed design bench - the display list writer: recorded kit passes (kitlist.h) to the files tools\hudbench\FORMAT.md
// describes (fldl/0 lists, one flatlas/0 atlas with its PNG, probes for the replayer's check, the font fingerprints).
// Every file of a run made from an install carries the private marker (tools\private_check.py); a synthetic run's are
// public. The list itself (Meta, Marker, ListJson) is the kit's, shared with the game's recorder: core\ui\hud2_kit_fldl.h.
#pragma once
#include "kitlist.h"
#include "ui/hud2_kit_fldl.h"

#include <string>
#include <vector>

namespace fldl
{
// A pixel the replay must show: (x, y) in the list's pixels, rgb, a tolerance per channel, and the replay target's alpha
// when a >= 0.
struct Probe
{
    std::string name;
    int x = 0, y = 0;
    u32 rgb = 0;
    int tol = 3;
    int a = -1;
};

// One atlas for every texture of the run: each box copied with its own texels and two empty texels round it, so linear
// filtering reads what it reads in the game's atlas (the game's boxes have two empty texels between them too).
// Writes <base>.png and <base>.json (the manifest names the PNG by its file name).
bool WriteAtlas(const std::string& base, bool priv, std::string& error);

std::string ProbesJson(const std::vector<Probe>& probes);

// The fingerprint of every font set built (the game's `hud2 fontdump` format, render::FontPrint), sorted.
std::string Prints();
// Compares each built set with the block of the same face, px and weight in a game's fontdump file. Returns the number
// of sets compared; `diff` names the first differing line of each set that differs, `missing` the sets the dump lacks.
int ComparePrints(const std::string& gameDump, std::string& diff, std::string& missing, int& differing);

bool WriteFile(const std::string& path, const std::string& bytes);
} // namespace fldl
