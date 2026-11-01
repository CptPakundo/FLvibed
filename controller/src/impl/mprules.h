// What a multiplayer server allows FLvibed's autopilot (docs\MULTIPLAYER.md "Server permission", 28 Oct 2026).
// Single player: everything, always. Multiplayer: the server decides - it says so in a server line of the chat (sender
// = the server, never a player) that is the tag alone (core\mprules_tag.h: FLHook relays players' chat as server lines):
// "[flvibed autopilot=on]" (everything), "[flvibed autopilot=travel]" (flying, docking,
// lanes and gates - no combat: no guns, missiles, mines or targeting fights), "[flvibed autopilot=off]". The last such
// line of the session wins; leaving the server forgets it. A server that says nothing gets the `mp_autopilot` setting
// (default 1 = travel only: the owner's decision, 28 Oct 2026).
#pragma once
#include "common.h"

#include <string>

namespace mprules
{
enum class Allow
{
    Off = 0,
    Travel = 1,
    Full = 2
};

// Per frame: reads new chat lines (cheap: one counter compare), forgets the server's word outside multiplayer.
void Update();
// What the autopilot may do now.
Allow Autopilot();
// Where that came from: "single player", "the server says on|travel|off", "the server is silent: mp_autopilot <n>".
std::string Source();
// The debug channel (`mp`): the state, the last lines read, the source.
std::string Describe();
// A chat line from a server (sender 0 or a console id, never a player's): the tag in it, if any. Exposed for tests.
bool ParseTag(const std::wstring& text, Allow& out);
} // namespace mprules
