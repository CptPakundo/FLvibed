// FLvibed - the Large size's margins and text: a handheld held in the hands crops nothing, so the safe margins that televisions
// need go, and the text is larger. modules\hud\docs\UI_HUD2.md 5.35; who turns it on: `hud_size`, core\game\hudsize.h.
// Pure: g_on is set once a frame by hudsize::Apply and read by the layouts, so the tests set it too.
#pragma once

namespace deckmode
{
inline bool g_on = false;

// text_size at Large, the owner's pick from the frames at 100 / 120 / 135. A size the player chose stays theirs.
constexpr int kTextSize = 120, kTextSizeDefault = 100;
inline int TextSize(bool deck, int chosen) { return deck && chosen == kTextSizeDefault ? kTextSize : chosen; }

// The edge gutter left at Large: no safe area, only enough that a plate's rim does not touch the bezel (fractions of W / H).
constexpr float kGutterW = 0.0125f, kGutterH = 0.015f;

// The flight HUD's margins: the spec's value, or the gutter.
inline float FlightSide(float spec) { return g_on ? kGutterW : spec; }
inline float FlightTopBottom(float spec) { return g_on ? kGutterH : spec; }

// The docked screens' safe area (the prompt lane, the strip, the character card): 0.05 W / 0.05 H, or the gutter.
inline float SideW() { return g_on ? kGutterW : 0.05f; }
inline float EdgeH() { return g_on ? kGutterH : 0.05f; }
// The prompt lane's band: its bottom at the safe edge, 0.04 H tall.
inline float LaneBottomH() { return 1.0f - EdgeH(); }
inline float LaneTopH() { return LaneBottomH() - 0.04f; }
} // namespace deckmode
