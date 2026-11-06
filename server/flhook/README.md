# flvibed_server.dll - the FLHook plugin for servers

For **server hosts** running HD Edition's FLServer with its **FLHook** (FLHDE/FLHook, plugin API 4.0). Players do not
need it. It tells every player who runs FLvibed what this server allows FLvibed's Advanced Autopilot
(`docs\MULTIPLAYER.md` "Server permission").

## Install
1. Build it (`build.cmd flhook`; the release does not ship it) and copy `flvibed_server.dll` into the server's `EXE\plugins\` (beside FLHook's other plugins).
2. Start FLServer. On first load the plugin writes `EXE\config\flvibed.json`:
   ```json
   { "autopilot": "travel" }
   ```
   - `on` - everything, combat included.
   - `travel` - flying, docking, trade lanes, gates, holding, evading, repairs and countermeasures; no guns, missiles or
     torpedoes, no combat start (the default, as for a server without the plugin).
   - `off` - no autopilot.
3. A change while the server runs: edit the file, then FLHook's admin command `reloadplugin flvibed` sends the new rule
   to everyone online (not tested yet: the test server has no admin console in use).

## What it does
When a player logs in with a character, the plugin sends them one server chat line holding only
`[flvibed autopilot=...]`. FLvibed reads it and keeps it out of the chat window; a player without FLvibed sees that one
line. Only server lines count (a player typing the tag changes nothing). It is a courtesy rule, not anti-cheat: a server
that must forbid automation needs its own checks.

## Build (developers)
`build.cmd flhook` -> `build\flhook\flvibed_server.dll`. It links against HD Edition's `FLHook.dll` exports only (an
import library made from `FLHook.def`), with FLHook's plugin types declared in the source to match FLHook's headers at
FLHDE/FLHook 8829534 (the state HD Edition 0.7.1 ships). Another FLHook line (Discovery's fork, TheStarport's 3.x) needs
its own build.

## Verified (28 Oct 2026)
HD Edition 0.7.1's FLServer + FLHook on the local test server (`tools\mptest`): the plugin loaded, the client received
`[flvibed autopilot=travel]` at login from the server sender (65536) and switched to "the server says travel"; a combat
start was refused, docking and a lane trip ran.
