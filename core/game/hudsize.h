// FLvibed - `hud_size`, the one size choice: AUTOMATIC, SMALL, NORMAL, LARGE (modules\hud\docs\UI_HUD2_INPUTS.md 7).
// Pure: the frame resolves it once from the setting, the screen and the machine (core\game\handheld.h) and writes three
// globals every layout reads: the scale's base (hudsize::g_baseK), the text floor (textfit::g_floorH) and Large's margins
// and text (deckmode::g_on). Small keeps Normal's text at the handheld's height, where its smaller text would go under the
// floor a handheld needs. hud_scale and text_size stay trims on top of the preset: 100 means the preset's own.
#pragma once
#include "game/deckmode.h"
#include "game/textfit.h"

namespace hudsize
{
enum Preset : int
{
    Automatic = 0,
    Small = 1,
    Normal = 2,
    Large = 3
};

constexpr float kSmallK = 0.875f;     // the classic HUD's text size, by the ratio of their heights in real frames
constexpr float kSmallFloorH = 0.021f, kNormalFloorH = 0.024f;
constexpr float kHandheldH = 800.0f;  // at or under this height Small keeps Normal's text

inline float g_baseK = 1.0f;          // the scale's base: what hud_scale 100 x text_size 100 mean (promptfit::Hud2Scale)
inline int g_preset = Normal;         // the preset in force (never Automatic)

// The preset the setting asks for. `detected` = the machine's answer for Automatic (Normal or Large).
inline int Resolve(int setting, int detected)
{
    if (setting >= Small && setting <= Large)
        return setting;
    return detected == Large ? Large : Normal;
}

// `deck_mode`'s saved value as a `hud_size`: 1 on = Large, 2 off = Normal, 0 automatic = Automatic.
inline int FromDeckMode(int deckMode) { return deckMode == 1 ? Large : deckMode == 2 ? Normal : Automatic; }

// The scale's base and the text floor of a preset on a screen this tall.
inline float BaseK(int preset, float H) { return preset == Small && H > kHandheldH ? kSmallK : 1.0f; }
inline float FloorH(int preset, float H) { return preset == Small && H > kHandheldH ? kSmallFloorH : kNormalFloorH; }

// The game's own text under `game_text_size` Automatic (0): Large raises it, the others leave it at the game's size.
inline int GameTextPercent(int setting, int preset) { return setting != 0 ? setting : preset == Large ? 125 : 100; }

// The frame's one write. Returns the text_size to draw with: Large's own unless the player chose a size.
inline int Apply(int preset, float H, int chosenTextSize)
{
    g_preset = preset;
    g_baseK = BaseK(preset, H);
    textfit::g_floorH = FloorH(preset, H);
    deckmode::g_on = preset == Large;
    return deckmode::TextSize(deckmode::g_on, chosenTextSize);
}
} // namespace hudsize
