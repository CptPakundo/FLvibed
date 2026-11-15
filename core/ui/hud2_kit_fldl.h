// HUD 2's draw kit: its passes as display lists (tools\hudbench\FORMAT.md, fldl/0), one writer for both programs that
// make them - the design bench (tools\hudbench: its list back end keeps each pass End submits) and the game (`hud2 bench
// record`, modules\hud\src\hud2_record.cpp: HUD 2's pass copied before its End). Each triangle's texture is named by the
// box its texture coordinates fall in (a glyph's inked box, an icon cell, a font's white block), so the kit carries no
// names and pays nothing for them; a triangle that samples no box, or reaches past its box, fails the whole list (never
// written in part). No device and no game: the textures are described by the kit's back end (hud2_kit_backend.h
// Describe). Moved out of tools\hudbench\fldl_write.cpp (bench B4b, 13 Nov 2026), its output unchanged.
#pragma once
#include "common.h"
#include "hud2_kit.h"
#include "hud2_kit_backend.h"

#include <memory>
#include <string>
#include <vector>

namespace fldl
{
// The private marker, assembled so that this source never holds it whole (tools\private_check.py would flag the file).
std::string Marker();
// A JSON string of UTF-8 text; UTF-16 as UTF-8.
std::string Quote(const std::string& s);
std::string Utf8(const std::wstring& w);

// One box of a texture, in texels: a glyph's inked box (glyph:<set>:U+XXXX) or an icon cell (icon:<Name>.<cell>).
struct Box
{
    std::string name;
    int x = 0, y = 0, w = 0, h = 0;
};
// A texture a layer draws with, as a list names it: a font set (its glyph boxes; its 4 x 4 white block at the origin is
// `white`, never a box) or an icon atlas (its cells), and for a font what the list's `sets` carries.
struct Texture
{
    std::string set; // a font set's name (SetName, the bench adds .2, .3 for a later build of the same three) or icons-<cell>
    bool font = false;
    int w = 0, h = 0;
    std::vector<Box> boxes;
    std::wstring face;
    int px = 0, weight = 0, ascent = 0, lineHeight = 0, source = 0; // source: 1 the install's font file, 2 Windows', 0 synthetic
    std::string print; // the set's fingerprint block (render::FontPrint, the `hud2 fontdump` format)
};
// A font set's name: the face's letters and digits, px and weight (`AgencyFB-21-700`).
std::string SetName(const std::wstring& face, int px, int weight);
// An icon atlas of `cell` texels per icon (flicons_decode's layout: icon i at cell (i % kCols, i / kCols)), no pixels.
void IconAtlas(int cell, Texture& out);

// One pass as the kit submits it; its textures by pointer (the caller keeps them alive while the pass is used).
struct Pass
{
    int sw = 0, sh = 0;
    bool keepDestAlpha = false;
    std::vector<hud2kit::Vertex> v[hud2kit::LCount];
    const Texture* tex[hud2kit::LCount] = {};
    std::vector<hud2kit::MarkAt> marks;
};

struct Meta
{
    std::string state, variant;
    std::string buildGame, buildBench;
    bool priv = true; // the private marker, else "public"
    // the game's recorder: the record this list was drawn from (its file name) and the FNV-1a 64 of that frame's inputs as
    // the record writes them (Render's record), so a list never stands apart from what drew it; both empty elsewhere
    std::string record, inputs;
};

// The list of one frame: `under` (may be null: an empty under pass) and `over`, the kit recorder's items. False with
// `error` naming the first triangle no box holds.
bool ListJson(const Meta& meta, const Pass* under, const Pass& over, const std::vector<hud2kit::Drawn>& items,
              std::string& out, std::string& error);

// The kit's open pass (the game's recorder: before End), each layer's texture described by the back end into `textures`
// (kept by the caller as long as `out` is used). False, `error` set, for a layer whose texture the back end does not know.
bool CopyOpenPass(Pass& out, std::vector<std::unique_ptr<Texture>>& textures, std::string& error);
// FNV-1a 64 over a layer's vertices as they are in memory: the hash `hud2 kitdump` prints per layer.
unsigned long long LayerHash(const std::vector<hud2kit::Vertex>& v);
} // namespace fldl
