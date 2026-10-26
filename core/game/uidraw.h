// FLvibed - the look of the game's panels and controls, drawn through core\game\render.* (inside a Begin / End pass):
// the space HUD's panel (moved from subtitles\src\impl\display.cpp HudPanel on 25 Oct 2026, the UI build), the
// subtitles' soft navy panel, and the Options screen's row controls as replicas - a checkbox, a slider track with its
// thumb and end arrows, a picker box with arrows, a focus frame, a dot, text with a shadow. Every size is given in pixels
// by the caller (fractions of the screen height through the UI camera; nothing here assumes a resolution); the colours
// are measured on the game's Options screen at 4K (research\experience\ui_2026-10-25\options_*.png, 25 Oct 2026).
#pragma once
#include "common.h"
#include "textfit.h"

#include <string>

namespace render
{
struct Font;
}

namespace uidraw
{
// The Options screen's palette (0xAARRGGBB).
constexpr u32 kText = 0xFF8ACEF1;     // labels, captions, ENABLED / DISABLED, the arrows (138, 206, 241)
constexpr u32 kFrame = 0xFF77AED2;    // button and picker frames (119, 174, 210)
constexpr u32 kTrack = 0xFF689DB5;    // the slider's track line (104, 157, 181)
constexpr u32 kThumb = 0xFF7EB5D2;    // the slider's thumb (126, 181, 210)
constexpr u32 kSelected = 0xFFFCF344; // the selected page / tab caption (252, 243, 68)
constexpr u32 kDisabled = 0xFF76767B; // a disabled button's frame and caption (118, 118, 123)
constexpr u32 kCheck = 0xFF1E9D23;    // the checkbox's tick (30, 157, 35)
constexpr u32 kBoxFrame = 0xFF2F729A; // the checkbox's frame (47, 114, 154)
constexpr u32 kBoxFill = 0xFF000000;  // the checkbox's inside
constexpr u32 kHelpBack = 0xD0484848; // the rollover help line's backing (72, 72, 72)
constexpr u32 kPanel = 0xF0131039;    // the Options panel's deep navy (19, 16, 57)
constexpr u32 kShadow = 0xC0000000;

// The space HUD's panel look (background_style 1): the translucent violet-navy base, the inner edge glow, the brighter
// top line and the scanline sweeping down every 4 s (measured on the HUD's empty panel: display.cpp's comment of 21 Oct
// 2026). `sh` = the screen height (the glow and line widths scale with it), `now` = seconds (the scanline's phase).
void HudPanel(float x0, float y0, float w, float h, float alpha, float sh, double now);
// The subtitles' soft navy panel: feathered over `feather` pixels instead of hard edges.
void SoftPanel(float x0, float y0, float w, float h, float alpha, float feather);
// A rectangle outline `t` pixels thick (inside the rectangle).
void Frame(float x, float y, float w, float h, float t, u32 argb);
// The focus frame of the page's rows: an outline with a soft inner glow, in the frame colour.
void FocusFrame(float x, float y, float w, float h, float sh, u32 argb = kFrame);
// A filled disc of radius r (the "changed from default" dot before a label), built from boxes.
void Dot(float cx, float cy, float r, u32 argb);
// A triangle pointing dir 0 up, 1 down, 2 left, 3 right, `size` pixels across its base, built from boxes.
void Triangle(float cx, float cy, float size, int dir, u32 argb);
// The game's slider: the track line from x to x + w at height y (its centre), the thumb at t (0..1), an arrow at each
// end (the arrows sit outside the track by `arrow` pixels). Greyed when not enabled.
void SliderTrack(float x, float y, float w, float sh, float t, bool enabled);
// The game's checkbox: a `size` square at (x, y), the tick when on.
void Checkbox(float x, float y, float size, bool on, bool enabled);
// The game's picker box (the resolution picker): the framed box with the up / down arrows in a cell at its right end;
// returns the width of the arrow cell (the caller centres its word in the rest).
float PickerBox(float x, float y, float w, float h, float sh, bool enabled);
// Text with a shadow `off` pixels down-right (the current font).
void TextShadow(float x, float y, const std::wstring& s, u32 argb, float off);
// THE text-fit rule (core\game\textfit.h) over a render::Font drawn at its own size: unchanged when it fits maxW, else
// the size steps down - only as far as needed - to floorPx (0.024 H at the player's text_size), only then the text is
// cut with an ellipsis. Cached per font / string / box. The result's size is in pixels (the font's px = drawn 1:1).
textfit::Result FitText(const render::Font& f, const std::wstring& s, float maxW, float floorPx);
// Draws a fit result with TextShadow's shadow in the current font `f` (set it first) at (x, y) = where the unscaled
// text's top-left would be: a smaller size is drawn scaled (render::TextScaled), centred on the same line.
void TextShadowFit(float x, float y, const render::Font& f, const textfit::Result& r, u32 argb, float off);
// A colour with its alpha scaled by a (0..1).
u32 Fade(u32 argb, float a);
} // namespace uidraw
