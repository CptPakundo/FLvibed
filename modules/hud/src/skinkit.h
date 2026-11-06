// HUD 2's skin primitives (modules\hud\docs\UI_HUD2_SKIN.md 4): plate, header, section label, body, row, chip, tab, focus, scroll bar, scrim, dialog,
// tooltip and the grammar's pips and HOLD ring, drawn through core\ui\hud2_kit; the sizes and rules are skin_rules.h. A screen the skin has not met yet is
// built from these before anything of its own. Only what two modules draw would go into hud2_kit; nothing here does yet. Coordinates are pixels, between
// hud2kit::Begin and End, after SetScreen.
#pragma once
#include "common.h"
#include "skin_rules.h"
#include "ui/hud2_kit.h"

#include <string>

namespace skinkit
{
// The screen this pass draws on: every size below is a fraction of its height.
void SetScreen(float W, float H);
float Px(float fractionOfH);

// A plate: glass (85 % over rooms, 92 % for a dialog), rim CHROME, chamfers where `corners` says.
void Plate(float x0, float y0, float x1, float y1, int corners, float fill = skinrules::kPlateFill, float rimAlpha = 0.69f);
// Its rim alone; `chamfer` in pixels.
void Rim(float x0, float y0, float x1, float y1, float chamfer, int corners, float alpha);
// A plate's edge in a colour: the grammar's accent, on the side skinrules::AccentEdge says.
void AccentEdge(float x0, float y0, float x1, float y1, skinrules::Edge edge, u32 rgb);
// A hairline across a plate at y, kept off its ends.
void Rule(float x0, float x1, float y, float alpha = 130 / 255.0f);
// Title 0.040 H INK, an optional small tag beside it, the rule under them. Returns the y under the rule.
float Header(float x0, float x1, float y, const std::wstring& title, const std::wstring& tag = std::wstring());
// Small caps CHROME.
void Section(float x, float y, const std::wstring& text);
// Body text, wrapped to maxLines by whole words. Returns the y under it.
float Body(float x, float y, float w, const std::wstring& text, int maxLines, u32 rgb = hud2kit::kInk, hud2kit::Align align = hud2kit::Left);
// One line of a list: the name left, the value right; `h` the row's height, at least 0.046 H. A name too long takes the marquee, never a smaller size.
void Row(float x0, float x1, float y, float h, const std::wstring& name, const std::wstring& value);
// A button: the word in it, centred. Focused: the one focus style, the word in INK (R9). Disabled: DIM.
struct ChipState
{
    bool focused = false, disabled = false;
};
void Chip(float x0, float y0, float x1, float y1, const std::wstring& word, ChipState state = {});
// A chip's glass alone, for a native caption that stays on it.
void ChipGlass(float x0, float y0, float x1, float y1);
// A tab: a chip, the current one yellow with an underline.
void Tab(float x0, float y0, float x1, float y1, const std::wstring& word, bool current);
// The one focus style: the active fill, its edges and the bar outside the left edge.
void Focus(float x0, float y0, float x1, float y1, float fill = skinrules::kFocusFill);
// A scroll bar: the track, the thumb from the native range. `first` and `span` are shares of the whole, 0..1.
void ScrollBar(float x, float y0, float y1, float first, float span);
// The modal scrim over the whole screen, at `peak` x `level` (level: skinrules::EaseStep's).
void Scrim(float peak, float level);
// A dialog's plate from skinrules::DialogFit, its top edge amber where progress can be lost, title and cost centred.
void Dialog(const skinrules::DialogBox& b, const std::wstring& title, const std::wstring& cost, bool amber);
// A tooltip: plate, body at 0.026 H, at (x, y) kept inside the safe area. False when nothing could be placed.
bool Tooltip(float x, float y, const std::wstring& text);
// The grammar: a quantity as segments (never a bare bar).
void Pips(float x, float y, float w, float h, float share, int n, u32 rgb);
// The grammar: a hold filling round a glyph.
void HoldRing(float cx, float cy, float r, float share, u32 rgb);
} // namespace skinkit
