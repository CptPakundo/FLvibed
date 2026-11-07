// The game's comm window (the speaker's live head or its generic icon, top left) placed by its live fields: docs\FREELANCER_RE.md "The comm window live". The subtitles' panel holds it in
// a slot (modules\subtitles\src\impl\display.cpp), and HUD 2 keeps it under its nav whenever the panel does not hold it (modules\hud\src\hud2_comm.*): no subtitles, no line, no slot.
// Whoever holds it this frame says so by Put; the natives are read once, here, so a hand-over between the two never reads the other's place for the game's.
#pragma once
#include "common.h"

namespace commwin
{
// The window's place as the game (or HudShift) set it, read while nothing has moved it.
struct Natives
{
    float pos[3] = {}, size[2] = {}, iconPos[3] = {}; // window centre and size in UI camera space, the icon control's position
};
// A screen square, px; sx, sy: the window's width and height in it (the opening animation: < 1, centred); fs: its outline's size in it.
struct Slot
{
    float x = 0, y = 0, s = 0, sx = 1, sy = 1, fs = 1;
};
// What Place did, for GiveBack and the slot's users.
struct Placed
{
    u32 icon = 0, caption = 0; // the controls moved / hidden (0 = none)
    float w = 0, h = 0;        // the window's size on the screen (smaller while it opens)
    float f = 0;               // its outline's size (px, square, centred)
};

enum class Who : int
{
    None,
    Subtitles,
    Hud
};

u32 Receiver();         // the HUD's comm receiver, 0 outside flight
bool IsOpen(u32 recv);  // its window is open
float NativePx();       // the window's size on the screen at its own place, px; 0 unknown
// The window into the slot, the caption hidden (the slot's holder names the speaker): each frame it holds it. False: the natives or the camera do not read, nothing moved.
bool Put(Who who, const Slot& s, Placed& out);
// `who` lets go: the game's place back, if `who` holds it.
void Let(Who who);
// `who` held it within the last frames.
bool HeldBy(Who who);
// A control's position by its own setter (a written +0xA8 would not move an icon button's mesh).
bool SetPosition(u32 ctl, const float* v);
} // namespace commwin
