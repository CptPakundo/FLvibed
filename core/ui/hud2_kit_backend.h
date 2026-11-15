// FLvibed - the seam inside HUD 2's drawing kit (hud2_kit.h). hud2_kit.cpp builds the pass with no device: the
// tessellation, the text layout and fitting, the six vertex lists. What needs the game or Direct3D is behind the two
// sets of calls below, defined by exactly one file of the program: in the game core\ui\hud2_kit_d3d8.cpp (the fonts
// through core\game\render, the faces through core\game\gamefont, the icons through core\game\flicons, the lists
// drawn with Direct3D 8); a program with no game, such as the design bench
// (..\..\..\audit_2026-11-12\71b_bench_design_v2.md), links its own: the same lists, written out instead of drawn.
// Split out of hud2_kit.cpp unchanged (bench B1b, 12 Nov 2026). Only the kit, its back ends and its list writer use it
// (hud2_kit_d3d8.cpp; the bench's tools\hudbench\kitlist.cpp, B2; hud2_kit_fldl.*, B4b); everything else draws through hud2_kit.h (the
// build's generated core_all.h includes every core\ui header, so its names are visible there all the same).
#pragma once
#include "common.h"
#include "game/flicons.h"
#include "game/fontraster.h"

#include <string>
#include <vector>

namespace fldl
{
struct Texture;
}

namespace hud2kit
{
// The vertex the lists hold: Direct3D's pre-transformed XYZRHW | DIFFUSE | TEX1, in back-buffer pixels with the -0.5
// texel-centre shift applied.
struct Vertex
{
    float x, y, z, rhw;
    u32 color;
    float u, v;
};

// The six layers, drawn in this order, one draw call each: shapes (the small font's white texel), big icons, small icons
// (the flat icon atlases), small text, big text, the game caption's text.
enum LayerId
{
    LShape = 0,
    LIconBig,
    LIconSmall,
    LTextSmall,
    LTextBig,
    LTextCaption,
    LCount
};
struct Layer
{
    std::vector<Vertex> v;
    void* tex = nullptr; // the layer's texture (a font's atlas, an icon atlas); a layer without one is not drawn
};

// A cut mark (hud2kit::Mark): every layer's vertex count when it was set, so a list can say which ranges a part drew.
struct MarkAt
{
    const char* name; // a string literal
    u32 counts[LCount];
};
// The marks set since the pass's Begin, in order (read by a back end's Submit).
const std::vector<MarkAt>& Marks();
// The pass as End would submit it now (the design bench's recorder in the game, hud2_kit_fldl.h CopyOpenPass: HUD 2's
// pass copied before its End): its layers, size and colour-writes-only flag.
struct OpenPass
{
    const Layer* layers;
    int sw, sh;
    bool keepDestAlpha;
};
OpenPass PassNow();

namespace backend
{
// ---- KitAssets: the fonts, the game's faces, the icons
bool GameFont();             // the game's label face resolved (gamefont::Load); false = its fallback face
const wchar_t* LabelFace();  // the label face's name (gamefont::LabelFace)
bool BuildFont(render::Font& font, const wchar_t* face, int px, int weight, const std::wstring& chars);
void FreeFont(render::Font& font);
// An icon's atlas texture and UV rectangle for a draw sizePx tall; false while the atlas is not ready.
bool IconUv(flicons::Icon icon, float sizePx, void** texture, float uv[4]);
// A layer's texture as a display list names it (hud2_kit_fldl.h): a font set's glyph boxes and fingerprint, or an icon
// atlas's cells. False for a texture the back end did not make. Research only (the recorder; nothing calls it per frame).
bool Describe(const void* texture, fldl::Texture& out);

// ---- KitBackend: the device
void Bind(void* device); // Begin's device: a new one forgets the device state saved for the last
// Draws the layers in order on the bound device, sw x sh viewport, colour writes only when keepDestAlpha (the back
// buffer's alpha kept); `calls` counts the draw calls made. The caller has checked a device is bound and a vertex exists.
void Submit(const Layer* layers, int count, int sw, int sh, bool keepDestAlpha, u32& calls);
void DeviceLost(); // before a device Reset: the saved device state released
} // namespace backend
} // namespace hud2kit
