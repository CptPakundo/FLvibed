// FLvibed design bench - the draw kit's list back end: core\ui\hud2_kit_backend.h for a program with no game and no
// device. The kit (core\ui\hud2_kit.cpp, unchanged) builds its six vertex lists as in the game; here End's Submit copies
// them into a recorded pass instead of drawing them, the fonts are rasterised by core\game\fontraster (the game's own
// GDI path) into images kept in memory, and the icons come from the install's flat icon file
// (core\game\flicons_decode). fldl_write.* turns the recorded passes into display lists (FORMAT.md).
//
// Two sources, chosen once by Open:
//   - an install: the game's label face resolved from <install>\DATA by gamefont::LoadFrom, as the game resolves it; a
//     face that would fall back to Windows' Segoe UI is refused (the bench never draws with a face the game would not
//     use: ..\..\..\audit_2026-11-12\71b_bench_design_v2.md section 3);
//   - synthetic: block glyphs and disc icons made up here, no font file, no game art, no real metrics: the unit test and
//     the replayer's check run on these, and their lists are public.
// The host reads the kit still makes (the pad's glyph family and glyph_look) are this program's inputs (Inputs).
#pragma once
#include "common.h"
#include "game/flicons.h"
#include "ui/glyphs.h"
#include "ui/hud2_kit.h"
#include "ui/hud2_kit_backend.h"
#include "ui/hud2_kit_fldl.h"

#include <memory>
#include <string>
#include <vector>

namespace kitlist
{
// One texture the kit can name: a font's atlas or an icon atlas, as the game would upload it (its description the list
// writer's, core\ui\hud2_kit_fldl.h; a font set's name gets .2, .3, ... for a later build of the same face, px and weight).
using Box = fldl::Box;
struct Texture : fldl::Texture
{
    std::vector<u32> argb; // w x h, 0xAARRGGBB top-down, the bytes the game uploads (fontraster::AtlasArgb)
};

// One pass as End submitted it.
using Pass = fldl::Pass;

struct Inputs
{
    glyphs::Family family = glyphs::Family::Xbox; // the pad family the game would have detected
    int glyphLook = 0;                            // the glyph_look setting (0 automatic, 1 coloured, 2 HUD 2's)
    bool icons = true; // the icon atlas stands (the game's flicons::Ready): off, every Icon fails and the parts draw shapes
};

enum class Source
{
    Install,
    Synthetic
};
// Opens the source. Install: `install` is the game's folder (holding DATA and EXE). Returns 0, or 2 with `why` set when
// the install is refused (no fonts.ini, the label face would be the fallback, no flat icon sheet).
int Open(Source source, const std::wstring& install, std::string& why);
Source Current();
Inputs& In();

// The passes submitted since the last Take, moved out.
std::vector<Pass> Take();
// Every texture made so far (kept alive for the whole run: a recorded pass names them by pointer).
const std::vector<std::unique_ptr<Texture>>& Textures();
// The device handle the kit's Begin takes (no device: any pointer that is not null and stays the same).
void* Device();
} // namespace kitlist
