// Controller glyphs drawn by FLvibed itself (the UI build record 1 "Glyphs", private; UI_PLAN.md 6.3): GDI shapes and letters
// rasterised at the needed pixel size into an atlas texture (render::Image), so they are sharp at any resolution and
// need no art file, licence or download. Four families over one positional layout (south = confirm): Xbox (A B X Y
// coloured letters), PlayStation (cross circle square triangle), Nintendo (B A Y X), Steam (A B X Y plain, L1 / R1). The
// family comes from `glyph_style`, automatic = the connected pad's HID vendor (Sony 054C PlayStation, Nintendo 057E
// Nintendo, a real Valve device 28DE or `SteamDeck=1` in the environment Steam, else Xbox). Steam's virtual gamepad
// (28DE:11FF) never decides: Steam Input shows it for any pad, an Xbox one included.
#pragma once
#include "common.h"
#include "game/render.h"

#include <string>

namespace glyphs
{
// Positional button ids (the pad's physical places; the preset tables and the spec's strings use these names).
enum Id : int
{
    A = 0, // south
    B,     // east
    X,     // west
    Y,     // north
    LB,
    RB,
    LT,
    RT,
    LS, // left stick (an axis)
    RS,
    L3, // stick press
    R3,
    DU, // D-pad
    DD,
    DL,
    DR,
    VIEW,
    MENU,
    COUNT
};
enum class Family
{
    Xbox,
    PlayStation,
    Nintendo,
    Steam, // the Deck / Steam Controller's own names: Xbox's face buttons and View / Menu, L1 / L2 / R1 / R2 (GitHub issue 19)
};

Id FromName(const char* name); // "A" .. "MENU" (the spec's names); COUNT if unknown
const char* Name(Id id);
// The menu grammar's button for a prompt written as A (accept) / B (back): the same, or exchanged when confirm_button
// is 1 (the right button accepts: the Nintendo habit; device.h ConfirmMask). Every other id is returned as it is.
// Flight bindings are positional and never go through this (the UI build record 5, 29 Oct 2026).
Id Menu(Id id);
// The same as a pure function of the setting's value (`confirmButton`: confirm_button), for a caller that hands it in.
inline Id MenuFor(int confirmButton, Id id)
{
    if (confirmButton != 1)
        return id;
    return id == A ? B : id == B ? A : id;
}
Family Current();
const char* FamilyName(Family f);

// Once per frame (cheap): applies glyph_style, re-detects the family when a real pad appears or leaves. `vendor`: the
// pad's USB vendor id when its device layer names it (Gamepad::Vendor: a device-layer test pad, or the SDL backend's pad by
// its SDL_GamepadType: pad_backend.h VendorForType), else 0 = the HID list.
void Update(bool realPadConnected, u32 vendor = 0);
// The D3D device the atlases are built on (render::SetDevice's; a new device rebuilds them).
void SetDevice(void* device);

// The glyphs' look is the HUD's own (TASK_GLYPHS_HUD.md 5): the body in the panel base, the outlines and the letters on
// bumpers / triggers / sticks / View / Menu in a tint colour given per draw - the HUD's chrome normally, the game's active
// yellow for a badge on a lit control; the face buttons keep their family colours, their rings too (OutlineRgb; a lit
// badge's ring stays yellow).
constexpr u32 kChrome = 0x82C9F3; // (130, 201, 243): the HUD's chrome blue
constexpr u32 kLit = 0xFCF344;    // (252, 243, 68): the game's active yellow (uidraw::kSelected)

// The one decision on the glyphs' look (`glyph_look`), asked by both drawing kits: Coloured = the face
// buttons' marks in their family colours (the Xbox and Nintendo letters, the PlayStation shapes; the D-pad's
// pressed arm light), everything else in the tint; Hud2 = HUD 2's style, the whole glyph in the tint (ring, mark, stroke).
// The surfaces keep their own sizes and positions; only this changes. `hud2Surface` = the caller is HUD 2's kit
// (hud2kit::Glyph: the flight HUD 2, the Extended HUD 2 pills), else the classic pad layer (Draw / Text).
// glyph_look 0 automatic = HUD 2 surfaces Hud2 but for the face marks (FaceColours), classic surfaces Coloured; 1 Coloured everywhere; 2 Hud2 everywhere.
enum class Look
{
    Coloured,
    Hud2,
};
Look LookFor(bool hud2Surface); // LookOf with the glyph_look setting
// HUD 2 draws the face marks in the family colours (the owner, 6 Nov 2026): glyph_look 1, or 0 with a family that has any.
bool FaceColours(Family f); // FaceColoursOf with the glyph_look setting
constexpr u32 kPressedArm = 0xF0F4F8; // the coloured look's pressed D-pad arm (and Steam's letters)

// The rules above as pure functions of the setting's value (`glyphLook`), so a program with no game (the design bench)
// decides as the game does (moved here from glyphs.cpp unchanged, bench B2, 12 Nov 2026).
inline Look LookOf(int glyphLook, bool hud2Surface)
{
    if (glyphLook == 1)
        return Look::Coloured;
    if (glyphLook == 2)
        return Look::Hud2;
    return hud2Surface ? Look::Hud2 : Look::Coloured; // 0 automatic: each kit its own look (before glyph_look)
}
inline bool FaceColoursOf(int glyphLook, Family f)
{
    return glyphLook == 1 || (glyphLook == 0 && f != Family::Steam); // (Steam's buttons have no colours of their own)
}
// The family colour of a face button's mark (0xRRGGBB); 0 for the other buttons. One table for both kits.
inline u32 FaceRgb(Family f, Id id)
{
    if (id < A || id > Y)
        return 0;
    // Each family takes the shades of its latest generation with coloured buttons (the owner, 6 Nov 2026): the Xbox Series pad, the DualShock 4 (the DualSense is
    // monochrome), the New Nintendo 3DS (the Switch and Switch 2 are). Sampled from photos, no maker publishes them; a shade under 4.5:1 on the glyph's plate is
    // only brightened: docs\research\pad-face-colours.md.
    static const u32 kXbox[4] = {0x78BE04, 0xD84830, 0x0D7CD5, 0xD3C906};
    static const u32 kPs[4] = {0x8698BF, 0xC37162, 0xCD94B7, 0x57AB94};       // cross, circle, square, triangle
    static const u32 kNintendo[4] = {0xD7A44C, 0xF04050, 0x268E71, 0x4075E2}; // by position: B yellow, A red, Y green, X blue
    return f == Family::Xbox ? kXbox[id] : f == Family::PlayStation ? kPs[id] : f == Family::Nintendo ? kNintendo[id] : kPressedArm;
}
// The colour of a face button's outline (the owner, 11 Nov 2026: the ring shares its button's colour, so the prompts read
// less monochrome): FaceRgb's for a family whose buttons have colours of their own, 0 = the outline keeps the caller's
// colour (Steam, whose buttons have none; every button but A B X Y). The kits use it only where the mark is coloured.
inline u32 OutlineRgb(Family f, Id id) { return f == Family::Steam ? 0 : FaceRgb(f, id); }
// Inside a render pass (render::Begin.. End): the glyph centred on (cx, cy), `h` pixels tall. False when no atlas
// could be built (no device).
bool Draw(Id id, float cx, float cy, float h, float alpha, u32 tintRgb = kChrome);
// The glyph's width at height h (bumpers and triggers are wider than tall).
float Width(Id id, float h);
// Text with inline glyph tokens ("{A} DOCK", "{VIEW}"): draws with the pass's current font from (x, yTop), the glyphs
// at the font's line height; returns the width drawn. `measure` = width only, nothing drawn.
float Text(const render::Font& font, float x, float yTop, const std::wstring& s, u32 argb, bool measure = false);

void Shutdown(); // frees the atlases (before the logic unloads)
std::string Status();
} // namespace glyphs
