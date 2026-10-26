// FLvibed - the game's flat icon sheets as one texture atlas for FLvibed's own HUD (HUD 2, docs\UI_HUD2.md 1.5):
// HDE's DATA\INTERFACE\buttontextures_flaticons.txm (falls back to buttontextures.txm, the file buttontextures.ini
// names), its texture library entries Montage64 (the maneuver bar's icons, 4 x 4 cells of 256 px), Montage32 (contacts,
// panels, equipment, 8 x 8 cells of 128 px) and HUD_targetingelement (the brackets / arrow / lead art), each a square
// MIP0 inside the UTF file (1024 px in HDE 0.7.1, 256 px in vanilla 1.1: the same grid, the cells scale with the sheet) -
// uncompressed TGA (both), DDS (DXT1 / DXT3 / DXT5) decoded too. Only the cells
// FLvibed draws are copied into the atlas (white on alpha, tinted at draw time), at two sizes (128 px cells for large
// draws, 48 px for small ones: no mip chain needed). The file is read on a worker thread the first time an icon is
// wanted (18 MB: never on the game's frame); the texture is built on the game thread once the pixels are ready. Fails
// closed: no file, no entry, an unknown format -> Ready() stays false and the caller draws its own shapes.
// The cells by meaning are the mock-up's (tools\uimock\hud2.py ICON). Compiled into each module part that uses it.
#pragma once
#include "common.h"

#include <string>

namespace flicons
{
enum Icon : int
{
    Cruise = 0, // Montage64
    Map,
    Free,
    Goto,
    Dock,
    Formation,
    Comms,
    Player,
    Fighter, // Montage32
    Enemy,
    Station,
    JumpGate,
    Lane,
    Waypoint,
    Planet,
    Transport,
    Tractor,
    Important,
    Info,
    Next,
    Star,
    Mine,
    Group,
    Loot,
    List,
    Bracket, // HUD_targetingelement: the bottom-left corner piece
    Arrow,   // the off-screen arrow (points 224 degrees counter-clockwise in its cell)
    Lead,    // the four-triangle lead marker
    Count
};

// Starts the load on first use; true once the atlas texture exists on `device` (call from the game thread with
// render::SetDevice(device) done: the texture is built through core\game\render).
bool Ready(void* device);
// The atlas texture (IDirect3DTexture8*) and an icon's UV rectangle (u0, v0, u1, v1) for a draw `sizePx` tall. False
// when not ready.
bool Uv(Icon icon, float sizePx, void** texture, float uv[4]);
// Frees the textures and waits for the worker (before the logic unloads).
void Shutdown();
std::string Status();
} // namespace flicons
