// FLvibed - the game's own name card under ui_style 0, and the rules where a card may stand beside a room's characters (gamecard.cpp). The card is the
// game's `ToolTip`; only its position is written, each frame before the UI pass, and only on a signed vtable: anything else leaves it where
// the game puts it.
#pragma once
#include "common.h"

#include <vector>

namespace gamecard
{
struct Box // a room character's box on screen (pixels)
{
    float x0, y0, x1, y1;
};
// The room's characters, as the host's list has them.
std::vector<void*> Characters();
// The on-screen boxes of the room's characters other than `self` (pixels; off-screen or empty ones left out).
std::vector<Box> OtherBoxes(const std::vector<void*>& chars, void* self, float W, float H);
// FLvibed's own modals over the base (the options page, the on-screen keyboard); and any modal menu, the game's included.
bool OwnModalOpen();
bool ModalOpen();
// The place for a card of w x h pixels by a character's box, clear of the keep-outs and of a strip reaching stripRight (pixels) x stripBottom
// (fractions of H; 0 = none). `placed` names the rule that won. False: no rule fits.
bool PlaceBy(float bx0, float by0, float bx1, float by1, float stripRight, float stripBottom, float w, float h, float W, float H, float& x,
             float& y, const char*& placed, const std::vector<Box>* others = nullptr);
// The name card's top-left through the `ToolTip`'s frame (pixels).
bool TipCorner(u32 tip, float& x, float& y);
// The shown card's top-left, on screen only.
bool GameCardCorner(float& x, float& y);
// Once per frame before the UI pass, under ui_style 0. `scene`: the letterbox is up, the card stays the game's.
void NativeCard(bool scene);

struct Native
{
    u32 tip = 0;      // the ToolTip control (cached: found at most 10 Hz)
    void* who = nullptr; // the hovered room character
    double nextFind = 0;
    int frames = 0, writes = 0, refused = 0;
    float x = 0, y = 0, w = 0, h = 0;  // last placement (pixels: the card's top-left and size as placed)
    float box[4] = {};                 // the character's box then (x0, y0, x1, y1)
    const char* placed = "-";
    float lastAt[2] = {}; // where the game last put the card (its passed position, pixels; state report)
    int hovered = 0;      // characters claiming the hover at the last find
};
const Native& NativeState(); // the debug report
} // namespace gamecard
