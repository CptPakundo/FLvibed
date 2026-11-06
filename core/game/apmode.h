// FLvibed - the autopilot's modes: the HUD names them, so they are core's, and the Autopilot module may be off.
#pragma once

namespace apmode
{
enum class Mode
{
    Off,
    Goto,      // fly to the target (or a point) with the game's goto
    Dock,      // dock / land / lane / jump with the target
    Follow,    // stay with a ship at a distance (pub::AI follow)
    Formation, // join the target's formation
    Face,      // turn to face the target and hold
    Escort,    // follow a friendly ship, fight what threatens it or the player, then rejoin
    Combat,    // fight the target, then (engage rules) the next hostile
    Travel, // the fastest way to a destination (or along the waypoint list): lanes, gates, fast dock (core\game\route)
    Hold,   // fly to a point (default: where the ship is) and keep station there until stopped
    Shoot,  // shoot named points of a target (a depot's cargo pods) from firing range, tractor what they drop
};
} // namespace apmode
