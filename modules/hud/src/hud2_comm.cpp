// The comm window under HUD 2: hud2_comm.h.
#include "hud2_comm.h"

#include "game/commwin.h"
#include "ui/hud2_kit.h"
#include "ui/winplates.h"

#include <algorithm>

namespace hud2comm
{
using namespace hud2kit;

constexpr float kMaxSize = 0.11f, kGap = 0.012f; // the subtitles' own fallback, display.cpp: the same place with them or without
static commwin::Slot g_slot;
static commwin::Placed g_placed;
static bool g_held = false;

void Frame(bool up, float x, float below, float H)
{
    const u32 recv = commwin::Receiver();
    bool want = up && recv && commwin::IsOpen(recv) && !commwin::HeldBy(commwin::Who::Subtitles); // (the panel's slot wins, by what it holds now, not by its settings)
    if (want)
    {
        g_slot.s = std::min(commwin::NativePx(), H * kMaxSize);
        g_slot.x = std::max(1.0f, x), g_slot.y = below + H * kGap;
        want = g_slot.s > 1 && g_slot.y + g_slot.s < H * 0.6f && commwin::Put(commwin::Who::Hud, g_slot, g_placed);
    }
    if (!want && g_held)
        commwin::Let(commwin::Who::Hud);
    g_held = want;
}

void Draw(float H, bool named)
{
    std::wstring name;
    if (!g_held || named || !g_placed.caption || !winplates::TextOf(g_placed.caption, name) || name.empty())
        return;
    // the game's caption holds its two lines in one string, the name over the faction: a character no font draws stands between them (the owner's frame, 7 Nov 2026: "Yates?Liberty Police")
    size_t cut = 0;
    while (cut < name.size() && name[cut] >= 0x20 && (name[cut] < 0x7F || name[cut] > 0x9F) && name[cut] != 0x2028)
        ++cut;
    const float size = TextFloor(), top = g_slot.y + g_slot.s + H * 0.008f + size / 2, room = H * 0.42f;
    TextFit(g_slot.x, top, name.substr(0, cut), size, room, Argb(kInk, 1));
    if (cut + 1 < name.size())
        TextFit(g_slot.x, top + size * 1.15f, name.substr(cut + 1), size, room, Argb(kChrome, 1));
}
} // namespace hud2comm
