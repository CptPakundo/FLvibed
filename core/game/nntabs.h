// FLvibed - the NeuroNet's tabs, pure: their order, their words, the step between them and the press held through a slide
// (modules\hud\docs\UI_HUD2_SKIN.md 6.1 "Every NeuroNet window stays one step away"). The HUD's strip draws them and the pad steps them;
// both ask through core\game\nnwin.*, which is this over the game's window manager.
#pragma once

namespace nntabs
{
// The game's commands, the Roots' own (0x45B8F0): one path for a click and a key.
constexpr int kCmdMap = 0x63, kCmdStatus = 0x65, kCmdInventory = 0x66, kCmdLog = 0x67, kCmdChat = 0x68, kCmdGroup = 0x69, kCmdInfo = 0x6A;

struct Tab
{
    int cmd;
    const wchar_t* word;
};
// With the map in the middle, INVENTORY and STATUS are one press from it: the order is the player's question answered, not the box's.
constexpr Tab kTabs[] = {{kCmdLog, L"LOG"},   {kCmdInventory, L"INVENTORY"}, {kCmdMap, L"MAP"},    {kCmdStatus, L"STATUS"},
                         {kCmdInfo, L"INFO"}, {kCmdChat, L"CHAT"},           {kCmdGroup, L"GROUP"}};
constexpr int kSinglePlayerTabs = 5, kAllTabs = 7;

// CHAT and GROUP exist in multiplayer only, as the game's box builds them.
inline int Count(bool multiplayer) { return multiplayer ? kAllTabs : kSinglePlayerTabs; }

// The tab of a command, -1 for none of the shown ones.
inline int IndexOf(int cmd, bool multiplayer)
{
    const int n = Count(multiplayer);
    for (int i = 0; i < n; ++i)
        if (kTabs[i].cmd == cmd)
            return i;
    return -1;
}

// The neighbour of `index`, wrapping.
inline int Step(int index, int dir, int count)
{
    if (count <= 0)
        return -1;
    const int i = (index + (dir < 0 ? -1 : 1)) % count;
    return i < 0 ? i + count : i;
}

// The game loses a press during a slide (0x45CCF0 returns at once), so a press then is held and sent the first frame after. The last
// press wins, so two bumper presses land on the second tab, one slide later. A hold that outlasts kHoldSeconds is dropped: never late.
constexpr float kHoldSeconds = 2.0f;

struct Press
{
    int cmd = 0;       // the command held, 0 none
    bool close = false; // the held press closes the window: it is sent even though it is the open one
    float left = 0;

    bool Held() const { return cmd != 0; }
    void Clear() { cmd = 0, close = false, left = 0; }
    // Where a step starts from: the press already held, else the open window.
    int Target(int open) const { return cmd ? cmd : open; }

    // Go to `want`. Returns the command to send now, 0 for none. The open window's own command would close it, so it only cancels a hold.
    int Go(int want, int open, bool busy)
    {
        if (busy)
        {
            cmd = want, close = false, left = kHoldSeconds;
            return 0;
        }
        Clear();
        return want == open ? 0 : want;
    }
    // Close the open window: the game's same command again.
    int Close(int open, bool busy)
    {
        if (!open && !busy)
            return 0;
        if (busy)
        {
            cmd = open ? open : cmd, close = true, left = kHoldSeconds;
            return 0;
        }
        Clear();
        return open;
    }
    // Once a frame. Returns the command to send now, 0 for none.
    int Tick(float dt, int open, bool busy)
    {
        if (!cmd)
            return 0;
        if (busy)
        {
            left -= dt;
            if (left <= 0)
                Clear();
            return 0;
        }
        const int send = cmd;
        const bool closing = close;
        Clear();
        if (closing)
            return send == open ? send : 0; // (the window it meant has gone already)
        return send == open ? 0 : send;
    }
};
} // namespace nntabs
