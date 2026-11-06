// FLvibed - Steam Deck mode: 1280x800 is almost only the Deck, a 7-inch screen held in the hands, so
// the text is larger and the safe margins go: they are there for televisions that crop the picture's edge, which a handheld's
// panel never does. modules\hud\docs\UI_HUD2.md 5.35.
//
// The signal is the resolution: Steam's own check (ISteamUtils::IsSteamRunningOnSteamDeck) needs the Steam SDK in a game Steam
// launched, a Freelancer install has neither, and no environment variable is documented. `deck_mode` overrides it either way.
// Pure: g_on is set once a frame by the controller and read by the layouts, so the tests set it too.
#pragma once

namespace deckmode
{
inline bool g_on = false;

// deck_mode: 0 automatic, 1 on, 2 off
inline bool Active(int setting, float W, float H)
{
    return setting == 1 || (setting == 0 && W == 1280.0f && H == 800.0f);
}

// text_size at the Deck, the owner's pick from the frames at 100 / 120 / 135. A size the player chose stays theirs.
constexpr int kTextSize = 120, kTextSizeDefault = 100;
inline int TextSize(bool deck, int chosen) { return deck && chosen == kTextSizeDefault ? kTextSize : chosen; }

// The edge gutter left in Deck mode: no safe area, only enough that a plate's rim does not touch the bezel (fractions of W / H).
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
