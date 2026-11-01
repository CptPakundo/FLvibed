# The local multiplayer test setup (TEST-ONLY, never shipped)

A local FLServer.exe and a test client on this machine, both from one **test instance** (`Install\instances\<n>`,
never the main install, never the owner's `v11nightly` or another worker's copy). Built 28 Oct 2026 for the
multiplayer work (`docs\MULTIPLAYER.md`).

## What is not the game's default here - keep track of it (the owner, 28 Oct 2026)

The owner's decisions: nothing of a multiplayer test may land outside the project, and a test must always know which
account key it uses and that the server's data path is not the game's default.

| What | The game's default | In a test instance | Who |
|---|---|---|---|
| The MP account key (`MPAccountName`, `MPAccountNameSig`) and the install key (`InstallKey`) | `HKCU\Software\Microsoft\Microsoft Games\Freelancer\1.0` - the PC's MP identity, shared with every personal install | `<instance>\SAVE\mptest\accounts\<set>\*.bin`; the registry is never read or written for these values | `flmptest.dll` (Common.dll's RegQueryValueExA / RegSetValueExA imports) |

**One slip (28 Oct 2026):** the first sessions redirected only the two account values; Common.dll also writes
`InstallKey`, which reached the registry then (`6ef01b6b-bff9af99-11f6a915-35a295d0-a4ac`; the key held only
`FIRSTRUN` and `default` before). Since then it is redirected too; the registry value was removed on the owner's word the same
day (the key holds `FIRSTRUN` and `default` again).
| FLServer's data (`FLServer.cfg`, `Accts\MultiPlayer\` accounts and characters, FLHook's sqlite files) | `Documents\My Games\Freelancer` | `<instance>\SAVE\mptest\server` | `flmptest.dll` (Common.dll `GetUserDataPath` + its `SHGetFolderPathA`), `[DACOM] SavePath` in the instance's `dacom.ini` (HDE's FLHook resolves the folder itself) |
| FLServer's window | shown | hidden, windowless console (FLHook's) | `flmptest.dll` with `FLVIBED_BACKGROUND` |

**The ledger**: `<instance>\SAVE\mptest\mptest.log` - every load of the helper, every account-key read / write (which
set, how many bytes, "NOT the registry"), the server's redirected paths and hidden windows. Read it after a run;
`Get-FLMpAccounts` lists the sets and the last use.

**Account sets** (`FLVIBED_MPACCOUNT`, default `default`): a set is one MP identity. **Re-use** a set to come back to
the same characters on the test server (the server keys its accounts by it); **make a new set** (a new name) for a
fresh player - a second client in parallel, a clean account test, a "new player" path. Never copy a set from or to the
registry. A missing set makes the game create a new key on its first need (the account screen after LAN shows it).

## Files

- `flmptest.cpp` -> `build\mptest\flmptest.dll` (`build.cmd mptest`): the redirections above. The client loads it from
  the instance's `dacom.ini` (first `[Libraries]` line); the server never through `dacomsrv.ini` - FLServer resolves
  its data folder in its own start-up, before DACOM's libraries (an empty `Documents\My Games\Freelancer` appeared that
  way on 28 Oct 2026; removed, it was new and empty).
- `flmplaunch.cpp` -> `build\mptest\flmplaunch.exe`: starts FLServer suspended, queues `LoadLibraryA(flmptest.dll)` on
  its main thread (runs once the loader mapped the static imports, before the program's entry point), resumes it hidden.
- `mptest.ps1` (dot-source after `controller\tools\flctl.ps1`, PowerShell tool only; `$env:FLVIBED_INSTANCE` set):
  `Initialize-FLMpInstance`, `Start-FLMpServer [-Account <set>]` (fills the first-run "Server Configuration" dialog
  through its controls: name, LAN only, new players allowed), `Stop-FLMpServer`, `Get-FLMpServerWindows`,
  `Connect-FLMpServer [-Character <name>]` (the client's way in, natively: MULTIPLAYER -> LAN -> account screen ->
  server list row (`table ServerTable <row>`) -> CONNECT -> character list -> NEW / LOAD -> docked), `Get-FLMpAccounts`.

## A session

```powershell
$env:FLVIBED_INSTANCE = '21'
. .\controller\tools\flctl.ps1; . .\tools\mptest\mptest.ps1
Start-FLMpServer
.\tools\deploy.ps1 -Instance 21 -Background   # the client, muted and in the background as every test
Connect-FLMpServer
# ... the test ...
Stop-FLMpServer   # and close the client (Quit-FLGame or the harness' close)
```

The server is LAN only (never "Make Your Server Internet Accessible"), and the client always picks LAN at the
"LAN / INTERNET" prompt: nothing contacts the global server list.

## A vanilla 1.1 client (28 Oct 2026)

Instance `vanilla_mp` (`Install\instances\vanilla_mp\Freelancer`, the original 1.1 files; its `Freelancer HD Edition` is a
junction so the tools find it): `$env:FLVIBED_INSTANCE = 'vanilla_mp'`, `Start-FLMpServer` (vanilla's own FLServer, no
FLHook: the server is silent, so the autopilot gets `mp_autopilot`), `deploy.ps1 -Release -NoBuild -Background` (no
`-Instance`: the name comes from the environment), `Connect-FLMpServer`. The vanilla 1.1 client lists a LAN server by its
IPv6 link-local address only and its connect fails; **flmptest.dll** (client processes only) patches the list to IPv4
hosts in memory - HD Edition's own exe patch at Freelancer.exe 0x5ACF6A (13 bytes; FLUF's "Prevent ipv6 address from
appearing"), applied only where vanilla's bytes are found, logged in the ledger ("LAN list IPv4 only ... test-only").
`deploy.ps1 -GameArgs '-s127.0.0.1:2302'` passes the game's own direct-connect switch (Discovery's launcher uses it), but
on its own it changed nothing here: the LAN list still offered the IPv6 entry. Never set both servers up at once: they
share UDP 2302.

## Several clients (28 Oct 2026)

The server on one instance, each client on its own (`Install\instances\<n>`; `New-FLInstance` makes a missing one), each
with its own account set (default `client<n>`: one MP identity per instance - two clients on one account are refused
"already in use"). Every harness call goes to `$env:FLVIBED_INSTANCE`'s game; `Invoke-FLMpAt <n> { ... }` runs a block
against instance n.

```powershell
. .\controller\tools\flctl.ps1; . .\tools\mptest\mptest.ps1
Invoke-FLMpAt 21 { Reset-FLMpServerConfig }      # only to change the server's options: its next start asks again
Start-FLMpSession -Server 21 -Clients 21,22 -PvP # server (+ "Allow Players to Harm Other Players"), clients, connected
& .\tools\mptest\pvp.ps1 -Clients 21,22 -Rounds 4 # autopilot-led PvP rounds (below)
Stop-FLMpSession -Server 21 -Clients 21,22       # each client leaves as a player does, then the server ends
```

- `Start-FLMpClient <n> [-Account set]`: flmptest.dll listed in that instance, `deploy.ps1 -Instance n -Background
  -NoBuild` with the set; waits for the main menu. `Connect-FLMpServer -Character <name>` then joins (a character
  that left in flight comes back in space: FLHook keeps it there).
- `Exit-FLMpClient`: QUIT TO MAIN MENU (the game menu's PauseButtonBox4, confirm `Confirm_delBtnYes`) -> EXIT
  (`ExitButtonBox0`): the server frees the account at once; a dead server's error box (`ErrorOkayButton`) -> the server
  list's MAIN MENU. Killing a client instead keeps its account "in use" until the server's timeout.
- PvP: the server's "Allow Players to Harm Other Players" is a box of its first-run configuration dialog (the flag lives
  in the binary `FLServer.cfg`: `Reset-FLMpServerConfig` moves the file aside, `Start-FLMpSession -PvP` ticks it);
  FLHook's `noPVPSystems` must not list the system; the plugin's `EXE\config\flvibed.json` "on" lets the autopilot fight.
- `pvp.ps1`: each round every client runs `ap start combat <the next client's ship id>`; the watchdog samples each
  client every ~1.5 s (`samples.tsv`: tactic, distance, kills, dealt, lost, shots, accuracy, hull, shield) and stops the
  round at once on a death, a stall (no shot, no damage for `-StallSec`), a stopped autopilot or a client gone. Before a
  round the dead respawn and every damaged ship repairs at a base (multiplayer respawns keep the damage) - fair rounds.
  Evidence: `research\experience\runs\mp_pvp_<time>_<label>\` (`transcript.txt`, `rounds.tsv`, per-round logs).
  `pvpmotion.py <FLMotion.tsv>`: how each fight was flown (speed, afterburner, throttle, strafe, nose turn rate).
