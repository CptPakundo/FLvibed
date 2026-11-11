// FLvibed - where the player is, in the words the modules share: the menus, a base, flight, flight with a window, a cutscene.
#pragma once

namespace gamemode
{
enum class Mode
{
    Menu,     // main menu, loading, multiplayer browser...
    Docked,   // on a base or planet
    Space,    // flying
    SpaceUi,  // in space with a NeuroNet window (nav map, inventory...) using the cursor
    Cutscene, // in-space cinematic or docking camera
};

const char* Name(Mode m);

// A NeuroNet window is open in space (the nav map, the inventory, the log, the status, help) by its root control's visibility
// (cached 100 ms). These windows hang under the HUD's own NeuroNet root, so "a window over the HUD roots" does not see them.
bool SpaceWindowOpen();

// This frame's mode. The two questions are the caller's until the windows are core's (P4), asked only in flight and in this
// order: only the flight view is open (the flight HUD and the boxes over the flight, such as the cargo demand: core\game\coreframe.cpp FlightViewOnly);
// a NeuroNet window is open.
Mode Detect(bool (*flightViewOnly)(), bool (*spaceWindowOpen)());
} // namespace gamemode
