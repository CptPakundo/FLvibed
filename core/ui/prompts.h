// FLvibed - one prompt of the bottom-right prompt bar, as core's UI hands it to the bar (Controller: promptbar.*).
#pragma once
#include "ui/glyphs.h"

namespace prompts
{
struct Prompt
{
    glyphs::Id btn;             // the glyph
    const wchar_t* label;       // "SELECT" (upper case, as the spec writes them); copied by Set (a temporary is fine)
    bool hold = false;          // a hold action ("HOLD" after the label)
    glyphs::Id btn2 = glyphs::COUNT; // a second glyph for the same label ("{LB} {RB} SHIFT"); COUNT = none
    float progress = 0;         // a hold in progress: 0..1 draws the sweep under the pair (the owner's hold timer)
    bool enabled = true;        // false: the label greyed (an action not available now; the goal panel's QUEUE)
};
} // namespace prompts
