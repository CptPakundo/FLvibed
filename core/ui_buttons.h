// FLvibed's native buttons (core\host\uibtn.*): their ids and what HUD 2 asks of its hotspots. Created by the host through the
// game's own icon-button factory (0x5A0C90), so the game's hit test, hover, click sound and click-vs-fire arbitration apply
// (UI_PLAN.md 2, 3.1): the host creates, places and shows them, routes their clicks to the logic and lets the logic set their state.
#pragma once

enum UiButton : int
{
    kUiBtnBar = 1,         // the fifth maneuver-bar button (the Advanced Autopilot), parented to HUD_Maneuvers
    kUiBtnOptionsPage = 2, // the FLVIBED page button in the game's Options screen (NN_Preferences' free slot)
    kUiBtnPauseFlvibed = 3, // the sixth button of the F1 pause menu (NN_Pause), FLVIBED
    // HUD 2's hotspots, kUiBtnHud2 + slot (slot 0 .. kUiBtnHud2Count - 1, the logic's numbering: SetHudHotspots)
    kUiBtnHud2 = 100,
    kUiBtnHud2Count = 32
};

// One HUD 2 hotspot as the logic wants it this frame: a rectangle in back-buffer pixels (x0 < x1, y0 < y1) and whether it is shown
// (an unshown one is hidden and not hit-testable).
struct HudHotspot
{
    float x0, y0, x1, y1;
    int shown;
};
